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
 *	Header file for semaphore structure definition.
 */
#ifndef CMA_SEMAPHORE_DEFS
#define CMA_SEMAPHORE_DEFS

/*
 *  INCLUDE FILES
 */
#include <cma.h>
#include <cma_queue.h>
#include <cma_defs.h>

/*
 * CONSTANTS AND MACROS
 */

#define cma__c_semaphore_timeout 1
#define cma__c_semaphore_event	 0
#define cma__c_select_timeout	 2

/*
 * TYPEDEFS
 */

typedef struct CMA__T_SEMAPHORE {
    cma__t_queue	queue;
    cma__t_atomic_bit	nopending;
    } cma__t_semaphore;

#endif
