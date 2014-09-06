/*
 * Portions of this file are subject to copyright(s).  See the Net-SNMP's
 * COPYING file for more details and other copyrights that may apply.
 */
#include <net-snmp/net-snmp-config.h>

#include <sys/types.h>
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#if HAVE_SYS_MBUF_H
#include <sys/mbuf.h>
#endif


#if HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
#if HAVE_NET_ROUTE_H
#include <net/route.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif

#include <errno.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <ctype.h>
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/route.h>

#include "ip-forward-mib/inetCidrRouteTable/inetCidrRouteTable.h"
#include "route_ioctl.h"

#ifdef cygwin
#include <windows.h>
#endif

#ifndef HAVE_STRUCT_RTENTRY_RT_DST
#   define rt_dst rt_nodes->rn_key
#endif
#ifndef HAVE_STRUCT_RTENTRY_RT_HASH
#   define rt_hash rt_pad1
#endif


#ifdef linux
#   define NETSNMP_ROUTE_WRITE_PROTOCOL PF_ROUTE
#else
#   define NETSNMP_ROUTE_WRITE_PROTOCOL 0
#endif

#ifdef irix6
#define SIOCADDRT SIOCADDMULTI
#define SIOCDELRT SIOCDELMULTI
#endif

#if defined SIOCADDRT && !defined(irix6)
int _netsnmp_ioctl_route_set_v4(netsnmp_route_entry * entry)
{
    struct sockaddr_in dst, gateway, mask;
    int             s, rc;
    RTENTRY         route;
    char *          DEBUGSTR;

    netsnmp_assert(NULL != entry); /* checked in netsnmp_arch_route_create */
    netsnmp_assert((4 == entry->rt_dest_len) && (4 == entry->rt_nexthop_len));

    s = socket(AF_INET, SOCK_RAW, NETSNMP_ROUTE_WRITE_PROTOCOL);
    if (s < 0) {
        snmp_log_perror("socket");
        return -3;
    }

    memset(&route, 0, sizeof(route));

    dst.sin_family = AF_INET;
    memcpy(&dst.sin_addr.s_addr, entry->rt_dest, 4);
    DEBUGSTR = inet_ntoa(dst.sin_addr);
    DEBUGMSGTL(("access:route","*** route to %s\n", DEBUGSTR));

    gateway.sin_family = AF_INET;
    memcpy(&gateway.sin_addr.s_addr, entry->rt_nexthop, 4);
    DEBUGSTR = inet_ntoa(gateway.sin_addr);
    DEBUGMSGTL(("access:route","    via %s\n", DEBUGSTR));

    mask.sin_family = AF_INET;
    mask.sin_addr.s_addr = htonl(0);
    DEBUGSTR = inet_ntoa(mask.sin_addr);
    DEBUGMSGTL(("access:route","    mask %s\n", DEBUGSTR));

    memcpy(&route.rt_dst, &dst, sizeof(struct sockaddr_in));
    memcpy(&route.rt_gateway, &gateway, sizeof(struct sockaddr_in));
    memcpy(&route.rt_genmask, &mask, sizeof(struct sockaddr_in));

    if (32 == entry->rt_pfx_len)
        route.rt_flags |= RTF_HOST;
    if (INETCIDRROUTETYPE_REMOTE == entry->rt_type)
        route.rt_flags |= RTF_GATEWAY;
    route.rt_flags |= RTF_UP;

#ifndef RTENTRY_4_4
    route.rt_hash = entry->if_index;
#endif

    rc = ioctl(s, SIOCADDRT, (caddr_t) & route);
    close(s);
    if (rc < 0) {
        snmp_log_perror("ioctl");
        return -4;
    }

    return 0;
}
#endif

#if defined SIOCDELRT && !defined(irix6)
int _netsnmp_ioctl_route_delete_v4(netsnmp_route_entry * entry)
{
    struct sockaddr_in dst;
    struct sockaddr_in gateway;
    int             s, rc;
    RTENTRY         route;

    netsnmp_assert(NULL != entry); /* checked in netsnmp_arch_route_delete */
    netsnmp_assert((4 == entry->rt_dest_len) && (4 == entry->rt_nexthop_len));

    s = socket(AF_INET, SOCK_RAW, NETSNMP_ROUTE_WRITE_PROTOCOL);
    if (s < 0) {
        snmp_log_perror("socket");
        return -3;
    }

    memset(&route, 0, sizeof(route));

    dst.sin_family = AF_INET;
    memcpy(&dst.sin_addr.s_addr, entry->rt_dest, 4);

    gateway.sin_family = AF_INET;
    memcpy(&gateway.sin_addr.s_addr, entry->rt_nexthop, 4);

    memcpy(&route.rt_dst, &dst, sizeof(struct sockaddr_in));
    memcpy(&route.rt_gateway, &gateway, sizeof(struct sockaddr_in));

    if (32 == entry->rt_pfx_len)
        route.rt_flags |= RTF_HOST;
    if (INETCIDRROUTETYPE_REMOTE == entry->rt_type)
        route.rt_flags |= RTF_GATEWAY;
    route.rt_flags |= RTF_UP;

#ifndef RTENTRY_4_4
    route.rt_hash = entry->if_index;
#endif

    rc = ioctl(s, SIOCDELRT, (caddr_t) & route);
    close(s);
    if (rc < 0) {
        snmp_log_perror("ioctl");
        rc = -4;
    }

    return rc;
}
#endif                           /* SIOCDELRT */
