/* umac32.c

   Copyright (C) 2013 Niels Möller

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
#include <string.h>

#include "umac.h"

#include "macros.h"

void
umac32_set_key (struct umac32_ctx *ctx, const uint8_t *key)
{
  _umac_set_key (ctx->l1_key, ctx->l2_key, ctx->l3_key1, ctx->l3_key2,
		 &ctx->pdf_key, key, 1);

  /* Clear nonce */
  memset (ctx->nonce, 0, sizeof(ctx->nonce));
  ctx->nonce_low = 0;
  ctx->nonce_length = sizeof(ctx->nonce);

  /* Initialize buffer */
  ctx->count = ctx->index = 0;
}

void
umac32_set_nonce (struct umac32_ctx *ctx,
		  size_t nonce_length, const uint8_t *nonce)
{
  assert (nonce_length > 0);
  assert (nonce_length <= AES_BLOCK_SIZE);

  memcpy (ctx->nonce, nonce, nonce_length);
  memset (ctx->nonce + nonce_length, 0, AES_BLOCK_SIZE - nonce_length);

  ctx->nonce_low = ctx->nonce[nonce_length - 1] & 3;
  ctx->nonce[nonce_length - 1] &= ~3;
  ctx->nonce_length = nonce_length;
}

#define UMAC32_BLOCK(ctx, block) do {					\
    uint64_t __umac32_y							\
      = _umac_nh (ctx->l1_key, UMAC_BLOCK_SIZE, block)			\
      + 8*UMAC_BLOCK_SIZE ;						\
    _umac_l2 (ctx->l2_key, ctx->l2_state, 1, ctx->count++, &__umac32_y); \
  } while (0)

void
umac32_update (struct umac32_ctx *ctx,
	       size_t length, const uint8_t *data)
{
  MD_UPDATE (ctx, length, data, UMAC32_BLOCK, (void)0);
}


void
umac32_digest (struct umac32_ctx *ctx,
	       size_t length, uint8_t *digest)
{
  uint32_t pad;

  assert (length > 0);
  assert (length <= 4);

  if (ctx->index > 0 || ctx->count == 0)
    {
      /* Zero pad to multiple of 32 */
      uint64_t y;
      unsigned pad = (ctx->index > 0) ? 31 & - ctx->index : 32;
      memset (ctx->block + ctx->index, 0, pad);

      y = _umac_nh (ctx->l1_key, ctx->index + pad, ctx->block)
	+ 8 * ctx->index;
      _umac_l2 (ctx->l2_key, ctx->l2_state, 1, ctx->count++, &y);
    }
  assert (ctx->count > 0);
  if ( !(ctx->nonce_low & _UMAC_NONCE_CACHED))
    {
      aes128_encrypt (&ctx->pdf_key, AES_BLOCK_SIZE,
		      (uint8_t *) ctx->pad_cache, ctx->nonce);
      ctx->nonce_low |= _UMAC_NONCE_CACHED;
    }

  pad = ctx->pad_cache[ctx->nonce_low & 3];

  /* Increment nonce */
  ctx->nonce_low++;
  if ( !(ctx->nonce_low & 3))
    {
      unsigned i = ctx->nonce_length - 1;

      ctx->nonce_low = 0;
      ctx->nonce[i] += 4;

      if (ctx->nonce[i] == 0 && i > 0)
	INCREMENT (i, ctx->nonce);
    }

  _umac_l2_final (ctx->l2_key, ctx->l2_state, 1, ctx->count);
  pad ^= ctx->l3_key2[0] ^ _umac_l3 (ctx->l3_key1, ctx->l2_state);
  memcpy (digest, &pad, length);

  /* Reinitialize */
  ctx->count = ctx->index = 0;
}
