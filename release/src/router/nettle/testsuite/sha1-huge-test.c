#include "testutils.h"

void
test_main(void)
{
  /* Hashes 10 000 000 x 30 000 bytes > 64 * 2^32. This overflows the
     low word of the block counter. This test vector is not cross
     checked with any other sha1 implementation. */
  test_hash_large(&nettle_sha1, 10000000, 30000, 'a',
		  SHEX("0ba79364dc64648f 2074fb4bc5c28bcf"
		       "b7a787b0"));
}
