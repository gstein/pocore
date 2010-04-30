/*
  types.c :  basic PoCore data types

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

#include <string.h>

#include "pc_types.h"
#include "pc_memory.h"

#include "pocore.h"


/* This hash table is implemented as an "open-addressing" table, and
   uses the FNV-1 32-bit hash algorithm.

   Some useful reference materials:

     FNV-1 hash algorithm:
     http://www.isthe.com/chongo/tech/comp/fnv/

     Comparisons of hashing algorithms:
     http://www.eternallyconfuzzled.com/tuts/algorithms/jsw_tut_hashing.aspx

     Notes about hash table design:
     http://svn.python.org/view/python/trunk/Objects/dictnotes.txt?view=markup
*/

/* All twin primes less than 2^31 (this table contains the lower of the
   pair). Our hash tables will not be allowed to grow larger than the
   largest prime in this array.

   Note: our slots are (on most architecture) 16 bytes. If size_t is just
   32 bits, then assume we can allocate no more than 2^31 for the slots,
   which implies a maximum of 2^27 slots (134,217,728).  */
static const int twins[] = {
           11,         17,         29,        59,       101,       179,
          269,        419,        641,      1019,      1607,      2549,
         3851,       5849,       8819,     13337,     20021,     30089,
        45137,      67757,     101747,    152639,    228959,    343529,
       515369,     773081,    1159661,   1739579,   2609489,   3914291,
      5871557,    8807417,   13211579,  19817657,  29726561,  44589869,
     66884999,  100327949
/* ### gotta get this symbol from somewhere.  */
#ifdef SIZE_T_IS_64BIT
    /* comma for after "last" element: */ ,
    150492581,  225739097,  338608757, 507913409,
    761870579, 1142806109, 1714209851
#endif
};
#define TWINS_COUNT (sizeof(twins) / sizeof(twins[0]))
/* Where does a twin > 10,000,000 occur in the above table?  */
#define TWIN_MIN_10MIL 32

/* Our desired loading factors, expressed as a percentage [0..100]  */
#define MAX_LOAD_PERCENT 65
#define INITIAL_LOAD 30

static const int slot_deleted;
#define SLOT_DELETED (&slot_deleted)
/* ### ugh. SLOT_DELETED markers cannot be cleaned up short of a full
   ### rebuild. true? is there a way we can remove them?
   ### while we can't remove DELETED, we can shift it around. consider:
   ### seeking A. find a deleted slot. go to next slot and we find A.
   ### move item to the deleted slot. mark current as DELETED.
*/

struct pc_hslot_s {
    const void *key;
    size_t klen;
    pc_u32_t hvalue;
    void *value;
};

struct pc_hash_s {
    int count;

    int twins_index;
    int alloc;  /* always the higher value of the twin primes  */
    struct pc_hslot_s *slots;

    /* Keep the pool around to realloc SLOTS  */
    /* ### in the future, we may find a sneakier way to do this. maybe we
       ### could use/free nonstd blocks.  */
    pc_pool_t *pool;
};

struct pc_hiter_s {
    const struct pc_hslot_s *current;
    const struct pc_hslot_s *last;
};


static pc_u32_t
compute_hvalue(const pc_u8_t *key, size_t klen)
{
    /* FNV-1 for 32-bit hash values  */

    const pc_u8_t *end = key + klen;
    pc_u32_t hvalue = 0x811c9dc5;

    while (key < end)
        hvalue = (hvalue * 16777619) ^ *key++;

    return hvalue;
}


static pc_u32_t
compute_str_hvalue(const char *key, size_t *klen)
{
    /* FNV-1 for 32-bit hash values  */

    const char *start = key;
    pc_u32_t hvalue = 0x811c9dc5;

    while (*key != '\0')
        hvalue = (hvalue * 16777619) ^ (pc_u8_t)*key++;

    *klen = key - start;
    return hvalue;
}


/* Returns TRUE if the key was previously in the hash.  */
static pc_bool_t
insert_item(const struct pc_hslot_s *item, struct pc_hslot_s *slots, int alloc)
{
    pc_bool_t existed = FALSE;
    int idx = item->hvalue % alloc;
    int step = (item->hvalue % (alloc - 2)) + 1;
    struct pc_hslot_s *probe = &slots[idx];
    struct pc_hslot_s *deleted = NULL;

    /* Given that ALLOC is prime, then any value of STEP will sequence
       through all of SLOTS before returning to the initial probe at IDX.
       To see this, consider how we would get back to IDX:

         (IDX + N * STEP) % ALLOC == IDX

       for N probes through the slots. Subtract out IDX:

         (N * STEP) % ALLOC == 0

       and remove the modulus operation:

         N * STEP == M * ALLOC

       ... and one more rewrite:

         (N * STEP) / M == ALLOC

       Since ALLOC is prime, there is no M that is a factor of ALLOC. Thus,
       M must be a factor of N and/or STEP, leaving us to find the value
       of ALLOC in N and/or STEP. Since STEP < ALLOC, that implies ALLOC
       is a factor of N (N = X * ALLOC). And M = X * STEP.

       We will never probe more than ALLOC times -- an empty slot will
       be in there somewhere.

       Twin primes are used so that we get STEP values that cover almost
       the entire range up to ALLOC. This makes it much harder for items
       to lockstep on the same STEP, creating chains that we must navigate
       to find an empty slot or a target item. We could generate a STEP
       just based on ALLOC, but using another prime against the HVALUE
       introduces better distribution.
    */

    while (probe->key != NULL)
    {
        if (probe->key == SLOT_DELETED)
        {
            /* Remember this slot. We may be able to use it.  */
            if (deleted == NULL)
                deleted = probe;
        }
        else if (probe->hvalue == item->hvalue
                 && probe->klen == item->klen
                 && memcmp(probe->key, item->key, probe->klen) == 0)
        {
            /* We found the node!  */
            existed = TRUE;
            break;
        }

        idx = (idx + step) % alloc;
        probe = &slots[idx];
    }

    existed = (probe->key != NULL);

    /* If we did not find the key, there MAY be a deleted slot that we can
       use which is "closer" to the start.  */
    if (!existed && deleted != NULL)
        probe = deleted;

    if (item->value == NULL)
        probe->key = SLOT_DELETED;
    else
        *probe = *item;

    return existed;
}


static void *
find_value(const pc_hash_t *hash, const void *key, size_t klen,
           pc_u32_t hvalue)
{
    /* See insert_item() for a discussion of our probing strategy.  */
    int start = hvalue % hash->alloc;
    int idx = start;
    int step = (hvalue % (hash->alloc - 2)) + 1;
    struct pc_hslot_s *probe = &hash->slots[idx];

    /* ### add code to remember the first SLOT_DELETED that we ran across.
       ### if we later find our target, then move it "closer" to START
       ### by copying it into the deleted slot, and marking its (old)
       ### location as SLOT_DELETED.  */

    do
    {
        /* If we hit an unused slot, then this item was never stored.  */
        if (probe->key == NULL)
            return NULL;

        if (probe->hvalue == hvalue
            && probe->klen == klen
            && memcmp(probe->key, key, klen) == 0)
            return probe->value;

        idx = (idx + step) % hash->alloc;
        probe = &hash->slots[idx];

    } while (idx != start);

    /* Wow. Every single slot in the table is used or is SLOT_DELETED.
       We may want to consider rebuilding the slots to get rid of all
       those SLOT_DELETED elements.  */

    /* Not found!  */
    return NULL;
}


static void
copy_items(const pc_hash_t *hash, struct pc_hslot_s *slots, int alloc)
{
    const struct pc_hslot_s *current = hash->slots;
    const struct pc_hslot_s *last = &hash->slots[hash->alloc];

    for ( ; current < last; ++current)
        if (current->key != NULL && current->key != SLOT_DELETED)
            insert_item(current, slots, alloc);
}


static void
maybe_grow(pc_hash_t *hash)
{
    int threshold;
    int alloc;
    struct pc_hslot_s *slots;

    if (hash->twins_index == TWINS_COUNT - 1)
        return;

    if (hash->alloc < 10000000)
        threshold = (hash->alloc * MAX_LOAD_PERCENT) / 100;
    else
        threshold = (hash->alloc / 100) * MAX_LOAD_PERCENT;

    if (hash->count + 1 < threshold)
        return;

    alloc = twins[++hash->twins_index] + 2;  /* the higher of the pair!  */
    slots = pc_alloc(hash->pool, alloc * sizeof(*slots));
    memset(slots, 0, alloc * sizeof(*slots));

    copy_items(hash, slots, alloc);

    /* ### free old hash->slots  */
    hash->alloc = alloc;
    hash->slots = slots;
}


pc_hash_t *pc_hash_create(pc_pool_t *pool)
{
    return pc_hash_create_min(pool, 0);
}


pc_hash_t *pc_hash_create_min(pc_pool_t *pool, int min_items)
{
    pc_hash_t *result = pc_alloc(pool, sizeof(*result));

    result->count = 0;
    result->pool = pool;

    if (min_items > 1000000000)
        result->twins_index = TWINS_COUNT - 1;
    else
    {
        /* Scale up MIN_ITEMS based on our desired initial load factor  */
        if (min_items > 10000000)
        {
            min_items = (min_items / INITIAL_LOAD) * 100;
            result->twins_index = TWIN_MIN_10MIL;
        }
        else
        {
            min_items = (min_items * 100) / INITIAL_LOAD;
            result->twins_index = 0;
        }

        while (twins[result->twins_index] < min_items
               && result->twins_index < TWINS_COUNT - 1)
            ++result->twins_index;
    }

    result->alloc = twins[result->twins_index] + 2;  /* higher of the pair!  */
    result->slots = pc_alloc(pool, result->alloc * sizeof(*result->slots));
    pc_hash_clear(result);

    return result;
}


pc_hash_t *pc_hash_copy(const pc_hash_t *hash, pc_pool_t *pool)
{
    /* Create a hash large enough to hold all these items.  */
    pc_hash_t *result = pc_hash_create_min(pool, hash->count);

    /* And insert the items into the (clean) RESULT.  */
    copy_items(hash, result->slots, result->alloc);
    result->count = hash->count;

    return result;
}


void pc_hash_set(pc_hash_t *hash, const void *key, size_t klen, void *value)
{
    pc_u32_t hvalue = compute_hvalue(key, klen);
    struct pc_hslot_s item = { key, klen, hvalue, value };
    pc_bool_t existed;

    maybe_grow(hash);
    existed = insert_item(&item, hash->slots, hash->alloc);
    if (!existed && value != NULL)
        ++hash->count;
    else if (existed && value == NULL)
        --hash->count;
}


void pc_hash_sets(pc_hash_t *hash, const char *key, void *value)
{
    size_t klen;
    pc_u32_t hvalue = compute_str_hvalue(key, &klen);
    struct pc_hslot_s item = { key, klen, hvalue, value };
    pc_bool_t existed;

    maybe_grow(hash);
    existed = insert_item(&item, hash->slots, hash->alloc);
    if (!existed && value != NULL)
        ++hash->count;
    else if (existed && value == NULL)
        --hash->count;
}


void *pc_hash_get(const pc_hash_t *hash, const void *key, size_t klen)
{
    pc_u32_t hvalue = compute_hvalue(key, klen);

    return find_value(hash, key, klen, hvalue);
}


void *pc_hash_gets(const pc_hash_t *hash, const char *key)
{
    size_t klen;
    pc_u32_t hvalue = compute_str_hvalue(key, &klen);

    return find_value(hash, key, klen, hvalue);
}


void pc_hash_clear(pc_hash_t *hash)
{
    hash->count = 0;
    memset(hash->slots, 0, hash->alloc * sizeof(*hash->slots));
}


int pc_hash_count(const pc_hash_t *hash)
{
    return hash->count;
}


pc_hiter_t *pc_hiter_begin(const pc_hash_t *hash, pc_pool_t *pool)
{
    pc_hiter_t *result = pc_alloc(pool, sizeof(*result));

    result->current = hash->slots;
    result->last = &hash->slots[hash->alloc];
    if (result->current->key == NULL || result->current->key == SLOT_DELETED)
        return pc_hiter_next(result);
    return result;
}


pc_hiter_t *pc_hiter_next(pc_hiter_t *hiter)
{
    while (TRUE)
    {
        const struct pc_hslot_s *cur = ++hiter->current;

        if (cur == hiter->last)
            return NULL;
        if (cur->key != NULL && cur->key != SLOT_DELETED)
            return hiter;
    }
}


const void *pc_hiter_key(const pc_hiter_t *hiter)
{
    return hiter->current->key;
}


size_t pc_hiter_klen(const pc_hiter_t *hiter)
{
    return hiter->current->klen;
}


void *pc_hiter_value(const pc_hiter_t *hiter)
{
    return hiter->current->value;
}
