/*
  alloc_method.c : perform some timing/testing of memory allocation methods

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

/* compile with: gcc alloc_method.c  */


#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <mach/mach_time.h>

#define REPS 100
#define COUNT 1000
#define ALLOC_SIZE 16384

static void *mem[COUNT];


static void
print_results(uint64_t start, uint64_t end)
{
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    uint64_t elapsed = (end - start) * info.numer / info.denom;
    printf("elapsed = %d.%03d msec\n",
           (int)(elapsed/1000000), (int)((elapsed/1000)%1000));
}


static void
exercise(void)
{
    int i;
    int j;

    /* Make sure all the pages are really mapped/page into memory.  */
    for (i = COUNT; i--; )
        for (j = 0; j < ALLOC_SIZE; j += 1000)
            ((char *)mem[i])[j] = 0;
}


static void
malloc_test(void)
{
    int j;
    uint64_t start = mach_absolute_time();

    for (j = REPS; j--; )
    {
        int i;

        for (i = COUNT; i--; )
            mem[i] = malloc(ALLOC_SIZE);
        for (i = COUNT; i--; )
            free(mem[i]);
    }
  
    uint64_t end = mach_absolute_time();
    print_results(start, end);
}


static void
mmap_test(void)
{
    int j;
    uint64_t start = mach_absolute_time();

    for (j = REPS; j--; )
    {
        int i;
        
        for (i = COUNT; i--; )
            mem[i] = mmap(0, ALLOC_SIZE, PROT_READ|PROT_WRITE, MAP_ANON,
                          -1, 0);
        for (i = COUNT; i--; )
            munmap(mem[i], ALLOC_SIZE);
    }
  
    uint64_t end = mach_absolute_time();
    print_results(start, end);
}


int main(int argc, const char **argv)
{
    if (argc != 2)
    {
      usage:
        fprintf(stderr, "USAGE: %s malloc|mmap\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "malloc") == 0)
        malloc_test();
    else if (strcmp(argv[1], "mmap") == 0)
        mmap_test();
    else
        goto usage;

    return EXIT_SUCCESS;
}
