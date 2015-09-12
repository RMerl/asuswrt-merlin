/* BGP routing table
   Copyright (C) 1998, 2001 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#ifndef _QUAGGA_BGP_TABLE_H
#define _QUAGGA_BGP_TABLE_H

#include "table.h"

typedef enum
{
  BGP_TABLE_MAIN,
  BGP_TABLE_RSCLIENT,
} bgp_table_t;

struct bgp_table
{
  bgp_table_t type;
  
  /* afi/safi of this table */
  afi_t afi;
  safi_t safi;
  
  int lock;

  /* The owner of this 'bgp_table' structure. */
  struct peer *owner;

  struct route_table *route_table;
};

struct bgp_node
{
  /*
   * CAUTION
   *
   * These fields must be the very first fields in this structure.
   *
   * @see bgp_node_to_rnode
   * @see bgp_node_from_rnode
   */
  ROUTE_NODE_FIELDS;

  struct bgp_adj_out *adj_out;

  struct bgp_adj_in *adj_in;

  struct bgp_node *prn;

  u_char flags;
#define BGP_NODE_PROCESS_SCHEDULED	(1 << 0)
};

/*
 * bgp_table_iter_t
 * 
 * Structure that holds state for iterating over a bgp table.
 */
typedef struct bgp_table_iter_t_
{
  struct bgp_table *table;
  route_table_iter_t rt_iter;
} bgp_table_iter_t;

extern struct bgp_table *bgp_table_init (afi_t, safi_t);
extern void bgp_table_lock (struct bgp_table *);
extern void bgp_table_unlock (struct bgp_table *);
extern void bgp_table_finish (struct bgp_table **);


/*
 * bgp_node_from_rnode
 *
 * Returns the bgp_node structure corresponding to a route_node.
 */
static inline struct bgp_node *
bgp_node_from_rnode (struct route_node *rnode)
{
  return (struct bgp_node *) rnode;
}

/*
 * bgp_node_to_rnode
 *
 * Returns the route_node structure corresponding to a bgp_node.
 */
static inline struct route_node *
bgp_node_to_rnode (struct bgp_node *node)
{
  return (struct route_node *) node;
}

/*
 * bgp_node_table
 *
 * Returns the bgp_table that the given node is in.
 */
static inline struct bgp_table *
bgp_node_table (struct bgp_node *node)
{
  return bgp_node_to_rnode (node)->table->info;
}

/*
 * bgp_node_info
 *
 * Returns the 'info' pointer corresponding to a bgp node.
 */
static inline void *
bgp_node_info (const struct bgp_node *node)
{
  return node->info;
}

/*
 * bgp_node_set_info
 */
static inline void
bgp_node_set_info (struct bgp_node *node, void *info)
{
  node->info = info;
}

/*
 * bgp_node_prefix
 */
static inline struct prefix *
bgp_node_prefix (struct bgp_node *node)
{
  return &node->p;
}

/*
 * bgp_node_prefixlen
 */
static inline u_char
bgp_node_prefixlen (struct bgp_node *node)
{
  return bgp_node_prefix (node)->prefixlen;
}

/*
 * bgp_node_parent_nolock
 *
 * Gets the parent node of the given node without locking it.
 */
static inline struct bgp_node *
bgp_node_parent_nolock (struct bgp_node *node)
{
  return bgp_node_from_rnode (node->parent);
}

/*
 * bgp_unlock_node
 */
static inline void
bgp_unlock_node (struct bgp_node *node)
{
  route_unlock_node (bgp_node_to_rnode (node));
}

/*
 * bgp_table_top_nolock
 *
 * Gets the top node in the table without locking it.
 *
 * @see bgp_table_top
 */
static inline struct bgp_node *
bgp_table_top_nolock (const struct bgp_table *const table)
{
  return bgp_node_from_rnode (table->route_table->top);
}

/*
 * bgp_table_top
 */
static inline struct bgp_node *
bgp_table_top (const struct bgp_table *const table)
{
  return bgp_node_from_rnode (route_top (table->route_table));
}

/*
 * bgp_route_next
 */
static inline struct bgp_node *
bgp_route_next (struct bgp_node *node)
{
  return bgp_node_from_rnode (route_next (bgp_node_to_rnode (node)));
}

/*
 * bgp_route_next_until
 */
static inline struct bgp_node *
bgp_route_next_until (struct bgp_node *node, struct bgp_node *limit)
{
  struct route_node *rnode;

  rnode = route_next_until (bgp_node_to_rnode (node),
			    bgp_node_to_rnode (limit));
  return bgp_node_from_rnode (rnode);
}

/*
 * bgp_node_get
 */
static inline struct bgp_node *
bgp_node_get (struct bgp_table *const table, struct prefix *p)
{
  return bgp_node_from_rnode (route_node_get (table->route_table, p));
}

/*
 * bgp_node_lookup
 */
static inline struct bgp_node *
bgp_node_lookup (const struct bgp_table *const table, struct prefix *p)
{
  return bgp_node_from_rnode (route_node_lookup (table->route_table, p));
}

/*
 * bgp_lock_node
 */
static inline struct bgp_node *
bgp_lock_node (struct bgp_node *node)
{
  return bgp_node_from_rnode (route_lock_node (bgp_node_to_rnode (node)));
}

/*
 * bgp_node_match
 */
static inline struct bgp_node *
bgp_node_match (const struct bgp_table *table, struct prefix *p)
{
  return bgp_node_from_rnode (route_node_match (table->route_table, p));
}

/*
 * bgp_node_match_ipv4
 */
static inline struct bgp_node *
bgp_node_match_ipv4 (const struct bgp_table *table, struct in_addr *addr)
{
  return bgp_node_from_rnode (route_node_match_ipv4 (table->route_table, 
						     addr));
}

#ifdef HAVE_IPV6

/*
 * bgp_node_match_ipv6
 */
static inline struct bgp_node *
bgp_node_match_ipv6 (const struct bgp_table *table, struct in6_addr *addr)
{
  return bgp_node_from_rnode (route_node_match_ipv6 (table->route_table,
						     addr));
}

#endif /* HAVE_IPV6 */

static inline unsigned long
bgp_table_count (const struct bgp_table *const table)
{
  return route_table_count (table->route_table);
}

/*
 * bgp_table_get_next
 */
static inline struct bgp_node *
bgp_table_get_next (const struct bgp_table *table, struct prefix *p)
{
  return bgp_node_from_rnode (route_table_get_next (table->route_table, p));
}

/*
 * bgp_table_iter_init
 */
static inline void
bgp_table_iter_init (bgp_table_iter_t * iter, struct bgp_table *table)
{
  bgp_table_lock (table);
  iter->table = table;
  route_table_iter_init (&iter->rt_iter, table->route_table);
}

/*
 * bgp_table_iter_next
 */
static inline struct bgp_node *
bgp_table_iter_next (bgp_table_iter_t * iter)
{
  return bgp_node_from_rnode (route_table_iter_next (&iter->rt_iter));
}

/*
 * bgp_table_iter_cleanup
 */
static inline void
bgp_table_iter_cleanup (bgp_table_iter_t * iter)
{
  route_table_iter_cleanup (&iter->rt_iter);
  bgp_table_unlock (iter->table);
  iter->table = NULL;
}

/*
 * bgp_table_iter_pause
 */
static inline void
bgp_table_iter_pause (bgp_table_iter_t * iter)
{
  route_table_iter_pause (&iter->rt_iter);
}

/*
 * bgp_table_iter_is_done
 */
static inline int
bgp_table_iter_is_done (bgp_table_iter_t * iter)
{
  return route_table_iter_is_done (&iter->rt_iter);
}

/*
 * bgp_table_iter_started
 */
static inline int
bgp_table_iter_started (bgp_table_iter_t * iter)
{
  return route_table_iter_started (&iter->rt_iter);
}

#endif /* _QUAGGA_BGP_TABLE_H */
