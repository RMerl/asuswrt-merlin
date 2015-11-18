/* md5.c

   The MD5 hash function, described in RFC 1321.

   Copyright (C) 2001 Niels Möller

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

/* Based on public domain code hacked by Colin Plumb, Andrew Kuchling, and
 * Niels Möller. */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <string.h>

#include "md5.h"

#include "macros.h"
#include "nettle-write.h"

void
md5_init(struct md5_ctx *ctx)
{
  const uint32_t iv[_MD5_DIGEST_LENGTH] =
    {
      0x67452301,
      0xefcdab89,
      0x98badcfe,
      0x10325476,
    };
  memcpy(ctx->state, iv, sizeof(ctx->state));
  ctx->count = 0;
  ctx->index = 0;
}

#define COMPRESS(ctx, data) (_nettle_md5_compress((ctx)->state, (data)))

void
md5_update(struct md5_ctx *ctx,
	   size_t length,
	   const uint8_t *data)
{
  MD_UPDATE(ctx, length, data, COMPRESS, ctx->count++);
}

void
md5_digest(struct md5_ctx *ctx,
	   size_t length,
	   uint8_t *digest)
{
  uint64_t bit_count;
  
  assert(length <= MD5_DIGEST_SIZE);

  MD_PAD(ctx, 8, COMPRESS);

  /* There are 512 = 2^9 bits in one block */
  bit_count = (ctx->count << 9) | (ctx->index << 3);

  LE_WRITE_UINT64(ctx->block + (MD5_BLOCK_SIZE - 8), bit_count);
  _nettle_md5_compress(ctx->state, ctx->block);

  _nettle_write_le32(length, digest, ctx->state);
  md5_init(ctx);
}
