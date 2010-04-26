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

#include <string.h>

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


struct pc_block_s *get_block(pc_context_t *ctx)
{
    struct pc_block_s *block;

    /* ### get from context  */
    block = NULL;

    block->prev = NULL;
    block->next = NULL;
    block->size = ctx->stdsize;

    return NULL;
}


pc_pool_t *pc_pool_create(pc_pool_t *parent)
{
    struct pc_block_s *block = get_block(parent->ctx);
    pc_pool_t *pool;

    /* ### align these sizeof values?  */

    pool = (pc_pool_t *)((char *)block + sizeof(*block));
    memset(pool, 0, sizeof(*pool));

    pool->current = (char *)pool + sizeof(*pool);
    pool->current_block = block;
    pool->current_post = &pool->first_post;
    pool->parent = parent;
    pool->ctx = parent->ctx;

    pool->first_post.owner = pool;
    pool->first_post.saved_current = pool->current;
    pool->first_post.saved_block = block;

    /* Hook this pool into the parent.  */
    pool->sibling = parent->first_child;
    parent->first_child = pool;

    return pool;
}


pc_post_t *pc_post_create(pc_pool_t *pool)
{
    pc_post_t *post = pc_alloc(pool, sizeof(*post));

    post->owner = pool;
    post->saved_current = pool->current;
    post->saved_block = pool->current_block;
    post->nonstd_blocks = NULL;
    /* ### what if it isn't being tracked?  */
    post->saved_owners = pool->track.a.owners;
    post->prev = pool->current_post;

    pool->current_post = post;

    return post;
}


void cleanup_owners(struct pc_tracklist_s *owners)
{
}


void return_blocks(struct pc_block_s *blocks)
{
}


void return_nonstd(struct pc_block_s *nonstd)
{
}


void pc_pool_reset_to(pc_post_t *post)
{
    pc_pool_t *pool = post->owner;
    pc_post_t *cur = pool->current_post;

    if (post == NULL)
        post = &pool->first_post;

    while (TRUE)
    {
        /* While the pool is still intact, clean up all the owners that
           were established since we set the post.  */
        cleanup_owners(cur->saved_owners);

        pool->current = cur->saved_current;
        pool->current_block = cur->saved_block;

        /* Return all blocks (std and nonstd) that were allocated since
           we established the post.  */
        return_blocks(cur->saved_block->next);
        return_nonstd(cur->nonstd_blocks);

        if (cur == post)
            break;
        cur = cur->prev;
    }

    pool->current_post = post;
}
