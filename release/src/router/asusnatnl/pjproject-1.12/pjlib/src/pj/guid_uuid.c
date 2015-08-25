/* $Id: guid_uuid.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/guid.h>
#include <pj/assert.h>
#include <pj/errno.h>
#include <pj/os.h>
#include <pj/string.h>

#include <uuid/uuid.h>

PJ_DEF_DATA(const unsigned) PJ_GUID_STRING_LENGTH=36;

PJ_DEF(unsigned) pj_GUID_STRING_LENGTH()
{
    return PJ_GUID_STRING_LENGTH;
}

PJ_DEF(pj_str_t*) pj_generate_unique_string(int inst_id, pj_str_t *str)
{
	PJ_UNUSED_ARG(inst_id);
    enum {GUID_LEN = 36};
    char sguid[GUID_LEN + 1];
    uuid_t uuid = {0};
    
    PJ_ASSERT_RETURN(GUID_LEN <= PJ_GUID_STRING_LENGTH, NULL);
    PJ_ASSERT_RETURN(str->ptr != NULL, NULL);
    PJ_CHECK_STACK();
    
    uuid_generate(uuid);
    uuid_unparse(uuid, sguid);
    
    pj_memcpy(str->ptr, sguid, GUID_LEN);
    str->slen = GUID_LEN;

    return str;
}

