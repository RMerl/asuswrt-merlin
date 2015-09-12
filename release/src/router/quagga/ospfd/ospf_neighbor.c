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

/* Fill in the the 'key' as appropriate to retrieve the entry for nbr
 * from the ospf_interface's nbrs table. Indexed by interface address
 * for all cases except Virtual-link interfaces, where neighbours are
 * indexed by router-ID instead.
 */
static void
ospf_nbr_key (struct ospf_interface *oi, struct ospf_neighbor *nbr,
              struct prefix *key)
{
  key->family = AF_INET;
  key->prefixlen = IPV4_MAX_BITLEN;

  /* vlinks are indexed by router-id */
  if (oi->type == OSPF_IFTYPE_VIRTUALLINK)
    key->u.prefix4 = nbr->router_id;
  else
    key->u.prefix4 = nbr->src;
  return;
}

struct ospf_neighbor *
ospf_nbr_new (struct ospf_interface *oi)
{
  struct ospf_neighbor *nbr;

  /* Allcate new neighbor. */
  nbr = XCALLOC (MTYPE_OSPF_NEIGHBOR, sizeof (struct ospf_neighbor));

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
  
  /* get appropriate prefix 'key' */
  ospf_nbr_key (oi, nbr, &p);

  rn = route_node_lookup (oi->nbrs, &p);
  if (rn)
    {
      /* If lookup for a NBR succeeds, the leaf route_node could
       * only exist because there is (or was) a nbr there.
       * If the nbr was deleted, the leaf route_node should have
       * lost its last refcount too, and be deleted.
       * Therefore a looked-up leaf route_node in nbrs table
       * should never have NULL info.
       */
      assert (rn->info);
      
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
  struct prefix p;
  struct route_node *rn;

  /* Initial state */
  oi->nbr_self->address = *oi->address;
  oi->nbr_self->priority = OSPF_IF_PARAM (oi, priority);
  oi->nbr_self->router_id = oi->ospf->router_id;
  oi->nbr_self->src = oi->address->u.prefix4;
  oi->nbr_self->state = NSM_TwoWay;
  
  switch (oi->area->external_routing)
    {
      case OSPF_AREA_DEFAULT:
        SET_FLAG (oi->nbr_self->options, OSPF_OPTION_E);
        break;
      case OSPF_AREA_STUB:
        UNSET_FLAG (oi->nbr_self->options, OSPF_OPTION_E);
        break;
      case OSPF_AREA_NSSA:
        UNSET_FLAG (oi->nbr_self->options, OSPF_OPTION_E);
        SET_FLAG (oi->nbr_self->options, OSPF_OPTION_NP);
        break;
    }
  
  /* Add nbr_self to nbrs table */
  ospf_nbr_key (oi, oi->nbr_self, &p);
  
  rn = route_node_get (oi->nbrs, &p);
  if (rn->info)
    {
      /* There is already pseudo neighbor. */
      assert (oi->nbr_self == rn->info);
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

/* lookup nbr by address - use this only if you know you must
 * otherwise use the ospf_nbr_lookup() wrapper, which deals 
 * with virtual link neighbours
 */
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
  
  /* See comment in ospf_nbr_delete */
  assert (rn->info);

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
  struct listnode *node;
  struct ospf_interface *oi;
  struct route_table *nbrs;
  struct route_node *rn;
  struct ospf_neighbor *nbr;

  /* At first, flush self-originated LSAs from routing domain. */
  ospf_flush_self_originated_lsas_now (top);

  /* Revert all neighbor status to ExStart. */
  for (ALL_LIST_ELEMENTS_RO (top->oiflist, node, oi))
    {
      if ((nbrs = oi->nbrs) == NULL)
        continue;

      for (rn = route_top (nbrs); rn; rn = route_next (rn))
        {
          if ((nbr = rn->info) == NULL || nbr == oi->nbr_self)
            continue;

          if (nbr->state < NSM_ExStart)
            continue;

          if (IS_DEBUG_OSPF_EVENT)
            zlog_debug ("Renegotiate optional capabilities with neighbor(%s)", inet_ntoa (nbr->router_id));

          OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_SeqNumberMismatch);
        }
    }

  return;
}


struct ospf_neighbor *
ospf_nbr_lookup (struct ospf_interface *oi, struct ip *iph,
                 struct ospf_header *ospfh)
{
  if (oi->type == OSPF_IFTYPE_VIRTUALLINK)
    return (ospf_nbr_lookup_by_routerid (oi->nbrs, &ospfh->router_id));
  else
    return (ospf_nbr_lookup_by_addr (oi->nbrs, &iph->ip_src));
}

static struct ospf_neighbor *
ospf_nbr_add (struct ospf_interface *oi, struct ospf_header *ospfh,
              struct prefix *p)
{
  struct ospf_neighbor *nbr;
  
  nbr = ospf_nbr_new (oi);
  nbr->state = NSM_Down;
  nbr->src = p->u.prefix4;
  memcpy (&nbr->address, p, sizeof (struct prefix));

  nbr->nbr_nbma = NULL;
  if (oi->type == OSPF_IFTYPE_NBMA)
    {
      struct ospf_nbr_nbma *nbr_nbma;
      struct listnode *node;

      for (ALL_LIST_ELEMENTS_RO (oi->nbr_nbma, node, nbr_nbma))
        {
          if (IPV4_ADDR_SAME(&nbr_nbma->addr, &nbr->src))
            {
              nbr_nbma->nbr = nbr;
              nbr->nbr_nbma = nbr_nbma;

              if (nbr_nbma->t_poll)
                OSPF_POLL_TIMER_OFF (nbr_nbma->t_poll);

              nbr->state_change = nbr_nbma->state_change + 1;
            }
        }
    }
      
  /* New nbr, save the crypto sequence number if necessary */
  if (ntohs (ospfh->auth_type) == OSPF_AUTH_CRYPTOGRAPHIC)
    nbr->crypt_seqnum = ospfh->u.crypt.crypt_seqnum;
  
  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("NSM[%s:%s]: start", IF_NAME (nbr->oi),
               inet_ntoa (nbr->router_id));
  
  return nbr;
}

struct ospf_neighbor *
ospf_nbr_get (struct ospf_interface *oi, struct ospf_header *ospfh,
              struct ip *iph, struct prefix *p)
{
  struct route_node *rn;
  struct prefix key;
  struct ospf_neighbor *nbr;
  
  key.family = AF_INET;
  key.prefixlen = IPV4_MAX_BITLEN;

  if (oi->type == OSPF_IFTYPE_VIRTUALLINK)
    key.u.prefix4 = ospfh->router_id;   /* index vlink nbrs by router-id */
  else
    key.u.prefix4 = iph->ip_src;

  rn = route_node_get (oi->nbrs, &key);
  if (rn->info)
    {
      route_unlock_node (rn);
      nbr = rn->info;
      
      if (oi->type == OSPF_IFTYPE_NBMA && nbr->state == NSM_Attempt)
        {
          nbr->src = iph->ip_src;
          memcpy (&nbr->address, p, sizeof (struct prefix));
        }
    }
  else
    {
      rn->info = nbr = ospf_nbr_add (oi, ospfh, p);
    }
  
  nbr->router_id = ospfh->router_id;

  return nbr;
}
