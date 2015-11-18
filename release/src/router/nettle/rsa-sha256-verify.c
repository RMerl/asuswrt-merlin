/* rsa-sha256-verify.c

   Verifying signatures created with RSA and SHA256.

   Copyright (C) 2001, 2003, 2006 Niels Möller

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

#include "rsa.h"

#include "bignum.h"
#include "pkcs1.h"

int
rsa_sha256_verify(const struct rsa_public_key *key,
		  struct sha256_ctx *hash,
		  const mpz_t s)
{
  int res;
  mpz_t m;

  mpz_init(m);

  res = (pkcs1_rsa_sha256_encode(m, key->size, hash)
	 &&_rsa_verify(key, m, s));
  
  mpz_clear(m);

  return res;
}

int
rsa_sha256_verify_digest(const struct rsa_public_key *key,
			 const uint8_t *digest,
			 const mpz_t s)
{
  int res;
  mpz_t m;

  mpz_init(m);
  
  res = (pkcs1_rsa_sha256_encode_digest(m, key->size, digest)
	 && _rsa_verify(key, m, s));
  
  mpz_clear(m);

  return res;
}
