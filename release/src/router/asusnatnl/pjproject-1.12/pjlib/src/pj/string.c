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
#include <pj/string.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/ctype.h>
#include <pj/rand.h>
#include <pj/os.h>

#if PJ_FUNCTIONS_ARE_INLINED==0
#  include <pj/string_i.h>
#endif


PJ_DEF(char*) pj_strstr(const pj_str_t *str, const pj_str_t *substr)
{
    const char *s, *ends;

    /* Special case when substr is zero */
    if (substr->slen == 0) {
	return (char*)str->ptr;
    }

    s = str->ptr;
    ends = str->ptr + str->slen - substr->slen;
    for (; s<=ends; ++s) {
	if (pj_ansi_strncmp(s, substr->ptr, substr->slen)==0)
	    return (char*)s;
    }
    return NULL;
}


PJ_DEF(char*) pj_stristr(const pj_str_t *str, const pj_str_t *substr)
{
    const char *s, *ends;

    /* Special case when substr is zero */
    if (substr->slen == 0) {
	return (char*)str->ptr;
    }

    s = str->ptr;
    ends = str->ptr + str->slen - substr->slen;
    for (; s<=ends; ++s) {
	if (pj_ansi_strnicmp(s, substr->ptr, substr->slen)==0)
	    return (char*)s;
    }
    return NULL;
}


PJ_DEF(pj_str_t*) pj_strltrim( pj_str_t *str )
{
    char *end = str->ptr + str->slen;
    register char *p = str->ptr;
    while (p < end && pj_isspace(*p))
	++p;
    str->slen -= (p - str->ptr);
    str->ptr = p;
    return str;
}

PJ_DEF(pj_str_t*) pj_strrtrim( pj_str_t *str )
{
    char *end = str->ptr + str->slen;
    register char *p = end - 1;
    while (p >= str->ptr && pj_isspace(*p))
        --p;
    str->slen -= ((end - p) - 1);
    return str;
}

PJ_DEF(char*) pj_create_random_string(char *str, pj_size_t len)
{
    unsigned i;
    char *p = str;

    PJ_CHECK_STACK();

    for (i=0; i<len/8; ++i) {
	pj_uint32_t val = pj_rand();
	pj_val_to_hex_digit( (val & 0xFF000000) >> 24, p+0 );
	pj_val_to_hex_digit( (val & 0x00FF0000) >> 16, p+2 );
	pj_val_to_hex_digit( (val & 0x0000FF00) >>  8, p+4 );
	pj_val_to_hex_digit( (val & 0x000000FF) >>  0, p+6 );
	p += 8;
    }
    for (i=i * 8; i<len; ++i) {
	*p++ = pj_hex_digits[ pj_rand() & 0x0F ];
    }
    return str;
}


PJ_DEF(unsigned long) pj_strtoul(const pj_str_t *str)
{
    unsigned long value;
    unsigned i;

    PJ_CHECK_STACK();

    value = 0;
    for (i=0; i<(unsigned)str->slen; ++i) {
	if (!pj_isdigit(str->ptr[i]))
	    break;
	value = value * 10 + (str->ptr[i] - '0');
    }
    return value;
}

PJ_DEF(unsigned long) pj_strtoul2(const pj_str_t *str, pj_str_t *endptr,
				  unsigned base)
{
    unsigned long value;
    unsigned i;

    PJ_CHECK_STACK();

    value = 0;
    if (base <= 10) {
	for (i=0; i<(unsigned)str->slen; ++i) {
	    unsigned c = (str->ptr[i] - '0');
	    if (c >= base)
		break;
	    value = value * base + c;
	}
    } else if (base == 16) {
	for (i=0; i<(unsigned)str->slen; ++i) {
	    if (!pj_isxdigit(str->ptr[i]))
		break;
	    value = value * 16 + pj_hex_digit_to_val(str->ptr[i]);
	}
    } else {
	pj_assert(!"Unsupported base");
	i = 0;
	value = 0xFFFFFFFFUL;
    }

    if (endptr) {
	endptr->ptr = str->ptr + i;
	endptr->slen = str->slen - i;
    }

    return value;
}

PJ_DEF(int) pj_utoa(unsigned long val, char *buf)
{
    return pj_utoa_pad(val, buf, 0, 0);
}

PJ_DEF(int) pj_utoa_pad( unsigned long val, char *buf, int min_dig, int pad)
{
    char *p;
    int len;

    PJ_CHECK_STACK();

    p = buf;
    do {
        unsigned long digval = (unsigned long) (val % 10);
        val /= 10;
        *p++ = (char) (digval + '0');
    } while (val > 0);

    len = p-buf;
    while (len < min_dig) {
	*p++ = (char)pad;
	++len;
    }
    *p-- = '\0';

    do {
        char temp = *p;
        *p = *buf;
        *buf = temp;
        --p;
        ++buf;
    } while (buf < p);

    return len;
}


