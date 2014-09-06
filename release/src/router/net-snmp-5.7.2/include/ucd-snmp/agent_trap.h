#ifdef UCD_COMPATIBLE

#include <net-snmp/agent/agent_trap.h>

#else

#error "Please update your headers or configure using --enable-ucd-snmp-compatibility"

#endif
