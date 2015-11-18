/* ecc-add-jjj.c

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
ecc_add_jjj (const struct ecc_curve *ecc,
	     mp_limb_t *r, const mp_limb_t *p, const mp_limb_t *q,
	     mp_limb_t *scratch)
{
  /* Formulas, from djb,
     http://www.hyperelliptic.org/EFD/g1p/auto-shortw-jacobian-3.html#addition-add-2007-bl:

     Computation		Operation	Live variables

      Z1Z1 = Z1^2		sqr		Z1Z1
      Z2Z2 = Z2^2		sqr		Z1Z1, Z2Z2
      U1 = X1*Z2Z2		mul		Z1Z1, Z2Z2, U1
      U2 = X2*Z1Z1		mul		Z1Z1, Z2Z2, U1, U2
      H = U2-U1					Z1Z1, Z2Z2, U1, H
      Z3 = ((Z1+Z2)^2-Z1Z1-Z2Z2)*H sqr, mul	Z1Z1, Z2Z2, U1, H
      S1 = Y1*Z2*Z2Z2		mul, mul	Z1Z1, U1, H, S1
      S2 = Y2*Z1*Z1Z1		mul, mul	U1, H, S1, S2
      W = 2*(S2-S1)	(djb: r)		U1, H, S1, W
      I = (2*H)^2		sqr		U1, H, S1, W, I
      J = H*I			mul		U1, S1, W, J, V
      V = U1*I			mul		S1, W, J, V
      X3 = W^2-J-2*V		sqr		S1, W, J, V
      Y3 = W*(V-X3)-2*S1*J	mul, mul
  */
  mp_limb_t *z1z1 = scratch;
  mp_limb_t *z2z2 = scratch + ecc->p.size;
  mp_limb_t *u1   = scratch + 2*ecc->p.size;
  mp_limb_t *u2   = scratch + 3*ecc->p.size;
  mp_limb_t *s1   = scratch; /* overlap z1z1 */
  mp_limb_t *s2   = scratch + ecc->p.size; /* overlap z2z2 */
  mp_limb_t *i    = scratch + 4*ecc->p.size;
  mp_limb_t *j    = scratch + 5*ecc->p.size;
  mp_limb_t *v    = scratch + 6*ecc->p.size;

  /* z1^2, z2^2, u1 = x1 x2^2, u2 = x2 z1^2 - u1 */
  ecc_modp_sqr (ecc, z1z1, p + 2*ecc->p.size);
  ecc_modp_sqr (ecc, z2z2, q + 2*ecc->p.size);
  ecc_modp_mul (ecc, u1, p, z2z2);
  ecc_modp_mul (ecc, u2, q, z1z1);
  ecc_modp_sub (ecc, u2, u2, u1);  /* Store h in u2 */

  /* z3, use i, j, v as scratch, result at i. */
  ecc_modp_add (ecc, i, p + 2*ecc->p.size, q + 2*ecc->p.size);
  ecc_modp_sqr (ecc, v, i);
  ecc_modp_sub (ecc, v, v, z1z1);
  ecc_modp_sub (ecc, v, v, z2z2);
  ecc_modp_mul (ecc, i, v, u2);
  /* Delayed write, to support in-place operation. */

  /* s1 = y1 z2^3, s2 = y2 z1^3, scratch at j and v */
  ecc_modp_mul (ecc, j, z1z1, p + 2*ecc->p.size); /* z1^3 */
  ecc_modp_mul (ecc, v, z2z2, q + 2*ecc->p.size); /* z2^3 */
  ecc_modp_mul (ecc, s1, p + ecc->p.size, v);
  ecc_modp_mul (ecc, v, j, q + ecc->p.size);
  ecc_modp_sub (ecc, s2, v, s1);
  ecc_modp_mul_1 (ecc, s2, s2, 2);

  /* Store z3 */
  mpn_copyi (r + 2*ecc->p.size, i, ecc->p.size);

  /* i, j, v */
  ecc_modp_sqr (ecc, i, u2);
  ecc_modp_mul_1 (ecc, i, i, 4);
  ecc_modp_mul (ecc, j, u2, i);
  ecc_modp_mul (ecc, v, u1, i);

  /* now, u1, u2 and i are free for reuse .*/
  /* x3, use u1, u2 as scratch */
  ecc_modp_sqr (ecc, u1, s2);
  ecc_modp_sub (ecc, r, u1, j);
  ecc_modp_submul_1 (ecc, r, v, 2);

  /* y3 */
  ecc_modp_mul (ecc, u1, s1, j); /* Frees j */
  ecc_modp_sub (ecc, u2, v, r);  /* Frees v */
  ecc_modp_mul (ecc, i, s2, u2);
  ecc_modp_submul_1 (ecc, i, u1, 2);
  mpn_copyi (r + ecc->p.size, i, ecc->p.size);
}
