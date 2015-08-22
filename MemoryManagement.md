This page describes the overall design, structures, and organization of PoCore's memory management system.

_more info TBD_

_reminder to self:_
we may want some Windows-specific allocation rather than malloc(). research starting [here](http://msdn.microsoft.com/en-us/library/aa366533(VS.85).aspx).

## Coalescing ##

_More info needed. Here are some rough notes_

When freeing a block, there are four cases, based on whether the blocks before/after the free'd block are allocated or part of the free block system.

```
#define PC__THIS_ALLOCATED 0x1
#define PC__AFTER_ALLOCATED 0x2
#define PC__BEFORE_DATA(b) (((size_t *)(b))[-1])
#define PC__THIS_DATA(b,len) (*(size_t *)((char *)(b) + (len)))
#define PC__BEFORE_ALLOCATED(b) ((PC__BEFORE_DATA(b) & PS__IS_ALLOCATED) != 0)
#define PC__AFTER_ALLOCATED(b,len) ((PC__THIS_DATA(b,len) & PC__AFTER_ALLOCATED) != 0)
#define PC__GET_SIZE(data) ((data) & ~3)
```

In the data field, the size is used to record "this" block's length if it is free. It records the "after" block's length if that block is free. Otherwise, it is unused ("this" and "after" are both allocated, thus the data value is 0x3). Note that there is no conflict on storing the size because both "this" and "after" cannot be free -- they would have been coalesced.

After each free operation, the block will be coalesced with its neighbors, if possible, and the resulting block will be stored into `remnants`. The (new) free block's size will be recorded into `PC__BEFORE_DATA`, also clearing `PC__AFTER_ALLOCATED`. The free block's size will also be recorded into `PC__THIS_DATA`, along with clearing `PC__THIS_ALLOCATED`.

  * _before_/_after_ are allocated
> _block_ is added to the remnants list.

  * _before_ is allocated, _after_ is free
> _after_ is extracted from the remnants (its size is in `PC__THIS_DATA`), combined with _block_, and re-entered into `remnants`.

  * _before_ is free, _after_ is allocated
> _before_ is extracted from the remnants (its size is in `PC__BEFORE_DATA`), combined with _block_, and re-entered into `remants`

  * _before_/_after_ are free
> _before_ and _after_ are extracted from the remnants, combined with _block_, and re-enetered into `remnants`

At the start of the region (a `pc_block_s`), a data field is inserted and marked with `PC__THIS_ALLOCATED` to avoid coalescing a "before" block which does not exist. The free block which touches the end of the region has `PC__AFTER_ALLOCATED` stored in it to avoid coalesced a non-exist "after" block.

Finding a given block in the remnants tree is an O(log N) process to locate the tree node of the exact size, then an O(M) scan of its list of same-size blocks. Returning the block is a simple unlink from that list (O(1)), adjusting a parent tree node to point to a block on the same-size list (O(1)), or removing the node from the tree (O(log N)).