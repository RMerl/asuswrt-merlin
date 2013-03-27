/* This file is part of psim (model of the PowerPC(tm) architecture)

   Copyright (C) 1994-1995, Andrew Cagney <cagney@highland.com.au>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.
 
   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
 
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
   --

   PowerPC is a trademark of International Business Machines Corporation. */


/* Basic type sizes for the PowerPC */

#ifndef _WORDS_H_
#define _WORDS_H_

/* TYPES:

     natural*	sign determined by host
     signed*    signed type of the given size
     unsigned*  The corresponding insigned type

   SIZES

     *NN	Size based on the number of bits
     *_NN       Size according to the number of bytes
     *_word     Size based on the target architecture's word
     		word size (32/64 bits)
     *_cell     Size based on the target architecture's
     		IEEE 1275 cell size (almost always 32 bits)

*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* bit based */
typedef char natural8;
typedef short natural16;
typedef int natural32;

typedef signed char signed8;
typedef signed short signed16;
typedef signed int signed32;

typedef unsigned char unsigned8;
typedef unsigned short unsigned16;
typedef unsigned int unsigned32;

#ifdef __GNUC__
typedef long long natural64;
typedef signed long long signed64;
typedef unsigned long long unsigned64;
#endif

#ifdef _MSC_VER
typedef __int64 natural64;
typedef signed __int64 signed64;
typedef unsigned __int64 unsigned64;
#endif 


/* byte based */
typedef natural8 natural_1;
typedef natural16 natural_2;
typedef natural32 natural_4;
typedef natural64 natural_8;

typedef signed8 signed_1;
typedef signed16 signed_2;
typedef signed32 signed_4;
typedef signed64 signed_8;

typedef unsigned8 unsigned_1;
typedef unsigned16 unsigned_2;
typedef unsigned32 unsigned_4;
typedef unsigned64 unsigned_8;


/* for general work, the following are defined */
/* unsigned: >= 32 bits */
/* signed:   >= 32 bits */
/* long:     >= 32 bits, sign undefined */
/* int:      small indicator */

/* target architecture based */
#if (WITH_TARGET_WORD_BITSIZE == 64)
typedef natural64 natural_word;
typedef unsigned64 unsigned_word;
typedef signed64 signed_word;
#else
typedef natural32 natural_word;
typedef unsigned32 unsigned_word;
typedef signed32 signed_word;
#endif


/* Other instructions */
typedef unsigned32 instruction_word;

/* IEEE 1275 cell size - only support 32bit mode at present */
typedef natural32 natural_cell;
typedef unsigned32 unsigned_cell;
typedef signed32 signed_cell;

#endif /* _WORDS_H_ */
