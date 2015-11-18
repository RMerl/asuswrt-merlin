/* rsa-blind.c

   RSA blinding. Used for resistance to timing-attacks.

   Copyright (C) 2001, 2012 Niels Möller, Nikos Mavrogiannopoulos

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

#include "rsa.h"

#include "bignum.h"

/* Blinds the c, by computing c *= r^e (mod n), for a random r. Also
   returns the inverse (ri), for use by rsa_unblind. */
void
_rsa_blind (const struct rsa_public_key *pub,
	    void *random_ctx, nettle_random_func *random,
	    mpz_t c, mpz_t ri)
{
  mpz_t r;

  mpz_init(r);

  /* c = c*(r^e)
   * ri = r^(-1)
   */
  do 
    {
      nettle_mpz_random(r, random_ctx, random, pub->n);
      /* invert r */
    }
  while (!mpz_invert (ri, r, pub->n));

  /* c = c*(r^e) mod n */
  mpz_powm(r, r, pub->e, pub->n);
  mpz_mul(c, c, r);
  mpz_fdiv_r(c, c, pub->n);

  mpz_clear(r);
}

/* c *= ri mod n */
void
_rsa_unblind (const struct rsa_public_key *pub, mpz_t c, const mpz_t ri)
{
  mpz_mul(c, c, ri);
  mpz_fdiv_r(c, c, pub->n);
}
