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
 *	Header file for attributes object
 */

#ifndef CMA_ATTR
#define CMA_ATTR

/*
 *  INCLUDE FILES
 */

#include <cma_defs.h>
#include <cma_queue.h>
#ifdef __hpux
# include <sys/param.h>
#endif
#if _CMA_UNIX_TYPE == _CMA__SVR4
#include <sys/unistd.h>
#endif
/*
 * CONSTANTS AND MACROS
 */


/*
 *  FUNCTIONAL DESCRIPTION:
 *
 *	cma__int_attr_get_priority -  Performs the work of cma_attr_get_priority
 *
 *  FORMAL PARAMETERS:
 *
 *	cma_t_attr	    *_att_	- Attribute object to get from
 *	cma_t_priority	    *_setting_	- Current setting
 *
 *  IMPLICIT INPUTS:
 *
 *	none
 *
 *  IMPLICIT OUTPUTS:
 *
 *	priority
 *
 *  FUNCTION VALUE:
 *
 *	none
 *
 *  SIDE EFFECTS:
 *
 *	none
 */
#define cma__int_attr_get_priority(_att_,_setting_) { \
    cma__t_int_attr     *_int_att_; \
    (_int_att_) = cma__validate_default_attr (_att_); \
    cma__int_lock ((_int_att_)->mutex); \
    (*(_setting_)) = (_int_att_)->priority; \
    cma__int_unlock ((_int_att_)->mutex); \
    }


/*
 *  FUNCTIONAL DESCRIPTION:
 *
 *	cma__int_attr_get_sched - Performs work of cma_attr_get_sched
 *
 *  FORMAL PARAMETERS:
 *
 *	cma_t_attr	    *_att_	_ Attributes object used
 *	cma_t_sched_policy  *_setting_	- Current setting
 *
 *  IMPLICIT INPUTS:
 *
 *	none
 *
 *  IMPLICIT OUTPUTS:
 *
 *	scheduling policy
 *
 *  FUNCTION VALUE:
 *
 *	none
 *
 *  SIDE EFFECTS:
 *
 *	none
 */
#define cma__int_attr_get_sched(_att_,_setting_) { \
    cma__t_int_attr     *_int_att_; \
    (_int_att_) = cma__validate_default_attr (_att_); \
    cma__int_lock ((_int_att_)->mutex); \
    (*(_setting_)) = (_int_att_)->policy; \
    cma__int_unlock ((_int_att_)->mutex); \
    }


/*
 *  FUNCTIONAL DESCRIPTION:
 *
 *	cma__int_attr_get_inherit_sched - Performs work of 
 *	cma_attr_get_inherit_sched
 *
 *  FORMAL PARAMETERS:
 *
 *	cma_t_attr	    *_att_	- Attributes object to use
 *	cma_t_sched_inherit *_setting_	- Current setting
 *
 *  IMPLICIT INPUTS:
 *
 *	none
 *
 *  IMPLICIT OUTPUTS:
 *
 *	Inheritable scheduling policy
 *
 *  FUNCTION VALUE:
 *
 *	none
 *
 *  SIDE EFFECTS:
 *
 *	none
 */
#define cma__int_attr_get_inherit_sched(_att_,_setting_) { \
    cma__t_int_attr	*_int_att_; \
    (_int_att_) = cma__validate_default_attr (_att_); \
    cma__int_lock ((_int_att_)->mutex); \
    (*(_setting_)) \
        = ((_int_att_)->inherit_sched ? cma_c_sched_inherit : cma_c_sched_use_default); \
    cma__int_unlock ((_int_att_)->mutex); \
    }

/*
 *  FUNCTIONAL DESCRIPTION:
 *
 *	cma__int_attr_set_stacksize - Performs work for cma_attr_set_stacksize
 *
 *  FORMAL PARAMETERS:
 *
 *      cma_t_attr          *_att_      - Attributes object to use
 *	cma_t_natural	    _setting_	- Setting
 *
 *  IMPLICIT INPUTS:
 *
 *	none
 *
 *  IMPLICIT OUTPUTS:
 *
 *	none
 *
 *  FUNCTION VALUE:
 *
 *	none
 *
 *  SIDE EFFECTS:
 *
 *	Change attribute objects stack size setting
 */
#define cma__int_attr_set_stacksize(_att_,_setting_) { \
    cma__t_int_attr     *_int_att_; \
    if ((_setting_) <= 0) \
        cma__error (cma_s_badparam); \
    _int_att_ = cma__validate_attr (_att_); \
    cma__int_lock ((_int_att_)->mutex); \
    _int_att_->stack_size = cma__roundup_chunksize(_setting_); \
    cma__free_cache (_int_att_, cma__c_obj_tcb); \
    _int_att_->cache[cma__c_obj_tcb].revision++; \
    _int_att_->cache[cma__c_obj_stack].revision++; \
    cma__int_unlock (_int_att_->mutex); \
    }

/*
 *  FUNCTIONAL DESCRIPTION:
 *
 *	cma__int_attr_get_stacksize - Performs work of cma_attr_get_stacksize
 *
 *  FORMAL PARAMETERS:
 *
 *      cma_t_attr          *_att_      - Attributes object to use
 *	cma_t_natural	    *_setting_	- Current setting
 *
 *  IMPLICIT INPUTS:
 *
 *	none
 *
 *  IMPLICIT OUTPUTS:
 *
 *	Attribute objects stack size setting
 *
 *  FUNCTION VALUE:
 *
 *	none
 *
 *  SIDE EFFECTS:
 *
 *	none
 */
#define cma__int_attr_get_stacksize(_att_,_setting_) { \
    cma__t_int_attr     *_int_att_; \
    (_int_att_) = cma__validate_default_attr (_att_); \
    cma__int_lock ((_int_att_)->mutex); \
    (*(_setting_)) = (_int_att_)->stack_size; \
    cma__int_unlock ((_int_att_)->mutex); \
    }


/*
 *  FUNCTIONAL DESCRIPTION:
 *
 *	cma__int_attr_set_guardsize - Performs work for cma_attr_set_guardsize
 *
 *  FORMAL PARAMETERS:
 *
 *      cma_t_attr          *_att_      - Attributes object to use
 *	cma_t_natural	    _setting_	- Setting
 *
 *  IMPLICIT INPUTS:
 *
 *	none
 *
 *  IMPLICIT OUTPUTS:
 *
 *	none
 *
 *  FUNCTION VALUE:
 *
 *	none
 *
 *  SIDE EFFECTS:
 *
 *	Change attribute objects guard size setting
 */
#define cma__int_attr_set_guardsize(_att_,_setting_) { \
    cma__t_int_attr     *_int_att_; \
    _int_att_ = cma__validate_attr (_att_); \
    cma__int_lock ((_int_att_)->mutex); \
    _int_att_->guard_size = cma__roundup_chunksize(_setting_); \
    cma__free_cache (_int_att_, cma__c_obj_tcb); \
    _int_att_->cache[cma__c_obj_tcb].revision++; \
    _int_att_->cache[cma__c_obj_stack].revision++; \
    cma__int_unlock (_int_att_->mutex); \
    }

/*
 *  FUNCTIONAL DESCRIPTION:
 *
 *	cma__int_attr_get_guardsize - Performs work of cma_attr_get_guardsize
 *
 *  FORMAL PARAMETERS:
 *
 *      cma_t_attr          *_att_      - Attributes object to use
 *	cma_t_natural	    *_setting_	- Current setting
 *
 *  IMPLICIT INPUTS:
 *
 *	none
 *
 *  IMPLICIT OUTPUTS:
 *
 *	Attribute objects guard size setting
 *
 *  FUNCTION VALUE:
 *
 *	none
 *
 *  SIDE EFFECTS:
 *
 *	none
 */
#define cma__int_attr_get_guardsize(_att_,_setting_) { \
    cma__t_int_attr     *_int_att_; \
    (_int_att_) = cma__validate_default_attr (_att_); \
    cma__int_lock ((_int_att_)->mutex); \
    (*(_setting_)) = (_int_att_)->guard_size; \
    cma__int_unlock ((_int_att_)->mutex); \
    }

/*
 * TYPEDEFS
 */
#ifndef __STDC__
struct CMA__T_INT_MUTEX;		/* Avoid circular dependency */
#endif

typedef struct CMA__T_CACHE {
    cma_t_natural		revision;	/* Revisions */
    cma_t_natural		count;
    cma__t_queue		queue;	/* Cache headers */
    } cma__t_cache;

typedef struct CMA__T_INT_ATTR {
    cma__t_object		header;		/* Common header */
    struct CMA__T_INT_ATTR	*attributes;	/* Point to controlling attr */
    struct CMA__T_INT_MUTEX	*mutex;		/* Serialize access to object */
    cma_t_priority		priority;	/* Priority of new thread */
    cma_t_sched_policy		policy;		/* Sched policy of thread */
    cma_t_boolean		inherit_sched;	/* Is scheduling inherited? */
    cma_t_natural		stack_size;	/* Size of stack (bytes) */
    cma_t_natural		guard_size;	/* Size of guard (bytes) */
    cma_t_mutex_kind		mutex_kind;	/* Mutex kind */
    cma__t_cache		cache[cma__c_obj_num];	/* Cache information */
    cma_t_boolean		delete_pending;	/* attr. obj. is deleted */
    cma_t_natural		refcnt;	/* Number of objects using attr. obj */
    } cma__t_int_attr;

/*
 *  GLOBAL DATA
 */

extern cma__t_int_attr	cma__g_def_attr;

/*
 * INTERNAL INTERFACES
 */

extern void cma__destroy_attributes  (cma__t_int_attr *);

extern void cma__free_attributes  (cma__t_int_attr	*);

extern void cma__free_cache  (cma__t_int_attr *,cma_t_natural );

extern cma__t_int_attr *cma__get_attributes  (cma__t_int_attr	*);

extern void cma__init_attr  (void);

extern void cma__reinit_attr  (cma_t_integer);

#endif
