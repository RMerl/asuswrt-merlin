/* $Id: cc_msvc.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_COMPAT_CC_MSVC_H__
#define __PJ_COMPAT_CC_MSVC_H__

/**
 * @file cc_msvc.h
 * @brief Describes Microsoft Visual C compiler specifics.
 */

#ifndef _MSC_VER
#  error "This header file is only for Visual C compiler!"
#endif

#define PJ_CC_NAME	    "msvc"
#define PJ_CC_VER_1	    (_MSC_VER/100)
#define PJ_CC_VER_2	    (_MSC_VER%100)
#define PJ_CC_VER_3	    0

/* Disable CRT deprecation warnings. */
#if PJ_CC_VER_1 >= 8 && !defined(_CRT_SECURE_NO_DEPRECATE)
#   define _CRT_SECURE_NO_DEPRECATE
#endif
#if PJ_CC_VER_1 >= 8 && !defined(_CRT_SECURE_NO_WARNINGS)
#   define _CRT_SECURE_NO_WARNINGS
    /* The above doesn't seem to work, at least on VS2005, so lets use
     * this construct as well.
     */
#   pragma warning(disable: 4996)
#endif

#pragma warning(disable: 4127) // conditional expression is constant
#pragma warning(disable: 4611) // not wise to mix setjmp with C++
#pragma warning(disable: 4514) // unref. inline function has been removed
#ifdef NDEBUG
#  pragma warning(disable: 4702) // unreachable code
#  pragma warning(disable: 4710) // function is not inlined.
#  pragma warning(disable: 4711) // function selected for auto inline expansion
#endif

#ifdef __cplusplus
#  define PJ_INLINE_SPECIFIER	inline
#else
#  define PJ_INLINE_SPECIFIER	static __inline
#endif

#define PJ_EXPORT_DECL_SPECIFIER    __declspec(dllexport)
#define PJ_EXPORT_DEF_SPECIFIER	    __declspec(dllexport)
#define PJ_IMPORT_DECL_SPECIFIER    __declspec(dllimport)

#define PJ_THREAD_FUNC	
#define PJ_NORETURN		__declspec(noreturn)
#define PJ_ATTR_NORETURN	

#define PJ_HAS_INT64	1

typedef __int64 pj_int64_t;
typedef unsigned __int64 pj_uint64_t;

#define PJ_INT64(val)		val##i64
#define PJ_UINT64(val)		val##ui64
#define PJ_INT64_FMT		"I64"

#define PJ_UNREACHED(x)	    	

#endif	/* __PJ_COMPAT_CC_MSVC_H__ */

