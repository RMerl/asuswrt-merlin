/* chacha-test.c

   Test program for the ChaCha stream cipher implementation.

   Copyright (C) 2013 Joachim Strömbergson
   Copyright (C) 2012, 2014 Niels Möller

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

#include "chacha.h"

static void
test_chacha(const struct tstring *key, const struct tstring *nonce,
	    const struct tstring *expected, unsigned rounds)
{
  struct chacha_ctx ctx;

  ASSERT (key->length == CHACHA_KEY_SIZE);
  chacha_set_key (&ctx, key->data);

  if (rounds == 20)
    {
      uint8_t *data = xalloc (expected->length + 2);
      size_t length;
      data++;

      for (length = 1; length <= expected->length; length++)
	{
	  data[-1] = 17;
	  memset (data, 0, length);
	  data[length] = 17;
	  if (nonce->length == CHACHA_NONCE_SIZE)
	    chacha_set_nonce(&ctx, nonce->data);
	  else if (nonce->length == CHACHA_NONCE96_SIZE)
	    {
	      chacha_set_nonce96(&ctx, nonce->data);
	      /* Use initial counter 1, for
		 draft-irtf-cfrg-chacha20-poly1305-08 test cases. */
	      ctx.state[12]++;
	    }
	  else
	    die ("Bad nonce size %u.\n", (unsigned) nonce->length);

	  chacha_crypt (&ctx, length, data, data);

	  ASSERT (data[-1] == 17);
	  ASSERT (data[length] == 17);
	  if (!MEMEQ(length, data, expected->data))
	    {
	      printf("Error, length %u, expected:\n", (unsigned) length);
	      print_hex (length, expected->data);
	      printf("Got:\n");
	      print_hex(length, data);
	      FAIL ();
	    }
	}
      if (verbose)
	{
	  printf("Result after encryption:\n");
	  print_hex(expected->length, data);
	}
      free (data - 1);
    }
  else
    {
      /* Uses the _chacha_core function to be able to test different
	 numbers of rounds. */
      uint32_t out[_CHACHA_STATE_LENGTH];
      ASSERT (expected->length == CHACHA_BLOCK_SIZE);
      ASSERT (nonce->length == CHACHA_NONCE_SIZE);

      chacha_set_nonce(&ctx, nonce->data);
      _chacha_core (out, ctx.state, rounds);

      if (!MEMEQ(CHACHA_BLOCK_SIZE, out, expected->data))
	{
	  printf("Error, expected:\n");
	  tstring_print_hex (expected);
	  printf("Got:\n");
	  print_hex(CHACHA_BLOCK_SIZE, (uint8_t *) out);
	  FAIL ();
	}

      if (verbose)
	{
	  printf("Result after encryption:\n");
	  print_hex(CHACHA_BLOCK_SIZE, (uint8_t *) out);
	}
    }
}

void
test_main(void)
{
  /* Test vectors from draft-strombergson-chacha-test-vectors */
#if 0
  /* TC1: All zero key and IV. 128 bit key and 8 rounds. */
  test_chacha (SHEX("0000000000000000 0000000000000000"),
	       SHEX("0000000000000000"),
	       SHEX("e28a5fa4a67f8c5d efed3e6fb7303486"
		    "aa8427d31419a729 572d777953491120"
		    "b64ab8e72b8deb85 cd6aea7cb6089a10"
		    "1824beeb08814a42 8aab1fa2c816081b"),
	       8);

  test_chacha (SHEX("0000000000000000 0000000000000000"),
	       SHEX("0000000000000000"),
	       SHEX("e1047ba9476bf8ff 312c01b4345a7d8c"
		    "a5792b0ad467313f 1dc412b5fdce3241"
		    "0dea8b68bd774c36 a920f092a04d3f95"
		    "274fbeff97bc8491 fcef37f85970b450"),
	       12);

  test_chacha (SHEX("0000000000000000 0000000000000000"),
	       SHEX("0000000000000000"),
	       SHEX("89670952608364fd 00b2f90936f031c8"
		    "e756e15dba04b849 3d00429259b20f46"
		    "cc04f111246b6c2c e066be3bfb32d9aa"
		    "0fddfbc12123d4b9 e44f34dca05a103f"

		    "6cd135c2878c832b 5896b134f6142a9d"
		    "4d8d0d8f1026d20a 0a81512cbce6e975"
		    "8a7143d021978022 a384141a80cea306"
		    "2f41f67a752e66ad 3411984c787e30ad"),
	       20);
#endif
  test_chacha (SHEX("0000000000000000 0000000000000000"
		    "0000000000000000 0000000000000000"),
	       SHEX("0000000000000000"),
	       SHEX("3e00ef2f895f40d6 7f5bb8e81f09a5a1"
		    "2c840ec3ce9a7f3b 181be188ef711a1e"
		    "984ce172b9216f41 9f445367456d5619"
		    "314a42a3da86b001 387bfdb80e0cfe42"

		    /* "d2aefa0deaa5c151 bf0adb6c01f2a5ad"
		    "c0fd581259f9a2aa dcf20f8fd566a26b"
		    "5032ec38bbc5da98 ee0c6f568b872a65"
		    "a08abf251deb21bb 4b56e5d8821e68aa" */),
	       8);

  test_chacha (SHEX("0000000000000000 0000000000000000"
		    "0000000000000000 0000000000000000"),
	       SHEX("0000000000000000"),
	       SHEX("9bf49a6a0755f953 811fce125f2683d5"
		    "0429c3bb49e07414 7e0089a52eae155f"
		    "0564f879d27ae3c0 2ce82834acfa8c79"
		    "3a629f2ca0de6919 610be82f411326be"

		    /* "0bd58841203e74fe 86fc71338ce0173d"
		    "c628ebb719bdcbcc 151585214cc089b4"
		    "42258dcda14cf111 c602b8971b8cc843"
		    "e91e46ca905151c0 2744a6b017e69316" */),
	       12);

  test_chacha (SHEX("0000000000000000 0000000000000000"
		    "0000000000000000 0000000000000000"),
	       SHEX("0000000000000000"),
	       SHEX("76b8e0ada0f13d90 405d6ae55386bd28"
		    "bdd219b8a08ded1a a836efcc8b770dc7"
		    "da41597c5157488d 7724e03fb8d84a37"
		    "6a43b8f41518a11c c387b669b2ee6586"

		    "9f07e7be5551387a 98ba977c732d080d"
		    "cb0f29a048e36569 12c6533e32ee7aed"
		    "29b721769ce64e43 d57133b074d839d5"
		    "31ed1f28510afb45 ace10a1f4b794d6f"),
	       20);

  /* TC2: Single bit in key set. All zero IV */
#if 0
  test_chacha (SHEX("0100000000000000 0000000000000000"),
	       SHEX("0000000000000000"),
	       SHEX("03a7669888605a07 65e8357475e58673"
		    "f94fc8161da76c2a 3aa2f3caf9fe5449"
		    "e0fcf38eb882656a f83d430d410927d5"
		    "5c972ac4c92ab9da 3713e19f761eaa14"),
	       8);

  test_chacha (SHEX("0100000000000000 0000000000000000"),
	       SHEX("0000000000000000"),
	       SHEX("2a865a3b8999fa83 ae8aacf33fc6be4f"
		    "32c8aa9762738d26 963270052f4eef8b"
		    "86af758f7867560a f6d0eeb973b5542b"
		    "b24c8abceac8b1f3 6d026963d6c8a9b2"),
	       12);

  test_chacha (SHEX("0100000000000000 0000000000000000"),
	       SHEX("0000000000000000"),
	       SHEX("ae56060d04f5b597 897ff2af1388dbce"
		    "ff5a2a4920335dc1 7a3cb1b1b10fbe70"
		    "ece8f4864d8c7cdf 0076453a8291c7db"
		    "eb3aa9c9d10e8ca3 6be4449376ed7c42"

		    "fc3d471c34a36fbb f616bc0a0e7c5230"
		    "30d944f43ec3e78d d6a12466547cb4f7"
		    "b3cebd0a5005e762 e562d1375b7ac445"
		    "93a991b85d1a60fb a2035dfaa2a642d5"),
	       20);
#endif
  test_chacha (SHEX("0100000000000000 0000000000000000"
		    "0000000000000000 0000000000000000"),
	       SHEX("0000000000000000"),
	       SHEX("cf5ee9a0494aa961 3e05d5ed725b804b"
		    "12f4a465ee635acc 3a311de8740489ea"
		    "289d04f43c7518db 56eb4433e498a123"
		    "8cd8464d3763ddbb 9222ee3bd8fae3c8"),
	       8);

  test_chacha (SHEX("0100000000000000 0000000000000000"
		    "0000000000000000 0000000000000000"),
	       SHEX("0000000000000000"),
	       SHEX("12056e595d56b0f6 eef090f0cd25a209"
		    "49248c2790525d0f 930218ff0b4ddd10"
		    "a6002239d9a454e2 9e107a7d06fefdfe"
		    "f0210feba044f9f2 9b1772c960dc29c0"),
	       12);

  test_chacha (SHEX("0100000000000000 0000000000000000"
		    "0000000000000000 0000000000000000"),
	       SHEX("0000000000000000"),
	       SHEX("c5d30a7ce1ec1193 78c84f487d775a85"
		    "42f13ece238a9455 e8229e888de85bbd"
		    "29eb63d0a17a5b99 9b52da22be4023eb"
		    "07620a54f6fa6ad8 737b71eb0464dac0"

		    "10f656e6d1fd5505 3e50c4875c9930a3"
		    "3f6d0263bd14dfd6 ab8c70521c19338b"
		    "2308b95cf8d0bb7d 202d2102780ea352"
		    "8f1cb48560f76b20 f382b942500fceac"),
	       20);

  /* TC3: Single bit in IV set. All zero key */
#if 0
  test_chacha (SHEX("0000000000000000 0000000000000000"),
	       SHEX("0100000000000000"),
	       SHEX("25f5bec6683916ff 44bccd12d102e692"
		    "176663f4cac53e71 9509ca74b6b2eec8"
		    "5da4236fb2990201 2adc8f0d86c8187d"
		    "25cd1c486966930d 0204c4ee88a6ab35"),
	       8);

  test_chacha (SHEX("0000000000000000 0000000000000000"),
	       SHEX("0100000000000000"),
	       SHEX("91cdb2f180bc89cf e86b8b6871cd6b3a"
		    "f61abf6eba01635d b619c40a0b2e19ed"
		    "fa8ce5a9bd7f53cc 2c9bcfea181e9754"
		    "a9e245731f658cc2 82c2ae1cab1ae02c"),
	       12);

  test_chacha (SHEX("0000000000000000 0000000000000000"),
	       SHEX("0100000000000000"),
	       SHEX("1663879eb3f2c994 9e2388caa343d361"
		    "bb132771245ae6d0 27ca9cb010dc1fa7"
		    "178dc41f8278bc1f 64b3f12769a24097"
		    "f40d63a86366bdb3 6ac08abe60c07fe8"

		    "b057375c89144408 cc744624f69f7f4c"
		    "cbd93366c92fc4df cada65f1b959d8c6"
		    "4dfc50de711fb464 16c2553cc60f21bb"
		    "fd006491cb17888b 4fb3521c4fdd8745"),
	       20);
#endif
  test_chacha (SHEX("0000000000000000 0000000000000000"
		    "0000000000000000 0000000000000000"),
	       SHEX("0100000000000000"),
	       SHEX("2b8f4bb3798306ca 5130d47c4f8d4ed1"
		    "3aa0edccc1be6942 090faeeca0d7599b"
		    "7ff0fe616bb25aa0 153ad6fdc88b9549"
		    "03c22426d478b97b 22b8f9b1db00cf06"),
	       8);

  test_chacha (SHEX("0000000000000000 0000000000000000"
		    "0000000000000000 0000000000000000"),
	       SHEX("0100000000000000"),
	       SHEX("64b8bdf87b828c4b 6dbaf7ef698de03d"
		    "f8b33f635714418f 9836ade59be12969"
		    "46c953a0f38ecffc 9ecb98e81d5d99a5"
		    "edfc8f9a0a45b9e4 1ef3b31f028f1d0f"),
	       12);

  test_chacha (SHEX("0000000000000000 0000000000000000"
		    "0000000000000000 0000000000000000"),
	       SHEX("0100000000000000"),
	       SHEX("ef3fdfd6c61578fb f5cf35bd3dd33b80"
		    "09631634d21e42ac 33960bd138e50d32"
		    "111e4caf237ee53c a8ad6426194a8854"
		    "5ddc497a0b466e7d 6bbdb0041b2f586b"

		    "5305e5e44aff19b2 35936144675efbe4"
		    "409eb7e8e5f1430f 5f5836aeb49bb532"
		    "8b017c4b9dc11f8a 03863fa803dc71d5"
		    "726b2b6b31aa3270 8afe5af1d6b69058"),
	       20);

  /* TC4: All bits in key and IV are set. */
#if 0
  test_chacha (SHEX("ffffffffffffffff ffffffffffffffff"),
	       SHEX("ffffffffffffffff"),
	       SHEX("2204d5b81ce66219 3e00966034f91302"
		    "f14a3fb047f58b6e 6ef0d72113230416"
		    "3e0fb640d76ff9c3 b9cd99996e6e38fa"
		    "d13f0e31c82244d3 3abbc1b11e8bf12d"),
	       8);

  test_chacha (SHEX("ffffffffffffffff ffffffffffffffff"),
	       SHEX("ffffffffffffffff"),
	       SHEX("60e349e60c38b328 c4baab90d44a7c72"
		    "7662770d36350d65 a1433bd92b00ecf4"
		    "83d5597d7a616258 ec3c5d5b30e1c5c8"
		    "5c5dfe2f92423b8e 36870f3185b6add9"),
	       12);

  test_chacha (SHEX("ffffffffffffffff ffffffffffffffff"),
	       SHEX("ffffffffffffffff"),
	       SHEX("992947c3966126a0 e660a3e95db048de"
		    "091fb9e0185b1e41 e41015bb7ee50150"
		    "399e4760b262f9d5 3f26d8dd19e56f5c"
		    "506ae0c3619fa67f b0c408106d0203ee"

		    "40ea3cfa61fa32a2 fda8d1238a2135d9"
		    "d4178775240f9900 7064a6a7f0c731b6"
		    "7c227c52ef796b6b ed9f9059ba0614bc"
		    "f6dd6e38917f3b15 0e576375be50ed67"),
	       20);
#endif
  test_chacha (SHEX("ffffffffffffffff ffffffffffffffff"
		    "ffffffffffffffff ffffffffffffffff"),
	       SHEX("ffffffffffffffff"),
	       SHEX("e163bbf8c9a739d1 8925ee8362dad2cd"
		    "c973df05225afb2a a26396f2a9849a4a"
		    "445e0547d31c1623 c537df4ba85c70a9"
		    "884a35bcbf3dfab0 77e98b0f68135f54"),
	       8);

  test_chacha (SHEX("ffffffffffffffff ffffffffffffffff"
		    "ffffffffffffffff ffffffffffffffff"),
	       SHEX("ffffffffffffffff"),
	       SHEX("04bf88dae8e47a22 8fa47b7e6379434b"
		    "a664a7d28f4dab84 e5f8b464add20c3a"
		    "caa69c5ab221a23a 57eb5f345c96f4d1"
		    "322d0a2ff7a9cd43 401cd536639a615a"),
	       12);

  test_chacha (SHEX("ffffffffffffffff ffffffffffffffff"
		    "ffffffffffffffff ffffffffffffffff"),
	       SHEX("ffffffffffffffff"),
	       SHEX("d9bf3f6bce6ed0b5 4254557767fb5744"
		    "3dd4778911b60605 5c39cc25e674b836"
		    "3feabc57fde54f79 0c52c8ae43240b79"
		    "d49042b777bfd6cb 80e931270b7f50eb"

		    "5bac2acd86a836c5 dc98c116c1217ec3"
		    "1d3a63a9451319f0 97f3b4d6dab07787"
		    "19477d24d24b403a 12241d7cca064f79"
		    "0f1d51ccaff6b166 7d4bbca1958c4306"),
	       20);

  /* TC5: Every even bit set in key and IV. */
#if 0
  test_chacha (SHEX("5555555555555555 5555555555555555"),
	       SHEX("5555555555555555"),
	       SHEX("f0a23bc36270e18e d0691dc384374b9b"
		    "2c5cb60110a03f56 fa48a9fbbad961aa"
		    "6bab4d892e96261b 6f1a0919514ae56f"
		    "86e066e17c71a417 6ac684af1c931996"),
	       8);

  test_chacha (SHEX("5555555555555555 5555555555555555"),
	       SHEX("5555555555555555"),
	       SHEX("90ec7a49ee0b20a8 08af3d463c1fac6c"
		    "2a7c897ce8f6e60d 793b62ddbebcf980"
		    "ac917f091e52952d b063b1d2b947de04"
		    "aac087190ca99a35 b5ea501eb535d570"),
	       12);

  test_chacha (SHEX("5555555555555555 5555555555555555"),
	       SHEX("5555555555555555"),
	       SHEX("357d7d94f966778f 5815a2051dcb0413"
		    "3b26b0ead9f57dd0 9927837bc3067e4b"
		    "6bf299ad81f7f50c 8da83c7810bfc17b"
		    "b6f4813ab6c32695 7045fd3fd5e19915"

		    "ec744a6b9bf8cbdc b36d8b6a5499c68a"
		    "08ef7be6cc1e93f2 f5bcd2cad4e47c18"
		    "a3e5d94b5666382c 6d130d822dd56aac"
		    "b0f8195278e7b292 495f09868ddf12cc"),
	       20);
#endif
  test_chacha (SHEX("5555555555555555 5555555555555555"
		    "5555555555555555 5555555555555555"),
	       SHEX("5555555555555555"),
	       SHEX("7cb78214e4d3465b 6dc62cf7a1538c88"
		    "996952b4fb72cb61 05f1243ce3442e29"
		    "75a59ebcd2b2a598 290d7538491fe65b"
		    "dbfefd060d887981 20a70d049dc2677d"),
	       8);

  test_chacha (SHEX("5555555555555555 5555555555555555"
		    "5555555555555555 5555555555555555"),
	       SHEX("5555555555555555"),
	       SHEX("a600f07727ff93f3 da00dd74cc3e8bfb"
		    "5ca7302f6a0a2944 953de00450eecd40"
		    "b860f66049f2eaed 63b2ef39cc310d2c"
		    "488f5d9a241b615d c0ab70f921b91b95"),
	       12);

  test_chacha (SHEX("5555555555555555 5555555555555555"
		    "5555555555555555 5555555555555555"),
	       SHEX("5555555555555555"),
	       SHEX("bea9411aa453c543 4a5ae8c92862f564"
		    "396855a9ea6e22d6 d3b50ae1b3663311"
		    "a4a3606c671d605c e16c3aece8e61ea1"
		    "45c59775017bee2f a6f88afc758069f7"

		    "e0b8f676e644216f 4d2a3422d7fa36c6"
		    "c4931aca950e9da4 2788e6d0b6d1cd83"
		    "8ef652e97b145b14 871eae6c6804c700"
		    "4db5ac2fce4c68c7 26d004b10fcaba86"),
	       20);

  /* TC6: Every odd bit set in key and IV. */
#if 0
  test_chacha (SHEX("aaaaaaaaaaaaaaaa aaaaaaaaaaaaaaaa"),
	       SHEX("aaaaaaaaaaaaaaaa"),
	       SHEX("312d95c0bc38eff4 942db2d50bdc500a"
		    "30641ef7132db1a8 ae838b3bea3a7ab0"
		    "3815d7a4cc09dbf5 882a3433d743aced"
		    "48136ebab7329950 6855c0f5437a36c6"),
	       8);

  test_chacha (SHEX("aaaaaaaaaaaaaaaa aaaaaaaaaaaaaaaa"),
	       SHEX("aaaaaaaaaaaaaaaa"),
	       SHEX("057fe84fead13c24 b76bb2a6fdde66f2"
		    "688e8eb6268275c2 2c6bcb90b85616d7"
		    "fe4d3193a1036b70 d7fb864f01453641"
		    "851029ecdb60ac38 79f56496f16213f4"),
	       12);

  test_chacha (SHEX("aaaaaaaaaaaaaaaa aaaaaaaaaaaaaaaa"),
	       SHEX("aaaaaaaaaaaaaaaa"),
	       SHEX("fc79acbd58526103 862776aab20f3b7d"
		    "8d3149b2fab65766 299316b6e5b16684"
		    "de5de548c1b7d083 efd9e3052319e0c6"
		    "254141da04a6586d f800f64d46b01c87"

		    "1f05bc67e07628eb e6f6865a2177e0b6"
		    "6a558aa7cc1e8ff1 a98d27f7071f8335"
		    "efce4537bb0ef7b5 73b32f32765f2900"
		    "7da53bba62e7a44d 006f41eb28fe15d6"),
	       20);
#endif
  test_chacha (SHEX("aaaaaaaaaaaaaaaa aaaaaaaaaaaaaaaa"
		    "aaaaaaaaaaaaaaaa aaaaaaaaaaaaaaaa"),
	       SHEX("aaaaaaaaaaaaaaaa"),
	       SHEX("40f9ab86c8f9a1a0 cdc05a75e5531b61"
		    "2d71ef7f0cf9e387 df6ed6972f0aae21"
		    "311aa581f816c90e 8a99de990b6b95aa"
		    "c92450f4e1127126 67b804c99e9c6eda"),
	       8);

  test_chacha (SHEX("aaaaaaaaaaaaaaaa aaaaaaaaaaaaaaaa"
		    "aaaaaaaaaaaaaaaa aaaaaaaaaaaaaaaa"),
	       SHEX("aaaaaaaaaaaaaaaa"),
	       SHEX("856505b01d3b47aa e03d6a97aa0f033a"
		    "9adcc94377babd86 08864fb3f625b6e3"
		    "14f086158f9f725d 811eeb953b7f7470"
		    "76e4c3f639fa841f ad6c9a709e621397"),
	       12);

  test_chacha (SHEX("aaaaaaaaaaaaaaaa aaaaaaaaaaaaaaaa"
		    "aaaaaaaaaaaaaaaa aaaaaaaaaaaaaaaa"),
	       SHEX("aaaaaaaaaaaaaaaa"),
	       SHEX("9aa2a9f656efde5a a7591c5fed4b35ae"
		    "a2895dec7cb4543b 9e9f21f5e7bcbcf3"
		    "c43c748a970888f8 248393a09d43e0b7"
		    "e164bc4d0b0fb240 a2d72115c4808906"

		    "72184489440545d0 21d97ef6b693dfe5"
		    "b2c132d47e6f041c 9063651f96b623e6"
		    "2a11999a23b6f7c4 61b2153026ad5e86"
		    "6a2e597ed07b8401 dec63a0934c6b2a9"),
	       20);

  /* TC7: Sequence patterns in key and IV. */
#if 0
  test_chacha (SHEX("0011223344556677 8899aabbccddeeff"),
	       SHEX("0f1e2d3c4b5a6978"),
	       SHEX("29560d280b452840 0a8f4b795369fb3a"
		    "01105599e9f1ed58 279cfc9ece2dc5f9"
		    "9f1c2e52c98238f5 42a5c0a881d850b6"
		    "15d3acd9fbdb026e 9368565da50e0d49"),
	       8);

  test_chacha (SHEX("0011223344556677 8899aabbccddeeff"),
	       SHEX("0f1e2d3c4b5a6978"),
	       SHEX("5eddc2d9428fceee c50a52a964eae0ff"
		    "b04b2de006a9b04c ff368ffa921116b2"
		    "e8e264babd2efa0d e43ef2e3b6d065e8"
		    "f7c0a17837b0a40e b0e2c7a3742c8753"),
	       12);

  test_chacha (SHEX("0011223344556677 8899aabbccddeeff"),
	       SHEX("0f1e2d3c4b5a6978"),
	       SHEX("d1abf630467eb4f6 7f1cfb47cd626aae"
		    "8afedbbe4ff8fc5f e9cfae307e74ed45"
		    "1f1404425ad2b545 69d5f18148939971"
		    "abb8fafc88ce4ac7 fe1c3d1f7a1eb7ca"

		    "e76ca87b61a97135 41497760dd9ae059"
		    "350cad0dcedfaa80 a883119a1a6f987f"
		    "d1ce91fd8ee08280 34b411200a9745a2"
		    "85554475d12afc04 887fef3516d12a2c"),
	       20);
#endif
  test_chacha (SHEX("0011223344556677 8899aabbccddeeff"
		    "ffeeddccbbaa9988 7766554433221100"),
	       SHEX("0f1e2d3c4b5a6978"),
	       SHEX("db43ad9d1e842d12 72e4530e276b3f56"
		    "8f8859b3f7cf6d9d 2c74fa53808cb515"
		    "7a8ebf46ad3dcc4b 6c7dadde131784b0"
		    "120e0e22f6d5f9ff a7407d4a21b695d9"),
	       8);

  test_chacha (SHEX("0011223344556677 8899aabbccddeeff"
		    "ffeeddccbbaa9988 7766554433221100"),
	       SHEX("0f1e2d3c4b5a6978"),
	       SHEX("7ed12a3a63912ae9 41ba6d4c0d5e862e"
		    "568b0e5589346935 505f064b8c2698db"
		    "f7d850667d8e67be 639f3b4f6a16f92e"
		    "65ea80f6c7429445 da1fc2c1b9365040"),
	       12);

  test_chacha (SHEX("0011223344556677 8899aabbccddeeff"
		    "ffeeddccbbaa9988 7766554433221100"),
	       SHEX("0f1e2d3c4b5a6978"),
	       SHEX("9fadf409c00811d0 0431d67efbd88fba"
		    "59218d5d6708b1d6 85863fabbb0e961e"
		    "ea480fd6fb532bfd 494b215101505742"
		    "3ab60a63fe4f55f7 a212e2167ccab931"

		    "fbfd29cf7bc1d279 eddf25dd316bb884"
		    "3d6edee0bd1ef121 d12fa17cbc2c574c"
		    "ccab5e275167b08b d686f8a09df87ec3"
		    "ffb35361b94ebfa1 3fec0e4889d18da5"),
	       20);

  /* TC8: hashed string patterns */
#if 0
  test_chacha(SHEX("c46ec1b18ce8a878 725a37e780dfb735"),
	      SHEX("1ada31d5cf688221"),
	      SHEX("6a870108859f6791 18f3e205e2a56a68"
		   "26ef5a60a4102ac8 d4770059fcb7c7ba"
		   "e02f5ce004a6bfbb ea53014dd82107c0"
		   "aa1c7ce11b7d78f2 d50bd3602bbd2594"),
	      8);

  test_chacha(SHEX("c46ec1b18ce8a878 725a37e780dfb735"),
	      SHEX("1ada31d5cf688221"),
	      SHEX("b02bd81eb55c8f68 b5e9ca4e307079bc"
		   "225bd22007eddc67 02801820709ce098"
		   "07046a0d2aa552bf dbb49466176d56e3"
		   "2d519e10f5ad5f27 46e241e09bdf9959"),
	      12);

  test_chacha(SHEX("c46ec1b18ce8a878 725a37e780dfb735"),
	      SHEX("1ada31d5cf688221"),
	      SHEX("826abdd84460e2e9 349f0ef4af5b179b"
		   "426e4b2d109a9c5b b44000ae51bea90a"
		   "496beeef62a76850 ff3f0402c4ddc99f"
		   "6db07f151c1c0dfa c2e56565d6289625"

		   "5b23132e7b469c7b fb88fa95d44ca5ae"
		   "3e45e848a4108e98 bad7a9eb15512784"
		   "a6a9e6e591dce674 120acaf9040ff50f"
		   "f3ac30ccfb5e1420 4f5e4268b90a8804"),
	      20);
#endif
  test_chacha(SHEX("c46ec1b18ce8a878 725a37e780dfb735"
		   "1f68ed2e194c79fb c6aebee1a667975d"),
	      SHEX("1ada31d5cf688221"),
	      SHEX("838751b42d8ddd8a 3d77f48825a2ba75"
		   "2cf4047cb308a597 8ef274973be374c9"
		   "6ad848065871417b 08f034e681fe46a9"
		   "3f7d5c61d1306614 d4aaf257a7cff08b"),
	      8);

  test_chacha(SHEX("c46ec1b18ce8a878 725a37e780dfb735"
		   "1f68ed2e194c79fb c6aebee1a667975d"),
	      SHEX("1ada31d5cf688221"),
	      SHEX("1482072784bc6d06 b4e73bdc118bc010"
		   "3c7976786ca918e0 6986aa251f7e9cc1"
		   "b2749a0a16ee83b4 242d2e99b08d7c20"
		   "092b80bc466c8728 3b61b1b39d0ffbab"),
	      12);

  test_chacha(SHEX("c46ec1b18ce8a878 725a37e780dfb735"
		   "1f68ed2e194c79fb c6aebee1a667975d"),
	      SHEX("1ada31d5cf688221"),
	      SHEX("f63a89b75c2271f9 368816542ba52f06"
		   "ed49241792302b00 b5e8f80ae9a473af"
		   "c25b218f519af0fd d406362e8d69de7f"
		   "54c604a6e00f353f 110f771bdca8ab92"

		   "e5fbc34e60a1d9a9 db17345b0a402736"
		   "853bf910b060bdf1 f897b6290f01d138"
		   "ae2c4c90225ba9ea 14d518f55929dea0"
		   "98ca7a6ccfe61227 053c84e49a4a3332"),
	      20);

  /* From draft-irtf-cfrg-chacha20-poly1305-08, with 96-bit nonce */
  test_chacha(SHEX("0001020304050607 08090a0b0c0d0e0f"
		   "1011121314151617 18191a1b1c1d1e1f"),
	      SHEX("000000090000004a 00000000"),
	      SHEX("10f1e7e4d13b5915 500fdd1fa32071c4"
		   "c7d1f4c733c06803 0422aa9ac3d46c4e"
		   "d2826446079faa09 14c2d705d98b02a2"
		   "b5129cd1de164eb9 cbd083e8a2503c4e"),
	      20);
}
