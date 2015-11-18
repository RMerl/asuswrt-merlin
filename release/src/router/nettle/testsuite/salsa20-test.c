#include "testutils.h"
#include "salsa20.h"

#include "memxor.h"

static int
memzero_p (const uint8_t *p, size_t n)
{
  size_t i;
  for (i = 0; i < n; i++)
    if (p[i])
      return 0;
  return 1;
}

/* The ecrypt testcases encrypt 512 zero bytes (8 blocks), then give
   the xor of all blocks, and the data for block 0 (0-63), 3,4
   (192-319), 7 (448-511) */

#define STREAM_LENGTH 512
static void
test_salsa20_stream(const struct tstring *key,
		    const struct tstring *iv,
		    const struct tstring *ciphertext,
		    const struct tstring *xor_ref)
{
  struct salsa20_ctx ctx;
  uint8_t data[STREAM_LENGTH + 1];
  uint8_t stream[STREAM_LENGTH + 1];
  uint8_t xor[SALSA20_BLOCK_SIZE];
  size_t j;

  ASSERT (iv->length == SALSA20_IV_SIZE);
  ASSERT (ciphertext->length == 4*SALSA20_BLOCK_SIZE);
  ASSERT (xor_ref->length == SALSA20_BLOCK_SIZE);

  salsa20_set_key(&ctx, key->length, key->data);
  salsa20_set_iv(&ctx, iv->data);
  memset(stream, 0, STREAM_LENGTH + 1);
  salsa20_crypt(&ctx, STREAM_LENGTH, stream, stream);
  if (stream[STREAM_LENGTH])
    {
      fprintf(stderr, "Stream of %d bytes wrote too much!\n", STREAM_LENGTH);
      FAIL();
    }
  if (!MEMEQ (64, stream, ciphertext->data))
    {
      fprintf(stderr, "Error failed, offset 0:\n");
      fprintf(stderr, "\nOutput: ");
      print_hex(64, stream);
      fprintf(stderr, "\nExpected:");
      print_hex(64, ciphertext->data);
      fprintf(stderr, "\n");
      FAIL();
    }
  if (!MEMEQ (128, stream + 192, ciphertext->data + 64))
    {
      fprintf(stderr, "Error failed, offset 192:\n");
      fprintf(stderr, "\nOutput: ");
      print_hex(128, stream + 192);
      fprintf(stderr, "\nExpected:");
      print_hex(64, ciphertext->data + 64);
      fprintf(stderr, "\n");
      FAIL();
    }
  if (!MEMEQ (64, stream + 448, ciphertext->data + 192))
    {
      fprintf(stderr, "Error failed, offset 448:\n");
      fprintf(stderr, "\nOutput: ");
      print_hex(64, stream + 448);
      fprintf(stderr, "\nExpected:");
      print_hex(64, ciphertext->data + 192);
      fprintf(stderr, "\n");
      FAIL();
    }

  memxor3 (xor, stream, stream + SALSA20_BLOCK_SIZE, SALSA20_BLOCK_SIZE);
  for (j = 2*SALSA20_BLOCK_SIZE; j < STREAM_LENGTH; j += SALSA20_BLOCK_SIZE)
    memxor (xor, stream + j, SALSA20_BLOCK_SIZE);

  if (!MEMEQ (SALSA20_BLOCK_SIZE, xor, xor_ref->data))
    {
      fprintf(stderr, "Error failed, bad xor 448:\n");
      fprintf(stderr, "\nOutput: ");
      print_hex(SALSA20_BLOCK_SIZE, xor);
      fprintf(stderr, "\nExpected:");
      print_hex(SALSA20_BLOCK_SIZE, xor_ref->data);
      fprintf(stderr, "\n");
      FAIL();
    }

  for (j = 1; j <= STREAM_LENGTH; j++)
    {
      memset(data, 0, STREAM_LENGTH + 1);
      salsa20_set_iv(&ctx, iv->data);
      salsa20_crypt(&ctx, j, data, data);

      if (!MEMEQ(j, data, stream))
	{
	  fprintf(stderr, "Encrypt failed for length %lu:\n",
		  (unsigned long) j);
	  fprintf(stderr, "\nOutput: ");
	  print_hex(j, data);
	  fprintf(stderr, "\nExpected:");
	  print_hex(j, stream);
	  fprintf(stderr, "\n");
	  FAIL();
	}
      if (!memzero_p (data + j, STREAM_LENGTH + 1 - j))
	{
	  fprintf(stderr, "Encrypt failed for length %lu, wrote too much:\n",
		  (unsigned long) j);
	  fprintf(stderr, "\nOutput: ");
	  print_hex(STREAM_LENGTH + 1 - j, data + j);
	  fprintf(stderr, "\n");
	  FAIL();
	}
    }
}

typedef void salsa20_func(struct salsa20_ctx *ctx,
			  size_t length, uint8_t *dst,
			  const uint8_t *src);
static void
_test_salsa20(salsa20_func *crypt,
	      const struct tstring *key,
	      const struct tstring *iv,
	      const struct tstring *cleartext,
	      const struct tstring *ciphertext)
{
  struct salsa20_ctx ctx;
  uint8_t *data;
  size_t length;

  ASSERT (cleartext->length == ciphertext->length);
  length = cleartext->length;

  ASSERT (iv->length == SALSA20_IV_SIZE);

  data = xalloc(length + 1);

  salsa20_set_key(&ctx, key->length, key->data);
  salsa20_set_iv(&ctx, iv->data);
  data[length] = 17;
  crypt(&ctx, length, data, cleartext->data);
  if (data[length] != 17)
    {
      fprintf(stderr, "Encrypt of %lu bytes wrote too much!\nInput:",
	      (unsigned long) length);
      tstring_print_hex(cleartext);
      fprintf(stderr, "\n");
      FAIL();
    }
  if (!MEMEQ(length, data, ciphertext->data))
    {
      fprintf(stderr, "Encrypt failed:\nInput:");
      tstring_print_hex(cleartext);
      fprintf(stderr, "\nOutput: ");
      print_hex(length, data);
      fprintf(stderr, "\nExpected:");
      tstring_print_hex(ciphertext);
      fprintf(stderr, "\n");
      FAIL();
    }
  salsa20_set_key(&ctx, key->length, key->data);
  salsa20_set_iv(&ctx, iv->data);
  crypt(&ctx, length, data, data);

  if (!MEMEQ(length, data, cleartext->data))
    {
      fprintf(stderr, "Decrypt failed:\nInput:");
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

#define test_salsa20(key, iv, cleartext, ciphertext) \
  _test_salsa20 (salsa20_crypt, (key), (iv), (cleartext), (ciphertext))

#define test_salsa20r12(key, iv, cleartext, ciphertext) \
  _test_salsa20 (salsa20r12_crypt, (key), (iv), (cleartext), (ciphertext))

  
void
test_main(void)
{
  /* http://www.ecrypt.eu.org/stream/svn/viewcvs.cgi/ecrypt/trunk/submissions/salsa20/reduced/12-rounds/verified.test-vectors?logsort=rev&rev=210&view=markup */
  test_salsa20r12(SHEX("80000000 00000000 00000000 00000000"),
		  SHEX("00000000 00000000"),
		  SHEX("00000000 00000000"),
		  SHEX("FC207DBF C76C5E17"));

  test_salsa20r12(SHEX("00400000 00000000 00000000 00000000"),
		  SHEX("00000000 00000000"),
		  SHEX("00000000 00000000"),
		  SHEX("6C11A3F9 5FEC7F48"));

  test_salsa20r12(SHEX("09090909090909090909090909090909"),
		  SHEX("0000000000000000"),
		  SHEX("00000000 00000000"),
		  SHEX("78E11FC3 33DEDE88"));

  test_salsa20r12(SHEX("1B1B1B1B1B1B1B1B1B1B1B1B1B1B1B1B"),
		  SHEX("00000000 00000000"),
		  SHEX("00000000 00000000"),
		  SHEX("A6747461 1DF551FF"));

  test_salsa20r12(SHEX("80000000000000000000000000000000"
		       "00000000000000000000000000000000"),
		  SHEX("00000000 00000000"),
		  SHEX("00000000 00000000"),
		  SHEX("AFE411ED 1C4E07E4"));

  test_salsa20r12(SHEX("0053A6F94C9FF24598EB3E91E4378ADD"
		       "3083D6297CCF2275C81B6EC11467BA0D"),
		  SHEX("0D74DB42A91077DE"),
		  SHEX("00000000 00000000"),
		  SHEX("52E20CF8 775AE882"));

  /* http://www.ecrypt.eu.org/stream/svn/viewcvs.cgi/ecrypt/trunk/submissions/salsa20/full/verified.test-vectors?logsort=rev&rev=210&view=markup */

  test_salsa20(SHEX("80000000 00000000 00000000 00000000"),
	       SHEX("00000000 00000000"),
	       SHEX("00000000 00000000"),
	       SHEX("4DFA5E48 1DA23EA0"));

  test_salsa20(SHEX("00000000 00000000 00000000 00000000"),
	       SHEX("80000000 00000000"),
	       SHEX("00000000 00000000"),
	       SHEX("B66C1E44 46DD9557"));

  test_salsa20(SHEX("0053A6F94C9FF24598EB3E91E4378ADD"),
	       SHEX("0D74DB42A91077DE"),
	       SHEX("00000000 00000000"),
	       SHEX("05E1E7BE B697D999"));

  test_salsa20(SHEX("80000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"),
	       SHEX("00000000 00000000"),
	       SHEX("00000000 00000000"),
	       SHEX("E3BE8FDD 8BECA2E3"));

  test_salsa20(SHEX("00000000 00000000 00000000 00000000"
		    "00000000 00000000 00000000 00000000"),
	       SHEX("80000000 00000000"),
	       SHEX("00000000 00000000"),
	       SHEX("2ABA3DC45B494700"));

  test_salsa20(SHEX("0053A6F94C9FF24598EB3E91E4378ADD"
		    "3083D6297CCF2275C81B6EC11467BA0D"),
	       SHEX("0D74DB42A91077DE"),
	       SHEX("00000000 00000000"),
	       SHEX("F5FAD53F 79F9DF58"));

  test_salsa20_stream(SHEX("80000000000000000000000000000000"),
		      SHEX("00000000 00000000"),
		      SHEX("4DFA5E481DA23EA09A31022050859936"
			   "DA52FCEE218005164F267CB65F5CFD7F"
			   "2B4F97E0FF16924A52DF269515110A07"
			   "F9E460BC65EF95DA58F740B7D1DBB0AA"
			   "DA9C1581F429E0A00F7D67E23B730676"
			   "783B262E8EB43A25F55FB90B3E753AEF"
			   "8C6713EC66C51881111593CCB3E8CB8F"
			   "8DE124080501EEEB389C4BCB6977CF95"
			   "7D5789631EB4554400E1E025935DFA7B"
			   "3E9039D61BDC58A8697D36815BF1985C"
			   "EFDF7AE112E5BB81E37ECF0616CE7147"
			   "FC08A93A367E08631F23C03B00A8DA2F"
			   "B375703739DACED4DD4059FD71C3C47F"
			   "C2F9939670FAD4A46066ADCC6A564578"
			   "3308B90FFB72BE04A6B147CBE38CC0C3"
			   "B9267C296A92A7C69873F9F263BE9703"),
		      SHEX("F7A274D268316790A67EC058F45C0F2A"
			   "067A99FCDE6236C0CEF8E056349FE54C"
			   "5F13AC74D2539570FD34FEAB06C57205"
			   "3949B59585742181A5A760223AFA22D4"));

  test_salsa20_stream(SHEX("48494A4B4C4D4E4F5051525354555657"
			   "58595A5B5C5D5E5F6061626364656667"),
		      SHEX("0000000000000000"),
		      SHEX("53AD3698A011F779AD71030F3EFBEBA0"
			   "A7EE3C55789681B1591EF33A7BE521ED"
			   "68FC36E58F53FFD6E1369B00E390E973"
			   "F656ACB097E0D603BE59A0B8F7975B98"
			   "A04698274C6AC6EC03F66ED3F94C08B7"
			   "9FFDBF2A1610E6F5814905E73AD6D0D2"
			   "8164EEB8450D8ED0BB4B644761B43512"
			   "52DD5DDF00C31E3DABA0BC17691CCFDC"
			   "B826C7F071E796D34E3BFFB3C96E76A1"
			   "209388392806947C7F19B86D379FA3AE"
			   "DFCD19EBF49803DACC6E577E5B97B0F6"
			   "D2036B6624D8196C96FCF02C865D30C1"
			   "B505D41E2C207FA1C0A0E93413DDCFFC"
			   "9BECA8030AFFAC2466E56482DA0EF428"
			   "E63880B5021D3051F18679505A2B9D4F"
			   "9B2C5A2D271D276DE3F51DBEBA934436"),
		      SHEX("7849651A820B1CDFE36D5D6632716534"
			   "E0635EDEFD538122D80870B60FB055DB"
			   "637C7CA2B78B116F83AFF46E40F8F71D"
			   "4CD6D2E1B750D5E011D1DF2E80F7210A"));
}

/* Intermediate values for the first salsa20 test case.
 0: 61707865       80        0        0
           0 3120646e        0        0
           0        0 79622d36       80
           0        0        0 6b206574
 1: 50e6ebaf 3093463d f190e454 9032fa35
    b83c32b0 7fdf3d47 eff21454 a6bf53f6
    59562a33 90327718 9bc1ab3d 49c5665e
    4b9c6232 a5b70d82 b1169b3c 8273a766
 2: 877140ed bc61b44d 60af1c4e 8a219997
    dfa36b55 9dc00f65 e245efc8 ece54d32
    72a63aac c0dc93d7 a1cd6536 b3d44ccb
    8ebd332b c4022fa0 5d4ff16b  65f222e
 3:  26693f1 c7ef1593  549a3a3 9396e54a
    c899675e  1f815f3  47c648d   ebbc01
    67f6ac0c d03d4afa 810d422e e7fd3e5b
    8cd07539 3eb6917b 54e58e29 ef2c818d
 4: 1dff67e3 39538859 717137d4  b935012
    f279ff60 26098b57 4cc2cc68 752f0a9c
    f62fef8b a3028de7 74c726e7 42bbaa73
    85d7ae1b 36e9c191 791019b1 82263e6a
 5: 5058d8b0 d3e44dcf 10bb47b1 7b673ef0
    19f30031 111e4716 ec0295bc 6fd5bf67
    12ffc7e4  d8b55c8  170d410 dd715714
    dcd50b85 1f2bfff6 bde9be51 dbcb0b76
 6: bc9cffbc 33ef9daa 8057f2b9 896b4878
     705ae8b d14227c3 64a13629 112fc18c
    bfe180ad eaf359a0 68467f43 a365bb13
     6b1e849 e6cc8032 70e6c3fe cb0a55bb
 7: 8d90ced2 54d545b4 85be446e b1632f4f
    a071ac6a 90e0a919 33e1e736 ca25d574
    a2b9cc17 7211ef22 c6d499c3 83fdd462
    69a1c02c 4ee14ab4  33c7598 6c536d35
 8: 27885144 2d2a552b f3f9bf1b 33ebeb6b
    104b8b7a  a96110e 9acb26ae 9dba5b23
    be384f78 4cdf3afc ef04b59d f0b9a6fe
    ae50a69b  6c6ea81 f11fe33a 5abcb2ae
 9: 3b2388b1 a820e0d9 1f008910 88c73d4e
    fc306490 8188ba2d d0cae010 9a65a2e4
    cf53e73f acec2667 4b870ab0 6cbfe29b
    27295feb  2a801ee 16f1c6fe 4f40ae38
10: 6d52785f 5d421f38 d44f5a20 a7ec3b7c
    c6a5c6cb f2a38eca  c45beae  69415ff
    93bbc87e cad09b7b c4627081 55276967
    3e13c4d8 aa4e20f4 2a485bf2 bcdbfc61
11: 5136a836 dd9db9bc 50366ca5 a65edf75
    75bb5d1e 6bd4e822 cb52477c 7323b939
    881133b8 38079a5c 14e61ea3  632aa57
    ac091b61 fc1c6ca1 7e5fcc1a 329a1938
12: 5e0ca897 175e6c47 7a1e9674  609ad5a
    ac25229b 49de7bae  370e70c f8bde5e7
    21f81ab3 e6130800 9e2a3e8f 70eed5f1
    d0fbb239 d78a8ea6 b644390a 2c582e03
13: 99fc90d3 f3e42871 78784440 a5885714
    28084a8c 27900f47 e453b985 39b7ca44
     81e5dbf 7860f2b0 693f4da0 74c5ce19
     5f2d43d a9563322 7bd6f4da 4d2e97f5
14: 25571a99 3197dbee 50a1c7d7 91f0e753
    2528837d 56153f81 287f5022 270918e4
    42764fa3 fea16e5b 9ec649fe a4e5e669
    8734b3d9 6fc1ae8a b79a8a04 af160a85
15: 34b906a6 3f56196a 6b690cbf 6c08907a
    60cfba2b 819592a7 3c9b803a 1a3ce6d2
     fdfc6af 282cd998 ad20e9ec  a0c76a1
    772ffdcc ebf39c76 15579a67 dada9ba0
16: 3d87b380 5f9d893a c676cd97 e6d2b4b8
    7d6ca34a 1dc97a79  e3de94b bccb03cd
    12e2a81b 23b00e62 74d433a6 acedb4cb
    6e34fe34 f4c034e4 b3349639 f6ac0473
17: e014e81d 916efb68 d833f0c9 2a0e2be9
    a334791f 71573537 d5c5cb06 c8abbb8c
    6abb97da 9031d7dc 9bea8440 90030a9c
    6c2bdf8a  2649fbf 3a3aeef8 ee0d66c8
18: e9fb1dd3 80e4f86d 2bf2e408 d1809e73
    20872c8d d93bc116 9012e00c 1c813d8e
    45aa03ae 7136cba6 a6b85fc3 9e2d048a
    48013f9e 1f2853d3  854c21d b9cdfb3c
19: 3d47db86 8392cea8 2cf87621 2cff7d58
    dea99415 5800a055 e3661354 86701443
     cc9d23f 616a0a0b 836c1eb9 6c1e72e7
    24cba2f0 54be11a6 6dcb2586 a5663106
20: e6ee81e8 a03ea19d 2002319a 36998550
    eefc52da e4e51bb3 b67c264f 7ffd5c5f
    e0974f2b 4a9216ff 1bc4b21c  70a1095
    bc60e4f9 da95ef65 b740f758 3f90765d
*/
