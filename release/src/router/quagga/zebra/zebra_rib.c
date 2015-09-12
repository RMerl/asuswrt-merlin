/* Routing Information Base.
 * Copyright (C) 1997, 98, 99, 2001 Kunihiro Ishiguro
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

#include "prefix.h"
#include "table.h"
#include "memory.h"
#include "str.h"
#include "command.h"
#include "if.h"
#include "log.h"
#include "sockunion.h"
#include "linklist.h"
#include "thread.h"
#include "workqueue.h"
#include "prefix.h"
#include "routemap.h"

#include "zebra/rib.h"
#include "zebra/rt.h"
#include "zebra/zserv.h"
#include "zebra/redistribute.h"
#include "zebra/debug.h"
#include "zebra/zebra_fpm.h"

/* Default rtm_table for all clients */
extern struct zebra_t zebrad;

/* Hold time for RIB process, should be very minimal.
 * it is useful to able to set it otherwise for testing, hence exported
 * as global here for test-rig code.
 */
int rib_process_hold_time = 10;

/* Each route type's string and default distance value. */
static const struct
{  
  int key;
  int distance;
} route_info[ZEBRA_ROUTE_MAX] =
{
  [ZEBRA_ROUTE_SYSTEM]  = {ZEBRA_ROUTE_SYSTEM,    0},
  [ZEBRA_ROUTE_KERNEL]  = {ZEBRA_ROUTE_KERNEL,    0},
  [ZEBRA_ROUTE_CONNECT] = {ZEBRA_ROUTE_CONNECT,   0},
  [ZEBRA_ROUTE_STATIC]  = {ZEBRA_ROUTE_STATIC,    1},
  [ZEBRA_ROUTE_RIP]     = {ZEBRA_ROUTE_RIP,     120},
  [ZEBRA_ROUTE_RIPNG]   = {ZEBRA_ROUTE_RIPNG,   120},
  [ZEBRA_ROUTE_OSPF]    = {ZEBRA_ROUTE_OSPF,    110},
  [ZEBRA_ROUTE_OSPF6]   = {ZEBRA_ROUTE_OSPF6,   110},
  [ZEBRA_ROUTE_ISIS]    = {ZEBRA_ROUTE_ISIS,    115},
  [ZEBRA_ROUTE_BGP]     = {ZEBRA_ROUTE_BGP,      20  /* IBGP is 200. */},
  [ZEBRA_ROUTE_BABEL]   = {ZEBRA_ROUTE_BABEL,    95},
  /* no entry/default: 150 */
};

/* Vector for routing table.  */
static vector vrf_vector;

/* RPF lookup behaviour */
static enum multicast_mode ipv4_multicast_mode = MCAST_NO_CONFIG;

static void
_rnode_zlog(const char *_func, struct route_node *rn, int priority,
	    const char *msgfmt, ...)
{
  char buf[INET6_ADDRSTRLEN + 4], *bptr;
  char msgbuf[512];
  va_list ap;

  va_start(ap, msgfmt);
  vsnprintf(msgbuf, sizeof(msgbuf), msgfmt, ap);
  va_end(ap);

  if (rn)
    {
      rib_table_info_t *info = rn->table->info;

      inet_ntop (rn->p.family, &rn->p.u.prefix, buf, INET6_ADDRSTRLEN);
      bptr = buf + strlen(buf);
      snprintf(bptr, buf + sizeof(buf) - bptr, "/%d%s", rn->p.prefixlen,
               info->safi == SAFI_MULTICAST ? " (MRIB)" : "");
    }
  else
    {
      snprintf(buf, sizeof(buf), "{(route_node *) NULL}");
    }

  zlog (NULL, priority, "%s: %s: %s", _func, buf, msgbuf);
}

#define rnode_debug(node, ...) \
	_rnode_zlog(__func__, node, LOG_DEBUG, __VA_ARGS__)
#define rnode_info(node, ...) \
	_rnode_zlog(__func__, node, LOG_INFO, __VA_ARGS__)

/*
 * vrf_table_create
 */
static void
vrf_table_create (struct vrf *vrf, afi_t afi, safi_t safi)
{
  rib_table_info_t *info;
  struct route_table *table;

  assert (!vrf->table[afi][safi]);

  table = route_table_init ();
  vrf->table[afi][safi] = table;

  info = XCALLOC (MTYPE_RIB_TABLE_INFO, sizeof (*info));
  info->vrf = vrf;
  info->afi = afi;
  info->safi = safi;
  table->info = info;
}

/* Allocate new VRF.  */
static struct vrf *
vrf_alloc (const char *name)
{
  struct vrf *vrf;

  vrf = XCALLOC (MTYPE_VRF, sizeof (struct vrf));

  /* Put name.  */
  if (name)
    vrf->name = XSTRDUP (MTYPE_VRF_NAME, name);

  /* Allocate routing table and static table.  */
  vrf_table_create (vrf, AFI_IP, SAFI_UNICAST);
  vrf_table_create (vrf, AFI_IP6, SAFI_UNICAST);
  vrf->stable[AFI_IP][SAFI_UNICAST] = route_table_init ();
  vrf->stable[AFI_IP6][SAFI_UNICAST] = route_table_init ();
  vrf_table_create (vrf, AFI_IP, SAFI_MULTICAST);
  vrf_table_create (vrf, AFI_IP6, SAFI_MULTICAST);
  vrf->stable[AFI_IP][SAFI_MULTICAST] = route_table_init ();
  vrf->stable[AFI_IP6][SAFI_MULTICAST] = route_table_init ();


  return vrf;
}

/* Lookup VRF by identifier.  */
struct vrf *
vrf_lookup (u_int32_t id)
{
  return vector_lookup (vrf_vector, id);
}

/* Initialize VRF.  */
static void
vrf_init (void)
{
  struct vrf *default_table;

  /* Allocate VRF vector.  */
  vrf_vector = vector_init (1);

  /* Allocate default main table.  */
  default_table = vrf_alloc ("Default-IP-Routing-Table");

  /* Default table index must be 0.  */
  vector_set_index (vrf_vector, 0, default_table);
}

/* Lookup route table.  */
struct route_table *
vrf_table (afi_t afi, safi_t safi, u_int32_t id)
{
  struct vrf *vrf;

  vrf = vrf_lookup (id);
  if (! vrf)
    return NULL;

  if( afi >= AFI_MAX  || safi >= SAFI_MAX )
    return NULL;

  return vrf->table[afi][safi];
}

/* Lookup static route table.  */
struct route_table *
vrf_static_table (afi_t afi, safi_t safi, u_int32_t id)
{
  struct vrf *vrf;

  vrf = vrf_lookup (id);
  if (! vrf)
    return NULL;

  if( afi >= AFI_MAX  || safi >= SAFI_MAX )
    return NULL;

  return vrf->stable[afi][safi];
}

/*
 * nexthop_type_to_str
 */
const char *
nexthop_type_to_str (enum nexthop_types_t nh_type)
{
  static const char *desc[] = {
    "none",
    "Directly connected",
    "Interface route",
    "IPv4 nexthop",
    "IPv4 nexthop with ifindex",
    "IPv4 nexthop with ifname",
    "IPv6 nexthop",
    "IPv6 nexthop with ifindex",
    "IPv6 nexthop with ifname",
    "Null0 nexthop",
  };

  if (nh_type >= ZEBRA_NUM_OF (desc))
    return "<Invalid nh type>";

  return desc[nh_type];
}

/* Add nexthop to the end of a nexthop list.  */
static void
_nexthop_add (struct nexthop **target, struct nexthop *nexthop)
{
  struct nexthop *last;

  for (last = *target; last && last->next; last = last->next)
    ;
  if (last)
    last->next = nexthop;
  else
    *target = nexthop;
  nexthop->prev = last;
}

/* Add nexthop to the end of a rib node's nexthop list */
static void
nexthop_add (struct rib *rib, struct nexthop *nexthop)
{
  _nexthop_add(&rib->nexthop, nexthop);
  rib->nexthop_num++;
}

/* Delete specified nexthop from the list. */
static void
nexthop_delete (struct rib *rib, struct nexthop *nexthop)
{
  if (nexthop->next)
    nexthop->next->prev = nexthop->prev;
  if (nexthop->prev)
    nexthop->prev->next = nexthop->next;
  else
    rib->nexthop = nexthop->next;
  rib->nexthop_num--;
}

static void nexthops_free(struct nexthop *nexthop);

/* Free nexthop. */
static void
nexthop_free (struct nexthop *nexthop)
{
  if (nexthop->ifname)
    XFREE (0, nexthop->ifname);
  if (nexthop->resolved)
    nexthops_free(nexthop->resolved);
  XFREE (MTYPE_NEXTHOP, nexthop);
}

/* Frees a list of nexthops */
static void
nexthops_free (struct nexthop *nexthop)
{
  struct nexthop *nh, *next;

  for (nh = nexthop; nh; nh = next)
    {
      next = nh->next;
      nexthop_free (nh);
    }
}

struct nexthop *
nexthop_ifindex_add (struct rib *rib, unsigned int ifindex)
{
  struct nexthop *nexthop;

  nexthop = XCALLOC (MTYPE_NEXTHOP, sizeof (struct nexthop));
  nexthop->type = NEXTHOP_TYPE_IFINDEX;
  nexthop->ifindex = ifindex;

  nexthop_add (rib, nexthop);

  return nexthop;
}

struct nexthop *
nexthop_ifname_add (struct rib *rib, char *ifname)
{
  struct nexthop *nexthop;

  nexthop = XCALLOC (MTYPE_NEXTHOP, sizeof (struct nexthop));
  nexthop->type = NEXTHOP_TYPE_IFNAME;
  nexthop->ifname = XSTRDUP (0, ifname);

  nexthop_add (rib, nexthop);

  return nexthop;
}

struct nexthop *
nexthop_ipv4_add (struct rib *rib, struct in_addr *ipv4, struct in_addr *src)
{
  struct nexthop *nexthop;

  nexthop = XCALLOC (MTYPE_NEXTHOP, sizeof (struct nexthop));
  nexthop->type = NEXTHOP_TYPE_IPV4;
  nexthop->gate.ipv4 = *ipv4;
  if (src)
    nexthop->src.ipv4 = *src;

  nexthop_add (rib, nexthop);

  return nexthop;
}

struct nexthop *
nexthop_ipv4_ifindex_add (struct rib *rib, struct in_addr *ipv4, 
                          struct in_addr *src, unsigned int ifindex)
{
  struct nexthop *nexthop;

  nexthop = XCALLOC (MTYPE_NEXTHOP, sizeof (struct nexthop));
  nexthop->type = NEXTHOP_TYPE_IPV4_IFINDEX;
  nexthop->gate.ipv4 = *ipv4;
  if (src)
    nexthop->src.ipv4 = *src;
  nexthop->ifindex = ifindex;

  nexthop_add (rib, nexthop);

  return nexthop;
}

#ifdef HAVE_IPV6
struct nexthop *
nexthop_ipv6_add (struct rib *rib, struct in6_addr *ipv6)
{
  struct nexthop *nexthop;

  nexthop = XCALLOC (MTYPE_NEXTHOP, sizeof (struct nexthop));
  nexthop->type = NEXTHOP_TYPE_IPV6;
  nexthop->gate.ipv6 = *ipv6;

  nexthop_add (rib, nexthop);

  return nexthop;
}

static struct nexthop *
nexthop_ipv6_ifname_add (struct rib *rib, struct in6_addr *ipv6,
			 char *ifname)
{
  struct nexthop *nexthop;

  nexthop = XCALLOC (MTYPE_NEXTHOP, sizeof (struct nexthop));
  nexthop->type = NEXTHOP_TYPE_IPV6_IFNAME;
  nexthop->gate.ipv6 = *ipv6;
  nexthop->ifname = XSTRDUP (0, ifname);

  nexthop_add (rib, nexthop);

  return nexthop;
}

static struct nexthop *
nexthop_ipv6_ifindex_add (struct rib *rib, struct in6_addr *ipv6,
			  unsigned int ifindex)
{
  struct nexthop *nexthop;

  nexthop = XCALLOC (MTYPE_NEXTHOP, sizeof (struct nexthop));
  nexthop->type = NEXTHOP_TYPE_IPV6_IFINDEX;
  nexthop->gate.ipv6 = *ipv6;
  nexthop->ifindex = ifindex;

  nexthop_add (rib, nexthop);

  return nexthop;
}
#endif /* HAVE_IPV6 */

struct nexthop *
nexthop_blackhole_add (struct rib *rib)
{
  struct nexthop *nexthop;

  nexthop = XCALLOC (MTYPE_NEXTHOP, sizeof (struct nexthop));
  nexthop->type = NEXTHOP_TYPE_BLACKHOLE;
  SET_FLAG (rib->flags, ZEBRA_FLAG_BLACKHOLE);

  nexthop_add (rib, nexthop);

  return nexthop;
}

/* This method checks whether a recursive nexthop has at
 * least one resolved nexthop in the fib.
 */
int
nexthop_has_fib_child(struct nexthop *nexthop)
{
  struct nexthop *nh;

  if (! CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE))
    return 0;

  for (nh = nexthop->resolved; nh; nh = nh->next)
    if (CHECK_FLAG (nh->flags, NEXTHOP_FLAG_FIB))
      return 1;

  return 0;
}

/* If force flag is not set, do not modify falgs at all for uninstall
   the route from FIB. */
static int
nexthop_active_ipv4 (struct rib *rib, struct nexthop *nexthop, int set,
		     struct route_node *top)
{
  struct prefix_ipv4 p;
  struct route_table *table;
  struct route_node *rn;
  struct rib *match;
  int resolved;
  struct nexthop *newhop;
  struct nexthop *resolved_hop;

  if (nexthop->type == NEXTHOP_TYPE_IPV4)
    nexthop->ifindex = 0;

  if (set)
    {
      UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE);
      nexthops_free(nexthop->resolved);
      nexthop->resolved = NULL;
    }

  /* Make lookup prefix. */
  memset (&p, 0, sizeof (struct prefix_ipv4));
  p.family = AF_INET;
  p.prefixlen = IPV4_MAX_PREFIXLEN;
  p.prefix = nexthop->gate.ipv4;

  /* Lookup table.  */
  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return 0;

  rn = route_node_match (table, (struct prefix *) &p);
  while (rn)
    {
      route_unlock_node (rn);
      
      /* If lookup self prefix return immediately. */
      if (rn == top)
	return 0;

      /* Pick up selected route. */
      RNODE_FOREACH_RIB (rn, match)
	{
	  if (CHECK_FLAG (match->status, RIB_ENTRY_REMOVED))
	    continue;
	  if (CHECK_FLAG (match->flags, ZEBRA_FLAG_SELECTED))
	    break;
	}

      /* If there is no selected route or matched route is EGP, go up
         tree. */
      if (! match 
	  || match->type == ZEBRA_ROUTE_BGP)
	{
	  do {
	    rn = rn->parent;
	  } while (rn && rn->info == NULL);
	  if (rn)
	    route_lock_node (rn);
	}
      else
	{
	  /* If the longest prefix match for the nexthop yields
	   * a blackhole, mark it as inactive. */
	  if (CHECK_FLAG (match->flags, ZEBRA_FLAG_BLACKHOLE)
	      || CHECK_FLAG (match->flags, ZEBRA_FLAG_REJECT))
	    return 0;

	  if (match->type == ZEBRA_ROUTE_CONNECT)
	    {
	      /* Directly point connected route. */
	      newhop = match->nexthop;
	      if (newhop && nexthop->type == NEXTHOP_TYPE_IPV4)
		nexthop->ifindex = newhop->ifindex;
	      
	      return 1;
	    }
	  else if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_INTERNAL))
	    {
	      resolved = 0;
	      for (newhop = match->nexthop; newhop; newhop = newhop->next)
		if (CHECK_FLAG (newhop->flags, NEXTHOP_FLAG_FIB)
		    && ! CHECK_FLAG (newhop->flags, NEXTHOP_FLAG_RECURSIVE))
		  {
		    if (set)
		      {
			SET_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE);

			resolved_hop = XCALLOC(MTYPE_NEXTHOP, sizeof (struct nexthop));
			SET_FLAG (resolved_hop->flags, NEXTHOP_FLAG_ACTIVE);
			/* If the resolving route specifies a gateway, use it */
			if (newhop->type == NEXTHOP_TYPE_IPV4
			    || newhop->type == NEXTHOP_TYPE_IPV4_IFINDEX
			    || newhop->type == NEXTHOP_TYPE_IPV4_IFNAME)
			  {
			    resolved_hop->type = newhop->type;
			    resolved_hop->gate.ipv4 = newhop->gate.ipv4;

			    if (newhop->ifindex)
			      {
				resolved_hop->type = NEXTHOP_TYPE_IPV4_IFINDEX;
				resolved_hop->ifindex = newhop->ifindex;
			      }
			  }

			/* If the resolving route is an interface route,
			 * it means the gateway we are looking up is connected
			 * to that interface. (The actual network is _not_ onlink).
			 * Therefore, the resolved route should have the original
			 * gateway as nexthop as it is directly connected.
			 *
			 * On Linux, we have to set the onlink netlink flag because
			 * otherwise, the kernel won't accept the route. */
			if (newhop->type == NEXTHOP_TYPE_IFINDEX
			    || newhop->type == NEXTHOP_TYPE_IFNAME)
			  {
			    resolved_hop->flags |= NEXTHOP_FLAG_ONLINK;
			    resolved_hop->type = NEXTHOP_TYPE_IPV4_IFINDEX;
			    resolved_hop->gate.ipv4 = nexthop->gate.ipv4;
			    resolved_hop->ifindex = newhop->ifindex;
			  }

			_nexthop_add(&nexthop->resolved, resolved_hop);
		      }
		    resolved = 1;
		  }
	      return resolved;
	    }
	  else
	    {
	      return 0;
	    }
	}
    }
  return 0;
}

#ifdef HAVE_IPV6
/* If force flag is not set, do not modify falgs at all for uninstall
   the route from FIB. */
static int
nexthop_active_ipv6 (struct rib *rib, struct nexthop *nexthop, int set,
		     struct route_node *top)
{
  struct prefix_ipv6 p;
  struct route_table *table;
  struct route_node *rn;
  struct rib *match;
  int resolved;
  struct nexthop *newhop;
  struct nexthop *resolved_hop;

  if (nexthop->type == NEXTHOP_TYPE_IPV6)
    nexthop->ifindex = 0;

  if (set)
    {
      UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE);
      nexthops_free(nexthop->resolved);
      nexthop->resolved = NULL;
    }

  /* Make lookup prefix. */
  memset (&p, 0, sizeof (struct prefix_ipv6));
  p.family = AF_INET6;
  p.prefixlen = IPV6_MAX_PREFIXLEN;
  p.prefix = nexthop->gate.ipv6;

  /* Lookup table.  */
  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (! table)
    return 0;

  rn = route_node_match (table, (struct prefix *) &p);
  while (rn)
    {
      route_unlock_node (rn);
      
      /* If lookup self prefix return immediately. */
      if (rn == top)
	return 0;

      /* Pick up selected route. */
      RNODE_FOREACH_RIB (rn, match)
	{
	  if (CHECK_FLAG (match->status, RIB_ENTRY_REMOVED))
	    continue;
	  if (CHECK_FLAG (match->flags, ZEBRA_FLAG_SELECTED))
	    break;
	}

      /* If there is no selected route or matched route is EGP, go up
         tree. */
      if (! match
	  || match->type == ZEBRA_ROUTE_BGP)
	{
	  do {
	    rn = rn->parent;
	  } while (rn && rn->info == NULL);
	  if (rn)
	    route_lock_node (rn);
	}
      else
	{
	  /* If the longest prefix match for the nexthop yields
	   * a blackhole, mark it as inactive. */
	  if (CHECK_FLAG (match->flags, ZEBRA_FLAG_BLACKHOLE)
	      || CHECK_FLAG (match->flags, ZEBRA_FLAG_REJECT))
	    return 0;

	  if (match->type == ZEBRA_ROUTE_CONNECT)
	    {
	      /* Directly point connected route. */
	      newhop = match->nexthop;

	      if (newhop && nexthop->type == NEXTHOP_TYPE_IPV6)
		nexthop->ifindex = newhop->ifindex;
	      
	      return 1;
	    }
	  else if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_INTERNAL))
	    {
	      resolved = 0;
	      for (newhop = match->nexthop; newhop; newhop = newhop->next)
		if (CHECK_FLAG (newhop->flags, NEXTHOP_FLAG_FIB)
		    && ! CHECK_FLAG (newhop->flags, NEXTHOP_FLAG_RECURSIVE))
		  {
		    if (set)
		      {
			SET_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE);

			resolved_hop = XCALLOC(MTYPE_NEXTHOP, sizeof (struct nexthop));
			SET_FLAG (resolved_hop->flags, NEXTHOP_FLAG_ACTIVE);
			/* See nexthop_active_ipv4 for a description how the
			 * resolved nexthop is constructed. */
			if (newhop->type == NEXTHOP_TYPE_IPV6
			    || newhop->type == NEXTHOP_TYPE_IPV6_IFINDEX
			    || newhop->type == NEXTHOP_TYPE_IPV6_IFNAME)
			  {
			    resolved_hop->type = newhop->type;
			    resolved_hop->gate.ipv6 = newhop->gate.ipv6;

			    if (newhop->ifindex)
			      {
				resolved_hop->type = NEXTHOP_TYPE_IPV6_IFINDEX;
				resolved_hop->ifindex = newhop->ifindex;
			      }
			  }

			if (newhop->type == NEXTHOP_TYPE_IFINDEX
			    || newhop->type == NEXTHOP_TYPE_IFNAME)
			  {
				resolved_hop->flags |= NEXTHOP_FLAG_ONLINK;
				resolved_hop->type = NEXTHOP_TYPE_IPV6_IFINDEX;
				resolved_hop->gate.ipv6 = nexthop->gate.ipv6;
				resolved_hop->ifindex = newhop->ifindex;
			  }

			_nexthop_add(&nexthop->resolved, resolved_hop);
		      }
		    resolved = 1;
		  }
	      return resolved;
	    }
	  else
	    {
	      return 0;
	    }
	}
    }
  return 0;
}
#endif /* HAVE_IPV6 */

struct rib *
rib_match_ipv4_safi (struct in_addr addr, safi_t safi, int skip_bgp,
		     struct route_node **rn_out)
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *match;
  struct nexthop *newhop, *tnewhop;
  int recursing;

  /* Lookup table.  */
  table = vrf_table (AFI_IP, safi, 0);
  if (! table)
    return 0;

  rn = route_node_match_ipv4 (table, &addr);

  while (rn)
    {
      route_unlock_node (rn);

      /* Pick up selected route. */
      RNODE_FOREACH_RIB (rn, match)
	{
	  if (CHECK_FLAG (match->status, RIB_ENTRY_REMOVED))
	    continue;
	  if (CHECK_FLAG (match->flags, ZEBRA_FLAG_SELECTED))
	    break;
	}

      /* If there is no selected route or matched route is EGP, go up
         tree. */
      if (!match || (skip_bgp && (match->type == ZEBRA_ROUTE_BGP)))
	{
	  do {
	    rn = rn->parent;
	  } while (rn && rn->info == NULL);
	  if (rn)
	    route_lock_node (rn);
	}
      else
	{
	  if (match->type != ZEBRA_ROUTE_CONNECT)
	    {
	      int found = 0;
	      for (ALL_NEXTHOPS_RO(match->nexthop, newhop, tnewhop, recursing))
		if (CHECK_FLAG (newhop->flags, NEXTHOP_FLAG_FIB))
		  {
		    found = 1;
		    break;
		  }
	      if (!found)
		return NULL;
	    }

	  if (rn_out)
	    *rn_out = rn;
	  return match;
	}
    }
  return NULL;
}

struct rib *
rib_match_ipv4_multicast (struct in_addr addr, struct route_node **rn_out)
{
  struct rib *rib = NULL, *mrib = NULL, *urib = NULL;
  struct route_node *m_rn = NULL, *u_rn = NULL;
  int skip_bgp = 0; /* bool */

  switch (ipv4_multicast_mode)
    {
    case MCAST_MRIB_ONLY:
      return rib_match_ipv4_safi (addr, SAFI_MULTICAST, skip_bgp, rn_out);
    case MCAST_URIB_ONLY:
      return rib_match_ipv4_safi (addr, SAFI_UNICAST, skip_bgp, rn_out);
    case MCAST_NO_CONFIG:
    case MCAST_MIX_MRIB_FIRST:
      rib = mrib = rib_match_ipv4_safi (addr, SAFI_MULTICAST, skip_bgp, &m_rn);
      if (!mrib)
	rib = urib = rib_match_ipv4_safi (addr, SAFI_UNICAST, skip_bgp, &u_rn);
      break;
    case MCAST_MIX_DISTANCE:
      mrib = rib_match_ipv4_safi (addr, SAFI_MULTICAST, skip_bgp, &m_rn);
      urib = rib_match_ipv4_safi (addr, SAFI_UNICAST, skip_bgp, &u_rn);
      if (mrib && urib)
	rib = urib->distance < mrib->distance ? urib : mrib;
      else if (mrib)
	rib = mrib;
      else if (urib)
	rib = urib;
      break;
    case MCAST_MIX_PFXLEN:
      mrib = rib_match_ipv4_safi (addr, SAFI_MULTICAST, skip_bgp, &m_rn);
      urib = rib_match_ipv4_safi (addr, SAFI_UNICAST, skip_bgp, &u_rn);
      if (mrib && urib)
	rib = u_rn->p.prefixlen > m_rn->p.prefixlen ? urib : mrib;
      else if (mrib)
	rib = mrib;
      else if (urib)
	rib = urib;
      break;
  }

  if (rn_out)
    *rn_out = (rib == mrib) ? m_rn : u_rn;

  if (IS_ZEBRA_DEBUG_RIB)
    {
      char buf[BUFSIZ];
      inet_ntop (AF_INET, &addr, buf, BUFSIZ);

      zlog_debug("%s: %s: found %s, using %s",
		 __func__, buf,
                 mrib ? (urib ? "MRIB+URIB" : "MRIB") :
                         urib ? "URIB" : "nothing",
		 rib == urib ? "URIB" : rib == mrib ? "MRIB" : "none");
    }
  return rib;
}

void
multicast_mode_ipv4_set (enum multicast_mode mode)
{
  if (IS_ZEBRA_DEBUG_RIB)
    zlog_debug("%s: multicast lookup mode set (%d)", __func__, mode);
  ipv4_multicast_mode = mode;
}

enum multicast_mode
multicast_mode_ipv4_get (void)
{
  return ipv4_multicast_mode;
}

struct rib *
rib_lookup_ipv4 (struct prefix_ipv4 *p)
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *match;
  struct nexthop *nexthop, *tnexthop;
  int recursing;

  /* Lookup table.  */
  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return 0;

  rn = route_node_lookup (table, (struct prefix *) p);

  /* No route for this prefix. */
  if (! rn)
    return NULL;

  /* Unlock node. */
  route_unlock_node (rn);

  RNODE_FOREACH_RIB (rn, match)
    {
      if (CHECK_FLAG (match->status, RIB_ENTRY_REMOVED))
	continue;
      if (CHECK_FLAG (match->flags, ZEBRA_FLAG_SELECTED))
	break;
    }

  if (! match || match->type == ZEBRA_ROUTE_BGP)
    return NULL;

  if (match->type == ZEBRA_ROUTE_CONNECT)
    return match;
  
  for (ALL_NEXTHOPS_RO(match->nexthop, nexthop, tnexthop, recursing))
    if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB))
      return match;

  return NULL;
}

/*
 * This clone function, unlike its original rib_lookup_ipv4(), checks
 * if specified IPv4 route record (prefix/mask -> gate) exists in
 * the whole RIB and has ZEBRA_FLAG_SELECTED set.
 *
 * Return values:
 * -1: error
 * 0: exact match found
 * 1: a match was found with a different gate
 * 2: connected route found
 * 3: no matches found
 */
int
rib_lookup_ipv4_route (struct prefix_ipv4 *p, union sockunion * qgate)
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *match;
  struct nexthop *nexthop, *tnexthop;
  int recursing;
  int nexthops_active;

  /* Lookup table.  */
  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return ZEBRA_RIB_LOOKUP_ERROR;

  /* Scan the RIB table for exactly matching RIB entry. */
  rn = route_node_lookup (table, (struct prefix *) p);

  /* No route for this prefix. */
  if (! rn)
    return ZEBRA_RIB_NOTFOUND;

  /* Unlock node. */
  route_unlock_node (rn);

  /* Find out if a "selected" RR for the discovered RIB entry exists ever. */
  RNODE_FOREACH_RIB (rn, match)
    {
      if (CHECK_FLAG (match->status, RIB_ENTRY_REMOVED))
	continue;
      if (CHECK_FLAG (match->flags, ZEBRA_FLAG_SELECTED))
	break;
    }

  /* None such found :( */
  if (!match)
    return ZEBRA_RIB_NOTFOUND;

  if (match->type == ZEBRA_ROUTE_CONNECT)
    return ZEBRA_RIB_FOUND_CONNECTED;
  
  /* Ok, we have a cood candidate, let's check it's nexthop list... */
  nexthops_active = 0;
  for (ALL_NEXTHOPS_RO(match->nexthop, nexthop, tnexthop, recursing))
    if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB))
      {
        nexthops_active = 1;
        if (nexthop->gate.ipv4.s_addr == sockunion2ip (qgate))
          return ZEBRA_RIB_FOUND_EXACT;
        if (IS_ZEBRA_DEBUG_RIB)
          {
            char gate_buf[INET_ADDRSTRLEN], qgate_buf[INET_ADDRSTRLEN];
            inet_ntop (AF_INET, &nexthop->gate.ipv4.s_addr, gate_buf, INET_ADDRSTRLEN);
            inet_ntop (AF_INET, &sockunion2ip(qgate), qgate_buf, INET_ADDRSTRLEN);
            zlog_debug ("%s: qgate == %s, %s == %s", __func__,
                        qgate_buf, recursing ? "rgate" : "gate", gate_buf);
          }
      }

  if (nexthops_active)
    return ZEBRA_RIB_FOUND_NOGATE;

  return ZEBRA_RIB_NOTFOUND;
}

#ifdef HAVE_IPV6
struct rib *
rib_match_ipv6 (struct in6_addr *addr)
{
  struct prefix_ipv6 p;
  struct route_table *table;
  struct route_node *rn;
  struct rib *match;
  struct nexthop *newhop, *tnewhop;
  int recursing;

  /* Lookup table.  */
  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (! table)
    return 0;

  memset (&p, 0, sizeof (struct prefix_ipv6));
  p.family = AF_INET6;
  p.prefixlen = IPV6_MAX_PREFIXLEN;
  IPV6_ADDR_COPY (&p.prefix, addr);

  rn = route_node_match (table, (struct prefix *) &p);

  while (rn)
    {
      route_unlock_node (rn);
      
      /* Pick up selected route. */
      RNODE_FOREACH_RIB (rn, match)
	{
	  if (CHECK_FLAG (match->status, RIB_ENTRY_REMOVED))
	    continue;
	  if (CHECK_FLAG (match->flags, ZEBRA_FLAG_SELECTED))
	    break;
	}

      /* If there is no selected route or matched route is EGP, go up
         tree. */
      if (! match 
	  || match->type == ZEBRA_ROUTE_BGP)
	{
	  do {
	    rn = rn->parent;
	  } while (rn && rn->info == NULL);
	  if (rn)
	    route_lock_node (rn);
	}
      else
	{
	  if (match->type == ZEBRA_ROUTE_CONNECT)
	    /* Directly point connected route. */
	    return match;
	  else
	    {
	      for (ALL_NEXTHOPS_RO(match->nexthop, newhop, tnewhop, recursing))
		if (CHECK_FLAG (newhop->flags, NEXTHOP_FLAG_FIB))
		  return match;
	      return NULL;
	    }
	}
    }
  return NULL;
}
#endif /* HAVE_IPV6 */

#define RIB_SYSTEM_ROUTE(R) \
        ((R)->type == ZEBRA_ROUTE_KERNEL || (R)->type == ZEBRA_ROUTE_CONNECT)

/* This function verifies reachability of one given nexthop, which can be
 * numbered or unnumbered, IPv4 or IPv6. The result is unconditionally stored
 * in nexthop->flags field. If the 4th parameter, 'set', is non-zero,
 * nexthop->ifindex will be updated appropriately as well.
 * An existing route map can turn (otherwise active) nexthop into inactive, but
 * not vice versa.
 *
 * The return value is the final value of 'ACTIVE' flag.
 */

static unsigned
nexthop_active_check (struct route_node *rn, struct rib *rib,
		      struct nexthop *nexthop, int set)
{
  rib_table_info_t *info = rn->table->info;
  struct interface *ifp;
  route_map_result_t ret = RMAP_MATCH;
  extern char *proto_rm[AFI_MAX][ZEBRA_ROUTE_MAX+1];
  struct route_map *rmap;
  int family;

  family = 0;
  switch (nexthop->type)
    {
    case NEXTHOP_TYPE_IFINDEX:
      ifp = if_lookup_by_index (nexthop->ifindex);
      if (ifp && if_is_operative(ifp))
	SET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
      else
	UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
      break;
    case NEXTHOP_TYPE_IPV6_IFNAME:
      family = AFI_IP6;
    case NEXTHOP_TYPE_IFNAME:
      ifp = if_lookup_by_name (nexthop->ifname);
      if (ifp && if_is_operative(ifp))
	{
	  if (set)
	    nexthop->ifindex = ifp->ifindex;
	  SET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
	}
      else
	{
	  if (set)
	    nexthop->ifindex = 0;
	  UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
	}
      break;
    case NEXTHOP_TYPE_IPV4:
    case NEXTHOP_TYPE_IPV4_IFINDEX:
      family = AFI_IP;
      if (nexthop_active_ipv4 (rib, nexthop, set, rn))
	SET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
      else
	UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
      break;
#ifdef HAVE_IPV6
    case NEXTHOP_TYPE_IPV6:
      family = AFI_IP6;
      if (nexthop_active_ipv6 (rib, nexthop, set, rn))
	SET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
      else
	UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
      break;
    case NEXTHOP_TYPE_IPV6_IFINDEX:
      family = AFI_IP6;
      if (IN6_IS_ADDR_LINKLOCAL (&nexthop->gate.ipv6))
	{
	  ifp = if_lookup_by_index (nexthop->ifindex);
	  if (ifp && if_is_operative(ifp))
	    SET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
	  else
	    UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
	}
      else
	{
	  if (nexthop_active_ipv6 (rib, nexthop, set, rn))
	    SET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
	  else
	    UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
	}
      break;
#endif /* HAVE_IPV6 */
    case NEXTHOP_TYPE_BLACKHOLE:
      SET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
      break;
    default:
      break;
    }
  if (! CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE))
    return 0;

  /* XXX: What exactly do those checks do? Do we support
   * e.g. IPv4 routes with IPv6 nexthops or vice versa? */
  if (RIB_SYSTEM_ROUTE(rib) ||
      (family == AFI_IP && rn->p.family != AF_INET) ||
      (family == AFI_IP6 && rn->p.family != AF_INET6))
    return CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);

  /* The original code didn't determine the family correctly
   * e.g. for NEXTHOP_TYPE_IFINDEX. Retrieve the correct afi
   * from the rib_table_info in those cases.
   * Possibly it may be better to use only the rib_table_info
   * in every case.
   */
  if (!family)
    family = info->afi;

  rmap = 0;
  if (rib->type >= 0 && rib->type < ZEBRA_ROUTE_MAX &&
        	proto_rm[family][rib->type])
    rmap = route_map_lookup_by_name (proto_rm[family][rib->type]);
  if (!rmap && proto_rm[family][ZEBRA_ROUTE_MAX])
    rmap = route_map_lookup_by_name (proto_rm[family][ZEBRA_ROUTE_MAX]);
  if (rmap) {
      ret = route_map_apply(rmap, &rn->p, RMAP_ZEBRA, nexthop);
  }

  if (ret == RMAP_DENYMATCH)
    UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
  return CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
}

/* Iterate over all nexthops of the given RIB entry and refresh their
 * ACTIVE flag. rib->nexthop_active_num is updated accordingly. If any
 * nexthop is found to toggle the ACTIVE flag, the whole rib structure
 * is flagged with ZEBRA_FLAG_CHANGED. The 4th 'set' argument is
 * transparently passed to nexthop_active_check().
 *
 * Return value is the new number of active nexthops.
 */

static int
nexthop_active_update (struct route_node *rn, struct rib *rib, int set)
{
  struct nexthop *nexthop;
  unsigned int prev_active, prev_index, new_active;

  rib->nexthop_active_num = 0;
  UNSET_FLAG (rib->flags, ZEBRA_FLAG_CHANGED);

  for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
  {
    prev_active = CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
    prev_index = nexthop->ifindex;
    if ((new_active = nexthop_active_check (rn, rib, nexthop, set)))
      rib->nexthop_active_num++;
    if (prev_active != new_active ||
	prev_index != nexthop->ifindex)
      SET_FLAG (rib->flags, ZEBRA_FLAG_CHANGED);
  }
  return rib->nexthop_active_num;
}



static void
rib_install_kernel (struct route_node *rn, struct rib *rib)
{
  int ret = 0;
  struct nexthop *nexthop, *tnexthop;
  rib_table_info_t *info = rn->table->info;
  int recursing;

  if (info->safi != SAFI_UNICAST)
    {
      for (ALL_NEXTHOPS_RO(rib->nexthop, nexthop, tnexthop, recursing))
        SET_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB);
      return;
    }

  /*
   * Make sure we update the FPM any time we send new information to
   * the kernel.
   */
  zfpm_trigger_update (rn, "installing in kernel");
  switch (PREFIX_FAMILY (&rn->p))
    {
    case AF_INET:
      ret = kernel_add_ipv4 (&rn->p, rib);
      break;
#ifdef HAVE_IPV6
    case AF_INET6:
      ret = kernel_add_ipv6 (&rn->p, rib);
      break;
#endif /* HAVE_IPV6 */
    }

  /* This condition is never met, if we are using rt_socket.c */
  if (ret < 0)
    {
      for (ALL_NEXTHOPS_RO(rib->nexthop, nexthop, tnexthop, recursing))
	UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB);
    }
}

/* Uninstall the route from kernel. */
static int
rib_uninstall_kernel (struct route_node *rn, struct rib *rib)
{
  int ret = 0;
  struct nexthop *nexthop, *tnexthop;
  rib_table_info_t *info = rn->table->info;
  int recursing;

  if (info->safi != SAFI_UNICAST)
    {
      for (ALL_NEXTHOPS_RO(rib->nexthop, nexthop, tnexthop, recursing))
        SET_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB);
      return ret;
    }

  /*
   * Make sure we update the FPM any time we send new information to
   * the kernel.
   */
  zfpm_trigger_update (rn, "uninstalling from kernel");

  switch (PREFIX_FAMILY (&rn->p))
    {
    case AF_INET:
      ret = kernel_delete_ipv4 (&rn->p, rib);
      break;
#ifdef HAVE_IPV6
    case AF_INET6:
      ret = kernel_delete_ipv6 (&rn->p, rib);
      break;
#endif /* HAVE_IPV6 */
    }

  for (ALL_NEXTHOPS_RO(rib->nexthop, nexthop, tnexthop, recursing))
    UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB);

  return ret;
}

/* Uninstall the route from kernel. */
static void
rib_uninstall (struct route_node *rn, struct rib *rib)
{
  rib_table_info_t *info = rn->table->info;

  if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELECTED))
    {
      if (info->safi == SAFI_UNICAST)
        zfpm_trigger_update (rn, "rib_uninstall");

      redistribute_delete (&rn->p, rib);
      if (! RIB_SYSTEM_ROUTE (rib))
	rib_uninstall_kernel (rn, rib);
      UNSET_FLAG (rib->flags, ZEBRA_FLAG_SELECTED);
    }
}

static void rib_unlink (struct route_node *, struct rib *);

/*
 * rib_can_delete_dest
 *
 * Returns TRUE if the given dest can be deleted from the table.
 */
static int
rib_can_delete_dest (rib_dest_t *dest)
{
  if (dest->routes)
    {
      return 0;
    }

  /*
   * Don't delete the dest if we have to update the FPM about this
   * prefix.
   */
  if (CHECK_FLAG (dest->flags, RIB_DEST_UPDATE_FPM) ||
      CHECK_FLAG (dest->flags, RIB_DEST_SENT_TO_FPM))
    return 0;

  return 1;
}

/*
 * rib_gc_dest
 *
 * Garbage collect the rib dest corresponding to the given route node
 * if appropriate.
 *
 * Returns TRUE if the dest was deleted, FALSE otherwise.
 */
int
rib_gc_dest (struct route_node *rn)
{
  rib_dest_t *dest;

  dest = rib_dest_from_rnode (rn);
  if (!dest)
    return 0;

  if (!rib_can_delete_dest (dest))
    return 0;

  if (IS_ZEBRA_DEBUG_RIB)
    rnode_debug (rn, "removing dest from table");

  dest->rnode = NULL;
  XFREE (MTYPE_RIB_DEST, dest);
  rn->info = NULL;

  /*
   * Release the one reference that we keep on the route node.
   */
  route_unlock_node (rn);
  return 1;
}

/* Core function for processing routing information base. */
static void
rib_process (struct route_node *rn)
{
  struct rib *rib;
  struct rib *next;
  struct rib *fib = NULL;
  struct rib *select = NULL;
  struct rib *del = NULL;
  int installed = 0;
  struct nexthop *nexthop = NULL, *tnexthop;
  int recursing;
  rib_table_info_t *info;

  assert (rn);

  info = rn->table->info;

  RNODE_FOREACH_RIB_SAFE (rn, rib, next)
    {
      /* Currently installed rib. */
      if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELECTED))
        {
          assert (fib == NULL);
          fib = rib;
        }
      
      /* Unlock removed routes, so they'll be freed, bar the FIB entry,
       * which we need to do do further work with below.
       */
      if (CHECK_FLAG (rib->status, RIB_ENTRY_REMOVED))
        {
          if (rib != fib)
            {
              if (IS_ZEBRA_DEBUG_RIB)
		rnode_debug (rn, "rn %p, removing rib %p", rn, rib);
	      rib_unlink (rn, rib);
            }
          else
            del = rib;
          
          continue;
        }
      
      /* Skip unreachable nexthop. */
      if (! nexthop_active_update (rn, rib, 0))
        continue;

      /* Infinit distance. */
      if (rib->distance == DISTANCE_INFINITY)
        continue;

      /* Newly selected rib, the common case. */
      if (!select)
        {
          select = rib;
          continue;
        }
      
      /* filter route selection in following order:
       * - connected beats other types
       * - lower distance beats higher
       * - lower metric beats higher for equal distance
       * - last, hence oldest, route wins tie break.
       */
      
      /* Connected routes. Pick the last connected
       * route of the set of lowest metric connected routes.
       */
      if (rib->type == ZEBRA_ROUTE_CONNECT)
        {
          if (select->type != ZEBRA_ROUTE_CONNECT
              || rib->metric <= select->metric)
            select = rib;
          continue;
        }
      else if (select->type == ZEBRA_ROUTE_CONNECT)
        continue;
      
      /* higher distance loses */
      if (rib->distance > select->distance)
        continue;
      
      /* lower wins */
      if (rib->distance < select->distance)
        {
          select = rib;
          continue;
        }
      
      /* metric tie-breaks equal distance */
      if (rib->metric <= select->metric)
        select = rib;
    } /* RNODE_FOREACH_RIB_SAFE */

  /* After the cycle is finished, the following pointers will be set:
   * select --- the winner RIB entry, if any was found, otherwise NULL
   * fib    --- the SELECTED RIB entry, if any, otherwise NULL
   * del    --- equal to fib, if fib is queued for deletion, NULL otherwise
   * rib    --- NULL
   */

  /* Same RIB entry is selected. Update FIB and finish. */
  if (select && select == fib)
    {
      if (IS_ZEBRA_DEBUG_RIB)
	rnode_debug (rn, "Updating existing route, select %p, fib %p",
		     select, fib);
      if (CHECK_FLAG (select->flags, ZEBRA_FLAG_CHANGED))
        {
          if (info->safi == SAFI_UNICAST)
	    zfpm_trigger_update (rn, "updating existing route");

          redistribute_delete (&rn->p, select);
          if (! RIB_SYSTEM_ROUTE (select))
            rib_uninstall_kernel (rn, select);

          /* Set real nexthop. */
          nexthop_active_update (rn, select, 1);
  
          if (! RIB_SYSTEM_ROUTE (select))
            rib_install_kernel (rn, select);
          redistribute_add (&rn->p, select);
        }
      else if (! RIB_SYSTEM_ROUTE (select))
        {
          /* Housekeeping code to deal with 
             race conditions in kernel with linux
             netlink reporting interface up before IPv4 or IPv6 protocol
             is ready to add routes.
             This makes sure the routes are IN the kernel.
           */

          for (ALL_NEXTHOPS_RO(select->nexthop, nexthop, tnexthop, recursing))
            if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB))
            {
              installed = 1;
              break;
            }
          if (! installed) 
            rib_install_kernel (rn, select);
        }
      goto end;
    }

  /* At this point we either haven't found the best RIB entry or it is
   * different from what we currently intend to flag with SELECTED. In both
   * cases, if a RIB block is present in FIB, it should be withdrawn.
   */
  if (fib)
    {
      if (IS_ZEBRA_DEBUG_RIB)
	rnode_debug (rn, "Removing existing route, fib %p", fib);

      if (info->safi == SAFI_UNICAST)
        zfpm_trigger_update (rn, "removing existing route");

      redistribute_delete (&rn->p, fib);
      if (! RIB_SYSTEM_ROUTE (fib))
	rib_uninstall_kernel (rn, fib);
      UNSET_FLAG (fib->flags, ZEBRA_FLAG_SELECTED);

      /* Set real nexthop. */
      nexthop_active_update (rn, fib, 1);
    }

  /* Regardless of some RIB entry being SELECTED or not before, now we can
   * tell, that if a new winner exists, FIB is still not updated with this
   * data, but ready to be.
   */
  if (select)
    {
      if (IS_ZEBRA_DEBUG_RIB)
	rnode_debug (rn, "Adding route, select %p", select);

      if (info->safi == SAFI_UNICAST)
        zfpm_trigger_update (rn, "new route selected");

      /* Set real nexthop. */
      nexthop_active_update (rn, select, 1);

      if (! RIB_SYSTEM_ROUTE (select))
        rib_install_kernel (rn, select);
      SET_FLAG (select->flags, ZEBRA_FLAG_SELECTED);
      redistribute_add (&rn->p, select);
    }

  /* FIB route was removed, should be deleted */
  if (del)
    {
      if (IS_ZEBRA_DEBUG_RIB)
        rnode_debug (rn, "Deleting fib %p, rn %p", del, rn);
      rib_unlink (rn, del);
    }

end:
  if (IS_ZEBRA_DEBUG_RIB_Q)
    rnode_debug (rn, "rn %p dequeued", rn);

  /*
   * Check if the dest can be deleted now.
   */
  rib_gc_dest (rn);
}

/* Take a list of route_node structs and return 1, if there was a record
 * picked from it and processed by rib_process(). Don't process more, 
 * than one RN record; operate only in the specified sub-queue.
 */
static unsigned int
process_subq (struct list * subq, u_char qindex)
{
  struct listnode *lnode  = listhead (subq);
  struct route_node *rnode;

  if (!lnode)
    return 0;

  rnode = listgetdata (lnode);
  rib_process (rnode);

  if (rnode->info)
    UNSET_FLAG (rib_dest_from_rnode (rnode)->flags, RIB_ROUTE_QUEUED (qindex));

#if 0
  else
    {
      zlog_debug ("%s: called for route_node (%p, %d) with no ribs",
                  __func__, rnode, rnode->lock);
      zlog_backtrace(LOG_DEBUG);
    }
#endif
  route_unlock_node (rnode);
  list_delete_node (subq, lnode);
  return 1;
}

/* Dispatch the meta queue by picking, processing and unlocking the next RN from
 * a non-empty sub-queue with lowest priority. wq is equal to zebra->ribq and data
 * is pointed to the meta queue structure.
 */
static wq_item_status
meta_queue_process (struct work_queue *dummy, void *data)
{
  struct meta_queue * mq = data;
  unsigned i;

  for (i = 0; i < MQ_SIZE; i++)
    if (process_subq (mq->subq[i], i))
      {
	mq->size--;
	break;
      }
  return mq->size ? WQ_REQUEUE : WQ_SUCCESS;
}

/*
 * Map from rib types to queue type (priority) in meta queue
 */
static const u_char meta_queue_map[ZEBRA_ROUTE_MAX] = {
  [ZEBRA_ROUTE_SYSTEM]  = 4,
  [ZEBRA_ROUTE_KERNEL]  = 0,
  [ZEBRA_ROUTE_CONNECT] = 0,
  [ZEBRA_ROUTE_STATIC]  = 1,
  [ZEBRA_ROUTE_RIP]     = 2,
  [ZEBRA_ROUTE_RIPNG]   = 2,
  [ZEBRA_ROUTE_OSPF]    = 2,
  [ZEBRA_ROUTE_OSPF6]   = 2,
  [ZEBRA_ROUTE_ISIS]    = 2,
  [ZEBRA_ROUTE_BGP]     = 3,
  [ZEBRA_ROUTE_HSLS]    = 4,
  [ZEBRA_ROUTE_BABEL]   = 2,
};

/* Look into the RN and queue it into one or more priority queues,
 * increasing the size for each data push done.
 */
static void
rib_meta_queue_add (struct meta_queue *mq, struct route_node *rn)
{
  struct rib *rib;

  RNODE_FOREACH_RIB (rn, rib)
    {
      u_char qindex = meta_queue_map[rib->type];

      /* Invariant: at this point we always have rn->info set. */
      if (CHECK_FLAG (rib_dest_from_rnode (rn)->flags,
		      RIB_ROUTE_QUEUED (qindex)))
	{
	  if (IS_ZEBRA_DEBUG_RIB_Q)
	    rnode_debug (rn, "rn %p is already queued in sub-queue %u",
			 rn, qindex);
	  continue;
	}

      SET_FLAG (rib_dest_from_rnode (rn)->flags, RIB_ROUTE_QUEUED (qindex));
      listnode_add (mq->subq[qindex], rn);
      route_lock_node (rn);
      mq->size++;

      if (IS_ZEBRA_DEBUG_RIB_Q)
	rnode_debug (rn, "queued rn %p into sub-queue %u",
		     rn, qindex);
    }
}

/* Add route_node to work queue and schedule processing */
static void
rib_queue_add (struct zebra_t *zebra, struct route_node *rn)
{
  assert (zebra && rn);

  /* Pointless to queue a route_node with no RIB entries to add or remove */
  if (!rnode_to_ribs (rn))
    {
      zlog_debug ("%s: called for route_node (%p, %d) with no ribs",
                  __func__, rn, rn->lock);
      zlog_backtrace(LOG_DEBUG);
      return;
    }

  if (IS_ZEBRA_DEBUG_RIB_Q)
    rnode_info (rn, "work queue added");

  assert (zebra);

  if (zebra->ribq == NULL)
    {
      zlog_err ("%s: work_queue does not exist!", __func__);
      return;
    }

  /*
   * The RIB queue should normally be either empty or holding the only
   * work_queue_item element. In the latter case this element would
   * hold a pointer to the meta queue structure, which must be used to
   * actually queue the route nodes to process. So create the MQ
   * holder, if necessary, then push the work into it in any case.
   * This semantics was introduced after 0.99.9 release.
   */
  if (!zebra->ribq->items->count)
    work_queue_add (zebra->ribq, zebra->mq);

  rib_meta_queue_add (zebra->mq, rn);

  if (IS_ZEBRA_DEBUG_RIB_Q)
    rnode_debug (rn, "rn %p queued", rn);

  return;
}

/* Create new meta queue.
   A destructor function doesn't seem to be necessary here.
 */
static struct meta_queue *
meta_queue_new (void)
{
  struct meta_queue *new;
  unsigned i;

  new = XCALLOC (MTYPE_WORK_QUEUE, sizeof (struct meta_queue));
  assert(new);

  for (i = 0; i < MQ_SIZE; i++)
    {
      new->subq[i] = list_new ();
      assert(new->subq[i]);
    }

  return new;
}

/* initialise zebra rib work queue */
static void
rib_queue_init (struct zebra_t *zebra)
{
  assert (zebra);
  
  if (! (zebra->ribq = work_queue_new (zebra->master, 
                                       "route_node processing")))
    {
      zlog_err ("%s: could not initialise work queue!", __func__);
      return;
    }

  /* fill in the work queue spec */
  zebra->ribq->spec.workfunc = &meta_queue_process;
  zebra->ribq->spec.errorfunc = NULL;
  /* XXX: TODO: These should be runtime configurable via vty */
  zebra->ribq->spec.max_retries = 3;
  zebra->ribq->spec.hold = rib_process_hold_time;
  
  if (!(zebra->mq = meta_queue_new ()))
  {
    zlog_err ("%s: could not initialise meta queue!", __func__);
    return;
  }
  return;
}

/* RIB updates are processed via a queue of pointers to route_nodes.
 *
 * The queue length is bounded by the maximal size of the routing table,
 * as a route_node will not be requeued, if already queued.
 *
 * RIBs are submitted via rib_addnode or rib_delnode which set minimal
 * state, or static_install_ipv{4,6} (when an existing RIB is updated)
 * and then submit route_node to queue for best-path selection later.
 * Order of add/delete state changes are preserved for any given RIB.
 *
 * Deleted RIBs are reaped during best-path selection.
 *
 * rib_addnode
 * |-> rib_link or unset RIB_ENTRY_REMOVE        |->Update kernel with
 *       |-------->|                             |  best RIB, if required
 *                 |                             |
 * static_install->|->rib_addqueue...... -> rib_process
 *                 |                             |
 *       |-------->|                             |-> rib_unlink
 * |-> set RIB_ENTRY_REMOVE                           |
 * rib_delnode                                  (RIB freed)
 *
 * The 'info' pointer of a route_node points to a rib_dest_t
 * ('dest'). Queueing state for a route_node is kept on the dest. The
 * dest is created on-demand by rib_link() and is kept around at least
 * as long as there are ribs hanging off it (@see rib_gc_dest()).
 * 
 * Refcounting (aka "locking" throughout the GNU Zebra and Quagga code):
 *
 * - route_nodes: refcounted by:
 *   - dest attached to route_node:
 *     - managed by: rib_link/rib_gc_dest
 *   - route_node processing queue
 *     - managed by: rib_addqueue, rib_process.
 *
 */
 
/* Add RIB to head of the route node. */
static void
rib_link (struct route_node *rn, struct rib *rib)
{
  struct rib *head;
  rib_dest_t *dest;

  assert (rib && rn);
  
  if (IS_ZEBRA_DEBUG_RIB)
    rnode_debug (rn, "rn %p, rib %p", rn, rib);

  dest = rib_dest_from_rnode (rn);
  if (!dest)
    {
      if (IS_ZEBRA_DEBUG_RIB)
	rnode_debug (rn, "adding dest to table");

      dest = XCALLOC (MTYPE_RIB_DEST, sizeof (rib_dest_t));
      route_lock_node (rn); /* rn route table reference */
      rn->info = dest;
      dest->rnode = rn;
    }

  head = dest->routes;
  if (head)
    {
      head->prev = rib;
    }
  rib->next = head;
  dest->routes = rib;
  rib_queue_add (&zebrad, rn);
}

static void
rib_addnode (struct route_node *rn, struct rib *rib)
{
  /* RIB node has been un-removed before route-node is processed. 
   * route_node must hence already be on the queue for processing.. 
   */
  if (CHECK_FLAG (rib->status, RIB_ENTRY_REMOVED))
    {
      if (IS_ZEBRA_DEBUG_RIB)
        rnode_debug (rn, "rn %p, un-removed rib %p", rn, rib);

      UNSET_FLAG (rib->status, RIB_ENTRY_REMOVED);
      return;
    }
  rib_link (rn, rib);
}

/*
 * rib_unlink
 *
 * Detach a rib structure from a route_node.
 *
 * Note that a call to rib_unlink() should be followed by a call to
 * rib_gc_dest() at some point. This allows a rib_dest_t that is no
 * longer required to be deleted.
 */
static void
rib_unlink (struct route_node *rn, struct rib *rib)
{
  rib_dest_t *dest;

  assert (rn && rib);

  if (IS_ZEBRA_DEBUG_RIB)
    rnode_debug (rn, "rn %p, rib %p", rn, rib);

  dest = rib_dest_from_rnode (rn);

  if (rib->next)
    rib->next->prev = rib->prev;

  if (rib->prev)
    rib->prev->next = rib->next;
  else
    {
      dest->routes = rib->next;
    }

  /* free RIB and nexthops */
  nexthops_free(rib->nexthop);
  XFREE (MTYPE_RIB, rib);

}

static void
rib_delnode (struct route_node *rn, struct rib *rib)
{
  if (IS_ZEBRA_DEBUG_RIB)
    rnode_debug (rn, "rn %p, rib %p, removing", rn, rib);
  SET_FLAG (rib->status, RIB_ENTRY_REMOVED);
  rib_queue_add (&zebrad, rn);
}

int
rib_add_ipv4 (int type, int flags, struct prefix_ipv4 *p, 
	      struct in_addr *gate, struct in_addr *src,
	      unsigned int ifindex, u_int32_t vrf_id,
	      u_int32_t metric, u_char distance, safi_t safi)
{
  struct rib *rib;
  struct rib *same = NULL;
  struct route_table *table;
  struct route_node *rn;
  struct nexthop *nexthop;

  /* Lookup table.  */
  table = vrf_table (AFI_IP, safi, 0);
  if (! table)
    return 0;

  /* Make it sure prefixlen is applied to the prefix. */
  apply_mask_ipv4 (p);

  /* Set default distance by route type. */
  if (distance == 0)
    {
      if ((unsigned)type >= array_size(route_info))
	distance = 150;
      else
        distance = route_info[type].distance;

      /* iBGP distance is 200. */
      if (type == ZEBRA_ROUTE_BGP && CHECK_FLAG (flags, ZEBRA_FLAG_IBGP))
	distance = 200;
    }

  /* Lookup route node.*/
  rn = route_node_get (table, (struct prefix *) p);

  /* If same type of route are installed, treat it as a implicit
     withdraw. */
  RNODE_FOREACH_RIB (rn, rib)
    {
      if (CHECK_FLAG (rib->status, RIB_ENTRY_REMOVED))
        continue;
      
      if (rib->type != type)
	continue;
      if (rib->type != ZEBRA_ROUTE_CONNECT)
        {
          same = rib;
          break;
        }
      /* Duplicate connected route comes in. */
      else if ((nexthop = rib->nexthop) &&
	       nexthop->type == NEXTHOP_TYPE_IFINDEX &&
	       nexthop->ifindex == ifindex &&
	       !CHECK_FLAG (rib->status, RIB_ENTRY_REMOVED))
	{
	  rib->refcnt++;
	  return 0 ;
	}
    }

  /* Allocate new rib structure. */
  rib = XCALLOC (MTYPE_RIB, sizeof (struct rib));
  rib->type = type;
  rib->distance = distance;
  rib->flags = flags;
  rib->metric = metric;
  rib->table = vrf_id;
  rib->nexthop_num = 0;
  rib->uptime = time (NULL);

  /* Nexthop settings. */
  if (gate)
    {
      if (ifindex)
	nexthop_ipv4_ifindex_add (rib, gate, src, ifindex);
      else
	nexthop_ipv4_add (rib, gate, src);
    }
  else
    nexthop_ifindex_add (rib, ifindex);

  /* If this route is kernel route, set FIB flag to the route. */
  if (type == ZEBRA_ROUTE_KERNEL || type == ZEBRA_ROUTE_CONNECT)
    for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
      SET_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB);

  /* Link new rib to node.*/
  if (IS_ZEBRA_DEBUG_RIB)
    zlog_debug ("%s: calling rib_addnode (%p, %p)", __func__, rn, rib);
  rib_addnode (rn, rib);
  
  /* Free implicit route.*/
  if (same)
  {
    if (IS_ZEBRA_DEBUG_RIB)
      zlog_debug ("%s: calling rib_delnode (%p, %p)", __func__, rn, rib);
    rib_delnode (rn, same);
  }
  
  route_unlock_node (rn);
  return 0;
}

/* This function dumps the contents of a given RIB entry into
 * standard debug log. Calling function name and IP prefix in
 * question are passed as 1st and 2nd arguments.
 */

void _rib_dump (const char * func,
		union prefix46constptr pp, const struct rib * rib)
{
  const struct prefix *p = pp.p;
  char straddr[INET6_ADDRSTRLEN];
  struct nexthop *nexthop, *tnexthop;
  int recursing;

  inet_ntop (p->family, &p->u.prefix, straddr, INET6_ADDRSTRLEN);
  zlog_debug ("%s: dumping RIB entry %p for %s/%d", func, rib, straddr, p->prefixlen);
  zlog_debug
  (
    "%s: refcnt == %lu, uptime == %lu, type == %u, table == %d",
    func,
    rib->refcnt,
    (unsigned long) rib->uptime,
    rib->type,
    rib->table
  );
  zlog_debug
  (
    "%s: metric == %u, distance == %u, flags == %u, status == %u",
    func,
    rib->metric,
    rib->distance,
    rib->flags,
    rib->status
  );
  zlog_debug
  (
    "%s: nexthop_num == %u, nexthop_active_num == %u, nexthop_fib_num == %u",
    func,
    rib->nexthop_num,
    rib->nexthop_active_num,
    rib->nexthop_fib_num
  );

  for (ALL_NEXTHOPS_RO(rib->nexthop, nexthop, tnexthop, recursing))
    {
      inet_ntop (p->family, &nexthop->gate, straddr, INET6_ADDRSTRLEN);
      zlog_debug
      (
        "%s: %s %s with flags %s%s%s",
        func,
        (recursing ? "  NH" : "NH"),
        straddr,
        (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE) ? "ACTIVE " : ""),
        (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB) ? "FIB " : ""),
        (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE) ? "RECURSIVE" : "")
      );
    }
  zlog_debug ("%s: dump complete", func);
}

/* This is an exported helper to rtm_read() to dump the strange
 * RIB entry found by rib_lookup_ipv4_route()
 */

void rib_lookup_and_dump (struct prefix_ipv4 * p)
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  char prefix_buf[INET_ADDRSTRLEN];

  /* Lookup table.  */
  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
  {
    zlog_err ("%s: vrf_table() returned NULL", __func__);
    return;
  }

  inet_ntop (AF_INET, &p->prefix.s_addr, prefix_buf, INET_ADDRSTRLEN);
  /* Scan the RIB table for exactly matching RIB entry. */
  rn = route_node_lookup (table, (struct prefix *) p);

  /* No route for this prefix. */
  if (! rn)
  {
    zlog_debug ("%s: lookup failed for %s/%d", __func__, prefix_buf, p->prefixlen);
    return;
  }

  /* Unlock node. */
  route_unlock_node (rn);

  /* let's go */
  RNODE_FOREACH_RIB (rn, rib)
  {
    zlog_debug
    (
      "%s: rn %p, rib %p: %s, %s",
      __func__,
      rn,
      rib,
      (CHECK_FLAG (rib->status, RIB_ENTRY_REMOVED) ? "removed" : "NOT removed"),
      (CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELECTED) ? "selected" : "NOT selected")
    );
    rib_dump (p, rib);
  }
}

/* Check if requested address assignment will fail due to another
 * route being installed by zebra in FIB already. Take necessary
 * actions, if needed: remove such a route from FIB and deSELECT
 * corresponding RIB entry. Then put affected RN into RIBQ head.
 */
void rib_lookup_and_pushup (struct prefix_ipv4 * p)
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  unsigned changed = 0;

  if (NULL == (table = vrf_table (AFI_IP, SAFI_UNICAST, 0)))
  {
    zlog_err ("%s: vrf_table() returned NULL", __func__);
    return;
  }

  /* No matches would be the simplest case. */
  if (NULL == (rn = route_node_lookup (table, (struct prefix *) p)))
    return;

  /* Unlock node. */
  route_unlock_node (rn);

  /* Check all RIB entries. In case any changes have to be done, requeue
   * the RN into RIBQ head. If the routing message about the new connected
   * route (generated by the IP address we are going to assign very soon)
   * comes before the RIBQ is processed, the new RIB entry will join
   * RIBQ record already on head. This is necessary for proper revalidation
   * of the rest of the RIB.
   */
  RNODE_FOREACH_RIB (rn, rib)
  {
    if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELECTED) &&
      ! RIB_SYSTEM_ROUTE (rib))
    {
      changed = 1;
      if (IS_ZEBRA_DEBUG_RIB)
      {
        char buf[INET_ADDRSTRLEN];
        inet_ntop (rn->p.family, &p->prefix, buf, INET_ADDRSTRLEN);
        zlog_debug ("%s: freeing way for connected prefix %s/%d", __func__, buf, p->prefixlen);
        rib_dump (&rn->p, rib);
      }
      rib_uninstall (rn, rib);
    }
  }
  if (changed)
    rib_queue_add (&zebrad, rn);
}

int
rib_add_ipv4_multipath (struct prefix_ipv4 *p, struct rib *rib, safi_t safi)
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *same;
  struct nexthop *nexthop;
  
  /* Lookup table.  */
  table = vrf_table (AFI_IP, safi, 0);
  if (! table)
    return 0;

  /* Make it sure prefixlen is applied to the prefix. */
  apply_mask_ipv4 (p);

  /* Set default distance by route type. */
  if (rib->distance == 0)
    {
      rib->distance = route_info[rib->type].distance;

      /* iBGP distance is 200. */
      if (rib->type == ZEBRA_ROUTE_BGP 
	  && CHECK_FLAG (rib->flags, ZEBRA_FLAG_IBGP))
	rib->distance = 200;
    }

  /* Lookup route node.*/
  rn = route_node_get (table, (struct prefix *) p);

  /* If same type of route are installed, treat it as a implicit
     withdraw. */
  RNODE_FOREACH_RIB (rn, same)
    {
      if (CHECK_FLAG (same->status, RIB_ENTRY_REMOVED))
        continue;
      
      if (same->type == rib->type && same->table == rib->table
	  && same->type != ZEBRA_ROUTE_CONNECT)
        break;
    }
  
  /* If this route is kernel route, set FIB flag to the route. */
  if (rib->type == ZEBRA_ROUTE_KERNEL || rib->type == ZEBRA_ROUTE_CONNECT)
    for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
      SET_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB);

  /* Link new rib to node.*/
  rib_addnode (rn, rib);
  if (IS_ZEBRA_DEBUG_RIB)
  {
    zlog_debug ("%s: called rib_addnode (%p, %p) on new RIB entry",
      __func__, rn, rib);
    rib_dump (p, rib);
  }

  /* Free implicit route.*/
  if (same)
  {
    if (IS_ZEBRA_DEBUG_RIB)
    {
      zlog_debug ("%s: calling rib_delnode (%p, %p) on existing RIB entry",
        __func__, rn, same);
      rib_dump (p, same);
    }
    rib_delnode (rn, same);
  }
  
  route_unlock_node (rn);
  return 0;
}

/* XXX factor with rib_delete_ipv6 */
int
rib_delete_ipv4 (int type, int flags, struct prefix_ipv4 *p,
		 struct in_addr *gate, unsigned int ifindex, u_int32_t vrf_id, safi_t safi)
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  struct rib *fib = NULL;
  struct rib *same = NULL;
  struct nexthop *nexthop, *tnexthop;
  int recursing;
  char buf1[INET_ADDRSTRLEN];
  char buf2[INET_ADDRSTRLEN];

  /* Lookup table.  */
  table = vrf_table (AFI_IP, safi, 0);
  if (! table)
    return 0;

  /* Apply mask. */
  apply_mask_ipv4 (p);

  if (IS_ZEBRA_DEBUG_KERNEL)
    {
      if (gate)
	zlog_debug ("rib_delete_ipv4(): route delete %s/%d via %s ifindex %d",
		    inet_ntop (AF_INET, &p->prefix, buf1, INET_ADDRSTRLEN),
		    p->prefixlen,
		    inet_ntoa (*gate),
		    ifindex);
      else
	zlog_debug ("rib_delete_ipv4(): route delete %s/%d ifindex %d",
		    inet_ntop (AF_INET, &p->prefix, buf1, INET_ADDRSTRLEN),
		    p->prefixlen,
		    ifindex);
    }

  /* Lookup route node. */
  rn = route_node_lookup (table, (struct prefix *) p);
  if (! rn)
    {
      if (IS_ZEBRA_DEBUG_KERNEL)
	{
	  if (gate)
	    zlog_debug ("route %s/%d via %s ifindex %d doesn't exist in rib",
		       inet_ntop (AF_INET, &p->prefix, buf1, INET_ADDRSTRLEN),
		       p->prefixlen,
		       inet_ntop (AF_INET, gate, buf2, INET_ADDRSTRLEN),
		       ifindex);
	  else
	    zlog_debug ("route %s/%d ifindex %d doesn't exist in rib",
		       inet_ntop (AF_INET, &p->prefix, buf1, INET_ADDRSTRLEN),
		       p->prefixlen,
		       ifindex);
	}
      return ZEBRA_ERR_RTNOEXIST;
    }

  /* Lookup same type route. */
  RNODE_FOREACH_RIB (rn, rib)
    {
      if (CHECK_FLAG (rib->status, RIB_ENTRY_REMOVED))
        continue;

      if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELECTED))
	fib = rib;

      if (rib->type != type)
	continue;
      if (rib->type == ZEBRA_ROUTE_CONNECT && (nexthop = rib->nexthop) &&
	  nexthop->type == NEXTHOP_TYPE_IFINDEX)
	{
	  if (nexthop->ifindex != ifindex)
	    continue;
	  if (rib->refcnt)
	    {
	      rib->refcnt--;
	      route_unlock_node (rn);
	      route_unlock_node (rn);
	      return 0;
	    }
	  same = rib;
	  break;
	}
      /* Make sure that the route found has the same gateway. */
      else
        {
          if (gate == NULL)
            {
              same = rib;
              break;
            }
          for (ALL_NEXTHOPS_RO(rib->nexthop, nexthop, tnexthop, recursing))
            if (IPV4_ADDR_SAME (&nexthop->gate.ipv4, gate))
              {
                same = rib;
                break;
              }
          if (same)
            break;
        }
    }
  /* If same type of route can't be found and this message is from
     kernel. */
  if (! same)
    {
      if (fib && type == ZEBRA_ROUTE_KERNEL)
	{
	  /* Unset flags. */
	  for (nexthop = fib->nexthop; nexthop; nexthop = nexthop->next)
	    UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB);

	  UNSET_FLAG (fib->flags, ZEBRA_FLAG_SELECTED);
	}
      else
	{
	  if (IS_ZEBRA_DEBUG_KERNEL)
	    {
	      if (gate)
		zlog_debug ("route %s/%d via %s ifindex %d type %d doesn't exist in rib",
			   inet_ntop (AF_INET, &p->prefix, buf1, INET_ADDRSTRLEN),
			   p->prefixlen,
			   inet_ntop (AF_INET, gate, buf2, INET_ADDRSTRLEN),
			   ifindex,
			   type);
	      else
		zlog_debug ("route %s/%d ifindex %d type %d doesn't exist in rib",
			   inet_ntop (AF_INET, &p->prefix, buf1, INET_ADDRSTRLEN),
			   p->prefixlen,
			   ifindex,
			   type);
	    }
	  route_unlock_node (rn);
	  return ZEBRA_ERR_RTNOEXIST;
	}
    }
  
  if (same)
    rib_delnode (rn, same);
  
  route_unlock_node (rn);
  return 0;
}

/* Install static route into rib. */
static void
static_install_ipv4 (safi_t safi, struct prefix *p, struct static_ipv4 *si)
{
  struct rib *rib;
  struct route_node *rn;
  struct route_table *table;

  /* Lookup table.  */
  table = vrf_table (AFI_IP, safi, 0);
  if (! table)
    return;

  /* Lookup existing route */
  rn = route_node_get (table, p);
  RNODE_FOREACH_RIB (rn, rib)
    {
       if (CHECK_FLAG (rib->status, RIB_ENTRY_REMOVED))
         continue;
        
       if (rib->type == ZEBRA_ROUTE_STATIC && rib->distance == si->distance)
         break;
    }

  if (rib)
    {
      /* Same distance static route is there.  Update it with new
         nexthop. */
      route_unlock_node (rn);
      switch (si->type)
        {
          case STATIC_IPV4_GATEWAY:
            nexthop_ipv4_add (rib, &si->gate.ipv4, NULL);
            break;
          case STATIC_IPV4_IFNAME:
            nexthop_ifname_add (rib, si->gate.ifname);
            break;
          case STATIC_IPV4_BLACKHOLE:
            nexthop_blackhole_add (rib);
            break;
        }
      rib_queue_add (&zebrad, rn);
    }
  else
    {
      /* This is new static route. */
      rib = XCALLOC (MTYPE_RIB, sizeof (struct rib));
      
      rib->type = ZEBRA_ROUTE_STATIC;
      rib->distance = si->distance;
      rib->metric = 0;
      rib->table = zebrad.rtm_table_default;
      rib->nexthop_num = 0;

      switch (si->type)
        {
          case STATIC_IPV4_GATEWAY:
            nexthop_ipv4_add (rib, &si->gate.ipv4, NULL);
            break;
          case STATIC_IPV4_IFNAME:
            nexthop_ifname_add (rib, si->gate.ifname);
            break;
          case STATIC_IPV4_BLACKHOLE:
            nexthop_blackhole_add (rib);
            break;
        }

      /* Save the flags of this static routes (reject, blackhole) */
      rib->flags = si->flags;

      /* Link this rib to the tree. */
      rib_addnode (rn, rib);
    }
}

static int
static_ipv4_nexthop_same (struct nexthop *nexthop, struct static_ipv4 *si)
{
  if (nexthop->type == NEXTHOP_TYPE_IPV4
      && si->type == STATIC_IPV4_GATEWAY
      && IPV4_ADDR_SAME (&nexthop->gate.ipv4, &si->gate.ipv4))
    return 1;
  if (nexthop->type == NEXTHOP_TYPE_IFNAME
      && si->type == STATIC_IPV4_IFNAME
      && strcmp (nexthop->ifname, si->gate.ifname) == 0)
    return 1;
  if (nexthop->type == NEXTHOP_TYPE_BLACKHOLE
      && si->type == STATIC_IPV4_BLACKHOLE)
    return 1;
  return 0;
}

/* Uninstall static route from RIB. */
static void
static_uninstall_ipv4 (safi_t safi, struct prefix *p, struct static_ipv4 *si)
{
  struct route_node *rn;
  struct rib *rib;
  struct nexthop *nexthop;
  struct route_table *table;

  /* Lookup table.  */
  table = vrf_table (AFI_IP, safi, 0);
  if (! table)
    return;
  
  /* Lookup existing route with type and distance. */
  rn = route_node_lookup (table, p);
  if (! rn)
    return;

  RNODE_FOREACH_RIB (rn, rib)
    {
      if (CHECK_FLAG (rib->status, RIB_ENTRY_REMOVED))
        continue;

      if (rib->type == ZEBRA_ROUTE_STATIC && rib->distance == si->distance)
        break;
    }

  if (! rib)
    {
      route_unlock_node (rn);
      return;
    }

  /* Lookup nexthop. */
  for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
    if (static_ipv4_nexthop_same (nexthop, si))
      break;

  /* Can't find nexthop. */
  if (! nexthop)
    {
      route_unlock_node (rn);
      return;
    }
  
  /* Check nexthop. */
  if (rib->nexthop_num == 1)
    rib_delnode (rn, rib);
  else
    {
      if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB))
        rib_uninstall (rn, rib);
      nexthop_delete (rib, nexthop);
      nexthop_free (nexthop);
      rib_queue_add (&zebrad, rn);
    }
  /* Unlock node. */
  route_unlock_node (rn);
}

int
static_add_ipv4_safi (safi_t safi, struct prefix *p, struct in_addr *gate,
		      const char *ifname, u_char flags, u_char distance,
		      u_int32_t vrf_id)
{
  u_char type = 0;
  struct route_node *rn;
  struct static_ipv4 *si;
  struct static_ipv4 *pp;
  struct static_ipv4 *cp;
  struct static_ipv4 *update = NULL;
  struct route_table *stable;

  /* Lookup table.  */
  stable = vrf_static_table (AFI_IP, safi, vrf_id);
  if (! stable)
    return -1;
  
  /* Lookup static route prefix. */
  rn = route_node_get (stable, p);

  /* Make flags. */
  if (gate)
    type = STATIC_IPV4_GATEWAY;
  else if (ifname)
    type = STATIC_IPV4_IFNAME;
  else
    type = STATIC_IPV4_BLACKHOLE;

  /* Do nothing if there is a same static route.  */
  for (si = rn->info; si; si = si->next)
    {
      if (type == si->type
	  && (! gate || IPV4_ADDR_SAME (gate, &si->gate.ipv4))
	  && (! ifname || strcmp (ifname, si->gate.ifname) == 0))
	{
	  if (distance == si->distance)
	    {
	      route_unlock_node (rn);
	      return 0;
	    }
	  else
	    update = si;
	}
    }

  /* Distance changed.  */
  if (update)
    static_delete_ipv4_safi (safi, p, gate, ifname, update->distance, vrf_id);

  /* Make new static route structure. */
  si = XCALLOC (MTYPE_STATIC_IPV4, sizeof (struct static_ipv4));

  si->type = type;
  si->distance = distance;
  si->flags = flags;

  if (gate)
    si->gate.ipv4 = *gate;
  if (ifname)
    si->gate.ifname = XSTRDUP (0, ifname);

  /* Add new static route information to the tree with sort by
     distance value and gateway address. */
  for (pp = NULL, cp = rn->info; cp; pp = cp, cp = cp->next)
    {
      if (si->distance < cp->distance)
	break;
      if (si->distance > cp->distance)
	continue;
      if (si->type == STATIC_IPV4_GATEWAY && cp->type == STATIC_IPV4_GATEWAY)
	{
	  if (ntohl (si->gate.ipv4.s_addr) < ntohl (cp->gate.ipv4.s_addr))
	    break;
	  if (ntohl (si->gate.ipv4.s_addr) > ntohl (cp->gate.ipv4.s_addr))
	    continue;
	}
    }

  /* Make linked list. */
  if (pp)
    pp->next = si;
  else
    rn->info = si;
  if (cp)
    cp->prev = si;
  si->prev = pp;
  si->next = cp;

  /* Install into rib. */
  static_install_ipv4 (safi, p, si);

  return 1;
}

int
static_delete_ipv4_safi (safi_t safi, struct prefix *p, struct in_addr *gate,
			 const char *ifname, u_char distance, u_int32_t vrf_id)
{
  u_char type = 0;
  struct route_node *rn;
  struct static_ipv4 *si;
  struct route_table *stable;

  /* Lookup table.  */
  stable = vrf_static_table (AFI_IP, safi, vrf_id);
  if (! stable)
    return -1;

  /* Lookup static route prefix. */
  rn = route_node_lookup (stable, p);
  if (! rn)
    return 0;

  /* Make flags. */
  if (gate)
    type = STATIC_IPV4_GATEWAY;
  else if (ifname)
    type = STATIC_IPV4_IFNAME;
  else
    type = STATIC_IPV4_BLACKHOLE;

  /* Find same static route is the tree */
  for (si = rn->info; si; si = si->next)
    if (type == si->type
	&& (! gate || IPV4_ADDR_SAME (gate, &si->gate.ipv4))
	&& (! ifname || strcmp (ifname, si->gate.ifname) == 0))
      break;

  /* Can't find static route. */
  if (! si)
    {
      route_unlock_node (rn);
      return 0;
    }

  /* Install into rib. */
  static_uninstall_ipv4 (safi, p, si);

  /* Unlink static route from linked list. */
  if (si->prev)
    si->prev->next = si->next;
  else
    rn->info = si->next;
  if (si->next)
    si->next->prev = si->prev;
  route_unlock_node (rn);
  
  /* Free static route configuration. */
  if (ifname)
    XFREE (0, si->gate.ifname);
  XFREE (MTYPE_STATIC_IPV4, si);

  route_unlock_node (rn);

  return 1;
}

#ifdef HAVE_IPV6
int
rib_add_ipv6 (int type, int flags, struct prefix_ipv6 *p,
	      struct in6_addr *gate, unsigned int ifindex, u_int32_t vrf_id,
	      u_int32_t metric, u_char distance, safi_t safi)
{
  struct rib *rib;
  struct rib *same = NULL;
  struct route_table *table;
  struct route_node *rn;
  struct nexthop *nexthop;

  /* Lookup table.  */
  table = vrf_table (AFI_IP6, safi, 0);
  if (! table)
    return 0;

  /* Make sure mask is applied. */
  apply_mask_ipv6 (p);

  /* Set default distance by route type. */
  if (!distance)
    distance = route_info[type].distance;
  
  if (type == ZEBRA_ROUTE_BGP && CHECK_FLAG (flags, ZEBRA_FLAG_IBGP))
    distance = 200;

  /* Lookup route node.*/
  rn = route_node_get (table, (struct prefix *) p);

  /* If same type of route are installed, treat it as a implicit
     withdraw. */
  RNODE_FOREACH_RIB (rn, rib)
    {
      if (CHECK_FLAG (rib->status, RIB_ENTRY_REMOVED))
        continue;

      if (rib->type != type)
	continue;
      if (rib->type != ZEBRA_ROUTE_CONNECT)
	{
	  same = rib;
	  break;
	}
      else if ((nexthop = rib->nexthop) &&
	       nexthop->type == NEXTHOP_TYPE_IFINDEX &&
	       nexthop->ifindex == ifindex)
	{
	  rib->refcnt++;
	  return 0;
	}
    }

  /* Allocate new rib structure. */
  rib = XCALLOC (MTYPE_RIB, sizeof (struct rib));
  
  rib->type = type;
  rib->distance = distance;
  rib->flags = flags;
  rib->metric = metric;
  rib->table = vrf_id;
  rib->nexthop_num = 0;
  rib->uptime = time (NULL);

  /* Nexthop settings. */
  if (gate)
    {
      if (ifindex)
	nexthop_ipv6_ifindex_add (rib, gate, ifindex);
      else
	nexthop_ipv6_add (rib, gate);
    }
  else
    nexthop_ifindex_add (rib, ifindex);

  /* If this route is kernel route, set FIB flag to the route. */
  if (type == ZEBRA_ROUTE_KERNEL || type == ZEBRA_ROUTE_CONNECT)
    for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
      SET_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB);

  /* Link new rib to node.*/
  rib_addnode (rn, rib);
  if (IS_ZEBRA_DEBUG_RIB)
  {
    zlog_debug ("%s: called rib_addnode (%p, %p) on new RIB entry",
      __func__, rn, rib);
    rib_dump (p, rib);
  }

  /* Free implicit route.*/
  if (same)
  {
    if (IS_ZEBRA_DEBUG_RIB)
    {
      zlog_debug ("%s: calling rib_delnode (%p, %p) on existing RIB entry",
        __func__, rn, same);
      rib_dump (p, same);
    }
    rib_delnode (rn, same);
  }
  
  route_unlock_node (rn);
  return 0;
}

/* XXX factor with rib_delete_ipv6 */
int
rib_delete_ipv6 (int type, int flags, struct prefix_ipv6 *p,
		 struct in6_addr *gate, unsigned int ifindex, u_int32_t vrf_id, safi_t safi)
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  struct rib *fib = NULL;
  struct rib *same = NULL;
  struct nexthop *nexthop, *tnexthop;
  int recursing;
  char buf1[INET6_ADDRSTRLEN];
  char buf2[INET6_ADDRSTRLEN];

  /* Apply mask. */
  apply_mask_ipv6 (p);

  /* Lookup table.  */
  table = vrf_table (AFI_IP6, safi, 0);
  if (! table)
    return 0;
  
  /* Lookup route node. */
  rn = route_node_lookup (table, (struct prefix *) p);
  if (! rn)
    {
      if (IS_ZEBRA_DEBUG_KERNEL)
	{
	  if (gate)
	    zlog_debug ("route %s/%d via %s ifindex %d doesn't exist in rib",
		       inet_ntop (AF_INET6, &p->prefix, buf1, INET6_ADDRSTRLEN),
		       p->prefixlen,
		       inet_ntop (AF_INET6, gate, buf2, INET6_ADDRSTRLEN),
		       ifindex);
	  else
	    zlog_debug ("route %s/%d ifindex %d doesn't exist in rib",
		       inet_ntop (AF_INET6, &p->prefix, buf1, INET6_ADDRSTRLEN),
		       p->prefixlen,
		       ifindex);
	}
      return ZEBRA_ERR_RTNOEXIST;
    }

  /* Lookup same type route. */
  RNODE_FOREACH_RIB (rn, rib)
    {
      if (CHECK_FLAG(rib->status, RIB_ENTRY_REMOVED))
        continue;

      if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELECTED))
	fib = rib;

      if (rib->type != type)
        continue;
      if (rib->type == ZEBRA_ROUTE_CONNECT && (nexthop = rib->nexthop) &&
	  nexthop->type == NEXTHOP_TYPE_IFINDEX)
	{
	  if (nexthop->ifindex != ifindex)
	    continue;
	  if (rib->refcnt)
	    {
	      rib->refcnt--;
	      route_unlock_node (rn);
	      route_unlock_node (rn);
	      return 0;
	    }
	  same = rib;
	  break;
	}
      /* Make sure that the route found has the same gateway. */
      else
        {
          if (gate == NULL)
            {
              same = rib;
              break;
            }
          for (ALL_NEXTHOPS_RO(rib->nexthop, nexthop, tnexthop, recursing))
            if (IPV6_ADDR_SAME (&nexthop->gate.ipv6, gate))
              {
                same = rib;
                break;
              }
          if (same)
            break;
        }
    }

  /* If same type of route can't be found and this message is from
     kernel. */
  if (! same)
    {
      if (fib && type == ZEBRA_ROUTE_KERNEL)
	{
	  /* Unset flags. */
	  for (nexthop = fib->nexthop; nexthop; nexthop = nexthop->next)
	    UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB);

	  UNSET_FLAG (fib->flags, ZEBRA_FLAG_SELECTED);
	}
      else
	{
	  if (IS_ZEBRA_DEBUG_KERNEL)
	    {
	      if (gate)
		zlog_debug ("route %s/%d via %s ifindex %d type %d doesn't exist in rib",
			   inet_ntop (AF_INET6, &p->prefix, buf1, INET6_ADDRSTRLEN),
			   p->prefixlen,
			   inet_ntop (AF_INET6, gate, buf2, INET6_ADDRSTRLEN),
			   ifindex,
			   type);
	      else
		zlog_debug ("route %s/%d ifindex %d type %d doesn't exist in rib",
			   inet_ntop (AF_INET6, &p->prefix, buf1, INET6_ADDRSTRLEN),
			   p->prefixlen,
			   ifindex,
			   type);
	    }
	  route_unlock_node (rn);
	  return ZEBRA_ERR_RTNOEXIST;
	}
    }

  if (same)
    rib_delnode (rn, same);
  
  route_unlock_node (rn);
  return 0;
}

/* Install static route into rib. */
static void
static_install_ipv6 (struct prefix *p, struct static_ipv6 *si)
{
  struct rib *rib;
  struct route_table *table;
  struct route_node *rn;

  /* Lookup table.  */
  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (! table)
    return;

  /* Lookup existing route */
  rn = route_node_get (table, p);
  RNODE_FOREACH_RIB (rn, rib)
    {
      if (CHECK_FLAG(rib->status, RIB_ENTRY_REMOVED))
        continue;

      if (rib->type == ZEBRA_ROUTE_STATIC && rib->distance == si->distance)
        break;
    }

  if (rib)
    {
      /* Same distance static route is there.  Update it with new
         nexthop. */
      route_unlock_node (rn);

      switch (si->type)
	{
	case STATIC_IPV6_GATEWAY:
	  nexthop_ipv6_add (rib, &si->ipv6);
	  break;
	case STATIC_IPV6_IFNAME:
	  nexthop_ifname_add (rib, si->ifname);
	  break;
	case STATIC_IPV6_GATEWAY_IFNAME:
	  nexthop_ipv6_ifname_add (rib, &si->ipv6, si->ifname);
	  break;
	}
      rib_queue_add (&zebrad, rn);
    }
  else
    {
      /* This is new static route. */
      rib = XCALLOC (MTYPE_RIB, sizeof (struct rib));
      
      rib->type = ZEBRA_ROUTE_STATIC;
      rib->distance = si->distance;
      rib->metric = 0;
      rib->table = zebrad.rtm_table_default;
      rib->nexthop_num = 0;

      switch (si->type)
	{
	case STATIC_IPV6_GATEWAY:
	  nexthop_ipv6_add (rib, &si->ipv6);
	  break;
	case STATIC_IPV6_IFNAME:
	  nexthop_ifname_add (rib, si->ifname);
	  break;
	case STATIC_IPV6_GATEWAY_IFNAME:
	  nexthop_ipv6_ifname_add (rib, &si->ipv6, si->ifname);
	  break;
	}

      /* Save the flags of this static routes (reject, blackhole) */
      rib->flags = si->flags;

      /* Link this rib to the tree. */
      rib_addnode (rn, rib);
    }
}

static int
static_ipv6_nexthop_same (struct nexthop *nexthop, struct static_ipv6 *si)
{
  if (nexthop->type == NEXTHOP_TYPE_IPV6
      && si->type == STATIC_IPV6_GATEWAY
      && IPV6_ADDR_SAME (&nexthop->gate.ipv6, &si->ipv6))
    return 1;
  if (nexthop->type == NEXTHOP_TYPE_IFNAME
      && si->type == STATIC_IPV6_IFNAME
      && strcmp (nexthop->ifname, si->ifname) == 0)
    return 1;
  if (nexthop->type == NEXTHOP_TYPE_IPV6_IFNAME
      && si->type == STATIC_IPV6_GATEWAY_IFNAME
      && IPV6_ADDR_SAME (&nexthop->gate.ipv6, &si->ipv6)
      && strcmp (nexthop->ifname, si->ifname) == 0)
    return 1;
  return 0;
}

static void
static_uninstall_ipv6 (struct prefix *p, struct static_ipv6 *si)
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  struct nexthop *nexthop;

  /* Lookup table.  */
  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (! table)
    return;

  /* Lookup existing route with type and distance. */
  rn = route_node_lookup (table, (struct prefix *) p);
  if (! rn)
    return;

  RNODE_FOREACH_RIB (rn, rib)
    {
      if (CHECK_FLAG (rib->status, RIB_ENTRY_REMOVED))
        continue;
    
      if (rib->type == ZEBRA_ROUTE_STATIC && rib->distance == si->distance)
        break;
    }

  if (! rib)
    {
      route_unlock_node (rn);
      return;
    }

  /* Lookup nexthop. */
  for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
    if (static_ipv6_nexthop_same (nexthop, si))
      break;

  /* Can't find nexthop. */
  if (! nexthop)
    {
      route_unlock_node (rn);
      return;
    }
  
  /* Check nexthop. */
  if (rib->nexthop_num == 1)
    {
      rib_delnode (rn, rib);
    }
  else
    {
      if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB))
        rib_uninstall (rn, rib);
      nexthop_delete (rib, nexthop);
      nexthop_free (nexthop);
      rib_queue_add (&zebrad, rn);
    }
  /* Unlock node. */
  route_unlock_node (rn);
}

/* Add static route into static route configuration. */
int
static_add_ipv6 (struct prefix *p, u_char type, struct in6_addr *gate,
		 const char *ifname, u_char flags, u_char distance,
		 u_int32_t vrf_id)
{
  struct route_node *rn;
  struct static_ipv6 *si;
  struct static_ipv6 *pp;
  struct static_ipv6 *cp;
  struct route_table *stable;

  /* Lookup table.  */
  stable = vrf_static_table (AFI_IP6, SAFI_UNICAST, vrf_id);
  if (! stable)
    return -1;
    
  if (!gate &&
      (type == STATIC_IPV6_GATEWAY || type == STATIC_IPV6_GATEWAY_IFNAME))
    return -1;
  
  if (!ifname && 
      (type == STATIC_IPV6_GATEWAY_IFNAME || type == STATIC_IPV6_IFNAME))
    return -1;

  /* Lookup static route prefix. */
  rn = route_node_get (stable, p);

  /* Do nothing if there is a same static route.  */
  for (si = rn->info; si; si = si->next)
    {
      if (distance == si->distance 
	  && type == si->type
	  && (! gate || IPV6_ADDR_SAME (gate, &si->ipv6))
	  && (! ifname || strcmp (ifname, si->ifname) == 0))
	{
	  route_unlock_node (rn);
	  return 0;
	}
    }

  /* Make new static route structure. */
  si = XCALLOC (MTYPE_STATIC_IPV6, sizeof (struct static_ipv6));

  si->type = type;
  si->distance = distance;
  si->flags = flags;

  switch (type)
    {
    case STATIC_IPV6_GATEWAY:
      si->ipv6 = *gate;
      break;
    case STATIC_IPV6_IFNAME:
      si->ifname = XSTRDUP (0, ifname);
      break;
    case STATIC_IPV6_GATEWAY_IFNAME:
      si->ipv6 = *gate;
      si->ifname = XSTRDUP (0, ifname);
      break;
    }

  /* Add new static route information to the tree with sort by
     distance value and gateway address. */
  for (pp = NULL, cp = rn->info; cp; pp = cp, cp = cp->next)
    {
      if (si->distance < cp->distance)
	break;
      if (si->distance > cp->distance)
	continue;
    }

  /* Make linked list. */
  if (pp)
    pp->next = si;
  else
    rn->info = si;
  if (cp)
    cp->prev = si;
  si->prev = pp;
  si->next = cp;

  /* Install into rib. */
  static_install_ipv6 (p, si);

  return 1;
}

/* Delete static route from static route configuration. */
int
static_delete_ipv6 (struct prefix *p, u_char type, struct in6_addr *gate,
		    const char *ifname, u_char distance, u_int32_t vrf_id)
{
  struct route_node *rn;
  struct static_ipv6 *si;
  struct route_table *stable;

  /* Lookup table.  */
  stable = vrf_static_table (AFI_IP6, SAFI_UNICAST, vrf_id);
  if (! stable)
    return -1;

  /* Lookup static route prefix. */
  rn = route_node_lookup (stable, p);
  if (! rn)
    return 0;

  /* Find same static route is the tree */
  for (si = rn->info; si; si = si->next)
    if (distance == si->distance 
	&& type == si->type
	&& (! gate || IPV6_ADDR_SAME (gate, &si->ipv6))
	&& (! ifname || strcmp (ifname, si->ifname) == 0))
      break;

  /* Can't find static route. */
  if (! si)
    {
      route_unlock_node (rn);
      return 0;
    }

  /* Install into rib. */
  static_uninstall_ipv6 (p, si);

  /* Unlink static route from linked list. */
  if (si->prev)
    si->prev->next = si->next;
  else
    rn->info = si->next;
  if (si->next)
    si->next->prev = si->prev;
  
  /* Free static route configuration. */
  if (ifname)
    XFREE (0, si->ifname);
  XFREE (MTYPE_STATIC_IPV6, si);

  return 1;
}
#endif /* HAVE_IPV6 */

/* RIB update function. */
void
rib_update (void)
{
  struct route_node *rn;
  struct route_table *table;
  
  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (table)
    for (rn = route_top (table); rn; rn = route_next (rn))
      if (rnode_to_ribs (rn))
        rib_queue_add (&zebrad, rn);

  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (table)
    for (rn = route_top (table); rn; rn = route_next (rn))
      if (rnode_to_ribs (rn))
        rib_queue_add (&zebrad, rn);
}


/* Remove all routes which comes from non main table.  */
static void
rib_weed_table (struct route_table *table)
{
  struct route_node *rn;
  struct rib *rib;
  struct rib *next;

  if (table)
    for (rn = route_top (table); rn; rn = route_next (rn))
      RNODE_FOREACH_RIB_SAFE (rn, rib, next)
	{
	  if (CHECK_FLAG (rib->status, RIB_ENTRY_REMOVED))
	    continue;

	  if (rib->table != zebrad.rtm_table_default &&
	      rib->table != RT_TABLE_MAIN)
            rib_delnode (rn, rib);
	}
}

/* Delete all routes from non main table. */
void
rib_weed_tables (void)
{
  rib_weed_table (vrf_table (AFI_IP, SAFI_UNICAST, 0));
  rib_weed_table (vrf_table (AFI_IP6, SAFI_UNICAST, 0));
}

/* Delete self installed routes after zebra is relaunched.  */
static void
rib_sweep_table (struct route_table *table)
{
  struct route_node *rn;
  struct rib *rib;
  struct rib *next;
  int ret = 0;

  if (table)
    for (rn = route_top (table); rn; rn = route_next (rn))
      RNODE_FOREACH_RIB_SAFE (rn, rib, next)
	{
	  if (CHECK_FLAG (rib->status, RIB_ENTRY_REMOVED))
	    continue;

	  if (rib->type == ZEBRA_ROUTE_KERNEL && 
	      CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELFROUTE))
	    {
	      ret = rib_uninstall_kernel (rn, rib);
	      if (! ret)
                rib_delnode (rn, rib);
	    }
	}
}

/* Sweep all RIB tables.  */
void
rib_sweep_route (void)
{
  rib_sweep_table (vrf_table (AFI_IP, SAFI_UNICAST, 0));
  rib_sweep_table (vrf_table (AFI_IP6, SAFI_UNICAST, 0));
}

/* Remove specific by protocol routes from 'table'. */
static unsigned long
rib_score_proto_table (u_char proto, struct route_table *table)
{
  struct route_node *rn;
  struct rib *rib;
  struct rib *next;
  unsigned long n = 0;

  if (table)
    for (rn = route_top (table); rn; rn = route_next (rn))
      RNODE_FOREACH_RIB_SAFE (rn, rib, next)
        {
          if (CHECK_FLAG (rib->status, RIB_ENTRY_REMOVED))
            continue;
          if (rib->type == proto)
            {
              rib_delnode (rn, rib);
              n++;
            }
        }

  return n;
}

/* Remove specific by protocol routes. */
unsigned long
rib_score_proto (u_char proto)
{
  return  rib_score_proto_table (proto, vrf_table (AFI_IP,  SAFI_UNICAST, 0))
         +rib_score_proto_table (proto, vrf_table (AFI_IP6, SAFI_UNICAST, 0));
}

/* Close RIB and clean up kernel routes. */
static void
rib_close_table (struct route_table *table)
{
  struct route_node *rn;
  rib_table_info_t *info = table->info;
  struct rib *rib;

  if (table)
    for (rn = route_top (table); rn; rn = route_next (rn))
      RNODE_FOREACH_RIB (rn, rib)
        {
          if (!CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELECTED))
	    continue;

          if (info->safi == SAFI_UNICAST)
            zfpm_trigger_update (rn, NULL);

	  if (! RIB_SYSTEM_ROUTE (rib))
	    rib_uninstall_kernel (rn, rib);
        }
}

/* Close all RIB tables.  */
void
rib_close (void)
{
  rib_close_table (vrf_table (AFI_IP, SAFI_UNICAST, 0));
  rib_close_table (vrf_table (AFI_IP6, SAFI_UNICAST, 0));
}

/* Routing information base initialize. */
void
rib_init (void)
{
  rib_queue_init (&zebrad);
  /* VRF initialization.  */
  vrf_init ();
}

/*
 * vrf_id_get_next
 *
 * Get the first vrf id that is greater than the given vrf id if any.
 *
 * Returns TRUE if a vrf id was found, FALSE otherwise.
 */
static inline int
vrf_id_get_next (uint32_t id, uint32_t *next_id_p)
{
  while (++id < vector_active (vrf_vector))
    {
      if (vrf_lookup (id))
	{
	  *next_id_p = id;
	  return 1;
	}
    }

  return 0;
}

/*
 * rib_tables_iter_next
 *
 * Returns the next table in the iteration.
 */
struct route_table *
rib_tables_iter_next (rib_tables_iter_t *iter)
{
  struct route_table *table;

  /*
   * Array that helps us go over all AFI/SAFI combinations via one
   * index.
   */
  static struct {
    afi_t afi;
    safi_t safi;
  } afi_safis[] = {
    { AFI_IP, SAFI_UNICAST },
    { AFI_IP, SAFI_MULTICAST },
    { AFI_IP6, SAFI_UNICAST },
    { AFI_IP6, SAFI_MULTICAST },
  };

  table = NULL;

  switch (iter->state)
    {

    case RIB_TABLES_ITER_S_INIT:
      iter->vrf_id = 0;
      iter->afi_safi_ix = -1;

      /* Fall through */

    case RIB_TABLES_ITER_S_ITERATING:
      iter->afi_safi_ix++;
      while (1)
	{

	  while (iter->afi_safi_ix < (int) ZEBRA_NUM_OF (afi_safis))
	    {
	      table = vrf_table (afi_safis[iter->afi_safi_ix].afi,
				 afi_safis[iter->afi_safi_ix].safi,
				 iter->vrf_id);
	      if (table)
		break;

	      iter->afi_safi_ix++;
	    }

	  /*
	   * Found another table in this vrf.
	   */
	  if (table)
	    break;

	  /*
	   * Done with all tables in the current vrf, go to the next
	   * one.
	   */
	  if (!vrf_id_get_next (iter->vrf_id, &iter->vrf_id))
	    break;

	  iter->afi_safi_ix = 0;
	}

      break;

    case RIB_TABLES_ITER_S_DONE:
      return NULL;
    }

  if (table)
    iter->state = RIB_TABLES_ITER_S_ITERATING;
  else
    iter->state = RIB_TABLES_ITER_S_DONE;

  return table;
}
