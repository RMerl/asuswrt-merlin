/* cast128.c

   The CAST-128 block cipher, described in RFC 2144.

   Copyright (C) 2001, 2014 Niels Möller

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

/* Based on:
 *
 *	CAST-128 in C
 *	Written by Steve Reid <sreid@sea-to-sky.net>
 *	100% Public Domain - no warranty
 *	Released 1997.10.11
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "cast128.h"
#include "cast128_sboxes.h"

#include "macros.h"

#define CAST_SMALL_KEY 10

#define S1 cast_sbox1
#define S2 cast_sbox2
#define S3 cast_sbox3
#define S4 cast_sbox4
#define S5 cast_sbox5
#define S6 cast_sbox6
#define S7 cast_sbox7
#define S8 cast_sbox8

/* Macros to access 8-bit bytes out of a 32-bit word */
#define B0(x) ( (uint8_t) (x>>24) )
#define B1(x) ( (uint8_t) ((x>>16)&0xff) )
#define B2(x) ( (uint8_t) ((x>>8)&0xff) )
#define B3(x) ( (uint8_t) ((x)&0xff) )

/* NOTE: Depends on ROTL32 supporting a zero shift count. */

/* CAST-128 uses three different round functions */
#define F1(l, r, i) do {					\
    t = ctx->Km[i] + r;						\
    t = ROTL32(ctx->Kr[i], t);					\
    l ^= ((S1[B0(t)] ^ S2[B1(t)]) - S3[B2(t)]) + S4[B3(t)];	\
  } while (0)
#define F2(l, r, i) do {					\
    t = ctx->Km[i] ^ r;						\
    t = ROTL32( ctx->Kr[i], t);					\
    l ^= ((S1[B0(t)] - S2[B1(t)]) + S3[B2(t)]) ^ S4[B3(t)];	\
  } while (0)
#define F3(l, r, i) do {					\
    t = ctx->Km[i] - r;						\
    t = ROTL32(ctx->Kr[i], t);					\
    l ^= ((S1[B0(t)] + S2[B1(t)]) ^ S3[B2(t)]) - S4[B3(t)];	\
  } while (0)


/***** Encryption Function *****/

void
cast128_encrypt(const struct cast128_ctx *ctx,
		size_t length, uint8_t *dst,
		const uint8_t *src)
{
  FOR_BLOCKS(length, dst, src, CAST128_BLOCK_SIZE)
    {
      uint32_t t, l, r;

      /* Get inblock into l,r */
      l = READ_UINT32(src);
      r = READ_UINT32(src+4);

      /* Do the work */
      F1(l, r,  0);
      F2(r, l,  1);
      F3(l, r,  2);
      F1(r, l,  3);
      F2(l, r,  4);
      F3(r, l,  5);
      F1(l, r,  6);
      F2(r, l,  7);
      F3(l, r,  8);
      F1(r, l,  9);
      F2(l, r, 10);
      F3(r, l, 11);
      /* Only do full 16 rounds if key length > 80 bits */
      if (ctx->rounds & 16) {
	F1(l, r, 12);
	F2(r, l, 13);
	F3(l, r, 14);
	F1(r, l, 15);
      }
      /* Put l,r into outblock */
      WRITE_UINT32(dst, r);
      WRITE_UINT32(dst + 4, l);
    }
}


/***** Decryption Function *****/

void
cast128_decrypt(const struct cast128_ctx *ctx,
		size_t length, uint8_t *dst,
		const uint8_t *src)
{
  FOR_BLOCKS(length, dst, src, CAST128_BLOCK_SIZE)
    {
      uint32_t t, l, r;

      /* Get inblock into l,r */
      r = READ_UINT32(src);
      l = READ_UINT32(src+4);

      /* Do the work */
      /* Only do full 16 rounds if key length > 80 bits */
      if (ctx->rounds & 16) {
	F1(r, l, 15);
	F3(l, r, 14);
	F2(r, l, 13);
	F1(l, r, 12);
      }
      F3(r, l, 11);
      F2(l, r, 10);
      F1(r, l,  9);
      F3(l, r,  8);
      F2(r, l,  7);
      F1(l, r,  6);
      F3(r, l,  5);
      F2(l, r,  4);
      F1(r, l,  3);
      F3(l, r,  2);
      F2(r, l,  1);
      F1(l, r,  0);

      /* Put l,r into outblock */
      WRITE_UINT32(dst, l);
      WRITE_UINT32(dst + 4, r);
    }
}

/***** Key Schedule *****/

#define SET_KM(i, k) ctx->Km[i] = (k)
#define SET_KR(i, k) ctx->Kr[i] = (k) & 31

#define EXPAND(set, full) do {						\
    z0 = x0 ^ S5[B1(x3)] ^ S6[B3(x3)] ^ S7[B0(x3)] ^ S8[B2(x3)] ^ S7[B0(x2)]; \
    z1 = x2 ^ S5[B0(z0)] ^ S6[B2(z0)] ^ S7[B1(z0)] ^ S8[B3(z0)] ^ S8[B2(x2)]; \
    z2 = x3 ^ S5[B3(z1)] ^ S6[B2(z1)] ^ S7[B1(z1)] ^ S8[B0(z1)] ^ S5[B1(x2)]; \
    z3 = x1 ^ S5[B2(z2)] ^ S6[B1(z2)] ^ S7[B3(z2)] ^ S8[B0(z2)] ^ S6[B3(x2)]; \
    									\
    set(0, S5[B0(z2)] ^ S6[B1(z2)] ^ S7[B3(z1)] ^ S8[B2(z1)] ^ S5[B2(z0)]); \
    set(1, S5[B2(z2)] ^ S6[B3(z2)] ^ S7[B1(z1)] ^ S8[B0(z1)] ^ S6[B2(z1)]); \
    set(2, S5[B0(z3)] ^ S6[B1(z3)] ^ S7[B3(z0)] ^ S8[B2(z0)] ^ S7[B1(z2)]); \
    set(3, S5[B2(z3)] ^ S6[B3(z3)] ^ S7[B1(z0)] ^ S8[B0(z0)] ^ S8[B0(z3)]); \
    									\
    x0 = z2 ^ S5[B1(z1)] ^ S6[B3(z1)] ^ S7[B0(z1)] ^ S8[B2(z1)] ^ S7[B0(z0)]; \
    x1 = z0 ^ S5[B0(x0)] ^ S6[B2(x0)] ^ S7[B1(x0)] ^ S8[B3(x0)] ^ S8[B2(z0)]; \
    x2 = z1 ^ S5[B3(x1)] ^ S6[B2(x1)] ^ S7[B1(x1)] ^ S8[B0(x1)] ^ S5[B1(z0)]; \
    x3 = z3 ^ S5[B2(x2)] ^ S6[B1(x2)] ^ S7[B3(x2)] ^ S8[B0(x2)] ^ S6[B3(z0)]; \
    									\
    set(4, S5[B3(x0)] ^ S6[B2(x0)] ^ S7[B0(x3)] ^ S8[B1(x3)] ^ S5[B0(x2)]); \
    set(5, S5[B1(x0)] ^ S6[B0(x0)] ^ S7[B2(x3)] ^ S8[B3(x3)] ^ S6[B1(x3)]); \
    set(6, S5[B3(x1)] ^ S6[B2(x1)] ^ S7[B0(x2)] ^ S8[B1(x2)] ^ S7[B3(x0)]); \
    set(7, S5[B1(x1)] ^ S6[B0(x1)] ^ S7[B2(x2)] ^ S8[B3(x2)] ^ S8[B3(x1)]); \
    									\
    z0 = x0 ^ S5[B1(x3)] ^ S6[B3(x3)] ^ S7[B0(x3)] ^ S8[B2(x3)] ^ S7[B0(x2)]; \
    z1 = x2 ^ S5[B0(z0)] ^ S6[B2(z0)] ^ S7[B1(z0)] ^ S8[B3(z0)] ^ S8[B2(x2)]; \
    z2 = x3 ^ S5[B3(z1)] ^ S6[B2(z1)] ^ S7[B1(z1)] ^ S8[B0(z1)] ^ S5[B1(x2)]; \
    z3 = x1 ^ S5[B2(z2)] ^ S6[B1(z2)] ^ S7[B3(z2)] ^ S8[B0(z2)] ^ S6[B3(x2)]; \
    									\
    set(8,  S5[B3(z0)] ^ S6[B2(z0)] ^ S7[B0(z3)] ^ S8[B1(z3)] ^ S5[B1(z2)]); \
    set(9,  S5[B1(z0)] ^ S6[B0(z0)] ^ S7[B2(z3)] ^ S8[B3(z3)] ^ S6[B0(z3)]); \
    set(10, S5[B3(z1)] ^ S6[B2(z1)] ^ S7[B0(z2)] ^ S8[B1(z2)] ^ S7[B2(z0)]); \
    set(11, S5[B1(z1)] ^ S6[B0(z1)] ^ S7[B2(z2)] ^ S8[B3(z2)] ^ S8[B2(z1)]); \
									\
    x0 = z2 ^ S5[B1(z1)] ^ S6[B3(z1)] ^ S7[B0(z1)] ^ S8[B2(z1)] ^ S7[B0(z0)]; \
    x1 = z0 ^ S5[B0(x0)] ^ S6[B2(x0)] ^ S7[B1(x0)] ^ S8[B3(x0)] ^ S8[B2(z0)]; \
    x2 = z1 ^ S5[B3(x1)] ^ S6[B2(x1)] ^ S7[B1(x1)] ^ S8[B0(x1)] ^ S5[B1(z0)]; \
    x3 = z3 ^ S5[B2(x2)] ^ S6[B1(x2)] ^ S7[B3(x2)] ^ S8[B0(x2)] ^ S6[B3(z0)]; \
    if (full)								\
      {									\
	set(12, S5[B0(x2)] ^ S6[B1(x2)] ^ S7[B3(x1)] ^ S8[B2(x1)] ^ S5[B3(x0)]); \
	set(13, S5[B2(x2)] ^ S6[B3(x2)] ^ S7[B1(x1)] ^ S8[B0(x1)] ^ S6[B3(x1)]); \
	set(14, S5[B0(x3)] ^ S6[B1(x3)] ^ S7[B3(x0)] ^ S8[B2(x0)] ^ S7[B0(x2)]); \
	set(15, S5[B2(x3)] ^ S6[B3(x3)] ^ S7[B1(x0)] ^ S8[B0(x0)] ^ S8[B1(x3)]); \
      }									\
} while (0)

void
cast5_set_key(struct cast128_ctx *ctx,
	      size_t length, const uint8_t *key)
{
  uint32_t x0, x1, x2, x3, z0, z1, z2, z3;
  uint32_t w;
  int full;

  assert (length >= CAST5_MIN_KEY_SIZE);
  assert (length <= CAST5_MAX_KEY_SIZE);

  full = (length > CAST_SMALL_KEY);

  x0 = READ_UINT32 (key);

  /* Read final word, possibly zero-padded. */
  switch (length & 3)
    {
    case 0:
      w = READ_UINT32 (key + length - 4);
      break;
    case 3:
      w = READ_UINT24 (key + length - 3) << 8;
      break;
    case 2:
      w = READ_UINT16 (key + length - 2) << 16;
      break;
    case 1:
      w = (uint32_t) key[length - 1] << 24;
      break;
    }

  if (length <= 8)
    {
      x1 = w;
      x2 = x3 = 0;
    }
  else
    {
      x1 = READ_UINT32 (key + 4);
      if (length <= 12)
	{
	  x2 = w;
	  x3 = 0;
	}
      else
	{
	  x2 = READ_UINT32 (key + 8);
	  x3 = w;
	}
    }

  EXPAND(SET_KM, full);
  EXPAND(SET_KR, full);

  ctx->rounds = full ? 16 : 12;
}

void
cast128_set_key(struct cast128_ctx *ctx, const uint8_t *key)
{
  cast5_set_key (ctx, CAST128_KEY_SIZE, key);
}
