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

$Log: mDNSUNP.c,v $
Revision 1.1  2009-06-30 02:31:08  steven
iTune Server

Revision 1.2.2.3  2005/09/11 19:13:48  rpedde
mdns fix for freebsd

Revision 1.2.2.2  2005/09/10 17:45:20  rpedde
Fixes for fbsd

Revision 1.2.2.1  2005/09/08 06:57:51  rpedde
Update to 0.2.2, fix amd64

Revision 1.2  2005/01/10 01:07:01  rpedde
Synchronize mDNS to Apples 58.8 drop

Revision 1.12  2003/09/02 20:47:13  cheshire
Fix signed/unsigned warning

Revision 1.11  2003/08/12 19:56:26  cheshire
Update to APSL 2.0

Revision 1.10  2003/08/06 18:20:51  cheshire
Makefile cleanup

Revision 1.9  2003/07/14 18:11:54  cheshire
Fix stricter compiler warnings

Revision 1.8  2003/07/02 21:19:59  cheshire
<rdar://problem/3313413> Update copyright notices, etc., in source code comments

Revision 1.7  2003/03/20 21:10:31  cheshire
Fixes done at IETF 56 to make mDNSProxyResponderPosix run on Solaris

Revision 1.6  2003/03/13 03:46:21  cheshire
Fixes to make the code build on Linux

Revision 1.5  2003/02/07 03:02:02  cheshire
Submitted by: Mitsutaka Watanabe
The code saying "index += 1;" was effectively making up random interface index values.
The right way to find the correct interface index is if_nametoindex();

Revision 1.4  2002/12/23 22:13:31  jgraessl

Reviewed by: Stuart Cheshire
Initial IPv6 support for mDNSResponder.

Revision 1.3  2002/09/21 20:44:53  zarzycki
Added APSL info

Revision 1.2  2002/09/19 04:20:44  cheshire
Remove high-ascii characters that confuse some systems

Revision 1.1  2002/09/17 06:24:34  cheshire
First checkin

*/

#include "mDNSUNP.h"

#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>

/* Solaris defined SIOCGIFCONF etc in <sys/sockio.h> but 
   other platforms don't even have that include file.  So, 
   if we haven't yet got a definition, let's try to find 
   <sys/sockio.h>.
*/

#ifndef SIOCGIFCONF
    #include <sys/sockio.h>
#endif

/* sockaddr_dl is only referenced if we're using IP_RECVIF, 
   so only include the header in that case.
*/

#ifdef  IP_RECVIF
    #include <net/if_dl.h>
#endif


struct ifi_info *get_ifi_info(int family, int doaliases)
{
    int                 junk;
    struct ifi_info     *ifi, *ifihead, **ifipnext;
    int                 sockfd, len, lastlen, flags, myflags;
    char                *ptr, *buf, lastname[IFNAMSIZ], *cptr;
    struct ifconf       ifc;
    struct ifreq        *ifr, ifrcopy;
    struct sockaddr_in  *sinptr;
    
#if defined(AF_INET6) && defined(HAVE_IPV6)
    struct sockaddr_in6 *sinptr6;
#endif

    sockfd = -1;
    buf = NULL;
    ifihead = NULL;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        goto gotError;
    }

    lastlen = 0;
    len = 100 * sizeof(struct ifreq);   /* initial buffer size guess */
    for ( ; ; ) {
        buf = malloc(len);
        if (buf == NULL) {
            goto gotError;
        }
        ifc.ifc_len = len;
        ifc.ifc_buf = buf;
        if (ioctl(sockfd, SIOCGIFCONF, &ifc) < 0) {
            if (errno != EINVAL || lastlen != 0) {
                goto gotError;
            }
        } else {
            if (ifc.ifc_len == lastlen)
                break;      /* success, len has not changed */
            lastlen = ifc.ifc_len;
        }
        len += 10 * sizeof(struct ifreq);   /* increment */
        free(buf);
    }
    ifihead = NULL;
    ifipnext = &ifihead;
    lastname[0] = 0;
/* end get_ifi_info1 */

/* include get_ifi_info2 */
    for (ptr = buf; ptr < buf + ifc.ifc_len; ) {
        ifr = (struct ifreq *) ptr;

		len = GET_SA_LEN(ifr->ifr_addr);

		/* This is completely whacked, and I really need to
		 * find out why this is the case, but I need to
		 * release a 0.2.2, and as the next stable won't
		 * have the apple mDNS included, I guess it's a
		 * small price to pay.
		 */
#ifdef FREEBSD
		    ptr += sizeof(ifr->ifr_name) + len; /* for next one in buffer */
		//		ptr += sizeof(*ifr);
#else

		    ptr += sizeof(struct ifreq); /* for next one in buffer */
#endif
    
//        fprintf(stderr, "intf %d name=%s AF=%d\n", index, ifr->ifr_name, ifr->ifr_addr.sa_family);
        
        if (ifr->ifr_addr.sa_family != family)
            continue;   /* ignore if not desired address family */

        myflags = 0;
        if ( (cptr = strchr(ifr->ifr_name, ':')) != NULL)
            *cptr = 0;      /* replace colon will null */
        if (strncmp(lastname, ifr->ifr_name, IFNAMSIZ) == 0) {
            if (doaliases == 0)
                continue;   /* already processed this interface */
            myflags = IFI_ALIAS;
        }
        memcpy(lastname, ifr->ifr_name, IFNAMSIZ);

        ifrcopy = *ifr;
        if (ioctl(sockfd, SIOCGIFFLAGS, &ifrcopy) < 0) {
            goto gotError;
        }
        
        flags = ifrcopy.ifr_flags;
        if ((flags & IFF_UP) == 0)
            continue;   /* ignore if interface not up */

        ifi = calloc(1, sizeof(struct ifi_info));
        if (ifi == NULL) {
            goto gotError;
        }
        *ifipnext = ifi;            /* prev points to this new one */
        ifipnext = &ifi->ifi_next;  /* pointer to next one goes here */

        ifi->ifi_flags = flags;     /* IFF_xxx values */
        ifi->ifi_myflags = myflags; /* IFI_xxx values */
        ifi->ifi_index = if_nametoindex(ifr->ifr_name);
        memcpy(ifi->ifi_name, ifr->ifr_name, IFI_NAME);
        ifi->ifi_name[IFI_NAME-1] = '\0';
/* end get_ifi_info2 */
/* include get_ifi_info3 */
        switch (ifr->ifr_addr.sa_family) {
        case AF_INET:
            sinptr = (struct sockaddr_in *) &ifr->ifr_addr;
            if (ifi->ifi_addr == NULL) {
                ifi->ifi_addr = calloc(1, sizeof(struct sockaddr_in));
                if (ifi->ifi_addr == NULL) {
                    goto gotError;
                }
                memcpy(ifi->ifi_addr, sinptr, sizeof(struct sockaddr_in));

#ifdef  SIOCGIFBRDADDR
                if (flags & IFF_BROADCAST) {
                    if (ioctl(sockfd, SIOCGIFBRDADDR, &ifrcopy) < 0) {
                        goto gotError;
                    }
                    sinptr = (struct sockaddr_in *) &ifrcopy.ifr_broadaddr;
                    ifi->ifi_brdaddr = calloc(1, sizeof(struct sockaddr_in));
                    if (ifi->ifi_brdaddr == NULL) {
                        goto gotError;
                    }
                    memcpy(ifi->ifi_brdaddr, sinptr, sizeof(struct sockaddr_in));
                }
#endif

#ifdef  SIOCGIFDSTADDR
                if (flags & IFF_POINTOPOINT) {
                    if (ioctl(sockfd, SIOCGIFDSTADDR, &ifrcopy) < 0) {
                        goto gotError;
                    }
                    sinptr = (struct sockaddr_in *) &ifrcopy.ifr_dstaddr;
                    ifi->ifi_dstaddr = calloc(1, sizeof(struct sockaddr_in));
                    if (ifi->ifi_dstaddr == NULL) {
                        goto gotError;
                    }
                    memcpy(ifi->ifi_dstaddr, sinptr, sizeof(struct sockaddr_in));
                }
#endif
            }
            break;

#if defined(AF_INET6) && defined(HAVE_IPV6)
        case AF_INET6:
            sinptr6 = (struct sockaddr_in6 *) &ifr->ifr_addr;
            if (ifi->ifi_addr == NULL) {
                ifi->ifi_addr = calloc(1, sizeof(struct sockaddr_in6));
                if (ifi->ifi_addr == NULL) {
                    goto gotError;
                }
                
                /* Some platforms (*BSD) inject the prefix in IPv6LL addresses */
                /* We need to strip that out */
                if (IN6_IS_ADDR_LINKLOCAL(&sinptr6->sin6_addr))
                	sinptr6->sin6_addr.__u6_addr.__u6_addr16[1] = 0;
                memcpy(ifi->ifi_addr, sinptr6, sizeof(struct sockaddr_in6));
            }
            break;
#endif

        default:
            break;
        }
    }
    goto done;
    
gotError:
    if (ifihead != NULL) {
        free_ifi_info(ifihead);
        ifihead = NULL;
    }

done:
    if (buf != NULL) {
        free(buf);
    }
    if (sockfd != -1) {
        junk = close(sockfd);
        assert(junk == 0);
    }
    return(ifihead);    /* pointer to first structure in linked list */
}
/* end get_ifi_info3 */

/* include free_ifi_info */
void
free_ifi_info(struct ifi_info *ifihead)
{
    struct ifi_info *ifi, *ifinext;

    for (ifi = ifihead; ifi != NULL; ifi = ifinext) {
        if (ifi->ifi_addr != NULL)
            free(ifi->ifi_addr);
        if (ifi->ifi_brdaddr != NULL)
            free(ifi->ifi_brdaddr);
        if (ifi->ifi_dstaddr != NULL)
            free(ifi->ifi_dstaddr);
        ifinext = ifi->ifi_next;    /* can't fetch ifi_next after free() */
        free(ifi);                  /* the ifi_info{} itself */
    }
}
/* end free_ifi_info */

ssize_t 
recvfrom_flags(int fd, void *ptr, size_t nbytes, int *flagsp,
               struct sockaddr *sa, socklen_t *salenptr, struct my_in_pktinfo *pktp)
{
    struct msghdr   msg;
    struct iovec    iov[1];
    ssize_t         n;

#ifdef CMSG_FIRSTHDR
    struct cmsghdr  *cmptr;
    union {
      struct cmsghdr    cm;
      char              control[1024];
    } control_un;

    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);
    msg.msg_flags = 0;
#else
    memset(&msg, 0, sizeof(msg));   /* make certain msg_accrightslen = 0 */
#endif /* CMSG_FIRSTHDR */

    msg.msg_name = (void *) sa;
    msg.msg_namelen = *salenptr;
    iov[0].iov_base = ptr;
    iov[0].iov_len = nbytes;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    if ( (n = recvmsg(fd, &msg, *flagsp)) < 0)
        return(n);

    *salenptr = msg.msg_namelen;    /* pass back results */
    if (pktp) {
        /* 0.0.0.0, i/f = -1 */
        /* We set the interface to -1 so that the caller can 
           tell whether we returned a meaningful value or 
           just some default.  Previously this code just 
           set the value to 0, but I'm concerned that 0 
           might be a valid interface value.
        */
        memset(pktp, 0, sizeof(struct my_in_pktinfo));
        pktp->ipi_ifindex = -1;
    }
/* end recvfrom_flags1 */

/* include recvfrom_flags2 */
#ifndef CMSG_FIRSTHDR
	#warning CMSG_FIRSTHDR not defined. Will not be able to determine destination address, received interface, etc.
    *flagsp = 0;                    /* pass back results */
    return(n);
#else

    *flagsp = msg.msg_flags;        /* pass back results */
    if (msg.msg_controllen < (socklen_t)sizeof(struct cmsghdr) ||
        (msg.msg_flags & MSG_CTRUNC) || pktp == NULL)
        return(n);

    for (cmptr = CMSG_FIRSTHDR(&msg); cmptr != NULL;
         cmptr = CMSG_NXTHDR(&msg, cmptr)) {

#ifdef  IP_PKTINFO
#if in_pktinfo_definition_is_missing
struct in_pktinfo
{
        int             ipi_ifindex;
        struct in_addr  ipi_spec_dst;
        struct in_addr  ipi_addr;
};
#endif
        if (cmptr->cmsg_level == IPPROTO_IP && 
            cmptr->cmsg_type == IP_PKTINFO) {
            struct in_pktinfo *tmp;
            struct sockaddr_in *sin = (struct sockaddr_in*)&pktp->ipi_addr;
            
            tmp = (struct in_pktinfo *) CMSG_DATA(cmptr);
            sin->sin_family = AF_INET;
            sin->sin_addr = tmp->ipi_addr;
            sin->sin_port = 0;
            pktp->ipi_ifindex = tmp->ipi_ifindex;
            continue;
        }
#endif

#ifdef  IP_RECVDSTADDR
        if (cmptr->cmsg_level == IPPROTO_IP &&
            cmptr->cmsg_type == IP_RECVDSTADDR) {
            struct sockaddr_in *sin = (struct sockaddr_in*)&pktp->ipi_addr;
            
            sin->sin_family = AF_INET;
            sin->sin_addr = *(struct in_addr*)CMSG_DATA(cmptr);
            sin->sin_port = 0;
            continue;
        }
#endif

#ifdef  IP_RECVIF
        if (cmptr->cmsg_level == IPPROTO_IP &&
            cmptr->cmsg_type == IP_RECVIF) {
            struct sockaddr_dl  *sdl = (struct sockaddr_dl *) CMSG_DATA(cmptr);
            int nameLen = (sdl->sdl_nlen < IFI_NAME - 1) ? sdl->sdl_nlen : (IFI_NAME - 1);
            pktp->ipi_ifindex = sdl->sdl_index;
#ifndef HAVE_BROKEN_RECVIF_NAME
            strncpy(pktp->ipi_ifname, sdl->sdl_data, nameLen);
#endif
            assert(pktp->ipi_ifname[IFI_NAME - 1] == 0);
            // null terminated because of memset above
            continue;
        }
#endif

#if defined(IPV6_PKTINFO) && defined(HAVE_IPV6)
        if (cmptr->cmsg_level == IPPROTO_IPV6 && 
            cmptr->cmsg_type == IPV6_PKTINFO) {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)&pktp->ipi_addr;
			struct in6_pktinfo *ip6_info = (struct in6_pktinfo*)CMSG_DATA(cmptr);
			
            sin6->sin6_family   = AF_INET6;
            sin6->sin6_len      = sizeof(*sin6);
            sin6->sin6_addr     = ip6_info->ipi6_addr;
            sin6->sin6_flowinfo = 0;
            sin6->sin6_scope_id = 0;
            sin6->sin6_port     = 0;
			pktp->ipi_ifindex   = ip6_info->ipi6_ifindex;
            continue;
        }
#endif
        assert(0);  // unknown ancillary data
    }
    return(n);
#endif /* CMSG_FIRSTHDR */
}
