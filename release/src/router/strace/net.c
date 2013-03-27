/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-2000 Wichert Akkerman <wichert@cistron.nl>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id$
 */

#include "defs.h"

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#if defined(HAVE_SIN6_SCOPE_ID_LINUX)
#define in6_addr in6_addr_libc
#define ipv6_mreq ipv6_mreq_libc
#define sockaddr_in6 sockaddr_in6_libc
#endif

#include <netinet/in.h>
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#ifdef HAVE_NETINET_UDP_H
#include <netinet/udp.h>
#endif
#ifdef HAVE_NETINET_SCTP_H
#include <netinet/sctp.h>
#endif
#include <arpa/inet.h>
#include <net/if.h>
#if defined(LINUX)
#include <asm/types.h>
#if defined(__GLIBC__) && (__GLIBC__ >= 2) && (__GLIBC__ + __GLIBC_MINOR__ >= 3)
#  include <netipx/ipx.h>
#else
#  include <linux/ipx.h>
#endif
#endif /* LINUX */

#if defined (__GLIBC__) && (((__GLIBC__ < 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 1)) || defined(HAVE_SIN6_SCOPE_ID_LINUX))
#if defined(HAVE_LINUX_IN6_H)
#if defined(HAVE_SIN6_SCOPE_ID_LINUX)
#undef in6_addr
#undef ipv6_mreq
#undef sockaddr_in6
#define in6_addr in6_addr_kernel
#define ipv6_mreq ipv6_mreq_kernel
#define sockaddr_in6 sockaddr_in6_kernel
#endif
#include <linux/in6.h>
#if defined(HAVE_SIN6_SCOPE_ID_LINUX)
#undef in6_addr
#undef ipv6_mreq
#undef sockaddr_in6
#define in6_addr in6_addr_libc
#define ipv6_mreq ipv6_mreq_libc
#define sockaddr_in6 sockaddr_in6_kernel
#endif
#endif
#endif

#if defined(HAVE_SYS_UIO_H)
#include <sys/uio.h>
#endif

#if defined(HAVE_LINUX_NETLINK_H)
#include <linux/netlink.h>
#endif

#if defined(HAVE_LINUX_IF_PACKET_H)
#include <linux/if_packet.h>
#endif

#if defined(HAVE_LINUX_ICMP_H)
#include <linux/icmp.h>
#endif

#ifndef PF_UNSPEC
#define PF_UNSPEC AF_UNSPEC
#endif

#if UNIXWARE >= 7
#define HAVE_SENDMSG		1		/* HACK - *FIXME* */
#endif

#ifdef LINUX
/* Under Linux these are enums so we can't test for them with ifdef. */
#define IPPROTO_EGP IPPROTO_EGP
#define IPPROTO_PUP IPPROTO_PUP
#define IPPROTO_IDP IPPROTO_IDP
#define IPPROTO_IGMP IPPROTO_IGMP
#define IPPROTO_RAW IPPROTO_RAW
#define IPPROTO_MAX IPPROTO_MAX
#endif

static const struct xlat domains[] = {
#ifdef PF_AAL5
	{ PF_AAL5,	"PF_AAL5"	},
#endif
#ifdef PF_APPLETALK
	{ PF_APPLETALK,	"PF_APPLETALK"	},
#endif
#ifdef PF_ASH
	{ PF_ASH,	"PF_ASH"	},
#endif
#ifdef PF_ATMPVC
	{ PF_ATMPVC,	"PF_ATMPVC"	},
#endif
#ifdef PF_ATMSVC
	{ PF_ATMSVC,	"PF_ATMSVC"	},
#endif
#ifdef PF_AX25
	{ PF_AX25,	"PF_AX25"	},
#endif
#ifdef PF_BLUETOOTH
	{ PF_BLUETOOTH,	"PF_BLUETOOTH"	},
#endif
#ifdef PF_BRIDGE
	{ PF_BRIDGE,	"PF_BRIDGE"	},
#endif
#ifdef PF_DECnet
	{ PF_DECnet,	"PF_DECnet"	},
#endif
#ifdef PF_DECNET
	{ PF_DECNET,	"PF_DECNET"	},
#endif
#ifdef PF_ECONET
	{ PF_ECONET,	"PF_ECONET"	},
#endif
#ifdef PF_FILE
	{ PF_FILE,	"PF_FILE"	},
#endif
#ifdef PF_IMPLINK
	{ PF_IMPLINK,	"PF_IMPLINK"	},
#endif
#ifdef PF_INET
	{ PF_INET,	"PF_INET"	},
#endif
#ifdef PF_INET6
	{ PF_INET6,	"PF_INET6"	},
#endif
#ifdef PF_IPX
	{ PF_IPX,	"PF_IPX"	},
#endif
#ifdef PF_IRDA
	{ PF_IRDA,	"PF_IRDA"	},
#endif
#ifdef PF_ISO
	{ PF_ISO,	"PF_ISO"	},
#endif
#ifdef PF_KEY
	{ PF_KEY,	"PF_KEY"	},
#endif
#ifdef PF_UNIX
	{ PF_UNIX,	"PF_UNIX"	},
#endif
#ifdef PF_LOCAL
	{ PF_LOCAL,	"PF_LOCAL"	},
#endif
#ifdef PF_NETBEUI
	{ PF_NETBEUI,	"PF_NETBEUI"	},
#endif
#ifdef PF_NETLINK
	{ PF_NETLINK,	"PF_NETLINK"	},
#endif
#ifdef PF_NETROM
	{ PF_NETROM,	"PF_NETROM"	},
#endif
#ifdef PF_PACKET
	{ PF_PACKET,	"PF_PACKET"	},
#endif
#ifdef PF_PPPOX
	{ PF_PPPOX,	"PF_PPPOX"	},
#endif
#ifdef PF_ROSE
	{ PF_ROSE,	"PF_ROSE"	},
#endif
#ifdef PF_ROUTE
	{ PF_ROUTE,	"PF_ROUTE"	},
#endif
#ifdef PF_SECURITY
	{ PF_SECURITY,	"PF_SECURITY"	},
#endif
#ifdef PF_SNA
	{ PF_SNA,	"PF_SNA"	},
#endif
#ifdef PF_UNSPEC
	{ PF_UNSPEC,	"PF_UNSPEC"	},
#endif
#ifdef PF_WANPIPE
	{ PF_WANPIPE,	"PF_WANPIPE"	},
#endif
#ifdef PF_X25
	{ PF_X25,	"PF_X25"	},
#endif
	{ 0,		NULL		},
};
const struct xlat addrfams[] = {
#ifdef AF_APPLETALK
	{ AF_APPLETALK,	"AF_APPLETALK"	},
#endif
#ifdef AF_ASH
	{ AF_ASH,	"AF_ASH"	},
#endif
#ifdef AF_ATMPVC
	{ AF_ATMPVC,	"AF_ATMPVC"	},
#endif
#ifdef AF_ATMSVC
	{ AF_ATMSVC,	"AF_ATMSVC"	},
#endif
#ifdef AF_AX25
	{ AF_AX25,	"AF_AX25"	},
#endif
#ifdef AF_BLUETOOTH
	{ AF_BLUETOOTH,	"AF_BLUETOOTH"	},
#endif
#ifdef AF_BRIDGE
	{ AF_BRIDGE,	"AF_BRIDGE"	},
#endif
#ifdef AF_DECnet
	{ AF_DECnet,	"AF_DECnet"	},
#endif
#ifdef AF_ECONET
	{ AF_ECONET,	"AF_ECONET"	},
#endif
#ifdef AF_FILE
	{ AF_FILE,	"AF_FILE"	},
#endif
#ifdef AF_IMPLINK
	{ AF_IMPLINK,	"AF_IMPLINK"	},
#endif
#ifdef AF_INET
	{ AF_INET,	"AF_INET"	},
#endif
#ifdef AF_INET6
	{ AF_INET6,	"AF_INET6"	},
#endif
#ifdef AF_IPX
	{ AF_IPX,	"AF_IPX"	},
#endif
#ifdef AF_IRDA
	{ AF_IRDA,	"AF_IRDA"	},
#endif
#ifdef AF_ISO
	{ AF_ISO,	"AF_ISO"	},
#endif
#ifdef AF_KEY
	{ AF_KEY,	"AF_KEY"	},
#endif
#ifdef AF_UNIX
	{ AF_UNIX,	"AF_UNIX"	},
#endif
#ifdef AF_LOCAL
	{ AF_LOCAL,	"AF_LOCAL"	},
#endif
#ifdef AF_NETBEUI
	{ AF_NETBEUI,	"AF_NETBEUI"	},
#endif
#ifdef AF_NETLINK
	{ AF_NETLINK,	"AF_NETLINK"	},
#endif
#ifdef AF_NETROM
	{ AF_NETROM,	"AF_NETROM"	},
#endif
#ifdef AF_PACKET
	{ AF_PACKET,	"AF_PACKET"	},
#endif
#ifdef AF_PPPOX
	{ AF_PPPOX,	"AF_PPPOX"	},
#endif
#ifdef AF_ROSE
	{ AF_ROSE,	"AF_ROSE"	},
#endif
#ifdef AF_ROUTE
	{ AF_ROUTE,	"AF_ROUTE"	},
#endif
#ifdef AF_SECURITY
	{ AF_SECURITY,	"AF_SECURITY"	},
#endif
#ifdef AF_SNA
	{ AF_SNA,	"AF_SNA"	},
#endif
#ifdef AF_UNSPEC
	{ AF_UNSPEC,	"AF_UNSPEC"	},
#endif
#ifdef AF_WANPIPE
	{ AF_WANPIPE,	"AF_WANPIPE"	},
#endif
#ifdef AF_X25
	{ AF_X25,	"AF_X25"	},
#endif
	{ 0,		NULL		},
};
static const struct xlat socktypes[] = {
	{ SOCK_STREAM,	"SOCK_STREAM"	},
	{ SOCK_DGRAM,	"SOCK_DGRAM"	},
#ifdef SOCK_RAW
	{ SOCK_RAW,	"SOCK_RAW"	},
#endif
#ifdef SOCK_RDM
	{ SOCK_RDM,	"SOCK_RDM"	},
#endif
#ifdef SOCK_SEQPACKET
	{ SOCK_SEQPACKET,"SOCK_SEQPACKET"},
#endif
#ifdef SOCK_DCCP
	{ SOCK_DCCP,	"SOCK_DCCP"	},
#endif
#ifdef SOCK_PACKET
	{ SOCK_PACKET,	"SOCK_PACKET"	},
#endif
	{ 0,		NULL		},
};
static const struct xlat sock_type_flags[] = {
#ifdef SOCK_CLOEXEC
	{ SOCK_CLOEXEC,	"SOCK_CLOEXEC"	},
#endif
#ifdef SOCK_NONBLOCK
	{ SOCK_NONBLOCK,"SOCK_NONBLOCK"	},
#endif
	{ 0,		NULL		},
};
#ifndef SOCK_TYPE_MASK
# define SOCK_TYPE_MASK 0xf
#endif
static const struct xlat socketlayers[] = {
#if defined(SOL_IP)
	{ SOL_IP,	"SOL_IP"	},
#endif
#if defined(SOL_ICMP)
	{ SOL_ICMP,	"SOL_ICMP"	},
#endif
#if defined(SOL_TCP)
	{ SOL_TCP,	"SOL_TCP"	},
#endif
#if defined(SOL_UDP)
	{ SOL_UDP,	"SOL_UDP"	},
#endif
#if defined(SOL_IPV6)
	{ SOL_IPV6,	"SOL_IPV6"	},
#endif
#if defined(SOL_ICMPV6)
	{ SOL_ICMPV6,	"SOL_ICMPV6"	},
#endif
#if defined(SOL_SCTP)
	{ SOL_SCTP,	"SOL_SCTP"	},
#endif
#if defined(SOL_UDPLITE)
	{ SOL_UDPLITE,	"SOL_UDPLITE"	},
#endif
#if defined(SOL_RAW)
	{ SOL_RAW,	"SOL_RAW"	},
#endif
#if defined(SOL_IPX)
	{ SOL_IPX,	"SOL_IPX"	},
#endif
#if defined(SOL_AX25)
	{ SOL_AX25,	"SOL_AX25"	},
#endif
#if defined(SOL_ATALK)
	{ SOL_ATALK,	"SOL_ATALK"	},
#endif
#if defined(SOL_NETROM)
	{ SOL_NETROM,	"SOL_NETROM"	},
#endif
#if defined(SOL_ROSE)
	{ SOL_ROSE,	"SOL_ROSE"	},
#endif
#if defined(SOL_DECNET)
	{ SOL_DECNET,	"SOL_DECNET"	},
#endif
#if defined(SOL_X25)
	{ SOL_X25,	"SOL_X25"	},
#endif
#if defined(SOL_PACKET)
	{ SOL_PACKET,	"SOL_PACKET"	},
#endif
#if defined(SOL_ATM)
	{ SOL_ATM,	"SOL_ATM"	},
#endif
#if defined(SOL_AAL)
	{ SOL_AAL,	"SOL_AAL"	},
#endif
#if defined(SOL_IRDA)
	{ SOL_IRDA,	"SOL_IRDA"	},
#endif
#if defined(SOL_NETBEUI)
	{ SOL_NETBEUI,	"SOL_NETBEUI"	},
#endif
#if defined(SOL_LLC)
	{ SOL_LLC,	"SOL_LLC"	},
#endif
#if defined(SOL_DCCP)
	{ SOL_DCCP,	"SOL_DCCP"	},
#endif
#if defined(SOL_NETLINK)
	{ SOL_NETLINK,	"SOL_NETLINK"	},
#endif
#if defined(SOL_TIPC)
	{ SOL_TIPC,	"SOL_TIPC"	},
#endif
#if defined(SOL_RXRPC)
	{ SOL_RXRPC,	"SOL_RXRPC"	},
#endif
#if defined(SOL_PPPOL2TP)
	{ SOL_PPPOL2TP,	"SOL_PPPOL2TP"	},
#endif
#if defined(SOL_BLUETOOTH)
	{ SOL_BLUETOOTH,"SOL_BLUETOOTH" },
#endif
#if defined(SOL_PNPIPE)
	{ SOL_PNPIPE,	"SOL_PNPIPE"	},
#endif
#if defined(SOL_RDS)
	{ SOL_RDS,	"SOL_RDS"	},
#endif
#if defined(SOL_IUVC)
	{ SOL_IUCV,	"SOL_IUCV"	},
#endif
#if defined(SOL_CAIF)
	{ SOL_CAIF,	"SOL_CAIF"	},
#endif
	{ SOL_SOCKET,	"SOL_SOCKET"	},	/* Never used! */
	/* The SOL_* array should remain not NULL-terminated. */
};
/*** WARNING: DANGER WILL ROBINSON: NOTE "socketlayers" array above
     falls into "protocols" array below!!!!   This is intended!!! ***/
static const struct xlat protocols[] = {
	{ IPPROTO_IP,	"IPPROTO_IP"	},
	{ IPPROTO_ICMP,	"IPPROTO_ICMP"	},
	{ IPPROTO_TCP,	"IPPROTO_TCP"	},
	{ IPPROTO_UDP,	"IPPROTO_UDP"	},
#ifdef IPPROTO_IGMP
	{ IPPROTO_IGMP,	"IPPROTO_IGMP"	},
#endif
#ifdef IPPROTO_GGP
	{ IPPROTO_GGP,	"IPPROTO_GGP"	},
#endif
#ifdef IPPROTO_IPIP
	{ IPPROTO_IPIP,	"IPPROTO_IPIP"	},
#endif
#ifdef IPPROTO_EGP
	{ IPPROTO_EGP,	"IPPROTO_EGP"	},
#endif
#ifdef IPPROTO_PUP
	{ IPPROTO_PUP,	"IPPROTO_PUP"	},
#endif
#ifdef IPPROTO_IDP
	{ IPPROTO_IDP,	"IPPROTO_IDP"	},
#endif
#ifdef IPPROTO_TP
	{ IPPROTO_TP,	"IPPROTO_TP"	},
#endif
#ifdef IPPROTO_DCCP
	{ IPPROTO_DCCP,	"IPPROTO_DCCP"	},
#endif
#ifdef IPPROTO_IPV6
	{ IPPROTO_IPV6,	"IPPROTO_IPV6"	},
#endif
#ifdef IPPROTO_ROUTING
	{ IPPROTO_ROUTING, "IPPROTO_ROUTING" },
#endif
#ifdef IPPROTO_FRAGMENT
	{ IPPROTO_FRAGMENT, "IPPROTO_FRAGMENT" },
#endif
#ifdef IPPROTO_RSVP
	{ IPPROTO_RSVP,	"IPPROTO_RSVP"	},
#endif
#ifdef IPPROTO_GRE
	{ IPPROTO_GRE,	"IPPROTO_GRE"	},
#endif
#ifdef IPPROTO_ESP
	{ IPPROTO_ESP,	"IPPROTO_ESP"	},
#endif
#ifdef IPPROTO_AH
	{ IPPROTO_AH,	"IPPROTO_AH"	},
#endif
#ifdef IPPROTO_ICMPV6
	{ IPPROTO_ICMPV6, "IPPROTO_ICMPV6" },
#endif
#ifdef IPPROTO_NONE
	{ IPPROTO_NONE,	"IPPROTO_NONE"	},
#endif
#ifdef IPPROTO_DSTOPTS
	{ IPPROTO_DSTOPTS, "IPPROTO_DSTOPTS" },
#endif
#ifdef IPPROTO_HELLO
	{ IPPROTO_HELLO, "IPPROTO_HELLO" },
#endif
#ifdef IPPROTO_ND
	{ IPPROTO_ND,	"IPPROTO_ND"	},
#endif
#ifdef IPPROTO_MTP
	{ IPPROTO_MTP,	"IPPROTO_MTP"	},
#endif
#ifdef IPPROTO_ENCAP
	{ IPPROTO_ENCAP, "IPPROTO_ENCAP" },
#endif
#ifdef IPPROTO_PIM
	{ IPPROTO_PIM,	"IPPROTO_PIM"	},
#endif
#ifdef IPPROTO_COMP
	{ IPPROTO_COMP,	"IPPROTO_COMP"	},
#endif
#ifdef IPPROTO_SCTP
	{ IPPROTO_SCTP,	"IPPROTO_SCTP"	},
#endif
#ifdef IPPROTO_UDPLITE
	{ IPPROTO_UDPLITE, "IPPROTO_UDPLITE" },
#endif
#ifdef IPPROTO_RAW
	{ IPPROTO_RAW,	"IPPROTO_RAW"	},
#endif
#ifdef IPPROTO_MAX
	{ IPPROTO_MAX,	"IPPROTO_MAX"	},
#endif
	{ 0,		NULL		},
};
static const struct xlat msg_flags[] = {
	{ MSG_OOB,		"MSG_OOB"		},
#ifdef MSG_DONTROUTE
	{ MSG_DONTROUTE,	"MSG_DONTROUTE"		},
#endif
#ifdef MSG_PEEK
	{ MSG_PEEK,		"MSG_PEEK"		},
#endif
#ifdef MSG_CTRUNC
	{ MSG_CTRUNC,		"MSG_CTRUNC"		},
#endif
#ifdef MSG_PROXY
	{ MSG_PROXY,		"MSG_PROXY"		},
#endif
#ifdef MSG_EOR
	{ MSG_EOR,		"MSG_EOR"		},
#endif
#ifdef MSG_WAITALL
	{ MSG_WAITALL,		"MSG_WAITALL"		},
#endif
#ifdef MSG_TRUNC
	{ MSG_TRUNC,		"MSG_TRUNC"		},
#endif
#ifdef MSG_CTRUNC
	{ MSG_CTRUNC,		"MSG_CTRUNC"		},
#endif
#ifdef MSG_ERRQUEUE
	{ MSG_ERRQUEUE,		"MSG_ERRQUEUE"		},
#endif
#ifdef MSG_DONTWAIT
	{ MSG_DONTWAIT,		"MSG_DONTWAIT"		},
#endif
#ifdef MSG_CONFIRM
	{ MSG_CONFIRM,		"MSG_CONFIRM"		},
#endif
#ifdef MSG_PROBE
	{ MSG_PROBE,		"MSG_PROBE"		},
#endif
#ifdef MSG_FIN
	{ MSG_FIN,		"MSG_FIN"		},
#endif
#ifdef MSG_SYN
	{ MSG_SYN,		"MSG_SYN"		},
#endif
#ifdef MSG_RST
	{ MSG_RST,		"MSG_RST"		},
#endif
#ifdef MSG_NOSIGNAL
	{ MSG_NOSIGNAL,		"MSG_NOSIGNAL"		},
#endif
#ifdef MSG_MORE
	{ MSG_MORE,		"MSG_MORE"		},
#endif
#ifdef MSG_CMSG_CLOEXEC
	{ MSG_CMSG_CLOEXEC,	"MSG_CMSG_CLOEXEC"	},
#endif
	{ 0,			NULL			},
};

static const struct xlat sockoptions[] = {
#ifdef SO_ACCEPTCONN
	{ SO_ACCEPTCONN,	"SO_ACCEPTCONN"	},
#endif
#ifdef SO_ALLRAW
	{ SO_ALLRAW,	"SO_ALLRAW"	},
#endif
#ifdef SO_ATTACH_FILTER
	{ SO_ATTACH_FILTER,	"SO_ATTACH_FILTER"	},
#endif
#ifdef SO_BINDTODEVICE
	{ SO_BINDTODEVICE,	"SO_BINDTODEVICE"	},
#endif
#ifdef SO_BROADCAST
	{ SO_BROADCAST,	"SO_BROADCAST"	},
#endif
#ifdef SO_BSDCOMPAT
	{ SO_BSDCOMPAT,	"SO_BSDCOMPAT"	},
#endif
#ifdef SO_DEBUG
	{ SO_DEBUG,	"SO_DEBUG"	},
#endif
#ifdef SO_DETACH_FILTER
	{ SO_DETACH_FILTER,	"SO_DETACH_FILTER"	},
#endif
#ifdef SO_DONTROUTE
	{ SO_DONTROUTE,	"SO_DONTROUTE"	},
#endif
#ifdef SO_ERROR
	{ SO_ERROR,	"SO_ERROR"	},
#endif
#ifdef SO_ICS
	{ SO_ICS,	"SO_ICS"	},
#endif
#ifdef SO_IMASOCKET
	{ SO_IMASOCKET,	"SO_IMASOCKET"	},
#endif
#ifdef SO_KEEPALIVE
	{ SO_KEEPALIVE,	"SO_KEEPALIVE"	},
#endif
#ifdef SO_LINGER
	{ SO_LINGER,	"SO_LINGER"	},
#endif
#ifdef SO_LISTENING
	{ SO_LISTENING,	"SO_LISTENING"	},
#endif
#ifdef SO_MGMT
	{ SO_MGMT,	"SO_MGMT"	},
#endif
#ifdef SO_NO_CHECK
	{ SO_NO_CHECK,	"SO_NO_CHECK"	},
#endif
#ifdef SO_OOBINLINE
	{ SO_OOBINLINE,	"SO_OOBINLINE"	},
#endif
#ifdef SO_ORDREL
	{ SO_ORDREL,	"SO_ORDREL"	},
#endif
#ifdef SO_PARALLELSVR
	{ SO_PARALLELSVR,	"SO_PARALLELSVR"	},
#endif
#ifdef SO_PASSCRED
	{ SO_PASSCRED,	"SO_PASSCRED"	},
#endif
#ifdef SO_PEERCRED
	{ SO_PEERCRED,	"SO_PEERCRED"	},
#endif
#ifdef SO_PEERNAME
	{ SO_PEERNAME,	"SO_PEERNAME"	},
#endif
#ifdef SO_PEERSEC
	{ SO_PEERSEC,	"SO_PEERSEC"	},
#endif
#ifdef SO_PRIORITY
	{ SO_PRIORITY,	"SO_PRIORITY"	},
#endif
#ifdef SO_PROTOTYPE
	{ SO_PROTOTYPE,	"SO_PROTOTYPE"	},
#endif
#ifdef SO_RCVBUF
	{ SO_RCVBUF,	"SO_RCVBUF"	},
#endif
#ifdef SO_RCVLOWAT
	{ SO_RCVLOWAT,	"SO_RCVLOWAT"	},
#endif
#ifdef SO_RCVTIMEO
	{ SO_RCVTIMEO,	"SO_RCVTIMEO"	},
#endif
#ifdef SO_RDWR
	{ SO_RDWR,	"SO_RDWR"	},
#endif
#ifdef SO_REUSEADDR
	{ SO_REUSEADDR,	"SO_REUSEADDR"	},
#endif
#ifdef SO_REUSEPORT
	{ SO_REUSEPORT,	"SO_REUSEPORT"	},
#endif
#ifdef SO_SECURITY_AUTHENTICATION
	{ SO_SECURITY_AUTHENTICATION,"SO_SECURITY_AUTHENTICATION"},
#endif
#ifdef SO_SECURITY_ENCRYPTION_NETWORK
	{ SO_SECURITY_ENCRYPTION_NETWORK,"SO_SECURITY_ENCRYPTION_NETWORK"},
#endif
#ifdef SO_SECURITY_ENCRYPTION_TRANSPORT
	{ SO_SECURITY_ENCRYPTION_TRANSPORT,"SO_SECURITY_ENCRYPTION_TRANSPORT"},
#endif
#ifdef SO_SEMA
	{ SO_SEMA,	"SO_SEMA"	},
#endif
#ifdef SO_SNDBUF
	{ SO_SNDBUF,	"SO_SNDBUF"	},
#endif
#ifdef SO_SNDLOWAT
	{ SO_SNDLOWAT,	"SO_SNDLOWAT"	},
#endif
#ifdef SO_SNDTIMEO
	{ SO_SNDTIMEO,	"SO_SNDTIMEO"	},
#endif
#ifdef SO_TIMESTAMP
	{ SO_TIMESTAMP,	"SO_TIMESTAMP"	},
#endif
#ifdef SO_TYPE
	{ SO_TYPE,	"SO_TYPE"	},
#endif
#ifdef SO_USELOOPBACK
	{ SO_USELOOPBACK,	"SO_USELOOPBACK"	},
#endif
	{ 0,		NULL		},
};

#if !defined (SOL_IP) && defined (IPPROTO_IP)
#define SOL_IP IPPROTO_IP
#endif

#ifdef SOL_IP
static const struct xlat sockipoptions[] = {
#ifdef IP_TOS
	{ IP_TOS,		"IP_TOS"		},
#endif
#ifdef IP_TTL
	{ IP_TTL,		"IP_TTL"		},
#endif
#ifdef IP_HDRINCL
	{ IP_HDRINCL,		"IP_HDRINCL"		},
#endif
#ifdef IP_OPTIONS
	{ IP_OPTIONS,		"IP_OPTIONS"		},
#endif
#ifdef IP_ROUTER_ALERT
	{ IP_ROUTER_ALERT,	"IP_ROUTER_ALERT"	},
#endif
#ifdef IP_RECVOPTIONS
	{ IP_RECVOPTIONS,	"IP_RECVOPTIONS"	},
#endif
#ifdef IP_RECVOPTS
	{ IP_RECVOPTS,		"IP_RECVOPTS"		},
#endif
#ifdef IP_RECVRETOPTS
	{ IP_RECVRETOPTS,	"IP_RECVRETOPTS"	},
#endif
#ifdef IP_RECVDSTADDR
	{ IP_RECVDSTADDR,	"IP_RECVDSTADDR"	},
#endif
#ifdef IP_RETOPTS
	{ IP_RETOPTS,		"IP_RETOPTS"		},
#endif
#ifdef IP_PKTINFO
	{ IP_PKTINFO,		"IP_PKTINFO"		},
#endif
#ifdef IP_PKTOPTIONS
	{ IP_PKTOPTIONS,	"IP_PKTOPTIONS"		},
#endif
#ifdef IP_MTU_DISCOVER
	{ IP_MTU_DISCOVER,	"IP_MTU_DISCOVER"	},
#endif
#ifdef IP_RECVERR
	{ IP_RECVERR,		"IP_RECVERR"		},
#endif
#ifdef IP_RECVTTL
	{ IP_RECVTTL,		"IP_RECVTTL"		},
#endif
#ifdef IP_RECVTOS
	{ IP_RECVTOS,		"IP_RECVTOS"		},
#endif
#ifdef IP_MTU
	{ IP_MTU,		"IP_MTU"		},
#endif
#ifdef IP_MULTICAST_IF
	{ IP_MULTICAST_IF,	"IP_MULTICAST_IF"	},
#endif
#ifdef IP_MULTICAST_TTL
	{ IP_MULTICAST_TTL,	"IP_MULTICAST_TTL"	},
#endif
#ifdef IP_MULTICAST_LOOP
	{ IP_MULTICAST_LOOP,	"IP_MULTICAST_LOOP"	},
#endif
#ifdef IP_ADD_MEMBERSHIP
	{ IP_ADD_MEMBERSHIP,	"IP_ADD_MEMBERSHIP"	},
#endif
#ifdef IP_DROP_MEMBERSHIP
	{ IP_DROP_MEMBERSHIP,	"IP_DROP_MEMBERSHIP"	},
#endif
#ifdef IP_BROADCAST_IF
	{ IP_BROADCAST_IF,	"IP_BROADCAST_IF"	},
#endif
#ifdef IP_RECVIFINDEX
	{ IP_RECVIFINDEX,	"IP_RECVIFINDEX"	},
#endif
#ifdef IP_MSFILTER
	{ IP_MSFILTER,		"IP_MSFILTER"		},
#endif
#ifdef MCAST_MSFILTER
	{ MCAST_MSFILTER,	"MCAST_MSFILTER"	},
#endif
#ifdef IP_FREEBIND
	{ IP_FREEBIND,		"IP_FREEBIND"		},
#endif
	{ 0,			NULL			},
};
#endif /* SOL_IP */

#ifdef SOL_IPV6
static const struct xlat sockipv6options[] = {
#ifdef IPV6_ADDRFORM
	{ IPV6_ADDRFORM,	"IPV6_ADDRFORM"		},
#endif
#ifdef MCAST_FILTER
	{ MCAST_FILTER,		"MCAST_FILTER"		},
#endif
#ifdef IPV6_PKTOPTIONS
	{ IPV6_PKTOPTIONS,	"IPV6_PKTOPTIONS"	},
#endif
#ifdef IPV6_MTU
	{ IPV6_MTU,		"IPV6_MTU"		},
#endif
#ifdef IPV6_V6ONLY
	{ IPV6_V6ONLY,		"IPV6_V6ONLY"		},
#endif
#ifdef IPV6_PKTINFO
	{ IPV6_PKTINFO,		"IPV6_PKTINFO"		},
#endif
#ifdef IPV6_HOPLIMIT
	{ IPV6_HOPLIMIT,	"IPV6_HOPLIMIT"		},
#endif
#ifdef IPV6_RTHDR
	{ IPV6_RTHDR,		"IPV6_RTHDR"		},
#endif
#ifdef IPV6_HOPOPTS
	{ IPV6_HOPOPTS,		"IPV6_HOPOPTS"		},
#endif
#ifdef IPV6_DSTOPTS
	{ IPV6_DSTOPTS,		"IPV6_DSTOPTS"		},
#endif
#ifdef IPV6_FLOWINFO
	{ IPV6_FLOWINFO,	"IPV6_FLOWINFO"		},
#endif
#ifdef IPV6_UNICAST_HOPS
	{ IPV6_UNICAST_HOPS,	"IPV6_UNICAST_HOPS"	},
#endif
#ifdef IPV6_MULTICAST_HOPS
	{ IPV6_MULTICAST_HOPS,	"IPV6_MULTICAST_HOPS"	},
#endif
#ifdef IPV6_MULTICAST_LOOP
	{ IPV6_MULTICAST_LOOP,	"IPV6_MULTICAST_LOOP"	},
#endif
#ifdef IPV6_MULTICAST_IF
	{ IPV6_MULTICAST_IF,	"IPV6_MULTICAST_IF"	},
#endif
#ifdef IPV6_MTU_DISCOVER
	{ IPV6_MTU_DISCOVER,	"IPV6_MTU_DISCOVER"	},
#endif
#ifdef IPV6_RECVERR
	{ IPV6_RECVERR,		"IPV6_RECVERR"		},
#endif
#ifdef IPV6_FLOWINFO_SEND
	{ IPV6_FLOWINFO_SEND,	"IPV6_FLOWINFO_SEND"	},
#endif
#ifdef IPV6_ADD_MEMBERSHIP
	{ IPV6_ADD_MEMBERSHIP,	"IPV6_ADD_MEMBERSHIP"	},
#endif
#ifdef IPV6_DROP_MEMBERSHIP
	{ IPV6_DROP_MEMBERSHIP,	"IPV6_DROP_MEMBERSHIP"	},
#endif
#ifdef IPV6_ROUTER_ALERT
	{ IPV6_ROUTER_ALERT,	"IPV6_ROUTER_ALERT"	},
#endif
	{ 0,			NULL			},
};
#endif /* SOL_IPV6 */

#ifdef SOL_IPX
static const struct xlat sockipxoptions[] = {
	{ IPX_TYPE,     "IPX_TYPE"      },
	{ 0,            NULL            },
};
#endif /* SOL_IPX */

#ifdef SOL_RAW
static const struct xlat sockrawoptions[] = {
#if defined(ICMP_FILTER)
	{ ICMP_FILTER,		"ICMP_FILTER"	},
#endif
	{ 0,			NULL		},
};
#endif /* SOL_RAW */

#ifdef SOL_PACKET
static const struct xlat sockpacketoptions[] = {
#ifdef PACKET_ADD_MEMBERSHIP
	{ PACKET_ADD_MEMBERSHIP,	"PACKET_ADD_MEMBERSHIP"	},
#endif
#ifdef PACKET_DROP_MEMBERSHIP
	{ PACKET_DROP_MEMBERSHIP,	"PACKET_DROP_MEMBERSHIP"},
#endif
#if defined(PACKET_RECV_OUTPUT)
	{ PACKET_RECV_OUTPUT,		"PACKET_RECV_OUTPUT"	},
#endif
#if defined(PACKET_RX_RING)
	{ PACKET_RX_RING,		"PACKET_RX_RING"	},
#endif
#if defined(PACKET_STATISTICS)
	{ PACKET_STATISTICS,		"PACKET_STATISTICS"	},
#endif
#if defined(PACKET_COPY_THRESH)
	{ PACKET_COPY_THRESH,		"PACKET_COPY_THRESH"	},
#endif
#if defined(PACKET_AUXDATA)
	{ PACKET_AUXDATA,		"PACKET_AUXDATA"	},
#endif
#if defined(PACKET_ORIGDEV)
	{ PACKET_ORIGDEV,		"PACKET_ORIGDEV"	},
#endif
#if defined(PACKET_VERSION)
	{ PACKET_VERSION,		"PACKET_VERSION"	},
#endif
#if defined(PACKET_HDRLEN)
	{ PACKET_HDRLEN,		"PACKET_HDRLEN"	},
#endif
#if defined(PACKET_RESERVE)
	{ PACKET_RESERVE,		"PACKET_RESERVE"	},
#endif
#if defined(PACKET_TX_RING)
	{ PACKET_TX_RING,		"PACKET_TX_RING"	},
#endif
#if defined(PACKET_LOSS)
	{ PACKET_LOSS,			"PACKET_LOSS"	},
#endif
	{ 0,				NULL			},
};
#endif /* SOL_PACKET */

#ifdef SOL_SCTP
static const struct xlat socksctpoptions[] = {
#if defined(SCTP_RTOINFO)
	{ SCTP_RTOINFO,			"SCTP_RTOINFO"	},
#endif
#if defined(SCTP_ASSOCINFO)
	{ SCTP_ASSOCINFO,		"SCTP_ASSOCINFO"},
#endif
#if defined(SCTP_INITMSG)
	{ SCTP_INITMSG,			"SCTP_INITMSG"	},
#endif
#if defined(SCTP_NODELAY)
	{ SCTP_NODELAY,			"SCTP_NODELAY"	},
#endif
#if defined(SCTP_AUTOCLOSE)
	{ SCTP_AUTOCLOSE,		"SCTP_AUTOCLOSE"},
#endif
#if defined(SCTP_SET_PEER_PRIMARY_ADDR)
	{ SCTP_SET_PEER_PRIMARY_ADDR,	"SCTP_SET_PEER_PRIMARY_ADDR"},
#endif
#if defined(SCTP_PRIMARY_ADDR)
	{ SCTP_PRIMARY_ADDR,		"SCTP_PRIMARY_ADDR"	},
#endif
#if defined(SCTP_ADAPTATION_LAYER)
	{ SCTP_ADAPTATION_LAYER,	"SCTP_ADAPTATION_LAYER"	},
#endif
#if defined(SCTP_DISABLE_FRAGMENTS)
	{ SCTP_DISABLE_FRAGMENTS,	"SCTP_DISABLE_FRAGMENTS"},
#endif
#if defined(SCTP_PEER_ADDR_PARAMS)
	{ SCTP_PEER_ADDR_PARAMS,	"SCTP_PEER_ADDR_PARAMS"	},
#endif
#if defined(SCTP_DEFAULT_SEND_PARAM)
	{ SCTP_DEFAULT_SEND_PARAM,	"SCTP_DEFAULT_SEND_PARAM"},
#endif
#if defined(SCTP_EVENTS)
	{ SCTP_EVENTS,			"SCTP_EVENTS"		},
#endif
#if defined(SCTP_I_WANT_MAPPED_V4_ADDR)
	{ SCTP_I_WANT_MAPPED_V4_ADDR,	"SCTP_I_WANT_MAPPED_V4_ADDR"},
#endif
#if defined(SCTP_MAXSEG)
	{ SCTP_MAXSEG,			"SCTP_MAXSEG"		},
#endif
#if defined(SCTP_STATUS)
	{ SCTP_STATUS,			"SCTP_STATUS"		},
#endif
#if defined(SCTP_GET_PEER_ADDR_INFO)
	{ SCTP_GET_PEER_ADDR_INFO,	"SCTP_GET_PEER_ADDR_INFO"},
#endif
#if defined(SCTP_DELAYED_ACK)
	{ SCTP_DELAYED_ACK,		"SCTP_DELAYED_ACK"	},
#endif
#if defined(SCTP_CONTEXT)
	{ SCTP_CONTEXT,			"SCTP_CONTEXT"		},
#endif
#if defined(SCTP_FRAGMENT_INTERLEAVE)
	{ SCTP_FRAGMENT_INTERLEAVE,	"SCTP_FRAGMENT_INTERLEAVE"},
#endif
#if defined(SCTP_PARTIAL_DELIVERY_POINT)
	{ SCTP_PARTIAL_DELIVERY_POINT,	"SCTP_PARTIAL_DELIVERY_POINT"},
#endif
#if defined(SCTP_MAX_BURST)
	{ SCTP_MAX_BURST,		"SCTP_MAX_BURST"	},
#endif
#if defined(SCTP_AUTH_CHUNK)
	{ SCTP_AUTH_CHUNK,		"SCTP_AUTH_CHUNK"	},
#endif
#if defined(SCTP_HMAC_IDENT)
	{ SCTP_HMAC_IDENT,		"SCTP_HMAC_IDENT"	},
#endif
#if defined(SCTP_AUTH_KEY)
	{ SCTP_AUTH_KEY,		"SCTP_AUTH_KEY"		},
#endif
#if defined(SCTP_AUTH_ACTIVE_KEY)
	{ SCTP_AUTH_ACTIVE_KEY,		"SCTP_AUTH_ACTIVE_KEY"	},
#endif
#if defined(SCTP_AUTH_DELETE_KEY)
	{ SCTP_AUTH_DELETE_KEY,		"SCTP_AUTH_DELETE_KEY"	},
#endif
#if defined(SCTP_PEER_AUTH_CHUNKS)
	{ SCTP_PEER_AUTH_CHUNKS,	"SCTP_PEER_AUTH_CHUNKS"	},
#endif
#if defined(SCTP_LOCAL_AUTH_CHUNKS)
	{ SCTP_LOCAL_AUTH_CHUNKS,	"SCTP_LOCAL_AUTH_CHUNKS"},
#endif
#if defined(SCTP_GET_ASSOC_NUMBER)
	{ SCTP_GET_ASSOC_NUMBER,	"SCTP_GET_ASSOC_NUMBER"	},
#endif

	/* linux specific things */
#if defined(SCTP_SOCKOPT_BINDX_ADD)
	{ SCTP_SOCKOPT_BINDX_ADD,	"SCTP_SOCKOPT_BINDX_ADD"	},
#endif
#if defined(SCTP_SOCKOPT_BINDX_REM)
	{ SCTP_SOCKOPT_BINDX_REM,	"SCTP_SOCKOPT_BINDX_REM"	},
#endif
#if defined(SCTP_SOCKOPT_PEELOFF)
	{ SCTP_SOCKOPT_PEELOFF,		"SCTP_SOCKOPT_PEELOFF"		},
#endif
#if defined(SCTP_GET_PEER_ADDRS_NUM_OLD)
	{ SCTP_GET_PEER_ADDRS_NUM_OLD,	"SCTP_GET_PEER_ADDRS_NUM_OLD"	},
#endif
#if defined(SCTP_GET_PEER_ADDRS_OLD)
	{ SCTP_GET_PEER_ADDRS_OLD,	"SCTP_GET_PEER_ADDRS_OLD"	},
#endif
#if defined(SCTP_GET_LOCAL_ADDRS_NUM_OLD)
	{ SCTP_GET_LOCAL_ADDRS_NUM_OLD,	"SCTP_GET_LOCAL_ADDRS_NUM_OLD"	},
#endif
#if defined(SCTP_GET_LOCAL_ADDRS_OLD)
	{ SCTP_GET_LOCAL_ADDRS_OLD,	"SCTP_GET_LOCAL_ADDRS_OLD"	},
#endif
#if defined(SCTP_SOCKOPT_CONNECTX_OLD)
	{ SCTP_SOCKOPT_CONNECTX_OLD,	"SCTP_SOCKOPT_CONNECTX_OLD"	},
#endif
#if defined(SCTP_GET_PEER_ADDRS)
	{ SCTP_GET_PEER_ADDRS,		"SCTP_GET_PEER_ADDRS"		},
#endif
#if defined(SCTP_GET_LOCAL_ADDRS)
	{ SCTP_GET_LOCAL_ADDRS,		"SCTP_GET_LOCAL_ADDRS"		},
#endif

	{ 0,	NULL	},
};
#endif

#if  !defined (SOL_TCP) && defined (IPPROTO_TCP)
#define SOL_TCP IPPROTO_TCP
#endif

#ifdef SOL_TCP
static const struct xlat socktcpoptions[] = {
	{ TCP_NODELAY,		"TCP_NODELAY"	},
	{ TCP_MAXSEG,		"TCP_MAXSEG"	},
#if defined(TCP_CORK)
	{ TCP_CORK,		"TCP_CORK"	},
#endif
#if defined(TCP_KEEPIDLE)
	{ TCP_KEEPIDLE,		"TCP_KEEPIDLE" },
#endif
#if defined(TCP_KEEPINTVL)
	{ TCP_KEEPINTVL,	"TCP_KEEPINTVL" },
#endif
#if defined(TCP_KEEPCNT)
	{ TCP_KEEPCNT,		"TCP_KEEPCNT" },
#endif
#if defined(TCP_NKEEP)
	{ TCP_NKEEP,		"TCP_NKEEP"	},
#endif
#if defined(TCP_SYNCNT)
	{ TCP_SYNCNT,		"TCP_SYNCNT" },
#endif
#if defined(TCP_LINGER2)
	{ TCP_LINGER2,		"TCP_LINGER2" },
#endif
#if defined(TCP_DEFER_ACCEPT)
	{ TCP_DEFER_ACCEPT,	"TCP_DEFER_ACCEPT" },
#endif
#if defined(TCP_WINDOW_CLAMP)
	{ TCP_WINDOW_CLAMP,	"TCP_WINDOW_CLAMP" },
#endif
#if defined(TCP_INFO)
	{ TCP_INFO,		"TCP_INFO" },
#endif
#if defined(TCP_QUICKACK)
	{ TCP_QUICKACK,		"TCP_QUICKACK" },
#endif
	{ 0,			NULL		},
};
#endif /* SOL_TCP */

#ifdef SOL_RAW
static const struct xlat icmpfilterflags[] = {
#if defined(ICMP_ECHOREPLY)
	{ (1<<ICMP_ECHOREPLY),		"ICMP_ECHOREPLY"	},
#endif
#if defined(ICMP_DEST_UNREACH)
	{ (1<<ICMP_DEST_UNREACH),	"ICMP_DEST_UNREACH"	},
#endif
#if defined(ICMP_SOURCE_QUENCH)
	{ (1<<ICMP_SOURCE_QUENCH),	"ICMP_SOURCE_QUENCH"	},
#endif
#if defined(ICMP_REDIRECT)
	{ (1<<ICMP_REDIRECT),		"ICMP_REDIRECT"		},
#endif
#if defined(ICMP_ECHO)
	{ (1<<ICMP_ECHO),		"ICMP_ECHO"		},
#endif
#if defined(ICMP_TIME_EXCEEDED)
	{ (1<<ICMP_TIME_EXCEEDED),	"ICMP_TIME_EXCEEDED"	},
#endif
#if defined(ICMP_PARAMETERPROB)
	{ (1<<ICMP_PARAMETERPROB),	"ICMP_PARAMETERPROB"	},
#endif
#if defined(ICMP_TIMESTAMP)
	{ (1<<ICMP_TIMESTAMP),		"ICMP_TIMESTAMP"	},
#endif
#if defined(ICMP_TIMESTAMPREPLY)
	{ (1<<ICMP_TIMESTAMPREPLY),	"ICMP_TIMESTAMPREPLY"	},
#endif
#if defined(ICMP_INFO_REQUEST)
	{ (1<<ICMP_INFO_REQUEST),	"ICMP_INFO_REQUEST"	},
#endif
#if defined(ICMP_INFO_REPLY)
	{ (1<<ICMP_INFO_REPLY),		"ICMP_INFO_REPLY"	},
#endif
#if defined(ICMP_ADDRESS)
	{ (1<<ICMP_ADDRESS),		"ICMP_ADDRESS"		},
#endif
#if defined(ICMP_ADDRESSREPLY)
	{ (1<<ICMP_ADDRESSREPLY),	"ICMP_ADDRESSREPLY"	},
#endif
	{ 0,				NULL			},
};
#endif /* SOL_RAW */

#if defined(AF_PACKET) /* from e.g. linux/if_packet.h */
static const struct xlat af_packet_types[] = {
#if defined(PACKET_HOST)
	{ PACKET_HOST,			"PACKET_HOST"		},
#endif
#if defined(PACKET_BROADCAST)
	{ PACKET_BROADCAST,		"PACKET_BROADCAST"	},
#endif
#if defined(PACKET_MULTICAST)
	{ PACKET_MULTICAST,		"PACKET_MULTICAST"	},
#endif
#if defined(PACKET_OTHERHOST)
	{ PACKET_OTHERHOST,		"PACKET_OTHERHOST"	},
#endif
#if defined(PACKET_OUTGOING)
	{ PACKET_OUTGOING,		"PACKET_OUTGOING"	},
#endif
#if defined(PACKET_LOOPBACK)
	{ PACKET_LOOPBACK,		"PACKET_LOOPBACK"	},
#endif
#if defined(PACKET_FASTROUTE)
	{ PACKET_FASTROUTE,		"PACKET_FASTROUTE"	},
#endif
	{ 0,				NULL			},
};
#endif /* defined(AF_PACKET) */


void
printsock(struct tcb *tcp, long addr, int addrlen)
{
	union {
		char pad[128];
		struct sockaddr sa;
		struct sockaddr_in sin;
		struct sockaddr_un sau;
#ifdef HAVE_INET_NTOP
		struct sockaddr_in6 sa6;
#endif
#if defined(LINUX) && defined(AF_IPX)
		struct sockaddr_ipx sipx;
#endif
#ifdef AF_PACKET
		struct sockaddr_ll ll;
#endif
#ifdef AF_NETLINK
		struct sockaddr_nl nl;
#endif
	} addrbuf;
	char string_addr[100];

	if (addr == 0) {
		tprintf("NULL");
		return;
	}
	if (!verbose(tcp)) {
		tprintf("%#lx", addr);
		return;
	}

	if (addrlen < 2 || addrlen > sizeof(addrbuf))
		addrlen = sizeof(addrbuf);

	memset(&addrbuf, 0, sizeof(addrbuf));
	if (umoven(tcp, addr, addrlen, addrbuf.pad) < 0) {
		tprintf("{...}");
		return;
	}
	addrbuf.pad[sizeof(addrbuf.pad) - 1] = '\0';

	tprintf("{sa_family=");
	printxval(addrfams, addrbuf.sa.sa_family, "AF_???");
	tprintf(", ");

	switch (addrbuf.sa.sa_family) {
	case AF_UNIX:
		if (addrlen == 2) {
			tprintf("NULL");
		} else if (addrbuf.sau.sun_path[0]) {
			tprintf("path=");
			printpathn(tcp, addr + 2, strlen(addrbuf.sau.sun_path));
		} else {
			tprintf("path=@");
			printpathn(tcp, addr + 3, strlen(addrbuf.sau.sun_path + 1));
		}
		break;
	case AF_INET:
		tprintf("sin_port=htons(%u), sin_addr=inet_addr(\"%s\")",
			ntohs(addrbuf.sin.sin_port), inet_ntoa(addrbuf.sin.sin_addr));
		break;
#ifdef HAVE_INET_NTOP
	case AF_INET6:
		inet_ntop(AF_INET6, &addrbuf.sa6.sin6_addr, string_addr, sizeof(string_addr));
		tprintf("sin6_port=htons(%u), inet_pton(AF_INET6, \"%s\", &sin6_addr), sin6_flowinfo=%u",
				ntohs(addrbuf.sa6.sin6_port), string_addr,
				addrbuf.sa6.sin6_flowinfo);
#ifdef HAVE_STRUCT_SOCKADDR_IN6_SIN6_SCOPE_ID
		{
#if defined(HAVE_IF_INDEXTONAME) && defined(IN6_IS_ADDR_LINKLOCAL) && defined(IN6_IS_ADDR_MC_LINKLOCAL)
		    int numericscope = 0;
		    if (IN6_IS_ADDR_LINKLOCAL (&addrbuf.sa6.sin6_addr)
			    || IN6_IS_ADDR_MC_LINKLOCAL (&addrbuf.sa6.sin6_addr)) {
			char scopebuf[IFNAMSIZ + 1];

			if (if_indextoname (addrbuf.sa6.sin6_scope_id, scopebuf) == NULL)
			    numericscope++;
			else
			    tprintf(", sin6_scope_id=if_nametoindex(\"%s\")", scopebuf);
		    } else
			numericscope++;

		    if (numericscope)
#endif
			tprintf(", sin6_scope_id=%u", addrbuf.sa6.sin6_scope_id);
		}
#endif
		    break;
#endif
#if defined(AF_IPX) && defined(linux)
	case AF_IPX:
		{
			int i;
			tprintf("sipx_port=htons(%u), ",
					ntohs(addrbuf.sipx.sipx_port));
			/* Yes, I know, this does not look too
			 * strace-ish, but otherwise the IPX
			 * addresses just look monstrous...
			 * Anyways, feel free if you don't like
			 * this way.. :)
			 */
			tprintf("%08lx:", (unsigned long)ntohl(addrbuf.sipx.sipx_network));
			for (i = 0; i<IPX_NODE_LEN; i++)
				tprintf("%02x", addrbuf.sipx.sipx_node[i]);
			tprintf("/[%02x]", addrbuf.sipx.sipx_type);
		}
		break;
#endif /* AF_IPX && linux */
#ifdef AF_PACKET
	case AF_PACKET:
		{
			int i;
			tprintf("proto=%#04x, if%d, pkttype=",
					ntohs(addrbuf.ll.sll_protocol),
					addrbuf.ll.sll_ifindex);
			printxval(af_packet_types, addrbuf.ll.sll_pkttype, "?");
			tprintf(", addr(%d)={%d, ",
					addrbuf.ll.sll_halen,
					addrbuf.ll.sll_hatype);
			for (i=0; i<addrbuf.ll.sll_halen; i++)
				tprintf("%02x", addrbuf.ll.sll_addr[i]);
		}
		break;

#endif /* AF_APACKET */
#ifdef AF_NETLINK
	case AF_NETLINK:
		tprintf("pid=%d, groups=%08x", addrbuf.nl.nl_pid, addrbuf.nl.nl_groups);
		break;
#endif /* AF_NETLINK */
	/* AF_AX25 AF_APPLETALK AF_NETROM AF_BRIDGE AF_AAL5
	AF_X25 AF_ROSE etc. still need to be done */

	default:
		tprintf("sa_data=");
		printstr(tcp, (long) &((struct sockaddr *) addr)->sa_data,
			sizeof addrbuf.sa.sa_data);
		break;
	}
	tprintf("}");
}

#if HAVE_SENDMSG
static const struct xlat scmvals[] = {
#ifdef SCM_RIGHTS
	{ SCM_RIGHTS,		"SCM_RIGHTS"		},
#endif
#ifdef SCM_CREDENTIALS
	{ SCM_CREDENTIALS,	"SCM_CREDENTIALS"	},
#endif
	{ 0,			NULL			}
};

static void
printcmsghdr(struct tcb *tcp, unsigned long addr, unsigned long len)
{
	struct cmsghdr *cmsg = len < sizeof(struct cmsghdr) ?
			       NULL : malloc(len);
	if (cmsg == NULL || umoven(tcp, addr, len, (char *) cmsg) < 0) {
		tprintf(", msg_control=%#lx", addr);
		free(cmsg);
		return;
	}

	tprintf(", {cmsg_len=%u, cmsg_level=", (unsigned) cmsg->cmsg_len);
	printxval(socketlayers, cmsg->cmsg_level, "SOL_???");
	tprintf(", cmsg_type=");

	if (cmsg->cmsg_level == SOL_SOCKET) {
		unsigned long cmsg_len;

		printxval(scmvals, cmsg->cmsg_type, "SCM_???");
		cmsg_len = (len < cmsg->cmsg_len) ? len : cmsg->cmsg_len;

		if (cmsg->cmsg_type == SCM_RIGHTS
		    && CMSG_LEN(sizeof(int)) <= cmsg_len) {
			int *fds = (int *) CMSG_DATA (cmsg);
			int first = 1;

			tprintf(", {");
			while ((char *) fds < ((char *) cmsg + cmsg_len)) {
				if (!first)
					tprintf(", ");
				tprintf("%d", *fds++);
				first = 0;
			}
			tprintf("}}");
			free(cmsg);
			return;
		}
		if (cmsg->cmsg_type == SCM_CREDENTIALS
		    && CMSG_LEN(sizeof(struct ucred)) <= cmsg_len) {
			struct ucred *uc = (struct ucred *) CMSG_DATA (cmsg);

			tprintf("{pid=%ld, uid=%ld, gid=%ld}}",
				(long)uc->pid, (long)uc->uid, (long)uc->gid);
			free(cmsg);
			return;
		}
	}
	free(cmsg);
	tprintf(", ...}");
}

static void
do_msghdr(struct tcb *tcp, struct msghdr *msg)
{
	tprintf("{msg_name(%d)=", msg->msg_namelen);
	printsock(tcp, (long)msg->msg_name, msg->msg_namelen);

	tprintf(", msg_iov(%lu)=", (unsigned long)msg->msg_iovlen);
	tprint_iov(tcp, (unsigned long)msg->msg_iovlen,
		   (unsigned long)msg->msg_iov);

#ifdef HAVE_STRUCT_MSGHDR_MSG_CONTROL
	tprintf(", msg_controllen=%lu", (unsigned long)msg->msg_controllen);
	if (msg->msg_controllen)
		printcmsghdr(tcp, (unsigned long) msg->msg_control,
			     msg->msg_controllen);
	tprintf(", msg_flags=");
	printflags(msg_flags, msg->msg_flags, "MSG_???");
#else /* !HAVE_STRUCT_MSGHDR_MSG_CONTROL */
	tprintf("msg_accrights=%#lx, msg_accrightslen=%u",
		(unsigned long) msg->msg_accrights, msg->msg_accrightslen);
#endif /* !HAVE_STRUCT_MSGHDR_MSG_CONTROL */
	tprintf("}");
}

static void
printmsghdr(tcp, addr)
struct tcb *tcp;
long addr;
{
	struct msghdr msg;

	if (umove(tcp, addr, &msg) < 0) {
		tprintf("%#lx", addr);
		return;
	}
	do_msghdr(tcp, &msg);
}

#ifdef LINUX
static void
printmmsghdr(struct tcb *tcp, long addr)
{
	struct mmsghdr {
		struct msghdr msg_hdr;
		unsigned msg_len;
	} mmsg;

	if (umove(tcp, addr, &mmsg) < 0) {
		tprintf("%#lx", addr);
		return;
	}
	tprintf("{");
	do_msghdr(tcp, &mmsg.msg_hdr);
	tprintf(", %u}", mmsg.msg_len);
}
#endif

#endif /* HAVE_SENDMSG */

/*
 * low bits of the socket type define real socket type,
 * other bits are socket type flags.
 */
static void
tprint_sock_type(struct tcb *tcp, int flags)
{
	const char *str = xlookup(socktypes, flags & SOCK_TYPE_MASK);

	if (str)
	{
		tprintf("%s", str);
		flags &= ~SOCK_TYPE_MASK;
		if (!flags)
			return;
		tprintf("|");
	}
	printflags(sock_type_flags, flags, "SOCK_???");
}

int
sys_socket(struct tcb *tcp)
{
	if (entering(tcp)) {
		printxval(domains, tcp->u_arg[0], "PF_???");
		tprintf(", ");
		tprint_sock_type(tcp, tcp->u_arg[1]);
		tprintf(", ");
		switch (tcp->u_arg[0]) {
		case PF_INET:
#ifdef PF_INET6
		case PF_INET6:
#endif
			printxval(protocols, tcp->u_arg[2], "IPPROTO_???");
			break;
#ifdef PF_IPX
		case PF_IPX:
			/* BTW: I don't believe this.. */
			tprintf("[");
			printxval(domains, tcp->u_arg[2], "PF_???");
			tprintf("]");
			break;
#endif /* PF_IPX */
		default:
			tprintf("%lu", tcp->u_arg[2]);
			break;
		}
	}
	return 0;
}

#ifdef SVR4
int
sys_so_socket(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		/* not sure really what these args are... but this
		 * is how truss prints it
		 */
		tprintf("%ld, %ld, %ld, ",
		  tcp->u_arg[0], tcp->u_arg[1], tcp->u_arg[2]);
		printpath(tcp, tcp->u_arg[3]);
		tprintf(", %ld", tcp->u_arg[4]);
	}
	return 0;
}

int
sys_so_socketpair(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		/* not sure what this arg is */
		tprintf("0x%lx", tcp->u_arg[0]);
	}
	return 0;
}
#endif /* SVR4 */

int
sys_bind(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
		printsock(tcp, tcp->u_arg[1], tcp->u_arg[2]);
		tprintf(", %lu", tcp->u_arg[2]);
	}
	return 0;
}

int
sys_connect(tcp)
struct tcb *tcp;
{
	return sys_bind(tcp);
}

int
sys_listen(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%ld, %lu", tcp->u_arg[0], tcp->u_arg[1]);
	}
	return 0;
}

static int
do_accept(struct tcb *tcp, int flags_arg)
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
		return 0;
	}
	if (!tcp->u_arg[2])
		tprintf("%#lx, NULL", tcp->u_arg[1]);
	else {
		int len;
		if (tcp->u_arg[1] == 0 || syserror(tcp)
		    || umove (tcp, tcp->u_arg[2], &len) < 0) {
			tprintf("%#lx", tcp->u_arg[1]);
		} else {
			printsock(tcp, tcp->u_arg[1], len);
		}
		tprintf(", ");
		printnum_int(tcp, tcp->u_arg[2], "%u");
	}
	if (flags_arg >= 0) {
		tprintf(", ");
		printflags(sock_type_flags, tcp->u_arg[flags_arg],
			   "SOCK_???");
	}
	return 0;
}

int
sys_accept(struct tcb *tcp)
{
	return do_accept(tcp, -1);
}

#ifdef LINUX
int
sys_accept4(struct tcb *tcp)
{
	return do_accept(tcp, 3);
}
#endif

int
sys_send(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
		printstr(tcp, tcp->u_arg[1], tcp->u_arg[2]);
		tprintf(", %lu, ", tcp->u_arg[2]);
		/* flags */
		printflags(msg_flags, tcp->u_arg[3], "MSG_???");
	}
	return 0;
}

int
sys_sendto(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
		printstr(tcp, tcp->u_arg[1], tcp->u_arg[2]);
		tprintf(", %lu, ", tcp->u_arg[2]);
		/* flags */
		printflags(msg_flags, tcp->u_arg[3], "MSG_???");
		/* to address */
		tprintf(", ");
		printsock(tcp, tcp->u_arg[4], tcp->u_arg[5]);
		/* to length */
		tprintf(", %lu", tcp->u_arg[5]);
	}
	return 0;
}

#ifdef HAVE_SENDMSG

int
sys_sendmsg(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
		printmsghdr(tcp, tcp->u_arg[1]);
		/* flags */
		tprintf(", ");
		printflags(msg_flags, tcp->u_arg[2], "MSG_???");
	}
	return 0;
}

#endif /* HAVE_SENDMSG */

int
sys_recv(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
	} else {
		if (syserror(tcp))
			tprintf("%#lx", tcp->u_arg[1]);
		else
			printstr(tcp, tcp->u_arg[1], tcp->u_rval);

		tprintf(", %lu, ", tcp->u_arg[2]);
		printflags(msg_flags, tcp->u_arg[3], "MSG_???");
	}
	return 0;
}

int
sys_recvfrom(tcp)
struct tcb *tcp;
{
	int fromlen;

	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
	} else {
		if (syserror(tcp)) {
			tprintf("%#lx, %lu, %lu, %#lx, %#lx",
				tcp->u_arg[1], tcp->u_arg[2], tcp->u_arg[3],
				tcp->u_arg[4], tcp->u_arg[5]);
			return 0;
		}
		/* buf */
		printstr(tcp, tcp->u_arg[1], tcp->u_rval);
		/* len */
		tprintf(", %lu, ", tcp->u_arg[2]);
		/* flags */
		printflags(msg_flags, tcp->u_arg[3], "MSG_???");
		/* from address, len */
		if (!tcp->u_arg[4] || !tcp->u_arg[5]) {
			if (tcp->u_arg[4] == 0)
				tprintf(", NULL");
			else
				tprintf(", %#lx", tcp->u_arg[4]);
			if (tcp->u_arg[5] == 0)
				tprintf(", NULL");
			else
				tprintf(", %#lx", tcp->u_arg[5]);
			return 0;
		}
		if (umove(tcp, tcp->u_arg[5], &fromlen) < 0) {
			tprintf(", {...}, [?]");
			return 0;
		}
		tprintf(", ");
		printsock(tcp, tcp->u_arg[4], tcp->u_arg[5]);
		/* from length */
		tprintf(", [%u]", fromlen);
	}
	return 0;
}

#ifdef HAVE_SENDMSG

int
sys_recvmsg(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
	} else {
		if (syserror(tcp) || !verbose(tcp))
			tprintf("%#lx", tcp->u_arg[1]);
		else
			printmsghdr(tcp, tcp->u_arg[1]);
		/* flags */
		tprintf(", ");
		printflags(msg_flags, tcp->u_arg[2], "MSG_???");
	}
	return 0;
}

#ifdef LINUX
int
sys_recvmmsg(struct tcb *tcp)
{
	static char str[128];
	if (entering(tcp)) {

		tprintf("%ld, ", tcp->u_arg[0]);
		if (verbose(tcp)) {
			sprint_timespec(str, tcp, tcp->u_arg[4]);
			tcp->auxstr = strdup(str);
		} else {
			tprintf("%#lx, %ld, ", tcp->u_arg[1], tcp->u_arg[2]);
			printflags(msg_flags, tcp->u_arg[3], "MSG_???");
			tprintf(", ");
			print_timespec(tcp, tcp->u_arg[4]);
		}
		return 0;
	} else {
		if (verbose(tcp)) {
			if (syserror(tcp))
				tprintf("%#lx", tcp->u_arg[1]);
			else
				printmmsghdr(tcp, tcp->u_arg[1]);
			tprintf(", %ld, ", tcp->u_arg[2]);
			/* flags */
			printflags(msg_flags, tcp->u_arg[3], "MSG_???");
			/* timeout on entrance */
			tprintf(", %s", tcp->auxstr ? tcp->auxstr : "{...}");
			free((void *) tcp->auxstr);
			tcp->auxstr = NULL;
		}
		if (syserror(tcp))
			return 0;
		if (tcp->u_rval == 0) {
			tcp->auxstr = "Timeout";
			return RVAL_STR;
		}
		if (!verbose(tcp))
			return 0;
		/* timeout on exit */
		strcpy(str, "left ");
		sprint_timespec(str + strlen(str), tcp, tcp->u_arg[4]);
		tcp->auxstr = str;
		return RVAL_STR;
	}
}
#endif

#endif /* HAVE_SENDMSG */

static const struct xlat shutdown_modes[] = {
       { 0,	"SHUT_RD"	},
       { 1,	"SHUT_WR"	},
       { 2,	"SHUT_RDWR"	},
       { 0,	NULL		}
};

int
sys_shutdown(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
		printxval(shutdown_modes, tcp->u_arg[1], "SHUT_???");
	}
	return 0;
}

int
sys_getsockname(tcp)
struct tcb *tcp;
{
	return sys_accept(tcp);
}

int
sys_getpeername(tcp)
struct tcb *tcp;
{
	return sys_accept(tcp);
}

static int
do_pipe(struct tcb *tcp, int flags_arg)
{
	if (exiting(tcp)) {
		if (syserror(tcp)) {
			tprintf("%#lx", tcp->u_arg[0]);
		} else {
#if defined(LINUX) && !defined(SPARC) && !defined(SPARC64) && !defined(SH) && !defined(IA64)
			int fds[2];

			if (umoven(tcp, tcp->u_arg[0], sizeof fds, (char *) fds) < 0)
				tprintf("[...]");
			else
				tprintf("[%u, %u]", fds[0], fds[1]);
#elif defined(SPARC) || defined(SPARC64) || defined(SH) || defined(SVR4) || defined(FREEBSD) || defined(IA64)
			tprintf("[%lu, %lu]", tcp->u_rval, getrval2(tcp));
#else
			tprintf("%#lx", tcp->u_arg[0]);
#endif
		}
		if (flags_arg >= 0) {
			tprintf(", ");
			printflags(open_mode_flags, tcp->u_arg[flags_arg], "O_???");
		}
	}
	return 0;
}

int
sys_pipe(struct tcb *tcp)
{
	return do_pipe(tcp, -1);
}

#ifdef LINUX
int
sys_pipe2(struct tcb *tcp)
{
	return do_pipe(tcp, 1);
}
#endif

int
sys_socketpair(struct tcb *tcp)
{
#ifdef LINUX
	int fds[2];
#endif

	if (entering(tcp)) {
		printxval(domains, tcp->u_arg[0], "PF_???");
		tprintf(", ");
		tprint_sock_type(tcp, tcp->u_arg[1]);
		tprintf(", ");
		switch (tcp->u_arg[0]) {
		case PF_INET:
			printxval(protocols, tcp->u_arg[2], "IPPROTO_???");
			break;
#ifdef PF_IPX
		case PF_IPX:
			/* BTW: I don't believe this.. */
			tprintf("[");
			printxval(domains, tcp->u_arg[2], "PF_???");
			tprintf("]");
			break;
#endif /* PF_IPX */
		default:
			tprintf("%lu", tcp->u_arg[2]);
			break;
		}
	} else {
		if (syserror(tcp)) {
			tprintf(", %#lx", tcp->u_arg[3]);
			return 0;
		}
#ifdef LINUX
		if (umoven(tcp, tcp->u_arg[3], sizeof fds, (char *) fds) < 0)
			tprintf(", [...]");
		else
			tprintf(", [%u, %u]", fds[0], fds[1]);
#endif /* LINUX */
#if defined(SUNOS4) || defined(SVR4) || defined(FREEBSD)
		tprintf(", [%lu, %lu]", tcp->u_rval, getrval2(tcp));
#endif /* SUNOS4 || SVR4 || FREEBSD */
	}
	return 0;
}

int
sys_getsockopt(struct tcb *tcp)
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
		printxval(socketlayers, tcp->u_arg[1], "SOL_???");
		tprintf (", ");
		switch (tcp->u_arg[1]) {
		case SOL_SOCKET:
			printxval(sockoptions, tcp->u_arg[2], "SO_???");
			break;
#ifdef SOL_IP
		case SOL_IP:
			printxval(sockipoptions, tcp->u_arg[2], "IP_???");
			break;
#endif
#ifdef SOL_IPV6
		case SOL_IPV6:
			printxval(sockipv6options, tcp->u_arg[2], "IPV6_???");
			break;
#endif
#ifdef SOL_IPX
		case SOL_IPX:
			printxval(sockipxoptions, tcp->u_arg[2], "IPX_???");
			break;
#endif
#ifdef SOL_PACKET
		case SOL_PACKET:
			printxval(sockpacketoptions, tcp->u_arg[2], "PACKET_???");
			break;
#endif
#ifdef SOL_TCP
		case SOL_TCP:
			printxval(socktcpoptions, tcp->u_arg[2], "TCP_???");
			break;
#endif
#ifdef SOL_SCTP
		case SOL_SCTP:
			printxval(socksctpoptions, tcp->u_arg[2], "SCTP_???");
			break;
#endif

		/* SOL_AX25 SOL_ROSE SOL_ATALK SOL_NETROM SOL_UDP SOL_DECNET SOL_X25
		 * etc. still need work */
		default:
			tprintf("%lu", tcp->u_arg[2]);
			break;
		}
		tprintf (", ");
	} else {
		int len;
		if (syserror(tcp) || umove (tcp, tcp->u_arg[4], &len) < 0) {
			tprintf("%#lx, %#lx",
				tcp->u_arg[3], tcp->u_arg[4]);
			return 0;
		}

		switch (tcp->u_arg[1]) {
		case SOL_SOCKET:
			switch (tcp->u_arg[2]) {
#ifdef SO_LINGER
			case SO_LINGER:
				if (len == sizeof (struct linger)) {
					struct linger linger;
					if (umove (tcp,
						   tcp->u_arg[3],
						   &linger) < 0)
						break;
					tprintf("{onoff=%d, linger=%d}, "
						"[%d]",
						linger.l_onoff,
						linger.l_linger,
						len);
					return 0;
				}
				break;
#endif
#ifdef SO_PEERCRED
			case SO_PEERCRED:
				if (len == sizeof (struct ucred)) {
					struct ucred uc;
					if (umove (tcp,
						   tcp->u_arg[3],
						   &uc) < 0)
						break;
					tprintf("{pid=%ld, uid=%ld, gid=%ld}, "
						"[%d]",
						(long)uc.pid,
						(long)uc.uid,
						(long)uc.gid,
						len);
					return 0;
				}
				break;
#endif
			}
			break;
		case SOL_PACKET:
			switch (tcp->u_arg[2]) {
#ifdef PACKET_STATISTICS
			case PACKET_STATISTICS:
				if (len == sizeof(struct tpacket_stats)) {
					struct tpacket_stats stats;
					if (umove (tcp,
						   tcp->u_arg[3],
						   &stats) < 0)
						break;
					tprintf("{packets=%u, drops=%u}, "
						"[%d]",
						stats.tp_packets,
						stats.tp_drops,
						len);
					return 0;
				}
				break;
#endif
			}
			break;
		}

		if (len == sizeof (int)) {
			printnum_int(tcp, tcp->u_arg[3], "%d");
		}
		else {
			printstr (tcp, tcp->u_arg[3], len);
		}
		tprintf(", [%d]", len);
	}
	return 0;
}

#if defined(ICMP_FILTER)
static void printicmpfilter(tcp, addr)
struct tcb *tcp;
long addr;
{
	struct icmp_filter	filter;

	if (!addr) {
		tprintf("NULL");
		return;
	}
	if (syserror(tcp) || !verbose(tcp)) {
		tprintf("%#lx", addr);
		return;
	}
	if (umove(tcp, addr, &filter) < 0) {
		tprintf("{...}");
		return;
	}

	tprintf("~(");
	printflags(icmpfilterflags, ~filter.data, "ICMP_???");
	tprintf(")");
}
#endif /* ICMP_FILTER */

static int
printsockopt (tcp, level, name, addr, len)
struct tcb *tcp;
int level;
int name;
long addr;
int len;
{
	printxval(socketlayers, level, "SOL_??");
	tprintf (", ");
	switch (level) {
	    case SOL_SOCKET:
		printxval(sockoptions, name, "SO_???");
		switch (name) {
#if defined(SO_LINGER)
		    case SO_LINGER:
			if (len == sizeof (struct linger)) {
				struct linger linger;
				if (umove (tcp, addr, &linger) < 0)
					break;
				tprintf(", {onoff=%d, linger=%d}",
					linger.l_onoff,
					linger.l_linger);
				return 0;
			}
			break;
#endif
		}
		break;
#ifdef SOL_IP
	    case SOL_IP:
		printxval(sockipoptions, name, "IP_???");
		break;
#endif
#ifdef SOL_IPV6
	    case SOL_IPV6:
		printxval(sockipv6options, name, "IPV6_???");
		break;
#endif
#ifdef SOL_IPX
	    case SOL_IPX:
		printxval(sockipxoptions, name, "IPX_???");
		break;
#endif
#ifdef SOL_PACKET
	    case SOL_PACKET:
		printxval(sockpacketoptions, name, "PACKET_???");
		/* TODO: decode packate_mreq for PACKET_*_MEMBERSHIP */
		switch (name) {
#ifdef PACKET_RX_RING
		    case PACKET_RX_RING:
#endif
#ifdef PACKET_TX_RING
		    case PACKET_TX_RING:
#endif
#if defined(PACKET_RX_RING) || defined(PACKET_TX_RING)
			if (len == sizeof(struct tpacket_req)) {
				struct tpacket_req req;
				if (umove(tcp, addr, &req) < 0)
					break;
				tprintf(", {block_size=%u, block_nr=%u, frame_size=%u, frame_nr=%u}",
					req.tp_block_size,
					req.tp_block_nr,
					req.tp_frame_size,
					req.tp_frame_nr);
				return 0;
			}
			break;
#endif /* PACKET_RX_RING || PACKET_TX_RING */
		}
		break;
#endif
#ifdef SOL_TCP
	    case SOL_TCP:
		printxval(socktcpoptions, name, "TCP_???");
		break;
#endif
#ifdef SOL_SCTP
	   case SOL_SCTP:
		printxval(socksctpoptions, name, "SCTP_???");
		break;
#endif
#ifdef SOL_RAW
	    case SOL_RAW:
		printxval(sockrawoptions, name, "RAW_???");
		switch (name) {
#if defined(ICMP_FILTER)
		    case ICMP_FILTER:
			tprintf(", ");
			printicmpfilter(tcp, addr);
			return 0;
#endif
		}
		break;
#endif

		/* SOL_AX25 SOL_ATALK SOL_NETROM SOL_UDP SOL_DECNET SOL_X25
		 * etc. still need work  */

	    default:
		tprintf("%u", name);
	}

	/* default arg printing */

	tprintf (", ");

	if (len == sizeof (int)) {
		printnum_int (tcp, addr, "%d");
	}
	else {
		printstr (tcp, addr, len);
	}
	return 0;
}


#ifdef HAVE_STRUCT_OPTHDR

void
print_sock_optmgmt (tcp, addr, len)
struct tcb *tcp;
long addr;
int len;
{
	int c = 0;
	struct opthdr hdr;

	while (len >= (int) sizeof hdr) {
		if (umove(tcp, addr, &hdr) < 0) break;
		if (c++) {
			tprintf (", ");
		}
		else if (len > hdr.len + sizeof hdr) {
			tprintf ("[");
		}
		tprintf ("{");
		addr += sizeof hdr;
		len -= sizeof hdr;
		printsockopt (tcp, hdr.level, hdr.name, addr, hdr.len);
		if (hdr.len > 0) {
			addr += hdr.len;
			len -= hdr.len;
		}
		tprintf ("}");
	}
	if (len > 0) {
		if (c++) tprintf (", ");
		printstr (tcp, addr, len);
	}
	if (c > 1) tprintf ("]");
}

#endif

int
sys_setsockopt(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		tprintf("%ld, ", tcp->u_arg[0]);
		printsockopt (tcp, tcp->u_arg[1], tcp->u_arg[2],
			      tcp->u_arg[3], tcp->u_arg[4]);
		tprintf(", %lu", tcp->u_arg[4]);
	}
	return 0;
}

#if UNIXWARE >= 7

static const struct xlat sock_version[] = {
	{ __NETLIB_UW211_SVR4,	"UW211_SVR4" },
	{ __NETLIB_UW211_XPG4,	"UW211_XPG4" },
	{ __NETLIB_GEMINI_SVR4,	"GEMINI_SVR4" },
	{ __NETLIB_GEMINI_XPG4,	"GEMINI_XPG4" },
	{ __NETLIB_FP1_SVR4,	"FP1_SVR4" },
	{ __NETLIB_FP1_XPG4,	"FP1_XPG4" },
	{ 0,            NULL            },
};


int
netlib_call(tcp, func)
struct tcb *tcp;
int (*func) ();
{
	if (entering(tcp)) {
		int i;
		printxval (sock_version, tcp->u_arg[0], "__NETLIB_???");
		tprintf(", ");
		--tcp->u_nargs;
		for (i = 0; i < tcp->u_nargs; i++)
			tcp->u_arg[i] = tcp->u_arg[i + 1];
		return func (tcp);

	}

	return func (tcp);
}

int
sys_xsocket(tcp)
struct tcb *tcp;
{
	return netlib_call (tcp, sys_socket);
}

int
sys_xsocketpair(tcp)
struct tcb *tcp;
{
	return netlib_call (tcp, sys_socketpair);
}

int
sys_xbind(tcp)
struct tcb *tcp;
{
	return netlib_call (tcp, sys_bind);
}

int
sys_xconnect(tcp)
struct tcb *tcp;
{
	return netlib_call (tcp, sys_connect);
}

int
sys_xlisten(tcp)
struct tcb *tcp;
{
	return netlib_call (tcp, sys_listen);
}

int
sys_xaccept(tcp)
struct tcb *tcp;
{
	return netlib_call (tcp, sys_accept);
}

int
sys_xsendmsg(tcp)
struct tcb *tcp;
{
	return netlib_call (tcp, sys_sendmsg);
}

int
sys_xrecvmsg(tcp)
struct tcb *tcp;
{
	return netlib_call (tcp, sys_recvmsg);
}

int
sys_xgetsockaddr(tcp)
struct tcb *tcp;
{
	if (entering(tcp)) {
		printxval (sock_version, tcp->u_arg[0], "__NETLIB_???");
		tprintf(", ");
		if (tcp->u_arg[1] == 0) {
			tprintf ("LOCALNAME, ");
		}
		else if (tcp->u_arg[1] == 1) {
			tprintf ("REMOTENAME, ");
		}
		else {
			tprintf ("%ld, ", tcp->u_arg [1]);
		}
		tprintf ("%ld, ", tcp->u_arg [2]);
	}
	else {
		if (tcp->u_arg[3] == 0 || syserror(tcp)) {
			tprintf("%#lx", tcp->u_arg[3]);
		} else {
			printsock(tcp, tcp->u_arg[3], tcp->u_arg[4]);
		}
		tprintf(", ");
		printnum(tcp, tcp->u_arg[4], "%lu");
	}

	return 0;

}

int
sys_xgetsockopt(tcp)
struct tcb *tcp;
{
	return netlib_call (tcp, sys_getsockopt);
}

int
sys_xsetsockopt(tcp)
struct tcb *tcp;
{
	return netlib_call (tcp, sys_setsockopt);
}

int
sys_xshutdown(tcp)
struct tcb *tcp;
{
	return netlib_call (tcp, sys_shutdown);
}

#endif /* UNIXWARE */
