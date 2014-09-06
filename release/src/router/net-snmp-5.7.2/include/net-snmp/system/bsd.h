#include <net-snmp/system/generic.h>

/*
 * the bsd route symbol adds an 's' at the end to this symbol name 
 */
#undef RTTABLES_SYMBOL
#define RTTABLES_SYMBOL "rt_tables"

/*
 * BSD systems use a different method of looking up sockaddr_in values 
 */
#define NEED_KLGETSA 1

/*
 * ARP_Scan_Next needs a 4th ifIndex argument 
 */
#define ARP_SCAN_FOUR_ARGUMENTS 1

#define UTMP_HAS_NO_TYPE 1
#define UTMP_HAS_NO_PID 1
