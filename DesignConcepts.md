## Context ##

_TBD_

_see [pc\_misc.h](http://pocore.googlecode.com/svn/trunk/include/pc_misc.h)_

## Pools ##

See MemoryManagement.

_also see [pc\_memory.h](http://pocore.googlecode.com/svn/trunk/include/pc_memory.h)_

## Resource Management ##

See ResourceManagement.

**NOTE**: the code is in transition. The prior "tracking" concept for resource management is found in this header: [pc\_track.h](http://pocore.googlecode.com/svn/trunk/include/pc_track.h?r=167)

## Error Handling ##

See ErrorHandling.

_also see [pc\_error.h](http://pocore.googlecode.com/svn/trunk/include/pc_error.h)_

## Thread-Safety ##

PoCore does not associate locks with any of its structures. This means the objects are **not** **re-entrant**. Thread-safety is left entirely to the application, which knows best about the use of PoCore's objects. PoCore does not contain any global variables -- everything is rooted from a `pc_context_t`.

Generally, an application should allocate one `pc_context_t` per thread, and ensure that (tracking) relationships are not established across context boundaries.

It is certainly possible to share PoCore objects among threads (provided appropriate management by the application). PoCore objects do not have thread-affinity.

## Locking and Atomicty ##

See MutexPrimitives and AtomicPrimitives.

_see [pc\_mutex.h](http://pocore.googlecode.com/svn/trunk/include/pc_mutex.h)_

## Sockets and Event Handling ##

See [SocketAPI](SocketAPI.md).