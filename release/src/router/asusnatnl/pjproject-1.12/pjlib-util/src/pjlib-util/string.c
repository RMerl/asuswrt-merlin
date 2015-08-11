/* $Id: string.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjlib-util/string.h>
#include <pj/ctype.h>
#include <pj/string.h>
#include <pj/pool.h>

PJ_DEF(pj_str_t) pj_str_unescape( pj_pool_t *pool, const pj_str_t *src_str)
{
    char *src = src_str->ptr;
    char *end = src + src_str->slen;
    pj_str_t dst_str;
    char *dst;
    
    if (pj_strchr(src_str, '%')==NULL)
	return *src_str;

    dst = dst_str.ptr = (char*) pj_pool_alloc(pool, src_str->slen);

    while (src != end) {
	if (*src == '%' && src < end-2 && pj_isxdigit(*(src+1)) && 
	    pj_isxdigit(*(src+2))) 
	{
	    *dst = (pj_uint8_t) ((pj_hex_digit_to_val(*(src+1)) << 4) + 
				 pj_hex_digit_to_val(*(src+2)));
	    ++dst;
	    src += 3;
	} else {
	    *dst++ = *src++;
	}
    }
    dst_str.slen = dst - dst_str.ptr;
    return dst_str;
}

PJ_DEF(pj_str_t*) pj_strcpy_unescape(pj_str_t *dst_str,
				     const pj_str_t *src_str)
{
    const char *src = src_str->ptr;
    const char *end = src + src_str->slen;
    char *dst = dst_str->ptr;
    
    while (src != end) {
	if (*src == '%' && src < end-2) {
	    *dst = (pj_uint8_t) ((pj_hex_digit_to_val(*(src+1)) << 4) + 
				 pj_hex_digit_to_val(*(src+2)));
	    ++dst;
	    src += 3;
	} else {
	    *dst++ = *src++;
	}
    }
    dst_str->slen = dst - dst_str->ptr;
    return dst_str;
}

PJ_DEF(pj_ssize_t) pj_strncpy2_escape( char *dst_str, const pj_str_t *src_str,
				       pj_ssize_t max, const pj_cis_t *unres)
{
    const char *src = src_str->ptr;
    const char *src_end = src + src_str->slen;
    char *dst = dst_str;
    char *dst_end = dst + max;

    if (max < src_str->slen)
	return -1;

    while (src != src_end && dst != dst_end) {
	if (pj_cis_match(unres, *src)) {
	    *dst++ = *src++;
	} else {
	    if (dst < dst_end-2) {
		*dst++ = '%';
		pj_val_to_hex_digit(*src, dst);
		dst+=2;
		++src;
	    } else {
		break;
	    }
	}
    }

    return src==src_end ? dst-dst_str : -1;
}

PJ_DEF(pj_str_t*) pj_strncpy_escape(pj_str_t *dst_str, 
				    const pj_str_t *src_str,
				    pj_ssize_t max, const pj_cis_t *unres)
{
    dst_str->slen = pj_strncpy2_escape(dst_str->ptr, src_str, max, unres);
    return dst_str->slen < 0 ? NULL : dst_str;
}

