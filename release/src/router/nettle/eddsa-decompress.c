/* eddsa-decompress.c

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

#include "eddsa.h"

#include "ecc-internal.h"
#include "gmp-glue.h"

mp_size_t
_eddsa_decompress_itch (const struct ecc_curve *ecc)
{
  return 4*ecc->p.size + ecc->p.sqrt_itch;
}

int
_eddsa_decompress (const struct ecc_curve *ecc, mp_limb_t *p,
		   const uint8_t *cp,
		   mp_limb_t *scratch)
{
  mp_limb_t sign, cy;
  int res;

#define xp p
#define yp (p + ecc->p.size)

#define y2 scratch
#define vp (scratch + ecc->p.size)
#define up scratch
#define tp (scratch + 2*ecc->p.size)
#define scratch_out (scratch + 4*ecc->p.size)

  sign = cp[ecc->p.bit_size / 8] >> (ecc->p.bit_size & 7);
  if (sign > 1)
    return 0;
  mpn_set_base256_le (yp, ecc->p.size, cp, 1 + ecc->p.bit_size / 8);
  /* Clear out the sign bit (if it fits) */
  yp[ecc->p.size - 1] &= ~(mp_limb_t) 0
    >> (ecc->p.size * GMP_NUMB_BITS - ecc->p.bit_size);
  ecc_modp_sqr (ecc, y2, yp);
  ecc_modp_mul (ecc, vp, y2, ecc->b);
  ecc_modp_sub (ecc, vp, vp, ecc->unit);
  ecc_modp_sub (ecc, up, ecc->unit, y2);
  res = ecc->p.sqrt (&ecc->p, tp, up, vp, scratch_out);

  cy = mpn_sub_n (xp, tp, ecc->p.m, ecc->p.size);
  cnd_copy (cy, xp, tp, ecc->p.size);
  sign ^= xp[0] & 1;
  mpn_sub_n (tp, ecc->p.m, xp, ecc->p.size);
  cnd_copy (sign, xp, tp, ecc->p.size);
  return res;
}
