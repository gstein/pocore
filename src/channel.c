/*
  channel.c :  implementation of PoCore's channel functions

  ====================================================================
    Copyright 2011 Greg Stein

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
  ====================================================================
*/

#include "pc_types.h"
#include "pc_channel.h"
#include "pc_error.h"
#include "pc_memory.h"

#include "pocore.h"

#ifndef PC__IS_WINDOWS
/* ### build out prototype on libev to validate the API and model.  */
#include <ev.h>
/* ### also see https://github.com/joyent/libuv  */
#endif

/* ### some handy information on (unix) sockets:
   ###   http://www.retran.com/beej/inet_ntopman.html
   ###   http://www.retran.com/beej/sockaddr_inman.html
   ###   http://cr.yp.to/docs/connect.html
   ###   http://msdn.microsoft.com/en-us/library/ms738547%28VS.85%29.aspx
*/

typedef struct read_buffer_s read_buffer_t;


struct pc__channel_ctx_s {
    /* The pool used for this context and various objects.  */
    pc_pool_t *pool;

    /* Marker to prevent re-entering run_events().  */
    pc_bool_t running;

    /* The libev loop to use when we run the event subsystem.  */
    struct ev_loop *loop;

    /* A timer used to cause the libev loop to timeout.  */
    ev_timer timeout;

    /* The set of buffers holding pending data for channels that has
       not been consumed.  */
    read_buffer_t *pending;

    /* Buffers that are empty and available for re-use.  */
    read_buffer_t *avail;

    /* This pool is used as a scratch pool for the callbacks.  */
    pc_pool_t *callback_scratch;

    /* We sometimes need to assemble some iovecs for writing pending
       data. That will use these slots, or switch to a pool when there
       are too many iovecs.  */
#define INTERNAL_IOVEC_COUNT 128
    struct iovec scratch_iov[INTERNAL_IOVEC_COUNT];
};

struct pc_channel_s {
    pc_context_t *ctx;

    int fd;   /* ### maybe omit since it is in watcher.fd  */
    ev_io watcher;

    /* These fields contain data that the application provided to us for
       writing to the socket. It will be valid until we invoke write_cb
       again.

       Each time the socket becomes writeable, we will examine these fields
       for pending data to write to the socket. Only when this becomes
       empty will we invoke the callback for more data.  */
    struct iovec *pending_iov;
    int pending_iovcnt;

    /* When we write to the socket, any completed iovecs will not be
       remembered in PENDING_IOV (we just index past them). However,
       we may have a short write for the first iovec. PENDING_BUF points
       to the first byte of unwritten data in that iovec.  */
    char *pending_buf;

    const pc_channel_callbacks_t *callbacks;
    void *cb_baton;

    pc_bool_t desire_read;
    pc_bool_t desire_write;

    /* ### the above structure is too big. need to reduce to better
       ### manage the C10k problem.  */
};

struct pc_listener_s {
    int foo;
};

struct pc_address_s {
    union {
        struct sockaddr_storage inet;
    } a;
};


struct read_buffer_s {
    /* The channel this buffer is holding data for. This will be NULL if
       this read buffer is on the CCTX->AVAIL list, waiting to be
       associated with another channel.  */
    pc_channel_t *channel;

    /* The buffer, allocated from CCTX->POOL.  */
    char *buf;
    size_t len;

    /* After a partial read, this points to the current location in the
       buffer for the next read.  */
    char *current;

    /* How many bytes of content remain in the buffer. This will be
       adjusted after each read consumes some portion.  */
    size_t remaining;

    /* Linked list of channels with pending content, or to link available
       buffers together.  */
    read_buffer_t *next;
};

/* We read from the network into a buffer of this size. We want to leave
   a little bit of room (ie. not page-aligned) to ensure that any overheads
   will not spill us into an extra page.

   ### this should be adjustable by pc_eventsys_set_bufsize()  */
#define READ_BUFFER_SIZE 16000


#define CHECK_CCTX(ctx) ((ctx)->cctx == NULL ? init_cctx(ctx) : (void)0)


static void
loop_timeout(struct ev_loop *loop, ev_timer *timeout, int revents)
{
    /* Nothing to do. The loop will exit after calling this.  */
}


static void
init_cctx(pc_context_t *ctx)
{
    /* ### custom config flags for this root pool?  */
    pc_pool_t *pool = pc_pool_root(ctx);
    struct pc__channel_ctx_s *cctx;

    cctx = ctx->cctx = pc_calloc(pool, sizeof(*cctx));
    cctx->pool = pool;

    cctx->loop = ev_loop_new(EVFLAG_AUTO);

    /* Use some arbitrary values for the timer. Before we call ev_run(),
       we'll put proper values into the timer.  */
    ev_timer_init(&cctx->timeout, loop_timeout, 5.0, 5.0);

    cctx->callback_scratch = pc_pool_create(pool);
}


void pc__channel_cleanup(pc_context_t *ctx)
{
    ev_timer_stop(ctx->cctx->loop, &ctx->cctx->timeout);
    pc_pool_destroy(ctx->cctx->pool);
    ctx->cctx = NULL;
}


static void
start_watcher(struct ev_loop *loop,
              ev_io *watcher,
              int events)
{
    /* We cannot modify the watcher while it is active. Yank it back.  */
    if (ev_is_active(watcher))
    {
        ev_io_stop(loop, watcher);
    }

    /* Update the flags and start the watcher.  */
    ev_io_set(watcher, watcher->fd, events);
    ev_io_start(loop, watcher);
}


static read_buffer_t *
get_read_buffer(struct pc__channel_ctx_s *cctx)
{
    read_buffer_t *rb = cctx->avail;

    if (rb == NULL)
    {
        rb = pc_alloc(cctx->pool, sizeof(*rb) + READ_BUFFER_SIZE);
        rb->buf = (char *)rb + sizeof(*rb);
        rb->len = READ_BUFFER_SIZE;

        return rb;
    }

    cctx->avail = rb->next;
    return rb;
}


static pc_bool_t
perform_read(pc_channel_t *channel)
{
    read_buffer_t *rb = get_read_buffer(channel->ctx->cctx);

    /* We are going to loop on the socket until we've drained it of all
       available data, or when the callback signals it does not want any
       more data at this time.  */
    while (1)
    {
        ssize_t amt;
        ssize_t consumed;
        pc_error_t *err;

        do
        {
            amt = read(channel->fd, rb->buf, rb->len);

            /* Data should be immediately available, but maybe we'll need to
               try a couple times.  */
        } while (amt == -1 && errno == EINTR);

        if (amt == -1 || amt == 0)
        {
            /* Return the read buffer to the context.  */
            rb->next = channel->ctx->cctx->avail;
            channel->ctx->cctx->avail = rb;

            if (amt == 0)
            {
                /* The other end closed the socket -- we just hit EOF.

                   ### signal the app somehow.
                   ### for now, turn off reading.  */
                channel->desire_read = FALSE;
                return TRUE;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /* Tell the callback that we are done for the moment.  */
                err = channel->callbacks->read_cb(
                        &consumed,
                        NULL /* buf */, 0 /* len */,
                        channel, channel->cb_baton,
                        channel->ctx->cctx->callback_scratch);
                pc_pool_clear(channel->ctx->cctx->callback_scratch);

                /* ### what to do with an error... */
                pc_error_handled(err);

                if (consumed == PC_CONSUMED_STOP)
                {
                    /* Stop reading from the socket.  */
                    channel->desire_read = FALSE;
                    return TRUE;
                }
                /* ### assert consumed == PC_CONSUMED_CONTINUE  */

                /* No changes to our desire to read (waiting for more)  */
                return FALSE;
            }

            if (errno == ECONNRESET)
            {
                /* ### signal the problem somehow  */
            }
            else
            {
                /* ### what others errors, and how to signal?  */
            }

            /* Stop reading from this socket.  */
            channel->desire_read = FALSE;
            return TRUE;
        }

        err = channel->callbacks->read_cb(
                &consumed, rb->buf, amt, channel,
                channel->cb_baton,
                channel->ctx->cctx->callback_scratch);
        pc_pool_clear(channel->ctx->cctx->callback_scratch);

        /* ### what to do with an error... */
        pc_error_handled(err);

        /* ### assert consumed >= 0 && consumed <= amt  */

        if (consumed < amt)
        {
            /* Store the read buffer in our list of pending reads.  */
            rb->channel = channel;
            rb->current = rb->buf + consumed;
            rb->remaining = amt - consumed;
            rb->next = channel->ctx->cctx->pending;
            channel->ctx->cctx->pending = rb;

            /* Stop reading.  */
            channel->desire_read = FALSE;
            return TRUE;
        }

        /* The callback read all the data provided. Loop back to read
           more data from the socket (if any).  */
    }

    /* NOTREACHED  */
}


static void
get_iovec(struct iovec **result_iov,
          int *result_iovcnt,
          pc_channel_t *channel)
{
    int iovcnt = channel->pending_iovcnt;
    char *buf = channel->pending_buf;

    assert(channel->pending_iov != NULL && iovcnt > 0);

    /* The simplest case: we can directly use PENDING_IOV because we don't
       need to adjust the first iovec for PENDING_BUF.  */
    if (buf == NULL || buf == channel->pending_iov[0].iov_base)
    {
        *result_iov = channel->pending_iov;
        *result_iovcnt = iovcnt;
        return;
    }

    /* The first iovec must be adjusted, so we need to copy it into our
       pre-allocated scratch area, or onto the heap.  */

    if (iovcnt <= INTERNAL_IOVEC_COUNT)
    {
        *result_iov = &channel->ctx->cctx->scratch_iov[0];
        *result_iovcnt = iovcnt;
    }
    else
    {
        *result_iov = pc_alloc(channel->ctx->cctx->callback_scratch,
                               iovcnt * sizeof(**result_iov));
        *result_iovcnt = iovcnt;
    }

    memcpy(*result_iov, channel->pending_iov, iovcnt * sizeof(**result_iov));
    (*result_iov)[0].iov_base = buf;
    (*result_iov)[0].iov_len =
        (channel->pending_iov[0].iov_len
         - (buf - (char *)channel->pending_iov[0].iov_base));
}


static void
maybe_free_iovec(pc_channel_t *channel,
                 struct iovec *iov,
                 int iovcnt)
{
    if (iov != channel->pending_iov
        && iov != &channel->ctx->cctx->scratch_iov[0])
    {
        pc_pool_freemem(channel->ctx->cctx->callback_scratch,
                        iov,
                        iovcnt * sizeof(*iov));
    }
}


static void
adjust_pending(pc_channel_t *channel,
               ssize_t amt)
{
    if (channel->pending_buf != NULL)
    {
        size_t remaining;

        remaining = (channel->pending_iov[0].iov_len
                     - (channel->pending_buf
                        - (char *)channel->pending_iov[0].iov_base));
        if (amt < remaining)
        {
            /* We still have some data left in the first iovec.  */
            channel->pending_buf += amt;
            return;
        }

        amt -= remaining;
        channel->pending_buf = NULL;
        if (--channel->pending_iovcnt == 0)
        {
            /* We consumed all the pending data.  */
            channel->pending_iov = NULL;
            return;
        }
        ++channel->pending_iov;
    }

    while (amt >= channel->pending_iov[0].iov_len)
    {
        if (--channel->pending_iovcnt == 0)
        {
            /* We must have written exactly the amount of the first iovec,
               if we just ran out of pending iovecs.  */
            assert(amt == channel->pending_iov[0].iov_len);
            channel->pending_iov = NULL;
            return;
        }

        amt -= channel->pending_iov[0].iov_len;
        ++channel->pending_iov;
    }

    /* Something was left over.  */
    channel->pending_buf = channel->pending_iov[0].iov_base + amt;
}


static pc_bool_t
perform_write(pc_channel_t *channel)
{
    while (1)
    {
        struct iovec *iov;
        int iovcnt;
        ssize_t amt;

        if (channel->pending_iov == NULL)
        {
            pc_error_t *err;

            /* Ask the application for some data to write.  */
            err = channel->callbacks->write_cb(
                    &channel->pending_iov,
                    &channel->pending_iovcnt,
                    channel,
                    channel->cb_baton,
                    channel->ctx->cctx->callback_scratch);
            pc_pool_clear(channel->ctx->cctx->callback_scratch);

            /* ### what to do with an error... */
            pc_error_handled(err);

            /* The application may state that it has nothing further
               to write.  */
            if (channel->pending_iov == NULL)
            {
                /* Stop writing.  */
                channel->desire_write = FALSE;
                return TRUE;
            }

            /* We have original data, without a partial write.  */
            channel->pending_buf = NULL;
        }

        /* Juggle around the iovecs as necessary to find data to write.
           Note that IOV might point into CALLBACK_SCRATCH.  */
        get_iovec(&iov, &iovcnt, channel);

        do
        {
            amt = writev(channel->fd, iov, iovcnt);

            /* The socket should be immediately writeable, but maybe
               we'll need to try a couple times.  */
        } while (amt == -1 && errno == EINTR);

        /* Done with the iovec. We can (later) use AMT to adjust all the
           data within CHANNEL->PENDING_*.  */
        maybe_free_iovec(channel, iov, iovcnt);

        if (amt == 0
            || (amt == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)))
        {
            /* Nothing was written. No need to change PENDING_IOV.
               Just return, signalling no change in desire to write.  */
            return FALSE;
        }

        if (amt == -1)
        {
            if (errno == ECONNRESET)
            {
                /* ### signal the problem somehow  */
            }
            else
            {
                /* ### what others errors, and how to signal?  */
            }

            /* Stop writing to this socket.  */
            channel->desire_write = FALSE;
            return TRUE;
        }

        /* Adjust the pending data, possibly emptying it.  */
        adjust_pending(channel, amt);

        /* Loop to see if we can write more data into the socket.  */
    }

    /* NOTREACHED  */
}


static void
channel_is_usable(struct ev_loop *loop, ev_io *watcher, int revents)
{
    pc_channel_t *channel = watcher->data;
    pc_bool_t dirty = FALSE;

    /* Note that we double-check the callbacks. We want to ignore
       spurious signals to read or write, and then we want to try
       to disable those events (again).  */

    if ((revents & EV_READ) != 0)
    {
        if (channel->desire_read)
            dirty = perform_read(channel);
        else
            dirty = TRUE;
    }
    if ((revents & EV_WRITE) != 0)
    {
        if (channel->desire_write)
            dirty |= perform_write(channel);
        else
            dirty = TRUE;
    }

    /* Do we need to adjust what events we're looking for?  */
    if (dirty)
    {
        int events = 0;

        if (channel->desire_read)
            events |= EV_READ;
        if (channel->desire_write)
            events |= EV_WRITE;

        start_watcher(loop, watcher, events);
    }
}


pc_error_t *pc_eventsys_run(pc_context_t *ctx, uint64_t timeout)
{
    CHECK_CCTX(ctx);

    if (ctx->cctx->running)
        return pc_error_create_x(ctx, PC_ERR_IMPROPER_REENTRY);

    ctx->cctx->running = TRUE;

    /* ### process all pending reads where the channel has (re)indicated
       ### a desire to read more data.  */

    /* Set up the timeout.  */
    /* ### need definition of PoCore times.  */
    ctx->cctx->timeout.repeat = (double)timeout;
    ev_timer_again(ctx->cctx->loop, &ctx->cctx->timeout);

    ev_run(ctx->cctx->loop, EVRUN_ONCE);

    ctx->cctx->running = FALSE;

    return PC_NO_ERROR;
}


void pc_eventsys_set_bufsize(pc_context_t *ctx, size_t bufsize)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_address_lookup(pc_hash_t **addresses,
                              const char *name,
                              int port,
                              int flags,
                              pc_pool_t *pool)
{
    char portbuf[6];
    struct addrinfo hints = { 0 };
    struct addrinfo *results;
    struct addrinfo *scan;
    int rv;

    if (port <= 0 || port > 65535)
        return pc_error_create_pm(pool, PC_ERR_BAD_PARAM,
                                  "port number out of range");
    snprintf(portbuf, sizeof(portbuf), "%d", port);

    /* ### these are the wrong hints for now, but let's just get started  */
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    rv = getaddrinfo(name, portbuf, &hints, &results);
    if (rv != 0)
    {
        const char *msg = gai_strerror(rv);

        return pc_error_create_pm(pool, PC_ERR_ADDRESS_LOOKUP, msg);
    }

    *addresses = pc_hash_create(pool);
    for (scan = results; scan != NULL; scan = scan->ai_next)
    {
        /* Can the returned address fit into our structure?
           ### this is probably bogus and needs some research.  */
        if (scan->ai_addrlen <= sizeof(((pc_address_t *)0)->a.inet))
        {
            pc_address_t *addr = pc_alloc(pool, sizeof(*addr));
            const char *readable;

            memcpy(&addr->a.inet, scan->ai_addr, scan->ai_addrlen);

            readable = pc_address_readable(addr, pool);
            pc_hash_sets(*addresses, readable, addr);
        }
    }

    freeaddrinfo(results);

    return PC_NO_ERROR;
}


const char *pc_address_readable(const pc_address_t *address,
                                pc_pool_t *pool)
{
    if (address->a.inet.ss_family == AF_INET)
    {
        char buf[INET_ADDRSTRLEN];

        if (inet_ntop(AF_INET,
                      &((struct sockaddr_in *)&address->a.inet)->sin_addr,
                      buf, sizeof(buf)) == NULL)
        {
            /* ### record the runtime error somehow.  */
            return "<error>";
        }

        return pc_strdup(pool, buf);
    }

    NOT_IMPLEMENTED();
}


pc_error_t *pc_channel_create_tcp(pc_channel_t **channel,
                                  pc_context_t *ctx,
                                  const pc_address_t *destination,
                                  const pc_address_t *source,
                                  int flags)
{
    int fd;
    int sktflags;
    pc_error_t *err;
    const char *msg;

    /* ### validate the addresses as INET  */

    CHECK_CCTX(ctx);

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == -1)
    {
        return pc_error_trace(pc__convert_os_error(ctx));
    }

    /* Make the socket non-blocking.  */
    sktflags = fcntl(fd, F_GETFL, 0);
    /* ### some platforms might not have O_NONBLOCK  */
    sktflags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, sktflags) == -1)
    {
        msg = "unable to set non-blocking";
        goto error_close;
    }

    /* Unless the caller requests it, disable Nagle's algorithm.  */
    if (!(flags & PC_CHANNEL_USE_NAGLE))
    {
        int disable = 0;

        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                       &disable, sizeof(disable)) == -1)
        {
            msg = "unable to disable Nagle's algorithm";
            goto error_close;
        }
    }

    if (source != NULL)
    {
        if (bind(fd, (struct sockaddr *)&source->a.inet,
                 PC__SOCKADDR_LEN(&source->a.inet)) == -1)
        {
            msg = "unable to bind source address";
            goto error_close;
        }
    }

    if (connect(fd, (struct sockaddr *)&destination->a.inet,
                PC__SOCKADDR_LEN(&destination->a.inet)) == -1)
    {
        if (errno != EINPROGRESS)
        {
            msg = "problem with connecting";
            goto error_close;
        }
    }

    *channel = pc_calloc(ctx->cctx->pool, sizeof(**channel));
    (*channel)->ctx = ctx;
    (*channel)->fd = fd;

    /* Initialize the watcher. We'll start it once a caller declares
       some interest in reading or writing the channel.  */
    ev_io_init(&(*channel)->watcher, channel_is_usable, fd, 0);

    /* Install the back-reference, for use in the callbacks.  */
    (*channel)->watcher.data = *channel;

    /* ### we should track this channel for auto-cleanup when the
       ### context (and the CCTX pool is cleaned up).  */

    return PC_NO_ERROR;

  error_close:
    /* Generate an error and close the file descriptor we opened.  */
    err = pc_error_annotate(msg, pc__convert_os_error(ctx));

    if (close(fd) == -1)
        err = pc_error_join(err, pc__convert_os_error(ctx));

    return err;
}


pc_error_t *pc_channel_create_udp(pc_channel_t **channel,
                                  pc_context_t *ctx,
                                  const pc_address_t *destination,
                                  const pc_address_t *source)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_channel_create_pipe(pc_channel_t **endpoint1,
                                   pc_channel_t **endpoint2,
                                   pc_context_t *ctx)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_channel_create_local(pc_channel_t **channel,
                                    pc_context_t *ctx,
                                    const char *name)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_channel_close(pc_channel_t *channel,
                             pc_bool_t stop_reading,
                             pc_bool_t stop_writing)
{
    int how;

    /* ### should probably return an error. "bad usage" or somesuch.  */
    if (!stop_reading && !stop_writing)
        return PC_NO_ERROR;

    how = stop_reading ? (stop_writing ? SHUT_RDWR : SHUT_RD) : SHUT_WR;
    if (shutdown(channel->fd, how) == -1)
        return pc_error_trace(pc__convert_os_error(channel->ctx));

    /* We could adjust the watcher here, but why bother. It simply won't
       receive certain events. We don't have to force it.  */

    return PC_NO_ERROR;
}


void pc_channel_destroy(pc_channel_t *channel)
{
    /* Shut the connection down, and ignore any errors that may happen
       since we have no good way to deal with the runtime error.  */
    pc_error_handled(pc_channel_close(channel, TRUE, TRUE));

    /* Make sure that we stop the watcher, to avoid confusing libevn.  */
    ev_io_stop(channel->ctx->cctx->loop, &channel->watcher);

    /* Done with the socket.  */
    (void) close(channel->fd);

    /* ### find pending read buffers and return them to the cctx.  */

    /* Return the channel's memory to the pool, so the pool does not
       have unbounded growth.  */
    pc_pool_freemem(channel->ctx->cctx->pool, channel, sizeof(*channel));
}


void pc_channel_register_callbacks(pc_channel_t *channel,
                                   const pc_channel_callbacks_t *callbacks,
                                   void *cb_baton)
{
    channel->callbacks = callbacks;
    channel->cb_baton = cb_baton;
}


void pc_channel_desire_read(pc_channel_t *channel)
{
    int events;

    channel->desire_read = TRUE;

    events = EV_READ;
    if (channel->desire_write)
        events |= EV_WRITE;

    start_watcher(channel->ctx->cctx->loop, &channel->watcher, events);
}


void pc_channel_desire_write(pc_channel_t *channel)
{
    int events;

    channel->desire_write = TRUE;

    events = EV_WRITE;
    if (channel->desire_read)
        events |= EV_READ;

    start_watcher(channel->ctx->cctx->loop, &channel->watcher, events);
}


pc_error_t *pc_channel_read(size_t *read,
                            pc_channel_t *channel,
                            void *buf,
                            size_t len,
                            pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_channel_write(size_t *written,
                             pc_channel_t *channel,
                             const void *buf,
                             size_t len,
                             pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_channel_read_from(void **buf,
                                 size_t *len,
                                 const pc_address_t **address,
                                 pc_channel_t *channel,
                                 pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_channel_write_to(pc_channel_t *channel,
                                const pc_address_t *address,
                                const void *buf,
                                size_t len,
                                pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_listener_create(pc_listener_t **listener,
                               pc_context_t *ctx,
                               const pc_address_t *address,
                               int backlog,
                               int flags,
                               pc_listener_acceptor_t callback,
                               void *baton)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_listener_close(pc_listener_t *listener)
{
    NOT_IMPLEMENTED();
}
