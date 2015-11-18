/* base16-encode.c

   Hex encoding.

   Copyright (C) 2002 Niels Möller

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

#include "base16.h"


static const uint8_t
hex_digits[16] = "0123456789abcdef";

#define DIGIT(x) (hex_digits[(x) & 0xf])

/* Encodes a single byte. Always stores two digits in dst[0] and dst[1]. */
void
base16_encode_single(uint8_t *dst,
		     uint8_t src)
{
  dst[0] = DIGIT(src/0x10);
  dst[1] = DIGIT(src);
}

/* Always stores BASE16_ENCODE_LENGTH(length) digits in dst. */
void
base16_encode_update(uint8_t *dst,
		     size_t length,
		     const uint8_t *src)
{
  size_t i;
  
  for (i = 0; i<length; i++, dst += 2)
    base16_encode_single(dst, src[i]);
}
