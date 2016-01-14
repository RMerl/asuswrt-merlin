/*
 * OSPF calculation.
 * Copyright (C) 1999 Kunihiro Ishiguro
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

#ifndef _QUAGGA_OSPF_SPF_H
#define _QUAGGA_OSPF_SPF_H

/* values for vertex->type */
#define OSPF_VERTEX_ROUTER  1  /* for a Router-LSA */
#define OSPF_VERTEX_NETWORK 2  /* for a Network-LSA */

/* values for vertex->flags */
#define OSPF_VERTEX_PROCESSED      0x01

/* The "root" is the node running the SPF calculation */

/* A router or network in an area */
struct vertex
{
  u_char flags;
  u_char type;		/* copied from LSA header */
  struct in_addr id;	/* copied from LSA header */
  struct lsa_header *lsa; /* Router or Network LSA */
  int *stat;		/* Link to LSA status. */
  u_int32_t distance;	/* from root to this vertex */  
  struct list *parents;		/* list of parents in SPF tree */
  struct list *children;	/* list of children in SPF tree*/
};

/* A nexthop taken on the root node to get to this (parent) vertex */
struct vertex_nexthop
{
  struct ospf_interface *oi;	/* output intf on root node */
  struct in_addr router;	/* router address to send to */
};

struct vertex_parent
{
  struct vertex_nexthop *nexthop; /* link to nexthop info for this parent */
  struct vertex *parent;	/* parent vertex */
  int backlink;			/* index back to parent for router-lsa's */
};

/* What triggered the SPF ? */
typedef enum {
  SPF_FLAG_ROUTER_LSA_INSTALL = 1,
  SPF_FLAG_NETWORK_LSA_INSTALL,
  SPF_FLAG_SUMMARY_LSA_INSTALL,
  SPF_FLAG_ASBR_SUMMARY_LSA_INSTALL,
  SPF_FLAG_MAXAGE,
  SPF_FLAG_ABR_STATUS_CHANGE,
  SPF_FLAG_ASBR_STATUS_CHANGE,
  SPF_FLAG_CONFIG_CHANGE,
} ospf_spf_reason_t;

extern void ospf_spf_calculate_schedule (struct ospf *, ospf_spf_reason_t);
extern void ospf_rtrs_free (struct route_table *);

/* void ospf_spf_calculate_timer_add (); */
#endif /* _QUAGGA_OSPF_SPF_H */
