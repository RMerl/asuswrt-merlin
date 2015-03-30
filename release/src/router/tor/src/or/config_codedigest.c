
const char *tor_get_digests(void);

/** Return a string describing the digest of the source files in src/or/
 */
const char *
tor_get_digests(void)
{
  return ""
#include "or_sha1.i"
    ;
}

