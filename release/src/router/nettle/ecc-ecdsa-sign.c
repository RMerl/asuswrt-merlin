/* ecc-ecdsa-sign.c

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
#include <stdlib.h>

#include "ecdsa.h"
#include "ecc-internal.h"

/* Low-level ECDSA signing */

mp_size_t
ecc_ecdsa_sign_itch (const struct ecc_curve *ecc)
{
  /* Needs 3*ecc->p.size + scratch for ecc->mul_g. Currently same for
     ecc_mul_g and ecc_mul_g_eh. */
  return ECC_ECDSA_SIGN_ITCH (ecc->p.size);
}

/* NOTE: Caller should check if r or s is zero. */
void
ecc_ecdsa_sign (const struct ecc_curve *ecc,
		const mp_limb_t *zp,
		/* Random nonce, must be invertible mod ecc group
		   order. */
		const mp_limb_t *kp,
		size_t length, const uint8_t *digest,
		mp_limb_t *rp, mp_limb_t *sp,
		mp_limb_t *scratch)
{
#define P	    scratch
#define kinv	    scratch                /* Needs 5*ecc->p.size for computation */
#define hp	    (scratch  + ecc->p.size) /* NOTE: ecc->p.size + 1 limbs! */
#define tp	    (scratch + 2*ecc->p.size)
  /* Procedure, according to RFC 6090, "KT-I". q denotes the group
     order.

     1. k <-- uniformly random, 0 < k < q

     2. R <-- (r_x, r_y) = k g

     3. s1 <-- r_x mod q

     4. s2 <-- (h + z*s1)/k mod q.
  */

  ecc->mul_g (ecc, P, kp, P + 3*ecc->p.size);
  /* x coordinate only, modulo q */
  ecc->h_to_a (ecc, 2, rp, P, P + 3*ecc->p.size);

  /* Invert k, uses 4 * ecc->p.size including scratch */
  ecc->q.invert (&ecc->q, kinv, kp, tp); /* NOTE: Also clobbers hp */
  
  /* Process hash digest */
  ecc_hash (&ecc->q, hp, length, digest);

  ecc_modq_mul (ecc, tp, zp, rp);
  ecc_modq_add (ecc, hp, hp, tp);
  ecc_modq_mul (ecc, tp, hp, kinv);

  mpn_copyi (sp, tp, ecc->p.size);
#undef P
#undef hp
#undef kinv
#undef tp
}
