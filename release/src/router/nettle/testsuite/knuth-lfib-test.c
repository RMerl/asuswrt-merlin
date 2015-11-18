#include "testutils.h"
#include "knuth-lfib.h"

void
test_main(void)
{
  struct knuth_lfib_ctx ctx;

  uint32_t a[2009];
  uint32_t x;
  unsigned m;
  
  knuth_lfib_init(&ctx, 310952);
  for (m = 0; m<2009; m++)
    knuth_lfib_get_array(&ctx, 1009, a);

  x = knuth_lfib_get(&ctx);
  ASSERT (x == 461390032);
}
