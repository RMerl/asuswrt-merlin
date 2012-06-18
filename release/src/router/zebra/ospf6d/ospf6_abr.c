/*
 * Area Border Router function.
 * Copyright (C) 2004 Yasuhiro Ohara
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
 * along with GNU Zebra; see the file COPYING.  If not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 * Boston, MA 02111-1307, USA.  
 */

#include <zebra.h>

#include "log.h"
#include "prefix.h"
#include "table.h"
#include "vty.h"
#include "linklist.h"
#include "command.h"
#include "thread.h"
#include "plist.h"
#include "filter.h"

#include "ospf6_proto.h"
#include "ospf6_route.h"
#include "ospf6_lsa.h"
#include "ospf6_route.h"
#include "ospf6_lsdb.h"
#include "ospf6_message.h"

#include "ospf6_top.h"
#include "ospf6_area.h"
#include "ospf6_interface.h"
#include "ospf6_neighbor.h"

#include "ospf6_flood.h"
#include "ospf6_intra.h"
#include "ospf6_abr.h"
#include "ospf6d.h"

unsigned char conf_debug_ospf6_abr;

int
ospf6_is_router_abr (struct ospf6 *o)
{
  listnode node;
  struct ospf6_area *oa;
  int area_count = 0;

  for (node = listhead (o->area_list); node; nextnode (node))
    {
      oa = OSPF6_AREA (getdata (node));
      if (IS_AREA_ENABLED (oa))
        area_count++;
    }

  if (area_count > 1)
    return 1;
  return 0;
}

void
ospf6_abr_enable_area (struct ospf6_area *area)
{
  struct ospf6_area *oa;
  struct ospf6_route *ro;
  listnode node;

  for (node = listhead (area->ospf6->area_list); node; nextnode (node))
    {
      oa = OSPF6_AREA (getdata (node));

      /* update B bit for each area */
      OSPF6_ROUTER_LSA_SCHEDULE (oa);

      /* install other area's configured address range */
      if (oa != area)
        {
          for (ro = ospf6_route_head (oa->range_table); ro;
               ro = ospf6_route_next (ro))
            {
              if (CHECK_FLAG (ro->flag, OSPF6_ROUTE_ACTIVE_SUMMARY))
                ospf6_abr_originate_summary_to_area (ro, area);
            }
        }
    }

  /* install calculated routes to border routers */
  for (ro = ospf6_route_head (area->ospf6->brouter_table); ro;
       ro = ospf6_route_next (ro))
    ospf6_abr_originate_summary_to_area (ro, area);

  /* install calculated routes to network (may be rejected by ranges) */
  for (ro = ospf6_route_head (area->ospf6->route_table); ro;
       ro = ospf6_route_next (ro))
    ospf6_abr_originate_summary_to_area (ro, area);
}

void
ospf6_abr_disable_area (struct ospf6_area *area)
{
  struct ospf6_area *oa;
  struct ospf6_route *ro;
  struct ospf6_lsa *old;
  listnode node;

  /* Withdraw all summary prefixes previously originated */
  for (ro = ospf6_route_head (area->summary_prefix); ro;
       ro = ospf6_route_next (ro))
    {
      old = ospf6_lsdb_lookup (ro->path.origin.type, ro->path.origin.id,
                               area->ospf6->router_id, area->lsdb);
      if (old)
        ospf6_lsa_purge (old);
      ospf6_route_remove (ro, area->summary_prefix);
    }

  /* Withdraw all summary router-routes previously originated */
  for (ro = ospf6_route_head (area->summary_router); ro;
       ro = ospf6_route_next (ro))
    {
      old = ospf6_lsdb_lookup (ro->path.origin.type, ro->path.origin.id,
                               area->ospf6->router_id, area->lsdb);
      if (old)
        ospf6_lsa_purge (old);
      ospf6_route_remove (ro, area->summary_router);
    }

  /* Schedule Router-LSA for each area (ABR status may change) */
  for (node = listhead (area->ospf6->area_list); node; nextnode (node))
    {
      oa = OSPF6_AREA (getdata (node));

      /* update B bit for each area */
      OSPF6_ROUTER_LSA_SCHEDULE (oa);
    }
}

/* RFC 2328 12.4.3. Summary-LSAs */
void
ospf6_abr_originate_summary_to_area (struct ospf6_route *route,
                                     struct ospf6_area *area)
{
  struct ospf6_lsa *lsa, *old = NULL;
  struct ospf6_interface *oi;
  struct ospf6_route *summary, *range = NULL;
  struct ospf6_area *route_area;
  char buffer[OSPF6_MAX_LSASIZE];
  struct ospf6_lsa_header *lsa_header;
  caddr_t p;
  struct ospf6_inter_prefix_lsa *prefix_lsa;
  struct ospf6_inter_router_lsa *router_lsa;
  struct ospf6_route_table *summary_table = NULL;
  u_int16_t type;
  char buf[64];
  int is_debug = 0;

  if (route->type == OSPF6_DEST_TYPE_ROUTER)
    {
      if (IS_OSPF6_DEBUG_ABR || IS_OSPF6_DEBUG_ORIGINATE (INTER_ROUTER))
        {
          is_debug++;
          inet_ntop (AF_INET, &(ADV_ROUTER_IN_PREFIX (&route->prefix)),
                     buf, sizeof (buf));
          zlog_info ("Originating summary in area %s for ASBR %s",
                     area->name, buf);
        }
      summary_table = area->summary_router;
    }
  else
    {
      if (IS_OSPF6_DEBUG_ABR || IS_OSPF6_DEBUG_ORIGINATE (INTER_PREFIX))
        {
          is_debug++;
          prefix2str (&route->prefix, buf, sizeof (buf));
          zlog_info ("Originating summary in area %s for %s",
                     area->name, buf);
        }
      summary_table = area->summary_prefix;
    }

  summary = ospf6_route_lookup (&route->prefix, summary_table);
  if (summary)
    old = ospf6_lsdb_lookup (summary->path.origin.type,
                             summary->path.origin.id,
                             area->ospf6->router_id, area->lsdb);

  /* if this route has just removed, remove corresponding LSA */
  if (CHECK_FLAG (route->flag, OSPF6_ROUTE_REMOVE))
    {
      if (is_debug)
        zlog_info ("The route has just removed, purge previous LSA");
      if (summary)
        ospf6_route_remove (summary, summary_table);
      if (old)
        ospf6_lsa_purge (old);
      return;
    }

  /* Only destination type network, range or ASBR are considered */
  if (route->type != OSPF6_DEST_TYPE_NETWORK &&
      route->type != OSPF6_DEST_TYPE_RANGE &&
      (route->type != OSPF6_DEST_TYPE_ROUTER ||
       ! CHECK_FLAG (route->path.router_bits, OSPF6_ROUTER_BIT_E)))
    {
      if (is_debug)
        zlog_info ("Route type is none of network, range nor ASBR, withdraw");
      if (summary)
        ospf6_route_remove (summary, summary_table);
      if (old)
        ospf6_lsa_purge (old);
      return;
    }

  /* AS External routes are never considered */
  if (route->path.type == OSPF6_PATH_TYPE_EXTERNAL1 ||
      route->path.type == OSPF6_PATH_TYPE_EXTERNAL2)
    {
      if (is_debug)
        zlog_info ("Path type is external, withdraw");
      if (summary)
        ospf6_route_remove (summary, summary_table);
      if (old)
        ospf6_lsa_purge (old);
      return;
    }

  /* do not generate if the path's area is the same as target area */
  if (route->path.area_id == area->area_id)
    {
      if (is_debug)
        zlog_info ("The route is in the area itself, ignore");
      if (summary)
        ospf6_route_remove (summary, summary_table);
      if (old)
        ospf6_lsa_purge (old);
      return;
    }

  /* do not generate if the nexthops belongs to the target area */
  oi = ospf6_interface_lookup_by_ifindex (route->nexthop[0].ifindex);
  if (oi && oi->area && oi->area == area)
    {
      if (is_debug)
        zlog_info ("The route's nexthop is in the same area, ignore");
      if (summary)
        ospf6_route_remove (summary, summary_table);
      if (old)
        ospf6_lsa_purge (old);
      return;
    }

  /* do not generate if the route cost is greater or equal to LSInfinity */
  if (route->path.cost >= LS_INFINITY)
    {
      if (is_debug)
        zlog_info ("The cost exceeds LSInfinity, withdraw");
      if (summary)
        ospf6_route_remove (summary, summary_table);
      if (old)
        ospf6_lsa_purge (old);
      return;
    }

  /* if this is a route to ASBR */
  if (route->type == OSPF6_DEST_TYPE_ROUTER)
    {
      /* Only the prefered best path is considered */
      if (! CHECK_FLAG (route->flag, OSPF6_ROUTE_BEST))
        {
          if (is_debug)
            zlog_info ("This is the secondary path to the ASBR, ignore");
          if (summary)
            ospf6_route_remove (summary, summary_table);
          if (old)
            ospf6_lsa_purge (old);
          return;
        }

      /* Do not generate if the area is stub */
      /* XXX */
    }

  /* if this is an intra-area route, this may be suppressed by aggregation */
  if (route->type == OSPF6_DEST_TYPE_NETWORK &&
      route->path.type == OSPF6_PATH_TYPE_INTRA)
    {
      /* search for configured address range for the route's area */
      route_area = ospf6_area_lookup (route->path.area_id, area->ospf6);
      assert (route_area);
      range = ospf6_route_lookup_bestmatch (&route->prefix,
                                            route_area->range_table);

      /* ranges are ignored when originate backbone routes to transit area.
         Otherwise, if ranges are configured, the route is suppressed. */
      if (range && ! CHECK_FLAG (range->flag, OSPF6_ROUTE_REMOVE) &&
          (route->path.area_id != BACKBONE_AREA_ID ||
           ! IS_AREA_TRANSIT (area)))
        {
          if (is_debug)
            {
              prefix2str (&range->prefix, buf, sizeof (buf));
              zlog_info ("Suppressed by range %s of area %s",
                         buf, route_area->name);
            }

          if (summary)
            ospf6_route_remove (summary, summary_table);
          if (old)
            ospf6_lsa_purge (old);
          return;
        }
    }

  /* If this is a configured address range */
  if (route->type == OSPF6_DEST_TYPE_RANGE)
    {
      /* If DoNotAdvertise is set */
      if (CHECK_FLAG (route->flag, OSPF6_ROUTE_DO_NOT_ADVERTISE))
        {
          if (is_debug)
            zlog_info ("This is the range with DoNotAdvertise set. ignore");
          if (summary)
            ospf6_route_remove (summary, summary_table);
          if (old)
            ospf6_lsa_purge (old);
          return;
        }

      /* Whether the route have active longer prefix */
      if (! CHECK_FLAG (route->flag, OSPF6_ROUTE_ACTIVE_SUMMARY))
        {
          if (is_debug)
            zlog_info ("The range is not active. withdraw");
          if (summary)
            ospf6_route_remove (summary, summary_table);
          if (old)
            ospf6_lsa_purge (old);
          return;
        }
    }

  /* Check export list */
  if (EXPORT_NAME (area))
    {
      if (EXPORT_LIST (area) == NULL)
        EXPORT_LIST (area) =
          access_list_lookup (AFI_IP6, EXPORT_NAME (area));

      if (EXPORT_LIST (area))
        if (access_list_apply (EXPORT_LIST (area),
                               &route->prefix) == FILTER_DENY)
          {
            if (is_debug)
              {
                inet_ntop (AF_INET, &(ADV_ROUTER_IN_PREFIX (&route->prefix)),
                           buf, sizeof(buf));
                zlog_debug ("prefix %s was denied by export list", buf);
              }
            return;
          }
    }

  /* Check filter-list */
  if (PREFIX_NAME_OUT (area))
    {
      if (PREFIX_LIST_OUT (area) == NULL)
        PREFIX_LIST_OUT (area) =
          prefix_list_lookup(AFI_IP6, PREFIX_NAME_OUT (area));

      if (PREFIX_LIST_OUT (area))
         if (prefix_list_apply (PREFIX_LIST_OUT (area), 
                                &route->prefix) != PREFIX_PERMIT) 
           {
             if (is_debug)
               {
                 inet_ntop (AF_INET, &(ADV_ROUTER_IN_PREFIX (&route->prefix)),
                            buf, sizeof (buf));
                 zlog_debug ("prefix %s was denied by filter-list out", buf);
               }
             return;
           }
    }

  /* the route is going to be originated. store it in area's summary_table */
  if (summary == NULL)
    {
      summary = ospf6_route_copy (route);
      if (route->type == OSPF6_DEST_TYPE_NETWORK ||
          route->type == OSPF6_DEST_TYPE_RANGE)
        summary->path.origin.type = htons (OSPF6_LSTYPE_INTER_PREFIX);
      else
        summary->path.origin.type = htons (OSPF6_LSTYPE_INTER_ROUTER);
      summary->path.origin.adv_router = area->ospf6->router_id;
      summary->path.origin.id =
        ospf6_new_ls_id (summary->path.origin.type,
                         summary->path.origin.adv_router, area->lsdb);
      summary = ospf6_route_add (summary, summary_table);
    }
  else
    {
      summary->type = route->type;
      gettimeofday (&summary->changed, NULL);
    }

  summary->path.router_bits = route->path.router_bits;
  summary->path.options[0] = route->path.options[0];
  summary->path.options[1] = route->path.options[1];
  summary->path.options[2] = route->path.options[2];
  summary->path.prefix_options = route->path.prefix_options;
  summary->path.area_id = area->area_id;
  summary->path.type = OSPF6_PATH_TYPE_INTER;
  summary->path.cost = route->path.cost;
  summary->nexthop[0] = route->nexthop[0];

  /* prepare buffer */
  memset (buffer, 0, sizeof (buffer));
  lsa_header = (struct ospf6_lsa_header *) buffer;

  if (route->type == OSPF6_DEST_TYPE_ROUTER)
    {
      router_lsa = (struct ospf6_inter_router_lsa *)
        ((caddr_t) lsa_header + sizeof (struct ospf6_lsa_header));
      p = (caddr_t) router_lsa + sizeof (struct ospf6_inter_router_lsa);

      /* Fill Inter-Area-Router-LSA */
      router_lsa->options[0] = route->path.options[0];
      router_lsa->options[1] = route->path.options[1];
      router_lsa->options[2] = route->path.options[2];
      OSPF6_ABR_SUMMARY_METRIC_SET (router_lsa, route->path.cost);
      router_lsa->router_id = ADV_ROUTER_IN_PREFIX (&route->prefix);
      type = htons (OSPF6_LSTYPE_INTER_ROUTER);
    }
  else
    {
      prefix_lsa = (struct ospf6_inter_prefix_lsa *)
        ((caddr_t) lsa_header + sizeof (struct ospf6_lsa_header));
      p = (caddr_t) prefix_lsa + sizeof (struct ospf6_inter_prefix_lsa);

      /* Fill Inter-Area-Prefix-LSA */
      OSPF6_ABR_SUMMARY_METRIC_SET (prefix_lsa, route->path.cost);
      prefix_lsa->prefix.prefix_length = route->prefix.prefixlen;
      prefix_lsa->prefix.prefix_options = route->path.prefix_options;

      /* set Prefix */
      memcpy (p, &route->prefix.u.prefix6,
              OSPF6_PREFIX_SPACE (route->prefix.prefixlen));
      ospf6_prefix_apply_mask (&prefix_lsa->prefix);
      p += OSPF6_PREFIX_SPACE (route->prefix.prefixlen);
      type = htons (OSPF6_LSTYPE_INTER_PREFIX);
    }

  /* Fill LSA Header */
  lsa_header->age = 0;
  lsa_header->type = type;
  lsa_header->id = summary->path.origin.id;
  lsa_header->adv_router = area->ospf6->router_id;
  lsa_header->seqnum =
    ospf6_new_ls_seqnum (lsa_header->type, lsa_header->id,
                         lsa_header->adv_router, area->lsdb);
  lsa_header->length = htons ((caddr_t) p - (caddr_t) lsa_header);

  /* LSA checksum */
  ospf6_lsa_checksum (lsa_header);

  /* create LSA */
  lsa = ospf6_lsa_create (lsa_header);

  /* Originate */
  ospf6_lsa_originate_area (lsa, area);
}

void
ospf6_abr_range_update (struct ospf6_route *range)
{
  u_int32_t cost = 0;
  struct ospf6_route *ro;

  assert (range->type == OSPF6_DEST_TYPE_RANGE);

  /* update range's cost and active flag */
  for (ro = ospf6_route_match_head (&range->prefix, ospf6->route_table);
       ro; ro = ospf6_route_match_next (&range->prefix, ro))
    {
      if (ro->path.area_id == range->path.area_id &&
          ! CHECK_FLAG (ro->flag, OSPF6_ROUTE_REMOVE))
        cost = MAX (cost, ro->path.cost);
    }

  if (range->path.cost != cost)
    {
      range->path.cost = cost;

      if (range->path.cost)
        SET_FLAG (range->flag, OSPF6_ROUTE_ACTIVE_SUMMARY);
      else
        UNSET_FLAG (range->flag, OSPF6_ROUTE_ACTIVE_SUMMARY);

      ospf6_abr_originate_summary (range);
    }
}

void
ospf6_abr_originate_summary (struct ospf6_route *route)
{
  listnode node;
  struct ospf6_area *oa;
  struct ospf6_route *range = NULL;

  if (route->type == OSPF6_DEST_TYPE_NETWORK)
    {
      oa = ospf6_area_lookup (route->path.area_id, ospf6);
      range = ospf6_route_lookup_bestmatch (&route->prefix, oa->range_table);
      if (range)
        ospf6_abr_range_update (range);
    }

  for (node = listhead (ospf6->area_list); node; nextnode (node))
    {
      oa = (struct ospf6_area *) getdata (node);
      ospf6_abr_originate_summary_to_area (route, oa);
    }
}

/* RFC 2328 16.2. Calculating the inter-area routes */
void
ospf6_abr_examin_summary (struct ospf6_lsa *lsa, struct ospf6_area *oa)
{
  struct prefix prefix, abr_prefix;
  struct ospf6_route_table *table = NULL;
  struct ospf6_route *range, *route, *old = NULL;
  struct ospf6_route *abr_entry;
  u_char type = 0;
  char options[3] = {0, 0, 0};
  u_int8_t prefix_options = 0;
  u_int32_t cost = 0;
  u_char router_bits = 0;
  int i;
  char buf[64];
  int is_debug = 0;

  if (lsa->header->type == htons (OSPF6_LSTYPE_INTER_PREFIX))
    {
      struct ospf6_inter_prefix_lsa *prefix_lsa;

      if (IS_OSPF6_DEBUG_EXAMIN (INTER_PREFIX))
        {
          is_debug++;
          zlog_info ("Examin %s in area %s", lsa->name, oa->name);
        }

      prefix_lsa = (struct ospf6_inter_prefix_lsa *)
        OSPF6_LSA_HEADER_END (lsa->header);
      prefix.family = AF_INET6;
      prefix.prefixlen = prefix_lsa->prefix.prefix_length;
      ospf6_prefix_in6_addr (&prefix.u.prefix6, &prefix_lsa->prefix);
      prefix2str (&prefix, buf, sizeof (buf));
      table = oa->ospf6->route_table;
      type = OSPF6_DEST_TYPE_NETWORK;
      prefix_options = prefix_lsa->prefix.prefix_options;
      cost = OSPF6_ABR_SUMMARY_METRIC (prefix_lsa);
    }
  else if (lsa->header->type == htons (OSPF6_LSTYPE_INTER_ROUTER))
    {
      struct ospf6_inter_router_lsa *router_lsa;

      if (IS_OSPF6_DEBUG_EXAMIN (INTER_ROUTER))
        {
          is_debug++;
          zlog_info ("Examin %s in area %s", lsa->name, oa->name);
        }

      router_lsa = (struct ospf6_inter_router_lsa *)
        OSPF6_LSA_HEADER_END (lsa->header);
      ospf6_linkstate_prefix (router_lsa->router_id, htonl (0), &prefix);
      inet_ntop (AF_INET, &router_lsa->router_id, buf, sizeof (buf));
      table = oa->ospf6->brouter_table;
      type = OSPF6_DEST_TYPE_ROUTER;
      options[0] = router_lsa->options[0];
      options[1] = router_lsa->options[1];
      options[2] = router_lsa->options[2];
      cost = OSPF6_ABR_SUMMARY_METRIC (router_lsa);
      SET_FLAG (router_bits, OSPF6_ROUTER_BIT_E);
    }
  else
    assert (0);

  /* Find existing route */
  route = ospf6_route_lookup (&prefix, table);
  if (route)
    ospf6_route_lock (route);
  while (route && ospf6_route_is_prefix (&prefix, route))
    {
      if (route->path.area_id == oa->area_id &&
          route->path.origin.type == lsa->header->type &&
          route->path.origin.id == lsa->header->id &&
          route->path.origin.adv_router == lsa->header->adv_router)
        old = route;
      route = ospf6_route_next (route);
    }

  /* (1) if cost == LSInfinity or if the LSA is MaxAge */
  if (cost == LS_INFINITY)
    {
      if (is_debug)
        zlog_info ("cost is LS_INFINITY, ignore");
      if (old)
        ospf6_route_remove (old, table);
      return;
    }
  if (OSPF6_LSA_IS_MAXAGE (lsa))
    {
      if (is_debug)
        zlog_info ("LSA is MaxAge, ignore");
      if (old)
        ospf6_route_remove (old, table);
      return;
    }

  /* (2) if the LSA is self-originated, ignore */
  if (lsa->header->adv_router == oa->ospf6->router_id)
    {
      if (is_debug)
        zlog_info ("LSA is self-originated, ignore");
      if (old)
        ospf6_route_remove (old, table);
      return;
    }

  /* (3) if the prefix is equal to an active configured address range */
  if (lsa->header->type == htons (OSPF6_LSTYPE_INTER_PREFIX))
    {
      range = ospf6_route_lookup (&prefix, oa->range_table);
      if (range)
        {
          if (is_debug)
            zlog_info ("Prefix is equal to address range, ignore");
          if (old)
            ospf6_route_remove (old, table);
          return;
        }
    }

  /* (4) if the routing table entry for the ABR does not exist */
  ospf6_linkstate_prefix (lsa->header->adv_router, htonl (0), &abr_prefix);
  abr_entry = ospf6_route_lookup (&abr_prefix, oa->ospf6->brouter_table);
  if (abr_entry == NULL || abr_entry->path.area_id != oa->area_id ||
      CHECK_FLAG (abr_entry->flag, OSPF6_ROUTE_REMOVE) ||
      ! CHECK_FLAG (abr_entry->path.router_bits, OSPF6_ROUTER_BIT_B))
    {
      if (is_debug)
        zlog_info ("ABR router entry does not exist, ignore");
      if (old)
        ospf6_route_remove (old, table);
      return;
    }

  /* Check import list */
  if (IMPORT_NAME (oa))
    {
      if (IMPORT_LIST (oa) == NULL)
        IMPORT_LIST (oa) = access_list_lookup (AFI_IP6, IMPORT_NAME (oa));

      if (IMPORT_LIST (oa))
        if (access_list_apply (IMPORT_LIST (oa), &prefix) == FILTER_DENY)
          {
            if (is_debug)
              zlog_debug ("Prefix was denied by import-list");
            if (old)
              ospf6_route_remove (old, table);
            return;
          }
    }

  /* Check input prefix-list */
  if (PREFIX_NAME_IN (oa))
    {
      if (PREFIX_LIST_IN (oa) == NULL)
        PREFIX_LIST_IN (oa) = prefix_list_lookup (AFI_IP6, PREFIX_NAME_IN (oa));

      if (PREFIX_LIST_IN (oa))
        if (prefix_list_apply (PREFIX_LIST_IN (oa), &prefix) != PREFIX_PERMIT)
          {
            if (is_debug)
              zlog_debug ("Prefix was denied by prefix-list");
            if (old)
              ospf6_route_remove (old, table);
            return;
          }
    }

  /* (5),(6),(7) the path preference is handled by the sorting
     in the routing table. Always install the path by substituting
     old route (if any). */
  if (old)
    route = ospf6_route_copy (old);
  else
    route = ospf6_route_create ();

  route->type = type;
  route->prefix = prefix;
  route->path.origin.type = lsa->header->type;
  route->path.origin.id = lsa->header->id;
  route->path.origin.adv_router = lsa->header->adv_router;
  route->path.router_bits = router_bits;
  route->path.options[0] = options[0];
  route->path.options[1] = options[1];
  route->path.options[2] = options[2];
  route->path.prefix_options = prefix_options;
  route->path.area_id = oa->area_id;
  route->path.type = OSPF6_PATH_TYPE_INTER;
  route->path.cost = abr_entry->path.cost + cost;
  for (i = 0; i < OSPF6_MULTI_PATH_LIMIT; i++)
    route->nexthop[i] = abr_entry->nexthop[i];

  if (is_debug)
    zlog_info ("Install route: %s", buf);
  ospf6_route_add (route, table);
}

void
ospf6_abr_examin_brouter (u_int32_t router_id)
{
  struct ospf6_lsa *lsa;
  struct ospf6_area *oa;
  listnode node;
  u_int16_t type;

  type = htons (OSPF6_LSTYPE_INTER_ROUTER);
  for (node = listhead (ospf6->area_list); node; nextnode (node))
    {
      oa = OSPF6_AREA (getdata (node));
      for (lsa = ospf6_lsdb_type_router_head (type, router_id, oa->lsdb); lsa;
           lsa = ospf6_lsdb_type_router_next (type, router_id, lsa))
        ospf6_abr_examin_summary (lsa, oa);
    }

  type = htons (OSPF6_LSTYPE_INTER_PREFIX);
  for (node = listhead (ospf6->area_list); node; nextnode (node))
    {
      oa = OSPF6_AREA (getdata (node));
      for (lsa = ospf6_lsdb_type_router_head (type, router_id, oa->lsdb); lsa;
           lsa = ospf6_lsdb_type_router_next (type, router_id, lsa))
        ospf6_abr_examin_summary (lsa, oa);
    }
}

void
ospf6_abr_reimport (struct ospf6_area *oa)
{
  struct ospf6_lsa *lsa;
  u_int16_t type;

  type = htons (OSPF6_LSTYPE_INTER_ROUTER);
  for (lsa = ospf6_lsdb_type_head (type, oa->lsdb); lsa;
       lsa = ospf6_lsdb_type_next (type, lsa))
    ospf6_abr_examin_summary (lsa, oa);

  type = htons (OSPF6_LSTYPE_INTER_PREFIX);
  for (lsa = ospf6_lsdb_type_head (type, oa->lsdb); lsa;
       lsa = ospf6_lsdb_type_next (type, lsa))
    ospf6_abr_examin_summary (lsa, oa);
}



/* Display functions */
int
ospf6_inter_area_prefix_lsa_show (struct vty *vty, struct ospf6_lsa *lsa)
{
  struct ospf6_inter_prefix_lsa *prefix_lsa;
  struct in6_addr in6;
  char buf[64];

  prefix_lsa = (struct ospf6_inter_prefix_lsa *)
    OSPF6_LSA_HEADER_END (lsa->header);

  vty_out (vty, "     Metric: %lu%s",
           (u_long) OSPF6_ABR_SUMMARY_METRIC (prefix_lsa), VNL);

  ospf6_prefix_options_printbuf (prefix_lsa->prefix.prefix_options,
                                 buf, sizeof (buf));
  vty_out (vty, "     Prefix Options: %s%s", buf, VNL);

  ospf6_prefix_in6_addr (&in6, &prefix_lsa->prefix);
  inet_ntop (AF_INET6, &in6, buf, sizeof (buf));
  vty_out (vty, "     Prefix: %s/%d%s", buf,
           prefix_lsa->prefix.prefix_length, VNL);

  return 0;
}

int
ospf6_inter_area_router_lsa_show (struct vty *vty, struct ospf6_lsa *lsa)
{
  struct ospf6_inter_router_lsa *router_lsa;
  char buf[64];

  router_lsa = (struct ospf6_inter_router_lsa *)
    OSPF6_LSA_HEADER_END (lsa->header);

  ospf6_options_printbuf (router_lsa->options, buf, sizeof (buf));
  vty_out (vty, "     Options: %s%s", buf, VNL);
  vty_out (vty, "     Metric: %lu%s",
           (u_long) OSPF6_ABR_SUMMARY_METRIC (router_lsa), VNL);
  inet_ntop (AF_INET, &router_lsa->router_id, buf, sizeof (buf));
  vty_out (vty, "     Destination Router ID: %s%s", buf, VNL);

  return 0;
}

/* Debug commands */
DEFUN (debug_ospf6_abr,
       debug_ospf6_abr_cmd,
       "debug ospf6 abr",
       DEBUG_STR
       OSPF6_STR
       "Debug OSPFv3 ABR function\n"
      )
{
  OSPF6_DEBUG_ABR_ON ();
  return CMD_SUCCESS;
}

DEFUN (no_debug_ospf6_abr,
       no_debug_ospf6_abr_cmd,
       "no debug ospf6 abr",
       NO_STR
       DEBUG_STR
       OSPF6_STR
       "Debug OSPFv3 ABR function\n"
      )
{
  OSPF6_DEBUG_ABR_OFF ();
  return CMD_SUCCESS;
}

int
config_write_ospf6_debug_abr (struct vty *vty)
{
  if (IS_OSPF6_DEBUG_ABR)
    vty_out (vty, "debug ospf6 abr%s", VNL);
  return 0;
}

void
install_element_ospf6_debug_abr ()
{
  install_element (ENABLE_NODE, &debug_ospf6_abr_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf6_abr_cmd);
  install_element (CONFIG_NODE, &debug_ospf6_abr_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf6_abr_cmd);
}

struct ospf6_lsa_handler inter_prefix_handler =
{
  OSPF6_LSTYPE_INTER_PREFIX,
  "Inter-Prefix",
  ospf6_inter_area_prefix_lsa_show
};

struct ospf6_lsa_handler inter_router_handler =
{
  OSPF6_LSTYPE_INTER_ROUTER,
  "Inter-Router",
  ospf6_inter_area_router_lsa_show
};

void
ospf6_abr_init ()
{
  ospf6_install_lsa_handler (&inter_prefix_handler);
  ospf6_install_lsa_handler (&inter_router_handler);
}


