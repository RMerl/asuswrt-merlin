/* ecdsa-hash.c

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

/* Development of Nettle's ECC support was funded by the .SE Internet Fund. */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "ecc-internal.h"

/* Convert hash value to an integer. If the digest is larger than
   the ecc bit size, then we must truncate it and use the leftmost
   bits. */

/* NOTE: We don't considered the hash value to be secret, so it's ok
   if the running time of this conversion depends on h.

   Requires m->size + 1 limbs, the extra limb may be needed for
   unusual limb sizes.
*/

void
ecc_hash (const struct ecc_modulo *m,
	  mp_limb_t *hp,
	  size_t length, const uint8_t *digest)
{
  if (length > ((size_t) m->bit_size + 7) / 8)
    length = (m->bit_size + 7) / 8;

  mpn_set_base256 (hp, m->size + 1, digest, length);

  if (8 * length > m->bit_size)
    /* We got a few extra bits, at the low end. Discard them. */
    mpn_rshift (hp, hp, m->size + 1, 8*length - m->bit_size);
}
