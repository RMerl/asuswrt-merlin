/* ecc-ecdsa-verify.c

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

/* Low-level ECDSA verify */

/* FIXME: Use mpn_zero_p. */
static int
zero_p (const mp_limb_t *xp, mp_size_t n)
{
  while (n > 0)
    if (xp[--n] > 0)
      return 0;
  return 1;
}

static int
ecdsa_in_range (const struct ecc_curve *ecc, const mp_limb_t *xp)
{
  return !zero_p (xp, ecc->p.size)
    && mpn_cmp (xp, ecc->q.m, ecc->p.size) < 0;
}

mp_size_t
ecc_ecdsa_verify_itch (const struct ecc_curve *ecc)
{
  /* Largest storage need is for the ecc->mul call. */
  return 5*ecc->p.size + ecc->mul_itch;
}

/* FIXME: Use faster primitives, not requiring side-channel silence. */
int
ecc_ecdsa_verify (const struct ecc_curve *ecc,
		  const mp_limb_t *pp, /* Public key */
		  size_t length, const uint8_t *digest,
		  const mp_limb_t *rp, const mp_limb_t *sp,
		  mp_limb_t *scratch)
{
  /* Procedure, according to RFC 6090, "KT-I". q denotes the group
     order.

     1. Check 0 < r, s < q.

     2. s' <-- s^{-1}  (mod q)

     3. u1  <-- h * s' (mod q)

     4. u2  <-- r * s' (mod q)

     5. R = u1 G + u2 Y

     6. Signature is valid if R_x = r (mod q).
  */

#define P2 scratch
#define u1 (scratch + 3*ecc->p.size)
#define u2 (scratch + 4*ecc->p.size)

#define P1 (scratch + 4*ecc->p.size)
#define sinv (scratch)
#define hp (scratch + ecc->p.size)

  if (! (ecdsa_in_range (ecc, rp)
	 && ecdsa_in_range (ecc, sp)))
    return 0;

  /* FIXME: Micro optimizations: Either simultaneous multiplication.
     Or convert to projective coordinates (can be done without
     division, I think), and write an ecc_add_ppp. */

  /* Compute sinv */
  ecc->q.invert (&ecc->q, sinv, sp, sinv + 2*ecc->p.size);

  /* u1 = h / s, P1 = u1 * G */
  ecc_hash (&ecc->q, hp, length, digest);
  ecc_modq_mul (ecc, u1, hp, sinv);

  /* u2 = r / s, P2 = u2 * Y */
  ecc_modq_mul (ecc, u2, rp, sinv);

   /* Total storage: 5*ecc->p.size + ecc->mul_itch */
  ecc->mul (ecc, P2, u2, pp, u2 + ecc->p.size);

  /* u = 0 can happen only if h = 0 or h = q, which is extremely
     unlikely. */
  if (!zero_p (u1, ecc->p.size))
    {
      /* Total storage: 7*ecc->p.size + ecc->mul_g_itch (ecc->p.size) */
      ecc->mul_g (ecc, P1, u1, P1 + 3*ecc->p.size);

      /* NOTE: ecc_add_jjj and/or ecc_j_to_a will produce garbage in
	 case u1 G = +/- u2 V. However, anyone who gets his or her
	 hands on a signature where this happens during verification,
	 can also get the private key as z = +/- u1 / u_2 (mod q). And
	 then it doesn't matter very much if verification of
	 signatures with that key succeeds or fails.

	 u1 G = - u2 V can never happen for a correctly generated
	 signature, since it implies k = 0.

	 u1 G = u2 V is possible, if we are unlucky enough to get h /
	 s_1 = z. Hitting that is about as unlikely as finding the
	 private key by guessing.
       */
      /* Total storage: 6*ecc->p.size + ecc->add_hhh_itch */
      ecc->add_hhh (ecc, P1, P1, P2, P1 + 3*ecc->p.size);
    }
  /* x coordinate only, modulo q */
  ecc->h_to_a (ecc, 2, P2, P1, P1 + 3*ecc->p.size);

  return (mpn_cmp (rp, P2, ecc->p.size) == 0);
#undef P2
#undef P1
#undef sinv
#undef u2
#undef hp
#undef u1
}
