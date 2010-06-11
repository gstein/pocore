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

typdef struct pc_channel_s pc_channel_t;


typedef enum {
    /* A datagram socket for IPv4 addresses.  */
    pc_channel_udp4,

    /* A datagram socket for IPv6 addresses.  */
    pc_channel_udp6,

    /* A streaming socket for IPv4 addresses.  */
    pc_channel_tcp4,

    /* A streaming socket for IPv4 addresses.  */
    pc_channel_tcp6,

    /* The 'local' type establishes a Unix domain socket on POSIX systems
       at a specified path. On Windows, it establishes a local-only named
       pipe.  */
    pc_channel_local,

    /* ### need to figure out read/write/duplex pipes.  */
    pc_channel_pipe

    /* ### RAW? unix socket dgram?  */
} pc_channel_type_t;

pc_error_t *pc_channel_create(pc_channel_t **new_channel,
                              pc_channel_type_t type,
                              pc_pool_t *pool);

void pc_channel_destroy(pc_channel_t *channel);


void pc_channel_track(const pc_channel_t *channel, pc_context_t *ctx);
void pc_channel_track_via(const pc_channel_t *channel, pc_pool_t *pool);
void pc_channel_owns(const pc_channel_t *channel, pc_pool_t *pool);

/* ### channels for stdio, stdout, stderr.
   ### do research about buffer sizes and binary-mode for these.  */


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
*/
pc_error_t *pc_channel_read_from(void **buf,
                                 size_t *len,
                                 const void **address,
                                 pc_channel_t *channel,
                                 pc_pool_t *pool);
pc_error_t *pc_channel_write_to(pc_channel_t *channel,
                                const void *address,
                                const void *buf,
                                size_t len,
                                pc_pool_t *pool);

/* ### accept, bind, connect, listen, options, shutdown.  */


/* ### docco on how long this runs. exit conditions. etc.
   ### time type.  */
pc_error_t *pc_channel_run_events(pc_context_t *ctx, uint64_t timeout, ...);


/* Callback should consume the data from BUF/LEN.

   ### an app should consume all available data? either toss the data, or
   ### copy to a separate buffer.
   ### hrm. it would be nice to have a way for the app to NOT read the
   ### data in order to create back-pressure against the writer. two
   ### options: boolean "stop" and consume all provided data, or a return
   ### len value saying "I consumed N bytes. hold the rest until I signal
   ### a desire to read again."
   ### letting the event system hold it could be preferable. it avoids a
   ### copy.
   ### the event system's read buffer can/should be adjustable via an API.
   ### if this API is provided, then we don't need a read size in the
   ### desire_read() call. this API also means the app can fine-tune the
   ### buffer for large data transfers, or keep it small for very high
   ### concurrency and small packets. this buffer size could be per-ob
   ### or possibly an event-system-wide value.

   This callback will be repeatedly invoked until all available data
   has been consumed.
   ### see above. we can signal "stop reading".

   ### whoop. CONTINUE is killed, too.
   After all available data has been consumed, the caller will examine
   the CONTINUE return value. If TRUE, then further data will be requested
   and provided to this callback when available. If FALSE, then this
   callback will be forgotten and the application must use desire_read()
   to set up another cycle of reads.
   ### CONTINUE can probably be implied by the return-value of a
   ### bytes-consumed param
   ### yes: if bytes-consumed < LEN, then we should stop reading. when
   ### consumed == LEN, then the callback is invoked again with more data,
   ### where it can signal 0 consumed. if the amount of data available
   ### is ONLY LEN, then the callback will be invoked again with BUF=NULL
   ### (see below). ... hmm. how to signal "give me more data"?
   ### maybe have CONSUMED be signed and (-1) says "I read everything and
   ### want more". any non-negative value signals "done". on a NULL call
   ### saying "no data yet", the callback can return -1 to indicate a
   ### desire for more data.

   The CONTINUE value is ignored while any data is pending.
   ### ugh. what if the app wants to toss some state, thinking it is done
   ### reading, yet the callback gets invoked again? hmm. how about a
   ### finalization callback? BUF=NULL to signal "no more". and maybe
   ### CONTINUE is examine *only* for that call?
   ### ooh. sounds good.

   ### it would be nice to *present* data. we might be able to do that
   ### portably. presenting data seems cleaner than coordinating with a
   ### separate read() call back into the system.

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


   ### OLD COMMENTARY before the "data presentation model"...

   ### must it exhaust the reading? maybe read some, then on to the next
   ### item available for reading, then eventually come back? what if an
   ### app does not exhaust the available readable data? throw error?
   ### maybe a system that says "gotta read at least 1 byte. I'll keep
   ### calling until all available data is read."

   Set *MORE to the amount of data the callback wants to continue reading.
   Set *MORE to 0 to indicate reading is no longer desired.
   ### how to say "whatever/everything I can get"?

   ### need dgram version
   ### scratch pool?
*/
typedef pc_error_t *(*pc_channel_readable_t)(ssize_t *consumed,
                                             const void *buf,
                                             size_t len,
                                             pc_channel_t *channel,
                                             void *baton);
#define PC_CONSUMED_CONTINUE ((ssize_t) -1)


/* Callback should fill in *BUF/*LEN with the data to write. After the
   data has been written, and CHANNEL is (again) available for writing,
   this callback will be invoked to request further data to write.

   Set *BUF to NULL and/or *LEN to 0 to indicate nothing futher to write.
   The callback will no longer be called. If the application wants to
   write more data to CHANNEL, then it must call desire_write().

   The returned BUF/LEN must exist, without change, until this callback
   is invoked again, or the channel is destroyed. The prior BUF may then
   be disposed.

   ### to best match serf and high-perf needs, this should be an iovec..

   ### more

   ### need dgram version
   ### scratch pool?
*/
typedef pc_error_t *(*pc_channel_writeable_t)(void **buf,
                                              size_t *len,
                                              pc_channel_t *channel,
                                              void *baton);

/* ### what to do here? the kinds of errors are scarily broad in scope.
   ### can we encapsulate all exceptions into this callback?

   ### pass one to desire_read and desire_write?
   ### maybe associate a triple-callback with CHANNEL, and then have
   ### signalling functions to look for read/write triggering?
   ### note that a set-of-functions means that we keep a single pointer
   ### per CHANNEL rather than a set of pointers. memory savings are
   ### important for the C10k problem. the set could contain other data,
   ### such as timeouts and a (debug) human-readable name.

   ### scratch pool?
*/
typedef pc_error_t *(*pc_channel_error_t)(error,
                                          pc_channel_t *channel,
                                          void *baton);


/* When the item has data, the callback will be invoked.

   ### this registers the channel with the event system (via pool->ctx),
   ### signaling a desire for reading data.

   ### the amount to read is determined by the buffer size (see set_readbuf),
   ### and the event system will continue reading, and invoking the callback,
   ### as long as data is available.

   ### add pool arg?
   ### more docco.  */
void pc_channel_desire_read(pc_channel_t *channel,
                            pc_event_readable_t callback,
                            void *baton);

/* When the item can be writen, the callback will be invoked.

   ### this registers the channel with the event system (via pool->ctx),
   ### signaling a desire forwriting data.

   ### add pool arg?
   ### more docco.  */
void pc_channel_desire_write(pc_channel_t *channel,
                             pc_event_writeable_t callback,
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
