/* ecc-size.c

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

#include "ecc.h"
#include "ecc-internal.h"

unsigned
ecc_bit_size (const struct ecc_curve *ecc)
{
  return ecc->p.bit_size;
}

mp_size_t
ecc_size (const struct ecc_curve *ecc)
{
  return ecc->p.size;
}

mp_size_t
ecc_size_a (const struct ecc_curve *ecc)
{
  return 2*ecc->p.size;
}

mp_size_t
ecc_size_j (const struct ecc_curve *ecc)
{
  return 3*ecc->p.size;
}
