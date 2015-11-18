/* sexp2bignum.c

   Copyright (C) 2002 Niels Möller

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

#include "sexp.h"
#include "bignum.h"

int
nettle_mpz_set_sexp(mpz_t x, unsigned limit, struct sexp_iterator *i)
{
  if (i->type == SEXP_ATOM
      && i->atom_length
      && !i->display)
    {
      /* Allow some extra here, for leading sign octets. */
      if (limit && (8 * i->atom_length > (16 + limit)))
	return 0;
      
      nettle_mpz_set_str_256_s(x, i->atom_length, i->atom);

      /* FIXME: How to interpret a limit for negative numbers? */
      if (limit && mpz_sizeinbase(x, 2) > limit)
	return 0;
      
      return sexp_iterator_next(i);
    }
  else
    return 0;
}
