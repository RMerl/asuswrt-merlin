/* $Id: pool.c 4395 2013-02-27 12:07:30Z ming $ */
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
#include <pj/except.h>
#include <pj/assert.h>
#include <pj/os.h>

#if !PJ_HAS_POOL_ALT_API


/* Include inline definitions when inlining is disabled. */
#if !PJ_FUNCTIONS_ARE_INLINED
#  include <pj/pool_i.h>
#endif

#define LOG(expr)   			PJ_LOG(6,expr)
#define ALIGN_PTR(PTR,ALIGNMENT)	(PTR + (-(long)(PTR) & (ALIGNMENT-1)))

PJ_DEF_DATA(int) PJ_NO_MEMORY_EXCEPTION;

PJ_DEF(int) pj_NO_MEMORY_EXCEPTION()
{
    return PJ_NO_MEMORY_EXCEPTION;
}

/*
 * Create new block.
 * Create a new big chunk of memory block, from which user allocation will be
 * taken from.
 */
static pj_pool_block *pj_pool_create_block( pj_pool_t *pool, pj_size_t size)
{
    pj_pool_block *block;

    PJ_CHECK_STACK();
    pj_assert(size >= sizeof(pj_pool_block));

    LOG((pool->obj_name, "create_block(sz=%u), cur.cap=%u, cur.used=%u", 
	 size, pool->capacity, pj_pool_get_used_size(pool)));

    /* Request memory from allocator. */
    block = (pj_pool_block*) 
	(*pool->factory->policy.block_alloc)(pool->factory, size);
    if (block == NULL) {
	(*pool->callback)(pool, size);
	return NULL;
    }

    /* Add capacity. */
    pool->capacity += size;

    /* Set start and end of buffer. */
    block->buf = ((unsigned char*)block) + sizeof(pj_pool_block);
    block->end = ((unsigned char*)block) + size;

    /* Set the start pointer, aligning it as needed */
    block->cur = ALIGN_PTR(block->buf, PJ_POOL_ALIGNMENT);

    /* Insert in the front of the list. */
    pj_list_insert_after(&pool->block_list, block);

    LOG((pool->obj_name," block created, buffer=%p-%p",block->buf, block->end));

    return block;
}

/*
 * Allocate memory chunk for user from available blocks.
 * This will iterate through block list to find space to allocate the chunk.
 * If no space is available in all the blocks, a new block might be created
 * (depending on whether the pool is allowed to resize).
 */
PJ_DEF(void*) pj_pool_allocate_find(pj_pool_t *pool, pj_size_t size)
{
    pj_pool_block *block = pool->block_list.next;
    void *p;
    unsigned block_size;

    PJ_CHECK_STACK();

    while (block != &pool->block_list) {
	p = pj_pool_alloc_from_block(block, size);
	if (p != NULL)
	    return p;
	block = block->next;
    }
    /* No available space in all blocks. */

    /* If pool is configured NOT to expand, return error. */
    if (pool->increment_size == 0) {
	LOG((pool->obj_name, "Can't expand pool to allocate %u bytes "
	     "(used=%u, cap=%u)",
	     size, pj_pool_get_used_size(pool), pool->capacity));
	(*pool->callback)(pool, size);
	return NULL;
    }

    /* If pool is configured to expand, but the increment size
     * is less than the required size, expand the pool by multiple
     * increment size. Also count the size wasted due to aligning
     * the block.
     */
    if (pool->increment_size < 
	    size + sizeof(pj_pool_block) + PJ_POOL_ALIGNMENT) 
    {
        pj_size_t count;
        count = (size + pool->increment_size + sizeof(pj_pool_block) +
                 PJ_POOL_ALIGNMENT) / 
                pool->increment_size;
        block_size = count * pool->increment_size;

    } else {
        block_size = pool->increment_size;
    }

    LOG((pool->obj_name, 
	 "%u bytes requested, resizing pool by %u bytes (used=%u, cap=%u)",
	 size, block_size, pj_pool_get_used_size(pool), pool->capacity));

    block = pj_pool_create_block(pool, block_size);
    if (!block)
	return NULL;

    p = pj_pool_alloc_from_block(block, size);
    pj_assert(p != NULL);
#if PJ_DEBUG
    if (p == NULL) {
	p = p;
    }
#endif
    return p;
}

/*
 * Internal function to initialize pool.
 */
PJ_DEF(void) pj_pool_init_int(  pj_pool_t *pool, 
				const char *name,
				pj_size_t increment_size,
				pj_pool_callback *callback)
{
    PJ_CHECK_STACK();

    pool->increment_size = increment_size;
    pool->callback = callback;

    if (name) {
	if (strchr(name, '%') != NULL) {
	    pj_ansi_snprintf(pool->obj_name, sizeof(pool->obj_name), 
			     name, pool);
	} else {
	    pj_ansi_strncpy(pool->obj_name, name, PJ_MAX_OBJ_NAME);
	    pool->obj_name[PJ_MAX_OBJ_NAME-1] = '\0';
	}
    } else {
	pool->obj_name[0] = '\0';
    }
}

/*
 * Create new memory pool.
 */
PJ_DEF(pj_pool_t*) pj_pool_create_int( pj_pool_factory *f, const char *name,
				       pj_size_t initial_size, 
				       pj_size_t increment_size,
				       pj_pool_callback *callback)
{
    pj_pool_t *pool;
    pj_pool_block *block;
    pj_uint8_t *buffer;

    PJ_CHECK_STACK();

    /* Size must be at least sizeof(pj_pool)+sizeof(pj_pool_block) */
    PJ_ASSERT_RETURN(initial_size >= sizeof(pj_pool_t)+sizeof(pj_pool_block),
		     NULL);

    /* If callback is NULL, set calback from the policy */
    if (callback == NULL)
	callback = f->policy.callback;

    /* Allocate initial block */
    buffer = (pj_uint8_t*) (*f->policy.block_alloc)(f, initial_size);
    if (!buffer)
	return NULL;

    /* Set pool administrative data. */
    pool = (pj_pool_t*)buffer;
    pj_bzero(pool, sizeof(*pool));

    pj_list_init(&pool->block_list);
    pool->factory = f;

    /* Create the first block from the memory. */
    block = (pj_pool_block*) (buffer + sizeof(*pool));
    block->buf = ((unsigned char*)block) + sizeof(pj_pool_block);
    block->end = buffer + initial_size;

    /* Set the start pointer, aligning it as needed */
    block->cur = ALIGN_PTR(block->buf, PJ_POOL_ALIGNMENT);

    pj_list_insert_after(&pool->block_list, block);

    pj_pool_init_int(pool, name, increment_size, callback);

    /* Pool initial capacity and used size */
    pool->capacity = initial_size;

    LOG((pool->obj_name, "pool created, size=%u", pool->capacity));
    return pool;
}

/*
 * Reset the pool to the state when it was created.
 * All blocks will be deallocated except the first block. All memory areas
 * are marked as free.
 */
static void reset_pool(pj_pool_t *pool)
{
    pj_pool_block *block;

    PJ_CHECK_STACK();

    block = pool->block_list.prev;
    if (block == &pool->block_list)
	return;

    /* Skip the first block because it is occupying the same memory
       as the pool itself.
    */
    block = block->prev;
    
    while (block != &pool->block_list) {
	pj_pool_block *prev = block->prev;
	pj_list_erase(block);
	(*pool->factory->policy.block_free)(pool->factory, block, 
					    block->end - (unsigned char*)block);
	block = prev;
    }

    block = pool->block_list.next;

    /* Set the start pointer, aligning it as needed */
    block->cur = ALIGN_PTR(block->buf, PJ_POOL_ALIGNMENT);

    pool->capacity = block->end - (unsigned char*)pool;
}

/*
 * The public function to reset pool.
 */
PJ_DEF(void) pj_pool_reset(pj_pool_t *pool)
{
    LOG((pool->obj_name, "reset(): cap=%d, used=%d(%d%%)", 
	pool->capacity, pj_pool_get_used_size(pool), 
	pj_pool_get_used_size(pool)*100/pool->capacity));

    reset_pool(pool);
}

/*
 * Destroy the pool.
 */
PJ_DEF(void) pj_pool_destroy_int(pj_pool_t *pool)
{
    pj_size_t initial_size;

    LOG((pool->obj_name, "destroy(): cap=%d, used=%d(%d%%), block0=%p-%p", 
	pool->capacity, pj_pool_get_used_size(pool), 
	pj_pool_get_used_size(pool)*100/pool->capacity,
	((pj_pool_block*)pool->block_list.next)->buf, 
	((pj_pool_block*)pool->block_list.next)->end));

    reset_pool(pool);
    initial_size = ((pj_pool_block*)pool->block_list.next)->end - 
		   (unsigned char*)pool;
    if (pool->factory->policy.block_free)
	(*pool->factory->policy.block_free)(pool->factory, pool, initial_size);
}


#endif	/* PJ_HAS_POOL_ALT_API */

