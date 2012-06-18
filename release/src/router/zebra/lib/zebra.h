/* Zebra common header.
   Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#ifndef _ZEBRA_H
#define _ZEBRA_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef SUNOS_5
#define _XPG4_2
#define __EXTENSIONS__
typedef unsigned int u_int32_t; 
typedef unsigned short u_int16_t; 
typedef unsigned short u_int8_t; 
#endif /* SUNOS_5 */

#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#endif /* HAVE_SOCKLEN_T */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif /* HAVE_STROPTS_H */
#include <sys/fcntl.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif /* HAVE_SYS_SYSCTL_H */
#include <sys/ioctl.h>
#ifdef HAVE_SYS_CONF_H
#include <sys/conf.h>
#endif /* HAVE_SYS_CONF_H */
#ifdef HAVE_SYS_KSYM_H
#include <sys/ksym.h>
#endif /* HAVE_SYS_KSYM_H */
#include <syslog.h>
#include <time.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#ifdef HAVE_RUSAGE
#include <sys/resource.h>
#endif /* HAVE_RUSAGE */

/* machine dependent includes */
#ifdef SUNOS_5
#include <limits.h>
#include <strings.h>
#endif /* SUNOS_5 */

/* machine dependent includes */
#ifdef HAVE_LINUX_VERSION_H
#include <linux/version.h>
#endif /* HAVE_LINUX_VERSION_H */

#ifdef HAVE_ASM_TYPES_H
#include <asm/types.h>
#endif /* HAVE_ASM_TYPES_H */

/* misc include group */
#include <stdarg.h>
#include <assert.h>

/* network include group */

#include <sys/socket.h>

#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif /* HAVE_SYS_SOCKIO_H */

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#ifdef HAVE_NET_NETOPT_H
#include <net/netopt.h>
#endif /* HAVE_NET_NETOPT_H */

#include <net/if.h>

#ifdef HAVE_NET_IF_DL_H
#include <net/if_dl.h>
#endif /* HAVE_NET_IF_DL_H */

#ifdef HAVE_NET_IF_VAR_H
#include <net/if_var.h>
#endif /* HAVE_NET_IF_VAR_H */

#include <net/route.h>

#ifdef HAVE_NETLINK
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#else
#define RT_TABLE_MAIN		0
#endif /* HAVE_NETLINK */

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif /* HAVE_NETDB_H */

#include <arpa/inet.h>
#include <arpa/telnet.h>

#ifdef HAVE_INET_ND_H
#include <inet/nd.h>
#endif /* HAVE_INET_ND_H */

#ifdef HAVE_NETINET_IN_VAR_H
#include <netinet/in_var.h>
#endif /* HAVE_NETINET_IN_VAR_H */

#ifdef HAVE_NETINET_IN6_VAR_H
#include <netinet/in6_var.h>
#endif /* HAVE_NETINET_IN6_VAR_H */

#ifdef HAVE_NETINET6_IN_H
#include <netinet6/in.h>
#endif /* HAVE_NETINET6_IN_H */


#ifdef HAVE_NETINET6_IP6_H
#include <netinet6/ip6.h>
#endif /* HAVE_NETINET6_IP6_H */

#ifdef HAVE_NETINET_ICMP6_H
#include <netinet/icmp6.h>
#endif /* HAVE_NETINET_ICMP6_H */

#ifdef HAVE_NETINET6_ND6_H
#include <netinet6/nd6.h>
#endif /* HAVE_NETINET6_ND6_H */

#ifdef HAVE_LIBUTIL_H
#include <libutil.h>
#endif /* HAVE_LIBUTIL_H */

#ifdef BSDI_NRL

#ifdef HAVE_NETINET6_IN6_H
#include <netinet6/in6.h>
#endif /* HAVE_NETINET6_IN6_H */

#ifdef NRL
#include <netinet6/in6.h>
#endif /* NRL */

#define IN6_ARE_ADDR_EQUAL IN6_IS_ADDR_EQUAL

/* BSD/OS 4.0 has lost belows defines, it should appear at
   /usr/include/sys/socket.h.  */
#define CMSG_ALIGN(n)           (((n) + 3) & ~3)
#define CMSG_SPACE(l)   (CMSG_ALIGN(sizeof(struct cmsghdr)) + CMSG_ALIGN(l))
#define CMSG_LEN(l)     (CMSG_ALIGN(sizeof(struct cmsghdr)) + (l))

#endif /* BSDI_NRL */

/*  The definition of struct in_pktinfo is missing in old version of
    GLIBC 2.1 (Redhat 6.1).  */
#if defined (GNU_LINUX) && ! defined (HAVE_INPKTINFO)
struct in_pktinfo
{
  int ipi_ifindex;
  struct in_addr ipi_spec_dst;
  struct in_addr ipi_addr;
};
#endif

/* OpenBSD release month check.  */
#ifdef __OpenBSD__
#if OpenBSD < 200311
#define OpenBSD_IP_LEN
#endif /* OpenBSD */
#endif /* __OpenBSD__ */

/* For old definition. */
#ifndef IN6_ARE_ADDR_EQUAL
#define IN6_ARE_ADDR_EQUAL IN6_IS_ADDR_EQUAL
#endif /* IN6_ARE_ADDR_EQUAL */

/* Zebra message types. */
#define ZEBRA_INTERFACE_ADD                1
#define ZEBRA_INTERFACE_DELETE             2
#define ZEBRA_INTERFACE_ADDRESS_ADD        3
#define ZEBRA_INTERFACE_ADDRESS_DELETE     4
#define ZEBRA_INTERFACE_UP                 5
#define ZEBRA_INTERFACE_DOWN               6
#define ZEBRA_IPV4_ROUTE_ADD               7
#define ZEBRA_IPV4_ROUTE_DELETE            8
#define ZEBRA_IPV6_ROUTE_ADD               9
#define ZEBRA_IPV6_ROUTE_DELETE           10
#define ZEBRA_REDISTRIBUTE_ADD            11
#define ZEBRA_REDISTRIBUTE_DELETE         12
#define ZEBRA_REDISTRIBUTE_DEFAULT_ADD    13
#define ZEBRA_REDISTRIBUTE_DEFAULT_DELETE 14
#define ZEBRA_IPV4_NEXTHOP_LOOKUP         15
#define ZEBRA_IPV6_NEXTHOP_LOOKUP         16
#define ZEBRA_IPV4_IMPORT_LOOKUP          17
#define ZEBRA_IPV6_IMPORT_LOOKUP          18
#define ZEBRA_MESSAGE_MAX                 19

/* Zebra route's types. */
#define ZEBRA_ROUTE_SYSTEM               0
#define ZEBRA_ROUTE_KERNEL               1
#define ZEBRA_ROUTE_CONNECT              2
#define ZEBRA_ROUTE_STATIC               3
#define ZEBRA_ROUTE_RIP                  4
#define ZEBRA_ROUTE_RIPNG                5
#define ZEBRA_ROUTE_OSPF                 6
#define ZEBRA_ROUTE_OSPF6                7
#define ZEBRA_ROUTE_BGP                  8
#define ZEBRA_ROUTE_MAX                  9

/* Zebra's family types. */
#define ZEBRA_FAMILY_IPV4                1
#define ZEBRA_FAMILY_IPV6                2
#define ZEBRA_FAMILY_MAX                 3

/* Error codes of zebra. */
#define ZEBRA_ERR_RTEXIST               -1
#define ZEBRA_ERR_RTUNREACH             -2
#define ZEBRA_ERR_EPERM                 -3
#define ZEBRA_ERR_RTNOEXIST             -4

/* Zebra message flags */
#define ZEBRA_FLAG_INTERNAL           0x01
#define ZEBRA_FLAG_SELFROUTE          0x02
#define ZEBRA_FLAG_BLACKHOLE          0x04
#define ZEBRA_FLAG_IBGP               0x08
#define ZEBRA_FLAG_SELECTED           0x10
#define ZEBRA_FLAG_CHANGED            0x20
#define ZEBRA_FLAG_STATIC             0x40

/* Zebra nexthop flags. */
#define ZEBRA_NEXTHOP_IFINDEX            1
#define ZEBRA_NEXTHOP_IFNAME             2
#define ZEBRA_NEXTHOP_IPV4               3
#define ZEBRA_NEXTHOP_IPV4_IFINDEX       4
#define ZEBRA_NEXTHOP_IPV4_IFNAME        5
#define ZEBRA_NEXTHOP_IPV6               6
#define ZEBRA_NEXTHOP_IPV6_IFINDEX       7
#define ZEBRA_NEXTHOP_IPV6_IFNAME        8
#define ZEBRA_NEXTHOP_BLACKHOLE          9 

#ifndef INADDR_LOOPBACK
#define	INADDR_LOOPBACK	0x7f000001	/* Internet address 127.0.0.1.  */
#endif

/* Address family numbers from RFC1700. */
#define AFI_IP                    1
#define AFI_IP6                   2
#define AFI_MAX                   3

/* Subsequent Address Family Identifier. */
#define SAFI_UNICAST              1
#define SAFI_MULTICAST            2
#define SAFI_UNICAST_MULTICAST    3
#define SAFI_MPLS_VPN             4
#define SAFI_MAX                  5

/* Filter direction.  */
#define FILTER_IN                 0
#define FILTER_OUT                1
#define FILTER_MAX                2

/* Default Administrative Distance of each protocol. */
#define ZEBRA_KERNEL_DISTANCE_DEFAULT      0
#define ZEBRA_CONNECT_DISTANCE_DEFAULT     0
#define ZEBRA_STATIC_DISTANCE_DEFAULT      1
#define ZEBRA_RIP_DISTANCE_DEFAULT       120
#define ZEBRA_RIPNG_DISTANCE_DEFAULT     120
#define ZEBRA_OSPF_DISTANCE_DEFAULT      110
#define ZEBRA_OSPF6_DISTANCE_DEFAULT     110
#define ZEBRA_IBGP_DISTANCE_DEFAULT      200
#define ZEBRA_EBGP_DISTANCE_DEFAULT       20

/* Flag manipulation macros. */
#define CHECK_FLAG(V,F)      ((V) & (F))
#define SET_FLAG(V,F)        (V) = (V) | (F)
#define UNSET_FLAG(V,F)      (V) = (V) & ~(F)

/* AFI and SAFI type. */
typedef u_int16_t afi_t;
typedef u_char safi_t;

/* Zebra types. */
typedef u_int16_t zebra_size_t;
typedef u_int8_t zebra_command_t;

/* FIFO -- first in first out structure and macros.  */
struct fifo
{
  struct fifo *next;
  struct fifo *prev;
};

#define FIFO_INIT(F)                                  \
  do {                                                \
    struct fifo *Xfifo = (struct fifo *)(F);          \
    Xfifo->next = Xfifo->prev = Xfifo;                \
  } while (0)

#define FIFO_ADD(F,N)                                 \
  do {                                                \
    struct fifo *Xfifo = (struct fifo *)(F);          \
    struct fifo *Xnode = (struct fifo *)(N);          \
    Xnode->next = Xfifo;                              \
    Xnode->prev = Xfifo->prev;                        \
    Xfifo->prev = Xfifo->prev->next = Xnode;          \
  } while (0)

#define FIFO_DEL(N)                                   \
  do {                                                \
    struct fifo *Xnode = (struct fifo *)(N);          \
    Xnode->prev->next = Xnode->next;                  \
    Xnode->next->prev = Xnode->prev;                  \
  } while (0)

#define FIFO_HEAD(F)                                  \
  ((((struct fifo *)(F))->next == (struct fifo *)(F)) \
  ? NULL : (F)->next)

#define FIFO_EMPTY(F)                                 \
  (((struct fifo *)(F))->next == (struct fifo *)(F))

#define FIFO_TOP(F)                                   \
  (FIFO_EMPTY(F) ? NULL : ((struct fifo *)(F))->next)

#endif /* _ZEBRA_H */
