The PoCore socket interface provides the standard set of (synchronous) functions. Sockets may also be placed into a non-blocking mode.

For performance, scalability, and portability reasons, a callback-based system will be used for processing socket events.
_(add background on why I think this is true)_

Rather than reinventing an event-based system, PoCore will provide tight integration of its sockets with a third-party library. See below.

## Event libraries ##

There are a number of choices for an event library:

  * [libev](http://software.schmorp.de/pkg/libev.html): this library uses global variables, so it is not possible to have multiple threads run an event loop

  * [Reconnoiter](https://labs.omniti.com/trac/reconnoiter): this system has a library named **eventer**, but (similar to libev) it also uses global variables, preventing multiple threads from running their own event loops

  * [libuv](https://github.com/joyent/libuv): this library was spun out of [Node.js](http://nodejs.org/), and is intended to be their portability layer. It incorporates Node.js's requirement for a purely asynchronous and highly-performant network layer, and it includes a solid Windows implementation. However, this library has grown from its "network I/O" roots into a larger system that may be burdensome for PoCore.

_**NOTE**: actually... globals might not be a problem for PoCore. after all, the operating system has globals, too (like file descriptors). as long we we can offer proper isolation at the PoCore API level, then we're good. as such, **eventer** will be investigated futher._

**NOTE**: PoCore has an initial implementation that is experimenting with libev as a validation of its API. Some changes are being made to libuv that may increase its attractiveness.

## Windows ##

None of the libraries researched so far provide a solid Windows implementation (although libuv is getting close; we may be able to provide feedback to libuv for our needs). **libev** uses BSD socket emulation and the `select()` function. **eventer** makes no provision for Windows at all.

High performance and scalable socket handling under Windows requires the use of [overlapped I/O](http://msdn.microsoft.com/en-us/library/ms686358(VS.85).aspx) (also see [this KB article](http://support.microsoft.com/kb/181611)).

Len Holgate has some code for this, along with many blog posts about the problem space. [Len's code](http://www.lenholgate.com/archives/000637.html) is not under an OSI-approved license, however, and is written in C++. Thus, it can only be a helpful reference rather than integrated alongside PoCore.

Also examine: http://social.msdn.microsoft.com/Forums/en/windowsgeneraldevelopmentissues/thread/10fc7d86-75d0-4594-adca-8c872877864d

After discussion with Ivan, we may use a pre-packaged event system for Unix-ish platforms, and implement a custom system using Windows' I/O Completion Ports.