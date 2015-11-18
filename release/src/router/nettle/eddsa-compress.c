/* eddsa-compress.c

   Copyright (C) 2014 Niels Möller

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
#include "gmp-glue.h"

mp_size_t
_eddsa_compress_itch (const struct ecc_curve *ecc)
{
  return 2*ecc->p.size + ecc->h_to_a_itch;
}

void
_eddsa_compress (const struct ecc_curve *ecc, uint8_t *r, mp_limb_t *p,
		 mp_limb_t *scratch)
{
#define xp scratch
#define yp (scratch + ecc->p.size)
#define scratch_out (scratch + 2*ecc->p.size)

  ecc->h_to_a (ecc, 0, xp, p, scratch_out);
  /* Encoding is the y coordinate and an appended "sign" bit, which is
     the low bit of x. Bit order is not specified explicitly, but for
     little-endian encoding, it makes most sense to append the bit
     after the most significant bit of y. */
  mpn_get_base256_le (r, 1 + ecc->p.bit_size / 8, yp, ecc->p.size);
  r[ecc->p.bit_size / 8] += (xp[0] & 1) << (ecc->p.bit_size & 7);
}
