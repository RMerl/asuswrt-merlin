/* rsa-sign-tr.c

   Creating RSA signatures, with some additional checks.

   Copyright (C) 2001, 2015 Niels Möller
   Copyright (C) 2012 Nikos Mavrogiannopoulos

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

/* Blinds m, by computing c = m r^e (mod n), for a random r. Also
   returns the inverse (ri), for use by rsa_unblind. */
static void
rsa_blind (const struct rsa_public_key *pub,
	   void *random_ctx, nettle_random_func *random,
	   mpz_t c, mpz_t ri, const mpz_t m)
{
  mpz_t r;

  mpz_init(r);

  /* c = m*(r^e)
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
  mpz_mul(c, m, r);
  mpz_fdiv_r(c, c, pub->n);

  mpz_clear(r);
}

/* m = c ri mod n */
static void
rsa_unblind (const struct rsa_public_key *pub,
	     mpz_t m, const mpz_t ri, const mpz_t c)
{
  mpz_mul(m, c, ri);
  mpz_fdiv_r(m, m, pub->n);
}

/* Checks for any errors done in the RSA computation. That avoids
 * attacks which rely on faults on hardware, or even software MPI
 * implementation. */
int
rsa_compute_root_tr(const struct rsa_public_key *pub,
		    const struct rsa_private_key *key,
		    void *random_ctx, nettle_random_func *random,
		    mpz_t x, const mpz_t m)
{
  int res;
  mpz_t t, mb, xb, ri;

  mpz_init (mb);
  mpz_init (xb);
  mpz_init (ri);
  mpz_init (t);

  rsa_blind (pub, random_ctx, random, mb, ri, m);

  rsa_compute_root (key, xb, mb);

  mpz_powm(t, xb, pub->e, pub->n);
  res = (mpz_cmp(mb, t) == 0);

  if (res)
    rsa_unblind (pub, x, ri, xb);

  mpz_clear (mb);
  mpz_clear (xb);
  mpz_clear (ri);
  mpz_clear (t);

  return res;
}
