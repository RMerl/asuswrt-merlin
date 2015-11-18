/* pkcs1-decrypt.c

   The RSA publickey algorithm. PKCS#1 decryption.

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

#include <string.h>

#include "pkcs1.h"

#include "bignum.h"
#include "gmp-glue.h"

int
pkcs1_decrypt (size_t key_size,
	       const mpz_t m,
	       size_t *length, uint8_t *message)
{
  TMP_GMP_DECL(em, uint8_t);
  uint8_t *terminator;
  size_t padding;
  size_t message_length;
  int ret;

  TMP_GMP_ALLOC(em, key_size);
  nettle_mpz_get_str_256(key_size, em, m);

  /* Check format */
  if (em[0] || em[1] != 2)
    {
      ret = 0;
      goto cleanup;
    }

  terminator = memchr(em + 2, 0, key_size - 2);

  if (!terminator)
    {
      ret = 0;
      goto cleanup;
    }
  
  padding = terminator - (em + 2);
  if (padding < 8)
    {
      ret = 0;
      goto cleanup;
    }

  message_length = key_size - 3 - padding;

  if (*length < message_length)
    {
      ret = 0;
      goto cleanup;
    }
  
  memcpy(message, terminator + 1, message_length);
  *length = message_length;

  ret = 1;
cleanup:
  TMP_GMP_FREE(em);
  return ret;
}
	       
