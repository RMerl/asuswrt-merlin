/* $Id: pool_caching.c 4395 2013-02-27 12:07:30Z ming $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pj/pool.h>
#include <pj/log.h>
#include <pj/string.h>
#include <pj/assert.h>
#include <pj/lock.h>
#include <pj/os.h>
#include <pj/pool_buf.h>

#if !PJ_HAS_POOL_ALT_API

static pj_pool_t* cpool_create_pool(pj_pool_factory *pf, 
				    const char *name,
				    pj_size_t initial_size, 
				    pj_size_t increment_sz,
				    pj_pool_callback *callback);
static void cpool_release_pool(pj_pool_factory *pf, pj_pool_t *pool);
static void cpool_dump_status(pj_pool_factory *factory, pj_bool_t detail );
static pj_bool_t cpool_on_block_alloc(pj_pool_factory *f, pj_size_t sz);
static void cpool_on_block_free(pj_pool_factory *f, pj_size_t sz);


static pj_size_t pool_sizes[PJ_CACHING_POOL_ARRAY_SIZE] = 
{
    256, 512, 1024, 2048, 4096, 8192, 12288, 16384, 
    20480, 24576, 28672, 32768, 40960, 49152, 57344, 65536
};

/* Index where the search for size should begin.
 * Start with pool_sizes[5], which is 8192.
 */
#define START_SIZE  5


PJ_DEF(void) pj_caching_pool_init( int inst_id, pj_caching_pool *cp, 
				   const pj_pool_factory_policy *policy,
				   pj_size_t max_capacity)
{
    int i;
    pj_pool_t *pool;

    PJ_CHECK_STACK();

    pj_bzero(cp, sizeof(*cp));
    
	cp->inst_id = inst_id;
    cp->max_capacity = max_capacity;
    pj_list_init(&cp->used_list);
    for (i=0; i<PJ_CACHING_POOL_ARRAY_SIZE; ++i)
	pj_list_init(&cp->free_list[i]);

    if (policy == NULL) {
    	policy = &pj_pool_factory_default_policy;
    }
    
    pj_memcpy(&cp->factory.policy, policy, sizeof(pj_pool_factory_policy));
    cp->factory.create_pool = &cpool_create_pool;
    cp->factory.release_pool = &cpool_release_pool;
    cp->factory.dump_status = &cpool_dump_status;
    cp->factory.on_block_alloc = &cpool_on_block_alloc;
    cp->factory.on_block_free = &cpool_on_block_free;
	cp->factory.inst_id = inst_id;
	//printf("inst_id=%d\n", cp->factory.inst_id);
    //modified by Charles
    pool = pj_pool_create_on_buf(inst_id, "cachingpool", cp->pool_buf, 2*sizeof(cp->pool_buf));
    pj_lock_create_simple_mutex(pool, "cachingpool", &cp->lock);
	//printf("%p\n", ((pj_mutex_t *)cp->lock));
}

PJ_DEF(void) pj_caching_pool_destroy( pj_caching_pool *cp )
{
    int i;
    pj_pool_t *pool;

    PJ_CHECK_STACK();

    /* Delete all pool in free list */
    for (i=0; i < PJ_CACHING_POOL_ARRAY_SIZE; ++i) {
	pj_pool_t *pool = (pj_pool_t*) cp->free_list[i].next;
	pj_pool_t *next;
	for (; pool != (void*)&cp->free_list[i]; pool = next) {
	    next = pool->next;
	    pj_list_erase(pool);
	    pj_pool_destroy_int(pool);
	}
    }

    /* Delete all pools in used list */
    pool = (pj_pool_t*) cp->used_list.next;
    while (pool != (pj_pool_t*) &cp->used_list) {
	pj_pool_t *next = pool->next;
	pj_list_erase(pool);
	PJ_LOG(4,(pool->obj_name, 
		  "Pool is not released by application, releasing now"));
	pj_pool_destroy_int(pool);
	pool = next;
    }

    if (cp->lock) {
	pj_lock_destroy(cp->lock);
	pj_lock_create_null_mutex(NULL, "cachingpool", &cp->lock);
    }
}

static pj_pool_t* cpool_create_pool(pj_pool_factory *pf, 
					      const char *name, 
					      pj_size_t initial_size, 
					      pj_size_t increment_sz, 
					      pj_pool_callback *callback)
{
    pj_caching_pool *cp = (pj_caching_pool*)pf;
    pj_pool_t *pool;
    int idx;

    PJ_CHECK_STACK();

    pj_lock_acquire(cp->lock);

    /* Use pool factory's policy when callback is NULL */
    if (callback == NULL) {
	callback = pf->policy.callback;
    }

    /* Search the suitable size for the pool. 
     * We'll just do linear search to the size array, as the array size itself
     * is only a few elements. Binary search I suspect will be less efficient
     * for this purpose.
     */
    if (initial_size <= pool_sizes[START_SIZE]) {
	for (idx=START_SIZE-1; 
	     idx >= 0 && pool_sizes[idx] >= initial_size;
	     --idx)
	    ;
	++idx;
    } else {
	for (idx=START_SIZE+1; 
	     idx < PJ_CACHING_POOL_ARRAY_SIZE && 
		  pool_sizes[idx] < initial_size;
	     ++idx)
	    ;
    }

    /* Check whether there's a pool in the list. */
    if (idx==PJ_CACHING_POOL_ARRAY_SIZE || pj_list_empty(&cp->free_list[idx])) {
	/* No pool is available. */
	/* Set minimum size. */
	if (idx < PJ_CACHING_POOL_ARRAY_SIZE)
	    initial_size =  pool_sizes[idx];

	/* Create new pool */
	pool = pj_pool_create_int(&cp->factory, name, initial_size, 
				  increment_sz, callback);
	if (!pool) {
	    pj_lock_release(cp->lock);
	    return NULL;
	}

    } else {
	/* Get one pool from the list. */
	pool = (pj_pool_t*) cp->free_list[idx].next;
	pj_list_erase(pool);

	/* Initialize the pool. */
	pj_pool_init_int(pool, name, increment_sz, callback);

	/* Update pool manager's free capacity. */
	if (cp->capacity > pj_pool_get_capacity(pool)) {
	cp->capacity -= pj_pool_get_capacity(pool);
	} else {
	    cp->capacity = 0;
	}

	PJ_LOG(6, (pool->obj_name, "pool reused, size=%u", pool->capacity));
    }

    /* Put in used list. */
    pj_list_insert_before( &cp->used_list, pool );

    /* Mark factory data */
    pool->factory_data = (void*) (long) idx;

    /* Increment used count. */
    ++cp->used_count;

    pj_lock_release(cp->lock);
    return pool;
}

static void cpool_release_pool( pj_pool_factory *pf, pj_pool_t *pool)
{
    pj_caching_pool *cp = (pj_caching_pool*)pf;
    pj_size_t pool_capacity;
    unsigned i;

    PJ_CHECK_STACK();

    PJ_ASSERT_ON_FAIL(pf && pool, return);

    pj_lock_acquire(cp->lock);

#if PJ_SAFE_POOL
    /* Make sure pool is still in our used list */
    if (pj_list_find_node(&cp->used_list, pool) != pool) {
	pj_assert(!"Attempt to destroy pool that has been destroyed before");
	return;
    }
#endif

    /* Erase from the used list. */
    pj_list_erase(pool);

    /* Decrement used count. */
    --cp->used_count;

    pool_capacity = pj_pool_get_capacity(pool);

    /* Destroy the pool if the size is greater than our size or if the total
     * capacity in our recycle list (plus the size of the pool) exceeds 
     * maximum capacity.
   . */
    if (pool_capacity > pool_sizes[PJ_CACHING_POOL_ARRAY_SIZE-1] ||
	cp->capacity + pool_capacity > cp->max_capacity)
    {
	pj_pool_destroy_int(pool);
	pj_lock_release(cp->lock);
	return;
    }

    /* Reset pool. */
    PJ_LOG(6, (pool->obj_name, "recycle(): cap=%d, used=%d(%d%%)", 
	       pool_capacity, pj_pool_get_used_size(pool), 
	       pj_pool_get_used_size(pool)*100/pool_capacity));
    pj_pool_reset(pool);

    pool_capacity = pj_pool_get_capacity(pool);

    /*
     * Otherwise put the pool in our recycle list.
     */
    i = (unsigned) (unsigned long) pool->factory_data;

    pj_assert(i<PJ_CACHING_POOL_ARRAY_SIZE);
    if (i >= PJ_CACHING_POOL_ARRAY_SIZE ) {
	/* Something has gone wrong with the pool. */
	pj_pool_destroy_int(pool);
	pj_lock_release(cp->lock);
	return;
    }

    pj_list_insert_after(&cp->free_list[i], pool);
    cp->capacity += pool_capacity;

    pj_lock_release(cp->lock);
}

static void cpool_dump_status(pj_pool_factory *factory, pj_bool_t detail )
{
#if PJ_LOG_MAX_LEVEL >= 3
    pj_caching_pool *cp = (pj_caching_pool*)factory;

    pj_lock_acquire(cp->lock);

    PJ_LOG(3,("cachpool", " Dumping caching pool:"));
    PJ_LOG(3,("cachpool", "   Capacity=%u, max_capacity=%u, used_cnt=%u", \
			     cp->capacity, cp->max_capacity, cp->used_count));
    if (detail) {
	pj_pool_t *pool = (pj_pool_t*) cp->used_list.next;
	pj_uint32_t total_used = 0, total_capacity = 0;
        PJ_LOG(3,("cachpool", "  Dumping all active pools:"));
	while (pool != (void*)&cp->used_list) {
	    unsigned pool_capacity = pj_pool_get_capacity(pool);
	    PJ_LOG(3,("cachpool", "   %16s: %8d of %8d (%d%%) used", 
				  pj_pool_getobjname(pool), 
				  pj_pool_get_used_size(pool), 
				  pool_capacity,
				  pj_pool_get_used_size(pool)*100/pool_capacity));
	    total_used += pj_pool_get_used_size(pool);
	    total_capacity += pool_capacity;
	    pool = pool->next;
	}
	if (total_capacity) {
	    PJ_LOG(3,("cachpool", "  Total %9d of %9d (%d %%) used!",
				  total_used, total_capacity,
				  total_used * 100 / total_capacity));
	}
    }

    pj_lock_release(cp->lock);
#else
    PJ_UNUSED_ARG(factory);
    PJ_UNUSED_ARG(detail);
#endif
}


static pj_bool_t cpool_on_block_alloc(pj_pool_factory *f, pj_size_t sz)
{
    pj_caching_pool *cp = (pj_caching_pool*)f;

    //Can't lock because mutex is not recursive
    //if (cp->mutex) pj_mutex_lock(cp->mutex);

    cp->used_size += sz;
    if (cp->used_size > cp->peak_used_size)
	cp->peak_used_size = cp->used_size;

    //if (cp->mutex) pj_mutex_unlock(cp->mutex);

    return PJ_TRUE;
}


static void cpool_on_block_free(pj_pool_factory *f, pj_size_t sz)
{
    pj_caching_pool *cp = (pj_caching_pool*)f;

    //pj_mutex_lock(cp->mutex);
    cp->used_size -= sz;
    //pj_mutex_unlock(cp->mutex);
}


#endif	/* PJ_HAS_POOL_ALT_API */

