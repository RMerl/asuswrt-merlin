/* portab.h -- portability layer

   This file is part of the LZO real-time data compression library.

   Copyright (C) 2011 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2010 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2009 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2008 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2007 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2006 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2005 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2004 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2003 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2002 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2001 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 2000 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 1999 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 1998 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 1997 Markus Franz Xaver Johannes Oberhumer
   Copyright (C) 1996 Markus Franz Xaver Johannes Oberhumer
   All Rights Reserved.

   The LZO library is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   The LZO library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with the LZO library; see the file COPYING.
   If not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

   Markus F.X.J. Oberhumer
   <markus@oberhumer.com>
   http://www.oberhumer.com/opensource/lzo/
 */


#include "lzo/lzoconf.h"

#if (LZO_CC_MSC && (_MSC_VER >= 1000 && _MSC_VER < 1200))
   /* avoid '-W4' warnings in system header files */
#  pragma warning(disable: 4201 4214 4514)
#endif
#if (LZO_CC_MSC && (_MSC_VER >= 1300))
   /* avoid '-Wall' warnings in system header files */
#  pragma warning(disable: 4163 4255 4820)
   /* avoid warnings about inlining */
#  pragma warning(disable: 4710 4711)
#endif
#if (LZO_CC_MSC && (_MSC_VER >= 1400))
   /* avoid warnings when using "deprecated" POSIX functions */
#  pragma warning(disable: 4996)
#endif
#if (LZO_CC_PELLESC && (__POCC__ >= 290))
#  pragma warn(disable:2002)
#endif


/*************************************************************************
//
**************************************************************************/

#if defined(__LZO_MMODEL_HUGE) || defined(ACC_WANT_ACCLIB_GETOPT) || !(defined(LZO_LIBC_ISOC90) || defined(LZO_LIBC_ISOC99))

#include "examples/portab_a.h"

#else

/* INFO:
 *   The "portab_a.h" version above uses the ACC library to add
 *   support for ancient systems (like 16-bit DOS) and to provide
 *   some gimmicks like Windows high-resolution timers.
 *   Still, on any halfway modern machine you can also use the
 *   following pure ANSI-C code instead.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#if defined(CLK_TCK) && !defined(CLOCKS_PER_SEC)
#  define CLOCKS_PER_SEC CLK_TCK
#endif

#if defined(WANT_LZO_MALLOC)
#  define lzo_malloc(a)         (malloc(a))
#  define lzo_free(a)           (free(a))
#endif
#if defined(WANT_LZO_FREAD)
#  define lzo_fread(f,b,s)      (fread(b,1,s,f))
#  define lzo_fwrite(f,b,s)     (fwrite(b,1,s,f))
#endif
#if defined(WANT_LZO_UCLOCK)
#  define lzo_uclock_handle_t   int
#  define lzo_uclock_t          double
#  define lzo_uclock_open(a)    ((void)(a))
#  define lzo_uclock_close(a)   ((void)(a))
#  define lzo_uclock_read(a,b)  *(b) = (clock() / (double)(CLOCKS_PER_SEC))
#  define lzo_uclock_get_elapsed(a,b,c) (*(c) - *(b))
#endif
#if defined(WANT_LZO_WILDARGV)
#  define lzo_wildargv(a,b)     ((void)0)
#endif

#endif


/*************************************************************************
// misc
**************************************************************************/

/* turn on assertions */
#undef NDEBUG
#include <assert.h>

/* just in case */
#undef xmalloc
#undef xfree
#undef xread
#undef xwrite
#undef xputc
#undef xgetc
#undef xread32
#undef xwrite32


#if defined(WANT_XMALLOC)
static lzo_voidp xmalloc(lzo_uint len)
{
    lzo_voidp p;
    lzo_uint align = (lzo_uint) sizeof(lzo_align_t);

    p = (lzo_voidp) lzo_malloc(len > 0 ? len : 1);
    if (p == NULL)
    {
        printf("%s: out of memory\n", progname);
        exit(1);
    }
    if (len >= align && __lzo_align_gap(p, align) != 0)
    {
        printf("%s: C library problem: malloc() returned misaligned pointer!\n", progname);
        exit(1);
    }
    return p;
}
#endif


#if defined(WANT_LZO_UCLOCK)

/* High quality benchmarking.
 *
 * Flush the CPU cache to get more accurate benchmark values.
 * This needs custom kernel patches. As a result - in combination with
 * the perfctr Linux kernel patches - accurate high-quality benchmarking
 * is possible.
 *
 * All other methods (rdtsc, QueryPerformanceCounter, gettimeofday, ...)
 * are completely unreliable for our purposes, and the only other
 * option is to boot into a legacy single-task operating system
 * like plain MSDOS and to directly reprogram the hardware clock.
 * [The djgpp2 port of the gcc compiler has support functions for this.]
 *
 * Also, for embedded systems it's best to benchmark by using a
 * CPU emulator/simulator software that can exactly count all
 * virtual clock ticks.
 */

#if !defined(lzo_uclock_flush_cpu_cache)
#  define lzo_uclock_flush_cpu_cache(h,flags)  ((void)(h))
#endif

#endif


/*
vi:ts=4:et
*/

