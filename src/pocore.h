/*
  pocore.h :  PoCore's internal declarations

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

#ifndef POCORE_H
#define POCORE_H

#include "pc_types.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ### for now, many of the library's core structures are library-visible.
   ### many will become private.
*/


/* Add debugging support. This is omitted from release builds.  */
/* ### not sure this construction is entirely right, but whatever. we want
   ### this stuff in here right now.  */
#ifndef NDEBUG
#define PC_DEBUG
#endif

/* Default and minimum standard block size.

   ### the minimum size (256) is just a number. the real minimum is probably
   ### sizeof(struct pc_memtree_s) with maybe some other padding. not sure
   ### that we allow such a small block though.  */
#define PC_MEMBLOCK_SIZE 8192
#define PC_MEMBLOCK_MINIMUM 256


struct pc_tracklist_s {
    struct pc_trackreg_s *reg;
    struct pc_tracklist_s *next;
};


/* Track registration record.  */
union pc_trackreg_u {
    struct {
        void (*cleanup_func)(void *tracked);

        /* ### use an array-based structure to eliminate NEXT ptrs?  */
        struct pc_tracklist_s *owners;
        struct pc_tracklist_s *dependents;
    } a;  /* allocated trackreg  */

    struct {
        union pc_trackreg_u *next;
    } f;  /* free'd trackreg  */
};


struct pc_block_s {
    /* This size INCLUDES the space used by this structure.  */
    size_t size;

    struct pc_block_s *next;
};


struct pc_context_s {
    /* ### return values: try one more time. return NULL. abort.  */
    int (*oom_handler)(size_t amt);

    /* When grabbing memory from the OS, what is the "standard size" to
       grab each time?  */
    size_t stdsize;

    /* A linked-list of available standard-sized blocks to use.  */
    struct pc_block_s *std_blocks;

    /* A tree of non-standard-sized blocks (ie. larger than STDSIZE). These
       are available for use on a best-fit basis.  */
    struct pc_memtree_s *nonstd_blocks;

    /* ### chained hashes to prevent realloc? subpool for this?
       ### we'll probably have the hash code return memory to its pool,
       ### so a realloc will not be much of a problem.  */
    pc_hash_t *ptr_to_track;
    union pc_trackreg_u *free_treg;
    struct pc_tracklist_s *free_tlist;
    struct pc_pool_s *track_pool;
};


struct pc_post_s {
    /* This post is placed in the OWNER pool.  */
    struct pc_pool_s *owner;

    /* Should allocations made after placing this post be coalescable?
       Or more specifically: when memory is returned to this post/pool,
       should we attempt to coalesce them?  */
    pc_bool_t coalesce;

    /* The original position within the saved block.  */
    char *saved_current;

    /* The original block allocations were coming from. pool->current_block
       may be the same, or linked from here via the ->next chain.  */
    struct pc_block_s *saved_block;

    /* Any remnants created after the post was set.  */
    struct pc_memtree_s *remnants;

    /* Any nonstd-sized blocks allocated after post was set. These will
       be queued back into the context when we reset to this post.  */
    struct pc_block_s *nonstd_blocks;

    /* The saved value of pool->track.a.owners. Any owners registered since
       the post was set exist from the *current* value of .owners, along
       the linked list until SAVED_OWNERS is reached.

       Each of these owners is (obviously) tracked. Upon reset, we will
       invoke the cleanup for each owner.  */
    struct pc_tracklist_s *saved_owners;

    /* Any child pools created since the post was set. These are linked
       through their SIBLING member.  */
    struct pc_pool_s *child;

    /* The previous post. The FIRST_POST will have prev=NULL.  */
    struct pc_post_s *prev;
};


struct pc_pool_s {
    char *current;

    /* Standard-size blocks are linked from the pool since a single block
       may be shared across multiple posts.  */
    struct pc_block_s *current_block;

    struct pc_post_s *current_post;

    struct pc_pool_s *parent;
    struct pc_pool_s *sibling;

    struct pc_context_s *ctx;

    /* Inlined. Every pool has a set of owners (tho no dependents). Using
       a trackreg structure allows the owners to deregister/cleanup and
       to update the pool's tracking, like any other dependent.

       When a trackreg is free'd, we can avoid putting this onto the
       FREE_TREG list by examing the CLEANUP_FUNC (is it the pool's func?)  */
    union pc_trackreg_u track;

    /* Allocate the first post as part of the pool.  */
    struct pc_post_s first_post;
};


/* A red-back binary tree containing pieces of memory to re-use.

   These pieces are:

     1) remnants from the end of a block that were "left behind" when we
        allocated and advanced to another block to satisfy a request.
     2) non-standard-sized (large) blocks that have been returned

   Note that the size of this structure provides a minimize size for
   remnants. If a remnant is smaller than this structure, it is simply
   "thrown away".

   We use red-black trees to guarantee worst-case time of O(log n) for
   all operations on this tree. We cannot afford O(n) worst case. For
   more information on red-black trees, see:
     http://en.wikipedia.org/wiki/Red-black_tree
*/
struct pc_memtree_s {
    /* The block contains this node's size, and NEXT field links to other
       (free) blocks of this same size.

       Note that the size's low-order bit is a flag. See the various
       macros in red_black.c.  */
    struct pc_block_s b;

    /* Any pieces that are SMALLER than this piece.  */
    struct pc_memtree_s *smaller;

    /* Any pieces that are LARGER than this piece.  */
    struct pc_memtree_s *larger;
};


/* ### docco  */
void
pc__memtree_insert(struct pc_memtree_s **root,
                   void *mem,
                   size_t size);


/* ### docco  */
struct pc_block_s *
pc__memtree_fetch(struct pc_memtree_s **root, size_t size);


#ifdef PC_DEBUG

int
pc__memtree_depth(const struct pc_memtree_s *node);

void
pc__memtree_print(const struct pc_memtree_s *root);

#endif /* PC_DEBUG  */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* POCORE_H */
