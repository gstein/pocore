PoCore provides a few primitives for atomic operations. These are useful within (potentially) multi-threaded environments. Most applications will only be interested in the simplest primitives (increment and decrement), and using PoCore's higher level MutexPrimitives for all other situations.

The design goal is to provide the **useful** primitives, and the **sufficient** primitives for building further [wait-free structures](http://en.wikipedia.org/wiki/Lock-free_and_wait-free_algorithms).

In particular, PoCore will use [Compare-and-Swap](http://en.wikipedia.org/wiki/Compare-and-swap) as its foundation, and provide further functionality from there.

# API #

I believe we want just four functions:

  1. increment
  1. decrement
  1. init-once
  1. compare-and-swap

The first three are the "useful" operations, while compare-and-swap is our building block for all atomic operations.

_actually, the init-once functionality will soon be reclassified under MutexPrimitives._

# Implementation #

Where an operating system provides these primitives, PoCore can simply defer to those functions (e.g. `OSAtomic*` on Mac OS, and `Interlocked*` on Windows).

For other implementations, PoCore will rely upon the [OpenPA library](http://trac.mcs.anl.gov/projects/openpa/).

_Note: need to look at this more, but using OpenPA will probably result in a structured atomic type, rather than simply `int32_t` values._

# Comparison to APR #

APR has defined an API around a number of atomic operations, and is useful as a comparison point for the design of PoCore's API. This section explains the differences, and rationale, from the APR model.

### read32/set32 ###

These two functions are throwbacks to **very** old systems. Processors and compilers will _always_ read/write values as an atomic unit. Classically, it was possible for these operations to span clock cycles, fetching partial word-sizes in each cycle, and (thus) open to race conditions around interrupts modifying partial-words between those cycles. This is simply not a concern in today's systems, so these "operations" are not part of PoCore's API.

These functions imply a memory barrier, but PoCore is designed around the four basic functionality points rather than low-level concerns about the values at a given point during execution.

_Note: if we move to structured values, then functions like this will be needed to fetch/store values -- not for atomicity purposes, but just basic access._

### add32/sub32 ###

These functions can be implemented with code similar to:
```
  do {
    value = *mem;
    swapped = compare_and_swap(mem, value, value + delta);
  } while (!swapped);
```

Theoretically, the above loop will never complete where multiple writers are continuously modifying _mem_, but in all practical scenarios it succeeds on a single iteration.

### inc32/dec32 ###

PoCore will provide these as "generally useful APIs", and they are effectively implemented as `add32(mem, 1)` and `sub32(mem, 1)`. In reality, they will be implemented in terms of compare-and-swap (see the loop above for add32/sub32), or possibly optimized for this specific functionality.

### cas32/casptr ###

This is essentially the compare-and-swap functionality that PoCore will implement. However, the return value for PoCore will be an "is swapped" boolean, rather than the old value. In almost all cases, the old value is completely irrelevant (since it is subject to races), and the only interesting piece of information is "was my new value installed?"

### xchg32/xchgptr ###

These functions are implemented similar to the loop described above for `add32`, but where the new value is specified instead of `value+delta`. This variant is used (for example) where the old value is a reference to (old) information that needs to be cleared/processed now that a new value has been stored.