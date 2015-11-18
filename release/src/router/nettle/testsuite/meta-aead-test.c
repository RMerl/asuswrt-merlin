#include "testutils.h"
#include "nettle-internal.h"
#include "nettle-meta.h"

const char* aeads[] = {
  "gcm_aes128",
  "gcm_aes192",
  "gcm_aes256",
  "gcm_camellia128",
  "gcm_camellia256",
  "eax_aes128",
  "chacha_poly1305",
};

void
test_main(void)
{
  int i,j;
  int count = sizeof(aeads)/sizeof(*aeads);
  for (i = 0; i < count; i++) {
    for (j = 0; NULL != nettle_aeads[j]; j++) {
      if (0 == strcmp(aeads[i], nettle_aeads[j]->name))
        break;
    }
    ASSERT(NULL != nettle_aeads[j]); /* make sure we found a matching aead */
  }
  j = 0;
  while (NULL != nettle_aeads[j])
    j++;
  ASSERT(j == count); /* we are not missing testing any aeads */
}
  
