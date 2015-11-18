/* ccm.c

   Counter with CBC-MAC mode, specified by NIST,
   http://csrc.nist.gov/publications/nistpubs/800-38C/SP800-38C_updated-July20_2007.pdf

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
#include <stdlib.h>
#include <string.h>

#include "ccm.h"
#include "ctr.h"

#include "memxor.h"
#include "nettle-internal.h"
#include "macros.h"

/*
 * The format of the CCM IV (for both CTR and CBC-MAC) is: flags | nonce | count
 *  flags = 1 octet
 *  nonce = N octets
 *  count >= 1 octet
 *
 * such that:
 *  sizeof(flags) + sizeof(nonce) + sizeof(count) == 1 block
 */
#define CCM_FLAG_L          0x07
#define CCM_FLAG_M          0x38
#define CCM_FLAG_ADATA      0x40
#define CCM_FLAG_RESERVED   0x80
#define CCM_FLAG_GET_L(_x_) (((_x_) & CCM_FLAG_L) + 1)
#define CCM_FLAG_SET_L(_x_) (((_x_) - 1) & CCM_FLAG_L)
#define CCM_FLAG_SET_M(_x_) ((((_x_) - 2) << 2) & CCM_FLAG_M)

#define CCM_OFFSET_FLAGS    0
#define CCM_OFFSET_NONCE    1
#define CCM_L_SIZE(_nlen_)  (CCM_BLOCK_SIZE - CCM_OFFSET_NONCE - (_nlen_))

/*
 * The data input to the CBC-MAC: L(a) | adata | padding | plaintext | padding
 *
 * blength is the length of data that has been added to the CBC-MAC modulus the
 * cipher block size. If the value of blength is non-zero then some data has
 * been XOR'ed into the CBC-MAC, and we will need to pad the block (XOR with 0),
 * and iterate the cipher one more time.
 *
 * The end of adata is detected implicitly by the first call to the encrypt()
 * and decrypt() functions, and will call ccm_pad() to insert the padding if
 * necessary. Because of the underlying CTR encryption, the encrypt() and
 * decrypt() functions must be called with a multiple of the block size and
 * therefore blength should be zero on all but the first call.
 *
 * Likewise, the end of the plaintext is implicitly determined by the first call
 * to the digest() function, which will pad if the final CTR encryption was not
 * a multiple of the block size.
 */
static void
ccm_pad(struct ccm_ctx *ctx, const void *cipher, nettle_cipher_func *f)
{
    if (ctx->blength) f(cipher, CCM_BLOCK_SIZE, ctx->tag.b, ctx->tag.b);
    ctx->blength = 0;
}

static void
ccm_build_iv(uint8_t *iv, size_t noncelen, const uint8_t *nonce,
	     uint8_t flags, size_t count)
{
  unsigned int i;

  /* Sanity check the nonce length. */
  assert(noncelen >= CCM_MIN_NONCE_SIZE);
  assert(noncelen <= CCM_MAX_NONCE_SIZE);

  /* Generate the IV */
  iv[CCM_OFFSET_FLAGS] = flags | CCM_FLAG_SET_L(CCM_L_SIZE(noncelen));
  memcpy(&iv[CCM_OFFSET_NONCE], nonce, noncelen);
  for (i=(CCM_BLOCK_SIZE - 1); i >= (CCM_OFFSET_NONCE + noncelen); i--) {
    iv[i] = count & 0xff;
    count >>= 8;
  }

  /* Ensure the count was not truncated. */
  assert(!count);
}

void
ccm_set_nonce(struct ccm_ctx *ctx, const void *cipher, nettle_cipher_func *f,
	      size_t length, const uint8_t *nonce,
	      size_t authlen, size_t msglen, size_t taglen)
{
  /* Generate the IV for the CTR and CBC-MAC */
  ctx->blength = 0;
  ccm_build_iv(ctx->tag.b, length, nonce, CCM_FLAG_SET_M(taglen), msglen);
  ccm_build_iv(ctx->ctr.b, length, nonce, 0, 1);

  /* If no auth data, encrypt B0 and skip L(a) */
  if (!authlen) {
    f(cipher, CCM_BLOCK_SIZE, ctx->tag.b, ctx->tag.b);
    return;
  }

  /* Encrypt B0 (with the adata flag), and input L(a) to the CBC-MAC. */
  ctx->tag.b[CCM_OFFSET_FLAGS] |= CCM_FLAG_ADATA;
  f(cipher, CCM_BLOCK_SIZE, ctx->tag.b, ctx->tag.b);
#if SIZEOF_SIZE_T > 4
  if (authlen >= (0x01ULL << 32)) {
    /* Encode L(a) as 0xff || 0xff || <64-bit integer> */
    ctx->tag.b[ctx->blength++] ^= 0xff;
    ctx->tag.b[ctx->blength++] ^= 0xff;
    ctx->tag.b[ctx->blength++] ^= (authlen >> 56) & 0xff;
    ctx->tag.b[ctx->blength++] ^= (authlen >> 48) & 0xff;
    ctx->tag.b[ctx->blength++] ^= (authlen >> 40) & 0xff;
    ctx->tag.b[ctx->blength++] ^= (authlen >> 32) & 0xff;
    ctx->tag.b[ctx->blength++] ^= (authlen >> 24) & 0xff;
    ctx->tag.b[ctx->blength++] ^= (authlen >> 16) & 0xff;
  }
  else
#endif
    if (authlen >= ((0x1ULL << 16) - (0x1ULL << 8))) {
      /* Encode L(a) as 0xff || 0xfe || <32-bit integer> */
      ctx->tag.b[ctx->blength++] ^= 0xff;
      ctx->tag.b[ctx->blength++] ^= 0xfe;
      ctx->tag.b[ctx->blength++] ^= (authlen >> 24) & 0xff;
      ctx->tag.b[ctx->blength++] ^= (authlen >> 16) & 0xff;
    }
  ctx->tag.b[ctx->blength++] ^= (authlen >> 8) & 0xff;
  ctx->tag.b[ctx->blength++] ^= (authlen >> 0) & 0xff;
}

void
ccm_update(struct ccm_ctx *ctx, const void *cipher, nettle_cipher_func *f,
	   size_t length, const uint8_t *data)
{
  const uint8_t *end = data + length;

  /* If we don't have enough to fill a block, save the data for later. */
  if ((ctx->blength + length) < CCM_BLOCK_SIZE) {
    memxor(&ctx->tag.b[ctx->blength], data, length);
    ctx->blength += length;
    return;
  }

  /* Process a partially filled block. */
  if (ctx->blength) {
    memxor(&ctx->tag.b[ctx->blength], data, CCM_BLOCK_SIZE - ctx->blength);
    data += (CCM_BLOCK_SIZE - ctx->blength);
    f(cipher, CCM_BLOCK_SIZE, ctx->tag.b, ctx->tag.b);
  }

  /* Process full blocks. */
  while ((data + CCM_BLOCK_SIZE) < end) {
    memxor(ctx->tag.b, data, CCM_BLOCK_SIZE);
    f(cipher, CCM_BLOCK_SIZE, ctx->tag.b, ctx->tag.b);
    data += CCM_BLOCK_SIZE;
  } /* while */

  /* Save leftovers for later. */
  ctx->blength = (end - data);
  if (ctx->blength) memxor(&ctx->tag.b, data, ctx->blength);
}

/*
 * Because of the underlying CTR mode encryption, when called multiple times
 * the data in intermediate calls must be provided in multiples of the block
 * size.
 */
void
ccm_encrypt(struct ccm_ctx *ctx, const void *cipher, nettle_cipher_func *f,
	    size_t length, uint8_t *dst, const uint8_t *src)
{
  ccm_pad(ctx, cipher, f);
  ccm_update(ctx, cipher, f, length, src);
  ctr_crypt(cipher, f, CCM_BLOCK_SIZE, ctx->ctr.b, length, dst, src);
}

/*
 * Because of the underlying CTR mode decryption, when called multiple times
 * the data in intermediate calls must be provided in multiples of the block
 * size.
 */
void
ccm_decrypt(struct ccm_ctx *ctx, const void *cipher, nettle_cipher_func *f,
	    size_t length, uint8_t *dst, const uint8_t *src)
{
  ctr_crypt(cipher, f, CCM_BLOCK_SIZE, ctx->ctr.b, length, dst, src);
  ccm_pad(ctx, cipher, f);
  ccm_update(ctx, cipher, f, length, dst);
}

void
ccm_digest(struct ccm_ctx *ctx, const void *cipher, nettle_cipher_func *f,
	   size_t length, uint8_t *digest)
{
  int i = CCM_BLOCK_SIZE - CCM_FLAG_GET_L(ctx->ctr.b[CCM_OFFSET_FLAGS]);
  assert(length <= CCM_BLOCK_SIZE);
  while (i < CCM_BLOCK_SIZE)  ctx->ctr.b[i++] = 0;
  ccm_pad(ctx, cipher, f);
  ctr_crypt(cipher, f, CCM_BLOCK_SIZE, ctx->ctr.b, length, digest, ctx->tag.b);
}

void
ccm_encrypt_message(const void *cipher, nettle_cipher_func *f,
		    size_t nlength, const uint8_t *nonce,
		    size_t alength, const uint8_t *adata, size_t tlength,
		    size_t clength, uint8_t *dst, const uint8_t *src)
{
  struct ccm_ctx ctx;
  uint8_t *tag = dst + (clength-tlength);
  assert(clength >= tlength);
  ccm_set_nonce(&ctx, cipher, f, nlength, nonce, alength, clength-tlength, tlength);
  ccm_update(&ctx, cipher, f, alength, adata);
  ccm_encrypt(&ctx, cipher, f, clength-tlength, dst, src);
  ccm_digest(&ctx, cipher, f, tlength, tag);
}

/* FIXME: Should be made public, under some suitable name. */
static int
memeql_sec (const void *a, const void *b, size_t n)
{
  volatile const unsigned char *ap = (const unsigned char *) a;
  volatile const unsigned char *bp = (const unsigned char *) b;
  volatile unsigned char d;
  size_t i;
  for (d = i = 0; i < n; i++)
    d |= (ap[i] ^ bp[i]);
  return d == 0;
}

int
ccm_decrypt_message(const void *cipher, nettle_cipher_func *f,
		    size_t nlength, const uint8_t *nonce,
		    size_t alength, const uint8_t *adata, size_t tlength,
		    size_t mlength, uint8_t *dst, const uint8_t *src)
{
  struct ccm_ctx ctx;
  uint8_t tag[CCM_BLOCK_SIZE];
  ccm_set_nonce(&ctx, cipher, f, nlength, nonce, alength, mlength, tlength);
  ccm_update(&ctx, cipher, f, alength, adata);
  ccm_decrypt(&ctx, cipher, f, mlength, dst, src);
  ccm_digest(&ctx, cipher, f, tlength, tag);
  return memeql_sec(tag, src + mlength, tlength);
}
