/* dsa-compat.c

   The DSA publickey algorithm, old interface.

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

#include "dsa-compat.h"

void
dsa_public_key_init(struct dsa_public_key *key)
{
  dsa_params_init ((struct dsa_params *) key);
  mpz_init(key->y);
}

void
dsa_public_key_clear(struct dsa_public_key *key)
{
  dsa_params_clear ((struct dsa_params *) key);
  mpz_clear(key->y);
}


void
dsa_private_key_init(struct dsa_private_key *key)
{
  mpz_init(key->x);
}

void
dsa_private_key_clear(struct dsa_private_key *key)
{
  mpz_clear(key->x);
}
