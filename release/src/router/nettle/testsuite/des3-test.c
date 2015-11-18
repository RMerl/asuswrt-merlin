#include "testutils.h"
#include "nettle-internal.h"
#include "des.h"

void
test_main(void)
{
  /* Intermediate values:
   *   After first DES encryption:  "cd ea 2a 20 c2 e0 9e 48"
   *   After second DES decryption: "69 52 6e 95 8b ea 49 bd"
   */

  test_cipher(&nettle_des3,
	      SHEX("3e 0b 10 b0 5d 49 c2 54"
		   "6b 46 e0 75 8a 91 61 85"
		   "cb 04 07 d3 20 16 cb a2"),
	      SDATA("Now is t"),
	      SHEX("0a 5d b5 2d 85 74 d1 c9"));
}
