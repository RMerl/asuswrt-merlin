#ifdef UCD_COMPATIBLE

#include <net-snmp/agent/mib_module_config.h>

#else

#error "Please update your headers or configure using --enable-ucd-snmp-compatibility"

#endif
