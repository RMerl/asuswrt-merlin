/* dsa-keygen.c

   Generation of DSA keypairs

   Copyright (C) 2002, 2014 Niels Möller

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


/* Valid sizes, according to FIPS 186-3 are (1024, 160), (2048, 224),
   (2048, 256), (3072, 256). Currenty, we use only q_bits of 160 or
   256. */
void
dsa_generate_keypair (const struct dsa_params *params,
		      mpz_t pub, mpz_t key,

		      void *random_ctx, nettle_random_func *random)
{
  mpz_t r;

  mpz_init_set(r, params->q);
  mpz_sub_ui(r, r, 2);
  nettle_mpz_random(key, random_ctx, random, r);

  mpz_add_ui(key, key, 1);
  mpz_powm(pub, params->g, key, params->p);
  mpz_clear (r);
}
