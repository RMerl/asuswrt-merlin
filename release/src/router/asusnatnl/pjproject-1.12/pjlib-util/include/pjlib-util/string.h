/* $Id: string.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJLIB_UTIL_STRING_H__
#define __PJLIB_UTIL_STRING_H__

/**
 * @file string.h
 * @brief More string functions.
 */

#include <pj/types.h>
#include <pjlib-util/scanner.h>

/**
 * @defgroup PJLIB_UTIL_STRING String Escaping Utilities
 * @ingroup PJLIB_TEXT
 * @{
 */

PJ_BEGIN_DECL

/**
 * Unescape string. If source string does not contain any escaped
 * characters, the function would simply return the original string.
 * Otherwise a new string will be allocated.
 *
 * @param pool	    Pool to allocate the string.
 * @param src	    Source string to unescape.
 *
 * @return	    String with no escaped characters.
 */
PJ_DECL(pj_str_t) pj_str_unescape( pj_pool_t *pool, const pj_str_t *src);

/**
 * Unescape string to destination.
 *
 * @param dst	    Target string.
 * @param src	    Source string.
 *
 * @return	    Target string.
 */
PJ_DECL(pj_str_t*) pj_strcpy_unescape(pj_str_t *dst, const pj_str_t *src);

/**
 * Copy string to destination while escaping reserved characters, up to
 * the specified maximum length.
 *
 * @param dst	    Target string.
 * @param src	    Source string.
 * @param max	    Maximum length to copy to target string.
 * @param unres	    Unreserved characters, which are allowed to appear 
 *		    unescaped.
 *
 * @return	    The target string if all characters have been copied 
 *		    successfully, or NULL if there's not enough buffer to
 *		    escape the strings.
 */
PJ_DECL(pj_str_t*) pj_strncpy_escape(pj_str_t *dst, const pj_str_t *src,
				     pj_ssize_t max, const pj_cis_t *unres);


/**
 * Copy string to destination while escaping reserved characters, up to
 * the specified maximum length.
 *
 * @param dst	    Target string.
 * @param src	    Source string.
 * @param max	    Maximum length to copy to target string.
 * @param unres	    Unreserved characters, which are allowed to appear 
 *		    unescaped.
 *
 * @return	    The length of the destination, or -1 if there's not
 *		    enough buffer.
 */
PJ_DECL(pj_ssize_t) pj_strncpy2_escape(char *dst, const pj_str_t *src,
				       pj_ssize_t max, const pj_cis_t *unres);

PJ_END_DECL


/**
 * @}
 */

#endif	/* __PJLIB_UTIL_STRING_H__ */
