/* Copyright (C) 1995, 1996, 1997, 2000 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _SYS_CACHECTL_H
#define _SYS_CACHECTL_H 1

#include <features.h>

/*
 * Get the kernel definition for the op bits.
 */
#include <asm/cachectl.h>

__BEGIN_DECLS

#ifdef __USE_MISC
extern int cachectl (void *addr, __const int nbytes, __const int op) __THROW;
#endif
extern int __cachectl (void *addr, __const int nbytes, __const int op) __THROW;
#ifdef __USE_MISC
extern int cacheflush (void *addr, __const int nbytes, __const int op) __THROW;
#endif
extern int _flush_cache (char *addr, __const int nbytes, __const int op) __THROW;

__END_DECLS

#endif /* sys/cachectl.h */
