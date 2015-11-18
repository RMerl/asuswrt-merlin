#include "testutils.h"

void
test_main(void)
{
  test_hash(&nettle_sha384, SDATA("abc"),
	    SHEX("cb00753f45a35e8b b5a03d699ac65007"
		 "272c32ab0eded163 1a8b605a43ff5bed"
		 "8086072ba1e7cc23 58baeca134c825a7"));
  
  test_hash(&nettle_sha384,
	    SDATA("abcdefghbcdefghicdefghijdefg"
		  "hijkefghijklfghijklmghijklmn"
		  "hijklmnoijklmnopjklmnopqklmn"
		  "opqrlmnopqrsmnopqrstnopqrstu"),
	    SHEX("09330c33f71147e8 3d192fc782cd1b47"
		 "53111b173b3b05d2 2fa08086e3b0f712"
		 "fcc7c71a557e2db9 66c3e9fa91746039"));

  /* Additional test vectors, from Daniel Kahn Gillmor */
  test_hash(&nettle_sha384, SDATA(""),
	    SHEX("38b060a751ac9638 4cd9327eb1b1e36a"
		 "21fdb71114be0743 4c0cc7bf63f6e1da"
		 "274edebfe76f65fb d51ad2f14898b95b"));
  test_hash(&nettle_sha384, SDATA("a"),
	    SHEX("54a59b9f22b0b808 80d8427e548b7c23"
		 "abd873486e1f035d ce9cd697e8517503"
		 "3caa88e6d57bc35e fae0b5afd3145f31"));
  test_hash(&nettle_sha384, SDATA("38"),
	    SHEX("c071d202ad950b6a 04a5f15c24596a99"
		 "3af8b212467958d5 70a3ffd478006063"
		 "8e3a3d06637691d3 012bd31122071b2c"));
  test_hash(&nettle_sha384, SDATA("message digest"),
	    SHEX("473ed35167ec1f5d 8e550368a3db39be"
		 "54639f828868e945 4c239fc8b52e3c61"
		 "dbd0d8b4de1390c2 56dcbb5d5fd99cd5"));
  test_hash(&nettle_sha384, SDATA("abcdefghijklmnopqrstuvwxyz"),
	    SHEX("feb67349df3db6f5 924815d6c3dc133f"
		 "091809213731fe5c 7b5f4999e463479f"
		 "f2877f5f2936fa63 bb43784b12f3ebb4"));
  test_hash(&nettle_sha384,
	    SDATA("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef"
		  "ghijklmnopqrstuvwxyz0123456789"),
	    SHEX("1761336e3f7cbfe5 1deb137f026f89e0"
		 "1a448e3b1fafa640 39c1464ee8732f11"
		 "a5341a6f41e0c202 294736ed64db1a84"));
  test_hash(&nettle_sha384,
	    SDATA("12345678901234567890123456789012"
		  "34567890123456789012345678901234"
		  "5678901234567890"),
	    SHEX("b12932b0627d1c06 0942f54477641556"
		 "55bd4da0c9afa6dd 9b9ef53129af1b8f"
		 "b0195996d2de9ca0 df9d821ffee67026"));
}
