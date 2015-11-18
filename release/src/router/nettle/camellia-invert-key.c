/* camellia-invert-key.c

   Inverting a key means reversing order of subkeys.

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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "camellia-internal.h"

#define SWAP(a, b) \
do { uint64_t t_swap = (a); (a) = (b); (b) = t_swap; } while(0)

void
_camellia_invert_key(unsigned nkeys,
		     uint64_t *dst, const uint64_t *src)
{
  unsigned i;
  if (dst == src)
    for (i = 0; i < nkeys - 1 - i; i++)
	SWAP (dst[i], dst[nkeys - 1- i]);
  else
    for (i = 0; i < nkeys; i++)
      dst[i] = src[nkeys - 1 - i];
}
