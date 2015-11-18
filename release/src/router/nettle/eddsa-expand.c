/* eddsa-expand.c

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
#include <string.h>

#include "eddsa.h"

#include "ecc.h"
#include "ecc-internal.h"
#include "nettle-meta.h"

/* Expands a private key, generating the secret scalar K2 and leaving
   the key K1 for nonce generation, at the end of the digest. */
void
_eddsa_expand_key (const struct ecc_curve *ecc,
		   const struct nettle_hash *H,
		   void *ctx,
		   const uint8_t *key,
		   uint8_t *digest,
		   mp_limb_t *k2)
{
  size_t nbytes = 1 + ecc->p.bit_size / 8;

  assert (H->digest_size >= 2*nbytes);

  H->init (ctx);
  H->update (ctx, nbytes, key);
  H->digest (ctx, 2*nbytes, digest);

  mpn_set_base256_le (k2, ecc->p.size, digest, nbytes);
  /* Clear low 3 bits */
  k2[0] &= ~(mp_limb_t) 7;
  /* Set bit number bit_size - 1 (bit 254 for curve25519) */
  k2[(ecc->p.bit_size - 1) / GMP_NUMB_BITS]
    |= (mp_limb_t) 1 << ((ecc->p.bit_size - 1) % GMP_NUMB_BITS);
  /* Clear any higher bits. */
  k2[ecc->p.size - 1] &= ~(mp_limb_t) 0
    >> (GMP_NUMB_BITS * ecc->p.size - ecc->p.bit_size);
}
