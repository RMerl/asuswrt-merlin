/*
 * Routing Information Base header
 * Copyright (C) 1997 Kunihiro Ishiguro
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

#ifndef _ZEBRA_RIB_H
#define _ZEBRA_RIB_H

#include "prefix.h"
#include "table.h"
#include "queue.h"

#define DISTANCE_INFINITY  255

/* Routing information base. */

union g_addr {
  struct in_addr ipv4;
#ifdef HAVE_IPV6
  struct in6_addr ipv6;
#endif /* HAVE_IPV6 */
};

struct rib
{
  /* Link list. */
  struct rib *next;
  struct rib *prev;
  
  /* Nexthop structure */
  struct nexthop *nexthop;
  
  /* Refrence count. */
  unsigned long refcnt;
  
  /* Uptime. */
  time_t uptime;

  /* Type fo this route. */
  int type;

  /* Which routing table */
  int table;			

  /* Metric */
  u_int32_t metric;

  /* Distance. */
  u_char distance;

  /* Flags of this route.
   * This flag's definition is in lib/zebra.h ZEBRA_FLAG_* and is exposed
   * to clients via Zserv
   */
  u_char flags;

  /* RIB internal status */
  u_char status;
#define RIB_ENTRY_REMOVED	(1 << 0)

  /* Nexthop information. */
  u_char nexthop_num;
  u_char nexthop_active_num;
  u_char nexthop_fib_num;
};

/* meta-queue structure:
 * sub-queue 0: connected, kernel
 * sub-queue 1: static
 * sub-queue 2: RIP, RIPng, OSPF, OSPF6, IS-IS
 * sub-queue 3: iBGP, eBGP
 * sub-queue 4: any other origin (if any)
 */
#define MQ_SIZE 5
struct meta_queue
{
  struct list *subq[MQ_SIZE];
  u_int32_t size; /* sum of lengths of all subqueues */
};

/*
 * Structure that represents a single destination (prefix).
 */
typedef struct rib_dest_t_
{

  /*
   * Back pointer to the route node for this destination. This helps
   * us get to the prefix that this structure is for.
   */
  struct route_node *rnode;

  /*
   * Doubly-linked list of routes for this prefix.
   */
  struct rib *routes;

  /*
   * Flags, see below.
   */
  u_int32_t flags;

  /*
   * Linkage to put dest on the FPM processing queue.
   */
  TAILQ_ENTRY(rib_dest_t_) fpm_q_entries;

} rib_dest_t;

#define RIB_ROUTE_QUEUED(x)	(1 << (x))

/*
 * The maximum qindex that can be used.
 */
#define ZEBRA_MAX_QINDEX        (MQ_SIZE - 1)

/*
 * This flag indicates that a given prefix has been 'advertised' to
 * the FPM to be installed in the forwarding plane.
 */
#define RIB_DEST_SENT_TO_FPM   (1 << (ZEBRA_MAX_QINDEX + 1))

/*
 * This flag is set when we need to send an update to the FPM about a
 * dest.
 */
#define RIB_DEST_UPDATE_FPM    (1 << (ZEBRA_MAX_QINDEX + 2))

/*
 * Macro to iterate over each route for a destination (prefix).
 */
#define RIB_DEST_FOREACH_ROUTE(dest, rib)				\
  for ((rib) = (dest) ? (dest)->routes : NULL; (rib); (rib) = (rib)->next)

/*
 * Same as above, but allows the current node to be unlinked.
 */
#define RIB_DEST_FOREACH_ROUTE_SAFE(dest, rib, next)	\
  for ((rib) = (dest) ? (dest)->routes : NULL;		\
       (rib) && ((next) = (rib)->next, 1);		\
       (rib) = (next))

#define RNODE_FOREACH_RIB(rn, rib)				\
  RIB_DEST_FOREACH_ROUTE (rib_dest_from_rnode (rn), rib)

#define RNODE_FOREACH_RIB_SAFE(rn, rib, next)				\
  RIB_DEST_FOREACH_ROUTE_SAFE (rib_dest_from_rnode (rn), rib, next)

/* Static route information. */
struct static_ipv4
{
  /* For linked list. */
  struct static_ipv4 *prev;
  struct static_ipv4 *next;

  /* Administrative distance. */
  u_char distance;

  /* Flag for this static route's type. */
  u_char type;
#define STATIC_IPV4_GATEWAY     1
#define STATIC_IPV4_IFNAME      2
#define STATIC_IPV4_BLACKHOLE   3

  /* Nexthop value. */
  union 
  {
    struct in_addr ipv4;
    char *ifname;
  } gate;

  /* bit flags */
  u_char flags;
/*
 see ZEBRA_FLAG_REJECT
     ZEBRA_FLAG_BLACKHOLE
 */
};

#ifdef HAVE_IPV6
/* Static route information. */
struct static_ipv6
{
  /* For linked list. */
  struct static_ipv6 *prev;
  struct static_ipv6 *next;

  /* Administrative distance. */
  u_char distance;

  /* Flag for this static route's type. */
  u_char type;
#define STATIC_IPV6_GATEWAY          1
#define STATIC_IPV6_GATEWAY_IFNAME   2
#define STATIC_IPV6_IFNAME           3

  /* Nexthop value. */
  struct in6_addr ipv6;
  char *ifname;

  /* bit flags */
  u_char flags;
/*
 see ZEBRA_FLAG_REJECT
     ZEBRA_FLAG_BLACKHOLE
 */
};
#endif /* HAVE_IPV6 */

enum nexthop_types_t
{
  NEXTHOP_TYPE_IFINDEX = 1,      /* Directly connected.  */
  NEXTHOP_TYPE_IFNAME,           /* Interface route.  */
  NEXTHOP_TYPE_IPV4,             /* IPv4 nexthop.  */
  NEXTHOP_TYPE_IPV4_IFINDEX,     /* IPv4 nexthop with ifindex.  */
  NEXTHOP_TYPE_IPV4_IFNAME,      /* IPv4 nexthop with ifname.  */
  NEXTHOP_TYPE_IPV6,             /* IPv6 nexthop.  */
  NEXTHOP_TYPE_IPV6_IFINDEX,     /* IPv6 nexthop with ifindex.  */
  NEXTHOP_TYPE_IPV6_IFNAME,      /* IPv6 nexthop with ifname.  */
  NEXTHOP_TYPE_BLACKHOLE,        /* Null0 nexthop.  */
};

/* Nexthop structure. */
struct nexthop
{
  struct nexthop *next;
  struct nexthop *prev;

  /* Interface index. */
  char *ifname;
  unsigned int ifindex;
  
  enum nexthop_types_t type;

  u_char flags;
#define NEXTHOP_FLAG_ACTIVE     (1 << 0) /* This nexthop is alive. */
#define NEXTHOP_FLAG_FIB        (1 << 1) /* FIB nexthop. */
#define NEXTHOP_FLAG_RECURSIVE  (1 << 2) /* Recursive nexthop. */
#define NEXTHOP_FLAG_ONLINK     (1 << 3) /* Nexthop should be installed onlink. */

  /* Nexthop address */
  union g_addr gate;
  union g_addr src;

  /* Nexthops obtained by recursive resolution.
   *
   * If the nexthop struct needs to be resolved recursively,
   * NEXTHOP_FLAG_RECURSIVE will be set in flags and the nexthops
   * obtained by recursive resolution will be added to `resolved'.
   * Only one level of recursive resolution is currently supported. */
  struct nexthop *resolved;
};

/* The following for loop allows to iterate over the nexthop
 * structure of routes.
 *
 * We have to maintain quite a bit of state:
 *
 * nexthop:   The pointer to the current nexthop, either in the
 *            top-level chain or in the resolved chain of ni.
 * tnexthop:  The pointer to the current nexthop in the top-level
 *            nexthop chain.
 * recursing: Information if nh currently is in the top-level chain
 *            (0) or in a resolved chain (1).
 *
 * Initialization: Set `nexthop' and `tnexthop' to the head of the
 * top-level chain. As nexthop is in the top level chain, set recursing
 * to 0.
 *
 * Iteration check: Check that the `nexthop' pointer is not NULL.
 *
 * Iteration step: This is the tricky part. Check if `nexthop' has
 * NEXTHOP_FLAG_RECURSIVE set. If yes, this implies that `nexthop' is in
 * the top level chain and has at least one nexthop attached to
 * `nexthop->resolved'. As we want to descend into `nexthop->resolved',
 * set `recursing' to 1 and set `nexthop' to `nexthop->resolved'.
 * `tnexthop' is left alone in that case so we can remember which nexthop
 * in the top level chain we are currently handling.
 *
 * If NEXTHOP_FLAG_RECURSIVE is not set, `nexthop' will progress in its
 * current chain. If we are recursing, `nexthop' will be set to
 * `nexthop->next' and `tnexthop' will be left alone. If we are not
 * recursing, both `tnexthop' and `nexthop' will be set to `nexthop->next'
 * as we are progressing in the top level chain.
 *   If we encounter `nexthop->next == NULL', we will clear the `recursing'
 * flag as we arived either at the end of the resolved chain or at the end
 * of the top level chain. In both cases, we set `tnexthop' and `nexthop'
 * to `tnexthop->next', progressing to the next position in the top-level
 * chain and possibly to its end marked by NULL.
 */
#define ALL_NEXTHOPS_RO(head, nexthop, tnexthop, recursing) \
  (tnexthop) = (nexthop) = (head), (recursing) = 0; \
  (nexthop); \
  (nexthop) = CHECK_FLAG((nexthop)->flags, NEXTHOP_FLAG_RECURSIVE) \
    ? (((recursing) = 1), (nexthop)->resolved) \
    : ((nexthop)->next ? ((recursing) ? (nexthop)->next \
                                      : ((tnexthop) = (nexthop)->next)) \
                       : (((recursing) = 0),((tnexthop) = (tnexthop)->next)))

/* Routing table instance.  */
struct vrf
{
  /* Identifier.  This is same as routing table vector index.  */
  u_int32_t id;

  /* Routing table name.  */
  char *name;

  /* Description.  */
  char *desc;

  /* FIB identifier.  */
  u_char fib_id;

  /* Routing table.  */
  struct route_table *table[AFI_MAX][SAFI_MAX];

  /* Static route configuration.  */
  struct route_table *stable[AFI_MAX][SAFI_MAX];
};

/*
 * rib_table_info_t
 *
 * Structure that is hung off of a route_table that holds information about
 * the table.
 */
typedef struct rib_table_info_t_
{

  /*
   * Back pointer to vrf.
   */
  struct vrf *vrf;
  afi_t afi;
  safi_t safi;

} rib_table_info_t;

typedef enum
{
  RIB_TABLES_ITER_S_INIT,
  RIB_TABLES_ITER_S_ITERATING,
  RIB_TABLES_ITER_S_DONE
} rib_tables_iter_state_t;

/*
 * Structure that holds state for iterating over all tables in the
 * Routing Information Base.
 */
typedef struct rib_tables_iter_t_
{
  uint32_t vrf_id;
  int afi_safi_ix;

  rib_tables_iter_state_t state;
} rib_tables_iter_t;

/* RPF lookup behaviour */
enum multicast_mode
{
  MCAST_NO_CONFIG = 0,	/* MIX_MRIB_FIRST, but no show in config write */
  MCAST_MRIB_ONLY,	/* MRIB only */
  MCAST_URIB_ONLY,	/* URIB only */
  MCAST_MIX_MRIB_FIRST,	/* MRIB, if nothing at all then URIB */
  MCAST_MIX_DISTANCE,	/* MRIB & URIB, lower distance wins */
  MCAST_MIX_PFXLEN,	/* MRIB & URIB, longer prefix wins */
			/* on equal value, MRIB wins for last 2 */
};

extern void multicast_mode_ipv4_set (enum multicast_mode mode);
extern enum multicast_mode multicast_mode_ipv4_get (void);

extern const char *nexthop_type_to_str (enum nexthop_types_t nh_type);
extern struct nexthop *nexthop_ifindex_add (struct rib *, unsigned int);
extern struct nexthop *nexthop_ifname_add (struct rib *, char *);
extern struct nexthop *nexthop_blackhole_add (struct rib *);
extern struct nexthop *nexthop_ipv4_add (struct rib *, struct in_addr *,
					 struct in_addr *);
extern struct nexthop *nexthop_ipv4_ifindex_add (struct rib *,
                                                 struct in_addr *,
                                                 struct in_addr *,
                                                 unsigned int);
extern int nexthop_has_fib_child(struct nexthop *);
extern void rib_lookup_and_dump (struct prefix_ipv4 *);
extern void rib_lookup_and_pushup (struct prefix_ipv4 *);
#define rib_dump(prefix ,rib) _rib_dump(__func__, prefix, rib)
extern void _rib_dump (const char *,
		       union prefix46constptr, const struct rib *);
extern int rib_lookup_ipv4_route (struct prefix_ipv4 *, union sockunion *);
#define ZEBRA_RIB_LOOKUP_ERROR -1
#define ZEBRA_RIB_FOUND_EXACT 0
#define ZEBRA_RIB_FOUND_NOGATE 1
#define ZEBRA_RIB_FOUND_CONNECTED 2
#define ZEBRA_RIB_NOTFOUND 3

#ifdef HAVE_IPV6
extern struct nexthop *nexthop_ipv6_add (struct rib *, struct in6_addr *);
#endif /* HAVE_IPV6 */

extern struct vrf *vrf_lookup (u_int32_t);
extern struct route_table *vrf_table (afi_t afi, safi_t safi, u_int32_t id);
extern struct route_table *vrf_static_table (afi_t afi, safi_t safi, u_int32_t id);

/* NOTE:
 * All rib_add_ipv[46]* functions will not just add prefix into RIB, but
 * also implicitly withdraw equal prefix of same type. */
extern int rib_add_ipv4 (int type, int flags, struct prefix_ipv4 *p, 
			 struct in_addr *gate, struct in_addr *src,
			 unsigned int ifindex, u_int32_t vrf_id,
			 u_int32_t, u_char, safi_t);

extern int rib_add_ipv4_multipath (struct prefix_ipv4 *, struct rib *, safi_t);

extern int rib_delete_ipv4 (int type, int flags, struct prefix_ipv4 *p,
		            struct in_addr *gate, unsigned int ifindex, 
		            u_int32_t, safi_t safi);

extern struct rib *rib_match_ipv4_safi (struct in_addr addr, safi_t safi,
					int skip_bgp, struct route_node **rn_out);
extern struct rib *rib_match_ipv4_multicast (struct in_addr addr,
					     struct route_node **rn_out);

extern struct rib *rib_lookup_ipv4 (struct prefix_ipv4 *);

extern void rib_update (void);
extern void rib_weed_tables (void);
extern void rib_sweep_route (void);
extern void rib_close (void);
extern void rib_init (void);
extern unsigned long rib_score_proto (u_char proto);

extern int
static_add_ipv4_safi (safi_t safi, struct prefix *p, struct in_addr *gate,
		      const char *ifname, u_char flags, u_char distance,
		      u_int32_t vrf_id);
extern int
static_delete_ipv4_safi (safi_t safi, struct prefix *p, struct in_addr *gate,
			 const char *ifname, u_char distance, u_int32_t vrf_id);

#ifdef HAVE_IPV6
extern int
rib_add_ipv6 (int type, int flags, struct prefix_ipv6 *p,
	      struct in6_addr *gate, unsigned int ifindex, u_int32_t vrf_id,
	      u_int32_t metric, u_char distance, safi_t safi);

extern int
rib_delete_ipv6 (int type, int flags, struct prefix_ipv6 *p,
		 struct in6_addr *gate, unsigned int ifindex, u_int32_t vrf_id, safi_t safi);

extern struct rib *rib_lookup_ipv6 (struct in6_addr *);

extern struct rib *rib_match_ipv6 (struct in6_addr *);

extern struct route_table *rib_table_ipv6;

extern int
static_add_ipv6 (struct prefix *p, u_char type, struct in6_addr *gate,
		 const char *ifname, u_char flags, u_char distance,
		 u_int32_t vrf_id);

extern int
static_delete_ipv6 (struct prefix *p, u_char type, struct in6_addr *gate,
		    const char *ifname, u_char distance, u_int32_t vrf_id);

#endif /* HAVE_IPV6 */

extern int rib_gc_dest (struct route_node *rn);
extern struct route_table *rib_tables_iter_next (rib_tables_iter_t *iter);

/*
 * Inline functions.
 */

/*
 * rib_table_info
 */
static inline rib_table_info_t *
rib_table_info (struct route_table *table)
{
  return (rib_table_info_t *) table->info;
}

/*
 * rib_dest_from_rnode
 */
static inline rib_dest_t *
rib_dest_from_rnode (struct route_node *rn)
{
  return (rib_dest_t *) rn->info;
}

/*
 * rnode_to_ribs
 *
 * Returns a pointer to the list of routes corresponding to the given
 * route_node.
 */
static inline struct rib *
rnode_to_ribs (struct route_node *rn)
{
  rib_dest_t *dest;

  dest = rib_dest_from_rnode (rn);
  if (!dest)
    return NULL;

  return dest->routes;
}

/*
 * rib_dest_prefix
 */
static inline struct prefix *
rib_dest_prefix (rib_dest_t *dest)
{
  return &dest->rnode->p;
}

/*
 * rib_dest_af
 *
 * Returns the address family that the destination is for.
 */
static inline u_char
rib_dest_af (rib_dest_t *dest)
{
  return dest->rnode->p.family;
}

/*
 * rib_dest_table
 */
static inline struct route_table *
rib_dest_table (rib_dest_t *dest)
{
  return dest->rnode->table;
}

/*
 * rib_dest_vrf
 */
static inline struct vrf *
rib_dest_vrf (rib_dest_t *dest)
{
  return rib_table_info (rib_dest_table (dest))->vrf;
}

/*
 * rib_tables_iter_init
 */
static inline void
rib_tables_iter_init (rib_tables_iter_t *iter)

{
  memset (iter, 0, sizeof (*iter));
  iter->state = RIB_TABLES_ITER_S_INIT;
}

/*
 * rib_tables_iter_started
 *
 * Returns TRUE if this iterator has started iterating over the set of
 * tables.
 */
static inline int
rib_tables_iter_started (rib_tables_iter_t *iter)
{
  return iter->state != RIB_TABLES_ITER_S_INIT;
}

/*
 * rib_tables_iter_cleanup
 */
static inline void
rib_tables_iter_cleanup (rib_tables_iter_t *iter)
{
  iter->state = RIB_TABLES_ITER_S_DONE;
}

#endif /*_ZEBRA_RIB_H */
