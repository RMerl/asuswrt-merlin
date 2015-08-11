/* $Id: string_i.h 3553 2011-05-05 06:14:19Z nanang $ */
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

PJ_IDEF(pj_str_t) pj_str(char *str)
{
    pj_str_t dst;
    dst.ptr = str;
    dst.slen = str ? pj_ansi_strlen(str) : 0;
    return dst;
}

PJ_IDEF(pj_str_t*) pj_strdup(pj_pool_t *pool,
			      pj_str_t *dst,
			      const pj_str_t *src)
{
    /* Without this, destination will be corrupted */
    if (dst == src)
	return dst;

    if (src->slen) {
	dst->ptr = (char*)pj_pool_alloc(pool, src->slen);
	pj_memcpy(dst->ptr, src->ptr, src->slen);
    }
    dst->slen = src->slen;
    return dst;
}

PJ_IDEF(pj_str_t*) pj_strdup_with_null( pj_pool_t *pool,
					pj_str_t *dst,
					const pj_str_t *src)
{
    dst->ptr = (char*)pj_pool_alloc(pool, src->slen+1);
    if (src->slen) {
	pj_memcpy(dst->ptr, src->ptr, src->slen);
    }
    dst->slen = src->slen;
    dst->ptr[dst->slen] = '\0';
    return dst;
}

PJ_IDEF(pj_str_t*) pj_strdup2(pj_pool_t *pool,
			      pj_str_t *dst,
			      const char *src)
{
    dst->slen = src ? pj_ansi_strlen(src) : 0;
    if (dst->slen) {
	dst->ptr = (char*)pj_pool_alloc(pool, dst->slen);
	pj_memcpy(dst->ptr, src, dst->slen);
    } else {
	dst->ptr = NULL;
    }
    return dst;
}

PJ_IDEF(pj_str_t*) pj_strdup2_with_null( pj_pool_t *pool,
					 pj_str_t *dst,
					 const char *src)
{
    dst->slen = src ? pj_ansi_strlen(src) : 0;
    dst->ptr = (char*)pj_pool_alloc(pool, dst->slen+1);
    if (dst->slen) {
	pj_memcpy(dst->ptr, src, dst->slen);
    }
    dst->ptr[dst->slen] = '\0';
    return dst;
}

PJ_IDEF(pj_str_t) pj_strdup3(pj_pool_t *pool, const char *src)
{
    pj_str_t temp;
    pj_strdup2(pool, &temp, src);
    return temp;
}

PJ_IDEF(pj_str_t*) pj_strassign( pj_str_t *dst, pj_str_t *src )
{
    dst->ptr = src->ptr;
    dst->slen = src->slen;
    return dst;
}

PJ_IDEF(pj_str_t*) pj_strcpy(pj_str_t *dst, const pj_str_t *src)
{
    dst->slen = src->slen;
    if (src->slen > 0)
	pj_memcpy(dst->ptr, src->ptr, src->slen);
    return dst;
}

PJ_IDEF(pj_str_t*) pj_strcpy2(pj_str_t *dst, const char *src)
{
    dst->slen = src ? pj_ansi_strlen(src) : 0;
    if (dst->slen > 0)
	pj_memcpy(dst->ptr, src, dst->slen);
    return dst;
}

PJ_IDEF(pj_str_t*) pj_strncpy( pj_str_t *dst, const pj_str_t *src, 
			       pj_ssize_t max)
{
    if (max > src->slen) max = src->slen;
    pj_memcpy(dst->ptr, src->ptr, max);
    dst->slen = max;
    return dst;
}

PJ_IDEF(pj_str_t*) pj_strncpy_with_null( pj_str_t *dst, const pj_str_t *src,
					 pj_ssize_t max)
{
    if (max <= src->slen)
	max = max-1;
    else
	max = src->slen;

    pj_memcpy(dst->ptr, src->ptr, max);
    dst->ptr[max] = '\0';
    dst->slen = max;
    return dst;
}


PJ_IDEF(int) pj_strcmp( const pj_str_t *str1, const pj_str_t *str2)
{
    if (str1->slen == 0) {
	return str2->slen==0 ? 0 : -1;
    } else if (str2->slen == 0) {
	return 1;
    } else {
	int min = (str1->slen < str2->slen)? str1->slen : str2->slen;
	int res = pj_memcmp(str1->ptr, str2->ptr, min);
	if (res == 0) {
	    return (str1->slen < str2->slen) ? -1 :
		    (str1->slen == str2->slen ? 0 : 1);
	} else {
	    return res;
	}
    }
}

PJ_IDEF(int) pj_strncmp( const pj_str_t *str1, const pj_str_t *str2, 
			 pj_size_t len)
{
    pj_str_t copy1, copy2;

    if (len < (unsigned)str1->slen) {
	copy1.ptr = str1->ptr;
	copy1.slen = len;
	str1 = &copy1;
    }

    if (len < (unsigned)str2->slen) {
	copy2.ptr = str2->ptr;
	copy2.slen = len;
	str2 = &copy2;
    }

    return pj_strcmp(str1, str2);
}

PJ_IDEF(int) pj_strncmp2( const pj_str_t *str1, const char *str2, 
			  pj_size_t len)
{
    pj_str_t copy2;

    if (str2) {
	copy2.ptr = (char*)str2;
	copy2.slen = pj_ansi_strlen(str2);
    } else {
	copy2.slen = 0;
    }

    return pj_strncmp(str1, &copy2, len);
}

PJ_IDEF(int) pj_strcmp2( const pj_str_t *str1, const char *str2 )
{
    pj_str_t copy2;

    if (str2) {
	copy2.ptr = (char*)str2;
	copy2.slen = pj_ansi_strlen(str2);
    } else {
	copy2.ptr = NULL;
	copy2.slen = 0;
    }

    return pj_strcmp(str1, &copy2);
}

PJ_IDEF(int) pj_stricmp( const pj_str_t *str1, const pj_str_t *str2)
{
    if (str1->slen == 0) {
	return str2->slen==0 ? 0 : -1;
    } else if (str2->slen == 0) {
	return 1;
    } else {
	int min = (str1->slen < str2->slen)? str1->slen : str2->slen;
	int res = pj_ansi_strnicmp(str1->ptr, str2->ptr, min);
	if (res == 0) {
	    return (str1->slen < str2->slen) ? -1 :
		    (str1->slen == str2->slen ? 0 : 1);
	} else {
	    return res;
	}
    }
}

#if defined(PJ_HAS_STRICMP_ALNUM) && PJ_HAS_STRICMP_ALNUM!=0
PJ_IDEF(int) strnicmp_alnum( const char *str1, const char *str2,
			     int len)
{
    if (len==0)
	return 0;
    else {
	register const pj_uint32_t *p1 = (pj_uint32_t*)str1, 
		                   *p2 = (pj_uint32_t*)str2;
	while (len > 3 && (*p1 & 0x5F5F5F5F)==(*p2 & 0x5F5F5F5F))
	    ++p1, ++p2, len-=4;

	if (len > 3)
	    return -1;
#if defined(PJ_IS_LITTLE_ENDIAN) && PJ_IS_LITTLE_ENDIAN!=0
	else if (len==3)
	    return ((*p1 & 0x005F5F5F)==(*p2 & 0x005F5F5F)) ? 0 : -1;
	else if (len==2)
	    return ((*p1 & 0x00005F5F)==(*p2 & 0x00005F5F)) ? 0 : -1;
	else if (len==1)
	    return ((*p1 & 0x0000005F)==(*p2 & 0x0000005F)) ? 0 : -1;
#else
	else if (len==3)
	    return ((*p1 & 0x5F5F5F00)==(*p2 & 0x5F5F5F00)) ? 0 : -1;
	else if (len==2)
	    return ((*p1 & 0x5F5F0000)==(*p2 & 0x5F5F0000)) ? 0 : -1;
	else if (len==1)
	    return ((*p1 & 0x5F000000)==(*p2 & 0x5F000000)) ? 0 : -1;
#endif
	else 
	    return 0;
    }
}

PJ_IDEF(int) pj_stricmp_alnum(const pj_str_t *str1, const pj_str_t *str2)
{
    register int len = str1->slen;

    if (len != str2->slen) {
	return (len < str2->slen) ? -1 : 1;
    } else if (len == 0) {
	return 0;
    } else {
	register const pj_uint32_t *p1 = (pj_uint32_t*)str1->ptr, 
		                   *p2 = (pj_uint32_t*)str2->ptr;
	while (len > 3 && (*p1 & 0x5F5F5F5F)==(*p2 & 0x5F5F5F5F))
	    ++p1, ++p2, len-=4;

	if (len > 3)
	    return -1;
#if defined(PJ_IS_LITTLE_ENDIAN) && PJ_IS_LITTLE_ENDIAN!=0
	else if (len==3)
	    return ((*p1 & 0x005F5F5F)==(*p2 & 0x005F5F5F)) ? 0 : -1;
	else if (len==2)
	    return ((*p1 & 0x00005F5F)==(*p2 & 0x00005F5F)) ? 0 : -1;
	else if (len==1)
	    return ((*p1 & 0x0000005F)==(*p2 & 0x0000005F)) ? 0 : -1;
#else
	else if (len==3)
	    return ((*p1 & 0x5F5F5F00)==(*p2 & 0x5F5F5F00)) ? 0 : -1;
	else if (len==2)
	    return ((*p1 & 0x5F5F0000)==(*p2 & 0x5F5F0000)) ? 0 : -1;
	else if (len==1)
	    return ((*p1 & 0x5F000000)==(*p2 & 0x5F000000)) ? 0 : -1;
#endif
	else 
	    return 0;
    }
}
#endif	/* PJ_HAS_STRICMP_ALNUM */

PJ_IDEF(int) pj_stricmp2( const pj_str_t *str1, const char *str2)
{
    pj_str_t copy2;

    if (str2) {
	copy2.ptr = (char*)str2;
	copy2.slen = pj_ansi_strlen(str2);
    } else {
	copy2.ptr = NULL;
	copy2.slen = 0;
    }

    return pj_stricmp(str1, &copy2);
}

PJ_IDEF(int) pj_strnicmp( const pj_str_t *str1, const pj_str_t *str2, 
			  pj_size_t len)
{
    pj_str_t copy1, copy2;

    if (len < (unsigned)str1->slen) {
	copy1.ptr = str1->ptr;
	copy1.slen = len;
	str1 = &copy1;
    }

    if (len < (unsigned)str2->slen) {
	copy2.ptr = str2->ptr;
	copy2.slen = len;
	str2 = &copy2;
    }

    return pj_stricmp(str1, str2);
}

PJ_IDEF(int) pj_strnicmp2( const pj_str_t *str1, const char *str2, 
			   pj_size_t len)
{
    pj_str_t copy2;

    if (str2) {
	copy2.ptr = (char*)str2;
	copy2.slen = pj_ansi_strlen(str2);
    } else {
	copy2.slen = 0;
    }

    return pj_strnicmp(str1, &copy2, len);
}

PJ_IDEF(void) pj_strcat(pj_str_t *dst, const pj_str_t *src)
{
    if (src->slen) {
	pj_memcpy(dst->ptr + dst->slen, src->ptr, src->slen);
	dst->slen += src->slen;
    }
}

PJ_IDEF(void) pj_strcat2(pj_str_t *dst, const char *str)
{
    unsigned len = str? pj_ansi_strlen(str) : 0;
    if (len) {
	pj_memcpy(dst->ptr + dst->slen, str, len);
	dst->slen += len;
    }
}

PJ_IDEF(pj_str_t*) pj_strtrim( pj_str_t *str )
{
    pj_strltrim(str);
    pj_strrtrim(str);
    return str;
}

