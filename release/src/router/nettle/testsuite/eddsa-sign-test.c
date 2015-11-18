/* eddsa-sign-test.c

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
test_eddsa_sign (const struct ecc_curve *ecc,
		 const struct nettle_hash *H,
		 const struct tstring *public,
		 const struct tstring *private,
		 const struct tstring *msg,
		 const struct tstring *ref)
{
  mp_limb_t *scratch = xalloc_limbs (_eddsa_sign_itch (ecc));
  size_t nbytes = 1 + ecc->p.bit_size / 8;
  uint8_t *signature = xalloc (2*nbytes);
  void *ctx = xalloc (H->context_size);
  uint8_t *public_out = xalloc (nbytes);
  uint8_t *digest = xalloc (2*nbytes);
  const uint8_t *k1 = digest + nbytes;
  mp_limb_t *k2 = xalloc_limbs (ecc->p.size);

  ASSERT (public->length == nbytes);
  ASSERT (private->length == nbytes);
  ASSERT (ref->length == 2*nbytes);

  _eddsa_expand_key (ecc, H, ctx, private->data,
		     digest, k2);
  _eddsa_public_key (ecc, k2, public_out, scratch);

  if (!MEMEQ (nbytes, public_out, public->data))
    {
      fprintf (stderr, "Bad public key from _eddsa_expand_key + _eddsa_public_key.\n");
      fprintf (stderr, "got:");
      print_hex (nbytes, public_out);
      fprintf (stderr, "\nref:");
      tstring_print_hex (public);
      fprintf (stderr, "\n");
      abort ();
    }
  H->update (ctx, nbytes, k1);
  
  _eddsa_sign (ecc, H, public->data, ctx, k2,
	       msg->length, msg->data, signature, scratch);

  if (!MEMEQ (2*nbytes, signature, ref->data))
    {
      fprintf (stderr, "Bad _eddsa_sign output.\n");
      fprintf (stderr, "Public key:");
      tstring_print_hex (public);
      fprintf (stderr, "\nPrivate key:");
      tstring_print_hex (private);
      fprintf (stderr, "\nk2:");
      mpn_out_str (stderr, 16, k2, ecc->p.size);
      fprintf (stderr, "\nMessage (length %u):", (unsigned) msg->length);
      tstring_print_hex (msg);      
      fprintf (stderr, "\ngot:");
      print_hex (2*nbytes, signature);
      fprintf (stderr, "\nref:");
      tstring_print_hex (ref);
      fprintf (stderr, "\n");
      abort ();
    }
  
  free (scratch);
  free (signature);
  free (ctx);
  free (digest);
  free (k2);
  free (public_out);
}

void test_main (void)
{
  /* Based on a few of the test vectors at
     http://ed25519.cr.yp.to/python/sign.input */
  test_eddsa_sign (&_nettle_curve25519, &nettle_sha512,
		   SHEX("d75a980182b10ab7 d54bfed3c964073a"
			"0ee172f3daa62325 af021a68f707511a"),
		   SHEX("9d61b19deffd5a60 ba844af492ec2cc4"
			"4449c5697b326919 703bac031cae7f60"),
		   SHEX(""),
		   SHEX("e5564300c360ac72 9086e2cc806e828a"
			"84877f1eb8e5d974 d873e06522490155"
			"5fb8821590a33bac c61e39701cf9b46b"
			"d25bf5f0595bbe24 655141438e7a100b"));
  test_eddsa_sign (&_nettle_curve25519, &nettle_sha512,
		   SHEX("3d4017c3e843895a 92b70aa74d1b7ebc"
			"9c982ccf2ec4968c c0cd55f12af4660c"),
		   SHEX("4ccd089b28ff96da 9db6c346ec114e0f"
			"5b8a319f35aba624 da8cf6ed4fb8a6fb"),
		   SHEX("72"),
		   SHEX("92a009a9f0d4cab8 720e820b5f642540"
			"a2b27b5416503f8f b3762223ebdb69da"
			"085ac1e43e15996e 458f3613d0f11d8c"
			"387b2eaeb4302aee b00d291612bb0c00"));
  test_eddsa_sign (&_nettle_curve25519, &nettle_sha512,
		   SHEX("1ed506485b09a645 0be7c9337d9fe87e"
			"f99c96f8bd11cd63 1ca160d0fd73067e"),
		   SHEX("f215d34fe2d757cf f9cf5c05430994de"
			"587987ce45cb0459 f61ec6c825c62259"),
		   SHEX("fbed2a7df418ec0e 8036312ec239fcee"
			"6ef97dc8c2df1f2e 14adee287808b788"
			"a6072143b851d975 c8e8a0299df846b1"
			"9113e38cee83da71 ea8e9bd6f57bdcd3"
			"557523f4feb616ca a595aea01eb0b3d4"
			"90b99b525ea4fbb9 258bc7fbb0deea8f"
			"568cb2"),
		   SHEX("cbef65b6f3fd5809 69fc3340cfae4f7c"
			"99df1340cce54626 183144ef46887163"
			"4b0a5c0033534108 e1c67c0dc99d3014"
			"f01084e98c95e101 4b309b1dbb2e6704"));
}
