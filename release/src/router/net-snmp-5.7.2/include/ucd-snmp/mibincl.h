#ifdef UCD_COMPATIBLE

#include <net-snmp/library/mibincl.h>

#else

#error "Please update your headers or configure using --enable-ucd-snmp-compatibility"

#endif
