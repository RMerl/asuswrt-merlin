/* write-le64.c

   Copyright (C) 2001, 2011, 2012 Niels Möller

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
*/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "nettle-write.h"

#include "macros.h"

void
_nettle_write_le64(size_t length, uint8_t *dst,
		   uint64_t *src)
{
  size_t i;
  size_t words;
  unsigned leftover;
  
  words = length / 8;
  leftover = length % 8;

  for (i = 0; i < words; i++, dst += 8)
    LE_WRITE_UINT64(dst, src[i]);

  if (leftover)
    {
      uint64_t word;
      
      word = src[i];

      do
	{
	  *dst++ = word & 0xff;
	  word >>= 8;
	}
      while (--leftover);
    }
}
