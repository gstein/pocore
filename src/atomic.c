/*
  atomic.c :  implement of PoCore's atomic functions

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

#include "pc_mutex.h"

#include "pocore.h"
#include "pocore_platform.h"

#ifdef PC__IS_MACOSX
#include <libkern/OSAtomic.h>
#endif
#ifdef PC__IS_WINDOWS
#include <Windows.h>
#endif

/* ### see http://trac.mcs.anl.gov/projects/openpa/   */


int32_t pc_atomic_inc(volatile int32_t *mem)
{
#if defined(PC__IS_MACOSX)
    return OSAtomicIncrement32Barrier(mem);
#elif defined(PC__IS_WINDOWS)
    return InterlockedIncrement(mem);
#else
    int32_t value;

    /* Spin until the increment succeeds.  */
    do
    {
        value = *mem;
    } while (!pc_atomic_swap(mem, value, value + 1));

    return value + 1;
#endif
}


int32_t pc_atomic_dec(volatile int32_t *mem)
{
#if defined(PC__IS_MACOSX)
    return OSAtomicDecrement32Barrier(mem);
#elif defined(PC__IS_WINDOWS)
    return InterlockedDecrement(mem);
#else
    int32_t value;

    /* Spin until the decrement succeeds.  */
    do
    {
        value = *mem;
    } while (!pc_atomic_swap(mem, value, value - 1));

    return value - 1;
#endif
}


pc_bool_t pc_atomic_swap(volatile int32_t *mem,
                         int32_t check_val,
                         int32_t new_val)
{
#if defined(PC__IS_MACOSX)
    return OSAtomicCompareAndSwap32Barrier(check_val, new_val, mem);
#elif defined(PC__IS_WINDOWS)
    return InterlockedCompareExchange(mem, new_val, check_val);
#else
#error Atomics are not supported on this platform
#endif
}


pc_bool_t pc_atomic_swapptr(void * volatile *mem,
                            void *check_ptr,
                            void *new_ptr)
{
#if defined(PC__IS_MACOSX)
    return OSAtomicCompareAndSwapPtrBarrier(check_ptr, new_ptr, mem);
#elif defined(PC__IS_WINDOWS)
    return InterlockedCompareExchangePointer(mem, new_ptr, check_ptr);
#else
#error Atomics are not supported on this platform
#endif
}


pc_error_t *pc_atomic_once(volatile int32_t *control,
                           pc_context_t *ctx,
                           pc_error_t *(*once_func)(void *once_baton),
                           void *once_baton)
{
    enum {
        ctrl_uninitialized = 0,
        ctrl_failed,
        ctrl_success
    };
    pc_error_t *err;

    /* A couple quick exits. There are race conditions here, but since
       these are terminal values, we can fast-path them.  */
    if (*control == ctrl_success)
        return NULL;
    if (*control == ctrl_failed)
        return /* ### some indicator */ NULL;

    /* We're going to need the mutex, so lazy-init the thing.  */
    pc__context_init_mutex(ctx);

    pc_mutex_lock(ctx->general_mutex);

    /* Are we the winning thread, to call the function?  */
    if (*control == ctrl_uninitialized)
    {
        err = (*once_func)(once_baton);
        *control = (err ? ctrl_failed : ctrl_success);
    }
    else if (*control == ctrl_success)
    {
        err = NULL;
    }
    else
    {
        err = /* ### indicator */ NULL;
    }

    pc_mutex_unlock(ctx->general_mutex);
    return err;
}
