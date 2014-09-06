#ifdef UCD_COMPATIBLE

#include <net-snmp/library/read_config.h>

#else

#error "Please update your headers or configure using --enable-ucd-snmp-compatibility"

#endif
