/* libcomapt.h - Prototypes for AC_REPLACE_FUNCtions.
 * Copyright (C) 2010 Free Software Foundation, Inc.
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser general Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GCRY_LIBCOMPAT_H
#define GCRY_LIBCOMPAT_H

const char *_gcry_compat_identification (void);


#ifndef HAVE_GETPID
pid_t _gcry_getpid (void);
#define getpid() _gcry_getpid ()
#endif

#ifndef HAVE_CLOCK
clock_t _gcry_clock (void);
#define clock() _gcry_clock ()
#endif


#endif /*GCRY_LIBCOMPAT_H*/
