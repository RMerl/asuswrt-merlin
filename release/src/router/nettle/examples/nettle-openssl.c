/* nettle-openssl.c

   Glue that's used only by the benchmark, and subject to change.

   Copyright (C) 2002 Niels Möller

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

/* Openssl glue, for comparative benchmarking only */

#if WITH_OPENSSL

/* No ancient ssleay compatibility */
#define NCOMPAT
#define OPENSSL_DISABLE_OLD_DES_SUPPORT

#include <assert.h>

#include <openssl/aes.h>
#include <openssl/blowfish.h>
#include <openssl/des.h>
#include <openssl/cast.h>
#include <openssl/rc4.h>

#include <openssl/md5.h>
#include <openssl/sha.h>

#include "nettle-internal.h"


/* AES */
static nettle_set_key_func openssl_aes128_set_encrypt_key;
static nettle_set_key_func openssl_aes128_set_decrypt_key;
static nettle_set_key_func openssl_aes192_set_encrypt_key;
static nettle_set_key_func openssl_aes192_set_decrypt_key;
static nettle_set_key_func openssl_aes256_set_encrypt_key;
static nettle_set_key_func openssl_aes256_set_decrypt_key;
static void
openssl_aes128_set_encrypt_key(void *ctx, const uint8_t *key)
{
  AES_set_encrypt_key(key, 128, ctx);
}
static void
openssl_aes128_set_decrypt_key(void *ctx, const uint8_t *key)
{
  AES_set_decrypt_key(key, 128, ctx);
}

static void
openssl_aes192_set_encrypt_key(void *ctx, const uint8_t *key)
{
  AES_set_encrypt_key(key, 192, ctx);
}
static void
openssl_aes192_set_decrypt_key(void *ctx, const uint8_t *key)
{
  AES_set_decrypt_key(key, 192, ctx);
}

static void
openssl_aes256_set_encrypt_key(void *ctx, const uint8_t *key)
{
  AES_set_encrypt_key(key, 256, ctx);
}
static void
openssl_aes256_set_decrypt_key(void *ctx, const uint8_t *key)
{
  AES_set_decrypt_key(key, 256, ctx);
}

static nettle_cipher_func openssl_aes_encrypt;
static void
openssl_aes_encrypt(const void *ctx, size_t length,
		    uint8_t *dst, const uint8_t *src)
{
  assert (!(length % AES_BLOCK_SIZE));
  while (length)
    {
      AES_ecb_encrypt(src, dst, ctx, AES_ENCRYPT);
      length -= AES_BLOCK_SIZE;
      dst += AES_BLOCK_SIZE;
      src += AES_BLOCK_SIZE;
    }
}

static nettle_cipher_func openssl_aes_decrypt;
static void
openssl_aes_decrypt(const void *ctx, size_t length,
		    uint8_t *dst, const uint8_t *src)
{
  assert (!(length % AES_BLOCK_SIZE));
  while (length)
    {
      AES_ecb_encrypt(src, dst, ctx, AES_DECRYPT);
      length -= AES_BLOCK_SIZE;
      dst += AES_BLOCK_SIZE;
      src += AES_BLOCK_SIZE;
    }
}

const struct nettle_cipher
nettle_openssl_aes128 = {
  "openssl aes128", sizeof(AES_KEY),
  16, 16,
  openssl_aes128_set_encrypt_key, openssl_aes128_set_decrypt_key,
  openssl_aes_encrypt, openssl_aes_decrypt
};

const struct nettle_cipher
nettle_openssl_aes192 = {
  "openssl aes192", sizeof(AES_KEY),
  /* Claim no block size, so that the benchmark doesn't try CBC mode
   * (as openssl cipher + nettle cbc is somewhat pointless to
   * benchmark). */
  16, 24,
  openssl_aes192_set_encrypt_key, openssl_aes192_set_decrypt_key,
  openssl_aes_encrypt, openssl_aes_decrypt
};

const struct nettle_cipher
nettle_openssl_aes256 = {
  "openssl aes256", sizeof(AES_KEY),
  /* Claim no block size, so that the benchmark doesn't try CBC mode
   * (as openssl cipher + nettle cbc is somewhat pointless to
   * benchmark). */
  16, 32,
  openssl_aes256_set_encrypt_key, openssl_aes256_set_decrypt_key,
  openssl_aes_encrypt, openssl_aes_decrypt
};

/* Arcfour */
static nettle_set_key_func openssl_arcfour128_set_key;
static void
openssl_arcfour128_set_key(void *ctx, const uint8_t *key)
{
  RC4_set_key(ctx, 16, key);
}

static nettle_crypt_func openssl_arcfour_crypt;
static void
openssl_arcfour_crypt(void *ctx, size_t length,
		      uint8_t *dst, const uint8_t *src)
{
  RC4(ctx, length, src, dst);
}

const struct nettle_aead
nettle_openssl_arcfour128 = {
  "openssl arcfour128", sizeof(RC4_KEY),
  1, 16, 0, 0,
  openssl_arcfour128_set_key,
  openssl_arcfour128_set_key,
  NULL, NULL,
  openssl_arcfour_crypt,
  openssl_arcfour_crypt,
  NULL,  
};

/* Blowfish */
static nettle_set_key_func openssl_bf128_set_key;
static void
openssl_bf128_set_key(void *ctx, const uint8_t *key)
{
  BF_set_key(ctx, 16, key);
}

static nettle_cipher_func openssl_bf_encrypt;
static void
openssl_bf_encrypt(const void *ctx, size_t length,
		   uint8_t *dst, const uint8_t *src)
{
  assert (!(length % BF_BLOCK));
  while (length)
    {
      BF_ecb_encrypt(src, dst, ctx, BF_ENCRYPT);
      length -= BF_BLOCK;
      dst += BF_BLOCK;
      src += BF_BLOCK;
    }
}

static nettle_cipher_func openssl_bf_decrypt;
static void
openssl_bf_decrypt(const void *ctx, size_t length,
		   uint8_t *dst, const uint8_t *src)
{
  assert (!(length % BF_BLOCK));
  while (length)
    {
      BF_ecb_encrypt(src, dst, ctx, BF_DECRYPT);
      length -= BF_BLOCK;
      dst += BF_BLOCK;
      src += BF_BLOCK;
    }
}

const struct nettle_cipher
nettle_openssl_blowfish128 = {
  "openssl bf128", sizeof(BF_KEY),
  8, 16,
  openssl_bf128_set_key, openssl_bf128_set_key,
  openssl_bf_encrypt, openssl_bf_decrypt
};


/* DES */
static nettle_set_key_func openssl_des_set_key;
static void
openssl_des_set_key(void *ctx, const uint8_t *key)
{
  /* Not sure what "unchecked" means. We want to ignore parity bits,
     but it would still make sense to check for weak keys. */
  /* Explicit cast used as I don't want to care about openssl's broken
     array typedefs DES_cblock and const_DES_cblock. */
  DES_set_key_unchecked( (void *) key, ctx);
}

#define DES_BLOCK_SIZE 8

static nettle_cipher_func openssl_des_encrypt;
static void
openssl_des_encrypt(const void *ctx, size_t length,
		    uint8_t *dst, const uint8_t *src)
{
  assert (!(length % DES_BLOCK_SIZE));
  while (length)
    {
      DES_ecb_encrypt((void *) src, (void *) dst,
		      (void *) ctx, DES_ENCRYPT);
      length -= DES_BLOCK_SIZE;
      dst += DES_BLOCK_SIZE;
      src += DES_BLOCK_SIZE;
    }
}

static nettle_cipher_func openssl_des_decrypt;
static void
openssl_des_decrypt(const void *ctx, size_t length,
		    uint8_t *dst, const uint8_t *src)
{
  assert (!(length % DES_BLOCK_SIZE));
  while (length)
    {
      DES_ecb_encrypt((void *) src, (void *) dst,
		      (void *) ctx, DES_DECRYPT);
      length -= DES_BLOCK_SIZE;
      dst += DES_BLOCK_SIZE;
      src += DES_BLOCK_SIZE;
    }
}

const struct nettle_cipher
nettle_openssl_des = {
  "openssl des", sizeof(DES_key_schedule),
  8, 8,
  openssl_des_set_key, openssl_des_set_key,
  openssl_des_encrypt, openssl_des_decrypt
};


/* Cast128 */
static nettle_set_key_func openssl_cast128_set_key;
static void
openssl_cast128_set_key(void *ctx, const uint8_t *key)
{
  CAST_set_key(ctx, 16, key);
}

static nettle_cipher_func openssl_cast_encrypt;
static void
openssl_cast_encrypt(const void *ctx, size_t length,
		     uint8_t *dst, const uint8_t *src)
{
  assert (!(length % CAST_BLOCK));
  while (length)
    {
      CAST_ecb_encrypt(src, dst, ctx, CAST_ENCRYPT);
      length -= CAST_BLOCK;
      dst += CAST_BLOCK;
      src += CAST_BLOCK;
    }
}

static nettle_cipher_func openssl_cast_decrypt;
static void
openssl_cast_decrypt(const void *ctx, size_t length,
		     uint8_t *dst, const uint8_t *src)
{
  assert (!(length % CAST_BLOCK));
  while (length)
    {
      CAST_ecb_encrypt(src, dst, ctx, CAST_DECRYPT);
      length -= CAST_BLOCK;
      dst += CAST_BLOCK;
      src += CAST_BLOCK;
    }
}

const struct nettle_cipher
nettle_openssl_cast128 = {
  "openssl cast128", sizeof(CAST_KEY),
  8, CAST_KEY_LENGTH,
  openssl_cast128_set_key, openssl_cast128_set_key,
  openssl_cast_encrypt, openssl_cast_decrypt
};

/* Hash functions */

/* md5 */
static nettle_hash_init_func openssl_md5_init;
static void
openssl_md5_init(void *ctx)
{
  MD5_Init(ctx);
}

static nettle_hash_update_func openssl_md5_update;
static void
openssl_md5_update(void *ctx,
		   size_t length,
		   const uint8_t *src)
{
  MD5_Update(ctx, src, length);
}

static nettle_hash_digest_func openssl_md5_digest;
static void
openssl_md5_digest(void *ctx,
		   size_t length, uint8_t *dst)
{
  assert(length == SHA_DIGEST_LENGTH);
  MD5_Final(dst, ctx);
  MD5_Init(ctx);
}

const struct nettle_hash
nettle_openssl_md5 = {
  "openssl md5", sizeof(SHA_CTX),
  SHA_DIGEST_LENGTH, SHA_CBLOCK,
  openssl_md5_init,
  openssl_md5_update,
  openssl_md5_digest
};

/* sha1 */
static nettle_hash_init_func openssl_sha1_init;
static void
openssl_sha1_init(void *ctx)
{
  SHA1_Init(ctx);
}

static nettle_hash_update_func openssl_sha1_update;
static void
openssl_sha1_update(void *ctx,
		    size_t length,
		    const uint8_t *src)
{
  SHA1_Update(ctx, src, length);
}

static nettle_hash_digest_func openssl_sha1_digest;
static void
openssl_sha1_digest(void *ctx,
		    size_t length, uint8_t *dst)
{
  assert(length == SHA_DIGEST_LENGTH);
  SHA1_Final(dst, ctx);
  SHA1_Init(ctx);
}

const struct nettle_hash
nettle_openssl_sha1 = {
  "openssl sha1", sizeof(SHA_CTX),
  SHA_DIGEST_LENGTH, SHA_CBLOCK,
  openssl_sha1_init,
  openssl_sha1_update,
  openssl_sha1_digest
};
  
#endif /* WITH_OPENSSL */
