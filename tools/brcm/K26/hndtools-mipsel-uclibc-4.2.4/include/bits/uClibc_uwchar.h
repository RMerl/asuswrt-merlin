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


/*  Define an internal unsigned int type __uwchar_t just large enough
 *  to hold a wchar_t.
 */

#ifndef _UCLIBC_UWCHAR_H
#define _UCLIBC_UWCHAR_H

#include <limits.h>
#include <stdint.h>

#if WCHAR_MIN == 0
typedef wchar_t				__uwchar_t;
#elif WCHAR_MAX <= USHRT_MAX
typedef unsigned short		__uwchar_t;
#elif WCHAR_MAX <= UINT_MAX
typedef unsigned int		__uwchar_t;
#elif WCHAR_MAX <= ULONG_MAX
typedef unsigned long		__uwchar_t;
#elif defined(ULLONG_MAX) && (WCHAR_MAX <= ULLONG_MAX)
typedef unsigned long long	__uwchar_t;
#elif WCHAR_MAX <= UINTMAX_MAX
typedef uintmax_t			__uwchar_t;
#else
#error Can not determine an appropriate type for __uwchar_t!
#endif

#endif /* _UCLIBC_UWCHAR_H */
