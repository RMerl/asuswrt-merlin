/* ecc-add-eh.c

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

/* Add two points on an Edwards curve, with result and first point in
   homogeneous coordinates. */
void
ecc_add_eh (const struct ecc_curve *ecc,
	    mp_limb_t *r, const mp_limb_t *p, const mp_limb_t *q,
	    mp_limb_t *scratch)
{
#define x1 p
#define y1 (p + ecc->p.size)
#define z1 (p + 2*ecc->p.size)

#define x2 q
#define y2 (q + ecc->p.size)

#define x3 r
#define y3 (r + ecc->p.size)
#define z3 (r + 2*ecc->p.size)

  /* Formulas (from djb,
     http://www.hyperelliptic.org/EFD/g1p/auto-edwards-projective.html#doubling-dbl-2007-bl):

     Computation	Operation	Live variables

     C = x1*x2		mul		C
     D = y1*y2		mul		C, D
     T = (x1+y1)(x2+y2) - C - D		C, D, T
     E = b*C*D		2 mul		C, E, T  (Replace C <-- D - C)
     B = z1^2		sqr		B, C, E, T
     F = B - E				B, C, E, F, T
     G = B + E     			C, F, G, T
     x3 = z1*F*T	3 mul		C, F, G, T
     y3 = z1*G*(D-C)	2 mul		F, G
     z3 = F*G		mul
  */
#define C (scratch)
#define D (scratch + 1*ecc->p.size)
#define T (scratch + 2*ecc->p.size)
#define E (scratch + 3*ecc->p.size) 
#define B (scratch + 4*ecc->p.size)
#define F D
#define G E
  
  ecc_modp_mul (ecc, C, x1, x2);
  ecc_modp_mul (ecc, D, y1, y2);
  ecc_modp_add (ecc, x3, x1, y1);
  ecc_modp_add (ecc, y3, x2, y2);
  ecc_modp_mul (ecc, T, x3, y3);
  ecc_modp_sub (ecc, T, T, C);
  ecc_modp_sub (ecc, T, T, D);
  ecc_modp_mul (ecc, x3, C, D);
  ecc_modp_mul (ecc, E, x3, ecc->b);

  ecc_modp_add (ecc, C, D, C); /* ! */
  ecc_modp_sqr (ecc, B, z1);
  ecc_modp_sub (ecc, F, B, E);
  ecc_modp_add (ecc, G, B, E);  

  /* x3 */
  ecc_modp_mul (ecc, B, G, T); /* ! */
  ecc_modp_mul (ecc, x3, B, z1);

  /* y3 */
  ecc_modp_mul (ecc, B, F, C); /* ! */
  ecc_modp_mul (ecc, y3, B, z1);

  /* z3 */
  ecc_modp_mul (ecc, B, F, G);
  mpn_copyi (z3, B, ecc->p.size);
}
