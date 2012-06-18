/*
 * OSPF ABR functions.
 * Copyright (C) 1999, 2000 Alex Zinin, Toshiaki Takada
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
#include "memory.h"
#include "linklist.h"
#include "prefix.h"
#include "if.h"
#include "table.h"
#include "vty.h"
#include "filter.h"
#include "plist.h"
#include "log.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_interface.h"
#include "ospfd/ospf_ism.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_lsdb.h"
#include "ospfd/ospf_neighbor.h"
#include "ospfd/ospf_nsm.h"
#include "ospfd/ospf_spf.h"
#include "ospfd/ospf_route.h"
#include "ospfd/ospf_ia.h"
#include "ospfd/ospf_flood.h"
#include "ospfd/ospf_abr.h"
#include "ospfd/ospf_ase.h"
#include "ospfd/ospf_zebra.h"
#include "ospfd/ospf_dump.h"


struct ospf_area_range *
ospf_area_range_new (struct prefix_ipv4 *p)
{
  struct ospf_area_range *range;

  range = XCALLOC (MTYPE_OSPF_AREA_RANGE, sizeof (struct ospf_area_range));
  range->addr = p->prefix;
  range->masklen = p->prefixlen;
  range->cost_config = OSPF_AREA_RANGE_COST_UNSPEC;

  return range;
}

void
ospf_area_range_free (struct ospf_area_range *range)
{
  XFREE (MTYPE_OSPF_AREA_RANGE, range);
}

void
ospf_area_range_add (struct ospf_area *area, struct ospf_area_range *range)
{
  struct route_node *rn;
  struct prefix_ipv4 p;

  p.family = AF_INET;
  p.prefixlen = range->masklen;
  p.prefix = range->addr;

  rn = route_node_get (area->ranges, (struct prefix *)&p);
  if (rn->info)
    route_unlock_node (rn);
  else
    rn->info = range;
}

void
ospf_area_range_delete (struct ospf_area *area, struct ospf_area_range *range)
{
  struct route_node *rn;
  struct prefix_ipv4 p;

  p.family = AF_INET;
  p.prefixlen = range->masklen;
  p.prefix = range->addr;

  rn = route_node_lookup (area->ranges, (struct prefix *)&p);
  if (rn)
    {
      ospf_area_range_free (rn->info);
      rn->info = NULL;
      route_unlock_node (rn);
      route_unlock_node (rn);
    }
}

struct ospf_area_range *
ospf_area_range_lookup (struct ospf_area *area, struct prefix_ipv4 *p)
{
  struct route_node *rn;

  rn = route_node_lookup (area->ranges, (struct prefix *)p);
  if (rn)
    {
      route_unlock_node (rn);
      return rn->info;
    }
  return NULL;
}

struct ospf_area_range *
ospf_area_range_lookup_next (struct ospf_area *area, struct in_addr *range_net,
			     int first)
{
  struct route_node *rn;
  struct prefix_ipv4 p;
  struct ospf_area_range *find;

  p.family = AF_INET;
  p.prefixlen = IPV4_MAX_BITLEN;
  p.prefix = *range_net;

  if (first)
    rn = route_top (area->ranges);
  else
    {
      rn = route_node_get (area->ranges, (struct prefix *) &p);
      rn = route_next (rn);
    }

  for (; rn; rn = route_next (rn))
    if (rn->info)
      break;

  if (rn && rn->info)
    {
      find = rn->info;
      *range_net = rn->p.u.prefix4;
      route_unlock_node (rn);
      return find;
    }
  return NULL;
}

struct ospf_area_range *
ospf_area_range_match (struct ospf_area *area, struct prefix_ipv4 *p)
{
  struct route_node *node;

  node = route_node_match (area->ranges, (struct prefix *) p);
  if (node)
    {
      route_unlock_node (node);
      return node->info;
    }
  return NULL;
}

struct ospf_area_range *
ospf_area_range_match_any (struct ospf *ospf, struct prefix_ipv4 *p)
{
  struct ospf_area_range *range;
  listnode node;

  for (node = listhead (ospf->areas); node; nextnode (node))
    if ((range = ospf_area_range_match (node->data, p)))
      return range;

  return NULL;
}

int
ospf_area_range_active (struct ospf_area_range *range)
{
  return range->specifics;
}

int
ospf_area_actively_attached (struct ospf_area *area)
{
  return area->act_ints;
}

int
ospf_area_range_set (struct ospf *ospf, struct in_addr area_id,
		     struct prefix_ipv4 *p, int advertise)
{
  struct ospf_area *area;
  struct ospf_area_range *range;
  int ret = OSPF_AREA_ID_FORMAT_ADDRESS;

  area = ospf_area_get (ospf, area_id, ret);
  if (area == NULL)
    return 0;

  range = ospf_area_range_lookup (area, p);
  if (range != NULL)
    {
      if ((CHECK_FLAG (range->flags, OSPF_AREA_RANGE_ADVERTISE)
	   && !CHECK_FLAG (advertise, OSPF_AREA_RANGE_ADVERTISE))
	  || (!CHECK_FLAG (range->flags, OSPF_AREA_RANGE_ADVERTISE)
	      && CHECK_FLAG (advertise, OSPF_AREA_RANGE_ADVERTISE)))
	ospf_schedule_abr_task (ospf);
    }
  else
    {
      range = ospf_area_range_new (p);
      ospf_area_range_add (area, range);
      ospf_schedule_abr_task (ospf);
    }

  if (CHECK_FLAG (advertise, OSPF_AREA_RANGE_ADVERTISE))
    SET_FLAG (range->flags, OSPF_AREA_RANGE_ADVERTISE);
  else
    UNSET_FLAG (range->flags, OSPF_AREA_RANGE_ADVERTISE);

  return 1;
}

int
ospf_area_range_cost_set (struct ospf *ospf, struct in_addr area_id,
			  struct prefix_ipv4 *p, u_int32_t cost)
{
  struct ospf_area *area;
  struct ospf_area_range *range;
  int ret = OSPF_AREA_ID_FORMAT_ADDRESS;

  area = ospf_area_get (ospf, area_id, ret);
  if (area == NULL)
    return 0;

  range = ospf_area_range_new (p);
  if (range == NULL)
    return 0;

  if (range->cost_config != cost)
    {
      range->cost_config = cost;
      if (ospf_area_range_active (range))
	ospf_schedule_abr_task (ospf);
    }

  return 1;
}

int
ospf_area_range_unset (struct ospf *ospf, struct in_addr area_id,
		       struct prefix_ipv4 *p)
{
  struct ospf_area *area;
  struct ospf_area_range *range;

  area = ospf_area_lookup_by_area_id (ospf, area_id);
  if (area == NULL)
    return 0;

  range = ospf_area_range_lookup (area, p);
  if (range == NULL)
    return 0;

  if (ospf_area_range_active (range))
    ospf_schedule_abr_task (ospf);

  ospf_area_range_delete (area, range);

  return 1;
}

int
ospf_area_range_substitute_set (struct ospf *ospf, struct in_addr area_id,
				struct prefix_ipv4 *p, struct prefix_ipv4 *s)
{
  struct ospf_area *area;
  struct ospf_area_range *range;
  int ret = OSPF_AREA_ID_FORMAT_ADDRESS;

  area = ospf_area_get (ospf, area_id, ret);
  range = ospf_area_range_lookup (area, p);

  if (range != NULL)
    {
      if (!CHECK_FLAG (range->flags, OSPF_AREA_RANGE_ADVERTISE) ||
	  !CHECK_FLAG (range->flags, OSPF_AREA_RANGE_SUBSTITUTE))
	ospf_schedule_abr_task (ospf);
    }
  else
    {
      range = ospf_area_range_new (p);
      ospf_area_range_add (area, range);
      ospf_schedule_abr_task (ospf);
    }

  SET_FLAG (range->flags, OSPF_AREA_RANGE_ADVERTISE);
  SET_FLAG (range->flags, OSPF_AREA_RANGE_SUBSTITUTE);
  range->subst_addr = s->prefix;
  range->subst_masklen = s->prefixlen;

  return 1;
}

int
ospf_area_range_substitute_unset (struct ospf *ospf, struct in_addr area_id,
				  struct prefix_ipv4 *p)
{
  struct ospf_area *area;
  struct ospf_area_range *range;

  area = ospf_area_lookup_by_area_id (ospf, area_id);
  if (area == NULL)
    return 0;

  range = ospf_area_range_lookup (area, p);
  if (range == NULL)
    return 0;

  if (CHECK_FLAG (range->flags, OSPF_AREA_RANGE_SUBSTITUTE))
    if (ospf_area_range_active (range))
      ospf_schedule_abr_task (ospf);

  UNSET_FLAG (range->flags, OSPF_AREA_RANGE_SUBSTITUTE);
  range->subst_addr.s_addr = 0;
  range->subst_masklen = 0;

  return 1;
}

int
ospf_act_bb_connection (struct ospf *ospf)
{
  if (ospf->backbone == NULL)
    return 0;

  return ospf->backbone->full_nbrs;
}

/* Check area border router status. */
void
ospf_check_abr_status (struct ospf *ospf)
{
  struct ospf_area *area;
  listnode node;
  int bb_configured = 0;
  int bb_act_attached = 0;
  int areas_configured = 0;
  int areas_act_attached = 0;
  u_char new_flags = ospf->flags;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_check_abr_status(): Start");

  for (node = listhead (ospf->areas); node; nextnode (node))
    {
      area = getdata (node);

      if (listcount (area->oiflist)) 
	{
	  areas_configured++;
	  
	  if (OSPF_IS_AREA_BACKBONE (area))
 	    bb_configured = 1;
	}

      if (ospf_area_actively_attached (area))
	{
	  areas_act_attached++;
	  
	  if (OSPF_IS_AREA_BACKBONE (area))
            bb_act_attached = 1;
	}
    }

  if (IS_DEBUG_OSPF_EVENT)
    {
      zlog_info ("ospf_check_abr_status(): looked through areas");
      zlog_info ("ospf_check_abr_status(): bb_configured: %d", bb_configured);
      zlog_info ("ospf_check_abr_status(): bb_act_attached: %d",
		 bb_act_attached);
      zlog_info ("ospf_check_abr_status(): areas_configured: %d",
		 areas_configured);
      zlog_info ("ospf_check_abr_status(): areas_act_attached: %d",
		 areas_act_attached);
    }

  switch (ospf->abr_type)
    {
    case OSPF_ABR_SHORTCUT:
    case OSPF_ABR_STAND:
      if (areas_act_attached > 1)
	SET_FLAG (new_flags, OSPF_FLAG_ABR);
      else
	UNSET_FLAG (new_flags, OSPF_FLAG_ABR);
      break;

    case OSPF_ABR_IBM:
      if ((areas_act_attached > 1) && bb_configured)
	SET_FLAG (new_flags, OSPF_FLAG_ABR);
      else
	UNSET_FLAG (new_flags, OSPF_FLAG_ABR);
      break;

    case OSPF_ABR_CISCO:
      if ((areas_configured > 1) && bb_act_attached)
	SET_FLAG (new_flags, OSPF_FLAG_ABR);
      else
	UNSET_FLAG (new_flags, OSPF_FLAG_ABR);
      break;
    default:
      break;
    }

  if (new_flags != ospf->flags)
    {
      ospf_spf_calculate_schedule (ospf);
      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_check_abr_status(): new router flags: %x",new_flags);
      ospf->flags = new_flags;
      OSPF_TIMER_ON (ospf->t_router_lsa_update,
		     ospf_router_lsa_update_timer, OSPF_LSA_UPDATE_DELAY);
    }
}

void
ospf_abr_update_aggregate (struct ospf_area_range *range,
			   struct ospf_route *or)
{
  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_update_aggregate(): Start");

  if (range->cost_config != -1)
    {
      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_update_aggregate(): use configured cost %d",
		   range->cost_config);

      range->cost = range->cost_config;
    }
  else
    {
      if (range->specifics == 0)
	range->cost = or->cost; /* 1st time get 1st cost */

      if (or->cost > range->cost)
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_abr_update_aggregate(): largest cost, update");

	  range->cost = or->cost;
	}
    }

  range->specifics++;
}

static void
set_metric (struct ospf_lsa *lsa, u_int32_t metric)
{
  struct summary_lsa *header;
  u_char *mp;
  metric = htonl (metric);
  mp = (char *) &metric;
  mp++;
  header = (struct summary_lsa *) lsa->data;
  memcpy(header->metric, mp, 3);
}

#ifdef HAVE_NSSA
int
ospf_abr_check_nssa_range (struct prefix_ipv4 *p, u_int32_t cost,
			   struct ospf_area *area)
{
  /* The Type-7 is tested against the aggregated prefix and forwarded
     for lsa installation and flooding */
  return 0;
}

/* ospf_abr_translate_nssa */
int
ospf_abr_translate_nssa (struct ospf_area *area, struct ospf_lsa *lsa)
{
  /* Incoming Type-7 or later aggregated Type-7 

  LSA is skipped if P-bit is off.
  LSA is aggregated if within range.

  The Type-7 is translated, Installed/Approved as a Type-5 into
  global LSDB, then Flooded through AS

  Later, any Unapproved Translated Type-5's are flushed/discarded */

  struct ospf_lsa *dup;

  if (! CHECK_FLAG (lsa->data->options, OSPF_OPTION_NP))
    {
      if (IS_DEBUG_OSPF_NSSA)
	zlog_info ("ospf_abr_nssa(): P-bit off, NO Translation");
      return 0; 
    }

  if (IS_DEBUG_OSPF_NSSA)
    zlog_info ("ospf_abr_nssa(): TRANSLATING 7 to 5");

  /* No more P-bit. */
  /* UNSET_FLAG (lsa->data->options, OSPF_OPTION_NP); */

  /* Area where Aggregate testing will be inserted, just like summary
     advertisements */
  /* ospf_abr_check_nssa_range (p_arg, lsa-> cost, lsa -> area); */

  /* Follow thru here means no aggregation */
  dup = ospf_lsa_dup (lsa);	/* keep LSDB intact, lock = 1 */

  SET_FLAG (dup->flags, OSPF_LSA_LOCAL_XLT); /* Translated from 7  */
  SET_FLAG (dup->flags, OSPF_LSA_APPROVED); /* So, do not remove it */

  dup->data->type = OSPF_AS_EXTERNAL_LSA;  /* make Type-5 */

  ospf_lsa_checksum (dup->data);

  ospf_lsa_install (area->ospf, NULL, dup);  /* Install this Type-5 into LSDB, Lock = 2. */

  ospf_flood_through_as (area->ospf, NULL, dup); /* flood non-NSSA/STUB areas */
  
  /* This translated Type-5 will go to all non-NSSA areas connected to
     this ABR; The Type-5 could come from any of the NSSA's connected
     to this ABR.  */

  return 0;
}

void
ospf_abr_translate_nssa_range (struct prefix_ipv4 *p, u_int32_t cost)
{
  /* The Type-7 is created from the aggregated prefix and forwarded
     for lsa installation and flooding... to be added... */
}
#endif /* HAVE_NSSA */

void
ospf_abr_announce_network_to_area (struct prefix_ipv4 *p, u_int32_t cost,
				   struct ospf_area *area)
{
  struct ospf_lsa *lsa, *old = NULL;
  struct summary_lsa *sl = NULL;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_announce_network_to_area(): Start");

  old = ospf_lsa_lookup_by_prefix (area->lsdb, OSPF_SUMMARY_LSA, p,
				   area->ospf->router_id);
  if (old)
    {
      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_announce_network_to_area(): old summary found");
      sl = (struct summary_lsa *) old->data;

      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_announce_network_to_area(): "
		   "old metric: %d, new metric: %d",
		   GET_METRIC (sl->metric), cost);
    }

  if (old && (GET_METRIC (sl->metric) == cost))
    {
      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_announce_network_to_area(): "
		   "old summary approved"); 
      SET_FLAG (old->flags, OSPF_LSA_APPROVED);
    }
  else
    {
      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_announce_network_to_area(): "
		   "creating new summary");
      if (old)
	{

	  set_metric (old, cost);
	  lsa = ospf_summary_lsa_refresh (area->ospf, old);
	  /* This will flood through area. */
	}
      else
	{
	  lsa = ospf_summary_lsa_originate (p, cost, area);
	  /* This will flood through area. */
	}
      

      SET_FLAG (lsa->flags, OSPF_LSA_APPROVED);
      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_announce_network_to_area(): "
		   "flooding new version of summary");

#ifndef HAVE_NSSA      
      ospf_flood_through_area (area, NULL, lsa);
#endif /* ! HAVE_NSSA */
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_announce_network_to_area(): Stop");
}

int
ospf_abr_nexthops_belong_to_area (struct ospf_route *or,
				  struct ospf_area *area)
{
  listnode node;

  for (node = listhead (or->path); node; nextnode (node))
    {
      struct ospf_path *path = node->data;
      struct ospf_interface *oi = path->oi;

      if (oi != NULL)
	if (oi->area == area)
	  return 1;
    }

  return 0;
}

int
ospf_abr_should_accept (struct prefix *p, struct ospf_area *area)
{
  if (IMPORT_NAME (area))
    {
      if (IMPORT_LIST (area) == NULL)
	IMPORT_LIST (area) = access_list_lookup (AFI_IP, IMPORT_NAME (area));

      if (IMPORT_LIST (area))
        if (access_list_apply (IMPORT_LIST (area), p) == FILTER_DENY)
	  return 0;
    }

  return 1;
}

int
ospf_abr_plist_in_check (struct ospf_area *area, struct ospf_route *or,
			 struct prefix *p)
{
  if (PREFIX_NAME_IN (area))
    {
      if (PREFIX_LIST_IN (area) == NULL)
	PREFIX_LIST_IN (area) = prefix_list_lookup (AFI_IP,
						    PREFIX_NAME_IN (area));
      if (PREFIX_LIST_IN (area))
	if (prefix_list_apply (PREFIX_LIST_IN (area), p) != PREFIX_PERMIT)
	  return 0;
    }
  return 1;
}

int
ospf_abr_plist_out_check (struct ospf_area *area, struct ospf_route *or,
			  struct prefix *p)
{
  if (PREFIX_NAME_OUT (area))
    {
      if (PREFIX_LIST_OUT (area) == NULL)
	PREFIX_LIST_OUT (area) = prefix_list_lookup (AFI_IP,
						     PREFIX_NAME_OUT (area));
      if (PREFIX_LIST_OUT (area))
	if (prefix_list_apply (PREFIX_LIST_OUT (area), p) != PREFIX_PERMIT)
	  return 0;
    }
  return 1;
}

void
ospf_abr_announce_network (struct ospf *ospf,
			   struct route_node *n, struct ospf_route *or)
{
  struct ospf_area_range *range;
  struct ospf_area *area, *or_area;
  struct prefix_ipv4 *p;
  listnode node;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_announce_network(): Start");
  p = (struct prefix_ipv4 *) &n->p;

  or_area = ospf_area_lookup_by_area_id (ospf, or->u.std.area_id); 
  assert (or_area);

  for (node = listhead (ospf->areas); node; nextnode (node))
    {
      area = getdata (node);

      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_announce_network(): looking at area %s",
		   inet_ntoa (area->area_id));

      if (IPV4_ADDR_SAME (&or->u.std.area_id, &area->area_id))
	continue;

      if (ospf_abr_nexthops_belong_to_area (or, area))
	continue;

      if (!ospf_abr_should_accept (&n->p, area))
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_abr_announce_network(): "
		       "prefix %s/%d was denied by import-list",
		       inet_ntoa (p->prefix), p->prefixlen);
	  continue; 
	}

      if (!ospf_abr_plist_in_check (area, or, &n->p))
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_abr_announce_network(): "
		       "prefix %s/%d was denied by prefix-list",
		       inet_ntoa (p->prefix), p->prefixlen);
	  continue; 
	}

      if (area->external_routing != OSPF_AREA_DEFAULT && area->no_summary)
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_abr_announce_network(): "
		       "area %s is stub and no_summary",
		       inet_ntoa (area->area_id));
          continue;
	}

      if (or->path_type == OSPF_PATH_INTER_AREA)
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_abr_announce_network(): this is "
		       "inter-area route to %s/%d",
		       inet_ntoa (p->prefix), p->prefixlen);

          if (!OSPF_IS_AREA_BACKBONE (area))
	    ospf_abr_announce_network_to_area (p, or->cost, area);
	}

      if (or->path_type == OSPF_PATH_INTRA_AREA)
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_abr_announce_network(): "
		       "this is intra-area route to %s/%d",
		       inet_ntoa (p->prefix), p->prefixlen);
	  if ((range = ospf_area_range_match (or_area, p)) &&
              !ospf_area_is_transit (area))
	    ospf_abr_update_aggregate (range, or);
	  else
	    ospf_abr_announce_network_to_area (p, or->cost, area);
	}
    }
}

int
ospf_abr_should_announce (struct ospf *ospf,
			  struct prefix *p, struct ospf_route *or)
{
  struct ospf_area *area;

  area = ospf_area_lookup_by_area_id (ospf, or->u.std.area_id);

  assert (area);
  
  if (EXPORT_NAME (area))
    {
      if (EXPORT_LIST (area) == NULL)
	EXPORT_LIST (area) = access_list_lookup (AFI_IP, EXPORT_NAME (area));

      if (EXPORT_LIST (area))
        if (access_list_apply (EXPORT_LIST (area), p) == FILTER_DENY)
	  return 0;
    }

  return 1;
}

#ifdef HAVE_NSSA
void
ospf_abr_process_nssa_translates (struct ospf *ospf)
{
  /* Scan through all NSSA_LSDB records for all areas;

  If P-bit is on, translate all Type-7's to 5's and aggregate or
  flood install as approved in Type-5 LSDB with XLATE Flag on
  later, do same for all aggregates...  At end, DISCARD all
  remaining UNAPPROVED Type-5's (Aggregate is for future ) */
  listnode node;
  struct ospf_area *area;
  struct route_node *rn;
  struct ospf_lsa *lsa;

  if (IS_DEBUG_OSPF_NSSA)
    zlog_info ("ospf_abr_process_nssa_translates(): Start");

  for (node = listhead (ospf->areas); node; nextnode (node))
    {
      area = getdata (node);

      if (! area->NSSATranslator)
	continue; /* skip if not translator */
      
      if (area->external_routing != OSPF_AREA_NSSA)
	continue;  /* skip if not Nssa Area */

      if (IS_DEBUG_OSPF_NSSA)
	zlog_info ("ospf_abr_process_nssa_translates(): "
		   "looking at area %s", inet_ntoa (area->area_id));
      
      LSDB_LOOP (NSSA_LSDB (area), rn, lsa)
	ospf_abr_translate_nssa (area, lsa);
    }
 
  if (IS_DEBUG_OSPF_NSSA)
    zlog_info ("ospf_abr_process_nssa_translates(): Stop");

}
#endif /* HAVE_NSSA */

void
ospf_abr_process_network_rt (struct ospf *ospf,
			     struct route_table *rt)
{
  struct ospf_area *area;
  struct ospf_route *or;
  struct route_node *rn;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_process_network_rt(): Start");

  for (rn = route_top (rt); rn; rn = route_next (rn))
    {
      if ((or = rn->info) == NULL)
	continue;

      if (!(area = ospf_area_lookup_by_area_id (ospf, or->u.std.area_id)))
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_abr_process_network_rt(): area %s no longer exists",
		       inet_ntoa (or->u.std.area_id));
	  continue;
	}

      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_process_network_rt(): this is a route to %s/%d",
		   inet_ntoa (rn->p.u.prefix4), rn->p.prefixlen);
      if (or->path_type >= OSPF_PATH_TYPE1_EXTERNAL)
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_abr_process_network_rt(): "
		       "this is an External router, skipping");
	  continue;
	}

      if (or->cost >= OSPF_LS_INFINITY)
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_abr_process_network_rt():"
		       " this route's cost is infinity, skipping");
	  continue;
	}

      if (or->type == OSPF_DESTINATION_DISCARD)
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_abr_process_network_rt():"
		       " this is a discard entry, skipping");
	  continue;
	}

      if (or->path_type == OSPF_PATH_INTRA_AREA &&
	  !ospf_abr_should_announce (ospf, &rn->p, or))
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info("ospf_abr_process_network_rt(): denied by export-list");
	  continue;
	}

      if (or->path_type == OSPF_PATH_INTRA_AREA &&
	  !ospf_abr_plist_out_check (area, or, &rn->p))
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info("ospf_abr_process_network_rt(): denied by prefix-list");
	  continue;
	}

      if ((or->path_type == OSPF_PATH_INTER_AREA) &&
          !OSPF_IS_AREA_ID_BACKBONE (or->u.std.area_id))
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_abr_process_network_rt():"
		       " this is route is not backbone one, skipping");
	  continue;
	}


      if ((ospf->abr_type == OSPF_ABR_CISCO) ||
          (ospf->abr_type == OSPF_ABR_IBM))

	if (!ospf_act_bb_connection (ospf) &&
	    or->path_type != OSPF_PATH_INTRA_AREA)
	  {
	    if (IS_DEBUG_OSPF_EVENT)
	      zlog_info ("ospf_abr_process_network_rt(): ALT ABR: "
			 "No BB connection, skip not intra-area routes");
	    continue;
	  }

      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_process_network_rt(): announcing");
      ospf_abr_announce_network (ospf, rn, or);
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_process_network_rt(): Stop");
}

void
ospf_abr_announce_rtr_to_area (struct prefix_ipv4 *p, u_int32_t cost,
			       struct ospf_area *area)
{
  struct ospf_lsa *lsa, *old = NULL;
  struct summary_lsa *slsa = NULL;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_announce_rtr_to_area(): Start");

  old = ospf_lsa_lookup_by_prefix (area->lsdb, OSPF_ASBR_SUMMARY_LSA,
				   p, area->ospf->router_id);
  if (old)
    {
      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_announce_rtr_to_area(): old summary found");
      slsa = (struct summary_lsa *) old->data;

      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_announce_network_to_area(): "
		   "old metric: %d, new metric: %d",
		   GET_METRIC (slsa->metric), cost);
    }

  if (old && (GET_METRIC (slsa->metric) == cost))
    {
      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_announce_rtr_to_area(): old summary approved");
      SET_FLAG (old->flags, OSPF_LSA_APPROVED);
    }
  else
    {
      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_announce_rtr_to_area(): 2.2");
       
      if (old) 
	{ 
	  set_metric (old, cost);
	  lsa = ospf_summary_asbr_lsa_refresh (area->ospf, old);
	}
      else
	lsa = ospf_summary_asbr_lsa_originate (p, cost, area);

      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_announce_rtr_to_area(): "
		   "flooding new version of summary");
      /*
	zlog_info ("ospf_abr_announce_rtr_to_area(): creating new summary");
	lsa = ospf_summary_asbr_lsa (p, cost, area, old); */

      SET_FLAG (lsa->flags, OSPF_LSA_APPROVED);
      /* ospf_flood_through_area (area, NULL, lsa);*/
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_announce_rtr_to_area(): Stop");
}


void
ospf_abr_announce_rtr (struct ospf *ospf,
		       struct prefix_ipv4 *p, struct ospf_route *or)
{
  listnode node;
  struct ospf_area *area;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_announce_rtr(): Start");

  for (node = listhead (ospf->areas); node; nextnode (node))
    {
      area = getdata (node);

      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_announce_rtr(): looking at area %s",
		   inet_ntoa (area->area_id));

      if (IPV4_ADDR_SAME (&or->u.std.area_id, &area->area_id))
	continue;

      if (ospf_abr_nexthops_belong_to_area (or, area))
	continue;

      if (area->external_routing != OSPF_AREA_DEFAULT)
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_abr_announce_network(): "
		       "area %s doesn't support external routing",
		       inet_ntoa(area->area_id));
          continue;
	}

      if (or->path_type == OSPF_PATH_INTER_AREA)
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_abr_announce_rtr(): "
		       "this is inter-area route to %s", inet_ntoa (p->prefix));
          if (!OSPF_IS_AREA_BACKBONE (area))
	    ospf_abr_announce_rtr_to_area (p, or->cost, area);
	}

      if (or->path_type == OSPF_PATH_INTRA_AREA)
	{
	  if (IS_DEBUG_OSPF_EVENT)
	    zlog_info ("ospf_abr_announce_rtr(): "
		       "this is intra-area route to %s", inet_ntoa (p->prefix));
          ospf_abr_announce_rtr_to_area (p, or->cost, area);
	}
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_announce_rtr(): Stop");
}

void
ospf_abr_process_router_rt (struct ospf *ospf, struct route_table *rt)
{
  struct ospf_route *or;
  struct route_node *rn;
  struct list *l;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_process_router_rt(): Start");

  for (rn = route_top (rt); rn; rn = route_next (rn))
    {
      listnode node;
      char flag = 0;
      struct ospf_route *best = NULL;

      if (rn->info == NULL)
	continue;

      l = rn->info;

      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_process_router_rt(): this is a route to %s",
		   inet_ntoa (rn->p.u.prefix4));

      for (node = listhead (l); node; nextnode (node))
	{
	  or = getdata (node);
	  if (or == NULL)
	    continue;

	  if (!ospf_area_lookup_by_area_id (ospf, or->u.std.area_id))
	    {
	      if (IS_DEBUG_OSPF_EVENT)
		zlog_info ("ospf_abr_process_router_rt(): area %s no longer exists",
			   inet_ntoa (or->u.std.area_id));
	      continue;
	    }


	  if (!CHECK_FLAG (or->u.std.flags, ROUTER_LSA_EXTERNAL))
	    {
	      if (IS_DEBUG_OSPF_EVENT)
		zlog_info ("ospf_abr_process_router_rt(): "
			   "This is not an ASBR, skipping");
	      continue;
	    }

	  if (!flag)
	    {
	      best = ospf_find_asbr_route (ospf, rt,
					   (struct prefix_ipv4 *) &rn->p);
	      flag = 1;
	    }
	  
	  if (best == NULL)
	    continue;
	
	  if (or != best)
	    {
	      if (IS_DEBUG_OSPF_EVENT)
		zlog_info ("ospf_abr_process_router_rt(): "
			   "This route is not the best among possible, skipping");
	      continue;
	    }
	
	  if (or->path_type == OSPF_PATH_INTER_AREA &&
	      !OSPF_IS_AREA_ID_BACKBONE (or->u.std.area_id))
	    {
	      if (IS_DEBUG_OSPF_EVENT)
		zlog_info ("ospf_abr_process_router_rt(): "
			   "This route is not a backbone one, skipping");
	      continue;
	    }

	  if (or->cost >= OSPF_LS_INFINITY)
	    {
	      if (IS_DEBUG_OSPF_EVENT)
		zlog_info ("ospf_abr_process_router_rt(): "
			   "This route has LS_INFINITY metric, skipping");
	      continue;
	    }

	  if (ospf->abr_type == OSPF_ABR_CISCO
	      || ospf->abr_type == OSPF_ABR_IBM)
	    if (!ospf_act_bb_connection (ospf)
		&& or->path_type != OSPF_PATH_INTRA_AREA)
	      {
		if (IS_DEBUG_OSPF_EVENT)
		  zlog_info("ospf_abr_process_network_rt(): ALT ABR: "
			    "No BB connection, skip not intra-area routes");
		continue;
	      }

	  ospf_abr_announce_rtr (ospf, (struct prefix_ipv4 *) &rn->p, or);

	}

    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_process_router_rt(): Stop");
}

#ifdef HAVE_NSSA
void
ospf_abr_unapprove_translates (struct ospf *ospf) /* For NSSA Translations */
{
  struct ospf_lsa *lsa;
  struct route_node *rn;

  if (IS_DEBUG_OSPF_NSSA)
    zlog_info ("ospf_abr_unapprove_translates(): Start");

  /* NSSA Translator is not checked, because it may have gone away,
     and we would want to flush any residuals anyway */

  LSDB_LOOP (EXTERNAL_LSDB (ospf), rn, lsa)
    if (CHECK_FLAG (lsa->flags, OSPF_LSA_LOCAL_XLT))
      UNSET_FLAG (lsa->flags, OSPF_LSA_APPROVED);

  if (IS_DEBUG_OSPF_NSSA)
    zlog_info ("ospf_abr_unapprove_translates(): Stop");
}
#endif /* HAVE_NSSA */

void
ospf_abr_unapprove_summaries (struct ospf *ospf)
{
  listnode node;
  struct ospf_area *area;
  struct route_node *rn;
  struct ospf_lsa *lsa;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_unapprove_summaries(): Start");

  for (node = listhead (ospf->areas); node; nextnode (node))
    {
      area = getdata (node);
      LSDB_LOOP (SUMMARY_LSDB (area), rn, lsa)
	if (ospf_lsa_is_self_originated (ospf, lsa))
	  UNSET_FLAG (lsa->flags, OSPF_LSA_APPROVED);

      LSDB_LOOP (ASBR_SUMMARY_LSDB (area), rn, lsa)
	if (ospf_lsa_is_self_originated (ospf, lsa))
	  UNSET_FLAG (lsa->flags, OSPF_LSA_APPROVED);
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_unapprove_summaries(): Stop");
}

void
ospf_abr_prepare_aggregates (struct ospf *ospf)
{
  listnode node;
  struct route_node *rn;
  struct ospf_area_range *range;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_prepare_aggregates(): Start");

  for (node = listhead (ospf->areas); node; nextnode (node))
    {
      struct ospf_area *area = getdata (node);

      for (rn = route_top (area->ranges); rn; rn = route_next (rn))
	if ((range = rn->info) != NULL)
	  {
	    range->cost = 0;
	    range->specifics = 0;
	  }
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_prepare_aggregates(): Stop");
}

void
ospf_abr_announce_aggregates (struct ospf *ospf)
{
  struct ospf_area *area, *ar;
  struct ospf_area_range *range;
  struct route_node *rn;
  struct prefix_ipv4 p;
  listnode node, n;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_announce_aggregates(): Start");

  for (node = listhead (ospf->areas); node; nextnode (node))
    {
      area = getdata (node);

      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_announce_aggregates(): looking at area %s",
		   inet_ntoa (area->area_id));

      for (rn = route_top (area->ranges); rn; rn = route_next (rn))
	if ((range =  rn->info))
	  {
	    if (!CHECK_FLAG (range->flags, OSPF_AREA_RANGE_ADVERTISE))
	      {
		if (IS_DEBUG_OSPF_EVENT)
		  zlog_info ("ospf_abr_announce_aggregates():"
			     " discarding suppress-ranges");
		continue;
	      }

	    p.family = AF_INET;
	    p.prefix = range->addr;
	    p.prefixlen = range->masklen;

	    if (IS_DEBUG_OSPF_EVENT)
	      zlog_info ("ospf_abr_announce_aggregates():"
			 " this is range: %s/%d",
			 inet_ntoa (p.prefix), p.prefixlen);

	    if (CHECK_FLAG (range->flags, OSPF_AREA_RANGE_SUBSTITUTE))
	      {
		p.family = AF_INET;
		p.prefix = range->subst_addr;
		p.prefixlen = range->subst_masklen;
	      }

	    if (range->specifics)
	      {
		if (IS_DEBUG_OSPF_EVENT)
		  zlog_info ("ospf_abr_announce_aggregates(): active range");

		for (n = listhead (ospf->areas); n; nextnode (n))
		  {
		    ar = getdata (n);
		    if (ar == area)
		      continue;

		    /* We do not check nexthops here, because
		       intra-area routes can be associated with
		       one area only */

		    /* backbone routes are not summarized
		       when announced into transit areas */

		    if (ospf_area_is_transit (ar) &&
			OSPF_IS_AREA_BACKBONE (area))
		      {
			if (IS_DEBUG_OSPF_EVENT)
			  zlog_info ("ospf_abr_announce_aggregates(): Skipping "
				     "announcement of BB aggregate into"
				     " a transit area");
			continue; 
		      }
		    ospf_abr_announce_network_to_area (&p, range->cost, ar);
		  }
	      }
	  }
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_announce_aggregates(): Stop");
}

#ifdef HAVE_NSSA
void
ospf_abr_send_nssa_aggregates (struct ospf *ospf) /* temporarily turned off */
{
  listnode node; /*, n; */
  struct ospf_area *area; /*, *ar; */
  struct route_node *rn;
  struct ospf_area_range *range;
  struct prefix_ipv4 p;

  if (IS_DEBUG_OSPF_NSSA)
    zlog_info ("ospf_abr_send_nssa_aggregates(): Start");

  for (node = listhead (ospf->areas); node; nextnode (node))
    {
      area = getdata (node);

      if (! area->NSSATranslator)
	continue;

      if (IS_DEBUG_OSPF_NSSA)
	zlog_info ("ospf_abr_send_nssa_aggregates(): looking at area %s",
		   inet_ntoa (area->area_id));

      for (rn = route_top (area->ranges); rn; rn = route_next (rn))
	{
          if (rn->info == NULL)
	    continue;

	  range = rn->info;

          if (!CHECK_FLAG (range->flags, OSPF_AREA_RANGE_ADVERTISE))
	    {
	      if (IS_DEBUG_OSPF_NSSA)
		zlog_info ("ospf_abr_send_nssa_aggregates():"
			   " discarding suppress-ranges");
	      continue;
	    }

          p.family = AF_INET;
          p.prefix = range->addr;
          p.prefixlen = range->masklen;

	  if (IS_DEBUG_OSPF_NSSA)
	    zlog_info ("ospf_abr_send_nssa_aggregates():"
		       " this is range: %s/%d",
		       inet_ntoa (p.prefix), p.prefixlen);

          if (CHECK_FLAG (range->flags, OSPF_AREA_RANGE_SUBSTITUTE))
	    {
	      p.family = AF_INET;
	      p.prefix = range->subst_addr;
	      p.prefixlen = range->subst_masklen;
	    }

          if (range->specifics)
	    {
	      if (IS_DEBUG_OSPF_NSSA)
		zlog_info ("ospf_abr_send_nssa_aggregates(): active range");

	      /* Fetch LSA-Type-7 from aggregate prefix, and then
                 translate, Install (as Type-5), Approve, and Flood */
	      ospf_abr_translate_nssa_range (&p, range->cost);
	    } /* if (range->specifics)*/
	} /* all area ranges*/
    } /* all areas */

  if (IS_DEBUG_OSPF_NSSA)
    zlog_info ("ospf_abr_send_nssa_aggregates(): Stop");
}

void
ospf_abr_announce_nssa_defaults (struct ospf *ospf) /* By ABR-Translator */
{
  listnode node;
  struct ospf_area *area;
  struct prefix_ipv4 p;

  if (! IS_OSPF_ABR (ospf))
    return;

  if (IS_DEBUG_OSPF_NSSA)
    zlog_info ("ospf_abr_announce_stub_defaults(): Start");

  p.family = AF_INET;
  p.prefix.s_addr = OSPF_DEFAULT_DESTINATION;
  p.prefixlen = 0;

  for (node = listhead (ospf->areas); node; nextnode (node))
    {
      area = getdata (node);
      if (IS_DEBUG_OSPF_NSSA)
	zlog_info ("ospf_abr_announce_nssa_defaults(): looking at area %s",
		   inet_ntoa (area->area_id));

      if (area->external_routing != OSPF_AREA_NSSA)
	continue;

      if (OSPF_IS_AREA_BACKBONE (area))
	continue; /* Sanity Check */

      /* if (!TranslatorRole continue V 1.0 look for "always" conf */
      if (area->NSSATranslator)
	{
	  if (IS_DEBUG_OSPF_NSSA)
	    zlog_info ("ospf_abr_announce_nssa_defaults(): "
		       "announcing 0.0.0.0/0 to this nssa");
	  /* ospf_abr_announce_nssa_asbr_to_as (&p, area->default_cost, area); */
	}
    }
}
#endif /* HAVE_NSSA */

void
ospf_abr_announce_stub_defaults (struct ospf *ospf)
{
  listnode node;
  struct ospf_area *area;
  struct prefix_ipv4 p;

  if (! IS_OSPF_ABR (ospf))
    return;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_announce_stub_defaults(): Start");

  p.family = AF_INET;
  p.prefix.s_addr = OSPF_DEFAULT_DESTINATION;
  p.prefixlen = 0;

  for (node = listhead (ospf->areas); node; nextnode (node))
    {
      area = getdata (node);
      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_announce_stub_defaults(): looking at area %s",
		   inet_ntoa (area->area_id));

#ifdef HAVE_NSSA
      if (area->external_routing != OSPF_AREA_STUB)
#else /* ! HAVE_NSSA */
	if (area->external_routing == OSPF_AREA_DEFAULT)
#endif /* HAVE_NSSA */
	  continue;

      if (OSPF_IS_AREA_BACKBONE (area))
	continue; /* Sanity Check */

      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_announce_stub_defaults(): "
		   "announcing 0.0.0.0/0 to this area");
      ospf_abr_announce_network_to_area (&p, area->default_cost, area);
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_announce_stub_defaults(): Stop");
}

#ifdef HAVE_NSSA
int
ospf_abr_remove_unapproved_translates_apply (struct ospf *ospf,
					     struct ospf_lsa *lsa)
{
  if (CHECK_FLAG (lsa->flags, OSPF_LSA_LOCAL_XLT)
      && ! CHECK_FLAG (lsa->flags, OSPF_LSA_APPROVED))
    {
      zlog_info ("ospf_abr_remove_unapproved_translates(): "
		 "removing unapproved translates, ID: %s",
		 inet_ntoa (lsa->data->id));

      /* FLUSH THROUGHOUT AS */
      ospf_lsa_flush_as (ospf, lsa);

      /* DISCARD from LSDB  */
    }
  return 0;
}

void
ospf_abr_remove_unapproved_translates (struct ospf *ospf)
{
  struct route_node *rn;
  struct ospf_lsa *lsa;

  /* All AREA PROCESS should have APPROVED necessary LSAs */
  /* Remove any left over and not APPROVED */
  if (IS_DEBUG_OSPF_NSSA)
    zlog_info ("ospf_abr_remove_unapproved_translates(): Start");

  LSDB_LOOP (EXTERNAL_LSDB (ospf), rn, lsa)
    ospf_abr_remove_unapproved_translates_apply (ospf, lsa);
 
  if (IS_DEBUG_OSPF_NSSA)
    zlog_info ("ospf_abr_remove_unapproved_translates(): Stop");
}
#endif /* HAVE_NSSA */

void
ospf_abr_remove_unapproved_summaries (struct ospf *ospf)
{
  listnode node;
  struct ospf_area *area;
  struct route_node *rn;
  struct ospf_lsa *lsa;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_remove_unapproved_summaries(): Start");

  for (node = listhead (ospf->areas); node; nextnode (node))
    {
      area = getdata (node);

      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_remove_unapproved_summaries(): "
		   "looking at area %s", inet_ntoa (area->area_id));

      LSDB_LOOP (SUMMARY_LSDB (area), rn, lsa)
	if (ospf_lsa_is_self_originated (ospf, lsa))
	  if (!CHECK_FLAG (lsa->flags, OSPF_LSA_APPROVED))
	    ospf_lsa_flush_area (lsa, area);

      LSDB_LOOP (ASBR_SUMMARY_LSDB (area), rn, lsa)
	if (ospf_lsa_is_self_originated (ospf, lsa))
	  if (!CHECK_FLAG (lsa->flags, OSPF_LSA_APPROVED))
	    ospf_lsa_flush_area (lsa, area);
    }
 
  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_remove_unapproved_summaries(): Stop");
}

void
ospf_abr_manage_discard_routes (struct ospf *ospf)
{
  listnode node;
  struct route_node *rn;
  struct ospf_area *area;
  struct ospf_area_range *range;

  for (node = listhead (ospf->areas); node; nextnode (node))
    if ((area = node->data) != NULL)
      for (rn = route_top (area->ranges); rn; rn = route_next (rn))
	if ((range = rn->info) != NULL)
	  if (CHECK_FLAG (range->flags, OSPF_AREA_RANGE_ADVERTISE))
	    {
	      if (range->specifics)
		ospf_add_discard_route (ospf->new_table, area,
					(struct prefix_ipv4 *) &rn->p);
	      else
		ospf_delete_discard_route ((struct prefix_ipv4 *) &rn->p);
	    }
}

#ifdef HAVE_NSSA
/* This is the function taking care about ABR NSSA, i.e.  NSSA
   Translator, -LSA aggregation and flooding. For all NSSAs

   Any SELF-AS-LSA is in the Type-5 LSDB and Type-7 LSDB.  These LSA's
   are refreshed from the Type-5 LSDB, installed into the Type-7 LSDB
   with the P-bit set.

   Any received Type-5s are legal for an ABR, else illegal for IR.
   Received Type-7s are installed, by area, with incoming P-bit.  They
   are flooded; if the Elected NSSA Translator, then P-bit off.

   Additionally, this ABR will place "translated type-7's" into the
   Type-5 LSDB in order to keep track of APPROVAL or not.

   It will scan through every area, looking for Type-7 LSAs with P-Bit
   SET. The Type-7's are either AS-FLOODED & 5-INSTALLED or
   AGGREGATED.  Later, the AGGREGATED LSAs are AS-FLOODED &
   5-INSTALLED.

   5-INSTALLED is into the Type-5 LSDB; Any UNAPPROVED Type-5 LSAs
   left over are FLUSHED and DISCARDED.

   For External Calculations, any NSSA areas use the Type-7 AREA-LSDB,
   any ABR-non-NSSA areas use the Type-5 GLOBAL-LSDB. */

void
ospf_abr_nssa_task (struct ospf *ospf) /* called only if any_nssa */
{
  if (IS_DEBUG_OSPF_NSSA)
    zlog_info ("Check for NSSA-ABR Tasks():");

  if (! IS_OSPF_ABR (ospf))
    return;

  if (! ospf->anyNSSA)
    return;

  /* Each area must confirm TranslatorRole */
  if (IS_DEBUG_OSPF_NSSA)
    zlog_info ("ospf_abr_nssa_task(): Start");

  /* For all Global Entries flagged "local-translate", unset APPROVED */
  if (IS_DEBUG_OSPF_NSSA)
    zlog_info ("ospf_abr_nssa_task(): unapprove translates");

  ospf_abr_unapprove_translates (ospf);

  /* RESET all Ranges in every Area, same as summaries */
  if (IS_DEBUG_OSPF_NSSA)
    zlog_info ("ospf_abr_nssa_task(): NSSA initialize aggregates");

  /*    ospf_abr_prepare_aggregates ();  TURNED OFF just for now */

  /* For all NSSAs, Type-7s, translate to 5's, INSTALL/FLOOD, or
     Aggregate as Type-7 */
  /* Install or Approve in Type-5 Global LSDB */
  if (IS_DEBUG_OSPF_NSSA)
    zlog_info ("ospf_abr_nssa_task(): process translates");

  ospf_abr_process_nssa_translates (ospf);

  /* Translate/Send any "ranged" aggregates, and also 5-Install and
     Approve */
  /* Scan Type-7's for aggregates, translate to Type-5's,
     Install/Flood/Approve */
  if (IS_DEBUG_OSPF_NSSA)
    zlog_info("ospf_abr_nssa_task(): send NSSA aggregates");
  /*       ospf_abr_send_nssa_aggregates ();  TURNED OFF FOR NOW */

  /* Send any NSSA defaults as Type-5 */
  if (IS_DEBUG_OSPF_NSSA)
    zlog_info ("ospf_abr_nssa_task(): announce nssa defaults");
  ospf_abr_announce_nssa_defaults (ospf);
   
  /* Flush any unapproved previous translates from Global Data Base */
  if (IS_DEBUG_OSPF_NSSA)
    zlog_info ("ospf_abr_nssa_task(): remove unapproved translates");
  ospf_abr_remove_unapproved_translates (ospf);

  ospf_abr_manage_discard_routes (ospf); /* same as normal...discard */

  if (IS_DEBUG_OSPF_NSSA)
    zlog_info ("ospf_abr_nssa_task(): Stop");
}
#endif /* HAVE_NSSA */

/* This is the function taking care about ABR stuff, i.e.
   summary-LSA origination and flooding. */
void
ospf_abr_task (struct ospf *ospf)
{
  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_task(): Start");

  if (ospf->new_table == NULL || ospf->new_rtrs == NULL)
    {
      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_task(): Routing tables are not yet ready");
      return;
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_task(): unapprove summaries");
  ospf_abr_unapprove_summaries (ospf);

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_task(): prepare aggregates");
  ospf_abr_prepare_aggregates (ospf);

  if (IS_OSPF_ABR (ospf))
    {
      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_task(): process network RT");
      ospf_abr_process_network_rt (ospf, ospf->new_table);

      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_task(): process router RT");
      ospf_abr_process_router_rt (ospf, ospf->new_rtrs);

      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_task(): announce aggregates");
      ospf_abr_announce_aggregates (ospf);

      if (IS_DEBUG_OSPF_EVENT)
	zlog_info ("ospf_abr_task(): announce stub defaults");
      ospf_abr_announce_stub_defaults (ospf);
    }

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_task(): remove unapproved summaries");
  ospf_abr_remove_unapproved_summaries (ospf);

  ospf_abr_manage_discard_routes (ospf);

#ifdef HAVE_NSSA
  ospf_abr_nssa_task (ospf); /* if nssa-abr, then scan Type-7 LSDB */
#endif /* HAVE_NSSA */

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("ospf_abr_task(): Stop");
}


int
ospf_abr_task_timer (struct thread *thread)
{
  struct ospf *ospf = THREAD_ARG (thread);

  ospf->t_abr_task = 0;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Running ABR task on timer");

  ospf_check_abr_status (ospf);

  ospf_abr_task (ospf);

  return 0;
}

void
ospf_schedule_abr_task (struct ospf *ospf)
{
  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Scheduling ABR task");

  if (ospf->t_abr_task == NULL)
    ospf->t_abr_task = thread_add_timer (master, ospf_abr_task_timer,
					 ospf, OSPF_ABR_TASK_DELAY);
}
