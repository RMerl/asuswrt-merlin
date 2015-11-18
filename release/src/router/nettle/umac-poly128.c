/* umac-poly128.c

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

#include "umac.h"

#define HI(x) (x >> 32)
#define LO(x) (x & 0xffffffffUL)

static void
poly128_mul (const uint32_t *k, uint64_t *y)
{
  uint64_t y0,y1,y2,y3,p0,p1,p2,p3,m0,m1,m2;
  y0 = LO (y[1]);
  y1 = HI (y[1]);
  y2 = LO (y[0]);
  y3 = HI (y[0]);

  p0 = y0 * k[3];
  m0 = y0 * k[2] + y1 * k[3];
  p1 = y0 * k[1] + y1 * k[2] + y2 * k[3];
  m1 = y0 * k[0] + y1 * k[1] + y2 * k[2] + y3 * k[3];
  p2 = y1 * k[0] + y2 * k[1] + y3 * k[2];
  m2 = y2 * k[0] + y3 * k[1];
  p3 = y3 * k[0];

  /* Collaps to 4 64-bit words,
     +---+---+---+---+
     | p3| p2| p1| p0|
     +-+-+-+-+-+-+-+-+
    +  | m2| m1| m0|
    -+-+-+-+-+-+-+-+-+
  */
  /* But it's convenient to reduce (p3,p2,p1,p0) and (m2,m1,m0) mod p first.*/
  m1 += UMAC_P128_OFFSET * HI(p3);
  p1 += UMAC_P128_OFFSET * (LO(p3) + HI(m2));
  m0 += UMAC_P128_OFFSET * (HI(p2) + LO(m2));
  p0 += UMAC_P128_OFFSET * (LO(p2) + HI(m1));

  /* Left to add
     +---+---+
     | p1| p0|
     +-+-+-+-+
     m1| m0|
     +-+---+
  */
  /* First add high parts, with no possibilities for carries */
  p1 += m0 >> 32;

  m0 <<= 32;
  m1 <<= 32;

  /* Remains:
     +---+---+
     | p1| p0|
     +-+-+---+
    +| m1| m0|
    -+---+---+
  */
  p0 += m0;
  p1 += (p0 < m0);
  p1 += m1;
  if (p1 < m1)
    {
      p0 += UMAC_P128_OFFSET;
      p1 += (p0 < UMAC_P128_OFFSET);
    }

  y[0] = p1;
  y[1] = p0;
}

void
_umac_poly128 (const uint32_t *k, uint64_t *y, uint64_t mh, uint64_t ml)
{
  uint64_t yh, yl, cy;

  if ( (mh >> 32) == 0xffffffff)
    {
      poly128_mul (k, y);
      if (y[1] > 0)
	y[1]--;
      else if (y[0] > 0)
	{
	  y[0]--;
	  y[1] = UMAC_P128_HI;
	}
      else
	{
	  y[0] = UMAC_P128_HI;
	  y[1] = UMAC_P128_LO-1;
	}

      mh -= (ml < UMAC_P128_OFFSET);
      ml -= UMAC_P128_OFFSET;
    }
  assert (mh < UMAC_P128_HI || ml < UMAC_P128_LO);

  poly128_mul (k, y);
  yl = y[1] + ml;
  cy = (yl < ml);
  yh = y[0] + cy;
  cy = (yh < cy);
  yh += mh;
  cy += (yh < mh);
  assert (cy <= 1);
  if (cy)
    {
      yl += UMAC_P128_OFFSET;
      yh += yl < UMAC_P128_OFFSET;
    }

  y[0] = yh;
  y[1] = yl;
}
