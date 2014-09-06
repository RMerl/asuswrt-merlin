#include "freebsd.h"

/*
 * dragonfly is a superset of freebsd4
 */
#define freebsd2 freebsd2
#define freebsd3 freebsd3
#define freebsd4 freebsd4
#define dragonfly dragonfly

#undef IFADDR_SYMBOL
#define IFADDR_SYMBOL "in_ifaddrhead"

#undef PROC_SYMBOL
#define PROC_SYMBOL "allproc"

#undef NPROC_SYMBOL
#define NPROC_SYMBOL "nprocs"

#undef TOTAL_MEMORY_SYMBOL

#undef MBSTAT_SYMBOL

#undef INP_NEXT_SYMBOL
#define INP_NEXT_SYMBOL inp_next
