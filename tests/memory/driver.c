/*
  driver.c :  drive the two workload exercises

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

/*
  $ gcc -c driver.c -I ../../include
  $ ./create_workload.py > workload.c
  $ gcc -c workload.c -I ../../include
  $ gcc ../../libpc-0.a driver.o workload.o -o workload -lapr-1 /path/to/libev.a
  $ ./workload
*/

#include <stdio.h>
#include <mach/mach_time.h>
#include <mach/task.h>

#include <apr-1/apr_pools.h>

#include "pc_misc.h"
#include "pc_memory.h"

#define COUNT 100


void exercise_apr(void);
void exercise_pocore(pc_context_t *ctx);


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
time_apr(void)
{
    apr_initialize();
    uint64_t start = mach_absolute_time();

    int i;
    for (i = COUNT; i--; )
        exercise_apr();

    uint64_t end = mach_absolute_time();
    apr_terminate();
    print_results(start, end);
}


static void
time_pocore(void)
{
    pc_context_t *ctx = pc_context_create();
    uint64_t start = mach_absolute_time();

    int i;
    for (i = COUNT; i--; )
        exercise_pocore(ctx);

    uint64_t end = mach_absolute_time();
    pc_context_destroy(ctx);
    print_results(start, end);
}


/* From: http://blog.kuriositaet.de/?p=257  */
static void
print_mem(void)
{
    task_t task = MACH_PORT_NULL;
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (task_info(mach_task_self(),
                  TASK_BASIC_INFO,
                  (task_info_t)&t_info,
                  &t_info_count) != KERN_SUCCESS)
        printf("memory usage: N/A\n");
    else
        printf("memory usage: rss = %ld   vs = %ld\n",
               (long)t_info.resident_size, (long)t_info.virtual_size);
}


int main(int argc, const char *argv[])
{
    if (argc != 2)
    {
      usage:
        printf("USAGE: %s  apr|pocore\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "apr") == 0)
        time_apr();
    else if (strcmp(argv[1], "pocore") == 0)
        time_pocore();
    else
        goto usage;

    print_mem();
    return EXIT_SUCCESS;
}
