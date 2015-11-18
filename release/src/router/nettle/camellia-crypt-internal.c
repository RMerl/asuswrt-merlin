/* camellia-crypt-internal.c

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

#include <assert.h>
#include <limits.h>

#include "camellia-internal.h"

#include "macros.h"

#define CAMELLIA_FL(x, k) do {			\
  uint32_t __xl, __xr, __kl, __kr, __t;		\
  __xl = (x) >> 32;				\
  __xr = (x) & 0xffffffff;			\
  __kl = (k) >> 32;				\
  __kr = (k) & 0xffffffff;			\
  __t = __xl & __kl;				\
  __xr ^= ROTL32(1, __t);			\
  __xl ^= (__xr | __kr);			\
  (x) = ((uint64_t) __xl << 32) | __xr;		\
} while (0)

#define CAMELLIA_FLINV(x, k) do {		\
  uint32_t __xl, __xr, __kl, __kr, __t;		\
  __xl = (x) >> 32;				\
  __xr = (x) & 0xffffffff;			\
  __kl = (k) >> 32;				\
  __kr = (k) & 0xffffffff;			\
  __xl ^= (__xr | __kr);			\
  __t = __xl & __kl;				\
  __xr ^= ROTL32(1, __t);			\
  (x) = ((uint64_t) __xl << 32) | __xr;		\
} while (0)

#if HAVE_NATIVE_64_BIT
#define CAMELLIA_ROUNDSM(T, x, k, y) do {			\
    uint32_t __il, __ir;					\
    __ir							\
      = T->sp1110[(x) & 0xff]					\
      ^ T->sp0222[((x) >> 24) & 0xff]				\
      ^ T->sp3033[((x) >> 16) & 0xff]				\
      ^ T->sp4404[((x) >> 8) & 0xff];				\
    /* ir == (t6^t7^t8),(t5^t7^t8),(t5^t6^t8),(t5^t6^t7) */	\
    __il							\
      = T->sp1110[ (x) >> 56]					\
      ^ T->sp0222[((x) >> 48) & 0xff]				\
      ^ T->sp3033[((x) >> 40) & 0xff]				\
      ^ T->sp4404[((x) >> 32) & 0xff];				\
    /* il == (t1^t3^t4),(t1^t2^t4),(t1^t2^t3),(t2^t3^t4) */	\
    __ir ^= __il;						\
    /* ir == (t1^t3^t4^t6^t7^t8),(t1^t2^t4^t5^t7^t8),		\
       (t1^t2^t3^t5^t6^t8),(t2^t3^t4^t5^t6^t7)			\
       == y1,y2,y3,y4 */					\
    __il = ROTL32(24, __il);					\
    /* il == (t2^t3^t4),(t1^t3^t4),(t1^t2^t4),(t1^t2^t3) */	\
    __il ^= __ir;						\
    /* il == (t1^t2^t6^t7^t8),(t2^t3^t5^t7^t8),			\
       (t3^t4^t5^t6^t8),(t1^t4^t5^t6^t7)			\
       == y5,y6,y7,y8 */					\
    y ^= (k);							\
    y ^= ((uint64_t) __ir << 32) | __il;			\
  } while (0)
#else /* !HAVE_NATIVE_64_BIT */
#define CAMELLIA_ROUNDSM(T, x, k, y) do {			\
    uint32_t __il, __ir;					\
    __ir							\
      = T->sp1110[(x) & 0xff]					\
      ^ T->sp0222[((x) >> 24) & 0xff]				\
      ^ T->sp3033[((x) >> 16) & 0xff]				\
      ^ T->sp4404[((x) >> 8) & 0xff];				\
    /* ir == (t6^t7^t8),(t5^t7^t8),(t5^t6^t8),(t5^t6^t7) */	\
    __il							\
      = T->sp1110[ (x) >> 56]					\
      ^ T->sp0222[((x) >> 48) & 0xff]				\
      ^ T->sp3033[((x) >> 40) & 0xff]				\
      ^ T->sp4404[((x) >> 32) & 0xff];				\
    /* il == (t1^t3^t4),(t1^t2^t4),(t1^t2^t3),(t2^t3^t4) */	\
    __il ^= (k) >> 32;						\
    __ir ^= (k) & 0xffffffff;					\
    __ir ^= __il;						\
    /* ir == (t1^t3^t4^t6^t7^t8),(t1^t2^t4^t5^t7^t8),		\
       (t1^t2^t3^t5^t6^t8),(t2^t3^t4^t5^t6^t7)			\
       == y1,y2,y3,y4 */					\
    __il = ROTL32(24, __il);					\
    /* il == (t2^t3^t4),(t1^t3^t4),(t1^t2^t4),(t1^t2^t3) */	\
    __il ^= __ir;						\
    /* il == (t1^t2^t6^t7^t8),(t2^t3^t5^t7^t8),			\
       (t3^t4^t5^t6^t8),(t1^t4^t5^t6^t7)			\
       == y5,y6,y7,y8 */					\
    y ^= ((uint64_t) __ir << 32) | __il;			\
  } while (0)
#endif

void
_camellia_crypt(unsigned nkeys,
		const uint64_t *keys,
		const struct camellia_table *T,
		size_t length, uint8_t *dst,
		const uint8_t *src)
{
  FOR_BLOCKS(length, dst, src, CAMELLIA_BLOCK_SIZE)
    {
      uint64_t i0,i1;
      unsigned i;

      i0 = READ_UINT64(src);
      i1 = READ_UINT64(src +  8);
      
      /* pre whitening but absorb kw2*/
      i0 ^= keys[0];

      /* main iteration */

      CAMELLIA_ROUNDSM(T, i0, keys[1], i1);
      CAMELLIA_ROUNDSM(T, i1, keys[2], i0);
      CAMELLIA_ROUNDSM(T, i0, keys[3], i1);
      CAMELLIA_ROUNDSM(T, i1, keys[4], i0);
      CAMELLIA_ROUNDSM(T, i0, keys[5], i1);
      CAMELLIA_ROUNDSM(T, i1, keys[6], i0);
      
      for (i = 0; i < nkeys - 8; i+= 8)
	{
	  CAMELLIA_FL(i0, keys[i+7]);
	  CAMELLIA_FLINV(i1, keys[i+8]);
	  
	  CAMELLIA_ROUNDSM(T, i0, keys[i+9], i1);
	  CAMELLIA_ROUNDSM(T, i1, keys[i+10], i0);
	  CAMELLIA_ROUNDSM(T, i0, keys[i+11], i1);
	  CAMELLIA_ROUNDSM(T, i1, keys[i+12], i0);
	  CAMELLIA_ROUNDSM(T, i0, keys[i+13], i1);
	  CAMELLIA_ROUNDSM(T, i1, keys[i+14], i0);
	}

      /* post whitening but kw4 */
      i1 ^= keys[i+7];

      WRITE_UINT64(dst     , i1);
      WRITE_UINT64(dst +  8, i0);
    }
}
