/* lzo1c_cc.h -- definitions for the the LZO1C compression driver

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


#ifndef __LZO1C_CC_H
#define __LZO1C_CC_H 1


/***********************************************************************
//
************************************************************************/

extern const lzo_compress_t _lzo1c_1_compress_func;
extern const lzo_compress_t _lzo1c_2_compress_func;
extern const lzo_compress_t _lzo1c_3_compress_func;
extern const lzo_compress_t _lzo1c_4_compress_func;
extern const lzo_compress_t _lzo1c_5_compress_func;
extern const lzo_compress_t _lzo1c_6_compress_func;
extern const lzo_compress_t _lzo1c_7_compress_func;
extern const lzo_compress_t _lzo1c_8_compress_func;
extern const lzo_compress_t _lzo1c_9_compress_func;

extern const lzo_compress_t _lzo1c_99_compress_func;


/***********************************************************************
//
************************************************************************/

LZO_EXTERN(lzo_bytep )
_lzo1c_store_run ( lzo_bytep const oo, const lzo_bytep const ii,
                   lzo_uint r_len);

#define STORE_RUN   _lzo1c_store_run


lzo_compress_t _lzo1c_get_compress_func(int clevel);

int _lzo1c_do_compress   ( const lzo_bytep in,  lzo_uint  in_len,
                                 lzo_bytep out, lzo_uintp out_len,
                                 lzo_voidp wrkmem,
                                 lzo_compress_t func );


#endif /* already included */

/*
vi:ts=4:et
*/


