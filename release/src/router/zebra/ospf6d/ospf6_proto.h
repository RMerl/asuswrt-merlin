/*
 * Common protocol data and data structures.
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

#ifndef OSPF6_PROTO_H
#define OSPF6_PROTO_H

/* OSPF protocol version */
#define OSPFV3_VERSION           3

/* OSPF protocol number. */
#ifndef IPPROTO_OSPFIGP
#define IPPROTO_OSPFIGP         89
#endif

/* TOS field normaly null */
#define DEFAULT_TOS_VALUE      0x0

/* Architectural Constants */
#define LS_REFRESH_TIME                1800  /* 30 min */
#define MIN_LS_INTERVAL                   5
#define MIN_LS_ARRIVAL                    1
#define MAXAGE                         3600  /* 1 hour */
#define CHECK_AGE                       300  /* 5 min */
#define MAX_AGE_DIFF                    900  /* 15 min */
#define LS_INFINITY                0xffffff  /* 24-bit binary value */
#define INITIAL_SEQUENCE_NUMBER  0x80000001  /* signed 32-bit integer */
#define MAX_SEQUENCE_NUMBER      0x7fffffff  /* signed 32-bit integer */

#define ALLSPFROUTERS6 "ff02::5"
#define ALLDROUTERS6   "ff02::6"

/* Configurable Constants */

#define DEFAULT_HELLO_INTERVAL       10
#define DEFAULT_ROUTER_DEAD_INTERVAL 40

#define OSPF6_ROUTER_BIT_W     (1 << 3)
#define OSPF6_ROUTER_BIT_V     (1 << 2)
#define OSPF6_ROUTER_BIT_E     (1 << 1)
#define OSPF6_ROUTER_BIT_B     (1 << 0)

/* OSPF options */
/* present in HELLO, DD, LSA */
#define OSPF6_OPT_SET(x,opt)   ((x)[2] |=  (opt))
#define OSPF6_OPT_ISSET(x,opt) ((x)[2] &   (opt))
#define OSPF6_OPT_CLEAR(x,opt) ((x)[2] &= ~(opt))
#define OSPF6_OPT_CLEAR_ALL(x) ((x)[0] = (x)[1] = (x)[2] = 0)

#define OSPF6_OPT_DC (1 << 5)   /* Demand Circuit handling Capability */
#define OSPF6_OPT_R  (1 << 4)   /* Forwarding Capability (Any Protocol) */
#define OSPF6_OPT_N  (1 << 3)   /* Handling Type-7 LSA Capability */
#define OSPF6_OPT_MC (1 << 2)   /* Multicasting Capability */
#define OSPF6_OPT_E  (1 << 1)   /* AS External Capability */
#define OSPF6_OPT_V6 (1 << 0)   /* IPv6 forwarding Capability */

/* OSPF6 Prefix */
struct ospf6_prefix
{
  u_int8_t prefix_length;
  u_int8_t prefix_options;
  union {
    u_int16_t _prefix_metric;
    u_int16_t _prefix_referenced_lstype;
  } u;
#define prefix_metric        u._prefix_metric
#define prefix_refer_lstype  u._prefix_referenced_lstype
  /* followed by one address_prefix */
};

#define OSPF6_PREFIX_OPTION_NU (1 << 0)  /* No Unicast */
#define OSPF6_PREFIX_OPTION_LA (1 << 1)  /* Local Address */
#define OSPF6_PREFIX_OPTION_MC (1 << 2)  /* MultiCast */
#define OSPF6_PREFIX_OPTION_P  (1 << 3)  /* Propagate (NSSA) */

/* caddr_t OSPF6_PREFIX_BODY (struct ospf6_prefix *); */
#define OSPF6_PREFIX_BODY(x) ((caddr_t)(x) + sizeof (struct ospf6_prefix))

/* size_t OSPF6_PREFIX_SPACE (int prefixlength); */
#define OSPF6_PREFIX_SPACE(x) ((((x) + 31) / 32) * 4)

/* size_t OSPF6_PREFIX_SIZE (struct ospf6_prefix *); */
#define OSPF6_PREFIX_SIZE(x) \
   (OSPF6_PREFIX_SPACE ((x)->prefix_length) + sizeof (struct ospf6_prefix))

/* struct ospf6_prefix *OSPF6_PREFIX_NEXT (struct ospf6_prefix *); */
#define OSPF6_PREFIX_NEXT(x) \
   ((struct ospf6_prefix *)((caddr_t)(x) + OSPF6_PREFIX_SIZE (x)))

#define ospf6_prefix_in6_addr(in6, op)                         \
do {                                                           \
  memset (in6, 0, sizeof (struct in6_addr));                   \
  memcpy (in6, (caddr_t) (op) + sizeof (struct ospf6_prefix),  \
          OSPF6_PREFIX_SPACE ((op)->prefix_length));           \
} while (0)

void ospf6_prefix_apply_mask (struct ospf6_prefix *op);
void ospf6_prefix_options_printbuf (u_int8_t prefix_options,
                                    char *buf, int size);
void ospf6_capability_printbuf (char capability, char *buf, int size);
void ospf6_options_printbuf (u_char *options, char *buf, int size);

#endif /* OSPF6_PROTO_H */

