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

/* NOTE: See end of file for #undef */
#define unsigned_N XCONCAT2(unsigned_,N)
#define endian_t2h_N XCONCAT2(endian_t2h_,N)
#define endian_h2t_N XCONCAT2(endian_h2t_,N)
#define _SWAP_N XCONCAT2(_SWAP_,N)
#define swap_N XCONCAT2(swap_,N)
#define endian_h2be_N XCONCAT2(endian_h2be_,N)
#define endian_be2h_N XCONCAT2(endian_be2h_,N)
#define endian_h2le_N XCONCAT2(endian_h2le_,N)
#define endian_le2h_N XCONCAT2(endian_le2h_,N)
#define offset_N XCONCAT2(offset_,N)

/* TAGS: endian_t2h_1 endian_t2h_2 endian_t2h_4 endian_t2h_8 endian_t2h_16 */

INLINE_SIM_ENDIAN\
(unsigned_N)
endian_t2h_N(unsigned_N raw_in)
{
  if (CURRENT_TARGET_BYTE_ORDER == CURRENT_HOST_BYTE_ORDER) {
    return raw_in;
  }
  else {
    _SWAP_N(return,raw_in);
  }
}

/* TAGS: endian_h2t_1 endian_h2t_2 endian_h2t_4 endian_h2t_8 endian_h2t_16 */

INLINE_SIM_ENDIAN\
(unsigned_N)
endian_h2t_N(unsigned_N raw_in)
{
  if (CURRENT_TARGET_BYTE_ORDER == CURRENT_HOST_BYTE_ORDER) {
    return raw_in;
  }
  else {
    _SWAP_N(return,raw_in);
  }
}

/* TAGS: swap_1 swap_2 swap_4 swap_8 swap_16 */

INLINE_SIM_ENDIAN\
(unsigned_N)
swap_N(unsigned_N raw_in)
{
  _SWAP_N(return,raw_in);
}

/* TAGS: endian_h2be_1 endian_h2be_2 endian_h2be_4 endian_h2be_8 endian_h2be_16 */

INLINE_SIM_ENDIAN\
(unsigned_N)
endian_h2be_N(unsigned_N raw_in)
{
  if (CURRENT_HOST_BYTE_ORDER == BIG_ENDIAN) {
    return raw_in;
  }
  else {
    _SWAP_N(return,raw_in);
  }
}

/* TAGS: endian_be2h_1 endian_be2h_2 endian_be2h_4 endian_be2h_8 endian_be2h_16 */

INLINE_SIM_ENDIAN\
(unsigned_N)
endian_be2h_N(unsigned_N raw_in)
{
  if (CURRENT_HOST_BYTE_ORDER == BIG_ENDIAN) {
    return raw_in;
  }
  else {
    _SWAP_N(return,raw_in);
  }
}

/* TAGS: endian_h2le_1 endian_h2le_2 endian_h2le_4 endian_h2le_8 endian_h2le_16 */

INLINE_SIM_ENDIAN\
(unsigned_N)
endian_h2le_N(unsigned_N raw_in)
{
  if (CURRENT_HOST_BYTE_ORDER == LITTLE_ENDIAN) {
    return raw_in;
  }
  else {
    _SWAP_N(return,raw_in);
  }
}

/* TAGS: endian_le2h_1 endian_le2h_2 endian_le2h_4 endian_le2h_8 endian_le2h_16 */

INLINE_SIM_ENDIAN\
(unsigned_N)
endian_le2h_N(unsigned_N raw_in)
{
  if (CURRENT_HOST_BYTE_ORDER == LITTLE_ENDIAN) {
    return raw_in;
  }
  else {
    _SWAP_N(return,raw_in);
  }
}

/* TAGS: offset_1 offset_2 offset_4 offset_8 offset_16 */

INLINE_SIM_ENDIAN\
(void*)
offset_N (unsigned_N *x,
	  unsigned sizeof_word,
	  unsigned word)
{
  char *in = (char*)x;
  char *out;
  unsigned offset = sizeof_word * word;
  ASSERT (offset + sizeof_word <= sizeof(unsigned_N));
  ASSERT (word < (sizeof (unsigned_N) / sizeof_word));
  ASSERT ((sizeof (unsigned_N) % sizeof_word) == 0);
  if (WITH_HOST_BYTE_ORDER == LITTLE_ENDIAN)
    {
      out = in + sizeof (unsigned_N) - offset - sizeof_word;
    }
  else
    {
      out = in + offset;
    }
  return out;
}


/* NOTE: See start of file for #define */
#undef unsigned_N
#undef endian_t2h_N
#undef endian_h2t_N
#undef _SWAP_N
#undef swap_N
#undef endian_h2be_N
#undef endian_be2h_N
#undef endian_h2le_N
#undef endian_le2h_N
#undef offset_N
