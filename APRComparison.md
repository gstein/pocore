_this page needs to be fleshed out. here is a raw dump from IRC._

"gstein: Just out of curiosity, why are you writing pocore? Is [APR](http://apr.apache.org/) too slow or does it miss something?"

  * pocore can free() memory back to the pool. this is **very** useful for [serf](http://code.google.com/p/serf/). today, serf does a lot of ugly stuff to compensate.
  * second, is that APR wants to be "library portable", not just "operating system portable". so it has DB portability. LDAP portability. and other similar kinds of crap. I think they're removing the LDAP stuff, thankfully. but the point is that pocore is focused on just OS. the APR community disagreed with my request for that focus, and want to stick to the larger portability question
  * and third, pocore is written based on 10 years of experience with APR, whereas APR is mostly just an extraction of code from [httpd](http://httpd.apache.org/). it was never rethought from scratch.
  * so... with that rethinking, I have a better memory allocator. it is faster **and** has more features.
  * the dependency tracking is better, and suits the complex arrangements needed by [svn](http://subversion.apache.org/).
  * the hash table uses actual research for its hash function selection ([FNV-1](http://www.isthe.com/chongo/tech/comp/fnv/))
  * better error handling (similar to svn's)
  * no globals