/*
 * OSPF network related functions
 *   Copyright (C) 1999 Toshiaki Takada
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

#include "thread.h"
#include "linklist.h"
#include "prefix.h"
#include "if.h"
#include "sockunion.h"
#include "log.h"
#include "sockopt.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_network.h"
#include "ospfd/ospf_interface.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_lsdb.h"
#include "ospfd/ospf_neighbor.h"
#include "ospfd/ospf_packet.h"

/* Join to the OSPF ALL SPF ROUTERS multicast group. */
int
ospf_if_add_allspfrouters (struct ospf *top, struct prefix *p,
			   unsigned int ifindex)
{
  int ret;
  
  ret = setsockopt_multicast_ipv4 (top->fd, IP_ADD_MEMBERSHIP,
                                   p->u.prefix4, htonl (OSPF_ALLSPFROUTERS),
                                   ifindex);
  if (ret < 0)
    zlog_warn ("can't setsockopt IP_ADD_MEMBERSHIP (AllSPFRouters): %s",
               strerror (errno));
  else
    zlog_info ("interface %s join AllSPFRouters Multicast group.",
	       inet_ntoa (p->u.prefix4));

  return ret;
}

int
ospf_if_drop_allspfrouters (struct ospf *top, struct prefix *p,
			    unsigned int ifindex)
{
  int ret;

  ret = setsockopt_multicast_ipv4 (top->fd, IP_DROP_MEMBERSHIP,
                                   p->u.prefix4, htonl (OSPF_ALLSPFROUTERS),
                                   ifindex);
  if (ret < 0)
    zlog_warn("can't setsockopt IP_DROP_MEMBERSHIP (AllSPFRouters): %s",
	      strerror (errno));
  else
    zlog_info ("interface %s leave AllSPFRouters Multicast group.",
	       inet_ntoa (p->u.prefix4));

  return ret;
}

/* Join to the OSPF ALL Designated ROUTERS multicast group. */
int
ospf_if_add_alldrouters (struct ospf *top, struct prefix *p, unsigned int
			 ifindex)
{
  int ret;

  ret = setsockopt_multicast_ipv4 (top->fd, IP_ADD_MEMBERSHIP,
                                   p->u.prefix4, htonl (OSPF_ALLDROUTERS),
                                   ifindex);
  if (ret < 0)
    zlog_warn ("can't setsockopt IP_ADD_MEMBERSHIP (AllDRouters): %s",
               strerror (errno));
  else
    zlog_info ("interface %s join AllDRouters Multicast group.",
	       inet_ntoa (p->u.prefix4));

  return ret;
}

int
ospf_if_drop_alldrouters (struct ospf *top, struct prefix *p, unsigned int
			  ifindex)
{
  int ret;

  ret = setsockopt_multicast_ipv4 (top->fd, IP_DROP_MEMBERSHIP,
                                   p->u.prefix4, htonl (OSPF_ALLDROUTERS),
                                   ifindex);
  if (ret < 0)
    zlog_warn ("can't setsockopt IP_DROP_MEMBERSHIP (AllDRouters): %s",
	       strerror (errno));
  else
    zlog_info ("interface %s leave AllDRouters Multicast group.",
	       inet_ntoa (p->u.prefix4));

  return ret;
}

int
ospf_if_ipmulticast (struct ospf *top, struct prefix *p, unsigned int ifindex)
{
  u_char val;
  int ret, len;
  
  val = 0;
  len = sizeof (val);
  
  /* Prevent receiving self-origined multicast packets. */
  ret = setsockopt (top->fd, IPPROTO_IP, IP_MULTICAST_LOOP, (void *)&val, len);
  if (ret < 0)
    zlog_warn ("can't setsockopt IP_MULTICAST_LOOP(0): %s", strerror (errno));
  
  /* Explicitly set multicast ttl to 1 -- endo. */
  val = 1;
  ret = setsockopt (top->fd, IPPROTO_IP, IP_MULTICAST_TTL, (void *)&val, len);
  if (ret < 0)
    zlog_warn ("can't setsockopt IP_MULTICAST_TTL(1): %s", strerror (errno));

  ret = setsockopt_multicast_ipv4 (top->fd, IP_MULTICAST_IF,
                                   p->u.prefix4, 0, ifindex);
  if (ret < 0)
    zlog_warn ("can't setsockopt IP_MULTICAST_IF: %s", strerror (errno));

  return ret;
}

int
ospf_sock_init (void)
{
  int ospf_sock;
  int ret, tos, hincl = 1;

  ospf_sock = socket (AF_INET, SOCK_RAW, IPPROTO_OSPFIGP);
  if (ospf_sock < 0)
    {
      zlog_warn ("ospf_read_sock_init: socket: %s", strerror (errno));
      return -1;
    }

  /* Set precedence field. */
#ifdef IPTOS_PREC_INTERNETCONTROL
  tos = IPTOS_PREC_INTERNETCONTROL;
  ret = setsockopt (ospf_sock, IPPROTO_IP, IP_TOS,
		    (char *) &tos, sizeof (int));
  if (ret < 0)
    {
      zlog_warn ("can't set sockopt IP_TOS %d to socket %d", tos, ospf_sock);
      close (ospf_sock);	/* Prevent sd leak. */
      return ret;
    }
#endif /* IPTOS_PREC_INTERNETCONTROL */

  /* we will include IP header with packet */
  ret = setsockopt (ospf_sock, IPPROTO_IP, IP_HDRINCL, &hincl, sizeof (hincl));
  if (ret < 0)
    zlog_warn ("Can't set IP_HDRINCL option");

#if defined (IP_PKTINFO)
  ret = setsockopt (ospf_sock, IPPROTO_IP, IP_PKTINFO, &hincl, sizeof (hincl));
   if (ret < 0)
    zlog_warn ("Can't set IP_PKTINFO option");
#elif defined (IP_RECVIF)
  ret = setsockopt (ospf_sock, IPPROTO_IP, IP_RECVIF, &hincl, sizeof (hincl));
   if (ret < 0)
    zlog_warn ("Can't set IP_RECVIF option");
#else
#warning "cannot be able to receive link information on this OS"
#endif
 
  return ospf_sock;
}
