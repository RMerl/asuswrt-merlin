/* umac-l2.c

   Copyright (C) 2013 Niels Möller

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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <string.h>

#include "umac.h"

#include "macros.h"

/* Same mask applied to low and high halves */
#define KEY_MASK 0x01ffffffUL

#if WORDS_BIGENDIAN
#define BE_SWAP32(x) x
#else
#define BE_SWAP32(x)				\
  ((ROTL32(8,  x) & 0x00FF00FFUL) |		\
   (ROTL32(24, x) & 0xFF00FF00UL))
#endif

void
_umac_l2_init (unsigned size, uint32_t *k)
{
  unsigned i;
  for (i = 0; i < size; i++)
    {
      uint32_t w = k[i];
      w = BE_SWAP32 (w);
      k[i] = w & KEY_MASK;
    }
}

void
_umac_l2(const uint32_t *key, uint64_t *state, unsigned n,
	 uint64_t count, const uint64_t *m)
{
  uint64_t *prev = state + 2*n;
  unsigned i;

  if (count == 0)
    memcpy (prev, m, n * sizeof(*m));
  else if (count == 1)
    for (i = 0; i < n; i++, key += 6)
      {
	uint64_t y = _umac_poly64 (key[0], key[1], 1, prev[i]);
	state[2*i+1] = _umac_poly64 (key[0], key[1], y, m[i]);
      }
  else if (count < UMAC_POLY64_BLOCKS)
    for (i = 0; i < n; i++, key += 6)
      state[2*i+1] = _umac_poly64 (key[0], key[1], state[2*i+1], m[i]);
  else if (count % 2 == 0)
    {
      if (count == UMAC_POLY64_BLOCKS)
	for (i = 0, key += 2; i < n; i++, key += 6)
	  {
	    uint64_t y = state[2*i+1];
	    if (y >= UMAC_P64)
	      y -= UMAC_P64;
	    state[2*i] = 0;
	    state[2*i+1] = 1;

	    _umac_poly128 (key, state + 2*i, 0, y);
	  }
      memcpy (prev, m, n * sizeof(*m));
    }
  else
    for (i = 0, key += 2; i < n; i++, key += 6)
      _umac_poly128 (key, state + 2*i, prev[i], m[i]);
}

void
_umac_l2_final(const uint32_t *key, uint64_t *state, unsigned n,
	       uint64_t count)
{
  uint64_t *prev = state + 2*n;
  unsigned i;

  assert (count > 0);
  if (count == 1)
    for (i = 0; i < n; i++)
      {
	*state++ = 0;
	*state++ = *prev++;
      }
  else if (count <= UMAC_POLY64_BLOCKS)
    for (i = 0; i < n; i++)
      {
	uint64_t y;
	*state++ = 0;

	y = *state;
	if (y >= UMAC_P64)
	  y -= UMAC_P64;
	*state++ = y;
      }
  else
    {
      uint64_t pad = (uint64_t) 1 << 63;
      if (count % 2 == 1)
	for (i = 0, key += 2; i < n; i++, key += 6)
	  _umac_poly128 (key, state + 2*i, prev[i], pad);
      else
	for (i = 0, key += 2; i < n; i++, key += 6)
	  _umac_poly128 (key, state + 2*i, pad, 0);

      for (i = 0; i < n; i++, state += 2)
	{
	  uint64_t yh, yl;

	  yh = state[0];
	  yl = state[1];
	  if (yh == UMAC_P128_HI && yl >= UMAC_P128_LO)
	    {
	      state[0] = 0;
	      state[1] = yl -= UMAC_P128_LO;
	    }
	}
    }
}
