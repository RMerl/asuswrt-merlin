/* rsa-sign.c

   Creating RSA signatures.

   Copyright (C) 2001, 2003 Niels Möller

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

void
rsa_private_key_init(struct rsa_private_key *key)
{
  mpz_init(key->d);
  mpz_init(key->p);
  mpz_init(key->q);
  mpz_init(key->a);
  mpz_init(key->b);
  mpz_init(key->c);

  /* Not really necessary, but it seems cleaner to initialize all the
   * storage. */
  key->size = 0;
}

void
rsa_private_key_clear(struct rsa_private_key *key)
{
  mpz_clear(key->d);
  mpz_clear(key->p);
  mpz_clear(key->q);
  mpz_clear(key->a);
  mpz_clear(key->b);
  mpz_clear(key->c);
}

int
rsa_private_key_prepare(struct rsa_private_key *key)
{
  mpz_t n;
  
  /* The size of the product is the sum of the sizes of the factors,
   * or sometimes one less. It's possible but tricky to compute the
   * size without computing the full product. */

  mpz_init(n);
  mpz_mul(n, key->p, key->q);

  key->size = _rsa_check_size(n);

  mpz_clear(n);
  
  return (key->size > 0);
}

/* Computing an rsa root. */
void
rsa_compute_root(const struct rsa_private_key *key,
		 mpz_t x, const mpz_t m)
{
  mpz_t xp; /* modulo p */
  mpz_t xq; /* modulo q */

  mpz_init(xp); mpz_init(xq);    

  /* Compute xq = m^d % q = (m%q)^b % q */
  mpz_fdiv_r(xq, m, key->q);
  mpz_powm(xq, xq, key->b, key->q);

  /* Compute xp = m^d % p = (m%p)^a % p */
  mpz_fdiv_r(xp, m, key->p);
  mpz_powm(xp, xp, key->a, key->p);

  /* Set xp' = (xp - xq) c % p. */
  mpz_sub(xp, xp, xq);
  mpz_mul(xp, xp, key->c);
  mpz_fdiv_r(xp, xp, key->p);

  /* Finally, compute x = xq + q xp'
   *
   * To prove that this works, note that
   *
   *   xp  = x + i p,
   *   xq  = x + j q,
   *   c q = 1 + k p
   *
   * for some integers i, j and k. Now, for some integer l,
   *
   *   xp' = (xp - xq) c + l p
   *       = (x + i p - (x + j q)) c + l p
   *       = (i p - j q) c + l p
   *       = (i c + l) p - j (c q)
   *       = (i c + l) p - j (1 + kp)
   *       = (i c + l - j k) p - j
   *
   * which shows that xp' = -j (mod p). We get
   *
   *   xq + q xp' = x + j q + (i c + l - j k) p q - j q
   *              = x + (i c + l - j k) p q
   *
   * so that
   *
   *   xq + q xp' = x (mod pq)
   *
   * We also get 0 <= xq + q xp' < p q, because
   *
   *   0 <= xq < q and 0 <= xp' < p.
   */
  mpz_mul(x, key->q, xp);
  mpz_add(x, x, xq);

  mpz_clear(xp); mpz_clear(xq);
}
