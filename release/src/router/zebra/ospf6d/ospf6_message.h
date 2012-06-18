/*
 * Copyright (C) 1999-2003 Yasuhiro Ohara
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

#ifndef OSPF6_MESSAGE_H
#define OSPF6_MESSAGE_H

#define OSPF6_MESSAGE_BUFSIZ  4096

/* Debug option */
extern unsigned char conf_debug_ospf6_message[];
#define OSPF6_DEBUG_MESSAGE_SEND 0x01
#define OSPF6_DEBUG_MESSAGE_RECV 0x02
#define OSPF6_DEBUG_MESSAGE_ON(type, level) \
  (conf_debug_ospf6_message[type] |= (level))
#define OSPF6_DEBUG_MESSAGE_OFF(type, level) \
  (conf_debug_ospf6_message[type] &= ~(level))
#define IS_OSPF6_DEBUG_MESSAGE(t, e) \
  (conf_debug_ospf6_message[t] & OSPF6_DEBUG_MESSAGE_ ## e)

/* Type */
#define OSPF6_MESSAGE_TYPE_UNKNOWN  0x0
#define OSPF6_MESSAGE_TYPE_HELLO    0x1  /* Discover/maintain neighbors */
#define OSPF6_MESSAGE_TYPE_DBDESC   0x2  /* Summarize database contents */
#define OSPF6_MESSAGE_TYPE_LSREQ    0x3  /* Database download request */
#define OSPF6_MESSAGE_TYPE_LSUPDATE 0x4  /* Database update */
#define OSPF6_MESSAGE_TYPE_LSACK    0x5  /* Flooding acknowledgment */
#define OSPF6_MESSAGE_TYPE_ALL      0x6  /* For debug option */

#define OSPF6_MESSAGE_TYPE_CANONICAL(T) \
  ((T) > OSPF6_MESSAGE_TYPE_LSACK ? OSPF6_MESSAGE_TYPE_UNKNOWN : (T))

extern char *ospf6_message_type_str[];
#define OSPF6_MESSAGE_TYPE_NAME(T) \
  (ospf6_message_type_str[ OSPF6_MESSAGE_TYPE_CANONICAL (T) ])

/* OSPFv3 packet header */
struct ospf6_header
{
  u_char    version;
  u_char    type;
  u_int16_t length;
  u_int32_t router_id;
  u_int32_t area_id;
  u_int16_t checksum;
  u_char    instance_id;
  u_char    reserved;
};

#define OSPF6_MESSAGE_END(H) ((caddr_t) (H) + ntohs ((H)->length))

/* Hello */
struct ospf6_hello
{
  u_int32_t interface_id;
  u_char    priority;
  u_char    options[3];
  u_int16_t hello_interval;
  u_int16_t dead_interval;
  u_int32_t drouter;
  u_int32_t bdrouter;
  /* Followed by Router-IDs */
};

/* Database Description */
struct ospf6_dbdesc
{
  u_char    reserved1;
  u_char    options[3];
  u_int16_t ifmtu;
  u_char    reserved2;
  u_char    bits;
  u_int32_t seqnum;
  /* Followed by LSA Headers */
};

#define OSPF6_DBDESC_MSBIT (0x01) /* master/slave bit */
#define OSPF6_DBDESC_MBIT  (0x02) /* more bit */
#define OSPF6_DBDESC_IBIT  (0x04) /* initial bit */

/* Link State Request */
/* It is just a sequence of entries below */
struct ospf6_lsreq_entry
{
  u_int16_t reserved;     /* Must Be Zero */
  u_int16_t type;         /* LS type */
  u_int32_t id;           /* Link State ID */
  u_int32_t adv_router;   /* Advertising Router */
};

/* Link State Update */
struct ospf6_lsupdate
{
  u_int32_t lsa_number;
  /* Followed by LSAs */
};

/* Link State Acknowledgement */
/* It is just a sequence of LSA Headers */

/* Function definition */
void ospf6_hello_print (struct ospf6_header *);
void ospf6_dbdesc_print (struct ospf6_header *);
void ospf6_lsreq_print (struct ospf6_header *);
void ospf6_lsupdate_print (struct ospf6_header *);
void ospf6_lsack_print (struct ospf6_header *);

int ospf6_iobuf_size (int size);
int ospf6_receive (struct thread *thread);

int ospf6_hello_send (struct thread *thread);
int ospf6_dbdesc_send (struct thread *thread);
int ospf6_dbdesc_send_newone (struct thread *thread);
int ospf6_lsreq_send (struct thread *thread);
int ospf6_lsupdate_send_interface (struct thread *thread);
int ospf6_lsupdate_send_neighbor (struct thread *thread);
int ospf6_lsack_send_interface (struct thread *thread);
int ospf6_lsack_send_neighbor (struct thread *thread);

int config_write_ospf6_debug_message (struct vty *);
void install_element_ospf6_debug_message ();

#endif /* OSPF6_MESSAGE_H */

