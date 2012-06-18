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

#ifndef OSPF6_TOP_H
#define OSPF6_TOP_H

#include "routemap.h"

/* OSPFv3 top level data structure */
struct ospf6
{
  /* my router id */
  u_int32_t router_id;

  /* start time */
  struct timeval starttime;

  /* list of areas */
  list area_list;

  /* AS scope link state database */
  struct ospf6_lsdb *lsdb;
  struct ospf6_lsdb *lsdb_self;

  struct ospf6_route_table *route_table;
  struct ospf6_route_table *brouter_table;

  struct ospf6_route_table *external_table;
  struct route_table *external_id_table;
  u_int32_t external_id;

  /* redistribute route-map */
  struct
  {
    char *name;
    struct route_map *map;
  } rmap[ZEBRA_ROUTE_MAX];

  u_char flag;

  struct thread *maxage_remover;
};

#define OSPF6_DISABLED    0x01

/* global pointer for OSPF top data structure */
extern struct ospf6 *ospf6;

/* prototypes */
void ospf6_top_init ();

void ospf6_maxage_remove (struct ospf6 *o);

#endif /* OSPF6_TOP_H */


