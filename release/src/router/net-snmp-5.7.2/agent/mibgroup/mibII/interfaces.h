/*
 *  Interfaces MIB group interface - interfaces.h
 *
 */
#ifndef _MIBGROUP_INTERFACES_H
#define _MIBGROUP_INTERFACES_H

/***********************************************************************
 * configure macros
 */
config_require(util_funcs/header_generic)

/*
 * conflicts with the new MFD rewrite
 */
config_exclude(if-mib/ifTable/ifTable)

#if !defined(WIN32) && !defined(cygwin)
config_require(if-mib/data_access/interface)
#endif

config_arch_require(solaris2, kernel_sunos5)
/*
 * need get_address in var_route for some platforms (USE_SYSCTL_IFLIST).
 * Not sure if that can be translated into a config_arch_require, so be
 * indiscriminate for now.
 */
config_require(mibII/var_route)

/***********************************************************************
 */
#ifndef USING_IF_MIB_IFTABLE_MODULE
#ifdef hpux11
#include <sys/mib.h>
#else
     struct in_ifaddr;
     struct ifnet;
#endif

     int             Interface_Scan_Get_Count(void);
     int             Interface_Index_By_Name(char *, int);
     void            Interface_Scan_Init(void);
#if defined(linux) || defined(sunV3)
     struct in_ifaddr {
         int             dummy;
     };
#endif
#if defined(hpux11)
     int             Interface_Scan_Next(short *, char *, nmapi_phystat *);
     int             Interface_Scan_NextInt(int *, char *, nmapi_phystat *);
#else
     int             Interface_Scan_Next(short *, char *, struct ifnet *,
                                         struct in_ifaddr *);
     int             Interface_Scan_NextInt(int *, char *, struct ifnet *,
                                         struct in_ifaddr *);
#endif

     void            init_interfaces(void);
     extern FindVarMethod var_interfaces;
     extern FindVarMethod var_ifEntry;

#endif /* USING_IF_MIB_IFTABLE_MODULE */

#define NETSNMP_IFNUMBER        0
#define NETSNMP_IFINDEX         1
#define NETSNMP_IFDESCR         2
#define NETSNMP_IFTYPE          3
#define NETSNMP_IFMTU           4
#define NETSNMP_IFSPEED         5
#define NETSNMP_IFPHYSADDRESS   6
#define NETSNMP_IFADMINSTATUS   7
#define NETSNMP_IFOPERSTATUS    8
#define NETSNMP_IFLASTCHANGE    9
#define NETSNMP_IFINOCTETS      10
#define NETSNMP_IFINUCASTPKTS   11
#define NETSNMP_IFINNUCASTPKTS  12
#define NETSNMP_IFINDISCARDS    13
#define NETSNMP_IFINERRORS      14
#define NETSNMP_IFINUNKNOWNPROTOS 15
#define NETSNMP_IFOUTOCTETS     16
#define NETSNMP_IFOUTUCASTPKTS  17
#define NETSNMP_IFOUTNUCASTPKTS 18
#define NETSNMP_IFOUTDISCARDS   19
#define NETSNMP_IFOUTERRORS     20
#define NETSNMP_IFOUTQLEN       21
#define NETSNMP_IFSPECIFIC      22

#ifdef linux
/*
 * this struct ifnet is cloned from the generic type and somewhat modified.
 * it will not work for other un*x'es...
 */

     struct ifnet {
         char           *if_name;       /* name, e.g. ``en'' or ``lo'' */
         char           *if_unit;       /* sub-unit for lower level driver */
         short           if_mtu;        /* maximum transmission unit */
         short           if_flags;      /* up/down, broadcast, etc. */
         int             if_metric;     /* routing metric (external only) */
         char            if_hwaddr[6];  /* ethernet address */
         int             if_type;       /* interface type: 1=generic,
                                         * 28=slip, ether=6, loopback=24 */
         u_long          if_speed;      /* interface speed: in bits/sec */

         struct sockaddr if_addr;       /* interface's address */
         struct sockaddr ifu_broadaddr; /* broadcast address */
         struct sockaddr ia_subnetmask; /* interface's mask */

         struct ifqueue {
             int             ifq_len;
             int             ifq_drops;
         } if_snd;              /* output queue */
         u_long          if_ibytes;     /* octets received on interface */
         u_long          if_ipackets;   /* packets received on interface */
         u_long          if_ierrors;    /* input errors on interface */
         u_long          if_iqdrops;    /* input queue overruns */
         u_long          if_obytes;     /* octets sent on interface */
         u_long          if_opackets;   /* packets sent on interface */
         u_long          if_oerrors;    /* output errors on interface */
         u_long          if_collisions; /* collisions on csma interfaces */
         /*
          * end statistics 
          */
         struct ifnet   *if_next;
     };
#endif   /* linux */
#endif                          /* _MIBGROUP_INTERFACES_H */
