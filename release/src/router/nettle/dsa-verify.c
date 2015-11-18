/* dsa-verify.c

   The DSA publickey algorithm.

   Copyright (C) 2002, 2003 Niels Möller

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

#include <stdlib.h>

#include "dsa.h"

#include "bignum.h"

int
dsa_verify(const struct dsa_params *params,
	   const mpz_t y,
	   size_t digest_size,
	   const uint8_t *digest,
	   const struct dsa_signature *signature)
{
  mpz_t w;
  mpz_t tmp;
  mpz_t v;

  int res;

  /* Check that r and s are in the proper range */
  if (mpz_sgn(signature->r) <= 0 || mpz_cmp(signature->r, params->q) >= 0)
    return 0;

  if (mpz_sgn(signature->s) <= 0 || mpz_cmp(signature->s, params->q) >= 0)
    return 0;

  mpz_init(w);

  /* Compute w = s^-1 (mod q) */

  /* NOTE: In gmp-2, mpz_invert sometimes generates negative inverses,
   * so we need gmp-3 or better. */
  if (!mpz_invert(w, signature->s, params->q))
    {
      mpz_clear(w);
      return 0;
    }

  mpz_init(tmp);
  mpz_init(v);

  /* The message digest */
  _dsa_hash (tmp, mpz_sizeinbase (params->q, 2), digest_size, digest);
  
  /* v = g^{w * h (mod q)} (mod p)  */
  mpz_mul(tmp, tmp, w);
  mpz_fdiv_r(tmp, tmp, params->q);

  mpz_powm(v, params->g, tmp, params->p);

  /* y^{w * r (mod q) } (mod p) */
  mpz_mul(tmp, signature->r, w);
  mpz_fdiv_r(tmp, tmp, params->q);

  mpz_powm(tmp, y, tmp, params->p);

  /* v = (g^{w * h} * y^{w * r} (mod p) ) (mod q) */
  mpz_mul(v, v, tmp);
  mpz_fdiv_r(v, v, params->p);

  mpz_fdiv_r(v, v, params->q);

  res = !mpz_cmp(v, signature->r);

  mpz_clear(w);
  mpz_clear(tmp);
  mpz_clear(v);

  return res;
}
