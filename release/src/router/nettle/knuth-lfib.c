/* knuth-lfib.c

   The "lagged fibonacci" pseudorandomness generator, described in
   Knuth, TAoCP, 3.6

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

/* This file includes code copied verbatim from Knuth's TAoCP.
   Technically, doing that probably requires asking for the author's
   explicit permission. I'd expect such a request to be granted, but I
   haven't asked, because I don't want to distract him from more
   important and interesting work. */



/* NOTE: This generator is totally inappropriate for cryptographic
 * applications. It is useful for generating deterministic but
 * random-looking test data, and is used by the Nettle testsuite. */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>

#include "knuth-lfib.h"

#include "macros.h"

#define KK _KNUTH_LFIB_KK
#define LL 37
#define MM (1UL << 30)
#define TT 70

void
knuth_lfib_init(struct knuth_lfib_ctx *ctx, uint32_t seed)
{
  uint32_t t,j;
  uint32_t x[2*KK - 1];
  uint32_t ss = (seed + 2) & (MM-2);

  for (j = 0; j<KK; j++)
    {
      x[j] = ss;
      ss <<= 1;  if (ss >= MM) ss -= (MM-2);
    }
  for (;j< 2*KK-1; j++)
    x[j] = 0;

  x[1]++;

  ss = seed & (MM-1);
  for (t = TT-1; t; )
    {
      for (j = KK-1; j>0; j--)
        x[j+j] = x[j];
      for (j = 2*KK-2; j > KK-LL; j-= 2)
        x[2*KK-1-j] = x[j] & ~1;
      for (j = 2*KK-2; j>=KK; j--)
        if (x[j] & 1)
          {
            x[j-(KK-LL)] = (x[j - (KK-LL)] - x[j]) & (MM-1);
            x[j-KK] = (x[j-KK] - x[j]) & (MM-1);
          }
      if (ss & 1)
        {
          for (j=KK; j>0; j--)
            x[j] = x[j-1];
          x[0] = x[KK];
          if (x[KK] & 1)
            x[LL] = (x[LL] - x[KK]) & (MM-1);
        }
      if (ss)
        ss >>= 1;
      else
        t--;
    }
  for (j=0; j<LL; j++)
    ctx->x[j+KK-LL] = x[j];
  for (; j<KK; j++)
    ctx->x[j-LL] = x[j];

  ctx->index = 0;
}     

/* Get's a single number in the range 0 ... 2^30-1 */
uint32_t
knuth_lfib_get(struct knuth_lfib_ctx *ctx)
{
  uint32_t value;
  assert(ctx->index < KK);
  
  value = ctx->x[ctx->index];
  ctx->x[ctx->index] -= ctx->x[(ctx->index + KK - LL) % KK];
  ctx->x[ctx->index] &= (MM-1);
  
  ctx->index = (ctx->index + 1) % KK;

  return value;
} 

/* NOTE: Not at all optimized. */
void
knuth_lfib_get_array(struct knuth_lfib_ctx *ctx,
		     size_t n, uint32_t *a)
{
  unsigned i;
  
  for (i = 0; i<n; i++)
    a[i] = knuth_lfib_get(ctx);
}

/* NOTE: Not at all optimized. */
void
knuth_lfib_random(struct knuth_lfib_ctx *ctx,
		  size_t n, uint8_t *dst)
{
  /* Use 24 bits from each number, xoring together some of the
     bits. */
  
  for (; n >= 3; n-=3, dst += 3)
    {
      uint32_t value = knuth_lfib_get(ctx);

      /* Xor the most significant octet (containing 6 significant bits)
       * into the lower octet. */
      value ^= (value >> 24);

      WRITE_UINT24(dst, value);
    }
  if (n)
    {
      /* We need one or two octets more */
      uint32_t value = knuth_lfib_get(ctx);
      switch (n)
	{
	case 1:
	  *dst++ = value & 0xff;
	  break;
	case 2:
	  WRITE_UINT16(dst, value);
	  break;
	default:
	  abort();
	}
    }
}
