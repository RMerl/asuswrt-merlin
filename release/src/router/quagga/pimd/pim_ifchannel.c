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

#include "linklist.h"
#include "thread.h"
#include "memory.h"

#include "pimd.h"
#include "pim_str.h"
#include "pim_iface.h"
#include "pim_ifchannel.h"
#include "pim_zebra.h"
#include "pim_time.h"
#include "pim_msg.h"
#include "pim_pim.h"
#include "pim_join.h"
#include "pim_rpf.h"
#include "pim_macro.h"

void pim_ifchannel_free(struct pim_ifchannel *ch)
{
  zassert(!ch->t_ifjoin_expiry_timer);
  zassert(!ch->t_ifjoin_prune_pending_timer);
  zassert(!ch->t_ifassert_timer);

  XFREE(MTYPE_PIM_IFCHANNEL, ch);
}

void pim_ifchannel_delete(struct pim_ifchannel *ch)
{
  struct pim_interface *pim_ifp;

  pim_ifp = ch->interface->info;
  zassert(pim_ifp);

  if (ch->ifjoin_state != PIM_IFJOIN_NOINFO) {
    pim_upstream_update_join_desired(ch->upstream);
  }

  pim_upstream_del(ch->upstream);

  THREAD_OFF(ch->t_ifjoin_expiry_timer);
  THREAD_OFF(ch->t_ifjoin_prune_pending_timer);
  THREAD_OFF(ch->t_ifassert_timer);

  /*
    notice that listnode_delete() can't be moved
    into pim_ifchannel_free() because the later is
    called by list_delete_all_node()
  */
  listnode_delete(pim_ifp->pim_ifchannel_list, ch);

  pim_ifchannel_free(ch);
}

#define IFCHANNEL_NOINFO(ch)					\
  (								\
   ((ch)->local_ifmembership == PIM_IFMEMBERSHIP_NOINFO)	\
   &&								\
   ((ch)->ifjoin_state == PIM_IFJOIN_NOINFO)			\
   &&								\
   ((ch)->ifassert_state == PIM_IFASSERT_NOINFO)		\
   )
   
static void delete_on_noinfo(struct pim_ifchannel *ch)
{
  if (IFCHANNEL_NOINFO(ch)) {

    /* In NOINFO state, timers should have been cleared */
    zassert(!ch->t_ifjoin_expiry_timer);
    zassert(!ch->t_ifjoin_prune_pending_timer);
    zassert(!ch->t_ifassert_timer);
    
    pim_ifchannel_delete(ch);
  }
}

void pim_ifchannel_ifjoin_switch(const char *caller,
				 struct pim_ifchannel *ch,
				 enum pim_ifjoin_state new_state)
{
  enum pim_ifjoin_state old_state = ch->ifjoin_state;

  if (old_state == new_state) {
    zlog_debug("%s calledby %s: non-transition on state %d (%s)",
	       __PRETTY_FUNCTION__, caller, new_state,
	       pim_ifchannel_ifjoin_name(new_state));
    return;
  }

  zassert(old_state != new_state);

  ch->ifjoin_state = new_state;

  /* Transition to/from NOINFO ? */
  if (
      (old_state == PIM_IFJOIN_NOINFO)
      ||
      (new_state == PIM_IFJOIN_NOINFO)
      ) {

    if (PIM_DEBUG_PIM_EVENTS) {
      char src_str[100];
      char grp_str[100];
      pim_inet4_dump("<src?>", ch->source_addr, src_str, sizeof(src_str));
      pim_inet4_dump("<grp?>", ch->group_addr, grp_str, sizeof(grp_str));
      zlog_debug("PIM_IFCHANNEL_%s: (S,G)=(%s,%s) on interface %s",
		 ((new_state == PIM_IFJOIN_NOINFO) ? "DOWN" : "UP"),
		 src_str, grp_str, ch->interface->name);
    }

    /*
      Record uptime of state transition to/from NOINFO
    */
    ch->ifjoin_creation = pim_time_monotonic_sec();

    pim_upstream_update_join_desired(ch->upstream);
    pim_ifchannel_update_could_assert(ch);
    pim_ifchannel_update_assert_tracking_desired(ch);
  }
}

const char *pim_ifchannel_ifjoin_name(enum pim_ifjoin_state ifjoin_state)
{
  switch (ifjoin_state) {
  case PIM_IFJOIN_NOINFO:        return "NOINFO";
  case PIM_IFJOIN_JOIN:          return "JOIN";
  case PIM_IFJOIN_PRUNE_PENDING: return "PRUNEP";
  }

  return "ifjoin_bad_state";
}

const char *pim_ifchannel_ifassert_name(enum pim_ifassert_state ifassert_state)
{
  switch (ifassert_state) {
  case PIM_IFASSERT_NOINFO:      return "NOINFO";
  case PIM_IFASSERT_I_AM_WINNER: return "WINNER";
  case PIM_IFASSERT_I_AM_LOSER:  return "LOSER";
  }

  return "ifassert_bad_state";
}

/*
  RFC 4601: 4.6.5.  Assert State Macros

  AssertWinner(S,G,I) defaults to NULL and AssertWinnerMetric(S,G,I)
  defaults to Infinity when in the NoInfo state.
*/
void reset_ifassert_state(struct pim_ifchannel *ch)
{
  THREAD_OFF(ch->t_ifassert_timer);

  pim_ifassert_winner_set(ch,
			  PIM_IFASSERT_NOINFO,
			  qpim_inaddr_any,
			  qpim_infinite_assert_metric);
}

static struct pim_ifchannel *pim_ifchannel_new(struct interface *ifp,
					       struct in_addr source_addr,
					       struct in_addr group_addr)
{
  struct pim_ifchannel *ch;
  struct pim_interface *pim_ifp;
  struct pim_upstream  *up;

  pim_ifp = ifp->info;
  zassert(pim_ifp);

  up = pim_upstream_add(source_addr, group_addr);
  if (!up) {
    char src_str[100];
    char grp_str[100];
    pim_inet4_dump("<src?>", source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", group_addr, grp_str, sizeof(grp_str));
    zlog_err("%s: could not attach upstream (S,G)=(%s,%s) on interface %s",
	     __PRETTY_FUNCTION__,
	     src_str, grp_str, ifp->name);
    return 0;
  }

  ch = XMALLOC(MTYPE_PIM_IFCHANNEL, sizeof(*ch));
  if (!ch) {
    zlog_err("%s: PIM XMALLOC(%zu) failure",
	     __PRETTY_FUNCTION__, sizeof(*ch));
    return 0;
  }

  ch->flags                        = 0;
  ch->upstream                     = up;
  ch->interface                    = ifp;
  ch->source_addr                  = source_addr;
  ch->group_addr                   = group_addr;
  ch->local_ifmembership           = PIM_IFMEMBERSHIP_NOINFO;

  ch->ifjoin_state                 = PIM_IFJOIN_NOINFO;
  ch->t_ifjoin_expiry_timer        = 0;
  ch->t_ifjoin_prune_pending_timer = 0;
  ch->ifjoin_creation              = 0;

  /* Assert state */
  ch->t_ifassert_timer   = 0;
  reset_ifassert_state(ch);
  if (pim_macro_ch_could_assert_eval(ch))
    PIM_IF_FLAG_SET_COULD_ASSERT(ch->flags);
  else
    PIM_IF_FLAG_UNSET_COULD_ASSERT(ch->flags);

  if (pim_macro_assert_tracking_desired_eval(ch))
    PIM_IF_FLAG_SET_ASSERT_TRACKING_DESIRED(ch->flags);
  else
    PIM_IF_FLAG_UNSET_ASSERT_TRACKING_DESIRED(ch->flags);

  ch->ifassert_my_metric = pim_macro_ch_my_assert_metric_eval(ch);

  /* Attach to list */
  listnode_add(pim_ifp->pim_ifchannel_list, ch);

  zassert(IFCHANNEL_NOINFO(ch));

  return ch;
}

struct pim_ifchannel *pim_ifchannel_find(struct interface *ifp,
					 struct in_addr source_addr,
					 struct in_addr group_addr)
{
  struct pim_interface *pim_ifp;
  struct listnode      *ch_node;
  struct pim_ifchannel *ch;

  zassert(ifp);

  pim_ifp = ifp->info;

  if (!pim_ifp) {
    char src_str[100];
    char grp_str[100];
    pim_inet4_dump("<src?>", source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", group_addr, grp_str, sizeof(grp_str));
    zlog_warn("%s: (S,G)=(%s,%s): multicast not enabled on interface %s",
	      __PRETTY_FUNCTION__,
	      src_str, grp_str,
	      ifp->name);
    return 0;
  }

  for (ALL_LIST_ELEMENTS_RO(pim_ifp->pim_ifchannel_list, ch_node, ch)) {
    if (
	(source_addr.s_addr == ch->source_addr.s_addr) &&
	(group_addr.s_addr == ch->group_addr.s_addr)
	) {
      return ch;
    }
  }

  return 0;
}

static void ifmembership_set(struct pim_ifchannel *ch,
			     enum pim_ifmembership membership)
{
  if (ch->local_ifmembership == membership)
    return;

  /* if (PIM_DEBUG_PIM_EVENTS) */ {
    char src_str[100];
    char grp_str[100];
    pim_inet4_dump("<src?>", ch->source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", ch->group_addr, grp_str, sizeof(grp_str));
    zlog_debug("%s: (S,G)=(%s,%s) membership now is %s on interface %s",
	       __PRETTY_FUNCTION__,
	       src_str, grp_str,
	       membership == PIM_IFMEMBERSHIP_INCLUDE ? "INCLUDE" : "NOINFO",
	       ch->interface->name);
  }
  
  ch->local_ifmembership = membership;

  pim_upstream_update_join_desired(ch->upstream);
  pim_ifchannel_update_could_assert(ch);
  pim_ifchannel_update_assert_tracking_desired(ch);
}


void pim_ifchannel_membership_clear(struct interface *ifp)
{
  struct pim_interface *pim_ifp;
  struct listnode      *ch_node;
  struct pim_ifchannel *ch;

  pim_ifp = ifp->info;
  zassert(pim_ifp);

  for (ALL_LIST_ELEMENTS_RO(pim_ifp->pim_ifchannel_list, ch_node, ch)) {
    ifmembership_set(ch, PIM_IFMEMBERSHIP_NOINFO);
  }
}

void pim_ifchannel_delete_on_noinfo(struct interface *ifp)
{
  struct pim_interface *pim_ifp;
  struct listnode      *node;
  struct listnode      *next_node;
  struct pim_ifchannel *ch;

  pim_ifp = ifp->info;
  zassert(pim_ifp);

  for (ALL_LIST_ELEMENTS(pim_ifp->pim_ifchannel_list, node, next_node, ch)) {
    delete_on_noinfo(ch);
  }
}

struct pim_ifchannel *pim_ifchannel_add(struct interface *ifp,
					struct in_addr source_addr,
					struct in_addr group_addr)
{
  struct pim_ifchannel *ch;
  char src_str[100];
  char grp_str[100];

  ch = pim_ifchannel_find(ifp, source_addr, group_addr);
  if (ch)
    return ch;

  ch = pim_ifchannel_new(ifp, source_addr, group_addr);
  if (ch)
    return ch;
    
  pim_inet4_dump("<src?>", source_addr, src_str, sizeof(src_str));
  pim_inet4_dump("<grp?>", group_addr, grp_str, sizeof(grp_str));
  zlog_warn("%s: pim_ifchannel_new() failure for (S,G)=(%s,%s) on interface %s",
	    __PRETTY_FUNCTION__,
	    src_str, grp_str, ifp->name);

  return 0;
}

static void ifjoin_to_noinfo(struct pim_ifchannel *ch)
{
  pim_forward_stop(ch);
  pim_ifchannel_ifjoin_switch(__PRETTY_FUNCTION__, ch, PIM_IFJOIN_NOINFO);
  delete_on_noinfo(ch);
}

static int on_ifjoin_expiry_timer(struct thread *t)
{
  struct pim_ifchannel *ch;

  zassert(t);
  ch = THREAD_ARG(t);
  zassert(ch);

  ch->t_ifjoin_expiry_timer = 0;

  zassert(ch->ifjoin_state == PIM_IFJOIN_JOIN);

  ifjoin_to_noinfo(ch);
  /* ch may have been deleted */

  return 0;
}

static void prune_echo(struct interface *ifp,
		       struct in_addr source_addr,
		       struct in_addr group_addr)
{
  struct pim_interface *pim_ifp;
  struct in_addr neigh_dst_addr;

  pim_ifp = ifp->info;
  zassert(pim_ifp);

  neigh_dst_addr = pim_ifp->primary_address;

  if (PIM_DEBUG_PIM_EVENTS) {
    char source_str[100];
    char group_str[100];
    char neigh_dst_str[100];
    pim_inet4_dump("<src?>", source_addr, source_str, sizeof(source_str));
    pim_inet4_dump("<grp?>", group_addr, group_str, sizeof(group_str));
    pim_inet4_dump("<neigh?>", neigh_dst_addr, neigh_dst_str, sizeof(neigh_dst_str));
    zlog_debug("%s: sending PruneEcho(S,G)=(%s,%s) to upstream=%s on interface %s",
	       __PRETTY_FUNCTION__, source_str, group_str, neigh_dst_str, ifp->name);
  }

  pim_joinprune_send(ifp, neigh_dst_addr, source_addr, group_addr,
		     0 /* boolean: send_join=false (prune) */);
}

static int on_ifjoin_prune_pending_timer(struct thread *t)
{
  struct pim_ifchannel *ch;
  int send_prune_echo; /* boolean */
  struct interface *ifp;
  struct pim_interface *pim_ifp;
  struct in_addr ch_source;
  struct in_addr ch_group;

  zassert(t);
  ch = THREAD_ARG(t);
  zassert(ch);

  ch->t_ifjoin_prune_pending_timer = 0;

  zassert(ch->ifjoin_state == PIM_IFJOIN_PRUNE_PENDING);

  /* Send PruneEcho(S,G) ? */
  ifp = ch->interface;
  pim_ifp = ifp->info;
  send_prune_echo = (listcount(pim_ifp->pim_neighbor_list) > 1);

  /* Save (S,G) */
  ch_source = ch->source_addr;
  ch_group = ch->group_addr;

  ifjoin_to_noinfo(ch);
  /* from here ch may have been deleted */

  if (send_prune_echo)
    prune_echo(ifp, ch_source, ch_group);

  return 0;
}

static void check_recv_upstream(int is_join,
				struct interface *recv_ifp,
				struct in_addr upstream,
				struct in_addr source_addr,
				struct in_addr group_addr,
				uint8_t source_flags,
				int holdtime)
{
  struct pim_upstream *up;

  /* Upstream (S,G) in Joined state ? */
  up = pim_upstream_find(source_addr, group_addr);
  if (!up)
    return;
  if (up->join_state != PIM_UPSTREAM_JOINED)
    return;

  /* Upstream (S,G) in Joined state */

  if (PIM_INADDR_IS_ANY(up->rpf.rpf_addr)) {
    /* RPF'(S,G) not found */
    char src_str[100];
    char grp_str[100];
    pim_inet4_dump("<src?>", source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", group_addr, grp_str, sizeof(grp_str));
    zlog_warn("%s %s: RPF'(%s,%s) not found",
	      __FILE__, __PRETTY_FUNCTION__, 
	      src_str, grp_str);
    return;
  }

  /* upstream directed to RPF'(S,G) ? */
  if (upstream.s_addr != up->rpf.rpf_addr.s_addr) {
    char src_str[100];
    char grp_str[100];
    char up_str[100];
    char rpf_str[100];
    pim_inet4_dump("<src?>", source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", group_addr, grp_str, sizeof(grp_str));
    pim_inet4_dump("<up?>", upstream, up_str, sizeof(up_str));
    pim_inet4_dump("<rpf?>", up->rpf.rpf_addr, rpf_str, sizeof(rpf_str));
    zlog_warn("%s %s: (S,G)=(%s,%s) upstream=%s not directed to RPF'(S,G)=%s on interface %s",
	      __FILE__, __PRETTY_FUNCTION__, 
	      src_str, grp_str,
	      up_str, rpf_str, recv_ifp->name);
    return;
  }
  /* upstream directed to RPF'(S,G) */

  if (is_join) {
    /* Join(S,G) to RPF'(S,G) */
    pim_upstream_join_suppress(up, up->rpf.rpf_addr, holdtime);
    return;
  }

  /* Prune to RPF'(S,G) */

  if (source_flags & PIM_RPT_BIT_MASK) {
    if (source_flags & PIM_WILDCARD_BIT_MASK) {
      /* Prune(*,G) to RPF'(S,G) */
      pim_upstream_join_timer_decrease_to_t_override("Prune(*,G)",
						     up, up->rpf.rpf_addr);
      return;
    }

    /* Prune(S,G,rpt) to RPF'(S,G) */
    pim_upstream_join_timer_decrease_to_t_override("Prune(S,G,rpt)",
						   up, up->rpf.rpf_addr);
    return;
  }

  /* Prune(S,G) to RPF'(S,G) */
  pim_upstream_join_timer_decrease_to_t_override("Prune(S,G)", up,
						 up->rpf.rpf_addr);
}

static int nonlocal_upstream(int is_join,
			     struct interface *recv_ifp,
			     struct in_addr upstream,
			     struct in_addr source_addr,
			     struct in_addr group_addr,
			     uint8_t source_flags,
			     uint16_t holdtime)
{
  struct pim_interface *recv_pim_ifp;
  int is_local; /* boolean */

  recv_pim_ifp = recv_ifp->info;
  zassert(recv_pim_ifp);

  is_local = (upstream.s_addr == recv_pim_ifp->primary_address.s_addr);
  
  if (PIM_DEBUG_PIM_TRACE) {
    char up_str[100];
    char src_str[100];
    char grp_str[100];
    pim_inet4_dump("<upstream?>", upstream, up_str, sizeof(up_str));
    pim_inet4_dump("<src?>", source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", group_addr, grp_str, sizeof(grp_str));
    zlog_warn("%s: recv %s (S,G)=(%s,%s) to %s upstream=%s on %s",
	      __PRETTY_FUNCTION__,
	      is_join ? "join" : "prune",
	      src_str, grp_str,
	      is_local ? "local" : "non-local",
	      up_str, recv_ifp->name);
  }

  if (is_local)
    return 0;

  /*
    Since recv upstream addr was not directed to our primary
    address, check if we should react to it in any way.
  */
  check_recv_upstream(is_join, recv_ifp, upstream, source_addr, group_addr,
		      source_flags, holdtime);

  return 1; /* non-local */
}

void pim_ifchannel_join_add(struct interface *ifp,
			    struct in_addr neigh_addr,
			    struct in_addr upstream,
			    struct in_addr source_addr,
			    struct in_addr group_addr,
			    uint8_t source_flags,
			    uint16_t holdtime)
{
  struct pim_interface *pim_ifp;
  struct pim_ifchannel *ch;

  if (nonlocal_upstream(1 /* join */, ifp, upstream,
			source_addr, group_addr, source_flags, holdtime)) {
    return;
  }

  ch = pim_ifchannel_add(ifp, source_addr, group_addr);
  if (!ch)
    return;

  /*
    RFC 4601: 4.6.1.  (S,G) Assert Message State Machine

    Transitions from "I am Assert Loser" State

    Receive Join(S,G) on Interface I

    We receive a Join(S,G) that has the Upstream Neighbor Address
    field set to my primary IP address on interface I.  The action is
    to transition to NoInfo state, delete this (S,G) assert state
    (Actions A5 below), and allow the normal PIM Join/Prune mechanisms
    to operate.

    Notice: The nonlocal_upstream() test above ensures the upstream
    address of the join message is our primary address.
   */
  if (ch->ifassert_state == PIM_IFASSERT_I_AM_LOSER) {
    char src_str[100];
    char grp_str[100];
    char neigh_str[100];
    pim_inet4_dump("<src?>", source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", group_addr, grp_str, sizeof(grp_str));
    pim_inet4_dump("<neigh?>", neigh_addr, neigh_str, sizeof(neigh_str));
    zlog_warn("%s: Assert Loser recv Join(%s,%s) from %s on %s",
	      __PRETTY_FUNCTION__,
	      src_str, grp_str, neigh_str, ifp->name);

    assert_action_a5(ch);
  }

  pim_ifp = ifp->info;
  zassert(pim_ifp);

  switch (ch->ifjoin_state) {
  case PIM_IFJOIN_NOINFO:
    pim_ifchannel_ifjoin_switch(__PRETTY_FUNCTION__, ch, PIM_IFJOIN_JOIN);
    if (pim_macro_chisin_oiflist(ch)) {
      pim_forward_start(ch);
    }
    break;
  case PIM_IFJOIN_JOIN:
    zassert(!ch->t_ifjoin_prune_pending_timer);

    /*
      In the JOIN state ch->t_ifjoin_expiry_timer may be NULL due to a
      previously received join message with holdtime=0xFFFF.
     */
    if (ch->t_ifjoin_expiry_timer) {
      unsigned long remain =
	thread_timer_remain_second(ch->t_ifjoin_expiry_timer);
      if (remain > holdtime) {
	/*
	  RFC 4601: 4.5.3.  Receiving (S,G) Join/Prune Messages

	  Transitions from Join State

          The (S,G) downstream state machine on interface I remains in
          Join state, and the Expiry Timer (ET) is restarted, set to
          maximum of its current value and the HoldTime from the
          triggering Join/Prune message.

	  Conclusion: Do not change the ET if the current value is
	  higher than the received join holdtime.
	 */
	return;
      }
    }
    THREAD_OFF(ch->t_ifjoin_expiry_timer);
    break;
  case PIM_IFJOIN_PRUNE_PENDING:
    zassert(!ch->t_ifjoin_expiry_timer);
    zassert(ch->t_ifjoin_prune_pending_timer);
    THREAD_OFF(ch->t_ifjoin_prune_pending_timer);
    pim_ifchannel_ifjoin_switch(__PRETTY_FUNCTION__, ch, PIM_IFJOIN_JOIN);
    break;
  }

  zassert(!IFCHANNEL_NOINFO(ch));

  if (holdtime != 0xFFFF) {
    THREAD_TIMER_ON(master, ch->t_ifjoin_expiry_timer,
		    on_ifjoin_expiry_timer,
		    ch, holdtime);
  }
}

void pim_ifchannel_prune(struct interface *ifp,
			 struct in_addr upstream,
			 struct in_addr source_addr,
			 struct in_addr group_addr,
			 uint8_t source_flags,
			 uint16_t holdtime)
{
  struct pim_ifchannel *ch;
  int jp_override_interval_msec;

  if (nonlocal_upstream(0 /* prune */, ifp, upstream,
			source_addr, group_addr, source_flags, holdtime)) {
    return;
  }

  ch = pim_ifchannel_add(ifp, source_addr, group_addr);
  if (!ch)
    return;

  switch (ch->ifjoin_state) {
  case PIM_IFJOIN_NOINFO:
  case PIM_IFJOIN_PRUNE_PENDING:
    /* nothing to do */
    break;
  case PIM_IFJOIN_JOIN:
    {
      struct pim_interface *pim_ifp;

      pim_ifp = ifp->info;

      zassert(ch->t_ifjoin_expiry_timer);
      zassert(!ch->t_ifjoin_prune_pending_timer);

      THREAD_OFF(ch->t_ifjoin_expiry_timer);
      
      pim_ifchannel_ifjoin_switch(__PRETTY_FUNCTION__, ch, PIM_IFJOIN_PRUNE_PENDING);
      
      if (listcount(pim_ifp->pim_neighbor_list) > 1) {
	jp_override_interval_msec = pim_if_jp_override_interval_msec(ifp);
      }
      else {
	jp_override_interval_msec = 0; /* schedule to expire immediately */
	/* If we called ifjoin_prune() directly instead, care should
	   be taken not to use "ch" afterwards since it would be
	   deleted. */
      }
      
      THREAD_TIMER_MSEC_ON(master, ch->t_ifjoin_prune_pending_timer,
			   on_ifjoin_prune_pending_timer,
			   ch, jp_override_interval_msec);
      
      zassert(!ch->t_ifjoin_expiry_timer);
      zassert(ch->t_ifjoin_prune_pending_timer);
    }
    break;
  }

}

void pim_ifchannel_local_membership_add(struct interface *ifp,
					struct in_addr source_addr,
					struct in_addr group_addr)
{
  struct pim_ifchannel *ch;
  struct pim_interface *pim_ifp;

  /* PIM enabled on interface? */
  pim_ifp = ifp->info;
  if (!pim_ifp)
    return;
  if (!PIM_IF_TEST_PIM(pim_ifp->options))
    return;

  ch = pim_ifchannel_add(ifp, source_addr, group_addr);
  if (!ch) {
    return;
  }

  ifmembership_set(ch, PIM_IFMEMBERSHIP_INCLUDE);

  zassert(!IFCHANNEL_NOINFO(ch));
}

void pim_ifchannel_local_membership_del(struct interface *ifp,
					struct in_addr source_addr,
					struct in_addr group_addr)
{
  struct pim_ifchannel *ch;
  struct pim_interface *pim_ifp;

  /* PIM enabled on interface? */
  pim_ifp = ifp->info;
  if (!pim_ifp)
    return;
  if (!PIM_IF_TEST_PIM(pim_ifp->options))
    return;

  ch = pim_ifchannel_find(ifp, source_addr, group_addr);
  if (!ch)
    return;

  ifmembership_set(ch, PIM_IFMEMBERSHIP_NOINFO);

  delete_on_noinfo(ch);
}

void pim_ifchannel_update_could_assert(struct pim_ifchannel *ch)
{
  int old_couldassert = PIM_FORCE_BOOLEAN(PIM_IF_FLAG_TEST_COULD_ASSERT(ch->flags));
  int new_couldassert = PIM_FORCE_BOOLEAN(pim_macro_ch_could_assert_eval(ch));

  if (new_couldassert == old_couldassert)
    return;

  if (PIM_DEBUG_PIM_EVENTS) {
    char src_str[100];
    char grp_str[100];
    pim_inet4_dump("<src?>", ch->source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", ch->group_addr, grp_str, sizeof(grp_str));
    zlog_debug("%s: CouldAssert(%s,%s,%s) changed from %d to %d",
	       __PRETTY_FUNCTION__,
	       src_str, grp_str, ch->interface->name,
	       old_couldassert, new_couldassert);
  }

  if (new_couldassert) {
    /* CouldAssert(S,G,I) switched from FALSE to TRUE */
    PIM_IF_FLAG_SET_COULD_ASSERT(ch->flags);
  }
  else {
    /* CouldAssert(S,G,I) switched from TRUE to FALSE */
    PIM_IF_FLAG_UNSET_COULD_ASSERT(ch->flags);

    if (ch->ifassert_state == PIM_IFASSERT_I_AM_WINNER) {
      assert_action_a4(ch);
    }
  }

  pim_ifchannel_update_my_assert_metric(ch);
}

/*
  my_assert_metric may be affected by:

  CouldAssert(S,G)
  pim_ifp->primary_address
  rpf->source_nexthop.mrib_metric_preference;
  rpf->source_nexthop.mrib_route_metric;
 */
void pim_ifchannel_update_my_assert_metric(struct pim_ifchannel *ch)
{
  struct pim_assert_metric my_metric_new = pim_macro_ch_my_assert_metric_eval(ch);

  if (pim_assert_metric_match(&my_metric_new, &ch->ifassert_my_metric))
      return;

  if (PIM_DEBUG_PIM_EVENTS) {
    char src_str[100];
    char grp_str[100];
    char old_addr_str[100];
    char new_addr_str[100];
    pim_inet4_dump("<src?>", ch->source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", ch->group_addr, grp_str, sizeof(grp_str));
    pim_inet4_dump("<old_addr?>", ch->ifassert_my_metric.ip_address, old_addr_str, sizeof(old_addr_str));
    pim_inet4_dump("<new_addr?>", my_metric_new.ip_address, new_addr_str, sizeof(new_addr_str));
    zlog_debug("%s: my_assert_metric(%s,%s,%s) changed from %u,%u,%u,%s to %u,%u,%u,%s",
	       __PRETTY_FUNCTION__,
	       src_str, grp_str, ch->interface->name,
	       ch->ifassert_my_metric.rpt_bit_flag,
	       ch->ifassert_my_metric.metric_preference,
	       ch->ifassert_my_metric.route_metric,
	       old_addr_str,
	       my_metric_new.rpt_bit_flag,
	       my_metric_new.metric_preference,
	       my_metric_new.route_metric,
	       new_addr_str);
  }

  ch->ifassert_my_metric = my_metric_new;

  if (pim_assert_metric_better(&ch->ifassert_my_metric,
			       &ch->ifassert_winner_metric)) {
    assert_action_a5(ch);
  }
}

void pim_ifchannel_update_assert_tracking_desired(struct pim_ifchannel *ch)
{
  int old_atd = PIM_FORCE_BOOLEAN(PIM_IF_FLAG_TEST_ASSERT_TRACKING_DESIRED(ch->flags));
  int new_atd = PIM_FORCE_BOOLEAN(pim_macro_assert_tracking_desired_eval(ch));

  if (new_atd == old_atd)
    return;

  if (PIM_DEBUG_PIM_EVENTS) {
    char src_str[100];
    char grp_str[100];
    pim_inet4_dump("<src?>", ch->source_addr, src_str, sizeof(src_str));
    pim_inet4_dump("<grp?>", ch->group_addr, grp_str, sizeof(grp_str));
    zlog_debug("%s: AssertTrackingDesired(%s,%s,%s) changed from %d to %d",
	       __PRETTY_FUNCTION__,
	       src_str, grp_str, ch->interface->name,
	       old_atd, new_atd);
  }

  if (new_atd) {
    /* AssertTrackingDesired(S,G,I) switched from FALSE to TRUE */
    PIM_IF_FLAG_SET_ASSERT_TRACKING_DESIRED(ch->flags);
  }
  else {
    /* AssertTrackingDesired(S,G,I) switched from TRUE to FALSE */
    PIM_IF_FLAG_UNSET_ASSERT_TRACKING_DESIRED(ch->flags);

    if (ch->ifassert_state == PIM_IFASSERT_I_AM_LOSER) {
      assert_action_a5(ch);
    }
  }
}
