/* hmac.c

   HMAC message authentication code (RFC-2104).

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

#include <assert.h>
/* Needed for alloca on freebsd */
#include <stdlib.h>
#include <string.h>

#include "hmac.h"

#include "memxor.h"
#include "nettle-internal.h"

#define IPAD 0x36
#define OPAD 0x5c

void
hmac_set_key(void *outer, void *inner, void *state,
	     const struct nettle_hash *hash,
	     size_t key_length, const uint8_t *key)
{
  TMP_DECL(pad, uint8_t, NETTLE_MAX_HASH_BLOCK_SIZE);
  TMP_ALLOC(pad, hash->block_size);
  
  hash->init(outer);
  hash->init(inner);

  if (key_length > hash->block_size)
    {
      /* Reduce key to the algorithm's hash size. Use the area pointed
       * to by state for the temporary state. */

      TMP_DECL(digest, uint8_t, NETTLE_MAX_HASH_DIGEST_SIZE);
      TMP_ALLOC(digest, hash->digest_size);

      hash->init(state);
      hash->update(state, key_length, key);
      hash->digest(state, hash->digest_size, digest);

      key = digest;
      key_length = hash->digest_size;
    }

  assert(key_length <= hash->block_size);
  
  memset(pad, OPAD, hash->block_size);
  memxor(pad, key, key_length);

  hash->update(outer, hash->block_size, pad);

  memset(pad, IPAD, hash->block_size);
  memxor(pad, key, key_length);

  hash->update(inner, hash->block_size, pad);

  memcpy(state, inner, hash->context_size);
}

void
hmac_update(void *state,
	    const struct nettle_hash *hash,
	    size_t length, const uint8_t *data)
{
  hash->update(state, length, data);
}

void
hmac_digest(const void *outer, const void *inner, void *state,
	    const struct nettle_hash *hash, 	    
	    size_t length, uint8_t *dst)
{
  TMP_DECL(digest, uint8_t, NETTLE_MAX_HASH_DIGEST_SIZE);
  TMP_ALLOC(digest, hash->digest_size);

  hash->digest(state, hash->digest_size, digest);

  memcpy(state, outer, hash->context_size);

  hash->update(state, hash->digest_size, digest);
  hash->digest(state, length, dst);

  memcpy(state, inner, hash->context_size);
}
