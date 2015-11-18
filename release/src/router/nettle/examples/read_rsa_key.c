/* read_rsa_key.c

   Used by the rsa example programs.

   Copyright (C) 2002, 2007 Niels Möller

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

#include <stdlib.h>

#include "io.h"
#include "rsa.h"

/* Split out from io.c, since it depends on hogweed. */
int
read_rsa_key(const char *name,
	     struct rsa_public_key *pub,
	     struct rsa_private_key *priv)
{
  unsigned length;
  char *buffer;
  int res;
  
  length = read_file(name, 0, &buffer);
  if (!length)
    return 0;

  res = rsa_keypair_from_sexp(pub, priv, 0, length, buffer);
  free(buffer);

  return res;
}
