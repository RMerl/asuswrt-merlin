/* rsa-sign.c

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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* string.h must be included before gmp.h */
#include "rsa.h"
#include "io.h"

int
main(int argc, char **argv)
{
  struct rsa_private_key key;
  struct sha1_ctx hash;
  mpz_t s;
  
  if (argc != 2)
    {
      werror("Usage: rsa-sign PRIVATE-KEY < file\n");
      return EXIT_FAILURE;
    }

  rsa_private_key_init(&key);
  
  if (!read_rsa_key(argv[1], NULL, &key))
    {
      werror("Invalid key\n");
      return EXIT_FAILURE;
    }

  sha1_init(&hash);
  if (!hash_file(&nettle_sha1, &hash, stdin))
    {
      werror("Failed reading stdin: %s\n",
	      strerror(errno));
      return 0;
    }

  mpz_init(s);
  if (!rsa_sha1_sign(&key, &hash, s))
    {
      werror("RSA key too small\n");
      return 0;
    }

  if (!mpz_out_str(stdout, 16, s))
    {
      werror("Failed writing signature: %s\n",
	      strerror(errno));
      return 0;
    }

  putchar('\n');
  
  mpz_clear(s);
  rsa_private_key_clear(&key);

  return EXIT_SUCCESS;
}
