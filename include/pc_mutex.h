/*
  pc_mutex.h :  locking primitives (eg. mutex, spinlock, semaphore)

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

#ifndef PC_MUTEX_H
#define PC_MUTEX_H

#include <stdint.h>

#include "pc_types.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* For information on PoCore's atomic primitives, see:
     http://code.google.com/p/pocore/wiki/AtomicPrimitives
*/

/* Increment *MEM by 1, and return its NEW value.  */
int32_t pc_atomic_inc(int32_t *mem);


/* Decrement *MEM by 1, and return its NEW value.  */
int32_t pc_atomic_dec(int32_t *mem);


/* If *MEM equals CHECK_VAL, then replace it with NEW_VAL. If this replacement
   occurs, then return TRUE. Otherwise, return FALSE.  */
pc_bool_t pc_atomic_swap(int32_t *mem, int32_t check_val, int32_t new_val);


/* ### mutex. spinlock. semaphore. condition. atomic.
   ### anything else?
*/


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PC_MUTEX_H */
