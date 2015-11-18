/* salsa20-core-internal.c

   Internal interface to the Salsa20 core function.

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
   salsa20-ref.c version 20051118
   D. J. Bernstein
   Public domain.
*/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <string.h>

#include "salsa20.h"

#include "macros.h"

/* For fat builds */
#if HAVE_NATIVE_salsa20_core
void
_nettle_salsa20_core_c(uint32_t *dst, const uint32_t *src, unsigned rounds);
#define _nettle_salsa20_core _nettle_salsa20_core_c
#endif

#ifndef SALSA20_DEBUG
# define SALSA20_DEBUG 0
#endif

#if SALSA20_DEBUG
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
  x1 ^= ROTL32(7, x0 + x3);	    \
  x2 ^= ROTL32(9, x1 + x0);	    \
  x3 ^= ROTL32(13, x2 + x1);	    \
  x0 ^= ROTL32(18, x3 + x2);	    \
  } while(0)

void
_salsa20_core(uint32_t *dst, const uint32_t *src, unsigned rounds)
{
  uint32_t x[_SALSA20_INPUT_LENGTH];
  unsigned i;

  assert ( (rounds & 1) == 0);

  memcpy (x, src, sizeof(x));
  for (i = 0; i < rounds;i += 2)
    {
      DEBUG (i);
      QROUND(x[0], x[4], x[8], x[12]);
      QROUND(x[5], x[9], x[13], x[1]);
      QROUND(x[10], x[14], x[2], x[6]);
      QROUND(x[15], x[3], x[7], x[11]);

      DEBUG (i+1);
      QROUND(x[0], x[1], x[2], x[3]);
      QROUND(x[5], x[6], x[7], x[4]);
      QROUND(x[10], x[11], x[8], x[9]);
      QROUND(x[15], x[12], x[13], x[14]);
    }
  DEBUG (i);

  for (i = 0; i < _SALSA20_INPUT_LENGTH; i++)
    {
      uint32_t t = x[i] + src[i];
      dst[i] = LE_SWAP32 (t);
    }
}
