/* lzo_str.c -- string functions for the the LZO library

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

#undef lzo_memcmp
#undef lzo_memcpy
#undef lzo_memmove
#undef lzo_memset


/***********************************************************************
// slow but portable <string.h> stuff, only used in assertions
************************************************************************/

#if !defined(__LZO_MMODEL_HUGE)
#  undef ACC_HAVE_MM_HUGE_PTR
#endif
#define acc_hsize_t             lzo_uint
#define acc_hvoid_p             lzo_voidp
#define acc_hbyte_p             lzo_bytep
#define ACCLIB_PUBLIC(r,f)      LZO_PUBLIC(r) f
#define acc_hmemcmp             lzo_memcmp
#define acc_hmemcpy             lzo_memcpy
#define acc_hmemmove            lzo_memmove
#define acc_hmemset             lzo_memset
#define ACC_WANT_ACCLIB_HMEMCPY 1
#include "miniacc.h"
#undef ACCLIB_PUBLIC


/*
vi:ts=4:et
*/
