/* ed25519-sha512-verify.c

   Copyright (C) 2014, 2015 Niels Möller

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

#include "eddsa.h"

#include "ecc-internal.h"
#include "sha2.h"

int
ed25519_sha512_verify (const uint8_t *pub,
		       size_t length, const uint8_t *msg,
		       const uint8_t *signature)
{
  const struct ecc_curve *ecc = &_nettle_curve25519;
  mp_size_t itch = 3*ecc->p.size + _eddsa_verify_itch (ecc);
  mp_limb_t *scratch = gmp_alloc_limbs (itch);
  struct sha512_ctx ctx;
  int res;
#define A scratch
#define scratch_out (scratch + 3*ecc->p.size)
  res = (_eddsa_decompress (ecc,
			    A, pub, scratch_out)
	 && _eddsa_verify (ecc, &nettle_sha512,
			   pub, A, &ctx,
			   length, msg, signature,
			   scratch_out));
  gmp_free_limbs (scratch, itch);
  return res;
#undef A
#undef scratch_out
}
