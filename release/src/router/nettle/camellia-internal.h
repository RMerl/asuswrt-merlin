/* camellia-internal.h

   The camellia block cipher.

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

#ifndef NETTLE_CAMELLIA_INTERNAL_H_INCLUDED
#define NETTLE_CAMELLIA_INTERNAL_H_INCLUDED

#include "camellia.h"

/* Name mangling */
#define _camellia_crypt _nettle_camellia_crypt
#define _camellia_absorb _nettle_camellia_absorb
#define _camellia_invert_key _nettle_camellia_invert_key
#define _camellia_table _nettle_camellia_table

/*
 *  macros
 */

/* Destructive rotation of 128 bit values. */
#define ROTL128(bits, xl, xr) do {		\
    uint64_t __rol128_t = (xl);			     \
    (xl) = ((xl) << (bits)) | ((xr) >> (64 - (bits)));	   \
    (xr) = ((xr) << (bits)) | (__rol128_t >> (64 - (bits)));	\
  } while (0)

struct camellia_table
{
  uint32_t sp1110[256];
  uint32_t sp0222[256];
  uint32_t sp3033[256];
  uint32_t sp4404[256];
};

/* key constants */

#define SIGMA1 0xA09E667F3BCC908BULL
#define SIGMA2 0xB67AE8584CAA73B2ULL
#define SIGMA3 0xC6EF372FE94F82BEULL
#define SIGMA4 0x54FF53A5F1D36F1CULL
#define SIGMA5 0x10E527FADE682D1DULL
#define SIGMA6 0xB05688C2B3E6C1FDULL

#define CAMELLIA_SP1110(INDEX) (_nettle_camellia_table.sp1110[(int)(INDEX)])
#define CAMELLIA_SP0222(INDEX) (_nettle_camellia_table.sp0222[(int)(INDEX)])
#define CAMELLIA_SP3033(INDEX) (_nettle_camellia_table.sp3033[(int)(INDEX)])
#define CAMELLIA_SP4404(INDEX) (_nettle_camellia_table.sp4404[(int)(INDEX)])

#define CAMELLIA_F(x, k, y) do {		\
    uint32_t __yl, __yr;			\
    uint64_t __i = (x) ^ (k);			\
    __yl					\
      = CAMELLIA_SP1110( __i & 0xff)		\
      ^ CAMELLIA_SP0222((__i >> 24) & 0xff)	\
      ^ CAMELLIA_SP3033((__i >> 16) & 0xff)	\
      ^ CAMELLIA_SP4404((__i >> 8) & 0xff);	\
    __yr					\
      = CAMELLIA_SP1110( __i >> 56)		\
      ^ CAMELLIA_SP0222((__i >> 48) & 0xff)	\
      ^ CAMELLIA_SP3033((__i >> 40) & 0xff)	\
      ^ CAMELLIA_SP4404((__i >> 32) & 0xff);	\
    __yl ^= __yr;				\
    __yr = ROTL32(24, __yr);			\
    __yr ^= __yl;				\
    (y) = ((uint64_t) __yl << 32) | __yr;	\
  } while (0)

#if ! HAVE_NATIVE_64_BIT
#define CAMELLIA_F_HALF_INV(x) do {            \
    uint32_t __t, __w;                         \
    __t = (x) >> 32;                           \
    __w = __t ^(x);                            \
    __w = ROTL32(8, __w);                       \
    (x) = ((uint64_t) __w << 32) | (__t ^ __w);        \
  } while (0)
#endif

void
_camellia_crypt(unsigned nkeys, const uint64_t *keys,
		const struct camellia_table *T,
		size_t length, uint8_t *dst,
		const uint8_t *src);

/* The initial NKEYS + 2 subkeys in SUBKEY are reduced to the final
   NKEYS subkeys stored in DST. SUBKEY data is modified in the
   process. */
void
_camellia_absorb(unsigned nkeys, uint64_t *dst, uint64_t *subkey);

void
_camellia_invert_key(unsigned nkeys,
		     uint64_t *dst, const uint64_t *src);

extern const struct camellia_table _camellia_table;

#endif /* NETTLE_CAMELLIA_INTERNAL_H_INCLUDED */
