/* ecc-ecdsa-verify.c

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

#include "gmp-glue.h"

int
ecdsa_verify (const struct ecc_point *pub,
	      size_t length, const uint8_t *digest,
	      const struct dsa_signature *signature)
{
  mp_limb_t size = ecc_size (pub->ecc);
  mp_size_t itch = 2*size + ecc_ecdsa_verify_itch (pub->ecc);
  /* For ECC_MUL_A_WBITS == 0, at most 1512 bytes. With
     ECC_MUL_A_WBITS == 4, currently needs 67 * ecc->size, at most
     4824 bytes. Don't use stack allocation for this. */
  mp_limb_t *scratch = gmp_alloc_limbs (itch);
  int res;

#define rp scratch
#define sp (scratch + size)
#define scratch_out (scratch + 2*size)

  if (mpz_sgn (signature->r) <= 0 || mpz_size (signature->r) > size
      || mpz_sgn (signature->s) <= 0 || mpz_size (signature->s) > size)
    return 0;
  
  mpz_limbs_copy (rp, signature->r, size);
  mpz_limbs_copy (sp, signature->s, size);

  res = ecc_ecdsa_verify (pub->ecc, pub->p, length, digest, rp, sp, scratch_out);

  gmp_free_limbs (scratch, itch);

  return res;
#undef rp
#undef sp
#undef scratch_out
}
