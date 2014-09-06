/*
 *  Template MIB group interface - var_route.h
 *
 */
#ifndef _MIBGROUP_VAR_ROUTE_H
#define _MIBGROUP_VAR_ROUTE_H

config_require(mibII/ip)
config_arch_require(solaris2, kernel_sunos5)

     void            init_var_route(void);
#if defined(RTENTRY_4_4) && !defined(hpux11)
     struct radix_node;
     void            load_rtentries(struct radix_node *);
#endif
#if defined(freebsd2) || defined(netbsd1) || defined(bsdi2) || defined(openbsd2)
     struct sockaddr_in *klgetsa(struct sockaddr_in *);
#endif

     extern FindVarMethod var_ipRouteEntry;

#if !defined(hpux11) && !defined(solaris2)
     RTENTRY **netsnmp_get_routes(size_t *out_numroutes);
#endif

#endif                          /* _MIBGROUP_VAR_ROUTE_H */
