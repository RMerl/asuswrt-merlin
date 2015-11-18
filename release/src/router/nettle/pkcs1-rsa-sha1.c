/* pkcs1-rsa-sha1.c

   PKCS stuff for rsa-sha1.

   Copyright (C) 2001, 2003 Niels Möller

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

#include "rsa.h"

#include "bignum.h"
#include "pkcs1.h"

#include "gmp-glue.h"

/* From pkcs-1v2
 *
 *   id-sha1 OBJECT IDENTIFIER ::=
 *     {iso(1) identified-organization(3) oiw(14) secsig(3)
 *   	 algorithms(2) 26}
 *   
 *   The default hash function is SHA-1: 
 *   sha1Identifier ::= AlgorithmIdentifier {id-sha1, NULL}
 */

static const uint8_t
sha1_prefix[] =
{
  /* 15 octets prefix, 20 octets hash, total 35 */
  0x30,       33, /* SEQUENCE */
    0x30,      9, /* SEQUENCE */
      0x06,    5, /* OBJECT IDENTIFIER */
  	  0x2b, 0x0e, 0x03, 0x02, 0x1a,
      0x05,    0, /* NULL */
    0x04,     20  /* OCTET STRING */
      /* Here comes the raw hash value */
};

int
pkcs1_rsa_sha1_encode(mpz_t m, size_t key_size, struct sha1_ctx *hash)
{
  uint8_t *p;
  TMP_GMP_DECL(em, uint8_t);

  TMP_GMP_ALLOC(em, key_size);

  p = _pkcs1_signature_prefix(key_size, em,
			      sizeof(sha1_prefix),
			      sha1_prefix,
			      SHA1_DIGEST_SIZE);
  if (p)
    {
      sha1_digest(hash, SHA1_DIGEST_SIZE, p);
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

int
pkcs1_rsa_sha1_encode_digest(mpz_t m, size_t key_size, const uint8_t *digest)
{
  uint8_t *p;
  TMP_GMP_DECL(em, uint8_t);

  TMP_GMP_ALLOC(em, key_size);

  p = _pkcs1_signature_prefix(key_size, em,
			      sizeof(sha1_prefix),
			      sha1_prefix,
			      SHA1_DIGEST_SIZE);
  if (p)
    {
      memcpy(p, digest, SHA1_DIGEST_SIZE);
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
