/*
 *  Template MIB group interface - route_write.h
 *
 */
#ifndef _MIBGROUP_ROUTE_WRITE_H
#define _MIBGROUP_ROUTE_WRITE_H

#if !defined(NETSNMP_ENABLE_MFD_REWRITES)
config_require(mibII/ip)
#endif

     int             addRoute(u_long, u_long, u_long, u_short);
     int             delRoute(u_long, u_long, u_long, u_short);
     struct rtent   *findCacheRTE(u_long);
     struct rtent   *newCacheRTE(void);
     int             delCacheRTE(u_long);
     struct rtent   *cacheKernelRTE(u_long);
     WriteMethod     write_rte;

#endif                          /* _MIBGROUP_ROUTE_WRITE_H */
