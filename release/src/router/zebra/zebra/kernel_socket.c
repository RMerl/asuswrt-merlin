/* Kernel communication using routing socket.
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

#include <zebra.h>

#include "if.h"
#include "prefix.h"
#include "sockunion.h"
#include "connected.h"
#include "memory.h"
#include "ioctl.h"
#include "log.h"
#include "str.h"
#include "table.h"
#include "rib.h"

#include "zebra/interface.h"
#include "zebra/zserv.h"
#include "zebra/debug.h"

/* Socket length roundup function. */
#define ROUNDUP(a) \
  ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

/* And this macro is wrapper for handling sa_len. */
#ifdef HAVE_SA_LEN
#define WRAPUP(X)   ROUNDUP(((struct sockaddr *)(X))->sa_len)
#else
#define WRAPUP(X)   ROUNDUP(sizeof (struct sockaddr))
#endif /* HAVE_SA_LEN */

/* Routing socket message types. */
struct message rtm_type_str[] =
{
  {RTM_ADD,      "RTM_ADD"},
  {RTM_DELETE,   "RTM_DELETE"},
  {RTM_CHANGE,   "RTM_CHANGE"},
  {RTM_GET,      "RTM_GET"},
  {RTM_LOSING,   "RTM_LOSING"},
  {RTM_REDIRECT, "RTM_REDIRECT"},
  {RTM_MISS,     "RTM_MISS"},
  {RTM_LOCK,     "RTM_LOCK"},
  {RTM_OLDADD,   "RTM_OLDADD"},
  {RTM_OLDDEL,   "RTM_OLDDEL"},
  {RTM_RESOLVE,  "RTM_RESOLVE"},
  {RTM_NEWADDR,  "RTM_NEWADDR"},
  {RTM_DELADDR,  "RTM_DELADDR"},
  {RTM_IFINFO,   "RTM_IFINFO"},
#ifdef RTM_OIFINFO
  {RTM_OIFINFO,   "RTM_OIFINFO"},
#endif /* RTM_OIFINFO */
#ifdef RTM_NEWMADDR
  {RTM_NEWMADDR, "RTM_NEWMADDR"},
#endif /* RTM_NEWMADDR */
#ifdef RTM_DELMADDR
  {RTM_DELMADDR, "RTM_DELMADDR"},
#endif /* RTM_DELMADDR */
#ifdef RTM_IFANNOUNCE
  {RTM_IFANNOUNCE, "RTM_IFANNOUNCE"},
#endif /* RTM_IFANNOUNCE */
  {0,            NULL}
};

struct message rtm_flag_str[] =
{
  {RTF_UP,        "UP"},
  {RTF_GATEWAY,   "GATEWAY"},
  {RTF_HOST,      "HOST"},
  {RTF_REJECT,    "REJECT"},
  {RTF_DYNAMIC,   "DYNAMIC"},
  {RTF_MODIFIED,  "MODIFIED"},
  {RTF_DONE,      "DONE"},
#ifdef RTF_MASK
  {RTF_MASK,      "MASK"},
#endif /* RTF_MASK */
  {RTF_CLONING,   "CLONING"},
  {RTF_XRESOLVE,  "XRESOLVE"},
  {RTF_LLINFO,    "LLINFO"},
  {RTF_STATIC,    "STATIC"},
  {RTF_BLACKHOLE, "BLACKHOLE"},
  {RTF_PROTO1,    "PROTO1"},
  {RTF_PROTO2,    "PROTO2"},
#ifdef RTF_PRCLONING
  {RTF_PRCLONING, "PRCLONING"},
#endif /* RTF_PRCLONING */
#ifdef RTF_WASCLONED
  {RTF_WASCLONED, "WASCLONED"},
#endif /* RTF_WASCLONED */
#ifdef RTF_PROTO3
  {RTF_PROTO3,    "PROTO3"},
#endif /* RTF_PROTO3 */
#ifdef RTF_PINNED
  {RTF_PINNED,    "PINNED"},
#endif /* RTF_PINNED */
#ifdef RTF_LOCAL
  {RTF_LOCAL,    "LOCAL"},
#endif /* RTF_LOCAL */
#ifdef RTF_BROADCAST
  {RTF_BROADCAST, "BROADCAST"},
#endif /* RTF_BROADCAST */
#ifdef RTF_MULTICAST
  {RTF_MULTICAST, "MULTICAST"},
#endif /* RTF_MULTICAST */
  {0,             NULL}
};

/* Kernel routing update socket. */
int routing_sock = -1;

/* Yes I'm checking ugly routing socket behavior. */
/* #define DEBUG */

/* Supported address family check. */
static int
af_check (int family)
{
  if (family == AF_INET)
    return 1;
#ifdef HAVE_IPV6
  if (family == AF_INET6)
    return 1;
#endif /* HAVE_IPV6 */
  return 0;
}

/* Dump routing table flag for debug purpose. */
void
rtm_flag_dump (int flag)
{
  struct message *mes;
  static char buf[BUFSIZ];

  buf[0] = '0';
  for (mes = rtm_flag_str; mes->key != 0; mes++)
    {
      if (mes->key & flag)
	{
	  strlcat (buf, mes->str, BUFSIZ);
	  strlcat (buf, " ", BUFSIZ);
	}
    }
  zlog_info ("Kernel: %s", buf);
}

#ifdef RTM_IFANNOUNCE
/* Interface adding function */
int
ifan_read (struct if_announcemsghdr *ifan)
{
  struct interface *ifp;

  ifp = if_lookup_by_index (ifan->ifan_index);
  if (ifp == NULL && ifan->ifan_what == IFAN_ARRIVAL)
    {
      /* Create Interface */
      ifp = if_get_by_name (ifan->ifan_name);
      ifp->ifindex = ifan->ifan_index;

      if_add_update (ifp);
    }
  else if (ifp != NULL && ifan->ifan_what == IFAN_DEPARTURE)
    {
      if_delete_update (ifp);
      if_delete (ifp);
    }

  if_get_flags (ifp);
  if_get_mtu (ifp);
  if_get_metric (ifp);

  if (IS_ZEBRA_DEBUG_KERNEL)
    zlog_info ("interface %s index %d", ifp->name, ifp->ifindex);

  return 0;
}
#endif /* RTM_IFANNOUNCE */

/* Interface adding function called from interface_list. */
int
ifm_read (struct if_msghdr *ifm)
{
  struct interface *ifp;
  struct sockaddr_dl *sdl = NULL;

  sdl = (struct sockaddr_dl *)(ifm + 1);

  /* Use sdl index. */
  ifp = if_lookup_by_index (ifm->ifm_index);

  if (ifp == NULL)
    {
      /* Check interface's address.*/
      if (! (ifm->ifm_addrs & RTA_IFP))
	{
	  zlog_warn ("There must be RTA_IFP address for ifindex %d\n",
		     ifm->ifm_index);
	  return -1;
	}

      ifp = if_create ();

      strncpy (ifp->name, sdl->sdl_data, sdl->sdl_nlen);
      ifp->ifindex = ifm->ifm_index;
      ifp->flags = ifm->ifm_flags;
#if defined(__bsdi__)
      if_kvm_get_mtu (ifp);
#else
      if_get_mtu (ifp);
#endif /* __bsdi__ */
      if_get_metric (ifp);

      /* Fetch hardware address. */
      if (sdl->sdl_family != AF_LINK)
	{
	  zlog_warn ("sockaddr_dl->sdl_family is not AF_LINK");
	  return -1;
	}
      memcpy (&ifp->sdl, sdl, sizeof (struct sockaddr_dl));

      if_add_update (ifp);
    }
  else
    {
      /* There is a case of promisc, allmulti flag modification. */
      if (if_is_up (ifp))
	{
	  ifp->flags = ifm->ifm_flags;
	  if (! if_is_up (ifp))
	    if_down (ifp);
	}
      else
	{
	  ifp->flags = ifm->ifm_flags;
	  if (if_is_up (ifp))
	    if_up (ifp);
	}
    }
  
#ifdef HAVE_NET_RT_IFLIST
  ifp->stats = ifm->ifm_data;
#endif /* HAVE_NET_RT_IFLIST */

  if (IS_ZEBRA_DEBUG_KERNEL)
    zlog_info ("interface %s index %d", ifp->name, ifp->ifindex);

  return 0;
}

/* Address read from struct ifa_msghdr. */
void
ifam_read_mesg (struct ifa_msghdr *ifm,
		union sockunion *addr,
		union sockunion *mask,
		union sockunion *dest)
{
  caddr_t pnt, end;

  pnt = (caddr_t)(ifm + 1);
  end = ((caddr_t)ifm) + ifm->ifam_msglen;

#define IFAMADDRGET(X,R) \
    if (ifm->ifam_addrs & (R)) \
      { \
        int len = WRAPUP(pnt); \
        if (((X) != NULL) && af_check (((struct sockaddr *)pnt)->sa_family)) \
          memcpy ((caddr_t)(X), pnt, len); \
        pnt += len; \
      }
#define IFAMMASKGET(X,R) \
    if (ifm->ifam_addrs & (R)) \
      { \
	int len = WRAPUP(pnt); \
        if ((X) != NULL) \
	  memcpy ((caddr_t)(X), pnt, len); \
	pnt += len; \
      }

  /* Be sure structure is cleared */
  memset (mask, 0, sizeof (union sockunion));
  memset (addr, 0, sizeof (union sockunion));
  memset (dest, 0, sizeof (union sockunion));

  /* We fetch each socket variable into sockunion. */
  IFAMADDRGET (NULL, RTA_DST);
  IFAMADDRGET (NULL, RTA_GATEWAY);
  IFAMMASKGET (mask, RTA_NETMASK);
  IFAMADDRGET (NULL, RTA_GENMASK);
  IFAMADDRGET (NULL, RTA_IFP);
  IFAMADDRGET (addr, RTA_IFA);
  IFAMADDRGET (NULL, RTA_AUTHOR);
  IFAMADDRGET (dest, RTA_BRD);

  /* Assert read up end point matches to end point */
  if (pnt != end)
    zlog_warn ("ifam_read() doesn't read all socket data");
}

/* Interface's address information get. */
int
ifam_read (struct ifa_msghdr *ifam)
{
  struct interface *ifp;
  union sockunion addr, mask, gate;

  /* Check does this interface exist or not. */
  ifp = if_lookup_by_index (ifam->ifam_index);
  if (ifp == NULL) 
    {
      zlog_warn ("no interface for index %d", ifam->ifam_index); 
      return -1;
    }

  /* Allocate and read address information. */
  ifam_read_mesg (ifam, &addr, &mask, &gate);

  /* Check interface flag for implicit up of the interface. */
  if_refresh (ifp);

  /* Add connected address. */
  switch (sockunion_family (&addr))
    {
    case AF_INET:
      if (ifam->ifam_type == RTM_NEWADDR)
	connected_add_ipv4 (ifp, 0, &addr.sin.sin_addr, 
			    ip_masklen (mask.sin.sin_addr),
			    &gate.sin.sin_addr, NULL);
      else
	connected_delete_ipv4 (ifp, 0, &addr.sin.sin_addr, 
			       ip_masklen (mask.sin.sin_addr),
			       &gate.sin.sin_addr, NULL);
      break;
#ifdef HAVE_IPV6
    case AF_INET6:
      /* Unset interface index from link-local address when IPv6 stack
	 is KAME. */
      if (IN6_IS_ADDR_LINKLOCAL (&addr.sin6.sin6_addr))
	SET_IN6_LINKLOCAL_IFINDEX (addr.sin6.sin6_addr, 0);

      if (ifam->ifam_type == RTM_NEWADDR)
	connected_add_ipv6 (ifp,
			    &addr.sin6.sin6_addr, 
			    ip6_masklen (mask.sin6.sin6_addr),
			    &gate.sin6.sin6_addr);
      else
	connected_delete_ipv6 (ifp,
			       &addr.sin6.sin6_addr, 
			       ip6_masklen (mask.sin6.sin6_addr),
			       &gate.sin6.sin6_addr);
      break;
#endif /* HAVE_IPV6 */
    default:
      /* Unsupported family silently ignore... */
      break;
    }
  return 0;
}

/* Interface function for reading kernel routing table information. */
int
rtm_read_mesg (struct rt_msghdr *rtm,
	       union sockunion *dest,
	       union sockunion *mask,
	       union sockunion *gate)
{
  caddr_t pnt, end;

  /* Pnt points out socket data start point. */
  pnt = (caddr_t)(rtm + 1);
  end = ((caddr_t)rtm) + rtm->rtm_msglen;

  /* rt_msghdr version check. */
  if (rtm->rtm_version != RTM_VERSION) 
      zlog (NULL, LOG_WARNING,
	      "Routing message version different %d should be %d."
	      "This may cause problem\n", rtm->rtm_version, RTM_VERSION);

#define RTMADDRGET(X,R) \
    if (rtm->rtm_addrs & (R)) \
      { \
	int len = WRAPUP (pnt); \
        if (((X) != NULL) && af_check (((struct sockaddr *)pnt)->sa_family)) \
	  memcpy ((caddr_t)(X), pnt, len); \
	pnt += len; \
      }
#define RTMMASKGET(X,R) \
    if (rtm->rtm_addrs & (R)) \
      { \
	int len = WRAPUP (pnt); \
        if ((X) != NULL) \
	  memcpy ((caddr_t)(X), pnt, len); \
	pnt += len; \
      }

  /* Be sure structure is cleared */
  memset (dest, 0, sizeof (union sockunion));
  memset (gate, 0, sizeof (union sockunion));
  memset (mask, 0, sizeof (union sockunion));

  /* We fetch each socket variable into sockunion. */
  RTMADDRGET (dest, RTA_DST);
  RTMADDRGET (gate, RTA_GATEWAY);
  RTMMASKGET (mask, RTA_NETMASK);
  RTMADDRGET (NULL, RTA_GENMASK);
  RTMADDRGET (NULL, RTA_IFP);
  RTMADDRGET (NULL, RTA_IFA);
  RTMADDRGET (NULL, RTA_AUTHOR);
  RTMADDRGET (NULL, RTA_BRD);

  /* If there is netmask information set it's family same as
     destination family*/
  if (rtm->rtm_addrs & RTA_NETMASK)
    mask->sa.sa_family = dest->sa.sa_family;

  /* Assert read up to the end of pointer. */
  if (pnt != end) 
      zlog (NULL, LOG_WARNING, "rtm_read() doesn't read all socket data.");

  return rtm->rtm_flags;
}

void
rtm_read (struct rt_msghdr *rtm)
{
  int flags;
  u_char zebra_flags;
  union sockunion dest, mask, gate;

  zebra_flags = 0;

  /* Discard self send message. */
  if (rtm->rtm_type != RTM_GET 
      && (rtm->rtm_pid == pid || rtm->rtm_pid == old_pid))
    return;

  /* Read destination and netmask and gateway from rtm message
     structure. */
  flags = rtm_read_mesg (rtm, &dest, &mask, &gate);

#ifdef RTF_CLONED	/*bsdi, netbsd 1.6*/
  if (flags & RTF_CLONED)
    return;
#endif
#ifdef RTF_WASCLONED	/*freebsd*/
  if (flags & RTF_WASCLONED)
    return;
#endif

  if ((rtm->rtm_type == RTM_ADD) && ! (flags & RTF_UP))
    return;

  /* This is connected route. */
  if (! (flags & RTF_GATEWAY))
      return;

  if (flags & RTF_PROTO1)
    SET_FLAG (zebra_flags, ZEBRA_FLAG_SELFROUTE);

  /* This is persistent route. */
  if (flags & RTF_STATIC)
    SET_FLAG (zebra_flags, ZEBRA_FLAG_STATIC);

  if (dest.sa.sa_family == AF_INET)
    {
      struct prefix_ipv4 p;

      p.family = AF_INET;
      p.prefix = dest.sin.sin_addr;
      if (flags & RTF_HOST)
	p.prefixlen = IPV4_MAX_PREFIXLEN;
      else
	p.prefixlen = ip_masklen (mask.sin.sin_addr);

      if (rtm->rtm_type == RTM_GET || rtm->rtm_type == RTM_ADD)
	rib_add_ipv4 (ZEBRA_ROUTE_KERNEL, zebra_flags, 
		      &p, &gate.sin.sin_addr, 0, 0, 0, 0);
      else
	rib_delete_ipv4 (ZEBRA_ROUTE_KERNEL, zebra_flags, 
		      &p, &gate.sin.sin_addr, 0, 0);
    }
#ifdef HAVE_IPV6
  if (dest.sa.sa_family == AF_INET6)
    {
      struct prefix_ipv6 p;
      unsigned int ifindex = 0;

      p.family = AF_INET6;
      p.prefix = dest.sin6.sin6_addr;
      if (flags & RTF_HOST)
	p.prefixlen = IPV6_MAX_PREFIXLEN;
      else
	p.prefixlen = ip6_masklen (mask.sin6.sin6_addr);

#ifdef KAME
      if (IN6_IS_ADDR_LINKLOCAL (&gate.sin6.sin6_addr))
	{
	  ifindex = IN6_LINKLOCAL_IFINDEX (gate.sin6.sin6_addr);
	  SET_IN6_LINKLOCAL_IFINDEX (gate.sin6.sin6_addr, 0);
	}
#endif /* KAME */

      if (rtm->rtm_type == RTM_GET || rtm->rtm_type == RTM_ADD)
	rib_add_ipv6 (ZEBRA_ROUTE_KERNEL, zebra_flags,
		      &p, &gate.sin6.sin6_addr, ifindex, 0);
      else
	rib_delete_ipv6 (ZEBRA_ROUTE_KERNEL, zebra_flags,
			 &p, &gate.sin6.sin6_addr, ifindex, 0);
    }
#endif /* HAVE_IPV6 */
}

/* Interface function for the kernel routing table updates.  Support
   for RTM_CHANGE will be needed. */
int
rtm_write (int message,
	   union sockunion *dest,
	   union sockunion *mask,
	   union sockunion *gate,
	   unsigned int index,
	   int zebra_flags,
	   int metric)
{
  int ret;
  caddr_t pnt;
  struct interface *ifp;
  struct sockaddr_in tmp_gate;
#ifdef HAVE_IPV6
  struct sockaddr_in6 tmp_gate6;
#endif /* HAVE_IPV6 */

  /* Sequencial number of routing message. */
  static int msg_seq = 0;

  /* Struct of rt_msghdr and buffer for storing socket's data. */
  struct 
  {
    struct rt_msghdr rtm;
    char buf[512];
  } msg;
  
  memset (&tmp_gate, 0, sizeof (struct sockaddr_in));
  tmp_gate.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
  tmp_gate.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_SIN_LEN */

#ifdef HAVE_IPV6
  memset (&tmp_gate6, 0, sizeof (struct sockaddr_in6));
  tmp_gate6.sin6_family = AF_INET6;
#ifdef SIN6_LEN
  tmp_gate6.sin6_len = sizeof (struct sockaddr_in6);
#endif /* SIN6_LEN */
#endif /* HAVE_IPV6 */

  if (routing_sock < 0)
    return ZEBRA_ERR_EPERM;

  /* Clear and set rt_msghdr values */
  memset (&msg, 0, sizeof (struct rt_msghdr));
  msg.rtm.rtm_version = RTM_VERSION;
  msg.rtm.rtm_type = message;
  msg.rtm.rtm_seq = msg_seq++;
  msg.rtm.rtm_addrs = RTA_DST;
  msg.rtm.rtm_addrs |= RTA_GATEWAY;
  msg.rtm.rtm_flags = RTF_UP;
  msg.rtm.rtm_index = index;

  if (metric != 0)
    {
      msg.rtm.rtm_rmx.rmx_hopcount = metric;
      msg.rtm.rtm_inits |= RTV_HOPCOUNT;
    }

  ifp = if_lookup_by_index (index);

  if (gate && message == RTM_ADD)
    msg.rtm.rtm_flags |= RTF_GATEWAY;

  if (! gate && message == RTM_ADD && ifp &&
      (ifp->flags & IFF_POINTOPOINT) == 0)
    msg.rtm.rtm_flags |= RTF_CLONING;

  /* If no protocol specific gateway is specified, use link
     address for gateway. */
  if (! gate)
    {
      if (!ifp)
        {
          zlog_warn ("no gateway found for interface index %d", index);
          return -1;
        }
      gate = (union sockunion *) & ifp->sdl;
    }

  if (mask)
    msg.rtm.rtm_addrs |= RTA_NETMASK;
  else if (message == RTM_ADD) 
    msg.rtm.rtm_flags |= RTF_HOST;

  /* Tagging route with flags */
  msg.rtm.rtm_flags |= (RTF_PROTO1);

  /* Additional flags. */
  if (zebra_flags & ZEBRA_FLAG_BLACKHOLE)
    msg.rtm.rtm_flags |= RTF_BLACKHOLE;

#ifdef HAVE_SIN_LEN
#define SOCKADDRSET(X,R) \
  if (msg.rtm.rtm_addrs & (R)) \
    { \
      int len = ROUNDUP ((X)->sa.sa_len); \
      memcpy (pnt, (caddr_t)(X), len); \
      pnt += len; \
    }
#else 
#define SOCKADDRSET(X,R) \
  if (msg.rtm.rtm_addrs & (R)) \
    { \
      int len = ROUNDUP (sizeof((X)->sa)); \
      memcpy (pnt, (caddr_t)(X), len); \
      pnt += len; \
    }
#endif /* HAVE_SIN_LEN */

  pnt = (caddr_t) msg.buf;

  /* Write each socket data into rtm message buffer */
  SOCKADDRSET (dest, RTA_DST);
  SOCKADDRSET (gate, RTA_GATEWAY);
  SOCKADDRSET (mask, RTA_NETMASK);

  msg.rtm.rtm_msglen = pnt - (caddr_t) &msg;

  ret = write (routing_sock, &msg, msg.rtm.rtm_msglen);

  if (ret != msg.rtm.rtm_msglen) 
    {
      if (errno == EEXIST) 
	return ZEBRA_ERR_RTEXIST;
      if (errno == ENETUNREACH)
	return ZEBRA_ERR_RTUNREACH;
      
      zlog_warn ("write : %s (%d)", strerror (errno), errno);
      return -1;
    }
  return 0;
}


#include "thread.h"
#include "zebra/zserv.h"

extern struct thread_master *master;

/* For debug purpose. */
void
rtmsg_debug (struct rt_msghdr *rtm)
{
  char *type = "Unknown";
  struct message *mes;

  for (mes = rtm_type_str; mes->str; mes++)
    if (mes->key == rtm->rtm_type)
      {
	type = mes->str;
	break;
      }

  zlog_info ("Kernel: Len: %d Type: %s", rtm->rtm_msglen, type);
  rtm_flag_dump (rtm->rtm_flags);
  zlog_info ("Kernel: message seq %d", rtm->rtm_seq);
  zlog_info ("Kernel: pid %d", rtm->rtm_pid);
}

/* This is pretty gross, better suggestions welcome -- mhandler */
#ifndef RTAX_MAX
#ifdef RTA_NUMBITS
#define RTAX_MAX	RTA_NUMBITS
#else
#define RTAX_MAX	8
#endif /* RTA_NUMBITS */
#endif /* RTAX_MAX */

/* Kernel routing table and interface updates via routing socket. */
int
kernel_read (struct thread *thread)
{
  int sock;
  int nbytes;
  struct rt_msghdr *rtm;

  union 
  {
    /* Routing information. */
    struct 
    {
      struct rt_msghdr rtm;
      struct sockaddr addr[RTAX_MAX];
    } r;

    /* Interface information. */
    struct
    {
      struct if_msghdr ifm;
      struct sockaddr addr[RTAX_MAX];
    } im;

    /* Interface address information. */
    struct
    {
      struct ifa_msghdr ifa;
      struct sockaddr addr[RTAX_MAX];
    } ia;

#ifdef RTM_IFANNOUNCE
    /* Interface arrival/departure */
    struct
    {
      struct if_announcemsghdr ifan;
      struct sockaddr addr[RTAX_MAX];
    } ian;
#endif /* RTM_IFANNOUNCE */

  } buf;

  /* Fetch routing socket. */
  sock = THREAD_FD (thread);

  nbytes= read (sock, &buf, sizeof buf);

  if (nbytes <= 0)
    {
      if (nbytes < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
	zlog_warn ("routing socket error: %s", strerror (errno));
      return 0;
    }

  thread_add_read (master, kernel_read, NULL, sock);

#ifdef DEBUG
  rtmsg_debug (&buf.r.rtm);
#endif /* DEBUG */

  rtm = &buf.r.rtm;

  switch (rtm->rtm_type)
    {
    case RTM_ADD:
    case RTM_DELETE:
      rtm_read (rtm);
      break;
    case RTM_IFINFO:
      ifm_read (&buf.im.ifm);
      break;
    case RTM_NEWADDR:
    case RTM_DELADDR:
      ifam_read (&buf.ia.ifa);
      break;
#ifdef RTM_IFANNOUNCE
    case RTM_IFANNOUNCE:
      ifan_read (&buf.ian.ifan);
      break;
#endif /* RTM_IFANNOUNCE */
    default:
      break;
    }
  return 0;
}

/* Make routing socket. */
void
routing_socket ()
{
  routing_sock = socket (AF_ROUTE, SOCK_RAW, 0);

  if (routing_sock < 0) 
    {
      zlog_warn ("Can't init kernel routing socket");
      return;
    }

  if (fcntl (routing_sock, F_SETFL, O_NONBLOCK) < 0) 
    zlog_warn ("Can't set O_NONBLOCK to routing socket");

  /* kernel_read needs rewrite. */
  thread_add_read (master, kernel_read, NULL, routing_sock);
}

/* Exported interface function.  This function simply calls
   routing_socket (). */
void
kernel_init ()
{
  routing_socket ();
}
