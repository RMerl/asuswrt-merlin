/* poly1305-internal.c

   Copyright: 2012-2013 Andrew M. (floodyberry)
   Copyright: 2013 Nikos Mavrogiannopoulos
   Copyright: 2013 Niels Möller

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

/* Based on https://github.com/floodyberry/poly1305-donna.
 * Modified for nettle by Nikos Mavrogiannopoulos and Niels Möller.
 * Original license notice:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <string.h>

#include "poly1305.h"

#include "macros.h"

#define mul32x32_64(a,b) ((uint64_t)(a) * (b))

#define r0 r.r32[0]
#define r1 r.r32[1]
#define r2 r.r32[2]
#define r3 r.r32[3]
#define r4 r.r32[4]
#define s1 r.r32[5]
#define s2 s32[0]
#define s3 s32[1]
#define s4 s32[2]

#define h0 h.h32[0]
#define h1 h.h32[1]
#define h2 h.h32[2]
#define h3 h.h32[3]
#define h4 hh

void
poly1305_set_key(struct poly1305_ctx *ctx, const uint8_t key[16])
{
  uint32_t t0,t1,t2,t3;

  t0 = LE_READ_UINT32(key);
  t1 = LE_READ_UINT32(key+4);
  t2 = LE_READ_UINT32(key+8);
  t3 = LE_READ_UINT32(key+12);

  ctx->r0 = t0 & 0x3ffffff; t0 >>= 26; t0 |= t1 << 6;
  ctx->r1 = t0 & 0x3ffff03; t1 >>= 20; t1 |= t2 << 12;
  ctx->r2 = t1 & 0x3ffc0ff; t2 >>= 14; t2 |= t3 << 18;
  ctx->r3 = t2 & 0x3f03fff; t3 >>= 8;
  ctx->r4 = t3 & 0x00fffff;

  ctx->s1 = ctx->r1 * 5;
  ctx->s2 = ctx->r2 * 5;
  ctx->s3 = ctx->r3 * 5;
  ctx->s4 = ctx->r4 * 5;

  ctx->h0 = 0;
  ctx->h1 = 0;
  ctx->h2 = 0;
  ctx->h3 = 0;
  ctx->h4 = 0;
}

void
_poly1305_block (struct poly1305_ctx *ctx, const uint8_t *m, unsigned t4)
{
  uint32_t t0,t1,t2,t3;
  uint32_t b;
  uint64_t t[5];
  uint64_t c;

  t0 = LE_READ_UINT32(m);
  t1 = LE_READ_UINT32(m+4);
  t2 = LE_READ_UINT32(m+8);
  t3 = LE_READ_UINT32(m+12);

  ctx->h0 += t0 & 0x3ffffff;
  ctx->h1 += ((((uint64_t)t1 << 32) | t0) >> 26) & 0x3ffffff;
  ctx->h2 += ((((uint64_t)t2 << 32) | t1) >> 20) & 0x3ffffff;
  ctx->h3 += ((((uint64_t)t3 << 32) | t2) >> 14) & 0x3ffffff;
  ctx->h4 += (t3 >> 8) | ((uint32_t) t4 << 24);

  /* poly1305_donna_mul: */
  t[0]  = mul32x32_64(ctx->h0,ctx->r0) + mul32x32_64(ctx->h1,ctx->s4) + mul32x32_64(ctx->h2,ctx->s3) + mul32x32_64(ctx->h3,ctx->s2) + mul32x32_64(ctx->h4,ctx->s1);
  t[1]  = mul32x32_64(ctx->h0,ctx->r1) + mul32x32_64(ctx->h1,ctx->r0) + mul32x32_64(ctx->h2,ctx->s4) + mul32x32_64(ctx->h3,ctx->s3) + mul32x32_64(ctx->h4,ctx->s2);
  t[2]  = mul32x32_64(ctx->h0,ctx->r2) + mul32x32_64(ctx->h1,ctx->r1) + mul32x32_64(ctx->h2,ctx->r0) + mul32x32_64(ctx->h3,ctx->s4) + mul32x32_64(ctx->h4,ctx->s3);
  t[3]  = mul32x32_64(ctx->h0,ctx->r3) + mul32x32_64(ctx->h1,ctx->r2) + mul32x32_64(ctx->h2,ctx->r1) + mul32x32_64(ctx->h3,ctx->r0) + mul32x32_64(ctx->h4,ctx->s4);
  t[4]  = mul32x32_64(ctx->h0,ctx->r4) + mul32x32_64(ctx->h1,ctx->r3) + mul32x32_64(ctx->h2,ctx->r2) + mul32x32_64(ctx->h3,ctx->r1) + mul32x32_64(ctx->h4,ctx->r0);

  ctx->h0 = (uint32_t)t[0] & 0x3ffffff; c =           (t[0] >> 26);
  t[1] += c;      ctx->h1 = (uint32_t)t[1] & 0x3ffffff; b = (uint32_t)(t[1] >> 26);
  t[2] += b;      ctx->h2 = (uint32_t)t[2] & 0x3ffffff; b = (uint32_t)(t[2] >> 26);
  t[3] += b;      ctx->h3 = (uint32_t)t[3] & 0x3ffffff; b = (uint32_t)(t[3] >> 26);
  t[4] += b;      ctx->h4 = (uint32_t)t[4] & 0x3ffffff; b = (uint32_t)(t[4] >> 26);
  ctx->h0 += b * 5;
}

/* Adds digest to the nonce */
void
poly1305_digest (struct poly1305_ctx *ctx, union nettle_block16 *s)
{
  uint32_t b, nb;
  uint64_t f0,f1,f2,f3;
  uint32_t g0,g1,g2,g3,g4;

  b = ctx->h0 >> 26; ctx->h0 = ctx->h0 & 0x3ffffff;
  ctx->h1 +=     b; b = ctx->h1 >> 26; ctx->h1 = ctx->h1 & 0x3ffffff;
  ctx->h2 +=     b; b = ctx->h2 >> 26; ctx->h2 = ctx->h2 & 0x3ffffff;
  ctx->h3 +=     b; b = ctx->h3 >> 26; ctx->h3 = ctx->h3 & 0x3ffffff;
  ctx->h4 +=     b; b = ctx->h4 >> 26; ctx->h4 = ctx->h4 & 0x3ffffff;
  ctx->h0 += b * 5; b = ctx->h0 >> 26; ctx->h0 = ctx->h0 & 0x3ffffff;
  ctx->h1 +=     b;

  g0 = ctx->h0 + 5; b = g0 >> 26; g0 &= 0x3ffffff;
  g1 = ctx->h1 + b; b = g1 >> 26; g1 &= 0x3ffffff;
  g2 = ctx->h2 + b; b = g2 >> 26; g2 &= 0x3ffffff;
  g3 = ctx->h3 + b; b = g3 >> 26; g3 &= 0x3ffffff;
  g4 = ctx->h4 + b - (1 << 26);

  b = (g4 >> 31) - 1;
  nb = ~b;
  ctx->h0 = (ctx->h0 & nb) | (g0 & b);
  ctx->h1 = (ctx->h1 & nb) | (g1 & b);
  ctx->h2 = (ctx->h2 & nb) | (g2 & b);
  ctx->h3 = (ctx->h3 & nb) | (g3 & b);
  ctx->h4 = (ctx->h4 & nb) | (g4 & b);

  /* FIXME: Take advantage of s being aligned as an unsigned long. */
  f0 = ((ctx->h0    )|(ctx->h1<<26)) + (uint64_t)LE_READ_UINT32(s->b);
  f1 = ((ctx->h1>> 6)|(ctx->h2<<20)) + (uint64_t)LE_READ_UINT32(s->b+4);
  f2 = ((ctx->h2>>12)|(ctx->h3<<14)) + (uint64_t)LE_READ_UINT32(s->b+8);
  f3 = ((ctx->h3>>18)|(ctx->h4<< 8)) + (uint64_t)LE_READ_UINT32(s->b+12);

  LE_WRITE_UINT32(s->b, f0);
  f1 += (f0 >> 32);
  LE_WRITE_UINT32(s->b+4, f1);
  f2 += (f1 >> 32);
  LE_WRITE_UINT32(s->b+8, f2);
  f3 += (f2 >> 32);
  LE_WRITE_UINT32(s->b+12, f3);

  ctx->h0 = 0;
  ctx->h1 = 0;
  ctx->h2 = 0;
  ctx->h3 = 0;
  ctx->h4 = 0;
}
