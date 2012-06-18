/* System-specific socket constants and types.  Linux/MIPS version.
   Copyright (C) 1991, 92, 1994-1999, 2000, 2001, 2004, 2005, 2006
   Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef __BITS_SOCKET_H
#define __BITS_SOCKET_H

#if !defined _SYS_SOCKET_H && !defined _NETINET_IN_H
# error "Never include <bits/socket.h> directly; use <sys/socket.h> instead."
#endif

#define	__need_size_t
#define __need_NULL
#include <stddef.h>

#include <limits.h>
#include <sys/types.h>

/* Type for length arguments in socket calls.  */
#ifndef __socklen_t_defined
typedef __socklen_t socklen_t;
# define __socklen_t_defined
#endif

/* Types of sockets.  */
enum __socket_type
{
  SOCK_DGRAM = 1,		/* Connectionless, unreliable datagrams
				   of fixed maximum length.  */
#define SOCK_DGRAM SOCK_DGRAM
  SOCK_STREAM = 2,		/* Sequenced, reliable, connection-based
				   byte streams.  */
#define SOCK_STREAM SOCK_STREAM
  SOCK_RAW = 3,			/* Raw protocol interface.  */
#define SOCK_RAW SOCK_RAW
  SOCK_RDM = 4,			/* Reliably-delivered messages.  */
#define SOCK_RDM SOCK_RDM
  SOCK_SEQPACKET = 5,		/* Sequenced, reliable, connection-based,
				   datagrams of fixed maximum length.  */
#define SOCK_SEQPACKET SOCK_SEQPACKET
  SOCK_PACKET = 10		/* Linux specific way of getting packets
				   at the dev level.  For writing rarp and
				   other similar things on the user level. */
#define SOCK_PACKET SOCK_PACKET
};

/* Protocol families.  */
#define	PF_UNSPEC	0	/* Unspecified.  */
#define	PF_LOCAL	1	/* Local to host (pipes and file-domain).  */
#define	PF_UNIX		PF_LOCAL /* Old BSD name for PF_LOCAL.  */
#define	PF_FILE		PF_LOCAL /* Another non-standard name for PF_LOCAL.  */
#define	PF_INET		2	/* IP protocol family.  */
#define	PF_AX25		3	/* Amateur Radio AX.25.  */
#define	PF_IPX		4	/* Novell Internet Protocol.  */
#define	PF_APPLETALK	5	/* Appletalk DDP.  */
#define	PF_NETROM	6	/* Amateur radio NetROM.  */
#define	PF_BRIDGE	7	/* Multiprotocol bridge.  */
#define	PF_ATMPVC	8	/* ATM PVCs.  */
#define	PF_X25		9	/* Reserved for X.25 project.  */
#define	PF_INET6	10	/* IP version 6.  */
#define	PF_ROSE		11	/* Amateur Radio X.25 PLP.  */
#define	PF_DECnet	12	/* Reserved for DECnet project.  */
#define	PF_NETBEUI	13	/* Reserved for 802.2LLC project.  */
#define	PF_SECURITY	14	/* Security callback pseudo AF.  */
#define	PF_KEY		15	/* PF_KEY key management API.  */
#define	PF_NETLINK	16
#define	PF_ROUTE	PF_NETLINK /* Alias to emulate 4.4BSD.  */
#define	PF_PACKET	17	/* Packet family.  */
#define	PF_ASH		18	/* Ash.  */
#define	PF_ECONET	19	/* Acorn Econet.  */
#define	PF_ATMSVC	20	/* ATM SVCs.  */
#define	PF_SNA		22	/* Linux SNA Project */
#define	PF_IRDA		23	/* IRDA sockets.  */
#define	PF_PPPOX	24	/* PPPoX sockets.  */
#define	PF_WANPIPE	25	/* Wanpipe API sockets.  */
#define	PF_BLUETOOTH	31	/* Bluetooth sockets.  */
#define	PF_MAX		32	/* For now..  */

/* Address families.  */
#define	AF_UNSPEC	PF_UNSPEC
#define	AF_LOCAL	PF_LOCAL
#define	AF_UNIX		PF_UNIX
#define	AF_FILE		PF_FILE
#define	AF_INET		PF_INET
#define	AF_AX25		PF_AX25
#define	AF_IPX		PF_IPX
#define	AF_APPLETALK	PF_APPLETALK
#define	AF_NETROM	PF_NETROM
#define	AF_BRIDGE	PF_BRIDGE
#define	AF_ATMPVC	PF_ATMPVC
#define	AF_X25		PF_X25
#define	AF_INET6	PF_INET6
#define	AF_ROSE		PF_ROSE
#define	AF_DECnet	PF_DECnet
#define	AF_NETBEUI	PF_NETBEUI
#define	AF_SECURITY	PF_SECURITY
#define	AF_KEY		PF_KEY
#define	AF_NETLINK	PF_NETLINK
#define	AF_ROUTE	PF_ROUTE
#define	AF_PACKET	PF_PACKET
#define	AF_ASH		PF_ASH
#define	AF_ECONET	PF_ECONET
#define	AF_ATMSVC	PF_ATMSVC
#define	AF_SNA		PF_SNA
#define	AF_IRDA		PF_IRDA
#define	AF_PPPOX	PF_PPPOX
#define	AF_WANPIPE	PF_WANPIPE
#define	AF_BLUETOOTH	PF_BLUETOOTH
#define	AF_MAX		PF_MAX

/* Socket level values.  Others are defined in the appropriate headers.

   XXX These definitions also should go into the appropriate headers as
   far as they are available.  */
#define SOL_RAW		255
#define SOL_DECNET      261
#define SOL_X25         262
#define SOL_PACKET	263
#define SOL_ATM		264	/* ATM layer (cell level).  */
#define SOL_AAL		265	/* ATM Adaption Layer (packet level).  */
#define SOL_IRDA	266

/* Maximum queue length specifiable by listen.  */
#define SOMAXCONN	128

/* Get the definition of the macro to define the common sockaddr members.  */
#include <bits/sockaddr.h>

/* Structure describing a generic socket address.  */
struct sockaddr
  {
    __SOCKADDR_COMMON (sa_);	/* Common data: address family and length.  */
    char sa_data[14];		/* Address data.  */
  };


/* Structure large enough to hold any socket address (with the historical
   exception of AF_UNIX).  We reserve 128 bytes.  */
#if ULONG_MAX > 0xffffffff
# define __ss_aligntype	__uint64_t
#else
# define __ss_aligntype	__uint32_t
#endif
#define _SS_SIZE	128
#define _SS_PADSIZE	(_SS_SIZE - (2 * sizeof (__ss_aligntype)))

struct sockaddr_storage
  {
    __SOCKADDR_COMMON (ss_);	/* Address family, etc.  */
    __ss_aligntype __ss_align;	/* Force desired alignment.  */
    char __ss_padding[_SS_PADSIZE];
  };


/* Bits in the FLAGS argument to `send', `recv', et al.  */
enum
  {
    MSG_OOB		= 0x01,	/* Process out-of-band data.  */
#define MSG_OOB		MSG_OOB
    MSG_PEEK		= 0x02,	/* Peek at incoming messages.  */
#define MSG_PEEK	MSG_PEEK
    MSG_DONTROUTE	= 0x04,	/* Don't use local routing.  */
#define MSG_DONTROUTE	MSG_DONTROUTE
#ifdef __USE_GNU
    /* DECnet uses a different name.  */
    MSG_TRYHARD		= MSG_DONTROUTE,
# define MSG_TRYHARD	MSG_DONTROUTE
#endif
    MSG_CTRUNC		= 0x08,	/* Control data lost before delivery.  */
#define MSG_CTRUNC	MSG_CTRUNC
    MSG_PROXY		= 0x10,	/* Supply or ask second address.  */
#define MSG_PROXY	MSG_PROXY
    MSG_TRUNC		= 0x20,
#define	MSG_TRUNC	MSG_TRUNC
    MSG_DONTWAIT	= 0x40, /* Nonblocking IO.  */
#define	MSG_DONTWAIT	MSG_DONTWAIT
    MSG_EOR		= 0x80, /* End of record.  */
#define	MSG_EOR		MSG_EOR
    MSG_WAITALL		= 0x100, /* Wait for a full request.  */
#define	MSG_WAITALL	MSG_WAITALL
    MSG_FIN		= 0x200,
#define	MSG_FIN		MSG_FIN
    MSG_SYN		= 0x400,
#define	MSG_SYN		MSG_SYN
    MSG_CONFIRM		= 0x800, /* Confirm path validity.  */
#define	MSG_CONFIRM	MSG_CONFIRM
    MSG_RST		= 0x1000,
#define	MSG_RST		MSG_RST
    MSG_ERRQUEUE	= 0x2000, /* Fetch message from error queue.  */
#define	MSG_ERRQUEUE	MSG_ERRQUEUE
    MSG_NOSIGNAL	= 0x4000, /* Do not generate SIGPIPE.  */
#define	MSG_NOSIGNAL	MSG_NOSIGNAL
    MSG_MORE		= 0x8000  /* Sender will send more.  */
#define	MSG_MORE	MSG_MORE
  };


/* Structure describing messages sent by
   `sendmsg' and received by `recvmsg'.  */
/* Note: do not change these members to match glibc; these match the
   SuSv3 spec already (e.g. msg_iovlen/msg_controllen).
   http://www.opengroup.org/onlinepubs/009695399/basedefs/sys/socket.h.html */
/* Note: linux kernel uses __kernel_size_t (which is 8bytes on 64bit
   platforms, and 4bytes on 32bit platforms) for msg_iovlen/msg_controllen */
struct msghdr
  {
    void *msg_name;		/* Address to send to/receive from.  */
    socklen_t msg_namelen;	/* Length of address data.  */

    struct iovec *msg_iov;	/* Vector of data to send/receive into.  */
#if __WORDSIZE == 32
    int msg_iovlen;		/* Number of elements in the vector.  */
#else
    size_t msg_iovlen;		/* Number of elements in the vector.  */
#endif

    void *msg_control;		/* Ancillary data (eg BSD filedesc passing). */
#if __WORDSIZE == 32
    socklen_t msg_controllen;	/* Ancillary data buffer length.  */
#else
    size_t msg_controllen;	/* Ancillary data buffer length.  */
#endif

    int msg_flags;		/* Flags on received message.  */
  };

/* Structure used for storage of ancillary data object information.  */
struct cmsghdr
  {
    size_t cmsg_len;		/* Length of data in cmsg_data plus length
				   of cmsghdr structure.  */
    int cmsg_level;		/* Originating protocol.  */
    int cmsg_type;		/* Protocol specific type.  */
#if (!defined __STRICT_ANSI__ && __GNUC__ >= 2) || __STDC_VERSION__ >= 199901L
    __extension__ unsigned char __cmsg_data __flexarr; /* Ancillary data.  */
#endif
  };

/* Ancillary data object manipulation macros.  */
#if (!defined __STRICT_ANSI__ && __GNUC__ >= 2) || __STDC_VERSION__ >= 199901L
# define CMSG_DATA(cmsg) ((cmsg)->__cmsg_data)
#else
# define CMSG_DATA(cmsg) ((unsigned char *) ((struct cmsghdr *) (cmsg) + 1))
#endif
#define CMSG_NXTHDR(mhdr, cmsg) __cmsg_nxthdr (mhdr, cmsg)
#define CMSG_FIRSTHDR(mhdr) \
  ((size_t) (mhdr)->msg_controllen >= sizeof (struct cmsghdr)		      \
   ? (struct cmsghdr *) (mhdr)->msg_control : (struct cmsghdr *) NULL)
#define CMSG_ALIGN(len) (((len) + sizeof (size_t) - 1) \
			 & (size_t) ~(sizeof (size_t) - 1))
#define CMSG_SPACE(len) (CMSG_ALIGN (len) \
			 + CMSG_ALIGN (sizeof (struct cmsghdr)))
#define CMSG_LEN(len)   (CMSG_ALIGN (sizeof (struct cmsghdr)) + (len))

extern struct cmsghdr *__cmsg_nxthdr (struct msghdr *__mhdr,
				      struct cmsghdr *__cmsg) __THROW;
#ifdef __USE_EXTERN_INLINES
# ifndef _EXTERN_INLINE
#  define _EXTERN_INLINE extern __inline
# endif
_EXTERN_INLINE struct cmsghdr *
__NTH (__cmsg_nxthdr (struct msghdr *__mhdr, struct cmsghdr *__cmsg))
{
  if ((size_t) __cmsg->cmsg_len < sizeof (struct cmsghdr))
    /* The kernel header does this so there may be a reason.  */
    return 0;

  __cmsg = (struct cmsghdr *) ((unsigned char *) __cmsg
			       + CMSG_ALIGN (__cmsg->cmsg_len));
  if ((unsigned char *) (__cmsg + 1) > ((unsigned char *) __mhdr->msg_control
					+ __mhdr->msg_controllen)
      || ((unsigned char *) __cmsg + CMSG_ALIGN (__cmsg->cmsg_len)
	  > ((unsigned char *) __mhdr->msg_control + __mhdr->msg_controllen)))
    /* No more entries.  */
    return 0;
  return __cmsg;
}
#endif	/* Use `extern inline'.  */

/* Socket level message types.  This must match the definitions in
   <linux/socket.h>.  */
enum
  {
    SCM_RIGHTS = 0x01		/* Transfer file descriptors.  */
#define SCM_RIGHTS SCM_RIGHTS
#ifdef __USE_BSD
    , SCM_CREDENTIALS = 0x02	/* Credentials passing.  */
# define SCM_CREDENTIALS SCM_CREDENTIALS
#endif
  };

/* User visible structure for SCM_CREDENTIALS message */

struct ucred
{
  pid_t pid;			/* PID of sending process.  */
  uid_t uid;			/* UID of sending process.  */
  gid_t gid;			/* GID of sending process.  */
};

/* Get socket manipulation related informations from kernel headers.  */
#ifndef __GLIBC__
#define __GLIBC__ 2
#include <asm/socket.h>
#undef __GLIBC__
#else
#include <asm/socket.h>
#endif


/* Structure used to manipulate the SO_LINGER option.  */
struct linger
  {
    int l_onoff;		/* Nonzero to linger on close.  */
    int l_linger;		/* Time to linger.  */
  };

#endif	/* bits/socket.h */
