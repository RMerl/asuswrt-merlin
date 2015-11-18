/* eax.c

   EAX mode, see http://www.cs.ucdavis.edu/~rogaway/papers/eax.pdf

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

#include "eax.h"

#include "ctr.h"
#include "memxor.h"

static void
omac_init (union nettle_block16 *state, unsigned t)
{
  memset (state->b, 0, EAX_BLOCK_SIZE - 1);
  state->b[EAX_BLOCK_SIZE - 1] = t;
}

/* Almost the same as gcm_gf_add */
static void
block16_xor (union nettle_block16 *dst, const union nettle_block16 *src)
{
  dst->w[0] ^= src->w[0];
  dst->w[1] ^= src->w[1];
#if SIZEOF_LONG == 4
  dst->w[2] ^= src->w[2];
  dst->w[3] ^= src->w[3];
#endif
}

static void
omac_update (union nettle_block16 *state, const struct eax_key *key,
	     const void *cipher, nettle_cipher_func *f,
	     size_t length, const uint8_t *data)
{
  for (; length >= EAX_BLOCK_SIZE;
       length -= EAX_BLOCK_SIZE, data += EAX_BLOCK_SIZE)
    {
      f (cipher, EAX_BLOCK_SIZE, state->b, state->b);
      memxor (state->b, data, EAX_BLOCK_SIZE);
    }
  if (length > 0)
    {
      /* Allowed only for the last call */
      f (cipher, EAX_BLOCK_SIZE, state->b, state->b);
      memxor (state->b, data, length);
      state->b[length] ^= 0x80;
      /* XOR with (P ^ B), since the digest processing
       * unconditionally XORs with B */
      block16_xor (state, &key->pad_partial);
    }
}

static void
omac_final (union nettle_block16 *state, const struct eax_key *key,
	    const void *cipher, nettle_cipher_func *f)
{
  block16_xor (state, &key->pad_block);
  f (cipher, EAX_BLOCK_SIZE, state->b, state->b);
}

/* Allows r == a */
static void
gf2_double (uint8_t *r, const uint8_t *a)
{
  unsigned high = - (a[0] >> 7);
  unsigned i;
  /* Shift left */
  for (i = 0; i < EAX_BLOCK_SIZE - 1; i++)
    r[i] = (a[i] << 1) + (a[i+1] >> 7);

  /* Wrap around for x^{128} = x^7 + x^2 + x + 1 */
  r[EAX_BLOCK_SIZE - 1] = (a[EAX_BLOCK_SIZE - 1] << 1) ^ (high & 0x87);
}

void
eax_set_key (struct eax_key *key, const void *cipher, nettle_cipher_func *f)
{
  static const union nettle_block16 zero_block;
  f (cipher, EAX_BLOCK_SIZE, key->pad_block.b, zero_block.b);
  gf2_double (key->pad_block.b, key->pad_block.b);
  gf2_double (key->pad_partial.b, key->pad_block.b);
  block16_xor (&key->pad_partial, &key->pad_block);
}

void
eax_set_nonce (struct eax_ctx *eax, const struct eax_key *key,
	       const void *cipher, nettle_cipher_func *f,
	       size_t nonce_length, const uint8_t *nonce)
{
  omac_init (&eax->omac_nonce, 0);
  omac_update (&eax->omac_nonce, key, cipher, f, nonce_length, nonce);
  omac_final (&eax->omac_nonce, key, cipher, f);
  memcpy (eax->ctr.b, eax->omac_nonce.b, EAX_BLOCK_SIZE);

  omac_init (&eax->omac_data, 1);
  omac_init (&eax->omac_message, 2);
}

void
eax_update (struct eax_ctx *eax, const struct eax_key *key,
	    const void *cipher, nettle_cipher_func *f,
	    size_t data_length, const uint8_t *data)
{
  omac_update (&eax->omac_data, key, cipher, f, data_length, data);
}

void
eax_encrypt (struct eax_ctx *eax, const struct eax_key *key,
	     const void *cipher, nettle_cipher_func *f,
	     size_t length, uint8_t *dst, const uint8_t *src)
{
  ctr_crypt (cipher, f, EAX_BLOCK_SIZE, eax->ctr.b, length, dst, src);
  omac_update (&eax->omac_message, key, cipher, f, length, dst);
}

void
eax_decrypt (struct eax_ctx *eax, const struct eax_key *key,
	     const void *cipher, nettle_cipher_func *f,
	     size_t length, uint8_t *dst, const uint8_t *src)
{
  omac_update (&eax->omac_message, key, cipher, f, length, src);
  ctr_crypt (cipher, f, EAX_BLOCK_SIZE, eax->ctr.b, length, dst, src);
}

void
eax_digest (struct eax_ctx *eax, const struct eax_key *key,
	    const void *cipher, nettle_cipher_func *f,
	    size_t length, uint8_t *digest)
{
  assert (length > 0);
  assert (length <= EAX_BLOCK_SIZE);
  omac_final (&eax->omac_data, key, cipher, f);
  omac_final (&eax->omac_message, key, cipher, f);

  block16_xor (&eax->omac_nonce, &eax->omac_data);
  memxor3 (digest, eax->omac_nonce.b, eax->omac_message.b, length);
}
