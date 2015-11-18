/* lfib-stream.c

   Generates a pseudorandom stream, using the Knuth lfib
   (non-cryptographic) pseudorandom generator.

   Copyright (C) 2003 Niels Möller

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

#include "knuth-lfib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#define BUFSIZE 500

static void
usage(void)
{
  fprintf(stderr, "Usage: lfib-stream [SEED]\n");
}

int
main(int argc, char **argv)
{
  struct knuth_lfib_ctx ctx;
  uint32_t seed;

  if (argc == 1)
    seed = time(NULL);

  else if (argc == 2)
    {
      seed = atoi(argv[1]);
      if (!seed)
	{
	  usage();
	  return EXIT_FAILURE;
	}
    }
  else
    {
      usage();
      return EXIT_FAILURE;
    }

  knuth_lfib_init(&ctx, seed);

  for (;;)
    {
      char buffer[BUFSIZE];
      knuth_lfib_random(&ctx, BUFSIZE, buffer);

      if (fwrite(buffer, 1, BUFSIZE, stdout) < BUFSIZE
	  || fflush(stdout) < 0)
	return EXIT_FAILURE;
    }

  /* Not reached. This program is usually terminated by SIGPIPE */
}
