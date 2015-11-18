/* rsa-sha512-sign.c

   Signatures using RSA and SHA512.

   Copyright (C) 2001, 2003, 2006, 2010 Niels Möller

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
rsa_sha512_sign(const struct rsa_private_key *key,
		struct sha512_ctx *hash,
		mpz_t s)
{
  if (pkcs1_rsa_sha512_encode(s, key->size, hash))
    {
      rsa_compute_root(key, s, s);
      return 1;
    }
  else
    {
      mpz_set_ui(s, 0);
      return 0;
    }  
}

int
rsa_sha512_sign_digest(const struct rsa_private_key *key,
		       const uint8_t *digest,
		       mpz_t s)
{
  if (pkcs1_rsa_sha512_encode_digest(s, key->size, digest))
    {
      rsa_compute_root(key, s, s);
      return 1;
    }
  else
    {
      mpz_set_ui(s, 0);
      return 0;
    }  
}
