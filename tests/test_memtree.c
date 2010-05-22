/*
  test_memtree.c :  test the memtree (a red-black tree) subsystem

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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <mach/mach_time.h>

#include "../src/pocore.h"


/* How many nodes do we want to test/work with?  */
#define COUNT 10000

/* What is the maximum size of the block to simulate? Keeping this small
   relative to COUNT will generate duplicate sizes.  */
#define MAX_SIZE 1000

/* The random number generator seed; useful for replicating a test run.  */
#define SEED 1

/* The minimum size remnant that we intend to insert into one of these
   trees. Non-standard-sized blocks will also be stored, but those are
   huge. Only the remnants may be "too small".  */
#define MIN_REMNANT sizeof(struct pc_memtree_s)

/* This is our "free memory blocks". We don't really need them allocated on
   the heap, nor does the memory need to correspond to mem[].b.size.  */
static struct pc_memtree_s mem[COUNT];

/* Remember the sizes that we assigned to each "block" of memory.  */
static size_t sizes[COUNT];

/* Various control flags over this testing.  */
#define CONTINUOUS_CHECK
#define PRINT_TIMING



int
main(int argc, const char **argv)
{
    struct pc_memtree_s *root = NULL;
    int i;
#ifdef PRINT_TIMING
    uint64_t start;
    uint64_t end;
#endif

    /* Initialize all the sizes. We can ignore all the other fields in
       the pc_memtree_s records. Only the red-black tree needs that; we're
       using the structure simply to guarantee enough space for the nodes
       when they're placed into the tree.  */
    srandom(SEED);
    for (i = COUNT; i--; )
    {
        sizes[i] = MIN_REMNANT + ((random() % MAX_SIZE) & ~3);
        mem[i].b.size = sizes[i];
    }

#ifdef PRINT_TIMING
    start = mach_absolute_time();
#endif

    /* Simple test. Put all of them in, then pull them all out.

       Do this four ways: insert forwards/reverse. Fetch forwards/reverse.

       NOTE: we expect to get back a block of *exactly* the requested
       size. The algorithm is supposed to find Best Fit, and we know each
       requested size will be in the tree (each fetch matches an inser).  */

    for (i = 0; i < COUNT; ++i)
    {
        pc__memtree_insert(&root, &mem[i], sizes[i]);

#ifdef CONTINUOUS_CHECK
#ifndef PRINT_TIMING
        /* Validate after each insertion. This loop becomes O(N * N)  */
        (void) pc__memtree_depth(root);
#endif
#endif
    }
#ifndef PRINT_TIMING
    printf("depth = %d\n", pc__memtree_depth(root));
#endif
    for (i = 0; i < COUNT; ++i)
    {
        struct pc_block_s *block = pc__memtree_fetch(&root, sizes[i]);
        assert(block->size == sizes[i]);
#ifdef CONTINOUS_CHECK
#ifndef PRINT_TIMING
        /* Validate after each removal. This loop becomes O(N * N)  */
        (void) pc__memtree_depth(root);
#endif
#endif
    }
    assert(root == NULL);  /* the tree should be empty  */

    for (i = COUNT; i--; )
        pc__memtree_insert(&root, &mem[i], sizes[i]);
#ifndef PRINT_TIMING
    printf("depth = %d\n", pc__memtree_depth(root));
#endif
    for (i = 0; i < COUNT; ++i)
    {
        struct pc_block_s *block = pc__memtree_fetch(&root, sizes[i]);
        assert(block->size == sizes[i]);
    }
    assert(root == NULL);  /* the tree should be empty  */

    for (i = 0; i < COUNT; ++i)
        pc__memtree_insert(&root, &mem[i], sizes[i]);
#ifndef PRINT_TIMING
    printf("depth = %d\n", pc__memtree_depth(root));
#endif
    for (i = COUNT; i--; )
    {
        struct pc_block_s *block = pc__memtree_fetch(&root, sizes[i]);
        assert(block->size == sizes[i]);
    }
    assert(root == NULL);  /* the tree should be empty  */

    for (i = COUNT; i--; )
        pc__memtree_insert(&root, &mem[i], sizes[i]);
#ifndef PRINT_TIMING
    printf("depth = %d\n", pc__memtree_depth(root));
#endif
    for (i = COUNT; i--; )
    {
        struct pc_block_s *block = pc__memtree_fetch(&root, sizes[i]);
        assert(block->size == sizes[i]);
    }
    assert(root == NULL);  /* the tree should be empty  */

#ifdef PRINT_TIMING
    end = mach_absolute_time();
    {
        mach_timebase_info_data_t info;
        mach_timebase_info(&info);
        uint64_t elapsed = (end - start) * info.numer / info.denom;
        printf("elapsed = %d.%03d msec\n",
               (int)(elapsed/1000000), (int)((elapsed/1000)%1000));
        printf("op = %d.%03d usec\n",
               (int)(elapsed/(COUNT*8))/1000,
               (int)(elapsed/(COUNT*8))%1000);
    }
#endif PRINT_TIMING

    return EXIT_SUCCESS;
}
