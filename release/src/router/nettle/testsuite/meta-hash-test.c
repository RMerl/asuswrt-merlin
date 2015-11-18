#include "testutils.h"
#include "nettle-internal.h"
#include "nettle-meta.h"

const char* hashes[] = {
  "md2",
  "md4",
  "md5",
  "ripemd160",
  "sha1",
  "sha224",
  "sha256",
  "sha384",
  "sha512"
};

void
test_main(void)
{
  int i,j;
  int count = sizeof(hashes)/sizeof(*hashes);
  for (i = 0; i < count; i++) {
    for (j = 0; NULL != nettle_hashes[j]; j++) {
      if (0 == strcmp(hashes[i], nettle_hashes[j]->name))
        break;
    }
    ASSERT(NULL != nettle_hashes[j]); /* make sure we found a matching hash */
  }
  j = 0;
  while (NULL != nettle_hashes[j])
    j++;
  ASSERT(j == count); /* we are not missing testing any hashes */
  for (j = 0; NULL != nettle_hashes[j]; j++)
    ASSERT(nettle_hashes[j]->digest_size <= NETTLE_MAX_HASH_DIGEST_SIZE);
}
  
