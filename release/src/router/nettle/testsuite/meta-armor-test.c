#include "testutils.h"
#include "nettle-meta.h"

const char* armors[] = {
  "base16",
  "base64",
  "base64url",
};

void
test_main(void)
{
  int i,j;
  int count = sizeof(armors)/sizeof(*armors);
  for (i = 0; i < count; i++) {
    for (j = 0; NULL != nettle_armors[j]; j++) {
      if (0 == strcmp(armors[i], nettle_armors[j]->name))
        break;
    }
    ASSERT(NULL != nettle_armors[j]); /* make sure we found a matching armor */
  }
  j = 0;
  while (NULL != nettle_armors[j])
    j++;
  ASSERT(j == count); /* we are not missing testing any armors */
}
  
