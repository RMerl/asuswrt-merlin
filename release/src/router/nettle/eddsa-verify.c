/* eddsa-verify.c

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

#include <assert.h>

#include "eddsa.h"

#include "ecc.h"
#include "ecc-internal.h"
#include "nettle-meta.h"

/* Checks if x1/z1 == x2/z2 (mod p). Assumes z1 and z2 are
   non-zero. */
static int
equal_h (const struct ecc_modulo *p,
	 const mp_limb_t *x1, const mp_limb_t *z1,
	 const mp_limb_t *x2, const mp_limb_t *z2,
	 mp_limb_t *scratch)
{
#define t0 scratch
#define t1 (scratch + p->size)

  ecc_mod_mul (p, t0, x1, z2);
  if (mpn_cmp (t0, p->m, p->size) >= 0)
    mpn_sub_n (t0, t0, p->m, p->size);

  ecc_mod_mul (p, t1, x2, z1);
  if (mpn_cmp (t1, p->m, p->size) >= 0)
    mpn_sub_n (t1, t1, p->m, p->size);

  return mpn_cmp (t0, t1, p->size) == 0;

#undef t0
#undef t1
}

mp_size_t
_eddsa_verify_itch (const struct ecc_curve *ecc)
{
  return 8*ecc->p.size + ecc->mul_itch;
}

int
_eddsa_verify (const struct ecc_curve *ecc,
	       const struct nettle_hash *H,
	       const uint8_t *pub,
	       const mp_limb_t *A,
	       void *ctx,
	       size_t length,
	       const uint8_t *msg,
	       const uint8_t *signature,
	       mp_limb_t *scratch)
{
  size_t nbytes;
#define R scratch
#define sp (scratch + 2*ecc->p.size)
#define hp (scratch + 3*ecc->p.size)
#define P (scratch + 5*ecc->p.size)
#define scratch_out (scratch + 8*ecc->p.size)
#define S R
#define hash ((uint8_t *) P)

  nbytes = 1 + ecc->p.bit_size / 8;

  /* Could maybe save some storage by delaying the R and S operations,
     but it makes sense to check them for validity up front. */
  if (!_eddsa_decompress (ecc, R, signature, R+2*ecc->p.size))
    return 0;

  mpn_set_base256_le (sp, ecc->q.size, signature + nbytes, nbytes);
  /* Check that s < q */
  if (mpn_cmp (sp, ecc->q.m, ecc->q.size) >= 0)
    return 0;

  H->init (ctx);
  H->update (ctx, nbytes, signature);
  H->update (ctx, nbytes, pub);
  H->update (ctx, length, msg);
  H->digest (ctx, 2*nbytes, hash);
  _eddsa_hash (&ecc->q, hp, hash);

  /* Compute h A + R - s G, which should be the neutral point */
  ecc->mul (ecc, P, hp, A, scratch_out);
  ecc_add_eh (ecc, P, P, R, scratch_out);
  /* Move out of the way. */
  mpn_copyi (hp, sp, ecc->q.size);
  ecc->mul_g (ecc, S, hp, scratch_out);

  return equal_h (&ecc->p,
		   P, P + 2*ecc->p.size,
		   S, S + 2*ecc->p.size, scratch_out)
    && equal_h (&ecc->p,
		P + ecc->p.size, P + 2*ecc->p.size,
		S + ecc->p.size, S + 2*ecc->p.size, scratch_out);

#undef R
#undef sp
#undef hp
#undef P
#undef S
}
