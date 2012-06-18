/*
 * OSPF Interface functions.
 * Copyright (C) 1999 Toshiaki Takada
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

#ifndef _ZEBRA_OSPF_INTERFACE_H
#define _ZEBRA_OSPF_INTERFACE_H

#define OSPF_AUTH_SIMPLE_SIZE           8
#define OSPF_AUTH_MD5_SIZE             16

#define IF_OSPF_IF_INFO(I) ((struct ospf_if_info *)((I)->info))
#define IF_DEF_PARAMS(I) (IF_OSPF_IF_INFO (I)->def_params)
#define IF_OIFS(I)  (IF_OSPF_IF_INFO (I)->oifs)
#define IF_OIFS_PARAMS(I) (IF_OSPF_IF_INFO (I)->params)
			    
#define OSPF_IF_PARAM_CONFIGURED(S, P) ((S) && (S)->P##__config)
#define OSPF_IF_PARAM(O, P) \
        (OSPF_IF_PARAM_CONFIGURED ((O)->params, P)?\
                        (O)->params->P:IF_DEF_PARAMS((O)->ifp)->P)

#define DECLARE_IF_PARAM(T, P) T P; u_char P##__config:1
#define UNSET_IF_PARAM(S, P) ((S)->P##__config) = 0
#define SET_IF_PARAM(S, P) ((S)->P##__config) = 1

struct ospf_if_params
{
  DECLARE_IF_PARAM (u_int32_t, transmit_delay); /* Interface Transmisson Delay */
  DECLARE_IF_PARAM (u_int32_t, output_cost_cmd);/* Command Interface Output Cost */
  DECLARE_IF_PARAM (u_int32_t, retransmit_interval); /* Retransmission Interval */
  DECLARE_IF_PARAM (u_char, passive_interface);      /* OSPF Interface is passive */
  DECLARE_IF_PARAM (u_char, priority);               /* OSPF Interface priority */
  DECLARE_IF_PARAM (u_char, type);                   /* type of interface */
#define OSPF_IF_ACTIVE                  0
#define OSPF_IF_PASSIVE		        1
  
  DECLARE_IF_PARAM (u_int32_t, v_hello);             /* Hello Interval */
  DECLARE_IF_PARAM (u_int32_t, v_wait);              /* Router Dead Interval */

  /* Authentication data. */
  u_char auth_simple[OSPF_AUTH_SIMPLE_SIZE + 1];       /* Simple password. */
  u_char auth_simple__config:1;
  
  DECLARE_IF_PARAM (list, auth_crypt);                 /* List of Auth cryptographic data. */
  DECLARE_IF_PARAM (int, auth_type);               /* OSPF authentication type */
};

struct ospf_if_info
{
  struct ospf_if_params *def_params;
  struct route_table *params;
  struct route_table *oifs;
};

struct ospf_interface;

struct ospf_vl_data
{
  struct in_addr    vl_peer;	   /* Router-ID of the peer for VLs. */
  struct in_addr    vl_area_id;	   /* Transit area for this VL. */
  int format;                      /* area ID format */
  struct ospf_interface *vl_oi;	   /* Interface data structure for the VL. */
  struct ospf_interface *out_oi;   /* The interface to go out. */
  struct in_addr    peer_addr;	   /* Address used to reach the peer. */
  u_char flags;
};


#define OSPF_VL_MAX_COUNT 256
#define OSPF_VL_MTU	  1500

#define OSPF_VL_FLAG_APPROVED 0x01

struct crypt_key
{
  u_char key_id;
  u_char auth_key[OSPF_AUTH_MD5_SIZE + 1];
};

/* OSPF interface structure. */
struct ospf_interface
{
  /* This interface's parent ospf instance. */
  struct ospf *ospf;

  /* OSPF Area. */
  struct ospf_area *area;

  /* Interface data from zebra. */
  struct interface *ifp;
  struct ospf_vl_data *vl_data;		/* Data for Virtual Link */
  
  /* Packet send buffer. */
  struct ospf_fifo *obuf;		/* Output queue */

  /* OSPF Network Type. */
  u_char type;
#define OSPF_IFTYPE_NONE		0
#define OSPF_IFTYPE_POINTOPOINT		1
#define OSPF_IFTYPE_BROADCAST		2
#define OSPF_IFTYPE_NBMA		3
#define OSPF_IFTYPE_POINTOMULTIPOINT	4
#define OSPF_IFTYPE_VIRTUALLINK		5
#define OSPF_IFTYPE_LOOPBACK            6
#define OSPF_IFTYPE_MAX			7

  /* State of Interface State Machine. */
  u_char state;

  struct prefix *address;		/* Interface prefix */
  struct connected *connected;          /* Pointer to connected */ 

  /* Configured varables. */
  struct ospf_if_params *params;
  u_int32_t crypt_seqnum;		/* Cryptographic Sequence Number */ 
  u_int32_t output_cost;	        /* Acutual Interface Output Cost */

  /* Neighbor information. */
  struct route_table *nbrs;             /* OSPF Neighbor List */
  struct ospf_neighbor *nbr_self;	/* Neighbor Self */
#define DR(I)			((I)->nbr_self->d_router)
#define BDR(I)			((I)->nbr_self->bd_router)
#define OPTIONS(I)		((I)->nbr_self->options)
#define PRIORITY(I)		((I)->nbr_self->priority)

  /* List of configured NBMA neighbor. */
  list nbr_nbma;

  /* self-originated LSAs. */
  struct ospf_lsa *network_lsa_self;	/* network-LSA. */
#ifdef HAVE_OPAQUE_LSA
  list opaque_lsa_self;			/* Type-9 Opaque-LSAs */
#endif /* HAVE_OPAQUE_LSA */

  struct route_table *ls_upd_queue;

  list ls_ack;				/* Link State Acknowledgment list. */
  
  struct
  {
    list ls_ack;
    struct in_addr dst;
  } ls_ack_direct;

  /* Timer values. */
  u_int32_t v_ls_ack;			/* Delayed Link State Acknowledgment */

  /* Threads. */
  struct thread *t_hello;               /* timer */
  struct thread *t_wait;                /* timer */
  struct thread *t_ls_ack;              /* timer */
  struct thread *t_ls_ack_direct;       /* event */
  struct thread *t_ls_upd_event;        /* event */
  struct thread *t_network_lsa_self;    /* self-originated network-LSA
                                           reflesh thread. timer */
#ifdef HAVE_OPAQUE_LSA
  struct thread *t_opaque_lsa_self;     /* Type-9 Opaque-LSAs */
#endif /* HAVE_OPAQUE_LSA */

  int on_write_q;
  
  /* Statistics fields. */
  u_int32_t hello_in;	        /* Hello message input count. */
  u_int32_t hello_out;	        /* Hello message output count. */
  u_int32_t db_desc_in;         /* database desc. message input count. */
  u_int32_t db_desc_out;        /* database desc. message output count. */
  u_int32_t ls_req_in;          /* LS request message input count. */
  u_int32_t ls_req_out;         /* LS request message output count. */
  u_int32_t ls_upd_in;          /* LS update message input count. */
  u_int32_t ls_upd_out;         /* LS update message output count. */
  u_int32_t ls_ack_in;          /* LS Ack message input count. */
  u_int32_t ls_ack_out;         /* LS Ack message output count. */
  u_int32_t discarded;		/* discarded input count by error. */
  u_int32_t state_change;	/* Number of status change. */

  u_int32_t full_nbrs;
};

/* Prototypes. */
char *ospf_if_name (struct ospf_interface *);
struct ospf_interface *ospf_if_new (struct ospf *, struct interface *,
				    struct prefix *);
void ospf_if_cleanup (struct ospf_interface *);
void ospf_if_free (struct ospf_interface *);
int ospf_if_up (struct ospf_interface *);
int ospf_if_down (struct ospf_interface *);

int ospf_if_is_up (struct ospf_interface *);
struct ospf_interface *ospf_if_lookup_by_name (char *);
struct ospf_interface *ospf_if_lookup_by_local_addr (struct ospf *,
						     struct interface *,
						     struct in_addr);
struct ospf_interface *ospf_if_lookup_by_prefix (struct ospf *,
						 struct prefix_ipv4 *);
struct ospf_interface *ospf_if_addr_local (struct in_addr);
struct ospf_interface *ospf_if_lookup_recv_if (struct ospf *, struct in_addr);
struct ospf_interface *ospf_if_is_configured (struct ospf *, struct in_addr *);

struct ospf_if_params *ospf_lookup_if_params (struct interface *,
					      struct in_addr);
struct ospf_if_params *ospf_get_if_params (struct interface *, struct in_addr);
void ospf_del_if_params (struct ospf_if_params *);
void ospf_free_if_params (struct interface *, struct in_addr);
void ospf_if_update_params (struct interface *, struct in_addr);

int ospf_if_new_hook (struct interface *);
void ospf_if_init ();
void ospf_if_stream_set (struct ospf_interface *);
void ospf_if_stream_unset (struct ospf_interface *);
void ospf_if_reset_variables (struct ospf_interface *);
int ospf_if_is_enable (struct ospf_interface *);
int ospf_if_get_output_cost (struct ospf_interface *);
void ospf_if_recalculate_output_cost (struct interface *);

struct ospf_interface *ospf_vl_new (struct ospf *, struct ospf_vl_data *);
struct ospf_vl_data *ospf_vl_data_new (struct ospf_area *, struct in_addr);
struct ospf_vl_data *ospf_vl_lookup (struct ospf_area *, struct in_addr);
void ospf_vl_data_free (struct ospf_vl_data *);
void ospf_vl_add (struct ospf *, struct ospf_vl_data *);
void ospf_vl_delete (struct ospf *, struct ospf_vl_data *);
void ospf_vl_up_check (struct ospf_area *, struct in_addr, struct vertex *);
void ospf_vl_unapprove (struct ospf *);
void ospf_vl_shut_unapproved (struct ospf *);
int ospf_full_virtual_nbrs (struct ospf_area *);
int ospf_vls_in_area (struct ospf_area *);

struct crypt_key *ospf_crypt_key_lookup (list, u_char);
struct crypt_key *ospf_crypt_key_new ();
void ospf_crypt_key_add (list, struct crypt_key *);
int ospf_crypt_key_delete (list, u_char);

#endif /* _ZEBRA_OSPF_INTERFACE_H */
