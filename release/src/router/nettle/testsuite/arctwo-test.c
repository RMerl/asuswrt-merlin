/* arctwo-test.c

   Copyright (C) 2004 Simon Josefsson
   Copyright (C) 2004 Niels Möller

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
#include "arctwo.h"

/* For tests with obscure values of ebk. */
static void
test_arctwo(unsigned ekb,
	    const struct tstring *key,
	    const struct tstring *cleartext,
	    const struct tstring *ciphertext)
{
  struct arctwo_ctx ctx;
  uint8_t *data;
  size_t length;

  ASSERT (cleartext->length == ciphertext->length);
  length = cleartext->length;
  
  data = xalloc(length);

  arctwo_set_key_ekb(&ctx, key->length, key->data, ekb);
  arctwo_encrypt(&ctx, length, data, cleartext->data);

  ASSERT(MEMEQ(length, data, ciphertext->data));

  arctwo_decrypt(&ctx, length, data, data);

  ASSERT(MEMEQ(length, data, cleartext->data));

  free(data);
}

void
test_main(void)
{
  /* Test vectors from Peter Gutmann's paper. */
  test_cipher(&nettle_arctwo_gutmann128,
	      SHEX("00000000 00000000 00000000 00000000"),
	      SHEX("00000000 00000000"),
	      SHEX("1c198a83 8df028b7"));

  test_cipher(&nettle_arctwo_gutmann128,
	      SHEX("00010203 04050607 08090a0b 0c0d0e0f"),
	      SHEX("00000000 00000000"),
	      SHEX("50dc0162 bd757f31"));

  /* This one was checked against libmcrypt's RFC2268. */
  test_cipher(&nettle_arctwo_gutmann128,
	      SHEX("30000000 00000000 00000000 00000000"),
	      SHEX("10000000 00000000"),
	      SHEX("8fd10389 336bf95e"));

  /* Test vectors from RFC 2268. */
  test_cipher(&nettle_arctwo64,
	      SHEX("ffffffff ffffffff"),
	      SHEX("ffffffff ffffffff"),
	      SHEX("278b27e4 2e2f0d49"));

  test_cipher(&nettle_arctwo64,
	      SHEX("30000000 00000000"),
	      SHEX("10000000 00000001"),
	      SHEX("30649edf 9be7d2c2"));

  test_cipher(&nettle_arctwo128,
	      SHEX("88bca90e 90875a7f 0f79c384 627bafb2"),
	      SHEX("00000000 00000000"),
	      SHEX("2269552a b0f85ca6"));

  /* More obscure tests from RFC 2286 */
  test_arctwo(63,
	      SHEX("00000000 00000000"),
	      SHEX("00000000 00000000"),
	      SHEX("ebb773f9 93278eff"));

  test_arctwo(64,
	      SHEX("88"),
	      SHEX("00000000 00000000"),
	      SHEX("61a8a244 adacccf0"));

  test_arctwo(64,
	      SHEX("88bca90e 90875a"),
	      SHEX("00000000 00000000"),
	      SHEX("6ccf4308 974c267f"));

  test_arctwo(64,
	      SHEX("88bca90e 90875a7f 0f79c384 627bafb2"),
	      SHEX("00000000 00000000"),
	      SHEX("1a807d27 2bbe5db1"));

  test_arctwo(129,
	      SHEX("88bca90e 90875a7f 0f79c384 627bafb2"
		   "16f80a6f 85920584 c42fceb0 be255daf 1e"),
	      SHEX("00000000 00000000"),
	      SHEX("5b78d3a4 3dfff1f1"));
}
