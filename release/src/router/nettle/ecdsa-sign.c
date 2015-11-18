/* ecdsa-sign.c

   Copyright (C) 2013 Niels Möller

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
#include "nettle-internal.h"

void
ecdsa_sign (const struct ecc_scalar *key,
	    void *random_ctx, nettle_random_func *random,
	    size_t digest_length,
	    const uint8_t *digest,
	    struct dsa_signature *signature)
{
  /* At most 936 bytes. */
  TMP_DECL(k, mp_limb_t, ECC_MAX_SIZE + ECC_ECDSA_SIGN_ITCH (ECC_MAX_SIZE));
  mp_limb_t size = key->ecc->p.size;
  mp_limb_t *rp = mpz_limbs_write (signature->r, size);
  mp_limb_t *sp = mpz_limbs_write (signature->s, size);

  TMP_ALLOC (k, size + ECC_ECDSA_SIGN_ITCH (size));

  /* Timing reveals the number of rounds through this loop, but the
     timing is still independent of the secret k finally used. */
  do
    {
      ecc_mod_random (&key->ecc->q, k, random_ctx, random, k + size);
      ecc_ecdsa_sign (key->ecc, key->p, k, digest_length, digest,
		   rp, sp, k + size);
      mpz_limbs_finish (signature->r, size);
      mpz_limbs_finish (signature->s, size);
    }
  while (mpz_sgn (signature->r) == 0 || mpz_sgn (signature->s) == 0);
}
