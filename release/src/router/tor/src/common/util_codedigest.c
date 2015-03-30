
#include "util.h"

/** Return a string describing the digest of the source files in src/common/
 */
const char *
libor_get_digests(void)
{
  return ""
#include "common_sha1.i"
    ;
}

