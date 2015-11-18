#include "testutils.h"

void
test_main(void)
{
  /* From FIPS180-2 addendum
     (http://csrc.nist.gov/publications/fips/fips180-2/fips180-2withchangenotice.pdf) */
  test_hash(&nettle_sha224, SDATA("abc"),
	    SHEX("23097d22 3405d822 8642a477 bda255b3"
		 "2aadbce4 bda0b3f7 e36c9da7"));

  test_hash(&nettle_sha224,
	    SDATA("abcdbcdecdefdefgefghfghighij"
		  "hijkijkljklmklmnlmnomnopnopq"),
	    SHEX("75388b16 512776cc 5dba5da1 fd890150"
		 "b0c6455c b4f58b19 52522525"));

  /* Additional test vectors, from Daniel Kahn Gillmor */
  test_hash(&nettle_sha224, SDATA(""),
	    SHEX("d14a028c2a3a2bc9 476102bb288234c4"
		 "15a2b01f828ea62a c5b3e42f"));
  test_hash(&nettle_sha224, SDATA("a"),
	    SHEX("abd37534c7d9a2ef b9465de931cd7055"
		 "ffdb8879563ae980 78d6d6d5"));
  test_hash(&nettle_sha224, SDATA("38"),
	    SHEX("4cfca6da32da6471 98225460722b7ea1"
		 "284f98c4b179e8db ae3f93d5"));
  test_hash(&nettle_sha224, SDATA("message digest"),
	    SHEX("2cb21c83ae2f004d e7e81c3c7019cbcb"
		 "65b71ab656b22d6d 0c39b8eb"));
  test_hash(&nettle_sha224, SDATA("abcdefghijklmnopqrstuvwxyz"),
	    SHEX("45a5f72c39c5cff2 522eb3429799e49e"
		 "5f44b356ef926bcf 390dccc2"));
  test_hash(&nettle_sha224,
	    SDATA("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef"
		  "ghijklmnopqrstuvwxyz0123456789"),
	    SHEX("bff72b4fcb7d75e5 632900ac5f90d219"
		 "e05e97a7bde72e74 0db393d9"));
  test_hash(&nettle_sha224,
	    SDATA("12345678901234567890123456789012"
		  "34567890123456789012345678901234"
		  "5678901234567890"),
	    SHEX("b50aecbe4e9bb0b5 7bc5f3ae760a8e01"
		 "db24f203fb3cdcd1 3148046e"));
}
