/* rsa2openpgp.c

   Converting rsa keys to OpenPGP format.

   Copyright (C) 2001, 2002 Niels Möller

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
#include <time.h>

#include "rsa.h"

#include "buffer.h"
#include "pgp.h"


/* According to RFC 2440, a public key consists of the following packets:
 *
 * Public key packet
 *
 * Zero or more revocation signatures
 *
 * One or more User ID packets
 *
 * After each User ID packet, zero or more signature packets
 *
 * Zero or more Subkey packets
 *
 * After each Subkey packet, one signature packet, optionally a
 * revocation.
 *
 * Currently, we generate a public key packet, a single user id, and a
 * signature. */

int
rsa_keypair_to_openpgp(struct nettle_buffer *buffer,
		       const struct rsa_public_key *pub,
		       const struct rsa_private_key *priv,
		       /* A single user id. NUL-terminated utf8. */
		       const char *userid)
{
  time_t now = time(NULL);

  unsigned key_start;
  unsigned userid_start;
  
  struct sha1_ctx key_hash;
  struct sha1_ctx signature_hash;
  uint8_t fingerprint[SHA1_DIGEST_SIZE];
  
  key_start = buffer->size;
  
  if (!pgp_put_public_rsa_key(buffer, pub, now))
    return 0;

  /* userid packet */
  userid_start = buffer->size;
  if (!pgp_put_userid(buffer, strlen(userid), userid))
    return 0;

  /* FIXME: We hash the key first, and then the user id. Is this right? */
  sha1_init(&key_hash);
  sha1_update(&key_hash,
	      userid_start - key_start,
	      buffer->contents + key_start);

  signature_hash = key_hash;
  sha1_digest(&key_hash, sizeof(fingerprint), fingerprint);

  sha1_update(&signature_hash,
	      buffer->size - userid_start,
	      buffer->contents + userid_start);
  
  return pgp_put_rsa_sha1_signature(buffer,
				    priv,
				    fingerprint + SHA1_DIGEST_SIZE - 8,
				    PGP_SIGN_CERTIFICATION,
				    &signature_hash);
}
