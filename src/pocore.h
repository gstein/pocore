/*
  pocore.h :  PoCore's internal declarations

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

#ifndef POCORE_H
#define POCORE_H

#include <stdlib.h>  /* for abort() in NOT_IMPLEMENTED()  */
#include <stdio.h>  /* for fprintf() in PC_DBG()  */

#include "pc_types.h"
#include "pc_mutex.h"
#include "pc_error.h"
#include "pc_cleanup.h"

/* Get our private platform-specific stuff.  */
#include "pocore_platform.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ### for now, many of the library's core structures are library-visible.
   ### many will become private.
*/


/* Add debugging support. This is omitted from release builds.  */
/* ### not sure this construction is entirely right, but whatever. we want
   ### this stuff in here right now.  */
#ifndef NDEBUG
#define PC_DEBUG
#endif

/* Default and minimum standard block size.

   ### the minimum size (256) is just a number. the real minimum is probably
   ### sizeof(struct pc_memtree_s) with maybe some other padding. not sure
   ### that we want to allow such a small block though.  */
#define PC_MEMBLOCK_SIZE 8192
#define PC_MEMBLOCK_MINIMUM 256


/* For areas that aren't implemented yet...  */
#define NOT_IMPLEMENTED()  abort()


#define PC_DBG(fmt, ...) fprintf(stderr, "DBG: %s:%d: " fmt "\n", \
                                 __FILE__, __LINE__, __VA_ARGS__)
#define PC_DBG0(msg) fprintf(stderr, "DBG: %s:%d: %s\n", \
                             __FILE__, __LINE__, (msg))

/* When we know the error will be unhandled, use this macro so that we
   can easily find them.  */
#define PC_UNHANDLED(ctx, code) ((void) pc_error_create_x(ctx, code))


struct pc_tracklist_s {
    union pc_trackreg_u *reg;
    struct pc_tracklist_s *next;
};


/* Track registration record.  */
union pc_trackreg_u {
    struct {
        /* In many cases, TRACKED is passed to us, and is also the key in
           the CTX->PTR_TO_REG hash table. But if we navigate to this
           registration via the OWNERS or DEPENDENTS links, then we'll
           need the original TRACKED pointer to call CLEANUP_FUNC.  */
        const void *tracked;

        /* The cleanup function registered for this item.  */
        void (*cleanup_func)(void *tracked);

        /* ### use an array-based structure to eliminate NEXT ptrs?  */
        struct pc_tracklist_s *owners;
        struct pc_tracklist_s *dependents;
    } a;  /* allocated trackreg  */

    /* ### hrm. can't we just omit the struct and drop the field into
       ### the union? is there something else we need in this struct?  */
    struct {
        union pc_trackreg_u *next;
    } f;  /* free'd trackreg  */
};


struct pc_cleanup_list_s {
    /* The data item requiring a cleanup.  */
    const void *data;

    /* Its cleanup function.  */
    pc_cleanup_func_t cleanup;

    struct pc_cleanup_list_s *next;
};


struct pc_block_s {
    /* This size INCLUDES the space used by this structure.  */
    size_t size;

    /* Blocks are typically placed into lists: those allocated towards a pool,
       or those free'd from a pool and returned to the owning context. This
       link chains the blocks together.  */
    struct pc_block_s *next;
};


/* ### need to shift pc_context_s in this file. for now, this will do.  */
struct pc_error_list_s;


/* ### need to document: creating multiple roots will divide the
   ###   std_blocks across those roots. for the best sharing of
   ###   already-allocated memory, it will be best to create as few
   ###   root pools as possible.  */
struct pc_memroot_s {
    /* The root of the pool tree using this particular configuration.  */
    pc_pool_t *pool;

    /* When grabbing memory from the OS, what is the "standard size" to
       grab each time?  */
    size_t stdsize;

    /* A linked-list of available standard-sized blocks to use.  */
    struct pc_block_s *std_blocks;

    /* The context maintains a list of roots. This connects the list.  */
    struct pc_memroot_s *next;
};


struct pc_context_s {
    /* ### return values: try one more time. return NULL. abort.  */
    int (*oom_handler)(size_t amt);

#ifdef PC__IS_WINDOWS
    /* Each context gets its own custom Heap so that we don't have to
       deal with the cross-thread mutex in the global heap (malloc).  */
    HANDLE heap;
#endif

#ifndef FIX_CODE_TO_USE_MEMROOT
    /* When grabbing memory from the OS, what is the "standard size" to
       grab each time?  */
    size_t stdsize;

    /* A linked-list of available standard-sized blocks to use.  */
    struct pc_block_s *std_blocks;
#endif

    /* All the root pools allocated by this context.  */
    struct pc_memroot_s *roots;

    /* A tree of non-standard-sized blocks. These are available for use
       on a best-fit basis.

       Different pool trees have a variant idea of what "standard" size
       means. It is entirely possible that a block within this memtree
       is smaller or equal to what a specific pool considers "standard".
       However, those pools with a larger stdsize will never ask for these
       smaller blocks. Thus, this tree will have some natural partitioning
       across pools' stdsize values. Small-stdsize pools may request a
       non-standard size, and they will locate those from "all" the various
       pools in the context. Sort of a universal-donor versus
       universal-recipient process occurring here.  */
    struct pc_memtree_s *nonstd_blocks;

    /* ### chained hashes to prevent realloc? subpool for this?
       ### we'll probably have the hash code return memory to its pool,
       ### so a realloc will not be much of a problem.  */
    /* Map tracked pointers to registration structures. This hash is
       created on-demand within TRACK_POOL (also created on-demand).  */
    pc_hash_t *ptr_to_reg;

    /* Free registration structures.  */
    union pc_trackreg_u *free_treg;

    /* Free tracking list structures.  */
    struct pc_tracklist_s *free_tlist;

    /* Free cleanup list records.  */
    struct pc_cleanup_list_s *free_cl;

    /* The pool to use for additional tracking allocations. This will be
       created on-demand and owned by the context.  */
    struct pc_pool_s *track_pool;
    struct pc_pool_s *cleanup_pool;

    /* Pool to hold all errors associated with this context. This will be
       created on-demand and owned by the context.  */
    struct pc_pool_s *error_pool;

    /* Error mappings. Namespace -> pc_errmap_t  */
    pc_hash_t *emaps;

    /* ### need mechanism to hook errors into this context.
       ### if TRACK_UNHANDLED is TRUE, then we will allocate errors as
       ### pc_error_list_s structures and link them into this list.
       ### otherwise, UNHANDLED will be NULL.  */
    pc_bool_t track_unhandled;
    struct pc_error_list_s *unhandled;

    /* Should we actually insert PC_ERR_TRACE records?  */
    pc_bool_t tracing;
    
    /* General-use mutex. To avoid contention, this mutex is/should only
       be used for:

       - pc_atomic_once()
    */
    pc_mutex_t *general_mutex;

    /* Some context specific to the channel subsystem. This lets us keep
       a lot of implementation-specific detail out of the primary context
       structure. We can also tell *if* the subsystem has been initialized
       since this starts as NULL.  */
    struct pc__channel_ctx_s *cctx;
};


struct pc_pool_s {
    char *current;

    /* Should allocations made in this pool be coalescable? Or more
       specifically: when memory is returned to this pool, should we
       attempt to coalesce them?  */
    pc_bool_t coalesce;

    /* Standard-size blocks are linked from the pool since a single block
       may be shared across multiple posts.  */
    struct pc_block_s *current_block;

    /* The first block allocated. This block, through .current_block, are
       all of the blocks associated with this pool.  */
    struct pc_block_s *first_block;

    /* Any remnants for later use by this pool.  */
    struct pc_memtree_s *remnants;

    /* Any nonstd-sized blocks allocated for this pool. These will
       be queued back into the context when we clear the pool.  */
    struct pc_block_s *nonstd_blocks;

    /* The context this pool is associated with.  */
    struct pc_context_s *ctx;

    /* The root of this tree of pools, specifying our configuration.  */
    struct pc_memroot_s *root;

    /* The parent of this pool.  */
    struct pc_pool_s *parent;

    /* The sibling of this pool; used to list all the children of PARENT.  */
    struct pc_pool_s *sibling;

    /* The child pools, which are linked through their SIBLING member.  */
    struct pc_pool_s *child;

    /* The cleanups registered with this pool.  */
    struct pc_cleanup_list_s *cleanups;

    /* Inlined. Every pool has a set of owners (tho no dependents). Using
       a trackreg structure allows the owners to deregister/cleanup and
       to update the pool's tracking, like any other dependent.

       When a trackreg is free'd, we can avoid putting this onto the
       FREE_TREG list by examing the CLEANUP_FUNC (is it the pool's func?)  */
    union pc_trackreg_u track;
};


/* A red-black binary tree containing pieces of memory to re-use.

   These pieces are:

     1) remnants from the end of a standard-sized block that were
        "left behind" when we allocated and advanced to another block
        to satisfy a request.
     2) non-standard-sized (large) blocks that have been returned
     3) portions of a returned, non-standard-sized block that was left
        behind after an allocation smaller than that block

   Note that the size of this structure provides a minimize size for
   remnants. If a remnant is smaller than this structure, it is simply
   "thrown away".

   We use red-black trees to guarantee worst-case time of O(log n) for
   all operations on this tree. We cannot afford O(n) worst case. For
   more information on red-black trees, see:
     http://en.wikipedia.org/wiki/Red-black_tree
*/
struct pc_memtree_s {
    /* The block contains this node's size, and NEXT field links to other
       (free) blocks of this same size.

       Note that the size's low-order bit is a flag. See the various
       macros in red_black.c.  */
    struct pc_block_s b;

    /* Any pieces that are SMALLER than this piece.  */
    struct pc_memtree_s *smaller;

    /* Any pieces that are LARGER than this piece.  */
    struct pc_memtree_s *larger;
};


struct pc_error_s {
    /* Context this error is associated with. Through this CTX, we find the
       pool to use for wrapping errors, and for tracking unhandled errors.  */
    pc_context_t *ctx;

    /* ### need some set of error codes for PoCore. redefining OS errors
       ### like APR is kind of a lost cause, I think. so this should
       ### probably just be a set of recognized, high-level errors. where
       ### the lower-level APIs return errno values of significance, we
       ### can create a code for them.  */
    int code;

    const char *msg;

    /* The file and line number that created this error. Typically, this is
       only available when PC_DEBUG is defined.  */
    const char *file;
    int lineno;

    /* ### svn has concepts like below, but PoCore is probably flat enough
       ### that we don't need stacks of errors. let's see what evolves.  */
    /* ### but apps build upon this error system, and want the wrapping,
       ### tracing, and association features.  */

    /* This error is providing additional information. More details are
       given in ORIGINAL.  */
    struct pc_error_s *original;

    /* A separate error occurred while processing this error (or ORIGINAL).
       It is not specifically related to ORIGINAL or the root cause of
       this error stack. Typically, these errors occur while recovering
       from ORIGINAL.  */
    struct pc_error_s *separate;
};


struct pc_error_list_s {
    /* The actual error is an embedded structure.  */
    pc_error_t error;

    /* These errors are part of a doubly-linked list, allowing for easy
       removal. These values will be NULL for wrapped and "separate" errors.
       Only the "root" of a tree of errors will be recorded into the
       UNHANDLED list maintained in the related context.  */
    struct pc_error_list_s *previous;
    struct pc_error_list_s *next;
};


struct pc_errmap_s {
    /* The associated context for sorting out error namespaces.  */
    pc_context_t *ctx;

    /* The namespace this map will manage.  */
    const char *ns;

    /* The base error value assigned to this error map.  */
    int baseval;

    /* The callback to get a default error message.  */
    pc_errmap_message_cb message_cb;
    void *message_baton;
};


/* ### docco  */
void
pc__memtree_insert(struct pc_memtree_s **root,
                   void *mem,
                   size_t size);


/* ### docco  */
struct pc_block_s *
pc__memtree_fetch(struct pc_memtree_s **root, size_t size);


/* Lazy-initialize the mutex within CTX.  */
void pc__context_init_mutex(pc_context_t *ctx);


/* Clean up the channel context.  */
void pc__channel_cleanup(pc_context_t *ctx);


/* Convert errno (### Windows equivalent!) into a pc_error_t.  */
pc_error_t *pc__convert_os_error(pc_context_t *ctx);


#ifdef PC_DEBUG

int
pc__memtree_depth(const struct pc_memtree_s *node);

void
pc__memtree_print(const struct pc_memtree_s *root);

#endif /* PC_DEBUG  */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* POCORE_H */
