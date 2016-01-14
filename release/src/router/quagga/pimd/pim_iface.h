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

#ifndef PIM_IFACE_H
#define PIM_IFACE_H

#include <zebra.h>

#include "if.h"
#include "vty.h"

#include "pim_igmp.h"
#include "pim_upstream.h"

#define PIM_IF_MASK_PIM                             (1 << 0)
#define PIM_IF_MASK_IGMP                            (1 << 1)
#define PIM_IF_MASK_IGMP_LISTEN_ALLROUTERS          (1 << 2)
#define PIM_IF_MASK_PIM_CAN_DISABLE_JOIN_SUPRESSION (1 << 3)

#define PIM_IF_IS_DELETED(ifp) ((ifp)->ifindex == IFINDEX_INTERNAL)

#define PIM_IF_TEST_PIM(options) (PIM_IF_MASK_PIM & (options))
#define PIM_IF_TEST_IGMP(options) (PIM_IF_MASK_IGMP & (options))
#define PIM_IF_TEST_IGMP_LISTEN_ALLROUTERS(options) (PIM_IF_MASK_IGMP_LISTEN_ALLROUTERS & (options))
#define PIM_IF_TEST_PIM_CAN_DISABLE_JOIN_SUPRESSION(options) (PIM_IF_MASK_PIM_CAN_DISABLE_JOIN_SUPRESSION & (options))

#define PIM_IF_DO_PIM(options) ((options) |= PIM_IF_MASK_PIM)
#define PIM_IF_DO_IGMP(options) ((options) |= PIM_IF_MASK_IGMP)
#define PIM_IF_DO_IGMP_LISTEN_ALLROUTERS(options) ((options) |= PIM_IF_MASK_IGMP_LISTEN_ALLROUTERS)
#define PIM_IF_DO_PIM_CAN_DISABLE_JOIN_SUPRESSION(options) ((options) |= PIM_IF_MASK_PIM_CAN_DISABLE_JOIN_SUPRESSION)

#define PIM_IF_DONT_PIM(options) ((options) &= ~PIM_IF_MASK_PIM)
#define PIM_IF_DONT_IGMP(options) ((options) &= ~PIM_IF_MASK_IGMP)
#define PIM_IF_DONT_IGMP_LISTEN_ALLROUTERS(options) ((options) &= ~PIM_IF_MASK_IGMP_LISTEN_ALLROUTERS)
#define PIM_IF_DONT_PIM_CAN_DISABLE_JOIN_SUPRESSION(options) ((options) &= ~PIM_IF_MASK_PIM_CAN_DISABLE_JOIN_SUPRESSION)

struct pim_interface {
  uint32_t       options;                            /* bit vector */
  int            mroute_vif_index;
  struct in_addr primary_address; /* remember addr to detect change */

  int          igmp_default_robustness_variable;            /* IGMPv3 QRV */
  int          igmp_default_query_interval;                 /* IGMPv3 secs between general queries */
  int          igmp_query_max_response_time_dsec;           /* IGMPv3 Max Response Time in dsecs for general queries */
  int          igmp_specific_query_max_response_time_dsec;  /* IGMPv3 Max Response Time in dsecs for specific queries */
  struct list *igmp_socket_list;                            /* list of struct igmp_sock */
  struct list *igmp_join_list;                              /* list of struct igmp_join */

  int            pim_sock_fd;       /* PIM socket file descriptor */
  struct thread *t_pim_sock_read;   /* thread for reading PIM socket */
  int64_t        pim_sock_creation; /* timestamp of PIM socket creation */

  struct thread *t_pim_hello_timer;
  int            pim_hello_period;
  int            pim_default_holdtime;
  int            pim_triggered_hello_delay;
  uint32_t       pim_generation_id;
  uint16_t       pim_propagation_delay_msec; /* config */
  uint16_t       pim_override_interval_msec; /* config */
  struct list   *pim_neighbor_list; /* list of struct pim_neighbor */
  struct list   *pim_ifchannel_list; /* list of struct pim_ifchannel */

  /* neighbors without lan_delay */
  int            pim_number_of_nonlandelay_neighbors;
  uint16_t       pim_neighbors_highest_propagation_delay_msec;
  uint16_t       pim_neighbors_highest_override_interval_msec;

  /* DR Election */
  int64_t        pim_dr_election_last; /* timestamp */
  int            pim_dr_election_count;
  int            pim_dr_election_changes;
  struct in_addr pim_dr_addr;
  uint32_t       pim_dr_priority;            /* config */
  int            pim_dr_num_nondrpri_neighbors; /* neighbors without dr_pri */

  int64_t        pim_ifstat_start; /* start timestamp for stats */
  uint32_t       pim_ifstat_hello_sent;
  uint32_t       pim_ifstat_hello_sendfail;
  uint32_t       pim_ifstat_hello_recv;
  uint32_t       pim_ifstat_hello_recvfail;
};

/*
  if default_holdtime is set (>= 0), use it;
  otherwise default_holdtime is 3.5 * hello_period
 */
#define PIM_IF_DEFAULT_HOLDTIME(pim_ifp) \
  (((pim_ifp)->pim_default_holdtime < 0) ? \
  ((pim_ifp)->pim_hello_period * 7 / 2) : \
  ((pim_ifp)->pim_default_holdtime))

void pim_if_init(void);

struct pim_interface *pim_if_new(struct interface *ifp, int igmp, int pim);
void                  pim_if_delete(struct interface *ifp);
void pim_if_addr_add(struct connected *ifc);
void pim_if_addr_del(struct connected *ifc, int force_prim_as_any);
void pim_if_addr_add_all(struct interface *ifp);
void pim_if_addr_del_all(struct interface *ifp);
void pim_if_addr_del_all_igmp(struct interface *ifp);
void pim_if_addr_del_all_pim(struct interface *ifp);

int pim_if_add_vif(struct interface *ifp);
int pim_if_del_vif(struct interface *ifp);
void pim_if_add_vif_all(void);
void pim_if_del_vif_all(void);

struct interface *pim_if_find_by_vif_index(int vif_index);
int pim_if_find_vifindex_by_ifindex(int ifindex);

int pim_if_lan_delay_enabled(struct interface *ifp);
uint16_t pim_if_effective_propagation_delay_msec(struct interface *ifp);
uint16_t pim_if_effective_override_interval_msec(struct interface *ifp);
uint16_t pim_if_jp_override_interval_msec(struct interface *ifp);
struct pim_neighbor *pim_if_find_neighbor(struct interface *ifp,
					  struct in_addr addr);

long pim_if_t_suppressed_msec(struct interface *ifp);
int pim_if_t_override_msec(struct interface *ifp);

struct in_addr pim_find_primary_addr(struct interface *ifp);

int pim_if_igmp_join_add(struct interface *ifp,
			 struct in_addr group_addr,
			 struct in_addr source_addr);
int pim_if_igmp_join_del(struct interface *ifp,
			 struct in_addr group_addr,
			 struct in_addr source_addr);

void pim_if_update_could_assert(struct interface *ifp);

void pim_if_assert_on_neighbor_down(struct interface *ifp,
				    struct in_addr neigh_addr);

void pim_if_rpf_interface_changed(struct interface *old_rpf_ifp,
				  struct pim_upstream *up);

void pim_if_update_join_desired(struct pim_interface *pim_ifp);

void pim_if_update_assert_tracking_desired(struct interface *ifp);

#endif /* PIM_IFACE_H */
