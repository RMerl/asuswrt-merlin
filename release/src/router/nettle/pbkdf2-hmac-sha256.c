/* pbkdf2-hmac-sha256.c

   PKCS #5 PBKDF2 used with HMAC-SHA256, see RFC 2898.

   Copyright (C) 2012 Simon Josefsson

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

#include "pbkdf2.h"

#include "hmac.h"

void
pbkdf2_hmac_sha256 (size_t key_length, const uint8_t *key,
		    unsigned iterations,
		    size_t salt_length, const uint8_t *salt,
		    size_t length, uint8_t *dst)
{
  struct hmac_sha256_ctx sha256ctx;

  hmac_sha256_set_key (&sha256ctx, key_length, key);
  PBKDF2 (&sha256ctx, hmac_sha256_update, hmac_sha256_digest,
	  SHA256_DIGEST_SIZE, iterations, salt_length, salt, length, dst);
}
