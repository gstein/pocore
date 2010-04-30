/*
  memory.c :  memory handling functions

  ====================================================================
     Licensed to the Apache Software Foundation (ASF) under one
     or more contributor license agreements.  See the NOTICE file
     distributed with this work for additional information
     regarding copyright ownership.  The ASF licenses this file
     to you under the Apache License, Version 2.0 (the
     "License"); you may not use this file except in compliance
     with the License.  You may obtain a copy of the License at
    
       http://www.apache.org/licenses/LICENSE-2.0
    
     Unless required by applicable law or agreed to in writing,
     software distributed under the License is distributed on an
     "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
     KIND, either express or implied.  See the License for the
     specific language governing permissions and limitations
     under the License.
  ====================================================================
*/

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "pc_types.h"
#include "pc_memory.h"

#include "pocore.h"

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
    block = malloc(size
#ifdef PC_DEBUG
                   + 4
#endif
        );

    block->size = size;
    block->next = NULL;

#ifdef PC_DEBUG
    *(pc_u32_t *)((char *)block + size) = 0x12345678;
#endif
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
    pool->parent = NULL;
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


void cleanup_owners(struct pc_tracklist_s *owners)
{
    /* ### no tracking yet  */
}


void return_nonstd(struct pc_block_s *blocks)
{
    /* ### just throw them away for now. eventually, put them back into
       ### the context.  */
    while (blocks != NULL)
    {
        struct pc_block_s *next = blocks->next;

        /* ### TODO: put it back into the context  */

#ifdef PC_DEBUG
        /* ### simple guard against double-free  */
        assert(blocks->size != 0);
        assert(*(pc_u32_t *)((char *)blocks + blocks->size) == 0x12345678);
        blocks->size = 0;
#endif

        free(blocks);
        blocks = next;
    }
}


void return_blocks(pc_context_t *ctx, struct pc_block_s *blocks)
{
    if (blocks != NULL)
    {
        struct pc_block_s *scan = blocks;

        /* Scan to the last block in the chain.  */
        /* ### a pointer to the tail in CTX or in POST could remove this
           ### scan. WAIT... we DO have the tail. the last block is
           ### pool->current_block. oooh.... */
        while (scan->next != NULL)
            scan = scan->next;

        /* Insert the whole chain into the context.  */
        scan->next = ctx->std_blocks;
        ctx->std_blocks = blocks;
    }
}


void pc_pool_reset_to(pc_post_t *post)
{
    pc_pool_t *pool = post->owner;
    pc_post_t *cur = pool->current_post;

    POOL_USABLE(pool);

    /* Reset to each (newer) post, backwards through the chain, until we
       reach the desired post.  */
    while (TRUE)
    {
        pc_pool_t *scan;

        /* While the pool is still intact, clean up all the owners that
           were established since we set the post.

           NOTE: run these first, while the pool is still "unmodified". They
           may need something from this pool (ie. something with a longer
           lifetime which is sitting in this pool).  */
        cleanup_owners(cur->saved_owners);
        cur->saved_owners = NULL;

        /* Destroy all the child pools since this post was set.  */
        for (scan = cur->child; scan != NULL; scan = scan->sibling)
            pc_pool_destroy(scan);
        cur->child = NULL;

        pool->current = cur->saved_current;
        pool->current_block = cur->saved_block;

        /* Return all blocks (std and nonstd) that were allocated since
           we established the post.  */
        return_blocks(pool->ctx, cur->saved_block->next);
        cur->saved_block->next = NULL;

        return_nonstd(cur->nonstd_blocks);
        cur->nonstd_blocks = NULL;

        /* Remnants come only from blocks. Those blocks have been recovered,
           so there are no remnants available for use.  */
        cur->remnants = NULL;

        if (cur == post)
            break;
        cur = cur->prev;
    }

    pool->current_post = post;
}


void pc_pool_clear(pc_pool_t *pool)
{
    POOL_USABLE(pool);

    pc_pool_reset_to(&pool->first_post);
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
    return_blocks(pool->ctx, pool->current_block);
}


void *pc_alloc(pc_pool_t *pool, size_t amt)
{
    size_t remaining;
    void *result;
    struct pc_block_s *block;

    POOL_USABLE(pool);

    /* ### is 4 a good alignment? or maybe 8 bytes?  */
    amt = (amt + 3) & ~3;

    /* Can we provide the allocation out of the current block?  */
    remaining = (((char *)pool->current_block + pool->current_block->size)
                 - pool->current);
    if (remaining >= amt)
    {
        result = pool->current;
        pool->current += amt;
        return result;
    }

    /* ### look in the remnants  */

    if (amt <= pool->ctx->stdsize - sizeof(struct pc_block_s))
    {
        /* ### TODO: for the (old) CURRENT_BLOCK, save the remaining space
           ### into current_post->remnants. (if it is larger than
           ### sizeof(pc_memtree_s))
        */

        block = get_block(pool->ctx);

        result = (char *)block + sizeof(*block);

        pool->current_block->next = block;
        pool->current_block = block;

        pool->current = (char *)result + amt;

        return result;
    }

    /* We need a non-standard-sized allocation.  */

    /* ### get it from somewhere  */
    block = alloc_block(sizeof(*block) + amt);

    block->next = pool->current_post->nonstd_blocks;
    pool->current_post->nonstd_blocks = block;

    return (char *)block + sizeof(*block);
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
    char *result;

    if (end != NULL)
        amt = end - str;

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
    /* ### need to track...  */
}
