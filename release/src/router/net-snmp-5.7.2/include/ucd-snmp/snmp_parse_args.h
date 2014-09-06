#ifdef UCD_COMPATIBLE

#include <net-snmp/library/snmp_parse_args.h>

#else

#error "Please update your headers or configure using --enable-ucd-snmp-compatibility"

#endif
