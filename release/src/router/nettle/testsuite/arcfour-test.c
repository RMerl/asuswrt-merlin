#include "testutils.h"
#include "arcfour.h"

static void
test_arcfour(const struct tstring *key,
	     const struct tstring *cleartext,
	     const struct tstring *ciphertext)
{
  size_t block;
  struct arcfour_ctx ctx;

  uint8_t *data;
  size_t length;

  ASSERT (cleartext->length == ciphertext->length);
  length = cleartext->length;

  data = xalloc(length + 1);

  for (block = 1; block <= length; block++)
    {
      size_t i;

      memset(data, 0x17, length + 1);
      arcfour_set_key(&ctx, key->length, key->data);

      for (i = 0; i + block < length; i += block)
	{
	  arcfour_crypt(&ctx, block, data + i, cleartext->data + i);
	  ASSERT (data[i + block] == 0x17);
	}

      arcfour_crypt(&ctx, length - i, data + i, cleartext->data + i);
      ASSERT (data[length] == 0x17);

      if (!MEMEQ(length, data, ciphertext->data))
	{
	  fprintf(stderr, "Encrypt failed, block size %lu\nInput:",
		  (unsigned long) block);
	  tstring_print_hex(cleartext);
	  fprintf(stderr, "\nOutput: ");
	  print_hex(length, data);
	  fprintf(stderr, "\nExpected:");
	  tstring_print_hex(ciphertext);
	  fprintf(stderr, "\n");
	  FAIL();
	}
    }

  arcfour_set_key(&ctx, key->length, key->data);
  arcfour_crypt(&ctx, length, data, data);

  ASSERT (data[length] == 0x17);

  if (!MEMEQ(length, data, cleartext->data))
    {
      fprintf(stderr, "Decrypt failed\nInput:");
      tstring_print_hex(ciphertext);
      fprintf(stderr, "\nOutput: ");
      print_hex(length, data);
      fprintf(stderr, "\nExpected:");
      tstring_print_hex(cleartext);
      fprintf(stderr, "\n");
      FAIL();
    }

  free(data);
}

void
test_main(void)
{
  test_arcfour(SHEX("01234567 89ABCDEF 00000000 00000000"),
	       SHEX("01234567 89ABCDEF"),
	       SHEX("69723659 1B5242B1"));

  /* More data. This ensures that we get some collisions between the S
     accesses at index i,j and the access at si + sj. I.e. the cases
     where the ordering of loads and stores matter. */
  test_arcfour(SHEX("aaaaaaaa bbbbbbbb cccccccc dddddddd"),
	       SHEX("00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"

		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
			
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
			
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
			
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
			
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
			
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
			
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"),
	       SHEX("a2b35dc7 bf95ae1e 1c432d15 f4fb8c1c"
		    "f264e1d0 bd090831 6caa7d17 5401ae67"
		    "3cfbd140 fd3dee42 1012d674 2fb69fa3"
		    "6522631e bb3d4703 535de1ce 4a81ddce"

		    "5780cfe0 b5fc9fae ebe14c96 26451bd9"
		    "992f2204 119cbe37 cbdc453c 7afa08c7"
		    "1380ccf8 48f81e53 a535cdfb 96c64faa"
		    "c3f759d0 fa1ff920 008d95cf 39d52324"

		    "d0aac3f9 749b22e2 6a065145 06fb249d"
		    "ffb8e05e cb0381fe 5346a04a 63dac61c"
		    "10b6683e 3ab427de d4c6bc60 6366545e"
		    "77d0e121 96037717 a745d49e e72a70aa"

		    "a50a612d 879b0580 fd4a89ae 3ee49871"
		    "2cf6c98d a62dfbc7 d7b2d901 2c3aaf27"
		    "42b7e089 ef2466ac 450b440c 138daa1a"
		    "cf9ebef6 f66a7a64 2677b213 06640130"

		    "de6651df 0065180d 4db366ba 9c377712"
		    "53d21cac 82ed72a4 c6c4d81e 4375fea3"
		    "1f935909 95322c83 13c64d8e 829c93a6"
		    "d540a1b3 20f41541 96800888 1a7afc9b"

		    "e39e89fc 3ac78be5 cdbbf774 33c36863"
		    "da2a3b1b d06e54a9 aa4b7edd 70b34941"
		    "b886f7db f36c3def f9fc4c80 7ce55ea5"
		    "98a7257b f68a9e1d caf4bfd6 43bd9853"

		    "c966629d 54e34221 6e140780 d48c69bb"
		    "5e77e886 86f2ebcb 807732d5 d29bc384"
		    "a4ca1c31 c7c1b5b9 85dbfcf1 8d845905"
		    "a0ff487a b4a3f252 a75caebf 857ba48b"

		    "613e3067 92cada3e 0e07f599 2f4794f3"
		    "af01f15a 491732fb 22aa09a3 d2e1e408"
		    "fe94bdb4 993c68b1 1bb79eb1 bb7ec446"
		    "760ef7bf 2caa8713 479760e5 a6e143cd"));
}
