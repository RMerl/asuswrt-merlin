/*
 * Routing Table
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

#ifndef _ZEBRA_TABLE_H
#define _ZEBRA_TABLE_H

/* Routing table top structure. */
struct route_table
{
  struct route_node *top;
};

/* Each routing entry. */
struct route_node
{
  /* Actual prefix of this radix. */
  struct prefix p;

  /* Tree link. */
  struct route_table *table;
  struct route_node *parent;
  struct route_node *link[2];
#define l_left   link[0]
#define l_right  link[1]

  /* Lock of this radix */
  unsigned int lock;

  /* Each node of route. */
  void *info;

  /* Aggregation. */
  void *aggregate;
};

/* Prototypes. */
struct route_table *route_table_init (void);
void route_table_finish (struct route_table *);
void route_unlock_node (struct route_node *node);
void route_node_delete (struct route_node *node);
struct route_node *route_top (struct route_table *);
struct route_node *route_next (struct route_node *);
struct route_node *route_next_until (struct route_node *, struct route_node *);
struct route_node *route_node_get (struct route_table *, struct prefix *);
struct route_node *route_node_lookup (struct route_table *, struct prefix *);
struct route_node *route_lock_node (struct route_node *node);
struct route_node *route_node_match (struct route_table *, struct prefix *);
struct route_node *route_node_match_ipv4 (struct route_table *,
					  struct in_addr *);
#ifdef HAVE_IPV6
struct route_node *route_node_match_ipv6 (struct route_table *,
					  struct in6_addr *);
#endif /* HAVE_IPV6 */

#endif /* _ZEBRA_TABLE_H */
