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

#include "zebra/rib.h"

#include "log.h"
#include "zclient.h"
#include "memory.h"
#include "thread.h"
#include "linklist.h"

#include "pimd.h"
#include "pim_pim.h"
#include "pim_str.h"
#include "pim_time.h"
#include "pim_iface.h"
#include "pim_join.h"
#include "pim_zlookup.h"
#include "pim_upstream.h"
#include "pim_ifchannel.h"
#include "pim_neighbor.h"
#include "pim_rpf.h"
#include "pim_zebra.h"
#include "pim_oil.h"
#include "pim_macro.h"

static void join_timer_start(struct pim_upstream *up);
static void pim_upstream_update_assert_tracking_desired(struct pim_upstream *up);

void pim_upstream_free(struct pim_upstream *up)
{
  XFREE(MTYPE_PIM_UPSTREAM, up);
}

static void upstream_channel_oil_detach(struct pim_upstream *up)
{
  if (up->channel_oil) {
    pim_channel_oil_del(up->channel_oil);
    up->channel_oil = 0;
  }
}

void pim_upstream_delete(struct pim_upstream *up)
{
  THREAD_OFF(up->t_join_timer);

  upstream_channel_oil_detach(up);

  /*
    notice that listnode_delete() can't be moved
    into pim_upstream_free() because the later is
    called by list_delete_all_node()
  */
  listnode_delete(qpim_upstream_list, up);

  pim_upstream_free(up);
}

static void send_join(struct pim_upstream *up)
{
  zassert(up->join_state == PIM_UPSTREAM_JOINED);

  
  if (PIM_DEBUG_PIM_TRACE) {
    if (PIM_INADDR_IS_ANY(up->rpf.rpf_addr)) {
      char src_str[100];
      char grp_str[100];
      char rpf_str[100];
      pim_inet4_dump("<src?>", up->source_addr, src_str, sizeof(src_str));
      pim_inet4_dump("<grp?>", up->group_addr, grp_str, sizeof(grp_str));
      pim_inet4_dump("<rpf?>", up->rpf.rpf_addr, rpf_str, sizeof(rpf_str));
      zlog_warn("%s: can't send join upstream: RPF'(%s,%s)=%s",
		__PRETTY_FUNCTION__,
		src_str, grp_str, rpf_str);
      /* warning only */
    }
  }
  
  /* send Join(S,G) to the current upstream neighbor */
  pim_joinprune_send(up->rpf.source_nexthop.interface,
  		     up->rpf.rpf_addr,
		     up->source_addr,
		     up->group_addr,
		     1 /* join */);
}

static int on_join_timer(struct thread *t)
{
  struct pim_upstream *up;

  zassert(t);
  up = THREAD_ARG(t);
  zassert(up);

  send_join(up);

  up->t_join_timer = 0;
  join_timer_start(up);

  return 0;
}

static void join_timer_start(struct pim_upstream *up)
{
  if (PIM_DEBUG_PIM_EVENTS) {
    char src_str[100];
    char grp_str[100];
    pim_inet4_dump("<src?>", up->source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", up->group_addr, grp_str, sizeof(grp_str));
    zlog_debug("%s: starting %d sec timer for upstream (S,G)=(%s,%s)",
	       __PRETTY_FUNCTION__,
	       qpim_t_periodic,
	       src_str, grp_str);
  }

  zassert(!up->t_join_timer);

  THREAD_TIMER_ON(master, up->t_join_timer,
		  on_join_timer,
		  up, qpim_t_periodic);
}

void pim_upstream_join_timer_restart(struct pim_upstream *up)
{
  THREAD_OFF(up->t_join_timer);
  join_timer_start(up);
}

static void pim_upstream_join_timer_restart_msec(struct pim_upstream *up,
						 int interval_msec)
{
  if (PIM_DEBUG_PIM_EVENTS) {
    char src_str[100];
    char grp_str[100];
    pim_inet4_dump("<src?>", up->source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", up->group_addr, grp_str, sizeof(grp_str));
    zlog_debug("%s: restarting %d msec timer for upstream (S,G)=(%s,%s)",
	       __PRETTY_FUNCTION__,
	       interval_msec,
	       src_str, grp_str);
  }

  THREAD_OFF(up->t_join_timer);
  THREAD_TIMER_MSEC_ON(master, up->t_join_timer,
		       on_join_timer,
		       up, interval_msec);
}

void pim_upstream_join_suppress(struct pim_upstream *up,
				struct in_addr rpf_addr,
				int holdtime)
{
  long t_joinsuppress_msec;
  long join_timer_remain_msec;

  t_joinsuppress_msec = MIN(pim_if_t_suppressed_msec(up->rpf.source_nexthop.interface),
			    1000 * holdtime);

  join_timer_remain_msec = pim_time_timer_remain_msec(up->t_join_timer);

  if (PIM_DEBUG_PIM_TRACE) {
    char src_str[100];
    char grp_str[100];
    char rpf_str[100];
    pim_inet4_dump("<src?>", up->source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", up->group_addr, grp_str, sizeof(grp_str));
    pim_inet4_dump("<rpf?>", rpf_addr, rpf_str, sizeof(rpf_str));
    zlog_debug("%s %s: detected Join(%s,%s) to RPF'(S,G)=%s: join_timer=%ld msec t_joinsuppress=%ld msec",
	       __FILE__, __PRETTY_FUNCTION__, 
	       src_str, grp_str,
	       rpf_str,
	       join_timer_remain_msec, t_joinsuppress_msec);
  }

  if (join_timer_remain_msec < t_joinsuppress_msec) {
    if (PIM_DEBUG_PIM_TRACE) {
      char src_str[100];
      char grp_str[100];
      pim_inet4_dump("<src?>", up->source_addr, src_str, sizeof(src_str));
      pim_inet4_dump("<grp?>", up->group_addr, grp_str, sizeof(grp_str));
      zlog_debug("%s %s: suppressing Join(S,G)=(%s,%s) for %ld msec",
		 __FILE__, __PRETTY_FUNCTION__, 
		 src_str, grp_str, t_joinsuppress_msec);
    }

    pim_upstream_join_timer_restart_msec(up, t_joinsuppress_msec);
  }
}

void pim_upstream_join_timer_decrease_to_t_override(const char *debug_label,
						    struct pim_upstream *up,
						    struct in_addr rpf_addr)
{
  long join_timer_remain_msec;
  int t_override_msec;

  join_timer_remain_msec = pim_time_timer_remain_msec(up->t_join_timer);
  t_override_msec = pim_if_t_override_msec(up->rpf.source_nexthop.interface);

  if (PIM_DEBUG_PIM_TRACE) {
    char src_str[100];
    char grp_str[100];
    char rpf_str[100];
    pim_inet4_dump("<src?>", up->source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", up->group_addr, grp_str, sizeof(grp_str));
    pim_inet4_dump("<rpf?>", rpf_addr, rpf_str, sizeof(rpf_str));
    zlog_debug("%s: to RPF'(%s,%s)=%s: join_timer=%ld msec t_override=%d msec",
	       debug_label,
	       src_str, grp_str, rpf_str,
	       join_timer_remain_msec, t_override_msec);
  }
    
  if (join_timer_remain_msec > t_override_msec) {
    if (PIM_DEBUG_PIM_TRACE) {
      char src_str[100];
      char grp_str[100];
      pim_inet4_dump("<src?>", up->source_addr, src_str, sizeof(src_str));
      pim_inet4_dump("<grp?>", up->group_addr, grp_str, sizeof(grp_str));
      zlog_debug("%s: decreasing (S,G)=(%s,%s) join timer to t_override=%d msec",
		 debug_label,
		 src_str, grp_str,
		 t_override_msec);
    }

    pim_upstream_join_timer_restart_msec(up, t_override_msec);
  }
}

static void forward_on(struct pim_upstream *up)
{
  struct listnode      *ifnode;
  struct listnode      *ifnextnode;
  struct listnode      *chnode;
  struct listnode      *chnextnode;
  struct interface     *ifp;
  struct pim_interface *pim_ifp;
  struct pim_ifchannel *ch;

  /* scan all interfaces */
  for (ALL_LIST_ELEMENTS(iflist, ifnode, ifnextnode, ifp)) {
    pim_ifp = ifp->info;
    if (!pim_ifp)
      continue;

    /* scan per-interface (S,G) state */
    for (ALL_LIST_ELEMENTS(pim_ifp->pim_ifchannel_list, chnode, chnextnode, ch)) {

      if (ch->upstream != up)
	continue;

      if (pim_macro_chisin_oiflist(ch))
	pim_forward_start(ch);

    } /* scan iface channel list */
  } /* scan iflist */
}

static void forward_off(struct pim_upstream *up)
{
  struct listnode      *ifnode;
  struct listnode      *ifnextnode;
  struct listnode      *chnode;
  struct listnode      *chnextnode;
  struct interface     *ifp;
  struct pim_interface *pim_ifp;
  struct pim_ifchannel *ch;

  /* scan all interfaces */
  for (ALL_LIST_ELEMENTS(iflist, ifnode, ifnextnode, ifp)) {
    pim_ifp = ifp->info;
    if (!pim_ifp)
      continue;

    /* scan per-interface (S,G) state */
    for (ALL_LIST_ELEMENTS(pim_ifp->pim_ifchannel_list, chnode, chnextnode, ch)) {

      if (ch->upstream != up)
	continue;

      pim_forward_stop(ch);

    } /* scan iface channel list */
  } /* scan iflist */
}

static void pim_upstream_switch(struct pim_upstream *up,
				enum pim_upstream_state new_state)
{
  enum pim_upstream_state old_state = up->join_state;

  zassert(old_state != new_state);
  
  up->join_state       = new_state;
  up->state_transition = pim_time_monotonic_sec();

  if (PIM_DEBUG_PIM_EVENTS) {
    char src_str[100];
    char grp_str[100];
    pim_inet4_dump("<src?>", up->source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", up->group_addr, grp_str, sizeof(grp_str));
    zlog_debug("%s: PIM_UPSTREAM_%s: (S,G)=(%s,%s)",
	       __PRETTY_FUNCTION__,
	       ((new_state == PIM_UPSTREAM_JOINED) ? "JOINED" : "NOTJOINED"),
	       src_str, grp_str);
  }

  pim_upstream_update_assert_tracking_desired(up);

  if (new_state == PIM_UPSTREAM_JOINED) {
    forward_on(up);
    send_join(up);
    join_timer_start(up);
  }
  else {
    forward_off(up);
    pim_joinprune_send(up->rpf.source_nexthop.interface,
		       up->rpf.rpf_addr,
		       up->source_addr,
		       up->group_addr,
		       0 /* prune */);
    zassert(up->t_join_timer);
    THREAD_OFF(up->t_join_timer);
  }

}

static struct pim_upstream *pim_upstream_new(struct in_addr source_addr,
					     struct in_addr group_addr)
{
  struct pim_upstream *up;

  up = XMALLOC(MTYPE_PIM_UPSTREAM, sizeof(*up));
  if (!up) {
    zlog_err("%s: PIM XMALLOC(%zu) failure",
	     __PRETTY_FUNCTION__, sizeof(*up));
    return 0;
  }
  
  up->source_addr                = source_addr;
  up->group_addr                 = group_addr;
  up->flags                      = 0;
  up->ref_count                  = 1;
  up->t_join_timer               = 0;
  up->join_state                 = 0;
  up->state_transition           = pim_time_monotonic_sec();
  up->channel_oil                = 0;

  up->rpf.source_nexthop.interface                = 0;
  up->rpf.source_nexthop.mrib_nexthop_addr.s_addr = PIM_NET_INADDR_ANY;
  up->rpf.source_nexthop.mrib_metric_preference   = qpim_infinite_assert_metric.metric_preference;
  up->rpf.source_nexthop.mrib_route_metric        = qpim_infinite_assert_metric.route_metric;
  up->rpf.rpf_addr.s_addr                         = PIM_NET_INADDR_ANY;

  pim_rpf_update(up, 0);

  listnode_add(qpim_upstream_list, up);

  return up;
}

struct pim_upstream *pim_upstream_find(struct in_addr source_addr,
				       struct in_addr group_addr)
{
  struct listnode     *up_node;
  struct pim_upstream *up;

  for (ALL_LIST_ELEMENTS_RO(qpim_upstream_list, up_node, up)) {
    if (
	(source_addr.s_addr == up->source_addr.s_addr) &&
	(group_addr.s_addr == up->group_addr.s_addr)
	) {
      return up;
    }
  }

  return 0;
}

struct pim_upstream *pim_upstream_add(struct in_addr source_addr,
				      struct in_addr group_addr)
{
  struct pim_upstream *up;

  up = pim_upstream_find(source_addr, group_addr);
  if (up) {
    ++up->ref_count;
  }
  else {
    up = pim_upstream_new(source_addr, group_addr);
  }

  return up;
}

void pim_upstream_del(struct pim_upstream *up)
{
  --up->ref_count;

  if (up->ref_count < 1) {
    pim_upstream_delete(up);
  }
}

/*
  Evaluate JoinDesired(S,G):

  JoinDesired(S,G) is true if there is a downstream (S,G) interface I
  in the set:

  inherited_olist(S,G) =
  joins(S,G) (+) pim_include(S,G) (-) lost_assert(S,G)

  JoinDesired(S,G) may be affected by changes in the following:

  pim_ifp->primary_address
  pim_ifp->pim_dr_addr
  ch->ifassert_winner_metric
  ch->ifassert_winner
  ch->local_ifmembership 
  ch->ifjoin_state
  ch->upstream->rpf.source_nexthop.mrib_metric_preference
  ch->upstream->rpf.source_nexthop.mrib_route_metric
  ch->upstream->rpf.source_nexthop.interface

  See also pim_upstream_update_join_desired() below.
 */
int pim_upstream_evaluate_join_desired(struct pim_upstream *up)
{
  struct listnode      *ifnode;
  struct listnode      *ifnextnode;
  struct listnode      *chnode;
  struct listnode      *chnextnode;
  struct interface     *ifp;
  struct pim_interface *pim_ifp;
  struct pim_ifchannel *ch;

  /* scan all interfaces */
  for (ALL_LIST_ELEMENTS(iflist, ifnode, ifnextnode, ifp)) {
    pim_ifp = ifp->info;
    if (!pim_ifp)
      continue;

    /* scan per-interface (S,G) state */
    for (ALL_LIST_ELEMENTS(pim_ifp->pim_ifchannel_list, chnode, chnextnode, ch)) {
      if (ch->upstream != up)
	continue;

      if (pim_macro_ch_lost_assert(ch))
	continue; /* keep searching */

      if (pim_macro_chisin_joins_or_include(ch))
	return 1; /* true */
    } /* scan iface channel list */
  } /* scan iflist */

  return 0; /* false */
}

/*
  See also pim_upstream_evaluate_join_desired() above.
*/
void pim_upstream_update_join_desired(struct pim_upstream *up)
{
  int was_join_desired; /* boolean */
  int is_join_desired; /* boolean */

  was_join_desired = PIM_UPSTREAM_FLAG_TEST_DR_JOIN_DESIRED(up->flags);

  is_join_desired = pim_upstream_evaluate_join_desired(up);
  if (is_join_desired)
    PIM_UPSTREAM_FLAG_SET_DR_JOIN_DESIRED(up->flags);
  else
    PIM_UPSTREAM_FLAG_UNSET_DR_JOIN_DESIRED(up->flags);

  /* switched from false to true */
  if (is_join_desired && !was_join_desired) {
    zassert(up->join_state == PIM_UPSTREAM_NOTJOINED);
    pim_upstream_switch(up, PIM_UPSTREAM_JOINED);
    return;
  }
      
  /* switched from true to false */
  if (!is_join_desired && was_join_desired) {
    zassert(up->join_state == PIM_UPSTREAM_JOINED);
    pim_upstream_switch(up, PIM_UPSTREAM_NOTJOINED);
    return;
  }
}

/*
  RFC 4601 4.5.7. Sending (S,G) Join/Prune Messages
  Transitions from Joined State
  RPF'(S,G) GenID changes

  The upstream (S,G) state machine remains in Joined state.  If the
  Join Timer is set to expire in more than t_override seconds, reset
  it so that it expires after t_override seconds.
*/
void pim_upstream_rpf_genid_changed(struct in_addr neigh_addr)
{
  struct listnode     *up_node;
  struct listnode     *up_nextnode;
  struct pim_upstream *up;

  /*
    Scan all (S,G) upstreams searching for RPF'(S,G)=neigh_addr
  */
  for (ALL_LIST_ELEMENTS(qpim_upstream_list, up_node, up_nextnode, up)) {

    if (PIM_DEBUG_PIM_TRACE) {
      char neigh_str[100];
      char src_str[100];
      char grp_str[100];
      char rpf_addr_str[100];
      pim_inet4_dump("<neigh?>", neigh_addr, neigh_str, sizeof(neigh_str));
      pim_inet4_dump("<src?>", up->source_addr, src_str, sizeof(src_str));
      pim_inet4_dump("<grp?>", up->group_addr, grp_str, sizeof(grp_str));
      pim_inet4_dump("<rpf?>", up->rpf.rpf_addr, rpf_addr_str, sizeof(rpf_addr_str));
      zlog_debug("%s: matching neigh=%s against upstream (S,G)=(%s,%s) joined=%d rpf_addr=%s",
		 __PRETTY_FUNCTION__,
		 neigh_str, src_str, grp_str,
		 up->join_state == PIM_UPSTREAM_JOINED,
		 rpf_addr_str);
    }

    /* consider only (S,G) upstream in Joined state */
    if (up->join_state != PIM_UPSTREAM_JOINED)
      continue;

    /* match RPF'(S,G)=neigh_addr */
    if (up->rpf.rpf_addr.s_addr != neigh_addr.s_addr)
      continue;

    pim_upstream_join_timer_decrease_to_t_override("RPF'(S,G) GenID change",
						   up, neigh_addr);
  }
}


void pim_upstream_rpf_interface_changed(struct pim_upstream *up,
					struct interface *old_rpf_ifp)
{
  struct listnode  *ifnode;
  struct listnode  *ifnextnode;
  struct interface *ifp;

  /* scan all interfaces */
  for (ALL_LIST_ELEMENTS(iflist, ifnode, ifnextnode, ifp)) {
    struct listnode      *chnode;
    struct listnode      *chnextnode;
    struct pim_ifchannel *ch;
    struct pim_interface *pim_ifp;

    pim_ifp = ifp->info;
    if (!pim_ifp)
      continue;

    /* search all ifchannels */
    for (ALL_LIST_ELEMENTS(pim_ifp->pim_ifchannel_list, chnode, chnextnode, ch)) {
      if (ch->upstream != up)
	continue;

      if (ch->ifassert_state == PIM_IFASSERT_I_AM_LOSER) {
	if (
	    /* RPF_interface(S) was NOT I */
	    (old_rpf_ifp == ch->interface)
	    &&
	    /* RPF_interface(S) stopped being I */
	    (ch->upstream->rpf.source_nexthop.interface != ch->interface)
	    ) {
	  assert_action_a5(ch);
	}
      } /* PIM_IFASSERT_I_AM_LOSER */

      pim_ifchannel_update_assert_tracking_desired(ch);
    }
  }
}

void pim_upstream_update_could_assert(struct pim_upstream *up)
{
  struct listnode      *ifnode;
  struct listnode      *ifnextnode;
  struct listnode      *chnode;
  struct listnode      *chnextnode;
  struct interface     *ifp;
  struct pim_interface *pim_ifp;
  struct pim_ifchannel *ch;

  /* scan all interfaces */
  for (ALL_LIST_ELEMENTS(iflist, ifnode, ifnextnode, ifp)) {
    pim_ifp = ifp->info;
    if (!pim_ifp)
      continue;

    /* scan per-interface (S,G) state */
    for (ALL_LIST_ELEMENTS(pim_ifp->pim_ifchannel_list, chnode, chnextnode, ch)) {

      if (ch->upstream != up)
	continue;

      pim_ifchannel_update_could_assert(ch);

    } /* scan iface channel list */
  } /* scan iflist */
}

void pim_upstream_update_my_assert_metric(struct pim_upstream *up)
{
  struct listnode      *ifnode;
  struct listnode      *ifnextnode;
  struct listnode      *chnode;
  struct listnode      *chnextnode;
  struct interface     *ifp;
  struct pim_interface *pim_ifp;
  struct pim_ifchannel *ch;

  /* scan all interfaces */
  for (ALL_LIST_ELEMENTS(iflist, ifnode, ifnextnode, ifp)) {
    pim_ifp = ifp->info;
    if (!pim_ifp)
      continue;

    /* scan per-interface (S,G) state */
    for (ALL_LIST_ELEMENTS(pim_ifp->pim_ifchannel_list, chnode, chnextnode, ch)) {

      if (ch->upstream != up)
	continue;

      pim_ifchannel_update_my_assert_metric(ch);

    } /* scan iface channel list */
  } /* scan iflist */
}

static void pim_upstream_update_assert_tracking_desired(struct pim_upstream *up)
{
  struct listnode      *ifnode;
  struct listnode      *ifnextnode;
  struct listnode      *chnode;
  struct listnode      *chnextnode;
  struct interface     *ifp;
  struct pim_interface *pim_ifp;
  struct pim_ifchannel *ch;

  /* scan all interfaces */
  for (ALL_LIST_ELEMENTS(iflist, ifnode, ifnextnode, ifp)) {
    pim_ifp = ifp->info;
    if (!pim_ifp)
      continue;

    /* scan per-interface (S,G) state */
    for (ALL_LIST_ELEMENTS(pim_ifp->pim_ifchannel_list, chnode, chnextnode, ch)) {

      if (ch->upstream != up)
	continue;

      pim_ifchannel_update_assert_tracking_desired(ch);

    } /* scan iface channel list */
  } /* scan iflist */
}
