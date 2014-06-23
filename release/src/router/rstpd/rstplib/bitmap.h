/************************************************************************ 
 * RSTP library - Rapid Spanning Tree (802.1t, 802.1w) 
 * Copyright (C) 2001-2003 Optical Access 
 * Author: Alex Rozin 
 * 
 * This file is part of RSTP library. 
 * 
 * RSTP library is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU Lesser General Public License as published by the 
 * Free Software Foundation; version 2.1 
 * 
 * RSTP library is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser 
 * General Public License for more details. 
 * 
 * You should have received a copy of the GNU Lesser General Public License 
 * along with RSTP library; see the file COPYING.  If not, write to the Free 
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
 * 02111-1307, USA. 
 **********************************************************************/

#ifndef __BITMAP_H
#define __BITMAP_H

#define NUMBER_OF_PORTS 4

typedef struct tagBITMAP
{
  unsigned long part0;     /* Least Significant part */
} BITMAP_T;

#define BitmapClear(BitmapPtr) \
        { (BitmapPtr)->part0 = 0; }

#define BitmapSetAllBits(BitmapPtr) \
        { (BitmapPtr)->part0 = 0xF; }

#define BitmapClearBits(BitmapPtr,BitmapBitsPtr) \
        { (BitmapPtr)->part0 &= ~((BitmapBitsPtr)->part0); }

#define BitmapSetBits(BitmapPtr,BitmapBitsPtr) \
        { (BitmapPtr)->part0 |= ((BitmapBitsPtr)->part0); }

#define BitmapOr(ResultPtr,BitmapPtr1,BitmapPtr2) \
        { (ResultPtr)->part0 = (BitmapPtr1)->part0 | (BitmapPtr2)->part0; }

#define BitmapAnd(ResultPtr,BitmapPtr1,BitmapPtr2) \
        { (ResultPtr)->part0 = (BitmapPtr1)->part0 & (BitmapPtr2)->part0; }

#define BitmapNot(ResultPtr,BitmapPtr) \
        { (ResultPtr)->part0 = ~((BitmapPtr)->part0); }


/* Return zero if identical */
#define BitmapCmp(BitmapPtr1,BitmapPtr2) \
        ((BitmapPtr1)->part0 != (BitmapPtr2)->part0) 

#define BitmapIsZero(BitmapPtr) \
        (!(BitmapPtr)->part0)

#define BitmapIsAllOnes(BitmapPtr) \
        ((BitmapPtr)->part0 == 0xF)

#define BitmapGetBit(BitmapPtr,Bit) \
         ((BitmapPtr)->part0 & (1 << (Bit)))

/* Bit range [0 .. 63] */
#define BitmapSetBit(BitmapPtr,Bit) \
    {(BitmapPtr)->part0 |= (1 << (Bit)); }

#define BitmapClearBit(BitmapPtr,Bit) \
    (BitmapPtr)->part0 &= ~(1 << (Bit));

#define BitmapCopy(BitmapDstPtr,BitmapSrcPtr) \
        { (BitmapDstPtr)->part0 = (BitmapSrcPtr)->part0; }

#define BitmapXor(ResultPtr,BitmapPtr1,BitmapPtr2) \
        { (ResultPtr)->part0 = (BitmapPtr1)->part0 ^ (BitmapPtr2)->part0; }

#endif /* __BITMAP_H */

