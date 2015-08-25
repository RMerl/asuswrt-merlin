/* $Id: pool_signature.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/assert.h>
#include <pj/string.h>

#if PJ_SAFE_POOL
#   define SIG_SIZE		sizeof(pj_uint32_t)

static void apply_signature(void *p, pj_size_t size);
static void check_pool_signature(void *p, pj_size_t size);

#   define APPLY_SIG(p,sz)	apply_signature(p,sz), \
				p=(void*)(((char*)p)+SIG_SIZE)
#   define REMOVE_SIG(p,sz)	check_pool_signature(p,sz), \
				p=(void*)(((char*)p)-SIG_SIZE)

#   define SIG_BEGIN	    0x600DC0DE
#   define SIG_END	    0x0BADC0DE

static void apply_signature(void *p, pj_size_t size)
{
    pj_uint32_t sig;

    sig = SIG_BEGIN;
    pj_memcpy(p, &sig, SIG_SIZE);

    sig = SIG_END;
    pj_memcpy(((char*)p)+SIG_SIZE+size, &sig, SIG_SIZE);
}

static void check_pool_signature(void *p, pj_size_t size)
{
    pj_uint32_t sig;
    pj_uint8_t *mem = (pj_uint8_t*)p;

    /* Check that signature at the start of the block is still intact */
    sig = SIG_BEGIN;
    pj_assert(!pj_memcmp(mem-SIG_SIZE, &sig, SIG_SIZE));

    /* Check that signature at the end of the block is still intact.
     * Note that "mem" has been incremented by SIG_SIZE 
     */
    sig = SIG_END;
    pj_assert(!pj_memcmp(mem+size, &sig, SIG_SIZE));
}

#else
#   define SIG_SIZE	    0
#   define APPLY_SIG(p,sz)
#   define REMOVE_SIG(p,sz)
#endif
