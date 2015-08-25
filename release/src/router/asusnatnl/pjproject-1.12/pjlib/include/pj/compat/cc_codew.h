/* $Id: cc_codew.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_COMPAT_CC_CODEW_H__
#define __PJ_COMPAT_CC_CODEW_H__

/**
 * @file cc_codew.h
 * @brief Describes MetroWerks Code Warrior compiler specifics.
 */

#ifndef __MWERKS__
#  error "This file is only for Code Warrior!"
#endif

#define PJ_CC_NAME		"codewarrior"
#define PJ_CC_VER_1		((__MWERKS__ & 0xF000) >> 12)
#define PJ_CC_VER_2		((__MWERKS__ & 0x0F00) >> 8)
#define PJ_CC_VER_3		((__MWERKS__ & 0xFF))


#define PJ_INLINE_SPECIFIER	static inline
#define PJ_THREAD_FUNC	
#define PJ_NORETURN		
#define PJ_ATTR_NORETURN	

#define PJ_HAS_INT64		1

typedef long long pj_int64_t;
typedef unsigned long long pj_uint64_t;

#define PJ_INT64(val)		val##LL
#define PJ_UINT64(val)		val##LLU
#define PJ_INT64_FMT		"L"

#define PJ_UNREACHED(x)	    	

#endif	/* __PJ_COMPAT_CC_CODEW_H__ */

