/*
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

#ifndef OSPF6_ROUTE_H
#define OSPF6_ROUTE_H

#define OSPF6_MULTI_PATH_LIMIT    4

/* Debug option */
extern unsigned char conf_debug_ospf6_route;
#define OSPF6_DEBUG_ROUTE_TABLE   0x01
#define OSPF6_DEBUG_ROUTE_INTRA   0x02
#define OSPF6_DEBUG_ROUTE_INTER   0x04
#define OSPF6_DEBUG_ROUTE_ON(level) \
  (conf_debug_ospf6_route |= (level))
#define OSPF6_DEBUG_ROUTE_OFF(level) \
  (conf_debug_ospf6_route &= ~(level))
#define IS_OSPF6_DEBUG_ROUTE(e) \
  (conf_debug_ospf6_route & OSPF6_DEBUG_ROUTE_ ## e)

/* Nexthop */
struct ospf6_nexthop
{
  /* Interface index */
  unsigned int ifindex;

  /* IP address, if any */
  struct in6_addr address;
};

#define ospf6_nexthop_is_set(x)                                \
  ((x)->ifindex || ! IN6_IS_ADDR_UNSPECIFIED (&(x)->address))
#define ospf6_nexthop_is_same(a,b)                             \
  ((a)->ifindex == (b)->ifindex &&                            \
   IN6_ARE_ADDR_EQUAL (&(a)->address, &(b)->address))
#define ospf6_nexthop_clear(x)                                \
  do {                                                        \
    (x)->ifindex = 0;                                         \
    memset (&(x)->address, 0, sizeof (struct in6_addr));      \
  } while (0)
#define ospf6_nexthop_copy(a, b)                              \
  do {                                                        \
    (a)->ifindex = (b)->ifindex;                              \
    memcpy (&(a)->address, &(b)->address,                     \
            sizeof (struct in6_addr));                        \
  } while (0)

/* Path */
struct ospf6_ls_origin
{
  u_int16_t type;
  u_int32_t id;
  u_int32_t adv_router;
};

struct ospf6_path
{
  /* Link State Origin */
  struct ospf6_ls_origin origin;

  /* Router bits */
  u_char router_bits;

  /* Optional Capabilities */
  u_char options[3];

  /* Prefix Options */
  u_char prefix_options;

  /* Associated Area */
  u_int32_t area_id;

  /* Path-type */
  u_char type;
  u_char subtype; /* only used for redistribute i.e ZEBRA_ROUTE_XXX */

  /* Cost */
  u_int8_t metric_type;
  u_int32_t cost;
  u_int32_t cost_e2;
};

#define OSPF6_PATH_TYPE_NONE         0
#define OSPF6_PATH_TYPE_INTRA        1
#define OSPF6_PATH_TYPE_INTER        2
#define OSPF6_PATH_TYPE_EXTERNAL1    3
#define OSPF6_PATH_TYPE_EXTERNAL2    4
#define OSPF6_PATH_TYPE_REDISTRIBUTE 5
#define OSPF6_PATH_TYPE_MAX          6

#include "prefix.h"
#include "table.h"

struct ospf6_route
{
  struct route_node *rnode;

  struct ospf6_route *prev;
  struct ospf6_route *next;

  unsigned int lock;

  /* Destination Type */
  u_char type;

  /* Destination ID */
  struct prefix prefix;

  /* Time */
  struct timeval installed;
  struct timeval changed;

  /* flag */
  u_char flag;

  /* path */
  struct ospf6_path path;

  /* nexthop */
  struct ospf6_nexthop nexthop[OSPF6_MULTI_PATH_LIMIT];

  /* route option */
  void *route_option;

  /* link state id for advertising */
  u_int32_t linkstate_id;
};

#define OSPF6_DEST_TYPE_NONE       0
#define OSPF6_DEST_TYPE_ROUTER     1
#define OSPF6_DEST_TYPE_NETWORK    2
#define OSPF6_DEST_TYPE_DISCARD    3
#define OSPF6_DEST_TYPE_LINKSTATE  4
#define OSPF6_DEST_TYPE_RANGE      5
#define OSPF6_DEST_TYPE_MAX        6

#define OSPF6_ROUTE_CHANGE           0x01
#define OSPF6_ROUTE_ADD              0x02
#define OSPF6_ROUTE_REMOVE           0x04
#define OSPF6_ROUTE_BEST             0x08
#define OSPF6_ROUTE_ACTIVE_SUMMARY   0x10
#define OSPF6_ROUTE_DO_NOT_ADVERTISE 0x20
#define OSPF6_ROUTE_WAS_REMOVED      0x40

struct ospf6_route_table
{
  /* patricia tree */
  struct route_table *table;

  u_int32_t count;

  /* hooks */
  void (*hook_add) (struct ospf6_route *);
  void (*hook_change) (struct ospf6_route *);
  void (*hook_remove) (struct ospf6_route *);
};

extern char *ospf6_dest_type_str[OSPF6_DEST_TYPE_MAX];
extern char *ospf6_dest_type_substr[OSPF6_DEST_TYPE_MAX];
#define OSPF6_DEST_TYPE_NAME(x)                       \
  (0 < (x) && (x) < OSPF6_DEST_TYPE_MAX ?             \
   ospf6_dest_type_str[(x)] : ospf6_dest_type_str[0])
#define OSPF6_DEST_TYPE_SUBSTR(x)                           \
  (0 < (x) && (x) < OSPF6_DEST_TYPE_MAX ?                   \
   ospf6_dest_type_substr[(x)] : ospf6_dest_type_substr[0])

extern char *ospf6_path_type_str[OSPF6_PATH_TYPE_MAX];
extern char *ospf6_path_type_substr[OSPF6_PATH_TYPE_MAX];
#define OSPF6_PATH_TYPE_NAME(x)                       \
  (0 < (x) && (x) < OSPF6_PATH_TYPE_MAX ?             \
   ospf6_path_type_str[(x)] : ospf6_path_type_str[0])
#define OSPF6_PATH_TYPE_SUBSTR(x)                           \
  (0 < (x) && (x) < OSPF6_PATH_TYPE_MAX ?                   \
   ospf6_path_type_substr[(x)] : ospf6_path_type_substr[0])

#define OSPF6_ROUTE_ADDRESS_STR "Display the route bestmatches the address\n"
#define OSPF6_ROUTE_PREFIX_STR  "Display the route\n"
#define OSPF6_ROUTE_MATCH_STR   "Display the route matches the prefix\n"

#define ospf6_route_is_prefix(p, r) \
  (memcmp (p, &(r)->prefix, sizeof (struct prefix)) == 0)
#define ospf6_route_is_same(ra, rb) \
  (prefix_same (&(ra)->prefix, &(rb)->prefix))
#define ospf6_route_is_same_origin(ra, rb) \
  ((ra)->path.area_id == (rb)->path.area_id && \
   memcmp (&(ra)->path.origin, &(rb)->path.origin, \
           sizeof (struct ospf6_ls_origin)) == 0)
#define ospf6_route_is_identical(ra, rb) \
  ((ra)->type == (rb)->type && \
   memcmp (&(ra)->prefix, &(rb)->prefix, sizeof (struct prefix)) == 0 && \
   memcmp (&(ra)->path, &(rb)->path, sizeof (struct ospf6_path)) == 0 && \
   memcmp (&(ra)->nexthop, &(rb)->nexthop,                               \
           sizeof (struct ospf6_nexthop) * OSPF6_MULTI_PATH_LIMIT) == 0)
#define ospf6_route_is_best(r) (CHECK_FLAG ((r)->flag, OSPF6_ROUTE_BEST))

#define ospf6_linkstate_prefix_adv_router(x) \
  (*(u_int32_t *)(&(x)->u.prefix6.s6_addr[0]))
#define ospf6_linkstate_prefix_id(x) \
  (*(u_int32_t *)(&(x)->u.prefix6.s6_addr[4]))

#define ADV_ROUTER_IN_PREFIX(x) \
  (*(u_int32_t *)(&(x)->u.prefix6.s6_addr[0]))
#define ID_IN_PREFIX(x) \
  (*(u_int32_t *)(&(x)->u.prefix6.s6_addr[4]))

/* Function prototype */
void ospf6_linkstate_prefix (u_int32_t adv_router, u_int32_t id,
                             struct prefix *prefix);
void ospf6_linkstate_prefix2str (struct prefix *prefix, char *buf, int size);

struct ospf6_route *ospf6_route_create ();
void ospf6_route_delete (struct ospf6_route *);
struct ospf6_route *ospf6_route_copy (struct ospf6_route *route);

void ospf6_route_lock (struct ospf6_route *route);
void ospf6_route_unlock (struct ospf6_route *route);

struct ospf6_route *
ospf6_route_lookup (struct prefix *prefix,
                    struct ospf6_route_table *table);
struct ospf6_route *
ospf6_route_lookup_identical (struct ospf6_route *route,
                              struct ospf6_route_table *table);
struct ospf6_route *
ospf6_route_lookup_bestmatch (struct prefix *prefix,
                              struct ospf6_route_table *table);

struct ospf6_route *
ospf6_route_add (struct ospf6_route *route, struct ospf6_route_table *table);
void
ospf6_route_remove (struct ospf6_route *route, struct ospf6_route_table *table);

struct ospf6_route *ospf6_route_head (struct ospf6_route_table *table);
struct ospf6_route *ospf6_route_next (struct ospf6_route *route);
struct ospf6_route *ospf6_route_best_next (struct ospf6_route *route);

struct ospf6_route *ospf6_route_match_head (struct prefix *prefix,
                                            struct ospf6_route_table *table);
struct ospf6_route *ospf6_route_match_next (struct prefix *prefix,
                                            struct ospf6_route *route);

void ospf6_route_remove_all (struct ospf6_route_table *);
struct ospf6_route_table *ospf6_route_table_create ();
void ospf6_route_table_delete (struct ospf6_route_table *);
void ospf6_route_dump (struct ospf6_route_table *table);


void ospf6_route_show (struct vty *vty, struct ospf6_route *route);
void ospf6_route_show_detail (struct vty *vty, struct ospf6_route *route);

int ospf6_route_table_show (struct vty *, int, char **,
                            struct ospf6_route_table *);
int ospf6_linkstate_table_show (struct vty *vty, int argc, char **argv,
                            struct ospf6_route_table *table);

void ospf6_brouter_show_header (struct vty *vty);
void ospf6_brouter_show (struct vty *vty, struct ospf6_route *route);

int config_write_ospf6_debug_route (struct vty *vty);
void install_element_ospf6_debug_route ();
void ospf6_route_init ();

#endif /* OSPF6_ROUTE_H */

