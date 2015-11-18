/* eddsa-pubkey.c

   Copyright (C) 2015 Niels Möller

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

#include "eddsa.h"

#include "ecc-internal.h"

mp_size_t
_eddsa_public_key_itch (const struct ecc_curve *ecc)
{
  return 3*ecc->p.size + ecc->mul_g_itch;
}

void
_eddsa_public_key (const struct ecc_curve *ecc,
		   const mp_limb_t *k, uint8_t *pub, mp_limb_t *scratch)
{
#define P scratch
#define scratch_out (scratch + 3*ecc->p.size)
  ecc->mul_g (ecc, P, k, scratch_out);
  _eddsa_compress (ecc, pub, P, scratch_out);
#undef P
#undef scratch_out
}
