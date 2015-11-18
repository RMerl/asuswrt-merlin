/* pkcs1-rsa-md5.c

   PKCS stuff for rsa-md5.

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
 *   md5 OBJECT IDENTIFIER ::=
 *     {iso(1) member-body(2) US(840) rsadsi(113549) digestAlgorithm(2) 5}
 *
 * The parameters part of the algorithm identifier is NULL:
 *
 *   md5Identifier ::= AlgorithmIdentifier {md5, NULL}
 */

static const uint8_t
md5_prefix[] =
{
  /* 18 octets prefix, 16 octets hash, 34 total. */
  0x30,       32, /* SEQUENCE */
    0x30,     12, /* SEQUENCE */
      0x06,    8, /* OBJECT IDENTIFIER */
  	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x05,
      0x05,    0, /* NULL */
    0x04,     16  /* OCTET STRING */
      /* Here comes the raw hash value */
};

int
pkcs1_rsa_md5_encode(mpz_t m, size_t key_size, struct md5_ctx *hash)
{
  uint8_t *p;
  TMP_GMP_DECL(em, uint8_t);

  TMP_GMP_ALLOC(em, key_size);

  p = _pkcs1_signature_prefix(key_size, em,
			      sizeof(md5_prefix),
			      md5_prefix,
			      MD5_DIGEST_SIZE);
  if (p)
    {
      md5_digest(hash, MD5_DIGEST_SIZE, p);
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
pkcs1_rsa_md5_encode_digest(mpz_t m, size_t key_size, const uint8_t *digest)
{
  uint8_t *p;
  TMP_GMP_DECL(em, uint8_t);

  TMP_GMP_ALLOC(em, key_size);

  p = _pkcs1_signature_prefix(key_size, em,
			      sizeof(md5_prefix),
			      md5_prefix,
			      MD5_DIGEST_SIZE);
  if (p)
    {
      memcpy(p, digest, MD5_DIGEST_SIZE);
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
