/*
 * RIPng related value and structure.
 * Copyright (C) 1998 Kunihiro Ishiguro
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
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#ifndef _ZEBRA_RIPNG_RIPNGD_H
#define _ZEBRA_RIPNG_RIPNGD_H

/* RIPng version and port number. */
#define RIPNG_V1                         1
#define RIPNG_PORT_DEFAULT             521
#define RIPNG_VTY_PORT                2603
#define RIPNG_VTYSH_PATH              "/tmp/.ripngd"
#define RIPNG_MAX_PACKET_SIZE         1500
#define RIPNG_PRIORITY_DEFAULT           0

/* RIPng commands. */
#define RIPNG_REQUEST                    1
#define RIPNG_RESPONSE                   2

/* RIPng metric and multicast group address. */
#define RIPNG_METRIC_INFINITY           16
#define RIPNG_METRIC_NEXTHOP          0xff
#define RIPNG_GROUP              "ff02::9"

/* RIPng timers. */
#define RIPNG_UPDATE_TIMER_DEFAULT      30
#define RIPNG_TIMEOUT_TIMER_DEFAULT    180
#define RIPNG_GARBAGE_TIMER_DEFAULT    120

/* Default config file name. */
#define RIPNG_DEFAULT_CONFIG "ripngd.conf"

/* RIPng route types. */
#define RIPNG_ROUTE_RTE                  0
#define RIPNG_ROUTE_STATIC               1
#define RIPNG_ROUTE_AGGREGATE            2

/* Interface send/receive configuration. */
#define RIPNG_SEND_UNSPEC                0
#define RIPNG_SEND_OFF                   1
#define RIPNG_RECEIVE_UNSPEC             0
#define RIPNG_RECEIVE_OFF                1

/* Split horizon definitions. */
#define RIPNG_SPLIT_HORIZON_UNSPEC       0
#define RIPNG_SPLIT_HORIZON_NONE         1
#define RIPNG_SPLIT_HORIZON              2
#define RIPNG_SPLIT_HORIZON_POISONED     3

/* RIP default route's accept/announce methods. */
#define RIPNG_DEFAULT_ADVERTISE_UNSPEC   0
#define RIPNG_DEFAULT_ADVERTISE_NONE     1
#define RIPNG_DEFAULT_ADVERTISE          2

#define RIPNG_DEFAULT_ACCEPT_UNSPEC      0
#define RIPNG_DEFAULT_ACCEPT_NONE        1
#define RIPNG_DEFAULT_ACCEPT             2

/* Default value for "default-metric" command. */
#define RIPNG_DEFAULT_METRIC_DEFAULT     1

/* For max RTE calculation. */
#ifndef IPV6_HDRLEN
#define IPV6_HDRLEN 40
#endif /* IPV6_HDRLEN */

#ifndef IFMINMTU
#define IFMINMTU    576
#endif /* IFMINMTU */

/* RIPng structure. */
struct ripng 
{
  /* RIPng socket. */
  int sock;

  /* RIPng Parameters.*/
  u_char command;
  u_char version;
  unsigned long update_time;
  unsigned long timeout_time;
  unsigned long garbage_time;
  int max_mtu;
  int default_metric;
  int default_information;

  /* Input/output buffer of RIPng. */
  struct stream *ibuf;
  struct stream *obuf;

  /* RIPng routing information base. */
  struct route_table *table;

  /* RIPng only static route information. */
  struct route_table *route;

  /* RIPng aggregate route information. */
  struct route_table *aggregate;

  /* RIPng threads. */
  struct thread *t_read;
  struct thread *t_write;
  struct thread *t_update;
  struct thread *t_garbage;
  struct thread *t_zebra;

  /* Triggered update hack. */
  int trigger;
  struct thread *t_triggered_update;
  struct thread *t_triggered_interval;

  /* For redistribute route map. */
  struct
  {
    char *name;
    struct route_map *map;
    int metric_config;
    u_int32_t metric;
  } route_map[ZEBRA_ROUTE_MAX];
};

/* Routing table entry. */
struct rte
{
  struct in6_addr addr;
  u_short tag;
  u_char prefixlen;
  u_char metric;
};

/* RIPNG send packet. */
struct ripng_packet
{
  u_char command;
  u_char version;
  u_int16_t zero; 
  struct rte rte[1];
};

/* Each route's information. */
struct ripng_info
{
  /* This route's type.  Static, ripng or aggregate. */
  u_char type;

  /* Sub type for static route. */
  u_char sub_type;

  /* RIPng specific information */
  struct in6_addr nexthop;	
  struct in6_addr from;

  /* Which interface does this route come from. */
  unsigned int ifindex;		

  /* Metric of this route.  */
  u_char metric;		

  /* Tag field of RIPng packet.*/
  u_int16_t tag;		

  /* For aggregation. */
  unsigned int suppress;

  /* Flags of RIPng route. */
#define RIPNG_RTF_FIB      1
#define RIPNG_RTF_CHANGED  2
  u_char flags;

  /* Garbage collect timer. */
  struct thread *t_timeout;
  struct thread *t_garbage_collect;

  /* Route-map features - this variables can be changed. */
  u_char metric_set;

  struct route_node *rp;
};

/* RIPng tag structure. */
struct ripng_tag
{
  /* Tag value. */
  u_int16_t tag;

  /* Port. */
  u_int16_t port;

  /* Multicast group. */
  struct in6_addr maddr;

  /* Table number. */
  int table;

  /* Distance. */
  int distance;

  /* Split horizon. */
  u_char split_horizon;

  /* Poison reverse. */
  u_char poison_reverse;
};

/* RIPng specific interface configuration. */
struct ripng_interface
{
  /* RIPng is enabled on this interface. */
  int enable_network;
  int enable_interface;

  /* RIPng is running on this interface. */
  int running;

  /* For filter type slot. */
#define RIPNG_FILTER_IN  0
#define RIPNG_FILTER_OUT 1
#define RIPNG_FILTER_MAX 2

  /* Access-list. */
  struct access_list *list[RIPNG_FILTER_MAX];

  /* Prefix-list. */
  struct prefix_list *prefix[RIPNG_FILTER_MAX];

  /* Route-map. */
  struct route_map *routemap[RIPNG_FILTER_MAX];

  /* RIPng tag configuration. */
  struct ripng_tag *rtag;

  /* Default information originate. */
  u_char default_originate;

  /* Default information only. */
  u_char default_only;

  /* Wake up thread. */
  struct thread *t_wakeup;

  /* Passive interface. */
  int passive;
};

/* All RIPng events. */
enum ripng_event
{
  RIPNG_READ,
  RIPNG_ZEBRA,
  RIPNG_REQUEST_EVENT,
  RIPNG_UPDATE_EVENT,
  RIPNG_TRIGGERED_UPDATE,
};

/* RIPng timer on/off macro. */
#define RIPNG_TIMER_ON(T,F,V) \
do { \
   if (!(T)) \
      (T) = thread_add_timer (master, (F), rinfo, (V)); \
} while (0)

#define RIPNG_TIMER_OFF(T) \
do { \
   if (T) \
     { \
       thread_cancel(T); \
       (T) = NULL; \
     } \
} while (0)

/* Count prefix size from mask length */
#define PSIZE(a) (((a) + 7) / (8))

/* Extern variables. */
extern struct ripng *ripng;

extern struct thread_master *master;

/* Prototypes. */
void ripng_init ();
void ripng_if_init ();
void ripng_terminate ();
void ripng_zclient_start ();
void zebra_init ();
struct ripng_info * ripng_info_new ();
void ripng_info_free (struct ripng_info *rinfo);
void ripng_event (enum ripng_event, int);
int ripng_request (struct interface *ifp);
void ripng_redistribute_add (int, int, struct prefix_ipv6 *, unsigned int);
void ripng_redistribute_delete (int, int, struct prefix_ipv6 *, unsigned int);
void ripng_redistribute_withdraw (int type);

void ripng_distribute_update_interface (struct interface *);
void ripng_if_rmap_update_interface (struct interface *);

void ripng_zebra_ipv6_add (struct prefix_ipv6 *p, struct in6_addr *nexthop, unsigned int ifindex);
void ripng_zebra_ipv6_delete (struct prefix_ipv6 *p, struct in6_addr *nexthop, unsigned int ifindex);
void ripng_route_map_init ();

#endif /* _ZEBRA_RIPNG_RIPNGD_H */
