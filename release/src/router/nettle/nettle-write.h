/* nettle-write.h

   Internal functions to write out word-sized data to byte arrays.

   Copyright (C) 2010 Niels Möller

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

#ifndef NETTLE_WRITE_H_INCLUDED
#define NETTLE_WRITE_H_INCLUDED

/* For size_t */
#include <stddef.h>

#include "nettle-stdint.h"

/* Write the word array at SRC to the byte array at DST, using little
   endian (le) or big endian (be) byte order, and truncating the
   result to LENGTH bytes. */

/* FIXME: Use a macro shortcut to memcpy for native endianness. */
void
_nettle_write_be32(size_t length, uint8_t *dst,
		   uint32_t *src);
void
_nettle_write_le32(size_t length, uint8_t *dst,
		   uint32_t *src);

void
_nettle_write_le64(size_t length, uint8_t *dst,
		   uint64_t *src);

#endif /* NETTLE_WRITE_H_INCLUDED */
