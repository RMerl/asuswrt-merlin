/* sha1.c

   The sha1 hash function.
   Defined by http://www.itl.nist.gov/fipspubs/fip180-1.htm.

   Copyright (C) 2001, 2013 Niels Möller

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
#include <stdlib.h>
#include <string.h>

#include "sha1.h"

#include "macros.h"
#include "nettle-write.h"

/* Initialize the SHA values */
void
sha1_init(struct sha1_ctx *ctx)
{
  /* FIXME: Put the buffer last in the struct, and arrange so that we
     can initialize with a single memcpy. */
  static const uint32_t iv[_SHA1_DIGEST_LENGTH] = 
    {
      /* SHA initial values, first 4 identical to md5's. */
      0x67452301L,
      0xEFCDAB89L,
      0x98BADCFEL,
      0x10325476L,
      0xC3D2E1F0L,
    };

  memcpy(ctx->state, iv, sizeof(ctx->state));
  ctx->count = 0;
  
  /* Initialize buffer */
  ctx->index = 0;
}

#define COMPRESS(ctx, data) (_nettle_sha1_compress((ctx)->state, data))

void
sha1_update(struct sha1_ctx *ctx,
	    size_t length, const uint8_t *data)
{
  MD_UPDATE (ctx, length, data, COMPRESS, ctx->count++);
}
	  
void
sha1_digest(struct sha1_ctx *ctx,
	    size_t length,
	    uint8_t *digest)
{
  uint64_t bit_count;

  assert(length <= SHA1_DIGEST_SIZE);

  MD_PAD(ctx, 8, COMPRESS);

  /* There are 512 = 2^9 bits in one block */
  bit_count = (ctx->count << 9) | (ctx->index << 3);

  /* append the 64 bit count */
  WRITE_UINT64(ctx->block + (SHA1_BLOCK_SIZE - 8), bit_count);
  _nettle_sha1_compress(ctx->state, ctx->block);

  _nettle_write_be32(length, digest, ctx->state);
  sha1_init(ctx);
}
