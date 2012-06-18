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

#define OSPF_VERTEX_ROUTER  1
#define OSPF_VERTEX_NETWORK 2

#define OSPF_VERTEX_PROCESSED      0x01


struct vertex
{
  u_char flags;
  u_char type;
  struct in_addr id;
  struct lsa_header *lsa;
  u_int32_t distance;
  list child;
  list nexthop;
};

struct vertex_nexthop
{
  struct ospf_interface *oi;
  struct in_addr router;
  struct vertex *parent;
};

void ospf_spf_calculate_schedule (struct ospf *);
void ospf_rtrs_free (struct route_table *);

/* void ospf_spf_calculate_timer_add (); */
