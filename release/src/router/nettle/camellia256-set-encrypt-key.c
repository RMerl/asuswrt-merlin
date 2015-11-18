/* camellia256-set-encrypt-key.c

   Key setup for the camellia block cipher.

   Copyright (C) 2006,2007 NTT
   (Nippon Telegraph and Telephone Corporation).

   Copyright (C) 2010, 2013 Niels Möller

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

/*
 * Algorithm Specification 
 *  http://info.isl.ntt.co.jp/crypt/eng/camellia/specifications.html
 */

/* Based on camellia.c ver 1.2.0, see
   http://info.isl.ntt.co.jp/crypt/eng/camellia/dl/camellia-LGPL-1.2.0.tar.gz.
 */
#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <limits.h>

#include "camellia-internal.h"

#include "macros.h"

static void
_camellia256_set_encrypt_key (struct camellia256_ctx *ctx,
			      uint64_t k0, uint64_t k1,
			      uint64_t k2, uint64_t k3)
{
  uint64_t subkey[_CAMELLIA256_NKEYS + 2];
  uint64_t w;
  
  /* generate KL dependent subkeys */
  subkey[0] = k0; subkey[1] = k1;
  ROTL128(45, k0, k1);
  subkey[12] = k0; subkey[13] = k1;
  ROTL128(15, k0, k1);
  subkey[16] = k0; subkey[17] = k1;
  ROTL128(17, k0, k1);
  subkey[22] = k0; subkey[23] = k1;
  ROTL128(34, k0, k1);
  subkey[30] = k0; subkey[31] = k1;

  /* generate KR dependent subkeys */
  ROTL128(15, k2, k3);
  subkey[4] = k2; subkey[5] = k3;
  ROTL128(15, k2, k3);
  subkey[8] = k2; subkey[9] = k3;
  ROTL128(30, k2, k3);
  subkey[18] = k2; subkey[19] = k3;
  ROTL128(34, k2, k3);
  subkey[26] = k2; subkey[27] = k3;
  ROTL128(34, k2, k3);

  /* generate KA */
  /* The construction of KA is done as

     D1 = (KL ^ KR) >> 64
     D2 = (KL ^ KR) & MASK64
     W = F(D1, SIGMA1)
     D2 = D2 ^ W
     D1 = F(D2, SIGMA2) ^ (KR >> 64)
     D2 = F(D1, SIGMA3) ^ W ^ (KR & MASK64)
     D1 = D1 ^ F(W, SIGMA2)
     D2 = D2 ^ F(D1, SIGMA3)
     D1 = D1 ^ F(D2, SIGMA4)
  */

  k0 = subkey[0] ^ k2;
  k1 = subkey[1] ^ k3;

  CAMELLIA_F(k0, SIGMA1, w);
  k1 ^= w;

  CAMELLIA_F(k1, SIGMA2, k0);
  k0 ^= k2;

  CAMELLIA_F(k0, SIGMA3, k1);
  k1 ^= w ^ k3;

  CAMELLIA_F(k1, SIGMA4, w);
  k0 ^= w;

  /* generate KB */
  k2 ^= k0; k3 ^= k1;
  CAMELLIA_F(k2, SIGMA5, w);
  k3 ^= w;
  CAMELLIA_F(k3, SIGMA6, w);
  k2 ^= w;

  /* generate KA dependent subkeys */
  ROTL128(15, k0, k1);
  subkey[6] = k0; subkey[7] = k1;
  ROTL128(30, k0, k1);
  subkey[14] = k0; subkey[15] = k1;
  ROTL128(32, k0, k1);
  subkey[24] = k0; subkey[25] = k1;
  ROTL128(17, k0, k1);
  subkey[28] = k0; subkey[29] = k1;

  /* generate KB dependent subkeys */
  subkey[2] = k2; subkey[3] = k3;
  ROTL128(30, k2, k3);
  subkey[10] = k2; subkey[11] = k3;
  ROTL128(30, k2, k3);
  subkey[20] = k2; subkey[21] = k3;
  ROTL128(51, k2, k3);
  subkey[32] = k2; subkey[33] = k3;

  /* Common final processing */
  _camellia_absorb (_CAMELLIA256_NKEYS, ctx->keys, subkey);
}

void
camellia256_set_encrypt_key(struct camellia256_ctx *ctx,
			    const uint8_t *key)
{
  uint64_t k0, k1, k2, k3;
  k0 = READ_UINT64(key);
  k1 = READ_UINT64(key +  8);
  k2 = READ_UINT64(key + 16);
  k3 = READ_UINT64(key + 24);

  _camellia256_set_encrypt_key (ctx, k0, k1, k2, k3);
}

void
camellia192_set_encrypt_key(struct camellia256_ctx *ctx,
			    const uint8_t *key)
{
  uint64_t k0, k1, k2;
  k0 = READ_UINT64(key);
  k1 = READ_UINT64(key +  8);
  k2 = READ_UINT64(key + 16);

  _camellia256_set_encrypt_key (ctx, k0, k1, k2, ~k2);
}
