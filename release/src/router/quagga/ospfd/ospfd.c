/* OSPF version 2 daemon program.
   Copyright (C) 1999, 2000 Toshiaki Takada

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

#include "thread.h"
#include "vty.h"
#include "command.h"
#include "linklist.h"
#include "prefix.h"
#include "table.h"
#include "if.h"
#include "memory.h"
#include "stream.h"
#include "log.h"
#include "sockunion.h"          /* for inet_aton () */
#include "zclient.h"
#include "plist.h"
#include "sockopt.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_network.h"
#include "ospfd/ospf_interface.h"
#include "ospfd/ospf_ism.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_lsdb.h"
#include "ospfd/ospf_neighbor.h"
#include "ospfd/ospf_nsm.h"
#include "ospfd/ospf_spf.h"
#include "ospfd/ospf_packet.h"
#include "ospfd/ospf_dump.h"
#include "ospfd/ospf_zebra.h"
#include "ospfd/ospf_abr.h"
#include "ospfd/ospf_flood.h"
#include "ospfd/ospf_route.h"
#include "ospfd/ospf_ase.h"



/* OSPF process wide configuration. */
static struct ospf_master ospf_master;

/* OSPF process wide configuration pointer to export. */
struct ospf_master *om;

extern struct zclient *zclient;
extern struct in_addr router_id_zebra;


static void ospf_remove_vls_through_area (struct ospf *, struct ospf_area *);
static void ospf_network_free (struct ospf *, struct ospf_network *);
static void ospf_area_free (struct ospf_area *);
static void ospf_network_run (struct prefix *, struct ospf_area *);
static void ospf_network_run_interface (struct prefix *, struct ospf_area *,
                                        struct interface *);
static int ospf_network_match_iface (const struct connected *,
				     const struct prefix *);
static void ospf_finish_final (struct ospf *);

#define OSPF_EXTERNAL_LSA_ORIGINATE_DELAY 1

void
ospf_router_id_update (struct ospf *ospf)
{
  struct in_addr router_id, router_id_old;
  struct ospf_interface *oi;
  struct interface *ifp;
  struct listnode *node;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("Router-ID[OLD:%s]: Update", inet_ntoa (ospf->router_id));

  router_id_old = ospf->router_id;

  /* Select the router ID based on these priorities:
       1. Statically assigned router ID is always the first choice.
       2. If there is no statically assigned router ID, then try to stick
          with the most recent value, since changing router ID's is very
	  disruptive.
       3. Last choice: just go with whatever the zebra daemon recommends.
  */
  if (ospf->router_id_static.s_addr != 0)
    router_id = ospf->router_id_static;
  else if (ospf->router_id.s_addr != 0)
    router_id = ospf->router_id;
  else
    router_id = router_id_zebra;

  ospf->router_id = router_id;
  
  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("Router-ID[NEW:%s]: Update", inet_ntoa (ospf->router_id));

  if (!IPV4_ADDR_SAME (&router_id_old, &router_id))
    {
      for (ALL_LIST_ELEMENTS_RO (ospf->oiflist, node, oi))
        /* Update self-neighbor's router_id. */
        oi->nbr_self->router_id = router_id;

      /* If AS-external-LSA is queued, then flush those LSAs. */
      if (router_id_old.s_addr == 0 && ospf->external_origin)
	{
	  int type;
	  /* Originate each redistributed external route. */
	  for (type = 0; type < ZEBRA_ROUTE_MAX; type++)
	    if (ospf->external_origin & (1 << type))
	      thread_add_event (master, ospf_external_lsa_originate_timer,
				ospf, type);
	  /* Originate Deafult. */
	  if (ospf->external_origin & (1 << ZEBRA_ROUTE_MAX))
	    thread_add_event (master, ospf_default_originate_timer, ospf, 0);

	  ospf->external_origin = 0;
	}

      /* update router-lsa's for each area */
      ospf_router_lsa_update (ospf);
      
      /* update ospf_interface's */
      for (ALL_LIST_ELEMENTS_RO (om->iflist, node, ifp))
        ospf_if_update (ospf, ifp);
    }
}

/* For OSPF area sort by area id. */
static int
ospf_area_id_cmp (struct ospf_area *a1, struct ospf_area *a2)
{
  if (ntohl (a1->area_id.s_addr) > ntohl (a2->area_id.s_addr))
    return 1;
  if (ntohl (a1->area_id.s_addr) < ntohl (a2->area_id.s_addr))
    return -1;
  return 0;
}

/* Allocate new ospf structure. */
static struct ospf *
ospf_new (void)
{
  int i;

  struct ospf *new = XCALLOC (MTYPE_OSPF_TOP, sizeof (struct ospf));

  new->router_id.s_addr = htonl (0);
  new->router_id_static.s_addr = htonl (0);

  new->abr_type = OSPF_ABR_DEFAULT;
  new->oiflist = list_new ();
  new->vlinks = list_new ();
  new->areas = list_new ();
  new->areas->cmp = (int (*)(void *, void *)) ospf_area_id_cmp;
  new->networks = route_table_init ();
  new->nbr_nbma = route_table_init ();

  new->lsdb = ospf_lsdb_new ();

  new->default_originate = DEFAULT_ORIGINATE_NONE;

  new->passive_interface_default = OSPF_IF_ACTIVE;
  
  new->new_external_route = route_table_init ();
  new->old_external_route = route_table_init ();
  new->external_lsas = route_table_init ();
  
  new->stub_router_startup_time = OSPF_STUB_ROUTER_UNCONFIGURED;
  new->stub_router_shutdown_time = OSPF_STUB_ROUTER_UNCONFIGURED;
  new->stub_router_admin_set     = OSPF_STUB_ROUTER_ADMINISTRATIVE_UNSET;

  /* Distribute parameter init. */
  for (i = 0; i <= ZEBRA_ROUTE_MAX; i++)
    {
      new->dmetric[i].type = -1;
      new->dmetric[i].value = -1;
    }
  new->default_metric = -1;
  new->ref_bandwidth = OSPF_DEFAULT_REF_BANDWIDTH;

  /* SPF timer value init. */
  new->spf_delay = OSPF_SPF_DELAY_DEFAULT;
  new->spf_holdtime = OSPF_SPF_HOLDTIME_DEFAULT;
  new->spf_max_holdtime = OSPF_SPF_MAX_HOLDTIME_DEFAULT;
  new->spf_hold_multiplier = 1;

  /* MaxAge init. */
  new->maxage_delay = OSPF_LSA_MAXAGE_REMOVE_DELAY_DEFAULT;
  new->maxage_lsa = route_table_init();
  new->t_maxage_walker =
    thread_add_timer (master, ospf_lsa_maxage_walker,
                      new, OSPF_LSA_MAXAGE_CHECK_INTERVAL);

  /* Distance table init. */
  new->distance_table = route_table_init ();

  new->lsa_refresh_queue.index = 0;
  new->lsa_refresh_interval = OSPF_LSA_REFRESH_INTERVAL_DEFAULT;
  new->t_lsa_refresher = thread_add_timer (master, ospf_lsa_refresh_walker,
					   new, new->lsa_refresh_interval);
  new->lsa_refresher_started = quagga_time (NULL);

  if ((new->fd = ospf_sock_init()) < 0)
    {
      zlog_err("ospf_new: fatal error: ospf_sock_init was unable to open "
	       "a socket");
      exit(1);
    }
  new->maxsndbuflen = getsockopt_so_sendbuf (new->fd);
  if (IS_DEBUG_OSPF (zebra, ZEBRA_INTERFACE))
    zlog_debug ("%s: starting with OSPF send buffer size %u",
      __func__, new->maxsndbuflen);
  if ((new->ibuf = stream_new(OSPF_MAX_PACKET_SIZE+1)) == NULL)
    {
      zlog_err("ospf_new: fatal error: stream_new(%u) failed allocating ibuf",
	       OSPF_MAX_PACKET_SIZE+1);
      exit(1);
    }
  new->t_read = thread_add_read (master, ospf_read, new, new->fd);
  new->oi_write_q = list_new ();
  
  return new;
}

struct ospf *
ospf_lookup ()
{
  if (listcount (om->ospf) == 0)
    return NULL;

  return listgetdata (listhead (om->ospf));
}

static void
ospf_add (struct ospf *ospf)
{
  listnode_add (om->ospf, ospf);
}

static void
ospf_delete (struct ospf *ospf)
{
  listnode_delete (om->ospf, ospf);
}

struct ospf *
ospf_get ()
{
  struct ospf *ospf;

  ospf = ospf_lookup ();
  if (ospf == NULL)
    {
      ospf = ospf_new ();
      ospf_add (ospf);

      if (ospf->router_id_static.s_addr == 0)
	ospf_router_id_update (ospf);

#ifdef HAVE_OPAQUE_LSA
      ospf_opaque_type11_lsa_init (ospf);
#endif /* HAVE_OPAQUE_LSA */
    }

  return ospf;
}

/* Handle the second half of deferred shutdown. This is called either
 * from the deferred-shutdown timer thread, or directly through
 * ospf_deferred_shutdown_check.
 *
 * Function is to cleanup G-R state, if required then call ospf_finish_final
 * to complete shutdown of this ospf instance. Possibly exit if the
 * whole process is being shutdown and this was the last OSPF instance.
 */
static void
ospf_deferred_shutdown_finish (struct ospf *ospf)
{
  ospf->stub_router_shutdown_time = OSPF_STUB_ROUTER_UNCONFIGURED;  
  OSPF_TIMER_OFF (ospf->t_deferred_shutdown);
  
  ospf_finish_final (ospf);
  
  /* *ospf is now invalid */
  
  /* ospfd being shut-down? If so, was this the last ospf instance? */
  if (CHECK_FLAG (om->options, OSPF_MASTER_SHUTDOWN)
      && (listcount (om->ospf) == 0))
    exit (0);

  return;
}

/* Timer thread for G-R */
static int
ospf_deferred_shutdown_timer (struct thread *t)
{
  struct ospf *ospf = THREAD_ARG(t);
  
  ospf_deferred_shutdown_finish (ospf);
  
  return 0;
}

/* Check whether deferred-shutdown must be scheduled, otherwise call
 * down directly into second-half of instance shutdown.
 */
static void
ospf_deferred_shutdown_check (struct ospf *ospf)
{
  unsigned long timeout;
  struct listnode *ln;
  struct ospf_area *area;
  
  /* deferred shutdown already running? */
  if (ospf->t_deferred_shutdown)
    return;
  
  /* Should we try push out max-metric LSAs? */
  if (ospf->stub_router_shutdown_time != OSPF_STUB_ROUTER_UNCONFIGURED)
    {
      for (ALL_LIST_ELEMENTS_RO (ospf->areas, ln, area))
        {
          SET_FLAG (area->stub_router_state, OSPF_AREA_ADMIN_STUB_ROUTED);
          
          if (!CHECK_FLAG (area->stub_router_state, OSPF_AREA_IS_STUB_ROUTED))
            ospf_router_lsa_update_area (area);
        }
      timeout = ospf->stub_router_shutdown_time;
    }
  else
    {
      /* No timer needed */
      ospf_deferred_shutdown_finish (ospf);
      return;
    }
  
  OSPF_TIMER_ON (ospf->t_deferred_shutdown, ospf_deferred_shutdown_timer,
                 timeout);
  return;
}

/* Shut down the entire process */
void
ospf_terminate (void)
{
  struct ospf *ospf;
  struct listnode *node, *nnode;
  
  /* shutdown already in progress */
  if (CHECK_FLAG (om->options, OSPF_MASTER_SHUTDOWN))
    return;
  
  SET_FLAG (om->options, OSPF_MASTER_SHUTDOWN);

  /* exit immediately if OSPF not actually running */
  if (listcount(om->ospf) == 0)
    exit(0);

  for (ALL_LIST_ELEMENTS (om->ospf, node, nnode, ospf))
    ospf_finish (ospf);

  /* Deliberately go back up, hopefully to thread scheduler, as
   * One or more ospf_finish()'s may have deferred shutdown to a timer
   * thread
   */
}

void
ospf_finish (struct ospf *ospf)
{
  /* let deferred shutdown decide */
  ospf_deferred_shutdown_check (ospf);
      
  /* if ospf_deferred_shutdown returns, then ospf_finish_final is
   * deferred to expiry of G-S timer thread. Return back up, hopefully
   * to thread scheduler.
   */
  return;
}

/* Final cleanup of ospf instance */
static void
ospf_finish_final (struct ospf *ospf)
{
  struct route_node *rn;
  struct ospf_nbr_nbma *nbr_nbma;
  struct ospf_lsa *lsa;
  struct ospf_interface *oi;
  struct ospf_area *area;
  struct ospf_vl_data *vl_data;
  struct listnode *node, *nnode;
  int i;

#ifdef HAVE_OPAQUE_LSA
  ospf_opaque_type11_lsa_term (ospf);
#endif /* HAVE_OPAQUE_LSA */
  
  /* be nice if this worked, but it doesn't */
  /*ospf_flush_self_originated_lsas_now (ospf);*/
  
  /* Unregister redistribution */
  for (i = 0; i < ZEBRA_ROUTE_MAX; i++)
    ospf_redistribute_unset (ospf, i);
  ospf_redistribute_default_unset (ospf);

  for (ALL_LIST_ELEMENTS (ospf->areas, node, nnode, area))
    ospf_remove_vls_through_area (ospf, area);
  
  for (ALL_LIST_ELEMENTS (ospf->vlinks, node, nnode, vl_data))
    ospf_vl_delete (ospf, vl_data);
  
  list_delete (ospf->vlinks);

  /* Reset interface. */
  for (ALL_LIST_ELEMENTS (ospf->oiflist, node, nnode, oi))
    ospf_if_free (oi);

  /* Clear static neighbors */
  for (rn = route_top (ospf->nbr_nbma); rn; rn = route_next (rn))
    if ((nbr_nbma = rn->info))
      {
	OSPF_POLL_TIMER_OFF (nbr_nbma->t_poll);

	if (nbr_nbma->nbr)
	  {
	    nbr_nbma->nbr->nbr_nbma = NULL;
	    nbr_nbma->nbr = NULL;
	  }

	if (nbr_nbma->oi)
	  {
	    listnode_delete (nbr_nbma->oi->nbr_nbma, nbr_nbma);
	    nbr_nbma->oi = NULL;
	  }

	XFREE (MTYPE_OSPF_NEIGHBOR_STATIC, nbr_nbma);
      }

  route_table_finish (ospf->nbr_nbma);

  /* Clear networks and Areas. */
  for (rn = route_top (ospf->networks); rn; rn = route_next (rn))
    {
      struct ospf_network *network;

      if ((network = rn->info) != NULL)
	{
	  ospf_network_free (ospf, network);
	  rn->info = NULL;
	  route_unlock_node (rn);
	}
    }

  for (ALL_LIST_ELEMENTS (ospf->areas, node, nnode, area))
    {
      listnode_delete (ospf->areas, area);
      ospf_area_free (area);
    }

  /* Cancel all timers. */
  OSPF_TIMER_OFF (ospf->t_external_lsa);
  OSPF_TIMER_OFF (ospf->t_spf_calc);
  OSPF_TIMER_OFF (ospf->t_ase_calc);
  OSPF_TIMER_OFF (ospf->t_maxage);
  OSPF_TIMER_OFF (ospf->t_maxage_walker);
  OSPF_TIMER_OFF (ospf->t_abr_task);
  OSPF_TIMER_OFF (ospf->t_asbr_check);
  OSPF_TIMER_OFF (ospf->t_distribute_update);
  OSPF_TIMER_OFF (ospf->t_lsa_refresher);
  OSPF_TIMER_OFF (ospf->t_read);
  OSPF_TIMER_OFF (ospf->t_write);
#ifdef HAVE_OPAQUE_LSA
  OSPF_TIMER_OFF (ospf->t_opaque_lsa_self);
#endif

  close (ospf->fd);
  stream_free(ospf->ibuf);
   
#ifdef HAVE_OPAQUE_LSA
  LSDB_LOOP (OPAQUE_AS_LSDB (ospf), rn, lsa)
    ospf_discard_from_db (ospf, ospf->lsdb, lsa);
#endif /* HAVE_OPAQUE_LSA */
  LSDB_LOOP (EXTERNAL_LSDB (ospf), rn, lsa)
    ospf_discard_from_db (ospf, ospf->lsdb, lsa);

  ospf_lsdb_delete_all (ospf->lsdb);
  ospf_lsdb_free (ospf->lsdb);

  for (rn = route_top (ospf->maxage_lsa); rn; rn = route_next (rn))
    {
      struct ospf_lsa *lsa;

      if ((lsa = rn->info) != NULL)
	{
	  ospf_lsa_unlock (&lsa);
	  rn->info = NULL;
	}
      route_unlock_node (rn);
    }
  route_table_finish (ospf->maxage_lsa);

  if (ospf->old_table)
    ospf_route_table_free (ospf->old_table);
  if (ospf->new_table)
    {
      ospf_route_delete (ospf->new_table);
      ospf_route_table_free (ospf->new_table);
    }
  if (ospf->old_rtrs)
    ospf_rtrs_free (ospf->old_rtrs);
  if (ospf->new_rtrs)
    ospf_rtrs_free (ospf->new_rtrs);
  if (ospf->new_external_route)
    {
      ospf_route_delete (ospf->new_external_route);
      ospf_route_table_free (ospf->new_external_route);
    }
  if (ospf->old_external_route)
    {
      ospf_route_delete (ospf->old_external_route);
      ospf_route_table_free (ospf->old_external_route);
    }
  if (ospf->external_lsas)
    {
      ospf_ase_external_lsas_finish (ospf->external_lsas);
    }

  list_delete (ospf->areas);
  
  for (i = ZEBRA_ROUTE_SYSTEM; i <= ZEBRA_ROUTE_MAX; i++)
    if (EXTERNAL_INFO (i) != NULL)
      for (rn = route_top (EXTERNAL_INFO (i)); rn; rn = route_next (rn))
	{
	  if (rn->info == NULL)
	    continue;
	  
	  XFREE (MTYPE_OSPF_EXTERNAL_INFO, rn->info);
	  rn->info = NULL;
	  route_unlock_node (rn);
	}

  ospf_distance_reset (ospf);
  route_table_finish (ospf->distance_table);

  ospf_delete (ospf);

  XFREE (MTYPE_OSPF_TOP, ospf);
}


/* allocate new OSPF Area object */
static struct ospf_area *
ospf_area_new (struct ospf *ospf, struct in_addr area_id)
{
  struct ospf_area *new;

  /* Allocate new config_network. */
  new = XCALLOC (MTYPE_OSPF_AREA, sizeof (struct ospf_area));

  new->ospf = ospf;

  new->area_id = area_id;

  new->external_routing = OSPF_AREA_DEFAULT;
  new->default_cost = 1;
  new->auth_type = OSPF_AUTH_NULL;
  
  /* New LSDB init. */
  new->lsdb = ospf_lsdb_new ();

  /* Self-originated LSAs initialize. */
  new->router_lsa_self = NULL;

#ifdef HAVE_OPAQUE_LSA
  ospf_opaque_type10_lsa_init (new);
#endif /* HAVE_OPAQUE_LSA */

  new->oiflist = list_new ();
  new->ranges = route_table_init ();

  if (area_id.s_addr == OSPF_AREA_BACKBONE)
    ospf->backbone = new;

  return new;
}

static void
ospf_area_free (struct ospf_area *area)
{
  struct route_node *rn;
  struct ospf_lsa *lsa;

  /* Free LSDBs. */
  LSDB_LOOP (ROUTER_LSDB (area), rn, lsa)
    ospf_discard_from_db (area->ospf, area->lsdb, lsa);
  LSDB_LOOP (NETWORK_LSDB (area), rn, lsa)
    ospf_discard_from_db (area->ospf, area->lsdb, lsa);
  LSDB_LOOP (SUMMARY_LSDB (area), rn, lsa)
    ospf_discard_from_db (area->ospf, area->lsdb, lsa);
  LSDB_LOOP (ASBR_SUMMARY_LSDB (area), rn, lsa)
    ospf_discard_from_db (area->ospf, area->lsdb, lsa);

  LSDB_LOOP (NSSA_LSDB (area), rn, lsa)
    ospf_discard_from_db (area->ospf, area->lsdb, lsa);
#ifdef HAVE_OPAQUE_LSA
  LSDB_LOOP (OPAQUE_AREA_LSDB (area), rn, lsa)
    ospf_discard_from_db (area->ospf, area->lsdb, lsa);
  LSDB_LOOP (OPAQUE_LINK_LSDB (area), rn, lsa)
    ospf_discard_from_db (area->ospf, area->lsdb, lsa);
#endif /* HAVE_OPAQUE_LSA */

  ospf_lsdb_delete_all (area->lsdb);
  ospf_lsdb_free (area->lsdb);

  ospf_lsa_unlock (&area->router_lsa_self);
  
  route_table_finish (area->ranges);
  list_delete (area->oiflist);

  if (EXPORT_NAME (area))
    free (EXPORT_NAME (area));

  if (IMPORT_NAME (area))
    free (IMPORT_NAME (area));

  /* Cancel timer. */
  OSPF_TIMER_OFF (area->t_stub_router);
#ifdef HAVE_OPAQUE_LSA
  OSPF_TIMER_OFF (area->t_opaque_lsa_self);
#endif /* HAVE_OPAQUE_LSA */
  
  if (OSPF_IS_AREA_BACKBONE (area))
    area->ospf->backbone = NULL;

  XFREE (MTYPE_OSPF_AREA, area);
}

void
ospf_area_check_free (struct ospf *ospf, struct in_addr area_id)
{
  struct ospf_area *area;

  area = ospf_area_lookup_by_area_id (ospf, area_id);
  if (area &&
      listcount (area->oiflist) == 0 &&
      area->ranges->top == NULL &&
      area->shortcut_configured == OSPF_SHORTCUT_DEFAULT &&
      area->external_routing == OSPF_AREA_DEFAULT &&
      area->no_summary == 0 &&
      area->default_cost == 1 &&
      EXPORT_NAME (area) == NULL &&
      IMPORT_NAME (area) == NULL &&
      area->auth_type == OSPF_AUTH_NULL)
    {
      listnode_delete (ospf->areas, area);
      ospf_area_free (area);
    }
}

struct ospf_area *
ospf_area_get (struct ospf *ospf, struct in_addr area_id, int format)
{
  struct ospf_area *area;
  
  area = ospf_area_lookup_by_area_id (ospf, area_id);
  if (!area)
    {
      area = ospf_area_new (ospf, area_id);
      area->format = format;
      listnode_add_sort (ospf->areas, area);
      ospf_check_abr_status (ospf);  
      if (ospf->stub_router_admin_set == OSPF_STUB_ROUTER_ADMINISTRATIVE_SET)
        {
          SET_FLAG (area->stub_router_state, OSPF_AREA_ADMIN_STUB_ROUTED);
        }
    }

  return area;
}

struct ospf_area *
ospf_area_lookup_by_area_id (struct ospf *ospf, struct in_addr area_id)
{
  struct ospf_area *area;
  struct listnode *node;

  for (ALL_LIST_ELEMENTS_RO (ospf->areas, node, area))
    if (IPV4_ADDR_SAME (&area->area_id, &area_id))
      return area;

  return NULL;
}

void
ospf_area_add_if (struct ospf_area *area, struct ospf_interface *oi)
{
  listnode_add (area->oiflist, oi);
}

void
ospf_area_del_if (struct ospf_area *area, struct ospf_interface *oi)
{
  listnode_delete (area->oiflist, oi);
}


/* Config network statement related functions. */
static struct ospf_network *
ospf_network_new (struct in_addr area_id, int format)
{
  struct ospf_network *new;
  new = XCALLOC (MTYPE_OSPF_NETWORK, sizeof (struct ospf_network));

  new->area_id = area_id;
  new->format = format;
  
  return new;
}

static void
ospf_network_free (struct ospf *ospf, struct ospf_network *network)
{
  ospf_area_check_free (ospf, network->area_id);
  ospf_schedule_abr_task (ospf);
  XFREE (MTYPE_OSPF_NETWORK, network);
}

int
ospf_network_set (struct ospf *ospf, struct prefix_ipv4 *p,
		  struct in_addr area_id)
{
  struct ospf_network *network;
  struct ospf_area *area;
  struct route_node *rn;
  struct external_info *ei;
  int ret = OSPF_AREA_ID_FORMAT_ADDRESS;

  rn = route_node_get (ospf->networks, (struct prefix *)p);
  if (rn->info)
    {
      /* There is already same network statement. */
      route_unlock_node (rn);
      return 0;
    }

  rn->info = network = ospf_network_new (area_id, ret);
  area = ospf_area_get (ospf, area_id, ret);

  /* Run network config now. */
  ospf_network_run ((struct prefix *)p, area);

  /* Update connected redistribute. */
  if (ospf_is_type_redistributed (ZEBRA_ROUTE_CONNECT))
    if (EXTERNAL_INFO (ZEBRA_ROUTE_CONNECT))
      for (rn = route_top (EXTERNAL_INFO (ZEBRA_ROUTE_CONNECT));
	   rn; rn = route_next (rn))
	if ((ei = rn->info) != NULL)
	  if (ospf_external_info_find_lsa (ospf, &ei->p))
	    if (!ospf_distribute_check_connected (ospf, ei))
	      ospf_external_lsa_flush (ospf, ei->type, &ei->p,
				       ei->ifindex /*, ei->nexthop */);

  ospf_area_check_free (ospf, area_id);

  return 1;
}

int
ospf_network_unset (struct ospf *ospf, struct prefix_ipv4 *p,
		    struct in_addr area_id)
{
  struct route_node *rn;
  struct ospf_network *network;
  struct external_info *ei;
  struct listnode *node, *nnode;
  struct ospf_interface *oi;

  rn = route_node_lookup (ospf->networks, (struct prefix *)p);
  if (rn == NULL)
    return 0;

  network = rn->info;
  route_unlock_node (rn);
  if (!IPV4_ADDR_SAME (&area_id, &network->area_id))
    return 0;

  ospf_network_free (ospf, rn->info);
  rn->info = NULL;
  route_unlock_node (rn);	/* initial reference */

  /* Find interfaces that not configured already.  */
  for (ALL_LIST_ELEMENTS (ospf->oiflist, node, nnode, oi))
    {
      int found = 0;
      struct connected *co = oi->connected;
      
      if (oi->type == OSPF_IFTYPE_VIRTUALLINK)
        continue;
      
      for (rn = route_top (ospf->networks); rn; rn = route_next (rn))
        {
          if (rn->info == NULL)
            continue;
          
          if (ospf_network_match_iface(co,&rn->p))
            {
              found = 1;
              route_unlock_node (rn);
              break;
            }
        }

      if (found == 0)
	{
	  ospf_if_free (oi);
	  ospf_area_check_free (ospf, area_id);
	}
    }
  
  /* Update connected redistribute. */
  if (ospf_is_type_redistributed (ZEBRA_ROUTE_CONNECT))
    if (EXTERNAL_INFO (ZEBRA_ROUTE_CONNECT))
      for (rn = route_top (EXTERNAL_INFO (ZEBRA_ROUTE_CONNECT));
	   rn; rn = route_next (rn))
	if ((ei = rn->info) != NULL)
	  if (!ospf_external_info_find_lsa (ospf, &ei->p))
	    if (ospf_distribute_check_connected (ospf, ei))
	      ospf_external_lsa_originate (ospf, ei);

  return 1;
}

/* Check whether interface matches given network
 * returns: 1, true. 0, false
 */
static int
ospf_network_match_iface(const struct connected *co, const struct prefix *net)
{
  /* new approach: more elegant and conceptually clean */
  return prefix_match(net, CONNECTED_PREFIX(co));
}

static void
ospf_network_run_interface (struct prefix *p, struct ospf_area *area,
                            struct interface *ifp)
{
  struct listnode *cnode;
  struct connected *co;
  
  if (memcmp (ifp->name, "VLINK", 5) == 0)
    return;
  
  /* if interface prefix is match specified prefix,
     then create socket and join multicast group. */
  for (ALL_LIST_ELEMENTS_RO (ifp->connected, cnode, co))
    {

      if (CHECK_FLAG(co->flags,ZEBRA_IFA_SECONDARY))
        continue;

      if (p->family == co->address->family 
	  && ! ospf_if_table_lookup(ifp, co->address)
          && ospf_network_match_iface(co,p))
        {
           struct ospf_interface *oi;
            
            oi = ospf_if_new (area->ospf, ifp, co->address);
            oi->connected = co;
            
            oi->area = area;

            oi->params = ospf_lookup_if_params (ifp, oi->address->u.prefix4);
            oi->output_cost = ospf_if_get_output_cost (oi);
            
            /* Add pseudo neighbor. */
            ospf_nbr_add_self (oi);

            /* Relate ospf interface to ospf instance. */
            oi->ospf = area->ospf;

            /* update network type as interface flag */
            /* If network type is specified previously,
               skip network type setting. */
            oi->type = IF_DEF_PARAMS (ifp)->type;
            
            ospf_area_add_if (oi->area, oi);
            
            /* if router_id is not configured, dont bring up
             * interfaces.
             * ospf_router_id_update() will call ospf_if_update
             * whenever r-id is configured instead.
             */
            if ((area->ospf->router_id.s_addr != 0)
                && if_is_operative (ifp)) 
              ospf_if_up (oi);
          }
    }
}

static void
ospf_network_run (struct prefix *p, struct ospf_area *area)
{
  struct interface *ifp;
  struct listnode *node;

  /* Schedule Router ID Update. */
  if (area->ospf->router_id.s_addr == 0)
    ospf_router_id_update (area->ospf);
  
  /* Get target interface. */
  for (ALL_LIST_ELEMENTS_RO (om->iflist, node, ifp))
    ospf_network_run_interface (p, area, ifp);
}

void
ospf_ls_upd_queue_empty (struct ospf_interface *oi)
{
  struct route_node *rn;
  struct listnode *node, *nnode;
  struct list *lst;
  struct ospf_lsa *lsa;

  /* empty ls update queue */
  for (rn = route_top (oi->ls_upd_queue); rn;
       rn = route_next (rn))
    if ((lst = (struct list *) rn->info))
      {
	for (ALL_LIST_ELEMENTS (lst, node, nnode, lsa))
          ospf_lsa_unlock (&lsa); /* oi->ls_upd_queue */
	list_free (lst);
	rn->info = NULL;
      }
  
  /* remove update event */
  if (oi->t_ls_upd_event)
    {
      thread_cancel (oi->t_ls_upd_event);
      oi->t_ls_upd_event = NULL;
    }
}

void
ospf_if_update (struct ospf *ospf, struct interface *ifp)
{
  struct route_node *rn;
  struct ospf_network *network;
  struct ospf_area *area;
  
  if (!ospf)
    ospf = ospf_lookup ();

  /* OSPF must be on and Router-ID must be configured. */
  if (!ospf || ospf->router_id.s_addr == 0)
    return;
  
  /* Run each netowrk for this interface. */
  for (rn = route_top (ospf->networks); rn; rn = route_next (rn))
    if (rn->info != NULL)
      {
        network = (struct ospf_network *) rn->info;
        area = ospf_area_get (ospf, network->area_id, network->format);
        ospf_network_run_interface (&rn->p, area, ifp);
      }
}

void
ospf_remove_vls_through_area (struct ospf *ospf, struct ospf_area *area)
{
  struct listnode *node, *nnode;
  struct ospf_vl_data *vl_data;

  for (ALL_LIST_ELEMENTS (ospf->vlinks, node, nnode, vl_data))
    if (IPV4_ADDR_SAME (&vl_data->vl_area_id, &area->area_id))
      ospf_vl_delete (ospf, vl_data);
}


static const struct message ospf_area_type_msg[] =
{
  { OSPF_AREA_DEFAULT,	"Default" },
  { OSPF_AREA_STUB,     "Stub" },
  { OSPF_AREA_NSSA,     "NSSA" },
};
static const int ospf_area_type_msg_max = OSPF_AREA_TYPE_MAX;

static void
ospf_area_type_set (struct ospf_area *area, int type)
{
  struct listnode *node;
  struct ospf_interface *oi;

  if (area->external_routing == type)
    {
      if (IS_DEBUG_OSPF_EVENT)
	zlog_debug ("Area[%s]: Types are the same, ignored.",
		   inet_ntoa (area->area_id));
      return;
    }

  area->external_routing = type;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_debug ("Area[%s]: Configured as %s", inet_ntoa (area->area_id),
	       LOOKUP (ospf_area_type_msg, type));

  switch (area->external_routing)
    {
    case OSPF_AREA_DEFAULT:
      for (ALL_LIST_ELEMENTS_RO (area->oiflist, node, oi))
        if (oi->nbr_self != NULL)
          {
	    UNSET_FLAG (oi->nbr_self->options, OSPF_OPTION_NP);
	    SET_FLAG (oi->nbr_self->options, OSPF_OPTION_E);
          }
      break;
    case OSPF_AREA_STUB:
      for (ALL_LIST_ELEMENTS_RO (area->oiflist, node, oi))
        if (oi->nbr_self != NULL)
          {
            if (IS_DEBUG_OSPF_EVENT)
              zlog_debug ("setting options on %s accordingly", IF_NAME (oi));
            UNSET_FLAG (oi->nbr_self->options, OSPF_OPTION_NP);
            UNSET_FLAG (oi->nbr_self->options, OSPF_OPTION_E);
            if (IS_DEBUG_OSPF_EVENT)
              zlog_debug ("options set on %s: %x",
                         IF_NAME (oi), OPTIONS (oi));
          }
      break;
    case OSPF_AREA_NSSA:
      for (ALL_LIST_ELEMENTS_RO (area->oiflist, node, oi))
        if (oi->nbr_self != NULL)
          {
            zlog_debug ("setting nssa options on %s accordingly", IF_NAME (oi));
            UNSET_FLAG (oi->nbr_self->options, OSPF_OPTION_E);
            SET_FLAG (oi->nbr_self->options, OSPF_OPTION_NP);
            zlog_debug ("options set on %s: %x", IF_NAME (oi), OPTIONS (oi));
          }
      break;
    default:
      break;
    }

  ospf_router_lsa_update_area (area);
  ospf_schedule_abr_task (area->ospf);
}

int
ospf_area_shortcut_set (struct ospf *ospf, struct ospf_area *area, int mode)
{
  if (area->shortcut_configured == mode)
    return 0;

  area->shortcut_configured = mode;
  ospf_router_lsa_update_area (area);
  ospf_schedule_abr_task (ospf);

  ospf_area_check_free (ospf, area->area_id);

  return 1;
}

int
ospf_area_shortcut_unset (struct ospf *ospf, struct ospf_area *area)
{
  area->shortcut_configured = OSPF_SHORTCUT_DEFAULT;
  ospf_router_lsa_update_area (area);
  ospf_area_check_free (ospf, area->area_id);
  ospf_schedule_abr_task (ospf);

  return 1;
}

static int
ospf_area_vlink_count (struct ospf *ospf, struct ospf_area *area)
{
  struct ospf_vl_data *vl;
  struct listnode *node;
  int count = 0;

  for (ALL_LIST_ELEMENTS_RO (ospf->vlinks, node, vl))
    if (IPV4_ADDR_SAME (&vl->vl_area_id, &area->area_id))
      count++;

  return count;
}

int
ospf_area_stub_set (struct ospf *ospf, struct in_addr area_id)
{
  struct ospf_area *area;
  int format = OSPF_AREA_ID_FORMAT_ADDRESS;

  area = ospf_area_get (ospf, area_id, format);
  if (ospf_area_vlink_count (ospf, area))
    return 0;

  if (area->external_routing != OSPF_AREA_STUB)
    ospf_area_type_set (area, OSPF_AREA_STUB);

  return 1;
}

int
ospf_area_stub_unset (struct ospf *ospf, struct in_addr area_id)
{
  struct ospf_area *area;

  area = ospf_area_lookup_by_area_id (ospf, area_id);
  if (area == NULL)
    return 1;

  if (area->external_routing == OSPF_AREA_STUB)
    ospf_area_type_set (area, OSPF_AREA_DEFAULT);

  ospf_area_check_free (ospf, area_id);

  return 1;
}

int
ospf_area_no_summary_set (struct ospf *ospf, struct in_addr area_id)
{
  struct ospf_area *area;
  int format = OSPF_AREA_ID_FORMAT_ADDRESS;

  area = ospf_area_get (ospf, area_id, format);
  area->no_summary = 1;

  return 1;
}

int
ospf_area_no_summary_unset (struct ospf *ospf, struct in_addr area_id)
{
  struct ospf_area *area;

  area = ospf_area_lookup_by_area_id (ospf, area_id);
  if (area == NULL)
    return 0;

  area->no_summary = 0;
  ospf_area_check_free (ospf, area_id);

  return 1;
}

int
ospf_area_nssa_set (struct ospf *ospf, struct in_addr area_id)
{
  struct ospf_area *area;
  int format = OSPF_AREA_ID_FORMAT_ADDRESS;

  area = ospf_area_get (ospf, area_id, format);
  if (ospf_area_vlink_count (ospf, area))
    return 0;

  if (area->external_routing != OSPF_AREA_NSSA)
    {
      ospf_area_type_set (area, OSPF_AREA_NSSA);
      ospf->anyNSSA++;
    }

  /* set NSSA area defaults */
  area->no_summary = 0;
  area->NSSATranslatorRole = OSPF_NSSA_ROLE_CANDIDATE;
  area->NSSATranslatorState = OSPF_NSSA_TRANSLATE_DISABLED;
  area->NSSATranslatorStabilityInterval = OSPF_NSSA_TRANS_STABLE_DEFAULT;

  return 1;
}

int
ospf_area_nssa_unset (struct ospf *ospf, struct in_addr area_id)
{
  struct ospf_area *area;

  area = ospf_area_lookup_by_area_id (ospf, area_id);
  if (area == NULL)
    return 0;

  if (area->external_routing == OSPF_AREA_NSSA)
    {
      ospf->anyNSSA--;
      ospf_area_type_set (area, OSPF_AREA_DEFAULT);
    }

  ospf_area_check_free (ospf, area_id);

  return 1;
}

int
ospf_area_nssa_translator_role_set (struct ospf *ospf, struct in_addr area_id,
				    int role)
{
  struct ospf_area *area;

  area = ospf_area_lookup_by_area_id (ospf, area_id);
  if (area == NULL)
    return 0;

  area->NSSATranslatorRole = role;

  return 1;
}

#if 0
/* XXX: unused? Leave for symmetry? */
static int
ospf_area_nssa_translator_role_unset (struct ospf *ospf,
				      struct in_addr area_id)
{
  struct ospf_area *area;

  area = ospf_area_lookup_by_area_id (ospf, area_id);
  if (area == NULL)
    return 0;

  area->NSSATranslatorRole = OSPF_NSSA_ROLE_CANDIDATE;

  ospf_area_check_free (ospf, area_id);

  return 1;
}
#endif

int
ospf_area_export_list_set (struct ospf *ospf,
			   struct ospf_area *area, const char *list_name)
{
  struct access_list *list;
  list = access_list_lookup (AFI_IP, list_name);

  EXPORT_LIST (area) = list;

  if (EXPORT_NAME (area))
    free (EXPORT_NAME (area));

  EXPORT_NAME (area) = strdup (list_name);
  ospf_schedule_abr_task (ospf);

  return 1;
}

int
ospf_area_export_list_unset (struct ospf *ospf, struct ospf_area * area)
{

  EXPORT_LIST (area) = 0;

  if (EXPORT_NAME (area))
    free (EXPORT_NAME (area));

  EXPORT_NAME (area) = NULL;

  ospf_area_check_free (ospf, area->area_id);
  
  ospf_schedule_abr_task (ospf);

  return 1;
}

int
ospf_area_import_list_set (struct ospf *ospf, struct ospf_area *area, 
                           const char *name)
{
  struct access_list *list;
  list = access_list_lookup (AFI_IP, name);

  IMPORT_LIST (area) = list;

  if (IMPORT_NAME (area))
    free (IMPORT_NAME (area));

  IMPORT_NAME (area) = strdup (name);
  ospf_schedule_abr_task (ospf);

  return 1;
}

int
ospf_area_import_list_unset (struct ospf *ospf, struct ospf_area * area)
{
  IMPORT_LIST (area) = 0;

  if (IMPORT_NAME (area))
    free (IMPORT_NAME (area));

  IMPORT_NAME (area) = NULL;
  ospf_area_check_free (ospf, area->area_id);

  ospf_schedule_abr_task (ospf);

  return 1;
}

int
ospf_timers_refresh_set (struct ospf *ospf, int interval)
{
  int time_left;

  if (ospf->lsa_refresh_interval == interval)
    return 1;

  time_left = ospf->lsa_refresh_interval -
    (quagga_time (NULL) - ospf->lsa_refresher_started);
  
  if (time_left > interval)
    {
      OSPF_TIMER_OFF (ospf->t_lsa_refresher);
      ospf->t_lsa_refresher =
	thread_add_timer (master, ospf_lsa_refresh_walker, ospf, interval);
    }
  ospf->lsa_refresh_interval = interval;

  return 1;
}

int
ospf_timers_refresh_unset (struct ospf *ospf)
{
  int time_left;

  time_left = ospf->lsa_refresh_interval -
    (quagga_time (NULL) - ospf->lsa_refresher_started);

  if (time_left > OSPF_LSA_REFRESH_INTERVAL_DEFAULT)
    {
      OSPF_TIMER_OFF (ospf->t_lsa_refresher);
      ospf->t_lsa_refresher =
	thread_add_timer (master, ospf_lsa_refresh_walker, ospf,
			  OSPF_LSA_REFRESH_INTERVAL_DEFAULT);
    }

  ospf->lsa_refresh_interval = OSPF_LSA_REFRESH_INTERVAL_DEFAULT;

  return 1;
}


static struct ospf_nbr_nbma *
ospf_nbr_nbma_new (void)
{
  struct ospf_nbr_nbma *nbr_nbma;

  nbr_nbma = XCALLOC (MTYPE_OSPF_NEIGHBOR_STATIC,
		      sizeof (struct ospf_nbr_nbma));

  nbr_nbma->priority = OSPF_NEIGHBOR_PRIORITY_DEFAULT;
  nbr_nbma->v_poll = OSPF_POLL_INTERVAL_DEFAULT;

  return nbr_nbma;
}

static void
ospf_nbr_nbma_free (struct ospf_nbr_nbma *nbr_nbma)
{
  XFREE (MTYPE_OSPF_NEIGHBOR_STATIC, nbr_nbma);
}

static void
ospf_nbr_nbma_delete (struct ospf *ospf, struct ospf_nbr_nbma *nbr_nbma)
{
  struct route_node *rn;
  struct prefix_ipv4 p;

  p.family = AF_INET;
  p.prefix = nbr_nbma->addr;
  p.prefixlen = IPV4_MAX_BITLEN;

  rn = route_node_lookup (ospf->nbr_nbma, (struct prefix *)&p);
  if (rn)
    {
      ospf_nbr_nbma_free (rn->info);
      rn->info = NULL;
      route_unlock_node (rn);
      route_unlock_node (rn);
    }
}

static void
ospf_nbr_nbma_down (struct ospf_nbr_nbma *nbr_nbma)
{
  OSPF_TIMER_OFF (nbr_nbma->t_poll);

  if (nbr_nbma->nbr)
    {
      nbr_nbma->nbr->nbr_nbma = NULL;
      OSPF_NSM_EVENT_EXECUTE (nbr_nbma->nbr, NSM_KillNbr);
    }

  if (nbr_nbma->oi)
    listnode_delete (nbr_nbma->oi->nbr_nbma, nbr_nbma);
}

static void
ospf_nbr_nbma_add (struct ospf_nbr_nbma *nbr_nbma,
		   struct ospf_interface *oi)
{
  struct ospf_neighbor *nbr;
  struct route_node *rn;
  struct prefix p;

  if (oi->type != OSPF_IFTYPE_NBMA)
    return;

  if (nbr_nbma->nbr != NULL)
    return;

  if (IPV4_ADDR_SAME (&oi->nbr_self->address.u.prefix4, &nbr_nbma->addr))
    return;
      
  nbr_nbma->oi = oi;
  listnode_add (oi->nbr_nbma, nbr_nbma);

  /* Get neighbor information from table. */
  p.family = AF_INET;
  p.prefixlen = IPV4_MAX_BITLEN;
  p.u.prefix4 = nbr_nbma->addr;

  rn = route_node_get (oi->nbrs, (struct prefix *)&p);
  if (rn->info)
    {
      nbr = rn->info;
      nbr->nbr_nbma = nbr_nbma;
      nbr_nbma->nbr = nbr;

      route_unlock_node (rn);
    }
  else
    {
      nbr = rn->info = ospf_nbr_new (oi);
      nbr->state = NSM_Down;
      nbr->src = nbr_nbma->addr;
      nbr->nbr_nbma = nbr_nbma;
      nbr->priority = nbr_nbma->priority;
      nbr->address = p;

      nbr_nbma->nbr = nbr;

      OSPF_NSM_EVENT_EXECUTE (nbr, NSM_Start);
    }
}

void
ospf_nbr_nbma_if_update (struct ospf *ospf, struct ospf_interface *oi)
{
  struct ospf_nbr_nbma *nbr_nbma;
  struct route_node *rn;
  struct prefix_ipv4 p;

  if (oi->type != OSPF_IFTYPE_NBMA)
    return;

  for (rn = route_top (ospf->nbr_nbma); rn; rn = route_next (rn))
    if ((nbr_nbma = rn->info))
      if (nbr_nbma->oi == NULL && nbr_nbma->nbr == NULL)
	{
	  p.family = AF_INET;
	  p.prefix = nbr_nbma->addr;
	  p.prefixlen = IPV4_MAX_BITLEN;

	  if (prefix_match (oi->address, (struct prefix *)&p))
	    ospf_nbr_nbma_add (nbr_nbma, oi);
	}
}

struct ospf_nbr_nbma *
ospf_nbr_nbma_lookup (struct ospf *ospf, struct in_addr nbr_addr)
{
  struct route_node *rn;
  struct prefix_ipv4 p;

  p.family = AF_INET;
  p.prefix = nbr_addr;
  p.prefixlen = IPV4_MAX_BITLEN;

  rn = route_node_lookup (ospf->nbr_nbma, (struct prefix *)&p);
  if (rn)
    {
      route_unlock_node (rn);
      return rn->info;
    }
  return NULL;
}

struct ospf_nbr_nbma *
ospf_nbr_nbma_lookup_next (struct ospf *ospf, struct in_addr *addr, int first)
{
#if 0
  struct ospf_nbr_nbma *nbr_nbma;
  struct listnode *node;
#endif

  if (ospf == NULL)
    return NULL;

#if 0
  for (ALL_LIST_ELEMENTS_RO (ospf->nbr_nbma, node, nbr_nbma))
    {
      if (first)
	{
	  *addr = nbr_nbma->addr;
	  return nbr_nbma;
	}
      else if (ntohl (nbr_nbma->addr.s_addr) > ntohl (addr->s_addr))
	{
	  *addr = nbr_nbma->addr;
	  return nbr_nbma;
	}
    }
#endif
  return NULL;
}

int
ospf_nbr_nbma_set (struct ospf *ospf, struct in_addr nbr_addr)
{
  struct ospf_nbr_nbma *nbr_nbma;
  struct ospf_interface *oi;
  struct prefix_ipv4 p;
  struct route_node *rn;
  struct listnode *node;

  nbr_nbma = ospf_nbr_nbma_lookup (ospf, nbr_addr);
  if (nbr_nbma)
    return 0;

  nbr_nbma = ospf_nbr_nbma_new ();
  nbr_nbma->addr = nbr_addr;

  p.family = AF_INET;
  p.prefix = nbr_addr;
  p.prefixlen = IPV4_MAX_BITLEN;

  rn = route_node_get (ospf->nbr_nbma, (struct prefix *)&p);
  rn->info = nbr_nbma;

  for (ALL_LIST_ELEMENTS_RO (ospf->oiflist, node, oi))
    {
      if (oi->type == OSPF_IFTYPE_NBMA)
	if (prefix_match (oi->address, (struct prefix *)&p))
	  {
	    ospf_nbr_nbma_add (nbr_nbma, oi);
	    break;
	  }
    }

  return 1;
}

int
ospf_nbr_nbma_unset (struct ospf *ospf, struct in_addr nbr_addr)
{
  struct ospf_nbr_nbma *nbr_nbma;

  nbr_nbma = ospf_nbr_nbma_lookup (ospf, nbr_addr);
  if (nbr_nbma == NULL)
    return 0;

  ospf_nbr_nbma_down (nbr_nbma);
  ospf_nbr_nbma_delete (ospf, nbr_nbma);

  return 1;
}

int
ospf_nbr_nbma_priority_set (struct ospf *ospf, struct in_addr nbr_addr,
			    u_char priority)
{
  struct ospf_nbr_nbma *nbr_nbma;

  nbr_nbma = ospf_nbr_nbma_lookup (ospf, nbr_addr);
  if (nbr_nbma == NULL)
    return 0;

  if (nbr_nbma->priority != priority)
    nbr_nbma->priority = priority;

  return 1;
}

int
ospf_nbr_nbma_priority_unset (struct ospf *ospf, struct in_addr nbr_addr)
{
  struct ospf_nbr_nbma *nbr_nbma;

  nbr_nbma = ospf_nbr_nbma_lookup (ospf, nbr_addr);
  if (nbr_nbma == NULL)
    return 0;

  if (nbr_nbma != OSPF_NEIGHBOR_PRIORITY_DEFAULT)
    nbr_nbma->priority = OSPF_NEIGHBOR_PRIORITY_DEFAULT;

  return 1;
}

int
ospf_nbr_nbma_poll_interval_set (struct ospf *ospf, struct in_addr nbr_addr,
				 unsigned int interval)
{
  struct ospf_nbr_nbma *nbr_nbma;

  nbr_nbma = ospf_nbr_nbma_lookup (ospf, nbr_addr);
  if (nbr_nbma == NULL)
    return 0;

  if (nbr_nbma->v_poll != interval)
    {
      nbr_nbma->v_poll = interval;
      if (nbr_nbma->oi && ospf_if_is_up (nbr_nbma->oi))
	{
	  OSPF_TIMER_OFF (nbr_nbma->t_poll);
	  OSPF_POLL_TIMER_ON (nbr_nbma->t_poll, ospf_poll_timer,
			      nbr_nbma->v_poll);
	}
    }

  return 1;
}

int
ospf_nbr_nbma_poll_interval_unset (struct ospf *ospf, struct in_addr addr)
{
  struct ospf_nbr_nbma *nbr_nbma;

  nbr_nbma = ospf_nbr_nbma_lookup (ospf, addr);
  if (nbr_nbma == NULL)
    return 0;

  if (nbr_nbma->v_poll != OSPF_POLL_INTERVAL_DEFAULT)
    nbr_nbma->v_poll = OSPF_POLL_INTERVAL_DEFAULT;

  return 1;
}

void
ospf_master_init ()
{
  memset (&ospf_master, 0, sizeof (struct ospf_master));

  om = &ospf_master;
  om->ospf = list_new ();
  om->master = thread_master_create ();
  om->start_time = quagga_time (NULL);
}
