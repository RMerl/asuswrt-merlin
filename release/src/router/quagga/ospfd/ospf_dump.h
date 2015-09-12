/*
 * OSPFd dump routine.
 * Copyright (C) 1999 Toshiaki Takada
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

#ifndef _ZEBRA_OSPF_DUMP_H
#define _ZEBRA_OSPF_DUMP_H

/* Debug Flags. */
#define OSPF_DEBUG_HELLO	0x01
#define OSPF_DEBUG_DB_DESC	0x02
#define OSPF_DEBUG_LS_REQ	0x04
#define OSPF_DEBUG_LS_UPD	0x08
#define OSPF_DEBUG_LS_ACK	0x10
#define OSPF_DEBUG_ALL		0x1f

#define OSPF_DEBUG_SEND		0x01
#define OSPF_DEBUG_RECV		0x02
#define OSPF_DEBUG_SEND_RECV    0x03
#define OSPF_DEBUG_DETAIL	0x04

#define OSPF_DEBUG_ISM_STATUS	0x01
#define OSPF_DEBUG_ISM_EVENTS	0x02
#define OSPF_DEBUG_ISM_TIMERS	0x04
#define OSPF_DEBUG_ISM		0x07
#define OSPF_DEBUG_NSM_STATUS	0x01
#define OSPF_DEBUG_NSM_EVENTS	0x02
#define OSPF_DEBUG_NSM_TIMERS   0x04
#define OSPF_DEBUG_NSM		0x07

#define OSPF_DEBUG_LSA_GENERATE 0x01
#define OSPF_DEBUG_LSA_FLOODING	0x02
#define OSPF_DEBUG_LSA_INSTALL  0x04
#define OSPF_DEBUG_LSA_REFRESH  0x08
#define OSPF_DEBUG_LSA		0x0F

#define OSPF_DEBUG_ZEBRA_INTERFACE     0x01
#define OSPF_DEBUG_ZEBRA_REDISTRIBUTE  0x02
#define OSPF_DEBUG_ZEBRA	       0x03

#define OSPF_DEBUG_EVENT        0x01
#define OSPF_DEBUG_NSSA		0x02

/* Macro for setting debug option. */
#define CONF_DEBUG_PACKET_ON(a, b)	    conf_debug_ospf_packet[a] |= (b)
#define CONF_DEBUG_PACKET_OFF(a, b)	    conf_debug_ospf_packet[a] &= ~(b)
#define TERM_DEBUG_PACKET_ON(a, b)	    term_debug_ospf_packet[a] |= (b)
#define TERM_DEBUG_PACKET_OFF(a, b)	    term_debug_ospf_packet[a] &= ~(b)
#define DEBUG_PACKET_ON(a, b) \
    do { \
      CONF_DEBUG_PACKET_ON(a, b); \
      TERM_DEBUG_PACKET_ON(a, b); \
    } while (0)
#define DEBUG_PACKET_OFF(a, b) \
    do { \
      CONF_DEBUG_PACKET_OFF(a, b); \
      TERM_DEBUG_PACKET_OFF(a, b); \
    } while (0)

#define CONF_DEBUG_ON(a, b)	 conf_debug_ospf_ ## a |= (OSPF_DEBUG_ ## b)
#define CONF_DEBUG_OFF(a, b)	 conf_debug_ospf_ ## a &= ~(OSPF_DEBUG_ ## b)
#define TERM_DEBUG_ON(a, b)	 term_debug_ospf_ ## a |= (OSPF_DEBUG_ ## b)
#define TERM_DEBUG_OFF(a, b)	 term_debug_ospf_ ## a &= ~(OSPF_DEBUG_ ## b)
#define DEBUG_ON(a, b) \
     do { \
       CONF_DEBUG_ON(a, b); \
       TERM_DEBUG_ON(a, b); \
     } while (0)
#define DEBUG_OFF(a, b) \
     do { \
       CONF_DEBUG_OFF(a, b); \
       TERM_DEBUG_OFF(a, b); \
     } while (0)

/* Macro for checking debug option. */
#define IS_DEBUG_OSPF_PACKET(a, b) \
	(term_debug_ospf_packet[a] & OSPF_DEBUG_ ## b)
#define IS_DEBUG_OSPF(a, b) \
	(term_debug_ospf_ ## a & OSPF_DEBUG_ ## b)
#define IS_DEBUG_OSPF_EVENT IS_DEBUG_OSPF(event,EVENT)

#define IS_DEBUG_OSPF_NSSA  IS_DEBUG_OSPF(nssa,NSSA)

#define IS_CONF_DEBUG_OSPF_PACKET(a, b) \
	(conf_debug_ospf_packet[a] & OSPF_DEBUG_ ## b)
#define IS_CONF_DEBUG_OSPF(a, b) \
	(conf_debug_ospf_ ## a & OSPF_DEBUG_ ## b)

#ifdef ORIGINAL_CODING
#else /* ORIGINAL_CODING */
struct stream;
#endif /* ORIGINAL_CODING */

#define AREA_NAME(A)    ospf_area_name_string ((A))
#define IF_NAME(I)      ospf_if_name_string ((I))

/* Extern debug flag. */
extern unsigned long term_debug_ospf_packet[];
extern unsigned long term_debug_ospf_event;
extern unsigned long term_debug_ospf_ism;
extern unsigned long term_debug_ospf_nsm;
extern unsigned long term_debug_ospf_lsa;
extern unsigned long term_debug_ospf_zebra;
extern unsigned long term_debug_ospf_nssa;

/* Message Strings. */
extern char *ospf_lsa_type_str[];
extern const struct message ospf_auth_type_str[];
extern const size_t ospf_auth_type_str_max;

/* Prototypes. */
extern const char *ospf_area_name_string (struct ospf_area *);
extern const char *ospf_area_desc_string (struct ospf_area *);
extern const char *ospf_if_name_string (struct ospf_interface *);
extern void ospf_nbr_state_message (struct ospf_neighbor *, char *, size_t);
extern char *ospf_options_dump (u_char);
extern const char *ospf_timer_dump (struct thread *, char *, size_t);
extern const char *ospf_timeval_dump (struct timeval *, char *, size_t);
extern void ospf_ip_header_dump (struct ip *);
extern void ospf_packet_dump (struct stream *);
extern void ospf_lsa_header_dump (struct lsa_header *);
extern void debug_init (void);

/* Appropriate buffer size to use with ospf_timer_dump and ospf_timeval_dump: */
#define OSPF_TIME_DUMP_SIZE	16

#endif /* _ZEBRA_OSPF_DUMP_H */
