#include "testutils.h"
#include "md4.h"

void
test_main(void)
{
  /* Testcases from RFC 1320 */
  test_hash(&nettle_md4, SDATA(""),
	    SHEX("31d6cfe0d16ae931b73c59d7e0c089c0"));
  test_hash(&nettle_md4, SDATA("a"),
	    SHEX("bde52cb31de33e46245e05fbdbd6fb24"));
  test_hash(&nettle_md4, SDATA("abc"),
	    SHEX("a448017aaf21d8525fc10ae87aa6729d"));
  test_hash(&nettle_md4, SDATA("message digest"),
	    SHEX("d9130a8164549fe818874806e1c7014b"));
  test_hash(&nettle_md4, SDATA("abcdefghijklmnopqrstuvwxyz"),
	    SHEX("d79e1c308aa5bbcdeea8ed63df412da9"));
  test_hash(&nettle_md4,
	    SDATA("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
		  "0123456789"),
	    SHEX("043f8582f241db351ce627e153e7f0e4"));
  test_hash(&nettle_md4,
	    SDATA("12345678901234567890123456789012345678901234567890"
		  "123456789012345678901234567890"),
	    SHEX("e33b4ddc9c38f2199c3e7b164fcc0536"));

  /* Additional test vectors, from Daniel Kahn Gillmor */
  test_hash(&nettle_md4, SDATA("38"),
	    SHEX("ae9c7ebfb68ea795483d270f5934b71d"));
  test_hash(&nettle_md4, SDATA("abc"),
	    SHEX("a448017aaf21d8525fc10ae87aa6729d"));
  test_hash(&nettle_md4, SDATA("message digest"),
	    SHEX("d9130a8164549fe818874806e1c7014b"));
  test_hash(&nettle_md4, SDATA("abcdefghijklmnopqrstuvwxyz"),
	    SHEX("d79e1c308aa5bbcdeea8ed63df412da9"));
}
