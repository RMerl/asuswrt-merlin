/* BGP routing table
   Copyright (C) 1998, 2001 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include <zebra.h>

#include "prefix.h"
#include "memory.h"
#include "sockunion.h"
#include "vty.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_table.h"

void
bgp_table_lock (struct bgp_table *rt)
{
  rt->lock++;
}

void
bgp_table_unlock (struct bgp_table *rt)
{
  assert (rt->lock > 0);
  rt->lock--;

  if (rt->lock != 0) 
    {
      return;
    }

  route_table_finish (rt->route_table);
  rt->route_table = NULL;

  if (rt->owner)
    {
      peer_unlock (rt->owner);
      rt->owner = NULL;
    }

  XFREE (MTYPE_BGP_TABLE, rt);
}

void
bgp_table_finish (struct bgp_table **rt)
{
  if (*rt != NULL)
    {
      bgp_table_unlock(*rt);
      *rt = NULL;
    }
}

/*
 * bgp_node_create
 */
static struct route_node *
bgp_node_create (route_table_delegate_t *delegate, struct route_table *table)
{
  struct bgp_node *node;
  node = XCALLOC (MTYPE_BGP_NODE, sizeof (struct bgp_node));
  return bgp_node_to_rnode (node);
}

/*
 * bgp_node_destroy
 */
static void
bgp_node_destroy (route_table_delegate_t *delegate,
		  struct route_table *table, struct route_node *node)
{
  struct bgp_node *bgp_node;
  bgp_node = bgp_node_from_rnode (node);
  XFREE (MTYPE_BGP_NODE, bgp_node);
}

/*
 * Function vector to customize the behavior of the route table
 * library for BGP route tables.
 */
route_table_delegate_t bgp_table_delegate = {
  .create_node = bgp_node_create,
  .destroy_node = bgp_node_destroy
};

/*
 * bgp_table_init
 */
struct bgp_table *
bgp_table_init (afi_t afi, safi_t safi)
{
  struct bgp_table *rt;

  rt = XCALLOC (MTYPE_BGP_TABLE, sizeof (struct bgp_table));

  rt->route_table = route_table_init_with_delegate (&bgp_table_delegate);

  /*
   * Set up back pointer to bgp_table.
   */
  rt->route_table->info = rt;

  bgp_table_lock (rt);
  rt->type = BGP_TABLE_MAIN;
  rt->afi = afi;
  rt->safi = safi;

  return rt;
}
