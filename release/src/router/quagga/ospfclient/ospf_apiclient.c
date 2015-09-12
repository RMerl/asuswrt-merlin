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

#include <zebra.h>

#include <lib/version.h>
#include "getopt.h"
#include "thread.h"
#include "prefix.h"
#include "linklist.h"
#include "if.h"
#include "vector.h"
#include "vty.h"
#include "command.h"
#include "filter.h"
#include "stream.h"
#include "log.h"
#include "memory.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_interface.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_opaque.h"
#include "ospfd/ospf_lsdb.h"
#include "ospfd/ospf_neighbor.h"
#include "ospfd/ospf_dump.h"
#include "ospfd/ospf_zebra.h"
#include "ospfd/ospf_api.h"

#include "ospf_apiclient.h"

/* Backlog for listen */
#define BACKLOG 5

/* -----------------------------------------------------------
 * Forward declarations
 * -----------------------------------------------------------
 */

void ospf_apiclient_handle_reply (struct ospf_apiclient *oclient,
				  struct msg *msg);
void ospf_apiclient_handle_update_notify (struct ospf_apiclient *oclient,
					  struct msg *msg);
void ospf_apiclient_handle_delete_notify (struct ospf_apiclient *oclient,
					  struct msg *msg);

/* -----------------------------------------------------------
 * Initialization
 * -----------------------------------------------------------
 */

static unsigned short
ospf_apiclient_getport (void)
{
  struct servent *sp = getservbyname ("ospfapi", "tcp");

  return sp ? ntohs (sp->s_port) : OSPF_API_SYNC_PORT;
}

/* -----------------------------------------------------------
 * Followings are functions for connection management
 * -----------------------------------------------------------
 */

struct ospf_apiclient *
ospf_apiclient_connect (char *host, int syncport)
{
  struct sockaddr_in myaddr_sync;
  struct sockaddr_in myaddr_async;
  struct sockaddr_in peeraddr;
  struct hostent *hp;
  struct ospf_apiclient *new;
  int size = 0;
  unsigned int peeraddrlen;
  int async_server_sock;
  int fd1, fd2;
  int ret;
  int on = 1;

  /* There are two connections between the client and the server.
     First the client opens a connection for synchronous requests/replies 
     to the server. The server will accept this connection and
     as a reaction open a reverse connection channel for 
     asynchronous messages. */

  async_server_sock = socket (AF_INET, SOCK_STREAM, 0);
  if (async_server_sock < 0)
    {
      fprintf (stderr,
	       "ospf_apiclient_connect: creating async socket failed\n");
      return NULL;
    }

  /* Prepare socket for asynchronous messages */
  /* Initialize async address structure */
  memset (&myaddr_async, 0, sizeof (struct sockaddr_in));
  myaddr_async.sin_family = AF_INET;
  myaddr_async.sin_addr.s_addr = htonl (INADDR_ANY);
  myaddr_async.sin_port = htons (syncport+1);
  size = sizeof (struct sockaddr_in);
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
  myaddr_async.sin_len = size;
#endif /* HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */

  /* This is a server socket, reuse addr and port */
  ret = setsockopt (async_server_sock, SOL_SOCKET,
		    SO_REUSEADDR, (void *) &on, sizeof (on));
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: SO_REUSEADDR failed\n");
      close (async_server_sock);
      return NULL;
    }

#ifdef SO_REUSEPORT
  ret = setsockopt (async_server_sock, SOL_SOCKET, SO_REUSEPORT,
		    (void *) &on, sizeof (on));
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: SO_REUSEPORT failed\n");
      close (async_server_sock);
      return NULL;
    }
#endif /* SO_REUSEPORT */

  /* Bind socket to address structure */
  ret = bind (async_server_sock, (struct sockaddr *) &myaddr_async, size);
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: bind async socket failed\n");
      close (async_server_sock);
      return NULL;
    }

  /* Wait for reverse channel connection establishment from server */
  ret = listen (async_server_sock, BACKLOG);
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: listen: %s\n", safe_strerror (errno));
      close (async_server_sock);
      return NULL;
    }

  /* Make connection for synchronous requests and connect to server */
  /* Resolve address of server */
  hp = gethostbyname (host);
  if (!hp)
    {
      fprintf (stderr, "ospf_apiclient_connect: no such host %s\n", host);
      close (async_server_sock);
      return NULL;
    }

  fd1 = socket (AF_INET, SOCK_STREAM, 0);
  if (fd1 < 0)
    {
      fprintf (stderr,
	       "ospf_apiclient_connect: creating sync socket failed\n");
      return NULL;
    }


  /* Reuse addr and port */
  ret = setsockopt (fd1, SOL_SOCKET,
                    SO_REUSEADDR, (void *) &on, sizeof (on));
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: SO_REUSEADDR failed\n");
      close (fd1);
      return NULL;
    }

#ifdef SO_REUSEPORT
  ret = setsockopt (fd1, SOL_SOCKET, SO_REUSEPORT,
                    (void *) &on, sizeof (on));
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: SO_REUSEPORT failed\n");
      close (fd1);
      return NULL;
    }
#endif /* SO_REUSEPORT */


  /* Bind sync socket to address structure. This is needed since we
     want the sync port number on a fixed port number. The reverse
     async channel will be at this port+1 */

  memset (&myaddr_sync, 0, sizeof (struct sockaddr_in));
  myaddr_sync.sin_family = AF_INET;
  myaddr_sync.sin_port = htons (syncport);
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
  myaddr_sync.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */

  ret = bind (fd1, (struct sockaddr *) &myaddr_sync, size);
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: bind sync socket failed\n");
      close (fd1);
      return NULL;
    }

  /* Prepare address structure for connect */
  memcpy (&myaddr_sync.sin_addr, hp->h_addr, hp->h_length);
  myaddr_sync.sin_family = AF_INET;
  myaddr_sync.sin_port = htons(ospf_apiclient_getport ());
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
  myaddr_sync.sin_len = sizeof (struct sockaddr_in);
#endif /* HAVE_STRUCT_SOCKADDR_IN_SIN_LEN */

  /* Now establish synchronous channel with OSPF daemon */
  ret = connect (fd1, (struct sockaddr *) &myaddr_sync,
		 sizeof (struct sockaddr_in));
  if (ret < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: sync connect failed\n");
      close (async_server_sock);
      close (fd1);
      return NULL;
    }

  /* Accept reverse connection */
  peeraddrlen = sizeof (struct sockaddr_in);
  memset (&peeraddr, 0, peeraddrlen);

  fd2 =
    accept (async_server_sock, (struct sockaddr *) &peeraddr, &peeraddrlen);
  if (fd2 < 0)
    {
      fprintf (stderr, "ospf_apiclient_connect: accept async failed\n");
      close (async_server_sock);
      close (fd1);
      return NULL;
    }

  /* Server socket is not needed anymore since we are not accepting more 
     connections */
  close (async_server_sock);

  /* Create new client-side instance */
  new = XCALLOC (MTYPE_OSPF_APICLIENT, sizeof (struct ospf_apiclient));

  /* Initialize socket descriptors for sync and async channels */
  new->fd_sync = fd1;
  new->fd_async = fd2;

  return new;
}

int
ospf_apiclient_close (struct ospf_apiclient *oclient)
{

  if (oclient->fd_sync >= 0)
    {
      close (oclient->fd_sync);
    }

  if (oclient->fd_async >= 0)
    {
      close (oclient->fd_async);
    }

  /* Free client structure */
  XFREE (MTYPE_OSPF_APICLIENT, oclient);
  return 0;
}

/* -----------------------------------------------------------
 * Followings are functions to send a request to OSPFd
 * -----------------------------------------------------------
 */

/* Send synchronous request, wait for reply */
static int
ospf_apiclient_send_request (struct ospf_apiclient *oclient, struct msg *msg)
{
  u_int32_t reqseq;
  struct msg_reply *msgreply;
  int rc;

  /* NB: Given "msg" is freed inside this function. */

  /* Remember the sequence number of the request */
  reqseq = ntohl (msg->hdr.msgseq);

  /* Write message to OSPFd */
  rc = msg_write (oclient->fd_sync, msg);
  msg_free (msg);

  if (rc < 0)
    {
      return -1;
    }

  /* Wait for reply *//* NB: New "msg" is allocated by "msg_read()". */
  msg = msg_read (oclient->fd_sync);
  if (!msg)
    return -1;

  assert (msg->hdr.msgtype == MSG_REPLY);
  assert (ntohl (msg->hdr.msgseq) == reqseq);

  msgreply = (struct msg_reply *) STREAM_DATA (msg->s);
  rc = msgreply->errcode;
  msg_free (msg);

  return rc;
}


/* -----------------------------------------------------------
 * Helper functions
 * -----------------------------------------------------------
 */

static u_int32_t
ospf_apiclient_get_seqnr (void)
{
  static u_int32_t seqnr = MIN_SEQ;
  u_int32_t tmp;

  tmp = seqnr;
  /* Increment sequence number */
  if (seqnr < MAX_SEQ)
    {
      seqnr++;
    }
  else
    {
      seqnr = MIN_SEQ;
    }
  return tmp;
}

/* -----------------------------------------------------------
 * API to access OSPF daemon by client applications.
 * -----------------------------------------------------------
 */

/*
 * Synchronous request to register opaque type.
 */
int
ospf_apiclient_register_opaque_type (struct ospf_apiclient *cl,
				     u_char ltype, u_char otype)
{
  struct msg *msg;
  int rc;

  /* just put 1 as a sequence number. */
  msg = new_msg_register_opaque_type (ospf_apiclient_get_seqnr (),
				      ltype, otype);
  if (!msg)
    {
      fprintf (stderr, "new_msg_register_opaque_type failed\n");
      return -1;
    }

  rc = ospf_apiclient_send_request (cl, msg);
  return rc;
}

/* 
 * Synchronous request to synchronize with OSPF's LSDB.
 * Two steps required: register_event in order to get
 * dynamic updates and LSDB_Sync.
 */
int
ospf_apiclient_sync_lsdb (struct ospf_apiclient *oclient)
{
  struct msg *msg;
  int rc;
  struct lsa_filter_type filter;

  filter.typemask = 0xFFFF;	/* all LSAs */
  filter.origin = ANY_ORIGIN;
  filter.num_areas = 0;		/* all Areas. */

  msg = new_msg_register_event (ospf_apiclient_get_seqnr (), &filter);
  if (!msg)
    {
      fprintf (stderr, "new_msg_register_event failed\n");
      return -1;
    }
  rc = ospf_apiclient_send_request (oclient, msg);

  if (rc != 0)
    goto out;

  msg = new_msg_sync_lsdb (ospf_apiclient_get_seqnr (), &filter);
  if (!msg)
    {
      fprintf (stderr, "new_msg_sync_lsdb failed\n");
      return -1;
    }
  rc = ospf_apiclient_send_request (oclient, msg);

out:
  return rc;
}

/* 
 * Synchronous request to originate or update an LSA.
 */

int
ospf_apiclient_lsa_originate (struct ospf_apiclient *oclient,
			      struct in_addr ifaddr,
			      struct in_addr area_id,
			      u_char lsa_type,
			      u_char opaque_type, u_int32_t opaque_id,
			      void *opaquedata, int opaquelen)
{
  struct msg *msg;
  int rc;
  u_char buf[OSPF_MAX_LSA_SIZE];
  struct lsa_header *lsah;
  u_int32_t tmp;


  /* We can only originate opaque LSAs */
  if (!IS_OPAQUE_LSA (lsa_type))
    {
      fprintf (stderr, "Cannot originate non-opaque LSA type %d\n", lsa_type);
      return OSPF_API_ILLEGALLSATYPE;
    }

  /* Make a new LSA from parameters */
  lsah = (struct lsa_header *) buf;
  lsah->ls_age = 0;
  lsah->options = 0;
  lsah->type = lsa_type;

  tmp = SET_OPAQUE_LSID (opaque_type, opaque_id);
  lsah->id.s_addr = htonl (tmp);
  lsah->adv_router.s_addr = 0;
  lsah->ls_seqnum = 0;
  lsah->checksum = 0;
  lsah->length = htons (sizeof (struct lsa_header) + opaquelen);

  memcpy (((u_char *) lsah) + sizeof (struct lsa_header), opaquedata,
	  opaquelen);

  msg = new_msg_originate_request (ospf_apiclient_get_seqnr (),
				   ifaddr, area_id, lsah);
  if (!msg)
    {
      fprintf (stderr, "new_msg_originate_request failed\n");
      return OSPF_API_NOMEMORY;
    }

  rc = ospf_apiclient_send_request (oclient, msg);
  return rc;
}

int
ospf_apiclient_lsa_delete (struct ospf_apiclient *oclient,
			   struct in_addr area_id, u_char lsa_type,
			   u_char opaque_type, u_int32_t opaque_id)
{
  struct msg *msg;
  int rc;

  /* Only opaque LSA can be deleted */
  if (!IS_OPAQUE_LSA (lsa_type))
    {
      fprintf (stderr, "Cannot delete non-opaque LSA type %d\n", lsa_type);
      return OSPF_API_ILLEGALLSATYPE;
    }

  /* opaque_id is in host byte order and will be converted
   * to network byte order by new_msg_delete_request */
  msg = new_msg_delete_request (ospf_apiclient_get_seqnr (),
				area_id, lsa_type, opaque_type, opaque_id);

  rc = ospf_apiclient_send_request (oclient, msg);
  return rc;
}

/* -----------------------------------------------------------
 * Followings are handlers for messages from OSPF daemon
 * -----------------------------------------------------------
 */

static void
ospf_apiclient_handle_ready (struct ospf_apiclient *oclient, struct msg *msg)
{
  struct msg_ready_notify *r;
  r = (struct msg_ready_notify *) STREAM_DATA (msg->s);

  /* Invoke registered callback function. */
  if (oclient->ready_notify)
    {
      (oclient->ready_notify) (r->lsa_type, r->opaque_type, r->addr);
    }
}

static void
ospf_apiclient_handle_new_if (struct ospf_apiclient *oclient, struct msg *msg)
{
  struct msg_new_if *n;
  n = (struct msg_new_if *) STREAM_DATA (msg->s);

  /* Invoke registered callback function. */
  if (oclient->new_if)
    {
      (oclient->new_if) (n->ifaddr, n->area_id);
    }
}

static void
ospf_apiclient_handle_del_if (struct ospf_apiclient *oclient, struct msg *msg)
{
  struct msg_del_if *d;
  d = (struct msg_del_if *) STREAM_DATA (msg->s);

  /* Invoke registered callback function. */
  if (oclient->del_if)
    {
      (oclient->del_if) (d->ifaddr);
    }
}

static void
ospf_apiclient_handle_ism_change (struct ospf_apiclient *oclient,
				  struct msg *msg)
{
  struct msg_ism_change *m;
  m = (struct msg_ism_change *) STREAM_DATA (msg->s);

  /* Invoke registered callback function. */
  if (oclient->ism_change)
    {
      (oclient->ism_change) (m->ifaddr, m->area_id, m->status);
    }

}

static void
ospf_apiclient_handle_nsm_change (struct ospf_apiclient *oclient,
				  struct msg *msg)
{
  struct msg_nsm_change *m;
  m = (struct msg_nsm_change *) STREAM_DATA (msg->s);

  /* Invoke registered callback function. */
  if (oclient->nsm_change)
    {
      (oclient->nsm_change) (m->ifaddr, m->nbraddr, m->router_id, m->status);
    }
}

static void
ospf_apiclient_handle_lsa_update (struct ospf_apiclient *oclient,
				  struct msg *msg)
{
  struct msg_lsa_change_notify *cn;
  struct lsa_header *lsa;
  int lsalen;

  cn = (struct msg_lsa_change_notify *) STREAM_DATA (msg->s);

  /* Extract LSA from message */
  lsalen = ntohs (cn->data.length);
  lsa = XMALLOC (MTYPE_OSPF_APICLIENT, lsalen);
  if (!lsa)
    {
      fprintf (stderr, "LSA update: Cannot allocate memory for LSA\n");
      return;
    }
  memcpy (lsa, &(cn->data), lsalen);

  /* Invoke registered update callback function */
  if (oclient->update_notify)
    {
      (oclient->update_notify) (cn->ifaddr, cn->area_id, 
				cn->is_self_originated, lsa);
    }

  /* free memory allocated by ospf apiclient library */
  XFREE (MTYPE_OSPF_APICLIENT, lsa);
}

static void
ospf_apiclient_handle_lsa_delete (struct ospf_apiclient *oclient,
				  struct msg *msg)
{
  struct msg_lsa_change_notify *cn;
  struct lsa_header *lsa;
  int lsalen;

  cn = (struct msg_lsa_change_notify *) STREAM_DATA (msg->s);

  /* Extract LSA from message */
  lsalen = ntohs (cn->data.length);
  lsa = XMALLOC (MTYPE_OSPF_APICLIENT, lsalen);
  if (!lsa)
    {
      fprintf (stderr, "LSA delete: Cannot allocate memory for LSA\n");
      return;
    }
  memcpy (lsa, &(cn->data), lsalen);

  /* Invoke registered update callback function */
  if (oclient->delete_notify)
    {
      (oclient->delete_notify) (cn->ifaddr, cn->area_id, 
				cn->is_self_originated, lsa);
    }

  /* free memory allocated by ospf apiclient library */
  XFREE (MTYPE_OSPF_APICLIENT, lsa);
}

static void
ospf_apiclient_msghandle (struct ospf_apiclient *oclient, struct msg *msg)
{
  /* Call message handler function. */
  switch (msg->hdr.msgtype)
    {
    case MSG_READY_NOTIFY:
      ospf_apiclient_handle_ready (oclient, msg);
      break;
    case MSG_NEW_IF:
      ospf_apiclient_handle_new_if (oclient, msg);
      break;
    case MSG_DEL_IF:
      ospf_apiclient_handle_del_if (oclient, msg);
      break;
    case MSG_ISM_CHANGE:
      ospf_apiclient_handle_ism_change (oclient, msg);
      break;
    case MSG_NSM_CHANGE:
      ospf_apiclient_handle_nsm_change (oclient, msg);
      break;
    case MSG_LSA_UPDATE_NOTIFY:
      ospf_apiclient_handle_lsa_update (oclient, msg);
      break;
    case MSG_LSA_DELETE_NOTIFY:
      ospf_apiclient_handle_lsa_delete (oclient, msg);
      break;
    default:
      fprintf (stderr, "ospf_apiclient_read: Unknown message type: %d\n",
	       msg->hdr.msgtype);
      break;
    }
}

/* -----------------------------------------------------------
 * Callback handler registration
 * -----------------------------------------------------------
 */

void
ospf_apiclient_register_callback (struct ospf_apiclient *oclient,
				  void (*ready_notify) (u_char lsa_type,
							u_char opaque_type,
							struct in_addr addr),
				  void (*new_if) (struct in_addr ifaddr,
						  struct in_addr area_id),
				  void (*del_if) (struct in_addr ifaddr),
				  void (*ism_change) (struct in_addr ifaddr,
						      struct in_addr area_id,
						      u_char status),
				  void (*nsm_change) (struct in_addr ifaddr,
						      struct in_addr nbraddr,
						      struct in_addr
						      router_id,
						      u_char status),
				  void (*update_notify) (struct in_addr
							 ifaddr,
							 struct in_addr
							 area_id,
							 u_char self_origin,
							 struct lsa_header *
							 lsa),
				  void (*delete_notify) (struct in_addr
							 ifaddr,
							 struct in_addr
							 area_id,
							 u_char self_origin,
							 struct lsa_header *
							 lsa))
{
  assert (oclient);
  assert (update_notify);

  /* Register callback function */
  oclient->ready_notify = ready_notify;
  oclient->new_if = new_if;
  oclient->del_if = del_if;
  oclient->ism_change = ism_change;
  oclient->nsm_change = nsm_change;
  oclient->update_notify = update_notify;
  oclient->delete_notify = delete_notify;
}

/* -----------------------------------------------------------
 * Asynchronous message handling
 * -----------------------------------------------------------
 */

int
ospf_apiclient_handle_async (struct ospf_apiclient *oclient)
{
  struct msg *msg;

  /* Get a message */
  msg = msg_read (oclient->fd_async);

  if (!msg)
    {
      /* Connection broke down */
      return -1;
    }

  /* Handle message */
  ospf_apiclient_msghandle (oclient, msg);

  /* Don't forget to free this message */
  msg_free (msg);

  return 0;
}
