/*
 * Client side of OSPF API.
 * Copyright (C) 2001, 2002, 2003 Ralph Keller
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

#ifndef _OSPF_APICLIENT_H
#define _OSPF_APICLIENT_H

#define MTYPE_OSPF_APICLIENT 0

/* Structure for the OSPF API client */
struct ospf_apiclient
{

  /* Sockets for sync requests and async notifications */
  int fd_sync;
  int fd_async;

  /* Pointer to callback functions */
  void (*ready_notify) (u_char lsa_type, u_char opaque_type,
			struct in_addr addr);
  void (*new_if) (struct in_addr ifaddr, struct in_addr area_id);
  void (*del_if) (struct in_addr ifaddr);
  void (*ism_change) (struct in_addr ifaddr, struct in_addr area_id,
		      u_char status);
  void (*nsm_change) (struct in_addr ifaddr, struct in_addr nbraddr,
		      struct in_addr router_id, u_char status);
  void (*update_notify) (struct in_addr ifaddr, struct in_addr area_id,
			 u_char self_origin,
			 struct lsa_header * lsa);
  void (*delete_notify) (struct in_addr ifaddr, struct in_addr area_id,
			 u_char self_origin,
			 struct lsa_header * lsa);
};


/* ---------------------------------------------------------
 * API function prototypes.
 * --------------------------------------------------------- */

/* Open connection to OSPF daemon. Two ports will be allocated on
   client, sync channel at syncport and reverse channel at syncport+1 */
struct ospf_apiclient *ospf_apiclient_connect (char *host, int syncport);

/* Shutdown connection to OSPF daemon. */
int ospf_apiclient_close (struct ospf_apiclient *oclient);

/* Synchronous request to register opaque type. */
int ospf_apiclient_register_opaque_type (struct ospf_apiclient *oclient,
					 u_char ltype, u_char otype);

/* Synchronous request to register event mask. */
int ospf_apiclient_register_events (struct ospf_apiclient *oclient,
				    u_int32_t mask);

/* Register callback functions.*/
void ospf_apiclient_register_callback (struct ospf_apiclient *oclient,
				       void (*ready_notify) (u_char lsa_type,
							     u_char
							     opaque_type,
							     struct in_addr
							     addr),
				       void (*new_if) (struct in_addr ifaddr,
						       struct in_addr
						       area_id),
				       void (*del_if) (struct in_addr ifaddr),
				       void (*ism_change) (struct in_addr
							   ifaddr,
							   struct in_addr
							   area_id,
							   u_char status),
				       void (*nsm_change) (struct in_addr
							   ifaddr,
							   struct in_addr
							   nbraddr,
							   struct in_addr
							   router_id,
							   u_char status),
				       void (*update_notify) (struct in_addr
							      ifaddr,
							      struct in_addr
							      area_id,
							      u_char selforig,
							      struct
							      lsa_header *
							      lsa),
				       void (*delete_notify) (struct in_addr
							      ifaddr,
							      struct in_addr
							      area_id,
							      u_char selforig,
							      struct
							      lsa_header *
							      lsa));

/* Synchronous request to synchronize LSDB. */
int ospf_apiclient_sync_lsdb (struct ospf_apiclient *oclient);

/* Synchronous request to originate or update opaque LSA. */
int
ospf_apiclient_lsa_originate(struct ospf_apiclient *oclient,
                             struct in_addr ifaddr,
                             struct in_addr area_id,
                             u_char lsa_type,
                             u_char opaque_type, u_int32_t opaque_id,
                             void *opaquedata, int opaquelen);


/* Synchronous request to delete opaque LSA. Parameter opaque_id is in
   host byte order */
int ospf_apiclient_lsa_delete (struct ospf_apiclient *oclient,
			       struct in_addr area_id, u_char lsa_type,
			       u_char opaque_type, u_int32_t opaque_id);

/* Fetch async message and handle it  */
int ospf_apiclient_handle_async (struct ospf_apiclient *oclient);

#endif /* _OSPF_APICLIENT_H */
