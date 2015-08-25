/* $Id: pool_policy_kmalloc.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/except.h>
#include <pj/os.h>


static void *default_block_alloc(pj_pool_factory *factory, pj_size_t size)
{
    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(factory);

    return kmalloc(size, GFP_ATOMIC);
}

static void default_block_free(pj_pool_factory *factory, 
			       void *mem, pj_size_t size)
{
    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(factory);
    PJ_UNUSED_ARG(size);

    kfree(mem);
}

static void default_pool_callback(pj_pool_t *pool, pj_size_t size)
{
    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(size);

    PJ_THROW(PJ_NO_MEMORY_EXCEPTION);
}

pj_pool_factory_policy pj_pool_factory_default_policy = 
{
    &default_block_alloc,
    &default_block_free,
    &default_pool_callback,
    0
};

PJ_DEF(const pj_pool_factory_policy*) pj_pool_factory_get_default_policy(void)
{
    return &pj_pool_factory_default_policy;
}

