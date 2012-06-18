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

#ifndef OSPF6_NEIGHBOR_H
#define OSPF6_NEIGHBOR_H

/* Debug option */
extern unsigned char conf_debug_ospf6_neighbor;
#define OSPF6_DEBUG_NEIGHBOR_STATE   0x01
#define OSPF6_DEBUG_NEIGHBOR_EVENT   0x02
#define OSPF6_DEBUG_NEIGHBOR_ON(level) \
  (conf_debug_ospf6_neighbor |= (level))
#define OSPF6_DEBUG_NEIGHBOR_OFF(level) \
  (conf_debug_ospf6_neighbor &= ~(level))
#define IS_OSPF6_DEBUG_NEIGHBOR(level) \
  (conf_debug_ospf6_neighbor & OSPF6_DEBUG_NEIGHBOR_ ## level)

/* Neighbor structure */
struct ospf6_neighbor
{
  /* Neighbor Router ID String */
  char name[32];

  /* OSPFv3 Interface this neighbor belongs to */
  struct ospf6_interface *ospf6_if;

  /* Neighbor state */
  u_char state;

  /* timestamp of last changing state */
  struct timeval last_changed;

  /* Neighbor Router ID */
  u_int32_t router_id;

  /* Neighbor Interface ID */
  u_int32_t ifindex;

  /* Router Priority of this neighbor */
  u_char priority;

  u_int32_t drouter;
  u_int32_t bdrouter;
  u_int32_t prev_drouter;
  u_int32_t prev_bdrouter;

  /* Options field (Capability) */
  char options[3];

  /* IPaddr of I/F on our side link */
  struct in6_addr linklocal_addr;

  /* For Database Exchange */
  u_char               dbdesc_bits;
  u_int32_t            dbdesc_seqnum;
  /* Last received Database Description packet */
  struct ospf6_dbdesc  dbdesc_last;

  /* LS-list */
  struct ospf6_lsdb *summary_list;
  struct ospf6_lsdb *request_list;
  struct ospf6_lsdb *retrans_list;

  /* LSA list for message transmission */
  struct ospf6_lsdb *dbdesc_list;
  struct ospf6_lsdb *lsreq_list;
  struct ospf6_lsdb *lsupdate_list;
  struct ospf6_lsdb *lsack_list;

  /* Inactivity timer */
  struct thread *inactivity_timer;

  /* Thread for sending message */
  struct thread *thread_send_dbdesc;
  struct thread *thread_send_lsreq;
  struct thread *thread_send_lsupdate;
  struct thread *thread_send_lsack;
};

/* Neighbor state */
#define OSPF6_NEIGHBOR_DOWN     1
#define OSPF6_NEIGHBOR_ATTEMPT  2
#define OSPF6_NEIGHBOR_INIT     3
#define OSPF6_NEIGHBOR_TWOWAY   4
#define OSPF6_NEIGHBOR_EXSTART  5
#define OSPF6_NEIGHBOR_EXCHANGE 6
#define OSPF6_NEIGHBOR_LOADING  7
#define OSPF6_NEIGHBOR_FULL     8

extern char *ospf6_neighbor_state_str[];


/* Function Prototypes */
int ospf6_neighbor_cmp (void *va, void *vb);
void ospf6_neighbor_dbex_init (struct ospf6_neighbor *on);

struct ospf6_neighbor *ospf6_neighbor_lookup (u_int32_t,
                                              struct ospf6_interface *);
struct ospf6_neighbor *ospf6_neighbor_create (u_int32_t,
                                              struct ospf6_interface *);
void ospf6_neighbor_delete (struct ospf6_neighbor *);

/* Neighbor event */
int hello_received (struct thread *);
int twoway_received (struct thread *);
int negotiation_done (struct thread *);
int exchange_done (struct thread *);
int loading_done (struct thread *);
int adj_ok (struct thread *);
int seqnumber_mismatch (struct thread *);
int bad_lsreq (struct thread *);
int oneway_received (struct thread *);
int inactivity_timer (struct thread *);

void ospf6_neighbor_init ();
int config_write_ospf6_debug_neighbor (struct vty *vty);
void install_element_ospf6_debug_neighbor ();

#endif /* OSPF6_NEIGHBOR_H */

