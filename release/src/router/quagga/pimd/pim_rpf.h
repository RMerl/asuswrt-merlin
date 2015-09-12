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

#ifndef PIM_RPF_H
#define PIM_RPF_H

#include <zebra.h>

#include "pim_upstream.h"
#include "pim_neighbor.h"

int pim_nexthop_lookup(struct pim_nexthop *nexthop,
		       struct in_addr addr);
enum pim_rpf_result pim_rpf_update(struct pim_upstream *up,
				   struct in_addr *old_rpf_addr);

#endif /* PIM_RPF_H */
