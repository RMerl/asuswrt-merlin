/*  Copyright (C) 2003     Manuel Novoa III
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  The GNU C Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with the GNU C Library; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 USA.
 */

/*  ATTENTION!   ATTENTION!   ATTENTION!   ATTENTION!   ATTENTION!
 *
 *  Besides uClibc, I'm using this code in my libc for elks, which is
 *  a 16-bit environment with a fairly limited compiler.  It would make
 *  things much easier for me if this file isn't modified unnecessarily.
 *  In particular, please put any new or replacement functions somewhere
 *  else, and modify the makefile to use your version instead.
 *  Thanks.  Manuel
 *
 *  ATTENTION!   ATTENTION!   ATTENTION!   ATTENTION!   ATTENTION! */

#ifndef _UCLIBC_TOUPLOW_H
#define _UCLIBC_TOUPLOW_H

#include <features.h>
#include <bits/types.h>

/* glibc uses the equivalent of - typedef __int32_t __ctype_touplow_t; */

typedef __uint16_t __ctype_mask_t;

#ifdef __UCLIBC_HAS_CTYPE_SIGNED__

typedef __int16_t __ctype_touplow_t;
#define __UCLIBC_CTYPE_B_TBL_OFFSET       128
#define __UCLIBC_CTYPE_TO_TBL_OFFSET      128

#else

typedef unsigned char __ctype_touplow_t;
#define __UCLIBC_CTYPE_B_TBL_OFFSET       1
#define __UCLIBC_CTYPE_TO_TBL_OFFSET      0

#endif

#endif /* _UCLIBC_TOUPLOW_H */

