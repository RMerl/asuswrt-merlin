/* ccm-test.c

   Self-test and vectors for CCM mode ciphers using AES-128 and AES-256.

   Copyright (C) 2014 Exegin Technologies Limited
   Copyright (C) 2014 Owen Kirby

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

/* The
 * test vectors have been collected from the following standards:
 *  NIST SP800-38C
 *  RFC 3610
 *  IEEE 802.15.4-2011
 *  IEEE P1619.1/D22 July 2007 (draft version)
 */

#include "testutils.h"
#include "aes.h"
#include "ccm.h"
#include "knuth-lfib.h"

static void
test_compare_results(const char *name,
        const struct tstring *adata,
        /* Expected results. */
        const struct tstring *e_clear,
		const struct tstring *e_cipher,
        /* Actual results. */
        const void *clear,
        const void *cipher,
        const void *digest) /* digest optional. */
{
  int tlength = (e_cipher->length - e_clear->length);
  if (digest && tlength && !MEMEQ(tlength, e_cipher->data + e_clear->length, digest))
    {
      fprintf(stderr, "%s digest failed:\nAdata:", name);
      tstring_print_hex(adata);
      fprintf(stderr, "\nInput: ");
      tstring_print_hex(e_clear);
      fprintf(stderr, "\nOutput: ");
      print_hex(tlength, digest);
      fprintf(stderr, "\nExpected:");
      print_hex(tlength, e_cipher->data + e_clear->length);
      fprintf(stderr, "\n");
      FAIL();
    }
  if (!MEMEQ(e_cipher->length, e_cipher->data, cipher))
    {
      fprintf(stderr, "%s: encryption failed\nAdata: ", name);
      tstring_print_hex(adata);
      fprintf(stderr, "\nInput: ");
      tstring_print_hex(e_clear);
      fprintf(stderr, "\nOutput: ");
      print_hex(e_cipher->length, cipher);
      fprintf(stderr, "\nExpected:");
      tstring_print_hex(e_cipher);
      fprintf(stderr, "\n");
      FAIL();
    }
  if (!MEMEQ(e_clear->length, e_clear->data, clear))
    {
      fprintf(stderr, "%s decrypt failed:\nAdata:", name);
      tstring_print_hex(adata);
      fprintf(stderr, "\nInput: ");
      tstring_print_hex(e_cipher);
      fprintf(stderr, "\nOutput: ");
      print_hex(e_clear->length, clear);
      fprintf(stderr, "\nExpected:");
      tstring_print_hex(e_clear);
      fprintf(stderr, "\n");
      FAIL();
    }
} /* test_compare_results */

static void
test_cipher_ccm(const struct nettle_cipher *cipher,
		const struct tstring *key,
		const struct tstring *nonce,
		const struct tstring *authdata,
		int repeat,
		const struct tstring *cleartext,
		const struct tstring *ciphertext)
{
  void *ctx = xalloc(cipher->context_size);
  uint8_t *en_data;
  uint8_t *de_data;
  uint8_t *en_digest;
  uint8_t de_digest[CCM_BLOCK_SIZE];
  size_t tlength;
  struct ccm_ctx ccm;
  int    i;

  ASSERT (key->length == cipher->key_size);
  ASSERT (cleartext->length <= ciphertext->length);
  ASSERT ((cleartext->length + CCM_BLOCK_SIZE) >= ciphertext->length);
  tlength = ciphertext->length - cleartext->length;

  de_data = xalloc(cleartext->length);
  en_data = xalloc(ciphertext->length);
  en_digest = en_data + cleartext->length;
  cipher->set_encrypt_key(ctx, key->data);

  /* Encrypt using the incremental API. */
  ccm_set_nonce(&ccm, ctx, cipher->encrypt, nonce->length, nonce->data,
		authdata->length * repeat, cleartext->length, tlength);
  for (i = 0; i < repeat; i++) {
    ccm_update(&ccm, ctx, cipher->encrypt, authdata->length, authdata->data);
  }
  ccm_encrypt(&ccm, ctx, cipher->encrypt, cleartext->length, en_data, cleartext->data);
  ccm_digest(&ccm, ctx, cipher->encrypt, tlength, en_digest);

  /* Decrypt using the incremental API. */
  ccm_set_nonce(&ccm, ctx, cipher->encrypt, nonce->length, nonce->data,
		authdata->length * repeat, cleartext->length, tlength);
  for (i = 0; i < repeat; i++) {
    ccm_update(&ccm, ctx, cipher->encrypt, authdata->length, authdata->data);
  }
  ccm_decrypt(&ccm, ctx, cipher->encrypt, cleartext->length, de_data, ciphertext->data);
  ccm_digest(&ccm, ctx, cipher->encrypt, tlength, de_digest);

  /* Compare results using the generic API. */
  test_compare_results("CCM", authdata,
		       cleartext, ciphertext, de_data, en_data, de_digest);

  /* Ensure we get the same answers using the all-in-one API. */
  if (repeat <= 1) {
    int ret;
    memset(de_data, 0, cleartext->length);
    memset(en_data, 0, ciphertext->length);
    memset(de_digest, 0, sizeof(de_digest));

    ccm_encrypt_message(ctx, cipher->encrypt, nonce->length, nonce->data,
			authdata->length, authdata->data, tlength,
			ciphertext->length, en_data, cleartext->data);

    ret = ccm_decrypt_message(ctx, cipher->encrypt, nonce->length, nonce->data,
			      authdata->length, authdata->data, tlength,
			      cleartext->length, de_data, ciphertext->data);

    if (ret != 1) fprintf(stderr, "ccm_decrypt_message failed to validate message\n");
    test_compare_results("CCM_MSG", authdata,
			 cleartext, ciphertext, de_data, en_data, NULL);

    /* Ensure that we can detect corrupted message or tag data. */
    if (tlength) {
      en_data[0] ^= 1;
      ret = ccm_decrypt_message(ctx, cipher->encrypt, nonce->length, nonce->data,
				authdata->length, authdata->data, tlength,
				cleartext->length, de_data, en_data);
      if (ret != 0) fprintf(stderr, "ccm_decrypt_message failed to detect corrupted message\n");
    }
    /* Ensure we can detect corrupted adata. */
    if (tlength && authdata->length) {
      ret = ccm_decrypt_message(ctx, cipher->encrypt, nonce->length, nonce->data,
				authdata->length-1, authdata->data, tlength,
				cleartext->length, de_data, ciphertext->data);
      if (ret != 0) fprintf(stderr, "ccm_decrypt_message failed to detect corrupted message\n");
    }
  }

  /* Ensure we get the same answers using the per-cipher API. */
  if (cipher == &nettle_aes128) {
    struct ccm_aes128_ctx aes;
    memset(de_data, 0, cleartext->length);
    memset(en_data, 0, ciphertext->length);
    memset(de_digest, 0, sizeof(de_digest));

    /* AES-128 encrypt. */
    ccm_aes128_set_key(&aes, key->data);
    ccm_aes128_set_nonce(&aes, nonce->length, nonce->data,
			 authdata->length * repeat, cleartext->length, tlength);
    for (i = 0; i < repeat; i++) {
      ccm_aes128_update(&aes, authdata->length, authdata->data);
    }
    ccm_aes128_encrypt(&aes, cleartext->length, en_data, cleartext->data);
    ccm_aes128_digest(&aes, tlength, en_digest);

    /* AES-128 decrypt. */
    ccm_aes128_set_nonce(&aes, nonce->length, nonce->data,
			 authdata->length * repeat, cleartext->length, tlength);
    for (i = 0; i < repeat; i++) {
      ccm_aes128_update(&aes, authdata->length, authdata->data);
    }
    ccm_aes128_decrypt(&aes, cleartext->length, de_data, ciphertext->data);
    ccm_aes128_digest(&aes, tlength, de_digest);

    test_compare_results("CCM_AES_128", authdata,
			 cleartext, ciphertext, de_data, en_data, de_digest);
  }
  /* TODO: I couldn't find any test cases for CCM-AES-192 */
  if (cipher == &nettle_aes256) {
    struct ccm_aes256_ctx aes;
    memset(de_data, 0, cleartext->length);
    memset(en_data, 0, ciphertext->length);
    memset(de_digest, 0, sizeof(de_digest));

    /* AES-256 encrypt. */
    ccm_aes256_set_key(&aes, key->data);
    ccm_aes256_set_nonce(&aes, nonce->length, nonce->data,
			 authdata->length * repeat, cleartext->length, tlength);
    for (i = 0; i < repeat; i++) {
      ccm_aes256_update(&aes, authdata->length, authdata->data);
    }
    ccm_aes256_encrypt(&aes, cleartext->length, en_data, cleartext->data);
    ccm_aes256_digest(&aes, tlength, en_digest);

    /* AES-256 decrypt. */
    ccm_aes256_set_nonce(&aes, nonce->length, nonce->data,
			 authdata->length * repeat, cleartext->length, tlength);
    for (i = 0; i < repeat; i++) {
      ccm_aes256_update(&aes, authdata->length, authdata->data);
    }
    ccm_aes256_decrypt(&aes, cleartext->length, de_data, ciphertext->data);
    ccm_aes256_digest(&aes, tlength, de_digest);

    test_compare_results("CCM_AES_256", authdata,
			 cleartext, ciphertext, de_data, en_data, de_digest);
  }

  free(ctx);
  free(en_data);
  free(de_data);
}

void
test_main(void)
{
  /* Create a pattern of 00010203 04050607 08090a00b 0c0d0e0f ... */
  struct tstring *adata;
  unsigned int i;
  adata = tstring_alloc(256);
  for (i=0; i<adata->length; i++) adata->data[i] = (i & 0xff);

  /* From NIST spec 800-38C on AES modes.
   *
   * Appendix C: Example Vectors
   */
  /*
   * C.1 Example 1
   * Klen = 128, Tlen = 32, Nlen = 56, Alen = 64, Plen = 32
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("404142434445464748494a4b4c4d4e4f"),
		  SHEX("10111213141516"),
		  SHEX("0001020304050607"), 1,
		  SHEX("20212223"),
		  SHEX("7162015b"
		       "4dac255d"));

  /*
   * C.2 Example 2
   * Klen = 128, Tlen = 48, Nlen = 64, Alen = 128, Plen = 128
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("404142434445464748494a4b4c4d4e4f"),
		  SHEX("1011121314151617"),
		  SHEX("000102030405060708090a0b0c0d0e0f"), 1,
		  SHEX("202122232425262728292a2b2c2d2e2f"),
		  SHEX("d2a1f0e051ea5f62081a7792073d593d"
		       "1fc64fbfaccd"));

  /*
   * C.3 Example 3
   * Klen = 128, Tlen = 64, Nlen = 96, Alen = 160, Plen = 192
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("404142434445464748494a4b4c4d4e4f"),
		  SHEX("101112131415161718191a1b"),
		  SHEX("000102030405060708090a0b0c0d0e0f"
		       "10111213"), 1,
		  SHEX("202122232425262728292a2b2c2d2e2f"
		       "3031323334353637"),
		  SHEX("e3b201a9f5b71a7a9b1ceaeccd97e70b"
		       "6176aad9a4428aa5 484392fbc1b09951"));

  /*
   * C.4 Example 4
   * Klen = 128, Tlen = 112, Nlen = 104, Alen = 524288, Plen = 256
   * A = 00010203 04050607 08090a0b 0c0d0e0f
   *     10111213 ...
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("404142434445464748494a4b4c4d4e4f"),
		  SHEX("101112131415161718191a1b1c"),
		  adata, 256,
		  SHEX("202122232425262728292a2b2c2d2e2f"
		       "303132333435363738393a3b3c3d3e3f"),
		  SHEX("69915dad1e84c6376a68c2967e4dab61"
		       "5ae0fd1faec44cc484828529463ccf72"
		       "b4ac6bec93e8598e7f0dadbcea5b"));

  /* From RFC 3610
   *
   * Section 8: Test Vectors
   * Packet Vector #1
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("C0 C1 C2 C3  C4 C5 C6 C7  C8 C9 CA CB  CC CD CE CF"),
		  SHEX("00 00 00 03  02 01 00 A0  A1 A2 A3 A4  A5"),
		  SHEX("00 01 02 03  04 05 06 07"), 1,
		  SHEX("08 09 0A 0B  0C 0D 0E 0F  10 11 12 13  14 15 16 17  18 19 1A 1B  1C 1D 1E"),
		  SHEX("58 8C 97 9A  61 C6 63 D2  F0 66 D0 C2  C0 F9 89 80  6D 5F 6B 61  DA C3 84"
		       "17 E8 D1 2C  FD F9 26 E0"));

  /*
   * Packet Vector #2
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("C0 C1 C2 C3  C4 C5 C6 C7  C8 C9 CA CB  CC CD CE CF"),
		  SHEX("00 00 00 04  03 02 01 A0  A1 A2 A3 A4  A5"),
		  SHEX("00 01 02 03  04 05 06 07"), 1,
		  SHEX("08 09 0A 0B  0C 0D 0E 0F  10 11 12 13  14 15 16 17  18 19 1A 1B  1C 1D 1E 1F"),
		  SHEX("72 C9 1A 36  E1 35 F8 CF  29 1C A8 94  08 5C 87 E3  CC 15 C4 39  C9 E4 3A 3B"
		       "A0 91 D5 6E  10 40 09 16"));

  /*
   * Packet Vector #3
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("C0 C1 C2 C3  C4 C5 C6 C7  C8 C9 CA CB  CC CD CE CF"),
		  SHEX("00 00 00 05  04 03 02 A0  A1 A2 A3 A4  A5"),
		  SHEX("00 01 02 03  04 05 06 07"), 1,
		  SHEX("08 09 0A 0B  0C 0D 0E 0F  10 11 12 13  14 15 16 17  18 19 1A 1B  1C 1D 1E 1F  20"),
		  SHEX("51 B1 E5 F4  4A 19 7D 1D  A4 6B 0F 8E  2D 28 2A E8  71 E8 38 BB  64 DA 85 96  57"
		       "4A DA A7 6F  BD 9F B0 C5"));

  /*
   * Packet Vector #4
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("C0 C1 C2 C3  C4 C5 C6 C7  C8 C9 CA CB  CC CD CE CF"),
		  SHEX("00 00 00 06  05 04 03 A0  A1 A2 A3 A4  A5"),
		  SHEX("00 01 02 03  04 05 06 07  08 09 0A 0B"), 1,
		  SHEX("0C 0D 0E 0F  10 11 12 13  14 15 16 17  18 19 1A 1B  1C 1D 1E"),
		  SHEX("A2 8C 68 65  93 9A 9A 79  FA AA 5C 4C  2A 9D 4A 91  CD AC 8C"
		       "96 C8 61 B9  C9 E6 1E F1"));

  /*
   * Packet Vector #5
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("C0 C1 C2 C3  C4 C5 C6 C7  C8 C9 CA CB  CC CD CE CF"),
		  SHEX("00 00 00 07  06 05 04 A0  A1 A2 A3 A4  A5"),
		  SHEX("00 01 02 03  04 05 06 07  08 09 0A 0B"), 1,
		  SHEX("0C 0D 0E 0F  10 11 12 13  14 15 16 17  18 19 1A 1B  1C 1D 1E 1F"),
		  SHEX("DC F1 FB 7B  5D 9E 23 FB  9D 4E 13 12  53 65 8A D8  6E BD CA 3E"
		       "51 E8 3F 07  7D 9C 2D 93"));

  /*
   * Packet Vector #6
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("C0 C1 C2 C3  C4 C5 C6 C7  C8 C9 CA CB  CC CD CE CF"),
		  SHEX("00 00 00 08  07 06 05 A0  A1 A2 A3 A4  A5"),
		  SHEX("00 01 02 03  04 05 06 07  08 09 0A 0B"), 1,
		  SHEX("0C 0D 0E 0F  10 11 12 13  14 15 16 17  18 19 1A 1B  1C 1D 1E 1F  20"),
		  SHEX("6F C1 B0 11  F0 06 56 8B  51 71 A4 2D  95 3D 46 9B  25 70 A4 BD  87"
		       "40 5A 04 43  AC 91 CB 94"));

  /*
   * Packet Vector #7
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("C0 C1 C2 C3  C4 C5 C6 C7  C8 C9 CA CB  CC CD CE CF"),
		  SHEX("00 00 00 09  08 07 06 A0  A1 A2 A3 A4  A5"),
		  SHEX("00 01 02 03  04 05 06 07"), 1,
		  SHEX("08 09 0A 0B  0C 0D 0E 0F  10 11 12 13  14 15 16 17  18 19 1A 1B  1C 1D 1E"),
		  SHEX("01 35 D1 B2  C9 5F 41 D5  D1 D4 FE C1  85 D1 66 B8  09 4E 99 9D  FE D9 6C"
		       "04 8C 56 60  2C 97 AC BB  74 90"));

  /*
   * Packet Vector #8
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("C0 C1 C2 C3  C4 C5 C6 C7  C8 C9 CA CB  CC CD CE CF"),
		  SHEX("00 00 00 0A  09 08 07 A0  A1 A2 A3 A4  A5"),
		  SHEX("00 01 02 03  04 05 06 07"), 1,
		  SHEX("08 09 0A 0B  0C 0D 0E 0F  10 11 12 13  14 15 16 17  18 19 1A 1B  1C 1D 1E 1F"),
		  SHEX("7B 75 39 9A  C0 83 1D D2  F0 BB D7 58  79 A2 FD 8F  6C AE 6B 6C  D9 B7 DB 24"
		       "C1 7B 44 33  F4 34 96 3F  34 B4"));

  /*
   * Packet Vector #9
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("C0 C1 C2 C3  C4 C5 C6 C7  C8 C9 CA CB  CC CD CE CF"),
		  SHEX("00 00 00 0B  0A 09 08 A0  A1 A2 A3 A4  A5"),
		  SHEX("00 01 02 03  04 05 06 07"), 1,
		  SHEX("08 09 0A 0B  0C 0D 0E 0F  10 11 12 13  14 15 16 17  18 19 1A 1B  1C 1D 1E 1F  20"),
		  SHEX("82 53 1A 60  CC 24 94 5A  4B 82 79 18  1A B5 C8 4D  F2 1C E7 F9  B7 3F 42 E1  97"
		       "EA 9C 07 E5  6B 5E B1 7E  5F 4E"));

  /*
   * Packet Vector #10
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("C0 C1 C2 C3  C4 C5 C6 C7  C8 C9 CA CB  CC CD CE CF"),
		  SHEX("00 00 00 0C  0B 0A 09 A0  A1 A2 A3 A4  A5"),
		  SHEX("00 01 02 03  04 05 06 07  08 09 0A 0B"), 1,
		  SHEX("0C 0D 0E 0F  10 11 12 13  14 15 16 17  18 19 1A 1B  1C 1D 1E"),
		  SHEX("07 34 25 94  15 77 85 15  2B 07 40 98  33 0A BB 14  1B 94 7B"
		       "56 6A A9 40  6B 4D 99 99  88 DD"));

  /*
   * Packet Vector #11
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("C0 C1 C2 C3  C4 C5 C6 C7  C8 C9 CA CB  CC CD CE CF"),
		  SHEX("00 00 00 0D  0C 0B 0A A0  A1 A2 A3 A4  A5"),
		  SHEX("00 01 02 03  04 05 06 07  08 09 0A 0B"), 1,
		  SHEX("0C 0D 0E 0F  10 11 12 13  14 15 16 17  18 19 1A 1B  1C 1D 1E 1F"),
		  SHEX("67 6B B2 03  80 B0 E3 01  E8 AB 79 59  0A 39 6D A7  8B 83 49 34"
		       "F5 3A A2 E9  10 7A 8B 6C  02 2C"));

  /*
   * Packet Vector #12
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("C0 C1 C2 C3  C4 C5 C6 C7  C8 C9 CA CB  CC CD CE CF"),
		  SHEX("00 00 00 0E  0D 0C 0B A0  A1 A2 A3 A4  A5"),
		  SHEX("00 01 02 03  04 05 06 07  08 09 0A 0B"), 1,
		  SHEX("0C 0D 0E 0F  10 11 12 13  14 15 16 17  18 19 1A 1B  1C 1D 1E 1F  20"),
		  SHEX("C0 FF A0 D6  F0 5B DB 67  F2 4D 43 A4  33 8D 2A A4  BE D7 B2 0E  43"
		       "CD 1A A3 16  62 E7 AD 65  D6 DB"));

  /*
   * Packet Vector #13
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("D7 82 8D 13  B2 B0 BD C3  25 A7 62 36  DF 93 CC 6B"),
		  SHEX("00 41 2B 4E  A9 CD BE 3C  96 96 76 6C  FA"),
		  SHEX("0B E1 A8 8B  AC E0 18 B1"), 1,
		  SHEX("08 E8 CF 97  D8 20 EA 25  84 60 E9 6A  D9 CF 52 89  05 4D 89 5C  EA C4 7C"),
		  SHEX("4C B9 7F 86  A2 A4 68 9A  87 79 47 AB  80 91 EF 53  86 A6 FF BD  D0 80 F8"
		       "E7 8C F7 CB  0C DD D7 B3"));

  /*
   * Packet Vector #14
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("D7 82 8D 13  B2 B0 BD C3  25 A7 62 36  DF 93 CC 6B"),
		  SHEX("00 33 56 8E  F7 B2 63 3C  96 96 76 6C  FA"),
		  SHEX("63 01 8F 76  DC 8A 1B CB"), 1,
		  SHEX("90 20 EA 6F  91 BD D8 5A  FA 00 39 BA  4B AF F9 BF  B7 9C 70 28  94 9C D0 EC"),
		  SHEX("4C CB 1E 7C  A9 81 BE FA  A0 72 6C 55  D3 78 06 12  98 C8 5C 92  81 4A BC 33"
		       "C5 2E E8 1D  7D 77 C0 8A"));

  /*
   * Packet Vector #15
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("D7 82 8D 13  B2 B0 BD C3  25 A7 62 36  DF 93 CC 6B"),
		  SHEX("00 10 3F E4  13 36 71 3C  96 96 76 6C  FA"),
		  SHEX("AA 6C FA 36  CA E8 6B 40"), 1,
		  SHEX("B9 16 E0 EA  CC 1C 00 D7  DC EC 68 EC  0B 3B BB 1A  02 DE 8A 2D  1A A3 46 13  2E"),
		  SHEX("B1 D2 3A 22  20 DD C0 AC  90 0D 9A A0  3C 61 FC F4  A5 59 A4 41  77 67 08 97  08"
		       "A7 76 79 6E  DB 72 35 06"));

  /*
   * Packet Vector #16
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("D7 82 8D 13  B2 B0 BD C3  25 A7 62 36  DF 93 CC 6B"),
		  SHEX("00 76 4C 63  B8 05 8E 3C  96 96 76 6C  FA"),
		  SHEX("D0 D0 73 5C  53 1E 1B EC  F0 49 C2 44"), 1,
		  SHEX("12 DA AC 56  30 EF A5 39  6F 77 0C E1  A6 6B 21 F7  B2 10 1C"),
		  SHEX("14 D2 53 C3  96 7B 70 60  9B 7C BB 7C  49 91 60 28  32 45 26"
		       "9A 6F 49 97  5B CA DE AF"));

  /*
   * Packet Vector #17
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("D7 82 8D 13  B2 B0 BD C3  25 A7 62 36  DF 93 CC 6B"),
		  SHEX("00 F8 B6 78  09 4E 3B 3C  96 96 76 6C  FA"),
		  SHEX("77 B6 0F 01  1C 03 E1 52  58 99 BC AE"), 1,
		  SHEX("E8 8B 6A 46  C7 8D 63 E5  2E B8 C5 46  EF B5 DE 6F  75 E9 CC 0D"),
		  SHEX("55 45 FF 1A  08 5E E2 EF  BF 52 B2 E0  4B EE 1E 23  36 C7 3E 3F"
		       "76 2C 0C 77  44 FE 7E 3C"));

  /*
   * Packet Vector #18
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("D7 82 8D 13  B2 B0 BD C3  25 A7 62 36  DF 93 CC 6B"),
		  SHEX("00 D5 60 91  2D 3F 70 3C  96 96 76 6C  FA"),
		  SHEX("CD 90 44 D2  B7 1F DB 81  20 EA 60 C0"), 1,
		  SHEX("64 35 AC BA  FB 11 A8 2E  2F 07 1D 7C  A4 A5 EB D9  3A 80 3B A8  7F"),
		  SHEX("00 97 69 EC  AB DF 48 62  55 94 C5 92  51 E6 03 57  22 67 5E 04  C8"
		       "47 09 9E 5A  E0 70 45 51"));

  /*
   * Packet Vector #19
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("D7 82 8D 13  B2 B0 BD C3  25 A7 62 36  DF 93 CC 6B"),
		  SHEX("00 42 FF F8  F1 95 1C 3C  96 96 76 6C  FA"),
		  SHEX("D8 5B C7 E6  9F 94 4F B8"), 1,
		  SHEX("8A 19 B9 50  BC F7 1A 01  8E 5E 67 01  C9 17 87 65  98 09 D6 7D  BE DD 18"),
		  SHEX("BC 21 8D AA  94 74 27 B6  DB 38 6A 99  AC 1A EF 23  AD E0 B5 29  39 CB 6A"
		       "63 7C F9 BE  C2 40 88 97  C6 BA"));

  /*
   * Packet Vector #20
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("D7 82 8D 13  B2 B0 BD C3  25 A7 62 36  DF 93 CC 6B"),
		  SHEX("00 92 0F 40  E5 6C DC 3C  96 96 76 6C  FA"),
		  SHEX("74 A0 EB C9  06 9F 5B 37"), 1,
		  SHEX("17 61 43 3C  37 C5 A3 5F  C1 F3 9F 40  63 02 EB 90  7C 61 63 BE  38 C9 84 37"),
		  SHEX("58 10 E6 FD  25 87 40 22  E8 03 61 A4  78 E3 E9 CF  48 4A B0 4F  44 7E FF F6"
		       "F0 A4 77 CC  2F C9 BF 54  89 44"));

  /*
   * Packet Vector #21
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("D7 82 8D 13  B2 B0 BD C3  25 A7 62 36  DF 93 CC 6B"),
		  SHEX("00 27 CA 0C  71 20 BC 3C  96 96 76 6C  FA"),
		  SHEX("44 A3 AA 3A  AE 64 75 CA"), 1,
		  SHEX("A4 34 A8 E5  85 00 C6 E4  15 30 53 88  62 D6 86 EA  9E 81 30 1B  5A E4 22 6B  FA"),
		  SHEX("F2 BE ED 7B  C5 09 8E 83  FE B5 B3 16  08 F8 E2 9C  38 81 9A 89  C8 E7 76 F1  54"
		       "4D 41 51 A4  ED 3A 8B 87  B9 CE"));

  /*
   * Packet Vector #22
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("D7 82 8D 13  B2 B0 BD C3  25 A7 62 36  DF 93 CC 6B"),
		  SHEX("00 5B 8C CB  CD 9A F8 3C  96 96 76 6C  FA"),
		  SHEX("EC 46 BB 63  B0 25 20 C3  3C 49 FD 70"), 1,
		  SHEX("B9 6B 49 E2  1D 62 17 41  63 28 75 DB  7F 6C 92 43  D2 D7 C2"),
		  SHEX("31 D7 50 A0  9D A3 ED 7F  DD D4 9A 20  32 AA BF 17  EC 8E BF"
		       "7D 22 C8 08  8C 66 6B E5  C1 97"));

  /*
   * Packet Vector #23
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("D7 82 8D 13  B2 B0 BD C3  25 A7 62 36  DF 93 CC 6B"),
		  SHEX("00 3E BE 94  04 4B 9A 3C  96 96 76 6C  FA"),
		  SHEX("47 A6 5A C7  8B 3D 59 42  27 E8 5E 71"), 1,
		  SHEX("E2 FC FB B8 80 44 2C 73  1B F9 51 67  C8 FF D7 89  5E 33 70 76"),
		  SHEX("E8 82 F1 DB D3 8C E3 ED  A7 C2 3F 04  DD 65 07 1E  B4 13 42 AC"
		       "DF 7E 00 DC  CE C7 AE 52  98 7D"));

  /*
   * Packet Vector #24
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("D7 82 8D 13  B2 B0 BD C3  25 A7 62 36  DF 93 CC 6B"),
		  SHEX("00 8D 49 3B  30 AE 8B 3C  96 96 76 6C  FA"),
		  SHEX("6E 37 A6 EF  54 6D 95 5D  34 AB 60 59"), 1,
		  SHEX("AB F2 1C 0B  02 FE B8 8F  85 6D F4 A3  73 81 BC E3  CC 12 85 17  D4"),
		  SHEX("F3 29 05 B8  8A 64 1B 04  B9 C9 FF B5  8C C3 90 90  0F 3D A1 2A  B1"
		       "6D CE 9E 82  EF A1 6D A6  20 59"));

  /* From IEEE 802.15.4-2011
   *
   * Annex C: Test vectors for cryptographic building blocks
   * C.2.1  MAC beacon frame
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA CB CC CD CE CF"),
		  SHEX("AC DE 48 00 00 00 00 01 00 00 00 05 02"),
		  SHEX("08 D0 84 21 43 01 00 00 00 00 48 DE AC 02 05 00 00 00 55 CF 00 00 51 52 53 54"), 1,
		  SHEX(""),
		  SHEX("22 3B C1 EC 84 1A B5 53"));

  /*
   * C.2.2 MAC data frame
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA CB CC CD CE CF"),
		  SHEX("AC DE 48 00 00 00 00 01 00 00 00 05 04"),
		  SHEX("69 DC 84 21 43 02 00 00 00 00 48 DE AC 01 00 00 00 00 48 DE AC 04 05 00 00 00"), 1,
		  SHEX("61 62 63 64"),
		  SHEX("D4 3E 02 2B"));

  /*
   * C.2.3 MAC command frame
   */
  test_cipher_ccm(&nettle_aes128,
		  SHEX("C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA CB CC CD CE CF"),
		  SHEX("AC DE 48 00 00 00 00 01 00 00 00 05 06"),
		  SHEX("2B DC 84 21 43 02 00 0000 00 48 DE AC FF FF 01 00 00 00 00 48 DE AC 06 05 00 00 00 01"), 1,
		  SHEX("CE"),
		  SHEX("D8 4F DE 52 90 61 F9 C6 F1"));

  /* From IEEE P1619.1/D22 July 2007 (draft version)
   *
   * Annex D: Test Vectors
   * D.2.1 CCM-128-AES-256 test vector 1
   */
  test_cipher_ccm(&nettle_aes256,
		  SHEX("0000000000000000000000000000000000000000000000000000000000000000"),
		  SHEX("000000000000000000000000"),
		  SHEX(""), 0,
		  SHEX("00000000000000000000000000000000"),
		  SHEX("c1944044c8e7aa95d2de9513c7f3dd8c"
		       "4b0a3e5e51f151eb0ffae7c43d010fdb"));

  /*
   * D.2.2 CCM-128-AES-256 test vector 2
   */
  test_cipher_ccm(&nettle_aes256,
		  SHEX("0000000000000000000000000000000000000000000000000000000000000000"),
		  SHEX("000000000000000000000000"),
		  SHEX("00000000000000000000000000000000"), 1,
		  SHEX(""),
		  SHEX("904704e89fb216443cb9d584911fc3c2"));

  /*
   * D.2.3 CCM-128-AES-256 test vector 3
   */
  test_cipher_ccm(&nettle_aes256,
		  SHEX("0000000000000000000000000000000000000000000000000000000000000000"),
		  SHEX("000000000000000000000000"),
		  SHEX("00000000000000000000000000000000"), 1,
		  SHEX("00000000000000000000000000000000"),
		  SHEX("c1944044c8e7aa95d2de9513c7f3dd8c"
		       "87314e9c1fa01abe6a6415943dc38521"));

  /*
   * D.2.4 CCM-128-AES-256 test vector 4
   */
  test_cipher_ccm(&nettle_aes256,
		  SHEX("fb7615b23d80891dd470980bc79584c8b2fb64ce60978f4d17fce45a49e830b7"),
		  SHEX("dbd1a3636024b7b402da7d6f"),
		  SHEX(""), 0,
		  SHEX("a845348ec8c5b5f126f50e76fefd1b1e"),
		  SHEX("cc881261c6a7fa72b96a1739176b277f"
		       "3472e1145f2c0cbe146349062cf0e423"));

  /*
   * D.2.5 CCM-128-AES-256 test vector 5
   */
  test_cipher_ccm(&nettle_aes256,
		  SHEX("404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f"),
		  SHEX("101112131415161718191a1b"),
		  SHEX("000102030405060708090a0b0c0d0e0f10111213"), 1,
		  SHEX("202122232425262728292a2b2c2d2e2f3031323334353637"),
		  SHEX("04f883aeb3bd0730eaf50bb6de4fa2212034e4e41b0e75e5"
		       "9bba3f3a107f3239bd63902923f80371"));

  /*
   * D.2.6 CCM-128-AES-256 test vector 6
   */
  test_cipher_ccm(&nettle_aes256,
		  SHEX("404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f"),
		  SHEX("101112131415161718191a1b"),
		  adata, 256,
		  SHEX("202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"),
		  SHEX("04f883aeb3bd0730eaf50bb6de4fa2212034e4e41b0e75e577f6bf2422c0f6d2"
		       "3376d2cf256ef613c56454cbb5265834"));

  /*
   * D.2.7 CCM-128-AES-256 test vector 7
   */
  test_cipher_ccm(&nettle_aes256,
		  SHEX("404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f"),
		  SHEX("101112131415161718191a1b"),
		  SHEX("202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"), 1,
		  SHEX("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
		       "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
		       "404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f"
		       "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f"
		       "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f"
		       "a0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
		       "c0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
		       "e0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"),
		  SHEX("24d8a38e939d2710cad52b96fe6f82010014c4c43b2e55c557d69f0402e0d6f2"
		       "06c53d6cbd3f1c3c6de5dcdcad9fb74f25741dea741149fe4278a0cc24741e86"
		       "58cc0523b8d7838c60fb1de4b7c3941f5b26dea9322aa29656ec37ac18a9b108"
		       "a6f38b7917f5a9c398838b22afbd17252e96694a9e6237964a0eae21c0a6e152"
		       "15a0e82022926be97268249599e456e05029c3ebc07d78fc5b4a0862e04e68c2"
		       "9514c7bdafc4b52e04833bf30622e4eb42504a44a9dcbc774752de7bb82891ad"
		       "1eba9dc3281422a8aba8654268d3d9c81705f4c5a531ef856df5609a159af738"
		       "eb753423ed2001b8f20c23725f2bef18c409f7e52132341f27cb8f0e79894dd9"
		       "ebb1fa9d28ccfe21bdfea7e6d91e0bab"));

  /*
   * D.2.8 CCM-128-AES-256 test vector 8
   */
  test_cipher_ccm(&nettle_aes256,
		  SHEX("fb7615b23d80891dd470980bc79584c8b2fb64ce6097878d17fce45a49e830b7"),
		  SHEX("dbd1a3636024b7b402da7d6f"),
		  SHEX("36"), 1,
		  SHEX("a9"),
		  SHEX("9d3261b1cf931431e99a32806738ecbd2a"));

  /*
   * D.2.9 CCM-128-AES-256 test vector 9
   */
  test_cipher_ccm(&nettle_aes256,
		  SHEX("f8d476cfd646ea6c2384cb1c27d6195dfef1a9f37b9c8d21a79c21f8cb90d289"),
		  SHEX("dbd1a3636024b7b402da7d6f"),
		  SHEX("7bd859a247961a21823b380e9fe8b65082ba61d3"), 1,
		  SHEX("90ae61cf7baebd4cade494c54a29ae70269aec71"),
		  SHEX("6c05313e45dc8ec10bea6c670bd94f31569386a6"
		       "8f3829e8e76ee23c04f566189e63c686"));
}
