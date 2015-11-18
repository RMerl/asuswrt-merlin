/* camellia-absorb.c

   Final key setup processing for the camellia block cipher.

   Copyright (C) 2006,2007 NTT
   (Nippon Telegraph and Telephone Corporation).

   Copyright (C) 2010 Niels Möller

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

/* For CHAR_BIT, needed by HAVE_NATIVE_64_BIT */
#include <limits.h>

#include "camellia-internal.h"

#include "macros.h"

void
_camellia_absorb(unsigned nkeys, uint64_t *dst, uint64_t *subkey)
{
  uint64_t kw2, kw4;
  uint32_t dw, tl, tr;
  unsigned i;
  
  /* At this point, the subkey array contains the subkeys as described
     in the spec, 26 for short keys and 34 for large keys. */

  /* absorb kw2 to other subkeys */
  kw2 = subkey[1];

  subkey[3] ^= kw2;
  subkey[5] ^= kw2;
  subkey[7] ^= kw2;
  for (i = 8; i < nkeys; i += 8)
    {
      /* FIXME: gcc for x86_32 is smart enough to fetch the 32 low bits
	 and xor the result into the 32 high bits, but it still generates
	 worse code than for explicit 32-bit operations. */
      kw2 ^= (kw2 & ~subkey[i+1]) << 32;
      dw = (kw2 & subkey[i+1]) >> 32; kw2 ^= ROTL32(1, dw); 

      subkey[i+3] ^= kw2;
      subkey[i+5] ^= kw2;
      subkey[i+7] ^= kw2;
    }
  subkey[i] ^= kw2;
  
  /* absorb kw4 to other subkeys */  
  kw4 = subkey[nkeys + 1];

  for (i = nkeys - 8; i > 0; i -= 8)
    {
      subkey[i+6] ^= kw4;
      subkey[i+4] ^= kw4;
      subkey[i+2] ^= kw4;
      kw4 ^= (kw4 & ~subkey[i]) << 32;
      dw = (kw4 & subkey[i]) >> 32; kw4 ^= ROTL32(1, dw);      
    }

  subkey[6] ^= kw4;
  subkey[4] ^= kw4;
  subkey[2] ^= kw4;
  subkey[0] ^= kw4;

  /* key XOR is end of F-function */
  dst[0] = subkey[0] ^ subkey[2];
  dst[1] = subkey[3];

  dst[2] = subkey[2] ^ subkey[4];
  dst[3] = subkey[3] ^ subkey[5];
  dst[4] = subkey[4] ^ subkey[6];
  dst[5] = subkey[5] ^ subkey[7];

  for (i = 8; i < nkeys; i += 8)
    {
      tl = (subkey[i+2] >> 32) ^ (subkey[i+2] & ~subkey[i]);
      dw = tl & (subkey[i] >> 32);
      tr = subkey[i+2] ^ ROTL32(1, dw);
      dst[i-2] = subkey[i-2] ^ ( ((uint64_t) tl << 32) | tr);

      dst[i-1] = subkey[i];
      dst[i] = subkey[i+1];

      tl = (subkey[i-1] >> 32) ^ (subkey[i-1] & ~subkey[i+1]);
      dw = tl & (subkey[i+1] >> 32);
      tr = subkey[i-1] ^ ROTL32(1, dw);
      dst[i+1] = subkey[i+3] ^ ( ((uint64_t) tl << 32) | tr);

      dst[i+2] = subkey[i+2] ^ subkey[i+4];
      dst[i+3] = subkey[i+3] ^ subkey[i+5];
      dst[i+4] = subkey[i+4] ^ subkey[i+6];
      dst[i+5] = subkey[i+5] ^ subkey[i+7];
    }
  dst[i-2] = subkey[i-2];
  dst[i-1] = subkey[i] ^ subkey[i-1];

#if !HAVE_NATIVE_64_BIT
  for (i = 0; i < nkeys; i += 8)
    {
      /* apply the inverse of the last half of F-function */
      CAMELLIA_F_HALF_INV(dst[i+1]);
      CAMELLIA_F_HALF_INV(dst[i+2]);
      CAMELLIA_F_HALF_INV(dst[i+3]);
      CAMELLIA_F_HALF_INV(dst[i+4]);
      CAMELLIA_F_HALF_INV(dst[i+5]);
      CAMELLIA_F_HALF_INV(dst[i+6]);
    }
#endif
  
}
