/*
  misc.c :  miscellaneous portability support

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

#include "pocore.h"


pc_context_t *pc_context_create(void)
{
    return pc_context_create_custom(0, NULL);
}


pc_context_t *pc_context_create_custom(size_t stdsize,
                                       int (*oom_handler)(size_t amt))
{
    pc_context_t *ctx = malloc(sizeof(*ctx));

    if (stdsize == PC_DEFAULT_STDSIZE)
        stdsize = PC_MEMBLOCK_SIZE;
    else if (stdsize < PC_MEMBLOCK_MINIMUM)
        stdsize = PC_MEMBLOCK_MINIMUM;

    memset(ctx, 0, sizeof(*ctx));

    ctx->oom_handler = oom_handler;
    ctx->stdsize = stdsize;

    return ctx;
}


void pc_context_destroy(pc_context_t *ctx)
{
    struct pc_block_s *scan = ctx->std_blocks;

    /* ### do something about unhandled errors?  */

    if (ctx->track_pool != NULL)
        pc_pool_destroy(ctx->track_pool);
    if (ctx->error_pool != NULL)
        pc_pool_destroy(ctx->error_pool);

    for (scan = ctx->std_blocks; scan != NULL; )
    {
        struct pc_block_s *next = scan->next;

        free(scan);
        scan = next;
    }

    while (ctx->nonstd_blocks != NULL)
    {
        /* Keep fetching the smallest node possible until it runs out.  */
        scan = pc__memtree_fetch(&ctx->nonstd_blocks,
                                 sizeof(struct pc_memtree_s));
        free(scan);
    }

    free(ctx);
}
