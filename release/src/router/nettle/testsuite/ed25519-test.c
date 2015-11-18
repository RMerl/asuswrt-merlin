/* ed25519-test.c

   Copyright (C) 2014 Niels Möller

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

#include "testutils.h"

#include <errno.h>

#include "eddsa.h"

#include "base16.h"

static void
decode_hex (size_t length, uint8_t *dst, const char *src)
{
  struct base16_decode_ctx ctx;
  size_t out_size;
  base16_decode_init (&ctx);
  ASSERT (base16_decode_update (&ctx, &out_size, dst, 2*length, src));
  ASSERT (out_size == length);
  ASSERT (base16_decode_final (&ctx));
}

/* Processes a single line in the format of
   http://ed25519.cr.yp.to/python/sign.input:

     sk pk : pk : m : s m :

   where sk (secret key) and pk (public key) are 32 bytes each, m is
   variable size, and s is 64 bytes. All values hex encoded.
*/
static void
test_one (const char *line)
{
  const char *p;
  const char *mp;
  uint8_t sk[ED25519_KEY_SIZE];
  uint8_t pk[ED25519_KEY_SIZE];
  uint8_t t[ED25519_KEY_SIZE];
  uint8_t s[ED25519_SIGNATURE_SIZE];
  uint8_t *msg;
  size_t msg_size;
  uint8_t s2[ED25519_SIGNATURE_SIZE];

  decode_hex (ED25519_KEY_SIZE, sk, line);

  p = strchr (line, ':');
  ASSERT (p == line + 128);
  p++;
  decode_hex (ED25519_KEY_SIZE, pk, p);
  p = strchr (p, ':');
  ASSERT (p == line + 193);
  mp = ++p;
  p = strchr (p, ':');
  ASSERT (p);
  ASSERT ((p - mp) % 2 == 0);
  msg_size = (p - mp) / 2;

  decode_hex (ED25519_SIGNATURE_SIZE, s, p+1);

  msg = xalloc (msg_size + 1);
  msg[msg_size] = 'x';

  decode_hex (msg_size, msg, mp);

  ed25519_sha512_public_key (t, sk);
  ASSERT (MEMEQ(ED25519_KEY_SIZE, t, pk));

  ed25519_sha512_sign (pk, sk, msg_size, msg, s2);
  ASSERT (MEMEQ (ED25519_SIGNATURE_SIZE, s, s2));

  ASSERT (ed25519_sha512_verify (pk, msg_size, msg, s));

  s2[ED25519_SIGNATURE_SIZE/3] ^= 0x40;
  ASSERT (!ed25519_sha512_verify (pk, msg_size, msg, s2));

  memcpy (s2, s, ED25519_SIGNATURE_SIZE);
  s2[2*ED25519_SIGNATURE_SIZE/3] ^= 0x40;
  ASSERT (!ed25519_sha512_verify (pk, msg_size, msg, s2));

  ASSERT (!ed25519_sha512_verify (pk, msg_size + 1, msg, s));

  if (msg_size > 0)
    {
      msg[msg_size-1] ^= 0x20;
      ASSERT (!ed25519_sha512_verify (pk, msg_size, msg, s));
    }
  free (msg);
}

#ifndef HAVE_GETLINE
static ssize_t
getline(char **lineptr, size_t *n, FILE *f)
{
  size_t i;
  int c;
  if (!*lineptr)
    {
      *n = 500;
      *lineptr = xalloc (*n);
    }

  i = 0;
  do
    {
      c = getc(f);
      if (c < 0)
	{
	  if (i > 0)
	    break;
	  return -1;
	}

      (*lineptr) [i++] = c;
      if (i == *n)
	{
	  *n *= 2;
	  *lineptr = realloc (*lineptr, *n);
	  if (!*lineptr)
	    die ("Virtual memory exhausted.\n");
	}
    } while (c != '\n');

  (*lineptr) [i] = 0;
  return i;
}
#endif

void
test_main(void)
{
  const char *input = getenv ("ED25519_SIGN_INPUT");
  if (input)
    {
      size_t buf_size;
      char *buf;
      FILE *f = fopen (input, "r");
      if (!f)
	die ("Opening input file '%s' failed: %s\n",
	     input, strerror (errno));

      for (buf = NULL; getline (&buf, &buf_size, f) >= 0; )
	test_one (buf);

      free (buf);
      fclose (f);
    }
  else
    {
      /* First few lines only */
      test_one ("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a:d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a::e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b:");
      test_one ("4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c:3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c:72:92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c0072:");
      test_one ("c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025:fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025:af82:6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40aaf82:");
      test_one ("0d4a05b07352a5436e180356da0ae6efa0345ff7fb1572575772e8005ed978e9e61a185bcef2613a6c7cb79763ce945d3b245d76114dd440bcf5f2dc1aa57057:e61a185bcef2613a6c7cb79763ce945d3b245d76114dd440bcf5f2dc1aa57057:cbc77b:d9868d52c2bebce5f3fa5a79891970f309cb6591e3e1702a70276fa97c24b3a8e58606c38c9758529da50ee31b8219cba45271c689afa60b0ea26c99db19b00ccbc77b:");
    }
}
