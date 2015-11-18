/* pbkdf2.c

   PKCS #5 password-based key derivation function PBKDF2, see RFC 2898.

   Copyright (C) 2012 Simon Josefsson, Niels Möller

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

#include "pbkdf2.h"

#include "macros.h"
#include "memxor.h"
#include "nettle-internal.h"

void
pbkdf2 (void *mac_ctx,
	nettle_hash_update_func *update,
	nettle_hash_digest_func *digest,
	size_t digest_size, unsigned iterations,
	size_t salt_length, const uint8_t *salt,
	size_t length, uint8_t *dst)
{
  TMP_DECL(U, uint8_t, NETTLE_MAX_HASH_DIGEST_SIZE);
  TMP_DECL(T, uint8_t, NETTLE_MAX_HASH_DIGEST_SIZE);
  
  unsigned i;

  assert (iterations > 0);

  if (length == 0)
    return;

  TMP_ALLOC (U, digest_size);
  TMP_ALLOC (T, digest_size);

  for (i = 1;;
       i++, dst += digest_size, length -= digest_size)
    {
      uint8_t tmp[4];
      uint8_t *prev;
      unsigned u;
      
      WRITE_UINT32 (tmp, i);
      
      update (mac_ctx, salt_length, salt);
      update (mac_ctx, sizeof(tmp), tmp);
      digest (mac_ctx, digest_size, T);

      prev = T;
      
      for (u = 1; u < iterations; u++, prev = U)
	{
	  update (mac_ctx, digest_size, prev);
	  digest (mac_ctx, digest_size, U);

	  memxor (T, U, digest_size);
	}

      if (length <= digest_size)
	{
	  memcpy (dst, T, length);
	  return;
	}
      memcpy (dst, T, digest_size);
    }
}
