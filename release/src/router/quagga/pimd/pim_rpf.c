/*
  PIM for Quagga
  Copyright (C) 2008  Everton da Silva Marques

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING; if not, write to the
  Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
  MA 02110-1301 USA
  
  $QuaggaId: $Format:%an, %ai, %h$ $
*/

#include <zebra.h>

#include "log.h"
#include "prefix.h"
#include "memory.h"

#include "pimd.h"
#include "pim_rpf.h"
#include "pim_pim.h"
#include "pim_str.h"
#include "pim_iface.h"
#include "pim_zlookup.h"
#include "pim_ifchannel.h"

static struct in_addr pim_rpf_find_rpf_addr(struct pim_upstream *up);

int pim_nexthop_lookup(struct pim_nexthop *nexthop,
		       struct in_addr addr)
{
  struct pim_zlookup_nexthop nexthop_tab[PIM_NEXTHOP_IFINDEX_TAB_SIZE];
  int num_ifindex;
  struct interface *ifp;
  int first_ifindex;

  num_ifindex = zclient_lookup_nexthop(qpim_zclient_lookup, nexthop_tab,
				       PIM_NEXTHOP_IFINDEX_TAB_SIZE,
				       addr, PIM_NEXTHOP_LOOKUP_MAX);
  if (num_ifindex < 1) {
    char addr_str[100];
    pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
    zlog_warn("%s %s: could not find nexthop ifindex for address %s",
	      __FILE__, __PRETTY_FUNCTION__,
	      addr_str);
    return -1;
  }

  first_ifindex = nexthop_tab[0].ifindex;

  if (num_ifindex > 1) {
    char addr_str[100];
    pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
    zlog_debug("%s %s: FIXME ignoring multiple nexthop ifindex'es num_ifindex=%d for address %s (using only ifindex=%d)",
	      __FILE__, __PRETTY_FUNCTION__,
	      num_ifindex, addr_str, first_ifindex);
    /* debug warning only, do not return */
  }

  ifp = if_lookup_by_index(first_ifindex);
  if (!ifp) {
    char addr_str[100];
    pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
    zlog_warn("%s %s: could not find interface for ifindex %d (address %s)",
	      __FILE__, __PRETTY_FUNCTION__,
	      first_ifindex, addr_str);
    return -2;
  }

  if (!ifp->info) {
    char addr_str[100];
    pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
    zlog_warn("%s: multicast not enabled on input interface %s (ifindex=%d, RPF for source %s)",
	      __PRETTY_FUNCTION__,
	      ifp->name, first_ifindex, addr_str);
    /* debug warning only, do not return */
  }

  if (PIM_DEBUG_PIM_TRACE) {
    char nexthop_str[100];
    char addr_str[100];
    pim_inet4_dump("<nexthop?>", nexthop_tab[0].nexthop_addr, nexthop_str, sizeof(nexthop_str));
    pim_inet4_dump("<addr?>", addr, addr_str, sizeof(addr_str));
    zlog_debug("%s %s: found nexthop %s for address %s: interface %s ifindex=%d metric=%d pref=%d",
	       __FILE__, __PRETTY_FUNCTION__,
	       nexthop_str, addr_str,
	       ifp->name, first_ifindex,
	       nexthop_tab[0].route_metric,
	       nexthop_tab[0].protocol_distance);
  }

  /* update nextop data */
  nexthop->interface              = ifp;
  nexthop->mrib_nexthop_addr      = nexthop_tab[0].nexthop_addr;
  nexthop->mrib_metric_preference = nexthop_tab[0].protocol_distance;
  nexthop->mrib_route_metric      = nexthop_tab[0].route_metric;

  return 0;
}

static int nexthop_mismatch(const struct pim_nexthop *nh1,
			    const struct pim_nexthop *nh2)
{
  return (nh1->interface != nh2->interface) 
    ||
    (nh1->mrib_nexthop_addr.s_addr != nh2->mrib_nexthop_addr.s_addr)
    ||
    (nh1->mrib_metric_preference != nh2->mrib_metric_preference)
    ||
    (nh1->mrib_route_metric != nh2->mrib_route_metric);
}

enum pim_rpf_result pim_rpf_update(struct pim_upstream *up,
				   struct in_addr *old_rpf_addr)
{
  struct in_addr      save_rpf_addr;
  struct pim_nexthop  save_nexthop;
  struct pim_rpf     *rpf = &up->rpf;

  save_nexthop  = rpf->source_nexthop; /* detect change in pim_nexthop */
  save_rpf_addr = rpf->rpf_addr;       /* detect change in RPF'(S,G) */

  if (pim_nexthop_lookup(&rpf->source_nexthop,
			 up->source_addr)) {
    return PIM_RPF_FAILURE;
  }

  rpf->rpf_addr = pim_rpf_find_rpf_addr(up);
  if (PIM_INADDR_IS_ANY(rpf->rpf_addr)) {
    /* RPF'(S,G) not found */
    char src_str[100];
    char grp_str[100];
    pim_inet4_dump("<src?>", up->source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", up->group_addr, grp_str, sizeof(grp_str));
    zlog_warn("%s %s: RPF'(%s,%s) not found: won't send join upstream",
              __FILE__, __PRETTY_FUNCTION__,
              src_str, grp_str);
    /* warning only */
  }

  /* detect change in pim_nexthop */
  if (nexthop_mismatch(&rpf->source_nexthop, &save_nexthop)) {

    /* if (PIM_DEBUG_PIM_EVENTS) */ {
      char src_str[100];
      char grp_str[100];
      char nhaddr_str[100];
      pim_inet4_dump("<src?>", up->source_addr, src_str, sizeof(src_str));
      pim_inet4_dump("<grp?>", up->group_addr, grp_str, sizeof(grp_str));
      pim_inet4_dump("<addr?>", rpf->source_nexthop.mrib_nexthop_addr, nhaddr_str, sizeof(nhaddr_str));
      zlog_debug("%s %s: (S,G)=(%s,%s) source nexthop now is: interface=%s address=%s pref=%d metric=%d",
		 __FILE__, __PRETTY_FUNCTION__,
		 src_str, grp_str,
		 rpf->source_nexthop.interface ? rpf->source_nexthop.interface->name : "<ifname?>",
		 nhaddr_str,
		 rpf->source_nexthop.mrib_metric_preference,
		 rpf->source_nexthop.mrib_route_metric);
      /* warning only */
    }

    pim_upstream_update_join_desired(up);
    pim_upstream_update_could_assert(up);
    pim_upstream_update_my_assert_metric(up);
  }

  /* detect change in RPF_interface(S) */
  if (save_nexthop.interface != rpf->source_nexthop.interface) {

    /* if (PIM_DEBUG_PIM_EVENTS) */ {
      char src_str[100];
      char grp_str[100];
      pim_inet4_dump("<src?>", up->source_addr, src_str, sizeof(src_str));
      pim_inet4_dump("<grp?>", up->group_addr, grp_str, sizeof(grp_str));
      zlog_debug("%s %s: (S,G)=(%s,%s) RPF_interface(S) changed from %s to %s",
		 __FILE__, __PRETTY_FUNCTION__,
		 src_str, grp_str,
		 save_nexthop.interface ? save_nexthop.interface->name : "<oldif?>",
		 rpf->source_nexthop.interface ? rpf->source_nexthop.interface->name : "<newif?>");
      /* warning only */
    }

    pim_upstream_rpf_interface_changed(up, save_nexthop.interface);
  }

  /* detect change in RPF'(S,G) */
  if (save_rpf_addr.s_addr != rpf->rpf_addr.s_addr) {

    /* return old rpf to caller ? */
    if (old_rpf_addr)
      *old_rpf_addr = save_rpf_addr;

    return PIM_RPF_CHANGED;
  }

  return PIM_RPF_OK;
}

/*
  RFC 4601: 4.1.6.  State Summarization Macros

     neighbor RPF'(S,G) {
         if ( I_Am_Assert_Loser(S, G, RPF_interface(S) )) {
              return AssertWinner(S, G, RPF_interface(S) )
         } else {
              return NBR( RPF_interface(S), MRIB.next_hop( S ) )
         }
     }

  RPF'(*,G) and RPF'(S,G) indicate the neighbor from which data
  packets should be coming and to which joins should be sent on the RP
  tree and SPT, respectively.
*/
static struct in_addr pim_rpf_find_rpf_addr(struct pim_upstream *up)
{
  struct pim_ifchannel *rpf_ch;
  struct pim_neighbor *neigh;
  struct in_addr rpf_addr;

  if (!up->rpf.source_nexthop.interface) {
    char src_str[100];
    char grp_str[100];
    pim_inet4_dump("<src?>", up->source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", up->group_addr, grp_str, sizeof(grp_str));
    zlog_warn("%s: missing RPF interface for upstream (S,G)=(%s,%s)",
	      __PRETTY_FUNCTION__,
	      src_str, grp_str);

    rpf_addr.s_addr = PIM_NET_INADDR_ANY;
    return rpf_addr;
  }

  rpf_ch = pim_ifchannel_find(up->rpf.source_nexthop.interface,
			      up->source_addr, up->group_addr);
  if (rpf_ch) {
    if (rpf_ch->ifassert_state == PIM_IFASSERT_I_AM_LOSER) {
      return rpf_ch->ifassert_winner;
    }
  }

  /* return NBR( RPF_interface(S), MRIB.next_hop( S ) ) */

  neigh = pim_if_find_neighbor(up->rpf.source_nexthop.interface,
			       up->rpf.source_nexthop.mrib_nexthop_addr);
  if (neigh)
    rpf_addr = neigh->source_addr;
  else
    rpf_addr.s_addr = PIM_NET_INADDR_ANY;

  return rpf_addr;
}
