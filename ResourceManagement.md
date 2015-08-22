

# Basics #

Applications need to properly manage any resources they use: memory, file descriptors, sockets, etc. In PoCore, the lifetimes of these resources is managed through pools (`pc_pool_t`).

When a resource is allocated/obtained, then a "cleanup function" should be registered with a pool. When that pool is cleared or destroyed, the cleanup function will be called. Typically, the pool used to allocate memory for the structure holding the reference to the resource will be used for the cleanup registration.

Cleanup functions are registered with the `pc_cleanup_register(POOL, DATA, CLEANUP)` function for each piece of data.

**Note**: the cleanups are associated with the **data item** (a pointer). Each data item may have only **one** cleanup function.

## Example ##

In this example, we are allocating a parser from a third-party library. To ensure that its resources will be cleaned up, we register a cleanup function at the time we create our container object.

```
static void cleanup(void *data)
{
    my_object *ob = data;

    FreeParser(ob->parser);
}

my_object *create_my_object(pc_pool_t *pool)
{
    my_object *me = pc_alloc(pool, sizeof(*me));

    me->parser = BuildParser();

    pc_cleanup_register(pool, me, cleanup);

    return me;
}
```

# Resources #

There are numerous types of resources an application needs to manage:

  * Memory
  * Descriptors (files, sockets, pipes, etc)
  * Handles (in Microsoft's Windows API)
  * Third party library objects
    * Database connections
    * Other objects and contexts (such as the parser example above)
  * Memory maps and shared memory
  * Mutexes and semaphores

In a PoCore application, the references/handles for the above items are invariably stored into memory allocated by a pool. Thus, the lifetime of the **storage** of the handle is limited to the pool's lifetime. We need to free/close the resource before we lose the handle, so the pool is the natural manager of that resource's lifetime.

# Ordering #

Pools are arranged into a tree: every pool has a set of zero or more child pools. These child pools are cleared/destroyed before the parent pool. Thus, any cleanup functions registered on child pools are executed before cleanups on the parent pool.

Within a single pool, the invocation order of cleanup functions is indeterminate. Applications must use the `pc_cleanup_before()` function to specify an ordering, if required.

For example, a socket and a protocol handler using that socket may have cleanup functions. One to close the socket and one to send a "goodbye" message. The socket cannot be cleaned up first, so the `pc_cleanup_before()` function is used:

```
    socket = create_socket(address, pool);
    pc_cleanup_register(pool, socket, close_socket);

    handler = create_handler(socket, pool);
    pc_cleanup_register(pool, handler, send_goodbye);

    /* Clean up the handler before the socket.  */
    pc_cleanup_before(pool, handler, socket);
```

## Complex Ordering ##

The cleanup ordering actually forms a directed acyclic graph. Multiple orderings may be defined across a set of data items:

```
    pc_cleanup_before(pool, A, C);
    pc_cleanup_before(pool, B, C);
    pc_cleanup_before(pool, C, D);
    pc_cleanup_before(pool, E, B);
```

In the above example, **A** or **E** will be cleaned up first (the order is indeterminate) since nothing has been specified as needing to be cleaned before them. Once **E** is cleaned up, then **B** can be cleaned up. Once **A** and **B** are cleaned, then **C**, then **D** may be cleaned.

Loops in the graph are not allowed; the result will be indeterminate cleanup ordering.

# Child Lifetimes #

In some cases, a resource's lifetime will mirror that of a child pool. This pattern is typically used for caches or other transient resources that are part of a longer-lived object.

For example:

```
static void cleanup(void *data)
{
    my_object *ob = data;

    FreeParser(ob->parser);
    ob->parser = NULL;
}

void maybe_build(my_object *ob, pc_pool_t *pool)
{
    if (ob->parser == NULL)
    {
        ob->parser = BuildParser();
        pc_cleanup_register(pool, ob, cleanup);
    }
}

...

    maybe_build(ob, child_pool);
    do_parse(ob->parser);

...

    /* Clean up temporary stuff.  */
    pc_pool_clear(child_pool);
```

in this example, `my_object` lives for a long time, while the parser's lifetime is bound to `child_pool`.

# Other #

The registered cleanup function for a given piece of data can be run manually:

```
    pc_cleanup_run(pool, data);
```

Or it can be removed altogether:

```
    pc_cleanup_deregister(pool, data);
```

# Caveats #

When a pool's cleanups are being run, there are some some things to be aware of:

  * It is possible to attach more cleanups to the pool
  * Child pools can be created
  * While destroying child pools, if a cleanup on a child pool **P** installs cleanup **C** on the parent, then **C** will be run after **P** has been been destroyed (and before other child pools continue to be destroyed). This means that **C** cannot depend upon data within **P**, although it may depend on other child pools that still exist.
  * The cleanups will seemingly disappear from the pool during their processing, which implies:
    * `pc_cleanup_before()` cannot reference one of those cleanups
    * `pc_cleanup_run()` and `pc_cleanup_deregister()` will do nothing since it will appear no cleanup function has been registered
    * `pc_cleanup_register()` can attach another function which will be run later during the pool clearing process
  * Cleanups are run before child pools are destroyed (so the cleanups can rely on data within a child pool)
  * When child pools are being destroyed, if a cleanup is detected as newly-registered on the pool, then the child pool destruction process will stop, in order to run the new cleanup.
  * It is possible to put the pool clearing process into an infinite loop by registering more cleanups within other cleanup callbacks. Developers should be wary about cleanup functions that could possibly register more cleanups.

**Note**: the above caveats only become important if a cleanup function is constructing child pools and/or registering new cleanups. Most cleanup functions will _close_ and _terminate_ resources, rather than _allocate_ more resources.