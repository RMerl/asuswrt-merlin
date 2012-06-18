/*
 * OSPF Neighbor functions.
 * Copyright (C) 1999, 2000 Toshiaki Takada
 *
 * This file is part of GNU Zebra.
 * 
 * GNU Zebra is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
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

#ifndef _ZEBRA_OSPF_NEIGHBOR_H
#define _ZEBRA_OSPF_NEIGHBOR_H

/* Neighbor Data Structure */
struct ospf_neighbor
{
  /* This neighbor's parent ospf interface. */
  struct ospf_interface *oi;

  /* OSPF neighbor Information */
  u_char state;				/* NSM status. */
  u_char dd_flags;			/* DD bit flags. */
  u_int32_t dd_seqnum;			/* DD Sequence Number. */

  /* Neighbor Information from Hello. */
  struct prefix address;		/* Neighbor Interface Address. */

  struct in_addr src;			/* Src address. */
  struct in_addr router_id;		/* Router ID. */
  u_char options;			/* Options. */
  int priority;				/* Router Priority. */
  struct in_addr d_router;		/* Designated Router. */
  struct in_addr bd_router;		/* Backup Designated Router. */

  /* Last sent Database Description packet. */
  struct ospf_packet *last_send;
  /* Timestemp when last Database Description packet was sent */
  struct timeval last_send_ts;

  /* Last received Databse Description packet. */
  struct
  {
    u_char options;
    u_char flags;
    u_int32_t dd_seqnum;
  } last_recv;

  /* LSA data. */
  struct ospf_lsdb ls_rxmt;
  struct ospf_lsdb db_sum;
  struct ospf_lsdb ls_req;
  struct ospf_lsa *ls_req_last;

  u_int32_t crypt_seqnum;           /* Cryptographic Sequence Number. */

  /* Timer values. */
  u_int32_t v_inactivity;
  u_int32_t v_db_desc;
  u_int32_t v_ls_req;
  u_int32_t v_ls_upd;

  /* Threads. */
  struct thread *t_inactivity;
  struct thread *t_db_desc;
  struct thread *t_ls_req;
  struct thread *t_ls_upd;
  struct thread *t_hello_reply;

  /* Statistics Field */
  u_int32_t state_change;
  struct ospf_nbr_nbma *nbr_nbma;
};

/* Macros. */
#define NBR_IS_DR(n)	IPV4_ADDR_SAME (&n->address.u.prefix4, &n->d_router)
#define NBR_IS_BDR(n)   IPV4_ADDR_SAME (&n->address.u.prefix4, &n->bd_router)

/* Prototypes. */
struct ospf_neighbor *ospf_nbr_new (struct ospf_interface *);
void ospf_nbr_free (struct ospf_neighbor *);
void ospf_nbr_delete (struct ospf_neighbor *);
int ospf_nbr_bidirectional (struct in_addr *, struct in_addr *, int);
void ospf_nbr_add_self (struct ospf_interface *);
int ospf_nbr_count (struct ospf_interface *, int);
#ifdef HAVE_OPAQUE_LSA
int ospf_nbr_count_opaque_capable (struct ospf_interface *);
#endif /* HAVE_OPAQUE_LSA */
struct ospf_neighbor *ospf_nbr_lookup_by_addr (struct route_table *,
					       struct in_addr *);
struct ospf_neighbor *ospf_nbr_lookup_by_routerid (struct route_table *,
						   struct in_addr *);
void ospf_renegotiate_optional_capabilities (struct ospf *top);

#endif /* _ZEBRA_OSPF_NEIGHBOR_H */
