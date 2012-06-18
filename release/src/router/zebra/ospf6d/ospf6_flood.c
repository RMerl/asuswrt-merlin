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

#include <zebra.h>

#include "log.h"
#include "thread.h"
#include "linklist.h"
#include "vty.h"
#include "command.h"

#include "ospf6d.h"
#include "ospf6_proto.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6_message.h"
#include "ospf6_route.h"
#include "ospf6_spf.h"

#include "ospf6_top.h"
#include "ospf6_area.h"
#include "ospf6_interface.h"
#include "ospf6_neighbor.h"

#include "ospf6_flood.h"

unsigned char conf_debug_ospf6_flooding;

struct ospf6_lsdb *
ospf6_get_scoped_lsdb (struct ospf6_lsa *lsa)
{
  struct ospf6_lsdb *lsdb = NULL;
  switch (OSPF6_LSA_SCOPE (lsa->header->type))
    {
    case OSPF6_SCOPE_LINKLOCAL:
      lsdb = OSPF6_INTERFACE (lsa->lsdb->data)->lsdb;
      break;
    case OSPF6_SCOPE_AREA:
      lsdb = OSPF6_AREA (lsa->lsdb->data)->lsdb;
      break;
    case OSPF6_SCOPE_AS:
      lsdb = OSPF6_PROCESS (lsa->lsdb->data)->lsdb;
      break;
    default:
      assert (0);
      break;
    }
  return lsdb;
}

struct ospf6_lsdb *
ospf6_get_scoped_lsdb_self (struct ospf6_lsa *lsa)
{
  struct ospf6_lsdb *lsdb_self = NULL;
  switch (OSPF6_LSA_SCOPE (lsa->header->type))
    {
    case OSPF6_SCOPE_LINKLOCAL:
      lsdb_self = OSPF6_INTERFACE (lsa->lsdb->data)->lsdb_self;
      break;
    case OSPF6_SCOPE_AREA:
      lsdb_self = OSPF6_AREA (lsa->lsdb->data)->lsdb_self;
      break;
    case OSPF6_SCOPE_AS:
      lsdb_self = OSPF6_PROCESS (lsa->lsdb->data)->lsdb_self;
      break;
    default:
      assert (0);
      break;
    }
  return lsdb_self;
}

void
ospf6_lsa_originate (struct ospf6_lsa *lsa)
{
  struct ospf6_lsa *old;
  struct ospf6_lsdb *lsdb_self;

  /* find previous LSA */
  old = ospf6_lsdb_lookup (lsa->header->type, lsa->header->id,
                           lsa->header->adv_router, lsa->lsdb);

  /* if the new LSA does not differ from previous,
     suppress this update of the LSA */
  if (old && ! OSPF6_LSA_IS_DIFFER (lsa, old))
    {
      if (IS_OSPF6_DEBUG_ORIGINATE_TYPE (lsa->header->type))
        zlog_info ("Suppress updating LSA: %s", lsa->name);
      ospf6_lsa_delete (lsa);
      return;
    }

  /* store it in the LSDB for self-originated LSAs */
  lsdb_self = ospf6_get_scoped_lsdb_self (lsa);
  ospf6_lsdb_add (ospf6_lsa_copy (lsa), lsdb_self);

  lsa->refresh = thread_add_timer (master, ospf6_lsa_refresh, lsa,
                                   LS_REFRESH_TIME);

  if (IS_OSPF6_DEBUG_LSA_TYPE (lsa->header->type) ||
      IS_OSPF6_DEBUG_ORIGINATE_TYPE (lsa->header->type))
    {
      zlog_info ("LSA Originate:");
      ospf6_lsa_header_print (lsa);
    }

  if (old)
    ospf6_flood_clear (old);
  ospf6_flood (NULL, lsa);
  ospf6_install_lsa (lsa);
}

void
ospf6_lsa_originate_process (struct ospf6_lsa *lsa,
                             struct ospf6 *process)
{
  lsa->lsdb = process->lsdb;
  ospf6_lsa_originate (lsa);
}

void
ospf6_lsa_originate_area (struct ospf6_lsa *lsa,
                          struct ospf6_area *oa)
{
  lsa->lsdb = oa->lsdb;
  ospf6_lsa_originate (lsa);
}

void
ospf6_lsa_originate_interface (struct ospf6_lsa *lsa,
                               struct ospf6_interface *oi)
{
  lsa->lsdb = oi->lsdb;
  ospf6_lsa_originate (lsa);
}

void
ospf6_lsa_purge (struct ospf6_lsa *lsa)
{
  struct ospf6_lsa *self;
  struct ospf6_lsdb *lsdb_self;

  /* remove it from the LSDB for self-originated LSAs */
  lsdb_self = ospf6_get_scoped_lsdb_self (lsa);
  self = ospf6_lsdb_lookup (lsa->header->type, lsa->header->id,
                            lsa->header->adv_router, lsdb_self);
  if (self)
    {
      THREAD_OFF (self->expire);
      THREAD_OFF (self->refresh);
      ospf6_lsdb_remove (self, lsdb_self);
    }

  ospf6_lsa_premature_aging (lsa);
}


void
ospf6_increment_retrans_count (struct ospf6_lsa *lsa)
{
  /* The LSA must be the original one (see the description
     in ospf6_decrement_retrans_count () below) */
  lsa->retrans_count++;
}

void
ospf6_decrement_retrans_count (struct ospf6_lsa *lsa)
{
  struct ospf6_lsdb *lsdb;
  struct ospf6_lsa *orig;

  /* The LSA must be on the retrans-list of a neighbor. It means
     the "lsa" is a copied one, and we have to decrement the
     retransmission count of the original one (instead of this "lsa"'s).
     In order to find the original LSA, first we have to find
     appropriate LSDB that have the original LSA. */
  lsdb = ospf6_get_scoped_lsdb (lsa);

  /* Find the original LSA of which the retrans_count should be decremented */
  orig = ospf6_lsdb_lookup (lsa->header->type, lsa->header->id,
                            lsa->header->adv_router, lsdb);
  if (orig)
    {
      orig->retrans_count--;
      assert (orig->retrans_count >= 0);
    }
}

/* RFC2328 section 13.2 Installing LSAs in the database */
void
ospf6_install_lsa (struct ospf6_lsa *lsa)
{
  struct ospf6_lsa *old;
  struct timeval now;

  if (IS_OSPF6_DEBUG_LSA_TYPE (lsa->header->type) ||
      IS_OSPF6_DEBUG_EXAMIN_TYPE (lsa->header->type))
    zlog_info ("Install LSA: %s", lsa->name);

  /* Remove the old instance from all neighbors' Link state
     retransmission list (RFC2328 13.2 last paragraph) */
  old = ospf6_lsdb_lookup (lsa->header->type, lsa->header->id,
                           lsa->header->adv_router, lsa->lsdb);
  if (old)
    {
      THREAD_OFF (old->expire);
      ospf6_flood_clear (old);
    }

  gettimeofday (&now, (struct timezone *) NULL);
  if (! OSPF6_LSA_IS_MAXAGE (lsa))
    lsa->expire = thread_add_timer (master, ospf6_lsa_expire, lsa,
                                    MAXAGE + lsa->birth.tv_sec - now.tv_sec);
  else
    lsa->expire = NULL;

  /* actually install */
  lsa->installed = now;
  ospf6_lsdb_add (lsa, lsa->lsdb);

  return;
}

/* RFC2740 section 3.5.2. Sending Link State Update packets */
/* RFC2328 section 13.3 Next step in the flooding procedure */
void
ospf6_flood_interface (struct ospf6_neighbor *from,
                       struct ospf6_lsa *lsa, struct ospf6_interface *oi)
{
  listnode node;
  struct ospf6_neighbor *on;
  struct ospf6_lsa *req;
  int retrans_added = 0;
  int is_debug = 0;

  if (IS_OSPF6_DEBUG_FLOODING ||
      IS_OSPF6_DEBUG_FLOOD_TYPE (lsa->header->type))
    {
      is_debug++;
      zlog_info ("Flooding on %s: %s", oi->interface->name, lsa->name);
    }

  /* (1) For each neighbor */
  for (node = listhead (oi->neighbor_list); node; nextnode (node))
    {
      on = (struct ospf6_neighbor *) getdata (node);

      if (is_debug)
        zlog_info ("To neighbor %s", on->name);

      /* (a) if neighbor state < Exchange, examin next */
      if (on->state < OSPF6_NEIGHBOR_EXCHANGE)
        {
          if (is_debug)
            zlog_info ("Neighbor state less than ExChange, next neighbor");
          continue;
        }

      /* (b) if neighbor not yet Full, check request-list */
      if (on->state != OSPF6_NEIGHBOR_FULL)
        {
          if (is_debug)
            zlog_info ("Neighbor not yet Full");

          req = ospf6_lsdb_lookup (lsa->header->type, lsa->header->id,
                                   lsa->header->adv_router, on->request_list);
          if (req == NULL)
            {
              if (is_debug)
                zlog_info ("Not on request-list for this neighbor");
              /* fall through */
            }
          else
            {
              /* If new LSA less recent, examin next neighbor */
              if (ospf6_lsa_compare (lsa, req) > 0)
                {
                  if (is_debug)
                    zlog_info ("Requesting is newer, next neighbor");
                  continue;
                }

              /* If the same instance, delete from request-list and
                 examin next neighbor */
              if (ospf6_lsa_compare (lsa, req) == 0)
                {
                  if (is_debug)
                    zlog_info ("Requesting the same, remove it, next neighbor");
                  ospf6_lsdb_remove (req, on->request_list);
                  continue;
                }

              /* If the new LSA is more recent, delete from request-list */
              if (ospf6_lsa_compare (lsa, req) < 0)
                {
                  if (is_debug)
                    zlog_info ("Received is newer, remove requesting");
                  ospf6_lsdb_remove (req, on->request_list);
                  /* fall through */
                }
            }
        }

      /* (c) If the new LSA was received from this neighbor,
         examin next neighbor */
      if (from == on)
        {
          if (is_debug)
            zlog_info ("Received is from the neighbor, next neighbor");
          continue;
        }

      /* (d) add retrans-list, schedule retransmission */
      if (is_debug)
        zlog_info ("Add retrans-list of this neighbor");
      ospf6_increment_retrans_count (lsa);
      ospf6_lsdb_add (ospf6_lsa_copy (lsa), on->retrans_list);
      if (on->thread_send_lsupdate == NULL)
        on->thread_send_lsupdate =
          thread_add_event (master, ospf6_lsupdate_send_neighbor,
                            on, on->ospf6_if->rxmt_interval);
      retrans_added++;
    }

  /* (2) examin next interface if not added to retrans-list */
  if (retrans_added == 0)
    {
      if (is_debug)
        zlog_info ("No retransmission scheduled, next interface");
      return;
    }

  /* (3) If the new LSA was received on this interface,
     and it was from DR or BDR, examin next interface */
  if (from && from->ospf6_if == oi &&
      (from->router_id == oi->drouter || from->router_id == oi->bdrouter))
    {
      if (is_debug)
        zlog_info ("Received is from the I/F's DR or BDR, next interface");
      return;
    }

  /* (4) If the new LSA was received on this interface,
     and the interface state is BDR, examin next interface */
  if (from && from->ospf6_if == oi && oi->state == OSPF6_INTERFACE_BDR)
    {
      if (is_debug)
        zlog_info ("Received is from the I/F, itself BDR, next interface");
      return;
    }

  /* (5) flood the LSA out the interface. */
  if (is_debug)
    zlog_info ("Schedule flooding for the interface");
  if (if_is_broadcast (oi->interface))
    {
      ospf6_lsdb_add (ospf6_lsa_copy (lsa), oi->lsupdate_list);
      if (oi->thread_send_lsupdate == NULL)
        oi->thread_send_lsupdate =
          thread_add_event (master, ospf6_lsupdate_send_interface, oi, 0);
    }
  else
    {
      /* reschedule retransmissions to all neighbors */
      for (node = listhead (oi->neighbor_list); node; nextnode (node))
        {
          on = (struct ospf6_neighbor *) getdata (node);
          THREAD_OFF (on->thread_send_lsupdate);
          on->thread_send_lsupdate =
            thread_add_event (master, ospf6_lsupdate_send_neighbor, on, 0);
        }
    }
}

void
ospf6_flood_area (struct ospf6_neighbor *from,
                  struct ospf6_lsa *lsa, struct ospf6_area *oa)
{
  listnode node;
  struct ospf6_interface *oi;

  for (node = listhead (oa->if_list); node; nextnode (node))
    {
      oi = OSPF6_INTERFACE (getdata (node));

      if (OSPF6_LSA_SCOPE (lsa->header->type) == OSPF6_SCOPE_LINKLOCAL &&
          oi != OSPF6_INTERFACE (lsa->lsdb->data))
        continue;

#if 0
      if (OSPF6_LSA_SCOPE (lsa->header->type) == OSPF6_SCOPE_AS &&
          ospf6_is_interface_virtual_link (oi))
        continue;
#endif/*0*/

      ospf6_flood_interface (from, lsa, oi);
    }
}

void
ospf6_flood_process (struct ospf6_neighbor *from,
                     struct ospf6_lsa *lsa, struct ospf6 *process)
{
  listnode node;
  struct ospf6_area *oa;

  for (node = listhead (process->area_list); node; nextnode (node))
    {
      oa = OSPF6_AREA (getdata (node));

      if (OSPF6_LSA_SCOPE (lsa->header->type) == OSPF6_SCOPE_AREA &&
          oa != OSPF6_AREA (lsa->lsdb->data))
        continue;
      if (OSPF6_LSA_SCOPE (lsa->header->type) == OSPF6_SCOPE_LINKLOCAL &&
          oa != OSPF6_INTERFACE (lsa->lsdb->data)->area)
        continue;

      if (ntohs (lsa->header->type) == OSPF6_LSTYPE_AS_EXTERNAL &&
          IS_AREA_STUB (oa))
        continue;

      ospf6_flood_area (from, lsa, oa);
    }
}

void
ospf6_flood (struct ospf6_neighbor *from, struct ospf6_lsa *lsa)
{
  ospf6_flood_process (from, lsa, ospf6);
}

void
ospf6_flood_clear_interface (struct ospf6_lsa *lsa, struct ospf6_interface *oi)
{
  listnode node;
  struct ospf6_neighbor *on;
  struct ospf6_lsa *rem;

  for (node = listhead (oi->neighbor_list); node; nextnode (node))
    {
      on = OSPF6_NEIGHBOR (getdata (node));
      rem = ospf6_lsdb_lookup (lsa->header->type, lsa->header->id,
                               lsa->header->adv_router, on->retrans_list);
      if (rem && ! ospf6_lsa_compare (rem, lsa))
        {
          if (IS_OSPF6_DEBUG_FLOODING ||
              IS_OSPF6_DEBUG_FLOOD_TYPE (lsa->header->type))
            zlog_info ("Remove %s from retrans_list of %s",
                       rem->name, on->name);
          ospf6_decrement_retrans_count (rem);
          ospf6_lsdb_remove (rem, on->retrans_list);
        }
    }
}

void
ospf6_flood_clear_area (struct ospf6_lsa *lsa, struct ospf6_area *oa)
{
  listnode node;
  struct ospf6_interface *oi;

  for (node = listhead (oa->if_list); node; nextnode (node))
    {
      oi = OSPF6_INTERFACE (getdata (node));

      if (OSPF6_LSA_SCOPE (lsa->header->type) == OSPF6_SCOPE_LINKLOCAL &&
          oi != OSPF6_INTERFACE (lsa->lsdb->data))
        continue;

#if 0
      if (OSPF6_LSA_SCOPE (lsa->header->type) == OSPF6_SCOPE_AS &&
          ospf6_is_interface_virtual_link (oi))
        continue;
#endif/*0*/

      ospf6_flood_clear_interface (lsa, oi);
    }
}

void
ospf6_flood_clear_process (struct ospf6_lsa *lsa, struct ospf6 *process)
{
  listnode node;
  struct ospf6_area *oa;

  for (node = listhead (process->area_list); node; nextnode (node))
    {
      oa = OSPF6_AREA (getdata (node));

      if (OSPF6_LSA_SCOPE (lsa->header->type) == OSPF6_SCOPE_AREA &&
          oa != OSPF6_AREA (lsa->lsdb->data))
        continue;
      if (OSPF6_LSA_SCOPE (lsa->header->type) == OSPF6_SCOPE_LINKLOCAL &&
          oa != OSPF6_INTERFACE (lsa->lsdb->data)->area)
        continue;

      if (ntohs (lsa->header->type) == OSPF6_LSTYPE_AS_EXTERNAL &&
          IS_AREA_STUB (oa))
        continue;

      ospf6_flood_clear_area (lsa, oa);
    }
}

void
ospf6_flood_clear (struct ospf6_lsa *lsa)
{
  ospf6_flood_clear_process (lsa, ospf6);
}


/* RFC2328 13.5 (Table 19): Sending link state acknowledgements. */
static void
ospf6_acknowledge_lsa_bdrouter (struct ospf6_lsa *lsa, int ismore_recent,
                                struct ospf6_neighbor *from)
{
  struct ospf6_interface *oi;
  int is_debug = 0;

  if (IS_OSPF6_DEBUG_FLOODING ||
      IS_OSPF6_DEBUG_FLOOD_TYPE (lsa->header->type))
    is_debug++;

  assert (from && from->ospf6_if);
  oi = from->ospf6_if;

  /* LSA has been flood back out receiving interface.
     No acknowledgement sent. */
  if (CHECK_FLAG (lsa->flag, OSPF6_LSA_FLOODBACK))
    {
      if (is_debug)
        zlog_info ("No acknowledgement (BDR & FloodBack)");
      return;
    }

  /* LSA is more recent than database copy, but was not flooded
     back out receiving interface. Delayed acknowledgement sent
     if advertisement received from Designated Router,
     otherwide do nothing. */
  if (ismore_recent < 0)
    {
      if (oi->drouter == from->router_id)
        {
          if (is_debug)
            zlog_info ("Delayed acknowledgement (BDR & MoreRecent & from DR)");
          /* Delayed acknowledgement */
          ospf6_lsdb_add (ospf6_lsa_copy (lsa), oi->lsack_list);
          if (oi->thread_send_lsack == NULL)
            oi->thread_send_lsack =
              thread_add_timer (master, ospf6_lsack_send_interface, oi, 3);
        }
      else
        {
          if (is_debug)
            zlog_info ("No acknowledgement (BDR & MoreRecent & ! from DR)");
        }
      return;
    }

  /* LSA is a duplicate, and was treated as an implied acknowledgement.
     Delayed acknowledgement sent if advertisement received from
     Designated Router, otherwise do nothing */
  if (CHECK_FLAG (lsa->flag, OSPF6_LSA_DUPLICATE) &&
      CHECK_FLAG (lsa->flag, OSPF6_LSA_IMPLIEDACK))
    {
      if (oi->drouter == from->router_id)
        {
          if (is_debug)
            zlog_info ("Delayed acknowledgement (BDR & Duplicate & ImpliedAck & from DR)");
          /* Delayed acknowledgement */
          ospf6_lsdb_add (ospf6_lsa_copy (lsa), oi->lsack_list);
          if (oi->thread_send_lsack == NULL)
            oi->thread_send_lsack =
              thread_add_timer (master, ospf6_lsack_send_interface, oi, 3);
        }
      else
        {
          if (is_debug)
            zlog_info ("No acknowledgement (BDR & Duplicate & ImpliedAck & ! from DR)");
        }
      return;
    }

  /* LSA is a duplicate, and was not treated as an implied acknowledgement.
     Direct acknowledgement sent */
  if (CHECK_FLAG (lsa->flag, OSPF6_LSA_DUPLICATE) &&
      ! CHECK_FLAG (lsa->flag, OSPF6_LSA_IMPLIEDACK))
    {
      if (is_debug)
        zlog_info ("Direct acknowledgement (BDR & Duplicate)");
      ospf6_lsdb_add (ospf6_lsa_copy (lsa), from->lsack_list);
      if (from->thread_send_lsack == NULL)
        from->thread_send_lsack =
          thread_add_event (master, ospf6_lsack_send_neighbor, from, 0);
      return;
    }

  /* LSA's LS age is equal to Maxage, and there is no current instance
     of the LSA in the link state database, and none of router's
     neighbors are in states Exchange or Loading */
  /* Direct acknowledgement sent, but this case is handled in
     early of ospf6_receive_lsa () */
}

static void
ospf6_acknowledge_lsa_allother (struct ospf6_lsa *lsa, int ismore_recent,
                                struct ospf6_neighbor *from)
{
  struct ospf6_interface *oi;
  int is_debug = 0;

  if (IS_OSPF6_DEBUG_FLOODING ||
      IS_OSPF6_DEBUG_FLOOD_TYPE (lsa->header->type))
    is_debug++;

  assert (from && from->ospf6_if);
  oi = from->ospf6_if;

  /* LSA has been flood back out receiving interface.
     No acknowledgement sent. */
  if (CHECK_FLAG (lsa->flag, OSPF6_LSA_FLOODBACK))
    {
      if (is_debug)
        zlog_info ("No acknowledgement (AllOther & FloodBack)");
      return;
    }

  /* LSA is more recent than database copy, but was not flooded
     back out receiving interface. Delayed acknowledgement sent. */
  if (ismore_recent < 0)
    {
      if (is_debug)
        zlog_info ("Delayed acknowledgement (AllOther & MoreRecent)");
      /* Delayed acknowledgement */
      ospf6_lsdb_add (ospf6_lsa_copy (lsa), oi->lsack_list);
      if (oi->thread_send_lsack == NULL)
        oi->thread_send_lsack =
          thread_add_timer (master, ospf6_lsack_send_interface, oi, 3);
      return;
    }

  /* LSA is a duplicate, and was treated as an implied acknowledgement.
     No acknowledgement sent. */
  if (CHECK_FLAG (lsa->flag, OSPF6_LSA_DUPLICATE) &&
      CHECK_FLAG (lsa->flag, OSPF6_LSA_IMPLIEDACK))
    {
      if (is_debug)
        zlog_info ("No acknowledgement (AllOther & Duplicate & ImpliedAck)");
      return;
    }

  /* LSA is a duplicate, and was not treated as an implied acknowledgement.
     Direct acknowledgement sent */
  if (CHECK_FLAG (lsa->flag, OSPF6_LSA_DUPLICATE) &&
      ! CHECK_FLAG (lsa->flag, OSPF6_LSA_IMPLIEDACK))
    {
      if (is_debug)
        zlog_info ("Direct acknowledgement (AllOther & Duplicate)");
      ospf6_lsdb_add (ospf6_lsa_copy (lsa), from->lsack_list);
      if (from->thread_send_lsack == NULL)
        from->thread_send_lsack =
          thread_add_event (master, ospf6_lsack_send_neighbor, from, 0);
      return;
    }

  /* LSA's LS age is equal to Maxage, and there is no current instance
     of the LSA in the link state database, and none of router's
     neighbors are in states Exchange or Loading */
  /* Direct acknowledgement sent, but this case is handled in
     early of ospf6_receive_lsa () */
}

void
ospf6_acknowledge_lsa (struct ospf6_lsa *lsa, int ismore_recent,
                       struct ospf6_neighbor *from)
{
  struct ospf6_interface *oi;

  assert (from && from->ospf6_if);
  oi = from->ospf6_if;

  if (oi->state == OSPF6_INTERFACE_BDR)
    ospf6_acknowledge_lsa_bdrouter (lsa, ismore_recent, from);
  else
    ospf6_acknowledge_lsa_allother (lsa, ismore_recent, from);
}

/* RFC2328 section 13 (4):
   if MaxAge LSA and if we have no instance, and no neighbor
   is in states Exchange or Loading
   returns 1 if match this case, else returns 0 */
static int
ospf6_is_maxage_lsa_drop (struct ospf6_lsa *lsa, struct ospf6_neighbor *from)
{
  struct ospf6_neighbor *on;
  struct ospf6_interface *oi;
  struct ospf6_area *oa;
  struct ospf6 *process = NULL;
  listnode i, j, k;
  int count = 0;

  if (! OSPF6_LSA_IS_MAXAGE (lsa))
    return 0;

  if (ospf6_lsdb_lookup (lsa->header->type, lsa->header->id,
                         lsa->header->adv_router, lsa->lsdb))
    return 0;

  process = from->ospf6_if->area->ospf6;
  for (i = listhead (process->area_list); i; nextnode (i))
    {
      oa = OSPF6_AREA (getdata (i));
      for (j = listhead (oa->if_list); j; nextnode (j))
        {
          oi = OSPF6_INTERFACE (getdata (j));
          for (k = listhead (oi->neighbor_list); k; nextnode (k))
            {
              on = OSPF6_NEIGHBOR (getdata (k));
              if (on->state == OSPF6_NEIGHBOR_EXCHANGE ||
                  on->state == OSPF6_NEIGHBOR_LOADING)
                count++;
            }
        }
    }

  if (count == 0)
    return 1;
  return 0;
}

/* RFC2328 section 13 The Flooding Procedure */
void
ospf6_receive_lsa (struct ospf6_neighbor *from,
                   struct ospf6_lsa_header *lsa_header)
{
  struct ospf6_lsa *new = NULL, *old = NULL, *rem = NULL;
  int ismore_recent;
  unsigned short cksum;
  int is_debug = 0;

  ismore_recent = 1;
  assert (from);

  /* make lsa structure for received lsa */
  new = ospf6_lsa_create (lsa_header);

  if (IS_OSPF6_DEBUG_FLOODING ||
      IS_OSPF6_DEBUG_FLOOD_TYPE (new->header->type))
    {
      is_debug++;
      zlog_info ("LSA Receive from %s", from->name);
      ospf6_lsa_header_print (new);
    }

  /* (1) LSA Checksum */
  cksum = ntohs (new->header->checksum);
  if (ntohs (ospf6_lsa_checksum (new->header)) != cksum)
    {
      if (is_debug)
        zlog_info ("Wrong LSA Checksum, discard");
      ospf6_lsa_delete (new);
      return;
    }

  /* (2) Examine the LSA's LS type. 
     RFC2470 3.5.1. Receiving Link State Update packets  */
  if (IS_AREA_STUB (from->ospf6_if->area) &&
      OSPF6_LSA_SCOPE (new->header->type) == OSPF6_SCOPE_AS)
    {
      if (is_debug)
        zlog_info ("AS-External-LSA (or AS-scope LSA) in stub area, discard");
      ospf6_lsa_delete (new);
      return;
    }

  /* (3) LSA which have reserved scope is discarded
     RFC2470 3.5.1. Receiving Link State Update packets  */
  /* Flooding scope check. LSAs with unknown scope are discarded here.
     Set appropriate LSDB for the LSA */
  switch (OSPF6_LSA_SCOPE (new->header->type))
    {
    case OSPF6_SCOPE_LINKLOCAL:
      new->lsdb = from->ospf6_if->lsdb;
      break;
    case OSPF6_SCOPE_AREA:
      new->lsdb = from->ospf6_if->area->lsdb;
      break;
    case OSPF6_SCOPE_AS:
      new->lsdb = from->ospf6_if->area->ospf6->lsdb;
      break;
    default:
      if (is_debug)
        zlog_info ("LSA has reserved scope, discard");
      ospf6_lsa_delete (new);
      return;
    }

  /* (4) if MaxAge LSA and if we have no instance, and no neighbor
         is in states Exchange or Loading */
  if (ospf6_is_maxage_lsa_drop (new, from))
    {
      /* log */
      if (is_debug)
        zlog_info ("Drop MaxAge LSA with direct acknowledgement.");

      /* a) Acknowledge back to neighbor (Direct acknowledgement, 13.5) */
      ospf6_lsdb_add (ospf6_lsa_copy (new), from->lsack_list);
      if (from->thread_send_lsack == NULL)
        from->thread_send_lsack =
          thread_add_event (master, ospf6_lsack_send_neighbor, from, 0);

      /* b) Discard */
      ospf6_lsa_delete (new);
      return;
    }

  /* (5) */
  /* lookup the same database copy in lsdb */
  old = ospf6_lsdb_lookup (new->header->type, new->header->id,
                           new->header->adv_router, new->lsdb);
  if (old)
    {
      ismore_recent = ospf6_lsa_compare (new, old);
      if (ntohl (new->header->seqnum) == ntohl (old->header->seqnum))
        {
          if (is_debug)
            zlog_info ("Received is duplicated LSA");
          SET_FLAG (new->flag, OSPF6_LSA_DUPLICATE);
        }
    }

  /* if no database copy or received is more recent */
  if (old == NULL || ismore_recent < 0)
    {
      /* in case we have no database copy */
      ismore_recent = -1;

      /* (a) MinLSArrival check */
      if (old)
        {
          struct timeval now, res;
          gettimeofday (&now, (struct timezone *) NULL);
          timersub (&now, &old->installed, &res);
          if (res.tv_sec < MIN_LS_ARRIVAL)
            {
              if (is_debug)
                zlog_info ("LSA can't be updated within MinLSArrival, discard");
              ospf6_lsa_delete (new);
              return;   /* examin next lsa */
            }
        }

      gettimeofday (&new->received, (struct timezone *) NULL);

      if (is_debug)
        zlog_info ("Flood, Install, Possibly acknowledge the received LSA");

      /* (b) immediately flood and (c) remove from all retrans-list */
      /* Prevent self-originated LSA to be flooded. this is to make
      reoriginated instance of the LSA not to be rejected by other routers
      due to MinLSArrival. */
      if (new->header->adv_router != from->ospf6_if->area->ospf6->router_id)
        ospf6_flood (from, new);

      /* (c) Remove the current database copy from all neighbors' Link
             state retransmission lists. */
      /* XXX, flood_clear ? */

      /* (d), installing lsdb, which may cause routing
              table calculation (replacing database copy) */
      ospf6_install_lsa (new);

      /* (e) possibly acknowledge */
      ospf6_acknowledge_lsa (new, ismore_recent, from);

      /* (f) Self Originated LSA, section 13.4 */
      if (new->header->adv_router == from->ospf6_if->area->ospf6->router_id)
        {
          /* Self-originated LSA (newer than ours) is received from
             another router. We have to make a new instance of the LSA
             or have to flush this LSA. */
          if (is_debug)
            {
              zlog_info ("Newer instance of the self-originated LSA");
              zlog_info ("Schedule reorigination");
            }
          new->refresh = thread_add_event (master, ospf6_lsa_refresh, new, 0);
        }

      return;
    }

  /* (6) if there is instance on sending neighbor's request list */
  if (ospf6_lsdb_lookup (new->header->type, new->header->id,
                         new->header->adv_router, from->request_list))
    {
      /* if no database copy, should go above state (5) */
      assert (old);

      if (is_debug)
        {
          zlog_info ("Received is not newer, on the neighbor's request-list");
          zlog_info ("BadLSReq, discard the received LSA");
        }

      /* BadLSReq */
      thread_add_event (master, bad_lsreq, from, 0);

      ospf6_lsa_delete (new);
      return;
    }

  /* (7) if neither one is more recent */
  if (ismore_recent == 0)
    {
      if (is_debug)
        zlog_info ("The same instance as database copy (neither recent)");

      /* (a) if on retrans-list, Treat this LSA as an Ack: Implied Ack */
      rem = ospf6_lsdb_lookup (new->header->type, new->header->id,
                               new->header->adv_router, from->retrans_list);
      if (rem)
        {
          if (is_debug)
            {
              zlog_info ("It is on the neighbor's retrans-list.");
              zlog_info ("Treat as an Implied acknowledgement");
            }
          SET_FLAG (new->flag, OSPF6_LSA_IMPLIEDACK);
          ospf6_decrement_retrans_count (rem);
          ospf6_lsdb_remove (rem, from->retrans_list);
        }

      if (is_debug)
        zlog_info ("Possibly acknowledge and then discard");

      /* (b) possibly acknowledge */
      ospf6_acknowledge_lsa (new, ismore_recent, from);

      ospf6_lsa_delete (new);
      return;
    }

  /* (8) previous database copy is more recent */
    {
      assert (old);

      /* If database copy is in 'Seqnumber Wrapping',
         simply discard the received LSA */
      if (OSPF6_LSA_IS_MAXAGE (old) &&
          old->header->seqnum == htonl (MAX_SEQUENCE_NUMBER))
        {
          if (is_debug)
            {
              zlog_info ("The LSA is in Seqnumber Wrapping");
              zlog_info ("MaxAge & MaxSeqNum, discard");
            }
          ospf6_lsa_delete (new);
          return;
        }

      /* Otherwise, Send database copy of this LSA to this neighbor */
        {
          if (is_debug)
            {
              zlog_info ("Database copy is more recent.");
              zlog_info ("Send back directly and then discard");
            }

          /* XXX, MinLSArrival check !? RFC 2328 13 (8) */

          ospf6_lsdb_add (ospf6_lsa_copy (old), from->lsupdate_list);
          if (from->thread_send_lsupdate == NULL)
            from->thread_send_lsupdate =
              thread_add_event (master, ospf6_lsupdate_send_neighbor, from, 0);
          ospf6_lsa_delete (new);
          return;
        }
      return;
    }
}


DEFUN (debug_ospf6_flooding,
       debug_ospf6_flooding_cmd,
       "debug ospf6 flooding",
       DEBUG_STR
       OSPF6_STR
       "Debug OSPFv3 flooding function\n"
      )
{
  OSPF6_DEBUG_FLOODING_ON ();
  return CMD_SUCCESS;
}

DEFUN (no_debug_ospf6_flooding,
       no_debug_ospf6_flooding_cmd,
       "no debug ospf6 flooding",
       NO_STR
       DEBUG_STR
       OSPF6_STR
       "Debug OSPFv3 flooding function\n"
      )
{
  OSPF6_DEBUG_FLOODING_OFF ();
  return CMD_SUCCESS;
}

int
config_write_ospf6_debug_flood (struct vty *vty)
{
  if (IS_OSPF6_DEBUG_FLOODING)
    vty_out (vty, "debug ospf6 flooding%s", VNL);
  return 0;
}

void
install_element_ospf6_debug_flood ()
{
  install_element (ENABLE_NODE, &debug_ospf6_flooding_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf6_flooding_cmd);
  install_element (CONFIG_NODE, &debug_ospf6_flooding_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf6_flooding_cmd);
}





