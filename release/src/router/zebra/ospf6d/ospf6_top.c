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
#include "ospf6d.h"

/* global ospf6d variable */
struct ospf6 *ospf6;

void
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

void
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

void
ospf6_top_route_hook_add (struct ospf6_route *route)
{
  ospf6_abr_originate_summary (route);
  ospf6_zebra_route_update_add (route);
}

void
ospf6_top_route_hook_remove (struct ospf6_route *route)
{
  ospf6_abr_originate_summary (route);
  ospf6_zebra_route_update_remove (route);
}

void
ospf6_top_brouter_hook_add (struct ospf6_route *route)
{
  ospf6_abr_examin_brouter (ADV_ROUTER_IN_PREFIX (&route->prefix));
  ospf6_asbr_lsentry_add (route);
  ospf6_abr_originate_summary (route);
}

void
ospf6_top_brouter_hook_remove (struct ospf6_route *route)
{
  ospf6_abr_examin_brouter (ADV_ROUTER_IN_PREFIX (&route->prefix));
  ospf6_asbr_lsentry_remove (route);
  ospf6_abr_originate_summary (route);
}

struct ospf6 *
ospf6_create ()
{
  struct ospf6 *o;

  o = XMALLOC (MTYPE_OSPF6_TOP, sizeof (struct ospf6));
  memset (o, 0, sizeof (struct ospf6));

  /* initialize */
  gettimeofday (&o->starttime, (struct timezone *) NULL);
  o->area_list = list_new ();
  o->area_list->cmp = ospf6_area_cmp;
  o->lsdb = ospf6_lsdb_create (o);
  o->lsdb_self = ospf6_lsdb_create (o);
  o->lsdb->hook_add = ospf6_top_lsdb_hook_add;
  o->lsdb->hook_remove = ospf6_top_lsdb_hook_remove;

  o->route_table = ospf6_route_table_create ();
  o->route_table->hook_add = ospf6_top_route_hook_add;
  o->route_table->hook_remove = ospf6_top_route_hook_remove;

  o->brouter_table = ospf6_route_table_create ();
  o->brouter_table->hook_add = ospf6_top_brouter_hook_add;
  o->brouter_table->hook_remove = ospf6_top_brouter_hook_remove;

  o->external_table = ospf6_route_table_create ();
  o->external_id_table = route_table_init ();

  return o;
}

void
ospf6_delete (struct ospf6 *o)
{
  listnode i;
  struct ospf6_area *oa;

  for (i = listhead (o->area_list); i; nextnode (i))
    {
      oa = (struct ospf6_area *) getdata (i);
      ospf6_area_delete (oa);
    }

  ospf6_lsdb_delete (o->lsdb);
  ospf6_lsdb_delete (o->lsdb_self);

  ospf6_route_table_delete (o->route_table);
  ospf6_route_table_delete (o->brouter_table);

  ospf6_route_table_delete (o->external_table);
  route_table_finish (o->external_id_table);

  XFREE (MTYPE_OSPF6_TOP, o);
}

void
ospf6_enable (struct ospf6 *o)
{
  listnode i;
  struct ospf6_area *oa;

  if (CHECK_FLAG (o->flag, OSPF6_DISABLED))
    {
      UNSET_FLAG (o->flag, OSPF6_DISABLED);
      for (i = listhead (o->area_list); i; nextnode (i))
        {
          oa = (struct ospf6_area *) getdata (i);
          ospf6_area_enable (oa);
        }
    }
}

void
ospf6_disable (struct ospf6 *o)
{
  listnode i;
  struct ospf6_area *oa;

  if (! CHECK_FLAG (o->flag, OSPF6_DISABLED))
    {
      SET_FLAG (o->flag, OSPF6_DISABLED);
      for (i = listhead (o->area_list); i; nextnode (i))
        {
          oa = (struct ospf6_area *) getdata (i);
          ospf6_area_disable (oa);
        }

      ospf6_lsdb_remove_all (o->lsdb);
      ospf6_route_remove_all (o->route_table);
      ospf6_route_remove_all (o->brouter_table);
    }
}

int
ospf6_maxage_remover (struct thread *thread)
{
  struct ospf6 *o = (struct ospf6 *) THREAD_ARG (thread);
  struct ospf6_area *oa;
  struct ospf6_interface *oi;
  struct ospf6_neighbor *on;
  listnode i, j, k;

  o->maxage_remover = (struct thread *) NULL;

  for (i = listhead (o->area_list); i; nextnode (i))
    {
      oa = (struct ospf6_area *) getdata (i);
      for (j = listhead (oa->if_list); j; nextnode (j))
        {
          oi = (struct ospf6_interface *) getdata (j);
          for (k = listhead (oi->neighbor_list); k; nextnode (k))
            {
              on = (struct ospf6_neighbor *) getdata (k);
              if (on->state != OSPF6_NEIGHBOR_EXCHANGE &&
                  on->state != OSPF6_NEIGHBOR_LOADING)
                continue;

              return 0;
            }
        }
    }

  for (i = listhead (o->area_list); i; nextnode (i))
    {
      oa = (struct ospf6_area *) getdata (i);
      for (j = listhead (oa->if_list); j; nextnode (j))
        {
          oi = (struct ospf6_interface *) getdata (j);
          OSPF6_LSDB_MAXAGE_REMOVER (oi->lsdb);
        }
      OSPF6_LSDB_MAXAGE_REMOVER (oa->lsdb);
    }
  OSPF6_LSDB_MAXAGE_REMOVER (o->lsdb);

  return 0;
}

void
ospf6_maxage_remove (struct ospf6 *o)
{
  if (o && ! o->maxage_remover)
    o->maxage_remover = thread_add_event (master, ospf6_maxage_remover, o, 0);
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
  if (CHECK_FLAG (ospf6->flag, OSPF6_DISABLED))
    ospf6_enable (ospf6);

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
  if (ospf6 == NULL || CHECK_FLAG (ospf6->flag, OSPF6_DISABLED))
    vty_out (vty, "OSPFv3 is not running%s", VNL);
  else
    ospf6_disable (ospf6);

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

  o->router_id = router_id;
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

  /* start up */
  thread_add_event (master, interface_up, oi, 0);

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
  struct ospf6 *o;
  struct ospf6_interface *oi;
  struct ospf6_area *oa;
  struct interface *ifp;
  u_int32_t area_id;

  o = (struct ospf6 *) vty->index;

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

void
ospf6_show (struct vty *vty, struct ospf6 *o)
{
  listnode n;
  struct ospf6_area *oa;
  char router_id[16], duration[32];
  struct timeval now, running;

  /* process id, router id */
  inet_ntop (AF_INET, &o->router_id, router_id, sizeof (router_id));
  vty_out (vty, " OSPFv3 Routing Process (0) with Router-ID %s%s",
           router_id, VNL);

  /* running time */
  gettimeofday (&now, (struct timezone *)NULL);
  timersub (&now, &o->starttime, &running);
  timerstring (&running, duration, sizeof (duration));
  vty_out (vty, " Running %s%s", duration, VNL);

  /* Redistribute configuration */
  /* XXX */

  /* LSAs */
  vty_out (vty, " Number of AS scoped LSAs is %u%s",
           o->lsdb->count, VNL);

  /* Areas */
  vty_out (vty, " Number of areas in this router is %u%s",
           listcount (o->area_list), VNL);
  for (n = listhead (o->area_list); n; nextnode (n))
    {
      oa = (struct ospf6_area *) getdata (n);
      ospf6_area_show (vty, oa);
    }
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
       );

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
  char *sargv[CMD_ARGC_MAX];
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
  char *sargv[CMD_ARGC_MAX];
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

ALIAS (show_ipv6_ospf6_route,
       show_ipv6_ospf6_route_type_cmd,
       "show ipv6 ospf6 route (intra-area|inter-area|external-1|external-2)",
       SHOW_STR
       IP6_STR
       OSPF6_STR
       ROUTE_STR
       "Dispaly Intra-Area routes\n"
       "Dispaly Inter-Area routes\n"
       "Dispaly Type-1 External routes\n"
       "Dispaly Type-2 External routes\n"
       );

DEFUN (show_ipv6_ospf6_route_type_detail,
       show_ipv6_ospf6_route_type_detail_cmd,
       "show ipv6 ospf6 route (intra-area|inter-area|external-1|external-2) detail",
       SHOW_STR
       IP6_STR
       OSPF6_STR
       ROUTE_STR
       "Dispaly Intra-Area routes\n"
       "Dispaly Inter-Area routes\n"
       "Dispaly Type-1 External routes\n"
       "Dispaly Type-2 External routes\n"
       "Detailed information\n"
       )
{
  char *sargv[CMD_ARGC_MAX];
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

/* OSPF configuration write function. */
int
config_write_ospf6 (struct vty *vty)
{
  char router_id[16];
  listnode j, k;
  struct ospf6_area *oa;
  struct ospf6_interface *oi;

  /* OSPFv6 configuration. */
  if (ospf6 == NULL)
    return CMD_SUCCESS;
  if (CHECK_FLAG (ospf6->flag, OSPF6_DISABLED))
    return CMD_SUCCESS;

  inet_ntop (AF_INET, &ospf6->router_id, router_id, sizeof (router_id));
  vty_out (vty, "router ospf6%s", VNL);
  vty_out (vty, " router-id %s%s", router_id, VNL);

  ospf6_redistribute_config_write (vty);
  ospf6_area_config_write (vty);

  for (j = listhead (ospf6->area_list); j; nextnode (j))
    {
      oa = (struct ospf6_area *) getdata (j);
      for (k = listhead (oa->if_list); k; nextnode (k))
        {
          oi = (struct ospf6_interface *) getdata (k);
          vty_out (vty, " interface %s area %s%s",
                   oi->interface->name, oa->name, VNL);
        }
    }
  vty_out (vty, "!%s", VNL);
  return 0;
}

/* OSPF6 node structure. */
struct cmd_node ospf6_node =
{
  OSPF6_NODE,
  "%s(config-ospf6)# ",
  1 /* VTYSH */
};

/* Install ospf related commands. */
void
ospf6_top_init ()
{
  /* Install ospf6 top node. */
  install_node (&ospf6_node, config_write_ospf6);

  install_element (VIEW_NODE, &show_ipv6_ospf6_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_cmd);
  install_element (CONFIG_NODE, &router_ospf6_cmd);

  install_element (VIEW_NODE, &show_ipv6_ospf6_route_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_route_detail_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_route_match_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_route_match_detail_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_route_type_cmd);
  install_element (VIEW_NODE, &show_ipv6_ospf6_route_type_detail_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_route_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_route_detail_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_route_match_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_route_match_detail_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_route_type_cmd);
  install_element (ENABLE_NODE, &show_ipv6_ospf6_route_type_detail_cmd);

  install_default (OSPF6_NODE);
  install_element (OSPF6_NODE, &ospf6_router_id_cmd);
  install_element (OSPF6_NODE, &ospf6_interface_area_cmd);
  install_element (OSPF6_NODE, &no_ospf6_interface_area_cmd);
  install_element (OSPF6_NODE, &no_router_ospf6_cmd);
}


