#ifdef UCD_COMPATIBLE

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/types.h>
#include <net-snmp/library/snmp_impl.h>

#else

#error "Please update your headers or configure using --enable-ucd-snmp-compatibility"

#endif
