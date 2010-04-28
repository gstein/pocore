#!/usr/bin/python
#
# analysis.py :  perform various analyses on allocation logs
#
# ====================================================================
#    Licensed to the Apache Software Foundation (ASF) under one
#    or more contributor license agreements.  See the NOTICE file
#    distributed with this work for additional information
#    regarding copyright ownership.  The ASF licenses this file
#    to you under the Apache License, Version 2.0 (the
#    "License"); you may not use this file except in compliance
#    with the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
# 
#    Unless required by applicable law or agreed to in writing,
#    software distributed under the License is distributed on an
#    "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#    KIND, either express or implied.  See the License for the
#    specific language governing permissions and limitations
#    under the License.
# ====================================================================
#

import sys

LOGFILE = 'svn-st-alloc.log'


def main(filename):
  data = load_data(filename)

  actions = {
    'create': 0,
    'alloc': 0,
    'clear': 0,
    'destroy': 0,
    }

  pools = { }
  history = [ ]
  max_depth = 0
  max_pools = 0
  max_amt = 0
  max_total = 0
  max_count = 0

  size_hist = [
    [50, 0],
    [100, 0],
    [250, 0],
    [1000, 0],
    [1500, 0],
    [2000, 0],
    [3000, 0],
    [4000, 0],
    [5000, 0],
    [10000, 0],
    [50000, 0],
    [200000, 0],
    ]

  depth_hist = [
    [5, 0],
    [10, 0],
    [15, 0],
    [20, 0],
    [30, 0],
    ]

  count_hist = [
    [5, 0],
    [10, 0],
    [100, 0],
    [1000, 0],
    [20000, 0],
    ]

  line = 0
  for action, pool, parent, amt in data:
    amt = int(amt)
    line += 1

    if amt > max_amt:
      max_amt = amt
    actions[action] += 1

    if action == 'create':
      if parent == '0x0':
        depth = 1
      else:
        depth = pools[parent][1] + 1
      pools[pool] = [parent, depth, 0, 0, 0]
      if depth > max_depth:
        max_depth = depth
      if len(pools) > max_pools:
        max_pools = len(pools)
    elif action == 'alloc':
      # lines 479047..050 are allocated AFTER 0x811418 is destroyed
      #assert pools.has_key(pool), 'line %d' % line
      if not pool in pools:
        continue
      info = pools[pool]
      info[2] += amt
      if info[2] > info[3]:
        info[3] = info[2]
        if info[3] > max_total:
          max_total = info[3]
      info[4] += 1
      if info[4] > max_count:
        max_count = info[4]
    elif action == 'clear':
      assert pools.has_key(pool), 'line %d' % line
      info = pools[pool]
      info[2] = 0
    elif action == 'destroy':
      info = pools[pool]
      history.append((info[3], info[4]))
      enter_histogram(size_hist, info[3])
      enter_histogram(depth_hist, info[1])
      enter_histogram(count_hist, info[4])
      del pools[pool]

  print 'Call counts:'
  for action in actions.keys():
    print '  %s: %d' % (action, actions[action])

  print 'Maximum depth:', max_depth
  print 'Maximum single allocation:', max_amt
  print 'Maximum allocs in a pool:', max_count
  print 'Maximum pool size:', max_total
  print 'Maximum live pools:', max_pools
  #print history

  print_histogram('Distribution of maximum pool size:', size_hist)
  print_histogram('Distribution of pool depth:', depth_hist)
  print_histogram('Distribution of counts', count_hist)


def load_data(filename):
  return [l.split() for l in open(filename).readlines()]


def enter_histogram(hist, value):
  for entry in hist:
    if value < entry[0]:
      entry[1] += 1
      return
  hist.append([value, 1])  # don't lose the value


def print_histogram(label, hist):
  print label
  last = 0
  for entry in hist:
    print '  [%d..%d]: %d' % (last, entry[0] - 1, entry[1])
    last = entry[0]


def write_test_program(filename):
  data = load_data(filename)

  varnames = { }
  for action, pool, parent, amt in data:
    varnames[pool] = None

  children = dict([(pool, [ ]) for pool in varnames.keys()])
  children['0x0'] = [ ]
  dead = varnames.copy()

  print HEADER
  for name in varnames.keys():
    print 'apr_pool_t *p%s;' % (name,)
  for action, pool, parent, amt in data:
    if action == 'create':
      if parent == '0x0':
        print 'apr_pool_create(&p%s, 0);' % (pool,)
      else:
        print 'apr_pool_create(&p%s, p%s);' % (pool, parent)
      children[parent].append(pool)
      del dead[pool]
    elif action == 'alloc':
      print 'apr_palloc(p%s, %s);' % (pool, amt)
    elif action == 'clear':
      print 'apr_pool_clear(p%s);' % (pool,)
      kill_children(pool, children, dead)
    elif action == 'destroy':
      if pool not in dead:
        print 'apr_pool_destroy(p%s);' % (pool,)
        kill_children(pool, children, dead)
        dead[pool] = None
  print FOOTER

def kill_children(pool, children, dead):
  for child in children[pool]:
    kill_children(child, children, dead)
    dead[child] = None
  children[pool] = [ ]

HEADER='''
/* build with: gcc -lapr-1 FILENAME.c  */
#include <stdio.h>
#include <mach/mach_time.h>
#include <apr-1/apr_pools.h>
int main(int argc, const char **argv)
{
  apr_initialize();
  uint64_t start = mach_absolute_time();
'''
FOOTER='''
  uint64_t end = mach_absolute_time();
  apr_terminate();
  mach_timebase_info_data_t info;
  mach_timebase_info(&info);
  uint64_t elapsed = (end - start) * info.numer / info.denom;
  printf("elapsed=%d.%03d usec\\n", (int)(elapsed/1000), (int)(elapsed%1000));
  return 0;
}
'''


if __name__ == '__main__':
  if len(sys.argv) >= 2 and sys.argv[1] == '--program':
    func = write_test_program
    del sys.argv[1]
  else:
    func = main
  if len(sys.argv) == 2:
    func(sys.argv[1])
  else:
    func(LOGFILE)
