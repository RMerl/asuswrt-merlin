/* curve25519-mul.c

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

/* Intended to be compatible with NaCl's crypto_scalarmult. */
void
curve25519_mul (uint8_t *q, const uint8_t *n, const uint8_t *p)
{
  const struct ecc_curve *ecc = &_nettle_curve25519;
  mp_size_t itch;
  mp_limb_t *scratch;
  int i;
  mp_limb_t cy;

  /* FIXME: Could save some more scratch space, e.g., by letting BB
     overlap C, D, and CB overlap A, D. And possibly reusing some of
     x2, z2, x3, z3. */
#define x1 scratch
#define x2 (scratch + ecc->p.size)
#define z2 (scratch + 2*ecc->p.size)
#define x3 (scratch + 3*ecc->p.size)
#define z3 (scratch + 4*ecc->p.size)

#define A  (scratch + 5*ecc->p.size)
#define B  (scratch + 6*ecc->p.size)
#define C  (scratch + 7*ecc->p.size)
#define D  (scratch + 8*ecc->p.size)
#define AA  (scratch + 9*ecc->p.size)
#define BB  (scratch +10*ecc->p.size)
#define E  (scratch + 10*ecc->p.size) /* Overlap BB */
#define DA  (scratch + 9*ecc->p.size) /* Overlap AA */
#define CB  (scratch + 10*ecc->p.size) /* Overlap BB */

  itch = ecc->p.size * 12;
  scratch = gmp_alloc_limbs (itch);

  mpn_set_base256_le (x1, ecc->p.size, p, CURVE25519_SIZE);

  /* Initialize, x2 = x1, z2 = 1 */
  mpn_copyi (x2, x1, ecc->p.size);
  z2[0] = 1;
  mpn_zero (z2+1, ecc->p.size - 1);

  /* Get x3, z3 from doubling. Since bit 254 is forced to 1. */
  ecc_modp_add (ecc, A, x2, z2);
  ecc_modp_sub (ecc, B, x2, z2);
  ecc_modp_sqr (ecc, AA, A);
  ecc_modp_sqr (ecc, BB, B);
  ecc_modp_mul (ecc, x3, AA, BB);
  ecc_modp_sub (ecc, E, AA, BB);
  ecc_modp_addmul_1 (ecc, AA, E, 121665);
  ecc_modp_mul (ecc, z3, E, AA);      

  for (i = 253; i >= 3; i--)
    {
      int bit = (n[i/8] >> (i & 7)) & 1;

      cnd_swap (bit, x2, x3, 2*ecc->p.size);

      /* Formulas from draft-turner-thecurve25519function-00-Mont. We
	 compute new coordinates in memory-address order, since mul
	 and sqr clobbers higher limbs. */
      ecc_modp_add (ecc, A, x2, z2);
      ecc_modp_sub (ecc, B, x2, z2);
      ecc_modp_sqr (ecc, AA, A);
      ecc_modp_sqr (ecc, BB, B);
      ecc_modp_mul (ecc, x2, AA, BB); /* Last use of BB */
      ecc_modp_sub (ecc, E, AA, BB);
      ecc_modp_addmul_1 (ecc, AA, E, 121665);
      ecc_modp_add (ecc, C, x3, z3);
      ecc_modp_sub (ecc, D, x3, z3);
      ecc_modp_mul (ecc, z2, E, AA); /* Last use of E and AA */
      ecc_modp_mul (ecc, DA, D, A);  /* Last use of D, A. FIXME: could
					let CB overlap. */
      ecc_modp_mul (ecc, CB, C, B);

      ecc_modp_add (ecc, C, DA, CB);
      ecc_modp_sqr (ecc, x3, C);
      ecc_modp_sub (ecc, C, DA, CB);
      ecc_modp_sqr (ecc, DA, C);
      ecc_modp_mul (ecc, z3, DA, x1);

      cnd_swap (bit, x2, x3, 2*ecc->p.size);
    }
  /* Do the 3 low zero bits, just duplicating x2 */
  for ( ; i >= 0; i--)
    {
      ecc_modp_add (ecc, A, x2, z2);
      ecc_modp_sub (ecc, B, x2, z2);
      ecc_modp_sqr (ecc, AA, A);
      ecc_modp_sqr (ecc, BB, B);
      ecc_modp_mul (ecc, x2, AA, BB);
      ecc_modp_sub (ecc, E, AA, BB);
      ecc_modp_addmul_1 (ecc, AA, E, 121665);
      ecc_modp_mul (ecc, z2, E, AA);      
    }
  ecc->p.invert (&ecc->p, x3, z2, z3 + ecc->p.size);
  ecc_modp_mul (ecc, z3, x2, x3);
  cy = mpn_sub_n (x2, z3, ecc->p.m, ecc->p.size);
  cnd_copy (cy, x2, z3, ecc->p.size);
  mpn_get_base256_le (q, CURVE25519_SIZE, x2, ecc->p.size);

  gmp_free_limbs (scratch, itch);
}
