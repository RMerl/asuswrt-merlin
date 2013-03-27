/* The common simulator framework for GDB, the GNU Debugger.

   Copyright 2002, 2007 Free Software Foundation, Inc.

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


#ifndef _SIM_ENDIAN_C_
#define _SIM_ENDIAN_C_

#include "sim-basics.h"
#include "sim-assert.h"
#include "sim-io.h"


#if !defined(_SWAP_1)
#define _SWAP_1(SET,RAW) SET (RAW)
#endif

#if !defined(_SWAP_2) && (WITH_HOST_BYTE_ORDER == LITTLE_ENDIAN) && defined(htons)
#define _SWAP_2(SET,RAW) SET htons (RAW)
#endif

#ifndef	_SWAP_2
#define _SWAP_2(SET,RAW) SET (((RAW) >> 8) | ((RAW) << 8))
#endif

#if !defined(_SWAP_4) && (WITH_HOST_BYTE_ORDER == LITTLE_ENDIAN) && defined(htonl)
#define _SWAP_4(SET,RAW) SET htonl (RAW)
#endif

#ifndef _SWAP_4
#define	_SWAP_4(SET,RAW) SET (((RAW) << 24) | (((RAW) & 0xff00) << 8) | (((RAW) & 0xff0000) >> 8) | ((RAW) >> 24))
#endif

#ifndef _SWAP_8
#define _SWAP_8(SET,RAW) \
  union { unsigned_8 dword; unsigned_4 words[2]; } in, out; \
  in.dword = RAW; \
  _SWAP_4 (out.words[0] =, in.words[1]); \
  _SWAP_4 (out.words[1] =, in.words[0]); \
  SET out.dword;
#endif

#ifndef _SWAP_16
#define _SWAP_16(SET,RAW) \
  union { unsigned_16 word; unsigned_4 words[4]; } in, out; \
  in.word = (RAW); \
  _SWAP_4 (out.words[0] =, in.words[3]); \
  _SWAP_4 (out.words[1] =, in.words[2]); \
  _SWAP_4 (out.words[2] =, in.words[1]); \
  _SWAP_4 (out.words[3] =, in.words[0]); \
  SET out.word;
#endif


#define N 1
#include "sim-n-endian.h"
#undef N

#define N 2
#include "sim-n-endian.h"
#undef N

#define N 4
#include "sim-n-endian.h"
#undef N

#define N 8
#include "sim-n-endian.h"
#undef N

#define N 16
#include "sim-n-endian.h"
#undef N


INLINE_SIM_ENDIAN\
(unsigned_8)
sim_endian_split_16 (unsigned_16 word, int w)
{
  if (CURRENT_HOST_BYTE_ORDER == LITTLE_ENDIAN)
    {
      return word.a[1 - w];
    }
  else
    {
      return word.a[w];
    }
}


INLINE_SIM_ENDIAN\
(unsigned_16)
sim_endian_join_16 (unsigned_8 h, unsigned_8 l)

{
  unsigned_16 word;
  if (CURRENT_HOST_BYTE_ORDER == LITTLE_ENDIAN)
    {
      word.a[0] = l;
      word.a[1] = h;
    }
  else
    {
      word.a[0] = h;
      word.a[1] = l;
    }
  return word;
}



#endif /* _SIM_ENDIAN_C_ */
