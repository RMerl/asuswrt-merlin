/*
 * OSPF Neighbor functions.
 * Copyright (C) 1999, 2000 Toshiaki Takada
 *
 * This file is part of GNU Zebra.
 * 
 * GNU Zebra is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
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

#include <zebra.h>

#include "linklist.h"
#include "prefix.h"
#include "memory.h"
#include "command.h"
#include "thread.h"
#include "stream.h"
#include "table.h"
#include "log.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_interface.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_lsdb.h"
#include "ospfd/ospf_neighbor.h"
#include "ospfd/ospf_nsm.h"
#include "ospfd/ospf_packet.h"
#include "ospfd/ospf_network.h"
#include "ospfd/ospf_flood.h"
#include "ospfd/ospf_dump.h"

struct ospf_neighbor *
ospf_nbr_new (struct ospf_interface *oi)
{
  struct ospf_neighbor *nbr;

  /* Allcate new neighbor. */
  nbr = XMALLOC (MTYPE_OSPF_NEIGHBOR, sizeof (struct ospf_neighbor));
  memset (nbr, 0, sizeof (struct ospf_neighbor));

  /* Relate neighbor to the interface. */
  nbr->oi = oi;

  /* Set default values. */
  nbr->state = NSM_Down;

  /* Set inheritance values. */
  nbr->v_inactivity = OSPF_IF_PARAM (oi, v_wait);
  nbr->v_db_desc = OSPF_IF_PARAM (oi, retransmit_interval);
  nbr->v_ls_req = OSPF_IF_PARAM (oi, retransmit_interval);
  nbr->v_ls_upd = OSPF_IF_PARAM (oi, retransmit_interval);
  nbr->priority = -1;

  /* DD flags. */
  nbr->dd_flags = OSPF_DD_FLAG_MS|OSPF_DD_FLAG_M|OSPF_DD_FLAG_I;

  /* Last received and sent DD. */
  nbr->last_send = NULL;

  nbr->nbr_nbma = NULL;

  ospf_lsdb_init (&nbr->db_sum);
  ospf_lsdb_init (&nbr->ls_rxmt);
  ospf_lsdb_init (&nbr->ls_req);

  nbr->crypt_seqnum = 0;

  return nbr;
}

void
ospf_nbr_free (struct ospf_neighbor *nbr)
{
  /* Free DB summary list. */
  if (ospf_db_summary_count (nbr))
    ospf_db_summary_clear (nbr);
    /* ospf_db_summary_delete_all (nbr); */

  /* Free ls request list. */
  if (ospf_ls_request_count (nbr))
    ospf_ls_request_delete_all (nbr);

  /* Free retransmit list. */
  if (ospf_ls_retransmit_count (nbr))
    ospf_ls_retransmit_clear (nbr);

  /* Cleanup LSDBs. */
  ospf_lsdb_cleanup (&nbr->db_sum);
  ospf_lsdb_cleanup (&nbr->ls_req);
  ospf_lsdb_cleanup (&nbr->ls_rxmt);
  
  /* Clear last send packet. */
  if (nbr->last_send)
    ospf_packet_free (nbr->last_send);

  if (nbr->nbr_nbma)
    {
      nbr->nbr_nbma->nbr = NULL;
      nbr->nbr_nbma = NULL;
    }

  /* Cancel all timers. */
  OSPF_NSM_TIMER_OFF (nbr->t_inactivity);
  OSPF_NSM_TIMER_OFF (nbr->t_db_desc);
  OSPF_NSM_TIMER_OFF (nbr->t_ls_req);
  OSPF_NSM_TIMER_OFF (nbr->t_ls_upd);

  /* Cancel all events. *//* Thread lookup cost would be negligible. */
  thread_cancel_event (master, nbr);

  XFREE (MTYPE_OSPF_NEIGHBOR, nbr);
}

/* Delete specified OSPF neighbor from interface. */
void
ospf_nbr_delete (struct ospf_neighbor *nbr)
{
  struct ospf_interface *oi;
  struct route_node *rn;
  struct prefix p;

  oi = nbr->oi;

  /* Unlink ospf neighbor from the interface. */
  p.family = AF_INET;
  p.prefixlen = IPV4_MAX_BITLEN;
  p.u.prefix4 = nbr->src;

  rn = route_node_lookup (oi->nbrs, &p);
  if (rn)
    {
      if (rn->info)
	{
	  rn->info = NULL;
	  route_unlock_node (rn);
	}
      else
	zlog_info ("Can't find neighbor %s in the interface %s",
		   inet_ntoa (nbr->src), IF_NAME (oi));

      route_unlock_node (rn);
    }

  /* Free ospf_neighbor structure. */
  ospf_nbr_free (nbr);
}

/* Check myself is in the neighbor list. */
int
ospf_nbr_bidirectional (struct in_addr *router_id,
			struct in_addr *neighbors, int size)
{
  int i;
  int max;

  max = size / sizeof (struct in_addr);

  for (i = 0; i < max; i ++)
    if (IPV4_ADDR_SAME (router_id, &neighbors[i]))
      return 1;

  return 0;
}

/* Add self to nbr list. */
void
ospf_nbr_add_self (struct ospf_interface *oi)
{
  struct ospf_neighbor *nbr;
  struct prefix p;
  struct route_node *rn;

  p.family = AF_INET;
  p.prefixlen = 32;
  p.u.prefix4 = oi->address->u.prefix4;

  rn = route_node_get (oi->nbrs, &p);
  if (rn->info)
    {
      /* There is already pseudo neighbor. */
      nbr = rn->info;
      route_unlock_node (rn);
    }
  else
    rn->info = oi->nbr_self;
}

/* Get neighbor count by status.
   Specify status = 0, get all neighbor other than myself. */
int
ospf_nbr_count (struct ospf_interface *oi, int state)
{
  struct ospf_neighbor *nbr;
  struct route_node *rn;
  int count = 0;

  for (rn = route_top (oi->nbrs); rn; rn = route_next (rn))
    if ((nbr = rn->info))
      if (!IPV4_ADDR_SAME (&nbr->router_id, &oi->ospf->router_id))
	if (state == 0 || nbr->state == state)
	  count++;

  return count;
}

#ifdef HAVE_OPAQUE_LSA
int
ospf_nbr_count_opaque_capable (struct ospf_interface *oi)
{
  struct ospf_neighbor *nbr;
  struct route_node *rn;
  int count = 0;

  for (rn = route_top (oi->nbrs); rn; rn = route_next (rn))
    if ((nbr = rn->info))
      if (!IPV4_ADDR_SAME (&nbr->router_id, &oi->ospf->router_id))
	if (nbr->state == NSM_Full)
	  if (CHECK_FLAG (nbr->options, OSPF_OPTION_O))
	    count++;

  return count;
}
#endif /* HAVE_OPAQUE_LSA */

struct ospf_neighbor *
ospf_nbr_lookup_by_addr (struct route_table *nbrs,
			 struct in_addr *addr)
{
  struct prefix p;
  struct route_node *rn;
  struct ospf_neighbor *nbr;

  p.family = AF_INET;
  p.prefixlen = IPV4_MAX_BITLEN;
  p.u.prefix4 = *addr;

  rn = route_node_lookup (nbrs, &p);
  if (! rn)
    return NULL;

  if (rn->info == NULL)
    {
      route_unlock_node (rn);
      return NULL;
    }

  nbr = (struct ospf_neighbor *) rn->info;
  route_unlock_node (rn);

  return nbr;
}

struct ospf_neighbor *
ospf_nbr_lookup_by_routerid (struct route_table *nbrs,
			     struct in_addr *id)
{
  struct route_node *rn;
  struct ospf_neighbor *nbr;

  for (rn = route_top (nbrs); rn; rn = route_next (rn))
    if ((nbr = rn->info) != NULL)
      if (IPV4_ADDR_SAME (&nbr->router_id, id))
	{
	  route_unlock_node(rn);
	  return nbr;
	}

  return NULL;
}

void
ospf_renegotiate_optional_capabilities (struct ospf *top)
{
  listnode node;
  struct ospf_interface *oi;
  struct route_table *nbrs;
  struct route_node *rn;
  struct ospf_neighbor *nbr;

  /* At first, flush self-originated LSAs from routing domain. */
  ospf_flush_self_originated_lsas_now (top);

  /* Revert all neighbor status to ExStart. */
  for (node = listhead (top->oiflist); node; nextnode (node))
    {
      if ((oi = getdata (node)) == NULL || (nbrs = oi->nbrs) == NULL)
        continue;

      for (rn = route_top (nbrs); rn; rn = route_next (rn))
        {
          if ((nbr = rn->info) == NULL || nbr == oi->nbr_self)
            continue;

          if (nbr->state < NSM_ExStart)
            continue;

          if (IS_DEBUG_OSPF_EVENT)
            zlog_info ("Renegotiate optional capabilities with neighbor(%s)", inet_ntoa (nbr->router_id));

          OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_SeqNumberMismatch);
        }
    }

  return;
}
