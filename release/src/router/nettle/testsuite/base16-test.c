#include "testutils.h"
#include "base16.h"

void
test_main(void)
{
  ASSERT(BASE16_ENCODE_LENGTH(0) == 0);
  ASSERT(BASE16_ENCODE_LENGTH(1) == 2);
  ASSERT(BASE16_ENCODE_LENGTH(2) == 4);

  ASSERT(BASE16_DECODE_LENGTH(0) == 0);
  ASSERT(BASE16_DECODE_LENGTH(1) == 1);
  ASSERT(BASE16_DECODE_LENGTH(2) == 1);
  ASSERT(BASE16_DECODE_LENGTH(3) == 2);
  ASSERT(BASE16_DECODE_LENGTH(4) == 2);
  
  test_armor(&nettle_base16, 0, "", "");
  test_armor(&nettle_base16, 1, "H", "48");
  test_armor(&nettle_base16, 2, "He", "4865");
  test_armor(&nettle_base16, 3, "Hel", "48656c");
  test_armor(&nettle_base16, 4, "Hell", "48656c6c");
  test_armor(&nettle_base16, 5, "Hello", "48656c6c6f");
  test_armor(&nettle_base16, 6, "Hello", "48656c6c6f00");
}
  
