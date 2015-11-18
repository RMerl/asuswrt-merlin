/* dsa-compat-keygen.c

   Generation of DSA keypairs

   Copyright (C) 2002, 2014 Niels Möller

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

#include "dsa-compat.h"

#include "bignum.h"

/* Undo name mangling */
#undef dsa_generate_keypair
#define dsa_generate_keypair nettle_dsa_generate_keypair

/* Valid sizes, according to FIPS 186-3 are (1024, 160), (2048, 224),
   (2048, 256), (3072, 256). */
int
dsa_compat_generate_keypair(struct dsa_public_key *pub,
			    struct dsa_private_key *key,
			    void *random_ctx, nettle_random_func *random,
			    void *progress_ctx, nettle_progress_func *progress,
			    unsigned p_bits, unsigned q_bits)
{
  struct dsa_params *params;

  switch (q_bits)
    {
    case 160:
      if (p_bits < DSA_SHA1_MIN_P_BITS)
	return 0;
      break;
    case 224:
    case 256:
      if (p_bits < DSA_SHA256_MIN_P_BITS)
	return 0;
      break;
    default:
      return 0;
    }

  /* NOTE: Depends on identical layout! */
  params = (struct dsa_params *) pub;

  if (!dsa_generate_params (params,
			    random_ctx, random,
			    progress_ctx, progress,
			    p_bits, q_bits))
    return 0;

  dsa_generate_keypair (params, pub->y, key->x, random_ctx, random);

  return 1;
}
