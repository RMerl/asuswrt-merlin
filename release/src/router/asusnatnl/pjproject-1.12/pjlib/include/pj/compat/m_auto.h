/* pjlib/include/pj/compat/m_auto.h.  Generated from m_auto.h.in by configure.  */
/* $Id: m_auto.h.in 3295 2010-08-25 12:51:29Z bennylp $ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
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
#ifndef __PJ_COMPAT_M_AUTO_H__
#define __PJ_COMPAT_M_AUTO_H__

/**
 * @file m_auto.h
 * @brief Automatically generated process definition file.
 */

/* Machine name, filled in by autoconf script */
#define PJ_M_NAME "arm"

/* Endianness. It's reported on pjsip list on 09/02/13 that autoconf
 * endianness detection failed for universal build, so special case
 * for it here. Thanks Ruud Klaver for the fix.
 */
#ifdef PJ_DARWINOS
#  ifdef __BIG_ENDIAN__
#    define WORDS_BIGENDIAN	1
#  endif
#else
    /* Endianness, as detected by autoconf */
/* #  undef WORDS_BIGENDIAN */
#endif

#ifdef WORDS_BIGENDIAN
#  define PJ_IS_LITTLE_ENDIAN	0
#  define PJ_IS_BIG_ENDIAN	1
#else
#  define PJ_IS_LITTLE_ENDIAN	1
#  define PJ_IS_BIG_ENDIAN	0
#endif


/* Specify if floating point is present/desired */
#define PJ_HAS_FLOATING_POINT 1

/* Deprecated */
#define PJ_HAS_PENTIUM		0

#endif	/* __PJ_COMPAT_M_AUTO_H__ */

