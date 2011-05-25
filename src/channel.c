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

#include <sys/socket.h>  /* for AF_*, SOCK_*  */
#include <netinet/in.h>  /* for IPPROTO_*  */
#include <netinet/tcp.h>  /* for TCP_NODELAY  */
#include <fcntl.h>
#include <unistd.h>

/* ### build out prototype on libev to validate the API and model.  */
#include <ev.h>
/* ### also see https://github.com/joyent/libuv  */

#include "pc_types.h"
#include "pc_channel.h"
#include "pc_error.h"
#include "pc_memory.h"

#include "pocore.h"


typedef struct read_buffer_s read_buffer_t;


struct pc__channel_ctx_s {
    /* The pool used for this context and various objects.  */
    pc_pool_t *pool;

    /* The libev loop to use when we run the event subsystem.  */
    struct ev_loop *loop;

    /* A timer used to cause the libev loop to timeout.  */
    ev_timer timeout;

    /* The set of buffers holding pending data for channels that has
       not been consumed.  */
    read_buffer_t *pending;

    /* Buffers that are empty and available for re-use.  */
    read_buffer_t *avail;
};

struct pc_channel_s {
    pc_context_t *ctx;

    int fd;
    ev_io watcher;

    struct iovec *pending_write;
    int pending_iovcnt;
    /* ### we need to skip some portion of the first iovec.  */

    pc_channel_readable_t read_cb;
    void *read_baton;

    pc_channel_writeable_t write_cb;
    void *write_baton;

    pc_channel_error_t error_cb;
    void *error_baton;

    /* ### the above structure is too big. need to reduce to better
       ### manage the C10k problem.  */
};

struct pc_listener_s {
    int foo;
};

struct pc_address_s {
    union {
        struct sockaddr_in inet;
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

#define CHECK_CCTX(ctx) ((ctx)->cctx == NULL ? init_cctx(ctx) : (void)0)


static void
loop_timeout(struct ev_loop *loop, ev_timer *timeout, int revents)
{
    /* Nothing to do. The loop will exit after calling this.  */
}


static void
init_cctx(pc_context_t *ctx)
{
    pc_pool_t *pool = pc_pool_root(ctx);
    struct pc__channel_ctx_s *cctx;

    cctx = ctx->cctx = pc_calloc(pool, sizeof(*cctx));
    cctx->pool = pool;

    cctx->loop = ev_loop_new(EVFLAG_AUTO);

    /* Use some arbitrary values for the timer. Before we call ev_run(),
       we'll put proper values into the timer.  */
    ev_timer_init(&cctx->timeout, loop_timeout, 5.0, 5.0);
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


static pc_bool_t
perform_read(pc_channel_t *channel)
{
    NOT_IMPLEMENTED();
}


static pc_bool_t
perform_write(pc_channel_t *channel)
{
    NOT_IMPLEMENTED();
}


static void
channel_is_usable(struct ev_loop *loop, ev_io *watcher, int revents)
{
    pc_channel_t *channel = watcher->data;
    pc_bool_t dirty = FALSE;

    if (revents & EV_READ)
    {
        dirty = perform_read(channel);
    }
    if (revents & EV_WRITE)
    {
        dirty |= perform_write(channel);
    }

    if (dirty)
    {
        int events = 0;

        if (channel->read_cb)
            events |= EV_READ;
        if (channel->write_cb)
            events |= EV_WRITE;

        start_watcher(loop, watcher, events);
    }
}


pc_error_t *pc_channel_run_events(pc_context_t *ctx, uint64_t timeout)
{
    CHECK_CCTX(ctx);

    /* ### process all pending reads where the channel as (re)indicated
       ### a desire to read more data.  */

    /* Set up the timeout.  */
    /* ### need definition of PoCore times.  */
    ctx->cctx->timeout.repeat = (double)timeout;
    ev_timer_again(ctx->cctx->loop, &ctx->cctx->timeout);

    ev_run(ctx->cctx->loop, EVRUN_ONCE);

    return PC_SUCCESS;
}


pc_error_t *pc_address_lookup(pc_hash_t **addresses,
                              const char *name,
                              int port,
                              int flags,
                              pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


const char *pc_address_readable(const pc_address_t *address,
                                pc_pool_t *pool)
{
    return NULL;
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
        goto error_close;

    /* Unless the caller requests it, disable Nagle's algorithm.  */
    if (!(flags & PC_CHANNEL_USE_NAGLE))
    {
        int disable = 0;

        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                       &disable, sizeof(disable)) == -1)
            goto error_close;
    }

    if (source != NULL)
    {
        if (bind(fd, (struct sockaddr *)&source->a.inet,
                 sizeof(source->a.inet)) == -1)
            goto error_close;
    }

    if (connect(fd, (struct sockaddr *)&destination->a.inet,
                sizeof(destination->a.inet)) == -1)
        goto error_close;

    *channel = pc_calloc(ctx->cctx->pool, sizeof(**channel));
    (*channel)->ctx = ctx;
    (*channel)->fd = fd;

    /* Initialize the watcher. We'll start it once a caller declares
       some interest in reading or writing the channel.  */
    ev_io_init(&(*channel)->watcher, channel_is_usable, fd, 0);

    /* Install the back-reference, for use in the callbacks.  */
    (*channel)->watcher.data = channel;

    return PC_SUCCESS;

  error_close:
    /* Generate an error and close the file descriptor we opened.  */
    err = pc__convert_os_error(ctx);

    if (close(fd) == -1)
        err = pc_error_join(err, pc__convert_os_error(ctx));

    return pc_error_trace(err);
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
        return PC_SUCCESS;

    how = stop_reading ? (stop_writing ? SHUT_RDWR : SHUT_RD) : SHUT_WR;
    if (shutdown(channel->fd, how) == -1)
        return pc_error_trace(pc__convert_os_error(channel->ctx));

    /* We could adjust the watcher here, but why bother. It simply won't
       receive certain events. We don't have to force it.  */

    return PC_SUCCESS;
}


void pc_channel_destroy(pc_channel_t *channel)
{
    /* Shut the connection down, and ignore any errors that may happen
       since we have no good way to deal with the runtime error.  */
    pc_error_handled(pc_channel_close(channel, TRUE, TRUE));

    /* Make sure that we stop the watcher, to avoid confusing libevn.  */
    ev_io_stop(channel->ctx->cctx->loop, &channel->watcher);

    /* ### find pending read buffers and return them to the cctx.  */

    /* Return the channel's memory to the pool, so the pool does not
       have unbounded growth.  */
    pc_pool_freemem(channel->ctx->cctx->pool, channel, sizeof(*channel));
}


void pc_channel_desire_read(pc_channel_t *channel,
                            pc_channel_readable_t callback,
                            void *baton)
{
    int events;

    channel->read_cb = callback;
    channel->read_baton = baton;

    events = EV_READ;
    if (channel->write_cb)
        events |= EV_WRITE;

    start_watcher(channel->ctx->cctx->loop, &channel->watcher, events);
}


void pc_channel_desire_write(pc_channel_t *channel,
                             pc_channel_writeable_t callback,
                             void *baton)
{
    int events;

    channel->write_cb = callback;
    channel->write_baton = baton;

    events = EV_WRITE;
    if (channel->read_cb)
        events |= EV_READ;

    start_watcher(channel->ctx->cctx->loop, &channel->watcher, events);
}


pc_error_t *pc_channel_set_readbuf(pc_channel_t *channel,
                                   size_t bufsize,
                                   pc_pool_t *pool)
{
    NOT_IMPLEMENTED();
}


pc_error_t *pc_channel_set_writebuf(pc_channel_t *channel,
                                    size_t bufsize,
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
