/* nettle-pbkdf2.c

   Command-line tool for pbkdf2 hashing.

   Copyright (C) 2013 Niels Möller

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pbkdf2.h"
#include "base16.h"

#include "getopt.h"
#include "misc.h"

#define DEFAULT_ITERATIONS 10000
#define DEFAULT_LENGTH 16
static void
usage (FILE *f)
{
  fprintf(f, "Usage: nettle-pbkdf2 [OPTIONS] SALT\n"
	  "Options:\n"
	  "  --help                 Show this help.\n"
	  "  -V, --version          Show version information.\n"
	  "  -i, --iterations=COUNT Desired iteration count (default %d).\n"
	  "  -l, --length=LENGTH    Desired output length (octets, default %d)\n"
	  "  --raw                  Raw binary output.\n"
	  "  --hex-salt             Use hex encoding for the salt.\n",
	  DEFAULT_ITERATIONS, DEFAULT_LENGTH);
}

#define MAX_PASSWORD 1024

int
main (int argc, char **argv)
{
  unsigned iterations = DEFAULT_ITERATIONS;
  unsigned output_length = DEFAULT_LENGTH;
  char password[MAX_PASSWORD];
  size_t password_length;
  char *output;
  size_t salt_length;
  char *salt;
  int raw = 0;
  int hex_salt = 0;
  int c;

  enum { OPT_HELP = 0x300, OPT_RAW, OPT_HEX_SALT };
  static const struct option options[] =
    {
      /* Name, args, flag, val */
      { "help", no_argument, NULL, OPT_HELP },
      { "version", no_argument, NULL, 'V' },
      { "length", required_argument, NULL, 'l' },
      { "iterations", required_argument, NULL, 'i' },
      { "raw", no_argument, NULL, OPT_RAW },
      { "hex-salt", no_argument, NULL, OPT_HEX_SALT },

      { NULL, 0, NULL, 0 }
    };

  while ( (c = getopt_long(argc, argv, "Vl:i:", options, NULL)) != -1)
    switch (c)
      {
      default:
	abort();
      case '?':
	usage (stderr);
	return EXIT_FAILURE;
      case OPT_HELP:
	usage (stdout);
	return EXIT_SUCCESS;
      case 'V':
	printf("nettle-pbkdf2 (" PACKAGE_STRING ")\n");
	return EXIT_SUCCESS;
      case 'l':
	{
	  int arg;
	  arg = atoi (optarg);
	  if (arg <= 0)
	    die ("Invalid length argument: `%s'\n", optarg);

	  output_length = arg;
	}
	break;
      case 'i':
	{
	  int arg;
	  arg = atoi (optarg);
	  if (arg <= 0)
	    die ("Invalid iteration count: `%s'\n", optarg);
	  iterations = arg;
	}
	break;
      case OPT_RAW:
	raw = 1;
	break;
      case OPT_HEX_SALT:
	hex_salt = 1;
	break;
      }
  argv += optind;
  argc -= optind;

  if (argc != 1)
    {
      usage (stderr);
      return EXIT_FAILURE;
    }

  salt = strdup (argv[0]);
  salt_length = strlen(salt);
  
  if (hex_salt)
    {
      struct base16_decode_ctx base16;

      base16_decode_init (&base16);
      if (!base16_decode_update (&base16,
				 &salt_length,
				 salt, salt_length, salt)
	  || !base16_decode_final (&base16))
	die ("Invalid salt (expecting hex encoding).\n");
    }
  
  password_length = fread (password, 1, sizeof(password), stdin);
  if (password_length == sizeof(password))
    die ("Password input too long. Current limit is %d characters.\n",
	 (int) sizeof(password) - 1);
  if (ferror (stdin))
    die ("Reading password input failed: %s.\n", strerror (errno));

  output = xalloc (output_length);
  pbkdf2_hmac_sha256 (password_length, password, iterations, salt_length, salt,
		      output_length, output);

  free (salt);

  if (raw)
    fwrite (output, output_length, 1, stdout);
  else
    {
      unsigned i;
      char hex[BASE16_ENCODE_LENGTH(8) + 1];
      for (i = 0; i + 8 < output_length; i += 8)
	{
	  base16_encode_update(hex, 8, output + i);
	  hex[BASE16_ENCODE_LENGTH(8)] = 0;
	  printf("%s%c", hex, i % 64 == 56 ? '\n' : ' ');
	}
      base16_encode_update(hex, output_length - i, output + i);
      hex[BASE16_ENCODE_LENGTH(output_length - i)] = 0;
      printf("%s\n", hex);
    }
  free (output);

  if (fflush(stdout) != 0 )
    die("Write failed: %s\n", STRERROR(errno));

  return EXIT_SUCCESS;
}
