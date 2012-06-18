/*
 * Copyright (C) 2003 Yasuhiro Ohara
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
 * along with GNU Zebra; see the file COPYING.  If not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 * Boston, MA 02111-1307, USA.  
 */

#include <zebra.h>

#include "log.h"
#include "memory.h"
#include "sockunion.h"

#include "ospf6_proto.h"
#include "ospf6_network.h"

int  ospf6_sock;
struct in6_addr allspfrouters6;
struct in6_addr alldrouters6;

/* setsockopt ReUseAddr to on */
void
ospf6_set_reuseaddr ()
{
  u_int on = 0;
  if (setsockopt (ospf6_sock, SOL_SOCKET, SO_REUSEADDR, &on,
                  sizeof (u_int)) < 0)
    zlog_warn ("Network: set SO_REUSEADDR failed: %s", strerror (errno));
}

/* setsockopt MulticastLoop to off */
void
ospf6_reset_mcastloop ()
{
  u_int off = 0;
  if (setsockopt (ospf6_sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
                  &off, sizeof (u_int)) < 0)
    zlog_warn ("Network: reset IPV6_MULTICAST_LOOP failed: %s",
               strerror (errno));
}

void
ospf6_set_pktinfo ()
{
  u_int on = 1;
#ifdef IPV6_RECVPKTINFO	/*2292bis-01*/
  if (setsockopt (ospf6_sock, IPPROTO_IPV6, IPV6_RECVPKTINFO,
                  &on, sizeof (u_int)) < 0)
    zlog_warn ("Network: set IPV6_RECVPKTINFO failed: %s", strerror (errno));
#else /*RFC2292*/
  if (setsockopt (ospf6_sock, IPPROTO_IPV6, IPV6_PKTINFO,
                  &on, sizeof (u_int)) < 0)
    zlog_warn ("Network: set IPV6_PKTINFO failed: %s", strerror (errno));
#endif
}

void
ospf6_set_checksum ()
{
  int offset = 12;
#ifndef DISABLE_IPV6_CHECKSUM
  if (setsockopt (ospf6_sock, IPPROTO_IPV6, IPV6_CHECKSUM,
                  &offset, sizeof (offset)) < 0)
    zlog_warn ("Network: set IPV6_CHECKSUM failed: %s", strerror (errno));
#else
  zlog_warn ("Network: Don't set IPV6_CHECKSUM");
#endif /* DISABLE_IPV6_CHECKSUM */
}

/* Make ospf6d's server socket. */
int
ospf6_serv_sock ()
{
  ospf6_sock = socket (AF_INET6, SOCK_RAW, IPPROTO_OSPFIGP);
  if (ospf6_sock < 0)
    {
      zlog_warn ("Network: can't create OSPF6 socket.");
      return -1;
    }

  /* set socket options */
#if 1
  sockopt_reuseaddr (ospf6_sock);
#else
  ospf6_set_reuseaddr ();
#endif /*1*/
  ospf6_reset_mcastloop ();
  ospf6_set_pktinfo ();
  ospf6_set_checksum ();

  /* setup global in6_addr, allspf6 and alldr6 for later use */
  inet_pton (AF_INET6, ALLSPFROUTERS6, &allspfrouters6);
  inet_pton (AF_INET6, ALLDROUTERS6, &alldrouters6);

  return 0;
}

void
ospf6_join_allspfrouters (u_int ifindex)
{
  struct ipv6_mreq mreq6;
  int retval;

  assert (ifindex);
  mreq6.ipv6mr_interface = ifindex;
  memcpy (&mreq6.ipv6mr_multiaddr, &allspfrouters6,
          sizeof (struct in6_addr));

  retval = setsockopt (ospf6_sock, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                       &mreq6, sizeof (mreq6));

  if (retval < 0)
    zlog_err ("Network: Join AllSPFRouters on ifindex %d failed: %s",
              ifindex, strerror (errno));
#if 0
  else
    zlog_info ("Network: Join AllSPFRouters on ifindex %d", ifindex);
#endif
}

void
ospf6_leave_allspfrouters (u_int ifindex)
{
  struct ipv6_mreq mreq6;

  assert (ifindex);
  mreq6.ipv6mr_interface = ifindex;
  memcpy (&mreq6.ipv6mr_multiaddr, &allspfrouters6,
          sizeof (struct in6_addr));

  if (setsockopt (ospf6_sock, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
                  &mreq6, sizeof (mreq6)) < 0)
    zlog_warn ("Network: Leave AllSPFRouters on ifindex %d Failed: %s",
               ifindex, strerror (errno));
#if 0
  else
    zlog_info ("Network: Leave AllSPFRouters on ifindex %d", ifindex);
#endif
}

void
ospf6_join_alldrouters (u_int ifindex)
{
  struct ipv6_mreq mreq6;

  assert (ifindex);
  mreq6.ipv6mr_interface = ifindex;
  memcpy (&mreq6.ipv6mr_multiaddr, &alldrouters6,
          sizeof (struct in6_addr));

  if (setsockopt (ospf6_sock, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                  &mreq6, sizeof (mreq6)) < 0)
    zlog_warn ("Network: Join AllDRouters on ifindex %d Failed: %s",
               ifindex, strerror (errno));
#if 0
  else
    zlog_info ("Network: Join AllDRouters on ifindex %d", ifindex);
#endif
}

void
ospf6_leave_alldrouters (u_int ifindex)
{
  struct ipv6_mreq mreq6;

  assert (ifindex);
  mreq6.ipv6mr_interface = ifindex;
  memcpy (&mreq6.ipv6mr_multiaddr, &alldrouters6,
          sizeof (struct in6_addr));

  if (setsockopt (ospf6_sock, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
                  &mreq6, sizeof (mreq6)) < 0)
    zlog_warn ("Network: Leave AllDRouters on ifindex %d Failed", ifindex);
#if 0
  else
    zlog_info ("Network: Leave AllDRouters on ifindex %d", ifindex);
#endif
}

int
iov_count (struct iovec *iov)
{
  int i;
  for (i = 0; iov[i].iov_base; i++)
    ;
  return i;
}

int
iov_totallen (struct iovec *iov)
{
  int i;
  int totallen = 0;
  for (i = 0; iov[i].iov_base; i++)
    totallen += iov[i].iov_len;
  return totallen;
}

int
ospf6_sendmsg (struct in6_addr *src, struct in6_addr *dst,
               unsigned int *ifindex, struct iovec *message)
{
  int retval;
  struct msghdr smsghdr;
  struct cmsghdr *scmsgp;
  u_char cmsgbuf[CMSG_SPACE(sizeof (struct in6_pktinfo))];
  struct in6_pktinfo *pktinfo;
  struct sockaddr_in6 dst_sin6;

  assert (dst);
  assert (*ifindex);

  scmsgp = (struct cmsghdr *)cmsgbuf;
  pktinfo = (struct in6_pktinfo *)(CMSG_DATA(scmsgp));
  memset (&dst_sin6, 0, sizeof (struct sockaddr_in6));

  /* source address */
  pktinfo->ipi6_ifindex = *ifindex;
  if (src)
    memcpy (&pktinfo->ipi6_addr, src, sizeof (struct in6_addr));
  else
    memset (&pktinfo->ipi6_addr, 0, sizeof (struct in6_addr));

  /* destination address */
  dst_sin6.sin6_family = AF_INET6;
#ifdef SIN6_LEN
  dst_sin6.sin6_len = sizeof (struct sockaddr_in6);
#endif /*SIN6_LEN*/
  memcpy (&dst_sin6.sin6_addr, dst, sizeof (struct in6_addr));
#ifdef HAVE_SIN6_SCOPE_ID
  dst_sin6.sin6_scope_id = *ifindex;
#endif

  /* send control msg */
  scmsgp->cmsg_level = IPPROTO_IPV6;
  scmsgp->cmsg_type = IPV6_PKTINFO;
  scmsgp->cmsg_len = CMSG_LEN (sizeof (struct in6_pktinfo));
  /* scmsgp = CMSG_NXTHDR (&smsghdr, scmsgp); */

  /* send msg hdr */
  smsghdr.msg_iov = message;
  smsghdr.msg_iovlen = iov_count (message);
  smsghdr.msg_name = (caddr_t) &dst_sin6;
  smsghdr.msg_namelen = sizeof (struct sockaddr_in6);
  smsghdr.msg_control = (caddr_t) cmsgbuf;
  smsghdr.msg_controllen = sizeof (cmsgbuf);

  retval = sendmsg (ospf6_sock, &smsghdr, 0);
  if (retval != iov_totallen (message))
    zlog_warn ("sendmsg failed: ifindex: %d: %s (%d)",
               *ifindex, strerror (errno), errno);

  return retval;
}

int
ospf6_recvmsg (struct in6_addr *src, struct in6_addr *dst,
               unsigned int *ifindex, struct iovec *message)
{
  int retval;
  struct msghdr rmsghdr;
  struct cmsghdr *rcmsgp;
  u_char cmsgbuf[CMSG_SPACE(sizeof (struct in6_pktinfo))];
  struct in6_pktinfo *pktinfo;
  struct sockaddr_in6 src_sin6;

  rcmsgp = (struct cmsghdr *)cmsgbuf;
  pktinfo = (struct in6_pktinfo *)(CMSG_DATA(rcmsgp));
  memset (&src_sin6, 0, sizeof (struct sockaddr_in6));

  /* receive control msg */
  rcmsgp->cmsg_level = IPPROTO_IPV6;
  rcmsgp->cmsg_type = IPV6_PKTINFO;
  rcmsgp->cmsg_len = CMSG_LEN (sizeof (struct in6_pktinfo));
  /* rcmsgp = CMSG_NXTHDR (&rmsghdr, rcmsgp); */

  /* receive msg hdr */
  rmsghdr.msg_iov = message;
  rmsghdr.msg_iovlen = iov_count (message);
  rmsghdr.msg_name = (caddr_t) &src_sin6;
  rmsghdr.msg_namelen = sizeof (struct sockaddr_in6);
  rmsghdr.msg_control = (caddr_t) cmsgbuf;
  rmsghdr.msg_controllen = sizeof (cmsgbuf);

  retval = recvmsg (ospf6_sock, &rmsghdr, 0);
  if (retval < 0)
    zlog_warn ("recvmsg failed: %s", strerror (errno));
  else if (retval == iov_totallen (message))
    zlog_warn ("recvmsg read full buffer size: %d", retval);

  /* source address */
  assert (src);
  memcpy (src, &src_sin6.sin6_addr, sizeof (struct in6_addr));

  /* destination address */
  if (ifindex)
    *ifindex = pktinfo->ipi6_ifindex;
  if (dst)
    memcpy (dst, &pktinfo->ipi6_addr, sizeof (struct in6_addr));

  return retval;
}


