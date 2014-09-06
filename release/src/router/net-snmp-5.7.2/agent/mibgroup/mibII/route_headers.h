#if defined(NETSNMP_CAN_USE_SYSCTL)

#include <stddef.h>
#include <stdlib.h>
#include <syslog.h>
#include <time.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <net/if_dl.h>
#if HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
#include <net/route.h>
#include <netinet/in.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "ip.h"
#include "kernel.h"
#include "interfaces.h"

#else /* !NETSNMP_CAN_USE_SYSCTL */

#define GATEWAY                 /* MultiNet is always configured this way! */
#include <stdio.h>
#include <sys/types.h>
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_SYSLOG_H
#include <syslog.h>
#endif
#if HAVE_MACHINE_PARAM_H
#include <machine/param.h>
#endif
#if HAVE_SYS_MBUF_H
#include <sys/mbuf.h>
#endif
#if HAVE_NET_IF_H
#include <net/if.h>
#endif
#ifdef HAVE_NET_IF_VAR_H
#include <net/if_var.h>
#endif
#if HAVE_SYS_HASHING_H
#include <sys/hashing.h>
#endif
#if HAVE_NETINET_IN_VAR_H
#include <netinet/in_var.h>
#endif
#define KERNEL                  /* to get routehash and RTHASHSIZ */
#if HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
#if HAVE_NET_ROUTE_H
#include <net/route.h>
#endif
#undef	KERNEL
#ifdef RTENTRY_4_4
#ifndef HAVE_STRUCT_RTENTRY_RT_UNIT
#define rt_unit rt_refcnt       /* Reuse this field for device # */
#endif
#ifndef HAVE_STRUCT_RTENTRY_RT_DST
#define rt_dst rt_nodes->rn_key
#endif
#else                           /* RTENTRY_4_3 */
#ifndef HAVE_STRUCT_RTENTRY_RT_DST
#define rt_dst rt_nodes->rn_key
#endif
#ifndef HAVE_STRUCT_RTENTRY_RT_HASH
#define rt_hash rt_pad1
#endif
#ifndef HAVE_STRUCT_RTENTRY_RT_REFCNT
#ifndef hpux10
#define rt_refcnt rt_pad2
#endif
#endif
#ifndef HAVE_STRUCT_RTENTRY_RT_USE
#define rt_use rt_pad3
#endif
#ifndef HAVE_STRUCT_RTENTRY_RT_UNIT
#define rt_unit rt_refcnt       /* Reuse this field for device # */
#endif
#endif
#ifndef NULL
#define NULL 0
#endif
#if HAVE_KVM_OPENFILES
#include <fcntl.h>
#endif
#if HAVE_KVM_H
#include <kvm.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_INET_MIB2_H
#include <inet/mib2.h>
#endif
#if HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>
#endif
#if HAVE_NET_IF_DL_H
#ifndef dynix
#include <net/if_dl.h>
#else
#include <sys/net/if_dl.h>
#endif
#endif

#if HAVE_NLIST_H
#include <nlist.h>
#endif

#ifdef solaris2
#include "kernel_sunos5.h"
/* Solaris 2.6/7 need sys/stream.h (mblk_t) to include inet/ip.h */
#ifdef HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
/* Solaris 2.6 needs inet/common.h (u16) to include inet/ip.h */
#ifdef HAVE_INET_COMMON_H
#include <inet/common.h>
#endif
#ifdef HAVE_INET_IP_H
#include <inet/ip.h>
#endif /* HAVE_INET_IP_H */
#endif

#ifdef HAVE_SYS_SYSCTL_H
# ifdef CTL_NET
#  ifdef PF_ROUTE
#   ifdef NET_RT_DUMP
#    define USE_SYSCTL_ROUTE_DUMP
#   endif
#  endif
# endif
#endif

#ifdef cygwin
#include <windows.h>
#endif

#endif /* !NETSNMP_CAN_USE_SYSCTL */
