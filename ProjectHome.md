## Portability Core Library ##

PoCore provides key functionality for tools and servers that need to be portable across operating systems. It contains primitives for memory management, files, sockets, threading, and more.

Generally speaking, PoCore provides a portable set of APIs to the set of services that most tools/servers require. This is generally the "lowest common denominator" because once you ask for functionality **beyond** that, then you're delving into non-portable code. In some areas, PoCore will compensate for platforms' missing specific features; however, some concepts are inherently non-portable and are not provided for (e.g Windows ACLs versus POSIX permissions).

One of the first users of PoCore is intended to be the [serf http client library](http://code.google.com/p/serf/).


---

If you're familiar with the [Apache Portable Runtime](http://apr.apache.org/), then I've started a [comparison document](APRComparison.md). In short, the two projects have a different focus, and I believe PoCore is more carefully designed based on a decade of experience with APR, [Subversion](http://subversion.apache.org/), [Apache HTTPd](http://httpd.apache.org/), and [serf](http://code.google.com/p/serf/).