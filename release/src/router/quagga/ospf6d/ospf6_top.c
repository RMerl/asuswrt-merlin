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
#include "memory.h"
#include "vty.h"
#include "linklist.h"
#include "prefix.h"
#include "table.h"
#include "thread.h"
#include "command.h"

#include "ospf6_proto.h"
#include "ospf6_message.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6_route.h"
#include "ospf6_zebra.h"

#include "ospf6_top.h"
#include "ospf6_area.h"
#include "ospf6_interface.h"
#include "ospf6_neighbor.h"

#include "ospf6_flood.h"
#include "ospf6_asbr.h"
#include "ospf6_abr.h"
#include "ospf6_intra.h"
#include "ospf6_spf.h"
#include "ospf6d.h"

/* global ospf6d variable */
struct ospf6 *ospf6;

static void ospf6_disable (struct ospf6 *o);

static void
ospf6_top_lsdb_hook_add (struct ospf6_lsa *lsa)
{
  switch (ntohs (lsa->header->type))
    {
      case OSPF6_LSTYPE_AS_EXTERNAL:
        ospf6_asbr_lsa_add (lsa);
        break;

      default:
        break;
    }
}

static void
ospf6_top_lsdb_hook_remove (struct ospf6_lsa *lsa)
{
  switch (ntohs (lsa->header->type))
    {
      case OSPF6_LSTYPE_AS_EXTERNAL:
        ospf6_asbr_lsa_remove (lsa);
        break;

      default:
        break;
    }
}

static void
ospf6_top_route_hook_add (struct ospf6_route *route)
{
  ospf6_abr_originate_summary (route);
  ospf6_zebra_route_update_add (route);
}

static void
ospf6_top_route_hook_remove (struct ospf6_route *route)
{
  ospf6_abr_originate_summary (route);
  ospf6_zebra_route_update_remove (route);
}

static void
ospf6_top_brouter_hook_add (struct ospf6_route *route)
{
  ospf6_abr_examin_brouter (ADV_ROUTER_IN_PREFIX (&route->prefix));
  ospf6_asbr_lsentry_add (route);
  ospf6_abr_originate_summary (route);
}

static void
ospf6_top_brouter_hook_remove (struct ospf6_route *route)
{
  ospf6_abr_examin_brouter (ADV_ROUTER_IN_PREFIX (&route->prefix));
  ospf6_asbr_lsentry_remove (route);
  ospf6_abr_originate_summary (route);
}

static struct ospf6 *
ospf6_create (void)
{
  struct ospf6 *o;

  o = XCALLOC (MTYPE_OSPF6_TOP, sizeof (struct ospf6));

  /* initialize */
  quagga_gettime (QUAGGA_CLK_MONOTONIC, &o->starttime);
  o->area_list = list_new ();
  o->area_list->cmp = ospf6_area_cmp;
  o->lsdb = ospf6_lsdb_create (o);
  o->lsdb_self = ospf6_lsdb_create (o);
  o->lsdb->hook_add = ospf6_top_lsdb_hook_add;
  o->lsdb->hook_remove = ospf6_top_lsdb_hook_remove;

  o->spf_delay = OSPF_SPF_DELAY_DEFAULT;
  o->spf_holdtime = OSPF_SPF_HOLDTIME_DEFAULT;
  o->spf_max_holdtime = OSPF_SPF_MAX_HOLDTIME_DEFAULT;
  o->spf_hold_multiplier = 1;

  o->route_table = OSPF6_ROUTE_TABLE_CREATE (GLOBAL, ROUTES);
  o->route_table->scope = o;
  o->route_table->hook_add = ospf6_top_route_hook_add;
  o->route_table->hook_remove = ospf6_top_route_hook_remove;

  o->brouter_table = OSPF6_ROUTE_TABLE_CREATE (GLOBAL, BORDER_ROUTERS);
  o->brouter_table->scope = o;
  o->brouter_table->hook_add = ospf6_top_brouter_hook_add;
  o->brouter_table->hook_remove = ospf6_top_brouter_hook_remove;

  o->external_table = OSPF6_ROUTE_TABLE_CREATE (GLOBAL, EXTERNAL_ROUTES);
  o->external_table->scope = o;

  o->external_id_table = route_table_init ();

  o->ref_bandwidth = OSPF6_REFERENCE_BANDWIDTH;

  return o;
}

void
ospf6_delete (struct ospf6 *o)
{
  struct listnode *node, *nnode;
  struct ospf6_area *oa;

  ospf6_disable (ospf6);

  for (ALL_LIST_ELEMENTS (o->area_list, node, nnode, oa))
    ospf6_area_delete (oa);


  list_delete (o->area_list);

  ospf6_lsdb_delete (o->lsdb);
  ospf6_lsdb_delete (o->lsdb_self);

  ospf6_route_table_delete (o->route_table);
  ospf6_route_table_delete (o->brouter_table);

  ospf6_route_table_delete (o->external_table);
  route_table_finish (o->external_id_table);

  XFREE (MTYPE_OSPF6_TOP, o);
}

static void
__attribute__((unused))
ospf6_enable (struct ospf6 *o)
{
  struct listnode *node, *nnode;
  struct ospf6_area *oa;

  if (CHECK_FLAG (o->flag, OSPF6_DISABLED))
    {
      UNSET_FLAG (o->flag, OSPF6_DISABLED);
      for (ALL_LIST_ELEMENTS (o->area_list, node, nnode, oa))
        ospf6_area_enable (oa);
    }
}

static void
ospf6_disable (struct ospf6 *o)
{
  struct listnode *node, *nnode;
  struct ospf6_area *oa;

  if (! CHECK_FLAG (o->flag, OSPF6_DISABLED))
    {
      SET_FLAG (o->flag, OSPF6_DISABLED);
      
      for (ALL_LIST_ELEMENTS (o->area_list, node, nnode, oa))
        ospf6_area_disable (oa);

      /* XXX: This also changes persistent settings */
      ospf6_asbr_redistribute_reset();

      ospf6_lsdb_remove_all (o->lsdb);
      ospf6_route_remove_all (o->route_table);
      ospf6_route_remove_all (o->brouter_table);

      THREAD_OFF(o->maxage_remover);
      THREAD_OFF(o->t_spf_calc);
      THREAD_OFF(o->t_ase_calc);
    }
}

static int
ospf6_maxage_remover (struct thread *thread)
{
  struct ospf6 *o = (struct ospf6 *) THREAD_ARG (thread);
  struct ospf6_area *oa;
  struct ospf6_interface *oi;
  struct ospf6_neighbor *on;
  struct listnode *i, *j, *k;
  int reschedule = 0;

  o->maxage_remover = (struct thread *) NULL;

  for (ALL_LIST_ELEMENTS_RO (o->area_list, i, oa))
    {
      for (ALL_LIST_ELEMENTS_RO (oa->if_list, j, oi))
        {
          for (ALL_LIST_ELEMENTS_RO (oi->neighbor_list, k, on))
            {
              if (on->state != OSPF6_NEIGHBOR_EXCHANGE &&
                  on->state != OSPF6_NEIGHBOR_LOADING)
		  continue;

	      ospf6_maxage_remove (o);
              return 0;
            }
        }
    }

  for (ALL_LIST_ELEMENTS_RO (o->area_list, i, oa))
    {
      for (ALL_LIST_ELEMENTS_RO (oa->if_list, j, oi))
	{
	  if (ospf6_lsdb_maxage_remover (oi->lsdb))
	    {
	      reschedule = 1;
	    }
	}
      
      if (ospf6_lsdb_maxage_remover (oa->lsdb))
	{
	    reschedule = 1;
	}
    }

  if (ospf6_lsdb_maxage_remover (o->lsdb))
    {
      reschedule = 1;
    }

  if (reschedule)
    {
      ospf6_maxage_remove (o);
    }

  return 0;
}

void
ospf6_maxage_remove (struct ospf6 *o)
{
  if (o && ! o->maxage_remover)
    o->maxage_remover = thread_add_timer (master, ospf6_maxage_remover, o,
					  OSPF_LSA_MAXAGE_REMOVE_DELAY_DEFAULT);
}

/* start ospf6 */
DEFUN (router_ospf6,
       router_ospf6_cmd,
       "router ospf6",
       ROUTER_STR
       OSPF6_STR)
{
  if (ospf6 == NULL)
    ospf6 = ospf6_create ();

  /* set current ospf point. */
  vty->node = OSPF6_NODE;
  vty->index = ospf6;

  return CMD_SUCCESS;
}

/* stop ospf6 */
DEFUN (no_router_ospf6,
       no_router_ospf6_cmd,
       "no router ospf6",
       NO_STR
       OSPF6_ROUTER_STR)
{
  if (ospf6 == NULL)
    vty_out (vty, "OSPFv3 is not configured%s", VNL);
  else
    {
      ospf6_delete (ospf6);
      ospf6 = NULL;
    }

  /* return to config node . */
  vty->node = CONFIG_NODE;
  vty->index = NULL;

  return CMD_SUCCESS;
}

/* change Router_ID commands. */
DEFUN (ospf6_router_id,
       ospf6_router_id_cmd,
       "router-id A.B.C.D",
       "Configure OSPF Router-ID\n"
       V4NOTATION_STR)
{
  int ret;
  u_int32_t router_id;
  struct ospf6 *o;

  o = (struct ospf6 *) vty->index;

  ret = inet_pton (AF_INET, argv[0], &router_id);
  if (ret == 0)
    {
      vty_out (vty, "malformed OSPF Router-ID: %s%s", argv[0], VNL);
      return CMD_SUCCESS;
    }

  o->router_id_static = router_id;
  if (o->router_id  == 0)
    o->router_id  = router_id;

  return CMD_SUCCESS;
}

DEFUN (ospf6_log_adjacency_changes,
       ospf6_log_adjacency_changes_cmd,
       "log-adjacency-changes",
       "Log changes in adjacency state\n")
{
  struct ospf6 *ospf6 = vty->index;

  SET_FLAG(ospf6->config_flags, OSPF6_LOG_ADJACENCY_CHANGES);
  return CMD_SUCCESS;
}

DEFUN (ospf6_log_adjacency_changes_detail,
       ospf6_log_adjacency_changes_detail_cmd,
       "log-adjacency-changes detail",
              "Log changes in adjacency state\n"
       "Log all state changes\n")
{
  struct ospf6 *ospf6 = vty->index;

  SET_FLAG(ospf6->config_flags, OSPF6_LOG_ADJACENCY_CHANGES);
  SET_FLAG(ospf6->config_flags, OSPF6_LOG_ADJACENCY_DETAIL);
  return CMD_SUCCESS;
}

DEFUN (no_ospf6_log_adjacency_changes,
       no_ospf6_log_adjacency_changes_cmd,
       "no log-adjacency-changes",
              NO_STR
       "Log changes in adjacency state\n")
{
  struct ospf6 *ospf6 = vty->index;

  UNSET_FLAG(ospf6->config_flags, OSPF6_LOG_ADJACENCY_DETAIL);
  UNSET_FLAG(ospf6->config_flags, OSPF6_LOG_ADJACENCY_CHANGES);
  return CMD_SUCCESS;
}

DEFUN (no_ospf6_log_adjacency_changes_detail,
       no_ospf6_log_adjacency_changes_detail_cmd,
       "no log-adjacency-changes detail",
              NO_STR
              "Log changes in adjacency state\n"
       "Log all state changes\n")
{
  struct ospf6 *ospf6 = vty->index;

  UNSET_FLAG(ospf6->config_flags, OSPF6_LOG_ADJACENCY_DETAIL);
  return CMD_SUCCESS;
}

DEFUN (ospf6_interface_area,
       ospf6_interface_area_cmd,
       "interface IFNAME area A.B.C.D",
       "Enable routing on an IPv6 interface\n"
       IFNAME_STR
       "Specify the OSPF6 area ID\n"
       "OSPF6 area ID in IPv4 address notation\n"
      )
{
  struct ospf6 *o;
  struct ospf6_area *oa;
  struct ospf6_interface *oi;
  struct interface *ifp;
  u_int32_t area_id;

  o = (struct ospf6 *) vty->index;

  /* find/create ospf6 interface */
  ifp = if_get_by_name (argv[0]);
  oi = (struct ospf6_interface *) ifp->info;
  if (oi == NULL)
    oi = ospf6_interface_create (ifp);
  if (oi->area)
    {
      vty_out (vty, "%s already attached to Area %s%s",
               oi->interface->name, oi->area->name, VNL);
      return CMD_SUCCESS;
    }

  /* parse Area-ID */
  if (inet_pton (AF_INET, argv[1], &area_id) != 1)
    {
      vty_out (vty, "Invalid Area-ID: %s%s", argv[1], VNL);
      return CMD_SUCCESS;
    }

  /* find/create ospf6 area */
  oa = ospf6_area_lookup (area_id, o);
  if (oa == NULL)
    oa = ospf6_area_create (area_id, o);

  /* attach interface to area */
  listnode_add (oa->if_list, oi); /* sort ?? */
  oi->area = oa;

  SET_FLAG (oa->flag, OSPF6_AREA_ENABLE);

  /* ospf6 process is currently disabled, not much more to do */
  if (CHECK_FLAG (o->flag, OSPF6_DISABLED))
    return CMD_SUCCESS;

  /* start up */
  ospf6_interface_enable (oi);

  /* If the router is ABR, originate summary routes */
  if (ospf6_is_router_abr (o))
    ospf6_abr_enable_area (oa);

  return CMD_SUCCESS;
}

DEFUN (no_ospf6_interface_area,
       no_ospf6_interface_area_cmd,
       "no interface IFNAME area A.B.C.D",
       NO_STR
       "Disable routing on an IPv6 interface\n"
       IFNAME_STR
       "Specify the OSPF6 area ID\n"
       "OSPF6 area ID in IPv4 address notation\n"
       )
{
  struct ospf6_interface *oi;
  struct ospf6_area *oa;
  struct interface *ifp;
  u_int32_t area_id;

  ifp = if_lookup_by_name (argv[0]);
  if (ifp == NULL)
    {
      vty_out (vty, "No such interface %s%s", argv[0], VNL);
      return CMD_SUCCESS;
    }

  oi = (struct ospf6_interface *) ifp->info;
  if (oi == NULL)
    {
      vty_out (vty, "Interface %s not enabled%s", ifp->name, VNL);
      return CMD_SUCCESS;
    }

  /* parse Area-ID */
  if (inet_pton (AF_INET, argv[1], &area_id) != 1)
    {
      vty_out (vty, "Invalid Area-ID: %s%s", argv[1], VNL);
      return CMD_SUCCESS;
    }

  /* Verify Area */
  if (oi->area == NULL)
    {
      vty_out (vty, "No such Area-ID: %s%s", argv[1], VNL);
      return CMD_SUCCESS;
    }

  if (oi->area->area_id != area_id)
    {
      vty_out (vty, "Wrong Area-ID: %s is attached to area %s%s",
               oi->interface->name, oi->area->name, VNL);
      return CMD_SUCCESS;
    }

  thread_execute (master, interface_down, oi, 0);

  oa = oi->area;
  listnode_delete (oi->area->if_list, oi);
  oi->area = (struct ospf6_area *) NULL;

  /* Withdraw inter-area routes from this area, if necessary */
  if (oa->if_list->count == 0)
    {
      UNSET_FLAG (oa->flag, OSPF6_AREA_ENABLE);
      ospf6_abr_disable_area (oa);
    }

  return CMD_SUCCESS;
}

DEFUN (ospf6_stub_router_admin,
       ospf6_stub_router_admin_cmd,
       "stub-router administrative",
       "Make router a stub router\n"
       "Advertise inability to be a transit router\n"
       "Administratively applied, for an indefinite period\n")
{
  struct listnode *node;
  struct ospf6_area *oa;

  if (!CHECK_FLAG (ospf6->flag, OSPF6_STUB_ROUTER))
    {
      for (ALL_LIST_ELEMENTS_RO (ospf6->area_list, node, oa))
	{
	   OSPF6_OPT_CLEAR (oa->options, OSPF6_OPT_V6);
	   OSPF6_OPT_CLEAR (oa->options, OSPF6_OPT_R);
	   OSPF6_ROUTER_LSA_SCHEDULE (oa);
	}
      SET_FLAG (ospf6->flag, OSPF6_STUB_ROUTER);
    }

  return CMD_SUCCESS;
}

DEFUN (no_ospf6_stub_router_admin,
       no_ospf6_stub_router_admin_cmd,
       "no stub-router administrative",
       NO_STR
       "Make router a stub router\n"
       "Advertise ability to be a transit router\n"
       "Administratively applied, for an indefinite period\n")
{
  struct listnode *node;
  struct ospf6_area *oa;

  if (CHECK_FLAG (ospf6->flag, OSPF6_STUB_ROUTER))
    {
      for (ALL_LIST_ELEMENTS_RO (ospf6->area_list, node, oa))
	{
	   OSPF6_OPT_SET (oa->options, OSPF6_OPT_V6);
	   OSPF6_OPT_SET (oa->options, OSPF6_OPT_R);
	   OSPF6_ROUTER_LSA_SCHEDULE (oa);
	}
      UNSET_FLAG (ospf6->flag, OSPF6_STUB_ROUTER);
    }

  return CMD_SUCCESS;
}

DEFUN (ospf6_stub_router_startup,
       ospf6_stub_router_startup_cmd,
       "stub-router on-startup <5-86400>",
       "Make router a stub router\n"
       "Advertise inability to be a transit router\n"
       "Automatically advertise as stub-router on startup of OSPF6\n"
       "Time (seconds) to advertise self as stub-router\n")
{
  return CMD_SUCCESS;
}

DEFUN (no_ospf6_stub_router_startup,
       no_ospf6_stub_router_startup_cmd,
       "no stub-router on-startup",
       NO_STR
       "Make router a stub router\n"
       "Advertise inability to be a transit router\n"
       "Automatically advertise as stub-router on startup of OSPF6\n"
       "Time (seconds) to advertise self as stub-router\n")
{
  return CMD_SUCCESS;
}

DEFUN (ospf6_stub_router_shutdown,
       ospf6_stub_router_shutdown_cmd,
       "stub-router on-shutdown <5-86400>",
       "Make router a stub router\n"
       "Advertise inability to be a transit router\n"
       "Automatically advertise as stub-router before shutdown\n"
       "Time (seconds) to advertise self as stub-router\n")
{
  return CMD_SUCCESS;
}

DEFUN (no_ospf6_stub_router_shutdown,
       no_ospf6_stub_router_shutdown_cmd,
       "no stub-router on-shutdown",
       NO_STR
       "Make router a stub router\n"
       "Advertise inability to be a transit router\n"
       "Automatically advertise as stub-router before shutdown\n"
       "Time (seconds) to advertise self as stub-router\n")
{
  return CMD_SUCCESS;
}

static void
ospf6_show (struct vty *vty, struct ospf6 *o)
{
  struct listnode *n;
  struct ospf6_area *oa;
  char router_id[16], duration[32];
  struct timeval now, running, result;
  char buf[32], rbuf[32];

  /* process id, router id */
  inet_ntop (AF_INET, &o->router_id, router_id, sizeof (router_id));
  vty_out (vty, " OSPFv3 Routing Process (0) with Router-ID %s%s",
           router_id, VNL);

  /* running time */
  quagga_gettime (QUAGGA_CLK_MONOTONIC, &now);
  timersub (&now, &o->starttime, &running);
  timerstring (&running, duration, sizeof (duration));
  vty_out (vty, " Running %s%s", duration, VNL);

  /* Redistribute configuration */
  /* XXX */

  /* Show SPF parameters */
  vty_out(vty, " Initial SPF scheduling delay %d millisec(s)%s"
	  " Minimum hold time between consecutive SPFs %d millsecond(s)%s"
	  " Maximum hold time between consecutive SPFs %d millsecond(s)%s"
	  " Hold time multiplier is currently %d%s",
	  o->spf_delay, VNL,
	  o->spf_holdtime, VNL,
	  o->spf_max_holdtime, VNL,
	  o->spf_hold_multiplier, VNL);

  vty_out(vty, " SPF algorithm ");
  if (o->ts_spf.tv_sec || o->ts_spf.tv_usec)
    {
      timersub(&now, &o->ts_spf, &result);
      timerstring(&result, buf, sizeof(buf));
      ospf6_spf_reason_string(o->last_spf_reason, rbuf, sizeof(rbuf));
      vty_out(vty, "last executed %s ago, reason %s%s", buf, rbuf, VNL);
      vty_out (vty, " Last SPF duration %ld sec %ld usec%s",
	       o->ts_spf_duration.tv_sec, o->ts_spf_duration.tv_usec, VNL);
    }
  else
    vty_out(vty, "has not been run$%s", VNL);
  threadtimer_string(now, o->t_spf_calc, buf, sizeof(buf));
  vty_out (vty, " SPF timer %s%s%s",
	   (o->t_spf_calc ? "due in " : "is "), buf, VNL);

  if (CHECK_FLAG (o->flag, OSPF6_STUB_ROUTER))
    vty_out (vty, " Router Is Stub Router%s", VNL);

  /* LSAs */
  vty_out (vty, " Number of AS scoped LSAs is %u%s",
           o->lsdb->count, VNL);

  /* Areas */
  vty_out (vty, " Number of areas in this router is %u%s",
           listcount (o->area_list), VNL);

  if (CHECK_FLAG(o->config_flags, OSPF6_LOG_ADJACENCY_CHANGES))
    {
      if (CHECK_FLAG(o->config_flags, OSPF6_LOG_ADJACENCY_DETAIL))
	vty_out(vty, " All adjacency changes are logged%s",VTY_NEWLINE);
      else
	vty_out(vty, " Adjacency changes are logged%s",VTY_NEWLINE);
    }

  vty_out (vty, "%s",VTY_NEWLINE);

  for (ALL_LIST_ELEMENTS_RO (o->area_list, n, oa))
    ospf6_area_show (vty, oa);
}

/* show top level structures */
DEFUN (show_ipv6_ospf6,
       show_ipv6_ospf6_cmd,
       "show ipv6 ospf6",
       SHOW_STR
       IP6_STR
       OSPF6_STR)
{
  OSPF6_CMD_CHECK_RUNNING ();

  ospf6_show (vty, ospf6);
  return CMD_SUCCESS;
}

DEFUN (show_ipv6_ospf6_route,
       show_ipv6_ospf6_route_cmd,
       "show ipv6 ospf6 route",
       SHOW_STR
       IP6_STR
       OSPF6_STR
       ROUTE_STR
       )
{
  ospf6_route_table_show (vty, argc, argv, ospf6->route_table);
  return CMD_SUCCESS;
}

ALIAS (show_ipv6_ospf6_route,
       show_ipv6_ospf6_route_detail_cmd,
       "show ipv6 ospf6 route (X:X::X:X|X:X::X:X/M|detail|summary)",
       SHOW_STR
       IP6_STR
       OSPF6_STR
       ROUTE_STR
       "Specify IPv6 address\n"
       "Specify IPv6 prefix\n"
       "Detailed information\n"
       "Summary of route table\n"
       )

DEFUN (show_ipv6_ospf6_route_match,
       show_ipv6_ospf6_route_match_cmd,
       "show ipv6 ospf6 route X:X::X:X/M match",
       SHOW_STR
       IP6_STR
       OSPF6_STR
       ROUTE_STR
       "Specify IPv6 prefix\n"
       "Display routes which match the specified route\n"
       )
{
  const char *sargv[CMD_ARGC_MAX];
  int i, sargc;

  /* copy argv to sargv and then append "match" */
  for (i = 0; i < argc; i++)
    sargv[i] = argv[i];
  sargc = argc;
  sargv[sargc++] = "match";
  sargv[sargc] = NULL;

  ospf6_route_table_show (vty, sargc, sargv, ospf6->route_table);
  return CMD_SUCCESS;
}

DEFUN (show_ipv6_ospf6_route_match_detail,
       show_ipv6_ospf6_route_match_detail_cmd,
       "show ipv6 ospf6 route X:X::X:X/M match detail",
       SHOW_STR
       IP6_STR
       OSPF6_STR
       ROUTE_STR
       "Specify IPv6 prefix\n"
       "Display routes which match the specified route\n"
       "Detailed information\n"
       )
{
  const char *sargv[CMD_ARGC_MAX];
  int i, sargc;

  /* copy argv to sargv and then append "match" and "detail" */
  for (i = 0; i < argc; i++)
    sargv[i] = argv[i];
  sargc = argc;
  sargv[sargc++] = "match";
  sargv[sargc++] = "detail";
  sargv[sargc] = NULL;

  ospf6_route_table_show (vty, sargc, sargv, ospf6->route_table);
  return CMD_SUCCESS;
}

ALIAS (show_ipv6_ospf6_route_match,
       show_ipv6_ospf6_route_longer_cmd,
       "show ipv6 ospf6 route X:X::X:X/M longer",
       SHOW_STR
       IP6_STR
       OSPF6_STR
       ROUTE_STR
       "Specify IPv6 prefix\n"
       "Display routes longer than the specified route\n"
       )

DEFUN (show_ipv6_ospf6_route_match_detail,
       show_ipv6_ospf6_route_longer_detail_cmd,
       "show ipv6 ospf6 route X:X::X:X/M longer detail",
       SHOW_STR
       IP6_STR
       OSPF6_STR
       ROUTE_STR
       "Specify IPv6 prefix\n"
       "Display routes longer than the specified route\n"
       "Detailed information\n"
       );

ALIAS (show_ipv6_ospf6_route,
       show_ipv6_ospf6_route_type_cmd,
       "show ipv6 ospf6 route (intra-area|inter-area|external-1|external-2)",
       SHOW_STR
       IP6_STR
       OSPF6_STR
       ROUTE_STR
       "Display Intra-Area routes\n"
       "Display Inter-Area routes\n"
       "Display Type-1 External routes\n"
       "Display Type-2 External routes\n"
       )

DEFUN (show_ipv6_ospf6_route_type_detail,
       show_ipv6_ospf6_route_type_detail_cmd,
       "show ipv6 ospf6 route (intra-area|inter-area|external-1|external-2) detail",
       SHOW_STR
       IP6_STR
       OSPF6_STR
       ROUTE_STR
       "Display Intra-Area routes\n"
       "Display Inter-Area routes\n"
       "Display Type-1 External routes\n"
       "Display Type-2 External routes\n"
       "Detailed information\n"
       )
{
  const char *sargv[CMD_ARGC_MAX];
  int i, sargc;

  /* copy argv to sargv and then append "detail" */
  for (i = 0; i < argc; i++)
    sargv[i] = argv[i];
  sargc = argc;
  sargv[sargc++] = "detail";
  sargv[sargc] = NULL;

  ospf6_route_table_show (vty, sargc, sargv, ospf6->route_table);
  return CMD_SUCCESS;
}

static void
ospf6_stub_router_config_write (struct vty *vty)
{
  if (CHECK_FLAG (ospf6->flag, OSPF6_STUB_ROUTER))
    {
      vty_out (vty, " stub-router administrative%s", VNL);
    }
    return;
}

/* OSPF configuration write function. */
static int
config_write_ospf6 (struct vty *vty)
{
  char router_id[16];
  struct listnode *j, *k;
  struct ospf6_area *oa;
  struct ospf6_interface *oi;

  /* OSPFv6 configuration. */
  if (ospf6 == NULL)
    return CMD_SUCCESS;

  inet_ntop (AF_INET, &ospf6->router_id_static, router_id, sizeof (router_id));
  vty_out (vty, "router ospf6%s", VNL);
  if (ospf6->router_id_static != 0)
    vty_out (vty, " router-id %s%s", router_id, VNL);

  /* log-adjacency-changes flag print. */
  if (CHECK_FLAG(ospf6->config_flags, OSPF6_LOG_ADJACENCY_CHANGES))
    {
      vty_out(vty, " log-adjacency-changes");
      if (CHECK_FLAG(ospf6->config_flags, OSPF6_LOG_ADJACENCY_DETAIL))
	vty_out(vty, " detail");
      vty_out(vty, "%s", VTY_NEWLINE);
    }

  if (ospf6->ref_bandwidth != OSPF6_REFERENCE_BANDWIDTH)
    vty_out (vty, " auto-cost reference-bandwidth %d%s", ospf6->ref_bandwidth / 1000,
             VNL);

  ospf6_stub_router_config_write (vty);
  ospf6_redistribute_config_write (vty);
  ospf6_area_config_write (vty);
  ospf6_spf_config_write (vty);

  for (ALL_LIST_ELEMENTS_RO (ospf6->area_list, j, oa))
    {
      for (ALL_LIST_ELEMENTS_RO (oa->if_list, k, oi))
        vty_out (vty, " interface %s area %s%s",
                 oi->interface->name, oa->name, VNL);
    }
  vty_out (vty, "!%s", VNL);
  return 0;
}

/* OSPF6 node structure. */
static struct cmd_node ospf6_node =
{
  OSPF6_NODE,
  "%s(config-ospf6)# ",
  1 /* VTYSH */
};

/* Install ospf related commands. */
void
ospf6_top_init (void)
{
  /* Install ospf6 top node. */
  install_node (&ospf6_node, config_write_ospf6);

  install_element (VIEW_NODE, &show_ipv6_ospf6_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_cmd);
  install_element (CONFIG_NODE, &router_ospf6_cmd);
  install_element (CONFIG_NODE, &no_router_ospf6_cmd);

  install_element (VIEW_NODE, &show_ipv6_ospf6_route_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_route_detail_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_route_match_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_route_match_detail_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_route_longer_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_route_longer_detail_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_route_type_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_route_type_detail_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_route_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_route_detail_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_route_match_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_route_match_detail_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_route_longer_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_route_longer_detail_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_route_type_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_route_type_detail_cmd);

  install_default (OSPF6_NODE);
  install_element (OSPF6_NODE, &ospf6_router_id_cmd);
  install_element (OSPF6_NODE, &ospf6_log_adjacency_changes_cmd);
  install_element (OSPF6_NODE, &ospf6_log_adjacency_changes_detail_cmd);
  install_element (OSPF6_NODE, &no_ospf6_log_adjacency_changes_cmd);
  install_element (OSPF6_NODE, &no_ospf6_log_adjacency_changes_detail_cmd);
  install_element (OSPF6_NODE, &ospf6_interface_area_cmd);
  install_element (OSPF6_NODE, &no_ospf6_interface_area_cmd);
  install_element (OSPF6_NODE, &ospf6_stub_router_admin_cmd);
  install_element (OSPF6_NODE, &no_ospf6_stub_router_admin_cmd);
  /* For a later time
  install_element (OSPF6_NODE, &ospf6_stub_router_startup_cmd);
  install_element (OSPF6_NODE, &no_ospf6_stub_router_startup_cmd);
  install_element (OSPF6_NODE, &ospf6_stub_router_shutdown_cmd);
  install_element (OSPF6_NODE, &no_ospf6_stub_router_shutdown_cmd);
  */
}


