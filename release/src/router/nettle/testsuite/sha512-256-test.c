#include "testutils.h"

void
test_main(void)
{
  /* From http://csrc.nist.gov/groups/ST/toolkit/documents/Examples/SHA_All.pdf */
  test_hash(&nettle_sha512_256, SDATA("abc"),
	    SHEX("53048E26 81941EF9 9B2E29B7 6B4C7DAB"
		 "E4C2D0C6 34FC6D46 E0E2F131 07E7AF23"));

  test_hash(&nettle_sha512_256, SDATA("abcdefghbcdefghicdefghijdefghijk"
				      "efghijklfghijklmghijklmnhijklmno"
				      "ijklmnopjklmnopqklmnopqrlmnopqrs"
				      "mnopqrstnopqrstu"),
	    SHEX("3928E184 FB8690F8 40DA3988 121D31BE"
		 "65CB9D3E F83EE614 6FEAC861 E19B563A"));
}
