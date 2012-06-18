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

#include "zebra/rib.h"
#include "zebra/rt.h"
#include "zebra/zserv.h"
#include "zebra/redistribute.h"
#include "zebra/debug.h"

/* Default rtm_table for all clients */
extern int rtm_table_default;

/* Each route type's string and default distance value. */
struct
{  
  int key;
  int distance;
} route_info[] =
{
  {ZEBRA_ROUTE_SYSTEM,    0},
  {ZEBRA_ROUTE_KERNEL,    0},
  {ZEBRA_ROUTE_CONNECT,   0},
  {ZEBRA_ROUTE_STATIC,    1},
  {ZEBRA_ROUTE_RIP,     120},
  {ZEBRA_ROUTE_RIPNG,   120},
  {ZEBRA_ROUTE_OSPF,    110},
  {ZEBRA_ROUTE_OSPF6,   110},
  {ZEBRA_ROUTE_BGP,      20  /* IBGP is 200. */}
};

/* Vector for routing table.  */
vector vrf_vector;

/* Allocate new VRF.  */
struct vrf *
vrf_alloc (char *name)
{
  struct vrf *vrf;

  vrf = XCALLOC (MTYPE_VRF, sizeof (struct vrf));

  /* Put name.  */
  if (name)
    vrf->name = XSTRDUP (MTYPE_VRF_NAME, name);

  /* Allocate routing table and static table.  */
  vrf->table[AFI_IP][SAFI_UNICAST] = route_table_init ();
  vrf->table[AFI_IP6][SAFI_UNICAST] = route_table_init ();
  vrf->stable[AFI_IP][SAFI_UNICAST] = route_table_init ();
  vrf->stable[AFI_IP6][SAFI_UNICAST] = route_table_init ();

  return vrf;
}

/* Free VRF.  */
void
vrf_free (struct vrf *vrf)
{
  if (vrf->name)
    XFREE (MTYPE_VRF_NAME, vrf->name);
  XFREE (MTYPE_VRF, vrf);
}

/* Lookup VRF by identifier.  */
struct vrf *
vrf_lookup (u_int32_t id)
{
  return vector_lookup (vrf_vector, id);
}

/* Lookup VRF by name.  */
struct vrf *
vrf_lookup_by_name (char *name)
{
  int i;
  struct vrf *vrf;

  for (i = 0; i < vector_max (vrf_vector); i++)
    if ((vrf = vector_slot (vrf_vector, i)) != NULL)
      if (vrf->name && name && strcmp (vrf->name, name) == 0)
	return vrf;
  return NULL;
}

/* Initialize VRF.  */
void
vrf_init ()
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

  return vrf->stable[afi][safi];
}

/* Add nexthop to the end of the list.  */
void
nexthop_add (struct rib *rib, struct nexthop *nexthop)
{
  struct nexthop *last;

  for (last = rib->nexthop; last && last->next; last = last->next)
    ;
  if (last)
    last->next = nexthop;
  else
    rib->nexthop = nexthop;
  nexthop->prev = last;

  rib->nexthop_num++;
}

/* Delete specified nexthop from the list. */
void
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

/* Free nexthop. */
void
nexthop_free (struct nexthop *nexthop)
{
  if (nexthop->type == NEXTHOP_TYPE_IFNAME && nexthop->ifname)
    free (nexthop->ifname);
  XFREE (MTYPE_NEXTHOP, nexthop);
}

struct nexthop *
nexthop_ifindex_add (struct rib *rib, unsigned int ifindex)
{
  struct nexthop *nexthop;

  nexthop = XMALLOC (MTYPE_NEXTHOP, sizeof (struct nexthop));
  memset (nexthop, 0, sizeof (struct nexthop));
  nexthop->type = NEXTHOP_TYPE_IFINDEX;
  nexthop->ifindex = ifindex;

  nexthop_add (rib, nexthop);

  return nexthop;
}

struct nexthop *
nexthop_ifname_add (struct rib *rib, char *ifname)
{
  struct nexthop *nexthop;

  nexthop = XMALLOC (MTYPE_NEXTHOP, sizeof (struct nexthop));
  memset (nexthop, 0, sizeof (struct nexthop));
  nexthop->type = NEXTHOP_TYPE_IFNAME;
  nexthop->ifname = strdup (ifname);

  nexthop_add (rib, nexthop);

  return nexthop;
}

struct nexthop *
nexthop_ipv4_add (struct rib *rib, struct in_addr *ipv4)
{
  struct nexthop *nexthop;

  nexthop = XMALLOC (MTYPE_NEXTHOP, sizeof (struct nexthop));
  memset (nexthop, 0, sizeof (struct nexthop));
  nexthop->type = NEXTHOP_TYPE_IPV4;
  nexthop->gate.ipv4 = *ipv4;

  nexthop_add (rib, nexthop);

  return nexthop;
}

struct nexthop *
nexthop_ipv4_ifindex_add (struct rib *rib, struct in_addr *ipv4, 
			  unsigned int ifindex)
{
  struct nexthop *nexthop;

  nexthop = XMALLOC (MTYPE_NEXTHOP, sizeof (struct nexthop));
  memset (nexthop, 0, sizeof (struct nexthop));
  nexthop->type = NEXTHOP_TYPE_IPV4_IFINDEX;
  nexthop->gate.ipv4 = *ipv4;
  nexthop->ifindex = ifindex;

  nexthop_add (rib, nexthop);

  return nexthop;
}

#ifdef HAVE_IPV6
struct nexthop *
nexthop_ipv6_add (struct rib *rib, struct in6_addr *ipv6)
{
  struct nexthop *nexthop;

  nexthop = XMALLOC (MTYPE_NEXTHOP, sizeof (struct nexthop));
  memset (nexthop, 0, sizeof (struct nexthop));
  nexthop->type = NEXTHOP_TYPE_IPV6;
  nexthop->gate.ipv6 = *ipv6;

  nexthop_add (rib, nexthop);

  return nexthop;
}

struct nexthop *
nexthop_ipv6_ifname_add (struct rib *rib, struct in6_addr *ipv6,
			 char *ifname)
{
  struct nexthop *nexthop;

  nexthop = XMALLOC (MTYPE_NEXTHOP, sizeof (struct nexthop));
  memset (nexthop, 0, sizeof (struct nexthop));
  nexthop->type = NEXTHOP_TYPE_IPV6_IFNAME;
  nexthop->gate.ipv6 = *ipv6;
  nexthop->ifname = XSTRDUP (0, ifname);

  nexthop_add (rib, nexthop);

  return nexthop;
}

struct nexthop *
nexthop_ipv6_ifindex_add (struct rib *rib, struct in6_addr *ipv6,
			  unsigned int ifindex)
{
  struct nexthop *nexthop;

  nexthop = XMALLOC (MTYPE_NEXTHOP, sizeof (struct nexthop));
  memset (nexthop, 0, sizeof (struct nexthop));
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

  nexthop = XMALLOC (MTYPE_NEXTHOP, sizeof (struct nexthop));
  memset (nexthop, 0, sizeof (struct nexthop));
  nexthop->type = NEXTHOP_TYPE_BLACKHOLE;
  SET_FLAG (rib->flags, ZEBRA_FLAG_BLACKHOLE);

  nexthop_add (rib, nexthop);

  return nexthop;
}

/* If force flag is not set, do not modify falgs at all for uninstall
   the route from FIB. */
int
nexthop_active_ipv4 (struct rib *rib, struct nexthop *nexthop, int set,
		     struct route_node *top)
{
  struct prefix_ipv4 p;
  struct route_table *table;
  struct route_node *rn;
  struct rib *match;
  struct nexthop *newhop;

  if (nexthop->type == NEXTHOP_TYPE_IPV4)
    nexthop->ifindex = 0;

  if (set)
    UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE);

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
      
      /* If lookup self prefix return immidiately. */
      if (rn == top)
	return 0;

      /* Pick up selected route. */
      for (match = rn->info; match; match = match->next)
	if (CHECK_FLAG (match->flags, ZEBRA_FLAG_SELECTED))
	  break;

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
	    {
	      /* Directly point connected route. */
	      newhop = match->nexthop;
	      if (newhop && nexthop->type == NEXTHOP_TYPE_IPV4)
		nexthop->ifindex = newhop->ifindex;
	      
	      return 1;
	    }
	  else if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_INTERNAL))
	    {
	      for (newhop = match->nexthop; newhop; newhop = newhop->next)
		if (CHECK_FLAG (newhop->flags, NEXTHOP_FLAG_FIB)
		    && ! CHECK_FLAG (newhop->flags, NEXTHOP_FLAG_RECURSIVE))
		  {
		    if (set)
		      {
			SET_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE);
			nexthop->rtype = newhop->type;
			if (newhop->type == NEXTHOP_TYPE_IPV4 ||
			    newhop->type == NEXTHOP_TYPE_IPV4_IFINDEX)
			  nexthop->rgate.ipv4 = newhop->gate.ipv4;
			if (newhop->type == NEXTHOP_TYPE_IFINDEX
			    || newhop->type == NEXTHOP_TYPE_IFNAME
			    || newhop->type == NEXTHOP_TYPE_IPV4_IFINDEX)
			  nexthop->rifindex = newhop->ifindex;
		      }
		    return 1;
		  }
	      return 0;
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
int
nexthop_active_ipv6 (struct rib *rib, struct nexthop *nexthop, int set,
		     struct route_node *top)
{
  struct prefix_ipv6 p;
  struct route_table *table;
  struct route_node *rn;
  struct rib *match;
  struct nexthop *newhop;

  if (nexthop->type == NEXTHOP_TYPE_IPV6)
    nexthop->ifindex = 0;

  if (set)
    UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE);

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
      
      /* If lookup self prefix return immidiately. */
      if (rn == top)
	return 0;

      /* Pick up selected route. */
      for (match = rn->info; match; match = match->next)
	if (CHECK_FLAG (match->flags, ZEBRA_FLAG_SELECTED))
	  break;

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
	    {
	      /* Directly point connected route. */
	      newhop = match->nexthop;

	      if (newhop && nexthop->type == NEXTHOP_TYPE_IPV6)
		nexthop->ifindex = newhop->ifindex;
	      
	      return 1;
	    }
	  else if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_INTERNAL))
	    {
	      for (newhop = match->nexthop; newhop; newhop = newhop->next)
		if (CHECK_FLAG (newhop->flags, NEXTHOP_FLAG_FIB)
		    && ! CHECK_FLAG (newhop->flags, NEXTHOP_FLAG_RECURSIVE))
		  {
		    if (set)
		      {
			SET_FLAG (nexthop->flags, NEXTHOP_FLAG_RECURSIVE);
			nexthop->rtype = newhop->type;
			if (newhop->type == NEXTHOP_TYPE_IPV6
			    || newhop->type == NEXTHOP_TYPE_IPV6_IFINDEX
			    || newhop->type == NEXTHOP_TYPE_IPV6_IFNAME)
			  nexthop->rgate.ipv6 = newhop->gate.ipv6;
			if (newhop->type == NEXTHOP_TYPE_IFINDEX
			    || newhop->type == NEXTHOP_TYPE_IFNAME
			    || newhop->type == NEXTHOP_TYPE_IPV6_IFINDEX
			    || newhop->type == NEXTHOP_TYPE_IPV6_IFNAME)
			  nexthop->rifindex = newhop->ifindex;
		      }
		    return 1;
		  }
	      return 0;
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
rib_match_ipv4 (struct in_addr addr)
{
  struct prefix_ipv4 p;
  struct route_table *table;
  struct route_node *rn;
  struct rib *match;
  struct nexthop *newhop;

  /* Lookup table.  */
  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return 0;

  memset (&p, 0, sizeof (struct prefix_ipv4));
  p.family = AF_INET;
  p.prefixlen = IPV4_MAX_PREFIXLEN;
  p.prefix = addr;

  rn = route_node_match (table, (struct prefix *) &p);

  while (rn)
    {
      route_unlock_node (rn);
      
      /* Pick up selected route. */
      for (match = rn->info; match; match = match->next)
	if (CHECK_FLAG (match->flags, ZEBRA_FLAG_SELECTED))
	  break;

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
	      for (newhop = match->nexthop; newhop; newhop = newhop->next)
		if (CHECK_FLAG (newhop->flags, NEXTHOP_FLAG_FIB))
		  return match;
	      return NULL;
	    }
	}
    }
  return NULL;
}

struct rib *
rib_lookup_ipv4 (struct prefix_ipv4 *p)
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *match;
  struct nexthop *nexthop;

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

  /* Pick up selected route. */
  for (match = rn->info; match; match = match->next)
    if (CHECK_FLAG (match->flags, ZEBRA_FLAG_SELECTED))
      break;

  if (! match || match->type == ZEBRA_ROUTE_BGP)
    return NULL;

  if (match->type == ZEBRA_ROUTE_CONNECT)
    return match;
  
  for (nexthop = match->nexthop; nexthop; nexthop = nexthop->next)
    if (CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB))
      return match;

  return NULL;
}

#ifdef HAVE_IPV6
struct rib *
rib_match_ipv6 (struct in6_addr *addr)
{
  struct prefix_ipv6 p;
  struct route_table *table;
  struct route_node *rn;
  struct rib *match;
  struct nexthop *newhop;

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
      for (match = rn->info; match; match = match->next)
	if (CHECK_FLAG (match->flags, ZEBRA_FLAG_SELECTED))
	  break;

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
	      for (newhop = match->nexthop; newhop; newhop = newhop->next)
		if (CHECK_FLAG (newhop->flags, NEXTHOP_FLAG_FIB))
		  return match;
	      return NULL;
	    }
	}
    }
  return NULL;
}
#endif /* HAVE_IPV6 */

int
nexthop_active_check (struct route_node *rn, struct rib *rib,
		      struct nexthop *nexthop, int set)
{
  struct interface *ifp;

  switch (nexthop->type)
    {
    case NEXTHOP_TYPE_IFINDEX:
      ifp = if_lookup_by_index (nexthop->ifindex);
      if (ifp && if_is_up (ifp))
	SET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
      else
	UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
      break;
    case NEXTHOP_TYPE_IFNAME:
    case NEXTHOP_TYPE_IPV6_IFNAME:
      ifp = if_lookup_by_name (nexthop->ifname);
      if (ifp && if_is_up (ifp))
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
      if (nexthop_active_ipv4 (rib, nexthop, set, rn))
	SET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
      else
	UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
      break;
#ifdef HAVE_IPV6
    case NEXTHOP_TYPE_IPV6:
      if (nexthop_active_ipv6 (rib, nexthop, set, rn))
	SET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
      else
	UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
      break;
    case NEXTHOP_TYPE_IPV6_IFINDEX:
      if (IN6_IS_ADDR_LINKLOCAL (&nexthop->gate.ipv6))
	{
	  ifp = if_lookup_by_index (nexthop->ifindex);
	  if (ifp && if_is_up (ifp))
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
  return CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
}

int
nexthop_active_update (struct route_node *rn, struct rib *rib, int set)
{
  struct nexthop *nexthop;
  int active;

  rib->nexthop_active_num = 0;
  UNSET_FLAG (rib->flags, ZEBRA_FLAG_CHANGED);

  for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
    {
      active = CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE);
      rib->nexthop_active_num += nexthop_active_check (rn, rib, nexthop, set);
      if (active != CHECK_FLAG (nexthop->flags, NEXTHOP_FLAG_ACTIVE))
	SET_FLAG (rib->flags, ZEBRA_FLAG_CHANGED);
    }
  return rib->nexthop_active_num;
}

#define RIB_SYSTEM_ROUTE(R) \
        ((R)->type == ZEBRA_ROUTE_KERNEL || (R)->type == ZEBRA_ROUTE_CONNECT)

void
newrib_free (struct rib *rib)
{
  struct nexthop *nexthop;
  struct nexthop *next;

  for (nexthop = rib->nexthop; nexthop; nexthop = next)
    {
      next = nexthop->next;
      nexthop_free (nexthop);
    }
  XFREE (MTYPE_RIB, rib);
}

void
rib_install_kernel (struct route_node *rn, struct rib *rib)
{
  int ret = 0;
  struct nexthop *nexthop;

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

  if (ret < 0)
    {
      for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
	UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB);
    }
}

/* Uninstall the route from kernel. */
int
rib_uninstall_kernel (struct route_node *rn, struct rib *rib)
{
  int ret = 0;
  struct nexthop *nexthop;

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

  for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
    UNSET_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB);

  return ret;
}

/* Uninstall the route from kernel. */
void
rib_uninstall (struct route_node *rn, struct rib *rib)
{
  if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELECTED))
    {
      redistribute_delete (&rn->p, rib);
      if (! RIB_SYSTEM_ROUTE (rib))
	rib_uninstall_kernel (rn, rib);
      UNSET_FLAG (rib->flags, ZEBRA_FLAG_SELECTED);
    }
}

/* Core function for processing routing information base. */
void
rib_process (struct route_node *rn, struct rib *del)
{
  struct rib *rib;
  struct rib *next;
  struct rib *fib = NULL;
  struct rib *select = NULL;

  for (rib = rn->info; rib; rib = next)
    {
      next = rib->next;

      /* Currently installed rib. */
      if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELECTED))
	fib = rib;

      /* Skip unreachable nexthop. */
      if (! nexthop_active_update (rn, rib, 0))
	continue;

      /* Infinit distance. */
      if (rib->distance == DISTANCE_INFINITY)
	continue;

      /* Newly selected rib. */
      if (! select || rib->distance < select->distance 
	  || rib->type == ZEBRA_ROUTE_CONNECT)
	select = rib;
    }

  /* Deleted route check. */
  if (del && CHECK_FLAG (del->flags, ZEBRA_FLAG_SELECTED))
    fib = del;

  /* Same route is selected. */
  if (select && select == fib)
    {
      if (CHECK_FLAG (select->flags, ZEBRA_FLAG_CHANGED))
	{
	  redistribute_delete (&rn->p, select);
	  if (! RIB_SYSTEM_ROUTE (select))
	    rib_uninstall_kernel (rn, select);

	  /* Set real nexthop. */
	  nexthop_active_update (rn, select, 1);
  
	  if (! RIB_SYSTEM_ROUTE (select))
	    rib_install_kernel (rn, select);
	  redistribute_add (&rn->p, select);
	}
      return;
    }

  /* Uninstall old rib from forwarding table. */
  if (fib)
    {
      redistribute_delete (&rn->p, fib);
      if (! RIB_SYSTEM_ROUTE (fib))
	rib_uninstall_kernel (rn, fib);
      UNSET_FLAG (fib->flags, ZEBRA_FLAG_SELECTED);

      /* Set real nexthop. */
      nexthop_active_update (rn, fib, 1);
    }

  /* Install new rib into forwarding table. */
  if (select)
    {
      /* Set real nexthop. */
      nexthop_active_update (rn, select, 1);

      if (! RIB_SYSTEM_ROUTE (select))
	rib_install_kernel (rn, select);
      SET_FLAG (select->flags, ZEBRA_FLAG_SELECTED);
      redistribute_add (&rn->p, select);
    }
}

/* Add RIB to head of the route node. */
void
rib_addnode (struct route_node *rn, struct rib *rib)
{
  struct rib *head;

  head = rn->info;
  if (head)
    head->prev = rib;
  rib->next = head;
  rn->info = rib;
}

void
rib_delnode (struct route_node *rn, struct rib *rib)
{
  if (rib->next)
    rib->next->prev = rib->prev;
  if (rib->prev)
    rib->prev->next = rib->next;
  else
    rn->info = rib->next;
}

int
rib_add_ipv4 (int type, int flags, struct prefix_ipv4 *p, 
	      struct in_addr *gate, unsigned int ifindex, u_int32_t vrf_id,
	      u_int32_t metric, u_char distance)
{
  struct rib *rib;
  struct rib *same = NULL;
  struct route_table *table;
  struct route_node *rn;
  struct nexthop *nexthop;

  /* Lookup table.  */
  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return 0;

  /* Make it sure prefixlen is applied to the prefix. */
  apply_mask_ipv4 (p);

  /* Set default distance by route type. */
  if (distance == 0)
    {
      distance = route_info[type].distance;

      /* iBGP distance is 200. */
      if (type == ZEBRA_ROUTE_BGP && CHECK_FLAG (flags, ZEBRA_FLAG_IBGP))
	distance = 200;
    }

  /* Lookup route node.*/
  rn = route_node_get (table, (struct prefix *) p);

  /* If same type of route are installed, treat it as a implicit
     withdraw. */
  for (rib = rn->info; rib; rib = rib->next)
    {
      if (rib->type == ZEBRA_ROUTE_CONNECT)
	{
	  nexthop = rib->nexthop;

	  /* Duplicate connected route comes in. */
	  if (rib->type == type
	      && nexthop && nexthop->type == NEXTHOP_TYPE_IFINDEX
	      && nexthop->ifindex == ifindex)
	    {
	      rib->refcnt++;
	      return 0 ;
	    }
	}
      else if (rib->type == type)
	{
	  same = rib;
	  rib_delnode (rn, same);
	  route_unlock_node (rn);
	  break;
	}
    }

  /* Allocate new rib structure. */
  rib = XMALLOC (MTYPE_RIB, sizeof (struct rib));
  memset (rib, 0, sizeof (struct rib));
  rib->type = type;
  rib->distance = distance;
  rib->flags = flags;
  rib->metric = metric;
  rib->nexthop_num = 0;
  rib->uptime = time (NULL);

  /* Nexthop settings. */
  if (gate)
    {
      if (ifindex)
	nexthop_ipv4_ifindex_add (rib, gate, ifindex);
      else
	nexthop_ipv4_add (rib, gate);
    }
  else
    nexthop_ifindex_add (rib, ifindex);

  /* If this route is kernel route, set FIB flag to the route. */
  if (type == ZEBRA_ROUTE_KERNEL || type == ZEBRA_ROUTE_CONNECT)
    for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
      SET_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB);

  /* Link new rib to node.*/
  rib_addnode (rn, rib);

  /* Process this route node. */
  rib_process (rn, same);

  /* Free implicit route.*/
  if (same)
    newrib_free (same);

  return 0;
}

int
rib_add_ipv4_multipath (struct prefix_ipv4 *p, struct rib *rib)
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *same;
  struct nexthop *nexthop;

  /* Lookup table.  */
  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
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
  for (same = rn->info; same; same = same->next)
    {
      if (same->type == rib->type && same->table == rib->table
	  && same->type != ZEBRA_ROUTE_CONNECT)
	{
	  rib_delnode (rn, same);
	  route_unlock_node (rn);
	  break;
	}
    }

  /* If this route is kernel route, set FIB flag to the route. */
  if (rib->type == ZEBRA_ROUTE_KERNEL || rib->type == ZEBRA_ROUTE_CONNECT)
    for (nexthop = rib->nexthop; nexthop; nexthop = nexthop->next)
      SET_FLAG (nexthop->flags, NEXTHOP_FLAG_FIB);

  /* Link new rib to node.*/
  rib_addnode (rn, rib);

  /* Process this route node. */
  rib_process (rn, same);

  /* Free implicit route.*/
  if (same)
    newrib_free (same);

  return 0;
}

int
rib_delete_ipv4 (int type, int flags, struct prefix_ipv4 *p,
		 struct in_addr *gate, unsigned int ifindex, u_int32_t vrf_id)
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  struct rib *fib = NULL;
  struct rib *same = NULL;
  struct nexthop *nexthop;
  char buf1[BUFSIZ];
  char buf2[BUFSIZ];

  /* Lookup table.  */
  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return 0;

  /* Apply mask. */
  apply_mask_ipv4 (p);

  /* Lookup route node. */
  rn = route_node_lookup (table, (struct prefix *) p);
  if (! rn)
    {
      if (IS_ZEBRA_DEBUG_KERNEL)
	{
	  if (gate)
	    zlog_info ("route %s/%d via %s ifindex %d doesn't exist in rib",
		       inet_ntop (AF_INET, &p->prefix, buf1, BUFSIZ),
		       p->prefixlen,
		       inet_ntop (AF_INET, gate, buf2, BUFSIZ),
		       ifindex);
	  else
	    zlog_info ("route %s/%d ifindex %d doesn't exist in rib",
		       inet_ntop (AF_INET, &p->prefix, buf1, BUFSIZ),
		       p->prefixlen,
		       ifindex);
	}
      return ZEBRA_ERR_RTNOEXIST;
    }

  /* Lookup same type route. */
  for (rib = rn->info; rib; rib = rib->next)
    {
      if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELECTED))
	fib = rib;

      if (rib->type == ZEBRA_ROUTE_CONNECT)
	{
	  nexthop = rib->nexthop;

	  if (rib->type == type
	      && nexthop && nexthop->type == NEXTHOP_TYPE_IFINDEX
	      && nexthop->ifindex == ifindex)
	    {
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
	}
      else
	{
	  if (rib->type == type)
	    {
	      same = rib;
	      break;
	    }
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
		zlog_info ("route %s/%d via %s ifindex %d type %d doesn't exist in rib",
			   inet_ntop (AF_INET, &p->prefix, buf1, BUFSIZ),
			   p->prefixlen,
			   inet_ntop (AF_INET, gate, buf2, BUFSIZ),
			   ifindex,
			   type);
	      else
		zlog_info ("route %s/%d ifindex %d type %d doesn't exist in rib",
			   inet_ntop (AF_INET, &p->prefix, buf1, BUFSIZ),
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

  /* Process changes. */
  rib_process (rn, same);

  if (same)
    {
      newrib_free (same);
      route_unlock_node (rn);
    }

  route_unlock_node (rn);

  return 0;
}

/* Install static route into rib. */
void
static_install_ipv4 (struct prefix *p, struct static_ipv4 *si)
{
  struct rib *rib;
  struct route_node *rn;
  struct route_table *table;

  /* Lookup table.  */
  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return;

  /* Lookup existing route */
  rn = route_node_get (table, p);
  for (rib = rn->info; rib; rib = rib->next)
    if (rib->type == ZEBRA_ROUTE_STATIC && rib->distance == si->distance)
      break;

  if (rib)
    {
      /* Same distance static route is there.  Update it with new
         nexthop. */
      rib_uninstall (rn, rib);
      route_unlock_node (rn);

      switch (si->type)
	{
	case STATIC_IPV4_GATEWAY:
	  nexthop_ipv4_add (rib, &si->gate.ipv4);
	  break;
	case STATIC_IPV4_IFNAME:
	  nexthop_ifname_add (rib, si->gate.ifname);
	  break;
	case STATIC_IPV4_BLACKHOLE:
	  nexthop_blackhole_add (rib);
	  break;
	}
      rib_process (rn, NULL);
    }
  else
    {
      /* This is new static route. */
      rib = XMALLOC (MTYPE_RIB, sizeof (struct rib));
      memset (rib, 0, sizeof (struct rib));

      rib->type = ZEBRA_ROUTE_STATIC;
      rib->distance = si->distance;
      rib->metric = 0;
      rib->nexthop_num = 0;

      switch (si->type)
	{
	case STATIC_IPV4_GATEWAY:
	  nexthop_ipv4_add (rib, &si->gate.ipv4);
	  break;
	case STATIC_IPV4_IFNAME:
	  nexthop_ifname_add (rib, si->gate.ifname);
	  break;
	case STATIC_IPV4_BLACKHOLE:
	  nexthop_blackhole_add (rib);
	  break;
	}

      /* Link this rib to the tree. */
      rib_addnode (rn, rib);

      /* Process this prefix. */
      rib_process (rn, NULL);
    }
}

int
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
  return 0;;
}

/* Uninstall static route from RIB. */
void
static_uninstall_ipv4 (struct prefix *p, struct static_ipv4 *si)
{
  struct route_node *rn;
  struct rib *rib;
  struct nexthop *nexthop;
  struct route_table *table;

  /* Lookup table.  */
  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (! table)
    return;

  /* Lookup existing route with type and distance. */
  rn = route_node_lookup (table, p);
  if (! rn)
    return;

  for (rib = rn->info; rib; rib = rib->next)
    if (rib->type == ZEBRA_ROUTE_STATIC && rib->distance == si->distance)
      break;

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
    {
      rib_delnode (rn, rib);
      rib_process (rn, rib);
      newrib_free (rib);
      route_unlock_node (rn);
    }
  else
    {
      rib_uninstall (rn, rib);
      nexthop_delete (rib, nexthop);
      nexthop_free (nexthop);
      rib_process (rn, rib);
    }

  /* Unlock node. */
  route_unlock_node (rn);
}

/* Add static route into static route configuration. */
int
static_add_ipv4 (struct prefix *p, struct in_addr *gate, char *ifname,
		 u_char distance, u_int32_t vrf_id)
{
  u_char type = 0;
  struct route_node *rn;
  struct static_ipv4 *si;
  struct static_ipv4 *pp;
  struct static_ipv4 *cp;
  struct static_ipv4 *update = NULL;
  struct route_table *stable;

  /* Lookup table.  */
  stable = vrf_static_table (AFI_IP, SAFI_UNICAST, vrf_id);
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

  /* Distance chaged.  */
  if (update)
    static_delete_ipv4 (p, gate, ifname, update->distance, vrf_id);

  /* Make new static route structure. */
  si = XMALLOC (MTYPE_STATIC_IPV4, sizeof (struct static_ipv4));
  memset (si, 0, sizeof (struct static_ipv4));

  si->type = type;
  si->distance = distance;

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
  static_install_ipv4 (p, si);

  return 1;
}

/* Delete static route from static route configuration. */
int
static_delete_ipv4 (struct prefix *p, struct in_addr *gate, char *ifname,
		    u_char distance, u_int32_t vrf_id)
{
  u_char type = 0;
  struct route_node *rn;
  struct static_ipv4 *si;
  struct route_table *stable;

  /* Lookup table.  */
  stable = vrf_static_table (AFI_IP, SAFI_UNICAST, vrf_id);
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
  static_uninstall_ipv4 (p, si);

  /* Unlink static route from linked list. */
  if (si->prev)
    si->prev->next = si->next;
  else
    rn->info = si->next;
  if (si->next)
    si->next->prev = si->prev;
  
  /* Free static route configuration. */
  XFREE (MTYPE_STATIC_IPV4, si);

  return 1;
}


#ifdef HAVE_IPV6
int
rib_bogus_ipv6 (int type, struct prefix_ipv6 *p,
		struct in6_addr *gate, unsigned int ifindex, int table)
{
  if (type == ZEBRA_ROUTE_CONNECT && IN6_IS_ADDR_UNSPECIFIED (&p->prefix))
    return 1;
  if (type == ZEBRA_ROUTE_KERNEL && IN6_IS_ADDR_UNSPECIFIED (&p->prefix)
      && p->prefixlen == 96 && gate && IN6_IS_ADDR_UNSPECIFIED (gate))
    {
      kernel_delete_ipv6_old (p, gate, ifindex, 0, table);
      return 1;
    }
  return 0;
}

int
rib_add_ipv6 (int type, int flags, struct prefix_ipv6 *p,
	      struct in6_addr *gate, unsigned int ifindex, u_int32_t vrf_id)
{
  struct rib *rib;
  struct rib *same = NULL;
  struct route_table *table;
  struct route_node *rn;
  struct nexthop *nexthop;

  int distance;
  u_int32_t metric = 0;

  /* Lookup table.  */
  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (! table)
    return 0;

  /* Make sure mask is applied. */
  apply_mask_ipv6 (p);

  /* Set default distance by route type. */
  distance = route_info[type].distance;
  
  if (type == ZEBRA_ROUTE_BGP && CHECK_FLAG (flags, ZEBRA_FLAG_IBGP))
    distance = 200;

  /* Filter bogus route. */
  if (rib_bogus_ipv6 (type, p, gate, ifindex, 0))
    return 0;

  /* Lookup route node.*/
  rn = route_node_get (table, (struct prefix *) p);

  /* If same type of route are installed, treat it as a implicit
     withdraw. */
  for (rib = rn->info; rib; rib = rib->next)
    {
      if (rib->type == ZEBRA_ROUTE_CONNECT)
	{
	  nexthop = rib->nexthop;

	  if (rib->type == type
	      && nexthop && nexthop->type == NEXTHOP_TYPE_IFINDEX
	      && nexthop->ifindex == ifindex)
	  {
	    rib->refcnt++;
	    return 0;
	  }
	}
      else if (rib->type == type)
	{
	  same = rib;
	  rib_delnode (rn, same);
	  route_unlock_node (rn);
	  break;
	}
    }

  /* Allocate new rib structure. */
  rib = XMALLOC (MTYPE_RIB, sizeof (struct rib));
  memset (rib, 0, sizeof (struct rib));
  rib->type = type;
  rib->distance = distance;
  rib->flags = flags;
  rib->metric = metric;
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

  /* Process this route node. */
  rib_process (rn, same);

  /* Free implicit route.*/
  if (same)
    newrib_free (same);

  return 0;
}

int
rib_delete_ipv6 (int type, int flags, struct prefix_ipv6 *p,
		 struct in6_addr *gate, unsigned int ifindex, u_int32_t vrf_id)
{
  struct route_table *table;
  struct route_node *rn;
  struct rib *rib;
  struct rib *fib = NULL;
  struct rib *same = NULL;
  struct nexthop *nexthop;
  char buf1[BUFSIZ];
  char buf2[BUFSIZ];

  /* Apply mask. */
  apply_mask_ipv6 (p);

  /* Lookup table.  */
  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (! table)
    return 0;

  /* Lookup route node. */
  rn = route_node_lookup (table, (struct prefix *) p);
  if (! rn)
    {
      if (IS_ZEBRA_DEBUG_KERNEL)
	{
	  if (gate)
	    zlog_info ("route %s/%d via %s ifindex %d doesn't exist in rib",
		       inet_ntop (AF_INET6, &p->prefix, buf1, BUFSIZ),
		       p->prefixlen,
		       inet_ntop (AF_INET6, gate, buf2, BUFSIZ),
		       ifindex);
	  else
	    zlog_info ("route %s/%d ifindex %d doesn't exist in rib",
		       inet_ntop (AF_INET6, &p->prefix, buf1, BUFSIZ),
		       p->prefixlen,
		       ifindex);
	}
      return ZEBRA_ERR_RTNOEXIST;
    }

  /* Lookup same type route. */
  for (rib = rn->info; rib; rib = rib->next)
    {
      if (CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELECTED))
	fib = rib;

      if (rib->type == ZEBRA_ROUTE_CONNECT)
	{
	  nexthop = rib->nexthop;

	  if (rib->type == type
	      && nexthop && nexthop->type == NEXTHOP_TYPE_IFINDEX
	      && nexthop->ifindex == ifindex)
	    {
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
	}
      else
	{
	  if (rib->type == type)
	    {
	      same = rib;
	      break;
	    }
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
		zlog_info ("route %s/%d via %s ifindex %d type %d doesn't exist in rib",
			   inet_ntop (AF_INET6, &p->prefix, buf1, BUFSIZ),
			   p->prefixlen,
			   inet_ntop (AF_INET6, gate, buf2, BUFSIZ),
			   ifindex,
			   type);
	      else
		zlog_info ("route %s/%d ifindex %d type %d doesn't exist in rib",
			   inet_ntop (AF_INET6, &p->prefix, buf1, BUFSIZ),
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

  /* Process changes. */
  rib_process (rn, same);

  if (same)
    {
      newrib_free (same);
      route_unlock_node (rn);
    }

  route_unlock_node (rn);

  return 0;
}

/* Install static route into rib. */
void
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
  for (rib = rn->info; rib; rib = rib->next)
    if (rib->type == ZEBRA_ROUTE_STATIC && rib->distance == si->distance)
      break;

  if (rib)
    {
      /* Same distance static route is there.  Update it with new
         nexthop. */
      rib_uninstall (rn, rib);
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
	case STATIC_IPV6_BLACKHOLE:
	  nexthop_blackhole_add (rib);
	  break;
	}
      rib_process (rn, NULL);
    }
  else
    {
      /* This is new static route. */
      rib = XMALLOC (MTYPE_RIB, sizeof (struct rib));
      memset (rib, 0, sizeof (struct rib));

      rib->type = ZEBRA_ROUTE_STATIC;
      rib->distance = si->distance;
      rib->metric = 0;
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
	case STATIC_IPV6_BLACKHOLE:
	  nexthop_blackhole_add (rib);
	  break;
	}

      /* Link this rib to the tree. */
      rib_addnode (rn, rib);

      /* Process this prefix. */
      rib_process (rn, NULL);
    }
}

int
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
  if (nexthop->type == NEXTHOP_TYPE_BLACKHOLE
      && si->type == STATIC_IPV6_BLACKHOLE)
    return 1;
  return 0;;
}

void
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

  for (rib = rn->info; rib; rib = rib->next)
    if (rib->type == ZEBRA_ROUTE_STATIC && rib->distance == si->distance)
      break;
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
      rib_process (rn, rib);
      newrib_free (rib);
      route_unlock_node (rn);
    }
  else
    {
      rib_uninstall (rn, rib);
      nexthop_delete (rib, nexthop);
      nexthop_free (nexthop);
      rib_process (rn, rib);
    }

  /* Unlock node. */
  route_unlock_node (rn);
}

/* Add static route into static route configuration. */
int
static_add_ipv6 (struct prefix *p, u_char type, struct in6_addr *gate,
		 char *ifname, u_char distance, u_int32_t vrf_id)
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
  si = XMALLOC (MTYPE_STATIC_IPV6, sizeof (struct static_ipv6));
  memset (si, 0, sizeof (struct static_ipv6));

  si->type = type;
  si->distance = distance;

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
		    char *ifname, u_char distance, u_int32_t vrf_id)
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
  XFREE (MTYPE_STATIC_IPV6, si);

  return 1;
}
#endif /* HAVE_IPV6 */

/* RIB update function. */
void
rib_update ()
{
  struct route_node *rn;
  struct route_table *table;

  table = vrf_table (AFI_IP, SAFI_UNICAST, 0);
  if (table)
    for (rn = route_top (table); rn; rn = route_next (rn))
      rib_process (rn, NULL);

  table = vrf_table (AFI_IP6, SAFI_UNICAST, 0);
  if (table)
    for (rn = route_top (table); rn; rn = route_next (rn))
      rib_process (rn, NULL);
}

/* Interface goes up. */
void
rib_if_up (struct interface *ifp)
{
  rib_update ();
}

/* Interface goes down. */
void
rib_if_down (struct interface *ifp)
{
  rib_update ();
}

/* Remove all routes which comes from non main table.  */
void
rib_weed_table (struct route_table *table)
{
  struct route_node *rn;
  struct rib *rib;
  struct rib *next;

  if (table)
    for (rn = route_top (table); rn; rn = route_next (rn))
      for (rib = rn->info; rib; rib = next)
	{
	  next = rib->next;

	  if (rib->table != rtm_table_default &&
	      rib->table != RT_TABLE_MAIN)
	    {
	      rib_delnode (rn, rib);
	      newrib_free (rib);
	      route_unlock_node (rn);
	    }
	}
}

/* Delete all routes from non main table. */
void
rib_weed_tables ()
{
  rib_weed_table (vrf_table (AFI_IP, SAFI_UNICAST, 0));
  rib_weed_table (vrf_table (AFI_IP6, SAFI_UNICAST, 0));
}

/* Delete self installed routes after zebra is relaunched.  */
void
rib_sweep_table (struct route_table *table)
{
  struct route_node *rn;
  struct rib *rib;
  struct rib *next;
  int ret = 0;

  if (table)
    for (rn = route_top (table); rn; rn = route_next (rn))
      for (rib = rn->info; rib; rib = next)
	{
	  next = rib->next;

	  if (rib->type == ZEBRA_ROUTE_KERNEL && 
	      CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELFROUTE))
	    {
	      ret = rib_uninstall_kernel (rn, rib);
	      if (! ret)
		{
		  rib_delnode (rn, rib);
		  newrib_free (rib);
		  route_unlock_node (rn);
		}
	    }
	}
}

/* Sweep all RIB tables.  */
void
rib_sweep_route ()
{
  rib_sweep_table (vrf_table (AFI_IP, SAFI_UNICAST, 0));
  rib_sweep_table (vrf_table (AFI_IP6, SAFI_UNICAST, 0));
}

/* Close RIB and clean up kernel routes. */
void
rib_close_table (struct route_table *table)
{
  struct route_node *rn;
  struct rib *rib;

  if (table)
    for (rn = route_top (table); rn; rn = route_next (rn))
      for (rib = rn->info; rib; rib = rib->next)
	if (! RIB_SYSTEM_ROUTE (rib)
	    && CHECK_FLAG (rib->flags, ZEBRA_FLAG_SELECTED))
	  rib_uninstall_kernel (rn, rib);
}

/* Close all RIB tables.  */
void
rib_close ()
{
  rib_close_table (vrf_table (AFI_IP, SAFI_UNICAST, 0));
  rib_close_table (vrf_table (AFI_IP6, SAFI_UNICAST, 0));
}

/* Routing information base initialize. */
void
rib_init ()
{
  /* VRF initialization.  */
  vrf_init ();
}
