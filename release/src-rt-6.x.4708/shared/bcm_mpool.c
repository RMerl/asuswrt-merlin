/*
 * bcm_mpool.c
 *
 * Implementation of the memory pools component: manager and pools object.
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

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcm_mpool_pub.h>
#include "bcm_mpool_pri.h"

#if defined(BCMDBG) || defined(BCMDBG_ERR)
#define MPOOL_ERROR(args)	printf args
#else
#define MPOOL_ERROR(args)
#endif


static int create_pool(bcm_mpm_mgr_h mgr,
                       unsigned int obj_sz,
                       unsigned int padded_obj_sz,
                       int nobj,
                       void *memstart,
                       const char poolname[BCM_MP_NAMELEN],
                       uint16 type,
                       bcm_mp_pool_h *newp);

static int delete_pool(bcm_mpm_mgr_h mgr, bcm_mp_pool_h *poolp);

static void free_list_init(bcm_mp_free_list_t **list, void *mem,
                           unsigned int obj_sz, int nobj);
static bcm_mp_free_list_t* free_list_remove(bcm_mp_free_list_t **list);
static void free_list_add(bcm_mp_free_list_t **list, bcm_mp_free_list_t *node);


/*
**************************************************************************
*
* API Routines on the pool manager.
*
**************************************************************************
*/

/*
 * bcm_mpm_init() - initialize the whole memory pool system.
 *
 * Parameters:
 *    osh:       INPUT  Operating system handle. Needed for heap memory allocation.
 *    max_pools: INPUT Maximum number of mempools supported.
 *    mgr:       OUTPUT The handle is written with the new pools manager object/handle.
 *
 * Returns:
 *    BCME_OK     Object initialized successfully. May be used.
 *    BCME_NOMEM  Initialization failed due to no memory. Object must not be used.
 */
int BCMATTACHFN(bcm_mpm_init)(osl_t *osh, int max_pools, bcm_mpm_mgr_h *mgrp)
{
	bcm_mpm_mgr_h      mgr = NULL;
	bcm_mp_pool_t      *pool_objs;

	/* Check parameters */
	if ((mgrp == NULL) || (osh == NULL) || (max_pools <= 0)) {
		return (BCME_BADARG);
	}

	/* Allocate memory pool manager object. */
	mgr = (bcm_mpm_mgr_h) MALLOC(osh, (sizeof(*mgr)));
	if (mgr == NULL) {
		return (BCME_NOMEM);
	}

	/* Init memory pool manager object. */
	memset(mgr, 0, sizeof(*mgr));
	mgr->osh = osh;
	mgr->max_pools = (uint16) max_pools;


	/* Pre-allocate pool meta-data (array of bcm_mp_pool structs). */
	pool_objs = (bcm_mp_pool_t *) MALLOC(osh, max_pools * sizeof(bcm_mp_pool_t));
	if (pool_objs == NULL) {
		MFREE(osh, mgr, sizeof(*mgr));
		return (BCME_NOMEM);
	}
	memset(pool_objs, 0, max_pools * sizeof(bcm_mp_pool_t));
	mgr->pool_objs = pool_objs;


	/* Chain all the memory pools onto the manager's free list. */
	STATIC_ASSERT(sizeof(bcm_mp_pool_t) > sizeof(bcm_mp_free_list_t));
	free_list_init(&mgr->free_pools, pool_objs, sizeof(bcm_mp_pool_t), max_pools);

	*mgrp = mgr;
	return (BCME_OK);
}

/*
 * bcm_mpm_deinit() - de-initialize the whole memory pool system. This should only
 *                    be called after all memory pooled have been deleted.
 *
 * Parameters:
 *    mgr:     INPUT  Pointer to pool manager handle.
 *
 * Returns:
 *    BCME_OK  Memory pool manager successfully de-initialized.
 *    other    Indicated error occured during de-initialization.
 */
int BCMATTACHFN(bcm_mpm_deinit)(bcm_mpm_mgr_h *mgrp)
{
	bcm_mpm_mgr_h mgr;
	osl_t         *osh;

	/* Check parameters */
	if ((mgrp == NULL) || (*mgrp == NULL)) {
		return (BCME_BADARG);
	}

	mgr = *mgrp;
	osh = mgr->osh;

	/* Verify that all memory pools have been deleted. */
	if (mgr->npools != 0) {
		MPOOL_ERROR(("ERROR: %s: memory leak - npools(%d)\n",
		             __FUNCTION__, mgr->npools));

		return (BCME_BUSY);
	}

	/* Free the pre-allocated pool meta-data (bcm_mp_pool structs). */
	MFREE(osh, mgr->pool_objs, mgr->max_pools * sizeof(bcm_mp_pool_t));


	/* Free memory pool manager object. */
	MFREE(osh, mgr, sizeof(*mgr));

	*mgrp = NULL;
	return (BCME_OK);
}

/*
 * bcm_mpm_create_prealloc_pool() - Create a new pool for fixed size objects. The
 *                                  pool uses a contiguous block of pre-alloced
 *                                  memory. The memory block may either be provided
 *                                  by the client or dynamically allocated by the
 *                                  pool manager.
 *
 * Parameters:
 *    mgr:      INPUT  The handle to the pool manager
 *    obj_sz:   INPUT  Size of objects that will be allocated by the new pool.
 *                     Must be >= sizeof(void *).
 *    nobj:     INPUT  Maximum number of concurrently existing objects to support
 *    memstart  INPUT  Pointer to the memory to use, or NULL to malloc()
 *    memsize   INPUT  Number of bytes referenced from memstart (for error checking)
 *                     Must be 0 if 'memstart' is NULL.
 *    poolname  INPUT  For instrumentation, the name of the pool
 *    newp:     OUTPUT The handle for the new pool, if creation is successful
 *
 * Returns:
 *    BCME_OK   Pool created ok.
 *    other     Pool not created due to indicated error. newpoolp set to NULL.
 *
 *
 */
int BCMATTACHFN(bcm_mpm_create_prealloc_pool)(bcm_mpm_mgr_h mgr,
                                              unsigned int obj_sz,
                                              int nobj,
                                              void *memstart,
                                              unsigned int memsize,
                                              const char poolname[BCM_MP_NAMELEN],
                                              bcm_mp_pool_h *newp)
{
	int ret;
	unsigned int padded_obj_sz;

	/* Check parameters */
	if ((nobj == 0) ||
	    (memstart && !memsize) ||
	    (memsize && !memstart)) {
		return (BCME_BADARG);
	}


	/* Get padded object size. */
	ret = bcm_mpm_get_obj_size(mgr, obj_sz, &padded_obj_sz);
	if (ret != BCME_OK) {
		return (ret);
	}


	/* For client allocated memory, validate memory block size and alignment. */
	if (memstart != NULL) {
		if ((memsize < (padded_obj_sz * nobj)) || (!ISALIGNED(memstart, sizeof(void *)))) {
			return (BCME_BADARG);
		}
	}

	return (create_pool(mgr, obj_sz, padded_obj_sz, nobj, memstart, poolname,
	                    BCM_MP_TYPE_PREALLOC, newp));
}


/*
 * bcm_mpm_delete_prealloc_pool() - Delete a memory pool. This should only be called after
 *                                  all memory objects have been freed back to the pool.
 *
 * Parameters:
 *    mgr:     INPUT The handle to the pools manager
 *    pool:    INPUT The handle of the  pool to delete
 *
 * Returns:
 *    BCME_OK   Pool deleted ok.
 *    other     Pool not deleted due to indicated error.
 *
 */
int BCMATTACHFN(bcm_mpm_delete_prealloc_pool)(bcm_mpm_mgr_h mgr, bcm_mp_pool_h *poolp)
{
	return (delete_pool(mgr, poolp));
}


/*
 * create_pool() - Create a new pool for fixed size objects. Helper function
 *                         common to all memory pool types.
 *
 * Parameters:
 *    mgr:           INPUT  The handle to the pool manager
 *    obj_sz:        INPUT  Size of objects that will be allocated by the new pool.
 *                          This is the size requested by the user. The actual size
 *                          of the memory object may be padded.
 *                          Must be > 0 for heap pools. Must be >= sizeof(void *) for
 *                          Prealloc pools.
 *    padded_obj_sz: INPUT  Size of objects that will be allocated by the new pool.
 *                          This is the actual size of memory objects. It is 'obj_sz'
 *                          plus optional padding required for alignment.
 *                          Must be > 0 for heap pools. Must be >= sizeof(void *) for
 *                          Prealloc pools.
 *    nobj:          INPUT  Maximum number of concurrently existing objects to support.
 *                          Must be specified for Prealloc pool. Ignored for heap pools.
 *    memstart       INPUT  Pointer to the memory to use, or NULL to malloc().
 *                          Ignored for heap pools.
 *    memsize        INPUT  Number of bytes referenced from memstart (for error checking).
 *                          Must be 0 if 'memstart' is NULL. Ignored for heap pools.
 *    poolname       INPUT  For instrumentation, the name of the pool
 *    type           INPUT  Pool type - BCM_MP_TYPE_xxx.
 *    newp:          OUTPUT The handle for the new pool, if creation is successful
 *
 * Returns:
 *    BCME_OK   Pool created ok.
 *    other     Pool not created due to indicated error. newpoolp set to NULL.
 *
 *
 */
static int BCMATTACHFN(create_pool)(bcm_mpm_mgr_h mgr,
                                    unsigned int obj_sz,
                                    unsigned int padded_obj_sz,
                                    int nobj,
                                    void *memstart,
                                    const char poolname[BCM_MP_NAMELEN],
                                    uint16 type,
                                    bcm_mp_pool_h *newp)
{
	bcm_mp_pool_t      *mem_pool = NULL;
	void               *malloc_memstart = NULL;

	/* Check parameters */
	if ((mgr == NULL) ||
	    (newp == NULL) ||
	    (obj_sz == 0) ||
	    (padded_obj_sz == 0) ||
	    (poolname == NULL) ||
	    (poolname[0] == '\0')) {
		return (BCME_BADARG);
	}

	/* Allocate memory object from pool. */
	mem_pool = (bcm_mp_pool_t *) free_list_remove(&mgr->free_pools);
	if (mem_pool == NULL) {
		return (BCME_NOMEM);
	}

	/* For pool manager allocated memory, malloc the contiguous memory
	 * block of objects.
	 */
	if ((type == BCM_MP_TYPE_PREALLOC) && (memstart == NULL)) {
		memstart = MALLOC(mgr->osh, padded_obj_sz * nobj);
		if (memstart == NULL) {
			free_list_add(&mgr->free_pools,
			                     (bcm_mp_free_list_t *) mem_pool);
			return (BCME_NOMEM);
		}
		malloc_memstart = memstart;
	}

	/* Init memory pool object (common). */
	memset(mem_pool, 0, sizeof(*mem_pool));
	mem_pool->objsz = obj_sz;
	BCM_MP_SET_POOL_TYPE(mem_pool, type);
	BCM_MP_SET_IN_USE(mem_pool);
	strncpy(mem_pool->name, poolname, sizeof(mem_pool->name));
	mem_pool->name[sizeof(mem_pool->name)-1] = '\0';


	if (type == BCM_MP_TYPE_PREALLOC) {
		/* Init memory pool object (Prealloc specific). */
		mem_pool->u.p.nobj            = (uint16) nobj;
		mem_pool->u.p.malloc_memstart = malloc_memstart;
		mem_pool->u.p.padded_objsz    = padded_obj_sz;

		/* Chain all the memory objects onto the pool's free list. */
		free_list_init(&mem_pool->u.p.free_objp, memstart, padded_obj_sz, nobj);
	}
	else {
		/* Init memory pool object (Heap specific). */
		mem_pool->u.h.osh = mgr->osh;
	}

	mgr->npools++;
	*newp = mem_pool;
	return (BCME_OK);
}


/*
 * delete_pool() - Delete a memory pool. Helper function common to all
 *                         memory pool types.
 *
 * Parameters:
 *    mgr:     INPUT The handle to the pools manager
 *    pool:    INPUT The handle of the  pool to delete
 *
 * Returns:
 *    BCME_OK   Pool deleted ok.
 *    other     Pool not deleted due to indicated error.
 *
 */
static int BCMATTACHFN(delete_pool)(bcm_mpm_mgr_h mgr, bcm_mp_pool_h *poolp)
{
	bcm_mp_pool_h      pool;

	/* Check parameters */
	if ((mgr == NULL) || (poolp == NULL) || (*poolp == NULL)) {
		return (BCME_BADARG);
	}


	/* Verify that all memory objects have been freed back to the pool. */
	pool = *poolp;
	if (pool->num_alloc != 0) {
		MPOOL_ERROR(("ERROR: %s: memory leak - nobj(%d) num_alloc(%d)\n",
		       __FUNCTION__, pool->u.p.nobj, pool->num_alloc));

		return (BCME_BUSY);
	}


	/* For pool manager allocated memory, free the contiguous block of memory objects. */
	if ((BCM_MP_GET_POOL_TYPE(pool) == BCM_MP_TYPE_PREALLOC) &&
	    (pool->u.p.malloc_memstart != NULL)) {
		MFREE(mgr->osh, pool->u.p.malloc_memstart,
		      pool->u.p.nobj * pool->u.p.padded_objsz);
	}


	/* Mark the memory pool as unused. */
	BCM_MP_CLEAR_IN_USE(pool);


	/* Add object memory back to pool. */
	free_list_add(&mgr->free_pools, (bcm_mp_free_list_t *) pool);

	mgr->npools--;


	*poolp = NULL;
	return (BCME_OK);
}


/*
 * bcm_mpm_create_heap_pool() - Create a new pool for fixed size objects. The memory
 *                              pool allocator uses the heap (malloc/free) for memory.
 *                              In this case, the pool allocator is just providing
 *                              statistics and instrumentation on top of the heap,
 *                              without modifying the heap allocation implementation.
 *
 * Parameters:
 *    mgr:      INPUT  The handle to the pool manager
 *    obj_sz:   INPUT  Size of objects that will be allocated by the new pool
 *    poolname  INPUT  For instrumentation, the name of the pool
 *    newp:     OUTPUT The handle for the new pool, if creation is successful
 *
 * Returns:
 *    BCME_OK   Pool created ok.
 *    other     Pool not created due to indicated error. newpoolp set to NULL.
 *
 *
 */
int BCMATTACHFN(bcm_mpm_create_heap_pool)(bcm_mpm_mgr_h mgr,
                                          unsigned int obj_sz,
                                          const char poolname[BCM_MP_NAMELEN],
                                          bcm_mp_pool_h *newp)
{
	return (create_pool(mgr, obj_sz, obj_sz, 0, NULL, poolname,
	                    BCM_MP_TYPE_HEAP, newp));
}


/*
 * bcm_mpm_delete_heap_pool() - Delete a memory pool. This should only be called after
 *                              all memory objects have been freed back to the pool.
 *
 * Parameters:
 *    mgr:     INPUT The handle to the pools manager
 *    pool:    INPUT The handle of the  pool to delete
 *
 * Returns:
 *    BCME_OK   Pool deleted ok.
 *    other     Pool not deleted due to indicated error.
 *
 */
int BCMATTACHFN(bcm_mpm_delete_heap_pool)(bcm_mpm_mgr_h mgr, bcm_mp_pool_h *poolp)
{
	return (delete_pool(mgr, poolp));
}


/*
 * bcm_mpm_stats() - Return stats for all pools
 *
 * Parameters:
 *    mgr:         INPUT   The handle to the pools manager
 *    stats:       OUTPUT  Array of pool statistics.
 *    nentries:    MOD     Max elements in 'stats' array on INPUT. Actual number
 *                         of array elements copied to 'stats' on OUTPUT.
 *
 * Returns:
 *    BCME_OK   Ok
 *    other     Error getting stats.
 *
 */
int bcm_mpm_stats(bcm_mpm_mgr_h mgr, bcm_mp_stats_t *stats, int *nentries)
{
	bcm_mp_pool_t  *pool;
	uint16         i;
	int            nentries_remaining = *nentries;


	/* Check parameters */
	ASSERT(mgr != NULL);
	ASSERT(stats != NULL);

	if ((*nentries) < mgr->npools) {
		return (BCME_BUFTOOSHORT);
	}

	/* Iterate list of pools, and get stats. */
	for (i = 0; i < mgr->max_pools; i++) {
		pool = &mgr->pool_objs[i];
		if ((BCM_MP_IS_IN_USE(pool)) && (nentries_remaining >= 0)) {
			bcm_mp_stats(pool, stats);
			stats++;
			nentries_remaining--;
		}
	}

	/* Internal consistency check. */
	ASSERT(mgr->npools == (*nentries - nentries_remaining));


	*nentries = mgr->npools;
	return (BCME_OK);
}


/*
 * bcm_mpm_dump() - Display statistics on all extant pools
 *
 * Parameters:
 *    mgr:     INPUT  The handle to the pools manager
 *    b:       OUTPUT Output buffer.
 *
 * Returns:
 *    BCME_OK   Ok
 *    other     Error during dump.
 *
 */
int bcm_mpm_dump(bcm_mpm_mgr_h mgr, struct bcmstrbuf *b)
{
#if defined(BCMDBG)
	bcm_mp_pool_t  *pool;
	uint16         i;

	/* Check parameters */
	ASSERT(mgr != NULL);
	ASSERT(b != NULL);

	/* Iterate list of pools, and dump. */
	bcm_bprintf(b, "\nDump pools (%d):\n", mgr->npools);
	bcm_bprintf(b, "%-8s %10s %5s %5s %5s %5s %6s %5s  %5s %10s %10s\n", "Name",
	            "Hdl", "SZ", "Curr", "HiWtr", "Fail", "Flags", "P_SZ", "Max",
	            "Freep", "Malloc");

	for (i = 0; i < mgr->max_pools; i++) {
		pool = &mgr->pool_objs[i];
		if (BCM_MP_IS_IN_USE(pool)) {
			bcm_mp_dump(pool, b);
		}
	}


#endif   
	return (BCME_OK);
}


/*
 * bcm_mpm_get_obj_size() - The size of memory objects may need to be padded to
 *                          compensate for alignment requirements of the objects.
 *                          This function provides the padded object size. If clients
 *                          pre-allocate a memory slab for a memory pool, the
 *                          padded object size should be used by the client to allocate
 *                          the memory slab (in order to provide sufficent space for
 *                          the maximum number of objects).
 *
 * Parameters:
 *    mgr:            INPUT   The handle to the pools manager.
 *    obj_sz:         INPUT   Input object size.
 *    padded_obj_sz:  OUTPUT  Padded object size.
 *
 * Returns:
 *    BCME_OK      Ok
 *    BCME_BADARG  Bad arguments.
 *
 */
int BCMATTACHFN(bcm_mpm_get_obj_size)(bcm_mpm_mgr_h mgr, unsigned int obj_sz,
                                      unsigned int *padded_obj_sz)
{
	UNUSED_PARAMETER(mgr);

	/* Check parameters */
	if (obj_sz == 0) {
		return (BCME_BADARG);
	}


	/* A linked-list of free memory objects is maintained by the memory pool.
	 * While free, the contents of the memory object body itself is used for the
	 * linked-list pointers. (Therefore no per-memory-object meta-data is
	 * maintained).
	 *
	 * If the requested memory object size is less than the size of free-list
	 * pointer, pad the object size.
	 *
	 * Also, if the requested memory object size is not pointer aligned, pad
	 * the object size so that the "next" free-list pointers are aligned correctly.
	 */
	if (obj_sz < sizeof(bcm_mp_free_list_t)) {
		*padded_obj_sz = sizeof(bcm_mp_free_list_t);
	}
	else {
		*padded_obj_sz = ALIGN_SIZE(obj_sz, sizeof(void*));
	}

	return (BCME_OK);
}


/*
***************************************************************************
*
* API Routines on a specific pool.
*
***************************************************************************
*/


/*
 * bcm_mp_alloc() - Allocate a memory pool object.
 *
 * Parameters:
 *    pool:    INPUT    The handle to the pool.
 *
 * Returns:
 *    A pointer to the new object. NULL on error.
 *
 */
void* bcm_mp_alloc(bcm_mp_pool_h pool)
{
	bcm_mp_free_list_t *obj_hdr;
	void               *objp = NULL;

	/* Check parameters */
	ASSERT(pool != NULL);


	if (BCM_MP_GET_POOL_TYPE(pool) == BCM_MP_TYPE_PREALLOC) {
		if (pool->u.p.free_objp) {
			/* Allocate memory object from pool. */
			obj_hdr = free_list_remove(&pool->u.p.free_objp);

			/* Clear new object header */
			memset(obj_hdr, 0, sizeof(*obj_hdr));
			objp = obj_hdr;
		}
	}
	else {
		/* Heap allocation memory pool. */
		objp = MALLOC(pool->u.h.osh, pool->objsz);
	}

	/* Update pool state. */
	if (objp) {
		pool->num_alloc++;
		if (pool->num_alloc > pool->high_water) {
			pool->high_water = pool->num_alloc;
		}
	} else {
		pool->failed_alloc++;
	}

	return (objp);
}

/*
 * bcm_mp_free() - Free a memory pool object.
 *
 * Parameters:
 *    pool:  INPUT   The handle to the pool.
 *    objp:  INPUT   A pointer to the object to free.
 *
 * Returns:
 *    BCME_OK   Ok
 *    other     Error during free.
 *
 */
int bcm_mp_free(bcm_mp_pool_h pool, void *objp)
{
	/* Check parameters */
	ASSERT(pool != NULL);

	if (objp == NULL) {
		return (BCME_BADARG);
	}

	if (BCM_MP_GET_POOL_TYPE(pool) == BCM_MP_TYPE_PREALLOC) {
		/* Add object memory back to pool. */
		free_list_add(&pool->u.p.free_objp, (bcm_mp_free_list_t *) objp);
	}
	else {
		/* Heap allocation memory pool. */
		MFREE(pool->u.h.osh, objp, pool->objsz);
	}

	/* Update pool state. */
	pool->num_alloc--;

	return (BCME_OK);
}

/*
 * bcm_mp_stats() - Return stats for this pool
 *
 * Parameters:
 *    pool:     INPUT    The handle to the pool
 *    stats:    OUTPUT   Pool statistics
 *
 * Returns:
 *    BCME_OK   Ok
 *    other     Error during dump.
 *
 */
int bcm_mp_stats(bcm_mp_pool_h pool, bcm_mp_stats_t *stats)
{
	/* Check parameters */
	ASSERT(pool != NULL);
	ASSERT(stats != NULL);

	memset(stats, 0, sizeof(*stats));
	stats->objsz        = pool->objsz;
	stats->num_alloc    = pool->num_alloc;
	stats->high_water   = pool->high_water;
	stats->failed_alloc = pool->failed_alloc;
	strncpy(stats->name, pool->name, sizeof(stats->name));
	stats->name[sizeof(stats->name) - 1] = '\0';

	if (BCM_MP_GET_POOL_TYPE(pool) == BCM_MP_TYPE_PREALLOC) {
		stats->nobj = pool->u.p.nobj;
	}

	return (BCME_OK);
}

/*
 * bcm_mp_dump() - Dump a pool
 *
 * Parameters:
 *    pool:    INPUT    The handle to the pool
 *    b        OUTPUT   Output buffer
 *
 * Returns:
 *    BCME_OK   Ok
 *    other     Error during dump.
 *
 */
int bcm_mp_dump(bcm_mp_pool_h pool, struct bcmstrbuf *b)
{
#if defined(BCMDBG)
	/* Check parameters */
	ASSERT(pool != NULL);
	ASSERT(b != NULL);

	bcm_bprintf(b, "%-8s 0x%08p %5d %5d %5d %5d 0x%04d", pool->name, pool, pool->objsz,
	            pool->num_alloc, pool->high_water, pool->failed_alloc, pool->flags);

	if (BCM_MP_GET_POOL_TYPE(pool) == BCM_MP_TYPE_PREALLOC) {
		bcm_mp_prealloc_pool_t *prealloc = &pool->u.p;

		bcm_bprintf(b, " %5d %5d 0x%08p 0x%08p", prealloc->padded_objsz,
		            prealloc->nobj, prealloc->free_objp, prealloc->malloc_memstart);
	}
	bcm_bprintf(b, "\n");

#endif   

	return (BCME_OK);
}


/*
 * free_list_init() - Initialize a free object list. Chain together an
 *                           array of objects into a linked-list.
 *
 * Parameters:
 *    list    OUTPUT List to intialize.
 *    mem     INPUT  Array of objects to link together.
 *    obj_sz  INPUT  Size of each object in the array.
 *    nobj    INPUT  Number of objects in array.
 *
 * Returns:
 *    Nothing.
 *
 */
static void BCMATTACHFN(free_list_init)(bcm_mp_free_list_t **list,
                                        void *mem,
                                        unsigned int obj_sz,
                                        int nobj)
{
	bcm_mp_free_list_t *prevp;
	bcm_mp_free_list_t *freep;
	int                idx;

	prevp = NULL;
	freep = (bcm_mp_free_list_t *)mem;
	for (idx = 0; idx < nobj; idx++) {
		memset(freep, 0, sizeof(*freep));
		freep->next = prevp;
		prevp = freep;
		freep = (bcm_mp_free_list_t *)((char *)(freep) + obj_sz);
	}
	*list = prevp;
}


/*
 * free_list_remove() - Remove object from the front of the free list.
 *
 * Parameters:
 *    list  MOD  List to remove object from.
 *
 * Returns:
 *    Removed object. NULL if no free object available.
 *
 */
static bcm_mp_free_list_t* free_list_remove(bcm_mp_free_list_t **list)
{
	bcm_mp_free_list_t *free_node = NULL;

	if (*list != NULL) {
		free_node = *list;
		*list = (*list)->next;
	}
	return (free_node);
}


/*
 * free_list_add() - Add object to the front of the free list.
 *
 * Parameters:
 *    list  MOD    List to add object to.
 *    node  INPUT  Object to add to list.
 *
 * Returns:
 *    Nothing.
 *
 */
static void free_list_add(bcm_mp_free_list_t **list, bcm_mp_free_list_t *node)
{
	/* Add object memory back to pool. */
	memset(node, 0, sizeof(*node));
	node->next = *list;
	*list = node;
}
