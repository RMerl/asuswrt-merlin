/* eax-aes128.c

   Copyright (C) 2013, 2014 Niels Möller

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

#include "eax.h"

void
eax_aes128_set_key(struct eax_aes128_ctx *ctx, const uint8_t *key)
{
  EAX_SET_KEY(ctx,
	      aes128_set_encrypt_key, aes128_encrypt,
	      key);
}

void
eax_aes128_set_nonce(struct eax_aes128_ctx *ctx,
		     size_t length, const uint8_t *iv)
{
  EAX_SET_NONCE(ctx, aes128_encrypt, length, iv);
}

void
eax_aes128_update(struct eax_aes128_ctx *ctx, size_t length, const uint8_t *data)
{
  EAX_UPDATE(ctx, aes128_encrypt, length, data);
}

void
eax_aes128_encrypt(struct eax_aes128_ctx *ctx,
		   size_t length, uint8_t *dst, const uint8_t *src)
{
  EAX_ENCRYPT(ctx, aes128_encrypt, length, dst, src);
}

void
eax_aes128_decrypt(struct eax_aes128_ctx *ctx,
		   size_t length, uint8_t *dst, const uint8_t *src)
{
  EAX_DECRYPT(ctx, aes128_encrypt, length, dst, src);
}

void
eax_aes128_digest(struct eax_aes128_ctx *ctx,
		  size_t length, uint8_t *digest)
{
  EAX_DIGEST(ctx, aes128_encrypt, length, digest);
}
