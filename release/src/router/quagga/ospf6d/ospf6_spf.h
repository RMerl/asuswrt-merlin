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

#ifndef OSPF6_SPF_H
#define OSPF6_SPF_H

#include "ospf6_top.h"

/* Debug option */
extern unsigned char conf_debug_ospf6_spf;
#define OSPF6_DEBUG_SPF_PROCESS   0x01
#define OSPF6_DEBUG_SPF_TIME      0x02
#define OSPF6_DEBUG_SPF_DATABASE  0x04
#define OSPF6_DEBUG_SPF_ON(level) \
  (conf_debug_ospf6_spf |= (level))
#define OSPF6_DEBUG_SPF_OFF(level) \
  (conf_debug_ospf6_spf &= ~(level))
#define IS_OSPF6_DEBUG_SPF(level) \
  (conf_debug_ospf6_spf & OSPF6_DEBUG_SPF_ ## level)

/* Transit Vertex */
struct ospf6_vertex
{
  /* type of this vertex */
  u_int8_t type;

  /* Vertex Identifier */
  struct prefix vertex_id;

  /* Identifier String */
  char name[128];

  /* Associated Area */
  struct ospf6_area *area;

  /* Associated LSA */
  struct ospf6_lsa *lsa;

  /* Distance from Root (i.e. Cost) */
  u_int32_t cost;

  /* Router hops to this node */
  u_char hops;

  /* nexthops to this node */
  struct ospf6_nexthop nexthop[OSPF6_MULTI_PATH_LIMIT];

  /* capability bits */
  u_char capability;

  /* Optional capabilities */
  u_char options[3];

  /* For tree display */
  struct ospf6_vertex *parent;
  struct list *child_list;
};

#define OSPF6_VERTEX_TYPE_ROUTER  0x01
#define OSPF6_VERTEX_TYPE_NETWORK 0x02
#define VERTEX_IS_TYPE(t, v) \
  ((v)->type == OSPF6_VERTEX_TYPE_ ## t ? 1 : 0)

/* What triggered the SPF? */
#define OSPF6_SPF_FLAGS_ROUTER_LSA_ADDED         (1 << 0)
#define OSPF6_SPF_FLAGS_ROUTER_LSA_REMOVED       (1 << 1)
#define OSPF6_SPF_FLAGS_NETWORK_LSA_ADDED        (1 << 2)
#define OSPF6_SPF_FLAGS_NETWORK_LSA_REMOVED      (1 << 3)
#define OSPF6_SPF_FLAGS_LINK_LSA_ADDED           (1 << 4)
#define OSPF6_SPF_FLAGS_LINK_LSA_REMOVED         (1 << 5)
#define OSPF6_SPF_FLAGS_ROUTER_LSA_ORIGINATED    (1 << 6)
#define OSPF6_SPF_FLAGS_NETWORK_LSA_ORIGINATED   (1 << 7)

static inline void
ospf6_set_spf_reason (struct ospf6* ospf, unsigned int reason)
{
  ospf->spf_reason |= reason;
}

static inline void
ospf6_reset_spf_reason (struct ospf6 *ospf)
{
  ospf->spf_reason = 0;
}

static inline unsigned int
ospf6_lsadd_to_spf_reason (struct ospf6_lsa *lsa)
{
  unsigned int reason = 0;

  switch (ntohs (lsa->header->type))
    {
    case OSPF6_LSTYPE_ROUTER:
      reason = OSPF6_SPF_FLAGS_ROUTER_LSA_ADDED;
      break;
    case OSPF6_LSTYPE_NETWORK:
      reason = OSPF6_SPF_FLAGS_NETWORK_LSA_ADDED;
      break;
    case OSPF6_LSTYPE_LINK:
      reason = OSPF6_SPF_FLAGS_LINK_LSA_ADDED;
      break;
    default:
      break;
    }
  return (reason);
}

static inline unsigned int
ospf6_lsremove_to_spf_reason (struct ospf6_lsa *lsa)
{
  unsigned int reason = 0;

  switch (ntohs (lsa->header->type))
    {
    case OSPF6_LSTYPE_ROUTER:
      reason = OSPF6_SPF_FLAGS_ROUTER_LSA_REMOVED;
      break;
    case OSPF6_LSTYPE_NETWORK:
      reason = OSPF6_SPF_FLAGS_NETWORK_LSA_REMOVED;
      break;
    case OSPF6_LSTYPE_LINK:
      reason = OSPF6_SPF_FLAGS_LINK_LSA_REMOVED;
      break;
    default:
      break;
    }
  return (reason);
}

extern void ospf6_spf_table_finish (struct ospf6_route_table *result_table);
extern void ospf6_spf_calculation (u_int32_t router_id,
                                   struct ospf6_route_table *result_table,
                                   struct ospf6_area *oa);
extern void ospf6_spf_schedule (struct ospf6 *ospf, unsigned int reason);

extern void ospf6_spf_display_subtree (struct vty *vty, const char *prefix,
                                       int rest, struct ospf6_vertex *v);

extern void ospf6_spf_config_write (struct vty *vty);
extern int config_write_ospf6_debug_spf (struct vty *vty);
extern void install_element_ospf6_debug_spf (void);
extern void ospf6_spf_init (void);
extern void ospf6_spf_reason_string (unsigned int reason, char *buf, int size);

#endif /* OSPF6_SPF_H */

