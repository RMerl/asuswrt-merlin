/* ecc-192.c

   Compile time constant (but machine dependent) tables.

   Copyright (C) 2013, 2014 Niels Möller

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

#include <assert.h>

/* FIXME: Remove ecc.h include, once prototypes of more internal
   functions are moved to ecc-internal.h */
#include "ecc.h"
#include "ecc-internal.h"

#define USE_REDC 0

#include "ecc-192.h"

#if HAVE_NATIVE_ecc_192_modp

#define ecc_192_modp nettle_ecc_192_modp
void
ecc_192_modp (const struct ecc_modulo *m, mp_limb_t *rp);

/* Use that p = 2^{192} - 2^64 - 1, to eliminate 128 bits at a time. */

#elif GMP_NUMB_BITS == 32
/* p is 6 limbs, p = B^6 - B^2 - 1 */
static void
ecc_192_modp (const struct ecc_modulo *m UNUSED, mp_limb_t *rp)
{
  mp_limb_t cy;

  /* Reduce from 12 to 9 limbs (top limb small)*/
  cy = mpn_add_n (rp + 2, rp + 2, rp + 8, 4);
  cy = sec_add_1 (rp + 6, rp + 6, 2, cy);
  cy += mpn_add_n (rp + 4, rp + 4, rp + 8, 4);
  assert (cy <= 2);

  rp[8] = cy;

  /* Reduce from 9 to 6 limbs */
  cy = mpn_add_n (rp, rp, rp + 6, 3);
  cy = sec_add_1 (rp + 3, rp + 3, 2, cy);
  cy += mpn_add_n (rp + 2, rp + 2, rp + 6, 3);
  cy = sec_add_1 (rp + 5, rp + 5, 1, cy);
  
  assert (cy <= 1);
  cy = cnd_add_n (cy, rp, ecc_Bmodp, 6);
  assert (cy == 0);  
}
#elif GMP_NUMB_BITS == 64
/* p is 3 limbs, p = B^3 - B - 1 */
static void
ecc_192_modp (const struct ecc_modulo *m UNUSED, mp_limb_t *rp)
{
  mp_limb_t cy;

  /* Reduce from 6 to 5 limbs (top limb small)*/
  cy = mpn_add_n (rp + 1, rp + 1, rp + 4, 2);
  cy = sec_add_1 (rp + 3, rp + 3, 1, cy);
  cy += mpn_add_n (rp + 2, rp + 2, rp + 4, 2);
  assert (cy <= 2);

  rp[4] = cy;

  /* Reduce from 5 to 4 limbs (high limb small) */
  cy = mpn_add_n (rp, rp, rp + 3, 2);
  cy = sec_add_1 (rp + 2, rp + 2, 1, cy);
  cy += mpn_add_n (rp + 1, rp + 1, rp + 3, 2);

  assert (cy <= 1);
  cy = cnd_add_n (cy, rp, ecc_Bmodp, 3);
  assert (cy == 0);  
}
  
#else
#define ecc_192_modp ecc_mod
#endif

const struct ecc_curve nettle_secp_192r1 =
{
  {
    192,
    ECC_LIMB_SIZE,
    ECC_BMODP_SIZE,
    ECC_REDC_SIZE,
    ECC_MOD_INV_ITCH (ECC_LIMB_SIZE),
    0,

    ecc_p,
    ecc_Bmodp,
    ecc_Bmodp_shifted,    
    ecc_redc_ppm1,
    ecc_pp1h,

    ecc_192_modp,
    ecc_192_modp,
    ecc_mod_inv,
    NULL,
  },
  {
    192,
    ECC_LIMB_SIZE,
    ECC_BMODQ_SIZE,
    0,
    ECC_MOD_INV_ITCH (ECC_LIMB_SIZE),
    0,

    ecc_q,
    ecc_Bmodq,
    ecc_Bmodq_shifted,
    NULL,
    ecc_qp1h,

    ecc_mod,
    ecc_mod,
    ecc_mod_inv,
    NULL,
  },
  
  USE_REDC,
  ECC_PIPPENGER_K,
  ECC_PIPPENGER_C,

  ECC_ADD_JJJ_ITCH (ECC_LIMB_SIZE),
  ECC_MUL_A_ITCH (ECC_LIMB_SIZE),
  ECC_MUL_G_ITCH (ECC_LIMB_SIZE),
  ECC_J_TO_A_ITCH (ECC_LIMB_SIZE),

  ecc_add_jjj,
  ecc_mul_a,
  ecc_mul_g,
  ecc_j_to_a,

  ecc_b,
  ecc_g,
  NULL,
  ecc_unit,
  ecc_table
};

