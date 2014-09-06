/*
 *  Template MIB group interface - udp.h
 *
 */
#ifndef _MIBGROUP_UDPTABLE_H
#define _MIBGROUP_UDPTABLE_H

config_arch_require(solaris2, kernel_sunos5)
config_require(mibII/ip)

extern void     init_udpTable(void);
extern Netsnmp_Node_Handler udpTable_handler;
extern NetsnmpCacheLoad udpTable_load;
extern NetsnmpCacheFree udpTable_free;
extern Netsnmp_First_Data_Point udpTable_first_entry;
extern Netsnmp_Next_Data_Point  udpTable_next_entry;

#define UDPLOCALADDRESS     1
#define UDPLOCALPORT	    2

#endif                          /* _MIBGROUP_UDPTABLE_H */
