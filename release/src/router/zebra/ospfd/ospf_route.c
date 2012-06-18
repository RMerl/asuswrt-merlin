/*
 * OSPF routing table.
 * Copyright (C) 1999, 2000 Toshiaki Takada
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
#include "linklist.h"
#include "log.h"
#include "if.h"
#include "command.h"
#include "sockunion.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_interface.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_route.h"
#include "ospfd/ospf_spf.h"
#include "ospfd/ospf_zebra.h"
#include "ospfd/ospf_dump.h"

struct ospf_route *
ospf_route_new ()
{
  struct ospf_route *new;

  new = XCALLOC (MTYPE_OSPF_ROUTE, sizeof (struct ospf_route));

  new->ctime = time (NULL);
  new->mtime = new->ctime;

  return new;
}

void
ospf_route_free (struct ospf_route *or)
{
  listnode node;

  if (or->path)
    {
      for (node = listhead (or->path); node; nextnode (node))
	ospf_path_free (node->data);

      list_delete (or->path);
    }

  XFREE (MTYPE_OSPF_ROUTE, or);
}

struct ospf_path *
ospf_path_new ()
{
  struct ospf_path *new;

  new = XCALLOC (MTYPE_OSPF_PATH, sizeof (struct ospf_path));

  return new;
}

struct ospf_path *
ospf_path_dup (struct ospf_path *path)
{
  struct ospf_path *new;

  new = ospf_path_new ();
  memcpy (new, path, sizeof (struct ospf_path));

  return new;
}

void
ospf_path_free (struct ospf_path *op)
{
  XFREE (MTYPE_OSPF_PATH, op);
}

void
ospf_route_delete (struct route_table *rt)
{
  struct route_node *rn;
  struct ospf_route *or;

  for (rn = route_top (rt); rn; rn = route_next (rn))
    if ((or = rn->info) != NULL)
      {
	if (or->type == OSPF_DESTINATION_NETWORK)
	  ospf_zebra_delete ((struct prefix_ipv4 *) &rn->p,
				       or);
	else if (or->type == OSPF_DESTINATION_DISCARD)
	  ospf_zebra_delete_discard ((struct prefix_ipv4 *) &rn->p);
      }
}

void
ospf_route_table_free (struct route_table *rt)
{
  struct route_node *rn;
  struct ospf_route *or;

  for (rn = route_top (rt); rn; rn = route_next (rn))
    if ((or = rn->info) != NULL)
      {
	ospf_route_free (or);

	rn->info = NULL;
	route_unlock_node (rn);
      }

   route_table_finish (rt);
}

/* If a prefix and a nexthop match any route in the routing table,
   then return 1, otherwise return 0. */
int
ospf_route_match_same (struct route_table *rt, struct prefix_ipv4 *prefix,
		       struct ospf_route *newor)
{
  struct route_node *rn;
  struct ospf_route *or;
  struct ospf_path *op;
  struct ospf_path *newop;
  listnode n1;
  listnode n2;

  if (! rt || ! prefix)
    return 0;

   rn = route_node_lookup (rt, (struct prefix *) prefix);
   if (! rn || ! rn->info)
     return 0;
 
   route_unlock_node (rn);

   or = rn->info;
   if (or->type == newor->type && or->cost == newor->cost)
     {
       if (or->type == OSPF_DESTINATION_NETWORK)
	 {
	   if (or->path->count != newor->path->count)
	     return 0;

	   /* Check each path. */
	   for (n1 = listhead (or->path), n2 = listhead (newor->path);
		n1 && n2; nextnode (n1), nextnode (n2))
	     { 
	       op = getdata (n1);
	       newop = getdata (n2);

	       if (! IPV4_ADDR_SAME (&op->nexthop, &newop->nexthop))
		 return 0;
	     }
	   return 1;
	 }
       else if (prefix_same (&rn->p, (struct prefix *) prefix))
	 return 1;
     }
  return 0;
}

/* rt: Old, cmprt: New */
void
ospf_route_delete_uniq (struct route_table *rt, struct route_table *cmprt)
{
  struct route_node *rn;
  struct ospf_route *or;

  for (rn = route_top (rt); rn; rn = route_next (rn))
    if ((or = rn->info) != NULL) 
      if (or->path_type == OSPF_PATH_INTRA_AREA ||
	  or->path_type == OSPF_PATH_INTER_AREA)
	{
	  if (or->type == OSPF_DESTINATION_NETWORK)
	    {
	      if (! ospf_route_match_same (cmprt, 
					   (struct prefix_ipv4 *) &rn->p, or))
		ospf_zebra_delete ((struct prefix_ipv4 *) &rn->p, or);
	    }
	  else if (or->type == OSPF_DESTINATION_DISCARD)
	    if (! ospf_route_match_same (cmprt,
					 (struct prefix_ipv4 *) &rn->p, or))
	      ospf_zebra_delete_discard ((struct prefix_ipv4 *) &rn->p);
	}
}

/* Install routes to table. */
void
ospf_route_install (struct ospf *ospf, struct route_table *rt)
{
  struct route_node *rn;
  struct ospf_route *or;

  /* rt contains new routing table, new_table contains an old one.
     updating pointers */
  if (ospf->old_table)
    ospf_route_table_free (ospf->old_table);

  ospf->old_table = ospf->new_table;
  ospf->new_table = rt;

  /* Delete old routes. */
  if (ospf->old_table)
    ospf_route_delete_uniq (ospf->old_table, rt);

  /* Install new routes. */
  for (rn = route_top (rt); rn; rn = route_next (rn))
    if ((or = rn->info) != NULL)
      {
	if (or->type == OSPF_DESTINATION_NETWORK)
	  {
	    if (! ospf_route_match_same (ospf->old_table,
					 (struct prefix_ipv4 *)&rn->p, or))
	      ospf_zebra_add ((struct prefix_ipv4 *) &rn->p, or);
	  }
	else if (or->type == OSPF_DESTINATION_DISCARD)
	  if (! ospf_route_match_same (ospf->old_table,
				       (struct prefix_ipv4 *) &rn->p, or))
	    ospf_zebra_add_discard ((struct prefix_ipv4 *) &rn->p);
      }
}

void
ospf_intra_route_add (struct route_table *rt, struct vertex *v,
		      struct ospf_area *area)
{
  struct route_node *rn;
  struct ospf_route *or;
  struct prefix_ipv4 p;
  struct ospf_path *path;
  struct vertex_nexthop *nexthop;
  listnode nnode;

  p.family = AF_INET;
  p.prefix = v->id;
  if (v->type == OSPF_VERTEX_ROUTER)
    p.prefixlen = IPV4_MAX_BITLEN;
  else
    {
      struct network_lsa *lsa = (struct network_lsa *) v->lsa;
      p.prefixlen = ip_masklen (lsa->mask);
    }
  apply_mask_ipv4 (&p);

  rn = route_node_get (rt, (struct prefix *) &p);
  if (rn->info)
    {
      zlog_warn ("Same routing information exists for %s", inet_ntoa (v->id));
      route_unlock_node (rn);
      return;
    }

  or = ospf_route_new ();

  if (v->type == OSPF_VERTEX_NETWORK)
    {
      or->type = OSPF_DESTINATION_NETWORK;
      or->path = list_new ();

      for (nnode = listhead (v->nexthop); nnode; nextnode (nnode))
	{
	  nexthop = getdata (nnode);
	  path = ospf_path_new ();
	  path->nexthop = nexthop->router;
	  listnode_add (or->path, path);
	}
    }
  else
    or->type = OSPF_DESTINATION_ROUTER;

  or->id = v->id;
  or->u.std.area_id = area->area_id;
#ifdef HAVE_NSSA
  or->u.std.external_routing= area->external_routing;
#endif /* HAVE_NSSA */
  or->path_type = OSPF_PATH_INTRA_AREA;
  or->cost = v->distance;

  rn->info = or;
}

/* RFC2328 16.1. (4). For "router". */
void
ospf_intra_add_router (struct route_table *rt, struct vertex *v,
		       struct ospf_area *area)
{
  struct route_node *rn;
  struct ospf_route *or;
  struct prefix_ipv4 p;
  struct router_lsa *lsa;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_intra_add_router: Start");

  lsa = (struct router_lsa *) v->lsa;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_intra_add_router: LS ID: %s",
	       inet_ntoa (lsa->header.id));

  ospf_vl_up_check (area, lsa->header.id, v);

  if (!CHECK_FLAG (lsa->flags, ROUTER_LSA_SHORTCUT))
    area->shortcut_capability = 0;

  /* If the newly added vertex is an area border router or AS boundary
     router, a routing table entry is added whose destination type is
     "router". */
  if (! IS_ROUTER_LSA_BORDER (lsa) && ! IS_ROUTER_LSA_EXTERNAL (lsa))
    {
      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_intra_add_router: "
		   "this router is neither ASBR nor ABR, skipping it");
      return;
    }

  /* Update ABR and ASBR count in this area. */
  if (IS_ROUTER_LSA_BORDER (lsa))
    area->abr_count++;
  if (IS_ROUTER_LSA_EXTERNAL (lsa))
    area->asbr_count++;

  /* The Options field found in the associated router-LSA is copied
     into the routing table entry's Optional capabilities field. Call
     the newly added vertex Router X. */
  or = ospf_route_new ();

  or->id = v->id;
  or->u.std.area_id = area->area_id;
#ifdef HAVE_NSSA
  or->u.std.external_routing = area->external_routing;
#endif /* HAVE_NSSA */
  or->path_type = OSPF_PATH_INTRA_AREA;
  or->cost = v->distance;
  or->type = OSPF_DESTINATION_ROUTER;
  or->u.std.origin = (struct lsa_header *) lsa;
  or->u.std.options = lsa->header.options;
  or->u.std.flags = lsa->flags;

  /* If Router X is the endpoint of one of the calculating router's
     virtual links, and the virtual link uses Area A as Transit area:
     the virtual link is declared up, the IP address of the virtual
     interface is set to the IP address of the outgoing interface
     calculated above for Router X, and the virtual neighbor's IP
     address is set to Router X's interface address (contained in
     Router X's router-LSA) that points back to the root of the
     shortest- path tree; equivalently, this is the interface that
     points back to Router X's parent vertex on the shortest-path tree
     (similar to the calculation in Section 16.1.1). */

  p.family = AF_INET;
  p.prefix = v->id;
  p.prefixlen = IPV4_MAX_BITLEN;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_intra_add_router: talking about %s/%d",
	       inet_ntoa (p.prefix), p.prefixlen);

  rn = route_node_get (rt, (struct prefix *) &p);

  /* Note that we keep all routes to ABRs and ASBRs, not only the best */
  if (rn->info == NULL)
    rn->info = list_new ();
  else
    route_unlock_node (rn);

  ospf_route_copy_nexthops_from_vertex (or, v);

  listnode_add (rn->info, or);

  zlog_info ("ospf_intra_add_router: Start");
}

/* RFC2328 16.1. (4).  For transit network. */
void
ospf_intra_add_transit (struct route_table *rt, struct vertex *v,
			struct ospf_area *area)
{
  struct route_node *rn;
  struct ospf_route *or;
  struct prefix_ipv4 p;
  struct network_lsa *lsa;

  lsa = (struct network_lsa*) v->lsa;

  /* If the newly added vertex is a transit network, the routing table
     entry for the network is located.  The entry's Destination ID is
     the IP network number, which can be obtained by masking the
     Vertex ID (Link State ID) with its associated subnet mask (found
     in the body of the associated network-LSA). */
  p.family = AF_INET;
  p.prefix = v->id;
  p.prefixlen = ip_masklen (lsa->mask);
  apply_mask_ipv4 (&p);

  rn = route_node_get (rt, (struct prefix *) &p);

  /* If the routing table entry already exists (i.e., there is already
     an intra-area route to the destination installed in the routing
     table), multiple vertices have mapped to the same IP network.
     For example, this can occur when a new Designated Router is being
     established.  In this case, the current routing table entry
     should be overwritten if and only if the newly found path is just
     as short and the current routing table entry's Link State Origin
     has a smaller Link State ID than the newly added vertex' LSA. */
  if (rn->info)
    {
      struct ospf_route *cur_or;

      route_unlock_node (rn);
      cur_or = rn->info;

      if (v->distance > cur_or->cost ||
          IPV4_ADDR_CMP (&cur_or->u.std.origin->id, &lsa->header.id) > 0)
	return;
      
      ospf_route_free (rn->info);
    }

  or = ospf_route_new ();

  or->id = v->id;
  or->u.std.area_id = area->area_id;
#ifdef HAVE_NSSA
  or->u.std.external_routing = area->external_routing;
#endif /* HAVE_NSSA */
  or->path_type = OSPF_PATH_INTRA_AREA;
  or->cost = v->distance;
  or->type = OSPF_DESTINATION_NETWORK;
  or->u.std.origin = (struct lsa_header *) lsa;

  ospf_route_copy_nexthops_from_vertex (or, v);
  
  rn->info = or;
}

/* RFC2328 16.1. second stage. */
void
ospf_intra_add_stub (struct route_table *rt, struct router_lsa_link *link,
		     struct vertex *v, struct ospf_area *area)
{
  u_int32_t cost;
  struct route_node *rn;
  struct ospf_route *or;
  struct prefix_ipv4 p;
  struct router_lsa *lsa;
  struct ospf_interface *oi;
  struct ospf_path *path;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_intra_add_stub(): Start");

  lsa = (struct router_lsa *) v->lsa;

  p.family = AF_INET;
  p.prefix = link->link_id;
  p.prefixlen = ip_masklen (link->link_data);
  apply_mask_ipv4 (&p);

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_intra_add_stub(): processing route to %s/%d",  
	       inet_ntoa (p.prefix), p.prefixlen);

  /* (1) Calculate the distance D of stub network from the root.  D is
     equal to the distance from the root to the router vertex
     (calculated in stage 1), plus the stub network link's advertised
     cost. */
  cost = v->distance + ntohs (link->m[0].metric);

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_intra_add_stub(): calculated cost is %d + %d = %d", 
	       v->distance, ntohs(link->m[0].metric), cost);

  rn = route_node_get (rt, (struct prefix *) &p);

  /* Lookup current routing table. */
  if (rn->info)
    {
      struct ospf_route *cur_or;

      route_unlock_node (rn);

      cur_or = rn->info;

      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_intra_add_stub(): "
		   "another route to the same prefix found");

      /* Compare this distance to the current best cost to the stub
	 network.  This is done by looking up the stub network's
	 current routing table entry.  If the calculated distance D is
	 larger, go on to examine the next stub network link in the
	 LSA. */
      if (cost > cur_or->cost)
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_intra_add_stub(): old route is better, exit");
	  return;
	}

      /* (2) If this step is reached, the stub network's routing table
	 entry must be updated.  Calculate the set of next hops that
	 would result from using the stub network link.  This
	 calculation is shown in Section 16.1.1; input to this
	 calculation is the destination (the stub network) and the
	 parent vertex (the router vertex). If the distance D is the
	 same as the current routing table cost, simply add this set
	 of next hops to the routing table entry's list of next hops.
	 In this case, the routing table already has a Link State
	 Origin.  If this Link State Origin is a router-LSA whose Link
	 State ID is smaller than V's Router ID, reset the Link State
	 Origin to V's router-LSA. */

      if (cost == cur_or->cost)
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_intra_add_stub(): routes are equal, merge");

	  ospf_route_copy_nexthops_from_vertex (cur_or, v);

	  if (IPV4_ADDR_CMP (&cur_or->u.std.origin->id, &lsa->header.id) < 0)
	    cur_or->u.std.origin = (struct lsa_header *) lsa;
	  return;
	}

      /* Otherwise D is smaller than the routing table cost.
	 Overwrite the current routing table entry by setting the
	 routing table entry's cost to D, and by setting the entry's
	 list of next hops to the newly calculated set.  Set the
	 routing table entry's Link State Origin to V's router-LSA.
	 Then go on to examine the next stub network link. */

      if (cost < cur_or->cost)
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_intra_add_stub(): new route is better, set it");

	  cur_or->cost = cost;

	  list_delete (cur_or->path);
	  cur_or->path = NULL;

	  ospf_route_copy_nexthops_from_vertex (cur_or, v);

	  cur_or->u.std.origin = (struct lsa_header *) lsa;
	  return;
	}
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_intra_add_stub(): installing new route");

  or = ospf_route_new ();

  or->id = v->id;
  or->u.std.area_id = area->area_id;
#ifdef HAVE_NSSA
  or->u.std.external_routing = area->external_routing;
#endif /* HAVE_NSSA */
  or->path_type = OSPF_PATH_INTRA_AREA;
  or->cost = cost;
  or->type = OSPF_DESTINATION_NETWORK;
  or->u.std.origin = (struct lsa_header *) lsa;
  or->path = list_new ();

  /* Nexthop is depend on connection type. */
  if (v != area->spf)
    {
      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_intra_add_stub(): this network is on remote router");
      ospf_route_copy_nexthops_from_vertex (or, v);
    }
  else
    {
      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_intra_add_stub(): this network is on this router");

      if ((oi = ospf_if_lookup_by_prefix (area->ospf, &p)))
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_intra_add_stub(): the interface is %s",
		       IF_NAME (oi));

	  path = ospf_path_new ();
	  path->nexthop.s_addr = 0;
	  path->oi = oi;
	  listnode_add (or->path, path);
	}
      else
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_intra_add_stub(): where's the interface ?");
	}
    }

  rn->info = or;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info("ospf_intra_add_stub(): Stop");
}

char *ospf_path_type_str[] =
{
  "unknown-type",
  "intra-area",
  "inter-area",
  "type1-external",
  "type2-external"
};

void
ospf_route_table_dump (struct route_table *rt)
{
  struct route_node *rn;
  struct ospf_route *or;
  char buf1[BUFSIZ];
  char buf2[BUFSIZ];
  listnode pnode;
  struct ospf_path *path;

  zlog_info ("========== OSPF routing table ==========");
  for (rn = route_top (rt); rn; rn = route_next (rn))
    if ((or = rn->info) != NULL)
      {
        if (or->type == OSPF_DESTINATION_NETWORK)
	  {
	    zlog_info ("N %s/%d\t%s\t%s\t%d", 
		       inet_ntop (AF_INET, &rn->p.u.prefix4, buf1, BUFSIZ),
		       rn->p.prefixlen,
		       inet_ntop (AF_INET, &or->u.std.area_id, buf2,
				  BUFSIZ),
		       ospf_path_type_str[or->path_type],
		       or->cost);
	    for (pnode = listhead (or->path); pnode; nextnode (pnode))
	      {
		path = getdata (pnode);
		zlog_info ("  -> %s", inet_ntoa (path->nexthop));
	      }
	  }
        else
	  zlog_info ("R %s\t%s\t%s\t%d", 
		     inet_ntop (AF_INET, &rn->p.u.prefix4, buf1, BUFSIZ),
		     inet_ntop (AF_INET, &or->u.std.area_id, buf2,
				BUFSIZ),
		     ospf_path_type_str[or->path_type],
		     or->cost);
      }
  zlog_info ("========================================");
}

void
ospf_terminate ()
{
  struct ospf *ospf;
  listnode node;

  LIST_LOOP (om->ospf, ospf, node)
    {
      if (ospf->new_table)
	ospf_route_delete (ospf->new_table);
      if (ospf->old_external_route)
	ospf_route_delete (ospf->old_external_route);
    }
}

/* This is 16.4.1 implementation.
   o Intra-area paths using non-backbone areas are always the most preferred.
   o The other paths, intra-area backbone paths and inter-area paths,
     are of equal preference. */
int
ospf_asbr_route_cmp (struct ospf *ospf, struct ospf_route *r1,
		     struct ospf_route *r2)
{
  u_char r1_type, r2_type;

  r1_type = r1->path_type;
  r2_type = r2->path_type;

  /* If RFC1583Compat flag is on -- all paths are equal. */
  if (CHECK_FLAG (ospf->config, OSPF_RFC1583_COMPATIBLE))
    return 0;

  /* r1/r2 itself is backbone, and it's Inter-area path. */
  if (OSPF_IS_AREA_ID_BACKBONE (r1->u.std.area_id))
    r1_type = OSPF_PATH_INTER_AREA;
  if (OSPF_IS_AREA_ID_BACKBONE (r2->u.std.area_id))
    r2_type = OSPF_PATH_INTER_AREA;

  return (r1_type - r2_type);
}

/* Compare two routes.
 ret <  0 -- r1 is better.
 ret == 0 -- r1 and r2 are the same.
 ret >  0 -- r2 is better. */
int
ospf_route_cmp (struct ospf *ospf, struct ospf_route *r1,
		struct ospf_route *r2)
{
  int ret = 0;

  /* Path types of r1 and r2 are not the same. */
  if ((ret = (r1->path_type - r2->path_type)))
    return ret;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Route[Compare]: Path types are the same.");
  /* Path types are the same, compare any cost. */
  switch (r1->path_type)
    {
    case OSPF_PATH_INTRA_AREA:
    case OSPF_PATH_INTER_AREA:
      break;
    case OSPF_PATH_TYPE1_EXTERNAL:
      if (!CHECK_FLAG (ospf->config, OSPF_RFC1583_COMPATIBLE))
	{
	  ret = ospf_asbr_route_cmp (ospf, r1->u.ext.asbr, r2->u.ext.asbr);
	  if (ret != 0)
	    return ret;
	}
      break;
    case OSPF_PATH_TYPE2_EXTERNAL:
      if ((ret = (r1->u.ext.type2_cost - r2->u.ext.type2_cost)))
	return ret;

      if (!CHECK_FLAG (ospf->config, OSPF_RFC1583_COMPATIBLE))
	{
	  ret = ospf_asbr_route_cmp (ospf, r1->u.ext.asbr, r2->u.ext.asbr);
	  if (ret != 0)
	    return ret;
	}
      break;
    }      

  /* Anyway, compare the costs. */
  return (r1->cost - r2->cost);
}

int
ospf_path_exist (struct list *plist, struct in_addr nexthop,
		 struct ospf_interface *oi)
{
  listnode node;
  struct ospf_path *path;

  for (node = listhead (plist); node; nextnode (node))
    {
      path = node->data;

      if (IPV4_ADDR_SAME (&path->nexthop, &nexthop) && path->oi == oi)
	return 1;
    }
  return 0;
}

void
ospf_route_copy_nexthops_from_vertex (struct ospf_route *to,
				      struct vertex *v)
{
  listnode nnode;
  struct ospf_path *path;
  struct vertex_nexthop *nexthop;

  if (to->path == NULL)
    to->path = list_new ();

  for (nnode = listhead (v->nexthop); nnode; nextnode (nnode))
    {
      nexthop = getdata (nnode);

      if (nexthop->oi != NULL) 
	{
	  if (! ospf_path_exist (to->path, nexthop->router, nexthop->oi))
	    {
	      path = ospf_path_new ();
	      path->nexthop = nexthop->router;
	      path->oi = nexthop->oi;
	      listnode_add (to->path, path);
	    }
	}
    }
}

struct ospf_path *
ospf_path_lookup (list plist, struct ospf_path *path)
{
  listnode node;

  for (node = listhead (plist); node; nextnode (node))
    {
      struct ospf_path *op = node->data;

      if (IPV4_ADDR_SAME (&op->nexthop, &path->nexthop) &&
	  IPV4_ADDR_SAME (&op->adv_router, &path->adv_router))
	return op;
    }

  return NULL;
}

void
ospf_route_copy_nexthops (struct ospf_route *to, list from)
{
  listnode node;

  if (to->path == NULL)
    to->path = list_new ();

  for (node = listhead (from); node; nextnode (node))
    /* The same routes are just discarded. */
    if (!ospf_path_lookup (to->path, node->data))
      listnode_add (to->path, ospf_path_dup (node->data));
}

void
ospf_route_subst_nexthops (struct ospf_route *to, list from)
{
  listnode node;
  struct ospf_path *op;

  for (node = listhead (to->path); node; nextnode (node))
    if ((op = getdata (node)) != NULL)
      {
	ospf_path_free (op);
	node->data = NULL;
      }

  list_delete_all_node (to->path);
  ospf_route_copy_nexthops (to, from);
}

void
ospf_route_subst (struct route_node *rn, struct ospf_route *new_or,
		  struct ospf_route *over)
{
  route_lock_node (rn);
  ospf_route_free (rn->info);

  ospf_route_copy_nexthops (new_or, over->path);
  rn->info = new_or;
  route_unlock_node (rn);
}

void
ospf_route_add (struct route_table *rt, struct prefix_ipv4 *p,
		struct ospf_route *new_or, struct ospf_route *over)
{
  struct route_node *rn;

  rn = route_node_get (rt, (struct prefix *) p);

  ospf_route_copy_nexthops (new_or, over->path);

  if (rn->info)
    {
      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_route_add(): something's wrong !");
      route_unlock_node (rn);
      return;
    }

  rn->info = new_or;
}

void
ospf_prune_unreachable_networks (struct route_table *rt)
{
  struct route_node *rn, *next;
  struct ospf_route *or;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Pruning unreachable networks");

  for (rn = route_top (rt); rn; rn = next)
    {
      next = route_next (rn);
      if (rn->info != NULL)
	{
	  or = rn->info;
	  if (listcount (or->path) == 0)
	    {
	      if (IS_DEBUG_OSPF_EVENT)
		zlog_info ("Pruning route to %s/%d",
			   inet_ntoa (rn->p.u.prefix4), rn->p.prefixlen);

	      ospf_route_free (or);
	      rn->info = NULL;
	      route_unlock_node (rn);
	    }
	}
    }
}

void
ospf_prune_unreachable_routers (struct route_table *rtrs)
{
  struct route_node *rn, *next;
  struct ospf_route *or;
  listnode node, nnext;
  list paths;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Pruning unreachable routers");

  for (rn = route_top (rtrs); rn; rn = next)
    {
      next = route_next (rn);
      if ((paths = rn->info) == NULL)
	continue;

      for (node = listhead (paths); node; node = nnext) 
	{
	  nnext = node->next;

	  or = getdata (node);

	  if (listcount (or->path) == 0)
	    {
	      if (IS_DEBUG_OSPF_EVENT)
		{
		  zlog_info ("Pruning route to rtr %s",
			     inet_ntoa (rn->p.u.prefix4));
		  zlog_info ("               via area %s",
			     inet_ntoa (or->u.std.area_id));
		}

	      listnode_delete (paths, or);
	      ospf_route_free (or);
	    }
	}

      if (listcount (paths) == 0)
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("Pruning router node %s", inet_ntoa (rn->p.u.prefix4));

	  list_delete (paths);
	  rn->info = NULL;
	  route_unlock_node (rn);
	}
    }
}

int
ospf_add_discard_route (struct route_table *rt, struct ospf_area *area,
			struct prefix_ipv4 *p)
{
  struct route_node *rn;
  struct ospf_route *or, *new_or;

  rn = route_node_get (rt, (struct prefix *) p);

  if (rn == NULL)
    {
      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_add_discard_route(): router installation error");
      return 0;
    }

  if (rn->info) /* If the route to the same destination is found */
    {
      route_unlock_node (rn);

      or = rn->info;

      if (or->path_type == OSPF_PATH_INTRA_AREA)
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_add_discard_route(): "
		       "an intra-area route exists");
	  return 0;
	}

      if (or->type == OSPF_DESTINATION_DISCARD)
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_add_discard_route(): "
		       "discard entry already installed");
	  return 0;
	}

      ospf_route_free (rn->info);
  }

  new_or = ospf_route_new ();
  new_or->type = OSPF_DESTINATION_DISCARD;
  new_or->id.s_addr = 0;
  new_or->cost = 0;
  new_or->u.std.area_id = area->area_id;
#ifdef HAVE_NSSA
  new_or->u.std.external_routing = area->external_routing;
#endif /* HAVE_NSSA */
  new_or->path_type = OSPF_PATH_INTER_AREA;
  rn->info = new_or;

  ospf_zebra_add_discard (p);

  return 1;
}

void
ospf_delete_discard_route (struct prefix_ipv4 *p)
{
  ospf_zebra_delete_discard(p);
}

