/*
 * Server side of OSPF API.
 * Copyright (C) 2001, 2002 Ralph Keller
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

#ifndef _OSPF_APISERVER_H
#define _OSPF_APISERVER_H

/* MTYPE definition is not reflected to "memory.h". */
#define MTYPE_OSPF_APISERVER MTYPE_TMP
#define MTYPE_OSPF_APISERVER_MSGFILTER MTYPE_TMP

/* List of opaque types that application registered */
struct registered_opaque_type
{
  u_char lsa_type;
  u_char opaque_type;
};


/* Server instance for each accepted client connection. */
struct ospf_apiserver
{
  /* Socket connections for synchronous commands and asynchronous
     notifications */
  int fd_sync;			/* synchronous requests */
  struct sockaddr_in peer_sync;

  int fd_async;			/* asynchronous notifications */
  struct sockaddr_in peer_async;

  /* List of all opaque types that application registers to use. Using
     a single connection with the OSPF daemon, multiple
     <lsa,opaque_type> pairs can be registered. However, each
     combination can only be registered once by all applications. */
  struct list *opaque_types;		/* of type registered_opaque_type */

  /* Temporary storage for LSA instances to be refreshed. */
  struct ospf_lsdb reserve;

  /* filter for LSA update/delete notifies */
  struct lsa_filter_type *filter;

  /* Fifo buffers for outgoing messages */
  struct msg_fifo *out_sync_fifo;
  struct msg_fifo *out_async_fifo;

  /* Read and write threads */
  struct thread *t_sync_read;
#ifdef USE_ASYNC_READ
  struct thread *t_async_read;
#endif /* USE_ASYNC_READ */
  struct thread *t_sync_write;
  struct thread *t_async_write;
};

enum event
{
  OSPF_APISERVER_ACCEPT,
  OSPF_APISERVER_SYNC_READ,
#ifdef USE_ASYNC_READ
  OSPF_APISERVER_ASYNC_READ,
#endif /* USE_ASYNC_READ */
  OSPF_APISERVER_SYNC_WRITE,
  OSPF_APISERVER_ASYNC_WRITE
};

/* -----------------------------------------------------------
 * Followings are functions to manage client connections.
 * -----------------------------------------------------------
 */

extern unsigned short ospf_apiserver_getport (void);
extern int ospf_apiserver_init (void);
extern void ospf_apiserver_term (void);
extern struct ospf_apiserver *ospf_apiserver_new (int fd_sync, int fd_async);
extern void ospf_apiserver_free (struct ospf_apiserver *apiserv);
extern void ospf_apiserver_event (enum event event, int fd,
			   struct ospf_apiserver *apiserv);
extern int ospf_apiserver_serv_sock_family (unsigned short port, int family);
extern int ospf_apiserver_accept (struct thread *thread);
extern int ospf_apiserver_read (struct thread *thread);
extern int ospf_apiserver_sync_write (struct thread *thread);
extern int ospf_apiserver_async_write (struct thread *thread);
extern int ospf_apiserver_send_reply (struct ospf_apiserver *apiserv,
			       u_int32_t seqnr, u_char rc);

/* -----------------------------------------------------------
 * Followings are message handler functions
 * -----------------------------------------------------------
 */

extern int ospf_apiserver_lsa9_originator (void *arg);
extern int ospf_apiserver_lsa10_originator (void *arg);
extern int ospf_apiserver_lsa11_originator (void *arg);

extern void ospf_apiserver_clients_notify_all (struct msg *msg);

extern void ospf_apiserver_clients_notify_ready_type9 (struct ospf_interface *oi);
extern void ospf_apiserver_clients_notify_ready_type10 (struct ospf_area *area);
extern void ospf_apiserver_clients_notify_ready_type11 (struct ospf *top);

extern void ospf_apiserver_clients_notify_new_if (struct ospf_interface *oi);
extern void ospf_apiserver_clients_notify_del_if (struct ospf_interface *oi);
extern void ospf_apiserver_clients_notify_ism_change (struct ospf_interface *oi);
extern void ospf_apiserver_clients_notify_nsm_change (struct ospf_neighbor *nbr);

extern int ospf_apiserver_is_ready_type9 (struct ospf_interface *oi);
extern int ospf_apiserver_is_ready_type10 (struct ospf_area *area);
extern int ospf_apiserver_is_ready_type11 (struct ospf *ospf);

extern void ospf_apiserver_notify_ready_type9 (struct ospf_apiserver *apiserv);
extern void ospf_apiserver_notify_ready_type10 (struct ospf_apiserver *apiserv);
extern void ospf_apiserver_notify_ready_type11 (struct ospf_apiserver *apiserv);

extern int ospf_apiserver_handle_msg (struct ospf_apiserver *apiserv,
			       struct msg *msg);
extern int ospf_apiserver_handle_register_opaque_type (struct ospf_apiserver
						*apiserv, struct msg *msg);
extern int ospf_apiserver_handle_unregister_opaque_type (struct ospf_apiserver
						  *apiserv, struct msg *msg);
extern int ospf_apiserver_handle_register_event (struct ospf_apiserver *apiserv,
					  struct msg *msg);
extern int ospf_apiserver_handle_originate_request (struct ospf_apiserver *apiserv,
					     struct msg *msg);
extern int ospf_apiserver_handle_delete_request (struct ospf_apiserver *apiserv,
					  struct msg *msg);
extern int ospf_apiserver_handle_sync_lsdb (struct ospf_apiserver *apiserv,
				     struct msg *msg);


/* -----------------------------------------------------------
 * Followings are functions for LSA origination/deletion
 * -----------------------------------------------------------
 */

extern int ospf_apiserver_register_opaque_type (struct ospf_apiserver *apiserver,
					 u_char lsa_type, u_char opaque_type);
extern int ospf_apiserver_unregister_opaque_type (struct ospf_apiserver *apiserver,
					   u_char lsa_type,
					   u_char opaque_type);
extern struct ospf_lsa *ospf_apiserver_opaque_lsa_new (struct ospf_area *area,
						struct ospf_interface *oi,
						struct lsa_header *protolsa);
extern struct ospf_interface *ospf_apiserver_if_lookup_by_addr (struct in_addr
							 address);
extern struct ospf_interface *ospf_apiserver_if_lookup_by_ifp (struct interface
							*ifp);
extern int ospf_apiserver_originate1 (struct ospf_lsa *lsa);
extern void ospf_apiserver_flood_opaque_lsa (struct ospf_lsa *lsa);


/* -----------------------------------------------------------
 * Followings are callback functions to handle opaque types 
 * -----------------------------------------------------------
 */

extern int ospf_apiserver_new_if (struct interface *ifp);
extern int ospf_apiserver_del_if (struct interface *ifp);
extern void ospf_apiserver_ism_change (struct ospf_interface *oi, int old_status);
extern void ospf_apiserver_nsm_change (struct ospf_neighbor *nbr, int old_status);
extern void ospf_apiserver_config_write_router (struct vty *vty);
extern void ospf_apiserver_config_write_if (struct vty *vty, struct interface *ifp);
extern void ospf_apiserver_show_info (struct vty *vty, struct ospf_lsa *lsa);
extern int ospf_ospf_apiserver_lsa_originator (void *arg);
extern struct ospf_lsa *ospf_apiserver_lsa_refresher (struct ospf_lsa *lsa);
extern void ospf_apiserver_flush_opaque_lsa (struct ospf_apiserver *apiserv,
				      u_char lsa_type, u_char opaque_type);

/* -----------------------------------------------------------
 * Followings are hooks when LSAs are updated or deleted
 * -----------------------------------------------------------
 */


/* Hooks that are invoked from ospf opaque module */

extern int ospf_apiserver_lsa_update (struct ospf_lsa *lsa);
extern int ospf_apiserver_lsa_delete (struct ospf_lsa *lsa);

extern void ospf_apiserver_clients_lsa_change_notify (u_char msgtype,
					       struct ospf_lsa *lsa);

#endif /* _OSPF_APISERVER_H */
