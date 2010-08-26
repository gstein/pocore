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


struct pc_block_s *alloc_block(size_t size)
{
    struct pc_block_s *block;

    /* ### should get a block from the context. for now: early bootstrap
       ### with a simple malloc.  */
#if 1
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

struct pc_block_s *get_block(pc_context_t *ctx)
{
    struct pc_block_s *result;

    if (ctx->std_blocks == NULL)
        return alloc_block(ctx->stdsize);

    result = ctx->std_blocks;
    ctx->std_blocks = result->next;
    result->next = NULL;
    return result;
}


pc_pool_t *pc_pool_root(pc_context_t *ctx)
{
    struct pc_block_s *block = get_block(ctx);
    pc_pool_t *pool;

    /* ### align these sizeof values?  */

    pool = (pc_pool_t *)((char *)block + sizeof(*block));
    memset(pool, 0, sizeof(*pool));

    pool->current = (char *)pool + sizeof(*pool);
    pool->current_block = block;
    pool->current_post = &pool->first_post;
    pool->ctx = ctx;

    pool->first_post.owner = pool;
    pool->first_post.saved_current = pool->current;
    pool->first_post.saved_block = block;

    return pool;
}


pc_pool_t *pc_pool_create(pc_pool_t *parent)
{
    pc_pool_t *pool = pc_pool_root(parent->ctx);

    pool->parent = parent;

    /* Hook this pool into the parent's current post.  */
    pool->sibling = parent->current_post->child;
    parent->current_post->child = pool;

    return pool;
}


pc_post_t *pc_post_create(pc_pool_t *pool)
{
    pc_post_t *post = pc_alloc(pool, sizeof(*post));

    POOL_USABLE(pool);

    post->owner = pool;
    post->coalesce = FALSE;
    post->saved_current = pool->current;
    post->saved_block = pool->current_block;
    post->remnants = NULL;
    post->nonstd_blocks = NULL;
    /* ### what if it isn't being tracked?  */
    post->saved_owners = pool->track.a.owners;
    post->child = NULL;
    post->prev = pool->current_post;

    pool->current_post = post;

    return post;
}


pc_post_t *pc_post_create_coalescing(pc_pool_t *pool)
{
    pc_post_t *post = pc_post_create(pool);

    post->coalesce = TRUE;

    return post;
}


void return_nonstd(pc_context_t *ctx, struct pc_block_s *blocks)
{
    /* Put all the blocks back into the context.  */
    while (blocks != NULL)
    {
        struct pc_block_s *next = blocks->next;

        pc__memtree_insert(&ctx->nonstd_blocks, blocks, blocks->size);

        blocks = next;
    }
}


void pc_post_recall(pc_post_t *post)
{
    pc_pool_t *pool = post->owner;
    pc_context_t *ctx = pool->ctx;
    pc_post_t *cur = pool->current_post;

    POOL_USABLE(pool);

    /* Reset to each (newer) post, backwards through the chain, until we
       reach the desired post.  */
    while (TRUE)
    {
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

           It is possible for the cleanup handlers to shoot themselves
           in the foot: if a child pool cleanup attaches a new handler
           to this pool, and that handler requires data from a child
           pool, then it will be in trouble. All child pools are destroyed
           before running the cleanup handlers (again), so when that new
           handler is run... the child pool will be gone.

           The simplest answer is for child pool cleanups to never attach
           anything to the parent pool.
        */

        do
        {
            /* While the pool is still intact, clean up all the owners that
               were established since we set the post.

               NOTE: run these first, while the pool is still "unmodified".
               They may need something from this pool (ie. something with a
               longer lifetime which is sitting in this pool), or maybe
               something from a child pool.  */
            pc__track_cleanup_owners(pool, cur->saved_owners);

            /* It is (remotely) possible that child pool destruction will
               save new owners against this parent pool. We need the set
               of owners to be correct and ready for that eventuality.  */
            cur->saved_owners = NULL;

            /* Destroy all the child pools since this post was set. Children
               will remove themselves from this list as they are destroyed.
               The removal should be fast since this is the current post and
               the child we are removing will be at the head of this list.

               Note that cleanups may add subpools. Not a problem, as we
               will torch them here.  */
            while (cur->child != NULL)
                pc_pool_destroy(cur->child);

            /* If a child pool has set new owners against this pool, then
               we need loop back to clean them up.  */
        } while (cur->saved_owners != NULL);

        /* If a block was allocated AFTER we established the post, then
           return those blocks.  */
        if (cur->saved_block->next != NULL)
        {
            /* Insert the whole chain into the context. HEAD is the block
               allocated *after* the save point, and TAIL is the current
               pool reference.  */
            pool->current_block->next = ctx->std_blocks;
            ctx->std_blocks = cur->saved_block->next;

            /* Forget the newly-allocated blocks.  */
            cur->saved_block->next = NULL;

            /* The pool should back up to the block that was being used
               when this post was set.  */
            pool->current_block = cur->saved_block;
        }

        /* Return all the non-standard-sized blocks to the context.  */
        return_nonstd(ctx, cur->nonstd_blocks);

        /* Reset the pool's pointer to the original state. Future
           iterations may need this, and final-state of the pool
           definitely requires it.  */
        pool->current = cur->saved_current;

        if (cur == post)
        {
            /* Put this post back into a useful state.  */

            /* This post no longer has associated non-standard-sized
               blocks.  */
            cur->nonstd_blocks = NULL;

            /* Remnants come only from blocks. Those blocks have been
               recovered, so there are no remnants available for use.  */
            cur->remnants = NULL;

            /* DONE!  */
            return;
        }

        /* We are recalling to an earlier post.  */
        cur = cur->prev;

        /* Set up the current post for operations that may occur during
           this recall process.  */
        pool->current_post = cur;
    }
}


void pc_pool_clear(pc_pool_t *pool)
{
    POOL_USABLE(pool);

    pc_post_recall(&pool->first_post);
}


void pc_pool_destroy(pc_pool_t *pool)
{
    POOL_USABLE(pool);

    /* Clear out everything in the pool.  */
    pc_pool_clear(pool);

    /* Remove this pool from the parent's list of child pools.  */
    if (pool->parent != NULL)
    {
        pc_post_t *post;

        /* We don't actually need a condition on this for loop because
           we KNOW that we'll find the child somewhere.  */
        for (post = pool->parent->current_post; ; post = post->prev)
        {
            pc_pool_t *scan = post->child;

            if (scan == pool)
            {
                /* We're at the head of the list. Point it to the
                   next pool.  */
                post->child = pool->sibling;
                break;
            }
            else
            {
                /* Find the child pool which refers to us, and then reset its
                   sibling link to skip self.  */
                while (scan != NULL && scan->sibling != pool)
                    scan = scan->sibling;
                if (scan != NULL)
                {
                    scan->sibling = pool->sibling;
                    break;
                }

                /* Move on to the next post.  */
            }
        }
    }

#ifdef PC_DEBUG
    /* Leave a marker that we've destroyed this pool already. This will
       also prevent further attempts at use.  */
    pool->current = NULL;
#endif

    /* Return the last block (which also contains this pool) to
       the context.  */
    assert(pool->current_block->next == NULL);
    pool->current_block->next = pool->ctx->std_blocks;
    pool->ctx->std_blocks = pool->current_block;
}


static void *
internal_alloc(pc_pool_t *pool, size_t amt)
{
    size_t remaining;
    void *result;
    struct pc_block_s *block;

    /* Can we provide the allocation out of the current block?  */
    remaining = (((char *)pool->current_block + pool->current_block->size)
                 - pool->current);
    if (remaining >= amt)
    {
        result = pool->current;
        pool->current += amt;
        return result;
    }

    /* The remnants tree might have a free block for us.  */
    block = pc__memtree_fetch(&pool->current_post->remnants, amt);
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
            pc__memtree_insert(&pool->current_post->remnants,
                               (char *)block + amt,
                               remnant_remaining);
        }

        return result;
    }

    /* Will the requested amount fit within a standard-sized block?  */
    if (amt <= pool->ctx->stdsize - sizeof(struct pc_block_s))
    {
        /* There is likely space at the end of CURRENT_BLOCK, so save that
           into the remnants tree.  */
        /* ### keep track of small bits?  */
        if (remaining > sizeof(struct pc_memtree_s))
        {
            pc__memtree_insert(&pool->current_post->remnants,
                               pool->current,
                               remaining);
        }

        block = get_block(pool->ctx);

        result = (char *)block + sizeof(*block);

        /* Append the new block to the end of the pool's chain of blocks.  */
        pool->current_block->next = block;
        pool->current_block = block;

        pool->current = (char *)result + amt;

        return result;
    }

    /* We need a non-standard-sized allocation.  */
    {
        size_t required = sizeof(*block) + amt;

        block = pc__memtree_fetch(&pool->ctx->nonstd_blocks, required);
        if (block == NULL)
            block = alloc_block(required);

        block->next = pool->current_post->nonstd_blocks;
        pool->current_post->nonstd_blocks = block;

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

    if (pool->current_post->coalesce)
        return coalesce_alloc(pool, amt);
    return internal_alloc(pool, amt);
}


void pc_post_freemem(pc_post_t *post, void *mem, size_t len)
{
    /* ### should we try and remember these small bits?  */
    if (len < sizeof(struct pc_memtree_s))
        return;

    /* ### coalesce  */

    pc__memtree_insert(&post->remnants, mem, len);
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


void pc_pool_track(pc_pool_t *pool)
{
    /* We have a tracking structure built right into the pool for easier
       manipulation of its owners. We need to jam that into the tracking
       registry directly. Use a special entry point in track.c  */
    pc__track_this_pool(pool);
}
