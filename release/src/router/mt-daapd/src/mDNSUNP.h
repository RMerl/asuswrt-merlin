/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@

    Change History (most recent first):

$Log: mDNSUNP.h,v $
Revision 1.1  2009-06-30 02:31:08  steven
iTune Server

Revision 1.3  2005/01/10 18:11:29  rpedde
Fix compile error on solaris

Revision 1.2  2005/01/10 01:07:01  rpedde
Synchronize mDNS to Apples 58.8 drop

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

#ifdef  __cplusplus
    extern "C" {
#endif

#ifdef NOT_HAVE_SOCKLEN_T
    typedef unsigned int socklen_t;
#endif

#if !defined(_SS_MAXSIZE)
    #define sockaddr_storage sockaddr
#endif

#ifndef NOT_HAVE_SA_LEN
#define GET_SA_LEN(X) (sizeof(struct sockaddr) > ((struct sockaddr*)&(X))->sa_len ? \
                       sizeof(struct sockaddr) : ((struct sockaddr*)&(X))->sa_len   )
#elif mDNSIPv6Support
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

extern ssize_t recvfrom_flags(int fd, void *ptr, size_t nbytes, int *flagsp,
               struct sockaddr *sa, socklen_t *salenptr, struct my_in_pktinfo *pktp);

struct ifi_info {
  char    ifi_name[IFI_NAME];   /* interface name, null terminated */
  u_char  ifi_haddr[IFI_HADDR]; /* hardware address */
  u_short ifi_hlen;             /* #bytes in hardware address: 0, 6, 8 */
  short   ifi_flags;            /* IFF_xxx constants from <net/if.h> */
  short   ifi_myflags;          /* our own IFI_xxx flags */
  int     ifi_index;            /* interface index */
  struct sockaddr  *ifi_addr;   /* primary address */
  struct sockaddr  *ifi_brdaddr;/* broadcast address */
  struct sockaddr  *ifi_dstaddr;/* destination address */
  struct ifi_info  *ifi_next;   /* next of these structures */
};

#define IFI_ALIAS   1           /* ifi_addr is an alias */

extern struct ifi_info  *get_ifi_info(int family, int doaliases);
extern void             free_ifi_info(struct ifi_info *);

#ifdef  __cplusplus
    }
#endif

#endif
