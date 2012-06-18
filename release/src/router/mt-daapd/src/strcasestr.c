#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>

#if !HAVE_STRCASESTR
/* case-independent string matching, similar to strstr but
 * matching */
char * strcasestr(char* haystack, char* needle) {
  int i;
  int nlength = strlen (needle);
  int hlength = strlen (haystack);

  if (nlength > hlength) return NULL;
  if (hlength <= 0) return NULL;
  if (nlength <= 0) return haystack;
  /* hlength and nlength > 0, nlength <= hlength */
  for (i = 0; i <= (hlength - nlength); i++) {
    if (strncasecmp (haystack + i, needle, nlength) == 0) {
      return haystack + i;
    }
  }
  /* substring not found */
  return NULL;
}

#endif /* !HAVE_STRCASESTR */
