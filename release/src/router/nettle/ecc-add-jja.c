/* ecc-add-jj.c

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

/* NOTE: Behaviour for corner cases:

   + p = 0   ==>  r = 0 (invalid except if also q = 0)

   + q = 0   ==>  r = invalid

   + p = -q  ==>  r = 0, correct!

   + p = q   ==>  r = 0, invalid
*/

void
ecc_add_jja (const struct ecc_curve *ecc,
	     mp_limb_t *r, const mp_limb_t *p, const mp_limb_t *q,
	     mp_limb_t *scratch)
{
  /* Formulas, from djb,
     http://www.hyperelliptic.org/EFD/g1p/auto-shortw-jacobian-3.html#doubling-dbl-2001-b):

     Computation		Operation	Live variables
     
      ZZ = Z_1^2		sqr		ZZ
      H = X_2*ZZ - X_1		mul (djb: U_2)	ZZ, H
      HH = H^2			sqr		ZZ, H, HH
      ZZZ = ZZ*Z_1		mul		ZZ, H, HH, ZZZ
      Z_3 = (Z_1+H)^2-ZZ-HH	sqr		H, HH, ZZZ
      W = 2 (Y_2*ZZZ - Y_1)	mul (djb: S_2)	H, HH, W
      I = 4*HH					H, W, I
      J = H*I			mul		W, I, J
      V = X_1*I			mul		W, J, V
      X_3 = W^2-J-2*V		sqr		W, J, V
      Y_3 = W*(V-X_3)-2*Y_1*J	mul, mul
  */
#define zz  scratch
#define h  (scratch + ecc->p.size)
#define hh (scratch + 2*ecc->p.size)
#define w  (scratch + 3*ecc->p.size)
#define j  (scratch + 4*ecc->p.size)
#define v   scratch

#define x1  p
#define y1 (p + ecc->p.size)
#define z1 (p + 2*ecc->p.size)
#define x2  q
#define y2 (q + ecc->p.size)

  /* zz */
  ecc_modp_sqr (ecc, zz, z1);
  /* h*/
  ecc_modp_mul (ecc, h, x2, zz);
  ecc_modp_sub (ecc, h, h, x1);
  /* hh */
  ecc_modp_sqr (ecc, hh, h);
  /* Do z^3 early, store at w. */
  ecc_modp_mul (ecc, w, zz, z1);
  /* z_3, use j area for scratch */
  ecc_modp_add (ecc, r + 2*ecc->p.size, p + 2*ecc->p.size, h);
  ecc_modp_sqr (ecc, j, r + 2*ecc->p.size);
  ecc_modp_sub (ecc, j, j, zz);
  ecc_modp_sub (ecc, r + 2*ecc->p.size, j, hh);
  
  /* w */
  ecc_modp_mul (ecc, j, y2, w);
  ecc_modp_sub (ecc, w, j, y1);
  ecc_modp_mul_1 (ecc, w, w, 2);
  
  /* i replaces hh, j */
  ecc_modp_mul_1 (ecc, hh, hh, 4);
  ecc_modp_mul (ecc, j, hh, h);

  /* v */
  ecc_modp_mul (ecc, v, x1, hh);

  /* x_3, use (h, hh) as sqratch */  
  ecc_modp_sqr (ecc, h, w);
  ecc_modp_sub (ecc, r, h, j);
  ecc_modp_submul_1 (ecc, r, v, 2);

  /* y_3, use (h, hh) as sqratch */
  ecc_modp_mul (ecc, h, y1, j); /* frees j */
  ecc_modp_sub (ecc, r + ecc->p.size, v, r);
  ecc_modp_mul (ecc, j, r + ecc->p.size, w);
  ecc_modp_submul_1 (ecc, j, h, 2);
  mpn_copyi (r + ecc->p.size, j, ecc->p.size);
}
