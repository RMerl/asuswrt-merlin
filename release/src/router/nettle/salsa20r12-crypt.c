/* salsa20r12-crypt.c

   The Salsa20 stream cipher, reduced round variant.

   Copyright (C) 2013 Nikos Mavrogiannopoulos

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

#include <string.h>

#include "salsa20.h"

#include "macros.h"
#include "memxor.h"

void
salsa20r12_crypt(struct salsa20_ctx *ctx,
		 size_t length,
		 uint8_t *c,
		 const uint8_t *m)
{
  uint32_t x[_SALSA20_INPUT_LENGTH];

  if (!length)
    return;
  
  for (;;)
    {

      _salsa20_core (x, ctx->input, 12);

      ctx->input[9] += (++ctx->input[8] == 0);

      /* stopping at 2^70 length per nonce is user's responsibility */
      
      if (length <= SALSA20_BLOCK_SIZE)
	{
	  memxor3 (c, m, x, length);
	  return;
	}
      memxor3 (c, m, x, SALSA20_BLOCK_SIZE);

      length -= SALSA20_BLOCK_SIZE;
      c += SALSA20_BLOCK_SIZE;
      m += SALSA20_BLOCK_SIZE;
    }
}
