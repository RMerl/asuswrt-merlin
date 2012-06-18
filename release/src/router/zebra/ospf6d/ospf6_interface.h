/*
 * Copyright (C) 2003 Yasuhiro Ohara
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

#ifndef OSPF6_INTERFACE_H
#define OSPF6_INTERFACE_H

#include "if.h"

/* Debug option */
extern unsigned char conf_debug_ospf6_interface;
#define OSPF6_DEBUG_INTERFACE_ON() \
  (conf_debug_ospf6_interface = 1)
#define OSPF6_DEBUG_INTERFACE_OFF() \
  (conf_debug_ospf6_interface = 0)
#define IS_OSPF6_DEBUG_INTERFACE \
  (conf_debug_ospf6_interface)

/* Interface structure */
struct ospf6_interface
{
  /* IF info from zebra */
  struct interface *interface;

  /* back pointer */
  struct ospf6_area *area;

  /* list of ospf6 neighbor */
  list neighbor_list;

  /* linklocal address of this I/F */
  struct in6_addr *linklocal_addr;

  /* Interface ID; use interface->ifindex */

  /* ospf6 instance id */
  u_char instance_id;

  /* I/F transmission delay */
  u_int32_t transdelay;

  /* Router Priority */
  u_char priority;

  /* Time Interval */
  u_int16_t hello_interval;
  u_int16_t dead_interval;
  u_int32_t rxmt_interval;

  /* Cost */
  u_int32_t cost;

  /* I/F MTU */
  u_int32_t ifmtu;

  /* Interface State */
  u_char state;

  /* OSPF6 Interface flag */
  char flag;

  /* Decision of DR Election */
  u_int32_t drouter;
  u_int32_t bdrouter;
  u_int32_t prev_drouter;
  u_int32_t prev_bdrouter;

  /* Linklocal LSA Database: includes Link-LSA */
  struct ospf6_lsdb *lsdb;
  struct ospf6_lsdb *lsdb_self;

  struct ospf6_lsdb *lsupdate_list;
  struct ospf6_lsdb *lsack_list;

  /* Ongoing Tasks */
  struct thread *thread_send_hello;
  struct thread *thread_send_lsupdate;
  struct thread *thread_send_lsack;

  struct thread *thread_network_lsa;
  struct thread *thread_link_lsa;
  struct thread *thread_intra_prefix_lsa;

  struct ospf6_route_table *route_connected;

  /* prefix-list name to filter connected prefix */
  char *plist_name;
};

/* interface state */
#define OSPF6_INTERFACE_NONE             0
#define OSPF6_INTERFACE_DOWN             1
#define OSPF6_INTERFACE_LOOPBACK         2
#define OSPF6_INTERFACE_WAITING          3
#define OSPF6_INTERFACE_POINTTOPOINT     4
#define OSPF6_INTERFACE_DROTHER          5
#define OSPF6_INTERFACE_BDR              6
#define OSPF6_INTERFACE_DR               7
#define OSPF6_INTERFACE_MAX              8

extern char *ospf6_interface_state_str[];

/* flags */
#define OSPF6_INTERFACE_DISABLE      0x01
#define OSPF6_INTERFACE_PASSIVE      0x02


/* Function Prototypes */

struct ospf6_interface *ospf6_interface_lookup_by_ifindex (int);
struct ospf6_interface *ospf6_interface_lookup_by_name (char *);
struct ospf6_interface *ospf6_interface_create (struct interface *);
void ospf6_interface_delete (struct ospf6_interface *);

void ospf6_interface_enable (struct ospf6_interface *);
void ospf6_interface_disable (struct ospf6_interface *);

void ospf6_interface_if_add (struct interface *);
void ospf6_interface_if_del (struct interface *);
void ospf6_interface_state_update (struct interface *);
void ospf6_interface_connected_route_update (struct interface *);

/* interface event */
int interface_up (struct thread *);
int interface_down (struct thread *);
int wait_timer (struct thread *);
int backup_seen (struct thread *);
int neighbor_change (struct thread *);

void ospf6_interface_init ();

int config_write_ospf6_debug_interface (struct vty *vty);
void install_element_ospf6_debug_interface ();

#endif /* OSPF6_INTERFACE_H */

