/*
 * OSPF Link State Advertisement
 * Copyright (C) 1999, 2000 Toshiaki Takada
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

#ifndef _ZEBRA_OSPF_LSA_H
#define _ZEBRA_OSPF_LSA_H

/* OSPF LSA Range definition. */
#define OSPF_MIN_LSA		1  /* begin range here */
#if defined (HAVE_OPAQUE_LSA)
#define OSPF_MAX_LSA           12
#elif defined (HAVE_NSSA)
#define OSPF_MAX_LSA		8
#else
#define OSPF_MAX_LSA		6
#endif

/* OSPF LSA Type definition. */
#define OSPF_UNKNOWN_LSA	      0
#define OSPF_ROUTER_LSA               1
#define OSPF_NETWORK_LSA              2
#define OSPF_SUMMARY_LSA              3
#define OSPF_ASBR_SUMMARY_LSA         4
#define OSPF_AS_EXTERNAL_LSA          5
#define OSPF_GROUP_MEMBER_LSA	      6  /* Not supported. */
#define OSPF_AS_NSSA_LSA	              7
#define OSPF_EXTERNAL_ATTRIBUTES_LSA  8  /* Not supported. */
#define OSPF_OPAQUE_LINK_LSA	      9
#define OSPF_OPAQUE_AREA_LSA	     10
#define OSPF_OPAQUE_AS_LSA	     11

#define OSPF_LSA_HEADER_SIZE	20
#define OSPF_MAX_LSA_SIZE	1500

/* AS-external-LSA refresh method. */
#define LSA_REFRESH_IF_CHANGED	0
#define LSA_REFRESH_FORCE	1

/* OSPF LSA header. */
struct lsa_header
{
  u_int16_t ls_age;
  u_char options;
  u_char type;
  struct in_addr id;
  struct in_addr adv_router;
  int ls_seqnum;
  u_int16_t checksum;
  u_int16_t length;
};

/* OSPF LSA. */
struct ospf_lsa
{
  /* LSA origination flag. */
  u_char flags;
#define OSPF_LSA_SELF		  0x01
#define OSPF_LSA_SELF_CHECKED	  0x02
#define OSPF_LSA_RECEIVED	  0x04
#define OSPF_LSA_APPROVED	  0x08
#define OSPF_LSA_DISCARD	  0x10
#ifdef HAVE_NSSA
#define OSPF_LSA_LOCAL_XLT	  0x20
#endif /* HAVE_NSSA */

  /* LSA data. */
  struct lsa_header *data;

  /* Received time stamp. */
  struct timeval tv_recv;

  /* Last time it was originated */
  struct timeval tv_orig;

  /* All of reference count, also lock to remove. */
  int lock;

  /* References to this LSA in neighbor retransmission lists*/
  int retransmit_counter;

  /* Area the LSA belongs to, may be NULL if AS-external-LSA. */
  struct ospf_area *area;

  /* Parent LSDB. */
  struct ospf_lsdb *lsdb;

  /* Related Route. */
  void *route;

  /* Refreshement List or Queue */
  int refresh_list;

#ifdef HAVE_OPAQUE_LSA
  /* For Type-9 Opaque-LSAs, reference to ospf-interface is required. */
  struct ospf_interface *oi;
#endif /* HAVE_OPAQUE_LSA */
};

/* OSPF LSA Link Type. */
#define LSA_LINK_TYPE_POINTOPOINT      1
#define LSA_LINK_TYPE_TRANSIT          2
#define LSA_LINK_TYPE_STUB             3
#define LSA_LINK_TYPE_VIRTUALLINK      4

/* OSPF Router LSA Flag. */
#define ROUTER_LSA_BORDER	       0x01 /* The router is an ABR */
#define ROUTER_LSA_EXTERNAL	       0x02 /* The router is an ASBR */
#define ROUTER_LSA_VIRTUAL	       0x04 /* The router has a VL in this area */
#define ROUTER_LSA_NT		       0x10 /* NSSA-specific flag */
#define ROUTER_LSA_SHORTCUT	       0x20 /* Shortcut-ABR specific flag */

#define IS_ROUTER_LSA_VIRTUAL(x)       ((x)->flags & ROUTER_LSA_VIRTUAL)
#define IS_ROUTER_LSA_EXTERNAL(x)      ((x)->flags & ROUTER_LSA_EXTERNAL)
#define IS_ROUTER_LSA_BORDER(x)	       ((x)->flags & ROUTER_LSA_BORDER)
#define IS_ROUTER_LSA_SHORTCUT(x)      ((x)->flags & ROUTER_LSA_SHORTCUT)

/* OSPF Router-LSA Link information. */
struct router_lsa_link
{
  struct in_addr link_id;
  struct in_addr link_data;
  struct
  {
    u_char type;
    u_char tos_count;
    u_int16_t metric;
  } m[1];
};

/* OSPF Router-LSAs structure. */
struct router_lsa
{
  struct lsa_header header;
  u_char flags;
  u_char zero;
  u_int16_t links;
  struct
  {
    struct in_addr link_id;
    struct in_addr link_data;
    u_char type;
    u_char tos;
    u_int16_t metric;
  } link[1];
};

/* OSPF Network-LSAs structure. */
struct network_lsa
{
  struct lsa_header header;
  struct in_addr mask;
  struct in_addr routers[1];
};

/* OSPF Summary-LSAs structure. */
struct summary_lsa
{
  struct lsa_header header;
  struct in_addr mask;
  u_char tos;
  u_char metric[3];
};

/* OSPF AS-external-LSAs structure. */
struct as_external_lsa
{
  struct lsa_header header;
  struct in_addr mask;
  struct
  {
    u_char tos;
    u_char metric[3];
    struct in_addr fwd_addr;
    u_int32_t route_tag;
  } e[1];
};

#ifdef HAVE_OPAQUE_LSA
#include "ospfd/ospf_opaque.h"
#endif /* HAVE_OPAQUE_LSA */

/* Macros. */
#define GET_METRIC(x) get_metric(x)
#define IS_EXTERNAL_METRIC(x)   ((x) & 0x80)

#define GET_AGE(x)     (ntohs ((x)->data->ls_age) + time (NULL) - (x)->tv_recv)
#define LS_AGE(x)      (OSPF_LSA_MAXAGE < get_age(x) ? \
                                           OSPF_LSA_MAXAGE : get_age(x))
#define IS_LSA_SELF(L)          (CHECK_FLAG ((L)->flags, OSPF_LSA_SELF))
#define IS_LSA_MAXAGE(L)        (LS_AGE ((L)) == OSPF_LSA_MAXAGE)

#define OSPF_LSA_UPDATE_DELAY		2

#define OSPF_LSA_UPDATE_TIMER_ON(T,F) \
      if (!(T)) \
        (T) = thread_add_timer (master, (F), 0, 2)

struct ospf_route;
struct ospf_lsdb;

/* Prototypes. */
struct timeval tv_adjust (struct timeval);
int tv_ceil (struct timeval);
int tv_floor (struct timeval);
struct timeval int2tv (int);
struct timeval tv_add (struct timeval, struct timeval);
struct timeval tv_sub (struct timeval, struct timeval);
int tv_cmp (struct timeval, struct timeval);

int get_age (struct ospf_lsa *);
u_int16_t ospf_lsa_checksum (struct lsa_header *);

struct stream;
const char *dump_lsa_key (struct ospf_lsa *);
u_int32_t lsa_seqnum_increment (struct ospf_lsa *);
void lsa_header_set (struct stream *, u_char, u_char, struct in_addr,
		     struct in_addr);
struct ospf_neighbor *ospf_nbr_lookup_ptop (struct ospf_interface *);

/* Prototype for LSA primitive. */
struct ospf_lsa *ospf_lsa_new ();
struct ospf_lsa *ospf_lsa_dup ();
void ospf_lsa_free (struct ospf_lsa *);
struct ospf_lsa *ospf_lsa_lock (struct ospf_lsa *);
void ospf_lsa_unlock (struct ospf_lsa *);
void ospf_lsa_discard (struct ospf_lsa *);

struct lsa_header *ospf_lsa_data_new (size_t);
struct lsa_header *ospf_lsa_data_dup (struct lsa_header *);
void ospf_lsa_data_free (struct lsa_header *);

/* Prototype for various LSAs */
struct ospf_lsa *ospf_router_lsa_originate (struct ospf_area *);
int ospf_router_lsa_update_timer (struct thread *);
void ospf_router_lsa_timer_add (struct ospf_area *);

int ospf_network_lsa_refresh (struct ospf_lsa *, struct ospf_interface *);
void ospf_network_lsa_timer_add (struct ospf_interface *);

struct ospf_lsa *ospf_summary_lsa_originate (struct prefix_ipv4 *, u_int32_t,
					     struct ospf_area *);
struct ospf_lsa *ospf_summary_asbr_lsa_originate (struct prefix_ipv4 *,
						  u_int32_t,
						  struct ospf_area *);
struct ospf_lsa *ospf_summary_lsa_refresh (struct ospf *, struct ospf_lsa *);
struct ospf_lsa *ospf_summary_asbr_lsa_refresh (struct ospf *, struct ospf_lsa *);

struct ospf_lsa *ospf_lsa_install (struct ospf *,
				   struct ospf_interface *, struct ospf_lsa *);

void ospf_nssa_lsa_flush (struct ospf *ospf, struct prefix_ipv4 *p);
void ospf_external_lsa_flush (struct ospf *, u_char, struct prefix_ipv4 *,
			      unsigned int, struct in_addr);

struct in_addr ospf_get_ip_from_ifp (struct ospf_interface *);

struct ospf_lsa *ospf_external_lsa_originate (struct ospf *, struct external_info *);
int ospf_external_lsa_originate_timer (struct thread *);
struct ospf_lsa *ospf_lsa_lookup (struct ospf_area *, u_int32_t,
				  struct in_addr, struct in_addr);
struct ospf_lsa *ospf_lsa_lookup_by_id (struct ospf_area *,u_int32_t, struct in_addr);
struct ospf_lsa *ospf_lsa_lookup_by_header (struct ospf_area *,
					    struct lsa_header *);
int ospf_lsa_more_recent (struct ospf_lsa *, struct ospf_lsa *);
int ospf_lsa_different (struct ospf_lsa *, struct ospf_lsa *);
void ospf_flush_self_originated_lsas_now (struct ospf *);

int ospf_lsa_is_self_originated (struct ospf *, struct ospf_lsa *);

struct ospf_lsa *ospf_lsa_lookup_by_prefix (struct ospf_lsdb *, u_char,
					    struct prefix_ipv4 *,
					    struct in_addr);

void ospf_lsa_maxage (struct ospf *, struct ospf_lsa *);
u_int32_t get_metric (u_char *);

int ospf_lsa_maxage_walker (struct thread *);

void ospf_external_lsa_refresh_default (struct ospf *);

void ospf_external_lsa_refresh_type (struct ospf *, u_char, int);
void ospf_external_lsa_refresh (struct ospf *, struct ospf_lsa *,
				struct external_info *, int);
struct in_addr ospf_lsa_unique_id (struct ospf *, struct ospf_lsdb *, u_char,
				   struct prefix_ipv4 *);
void ospf_schedule_lsa_flood_area (struct ospf_area *, struct ospf_lsa *);
void ospf_schedule_lsa_flush_area (struct ospf_area *, struct ospf_lsa *);

void ospf_refresher_register_lsa (struct ospf *, struct ospf_lsa *);
void ospf_refresher_unregister_lsa (struct ospf *, struct ospf_lsa *);
int ospf_lsa_refresh_walker (struct thread *);

void ospf_lsa_maxage_delete (struct ospf *, struct ospf_lsa *);

void ospf_discard_from_db (struct ospf *, struct ospf_lsdb *, struct ospf_lsa*);
int is_prefix_default (struct prefix_ipv4 *);

int metric_type (struct ospf *, u_char);
int metric_value (struct ospf *, u_char);

#ifdef HAVE_NSSA
struct in_addr ospf_get_nssa_ip (struct ospf_area *);
#endif /* HAVE NSSA */

#endif /* _ZEBRA_OSPF_LSA_H */
