#include "freebsd.h"

#define freebsd2 freebsd2       /* freebsd3 is a superset of freebsd2 */

#undef IFADDR_SYMBOL
#define IFADDR_SYMBOL "in_ifaddrhead"

#undef PROC_SYMBOL
#undef NPROC_SYMBOL

#undef TOTAL_MEMORY_SYMBOL
