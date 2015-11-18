/* rsa.c

   The RSA publickey algorithm.

   Copyright (C) 2001 Niels Möller

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
rsa_public_key_init(struct rsa_public_key *key)
{
  mpz_init(key->n);
  mpz_init(key->e);

  /* Not really necessary, but it seems cleaner to initialize all the
   * storage. */
  key->size = 0;
}

void
rsa_public_key_clear(struct rsa_public_key *key)
{
  mpz_clear(key->n);
  mpz_clear(key->e);
}

/* Computes the size, in octets, of a the modulo. Returns 0 if the
 * modulo is too small to be useful. */

size_t
_rsa_check_size(mpz_t n)
{
  /* Round upwards */
  size_t size = (mpz_sizeinbase(n, 2) + 7) / 8;

  if (size < RSA_MINIMUM_N_OCTETS)
    return 0;

  return size;
}

int
rsa_public_key_prepare(struct rsa_public_key *key)
{
  key->size = _rsa_check_size(key->n);
  
  return (key->size > 0);
}
