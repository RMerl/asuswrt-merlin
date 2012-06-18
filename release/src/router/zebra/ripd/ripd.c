/* RIP version 1 and 2.
 * Copyright (C) 1997, 98, 99 Kunihiro Ishiguro <kunihiro@zebra.org>
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
#include "command.h"
#include "prefix.h"
#include "table.h"
#include "thread.h"
#include "memory.h"
#include "log.h"
#include "stream.h"
#include "filter.h"
#include "sockunion.h"
#include "routemap.h"
#include "plist.h"
#include "distribute.h"
#include "md5-gnu.h"
#include "keychain.h"

#include "ripd/ripd.h"
#include "ripd/rip_debug.h"

/* RIP Structure. */
struct rip *rip = NULL;

/* RIP neighbor address table. */
struct route_table *rip_neighbor_table;

/* RIP route changes. */
long rip_global_route_changes = 0;

/* RIP queries. */
long rip_global_queries = 0;

/* Prototypes. */
void rip_event (enum rip_event, int);

void rip_output_process (struct interface *, struct sockaddr_in *, 
			 int, u_char);

/* RIP output routes type. */
enum
  {
    rip_all_route,
    rip_changed_route
  };

/* RIP command strings. */
struct message rip_msg[] = 
  {
    {RIP_REQUEST,    "REQUEST"},
    {RIP_RESPONSE,   "RESPONSE"},
    {RIP_TRACEON,    "TRACEON"},
    {RIP_TRACEOFF,   "TRACEOFF"},
    {RIP_POLL,       "POLL"},
    {RIP_POLL_ENTRY, "POLL ENTRY"},
    {0,              NULL}
  };

/* Each route type's strings and default preference. */
struct
{  
  int key;
  char *str;
  char *str_long;
} route_info[] =
  {
    { ZEBRA_ROUTE_SYSTEM,  "X", "system"},
    { ZEBRA_ROUTE_KERNEL,  "K", "kernel"},
    { ZEBRA_ROUTE_CONNECT, "C", "connected"},
    { ZEBRA_ROUTE_STATIC,  "S", "static"},
    { ZEBRA_ROUTE_RIP,     "R", "rip"},
    { ZEBRA_ROUTE_RIPNG,   "R", "ripng"},
    { ZEBRA_ROUTE_OSPF,    "O", "ospf"},
    { ZEBRA_ROUTE_OSPF6,   "O", "ospf6"},
    { ZEBRA_ROUTE_BGP,     "B", "bgp"}
  };

/* Utility function to set boradcast option to the socket.  */
int
sockopt_broadcast (int sock)
{
  int ret;
  int on = 1;

  ret = setsockopt (sock, SOL_SOCKET, SO_BROADCAST, (char *) &on, sizeof on);
  if (ret < 0)
    {
      zlog_warn ("can't set sockopt SO_BROADCAST to socket %d: %s",
		 sock, strerror (errno));
      return ret;
    }
  return 0;
}

/* Utility function to set size of UDP SO_RCVBUF.  */
static int
sockopt_recvbuf (int sock, int size)
{
  int ret;
  
  ret = setsockopt (sock, SOL_SOCKET, SO_RCVBUF, (char *) &size, sizeof (int));
  if (ret < 0)
    zlog_warn ("can't setsockopt SO_RCVBUF to socket %d: %s",
	       sock, strerror (errno));
  return ret;
}

int
rip_route_rte (struct rip_info *rinfo)
{
  return (rinfo->type == ZEBRA_ROUTE_RIP && rinfo->sub_type == RIP_ROUTE_RTE);
}

struct rip_info *
rip_info_new ()
{
  struct rip_info *new;

  new = XMALLOC (MTYPE_RIP_INFO, sizeof (struct rip_info));
  memset (new, 0, sizeof (struct rip_info));
  return new;
}

void
rip_info_free (struct rip_info *rinfo)
{
  XFREE (MTYPE_RIP_INFO, rinfo);
}

/* RIP route garbage collect timer. */
int
rip_garbage_collect (struct thread *t)
{
  struct rip_info *rinfo;
  struct route_node *rp;

  rinfo = THREAD_ARG (t);
  rinfo->t_garbage_collect = NULL;

  /* Off timeout timer. */
  RIP_TIMER_OFF (rinfo->t_timeout);
  
  /* Get route_node pointer. */
  rp = rinfo->rp;

  /* Unlock route_node. */
  rp->info = NULL;
  route_unlock_node (rp);

  /* Free RIP routing information. */
  rip_info_free (rinfo);

  return 0;
}

/* Timeout RIP routes. */
int
rip_timeout (struct thread *t)
{
  struct rip_info *rinfo;
  struct route_node *rn;

  rinfo = THREAD_ARG (t);
  rinfo->t_timeout = NULL;

  rn = rinfo->rp;

  /* - The garbage-collection timer is set for 120 seconds. */
  RIP_TIMER_ON (rinfo->t_garbage_collect, rip_garbage_collect, 
		rip->garbage_time);

  rip_zebra_ipv4_delete ((struct prefix_ipv4 *)&rn->p, &rinfo->nexthop,
			 rinfo->metric);
  /* - The metric for the route is set to 16 (infinity).  This causes
     the route to be removed from service. */
  rinfo->metric = RIP_METRIC_INFINITY;
  rinfo->flags &= ~RIP_RTF_FIB;

  /* - The route change flag is to indicate that this entry has been
     changed. */
  rinfo->flags |= RIP_RTF_CHANGED;

  /* - The output process is signalled to trigger a response. */
  rip_event (RIP_TRIGGERED_UPDATE, 0);

  return 0;
}

void
rip_timeout_update (struct rip_info *rinfo)
{
  if (rinfo->metric != RIP_METRIC_INFINITY)
    {
      RIP_TIMER_OFF (rinfo->t_timeout);
      RIP_TIMER_ON (rinfo->t_timeout, rip_timeout, rip->timeout_time);
    }
}

int
rip_incoming_filter (struct prefix_ipv4 *p, struct rip_interface *ri)
{
  struct distribute *dist;
  struct access_list *alist;
  struct prefix_list *plist;

  /* Input distribute-list filtering. */
  if (ri->list[RIP_FILTER_IN])
    {
      if (access_list_apply (ri->list[RIP_FILTER_IN], 
			     (struct prefix *) p) == FILTER_DENY)
	{
	  if (IS_RIP_DEBUG_PACKET)
	    zlog_info ("%s/%d filtered by distribute in",
		       inet_ntoa (p->prefix), p->prefixlen);
	  return -1;
	}
    }
  if (ri->prefix[RIP_FILTER_IN])
    {
      if (prefix_list_apply (ri->prefix[RIP_FILTER_IN], 
			     (struct prefix *) p) == PREFIX_DENY)
	{
	  if (IS_RIP_DEBUG_PACKET)
	    zlog_info ("%s/%d filtered by prefix-list in",
		       inet_ntoa (p->prefix), p->prefixlen);
	  return -1;
	}
    }

  /* All interface filter check. */
  dist = distribute_lookup (NULL);
  if (dist)
    {
      if (dist->list[DISTRIBUTE_IN])
	{
	  alist = access_list_lookup (AFI_IP, dist->list[DISTRIBUTE_IN]);
	    
	  if (alist)
	    {
	      if (access_list_apply (alist,
				     (struct prefix *) p) == FILTER_DENY)
		{
		  if (IS_RIP_DEBUG_PACKET)
		    zlog_info ("%s/%d filtered by distribute in",
			       inet_ntoa (p->prefix), p->prefixlen);
		  return -1;
		}
	    }
	}
      if (dist->prefix[DISTRIBUTE_IN])
	{
	  plist = prefix_list_lookup (AFI_IP, dist->prefix[DISTRIBUTE_IN]);
	  
	  if (plist)
	    {
	      if (prefix_list_apply (plist,
				     (struct prefix *) p) == PREFIX_DENY)
		{
		  if (IS_RIP_DEBUG_PACKET)
		    zlog_info ("%s/%d filtered by prefix-list in",
			       inet_ntoa (p->prefix), p->prefixlen);
		  return -1;
		}
	    }
	}
    }
  return 0;
}

int
rip_outgoing_filter (struct prefix_ipv4 *p, struct rip_interface *ri)
{
  struct distribute *dist;
  struct access_list *alist;
  struct prefix_list *plist;

  if (ri->list[RIP_FILTER_OUT])
    {
      if (access_list_apply (ri->list[RIP_FILTER_OUT],
			     (struct prefix *) p) == FILTER_DENY)
	{
	  if (IS_RIP_DEBUG_PACKET)
	    zlog_info ("%s/%d is filtered by distribute out",
		       inet_ntoa (p->prefix), p->prefixlen);
	  return -1;
	}
    }
  if (ri->prefix[RIP_FILTER_OUT])
    {
      if (prefix_list_apply (ri->prefix[RIP_FILTER_OUT],
			     (struct prefix *) p) == PREFIX_DENY)
	{
	  if (IS_RIP_DEBUG_PACKET)
	    zlog_info ("%s/%d is filtered by prefix-list out",
		       inet_ntoa (p->prefix), p->prefixlen);
	  return -1;
	}
    }

  /* All interface filter check. */
  dist = distribute_lookup (NULL);
  if (dist)
    {
      if (dist->list[DISTRIBUTE_OUT])
	{
	  alist = access_list_lookup (AFI_IP, dist->list[DISTRIBUTE_OUT]);
	    
	  if (alist)
	    {
	      if (access_list_apply (alist,
				     (struct prefix *) p) == FILTER_DENY)
		{
		  if (IS_RIP_DEBUG_PACKET)
		    zlog_info ("%s/%d filtered by distribute out",
			       inet_ntoa (p->prefix), p->prefixlen);
		  return -1;
		}
	    }
	}
      if (dist->prefix[DISTRIBUTE_OUT])
	{
	  plist = prefix_list_lookup (AFI_IP, dist->prefix[DISTRIBUTE_OUT]);
	  
	  if (plist)
	    {
	      if (prefix_list_apply (plist,
				     (struct prefix *) p) == PREFIX_DENY)
		{
		  if (IS_RIP_DEBUG_PACKET)
		    zlog_info ("%s/%d filtered by prefix-list out",
			       inet_ntoa (p->prefix), p->prefixlen);
		  return -1;
		}
	    }
	}
    }
  return 0;
}

/* Check nexthop address validity. */
static int
rip_nexthop_check (struct in_addr *addr)
{
  listnode node;
  listnode cnode;
  struct interface *ifp;
  struct connected *ifc;
  struct prefix *p;

  /* If nexthop address matches local configured address then it is
     invalid nexthop. */
  for (node = listhead (iflist); node; nextnode (node))
    {
      ifp = getdata (node);

      for (cnode = listhead (ifp->connected); cnode; nextnode (cnode))
	{	    
	  ifc = getdata (cnode);
	  p = ifc->address;

	  if (p->family == AF_INET
	      && IPV4_ADDR_SAME (&p->u.prefix4, addr))
	    return -1;
	}
    }
  return 0;
}

/* RIP add route to routing table. */
void
rip_rte_process (struct rte *rte, struct sockaddr_in *from,
		 struct interface *ifp)

{
  int ret;
  struct prefix_ipv4 p;
  struct route_node *rp;
  struct rip_info *rinfo;
  struct rip_interface *ri;
  struct in_addr *nexthop;
  u_char oldmetric;
  int same = 0;

  /* Make prefix structure. */
  memset (&p, 0, sizeof (struct prefix_ipv4));
  p.family = AF_INET;
  p.prefix = rte->prefix;
  p.prefixlen = ip_masklen (rte->mask);

  /* Make sure mask is applied. */
  apply_mask_ipv4 (&p);

  /* Apply input filters. */
  ri = ifp->info;

  ret = rip_incoming_filter (&p, ri);
  if (ret < 0)
    return;

  /* Once the entry has been validated, update the metric by
     adding the cost of the network on wich the message
     arrived. If the result is greater than infinity, use infinity
     (RFC2453 Sec. 3.9.2) */
  /* Zebra ripd can handle offset-list in. */
  ret = rip_offset_list_apply_in (&p, ifp, &rte->metric);

  /* If offset-list does not modify the metric use interface's
     metric. */
  if (! ret)
    rte->metric += ifp->metric;

  if (rte->metric > RIP_METRIC_INFINITY)
    rte->metric = RIP_METRIC_INFINITY;

  /* Set nexthop pointer. */
  if (rte->nexthop.s_addr == 0)
    nexthop = &from->sin_addr;
  else
    nexthop = &rte->nexthop;

  /* Check nexthop address. */
  if (rip_nexthop_check (nexthop) < 0)
    {
      if (IS_RIP_DEBUG_PACKET)
	zlog_info ("Nexthop address %s is invalid", inet_ntoa (*nexthop));
      return;
    }

  /* Get index for the prefix. */
  rp = route_node_get (rip->table, (struct prefix *) &p);

  /* Check to see whether there is already RIP route on the table. */
  rinfo = rp->info;

  if (rinfo)
    {
      /* Redistributed route check. */
      if (rinfo->type != ZEBRA_ROUTE_RIP
	  && rinfo->metric != RIP_METRIC_INFINITY)
	return;

      /* Local static route. */
      if (rinfo->type == ZEBRA_ROUTE_RIP
	  && rinfo->sub_type == RIP_ROUTE_STATIC
	  && rinfo->metric != RIP_METRIC_INFINITY)
	return;
    }
  
  if (! rinfo)
    {
      /* Now, check to see whether there is already an explicit route
	 for the destination prefix.  If there is no such route, add
	 this route to the routing table, unless the metric is
	 infinity (there is no point in adding a route which
	 unusable). */
      if (rte->metric != RIP_METRIC_INFINITY)
	{
	  rinfo = rip_info_new ();
	  
	  /* - Setting the destination prefix and length to those in
	     the RTE. */
	  rinfo->rp = rp;

	  /* - Setting the metric to the newly calculated metric (as
	     described above). */
	  rinfo->metric = rte->metric;
	  rinfo->tag = ntohs (rte->tag);

	  /* - Set the next hop address to be the address of the router
	     from which the datagram came or the next hop address
	     specified by a next hop RTE. */
	  IPV4_ADDR_COPY (&rinfo->nexthop, nexthop);
	  IPV4_ADDR_COPY (&rinfo->from, &from->sin_addr);
	  rinfo->ifindex = ifp->ifindex;

	  /* - Initialize the timeout for the route.  If the
	     garbage-collection timer is running for this route, stop it
	     (see section 2.3 for a discussion of the timers). */
	  rip_timeout_update (rinfo);

	  /* - Set the route change flag. */
	  rinfo->flags |= RIP_RTF_CHANGED;

	  /* - Signal the output process to trigger an update (see section
	     2.5). */
	  rip_event (RIP_TRIGGERED_UPDATE, 0);

	  /* Finally, route goes into the kernel. */
	  rinfo->type = ZEBRA_ROUTE_RIP;
	  rinfo->sub_type = RIP_ROUTE_RTE;

	  /* Set distance value. */
	  rinfo->distance = rip_distance_apply (rinfo);

	  rp->info = rinfo;
	  rip_zebra_ipv4_add (&p, &rinfo->nexthop, rinfo->metric,
			      rinfo->distance);
	  rinfo->flags |= RIP_RTF_FIB;
	}
    }
  else
    {
      /* Route is there but we are not sure the route is RIP or not. */
      rinfo = rp->info;
	  
      /* If there is an existing route, compare the next hop address
	 to the address of the router from which the datagram came.
	 If this datagram is from the same router as the existing
	 route, reinitialize the timeout.  */
      same = IPV4_ADDR_SAME (&rinfo->from, &from->sin_addr);

      if (same)
	rip_timeout_update (rinfo);

      /* Next, compare the metrics.  If the datagram is from the same
	 router as the existing route, and the new metric is different
	 than the old one; or, if the new metric is lower than the old
	 one; do the following actions: */
      if ((same && (rinfo->metric != rte->metric
		    || rinfo->tag != rte->tag
		    || !IPV4_ADDR_SAME(&rte->nexthop, &rinfo->nexthop)))
	  || rte->metric < rinfo->metric)
	{
	  /* - Adopt the route from the datagram.  That is, put the
	     new metric in, and adjust the next hop address (if
	     necessary). */
	  oldmetric = rinfo->metric;
	  rinfo->metric = rte->metric;
	  rinfo->tag = ntohs (rte->tag);
	  IPV4_ADDR_COPY (&rinfo->from, &from->sin_addr);
	  rinfo->ifindex = ifp->ifindex;
	  rinfo->distance = rip_distance_apply (rinfo);

	  /* Should a new route to this network be established
	     while the garbage-collection timer is running, the
	     new route will replace the one that is about to be
	     deleted.  In this case the garbage-collection timer
	     must be cleared. */

	  if (oldmetric == RIP_METRIC_INFINITY &&
	      rinfo->metric < RIP_METRIC_INFINITY)
	    {
	      rinfo->type = ZEBRA_ROUTE_RIP;
	      rinfo->sub_type = RIP_ROUTE_RTE;

	      RIP_TIMER_OFF (rinfo->t_garbage_collect);

	      if (! IPV4_ADDR_SAME (&rinfo->nexthop, nexthop))
		IPV4_ADDR_COPY (&rinfo->nexthop, nexthop);

	      rip_zebra_ipv4_add (&p, nexthop, rinfo->metric,
				  rinfo->distance);
	      rinfo->flags |= RIP_RTF_FIB;
	    }

	  /* Update nexthop and/or metric value.  */
	  if (oldmetric != RIP_METRIC_INFINITY)
	    {
	      rip_zebra_ipv4_delete (&p, &rinfo->nexthop, oldmetric);
	      rip_zebra_ipv4_add (&p, nexthop, rinfo->metric,
				  rinfo->distance);
	      rinfo->flags |= RIP_RTF_FIB;

	      if (! IPV4_ADDR_SAME (&rinfo->nexthop, nexthop))
		IPV4_ADDR_COPY (&rinfo->nexthop, nexthop);
	    }

	  /* - Set the route change flag and signal the output process
	     to trigger an update. */
	  rinfo->flags |= RIP_RTF_CHANGED;
	  rip_event (RIP_TRIGGERED_UPDATE, 0);

	  /* - If the new metric is infinity, start the deletion
	     process (described above); */
	  if (rinfo->metric == RIP_METRIC_INFINITY)
	    {
	      /* If the new metric is infinity, the deletion process
		 begins for the route, which is no longer used for
		 routing packets.  Note that the deletion process is
		 started only when the metric is first set to
		 infinity.  If the metric was already infinity, then a
		 new deletion process is not started. */
	      if (oldmetric != RIP_METRIC_INFINITY)
		{
		  /* - The garbage-collection timer is set for 120 seconds. */
		  RIP_TIMER_ON (rinfo->t_garbage_collect, 
				rip_garbage_collect, rip->garbage_time);
		  RIP_TIMER_OFF (rinfo->t_timeout);

		  /* - The metric for the route is set to 16
		     (infinity).  This causes the route to be removed
		     from service.*/
		  rip_zebra_ipv4_delete (&p, &rinfo->nexthop, oldmetric);
		  rinfo->flags &= ~RIP_RTF_FIB;

		  /* - The route change flag is to indicate that this
		     entry has been changed. */
		  /* - The output process is signalled to trigger a
                     response. */
		  ;  /* Above processes are already done previously. */
		}
	    }
	  else
	    {
	      /* otherwise, re-initialize the timeout. */
	      rip_timeout_update (rinfo);
	    }
	}
      /* Unlock tempolary lock of the route. */
      route_unlock_node (rp);
    }
}

/* Dump RIP packet */
void
rip_packet_dump (struct rip_packet *packet, int size, char *sndrcv)
{
  caddr_t lim;
  struct rte *rte;
  char *command_str;
  char pbuf[BUFSIZ], nbuf[BUFSIZ];
  u_char netmask = 0;
  u_char *p;

  /* Set command string. */
  if (packet->command > 0 && packet->command < RIP_COMMAND_MAX)
    command_str = lookup (rip_msg, packet->command);
  else
    command_str = "unknown";

  /* Dump packet header. */
  zlog_info ("%s %s version %d packet size %d",
	     sndrcv, command_str, packet->version, size);

  /* Dump each routing table entry. */
  rte = packet->rte;
  
  for (lim = (caddr_t) packet + size; (caddr_t) rte < lim; rte++)
    {
      if (packet->version == RIPv2)
	{
	  netmask = ip_masklen (rte->mask);

	  if (ntohs (rte->family) == 0xffff)
            {
	      if (ntohs (rte->tag) == RIP_AUTH_SIMPLE_PASSWORD)
		{
		  p = (u_char *)&rte->prefix;

		  zlog_info ("  family 0x%X type %d auth string: %s",
			     ntohs (rte->family), ntohs (rte->tag), p);
		}
	      else if (ntohs (rte->tag) == RIP_AUTH_MD5)
		{
		  struct rip_md5_info *md5;

		  md5 = (struct rip_md5_info *) &packet->rte;

		  zlog_info ("  family 0x%X type %d (MD5 authentication)",
			     ntohs (md5->family), ntohs (md5->type));
		  zlog_info ("    RIP-2 packet len %d Key ID %d"
			     " Auth Data len %d", ntohs (md5->packet_len),
			     md5->keyid, md5->auth_len);
		  zlog_info ("    Sequence Number %ld", (u_long)ntohl (md5->sequence));
		}
	      else if (ntohs (rte->tag) == RIP_AUTH_DATA)
		{
		  p = (u_char *)&rte->prefix;

		  zlog_info ("  family 0x%X type %d (MD5 data)",
			     ntohs (rte->family), ntohs (rte->tag));
		  zlog_info ("    MD5: %02X%02X%02X%02X%02X%02X%02X%02X"
			     "%02X%02X%02X%02X%02X%02X%02X",
			     p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],
			     p[9],p[10],p[11],p[12],p[13],p[14],p[15]);
		}
	      else
		{
		  zlog_info ("  family 0x%X type %d (Unknown auth type)",
			     ntohs (rte->family), ntohs (rte->tag));
		}
            }
	  else
	    zlog_info ("  %s/%d -> %s family %d tag %d metric %ld",
		       inet_ntop (AF_INET, &rte->prefix, pbuf, BUFSIZ),netmask,
		       inet_ntop (AF_INET, &rte->nexthop, nbuf, BUFSIZ),
		       ntohs (rte->family), ntohs (rte->tag), 
		       (u_long)ntohl (rte->metric));
	}
      else
	{
	  zlog_info ("  %s family %d tag %d metric %ld", 
		     inet_ntop (AF_INET, &rte->prefix, pbuf, BUFSIZ),
		     ntohs (rte->family), ntohs (rte->tag),
		     (u_long)ntohl (rte->metric));
	}
    }
}

/* Check if the destination address is valid (unicast; not net 0
   or 127) (RFC2453 Section 3.9.2 - Page 26).  But we don't
   check net 0 because we accept default route. */
int
rip_destination_check (struct in_addr addr)
{
  u_int32_t destination;

  /* Convert to host byte order. */
  destination = ntohl (addr.s_addr);

  if (IPV4_NET127 (destination))
    return 0;

  /* Net 0 may match to the default route. */
  if (IPV4_NET0 (destination) && destination != 0)
    return 0;

  /* Unicast address must belong to class A, B, C. */
  if (IN_CLASSA (destination))
    return 1;
  if (IN_CLASSB (destination))
    return 1;
  if (IN_CLASSC (destination))
    return 1;

  return 0;
}

/* RIP version 2 authentication. */
int
rip_auth_simple_password (struct rte *rte, struct sockaddr_in *from,
			  struct interface *ifp)
{
  struct rip_interface *ri;
  char *auth_str;

  if (IS_RIP_DEBUG_EVENT)
    zlog_info ("RIPv2 simple password authentication from %s",
	       inet_ntoa (from->sin_addr));

  ri = ifp->info;

  if (ri->auth_type != RIP_AUTH_SIMPLE_PASSWORD
      || ntohs (rte->tag) != RIP_AUTH_SIMPLE_PASSWORD)
    return 0;

  /* Simple password authentication. */
  if (ri->auth_str)
    {
      auth_str = (char *) &rte->prefix;
	  
      if (strncmp (auth_str, ri->auth_str, 16) == 0)
	return 1;
    }
  if (ri->key_chain)
    {
      struct keychain *keychain;
      struct key *key;

      keychain = keychain_lookup (ri->key_chain);
      if (keychain == NULL)
	return 0;

      key = key_match_for_accept (keychain, (char *) &rte->prefix);
      if (key)
	return 1;
    }
  return 0;
}

/* RIP version 2 authentication with MD5. */
int
rip_auth_md5 (struct rip_packet *packet, struct sockaddr_in *from,
	      struct interface *ifp)
{
  struct rip_interface *ri;
  struct rip_md5_info *md5;
  struct rip_md5_data *md5data;
  struct keychain *keychain;
  struct key *key;
  struct md5_ctx ctx;
  u_char pdigest[RIP_AUTH_MD5_SIZE];
  u_char digest[RIP_AUTH_MD5_SIZE];
  u_int16_t packet_len;
  char *auth_str = NULL;
  
  if (IS_RIP_DEBUG_EVENT)
    zlog_info ("RIPv2 MD5 authentication from %s", inet_ntoa (from->sin_addr));

  ri = ifp->info;
  md5 = (struct rip_md5_info *) &packet->rte;

  /* Check auth type. */
  if (ri->auth_type != RIP_AUTH_MD5 || ntohs (md5->type) != RIP_AUTH_MD5)
    return 0;

  if (md5->auth_len != RIP_HEADER_SIZE + RIP_AUTH_MD5_SIZE)
    return 0;

  if (ri->key_chain)
    {
      keychain = keychain_lookup (ri->key_chain);
      if (keychain == NULL)
	return 0;

      key = key_lookup_for_accept (keychain, md5->keyid);
      if (key == NULL)
	return 0;

      auth_str = key->string;
    }

  if (ri->auth_str)
    auth_str = ri->auth_str;

  if (! auth_str)
    return 0;

  /* MD5 digest authentication. */
  packet_len = ntohs (md5->packet_len);
  md5data = (struct rip_md5_data *)(((u_char *) packet) + packet_len);

  /* Save digest to pdigest. */
  memcpy (pdigest, md5data->digest, RIP_AUTH_MD5_SIZE);

  /* Overwrite digest by my secret. */
  memset (md5data->digest, 0, RIP_AUTH_MD5_SIZE);
  strncpy (md5data->digest, auth_str, RIP_AUTH_MD5_SIZE);

  md5_init_ctx (&ctx);
  md5_process_bytes (packet, packet_len + md5->auth_len, &ctx);
  md5_finish_ctx (&ctx, digest);

  if (memcmp (pdigest, digest, RIP_AUTH_MD5_SIZE) == 0)
    return packet_len;
  else
    return 0;
}

void
rip_auth_md5_set (struct stream *s, struct interface *ifp)
{
  struct rip_interface *ri;
  struct keychain *keychain = NULL;
  struct key *key = NULL;
  unsigned long len;
  struct md5_ctx ctx;
  unsigned char secret[RIP_AUTH_MD5_SIZE];
  unsigned char digest[RIP_AUTH_MD5_SIZE];
  char *auth_str = NULL;

  ri = ifp->info;

  /* Make it sure this interface is configured as MD5
     authentication. */
  if (ri->auth_type != RIP_AUTH_MD5)
    return;

  /* Lookup key chain. */
  if (ri->key_chain)
    {
      keychain = keychain_lookup (ri->key_chain);
      if (keychain == NULL)
	return;

      /* Lookup key. */
      key = key_lookup_for_send (keychain);
      if (key == NULL)
	return;

      auth_str = key->string;
    }

  if (ri->auth_str)
    auth_str = ri->auth_str;

  if (! auth_str)
    return;

  /* Get packet length. */
  len = s->putp;

  /* Check packet length. */
  if (len < (RIP_HEADER_SIZE + RIP_RTE_SIZE))
    {
      zlog_err ("rip_auth_md5_set(): packet length %ld is less than minimum length.", len);
      return;
    }

  /* Move RTE. */
  memmove (s->data + RIP_HEADER_SIZE + RIP_RTE_SIZE,
	   s->data + RIP_HEADER_SIZE,
	   len - RIP_HEADER_SIZE);
  
  /* Set pointer to authentication header. */
  stream_set_putp (s, RIP_HEADER_SIZE);
  len += RIP_RTE_SIZE;

  /* MD5 authentication. */
  stream_putw (s, 0xffff);
  stream_putw (s, RIP_AUTH_MD5);

  /* RIP-2 Packet length.  Actual value is filled in
     rip_auth_md5_set(). */
  stream_putw (s, len);

  /* Key ID. */
  if (key)
    stream_putc (s, key->index % 256);
  else
    stream_putc (s, 1);

  /* Auth Data Len.  Set 16 for MD5 authentication
     data. */
  stream_putc (s, RIP_AUTH_MD5_SIZE + RIP_HEADER_SIZE);

  /* Sequence Number (non-decreasing). */
  /* RFC2080: The value used in the sequence number is
     arbitrary, but two suggestions are the time of the
     message's creation or a simple message counter. */
  stream_putl (s, time (NULL));
	      
  /* Reserved field must be zero. */
  stream_putl (s, 0);
  stream_putl (s, 0);

  /* Set pointer to authentication data. */
  stream_set_putp (s, len);

  /* Set authentication data. */
  stream_putw (s, 0xffff);
  stream_putw (s, 0x01);

  /* Generate a digest for the RIP packet. */
  memset (secret, 0, RIP_AUTH_MD5_SIZE);
  strncpy (secret, auth_str, RIP_AUTH_MD5_SIZE);
  md5_init_ctx (&ctx);
  md5_process_bytes (s->data, s->endp, &ctx);
  md5_process_bytes (secret, RIP_AUTH_MD5_SIZE, &ctx);
  md5_finish_ctx (&ctx, digest);

  /* Copy the digest to the packet. */
  stream_write (s, digest, RIP_AUTH_MD5_SIZE);
}

/* RIP routing information. */
void
rip_response_process (struct rip_packet *packet, int size, 
		      struct sockaddr_in *from, struct interface *ifp)
{
  caddr_t lim;
  struct rte *rte;
      
  /* The Response must be ignored if it is not from the RIP
     port. (RFC2453 - Sec. 3.9.2)*/
  if (ntohs (from->sin_port) != RIP_PORT_DEFAULT) 
    {
      zlog_info ("response doesn't come from RIP port: %d",
		 from->sin_port);
      rip_peer_bad_packet (from);
      return;
    }

  /* The datagram's IPv4 source address should be checked to see
     whether the datagram is from a valid neighbor; the source of the
     datagram must be on a directly connected network  */
  if (! if_valid_neighbor (from->sin_addr)) 
    {
      zlog_info ("This datagram doesn't came from a valid neighbor: %s",
		 inet_ntoa (from->sin_addr));
      rip_peer_bad_packet (from);
      return;
    }

  /* It is also worth checking to see whether the response is from one
     of the router's own addresses. */

  ; /* Alredy done in rip_read () */

  /* Update RIP peer. */
  rip_peer_update (from, packet->version);

  /* Set RTE pointer. */
  rte = packet->rte;

  for (lim = (caddr_t) packet + size; (caddr_t) rte < lim; rte++)
    {
      /* RIPv2 authentication check. */
      /* If the Address Family Identifier of the first (and only the
	 first) entry in the message is 0xFFFF, then the remainder of
	 the entry contains the authentication. */
      /* If the packet gets here it means authentication enabled */
      /* Check is done in rip_read(). So, just skipping it */
      if (packet->version == RIPv2 &&
	  rte == packet->rte &&
	  rte->family == 0xffff)
	continue;

      if (ntohs (rte->family) != AF_INET)
	{
	  /* Address family check.  RIP only supports AF_INET. */
	  zlog_info ("Unsupported family %d from %s.",
		     ntohs (rte->family), inet_ntoa (from->sin_addr));
	  continue;
	}

      /* - is the destination address valid (e.g., unicast; not net 0
         or 127) */
      if (! rip_destination_check (rte->prefix))
        {
	  zlog_info ("Network is net 0 or net 127 or it is not unicast network");
	  rip_peer_bad_route (from);
	  continue;
	} 

      /* Convert metric value to host byte order. */
      rte->metric = ntohl (rte->metric);

      /* - is the metric valid (i.e., between 1 and 16, inclusive) */
      if (! (rte->metric >= 1 && rte->metric <= 16))
	{
	  zlog_info ("Route's metric is not in the 1-16 range.");
	  rip_peer_bad_route (from);
	  continue;
	}

      /* RIPv1 does not have nexthop value. */
      if (packet->version == RIPv1 && rte->nexthop.s_addr != 0)
	{
	  zlog_info ("RIPv1 packet with nexthop value %s",
		     inet_ntoa (rte->nexthop));
	  rip_peer_bad_route (from);
	  continue;
	}

      /* That is, if the provided information is ignored, a possibly
	 sub-optimal, but absolutely valid, route may be taken.  If
	 the received Next Hop is not directly reachable, it should be
	 treated as 0.0.0.0. */
      if (packet->version == RIPv2 && rte->nexthop.s_addr != 0)
	{
	  u_int32_t addrval;

	  /* Multicast address check. */
	  addrval = ntohl (rte->nexthop.s_addr);
	  if (IN_CLASSD (addrval))
	    {
	      zlog_info ("Nexthop %s is multicast address, skip this rte",
			 inet_ntoa (rte->nexthop));
	      continue;
	    }

	  if (! if_lookup_address (rte->nexthop))
	    {
	      struct route_node *rn;
	      struct rip_info *rinfo;

	      rn = route_node_match_ipv4 (rip->table, &rte->nexthop);

	      if (rn)
		{
		  rinfo = rn->info;

		  if (rinfo->type == ZEBRA_ROUTE_RIP
		      && rinfo->sub_type == RIP_ROUTE_RTE)
		    {
		      if (IS_RIP_DEBUG_EVENT)
			zlog_info ("Next hop %s is on RIP network.  Set nexthop to the packet's originator", inet_ntoa (rte->nexthop));
		      rte->nexthop = rinfo->from;
		    }
		  else
		    {
		      if (IS_RIP_DEBUG_EVENT)
			zlog_info ("Next hop %s is not directly reachable. Treat it as 0.0.0.0", inet_ntoa (rte->nexthop));
		      rte->nexthop.s_addr = 0;
		    }

		  route_unlock_node (rn);
		}
	      else
		{
		  if (IS_RIP_DEBUG_EVENT)
		    zlog_info ("Next hop %s is not directly reachable. Treat it as 0.0.0.0", inet_ntoa (rte->nexthop));
		  rte->nexthop.s_addr = 0;
		}

	    }
	}

      /* For RIPv1, there won't be a valid netmask.  

      This is a best guess at the masks.  If everyone was using old
      Ciscos before the 'ip subnet zero' option, it would be almost
      right too :-)
      
      Cisco summarize ripv1 advertisments to the classful boundary
      (/16 for class B's) except when the RIP packet does to inside
      the classful network in question.  */

      if ((packet->version == RIPv1 && rte->prefix.s_addr != 0) 
	  || (packet->version == RIPv2 
	      && (rte->prefix.s_addr != 0 && rte->mask.s_addr == 0)))
	{
	  u_int32_t destination;

	  destination = ntohl (rte->prefix.s_addr);

	  if (destination & 0xff) 
	    {
	      masklen2ip (32, &rte->mask);
	    }
	  else if ((destination & 0xff00) || IN_CLASSC (destination)) 
	    {
	      masklen2ip (24, &rte->mask);
	    }
	  else if ((destination & 0xff0000) || IN_CLASSB (destination)) 
	    {
	      masklen2ip (16, &rte->mask);
	    }
	  else 
	    {
	      masklen2ip (8, &rte->mask);
	    }
	}

      /* In case of RIPv2, if prefix in RTE is not netmask applied one
         ignore the entry.  */
      if ((packet->version == RIPv2) 
	  && (rte->mask.s_addr != 0) 
	  && ((rte->prefix.s_addr & rte->mask.s_addr) != rte->prefix.s_addr))
	{
	  zlog_warn ("RIPv2 address %s is not mask /%d applied one",
		     inet_ntoa (rte->prefix), ip_masklen (rte->mask));
	  rip_peer_bad_route (from);
	  continue;
	}

      /* Default route's netmask is ignored. */
      if (packet->version == RIPv2
	  && (rte->prefix.s_addr == 0)
	  && (rte->mask.s_addr != 0))
	{
	  if (IS_RIP_DEBUG_EVENT)
	    zlog_info ("Default route with non-zero netmask.  Set zero to netmask");
	  rte->mask.s_addr = 0;
	}
	  
      /* Routing table updates. */
      rip_rte_process (rte, from, ifp);
    }
}

/* RIP packet send to destination address. */
int
rip_send_packet (caddr_t buf, int size, struct sockaddr_in *to, 
		 struct interface *ifp)
{
  int ret;
  struct sockaddr_in sin;
  int sock;

  /* Make destination address. */
  memset (&sin, 0, sizeof (struct sockaddr_in));
  sin.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
  sin.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_SIN_LEN */

  /* When destination is specified, use it's port and address. */
  if (to)
    {
      sock = rip->sock;

      sin.sin_port = to->sin_port;
      sin.sin_addr = to->sin_addr;
    }
  else
    {
      sock = socket (AF_INET, SOCK_DGRAM, 0);
      
      sockopt_broadcast (sock);
      sockopt_reuseaddr (sock);
      sockopt_reuseport (sock);

      sin.sin_port = htons (RIP_PORT_DEFAULT);
      sin.sin_addr.s_addr = htonl (INADDR_RIP_GROUP);

      /* Set multicast interface. */
      rip_interface_multicast_set (sock, ifp);
    }

  ret = sendto (sock, buf, size, 0, (struct sockaddr *)&sin,
		sizeof (struct sockaddr_in));

  if (IS_RIP_DEBUG_EVENT)
    zlog_info ("SEND to socket %d port %d addr %s",
	       sock, ntohs (sin.sin_port), inet_ntoa(sin.sin_addr));

  if (ret < 0)
    zlog_warn ("can't send packet : %s", strerror (errno));

  if (! to)
    close (sock);

  return ret;
}

/* Add redistributed route to RIP table. */
void
rip_redistribute_add (int type, int sub_type, struct prefix_ipv4 *p, 
		      unsigned int ifindex, struct in_addr *nexthop)
{
  int ret;
  struct route_node *rp;
  struct rip_info *rinfo;

  /* Redistribute route  */
  ret = rip_destination_check (p->prefix);
  if (! ret)
    return;

  rp = route_node_get (rip->table, (struct prefix *) p);

  rinfo = rp->info;

  if (rinfo)
    {
      if (rinfo->type == ZEBRA_ROUTE_CONNECT 
	  && rinfo->sub_type == RIP_ROUTE_INTERFACE
	  && rinfo->metric != RIP_METRIC_INFINITY)
	{
	  route_unlock_node (rp);
	  return;
	}

      /* Manually configured RIP route check. */
      if (rinfo->type == ZEBRA_ROUTE_RIP 
	  && rinfo->sub_type == RIP_ROUTE_STATIC)
	{
	  if (type != ZEBRA_ROUTE_RIP || sub_type != RIP_ROUTE_STATIC)
	    {
	      route_unlock_node (rp);
	      return;
	    }
	}

      RIP_TIMER_OFF (rinfo->t_timeout);
      RIP_TIMER_OFF (rinfo->t_garbage_collect);

      if (rip_route_rte (rinfo))
	rip_zebra_ipv4_delete ((struct prefix_ipv4 *)&rp->p, &rinfo->nexthop,
			       rinfo->metric);
      rp->info = NULL;
      rip_info_free (rinfo);
      
      route_unlock_node (rp);      
    }

  rinfo = rip_info_new ();
    
  rinfo->type = type;
  rinfo->sub_type = sub_type;
  rinfo->ifindex = ifindex;
  rinfo->metric = 1;
  rinfo->rp = rp;

  if (nexthop)
    rinfo->nexthop = *nexthop;

  rinfo->flags |= RIP_RTF_FIB;
  rp->info = rinfo;

  rinfo->flags |= RIP_RTF_CHANGED;

  rip_event (RIP_TRIGGERED_UPDATE, 0);
}

/* Delete redistributed route from RIP table. */
void
rip_redistribute_delete (int type, int sub_type, struct prefix_ipv4 *p, 
			 unsigned int ifindex)
{
  int ret;
  struct route_node *rp;
  struct rip_info *rinfo;

  ret = rip_destination_check (p->prefix);
  if (! ret)
    return;

  rp = route_node_lookup (rip->table, (struct prefix *) p);
  if (rp)
    {
      rinfo = rp->info;

      if (rinfo != NULL
	  && rinfo->type == type 
	  && rinfo->sub_type == sub_type 
	  && rinfo->ifindex == ifindex)
	{
	  /* Perform poisoned reverse. */
	  rinfo->metric = RIP_METRIC_INFINITY;
	  RIP_TIMER_ON (rinfo->t_garbage_collect, 
			rip_garbage_collect, rip->garbage_time);
	  RIP_TIMER_OFF (rinfo->t_timeout);
	  rinfo->flags |= RIP_RTF_CHANGED;

	  rip_event (RIP_TRIGGERED_UPDATE, 0);
	}
    }
}

/* Response to request called from rip_read ().*/
void
rip_request_process (struct rip_packet *packet, int size, 
		     struct sockaddr_in *from, struct interface *ifp)
{
  caddr_t lim;
  struct rte *rte;
  struct prefix_ipv4 p;
  struct route_node *rp;
  struct rip_info *rinfo;
  struct rip_interface *ri;

  ri = ifp->info;

  /* When passive interface is specified, suppress responses */
  if (ri->passive)
    return;

  /* RIP peer update. */
  rip_peer_update (from, packet->version);

  lim = ((caddr_t) packet) + size;
  rte = packet->rte;

  /* The Request is processed entry by entry.  If there are no
     entries, no response is given. */
  if (lim == (caddr_t) rte)
    return;

  /* There is one special case.  If there is exactly one entry in the
     request, and it has an address family identifier of zero and a
     metric of infinity (i.e., 16), then this is a request to send the
     entire routing table. */
  if ((rte->family == 0xFFFF) && (lim == ((caddr_t) (rte + 2)) )) rte++;
  
  if (lim == ((caddr_t) (rte + 1)) &&
      ntohs (rte->family) == 0 &&
      ntohl (rte->metric) == RIP_METRIC_INFINITY)
    {	
      /* All route with split horizon */
      rip_output_process (ifp, from, rip_all_route, packet->version);
    }
  else
    {
      /* Examine the list of RTEs in the Request one by one.  For each
	 entry, look up the destination in the router's routing
	 database and, if there is a route, put that route's metric in
	 the metric field of the RTE.  If there is no explicit route
	 to the specified destination, put infinity in the metric
	 field.  Once all the entries have been filled in, change the
	 command from Request to Response and send the datagram back
	 to the requestor. */
      p.family = AF_INET;

      for (; ((caddr_t) rte) < lim; rte++)
	{
	  p.prefix = rte->prefix;
	  p.prefixlen = ip_masklen (rte->mask);
	  apply_mask_ipv4 (&p);
	  
	  rp = route_node_lookup (rip->table, (struct prefix *) &p);
	  if (rp)
	    {
	      rinfo = rp->info;
	      rte->metric = htonl (rinfo->metric);
	      route_unlock_node (rp);
	    }
	  else
	    rte->metric = htonl (RIP_METRIC_INFINITY);
	}
      packet->command = RIP_RESPONSE;

      rip_send_packet ((caddr_t) packet, size, from, ifp);
    }
  rip_global_queries++;
}

#if RIP_RECVMSG
/* Set IPv6 packet info to the socket. */
static int
setsockopt_pktinfo (int sock)
{
  int ret;
  int val = 1;
    
  ret = setsockopt(sock, IPPROTO_IP, IP_PKTINFO, &val, sizeof(val));
  if (ret < 0)
    zlog_warn ("Can't setsockopt IP_PKTINFO : %s", strerror (errno));
  return ret;
}

/* Read RIP packet by recvmsg function. */
int
rip_recvmsg (int sock, u_char *buf, int size, struct sockaddr_in *from,
	     int *ifindex)
{
  int ret;
  struct msghdr msg;
  struct iovec iov;
  struct cmsghdr *ptr;
  char adata[1024];

  msg.msg_name = (void *) from;
  msg.msg_namelen = sizeof (struct sockaddr_in);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = (void *) adata;
  msg.msg_controllen = sizeof adata;
  iov.iov_base = buf;
  iov.iov_len = size;

  ret = recvmsg (sock, &msg, 0);
  if (ret < 0)
    return ret;

  for (ptr = CMSG_FIRSTHDR(&msg); ptr != NULL; ptr = CMSG_NXTHDR(&msg, ptr))
    if (ptr->cmsg_level == IPPROTO_IP && ptr->cmsg_type == IP_PKTINFO) 
      {
	struct in_pktinfo *pktinfo;
	int i;

	pktinfo = (struct in_pktinfo *) CMSG_DATA (ptr);
	i = pktinfo->ipi_ifindex;
      }
  return ret;
}

/* RIP packet read function. */
int
rip_read_new (struct thread *t)
{
  int ret;
  int sock;
  char buf[RIP_PACKET_MAXSIZ];
  struct sockaddr_in from;
  unsigned int ifindex;
  
  /* Fetch socket then register myself. */
  sock = THREAD_FD (t);
  rip_event (RIP_READ, sock);

  /* Read RIP packet. */
  ret = rip_recvmsg (sock, buf, RIP_PACKET_MAXSIZ, &from, (int *)&ifindex);
  if (ret < 0)
    {
      zlog_warn ("Can't read RIP packet: %s", strerror (errno));
      return ret;
    }

  return ret;
}
#endif /* RIP_RECVMSG */

/* First entry point of RIP packet. */
int
rip_read (struct thread *t)
{
  int sock;
  int ret;
  int rtenum;
  union rip_buf rip_buf;
  struct rip_packet *packet;
  struct sockaddr_in from;
  int fromlen, len;
  struct interface *ifp;
  struct rip_interface *ri;

  /* Fetch socket then register myself. */
  sock = THREAD_FD (t);
  rip->t_read = NULL;

  /* Add myself to tne next event */
  rip_event (RIP_READ, sock);

  /* RIPd manages only IPv4. */
  memset (&from, 0, sizeof (struct sockaddr_in));
  fromlen = sizeof (struct sockaddr_in);

  len = recvfrom (sock, (char *)&rip_buf.buf, sizeof (rip_buf.buf), 0, 
		  (struct sockaddr *) &from, &fromlen);
  if (len < 0) 
    {
      zlog_info ("recvfrom failed: %s", strerror (errno));
      return len;
    }

  /* Check is this packet comming from myself? */
  if (if_check_address (from.sin_addr)) 
    {
      if (IS_RIP_DEBUG_PACKET)
	zlog_warn ("ignore packet comes from myself");
      return -1;
    }

  /* Which interface is this packet comes from. */
  ifp = if_lookup_address (from.sin_addr);

  /* RIP packet received */
  if (IS_RIP_DEBUG_EVENT)
    zlog_info ("RECV packet from %s port %d on %s",
	       inet_ntoa (from.sin_addr), ntohs (from.sin_port),
	       ifp ? ifp->name : "unknown");

  /* If this packet come from unknown interface, ignore it. */
  if (ifp == NULL)
    {
      zlog_info ("packet comes from unknown interface");
      return -1;
    }

  /* Packet length check. */
  if (len < RIP_PACKET_MINSIZ)
    {
      zlog_warn ("packet size %d is smaller than minimum size %d",
		 len, RIP_PACKET_MINSIZ);
      rip_peer_bad_packet (&from);
      return len;
    }
  if (len > RIP_PACKET_MAXSIZ)
    {
      zlog_warn ("packet size %d is larger than max size %d",
		 len, RIP_PACKET_MAXSIZ);
      rip_peer_bad_packet (&from);
      return len;
    }

  /* Packet alignment check. */
  if ((len - RIP_PACKET_MINSIZ) % 20)
    {
      zlog_warn ("packet size %d is wrong for RIP packet alignment", len);
      rip_peer_bad_packet (&from);
      return len;
    }

  /* Set RTE number. */
  rtenum = ((len - RIP_PACKET_MINSIZ) / 20);

  /* For easy to handle. */
  packet = &rip_buf.rip_packet;

  /* RIP version check. */
  if (packet->version == 0 || packet->version > RIPv2)
    {
      zlog_info ("version 0 with command %d received.", packet->command);
      rip_peer_bad_packet (&from);
      return -1;
    }

  /* Dump RIP packet. */
  if (IS_RIP_DEBUG_RECV)
    rip_packet_dump (packet, len, "RECV");

  /* RIP version adjust.  This code should rethink now.  RFC1058 says
     that "Version 1 implementations are to ignore this extra data and
     process only the fields specified in this document.". So RIPv3
     packet should be treated as RIPv1 ignoring must be zero field. */

  /* Linksys test guy and CT RDQA says we should treat these packet as illegal,
     so dump it above. (packet->version == 0 || packet->version > RIPv2)
     Lili 2007-07-05 */
#if 0     
  if (packet->version > RIPv2)
    packet->version = RIPv2;
#endif

  /* Is RIP running or is this RIP neighbor ?*/
  ri = ifp->info;
  if (! ri->running && ! rip_neighbor_lookup (&from))
    {
      if (IS_RIP_DEBUG_EVENT)
	zlog_info ("RIP is not enabled on interface %s.", ifp->name);
      rip_peer_bad_packet (&from);
      return -1;
    }

  /* RIP Version check. */
  if (packet->command == RIP_RESPONSE)
    {
      if (ri->ri_receive == RI_RIP_UNSPEC)
	{
	  if (packet->version != rip->version) 
	    {
	      if (IS_RIP_DEBUG_PACKET)
		zlog_warn ("  packet's v%d doesn't fit to my version %d", 
			   packet->version, rip->version);
	      rip_peer_bad_packet (&from);
	      return -1;
	    }
	}
      else
	{
	  if (packet->version == RIPv1)
	    if (! (ri->ri_receive & RIPv1))
	      {
		if (IS_RIP_DEBUG_PACKET)
		  zlog_warn ("  packet's v%d doesn't fit to if version spec", 
			     packet->version);
		rip_peer_bad_packet (&from);
		return -1;
	      }
	  if (packet->version == RIPv2)
	    if (! (ri->ri_receive & RIPv2))
	      {
		if (IS_RIP_DEBUG_PACKET)
		  zlog_warn ("  packet's v%d doesn't fit to if version spec", 
			     packet->version);
		rip_peer_bad_packet (&from);
		return -1;
	      }
	}
    }

  /* RFC2453 5.2 If the router is not configured to authenticate RIP-2
     messages, then RIP-1 and unauthenticated RIP-2 messages will be
     accepted; authenticated RIP-2 messages shall be discarded.  */

  if ((ri->auth_type == RIP_NO_AUTH) 
      && rtenum 
      && (packet->version == RIPv2) && (packet->rte->family == 0xffff))
    {
      if (IS_RIP_DEBUG_EVENT)
	zlog_warn ("packet RIPv%d is dropped because authentication disabled", 
		   packet->version);
      rip_peer_bad_packet (&from);
      return -1;
    }

  /* If the router is configured to authenticate RIP-2 messages, then
     RIP-1 messages and RIP-2 messages which pass authentication
     testing shall be accepted; unauthenticated and failed
     authentication RIP-2 messages shall be discarded.  For maximum
     security, RIP-1 messages should be ignored when authentication is
     in use (see section 4.1); otherwise, the routing information from
     authenticated messages will be propagated by RIP-1 routers in an
     unauthenticated manner. */

  if ((ri->auth_type == RIP_AUTH_SIMPLE_PASSWORD 
       || ri->auth_type == RIP_AUTH_MD5)
      && rtenum)
    {
      /* We follow maximum security. */
      if (packet->version == RIPv1 && packet->rte->family == 0xffff)
	{
	  if (IS_RIP_DEBUG_PACKET)
	    zlog_warn ("packet RIPv%d is dropped because authentication enabled", packet->version);
	  rip_peer_bad_packet (&from);
	  return -1;
	}

      /* Check RIPv2 authentication. */
      if (packet->version == RIPv2)
	{
	  if (packet->rte->family == 0xffff)
	    {
	      if (ntohs (packet->rte->tag) == RIP_AUTH_SIMPLE_PASSWORD)
                {
		  ret = rip_auth_simple_password (packet->rte, &from, ifp);
		  if (! ret)
		    {
		      if (IS_RIP_DEBUG_EVENT)
			zlog_warn ("RIPv2 simple password authentication failed");
		      rip_peer_bad_packet (&from);
		      return -1;
		    }
		  else
		    {
		      if (IS_RIP_DEBUG_EVENT)
			zlog_info ("RIPv2 simple password authentication success");
		    }
                }
	      else if (ntohs (packet->rte->tag) == RIP_AUTH_MD5)
                {
		  ret = rip_auth_md5 (packet, &from, ifp);
		  if (! ret)
		    {
		      if (IS_RIP_DEBUG_EVENT)
			zlog_warn ("RIPv2 MD5 authentication failed");
		      rip_peer_bad_packet (&from);
		      return -1;
		    }
		  else
		    {
		      if (IS_RIP_DEBUG_EVENT)
			zlog_info ("RIPv2 MD5 authentication success");
		    }
		  /* Reset RIP packet length to trim MD5 data. */
		  len = ret; 
                }
	      else
		{
		  if (IS_RIP_DEBUG_EVENT)
		    zlog_warn ("Unknown authentication type %d",
			       ntohs (packet->rte->tag));
		  rip_peer_bad_packet (&from);
		  return -1;
		}
	    }
	  else
	    {
	      /* There is no authentication in the packet. */
	      if (ri->auth_str || ri->key_chain)
		{
		  if (IS_RIP_DEBUG_EVENT)
		    zlog_warn ("RIPv2 authentication failed: no authentication in packet");
		  rip_peer_bad_packet (&from);
		  return -1;
		}
	    }
	}
    }
  
  /* Process each command. */
  switch (packet->command)
    {
    case RIP_RESPONSE:
      rip_response_process (packet, len, &from, ifp);
      break;
    case RIP_REQUEST:
    case RIP_POLL:
      rip_request_process (packet, len, &from, ifp);
      break;
    case RIP_TRACEON:
    case RIP_TRACEOFF:
      zlog_info ("Obsolete command %s received, please sent it to routed", 
		 lookup (rip_msg, packet->command));
      rip_peer_bad_packet (&from);
      break;
    case RIP_POLL_ENTRY:
      zlog_info ("Obsolete command %s received", 
		 lookup (rip_msg, packet->command));
      rip_peer_bad_packet (&from);
      break;
    default:
      zlog_info ("Unknown RIP command %d received", packet->command);
      rip_peer_bad_packet (&from);
      break;
    }

  return len;
}

/* Make socket for RIP protocol. */
int 
rip_create_socket ()
{
  int ret;
  int sock;
  struct sockaddr_in addr;
  struct servent *sp;

  memset (&addr, 0, sizeof (struct sockaddr_in));

  /* Set RIP port. */
  sp = getservbyname ("router", "udp");
  if (sp) 
    addr.sin_port = sp->s_port;
  else 
    addr.sin_port = htons (RIP_PORT_DEFAULT);

  /* Address shoud be any address. */
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;

  /* Make datagram socket. */
  sock = socket (AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) 
    {
      perror ("socket");
      exit (1);
    }

  sockopt_broadcast (sock);
  sockopt_reuseaddr (sock);
  sockopt_reuseport (sock);
  sockopt_recvbuf (sock, RIP_UDP_RCV_BUF);
#ifdef RIP_RECVMSG
  setsockopt_pktinfo (sock);
#endif /* RIP_RECVMSG */

  ret = bind (sock, (struct sockaddr *) & addr, sizeof (addr));
  if (ret < 0)
    {
      perror ("bind");
      return ret;
    }
  
  return sock;
}

/* Write routing table entry to the stream and return next index of
   the routing table entry in the stream. */
int
rip_write_rte (int num, struct stream *s, struct prefix_ipv4 *p,
	       u_char version, struct rip_info *rinfo, struct interface *ifp)
{
  struct in_addr mask;
  struct rip_interface *ri;

  /* RIP packet header. */
  if (num == 0)
    {
      stream_putc (s, RIP_RESPONSE);
      stream_putc (s, version);
      stream_putw (s, 0);

      /* In case of we need RIPv2 authentication. */
      if (version == RIPv2 && ifp)
	{
	  ri = ifp->info;
	      
	  if (ri->auth_type == RIP_AUTH_SIMPLE_PASSWORD)
	    {
	      if (ri->auth_str)
		{
		  stream_putw (s, 0xffff);
		  stream_putw (s, RIP_AUTH_SIMPLE_PASSWORD);

		  memset ((s->data + s->putp), 0, 16);
		  strncpy ((s->data + s->putp), ri->auth_str, 16);
		  stream_set_putp (s, s->putp + 16);

		  num++;
		}
	      if (ri->key_chain)
		{
		  struct keychain *keychain;
		  struct key *key;

		  keychain = keychain_lookup (ri->key_chain);

		  if (keychain)
		    {
		      key = key_lookup_for_send (keychain);
		      
		      if (key)
			{
			  stream_putw (s, 0xffff);
			  stream_putw (s, RIP_AUTH_SIMPLE_PASSWORD);

			  memset ((s->data + s->putp), 0, 16);
			  strncpy ((s->data + s->putp), key->string, 16);
			  stream_set_putp (s, s->putp + 16);

			  num++;
			}
		    }
		}
	    }
	}
    }

  /* Write routing table entry. */
  if (version == RIPv1)
    {
      stream_putw (s, AF_INET);
      stream_putw (s, 0);
      stream_put_ipv4 (s, p->prefix.s_addr);
      stream_put_ipv4 (s, 0);
      stream_put_ipv4 (s, 0);
      stream_putl (s, rinfo->metric_out);
    }
  else
    {
      masklen2ip (p->prefixlen, &mask);

      stream_putw (s, AF_INET);
      stream_putw (s, rinfo->tag);
      stream_put_ipv4 (s, p->prefix.s_addr);
      stream_put_ipv4 (s, mask.s_addr);
      stream_put_ipv4 (s, rinfo->nexthop_out.s_addr);
      stream_putl (s, rinfo->metric_out);
    }

  return ++num;
}

/* Send update to the ifp or spcified neighbor. */
void
rip_output_process (struct interface *ifp, struct sockaddr_in *to,
		    int route_type, u_char version)
{
  int ret;
  struct stream *s;
  struct route_node *rp;
  struct rip_info *rinfo;
  struct rip_interface *ri;
  struct prefix_ipv4 *p;
  struct prefix_ipv4 classfull;
  int num;
  int rtemax;

  /* Logging output event. */
  if (IS_RIP_DEBUG_EVENT)
    {
      if (to)
	zlog_info ("update routes to neighbor %s", inet_ntoa (to->sin_addr));
      else
	zlog_info ("update routes on interface %s ifindex %d",
		   ifp->name, ifp->ifindex);
    }

  /* Set output stream. */
  s = rip->obuf;

  /* Reset stream and RTE counter. */
  stream_reset (s);
  num = 0;
  rtemax = (RIP_PACKET_MAXSIZ - 4) / 20;

  /* Get RIP interface. */
  ri = ifp->info;
    
  /* If output interface is in simple password authentication mode, we
     need space for authentication data.  */
  if (ri->auth_type == RIP_AUTH_SIMPLE_PASSWORD)
    rtemax -= 1;

  /* If output interface is in MD5 authentication mode, we need space
     for authentication header and data. */
  if (ri->auth_type == RIP_AUTH_MD5)
    rtemax -= 2;

  /* If output interface is in simple password authentication mode
     and string or keychain is specified we need space for auth. data */
  if (ri->auth_type == RIP_AUTH_SIMPLE_PASSWORD)
    {
      if (ri->key_chain)
	{
	  struct keychain *keychain;

	  keychain = keychain_lookup (ri->key_chain);
	  if (keychain)
	    if (key_lookup_for_send (keychain))
	      rtemax -=1;
	}
      else
	if (ri->auth_str)
	  rtemax -=1;
    }

  for (rp = route_top (rip->table); rp; rp = route_next (rp))
    if ((rinfo = rp->info) != NULL)
      {
	/* Some inheritance stuff:                                          */
	/* Before we process with ipv4 prefix we should mask it             */
	/* with Classful mask if we send RIPv1 packet.That's because        */
	/* user could set non-classful mask or we could get it by RIPv2     */
	/* or other protocol. checked with Cisco's way of life :)           */
	
	if (version == RIPv1)
	  {
	    memcpy (&classfull, &rp->p, sizeof (struct prefix_ipv4));

	    if (IS_RIP_DEBUG_PACKET)
	      zlog_info("%s/%d before RIPv1 mask check ",
			inet_ntoa (classfull.prefix), classfull.prefixlen);

	    apply_classful_mask_ipv4 (&classfull);
	    p = &classfull;

	    if (IS_RIP_DEBUG_PACKET)
	      zlog_info("%s/%d after RIPv1 mask check",
			inet_ntoa (p->prefix), p->prefixlen);
	  }
	else 
	  p = (struct prefix_ipv4 *) &rp->p;

	/* Apply output filters. */
	ret = rip_outgoing_filter (p, ri);
	if (ret < 0)
	  continue;

	/* Changed route only output. */
	if (route_type == rip_changed_route &&
	    (! (rinfo->flags & RIP_RTF_CHANGED)))
	  continue;

	/* Split horizon. */
	/* if (split_horizon == rip_split_horizon) */
	if (ri->split_horizon)
	  {
	    /* We perform split horizon for RIP and connected route. */
	    if ((rinfo->type == ZEBRA_ROUTE_RIP ||
		 rinfo->type == ZEBRA_ROUTE_CONNECT) &&
		rinfo->ifindex == ifp->ifindex)
	      continue;
	  }

	/* Preparation for route-map. */
	rinfo->metric_set = 0;
	rinfo->nexthop_out.s_addr = 0;
	rinfo->metric_out = rinfo->metric;
	rinfo->ifindex_out = ifp->ifindex;

	/* In order to avoid some local loops, if the RIP route has a
	   nexthop via this interface, keep the nexthop, otherwise set
	   it to 0. The nexthop should not be propagated beyond the
	   local broadcast/multicast area in order to avoid an IGP
	   multi-level recursive look-up.  For RIP and connected
	   route, we don't set next hop value automatically.  For
	   settting next hop to those routes, please use
	   route-map.  */

	if (rinfo->type != ZEBRA_ROUTE_RIP
	    && rinfo->type != ZEBRA_ROUTE_CONNECT
	    && rinfo->ifindex == ifp->ifindex)
	  rinfo->nexthop_out = rinfo->nexthop;
           
	/* Apply route map - continue, if deny */
	if (rip->route_map[rinfo->type].name
	    && rinfo->sub_type != RIP_ROUTE_INTERFACE)
	  {
	    ret = route_map_apply (rip->route_map[rinfo->type].map,
				   (struct prefix *)p, RMAP_RIP, rinfo);

	    if (ret == RMAP_DENYMATCH) 
	      {
		if (IS_RIP_DEBUG_PACKET)
		  zlog_info ("%s/%d is filtered by route-map",
			     inet_ntoa (p->prefix), p->prefixlen);
		continue;
	      }
	  }

	/* When route-map does not set metric. */
	if (! rinfo->metric_set)
	  {
	    /* If redistribute metric is set. */
	    if (rip->route_map[rinfo->type].metric_config
		&& rinfo->metric != RIP_METRIC_INFINITY)
	      {
		rinfo->metric_out = rip->route_map[rinfo->type].metric;
	      }
	    else
	      {
		/* If the route is not connected or localy generated
		   one, use default-metric value*/
		if (rinfo->type != ZEBRA_ROUTE_RIP 
		    && rinfo->type != ZEBRA_ROUTE_CONNECT
		    && rinfo->metric != RIP_METRIC_INFINITY)
		  rinfo->metric_out = rip->default_metric;
	      }
	  }

	/* Apply offset-list */
	if (rinfo->metric != RIP_METRIC_INFINITY)
	  rip_offset_list_apply_out (p, ifp, &rinfo->metric_out);

	if (rinfo->metric_out > RIP_METRIC_INFINITY)
	  rinfo->metric_out = RIP_METRIC_INFINITY;
	  
	/* Write RTE to the stream. */
	num = rip_write_rte (num, s, p, version, rinfo, to ? NULL : ifp);
	if (num == rtemax)
	  {
	    if (version == RIPv2 && ri->auth_type == RIP_AUTH_MD5)
	      rip_auth_md5_set (s, ifp);

	    ret = rip_send_packet (STREAM_DATA (s), stream_get_endp (s),
				   to, ifp);

	    if (ret >= 0 && IS_RIP_DEBUG_SEND)
	      rip_packet_dump ((struct rip_packet *)STREAM_DATA (s),
			       stream_get_endp(s), "SEND");
	    num = 0;
	    stream_reset (s);
	  }
      }

  /* Flush unwritten RTE. */
  if (num != 0)
    {
      if (version == RIPv2 && ri->auth_type == RIP_AUTH_MD5)
	rip_auth_md5_set (s, ifp);

      ret = rip_send_packet (STREAM_DATA (s), stream_get_endp (s), to, ifp);

      if (ret >= 0 && IS_RIP_DEBUG_SEND)
	rip_packet_dump ((struct rip_packet *)STREAM_DATA (s),
			 stream_get_endp (s), "SEND");
      num = 0;
      stream_reset (s);
    }

  /* Statistics updates. */
  ri->sent_updates++;
}

/* Send RIP packet to the interface. */
void
rip_update_interface (struct interface *ifp, u_char version, int route_type)
{
  struct prefix_ipv4 *p;
  struct connected *connected;
  listnode node;
  struct sockaddr_in to;

  /* When RIP version is 2 and multicast enable interface. */
  if (version == RIPv2 && if_is_multicast (ifp)) 
    {
      if (IS_RIP_DEBUG_EVENT)
	zlog_info ("multicast announce on %s ", ifp->name);

      rip_output_process (ifp, NULL, route_type, version);
      return;
    }

  /* If we can't send multicast packet, send it with unicast. */
  if (if_is_broadcast (ifp) || if_is_pointopoint (ifp))
    {
      for (node = listhead (ifp->connected); node; nextnode (node))
	{	    
	  connected = getdata (node);

	  /* Fetch broadcast address or poin-to-point destination
             address . */
	  p = (struct prefix_ipv4 *) connected->destination;

	  if (p->family == AF_INET)
	    {
	      /* Destination address and port setting. */
	      memset (&to, 0, sizeof (struct sockaddr_in));
	      to.sin_addr = p->prefix;
	      to.sin_port = htons (RIP_PORT_DEFAULT);

	      if (IS_RIP_DEBUG_EVENT)
		zlog_info ("%s announce to %s on %s",
			   if_is_pointopoint (ifp) ? "unicast" : "broadcast",
			   inet_ntoa (to.sin_addr), ifp->name);

	      rip_output_process (ifp, &to, route_type, version);
	    }
	}
    }
}

/* Update send to all interface and neighbor. */
void
rip_update_process (int route_type)
{
  listnode node;
  struct interface *ifp;
  struct rip_interface *ri;
  struct route_node *rp;
  struct sockaddr_in to;
  struct prefix_ipv4 *p;

  /* Send RIP update to each interface. */
  for (node = listhead (iflist); node; nextnode (node))
    {
      ifp = getdata (node);

      if (if_is_loopback (ifp))
	continue;

      if (! if_is_up (ifp))
	continue;

      /* Fetch RIP interface information. */
      ri = ifp->info;

      /* When passive interface is specified, suppress announce to the
         interface. */
      if (ri->passive)
	continue;

      if (ri->running)
	{
	  if (IS_RIP_DEBUG_EVENT) 
	    {
	      if (ifp->name) 
		zlog_info ("SEND UPDATE to %s ifindex %d",
			   ifp->name, ifp->ifindex);
	      else
		zlog_info ("SEND UPDATE to _unknown_ ifindex %d",
			   ifp->ifindex);
	    }

	  /* If there is no version configuration in the interface,
             use rip's version setting. */
	  if (ri->ri_send == RI_RIP_UNSPEC)
	    {
	      if (rip->version == RIPv1)
		rip_update_interface (ifp, RIPv1, route_type);
	      else
		rip_update_interface (ifp, RIPv2, route_type);
	    }
	  /* If interface has RIP version configuration use it. */
	  else
	    {
	      if (ri->ri_send & RIPv1)
		rip_update_interface (ifp, RIPv1, route_type);
	      if (ri->ri_send & RIPv2)
		rip_update_interface (ifp, RIPv2, route_type);
	    }
	}
    }

  /* RIP send updates to each neighbor. */
  for (rp = route_top (rip->neighbor); rp; rp = route_next (rp))
    if (rp->info != NULL)
      {
	p = (struct prefix_ipv4 *) &rp->p;

	ifp = if_lookup_address (p->prefix);
	if (! ifp)
	  {
	    zlog_warn ("Neighbor %s doesn't exist direct connected network",
		       inet_ntoa (p->prefix));
	    continue;
	  }

	/* Set destination address and port */
	memset (&to, 0, sizeof (struct sockaddr_in));
	to.sin_addr = p->prefix;
	to.sin_port = htons (RIP_PORT_DEFAULT);

	/* RIP version is rip's configuration. */
	rip_output_process (ifp, &to, route_type, rip->version);
      }
}

/* RIP's periodical timer. */
int
rip_update (struct thread *t)
{
  /* Clear timer pointer. */
  rip->t_update = NULL;

  if (IS_RIP_DEBUG_EVENT)
    zlog_info ("update timer fire!");

  /* Process update output. */
  rip_update_process (rip_all_route);

  /* Triggered updates may be suppressed if a regular update is due by
     the time the triggered update would be sent. */
  if (rip->t_triggered_interval)
    {
      thread_cancel (rip->t_triggered_interval);
      rip->t_triggered_interval = NULL;
    }
  rip->trigger = 0;

  /* Register myself. */
  rip_event (RIP_UPDATE_EVENT, 0);

  return 0;
}

/* Walk down the RIP routing table then clear changed flag. */
void
rip_clear_changed_flag ()
{
  struct route_node *rp;
  struct rip_info *rinfo;

  for (rp = route_top (rip->table); rp; rp = route_next (rp))
    if ((rinfo = rp->info) != NULL)
      if (rinfo->flags & RIP_RTF_CHANGED)
	rinfo->flags &= ~RIP_RTF_CHANGED;
}

/* Triggered update interval timer. */
int
rip_triggered_interval (struct thread *t)
{
  int rip_triggered_update (struct thread *);

  rip->t_triggered_interval = NULL;

  if (rip->trigger)
    {
      rip->trigger = 0;
      rip_triggered_update (t);
    }
  return 0;
}     

/* Execute triggered update. */
int
rip_triggered_update (struct thread *t)
{
  int interval;

  /* Clear thred pointer. */
  rip->t_triggered_update = NULL;

  /* Cancel interval timer. */
  if (rip->t_triggered_interval)
    {
      thread_cancel (rip->t_triggered_interval);
      rip->t_triggered_interval = NULL;
    }
  rip->trigger = 0;

  /* Logging triggered update. */
  if (IS_RIP_DEBUG_EVENT)
    zlog_info ("triggered update!");

  /* Split Horizon processing is done when generating triggered
     updates as well as normal updates (see section 2.6). */
  rip_update_process (rip_changed_route);

  /* Once all of the triggered updates have been generated, the route
     change flags should be cleared. */
  rip_clear_changed_flag ();

  /* After a triggered update is sent, a timer should be set for a
     random interval between 1 and 5 seconds.  If other changes that
     would trigger updates occur before the timer expires, a single
     update is triggered when the timer expires. */
  interval = (random () % 5) + 1;

  rip->t_triggered_interval = 
    thread_add_timer (master, rip_triggered_interval, NULL, interval);

  return 0;
}

/* Withdraw redistributed route. */
void
rip_redistribute_withdraw (int type)
{
  struct route_node *rp;
  struct rip_info *rinfo;

  if (!rip)
    return;

  for (rp = route_top (rip->table); rp; rp = route_next (rp))
    if ((rinfo = rp->info) != NULL)
      {
	if (rinfo->type == type
	    && rinfo->sub_type != RIP_ROUTE_INTERFACE)
	  {
	    /* Perform poisoned reverse. */
	    rinfo->metric = RIP_METRIC_INFINITY;
	    RIP_TIMER_ON (rinfo->t_garbage_collect, 
			  rip_garbage_collect, rip->garbage_time);
	    RIP_TIMER_OFF (rinfo->t_timeout);
	    rinfo->flags |= RIP_RTF_CHANGED;

	    rip_event (RIP_TRIGGERED_UPDATE, 0);
	  }
      }
}

/* Create new RIP instance and set it to global variable. */
int
rip_create ()
{
  rip = XMALLOC (MTYPE_RIP, sizeof (struct rip));
  memset (rip, 0, sizeof (struct rip));

  /* Set initial value. */
  rip->version = RIPv2;
  rip->update_time = RIP_UPDATE_TIMER_DEFAULT;
  rip->timeout_time = RIP_TIMEOUT_TIMER_DEFAULT;
  rip->garbage_time = RIP_GARBAGE_TIMER_DEFAULT;
  rip->default_metric = RIP_DEFAULT_METRIC_DEFAULT;

  /* Initialize RIP routig table. */
  rip->table = route_table_init ();
  rip->route = route_table_init ();
  rip->neighbor = route_table_init ();

  /* Make output stream. */
  rip->obuf = stream_new (1500);

  /* Make socket. */
  rip->sock = rip_create_socket ();
  if (rip->sock < 0)
    return rip->sock;

  /* Create read and timer thread. */
  rip_event (RIP_READ, rip->sock);
  rip_event (RIP_UPDATE_EVENT, 1);

  return 0;
}

/* Sned RIP request to the destination. */
int
rip_request_send (struct sockaddr_in *to, struct interface *ifp,
		  u_char version)
{
  struct rte *rte;
  struct rip_packet rip_packet;

  memset (&rip_packet, 0, sizeof (rip_packet));

  rip_packet.command = RIP_REQUEST;
  rip_packet.version = version;
  rte = rip_packet.rte;
  rte->metric = htonl (RIP_METRIC_INFINITY);

  return rip_send_packet ((caddr_t) &rip_packet, sizeof (rip_packet), to, ifp);
}

int
rip_update_jitter (unsigned long time)
{
  return ((rand () % (time + 1)) - (time / 2));
}

void
rip_event (enum rip_event event, int sock)
{
  int jitter = 0;

  switch (event)
    {
    case RIP_READ:
      rip->t_read = thread_add_read (master, rip_read, NULL, sock);
      break;
    case RIP_UPDATE_EVENT:
      if (rip->t_update)
	{
	  thread_cancel (rip->t_update);
	  rip->t_update = NULL;
	}
      jitter = rip_update_jitter (rip->update_time);
      rip->t_update = 
	thread_add_timer (master, rip_update, NULL, 
			  sock ? 2 : rip->update_time + jitter);
      break;
    case RIP_TRIGGERED_UPDATE:
      if (rip->t_triggered_interval)
	rip->trigger = 1;
      else if (! rip->t_triggered_update)
	rip->t_triggered_update = 
	  thread_add_event (master, rip_triggered_update, NULL, 0);
      break;
    default:
      break;
    }
}

DEFUN (router_rip,
       router_rip_cmd,
       "router rip",
       "Enable a routing process\n"
       "Routing Information Protocol (RIP)\n")
{
  int ret;

  /* If rip is not enabled before. */
  if (! rip)
    {
      ret = rip_create ();
      if (ret < 0)
	{
	  zlog_info ("Can't create RIP");
	  return CMD_WARNING;
	}
    }
  vty->node = RIP_NODE;
  vty->index = rip;

  return CMD_SUCCESS;
}

DEFUN (no_router_rip,
       no_router_rip_cmd,
       "no router rip",
       NO_STR
       "Enable a routing process\n"
       "Routing Information Protocol (RIP)\n")
{
  if (rip)
    rip_clean ();
  return CMD_SUCCESS;
}

DEFUN (rip_version,
       rip_version_cmd,
       "version <1-2>",
       "Set routing protocol version\n"
       "version\n")
{
  int version;

  version = atoi (argv[0]);
  if (version != RIPv1 && version != RIPv2)
    {
      vty_out (vty, "invalid rip version %d%s", version,
	       VTY_NEWLINE);
      return CMD_WARNING;
    }
  rip->version = version;

  return CMD_SUCCESS;
} 

DEFUN (no_rip_version,
       no_rip_version_cmd,
       "no version",
       NO_STR
       "Set routing protocol version\n")
{
  /* Set RIP version to the default. */
  rip->version = RIPv2;

  return CMD_SUCCESS;
} 

ALIAS (no_rip_version,
       no_rip_version_val_cmd,
       "no version <1-2>",
       NO_STR
       "Set routing protocol version\n"
       "version\n");

DEFUN (rip_route,
       rip_route_cmd,
       "route A.B.C.D/M",
       "RIP static route configuration\n"
       "IP prefix <network>/<length>\n")
{
  int ret;
  struct prefix_ipv4 p;
  struct route_node *node;

  ret = str2prefix_ipv4 (argv[0], &p);
  if (ret < 0)
    {
      vty_out (vty, "Malformed address%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  apply_mask_ipv4 (&p);

  /* For router rip configuration. */
  node = route_node_get (rip->route, (struct prefix *) &p);

  if (node->info)
    {
      vty_out (vty, "There is already same static route.%s", VTY_NEWLINE);
      route_unlock_node (node);
      return CMD_WARNING;
    }

  node->info = "static";

  rip_redistribute_add (ZEBRA_ROUTE_RIP, RIP_ROUTE_STATIC, &p, 0, NULL);

  return CMD_SUCCESS;
}

DEFUN (no_rip_route,
       no_rip_route_cmd,
       "no route A.B.C.D/M",
       NO_STR
       "RIP static route configuration\n"
       "IP prefix <network>/<length>\n")
{
  int ret;
  struct prefix_ipv4 p;
  struct route_node *node;

  ret = str2prefix_ipv4 (argv[0], &p);
  if (ret < 0)
    {
      vty_out (vty, "Malformed address%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  apply_mask_ipv4 (&p);

  /* For router rip configuration. */
  node = route_node_lookup (rip->route, (struct prefix *) &p);
  if (! node)
    {
      vty_out (vty, "Can't find route %s.%s", argv[0],
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  rip_redistribute_delete (ZEBRA_ROUTE_RIP, RIP_ROUTE_STATIC, &p, 0);
  route_unlock_node (node);

  node->info = NULL;
  route_unlock_node (node);

  return CMD_SUCCESS;
}

void
rip_update_default_metric ()
{
  struct route_node *np;
  struct rip_info *rinfo;

  for (np = route_top (rip->table); np; np = route_next (np))
    if ((rinfo = np->info) != NULL)
      if (rinfo->type != ZEBRA_ROUTE_RIP && rinfo->type != ZEBRA_ROUTE_CONNECT)
        rinfo->metric = rip->default_metric;
}

DEFUN (rip_default_metric,
       rip_default_metric_cmd,
       "default-metric <1-16>",
       "Set a metric of redistribute routes\n"
       "Default metric\n")
{
  if (rip)
    {
      rip->default_metric = atoi (argv[0]);
      /* rip_update_default_metric (); */
    }
  return CMD_SUCCESS;
}

DEFUN (no_rip_default_metric,
       no_rip_default_metric_cmd,
       "no default-metric",
       NO_STR
       "Set a metric of redistribute routes\n"
       "Default metric\n")
{
  if (rip)
    {
      rip->default_metric = RIP_DEFAULT_METRIC_DEFAULT;
      /* rip_update_default_metric (); */
    }
  return CMD_SUCCESS;
}

ALIAS (no_rip_default_metric,
       no_rip_default_metric_val_cmd,
       "no default-metric <1-16>",
       NO_STR
       "Set a metric of redistribute routes\n"
       "Default metric\n");

DEFUN (rip_timers,
       rip_timers_cmd,
       "timers basic <5-2147483647> <5-2147483647> <5-2147483647>",
       "Adjust routing timers\n"
       "Basic routing protocol update timers\n"
       "Routing table update timer value in second. Default is 30.\n"
       "Routing information timeout timer. Default is 180.\n"
       "Garbage collection timer. Default is 120.\n")
{
  unsigned long update;
  unsigned long timeout;
  unsigned long garbage;
  char *endptr = NULL;
  unsigned long RIP_TIMER_MAX = 2147483647;
  unsigned long RIP_TIMER_MIN = 5;

  update = strtoul (argv[0], &endptr, 10);
  if (update > RIP_TIMER_MAX || update < RIP_TIMER_MIN || *endptr != '\0')  
    {
      vty_out (vty, "update timer value error%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  
  timeout = strtoul (argv[1], &endptr, 10);
  if (timeout > RIP_TIMER_MAX || timeout < RIP_TIMER_MIN || *endptr != '\0') 
    {
      vty_out (vty, "timeout timer value error%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  
  garbage = strtoul (argv[2], &endptr, 10);
  if (garbage > RIP_TIMER_MAX || garbage < RIP_TIMER_MIN || *endptr != '\0') 
    {
      vty_out (vty, "garbage timer value error%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Set each timer value. */
  rip->update_time = update;
  rip->timeout_time = timeout;
  rip->garbage_time = garbage;

  /* Reset update timer thread. */
  rip_event (RIP_UPDATE_EVENT, 0);

  return CMD_SUCCESS;
}

DEFUN (no_rip_timers,
       no_rip_timers_cmd,
       "no timers basic",
       NO_STR
       "Adjust routing timers\n"
       "Basic routing protocol update timers\n")
{
  /* Set each timer value to the default. */
  rip->update_time = RIP_UPDATE_TIMER_DEFAULT;
  rip->timeout_time = RIP_TIMEOUT_TIMER_DEFAULT;
  rip->garbage_time = RIP_GARBAGE_TIMER_DEFAULT;

  /* Reset update timer thread. */
  rip_event (RIP_UPDATE_EVENT, 0);

  return CMD_SUCCESS;
}

struct route_table *rip_distance_table;

struct rip_distance
{
  /* Distance value for the IP source prefix. */
  u_char distance;

  /* Name of the access-list to be matched. */
  char *access_list;
};

struct rip_distance *
rip_distance_new ()
{
  struct rip_distance *new;
  new = XMALLOC (MTYPE_RIP_DISTANCE, sizeof (struct rip_distance));
  memset (new, 0, sizeof (struct rip_distance));
  return new;
}

void
rip_distance_free (struct rip_distance *rdistance)
{
  XFREE (MTYPE_RIP_DISTANCE, rdistance);
}

int
rip_distance_set (struct vty *vty, char *distance_str, char *ip_str,
		  char *access_list_str)
{
  int ret;
  struct prefix_ipv4 p;
  u_char distance;
  struct route_node *rn;
  struct rip_distance *rdistance;

  ret = str2prefix_ipv4 (ip_str, &p);
  if (ret == 0)
    {
      vty_out (vty, "Malformed prefix%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  distance = atoi (distance_str);

  /* Get RIP distance node. */
  rn = route_node_get (rip_distance_table, (struct prefix *) &p);
  if (rn->info)
    {
      rdistance = rn->info;
      route_unlock_node (rn);
    }
  else
    {
      rdistance = rip_distance_new ();
      rn->info = rdistance;
    }

  /* Set distance value. */
  rdistance->distance = distance;

  /* Reset access-list configuration. */
  if (rdistance->access_list)
    {
      free (rdistance->access_list);
      rdistance->access_list = NULL;
    }
  if (access_list_str)
    rdistance->access_list = strdup (access_list_str);

  return CMD_SUCCESS;
}

int
rip_distance_unset (struct vty *vty, char *distance_str, char *ip_str,
		    char *access_list_str)
{
  int ret;
  struct prefix_ipv4 p;
  u_char distance;
  struct route_node *rn;
  struct rip_distance *rdistance;

  ret = str2prefix_ipv4 (ip_str, &p);
  if (ret == 0)
    {
      vty_out (vty, "Malformed prefix%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  distance = atoi (distance_str);

  rn = route_node_lookup (rip_distance_table, (struct prefix *)&p);
  if (! rn)
    {
      vty_out (vty, "Can't find specified prefix%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  rdistance = rn->info;

  if (rdistance->access_list)
    free (rdistance->access_list);
  rip_distance_free (rdistance);

  rn->info = NULL;
  route_unlock_node (rn);
  route_unlock_node (rn);

  return CMD_SUCCESS;
}

void
rip_distance_reset ()
{
  struct route_node *rn;
  struct rip_distance *rdistance;

  for (rn = route_top (rip_distance_table); rn; rn = route_next (rn))
    if ((rdistance = rn->info) != NULL)
      {
	if (rdistance->access_list)
	  free (rdistance->access_list);
	rip_distance_free (rdistance);
	rn->info = NULL;
	route_unlock_node (rn);
      }
}

/* Apply RIP information to distance method. */
u_char
rip_distance_apply (struct rip_info *rinfo)
{
  struct route_node *rn;
  struct prefix_ipv4 p;
  struct rip_distance *rdistance;
  struct access_list *alist;

  if (! rip)
    return 0;

  memset (&p, 0, sizeof (struct prefix_ipv4));
  p.family = AF_INET;
  p.prefix = rinfo->from;
  p.prefixlen = IPV4_MAX_BITLEN;

  /* Check source address. */
  rn = route_node_match (rip_distance_table, (struct prefix *) &p);
  if (rn)
    {
      rdistance = rn->info;
      route_unlock_node (rn);

      if (rdistance->access_list)
	{
	  alist = access_list_lookup (AFI_IP, rdistance->access_list);
	  if (alist == NULL)
	    return 0;
	  if (access_list_apply (alist, &rinfo->rp->p) == FILTER_DENY)
	    return 0;

	  return rdistance->distance;
	}
      else
	return rdistance->distance;
    }

  if (rip->distance)
    return rip->distance;

  return 0;
}

void
rip_distance_show (struct vty *vty)
{
  struct route_node *rn;
  struct rip_distance *rdistance;
  int header = 1;
  char buf[BUFSIZ];
  
  vty_out (vty, "  Distance: (default is %d)%s",
	   rip->distance ? rip->distance :ZEBRA_RIP_DISTANCE_DEFAULT,
	   VTY_NEWLINE);

  for (rn = route_top (rip_distance_table); rn; rn = route_next (rn))
    if ((rdistance = rn->info) != NULL)
      {
	if (header)
	  {
	    vty_out (vty, "    Address           Distance  List%s",
		     VTY_NEWLINE);
	    header = 0;
	  }
	sprintf (buf, "%s/%d", inet_ntoa (rn->p.u.prefix4), rn->p.prefixlen);
	vty_out (vty, "    %-20s  %4d  %s%s",
		 buf, rdistance->distance,
		 rdistance->access_list ? rdistance->access_list : "",
		 VTY_NEWLINE);
      }
}

DEFUN (rip_distance,
       rip_distance_cmd,
       "distance <1-255>",
       "Administrative distance\n"
       "Distance value\n")
{
  rip->distance = atoi (argv[0]);
  return CMD_SUCCESS;
}

DEFUN (no_rip_distance,
       no_rip_distance_cmd,
       "no distance <1-255>",
       NO_STR
       "Administrative distance\n"
       "Distance value\n")
{
  rip->distance = 0;
  return CMD_SUCCESS;
}

DEFUN (rip_distance_source,
       rip_distance_source_cmd,
       "distance <1-255> A.B.C.D/M",
       "Administrative distance\n"
       "Distance value\n"
       "IP source prefix\n")
{
  rip_distance_set (vty, argv[0], argv[1], NULL);
  return CMD_SUCCESS;
}

DEFUN (no_rip_distance_source,
       no_rip_distance_source_cmd,
       "no distance <1-255> A.B.C.D/M",
       NO_STR
       "Administrative distance\n"
       "Distance value\n"
       "IP source prefix\n")
{
  rip_distance_unset (vty, argv[0], argv[1], NULL);
  return CMD_SUCCESS;
}

DEFUN (rip_distance_source_access_list,
       rip_distance_source_access_list_cmd,
       "distance <1-255> A.B.C.D/M WORD",
       "Administrative distance\n"
       "Distance value\n"
       "IP source prefix\n"
       "Access list name\n")
{
  rip_distance_set (vty, argv[0], argv[1], argv[2]);
  return CMD_SUCCESS;
}

DEFUN (no_rip_distance_source_access_list,
       no_rip_distance_source_access_list_cmd,
       "no distance <1-255> A.B.C.D/M WORD",
       NO_STR
       "Administrative distance\n"
       "Distance value\n"
       "IP source prefix\n"
       "Access list name\n")
{
  rip_distance_unset (vty, argv[0], argv[1], argv[2]);
  return CMD_SUCCESS;
}

/* Print out routes update time. */
void
rip_vty_out_uptime (struct vty *vty, struct rip_info *rinfo)
{
  struct timeval timer_now;
  time_t clock;
  struct tm *tm;
#define TIME_BUF 25
  char timebuf [TIME_BUF];
  struct thread *thread;

  gettimeofday (&timer_now, NULL);

  if ((thread = rinfo->t_timeout) != NULL)
    {
      clock = thread->u.sands.tv_sec - timer_now.tv_sec;
      tm = gmtime (&clock);
      strftime (timebuf, TIME_BUF, "%M:%S", tm);
      vty_out (vty, "%5s", timebuf);
    }
  else if ((thread = rinfo->t_garbage_collect) != NULL)
    {
      clock = thread->u.sands.tv_sec - timer_now.tv_sec;
      tm = gmtime (&clock);
      strftime (timebuf, TIME_BUF, "%M:%S", tm);
      vty_out (vty, "%5s", timebuf);
    }
}

char *
rip_route_type_print (int sub_type)
{
  switch (sub_type)
    {
    case RIP_ROUTE_RTE:
      return "n";
    case RIP_ROUTE_STATIC:
      return "s";
    case RIP_ROUTE_DEFAULT:
      return "d";
    case RIP_ROUTE_REDISTRIBUTE:
      return "r";
    case RIP_ROUTE_INTERFACE:
      return "i";
    default:
      return "?";
    }
}

DEFUN (show_ip_rip,
       show_ip_rip_cmd,
       "show ip rip",
       SHOW_STR
       IP_STR
       "Show RIP routes\n")
{
  struct route_node *np;
  struct rip_info *rinfo;

  if (! rip)
    return CMD_SUCCESS;

  vty_out (vty, "Codes: R - RIP, C - connected, O - OSPF, B - BGP%s"
	   "      (n) - normal, (s) - static, (d) - default, (r) - redistribute,%s"
	   "      (i) - interface%s%s"
	   "     Network            Next Hop         Metric From            Time%s",
	   VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE, VTY_NEWLINE);
  
  for (np = route_top (rip->table); np; np = route_next (np))
    if ((rinfo = np->info) != NULL)
      {
	int len;

	len = vty_out (vty, "%s(%s) %s/%d",
		       /* np->lock, For debugging. */
		       route_info[rinfo->type].str,
		       rip_route_type_print (rinfo->sub_type),
		       inet_ntoa (np->p.u.prefix4), np->p.prefixlen);
	
	len = 24 - len;

	if (len > 0)
	  vty_out (vty, "%*s", len, " ");

        if (rinfo->nexthop.s_addr) 
	  vty_out (vty, "%-20s %2d ", inet_ntoa (rinfo->nexthop),
		   rinfo->metric);
        else
	  vty_out (vty, "0.0.0.0              %2d ", rinfo->metric);

	/* Route which exist in kernel routing table. */
	if ((rinfo->type == ZEBRA_ROUTE_RIP) && 
	    (rinfo->sub_type == RIP_ROUTE_RTE))
	  {
	    vty_out (vty, "%-15s ", inet_ntoa (rinfo->from));
	    rip_vty_out_uptime (vty, rinfo);
	  }
	else if (rinfo->metric == RIP_METRIC_INFINITY)
	  {
	    vty_out (vty, "self            ");
	    rip_vty_out_uptime (vty, rinfo);
	  }
	else
	  vty_out (vty, "self");

	vty_out (vty, "%s", VTY_NEWLINE);
      }
  return CMD_SUCCESS;
}

/* Return next event time. */
int
rip_next_thread_timer (struct thread *thread)
{
  struct timeval timer_now;

  gettimeofday (&timer_now, NULL);

  return thread->u.sands.tv_sec - timer_now.tv_sec;
}

DEFUN (show_ip_protocols_rip,
       show_ip_protocols_rip_cmd,
       "show ip protocols",
       SHOW_STR
       IP_STR
       "IP routing protocol process parameters and statistics\n")
{
  listnode node;
  struct interface *ifp;
  struct rip_interface *ri;
  extern struct message ri_version_msg[];
  char *send_version;
  char *receive_version;

  if (! rip)
    return CMD_SUCCESS;

  vty_out (vty, "Routing Protocol is \"rip\"%s", VTY_NEWLINE);
  vty_out (vty, "  Sending updates every %ld seconds with +/-50%%,",
	   rip->update_time);
  vty_out (vty, " next due in %d seconds%s", 
	   rip_next_thread_timer (rip->t_update),
	   VTY_NEWLINE);
  vty_out (vty, "  Timeout after %ld seconds,", rip->timeout_time);
  vty_out (vty, " garbage collect after %ld seconds%s", rip->garbage_time,
	   VTY_NEWLINE);

  /* Filtering status show. */
  config_show_distribute (vty);
		 
  /* Default metric information. */
  vty_out (vty, "  Default redistribution metric is %d%s",
	   rip->default_metric, VTY_NEWLINE);

  /* Redistribute information. */
  vty_out (vty, "  Redistributing:");
  config_write_rip_redistribute (vty, 0);
  vty_out (vty, "%s", VTY_NEWLINE);

  vty_out (vty, "  Default version control: send version %d,", rip->version);
  vty_out (vty, " receive version %d %s", rip->version,
	   VTY_NEWLINE);

  vty_out (vty, "    Interface        Send  Recv   Key-chain%s", VTY_NEWLINE);

  for (node = listhead (iflist); node; node = nextnode (node))
    {
      ifp = getdata (node);
      ri = ifp->info;

      if (ri->enable_network || ri->enable_interface)
	{
	  if (ri->ri_send == RI_RIP_UNSPEC)
	    send_version = lookup (ri_version_msg, rip->version);
	  else
	    send_version = lookup (ri_version_msg, ri->ri_send);

	  if (ri->ri_receive == RI_RIP_UNSPEC)
	    receive_version = lookup (ri_version_msg, rip->version);
	  else
	    receive_version = lookup (ri_version_msg, ri->ri_receive);
	
	  vty_out (vty, "    %-17s%-3s   %-3s    %s%s", ifp->name,
		   send_version,
		   receive_version,
		   ri->key_chain ? ri->key_chain : "",
		   VTY_NEWLINE);
	}
    }

  vty_out (vty, "  Routing for Networks:%s", VTY_NEWLINE);
  config_write_rip_network (vty, 0);  

  vty_out (vty, "  Routing Information Sources:%s", VTY_NEWLINE);
  vty_out (vty, "    Gateway          BadPackets BadRoutes  Distance Last Update%s", VTY_NEWLINE);
  rip_peer_display (vty);

  rip_distance_show (vty);

  return CMD_SUCCESS;
}

/* RIP configuration write function. */
int
config_write_rip (struct vty *vty)
{
  int write = 0;
  struct route_node *rn;
  struct rip_distance *rdistance;

  if (rip)
    {
      /* Router RIP statement. */
      vty_out (vty, "router rip%s", VTY_NEWLINE);
      write++;
  
      /* RIP version statement.  Default is RIP version 2. */
      if (rip->version != RIPv2)
	vty_out (vty, " version %d%s", rip->version,
		 VTY_NEWLINE);
 
      /* RIP timer configuration. */
      if (rip->update_time != RIP_UPDATE_TIMER_DEFAULT 
	  || rip->timeout_time != RIP_TIMEOUT_TIMER_DEFAULT 
	  || rip->garbage_time != RIP_GARBAGE_TIMER_DEFAULT)
	vty_out (vty, " timers basic %lu %lu %lu%s",
		 rip->update_time,
		 rip->timeout_time,
		 rip->garbage_time,
		 VTY_NEWLINE);

      /* Default information configuration. */
      if (rip->default_information)
	{
	  if (rip->default_information_route_map)
	    vty_out (vty, " default-information originate route-map %s%s",
		     rip->default_information_route_map, VTY_NEWLINE);
	  else
	    vty_out (vty, " default-information originate%s",
		     VTY_NEWLINE);
	}

      /* Redistribute configuration. */
      config_write_rip_redistribute (vty, 1);

      /* RIP offset-list configuration. */
      config_write_rip_offset_list (vty);

      /* RIP enabled network and interface configuration. */
      config_write_rip_network (vty, 1);
			
      /* RIP default metric configuration */
      if (rip->default_metric != RIP_DEFAULT_METRIC_DEFAULT)
        vty_out (vty, " default-metric %d%s",
		 rip->default_metric, VTY_NEWLINE);

      /* Distribute configuration. */
      write += config_write_distribute (vty);

      /* Distance configuration. */
      if (rip->distance)
	vty_out (vty, " distance %d%s", rip->distance, VTY_NEWLINE);

      /* RIP source IP prefix distance configuration. */
      for (rn = route_top (rip_distance_table); rn; rn = route_next (rn))
	if ((rdistance = rn->info) != NULL)
	  vty_out (vty, " distance %d %s/%d %s%s", rdistance->distance,
		   inet_ntoa (rn->p.u.prefix4), rn->p.prefixlen,
		   rdistance->access_list ? rdistance->access_list : "",
		   VTY_NEWLINE);

      /* RIP static route configuration. */
      for (rn = route_top (rip->route); rn; rn = route_next (rn))
	if (rn->info)
	  vty_out (vty, " route %s/%d%s", 
		   inet_ntoa (rn->p.u.prefix4),
		   rn->p.prefixlen,
		   VTY_NEWLINE);

    }
  return write;
}

/* RIP node structure. */
struct cmd_node rip_node =
  {
    RIP_NODE,
    "%s(config-router)# ",
    1
  };

/* Distribute-list update functions. */
void
rip_distribute_update (struct distribute *dist)
{
  struct interface *ifp;
  struct rip_interface *ri;
  struct access_list *alist;
  struct prefix_list *plist;

  if (! dist->ifname)
    return;

  ifp = if_lookup_by_name (dist->ifname);
  if (ifp == NULL)
    return;

  ri = ifp->info;

  if (dist->list[DISTRIBUTE_IN])
    {
      alist = access_list_lookup (AFI_IP, dist->list[DISTRIBUTE_IN]);
      if (alist)
	ri->list[RIP_FILTER_IN] = alist;
      else
	ri->list[RIP_FILTER_IN] = NULL;
    }
  else
    ri->list[RIP_FILTER_IN] = NULL;

  if (dist->list[DISTRIBUTE_OUT])
    {
      alist = access_list_lookup (AFI_IP, dist->list[DISTRIBUTE_OUT]);
      if (alist)
	ri->list[RIP_FILTER_OUT] = alist;
      else
	ri->list[RIP_FILTER_OUT] = NULL;
    }
  else
    ri->list[RIP_FILTER_OUT] = NULL;

  if (dist->prefix[DISTRIBUTE_IN])
    {
      plist = prefix_list_lookup (AFI_IP, dist->prefix[DISTRIBUTE_IN]);
      if (plist)
	ri->prefix[RIP_FILTER_IN] = plist;
      else
	ri->prefix[RIP_FILTER_IN] = NULL;
    }
  else
    ri->prefix[RIP_FILTER_IN] = NULL;

  if (dist->prefix[DISTRIBUTE_OUT])
    {
      plist = prefix_list_lookup (AFI_IP, dist->prefix[DISTRIBUTE_OUT]);
      if (plist)
	ri->prefix[RIP_FILTER_OUT] = plist;
      else
	ri->prefix[RIP_FILTER_OUT] = NULL;
    }
  else
    ri->prefix[RIP_FILTER_OUT] = NULL;
}

void
rip_distribute_update_interface (struct interface *ifp)
{
  struct distribute *dist;

  dist = distribute_lookup (ifp->name);
  if (dist)
    rip_distribute_update (dist);
}

/* Update all interface's distribute list. */
void
rip_distribute_update_all ()
{
  struct interface *ifp;
  listnode node;

  for (node = listhead (iflist); node; nextnode (node))
    {
      ifp = getdata (node);
      rip_distribute_update_interface (ifp);
    }
}

/* Delete all added rip route. */
void
rip_clean ()
{
  int i;
  struct route_node *rp;
  struct rip_info *rinfo;

  if (rip)
    {
      /* Clear RIP routes */
      for (rp = route_top (rip->table); rp; rp = route_next (rp))
	if ((rinfo = rp->info) != NULL)
	  {
	    if (rinfo->type == ZEBRA_ROUTE_RIP &&
		rinfo->sub_type == RIP_ROUTE_RTE)
	      rip_zebra_ipv4_delete ((struct prefix_ipv4 *)&rp->p,
				     &rinfo->nexthop, rinfo->metric);
	
	    RIP_TIMER_OFF (rinfo->t_timeout);
	    RIP_TIMER_OFF (rinfo->t_garbage_collect);

	    rp->info = NULL;
	    route_unlock_node (rp);

	    rip_info_free (rinfo);
	  }

      /* Cancel RIP related timers. */
      RIP_TIMER_OFF (rip->t_update);
      RIP_TIMER_OFF (rip->t_triggered_update);
      RIP_TIMER_OFF (rip->t_triggered_interval);

      /* Cancel read thread. */
      if (rip->t_read)
	{
	  thread_cancel (rip->t_read);
	  rip->t_read = NULL;
	}

      /* Close RIP socket. */
      if (rip->sock >= 0)
	{
	  close (rip->sock);
	  rip->sock = -1;
	}

      /* Static RIP route configuration. */
      for (rp = route_top (rip->route); rp; rp = route_next (rp))
	if (rp->info)
	  {
	    rp->info = NULL;
	    route_unlock_node (rp);
	  }

      /* RIP neighbor configuration. */
      for (rp = route_top (rip->neighbor); rp; rp = route_next (rp))
	if (rp->info)
	  {
	    rp->info = NULL;
	    route_unlock_node (rp);
	  }

      /* Redistribute related clear. */
      if (rip->default_information_route_map)
	free (rip->default_information_route_map);

      for (i = 0; i < ZEBRA_ROUTE_MAX; i++)
	if (rip->route_map[i].name)
	  free (rip->route_map[i].name);

      XFREE (MTYPE_ROUTE_TABLE, rip->table);
      XFREE (MTYPE_ROUTE_TABLE, rip->route);
      XFREE (MTYPE_ROUTE_TABLE, rip->neighbor);
      
      XFREE (MTYPE_RIP, rip);
      rip = NULL;
    }

  rip_clean_network ();
  rip_passive_interface_clean ();
  rip_offset_clean ();
  rip_interface_clean ();
  rip_distance_reset ();
  rip_redistribute_clean ();
}

/* Reset all values to the default settings. */
void
rip_reset ()
{
  /* Reset global counters. */
  rip_global_route_changes = 0;
  rip_global_queries = 0;

  /* Call ripd related reset functions. */
  rip_debug_reset ();
  rip_route_map_reset ();

  /* Call library reset functions. */
  vty_reset ();
  access_list_reset ();
  prefix_list_reset ();

  distribute_list_reset ();

  rip_interface_reset ();
  rip_distance_reset ();

  rip_zclient_reset ();
}

/* Allocate new rip structure and set default value. */
void
rip_init ()
{
  /* Randomize for triggered update random(). */
  srand (time (NULL));

  /* Install top nodes. */
  install_node (&rip_node, config_write_rip);

  /* Install rip commands. */
  install_element (VIEW_NODE, &show_ip_rip_cmd);
  install_element (VIEW_NODE, &show_ip_protocols_rip_cmd);
  install_element (ENABLE_NODE, &show_ip_rip_cmd);
  install_element (ENABLE_NODE, &show_ip_protocols_rip_cmd);
  install_element (CONFIG_NODE, &router_rip_cmd);
  install_element (CONFIG_NODE, &no_router_rip_cmd);

  install_default (RIP_NODE);
  install_element (RIP_NODE, &rip_version_cmd);
  install_element (RIP_NODE, &no_rip_version_cmd);
  install_element (RIP_NODE, &no_rip_version_val_cmd);
  install_element (RIP_NODE, &rip_default_metric_cmd);
  install_element (RIP_NODE, &no_rip_default_metric_cmd);
  install_element (RIP_NODE, &no_rip_default_metric_val_cmd);
  install_element (RIP_NODE, &rip_timers_cmd);
  install_element (RIP_NODE, &no_rip_timers_cmd);
  install_element (RIP_NODE, &rip_route_cmd);
  install_element (RIP_NODE, &no_rip_route_cmd);
  install_element (RIP_NODE, &rip_distance_cmd);
  install_element (RIP_NODE, &no_rip_distance_cmd);
  install_element (RIP_NODE, &rip_distance_source_cmd);
  install_element (RIP_NODE, &no_rip_distance_source_cmd);
  install_element (RIP_NODE, &rip_distance_source_access_list_cmd);
  install_element (RIP_NODE, &no_rip_distance_source_access_list_cmd);

  /* Debug related init. */
  rip_debug_init ();

  /* Filter related init. */
  rip_route_map_init ();
  rip_offset_init ();

  /* SNMP init. */
#ifdef HAVE_SNMP
  rip_snmp_init ();
#endif /* HAVE_SNMP */

  /* Access list install. */
  access_list_init ();
  access_list_add_hook (rip_distribute_update_all);
  access_list_delete_hook (rip_distribute_update_all);

  /* Prefix list initialize.*/
  prefix_list_init ();
  prefix_list_add_hook (rip_distribute_update_all);
  prefix_list_delete_hook (rip_distribute_update_all);

  /* Distribute list install. */
  distribute_list_init (RIP_NODE);
  distribute_list_add_hook (rip_distribute_update);
  distribute_list_delete_hook (rip_distribute_update);

  /* Distance control. */
  rip_distance_table = route_table_init ();
}
