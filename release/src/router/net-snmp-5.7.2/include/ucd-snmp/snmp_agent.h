#ifdef UCD_COMPATIBLE

#include <net-snmp/agent/snmp_agent.h>

#else

#error "Please update your headers or configure using --enable-ucd-snmp-compatibility"

#endif
