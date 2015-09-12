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
#include "pim_neighbor.h"
#include "pim_time.h"
#include "pim_str.h"
#include "pim_iface.h"
#include "pim_pim.h"
#include "pim_upstream.h"
#include "pim_ifchannel.h"

static void dr_election_by_addr(struct interface *ifp)
{
  struct pim_interface *pim_ifp;
  struct listnode      *node;
  struct pim_neighbor  *neigh;

  pim_ifp = ifp->info;
  zassert(pim_ifp);

  pim_ifp->pim_dr_addr = pim_ifp->primary_address;

  if (PIM_DEBUG_PIM_TRACE) {
    zlog_debug("%s: on interface %s",
	       __PRETTY_FUNCTION__,
	       ifp->name);
  }

  for (ALL_LIST_ELEMENTS_RO(pim_ifp->pim_neighbor_list, node, neigh)) {
    if (ntohl(neigh->source_addr.s_addr) > ntohl(pim_ifp->pim_dr_addr.s_addr)) {
      pim_ifp->pim_dr_addr = neigh->source_addr;
    }
  }
}

static void dr_election_by_pri(struct interface *ifp)
{
  struct pim_interface *pim_ifp;
  struct listnode      *node;
  struct pim_neighbor  *neigh;
  uint32_t              dr_pri;

  pim_ifp = ifp->info;
  zassert(pim_ifp);

  pim_ifp->pim_dr_addr = pim_ifp->primary_address;
  dr_pri = pim_ifp->pim_dr_priority;

  if (PIM_DEBUG_PIM_TRACE) {
    zlog_debug("%s: dr pri %u on interface %s",
	       __PRETTY_FUNCTION__,
	       dr_pri, ifp->name);
  }

  for (ALL_LIST_ELEMENTS_RO(pim_ifp->pim_neighbor_list, node, neigh)) {
    if (PIM_DEBUG_PIM_TRACE) {
      zlog_info("%s: neigh pri %u addr %x if dr addr %x",
		__PRETTY_FUNCTION__,
		neigh->dr_priority,
		ntohl(neigh->source_addr.s_addr),
		ntohl(pim_ifp->pim_dr_addr.s_addr));
    }
    if (
	(neigh->dr_priority > dr_pri) ||
	(
	 (neigh->dr_priority == dr_pri) &&
	 (ntohl(neigh->source_addr.s_addr) > ntohl(pim_ifp->pim_dr_addr.s_addr))
	 )
	) {
      pim_ifp->pim_dr_addr = neigh->source_addr;
      dr_pri               = neigh->dr_priority;
    }
  }
}

/*
  RFC 4601: 4.3.2.  DR Election

  A router's idea of the current DR on an interface can change when a
  PIM Hello message is received, when a neighbor times out, or when a
  router's own DR Priority changes.
 */
void pim_if_dr_election(struct interface *ifp)
{
  struct pim_interface *pim_ifp = ifp->info;
  struct in_addr old_dr_addr;

  ++pim_ifp->pim_dr_election_count;

  old_dr_addr = pim_ifp->pim_dr_addr;

  if (pim_ifp->pim_dr_num_nondrpri_neighbors) {
    dr_election_by_addr(ifp);
  }
  else {
    dr_election_by_pri(ifp);
  }

  /* DR changed ? */
  if (old_dr_addr.s_addr != pim_ifp->pim_dr_addr.s_addr) {

    /* if (PIM_DEBUG_PIM_EVENTS) */ {
      char dr_old_str[100];
      char dr_new_str[100];
      pim_inet4_dump("<old_dr?>", old_dr_addr, dr_old_str, sizeof(dr_old_str));
      pim_inet4_dump("<new_dr?>", pim_ifp->pim_dr_addr, dr_new_str, sizeof(dr_new_str));
      zlog_debug("%s: DR was %s now is %s on interface %s",
		 __PRETTY_FUNCTION__,
		 dr_old_str, dr_new_str, ifp->name);
    }

    pim_ifp->pim_dr_election_last = pim_time_monotonic_sec(); /* timestamp */
    ++pim_ifp->pim_dr_election_changes; 
    pim_if_update_join_desired(pim_ifp);
    pim_if_update_could_assert(ifp);
    pim_if_update_assert_tracking_desired(ifp);
  }
}

static void update_dr_priority(struct pim_neighbor *neigh,
			       pim_hello_options hello_options,
			       uint32_t dr_priority)
{
  pim_hello_options will_set_pri; /* boolean */
  pim_hello_options bit_flip;     /* boolean */
  pim_hello_options pri_change;   /* boolean */

  will_set_pri = PIM_OPTION_IS_SET(hello_options,
				   PIM_OPTION_MASK_DR_PRIORITY);

  bit_flip =
    (
     will_set_pri !=
     PIM_OPTION_IS_SET(neigh->hello_options, PIM_OPTION_MASK_DR_PRIORITY)
     );

  if (bit_flip) {
    struct pim_interface *pim_ifp = neigh->interface->info;

    /* update num. of neighbors without dr_pri */

    if (will_set_pri) {
      --pim_ifp->pim_dr_num_nondrpri_neighbors; 
    }
    else {
      ++pim_ifp->pim_dr_num_nondrpri_neighbors; 
    }
  }

  pri_change = 
    (
     bit_flip
     ||
     (neigh->dr_priority != dr_priority)
     );

  if (will_set_pri) {
    neigh->dr_priority = dr_priority;
  }
  else {
    neigh->dr_priority = 0; /* cosmetic unset */
  }

  if (pri_change) {
    /*
      RFC 4601: 4.3.2.  DR Election
      
      A router's idea of the current DR on an interface can change when a
      PIM Hello message is received, when a neighbor times out, or when a
      router's own DR Priority changes.
    */
    pim_if_dr_election(neigh->interface); // router's own DR Priority changes
  }
}

static int on_neighbor_timer(struct thread *t)
{
  struct pim_neighbor *neigh;
  struct interface *ifp;
  char msg[100];

  zassert(t);
  neigh = THREAD_ARG(t);
  zassert(neigh);

  ifp = neigh->interface;

  if (PIM_DEBUG_PIM_TRACE) {
    char src_str[100];
    pim_inet4_dump("<src?>", neigh->source_addr, src_str, sizeof(src_str));
    zlog_debug("Expired %d sec holdtime for neighbor %s on interface %s",
	       neigh->holdtime, src_str, ifp->name);
  }

  neigh->t_expire_timer = 0;

  snprintf(msg, sizeof(msg), "%d-sec holdtime expired", neigh->holdtime);
  pim_neighbor_delete(ifp, neigh, msg);

  /*
    RFC 4601: 4.3.2.  DR Election
    
    A router's idea of the current DR on an interface can change when a
    PIM Hello message is received, when a neighbor times out, or when a
    router's own DR Priority changes.
  */
  pim_if_dr_election(ifp); // neighbor times out

  return 0;
}

static void neighbor_timer_off(struct pim_neighbor *neigh)
{
  if (PIM_DEBUG_PIM_TRACE) {
    if (neigh->t_expire_timer) {
      char src_str[100];
      pim_inet4_dump("<src?>", neigh->source_addr, src_str, sizeof(src_str));
      zlog_debug("%s: cancelling timer for neighbor %s on %s",
		 __PRETTY_FUNCTION__,
		 src_str, neigh->interface->name);
    }
  }
  THREAD_OFF(neigh->t_expire_timer);
  zassert(!neigh->t_expire_timer);
}

void pim_neighbor_timer_reset(struct pim_neighbor *neigh, uint16_t holdtime)
{
  neigh->holdtime = holdtime;

  neighbor_timer_off(neigh);

  /*
    0xFFFF is request for no holdtime
   */
  if (neigh->holdtime == 0xFFFF) {
    return;
  }

  if (PIM_DEBUG_PIM_TRACE) {
    char src_str[100];
    pim_inet4_dump("<src?>", neigh->source_addr, src_str, sizeof(src_str));
    zlog_debug("%s: starting %u sec timer for neighbor %s on %s",
	       __PRETTY_FUNCTION__,
	       neigh->holdtime, src_str, neigh->interface->name);
  }

  THREAD_TIMER_ON(master, neigh->t_expire_timer,
		  on_neighbor_timer,
		  neigh, neigh->holdtime);
}

static struct pim_neighbor *pim_neighbor_new(struct interface *ifp,
					     struct in_addr source_addr,
					     pim_hello_options hello_options,
					     uint16_t holdtime,
					     uint16_t propagation_delay,
					     uint16_t override_interval,
					     uint32_t dr_priority,
					     uint32_t generation_id,
					     struct list *addr_list)
{
  struct pim_interface *pim_ifp;
  struct pim_neighbor *neigh;
  char src_str[100];

  zassert(ifp);
  pim_ifp = ifp->info;
  zassert(pim_ifp);

  neigh = XMALLOC(MTYPE_PIM_NEIGHBOR, sizeof(*neigh));
  if (!neigh) {
    zlog_err("%s: PIM XMALLOC(%zu) failure",
	     __PRETTY_FUNCTION__, sizeof(*neigh));
    return 0;
  }

  neigh->creation               = pim_time_monotonic_sec();
  neigh->source_addr            = source_addr;
  neigh->hello_options          = hello_options;
  neigh->propagation_delay_msec = propagation_delay;
  neigh->override_interval_msec = override_interval;
  neigh->dr_priority            = dr_priority;
  neigh->generation_id          = generation_id;
  neigh->prefix_list            = addr_list;
  neigh->t_expire_timer         = 0;
  neigh->interface              = ifp;

  pim_neighbor_timer_reset(neigh, holdtime);

  pim_inet4_dump("<src?>", source_addr, src_str, sizeof(src_str));

  if (PIM_DEBUG_PIM_EVENTS) {
    zlog_debug("%s: creating PIM neighbor %s on interface %s",
	       __PRETTY_FUNCTION__,
	       src_str, ifp->name);
  }

  zlog_info("PIM NEIGHBOR UP: neighbor %s on interface %s",
	    src_str, ifp->name);

  if (neigh->propagation_delay_msec > pim_ifp->pim_neighbors_highest_propagation_delay_msec) {
    pim_ifp->pim_neighbors_highest_propagation_delay_msec = neigh->propagation_delay_msec;
  }
  if (neigh->override_interval_msec > pim_ifp->pim_neighbors_highest_override_interval_msec) {
    pim_ifp->pim_neighbors_highest_override_interval_msec = neigh->override_interval_msec;
  }

  if (!PIM_OPTION_IS_SET(neigh->hello_options,
			 PIM_OPTION_MASK_LAN_PRUNE_DELAY)) {
    /* update num. of neighbors without hello option lan_delay */
    ++pim_ifp->pim_number_of_nonlandelay_neighbors; 
  }

  if (!PIM_OPTION_IS_SET(neigh->hello_options,
			 PIM_OPTION_MASK_DR_PRIORITY)) {
    /* update num. of neighbors without hello option dr_pri */
    ++pim_ifp->pim_dr_num_nondrpri_neighbors; 
  }

  /*
    RFC 4601: 4.3.2.  DR Election
    
    A router's idea of the current DR on an interface can change when a
    PIM Hello message is received, when a neighbor times out, or when a
    router's own DR Priority changes.
  */
  pim_if_dr_election(neigh->interface); // new neighbor -- should not trigger dr election...

  /*
    RFC 4601: 4.3.1.  Sending Hello Messages

    To allow new or rebooting routers to learn of PIM neighbors quickly,
    when a Hello message is received from a new neighbor, or a Hello
    message with a new GenID is received from an existing neighbor, a
    new Hello message should be sent on this interface after a
    randomized delay between 0 and Triggered_Hello_Delay.
  */
  pim_hello_restart_triggered(neigh->interface);
  
  return neigh;
}

static void delete_prefix_list(struct pim_neighbor *neigh)
{
  if (neigh->prefix_list) {

#ifdef DUMP_PREFIX_LIST
    struct listnode *p_node;
    struct prefix *p;
    char addr_str[10];
    int list_size = neigh->prefix_list ? (int) listcount(neigh->prefix_list) : -1;
    int i = 0;
    for (ALL_LIST_ELEMENTS_RO(neigh->prefix_list, p_node, p)) {
      pim_inet4_dump("<addr?>", p->u.prefix4, addr_str, sizeof(addr_str));
      zlog_debug("%s: DUMP_PREFIX_LIST neigh=%x prefix_list=%x prefix=%x addr=%s [%d/%d]",
		 __PRETTY_FUNCTION__,
		 (unsigned) neigh, (unsigned) neigh->prefix_list, (unsigned) p,
		 addr_str, i, list_size);
      ++i;
    }
#endif

    list_delete(neigh->prefix_list);
    neigh->prefix_list = 0;
  }
}

void pim_neighbor_free(struct pim_neighbor *neigh)
{
  zassert(!neigh->t_expire_timer);

  delete_prefix_list(neigh);

  XFREE(MTYPE_PIM_NEIGHBOR, neigh);
}

struct pim_neighbor *pim_neighbor_find(struct interface *ifp,
				       struct in_addr source_addr)
{
  struct pim_interface *pim_ifp;
  struct listnode      *node;
  struct pim_neighbor  *neigh;

  pim_ifp = ifp->info;
  zassert(pim_ifp);

  for (ALL_LIST_ELEMENTS_RO(pim_ifp->pim_neighbor_list, node, neigh)) {
    if (source_addr.s_addr == neigh->source_addr.s_addr) {
      return neigh;
    }
  }

  return 0;
}

struct pim_neighbor *pim_neighbor_add(struct interface *ifp,
				      struct in_addr source_addr,
				      pim_hello_options hello_options,
				      uint16_t holdtime,
				      uint16_t propagation_delay,
				      uint16_t override_interval,
				      uint32_t dr_priority,
				      uint32_t generation_id,
				      struct list *addr_list)
{
  struct pim_interface *pim_ifp;
  struct pim_neighbor *neigh;

  neigh = pim_neighbor_new(ifp, source_addr,
			   hello_options,
			   holdtime,
			   propagation_delay,
			   override_interval,
			   dr_priority,
			   generation_id,
			   addr_list);
  if (!neigh) {
    return 0;
  }

  pim_ifp = ifp->info;
  zassert(pim_ifp);

  listnode_add(pim_ifp->pim_neighbor_list, neigh);

  return neigh;
}

static uint16_t
find_neighbors_next_highest_propagation_delay_msec(struct interface *ifp,
						   struct pim_neighbor *highest_neigh)
{
  struct pim_interface *pim_ifp;
  struct listnode *neigh_node;
  struct pim_neighbor *neigh;
  uint16_t next_highest_delay_msec;

  pim_ifp = ifp->info;
  zassert(pim_ifp);

  next_highest_delay_msec = pim_ifp->pim_propagation_delay_msec;

  for (ALL_LIST_ELEMENTS_RO(pim_ifp->pim_neighbor_list, neigh_node, neigh)) {
    if (neigh == highest_neigh)
      continue;
    if (neigh->propagation_delay_msec > next_highest_delay_msec)
      next_highest_delay_msec = neigh->propagation_delay_msec;
  }

  return next_highest_delay_msec;
}

static uint16_t
find_neighbors_next_highest_override_interval_msec(struct interface *ifp,
						   struct pim_neighbor *highest_neigh)
{
  struct pim_interface *pim_ifp;
  struct listnode *neigh_node;
  struct pim_neighbor *neigh;
  uint16_t next_highest_interval_msec;

  pim_ifp = ifp->info;
  zassert(pim_ifp);

  next_highest_interval_msec = pim_ifp->pim_override_interval_msec;

  for (ALL_LIST_ELEMENTS_RO(pim_ifp->pim_neighbor_list, neigh_node, neigh)) {
    if (neigh == highest_neigh)
      continue;
    if (neigh->override_interval_msec > next_highest_interval_msec)
      next_highest_interval_msec = neigh->override_interval_msec;
  }

  return next_highest_interval_msec;
}

void pim_neighbor_delete(struct interface *ifp,
			 struct pim_neighbor *neigh,
			 const char *delete_message)
{
  struct pim_interface *pim_ifp;
  char src_str[100];

  pim_ifp = ifp->info;
  zassert(pim_ifp);

  pim_inet4_dump("<src?>", neigh->source_addr, src_str, sizeof(src_str));
  zlog_info("PIM NEIGHBOR DOWN: neighbor %s on interface %s: %s",
	    src_str, ifp->name, delete_message);

  neighbor_timer_off(neigh);

  pim_if_assert_on_neighbor_down(ifp, neigh->source_addr);

  if (!PIM_OPTION_IS_SET(neigh->hello_options,
                         PIM_OPTION_MASK_LAN_PRUNE_DELAY)) {
    /* update num. of neighbors without hello option lan_delay */

    --pim_ifp->pim_number_of_nonlandelay_neighbors;
  }

  if (!PIM_OPTION_IS_SET(neigh->hello_options,
			 PIM_OPTION_MASK_DR_PRIORITY)) {
    /* update num. of neighbors without dr_pri */

    --pim_ifp->pim_dr_num_nondrpri_neighbors; 
  }

  zassert(neigh->propagation_delay_msec <= pim_ifp->pim_neighbors_highest_propagation_delay_msec);
  zassert(neigh->override_interval_msec <= pim_ifp->pim_neighbors_highest_override_interval_msec);

  if (pim_if_lan_delay_enabled(ifp)) {

    /* will delete a neighbor with highest propagation delay? */
    if (neigh->propagation_delay_msec == pim_ifp->pim_neighbors_highest_propagation_delay_msec) {
      /* then find the next highest propagation delay */
      pim_ifp->pim_neighbors_highest_propagation_delay_msec =
	find_neighbors_next_highest_propagation_delay_msec(ifp, neigh);
    }

    /* will delete a neighbor with highest override interval? */
    if (neigh->override_interval_msec == pim_ifp->pim_neighbors_highest_override_interval_msec) {
      /* then find the next highest propagation delay */
      pim_ifp->pim_neighbors_highest_override_interval_msec =
	find_neighbors_next_highest_override_interval_msec(ifp, neigh);
    }
  }

  if (PIM_DEBUG_PIM_TRACE) {
    zlog_debug("%s: deleting PIM neighbor %s on interface %s",
	       __PRETTY_FUNCTION__,
	       src_str, ifp->name);
  }

  listnode_delete(pim_ifp->pim_neighbor_list, neigh);

  pim_neighbor_free(neigh);
}

void pim_neighbor_delete_all(struct interface *ifp,
			     const char *delete_message)
{
  struct pim_interface *pim_ifp;
  struct listnode *neigh_node;
  struct listnode *neigh_nextnode;
  struct pim_neighbor *neigh;

  pim_ifp = ifp->info;
  zassert(pim_ifp);

  for (ALL_LIST_ELEMENTS(pim_ifp->pim_neighbor_list, neigh_node,
			 neigh_nextnode, neigh)) {
    pim_neighbor_delete(ifp, neigh, delete_message);
  }
}

struct prefix *pim_neighbor_find_secondary(struct pim_neighbor *neigh,
					   struct in_addr addr)
{
  struct listnode *node;
  struct prefix   *p;

  if (!neigh->prefix_list)
    return 0;

  for (ALL_LIST_ELEMENTS_RO(neigh->prefix_list, node, p)) {
    if (p->family == AF_INET) {
      if (addr.s_addr == p->u.prefix4.s_addr) {
	return p;
      }
    }
  }

  return 0;
}

/*
  RFC 4601: 4.3.4.  Maintaining Secondary Address Lists
  
  All the advertised secondary addresses in received Hello messages
  must be checked against those previously advertised by all other
  PIM neighbors on that interface.  If there is a conflict and the
  same secondary address was previously advertised by another
  neighbor, then only the most recently received mapping MUST be
  maintained, and an error message SHOULD be logged to the
  administrator in a rate-limited manner.
*/
static void delete_from_neigh_addr(struct interface *ifp,
				   struct list *addr_list,
				   struct in_addr neigh_addr)
{
  struct listnode      *addr_node;
  struct prefix        *addr;
  struct pim_interface *pim_ifp;

  pim_ifp = ifp->info;
  zassert(pim_ifp);

  zassert(addr_list);

  /*
    Scan secondary address list
  */
  for (ALL_LIST_ELEMENTS_RO(addr_list, addr_node,
			    addr)) {
    struct listnode      *neigh_node;
    struct pim_neighbor  *neigh;

    if (addr->family != AF_INET)
      continue;

    /*
      Scan neighbors
    */
    for (ALL_LIST_ELEMENTS_RO(pim_ifp->pim_neighbor_list, neigh_node,
			      neigh)) {
      {
	struct prefix *p = pim_neighbor_find_secondary(neigh, addr->u.prefix4);
	if (p) {
	  char addr_str[100];
	  char this_neigh_str[100];
	  char other_neigh_str[100];
	  
	  pim_inet4_dump("<addr?>", addr->u.prefix4, addr_str, sizeof(addr_str));
	  pim_inet4_dump("<neigh1?>", neigh_addr, this_neigh_str, sizeof(this_neigh_str));
	  pim_inet4_dump("<neigh2?>", neigh->source_addr, other_neigh_str, sizeof(other_neigh_str));
	  
	  zlog_info("secondary addr %s recvd from neigh %s deleted from neigh %s on %s",
		    addr_str, this_neigh_str, other_neigh_str, ifp->name);
	  
	  listnode_delete(neigh->prefix_list, p);
	  prefix_free(p);
	}
      }

    } /* scan neighbors */
    
  } /* scan addr list */

}

void pim_neighbor_update(struct pim_neighbor *neigh,
			 pim_hello_options hello_options,
			 uint16_t holdtime,
			 uint32_t dr_priority,
			 struct list *addr_list)
{
  struct pim_interface *pim_ifp = neigh->interface->info;

  /* Received holdtime ? */
  if (PIM_OPTION_IS_SET(hello_options, PIM_OPTION_MASK_HOLDTIME)) {
    pim_neighbor_timer_reset(neigh, holdtime);
  }
  else {
    pim_neighbor_timer_reset(neigh, PIM_IF_DEFAULT_HOLDTIME(pim_ifp));
  }

#ifdef DUMP_PREFIX_LIST
  zlog_debug("%s: DUMP_PREFIX_LIST old_prefix_list=%x old_size=%d new_prefix_list=%x new_size=%d",
	     __PRETTY_FUNCTION__,
	     (unsigned) neigh->prefix_list,
	     neigh->prefix_list ? (int) listcount(neigh->prefix_list) : -1,
	     (unsigned) addr_list,
	     addr_list ? (int) listcount(addr_list) : -1);
#endif

  if (neigh->prefix_list == addr_list) {
    if (addr_list) {
      zlog_err("%s: internal error: trying to replace same prefix list=%p",
	       __PRETTY_FUNCTION__, (void *) addr_list);
    }
  }
  else {
    /* Delete existing secondary address list */
    delete_prefix_list(neigh);
  }

  if (addr_list) {
    delete_from_neigh_addr(neigh->interface, addr_list, neigh->source_addr);
  }

  /* Replace secondary address list */
  neigh->prefix_list = addr_list;

  update_dr_priority(neigh,
		     hello_options,
		     dr_priority);
  /*
    Copy flags
   */
  neigh->hello_options = hello_options;
}
