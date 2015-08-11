/* $Id: stun_msg_dump.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjnath/stun_msg.h>
#include <pjnath/errno.h>
#include <pj/assert.h>
#include <pj/os.h>
#include <pj/string.h>

#if PJ_LOG_MAX_LEVEL > 0


#define APPLY()		if (len < 1 || len >= (end-p)) \
			    goto on_return; \
			p += len

static int print_binary(char *buffer, unsigned length,
			const pj_uint8_t *data, unsigned data_len)
{
    unsigned i;

    if (length < data_len * 2 + 8)
	return -1;

    pj_ansi_sprintf(buffer, ", data=");
    buffer += 7;

    for (i=0; i<data_len; ++i) {
	pj_ansi_sprintf(buffer, "%02x", (*data) & 0xFF);
	buffer += 2;
	data++;
    }

    pj_ansi_sprintf(buffer, "\n");
    buffer++;

    return data_len * 2 + 8;
}

static int print_attr(char *buffer, unsigned length,
		      const pj_stun_attr_hdr *ahdr)
{
    char *p = buffer, *end = buffer + length;
    const char *attr_name = pj_stun_get_attr_name(ahdr->type);
    char attr_buf[32];
    int len;

    if (*attr_name == '?') {
	pj_ansi_snprintf(attr_buf, sizeof(attr_buf), "Attr 0x%x", 
			 ahdr->type);
	attr_name = attr_buf;
    }

    len = pj_ansi_snprintf(p, end-p,
			   "  %s: length=%d",
			   attr_name,
			   (int)ahdr->length);
    APPLY();


    switch (ahdr->type) {
    case PJ_STUN_ATTR_MAPPED_ADDR:
    case PJ_STUN_ATTR_RESPONSE_ADDR:
    case PJ_STUN_ATTR_SOURCE_ADDR:
    case PJ_STUN_ATTR_CHANGED_ADDR:
    case PJ_STUN_ATTR_REFLECTED_FROM:
    case PJ_STUN_ATTR_XOR_PEER_ADDR:
    case PJ_STUN_ATTR_XOR_RELAYED_ADDR:
    case PJ_STUN_ATTR_XOR_MAPPED_ADDR:
    case PJ_STUN_ATTR_XOR_REFLECTED_FROM:
    case PJ_STUN_ATTR_ALTERNATE_SERVER:
	{
	    const pj_stun_sockaddr_attr *attr;

	    attr = (const pj_stun_sockaddr_attr*)ahdr;

	    if (attr->sockaddr.addr.sa_family == pj_AF_INET()) {
		len = pj_ansi_snprintf(p, end-p,
				       ", IPv4 addr=%s:%d\n",
				       pj_inet_ntoa(attr->sockaddr.ipv4.sin_addr),
				       pj_ntohs(attr->sockaddr.ipv4.sin_port));

	    } else if (attr->sockaddr.addr.sa_family == pj_AF_INET6()) {
		len = pj_ansi_snprintf(p, end-p,
				       ", IPv6 addr present\n");
	    } else {
		len = pj_ansi_snprintf(p, end-p,
				       ", INVALID ADDRESS FAMILY!\n");
	    }
	    APPLY();
	}
	break;

    case PJ_STUN_ATTR_CHANNEL_NUMBER:
	{
	    const pj_stun_uint_attr *attr;

	    attr = (const pj_stun_uint_attr*)ahdr;
	    len = pj_ansi_snprintf(p, end-p,
				   ", chnum=%u (0x%x)\n",
				   (int)PJ_STUN_GET_CH_NB(attr->value),
				   (int)PJ_STUN_GET_CH_NB(attr->value));
	    APPLY();
	}
	break;

    case PJ_STUN_ATTR_CHANGE_REQUEST:
    case PJ_STUN_ATTR_LIFETIME:
    case PJ_STUN_ATTR_BANDWIDTH:
    case PJ_STUN_ATTR_REQ_ADDR_TYPE:
    case PJ_STUN_ATTR_EVEN_PORT:
    case PJ_STUN_ATTR_REQ_TRANSPORT:
    case PJ_STUN_ATTR_TIMER_VAL:
    case PJ_STUN_ATTR_PRIORITY:
    case PJ_STUN_ATTR_FINGERPRINT:
    case PJ_STUN_ATTR_REFRESH_INTERVAL:
    case PJ_STUN_ATTR_ICMP:
	{
	    const pj_stun_uint_attr *attr;

	    attr = (const pj_stun_uint_attr*)ahdr;
	    len = pj_ansi_snprintf(p, end-p,
				   ", value=%u (0x%x)\n",
				   (pj_uint32_t)attr->value,
				   (pj_uint32_t)attr->value);
	    APPLY();
	}
	break;

    case PJ_STUN_ATTR_USERNAME:
    case PJ_STUN_ATTR_PASSWORD:
    case PJ_STUN_ATTR_REALM:
    case PJ_STUN_ATTR_NONCE:
    case PJ_STUN_ATTR_SOFTWARE:
	{
	    const pj_stun_string_attr *attr;

	    attr = (pj_stun_string_attr*)ahdr;
	    len = pj_ansi_snprintf(p, end-p,
				   ", value=\"%.*s\"\n",
				   (int)attr->value.slen,
				   attr->value.ptr);
	    APPLY();
	}
	break;

    case PJ_STUN_ATTR_ERROR_CODE:
	{
	    const pj_stun_errcode_attr *attr;

	    attr = (const pj_stun_errcode_attr*) ahdr;
	    len = pj_ansi_snprintf(p, end-p,
				   ", err_code=%d, reason=\"%.*s\"\n",
				   attr->err_code,
				   (int)attr->reason.slen,
				   attr->reason.ptr);
	    APPLY();
	}
	break;

    case PJ_STUN_ATTR_UNKNOWN_ATTRIBUTES:
	{
	    const pj_stun_unknown_attr *attr;
	    unsigned j;

	    attr = (const pj_stun_unknown_attr*) ahdr;

	    len = pj_ansi_snprintf(p, end-p,
				   ", unknown list:");
	    APPLY();

	    for (j=0; j<attr->attr_count; ++j) {
		len = pj_ansi_snprintf(p, end-p,
				       " %d",
				       (int)attr->attrs[j]);
		APPLY();
	    }
	}
	break;

    case PJ_STUN_ATTR_MESSAGE_INTEGRITY:
	{
	    const pj_stun_msgint_attr *attr;

	    attr = (const pj_stun_msgint_attr*) ahdr;
	    len = print_binary(p, end-p, attr->hmac, 20);
	    APPLY();
	}
	break;

    case PJ_STUN_ATTR_DATA:
	{
	    const pj_stun_binary_attr *attr;

	    attr = (const pj_stun_binary_attr*) ahdr;
	    len = print_binary(p, end-p, attr->data, attr->length);
	    APPLY();
	}
	break;
    case PJ_STUN_ATTR_ICE_CONTROLLED:
    case PJ_STUN_ATTR_ICE_CONTROLLING:
    case PJ_STUN_ATTR_RESERVATION_TOKEN:
	{
	    const pj_stun_uint64_attr *attr;
	    pj_uint8_t data[8];
	    int i;

	    attr = (const pj_stun_uint64_attr*) ahdr;

	    for (i=0; i<8; ++i)
		data[i] = ((const pj_uint8_t*)&attr->value)[7-i];

	    len = print_binary(p, end-p, data, 8);
	    APPLY();
	}
	break;
    case PJ_STUN_ATTR_USE_CANDIDATE:
    case PJ_STUN_ATTR_DONT_FRAGMENT:
    default:
	len = pj_ansi_snprintf(p, end-p, "\n");
	APPLY();
	break;
    }

    return (p-buffer);

on_return:
    return len;
}


/*
 * Dump STUN message to a printable string output.
 */
PJ_DEF(char*) pj_stun_msg_dump(const pj_stun_msg *msg,
			       char *buffer,
			       unsigned length,
			       unsigned *printed_len)
{
    char *p, *end;
    int len;
    unsigned i;

    PJ_ASSERT_RETURN(msg && buffer && length, NULL);

    PJ_CHECK_STACK();
    
    p = buffer;
    end = buffer + length;

    len = pj_ansi_snprintf(p, end-p, "STUN %s %s\n",
			   pj_stun_get_method_name(msg->hdr.type),
			   pj_stun_get_class_name(msg->hdr.type));
    APPLY();

    len = pj_ansi_snprintf(p, end-p, 
			   " Hdr: length=%d, magic=%08x, tsx_id=%08x%08x%08x\n"
			   " Attributes:\n",
			   msg->hdr.length,
			   msg->hdr.magic,
			   pj_ntohl(*(pj_uint32_t*)&msg->hdr.tsx_id[0]),
			   pj_ntohl(*(pj_uint32_t*)&msg->hdr.tsx_id[4]),
			   pj_ntohl(*(pj_uint32_t*)&msg->hdr.tsx_id[8]));
    APPLY();

    for (i=0; i<msg->attr_count; ++i) {
	len = print_attr(p, end-p, msg->attr[i]);
	APPLY();
    }

on_return:
    *p = '\0';
    if (printed_len)
	*printed_len = (p-buffer);
    return buffer;

#undef APPLY
}


#endif	/* PJ_LOG_MAX_LEVEL > 0 */

