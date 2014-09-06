/*
 * irix.h
 * 
 * Date Created:   Mon Feb 16 22:19:39 1998
 * Initial Author: Simon Leinen  <simon@switch.ch>
 */

#include <net-snmp/system/generic.h>

#undef TCP_TTL_SYMBOL
#define TCP_TTL_SYMBOL "tcp_ttl"

#undef IPSTAT_SYMBOL
#define NO_DOUBLE_ICMPSTAT
#undef ICMPSTAT_SYMBOL
#undef TCPSTAT_SYMBOL
#undef UDPSTAT_SYMBOL

#define ARP_SCAN_FOUR_ARGUMENTS 1

#define _KMEMUSER 1

/*
 * don't define _KERNEL before including sys/unistd.h 
 */
#define NETSNMP_IFNET_NEEDS_KERNEL_LATE  1

#define STREAM_NEEDS_KERNEL_ISLANDS

#ifndef __GNUC__
#  undef NETSNMP_ENABLE_INLINE
#  define NETSNMP_ENABLE_INLINE 0
#endif
