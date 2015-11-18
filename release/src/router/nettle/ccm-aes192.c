/* ccm-aes192.c

   Counter with CBC-MAC mode using AES192 as the underlying cipher.

   Copyright (C) 2014 Exegin Technologies Limited
   Copyright (C) 2014 Owen Kirby

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

#include "aes.h"
#include "ccm.h"


void
ccm_aes192_set_key(struct ccm_aes192_ctx *ctx, const uint8_t *key)
{
  aes192_set_encrypt_key(&ctx->cipher, key);
}

void
ccm_aes192_set_nonce(struct ccm_aes192_ctx *ctx, size_t length, const uint8_t *nonce,
		     size_t authlen, size_t msglen, size_t taglen)
{
  ccm_set_nonce(&ctx->ccm, &ctx->cipher, (nettle_cipher_func *) aes192_encrypt,
		length, nonce, authlen, msglen, taglen);
}

void
ccm_aes192_update(struct ccm_aes192_ctx *ctx,
		  size_t length, const uint8_t *data)
{
  ccm_update(&ctx->ccm, &ctx->cipher, (nettle_cipher_func *) aes192_encrypt,
	     length, data);
}

void
ccm_aes192_encrypt(struct ccm_aes192_ctx *ctx,
		   size_t length, uint8_t *dst, const uint8_t *src)
{
  ccm_encrypt(&ctx->ccm, &ctx->cipher, (nettle_cipher_func *) aes192_encrypt,
	      length, dst, src);
}

void
ccm_aes192_decrypt(struct ccm_aes192_ctx *ctx,
		   size_t length, uint8_t *dst, const uint8_t *src)
{
  ccm_decrypt(&ctx->ccm, &ctx->cipher, (nettle_cipher_func *) aes192_encrypt,
	      length, dst, src);
}

void
ccm_aes192_digest(struct ccm_aes192_ctx *ctx,
		  size_t length, uint8_t *digest)
{
  ccm_digest(&ctx->ccm, &ctx->cipher, (nettle_cipher_func *) aes192_encrypt,
	     length, digest);
}

void
ccm_aes192_encrypt_message(struct ccm_aes192_ctx *ctx,
			   size_t nlength, const uint8_t *nonce,
			   size_t alength, const uint8_t *adata,
			   size_t tlength,
			   size_t clength, uint8_t *dst, const uint8_t *src)
{
  ccm_encrypt_message(&ctx->cipher, (nettle_cipher_func *) aes192_encrypt,
		      nlength, nonce, alength, adata,
		      tlength, clength, dst, src);
}

int
ccm_aes192_decrypt_message(struct ccm_aes192_ctx *ctx,
			   size_t nlength, const uint8_t *nonce,
			   size_t alength, const uint8_t *adata,
			   size_t tlength,
			   size_t mlength, uint8_t *dst, const uint8_t *src)
{
  return ccm_decrypt_message(&ctx->cipher,
			     (nettle_cipher_func *) aes192_encrypt,
			     nlength, nonce, alength, adata,
			     tlength, mlength, dst, src);
}
