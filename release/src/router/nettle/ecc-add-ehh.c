/* ecc-add-ehh.c

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

#include "ecc.h"
#include "ecc-internal.h"

/* Add two points on an Edwards curve, in homogeneous coordinates */
void
ecc_add_ehh (const struct ecc_curve *ecc,
	     mp_limb_t *r, const mp_limb_t *p, const mp_limb_t *q,
	     mp_limb_t *scratch)
{
#define x1 p
#define y1 (p + ecc->p.size)
#define z1 (p + 2*ecc->p.size)

#define x2 q
#define y2 (q + ecc->p.size)
#define z2 (q + 2*ecc->p.size)

#define x3 r
#define y3 (r + ecc->p.size)
#define z3 (r + 2*ecc->p.size)

  /* Formulas (from djb,
     http://www.hyperelliptic.org/EFD/g1p/auto-edwards-projective.html#addition-add-2007-bl):

     Computation	Operation	Live variables

     C = x1*x2		mul		C
     D = y1*y2		mul		C, D
     T = (x1+y1)(x2+y2) - C - D, mul	C, D, T
     E = b*C*D		2 mul		C, E, T (Replace C <-- D - C)
     A = z1*z2		mul		A, C, E, T
     B = A^2		sqr		A, B, C, E, T
     F = B - E				A, B, C, E, F, T
     G = B + E     			A, C, F, G, T
     x3 = A*F*T		2 mul		A, C, G
     y3 = A*G*(D-C)	2 mul		F, G
     z3 = F*G		mul

     But when working with the twist curve, we have to negate the
     factor C = x1*x2. We change subtract to add in the y3
     expression, and swap F and G.
  */
#define C scratch
#define D (scratch + ecc->p.size)
#define T (scratch + 2*ecc->p.size)
#define E (scratch + 3*ecc->p.size) 
#define A (scratch + 4*ecc->p.size)
#define B (scratch + 5*ecc->p.size)
#define F D
#define G E

  ecc_modp_mul (ecc, C, x1, x2);
  ecc_modp_mul (ecc, D, y1, y2);
  ecc_modp_add (ecc, A, x1, y1);
  ecc_modp_add (ecc, B, x2, y2);
  ecc_modp_mul (ecc, T, A, B);
  ecc_modp_sub (ecc, T, T, C);
  ecc_modp_sub (ecc, T, T, D);
  ecc_modp_mul (ecc, x3, C, D);
  ecc_modp_mul (ecc, E, x3, ecc->b);
  ecc_modp_add (ecc, C, D, C);	/* ! */
  
  ecc_modp_mul (ecc, A, z1, z2);
  ecc_modp_sqr (ecc, B, A);

  ecc_modp_sub (ecc, F, B, E);
  ecc_modp_add (ecc, G, B, E);

  /* x3 */
  ecc_modp_mul (ecc, B, G, T); /* ! */
  ecc_modp_mul (ecc, x3, B, A);

  /* y3 */
  ecc_modp_mul (ecc, B, F, C); /* ! */
  ecc_modp_mul (ecc, y3, B, A);

  /* z3 */
  ecc_modp_mul (ecc, B, F, G);
  mpn_copyi (z3, B, ecc->p.size);
}
