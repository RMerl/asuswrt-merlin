/*
 * Interface related function for RIPng.
 * Copyright (C) 1998 Kunihiro Ishiguro
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

#include "linklist.h"
#include "if.h"
#include "prefix.h"
#include "memory.h"
#include "network.h"
#include "filter.h"
#include "log.h"
#include "stream.h"
#include "zclient.h"
#include "command.h"
#include "table.h"
#include "thread.h"
#include "privs.h"

#include "ripngd/ripngd.h"
#include "ripngd/ripng_debug.h"

/* If RFC2133 definition is used. */
#ifndef IPV6_JOIN_GROUP
#define IPV6_JOIN_GROUP  IPV6_ADD_MEMBERSHIP 
#endif
#ifndef IPV6_LEAVE_GROUP
#define IPV6_LEAVE_GROUP IPV6_DROP_MEMBERSHIP 
#endif

extern struct zebra_privs_t ripngd_privs;

/* Static utility function. */
static void ripng_enable_apply (struct interface *);
static void ripng_passive_interface_apply (struct interface *);
static int ripng_enable_if_lookup (const char *);
static int ripng_enable_network_lookup2 (struct connected *);
static void ripng_enable_apply_all (void);

/* Join to the all rip routers multicast group. */
static int
ripng_multicast_join (struct interface *ifp)
{
  int ret;
  struct ipv6_mreq mreq;
  int save_errno;

  if (if_is_up (ifp) && if_is_multicast (ifp)) {
    memset (&mreq, 0, sizeof (mreq));
    inet_pton(AF_INET6, RIPNG_GROUP, &mreq.ipv6mr_multiaddr);
    mreq.ipv6mr_interface = ifp->ifindex;

    /*
     * NetBSD 1.6.2 requires root to join groups on gif(4).
     * While this is bogus, privs are available and easy to use
     * for this call as a workaround.
     */
    if (ripngd_privs.change (ZPRIVS_RAISE))
      zlog_err ("ripng_multicast_join: could not raise privs");

    ret = setsockopt (ripng->sock, IPPROTO_IPV6, IPV6_JOIN_GROUP,
		      (char *) &mreq, sizeof (mreq));
    save_errno = errno;

    if (ripngd_privs.change (ZPRIVS_LOWER))
      zlog_err ("ripng_multicast_join: could not lower privs");

    if (ret < 0 && save_errno == EADDRINUSE)
      {
	/*
	 * Group is already joined.  This occurs due to sloppy group
	 * management, in particular declining to leave the group on
	 * an interface that has just gone down.
	 */
	zlog_warn ("ripng join on %s EADDRINUSE (ignoring)\n", ifp->name);
	return 0;		/* not an error */
      }

    if (ret < 0)
      zlog_warn ("can't setsockopt IPV6_JOIN_GROUP: %s",
      		 safe_strerror (save_errno));

    if (IS_RIPNG_DEBUG_EVENT)
      zlog_debug ("RIPng %s join to all-rip-routers multicast group", ifp->name);

    if (ret < 0)
      return -1;
  }
  return 0;
}

/* Leave from the all rip routers multicast group. */
static int
ripng_multicast_leave (struct interface *ifp)
{
  int ret;
  struct ipv6_mreq mreq;

  if (if_is_up (ifp) && if_is_multicast (ifp)) {
    memset (&mreq, 0, sizeof (mreq));
    inet_pton(AF_INET6, RIPNG_GROUP, &mreq.ipv6mr_multiaddr);
    mreq.ipv6mr_interface = ifp->ifindex;

    ret = setsockopt (ripng->sock, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
		      (char *) &mreq, sizeof (mreq));
    if (ret < 0)
      zlog_warn ("can't setsockopt IPV6_LEAVE_GROUP: %s\n", safe_strerror (errno));

    if (IS_RIPNG_DEBUG_EVENT)
      zlog_debug ("RIPng %s leave from all-rip-routers multicast group",
	         ifp->name);

    if (ret < 0)
      return -1;
  }

  return 0;
}

/* How many link local IPv6 address could be used on the interface ? */
static int
ripng_if_ipv6_lladdress_check (struct interface *ifp)
{
  struct listnode *nn;
  struct connected *connected;
  int count = 0;

  for (ALL_LIST_ELEMENTS_RO (ifp->connected, nn, connected))
    {
      struct prefix *p;
      p = connected->address;

      if ((p->family == AF_INET6) &&
          IN6_IS_ADDR_LINKLOCAL (&p->u.prefix6))
        count++;
    }

  return count;
}

static int
ripng_if_down (struct interface *ifp)
{
  struct route_node *rp;
  struct ripng_info *rinfo;
  struct ripng_interface *ri;

  if (ripng)
    {
      for (rp = route_top (ripng->table); rp; rp = route_next (rp))
	if ((rinfo = rp->info) != NULL)
	  {
	    /* Routes got through this interface. */
	    if (rinfo->ifindex == ifp->ifindex
		&& rinfo->type == ZEBRA_ROUTE_RIPNG
		&& rinfo->sub_type == RIPNG_ROUTE_RTE)
	      {
		ripng_zebra_ipv6_delete ((struct prefix_ipv6 *) &rp->p,
					 &rinfo->nexthop,
					 rinfo->ifindex);

		ripng_redistribute_delete (rinfo->type, rinfo->sub_type,
					   (struct prefix_ipv6 *)&rp->p,
					   rinfo->ifindex);
	      }
	    else
	      {
		/* All redistributed routes got through this interface,
		 * but the static and system ones are kept. */
		if ((rinfo->ifindex == ifp->ifindex) &&
		    (rinfo->type != ZEBRA_ROUTE_STATIC) &&
		    (rinfo->type != ZEBRA_ROUTE_SYSTEM))
		  ripng_redistribute_delete (rinfo->type, rinfo->sub_type,
					     (struct prefix_ipv6 *) &rp->p,
					     rinfo->ifindex);
	      }
	  }
    }

  ri = ifp->info;
  
  if (ri->running)
   {
     if (IS_RIPNG_DEBUG_EVENT)
       zlog_debug ("turn off %s", ifp->name);

     /* Leave from multicast group. */
     ripng_multicast_leave (ifp);

     ri->running = 0;
   }

  return 0;
}

/* Inteface link up message processing. */
int
ripng_interface_up (int command, struct zclient *zclient, zebra_size_t length)
{
  struct stream *s;
  struct interface *ifp;

  /* zebra_interface_state_read() updates interface structure in iflist. */
  s = zclient->ibuf;
  ifp = zebra_interface_state_read (s);

  if (ifp == NULL)
    return 0;

  if (IS_RIPNG_DEBUG_ZEBRA)
    zlog_debug ("interface up %s index %d flags %llx metric %d mtu %d",
	       ifp->name, ifp->ifindex, (unsigned long long)ifp->flags,
	       ifp->metric, ifp->mtu6);

  /* Check if this interface is RIPng enabled or not. */
  ripng_enable_apply (ifp);

  /* Check for a passive interface. */
  ripng_passive_interface_apply (ifp);

  /* Apply distribute list to the all interface. */
  ripng_distribute_update_interface (ifp);

  return 0;
}

/* Inteface link down message processing. */
int
ripng_interface_down (int command, struct zclient *zclient,
		      zebra_size_t length)
{
  struct stream *s;
  struct interface *ifp;

  /* zebra_interface_state_read() updates interface structure in iflist. */
  s = zclient->ibuf;
  ifp = zebra_interface_state_read (s);

  if (ifp == NULL)
    return 0;

  ripng_if_down (ifp);

  if (IS_RIPNG_DEBUG_ZEBRA)
    zlog_debug ("interface down %s index %d flags %#llx metric %d mtu %d",
		ifp->name, ifp->ifindex,
		(unsigned long long) ifp->flags, ifp->metric, ifp->mtu6);

  return 0;
}

/* Inteface addition message from zebra. */
int
ripng_interface_add (int command, struct zclient *zclient, zebra_size_t length)
{
  struct interface *ifp;

  ifp = zebra_interface_add_read (zclient->ibuf);

  if (IS_RIPNG_DEBUG_ZEBRA)
    zlog_debug ("RIPng interface add %s index %d flags %#llx metric %d mtu %d",
		ifp->name, ifp->ifindex, (unsigned long long) ifp->flags,
		ifp->metric, ifp->mtu6);

  /* Check is this interface is RIP enabled or not.*/
  ripng_enable_apply (ifp);

  /* Apply distribute list to the interface. */
  ripng_distribute_update_interface (ifp);

  /* Check interface routemap. */
  ripng_if_rmap_update_interface (ifp);

  return 0;
}

int
ripng_interface_delete (int command, struct zclient *zclient,
			zebra_size_t length)
{
  struct interface *ifp;
  struct stream *s;

  s = zclient->ibuf;
  /*  zebra_interface_state_read() updates interface structure in iflist */
  ifp = zebra_interface_state_read(s);

  if (ifp == NULL)
    return 0;

  if (if_is_up (ifp)) {
    ripng_if_down(ifp);
  }

  zlog_info("interface delete %s index %d flags %#llx metric %d mtu %d",
            ifp->name, ifp->ifindex, (unsigned long long) ifp->flags,
	    ifp->metric, ifp->mtu6);

  /* To support pseudo interface do not free interface structure.  */
  /* if_delete(ifp); */
  ifp->ifindex = IFINDEX_INTERNAL;

  return 0;
}

void
ripng_interface_clean (void)
{
  struct listnode *node, *nnode;
  struct interface *ifp;
  struct ripng_interface *ri;

  for (ALL_LIST_ELEMENTS (iflist, node, nnode, ifp))
    {
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
ripng_interface_reset (void)
{
  struct listnode *node;
  struct interface *ifp;
  struct ripng_interface *ri;

  for (ALL_LIST_ELEMENTS_RO (iflist, node, ifp))
    {
      ri = ifp->info;

      ri->enable_network = 0;
      ri->enable_interface = 0;
      ri->running = 0;

      ri->split_horizon = RIPNG_NO_SPLIT_HORIZON;
      ri->split_horizon_default = RIPNG_NO_SPLIT_HORIZON;

      ri->list[RIPNG_FILTER_IN] = NULL;
      ri->list[RIPNG_FILTER_OUT] = NULL;

      ri->prefix[RIPNG_FILTER_IN] = NULL;
      ri->prefix[RIPNG_FILTER_OUT] = NULL;

      if (ri->t_wakeup)
        {
          thread_cancel (ri->t_wakeup);
          ri->t_wakeup = NULL;
        }

      ri->passive = 0;
    }
}

static void
ripng_apply_address_add (struct connected *ifc) {
  struct prefix_ipv6 address;
  struct prefix *p;

  if (!ripng)
    return;

  if (! if_is_up(ifc->ifp))
    return;

  p = ifc->address;

  memset (&address, 0, sizeof (address));
  address.family = p->family;
  address.prefix = p->u.prefix6;
  address.prefixlen = p->prefixlen;
  apply_mask_ipv6(&address);

  /* Check if this interface is RIP enabled or not
     or  Check if this address's prefix is RIP enabled */
  if ((ripng_enable_if_lookup(ifc->ifp->name) >= 0) ||
      (ripng_enable_network_lookup2(ifc) >= 0))
    ripng_redistribute_add(ZEBRA_ROUTE_CONNECT, RIPNG_ROUTE_INTERFACE,
                           &address, ifc->ifp->ifindex, NULL);

}

int
ripng_interface_address_add (int command, struct zclient *zclient,
			     zebra_size_t length)
{
  struct connected *c;
  struct prefix *p;

  c = zebra_interface_address_read (ZEBRA_INTERFACE_ADDRESS_ADD, 
                                    zclient->ibuf);

  if (c == NULL)
    return 0;

  p = c->address;

  if (p->family == AF_INET6)
    {
      struct ripng_interface *ri = c->ifp->info;
      
      if (IS_RIPNG_DEBUG_ZEBRA)
	zlog_debug ("RIPng connected address %s/%d add",
		   inet6_ntoa(p->u.prefix6),
		   p->prefixlen);
      
      /* Check is this prefix needs to be redistributed. */
      ripng_apply_address_add(c);

      /* Let's try once again whether the interface could be activated */
      if (!ri->running) {
        /* Check if this interface is RIP enabled or not.*/
        ripng_enable_apply (c->ifp);

        /* Apply distribute list to the interface. */
        ripng_distribute_update_interface (c->ifp);

        /* Check interface routemap. */
        ripng_if_rmap_update_interface (c->ifp);
      }

    }

  return 0;
}

static void
ripng_apply_address_del (struct connected *ifc) {
  struct prefix_ipv6 address;
  struct prefix *p;

  if (!ripng)
    return;

  if (! if_is_up(ifc->ifp))
    return;

  p = ifc->address;

  memset (&address, 0, sizeof (address));
  address.family = p->family;
  address.prefix = p->u.prefix6;
  address.prefixlen = p->prefixlen;
  apply_mask_ipv6(&address);

  ripng_redistribute_delete(ZEBRA_ROUTE_CONNECT, RIPNG_ROUTE_INTERFACE,
                            &address, ifc->ifp->ifindex);
}

int
ripng_interface_address_delete (int command, struct zclient *zclient,
				zebra_size_t length)
{
  struct connected *ifc;
  struct prefix *p;
  char buf[INET6_ADDRSTRLEN];

  ifc = zebra_interface_address_read (ZEBRA_INTERFACE_ADDRESS_DELETE, 
                                      zclient->ibuf);
  
  if (ifc)
    {
      p = ifc->address;

      if (p->family == AF_INET6)
	{
	  if (IS_RIPNG_DEBUG_ZEBRA)
	    zlog_debug ("RIPng connected address %s/%d delete",
		       inet_ntop (AF_INET6, &p->u.prefix6, buf,
				  INET6_ADDRSTRLEN),
		       p->prefixlen);

	  /* Check wether this prefix needs to be removed. */
	  ripng_apply_address_del(ifc);
	}
      connected_free (ifc);
    }

  return 0;
}

/* RIPng enable interface vector. */
vector ripng_enable_if;

/* RIPng enable network table. */
struct route_table *ripng_enable_network;

/* Lookup RIPng enable network. */
/* Check wether the interface has at least a connected prefix that
 * is within the ripng_enable_network table. */
static int
ripng_enable_network_lookup_if (struct interface *ifp)
{
  struct listnode *node;
  struct connected *connected;
  struct prefix_ipv6 address;

  for (ALL_LIST_ELEMENTS_RO (ifp->connected, node, connected))
    {
      struct prefix *p; 
      struct route_node *node;

      p = connected->address;

      if (p->family == AF_INET6)
        {
          address.family = AF_INET6;
          address.prefix = p->u.prefix6;
          address.prefixlen = IPV6_MAX_BITLEN;

          node = route_node_match (ripng_enable_network,
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

/* Check wether connected is within the ripng_enable_network table. */
static int
ripng_enable_network_lookup2 (struct connected *connected)
{
  struct prefix_ipv6 address;
  struct prefix *p;

  p = connected->address;

  if (p->family == AF_INET6) {
    struct route_node *node;

    address.family = p->family;
    address.prefix = p->u.prefix6;
    address.prefixlen = IPV6_MAX_BITLEN;

    /* LPM on p->family, p->u.prefix6/IPV6_MAX_BITLEN within ripng_enable_network */
    node = route_node_match (ripng_enable_network,
                             (struct prefix *)&address);

    if (node) {
      route_unlock_node (node);
      return 1;
    }
  }

  return -1;
}

/* Add RIPng enable network. */
static int
ripng_enable_network_add (struct prefix *p)
{
  struct route_node *node;

  node = route_node_get (ripng_enable_network, p);

  if (node->info)
    {
      route_unlock_node (node);
      return -1;
    }
  else
    node->info = (char *) "enabled";

  /* XXX: One should find a better solution than a generic one */
  ripng_enable_apply_all();

  return 1;
}

/* Delete RIPng enable network. */
static int
ripng_enable_network_delete (struct prefix *p)
{
  struct route_node *node;

  node = route_node_lookup (ripng_enable_network, p);
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

/* Lookup function. */
static int
ripng_enable_if_lookup (const char *ifname)
{
  unsigned int i;
  char *str;

  for (i = 0; i < vector_active (ripng_enable_if); i++)
    if ((str = vector_slot (ripng_enable_if, i)) != NULL)
      if (strcmp (str, ifname) == 0)
	return i;
  return -1;
}

/* Add interface to ripng_enable_if. */
static int
ripng_enable_if_add (const char *ifname)
{
  int ret;

  ret = ripng_enable_if_lookup (ifname);
  if (ret >= 0)
    return -1;

  vector_set (ripng_enable_if, strdup (ifname));

  ripng_enable_apply_all();

  return 1;
}

/* Delete interface from ripng_enable_if. */
static int
ripng_enable_if_delete (const char *ifname)
{
  int index;
  char *str;

  index = ripng_enable_if_lookup (ifname);
  if (index < 0)
    return -1;

  str = vector_slot (ripng_enable_if, index);
  free (str);
  vector_unset (ripng_enable_if, index);

  ripng_enable_apply_all();

  return 1;
}

/* Wake up interface. */
static int
ripng_interface_wakeup (struct thread *t)
{
  struct interface *ifp;
  struct ripng_interface *ri;

  /* Get interface. */
  ifp = THREAD_ARG (t);

  ri = ifp->info;
  ri->t_wakeup = NULL;

  /* Join to multicast group. */
  if (ripng_multicast_join (ifp) < 0) {
    zlog_err ("multicast join failed, interface %s not running", ifp->name);
    return 0;
  }
    
  /* Set running flag. */
  ri->running = 1;

  /* Send RIP request to the interface. */
  ripng_request (ifp);

  return 0;
}

static void
ripng_connect_set (struct interface *ifp, int set)
{
  struct listnode *node, *nnode;
  struct connected *connected;
  struct prefix_ipv6 address;

  for (ALL_LIST_ELEMENTS (ifp->connected, node, nnode, connected))
    {
      struct prefix *p;
      p = connected->address;

      if (p->family != AF_INET6)
        continue;

      address.family = AF_INET6;
      address.prefix = p->u.prefix6;
      address.prefixlen = p->prefixlen;
      apply_mask_ipv6 (&address);

      if (set) {
        /* Check once more wether this prefix is within a "network IF_OR_PREF" one */
        if ((ripng_enable_if_lookup(connected->ifp->name) >= 0) ||
            (ripng_enable_network_lookup2(connected) >= 0))
          ripng_redistribute_add (ZEBRA_ROUTE_CONNECT, RIPNG_ROUTE_INTERFACE,
                                  &address, connected->ifp->ifindex, NULL);
      } else {
        ripng_redistribute_delete (ZEBRA_ROUTE_CONNECT, RIPNG_ROUTE_INTERFACE,
                                   &address, connected->ifp->ifindex);
        if (ripng_redistribute_check (ZEBRA_ROUTE_CONNECT))
          ripng_redistribute_add (ZEBRA_ROUTE_CONNECT, RIPNG_ROUTE_REDISTRIBUTE,
                                  &address, connected->ifp->ifindex, NULL);
      }
    }
}

/* Check RIPng is enabed on this interface. */
void
ripng_enable_apply (struct interface *ifp)
{
  int ret;
  struct ripng_interface *ri = NULL;

  /* Check interface. */
  if (! if_is_up (ifp))
    return;
  
  ri = ifp->info;

  /* Is this interface a candidate for RIPng ? */
  ret = ripng_enable_network_lookup_if (ifp);

  /* If the interface is matched. */
  if (ret > 0)
    ri->enable_network = 1;
  else
    ri->enable_network = 0;

  /* Check interface name configuration. */
  ret = ripng_enable_if_lookup (ifp->name);
  if (ret >= 0)
    ri->enable_interface = 1;
  else
    ri->enable_interface = 0;

  /* any candidate interface MUST have a link-local IPv6 address */
  if ((! ripng_if_ipv6_lladdress_check (ifp)) &&
      (ri->enable_network || ri->enable_interface)) {
    ri->enable_network = 0;
    ri->enable_interface = 0;
    zlog_warn("Interface %s does not have any link-local address",
              ifp->name);
  }

  /* Update running status of the interface. */
  if (ri->enable_network || ri->enable_interface)
    {
	{
	  if (IS_RIPNG_DEBUG_EVENT)
	    zlog_debug ("RIPng turn on %s", ifp->name);

	  /* Add interface wake up thread. */
	  if (! ri->t_wakeup)
	    ri->t_wakeup = thread_add_timer (master, ripng_interface_wakeup,
					     ifp, 1);

	  ripng_connect_set (ifp, 1);
	}
    }
  else
    {
      if (ri->running)
	{
	  /* Might as well clean up the route table as well
	   * ripng_if_down sets to 0 ri->running, and displays "turn off %s"
	   **/
	  ripng_if_down(ifp);

	  ripng_connect_set (ifp, 0);
	}
    }
}

/* Set distribute list to all interfaces. */
static void
ripng_enable_apply_all (void)
{
  struct interface *ifp;
  struct listnode *node;

  for (ALL_LIST_ELEMENTS_RO (iflist, node, ifp))
    ripng_enable_apply (ifp);
}

/* Clear all network and neighbor configuration */
void
ripng_clean_network ()
{
  unsigned int i;
  char *str;
  struct route_node *rn;

  /* ripng_enable_network */
  for (rn = route_top (ripng_enable_network); rn; rn = route_next (rn))
    if (rn->info) {
      rn->info = NULL;
      route_unlock_node(rn);
    }

  /* ripng_enable_if */
  for (i = 0; i < vector_active (ripng_enable_if); i++)
    if ((str = vector_slot (ripng_enable_if, i)) != NULL) {
      free (str);
      vector_slot (ripng_enable_if, i) = NULL;
    }
}

/* Vector to store passive-interface name. */
vector Vripng_passive_interface;

/* Utility function for looking up passive interface settings. */
static int
ripng_passive_interface_lookup (const char *ifname)
{
  unsigned int i;
  char *str;

  for (i = 0; i < vector_active (Vripng_passive_interface); i++)
    if ((str = vector_slot (Vripng_passive_interface, i)) != NULL)
      if (strcmp (str, ifname) == 0)
	return i;
  return -1;
}

void
ripng_passive_interface_apply (struct interface *ifp)
{
  int ret;
  struct ripng_interface *ri;

  ri = ifp->info;

  ret = ripng_passive_interface_lookup (ifp->name);
  if (ret < 0)
    ri->passive = 0;
  else
    ri->passive = 1;
}

static void
ripng_passive_interface_apply_all (void)
{
  struct interface *ifp;
  struct listnode *node;

  for (ALL_LIST_ELEMENTS_RO (iflist, node, ifp))
    ripng_passive_interface_apply (ifp);
}

/* Passive interface. */
static int
ripng_passive_interface_set (struct vty *vty, const char *ifname)
{
  if (ripng_passive_interface_lookup (ifname) >= 0)
    return CMD_WARNING;

  vector_set (Vripng_passive_interface, strdup (ifname));

  ripng_passive_interface_apply_all ();

  return CMD_SUCCESS;
}

static int
ripng_passive_interface_unset (struct vty *vty, const char *ifname)
{
  int i;
  char *str;

  i = ripng_passive_interface_lookup (ifname);
  if (i < 0)
    return CMD_WARNING;

  str = vector_slot (Vripng_passive_interface, i);
  free (str);
  vector_unset (Vripng_passive_interface, i);

  ripng_passive_interface_apply_all ();

  return CMD_SUCCESS;
}

/* Free all configured RIP passive-interface settings. */
void
ripng_passive_interface_clean (void)
{
  unsigned int i;
  char *str;

  for (i = 0; i < vector_active (Vripng_passive_interface); i++)
    if ((str = vector_slot (Vripng_passive_interface, i)) != NULL)
      {
	free (str);
	vector_slot (Vripng_passive_interface, i) = NULL;
      }
  ripng_passive_interface_apply_all ();
}

/* Write RIPng enable network and interface to the vty. */
int
ripng_network_write (struct vty *vty, int config_mode)
{
  unsigned int i;
  const char *ifname;
  struct route_node *node;
  char buf[BUFSIZ];

  /* Write enable network. */
  for (node = route_top (ripng_enable_network); node; node = route_next (node))
    if (node->info)
      {
	struct prefix *p = &node->p;
	vty_out (vty, "%s%s/%d%s", 
		 config_mode ? " network " : "    ",
		 inet_ntop (p->family, &p->u.prefix, buf, BUFSIZ),
		 p->prefixlen,
		 VTY_NEWLINE);

      }
  
  /* Write enable interface. */
  for (i = 0; i < vector_active (ripng_enable_if); i++)
    if ((ifname = vector_slot (ripng_enable_if, i)) != NULL)
      vty_out (vty, "%s%s%s",
	       config_mode ? " network " : "    ",
	       ifname,
	       VTY_NEWLINE);

  /* Write passive interface. */
  if (config_mode)
    for (i = 0; i < vector_active (Vripng_passive_interface); i++)
      if ((ifname = vector_slot (Vripng_passive_interface, i)) != NULL)
        vty_out (vty, " passive-interface %s%s", ifname, VTY_NEWLINE);

  return 0;
}

/* RIPng enable on specified interface or matched network. */
DEFUN (ripng_network,
       ripng_network_cmd,
       "network IF_OR_ADDR",
       "RIPng enable on specified interface or network.\n"
       "Interface or address")
{
  int ret;
  struct prefix p;

  ret = str2prefix (argv[0], &p);

  /* Given string is IPv6 network or interface name. */
  if (ret)
    ret = ripng_enable_network_add (&p);
  else
    ret = ripng_enable_if_add (argv[0]);

  if (ret < 0)
    {
      vty_out (vty, "There is same network configuration %s%s", argv[0],
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  return CMD_SUCCESS;
}

/* RIPng enable on specified interface or matched network. */
DEFUN (no_ripng_network,
       no_ripng_network_cmd,
       "no network IF_OR_ADDR",
       NO_STR
       "RIPng enable on specified interface or network.\n"
       "Interface or address")
{
  int ret;
  struct prefix p;

  ret = str2prefix (argv[0], &p);

  /* Given string is interface name. */
  if (ret)
    ret = ripng_enable_network_delete (&p);
  else
    ret = ripng_enable_if_delete (argv[0]);

  if (ret < 0)
    {
      vty_out (vty, "can't find network %s%s", argv[0],
	       VTY_NEWLINE);
      return CMD_WARNING;
    }
  
  return CMD_SUCCESS;
}

DEFUN (ipv6_ripng_split_horizon,
       ipv6_ripng_split_horizon_cmd,
       "ipv6 ripng split-horizon",
       IPV6_STR
       "Routing Information Protocol\n"
       "Perform split horizon\n")
{
  struct interface *ifp;
  struct ripng_interface *ri;

  ifp = vty->index;
  ri = ifp->info;

  ri->split_horizon = RIPNG_SPLIT_HORIZON;
  return CMD_SUCCESS;
}

DEFUN (ipv6_ripng_split_horizon_poisoned_reverse,
       ipv6_ripng_split_horizon_poisoned_reverse_cmd,
       "ipv6 ripng split-horizon poisoned-reverse",
       IPV6_STR
       "Routing Information Protocol\n"
       "Perform split horizon\n"
       "With poisoned-reverse\n")
{
  struct interface *ifp;
  struct ripng_interface *ri;

  ifp = vty->index;
  ri = ifp->info;

  ri->split_horizon = RIPNG_SPLIT_HORIZON_POISONED_REVERSE;
  return CMD_SUCCESS;
}

DEFUN (no_ipv6_ripng_split_horizon,
       no_ipv6_ripng_split_horizon_cmd,
       "no ipv6 ripng split-horizon",
       NO_STR
       IPV6_STR
       "Routing Information Protocol\n"
       "Perform split horizon\n")
{
  struct interface *ifp;
  struct ripng_interface *ri;

  ifp = vty->index;
  ri = ifp->info;

  ri->split_horizon = RIPNG_NO_SPLIT_HORIZON;
  return CMD_SUCCESS;
}

ALIAS (no_ipv6_ripng_split_horizon,
       no_ipv6_ripng_split_horizon_poisoned_reverse_cmd,
       "no ipv6 ripng split-horizon poisoned-reverse",
       NO_STR
       IPV6_STR
       "Routing Information Protocol\n"
       "Perform split horizon\n"
       "With poisoned-reverse\n")

DEFUN (ripng_passive_interface,
       ripng_passive_interface_cmd,
       "passive-interface IFNAME",
       "Suppress routing updates on an interface\n"
       "Interface name\n")
{
  return ripng_passive_interface_set (vty, argv[0]);
}

DEFUN (no_ripng_passive_interface,
       no_ripng_passive_interface_cmd,
       "no passive-interface IFNAME",
       NO_STR
       "Suppress routing updates on an interface\n"
       "Interface name\n")
{
  return ripng_passive_interface_unset (vty, argv[0]);
}

static struct ripng_interface *
ri_new (void)
{
  struct ripng_interface *ri;
  ri = XCALLOC (MTYPE_IF, sizeof (struct ripng_interface));

  /* Set default split-horizon behavior.  If the interface is Frame
     Relay or SMDS is enabled, the default value for split-horizon is
     off.  But currently Zebra does detect Frame Relay or SMDS
     interface.  So all interface is set to split horizon.  */
  ri->split_horizon_default = RIPNG_SPLIT_HORIZON;
  ri->split_horizon = ri->split_horizon_default;

  return ri;
}

static int
ripng_if_new_hook (struct interface *ifp)
{
  ifp->info = ri_new ();
  return 0;
}

/* Called when interface structure deleted. */
static int
ripng_if_delete_hook (struct interface *ifp)
{
  XFREE (MTYPE_IF, ifp->info);
  ifp->info = NULL;
  return 0;
}

/* Configuration write function for ripngd. */
static int
interface_config_write (struct vty *vty)
{
  struct listnode *node;
  struct interface *ifp;
  struct ripng_interface *ri;
  int write = 0;

  for (ALL_LIST_ELEMENTS_RO (iflist, node, ifp))
    {
      ri = ifp->info;

      /* Do not display the interface if there is no
       * configuration about it.
       **/
      if ((!ifp->desc) &&
          (ri->split_horizon == ri->split_horizon_default))
        continue;

      vty_out (vty, "interface %s%s", ifp->name,
	       VTY_NEWLINE);
      if (ifp->desc)
	vty_out (vty, " description %s%s", ifp->desc,
		 VTY_NEWLINE);

      /* Split horizon. */
      if (ri->split_horizon != ri->split_horizon_default)
	{
          switch (ri->split_horizon) {
          case RIPNG_SPLIT_HORIZON:
            vty_out (vty, " ipv6 ripng split-horizon%s", VTY_NEWLINE);
            break;
          case RIPNG_SPLIT_HORIZON_POISONED_REVERSE:
            vty_out (vty, " ipv6 ripng split-horizon poisoned-reverse%s",
                          VTY_NEWLINE);
            break;
          case RIPNG_NO_SPLIT_HORIZON:
          default:
            vty_out (vty, " no ipv6 ripng split-horizon%s", VTY_NEWLINE);
            break;
          }
	}

      vty_out (vty, "!%s", VTY_NEWLINE);

      write++;
    }
  return write;
}

/* ripngd's interface node. */
static struct cmd_node interface_node =
{
  INTERFACE_NODE,
  "%s(config-if)# ",
  1 /* VTYSH */
};

/* Initialization of interface. */
void
ripng_if_init ()
{
  /* Interface initialize. */
  iflist = list_new ();
  if_add_hook (IF_NEW_HOOK, ripng_if_new_hook);
  if_add_hook (IF_DELETE_HOOK, ripng_if_delete_hook);

  /* RIPng enable network init. */
  ripng_enable_network = route_table_init ();

  /* RIPng enable interface init. */
  ripng_enable_if = vector_init (1);

  /* RIPng passive interface. */
  Vripng_passive_interface = vector_init (1);

  /* Install interface node. */
  install_node (&interface_node, interface_config_write);
  
  /* Install commands. */
  install_element (CONFIG_NODE, &interface_cmd);
  install_element (CONFIG_NODE, &no_interface_cmd);
  install_default (INTERFACE_NODE);
  install_element (INTERFACE_NODE, &interface_desc_cmd);
  install_element (INTERFACE_NODE, &no_interface_desc_cmd);

  install_element (RIPNG_NODE, &ripng_network_cmd);
  install_element (RIPNG_NODE, &no_ripng_network_cmd);
  install_element (RIPNG_NODE, &ripng_passive_interface_cmd);
  install_element (RIPNG_NODE, &no_ripng_passive_interface_cmd);

  install_element (INTERFACE_NODE, &ipv6_ripng_split_horizon_cmd);
  install_element (INTERFACE_NODE, &ipv6_ripng_split_horizon_poisoned_reverse_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_ripng_split_horizon_cmd);
  install_element (INTERFACE_NODE, &no_ipv6_ripng_split_horizon_poisoned_reverse_cmd);
}
