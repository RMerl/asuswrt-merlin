/* rsa-decrypt.c

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
#ifdef WIN32
#include <fcntl.h>
#endif

/* string.h must be included before gmp.h */
#include "aes.h"
#include "bignum.h"
#include "buffer.h"
#include "cbc.h"
#include "hmac.h"
#include "macros.h"
#include "rsa.h"
#include "yarrow.h"

#include "io.h"
#include "rsa-session.h"

void
rsa_session_set_decrypt_key(struct rsa_session *ctx,
			    const struct rsa_session_info *key)
{
  const uint8_t *aes_key = SESSION_AES_KEY(key);
  const uint8_t *iv = SESSION_IV(key);
  const uint8_t *hmac_key = SESSION_HMAC_KEY(key);
  
  aes_set_decrypt_key(&ctx->aes.ctx, AES_KEY_SIZE, aes_key);
  CBC_SET_IV(&ctx->aes, iv);
  hmac_sha1_set_key(&ctx->hmac, SHA1_DIGEST_SIZE, hmac_key);
}

static int
read_uint32(FILE *f, uint32_t *n)
{
  uint8_t buf[4];
  if (fread(buf, 1, sizeof(buf), f) != sizeof(buf))
    return 0;

  *n = READ_UINT32(buf);
  return 1;
}

static int
read_version(FILE *f)
{
  uint32_t version;
  return read_uint32(f, &version) && version == RSA_VERSION;
}

static int
read_bignum(FILE *f, mpz_t x)
{
  uint32_t size;
  if (read_uint32(f, &size)
      && size < 1000)
    {
      uint8_t *p = xalloc(size);
      if (fread(p, 1, size, f) != size)
	{
	  free(p);
	  return 0;
	}

      nettle_mpz_set_str_256_u(x, size, p);
      free(p);

      return 1;
    }
  return 0;
}

struct process_ctx
{
  struct CBC_CTX(struct aes_ctx, AES_BLOCK_SIZE) aes;
  struct hmac_sha1_ctx hmac;
  struct yarrow256_ctx yarrow;
};

#define BUF_SIZE (100 * AES_BLOCK_SIZE)

/* Trailing data that needs special processing */
#define BUF_FINAL (AES_BLOCK_SIZE + SHA1_DIGEST_SIZE)

static int
process_file(struct rsa_session *ctx,
	     FILE *in, FILE *out)
{
  uint8_t buffer[BUF_SIZE + BUF_FINAL];
  uint8_t digest[SHA1_DIGEST_SIZE];
  size_t size;
  unsigned padding;

  size = fread(buffer, 1, BUF_FINAL, in);
  if (size < BUF_FINAL)
    {
      if (ferror(in))
	werror("Reading input failed: %s\n", strerror(errno));
      else
	werror("Unexpected EOF on input.\n");
      return 0;
    }

  do
    {
      size = fread(buffer + BUF_FINAL, 1, BUF_SIZE, in);

      if (size < BUF_SIZE && ferror(in))
	{
	  werror("Reading input failed: %s\n", strerror(errno));
	  return 0;
	}

      if (size % AES_BLOCK_SIZE != 0)
	{
	  werror("Unexpected EOF on input.\n");
	  return 0;
	}

      if (size)
	{
	  CBC_DECRYPT(&ctx->aes, aes_decrypt, size, buffer, buffer);
	  hmac_sha1_update(&ctx->hmac, size, buffer);
	  if (!write_string(out, size, buffer))
	    {
	      werror("Writing output failed: %s\n", strerror(errno));
	      return 0;
	    }
	  memmove(buffer, buffer + size, BUF_FINAL);
	}
    }
  while (size == BUF_SIZE);

  /* Decrypt final block */
  CBC_DECRYPT(&ctx->aes, aes_decrypt, AES_BLOCK_SIZE, buffer, buffer);
  padding = buffer[AES_BLOCK_SIZE - 1];
  if (padding > AES_BLOCK_SIZE)
    {
      werror("Decryption failed: Invalid padding.\n");
      return 0;
    }

  if (padding < AES_BLOCK_SIZE)
    {
      unsigned leftover = AES_BLOCK_SIZE - padding;
      hmac_sha1_update(&ctx->hmac, leftover, buffer);
      if (!write_string(out, leftover, buffer))
	{
	  werror("Writing output failed: %s\n", strerror(errno));
	  return 0;
	}
    }
  hmac_sha1_digest(&ctx->hmac, SHA1_DIGEST_SIZE, digest);
  if (memcmp(digest, buffer + AES_BLOCK_SIZE, SHA1_DIGEST_SIZE) != 0)
    {
      werror("Decryption failed: Invalid mac.\n");
      return 0;
    }
  
  return 1;
}

int
main(int argc, char **argv)
{
  struct rsa_private_key key;
  struct rsa_session ctx;
  struct rsa_session_info session;

  size_t length;
  mpz_t x;

  mpz_init(x);
  
  if (argc != 2)
    {
      werror("Usage: rsa-decrypt PRIVATE-KEY < ciphertext\n");
      return EXIT_FAILURE;
    }

  rsa_private_key_init(&key);
  
  if (!read_rsa_key(argv[1], NULL, &key))
    {
      werror("Invalid key\n");
      return EXIT_FAILURE;
    }

#ifdef WIN32
  _setmode(0, O_BINARY);
  _setmode(1, O_BINARY);
#endif

  if (!read_version(stdin))
    {
      werror("Bad version number in input file.\n");
      return EXIT_FAILURE;
    }

  if (!read_bignum(stdin, x))
    {
      werror("Bad rsa header in input file.\n");
      return EXIT_FAILURE;
    }

  length = sizeof(session.key);
  if (!rsa_decrypt(&key, &length, session.key, x) || length != sizeof(session.key))
    {
      werror("Failed to decrypt rsa header in input file.\n");
      return EXIT_FAILURE;      
    }
  mpz_clear(x);
  
  rsa_session_set_decrypt_key(&ctx, &session);

  if (!process_file(&ctx,
		    stdin, stdout))
    return EXIT_FAILURE;
  
  rsa_private_key_clear(&key);

  return EXIT_SUCCESS;
}
