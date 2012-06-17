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
#include <ctype.h>

#include "pc_types.h"
#include "pc_misc.h"
#include "pc_memory.h"

#include "pocore.h"


pc_context_t *pc_context_create(void)
{
    return pc_context_create_custom(PC_DEFAULT_STDSIZE, NULL, TRUE);
}


pc_context_t *pc_context_create_custom(size_t stdsize,
                                       int (*oom_handler)(size_t amt),
                                       pc_bool_t track_unhandled)
{
    pc_context_t *ctx;

#ifdef PC__IS_WINDOWS
    {
        HANDLE heap;

        heap = HeapCreate(HEAP_NO_SERIALIZE,
                          0 /* dwInitialSize */,
                          0 /* dwMaximumSize */);
        ctx = HeapAlloc(heap, HEAP_ZERO_MEMORY, sizeof(*ctx));
        ctx->heap = heap;
    }
#else
    ctx = malloc(sizeof(*ctx));
    memset(ctx, 0, sizeof(*ctx));
#endif

    if (stdsize == PC_DEFAULT_STDSIZE)
        stdsize = PC_MEMBLOCK_SIZE;
    else if (stdsize < PC_MEMBLOCK_MINIMUM)
        stdsize = PC_MEMBLOCK_MINIMUM;

    ctx->oom_handler = oom_handler;
    ctx->stdsize = stdsize;
    ctx->track_unhandled = track_unhandled;

    return ctx;
}


void pc_context_destroy(pc_context_t *ctx)
{
    /* ### do something about unhandled errors?  */

    if (ctx->cctx != NULL)
        pc__channel_cleanup(ctx);

    /* Blast all memroots. This will destroy CLEANUP_POOL, ERROR_POOL,
       and anything the channel subsystem may have allocated.

       Note: we destroy the head of the list, which is faster for
       pc_pool_destroy() to pop the memroot from the list.

       ### right now, pc__channel_cleanup() preemptively destroys its
       ### private pool. that may continue to make sense if, say, we
       ### provide a way to shut down the channel system, so it should
       ### continue to manage its internal pool.  */
    while (ctx->memroots != NULL)
        pc_pool_destroy(ctx->memroots->pool);

    /* ### it would probably be best to just dig into the tree structure
       ### rather than pop one out at a time, with the associated tree
       ### rebalancing. ie. switch from O(N log N) to just O(N).  */
    while (ctx->nonstd_blocks != NULL)
    {
        struct pc_block_s *scan;

        /* Keep fetching the smallest node possible until it runs out.  */
        scan = pc__memtree_fetch(&ctx->nonstd_blocks,
                                 sizeof(struct pc_memtree_s));
        PC__FREE(ctx, scan);
    }

#ifdef PC__IS_WINDOWS
    HeapDestroy(ctx->heap);
#else
    free(ctx);
#endif
}


void pc_context_tracing(pc_context_t *ctx, pc_bool_t tracing)
{
    ctx->tracing = tracing;
}


pc_error_t *pc_context_unhandled(pc_context_t *ctx)
{
    if (ctx->unhandled == NULL)
        return NULL;
    return &ctx->unhandled->error;
}


void pc__context_init_mutex(pc_context_t *ctx)
{
    /* ### todo. make sure to use swapptr to avoid rewriting non-NULL.  */
}


void pc_lib_version(
    int *major,
    int *minor,
    int *patch)
{
    *major = PC_MAJOR_VERSION;
    *minor = PC_MINOR_VERSION;
    *patch = PC_PATCH_VERSION;
}


void pc_uuid_create(pc_uuid_t *uuid_out)
{
#if defined(PC__IS_WINDOWS)
    pc__windows_uuid_create(uuid_out);
#elif defined(PC__IS_BSD)
    pc__bsd_uuid_create(uuid_out);
#elif defined(PC__USE_UUID_GENERATE)
    {
        uuid_t out;

        uuid_generate(out);
        memcpy(&uuid_out->bytes[0], &out, 16);
    }
#else
#error uuid support was not discovered
#endif
}


void pc_uuid_format(char *human_out, const pc_uuid_t *uuid)
{
    const uint8_t *b = &uuid->bytes[0];

    sprintf(human_out,
            "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X"
            "-%02X%02X%02X%02X%02X%02X",
            b[0], b[1], b[ 2], b[ 3], b[ 4], b[ 5], b[ 6], b[ 7],
            b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
}


/* C1 and C2 have been validated as hex digits.  */
static uint8_t parse_byte(char c1, char c2)
{
    uint8_t v;

    if (c1 >= 'a')
        v = (c1 - 'a' + 10) << 4;
    else if (c1 >= 'A')
        v = (c1 - 'A' + 10) << 4;
    else
        v = (c1 - '0') << 4;

    if (c2 >= 'a')
        return v | (c2 - 'a' + 10);
    if (c2 >= 'A')
        return v | (c2 - 'A' + 10);
    return v | (c2 - '0');
}


pc_bool_t pc_uuid_parse(pc_uuid_t *uuid_out, const char *human)
{
    int i;
    uint8_t *b;

    for (i = 0; i < 36; ++i)
    {
	char c = human[i];

        /* If these four locations do not have '-', or the other locations
           are not hex digits, then we have an incorrect format.  */
        if (i == 8 || i == 13 || i == 18 || i == 23)
        {
            if (c != '-')
                return TRUE;
        }
        else if (!isxdigit(c))
            return TRUE;
    }
    if (human[36] != '\0')
        return TRUE;

    b = uuid_out->bytes;

    b[ 0] = parse_byte(human[ 0], human[ 1]);
    b[ 1] = parse_byte(human[ 2], human[ 3]);
    b[ 2] = parse_byte(human[ 4], human[ 5]);
    b[ 3] = parse_byte(human[ 6], human[ 7]);

    /* - */

    b[ 4] = parse_byte(human[ 9], human[10]);
    b[ 5] = parse_byte(human[11], human[12]);

    /* - */

    b[ 6] = parse_byte(human[14], human[15]);
    b[ 7] = parse_byte(human[16], human[17]);

    /* - */

    b[ 8] = parse_byte(human[19], human[20]);
    b[ 9] = parse_byte(human[21], human[22]);

    /* - */

    for (i = 6; i--; )
        b[i + 10] = parse_byte(human[24 + i*2], human[25 + i*2]);

    return FALSE;
}
