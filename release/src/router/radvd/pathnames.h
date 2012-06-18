/*
 *
 *   Authors:
 *    Pedro Roque		<roque@di.fc.ul.pt>
 *    Lars Fenneberg		<lf@elemental.net>
 *
 *   This software is Copyright 1996,1997 by the above mentioned author(s),
 *   All Rights Reserved.
 *
 *   The license which is distributed with this software in the file COPYRIGHT
 *   applies to this software. If your distribution is missing this file, you
 *   may request it from <pekkas@netcore.fi>.
 *
 */

#ifndef PATHNAMES_H
#define PATHNAMES_H

#ifndef PATH_RADVD_CONF
#define PATH_RADVD_CONF "/etc/radvd.conf"
#endif

#ifndef PATH_RADVD_PID
#define PATH_RADVD_PID "/var/run/radvd.pid"
#endif

#ifndef PATH_RADVD_LOG
#define PATH_RADVD_LOG "/var/log/radvd.log"
#endif

#define PATH_PROC_NET_IF_INET6 "/proc/net/if_inet6"
#define PATH_PROC_NET_IGMP6 "/proc/net/igmp6"

#ifdef __linux__
#define SYSCTL_IP6_FORWARDING CTL_NET, NET_IPV6, NET_IPV6_CONF, NET_PROTO_CONF_ALL, NET_IPV6_FORWARDING
#define PROC_SYS_IP6_FORWARDING "/proc/sys/net/ipv6/conf/all/forwarding"
#define PROC_SYS_IP6_LINKMTU "/proc/sys/net/ipv6/conf/%s/mtu"
#define PROC_SYS_IP6_CURHLIM "/proc/sys/net/ipv6/conf/%s/hop_limit"
#define PROC_SYS_IP6_BASEREACHTIME_MS "/proc/sys/net/ipv6/neigh/%s/base_reachable_time_ms"
#define PROC_SYS_IP6_BASEREACHTIME "/proc/sys/net/ipv6/neigh/%s/base_reachable_time"
#define PROC_SYS_IP6_RETRANSTIMER_MS "/proc/sys/net/ipv6/neigh/%s/retrans_time_ms"
#define PROC_SYS_IP6_RETRANSTIMER "/proc/sys/net/ipv6/neigh/%s/retrans_time"
#else /* BSD */
#define SYSCTL_IP6_FORWARDING CTL_NET, PF_INET6, IPPROTO_IPV6, IPV6CTL_FORWARDING
#endif

#endif
