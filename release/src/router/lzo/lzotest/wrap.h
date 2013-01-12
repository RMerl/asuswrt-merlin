/* wrap.h -- wrapper functions

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
// compression levels of LZO1X-999 and LZO1Y-999
**************************************************************************/

#if defined(HAVE_LZO1X_H)

LZO_PRIVATE(int)
lzo1x_999_1_compress    ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    return lzo1x_999_compress_level(src, src_len, dst, dst_len, wrkmem,
                                    dict.ptr, dict.len, 0, 1);
}

LZO_PRIVATE(int)
lzo1x_999_2_compress    ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    return lzo1x_999_compress_level(src, src_len, dst, dst_len, wrkmem,
                                    dict.ptr, dict.len, 0, 2);
}

LZO_PRIVATE(int)
lzo1x_999_3_compress    ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    return lzo1x_999_compress_level(src, src_len, dst, dst_len, wrkmem,
                                    dict.ptr, dict.len, 0, 3);
}

LZO_PRIVATE(int)
lzo1x_999_4_compress    ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    return lzo1x_999_compress_level(src, src_len, dst, dst_len, wrkmem,
                                    dict.ptr, dict.len, 0, 4);
}

LZO_PRIVATE(int)
lzo1x_999_5_compress    ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    return lzo1x_999_compress_level(src, src_len, dst, dst_len, wrkmem,
                                    dict.ptr, dict.len, 0, 5);
}

LZO_PRIVATE(int)
lzo1x_999_6_compress    ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    return lzo1x_999_compress_level(src, src_len, dst, dst_len, wrkmem,
                                    dict.ptr, dict.len, 0, 6);
}

LZO_PRIVATE(int)
lzo1x_999_7_compress    ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    return lzo1x_999_compress_level(src, src_len, dst, dst_len, wrkmem,
                                    dict.ptr, dict.len, 0, 7);
}

LZO_PRIVATE(int)
lzo1x_999_8_compress    ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    return lzo1x_999_compress_level(src, src_len, dst, dst_len, wrkmem,
                                    dict.ptr, dict.len, 0, 8);
}

LZO_PRIVATE(int)
lzo1x_999_9_compress    ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    return lzo1x_999_compress_level(src, src_len, dst, dst_len, wrkmem,
                                    dict.ptr, dict.len, 0, 9);
}

#endif


#if defined(HAVE_LZO1Y_H)

LZO_PRIVATE(int)
lzo1y_999_1_compress    ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    return lzo1y_999_compress_level(src, src_len, dst, dst_len, wrkmem,
                                    dict.ptr, dict.len, 0, 1);
}

LZO_PRIVATE(int)
lzo1y_999_2_compress    ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    return lzo1y_999_compress_level(src, src_len, dst, dst_len, wrkmem,
                                    dict.ptr, dict.len, 0, 2);
}

LZO_PRIVATE(int)
lzo1y_999_3_compress    ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    return lzo1y_999_compress_level(src, src_len, dst, dst_len, wrkmem,
                                    dict.ptr, dict.len, 0, 3);
}

LZO_PRIVATE(int)
lzo1y_999_4_compress    ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    return lzo1y_999_compress_level(src, src_len, dst, dst_len, wrkmem,
                                    dict.ptr, dict.len, 0, 4);
}

LZO_PRIVATE(int)
lzo1y_999_5_compress    ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    return lzo1y_999_compress_level(src, src_len, dst, dst_len, wrkmem,
                                    dict.ptr, dict.len, 0, 5);
}

LZO_PRIVATE(int)
lzo1y_999_6_compress    ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    return lzo1y_999_compress_level(src, src_len, dst, dst_len, wrkmem,
                                    dict.ptr, dict.len, 0, 6);
}

LZO_PRIVATE(int)
lzo1y_999_7_compress    ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    return lzo1y_999_compress_level(src, src_len, dst, dst_len, wrkmem,
                                    dict.ptr, dict.len, 0, 7);
}

LZO_PRIVATE(int)
lzo1y_999_8_compress    ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    return lzo1y_999_compress_level(src, src_len, dst, dst_len, wrkmem,
                                    dict.ptr, dict.len, 0, 8);
}

LZO_PRIVATE(int)
lzo1y_999_9_compress    ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    return lzo1y_999_compress_level(src, src_len, dst, dst_len, wrkmem,
                                    dict.ptr, dict.len, 0, 9);
}

#endif


/*************************************************************************
// other wrappers (pseudo compressors)
**************************************************************************/

LZO_PRIVATE(int)
memcpy_x_compress       ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    lzo_memcpy(dst,src,src_len);
    *dst_len = src_len;
    if (wrkmem) wrkmem = 0; /* avoid warning */
    return 0;
}


LZO_PRIVATE(int)
memset_x_compress       ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    lzo_memset(dst,0,src_len);
    *dst_len = src_len;
    if (src) src = 0;       /* avoid warning */
    if (wrkmem) wrkmem = 0; /* avoid warning */
    return 0;
}


LZO_PRIVATE(int)
adler32_x_compress      ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    lzo_uint32 adler;
    adler = lzo_adler32(0, NULL, 0);
    adler = lzo_adler32(adler, dst, src_len);
    *dst_len = src_len;
    if (src) src = 0;       /* avoid warning */
    if (wrkmem) wrkmem = 0; /* avoid warning */
    return 0;
}


LZO_PRIVATE(int)
crc32_x_compress        ( const lzo_bytep src, lzo_uint  src_len,
                                lzo_bytep dst, lzo_uintp dst_len,
                                lzo_voidp wrkmem )
{
    lzo_uint32 crc;
    crc = lzo_crc32(0, NULL, 0);
    crc = lzo_crc32(crc, dst, src_len);
    *dst_len = src_len;
    if (src) src = 0;       /* avoid warning */
    if (wrkmem) wrkmem = 0; /* avoid warning */
    return 0;
}


#if defined(__LZO_PROFESSIONAL__)
#  include "lzopro/t_wrap.ch"
#endif


/*
vi:ts=4:et
*/

