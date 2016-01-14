/* Router advertisement
 * Copyright (C) 1999 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#ifndef _ZEBRA_SOCKOPT_H
#define _ZEBRA_SOCKOPT_H

#include "sockunion.h"

extern int setsockopt_so_recvbuf (int sock, int size);
extern int setsockopt_so_sendbuf (const int sock, int size);
extern int getsockopt_so_sendbuf (const int sock);

#ifdef HAVE_IPV6
extern int setsockopt_ipv6_pktinfo (int, int);
extern int setsockopt_ipv6_checksum (int, int);
extern int setsockopt_ipv6_multicast_hops (int, int);
extern int setsockopt_ipv6_unicast_hops (int, int);
extern int setsockopt_ipv6_hoplimit (int, int);
extern int setsockopt_ipv6_multicast_loop (int, int);
extern int setsockopt_ipv6_tclass (int, int);
#endif /* HAVE_IPV6 */

/*
 * It is OK to reference in6_pktinfo here without a protecting #if
 * because this macro will only be used #if HAVE_IPV6, and in6_pktinfo
 * is not optional for HAVE_IPV6.
 */
#define SOPT_SIZE_CMSG_PKTINFO_IPV6() (sizeof (struct in6_pktinfo));

/*
 * Size defines for control messages used to get ifindex.  We define
 * values for each method, and define a macro that can be used by code
 * that is unaware of which method is in use.
 * These values are without any alignment needed (see CMSG_SPACE in RFC3542).
 */
#if defined (IP_PKTINFO)
/* Linux in_pktinfo. */
#define SOPT_SIZE_CMSG_PKTINFO_IPV4()  (CMSG_SPACE(sizeof (struct in_pktinfo)))
/* XXX This should perhaps be defined even if IP_PKTINFO is not. */
#define SOPT_SIZE_CMSG_PKTINFO(af) \
  ((af == AF_INET) ? SOPT_SIZE_CMSG_PKTINFO_IPV4() \
                   : SOPT_SIZE_CMSG_PKTINFO_IPV6()
#endif /* IP_PKTINFO */

#if defined (IP_RECVIF)
/* BSD/Solaris */

#if defined (SUNOS_5)
#define SOPT_SIZE_CMSG_RECVIF_IPV4()  (sizeof (uint_t))
#else
#define SOPT_SIZE_CMSG_RECVIF_IPV4()	(sizeof (struct sockaddr_dl))
#endif /* SUNOS_5 */
#endif /* IP_RECVIF */

/* SOPT_SIZE_CMSG_IFINDEX_IPV4 - portable type */
#if defined (SOPT_SIZE_CMSG_PKTINFO)
#define SOPT_SIZE_CMSG_IFINDEX_IPV4() SOPT_SIZE_CMSG_PKTINFO_IPV4()
#elif defined (SOPT_SIZE_CMSG_RECVIF_IPV4)
#define SOPT_SIZE_CMSG_IFINDEX_IPV4() SOPT_SIZE_CMSG_RECVIF_IPV4()
#else /* Nothing available */
#define SOPT_SIZE_CMSG_IFINDEX_IPV4() (sizeof (char *))
#endif /* SOPT_SIZE_CMSG_IFINDEX_IPV4 */

#define SOPT_SIZE_CMSG_IFINDEX(af) \
  (((af) == AF_INET) : SOPT_SIZE_CMSG_IFINDEX_IPV4() \
                    ? SOPT_SIZE_CMSG_PKTINFO_IPV6())

extern int setsockopt_ipv4_multicast_if(int sock,
			             unsigned int ifindex);
extern int setsockopt_ipv4_multicast(int sock, int optname,
                                     unsigned int mcast_addr,
			             unsigned int ifindex);
extern int setsockopt_ipv4_tos(int sock, int tos);

/* Ask for, and get, ifindex, by whatever method is supported. */
extern int setsockopt_ifindex (int, int, int);
extern int getsockopt_ifindex (int, struct msghdr *);

/* swab the fields in iph between the host order and system order expected 
 * for IP_HDRINCL.
 */
extern void sockopt_iphdrincl_swab_htosys (struct ip *iph);
extern void sockopt_iphdrincl_swab_systoh (struct ip *iph);

extern int sockopt_tcp_signature(int sock, union sockunion *su,
                                 const char *password);
#endif /*_ZEBRA_SOCKOPT_H */
