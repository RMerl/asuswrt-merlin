#include <net-snmp/system/generic.h>

typedef int     ssize_t;

#undef IP_FORWARDING_SYMBOL
#define IP_FORWARDING_SYMBOL "ip_forwarding"

#undef TCP_TTL_SYMBOL
#define TCP_TTL_SYMBOL "tcp_ttl"

#define UTMP_HAS_NO_TYPE 1
#define UTMP_FILE "/etc/utmp"
