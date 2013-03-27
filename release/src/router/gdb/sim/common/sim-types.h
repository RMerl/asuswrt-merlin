/* The common simulator framework for GDB, the GNU Debugger.

   Copyright 2002, 2005, 2007 Free Software Foundation, Inc.

   Contributed by Andrew Cagney and Red Hat.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


#ifndef SIM_TYPES_H
/* #define SIM_TYPES_H */

/* INTEGER QUANTITIES:

   TYPES:

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


#if !defined (SIM_TYPES_H) && defined (__GNUC__)
#define SIM_TYPES_H

/* bit based */

#define UNSIGNED32(X) ((unsigned32) X##UL)
#define UNSIGNED64(X) ((unsigned64) X##ULL)

#define SIGNED32(X) ((signed32) X##L)
#define SIGNED64(X) ((signed64) X##LL)

typedef signed int signed8 __attribute__ ((__mode__ (__QI__)));
typedef signed int signed16 __attribute__ ((__mode__ (__HI__)));
typedef signed int signed32 __attribute__ ((__mode__ (__SI__)));
typedef signed int signed64 __attribute__ ((__mode__ (__DI__)));

typedef unsigned int unsigned8 __attribute__ ((__mode__ (__QI__)));
typedef unsigned int unsigned16 __attribute__ ((__mode__ (__HI__)));
typedef unsigned int unsigned32 __attribute__ ((__mode__ (__SI__)));
typedef unsigned int unsigned64 __attribute__ ((__mode__ (__DI__)));

typedef struct { unsigned64 a[2]; } unsigned128;
typedef struct { signed64 a[2]; } signed128;

#endif


#if !defined (SIM_TYPES_H) && defined (_MSC_VER)
#define SIM_TYPES_H

/* bit based */

#define UNSIGNED32(X) (X##ui32)
#define UNSIGNED64(X) (X##ui64)

#define SIGNED32(X) (X##i32)
#define SIGNED64(X) (X##i64)

typedef signed char signed8;
typedef signed short signed16;
typedef signed int signed32;
typedef signed __int64 signed64;

typedef unsigned int unsigned8;
typedef unsigned int unsigned16;
typedef unsigned int unsigned32;
typedef unsigned __int64 unsigned64;

typedef struct { unsigned64 a[2]; } unsigned128;
typedef struct { signed64 a[2]; } signed128;

#endif /* _MSC_VER */


#if !defined (SIM_TYPES_H)
#define SIM_TYPES_H

/* bit based */

#define UNSIGNED32(X) (X##UL)
#define UNSIGNED64(X) (X##ULL)

#define SIGNED32(X) (X##L)
#define SIGNED64(X) (X##LL)

typedef signed char signed8;
typedef signed short signed16;
#if defined (__ALPHA__)
typedef signed int signed32;
typedef signed long signed64;
#else
typedef signed long signed32;
typedef signed long long signed64;
#endif

typedef unsigned char unsigned8;
typedef unsigned short unsigned16;
#if defined (__ALPHA__)
typedef unsigned int unsigned32;
typedef unsigned long unsigned64;
#else
typedef unsigned long unsigned32;
typedef unsigned long long unsigned64;
#endif

typedef struct { unsigned64 a[2]; } unsigned128;
typedef struct { signed64 a[2]; } signed128;

#endif


/* byte based */

typedef signed8 signed_1;
typedef signed16 signed_2;
typedef signed32 signed_4;
typedef signed64 signed_8;
typedef signed128 signed_16;

typedef unsigned8 unsigned_1;
typedef unsigned16 unsigned_2;
typedef unsigned32 unsigned_4;
typedef unsigned64 unsigned_8;
typedef unsigned128 unsigned_16;


/* for general work, the following are defined */
/* unsigned: >= 32 bits */
/* signed:   >= 32 bits */
/* long:     >= 32 bits, sign undefined */
/* int:      small indicator */

/* target architecture based */
#if (WITH_TARGET_WORD_BITSIZE == 64)
typedef unsigned64 unsigned_word;
typedef signed64 signed_word;
#endif
#if (WITH_TARGET_WORD_BITSIZE == 32)
typedef unsigned32 unsigned_word;
typedef signed32 signed_word;
#endif
#if (WITH_TARGET_WORD_BITSIZE == 16)
typedef unsigned16 unsigned_word;
typedef signed16 signed_word;
#endif


/* Other instructions */
#if (WITH_TARGET_ADDRESS_BITSIZE == 64)
typedef unsigned64 unsigned_address;
typedef signed64 signed_address;
#endif
#if (WITH_TARGET_ADDRESS_BITSIZE == 32)
typedef unsigned32 unsigned_address;
typedef signed32 signed_address;
#endif
#if (WITH_TARGET_ADDRESS_BITSIZE == 16)
typedef unsigned16 unsigned_address;
typedef signed16 signed_address;
#endif
typedef unsigned_address address_word;


/* IEEE 1275 cell size */
#if (WITH_TARGET_CELL_BITSIZE == 64)
typedef unsigned64 unsigned_cell;
typedef signed64 signed_cell;
#endif
#if (WITH_TARGET_CELL_BITSIZE == 32)
typedef unsigned32 unsigned_cell;
typedef signed32 signed_cell;
#endif
typedef signed_cell cell_word; /* cells are normally signed */


/* Floating point registers */
#if (WITH_TARGET_FLOATING_POINT_BITSIZE == 64)
typedef unsigned64 fp_word;
#endif
#if (WITH_TARGET_FLOATING_POINT_BITSIZE == 32)
typedef unsigned32 fp_word;
#endif

#endif
