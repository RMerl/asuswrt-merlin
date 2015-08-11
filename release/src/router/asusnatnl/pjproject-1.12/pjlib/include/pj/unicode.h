/* $Id: unicode.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_UNICODE_H__
#define __PJ_UNICODE_H__

#include <pj/types.h>


/**
 * @defgroup PJ_UNICODE Unicode Support
 * @ingroup PJ_MISC
 * @{
 */

PJ_BEGIN_DECL


/**
 * @file unicode.h
 * @brief Provides Unicode conversion for Unicode OSes
 */

/**
 * Convert ANSI strings to Unicode strings.
 *
 * @param str		    The ANSI string to be converted.
 * @param len		    The length of the input string.
 * @param wbuf		    Buffer to hold the Unicode string output.
 * @param wbuf_count	    Buffer size, in number of elements (not bytes).
 *
 * @return		    The Unicode string, NULL terminated.
 */
PJ_DECL(wchar_t*) pj_ansi_to_unicode(const char *str, pj_size_t len,
				     wchar_t *wbuf, pj_size_t wbuf_count);


/**
 * Convert Unicode string to ANSI string.
 *
 * @param wstr		    The Unicode string to be converted.
 * @param len		    The length of the input string.
 * @param buf		    Buffer to hold the ANSI string output.
 * @param buf_size	    Size of the output buffer.
 *
 * @return		    The ANSI string, NULL terminated.
 */
PJ_DECL(char*) pj_unicode_to_ansi(const wchar_t *wstr, pj_size_t len,
				  char *buf, pj_size_t buf_size);


#if defined(PJ_NATIVE_STRING_IS_UNICODE) && PJ_NATIVE_STRING_IS_UNICODE!=0

/**
 * This macro is used to declare temporary Unicode buffer for ANSI to 
 * Unicode conversion, and should be put in declaration section of a block.
 * When PJ_NATIVE_STRING_IS_UNICODE macro is not defined, this 
 * macro will expand to nothing.
 */
#   define PJ_DECL_UNICODE_TEMP_BUF(buf,size)   wchar_t buf[size];

/**
 * This macro will convert ANSI string to native, when the platform's
 * native string is Unicode (PJ_NATIVE_STRING_IS_UNICODE is non-zero).
 */
#   define PJ_STRING_TO_NATIVE(s,buf,max)	pj_ansi_to_unicode( \
						    s, strlen(s), \
						    buf, max)

/**
 * This macro is used to declare temporary ANSI buffer for Unicode to 
 * ANSI conversion, and should be put in declaration section of a block.
 * When PJ_NATIVE_STRING_IS_UNICODE macro is not defined, this 
 * macro will expand to nothing.
 */
#   define PJ_DECL_ANSI_TEMP_BUF(buf,size)	char buf[size];


/**
 * This macro will convert Unicode string to ANSI, when the platform's
 * native string is Unicode (PJ_NATIVE_STRING_IS_UNICODE is non-zero).
 */
#   define PJ_NATIVE_TO_STRING(cs,buf,max)	pj_unicode_to_ansi( \
						    cs, wcslen(cs), \
						    buf, max)

#else

/**
 * This macro is used to declare temporary Unicode buffer for ANSI to 
 * Unicode conversion, and should be put in declaration section of a block.
 * When PJ_NATIVE_STRING_IS_UNICODE macro is not defined, this 
 * macro will expand to nothing.
 */
#   define PJ_DECL_UNICODE_TEMP_BUF(var,size)
/**
 * This macro will convert ANSI string to native, when the platform's
 * native string is Unicode (PJ_NATIVE_STRING_IS_UNICODE is non-zero).
 */
#   define PJ_STRING_TO_NATIVE(s,buf,max)	((char*)s)
/**
 * This macro is used to declare temporary ANSI buffer for Unicode to 
 * ANSI conversion, and should be put in declaration section of a block.
 * When PJ_NATIVE_STRING_IS_UNICODE macro is not defined, this 
 * macro will expand to nothing.
 */
#   define PJ_DECL_ANSI_TEMP_BUF(buf,size)
/**
 * This macro will convert Unicode string to ANSI, when the platform's
 * native string is Unicode (PJ_NATIVE_STRING_IS_UNICODE is non-zero).
 */
#   define PJ_NATIVE_TO_STRING(cs,buf,max)	((char*)(const char*)cs)

#endif



PJ_END_DECL

/*
 * @}
 */


#endif	/* __PJ_UNICODE_H__ */
