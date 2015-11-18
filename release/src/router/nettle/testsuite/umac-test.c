#include "testutils.h"
#include "umac.h"

/* FIXME: Missing tests:

   Getting to unlikely cases in the poly64 and poly128 operations.
*/

static void
update (void *ctx, nettle_hash_update_func *f,
	const struct tstring *msg,
	size_t length)
{
  for (; length > msg->length; length -= msg->length)
    f(ctx, msg->length, msg->data);
  f(ctx, length, msg->data);
}

static void
check_digest (const char *name, void *ctx, nettle_hash_digest_func *f,
	      const struct tstring *msg, size_t length,
	      size_t tag_length, const uint8_t *ref)
{
  uint8_t tag[16];
  f(ctx, tag_length, tag);
  if (memcmp (tag, ref, tag_length) != 0)
    {
      printf ("%s failed\n", name);
      printf ("msg: "); print_hex (msg->length, msg->data);
      printf ("length: %lu\n", (unsigned long) length);
      printf ("tag: "); print_hex (tag_length, tag);
      printf ("ref: "); print_hex (tag_length, ref);
      abort ();
    }

}

static void
test_umac (const struct tstring *key,
	   const struct tstring *nonce,
	   const struct tstring *msg,
	   size_t length,
	   const struct tstring *ref32,
	   const struct tstring *ref64,
	   const struct tstring *ref128)
{
  struct umac32_ctx ctx32;
  struct umac64_ctx ctx64;
  struct umac96_ctx ctx96;
  struct umac128_ctx ctx128;

  ASSERT (key->length == UMAC_KEY_SIZE);
  ASSERT (ref32->length == 4);
  ASSERT (ref64->length == 8);
  ASSERT (ref128->length == 16);

  umac32_set_key (&ctx32, key->data);
  umac32_set_nonce (&ctx32, nonce->length, nonce->data);

  update(&ctx32, (nettle_hash_update_func *) umac32_update, msg, length);

  check_digest ("umac32", &ctx32, (nettle_hash_digest_func *) umac32_digest,
		msg, length, 4, ref32->data);

  umac64_set_key (&ctx64, key->data);
  umac64_set_nonce (&ctx64, nonce->length, nonce->data);

  update(&ctx64, (nettle_hash_update_func *) umac64_update, msg, length);

  check_digest ("umac64", &ctx64, (nettle_hash_digest_func *) umac64_digest,
		msg, length, 8, ref64->data);

  umac96_set_key (&ctx96, key->data);
  umac96_set_nonce (&ctx96, nonce->length, nonce->data);

  update(&ctx96, (nettle_hash_update_func *) umac96_update, msg, length);

  check_digest ("umac96", &ctx96, (nettle_hash_digest_func *) umac96_digest,
		msg, length, 12, ref128->data);

  umac128_set_key (&ctx128, key->data);
  umac128_set_nonce (&ctx128, nonce->length, nonce->data);

  update(&ctx128, (nettle_hash_update_func *) umac128_update, msg, length);

  check_digest ("umac128", &ctx128, (nettle_hash_digest_func *) umac128_digest,
		msg, length, 16, ref128->data);
}

static void
test_align(const struct tstring *key,
	   const struct tstring *nonce,
	   const struct tstring *msg,
	   size_t length,
	   const struct tstring *ref32,
	   const struct tstring *ref64,
	   const struct tstring *ref128)
{
  uint8_t *buffer = xalloc(length + 16);
  unsigned offset;
  for (offset = 0; offset < 16; offset++)
    {
      struct umac32_ctx ctx32;
      struct umac64_ctx ctx64;
      struct umac96_ctx ctx96;
      struct umac128_ctx ctx128;

      uint8_t *input;
      size_t i;

      memset(buffer, 17, length + 16);
      input = buffer + offset;

      for (i = 0; i + msg->length < length; i += msg->length)
	memcpy (input + i, msg->data, msg->length);
      memcpy (input + i, msg->data, length - i);

      umac32_set_key (&ctx32, key->data);
      umac32_set_nonce (&ctx32, nonce->length, nonce->data);

      umac32_update(&ctx32, length, input);

      check_digest ("umac32 (alignment)",
		    &ctx32, (nettle_hash_digest_func *) umac32_digest,
		    msg, length, 4, ref32->data);

      umac64_set_key (&ctx64, key->data);
      umac64_set_nonce (&ctx64, nonce->length, nonce->data);

      umac64_update(&ctx64, length, input);

      check_digest ("umac64 (alignment)",
		    &ctx64, (nettle_hash_digest_func *) umac64_digest,
		    msg, length, 8, ref64->data);

      umac96_set_key (&ctx96, key->data);
      umac96_set_nonce (&ctx96, nonce->length, nonce->data);

      umac96_update(&ctx96, length, input);

      check_digest ("umac96 (alignment)",
		    &ctx96, (nettle_hash_digest_func *) umac96_digest,
		    msg, length, 12, ref128->data);

      umac128_set_key (&ctx128, key->data);
      umac128_set_nonce (&ctx128, nonce->length, nonce->data);

      umac128_update(&ctx128, length, input);

      check_digest ("umac128 (alignment)",
		    &ctx128, (nettle_hash_digest_func *) umac128_digest,
		    msg, length, 16, ref128->data);
    }
  free (buffer);
}

static void
test_incr (const struct tstring *key,
	   const struct tstring *nonce,
	   unsigned count,
	   const struct tstring *msg,
	   const struct tstring *ref32,
	   const struct tstring *ref64,
	   const struct tstring *ref128)
{
  struct umac32_ctx ctx32;
  struct umac64_ctx ctx64;
  struct umac96_ctx ctx96;
  struct umac128_ctx ctx128;

  unsigned i;

  ASSERT (key->length == UMAC_KEY_SIZE);
  ASSERT (ref32->length == 4 * count);
  ASSERT (ref64->length == 8 * count);
  ASSERT (ref128->length == 16 * count);
  umac32_set_key (&ctx32, key->data);
  umac64_set_key (&ctx64, key->data);
  umac96_set_key (&ctx96, key->data);
  umac128_set_key (&ctx128, key->data);
  if (nonce)
    {
      umac32_set_nonce (&ctx32, nonce->length, nonce->data);
      umac64_set_nonce (&ctx64, nonce->length, nonce->data);
      umac96_set_nonce (&ctx96, nonce->length, nonce->data);
      umac128_set_nonce (&ctx128, nonce->length, nonce->data);
    }
  for (i = 0; i < count; i++)
    {
      umac32_update (&ctx32, msg->length, msg->data);
      check_digest ("umac32 incr",
		    &ctx32, (nettle_hash_digest_func *) umac32_digest,
		    msg, i, 4, ref32->data + 4*i);

      umac64_update (&ctx64, msg->length, msg->data);
      check_digest ("umac64 incr",
		    &ctx64, (nettle_hash_digest_func *) umac64_digest,
		    msg, i, 8, ref64->data + 8*i);

      umac96_update (&ctx96, msg->length, msg->data);
      check_digest ("umac96 incr",
		    &ctx96, (nettle_hash_digest_func *) umac96_digest,
		    msg, i, 12, ref128->data + 16*i);

      umac128_update (&ctx128, msg->length, msg->data);
      check_digest ("umac128 incr",
		    &ctx128, (nettle_hash_digest_func *) umac128_digest,
		    msg, i, 16, ref128->data + 16*i);

    }
}

void
test_main(void)
{
  /* From RFC 4418 (except that it lacks the last 32 bits of 128-bit
     tags) */
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghi"),
	     SDATA(""), 0,
	     SHEX("113145FB"),
	     SHEX("6E155FAD26900BE1"),
	     SHEX("32fedb100c79ad58f07ff7643cc60465"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghi"),
	     SDATA("a"), 3,
	     SHEX("3B91D102"),
	     SHEX("44B5CB542F220104"),
	     SHEX("185e4fe905cba7bd85e4c2dc3d117d8d"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghi"),
	     SDATA("a"), 1<<10,
	     SHEX("599B350B"),
	     SHEX("26BF2F5D60118BD9"),
	     SHEX("7a54abe04af82d60fb298c3cbd195bcb"));

  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghi"),
	     SDATA("aaaaaaaa"), 1<<15,
	     SHEX("58DCF532"),
	     SHEX("27F8EF643B0D118D"),
	     SHEX("7b136bd911e4b734286ef2be501f2c3c"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghi"),
	     SDATA("aaaaaaaa"), 1<<20,
	     SHEX("DB6364D1"),
	     SHEX("A4477E87E9F55853"),
	     SHEX("f8acfa3ac31cfeea047f7b115b03bef5"));
  /* Needs POLY128 */
  /* For the 'a' * 2^25 testcase, see errata
     http://fastcrypto.org/umac/rfc4418.errata.txt */
  test_umac (SDATA("abcdefghijklmnop"), SDATA ("bcdefghi"),
	     SDATA ("aaaaaaaa"), 1<<25,
	     SHEX("85EE5CAE"),
	     SHEX("FACA46F856E9B45F"),
	     SHEX("a621c2457c0012e64f3fdae9e7e1870c"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA ("bcdefghi"),
	     SDATA ("abc"), 3,
	     SHEX("ABF3A3A0"),
	     SHEX("D4D7B9F6BD4FBFCF"),
	     SHEX("883c3d4b97a61976ffcf232308cba5a5"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA ("bcdefghi"),
	     SDATA ("abc"), 1500,
	     SHEX("ABEB3C8B"),
	     SHEX("D4CF26DDEFD5C01A"),
	     SHEX("8824a260c53c66a36c9260a62cb83aa1"));

  test_incr (SDATA("abcdefghijklmnop"), NULL, 6,
	     SDATA("zero"),
	     SHEX("a0e94011 8c6fea51 6d897143 db1b28c5 a75e23b7 44ea26be"),
	     SHEX("a0e940111c9c2cd5 6d8971434be8ee41 c9c9aef87e2be502"
		  "a0a112b593656107 a75e23b7d419e03a 950526f26a8cc07a"),
	     SHEX("a0e940111c9c2cd5fa59090e3ac2061f"
		  "cbbf18b799fd0f4afb9216e52a89f247"
		  "c9c9aef87e2be50237716af8e24f8959"
		  "d6e96ef461f54d1c85aa66cbd76ca336"
		  "a75e23b7d419e03a02d55ebf1ba62824"
		  "2e63031d182a59b84f148d9a91de70a3"));

  test_incr (SDATA("abcdefghijklmnop"), SDATA("a"), 5,
	     SDATA("nonce-a"),
	     SHEX("81b4ac24 b7e8aad0 f70246fe 0595f0bf a8e9fe85"),
	     SHEX("b7e8aad0da6e7f99 138814c6a03bdadf fb77dd1cd4c7074f"
		  "0595f0bf8585c7e2 817c0b7757cb60f7"),
	     SHEX("d7604bffb5e368da5fe564da0068d2cc"
		  "138814c6a03bdadff7f1666e1bd881aa"
		  "86a016d9e67957c8ab5ebb78a673e4e9"
		  "0595f0bf8585c7e28dfab00598d4e612"
		  "3266ec16a9d85b4f0dc74ec8272238a9"));

  test_incr (SDATA("abcdefghijklmnop"), SHEX("beafcafe"), 5,
	     SDATA("nonce-beaf-cafe"),
	     SHEX("f19d9dc1 4604a56a 4ba9420e da86ff71 77facd79"),
	     SHEX("9e878413aa079032 9cfd7af0bb107748 4ba9420e55b6ba13"
		  "77facd797b686e24 9000c0de4f5f7236"),
	     SHEX("9e878413aa0790329604f3b6ae980e58"
		  "f2b2dd5dab08bb3bc5e9a83e1b4ab2e7"
		  "4ba9420e55b6ba137d03443f6ee01734"
		  "2721ca2e1bcda53a54ae65e0da139c0d"
		  "9000c0de4f5f7236b81ae1a52e78a821"));

  /* Tests exercising various sizes of nonce and data: All nonce
     lengths from 1 to 16 bytes. Data sizes chosen for testing for
     various off-by-one errors,

       0, 1, 2, 3, 4,
       1020, 1021, 1022, 1023, 1024, 1025, 1026, 1027,
       2046, 2047, 2048, 2049, 2050
       16777212, 16777213, 16777214, 16777215, 16777216, 16777217,
       16778239, 16778240, 16778241, 16778242, 16778243, 16778244
  */
  test_umac (SDATA("abcdefghijklmnop"), SDATA("b"),
	     SDATA("defdefdefdefdef"), 0,
	     SHEX("3a58486b"),
	     SHEX("9e38f67da91a08d9"),
	     SHEX("9e38f67da91a08d9c980f4db4089c877"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bc"),
	     SDATA("defdefdefdefdef"), 1,
	     SHEX("d86b1512"),
	     SHEX("fb0e207971b8e66a"),
	     SHEX("ef406c2ec70d0222f59e860eabb79ed0"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcd"),
	     SDATA("defdefdefdefdef"), 2,
	     SHEX("1ae6e02d"),
	     SHEX("1ae6e02d73aa9ab2"),
	     SHEX("1ae6e02d73aa9ab2a27fb89e014dc07b"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcde"),
	     SDATA("defdefdefdefdef"), 3,
	     SHEX("e8c1eb59"),
	     SHEX("c81cf22342e84302"),
	     SHEX("82626d0d575e01038e5e2cc6408216f5"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdef"),
	     SDATA("defdefdefdefdef"), 4,
	     SHEX("8950f0d3"),
	     SHEX("aba003e7bd673cc3"),
	     SHEX("aba003e7bd673cc368ba8513cecf2e7c"));

  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefg"),
	     SDATA("defdefdefdefdef"), 1020,
	     SHEX("7412167c"),
	     SHEX("f98828a161bb4ae3"),
	     SHEX("d8b4811f747d588d7a913360960de7cf"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefgh"),
	     SDATA("defdefdefdefdef"), 1021,
	     SHEX("2d54936b"),
	     SHEX("2d54936be5bff72d"),
	     SHEX("2d54936be5bff72d2e1052361163b474"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghi"),
	     SDATA("defdefdefdefdef"), 1022,
	     SHEX("53ca8dd2"),
	     SHEX("2cee9784556387b3"),
	     SHEX("700513397f8a210a98938d3e7ac3bd88"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghij"),
	     SDATA("defdefdefdefdef"), 1023,
	     SHEX("26cc58df"),
	     SHEX("24ac4284ca371f42"),
	     SHEX("24ac4284ca371f4280f60bd274633d67"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghijk"),
	     SDATA("defdefdefdefdef"), 1024,
	     SHEX("3cada45a"),
	     SHEX("64c6a0fd14615a76"),
	     SHEX("abc223116cedd2db5af365e641a97539"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghijkl"),
	     SDATA("defdefdefdefdef"), 1025,
	     SHEX("93251e18"),
	     SHEX("93251e18e56bbdc4"),
	     SHEX("93251e18e56bbdc457de556f95c59931"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghijklm"),
	     SDATA("defdefdefdefdef"), 1026,
	     SHEX("24a4c3ab"),
	     SHEX("5d98bd8dfaf16352"),
	     SHEX("c1298672e52386753383a15ed58c0e42"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghijklmn"),
	     SDATA("defdefdefdefdef"), 1027,
	     SHEX("e7e98945"),
	     SHEX("5b0557c9fdcf661b"),
	     SHEX("5b0557c9fdcf661b1758efc603516ebe"));

  /* Test varying the alignment of the buffer eventually passed to
     _umac_nh and _umac_nh_n. */
  test_align (SDATA("abcdefghijklmnop"), SDATA("bcdefghijk"),
	      SDATA("defdefdefdefdef"), 1024,
	      SHEX("3cada45a"),
	      SHEX("64c6a0fd14615a76"),
	      SHEX("abc223116cedd2db5af365e641a97539"));

  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghijklmno"),
	     SDATA("defdefdefdefdef"), 2046,
	     SHEX("e12ddc9f"),
	     SHEX("65e85d47447c2277"),
	     SHEX("16bb5183017826ed47c9995c1e5834f3"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghijklmnop"),
	     SDATA("defdefdefdefdef"), 2047,
	     SHEX("34d723a6"),
	     SHEX("34d723a6cb1676d3"),
	     SHEX("34d723a6cb1676d3547a5064dc5b0a37"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghijklmnopq"),
	     SDATA("defdefdefdefdef"), 2048,
	     SHEX("21fd8802"),
	     SHEX("3968d5d0af147884"),
	     SHEX("84565620def1e3a614d274e87626f215"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("b"),
	     SDATA("defdefdefdefdef"), 2049,
	     SHEX("097e5abd"),
	     SHEX("ad1ee4ab606061c5"),
	     SHEX("ad1ee4ab606061c55e0d2ecfee59940a"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bc"),
	     SDATA("defdefdefdefdef"), 2050,
	     SHEX("a03a7fe9"),
	     SHEX("835f4a8242100055"),
	     SHEX("971106d5f4a5e41dce40a91704cfe1f3"));

  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcd"),
	     SDATA("defdefdefdefdef"), 16777212,
	     SHEX("7ef41cf3"),
	     SHEX("7ef41cf351960aaf"),
	     SHEX("7ef41cf351960aaf729bb19fcee7d8c4"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcde"),
	     SDATA("defdefdefdefdef"), 16777213,
	     SHEX("8bf81932"),
	     SHEX("ab250048807ff640"),
	     SHEX("e15b9f6695c9b441de035e9b10b8ac32"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdef"),
	     SDATA("defdefdefdefdef"), 16777214,
	     SHEX("ddb2f0ab"),
	     SHEX("ff42039fcfe1248e"),
	     SHEX("ff42039fcfe1248e36c19efed14d7140"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefg"),
	     SDATA("defdefdefdefdef"), 16777215,
	     SHEX("e67ad507"),
	     SHEX("6be0ebda623d76df"),
	     SHEX("4adc426477fb64b1ce5afd76d505f048"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefgh"),
	     SDATA("defdefdefdefdef"), 16777216,
	     SHEX("42d8562a"),
	     SHEX("42d8562a224a9e9a"),
	     SHEX("42d8562a224a9e9a75c2f85d39462d07"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghi"),
	     SDATA("defdefdefdefdef"), 16777217,
	     SHEX("486b138d"),
	     SHEX("374f09dbb0b84b88"),
	     SHEX("6ba48d669a51ed3195ebc2aa562ee71b"));

  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghij"),
	     SDATA("defdefdefdefdef"), 16778239,
	     SHEX("850cb2c5"),
	     SHEX("876ca89ed045777b"),
	     SHEX("876ca89ed045777bf7efa7934e1758c2"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghijk"),
	     SDATA("defdefdefdefdef"), 16778240,
	     SHEX("b9fc4f81"),
	     SHEX("e1974b26fb35f2c6"),
	     SHEX("2e93c8ca83b97a6b1a21082e2a4c540d"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghijkl"),
	     SDATA("defdefdefdefdef"), 16778241,
	     SHEX("ffced8f2"),
	     SHEX("ffced8f2494d85bf"),
	     SHEX("ffced8f2494d85bf0cb39408ddfe0295"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghijklm"),
	     SDATA("defdefdefdefdef"), 16778242,
	     SHEX("1c99c5fb"),
	     SHEX("65a5bbdda3b85368"),
	     SHEX("f9148022bc6ab64f019e9db83704c17b"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghijklmn"),
	     SDATA("defdefdefdefdef"), 16778243,
	     SHEX("ec304be9"),
	     SHEX("50dc9565fbfc4884"),
	     SHEX(" 50dc9565fbfc48844a4be34403804605"));
  test_umac (SDATA("abcdefghijklmnop"), SDATA("bcdefghijklmno"),
	     SDATA("defdefdefdefdef"), 16778244,
	     SHEX("8034e26f"),
	     SHEX("04f163b7c2d5d849"),
	     SHEX("77a26f7387d1dcd39378a3220652cff7"));
}
