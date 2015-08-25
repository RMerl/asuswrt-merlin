/* $Id: guid.c 4385 2013-02-27 10:11:59Z nanang $ */
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
#include <pj/ctype.h>
#include <pj/guid.h>
#include <pj/pool.h>

PJ_DEF(pj_str_t*) pj_generate_unique_string_lower(int inst_id, pj_str_t *str)
{
    int i;

    pj_generate_unique_string(inst_id, str);
    for (i = 0; i < str->slen; i++)
	str->ptr[i] = (char)pj_tolower(str->ptr[i]);

    return str;
}

PJ_DEF(void) pj_create_unique_string(pj_pool_t *pool, pj_str_t *str)
{
    str->ptr = (char*)pj_pool_alloc(pool, PJ_GUID_STRING_LENGTH);
    pj_generate_unique_string(pool->factory->inst_id, str);
}

PJ_DEF(void) pj_create_unique_string_lower(pj_pool_t *pool, pj_str_t *str)
{
    int i;

    pj_create_unique_string(pool, str);
    for (i = 0; i < str->slen; i++)
	str->ptr[i] = (char)pj_tolower(str->ptr[i]);
}
