/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

    Change History (most recent first):

$Log: mDNSUNP.h,v $
Revision 1.19  2006/08/14 23:24:47  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.18  2005/04/08 21:37:57  ksekar
<rdar://problem/3792767> get_ifi_info doesn't return IPv6 interfaces on Linux

Revision 1.17  2004/12/17 19:32:43  cheshire
Add missing semicolon

Revision 1.16  2004/12/01 04:25:05  cheshire
<rdar://problem/3872803> Darwin patches for Solaris and Suse
Provide daemon() for platforms that don't have it

Revision 1.15  2004/11/30 22:37:01  cheshire
Update copyright dates and add "Mode: C; tab-width: 4" headers

Revision 1.14  2004/10/16 00:17:01  cheshire
<rdar://problem/3770558> Replace IP TTL 255 check with local subnet source address check

Revision 1.13  2004/03/20 05:37:09  cheshire
Fix contributed by Terry Lambert & Alfred Perlstein:
Don't use uint8_t -- it requires stdint.h, which doesn't exist on FreeBSD 4.x

Revision 1.12  2004/01/28 21:12:15  cheshire
Reconcile mDNSIPv6Support & HAVE_IPV6 into a single flag (HAVE_IPV6)

Revision 1.11  2003/12/13 05:43:09  bradley
Fixed non-sa_len and non-IPv6 version of GET_SA_LEN macro to cast as sockaddr to access
sa_family so it works with any sockaddr-compatible address structure (e.g. sockaddr_storage).

Revision 1.10  2003/12/11 03:03:51  rpantos
Clean up mDNSPosix so that it builds on OS X again.

Revision 1.9  2003/12/08 20:47:02  rpantos
Add support for mDNSResponder on Linux.

Revision 1.8  2003/08/12 19:56:26  cheshire
Update to APSL 2.0

Revision 1.7  2003/08/06 18:20:51  cheshire
Makefile cleanup

Revision 1.6  2003/07/02 21:19:59  cheshire
<rdar://problem/3313413> Update copyright notices, etc., in source code comments

Revision 1.5  2003/03/13 03:46:21  cheshire
Fixes to make the code build on Linux

Revision 1.4  2002/12/23 22:13:32  jgraessl

Reviewed by: Stuart Cheshire
Initial IPv6 support for mDNSResponder.

Revision 1.3  2002/09/21 20:44:53  zarzycki
Added APSL info

Revision 1.2  2002/09/19 04:20:44  cheshire
Remove high-ascii characters that confuse some systems

Revision 1.1  2002/09/17 06:24:35  cheshire
First checkin

*/

#ifndef __mDNSUNP_h
#define __mDNSUNP_h

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>

#ifdef HAVE_LINUX
#include <linux/socket.h>
#endif

#ifdef  __cplusplus
    extern "C" {
#endif

#ifdef NOT_HAVE_SOCKLEN_T
    typedef unsigned int socklen_t;
#endif

#if !defined(_SS_MAXSIZE)
#if HAVE_IPV6
#define sockaddr_storage sockaddr_in6
#else
#define sockaddr_storage sockaddr
#endif // HAVE_IPV6	
#endif // !defined(_SS_MAXSIZE)

#ifndef NOT_HAVE_SA_LEN
#define GET_SA_LEN(X) (sizeof(struct sockaddr) > ((struct sockaddr*)&(X))->sa_len ? \
                       sizeof(struct sockaddr) : ((struct sockaddr*)&(X))->sa_len   )
#elif HAVE_IPV6
#define GET_SA_LEN(X) (((struct sockaddr*)&(X))->sa_family == AF_INET  ? sizeof(struct sockaddr_in) : \
                       ((struct sockaddr*)&(X))->sa_family == AF_INET6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr))
#else
#define GET_SA_LEN(X) (((struct sockaddr*)&(X))->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr))
#endif

#define IFI_NAME    16          /* same as IFNAMSIZ in <net/if.h> */
#define IFI_HADDR    8          /* allow for 64-bit EUI-64 in future */

// Renamed from my_in_pktinfo because in_pktinfo is used by Linux.

struct my_in_pktinfo {
    struct sockaddr_storage ipi_addr;
    int                     ipi_ifindex;            /* received interface index */
    char                    ipi_ifname[IFI_NAME];   /* received interface name  */
};

/* From the text (Stevens, section 20.2): */
/* 'As an example of recvmsg we will write a function named recvfrom_flags that */
/* is similar to recvfrom but also returns: */
/*	1. the returned msg_flags value, */
/*	2. the destination addres of the received datagram (from the IP_RECVDSTADDR socket option, and */
/*	3. the index of the interface on which the datagram was received (the IP_RECVIF socket option).' */
extern ssize_t recvfrom_flags(int fd, void *ptr, size_t nbytes, int *flagsp,
               struct sockaddr *sa, socklen_t *salenptr, struct my_in_pktinfo *pktp, u_char *ttl);

struct ifi_info {
  char    ifi_name[IFI_NAME];   /* interface name, null terminated */
  u_char  ifi_haddr[IFI_HADDR]; /* hardware address */
  u_short ifi_hlen;             /* #bytes in hardware address: 0, 6, 8 */
  short   ifi_flags;            /* IFF_xxx constants from <net/if.h> */
  short   ifi_myflags;          /* our own IFI_xxx flags */
  int     ifi_index;            /* interface index */
  struct sockaddr  *ifi_addr;   /* primary address */
  struct sockaddr  *ifi_netmask;
  struct sockaddr  *ifi_brdaddr;/* broadcast address */
  struct sockaddr  *ifi_dstaddr;/* destination address */
  struct ifi_info  *ifi_next;   /* next of these structures */
};

#if defined(AF_INET6) && HAVE_IPV6 && HAVE_LINUX
#define PROC_IFINET6_PATH "/proc/net/if_inet6"
extern struct ifi_info  *get_ifi_info_linuxv6(int family, int doaliases);
#endif
	
#if defined(AF_INET6) && HAVE_IPV6
#define INET6_ADDRSTRLEN 46 /*Maximum length of IPv6 address */
#endif
	

	
#define IFI_ALIAS   1           /* ifi_addr is an alias */

/* From the text (Stevens, section 16.6): */
/* 'Since many programs need to know all the interfaces on a system, we will develop a */
/* function of our own named get_ifi_info that returns a linked list of structures, one */
/* for each interface that is currently "up."' */
extern struct ifi_info  *get_ifi_info(int family, int doaliases);

/* 'The free_ifi_info function, which takes a pointer that was */
/* returned by get_ifi_info and frees all the dynamic memory.' */
extern void             free_ifi_info(struct ifi_info *);

#ifdef NOT_HAVE_DAEMON
extern int daemon(int nochdir, int noclose);
#endif

#ifdef  __cplusplus
    }
#endif

#endif
