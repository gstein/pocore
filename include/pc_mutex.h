/*
  pc_mutex.h :  locking primitives (eg. mutex, spinlock, semaphore)

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

#ifndef PC_MUTEX_H
#define PC_MUTEX_H

#include <stdint.h>

#include "pc_types.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct pc_mutex_s pc_mutex_t;


/* For information on PoCore's atomic primitives, see:
     http://code.google.com/p/pocore/wiki/AtomicPrimitives
*/

/* ### how to describe alignment restrictions on these pointers?  */

/* Increment *MEM by 1, and return its NEW value.  */
int32_t pc_atomic_inc(volatile int32_t *mem);


/* Decrement *MEM by 1, and return its NEW value.  */
int32_t pc_atomic_dec(volatile int32_t *mem);


/* If *MEM equals CHECK_VAL, then replace it with NEW_VAL. If this replacement
   occurs, then return TRUE. Otherwise, return FALSE.  */
pc_bool_t pc_atomic_swap(volatile int32_t *mem,
                         int32_t check_val,
                         int32_t new_val);


/* If *MEM equals CHECK_PTR, then replace it with NEW_PTR. If this replacement
   occurs, then return TRUE. Otherwise, return FALSE.  */
pc_bool_t pc_atomic_swapptr(void * volatile *mem,
                            void *check_ptr,
                            void *new_ptr);


/* Ensure that ONCE_FUNC is called a single time, passing ONCE_BATON.
   CONTROL is used to determine whether the function has been called,
   and whether it has (yet) returned and whether it raised an error.
   CTX is used to mutex the initialization process, in case ONCE_FUNC
   is a time-consuming initialization. All threads should specify the
   same context (ideally, the initialization would occur before the
   application spins up threads).

   CONTROL is typically defined as follows:

     static volatile int32_t control = 0;

   ### need to define some error returns.  */
pc_error_t *pc_atomic_once(volatile int32_t *control,
                           pc_context_t *ctx,
                           pc_error_t *(*once_func)(void *once_baton),
                           void *once_baton);


/* ### return values?  */
void pc_mutex_lock(pc_mutex_t *mutex);
void pc_mutex_unlock(pc_mutex_t *mutex);

/* ### mutex. spinlock. semaphore. condition. atomic.
   ### anything else?
*/


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PC_MUTEX_H */
