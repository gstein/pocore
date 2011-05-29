/*
  test_lookup.c :  basic test of address lookup

  ====================================================================
    Copyright 2011 Greg Stein

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
#include "pc_misc.h"
#include "pc_memory.h"
#include "pc_channel.h"
#include "pc_error.h"


int main(int argc, const char **argv)
{
    pc_context_t *ctx = pc_context_create();
    pc_pool_t *pool = pc_pool_root(ctx);
    pc_error_t *err;
    pc_hash_t *addresses;
    pc_hiter_t *hi;

    err = pc_address_lookup(&addresses, argv[1], atoi(argv[2]), 0, pool);
    if (err)
    {
        printf("error: [%d] %s\n", pc_error_code(err), pc_error_message(err));
        return EXIT_FAILURE;
    }

    for (hi = pc_hiter_begin(addresses, pool);
         hi != NULL;
         hi = pc_hiter_next(hi))
    {
        printf("result: %s\n", pc_hiter_key(hi));
    }

    return EXIT_SUCCESS;
}
