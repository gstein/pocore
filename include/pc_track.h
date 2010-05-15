/*
  pc_track.h :  tracking lifespan/item dependencies

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

#ifndef PC_TRACK_H
#define PC_TRACK_H

#include "pc_types.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* NOTE: tracked pointers use the "const void *" type to denote that the
   tracking system will not attempt to modify tracked items in any way.
   However, it is likely the cleanup function may need to change the
   contents of the tracked item, so the const is cast away when the cleanup
   function is called.  */


/* A cleanup function for a tracked item.  */
typedef void (*pc_cleanup_func_t)(void *tracked);


/* Register TRACKED in the dependency tracking registry as part of CTX.
   When this item needs to be cleaned up, CLEANUP will be invoked with
   TRACKED as its sole parameter.

   It is okay to register an item multiple times. Only the last CLEANUP will
   be saved, and run at cleanup time.

   CLEANUP may not be NULL.  */
void pc_track(pc_context_t *ctx,
              const void *tracked, pc_cleanup_func_t cleanup);


/* Register TRACKED in the tracking registry of the context that POOL is
   part of.  */
void pc_track_via(pc_pool_t *pool,
                  const void *tracked, pc_cleanup_func_t cleanup);


/* Remove TRACKED from the tracking registry. It should not have any owners,
   and it will be removed from all dependents' list of owners.

   The cleanup will NOT be run.

   It is acceptable to deregister an item that is not being tracked.  */
void pc_track_deregister(pc_context_t *ctx, const void *tracked);


/* Both OWNER and DEPENDENT must be registered within the tracking registry
   of context CTX.  */
void pc_track_dependent(pc_context_t *ctx,
                        const void *owner, const void *dependent);


/* Run the cleanup for this particular tracked item, then de-register it.
   This item should not have any owners. This operation is a no-op if the
   item is not registered for tracking.  */
void pc_track_cleanup(pc_context_t *ctx, const void *tracked);


/* ### no... only pools should do this.  */
void pc_track_cleanup_owners(const void *tracked);


/*
  cleanup process:

  pc_track_cleanup(T):
    assert T->cleanup_func != NULL
    assert T->owner == NULL

    // for debugging, app may be tracking dependencies, but does not have
    // a cleanup_func for this item. ### tough. use an empty func.
    func = T->cleanup_func:
    // prevent double-run
    T->cleanup_func = NULL
    func(T->cleanup_baton)

    // ### no. do not clean dependents. other things may depend upon
    // ### them. we may have depended upon E, and so did D. so our
    // ### child is D, depending upon E. we have no right to clean D.
    for each dependent D:
      D.owner = NULL

  thus, all dependents are available at cleanup time

  T.owns(D1)
  D1.owner(T)
  D1.owner = T

  written: D1->T

  D1->T
  D2->T
  D2->D1

  D2 now has multiple owners, and implied ordering of cleanup.
  must cleanup: T, D1, D2.   NOT: T, D2, D1

  Multiple sequences of registration:

      (a)     (b)     (c)
  1. D1->T.  D2->T.  D2->D1.
  2. D1->T.  D2->D1. D2->T.
  3. D2->T.  D1->T.  D2->D1.
  4. D2->T.  D2->D1. D1->T.
  5. D2->D1. D1->T.  D2->T.
  6. D2->D1. D2->T.  D1->T.

  desired result: T.children=[D1,D2]. D1.children=[D2]

  errors:

  7. T->D1. D1->T.
  8. T->D1. D1->D2. D2->T.

  add_dependent(owner, dep):
    ### if debug, check for loop
    owner.dependents.append(dep)
    dep.owners.append(owner)
   
*/



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PC_TRACK_H */
