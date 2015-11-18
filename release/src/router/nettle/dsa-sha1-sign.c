/* dsa-sha1-sign.c

   The original DSA publickey algorithm, using SHA-1.

   Copyright (C) 2010 Niels Möller

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

#include "dsa-compat.h"

int
dsa_sha1_sign_digest(const struct dsa_public_key *pub,
		     const struct dsa_private_key *key,
		     void *random_ctx, nettle_random_func *random,
		     const uint8_t *digest,
		     struct dsa_signature *signature)
{
  return dsa_sign((const struct dsa_params *) pub, key->x,
		  random_ctx, random,
		  SHA1_DIGEST_SIZE, digest, signature);
}


int
dsa_sha1_sign(const struct dsa_public_key *pub,
	      const struct dsa_private_key *key,
	      void *random_ctx, nettle_random_func *random,
	      struct sha1_ctx *hash,
	      struct dsa_signature *signature)
{
  uint8_t digest[SHA1_DIGEST_SIZE];
  sha1_digest(hash, sizeof(digest), digest);
  
  return dsa_sign((const struct dsa_params *) pub, key->x,
		  random_ctx, random,
		  sizeof(digest), digest, signature);
}
