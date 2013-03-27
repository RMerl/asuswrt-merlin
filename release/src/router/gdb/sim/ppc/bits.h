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


#ifndef _BITS_H_
#define _BITS_H_


/* bit manipulation routines:

   Bit numbering: The bits are numbered according to the PowerPC
   convention - the left most (or most significant) is bit 0 while the
   right most (least significant) is bit 1.

   Size convention: Each macro is in three forms - <MACRO>32 which
   operates in 32bit quantity (bits are numbered 0..31); <MACRO>64
   which operates using 64bit quantites (and bits are numbered 0..64);
   and <MACRO> which operates using the bit size of the target
   architecture (bits are still numbered 0..63), with 32bit
   architectures ignoring the first 32bits having bit 32 as the most
   significant.

   BIT*(POS): Quantity with just 1 bit set.

   MASK*(FIRST, LAST): Create a constant bit mask of the specified
   size with bits [FIRST .. LAST] set.

   MASKED*(VALUE, FIRST, LAST): Masks out all but bits [FIRST
   .. LAST].

   LSMASKED*(VALUE, FIRST, LAST): Like MASKED - LS bit is zero.

   EXTRACTED*(VALUE, FIRST, LAST): Masks out bits [FIRST .. LAST] but
   also right shifts the masked value so that bit LAST becomes the
   least significant (right most).

   LSEXTRACTED*(VALUE, FIRST, LAST): Same as extracted - LS bit is
   zero.

   SHUFFLED**(VALUE, OLD, NEW): Mask then move a single bit from OLD
   new NEW.

   MOVED**(VALUE, OLD_FIRST, OLD_LAST, NEW_FIRST, NEW_LAST): Moves
   things around so that bits OLD_FIRST..OLD_LAST are masked then
   moved to NEW_FIRST..NEW_LAST.

   INSERTED*(VALUE, FIRST, LAST): Takes VALUE and `inserts' the (LAST
   - FIRST + 1) least significant bits into bit positions [ FIRST
   .. LAST ].  This is almost the complement to EXTRACTED.

   IEA_MASKED(SHOULD_MASK, ADDR): Convert the address to the targets
   natural size.  If in 32bit mode, discard the high 32bits.

   EXTENDED(VALUE): Convert VALUE (32bits of it) to the targets
   natural size.  If in 64bit mode, sign extend the value.

   ALIGN_*(VALUE): Round upwards the value so that it is aligned.

   FLOOR_*(VALUE): Truncate the value so that it is aligned.

   ROTL*(VALUE, NR_BITS): Return the value rotated by NR_BITS

   */

#define _MAKE_SHIFT(WIDTH, pos) ((WIDTH) - 1 - (pos))


#if (WITH_TARGET_WORD_MSB == 0)
#define _LSB_POS(WIDTH, SHIFT) (WIDTH - 1 - SHIFT)
#else
#define _LSB_POS(WIDTH, SHIFT) (SHIFT)
#endif


/* MakeBit */
#define _BITn(WIDTH, pos) (((natural##WIDTH)(1)) \
			   << _MAKE_SHIFT(WIDTH, pos))

#define BIT4(POS)  (1 << _MAKE_SHIFT(4, POS))
#define BIT5(POS)  (1 << _MAKE_SHIFT(5, POS))
#define BIT8(POS)  (1 << _MAKE_SHIFT(8, POS))
#define BIT10(POS)  (1 << _MAKE_SHIFT(10, POS))
#define BIT32(POS) _BITn(32, POS)
#define BIT64(POS) _BITn(64, POS)

#if (WITH_TARGET_WORD_BITSIZE == 64)
#define BIT(POS)   BIT64(POS)
#else
#define BIT(POS)   (((POS) < 32) ? 0 : _BITn(32, (POS)-32))
#endif


/* multi bit mask */
#define _MASKn(WIDTH, START, STOP) \
(((((unsigned##WIDTH)0) - 1) \
  >> (WIDTH - ((STOP) - (START) + 1))) \
 << (WIDTH - 1 - (STOP)))

#define MASK32(START, STOP)   _MASKn(32, START, STOP)
#define MASK64(START, STOP)   _MASKn(64, START, STOP)

/* Multi-bit mask on least significant bits */

#define _LSMASKn(WIDTH, FIRST, LAST) _MASKn (WIDTH, \
					     _LSB_POS (WIDTH, FIRST), \
					     _LSB_POS (WIDTH, LAST))

#define LSMASK64(FIRST, LAST)  _LSMASKn (64, (FIRST), (LAST))

#if (WITH_TARGET_WORD_BITSIZE == 64)
#define MASK(START, STOP) \
(((START) <= (STOP)) \
 ? _MASKn(64, START, STOP) \
 : (_MASKn(64, 0, STOP) \
    | _MASKn(64, START, 63)))
#else
#define MASK(START, STOP) \
(((START) <= (STOP)) \
 ? (((STOP) < 32) \
    ? 0 \
    : _MASKn(32, \
	     (START) < 32 ? 0 : (START) - 32, \
	     (STOP)-32)) \
 : (_MASKn(32, \
	   (START) < 32 ? 0 : (START) - 32, \
	   31) \
    | (((STOP) < 32) \
       ? 0 \
       : _MASKn(32, \
		0, \
		(STOP) - 32))))
#endif


/* mask the required bits, leaving them in place */

INLINE_BITS\
(unsigned32) MASKED32
(unsigned32 word,
 unsigned start,
 unsigned stop);

INLINE_BITS\
(unsigned64) MASKED64
(unsigned64 word,
 unsigned start,
 unsigned stop);

INLINE_BITS\
(unsigned_word) MASKED
(unsigned_word word,
 unsigned start,
 unsigned stop);

INLINE_BITS\
(unsigned64) LSMASKED64
(unsigned64 word,
 int first,
  int last);


/* extract the required bits aligning them with the lsb */
#define _EXTRACTEDn(WIDTH, WORD, START, STOP) \
((((natural##WIDTH)(WORD)) >> (WIDTH - (STOP) - 1)) \
 & _MASKn(WIDTH, WIDTH-1+(START)-(STOP), WIDTH-1))

/* #define EXTRACTED10(WORD, START, STOP) _EXTRACTEDn(10, WORD, START, STOP) */
#define EXTRACTED32(WORD, START, STOP) _EXTRACTEDn(32, WORD, START, STOP)
#define EXTRACTED64(WORD, START, STOP) _EXTRACTEDn(64, WORD, START, STOP)

INLINE_BITS\
(unsigned_word) EXTRACTED
(unsigned_word val,
 unsigned start,
 unsigned stop);

INLINE_BITS\
(unsigned64) LSEXTRACTED64
(unsigned64 val,
 int start,
 int stop);

/* move a single bit around */
/* NB: the wierdness (N>O?N-O:0) is to stop a warning from GCC */
#define _SHUFFLEDn(N, WORD, OLD, NEW) \
((OLD) < (NEW) \
 ? (((unsigned##N)(WORD) \
     >> (((NEW) > (OLD)) ? ((NEW) - (OLD)) : 0)) \
    & MASK32((NEW), (NEW))) \
 : (((unsigned##N)(WORD) \
     << (((OLD) > (NEW)) ? ((OLD) - (NEW)) : 0)) \
    & MASK32((NEW), (NEW))))

#define SHUFFLED32(WORD, OLD, NEW) _SHUFFLEDn(32, WORD, OLD, NEW)
#define SHUFFLED64(WORD, OLD, NEW) _SHUFFLEDn(64, WORD, OLD, NEW)

#define SHUFFLED(WORD, OLD, NEW) _SHUFFLEDn(_word, WORD, OLD, NEW)


/* move a group of bits around */
#define _INSERTEDn(N, WORD, START, STOP) \
(((natural##N)(WORD) << _MAKE_SHIFT(N, STOP)) & _MASKn(N, START, STOP))

#define INSERTED32(WORD, START, STOP) _INSERTEDn(32, WORD, START, STOP)
#define INSERTED64(WORD, START, STOP) _INSERTEDn(64, WORD, START, STOP)

INLINE_BITS\
(unsigned_word) INSERTED
(unsigned_word val,
 unsigned start,
 unsigned stop);


/* depending on MODE return a 64bit or 32bit (sign extended) value */
#if (WITH_TARGET_WORD_BITSIZE == 64)
#define EXTENDED(X)     ((signed64)(signed32)(X))
#else
#define EXTENDED(X)     (X)
#endif


/* memory alignment macro's */
#define _ALIGNa(A,X)  (((X) + ((A) - 1)) & ~((A) - 1))
#define _FLOORa(A,X)  ((X) & ~((A) - 1))

#define ALIGN_8(X)	_ALIGNa(8, X)
#define ALIGN_16(X)	_ALIGNa(16, X)

#define ALIGN_PAGE(X)	_ALIGNa(0x1000, X)
#define FLOOR_PAGE(X)   ((X) & ~(0x1000 - 1))


/* bit bliting macro's */
#define BLIT32(V, POS, BIT) \
do { \
  if (BIT) \
    V |= BIT32(POS); \
  else \
    V &= ~BIT32(POS); \
} while (0)
#define MBLIT32(V, LO, HI, VAL) \
do { \
  (V) = (((V) & ~MASK32((LO), (HI))) \
	 | INSERTED32(VAL, LO, HI)); \
} while (0)


/* some rotate functions to make things easier

   NOTE: These are functions not macro's as the latter tickles bugs in
   gcc-2.6.3 */

#define _ROTLn(N, VAL, SHIFT) \
(((VAL) << (SHIFT)) | ((VAL) >> ((N)-(SHIFT))))

INLINE_BITS\
(unsigned32) ROTL32
(unsigned32 val,
 long shift);

INLINE_BITS\
(unsigned64) ROTL64
(unsigned64 val,
 long shift);


#if (BITS_INLINE & INCLUDE_MODULE)
#include "bits.c"
#endif

#endif /* _BITS_H_ */
