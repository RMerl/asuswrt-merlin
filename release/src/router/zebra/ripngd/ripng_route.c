/*
 * RIPng routes function.
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

#include <zebra.h>

#include "prefix.h"
#include "table.h"
#include "memory.h"
#include "if.h"

#include "ripngd/ripngd.h"
#include "ripngd/ripng_route.h"

struct ripng_aggregate *
ripng_aggregate_new ()
{
  struct ripng_aggregate *new;

  new = XCALLOC (MTYPE_RIPNG_AGGREGATE, sizeof (struct ripng_aggregate));
  return new;
}

void
ripng_aggregate_free (struct ripng_aggregate *aggregate)
{
  XFREE (MTYPE_RIPNG_AGGREGATE, aggregate);
}

/* Aggregate count increment check. */
void
ripng_aggregate_increment (struct route_node *child, struct ripng_info *rinfo)
{
  struct route_node *np;
  struct ripng_aggregate *aggregate;

  for (np = child; np; np = np->parent)
    if ((aggregate = np->aggregate) != NULL)
      {
	aggregate->count++;
	rinfo->suppress++;
      }
}

/* Aggregate count decrement check. */
void
ripng_aggregate_decrement (struct route_node *child, struct ripng_info *rinfo)
{
  struct route_node *np;
  struct ripng_aggregate *aggregate;

  for (np = child; np; np = np->parent)
    if ((aggregate = np->aggregate) != NULL)
      {
	aggregate->count--;
	rinfo->suppress--;
      }
}

/* RIPng routes treatment. */
int
ripng_aggregate_add (struct prefix *p)
{
  struct route_node *top;
  struct route_node *rp;
  struct ripng_info *rinfo;
  struct ripng_aggregate *aggregate;
  struct ripng_aggregate *sub;

  /* Get top node for aggregation. */
  top = route_node_get (ripng->table, p);

  /* Allocate new aggregate. */
  aggregate = ripng_aggregate_new ();
  aggregate->metric = 1;

  top->aggregate = aggregate;

  /* Suppress routes match to the aggregate. */
  for (rp = route_lock_node (top); rp; rp = route_next_until (rp, top))
    {
      /* Suppress normal route. */
      if ((rinfo = rp->info) != NULL)
	{
	  aggregate->count++;
	  rinfo->suppress++;
	}
      /* Suppress aggregate route.  This may not need. */
      if (rp != top && (sub = rp->aggregate) != NULL)
	{
	  aggregate->count++;
	  sub->suppress++;
	}
    }

  return 0;
}

/* Delete RIPng static route. */
int
ripng_aggregate_delete (struct prefix *p)
{
  struct route_node *top;
  struct route_node *rp;
  struct ripng_info *rinfo;
  struct ripng_aggregate *aggregate;
  struct ripng_aggregate *sub;

  /* Get top node for aggregation. */
  top = route_node_get (ripng->table, p);

  /* Allocate new aggregate. */
  aggregate = top->aggregate;

  /* Suppress routes match to the aggregate. */
  for (rp = route_lock_node (top); rp; rp = route_next_until (rp, top))
    {
      /* Suppress normal route. */
      if ((rinfo = rp->info) != NULL)
	{
	  aggregate->count--;
	  rinfo->suppress--;
	}

      if (rp != top && (sub = rp->aggregate) != NULL)
	{
	  aggregate->count--;
	  sub->suppress--;
	}
    }

  top->aggregate = NULL;
  ripng_aggregate_free (aggregate);

  route_unlock_node (top);
  route_unlock_node (top);

  return 0;
}
