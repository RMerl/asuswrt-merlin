/* ecc-521.c

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

#include "ecc.h"
#include "ecc-internal.h"

#define USE_REDC 0

#include "ecc-521.h"

#if HAVE_NATIVE_ecc_521_modp
#define ecc_521_modp nettle_ecc_521_modp
void
ecc_521_modp (const struct ecc_modulo *m, mp_limb_t *rp);

#else

#define B_SHIFT (521 % GMP_NUMB_BITS)
#define BMODP_SHIFT (GMP_NUMB_BITS - B_SHIFT)
#define BMODP ((mp_limb_t) 1 << BMODP_SHIFT)

/* Result may be *slightly* larger than 2^521 */
static void
ecc_521_modp (const struct ecc_modulo *m UNUSED, mp_limb_t *rp)
{
  /* FIXME: Should use mpn_addlsh_n_ip1 */
  mp_limb_t hi;
  /* Reduce from 2*ECC_LIMB_SIZE to ECC_LIMB_SIZE + 1 */
  rp[ECC_LIMB_SIZE]
    = mpn_addmul_1 (rp, rp + ECC_LIMB_SIZE, ECC_LIMB_SIZE, BMODP);
  hi = mpn_addmul_1 (rp, rp + ECC_LIMB_SIZE, 1, BMODP);
  hi = sec_add_1 (rp + 1, rp + 1, ECC_LIMB_SIZE - 1, hi);

  /* Combine hi with top bits, and add in. */
  hi = (hi << BMODP_SHIFT) | (rp[ECC_LIMB_SIZE-1] >> B_SHIFT);
  rp[ECC_LIMB_SIZE-1] = (rp[ECC_LIMB_SIZE-1]
			 & (((mp_limb_t) 1 << B_SHIFT)-1))
    + sec_add_1 (rp, rp, ECC_LIMB_SIZE - 1, hi);
}
#endif

const struct ecc_curve nettle_secp_521r1 =
{
  {
    521,
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

    ecc_521_modp,
    ecc_521_modp,
    ecc_mod_inv,
    NULL,
  },
  {
    521,
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

