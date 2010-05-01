/*
  memtree.c :  a red-black tree for use by the memory subsystem

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

/*
  For further information, see:

    http://en.wikipedia.org/wiki/Red-black_tree
    http://www.upgrade-cepis.org/issues/2004/5/up5-5Mosaic.pdf


  It is important to note that the nodes in this tree represent the
  free memory. We cannot copy node information from one to the next.
  Thus, node deletion requires manipulation of the tree instead of
  simply moving values around (as described by most documentation of
  the red-black tree algorithm).
*/

#include "pc_types.h"

#include "pocore.h"

/* The maximum depth of any node in a memtree. A red-black does not have
   to be perfectly-balanced. The depth of a node can be up to twice that
   of other nodes. If we want 2^32 nodes in this tree (gasp!), then some
   nodes could be at depth 32, and others at 64.  */
#define MT_DEPTH 64

/* P is the parents[] variable. D is the depth of the node.  */
#define MT_PARENT(P, D) ((D) ? (P)[(D)-1] : NULL)
#define MT_GRANDPARENT(P, D) ((D) > 1 ? (P)[(D)-2] : NULL)

/* Macros to manage pc_memtree_s.b.size.  */
/* ### might have to make MT_SIZE public at some point.  */
#define MT_SIZE(m)  ((m)->b.size & ~1)

#define MT_IS_BLACK(m)  (((m)->b.size & 1) == 0)
#define MT_IS_RED(m)  (((m)->b.size & 1) == 1)

#define MT_MAKE_BLACK(m)  ((m)->b.size &= ~1)
#define MT_MAKE_RED(m)  ((m)->b.size |= 1)


/* ### inline this  */
static struct pc_memtree_s *
get_uncle(struct pc_memtree_s *parents[], int depth)
{
    struct pc_memtree_s *gramps = MT_GRANDPARENT(parents, depth);

    /* ### there is one call-site, and GRAMPS is not NULL.  */
    if (gramps == NULL)
        return NULL;
    if (MT_PARENT(parents, depth-1) == gramps->smaller)
        return gramps->larger;
    return gramps->smaller;
}


/* See: http://en.wikipedia.org/wiki/Tree_rotation  */
/* ### inline this  */
static void
rotate_left(struct pc_memtree_s *pivot, struct pc_memtree_s **root)
{
    (*root)->larger = pivot->smaller;
    pivot->smaller = *root;
    *root = pivot;
}


/* See: http://en.wikipedia.org/wiki/Tree_rotation  */
/* ### inline this  */
static void
rotate_right(struct pc_memtree_s *pivot, struct pc_memtree_s **root)
{
    (*root)->smaller = pivot->larger;
    pivot->larger = *root;
    *root = pivot;
}


void
pc__memtree_insert(struct pc_memtree_s **root,
                   void *mem,
                   size_t size)
{
    struct pc_memtree_s *parents[MT_DEPTH];
    int depth;
    struct pc_memtree_s *scan;
    struct pc_memtree_s *node;
    struct pc_memtree_s *gramps;
    struct pc_memtree_s *parent;
    struct pc_memtree_s **rotation_parent;

    /* The algorithm used here is lifted from the Wikipedia page.  */

    /* ### for the rest of this function, we assume size is properly
       ### aligned. whatever boundary. but it should match the size that
       ### the underlying memory block has available.  */

    /* insert_case1()  */
    if (*root == NULL)
    {
        /* The tree is empty. Turn the given memory into a pc_memtree_s,
           mark it black, and make it the root.  */

        /* SIZE is assumed to be aligned, and (thus) the low-order bit is
           0, which means BLACK.  */
        node = mem;
        node->b.size = size;
        node->b.next = NULL;
        node->smaller = NULL;
        node->larger = NULL;

        /* Initialize the tree.  */
        *root = node;

        return;
    }

    /* This is not a case listed specifically on Wikipedia, but is at the
       start of the "Insertion" section. We are performing the binary-tree
       insertion. For later use, we also remember all the parents as we
       traverse down the tree to our target node.  */

    depth = 0;
    scan = *root;
    do
    {
        /* Remember this for when we need parental information, since the
           nodes don't track it themselves.  */
        parents[depth] = scan;

        if (scan->b.size == size)
        {
            /* Woo hoo! Easy out. No tree manipulations needed since we found
               a node with the same size. We can simply turn this memory into
               a standard pc_block_s and hook it into the chain from this
               node.  */
            struct pc_block_s *block = mem;

            block->size = size;
            block->next = scan->b.next;
            scan->b.next = block;
            return;
        }

        if (size < scan->b.size)
        {
            if (scan->smaller == NULL)
            {
                /* We're about to turn MEM into a node. It's size is smaller
                   than SCAN, so let's hook it in, then break out of this
                   loop to set it up.  */
                scan->smaller = mem;
                break;
            }

            /* Scan down the smaller-sized branch.  */
            scan = scan->smaller;
        }
        else
        {
            if (scan->larger == NULL)
            {
                /* We're about to turn MEM into a node. It's size is larger
                   than SCAN, so let's hook it in, then break out of this
                   loop to set it up.  */
                scan->larger = mem;
                break;
            }

            /* Scan down the larger-sized branch.  */
            scan = scan->larger;
        }

        /* We're moving deeper.  */
        ++depth;

    } while (depth < MT_DEPTH);

    /* If we managed to fall out of that loop at MT_DEPTH, then give up.  */
    assert(depth < MT_DEPTH);

    /* Turn MEM into a proper pc_memtree_s node.  */
    node = mem;
    node->b.size = size | 1;  /* mark it RED  */
    node->b.next = NULL;
    node->smaller = NULL;
    node->larger = NULL;

    /* Now that we've inserted the node into the tree, it is time to adjust
       the tree back into its Proper State (ie. meet all 5 properties).  */

    /* insert_case2(). Note that we may loop back to this point.  */
  insert_case2:
    /* If we reach here, then a parent MUST exist. Falling through from
       above, this node was inserted below the root. Or reaching here
       from the 'goto' below, the current depth does not refer to the
       root. Thus, a parent must exist.  */
    parent = MT_PARENT(parents, depth);
    /* ### do some PC_DEBUG magic here?  */
    assert(parent != NULL);

    if (MT_IS_BLACK(parent))
        return;

    /* The grandparent is needed within the case3 conditional, and for
       all parts of case4, so we can fetch it once here.  */
    gramps = MT_GRANDPARENT(parents, depth);

    /* The parent is RED, so persuant to property 2, the parent is NOT
       the root. Thus, the grandparent exists.  */
    /* ### do some PC_DEBUG magic here?  */
    assert(gramps != NULL);

    /* insert_case3()  */
    {
        struct pc_memtree_s *uncle = get_uncle(parents, depth);

        if (uncle != NULL && MT_IS_RED(uncle))
        {
            MT_MAKE_BLACK(parent);
            MT_MAKE_BLACK(uncle);

            MT_MAKE_RED(gramps);

            /* insert_case1() is here again, but where *ROOT is not NULL.  */
            if (gramps == *root)
            {
                MT_MAKE_BLACK(gramps);
                return;
            }

            /* If the above condition of insert_case1() does not fire, then
               we move on to insert_case2(), from the grandparent. We can
               only perform O(log n) jumps back up the tree.  */
            depth -= 2;
            goto insert_case2;
        }
    }
    /* Uncle is BLACK (or does not exist, so is implicitly black).  */
    /* GRAMPS is not NULL.  */

    /* insert_case4()  */
    {
        struct pc_memtree_s *new_node = NULL;

        /* Whack the tree into a state where we can rotate around GRAMPS
           to put the tree back into shape.  */
        if (node == parent->larger && parent == gramps->smaller)
        {
            rotate_left(parent, &gramps->smaller);
            new_node = parent;
        }
        else if (node == parent->smaller && parent == gramps->larger)
        {
            rotate_right(parent, &gramps->larger);
            new_node = parent;
        }

        /* If we performed a rotation, then various localvar's values
           need to be rejiggered.  */
        if (new_node != NULL)
        {
            /* NODE moved upwards, but our node of interest (PARENT) is
               one level down; thus, DEPTH remains constant.

               PARENT is now NODE, as we rotated it up. And NODE is now
               the PARENT (stashed into NEW_NODE).
            */               
            parent = node;
            node = new_node;
        }
    }
    /* ### do some PC_DEBUG magic here?  */
    assert(MT_IS_RED(parent));
    assert(MT_IS_RED(node));

    /* insert_case5()  */
    MT_MAKE_BLACK(parent);
    MT_MAKE_RED(gramps);

    /* Note: if GRAMPS is not ROOT, then it must have a parent. Thus,
       parents[depth-3] is valid.  */
    if (gramps == *root)
    {
        rotation_parent = root;
    }
    else if (parents[depth-3]->smaller == gramps)
    {
        rotation_parent = &parents[depth-3]->smaller;
    }
    else
    {
        rotation_parent = &parents[depth-3]->larger;
    }

    /* Rotate PARENT (BLACK) up into GRAMP's position, pushing the
       GRAMPS node down.  */
    if (node == parent->smaller && parent == gramps->smaller)
    {
        rotate_right(gramps, rotation_parent);
    }
    else
    {
        /* node == parent->larger && parent == gramps->larger  */
        rotate_left(gramps, rotation_parent);
    }
}
