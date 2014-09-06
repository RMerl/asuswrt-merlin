/*
 *  ICMP MIB group interface - icmp.h
 *
 */
#ifndef _MIBGROUP_ICMP_H
#define _MIBGROUP_ICMP_H

config_arch_require(solaris2,        kernel_sunos5)
config_arch_require(linux,     mibII/kernel_linux)
config_arch_require(netbsd,    mibII/kernel_netbsd)
config_arch_require(netbsdelf, mibII/kernel_netbsd)

#include <net-snmp/agent/cache_handler.h>

extern void     init_icmp(void);
extern Netsnmp_Node_Handler icmp_handler;
extern Netsnmp_Node_Handler icmp_stats_table_handler;
extern Netsnmp_Node_Handler icmp_msg_stats_table_handler;
extern NetsnmpCacheLoad icmp_load;
extern NetsnmpCacheFree icmp_free;

#define ICMPINMSGS	      1
#define ICMPINERRORS	      2
#define ICMPINDESTUNREACHS    3
#define ICMPINTIMEEXCDS       4
#define ICMPINPARMPROBS       5
#define ICMPINSRCQUENCHS      6
#define ICMPINREDIRECTS       7
#define ICMPINECHOS	      8
#define ICMPINECHOREPS	      9
#define ICMPINTIMESTAMPS     10
#define ICMPINTIMESTAMPREPS  11
#define ICMPINADDRMASKS      12
#define ICMPINADDRMASKREPS   13
#define ICMPOUTMSGS	     14
#define ICMPOUTERRORS	     15
#define ICMPOUTDESTUNREACHS  16
#define ICMPOUTTIMEEXCDS     17
#define ICMPOUTPARMPROBS     18
#define ICMPOUTSRCQUENCHS    19
#define ICMPOUTREDIRECTS     20
#define ICMPOUTECHOS	     21
#define ICMPOUTECHOREPS      22
#define ICMPOUTTIMESTAMPS    23
#define ICMPOUTTIMESTAMPREPS 24
#define ICMPOUTADDRMASKS     25
#define ICMPOUTADDRMASKREPS  26

#define ICMPSTATSTABLE       29
#define ICMP_STAT_IPVER       1
#define ICMP_STAT_INMSG       2 
#define ICMP_STAT_INERR       3 
#define ICMP_STAT_OUTMSG      4 
#define ICMP_STAT_OUTERR      5 

#define ICMPMSGSTATSTABLE     30
#define ICMP_MSG_STAT_IPVER    1
#define ICMP_MSG_STAT_TYPE     2
#define ICMP_MSG_STAT_IN_PKTS  3
#define ICMP_MSG_STAT_OUT_PKTS 4

#endif                          /* _MIBGROUP_ICMP_H */
