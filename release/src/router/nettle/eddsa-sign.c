/* eddsa-sign.c

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

mp_size_t
_eddsa_sign_itch (const struct ecc_curve *ecc)
{
  return 5*ecc->p.size + ecc->mul_g_itch;
}

void
_eddsa_sign (const struct ecc_curve *ecc,
	     const struct nettle_hash *H,
	     const uint8_t *pub,
	     void *ctx,
	     const mp_limb_t *k2,
	     size_t length,
	     const uint8_t *msg,
	     uint8_t *signature,
	     mp_limb_t *scratch)
{
  mp_size_t size;
  size_t nbytes;
#define rp scratch
#define hp (scratch + size)
#define P (scratch + 2*size)
#define sp (scratch + 2*size)
#define hash ((uint8_t *) (scratch + 3*size))
#define scratch_out (scratch + 5*size)

  size = ecc->p.size;
  nbytes = 1 + ecc->p.bit_size / 8;

  assert (H->digest_size >= 2 * nbytes);

  H->update (ctx, length, msg);
  H->digest (ctx, 2*nbytes, hash);
  _eddsa_hash (&ecc->q, rp, hash);
  ecc->mul_g (ecc, P, rp, scratch_out);
  _eddsa_compress (ecc, signature, P, scratch_out);

  H->update (ctx, nbytes, signature);
  H->update (ctx, nbytes, pub);
  H->update (ctx, length, msg);
  H->digest (ctx, 2*nbytes, hash);
  _eddsa_hash (&ecc->q, hp, hash);

  ecc_modq_mul (ecc, sp, hp, k2);
  ecc_modq_add (ecc, sp, sp, rp); /* FIXME: Can be plain add */
  /* FIXME: Special code duplicated in ecc_25519_modq and ecc_eh_to_a.
     Define a suitable method? */
  {
    unsigned shift;
    mp_limb_t cy;
    assert (ecc->p.bit_size == 255);
    shift = 252 - GMP_NUMB_BITS * (ecc->p.size - 1);
    cy = mpn_submul_1 (sp, ecc->q.m, ecc->p.size,
		       sp[ecc->p.size-1] >> shift);
    assert (cy < 2);
    cnd_add_n (cy, sp, ecc->q.m, ecc->p.size);
  }
  mpn_get_base256_le (signature + nbytes, nbytes, sp, ecc->q.size);
#undef rp
#undef hp
#undef P
#undef sp
#undef hash
}
