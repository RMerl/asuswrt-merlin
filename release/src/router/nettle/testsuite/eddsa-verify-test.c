/* eddsa-verify-test.c

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

#include "eddsa.h"

static void
test_eddsa (const struct ecc_curve *ecc,
	    const struct nettle_hash *H,
	    const uint8_t *pub,
	    const struct tstring *msg,
	    const uint8_t *signature)
{
  mp_limb_t *A = xalloc_limbs (ecc_size_a (ecc));
  mp_limb_t *scratch = xalloc_limbs (_eddsa_verify_itch (ecc));
  size_t nbytes = 1 + ecc->p.bit_size / 8;
  uint8_t *cmsg = xalloc (msg->length);
  uint8_t *csignature = xalloc (2*nbytes);
  void *ctx = xalloc (H->context_size);

  if (!_eddsa_decompress (ecc, A, pub, scratch))
    die ("Invalid eddsa public key.\n");

  memcpy (csignature, signature, 2*nbytes);
  if (!_eddsa_verify (ecc, H, pub, A, ctx,
		      msg->length, msg->data, csignature, scratch))
    {
      fprintf (stderr, "eddsa_verify failed with valid signature.\n");
    fail:
      fprintf (stderr, "bit_size = %u\npub = ", ecc->p.bit_size);
      print_hex (nbytes, pub);
      fprintf (stderr, "\nmsg = ");
      tstring_print_hex (msg);
      fprintf (stderr, "\nsign = ");
      print_hex (2*nbytes, csignature);
      fprintf (stderr, "\n");
      abort();
    }

  memcpy (csignature, signature, 2*nbytes);
  csignature[nbytes/3] ^= 0x40;
  if (_eddsa_verify (ecc, H, pub, A, ctx,
		     msg->length, msg->data, csignature, scratch))
    {
      fprintf (stderr,
	       "ecdsa_verify unexpectedly succeeded with invalid signature r.\n");
      goto fail;
    }

  memcpy (csignature, signature, 2*nbytes);
  csignature[5*nbytes/3] ^= 0x8;

  if (_eddsa_verify (ecc, H, pub, A, ctx,
		     msg->length, msg->data, csignature, scratch))
    {
      fprintf (stderr,
	       "ecdsa_verify unexpectedly succeeded with invalid signature s.\n");
      goto fail;
    }

  if (msg->length == 0)
    {
      if (_eddsa_verify  (ecc, H, pub, A, ctx,
			  3, "foo", signature, scratch))
	{
	  fprintf (stderr,
		   "ecdsa_verify unexpectedly succeeded with different message.\n");
	  goto fail;
	}
    }
  else
    {
      if (_eddsa_verify  (ecc, H, pub, A, ctx,
			  msg->length - 1, msg->data,
			  signature, scratch))
	{
	  fprintf (stderr,
		   "ecdsa_verify unexpectedly succeeded with truncated message.\n");
	  goto fail;
	}
      memcpy (cmsg, msg->data, msg->length);
      cmsg[2*msg->length / 3] ^= 0x20;
      if (_eddsa_verify  (ecc, H, pub, A, ctx,
			  msg->length, cmsg, signature, scratch))
	{
	  fprintf (stderr,
		   "ecdsa_verify unexpectedly succeeded with modified message.\n");
	  goto fail;
	}
    }
  free (A);
  free (scratch);
  free (cmsg);
  free (csignature);
  free (ctx);
}

void
test_main (void)
{
  test_eddsa (&_nettle_curve25519, &nettle_sha512,
	      H("d75a980182b10ab7 d54bfed3c964073a"
		"0ee172f3daa62325 af021a68f707511a"),
	      SHEX(""),
	      H("e5564300c360ac72 9086e2cc806e828a"
		"84877f1eb8e5d974 d873e06522490155"
		"5fb8821590a33bac c61e39701cf9b46b"
		"d25bf5f0595bbe24 655141438e7a100b"));
  test_eddsa (&_nettle_curve25519, &nettle_sha512,
	      H("3d4017c3e843895a 92b70aa74d1b7ebc"
		"9c982ccf2ec4968c c0cd55f12af4660c"),
	      SHEX("72"),
	      H("92a009a9f0d4cab8 720e820b5f642540"
		"a2b27b5416503f8f b3762223ebdb69da"
		"085ac1e43e15996e 458f3613d0f11d8c"
		"387b2eaeb4302aee b00d291612bb0c00"));
  test_eddsa (&_nettle_curve25519, &nettle_sha512,
	      H("1ed506485b09a645 0be7c9337d9fe87e"
		"f99c96f8bd11cd63 1ca160d0fd73067e"),
	      SHEX("fbed2a7df418ec0e 8036312ec239fcee"
		   "6ef97dc8c2df1f2e 14adee287808b788"
		   "a6072143b851d975 c8e8a0299df846b1"
		   "9113e38cee83da71 ea8e9bd6f57bdcd3"
		   "557523f4feb616ca a595aea01eb0b3d4"
		   "90b99b525ea4fbb9 258bc7fbb0deea8f"
		   "568cb2"),
	      H("cbef65b6f3fd5809 69fc3340cfae4f7c"
		"99df1340cce54626 183144ef46887163"
		"4b0a5c0033534108 e1c67c0dc99d3014"
		"f01084e98c95e101 4b309b1dbb2e6704"));
}
