#include "testutils.h"
#include "aes.h"
#include "ctr.h"

void
test_main(void)
{
  /* From NIST spec 800-38a on AES modes,
   *
   * http://csrc.nist.gov/CryptoToolkit/modes/800-38_Series_Publications/SP800-38A.pdf
   *
   * F.5  CTR Example Vectors
   */

  /* Zero-length data. Exposes bug reported by Tim Kosse, where
     ctr_crypt increment the ctr when it shouldn't. */
  test_cipher_ctr(&nettle_aes128,
		  SHEX("2b7e151628aed2a6abf7158809cf4f3c"),
		  SHEX(""), SHEX(""),
		  SHEX("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"));
  
  /* F.5.1  CTR-AES128.Encrypt */
  test_cipher_ctr(&nettle_aes128,
		  SHEX("2b7e151628aed2a6abf7158809cf4f3c"),
		  SHEX("6bc1bee22e409f96e93d7e117393172a"
		       "ae2d8a571e03ac9c9eb76fac45af8e51"
		       "30c81c46a35ce411e5fbc1191a0a52ef"
		       "f69f2445df4f9b17ad2b417be66c3710"),
		  SHEX("874d6191b620e3261bef6864990db6ce"
		       "9806f66b7970fdff8617187bb9fffdff"
		       "5ae4df3edbd5d35e5b4f09020db03eab"
		       "1e031dda2fbe03d1792170a0f3009cee"),
		  SHEX("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"));

  /* F.5.3  CTR-AES192.Encrypt */
  test_cipher_ctr(&nettle_aes192,
		  SHEX("8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b"),
		  SHEX("6bc1bee22e409f96e93d7e117393172a"
		       "ae2d8a571e03ac9c9eb76fac45af8e51"
		       "30c81c46a35ce411e5fbc1191a0a52ef"
		       "f69f2445df4f9b17ad2b417be66c3710"),
		  SHEX("1abc932417521ca24f2b0459fe7e6e0b"
		       "090339ec0aa6faefd5ccc2c6f4ce8e94"
		       "1e36b26bd1ebc670d1bd1d665620abf7"
		       "4f78a7f6d29809585a97daec58c6b050"),
		  SHEX("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"));

  /* F.5.5  CTR-AES256.Encrypt */
  test_cipher_ctr(&nettle_aes256,
		  SHEX("603deb1015ca71be2b73aef0857d7781"
		       "1f352c073b6108d72d9810a30914dff4"),
		  SHEX("6bc1bee22e409f96e93d7e117393172a"
		       "ae2d8a571e03ac9c9eb76fac45af8e51"
		       "30c81c46a35ce411e5fbc1191a0a52ef"
		       "f69f2445df4f9b17ad2b417be66c3710"),
		  SHEX("601ec313775789a5b7a7f504bbf3d228"
		       "f443e3ca4d62b59aca84e990cacaf5c5"
		       "2b0930daa23de94ce87017ba2d84988d"
		       "dfc9c58db67aada613c2dd08457941a6"),
		  SHEX("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"));
}

/*
  F.5.1  CTR-AES128.Encrypt
  Key            2b7e151628aed2a6abf7158809cf4f3c
  Init. Counter f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
  Block #1
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
  Output Block   ec8cdf7398607cb0f2d21675ea9ea1e4
  Plaintext      6bc1bee22e409f96e93d7e117393172a
  Ciphertext     874d6191b620e3261bef6864990db6ce
  Block #2
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff00
  Output Block   362b7c3c6773516318a077d7fc5073ae
  Plaintext      ae2d8a571e03ac9c9eb76fac45af8e51
  Ciphertext     9806f66b7970fdff8617187bb9fffdff
  Block #3
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff01
  Output Block   6a2cc3787889374fbeb4c81b17ba6c44
  Plaintext      30c81c46a35ce411e5fbc1191a0a52ef
  Ciphertext     5ae4df3edbd5d35e5b4f09020db03eab
  Block #4
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff02
  Output Block   e89c399ff0f198c6d40a31db156cabfe
  Plaintext      f69f2445df4f9b17ad2b417be66c3710
  Ciphertext     1e031dda2fbe03d1792170a0f3009cee
  
  F.5.2  CTR-AES128.Decrypt
  Key            2b7e151628aed2a6abf7158809cf4f3c
  Init. Counter f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
  Block #1
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
  Output Block   ec8cdf7398607cb0f2d21675ea9ea1e4
  Ciphertext     874d6191b620e3261bef6864990db6ce
  Plaintext      6bc1bee22e409f96e93d7e117393172a
  Block #2
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff00
  Output Block   362b7c3c6773516318a077d7fc5073ae
  Ciphertext     9806f66b7970fdff8617187bb9fffdff
  Plaintext      ae2d8a571e03ac9c9eb76fac45af8e51
  Block #3
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff01
  Output Block   6a2cc3787889374fbeb4c81b17ba6c44
  Ciphertext     5ae4df3edbd5d35e5b4f09020db03eab
  Plaintext      30c81c46a35ce411e5fbc1191a0a52ef
  Block #4
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff02
  Output Block   e89c399ff0f198c6d40a31db156cabfe
  Ciphertext     1e031dda2fbe03d1792170a0f3009cee
  Plaintext      f69f2445df4f9b17ad2b417be66c3710
  
  F.5.3  CTR-AES192.Encrypt
  Key            8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b
  Init. Counter f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
  Block #1
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
  Output Block   717d2dc639128334a6167a488ded7921
  Plaintext      6bc1bee22e409f96e93d7e117393172a
  Ciphertext     1abc932417521ca24f2b0459fe7e6e0b
  Block #2
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff00
  Output Block   a72eb3bb14a556734b7bad6ab16100c5
  Plaintext      ae2d8a571e03ac9c9eb76fac45af8e51
  Ciphertext     090339ec0aa6faefd5ccc2c6f4ce8e94
  Block #3
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff01
  Output Block   2efeae2d72b722613446dc7f4c2af918
  Plaintext      30c81c46a35ce411e5fbc1191a0a52ef
  Ciphertext     1e36b26bd1ebc670d1bd1d665620abf7
  Block #4
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff02
  Output Block   b9e783b30dd7924ff7bc9b97beaa8740
  Plaintext      f69f2445df4f9b17ad2b417be66c3710
  Ciphertext     4f78a7f6d29809585a97daec58c6b050
  
  F.5.4  CTR-AES192.Decrypt
  Key            8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b
  Init. Counter f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
  Block #1
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
  Output Block   717d2dc639128334a6167a488ded7921
  Ciphertext     1abc932417521ca24f2b0459fe7e6e0b
  Plaintext      6bc1bee22e409f96e93d7e117393172a
  Block #2
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff00
  Output Block   a72eb3bb14a556734b7bad6ab16100c5
  Ciphertext     090339ec0aa6faefd5ccc2c6f4ce8e94
  Plaintext      ae2d8a571e03ac9c9eb76fac45af8e51
  Block #3
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff01
  Output Block   2efeae2d72b722613446dc7f4c2af918
  Ciphertext     1e36b26bd1ebc670d1bd1d665620abf7
  Plaintext      30c81c46a35ce411e5fbc1191a0a52ef
  Block #4
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff02
  Output Block   b9e783b30dd7924ff7bc9b97beaa8740
  Ciphertext     4f78a7f6d29809585a97daec58c6b050
  Plaintext      f69f2445df4f9b17ad2b417be66c3710
  
  F.5.5  CTR-AES256.Encrypt
  Key            603deb1015ca71be2b73aef0857d7781
                 1f352c073b6108d72d9810a30914dff4
  Init. Counter f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
  Block #1
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
  Output Block   0bdf7df1591716335e9a8b15c860c502
  Plaintext      6bc1bee22e409f96e93d7e117393172a
  Ciphertext     601ec313775789a5b7a7f504bbf3d228
  Block #2
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff00
  Output Block   5a6e699d536119065433863c8f657b94
  Plaintext      ae2d8a571e03ac9c9eb76fac45af8e51
  Ciphertext     f443e3ca4d62b59aca84e990cacaf5c5
  Block #3
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff01
  Output Block   1bc12c9c01610d5d0d8bd6a3378eca62
  Plaintext      30c81c46a35ce411e5fbc1191a0a52ef
  Ciphertext     2b0930daa23de94ce87017ba2d84988d
  Block #4
  Input Block    f0f1f2f3f4f5f6f7f8f9fafbfcfdff02
  Output Block   2956e1c8693536b1bee99c73a31576b6
  Plaintext      f69f2445df4f9b17ad2b417be66c3710
  Ciphertext     dfc9c58db67aada613c2dd08457941a6
  
  F.5.6  CTR-AES256.Decrypt
  Key            603deb1015ca71be2b73aef0857d7781
                 1f352c073b6108d72d9810a30914dff4
  Init. Counter f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
  Block #1
  Input Block  f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
  Output Block 0bdf7df1591716335e9a8b15c860c502
  Ciphertext   601ec313775789a5b7a7f504bbf3d228
  Plaintext    6bc1bee22e409f96e93d7e117393172a
  Block #2
  Input Block  f0f1f2f3f4f5f6f7f8f9fafbfcfdff00
  Output Block 5a6e699d536119065433863c8f657b94
  Ciphertext   f443e3ca4d62b59aca84e990cacaf5c5
  Plaintext    ae2d8a571e03ac9c9eb76fac45af8e51
  Block #3
  Input Block  f0f1f2f3f4f5f6f7f8f9fafbfcfdff01
  Output Block 1bc12c9c01610d5d0d8bd6a3378eca62
  Ciphertext   2b0930daa23de94ce87017ba2d84988d
  Plaintext    30c81c46a35ce411e5fbc1191a0a52ef
  Block #4
  Input Block  f0f1f2f3f4f5f6f7f8f9fafbfcfdff02
  Output Block 2956e1c8693536b1bee99c73a31576b6
  Ciphertext   dfc9c58db67aada613c2dd08457941a6
  Plaintext    f69f2445df4f9b17ad2b417be66c3710
*/
