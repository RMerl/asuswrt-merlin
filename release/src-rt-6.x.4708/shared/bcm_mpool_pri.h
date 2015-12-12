/*
 *
 * Memory pool type definitions
 *
 * Prealloc memory pools:
 *    These memory pools are memory allocators based on subdividing a single
 *    block of memory into homogenous objects. It's suitable for data structures
 *    with a statically configured maximum # of objects, such as using tunable
 *    parameters. It's not suitable for heterogenous data structures nor for
 *    data structures where the maximum count is unbounded.
 *
 *    Since the memory for all the objects is preallocated during initialization,
 *    that memory is unavailable to the rest of the system. This is appropriate both for
 *    small data structures, and for medium/large data structures where the peak and average
 *    utilization are similar. For data structures where average utilization is low but peak
 *    is very high, it's better to use the real heap so the memory gets freed when it's
 *    not in use.
 *
 *    Each individual memory pool can obtain it's memory from the user/client
 *    code or from the heap, as desired.
 *
 *
 * Heap memory pools:
 *    The memory pool allocator uses the heap (malloc/free) for memory.
 *    In this case, the pool allocator is just providing statistics and instrumentation
 *    on top of the heap, without modifying the heap allocation implementation.
 *
 * The memory pool component consists of two major abstractions:
 *     Manager: the factory for memory pools
 *     Pool: an individual pool that can allocate objects
 *
 * The memory pool manager maintains an array of individual pools so that instrumentation
 * can examine and manage all of them globally.
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
 */

/*
 *
 *  List Manager
 *  +------------+
 *  | free_pools |------------------------------------+
 *  +------------+                                    |
 *  | pool_objs  |>+                                  |
 *  +------------+ |                                  |
 *  | npools     | |                                  |
 *  +------------+ |                                  |
 *  | max_pools  | |                                  |
 *  +------------+ |                                  |
 *  | osh        | |                                  |
 *  +------------+ |                                  |
 *                 |                                  |
 *                 |                                  |
 *                 |                                  |
 *                 |                                  |
 *                 v                                  v
 *              +--------------++--------------++--------------++--------------+
 *              | name         || name         || name         || name         |
 *       Meta   +--------------++--------------++--------------++--------------+
 *       Data   | objsz        || objsz        || objsz        || objsz        |
 *              +--------------++--------------++--------------++--------------+
 *              | num_alloc    || num_alloc    || num_alloc    || num_alloc    |
 *              +--------------++--------------++--------------++--------------+
 *              | high_water   || high_water   || high_water   || high_water   |
 *              +--------------++--------------++--------------++--------------+
 *              | failed_alloc || failed_alloc || failed_alloc || failed_alloc |
 *              +--------------++--------------++--------------++--------------+
 *              | flags (used) || flags (used) || flags (free) || flags (used) |
 *              +--------------++--------------++--------------++--------------+
 *              | nobj         || nobj         || nobj         || nobj         |
 *              +--------------++--------------++--------------++--------------+
 *              | memstart     || memstart     || memstart     || memstart     |
 *              +--------------++--------------++--------------++--------------+
 *              | freep        || freep        || freep        || freep        |
 *              +--------------++--------------++--------------++--------------+
 *                     |               |                                |
 *                     |               |                                |
 *                     v               v                                v
 *                 +-------+       +-------+                        +-------+
 *                 |       |       |       |                        |       |
 *                 | obj1  |       | obj1  |                        | obj1  |
 *                 |       |       |       |                        |       |
 *                 +-------+       +-------+                        +-------+
 *                     .               .                                .
 *                     .               .                                .
 *                     .               .                                .
 *                 +-------+       +-------+                        +-------+
 *                 |       |       |       |                        |       |
 *                 |       |       | objM  |                        |       |
 *                 |       |       |       |                        |       |
 *                 +-------+       +-------+                        +-------+
 *                 |       |                                        |       |
 *                 |       |                                        |       |
 *                 |       |                                        |       |
 *                 +-------+                                        +-------+
 *                 |       |                                        |       |
 *                 |       |                                        |       |
 *                 |       |                                        |       |
 *                 +-------+                                        +-------+
 *                 |       |                                        |       |
 *                 | objN  |                                        |       |
 *                 |       |                                        |       |
 *                 +-------+                                        +-------+
 *                                                                  |       |
 *                                                                  | objP  |
 *                                                                  |       |
 *                                                                  +-------+
 *
 */

#ifndef _BCM_MPOOL_PRI_H
#define _BCM_MPOOL_PRI_H 1


#include "osl.h"
#include "typedefs.h"
#include "bcm_mpool_pub.h"


/*
 * Pool definitions.
 */


/*
 * This structure is used to maintain a list of free objects.
 */
typedef struct bcm_mp_free_list {
	struct bcm_mp_free_list *next;  /* Next pointer for the linked-list */
} bcm_mp_free_list_t;


/*
 * Type definition for an instance data specific to a prealloc pool.
 */
typedef struct bcm_mp_prealloc_pool {
	/* Pre-alloc pool specific state. */
	uint16             nobj;             /* Total number of objects in this pool */
	unsigned int       padded_objsz;     /* Padded object size allocated in this pool */
	void               *malloc_memstart; /* Contiguous block of malloced memory objects. */
	bcm_mp_free_list_t *free_objp;       /* Free list of memory objects. */
} bcm_mp_prealloc_pool_t;


/*
 * Type definition for an instance data specific to a heap pool.
 */
typedef struct bcm_mp_heap_pool {
	/* Heap pool specific state. */
	osl_t *osh;   /* OSH for malloc and free */
} bcm_mp_heap_pool_t;


/*
 * Type definition for an instance of a pool. This handle is used for allocating
 * and freeing memory through the pool, as well as management/instrumentation on this
 * specific pool.
 */
typedef struct bcm_mp_pool {
	/* Common state. */
	char   name[BCM_MP_NAMELEN];      /* Name of this pool. */
	unsigned int objsz;               /* Object size allocated in this pool */
	uint16 num_alloc;                 /* Number of objects currently allocated */
	uint16 high_water;                /* Max number of allocated objects. */
	uint16 failed_alloc;              /* Failed allocations. */
	uint16 flags;                     /* Flags */

	/* State specific to pool-type. */
	union {
		bcm_mp_prealloc_pool_t p;  /* Prealloc pool. */
		bcm_mp_heap_pool_t     h;  /* Heap pool. */
	} u;
} bcm_mp_pool_t;


/*
 * Flags definitions for 'flags' structure member of 'bcm_mp_pool'.
 */

/* Bits for pool-type. */
#define BCM_MP_TYPE_MASK     0x0003 /* Mask for the bit position of pool type */
#define BCM_MP_TYPE_HEAP     0      /* Bit equals zero means heap pool type */
#define BCM_MP_TYPE_PREALLOC 1      /* Bit equals one means pre-alloc pool type */

/* Accessor functions for pool-type. */
#define BCM_MP_GET_POOL_TYPE(pool) (((pool)->flags) & BCM_MP_TYPE_MASK)
#define BCM_MP_SET_POOL_TYPE(pool, type) ((pool)->flags = ((pool)->flags & ~BCM_MP_TYPE_MASK) \
	                                                   | (type))


/* Bits in indicate if memory pool is in-use. */
#define BCM_MP_POOL_IN_USE_MASK 0x0004

/* Bits in indicate if memory pool is in use. */
#define BCM_MP_SET_IN_USE(pool) ((pool)->flags |= BCM_MP_POOL_IN_USE_MASK)
#define BCM_MP_CLEAR_IN_USE(pool) ((pool)->flags &= ~BCM_MP_POOL_IN_USE_MASK)
#define BCM_MP_IS_IN_USE(pool) (((pool)->flags & BCM_MP_POOL_IN_USE_MASK) == \
	                                         BCM_MP_POOL_IN_USE_MASK)


/*
 * Manager definition. Used to create new pools and for global instrumentation.
 */
typedef struct bcm_mpm_mgr {
	bcm_mp_pool_t      *pool_objs;   /* Array of malloced memory pool objects. */
	bcm_mp_free_list_t *free_pools;  /* Free list of memory pools. */
	uint16             npools;       /* Number of pools currently allocated */
	uint16             max_pools;    /* Max number of pools  */
	osl_t              *osh;         /* OSH for malloc and free */
} bcm_mpm_mgr_t;


#endif /* _BCM_MPOOL_PRI_H */
