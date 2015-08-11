/* $Id: pool_alt.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_POOL_ALT_H__
#define __PJ_POOL_ALT_H__

#define __PJ_POOL_H__


/**
 * The type for function to receive callback from the pool when it is unable
 * to allocate memory. The elegant way to handle this condition is to throw
 * exception, and this is what is expected by most of this library 
 * components.
 */
typedef void pj_pool_callback(pj_pool_t *pool, pj_size_t size);

struct pj_pool_mem
{
    struct pj_pool_mem *next;

    /* data follows immediately */
};


struct pj_pool_t
{
    struct pj_pool_mem *first_mem;
    pj_pool_factory    *factory;
    char	        obj_name[32];
    pj_size_t		used_size;
    pj_pool_callback   *cb;
};


#define PJ_POOL_SIZE	        (sizeof(struct pj_pool_t))

/**
 * This constant denotes the exception number that will be thrown by default
 * memory factory policy when memory allocation fails.
 */
extern int PJ_NO_MEMORY_EXCEPTION;



/*
 * Declare all pool API as macro that calls the implementation
 * function.
 */
#define pj_pool_create(fc,nm,init,inc,cb)   \
	pj_pool_create_imp(__FILE__, __LINE__, fc, nm, init, inc, cb)

#define pj_pool_release(pool)		    pj_pool_release_imp(pool)
#define pj_pool_getobjname(pool)	    pj_pool_getobjname_imp(pool)
#define pj_pool_reset(pool)		    pj_pool_reset_imp(pool)
#define pj_pool_get_capacity(pool)	    pj_pool_get_capacity_imp(pool)
#define pj_pool_get_used_size(pool)	    pj_pool_get_used_size_imp(pool)
#define pj_pool_alloc(pool,sz)		    \
	pj_pool_alloc_imp(__FILE__, __LINE__, pool, sz)

#define pj_pool_calloc(pool,cnt,elem)	    \
	pj_pool_calloc_imp(__FILE__, __LINE__, pool, cnt, elem)

#define pj_pool_zalloc(pool,sz)		    \
	pj_pool_zalloc_imp(__FILE__, __LINE__, pool, sz)



/*
 * Declare prototypes for pool implementation API.
 */

/* Create pool */
PJ_DECL(pj_pool_t*) pj_pool_create_imp(const char *file, int line,
				       void *factory,
				       const char *name,
				       pj_size_t initial_size,
				       pj_size_t increment_size,
				       pj_pool_callback *callback);

/* Release pool */
PJ_DECL(void) pj_pool_release_imp(pj_pool_t *pool);

/* Get pool name */
PJ_DECL(const char*) pj_pool_getobjname_imp(pj_pool_t *pool);

/* Reset pool */
PJ_DECL(void) pj_pool_reset_imp(pj_pool_t *pool);

/* Get capacity */
PJ_DECL(pj_size_t) pj_pool_get_capacity_imp(pj_pool_t *pool);

/* Get total used size */
PJ_DECL(pj_size_t) pj_pool_get_used_size_imp(pj_pool_t *pool);

/* Allocate memory from the pool */
PJ_DECL(void*) pj_pool_alloc_imp(const char *file, int line, 
				 pj_pool_t *pool, pj_size_t sz);

/* Allocate memory from the pool and zero the memory */
PJ_DECL(void*) pj_pool_calloc_imp(const char *file, int line, 
				  pj_pool_t *pool, unsigned cnt, 
				  unsigned elemsz);

/* Allocate memory from the pool and zero the memory */
PJ_DECL(void*) pj_pool_zalloc_imp(const char *file, int line, 
				  pj_pool_t *pool, pj_size_t sz);


#define PJ_POOL_ZALLOC_T(pool,type) \
	    ((type*)pj_pool_zalloc(pool, sizeof(type)))
#define PJ_POOL_ALLOC_T(pool,type) \
	    ((type*)pj_pool_alloc(pool, sizeof(type)))
#ifndef PJ_POOL_ALIGNMENT
#   define PJ_POOL_ALIGNMENT    4
#endif

/**
 * This structure declares pool factory interface.
 */
typedef struct pj_pool_factory_policy
{
    /**
     * Allocate memory block (for use by pool). This function is called
     * by memory pool to allocate memory block.
     * 
     * @param factory	Pool factory.
     * @param size	The size of memory block to allocate.
     *
     * @return		Memory block.
     */
    void* (*block_alloc)(pj_pool_factory *factory, pj_size_t size);

    /**
     * Free memory block.
     *
     * @param factory	Pool factory.
     * @param mem	Memory block previously allocated by block_alloc().
     * @param size	The size of memory block.
     */
    void (*block_free)(pj_pool_factory *factory, void *mem, pj_size_t size);

    /**
     * Default callback to be called when memory allocation fails.
     */
    pj_pool_callback *callback;

    /**
     * Option flags.
     */
    unsigned flags;

} pj_pool_factory_policy;

struct pj_pool_factory
{
    pj_pool_factory_policy policy;
    int dummy;
};

struct pj_caching_pool 
{
    pj_pool_factory factory;

    /* just to make it compilable */
    unsigned used_count;
    unsigned used_size;
    unsigned peak_used_size;
};

/* just to make it compilable */
typedef struct pj_pool_block
{
    int dummy;
} pj_pool_block;

#define pj_caching_pool_init( cp, pol, mac)
#define pj_caching_pool_destroy(cp)
#define pj_pool_factory_dump(pf, detail)

#endif	/* __PJ_POOL_ALT_H__ */

