/* random-prime.c

   Command line tool for prime generation.

   Copyright (C) 2010 Niels Möller

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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "bignum.h"
#include "yarrow.h"

#include "io.h"

#include "getopt.h"

static void
usage(void)
{
  fprintf(stderr, "Usage: random-prime [OPTIONS] bits\n\n"
	  "Options:\n"
	  "      --help         Display this message.\n"
	  "  -v, --verbose      Display timing information.\n"
	  "  -r, --random FILE  Random data to use for seeding.\n");
}

int
main(int argc, char **argv)
{
  long bits;
  mpz_t p;
  struct yarrow256_ctx yarrow;

  int verbose = 0;  
  const char *random_file = NULL;

  int c;
  char *arg_end;

  clock_t start;
  clock_t end;
  
  enum { OPT_HELP = 300 };
  static const struct option options[] =
    {
      /* Name, args, flag, val */
      { "help", no_argument, NULL, OPT_HELP },
      { "verbose", no_argument, NULL, 'v' },
      { "random", required_argument, NULL, 'r' },
      { NULL, 0, NULL, 0}
    };

  while ( (c = getopt_long(argc, argv, "vr:", options, NULL)) != -1)
    switch (c)
      {
      case 'v':
	verbose = 1;
	break;
      case 'r':
	random_file = optarg;
	break;
      case OPT_HELP:
	usage();
	return EXIT_SUCCESS;
      case '?':
	return EXIT_FAILURE;
      default:
	abort();
      }

  argc -= optind;
  argv += optind;

  if (argc != 1)
    {
      usage();
      return EXIT_FAILURE;
    }

  bits = strtol(argv[0], &arg_end, 0);
  if (*arg_end || bits < 0)
    {
      fprintf(stderr, "Invalid number.\n");
      return EXIT_FAILURE;
    }

  if (bits < 3)
    {
      fprintf(stderr, "Bitsize must be at least 3.\n");
      return EXIT_FAILURE;
    }

  /* NOTE: No sources */
  yarrow256_init(&yarrow, 0, NULL);

  /* Read some data to seed the generator */
  if (!simple_random(&yarrow, random_file))
    {
      werror("Initialization of randomness generator failed.\n");
      return EXIT_FAILURE;
    }
  
  mpz_init(p);

  start = clock();

  nettle_random_prime(p, bits, 0,
		      &yarrow, (nettle_random_func *) yarrow256_random,
		      NULL, NULL);

  end = clock();

  mpz_out_str(stdout, 10, p);
  printf("\n");

  if (verbose)
    fprintf(stderr, "time: %.3g s\n",
	    (double)(end - start) / CLOCKS_PER_SEC);

  return EXIT_SUCCESS;
}
