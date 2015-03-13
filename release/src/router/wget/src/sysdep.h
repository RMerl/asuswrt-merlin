/* Dirty system-dependent hacks.
   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2008, 2009, 2010, 2011 Free Software Foundation,
   Inc.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or (at
your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget.  If not, see <http://www.gnu.org/licenses/>.

Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by linking or
combining it with the OpenSSL project's OpenSSL library (or a
modified version of that library), containing parts covered by the
terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
grants you additional permission to convey the resulting work.
Corresponding Source for a non-source form of such a combination
shall include the source code for the parts of OpenSSL used as well
as that of the covered work.  */

/* This file is included by wget.h.  Random .c files need not include
   it.  */

#ifndef SYSDEP_H
#define SYSDEP_H

/* Testing for __sun is not enough because it's also defined on SunOS.  */
#ifdef __sun
# ifdef __SVR4
#  define solaris
# endif
#endif

#if defined(__INTERIX) && !defined(_ALL_SOURCE)
# define _ALL_SOURCE
#endif

/* The "namespace tweaks" below attempt to set a friendly "compilation
   environment" under popular operating systems.  Default compilation
   environment often means that some functions that are "extensions"
   are not declared -- `strptime' is one example.

   But non-default environments can expose bugs in the system header
   files, crippling compilation in _very_ non-obvious ways.  Because
   of that, we define them only on well-tested architectures where we
   know they will work.  */

#undef NAMESPACE_TWEAKS

#ifdef solaris
# define NAMESPACE_TWEAKS
#endif

#if defined(__linux__) || defined(__GLIBC__)
# define NAMESPACE_TWEAKS
#endif

#ifdef NAMESPACE_TWEAKS

/* Request the "Unix 98 compilation environment". */
#define _XOPEN_SOURCE 500

#endif /* NAMESPACE_TWEAKS */


/* Alloca declaration, based on recommendation in the Autoconf manual.
   These have to be after the above namespace tweaks, but before any
   non-preprocessor code.  */

#include <alloca.h>

/* Must include these, so we can test for the missing stat macros and
   define them as necessary.  */
#include <sys/types.h>
#include <sys/stat.h>

#include <stdint.h>
#include <inttypes.h>

#ifdef WINDOWS
/* Windows doesn't have some functions normally found on Unix-like
   systems, such as strcasecmp, strptime, etc.  Include mswindows.h so
   we get the declarations for their replacements in mswindows.c, as
   well as to pick up Windows-specific includes and constants.  To be
   able to test for such features, the file must be included as early
   as possible.  */
# include "mswindows.h"
#endif

/* Provided by gnulib on systems that don't have it: */
# include <stdbool.h>

#ifndef struct_stat
# define struct_stat struct stat
#endif
#ifndef struct_fstat
# define struct_fstat struct stat
#endif

#include <intprops.h>

/* For CHAR_BIT, LONG_MAX, etc. */
#include <limits.h>

#ifndef CHAR_BIT
# define CHAR_BIT 8
#endif

/* These are defined in cmpt.c if missing, so we must declare
   them.  */
#ifndef HAVE_STRCASECMP
int strcasecmp ();
#endif
#ifndef HAVE_STRNCASECMP
int strncasecmp ();
#endif
#ifndef HAVE_STRPTIME
char *strptime ();
#endif
#ifndef HAVE_TIMEGM
# include <time.h>
time_t timegm (struct tm *);
#endif
#ifndef HAVE_MEMRCHR
void *memrchr (const void *, int, size_t);
#endif

/* These are defined in snprintf.c.  It would be nice to have an
   snprintf.h, though.  */
#ifndef HAVE_SNPRINTF
int snprintf (char *str, size_t count, const char *fmt, ...);
#endif
#ifndef HAVE_VSNPRINTF
#include <stdarg.h>
int vsnprintf (char *str, size_t count, const char *fmt, va_list arg);
#endif

/* Some systems (Linux libc5, "NCR MP-RAS 3.0", and others) don't
   provide MAP_FAILED, a symbolic constant for the value returned by
   mmap() when it doesn't work.  Usually, this constant should be -1.
   This only makes sense for files that use mmap() and include
   sys/mman.h *before* sysdep.h, but doesn't hurt others.  */

#ifndef MAP_FAILED
# define MAP_FAILED ((void *) -1)
#endif

/* Enable system fnmatch only on systems where fnmatch.h is usable.
   If the fnmatch on your system is buggy, undef this symbol and a
   replacement implementation will be used instead.  */
#ifdef HAVE_WORKING_FNMATCH_H
# define SYSTEM_FNMATCH
#endif

#include <fnmatch.h>

/* Provide sig_atomic_t if the system doesn't.  */
#ifndef HAVE_SIG_ATOMIC_T
typedef int sig_atomic_t;
#endif

/* Provide uint32_t on the platforms that don't define it.  Although
   most code should be agnostic about integer sizes, some code really
   does need a 32-bit integral type.  Such code should use uint32_t.
   (The exception is gnu-md5.[ch], which uses its own detection for
   portability across platforms.)  */

#ifndef HAVE_UINT32_T
# if SIZEOF_INT == 4
typedef unsigned int uint32_t;
# else
#  if SIZEOF_LONG == 4
typedef unsigned long uint32_t;
#  else
#   if SIZEOF_SHORT == 4
typedef unsigned short uint32_t;
#   else
 #error "Cannot determine a 32-bit unsigned integer type"
#   endif
#  endif
# endif
#endif

/* If uintptr_t isn't defined, simply typedef it to unsigned long. */
#ifndef HAVE_UINTPTR_T
typedef unsigned long uintptr_t;
#endif

/* If intptr_t isn't defined, simply typedef it to long. */
#ifndef HAVE_INTPTR_T
typedef long intptr_t;
#endif

#endif /* SYSDEP_H */
