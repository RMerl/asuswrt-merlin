/* asm.h -- library assembler function prototypes

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


/*************************************************************************
//
**************************************************************************/

#if defined(LZO_USE_ASM)
#  include "lzo/lzo_asm.h"
#else
#  define lzo1c_decompress_asm              0
#  define lzo1c_decompress_asm_safe         0
#  define lzo1f_decompress_asm_fast         0
#  define lzo1f_decompress_asm_fast_safe    0
#  define lzo1x_decompress_asm              0
#  define lzo1x_decompress_asm_safe         0
#  define lzo1x_decompress_asm_fast         0
#  define lzo1x_decompress_asm_fast_safe    0
#  define lzo1y_decompress_asm              0
#  define lzo1y_decompress_asm_safe         0
#  define lzo1y_decompress_asm_fast         0
#  define lzo1y_decompress_asm_fast_safe    0
#endif


/*************************************************************************
// these are not yet implemented
**************************************************************************/

#define lzo1b_decompress_asm                0
#define lzo1b_decompress_asm_safe           0
#define lzo1b_decompress_asm_fast           0
#define lzo1b_decompress_asm_fast_safe      0

#define lzo1c_decompress_asm_fast           0
#define lzo1c_decompress_asm_fast_safe      0

#define lzo1f_decompress_asm                0
#define lzo1f_decompress_asm_safe           0


/*
vi:ts=4:et
*/

