/*
 * OSPF version 2  Neighbor State Machine
 * From RFC2328 [OSPF Version 2]
 * Copyright (C) 1999, 2000 Toshiaki Takada
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

#include "thread.h"
#include "memory.h"
#include "hash.h"
#include "linklist.h"
#include "prefix.h"
#include "if.h"
#include "table.h"
#include "stream.h"
#include "table.h"
#include "log.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_interface.h"
#include "ospfd/ospf_ism.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_lsdb.h"
#include "ospfd/ospf_neighbor.h"
#include "ospfd/ospf_nsm.h"
#include "ospfd/ospf_network.h"
#include "ospfd/ospf_packet.h"
#include "ospfd/ospf_dump.h"
#include "ospfd/ospf_flood.h"
#include "ospfd/ospf_abr.h"

void nsm_reset_nbr (struct ospf_neighbor *);


/* OSPF NSM Timer functions. */
int
ospf_inactivity_timer (struct thread *thread)
{
  struct ospf_neighbor *nbr;

  nbr = THREAD_ARG (thread);
  nbr->t_inactivity = NULL;

  if (IS_DEBUG_OSPF (nsm, NSM_TIMERS))
    zlog (NULL, LOG_DEBUG, "NSM[%s:%s]: Timer (Inactivity timer expire)",
	  IF_NAME (nbr->oi), inet_ntoa (nbr->router_id));

  OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_InactivityTimer);

  return 0;
}

int
ospf_db_desc_timer (struct thread *thread)
{
  struct ospf_interface *oi;
  struct ospf_neighbor *nbr;

  nbr = THREAD_ARG (thread);
  nbr->t_db_desc = NULL;

  oi = nbr->oi;

  if (IS_DEBUG_OSPF (nsm, NSM_TIMERS))
    zlog (NULL, LOG_INFO, "NSM[%s:%s]: Timer (DD Retransmit timer expire)",
	  IF_NAME (nbr->oi), inet_ntoa (nbr->src));

  /* resent last send DD packet. */
  assert (nbr->last_send);
  ospf_db_desc_resend (nbr);

  /* DD Retransmit timer set. */
  OSPF_NSM_TIMER_ON (nbr->t_db_desc, ospf_db_desc_timer, nbr->v_db_desc);

  return 0;
}

/* Hook function called after ospf NSM event is occured. */

void
nsm_timer_set (struct ospf_neighbor *nbr)
{
  switch (nbr->state)
    {
    case NSM_Down:
      OSPF_NSM_TIMER_OFF (nbr->t_db_desc);
      OSPF_NSM_TIMER_OFF (nbr->t_ls_upd);
      break;
    case NSM_Attempt:
      OSPF_NSM_TIMER_OFF (nbr->t_db_desc);
      OSPF_NSM_TIMER_OFF (nbr->t_ls_upd);
      break;
    case NSM_Init:
      OSPF_NSM_TIMER_OFF (nbr->t_db_desc);
      OSPF_NSM_TIMER_OFF (nbr->t_ls_upd);
      break;
    case NSM_TwoWay:
      OSPF_NSM_TIMER_OFF (nbr->t_db_desc);
      OSPF_NSM_TIMER_OFF (nbr->t_ls_upd);
      break;
    case NSM_ExStart:
      OSPF_NSM_TIMER_ON (nbr->t_db_desc, ospf_db_desc_timer, nbr->v_db_desc);
      OSPF_NSM_TIMER_OFF (nbr->t_ls_upd);
      break;
    case NSM_Exchange:
      OSPF_NSM_TIMER_ON (nbr->t_ls_upd, ospf_ls_upd_timer, nbr->v_ls_upd);
      if (!IS_SET_DD_MS (nbr->dd_flags))      
	OSPF_NSM_TIMER_OFF (nbr->t_db_desc);
      break;
    case NSM_Loading:
      OSPF_NSM_TIMER_OFF (nbr->t_db_desc);
      break;
    case NSM_Full:
      OSPF_NSM_TIMER_OFF (nbr->t_db_desc);
      break;
    default:
      OSPF_NSM_TIMER_OFF (nbr->t_db_desc);
      break;
    }
}


/* OSPF NSM functions. */
int
nsm_ignore (struct ospf_neighbor *nbr)
{
  if (IS_DEBUG_OSPF (nsm, NSM_EVENTS))
    zlog (NULL, LOG_INFO, "NSM[%s:%s]: nsm_ignore called",
	  IF_NAME (nbr->oi), inet_ntoa (nbr->router_id));

  return 0;
}

int
nsm_hello_received (struct ospf_neighbor *nbr)
{
  /* Start or Restart Inactivity Timer. */
  OSPF_NSM_TIMER_OFF (nbr->t_inactivity);
  
  OSPF_NSM_TIMER_ON (nbr->t_inactivity, ospf_inactivity_timer,
		     nbr->v_inactivity);

  if (nbr->oi->type == OSPF_IFTYPE_NBMA && nbr->nbr_nbma)
    OSPF_POLL_TIMER_OFF (nbr->nbr_nbma->t_poll);

  return 0;
}

int
nsm_start (struct ospf_neighbor *nbr)
{

  nsm_reset_nbr (nbr);

  if (nbr->nbr_nbma)
      OSPF_POLL_TIMER_OFF (nbr->nbr_nbma->t_poll);

  OSPF_NSM_TIMER_OFF (nbr->t_inactivity);
  
  OSPF_NSM_TIMER_ON (nbr->t_inactivity, ospf_inactivity_timer,
                     nbr->v_inactivity);

  return 0;
}

int
nsm_twoway_received (struct ospf_neighbor *nbr)
{
  struct ospf_interface *oi;
  int next_state = NSM_TwoWay;

  oi = nbr->oi;

  /* These netowork types must be adjacency. */
  if (oi->type == OSPF_IFTYPE_POINTOPOINT ||
      oi->type == OSPF_IFTYPE_POINTOMULTIPOINT ||
      oi->type == OSPF_IFTYPE_VIRTUALLINK)
    next_state = NSM_ExStart;

  /* Router itself is the DRouter or the BDRouter. */
  if (IPV4_ADDR_SAME (&oi->address->u.prefix4, &DR (oi)) ||
      IPV4_ADDR_SAME (&oi->address->u.prefix4, &BDR (oi)))
    next_state = NSM_ExStart;

  /* Neighboring Router is the DRouter or the BDRouter. */
  if (IPV4_ADDR_SAME (&nbr->address.u.prefix4, &nbr->d_router) ||
      IPV4_ADDR_SAME (&nbr->address.u.prefix4, &nbr->bd_router))
    next_state = NSM_ExStart;

  return next_state;
}

int
ospf_db_summary_count (struct ospf_neighbor *nbr)
{
  return ospf_lsdb_count_all (&nbr->db_sum);
}

int
ospf_db_summary_isempty (struct ospf_neighbor *nbr)
{
  return ospf_lsdb_isempty (&nbr->db_sum);
}

int
ospf_db_summary_add (struct ospf_neighbor *nbr, struct ospf_lsa *lsa)
{
#ifdef HAVE_OPAQUE_LSA
  switch (lsa->data->type)
    {
    case OSPF_OPAQUE_LINK_LSA:
      /* Exclude type-9 LSAs that does not have the same "oi" with "nbr". */
      if (lsa->oi != nbr->oi)
          return 0;
      break;
    case OSPF_OPAQUE_AREA_LSA:
      /*
       * It is assured by the caller function "nsm_negotiation_done()"
       * that every given LSA belongs to the same area with "nbr".
       */
      break;
    case OSPF_OPAQUE_AS_LSA:
    default:
      break;
    }
#endif /* HAVE_OPAQUE_LSA */

#ifdef HAVE_NSSA
  /* Stay away from any Local Translated Type-7 LSAs */
  if (CHECK_FLAG (lsa->flags, OSPF_LSA_LOCAL_XLT))
    return 0;
#endif /* HAVE_NSSA */

  if (IS_LSA_MAXAGE (lsa))
    ospf_ls_retransmit_add (nbr, lsa);                      
  else 
    ospf_lsdb_add (&nbr->db_sum, lsa);

  return 0;
}

void
ospf_db_summary_clear (struct ospf_neighbor *nbr)
{
  struct ospf_lsdb *lsdb;
  int i;

  lsdb = &nbr->db_sum;
  for (i = OSPF_MIN_LSA; i < OSPF_MAX_LSA; i++)
    {
      struct route_table *table = lsdb->type[i].db;
      struct route_node *rn;

      for (rn = route_top (table); rn; rn = route_next (rn))
	if (rn->info)
	  ospf_lsdb_delete (&nbr->db_sum, rn->info);
    }
}



/* The area link state database consists of the router-LSAs,
   network-LSAs and summary-LSAs contained in the area structure,
   along with the AS-external-LSAs contained in the global structure.
   AS-external-LSAs are omitted from a virtual neighbor's Database
   summary list.  AS-external-LSAs are omitted from the Database
   summary list if the area has been configured as a stub. */
int
nsm_negotiation_done (struct ospf_neighbor *nbr)
{
  struct ospf_area *area = nbr->oi->area;
  struct ospf_lsa *lsa;
  struct route_node *rn;

  LSDB_LOOP (ROUTER_LSDB (area), rn, lsa)
    ospf_db_summary_add (nbr, lsa);
  LSDB_LOOP (NETWORK_LSDB (area), rn, lsa)
    ospf_db_summary_add (nbr, lsa);
  LSDB_LOOP (SUMMARY_LSDB (area), rn, lsa)
    ospf_db_summary_add (nbr, lsa);
  LSDB_LOOP (ASBR_SUMMARY_LSDB (area), rn, lsa)
    ospf_db_summary_add (nbr, lsa);

#ifdef HAVE_OPAQUE_LSA
  /* Process only if the neighbor is opaque capable. */
  if (CHECK_FLAG (nbr->options, OSPF_OPTION_O))
    {
      LSDB_LOOP (OPAQUE_LINK_LSDB (area), rn, lsa)
	ospf_db_summary_add (nbr, lsa);
      LSDB_LOOP (OPAQUE_AREA_LSDB (area), rn, lsa)
	ospf_db_summary_add (nbr, lsa);
    }
#endif /* HAVE_OPAQUE_LSA */

  if (nbr->oi->type != OSPF_IFTYPE_VIRTUALLINK
      && area->external_routing == OSPF_AREA_DEFAULT)
    LSDB_LOOP (EXTERNAL_LSDB (nbr->oi->ospf), rn, lsa)
      ospf_db_summary_add (nbr, lsa);

#ifdef HAVE_OPAQUE_LSA
  if (CHECK_FLAG (nbr->options, OSPF_OPTION_O)
      && (nbr->oi->type != OSPF_IFTYPE_VIRTUALLINK
	  && area->external_routing == OSPF_AREA_DEFAULT))
    LSDB_LOOP (OPAQUE_AS_LSDB (nbr->oi->ospf), rn, lsa)
      ospf_db_summary_add (nbr, lsa);
#endif /* HAVE_OPAQUE_LSA */

  return 0;
}

int
nsm_exchange_done (struct ospf_neighbor *nbr)
{
  if (ospf_ls_request_isempty (nbr))
    return NSM_Full;

  /* Cancel dd retransmit timer. */
  /* OSPF_NSM_TIMER_OFF (nbr->t_db_desc); */

  /* Send Link State Request. */
  ospf_ls_req_send (nbr);

  return NSM_Loading;
}

int
nsm_bad_ls_req (struct ospf_neighbor *nbr)
{
  /* Clear neighbor. */
  nsm_reset_nbr (nbr);

  return 0;
}

int
nsm_adj_ok (struct ospf_neighbor *nbr)
{
  struct ospf_interface *oi;
  int next_state;
  int flag = 0;

  oi = nbr->oi;
  next_state = nbr->state;

  /* These netowork types must be adjacency. */
  if (oi->type == OSPF_IFTYPE_POINTOPOINT
      || oi->type == OSPF_IFTYPE_POINTOMULTIPOINT
      || oi->type == OSPF_IFTYPE_VIRTUALLINK)
    flag = 1;

  /* Router itself is the DRouter or the BDRouter. */
  if (IPV4_ADDR_SAME (&oi->address->u.prefix4, &DR (oi))
      || IPV4_ADDR_SAME (&oi->address->u.prefix4, &BDR (oi)))
    flag = 1;

  if (IPV4_ADDR_SAME (&nbr->address.u.prefix4, &DR (oi))
      || IPV4_ADDR_SAME (&nbr->address.u.prefix4, &BDR (oi)))
    flag = 1;

  if (nbr->state == NSM_TwoWay && flag == 1)
    next_state = NSM_ExStart;
  else if (nbr->state >= NSM_ExStart && flag == 0)
    next_state = NSM_TwoWay;

  return next_state;
}

int
nsm_seq_number_mismatch (struct ospf_neighbor *nbr)
{
  /* Clear neighbor. */
  nsm_reset_nbr (nbr);

  return 0;
}

int
nsm_oneway_received (struct ospf_neighbor *nbr)
{
  /* Clear neighbor. */
  nsm_reset_nbr (nbr);

  return 0;
}

void
nsm_reset_nbr (struct ospf_neighbor *nbr)
{
  /* Clear Database Summary list. */
  if (!ospf_db_summary_isempty (nbr))
    ospf_db_summary_clear (nbr);

  /* Clear Link State Request list. */
  if (!ospf_ls_request_isempty (nbr))
    ospf_ls_request_delete_all (nbr);

  /* Clear Link State Retransmission list. */
  if (!ospf_ls_retransmit_isempty (nbr))
    ospf_ls_retransmit_clear (nbr);

  /* Cancel thread. */
  OSPF_NSM_TIMER_OFF (nbr->t_db_desc);
  OSPF_NSM_TIMER_OFF (nbr->t_ls_req);
  OSPF_NSM_TIMER_OFF (nbr->t_ls_upd);
  OSPF_NSM_TIMER_OFF (nbr->t_hello_reply);

#ifdef HAVE_OPAQUE_LSA
  if (CHECK_FLAG (nbr->options, OSPF_OPTION_O))
    UNSET_FLAG (nbr->options, OSPF_OPTION_O);
#endif /* HAVE_OPAQUE_LSA */
}

int
nsm_kill_nbr (struct ospf_neighbor *nbr)
{
  /* call it here because we cannot call it from ospf_nsm_event */
  nsm_change_state (nbr, NSM_Down);
  
  /* Reset neighbor. */
  nsm_reset_nbr (nbr);

  if (nbr->oi->type == OSPF_IFTYPE_NBMA && nbr->nbr_nbma != NULL)
    {
      struct ospf_nbr_nbma *nbr_nbma = nbr->nbr_nbma;

      nbr_nbma->nbr = NULL;
      nbr_nbma->state_change = nbr->state_change;

      nbr->nbr_nbma = NULL;

      OSPF_POLL_TIMER_ON (nbr_nbma->t_poll, ospf_poll_timer,
			  nbr_nbma->v_poll);

      if (IS_DEBUG_OSPF (nsm, NSM_EVENTS))
	zlog_info ("NSM[%s:%s]: Down (PollIntervalTimer scheduled)",
		   IF_NAME (nbr->oi), inet_ntoa (nbr->address.u.prefix4));  
    }

  /* Delete neighbor from interface. */
  ospf_nbr_delete (nbr);

  return 0;
}

int
nsm_inactivity_timer (struct ospf_neighbor *nbr)
{
  /* Kill neighbor. */
  nsm_kill_nbr (nbr);

  return 0;
}

int
nsm_ll_down (struct ospf_neighbor *nbr)
{
  /* Reset neighbor. */
  /*nsm_reset_nbr (nbr);*/
  
  /* Kill neighbor. */
  nsm_kill_nbr (nbr);

  return 0;
}

/* Neighbor State Machine */
struct {
  int (*func) ();
  int next_state;
} NSM [OSPF_NSM_STATE_MAX][OSPF_NSM_EVENT_MAX] =
{
  {
    /* DependUpon: dummy state. */
    { nsm_ignore,              NSM_DependUpon }, /* NoEvent           */
    { nsm_ignore,              NSM_DependUpon }, /* HelloReceived     */
    { nsm_ignore,              NSM_DependUpon }, /* Start             */
    { nsm_ignore,              NSM_DependUpon }, /* 2-WayReceived     */
    { nsm_ignore,              NSM_DependUpon }, /* NegotiationDone   */
    { nsm_ignore,              NSM_DependUpon }, /* ExchangeDone      */
    { nsm_ignore,              NSM_DependUpon }, /* BadLSReq          */
    { nsm_ignore,              NSM_DependUpon }, /* LoadingDone       */
    { nsm_ignore,              NSM_DependUpon }, /* AdjOK?            */
    { nsm_ignore,              NSM_DependUpon }, /* SeqNumberMismatch */
    { nsm_ignore,              NSM_DependUpon }, /* 1-WayReceived     */
    { nsm_ignore,              NSM_DependUpon }, /* KillNbr           */
    { nsm_ignore,              NSM_DependUpon }, /* InactivityTimer   */
    { nsm_ignore,              NSM_DependUpon }, /* LLDown            */
  },
  {
    /* Down: */
    { nsm_ignore,              NSM_DependUpon }, /* NoEvent           */
    { nsm_hello_received,      NSM_Init       }, /* HelloReceived     */
    { nsm_start,               NSM_Attempt    }, /* Start             */
    { nsm_ignore,              NSM_Down       }, /* 2-WayReceived     */
    { nsm_ignore,              NSM_Down       }, /* NegotiationDone   */
    { nsm_ignore,              NSM_Down       }, /* ExchangeDone      */
    { nsm_ignore,              NSM_Down       }, /* BadLSReq          */
    { nsm_ignore,              NSM_Down       }, /* LoadingDone       */
    { nsm_ignore,              NSM_Down       }, /* AdjOK?            */
    { nsm_ignore,              NSM_Down       }, /* SeqNumberMismatch */
    { nsm_ignore,              NSM_Down       }, /* 1-WayReceived     */
    { nsm_kill_nbr,            NSM_Down       }, /* KillNbr           */
    { nsm_inactivity_timer,    NSM_Down       }, /* InactivityTimer   */
    { nsm_ll_down,             NSM_Down       }, /* LLDown            */
  },
  {
    /* Attempt: */
    { nsm_ignore,              NSM_DependUpon }, /* NoEvent           */
    { nsm_hello_received,      NSM_Init       }, /* HelloReceived     */
    { nsm_ignore,              NSM_Attempt    }, /* Start             */
    { nsm_ignore,              NSM_Attempt    }, /* 2-WayReceived     */
    { nsm_ignore,              NSM_Attempt    }, /* NegotiationDone   */
    { nsm_ignore,              NSM_Attempt    }, /* ExchangeDone      */
    { nsm_ignore,              NSM_Attempt    }, /* BadLSReq          */
    { nsm_ignore,              NSM_Attempt    }, /* LoadingDone       */
    { nsm_ignore,              NSM_Attempt    }, /* AdjOK?            */
    { nsm_ignore,              NSM_Attempt    }, /* SeqNumberMismatch */
    { nsm_ignore,              NSM_Attempt    }, /* 1-WayReceived     */
    { nsm_kill_nbr,            NSM_Down       }, /* KillNbr           */
    { nsm_inactivity_timer,    NSM_Down       }, /* InactivityTimer   */
    { nsm_ll_down,             NSM_Down       }, /* LLDown            */
  },
  {
    /* Init: */
    { nsm_ignore,              NSM_DependUpon }, /* NoEvent           */
    { nsm_hello_received,      NSM_Init       }, /* HelloReceived     */
    { nsm_ignore,              NSM_Init       }, /* Start             */
    { nsm_twoway_received,     NSM_DependUpon }, /* 2-WayReceived     */
    { nsm_ignore,              NSM_Init       }, /* NegotiationDone   */
    { nsm_ignore,              NSM_Init       }, /* ExchangeDone      */
    { nsm_ignore,              NSM_Init       }, /* BadLSReq          */
    { nsm_ignore,              NSM_Init       }, /* LoadingDone       */
    { nsm_ignore,              NSM_Init       }, /* AdjOK?            */
    { nsm_ignore,              NSM_Init       }, /* SeqNumberMismatch */
    { nsm_ignore,              NSM_Init       }, /* 1-WayReceived     */
    { nsm_kill_nbr,            NSM_Down       }, /* KillNbr           */
    { nsm_inactivity_timer,    NSM_Down       }, /* InactivityTimer   */
    { nsm_ll_down,             NSM_Down       }, /* LLDown            */
  },
  {
    /* 2-Way: */
    { nsm_ignore,              NSM_DependUpon }, /* NoEvent           */
    { nsm_hello_received,      NSM_TwoWay     }, /* HelloReceived     */
    { nsm_ignore,              NSM_TwoWay     }, /* Start             */
    { nsm_ignore,              NSM_TwoWay     }, /* 2-WayReceived     */
    { nsm_ignore,              NSM_TwoWay     }, /* NegotiationDone   */
    { nsm_ignore,              NSM_TwoWay     }, /* ExchangeDone      */
    { nsm_ignore,              NSM_TwoWay     }, /* BadLSReq          */
    { nsm_ignore,              NSM_TwoWay     }, /* LoadingDone       */
    { nsm_adj_ok,              NSM_DependUpon }, /* AdjOK?            */
    { nsm_ignore,              NSM_TwoWay     }, /* SeqNumberMismatch */
    { nsm_oneway_received,     NSM_Init       }, /* 1-WayReceived     */
    { nsm_kill_nbr,            NSM_Down       }, /* KillNbr           */
    { nsm_inactivity_timer,    NSM_Down       }, /* InactivityTimer   */
    { nsm_ll_down,             NSM_Down       }, /* LLDown            */
  },
  {
    /* ExStart: */
    { nsm_ignore,              NSM_DependUpon }, /* NoEvent           */
    { nsm_hello_received,      NSM_ExStart    }, /* HelloReceived     */
    { nsm_ignore,              NSM_ExStart    }, /* Start             */
    { nsm_ignore,              NSM_ExStart    }, /* 2-WayReceived     */
    { nsm_negotiation_done,    NSM_Exchange   }, /* NegotiationDone   */
    { nsm_ignore,              NSM_ExStart    }, /* ExchangeDone      */
    { nsm_ignore,              NSM_ExStart    }, /* BadLSReq          */
    { nsm_ignore,              NSM_ExStart    }, /* LoadingDone       */
    { nsm_adj_ok,              NSM_DependUpon }, /* AdjOK?            */
    { nsm_ignore,              NSM_ExStart    }, /* SeqNumberMismatch */
    { nsm_oneway_received,     NSM_Init       }, /* 1-WayReceived     */
    { nsm_kill_nbr,            NSM_Down       }, /* KillNbr           */
    { nsm_inactivity_timer,    NSM_Down       }, /* InactivityTimer   */
    { nsm_ll_down,             NSM_Down       }, /* LLDown            */
  },
  {
    /* Exchange: */
    { nsm_ignore,              NSM_DependUpon }, /* NoEvent           */
    { nsm_hello_received,      NSM_Exchange   }, /* HelloReceived     */
    { nsm_ignore,              NSM_Exchange   }, /* Start             */
    { nsm_ignore,              NSM_Exchange   }, /* 2-WayReceived     */
    { nsm_ignore,              NSM_Exchange   }, /* NegotiationDone   */
    { nsm_exchange_done,       NSM_DependUpon }, /* ExchangeDone      */
    { nsm_bad_ls_req,          NSM_ExStart    }, /* BadLSReq          */
    { nsm_ignore,              NSM_Exchange   }, /* LoadingDone       */
    { nsm_adj_ok,              NSM_DependUpon }, /* AdjOK?            */
    { nsm_seq_number_mismatch, NSM_ExStart    }, /* SeqNumberMismatch */
    { nsm_oneway_received,     NSM_Init       }, /* 1-WayReceived     */
    { nsm_kill_nbr,            NSM_Down       }, /* KillNbr           */
    { nsm_inactivity_timer,    NSM_Down       }, /* InactivityTimer   */
    { nsm_ll_down,             NSM_Down       }, /* LLDown            */
  },
  {
    /* Loading: */
    { nsm_ignore,              NSM_DependUpon }, /* NoEvent           */
    { nsm_hello_received,      NSM_Loading    }, /* HelloReceived     */
    { nsm_ignore,              NSM_Loading    }, /* Start             */
    { nsm_ignore,              NSM_Loading    }, /* 2-WayReceived     */
    { nsm_ignore,              NSM_Loading    }, /* NegotiationDone   */
    { nsm_ignore,              NSM_Loading    }, /* ExchangeDone      */
    { nsm_bad_ls_req,          NSM_ExStart    }, /* BadLSReq          */
    { nsm_ignore,              NSM_Full       }, /* LoadingDone       */
    { nsm_adj_ok,              NSM_DependUpon }, /* AdjOK?            */
    { nsm_seq_number_mismatch, NSM_ExStart    }, /* SeqNumberMismatch */
    { nsm_oneway_received,     NSM_Init       }, /* 1-WayReceived     */
    { nsm_kill_nbr,            NSM_Down       }, /* KillNbr           */
    { nsm_inactivity_timer,    NSM_Down       }, /* InactivityTimer   */
    { nsm_ll_down,             NSM_Down       }, /* LLDown            */
  },
  { /* Full: */
    { nsm_ignore,              NSM_DependUpon }, /* NoEvent           */
    { nsm_hello_received,      NSM_Full       }, /* HelloReceived     */
    { nsm_ignore,              NSM_Full       }, /* Start             */
    { nsm_ignore,              NSM_Full       }, /* 2-WayReceived     */
    { nsm_ignore,              NSM_Full       }, /* NegotiationDone   */
    { nsm_ignore,              NSM_Full       }, /* ExchangeDone      */
    { nsm_bad_ls_req,          NSM_ExStart    }, /* BadLSReq          */
    { nsm_ignore,              NSM_Full       }, /* LoadingDone       */
    { nsm_adj_ok,              NSM_DependUpon }, /* AdjOK?            */
    { nsm_seq_number_mismatch, NSM_ExStart    }, /* SeqNumberMismatch */
    { nsm_oneway_received,     NSM_Init       }, /* 1-WayReceived     */
    { nsm_kill_nbr,            NSM_Down       }, /* KillNbr           */
    { nsm_inactivity_timer,    NSM_Down       }, /* InactivityTimer   */
    { nsm_ll_down,             NSM_Down       }, /* LLDown            */
  },
};

static char *ospf_nsm_event_str[] =
{
  "NoEvent",
  "HelloReceived",
  "Start",
  "2-WayReceived",
  "NegotiationDone",
  "ExchangeDone",
  "BadLSReq",
  "LoadingDone",
  "AdjOK?",
  "SeqNumberMismatch",
  "1-WayReceived",
  "KillNbr",
  "InactivityTimer",
  "LLDown",
};

void
nsm_change_state (struct ospf_neighbor *nbr, int state)
{
  struct ospf_interface *oi = nbr->oi;
  struct ospf_area *vl_area = NULL;
  u_char old_state;
  int x;
  int force = 1;
  
  /* Logging change of status. */
  if (IS_DEBUG_OSPF (nsm, NSM_STATUS))
    zlog_info ("NSM[%s:%s]: State change %s -> %s",
	       IF_NAME (nbr->oi), inet_ntoa (nbr->router_id),
	       LOOKUP (ospf_nsm_state_msg, nbr->state),
	       LOOKUP (ospf_nsm_state_msg, state));

  /* Preserve old status. */
  old_state = nbr->state;

  /* Change to new status. */
  nbr->state = state;

  /* Statistics. */
  nbr->state_change++;

  if (oi->type == OSPF_IFTYPE_VIRTUALLINK)
    vl_area = ospf_area_lookup_by_area_id (oi->ospf, oi->vl_data->vl_area_id);
  
  /* One of the neighboring routers changes to/from the FULL state. */
  if ((old_state != NSM_Full && state == NSM_Full) ||
      (old_state == NSM_Full && state != NSM_Full))
    { 
      if (state == NSM_Full)
	{
	  oi->full_nbrs++;
	  oi->area->full_nbrs++;

          ospf_check_abr_status (oi->ospf);

	  if (oi->type == OSPF_IFTYPE_VIRTUALLINK && vl_area)
            if (++vl_area->full_vls == 1)
	      ospf_schedule_abr_task (oi->ospf);

	  /* kevinm: refresh any redistributions */
	  for (x = ZEBRA_ROUTE_SYSTEM; x < ZEBRA_ROUTE_MAX; x++)
	    {
	      if (x == ZEBRA_ROUTE_OSPF || x == ZEBRA_ROUTE_OSPF6)
		continue;
	      ospf_external_lsa_refresh_type (oi->ospf, x, force);
	    }
	}
      else
	{
	  oi->full_nbrs--;
	  oi->area->full_nbrs--;

          ospf_check_abr_status (oi->ospf);

	  if (oi->type == OSPF_IFTYPE_VIRTUALLINK && vl_area)
	    if (vl_area->full_vls > 0)
	      if (--vl_area->full_vls == 0)
		ospf_schedule_abr_task (oi->ospf);
 
          /* clear neighbor retransmit list */
          if (!ospf_ls_retransmit_isempty (nbr))
            ospf_ls_retransmit_clear (nbr);
	}

      zlog_info ("nsm_change_state(): "
		 "scheduling new router-LSA origination");

      ospf_router_lsa_timer_add (oi->area);

      if (oi->type == OSPF_IFTYPE_VIRTUALLINK)
	{
	  struct ospf_area *vl_area =
	    ospf_area_lookup_by_area_id (oi->ospf, oi->vl_data->vl_area_id);
	  
	  if (vl_area)
	    ospf_router_lsa_timer_add (vl_area);
	}

      /* Originate network-LSA. */
      if (oi->state == ISM_DR)
	{
	  if (oi->network_lsa_self && oi->full_nbrs == 0)
	    {
	      ospf_lsa_flush_area (oi->network_lsa_self, oi->area);
	      ospf_lsa_unlock (oi->network_lsa_self);
	      oi->network_lsa_self = NULL;
	      OSPF_TIMER_OFF (oi->t_network_lsa_self);
	    }
	  else
	    ospf_network_lsa_timer_add (oi);
	}
    }

#ifdef HAVE_OPAQUE_LSA
  ospf_opaque_nsm_change (nbr, old_state);
#endif /* HAVE_OPAQUE_LSA */

  /* Start DD exchange protocol */
  if (state == NSM_ExStart)
    {
      if (nbr->dd_seqnum == 0)
	nbr->dd_seqnum = time (NULL);
      else
	nbr->dd_seqnum++;

      nbr->dd_flags = OSPF_DD_FLAG_I|OSPF_DD_FLAG_M|OSPF_DD_FLAG_MS;
      ospf_db_desc_send (nbr);
    }

  /* clear cryptographic sequence number */
  if (state == NSM_Down)
    nbr->crypt_seqnum = 0;
  
  /* Generete NeighborChange ISM event. */
#ifdef BUGGY_ISM_TRANSITION
  if ((old_state < NSM_TwoWay && state >= NSM_TwoWay) ||
      (old_state >= NSM_TwoWay && state < NSM_TwoWay))
    OSPF_ISM_EVENT_EXECUTE (oi, ISM_NeighborChange);
#else /* BUGGY_ISM_TRANSITION */
  switch (oi->state) {
  case ISM_DROther:
  case ISM_Backup:
  case ISM_DR:
    if ((old_state < NSM_TwoWay && state >= NSM_TwoWay) ||
        (old_state >= NSM_TwoWay && state < NSM_TwoWay))
      OSPF_ISM_EVENT_EXECUTE (oi, ISM_NeighborChange);
    break;
  default:
    /* ISM_PointToPoint -> ISM_Down, ISM_Loopback -> ISM_Down, etc. */
    break;
  }
#endif /* BUGGY_ISM_TRANSITION */

  /* Performance hack. Send hello immideately when some neighbor enter
     Init state.  This whay we decrease neighbor discovery time. Gleb.*/
  if (state == NSM_Init)
    {
      OSPF_ISM_TIMER_OFF (oi->t_hello);
      OSPF_ISM_TIMER_ON (oi->t_hello, ospf_hello_timer, 1);
    }

  /* Preserve old status? */
}

/* Execute NSM event process. */
int
ospf_nsm_event (struct thread *thread)
{
  int event;
  int next_state;
  struct ospf_neighbor *nbr;
  struct in_addr router_id;
  int old_state;
  struct ospf_interface *oi;

  nbr = THREAD_ARG (thread);
  event = THREAD_VAL (thread);
  router_id = nbr->router_id;

  old_state = nbr->state;
  oi = nbr->oi ;
  
  /* Call function. */
  next_state = (*(NSM [nbr->state][event].func))(nbr);

  /* When event is NSM_KillNbr or InactivityTimer, the neighbor is
     deleted. */
  if (event == NSM_KillNbr || event == NSM_InactivityTimer)
    {
      if (IS_DEBUG_OSPF (nsm, NSM_EVENTS))
	zlog_info ("NSM[%s:%s]: neighbor deleted",
		   IF_NAME (oi), inet_ntoa (router_id));

      /* Timers are canceled in ospf_nbr_free, moreover we cannot call
         nsm_timer_set here because nbr is freed already!!!*/
      /*nsm_timer_set (nbr);*/

      return 0;
    }

  if (! next_state)
    next_state = NSM [nbr->state][event].next_state;

  if (IS_DEBUG_OSPF (nsm, NSM_EVENTS))
    zlog_info ("NSM[%s:%s]: %s (%s)", IF_NAME (oi),
	       inet_ntoa (nbr->router_id),
	       LOOKUP (ospf_nsm_state_msg, nbr->state),
	       ospf_nsm_event_str [event]);
  
  /* If state is changed. */
  if (next_state != nbr->state)
    nsm_change_state (nbr, next_state);

  /* Make sure timer is set. */
  nsm_timer_set (nbr);

  return 0;
}

/* Check loading state. */
void
ospf_check_nbr_loading (struct ospf_neighbor *nbr)
{
  if (nbr->state == NSM_Loading)
    {
      if (ospf_ls_request_isempty (nbr))
	OSPF_NSM_EVENT_SCHEDULE (nbr, NSM_LoadingDone);
      else if (nbr->ls_req_last == NULL)
	ospf_ls_req_event (nbr);
    }
}
