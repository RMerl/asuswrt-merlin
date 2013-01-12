/* lzo_init.c -- initialization of the LZO library

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


#include "lzo_conf.h"


/***********************************************************************
// Runtime check of the assumptions about the size of builtin types,
// memory model, byte order and other low-level constructs.
//
// We are really paranoid here - LZO should either fail
// at startup or not at all.
//
// Because of inlining much of these functions evaluates to nothing.
//
// And while many of the tests seem highly obvious and redundant they are
// here to catch compiler/optimizer bugs. Yes, these do exist.
************************************************************************/

#if !defined(__LZO_IN_MINILZO)

#define ACC_WANT_ACC_CHK_CH 1
#undef ACCCHK_ASSERT
#include "miniacc.h"

    ACCCHK_ASSERT_IS_SIGNED_T(lzo_int)
    ACCCHK_ASSERT_IS_UNSIGNED_T(lzo_uint)

    ACCCHK_ASSERT_IS_SIGNED_T(lzo_int32)
    ACCCHK_ASSERT_IS_UNSIGNED_T(lzo_uint32)
    ACCCHK_ASSERT((LZO_UINT32_C(1) << (int)(8*sizeof(LZO_UINT32_C(1))-1)) > 0)
    ACCCHK_ASSERT(sizeof(lzo_uint32) >= 4)
#if defined(LZO_UINT64_MAX)
    ACCCHK_ASSERT(sizeof(lzo_uint64) == 8)
    ACCCHK_ASSERT_IS_SIGNED_T(lzo_int64)
    ACCCHK_ASSERT_IS_UNSIGNED_T(lzo_uint64)
#endif

#if !defined(__LZO_UINTPTR_T_IS_POINTER)
    ACCCHK_ASSERT_IS_UNSIGNED_T(lzo_uintptr_t)
#endif
    ACCCHK_ASSERT(sizeof(lzo_uintptr_t) >= sizeof(lzo_voidp))

    ACCCHK_ASSERT_IS_UNSIGNED_T(lzo_xint)
    ACCCHK_ASSERT(sizeof(lzo_xint) >= sizeof(lzo_uint32))
    ACCCHK_ASSERT(sizeof(lzo_xint) >= sizeof(lzo_uint))
    ACCCHK_ASSERT(sizeof(lzo_xint) == sizeof(lzo_uint32) || sizeof(lzo_xint) == sizeof(lzo_uint))

#endif
#undef ACCCHK_ASSERT


/***********************************************************************
//
************************************************************************/

#define WANT_lzo_bitops_clz32 1
#define WANT_lzo_bitops_clz64 1
#define WANT_lzo_bitops_ctz32 1
#define WANT_lzo_bitops_ctz64 1
#include "lzo_func.ch"

#if 0
#define u2p(ptr,off) ((lzo_voidp) (((lzo_bytep)(lzo_voidp)(ptr)) + (off)))
#else
static __lzo_noinline lzo_voidp u2p(lzo_voidp ptr, lzo_uint off)
{
    return (lzo_voidp) ((lzo_bytep) ptr + off);
}
#endif

LZO_PUBLIC(int)
_lzo_config_check(void)
{
    lzo_bool r = 1;
    union {
        lzo_xint a[2]; unsigned char b[2*LZO_MAX(8,sizeof(lzo_xint))];
#if defined(LZO_UNALIGNED_OK_8)
        lzo_uint64 c[2];
#endif
        unsigned short x[2]; lzo_uint32 y[2]; lzo_uint z[2];
    } u;
    lzo_voidp p;

    u.a[0] = u.a[1] = 0;
    p = u2p(&u, 0);
    r &= ((* (lzo_bytep) p) == 0);
#if !defined(LZO_CFG_NO_CONFIG_CHECK)
#if defined(LZO_ABI_BIG_ENDIAN)
    u.a[0] = u.a[1] = 0; u.b[sizeof(lzo_uint) - 1] = 128;
    p = u2p(&u, 0);
    r &= ((* (lzo_uintp) p) == 128);
#endif
#if defined(LZO_ABI_LITTLE_ENDIAN)
    u.a[0] = u.a[1] = 0; u.b[0] = 128;
    p = u2p(&u, 0);
    r &= ((* (lzo_uintp) p) == 128);
#endif
#if defined(LZO_UNALIGNED_OK_2)
    u.a[0] = u.a[1] = 0;
    u.b[0] = 1; u.b[sizeof(unsigned short) + 1] = 2;
    p = u2p(&u, 1);
    r &= ((* (lzo_ushortp) p) == 0);
#endif
#if defined(LZO_UNALIGNED_OK_4)
    u.a[0] = u.a[1] = 0;
    u.b[0] = 3; u.b[sizeof(lzo_uint32) + 1] = 4;
    p = u2p(&u, 1);
    r &= ((* (lzo_uint32p) p) == 0);
#endif
#if defined(LZO_UNALIGNED_OK_8)
    u.c[0] = u.c[1] = 0;
    u.b[0] = 5; u.b[sizeof(lzo_uint64) + 1] = 6;
    p = u2p(&u, 1);
    r &= ((* (lzo_uint64p) p) == 0);
#endif
#if defined(lzo_bitops_clz32)
    { unsigned i; lzo_uint32 v = 1;
    for (i = 0; i < 32; i++, v <<= 1)
        r &= lzo_bitops_clz32(v) == 31 - i;
    }
#endif
#if defined(lzo_bitops_clz64)
    { unsigned i; lzo_uint64 v = 1;
    for (i = 0; i < 64; i++, v <<= 1)
        r &= lzo_bitops_clz64(v) == 63 - i;
    }
#endif
#if defined(lzo_bitops_ctz32)
    { unsigned i; lzo_uint32 v = 1;
    for (i = 0; i < 32; i++, v <<= 1)
        r &= lzo_bitops_ctz32(v) == i;
    }
#endif
#if defined(lzo_bitops_ctz64)
    { unsigned i; lzo_uint64 v = 1;
    for (i = 0; i < 64; i++, v <<= 1)
        r &= lzo_bitops_ctz64(v) == i;
    }
#endif
#endif

    return r == 1 ? LZO_E_OK : LZO_E_ERROR;
}


/***********************************************************************
//
************************************************************************/

LZO_PUBLIC(int)
__lzo_init_v2(unsigned v, int s1, int s2, int s3, int s4, int s5,
                          int s6, int s7, int s8, int s9)
{
    int r;

#if defined(__LZO_IN_MINILZO)
#elif (LZO_CC_MSC && ((_MSC_VER) < 700))
#else
#define ACC_WANT_ACC_CHK_CH 1
#undef ACCCHK_ASSERT
#define ACCCHK_ASSERT(expr)  LZO_COMPILE_TIME_ASSERT(expr)
#include "miniacc.h"
#endif
#undef ACCCHK_ASSERT

    if (v == 0)
        return LZO_E_ERROR;

    r = (s1 == -1 || s1 == (int) sizeof(short)) &&
        (s2 == -1 || s2 == (int) sizeof(int)) &&
        (s3 == -1 || s3 == (int) sizeof(long)) &&
        (s4 == -1 || s4 == (int) sizeof(lzo_uint32)) &&
        (s5 == -1 || s5 == (int) sizeof(lzo_uint)) &&
        (s6 == -1 || s6 == (int) lzo_sizeof_dict_t) &&
        (s7 == -1 || s7 == (int) sizeof(char *)) &&
        (s8 == -1 || s8 == (int) sizeof(lzo_voidp)) &&
        (s9 == -1 || s9 == (int) sizeof(lzo_callback_t));
    if (!r)
        return LZO_E_ERROR;

    r = _lzo_config_check();
    if (r != LZO_E_OK)
        return r;

    return r;
}


#if !defined(__LZO_IN_MINILZO)
#include "lzo_dll.ch"
#endif


/*
vi:ts=4:et
*/
