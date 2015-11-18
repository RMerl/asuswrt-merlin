/* chacha-crypt.c

   The crypt function in the ChaCha stream cipher.
   Heavily based on the Salsa20 implementation in Nettle.

   Copyright (C) 2014 Niels Möller
   Copyright (C) 2013 Joachim Strömbergson
   Copyright (C) 2012 Simon Josefsson

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
   chacha-ref.c version 2008.01.20.
   D. J. Bernstein
   Public domain.
*/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>

#include "chacha.h"

#include "macros.h"
#include "memxor.h"

#define CHACHA_ROUNDS 20

void
chacha_crypt(struct chacha_ctx *ctx,
	      size_t length,
	      uint8_t *c,
	      const uint8_t *m)
{
  if (!length)
    return;
  
  for (;;)
    {
      uint32_t x[_CHACHA_STATE_LENGTH];

      _chacha_core (x, ctx->state, CHACHA_ROUNDS);

      ctx->state[13] += (++ctx->state[12] == 0);

      /* stopping at 2^70 length per nonce is user's responsibility */
      
      if (length <= CHACHA_BLOCK_SIZE)
	{
	  memxor3 (c, m, x, length);
	  return;
	}
      memxor3 (c, m, x, CHACHA_BLOCK_SIZE);

      length -= CHACHA_BLOCK_SIZE;
      c += CHACHA_BLOCK_SIZE;
      m += CHACHA_BLOCK_SIZE;
  }
}
