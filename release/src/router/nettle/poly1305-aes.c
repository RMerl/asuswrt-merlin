/* poly1305-aes.c

   Copyright (C) 2013 Nikos Mavrogiannopoulos
   Copyright (C) 2014 Niels Möller

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
#include "config.h"
#endif

#include <assert.h>
#include <string.h>

#include "poly1305.h"
#include "macros.h"

void
poly1305_aes_set_key (struct poly1305_aes_ctx *ctx, const uint8_t * key)
{
  aes128_set_encrypt_key(&ctx->aes, (key));
  poly1305_set_key(&ctx->pctx, (key+16));
  ctx->index = 0;
}

void
poly1305_aes_set_nonce (struct poly1305_aes_ctx *ctx,
			const uint8_t * nonce)
{
  memcpy (ctx->nonce, nonce, POLY1305_AES_NONCE_SIZE);
}

#define COMPRESS(ctx, data) _poly1305_block(&(ctx)->pctx, (data), 1)

void
poly1305_aes_update (struct poly1305_aes_ctx *ctx,
		     size_t length, const uint8_t *data)
{
  MD_UPDATE (ctx, length, data, COMPRESS, (void) 0);
}

void
poly1305_aes_digest (struct poly1305_aes_ctx *ctx,
		     size_t length, uint8_t *digest)
{
  union nettle_block16 s;
  /* final bytes */
  if (ctx->index > 0)
    {
      assert (ctx->index < POLY1305_BLOCK_SIZE);

      ctx->block[ctx->index] = 1;
      memset (ctx->block + ctx->index + 1,
	      0, POLY1305_BLOCK_SIZE - 1 - ctx->index);

      _poly1305_block (&ctx->pctx, ctx->block, 0);
    }
  aes128_encrypt(&ctx->aes, POLY1305_BLOCK_SIZE, s.b, ctx->nonce);
  
  poly1305_digest (&ctx->pctx, &s);
  memcpy (digest, s.b, length);

  INCREMENT (16, ctx->nonce);
  ctx->index = 0;
}
