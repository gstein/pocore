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
    struct pc_block_s *nonstd_blocks;

    /* ### chained hashes to prevent realloc? subpool for this?  */
    pc_hash_t *ptr_to_track;
    union pc_trackreg_u *free_tracks;
    struct pc_pool_s *track_pool;
};


struct pc_pool_s {
    char *current;
    struct pc_block_s *current_block;

    struct pc_post_s *current_post;

    struct pc_pool_s *parent;
    struct pc_pool_s *temp_children; /* ### link these */

    struct pc_context_s *ctx;

    /* Inlined. Every pool has a set of owners (tho no dependents). Using
       a trackreg structure allows the owners to deregister/cleanup and
       to update the pool's tracking, like any other dependent.  */
    union pc_trackreg_u track;
};


struct pc_post_s {
    struct pc_pool_s *owner;
    char *saved_current;
    struct pc_block_s *saved_block;

    /* Any nonstd-sized blocks allocated after post was set.  */
    struct pc_block_s *nonstd_blocks;

    /* The saved value of pool->track.a.owners. Any owners registered since
       the post was set exist from the *current* value of .owners, along
       the linked list until SAVED_OWNERS is reached.  */
    struct pc_tracklist_s *saved_owners;

    struct pc_post_s *prev;
};


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* POCORE_H */
