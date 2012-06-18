/*
 * RIPng daemon
 * Copyright (C) 1998 Kunihiro Ishiguro
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

#ifndef _ZEBRA_RIPNG_ROUTE_H
#define _ZEBRA_RIPNG_ROUTE_H

struct ripng_aggregate
{
  /* Aggregate route count. */
  unsigned int count;

  /* Suppressed route count. */
  unsigned int suppress;

  /* Metric of this route.  */
  u_char metric;		

  /* Tag field of RIPng packet.*/
  u_short tag;		
};

void
ripng_aggregate_increment (struct route_node *rp, struct ripng_info *rinfo);

void
ripng_aggregate_decrement (struct route_node *rp, struct ripng_info *rinfo);

int
ripng_aggregate_add (struct prefix *p);

int
ripng_aggregate_delete (struct prefix *p);

#endif /* _ZEBRA_RIPNG_ROUTE_H */
