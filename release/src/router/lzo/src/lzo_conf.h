/* lzo_conf.h -- main internal configuration file for the the LZO library

   This file is part of the LZO real-time data compression library.

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


#ifndef __LZO_CONF_H
#define __LZO_CONF_H

#if !defined(__LZO_IN_MINILZO)
#if defined(LZO_CFG_FREESTANDING)
#  define LZO_LIBC_FREESTANDING 1
#  define LZO_OS_FREESTANDING 1
#  define ACC_LIBC_FREESTANDING 1
#  define ACC_OS_FREESTANDING 1
#endif
#if defined(LZO_CFG_NO_UNALIGNED)
#  define ACC_CFG_NO_UNALIGNED 1
#endif
#if defined(LZO_ARCH_GENERIC)
#  define ACC_ARCH_GENERIC 1
#endif
#if defined(LZO_ABI_NEUTRAL_ENDIAN)
#  define ACC_ABI_NEUTRAL_ENDIAN 1
#endif
#if defined(LZO_HAVE_CONFIG_H)
#  define ACC_CONFIG_NO_HEADER 1
#endif
#if defined(LZO_CFG_EXTRA_CONFIG_HEADER)
#  include LZO_CFG_EXTRA_CONFIG_HEADER
#endif
#if defined(__LZOCONF_H) || defined(__LZOCONF_H_INCLUDED)
#  error "include this file first"
#endif
#include "lzo/lzoconf.h"
#endif

#if (LZO_VERSION < 0x02000) || !defined(__LZOCONF_H_INCLUDED)
#  error "version mismatch"
#endif


/***********************************************************************
// pragmas
************************************************************************/

#if (LZO_CC_BORLANDC && LZO_ARCH_I086)
#  pragma option -h         /* enable fast huge pointers */
#endif

#if (LZO_CC_MSC && (_MSC_VER >= 1000))
#  pragma warning(disable: 4127 4701)
#endif
#if (LZO_CC_MSC && (_MSC_VER >= 1300))
   /* avoid `-Wall' warnings in system header files */
#  pragma warning(disable: 4820)
   /* avoid warnings about inlining */
#  pragma warning(disable: 4514 4710 4711)
#endif

#if (LZO_CC_SUNPROC)
#  pragma error_messages(off,E_END_OF_LOOP_CODE_NOT_REACHED)
#  pragma error_messages(off,E_LOOP_NOT_ENTERED_AT_TOP)
#endif


/***********************************************************************
//
************************************************************************/

#if defined(__LZO_MMODEL_HUGE) && (!LZO_HAVE_MM_HUGE_PTR)
#  error "this should not happen - check defines for __huge"
#endif

#if defined(__LZO_IN_MINILZO) || defined(LZO_CFG_FREESTANDING)
#elif (LZO_OS_DOS16 || LZO_OS_OS216 || LZO_OS_WIN16)
#  define ACC_WANT_ACC_INCD_H 1
#  define ACC_WANT_ACC_INCE_H 1
#  define ACC_WANT_ACC_INCI_H 1
#elif 1
#  include <string.h>
#else
#  define ACC_WANT_ACC_INCD_H 1
#endif
#include "miniacc.h"


#if defined(LZO_CFG_FREESTANDING)
#  undef HAVE_MEMCMP
#  undef HAVE_MEMCPY
#  undef HAVE_MEMMOVE
#  undef HAVE_MEMSET
#endif

#if !defined(HAVE_MEMCMP)
#  undef memcmp
#  define memcmp(a,b,c)         lzo_memcmp(a,b,c)
#elif !defined(__LZO_MMODEL_HUGE)
#  define lzo_memcmp(a,b,c)     memcmp(a,b,c)
#endif
#if !defined(HAVE_MEMCPY)
#  undef memcpy
#  define memcpy(a,b,c)         lzo_memcpy(a,b,c)
#elif !defined(__LZO_MMODEL_HUGE)
#  define lzo_memcpy(a,b,c)     memcpy(a,b,c)
#endif
#if !defined(HAVE_MEMMOVE)
#  undef memmove
#  define memmove(a,b,c)        lzo_memmove(a,b,c)
#elif !defined(__LZO_MMODEL_HUGE)
#  define lzo_memmove(a,b,c)    memmove(a,b,c)
#endif
#if !defined(HAVE_MEMSET)
#  undef memset
#  define memset(a,b,c)         lzo_memset(a,b,c)
#elif !defined(__LZO_MMODEL_HUGE)
#  define lzo_memset(a,b,c)     memset(a,b,c)
#endif


#undef NDEBUG
#if defined(LZO_CFG_FREESTANDING)
#  undef LZO_DEBUG
#  define NDEBUG 1
#  undef assert
#  define assert(e) ((void)0)
#else
#  if !defined(LZO_DEBUG)
#    define NDEBUG 1
#  endif
#  include <assert.h>
#endif

#if 0 && defined(__BOUNDS_CHECKING_ON)
#  include <unchecked.h>
#else
#  define BOUNDS_CHECKING_OFF_DURING(stmt)      stmt
#  define BOUNDS_CHECKING_OFF_IN_EXPR(expr)     (expr)
#endif

#if !defined(__lzo_inline)
#  define __lzo_inline
#endif
#if !defined(__lzo_forceinline)
#  define __lzo_forceinline
#endif
#if !defined(__lzo_noinline)
#  define __lzo_noinline
#endif


/***********************************************************************
//
************************************************************************/

#if 1
#  define LZO_BYTE(x)       ((unsigned char) (x))
#else
#  define LZO_BYTE(x)       ((unsigned char) ((x) & 0xff))
#endif

#define LZO_MAX(a,b)        ((a) >= (b) ? (a) : (b))
#define LZO_MIN(a,b)        ((a) <= (b) ? (a) : (b))
#define LZO_MAX3(a,b,c)     ((a) >= (b) ? LZO_MAX(a,c) : LZO_MAX(b,c))
#define LZO_MIN3(a,b,c)     ((a) <= (b) ? LZO_MIN(a,c) : LZO_MIN(b,c))

#define lzo_sizeof(type)    ((lzo_uint) (sizeof(type)))

#define LZO_HIGH(array)     ((lzo_uint) (sizeof(array)/sizeof(*(array))))

/* this always fits into 16 bits */
#define LZO_SIZE(bits)      (1u << (bits))
#define LZO_MASK(bits)      (LZO_SIZE(bits) - 1)

#define LZO_LSIZE(bits)     (1ul << (bits))
#define LZO_LMASK(bits)     (LZO_LSIZE(bits) - 1)

#define LZO_USIZE(bits)     ((lzo_uint) 1 << (bits))
#define LZO_UMASK(bits)     (LZO_USIZE(bits) - 1)

#if !defined(DMUL)
#if 0
   /* 32*32 multiplies may be faster than 64*64 on some 64-bit machines,
    * but then we need extra casts from unsigned<->size_t */
#  define DMUL(a,b) ((lzo_xint) ((lzo_uint32)(a) * (lzo_uint32)(b)))
#else
#  define DMUL(a,b) ((lzo_xint) ((a) * (b)))
#endif
#endif


/***********************************************************************
// compiler and architecture specific stuff
************************************************************************/

/* Some defines that indicate if memory can be accessed at unaligned
 * memory addresses. You should also test that this is actually faster
 * even if it is allowed by your system.
 */

#if 1 && !defined(LZO_CFG_NO_UNALIGNED)
#if 1 && (LZO_ARCH_AMD64 || LZO_ARCH_I386)
#  if (LZO_SIZEOF_SHORT == 2)
#    define LZO_UNALIGNED_OK_2
#  endif
#  if (LZO_SIZEOF_INT == 4)
#    define LZO_UNALIGNED_OK_4
#  endif
#endif
#endif

#if defined(LZO_UNALIGNED_OK_2)
  LZO_COMPILE_TIME_ASSERT_HEADER(sizeof(short) == 2)
#endif
#if defined(LZO_UNALIGNED_OK_4)
  LZO_COMPILE_TIME_ASSERT_HEADER(sizeof(lzo_uint32) == 4)
#elif defined(LZO_ALIGNED_OK_4)
  LZO_COMPILE_TIME_ASSERT_HEADER(sizeof(lzo_uint32) == 4)
#endif


/* Fast memcpy that copies multiples of 8 byte chunks.
 * len is the number of bytes.
 * note: all parameters must be lvalues, len >= 8
 *       dest and src advance, len is undefined afterwards
 */

#define MEMCPY8_DS(dest,src,len) \
    lzo_memcpy(dest,src,len); dest += len; src += len

#define BZERO8_PTR(s,l,n) \
    lzo_memset((lzo_voidp)(s),0,(lzo_uint)(l)*(n))

#define MEMCPY_DS(dest,src,len) \
    do *dest++ = *src++; while (--len > 0)


/***********************************************************************
// some globals
************************************************************************/

__LZO_EXTERN_C int __lzo_init_done;
__LZO_EXTERN_C const char __lzo_copyright[];
LZO_EXTERN(const lzo_bytep) lzo_copyright(void);


/***********************************************************************
//
************************************************************************/

#include "lzo_ptr.h"


/* Generate compressed data in a deterministic way.
 * This is fully portable, and compression can be faster as well.
 * A reason NOT to be deterministic is when the block size is
 * very small (e.g. 8kB) or the dictionary is big, because
 * then the initialization of the dictionary becomes a relevant
 * magnitude for compression speed.
 */
#define LZO_DETERMINISTIC


#define LZO_DICT_USE_PTR
#if 0 && (LZO_ARCH_I086)
#  undef LZO_DICT_USE_PTR
#endif

#if defined(LZO_DICT_USE_PTR)
#  define lzo_dict_t    const lzo_bytep
#  define lzo_dict_p    lzo_dict_t __LZO_MMODEL *
#else
#  define lzo_dict_t    lzo_uint
#  define lzo_dict_p    lzo_dict_t __LZO_MMODEL *
#endif


#endif /* already included */

/*
vi:ts=4:et
*/

