/*
  track.c :  tracking lifespan/item dependencies

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
#include <stdint.h>

#include "pc_memory.h"
#include "pc_track.h"

#include "pocore.h"

#define TRACKING_STARTED(ctx) ((ctx)->track_pool != NULL)


static void
prepare_for_tracking(pc_context_t *ctx)
{
    if (!TRACKING_STARTED(ctx))
    {
        ctx->track_pool = pc_pool_root(ctx);
        ctx->ptr_to_reg = pc_hash_create(ctx->track_pool);
    }
}


static union pc_trackreg_u *
get_treg(pc_context_t *ctx)
{
    union pc_trackreg_u *result = ctx->free_treg;

    if (result == NULL)
        return pc_alloc(ctx->track_pool, sizeof(union pc_trackreg_u *));

    ctx->free_treg = result->f.next;
    return result;
}


static struct pc_tracklist_s *
get_tlist(pc_context_t *ctx)
{
    struct pc_tracklist_s *result = ctx->free_tlist;

    if (result == NULL)
        return pc_alloc(ctx->track_pool, sizeof(struct pc_tracklist_s *));

    ctx->free_tlist = result->next;
    return result;
}


static void
add_to_list(pc_context_t *ctx,
            struct pc_tracklist_s **list,
            union pc_trackreg_u *reg)
{
    struct pc_tracklist_s *tlist = get_tlist(ctx);

#ifdef PC_DEBUG
    {
        /* ### ensure this item is not already on the list.  */
    }
#endif

    tlist->reg = reg;
    tlist->next = *list;
    *list = tlist;
}


static void
remove_from_list(pc_context_t *ctx,
                 struct pc_tracklist_s **list,
                 union pc_trackreg_u *reg)
{
    struct pc_tracklist_s *scan = *list;
    struct pc_tracklist_s *tlist;

    /* This function can/should only be called to remove an item that is
       KNOWN to be in the list. Bail if the list doesn't even exist.  */
    assert(scan != NULL);

    /* Special handling for when the item is at the head of the list.  */
    if (scan->reg == reg)
    {
        *list = scan->next;
        scan->next = ctx->free_tlist;
        ctx->free_tlist = scan;
        return;
    }

    while (scan->next != NULL && scan->reg != reg)
        scan = scan->next;

    /* We should have found the item, rather than hit the end of the list.  */
    assert(scan->next != NULL);

    /* Remove from the specified list, and add the now-free structure into
       the context's free list.  */
    tlist = scan->next;
    scan->next = tlist->next;
    tlist->next = ctx->free_tlist;
    ctx->free_tlist = tlist;
}


static union pc_trackreg_u *
lookup_reg(pc_context_t *ctx, void *tracked)
{
    uintptr_t value;

    if (!TRACKING_STARTED(ctx))
        return NULL;

    value = (uintptr_t)tracked;
    return pc_hash_get(ctx->ptr_to_reg, &value, sizeof(value));
}


void pc_track(pc_context_t *ctx,
              void *tracked, pc_cleanup_func_t cleanup)
{
    union pc_trackreg_u *reg;

    prepare_for_tracking(ctx);

    reg = lookup_reg(ctx, tracked);
    if (reg == NULL)
    {
        uintptr_t value = (uintptr_t)tracked;

        /* Nothing in our tracking table yet. Start one.  */
        reg = get_treg(ctx);
        reg->a.cleanup_func = cleanup;
        reg->a.owners = NULL;
        reg->a.dependents = NULL;
        pc_hash_set(ctx->ptr_to_reg, &value, sizeof(value), reg);
    }
    else
    {
        /* Keep all the other data, but simply update the cleanup.  */
        reg->a.cleanup_func = cleanup;
    }
}


void pc_track_via(pc_pool_t *pool,
                  void *tracked, pc_cleanup_func_t cleanup)
{
    pc_track(pool->ctx, tracked, cleanup);
}


void pc_track_deregister(pc_context_t *ctx, void *tracked)
{
    union pc_trackreg_u *reg = lookup_reg(ctx, tracked);
    struct pc_tracklist_s *scan;

    /* Also handles TRACKING_STARTED.  */
    if (reg == NULL)
        return;

    if (reg->a.owners != NULL)
    {
        /* ### this is an error. register a programmer error.  */
        abort();
    }

    /* For each dependent, remove self from the owners list.  */
    for (scan = reg->a.dependents; scan != NULL; scan = scan->next)
        remove_from_list(ctx, &scan->reg->a.owners, reg);

    /* This registration structure is now available for re-use.  */
    reg->f.next = ctx->free_treg;
    ctx->free_treg = reg;
}


void pc_track_dependent(pc_context_t *ctx, void *owner, void *dependent)
{
    union pc_trackreg_u *reg_owner;
    union pc_trackreg_u *reg_dep;

    if (!TRACKING_STARTED(ctx))
    {
        /* ### this is an error. register a programmer error.  */
        abort();
    }

    reg_owner = lookup_reg(ctx, owner);
    if (reg_owner == NULL)
    {
        /* ### this is an error. register a programmer error.  */
        abort();
    }

    reg_dep = lookup_reg(ctx, dependent);
    if (reg_dep == NULL)
    {
        /* ### this is an error. register a programmer error.  */
        abort();
    }

    add_to_list(ctx, &reg_owner->a.dependents, reg_dep);
    add_to_list(ctx, &reg_dep->a.owners, reg_owner);
}


void pc_track_cleanup(pc_context_t *ctx, void *tracked)
{
    union pc_trackreg_u *reg = lookup_reg(ctx, tracked);

    /* Also handles TRACKING_STARTED.  */
    if (reg == NULL)
        return;

    if (reg->a.owners != NULL)
    {
        /* ### this is an error. register a programmer error.  */
        abort();
    }

    /* All good. Run the cleanup, then deregister this sucker.  */
    (*reg->a.cleanup_func)(tracked);

    /* ### switch this to an internal function. we can remove a number of
       ### redundant lookups/checks.  */
    pc_track_deregister(ctx, tracked);
}


void pc_track_cleanup_owners(void *tracked)
{
    /* ### probably not a public function. make private.  */
    abort();
}
