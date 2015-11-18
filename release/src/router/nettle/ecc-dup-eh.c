/* ecc-dup-eh.c

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

/* Double a point on an Edwards curve, in homogeneous coordinates */
void
ecc_dup_eh (const struct ecc_curve *ecc,
	    mp_limb_t *r, const mp_limb_t *p,
	    mp_limb_t *scratch)
{
  /* Formulas (from djb,
     http://www.hyperelliptic.org/EFD/g1p/auto-edwards-projective.html#doubling-dbl-2007-bl):

     Computation	Operation	Live variables
     
     b = (x+y)^2	sqr		b
     c = x^2		sqr		b, c
     d = y^2		sqr		b, c, d
     e = c+d				b, c, d, e
     h = z^2		sqr		b, c, d, e, h
     j = e-2*h				b, c, d, e, j
     x' = (b-e)*j	mul		c, d, e, j
     y' = e*(c-d)	mul		e, j
     z' = e*j		mul

     But for the twisted curve, we need some sign changes.

     b = (x+y)^2	sqr		b
     c = x^2		sqr		b, c
     d = y^2		sqr		b, c, d
   ! e = -c+d				b, c, d, e
     h = z^2		sqr		b, c, d, e, h
   ! j = -e+2*h				b, c, d, e, j
   ! x' = (b-c-d)*j	mul		c, d, e, j
   ! y' = e*(c+d)	mul		e, j
     z' = e*j		mul
  */
#define b scratch 
#define c (scratch  + ecc->p.size)
#define d (scratch  + 2*ecc->p.size)
#define e (scratch  + 3*ecc->p.size)
#define j (scratch  + 4*ecc->p.size)

  /* b */
  ecc_modp_add (ecc, e, p, p + ecc->p.size);
  ecc_modp_sqr (ecc, b, e);

  /* c */
  ecc_modp_sqr (ecc, c, p);
  /* d */
  ecc_modp_sqr (ecc, d, p + ecc->p.size);
  /* h, can use r as scratch, even for in-place operation. */
  ecc_modp_sqr (ecc, r, p + 2*ecc->p.size);
  /* e, */
  ecc_modp_sub (ecc, e, d, c);
  /* b - c - d */
  ecc_modp_sub (ecc, b, b, c);
  ecc_modp_sub (ecc, b, b, d);
  /* j */
  ecc_modp_add (ecc, r, r, r);
  ecc_modp_sub (ecc, j, r, e);

  /* x' */
  ecc_modp_mul (ecc, r, b, j);
  /* y' */
  ecc_modp_add (ecc, c, c, d); /* Redundant */
  ecc_modp_mul (ecc, r + ecc->p.size, e, c);
  /* z' */
  ecc_modp_mul (ecc, b, e, j);
  mpn_copyi (r + 2*ecc->p.size, b, ecc->p.size);
}
