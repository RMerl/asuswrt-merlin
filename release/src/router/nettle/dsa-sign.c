/* dsa-sign.c

   The DSA publickey algorithm.

   Copyright (C) 2002, 2010 Niels Möller

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
#include <stdlib.h>

#include "dsa.h"

#include "bignum.h"


int
dsa_sign(const struct dsa_params *params,
	 const mpz_t x,
	 void *random_ctx, nettle_random_func *random,
	 size_t digest_size,
	 const uint8_t *digest,
	 struct dsa_signature *signature)
{
  mpz_t k;
  mpz_t h;
  mpz_t tmp;
  int res;
  
  /* Select k, 0<k<q, randomly */
  mpz_init_set(tmp, params->q);
  mpz_sub_ui(tmp, tmp, 1);

  mpz_init(k);
  nettle_mpz_random(k, random_ctx, random, tmp);
  mpz_add_ui(k, k, 1);

  /* Compute r = (g^k (mod p)) (mod q) */
  mpz_powm(tmp, params->g, k, params->p);
  mpz_fdiv_r(signature->r, tmp, params->q);

  /* Compute hash */
  mpz_init(h);
  _dsa_hash (h, mpz_sizeinbase(params->q, 2), digest_size, digest);

  /* Compute k^-1 (mod q) */
  if (mpz_invert(k, k, params->q))
    {
      /* Compute signature s = k^-1 (h + xr) (mod q) */
      mpz_mul(tmp, signature->r, x);
      mpz_fdiv_r(tmp, tmp, params->q);
      mpz_add(tmp, tmp, h);
      mpz_mul(tmp, tmp, k);
      mpz_fdiv_r(signature->s, tmp, params->q);
      res = 1;
    }
  else
    /* What do we do now? The key is invalid. */
    res = 0;

  mpz_clear(k);
  mpz_clear(h);
  mpz_clear(tmp);

  return res;
}
