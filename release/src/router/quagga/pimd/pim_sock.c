/*
  PIM for Quagga
  Copyright (C) 2008  Everton da Silva Marques

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING; if not, write to the
  Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
  MA 02110-1301 USA
  
  $QuaggaId: $Format:%an, %ai, %h$ $
*/

#include "pim_mroute.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/igmp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

#include <zebra.h>
#include "log.h"
#include "privs.h"

#include "pimd.h"
#include "pim_sock.h"
#include "pim_str.h"
#include "pim_igmp_join.h"

/* GLOBAL VARS */
extern struct zebra_privs_t pimd_privs;

int pim_socket_raw(int protocol)
{
  int fd;

  if ( pimd_privs.change (ZPRIVS_RAISE) )
       zlog_err ("pim_sockek_raw: could not raise privs, %s",
                 safe_strerror (errno) );

  fd = socket(AF_INET, SOCK_RAW, protocol);

  if ( pimd_privs.change (ZPRIVS_LOWER) )
       zlog_err ("pim_socket_raw: could not lower privs, %s",
                 safe_strerror (errno) );

  if (fd < 0) {
    zlog_warn("Could not create raw socket: errno=%d: %s",
	      errno, safe_strerror(errno));
    return PIM_SOCK_ERR_SOCKET;
  }
  
  return fd;
}

int pim_socket_mcast(int protocol, struct in_addr ifaddr, int loop)
{
  int fd;

  fd = pim_socket_raw(protocol);
  if (fd < 0) {
    zlog_warn("Could not create multicast socket: errno=%d: %s",
	      errno, safe_strerror(errno));
    return PIM_SOCK_ERR_SOCKET;
  }

  /* Needed to obtain destination address from recvmsg() */
  {
#if defined(HAVE_IP_PKTINFO)
    /* Linux IP_PKTINFO */
    int opt = 1;
    if (setsockopt(fd, SOL_IP, IP_PKTINFO, &opt, sizeof(opt))) {
      zlog_warn("Could not set IP_PKTINFO on socket fd=%d: errno=%d: %s",
		fd, errno, safe_strerror(errno));
    }
#elif defined(HAVE_IP_RECVDSTADDR)
    /* BSD IP_RECVDSTADDR */
    int opt = 1;
    if (setsockopt(fd, IPPROTO_IP, IP_RECVDSTADDR, &opt, sizeof(opt))) {
      zlog_warn("Could not set IP_RECVDSTADDR on socket fd=%d: errno=%d: %s",
		fd, errno, safe_strerror(errno));
    }
#else
    zlog_err("%s %s: Missing IP_PKTINFO and IP_RECVDSTADDR: unable to get dst addr from recvmsg()",
	     __FILE__, __PRETTY_FUNCTION__);
    close(fd);
    return PIM_SOCK_ERR_DSTADDR;
#endif
  }

  
  /* Set router alert (RFC 2113) for all IGMP messages (RFC 3376 4. Message Formats)*/
  if (protocol == IPPROTO_IGMP) {
    char ra[4];
    ra[0] = 148;
    ra[1] = 4;
    ra[2] = 0;
    ra[3] = 0;
    if (setsockopt(fd, IPPROTO_IP, IP_OPTIONS, ra, 4)) {
      zlog_warn("Could not set Router Alert Option on socket fd=%d: errno=%d: %s",
		fd, errno, safe_strerror(errno));
      close(fd);
      return PIM_SOCK_ERR_RA;
    }
  }

  {
    int reuse = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
		   (void *) &reuse, sizeof(reuse))) {
      zlog_warn("Could not set Reuse Address Option on socket fd=%d: errno=%d: %s",
		fd, errno, safe_strerror(errno));
      close(fd);
      return PIM_SOCK_ERR_REUSE;
    }
  }

  {
    const int MTTL = 1;
    int ttl = MTTL;
    if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL,
		   (void *) &ttl, sizeof(ttl))) {
      zlog_warn("Could not set multicast TTL=%d on socket fd=%d: errno=%d: %s",
		MTTL, fd, errno, safe_strerror(errno));
      close(fd);
      return PIM_SOCK_ERR_TTL;
    }
  }

  if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP,
		 (void *) &loop, sizeof(loop))) {
    zlog_warn("Could not %s Multicast Loopback Option on socket fd=%d: errno=%d: %s",
	      loop ? "enable" : "disable",
	      fd, errno, safe_strerror(errno));
    close(fd);
    return PIM_SOCK_ERR_LOOP;
  }

  if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF,
		 (void *) &ifaddr, sizeof(ifaddr))) {
    zlog_warn("Could not set Outgoing Interface Option on socket fd=%d: errno=%d: %s",
	      fd, errno, safe_strerror(errno));
    close(fd);
    return PIM_SOCK_ERR_IFACE;
  }

  {
    long flags;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
      zlog_warn("Could not get fcntl(F_GETFL,O_NONBLOCK) on socket fd=%d: errno=%d: %s",
		fd, errno, safe_strerror(errno));
      close(fd);
      return PIM_SOCK_ERR_NONBLOCK_GETFL;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK)) {
      zlog_warn("Could not set fcntl(F_SETFL,O_NONBLOCK) on socket fd=%d: errno=%d: %s",
		fd, errno, safe_strerror(errno));
      close(fd);
      return PIM_SOCK_ERR_NONBLOCK_SETFL;
    }
  }

  return fd;
}

int pim_socket_join(int fd, struct in_addr group,
		    struct in_addr ifaddr, int ifindex)
{
  int ret;

#ifdef HAVE_STRUCT_IP_MREQN_IMR_IFINDEX
  struct ip_mreqn opt;
#else
  struct ip_mreq opt;
#endif

  opt.imr_multiaddr = group;

#ifdef HAVE_STRUCT_IP_MREQN_IMR_IFINDEX
  opt.imr_address   = ifaddr;
  opt.imr_ifindex   = ifindex;
#else
  opt.imr_interface = ifaddr;
#endif

  ret = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &opt, sizeof(opt));
  if (ret) {
    char group_str[100];
    char ifaddr_str[100];
    if (!inet_ntop(AF_INET, &group, group_str , sizeof(group_str)))
      sprintf(group_str, "<group?>");
    if (!inet_ntop(AF_INET, &ifaddr, ifaddr_str , sizeof(ifaddr_str)))
      sprintf(ifaddr_str, "<ifaddr?>");

    zlog_err("Failure socket joining fd=%d group %s on interface address %s: errno=%d: %s",
	     fd, group_str, ifaddr_str, errno, safe_strerror(errno));
    return ret;
  }

  if (PIM_DEBUG_TRACE) {
    char group_str[100];
    char ifaddr_str[100];
    if (!inet_ntop(AF_INET, &group, group_str , sizeof(group_str)))
      sprintf(group_str, "<group?>");
    if (!inet_ntop(AF_INET, &ifaddr, ifaddr_str , sizeof(ifaddr_str)))
      sprintf(ifaddr_str, "<ifaddr?>");

    zlog_debug("Socket fd=%d joined group %s on interface address %s",
	       fd, group_str, ifaddr_str);
  }

  return ret;
}

int pim_socket_join_source(int fd, int ifindex,
			   struct in_addr group_addr,
			   struct in_addr source_addr,
			   const char *ifname)
{
  if (pim_igmp_join_source(fd, ifindex, group_addr, source_addr)) {
    int e = errno;
    char group_str[100];
    char source_str[100];
    pim_inet4_dump("<grp?>", group_addr, group_str, sizeof(group_str));
    pim_inet4_dump("<src?>", source_addr, source_str, sizeof(source_str));
    zlog_warn("%s: setsockopt(fd=%d) failure for IGMP group %s source %s ifindex %d on interface %s: errno=%d: %s",
	      __PRETTY_FUNCTION__,
	      fd, group_str, source_str, ifindex, ifname,
	      e, safe_strerror(e));
    return -1;
  }

  return 0;
}

int pim_socket_recvfromto(int fd, uint8_t *buf, size_t len,
			  struct sockaddr_in *from, socklen_t *fromlen,
			  struct sockaddr_in *to, socklen_t *tolen,
			  int *ifindex)
{
  struct msghdr msgh;
  struct cmsghdr *cmsg;
  struct iovec iov;
  char cbuf[1000];
  int err;

  /*
   * IP_PKTINFO / IP_RECVDSTADDR don't yield sin_port.
   * Use getsockname() to get sin_port.
   */
  if (to) {
    struct sockaddr_in si;
    socklen_t si_len = sizeof(si);
    
    ((struct sockaddr_in *) to)->sin_family = AF_INET;

    if (pim_socket_getsockname(fd, (struct sockaddr *) &si, &si_len)) {
      ((struct sockaddr_in *) to)->sin_port        = ntohs(0);
      ((struct sockaddr_in *) to)->sin_addr.s_addr = ntohl(0);
    }
    else {
      ((struct sockaddr_in *) to)->sin_port = si.sin_port;
      ((struct sockaddr_in *) to)->sin_addr = si.sin_addr;
    }

    if (tolen) 
      *tolen = sizeof(si);
  }

  memset(&msgh, 0, sizeof(struct msghdr));
  iov.iov_base = buf;
  iov.iov_len  = len;
  msgh.msg_control = cbuf;
  msgh.msg_controllen = sizeof(cbuf);
  msgh.msg_name = from;
  msgh.msg_namelen = fromlen ? *fromlen : 0;
  msgh.msg_iov  = &iov;
  msgh.msg_iovlen = 1;
  msgh.msg_flags = 0;

  err = recvmsg(fd, &msgh, 0);
  if (err < 0)
    return err;

  if (fromlen)
    *fromlen = msgh.msg_namelen;

  for (cmsg = CMSG_FIRSTHDR(&msgh);
       cmsg != NULL;
       cmsg = CMSG_NXTHDR(&msgh,cmsg)) {

#ifdef HAVE_IP_PKTINFO
    if ((cmsg->cmsg_level == SOL_IP) && (cmsg->cmsg_type == IP_PKTINFO)) {
      struct in_pktinfo *i = (struct in_pktinfo *) CMSG_DATA(cmsg);
      if (to)
	((struct sockaddr_in *) to)->sin_addr = i->ipi_addr;
      if (tolen)
	*tolen = sizeof(struct sockaddr_in);
      if (ifindex)
	*ifindex = i->ipi_ifindex;

      if (to && PIM_DEBUG_PACKETS) {
	char to_str[100];
	pim_inet4_dump("<to?>", to->sin_addr, to_str, sizeof(to_str));
	zlog_debug("%s: HAVE_IP_PKTINFO to=%s,%d",
		   __PRETTY_FUNCTION__,
		   to_str, ntohs(to->sin_port));
      }

      break;
    }
#endif

#ifdef HAVE_IP_RECVDSTADDR
    if ((cmsg->cmsg_level == IPPROTO_IP) && (cmsg->cmsg_type == IP_RECVDSTADDR)) {
      struct in_addr *i = (struct in_addr *) CMSG_DATA(cmsg);
      if (to)
	((struct sockaddr_in *) to)->sin_addr = *i;
      if (tolen)
	*tolen = sizeof(struct sockaddr_in);

      if (to && PIM_DEBUG_PACKETS) {
	char to_str[100];
	pim_inet4_dump("<to?>", to->sin_addr, to_str, sizeof(to_str));
	zlog_debug("%s: HAVE_IP_RECVDSTADDR to=%s,%d",
		   __PRETTY_FUNCTION__,
		   to_str, ntohs(to->sin_port));
      }

      break;
    }
#endif

#if defined(HAVE_IP_RECVIF) && defined(CMSG_IFINDEX)
      if (cmsg->cmsg_type == IP_RECVIF)
	if (ifindex)
	  *ifindex = CMSG_IFINDEX(cmsg);
#endif

  } /* for (cmsg) */

  return err; /* len */
}

int pim_socket_mcastloop_get(int fd)
{
  int loop;
  socklen_t loop_len = sizeof(loop);
  
  if (getsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP,
		 &loop, &loop_len)) {
    int e = errno;
    zlog_warn("Could not get Multicast Loopback Option on socket fd=%d: errno=%d: %s",
	      fd, errno, safe_strerror(errno));
    errno = e;
    return PIM_SOCK_ERR_LOOP;
  }
  
  return loop;
}

int pim_socket_getsockname(int fd, struct sockaddr *name, socklen_t *namelen)
{
  if (getsockname(fd, name, namelen)) {
    int e = errno;
    zlog_warn("Could not get Socket Name for socket fd=%d: errno=%d: %s",
	      fd, errno, safe_strerror(errno));
    errno = e;
    return PIM_SOCK_ERR_NAME;
  }

  return PIM_SOCK_ERR_NONE;
}
