/* arcfour.c

   The arcfour/rc4 stream cipher.

   Copyright (C) 2001, 2014 Niels Möller

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

#include <assert.h>

#include "arcfour.h"

#define SWAP(a,b) do { int _t = a; a = b; b = _t; } while(0)

void
arcfour_set_key(struct arcfour_ctx *ctx,
		size_t length, const uint8_t *key)
{
  unsigned i, j, k;
  
  assert(length >= ARCFOUR_MIN_KEY_SIZE);
  assert(length <= ARCFOUR_MAX_KEY_SIZE);

  /* Initialize context */
  for (i = 0; i<256; i++)
    ctx->S[i] = i;

  for (i = j = k = 0; i<256; i++)
    {
      j += ctx->S[i] + key[k]; j &= 0xff;
      SWAP(ctx->S[i], ctx->S[j]);
      /* Repeat key as needed */
      k = (k + 1) % length;
    }
  ctx->i = ctx->j = 0;
}

void
arcfour128_set_key(struct arcfour_ctx *ctx, const uint8_t *key)
{
  arcfour_set_key (ctx, ARCFOUR128_KEY_SIZE, key);
}
