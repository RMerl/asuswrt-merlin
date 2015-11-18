/* chacha-set-nonce.c

   Setting the nonce the ChaCha stream cipher.
   Based on the Salsa20 implementation in Nettle.

   Copyright (C) 2013 Joachim Strömbergon
   Copyright (C) 2012 Simon Josefsson
   Copyright (C) 2012, 2014 Niels Möller

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
   ChaCha specification (doc id: 4027b5256e17b9796842e6d0f68b0b5e) and reference 
   implementation dated 2008.01.20
   D. J. Bernstein
   Public domain.
*/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include "chacha.h"

#include "macros.h"

void
chacha_set_nonce(struct chacha_ctx *ctx, const uint8_t *nonce)
{
  ctx->state[12] = 0;
  ctx->state[13] = 0;
  ctx->state[14] = LE_READ_UINT32(nonce + 0);
  ctx->state[15] = LE_READ_UINT32(nonce + 4);
}

void
chacha_set_nonce96(struct chacha_ctx *ctx, const uint8_t *nonce)
{
  ctx->state[12] = 0;
  ctx->state[13] = LE_READ_UINT32(nonce + 0);
  ctx->state[14] = LE_READ_UINT32(nonce + 4);
  ctx->state[15] = LE_READ_UINT32(nonce + 8);
}
