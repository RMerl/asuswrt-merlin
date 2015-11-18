/* des.c

   The des block cipher.

   Copyright (C) 2001, 2010 Niels Möller
   Copyright (C) 1992  Dana L. How

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

/*	des - fast & portable DES encryption & decryption.
 *	Copyright (C) 1992  Dana L. How
 *	Please see the file `descore.README' for the complete copyright notice.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include "des.h"

#include "desCode.h"

/* various tables */

static const uint32_t
des_keymap[] = {
#include	"keymap.h"
};

static const uint8_t
rotors[] = {
#include	"rotors.h"
};

static ENCRYPT(DesSmallFipsEncrypt,TEMPSMALL, LOADFIPS,KEYMAPSMALL,SAVEFIPS)
static DECRYPT(DesSmallFipsDecrypt,TEMPSMALL, LOADFIPS,KEYMAPSMALL,SAVEFIPS)

/* If parity bits are used, keys should have odd parity. We use a
   small table, to not waste any memory on this fairly obscure DES
   feature. */

static const unsigned
parity_16[16] =
{ 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

#define PARITY(x) (parity_16[(x)&0xf] ^ parity_16[((x)>>4) & 0xf])

int
des_check_parity(size_t length, const uint8_t *key)
{
  size_t i;
  for (i = 0; i<length; i++)
    if (!PARITY(key[i]))
      return 0;

  return 1;
}

void
des_fix_parity(size_t length, uint8_t *dst,
	       const uint8_t *src)
{
  size_t i;
  for (i = 0; i<length; i++)
    dst[i] = src[i] ^ PARITY(src[i]) ^ 1;
}

/* Weak and semiweak keys, excluding parity:
 *
 * 00 00 00 00  00 00 00 00
 * 7f 7f 7f 7f  7f 7f 7f 7f 
 * 0f 0f 0f 0f  07 07 07 07
 * 70 70 70 70  78 78 78 78
 *
 * 00 7f 00 7f  00 7f 00 7f
 * 7f 00 7f 00  7f 00 7f 00
 *
 * 0f 70 0f 70  07 78 07 78
 * 70 0f 70 0f  78 07 78 07
 *
 * 00 70 00 70  00 78 00 78
 * 70 00 70 00  78 00 78 00
 *
 * 0f 7f 0f 7f  07 7f 07 7f
 * 7f 0f 7f 0f  7f 07 7f 07
 *
 * 00 0f 00 0f  00 07 00 07
 * 0f 00 0f 00  07 00 07 00
 *
 * 70 7f 70 7f  78 7f 78 7f
 * 7f 70 7f 70  7f 78 7f 78
 */

static int
des_weak_p(const uint8_t *key)
{
  /* Hash function generated using gperf. */
  static const unsigned char asso_values[0x81] =
    {
      16,  9, 26, 26, 26, 26, 26, 26, 26, 26,
      26, 26, 26, 26, 26,  6,  2, 26, 26, 26,
      26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
      26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
      26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
      26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
      26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
      26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
      26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
      26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
      26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
      26, 26,  3,  1, 26, 26, 26, 26, 26, 26,
      26, 26, 26, 26, 26, 26, 26,  0,  0
    };

  static const int8_t weak_key_hash[26][4] =
    {
      /*  0 */ {0x7f,0x7f, 0x7f,0x7f},
      /*  1 */ {0x7f,0x70, 0x7f,0x78},
      /*  2 */ {0x7f,0x0f, 0x7f,0x07},
      /*  3 */ {0x70,0x7f, 0x78,0x7f},
      /*  4 */ {0x70,0x70, 0x78,0x78},
      /*  5 */ {0x70,0x0f, 0x78,0x07},
      /*  6 */ {0x0f,0x7f, 0x07,0x7f},
      /*  7 */ {0x0f,0x70, 0x07,0x78},
      /*  8 */ {0x0f,0x0f, 0x07,0x07},
      /*  9 */ {0x7f,0x00, 0x7f,0x00},
      /* 10 */ {-1,-1,-1,-1},
      /* 11 */ {-1,-1,-1,-1},
      /* 12 */ {0x70,0x00, 0x78,0x00},
      /* 13 */ {-1,-1,-1,-1},
      /* 14 */ {-1,-1,-1,-1},
      /* 15 */ {0x0f,0x00, 0x07,0x00},
      /* 16 */ {0x00,0x7f, 0x00,0x7f},
      /* 17 */ {0x00,0x70, 0x00,0x78},
      /* 18 */ {0x00,0x0f, 0x00,0x07},
      /* 19 */ {-1,-1,-1,-1},
      /* 20 */ {-1,-1,-1,-1},
      /* 21 */ {-1,-1,-1,-1},
      /* 22 */ {-1,-1,-1,-1},
      /* 23 */ {-1,-1,-1,-1},
      /* 24 */ {-1,-1,-1,-1},
      /* 25 */ {0x00,0x00, 0x00,0x00}
    };

  int8_t k0 = key[0] >> 1;
  int8_t k1 = key[1] >> 1;

  unsigned hash = asso_values[k1 + 1] + asso_values[k0];
  const int8_t *candidate = weak_key_hash[hash];

  if (hash > 25)
    return 0;
  if (k0 != candidate[0]
      || k1 != candidate[1])
    return 0;
  
  if ( (key[2] >> 1) != k0
       || (key[3] >> 1) != k1)
    return 0;

  k0 = key[4] >> 1;
  k1 = key[5] >> 1;
  if (k0 != candidate[2]
      || k1 != candidate[3])
    return 0;
  if ( (key[6] >> 1) != k0
       || (key[7] >> 1) != k1)
    return 0;

  return 1;
}

int
des_set_key(struct des_ctx *ctx, const uint8_t *key)
{
  register uint32_t n, w;
  register char * b0, * b1;
  char bits0[56], bits1[56];
  uint32_t *method;
  const uint8_t *k;

  /* explode the bits */
  n = 56;
  b0 = bits0;
  b1 = bits1;
  k = key;
  do {
    w = (256 | *k++) << 2;
    do {
      --n;
      b1[n] = 8 & w;
      w >>= 1;
      b0[n] = 4 & w;
    } while ( w >= 16 );
  } while ( n );

  /* put the bits in the correct places */
  n = 16;
  k = rotors;
  method = ctx->key;
  
  do {
    w   = (b1[k[ 0   ]] | b0[k[ 1   ]]) << 4;
    w  |= (b1[k[ 2   ]] | b0[k[ 3   ]]) << 2;
    w  |=  b1[k[ 4   ]] | b0[k[ 5   ]];
    w <<= 8;
    w  |= (b1[k[ 6   ]] | b0[k[ 7   ]]) << 4;
    w  |= (b1[k[ 8   ]] | b0[k[ 9   ]]) << 2;
    w  |=  b1[k[10   ]] | b0[k[11   ]];
    w <<= 8;
    w  |= (b1[k[12   ]] | b0[k[13   ]]) << 4;
    w  |= (b1[k[14   ]] | b0[k[15   ]]) << 2;
    w  |=  b1[k[16   ]] | b0[k[17   ]];
    w <<= 8;
    w  |= (b1[k[18   ]] | b0[k[19   ]]) << 4;
    w  |= (b1[k[20   ]] | b0[k[21   ]]) << 2;
    w  |=  b1[k[22   ]] | b0[k[23   ]];

    method[0] = w;

    w   = (b1[k[ 0+24]] | b0[k[ 1+24]]) << 4;
    w  |= (b1[k[ 2+24]] | b0[k[ 3+24]]) << 2;
    w  |=  b1[k[ 4+24]] | b0[k[ 5+24]];
    w <<= 8;
    w  |= (b1[k[ 6+24]] | b0[k[ 7+24]]) << 4;
    w  |= (b1[k[ 8+24]] | b0[k[ 9+24]]) << 2;
    w  |=  b1[k[10+24]] | b0[k[11+24]];
    w <<= 8;
    w  |= (b1[k[12+24]] | b0[k[13+24]]) << 4;
    w  |= (b1[k[14+24]] | b0[k[15+24]]) << 2;
    w  |=  b1[k[16+24]] | b0[k[17+24]];
    w <<= 8;
    w  |= (b1[k[18+24]] | b0[k[19+24]]) << 4;
    w  |= (b1[k[20+24]] | b0[k[21+24]]) << 2;
    w  |=  b1[k[22+24]] | b0[k[23+24]];

    ROR(w, 4, 28);		/* could be eliminated */
    method[1] = w;

    k	+= 48;
    method	+= 2;
  } while ( --n );

  return !des_weak_p (key);
}

void
des_encrypt(const struct des_ctx *ctx,
	    size_t length, uint8_t *dst,
	    const uint8_t *src)
{
  assert(!(length % DES_BLOCK_SIZE));
  
  while (length)
    {
      DesSmallFipsEncrypt(dst, ctx->key, src);
      length -= DES_BLOCK_SIZE;
      src += DES_BLOCK_SIZE;
      dst += DES_BLOCK_SIZE;
    }
}

void
des_decrypt(const struct des_ctx *ctx,
	    size_t length, uint8_t *dst,
	    const uint8_t *src)
{
  assert(!(length % DES_BLOCK_SIZE));

  while (length)
    {
      DesSmallFipsDecrypt(dst, ctx->key, src);
      length -= DES_BLOCK_SIZE;
      src += DES_BLOCK_SIZE;
      dst += DES_BLOCK_SIZE;
    }
}
