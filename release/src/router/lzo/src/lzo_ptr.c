/* lzo_ptr.c -- low-level pointer constructs

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


#include "lzo_conf.h"


/***********************************************************************
//
************************************************************************/

LZO_PUBLIC(lzo_uintptr_t)
__lzo_ptr_linear(const lzo_voidp ptr)
{
    lzo_uintptr_t p;

#if (LZO_ARCH_I086)
    p = (((lzo_uintptr_t)(ACC_PTR_FP_SEG(ptr))) << (16 - ACC_MM_AHSHIFT)) + (ACC_PTR_FP_OFF(ptr));
#elif (LZO_MM_PVP)
    p = (lzo_uintptr_t) (ptr);
    p = (p << 3) | (p >> 61);
#else
    p = (lzo_uintptr_t) PTR_LINEAR(ptr);
#endif

    return p;
}


/***********************************************************************
//
************************************************************************/

LZO_PUBLIC(unsigned)
__lzo_align_gap(const lzo_voidp ptr, lzo_uint size)
{
#if defined(__LZO_UINTPTR_T_IS_POINTER)
    size_t n = (size_t) ptr;
    n = (((n + size - 1) / size) * size) - n;
#else
    lzo_uintptr_t p, n;
    p = __lzo_ptr_linear(ptr);
    n = (((p + size - 1) / size) * size) - p;
#endif

    assert(size > 0);
    assert((long)n >= 0);
    assert(n <= size);
    return (unsigned)n;
}



/*
vi:ts=4:et
*/
