/* Copyright (C) 2005       Manuel Novoa III    <mjn3@codepoet.org>
 *
 * Dedicated to Toni.  See uClibc/DEDICATION.mjn3 for details.
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

#ifndef _UCLIBC_VA_COPY_H
#define _UCLIBC_VA_COPY_H 1

#include <stdarg.h>

/* Deal with pre-C99 compilers. */
#ifndef va_copy

#ifdef __va_copy
#define va_copy(A,B)	__va_copy(A,B)
#else
#warning Neither va_copy (C99/SUSv3) or __va_copy is defined.  Using a simple copy instead.  But you should really check that this is appropriate and supply an arch-specific override if necessary.
	/* the glibc manual suggests that this will usually suffice when
        __va_copy doesn't exist.  */
#define va_copy(A,B)	A = B
#endif

#endif /* va_copy */

#endif /* _UCLIBC_VA_COPY_H */
