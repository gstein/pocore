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
    /* ### do we need a doubly-linked list? HEAD of the list of blocks
       ### assigned to this pool is pool->first_post.saved_block. the
       ### TAIL is at pool->current_block.  */
    struct pc_block_s *prev;
    struct pc_block_s *next;
    size_t size;
};


struct pc_context_s {
    /* ### return values: try one more time. return NULL. abort.  */
    int (*oom_handler)(size_t amt);

    size_t stdsize;
    struct pc_block_s *std_blocks;

    /* ### use a bintree to do bestfit.  */
    struct pc_memtree_s *nonstd_blocks;

    /* ### chained hashes to prevent realloc? subpool for this?  */
    pc_hash_t *ptr_to_track;
    union pc_trackreg_u *free_treg;
    struct pc_tracklist_s *free_tlist;
    struct pc_pool_s *track_pool;
};


struct pc_post_s {
    struct pc_pool_s *owner;
    char *saved_current;
    struct pc_block_s *saved_block;

    /* Any nonstd-sized blocks allocated after post was set. These will
       be queued back into the context when we reset to this post.  */
    struct pc_block_s *nonstd_blocks;

    /* The saved value of pool->track.a.owners. Any owners registered since
       the post was set exist from the *current* value of .owners, along
       the linked list until SAVED_OWNERS is reached.

       Each of these owners is (obviously) tracked. Upon reset, we will
       invoke the cleanup for each owner.  */
    struct pc_tracklist_s *saved_owners;

    /* The previous post. The FIRST_POST will have prev=NULL.  */
    struct pc_post_s *prev;
};


struct pc_pool_s {
    char *current;
    struct pc_block_s *current_block;

    struct pc_memtree_s *remnants;

    struct pc_post_s *current_post;

    struct pc_pool_s *parent;

    /* ### use an array?  */
    struct pc_pool_s *first_child;
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


struct pc_memtree_s {
    size_t size;

    struct pc_memtree_s *left;
    struct pc_memtree_s *right;
};


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* POCORE_H */
