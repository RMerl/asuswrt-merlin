/* lzo_func.ch -- functions

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


/* WARNING: this file should *not* be used by applications. It is
   part of the implementation of the library and is subject
   to change.
 */


/***********************************************************************
// bitops
************************************************************************/

#if (defined(_WIN32) || defined(_WIN64)) && ((LZO_CC_INTELC && (__INTEL_COMPILER >= 1000)) || (LZO_CC_MSC && (_MSC_VER >= 1400)))
#include <intrin.h>
#if !defined(lzo_bitops_clz32) && defined(WANT_lzo_bitops_clz32) && 0
#pragma intrinsic(_BitScanReverse)
static __lzo_inline unsigned lzo_bitops_clz32(lzo_uint32 v)
{
    unsigned long r;
    (void) _BitScanReverse(&r, v);
    return (unsigned) r;
}
#define lzo_bitops_clz32 lzo_bitops_clz32
#endif
#if !defined(lzo_bitops_clz64) && defined(WANT_lzo_bitops_clz64) && defined(LZO_UINT64_MAX) && 0
#pragma intrinsic(_BitScanReverse64)
static __lzo_inline unsigned lzo_bitops_clz64(lzo_uint64 v)
{
    unsigned long r;
    (void) _BitScanReverse64(&r, v);
    return (unsigned) r;
}
#define lzo_bitops_clz64 lzo_bitops_clz64
#endif
#if !defined(lzo_bitops_ctz32) && defined(WANT_lzo_bitops_ctz32)
#pragma intrinsic(_BitScanForward)
static __lzo_inline unsigned lzo_bitops_ctz32(lzo_uint32 v)
{
    unsigned long r;
    (void) _BitScanForward(&r, v);
    return (unsigned) r;
}
#define lzo_bitops_ctz32 lzo_bitops_ctz32
#endif
#if !defined(lzo_bitops_ctz64) && defined(WANT_lzo_bitops_ctz64) && defined(LZO_UINT64_MAX)
#pragma intrinsic(_BitScanForward64)
static __lzo_inline unsigned lzo_bitops_ctz64(lzo_uint64 v)
{
    unsigned long r;
    (void) _BitScanForward64(&r, v);
    return (unsigned) r;
}
#define lzo_bitops_ctz64 lzo_bitops_ctz64
#endif

#elif (LZO_CC_CLANG || (LZO_CC_GNUC >= 0x030400ul) || (LZO_CC_INTELC && (__INTEL_COMPILER >= 1000)) || (LZO_CC_LLVM && (!defined(__llvm_tools_version__) || (__llvm_tools_version__+0 >= 0x010500ul))))
#if !defined(lzo_bitops_clz32) && defined(WANT_lzo_bitops_clz32)
#define lzo_bitops_clz32(v) ((unsigned) __builtin_clz(v))
#endif
#if !defined(lzo_bitops_clz64) && defined(WANT_lzo_bitops_clz64) && defined(LZO_UINT64_MAX)
#define lzo_bitops_clz64(v) ((unsigned) __builtin_clzll(v))
#endif
#if !defined(lzo_bitops_ctz32) && defined(WANT_lzo_bitops_ctz32)
#define lzo_bitops_ctz32(v) ((unsigned) __builtin_ctz(v))
#endif
#if !defined(lzo_bitops_ctz64) && defined(WANT_lzo_bitops_ctz64) && defined(LZO_UINT64_MAX)
#define lzo_bitops_ctz64(v) ((unsigned) __builtin_ctzll(v))
#endif
#if !defined(lzo_bitops_popcount32) && defined(WANT_lzo_bitops_popcount32)
#define lzo_bitops_popcount32(v) ((unsigned) __builtin_popcount(v))
#endif
#if !defined(lzo_bitops_popcount32) && defined(WANT_lzo_bitops_popcount64) && defined(LZO_UINT64_MAX)
#define lzo_bitops_popcount64(v) ((unsigned) __builtin_popcountll(v))
#endif
#endif


/*
vi:ts=4:et
*/

