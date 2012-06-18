/* Interface related function for RIP.
 * Copyright (C) 1997, 98 Kunihiro Ishiguro <kunihiro@zebra.org>
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

#include "command.h"
#include "if.h"
#include "sockunion.h"
#include "prefix.h"
#include "memory.h"
#include "network.h"
#include "table.h"
#include "log.h"
#include "stream.h"
#include "thread.h"
#include "zclient.h"
#include "filter.h"
#include "sockopt.h"

#include "zebra/connected.h"

#include "ripd/ripd.h"
#include "ripd/rip_debug.h"

void rip_enable_apply (struct interface *);
void rip_passive_interface_apply (struct interface *);
int rip_if_down(struct interface *ifp);

struct message ri_version_msg[] = 
{
  {RI_RIP_VERSION_1,       "1"},
  {RI_RIP_VERSION_2,       "2"},
  {RI_RIP_VERSION_1_AND_2, "1 2"},
  {0,                      NULL}
};

/* RIP enabled network vector. */
vector rip_enable_interface;

/* RIP enabled interface table. */
struct route_table *rip_enable_network;

/* Vector to store passive-interface name. */
vector Vrip_passive_interface;

/* Join to the RIP version 2 multicast group. */
int
ipv4_multicast_join (int sock, 
		     struct in_addr group, 
		     struct in_addr ifa,
		     unsigned int ifindex)
{
  int ret;

  ret = setsockopt_multicast_ipv4 (sock, 
				   IP_ADD_MEMBERSHIP, 
				   ifa, 
				   group.s_addr, 
				   ifindex); 

  if (ret < 0) 
    zlog (NULL, LOG_INFO, "can't setsockopt IP_ADD_MEMBERSHIP %s",
	  strerror (errno));

  return ret;
}

/* Leave from the RIP version 2 multicast group. */
int
ipv4_multicast_leave (int sock, 
		      struct in_addr group, 
		      struct in_addr ifa,
		      unsigned int ifindex)
{
  int ret;

  ret = setsockopt_multicast_ipv4 (sock, 
				   IP_DROP_MEMBERSHIP, 
				   ifa, 
				   group.s_addr, 
				   ifindex);

  if (ret < 0) 
    zlog (NULL, LOG_INFO, "can't setsockopt IP_DROP_MEMBERSHIP");

  return ret;
}

/* Allocate new RIP's interface configuration. */
struct rip_interface *
rip_interface_new ()
{
  struct rip_interface *ri;

  ri = XMALLOC (MTYPE_RIP_INTERFACE, sizeof (struct rip_interface));
  memset (ri, 0, sizeof (struct rip_interface));

  /* Default authentication type is simple password for Cisco
     compatibility. */
  /* ri->auth_type = RIP_NO_AUTH; */
  ri->auth_type = RIP_AUTH_SIMPLE_PASSWORD;

  /* Set default split-horizon behavior.  If the interface is Frame
     Relay or SMDS is enabled, the default value for split-horizon is
     off.  But currently Zebra does detect Frame Relay or SMDS
     interface.  So all interface is set to split horizon.  */
  ri->split_horizon_default = 1;
  ri->split_horizon = ri->split_horizon_default;

  return ri;
}

void
rip_interface_multicast_set (int sock, struct interface *ifp)
{
  int ret;
  listnode node;
  struct servent *sp;
  struct sockaddr_in from;

  for (node = listhead (ifp->connected); node; nextnode (node))
    {
      struct prefix_ipv4 *p;
      struct connected *connected;
      struct in_addr addr;

      connected = getdata (node);
      p = (struct prefix_ipv4 *) connected->address;

      if (p->family == AF_INET)
	{
	  addr = p->prefix;

	  if (setsockopt_multicast_ipv4 (sock, IP_MULTICAST_IF,
					 addr, 0, ifp->ifindex) < 0) 
	    {
	      zlog_warn ("Can't setsockopt IP_MULTICAST_IF to fd %d", sock);
	      return;
	    }

	  /* Bind myself. */
	  memset (&from, 0, sizeof (struct sockaddr_in));

	  /* Set RIP port. */
	  sp = getservbyname ("router", "udp");
	  if (sp) 
	    from.sin_port = sp->s_port;
	  else 
	    from.sin_port = htons (RIP_PORT_DEFAULT);

	  /* Address shoud be any address. */
	  from.sin_family = AF_INET;
	  from.sin_addr = addr;
#ifdef HAVE_SIN_LEN
	  from.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_SIN_LEN */

	  ret = bind (sock, (struct sockaddr *) & from, 
		      sizeof (struct sockaddr_in));
	  if (ret < 0)
	    {
	      zlog_warn ("Can't bind socket: %s", strerror (errno));
	      return;
	    }

	  return;

	}
    }
}

/* Send RIP request packet to specified interface. */
void
rip_request_interface_send (struct interface *ifp, u_char version)
{
  struct sockaddr_in to;

  /* RIPv2 support multicast. */
  if (version == RIPv2 && if_is_multicast (ifp))
    {
      
      if (IS_RIP_DEBUG_EVENT)
	zlog_info ("multicast request on %s", ifp->name);

      rip_request_send (NULL, ifp, version);
      return;
    }

  /* RIPv1 and non multicast interface. */
  if (if_is_pointopoint (ifp) || if_is_broadcast (ifp))
    {
      listnode cnode;

      if (IS_RIP_DEBUG_EVENT)
	zlog_info ("broadcast request to %s", ifp->name);

      for (cnode = listhead (ifp->connected); cnode; nextnode (cnode))
	{
	  struct prefix_ipv4 *p;
	  struct connected *connected;

	  connected = getdata (cnode);
	  p = (struct prefix_ipv4 *) connected->destination;

	  if (p->family == AF_INET)
	    {
	      memset (&to, 0, sizeof (struct sockaddr_in));
	      to.sin_port = htons (RIP_PORT_DEFAULT);
	      to.sin_addr = p->prefix;

	      rip_request_send (&to, ifp, version);
	    }
	}
    }
}

/* This will be executed when interface goes up. */
void
rip_request_interface (struct interface *ifp)
{
  struct rip_interface *ri;

  /* In default ripd doesn't send RIP_REQUEST to the loopback interface. */
  if (if_is_loopback (ifp))
    return;

  /* If interface is down, don't send RIP packet. */
  if (! if_is_up (ifp))
    return;

  /* Fetch RIP interface information. */
  ri = ifp->info;


  /* If there is no version configuration in the interface,
     use rip's version setting. */
  if (ri->ri_send == RI_RIP_UNSPEC)
    {
      if (rip->version == RIPv1)
	rip_request_interface_send (ifp, RIPv1);
      else
	rip_request_interface_send (ifp, RIPv2);
    }
  /* If interface has RIP version configuration use it. */
  else
    {
      if (ri->ri_send & RIPv1)
	rip_request_interface_send (ifp, RIPv1);
      if (ri->ri_send & RIPv2)
	rip_request_interface_send (ifp, RIPv2);
    }
}

/* Send RIP request to the neighbor. */
void
rip_request_neighbor (struct in_addr addr)
{
  struct sockaddr_in to;

  memset (&to, 0, sizeof (struct sockaddr_in));
  to.sin_port = htons (RIP_PORT_DEFAULT);
  to.sin_addr = addr;

  rip_request_send (&to, NULL, rip->version);
}

/* Request routes at all interfaces. */
void
rip_request_neighbor_all ()
{
  struct route_node *rp;

  if (! rip)
    return;

  if (IS_RIP_DEBUG_EVENT)
    zlog_info ("request to the all neighbor");

  /* Send request to all neighbor. */
  for (rp = route_top (rip->neighbor); rp; rp = route_next (rp))
    if (rp->info)
      rip_request_neighbor (rp->p.u.prefix4);
}

/* Multicast packet receive socket. */
int
rip_multicast_join (struct interface *ifp, int sock)
{
  struct rip_interface *ri;
  listnode cnode;

  ri = ifp->info;

  if (if_is_up (ifp) && if_is_multicast (ifp))
    {
      if (IS_RIP_DEBUG_EVENT)
	zlog_info ("multicast join at %s", ifp->name);

      for (cnode = listhead (ifp->connected); cnode; nextnode (cnode))
	{
	  struct prefix_ipv4 *p;
	  struct connected *connected;
	  struct in_addr group;
	      
	  connected = getdata (cnode);
	  p = (struct prefix_ipv4 *) connected->address;
      
	  if (p->family != AF_INET)
	    continue;
      
	  group.s_addr = htonl (INADDR_RIP_GROUP);
	  if (ipv4_multicast_join (sock, group, p->prefix, ifp->ifindex) < 0)
	    return -1;
	  else
	    {
	      ri->joined_multicast = 1;
	      return 0;
	    }
	}
    }
  return 0;
}

/* Leave from multicast group. */
void
rip_multicast_leave (struct interface *ifp, int sock)
{
  struct rip_interface *ri;
  listnode cnode;

  ri = ifp->info;

  if (ri->joined_multicast)
    {
      ri->joined_multicast = 0;
      if (IS_RIP_DEBUG_EVENT)
	zlog_info ("multicast leave from %s", ifp->name);

      for (cnode = listhead (ifp->connected); cnode; nextnode (cnode))
	{
	  struct prefix_ipv4 *p;
	  struct connected *connected;
	  struct in_addr group;
	      
	  connected = getdata (cnode);
	  p = (struct prefix_ipv4 *) connected->address;
      
	  if (p->family != AF_INET)
	    continue;
      
	  group.s_addr = htonl (INADDR_RIP_GROUP);
          if (ipv4_multicast_leave (sock, group, p->prefix, ifp->ifindex) == 0)
	    return;
        }
    }
}

/* Is there and address on interface that I could use ? */
int
rip_if_ipv4_address_check (struct interface *ifp)
{
  struct listnode *nn;
  struct connected *connected;
  int count = 0;

  for (nn = listhead (ifp->connected); nn; nextnode (nn))
    if ((connected = getdata (nn)) != NULL)
      {
	struct prefix *p;

	p = connected->address;

	if (p->family == AF_INET)
          {
	    count++;
	  }
      }
						
  return count;
}
						
						
						

/* Does this address belongs to me ? */
int
if_check_address (struct in_addr addr)
{
  listnode node;

  for (node = listhead (iflist); node; nextnode (node))
    {
      listnode cnode;
      struct interface *ifp;

      ifp = getdata (node);

      for (cnode = listhead (ifp->connected); cnode; nextnode (cnode))
	{
	  struct connected *connected;
	  struct prefix_ipv4 *p;

	  connected = getdata (cnode);
	  p = (struct prefix_ipv4 *) connected->address;

	  if (p->family != AF_INET)
	    continue;

	  if (IPV4_ADDR_CMP (&p->prefix, &addr) == 0)
	    return 1;
	}
    }
  return 0;
}

/* is this address from a valid neighbor? (RFC2453 - Sec. 3.9.2) */
int
if_valid_neighbor (struct in_addr addr)
{
  listnode node;
  struct connected *connected = NULL;
  struct prefix_ipv4 *p;

  for (node = listhead (iflist); node; nextnode (node))
    {
      listnode cnode;
      struct interface *ifp;

      ifp = getdata (node);

      for (cnode = listhead (ifp->connected); cnode; nextnode (cnode))
	{
	  struct prefix *pxn = NULL; /* Prefix of the neighbor */
	  struct prefix *pxc = NULL; /* Prefix of the connected network */

	  connected = getdata (cnode);

	  if (if_is_pointopoint (ifp))
	    {
	      p = (struct prefix_ipv4 *) connected->address;

	      if (p && p->family == AF_INET)
		{
		  if (IPV4_ADDR_SAME (&p->prefix, &addr))
		    return 1;

		  p = (struct prefix_ipv4 *) connected->destination;
		  if (p && IPV4_ADDR_SAME (&p->prefix, &addr))
		    return 1;
		}
	    }
	  else
	    {
	      p = (struct prefix_ipv4 *) connected->address;

	      if (p->family != AF_INET)
		continue;

	      pxn = prefix_new();
	      pxn->family = AF_INET;
	      pxn->prefixlen = 32;
	      pxn->u.prefix4 = addr;
	      
	      pxc = prefix_new();
	      prefix_copy(pxc, (struct prefix *) p);
	      apply_mask(pxc);
	  
	      if (prefix_match (pxc, pxn)) 
		{
		  prefix_free (pxn);
		  prefix_free (pxc);
		  return 1;
		}
	      prefix_free(pxc);
	      prefix_free(pxn);
	    }
	}
    }
  return 0;
}

/* Inteface link down message processing. */
int
rip_interface_down (int command, struct zclient *zclient, zebra_size_t length)
{
  struct interface *ifp;
  struct stream *s;

  s = zclient->ibuf;  

  /* zebra_interface_state_read() updates interface structure in
     iflist. */
  ifp = zebra_interface_state_read(s);

  if (ifp == NULL)
    return 0;

  rip_if_down(ifp);
 
  if (IS_RIP_DEBUG_ZEBRA)
    zlog_info ("interface %s index %d flags %ld metric %d mtu %d is down",
	       ifp->name, ifp->ifindex, ifp->flags, ifp->metric, ifp->mtu);

  return 0;
}

/* Inteface link up message processing */
int
rip_interface_up (int command, struct zclient *zclient, zebra_size_t length)
{
  struct interface *ifp;

  /* zebra_interface_state_read () updates interface structure in
     iflist. */
  ifp = zebra_interface_state_read (zclient->ibuf);

  if (ifp == NULL)
    return 0;

  if (IS_RIP_DEBUG_ZEBRA)
    zlog_info ("interface %s index %d flags %ld metric %d mtu %d is up",
	       ifp->name, ifp->ifindex, ifp->flags, ifp->metric, ifp->mtu);

  /* Check if this interface is RIP enabled or not.*/
  rip_enable_apply (ifp);
 
  /* Check for a passive interface */
  rip_passive_interface_apply (ifp);

  /* Apply distribute list to the all interface. */
  rip_distribute_update_interface (ifp);

  return 0;
}

/* Inteface addition message from zebra. */
int
rip_interface_add (int command, struct zclient *zclient, zebra_size_t length)
{
  struct interface *ifp;

  ifp = zebra_interface_add_read (zclient->ibuf);

  if (IS_RIP_DEBUG_ZEBRA)
    zlog_info ("interface add %s index %d flags %ld metric %d mtu %d",
	       ifp->name, ifp->ifindex, ifp->flags, ifp->metric, ifp->mtu);

  /* Check if this interface is RIP enabled or not.*/
  rip_enable_apply (ifp);

  /* Apply distribute list to the all interface. */
  rip_distribute_update_interface (ifp);

  /* rip_request_neighbor_all (); */

  return 0;
}

int
rip_interface_delete (int command, struct zclient *zclient,
		      zebra_size_t length)
{
  struct interface *ifp;
  struct stream *s;


  s = zclient->ibuf;  
  /* zebra_interface_state_read() updates interface structure in iflist */
  ifp = zebra_interface_state_read(s);

  if (ifp == NULL)
    return 0;

  if (if_is_up (ifp)) {
    rip_if_down(ifp);
  } 
  
  zlog_info("interface delete %s index %d flags %ld metric %d mtu %d",
	    ifp->name, ifp->ifindex, ifp->flags, ifp->metric, ifp->mtu);  
  
  /* To support pseudo interface do not free interface structure.  */
  /* if_delete(ifp); */

  return 0;
}

void
rip_interface_clean ()
{
  listnode node;
  struct interface *ifp;
  struct rip_interface *ri;

  for (node = listhead (iflist); node; nextnode (node))
    {
      ifp = getdata (node);
      ri = ifp->info;

      ri->enable_network = 0;
      ri->enable_interface = 0;
      ri->running = 0;

      if (ri->t_wakeup)
	{
	  thread_cancel (ri->t_wakeup);
	  ri->t_wakeup = NULL;
	}
    }
}

void
rip_interface_reset ()
{
  listnode node;
  struct interface *ifp;
  struct rip_interface *ri;

  for (node = listhead (iflist); node; nextnode (node))
    {
      ifp = getdata (node);
      ri = ifp->info;

      ri->enable_network = 0;
      ri->enable_interface = 0;
      ri->running = 0;

      ri->ri_send = RI_RIP_UNSPEC;
      ri->ri_receive = RI_RIP_UNSPEC;

      /* ri->auth_type = RIP_NO_AUTH; */
      ri->auth_type = RIP_AUTH_SIMPLE_PASSWORD;

      if (ri->auth_str)
	{
	  free (ri->auth_str);
	  ri->auth_str = NULL;
	}
      if (ri->key_chain)
	{
	  free (ri->key_chain);
	  ri->key_chain = NULL;
	}

      ri->split_horizon = 0;
      ri->split_horizon_default = 0;

      ri->list[RIP_FILTER_IN] = NULL;
      ri->list[RIP_FILTER_OUT] = NULL;

      ri->prefix[RIP_FILTER_IN] = NULL;
      ri->prefix[RIP_FILTER_OUT] = NULL;
      
      if (ri->t_wakeup)
	{
	  thread_cancel (ri->t_wakeup);
	  ri->t_wakeup = NULL;
	}

      ri->recv_badpackets = 0;
      ri->recv_badroutes = 0;
      ri->sent_updates = 0;

      ri->passive = 0;
    }
}

int
rip_if_down(struct interface *ifp)
{
  struct route_node *rp;
  struct rip_info *rinfo;
  struct rip_interface *ri = NULL;
  if (rip)
    {
      for (rp = route_top (rip->table); rp; rp = route_next (rp))
	if ((rinfo = rp->info) != NULL)
	  {
	    /* Routes got through this interface. */
	    if (rinfo->ifindex == ifp->ifindex &&
		rinfo->type == ZEBRA_ROUTE_RIP &&
		rinfo->sub_type == RIP_ROUTE_RTE)
	      {
		rip_zebra_ipv4_delete ((struct prefix_ipv4 *) &rp->p,
				       &rinfo->nexthop,
				       rinfo->ifindex);

		rip_redistribute_delete (rinfo->type,rinfo->sub_type,
					 (struct prefix_ipv4 *)&rp->p,
					 rinfo->ifindex);
	      }
	    else
	      {
		/* All redistributed routes but static and system */
		if ((rinfo->ifindex == ifp->ifindex) &&
		    (rinfo->type != ZEBRA_ROUTE_STATIC) &&
		    (rinfo->type != ZEBRA_ROUTE_SYSTEM))
		  rip_redistribute_delete (rinfo->type,rinfo->sub_type,
					   (struct prefix_ipv4 *)&rp->p,
					   rinfo->ifindex);
	      }
	  }
    }
	    
  ri = ifp->info;
  
  if (ri->running)
   {
     if (IS_RIP_DEBUG_EVENT)
       zlog_info ("turn off %s", ifp->name);

     /* Leave from multicast group. */
     rip_multicast_leave (ifp, rip->sock);

     ri->running = 0;
   }

  return 0;
}

/* Needed for stop RIP process. */
void
rip_if_down_all ()
{
  struct interface *ifp;
  listnode node;

  for (node = listhead (iflist); node; nextnode (node))
    {
      ifp = getdata (node);
      rip_if_down (ifp);
    }
}

int
rip_interface_address_add (int command, struct zclient *zclient,
			   zebra_size_t length)
{
  struct connected *ifc;
  struct prefix *p;

  ifc = zebra_interface_address_add_read (zclient->ibuf);

  if (ifc == NULL)
    return 0;

  p = ifc->address;

  if (p->family == AF_INET)
    {
      if (IS_RIP_DEBUG_ZEBRA)
	zlog_info ("connected address %s/%d is added", 
		   inet_ntoa (p->u.prefix4), p->prefixlen);
      
      /* Check is this interface is RIP enabled or not.*/
      rip_enable_apply (ifc->ifp);

#ifdef HAVE_SNMP
      rip_ifaddr_add (ifc->ifp, ifc);
#endif /* HAVE_SNMP */
    }

  return 0;
}

int
rip_interface_address_delete (int command, struct zclient *zclient,
			      zebra_size_t length)
{
  struct connected *ifc;
  struct prefix *p;

  ifc = zebra_interface_address_delete_read (zclient->ibuf);
  
  if (ifc)
    {
      p = ifc->address;
      if (p->family == AF_INET)
	{
	  if (IS_RIP_DEBUG_ZEBRA)

	    zlog_info ("connected address %s/%d is deleted",
		       inet_ntoa (p->u.prefix4), p->prefixlen);

#ifdef HAVE_SNMP
	  rip_ifaddr_delete (ifc->ifp, ifc);
#endif /* HAVE_SNMP */

	  /* Check if this interface is RIP enabled or not.*/
	  rip_enable_apply (ifc->ifp);
	}

      connected_free (ifc);

    }

  return 0;
}

/* Check interface is enabled by network statement. */
int
rip_enable_network_lookup (struct interface *ifp)
{
  struct listnode *nn;
  struct connected *connected;
  struct prefix_ipv4 address;

  for (nn = listhead (ifp->connected); nn; nextnode (nn))
    if ((connected = getdata (nn)) != NULL)
      {
	struct prefix *p; 
	struct route_node *node;

	p = connected->address;

	if (p->family == AF_INET)
	  {
	    address.family = AF_INET;
	    address.prefix = p->u.prefix4;
	    address.prefixlen = IPV4_MAX_BITLEN;
	    
	    node = route_node_match (rip_enable_network,
				     (struct prefix *)&address);
	    if (node)
	      {
		route_unlock_node (node);
		return 1;
	      }
	  }
      }
  return -1;
}

/* Add RIP enable network. */
int
rip_enable_network_add (struct prefix *p)
{
  struct route_node *node;

  node = route_node_get (rip_enable_network, p);

  if (node->info)
    {
      route_unlock_node (node);
      return -1;
    }
  else
    node->info = "enabled";

  return 1;
}

/* Delete RIP enable network. */
int
rip_enable_network_delete (struct prefix *p)
{
  struct route_node *node;

  node = route_node_lookup (rip_enable_network, p);
  if (node)
    {
      node->info = NULL;

      /* Unlock info lock. */
      route_unlock_node (node);

      /* Unlock lookup lock. */
      route_unlock_node (node);

      return 1;
    }
  return -1;
}

/* Check interface is enabled by ifname statement. */
int
rip_enable_if_lookup (char *ifname)
{
  int i;
  char *str;

  for (i = 0; i < vector_max (rip_enable_interface); i++)
    if ((str = vector_slot (rip_enable_interface, i)) != NULL)
      if (strcmp (str, ifname) == 0)
	return i;
  return -1;
}

/* Add interface to rip_enable_if. */
int
rip_enable_if_add (char *ifname)
{
  int ret;

  ret = rip_enable_if_lookup (ifname);
  if (ret >= 0)
    return -1;

  vector_set (rip_enable_interface, strdup (ifname));

  return 1;
}

/* Delete interface from rip_enable_if. */
int
rip_enable_if_delete (char *ifname)
{
  int index;
  char *str;

  index = rip_enable_if_lookup (ifname);
  if (index < 0)
    return -1;

  str = vector_slot (rip_enable_interface, index);
  free (str);
  vector_unset (rip_enable_interface, index);

  return 1;
}

/* Join to multicast group and send request to the interface. */
int
rip_interface_wakeup (struct thread *t)
{
  struct interface *ifp;
  struct rip_interface *ri;

  /* Get interface. */
  ifp = THREAD_ARG (t);

  ri = ifp->info;
  ri->t_wakeup = NULL;

  /* Join to multicast group. */
  if (rip_multicast_join (ifp, rip->sock) < 0)
    {
      zlog_err ("multicast join failed, interface %s not running", ifp->name);
      return 0;
    }

  /* Set running flag. */
  ri->running = 1;

  /* Send RIP request to the interface. */
  rip_request_interface (ifp);

  return 0;
}

int rip_redistribute_check (int);

void
rip_connect_set (struct interface *ifp, int set)
{
  struct listnode *nn;
  struct connected *connected;
  struct prefix_ipv4 address;

  for (nn = listhead (ifp->connected); nn; nextnode (nn))
    if ((connected = getdata (nn)) != NULL)
      {
	struct prefix *p; 
	p = connected->address;

	if (p->family != AF_INET)
	  continue;

	address.family = AF_INET;
	address.prefix = p->u.prefix4;
	address.prefixlen = p->prefixlen;
	apply_mask_ipv4 (&address);

	if (set)
	  rip_redistribute_add (ZEBRA_ROUTE_CONNECT, RIP_ROUTE_INTERFACE,
				&address, connected->ifp->ifindex, NULL);
	else
	  {
	    rip_redistribute_delete (ZEBRA_ROUTE_CONNECT, RIP_ROUTE_INTERFACE,
				     &address, connected->ifp->ifindex);
	    if (rip_redistribute_check (ZEBRA_ROUTE_CONNECT))
	      rip_redistribute_add (ZEBRA_ROUTE_CONNECT, RIP_ROUTE_REDISTRIBUTE,
				    &address, connected->ifp->ifindex, NULL);
	  }
      }
}

/* Update interface status. */
void
rip_enable_apply (struct interface *ifp)
{
  int ret;
  struct rip_interface *ri = NULL;

  /* Check interface. */
  if (if_is_loopback (ifp))
    return;

  if (! if_is_up (ifp))
    return;

  ri = ifp->info;

  /* Check network configuration. */
  ret = rip_enable_network_lookup (ifp);

  /* If the interface is matched. */
  if (ret > 0)
    ri->enable_network = 1;
  else
    ri->enable_network = 0;

  /* Check interface name configuration. */
  ret = rip_enable_if_lookup (ifp->name);
  if (ret >= 0)
    ri->enable_interface = 1;
  else
    ri->enable_interface = 0;

  /* any interface MUST have an IPv4 address */
  if ( ! rip_if_ipv4_address_check (ifp) )
    {
      ri->enable_network = 0;
      ri->enable_interface = 0;
    }

  /* Update running status of the interface. */
  if (ri->enable_network || ri->enable_interface)
    {
      if (! ri->running)
	{
	  if (IS_RIP_DEBUG_EVENT)
	    zlog_info ("turn on %s", ifp->name);

	  /* Add interface wake up thread. */
	  if (! ri->t_wakeup)
	    ri->t_wakeup = thread_add_timer (master, rip_interface_wakeup,
					     ifp, 1);
          rip_connect_set (ifp, 1);
	}
    }
  else
    {
      if (ri->running)
	{
	  if (IS_RIP_DEBUG_EVENT)
	    zlog_info ("turn off %s", ifp->name);

	  /* Might as well clean up the route table as well */ 
	  rip_if_down(ifp);

	  ri->running = 0;
          rip_connect_set (ifp, 0);
	}
    }
}

/* Apply network configuration to all interface. */
void
rip_enable_apply_all ()
{
  struct interface *ifp;
  listnode node;

  /* Check each interface. */
  for (node = listhead (iflist); node; nextnode (node))
    {
      ifp = getdata (node);
      rip_enable_apply (ifp);
    }
}

int
rip_neighbor_lookup (struct sockaddr_in *from)
{
  struct prefix_ipv4 p;
  struct route_node *node;

  memset (&p, 0, sizeof (struct prefix_ipv4));
  p.family = AF_INET;
  p.prefix = from->sin_addr;
  p.prefixlen = IPV4_MAX_BITLEN;

  node = route_node_lookup (rip->neighbor, (struct prefix *) &p);
  if (node)
    {
      route_unlock_node (node);
      return 1;
    }
  return 0;
}

/* Add new RIP neighbor to the neighbor tree. */
int
rip_neighbor_add (struct prefix_ipv4 *p)
{
  struct route_node *node;

  node = route_node_get (rip->neighbor, (struct prefix *) p);

  if (node->info)
    return -1;

  node->info = rip->neighbor;

  return 0;
}

/* Delete RIP neighbor from the neighbor tree. */
int
rip_neighbor_delete (struct prefix_ipv4 *p)
{
  struct route_node *node;

  /* Lock for look up. */
  node = route_node_lookup (rip->neighbor, (struct prefix *) p);
  if (! node)
    return -1;
  
  node->info = NULL;

  /* Unlock lookup lock. */
  route_unlock_node (node);

  /* Unlock real neighbor information lock. */
  route_unlock_node (node);

  return 0;
}

/* Clear all network and neighbor configuration. */
void
rip_clean_network ()
{
  int i;
  char *str;
  struct route_node *rn;

  /* rip_enable_network. */
  for (rn = route_top (rip_enable_network); rn; rn = route_next (rn))
    if (rn->info)
      {
	rn->info = NULL;
	route_unlock_node (rn);
      }

  /* rip_enable_interface. */
  for (i = 0; i < vector_max (rip_enable_interface); i++)
    if ((str = vector_slot (rip_enable_interface, i)) != NULL)
      {
	free (str);
	vector_slot (rip_enable_interface, i) = NULL;
      }
}

/* Utility function for looking up passive interface settings. */
int
rip_passive_interface_lookup (char *ifname)
{
  int i;
  char *str;

  for (i = 0; i < vector_max (Vrip_passive_interface); i++)
    if ((str = vector_slot (Vrip_passive_interface, i)) != NULL)
      if (strcmp (str, ifname) == 0)
	return i;
  return -1;
}

void
rip_passive_interface_apply (struct interface *ifp)
{
  int ret;
  struct rip_interface *ri;

  ri = ifp->info;

  ret = rip_passive_interface_lookup (ifp->name);
  if (ret < 0)
    ri->passive = 0;
  else
    ri->passive = 1;
}

void
rip_passive_interface_apply_all ()
{
  struct interface *ifp;
  listnode node;

  for (node = listhead (iflist); node; nextnode (node))
    {
      ifp = getdata (node);
      rip_passive_interface_apply (ifp);
    }
}

/* Passive interface. */
int
rip_passive_interface_set (struct vty *vty, char *ifname)
{
  if (rip_passive_interface_lookup (ifname) >= 0)
    return CMD_WARNING;

  vector_set (Vrip_passive_interface, strdup (ifname));

  rip_passive_interface_apply_all ();

  return CMD_SUCCESS;
}

int
rip_passive_interface_unset (struct vty *vty, char *ifname)
{
  int i;
  char *str;

  i = rip_passive_interface_lookup (ifname);
  if (i < 0)
    return CMD_WARNING;

  str = vector_slot (Vrip_passive_interface, i);
  free (str);
  vector_unset (Vrip_passive_interface, i);

  rip_passive_interface_apply_all ();

  return CMD_SUCCESS;
}

/* Free all configured RIP passive-interface settings. */
void
rip_passive_interface_clean ()
{
  int i;
  char *str;

  for (i = 0; i < vector_max (Vrip_passive_interface); i++)
    if ((str = vector_slot (Vrip_passive_interface, i)) != NULL)
      {
	free (str);
	vector_slot (Vrip_passive_interface, i) = NULL;
      }
  rip_passive_interface_apply_all ();
}

/* RIP enable network or interface configuration. */
DEFUN (rip_network,
       rip_network_cmd,
       "network (A.B.C.D/M|WORD)",
       "Enable routing on an IP network\n"
       "IP prefix <network>/<length>, e.g., 35.0.0.0/8\n"
       "Interface name\n")
{
  int ret;
  struct prefix_ipv4 p;

  ret = str2prefix_ipv4 (argv[0], &p);

  if (ret)
    ret = rip_enable_network_add ((struct prefix *) &p);
  else
    ret = rip_enable_if_add (argv[0]);

  if (ret < 0)
    {
      vty_out (vty, "There is a same network configuration %s%s", argv[0],
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  rip_enable_apply_all ();

  return CMD_SUCCESS;
}

/* RIP enable network or interface configuration. */
DEFUN (no_rip_network,
       no_rip_network_cmd,
       "no network (A.B.C.D/M|WORD)",
       NO_STR
       "Enable routing on an IP network\n"
       "IP prefix <network>/<length>, e.g., 35.0.0.0/8\n"
       "Interface name\n")
{
  int ret;
  struct prefix_ipv4 p;

  ret = str2prefix_ipv4 (argv[0], &p);

  if (ret)
    ret = rip_enable_network_delete ((struct prefix *) &p);
  else
    ret = rip_enable_if_delete (argv[0]);

  if (ret < 0)
    {
      vty_out (vty, "Can't find network configuration %s%s", argv[0],
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  rip_enable_apply_all ();

  return CMD_SUCCESS;
}

/* RIP neighbor configuration set. */
DEFUN (rip_neighbor,
       rip_neighbor_cmd,
       "neighbor A.B.C.D",
       "Specify a neighbor router\n"
       "Neighbor address\n")
{
  int ret;
  struct prefix_ipv4 p;

  ret = str2prefix_ipv4 (argv[0], &p);

  if (ret <= 0)
    {
      vty_out (vty, "Please specify address by A.B.C.D%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  rip_neighbor_add (&p);
  
  return CMD_SUCCESS;
}

/* RIP neighbor configuration unset. */
DEFUN (no_rip_neighbor,
       no_rip_neighbor_cmd,
       "no neighbor A.B.C.D",
       NO_STR
       "Specify a neighbor router\n"
       "Neighbor address\n")
{
  int ret;
  struct prefix_ipv4 p;

  ret = str2prefix_ipv4 (argv[0], &p);

  if (ret <= 0)
    {
      vty_out (vty, "Please specify address by A.B.C.D%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  rip_neighbor_delete (&p);
  
  return CMD_SUCCESS;
}

DEFUN (ip_rip_receive_version,
       ip_rip_receive_version_cmd,
       "ip rip receive version (1|2)",
       IP_STR
       "Routing Information Protocol\n"
       "Advertisement reception\n"
       "Version control\n"
       "RIP version 1\n"
       "RIP version 2\n")
{
  struct interface *ifp;
  struct rip_interface *ri;

  ifp = (struct interface *)vty->index;
  ri = ifp->info;

  /* Version 1. */
  if (atoi (argv[0]) == 1)
    {
      ri->ri_receive = RI_RIP_VERSION_1;
      return CMD_SUCCESS;
    }
  if (atoi (argv[0]) == 2)
    {
      ri->ri_receive = RI_RIP_VERSION_2;
      return CMD_SUCCESS;
    }
  return CMD_WARNING;
}

DEFUN (ip_rip_receive_version_1,
       ip_rip_receive_version_1_cmd,
       "ip rip receive version 1 2",
       IP_STR
       "Routing Information Protocol\n"
       "Advertisement reception\n"
       "Version control\n"
       "RIP version 1\n"
       "RIP version 2\n")
{
  struct interface *ifp;
  struct rip_interface *ri;

  ifp = (struct interface *)vty->index;
  ri = ifp->info;

  /* Version 1 and 2. */
  ri->ri_receive = RI_RIP_VERSION_1_AND_2;
  return CMD_SUCCESS;
}

DEFUN (ip_rip_receive_version_2,
       ip_rip_receive_version_2_cmd,
       "ip rip receive version 2 1",
       IP_STR
       "Routing Information Protocol\n"
       "Advertisement reception\n"
       "Version control\n"
       "RIP version 2\n"
       "RIP version 1\n")
{
  struct interface *ifp;
  struct rip_interface *ri;

  ifp = (struct interface *)vty->index;
  ri = ifp->info;

  /* Version 1 and 2. */
  ri->ri_receive = RI_RIP_VERSION_1_AND_2;
  return CMD_SUCCESS;
}

DEFUN (no_ip_rip_receive_version,
       no_ip_rip_receive_version_cmd,
       "no ip rip receive version",
       NO_STR
       IP_STR
       "Routing Information Protocol\n"
       "Advertisement reception\n"
       "Version control\n")
{
  struct interface *ifp;
  struct rip_interface *ri;

  ifp = (struct interface *)vty->index;
  ri = ifp->info;

  ri->ri_receive = RI_RIP_UNSPEC;
  return CMD_SUCCESS;
}

ALIAS (no_ip_rip_receive_version,
       no_ip_rip_receive_version_num_cmd,
       "no ip rip receive version (1|2)",
       NO_STR
       IP_STR
       "Routing Information Protocol\n"
       "Advertisement reception\n"
       "Version control\n"
       "Version 1\n"
       "Version 2\n");

DEFUN (ip_rip_send_version,
       ip_rip_send_version_cmd,
       "ip rip send version (1|2)",
       IP_STR
       "Routing Information Protocol\n"
       "Advertisement transmission\n"
       "Version control\n"
       "RIP version 1\n"
       "RIP version 2\n")
{
  struct interface *ifp;
  struct rip_interface *ri;

  ifp = (struct interface *)vty->index;
  ri = ifp->info;

  /* Version 1. */
  if (atoi (argv[0]) == 1)
    {
      ri->ri_send = RI_RIP_VERSION_1;
      return CMD_SUCCESS;
    }
  if (atoi (argv[0]) == 2)
    {
      ri->ri_send = RI_RIP_VERSION_2;
      return CMD_SUCCESS;
    }
  return CMD_WARNING;
}

DEFUN (ip_rip_send_version_1,
       ip_rip_send_version_1_cmd,
       "ip rip send version 1 2",
       IP_STR
       "Routing Information Protocol\n"
       "Advertisement transmission\n"
       "Version control\n"
       "RIP version 1\n"
       "RIP version 2\n")
{
  struct interface *ifp;
  struct rip_interface *ri;

  ifp = (struct interface *)vty->index;
  ri = ifp->info;

  /* Version 1 and 2. */
  ri->ri_send = RI_RIP_VERSION_1_AND_2;
  return CMD_SUCCESS;
}

DEFUN (ip_rip_send_version_2,
       ip_rip_send_version_2_cmd,
       "ip rip send version 2 1",
       IP_STR
       "Routing Information Protocol\n"
       "Advertisement transmission\n"
       "Version control\n"
       "RIP version 2\n"
       "RIP version 1\n")
{
  struct interface *ifp;
  struct rip_interface *ri;

  ifp = (struct interface *)vty->index;
  ri = ifp->info;

  /* Version 1 and 2. */
  ri->ri_send = RI_RIP_VERSION_1_AND_2;
  return CMD_SUCCESS;
}

DEFUN (no_ip_rip_send_version,
       no_ip_rip_send_version_cmd,
       "no ip rip send version",
       NO_STR
       IP_STR
       "Routing Information Protocol\n"
       "Advertisement transmission\n"
       "Version control\n")
{
  struct interface *ifp;
  struct rip_interface *ri;

  ifp = (struct interface *)vty->index;
  ri = ifp->info;

  ri->ri_send = RI_RIP_UNSPEC;
  return CMD_SUCCESS;
}

ALIAS (no_ip_rip_send_version,
       no_ip_rip_send_version_num_cmd,
       "no ip rip send version (1|2)",
       NO_STR
       IP_STR
       "Routing Information Protocol\n"
       "Advertisement transmission\n"
       "Version control\n"
       "Version 1\n"
       "Version 2\n");

DEFUN (ip_rip_authentication_mode,
       ip_rip_authentication_mode_cmd,
       "ip rip authentication mode (md5|text)",
       IP_STR
       "Routing Information Protocol\n"
       "Authentication control\n"
       "Authentication mode\n"
       "Keyed message digest\n"
       "Clear text authentication\n")
{
  struct interface *ifp;
  struct rip_interface *ri;

  ifp = (struct interface *)vty->index;
  ri = ifp->info;

  if (strncmp ("md5", argv[0], strlen (argv[0])) == 0)
    ri->auth_type = RIP_AUTH_MD5;
  else if (strncmp ("text", argv[0], strlen (argv[0])) == 0)
    ri->auth_type = RIP_AUTH_SIMPLE_PASSWORD;
  else
    {
      vty_out (vty, "mode should be md5 or text%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  return CMD_SUCCESS;
}

DEFUN (no_ip_rip_authentication_mode,
       no_ip_rip_authentication_mode_cmd,
       "no ip rip authentication mode",
       NO_STR
       IP_STR
       "Routing Information Protocol\n"
       "Authentication control\n"
       "Authentication mode\n")
{
  struct interface *ifp;
  struct rip_interface *ri;

  ifp = (struct interface *)vty->index;
  ri = ifp->info;

  /* ri->auth_type = RIP_NO_AUTH; */
  ri->auth_type = RIP_AUTH_SIMPLE_PASSWORD;

  return CMD_SUCCESS;
}

ALIAS (no_ip_rip_authentication_mode,
       no_ip_rip_authentication_mode_type_cmd,
       "no ip rip authentication mode (md5|text)",
       NO_STR
       IP_STR
       "Routing Information Protocol\n"
       "Authentication control\n"
       "Authentication mode\n"
       "Keyed message digest\n"
       "Clear text authentication\n");

DEFUN (ip_rip_authentication_string,
       ip_rip_authentication_string_cmd,
       "ip rip authentication string LINE",
       IP_STR
       "Routing Information Protocol\n"
       "Authentication control\n"
       "Authentication string\n"
       "Authentication string\n")
{
  struct interface *ifp;
  struct rip_interface *ri;

  ifp = (struct interface *)vty->index;
  ri = ifp->info;

  if (strlen (argv[0]) > 16)
    {
      vty_out (vty, "%% RIPv2 authentication string must be shorter than 16%s",
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (ri->key_chain)
    {
      vty_out (vty, "%% key-chain configuration exists%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (ri->auth_str)
    free (ri->auth_str);

  ri->auth_str = strdup (argv[0]);

  return CMD_SUCCESS;
}

DEFUN (no_ip_rip_authentication_string,
       no_ip_rip_authentication_string_cmd,
       "no ip rip authentication string",
       NO_STR
       IP_STR
       "Routing Information Protocol\n"
       "Authentication control\n"
       "Authentication string\n")
{
  struct interface *ifp;
  struct rip_interface *ri;

  ifp = (struct interface *)vty->index;
  ri = ifp->info;

  if (ri->auth_str)
    free (ri->auth_str);

  ri->auth_str = NULL;

  return CMD_SUCCESS;
}

ALIAS (no_ip_rip_authentication_string,
       no_ip_rip_authentication_string2_cmd,
       "no ip rip authentication string LINE",
       NO_STR
       IP_STR
       "Routing Information Protocol\n"
       "Authentication control\n"
       "Authentication string\n"
       "Authentication string\n");

DEFUN (ip_rip_authentication_key_chain,
       ip_rip_authentication_key_chain_cmd,
       "ip rip authentication key-chain LINE",
       IP_STR
       "Routing Information Protocol\n"
       "Authentication control\n"
       "Authentication key-chain\n"
       "name of key-chain\n")
{
  struct interface *ifp;
  struct rip_interface *ri;

  ifp = (struct interface *) vty->index;
  ri = ifp->info;

  if (ri->auth_str)
    {
      vty_out (vty, "%% authentication string configuration exists%s",
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (ri->key_chain)
    free (ri->key_chain);

  ri->key_chain = strdup (argv[0]);

  return CMD_SUCCESS;
}

DEFUN (no_ip_rip_authentication_key_chain,
       no_ip_rip_authentication_key_chain_cmd,
       "no ip rip authentication key-chain",
       NO_STR
       IP_STR
       "Routing Information Protocol\n"
       "Authentication control\n"
       "Authentication key-chain\n")
{
  struct interface *ifp;
  struct rip_interface *ri;

  ifp = (struct interface *) vty->index;
  ri = ifp->info;

  if (ri->key_chain)
    free (ri->key_chain);

  ri->key_chain = NULL;

  return CMD_SUCCESS;
}

ALIAS (no_ip_rip_authentication_key_chain,
       no_ip_rip_authentication_key_chain2_cmd,
       "no ip rip authentication key-chain LINE",
       NO_STR
       IP_STR
       "Routing Information Protocol\n"
       "Authentication control\n"
       "Authentication key-chain\n"
       "name of key-chain\n");

DEFUN (rip_split_horizon,
       rip_split_horizon_cmd,
       "ip split-horizon",
       IP_STR
       "Perform split horizon\n")
{
  struct interface *ifp;
  struct rip_interface *ri;

  ifp = vty->index;
  ri = ifp->info;

  ri->split_horizon = 1;
  return CMD_SUCCESS;
}

DEFUN (no_rip_split_horizon,
       no_rip_split_horizon_cmd,
       "no ip split-horizon",
       NO_STR
       IP_STR
       "Perform split horizon\n")
{
  struct interface *ifp;
  struct rip_interface *ri;

  ifp = vty->index;
  ri = ifp->info;

  ri->split_horizon = 0;
  return CMD_SUCCESS;
}

DEFUN (rip_passive_interface,
       rip_passive_interface_cmd,
       "passive-interface IFNAME",
       "Suppress routing updates on an interface\n"
       "Interface name\n")
{
  return rip_passive_interface_set (vty, argv[0]);
}

DEFUN (no_rip_passive_interface,
       no_rip_passive_interface_cmd,
       "no passive-interface IFNAME",
       NO_STR
       "Suppress routing updates on an interface\n"
       "Interface name\n")
{
  return rip_passive_interface_unset (vty, argv[0]);
}

/* Write rip configuration of each interface. */
int
rip_interface_config_write (struct vty *vty)
{
  listnode node;
  struct interface *ifp;

  for (node = listhead (iflist); node; nextnode (node))
    {
      struct rip_interface *ri;

      ifp = getdata (node);
      ri = ifp->info;

      vty_out (vty, "interface %s%s", ifp->name,
	       VTY_NEWLINE);

      if (ifp->desc)
	vty_out (vty, " description %s%s", ifp->desc,
		 VTY_NEWLINE);

      /* Split horizon. */
      if (ri->split_horizon != ri->split_horizon_default)
	{
	  if (ri->split_horizon)
	    vty_out (vty, " ip split-horizon%s", VTY_NEWLINE);
	  else
	    vty_out (vty, " no ip split-horizon%s", VTY_NEWLINE);
	}

      /* RIP version setting. */
      if (ri->ri_send != RI_RIP_UNSPEC)
	vty_out (vty, " ip rip send version %s%s",
		 lookup (ri_version_msg, ri->ri_send),
		 VTY_NEWLINE);

      if (ri->ri_receive != RI_RIP_UNSPEC)
	vty_out (vty, " ip rip receive version %s%s",
		 lookup (ri_version_msg, ri->ri_receive),
		 VTY_NEWLINE);

      /* RIP authentication. */
      if (ri->auth_type == RIP_AUTH_MD5)
	vty_out (vty, " ip rip authentication mode md5%s", VTY_NEWLINE);

      if (ri->auth_str)
	vty_out (vty, " ip rip authentication string %s%s",
		 ri->auth_str, VTY_NEWLINE);

      if (ri->key_chain)
	vty_out (vty, " ip rip authentication key-chain %s%s",
		 ri->key_chain, VTY_NEWLINE);

      vty_out (vty, "!%s", VTY_NEWLINE);
    }
  return 0;
}

int
config_write_rip_network (struct vty *vty, int config_mode)
{
  int i;
  char *ifname;
  struct route_node *node;

  /* Network type RIP enable interface statement. */
  for (node = route_top (rip_enable_network); node; node = route_next (node))
    if (node->info)
      vty_out (vty, "%s%s/%d%s", 
	       config_mode ? " network " : "    ",
	       inet_ntoa (node->p.u.prefix4),
	       node->p.prefixlen,
	       VTY_NEWLINE);

  /* Interface name RIP enable statement. */
  for (i = 0; i < vector_max (rip_enable_interface); i++)
    if ((ifname = vector_slot (rip_enable_interface, i)) != NULL)
      vty_out (vty, "%s%s%s",
	       config_mode ? " network " : "    ",
	       ifname,
	       VTY_NEWLINE);

  /* RIP neighbors listing. */
  for (node = route_top (rip->neighbor); node; node = route_next (node))
    if (node->info)
      vty_out (vty, "%s%s%s", 
	       config_mode ? " neighbor " : "    ",
	       inet_ntoa (node->p.u.prefix4),
	       VTY_NEWLINE);

  /* RIP passive interface listing. */
  if (config_mode)
    for (i = 0; i < vector_max (Vrip_passive_interface); i++)
      if ((ifname = vector_slot (Vrip_passive_interface, i)) != NULL)
	vty_out (vty, " passive-interface %s%s", ifname, VTY_NEWLINE);

  return 0;
}

struct cmd_node interface_node =
{
  INTERFACE_NODE,
  "%s(config-if)# ",
  1,
};

/* Called when interface structure allocated. */
int
rip_interface_new_hook (struct interface *ifp)
{
  ifp->info = rip_interface_new ();
  return 0;
}

/* Called when interface structure deleted. */
int
rip_interface_delete_hook (struct interface *ifp)
{
  XFREE (MTYPE_RIP_INTERFACE, ifp->info);
  return 0;
}

/* Allocate and initialize interface vector. */
void
rip_if_init ()
{
  /* Default initial size of interface vector. */
  if_init();
  if_add_hook (IF_NEW_HOOK, rip_interface_new_hook);
  if_add_hook (IF_DELETE_HOOK, rip_interface_delete_hook);
  
  /* RIP network init. */
  rip_enable_interface = vector_init (1);
  rip_enable_network = route_table_init ();

  /* RIP passive interface. */
  Vrip_passive_interface = vector_init (1);

  /* Install interface node. */
  install_node (&interface_node, rip_interface_config_write);

  /* Install commands. */
  install_element (CONFIG_NODE, &interface_cmd);
  install_default (INTERFACE_NODE);
  install_element (INTERFACE_NODE, &interface_desc_cmd);
  install_element (INTERFACE_NODE, &no_interface_desc_cmd);
  install_element (RIP_NODE, &rip_network_cmd);
  install_element (RIP_NODE, &no_rip_network_cmd);
  install_element (RIP_NODE, &rip_neighbor_cmd);
  install_element (RIP_NODE, &no_rip_neighbor_cmd);

  install_element (RIP_NODE, &rip_passive_interface_cmd);
  install_element (RIP_NODE, &no_rip_passive_interface_cmd);

  install_element (INTERFACE_NODE, &ip_rip_send_version_cmd);
  install_element (INTERFACE_NODE, &ip_rip_send_version_1_cmd);
  install_element (INTERFACE_NODE, &ip_rip_send_version_2_cmd);
  install_element (INTERFACE_NODE, &no_ip_rip_send_version_cmd);
  install_element (INTERFACE_NODE, &no_ip_rip_send_version_num_cmd);

  install_element (INTERFACE_NODE, &ip_rip_receive_version_cmd);
  install_element (INTERFACE_NODE, &ip_rip_receive_version_1_cmd);
  install_element (INTERFACE_NODE, &ip_rip_receive_version_2_cmd);
  install_element (INTERFACE_NODE, &no_ip_rip_receive_version_cmd);
  install_element (INTERFACE_NODE, &no_ip_rip_receive_version_num_cmd);

  install_element (INTERFACE_NODE, &ip_rip_authentication_mode_cmd);
  install_element (INTERFACE_NODE, &no_ip_rip_authentication_mode_cmd);
  install_element (INTERFACE_NODE, &no_ip_rip_authentication_mode_type_cmd);

  install_element (INTERFACE_NODE, &ip_rip_authentication_key_chain_cmd);
  install_element (INTERFACE_NODE, &no_ip_rip_authentication_key_chain_cmd);
  install_element (INTERFACE_NODE, &no_ip_rip_authentication_key_chain2_cmd);

  install_element (INTERFACE_NODE, &ip_rip_authentication_string_cmd);
  install_element (INTERFACE_NODE, &no_ip_rip_authentication_string_cmd);
  install_element (INTERFACE_NODE, &no_ip_rip_authentication_string2_cmd);

  install_element (INTERFACE_NODE, &rip_split_horizon_cmd);
  install_element (INTERFACE_NODE, &no_rip_split_horizon_cmd);
}
