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
 *	Header file for stack management
 */
#ifndef CMA_STACK
#define CMA_STACK

/*
 *  INCLUDE FILES
 */

#include <cma_tcb_defs.h>
#include <cma.h>
#include <cma_attr.h>
#include <cma_queue.h>
#include <cma_stack_int.h>

/*
 * CONSTANTS AND MACROS
 */

#if _CMA_UNIPROCESSOR_
# define cma__get_self_tcb()	(cma__g_current_thread)
#endif

/*
 * Round the given value (a) upto cma__g_chunk_size
 */
#define cma__roundup_chunksize(a)   (cma__roundup(a,cma__g_chunk_size))

/*
 * TYPEDEFS
 */

/*
 *  GLOBAL DATA
 */

extern cma__t_list	cma__g_stack_clusters;
extern cma__t_int_tcb	*cma__g_current_thread;
extern cma_t_integer	cma__g_chunk_size;

/*
 * INTERNAL INTERFACES
 */

extern void cma__assign_stack  (cma__t_int_stack *,cma__t_int_tcb *);

extern void cma__free_stack  (cma__t_int_stack *);

extern void cma__free_stack_list  (cma__t_queue *);

#if !_CMA_UNIPROCESSOR_
extern cma__t_int_tcb * cma__get_self_tcb  (void);
#endif

extern cma__t_int_tcb * cma__get_sp_tcb  (cma_t_address);

extern cma__t_int_stack * cma__get_stack  (cma__t_int_attr *);

extern void cma__init_stack  (void);

extern void cma__reinit_stack  (cma_t_integer);

#if _CMA_PROTECT_MEMORY_
extern void cma__remap_stack_holes  (void);
#endif

#endif
