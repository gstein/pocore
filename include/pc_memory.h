/*
  pc_memory.h :  memory management primitives

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

#ifndef PC_MEMORY_H
#define PC_MEMORY_H

/* ### is this always the correct include?  */
#include <stdarg.h>  /* for va_list  */
#include <string.h>  /* for memset()  */

#include "pc_types.h"
#include "pc_misc.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* Declare the basic types. Note that pc_pool_t is in pc_types.h  */

typedef struct pc_post_s pc_post_t;


pc_pool_t *pc_pool_root(pc_context_t *ctx);

pc_pool_t *pc_pool_create(pc_pool_t *parent);

void pc_pool_destroy(pc_pool_t *pool);

pc_post_t *pc_post_create(pc_pool_t *pool);

pc_post_t *pc_post_create_coalescing(pc_pool_t *pool);

/* Reset the POST's associated pool back to the state when POST was set.
   All memory will be returned to the pool. Later posts are also reset,
   then forgotten.

   Any tracked owners (established since the POST) will be cleaned up.  */
void pc_post_recall(pc_post_t *post);

/* Return MEM/LEN to the POST it was allocated from.  */
void pc_post_freemem(pc_post_t *post, void *mem, size_t len);


void pc_pool_clear(pc_pool_t *pool);

/* Begin tracking for this pool. Typically, applications will use
   pc_track_owns_pool() or functions like pc_type_owns() instead of
   this function. If a pool is cleaned up by the tracking system, that
   is equivalent to calling pc_pool_destroy() on it, which will also
   cause all the pool's owners to be cleaned up.  */
void pc_pool_track(pc_pool_t *pool);


void *pc_alloc(pc_pool_t *pool, size_t amt);

#define pc_calloc(pool, amt) memset(pc_alloc(pool, amt), 0, amt)

char *pc_strdup(pc_pool_t *pool, const char *str);

char *pc_strmemdup(pc_pool_t *pool, const char *str, size_t len);

char *pc_strndup(pc_pool_t *pool, const char *str, size_t amt);

void *pc_memdup(pc_pool_t *pool, void *mem, size_t len);

char *pc_strcat(pc_pool_t *pool, ...);

char *pc_vsprintf(pc_pool_t *pool, const char *fmt, va_list ap);

char *pc_sprintf(pc_pool_t *pool, const char *fmt, ...)
        __attribute__((format(printf, 2, 3)));


/* ### shared memory functions  */
/* ### should pc_mmap.h go in here, too?  */

#if 0
func(pool)
{
    scratch = pc_create_temp(pool);

    ob1 = pc_alloc(scratch);
    ob2 = pc_alloc(scratch);

    scratch_post = pc_create_post(scratch);
    for (...)
    {
        /* ### toss temps created after POST.  */
        pc_reset_to(scratch_post);

        func1(scratch);
        ob = func2(scratch);
        func3(pool);
    }

    pc_destroy(scratch);
}
#endif


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PC_MEMORY_H */
