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


#ifndef _BITS_C_
#define _BITS_C_

#include "basics.h"

INLINE_BITS\
(unsigned64)
LSMASKED64 (unsigned64 word,
	    int start,
	    int stop)
{
  word &= LSMASK64 (start, stop);
  return word;
}

INLINE_BITS\
(unsigned64)
LSEXTRACTED64 (unsigned64 val,
	       int start,
	       int stop)
{
  val <<= (64 - 1 - start); /* drop high bits */
  val >>= (64 - 1 - start) + (stop); /* drop low bits */
  return val;
}
 
INLINE_BITS\
(unsigned32)
MASKED32(unsigned32 word,
	 unsigned start,
	 unsigned stop)
{
  return (word & MASK32(start, stop));
}

INLINE_BITS\
(unsigned64)
MASKED64(unsigned64 word,
	 unsigned start,
	 unsigned stop)
{
  return (word & MASK64(start, stop));
}

INLINE_BITS\
(unsigned_word)
MASKED(unsigned_word word,
       unsigned start,
       unsigned stop)
{
  return ((word) & MASK(start, stop));
}



INLINE_BITS\
(unsigned_word)
EXTRACTED(unsigned_word word,
	  unsigned start,
	  unsigned stop)
{
  ASSERT(start <= stop);
#if (WITH_TARGET_WORD_BITSIZE == 64)
  return _EXTRACTEDn(64, word, start, stop);
#else
  if (stop < 32)
    return 0;
  else
    return ((word >> (63 - stop))
	    & MASK(start+(63-stop), 63));
#endif
}


INLINE_BITS\
(unsigned_word)
INSERTED(unsigned_word word,
	 unsigned start,
	 unsigned stop)
{
  ASSERT(start <= stop);
#if (WITH_TARGET_WORD_BITSIZE == 64)
  return _INSERTEDn(64, word, start, stop);
#else
  if (stop < 32)
    return 0;
  else
    return ((word & MASK(start+(63-stop), 63))
	    << (63 - stop));
#endif
}


INLINE_BITS\
(unsigned32)
ROTL32(unsigned32 val,
       long shift)
{
  ASSERT(shift >= 0 && shift <= 32);
  return _ROTLn(32, val, shift);
}


INLINE_BITS\
(unsigned64)
ROTL64(unsigned64 val,
       long shift)
{
  ASSERT(shift >= 0 && shift <= 64);
  return _ROTLn(64, val, shift);
}

#endif /* _BITS_C_ */
