/* $Id: pool_i.h 4395 2013-02-27 12:07:30Z ming $ */
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


#include <pj/string.h>


PJ_IDEF(pj_size_t) pj_pool_get_capacity( pj_pool_t *pool )
{
    return pool->capacity;
}

PJ_IDEF(pj_size_t) pj_pool_get_used_size( pj_pool_t *pool )
{
    pj_pool_block *b = pool->block_list.next;
    pj_size_t used_size = sizeof(pj_pool_t);
    while (b != &pool->block_list) {
	used_size += (b->cur - b->buf) + sizeof(pj_pool_block);
	b = b->next;
    }
    return used_size;
}

PJ_IDEF(void*) pj_pool_alloc_from_block( pj_pool_block *block, pj_size_t size )
{
    /* The operation below is valid for size==0. 
     * When size==0, the function will return the pointer to the pool
     * memory address, but no memory will be allocated.
     */
    if (size & (PJ_POOL_ALIGNMENT-1)) {
	size = (size + PJ_POOL_ALIGNMENT) & ~(PJ_POOL_ALIGNMENT-1);
    }
    if ((pj_size_t)(block->end - block->cur) >= size) {
	void *ptr = block->cur;
	block->cur += size;
	return ptr;
    }
    return NULL;
}

PJ_IDEF(void*) pj_pool_alloc( pj_pool_t *pool, pj_size_t size)
{
    void *ptr = pj_pool_alloc_from_block(pool->block_list.next, size);
    if (!ptr)
	ptr = pj_pool_allocate_find(pool, size);
    return ptr;
}


PJ_IDEF(void*) pj_pool_calloc( pj_pool_t *pool, pj_size_t count, pj_size_t size)
{
    void *buf = pj_pool_alloc( pool, size*count);
    if (buf)
	pj_bzero(buf, size * count);
    return buf;
}

PJ_IDEF(const char *) pj_pool_getobjname( const pj_pool_t *pool )
{
    return pool->obj_name;
}

PJ_IDEF(pj_pool_t*) pj_pool_create( pj_pool_factory *f, 
				    const char *name,
				    pj_size_t initial_size, 
				    pj_size_t increment_size,
				    pj_pool_callback *callback)
{
    return (*f->create_pool)(f, name, initial_size, increment_size, callback);
}

PJ_IDEF(void) pj_pool_release( pj_pool_t *pool )
{
    if (pool->factory->release_pool)
	(*pool->factory->release_pool)(pool->factory, pool);
}

