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


#ifndef N
#error "N must be #defined"
#endif

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


INLINE_PSIM_ENDIAN\
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


INLINE_PSIM_ENDIAN\
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


INLINE_PSIM_ENDIAN\
(unsigned_N)
swap_N(unsigned_N raw_in)
{
  _SWAP_N(return,raw_in);
}



INLINE_PSIM_ENDIAN\
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


INLINE_PSIM_ENDIAN\
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


INLINE_PSIM_ENDIAN\
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


INLINE_PSIM_ENDIAN\
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
