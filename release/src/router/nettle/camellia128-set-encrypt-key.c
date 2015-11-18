/* camellia128-set-encrypt-key.c

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

void
camellia128_set_encrypt_key (struct camellia128_ctx *ctx,
			     const uint8_t *key)
{
  uint64_t k0, k1;

  uint64_t subkey[_CAMELLIA128_NKEYS + 2];
  uint64_t w;

  k0 = READ_UINT64(key);
  k1 = READ_UINT64(key +  8);
  
  /**
   * generate KL dependent subkeys
   */
  subkey[0] = k0; subkey[1] = k1;
  ROTL128(15, k0, k1);
  subkey[4] = k0; subkey[5] = k1;
  ROTL128(30, k0, k1);
  subkey[10] = k0; subkey[11] = k1;
  ROTL128(15, k0, k1);
  subkey[13] = k1;
  ROTL128(17, k0, k1);
  subkey[16] = k0; subkey[17] = k1;
  ROTL128(17, k0, k1);
  subkey[18] = k0; subkey[19] = k1;
  ROTL128(17, k0, k1);
  subkey[22] = k0; subkey[23] = k1;

  /* generate KA. D1 is k0, d2 is k1. */
  /* FIXME: Make notation match the spec better. */
  /* For the 128-bit case, KR = 0, the construction of KA reduces to:

     D1 = KL >> 64;
     W = KL & MASK64;
     D2 = F(D1, Sigma1);
     W = D2 ^ W
     D1 = F(W, Sigma2)
     D2 = D2 ^ F(D1, Sigma3);
     D1 = D1 ^ F(D2, Sigma4);
     KA = (D1 << 64) | D2;
  */
  k0 = subkey[0]; w = subkey[1];
  CAMELLIA_F(k0, SIGMA1, k1);
  w ^= k1;
  CAMELLIA_F(w, SIGMA2, k0);
  CAMELLIA_F(k0, SIGMA3, w);
  k1 ^= w;
  CAMELLIA_F(k1, SIGMA4, w);
  k0 ^= w;

  /* generate KA dependent subkeys */
  subkey[2] = k0; subkey[3] = k1;
  ROTL128(15, k0, k1);
  subkey[6] = k0; subkey[7] = k1;
  ROTL128(15, k0, k1);
  subkey[8] = k0; subkey[9] = k1;
  ROTL128(15, k0, k1);
  subkey[12] = k0;
  ROTL128(15, k0, k1);
  subkey[14] = k0; subkey[15] = k1;
  ROTL128(34, k0, k1);
  subkey[20] = k0; subkey[21] = k1;
  ROTL128(17, k0, k1);
  subkey[24] = k0; subkey[25] = k1;

  /* Common final processing */
  _camellia_absorb (_CAMELLIA128_NKEYS, ctx->keys, subkey);
}
