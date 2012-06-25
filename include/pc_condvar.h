/*
  pc_condvar.h :  condition variables

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

#ifndef PC_CONDVAR_H
#define PC_CONDVAR_H

#include <stdint.h>

#include "pc_types.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct pc_condvar_s pc_condvar_t;
typedef struct pc_mutex_s pc_mutex_t;

void pc_condvar_create(pc_condvar_t *cv);

void pc_condvar_cleanup(pc_condvar_t *cv);

void pc_condvar_sleep(pc_condvar_t *cv, pc_mutex_t *mx);

void pc_condvar_signal(pc_condvar_t *cv);

void pc_condvar_broadcast(pc_condvar_t *cv);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PC_CONDVAR_H */
