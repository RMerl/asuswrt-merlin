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
 *	Header file for stack management (internal to cma_stack.c, but
 *	separate for convenience, and unit testing).
 */

#ifndef CMA_STACK_INT
#define CMA_STACK_INT

/*
 *  INCLUDE FILES
 */

#include <cma.h>
#include <cma_queue.h>
#include <cma_list.h>
#include <cma_tcb_defs.h>

/*
 * CONSTANTS AND MACROS
 */

#define cma___c_first_free_chunk	0
#define cma___c_min_count	2	/* Smallest number of chunks to leave */
#define cma___c_end		(-1)	/* End of free list (flag) */
#define cma__c_yellow_size	0

/* 
 * Cluster types
 */
#define cma___c_cluster  0	/* Default cluster */
#define cma___c_bigstack 1	/* Looks like a cluster, but it's a stack */


#define cma___c_null_cluster	(cma___t_cluster *)cma_c_null_ptr


/*
 * TYPEDEFS
 */

#ifndef __STDC__
struct CMA__T_INT_STACK;
#endif

typedef cma_t_natural	cma___t_index;	/* Type for chunk index */

typedef struct CMA___T_CLU_DESC {
    cma__t_list		list;		/* Queue element for cluster list */
    cma_t_integer	type;		/* Type of cluster */
    cma_t_address	stacks;
    cma_t_address	limit;
    } cma___t_clu_desc;

typedef union CMA___T_MAP_ENTRY {
    struct {
	cma__t_int_tcb	*tcb;		/* TCB associated with stack chunk */
	struct CMA__T_INT_STACK	*stack;	/* Stack desc. ass. with stack chunk */
	} mapped;
    struct {
	cma___t_index		size;	/* Number of chunks in block */
	cma___t_index		next;	/* Next free block */
	} free;
    } cma___t_map_entry;

/*
 * NOTE: It is VERY IMPORTANT that both cma___t_cluster and cma___t_bigstack
 * begin with the cma___t_clu_desc structure, as there is some code in the
 * stack manager that relies on being able to treat both as equivalent!
 */
typedef struct CMA___T_CLUSTER {
    cma___t_clu_desc	desc;		/* Describe this cluster */
    cma___t_map_entry	map[cma__c_chunk_count];	/* thread map */
    cma___t_index	free;		/* First free chunk index */
    } cma___t_cluster;

/*
 * NOTE: It is VERY IMPORTANT that both cma___t_cluster and cma___t_bigstack
 * begin with the cma___t_clu_desc structure, as there is some code in the
 * stack manager that relies on being able to treat both as equivalent!
 */
typedef struct CMA___T_BIGSTACK {
    cma___t_clu_desc	desc;		/* Describe this cluster */
    cma__t_int_tcb	*tcb;		/* TCB associated with stack */
    struct CMA__T_INT_STACK	*stack;	/* Stack desc. ass. with stack */
    cma_t_natural	size;		/* Size of big stack */
    cma_t_boolean	in_use;		/* Set if allocated */
    } cma___t_bigstack;

#if _CMA_PROTECT_MEMORY_
typedef struct CMA___T_INT_HOLE {
    cma__t_queue	link;		/* Link holes together */
    cma_t_boolean	protected;	/* Set when pages are protected */
    cma_t_address	first;		/* First protected byte */
    cma_t_address	last;		/* Last protected byte */
    } cma___t_int_hole;
#endif

typedef struct CMA__T_INT_STACK {
    cma__t_object	header;		/* Common header (sequence, type info */
    cma__t_int_attr	*attributes;	/* Backpointer to attr obj */
    cma___t_cluster	*cluster;	/* Stack's cluster */
    cma_t_address	stack_base;	/* base address of stack */
    cma_t_address	yellow_zone;	/* first address of yellow zone */
    cma_t_address	last_guard;	/* last address of guard pages */
    cma_t_natural	first_chunk;	/* First chunk allocated */
    cma_t_natural	chunk_count;	/* Count of chunks allocated */
    cma__t_int_tcb	*tcb;		/* TCB backpointer */
#if _CMA_PROTECT_MEMORY_
    cma___t_int_hole	hole;		/* Description of hole */
#endif
    } cma__t_int_stack;

/*
 *  GLOBAL DATA
 */

/*
 * INTERNAL INTERFACES
 */

#endif
