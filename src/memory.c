/*
  memory.c :  memory handling functions

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

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>

#include "pc_types.h"
#include "pc_memory.h"

#include "pocore.h"

/* For design/implementation information, see:
     http://code.google.com/p/pocore/wiki/MemoryManagement
*/

/*
  "apr pools & memory leaks" from Ben, with Google's experiences
  http://mail-archives.apache.org/mod_mbox/apr-dev/200810.mbox/%3C53c059c90810011111v37c36635y7279870f9bc852a0@mail.gmail.com%3E

  Consider an app that (over long periods of time) allocates 10k, 20k, 30k,
  40k, 50k, ...  It is also allocating smaller pieces, which are being
  fulfilled by existing free blocks. However, over the long haul, a new
  peak arrives which requires a new malloc() block. Or possibly, that 50k
  block sitting in the free pool satisfies a 45k alloc, and another 45k
  comes in, requesting a new malloc block.

  Unless we are guaranteed as the manager of sbrk(), we cannot assume that
  free() will return memory to the system. It could very well be below
  the break value, thus not able to return memory. We could very well use
  mmap to allocate/return blocks of memory. Providing a threshold size for
  switching over to mmap would be helpful. Maybe some mallocs automatically
  use malloc? ie. how did Google's situation automagically improve by using
  the free() function? Coalescing within the heap?

  Finding a way to coalesce blocks would be good, but the best case is at
  pc_block size. ie. we could coalesce everything within a block, but there
  is no way to coalesce across blocks. Given that we want to limit the block
  size (allocating 200M wouldn't be good), then we also limit our maximum
  result of coalescing. Given a target block size of N, it is a fair
  assumption that over a long period of time, numerous requests will come
  in for sizes greater than N. No matter where we establish the value,
  a long-running process with variant memory consumption could blast it.

  Heh. One answer is "wtf you doing allocating unbounded memory?"
*/

#ifdef PC_DEBUG
#define POOL_USABLE(pool) assert((pool)->current != NULL);
#else
#define POOL_USABLE(pool) ((void)0)
#endif


#ifdef PC__IS_WINDOWS
#define ALLOC_BLOCK(ctx, size) alloc_block(ctx, size)
#else
#define ALLOC_BLOCK(ctx, size) alloc_block(size)
#endif

static struct pc_block_s *alloc_block(
#ifdef PC__IS_WINDOWS
    pc_context_t *ctx,
#endif
    size_t size)
{
    struct pc_block_s *block;

    /* ### should get a block from the context. for now: early bootstrap
       ### with a simple malloc.  */
#ifdef PC__IS_WINDOWS
    block = HeapAlloc(ctx->heap, 0 /* dwFlags */, size)
#elif 1
    block = malloc(size);
#else
    block = mmap(0, size, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
    if (block == (void *)-1)
    {
        int problem = errno;
        abort();
    }
#endif

    block->size = size;
    block->next = NULL;

    return block;
}


static struct pc_memroot_s *alloc_memroot(pc_context_t *ctx,
                                          size_t stdsize)
{
    /* ### we should also grab mem from CTX->NONSTD_BLOCKS. the resulting
       ### size could be non-standard. gotta adjust other stuff first.  */
    struct pc_block_s *block = ALLOC_BLOCK(ctx, stdsize);
    struct pc_memroot_s *memroot;

    /* We can completely forget the pc_block_s. Recast.  */
    memroot = (struct pc_memroot_s *)block;

    /* memroot->pool set by caller.  */
    memroot->stdsize = stdsize;
    memroot->std_blocks = NULL;
    memroot->ctx = ctx;

    /* Hook into the context.  */
    memroot->next = ctx->memroots;
    ctx->memroots = memroot;

    return memroot;
}


static struct pc_block_s *get_block(pc_context_t *ctx)
{
    struct pc_block_s *result;

    if (ctx->std_blocks == NULL)
        return ALLOC_BLOCK(ctx, ctx->stdsize);

    result = ctx->std_blocks;
    ctx->std_blocks = result->next;
    result->next = NULL;
    return result;
}


pc_pool_t *pc_pool_root_custom(pc_context_t *ctx,
                               size_t stdsize)
{
    struct pc_memroot_s *memroot = alloc_memroot(ctx, stdsize);
    pc_pool_t *pool;

    /* ### align the structure size, rather than using +1 ?  */

    pool = (pc_pool_t *)(memroot + 1);
    memset(pool, 0, sizeof(*pool));

    pool->current = (char *)(pool + 1);
    pool->endmem = (char *)memroot + stdsize;
    pool->initial_endmem = pool->endmem;
    pool->memroot = memroot;

    /* ### need to remember this pool, and destroy it at ctx destroy.
       ### see above memroot and ctx->roots.  */

    return pool;
}


pc_pool_t *pc_pool_root(pc_context_t *ctx)
{
    return pc_pool_root_custom(ctx, ctx->stdsize);
}


pc_pool_t *pc_pool_create(pc_pool_t *parent)
{
    struct pc_block_s *block = get_block(parent->memroot->ctx);
    pc_pool_t *pool;

    /* ### align the structure size, rather than using +1 ?  */

    pool = (pc_pool_t *)(block + 1);
    memset(pool, 0, sizeof(*pool));

    pool->current = (char *)(pool + 1);
    pool->endmem = (char *)block + block->size;
    pool->initial_endmem = pool->endmem;
    pool->memroot = parent->memroot;

    /* Hook this pool into the parent.  */
    pool->parent = parent;
    pool->sibling = parent->child;
    parent->child = pool;

    return pool;
}


pc_pool_t *pc_pool_create_coalescing(pc_pool_t *parent)
{
    pc_pool_t *pool = pc_pool_create(parent);

    pool->coalesce = TRUE;

    return pool;
}


void pc_pool_clear(pc_pool_t *pool)
{
    pc_context_t *ctx = pool->memroot->ctx;
    struct pc_block_s *nonstd;

    POOL_USABLE(pool);

    /* NOTE: it is possible for cleanups to create an infinite loop.

       Possibility: the cleanup function registers other cleanup
       functions which register more, etc, such that we can never
       empty the set of owners of this pool.

       Possibility: cleanup functions on child pools register a
       cleanup on this parent pool, and the cleanup creates that
       child pool and its cleanup, etc.

       These are application problems that we will not attempt to
       detect or counter.

       That said, it *is* legal for cleanups to create child pools,
       to add new cleanups, and for those pools to add cleanups or
       create other child pools. As long as the sequence reaches a
       steady-state of destruction.
    */

    {
        /* While the pool is still intact, run all registered cleanups.

           These cleanups are *ordered*, based on calls to
           pc_cleanup_before(). POOL->CLEANUPS must be called before
           the cleanups later in the list.

           NOTE: run these first, while the pool is still "unmodified".
           They may need something from this pool (ie. something with a
           longer lifetime which is sitting in this pool), or maybe
           something from a child pool.

           NOTE: implementation detail: this will run until the cleanup
           list is empty. If cleanup handlers attach more cleanups, then
           they will be executed immediately.

           NOTE: if child pool destruction happens to attach a cleanup
           to *this* pool, then we return and run the cleanup(s) while
           the child pool(s) exist. That cleanup may need the pool, so
           running the cleanup takes priority over destroying any
           child pools.
        */
        while (pool->cleanups != NULL)
        {
            struct pc_cleanup_list_s *cl;

          run_cleanups:
            cl = pool->cleanups;

            /* Yank CL from the list of cleanups, then run it.  */
            pool->cleanups = cl->next;
            cl->cleanup((/* const */ void *)cl->data);
        }

        /* Destroy all the child pools. Children will remove themselves
           from this list as they are destroyed, so we just keep destroying
           the head of the list until nothing is left.

           Note that cleanups (run just above, or associated with these
           child pools) may add more subpools. Not a problem, as we will
           torch them here.  */
        while (pool->child != NULL)
        {
            pc_pool_destroy(pool->child);

            /* Woah. A cleanup handler in a child pool attached a cleanup
               onto THIS pool. This cleanup takes priority, as it may
               require access to data in a child pool.  */
            if (pool->cleanups != NULL)
                goto run_cleanups;  /* don't tell me gotos are harmful :-)  */
        }
    }

    /* Return all the non-standard-sized blocks to the context. Use a
       local variable to avoid continual POOL dereferences.  */
    if ((nonstd = pool->nonstd_blocks) != NULL)
    {
        do
        {
            struct pc_block_s *next = nonstd->next;

            pc__memtree_insert(&ctx->nonstd_blocks, nonstd, nonstd->size);

            nonstd = next;

        } while (nonstd != NULL);

        pool->nonstd_blocks = NULL;
    }

    /* The pool structure is allocated within memory defined by POOL and
       INITIAL_ENDMEM. We need to return any blocks allocated *after* that
       back to the context. These blocks are linked via EXTRA_HEAD/TAIL.  */
    if (pool->extra_head != NULL)
    {
        /* Link the blocks.  */
        pool->extra_tail->next = ctx->std_blocks;
        ctx->std_blocks = pool->extra_head;

        /* Detach those blocks from our knowledge.  */
        pool->extra_head = NULL;
    }

    /* Get ready for the next allocation.  */
    pool->current = (char *)pool + sizeof(*pool);
    pool->endmem = pool->initial_endmem;

    /* All the extra blocks have been returned, and we've reset the "First"
       block. Thus, there are no more remnants.  */
    pool->remnants = NULL;
}


void pc_pool_destroy(pc_pool_t *pool)
{
    pc_context_t *ctx = pool->memroot->ctx;

    POOL_USABLE(pool);

    /* Clear out everything in the pool.  */
    pc_pool_clear(pool);

#ifdef PC_DEBUG
    /* Leave a marker that we've destroyed this pool already. This will
       also prevent further attempts at use.  */
    pool->current = NULL;
#endif

    /* All extra blocks should have been returned.  */
    assert(pool->extra_head == NULL);

    /* Remove this pool from the parent's list of child pools.  */
    if (pool->parent != NULL)
    {
        pc_pool_t *scan = pool->parent->child;
        struct pc_block_s *block;

        if (scan == pool)
        {
            /* We're at the head of the list. Point it to the next pool.  */
            pool->parent->child = pool->sibling;
        }
        else
        {
            /* Find the child pool which refers to us, and then reset its
               sibling link to skip self.

               NOTE: we should find POOL in this list, so we don't need to
               check for end-of-list.  */
            while (scan->sibling != pool)
                scan = scan->sibling;

            /* ### assert scan != NULL  */
            scan->sibling = pool->sibling;
        }

        /* This pool was allocated within a tree of pools. Thus, it used
           a standard-sized block for its allocation. Return that block
           to the set in CTX.  */
        block = (struct pc_block_s *)((char *)pool - sizeof(*block));
        block->next = ctx->std_blocks;
        ctx->std_blocks = block;
    }
    else
    {
        struct pc_block_s *scan;

        /* This is a root pool, so the pool structure was allocated as
           part of a memroot. That memroot memory simply needs to be
           free'd.  */

        /* Get rid of all the blocks we allocated for this memroot.

           ### in the future, we can move these into the context's nonstd
           ### block storage for later reuse.  */
        for (scan = pool->memroot->std_blocks; scan != NULL; )
        {
            struct pc_block_s *next = scan->next;

            PC__FREE(ctx, scan);
            scan = next;
        }

        /* ### in the future, we can insert the memory into the context's
           ### NONSTD_BLOCKS for later reuse. can't do that right now
           ### because we don't go look for new pool memory in the nonstd
           ### area. until we reuse that, create/destroy pool would malloc
           ### (unbounded) a new block for each pool.  */
        PC__FREE(ctx, pool->memroot);
    }
}


static void *
internal_alloc(pc_pool_t *pool, size_t amt)
{
    size_t remaining;
    void *result;
    struct pc_block_s *block;
    pc_context_t *ctx;

    /* Can we provide the allocation out of the current block?  */
    remaining = pool->endmem - pool->current;
    if (remaining >= amt)
    {
        result = pool->current;
        pool->current += amt;
        return result;
    }

    /* The remnants tree might have a free block for us.  */
    block = pc__memtree_fetch(&pool->remnants, amt);
    if (block != NULL)
    {
        size_t remnant_remaining;

        result = block;

        /* If there is extra space at the end of the remnant, then put it
           back into the remnants tree.  */
        remnant_remaining = block->size - amt;
        /* ### keep track of small bits?  */
        if (remnant_remaining > sizeof(struct pc_memtree_s))
        {
            pc__memtree_insert(&pool->remnants,
                               (char *)block + amt,
                               remnant_remaining);
        }

        return result;
    }

    ctx = pool->memroot->ctx;

    /* Will the requested amount fit within a standard-sized block?  */
    if (amt <= ctx->stdsize - sizeof(struct pc_block_s))
    {
        /* There is likely space at the end of the current allocation
           (ie. the space between CURRENT and ENDMEM), so save that into
           the remnants tree.  */
        /* ### keep track of small bits?  */
        if (remaining > sizeof(struct pc_memtree_s))
        {
            pc__memtree_insert(&pool->remnants, pool->current, remaining);
        }

        /* Grab a standard-sized block, and return a portion of it. There
           may be memory after the result, usable for later allocations.  */

        block = get_block(ctx);

        result = (char *)block + sizeof(*block);

        /* Remember that we grabbed an extra block for this pool. Place
           it onto the tail of our list of blocks.  */
        if (pool->extra_head == NULL)
        {
            pool->extra_head = pool->extra_tail = block;
        }
        else
        {
            pool->extra_tail->next = block;
            pool->extra_tail = block;
        }

        /* Set up the pointers for later allocations.  */
        pool->current = (char *)result + amt;
        pool->endmem = (char *)block + block->size;

        return result;
    }

    /* We need a non-standard-sized allocation.  */
    {
        size_t required = sizeof(*block) + amt;

        block = pc__memtree_fetch(&ctx->nonstd_blocks, required);
        if (block == NULL)
            block = ALLOC_BLOCK(ctx, required);

        block->next = pool->nonstd_blocks;
        pool->nonstd_blocks = block;

        /* ### TODO: the block pulled out of the tree may be larger than
           ### we need. the extra should go into the REMNANTS tree.  */

        return (char *)block + sizeof(*block);
    }
}


static void *
coalesce_alloc(pc_pool_t *pool, size_t amt)
{
    char *result = internal_alloc(pool, amt + sizeof(size_t));

    *(size_t *)(result + amt) = amt;
    return result;
}


void *pc_alloc(pc_pool_t *pool, size_t amt)
{
    POOL_USABLE(pool);

    /* ### is 4 a good alignment? or maybe 8 bytes?  */
    amt = (amt + 3) & ~3;

    if (pool->coalesce)
        return coalesce_alloc(pool, amt);
    return internal_alloc(pool, amt);
}


void pc_pool_freemem(pc_pool_t *pool, void *mem, size_t len)
{
    /* ### should we try and remember these small bits?  */
    if (len < sizeof(struct pc_memtree_s))
        return;

    /* ### coalesce  */

    pc__memtree_insert(&pool->remnants, mem, len);
}


char *pc_strdup(pc_pool_t *pool, const char *str)
{
    size_t len = strlen(str);

    return pc_strmemdup(pool, str, len);
}


char *pc_strmemdup(pc_pool_t *pool, const char *str, size_t len)
{
    char *result = pc_alloc(pool, len + 1);

    memcpy(result, str, len);
    result[len] = '\0';
    return result;
}


char *pc_strndup(pc_pool_t *pool, const char *str, size_t amt)
{
    const char *end = memchr(str, '\0', amt);

    if (end != NULL)
        amt = end - str;

    /* ### maybe use an inline helper instead?  */
    return pc_strmemdup(pool, str, amt);
}


void *pc_memdup(pc_pool_t *pool, void *mem, size_t len)
{
    void *result = pc_alloc(pool, len);

    memcpy(result, mem, len);
    return result;
}


char *pc_strcat(pc_pool_t *pool, ...)
{
    return NULL;
}


char *pc_vsprintf(pc_pool_t *pool, const char *fmt, va_list ap)
{
    return NULL;
}


char *pc_sprintf(pc_pool_t *pool, const char *fmt, ...)
{
    return NULL;
}
