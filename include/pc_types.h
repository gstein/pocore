/*
  pc_types.h :  basic types

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

#ifndef PC_TYPES_H
#define PC_TYPES_H

/* ### is this correct on all platforms nowadays?  */
#include <stdlib.h>  /* for size_t  */


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* Integer types when specific sizes are required.  */
/* ### we'll need configure magic to pick up the right types  */
typedef signed char pc_i8_t;
typedef unsigned char pc_u8_t;

typedef signed short pc_i16_t;
typedef unsigned short pc_u16_t;

typedef signed int pc_i32_t;
typedef unsigned int pc_u32_t;

typedef signed long long pc_i64_t;
typedef unsigned long long pc_u64_t;


/* The PoCore boolean type is a simple integer, used for self-documenting
   purposes.  */
typedef int pc_bool_t;
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif


/* Declare the pool type here, for use below.  */

typedef struct pc_pool_s pc_pool_t;

/* ### move all other incomplete-struct typedefs here?  */


/* PoCore's hash type and supporting functions.  */
/* ### should these move into their own header?  */

typedef struct pc_hash_s pc_hash_t;
typedef struct pc_hiter_s pc_hiter_t;

pc_hash_t *pc_hash_create(pc_pool_t *pool);

pc_hash_t *pc_hash_copy(const pc_hash_t *hash, pc_pool_t *pool);

void pc_hash_set(pc_hash_t *hash, const void *key, size_t klen, void *value);

void pc_hash_sets(pc_hash_t *hash, const char *key, void *value);

void *pc_hash_get(const pc_hash_t *hash, const void *key, size_t klen);

void *pc_hash_gets(const pc_hash_t *hash, const char *key);

void pc_hash_clear(pc_hash_t *hash);

int pc_hash_count(const pc_hash_t *hash);

pc_hiter_t *pc_hash_first(const pc_hash_t *hash, pc_pool_t *pool);

void pc_hiter_next(pc_hiter_t *hiter);

const void *pc_hiter_key(const pc_hiter_t *hiter);
size_t pc_hiter_klen(const pc_hiter_t *hiter);
void *pc_hiter_value(const pc_hiter_t *hiter);


/* PoCore's array type and supporting functions.  */

typedef struct {
    void *elems;

    size_t elem_size;

    int count;

    int alloc;

    pc_pool_t *pool;

} pc_array_t;

#define PC_ARRAY_IDX(ary, i, type) (((type *)(ary)->elems)[i])

#define PC_ARRAY_PUSH(ary, type) (*((type *)pc_array_add(ary)))

pc_array_t *pc_array_create(int alloc, size_t elem_size, pc_pool_t *pool);

pc_array_t *pc_array_copy(const pc_array_t *ary, pc_pool_t *pool);

void *pc_array_add(pc_array_t *ary);

#define PC_ARRAY_CLEAR(ary) ((ary)->count = 0)

#define PC_ARRAY_COUNT(ary) ((ary)->count)


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PC_TYPES_H */
