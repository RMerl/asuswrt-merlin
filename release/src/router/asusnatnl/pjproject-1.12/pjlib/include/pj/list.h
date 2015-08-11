/* $Id: list.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_LIST_H__
#define __PJ_LIST_H__

/**
 * @file list.h
 * @brief Linked List data structure.
 */

#include <pj/types.h>

PJ_BEGIN_DECL

/*
 * @defgroup PJ_DS Data Structure.
 */

/**
 * @defgroup PJ_LIST Linked List
 * @ingroup PJ_DS
 * @{
 *
 * List in PJLIB is implemented as doubly-linked list, and it won't require
 * dynamic memory allocation (just as all PJLIB data structures). The list here
 * should be viewed more like a low level C list instead of high level C++ list
 * (which normally are easier to use but require dynamic memory allocations),
 * therefore all caveats with C list apply here too (such as you can NOT put
 * a node in more than one lists).
 *
 * \section pj_list_example_sec Examples
 *
 * See below for examples on how to manipulate linked list:
 *  - @ref page_pjlib_samples_list_c
 *  - @ref page_pjlib_list_test
 */


/**
 * Use this macro in the start of the structure declaration to declare that
 * the structure can be used in the linked list operation. This macro simply
 * declares additional member @a prev and @a next to the structure.
 * @hideinitializer
 */
#define PJ_DECL_LIST_MEMBER(type)                       \
                                   /** List @a prev. */ \
                                   type *prev;          \
                                   /** List @a next. */ \
                                   type *next 


/**
 * This structure describes generic list node and list. The owner of this list
 * must initialize the 'value' member to an appropriate value (typically the
 * owner itself).
 */
struct pj_list
{
    PJ_DECL_LIST_MEMBER(void);
};


/**
 * Initialize the list.
 * Initially, the list will have no member, and function pj_list_empty() will
 * always return nonzero (which indicates TRUE) for the newly initialized 
 * list.
 *
 * @param node The list head.
 */
PJ_INLINE(void) pj_list_init(pj_list_type * node)
{
    ((pj_list*)node)->next = ((pj_list*)node)->prev = node;
}


/**
 * Check that the list is empty.
 *
 * @param node	The list head.
 *
 * @return Non-zero if the list is empty, or zero if it is not empty.
 *
 */
PJ_INLINE(int) pj_list_empty(const pj_list_type * node)
{
    return ((pj_list*)node)->next == node;
}


/**
 * Insert the node to the list before the specified element position.
 *
 * @param pos	The element to which the node will be inserted before. 
 * @param node	The element to be inserted.
 *
 * @return void.
 */
PJ_IDECL(void)	pj_list_insert_before(pj_list_type *pos, pj_list_type *node);


/**
 * Insert the node to the back of the list. This is just an alias for
 * #pj_list_insert_before().
 *
 * @param list	The list. 
 * @param node	The element to be inserted.
 */
PJ_INLINE(void) pj_list_push_back(pj_list_type *list, pj_list_type *node)
{
    pj_list_insert_before(list, node);
}


/**
 * Inserts all nodes in \a nodes to the target list.
 *
 * @param lst	    The target list.
 * @param nodes	    Nodes list.
 */
PJ_IDECL(void) pj_list_insert_nodes_before(pj_list_type *lst,
					   pj_list_type *nodes);

/**
 * Insert a node to the list after the specified element position.
 *
 * @param pos	    The element in the list which will precede the inserted 
 *		    element.
 * @param node	    The element to be inserted after the position element.
 *
 * @return void.
 */
PJ_IDECL(void) pj_list_insert_after(pj_list_type *pos, pj_list_type *node);


/**
 * Insert the node to the front of the list. This is just an alias for
 * #pj_list_insert_after().
 *
 * @param list	The list. 
 * @param node	The element to be inserted.
 */
PJ_INLINE(void) pj_list_push_front(pj_list_type *list, pj_list_type *node)
{
    pj_list_insert_after(list, node);
}


/**
 * Insert all nodes in \a nodes to the target list.
 *
 * @param lst	    The target list.
 * @param nodes	    Nodes list.
 */
PJ_IDECL(void) pj_list_insert_nodes_after(pj_list_type *lst,
					  pj_list_type *nodes);


/**
 * Remove elements from the source list, and insert them to the destination
 * list. The elements of the source list will occupy the
 * front elements of the target list. Note that the node pointed by \a list2
 * itself is not considered as a node, but rather as the list descriptor, so
 * it will not be inserted to the \a list1. The elements to be inserted starts
 * at \a list2->next. If \a list2 is to be included in the operation, use
 * \a pj_list_insert_nodes_before.
 *
 * @param list1	The destination list.
 * @param list2	The source list.
 *
 * @return void.
 */
PJ_IDECL(void) pj_list_merge_first(pj_list_type *list1, pj_list_type *list2);


/**
 * Remove elements from the second list argument, and insert them to the list 
 * in the first argument. The elements from the second list will be appended
 * to the first list. Note that the node pointed by \a list2
 * itself is not considered as a node, but rather as the list descriptor, so
 * it will not be inserted to the \a list1. The elements to be inserted starts
 * at \a list2->next. If \a list2 is to be included in the operation, use
 * \a pj_list_insert_nodes_before.
 *
 * @param list1	    The element in the list which will precede the inserted 
 *		    element.
 * @param list2	    The element in the list to be inserted.
 *
 * @return void.
 */
PJ_IDECL(void) pj_list_merge_last( pj_list_type *list1, pj_list_type *list2);


/**
 * Erase the node from the list it currently belongs.
 *
 * @param node	    The element to be erased.
 */
PJ_IDECL(void) pj_list_erase(pj_list_type *node);


/**
 * Find node in the list.
 *
 * @param list	    The list head.
 * @param node	    The node element to be searched.
 *
 * @return The node itself if it is found in the list, or NULL if it is not 
 *         found in the list.
 */
PJ_IDECL(pj_list_type*) pj_list_find_node(pj_list_type *list, 
					  pj_list_type *node);


/**
 * Search the list for the specified value, using the specified comparison
 * function. This function iterates on nodes in the list, started with the
 * first node, and call the user supplied comparison function until the
 * comparison function returns ZERO.
 *
 * @param list	    The list head.
 * @param value	    The user defined value to be passed in the comparison 
 *		    function
 * @param comp	    The comparison function, which should return ZERO to 
 *		    indicate that the searched value is found.
 *
 * @return The first node that matched, or NULL if it is not found.
 */
PJ_IDECL(pj_list_type*) pj_list_search(pj_list_type *list, void *value,
				       int (*comp)(void *value, 
						   const pj_list_type *node)
				       );


/**
 * Traverse the list to get the number of elements in the list.
 *
 * @param list	    The list head.
 *
 * @return	    Number of elements.
 */
PJ_IDECL(pj_size_t) pj_list_size(const pj_list_type *list);


/**
 * @}
 */

#if PJ_FUNCTIONS_ARE_INLINED
#  include "list_i.h"
#endif

PJ_END_DECL

#endif	/* __PJ_LIST_H__ */

