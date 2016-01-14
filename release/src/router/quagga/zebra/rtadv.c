/* Router advertisement
 * Copyright (C) 2005 6WIND <jean-mickael.guerin@6wind.com>
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

#include "memory.h"
#include "sockopt.h"
#include "thread.h"
#include "if.h"
#include "log.h"
#include "prefix.h"
#include "linklist.h"
#include "command.h"
#include "privs.h"

#include "zebra/interface.h"
#include "zebra/rtadv.h"
#include "zebra/debug.h"
#include "zebra/rib.h"
#include "zebra/zserv.h"

extern struct zebra_privs_t zserv_privs;

#if defined (HAVE_IPV6) && defined (RTADV)

#ifdef OPEN_BSD
#include <netinet/icmp6.h>
#endif

/* If RFC2133 definition is used. */
#ifndef IPV6_JOIN_GROUP
#define IPV6_JOIN_GROUP  IPV6_ADD_MEMBERSHIP 
#endif
#ifndef IPV6_LEAVE_GROUP
#define IPV6_LEAVE_GROUP IPV6_DROP_MEMBERSHIP 
#endif

#define ALLNODE   "ff02::1"
#define ALLROUTER "ff02::2"

extern struct zebra_t zebrad;

enum rtadv_event {RTADV_START, RTADV_STOP, RTADV_TIMER, 
		  RTADV_TIMER_MSEC, RTADV_READ};

static void rtadv_event (enum rtadv_event, int);

static int if_join_all_router (int, struct interface *);
static int if_leave_all_router (int, struct interface *);

/* Structure which hold status of router advertisement. */
struct rtadv
{
  int sock;

  int adv_if_count;
  int adv_msec_if_count;

  struct thread *ra_read;
  struct thread *ra_timer;
};

struct rtadv *rtadv = NULL;

static struct rtadv *
rtadv_new (void)
{
  return XCALLOC (MTYPE_TMP, sizeof (struct rtadv));
}

static int
rtadv_recv_packet (int sock, u_char *buf, int buflen,
		   struct sockaddr_in6 *from, unsigned int *ifindex,
		   int *hoplimit)
{
  int ret;
  struct msghdr msg;
  struct iovec iov;
  struct cmsghdr  *cmsgptr;
  struct in6_addr dst;

  char adata[1024];

  /* Fill in message and iovec. */
  msg.msg_name = (void *) from;
  msg.msg_namelen = sizeof (struct sockaddr_in6);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = (void *) adata;
  msg.msg_controllen = sizeof adata;
  iov.iov_base = buf;
  iov.iov_len = buflen;

  /* If recvmsg fail return minus value. */
  ret = recvmsg (sock, &msg, 0);
  if (ret < 0)
    return ret;

  for (cmsgptr = ZCMSG_FIRSTHDR(&msg); cmsgptr != NULL;
       cmsgptr = CMSG_NXTHDR(&msg, cmsgptr)) 
    {
      /* I want interface index which this packet comes from. */
      if (cmsgptr->cmsg_level == IPPROTO_IPV6 &&
	  cmsgptr->cmsg_type == IPV6_PKTINFO) 
	{
	  struct in6_pktinfo *ptr;
	  
	  ptr = (struct in6_pktinfo *) CMSG_DATA (cmsgptr);
	  *ifindex = ptr->ipi6_ifindex;
	  memcpy(&dst, &ptr->ipi6_addr, sizeof(ptr->ipi6_addr));
        }

      /* Incoming packet's hop limit. */
      if (cmsgptr->cmsg_level == IPPROTO_IPV6 &&
	  cmsgptr->cmsg_type == IPV6_HOPLIMIT)
	{
	  int *hoptr = (int *) CMSG_DATA (cmsgptr);
	  *hoplimit = *hoptr;
	}
    }
  return ret;
}

#define RTADV_MSG_SIZE 4096

/* Send router advertisement packet. */
static void
rtadv_send_packet (int sock, struct interface *ifp)
{
  struct msghdr msg;
  struct iovec iov;
  struct cmsghdr  *cmsgptr;
  struct in6_pktinfo *pkt;
  struct sockaddr_in6 addr;
#ifdef HAVE_STRUCT_SOCKADDR_DL
  struct sockaddr_dl *sdl;
#endif /* HAVE_STRUCT_SOCKADDR_DL */
  static void *adata = NULL;
  unsigned char buf[RTADV_MSG_SIZE];
  struct nd_router_advert *rtadv;
  int ret;
  int len = 0;
  struct zebra_if *zif;
  struct rtadv_prefix *rprefix;
  u_char all_nodes_addr[] = {0xff,0x02,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
  struct listnode *node;
  u_int16_t pkt_RouterLifetime;

  /*
   * Allocate control message bufffer.  This is dynamic because
   * CMSG_SPACE is not guaranteed not to call a function.  Note that
   * the size will be different on different architectures due to
   * differing alignment rules.
   */
  if (adata == NULL)
    {
      /* XXX Free on shutdown. */
      adata = malloc(CMSG_SPACE(sizeof(struct in6_pktinfo)));
	   
      if (adata == NULL)
	zlog_err("rtadv_send_packet: can't malloc control data\n");
    }

  /* Logging of packet. */
  if (IS_ZEBRA_DEBUG_PACKET)
    zlog_debug ("Router advertisement send to %s", ifp->name);

  /* Fill in sockaddr_in6. */
  memset (&addr, 0, sizeof (struct sockaddr_in6));
  addr.sin6_family = AF_INET6;
#ifdef SIN6_LEN
  addr.sin6_len = sizeof (struct sockaddr_in6);
#endif /* SIN6_LEN */
  addr.sin6_port = htons (IPPROTO_ICMPV6);
  IPV6_ADDR_COPY (&addr.sin6_addr, all_nodes_addr);

  /* Fetch interface information. */
  zif = ifp->info;

  /* Make router advertisement message. */
  rtadv = (struct nd_router_advert *) buf;

  rtadv->nd_ra_type = ND_ROUTER_ADVERT;
  rtadv->nd_ra_code = 0;
  rtadv->nd_ra_cksum = 0;

  rtadv->nd_ra_curhoplimit = 64;

  /* RFC4191: Default Router Preference is 0 if Router Lifetime is 0. */
  rtadv->nd_ra_flags_reserved =
    zif->rtadv.AdvDefaultLifetime == 0 ? 0 : zif->rtadv.DefaultPreference;
  rtadv->nd_ra_flags_reserved <<= 3;

  if (zif->rtadv.AdvManagedFlag)
    rtadv->nd_ra_flags_reserved |= ND_RA_FLAG_MANAGED;
  if (zif->rtadv.AdvOtherConfigFlag)
    rtadv->nd_ra_flags_reserved |= ND_RA_FLAG_OTHER;
  if (zif->rtadv.AdvHomeAgentFlag)
    rtadv->nd_ra_flags_reserved |= ND_RA_FLAG_HOME_AGENT;
  /* Note that according to Neighbor Discovery (RFC 4861 [18]),
   * AdvDefaultLifetime is by default based on the value of
   * MaxRtrAdvInterval.  AdvDefaultLifetime is used in the Router Lifetime
   * field of Router Advertisements.  Given that this field is expressed
   * in seconds, a small MaxRtrAdvInterval value can result in a zero
   * value for this field.  To prevent this, routers SHOULD keep
   * AdvDefaultLifetime in at least one second, even if the use of
   * MaxRtrAdvInterval would result in a smaller value. -- RFC6275, 7.5 */
  pkt_RouterLifetime = zif->rtadv.AdvDefaultLifetime != -1 ?
    zif->rtadv.AdvDefaultLifetime :
    MAX (1, 0.003 * zif->rtadv.MaxRtrAdvInterval);
  rtadv->nd_ra_router_lifetime = htons (pkt_RouterLifetime);
  rtadv->nd_ra_reachable = htonl (zif->rtadv.AdvReachableTime);
  rtadv->nd_ra_retransmit = htonl (0);

  len = sizeof (struct nd_router_advert);

  /* If both the Home Agent Preference and Home Agent Lifetime are set to
   * their default values specified above, this option SHOULD NOT be
   * included in the Router Advertisement messages sent by this home
   * agent. -- RFC6275, 7.4 */
  if
  (
    zif->rtadv.AdvHomeAgentFlag &&
    (zif->rtadv.HomeAgentPreference || zif->rtadv.HomeAgentLifetime != -1)
  )
    {
      struct nd_opt_homeagent_info *ndopt_hai = 
	(struct nd_opt_homeagent_info *)(buf + len);
      ndopt_hai->nd_opt_hai_type = ND_OPT_HA_INFORMATION;
      ndopt_hai->nd_opt_hai_len = 1;
      ndopt_hai->nd_opt_hai_reserved = 0;
      ndopt_hai->nd_opt_hai_preference = htons(zif->rtadv.HomeAgentPreference);
      /* 16-bit unsigned integer.  The lifetime associated with the home
       * agent in units of seconds.  The default value is the same as the
       * Router Lifetime, as specified in the main body of the Router
       * Advertisement.  The maximum value corresponds to 18.2 hours.  A
       * value of 0 MUST NOT be used. -- RFC6275, 7.5 */
      ndopt_hai->nd_opt_hai_lifetime = htons
      (
        zif->rtadv.HomeAgentLifetime != -1 ?
        zif->rtadv.HomeAgentLifetime :
        MAX (1, pkt_RouterLifetime) /* 0 is OK for RL, but not for HAL*/
      );
      len += sizeof(struct nd_opt_homeagent_info);
    }

  if (zif->rtadv.AdvIntervalOption)
    {
      struct nd_opt_adv_interval *ndopt_adv = 
	(struct nd_opt_adv_interval *)(buf + len);
      ndopt_adv->nd_opt_ai_type = ND_OPT_ADV_INTERVAL;
      ndopt_adv->nd_opt_ai_len = 1;
      ndopt_adv->nd_opt_ai_reserved = 0;
      ndopt_adv->nd_opt_ai_interval = htonl(zif->rtadv.MaxRtrAdvInterval);
      len += sizeof(struct nd_opt_adv_interval);
    }

  /* Fill in prefix. */
  for (ALL_LIST_ELEMENTS_RO (zif->rtadv.AdvPrefixList, node, rprefix))
    {
      struct nd_opt_prefix_info *pinfo;

      pinfo = (struct nd_opt_prefix_info *) (buf + len);

      pinfo->nd_opt_pi_type = ND_OPT_PREFIX_INFORMATION;
      pinfo->nd_opt_pi_len = 4;
      pinfo->nd_opt_pi_prefix_len = rprefix->prefix.prefixlen;

      pinfo->nd_opt_pi_flags_reserved = 0;
      if (rprefix->AdvOnLinkFlag)
	pinfo->nd_opt_pi_flags_reserved |= ND_OPT_PI_FLAG_ONLINK;
      if (rprefix->AdvAutonomousFlag)
	pinfo->nd_opt_pi_flags_reserved |= ND_OPT_PI_FLAG_AUTO;
      if (rprefix->AdvRouterAddressFlag)
	pinfo->nd_opt_pi_flags_reserved |= ND_OPT_PI_FLAG_RADDR;

      pinfo->nd_opt_pi_valid_time = htonl (rprefix->AdvValidLifetime);
      pinfo->nd_opt_pi_preferred_time = htonl (rprefix->AdvPreferredLifetime);
      pinfo->nd_opt_pi_reserved2 = 0;

      IPV6_ADDR_COPY (&pinfo->nd_opt_pi_prefix, &rprefix->prefix.prefix);

#ifdef DEBUG
      {
	u_char buf[INET6_ADDRSTRLEN];

	zlog_debug ("DEBUG %s", inet_ntop (AF_INET6, &pinfo->nd_opt_pi_prefix, 
	           buf, INET6_ADDRSTRLEN));

      }
#endif /* DEBUG */

      len += sizeof (struct nd_opt_prefix_info);
    }

  /* Hardware address. */
#ifdef HAVE_STRUCT_SOCKADDR_DL
  sdl = &ifp->sdl;
  if (sdl != NULL && sdl->sdl_alen != 0)
    {
      buf[len++] = ND_OPT_SOURCE_LINKADDR;

      /* Option length should be rounded up to next octet if
         the link address does not end on an octet boundary. */
      buf[len++] = (sdl->sdl_alen + 9) >> 3;

      memcpy (buf + len, LLADDR (sdl), sdl->sdl_alen);
      len += sdl->sdl_alen;

      /* Pad option to end on an octet boundary. */
      memset (buf + len, 0, -(sdl->sdl_alen + 2) & 0x7);
      len += -(sdl->sdl_alen + 2) & 0x7;
    }
#else
  if (ifp->hw_addr_len != 0)
    {
      buf[len++] = ND_OPT_SOURCE_LINKADDR;

      /* Option length should be rounded up to next octet if
         the link address does not end on an octet boundary. */
      buf[len++] = (ifp->hw_addr_len + 9) >> 3;

      memcpy (buf + len, ifp->hw_addr, ifp->hw_addr_len);
      len += ifp->hw_addr_len;

      /* Pad option to end on an octet boundary. */
      memset (buf + len, 0, -(ifp->hw_addr_len + 2) & 0x7);
      len += -(ifp->hw_addr_len + 2) & 0x7;
    }
#endif /* HAVE_STRUCT_SOCKADDR_DL */

  /* MTU */
  if (zif->rtadv.AdvLinkMTU)
    {
      struct nd_opt_mtu * opt = (struct nd_opt_mtu *) (buf + len);
      opt->nd_opt_mtu_type = ND_OPT_MTU;
      opt->nd_opt_mtu_len = 1;
      opt->nd_opt_mtu_reserved = 0;
      opt->nd_opt_mtu_mtu = htonl (zif->rtadv.AdvLinkMTU);
      len += sizeof (struct nd_opt_mtu);
    }

  msg.msg_name = (void *) &addr;
  msg.msg_namelen = sizeof (struct sockaddr_in6);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = (void *) adata;
  msg.msg_controllen = CMSG_SPACE(sizeof(struct in6_pktinfo));
  msg.msg_flags = 0;
  iov.iov_base = buf;
  iov.iov_len = len;

  cmsgptr = ZCMSG_FIRSTHDR(&msg);
  cmsgptr->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
  cmsgptr->cmsg_level = IPPROTO_IPV6;
  cmsgptr->cmsg_type = IPV6_PKTINFO;

  pkt = (struct in6_pktinfo *) CMSG_DATA (cmsgptr);
  memset (&pkt->ipi6_addr, 0, sizeof (struct in6_addr));
  pkt->ipi6_ifindex = ifp->ifindex;

  ret = sendmsg (sock, &msg, 0);
  if (ret < 0)
    {
      zlog_err ("rtadv_send_packet: sendmsg %d (%s)\n",
		errno, safe_strerror(errno));
    }
}

static int
rtadv_timer (struct thread *thread)
{
  struct listnode *node, *nnode;
  struct interface *ifp;
  struct zebra_if *zif;
  int period;

  rtadv->ra_timer = NULL;
  if (rtadv->adv_msec_if_count == 0)
    {
      period = 1000; /* 1 s */
      rtadv_event (RTADV_TIMER, 1 /* 1 s */);
    } 
  else
    {
      period = 10; /* 10 ms */
      rtadv_event (RTADV_TIMER_MSEC, 10 /* 10 ms */);
    }

  for (ALL_LIST_ELEMENTS (iflist, node, nnode, ifp))
    {
      if (if_is_loopback (ifp) || ! if_is_operative (ifp))
	continue;

      zif = ifp->info;

      if (zif->rtadv.AdvSendAdvertisements)
	{ 
	  zif->rtadv.AdvIntervalTimer -= period;
	  if (zif->rtadv.AdvIntervalTimer <= 0)
	    {
	      /* FIXME: using MaxRtrAdvInterval each time isn't what section
	         6.2.4 of RFC4861 tells to do. */
	      zif->rtadv.AdvIntervalTimer = zif->rtadv.MaxRtrAdvInterval;
	      rtadv_send_packet (rtadv->sock, ifp);
	    }
	}
    }
  return 0;
}

static void
rtadv_process_solicit (struct interface *ifp)
{
  zlog_info ("Router solicitation received on %s", ifp->name);

  rtadv_send_packet (rtadv->sock, ifp);
}

static void
rtadv_process_advert (void)
{
  zlog_info ("Router advertisement received");
}

static void
rtadv_process_packet (u_char *buf, unsigned int len, unsigned int ifindex, int hoplimit)
{
  struct icmp6_hdr *icmph;
  struct interface *ifp;
  struct zebra_if *zif;

  /* Interface search. */
  ifp = if_lookup_by_index (ifindex);
  if (ifp == NULL)
    {
      zlog_warn ("Unknown interface index: %d", ifindex);
      return;
    }

  if (if_is_loopback (ifp))
    return;

  /* Check interface configuration. */
  zif = ifp->info;
  if (! zif->rtadv.AdvSendAdvertisements)
    return;

  /* ICMP message length check. */
  if (len < sizeof (struct icmp6_hdr))
    {
      zlog_warn ("Invalid ICMPV6 packet length: %d", len);
      return;
    }

  icmph = (struct icmp6_hdr *) buf;

  /* ICMP message type check. */
  if (icmph->icmp6_type != ND_ROUTER_SOLICIT &&
      icmph->icmp6_type != ND_ROUTER_ADVERT)
    {
      zlog_warn ("Unwanted ICMPV6 message type: %d", icmph->icmp6_type);
      return;
    }

  /* Hoplimit check. */
  if (hoplimit >= 0 && hoplimit != 255)
    {
      zlog_warn ("Invalid hoplimit %d for router advertisement ICMP packet",
		 hoplimit);
      return;
    }

  /* Check ICMP message type. */
  if (icmph->icmp6_type == ND_ROUTER_SOLICIT)
    rtadv_process_solicit (ifp);
  else if (icmph->icmp6_type == ND_ROUTER_ADVERT)
    rtadv_process_advert ();

  return;
}

static int
rtadv_read (struct thread *thread)
{
  int sock;
  int len;
  u_char buf[RTADV_MSG_SIZE];
  struct sockaddr_in6 from;
  unsigned int ifindex = 0;
  int hoplimit = -1;

  sock = THREAD_FD (thread);
  rtadv->ra_read = NULL;

  /* Register myself. */
  rtadv_event (RTADV_READ, sock);

  len = rtadv_recv_packet (sock, buf, BUFSIZ, &from, &ifindex, &hoplimit);

  if (len < 0) 
    {
      zlog_warn ("router solicitation recv failed: %s.", safe_strerror (errno));
      return len;
    }

  rtadv_process_packet (buf, (unsigned)len, ifindex, hoplimit);

  return 0;
}

static int
rtadv_make_socket (void)
{
  int sock;
  int ret;
  struct icmp6_filter filter;

  if ( zserv_privs.change (ZPRIVS_RAISE) )
       zlog_err ("rtadv_make_socket: could not raise privs, %s",
                  safe_strerror (errno) );
                  
  sock = socket (AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);

  if ( zserv_privs.change (ZPRIVS_LOWER) )
       zlog_err ("rtadv_make_socket: could not lower privs, %s",
       			 safe_strerror (errno) );

  /* When we can't make ICMPV6 socket simply back.  Router
     advertisement feature will not be supported. */
  if (sock < 0)
    return -1;

  ret = setsockopt_ipv6_pktinfo (sock, 1);
  if (ret < 0)
    return ret;
  ret = setsockopt_ipv6_multicast_loop (sock, 0);
  if (ret < 0)
    return ret;
  ret = setsockopt_ipv6_unicast_hops (sock, 255);
  if (ret < 0)
    return ret;
  ret = setsockopt_ipv6_multicast_hops (sock, 255);
  if (ret < 0)
    return ret;
  ret = setsockopt_ipv6_hoplimit (sock, 1);
  if (ret < 0)
    return ret;

  ICMP6_FILTER_SETBLOCKALL(&filter);
  ICMP6_FILTER_SETPASS (ND_ROUTER_SOLICIT, &filter);
  ICMP6_FILTER_SETPASS (ND_ROUTER_ADVERT, &filter);

  ret = setsockopt (sock, IPPROTO_ICMPV6, ICMP6_FILTER, &filter,
		    sizeof (struct icmp6_filter));
  if (ret < 0)
    {
      zlog_info ("ICMP6_FILTER set fail: %s", safe_strerror (errno));
      return ret;
    }

  return sock;
}

static struct rtadv_prefix *
rtadv_prefix_new (void)
{
  return XCALLOC (MTYPE_RTADV_PREFIX, sizeof (struct rtadv_prefix));
}

static void
rtadv_prefix_free (struct rtadv_prefix *rtadv_prefix)
{
  XFREE (MTYPE_RTADV_PREFIX, rtadv_prefix);
}

static struct rtadv_prefix *
rtadv_prefix_lookup (struct list *rplist, struct prefix_ipv6 *p)
{
  struct listnode *node;
  struct rtadv_prefix *rprefix;

  for (ALL_LIST_ELEMENTS_RO (rplist, node, rprefix))
    if (prefix_same ((struct prefix *) &rprefix->prefix, (struct prefix *) p))
      return rprefix;
  return NULL;
}

static struct rtadv_prefix *
rtadv_prefix_get (struct list *rplist, struct prefix_ipv6 *p)
{
  struct rtadv_prefix *rprefix;
  
  rprefix = rtadv_prefix_lookup (rplist, p);
  if (rprefix)
    return rprefix;

  rprefix = rtadv_prefix_new ();
  memcpy (&rprefix->prefix, p, sizeof (struct prefix_ipv6));
  listnode_add (rplist, rprefix);

  return rprefix;
}

static void
rtadv_prefix_set (struct zebra_if *zif, struct rtadv_prefix *rp)
{
  struct rtadv_prefix *rprefix;
  
  rprefix = rtadv_prefix_get (zif->rtadv.AdvPrefixList, &rp->prefix);

  /* Set parameters. */
  rprefix->AdvValidLifetime = rp->AdvValidLifetime;
  rprefix->AdvPreferredLifetime = rp->AdvPreferredLifetime;
  rprefix->AdvOnLinkFlag = rp->AdvOnLinkFlag;
  rprefix->AdvAutonomousFlag = rp->AdvAutonomousFlag;
  rprefix->AdvRouterAddressFlag = rp->AdvRouterAddressFlag;
}

static int
rtadv_prefix_reset (struct zebra_if *zif, struct rtadv_prefix *rp)
{
  struct rtadv_prefix *rprefix;
  
  rprefix = rtadv_prefix_lookup (zif->rtadv.AdvPrefixList, &rp->prefix);
  if (rprefix != NULL)
    {
      listnode_delete (zif->rtadv.AdvPrefixList, (void *) rprefix);
      rtadv_prefix_free (rprefix);
      return 1;
    }
  else
    return 0;
}

DEFUN (ipv6_nd_suppress_ra,
       ipv6_nd_suppress_ra_cmd,
       "ipv6 nd suppress-ra",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Suppress Router Advertisement\n")
{
  struct interface *ifp;
  struct zebra_if *zif;

  ifp = vty->index;
  zif = ifp->info;

  if (if_is_loopback (ifp))
    {
      vty_out (vty, "Invalid interface%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (zif->rtadv.AdvSendAdvertisements)
    {
      zif->rtadv.AdvSendAdvertisements = 0;
      zif->rtadv.AdvIntervalTimer = 0;
      rtadv->adv_if_count--;

      if_leave_all_router (rtadv->sock, ifp);

      if (rtadv->adv_if_count == 0)
	rtadv_event (RTADV_STOP, 0);
    }

  return CMD_SUCCESS;
}

DEFUN (no_ipv6_nd_suppress_ra,
       no_ipv6_nd_suppress_ra_cmd,
       "no ipv6 nd suppress-ra",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Suppress Router Advertisement\n")
{
  struct interface *ifp;
  struct zebra_if *zif;

  ifp = vty->index;
  zif = ifp->info;

  if (if_is_loopback (ifp))
    {
      vty_out (vty, "Invalid interface%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (! zif->rtadv.AdvSendAdvertisements)
    {
      zif->rtadv.AdvSendAdvertisements = 1;
      zif->rtadv.AdvIntervalTimer = 0;
      rtadv->adv_if_count++;

      if_join_all_router (rtadv->sock, ifp);

      if (rtadv->adv_if_count == 1)
	rtadv_event (RTADV_START, rtadv->sock);
    }

  return CMD_SUCCESS;
}

DEFUN (ipv6_nd_ra_interval_msec,
       ipv6_nd_ra_interval_msec_cmd,
       "ipv6 nd ra-interval msec <70-1800000>",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Router Advertisement interval\n"
       "Router Advertisement interval in milliseconds\n")
{
  unsigned interval;
  struct interface *ifp = (struct interface *) vty->index;
  struct zebra_if *zif = ifp->info;

  VTY_GET_INTEGER_RANGE ("router advertisement interval", interval, argv[0], 70, 1800000);
  if ((zif->rtadv.AdvDefaultLifetime != -1 && interval > (unsigned)zif->rtadv.AdvDefaultLifetime * 1000))
  {
    vty_out (vty, "This ra-interval would conflict with configured ra-lifetime!%s", VTY_NEWLINE);
    return CMD_WARNING;
  }

  if (zif->rtadv.MaxRtrAdvInterval % 1000)
    rtadv->adv_msec_if_count--;

  if (interval % 1000)
    rtadv->adv_msec_if_count++;
  
  zif->rtadv.MaxRtrAdvInterval = interval;
  zif->rtadv.MinRtrAdvInterval = 0.33 * interval;
  zif->rtadv.AdvIntervalTimer = 0;

  return CMD_SUCCESS;
}

DEFUN (ipv6_nd_ra_interval,
       ipv6_nd_ra_interval_cmd,
       "ipv6 nd ra-interval <1-1800>",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Router Advertisement interval\n"
       "Router Advertisement interval in seconds\n")
{
  unsigned interval;
  struct interface *ifp = (struct interface *) vty->index;
  struct zebra_if *zif = ifp->info;

  VTY_GET_INTEGER_RANGE ("router advertisement interval", interval, argv[0], 1, 1800);
  if ((zif->rtadv.AdvDefaultLifetime != -1 && interval > (unsigned)zif->rtadv.AdvDefaultLifetime))
  {
    vty_out (vty, "This ra-interval would conflict with configured ra-lifetime!%s", VTY_NEWLINE);
    return CMD_WARNING;
  }

  if (zif->rtadv.MaxRtrAdvInterval % 1000)
    rtadv->adv_msec_if_count--;
	
  /* convert to milliseconds */
  interval = interval * 1000; 
	
  zif->rtadv.MaxRtrAdvInterval = interval;
  zif->rtadv.MinRtrAdvInterval = 0.33 * interval;
  zif->rtadv.AdvIntervalTimer = 0;

  return CMD_SUCCESS;
}

DEFUN (no_ipv6_nd_ra_interval,
       no_ipv6_nd_ra_interval_cmd,
       "no ipv6 nd ra-interval",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Router Advertisement interval\n")
{
  struct interface *ifp;
  struct zebra_if *zif;

  ifp = (struct interface *) vty->index;
  zif = ifp->info;

  if (zif->rtadv.MaxRtrAdvInterval % 1000)
    rtadv->adv_msec_if_count--;
  
  zif->rtadv.MaxRtrAdvInterval = RTADV_MAX_RTR_ADV_INTERVAL;
  zif->rtadv.MinRtrAdvInterval = RTADV_MIN_RTR_ADV_INTERVAL;
  zif->rtadv.AdvIntervalTimer = zif->rtadv.MaxRtrAdvInterval;

  return CMD_SUCCESS;
}

ALIAS (no_ipv6_nd_ra_interval,
       no_ipv6_nd_ra_interval_val_cmd,
       "no ipv6 nd ra-interval <1-1800>",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Router Advertisement interval\n")

ALIAS (no_ipv6_nd_ra_interval,
       no_ipv6_nd_ra_interval_msec_val_cmd,
       "no ipv6 nd ra-interval msec <1-1800000>",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Router Advertisement interval\n"
       "Router Advertisement interval in milliseconds\n")

DEFUN (ipv6_nd_ra_lifetime,
       ipv6_nd_ra_lifetime_cmd,
       "ipv6 nd ra-lifetime <0-9000>",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Router lifetime\n"
       "Router lifetime in seconds (0 stands for a non-default gw)\n")
{
  int lifetime;
  struct interface *ifp;
  struct zebra_if *zif;

  ifp = (struct interface *) vty->index;
  zif = ifp->info;

  VTY_GET_INTEGER_RANGE ("router lifetime", lifetime, argv[0], 0, 9000);

  /* The value to be placed in the Router Lifetime field
   * of Router Advertisements sent from the interface,
   * in seconds.  MUST be either zero or between
   * MaxRtrAdvInterval and 9000 seconds. -- RFC4861, 6.2.1 */
  if ((lifetime != 0 && lifetime * 1000 < zif->rtadv.MaxRtrAdvInterval))
    {
      vty_out (vty, "This ra-lifetime would conflict with configured ra-interval%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  zif->rtadv.AdvDefaultLifetime = lifetime;

  return CMD_SUCCESS;
}

DEFUN (no_ipv6_nd_ra_lifetime,
       no_ipv6_nd_ra_lifetime_cmd,
       "no ipv6 nd ra-lifetime",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Router lifetime\n")
{
  struct interface *ifp;
  struct zebra_if *zif;

  ifp = (struct interface *) vty->index;
  zif = ifp->info;

  zif->rtadv.AdvDefaultLifetime = -1;

  return CMD_SUCCESS;
}

ALIAS (no_ipv6_nd_ra_lifetime,
       no_ipv6_nd_ra_lifetime_val_cmd,
       "no ipv6 nd ra-lifetime <0-9000>",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Router lifetime\n"
       "Router lifetime in seconds (0 stands for a non-default gw)\n")

DEFUN (ipv6_nd_reachable_time,
       ipv6_nd_reachable_time_cmd,
       "ipv6 nd reachable-time <1-3600000>",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Reachable time\n"
       "Reachable time in milliseconds\n")
{
  struct interface *ifp = (struct interface *) vty->index;
  struct zebra_if *zif = ifp->info;
  VTY_GET_INTEGER_RANGE ("reachable time", zif->rtadv.AdvReachableTime, argv[0], 1, RTADV_MAX_REACHABLE_TIME);
  return CMD_SUCCESS;
}

DEFUN (no_ipv6_nd_reachable_time,
       no_ipv6_nd_reachable_time_cmd,
       "no ipv6 nd reachable-time",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Reachable time\n")
{
  struct interface *ifp;
  struct zebra_if *zif;

  ifp = (struct interface *) vty->index;
  zif = ifp->info;

  zif->rtadv.AdvReachableTime = 0;

  return CMD_SUCCESS;
}

ALIAS (no_ipv6_nd_reachable_time,
       no_ipv6_nd_reachable_time_val_cmd,
       "no ipv6 nd reachable-time <1-3600000>",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Reachable time\n"
       "Reachable time in milliseconds\n")

DEFUN (ipv6_nd_homeagent_preference,
       ipv6_nd_homeagent_preference_cmd,
       "ipv6 nd home-agent-preference <0-65535>",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Home Agent preference\n"
       "preference value (default is 0, least preferred)\n")
{
  struct interface *ifp = (struct interface *) vty->index;
  struct zebra_if *zif = ifp->info;
  VTY_GET_INTEGER_RANGE ("home agent preference", zif->rtadv.HomeAgentPreference, argv[0], 0, 65535);
  return CMD_SUCCESS;
}

DEFUN (no_ipv6_nd_homeagent_preference,
       no_ipv6_nd_homeagent_preference_cmd,
       "no ipv6 nd home-agent-preference",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Home Agent preference\n")
{
  struct interface *ifp;
  struct zebra_if *zif;

  ifp = (struct interface *) vty->index;
  zif = ifp->info;

  zif->rtadv.HomeAgentPreference = 0;

  return CMD_SUCCESS;
}

ALIAS (no_ipv6_nd_homeagent_preference,
       no_ipv6_nd_homeagent_preference_val_cmd,
       "no ipv6 nd home-agent-preference <0-65535>",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Home Agent preference\n"
       "preference value (default is 0, least preferred)\n")

DEFUN (ipv6_nd_homeagent_lifetime,
       ipv6_nd_homeagent_lifetime_cmd,
       "ipv6 nd home-agent-lifetime <0-65520>",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Home Agent lifetime\n"
       "Home Agent lifetime in seconds (0 to track ra-lifetime)\n")
{
  struct interface *ifp = (struct interface *) vty->index;
  struct zebra_if *zif = ifp->info;
  VTY_GET_INTEGER_RANGE ("home agent lifetime", zif->rtadv.HomeAgentLifetime, argv[0], 0, RTADV_MAX_HALIFETIME);
  return CMD_SUCCESS;
}

DEFUN (no_ipv6_nd_homeagent_lifetime,
       no_ipv6_nd_homeagent_lifetime_cmd,
       "no ipv6 nd home-agent-lifetime",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Home Agent lifetime\n")
{
  struct interface *ifp;
  struct zebra_if *zif;

  ifp = (struct interface *) vty->index;
  zif = ifp->info;

  zif->rtadv.HomeAgentLifetime = -1;

  return CMD_SUCCESS;
}

ALIAS (no_ipv6_nd_homeagent_lifetime,
       no_ipv6_nd_homeagent_lifetime_val_cmd,
       "no ipv6 nd home-agent-lifetime <0-65520>",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Home Agent lifetime\n"
       "Home Agent lifetime in seconds (0 to track ra-lifetime)\n")

DEFUN (ipv6_nd_managed_config_flag,
       ipv6_nd_managed_config_flag_cmd,
       "ipv6 nd managed-config-flag",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Managed address configuration flag\n")
{
  struct interface *ifp;
  struct zebra_if *zif;

  ifp = (struct interface *) vty->index;
  zif = ifp->info;

  zif->rtadv.AdvManagedFlag = 1;

  return CMD_SUCCESS;
}

DEFUN (no_ipv6_nd_managed_config_flag,
       no_ipv6_nd_managed_config_flag_cmd,
       "no ipv6 nd managed-config-flag",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Managed address configuration flag\n")
{
  struct interface *ifp;
  struct zebra_if *zif;

  ifp = (struct interface *) vty->index;
  zif = ifp->info;

  zif->rtadv.AdvManagedFlag = 0;

  return CMD_SUCCESS;
}

DEFUN (ipv6_nd_homeagent_config_flag,
       ipv6_nd_homeagent_config_flag_cmd,
       "ipv6 nd home-agent-config-flag",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Home Agent configuration flag\n")
{
  struct interface *ifp;
  struct zebra_if *zif;

  ifp = (struct interface *) vty->index;
  zif = ifp->info;

  zif->rtadv.AdvHomeAgentFlag = 1;

  return CMD_SUCCESS;
}

DEFUN (no_ipv6_nd_homeagent_config_flag,
       no_ipv6_nd_homeagent_config_flag_cmd,
       "no ipv6 nd home-agent-config-flag",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Home Agent configuration flag\n")
{
  struct interface *ifp;
  struct zebra_if *zif;

  ifp = (struct interface *) vty->index;
  zif = ifp->info;

  zif->rtadv.AdvHomeAgentFlag = 0;

  return CMD_SUCCESS;
}

DEFUN (ipv6_nd_adv_interval_config_option,
       ipv6_nd_adv_interval_config_option_cmd,
       "ipv6 nd adv-interval-option",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Advertisement Interval Option\n")
{
  struct interface *ifp;
  struct zebra_if *zif;

  ifp = (struct interface *) vty->index;
  zif = ifp->info;

  zif->rtadv.AdvIntervalOption = 1;

  return CMD_SUCCESS;
}

DEFUN (no_ipv6_nd_adv_interval_config_option,
       no_ipv6_nd_adv_interval_config_option_cmd,
       "no ipv6 nd adv-interval-option",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Advertisement Interval Option\n")
{
  struct interface *ifp;
  struct zebra_if *zif;

  ifp = (struct interface *) vty->index;
  zif = ifp->info;

  zif->rtadv.AdvIntervalOption = 0;

  return CMD_SUCCESS;
}

DEFUN (ipv6_nd_other_config_flag,
       ipv6_nd_other_config_flag_cmd,
       "ipv6 nd other-config-flag",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Other statefull configuration flag\n")
{
  struct interface *ifp;
  struct zebra_if *zif;

  ifp = (struct interface *) vty->index;
  zif = ifp->info;

  zif->rtadv.AdvOtherConfigFlag = 1;

  return CMD_SUCCESS;
}

DEFUN (no_ipv6_nd_other_config_flag,
       no_ipv6_nd_other_config_flag_cmd,
       "no ipv6 nd other-config-flag",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Other statefull configuration flag\n")
{
  struct interface *ifp;
  struct zebra_if *zif;

  ifp = (struct interface *) vty->index;
  zif = ifp->info;

  zif->rtadv.AdvOtherConfigFlag = 0;

  return CMD_SUCCESS;
}

DEFUN (ipv6_nd_prefix,
       ipv6_nd_prefix_cmd,
       "ipv6 nd prefix X:X::X:X/M (<0-4294967295>|infinite) "
       "(<0-4294967295>|infinite) (off-link|) (no-autoconfig|) (router-address|)",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Prefix information\n"
       "IPv6 prefix\n"
       "Valid lifetime in seconds\n"
       "Infinite valid lifetime\n"
       "Preferred lifetime in seconds\n"
       "Infinite preferred lifetime\n"
       "Do not use prefix for onlink determination\n"
       "Do not use prefix for autoconfiguration\n"
       "Set Router Address flag\n")
{
  int i;
  int ret;
  int cursor = 1;
  struct interface *ifp;
  struct zebra_if *zebra_if;
  struct rtadv_prefix rp;

  ifp = (struct interface *) vty->index;
  zebra_if = ifp->info;

  ret = str2prefix_ipv6 (argv[0], &rp.prefix);
  if (!ret)
    {
      vty_out (vty, "Malformed IPv6 prefix%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  apply_mask_ipv6 (&rp.prefix); /* RFC4861 4.6.2 */
  rp.AdvOnLinkFlag = 1;
  rp.AdvAutonomousFlag = 1;
  rp.AdvRouterAddressFlag = 0;
  rp.AdvValidLifetime = RTADV_VALID_LIFETIME;
  rp.AdvPreferredLifetime = RTADV_PREFERRED_LIFETIME;

  if (argc > 1)
    {
      if ((isdigit(argv[1][0])) || strncmp (argv[1], "i", 1) == 0)
	{
	  if ( strncmp (argv[1], "i", 1) == 0)
	    rp.AdvValidLifetime = UINT32_MAX;
	  else
	    rp.AdvValidLifetime = (u_int32_t) strtoll (argv[1],
		(char **)NULL, 10);
      
	  if ( strncmp (argv[2], "i", 1) == 0)
	    rp.AdvPreferredLifetime = UINT32_MAX;
	  else
	    rp.AdvPreferredLifetime = (u_int32_t) strtoll (argv[2],
		(char **)NULL, 10);

	  if (rp.AdvPreferredLifetime > rp.AdvValidLifetime)
	    {
	      vty_out (vty, "Invalid preferred lifetime%s", VTY_NEWLINE);
	      return CMD_WARNING;
	    }
	  cursor = cursor + 2;
	}
      if (argc > cursor)
	{
	  for (i = cursor; i < argc; i++)
	    {
	      if (strncmp (argv[i], "of", 2) == 0)
		rp.AdvOnLinkFlag = 0;
	      if (strncmp (argv[i], "no", 2) == 0)
		rp.AdvAutonomousFlag = 0;
	      if (strncmp (argv[i], "ro", 2) == 0)
		rp.AdvRouterAddressFlag = 1;
	    }
	}
    }

  rtadv_prefix_set (zebra_if, &rp);

  return CMD_SUCCESS;
}

ALIAS (ipv6_nd_prefix,
       ipv6_nd_prefix_val_nortaddr_cmd,
       "ipv6 nd prefix X:X::X:X/M (<0-4294967295>|infinite) "
       "(<0-4294967295>|infinite) (off-link|) (no-autoconfig|)",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Prefix information\n"
       "IPv6 prefix\n"
       "Valid lifetime in seconds\n"
       "Infinite valid lifetime\n"
       "Preferred lifetime in seconds\n"
       "Infinite preferred lifetime\n"
       "Do not use prefix for onlink determination\n"
       "Do not use prefix for autoconfiguration\n")

ALIAS (ipv6_nd_prefix,
       ipv6_nd_prefix_val_rev_cmd,
       "ipv6 nd prefix X:X::X:X/M (<0-4294967295>|infinite) "
       "(<0-4294967295>|infinite) (no-autoconfig|) (off-link|)",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Prefix information\n"
       "IPv6 prefix\n"
       "Valid lifetime in seconds\n"
       "Infinite valid lifetime\n"
       "Preferred lifetime in seconds\n"
       "Infinite preferred lifetime\n"
       "Do not use prefix for autoconfiguration\n"
       "Do not use prefix for onlink determination\n")

ALIAS (ipv6_nd_prefix,
       ipv6_nd_prefix_val_rev_rtaddr_cmd,
       "ipv6 nd prefix X:X::X:X/M (<0-4294967295>|infinite) "
       "(<0-4294967295>|infinite) (no-autoconfig|) (off-link|) (router-address|)",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Prefix information\n"
       "IPv6 prefix\n"
       "Valid lifetime in seconds\n"
       "Infinite valid lifetime\n"
       "Preferred lifetime in seconds\n"
       "Infinite preferred lifetime\n"
       "Do not use prefix for autoconfiguration\n"
       "Do not use prefix for onlink determination\n"
       "Set Router Address flag\n")

ALIAS (ipv6_nd_prefix,
       ipv6_nd_prefix_val_noauto_cmd,
       "ipv6 nd prefix X:X::X:X/M (<0-4294967295>|infinite) "
       "(<0-4294967295>|infinite) (no-autoconfig|)",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Prefix information\n"
       "IPv6 prefix\n"
       "Valid lifetime in seconds\n"
       "Infinite valid lifetime\n"
       "Preferred lifetime in seconds\n"
       "Infinite preferred lifetime\n"
       "Do not use prefix for autoconfiguration")

ALIAS (ipv6_nd_prefix,
       ipv6_nd_prefix_val_offlink_cmd,
       "ipv6 nd prefix X:X::X:X/M (<0-4294967295>|infinite) "
       "(<0-4294967295>|infinite) (off-link|)",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Prefix information\n"
       "IPv6 prefix\n"
       "Valid lifetime in seconds\n"
       "Infinite valid lifetime\n"
       "Preferred lifetime in seconds\n"
       "Infinite preferred lifetime\n"
       "Do not use prefix for onlink determination\n")

ALIAS (ipv6_nd_prefix,
       ipv6_nd_prefix_val_rtaddr_cmd,
       "ipv6 nd prefix X:X::X:X/M (<0-4294967295>|infinite) "
       "(<0-4294967295>|infinite) (router-address|)",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Prefix information\n"
       "IPv6 prefix\n"
       "Valid lifetime in seconds\n"
       "Infinite valid lifetime\n"
       "Preferred lifetime in seconds\n"
       "Infinite preferred lifetime\n"
       "Set Router Address flag\n")

ALIAS (ipv6_nd_prefix,
       ipv6_nd_prefix_val_cmd,
       "ipv6 nd prefix X:X::X:X/M (<0-4294967295>|infinite) "
       "(<0-4294967295>|infinite)",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Prefix information\n"
       "IPv6 prefix\n"
       "Valid lifetime in seconds\n"
       "Infinite valid lifetime\n"
       "Preferred lifetime in seconds\n"
       "Infinite preferred lifetime\n")

ALIAS (ipv6_nd_prefix,
       ipv6_nd_prefix_noval_cmd,
       "ipv6 nd prefix X:X::X:X/M (no-autoconfig|) (off-link|)",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Prefix information\n"
       "IPv6 prefix\n"
       "Do not use prefix for autoconfiguration\n"
       "Do not use prefix for onlink determination\n")

ALIAS (ipv6_nd_prefix,
       ipv6_nd_prefix_noval_rev_cmd,
       "ipv6 nd prefix X:X::X:X/M (off-link|) (no-autoconfig|)",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Prefix information\n"
       "IPv6 prefix\n"
       "Do not use prefix for onlink determination\n"
       "Do not use prefix for autoconfiguration\n")

ALIAS (ipv6_nd_prefix,
       ipv6_nd_prefix_noval_noauto_cmd,
       "ipv6 nd prefix X:X::X:X/M (no-autoconfig|)",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Prefix information\n"
       "IPv6 prefix\n"
       "Do not use prefix for autoconfiguration\n")

ALIAS (ipv6_nd_prefix,
       ipv6_nd_prefix_noval_offlink_cmd,
       "ipv6 nd prefix X:X::X:X/M (off-link|)",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Prefix information\n"
       "IPv6 prefix\n"
       "Do not use prefix for onlink determination\n")

ALIAS (ipv6_nd_prefix,
       ipv6_nd_prefix_noval_rtaddr_cmd,
       "ipv6 nd prefix X:X::X:X/M (router-address|)",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Prefix information\n"
       "IPv6 prefix\n"
       "Set Router Address flag\n")

ALIAS (ipv6_nd_prefix,
       ipv6_nd_prefix_prefix_cmd,
       "ipv6 nd prefix X:X::X:X/M",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Prefix information\n"
       "IPv6 prefix\n")

DEFUN (no_ipv6_nd_prefix,
       no_ipv6_nd_prefix_cmd,
       "no ipv6 nd prefix IPV6PREFIX",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Prefix information\n"
       "IPv6 prefix\n")
{
  int ret;
  struct interface *ifp;
  struct zebra_if *zebra_if;
  struct rtadv_prefix rp;

  ifp = (struct interface *) vty->index;
  zebra_if = ifp->info;

  ret = str2prefix_ipv6 (argv[0], &rp.prefix);
  if (!ret)
    {
      vty_out (vty, "Malformed IPv6 prefix%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  apply_mask_ipv6 (&rp.prefix); /* RFC4861 4.6.2 */

  ret = rtadv_prefix_reset (zebra_if, &rp);
  if (!ret)
    {
      vty_out (vty, "Non-exist IPv6 prefix%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  return CMD_SUCCESS;
}

DEFUN (ipv6_nd_router_preference,
       ipv6_nd_router_preference_cmd,
       "ipv6 nd router-preference (high|medium|low)",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Default router preference\n"
       "High default router preference\n"
       "Low default router preference\n"
       "Medium default router preference (default)\n")
{
  struct interface *ifp;
  struct zebra_if *zif;
  int i = 0;

  ifp = (struct interface *) vty->index;
  zif = ifp->info;

  while (0 != rtadv_pref_strs[i])
    {
      if (strncmp (argv[0], rtadv_pref_strs[i], 1) == 0)
	{
	  zif->rtadv.DefaultPreference = i;
	  return CMD_SUCCESS;
	}
      i++;
    }

  return CMD_ERR_NO_MATCH;
}

DEFUN (no_ipv6_nd_router_preference,
       no_ipv6_nd_router_preference_cmd,
       "no ipv6 nd router-preference",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Default router preference\n")
{
  struct interface *ifp;
  struct zebra_if *zif;

  ifp = (struct interface *) vty->index;
  zif = ifp->info;

  zif->rtadv.DefaultPreference = RTADV_PREF_MEDIUM; /* Default per RFC4191. */

  return CMD_SUCCESS;
}

ALIAS (no_ipv6_nd_router_preference,
       no_ipv6_nd_router_preference_val_cmd,
       "no ipv6 nd router-preference (high|medium|low)",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Default router preference\n"
       "High default router preference\n"
       "Low default router preference\n"
       "Medium default router preference (default)\n")

DEFUN (ipv6_nd_mtu,
       ipv6_nd_mtu_cmd,
       "ipv6 nd mtu <1-65535>",
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Advertised MTU\n"
       "MTU in bytes\n")
{
  struct interface *ifp = (struct interface *) vty->index;
  struct zebra_if *zif = ifp->info;
  VTY_GET_INTEGER_RANGE ("MTU", zif->rtadv.AdvLinkMTU, argv[0], 1, 65535);
  return CMD_SUCCESS;
}

DEFUN (no_ipv6_nd_mtu,
       no_ipv6_nd_mtu_cmd,
       "no ipv6 nd mtu",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Advertised MTU\n")
{
  struct interface *ifp = (struct interface *) vty->index;
  struct zebra_if *zif = ifp->info;
  zif->rtadv.AdvLinkMTU = 0;
  return CMD_SUCCESS;
}

ALIAS (no_ipv6_nd_mtu,
       no_ipv6_nd_mtu_val_cmd,
       "no ipv6 nd mtu <1-65535>",
       NO_STR
       "Interface IPv6 config commands\n"
       "Neighbor discovery\n"
       "Advertised MTU\n"
       "MTU in bytes\n")

/* Write configuration about router advertisement. */
void
rtadv_config_write (struct vty *vty, struct interface *ifp)
{
  struct zebra_if *zif;
  struct listnode *node;
  struct rtadv_prefix *rprefix;
  u_char buf[INET6_ADDRSTRLEN];
  int interval;

  if (! rtadv)
    return;

  zif = ifp->info;

  if (! if_is_loopback (ifp))
    {
      if (zif->rtadv.AdvSendAdvertisements)
	vty_out (vty, " no ipv6 nd suppress-ra%s", VTY_NEWLINE);
      else
	vty_out (vty, " ipv6 nd suppress-ra%s", VTY_NEWLINE);
    }

  
  interval = zif->rtadv.MaxRtrAdvInterval;
  if (interval % 1000)
    vty_out (vty, " ipv6 nd ra-interval msec %d%s", interval,
	     VTY_NEWLINE);
  else
    if (interval != RTADV_MAX_RTR_ADV_INTERVAL)
      vty_out (vty, " ipv6 nd ra-interval %d%s", interval / 1000,
	     VTY_NEWLINE);

  if (zif->rtadv.AdvIntervalOption)
    vty_out (vty, " ipv6 nd adv-interval-option%s", VTY_NEWLINE);

  if (zif->rtadv.AdvDefaultLifetime != -1)
    vty_out (vty, " ipv6 nd ra-lifetime %d%s", zif->rtadv.AdvDefaultLifetime,
	     VTY_NEWLINE);

  if (zif->rtadv.HomeAgentPreference)
    vty_out (vty, " ipv6 nd home-agent-preference %u%s",
	     zif->rtadv.HomeAgentPreference, VTY_NEWLINE);

  if (zif->rtadv.HomeAgentLifetime != -1)
    vty_out (vty, " ipv6 nd home-agent-lifetime %u%s",
	     zif->rtadv.HomeAgentLifetime, VTY_NEWLINE);

  if (zif->rtadv.AdvHomeAgentFlag)
    vty_out (vty, " ipv6 nd home-agent-config-flag%s", VTY_NEWLINE);

  if (zif->rtadv.AdvReachableTime)
    vty_out (vty, " ipv6 nd reachable-time %d%s", zif->rtadv.AdvReachableTime,
	     VTY_NEWLINE);

  if (zif->rtadv.AdvManagedFlag)
    vty_out (vty, " ipv6 nd managed-config-flag%s", VTY_NEWLINE);

  if (zif->rtadv.AdvOtherConfigFlag)
    vty_out (vty, " ipv6 nd other-config-flag%s", VTY_NEWLINE);

  if (zif->rtadv.DefaultPreference != RTADV_PREF_MEDIUM)
    vty_out (vty, " ipv6 nd router-preference %s%s",
	     rtadv_pref_strs[zif->rtadv.DefaultPreference],
	     VTY_NEWLINE);

  if (zif->rtadv.AdvLinkMTU)
    vty_out (vty, " ipv6 nd mtu %d%s", zif->rtadv.AdvLinkMTU, VTY_NEWLINE);

  for (ALL_LIST_ELEMENTS_RO (zif->rtadv.AdvPrefixList, node, rprefix))
    {
      vty_out (vty, " ipv6 nd prefix %s/%d",
	       inet_ntop (AF_INET6, &rprefix->prefix.prefix,
			  (char *) buf, INET6_ADDRSTRLEN),
	       rprefix->prefix.prefixlen);
      if ((rprefix->AdvValidLifetime != RTADV_VALID_LIFETIME) || 
	  (rprefix->AdvPreferredLifetime != RTADV_PREFERRED_LIFETIME))
	{
	  if (rprefix->AdvValidLifetime == UINT32_MAX)
	    vty_out (vty, " infinite");
	  else
	    vty_out (vty, " %u", rprefix->AdvValidLifetime);
	  if (rprefix->AdvPreferredLifetime == UINT32_MAX)
	    vty_out (vty, " infinite");
	  else
	    vty_out (vty, " %u", rprefix->AdvPreferredLifetime);
	}
      if (!rprefix->AdvOnLinkFlag)
	vty_out (vty, " off-link");
      if (!rprefix->AdvAutonomousFlag)
	vty_out (vty, " no-autoconfig");
      if (rprefix->AdvRouterAddressFlag)
	vty_out (vty, " router-address");
      vty_out (vty, "%s", VTY_NEWLINE);
    }
}


static void
rtadv_event (enum rtadv_event event, int val)
{
  switch (event)
    {
    case RTADV_START:
      if (! rtadv->ra_read)
	rtadv->ra_read = thread_add_read (zebrad.master, rtadv_read, NULL, val);
      if (! rtadv->ra_timer)
	rtadv->ra_timer = thread_add_event (zebrad.master, rtadv_timer,
	                                    NULL, 0);
      break;
    case RTADV_STOP:
      if (rtadv->ra_timer)
	{
	  thread_cancel (rtadv->ra_timer);
	  rtadv->ra_timer = NULL;
	}
      if (rtadv->ra_read)
	{
	  thread_cancel (rtadv->ra_read);
	  rtadv->ra_read = NULL;
	}
      break;
    case RTADV_TIMER:
      if (! rtadv->ra_timer)
	rtadv->ra_timer = thread_add_timer (zebrad.master, rtadv_timer, NULL,
	                                    val);
      break;
    case RTADV_TIMER_MSEC:
      if (! rtadv->ra_timer)
	rtadv->ra_timer = thread_add_timer_msec (zebrad.master, rtadv_timer, 
					    NULL, val);
      break;
    case RTADV_READ:
      if (! rtadv->ra_read)
	rtadv->ra_read = thread_add_read (zebrad.master, rtadv_read, NULL, val);
      break;
    default:
      break;
    }
  return;
}

void
rtadv_init (void)
{
  int sock;

  sock = rtadv_make_socket ();
  if (sock < 0)
    return;

  rtadv = rtadv_new ();
  rtadv->sock = sock;

  install_element (INTERFACE_NODE, &ipv6_nd_suppress_ra_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_suppress_ra_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_ra_interval_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_ra_interval_msec_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_ra_interval_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_ra_interval_val_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_ra_interval_msec_val_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_ra_lifetime_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_ra_lifetime_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_ra_lifetime_val_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_reachable_time_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_reachable_time_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_reachable_time_val_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_managed_config_flag_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_managed_config_flag_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_other_config_flag_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_other_config_flag_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_homeagent_config_flag_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_homeagent_config_flag_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_homeagent_preference_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_homeagent_preference_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_homeagent_preference_val_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_homeagent_lifetime_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_homeagent_lifetime_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_homeagent_lifetime_val_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_adv_interval_config_option_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_adv_interval_config_option_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_prefix_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_prefix_val_rev_rtaddr_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_prefix_val_nortaddr_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_prefix_val_rev_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_prefix_val_noauto_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_prefix_val_offlink_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_prefix_val_rtaddr_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_prefix_val_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_prefix_noval_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_prefix_noval_rev_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_prefix_noval_noauto_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_prefix_noval_offlink_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_prefix_noval_rtaddr_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_prefix_prefix_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_prefix_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_router_preference_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_router_preference_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_router_preference_val_cmd);
  install_element (INTERFACE_NODE, &ipv6_nd_mtu_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_mtu_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_nd_mtu_val_cmd);
}

static int
if_join_all_router (int sock, struct interface *ifp)
{
  int ret;

  struct ipv6_mreq mreq;

  memset (&mreq, 0, sizeof (struct ipv6_mreq));
  inet_pton (AF_INET6, ALLROUTER, &mreq.ipv6mr_multiaddr);
  mreq.ipv6mr_interface = ifp->ifindex;

  ret = setsockopt (sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, 
		    (char *) &mreq, sizeof mreq);
  if (ret < 0)
    zlog_warn ("can't setsockopt IPV6_JOIN_GROUP: %s", safe_strerror (errno));

  zlog_info ("rtadv: %s join to all-routers multicast group", ifp->name);

  return 0;
}

static int
if_leave_all_router (int sock, struct interface *ifp)
{
  int ret;

  struct ipv6_mreq mreq;

  memset (&mreq, 0, sizeof (struct ipv6_mreq));
  inet_pton (AF_INET6, ALLROUTER, &mreq.ipv6mr_multiaddr);
  mreq.ipv6mr_interface = ifp->ifindex;

  ret = setsockopt (sock, IPPROTO_IPV6, IPV6_LEAVE_GROUP, 
		    (char *) &mreq, sizeof mreq);
  if (ret < 0)
    zlog_warn ("can't setsockopt IPV6_LEAVE_GROUP: %s", safe_strerror (errno));

  zlog_info ("rtadv: %s leave from all-routers multicast group", ifp->name);

  return 0;
}

#else
void
rtadv_init (void)
{
  /* Empty.*/;
}
#endif /* RTADV && HAVE_IPV6 */
