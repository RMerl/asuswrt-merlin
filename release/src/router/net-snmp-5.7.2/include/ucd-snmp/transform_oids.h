#ifdef UCD_COMPATIBLE

#include <net-snmp/library/transform_oids.h>

#else

#error "Please update your headers or configure using --enable-ucd-snmp-compatibility"

#endif
