/* dsa-hash.c

   Copyright (C) 2013 Niels Möller

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

#include "dsa.h"

#include "bignum.h"

/* Convert hash value to an integer. The general description of DSA in
   FIPS186-3 allows both larger and smaller q; in the the latter case,
   the hash must be truncated to the right number of bits. */
void
_dsa_hash (mpz_t h, unsigned bit_size,
	   size_t length, const uint8_t *digest)
{
  
  if (length > (bit_size + 7) / 8)
    length = (bit_size + 7) / 8;

  nettle_mpz_set_str_256_u(h, length, digest);

  if (8 * length > bit_size)
    /* We got a few extra bits, at the low end. Discard them. */
    mpz_tdiv_q_2exp (h, h, 8*length - bit_size);
}
