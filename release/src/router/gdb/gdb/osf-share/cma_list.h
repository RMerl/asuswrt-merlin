/* 
 * (c) Copyright 1990-1996 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1990-1996 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1990-1996 DIGITAL EQUIPMENT CORPORATION
 * (c) Copyright 1991, 1992 Siemens-Nixdorf Information Systems
 * To anyone who acknowledges that this file is provided "AS IS" without
 * any express or implied warranty: permission to use, copy, modify, and
 * distribute this file for any purpose is hereby granted without fee,
 * provided that the above copyright notices and this notice appears in
 * all source code copies, and that none of the names listed above be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  None of these organizations
 * makes any representations about the suitability of this software for
 * any purpose.
 */
/*
 *	Header file for generic list functions operating on singly linked
 *	null-terminated lists.  Items may not be REMOVED from the list!  The
 *	intent is that the list can be traversed (for read-only operations)
 *	without locking, since insertion is "safe" (though not truely
 *	atomic).  THIS ASSUMES THAT THE HARDWARE MAKES WRITES VISIBLE TO READS
 *	IN THE ORDER IN WHICH THEY OCCURRED!  WITHOUT SUCH READ/WRITE
 *	ORDERING, IT MAY BE NECESSARY TO INSERT "BARRIERS" TO PRODUCE THE
 *	REQUIRED VISIBILITY!
 */

#ifndef CMA_LIST
#define CMA_LIST

/*
 *  INCLUDE FILES
 */

#include <cma.h>

/*
 * CONSTANTS AND MACROS
 */

#define cma__c_null_list	((cma__t_list *)cma_c_null_ptr)

/*
 * Test whether a list is empty.  Return cma_c_true if so, else
 * cma_c_false.
 */
#define cma__list_empty(head)	((head)->link == cma__c_null_list)

/*
 * Initialize a queue header to empty.
 */
#define cma__list_init(head)	(void)((head)->link = cma__c_null_list)

/*
 * Insert an element in a list following the specified item (or at the
 * beginning of the list if "list" is the list head).  NOTE: insertion
 * operations should be interlocked by the caller!
 */
#define cma__list_insert(element,list)    (void)(	\
    (element)->link		= (list)->link,		\
    (list)->link		= (element))

/*
 * Return the next item in a list (or the first, if the address is of the
 * list header)
 */
#define cma__list_next(element)    ((element)->link)

/*
 * TYPEDEFS
 */

typedef struct CMA__T_LIST {
    struct CMA__T_LIST	*link;		/* Forward link */
    } cma__t_list;

/*
 *  GLOBAL DATA
 */

/*
 * INTERNAL INTERFACES
 */

#endif
