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


#ifndef N
#error "N must be #defined"
#endif

#include "symcat.h"

#if defined(__STDC__) && defined(signed)
/* If signed were defined to be say __signed (ie, some versions of Linux),
   then the signedN macro would not work correctly.  If we have a standard
   compiler, we have signed.  */
#undef signed
#endif

/* NOTE: See end of file for #undef */
#define unsignedN XCONCAT2(unsigned,N)
#define signedN XCONCAT2(signed,N)
#define LSMASKn XCONCAT2(LSMASK,N)
#define MSMASKn XCONCAT2(MSMASK,N)
#define LSMASKEDn XCONCAT2(LSMASKED,N)
#define MSMASKEDn XCONCAT2(MSMASKED,N)
#define LSEXTRACTEDn XCONCAT2(LSEXTRACTED,N)
#define MSEXTRACTEDn XCONCAT2(MSEXTRACTED,N)
#define LSINSERTEDn XCONCAT2(LSINSERTED,N)
#define MSINSERTEDn XCONCAT2(MSINSERTED,N)
#define ROTn XCONCAT2(ROT,N)
#define ROTLn XCONCAT2(ROTL,N)
#define ROTRn XCONCAT2(ROTR,N)
#define MSSEXTn XCONCAT2(MSSEXT,N)
#define LSSEXTn XCONCAT2(LSSEXT,N)

/* TAGS: LSMASKED16 LSMASKED32 LSMASKED64 */

INLINE_SIM_BITS\
(unsignedN)
LSMASKEDn (unsignedN word,
	   int start,
	   int stop)
{
  word &= LSMASKn (start, stop);
  return word;
}

/* TAGS: MSMASKED16 MSMASKED32 MSMASKED64 */

INLINE_SIM_BITS\
(unsignedN)
MSMASKEDn (unsignedN word,
	   int start,
	   int stop)
{
  word &= MSMASKn (start, stop);
  return word;
}

/* TAGS: LSEXTRACTED16 LSEXTRACTED32 LSEXTRACTED64 */

INLINE_SIM_BITS\
(unsignedN)
LSEXTRACTEDn (unsignedN val,
	      int start,
	      int stop)
{
  val <<= (N - 1 - start); /* drop high bits */
  val >>= (N - 1 - start) + (stop); /* drop low bits */
  return val;
}

/* TAGS: MSEXTRACTED16 MSEXTRACTED32 MSEXTRACTED64 */

INLINE_SIM_BITS\
(unsignedN)
MSEXTRACTEDn (unsignedN val,
	      int start,
	      int stop)
{
  val <<= (start); /* drop high bits */
  val >>= (start) + (N - 1 - stop); /* drop low bits */
  return val;
}

/* TAGS: LSINSERTED16 LSINSERTED32 LSINSERTED64 */

INLINE_SIM_BITS\
(unsignedN)
LSINSERTEDn (unsignedN val,
	     int start,
	     int stop)
{
  val <<= stop;
  val &= LSMASKn (start, stop);
  return val;
}

/* TAGS: MSINSERTED16 MSINSERTED32 MSINSERTED64 */

INLINE_SIM_BITS\
(unsignedN)
MSINSERTEDn (unsignedN val,
	     int start,
	     int stop)
{
  val <<= ((N - 1) - stop);
  val &= MSMASKn (start, stop);
  return val;
}

/* TAGS: ROT16 ROT32 ROT64 */

INLINE_SIM_BITS\
(unsignedN)
ROTn (unsignedN val,
      int shift)
{
  if (shift > 0)
    return ROTRn (val, shift);
  else if (shift < 0)
    return ROTLn (val, -shift);
  else
    return val;
}

/* TAGS: ROTL16 ROTL32 ROTL64 */

INLINE_SIM_BITS\
(unsignedN)
ROTLn (unsignedN val,
       int shift)
{
  unsignedN result;
  ASSERT (shift <= N);
  result = (((val) << (shift)) | ((val) >> ((N)-(shift))));
  return result;
}

/* TAGS: ROTR16 ROTR32 ROTR64 */

INLINE_SIM_BITS\
(unsignedN)
ROTRn (unsignedN val,
       int shift)
{
  unsignedN result;
  ASSERT (shift <= N);
  result = (((val) >> (shift)) | ((val) << ((N)-(shift))));
  return result;
}

/* TAGS: LSSEXT16 LSSEXT32 LSSEXT64 */

INLINE_SIM_BITS\
(unsignedN)
LSSEXTn (signedN val,
	 int sign_bit)
{
  int shift;
  /* make the sign-bit most significant and then smear it back into
     position */
  ASSERT (sign_bit < N);
  shift = ((N - 1) - sign_bit);
  val <<= shift;
  val >>= shift;
  return val;
}

/* TAGS: MSSEXT16 MSSEXT32 MSSEXT64 */

INLINE_SIM_BITS\
(unsignedN)
MSSEXTn (signedN val,
	 int sign_bit)
{
  /* make the sign-bit most significant and then smear it back into
     position */
  ASSERT (sign_bit < N);
  val <<= sign_bit;
  val >>= sign_bit;
  return val;
}


/* NOTE: See start of file for #define */
#undef LSSEXTn
#undef MSSEXTn
#undef ROTLn
#undef ROTRn
#undef ROTn
#undef LSINSERTEDn
#undef MSINSERTEDn
#undef LSEXTRACTEDn
#undef MSEXTRACTEDn
#undef LSMASKEDn
#undef LSMASKn
#undef MSMASKEDn
#undef MSMASKn
#undef signedN
#undef unsignedN
