#include <net-snmp/system/generic.h>
#include <sys/types.h>

typedef int     ssize_t;

#undef TCP_TTL_SYMBOL
#define TCP_TTL_SYMBOL "tcp_ttl"

#undef RTTABLES_SYMBOL
