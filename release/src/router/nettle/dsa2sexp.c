/* dsa2sexp.c

   Copyright (C) 2002, 2009, 2014 Niels Möller, Magnus Holmgren

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

#include "sexp.h"

int
dsa_keypair_to_sexp(struct nettle_buffer *buffer,
		    const char *algorithm_name,
		    const struct dsa_params *params,
		    const mpz_t pub,
		    const mpz_t priv)
{
  if (!algorithm_name)
    algorithm_name = "dsa";

  if (priv)
    return sexp_format(buffer,
		       "(private-key(%0s(p%b)(q%b)"
		       "(g%b)(y%b)(x%b)))",
		       algorithm_name, params->p, params->q,
		       params->g, pub, priv);

  else
    return sexp_format(buffer,
		       "(public-key(%0s(p%b)(q%b)"
		       "(g%b)(y%b)))",
		       algorithm_name, params->p, params->q,
		       params->g, pub);
}
