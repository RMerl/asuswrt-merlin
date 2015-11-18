/* dsa.c

   The DSA publickey algorithm.

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

#include "dsa.h"

#include "bignum.h"

void
dsa_params_init (struct dsa_params *params)
{
  mpz_init(params->p);
  mpz_init(params->q);
  mpz_init(params->g);
}

void
dsa_params_clear (struct dsa_params *params)
{
  mpz_clear(params->p);
  mpz_clear(params->q);
  mpz_clear(params->g);
}

void
dsa_signature_init(struct dsa_signature *signature)
{
  mpz_init(signature->r);
  mpz_init(signature->s);
}

void
dsa_signature_clear(struct dsa_signature *signature)
{
  mpz_clear(signature->r);
  mpz_clear(signature->s);
}
