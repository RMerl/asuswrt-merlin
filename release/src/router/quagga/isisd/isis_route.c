/*
 * IS-IS Rout(e)ing protocol               - isis_route.c
 * Copyright (C) 2001,2002   Sampo Saaristo
 *                           Tampere University of Technology      
 *                           Institute of Communications Engineering
 *
 *                                         based on ../ospf6d/ospf6_route.[ch]
 *                                         by Yasuhiro Ohara
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public Licenseas published by the Free 
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
 * more details.

 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <zebra.h>

#include "thread.h"
#include "linklist.h"
#include "vty.h"
#include "log.h"
#include "memory.h"
#include "prefix.h"
#include "hash.h"
#include "if.h"
#include "table.h"

#include "isis_constants.h"
#include "isis_common.h"
#include "isis_flags.h"
#include "dict.h"
#include "isisd.h"
#include "isis_misc.h"
#include "isis_adjacency.h"
#include "isis_circuit.h"
#include "isis_tlv.h"
#include "isis_pdu.h"
#include "isis_lsp.h"
#include "isis_spf.h"
#include "isis_route.h"
#include "isis_zebra.h"

static struct isis_nexthop *
isis_nexthop_create (struct in_addr *ip, unsigned int ifindex)
{
  struct listnode *node;
  struct isis_nexthop *nexthop;

  for (ALL_LIST_ELEMENTS_RO (isis->nexthops, node, nexthop))
    {
      if (nexthop->ifindex != ifindex)
	continue;
      if (ip && memcmp (&nexthop->ip, ip, sizeof (struct in_addr)) != 0)
	continue;

      nexthop->lock++;
      return nexthop;
    }

  nexthop = XCALLOC (MTYPE_ISIS_NEXTHOP, sizeof (struct isis_nexthop));
  if (!nexthop)
    {
      zlog_err ("ISIS-Rte: isis_nexthop_create: out of memory!");
    }

  nexthop->ifindex = ifindex;
  memcpy (&nexthop->ip, ip, sizeof (struct in_addr));
  listnode_add (isis->nexthops, nexthop);
  nexthop->lock++;

  return nexthop;
}

static void
isis_nexthop_delete (struct isis_nexthop *nexthop)
{
  nexthop->lock--;
  if (nexthop->lock == 0)
    {
      listnode_delete (isis->nexthops, nexthop);
      XFREE (MTYPE_ISIS_NEXTHOP, nexthop);
    }

  return;
}

static int
nexthoplookup (struct list *nexthops, struct in_addr *ip,
	       unsigned int ifindex)
{
  struct listnode *node;
  struct isis_nexthop *nh;

  for (ALL_LIST_ELEMENTS_RO (nexthops, node, nh))
    {
      if (!(memcmp (ip, &nh->ip, sizeof (struct in_addr))) &&
	  ifindex == nh->ifindex)
	return 1;
    }

  return 0;
}

#ifdef EXTREME_DEBUG
static void
nexthop_print (struct isis_nexthop *nh)
{
  u_char buf[BUFSIZ];

  inet_ntop (AF_INET, &nh->ip, (char *) buf, BUFSIZ);

  zlog_debug ("      %s %u", buf, nh->ifindex);
}

static void
nexthops_print (struct list *nhs)
{
  struct listnode *node;
  struct isis_nexthop *nh;

  for (ALL_LIST_ELEMENTS_RO (nhs, node, nh))
    nexthop_print (nh);
}
#endif /* EXTREME_DEBUG */

#ifdef HAVE_IPV6
static struct isis_nexthop6 *
isis_nexthop6_new (struct in6_addr *ip6, unsigned int ifindex)
{
  struct isis_nexthop6 *nexthop6;

  nexthop6 = XCALLOC (MTYPE_ISIS_NEXTHOP6, sizeof (struct isis_nexthop6));
  if (!nexthop6)
    {
      zlog_err ("ISIS-Rte: isis_nexthop_create6: out of memory!");
    }

  nexthop6->ifindex = ifindex;
  memcpy (&nexthop6->ip6, ip6, sizeof (struct in6_addr));
  nexthop6->lock++;

  return nexthop6;
}

static struct isis_nexthop6 *
isis_nexthop6_create (struct in6_addr *ip6, unsigned int ifindex)
{
  struct listnode *node;
  struct isis_nexthop6 *nexthop6;

  for (ALL_LIST_ELEMENTS_RO (isis->nexthops6, node, nexthop6))
    {
      if (nexthop6->ifindex != ifindex)
	continue;
      if (ip6 && memcmp (&nexthop6->ip6, ip6, sizeof (struct in6_addr)) != 0)
	continue;

      nexthop6->lock++;
      return nexthop6;
    }

  nexthop6 = isis_nexthop6_new (ip6, ifindex);

  return nexthop6;
}

static void
isis_nexthop6_delete (struct isis_nexthop6 *nexthop6)
{

  nexthop6->lock--;
  if (nexthop6->lock == 0)
    {
      listnode_delete (isis->nexthops6, nexthop6);
      XFREE (MTYPE_ISIS_NEXTHOP6, nexthop6);
    }

  return;
}

static int
nexthop6lookup (struct list *nexthops6, struct in6_addr *ip6,
		unsigned int ifindex)
{
  struct listnode *node;
  struct isis_nexthop6 *nh6;

  for (ALL_LIST_ELEMENTS_RO (nexthops6, node, nh6))
    {
      if (!(memcmp (ip6, &nh6->ip6, sizeof (struct in6_addr))) &&
	  ifindex == nh6->ifindex)
	return 1;
    }

  return 0;
}

#ifdef EXTREME_DEBUG
static void
nexthop6_print (struct isis_nexthop6 *nh6)
{
  u_char buf[BUFSIZ];

  inet_ntop (AF_INET6, &nh6->ip6, (char *) buf, BUFSIZ);

  zlog_debug ("      %s %u", buf, nh6->ifindex);
}

static void
nexthops6_print (struct list *nhs6)
{
  struct listnode *node;
  struct isis_nexthop6 *nh6;

  for (ALL_LIST_ELEMENTS_RO (nhs6, node, nh6))
    nexthop6_print (nh6);
}
#endif /* EXTREME_DEBUG */
#endif /* HAVE_IPV6 */

static void
adjinfo2nexthop (struct list *nexthops, struct isis_adjacency *adj)
{
  struct isis_nexthop *nh;
  struct listnode *node;
  struct in_addr *ipv4_addr;

  if (adj->ipv4_addrs == NULL)
    return;

  for (ALL_LIST_ELEMENTS_RO (adj->ipv4_addrs, node, ipv4_addr))
    {
      if (!nexthoplookup (nexthops, ipv4_addr,
			  adj->circuit->interface->ifindex))
	{
	  nh = isis_nexthop_create (ipv4_addr,
				    adj->circuit->interface->ifindex);
          nh->router_address = adj->router_address;
	  listnode_add (nexthops, nh);
	}
    }
}

#ifdef HAVE_IPV6
static void
adjinfo2nexthop6 (struct list *nexthops6, struct isis_adjacency *adj)
{
  struct listnode *node;
  struct in6_addr *ipv6_addr;
  struct isis_nexthop6 *nh6;

  if (!adj->ipv6_addrs)
    return;

  for (ALL_LIST_ELEMENTS_RO (adj->ipv6_addrs, node, ipv6_addr))
    {
      if (!nexthop6lookup (nexthops6, ipv6_addr,
			   adj->circuit->interface->ifindex))
	{
	  nh6 = isis_nexthop6_create (ipv6_addr,
				      adj->circuit->interface->ifindex);
          nh6->router_address6 = adj->router_address6;
	  listnode_add (nexthops6, nh6);
	}
    }
}
#endif /* HAVE_IPV6 */

static struct isis_route_info *
isis_route_info_new (struct prefix *prefix, uint32_t cost, uint32_t depth,
                     struct list *adjacencies)
{
  struct isis_route_info *rinfo;
  struct isis_adjacency *adj;
  struct listnode *node;

  rinfo = XCALLOC (MTYPE_ISIS_ROUTE_INFO, sizeof (struct isis_route_info));
  if (!rinfo)
    {
      zlog_err ("ISIS-Rte: isis_route_info_new: out of memory!");
      return NULL;
    }

  if (prefix->family == AF_INET)
    {
      rinfo->nexthops = list_new ();
      for (ALL_LIST_ELEMENTS_RO (adjacencies, node, adj))
        {
          /* check for force resync this route */
          if (CHECK_FLAG (adj->circuit->flags, ISIS_CIRCUIT_FLAPPED_AFTER_SPF))
            SET_FLAG (rinfo->flag, ISIS_ROUTE_FLAG_ZEBRA_RESYNC);
          /* update neighbor router address */
          if (depth == 2 && prefix->prefixlen == 32)
            adj->router_address = prefix->u.prefix4;
          adjinfo2nexthop (rinfo->nexthops, adj);
        }
    }
#ifdef HAVE_IPV6
  if (prefix->family == AF_INET6)
    {
      rinfo->nexthops6 = list_new ();
      for (ALL_LIST_ELEMENTS_RO (adjacencies, node, adj))
        {
          /* check for force resync this route */
          if (CHECK_FLAG (adj->circuit->flags, ISIS_CIRCUIT_FLAPPED_AFTER_SPF))
            SET_FLAG (rinfo->flag, ISIS_ROUTE_FLAG_ZEBRA_RESYNC);
          /* update neighbor router address */
          if (depth == 2 && prefix->prefixlen == 128)
            adj->router_address6 = prefix->u.prefix6;
          adjinfo2nexthop6 (rinfo->nexthops6, adj);
        }
    }

#endif /* HAVE_IPV6 */

  rinfo->cost = cost;
  rinfo->depth = depth;

  return rinfo;
}

static void
isis_route_info_delete (struct isis_route_info *route_info)
{
  if (route_info->nexthops)
    {
      route_info->nexthops->del = (void (*)(void *)) isis_nexthop_delete;
      list_delete (route_info->nexthops);
    }

#ifdef HAVE_IPV6
  if (route_info->nexthops6)
    {
      route_info->nexthops6->del = (void (*)(void *)) isis_nexthop6_delete;
      list_delete (route_info->nexthops6);
    }
#endif /* HAVE_IPV6 */

  XFREE (MTYPE_ISIS_ROUTE_INFO, route_info);
}

static int
isis_route_info_same_attrib (struct isis_route_info *new,
			     struct isis_route_info *old)
{
  if (new->cost != old->cost)
    return 0;
  if (new->depth != old->depth)
    return 0;

  return 1;
}

static int
isis_route_info_same (struct isis_route_info *new,
		      struct isis_route_info *old, u_char family)
{
  struct listnode *node;
  struct isis_nexthop *nexthop;
#ifdef HAVE_IPV6
  struct isis_nexthop6 *nexthop6;
#endif /* HAVE_IPV6 */

  if (!CHECK_FLAG (old->flag, ISIS_ROUTE_FLAG_ZEBRA_SYNCED))
    return 0;

  if (CHECK_FLAG (new->flag, ISIS_ROUTE_FLAG_ZEBRA_RESYNC))
    return 0;

  if (!isis_route_info_same_attrib (new, old))
    return 0;

  if (family == AF_INET)
    {
      for (ALL_LIST_ELEMENTS_RO (new->nexthops, node, nexthop))
        if (nexthoplookup (old->nexthops, &nexthop->ip, nexthop->ifindex)
              == 0)
          return 0;

      for (ALL_LIST_ELEMENTS_RO (old->nexthops, node, nexthop))
        if (nexthoplookup (new->nexthops, &nexthop->ip, nexthop->ifindex)
             == 0)
          return 0;
    }
#ifdef HAVE_IPV6
  else if (family == AF_INET6)
    {
      for (ALL_LIST_ELEMENTS_RO (new->nexthops6, node, nexthop6))
        if (nexthop6lookup (old->nexthops6, &nexthop6->ip6,
                            nexthop6->ifindex) == 0)
          return 0;

      for (ALL_LIST_ELEMENTS_RO (old->nexthops6, node, nexthop6))
        if (nexthop6lookup (new->nexthops6, &nexthop6->ip6,
                            nexthop6->ifindex) == 0)
          return 0;
    }
#endif /* HAVE_IPV6 */

  return 1;
}

struct isis_route_info *
isis_route_create (struct prefix *prefix, u_int32_t cost, u_int32_t depth,
		   struct list *adjacencies, struct isis_area *area,
		   int level)
{
  struct route_node *route_node;
  struct isis_route_info *rinfo_new, *rinfo_old, *route_info = NULL;
  u_char buff[BUFSIZ];
  u_char family;

  family = prefix->family;
  /* for debugs */
  prefix2str (prefix, (char *) buff, BUFSIZ);

  rinfo_new = isis_route_info_new (prefix, cost, depth, adjacencies);
  if (!rinfo_new)
    {
      zlog_err ("ISIS-Rte (%s): isis_route_create: out of memory!",
		area->area_tag);
      return NULL;
    }

  if (family == AF_INET)
    route_node = route_node_get (area->route_table[level - 1], prefix);
#ifdef HAVE_IPV6
  else if (family == AF_INET6)
    route_node = route_node_get (area->route_table6[level - 1], prefix);
#endif /* HAVE_IPV6 */
  else
    {
      isis_route_info_delete (rinfo_new);
      return NULL;
    }

  rinfo_old = route_node->info;
  if (!rinfo_old)
    {
      if (isis->debugs & DEBUG_RTE_EVENTS)
        zlog_debug ("ISIS-Rte (%s) route created: %s", area->area_tag, buff);
      route_info = rinfo_new;
      UNSET_FLAG (route_info->flag, ISIS_ROUTE_FLAG_ZEBRA_SYNCED);
    }
  else
    {
      if (isis->debugs & DEBUG_RTE_EVENTS)
        zlog_debug ("ISIS-Rte (%s) route already exists: %s", area->area_tag,
                   buff);
      if (isis_route_info_same (rinfo_new, rinfo_old, family))
        {
          if (isis->debugs & DEBUG_RTE_EVENTS)
            zlog_debug ("ISIS-Rte (%s) route unchanged: %s", area->area_tag,
                        buff);
          isis_route_info_delete (rinfo_new);
          route_info = rinfo_old;
        }
      else
        {
          if (isis->debugs & DEBUG_RTE_EVENTS)
            zlog_debug ("ISIS-Rte (%s) route changed: %s", area->area_tag,
                        buff);
          isis_route_info_delete (rinfo_old);
          route_info = rinfo_new;
          UNSET_FLAG (route_info->flag, ISIS_ROUTE_FLAG_ZEBRA_SYNCED);
        }
    }

  SET_FLAG (route_info->flag, ISIS_ROUTE_FLAG_ACTIVE);
  route_node->info = route_info;

  return route_info;
}

static void
isis_route_delete (struct prefix *prefix, struct route_table *table)
{
  struct route_node *rode;
  struct isis_route_info *rinfo;
  char buff[BUFSIZ];

  /* for log */
  prefix2str (prefix, buff, BUFSIZ);


  rode = route_node_get (table, prefix);
  rinfo = rode->info;

  if (rinfo == NULL)
    {
      if (isis->debugs & DEBUG_RTE_EVENTS)
	zlog_debug ("ISIS-Rte: tried to delete non-existant route %s", buff);
      return;
    }

  if (CHECK_FLAG (rinfo->flag, ISIS_ROUTE_FLAG_ZEBRA_SYNCED))
    {
      UNSET_FLAG (rinfo->flag, ISIS_ROUTE_FLAG_ACTIVE);
      if (isis->debugs & DEBUG_RTE_EVENTS)
	zlog_debug ("ISIS-Rte: route delete  %s", buff);
      isis_zebra_route_update (prefix, rinfo);
    }
  isis_route_info_delete (rinfo);
  rode->info = NULL;

  return;
}

/* Validating routes in particular table. */
static void
isis_route_validate_table (struct isis_area *area, struct route_table *table)
{
  struct route_node *rnode, *drnode;
  struct isis_route_info *rinfo;
  u_char buff[BUFSIZ];

  for (rnode = route_top (table); rnode; rnode = route_next (rnode))
    {
      if (rnode->info == NULL)
	continue;
      rinfo = rnode->info;

      if (isis->debugs & DEBUG_RTE_EVENTS)
	{
	  prefix2str (&rnode->p, (char *) buff, BUFSIZ);
	  zlog_debug ("ISIS-Rte (%s): route validate: %s %s %s %s",
		      area->area_tag,
		      (CHECK_FLAG (rinfo->flag, ISIS_ROUTE_FLAG_ZEBRA_SYNCED) ?
		      "synced" : "not-synced"),
		      (CHECK_FLAG (rinfo->flag, ISIS_ROUTE_FLAG_ZEBRA_RESYNC) ?
		      "resync" : "not-resync"),
		      (CHECK_FLAG (rinfo->flag, ISIS_ROUTE_FLAG_ACTIVE) ?
		      "active" : "inactive"), buff);
	}

      isis_zebra_route_update (&rnode->p, rinfo);
      if (!CHECK_FLAG (rinfo->flag, ISIS_ROUTE_FLAG_ACTIVE))
	{
	  /* Area is either L1 or L2 => we use level route tables directly for
	   * validating => no problems with deleting routes. */
	  if (area->is_type != IS_LEVEL_1_AND_2)
	    {
	      isis_route_delete (&rnode->p, table);
	      continue;
	    }
	  /* If area is L1L2, we work with merge table and therefore must
	   * delete node from level tables as well before deleting route info.
	   * FIXME: Is it performance problem? There has to be the better way.
	   * Like not to deal with it here at all (see the next comment)? */
	  if (rnode->p.family == AF_INET)
	    {
	      drnode = route_node_get (area->route_table[0], &rnode->p);
	      if (drnode->info == rnode->info)
		drnode->info = NULL;
	      drnode = route_node_get (area->route_table[1], &rnode->p);
	      if (drnode->info == rnode->info)
		drnode->info = NULL;
	    }

#ifdef HAVE_IPV6
	  if (rnode->p.family == AF_INET6)
	    {
	      drnode = route_node_get (area->route_table6[0], &rnode->p);
	      if (drnode->info == rnode->info)
		drnode->info = NULL;
	      drnode = route_node_get (area->route_table6[1], &rnode->p);
	      if (drnode->info == rnode->info)
		drnode->info = NULL;
	    }
#endif
	      
	  isis_route_delete (&rnode->p, table);
	}
    }
}

/* Function to validate route tables for L1L2 areas. In this case we can't use
 * level route tables directly, we have to merge them at first. L1 routes are
 * preferred over the L2 ones.
 *
 * Merge algorithm is trivial (at least for now). All L1 paths are copied into
 * merge table at first, then L2 paths are added if L1 path for same prefix
 * doesn't already exists there.
 *
 * FIXME: Is it right place to do it at all? Maybe we should push both levels
 * to the RIB with different zebra route types and let RIB handle this? */
static void
isis_route_validate_merge (struct isis_area *area, int family)
{
  struct route_table *table = NULL;
  struct route_table *merge;
  struct route_node *rnode, *mrnode;

  merge = route_table_init ();

  if (family == AF_INET)
    table = area->route_table[0];
#ifdef HAVE_IPV6
  else if (family == AF_INET6)
    table = area->route_table6[0];
#endif

  for (rnode = route_top (table); rnode; rnode = route_next (rnode))
    {
      if (rnode->info == NULL)
        continue;
      mrnode = route_node_get (merge, &rnode->p);
      mrnode->info = rnode->info;
    }

  if (family == AF_INET)
    table = area->route_table[1];
#ifdef HAVE_IPV6
  else if (family == AF_INET6)
    table = area->route_table6[1];
#endif

  for (rnode = route_top (table); rnode; rnode = route_next (rnode))
    {
      if (rnode->info == NULL)
        continue;
      mrnode = route_node_get (merge, &rnode->p);
      if (mrnode->info != NULL)
        continue;
      mrnode->info = rnode->info;
    }

  isis_route_validate_table (area, merge);
  route_table_finish (merge);
}

/* Walk through route tables and propagate necessary changes into RIB. In case
 * of L1L2 area, level tables have to be merged at first. */
void
isis_route_validate (struct isis_area *area)
{
  struct listnode *node;
  struct isis_circuit *circuit;

  if (area->is_type == IS_LEVEL_1)
    isis_route_validate_table (area, area->route_table[0]);
  else if (area->is_type == IS_LEVEL_2)
    isis_route_validate_table (area, area->route_table[1]);
  else
    isis_route_validate_merge (area, AF_INET);

#ifdef HAVE_IPV6
  if (area->is_type == IS_LEVEL_1)
    isis_route_validate_table (area, area->route_table6[0]);
  else if (area->is_type == IS_LEVEL_2)
    isis_route_validate_table (area, area->route_table6[1]);
  else
    isis_route_validate_merge (area, AF_INET6);
#endif

  /* walk all circuits and reset any spf specific flags */
  for (ALL_LIST_ELEMENTS_RO (area->circuit_list, node, circuit))
    UNSET_FLAG(circuit->flags, ISIS_CIRCUIT_FLAPPED_AFTER_SPF);

  return;
}

void
isis_route_invalidate_table (struct isis_area *area, struct route_table *table)
{
  struct route_node *rode;
  struct isis_route_info *rinfo;
  for (rode = route_top (table); rode; rode = route_next (rode))
    {
      if (rode->info == NULL)
        continue;
      rinfo = rode->info;

      UNSET_FLAG (rinfo->flag, ISIS_ROUTE_FLAG_ACTIVE);
    }
}

void
isis_route_invalidate (struct isis_area *area)
{
  if (area->is_type & IS_LEVEL_1)
    isis_route_invalidate_table (area, area->route_table[0]);
  if (area->is_type & IS_LEVEL_2)
    isis_route_invalidate_table (area, area->route_table[1]);
}
