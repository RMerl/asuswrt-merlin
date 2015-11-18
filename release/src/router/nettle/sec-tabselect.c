/* sec-tabselect.c

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

#include <assert.h>

#include "ecc-internal.h"

/* Copy the k'th element of the table out tn elements, each of size
   rn. Always read complete table. Similar to gmp's mpn_tabselect. */
/* FIXME: Should we need to volatile declare anything? */
void
sec_tabselect (mp_limb_t *rp, mp_size_t rn,
	       const mp_limb_t *table, unsigned tn,
	       unsigned k)
{
  const mp_limb_t *end = table + tn * rn;
  const mp_limb_t *p;
  mp_size_t i;
  
  assert (k < tn);
  mpn_zero (rp, rn);
  for (p = table; p < end; p += rn, k--)
    {
      mp_limb_t mask = - (mp_limb_t) (k == 0);
      for (i = 0; i < rn; i++)
	rp[i] += mask & p[i];
    }
}
