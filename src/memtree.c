/*
  memtree.c :  a red-black tree for use by the memory subsystem

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
  For further information, see:

    http://en.wikipedia.org/wiki/Red-black_tree
    http://www.upgrade-cepis.org/issues/2004/5/up5-5Mosaic.pdf


  It is important to note that the nodes in this tree represent the
  free memory. We cannot copy node information from one to the next.
  Thus, node deletion requires manipulation of the tree instead of
  simply moving values around (as described by most documentation of
  the red-black tree algorithm).
*/

#include <assert.h>
#include <stdio.h>

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

#define MT_IS_BLACK_NULL(m)  ((m) == NULL || ((m)->b.size & 1) == 0)

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
    if (MT_PARENT(parents, depth) == gramps->smaller)
        return gramps->larger;
    return gramps->smaller;
}


/* See: http://en.wikipedia.org/wiki/Tree_rotation

   OLD_ROOT will be pushed down, and NEW_ROOT lifted up. Pass a reference
   the the parent's link to OLD_ROOT so that we can update it.  */
/* ### inline this  */
static void
rotate_left(struct pc_memtree_s *new_root, struct pc_memtree_s **old_root)
{
    (*old_root)->larger = new_root->smaller;
    new_root->smaller = *old_root;
    *old_root = new_root;
}


/* See: http://en.wikipedia.org/wiki/Tree_rotation

   OLD_ROOT will be pushed down, and NEW_ROOT lifted up. Pass a reference
   the the parent's link to OLD_ROOT so that we can update it.  */
/* ### inline this  */
static void
rotate_right(struct pc_memtree_s *new_root, struct pc_memtree_s **old_root)
{
    (*old_root)->smaller = new_root->larger;
    new_root->larger = *old_root;
    *old_root = new_root;
}


/* Find the link in the parent of NODE which refers to NODE. This may be
   the ROOT of the tree.

   Note that we could look up NODE from the array, but the caller has
   already done that. Pushing that value might cost about the same as
   looking it up, but if this function gets inlined, then the push cost
   is obviated.

   NOTE: we depend upon this behavior when we remove TARGET from the tree
   during deletion. It's parent has a stale link, so we need to look for
   the stale value, rather than what is at parents[depth].

   NOTE: we also depend upon this behavior sometimes to look for the
   SIBLING rather than the target of the operation.  */
/* ### inline this  */
static struct pc_memtree_s **
get_reference(struct pc_memtree_s *parents[],
              int depth,
              const struct pc_memtree_s *node,
              struct pc_memtree_s **root)
{
    struct pc_memtree_s *parent;

    parent = MT_PARENT(parents, depth);
    if (parent == NULL)
        return root;
    if (parent->smaller == node)
        return &parent->smaller;
    return &parent->larger;
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

        if (MT_SIZE(scan) == size)
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

    /* NODE is one level below SCAN.  */
    ++depth;

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
            node = gramps;
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
            rotate_left(node, &gramps->smaller);
            new_node = parent;
        }
        else if (node == parent->smaller && parent == gramps->larger)
        {
            rotate_right(node, &gramps->larger);
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

    /* Get a reference to whatever is pointing at GRAMPS so that we can
       update it to point to PARENT as it gets rotated upwards.  */
    rotation_parent = get_reference(parents, depth - 2, gramps, root);

    /* Rotate PARENT (BLACK) up into GRAMP's position, pushing the
       GRAMPS node down.  */
    if (node == parent->smaller && parent == gramps->smaller)
    {
        rotate_right(parent, rotation_parent);
    }
    else
    {
        /* node == parent->larger && parent == gramps->larger  */
        rotate_left(parent, rotation_parent);
    }
}


struct pc_block_s *
pc__memtree_fetch(struct pc_memtree_s **root, size_t size)
{
    struct pc_memtree_s *parents[MT_DEPTH];
    int depth;
    int larger_depth;
    struct pc_memtree_s *scan;
    struct pc_memtree_s *target;
    struct pc_memtree_s *child;
    struct pc_memtree_s *parent;
    struct pc_memtree_s *sibling;
    struct pc_memtree_s **rotation_parent;
    pc_bool_t target_is_red;

    /* We have no nodes that will fit the requested size.  */
    if (*root == NULL)
        return NULL;

    /* We need to scan the tree to find the node which is *larger* than
       or equal to SIZE, but is as close to it as possible.

       If a given node is larger than SIZE, then we *may* find a node
       closer to SIZE on its SMALLER branch. Of course, all those nodes
       may be smaller than SIZE.

       If a given node is smaller than SIZE, then we *may* find a node
       closer to SIZE on its LARGER branch. Again, we may never reach
       a node that is larger than SIZE.

       Each move down the tree, choosing one of the branches will scan
       over nodes that get progressively closer to SIZE. We want to
       remember the one that was equal or larger.

       When we reach bottom, that node is the PREDECESSOR of our desired
       node. We will (effectively) move it "up" to take the place of the
       target node, and then run the deletion operation on that bottom
       node.  */

    depth = 0;
    larger_depth = -1;
    scan = *root;

    /* We'll hit the bottom of the tree at some point, so no conditional
       is needed on this scanning loop.  */
    while (TRUE)
    {
        /* Remember this for when we need parental information, since the
           nodes don't track it themselves.  */
        parents[depth] = scan;

        if (size <= scan->b.size)
        {
            larger_depth = depth;

            /* If there are no smaller nodes (closer to SIZE), we're done.
               Even if this node is *equal* to SIZE, we still continue the
               scsan since we're looking for the predecessor node.  */
            if (scan->smaller == NULL)
                break;

            /* Scan down the smaller-sized branch.  */
            scan = scan->smaller;
        }
        else
        {
            /* If there are no larger nodes (closer to SIZE), we're done.  */
            if (scan->larger == NULL)
                break;

            /* Scan down the larger-sized branch.  */
            scan = scan->larger;
        }

        /* We're moving deeper.  */
        ++depth;
    }

    /* We never saw a node with a sufficient size. Ah well.  */
    if (larger_depth == -1)
        return NULL;

    /* If SCAN has sufficient size, then it is the target of our operation.
       It will provide the needed memory, and may be deleted.

       Otherwise, the node at LARGER_DEPTH is the closest to SIZE and will
       be our target.  */
    if (size <= scan->b.size)
        target = scan;
    else
        target = parents[larger_depth];

    /* If there are extra blocks hanging off this node, then we simply
       unlink one and return it. No further tree manipulations are needed.  */
    if (target->b.next != NULL)
    {
        struct pc_block_s *result = target->b.next;

        target->b.next = result->next;
        result->next = NULL;
        return result;
    }
    /* We need to return this node as the block. Remove it from the tree.  */

    /* TARGET is what we want to delete from the tree.
       SCAN is its predecessor.  */

    /* This algorithm comes right off the Wikipedia article.  */

    /* If TARGET has two children, then we need to "swap" it with SCAN
       since the algorithm requires, at most, one child. We know that
       SCAN has no more than one child, since it terminated the search
       with a NULL link.  */
    if (target->smaller != NULL && target->larger != NULL)
    {
        int target_color_flag = MT_IS_RED(target);

        /* We are swapping data between TARGET and SCAN, but NOT the color.
           Get the appropriate color for the TARGET of deletion.  */
        target_is_red = MT_IS_RED(scan);

        /* SCAN will be at the old TARGET's position, and should assume its
           color.  */
        scan->b.size = MT_SIZE(scan) | target_color_flag;

        /* The parent should point to SCAN now, not TARGET.  */
        *get_reference(parents, larger_depth, target, root) = scan;

        /* Remember the at-most one child (may be NULL) of the predecessor.  */
        child = scan->smaller ? scan->smaller : scan->larger;

        /* We need to be careful if TARGET is the parent of SCAN.  */
        if (larger_depth == depth - 1)
        {
            if (target->smaller == scan)
            {
                scan->smaller = child;
                scan->larger = target->larger;
            }
            else
            {
                scan->smaller = target->smaller;
                scan->larger = child;
            }
        }
        else
        {
            /* Copy over the links.  */
            scan->smaller = target->smaller;
            scan->larger = target->larger;

            /* The parent that was referring to SCAN should now reference
               the CHILD node.  */
            *get_reference(parents, depth, scan, root) = child;
        }

        /* Adjust the set of remembered parents.  */
        parents[larger_depth] = scan;
    }
    else
    {
        /* What color is TARGET?  */
        target_is_red = MT_IS_RED(target);

        if (target->smaller != NULL)
            child = target->smaller;
        else
            child = target->larger;

        /* If the target is not SCAN, then it is somewhere higher in the
           tree. Reset the depth.  */
        if (target != scan)
            depth = larger_depth;

        /* The parent that was referring to TARGET should now refer
           to CHILD.  */
        *get_reference(parents, depth, target, root) = child;
    }

    /* Update the set of PARENTS to reflect CHILD moving up a level.  */
    parents[depth] = child;

    /* Fix up TARGET's size, so we don't return something slightly off
       because it was marked RED.  */
    MT_MAKE_BLACK(target);

    /* delete_one_child()  */
    if (target_is_red)
    {
        /* The node we're removing is marked RED. This could be somewhere
           up in the tree, or way down as a leaf.  */
        return &target->b;
    }
    if (child != NULL && MT_IS_RED(child))
    {
        MT_MAKE_BLACK(child);
        return &target->b;
    }
    /* We are now "deleting" (fixing tree state of) the CHILD node. At this
       point, we know that CHILD is BLACK (or NULL, which implies BLACK).  */

    /* delete_case1()  */
  delete_case1:
    /* CHILD has a one-fewer BLACK path through it. We need to rebalance
       to account for this now-missing BLACK.  */

    if (depth == 0)
        return &target->b;

    /* delete_case2()  */
    parent = MT_PARENT(parents, depth);
    if (parent->smaller == child)
        sibling = parent->larger;
    else
        sibling = parent->smaller;

    /* TARGET and CHILD were BLACK. Thus, SIBLING's side of PARENT must
       have two BLACK's, which is impossible if SIBLING is NULL. Note that
       this also holds true if we restarted case1 from the 'goto' below.  */
    /* ### PC_DEBUG stuff instead?  */
    assert(sibling != NULL);

    if (MT_IS_RED(sibling))
    {
        struct pc_memtree_s *new_sibling;

        /* Since SIBLING was RED, then PARENT must be BLACK.  */
        MT_MAKE_RED(parent);
        MT_MAKE_BLACK(sibling);

        rotation_parent = get_reference(parents, depth - 1, parent, root);
        if (parent->smaller == child)
        {
            new_sibling = sibling->smaller;
            rotate_left(sibling, rotation_parent);
        }
        else
        {
            new_sibling = sibling->larger;
            rotate_right(sibling, rotation_parent);
        }

        /* CHILD was moved further down the tree.  */
        parents[depth - 1] = sibling;
        parents[depth] = parent;
        parents[++depth] = child;

        /* We have the same parent, but a new sibling.  */
        sibling = new_sibling;

        /* PARENT, TARGET, and CHILD were all BLACK. Thus, each path through
           SIBLING must contain (at least) 3 BLACK nodes. That would be
           PARENT, one child oF SIBLING (such as NEW_SIBLING), and one
           grandchild of SIBLING. For a grandchild to exist, the child can
           not be NULL.  */
        /* ### PC_DEBUG?  */
        assert(sibling != NULL);

        /* We can skip delete_case3() because PARENT is now RED.  */
        goto delete_case4;
    }

    /* delete_case3()  */
    if (MT_IS_BLACK(parent)
        && MT_IS_BLACK(sibling)
        && MT_IS_BLACK_NULL(sibling->smaller)
        && MT_IS_BLACK_NULL(sibling->larger))
    {
        MT_MAKE_RED(sibling);

        /* Rebalance starting at PARENT.

           Note in delete_case2(), we talk about TARGET and CHILD both
           being black, thus assuring SIBLING will exist. At this point,
           both CHILD and PARENT are black (2 blacks), so as we move up,
           the new sibling must also provide 2 blacks; thus, it must not
           be a leaf (NULL) node.  */
        child = parent;
        --depth;
        goto delete_case1;
    }

    /* delete_case4()  */
  delete_case4:
    /* We know that SIBLING is not NULL.  */

    if (MT_IS_RED(parent)
        && MT_IS_BLACK(sibling)
        && MT_IS_BLACK_NULL(sibling->smaller)
        && MT_IS_BLACK_NULL(sibling->larger))
    {
        MT_MAKE_RED(sibling);
        MT_MAKE_BLACK(parent);

        return &target->b;
    }

    /* delete_case5()  */
    /* This is BLACK because we have one or two RED children (see below).  */
    assert(MT_IS_BLACK(sibling));
    {
        rotation_parent = get_reference(parents, depth, sibling, root);

        /* case3 (BLACK PARENT) and case4 (RED PARENT) eliminated the
           BLACK/BLACK children cases. Thus, if we have reached case5,
           then we have RED/BLACK, BLACK/RED, or RED/RED children.  */
        if (parent->smaller == child
            && MT_IS_BLACK_NULL(sibling->larger))
        {
            struct pc_memtree_s *new_sibling = sibling->smaller;

            /* RED/BLACK  */
            assert(MT_IS_RED(new_sibling));

            MT_MAKE_RED(sibling);
            MT_MAKE_BLACK(new_sibling);

            rotate_right(new_sibling, rotation_parent);
            sibling = new_sibling;
        }
        else if (parent->larger == child
                 && MT_IS_BLACK_NULL(sibling->smaller))
        {
            struct pc_memtree_s *new_sibling = sibling->larger;

            /* BLACK/RED  */
            assert(MT_IS_RED(new_sibling));

            MT_MAKE_RED(sibling);
            MT_MAKE_BLACK(new_sibling);

            rotate_left(new_sibling, rotation_parent);
            sibling = new_sibling;
        }

        /* PARENT and PARENTS remain the same. SIBLING may have been updated.

           Since the (new) SIBLING was RED, then it could not have been
           a NULL node.

           If no rotation has occurred, then SIBLING is the same as before,
           which we know is not-NULL.  */
        /* ### PC_DEBUG?  */
        assert(sibling != NULL);
    }

    /* One of SIBLING's children is RED (we no longer care about the other
       child's color), and has been moved to the "correct" child to prepare
       for case6.  */

    /* delete_case6()  */
    if (MT_IS_BLACK(parent))
    {
        MT_MAKE_BLACK(sibling);
    }
    else
    {
        MT_MAKE_RED(sibling);
        MT_MAKE_BLACK(parent);
    }
    rotation_parent = get_reference(parents, depth - 1, parent, root);
    if (parent->smaller == child)
    {
        MT_MAKE_BLACK(sibling->larger);
        rotate_left(sibling, rotation_parent);
    }
    else
    {
        MT_MAKE_BLACK(sibling->smaller);
        rotate_right(sibling, rotation_parent);
    }

    /* No need to update PARENT, SIBLING, PARENTS after the above rotations
       since we now exit.  */

    return &target->b;
}


#ifdef PC_DEBUG

/* Return the BLACK depth of ROOT. For this function, a NULL node is
   considered to be BLACK (thus, validating property 3 -- all leaves are
   BLACK). Thus, an empty tree has depth 1, and a tree with a single
   node is depth 2.

   This function also validates property 4 (children of every RED node
   is BLACK), and property 5 (all paths have the same BLACK depth).

   Property 1 (all nodes are RED or BLACK) is implied by the implementation,
   so it cannot be "validated".

   Property 2 (the root is BLACK) is not validated. The recursive nature
   of this function cannot determine whether it is looking at the root
   of the tree. In practice, the root does not have to be BLACK since a
   simple recoloring will not affect any of the properties/invariants.  */
int
pc__memtree_depth(const struct pc_memtree_s *node)
{
    int depth;

    if (node == NULL)
        return 1;

    if (MT_IS_RED(node))
    {
        if (node->smaller == NULL)
        {
            assert(node->larger == NULL);
            return 1;
        }
        assert(node->larger != NULL);

        assert(MT_IS_BLACK(node->smaller));
        assert(MT_IS_BLACK(node->larger));

        depth = pc__memtree_depth(node->smaller);
        assert(pc__memtree_depth(node->larger) == depth);
        return depth;
    }

    if (node->smaller == NULL)
    {
        if (node->larger != NULL)
            assert(pc__memtree_depth(node->larger) == 1);
        return 2;
    }
    if (node->larger == NULL)
    {
        assert(pc__memtree_depth(node->smaller) == 1);
        return 2;
    }

    depth = pc__memtree_depth(node->smaller);
    assert(pc__memtree_depth(node->larger) == depth);
    return depth + 1;
}


#define MT_COLOR(n) (MT_IS_BLACK(n) ? "BLACK" : "RED")

static void
print_node(const struct pc_memtree_s *node, int depth)
{
    int i;

    /* Whoops.  */
    if (depth >= MT_DEPTH)
    {
        printf("=== LOOP DETECTED\n");
        return;
    }

    for (i = depth; i--; )
        printf(". ");

    if (node == NULL)
    {
        printf("null\n");
        return;
    }
    printf("%s:%lu\n", MT_COLOR(node), (unsigned long)MT_SIZE(node));

    print_node(node->smaller, ++depth);
    print_node(node->larger, depth);
}


void
pc__memtree_print(const struct pc_memtree_s *root)
{
    print_node(root, 0);
}

#endif /* PC_DEBUG  */
