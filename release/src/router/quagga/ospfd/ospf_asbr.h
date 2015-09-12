/*
 * OSPF AS Boundary Router functions.
 * Copyright (C) 1999, 2000 Kunihiro Ishiguro, Toshiaki Takada
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

#ifndef _ZEBRA_OSPF_ASBR_H
#define _ZEBRA_OSPF_ASBR_H

struct route_map_set_values
{
  int32_t metric;
  int32_t metric_type;
};

/* Redistributed external information. */
struct external_info
{
  /* Type of source protocol. */
  u_char type;

  /* Prefix. */
  struct prefix_ipv4 p;

  /* Interface index. */
  unsigned int ifindex;

  /* Nexthop address. */
  struct in_addr nexthop;

  /* Additional Route tag. */
  u_int32_t tag;

  struct route_map_set_values route_map_set;
#define ROUTEMAP_METRIC(E)      (E)->route_map_set.metric
#define ROUTEMAP_METRIC_TYPE(E) (E)->route_map_set.metric_type
};

#define OSPF_ASBR_CHECK_DELAY 30

extern void ospf_external_route_remove (struct ospf *, struct prefix_ipv4 *);
extern struct external_info *ospf_external_info_new (u_char);
extern void ospf_reset_route_map_set_values (struct route_map_set_values *);
extern int ospf_route_map_set_compare (struct route_map_set_values *,
				struct route_map_set_values *);
extern struct external_info *ospf_external_info_add (u_char, 
                                              struct prefix_ipv4,
					      unsigned int, 
					      struct in_addr);
extern void ospf_external_info_delete (u_char, struct prefix_ipv4);
extern struct external_info *ospf_external_info_lookup (u_char, 
                                                 struct prefix_ipv4 *);
extern struct ospf_route *ospf_external_route_lookup (struct ospf *, 
                                               struct prefix_ipv4 *);
extern void ospf_asbr_status_update (struct ospf *, u_char);

extern void ospf_redistribute_withdraw (struct ospf *, u_char);
extern void ospf_asbr_check (void);
extern void ospf_schedule_asbr_check (void);
extern void ospf_asbr_route_install_lsa (struct ospf_lsa *);
extern struct ospf_lsa *ospf_external_info_find_lsa (struct ospf *,
					      struct prefix_ipv4 *p);

#endif /* _ZEBRA_OSPF_ASBR_H */
