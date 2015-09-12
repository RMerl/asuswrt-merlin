/*
 * API message handling module for OSPF daemon and client.
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


/* This file is used both by the OSPFd and client applications to
   define message formats used for communication. */

#ifndef _OSPF_API_H
#define _OSPF_API_H

#define OSPF_API_VERSION           1

/* MTYPE definition is not reflected to "memory.h". */
#define MTYPE_OSPF_API_MSG      MTYPE_TMP
#define MTYPE_OSPF_API_FIFO     MTYPE_TMP

/* Default API server port to accept connection request from client-side. */
/* This value could be overridden by "ospfapi" entry in "/etc/services". */
#define OSPF_API_SYNC_PORT      2607

/* -----------------------------------------------------------
 * Generic messages 
 * -----------------------------------------------------------
 */

/* Message header structure, fields are in network byte order and
   aligned to four octets. */
struct apimsghdr
{
  u_char version;		/* OSPF API protocol version */
  u_char msgtype;		/* Type of message */
  u_int16_t msglen;		/* Length of message w/o header */
  u_int32_t msgseq;		/* Sequence number */
};

/* Message representation with header and body */
struct msg
{
  struct msg *next;		/* to link into fifo */

  /* Message header */
  struct apimsghdr hdr;

  /* Message body */
  struct stream *s;
};

/* Prototypes for generic messages. */
extern struct msg *msg_new (u_char msgtype, void *msgbody,
		     u_int32_t seqnum, u_int16_t msglen);
extern struct msg *msg_dup (struct msg *msg);
extern void msg_print (struct msg *msg);	/* XXX debug only */
extern void msg_free (struct msg *msg);
struct msg *msg_read (int fd);
extern int msg_write (int fd, struct msg *msg);

/* For requests, the message sequence number is between MIN_SEQ and
   MAX_SEQ. For notifications, the sequence number is 0. */

#define MIN_SEQ          1
#define MAX_SEQ 2147483647

extern void msg_set_seq (struct msg *msg, u_int32_t seqnr);
extern u_int32_t msg_get_seq (struct msg *msg);

/* -----------------------------------------------------------
 * Message fifo queues
 * -----------------------------------------------------------
 */

/* Message queue structure. */
struct msg_fifo
{
  unsigned long count;

  struct msg *head;
  struct msg *tail;
};

/* Prototype for message fifo queues. */
extern struct msg_fifo *msg_fifo_new (void);
extern void msg_fifo_push (struct msg_fifo *, struct msg *msg);
extern struct msg *msg_fifo_pop (struct msg_fifo *fifo);
extern struct msg *msg_fifo_head (struct msg_fifo *fifo);
extern void msg_fifo_flush (struct msg_fifo *fifo);
extern void msg_fifo_free (struct msg_fifo *fifo);

/* -----------------------------------------------------------
 * Specific message type and format definitions
 * -----------------------------------------------------------
 */

/* Messages to OSPF daemon. */
#define MSG_REGISTER_OPAQUETYPE   1
#define MSG_UNREGISTER_OPAQUETYPE 2
#define MSG_REGISTER_EVENT        3
#define MSG_SYNC_LSDB             4
#define MSG_ORIGINATE_REQUEST     5
#define MSG_DELETE_REQUEST        6

/* Messages from OSPF daemon. */
#define MSG_REPLY                10
#define MSG_READY_NOTIFY         11
#define MSG_LSA_UPDATE_NOTIFY    12
#define MSG_LSA_DELETE_NOTIFY    13
#define MSG_NEW_IF               14
#define MSG_DEL_IF               15
#define MSG_ISM_CHANGE           16
#define MSG_NSM_CHANGE           17

struct msg_register_opaque_type
{
  u_char lsatype;
  u_char opaquetype;
  u_char pad[2];		/* padding */
};

struct msg_unregister_opaque_type
{
  u_char lsatype;
  u_char opaquetype;
  u_char pad[2];		/* padding */
};

/* Power2 is needed to convert LSA types into bit positions,
 * see typemask below. Type definition starts at 1, so
 * Power2[0] is not used. */


#ifdef ORIGINAL_CODING
static const u_int16_t
  Power2[] = { 0x0, 0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
  0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000
};
#else
static const u_int16_t
  Power2[] = { 0, (1 << 0), (1 << 1), (1 << 2), (1 << 3), (1 << 4),
  (1 << 5), (1 << 6), (1 << 7), (1 << 8), (1 << 9),
  (1 << 10), (1 << 11), (1 << 12), (1 << 13), (1 << 14),
  (1 << 15)
};
#endif /* ORIGINAL_CODING */

struct lsa_filter_type
{
  u_int16_t typemask;		/* bitmask for selecting LSA types (1..16) */
  u_char origin;		/* selects according to origin. */
#define NON_SELF_ORIGINATED	0
#define	SELF_ORIGINATED  (OSPF_LSA_SELF)
#define	ANY_ORIGIN 2

  u_char num_areas;		/* number of areas in the filter. */
  /* areas, if any, go here. */
};

struct msg_register_event
{
  struct lsa_filter_type filter;
};

struct msg_sync_lsdb
{
  struct lsa_filter_type filter;
};

struct msg_originate_request
{
  /* Used for LSA type 9 otherwise ignored */
  struct in_addr ifaddr;

  /* Used for LSA type 10 otherwise ignored */
  struct in_addr area_id;

  /* LSA header and LSA-specific part */
  struct lsa_header data;
};

struct msg_delete_request
{
  struct in_addr area_id;	/* "0.0.0.0" for AS-external opaque LSAs */
  u_char lsa_type;
  u_char opaque_type;
  u_char pad[2];		/* padding */
  u_int32_t opaque_id;
};

struct msg_reply
{
  signed char errcode;
#define OSPF_API_OK                         0
#define OSPF_API_NOSUCHINTERFACE          (-1)
#define OSPF_API_NOSUCHAREA               (-2)
#define OSPF_API_NOSUCHLSA                (-3)
#define OSPF_API_ILLEGALLSATYPE           (-4)
#define OSPF_API_OPAQUETYPEINUSE          (-5)
#define OSPF_API_OPAQUETYPENOTREGISTERED  (-6)
#define OSPF_API_NOTREADY                 (-7)
#define OSPF_API_NOMEMORY                 (-8)
#define OSPF_API_ERROR                    (-9)
#define OSPF_API_UNDEF                   (-10)
  u_char pad[3];		/* padding to four byte alignment */
};

/* Message to tell client application that it ospf daemon is 
 * ready to accept opaque LSAs for a given interface or area. */

struct msg_ready_notify
{
  u_char lsa_type;
  u_char opaque_type;
  u_char pad[2];		/* padding */
  struct in_addr addr;		/* interface address or area address */
};

/* These messages have a dynamic length depending on the embodied LSA.
   They are aligned to four octets. msg_lsa_change_notify is used for
   both LSA update and LSAs delete. */

struct msg_lsa_change_notify
{
  /* Used for LSA type 9 otherwise ignored */
  struct in_addr ifaddr;
  /* Area ID. Not valid for AS-External and Opaque11 LSAs. */
  struct in_addr area_id;
  u_char is_self_originated;	/* 1 if self originated. */
  u_char pad[3];
  struct lsa_header data;
};

struct msg_new_if
{
  struct in_addr ifaddr;	/* interface IP address */
  struct in_addr area_id;	/* area this interface belongs to */
};

struct msg_del_if
{
  struct in_addr ifaddr;	/* interface IP address */
};

struct msg_ism_change
{
  struct in_addr ifaddr;	/* interface IP address */
  struct in_addr area_id;	/* area this interface belongs to */
  u_char status;		/* interface status (up/down) */
  u_char pad[3];		/* not used */
};

struct msg_nsm_change
{
  struct in_addr ifaddr;	/* attached interface */
  struct in_addr nbraddr;	/* Neighbor interface address */
  struct in_addr router_id;	/* Router ID of neighbor */
  u_char status;		/* NSM status */
  u_char pad[3];
};

/* We make use of a union to define a structure that covers all
   possible API messages. This allows us to find out how much memory
   needs to be reserved for the largest API message. */
struct apimsg
{
  struct apimsghdr hdr;
  union
  {
    struct msg_register_opaque_type register_opaque_type;
    struct msg_register_event register_event;
    struct msg_sync_lsdb sync_lsdb;
    struct msg_originate_request originate_request;
    struct msg_delete_request delete_request;
    struct msg_reply reply;
    struct msg_ready_notify ready_notify;
    struct msg_new_if new_if;
    struct msg_del_if del_if;
    struct msg_ism_change ism_change;
    struct msg_nsm_change nsm_change;
    struct msg_lsa_change_notify lsa_change_notify;
  }
  u;
};

#define OSPF_API_MAX_MSG_SIZE (sizeof(struct apimsg) + OSPF_MAX_LSA_SIZE)

/* -----------------------------------------------------------
 * Prototypes for specific messages
 * -----------------------------------------------------------
 */

/* For debugging only. */
extern void api_opaque_lsa_print (struct lsa_header *data);

/* Messages sent by client */
extern struct msg *new_msg_register_opaque_type (u_int32_t seqnum,
						 u_char ltype, u_char otype);
extern struct msg *new_msg_register_event (u_int32_t seqnum,
					   struct lsa_filter_type *filter);
extern struct msg *new_msg_sync_lsdb (u_int32_t seqnum,
				      struct lsa_filter_type *filter);
extern struct msg *new_msg_originate_request (u_int32_t seqnum,
					      struct in_addr ifaddr,
					      struct in_addr area_id,
					      struct lsa_header *data);
extern struct msg *new_msg_delete_request (u_int32_t seqnum,
					   struct in_addr area_id,
					   u_char lsa_type,
					   u_char opaque_type,
					   u_int32_t opaque_id);

/* Messages sent by OSPF daemon */
extern struct msg *new_msg_reply (u_int32_t seqnum, u_char rc);

extern struct msg *new_msg_ready_notify (u_int32_t seqnr, u_char lsa_type,
					 u_char opaque_type,
					 struct in_addr addr);

extern struct msg *new_msg_new_if (u_int32_t seqnr,
				   struct in_addr ifaddr,
				   struct in_addr area);

extern struct msg *new_msg_del_if (u_int32_t seqnr, struct in_addr ifaddr);

extern struct msg *new_msg_ism_change (u_int32_t seqnr, struct in_addr ifaddr,
				       struct in_addr area, u_char status);

extern struct msg *new_msg_nsm_change (u_int32_t seqnr, struct in_addr ifaddr,
				       struct in_addr nbraddr,
				       struct in_addr router_id,
				       u_char status);

/* msgtype is MSG_LSA_UPDATE_NOTIFY or MSG_LSA_DELETE_NOTIFY */
extern struct msg *new_msg_lsa_change_notify (u_char msgtype,
					      u_int32_t seqnum,
					      struct in_addr ifaddr,
					      struct in_addr area_id,
					      u_char is_self_originated,
					      struct lsa_header *data);

/* string printing functions */
extern const char *ospf_api_errname (int errcode);
extern const char *ospf_api_typename (int msgtype);

#endif /* _OSPF_API_H */
