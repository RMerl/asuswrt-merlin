#include "testutils.h"

void
test_main(void)
{
  /* From http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/SHA_All.pdf */
  test_hash(&nettle_sha512_224, SDATA("abc"),
	    SHEX("4634270F 707B6A54 DAAE7530 460842E2"
		 "0E37ED26 5CEEE9A4 3E8924AA"));

  test_hash(&nettle_sha512_224, SDATA("abcdefghbcdefghicdefghijdefghijk"
				      "efghijklfghijklmghijklmnhijklmno"
				      "ijklmnopjklmnopqklmnopqrlmnopqrs"
				      "mnopqrstnopqrstu"),
	    SHEX("23FEC5BB 94D60B23 30819264 0B0C4533"
		 "35D66473 4FE40E72 68674AF9"));
}
