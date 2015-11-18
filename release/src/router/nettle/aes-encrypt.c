/* aes-encrypt.c

   Encryption function for the aes/rijndael block cipher.

   Copyright (C) 2002, 2013 Niels Möller

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

#include "aes-internal.h"

/* The main point on this function is to help the assembler
   implementations of _nettle_aes_encrypt to get the table pointer.
   For PIC code, the details can be complex and system dependent. */
void
aes_encrypt(const struct aes_ctx *ctx,
	    size_t length, uint8_t *dst,
	    const uint8_t *src)
{
  assert(!(length % AES_BLOCK_SIZE) );
  _aes_encrypt(ctx->rounds, ctx->keys, &_aes_encrypt_table,
	       length, dst, src);
}

void
aes128_encrypt(const struct aes128_ctx *ctx,
	       size_t length, uint8_t *dst,
	       const uint8_t *src)
{
  assert(!(length % AES_BLOCK_SIZE) );
  _aes_encrypt(_AES128_ROUNDS, ctx->keys, &_aes_encrypt_table,
	       length, dst, src);
}

void
aes192_encrypt(const struct aes192_ctx *ctx,
	       size_t length, uint8_t *dst,
	       const uint8_t *src)
{
  assert(!(length % AES_BLOCK_SIZE) );
  _aes_encrypt(_AES192_ROUNDS, ctx->keys, &_aes_encrypt_table,
	       length, dst, src);
}

void
aes256_encrypt(const struct aes256_ctx *ctx,
	       size_t length, uint8_t *dst,
	       const uint8_t *src)
{
  assert(!(length % AES_BLOCK_SIZE) );
  _aes_encrypt(_AES256_ROUNDS, ctx->keys, &_aes_encrypt_table,
	       length, dst, src);
}
