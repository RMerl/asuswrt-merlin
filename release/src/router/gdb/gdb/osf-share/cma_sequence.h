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
 *	Header file for sequence generator functions
 */

#ifndef CMA_SEQUENCE
#define CMA_SEQUENCE

/*
 *  INCLUDE FILES
 */

/*
 * CONSTANTS AND MACROS
 */

/*
 * TYPEDEFS
 */

#ifndef __STDC__
struct CMA__T_INT_MUTEX;
#endif

typedef struct CMA__T_SEQUENCE {
    struct CMA__T_INT_MUTEX	*mutex;	/* Serialize access to counter */
    cma_t_natural		seq;	/* Sequence number for object */
    } cma__t_sequence;

/*
 *  GLOBAL DATA
 */

/*
 * INTERNAL INTERFACES
 */

extern cma_t_natural cma__assign_sequence (cma__t_sequence *);

extern void cma__init_sequence (cma__t_sequence *);	

#endif
