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

#ifndef PIM_NEIGHBOR_H
#define PIM_NEIGHBOR_H

#include <zebra.h>

#include "if.h"
#include "linklist.h"

#include "pim_tlv.h"

struct pim_neighbor {
  int64_t            creation; /* timestamp of creation */
  struct in_addr     source_addr;
  pim_hello_options  hello_options;
  uint16_t           holdtime;
  uint16_t           propagation_delay_msec;
  uint16_t           override_interval_msec;
  uint32_t           dr_priority;
  uint32_t           generation_id;
  struct list       *prefix_list; /* list of struct prefix */
  struct thread     *t_expire_timer;
  struct interface  *interface;
};

void pim_neighbor_timer_reset(struct pim_neighbor *neigh, uint16_t holdtime);
void pim_neighbor_free(struct pim_neighbor *neigh);
struct pim_neighbor *pim_neighbor_find(struct interface *ifp,
				       struct in_addr source_addr);
struct pim_neighbor *pim_neighbor_add(struct interface *ifp,
				      struct in_addr source_addr,
				      pim_hello_options hello_options,
				      uint16_t holdtime,
				      uint16_t propagation_delay,
				      uint16_t override_interval,
				      uint32_t dr_priority,
				      uint32_t generation_id,
				      struct list *addr_list);
void pim_neighbor_delete(struct interface *ifp,
			 struct pim_neighbor *neigh,
			 const char *delete_message);
void pim_neighbor_delete_all(struct interface *ifp,
			     const char *delete_message);
void pim_neighbor_update(struct pim_neighbor *neigh,
			 pim_hello_options hello_options,
			 uint16_t holdtime,
			 uint32_t dr_priority,
			 struct list *addr_list);
struct prefix *pim_neighbor_find_secondary(struct pim_neighbor *neigh,
					   struct in_addr addr);
void pim_if_dr_election(struct interface *ifp);

#endif /* PIM_NEIGHBOR_H */
