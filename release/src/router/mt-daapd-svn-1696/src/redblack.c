static char rcsid[]="$Id: redblack.c 1484 2007-01-17 01:06:16Z rpedde $";

/*
   Redblack balanced tree algorithm
   Copyright (C) Damian Ivereigh 2000

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version. See the file COPYING for details.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Implement the red/black tree structure. It is designed to emulate
** the standard tsearch() stuff. i.e. the calling conventions are
** exactly the same
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stddef.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "daapd.h"
#include "redblack.h"

#define assert(expr)

/* Uncomment this if you would rather use a raw sbrk to get memory
** (however the memory is never released again (only re-used). Can't
** see any point in using this these days.
*/
/* #define USE_SBRK */

enum nodecolour { BLACK, RED };

struct RB_ENTRY(node)
{
        struct RB_ENTRY(node) *left;            /* Left down */
        struct RB_ENTRY(node) *right;           /* Right down */
        struct RB_ENTRY(node) *up;              /* Up */
        enum nodecolour colour;         /* Node colour */
#ifdef RB_INLINE
        RB_ENTRY(data_t) key;           /* User's key (and data) */
#define RB_GET(x,y)             &x->y
#define RB_SET(x,y,v)           x->y = *(v)
#else
        const RB_ENTRY(data_t) *key;    /* Pointer to user's key (and data) */
#define RB_GET(x,y)             x->y
#define RB_SET(x,y,v)           x->y = v
#endif /* RB_INLINE */
};

/* Dummy (sentinel) node, so that we can make X->left->up = X
** We then use this instead of NULL to mean the top or bottom
** end of the rb tree. It is a black node.
**
** Initialization of the last field in this initializer is left implicit
** because it could be of any type.  We count on the compiler to zero it.
*/
struct RB_ENTRY(node) RB_ENTRY(_null)={&RB_ENTRY(_null), &RB_ENTRY(_null), &RB_ENTRY(_null), BLACK};
#define RBNULL (&RB_ENTRY(_null))

#if defined(USE_SBRK)

static struct RB_ENTRY(node) *RB_ENTRY(_alloc)();
static void RB_ENTRY(_free)(struct RB_ENTRY(node) *);

#else

static struct RB_ENTRY(node) *RB_ENTRY(_alloc)() {return (struct RB_ENTRY(node) *) malloc(sizeof(struct RB_ENTRY(node)));}
static void RB_ENTRY(_free)(struct RB_ENTRY(node) *x) {free(x);}

#endif

/* These functions are always needed */
static void RB_ENTRY(_left_rotate)(struct RB_ENTRY(node) **, struct RB_ENTRY(node) *);
static void RB_ENTRY(_right_rotate)(struct RB_ENTRY(node) **, struct RB_ENTRY(node) *);
static struct RB_ENTRY(node) *RB_ENTRY(_successor)(const struct RB_ENTRY(node) *);
static struct RB_ENTRY(node) *RB_ENTRY(_predecessor)(const struct RB_ENTRY(node) *);
static struct RB_ENTRY(node) *RB_ENTRY(_traverse)(int, const RB_ENTRY(data_t) * , struct RB_ENTRY(tree) *);

/* These functions may not be needed */
#ifndef no_lookup
static struct RB_ENTRY(node) *RB_ENTRY(_lookup)(int, const RB_ENTRY(data_t) * , struct RB_ENTRY(tree) *);
#endif

#ifndef no_destroy
static void RB_ENTRY(_destroy)(struct RB_ENTRY(node) *);
#endif

#ifndef no_delete
static void RB_ENTRY(_delete)(struct RB_ENTRY(node) **, struct RB_ENTRY(node) *);
static void RB_ENTRY(_delete_fix)(struct RB_ENTRY(node) **, struct RB_ENTRY(node) *);
#endif

#ifndef no_walk
static void RB_ENTRY(_walk)(const struct RB_ENTRY(node) *, void (*)(const RB_ENTRY(data_t) *, const VISIT, const int, void *), void *, int);
#endif

#ifndef no_readlist
static RBLIST *RB_ENTRY(_openlist)(const struct RB_ENTRY(node) *);
static const RB_ENTRY(data_t) * RB_ENTRY(_readlist)(RBLIST *);
static void RB_ENTRY(_closelist)(RBLIST *);
#endif

/*
** OK here we go, the balanced tree stuff. The algorithm is the
** fairly standard red/black taken from "Introduction to Algorithms"
** by Cormen, Leiserson & Rivest. Maybe one of these days I will
** fully understand all this stuff.
**
** Basically a red/black balanced tree has the following properties:-
** 1) Every node is either red or black (colour is RED or BLACK)
** 2) A leaf (RBNULL pointer) is considered black
** 3) If a node is red then its children are black
** 4) Every path from a node to a leaf contains the same no
**    of black nodes
**
** 3) & 4) above guarantee that the longest path (alternating
** red and black nodes) is only twice as long as the shortest
** path (all black nodes). Thus the tree remains fairly balanced.
*/

/*
 * Initialise a tree. Identifies the comparison routine and any config
 * data that must be sent to it when called.
 * Returns a pointer to the top of the tree.
 */
#ifndef RB_CUSTOMIZE
RB_STATIC struct RB_ENTRY(tree) *
rbinit(int (*cmp)(const void *, const void *, const void *), const void *config)
#else
RB_STATIC struct RB_ENTRY(tree) *RB_ENTRY(init)(void)
#endif /* RB_CUSTOMIZE */
{
        struct RB_ENTRY(tree) *retval;
        char c;

        c=rcsid[0]; /* This does nothing but shutup the -Wall */

        if ((retval=(struct RB_ENTRY(tree) *) malloc(sizeof(struct RB_ENTRY(tree))))==NULL)
                return(NULL);

#ifndef RB_CUSTOMIZE
        retval->rb_cmp=cmp;
        retval->rb_config=config;
#endif /* RB_CUSTOMIZE */
        retval->rb_root=RBNULL;

        return(retval);
}

#ifndef no_destroy
RB_STATIC void
RB_ENTRY(destroy)(struct RB_ENTRY(tree) *rbinfo)
{
        if (rbinfo==NULL)
                return;

        if (rbinfo->rb_root!=RBNULL)
                RB_ENTRY(_destroy)(rbinfo->rb_root);

        free(rbinfo);
}
#endif /* no_destroy */

#ifndef no_search
RB_STATIC const RB_ENTRY(data_t) *
RB_ENTRY(search)(const RB_ENTRY(data_t) *key, struct RB_ENTRY(tree) *rbinfo)
{
        struct RB_ENTRY(node) *x;

        if (rbinfo==NULL)
                return(NULL);

        x=RB_ENTRY(_traverse)(1, key, rbinfo);

        return((x==RBNULL) ? NULL : RB_GET(x, key));
}
#endif /* no_search */

#ifndef no_find
RB_STATIC const RB_ENTRY(data_t) *
RB_ENTRY(find)(const RB_ENTRY(data_t) *key, struct RB_ENTRY(tree) *rbinfo)
{
        struct RB_ENTRY(node) *x;

        if (rbinfo==NULL)
                return(NULL);

        /* If we have a NULL root (empty tree) then just return */
        if (rbinfo->rb_root==RBNULL)
                return(NULL);

        x=RB_ENTRY(_traverse)(0, key, rbinfo);

        return((x==RBNULL) ? NULL : RB_GET(x, key));
}
#endif /* no_find */

#ifndef no_delete
RB_STATIC const RB_ENTRY(data_t) *
RB_ENTRY(delete)(const RB_ENTRY(data_t) *key, struct RB_ENTRY(tree) *rbinfo)
{
        struct RB_ENTRY(node) *x;
        const RB_ENTRY(data_t) * y;

        if (rbinfo==NULL)
                return(NULL);

        x=RB_ENTRY(_traverse)(0, key, rbinfo);

        if (x==RBNULL)
        {
                return(NULL);
        }
        else
        {
                y=RB_GET(x, key);
                RB_ENTRY(_delete)(&rbinfo->rb_root, x);

                return(y);
        }
}
#endif /* no_delete */

#ifndef no_walk
RB_STATIC void
RB_ENTRY(walk)(const struct RB_ENTRY(tree) *rbinfo, void (*action)(const RB_ENTRY(data_t) *, const VISIT, const int, void *), void *arg)
{
        if (rbinfo==NULL)
                return;

        RB_ENTRY(_walk)(rbinfo->rb_root, action, arg, 0);
}
#endif /* no_walk */

#ifndef no_readlist
RB_STATIC RBLIST *
RB_ENTRY(openlist)(const struct RB_ENTRY(tree) *rbinfo)
{
        if (rbinfo==NULL)
                return(NULL);

        return(RB_ENTRY(_openlist)(rbinfo->rb_root));
}

RB_STATIC const RB_ENTRY(data_t) *
RB_ENTRY(readlist)(RBLIST *rblistp)
{
        if (rblistp==NULL)
                return(NULL);

        return(RB_ENTRY(_readlist)(rblistp));
}

RB_STATIC void
RB_ENTRY(closelist)(RBLIST *rblistp)
{
        if (rblistp==NULL)
                return;

        RB_ENTRY(_closelist)(rblistp);
}
#endif /* no_readlist */

#ifndef no_lookup
RB_STATIC const RB_ENTRY(data_t) *
RB_ENTRY(lookup)(int mode, const RB_ENTRY(data_t) *key, struct RB_ENTRY(tree) *rbinfo)
{
        struct RB_ENTRY(node) *x;

        /* If we have a NULL root (empty tree) then just return NULL */
        if (rbinfo==NULL || rbinfo->rb_root==NULL)
                return(NULL);

        x=RB_ENTRY(_lookup)(mode, key, rbinfo);

        return((x==RBNULL) ? NULL : RB_GET(x, key));
}
#endif /* no_lookup */

/* --------------------------------------------------------------------- */

/* Search for and if not found and insert is true, will add a new
** node in. Returns a pointer to the new node, or the node found
*/
static struct RB_ENTRY(node) *
RB_ENTRY(_traverse)(int insert, const RB_ENTRY(data_t) *key, struct RB_ENTRY(tree) *rbinfo)
{
        struct RB_ENTRY(node) *x,*y,*z;
        int cmp;
        int found=0;
        int cmpmods();

        y=RBNULL; /* points to the parent of x */
        x=rbinfo->rb_root;

        /* walk x down the tree */
        while(x!=RBNULL && found==0)
        {
                y=x;
                /* printf("key=%s, RB_GET(x, key)=%s\n", key, RB_GET(x, key)); */
#ifndef RB_CUSTOMIZE
                cmp=RB_CMP(key, RB_GET(x, key), rbinfo->rb_config);
#else
                cmp=RB_CMP(key, RB_GET(x, key));
#endif /* RB_CUSTOMIZE */

                if (cmp<0)
                        x=x->left;
                else if (cmp>0)
                        x=x->right;
                else
                        found=1;
        }

        if (found || !insert)
                return(x);

        if ((z=RB_ENTRY(_alloc)())==NULL)
        {
                /* Whoops, no memory */
                return(RBNULL);
        }

        RB_SET(z, key, key);
        z->up=y;
        if (y==RBNULL)
        {
                rbinfo->rb_root=z;
        }
        else
        {
#ifndef RB_CUSTOMIZE
                cmp=RB_CMP(RB_GET(z, key), RB_GET(y, key), rbinfo->rb_config);
#else
                cmp=RB_CMP(RB_GET(z, key), RB_GET(y, key));
#endif /* RB_CUSTOMIZE */
                if (cmp<0)
                        y->left=z;
                else
                        y->right=z;
        }

        z->left=RBNULL;
        z->right=RBNULL;

        /* colour this new node red */
        z->colour=RED;

        /* Having added a red node, we must now walk back up the tree balancing
        ** it, by a series of rotations and changing of colours
        */
        x=z;

        /* While we are not at the top and our parent node is red
        ** N.B. Since the root node is garanteed black, then we
        ** are also going to stop if we are the child of the root
        */

        while(x != rbinfo->rb_root && (x->up->colour == RED))
        {
                /* if our parent is on the left side of our grandparent */
                if (x->up == x->up->up->left)
                {
                        /* get the right side of our grandparent (uncle?) */
                        y=x->up->up->right;
                        if (y->colour == RED)
                        {
                                /* make our parent black */
                                x->up->colour = BLACK;
                                /* make our uncle black */
                                y->colour = BLACK;
                                /* make our grandparent red */
                                x->up->up->colour = RED;

                                /* now consider our grandparent */
                                x=x->up->up;
                        }
                        else
                        {
                                /* if we are on the right side of our parent */
                                if (x == x->up->right)
                                {
                                        /* Move up to our parent */
                                        x=x->up;
                                        RB_ENTRY(_left_rotate)(&rbinfo->rb_root, x);
                                }

                                /* make our parent black */
                                x->up->colour = BLACK;
                                /* make our grandparent red */
                                x->up->up->colour = RED;
                                /* right rotate our grandparent */
                                RB_ENTRY(_right_rotate)(&rbinfo->rb_root, x->up->up);
                        }
                }
                else
                {
                        /* everything here is the same as above, but
                        ** exchanging left for right
                        */

                        y=x->up->up->left;
                        if (y->colour == RED)
                        {
                                x->up->colour = BLACK;
                                y->colour = BLACK;
                                x->up->up->colour = RED;

                                x=x->up->up;
                        }
                        else
                        {
                                if (x == x->up->left)
                                {
                                        x=x->up;
                                        RB_ENTRY(_right_rotate)(&rbinfo->rb_root, x);
                                }

                                x->up->colour = BLACK;
                                x->up->up->colour = RED;
                                RB_ENTRY(_left_rotate)(&rbinfo->rb_root, x->up->up);
                        }
                }
        }

        /* Set the root node black */
        (rbinfo->rb_root)->colour = BLACK;

        return(z);
}

#ifndef no_lookup
/* Search for a key according to mode (see redblack.h)
*/
static struct RB_ENTRY(node) *
RB_ENTRY(_lookup)(int mode, const RB_ENTRY(data_t) *key, struct RB_ENTRY(tree) *rbinfo)
{
        struct RB_ENTRY(node) *x,*y;
        int cmp=0;
        int found=0;

        y=RBNULL; /* points to the parent of x */
        x=rbinfo->rb_root;

        if (mode==RB_LUFIRST)
        {
                /* Keep going left until we hit a NULL */
                while(x!=RBNULL)
                {
                        y=x;
                        x=x->left;
                }

                return(y);
        }
        else if (mode==RB_LULAST)
        {
                /* Keep going right until we hit a NULL */
                while(x!=RBNULL)
                {
                        y=x;
                        x=x->right;
                }

                return(y);
        }

        /* walk x down the tree */
        while(x!=RBNULL && found==0)
        {
                y=x;
                /* printf("key=%s, RB_GET(x, key)=%s\n", key, RB_GET(x, key)); */
#ifndef RB_CUSTOMIZE
                cmp=RB_CMP(key, RB_GET(x, key), rbinfo->rb_config);
#else
                cmp=RB_CMP(key, RB_GET(x, key));
#endif /* RB_CUSTOMIZE */


                if (cmp<0)
                        x=x->left;
                else if (cmp>0)
                        x=x->right;
                else
                        found=1;
        }

        if (found && (mode==RB_LUEQUAL || mode==RB_LUGTEQ || mode==RB_LULTEQ))
                return(x);

        if (!found && (mode==RB_LUEQUAL || mode==RB_LUNEXT || mode==RB_LUPREV))
                return(RBNULL);

        if (mode==RB_LUGTEQ || (!found && mode==RB_LUGREAT))
        {
                if (cmp>0)
                        return(RB_ENTRY(_successor)(y));
                else
                        return(y);
        }

        if (mode==RB_LULTEQ || (!found && mode==RB_LULESS))
        {
                if (cmp<0)
                        return(RB_ENTRY(_predecessor)(y));
                else
                        return(y);
        }

        if (mode==RB_LUNEXT || (found && mode==RB_LUGREAT))
                return(RB_ENTRY(_successor)(x));

        if (mode==RB_LUPREV || (found && mode==RB_LULESS))
                return(RB_ENTRY(_predecessor)(x));

        /* Shouldn't get here */
        return(RBNULL);
}
#endif /* no_lookup */

#ifndef no_destroy
/*
 * Destroy all the elements blow us in the tree
 * only useful as part of a complete tree destroy.
 */
static void
RB_ENTRY(_destroy)(struct RB_ENTRY(node) *x)
{
        if (x!=RBNULL)
        {
                if (x->left!=RBNULL)
                        RB_ENTRY(_destroy)(x->left);
                if (x->right!=RBNULL)
                        RB_ENTRY(_destroy)(x->right);
                RB_ENTRY(_free)(x);
        }
}
#endif /* no_destroy */

/*
** Rotate our tree thus:-
**
**             X        rb_left_rotate(X)--->            Y
**           /   \                                     /   \
**          A     Y     <---rb_right_rotate(Y)        X     C
**              /   \                               /   \
**             B     C                             A     B
**
** N.B. This does not change the ordering.
**
** We assume that neither X or Y is NULL
*/

static void
RB_ENTRY(_left_rotate)(struct RB_ENTRY(node) **rootp, struct RB_ENTRY(node) *x)
{
        struct RB_ENTRY(node) *y;

        assert(x!=RBNULL);
        assert(x->right!=RBNULL);

        y=x->right; /* set Y */

        /* Turn Y's left subtree into X's right subtree (move B)*/
        x->right = y->left;

        /* If B is not null, set it's parent to be X */
        if (y->left != RBNULL)
                y->left->up = x;

        /* Set Y's parent to be what X's parent was */
        y->up = x->up;

        /* if X was the root */
        if (x->up == RBNULL)
        {
                *rootp=y;
        }
        else
        {
                /* Set X's parent's left or right pointer to be Y */
                if (x == x->up->left)
                {
                        x->up->left=y;
                }
                else
                {
                        x->up->right=y;
                }
        }

        /* Put X on Y's left */
        y->left=x;

        /* Set X's parent to be Y */
        x->up = y;
}

static void
RB_ENTRY(_right_rotate)(struct RB_ENTRY(node) **rootp, struct RB_ENTRY(node) *y)
{
        struct RB_ENTRY(node) *x;

        assert(y!=RBNULL);
        assert(y->left!=RBNULL);

        x=y->left; /* set X */

        /* Turn X's right subtree into Y's left subtree (move B) */
        y->left = x->right;

        /* If B is not null, set it's parent to be Y */
        if (x->right != RBNULL)
                x->right->up = y;

        /* Set X's parent to be what Y's parent was */
        x->up = y->up;

        /* if Y was the root */
        if (y->up == RBNULL)
        {
                *rootp=x;
        }
        else
        {
                /* Set Y's parent's left or right pointer to be X */
                if (y == y->up->left)
                {
                        y->up->left=x;
                }
                else
                {
                        y->up->right=x;
                }
        }

        /* Put Y on X's right */
        x->right=y;

        /* Set Y's parent to be X */
        y->up = x;
}

/* Return a pointer to the smallest key greater than x
*/
static struct RB_ENTRY(node) *
RB_ENTRY(_successor)(const struct RB_ENTRY(node) *x)
{
        struct RB_ENTRY(node) *y;

        if (x->right!=RBNULL)
        {
                /* If right is not NULL then go right one and
                ** then keep going left until we find a node with
                ** no left pointer.
                */
                for (y=x->right; y->left!=RBNULL; y=y->left);
        }
        else
        {
                /* Go up the tree until we get to a node that is on the
                ** left of its parent (or the root) and then return the
                ** parent.
                */
                y=x->up;
                while(y!=RBNULL && x==y->right)
                {
                        x=y;
                        y=y->up;
                }
        }
        return(y);
}

/* Return a pointer to the largest key smaller than x
*/
static struct RB_ENTRY(node) *
RB_ENTRY(_predecessor)(const struct RB_ENTRY(node) *x)
{
        struct RB_ENTRY(node) *y;

        if (x->left!=RBNULL)
        {
                /* If left is not NULL then go left one and
                ** then keep going right until we find a node with
                ** no right pointer.
                */
                for (y=x->left; y->right!=RBNULL; y=y->right);
        }
        else
        {
                /* Go up the tree until we get to a node that is on the
                ** right of its parent (or the root) and then return the
                ** parent.
                */
                y=x->up;
                while(y!=RBNULL && x==y->left)
                {
                        x=y;
                        y=y->up;
                }
        }
        return(y);
}

#ifndef no_delete
/* Delete the node z, and free up the space
*/
static void
RB_ENTRY(_delete)(struct RB_ENTRY(node) **rootp, struct RB_ENTRY(node) *z)
{
        struct RB_ENTRY(node) *x, *y;

        if (z->left == RBNULL || z->right == RBNULL)
                y=z;
        else
                y=RB_ENTRY(_successor)(z);

        if (y->left != RBNULL)
                x=y->left;
        else
                x=y->right;

        x->up = y->up;

        if (y->up == RBNULL)
        {
                *rootp=x;
        }
        else
        {
                if (y==y->up->left)
                        y->up->left = x;
                else
                        y->up->right = x;
        }

        if (y!=z)
        {
                RB_SET(z, key, RB_GET(y, key));
        }

        if (y->colour == BLACK)
                RB_ENTRY(_delete_fix)(rootp, x);

        RB_ENTRY(_free)(y);
}

/* Restore the reb-black properties after a delete */
static void
RB_ENTRY(_delete_fix)(struct RB_ENTRY(node) **rootp, struct RB_ENTRY(node) *x)
{
        struct RB_ENTRY(node) *w;

        while (x!=*rootp && x->colour==BLACK)
        {
                if (x==x->up->left)
                {
                        w=x->up->right;
                        if (w->colour==RED)
                        {
                                w->colour=BLACK;
                                x->up->colour=RED;
                                rb_left_rotate(rootp, x->up);
                                w=x->up->right;
                        }

                        if (w->left->colour==BLACK && w->right->colour==BLACK)
                        {
                                w->colour=RED;
                                x=x->up;
                        }
                        else
                        {
                                if (w->right->colour == BLACK)
                                {
                                        w->left->colour=BLACK;
                                        w->colour=RED;
                                        RB_ENTRY(_right_rotate)(rootp, w);
                                        w=x->up->right;
                                }


                                w->colour=x->up->colour;
                                x->up->colour = BLACK;
                                w->right->colour = BLACK;
                                RB_ENTRY(_left_rotate)(rootp, x->up);
                                x=*rootp;
                        }
                }
                else
                {
                        w=x->up->left;
                        if (w->colour==RED)
                        {
                                w->colour=BLACK;
                                x->up->colour=RED;
                                RB_ENTRY(_right_rotate)(rootp, x->up);
                                w=x->up->left;
                        }

                        if (w->right->colour==BLACK && w->left->colour==BLACK)
                        {
                                w->colour=RED;
                                x=x->up;
                        }
                        else
                        {
                                if (w->left->colour == BLACK)
                                {
                                        w->right->colour=BLACK;
                                        w->colour=RED;
                                        RB_ENTRY(_left_rotate)(rootp, w);
                                        w=x->up->left;
                                }

                                w->colour=x->up->colour;
                                x->up->colour = BLACK;
                                w->left->colour = BLACK;
                                RB_ENTRY(_right_rotate)(rootp, x->up);
                                x=*rootp;
                        }
                }
        }

        x->colour=BLACK;
}
#endif /* no_delete */

#ifndef no_walk
static void
RB_ENTRY(_walk)(const struct RB_ENTRY(node) *x, void (*action)(const RB_ENTRY(data_t) *, const VISIT, const int, void *), void *arg, int level)
{
        if (x==RBNULL)
                return;

        if (x->left==RBNULL && x->right==RBNULL)
        {
                /* leaf */
                (*action)(RB_GET(x, key), leaf, level, arg);
        }
        else
        {
                (*action)(RB_GET(x, key), preorder, level, arg);

                RB_ENTRY(_walk)(x->left, action, arg, level+1);

                (*action)(RB_GET(x, key), postorder, level, arg);

                RB_ENTRY(_walk)(x->right, action, arg, level+1);

                (*action)(RB_GET(x, key), endorder, level, arg);
        }
}
#endif /* no_walk */

#ifndef no_readlist
static RBLIST *
RB_ENTRY(_openlist)(const struct RB_ENTRY(node) *rootp)
{
        RBLIST *rblistp;

        rblistp=(RBLIST *) malloc(sizeof(RBLIST));
        if (!rblistp)
                return(NULL);

        rblistp->rootp=rootp;
        rblistp->nextp=rootp;

        if (rootp!=RBNULL)
        {
                while(rblistp->nextp->left!=RBNULL)
                {
                        rblistp->nextp=rblistp->nextp->left;
                }
        }

        return(rblistp);
}

static const RB_ENTRY(data_t) *
RB_ENTRY(_readlist)(RBLIST *rblistp)
{
        const RB_ENTRY(data_t) *key=NULL;

        if (rblistp!=NULL && rblistp->nextp!=RBNULL)
        {
                key=RB_GET(rblistp->nextp, key);
                rblistp->nextp=RB_ENTRY(_successor)(rblistp->nextp);
        }

        return(key);
}

static void
rb_closelist(RBLIST *rblistp)
{
        if (rblistp)
                free(rblistp);
}
#endif /* no_readlist */

#if defined(RB_USE_SBRK)
/* Allocate space for our nodes, allowing us to get space from
** sbrk in larger chucks.
*/
static struct RB_ENTRY(node) *rbfreep=NULL;

#define RB_ENTRY(NODE)ALLOC_CHUNK_SIZE 1000
static struct RB_ENTRY(node) *
RB_ENTRY(_alloc)()
{
        struct RB_ENTRY(node) *x;
        int i;

        if (rbfreep==NULL)
        {
                /* must grab some more space */
                rbfreep=(struct RB_ENTRY(node) *) sbrk(sizeof(struct RB_ENTRY(node)) * RB_ENTRY(NODE)ALLOC_CHUNK_SIZE);

                if (rbfreep==(struct RB_ENTRY(node) *) -1)
                {
                        return(NULL);
                }

                /* tie them together in a linked list (use the up pointer) */
                for (i=0, x=rbfreep; i<RB_ENTRY(NODE)ALLOC_CHUNK_SIZE-1; i++, x++)
                {
                        x->up = (x+1);
                }
                x->up=NULL;
        }

        x=rbfreep;
        rbfreep = rbfreep->up;
#ifdef RB_ALLOC
        RB_ALLOC(ACCESS(x, key));
#endif /* RB_ALLOC */
        return(x);
}

/* free (dealloc) an RB_ENTRY(node) structure - add it onto the front of the list
** N.B. RB_ENTRY(node) need not have been allocated through rb_alloc()
*/
static void
RB_ENTRY(_free)(struct RB_ENTRY(node) *x)
{
#ifdef RB_FREE
        RB_FREE(ACCESS(x, key));
#endif /* RB_FREE */
        x->up=rbfreep;
        rbfreep=x;
}

#endif

#if 0
int
RB_ENTRY(_check)(struct RB_ENTRY(node) *rootp)
{
        if (rootp==NULL || rootp==RBNULL)
                return(0);

        if (rootp->up!=RBNULL)
        {
                fprintf(stderr, "Root up pointer not RBNULL");
                dumptree(rootp, 0);
                return(1);
        }

        if (RB_ENTRY(_check)1(rootp))
        {
                RB_ENTRY(dumptree)(rootp, 0);
                return(1);
        }

        if (RB_ENTRY(count_black)(rootp)==-1)
        {
                RB_ENTRY(dumptree)(rootp, 0);
                return(-1);
        }

        return(0);
}

int
RB_ENTRY(_check1)(struct RB_ENTRY(node) *x)
{
        if (x->left==NULL || x->right==NULL)
        {
                fprintf(stderr, "Left or right is NULL");
                return(1);
        }

        if (x->colour==RED)
        {
                if (x->left->colour!=BLACK && x->right->colour!=BLACK)
                {
                        fprintf(stderr, "Children of red node not both black, x=%ld", x);
                        return(1);
                }
        }

        if (x->left != RBNULL)
        {
                if (x->left->up != x)
                {
                        fprintf(stderr, "x->left->up != x, x=%ld", x);
                        return(1);
                }

                if (rb_check1(x->left))
                        return(1);
        }

        if (x->right != RBNULL)
        {
                if (x->right->up != x)
                {
                        fprintf(stderr, "x->right->up != x, x=%ld", x);
                        return(1);
                }

                if (rb_check1(x->right))
                        return(1);
        }
        return(0);
}

RB_ENTRY(count_black)(struct RB_ENTRY(node) *x)
{
        int nleft, nright;

        if (x==RBNULL)
                return(1);

        nleft=RB_ENTRY(count_black)(x->left);
        nright=RB_ENTRY(count_black)(x->right);

        if (nleft==-1 || nright==-1)
                return(-1);

        if (nleft != nright)
        {
                fprintf(stderr, "Black count not equal on left & right, x=%ld", x);
                return(-1);
        }

        if (x->colour == BLACK)
        {
                nleft++;
        }

        return(nleft);
}

RB_ENTRY(dumptree)(struct RB_ENTRY(node) *x, int n)
{
        char *prkey();

        if (x!=NULL && x!=RBNULL)
        {
                n++;
                fprintf(stderr, "Tree: %*s %ld: left=%ld, right=%ld, colour=%s, key=%s",
                        n,
                        "",
                        x,
                        x->left,
                        x->right,
                        (x->colour==BLACK) ? "BLACK" : "RED",
                        prkey(RB_GET(x, key)));

                RB_ENTRY(dumptree)(x->left, n);
                RB_ENTRY(dumptree)(x->right, n);
        }
}
#endif

/*
 * $Log$
 * Revision 1.4  2006/02/26 08:46:24  rpedde
 * Merged win32-branch
 *
 * Revision 1.3.2.2  2006/02/26 08:28:35  rpedde
 * unix fixes from win32 port
 *
 * Revision 1.3.2.1  2006/02/23 03:19:40  rpedde
 * First pass at win32 port.
 *
 * Revision 1.3  2005/05/21 05:56:09  rpedde
 * for quick translation from itunes song id to mt-daapd song id
 *
 * Revision 1.1  2004/03/13 23:43:02  rpedde
 * Add Damian Ivereigh's redblack tree implementation to speed lookups
 *
 * Revision 1.9  2003/10/24 01:31:21  damo
 * Patches from Eric Raymond: %prefix is implemented.  Various other small
 * changes avoid stepping on global namespaces and improve the documentation.
 *
 * Revision 1.8  2002/08/26 05:33:47  damo
 * Some minor fixes:-
 * Stopped ./configure warning about stuff being in the wrong order
 * Fixed compiler warning about const (not sure about this)
 * Changed directory of redblack.c in documentation
 *
 * Revision 1.7  2002/08/26 03:11:40  damo
 * Fixed up a bunch of compiler warnings when compiling example4
 *
 * Tidies up the Makefile.am & Specfile.
 *
 * Renamed redblack to rbgen
 *
 * Revision 1.6  2002/08/26 01:03:35  damo
 * Patch from Eric Raymond to change the way the library is used:-
 *
 * Eric's idea is to convert libredblack into a piece of in-line code
 * generated by another program. This should be faster, smaller and easier
 * to use.
 *
 * This is the first check-in of his code before I start futzing with it!
 *
 * Revision 1.5  2002/01/30 07:54:53  damo
 * Fixed up the libtool versioning stuff (finally)
 * Fixed bug 500600 (not detecting a NULL return from malloc)
 * Fixed bug 509485 (no longer needs search.h)
 * Cleaned up debugging section
 * Allow multiple inclusions of redblack.h
 * Thanks to Matthias Andree for reporting (and fixing) these
 *
 * Revision 1.4  2000/06/06 14:43:43  damo
 * Added all the rbwalk & rbopenlist stuff. Fixed up malloc instead of sbrk.
 * Added two new examples
 *
 * Revision 1.3  2000/05/24 06:45:27  damo
 * Converted everything over to using const
 * Added a new example1.c file to demonstrate the worst case scenario
 * Minor fixups of the spec file
 *
 * Revision 1.2  2000/05/24 06:17:10  damo
 * Fixed up the License (now the LGPL)
 *
 * Revision 1.1  2000/05/24 04:15:53  damo
 * Initial import of files. Versions are now all over the place. Oh well
 *
 */

