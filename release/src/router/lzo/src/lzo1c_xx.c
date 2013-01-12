/* lzo1c_xx.c -- LZO1C compression public entry point

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


#include "config1c.h"


/***********************************************************************
//
************************************************************************/

static const lzo_compress_t * const c_funcs [9] =
{
    &_lzo1c_1_compress_func,
    &_lzo1c_2_compress_func,
    &_lzo1c_3_compress_func,
    &_lzo1c_4_compress_func,
    &_lzo1c_5_compress_func,
    &_lzo1c_6_compress_func,
    &_lzo1c_7_compress_func,
    &_lzo1c_8_compress_func,
    &_lzo1c_9_compress_func
};


lzo_compress_t _lzo1c_get_compress_func(int clevel)
{
    const lzo_compress_t *f;

    if (clevel < LZO1C_BEST_SPEED || clevel > LZO1C_BEST_COMPRESSION)
    {
        if (clevel == LZO1C_DEFAULT_COMPRESSION)
            clevel = LZO1C_BEST_SPEED;
        else
            return 0;
    }
    f = c_funcs[clevel-1];
    assert(f && *f);
    return *f;
}


LZO_PUBLIC(int)
lzo1c_compress ( const lzo_bytep src, lzo_uint  src_len,
                       lzo_bytep dst, lzo_uintp dst_len,
                       lzo_voidp wrkmem,
                       int clevel )
{
    lzo_compress_t f;

    f = _lzo1c_get_compress_func(clevel);
    if (!f)
        return LZO_E_ERROR;
    return _lzo1c_do_compress(src,src_len,dst,dst_len,wrkmem,f);
}



/*
vi:ts=4:et
*/

