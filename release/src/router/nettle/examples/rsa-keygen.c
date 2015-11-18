/* rsa-keygen.c

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

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "buffer.h"
#include "rsa.h"
#include "sexp.h"
#include "yarrow.h"

#include "io.h"

#include "getopt.h"

#define DEFAULT_KEYSIZE 2048
#define ESIZE 30

static void
progress(void *ctx, int c)
{
  (void) ctx;
  fputc(c, stderr);
}

static unsigned long
uint_arg (char c, const char *arg)
{
  unsigned long val;
  char *end;

  val = strtoul(arg, &end, 0);
  if (*arg == '\0' || *end != '\0')
    {
      werror ("Invalid integer argument for -%c option.\n", c);
      exit (EXIT_FAILURE);
    }

  return val;      
}

int
main(int argc, char **argv)
{
  struct yarrow256_ctx yarrow;
  struct rsa_public_key pub;
  struct rsa_private_key priv;

  int c;
  char *pub_name = NULL;
  const char *priv_name = NULL;
  const char *random_name = NULL;
  
  struct nettle_buffer pub_buffer;
  struct nettle_buffer priv_buffer;

  unsigned long key_size = DEFAULT_KEYSIZE;
  unsigned long key_e = 0;

  enum { OPT_HELP = 300 };
  static const struct option options[] =
    {
      /* Name, args, flag, val */
      { "help", no_argument, NULL, OPT_HELP },
      { "random", required_argument, NULL, 'r' },
      { NULL, 0, NULL, 0}
    };
  
  while ( (c = getopt_long(argc, argv, "o:r:e:s:", options, NULL)) != -1)
    switch (c)
      {	
      case 'o':
	priv_name = optarg;
	break;

      case 'r':
	random_name = optarg;
	break;

      case 's':
	key_size = uint_arg ('s', optarg);
	break;

      case 'e':
	key_e = uint_arg ('e', optarg);
	break;

      case OPT_HELP:
	printf("FIXME: Usage information.\n");
	return EXIT_SUCCESS;

      case '?':
	return EXIT_FAILURE;

      default:
	abort();
      }

  if (!priv_name)
    {
      werror("No filename provided.\n");
      return EXIT_FAILURE;
    }

  pub_name = xalloc(strlen(priv_name) + 5);  
  sprintf(pub_name, "%s.pub", priv_name);

  /* NOTE: No sources */
  yarrow256_init(&yarrow, 0, NULL);

  /* Read some data to seed the generator */
  if (!simple_random(&yarrow, random_name))
    {
      werror("Initialization of randomness generator failed.\n");
      return EXIT_FAILURE;
    }

  rsa_public_key_init(&pub);
  rsa_private_key_init(&priv);

  if (key_e)
    mpz_set_ui (pub.e, key_e);

  if (!rsa_generate_keypair
      (&pub, &priv,
       (void *) &yarrow, (nettle_random_func *) yarrow256_random,
       NULL, progress,
       key_size, key_e == 0 ? ESIZE : 0))
    {
      werror("Key generation failed.\n");
      return EXIT_FAILURE;
    }

  nettle_buffer_init(&priv_buffer);
  nettle_buffer_init(&pub_buffer);
  
  if (!rsa_keypair_to_sexp(&pub_buffer, "rsa-pkcs1-sha1", &pub, NULL))
    {
      werror("Formatting public key failed.\n");
      return EXIT_FAILURE;
    }

  if (!rsa_keypair_to_sexp(&priv_buffer, "rsa-pkcs1-sha1", &pub, &priv))
    {
      werror("Formatting private key failed.\n");
      return EXIT_FAILURE;
    }
  
  if (!write_file(pub_name, pub_buffer.size, pub_buffer.contents))
    {
      werror("Failed to write public key: %s\n",
	      strerror(errno));
      return EXIT_FAILURE;
    }

  /* NOTE: This doesn't set up paranoid access restrictions on the
   * private key file, like a serious key generation tool would do. */
  if (!write_file(priv_name, priv_buffer.size, priv_buffer.contents))
    {
      werror("Failed to write private key: %s\n",
	      strerror(errno));
      return EXIT_FAILURE;
    }

  nettle_buffer_clear(&priv_buffer);
  nettle_buffer_clear(&pub_buffer);
  rsa_public_key_clear(&pub);
  rsa_private_key_clear(&priv);
  free (pub_name);
  
  return EXIT_SUCCESS;
}
