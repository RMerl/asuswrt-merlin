/* salsa20-128-set-key.c

   The Salsa20 stream cipher. Key setup for 256-bit keys.

   Copyright (C) 2012 Simon Josefsson
   Copyright (C) 2012-2014 Niels Möller

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

#include "salsa20.h"

#include "macros.h"

void
salsa20_128_set_key(struct salsa20_ctx *ctx, const uint8_t *key)
{
  ctx->input[11] = ctx->input[1] = LE_READ_UINT32(key + 0);
  ctx->input[12] = ctx->input[2] = LE_READ_UINT32(key + 4);
  ctx->input[13] = ctx->input[3] = LE_READ_UINT32(key + 8);
  ctx->input[14] = ctx->input[4] = LE_READ_UINT32(key + 12);

  /* "expand 16-byte k" */
  ctx->input[0]  = 0x61707865;
  ctx->input[5]  = 0x3120646e;
  ctx->input[10] = 0x79622d36;
  ctx->input[15] = 0x6b206574;
}
