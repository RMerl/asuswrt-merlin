/* rsa-verify.c

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

#include "rsa.h"
#include "io.h"

static int
read_signature(const char *name, mpz_t s)
{
  char *buffer;
  unsigned length;
  int res;
  
  length = read_file(name, 0, &buffer);
  if (!length)
    return 0;

  res = (mpz_set_str(s, buffer, 16) == 0);
  free(buffer);

  return res;
}

int
main(int argc, char **argv)
{
  struct rsa_public_key key;
  struct sha1_ctx hash;
  mpz_t s;
  
  if (argc != 3)
    {
      werror("Usage: rsa-verify PUBLIC-KEY SIGNATURE-FILE < FILE\n");
      return EXIT_FAILURE;
    }

  rsa_public_key_init(&key);
  
  if (!read_rsa_key(argv[1], &key, NULL))
    {
      werror("Invalid key\n");
      return EXIT_FAILURE;
    }

  mpz_init(s);

  if (!read_signature(argv[2], s))
    {
      werror("Failed to read signature file `%s'\n",
	      argv[2]);
      return EXIT_FAILURE;
    }
  
  sha1_init(&hash);
  if (!hash_file(&nettle_sha1, &hash, stdin))
    {
      werror("Failed reading stdin: %s\n",
	      strerror(errno));
      return 0;
    }

  if (!rsa_sha1_verify(&key, &hash, s))
    {
      werror("Invalid signature!\n");
      return EXIT_FAILURE;
    }
    
  mpz_clear(s);
  rsa_public_key_clear(&key);

  return EXIT_SUCCESS;
}
