/*
  pc_memory.h :  memory management primitives

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

#ifndef PC_MEMORY_H
#define PC_MEMORY_H

/* ### is this always the correct include?  */
#include <stdarg.h>  /* for va_list  */

#include "pc_types.h"
#include "pc_misc.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* Declare the basic types. Note that pc_pool_t is in pc_types.h  */

typedef struct pc_post_s pc_post_t;


/*
TEMP_POOL = pc_create_temp(pool);

POST = pc_create_post(pool);

pc_reset_to(POST);

pc_destroy(pool)


pc_clear_temps(pool)
pc_has_temp(pool)  // to verify children cleared temps

*/

pc_pool_t *pc_pool_root(pc_context_t *ctx);

pc_pool_t *pc_pool_create(pc_pool_t *parent);

void pc_pool_destroy(pc_pool_t *pool);

pc_post_t *pc_post_create(pc_pool_t *pool);

/* If POST is NULL, then the entire pool will be reset.

   Any tracked owners (established since the POST) will be cleaned up.  */
void pc_pool_reset_to(pc_post_t *post);

/* Begin tracking for this pool.  */
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
