/*
 *  snmp_rip2.h
 *
 */
#ifndef _MIBGROUP_SNMP_RIP2_H
#define _MIBGROUP_SNMP_RIP2_H

config_require(smux/smux)

     extern FindVarMethod var_rip2;;
     void            init_snmp_rip2(void);


#define RIP2GLOBALROUTECHANGES  0
#define RIP2GLOBALQUERIES       1
#define RIP2IFSTATADDRESS       2
#define RIP2IFSTATRCVBADPKTS    3
#define RIP2IFSTATRCVBADROUTES  4
#define RIP2IFSTATSENTUPDATES   5
#define RIP2IFSTATSTATUS        6
#define RIP2IFCONFADDRESS       7
#define RIP2IFCONFDOMAIN        8
#define RIP2IFCONFAUTHTYPE      9
#define RIP2IFCONFAUTHKEY       10
#define RIP2IFCONFSEND          11
#define RIP2IFCONFRECEIVE       12
#define RIP2IFCONFDEFAULTMETRIC 13
#define RIP2IFCONFSTATUS        14
#define RIP2IFCONFSRCADDRESS    15
#define RIP2PEERADDRESS         16
#define RIP2PEERDOMAIN          17
#define RIP2PEERLASTUPDATE      18
#define RIP2PEERVERSION         19
#define RIP2PEERRCVBADPKTS      20
#define RIP2PEERRCVBADROUTES    21

#endif                          /* _MIBGROUP_SNMP_RIP2_H */
