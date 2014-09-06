#include <net-snmp/system/generic.h>

/* got socklen_t? */
#define HAVE_SOCKLEN_T 1

#ifdef HAVE_STDINT_H
#include <stdint.h>	/* uint32_t */
#endif

#undef bsdlike
#undef MBSTAT_SYMBOL
#undef TOTAL_MEMORY_SYMBOL
