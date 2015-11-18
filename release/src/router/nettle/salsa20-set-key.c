/* salsa20-set-key.c

   The Salsa20 stream cipher.

   Copyright (C) 2012 Simon Josefsson, Niels Möller

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

/* Based on:
   salsa20-ref.c version 20051118
   D. J. Bernstein
   Public domain.
*/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include "salsa20.h"

#include "macros.h"

void
salsa20_set_key(struct salsa20_ctx *ctx,
		size_t length, const uint8_t *key)
{
  switch (length)
    {
    case SALSA20_128_KEY_SIZE:
      salsa20_128_set_key (ctx, key);
      break;
    case SALSA20_256_KEY_SIZE:
      salsa20_256_set_key (ctx, key);
      break;
    default:
      abort();
    }
}
