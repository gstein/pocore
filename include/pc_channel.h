/*
  pc_channel.h :  I/O channel handling

  ====================================================================
    Copyright 2010 Greg Stein

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

#ifndef PC_CHANNEL_H
#define PC_CHANNEL_H

#include <stdint.h>
#include <sys/uio.h>  /* for struct iovec  */

#include "pc_types.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ### META: there is a lot of conflicting discussion in this file.
   ### the idea is to record ALL thoughts and approaches until the
   ### issue is settled.  */

/* Note: files are not part of the Channels API. They have a richer set
   of semantics (seeking, positions, etc) than the streams/datagrams
   that are part of the Channels. Files do not have edge-triggering events
   likes sockets and pipes -- they are "always available".  */

/* ### create, or one-per-context? there are probably no broad defaults
   ### or values that two event run-loops might need to alter. thus, it
   ### seems that we could do one-per-context and simply block on the
   ### run() call. if an app/lib wants simultaneous loops, then it could
   ### instantiate multiple contexts.
   ### BUT: if we block the thread on a context, then other threads could
   ### not use the context (since we provide ZERO interlocks). that would
   ### effectively consume this context.
   ### OOP! answer solved: since the event system and the callbacks will
   ### be operating upon the involved objects, it would be a Bad Thing to
   ### let other threads work with this context/objects/pools. we could
   ### potentially require the application to interlock all object
   ### accesses from within the callbacks, but it is simpler to just
   ### acknowledge that apps will VERY RARELY want to invoke two event
   ### run-loops. if an app cares to do this, then it can create two
   ### contexts and two eeparate sets of independent objects. for the
   ### more typical case of a single app and run-loop, associating that
   ### run-loop directly with the context will be more than adequate.

   The various channel and listener objects are allocated within a private
   pool of the context, and automatically associated with the event system
   within the context.
*/


/* ### ugh. how to do socket/named-pipe addressing in a good way?  */

/*
         U _apr_sockaddr_info_get
         U _apr_socket_accept
         U _apr_socket_bind
         U _apr_socket_connect
         U _apr_socket_listen
         U _apr_socket_opt_set
         U _apr_socket_recv
         U _apr_socket_sendv
         U _apr_socket_timeout_set

         ;

         U _apr_socket_close
         U _apr_socket_create
*/

typedef struct pc_channel_s pc_channel_t;


/* ### is this needed, given pc_channel_close() ?  */
void pc_channel_destroy(pc_channel_t *channel);


/* ### need to think more on this.  */
void pc_channel_track(const pc_channel_t *channel, pc_context_t *ctx);
void pc_channel_track_via(const pc_channel_t *channel, pc_pool_t *pool);
void pc_channel_owns(const pc_channel_t *channel, pc_pool_t *pool);

/* ### channels for stdio, stdout, stderr.
   ### do research about buffer sizes and binary-mode for these.  */


/* Socket addressing.  */

/* Addresses in PoCore are opaque objects. Applications really don't care
   about the internals. They want to map names to an address, to use the
   address in other APIs, and to create a human-readable form of the address.
   We can manage all of this with appropriate APIs.  */
typedef struct pc_address_s pc_address_t;


/* Look up @a name (synchronously) and return the address information in
   @a addresses, which will be allocated in @a pool. Since a lookup may
   yield multiple addresses for a given name, the individual addresses
   are returned as a hash table mapping the human-readable name to a
   pc_address_t pointer.

   The addresses will be associated with @a port.

   The @a flags parameter controls the lookup process.  */
pc_error_t *pc_address_lookup(pc_hash_t **addresses,
                              const char *name,
                              int port,
                              int flags,
                              pc_pool_t *pool);

/* Flags to control the address lookup process.  */
#define PC_ADDRESS_PREFER_IPV4 0x0001
#define PC_ADDRESS_PREFER_IPV6 0x0002


/* ### offer an asynchronous lookup? I think "no" since that means external
   ### libraries on all platforms. the application can just do that.
   ### though... that *does* mean the application would need a way to
   ### convert resulting address info into our structures.  */


/* Format @a address into a human-readable form, allocated in @a pool.  */
const char *pc_address_readable(const pc_address_t *address,
                                pc_pool_t *pool);


/* Named value for the default set of flags.  */
#define PC_CHANNEL_DEFAULT_FLAGS 0

/* New listeners should not reuse the address. By default, listeners will
   reuse the listening address. See SO_REUSEADDR.  */
#define PC_CHANNEL_NO_REUSE 0x0001

/* A new or accepted channel should use Nagle's algorithm. By default,
   channels disable Nagle.  */
#define PC_CHANNEL_USE_NAGLE 0x0002


/* Create a TCP channel and return it in @a channel. The channel will begin
   connecting to the destination @a address. The application must wait for
   the channel to become readable or writeable, indicating the connection
   is complete. If @a source is not NULL, then it will be bound as the
   source address for the channel.

   @a flags specifies options for the connection. In particular, see
   PC_CHANNEL_USE_NAGLE.  */
pc_error_t *pc_channel_create_tcp(pc_channel_t **channel,
                                  pc_context_t *ctx,
                                  const pc_address_t *destination,
                                  const pc_address_t *source,
                                  int flags);


/* Create a UDP channel and return it in @a channel. If @a source is not
   NULL, then it will be bound as the source address for the channel.  */
pc_error_t *pc_channel_create_udp(pc_channel_t **channel,
                                  pc_context_t *ctx,
                                  const pc_address_t *destination,
                                  const pc_address_t *source);


/* ### is there anything more to creating a pipe?  */
pc_error_t *pc_channel_create_pipe(pc_channel_t **endpoint1,
                                   pc_channel_t **endpoint2,
                                   pc_context_t *ctx);


/* A 'local' channel establishes a Unix domain socket on POSIX systems
   at a specified path. On Windows, it establishes a local-only named
   pipe.

   ### do we need anything more to address the local channel?
   ### note that paths (potentially) have complicated charset issues.
   ###   see pc_file.h  */
pc_error_t *pc_channel_create_local(pc_channel_t **channel,
                                    pc_context_t *ctx,
                                    const char *name);


/* Close this end of @a channel. If @a stop_reading is TRUE, then further
   synchronous reads will not be possible and the read-callback will no
   longer be invoked. @a stop_writing is similar, but for writing.  */
pc_error_t *pc_channel_close(pc_channel_t *channel,
                             pc_bool_t stop_reading,
                             pc_bool_t stop_writing);


typedef struct pc_listener_s pc_listener_t;

/* ### docco
   
   The callback may use @a pool for temporary allocations. It will be
   cleared after every invocation of the callback.
*/
typedef pc_error_t *(*pc_listener_acceptor_t)(pc_listener_t *listener,
                                              pc_channel_t *new_channel,
                                              void *baton,
                                              pc_pool_t *pool);


/* Set up a listening socket at @a address with a connection backlog
   specified by @a backlog. When a connection arrives, it will be
   accepted and @a callback will be invoked, passing along @a baton.  */
pc_error_t *pc_listener_create(pc_listener_t **listener,
                               pc_context_t *ctx,
                               const pc_address_t *address,
                               int backlog,
                               int flags,
                               pc_listener_acceptor_t callback,
                               void *baton);

/* A default backlog value to use when applications do not require
   a special value.  */
#define PC_LISTENER_DEFAULT_BACKLOG 5


pc_error_t *pc_listener_close(pc_listener_t *listener);


/* Synchronous read from CHANNEL.  */
pc_error_t *pc_channel_read(size_t *read,
                            pc_channel_t *channel,
                            void *buf,
                            size_t len,
                            pc_pool_t *pool);

/* Synchronous write to CHANNEL.  */
pc_error_t *pc_channel_write(size_t *written,
                             pc_channel_t *channel,
                             const void *buf,
                             size_t len,
                             pc_pool_t *pool);

/* ### need addressing concept.

   When a datagram is read from the channel, the data resides in an internal
   buffer. This ptr/len pair is returned, along with the address that sent
   the data.

   The buffer will exist, unchanged until the next call to read_from() or
   the destruction of this channel.

   ### note: destruction... not endpoint shutdown since dgrams are not
   ### connection-oriented.

   ### probably need to hook datagram channels into the event system
*/
pc_error_t *pc_channel_read_from(void **buf,
                                 size_t *len,
                                 const pc_address_t **address,
                                 pc_channel_t *channel,
                                 pc_pool_t *pool);
pc_error_t *pc_channel_write_to(pc_channel_t *channel,
                                const pc_address_t *address,
                                const void *buf,
                                size_t len,
                                pc_pool_t *pool);

/* ### accept, bind, connect, listen, options, shutdown.  */


/* ### docco on how long this runs. exit conditions. etc.
   ### time type.  */
pc_error_t *pc_channel_run_events(pc_context_t *ctx, uint64_t timeout);


/* Callback should consume @a len bytes of data from the buffer at @a buf.
   The number of bytes consumed should be stored into @a consumed.

   If the callback consumes less than @a len bytes, then the callback will
   not be invoked again. The application must call pc_channel_desire_read()
   to indicate its readiness to finish reading available data. The event
   system will stop reading from the source, thus placing back-pressure
   on the other end of the channel. The unread data will be retained by
   the event system (which may present an issue in high-connection-count
   scenarios).

   If the callback consumes all @a len bytes, then the callback will be
   (immediately) invoked again. The second invocation will take one of
   two forms:

     1) More data will be presented in @a buf and @a len.

     2) The @a buf parameter will be NULL (and @a len is unused). This
        indicates that the event system has no more data for the callback
        to read at this time. The callback must set @a consumed to either
        PC_CONSUMED_STOP or PC_CONSUMED_CONTINUE. If @a consumed is set
        to PC_CONSUMED_STOP, then the callback will not be invoked again,
        and the application must signal readiness, to read again, with
        a call to pc_channel_desire_read(). If @a consumed is set to
        PC_CONSUMED_CONTINUE, then the callback will be invoked when
        additional data arrives.

   For the PC_CONSUMED_CONTINUE case, the event system will signal the
   desire to read from the underlying source. In all other cases, the
   event system will turn off the readiness indicator for reading.

   The callback may use @a pool for temporary allocations. It will be
   cleared after every invocation of the callback.

   ### the event system's read buffer can/should be adjustable via an API.
   ### if this API is provided, then we don't need a read size in the
   ### desire_read() call. this API also means the app can fine-tune the
   ### buffer for large data transfers, or keep it small for very high
   ### concurrency and small packets. this buffer size could be per-ob
   ### or possibly an event-system-wide value.

   ### on Windows, we simply present the buffer that was passed to the
   ### Overlapped I/O reading function. "here is the ready data".
   ### on POSIX, we could read the ob into an event-internal buffer and
   ### present that to the callback.

   ### need a way to say "I expect no further content. don't look for
   ### a readable state". maybe a simple boolean return? (DONE)
   ### on Windows, this means we *skip* another Overlapped read, thus
   ### freeing the read-buffer from non-paged memory.
   ### on POSIX, we disable triggering on readable state, and let the
   ### OS buffer up any incoming data until we decide to deal with it.

   ### need dgram version
*/
typedef pc_error_t *(*pc_channel_readable_t)(ssize_t *consumed,
                                             const void *buf,
                                             size_t len,
                                             pc_channel_t *channel,
                                             void *baton,
                                             pc_pool_t *pool);
#define PC_CONSUMED_STOP ((ssize_t) -1)
#define PC_CONSUMED_CONTINUE ((ssize_t) -2)


/* Callback should fill in @a iov and @a iovcnt with the data to write.
   After the data has been written, and @a channel is (again) available
   for writing, this callback will be invoked to request further data
   to write.

   Set @a iov to NULL to indicate nothing futher to write (and @a iovcnt
   will be ignored in this case). The callback will no longer be invoked.
   If the application wants to write more data to @a channel, then it must
   call pc_channel_desire_write().

   The data referenced by the returned iovec must exist, without change,
   until this callback is invoked again, or the channel is destroyed.
   The prior contents may then be disposed or re-used.

   The callback may use @a pool for temporary allocations. It will be
   cleared after every invocation of the callback.

   ### more

   ### need dgram version
*/
typedef pc_error_t *(*pc_channel_writeable_t)(struct iovec **iov,
                                              int *iovcnt,
                                              pc_channel_t *channel,
                                              void *baton,
                                              pc_pool_t *pool);

/* ### what to do here? the kinds of errors are scarily broad in scope.
   ### can we encapsulate all exceptions into this callback?

   ### pass one to desire_read and desire_write?
   ### maybe associate a triple-callback with CHANNEL, and then have
   ### signalling functions to look for read/write triggering?
   ### note that a set-of-functions means that we keep a single pointer
   ### per CHANNEL rather than a set of pointers. memory savings are
   ### important for the C10k problem. the set could contain other data,
   ### such as timeouts and a (debug) human-readable name.
*/
typedef pc_error_t *(*pc_channel_error_t)(void *error,
                                          pc_channel_t *channel,
                                          void *baton,
                                          pc_pool_t *pool);


/* ### we need to investigate how to minimize the per-channel memory.
   ### for the C10k problem, we want to minimize. one easy option would
   ### be sharing a baton across all channel callbacks. a bit harder (or
   ### possibly just harder to use) is a vtable of callbacks and flags
   ### denoting which conditions are desired.  */


/* When the item has data, the callback will be invoked.

   ### this registers the channel with the event system (via CHANNEL),
   ### signaling a desire for reading data.

   ### the amount to read is determined by the buffer size (see set_readbuf),
   ### and the event system will continue reading, and invoking the callback,
   ### as long as data is available.

   ### more docco.  */
void pc_channel_desire_read(pc_channel_t *channel,
                            pc_channel_readable_t callback,
                            void *baton);

/* When the item can be writen, the callback will be invoked.

   ### this registers the channel with the event system (via CHANNEL),
   ### signaling a desire for writing data.

   ### more docco.  */
void pc_channel_desire_write(pc_channel_t *channel,
                             pc_channel_writeable_t callback,
                             void *baton);


/* Adjust the channel's buffer sizes that it uses for read/write with the OS.

   ### docco for interaction with dgrams
   ### pipes have buffer sizes
   ### this bufsize w.r.t reading from a socket
   ### how much of this bufsize is app-level (eventsys) vs OS?

   ### need pools?
   ### better name?
*/
pc_error_t *pc_channel_set_readbuf(pc_channel_t *channel,
                                   size_t bufsize,
                                   pc_pool_t *pool);
pc_error_t *pc_channel_set_writebuf(pc_channel_t *channel,
                                    size_t bufsize,
                                    pc_pool_t *pool);


/* ### in the serf scenario, the socket bucket accepts the read callback and
   ### holds onto the ptr/len pair. a bucket read consumes data from that
   ### memory. note that the serf read cycle is synchronous from the read
   ### callback (serf tells the response handler to do a read). after the
   ### cycle completes, it can return the amount consumed back into pocore.

   ### for a write callback, serf reads from a request bucket, and then
   ### returns that as data to write.
*/


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PC_CHANNEL_H */
