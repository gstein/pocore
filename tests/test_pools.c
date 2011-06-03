/*
  test_pools.c :  basic test of the pool system

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

#include "pc_types.h"
#include "pc_misc.h"
#include "pc_memory.h"


int main(int argc, const char **argv)
{
    pc_context_t *ctx = pc_context_create();
    pc_pool_t *pool = pc_pool_root(ctx);
    pc_pool_t *subpool = pc_pool_create(pool);

    void *mem = pc_alloc(pool, 10);
    memset(mem, 0, 10);

    mem = pc_alloc(pool, 10000);
    memset(mem, 0, 10000);

    mem = pc_alloc(subpool, 10);
    memset(mem, 0, 10);

    pc_pool_clear(pool);

    return EXIT_SUCCESS;
}
