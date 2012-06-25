/*
  condvar.c :  condition variable primitives

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

#include "pc_condvar.h"

#include "pocore.h"

#ifdef PC__IS_WINDOWS

void pc_condvar_create(pc_condvar_t *cv)
{
    InitializeConditionVariable(&cv->cv);
}

void pc_condvar_cleanup(pc_condvar_t *cv)
{
    /* XXX: Verify there really isn't a cleanup function on windows */
}

void pc_condvar_sleep(pc_condvar_t *cv, pc_mutex_t *mx)
{
    SleepConditionVariableCS(&cv->cv, &mx->cs, INFINITE);
}

void pc_condvar_signal(pc_condvar_t *cv)
{
    WakeConditionVariable(&cv->cv);
}

void pc_condvar_broadcast(pc_condvar_t *cv)
{
    WakeAllConditionVariable(&cv->cv);
}

#else

void pc_condvar_create(pc_condvar_t *cv)
{
    /* ### todo */
}

void pc_condvar_cleanup(pc_condvar_t *cv)
{
    /* ### todo */
}

void pc_condvar_sleep(pc_condvar_t *cv, pc_mutex_t *mx)
{
    /* ### todo */
}

void pc_condvar_signal(pc_condvar_t *cv)
{
    /* ### todo */
}

void pc_condvar_broadcast(pc_condvar_t *cv)
{
    /* ### todo */
}

#endif
