/*
  array.c :  basic array type

  ====================================================================
    Copyright 2012 Greg Stein

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

#include "pc_memory.h"

#include "pocore.h"

#define ELEM_ADDR(ary, i) ((char *)(ary)->elems + (i) * (ary)->elem_size)

/* For "smallish" arrays, if we need to resize, then just bump it up to
   100 elements (no need to be super careful here). Or to 1000 or 10,000.
   For arrays larger than that, we grow the array by 1.5x.

   Note: this isn't all that great when N=99 (etc), but that should only
   happen when somebody *creates* the array at that size. And once we
   hit one bump, the next will grow nicely.  */
#define NEW_ALLOC(n) ((n) < 100 ? 100 : \
                      (n) < 1000 ? 1000 : \
                      (n) < 10000 ? 10000 : (((n) * 3) / 2))


pc_array_t *pc_array_create(int alloc, size_t elem_size, pc_pool_t *pool)
{
    pc_array_t *ary = pc_alloc(pool, sizeof(*ary));

    /* Note: we allocate ELEMS separately from ARY so that we can return
       the ELEMS back to the pool in the array is resized.  */
    ary->elems = pc_alloc(pool, alloc * elem_size);
    ary->elem_size = elem_size;
    ary->count = 0;
    ary->alloc = alloc;
    ary->pool = pool;

    return ary;
}


pc_array_t *pc_array_copy(const pc_array_t *ary, pc_pool_t *pool)
{
    pc_array_t *copy = pc_alloc(pool, sizeof(*ary));

    copy->elems = pc_memdup(pool, ary->elems, ary->alloc * ary->elem_size);
    copy->elem_size = ary->elem_size;
    copy->count = ary->count;
    copy->alloc = ary->alloc;
    copy->pool = pool;

    return copy;
}


void *pc_array_add(pc_array_t *ary)
{
    if (++ary->count > ary->alloc)
    {
        size_t old_size = ary->alloc * ary->elem_size;
        int alloc = NEW_ALLOC(ary->alloc);
        void *new_elems = pc_alloc(ary->pool, alloc * ary->elem_size);

        /* Copy over all prior elements.  */
        memcpy(new_elems, ary->elems, old_size);

        /* Return the prior element memory to the pool.  */
        pc_pool_freemem(ary->pool, ary->elems, old_size);

        /* Ready. Use the new elements.  */
        ary->elems = new_elems;
    }

    return ELEM_ADDR(ary, ary->count - 1);
}

 
void pc_array_delete(pc_array_t *ary, int idx)
{
    assert(idx >= 0 && idx < ary->count);

    if (idx < --ary->count)
        memmove(ELEM_ADDR(ary, idx), ELEM_ADDR(ary, idx + 1),
                (ary->count - idx) * ary->elem_size);
}
