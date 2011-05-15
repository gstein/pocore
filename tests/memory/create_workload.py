#!/usr/bin/python
#
# create_workload.py :  create a C program to exercise APR and PoCore
#
# ====================================================================
#   Copyright 2010 Greg Stein
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
# ====================================================================
#

import sys
import random
import math

import ezt

TEMPLATE = 'create_workload.ezt'
MAX_ALLOC = 50000

# From analysis.py
DISTRIBUTION = [
  (2862, 50),
  (12443, 100),
  (6509, 250),
  (11, 1000),
  (28, 1500),
  (78, 2000),
  (72, 3000),
  (28, 4000),
  (18, 5000),
  (32, 10000),
  (178, 50000),
  (9, 270000),
  ]
TOTAL = sum([count for (count, amt) in DISTRIBUTION])


def alloc_one():
  pick = random.randrange(TOTAL)
  for count, amt in DISTRIBUTION:
    if pick < count:
      return random.randint(1, amt)
    pick -= count
  raise "oops"


def random_sizes(count):
  # use sqrt to weight the results lower
  return [alloc_one() for i in xrange(count)]


class item(object):
  def __init__(self, **kw):
    vars(self).update(kw)


def allocs(count, names):
  return [item(pool=random.choice(names), amt=alloc_one())
          for i in xrange(count)]


def prepare_data():
  random.seed(1)

  root_pools = ['rp_%d' % i for i in xrange(10)]
  root_pools.append('p_iter')

  pools = [item(var='p_0', parent='rp_0'),
              item(var='p_1', parent='rp_1'),
              item(var='p_2', parent='rp_2'),
              ]
  pool_names = [p.var for p in pools] + root_pools

  clear_pools = ['p_clear%d' % i for i in xrange(10)]
  for p in clear_pools:
    pools.append(item(var=p, parent=random.choice(pool_names)))

  all_pools = pool_names + clear_pools

  data = {
    'root_pools': root_pools,
    'pools': pools,
    'alloc': allocs(100000, all_pools),
    'clear': clear_pools,
    'alloc2': allocs(1000, clear_pools),
    'iter_parent': 'p_iter',
    'iter1': random_sizes(1000),
    'iter2': random_sizes(100),
    'iter3': random_sizes(10),
    }
  return data


def generate():
  template = ezt.Template(TEMPLATE, compress_whitespace=False)
  template.generate(sys.stdout, prepare_data())


if __name__ == '__main__':
  generate()
