/* lzo_asm.h -- assembler prototypes for the LZO data compression library

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


#ifndef __LZO_ASM_H_INCLUDED
#define __LZO_ASM_H_INCLUDED

#ifndef __LZOCONF_H_INCLUDED
#include "lzoconf.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


/***********************************************************************
// assembly decompressors
************************************************************************/

LZO_EXTERN(int) lzo1c_decompress_asm
                                (const lzo_bytep src, lzo_uint  src_len,
                                       lzo_bytep dst, lzo_uintp dst_len,
                                       lzo_voidp wrkmem);
LZO_EXTERN(int) lzo1c_decompress_asm_safe
                                (const lzo_bytep src, lzo_uint  src_len,
                                       lzo_bytep dst, lzo_uintp dst_len,
                                       lzo_voidp wrkmem);

LZO_EXTERN(int) lzo1f_decompress_asm_fast
                                (const lzo_bytep src, lzo_uint  src_len,
                                       lzo_bytep dst, lzo_uintp dst_len,
                                       lzo_voidp wrkmem);
LZO_EXTERN(int) lzo1f_decompress_asm_fast_safe
                                (const lzo_bytep src, lzo_uint  src_len,
                                       lzo_bytep dst, lzo_uintp dst_len,
                                       lzo_voidp wrkmem);

LZO_EXTERN(int) lzo1x_decompress_asm
                                (const lzo_bytep src, lzo_uint  src_len,
                                       lzo_bytep dst, lzo_uintp dst_len,
                                       lzo_voidp wrkmem);
LZO_EXTERN(int) lzo1x_decompress_asm_safe
                                (const lzo_bytep src, lzo_uint  src_len,
                                       lzo_bytep dst, lzo_uintp dst_len,
                                       lzo_voidp wrkmem);
LZO_EXTERN(int) lzo1x_decompress_asm_fast
                                (const lzo_bytep src, lzo_uint  src_len,
                                       lzo_bytep dst, lzo_uintp dst_len,
                                       lzo_voidp wrkmem);
LZO_EXTERN(int) lzo1x_decompress_asm_fast_safe
                                (const lzo_bytep src, lzo_uint  src_len,
                                       lzo_bytep dst, lzo_uintp dst_len,
                                       lzo_voidp wrkmem);

LZO_EXTERN(int) lzo1y_decompress_asm
                                (const lzo_bytep src, lzo_uint  src_len,
                                       lzo_bytep dst, lzo_uintp dst_len,
                                       lzo_voidp wrkmem);
LZO_EXTERN(int) lzo1y_decompress_asm_safe
                                (const lzo_bytep src, lzo_uint  src_len,
                                       lzo_bytep dst, lzo_uintp dst_len,
                                       lzo_voidp wrkmem);
LZO_EXTERN(int) lzo1y_decompress_asm_fast
                                (const lzo_bytep src, lzo_uint  src_len,
                                       lzo_bytep dst, lzo_uintp dst_len,
                                       lzo_voidp wrkmem);
LZO_EXTERN(int) lzo1y_decompress_asm_fast_safe
                                (const lzo_bytep src, lzo_uint  src_len,
                                       lzo_bytep dst, lzo_uintp dst_len,
                                       lzo_voidp wrkmem);


/***********************************************************************
// checksum and misc functions
************************************************************************/

#if 0

LZO_EXTERN(lzo_uint32)
lzo_crc32_asm(lzo_uint32 _c, const lzo_bytep _buf, lzo_uint _len,
              const lzo_uint32p _crc_table);

LZO_EXTERN(lzo_uint32)
lzo_crc32_asm_small(lzo_uint32 _c, const lzo_bytep _buf, lzo_uint _len);

LZO_EXTERN(int)
lzo_cpuid_asm(lzo_uint32p /* lzo_uint32 info[16] */ );

LZO_EXTERN(lzo_uint32)
lzo_rdtsc_asm(lzo_uint32p /* lzo_uint32 ticks[2] */ );

#endif


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* already included */


/* vim:set ts=4 et: */
