/* pkcs1-rsa-digest.c

   Copyright (C) 2001, 2003, 2012 Niels Möller

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

#include "pkcs1.h"

#include "bignum.h"
#include "gmp-glue.h"
#include "nettle-internal.h"

int
pkcs1_rsa_digest_encode(mpz_t m, size_t key_size,
			size_t di_length, const uint8_t *digest_info)
{
  TMP_GMP_DECL(em, uint8_t);

  TMP_GMP_ALLOC(em, key_size);

  if (_pkcs1_signature_prefix(key_size, em,
			      di_length, digest_info, 0))
    {
      nettle_mpz_set_str_256_u(m, key_size, em);
      TMP_GMP_FREE(em);
      return 1;
    }
  else
    {
      TMP_GMP_FREE(em);
      return 0;
    }
}
