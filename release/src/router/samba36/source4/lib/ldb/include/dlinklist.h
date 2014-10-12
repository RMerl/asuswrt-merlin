/* 
   Unix SMB/CIFS implementation.
   some simple double linked list macros

   Copyright (C) Andrew Tridgell 1998-2010

     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

/* To use these macros you must have a structure containing a next and
   prev pointer */

#ifndef _DLINKLIST_H
#define _DLINKLIST_H

/*
  February 2010 - changed list format to have a prev pointer from the
  list head. This makes DLIST_ADD_END() O(1) even though we only have
  one list pointer.

  The scheme is as follows:

     1) with no entries in the list:
          list_head == NULL

     2) with 1 entry in the list:
          list_head->next == NULL
          list_head->prev == list_head

     3) with 2 entries in the list:
          list_head->next == element2
          list_head->prev == element2
	  element2->prev == list_head
	  element2->next == NULL

     4) with N entries in the list:
          list_head->next == element2
          list_head->prev == elementN
	  elementN->prev == element{N-1}
	  elementN->next == NULL

  This allows us to find the tail of the list by using
  list_head->prev, which means we can add to the end of the list in
  O(1) time


  Note that the 'type' arguments below are no longer needed, but
  are kept for now to prevent an incompatible argument change
 */


/*
   add an element at the front of a list
*/
#define DLIST_ADD(list, p) \
do { \
        if (!(list)) { \
		(p)->prev = (list) = (p);  \
		(p)->next = NULL; \
	} else { \
		(p)->prev = (list)->prev; \
		(list)->prev = (p); \
		(p)->next = (list); \
		(list) = (p); \
	} \
} while (0)

/*
   remove an element from a list
   Note that the element doesn't have to be in the list. If it
   isn't then this is a no-op
*/
#define DLIST_REMOVE(list, p) \
do { \
	if ((p) == (list)) { \
		if ((p)->next) (p)->next->prev = (p)->prev; \
		(list) = (p)->next; \
	} else if ((list) && (p) == (list)->prev) {	\
		(p)->prev->next = NULL; \
		(list)->prev = (p)->prev; \
	} else { \
		if ((p)->prev) (p)->prev->next = (p)->next; \
		if ((p)->next) (p)->next->prev = (p)->prev; \
	} \
	if ((p) != (list)) (p)->next = (p)->prev = NULL;	\
} while (0)

/*
   find the head of the list given any element in it.
   Note that this costs O(N), so you should avoid this macro
   if at all possible!
*/
#define DLIST_HEAD(p, result_head) \
do { \
       (result_head) = (p); \
       while (DLIST_PREV(result_head)) (result_head) = (result_head)->prev; \
} while(0)

/* return the last element in the list */
#define DLIST_TAIL(list) ((list)?(list)->prev:NULL)

/* return the previous element in the list. */
#define DLIST_PREV(p) (((p)->prev && (p)->prev->next != NULL)?(p)->prev:NULL)

/* insert 'p' after the given element 'el' in a list. If el is NULL then
   this is the same as a DLIST_ADD() */
#define DLIST_ADD_AFTER(list, p, el) \
do { \
        if (!(list) || !(el)) { \
		DLIST_ADD(list, p); \
	} else { \
		(p)->prev = (el);   \
		(p)->next = (el)->next;		\
		(el)->next = (p);		\
		if ((p)->next) (p)->next->prev = (p);	\
		if ((list)->prev == (el)) (list)->prev = (p); \
	}\
} while (0)


/*
   add to the end of a list.
   Note that 'type' is ignored
*/
#define DLIST_ADD_END(list, p, type)			\
do { \
	if (!(list)) { \
		DLIST_ADD(list, p); \
	} else { \
		DLIST_ADD_AFTER(list, p, (list)->prev); \
	} \
} while (0)

/* promote an element to the from of a list */
#define DLIST_PROMOTE(list, p) \
do { \
          DLIST_REMOVE(list, p); \
          DLIST_ADD(list, p); \
} while (0)

/*
   demote an element to the end of a list.
   Note that 'type' is ignored
*/
#define DLIST_DEMOTE(list, p, type)			\
do { \
	DLIST_REMOVE(list, p); \
	DLIST_ADD_END(list, p, NULL);		\
} while (0)

/*
   concatenate two lists - putting all elements of the 2nd list at the
   end of the first list.
   Note that 'type' is ignored
*/
#define DLIST_CONCATENATE(list1, list2, type)	\
do { \
	if (!(list1)) { \
		(list1) = (list2); \
	} else { \
		(list1)->prev->next = (list2); \
		if (list2) { \
			void *_tmplist = (void *)(list1)->prev; \
			(list1)->prev = (list2)->prev; \
			(list2)->prev = _tmplist; \
		} \
	} \
} while (0)

#endif /* _DLINKLIST_H */
