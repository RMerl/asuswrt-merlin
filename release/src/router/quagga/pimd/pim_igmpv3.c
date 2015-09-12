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
#include "memory.h"

#include "pimd.h"
#include "pim_iface.h"
#include "pim_igmp.h"
#include "pim_igmpv3.h"
#include "pim_str.h"
#include "pim_util.h"
#include "pim_time.h"
#include "pim_zebra.h"
#include "pim_oil.h"

static void group_retransmit_timer_on(struct igmp_group *group);
static long igmp_group_timer_remain_msec(struct igmp_group *group);
static long igmp_source_timer_remain_msec(struct igmp_source *source);
static void group_query_send(struct igmp_group *group);
static void source_query_send_by_flag(struct igmp_group *group,
				      int num_sources_tosend);

static void on_trace(const char *label,
		     struct interface *ifp, struct in_addr from,
		     struct in_addr group_addr,
		     int num_sources, struct in_addr *sources)
{
  if (PIM_DEBUG_IGMP_TRACE) {
    char from_str[100];
    char group_str[100];

    pim_inet4_dump("<from?>", from, from_str, sizeof(from_str));
    pim_inet4_dump("<group?>", group_addr, group_str, sizeof(group_str));

    zlog_debug("%s: from %s on %s: group=%s sources=%d",
	       label, from_str, ifp->name, group_str, num_sources);
  }
}

int igmp_group_compat_mode(const struct igmp_sock *igmp,
			   const struct igmp_group *group)
{
  struct pim_interface *pim_ifp;
  int64_t               now_dsec;
  long                  older_host_present_interval_dsec;

  zassert(igmp);
  zassert(igmp->interface);
  zassert(igmp->interface->info);

  pim_ifp = igmp->interface->info;

  /*
    RFC 3376: 8.13. Older Host Present Interval

    This value MUST be ((the Robustness Variable) times (the Query
    Interval)) plus (one Query Response Interval).

    older_host_present_interval_dsec = \
      igmp->querier_robustness_variable * \
      10 * igmp->querier_query_interval + \
      pim_ifp->query_max_response_time_dsec;
  */
  older_host_present_interval_dsec =
    PIM_IGMP_OHPI_DSEC(igmp->querier_robustness_variable,
		       igmp->querier_query_interval,
		       pim_ifp->igmp_query_max_response_time_dsec);

  now_dsec = pim_time_monotonic_dsec();
  if (now_dsec < 1) {
    /* broken timer logged by pim_time_monotonic_dsec() */
    return 3;
  }

  if ((now_dsec - group->last_igmp_v1_report_dsec) < older_host_present_interval_dsec)
    return 1; /* IGMPv1 */

  if ((now_dsec - group->last_igmp_v2_report_dsec) < older_host_present_interval_dsec)
    return 2; /* IGMPv2 */

  return 3; /* IGMPv3 */
}

void igmp_group_reset_gmi(struct igmp_group *group)
{
  long group_membership_interval_msec;
  struct pim_interface *pim_ifp;
  struct igmp_sock *igmp;
  struct interface *ifp;

  igmp = group->group_igmp_sock;
  ifp = igmp->interface;
  pim_ifp = ifp->info;

  /*
    RFC 3376: 8.4. Group Membership Interval

    The Group Membership Interval is the amount of time that must pass
    before a multicast router decides there are no more members of a
    group or a particular source on a network.

    This value MUST be ((the Robustness Variable) times (the Query
    Interval)) plus (one Query Response Interval).

    group_membership_interval_msec = querier_robustness_variable *
                                     (1000 * querier_query_interval) +
                                     100 * query_response_interval_dsec;
  */
  group_membership_interval_msec =
    PIM_IGMP_GMI_MSEC(igmp->querier_robustness_variable,
		      igmp->querier_query_interval,
		      pim_ifp->igmp_query_max_response_time_dsec);

  if (PIM_DEBUG_IGMP_TRACE) {
    char group_str[100];
    pim_inet4_dump("<group?>", group->group_addr, group_str, sizeof(group_str));
    zlog_debug("Resetting group %s timer to GMI=%ld.%03ld sec on %s",
	       group_str,
	       group_membership_interval_msec / 1000,
	       group_membership_interval_msec % 1000,
	       ifp->name);
  }

  /*
    RFC 3376: 6.2.2. Definition of Group Timers

    The group timer is only used when a group is in EXCLUDE mode and
    it represents the time for the *filter-mode* of the group to
    expire and switch to INCLUDE mode.
  */
  zassert(group->group_filtermode_isexcl);

  igmp_group_timer_on(group, group_membership_interval_msec, ifp->name);
}

static int igmp_source_timer(struct thread *t)
{
  struct igmp_source *source;
  struct igmp_group *group;

  zassert(t);
  source = THREAD_ARG(t);
  zassert(source);

  group = source->source_group;

  if (PIM_DEBUG_IGMP_TRACE) {
    char group_str[100];
    char source_str[100];
    pim_inet4_dump("<group?>", group->group_addr, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", source->source_addr, source_str, sizeof(source_str));
    zlog_debug("%s: Source timer expired for group %s source %s on %s",
	       __PRETTY_FUNCTION__,
	       group_str, source_str,
	       group->group_igmp_sock->interface->name);
  }

  zassert(source->t_source_timer);
  source->t_source_timer = 0;

  /*
    RFC 3376: 6.3. IGMPv3 Source-Specific Forwarding Rules

    Group
    Filter-Mode    Source Timer Value    Action
    -----------    ------------------    ------
    INCLUDE        TIMER == 0            Suggest to stop forwarding
                                         traffic from source and
                                         remove source record.  If
                                         there are no more source
                                         records for the group, delete
                                         group record.

    EXCLUDE        TIMER == 0            Suggest to not forward
                                         traffic from source
                                         (DO NOT remove record)

    Source timer switched from (T > 0) to (T == 0): disable forwarding.
   */

  zassert(!source->t_source_timer);

  if (group->group_filtermode_isexcl) {
    /* EXCLUDE mode */

    igmp_source_forward_stop(source);
  }
  else {
    /* INCLUDE mode */

    /* igmp_source_delete() will stop forwarding source */
    igmp_source_delete(source);

    /*
      If there are no more source records for the group, delete group
      record.
    */
    if (!listcount(group->group_source_list)) {
      igmp_group_delete_empty_include(group);
    }
  }

  return 0;
}

static void source_timer_off(struct igmp_group *group,
			     struct igmp_source *source)
{
  if (!source->t_source_timer)
    return;
  
  if (PIM_DEBUG_IGMP_TRACE) {
    char group_str[100];
    char source_str[100];
    pim_inet4_dump("<group?>", group->group_addr, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", source->source_addr, source_str, sizeof(source_str));
    zlog_debug("Cancelling TIMER event for group %s source %s on %s",
	       group_str, source_str,
	       group->group_igmp_sock->interface->name);
  }

  THREAD_OFF(source->t_source_timer);
  zassert(!source->t_source_timer);
}

static void igmp_source_timer_on(struct igmp_group *group,
				 struct igmp_source *source,
				 long interval_msec)
{
  source_timer_off(group, source);

  if (PIM_DEBUG_IGMP_EVENTS) {
    char group_str[100];
    char source_str[100];
    pim_inet4_dump("<group?>", group->group_addr, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", source->source_addr, source_str, sizeof(source_str));
    zlog_debug("Scheduling %ld.%03ld sec TIMER event for group %s source %s on %s",
	       interval_msec / 1000,
	       interval_msec % 1000,
	       group_str, source_str,
	       group->group_igmp_sock->interface->name);
  }

  THREAD_TIMER_MSEC_ON(master, source->t_source_timer,
		       igmp_source_timer,
		       source, interval_msec);
  zassert(source->t_source_timer);

  /*
    RFC 3376: 6.3. IGMPv3 Source-Specific Forwarding Rules
    
    Source timer switched from (T == 0) to (T > 0): enable forwarding.
  */
  igmp_source_forward_start(source);
}

void igmp_source_reset_gmi(struct igmp_sock *igmp,
			   struct igmp_group *group,
			   struct igmp_source *source)
{
  long group_membership_interval_msec;
  struct pim_interface *pim_ifp;
  struct interface *ifp;

  ifp = igmp->interface;
  pim_ifp = ifp->info;

  group_membership_interval_msec =
    PIM_IGMP_GMI_MSEC(igmp->querier_robustness_variable,
		      igmp->querier_query_interval,
		      pim_ifp->igmp_query_max_response_time_dsec);

  if (PIM_DEBUG_IGMP_TRACE) {
    char group_str[100];
    char source_str[100];

    pim_inet4_dump("<group?>", group->group_addr, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", source->source_addr, source_str, sizeof(source_str));

    zlog_debug("Resetting source %s timer to GMI=%ld.%03ld sec for group %s on %s",
	       source_str,
	       group_membership_interval_msec / 1000,
	       group_membership_interval_msec % 1000,
	       group_str,
	       ifp->name);
  }

  igmp_source_timer_on(group, source,
		       group_membership_interval_msec);
}

static void source_mark_delete_flag(struct list *source_list)
{
  struct listnode    *src_node;
  struct igmp_source *src;

  for (ALL_LIST_ELEMENTS_RO(source_list, src_node, src)) {
    IGMP_SOURCE_DO_DELETE(src->source_flags);
  }
}

static void source_mark_send_flag(struct list *source_list)
{
  struct listnode    *src_node;
  struct igmp_source *src;

  for (ALL_LIST_ELEMENTS_RO(source_list, src_node, src)) {
    IGMP_SOURCE_DO_SEND(src->source_flags);
  }
}

static int source_mark_send_flag_by_timer(struct list *source_list)
{
  struct listnode    *src_node;
  struct igmp_source *src;
  int                 num_marked_sources = 0;

  for (ALL_LIST_ELEMENTS_RO(source_list, src_node, src)) {
    /* Is source timer running? */
    if (src->t_source_timer) {
      IGMP_SOURCE_DO_SEND(src->source_flags);
      ++num_marked_sources;
    }
    else {
      IGMP_SOURCE_DONT_SEND(src->source_flags);
    }
  }

  return num_marked_sources;
}

static void source_clear_send_flag(struct list *source_list)
{
  struct listnode    *src_node;
  struct igmp_source *src;

  for (ALL_LIST_ELEMENTS_RO(source_list, src_node, src)) {
    IGMP_SOURCE_DONT_SEND(src->source_flags);
  }
}

/*
  Any source (*,G) is forwarded only if mode is EXCLUDE {empty}
*/
static void group_exclude_fwd_anysrc_ifempty(struct igmp_group *group)
{
  zassert(group->group_filtermode_isexcl);

  if (listcount(group->group_source_list) < 1) {
    igmp_anysource_forward_start(group);
  }
}

void igmp_source_free(struct igmp_source *source)
{
  /* make sure there is no source timer running */
  zassert(!source->t_source_timer);

  XFREE(MTYPE_PIM_IGMP_GROUP_SOURCE, source);
}

static void source_channel_oil_detach(struct igmp_source *source)
{
  if (source->source_channel_oil) {
    pim_channel_oil_del(source->source_channel_oil);
    source->source_channel_oil = 0;
  }
}

/*
  igmp_source_delete:       stop fowarding, and delete the source
  igmp_source_forward_stop: stop fowarding, but keep the source
*/
void igmp_source_delete(struct igmp_source *source)
{
  struct igmp_group *group;

  group = source->source_group;

  if (PIM_DEBUG_IGMP_TRACE) {
    char group_str[100];
    char source_str[100];
    pim_inet4_dump("<group?>", group->group_addr, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", source->source_addr, source_str, sizeof(source_str));
    zlog_debug("Deleting IGMP source %s for group %s from socket %d interface %s",
	       source_str, group_str,
	       group->group_igmp_sock->fd,
	       group->group_igmp_sock->interface->name);
  }

  source_timer_off(group, source);
  igmp_source_forward_stop(source);

  /* sanity check that forwarding has been disabled */
  if (IGMP_SOURCE_TEST_FORWARDING(source->source_flags)) {
    char group_str[100];
    char source_str[100];
    pim_inet4_dump("<group?>", group->group_addr, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", source->source_addr, source_str, sizeof(source_str));
    zlog_warn("%s: forwarding=ON(!) IGMP source %s for group %s from socket %d interface %s",
	      __PRETTY_FUNCTION__,
	      source_str, group_str,
	      group->group_igmp_sock->fd,
	      group->group_igmp_sock->interface->name);
    /* warning only */
  }

  source_channel_oil_detach(source);

  /*
    notice that listnode_delete() can't be moved
    into igmp_source_free() because the later is
    called by list_delete_all_node()
  */
  listnode_delete(group->group_source_list, source);

  igmp_source_free(source);

  if (group->group_filtermode_isexcl) {
    group_exclude_fwd_anysrc_ifempty(group);
  }
}

static void source_delete_by_flag(struct list *source_list)
{
  struct listnode    *src_node;
  struct listnode    *src_nextnode;
  struct igmp_source *src;
  
  for (ALL_LIST_ELEMENTS(source_list, src_node, src_nextnode, src))
    if (IGMP_SOURCE_TEST_DELETE(src->source_flags))
      igmp_source_delete(src);
}

void igmp_source_delete_expired(struct list *source_list)
{
  struct listnode    *src_node;
  struct listnode    *src_nextnode;
  struct igmp_source *src;
  
  for (ALL_LIST_ELEMENTS(source_list, src_node, src_nextnode, src))
    if (!src->t_source_timer)
      igmp_source_delete(src);
}

struct igmp_source *igmp_find_source_by_addr(struct igmp_group *group,
					     struct in_addr src_addr)
{
  struct listnode    *src_node;
  struct igmp_source *src;

  for (ALL_LIST_ELEMENTS_RO(group->group_source_list, src_node, src))
    if (src_addr.s_addr == src->source_addr.s_addr)
      return src;

  return 0;
}

static struct igmp_source *source_new(struct igmp_group *group,
				      struct in_addr src_addr,
				      const char *ifname)
{
  struct igmp_source *src;

  if (PIM_DEBUG_IGMP_TRACE) {
    char group_str[100];
    char source_str[100];
    pim_inet4_dump("<group?>", group->group_addr, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", src_addr, source_str, sizeof(source_str));
    zlog_debug("Creating new IGMP source %s for group %s on socket %d interface %s",
	       source_str, group_str,
	       group->group_igmp_sock->fd,
	       ifname);
  }

  src = XMALLOC(MTYPE_PIM_IGMP_GROUP_SOURCE, sizeof(*src));
  if (!src) {
    zlog_warn("%s %s: XMALLOC() failure",
	      __FILE__, __PRETTY_FUNCTION__);
    return 0; /* error, not found, could not create */
  }
  
  src->t_source_timer                = 0;
  src->source_group                  = group; /* back pointer */
  src->source_addr                   = src_addr;
  src->source_creation               = pim_time_monotonic_sec();
  src->source_flags                  = 0;
  src->source_query_retransmit_count = 0;
  src->source_channel_oil            = 0;

  listnode_add(group->group_source_list, src);

  zassert(!src->t_source_timer); /* source timer == 0 */

  /* Any source (*,G) is forwarded only if mode is EXCLUDE {empty} */
  igmp_anysource_forward_stop(group);

  return src;
}

static struct igmp_source *add_source_by_addr(struct igmp_sock *igmp,
					      struct igmp_group *group,
					      struct in_addr src_addr,
					      const char *ifname)
{
  struct igmp_source *src;

  src = igmp_find_source_by_addr(group, src_addr);
  if (src) {
    return src;
  }

  src = source_new(group, src_addr, ifname);
  if (!src) {
    return 0;
  }

  return src;
}

static void allow(struct igmp_sock *igmp, struct in_addr from,
		  struct in_addr group_addr,
		  int num_sources, struct in_addr *sources)
{
  struct interface *ifp = igmp->interface;
  struct igmp_group *group;
  int    i;

  /* non-existant group is created as INCLUDE {empty} */
  group = igmp_add_group_by_addr(igmp, group_addr, ifp->name);
  if (!group) {
    return;
  }

  /* scan received sources */
  for (i = 0; i < num_sources; ++i) {
    struct igmp_source *source;
    struct in_addr     *src_addr;

    src_addr = sources + i;

    source = add_source_by_addr(igmp, group, *src_addr,	ifp->name);
    if (!source) {
      continue;
    }

    /*
      RFC 3376: 6.4.1. Reception of Current-State Records

      When receiving IS_IN reports for groups in EXCLUDE mode is
      sources should be moved from set with (timers = 0) to set with
      (timers > 0).

      igmp_source_reset_gmi() below, resetting the source timers to
      GMI, accomplishes this.
    */
    igmp_source_reset_gmi(igmp, group, source);

  } /* scan received sources */
}

void igmpv3_report_isin(struct igmp_sock *igmp, struct in_addr from,
			struct in_addr group_addr,
			int num_sources, struct in_addr *sources)
{
  on_trace(__PRETTY_FUNCTION__,
	   igmp->interface, from, group_addr, num_sources, sources);

  allow(igmp, from, group_addr, num_sources, sources);
}

static void isex_excl(struct igmp_group *group,
		      int num_sources, struct in_addr *sources)
{
  int     i;

  /* EXCLUDE mode */
  zassert(group->group_filtermode_isexcl);
  
  /* E.1: set deletion flag for known sources (X,Y) */
  source_mark_delete_flag(group->group_source_list);

  /* scan received sources (A) */
  for (i = 0; i < num_sources; ++i) {
    struct igmp_source *source;
    struct in_addr     *src_addr;

    src_addr = sources + i;

    /* E.2: lookup reported source from (A) in (X,Y) */
    source = igmp_find_source_by_addr(group, *src_addr);
    if (source) {
      /* E.3: if found, clear deletion flag: (X*A) or (Y*A) */
      IGMP_SOURCE_DONT_DELETE(source->source_flags);
    }
    else {
      /* E.4: if not found, create source with timer=GMI: (A-X-Y) */
      source = source_new(group, *src_addr,
			  group->group_igmp_sock->interface->name);
      if (!source) {
	/* ugh, internal malloc failure, skip source */
	continue;
      }
      zassert(!source->t_source_timer); /* timer == 0 */
      igmp_source_reset_gmi(group->group_igmp_sock, group, source);
      zassert(source->t_source_timer); /* (A-X-Y) timer > 0 */
    }

  } /* scan received sources */

  /* E.5: delete all sources marked with deletion flag: (X-A) and (Y-A) */
  source_delete_by_flag(group->group_source_list);
}

static void isex_incl(struct igmp_group *group,
		      int num_sources, struct in_addr *sources)
{
  int i;

  /* INCLUDE mode */
  zassert(!group->group_filtermode_isexcl);
  
  /* I.1: set deletion flag for known sources (A) */
  source_mark_delete_flag(group->group_source_list);

  /* scan received sources (B) */
  for (i = 0; i < num_sources; ++i) {
    struct igmp_source *source;
    struct in_addr     *src_addr;

    src_addr = sources + i;

    /* I.2: lookup reported source (B) */
    source = igmp_find_source_by_addr(group, *src_addr);
    if (source) {
      /* I.3: if found, clear deletion flag (A*B) */
      IGMP_SOURCE_DONT_DELETE(source->source_flags);
    }
    else {
      /* I.4: if not found, create source with timer=0 (B-A) */
      source = source_new(group, *src_addr,
			  group->group_igmp_sock->interface->name);
      if (!source) {
	/* ugh, internal malloc failure, skip source */
	continue;
      }
      zassert(!source->t_source_timer); /* (B-A) timer=0 */
    }

  } /* scan received sources */

  /* I.5: delete all sources marked with deletion flag (A-B) */
  source_delete_by_flag(group->group_source_list);

  group->group_filtermode_isexcl = 1; /* boolean=true */

  zassert(group->group_filtermode_isexcl);

  group_exclude_fwd_anysrc_ifempty(group);
}

void igmpv3_report_isex(struct igmp_sock *igmp, struct in_addr from,
			struct in_addr group_addr,
			int num_sources, struct in_addr *sources)
{
  struct interface *ifp = igmp->interface;
  struct igmp_group *group;

  on_trace(__PRETTY_FUNCTION__,
	   ifp, from, group_addr, num_sources, sources);

  /* non-existant group is created as INCLUDE {empty} */
  group = igmp_add_group_by_addr(igmp, group_addr, ifp->name);
  if (!group) {
    return;
  }

  if (group->group_filtermode_isexcl) {
    /* EXCLUDE mode */
    isex_excl(group, num_sources, sources);
  }
  else {
    /* INCLUDE mode */
    isex_incl(group, num_sources, sources);
    zassert(group->group_filtermode_isexcl);
  }

  zassert(group->group_filtermode_isexcl);

  igmp_group_reset_gmi(group);
}

static void toin_incl(struct igmp_group *group,
		      int num_sources, struct in_addr *sources)
{
  struct igmp_sock *igmp = group->group_igmp_sock;
  int num_sources_tosend = listcount(group->group_source_list);
  int i;

  /* Set SEND flag for all known sources (A) */
  source_mark_send_flag(group->group_source_list);

  /* Scan received sources (B) */
  for (i = 0; i < num_sources; ++i) {
    struct igmp_source *source;
    struct in_addr     *src_addr;

    src_addr = sources + i;

    /* Lookup reported source (B) */
    source = igmp_find_source_by_addr(group, *src_addr);
    if (source) {
      /* If found, clear SEND flag (A*B) */
      IGMP_SOURCE_DONT_SEND(source->source_flags);
      --num_sources_tosend;
    }
    else {
      /* If not found, create new source */
      source = source_new(group, *src_addr,
			  group->group_igmp_sock->interface->name);
      if (!source) {
	/* ugh, internal malloc failure, skip source */
	continue;
      }
    }

    /* (B)=GMI */
    igmp_source_reset_gmi(igmp, group, source);
  }

  /* Send sources marked with SEND flag: Q(G,A-B) */
  if (num_sources_tosend > 0) {
    source_query_send_by_flag(group, num_sources_tosend);
  }
}

static void toin_excl(struct igmp_group *group,
		      int num_sources, struct in_addr *sources)
{
  struct igmp_sock *igmp = group->group_igmp_sock;
  int num_sources_tosend;
  int i;

  /* Set SEND flag for X (sources with timer > 0) */
  num_sources_tosend = source_mark_send_flag_by_timer(group->group_source_list);

  /* Scan received sources (A) */
  for (i = 0; i < num_sources; ++i) {
    struct igmp_source *source;
    struct in_addr     *src_addr;

    src_addr = sources + i;

    /* Lookup reported source (A) */
    source = igmp_find_source_by_addr(group, *src_addr);
    if (source) {
      if (source->t_source_timer) {
	/* If found and timer running, clear SEND flag (X*A) */
	IGMP_SOURCE_DONT_SEND(source->source_flags);
	--num_sources_tosend;
      }
    }
    else {
      /* If not found, create new source */
      source = source_new(group, *src_addr,
			  group->group_igmp_sock->interface->name);
      if (!source) {
	/* ugh, internal malloc failure, skip source */
	continue;
      }
    }

    /* (A)=GMI */
    igmp_source_reset_gmi(igmp, group, source);
  }

  /* Send sources marked with SEND flag: Q(G,X-A) */
  if (num_sources_tosend > 0) {
    source_query_send_by_flag(group, num_sources_tosend);
  }

  /* Send Q(G) */
  group_query_send(group);
}

void igmpv3_report_toin(struct igmp_sock *igmp, struct in_addr from,
			struct in_addr group_addr,
			int num_sources, struct in_addr *sources)
{
  struct interface *ifp = igmp->interface;
  struct igmp_group *group;

  on_trace(__PRETTY_FUNCTION__,
	   ifp, from, group_addr, num_sources, sources);

  /* non-existant group is created as INCLUDE {empty} */
  group = igmp_add_group_by_addr(igmp, group_addr, ifp->name);
  if (!group) {
    return;
  }

  if (group->group_filtermode_isexcl) {
    /* EXCLUDE mode */
    toin_excl(group, num_sources, sources);
  }
  else {
    /* INCLUDE mode */
    toin_incl(group, num_sources, sources);
  }
}

static void toex_incl(struct igmp_group *group,
		      int num_sources, struct in_addr *sources)
{
  int num_sources_tosend = 0;
  int i;

  zassert(!group->group_filtermode_isexcl);

  /* Set DELETE flag for all known sources (A) */
  source_mark_delete_flag(group->group_source_list);

  /* Clear off SEND flag from all known sources (A) */
  source_clear_send_flag(group->group_source_list);

  /* Scan received sources (B) */
  for (i = 0; i < num_sources; ++i) {
    struct igmp_source *source;
    struct in_addr     *src_addr;

    src_addr = sources + i;

    /* Lookup reported source (B) */
    source = igmp_find_source_by_addr(group, *src_addr);
    if (source) {
      /* If found, clear deletion flag: (A*B) */
      IGMP_SOURCE_DONT_DELETE(source->source_flags);
      /* and set SEND flag (A*B) */
      IGMP_SOURCE_DO_SEND(source->source_flags);
      ++num_sources_tosend;
    }
    else {
      /* If source not found, create source with timer=0: (B-A)=0 */
      source = source_new(group, *src_addr,
			  group->group_igmp_sock->interface->name);
      if (!source) {
	/* ugh, internal malloc failure, skip source */
	continue;
      }
      zassert(!source->t_source_timer); /* (B-A) timer=0 */
    }

  } /* Scan received sources (B) */

  group->group_filtermode_isexcl = 1; /* boolean=true */

  /* Delete all sources marked with DELETE flag (A-B) */
  source_delete_by_flag(group->group_source_list);

  /* Send sources marked with SEND flag: Q(G,A*B) */
  if (num_sources_tosend > 0) {
    source_query_send_by_flag(group, num_sources_tosend);
  }

  zassert(group->group_filtermode_isexcl);

  group_exclude_fwd_anysrc_ifempty(group);
}

static void toex_excl(struct igmp_group *group,
		      int num_sources, struct in_addr *sources)
{
  int num_sources_tosend = 0;
  int i;

  /* set DELETE flag for all known sources (X,Y) */
  source_mark_delete_flag(group->group_source_list);

  /* clear off SEND flag from all known sources (X,Y) */
  source_clear_send_flag(group->group_source_list);

  /* scan received sources (A) */
  for (i = 0; i < num_sources; ++i) {
    struct igmp_source *source;
    struct in_addr     *src_addr;
    
    src_addr = sources + i;
    
    /* lookup reported source (A) in known sources (X,Y) */
    source = igmp_find_source_by_addr(group, *src_addr);
    if (source) {
      /* if found, clear off DELETE flag from reported source (A) */
      IGMP_SOURCE_DONT_DELETE(source->source_flags);
    }
    else {
      /* if not found, create source with Group Timer: (A-X-Y)=Group Timer */
      long group_timer_msec;
      source = source_new(group, *src_addr,
			  group->group_igmp_sock->interface->name);
      if (!source) {
	/* ugh, internal malloc failure, skip source */
	continue;
      }

      zassert(!source->t_source_timer); /* timer == 0 */
      group_timer_msec = igmp_group_timer_remain_msec(group);
      igmp_source_timer_on(group, source, group_timer_msec);
      zassert(source->t_source_timer); /* (A-X-Y) timer > 0 */

      /* make sure source is created with DELETE flag unset */
      zassert(!IGMP_SOURCE_TEST_DELETE(source->source_flags));
    }

    /* make sure reported source has DELETE flag unset */
    zassert(!IGMP_SOURCE_TEST_DELETE(source->source_flags));

    if (source->t_source_timer) {
      /* if source timer>0 mark SEND flag: Q(G,A-Y) */
      IGMP_SOURCE_DO_SEND(source->source_flags);
      ++num_sources_tosend;
    }

  } /* scan received sources (A) */

  /*
    delete all sources marked with DELETE flag:
    Delete (X-A)
    Delete (Y-A)
  */
  source_delete_by_flag(group->group_source_list);
 
  /* send sources marked with SEND flag: Q(G,A-Y) */
  if (num_sources_tosend > 0) {
    source_query_send_by_flag(group, num_sources_tosend);
  }
}

void igmpv3_report_toex(struct igmp_sock *igmp, struct in_addr from,
			struct in_addr group_addr,
			int num_sources, struct in_addr *sources)
{
  struct interface *ifp = igmp->interface;
  struct igmp_group *group;

  on_trace(__PRETTY_FUNCTION__,
	   ifp, from, group_addr, num_sources, sources);

  /* non-existant group is created as INCLUDE {empty} */
  group = igmp_add_group_by_addr(igmp, group_addr, ifp->name);
  if (!group) {
    return;
  }

  if (group->group_filtermode_isexcl) {
    /* EXCLUDE mode */
    toex_excl(group, num_sources, sources);
  }
  else {
    /* INCLUDE mode */
    toex_incl(group, num_sources, sources);
    zassert(group->group_filtermode_isexcl);
  }
  zassert(group->group_filtermode_isexcl);

  /* Group Timer=GMI */
  igmp_group_reset_gmi(group);
}

void igmpv3_report_allow(struct igmp_sock *igmp, struct in_addr from,
			 struct in_addr group_addr,
			 int num_sources, struct in_addr *sources)
{
  on_trace(__PRETTY_FUNCTION__,
	   igmp->interface, from, group_addr, num_sources, sources);

  allow(igmp, from, group_addr, num_sources, sources);
}

/*
  RFC3376: 6.6.3.1. Building and Sending Group Specific Queries

  When transmitting a group specific query, if the group timer is
  larger than LMQT, the "Suppress Router-Side Processing" bit is set
  in the query message.
*/
static void group_retransmit_group(struct igmp_group *group)
{
  char                  query_buf[PIM_IGMP_BUFSIZE_WRITE];
  struct igmp_sock     *igmp;
  struct pim_interface *pim_ifp;
  long                  lmqc;      /* Last Member Query Count */
  long                  lmqi_msec; /* Last Member Query Interval */
  long                  lmqt_msec; /* Last Member Query Time */
  int                   s_flag;

  igmp = group->group_igmp_sock;
  pim_ifp = igmp->interface->info;

  lmqc      = igmp->querier_robustness_variable;
  lmqi_msec = 100 * pim_ifp->igmp_specific_query_max_response_time_dsec;
  lmqt_msec = lmqc * lmqi_msec;

  /*
    RFC3376: 6.6.3.1. Building and Sending Group Specific Queries
    
    When transmitting a group specific query, if the group timer is
    larger than LMQT, the "Suppress Router-Side Processing" bit is set
    in the query message.
  */
  s_flag = igmp_group_timer_remain_msec(group) > lmqt_msec;

  if (PIM_DEBUG_IGMP_TRACE) {
    char group_str[100];
    pim_inet4_dump("<group?>", group->group_addr, group_str, sizeof(group_str));
    zlog_debug("retransmit_group_specific_query: group %s on %s: s_flag=%d count=%d",
	       group_str, igmp->interface->name, s_flag,
	       group->group_specific_query_retransmit_count);
  }

  /*
    RFC3376: 4.1.12. IP Destination Addresses for Queries

    Group-Specific and Group-and-Source-Specific Queries are sent with
    an IP destination address equal to the multicast address of
    interest.
  */

  pim_igmp_send_membership_query(group,
				 igmp->fd,
				 igmp->interface->name,
				 query_buf,
				 sizeof(query_buf),
				 0 /* num_sources_tosend */,
				 group->group_addr /* dst_addr */,
				 group->group_addr /* group_addr */,
				 pim_ifp->igmp_specific_query_max_response_time_dsec,
				 s_flag,
				 igmp->querier_robustness_variable,
				 igmp->querier_query_interval);
}

/*
  RFC3376: 6.6.3.2. Building and Sending Group and Source Specific Queries

  When building a group and source specific query for a group G, two
  separate query messages are sent for the group.  The first one has
  the "Suppress Router-Side Processing" bit set and contains all the
  sources with retransmission state and timers greater than LMQT.  The
  second has the "Suppress Router-Side Processing" bit clear and
  contains all the sources with retransmission state and timers lower
  or equal to LMQT.  If either of the two calculated messages does not
  contain any sources, then its transmission is suppressed.
 */
static int group_retransmit_sources(struct igmp_group *group,
				    int send_with_sflag_set)
{
  struct igmp_sock     *igmp;
  struct pim_interface *pim_ifp;
  long                  lmqc;      /* Last Member Query Count */
  long                  lmqi_msec; /* Last Member Query Interval */
  long                  lmqt_msec; /* Last Member Query Time */
  char                  query_buf1[PIM_IGMP_BUFSIZE_WRITE]; /* 1 = with s_flag set */
  char                  query_buf2[PIM_IGMP_BUFSIZE_WRITE]; /* 2 = with s_flag clear */
  int                   query_buf1_max_sources;
  int                   query_buf2_max_sources;
  struct in_addr       *source_addr1;
  struct in_addr       *source_addr2;
  int                   num_sources_tosend1;
  int                   num_sources_tosend2;
  struct listnode      *src_node;
  struct igmp_source   *src;
  int                   num_retransmit_sources_left = 0;
  
  query_buf1_max_sources = (sizeof(query_buf1) - IGMP_V3_SOURCES_OFFSET) >> 2;
  query_buf2_max_sources = (sizeof(query_buf2) - IGMP_V3_SOURCES_OFFSET) >> 2;
  
  source_addr1 = (struct in_addr *)(query_buf1 + IGMP_V3_SOURCES_OFFSET);
  source_addr2 = (struct in_addr *)(query_buf2 + IGMP_V3_SOURCES_OFFSET);

  igmp = group->group_igmp_sock;
  pim_ifp = igmp->interface->info;

  lmqc      = igmp->querier_robustness_variable;
  lmqi_msec = 100 * pim_ifp->igmp_specific_query_max_response_time_dsec;
  lmqt_msec = lmqc * lmqi_msec;

  /* Scan all group sources */
  for (ALL_LIST_ELEMENTS_RO(group->group_source_list, src_node, src)) {

    /* Source has retransmission state? */
    if (src->source_query_retransmit_count < 1)
      continue;

    if (--src->source_query_retransmit_count > 0) {
      ++num_retransmit_sources_left;
    }

    /* Copy source address into appropriate query buffer */
    if (igmp_source_timer_remain_msec(src) > lmqt_msec) {
      *source_addr1 = src->source_addr;
      ++source_addr1;
    }
    else {
      *source_addr2 = src->source_addr;
      ++source_addr2;
    }

  }
  
  num_sources_tosend1 = source_addr1 - (struct in_addr *)(query_buf1 + IGMP_V3_SOURCES_OFFSET);
  num_sources_tosend2 = source_addr2 - (struct in_addr *)(query_buf2 + IGMP_V3_SOURCES_OFFSET);

  if (PIM_DEBUG_IGMP_TRACE) {
    char group_str[100];
    pim_inet4_dump("<group?>", group->group_addr, group_str, sizeof(group_str));
    zlog_debug("retransmit_grp&src_specific_query: group %s on %s: srcs_with_sflag=%d srcs_wo_sflag=%d will_send_sflag=%d retransmit_src_left=%d",
	       group_str, igmp->interface->name,
	       num_sources_tosend1,
	       num_sources_tosend2,
	       send_with_sflag_set,
	       num_retransmit_sources_left);
  }

  if (num_sources_tosend1 > 0) {
    /*
      Send group-and-source-specific query with s_flag set and all
      sources with timers greater than LMQT.
    */

    if (send_with_sflag_set) {

      query_buf1_max_sources = (sizeof(query_buf1) - IGMP_V3_SOURCES_OFFSET) >> 2;
      if (num_sources_tosend1 > query_buf1_max_sources) {
	char group_str[100];
	pim_inet4_dump("<group?>", group->group_addr, group_str, sizeof(group_str));
	zlog_warn("%s: group %s on %s: s_flag=1 unable to fit %d sources into buf_size=%zu (max_sources=%d)",
		  __PRETTY_FUNCTION__, group_str, igmp->interface->name,
		  num_sources_tosend1, sizeof(query_buf1), query_buf1_max_sources);
      }
      else {
	/*
	  RFC3376: 4.1.12. IP Destination Addresses for Queries
      
	  Group-Specific and Group-and-Source-Specific Queries are sent with
	  an IP destination address equal to the multicast address of
	  interest.
	*/
    
	pim_igmp_send_membership_query(group,
				       igmp->fd,
				       igmp->interface->name,
				       query_buf1,
				       sizeof(query_buf1),
				       num_sources_tosend1,
				       group->group_addr,
				       group->group_addr,
				       pim_ifp->igmp_specific_query_max_response_time_dsec,
				       1 /* s_flag */,
				       igmp->querier_robustness_variable,
				       igmp->querier_query_interval);
    
      }

    } /* send_with_sflag_set */

  }

  if (num_sources_tosend2 > 0) {
    /*
      Send group-and-source-specific query with s_flag clear and all
      sources with timers lower or equal to LMQT.
    */
  
    query_buf2_max_sources = (sizeof(query_buf2) - IGMP_V3_SOURCES_OFFSET) >> 2;
    if (num_sources_tosend2 > query_buf2_max_sources) {
      char group_str[100];
      pim_inet4_dump("<group?>", group->group_addr, group_str, sizeof(group_str));
      zlog_warn("%s: group %s on %s: s_flag=0 unable to fit %d sources into buf_size=%zu (max_sources=%d)",
		__PRETTY_FUNCTION__, group_str, igmp->interface->name,
		num_sources_tosend2, sizeof(query_buf2), query_buf2_max_sources);
    }
    else {
      /*
	RFC3376: 4.1.12. IP Destination Addresses for Queries

	Group-Specific and Group-and-Source-Specific Queries are sent with
	an IP destination address equal to the multicast address of
	interest.
      */

      pim_igmp_send_membership_query(group,
				     igmp->fd,
				     igmp->interface->name,
				     query_buf2,
				     sizeof(query_buf2),
				     num_sources_tosend2,
				     group->group_addr,
				     group->group_addr,
				     pim_ifp->igmp_specific_query_max_response_time_dsec,
				     0 /* s_flag */,
				     igmp->querier_robustness_variable,
				     igmp->querier_query_interval);

    }
  }

  return num_retransmit_sources_left;
}

static int igmp_group_retransmit(struct thread *t)
{
  struct igmp_group *group;
  int num_retransmit_sources_left;
  int send_with_sflag_set; /* boolean */

  zassert(t);
  group = THREAD_ARG(t);
  zassert(group);

  if (PIM_DEBUG_IGMP_TRACE) {
    char group_str[100];
    pim_inet4_dump("<group?>", group->group_addr, group_str, sizeof(group_str));
    zlog_debug("group_retransmit_timer: group %s on %s",
	       group_str, group->group_igmp_sock->interface->name);
  }

  /* Retransmit group-specific queries? (RFC3376: 6.6.3.1) */
  if (group->group_specific_query_retransmit_count > 0) {

    /* Retransmit group-specific queries (RFC3376: 6.6.3.1) */
    group_retransmit_group(group);
    --group->group_specific_query_retransmit_count;

    /*
      RFC3376: 6.6.3.2
      If a group specific query is scheduled to be transmitted at the
      same time as a group and source specific query for the same group,
      then transmission of the group and source specific message with the
      "Suppress Router-Side Processing" bit set may be suppressed.
    */
    send_with_sflag_set = 0; /* boolean=false */
  }
  else {
    send_with_sflag_set = 1; /* boolean=true */
  }

  /* Retransmit group-and-source-specific queries (RFC3376: 6.6.3.2) */
  num_retransmit_sources_left = group_retransmit_sources(group,
							 send_with_sflag_set);

  group->t_group_query_retransmit_timer = 0;

  /*
    Keep group retransmit timer running if there is any retransmit
    counter pending
  */
  if ((num_retransmit_sources_left > 0) ||
      (group->group_specific_query_retransmit_count > 0)) {
    group_retransmit_timer_on(group);
  }

  return 0;
}

/*
  group_retransmit_timer_on:
  if group retransmit timer isn't running, starts it;
  otherwise, do nothing
*/
static void group_retransmit_timer_on(struct igmp_group *group)
{
  struct igmp_sock     *igmp;
  struct pim_interface *pim_ifp;
  long                  lmqi_msec; /* Last Member Query Interval */

  /* if group retransmit timer is running, do nothing */
  if (group->t_group_query_retransmit_timer) {
    return;
  }

  igmp = group->group_igmp_sock;
  pim_ifp = igmp->interface->info;

  lmqi_msec = 100 * pim_ifp->igmp_specific_query_max_response_time_dsec;

  if (PIM_DEBUG_IGMP_TRACE) {
    char group_str[100];
    pim_inet4_dump("<group?>", group->group_addr, group_str, sizeof(group_str));
    zlog_debug("Scheduling %ld.%03ld sec retransmit timer for group %s on %s",
	       lmqi_msec / 1000,
	       lmqi_msec % 1000,
	       group_str,
	       igmp->interface->name);
  }

  THREAD_TIMER_MSEC_ON(master, group->t_group_query_retransmit_timer,
		       igmp_group_retransmit,
		       group, lmqi_msec);
}

static long igmp_group_timer_remain_msec(struct igmp_group *group)
{
  return pim_time_timer_remain_msec(group->t_group_timer);
}

static long igmp_source_timer_remain_msec(struct igmp_source *source)
{
  return pim_time_timer_remain_msec(source->t_source_timer);
}

/*
  RFC3376: 6.6.3.1. Building and Sending Group Specific Queries
*/
static void group_query_send(struct igmp_group *group)
{
  long              lmqc;    /* Last Member Query Count */

  lmqc = group->group_igmp_sock->querier_robustness_variable;

  /* lower group timer to lmqt */
  igmp_group_timer_lower_to_lmqt(group);

  /* reset retransmission counter */
  group->group_specific_query_retransmit_count = lmqc;

  /* immediately send group specific query (decrease retransmit counter by 1)*/
  group_retransmit_group(group);

  /* make sure group retransmit timer is running */
  group_retransmit_timer_on(group);
}

/*
  RFC3376: 6.6.3.2. Building and Sending Group and Source Specific Queries
*/
static void source_query_send_by_flag(struct igmp_group *group,
				      int num_sources_tosend)
{
  struct igmp_sock     *igmp;
  struct pim_interface *pim_ifp;
  struct listnode      *src_node;
  struct igmp_source   *src;
  long                  lmqc;      /* Last Member Query Count */
  long                  lmqi_msec; /* Last Member Query Interval */
  long                  lmqt_msec; /* Last Member Query Time */

  zassert(num_sources_tosend > 0);

  igmp = group->group_igmp_sock;
  pim_ifp = igmp->interface->info;

  lmqc      = igmp->querier_robustness_variable;
  lmqi_msec = 100 * pim_ifp->igmp_specific_query_max_response_time_dsec;
  lmqt_msec = lmqc * lmqi_msec;

  /*
    RFC3376: 6.6.3.2. Building and Sending Group and Source Specific Queries

    (...) for each of the sources in X of group G, with source timer larger
    than LMQT:
    o Set number of retransmissions for each source to [Last Member
    Query Count].
    o Lower source timer to LMQT.
  */
  for (ALL_LIST_ELEMENTS_RO(group->group_source_list, src_node, src)) {
    if (IGMP_SOURCE_TEST_SEND(src->source_flags)) {
      /* source "src" in X of group G */
      if (igmp_source_timer_remain_msec(src) > lmqt_msec) {
	src->source_query_retransmit_count = lmqc;
	igmp_source_timer_lower_to_lmqt(src);
      }
    }
  }

  /* send group-and-source specific queries */
  group_retransmit_sources(group, 1 /* send_with_sflag_set=true */);

  /* make sure group retransmit timer is running */
  group_retransmit_timer_on(group);
}

static void block_excl(struct igmp_group *group,
		       int num_sources, struct in_addr *sources)
{
  int num_sources_tosend = 0;
  int i;

  /* 1. clear off SEND flag from all known sources (X,Y) */
  source_clear_send_flag(group->group_source_list);

  /* 2. scan received sources (A) */
  for (i = 0; i < num_sources; ++i) {
    struct igmp_source *source;
    struct in_addr     *src_addr;
    
    src_addr = sources + i;
    
    /* lookup reported source (A) in known sources (X,Y) */
    source = igmp_find_source_by_addr(group, *src_addr);
    if (!source) {
      /* 3: if not found, create source with Group Timer: (A-X-Y)=Group Timer */
      long group_timer_msec;
      source = source_new(group, *src_addr,
			  group->group_igmp_sock->interface->name);
      if (!source) {
	/* ugh, internal malloc failure, skip source */
	continue;
      }

      zassert(!source->t_source_timer); /* timer == 0 */
      group_timer_msec = igmp_group_timer_remain_msec(group);
      igmp_source_timer_on(group, source, group_timer_msec);
      zassert(source->t_source_timer); /* (A-X-Y) timer > 0 */
    }

    if (source->t_source_timer) {
      /* 4. if source timer>0 mark SEND flag: Q(G,A-Y) */
      IGMP_SOURCE_DO_SEND(source->source_flags);
      ++num_sources_tosend;
    }
  }
 
  /* 5. send sources marked with SEND flag: Q(G,A-Y) */
  if (num_sources_tosend > 0) {
    source_query_send_by_flag(group, num_sources_tosend);
  }
}

static void block_incl(struct igmp_group *group,
		       int num_sources, struct in_addr *sources)
{
  int num_sources_tosend = 0;
  int i;

  /* 1. clear off SEND flag from all known sources (B) */
  source_clear_send_flag(group->group_source_list);

  /* 2. scan received sources (A) */
  for (i = 0; i < num_sources; ++i) {
    struct igmp_source *source;
    struct in_addr     *src_addr;
    
    src_addr = sources + i;
    
    /* lookup reported source (A) in known sources (B) */
    source = igmp_find_source_by_addr(group, *src_addr);
    if (source) {
      /* 3. if found (A*B), mark SEND flag: Q(G,A*B) */
      IGMP_SOURCE_DO_SEND(source->source_flags);
      ++num_sources_tosend;
    }
  } 
 
  /* 4. send sources marked with SEND flag: Q(G,A*B) */
  if (num_sources_tosend > 0) {
    source_query_send_by_flag(group, num_sources_tosend);
  }
}

void igmpv3_report_block(struct igmp_sock *igmp, struct in_addr from,
			 struct in_addr group_addr,
			 int num_sources, struct in_addr *sources)
{
  struct interface *ifp = igmp->interface;
  struct igmp_group *group;

  on_trace(__PRETTY_FUNCTION__,
	   ifp, from, group_addr, num_sources, sources);

  /* non-existant group is created as INCLUDE {empty} */
  group = igmp_add_group_by_addr(igmp, group_addr, ifp->name);
  if (!group) {
    return;
  }

  if (group->group_filtermode_isexcl) {
    /* EXCLUDE mode */
    block_excl(group, num_sources, sources);
  }
  else {
    /* INCLUDE mode */
    block_incl(group, num_sources, sources);
  }
}

void igmp_group_timer_lower_to_lmqt(struct igmp_group *group)
{
  struct igmp_sock     *igmp;
  struct interface     *ifp;
  struct pim_interface *pim_ifp;
  char                 *ifname;
  int   lmqi_dsec; /* Last Member Query Interval */
  int   lmqc;      /* Last Member Query Count */
  int   lmqt_msec; /* Last Member Query Time */

  /*
    RFC 3376: 6.2.2. Definition of Group Timers
    
    The group timer is only used when a group is in EXCLUDE mode and
    it represents the time for the *filter-mode* of the group to
    expire and switch to INCLUDE mode.
  */
  if (!group->group_filtermode_isexcl) {
    return;
  }

  igmp    = group->group_igmp_sock;
  ifp     = igmp->interface;
  pim_ifp = ifp->info;
  ifname  = ifp->name;

  lmqi_dsec = pim_ifp->igmp_specific_query_max_response_time_dsec;
  lmqc      = igmp->querier_robustness_variable;
  lmqt_msec = PIM_IGMP_LMQT_MSEC(lmqi_dsec, lmqc); /* lmqt_msec = (100 * lmqi_dsec) * lmqc */

  if (PIM_DEBUG_IGMP_TRACE) {
    char group_str[100];
    pim_inet4_dump("<group?>", group->group_addr, group_str, sizeof(group_str));
    zlog_debug("%s: group %s on %s: LMQC=%d LMQI=%d dsec LMQT=%d msec",
	       __PRETTY_FUNCTION__,
	       group_str, ifname,
	       lmqc, lmqi_dsec, lmqt_msec);
  }

  zassert(group->group_filtermode_isexcl);

  igmp_group_timer_on(group, lmqt_msec, ifname);
}

void igmp_source_timer_lower_to_lmqt(struct igmp_source *source)
{
  struct igmp_group    *group;
  struct igmp_sock     *igmp;
  struct interface     *ifp;
  struct pim_interface *pim_ifp;
  char                 *ifname;
  int   lmqi_dsec; /* Last Member Query Interval */
  int   lmqc;      /* Last Member Query Count */
  int   lmqt_msec; /* Last Member Query Time */

  group   = source->source_group;
  igmp    = group->group_igmp_sock;
  ifp     = igmp->interface;
  pim_ifp = ifp->info;
  ifname  = ifp->name;

  lmqi_dsec = pim_ifp->igmp_specific_query_max_response_time_dsec;
  lmqc      = igmp->querier_robustness_variable;
  lmqt_msec = PIM_IGMP_LMQT_MSEC(lmqi_dsec, lmqc); /* lmqt_msec = (100 * lmqi_dsec) * lmqc */

  if (PIM_DEBUG_IGMP_TRACE) {
    char group_str[100];
    char source_str[100];
    pim_inet4_dump("<group?>", group->group_addr, group_str, sizeof(group_str));
    pim_inet4_dump("<source?>", source->source_addr, source_str, sizeof(source_str));
    zlog_debug("%s: group %s source %s on %s: LMQC=%d LMQI=%d dsec LMQT=%d msec",
	       __PRETTY_FUNCTION__,
	       group_str, source_str, ifname,
	       lmqc, lmqi_dsec, lmqt_msec);
  }

  igmp_source_timer_on(group, source, lmqt_msec);
}

/*
  Copy sources to message:
    
  struct in_addr *sources = (struct in_addr *)(query_buf + IGMP_V3_SOURCES_OFFSET);
  if (num_sources > 0) {
  struct listnode    *node;
  struct igmp_source *src;
  int                 i = 0;

  for (ALL_LIST_ELEMENTS_RO(source_list, node, src)) {
  sources[i++] = src->source_addr;
  }
  }
*/
void pim_igmp_send_membership_query(struct igmp_group *group,
				    int fd,
				    const char *ifname,
				    char *query_buf,
				    int query_buf_size,
				    int num_sources,
				    struct in_addr dst_addr,
				    struct in_addr group_addr,
				    int query_max_response_time_dsec,
				    uint8_t s_flag,
				    uint8_t querier_robustness_variable,
				    uint16_t querier_query_interval)
{
  ssize_t             msg_size;
  uint8_t             max_resp_code;
  uint8_t             qqic;
  ssize_t             sent;
  struct sockaddr_in  to;
  socklen_t           tolen;
  uint16_t            checksum;

  zassert(num_sources >= 0);

  msg_size = IGMP_V3_SOURCES_OFFSET + (num_sources << 2);
  if (msg_size > query_buf_size) {
    zlog_err("%s %s: unable to send: msg_size=%zd larger than query_buf_size=%d",
	     __FILE__, __PRETTY_FUNCTION__,
	     msg_size, query_buf_size);
    return;
  }

  s_flag = PIM_FORCE_BOOLEAN(s_flag);
  zassert((s_flag == 0) || (s_flag == 1));

  max_resp_code = igmp_msg_encode16to8(query_max_response_time_dsec);
  qqic          = igmp_msg_encode16to8(querier_query_interval);

  /*
    RFC 3376: 4.1.6. QRV (Querier's Robustness Variable)

    If non-zero, the QRV field contains the [Robustness Variable]
    value used by the querier, i.e., the sender of the Query.  If the
    querier's [Robustness Variable] exceeds 7, the maximum value of
    the QRV field, the QRV is set to zero.
  */
  if (querier_robustness_variable > 7) {
    querier_robustness_variable = 0;
  }

  query_buf[0]                                         = PIM_IGMP_MEMBERSHIP_QUERY;
  query_buf[1]                                         = max_resp_code;
  *(uint16_t *)(query_buf + IGMP_V3_CHECKSUM_OFFSET)   = 0; /* for computing checksum */
  memcpy(query_buf+4, &group_addr, sizeof(struct in_addr));

  query_buf[8]                                         = (s_flag << 3) | querier_robustness_variable;
  query_buf[9]                                         = qqic;
  *(uint16_t *)(query_buf + IGMP_V3_NUMSOURCES_OFFSET) = htons(num_sources);

  checksum = in_cksum(query_buf, msg_size);
  *(uint16_t *)(query_buf + IGMP_V3_CHECKSUM_OFFSET) = checksum;

  if (PIM_DEBUG_IGMP_PACKETS) {
    char dst_str[100];
    char group_str[100];
    pim_inet4_dump("<dst?>", dst_addr, dst_str, sizeof(dst_str));
    pim_inet4_dump("<group?>", group_addr, group_str, sizeof(group_str));
    zlog_debug("%s: to %s on %s: group=%s sources=%d msg_size=%zd s_flag=%x QRV=%u QQI=%u QQIC=%02x checksum=%x",
	       __PRETTY_FUNCTION__,
	       dst_str, ifname, group_str, num_sources,
	       msg_size, s_flag, querier_robustness_variable,
	       querier_query_interval, qqic, checksum);
  }

#if 0
  memset(&to, 0, sizeof(to));
#endif
  to.sin_family = AF_INET;
  to.sin_addr = dst_addr;
#if 0
  to.sin_port = htons(0);
#endif
  tolen = sizeof(to);

  sent = sendto(fd, query_buf, msg_size, MSG_DONTWAIT, &to, tolen);
  if (sent != (ssize_t) msg_size) {
    int e = errno;
    char dst_str[100];
    char group_str[100];
    pim_inet4_dump("<dst?>", dst_addr, dst_str, sizeof(dst_str));
    pim_inet4_dump("<group?>", group_addr, group_str, sizeof(group_str));
    if (sent < 0) {
      zlog_warn("%s: sendto() failure to %s on %s: group=%s msg_size=%zd: errno=%d: %s",
		__PRETTY_FUNCTION__,
		dst_str, ifname, group_str, msg_size,
		e, safe_strerror(e));
    }
    else {
      zlog_warn("%s: sendto() partial to %s on %s: group=%s msg_size=%zd: sent=%zd",
		__PRETTY_FUNCTION__,
		dst_str, ifname, group_str,
		msg_size, sent);
    }
    return;
  }

  /*
    s_flag sanity test: s_flag must be set for general queries

    RFC 3376: 6.6.1. Timer Updates

    When a router sends or receives a query with a clear Suppress
    Router-Side Processing flag, it must update its timers to reflect
    the correct timeout values for the group or sources being queried.

    General queries don't trigger timer update.
  */
  if (!s_flag) {
    /* general query? */
    if (PIM_INADDR_IS_ANY(group_addr)) {
      char dst_str[100];
      char group_str[100];
      pim_inet4_dump("<dst?>", dst_addr, dst_str, sizeof(dst_str));
      pim_inet4_dump("<group?>", group_addr, group_str, sizeof(group_str));
      zlog_warn("%s: to %s on %s: group=%s sources=%d: s_flag is clear for general query!",
		__PRETTY_FUNCTION__,
		dst_str, ifname, group_str, num_sources);
    }
  }

}
