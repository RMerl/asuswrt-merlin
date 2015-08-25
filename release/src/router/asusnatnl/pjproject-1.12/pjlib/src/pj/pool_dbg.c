/* $Id: pool_dbg.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/string.h>

#if PJ_HAS_POOL_ALT_API

#if PJ_HAS_MALLOC_H
#   include <malloc.h>
#endif


#if PJ_HAS_STDLIB_H
#   include <stdlib.h>
#endif


#if defined(PJ_WIN32) && PJ_WIN32!=0 && defined(PJ_DEBUG) && PJ_DEBUG!=0 \
    && !PJ_NATIVE_STRING_IS_UNICODE
#   include <windows.h>
#   define TRACE_(msg)	OutputDebugString(msg)
#endif

/* Uncomment this to enable TRACE_ */
//#undef TRACE_



int PJ_NO_MEMORY_EXCEPTION;


PJ_DEF(int) pj_NO_MEMORY_EXCEPTION()
{
    return PJ_NO_MEMORY_EXCEPTION;
}

/* Create pool */
PJ_DEF(pj_pool_t*) pj_pool_create_imp( const char *file, int line,
				       void *factory,
				       const char *name,
				       pj_size_t initial_size,
				       pj_size_t increment_size,
				       pj_pool_callback *callback)
{
    pj_pool_t *pool;

    PJ_UNUSED_ARG(file);
    PJ_UNUSED_ARG(line);
    PJ_UNUSED_ARG(factory);
    PJ_UNUSED_ARG(initial_size);
    PJ_UNUSED_ARG(increment_size);

    pool = malloc(sizeof(struct pj_pool_t));
    if (!pool)
	return NULL;

    if (name) {
	pj_ansi_strncpy(pool->obj_name, name, sizeof(pool->obj_name));
	pool->obj_name[sizeof(pool->obj_name)-1] = '\0';
    } else {
	strcpy(pool->obj_name, "altpool");
    }

    pool->factory = NULL;
    pool->first_mem = NULL;
    pool->used_size = 0;
    pool->cb = callback;

    return pool;
}


/* Release pool */
PJ_DEF(void) pj_pool_release_imp(pj_pool_t *pool)
{
    pj_pool_reset(pool);
    free(pool);
}

/* Get pool name */
PJ_DEF(const char*) pj_pool_getobjname_imp(pj_pool_t *pool)
{
    PJ_UNUSED_ARG(pool);
    return "pooldbg";
}

/* Reset pool */
PJ_DEF(void) pj_pool_reset_imp(pj_pool_t *pool)
{
    struct pj_pool_mem *mem;

    mem = pool->first_mem;
    while (mem) {
	struct pj_pool_mem *next = mem->next;
	free(mem);
	mem = next;
    }

    pool->first_mem = NULL;
}

/* Get capacity */
PJ_DEF(pj_size_t) pj_pool_get_capacity_imp(pj_pool_t *pool)
{
    PJ_UNUSED_ARG(pool);

    /* Unlimited capacity */
    return 0x7FFFFFFFUL;
}

/* Get total used size */
PJ_DEF(pj_size_t) pj_pool_get_used_size_imp(pj_pool_t *pool)
{
    return pool->used_size;
}

/* Allocate memory from the pool */
PJ_DEF(void*) pj_pool_alloc_imp( const char *file, int line, 
				 pj_pool_t *pool, pj_size_t sz)
{
    struct pj_pool_mem *mem;

    PJ_UNUSED_ARG(file);
    PJ_UNUSED_ARG(line);

    mem = malloc(sz + sizeof(struct pj_pool_mem));
    if (!mem) {
	if (pool->cb)
	    (*pool->cb)(pool, sz);
	return NULL;
    }

    mem->next = pool->first_mem;
    pool->first_mem = mem;

#ifdef TRACE_
    {
	char msg[120];
	pj_ansi_sprintf(msg, "Mem %X (%d+%d bytes) allocated by %s:%d\r\n",
			mem, sz, sizeof(struct pj_pool_mem), 
			file, line);
	TRACE_(msg);
    }
#endif

    return ((char*)mem) + sizeof(struct pj_pool_mem);
}

/* Allocate memory from the pool and zero the memory */
PJ_DEF(void*) pj_pool_calloc_imp( const char *file, int line, 
				  pj_pool_t *pool, unsigned cnt, 
				  unsigned elemsz)
{
    void *mem;

    mem = pj_pool_alloc_imp(file, line, pool, cnt*elemsz);
    if (!mem)
	return NULL;

    pj_bzero(mem, cnt*elemsz);
    return mem;
}

/* Allocate memory from the pool and zero the memory */
PJ_DEF(void*) pj_pool_zalloc_imp( const char *file, int line, 
				  pj_pool_t *pool, pj_size_t sz)
{
    return pj_pool_calloc_imp(file, line, pool, 1, sz); 
}



#endif	/* PJ_HAS_POOL_ALT_API */
