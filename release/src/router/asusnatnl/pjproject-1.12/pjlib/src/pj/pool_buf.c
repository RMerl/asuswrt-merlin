/* $Id: pool_buf.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/pool_buf.h>
#include <pj/assert.h>
#include <pj/os.h>

struct pj_pool_factory stack_based_factory[PJSUA_MAX_INSTANCES];

struct creation_param
{
    void	*stack_buf;
    pj_size_t	 size;
};

static int is_initialized[PJSUA_MAX_INSTANCES];
static long tls[PJSUA_MAX_INSTANCES];
static void* stack_alloc(pj_pool_factory *factory, pj_size_t size);
static int is_tls_initialized;

static void tls_id_initialize()
{
	int i;
	if(is_tls_initialized)
		return;

	for (i=0; i < PJ_ARRAY_SIZE(tls); i++)
	{
		tls[i] = -1;
	}

	is_tls_initialized = 1;
}

static void pool_buf_cleanup(int inst_id)
{
    if (tls[inst_id] != -1) {
	pj_thread_local_free(tls[inst_id]);
	tls[inst_id] = -1;
    }
    if (is_initialized[inst_id])
	is_initialized[inst_id] = 0;
}

static pj_status_t pool_buf_initialize(int inst_id)
{
	tls_id_initialize();

    pj_atexit(inst_id, &pool_buf_cleanup);

    stack_based_factory[inst_id].policy.block_alloc = &stack_alloc;
	stack_based_factory[inst_id].inst_id = inst_id;

    return pj_thread_local_alloc(&tls[inst_id]);
}

static void* stack_alloc(pj_pool_factory *factory, pj_size_t size)
{
    struct creation_param *param;
    void *buf;

    PJ_UNUSED_ARG(factory);

    param = (struct creation_param*) pj_thread_local_get(tls[factory->inst_id]);
    if (param == NULL) {
	/* Don't assert(), this is normal no-memory situation */
	return NULL;
    }

    pj_thread_local_set(tls[factory->inst_id], NULL);

    PJ_ASSERT_RETURN(size <= param->size, NULL);

    buf = param->stack_buf;

    /* Prevent the buffer from being reused */
    param->stack_buf = NULL;

    return buf;
}


PJ_DEF(pj_pool_t*) pj_pool_create_on_buf(int inst_id,
					 const char *name,
					 void *buf,
					 pj_size_t size)
{
#if PJ_HAS_POOL_ALT_API == 0
    struct creation_param param;
    long align_diff;

    PJ_ASSERT_RETURN(buf && size, NULL);

    if (!is_initialized[inst_id]) {
	if (pool_buf_initialize(inst_id) != PJ_SUCCESS)
	    return NULL;
	is_initialized[inst_id] = 1;
    }

    /* Check and align buffer */
    align_diff = (long)buf;
    if (align_diff & (PJ_POOL_ALIGNMENT-1)) {
	align_diff &= (PJ_POOL_ALIGNMENT-1);
	buf = (void*) (((char*)buf) + align_diff);
	size -= align_diff;
    }

    param.stack_buf = buf;
    param.size = size;
    pj_thread_local_set(tls[inst_id], &param);

    return pj_pool_create_int(&stack_based_factory[inst_id], name, size, 0, 
			      pj_pool_factory_default_policy.callback);
#else
    PJ_UNUSED_ARG(buf);
    return pj_pool_create(NULL, name, size, size, NULL);
#endif
}

