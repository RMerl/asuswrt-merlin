/* curve25519-x.c

   Copyright (C) 2014 Niels Möller

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

#include <string.h>

#include "curve25519.h"

#include "ecc.h"
#include "ecc-internal.h"

/* Transform a point on the twisted Edwards curve to the curve25519
   Montgomery curve, and return the x coordinate. */
void
curve25519_eh_to_x (mp_limb_t *xp, const mp_limb_t *p,
		    mp_limb_t *scratch)
{
#define vp (p + ecc->p.size)
#define wp (p + 2*ecc->p.size)
#define t0 scratch
#define t1 (scratch + ecc->p.size)
#define t2 (scratch + 2*ecc->p.size)

  const struct ecc_curve *ecc = &_nettle_curve25519;
  mp_limb_t cy;

  /* If u = U/W and v = V/W are the coordiantes of the point on the
     Edwards curve we get the curve25519 x coordinate as

     x = (1+v) / (1-v) = (W + V) / (W - V)
  */
  /* NOTE: For the infinity point, this subtraction gives zero (mod
     p), which isn't invertible. For curve25519, the desired output is
     x = 0, and we should be fine, since ecc_modp_inv returns 0
     in this case. */
  ecc_modp_sub (ecc, t0, wp, vp);
  /* Needs a total of 5*size storage. */
  ecc->p.invert (&ecc->p, t1, t0, t2 + ecc->p.size);
  
  ecc_modp_add (ecc, t0, wp, vp);
  ecc_modp_mul (ecc, t2, t0, t1);

  cy = mpn_sub_n (xp, t2, ecc->p.m, ecc->p.size);
  cnd_copy (cy, xp, t2, ecc->p.size);
#undef vp
#undef wp
#undef t0
#undef t1
#undef t2
}
