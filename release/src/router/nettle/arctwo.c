/* arctwo.c

   The cipher described in rfc2268; aka Ron's Cipher 2.
   
   Copyright (C) 2004 Simon Josefsson
   Copyright (C) 2003 Nikos Mavroyanopoulos
   Copyright (C) 2004 Free Software Foundation, Inc.
   Copyright (C) 2004, 2014 Niels Möller

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

/* This implementation was written by Nikos Mavroyanopoulos for GNUTLS
 * as a Libgcrypt module (gnutls/lib/x509/rc2.c) and later adapted for
 * direct use by Libgcrypt by Werner Koch and later adapted for direct
 * use by Nettle by Simon Josefsson and Niels Möller.
 *
 * The implementation here is based on Peter Gutmann's RRC.2 paper and
 * RFC 2268.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include "arctwo.h"

#include "macros.h"

static const uint8_t arctwo_sbox[] = {
  0xd9, 0x78, 0xf9, 0xc4, 0x19, 0xdd, 0xb5, 0xed,
  0x28, 0xe9, 0xfd, 0x79, 0x4a, 0xa0, 0xd8, 0x9d,
  0xc6, 0x7e, 0x37, 0x83, 0x2b, 0x76, 0x53, 0x8e,
  0x62, 0x4c, 0x64, 0x88, 0x44, 0x8b, 0xfb, 0xa2,
  0x17, 0x9a, 0x59, 0xf5, 0x87, 0xb3, 0x4f, 0x13,
  0x61, 0x45, 0x6d, 0x8d, 0x09, 0x81, 0x7d, 0x32,
  0xbd, 0x8f, 0x40, 0xeb, 0x86, 0xb7, 0x7b, 0x0b,
  0xf0, 0x95, 0x21, 0x22, 0x5c, 0x6b, 0x4e, 0x82,
  0x54, 0xd6, 0x65, 0x93, 0xce, 0x60, 0xb2, 0x1c,
  0x73, 0x56, 0xc0, 0x14, 0xa7, 0x8c, 0xf1, 0xdc,
  0x12, 0x75, 0xca, 0x1f, 0x3b, 0xbe, 0xe4, 0xd1,
  0x42, 0x3d, 0xd4, 0x30, 0xa3, 0x3c, 0xb6, 0x26,
  0x6f, 0xbf, 0x0e, 0xda, 0x46, 0x69, 0x07, 0x57,
  0x27, 0xf2, 0x1d, 0x9b, 0xbc, 0x94, 0x43, 0x03,
  0xf8, 0x11, 0xc7, 0xf6, 0x90, 0xef, 0x3e, 0xe7,
  0x06, 0xc3, 0xd5, 0x2f, 0xc8, 0x66, 0x1e, 0xd7,
  0x08, 0xe8, 0xea, 0xde, 0x80, 0x52, 0xee, 0xf7,
  0x84, 0xaa, 0x72, 0xac, 0x35, 0x4d, 0x6a, 0x2a,
  0x96, 0x1a, 0xd2, 0x71, 0x5a, 0x15, 0x49, 0x74,
  0x4b, 0x9f, 0xd0, 0x5e, 0x04, 0x18, 0xa4, 0xec,
  0xc2, 0xe0, 0x41, 0x6e, 0x0f, 0x51, 0xcb, 0xcc,
  0x24, 0x91, 0xaf, 0x50, 0xa1, 0xf4, 0x70, 0x39,
  0x99, 0x7c, 0x3a, 0x85, 0x23, 0xb8, 0xb4, 0x7a,
  0xfc, 0x02, 0x36, 0x5b, 0x25, 0x55, 0x97, 0x31,
  0x2d, 0x5d, 0xfa, 0x98, 0xe3, 0x8a, 0x92, 0xae,
  0x05, 0xdf, 0x29, 0x10, 0x67, 0x6c, 0xba, 0xc9,
  0xd3, 0x00, 0xe6, 0xcf, 0xe1, 0x9e, 0xa8, 0x2c,
  0x63, 0x16, 0x01, 0x3f, 0x58, 0xe2, 0x89, 0xa9,
  0x0d, 0x38, 0x34, 0x1b, 0xab, 0x33, 0xff, 0xb0,
  0xbb, 0x48, 0x0c, 0x5f, 0xb9, 0xb1, 0xcd, 0x2e,
  0xc5, 0xf3, 0xdb, 0x47, 0xe5, 0xa5, 0x9c, 0x77,
  0x0a, 0xa6, 0x20, 0x68, 0xfe, 0x7f, 0xc1, 0xad
};

#define rotl16(x,n) (((x) << ((uint16_t)(n))) | ((x) >> (16 - (uint16_t)(n))))
#define rotr16(x,n) (((x) >> ((uint16_t)(n))) | ((x) << (16 - (uint16_t)(n))))

void
arctwo_encrypt (struct arctwo_ctx *ctx,
		size_t length, uint8_t *dst, const uint8_t *src)
{
  FOR_BLOCKS (length, dst, src, ARCTWO_BLOCK_SIZE)
  {
    register unsigned i;
    uint16_t w0, w1, w2, w3;

    w0 = LE_READ_UINT16 (&src[0]);
    w1 = LE_READ_UINT16 (&src[2]);
    w2 = LE_READ_UINT16 (&src[4]);
    w3 = LE_READ_UINT16 (&src[6]);

    for (i = 0; i < 16; i++)
      {
	register unsigned j = i * 4;
	/* For some reason I cannot combine those steps. */
	w0 += (w1 & ~w3) + (w2 & w3) + ctx->S[j];
	w0 = rotl16 (w0, 1);

	w1 += (w2 & ~w0) + (w3 & w0) + ctx->S[j + 1];
	w1 = rotl16 (w1, 2);

	w2 += (w3 & ~w1) + (w0 & w1) + ctx->S[j + 2];
	w2 = rotl16 (w2, 3);

	w3 += (w0 & ~w2) + (w1 & w2) + ctx->S[j + 3];
	w3 = rotl16 (w3, 5);

	if (i == 4 || i == 10)
	  {
	    w0 += ctx->S[w3 & 63];
	    w1 += ctx->S[w0 & 63];
	    w2 += ctx->S[w1 & 63];
	    w3 += ctx->S[w2 & 63];
	  }
      }
    LE_WRITE_UINT16 (&dst[0], w0);
    LE_WRITE_UINT16 (&dst[2], w1);
    LE_WRITE_UINT16 (&dst[4], w2);
    LE_WRITE_UINT16 (&dst[6], w3);
  }
}

void
arctwo_decrypt (struct arctwo_ctx *ctx,
		size_t length, uint8_t *dst, const uint8_t *src)
{
  FOR_BLOCKS (length, dst, src, ARCTWO_BLOCK_SIZE)
  {
    register unsigned i;
    uint16_t w0, w1, w2, w3;

    w0 = LE_READ_UINT16 (&src[0]);
    w1 = LE_READ_UINT16 (&src[2]);
    w2 = LE_READ_UINT16 (&src[4]);
    w3 = LE_READ_UINT16 (&src[6]);

    for (i = 16; i-- > 0; )
      {
	register unsigned j = i * 4;

	w3 = rotr16 (w3, 5);
	w3 -= (w0 & ~w2) + (w1 & w2) + ctx->S[j + 3];

	w2 = rotr16 (w2, 3);
	w2 -= (w3 & ~w1) + (w0 & w1) + ctx->S[j + 2];

	w1 = rotr16 (w1, 2);
	w1 -= (w2 & ~w0) + (w3 & w0) + ctx->S[j + 1];

	w0 = rotr16 (w0, 1);
	w0 -= (w1 & ~w3) + (w2 & w3) + ctx->S[j];

	if (i == 5 || i == 11)
	  {
	    w3 = w3 - ctx->S[w2 & 63];
	    w2 = w2 - ctx->S[w1 & 63];
	    w1 = w1 - ctx->S[w0 & 63];
	    w0 = w0 - ctx->S[w3 & 63];
	  }

      }
    LE_WRITE_UINT16 (&dst[0], w0);
    LE_WRITE_UINT16 (&dst[2], w1);
    LE_WRITE_UINT16 (&dst[4], w2);
    LE_WRITE_UINT16 (&dst[6], w3);
  }
}

void
arctwo_set_key_ekb (struct arctwo_ctx *ctx,
		    size_t length, const uint8_t *key, unsigned ekb)
{
  size_t i;
  /* Expanded key, treated as octets */
  uint8_t S[128];
  uint8_t x;

  assert (length >= ARCTWO_MIN_KEY_SIZE);
  assert (length <= ARCTWO_MAX_KEY_SIZE);
  assert (ekb <= 1024);

  for (i = 0; i < length; i++)
    S[i] = key[i];

  /* Phase 1: Expand input key to 128 bytes */
  for (i = length; i < ARCTWO_MAX_KEY_SIZE; i++)
    S[i] = arctwo_sbox[(S[i - length] + S[i - 1]) & 255];

  S[0] = arctwo_sbox[S[0]];

  /* Reduce effective key size to ekb bits, if requested by caller. */
  if (ekb > 0 && ekb < 1024)
    {
      int len = (ekb + 7) >> 3;
      i = 128 - len;
      x = arctwo_sbox[S[i] & (255 >> (7 & -ekb))];
      S[i] = x;

      while (i--)
	{
	  x = arctwo_sbox[x ^ S[i + len]];
	  S[i] = x;
	}
    }

  /* Make the expanded key endian independent. */
  for (i = 0; i < 64; i++)
    ctx->S[i] = LE_READ_UINT16(S + i * 2);
}

void
arctwo_set_key (struct arctwo_ctx *ctx, size_t length, const uint8_t *key)
{
  arctwo_set_key_ekb (ctx, length, key, 8 * length);
}

void
arctwo_set_key_gutmann (struct arctwo_ctx *ctx,
			size_t length, const uint8_t *key)
{
  arctwo_set_key_ekb (ctx, length, key, 0);
}

void
arctwo40_set_key (struct arctwo_ctx *ctx, const uint8_t *key)
{
  arctwo_set_key_ekb (ctx, 5, key, 40);
}
void
arctwo64_set_key (struct arctwo_ctx *ctx, const uint8_t *key)
{
  arctwo_set_key_ekb (ctx, 8, key, 64);
}

void
arctwo128_set_key (struct arctwo_ctx *ctx, const uint8_t *key)
{
  arctwo_set_key_ekb (ctx, 16, key, 128);
}
void
arctwo128_set_key_gutmann (struct arctwo_ctx *ctx,
			   const uint8_t *key)
{
  arctwo_set_key_ekb (ctx, 16, key, 1024);
}
