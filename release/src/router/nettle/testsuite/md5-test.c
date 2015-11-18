#include "testutils.h"
#include "md5.h"

void
test_main(void)
{
  test_hash(&nettle_md5, SDATA(""),
	    SHEX("D41D8CD98F00B204 E9800998ECF8427E"));

  test_hash(&nettle_md5, SDATA("a"),
	    SHEX("0CC175B9C0F1B6A8 31C399E269772661"));
	    
  test_hash(&nettle_md5, SDATA("abc"),
	    SHEX("900150983cd24fb0 D6963F7D28E17F72"));

  test_hash(&nettle_md5, SDATA("message digest"),
	    SHEX("F96B697D7CB7938D 525A2F31AAF161D0"));
  
  test_hash(&nettle_md5, SDATA("abcdefghijklmnopqrstuvwxyz"),
	    SHEX("C3FCD3D76192E400 7DFB496CCA67E13B"));
  
  test_hash(&nettle_md5,
	    SDATA("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		  "abcdefghijklmnopqrstuvwxyz"
		  "0123456789"),
	    SHEX("D174AB98D277D9F5 A5611C2C9F419D9F"));

  test_hash(&nettle_md5,
	    SDATA("1234567890123456789012345678901234567890"
		  "1234567890123456789012345678901234567890"),
	    SHEX("57EDF4A22BE3C955 AC49DA2E2107B67A"));

  /* Additional test vector, from Daniel Kahn Gillmor */
  test_hash(&nettle_md5, SDATA("38"),
	    SHEX("a5771bce93e200c3 6f7cd9dfd0e5deaa"));

  /* Collisions, reported by Xiaoyun Wang1, Dengguo Feng2, Xuejia
     Lai3, Hongbo Yu1, http://eprint.iacr.org/2004/199. */

#define M0 \
  /*                                          vv */				\
  "d131dd02 c5e6eec4 693d9a06 98aff95c 2fcab5 87 12467eab 4004583e b8fb7f89"	\
  "55ad3406 09f4b302 83e48883 25 71 415a 085125e8 f7cdc99f d91dbd f2 80373c5b"	\
  /*                             ^^                               ^^ */

#define M1 \
  /*                                          vv */				\
  "d131dd02 c5e6eec4 693d9a06 98aff95c 2fcab5 07 12467eab 4004583e b8fb7f89"	\
  "55ad3406 09f4b302 83e48883 25 f1 415a 085125e8 f7cdc99f d91dbd 72 80373c5b"	\
  /*                             ^^                               ^^ */

#define N0									\
  /*                                          vv */				\
  "960b1dd1 dc417b9c e4d897f4 5a6555d5 35739a c7 f0ebfd0c 3029f166 d109b18f"	\
  "75277f79 30d55ceb 22e8adba 79 cc 155c ed74cbdd 5fc5d36d b19b0a d8 35cca7e3"	\
  /*                             ^^                               ^^ */

#define N1									\
  /*                                          vv */				\
  "960b1dd1 dc417b9c e4d897f4 5a6555d5 35739a 47 f0ebfd0c 3029f166 d109b18f"	\
  "75277f79 30d55ceb 22e8adba 79 4c 155c ed74cbdd 5fc5d36d b19b0a 58 35cca7e3"	\
  /*                             ^^                               ^^ */

  /* Note: The checksum in the paper, 1f160396 efc71ff4 bcff659f
     bf9d0fa3, is incorrect. */

#define H0 "a4c0d35c 95a63a80 5915367d cfe6b751"

#define N2									\
  /*                                          vv */				\
  "d8823e31 56348f5b ae6dacd4 36c919c6 dd53e2 b4 87da03fd 02396306 d248cda0"	\
  "e99f3342 0f577ee8 ce54b670 80 a8 0d1e c69821bc b6a88393 96f965 2b 6ff72a70"	\
  /*                             ^^                               ^^ */

#define N3									\
  /*                                          vv */				\
  "d8823e31 56348f5b ae6dacd4 36c919c6 dd53e2 34 87da03fd 02396306 d248cda0"	\
  "e99f3342 0f577ee8 ce54b670 80 28 0d1e c69821bc b6a88393 96f965 ab 6ff72a70"	\
  /*                             ^^                               ^^ */

  /* Note: Also different from the checksum in the paper */
  
#define H1 "79054025 255fb1a2 6e4bc422 aef54eb4"
  
  test_hash(&nettle_md5,
	    SHEX(M0 N0), SHEX(H0));

  test_hash(&nettle_md5,
	    SHEX(M1 N1), SHEX(H0));

  test_hash(&nettle_md5,
	    SHEX(M0 N2), SHEX(H1));

  test_hash(&nettle_md5,
	    SHEX(M1 N3), SHEX(H1));
}

/* Intermediate values for the single _nettle_md5_compress call for
   the first test case. Each row gives the values for a, b, c, d after
   the i:th round. The row i = -1 gives the initial values, and i = 99
   gives the output values.

     i         a        b        c        d
    -1: 67452301 efcdab89 98badcfe 10325476
     0: a5202774 efcdab89 98badcfe 10325476
     1: a5202774 efcdab89 98badcfe f59592dd
    15: f56c7cf1 d6819c6a 5aa53f75 374943a7
    16: 1c7d7513 d6819c6a 5aa53f75 374943a7
    17: 1c7d7513 d6819c6a 5aa53f75 7bd57a3a
    31: 13707036 a2205f1f 1c31c384 ae7813db
    32: df63eaa1 a2205f1f 1c31c384 ae7813db
    33: df63eaa1 a2205f1f 1c31c384 c3689f5b
    47: 3f55edfd ca7d2dbd 68d84ea2 22a31f54
    48: 93aa2577 ca7d2dbd 68d84ea2 22a31f54
    49: 93aa2577 ca7d2dbd 68d84ea2 1688dc85
    63: 7246fad3 14e45506 ff4ea3eb 6e10a476
    99: d98c1dd4  4b2008f 980980e9 7e42f8ec
*/
