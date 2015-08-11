/* $Id: rbtree.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_RBTREE_H__
#define __PJ_RBTREE_H__

/**
 * @file rbtree.h
 * @brief Red/Black Tree
 */

#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_RBTREE Red/Black Balanced Tree
 * @ingroup PJ_DS
 * @brief
 * Red/Black tree is the variant of balanced tree, where the search, insert, 
 * and delete operation is \b guaranteed to take at most \a O( lg(n) ).
 * @{
 */
/**
 * Color type for Red-Black tree.
 */ 
typedef enum pj_rbcolor_t
{
    PJ_RBCOLOR_BLACK,
    PJ_RBCOLOR_RED
} pj_rbcolor_t;

/**
 * The type of the node of the R/B Tree.
 */
typedef struct pj_rbtree_node 
{
    /** Pointers to the node's parent, and left and right siblings. */
    struct pj_rbtree_node *parent, *left, *right;

    /** Key associated with the node. */
    const void *key;

    /** User data associated with the node. */
    void *user_data;

    /** The R/B Tree node color. */
    pj_rbcolor_t color;

} pj_rbtree_node;


/**
 * The type of function use to compare key value of tree node.
 * @return
 *  0 if the keys are equal
 * <0 if key1 is lower than key2
 * >0 if key1 is greater than key2.
 */
typedef int pj_rbtree_comp(const void *key1, const void *key2);


/**
 * Declaration of a red-black tree. All elements in the tree must have UNIQUE
 * key.
 * A red black tree always maintains the balance of the tree, so that the
 * tree height will not be greater than lg(N). Insert, search, and delete
 * operation will take lg(N) on the worst case. But for insert and delete,
 * there is additional time needed to maintain the balance of the tree.
 */
typedef struct pj_rbtree
{
    pj_rbtree_node null_node;   /**< Constant to indicate NULL node.    */
    pj_rbtree_node *null;       /**< Constant to indicate NULL node.    */
    pj_rbtree_node *root;       /**< Root tree node.                    */
    unsigned size;              /**< Number of elements in the tree.    */
    pj_rbtree_comp *comp;       /**< Key comparison function.           */
} pj_rbtree;


/**
 * Guidance on how much memory required for each of the node.
 */
#define PJ_RBTREE_NODE_SIZE	    (sizeof(pj_rbtree_node))


/**
 * Guidance on memory required for the tree.
 */
#define PJ_RBTREE_SIZE		    (sizeof(pj_rbtree))


/**
 * Initialize the tree.
 * @param tree the tree to be initialized.
 * @param comp key comparison function to be used for this tree.
 */
PJ_DECL(void) pj_rbtree_init( pj_rbtree *tree, pj_rbtree_comp *comp);

/**
 * Get the first element in the tree.
 * The first element always has the least value for the key, according to
 * the comparison function.
 * @param tree the tree.
 * @return the tree node, or NULL if the tree has no element.
 */
PJ_DECL(pj_rbtree_node*) pj_rbtree_first( pj_rbtree *tree );

/**
 * Get the last element in the tree.
 * The last element always has the greatest key value, according to the
 * comparison function defined for the tree.
 * @param tree the tree.
 * @return the tree node, or NULL if the tree has no element.
 */
PJ_DECL(pj_rbtree_node*) pj_rbtree_last( pj_rbtree *tree );

/**
 * Get the successive element for the specified node.
 * The successive element is an element with greater key value.
 * @param tree the tree.
 * @param node the node.
 * @return the successive node, or NULL if the node has no successor.
 */
PJ_DECL(pj_rbtree_node*) pj_rbtree_next( pj_rbtree *tree, 
					 pj_rbtree_node *node );

/**
 * The the previous node for the specified node.
 * The previous node is an element with less key value.
 * @param tree the tree.
 * @param node the node.
 * @return the previous node, or NULL if the node has no previous node.
 */
PJ_DECL(pj_rbtree_node*) pj_rbtree_prev( pj_rbtree *tree, 
					 pj_rbtree_node *node );

/**
 * Insert a new node. 
 * The node will be inserted at sorted location. The key of the node must 
 * be UNIQUE, i.e. it hasn't existed in the tree.
 * @param tree the tree.
 * @param node the node to be inserted.
 * @return zero on success, or -1 if the key already exist.
 */
PJ_DECL(int) pj_rbtree_insert( pj_rbtree *tree, 
			       pj_rbtree_node *node );

/**
 * Find a node which has the specified key.
 * @param tree the tree.
 * @param key the key to search.
 * @return the tree node with the specified key, or NULL if the key can not
 *         be found.
 */
PJ_DECL(pj_rbtree_node*) pj_rbtree_find( pj_rbtree *tree,
					 const void *key );

/**
 * Erase a node from the tree.
 * @param tree the tree.
 * @param node the node to be erased.
 * @return the tree node itself.
 */
PJ_DECL(pj_rbtree_node*) pj_rbtree_erase( pj_rbtree *tree,
					  pj_rbtree_node *node );

/**
 * Get the maximum tree height from the specified node.
 * @param tree the tree.
 * @param node the node, or NULL to get the root of the tree.
 * @return the maximum height, which should be at most lg(N)
 */
PJ_DECL(unsigned) pj_rbtree_max_height( pj_rbtree *tree,
					pj_rbtree_node *node );

/**
 * Get the minumum tree height from the specified node.
 * @param tree the tree.
 * @param node the node, or NULL to get the root of the tree.
 * @return the height
 */
PJ_DECL(unsigned) pj_rbtree_min_height( pj_rbtree *tree,
					pj_rbtree_node *node );


/**
 * @}
 */

PJ_END_DECL

#endif	/* __PJ_RBTREE_H__ */

