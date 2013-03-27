/*  This file is part of the program psim.

    Copyright (C) 1994-1995, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#ifndef _SIM_ENDIAN_H_
#define _SIM_ENDIAN_H_


/* C byte conversion functions */

INLINE_PSIM_ENDIAN(unsigned_1) endian_h2t_1(unsigned_1 x);
INLINE_PSIM_ENDIAN(unsigned_2) endian_h2t_2(unsigned_2 x);
INLINE_PSIM_ENDIAN(unsigned_4) endian_h2t_4(unsigned_4 x);
INLINE_PSIM_ENDIAN(unsigned_8) endian_h2t_8(unsigned_8 x);

INLINE_PSIM_ENDIAN(unsigned_1) endian_t2h_1(unsigned_1 x);
INLINE_PSIM_ENDIAN(unsigned_2) endian_t2h_2(unsigned_2 x);
INLINE_PSIM_ENDIAN(unsigned_4) endian_t2h_4(unsigned_4 x);
INLINE_PSIM_ENDIAN(unsigned_8) endian_t2h_8(unsigned_8 x);

INLINE_PSIM_ENDIAN(unsigned_1) swap_1(unsigned_1 x);
INLINE_PSIM_ENDIAN(unsigned_2) swap_2(unsigned_2 x);
INLINE_PSIM_ENDIAN(unsigned_4) swap_4(unsigned_4 x);
INLINE_PSIM_ENDIAN(unsigned_8) swap_8(unsigned_8 x);

INLINE_PSIM_ENDIAN(unsigned_1) endian_h2be_1(unsigned_1 x);
INLINE_PSIM_ENDIAN(unsigned_2) endian_h2be_2(unsigned_2 x);
INLINE_PSIM_ENDIAN(unsigned_4) endian_h2be_4(unsigned_4 x);
INLINE_PSIM_ENDIAN(unsigned_8) endian_h2be_8(unsigned_8 x);

INLINE_PSIM_ENDIAN(unsigned_1) endian_be2h_1(unsigned_1 x);
INLINE_PSIM_ENDIAN(unsigned_2) endian_be2h_2(unsigned_2 x);
INLINE_PSIM_ENDIAN(unsigned_4) endian_be2h_4(unsigned_4 x);
INLINE_PSIM_ENDIAN(unsigned_8) endian_be2h_8(unsigned_8 x);

INLINE_PSIM_ENDIAN(unsigned_1) endian_h2le_1(unsigned_1 x);
INLINE_PSIM_ENDIAN(unsigned_2) endian_h2le_2(unsigned_2 x);
INLINE_PSIM_ENDIAN(unsigned_4) endian_h2le_4(unsigned_4 x);
INLINE_PSIM_ENDIAN(unsigned_8) endian_h2le_8(unsigned_8 x);

INLINE_PSIM_ENDIAN(unsigned_1) endian_le2h_1(unsigned_1 x);
INLINE_PSIM_ENDIAN(unsigned_2) endian_le2h_2(unsigned_2 x);
INLINE_PSIM_ENDIAN(unsigned_4) endian_le2h_4(unsigned_4 x);
INLINE_PSIM_ENDIAN(unsigned_8) endian_le2h_8(unsigned_8 x);


/* Host dependant:

   The CPP below defines information about the compilation host.  In
   particular it defines the macro's:

   	WITH_HOST_BYTE_ORDER	The byte order of the host. Could
				be any of LITTLE_ENDIAN, BIG_ENDIAN
				or 0 (unknown).  Those macro's also
				need to be defined.

 */


/* NetBSD:

   NetBSD is easy, everything you could ever want is in a header file
   (well almost :-) */

#if defined(__NetBSD__)
# include <machine/endian.h>
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER BYTE_ORDER
# endif
# if (BYTE_ORDER != WITH_HOST_BYTE_ORDER)
#  error "host endian incorrectly configured, check config.h"
# endif
#endif

/* Linux is similarly easy.  */

#if defined(__linux__)
# include <endian.h>
# if defined(__LITTLE_ENDIAN) && !defined(LITTLE_ENDIAN)
#  define LITTLE_ENDIAN __LITTLE_ENDIAN
# endif
# if defined(__BIG_ENDIAN) && !defined(BIG_ENDIAN)
#  define BIG_ENDIAN __BIG_ENDIAN
# endif
# if defined(__BYTE_ORDER) && !defined(BYTE_ORDER)
#  define BYTE_ORDER __BYTE_ORDER
# endif
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER BYTE_ORDER
# endif
# if (BYTE_ORDER != WITH_HOST_BYTE_ORDER)
#  error "host endian incorrectly configured, check config.h"
# endif
#endif

/* INSERT HERE - hosts that have available LITTLE_ENDIAN and
   BIG_ENDIAN macro's */


/* Some hosts don't define LITTLE_ENDIAN or BIG_ENDIAN, help them out */

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN 1234
#endif
#ifndef BIG_ENDIAN
#define BIG_ENDIAN 4321
#endif


/* SunOS on SPARC:

   Big endian last time I looked */

#if defined(sparc) || defined(__sparc__)
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER BIG_ENDIAN
# endif
# if (WITH_HOST_BYTE_ORDER != BIG_ENDIAN)
#  error "sun was big endian last time I looked ..."
# endif
#endif


/* Random x86

   Little endian last time I looked */

#if defined(i386) || defined(i486) || defined(i586) || defined (i686) || defined(__i386__) || defined(__i486__) || defined(__i586__) || defined (__i686__)
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER LITTLE_ENDIAN
# endif
# if (WITH_HOST_BYTE_ORDER != LITTLE_ENDIAN)
#  error "x86 was little endian last time I looked ..."
# endif
#endif

#if (defined (__i486__) || defined (__i586__) || defined (__i686__)) && defined(__GNUC__) && WITH_BSWAP
#undef  htonl
#undef  ntohl
#define htonl(IN) __extension__ ({ int _out; __asm__ ("bswap %0" : "=r" (_out) : "0" (IN)); _out; })
#define ntohl(IN) __extension__ ({ int _out; __asm__ ("bswap %0" : "=r" (_out) : "0" (IN)); _out; })
#endif

/* Power or PowerPC running AIX  */
#if defined(_POWER) && defined(_AIX)
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER BIG_ENDIAN
# endif
# if (WITH_HOST_BYTE_ORDER != BIG_ENDIAN)
#  error "Power/PowerPC AIX was big endian last time I looked ..."
# endif
#endif

/* Solaris running PowerPC */
#if defined(__PPC) && defined(__sun__)
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER LITTLE_ENDIAN
# endif
# if (WITH_HOST_BYTE_ORDER != LITTLE_ENDIAN)
#  error "Solaris on PowerPCs was little endian last time I looked ..."
# endif
#endif

/* HP/PA */
#if defined(__hppa__)
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER BIG_ENDIAN
# endif
# if (WITH_HOST_BYTE_ORDER != BIG_ENDIAN)
#  error "HP/PA was big endian last time I looked ..."
# endif
#endif

/* Big endian MIPS */
#if defined(__MIPSEB__)
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER BIG_ENDIAN
# endif
# if (WITH_HOST_BYTE_ORDER != BIG_ENDIAN)
#  error "MIPSEB was big endian last time I looked ..."
# endif
#endif

/* Little endian MIPS */
#if defined(__MIPSEL__)
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER LITTLE_ENDIAN
# endif
# if (WITH_HOST_BYTE_ORDER != LITTLE_ENDIAN)
#  error "MIPSEL was little endian last time I looked ..."
# endif
#endif

/* Windows NT */
#if defined(__WIN32__)
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER LITTLE_ENDIAN
# endif
# if (WITH_HOST_BYTE_ORDER != LITTLE_ENDIAN)
#  error "Windows NT was little endian last time I looked ..."
# endif
#endif

/* Alpha running DEC unix */
#if defined(__osf__) && defined(__alpha__)
# if (WITH_HOST_BYTE_ORDER == 0)
#  undef WITH_HOST_BYTE_ORDER
#  define WITH_HOST_BYTE_ORDER LITTLE_ENDIAN
# endif
# if (WITH_HOST_BYTE_ORDER != LITTLE_ENDIAN)
#  error "AXP running DEC unix was little endian last time I looked ..."
# endif
#endif


/* INSERT HERE - additional hosts that do not have LITTLE_ENDIAN and
   BIG_ENDIAN definitions available.  */

/* SWAP */

#define SWAP_1 swap_1
#define SWAP_2 swap_2
#define SWAP_4 swap_4
#define SWAP_8 swap_8


/* HOST to BE */

#define H2BE_1 endian_h2be_1
#define H2BE_2 endian_h2be_2
#define H2BE_4 endian_h2be_4
#define H2BE_8 endian_h2be_8
#define BE2H_1 endian_be2h_1
#define BE2H_2 endian_be2h_2
#define BE2H_4 endian_be2h_4
#define BE2H_8 endian_be2h_8


/* HOST to LE */

#define H2LE_1 endian_h2le_1
#define H2LE_2 endian_h2le_2
#define H2LE_4 endian_h2le_4
#define H2LE_8 endian_h2le_8
#define LE2H_1 endian_le2h_1
#define LE2H_2 endian_le2h_2
#define LE2H_4 endian_le2h_4
#define LE2H_8 endian_le2h_8


/* HOST to TARGET */

#define H2T_1 endian_h2t_1
#define H2T_2 endian_h2t_2
#define H2T_4 endian_h2t_4
#define H2T_8 endian_h2t_8
#define T2H_1 endian_t2h_1
#define T2H_2 endian_t2h_2
#define T2H_4 endian_t2h_4
#define T2H_8 endian_t2h_8


/* CONVERT IN PLACE

   These macros, given an argument of unknown size, swap its value in
   place if a host/target conversion is required. */

#define H2T(VARIABLE) \
do { \
  switch (sizeof(VARIABLE)) { \
  case 1: VARIABLE = H2T_1(VARIABLE); break; \
  case 2: VARIABLE = H2T_2(VARIABLE); break; \
  case 4: VARIABLE = H2T_4(VARIABLE); break; \
  case 8: VARIABLE = H2T_8(VARIABLE); break; \
  } \
} while (0)

#define T2H(VARIABLE) \
do { \
  switch (sizeof(VARIABLE)) { \
  case 1: VARIABLE = T2H_1(VARIABLE); break; \
  case 2: VARIABLE = T2H_2(VARIABLE); break; \
  case 4: VARIABLE = T2H_4(VARIABLE); break; \
  case 8: VARIABLE = T2H_8(VARIABLE); break; \
  } \
} while (0)

#define SWAP(VARIABLE) \
do { \
  switch (sizeof(VARIABLE)) { \
  case 1: VARIABLE = SWAP_1(VARIABLE); break; \
  case 2: VARIABLE = SWAP_2(VARIABLE); break; \
  case 4: VARIABLE = SWAP_4(VARIABLE); break; \
  case 8: VARIABLE = SWAP_8(VARIABLE); break; \
  } \
} while (0)

#define H2BE(VARIABLE) \
do { \
  switch (sizeof(VARIABLE)) { \
  case 1: VARIABLE = H2BE_1(VARIABLE); break; \
  case 2: VARIABLE = H2BE_2(VARIABLE); break; \
  case 4: VARIABLE = H2BE_4(VARIABLE); break; \
  case 8: VARIABLE = H2BE_8(VARIABLE); break; \
  } \
} while (0)

#define BE2H(VARIABLE) \
do { \
  switch (sizeof(VARIABLE)) { \
  case 1: VARIABLE = BE2H_1(VARIABLE); break; \
  case 2: VARIABLE = BE2H_2(VARIABLE); break; \
  case 4: VARIABLE = BE2H_4(VARIABLE); break; \
  case 8: VARIABLE = BE2H_8(VARIABLE); break; \
  } \
} while (0)

#define H2LE(VARIABLE) \
do { \
  switch (sizeof(VARIABLE)) { \
  case 1: VARIABLE = H2LE_1(VARIABLE); break; \
  case 2: VARIABLE = H2LE_2(VARIABLE); break; \
  case 4: VARIABLE = H2LE_4(VARIABLE); break; \
  case 8: VARIABLE = H2LE_8(VARIABLE); break; \
  } \
} while (0)

#define LE2H(VARIABLE) \
do { \
  switch (sizeof(VARIABLE)) { \
  case 1: VARIABLE = LE2H_1(VARIABLE); break; \
  case 2: VARIABLE = LE2H_2(VARIABLE); break; \
  case 4: VARIABLE = LE2H_4(VARIABLE); break; \
  case 8: VARIABLE = LE2H_8(VARIABLE); break; \
  } \
} while (0)



/* TARGET WORD:

   Byte swap a quantity the size of the targets word */

#if (WITH_TARGET_WORD_BITSIZE == 64)
#define H2T_word H2T_8
#define T2H_word T2H_8
#define H2BE_word H2BE_8
#define BE2H_word BE2H_8
#define H2LE_word H2LE_8
#define LE2H_word LE2H_8
#define SWAP_word SWAP_8
#endif
#if (WITH_TARGET_WORD_BITSIZE == 32)
#define H2T_word H2T_4
#define T2H_word T2H_4
#define H2BE_word H2BE_4
#define BE2H_word BE2H_4
#define H2LE_word H2LE_4
#define LE2H_word LE2H_4
#define SWAP_word SWAP_4
#endif


/* TARGET CELL:

   Byte swap a quantity the size of the targets IEEE 1275 memory cell */

#define H2T_cell H2T_4
#define T2H_cell T2H_4
#define H2BE_cell H2BE_4
#define BE2H_cell BE2H_4
#define H2LE_cell H2LE_4
#define LE2H_cell LE2H_4
#define SWAP_cell SWAP_4


#if (SIM_ENDIAN_INLINE & INCLUDE_MODULE)
# include "sim-endian.c"
#endif

#endif /* _SIM_ENDIAN_H_ */
