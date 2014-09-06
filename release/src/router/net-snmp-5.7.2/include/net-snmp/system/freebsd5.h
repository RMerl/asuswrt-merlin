/* freebsd5 is a superset of freebsd4 */
#include "freebsd4.h"
#define freebsd4 freebsd4

/* don't define _KERNEL on FreeBSD 5.3 even if configure thinks we need it */
#ifdef freebsd5
#undef NETSNMP_IFNET_NEEDS_KERNEL
#endif
