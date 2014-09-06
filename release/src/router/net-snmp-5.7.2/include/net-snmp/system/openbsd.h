#include "netbsd.h"

#define netbsd1 netbsd1         /* we're really close to this */
#define openbsd openbsd
#define UVM

#undef MBPOOL_SYMBOL
#undef MCLPOOL_SYMBOL
#undef TOTAL_MEMORY_SYMBOL

/* at least OpenBSD/SPARC 3.7 doesn't define this */
#ifndef UINT32_MAX
#define UINT32_MAX	(4294967295U)
#endif

/* define the extra mib modules that are supported */
#define NETSNMP_INCLUDE_HOST_RESOURCES
#define NETSNMP_INCLUDE_IFTABLE_REWRITES
