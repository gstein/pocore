/*
  suite_hash.c :  test suite for the hash table implementation

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

#include <string.h>
#include <stdio.h>

#include "pc_types.h"
#include "pc_memory.h"

#include "ctest/ctest.h"


CTEST(hash, basic)
{
    pc_context_t *ctx = pc_context_create();
    pc_pool_t *pool = pc_pool_root(ctx);
    pc_hash_t *hash = pc_hash_create(pool);

    /* Two addresses, same value.  */
    char val1[] = "/A";
    char val2[] = "/A";

    /* Nothing in the hash table yet.  */
    ASSERT_EQUAL(pc_hash_count(hash), 0);
    ASSERT_NULL(pc_hash_gets(hash, val1));
    ASSERT_NULL(pc_hash_gets(hash, val2));

    /* Store a value and validate it.  */
    pc_hash_sets(hash, val1, "hi");
    ASSERT_STR(pc_hash_gets(hash, val1), "hi");
    ASSERT_STR(pc_hash_gets(hash, val2), "hi");

    /* Store through other address. Validate it.  */
    pc_hash_sets(hash, val2, "bye");
    ASSERT_STR(pc_hash_gets(hash, val1), "bye");
    ASSERT_STR(pc_hash_gets(hash, val2), "bye");
}
