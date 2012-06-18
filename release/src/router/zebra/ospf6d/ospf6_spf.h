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
  list child_list;
};

#define OSPF6_VERTEX_TYPE_ROUTER  0x01
#define OSPF6_VERTEX_TYPE_NETWORK 0x02
#define VERTEX_IS_TYPE(t, v) \
  ((v)->type == OSPF6_VERTEX_TYPE_ ## t ? 1 : 0)

void ospf6_spf_table_finish (struct ospf6_route_table *result_table);
void ospf6_spf_calculation (u_int32_t router_id,
                            struct ospf6_route_table *result_table,
                            struct ospf6_area *oa);
void ospf6_spf_schedule (struct ospf6_area *oa);

void ospf6_spf_display_subtree (struct vty *vty, char *prefix,
                                int rest, struct ospf6_vertex *v);

int config_write_ospf6_debug_spf (struct vty *vty);
void install_element_ospf6_debug_spf ();
void ospf6_spf_init ();

#endif /* OSPF6_SPF_H */

