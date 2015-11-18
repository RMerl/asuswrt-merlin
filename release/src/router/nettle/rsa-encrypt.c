/* rsa-encrypt.c

   The RSA publickey algorithm. PKCS#1 encryption.

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

#include "pkcs1.h"

int
rsa_encrypt(const struct rsa_public_key *key,
	    /* For padding */
	    void *random_ctx, nettle_random_func *random,
	    size_t length, const uint8_t *message,
	    mpz_t gibberish)
{
  if (pkcs1_encrypt (key->size, random_ctx, random,
		     length, message, gibberish))
    {
      mpz_powm(gibberish, gibberish, key->e, key->n);
      return 1;
    }
  else
    return 0;
}
