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


#ifndef _SIM_ENDIAN_C_
#define _SIM_ENDIAN_C_

#include "config.h"
#include "basics.h"


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

#define N 1
#include "sim-endian-n.h"
#undef N

#define N 2
#include "sim-endian-n.h"
#undef N

#define N 4
#include "sim-endian-n.h"
#undef N

#define N 8
#include "sim-endian-n.h"
#undef N

#endif /* _SIM_ENDIAN_C_ */
