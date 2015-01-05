#ifndef VSF_HPUX_BOGONS_H
#define VSF_HPUX_BOGONS_H

#include <sys/mman.h>
/* HP-UX has MAP_ANONYMOUS but not MAP_ANON - I'm not sure which is more
 * standard!
 */
#ifdef MAP_ANONYMOUS
  #ifndef MAP_ANON
    #define MAP_ANON MAP_ANONYMOUS
  #endif
#endif

/* Ancient versions of HP-UX don't have MAP_FAILED */
#ifndef MAP_FAILED
  #define MAP_FAILED (void *) -1L
#endif

/* Need dirfd() */
#include "dirfd_extras.h"

#endif /* VSF_HPUX_BOGONS_H */

