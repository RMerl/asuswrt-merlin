/* $Id: rbtree.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pj/rbtree.h>
#include <pj/os.h>

static void left_rotate( pj_rbtree *tree, pj_rbtree_node *node ) 
{
    pj_rbtree_node *rnode, *parent;

    PJ_CHECK_STACK();

    rnode = node->right;
    if (rnode == tree->null)
        return;
    
    node->right = rnode->left;
    if (rnode->left != tree->null)
        rnode->left->parent = node;
    parent = node->parent;
    rnode->parent = parent;
    if (parent != tree->null) {
        if (parent->left == node)
	   parent->left = rnode;
        else
	   parent->right = rnode;
    } else {
        tree->root = rnode;
    }
    rnode->left = node;
    node->parent = rnode;
}

static void right_rotate( pj_rbtree *tree, pj_rbtree_node *node ) 
{
    pj_rbtree_node *lnode, *parent;

    PJ_CHECK_STACK();

    lnode = node->left;
    if (lnode == tree->null)
        return;

    node->left = lnode->right;
    if (lnode->right != tree->null)
	lnode->right->parent = node;
    parent = node->parent;
    lnode->parent = parent;

    if (parent != tree->null) {
        if (parent->left == node)
	    parent->left = lnode;
	else
	    parent->right = lnode;
    } else {
        tree->root = lnode;
    }
    lnode->right = node;
    node->parent = lnode;
}

static void insert_fixup( pj_rbtree *tree, pj_rbtree_node *node ) 
{
    pj_rbtree_node *temp, *parent;

    PJ_CHECK_STACK();

    while (node != tree->root && node->parent->color == PJ_RBCOLOR_RED) {
        parent = node->parent;
        if (parent == parent->parent->left) {
	    temp = parent->parent->right;
	    if (temp->color == PJ_RBCOLOR_RED) {
	        temp->color = PJ_RBCOLOR_BLACK;
	        node = parent;
	        node->color = PJ_RBCOLOR_BLACK;
	        node = node->parent;
	        node->color = PJ_RBCOLOR_RED;
	    } else {
	        if (node == parent->right) {
		   node = parent;
		   left_rotate(tree, node);
	        }
	        temp = node->parent;
	        temp->color = PJ_RBCOLOR_BLACK;
	        temp = temp->parent;
	        temp->color = PJ_RBCOLOR_RED;
	        right_rotate( tree, temp);
	    }
        } else {
	    temp = parent->parent->left;
	    if (temp->color == PJ_RBCOLOR_RED) {
	        temp->color = PJ_RBCOLOR_BLACK;
	        node = parent;
	        node->color = PJ_RBCOLOR_BLACK;
	        node = node->parent;
	        node->color = PJ_RBCOLOR_RED;
	    } else {
	        if (node == parent->left) {
		    node = parent;
		    right_rotate(tree, node);
	        }
	        temp = node->parent;
	        temp->color = PJ_RBCOLOR_BLACK;
	        temp = temp->parent;
	        temp->color = PJ_RBCOLOR_RED;
	        left_rotate(tree, temp);
	   }
        }
    }
	
    tree->root->color = PJ_RBCOLOR_BLACK;
}


static void delete_fixup( pj_rbtree *tree, pj_rbtree_node *node )
{
    pj_rbtree_node *temp;

    PJ_CHECK_STACK();

    while (node != tree->root && node->color == PJ_RBCOLOR_BLACK) {
        if (node->parent->left == node) {
	    temp = node->parent->right;
	    if (temp->color == PJ_RBCOLOR_RED) {
	        temp->color = PJ_RBCOLOR_BLACK;
	        node->parent->color = PJ_RBCOLOR_RED;
	        left_rotate(tree, node->parent);
	        temp = node->parent->right;
	    }
	    if (temp->left->color == PJ_RBCOLOR_BLACK && 
	        temp->right->color == PJ_RBCOLOR_BLACK) 
	    {
	        temp->color = PJ_RBCOLOR_RED;
	        node = node->parent;
	    } else {
	        if (temp->right->color == PJ_RBCOLOR_BLACK) {
		    temp->left->color = PJ_RBCOLOR_BLACK;
		    temp->color = PJ_RBCOLOR_RED;
		    right_rotate( tree, temp);
		    temp = node->parent->right;
	        }
	        temp->color = node->parent->color;
	        temp->right->color = PJ_RBCOLOR_BLACK;
	        node->parent->color = PJ_RBCOLOR_BLACK;
	        left_rotate(tree, node->parent);
	        node = tree->root;
	    }
        } else {
	    temp = node->parent->left;
	    if (temp->color == PJ_RBCOLOR_RED) {
	        temp->color = PJ_RBCOLOR_BLACK;
	        node->parent->color = PJ_RBCOLOR_RED;
	        right_rotate( tree, node->parent);
	        temp = node->parent->left;
	    }
	    if (temp->right->color == PJ_RBCOLOR_BLACK && 
		temp->left->color == PJ_RBCOLOR_BLACK) 
	    {
	        temp->color = PJ_RBCOLOR_RED;
	        node = node->parent;
	    } else {
	        if (temp->left->color == PJ_RBCOLOR_BLACK) {
		    temp->right->color = PJ_RBCOLOR_BLACK;
		    temp->color = PJ_RBCOLOR_RED;
		    left_rotate( tree, temp);
		    temp = node->parent->left;
	        }
	        temp->color = node->parent->color;
	        node->parent->color = PJ_RBCOLOR_BLACK;
	        temp->left->color = PJ_RBCOLOR_BLACK;
	        right_rotate(tree, node->parent);
	        node = tree->root;
	    }
        }
    }
	
    node->color = PJ_RBCOLOR_BLACK;
}


PJ_DEF(void) pj_rbtree_init( pj_rbtree *tree, pj_rbtree_comp *comp )
{
    PJ_CHECK_STACK();

    tree->null = tree->root = &tree->null_node;
    tree->null->key = NULL;
    tree->null->user_data = NULL;
    tree->size = 0;
    tree->null->left = tree->null->right = tree->null->parent = tree->null;
    tree->null->color = PJ_RBCOLOR_BLACK;
    tree->comp = comp;
}

PJ_DEF(pj_rbtree_node*) pj_rbtree_first( pj_rbtree *tree )
{
    register pj_rbtree_node *node = tree->root;
    register pj_rbtree_node *null = tree->null;
    
    PJ_CHECK_STACK();

    while (node->left != null)
	node = node->left;
    return node != null ? node : NULL;
}

PJ_DEF(pj_rbtree_node*) pj_rbtree_last( pj_rbtree *tree )
{
    register pj_rbtree_node *node = tree->root;
    register pj_rbtree_node *null = tree->null;
    
    PJ_CHECK_STACK();

    while (node->right != null)
	node = node->right;
    return node != null ? node : NULL;
}

PJ_DEF(pj_rbtree_node*) pj_rbtree_next( pj_rbtree *tree, 
					register pj_rbtree_node *node )
{
    register pj_rbtree_node *null = tree->null;
    
    PJ_CHECK_STACK();

    if (node->right != null) {
	for (node=node->right; node->left!=null; node = node->left)
	    /* void */;
    } else {
        register pj_rbtree_node *temp = node->parent;
        while (temp!=null && temp->right==node) {
	    node = temp;
	    temp = temp->parent;
	}
	node = temp;
    }    
    return node != null ? node : NULL;
}

PJ_DEF(pj_rbtree_node*) pj_rbtree_prev( pj_rbtree *tree, 
					register pj_rbtree_node *node )
{
    register pj_rbtree_node *null = tree->null;
    
    PJ_CHECK_STACK();

    if (node->left != null) {
        for (node=node->left; node->right!=null; node=node->right)
	   /* void */;
    } else {
        register pj_rbtree_node *temp = node->parent;
        while (temp!=null && temp->left==node) {
	    node = temp;
	    temp = temp->parent;
        }
        node = temp;
    }    
    return node != null ? node : NULL;
}

PJ_DEF(int) pj_rbtree_insert( pj_rbtree *tree, 
			      pj_rbtree_node *element )
{
    int rv = 0;
    pj_rbtree_node *node, *parent = tree->null, 
		   *null = tree->null;
    pj_rbtree_comp *comp = tree->comp;
	
    PJ_CHECK_STACK();

    node = tree->root;	
    while (node != null) {
        rv = (*comp)(element->key, node->key);
        if (rv == 0) {
	    /* found match, i.e. entry with equal key already exist */
	    return -1;
	}    
	parent = node;
        node = rv < 0 ? node->left : node->right;
    }

    element->color = PJ_RBCOLOR_RED;
    element->left = element->right = null;

    node = element;
    if (parent != null) {
        node->parent = parent;
        if (rv < 0)
	   parent->left = node;
        else
	   parent->right = node;
        insert_fixup( tree, node);
    } else {
        tree->root = node;
        node->parent = null;
        node->color = PJ_RBCOLOR_BLACK;
    }
	
    ++tree->size;
    return 0;
}


PJ_DEF(pj_rbtree_node*) pj_rbtree_find( pj_rbtree *tree,
					const void *key )
{
    int rv;
    pj_rbtree_node *node = tree->root;
    pj_rbtree_node *null = tree->null;
    pj_rbtree_comp *comp = tree->comp;
    
    while (node != null) {
        rv = (*comp)(key, node->key);
        if (rv == 0)
	    return node;
        node = rv < 0 ? node->left : node->right;
    }
    return node != null ? node : NULL;
}

PJ_DEF(pj_rbtree_node*) pj_rbtree_erase( pj_rbtree *tree,
					 pj_rbtree_node *node )
{
    pj_rbtree_node *succ;
    pj_rbtree_node *null = tree->null;
    pj_rbtree_node *child;
    pj_rbtree_node *parent;
    
    PJ_CHECK_STACK();

    if (node->left == null || node->right == null) {
        succ = node;
    } else {
        for (succ=node->right; succ->left!=null; succ=succ->left)
	   /* void */;
    }

    child = succ->left != null ? succ->left : succ->right;
    parent = succ->parent;
    child->parent = parent;
    
    if (parent != null) {
	if (parent->left == succ)
	    parent->left = child;
        else
	   parent->right = child;
    } else
        tree->root = child;

    if (succ != node) {
        succ->parent = node->parent;
        succ->left = node->left;
        succ->right = node->right;
        succ->color = node->color;

        parent = node->parent;
        if (parent != null) {
	   if (parent->left==node)
	        parent->left=succ;
	   else
		parent->right=succ;
        }
        if (node->left != null)
	   node->left->parent = succ;;
        if (node->right != null)
	    node->right->parent = succ;

        if (tree->root == node)
	   tree->root = succ;
    }

    if (succ->color == PJ_RBCOLOR_BLACK) {
	if (child != null) 
	    delete_fixup(tree, child);
        tree->null->color = PJ_RBCOLOR_BLACK;
    }

    --tree->size;
    return node;
}


PJ_DEF(unsigned) pj_rbtree_max_height( pj_rbtree *tree,
				       pj_rbtree_node *node )
{
    unsigned l, r;
    
    PJ_CHECK_STACK();

    if (node==NULL) 
	node = tree->root;
    
    l = node->left != tree->null ? pj_rbtree_max_height(tree,node->left)+1 : 0;
    r = node->right != tree->null ? pj_rbtree_max_height(tree,node->right)+1 : 0;
    return l > r ? l : r;
}

PJ_DEF(unsigned) pj_rbtree_min_height( pj_rbtree *tree,
				       pj_rbtree_node *node )
{
    unsigned l, r;
    
    PJ_CHECK_STACK();

    if (node==NULL) 
	node=tree->root;
    
    l = (node->left != tree->null) ? pj_rbtree_max_height(tree,node->left)+1 : 0;
    r = (node->right != tree->null) ? pj_rbtree_max_height(tree,node->right)+1 : 0;
    return l > r ? r : l;
}


