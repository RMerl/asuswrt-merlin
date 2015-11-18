#include "testutils.h"
#include "aes.h"
#include "cbc.h"
#include "knuth-lfib.h"

/* Test with more data and inplace decryption, to check that the
 * cbc_decrypt buffering works. */
#define CBC_BULK_DATA 0x2710 /* 10000 */

static void
test_cbc_bulk(void)
{
  struct knuth_lfib_ctx random;
  
  uint8_t clear[CBC_BULK_DATA];
  
  uint8_t cipher[CBC_BULK_DATA + 1];

  const uint8_t *key = H("966c7bf00bebe6dc 8abd37912384958a"
			 "743008105a08657d dcaad4128eee38b3");
  
  const uint8_t *start_iv = H("11adbff119749103 207619cfa0e8d13a");
  const uint8_t *end_iv = H("c7a42a569b421224 d0c23e52f46f97f5");
  
  struct CBC_CTX(struct aes_ctx, AES_BLOCK_SIZE) aes;
  
  knuth_lfib_init(&random, CBC_BULK_DATA);
  knuth_lfib_random(&random, CBC_BULK_DATA, clear);

  /* Byte that should not be overwritten */
  cipher[CBC_BULK_DATA] = 17;
  
  aes_set_encrypt_key(&aes.ctx, 32, key);
  CBC_SET_IV(&aes, start_iv);

  CBC_ENCRYPT(&aes, aes_encrypt, CBC_BULK_DATA, cipher, clear);

  ASSERT(cipher[CBC_BULK_DATA] == 17);

  if (verbose)
    {
      printf("IV after bulk encryption: ");
      print_hex(AES_BLOCK_SIZE, aes.iv);
      printf("\n");
    }

  ASSERT(MEMEQ(AES_BLOCK_SIZE, aes.iv, end_iv));
  
  /* Decrypt, in place */
  aes_set_decrypt_key(&aes.ctx, 32, key);
  CBC_SET_IV(&aes, start_iv);
  CBC_DECRYPT(&aes, aes_decrypt, CBC_BULK_DATA, cipher, cipher);

  ASSERT(cipher[CBC_BULK_DATA] == 17);

  if (verbose)
    {
      printf("IV after bulk decryption: ");
      print_hex(AES_BLOCK_SIZE, aes.iv);
      printf("\n");
    }

  ASSERT (MEMEQ(AES_BLOCK_SIZE, aes.iv, end_iv));
  ASSERT (MEMEQ(CBC_BULK_DATA, clear, cipher));
}

void
test_main(void)
{
  /* Intermediate values:
   *   iv XOR first message block:
   *       "a5 ce 55 d4 21 15 a1 c6 4a a4 0c b2 ca a6 d1 37"
   *   First ciphertext block, c1:
   *       "1f 94 fc 85 f2 36 21 06 4a ea e3 c9 cc 38 01 0e"
   *   c1 XOR second message block:
   *       "3f e0 94 ec 81 16 4e 68 26 93 c3 a6 a2 5b 64 2f"
   *   Second ciphertext block, c1:
   *       "7b f6 5f c5 02 59 2e 71 af bf 34 87 c0 36 2a 16"
   */

  test_cipher_cbc(&nettle_aes256,
		  SHEX("8d ae 93 ff fc 78 c9 44"
		       "2a bd 0c 1e 68 bc a6 c7"
		       "05 c7 84 e3 5a a9 11 8b"
		       "d3 16 aa 54 9b 44 08 9e"),
		  SDATA("Listen, I'll say this only once!"),
		  SHEX("1f 94 fc 85 f2 36 21 06"
		       "4a ea e3 c9 cc 38 01 0e"
		       "7b f6 5f c5 02 59 2e 71"
		       "af bf 34 87 c0 36 2a 16"),
		  SHEX("e9 a7 26 a0 44 7b 8d e6  03 83 60 de ea d5 b0 4e"));

  /* From NIST spec 800-38a on AES modes.
   *
   * F.2  CBC Example Vectors 
   * F.2.1 CBC-AES128.Encrypt
   */

  /* Intermediate values, blocks input to AES:
   *
   *   6bc0bce12a459991e134741a7f9e1925 
   *   d86421fb9f1a1eda505ee1375746972c 
   *   604ed7ddf32efdff7020d0238b7c2a5d 
   *   8521f2fd3c8eef2cdc3da7e5c44ea206 
   */
  test_cipher_cbc(&nettle_aes128,
		  SHEX("2b7e151628aed2a6abf7158809cf4f3c"),
		  SHEX("6bc1bee22e409f96e93d7e117393172a"
		       "ae2d8a571e03ac9c9eb76fac45af8e51"
		       "30c81c46a35ce411e5fbc1191a0a52ef"
		       "f69f2445df4f9b17ad2b417be66c3710"),
		  SHEX("7649abac8119b246cee98e9b12e9197d"
		       "5086cb9b507219ee95db113a917678b2"
		       "73bed6b8e3c1743b7116e69e22229516"
		       "3ff1caa1681fac09120eca307586e1a7"),
		  SHEX("000102030405060708090a0b0c0d0e0f"));
  
  /* F.2.3 CBC-AES192.Encrypt */
  
  /* Intermediate values, blcoks input to AES:
   *
   *   6bc0bce12a459991e134741a7f9e1925 
   *   e12f97e55dbfcfa1efcf7796da0fffb9
   *   8411b1ef0e2109e5001cf96f256346b5 
   *   a1840065cdb4e1f7d282fbd7db9d35f0
   */

  test_cipher_cbc(&nettle_aes192,
		  SHEX("8e73b0f7da0e6452c810f32b809079e5"
		       "62f8ead2522c6b7b"),
		  SHEX("6bc1bee22e409f96e93d7e117393172a"
		       "ae2d8a571e03ac9c9eb76fac45af8e51"
		       "30c81c46a35ce411e5fbc1191a0a52ef"
		       "f69f2445df4f9b17ad2b417be66c3710"),
		  SHEX("4f021db243bc633d7178183a9fa071e8"
		       "b4d9ada9ad7dedf4e5e738763f69145a"
		       "571b242012fb7ae07fa9baac3df102e0"
		       "08b0e27988598881d920a9e64f5615cd"),
		  SHEX("000102030405060708090a0b0c0d0e0f"));
   
  /* F.2.5 CBC-AES256.Encrypt */

  /* Intermediate values, blcoks input to AES:
   *
   *   6bc0bce12a459991e134741a7f9e1925 
   *   5ba1c653c8e65d26e929c4571ad47587 
   *   ac3452d0dd87649c8264b662dc7a7e92
   *   cf6d172c769621d8081ba318e24f2371 
   */

  test_cipher_cbc(&nettle_aes256,
		  SHEX("603deb1015ca71be2b73aef0857d7781"
		       "1f352c073b6108d72d9810a30914dff4"),
		  SHEX("6bc1bee22e409f96e93d7e117393172a"
		       "ae2d8a571e03ac9c9eb76fac45af8e51"
		       "30c81c46a35ce411e5fbc1191a0a52ef"
		       "f69f2445df4f9b17ad2b417be66c3710"),
		  SHEX("f58c4c04d6e5f1ba779eabfb5f7bfbd6"
		       "9cfc4e967edb808d679f777bc6702c7d"
		       "39f23369a9d9bacfa530e26304231461"
		       "b2eb05e2c39be9fcda6c19078c6a9d1b"),
		  SHEX("000102030405060708090a0b0c0d0e0f"));

  test_cbc_bulk();
}

/*
IV 
  000102030405060708090a0b0c0d0e0f 
Block #1 
Plaintext      6bc1bee22e409f96e93d7e117393172a 
Input Block     6bc0bce12a459991e134741a7f9e1925 
Output Block  7649abac8119b246cee98e9b12e9197d 
Ciphertext 7649abac8119b246cee98e9b12e9197d 
Block #2 
Plaintext      ae2d8a571e03ac9c9eb76fac45af8e51 
Input Block     d86421fb9f1a1eda505ee1375746972c 
Output Block  5086cb9b507219ee95db113a917678b2 
Ciphertext 5086cb9b507219ee95db113a917678b2 
Block #3 
Plaintext      30c81c46a35ce411e5fbc1191a0a52ef 
Input Block     604ed7ddf32efdff7020d0238b7c2a5d 
Output Block  73bed6b8e3c1743b7116e69e22229516 
Ciphertext 73bed6b8e3c1743b7116e69e22229516 
Block #4 
Plaintext      f69f2445df4f9b17ad2b417be66c3710 
Input Block     8521f2fd3c8eef2cdc3da7e5c44ea206 
Output Block  3ff1caa1681fac09120eca307586e1a7 
Ciphertext 3ff1caa1681fac09120eca307586e1a7 
 F.2.2 CBC-AES128.Decrypt 
Key 
  2b7e151628aed2a6abf7158809cf4f3c 
IV 
  000102030405060708090a0b0c0d0e0f 
Block #1 
Ciphertext 7649abac8119b246cee98e9b12e9197d 
Input Block     7649abac8119b246cee98e9b12e9197d 
Output Block  6bc0bce12a459991e134741a7f9e1925 
Plaintext      6bc1bee22e409f96e93d7e117393172a 
Block #2 
Ciphertext 5086cb9b507219ee95db113a917678b2 
Input Block     5086cb9b507219ee95db113a917678b2 
Output Block  d86421fb9f1a1eda505ee1375746972c 
Plaintext      ae2d8a571e03ac9c9eb76fac45af8e51 
Block #3 
Ciphertext 73bed6b8e3c1743b7116e69e22229516 
Input Block     73bed6b8e3c1743b7116e69e22229516 
Output Block  604ed7ddf32efdff7020d0238b7c2a5d 
Plaintext      30c81c46a35ce411e5fbc1191a0a52ef 
Block #4 
Ciphertext 3ff1caa1681fac09120eca307586e1a7 
Input Block     3ff1caa1681fac09120eca307586e1a7 


Output Block  8521f2fd3c8eef2cdc3da7e5c44ea206 
Plaintext      f69f2445df4f9b17ad2b417be66c3710 
 F.2.3 CBC-AES192.Encrypt 
Key 
  8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b
IV 
  000102030405060708090a0b0c0d0e0f 
Block #1 
Plaintext      6bc1bee22e409f96e93d7e117393172a 
Input Block     6bc0bce12a459991e134741a7f9e1925 
Output Block  4f021db243bc633d7178183a9fa071e8 
Ciphertext 4f021db243bc633d7178183a9fa071e8 
Block #2 
Plaintext      ae2d8a571e03ac9c9eb76fac45af8e51 
Input Block     e12f97e55dbfcfa1efcf7796da0fffb9 
Output Block  b4d9ada9ad7dedf4e5e738763f69145a 
Ciphertext b4d9ada9ad7dedf4e5e738763f69145a 
Block #3 
Plaintext      30c81c46a35ce411e5fbc1191a0a52ef 
Input Block     8411b1ef0e2109e5001cf96f256346b5 
Output Block  571b242012fb7ae07fa9baac3df102e0 
Ciphertext 571b242012fb7ae07fa9baac3df102e0 
Block #4 
Plaintext      f69f2445df4f9b17ad2b417be66c3710 
Input Block     a1840065cdb4e1f7d282fbd7db9d35f0 
Output Block  08b0e27988598881d920a9e64f5615cd 
Ciphertext 08b0e27988598881d920a9e64f5615cd 
 F.2.4 CBC-AES192.Decrypt 
Key 
  8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b
IV 
  000102030405060708090a0b0c0d0e0f 
Block #1 
Ciphertext 4f021db243bc633d7178183a9fa071e8 
Input Block     4f021db243bc633d7178183a9fa071e8 
Output Block  6bc0bce12a459991e134741a7f9e1925 
Plaintext      6bc1bee22e409f96e93d7e117393172a 
Block #2 
Ciphertext b4d9ada9ad7dedf4e5e738763f69145a 
Input Block     b4d9ada9ad7dedf4e5e738763f69145a 
Output Block  e12f97e55dbfcfa1efcf7796da0fffb9 
Plaintext      ae2d8a571e03ac9c9eb76fac45af8e51 
Block #3 
Ciphertext 571b242012fb7ae07fa9baac3df102e0 
Input Block     571b242012fb7ae07fa9baac3df102e0 
Output Block  8411b1ef0e2109e5001cf96f256346b5 
Plaintext      30c81c46a35ce411e5fbc1191a0a52ef 
Block #4 
Ciphertext 08b0e27988598881d920a9e64f5615cd 
Input Block     08b0e27988598881d920a9e64f5615cd 
Output Block  a1840065cdb4e1f7d282fbd7db9d35f0 
Plaintext      f69f2445df4f9b17ad2b417be66c3710 
 F.2.5 CBC-AES256.Encrypt 
Key 
  603deb1015ca71be2b73aef0857d7781 
1f352c073b6108d72d9810a30914dff4 
  IV 
  000102030405060708090a0b0c0d0e0f 
Block #1 
Plaintext      6bc1bee22e409f96e93d7e117393172a 
Input Block     6bc0bce12a459991e134741a7f9e1925 
Output Block  f58c4c04d6e5f1ba779eabfb5f7bfbd6 
Ciphertext f58c4c04d6e5f1ba779eabfb5f7bfbd6 
Block #2 
Plaintext      ae2d8a571e03ac9c9eb76fac45af8e51 
Input Block     5ba1c653c8e65d26e929c4571ad47587 
Output Block  9cfc4e967edb808d679f777bc6702c7d 
Ciphertext 9cfc4e967edb808d679f777bc6702c7d
Block #3 
Plaintext      30c81c46a35ce411e5fbc1191a0a52ef 
Input Block     ac3452d0dd87649c8264b662dc7a7e92 
Output Block  39f23369a9d9bacfa530e26304231461 
Ciphertext 39f23369a9d9bacfa530e26304231461 
Block #4 
Plaintext      f69f2445df4f9b17ad2b417be66c3710 
Input Block     cf6d172c769621d8081ba318e24f2371 
Output Block  b2eb05e2c39be9fcda6c19078c6a9d1b 
Ciphertext b2eb05e2c39be9fcda6c19078c6a9d1b
 F.2.6 CBC-AES256.Decrypt 
Key 
  603deb1015ca71be2b73aef0857d7781 
   1f352c073b6108d72d9810a30914dff4 
IV 
  000102030405060708090a0b0c0d0e0f 
Block #1 
Ciphertext f58c4c04d6e5f1ba779eabfb5f7bfbd6 
Input Block     f58c4c04d6e5f1ba779eabfb5f7bfbd6 
Output Block  6bc0bce12a459991e134741a7f9e1925 
Plaintext      6bc1bee22e409f96e93d7e117393172a 
Block #2 
Ciphertext 9cfc4e967edb808d679f777bc6702c7d 
Input Block     9cfc4e967edb808d679f777bc6702c7d 
Output Block  5ba1c653c8e65d26e929c4571ad47587 
Plaintext      ae2d8a571e03ac9c9eb76fac45af8e51 
Block #3 
Ciphertext 39f23369a9d9bacfa530e26304231461 
Input Block     39f23369a9d9bacfa530e26304231461 
Output Block  ac3452d0dd87649c8264b662dc7a7e92 
Plaintext      30c81c46a35ce411e5fbc1191a0a52ef 
Block #4 
Ciphertext b2eb05e2c39be9fcda6c19078c6a9d1b 
Input Block     b2eb05e2c39be9fcda6c19078c6a9d1b 
Output Block  cf6d172c769621d8081ba318e24f2371 
Plaintext      f69f2445df4f9b17ad2b417be66c3710 
*/
