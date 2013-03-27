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
 *	Header file for handles
 */

#ifndef CMA_HANDLE
#define CMA_HANDLE

/*
 *  INCLUDE FILES
 */

#include <cma_defs.h>
#include <cma_attr.h>

/*
 * CONSTANTS AND MACROS
 */

#define cma__validate_attr(handle) \
    ((cma__t_int_attr *)cma__validate_handle ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_attr))

#define cma__validate_cv(handle) \
    ((cma__t_int_cv *)cma__validate_handle ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_cv))

#define cma__validate_mutex(handle) \
    ((cma__t_int_mutex *)cma__validate_handle ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_mutex))

#define cma__validate_tcb(handle) \
    ((cma__t_int_tcb *)cma__validate_handle ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_tcb))

#define cma__validate_stack(handle) \
    ((cma__t_int_stack *)cma__validate_handle ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_stack))

#define cma__validate_null_attr(handle) \
    ((cma__t_int_attr *)cma__validate_handle_null ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_attr))

#define cma__validate_null_cv(handle) \
    ((cma__t_int_cv *)cma__validate_handle_null ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_cv))

#define cma__validate_null_mutex(handle) \
    ((cma__t_int_mutex *)cma__validate_handle_null ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_mutex))

#define cma__validate_null_tcb(handle) \
    ((cma__t_int_tcb *)cma__validate_handle_null ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_tcb))

#define cma__validate_null_stack(handle) \
    ((cma__t_int_stack *)cma__validate_handle_null ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_stack))

#define cma__val_attr_stat(handle,obj) \
    cma__val_hand_stat ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_attr, \
	    (cma__t_object **)obj)

#define cma__val_cv_stat(handle,obj) \
    cma__val_hand_stat ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_cv, \
	    (cma__t_object **)obj)

#define cma__val_mutex_stat(handle,obj) \
    cma__val_hand_stat ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_mutex, \
	    (cma__t_object **)obj)

#define cma__val_tcb_stat(handle) \
    cma__val_hand_stat ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_tcb, \
	    (cma__t_object **)obj)

#define cma__val_stack_stat(handle,obj) \
    cma__val_hand_stat ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_stack, \
	    (cma__t_object **)obj)

#define cma__val_nullattr_stat(handle,obj) \
    cma__val_handnull_stat ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_attr, \
	    (cma__t_object **)obj)

#define cma__val_nullcv_stat(handle,obj) \
    cma__val_handnull_stat ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_cv, \
	    (cma__t_object **)obj)

#define cma__val_nullmutex_stat(handle,obj) \
    cma__val_handnull_stat ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_mutex, \
	    (cma__t_object **)obj)

#define cma__val_nulltcb_stat(handle,obj) \
    cma__val_handnull_stat ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_tcb, \
	    (cma__t_object **)obj)

#define cma__val_nullstack_stat(handle) \
    cma__val_handnull_stat ( \
	    (cma_t_handle *)(handle), \
	    cma__c_obj_stack, \
	    (cma__t_object **)obj)

/*
 * TYPEDEFS
 */

/*
 * Internal format of a handle (to the outside world it's an array of two
 * addresses, but we know better).
 */
typedef struct CMA__T_INT_HANDLE {
    cma__t_object	*pointer;	/* Address of internal structure */
    cma__t_short	sequence;	/* Sequence number of object */
    cma__t_short	type;		/* Type code of object */
    } cma__t_int_handle;

/*
 *  GLOBAL DATA
 */

/*
 * INTERNAL INTERFACES
 */

extern void cma__clear_handle (cma_t_handle *);

extern void cma__object_to_handle (cma__t_object *,cma_t_handle *);

extern cma__t_int_attr * cma__validate_default_attr (cma_t_handle *);

extern cma_t_status cma__val_defattr_stat (cma_t_handle *,cma__t_int_attr **);

extern cma__t_object * cma__validate_handle (cma_t_handle *,cma_t_natural );

extern cma_t_status cma__val_hand_stat (cma_t_handle *,cma_t_natural,cma__t_object **);

extern 	cma__t_object	*cma__validate_handle_null (cma_t_handle *,cma_t_natural);

extern cma_t_status cma__val_handnull_stat (cma_t_handle *,cma_t_natural,cma__t_object **);

#endif
