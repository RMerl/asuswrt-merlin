/* $Id: stun_simple.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjlib-util/stun_simple.h>
#include <pjlib-util/errno.h>
#include <pj/pool.h>
#include <pj/log.h>
#include <pj/sock.h>
#include <pj/os.h>

#define THIS_FILE   "stun_simple.c"

PJ_DEF(pj_status_t) pjstun_create_bind_req( pj_pool_t *pool, 
					     void **msg, pj_size_t *len,
					     pj_uint32_t id_hi, 
					     pj_uint32_t id_lo)
{
    pjstun_msg_hdr *hdr;
    
    PJ_CHECK_STACK();


    hdr = PJ_POOL_ZALLOC_T(pool, pjstun_msg_hdr);
    if (!hdr)
	return PJ_ENOMEM;

    hdr->type = pj_htons(PJSTUN_BINDING_REQUEST);
    hdr->tsx[2] = pj_htonl(id_hi);
    hdr->tsx[3] = pj_htonl(id_lo);
    *msg = hdr;
    *len = sizeof(pjstun_msg_hdr);

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pjstun_parse_msg( void *buf, pj_size_t len, 
				       pjstun_msg *msg)
{
    pj_uint16_t msg_type, msg_len;
    char *p_attr;

    PJ_CHECK_STACK();

    msg->hdr = (pjstun_msg_hdr*)buf;
    msg_type = pj_ntohs(msg->hdr->type);

    switch (msg_type) {
    case PJSTUN_BINDING_REQUEST:
    case PJSTUN_BINDING_RESPONSE:
    case PJSTUN_BINDING_ERROR_RESPONSE:
    case PJSTUN_SHARED_SECRET_REQUEST:
    case PJSTUN_SHARED_SECRET_RESPONSE:
    case PJSTUN_SHARED_SECRET_ERROR_RESPONSE:
	break;
    default:
	PJ_LOG(4,(THIS_FILE, "Error: unknown msg type %d", msg_type));
	return PJLIB_UTIL_ESTUNINMSGTYPE;
    }

    msg_len = pj_ntohs(msg->hdr->length);
    if (msg_len != len - sizeof(pjstun_msg_hdr)) {
	PJ_LOG(4,(THIS_FILE, "Error: invalid msg_len %d (expecting %d)", 
			     msg_len, len - sizeof(pjstun_msg_hdr)));
	return PJLIB_UTIL_ESTUNINMSGLEN;
    }

    msg->attr_count = 0;
    p_attr = (char*)buf + sizeof(pjstun_msg_hdr);

    while (msg_len > 0) {
	pjstun_attr_hdr **attr = &msg->attr[msg->attr_count];
	pj_uint32_t len;
	pj_uint16_t attr_type;

	*attr = (pjstun_attr_hdr*)p_attr;
	len = pj_ntohs((pj_uint16_t) ((*attr)->length)) + sizeof(pjstun_attr_hdr);
	len = (len + 3) & ~3;

	if (msg_len < len) {
	    PJ_LOG(4,(THIS_FILE, "Error: length mismatch in attr %d", 
				 msg->attr_count));
	    return PJLIB_UTIL_ESTUNINATTRLEN;
	}

	attr_type = pj_ntohs((*attr)->type);
	if (attr_type > PJSTUN_ATTR_REFLECTED_FROM &&
	    attr_type != PJSTUN_ATTR_XOR_MAPPED_ADDR)
	{
	    PJ_LOG(5,(THIS_FILE, "Warning: unknown attr type %x in attr %d. "
				 "Attribute was ignored.",
				 attr_type, msg->attr_count));
	}

	msg_len = (pj_uint16_t)(msg_len - len);
	p_attr += len;
	++msg->attr_count;
    }

    return PJ_SUCCESS;
}

PJ_DEF(void*) pjstun_msg_find_attr( pjstun_msg *msg, pjstun_attr_type t)
{
    int i;

    PJ_CHECK_STACK();

    for (i=0; i<msg->attr_count; ++i) {
	pjstun_attr_hdr *attr = msg->attr[i];
	if (pj_ntohs(attr->type) == t)
	    return attr;
    }

    return 0;
}
