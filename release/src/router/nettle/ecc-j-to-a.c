/* ecc-j-to-a.c

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

/* Development of Nettle's ECC support was funded by the .SE Internet Fund. */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "ecc.h"
#include "ecc-internal.h"

void
ecc_j_to_a (const struct ecc_curve *ecc,
	    int op,
	    mp_limb_t *r, const mp_limb_t *p,
	    mp_limb_t *scratch)
{
#define izp   scratch
#define up   (scratch + 2*ecc->p.size)
#define iz2p (scratch + ecc->p.size)
#define iz3p (scratch + 2*ecc->p.size)
#define izBp (scratch + 3*ecc->p.size)
#define tp    scratch

  mp_limb_t cy;

  if (ecc->use_redc)
    {
      /* Set v = (r_z / B^2)^-1,

	 r_x = p_x v^2 / B^3 =  ((v/B * v)/B * p_x)/B
	 r_y = p_y v^3 / B^4 = (((v/B * v)/B * v)/B * p_y)/B
      */

      mpn_copyi (up, p + 2*ecc->p.size, ecc->p.size);
      mpn_zero (up + ecc->p.size, ecc->p.size);
      ecc->p.reduce (&ecc->p, up);
      mpn_zero (up + ecc->p.size, ecc->p.size);
      ecc->p.reduce (&ecc->p, up);

      ecc->p.invert (&ecc->p, izp, up, up + ecc->p.size);

      /* Divide this common factor by B */
      mpn_copyi (izBp, izp, ecc->p.size);
      mpn_zero (izBp + ecc->p.size, ecc->p.size);
      ecc->p.reduce (&ecc->p, izBp);

      ecc_modp_mul (ecc, iz2p, izp, izBp);
    }
  else
    {
      /* Set s = p_z^{-1}, r_x = p_x s^2, r_y = p_y s^3 */

      mpn_copyi (up, p+2*ecc->p.size, ecc->p.size); /* p_z */
      ecc->p.invert (&ecc->p, izp, up, up + ecc->p.size);

      ecc_modp_sqr (ecc, iz2p, izp);
    }

  ecc_modp_mul (ecc, iz3p, iz2p, p);
  /* ecc_modp (and ecc_modp_mul) may return a value up to 2p - 1, so
     do a conditional subtraction. */
  cy = mpn_sub_n (r, iz3p, ecc->p.m, ecc->p.size);
  cnd_copy (cy, r, iz3p, ecc->p.size);

  if (op)
    {
      /* Skip y coordinate */
      if (op > 1)
	{
	  /* Also reduce the x coordinate mod ecc->q. It should
	     already be < 2*ecc->q, so one subtraction should
	     suffice. */
	  cy = mpn_sub_n (scratch, r, ecc->q.m, ecc->p.size);
	  cnd_copy (cy == 0, r, scratch, ecc->p.size);
	}
      return;
    }
  ecc_modp_mul (ecc, iz3p, iz2p, izp);
  ecc_modp_mul (ecc, tp, iz3p, p + ecc->p.size);
  /* And a similar subtraction. */
  cy = mpn_sub_n (r + ecc->p.size, tp, ecc->p.m, ecc->p.size);
  cnd_copy (cy, r + ecc->p.size, tp, ecc->p.size);

#undef izp
#undef up
#undef iz2p
#undef iz3p
#undef tp
}
