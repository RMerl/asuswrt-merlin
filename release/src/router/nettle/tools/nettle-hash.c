/* nettle-hash.c

   General hashing tool.

   Copyright (C) 2011, 2013 Niels Möller

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

#include "nettle-meta.h"
#include "base16.h"

#include "getopt.h"
#include "misc.h"

#define BUFSIZE 16384

static void
list_algorithms (void)
{
  unsigned i;
  const struct nettle_hash *alg;
  printf ("%10s digestsize (internal block size), in units of octets\n", "name");

  for (i = 0; (alg = nettle_hashes[i]); i++)
    printf ("%10s %d (%d)\n",
	    alg->name, alg->digest_size, alg->block_size);
};

static const struct nettle_hash *
find_algorithm (const char *name)
{
  const struct nettle_hash *alg;
  unsigned i;
  
  for (i = 0; (alg = nettle_hashes[i]); i++)
    if (!strcmp(name, alg->name))
      return alg;

  return NULL;
}

/* Also in examples/io.c */
static int
hash_file(const struct nettle_hash *hash, void *ctx, FILE *f)
{
  for (;;)
    {
      char buffer[BUFSIZE];
      size_t res = fread(buffer, 1, sizeof(buffer), f);
      if (ferror(f))
	return 0;
      
      hash->update(ctx, res, buffer);
      if (feof(f))
	return 1;
    }
}

static int
digest_file(const struct nettle_hash *alg,
	    unsigned digest_length, int raw,
	    FILE *f)
{
  void *ctx;
  uint8_t *digest;
  ctx = xalloc(alg->context_size);

  alg->init(ctx);

  if (!hash_file (alg, ctx, f))
    {
      free (ctx);
      return 0;
    }

  digest = xalloc(digest_length);
  alg->digest(ctx, digest_length, digest);
  free(ctx);

  if (raw)
    fwrite (digest, digest_length, 1, stdout);

  else
    {
      unsigned i;
      char hex[BASE16_ENCODE_LENGTH(8) + 1];
      for (i = 0; i + 8 < digest_length; i += 8)
	{
	  base16_encode_update(hex, 8, digest + i);
	  hex[BASE16_ENCODE_LENGTH(8)] = 0;
	  printf("%s ", hex);
	}
      base16_encode_update(hex, digest_length - i, digest + i);
      hex[BASE16_ENCODE_LENGTH(digest_length - i)] = 0;
      printf("%s %s\n", hex, alg->name);
    }
  
  free(digest);

  return 1;
}

/* FIXME: Be more compatible with md5sum and sha1sum. Options -c
   (check), -b (binary), -t (text), and output format with hex hash
   sum, optional star (meaning binary mode), and file name. */
int
main (int argc, char **argv)
{
  const char *alg_name = NULL;
  const struct nettle_hash *alg;
  unsigned length = 0;
  int raw = 0;
  int c;

  enum { OPT_HELP = 0x300, OPT_RAW, OPT_LIST };
  static const struct option options[] =
    {
      /* Name, args, flag, val */
      { "help", no_argument, NULL, OPT_HELP },
      { "version", no_argument, NULL, 'V' },
      { "algorithm", required_argument, NULL, 'a' },
      { "length", required_argument, NULL, 'l' },
      { "list", no_argument, NULL, OPT_LIST },
      { "raw", no_argument, NULL, OPT_RAW },

      { NULL, 0, NULL, 0 }
    };

  while ( (c = getopt_long(argc, argv, "Va:l:", options, NULL)) != -1)
    switch (c)
      {
      default:
	abort();
      case OPT_HELP:
	printf("nettle-hash -a ALGORITHM [OPTIONS] [FILE ...]\n"
	       "Options:\n"
	       "  --help              Show this help.\n"
	       "  -V, --version       Show version information.\n"
	       "  --list              List supported hash algorithms.\n"
	       "  -a, --algorithm=ALG Hash algorithm to use.\n"
	       "  -l, --length=LENGTH Desired digest length (octets)\n"
	       "  --raw               Raw binary output.\n");
	return EXIT_SUCCESS;
      case 'V':
	printf("nettle-hash (" PACKAGE_STRING ")\n");
	return EXIT_SUCCESS;
      case 'a':
	alg_name = optarg;
	break;
      case 'l':
	{
	  int arg;
	  arg = atoi (optarg);
	  if (arg <= 0)
	    die ("Invalid length argument: `%s'\n", optarg);
	  length = arg;
	}
	break;
      case OPT_RAW:
	raw = 1;
	break;
      case OPT_LIST:
	list_algorithms();
	return EXIT_SUCCESS;
      }

  if (!alg_name)
    die("Algorithm argument (-a option) is mandatory.\n"
	"See nettle-hash --help for further information.\n");
      
  alg = find_algorithm (alg_name);
  if (!alg)
    die("Hash algorithm `%s' not supported or .\n"
	"Use nettle-hash --list to list available algorithms.\n",
	alg_name);

  if (length == 0)
    length = alg->digest_size;
  else if (length > alg->digest_size)
    die ("Length argument %d too large for selected algorithm.\n",
	 length);
    
  argv += optind;
  argc -= optind;

  if (argc == 0)
    digest_file (alg, length, raw, stdin);
  else
    {
      int i;
      for (i = 0; i < argc; i++)
	{
	  FILE *f = fopen (argv[i], "rb");
	  if (!f)
	    die ("Cannot open `%s': %s\n", argv[i], STRERROR(errno));
	  printf("%s: ", argv[i]);
	  if (!digest_file (alg, length, raw, f))
	    die("Reading `%s' failed: %s\n", argv[i], STRERROR(errno));
	  fclose(f);
	}
    }
  if (fflush(stdout) != 0 )
    die("Write failed: %s\n", STRERROR(errno));

  return EXIT_SUCCESS;
}
