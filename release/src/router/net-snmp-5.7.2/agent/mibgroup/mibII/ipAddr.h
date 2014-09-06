/*
 *  Template MIB group interface - ipAddr.h
 *
 */
#ifndef _MIBGROUP_IPADDR_H
#define _MIBGROUP_IPADDR_H

#if !defined(NETSNMP_ENABLE_MFD_REWRITES)
config_require(mibII/ip)
#endif

     extern FindVarMethod var_ipAddrEntry;

#endif                          /* _MIBGROUP_IPADDR_H */
