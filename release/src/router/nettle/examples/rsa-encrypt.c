/* rsa-encrypt.c

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

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef WIN32
#include <fcntl.h>
#endif

/* string.h must be included before gmp.h */
#include "bignum.h"
#include "buffer.h"
#include "macros.h"
#include "rsa.h"
#include "yarrow.h"

#include "io.h"
#include "rsa-session.h"

#include "getopt.h"

void
rsa_session_set_encrypt_key(struct rsa_session *ctx,
			    const struct rsa_session_info *key)
{
  const uint8_t *aes_key = SESSION_AES_KEY(key);
  const uint8_t *iv = SESSION_IV(key);
  const uint8_t *hmac_key = SESSION_HMAC_KEY(key);
  
  aes_set_encrypt_key(&ctx->aes.ctx, AES_KEY_SIZE, aes_key);
  CBC_SET_IV(&ctx->aes, iv);
  hmac_sha1_set_key(&ctx->hmac, SHA1_DIGEST_SIZE, hmac_key);
}

static int
write_uint32(FILE *f, uint32_t n)
{
  uint8_t buffer[4];
  WRITE_UINT32(buffer, n);

  return write_string(f, sizeof(buffer), buffer);
}

static int
write_version(FILE *f)
{
  return write_uint32(f, 1);
}

static int
write_bignum(FILE *f, mpz_t x)
{
  unsigned size = nettle_mpz_sizeinbase_256_u(x);
  uint8_t *p;
  int res;
  
  if (!write_uint32(f, size))
    return 0;
  
  p = xalloc(size);
  nettle_mpz_get_str_256(size, p, x);

  res = write_string(f, size, p);
  free(p);
  return res;
}

#define BLOCK_SIZE (AES_BLOCK_SIZE * 100)

static int
process_file(struct rsa_session *ctx,
	     FILE *in, FILE *out)
{
  uint8_t buffer[BLOCK_SIZE + SHA1_DIGEST_SIZE];

  for (;;)
    {
      size_t size = fread(buffer, 1, BLOCK_SIZE, in);
      hmac_sha1_update(&ctx->hmac, size, buffer);

      if (size < BLOCK_SIZE)
	{
	  unsigned leftover;
	  unsigned padding;

	  if (ferror(in))
	    {
	      werror("Reading input failed: %s\n", strerror(errno));
	      return 0;
	    }
	  
	  leftover = size % AES_BLOCK_SIZE;
	  padding = AES_BLOCK_SIZE - leftover;

	  assert (size + padding <= BLOCK_SIZE);
	  
	  if (padding > 1)
	    yarrow256_random(&ctx->yarrow, padding - 1, buffer + size);

	  size += padding;

	  buffer[size - 1] = padding;
	  CBC_ENCRYPT(&ctx->aes, aes_encrypt, size, buffer, buffer);

	  assert (size + SHA1_DIGEST_SIZE <= sizeof(buffer));

	  hmac_sha1_digest(&ctx->hmac, SHA1_DIGEST_SIZE, buffer + size);
	  size += SHA1_DIGEST_SIZE;

	  if (!write_string(out, size, buffer))
	    {
	      werror("Writing output failed: %s\n", strerror(errno));
	      return 0;
	    }
	  return 1;
	}

      CBC_ENCRYPT(&ctx->aes, aes_encrypt, size, buffer, buffer);
      if (!write_string(out, size, buffer))
	{
	  werror("Writing output failed: %s\n", strerror(errno));
	  return 0;
	}
    }
}

static void
usage (FILE *out)
{
  fprintf (out, "Usage: rsa-encrypt [OPTIONS] PUBLIC-KEY < cleartext\n"
	   "Options:\n"
	   "   -r, --random=FILE   seed file for randomness generator\n"
	   "       --help          display this help\n");  
}

int
main(int argc, char **argv)
{
  struct rsa_session ctx;
  struct rsa_session_info info;
  
  struct rsa_public_key key;
  mpz_t x;
  
  int c;
  const char *random_name = NULL;

  enum { OPT_HELP = 300 };
  
  static const struct option options[] =
    {
      /* Name, args, flag, val */
      { "help", no_argument, NULL, OPT_HELP },
      { "random", required_argument, NULL, 'r' },
      { NULL, 0, NULL, 0}
    };
  
  while ( (c = getopt_long(argc, argv, "o:r:", options, NULL)) != -1)
    switch (c)
      {
      case 'r':
	random_name = optarg;
	break;
	
      case '?':
	return EXIT_FAILURE;

      case OPT_HELP:
	usage(stdout);
	return EXIT_SUCCESS;
      default:
	abort();
      }

  argv += optind;
  argc -= optind;

  if (argc != 1)
    {
      usage (stderr);
      return EXIT_FAILURE;
    }

  rsa_public_key_init(&key);
  
  if (!read_rsa_key(argv[0], &key, NULL))
    {
      werror("Invalid key\n");
      return EXIT_FAILURE;
    }

  /* NOTE: No sources */
  yarrow256_init(&ctx.yarrow, 0, NULL);
  
  /* Read some data to seed the generator */
  if (!simple_random(&ctx.yarrow, random_name))
    {
      werror("Initialization of randomness generator failed.\n");
      return EXIT_FAILURE;
    }

  WRITE_UINT32(SESSION_VERSION(&info), RSA_VERSION);
  
  yarrow256_random(&ctx.yarrow, sizeof(info.key) - 4, info.key + 4);

  rsa_session_set_encrypt_key(&ctx, &info);
  
#ifdef WIN32
  _setmode(0, O_BINARY);
  _setmode(1, O_BINARY);
#endif

  write_version(stdout);
  
  mpz_init(x);

  if (!rsa_encrypt(&key,
		   &ctx.yarrow, (nettle_random_func *) yarrow256_random,
		   sizeof(info.key), info.key, 
		   x))
    {
      werror("RSA encryption failed.\n");
      return EXIT_FAILURE;
    }

  write_bignum(stdout, x);

  mpz_clear (x);

  if (!process_file(&ctx,
		    stdin, stdout))
    return EXIT_FAILURE;

  rsa_public_key_clear(&key);

  return EXIT_SUCCESS;
}
