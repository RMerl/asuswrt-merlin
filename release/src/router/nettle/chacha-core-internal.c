/* chacha-core-internal.c

   Core functionality of the ChaCha stream cipher.
   Heavily based on the Salsa20 implementation in Nettle.

   Copyright (C) 2013 Joachim Strömbergson
   Copyright (C) 2012 Simon Josefsson, Niels Möller

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
   chacha-ref.c version 2008.01.20.
   D. J. Bernstein
   Public domain.
*/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <string.h>

#include "chacha.h"

#include "macros.h"

#ifndef CHACHA_DEBUG
# define CHACHA_DEBUG 0
#endif

#if CHACHA_DEBUG
# include <stdio.h>
# define DEBUG(i) do {				\
    unsigned debug_j;				\
    for (debug_j = 0; debug_j < 16; debug_j++)	\
      {						\
	if (debug_j == 0)			\
	  fprintf(stderr, "%2d:", (i));		\
	else if (debug_j % 4 == 0)		\
	  fprintf(stderr, "\n   ");		\
	fprintf(stderr, " %8x", x[debug_j]);	\
      }						\
    fprintf(stderr, "\n");			\
  } while (0)
#else
# define DEBUG(i)
#endif

#ifdef WORDS_BIGENDIAN
#define LE_SWAP32(v)				\
  ((ROTL32(8,  v) & 0x00FF00FFUL) |		\
   (ROTL32(24, v) & 0xFF00FF00UL))
#else
#define LE_SWAP32(v) (v)
#endif

#define QROUND(x0, x1, x2, x3) do { \
  x0 = x0 + x1; x3 = ROTL32(16, (x0 ^ x3)); \
  x2 = x2 + x3; x1 = ROTL32(12, (x1 ^ x2)); \
  x0 = x0 + x1; x3 = ROTL32(8,  (x0 ^ x3)); \
  x2 = x2 + x3; x1 = ROTL32(7,  (x1 ^ x2)); \
  } while(0)

void
_chacha_core(uint32_t *dst, const uint32_t *src, unsigned rounds)
{
  uint32_t x[_CHACHA_STATE_LENGTH];
  unsigned i;

  assert ( (rounds & 1) == 0);

  memcpy (x, src, sizeof(x));
  for (i = 0; i < rounds;i += 2)
    {
      DEBUG (i);
      QROUND(x[0], x[4], x[8],  x[12]);
      QROUND(x[1], x[5], x[9],  x[13]);
      QROUND(x[2], x[6], x[10], x[14]);
      QROUND(x[3], x[7], x[11], x[15]);

      DEBUG (i+1);
      QROUND(x[0], x[5], x[10], x[15]);
      QROUND(x[1], x[6], x[11], x[12]);
      QROUND(x[2], x[7], x[8],  x[13]);
      QROUND(x[3], x[4], x[9],  x[14]);
    }
  DEBUG (i);

  for (i = 0; i < _CHACHA_STATE_LENGTH; i++)
    {
      uint32_t t = x[i] + src[i];
      dst[i] = LE_SWAP32 (t);
    }
}







