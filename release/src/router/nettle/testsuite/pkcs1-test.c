#include "testutils.h"

#include "pkcs1.h"

void
test_main(void)
{
  uint8_t buffer[16];
  uint8_t expected[16] = { 0,    1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
			   0xff, 0xff, 0xff, 0xff, 0,    'a',  'b',  'c' };

  _pkcs1_signature_prefix(sizeof(buffer), buffer,
			  3, "abc", 0);

  ASSERT(MEMEQ(sizeof(buffer), buffer, expected));
}
