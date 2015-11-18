/* pkcs1-encrypt.c

   The RSA publickey algorithm. PKCS#1 encryption.

   Copyright (C) 2001, 2012 Niels Möller

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
#include <stdlib.h>
#include <string.h>

#include "pkcs1.h"

#include "bignum.h"
#include "gmp-glue.h"

int
pkcs1_encrypt (size_t key_size,
	       /* For padding */
	       void *random_ctx, nettle_random_func *random,
	       size_t length, const uint8_t *message,
	       mpz_t m)
{
  TMP_GMP_DECL(em, uint8_t);
  size_t padding;
  size_t i;

  /* The message is encoded as a string of the same length as the
   * modulo n, of the form
   *
   *   00 02 pad 00 message
   *
   * where padding should be at least 8 pseudorandomly generated
   * *non-zero* octets. */
     
  if (length + 11 > key_size)
    /* Message too long for this key. */
    return 0;

  /* At least 8 octets of random padding */
  padding = key_size - length - 3;
  assert(padding >= 8);
  
  TMP_GMP_ALLOC(em, key_size - 1);
  em[0] = 2;

  random(random_ctx, padding, em + 1);

  /* Replace 0-octets with 1 */
  for (i = 0; i<padding; i++)
    if (!em[i+1])
      em[i+1] = 1;

  em[padding+1] = 0;
  memcpy(em + padding + 2, message, length);

  nettle_mpz_set_str_256_u(m, key_size - 1, em);

  TMP_GMP_FREE(em);
  return 1;
}
