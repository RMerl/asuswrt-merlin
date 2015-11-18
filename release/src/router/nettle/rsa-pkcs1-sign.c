/* rsa-pkcs1-sign.c

   PKCS#1 version 1.5 signatures.

   Copyright (C) 2012 Niels Möller

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
rsa_pkcs1_sign(const struct rsa_private_key *key,
	       size_t length, const uint8_t *digest_info,
	       mpz_t s)
{  
  if (pkcs1_rsa_digest_encode (s, key->size, length, digest_info))
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
