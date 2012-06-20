/*
  pc_cleanup.h :  data cleanup functionality

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

#ifndef PC_CLEANUP_H
#define PC_CLEANUP_H

#include "pc_types.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* NOTE: data items use the "const void *" type to denote that the
   cleanup system will not attempt to modify the data items in any way.
   However, it is likely the cleanup function may need to change the
   data, so the const is cast away when the cleanup function is called.  */


/* A cleanup function for a piece of data.  */
typedef void (*pc_cleanup_func_t)(void *data);

/* A function to shift a piece of data to a different context.
   DATA is typically a resource.  OLD_CTX is the context that the
   pool this cleanup was registered against was in previously.  */
typedef void (*pc_shift_func_t)(void *data, pc_context_t *old_ctx);


/* Register DATA in the cleanup registry of POOL. When this item needs to
   be cleaned up, CLEANUP will be invoked with DATA as its sole parameter.

   It is okay to register an item multiple times. Only the last CLEANUP will
   be saved, and run at cleanup time.

   CLEANUP may not be NULL.  */
void pc_cleanup_register(pc_pool_t *pool,
                         const void *data,
                         pc_cleanup_func_t cleanup,
                         pc_shift_func_t shift);


/* Remove DATA from POOL's cleanup registry.

   The cleanup will NOT be run.

   Warning: The data item wil be deregistered regardless of any data
   item that may need to be cleaned "before" this item.

   It is acceptable to deregister an item has not been registered.  */
void pc_cleanup_deregister(pc_pool_t *pool, const void *data);


/* Ensure that BEFORE is cleaned up before AFTER, within the cleanup
   registry within POOL.  */
void pc_cleanup_before(pc_pool_t *pool,
                       const void *before,
                       const void *after);


/* Run the cleanup for this particular data item, then de-register it.
   This item should not have any owners. This operation is a no-op if the
   data item is not registered for cleanup.  */
void pc_cleanup_run(pc_pool_t *pool, const void *data);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PC_CLEANUP_H */
