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
  u_int32_t state_change;
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

  struct ospf6_lsa *last_ls_req;

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

/* Neighbor Events */
#define OSPF6_NEIGHBOR_EVENT_NO_EVENT             0
#define OSPF6_NEIGHBOR_EVENT_HELLO_RCVD           1
#define OSPF6_NEIGHBOR_EVENT_TWOWAY_RCVD          2
#define OSPF6_NEIGHBOR_EVENT_NEGOTIATION_DONE     3
#define OSPF6_NEIGHBOR_EVENT_EXCHANGE_DONE        4
#define OSPF6_NEIGHBOR_EVENT_LOADING_DONE         5
#define OSPF6_NEIGHBOR_EVENT_ADJ_OK               6
#define OSPF6_NEIGHBOR_EVENT_SEQNUMBER_MISMATCH   7
#define OSPF6_NEIGHBOR_EVENT_BAD_LSREQ            8
#define OSPF6_NEIGHBOR_EVENT_ONEWAY_RCVD          9
#define OSPF6_NEIGHBOR_EVENT_INACTIVITY_TIMER    10
#define OSPF6_NEIGHBOR_EVENT_MAX_EVENT           11

extern const char *ospf6_neighbor_state_str[];

/* Function Prototypes */
int ospf6_neighbor_cmp (void *va, void *vb);
void ospf6_neighbor_dbex_init (struct ospf6_neighbor *on);

struct ospf6_neighbor *ospf6_neighbor_lookup (u_int32_t,
                                              struct ospf6_interface *);
struct ospf6_neighbor *ospf6_neighbor_create (u_int32_t,
                                              struct ospf6_interface *);
void ospf6_neighbor_delete (struct ospf6_neighbor *);

/* Neighbor event */
extern int hello_received (struct thread *);
extern int twoway_received (struct thread *);
extern int negotiation_done (struct thread *);
extern int exchange_done (struct thread *);
extern int loading_done (struct thread *);
extern int adj_ok (struct thread *);
extern int seqnumber_mismatch (struct thread *);
extern int bad_lsreq (struct thread *);
extern int oneway_received (struct thread *);
extern int inactivity_timer (struct thread *);
extern void ospf6_check_nbr_loading (struct ospf6_neighbor *);

extern void ospf6_neighbor_init (void);
extern int config_write_ospf6_debug_neighbor (struct vty *vty);
extern void install_element_ospf6_debug_neighbor (void);

#endif /* OSPF6_NEIGHBOR_H */
