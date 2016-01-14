/*
  PIM for Quagga
  Copyright (C) 2008  Everton da Silva Marques

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING; if not, write to the
  Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
  MA 02110-1301 USA
  
  $QuaggaId: $Format:%an, %ai, %h$ $
*/

#ifndef PIM_ZLOOKUP_H
#define PIM_ZLOOKUP_H

#include <zebra.h>

#include "zclient.h"

#define PIM_NEXTHOP_LOOKUP_MAX (3) /* max. recursive route lookup */

struct pim_zlookup_nexthop {
  struct in_addr nexthop_addr;
  int            ifindex;
  uint32_t       route_metric;
  uint8_t        protocol_distance;
};

struct zclient *zclient_lookup_new(void);

int zclient_lookup_nexthop(struct zclient *zlookup,
			   struct pim_zlookup_nexthop nexthop_tab[],
			   const int tab_size,
			   struct in_addr addr,
			   int max_lookup);

#endif /* PIM_ZLOOKUP_H */
