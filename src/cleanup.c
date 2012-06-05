/*
  cleanup.c :  data cleanup functionality

  ====================================================================
    Copyright 2012 Greg Stein

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
#include <stdint.h>

#include "pc_memory.h"
#include "pc_cleanup.h"

#include "pocore.h"


/* Get an available cleanup list item, or allocate one.  */
static struct pc_cleanup_list_s *
get_cl(pc_context_t *ctx)
{
    struct pc_cleanup_list_s *result = ctx->free_cl;

    if (result == NULL)
        return pc_alloc(ctx->cleanup_pool, sizeof(*result));

    ctx->free_cl = result->next;
    return result;
}


/* Locate a record with DATA in it, and return the associated CLEANUP.
   Store NULL into CLEANUP if no record is found.

   The record will be extracted from the list rooted at HEAD, and then
   inserted into the free list on CTX.  */
static void
extract_cleanup(pc_cleanup_func_t *cleanup,
                struct pc_cleanup_list_s **head,
                pc_context_t *ctx,
                const void *data)
{
    /* HEAD points to a "next" pointer (or the actual head). If the
       targeted record is what we're looking for, then start munging
       various lists.  */
    while (*head != NULL)
    {
        if ((*head)->data == data)
        {
            struct pc_cleanup_list_s *cl = *head;

            /* Return the one interesting piece of data.  */
            *cleanup = cl->cleanup;

            /* Remove the record (CL) from the active list of cleanups.
               This basically jumps the prior record's "next" pointer
               over CL.  */
            *head = cl->next;

            /* Now insert CL into the context's list of free records.  */
            cl->next = ctx->free_cl;
            ctx->free_cl = cl;

            /* Woot. Done!  */
            return;
        }

        /* Shift forward on the list, but keeping a pointer to the prior
           record's "next" pointer so we can potentially adjust it.  */
        head = &(*head)->next;
    }

    /* Hit the end of the list. DATA was not found.  */
    *cleanup = NULL;
}

               
void pc_cleanup_register(pc_pool_t *pool,
                         const void *data,
                         pc_cleanup_func_t cleanup)
{
    struct pc_cleanup_list_s *cl;
    pc_context_t *ctx = pool->memroot->ctx;

    /* We gotta have a cleanup pool once we start down that road...  */
    if (ctx->cleanup_pool == NULL)
        ctx->cleanup_pool = pc_pool_root(ctx);

#if 0
    if (pool->cleanup_hash == NULL)
    {
        pool->cleanup_hash = pc_hash_create(ctx->cleanup_pool);
    }
    else
    {
        /* If DATA is already registered, then just update CLEANUP.  */
        cl = pc_hash_get(pool->cleanup_hash, &data, sizeof(data));
        if (cl != NULL)
        {
            cl->cleanup = cleanup;
            return;
        }
    }
#endif

    /* Construct a new cleanup list record.  */
    cl = get_cl(ctx);
    cl->data = data;
    cl->cleanup = cleanup;

    /* Hook the new record into the pool.  */
    cl->next = pool->cleanups;
    pool->cleanups = cl;

#if 0
    pc_hash_set(pool->cleanup_hash, &data, sizeof(data));
#endif
}


void pc_cleanup_deregister(pc_pool_t *pool, const void *data)
{
    pc_cleanup_func_t unused;

    extract_cleanup(&unused, &pool->cleanups, pool->memroot->ctx, data);
}


void pc_cleanup_before(pc_pool_t *pool,
                       const void *before,
                       const void *after)
{
    struct pc_cleanup_list_s *cl_after = NULL;
    struct pc_cleanup_list_s *scan;

    /* There is a good way, and a bady way of arranging the cleanup
       ordering. Say we start with:

         HEAD -> A -> B -> C

       Where we cleanup starting at A, then B, then C.

       Now the app calls: before(C, B), then before(B, A)

       Bad way, we move the "before" item towards HEAD:

         before(C, B) results in HEAD -> A -> C -> B
         before(B, A) results in HEAD -> B -> A -> C

       Note that C is no longer before B.

       The correct answer is to push the "after" item away from HEAD:

         before(C, B) results in HEAD -> A -> C -> B
         before(B, A) results in HEAD -> C -> B -> A

       The correct ordering is maintained.
    */

    /* No cleanups? WTF are we being called?  */
    if (pool->cleanups == NULL)
        return;

    /* Fast path: if BEFORE is at HEAD, then we're already done.  */
    if (pool->cleanups->data == before)
        return;

    /* NOTE: it is likely that we can combine some of the special case
       with the loop by using an extra indirection on NEXT pointers
       (much like extract_cleanup() does), but that gets a bit harder
       to understand. For now, leaving this unrolled...  */

    /* Need special handling for AFTER at HEAD.  */
    if (pool->cleanups->data == after)
    {
        cl_after = pool->cleanups;

        /* Bail if this is the only record.  */
        if (cl_after->next == NULL)
            return;

        /* Unhook AFTER from HEAD.  */
        pool->cleanups = cl_after->next;

        /* Are we looking at BEFORE now? Put AFTER just after it.  */
        if (pool->cleanups->data == before)
        {
            cl_after->next = pool->cleanups->next;
            pool->cleanups->next = cl_after;
            return;
        }
    }

    /* Start scanning. If we find BEFORE first, then we're done. If we
       find AFTER, then detach it from the cleanup list. We'll re-insert
       it once we find BEFORE.  */
    for (scan = pool->cleanups;
         scan->next != NULL;
         scan = scan->next)
    {
        if (scan->next->data == before)
        {
            if (cl_after == NULL)
                return;  /* Found BEFORE first.  */

            /* Insert AFTER after BEFORE, and we're done.  */
            cl_after->next = scan->next->next;
            scan->next->next = cl_after;
            return;
        }

        if (scan->next->data == after)
        {
            /* Unhook AFTER from the list.  */
            cl_after = scan->next;
            scan->next = cl_after->next;
        }
    }

    /* Buh... we hit the end of the list without performing a re-ordering.
       If we extracted AFTER from the list, then put it back (at HEAD).

       ### this is likely to be bad. it pulls AFTER to the front of the
       ### list, possibly breaking other orderings against AFTER. should
       ### probably switch this to append (SCAN still points to the last
       ### cleanup record).

       ### we should probably register an unhandled error.  */
    if (cl_after != NULL)
    {
        cl_after->next = pool->cleanups;
        pool->cleanups = cl_after;
    }
}


void pc_cleanup_run(pc_pool_t *pool, const void *data)
{
    pc_cleanup_func_t cleanup;

    extract_cleanup(&cleanup, &pool->cleanups, pool->memroot->ctx, data);

    if (cleanup != NULL)
        cleanup((/* const */ void *) data);
}
