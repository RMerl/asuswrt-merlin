/* des-compat.c

   The des block cipher, old libdes/openssl-style interface.

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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <assert.h>

#include "des-compat.h"

#include "cbc.h"
#include "macros.h"
#include "memxor.h"

struct des_compat_des3 { const struct des_ctx *keys[3]; }; 

static void
des_compat_des3_encrypt(struct des_compat_des3 *ctx,
			uint32_t length, uint8_t *dst, const uint8_t *src)
{
  nettle_des_encrypt(ctx->keys[0], length, dst, src);
  nettle_des_decrypt(ctx->keys[1], length, dst, dst);
  nettle_des_encrypt(ctx->keys[2], length, dst, dst);
}

static void
des_compat_des3_decrypt(struct des_compat_des3 *ctx,
			uint32_t length, uint8_t *dst, const uint8_t *src)
{
  nettle_des_decrypt(ctx->keys[2], length, dst, src);
  nettle_des_encrypt(ctx->keys[1], length, dst, dst);
  nettle_des_decrypt(ctx->keys[0], length, dst, dst);
}

void
des_ecb3_encrypt(const_des_cblock *src, des_cblock *dst,
		 des_key_schedule k1,
		 des_key_schedule k2,
		 des_key_schedule k3, int enc)
{
  struct des_compat_des3 keys;
  keys.keys[0] = k1;
  keys.keys[1] = k2;
  keys.keys[2] = k3;

  ((enc == DES_ENCRYPT) ? des_compat_des3_encrypt : des_compat_des3_decrypt)
    (&keys, DES_BLOCK_SIZE, *dst, *src);
}

/* If input is not a integral number of blocks, the final block is
   padded with zeros, no length field or anything like that. That's
   pretty broken, since it means that "$100" and "$100\0" always have
   the same checksum, but I think that's how it's supposed to work. */
uint32_t
des_cbc_cksum(const uint8_t *src, des_cblock *dst,
	      long length, des_key_schedule ctx,
	      const_des_cblock *iv)
{
  /* FIXME: I'm not entirely sure how this function is supposed to
   * work, in particular what it should return, and if iv can be
   * modified. */
  uint8_t block[DES_BLOCK_SIZE];

  memcpy(block, *iv, DES_BLOCK_SIZE);

  while (length >= DES_BLOCK_SIZE)
    {
      memxor(block, src, DES_BLOCK_SIZE);
      nettle_des_encrypt(ctx, DES_BLOCK_SIZE, block, block);

      src += DES_BLOCK_SIZE;
      length -= DES_BLOCK_SIZE;	  
    }
  if (length > 0)
    {
      memxor(block, src, length);
      nettle_des_encrypt(ctx, DES_BLOCK_SIZE, block, block);	  
    }
  memcpy(*dst, block, DES_BLOCK_SIZE);

  return LE_READ_UINT32(block + 4);
}

void
des_ncbc_encrypt(const_des_cblock *src, des_cblock *dst, long length,
                 des_key_schedule ctx, des_cblock *iv,
                 int enc)
{
  switch (enc)
    {
    case DES_ENCRYPT:
      nettle_cbc_encrypt(ctx, (nettle_cipher_func *) des_encrypt,
			 DES_BLOCK_SIZE, *iv,
			 length, *dst, *src);
      break;
    case DES_DECRYPT:
      nettle_cbc_decrypt(ctx,
			 (nettle_cipher_func *) des_decrypt,
			 DES_BLOCK_SIZE, *iv,
			 length, *dst, *src);
      break;
    default:
      abort();
    }
}

void
des_cbc_encrypt(const_des_cblock *src, des_cblock *dst, long length,
		des_key_schedule ctx, const_des_cblock *civ,
		int enc)
{
  des_cblock iv;

  memcpy(iv, civ, DES_BLOCK_SIZE);

  des_ncbc_encrypt(src, dst, length, ctx, &iv, enc);
}


void
des_ecb_encrypt(const_des_cblock *src, des_cblock *dst,
		des_key_schedule ctx,
		int enc)
{
  ((enc == DES_ENCRYPT) ? nettle_des_encrypt : nettle_des_decrypt)
    (ctx, DES_BLOCK_SIZE, *dst, *src);
}

void
des_ede3_cbc_encrypt(const_des_cblock *src, des_cblock *dst, long length,
		     des_key_schedule k1,
		     des_key_schedule k2,
		     des_key_schedule k3,
		     des_cblock *iv,
		     int enc)
{
  struct des_compat_des3 keys;
  keys.keys[0] = k1;
  keys.keys[1] = k2;
  keys.keys[2] = k3;

  switch (enc)
    {
    case DES_ENCRYPT:
      nettle_cbc_encrypt(&keys, (nettle_cipher_func *) des_compat_des3_encrypt,
			 DES_BLOCK_SIZE, *iv,
			 length, *dst, *src);
      break;
    case DES_DECRYPT:
      nettle_cbc_decrypt(&keys, (nettle_cipher_func *) des_compat_des3_decrypt,
			 DES_BLOCK_SIZE, *iv,
			 length, *dst, *src);
      break;
    default:
      abort();
    }
}

int
des_set_odd_parity(des_cblock *key)
{
  nettle_des_fix_parity(DES_KEY_SIZE, *key, *key);

  /* FIXME: What to return? */
  return 0;
}


/* If des_check_key is non-zero, returns
 *
 *   0 for ok, -1 for bad parity, and -2 for weak keys.
 *
 * If des_check_key is zero (the default), always returns zero.
 */

int des_check_key = 0;

int
des_key_sched(const_des_cblock *key, des_key_schedule ctx)
{
  if (des_check_key && !des_check_parity (DES_KEY_SIZE, *key))
    /* Bad parity */
    return -1;
  
  if (!nettle_des_set_key(ctx, *key) && des_check_key)
    /* Weak key */
    return -2;

  return 0;
}

int
des_is_weak_key(const_des_cblock *key)
{
  struct des_ctx ctx;

  return !nettle_des_set_key(&ctx, *key);
}
