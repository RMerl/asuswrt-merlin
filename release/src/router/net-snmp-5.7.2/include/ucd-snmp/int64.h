#ifdef UCD_COMPATIBLE

#include <net-snmp/library/int64.h>

#else

#error "Please update your headers or configure using --enable-ucd-snmp-compatibility"

#endif
