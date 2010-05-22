#!/usr/bin/python
#
# twins.py :  find twin primes, for use by pc_hash_t
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

GROWTH = 1.5
START = 11
MAX = 1L << 31

def main():
  current = START
  while True:
    print current

    current = long(current * GROWTH)
    if current > MAX:
      break
    if (current & 1) == 0:
      current += 1

    while True:
      if isprime(current) and isprime(current + 2):
        break
      current += 2


def isprime(n):
  sqrt = n ** 0.5
  for prime in PRIMES:
    if prime > sqrt:
      return True
    if n % prime == 0:
      return False


def load_primes():
  global PRIMES
  PRIMES = [int(p) for p in open('primes.txt').read().split()]


if __name__ == '__main__':
  load_primes()
  main()
