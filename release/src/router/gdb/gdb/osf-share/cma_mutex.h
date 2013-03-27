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
 *	Header file for mutex operations
 */

#ifndef CMA_MUTEX
#define CMA_MUTEX

/*
 *  INCLUDE FILES
 */

#include <cma.h>
#include <cma_attr.h>
#include <cma_defs.h>
#include <cma_semaphore_defs.h>
#include <cma_sequence.h>
#include <cma_tcb_defs.h>
#include <cma_stack.h>

/*
 * CONSTANTS AND MACROS
 */

/*
 * TYPEDEFS
 */

typedef struct CMA__T_INT_MUTEX {
    cma__t_object	header;		/* Common header (sequence, type) */
    cma__t_int_attr	*attributes;	/* Back link */
    cma__t_int_tcb	*owner;		/* Current owner (if any) */
    cma_t_integer	nest_count;	/* Nesting level for recursive mutex */
    cma__t_atomic_bit	*unlock;	/* Pointer used for unlock operation */
    cma__t_atomic_bit	lock;		/* Set if currently locked */
    struct CMA__T_INT_MUTEX *int_lock;	/* Internal protection for mutex */
    cma__t_atomic_bit	event;		/* Clear when unlock requires action */
    cma__t_atomic_bit	waiters;	/* Clear when threads are waiting */
    cma__t_atomic_bit	bitbucket;	/* Fake bit to keep friendlies locked */
    cma_t_mutex_kind	mutex_kind;	/* Kind of mutex */
    cma__t_semaphore	semaphore;	/* Semaphore for low-level wait */
    } cma__t_int_mutex;


/*
 *  FUNCTIONAL DESCRIPTION:
 *
 *	Lock a mutex (internal)
 *
 *  FORMAL PARAMETERS:
 *
 *	mutex		Pointer to mutex object to lock
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
 *	none
 */
#ifdef NDEBUG
# define cma__int_lock(mutex) { \
    if (cma__test_and_set (&((cma__t_int_mutex *)mutex)->lock)) { \
	cma_t_status	res;\
	res = cma__int_mutex_block ((cma__t_int_mutex *)mutex); \
	if (res != cma_s_normal) cma__error (res); \
	} \
    }
#else
# define cma__int_lock(mutex) { \
    cma__t_int_tcb *__ltcb__; \
    __ltcb__ = cma__get_self_tcb (); \
    if (cma__test_and_set (&((cma__t_int_mutex *)mutex)->lock)) { \
	cma_t_status	res;\
	res = cma__int_mutex_block ((cma__t_int_mutex *)mutex); \
	if (res != cma_s_normal) cma__error (res); \
	} \
    ((cma__t_int_mutex *)mutex)->owner = __ltcb__; \
    }
#endif

/*
 *  FUNCTIONAL DESCRIPTION:
 *
 *	Unlock a mutex (internal)
 *
 *  FORMAL PARAMETERS:
 *
 *	mutex		Pointer to mutex object to unlock
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
 *	none
 */
#ifdef NDEBUG
# define cma__int_unlock(mutex) { \
    cma__unset (((cma__t_int_mutex *)mutex)->unlock); \
    if (!cma__test_and_set (&((cma__t_int_mutex *)mutex)->event)) { \
	cma_t_status	res;\
	res = cma__int_mutex_unblock ((cma__t_int_mutex *)mutex); \
	if (res != cma_s_normal) cma__error (res); \
	} \
    }
#else
# define cma__int_unlock(mutex) { \
    cma__t_int_tcb	*__utcb__; \
    __utcb__ = cma__get_self_tcb (); \
    if (((cma__t_int_mutex *)mutex)->mutex_kind == cma_c_mutex_fast) { \
	cma__assert_warn ( \
		(__utcb__ == ((cma__t_int_mutex *)mutex)->owner), \
		"attempt to release mutx owned by another thread"); \
	((cma__t_int_mutex *)mutex)->owner = (cma__t_int_tcb *)cma_c_null_ptr; \
	} \
    cma__unset (((cma__t_int_mutex *)mutex)->unlock); \
    if (!cma__test_and_set (&((cma__t_int_mutex *)mutex)->event)) { \
	cma_t_status	res;\
	res = cma__int_mutex_unblock ((cma__t_int_mutex *)mutex); \
	if (res != cma_s_normal) cma__error (res); \
	} \
    }
#endif

/*
 *  FUNCTIONAL DESCRIPTION:
 *
 *	cma__int_mutex_delete - Performs work for cma_mutex_delete
 *
 *  FORMAL PARAMETERS:
 *
 *	cma__t_mutex	    _mutex_	- Mutex to be deleted
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
 *	none
 */
#define cma__int_mutex_delete(_mutex_) { \
    cma__t_int_mutex    *_int_mutex_; \
    _int_mutex_ = cma__validate_null_mutex (_mutex_); \
    if (_int_mutex_ == (cma__t_int_mutex *)cma_c_null_ptr) \
        return; \
    if (cma__int_mutex_locked (_int_mutex_)) \
        cma__error (cma_s_in_use); \
    cma__free_mutex (_int_mutex_); \
    cma__clear_handle (_mutex_); \
    }


/*
 *  GLOBAL DATA
 */

extern cma__t_sequence	cma__g_mutex_seq;
extern cma__t_int_mutex	*cma__g_global_lock;

/*
 * INTERNAL INTERFACES
 */

extern void cma__destroy_mutex  (cma__t_int_mutex *);

extern void cma__free_mutex  (cma__t_int_mutex *);

extern void cma__free_mutex_nolock  (cma__t_int_mutex *);

extern cma__t_int_mutex * cma__get_first_mutex  (cma__t_int_attr *);

extern cma__t_int_mutex * cma__get_mutex  (cma__t_int_attr *);

extern void cma__init_mutex  (void);

extern cma_t_status cma__int_mutex_block  (cma__t_int_mutex *);

extern cma_t_boolean cma__int_mutex_locked  (cma__t_int_mutex *);

extern cma_t_boolean cma__int_try_lock  (cma__t_int_mutex *);

extern cma_t_status cma__int_mutex_unblock  (cma__t_int_mutex *);

extern cma_t_boolean cma__mutex_locked  (cma_t_mutex);

extern void cma__reinit_mutex  (cma_t_integer);

#endif
