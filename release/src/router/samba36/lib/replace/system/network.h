#ifndef _system_network_h
#define _system_network_h
/* 
   Unix SMB/CIFS implementation.

   networking system include wrappers

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Jelmer Vernooij 2007
   
     ** NOTE! The following LGPL license applies to the replace
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.

*/

#ifndef LIBREPLACE_NETWORK_CHECKS
#error "AC_LIBREPLACE_NETWORK_CHECKS missing in configure"
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_UNIXSOCKET
#include <sys/un.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

/*
 * The next three defines are needed to access the IPTOS_* options
 * on some systems.
 */

#ifdef HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#endif

#ifdef HAVE_NETINET_IN_IP_H
#include <netinet/in_ip.h>
#endif

#ifdef HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif

#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif

#ifndef HAVE_SOCKLEN_T
#define HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

#if !defined (HAVE_INET_NTOA) || defined(REPLACE_INET_NTOA)
/* define is in "replace.h" */
char *rep_inet_ntoa(struct in_addr ip);
#endif

#ifndef HAVE_INET_PTON
/* define is in "replace.h" */
int rep_inet_pton(int af, const char *src, void *dst);
#endif

#ifndef HAVE_INET_NTOP
/* define is in "replace.h" */
const char *rep_inet_ntop(int af, const void *src, char *dst, socklen_t size);
#endif

#ifndef HAVE_INET_ATON
/* define is in "replace.h" */
int rep_inet_aton(const char *src, struct in_addr *dst);
#endif

#ifndef HAVE_CONNECT
/* define is in "replace.h" */
int rep_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
#endif

#ifndef HAVE_GETHOSTBYNAME
/* define is in "replace.h" */
struct hostent *rep_gethostbyname(const char *name);
#endif

#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif

#ifndef HAVE_STRUCT_IFADDRS
struct ifaddrs {
	struct ifaddrs   *ifa_next;         /* Pointer to next struct */
	char             *ifa_name;         /* Interface name */
	unsigned int     ifa_flags;         /* Interface flags */
	struct sockaddr  *ifa_addr;         /* Interface address */
	struct sockaddr  *ifa_netmask;      /* Interface netmask */
#undef ifa_dstaddr
	struct sockaddr  *ifa_dstaddr;      /* P2P interface destination */
	void             *ifa_data;         /* Address specific data */
};
#endif

#ifndef HAVE_GETIFADDRS
int rep_getifaddrs(struct ifaddrs **);
#endif

#ifndef HAVE_FREEIFADDRS
void rep_freeifaddrs(struct ifaddrs *);
#endif

#ifndef HAVE_SOCKETPAIR
/* define is in "replace.h" */
int rep_socketpair(int d, int type, int protocol, int sv[2]);
#endif

/*
 * Some systems have getaddrinfo but not the
 * defines needed to use it.
 */

/* Various macros that ought to be in <netdb.h>, but might not be */

#ifndef EAI_FAIL
#define EAI_BADFLAGS	(-1)
#define EAI_NONAME	(-2)
#define EAI_AGAIN	(-3)
#define EAI_FAIL	(-4)
#define EAI_FAMILY	(-6)
#define EAI_SOCKTYPE	(-7)
#define EAI_SERVICE	(-8)
#define EAI_MEMORY	(-10)
#define EAI_SYSTEM	(-11)
#endif   /* !EAI_FAIL */

#ifndef AI_PASSIVE
#define AI_PASSIVE	0x0001
#endif

#ifndef AI_CANONNAME
#define AI_CANONNAME	0x0002
#endif

#ifndef AI_NUMERICHOST
/*
 * some platforms don't support AI_NUMERICHOST; define as zero if using
 * the system version of getaddrinfo...
 */
#if defined(HAVE_STRUCT_ADDRINFO) && defined(HAVE_GETADDRINFO)
#define AI_NUMERICHOST	0
#else
#define AI_NUMERICHOST	0x0004
#endif
#endif

/*
 * Some of the functions in source3/lib/util_sock.c use AI_ADDRCONFIG. On QNX
 * 6.3.0, this macro is defined but, if it's used, getaddrinfo will fail. This
 * prevents smbd from opening any sockets.
 *
 * If I undefine AI_ADDRCONFIG on such systems and define it to be 0,
 * this works around the issue.
 */
#ifdef __QNX__
#include <sys/neutrino.h>
#if _NTO_VERSION == 630
#undef AI_ADDRCONFIG
#endif
#endif
#ifndef AI_ADDRCONFIG
/*
 * logic copied from AI_NUMERICHOST
 */
#if defined(HAVE_STRUCT_ADDRINFO) && defined(HAVE_GETADDRINFO)
#define AI_ADDRCONFIG	0
#else
#define AI_ADDRCONFIG	0x0020
#endif
#endif

#ifndef AI_NUMERICSERV
/*
 * logic copied from AI_NUMERICHOST
 */
#if defined(HAVE_STRUCT_ADDRINFO) && defined(HAVE_GETADDRINFO)
#define AI_NUMERICSERV	0
#else
#define AI_NUMERICSERV	0x0400
#endif
#endif

#ifndef NI_NUMERICHOST
#define NI_NUMERICHOST	1
#endif

#ifndef NI_NUMERICSERV
#define NI_NUMERICSERV	2
#endif

#ifndef NI_NOFQDN
#define NI_NOFQDN	4
#endif

#ifndef NI_NAMEREQD
#define NI_NAMEREQD 	8
#endif

#ifndef NI_DGRAM
#define NI_DGRAM	16
#endif


#ifndef NI_MAXHOST
#define NI_MAXHOST	1025
#endif

#ifndef NI_MAXSERV
#define NI_MAXSERV	32
#endif

/*
 * glibc on linux doesn't seem to have MSG_WAITALL
 * defined. I think the kernel has it though..
 */
#ifndef MSG_WAITALL
#define MSG_WAITALL 0
#endif

#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001
#endif

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT EINVAL
#endif

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN HOST_NAME_MAX
#endif

#ifndef HAVE_SA_FAMILY_T
#define HAVE_SA_FAMILY_T
typedef unsigned short int sa_family_t;
#endif

#ifndef HAVE_STRUCT_SOCKADDR_STORAGE
#define HAVE_STRUCT_SOCKADDR_STORAGE
#ifdef HAVE_STRUCT_SOCKADDR_IN6
#define sockaddr_storage sockaddr_in6
#define ss_family sin6_family
#define HAVE_SS_FAMILY 1
#else /*HAVE_STRUCT_SOCKADDR_IN6*/
#define sockaddr_storage sockaddr_in
#define ss_family sin_family
#define HAVE_SS_FAMILY 1
#endif /*HAVE_STRUCT_SOCKADDR_IN6*/
#endif /*HAVE_STRUCT_SOCKADDR_STORAGE*/

#ifndef HAVE_SS_FAMILY
#ifdef HAVE___SS_FAMILY
#define ss_family __ss_family
#define HAVE_SS_FAMILY 1
#endif
#endif

#ifndef IOV_MAX
# ifdef UIO_MAXIOV
#  define IOV_MAX UIO_MAXIOV
# else
#  ifdef __sgi
    /*
     * IRIX 6.5 has sysconf(_SC_IOV_MAX)
     * which might return 512 or bigger
     */
#   define IOV_MAX 512
#  endif
# endif
#endif

#ifndef HAVE_STRUCT_ADDRINFO
#define HAVE_STRUCT_ADDRINFO
struct addrinfo {
	int			ai_flags;
	int			ai_family;
	int			ai_socktype;
	int			ai_protocol;
	socklen_t		ai_addrlen;
	struct sockaddr 	*ai_addr;
	char			*ai_canonname;
	struct addrinfo		*ai_next;
};
#endif   /* HAVE_STRUCT_ADDRINFO */

#if !defined(HAVE_GETADDRINFO)
#include "getaddrinfo.h"
#endif

/* Needed for some systems that don't define it (Solaris). */
#ifndef ifr_netmask
#define ifr_netmask ifr_addr
#endif

/* Some old Linux systems have broken header files */
#ifdef HAVE_IPV6
#ifdef HAVE_LINUX_IPV6_V6ONLY_26
#define IPV6_V6ONLY 26
#endif /* HAVE_LINUX_IPV6_V6ONLY_26 */
#endif /* HAVE_IPV6 */

#ifdef SOCKET_WRAPPER
#ifndef SOCKET_WRAPPER_DISABLE
#ifndef SOCKET_WRAPPER_NOT_REPLACE
#define SOCKET_WRAPPER_REPLACE
#endif /* SOCKET_WRAPPER_NOT_REPLACE */
#include "../socket_wrapper/socket_wrapper.h"
#endif /* SOCKET_WRAPPER_DISABLE */
#endif /* SOCKET_WRAPPER */

#endif
