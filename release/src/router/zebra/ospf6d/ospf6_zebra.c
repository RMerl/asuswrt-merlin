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
#include "vty.h"
#include "command.h"
#include "prefix.h"
#include "stream.h"
#include "zclient.h"
#include "memory.h"

#include "ospf6_proto.h"
#include "ospf6_top.h"
#include "ospf6_interface.h"
#include "ospf6_route.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"
#include "ospf6_asbr.h"
#include "ospf6_zebra.h"
#include "ospf6d.h"

unsigned char conf_debug_ospf6_zebra = 0;

/* information about zebra. */
struct zclient *zclient = NULL;

/* redistribute function */
void
ospf6_zebra_redistribute (int type)
{
  if (zclient->redist[type])
    return;
  zclient->redist[type] = 1;
  if (zclient->sock > 0)
    zebra_redistribute_send (ZEBRA_REDISTRIBUTE_ADD, zclient->sock, type);
}

void
ospf6_zebra_no_redistribute (int type)
{
  if (! zclient->redist[type])
    return;
  zclient->redist[type] = 0;
  if (zclient->sock > 0)
    zebra_redistribute_send (ZEBRA_REDISTRIBUTE_DELETE, zclient->sock, type);
}

/* Inteface addition message from zebra. */
int
ospf6_zebra_if_add (int command, struct zclient *zclient, zebra_size_t length)
{
  struct interface *ifp;

  ifp = zebra_interface_add_read (zclient->ibuf);
  if (IS_OSPF6_DEBUG_ZEBRA (RECV))
    zlog_info ("Zebra Interface add: %s index %d mtu %d",
               ifp->name, ifp->ifindex, ifp->mtu);
  ospf6_interface_if_add (ifp);
  return 0;
}

int
ospf6_zebra_if_del (int command, struct zclient *zclient, zebra_size_t length)
{
#if 0
  struct interface *ifp;

  ifp = zebra_interface_delete_read (zclient->ibuf);
  if (IS_OSPF6_DEBUG_ZEBRA (RECV))
    zlog_info ("Zebra Interface delete: %s index %d mtu %d",
               ifp->name, ifp->ifindex, ifp->mtu);

  ospf6_interface_if_del (ifp);
#endif /*0*/
  return 0;
}

int
ospf6_zebra_if_state_update (int command, struct zclient *zclient,
                             zebra_size_t length)
{
  struct interface *ifp;

  ifp = zebra_interface_state_read (zclient->ibuf);
  if (IS_OSPF6_DEBUG_ZEBRA (RECV))
    zlog_info ("Zebra Interface state change: "
                 "%s index %d flags %ld metric %d mtu %d",
               ifp->name, ifp->ifindex, ifp->flags, ifp->metric, ifp->mtu);

  ospf6_interface_state_update (ifp);
  return 0;
}

int
ospf6_zebra_if_address_update_add (int command, struct zclient *zclient,
                                   zebra_size_t length)
{
  struct connected *c;
  char buf[128];

  c = zebra_interface_address_add_read (zclient->ibuf);
  if (c == NULL)
    return 0;

  if (IS_OSPF6_DEBUG_ZEBRA (RECV))
    zlog_info ("Zebra Interface address add: %s %5s %s/%d",
               c->ifp->name, prefix_family_str (c->address),
               inet_ntop (c->address->family, &c->address->u.prefix,
                          buf, sizeof (buf)), c->address->prefixlen);

  if (c->address->family == AF_INET6)
    ospf6_interface_connected_route_update (c->ifp);

  return 0;
}

int
ospf6_zebra_if_address_update_delete (int command, struct zclient *zclient,
                               zebra_size_t length)
{
  struct connected *c;
  char buf[128];

  c = zebra_interface_address_delete_read (zclient->ibuf);
  if (c == NULL)
    return 0;

  if (IS_OSPF6_DEBUG_ZEBRA (RECV))
    zlog_info ("Zebra Interface address delete: %s %5s %s/%d",
               c->ifp->name, prefix_family_str (c->address),
               inet_ntop (c->address->family, &c->address->u.prefix,
                          buf, sizeof (buf)), c->address->prefixlen);

  if (c->address->family == AF_INET6)
    ospf6_interface_connected_route_update (c->ifp);

  return 0;
}



const char *zebra_route_name[ZEBRA_ROUTE_MAX] =
  { "System", "Kernel", "Connect", "Static", "RIP", "RIPng", "OSPF",
    "OSPF6", "BGP" };

const char *zebra_route_abname[ZEBRA_ROUTE_MAX] =
  { "X", "K", "C", "S", "r", "R", "o", "O", "B" };

int
ospf6_zebra_read_ipv6 (int command, struct zclient *zclient,
                       zebra_size_t length)
{
  struct stream *s;
  struct zapi_ipv6 api;
  unsigned long ifindex;
  struct prefix_ipv6 p;
  struct in6_addr *nexthop;

  s = zclient->ibuf;
  ifindex = 0;
  nexthop = NULL;
  memset (&api, 0, sizeof (api));

  /* Type, flags, message. */
  api.type = stream_getc (s);
  api.flags = stream_getc (s);
  api.message = stream_getc (s);

  /* IPv6 prefix. */
  memset (&p, 0, sizeof (struct prefix_ipv6));
  p.family = AF_INET6;
  p.prefixlen = stream_getc (s);
  stream_get (&p.prefix, s, PSIZE (p.prefixlen));

  /* Nexthop, ifindex, distance, metric. */
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_NEXTHOP))
    {
      api.nexthop_num = stream_getc (s);
      nexthop = (struct in6_addr *)
        malloc (api.nexthop_num * sizeof (struct in6_addr));
      stream_get (nexthop, s, api.nexthop_num * sizeof (struct in6_addr));
    }
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_IFINDEX))
    {
      api.ifindex_num = stream_getc (s);
      ifindex = stream_getl (s);
    }
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_DISTANCE))
    api.distance = stream_getc (s);
  else
    api.distance = 0;
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_METRIC))
    api.metric = stream_getl (s);
  else
    api.metric = 0;

  if (IS_OSPF6_DEBUG_ZEBRA (RECV))
    {
      char prefixstr[128], nexthopstr[128];
      prefix2str ((struct prefix *)&p, prefixstr, sizeof (prefixstr));
      if (nexthop)
        inet_ntop (AF_INET6, nexthop, nexthopstr, sizeof (nexthopstr));
      else
        snprintf (nexthopstr, sizeof (nexthopstr), "::");

      zlog_info ("Zebra Receive route %s: %s %s nexthop %s ifindex %ld",
                 (command == ZEBRA_IPV6_ROUTE_ADD ? "add" : "delete"),
                 zebra_route_name[api.type], prefixstr, nexthopstr, ifindex);
    }
 
  if (command == ZEBRA_IPV6_ROUTE_ADD)
    ospf6_asbr_redistribute_add (api.type, ifindex, (struct prefix *) &p,
                                 api.nexthop_num, nexthop);
  else
    ospf6_asbr_redistribute_remove (api.type, ifindex, (struct prefix *) &p);

  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_NEXTHOP))
    free (nexthop);

  return 0;
}




DEFUN (show_zebra,
       show_zebra_cmd,
       "show zebra",
       SHOW_STR
       "Zebra information\n")
{
  int i;
  if (zclient == NULL)
    {
      vty_out (vty, "Not connected to zebra%s", VNL);
      return CMD_SUCCESS;
    }

  vty_out (vty, "Zebra Infomation%s", VNL);
  vty_out (vty, "  enable: %d fail: %d%s",
           zclient->enable, zclient->fail, VNL);
  vty_out (vty, "  redistribute default: %d%s", zclient->redist_default,
           VNL);
  vty_out (vty, "  redistribute:");
  for (i = 0; i < ZEBRA_ROUTE_MAX; i++)
    {
      if (zclient->redist[i])
        vty_out (vty, " %s", zebra_route_name[i]);
    }
  vty_out (vty, "%s", VNL);
  return CMD_SUCCESS;
}

DEFUN (router_zebra,
       router_zebra_cmd,
       "router zebra",
       "Enable a routing process\n"
       "Make connection to zebra daemon\n")
{
  vty->node = ZEBRA_NODE;
  zclient->enable = 1;
  zclient_start (zclient);
  return CMD_SUCCESS;
}

DEFUN (no_router_zebra,
       no_router_zebra_cmd,
       "no router zebra",
       NO_STR
       "Configure routing process\n"
       "Disable connection to zebra daemon\n")
{
  zclient->enable = 0;
  zclient_stop (zclient);
  return CMD_SUCCESS;
}

/* Zebra configuration write function. */
int
config_write_ospf6_zebra (struct vty *vty)
{
  if (! zclient->enable)
    {
      vty_out (vty, "no router zebra%s", VNL);
      vty_out (vty, "!%s", VNL);
    }
  else if (! zclient->redist[ZEBRA_ROUTE_OSPF6])
    {
      vty_out (vty, "router zebra%s", VNL);
      vty_out (vty, " no redistribute ospf6%s", VNL);
      vty_out (vty, "!%s", VNL);
    }
  return 0;
}

/* Zebra node structure. */
struct cmd_node zebra_node =
{
  ZEBRA_NODE,
  "%s(config-zebra)# ",
};

#define ADD    0
#define REM    1
static void
ospf6_zebra_route_update (int type, struct ospf6_route *request)
{
  struct zapi_ipv6 api;
  char buf[64], ifname[IFNAMSIZ];
  int nhcount;
  struct in6_addr **nexthops;
  unsigned int *ifindexes;
  int i, ret = 0;
  struct prefix_ipv6 *dest;

  if (IS_OSPF6_DEBUG_ZEBRA (SEND))
    {
      prefix2str (&request->prefix, buf, sizeof (buf));
      zlog_info ("Send %s route: %s",
                 (type == REM ? "remove" : "add"), buf);
    }

  if (zclient->sock < 0)
    {
      if (IS_OSPF6_DEBUG_ZEBRA (SEND))
        zlog_info ("  Not connected to Zebra");
      return;
    }

  if (request->path.origin.adv_router == ospf6->router_id &&
      (request->path.type == OSPF6_PATH_TYPE_EXTERNAL1 ||
       request->path.type == OSPF6_PATH_TYPE_EXTERNAL2))
    {
      if (IS_OSPF6_DEBUG_ZEBRA (SEND))
        zlog_info ("  Ignore self-originated external route");
      return;
    }

  /* If removing is the best path and if there's another path,
     treat this request as add the secondary path */
  if (type == REM && ospf6_route_is_best (request) &&
      request->next && ospf6_route_is_same (request, request->next))
    {
      if (IS_OSPF6_DEBUG_ZEBRA (SEND))
        zlog_info ("  Best-path removal resulted Sencondary addition");
      type = ADD;
      request = request->next;
    }

  /* Only the best path will be sent to zebra. */
  if (! ospf6_route_is_best (request))
    {
      /* this is not preferred best route, ignore */
      if (IS_OSPF6_DEBUG_ZEBRA (SEND))
        zlog_info ("  Ignore non-best route");
      return;
    }

  nhcount = 0;
  for (i = 0; i < OSPF6_MULTI_PATH_LIMIT; i++)
    if (ospf6_nexthop_is_set (&request->nexthop[i]))
      nhcount++;

  if (nhcount == 0)
    {
      if (IS_OSPF6_DEBUG_ZEBRA (SEND))
        zlog_info ("  No nexthop, ignore");
      return;
    }

  /* allocate memory for nexthop_list */
  nexthops = XCALLOC (MTYPE_OSPF6_OTHER,
                      nhcount * sizeof (struct in6_addr *));
  if (nexthops == NULL)
    {
      zlog_warn ("Can't send route to zebra: malloc failed");
      return;
    }

  /* allocate memory for ifindex_list */
  ifindexes = XCALLOC (MTYPE_OSPF6_OTHER,
                       nhcount * sizeof (unsigned int));
  if (ifindexes == NULL)
    {
      zlog_warn ("Can't send route to zebra: malloc failed");
      XFREE (MTYPE_OSPF6_OTHER, nexthops);
      return;
    }

  for (i = 0; i < nhcount; i++)
    {
      if (IS_OSPF6_DEBUG_ZEBRA (SEND))
        {
          inet_ntop (AF_INET6, &request->nexthop[i].address,
                     buf, sizeof (buf));
          if_indextoname (request->nexthop[i].ifindex, ifname);
          zlog_info ("  nexthop: %s%%%s(%d)", buf, ifname,
                     request->nexthop[i].ifindex);
        }
      nexthops[i] = &request->nexthop[i].address;
      ifindexes[i] = request->nexthop[i].ifindex;
    }

  api.type = ZEBRA_ROUTE_OSPF6;
  api.flags = 0;
  api.message = 0;
  SET_FLAG (api.message, ZAPI_MESSAGE_NEXTHOP);
  api.nexthop_num = nhcount;
  api.nexthop = nexthops;
  SET_FLAG (api.message, ZAPI_MESSAGE_IFINDEX);
  api.ifindex_num = nhcount;
  api.ifindex = ifindexes;
  SET_FLAG (api.message, ZAPI_MESSAGE_METRIC);
  api.metric = (request->path.metric_type == 2 ?
                request->path.cost_e2 : request->path.cost);

  dest = (struct prefix_ipv6 *) &request->prefix;
  if (type == REM)
    ret = zapi_ipv6_delete (zclient, dest, &api);
  else
    ret = zapi_ipv6_add (zclient, dest, &api);

  if (ret < 0)
    zlog_err ("zapi_ipv6_%s () failed: %s",
              (type == REM ? "delete" : "add"), strerror (errno));

  XFREE (MTYPE_OSPF6_OTHER, nexthops);
  XFREE (MTYPE_OSPF6_OTHER, ifindexes);

  return;
}

void
ospf6_zebra_route_update_add (struct ospf6_route *request)
{
  if (! zclient->redist[ZEBRA_ROUTE_OSPF6])
    {
      ospf6->route_table->hook_add = NULL;
      ospf6->route_table->hook_remove = NULL;
      return;
    }
  ospf6_zebra_route_update (ADD, request);
}

void
ospf6_zebra_route_update_remove (struct ospf6_route *request)
{
  if (! zclient->redist[ZEBRA_ROUTE_OSPF6])
    {
      ospf6->route_table->hook_add = NULL;
      ospf6->route_table->hook_remove = NULL;
      return;
    }
  ospf6_zebra_route_update (REM, request);
}

DEFUN (redistribute_ospf6,
       redistribute_ospf6_cmd,
       "redistribute ospf6",
       "Redistribute control\n"
       "OSPF6 route\n")
{
  struct ospf6_route *route;

  if (zclient->redist[ZEBRA_ROUTE_OSPF6])
    return CMD_SUCCESS;

  zclient->redist[ZEBRA_ROUTE_OSPF6] = 1;

  if (ospf6 == NULL)
    return CMD_SUCCESS;

  /* send ospf6 route to zebra route table */
  for (route = ospf6_route_head (ospf6->route_table); route;
       route = ospf6_route_next (route))
    ospf6_zebra_route_update_add (route);

  ospf6->route_table->hook_add = ospf6_zebra_route_update_add;
  ospf6->route_table->hook_remove = ospf6_zebra_route_update_remove;

  return CMD_SUCCESS;
}

DEFUN (no_redistribute_ospf6,
       no_redistribute_ospf6_cmd,
       "no redistribute ospf6",
       NO_STR
       "Redistribute control\n"
       "OSPF6 route\n")
{
  struct ospf6_route *route;

  if (! zclient->redist[ZEBRA_ROUTE_OSPF6])
    return CMD_SUCCESS;

  zclient->redist[ZEBRA_ROUTE_OSPF6] = 0;

  if (ospf6 == NULL)
    return CMD_SUCCESS;

  ospf6->route_table->hook_add = NULL;
  ospf6->route_table->hook_remove = NULL;

  /* withdraw ospf6 route from zebra route table */
  for (route = ospf6_route_head (ospf6->route_table); route;
       route = ospf6_route_next (route))
    ospf6_zebra_route_update_remove (route);

  return CMD_SUCCESS;
}

void
ospf6_zebra_init ()
{
  /* Allocate zebra structure. */
  zclient = zclient_new ();
  zclient_init (zclient, ZEBRA_ROUTE_OSPF6);
  zclient->interface_add = ospf6_zebra_if_add;
  zclient->interface_delete = ospf6_zebra_if_del;
  zclient->interface_up = ospf6_zebra_if_state_update;
  zclient->interface_down = ospf6_zebra_if_state_update;
  zclient->interface_address_add = ospf6_zebra_if_address_update_add;
  zclient->interface_address_delete = ospf6_zebra_if_address_update_delete;
  zclient->ipv4_route_add = NULL;
  zclient->ipv4_route_delete = NULL;
  zclient->ipv6_route_add = ospf6_zebra_read_ipv6;
  zclient->ipv6_route_delete = ospf6_zebra_read_ipv6;

  /* redistribute connected route by default */
  /* ospf6_zebra_redistribute (ZEBRA_ROUTE_CONNECT); */

  /* Install zebra node. */
  install_node (&zebra_node, config_write_ospf6_zebra);

  /* Install command element for zebra node. */
  install_element (VIEW_NODE, &show_zebra_cmd);
  install_element (ENABLE_NODE, &show_zebra_cmd);
  install_element (CONFIG_NODE, &router_zebra_cmd);
  install_element (CONFIG_NODE, &no_router_zebra_cmd);

  install_default (ZEBRA_NODE);
  install_element (ZEBRA_NODE, &redistribute_ospf6_cmd);
  install_element (ZEBRA_NODE, &no_redistribute_ospf6_cmd);

  return;
}

/* Debug */

DEFUN (debug_ospf6_zebra_sendrecv,
       debug_ospf6_zebra_sendrecv_cmd,
       "debug ospf6 zebra (send|recv)",
       DEBUG_STR
       OSPF6_STR
       "Debug connection between zebra\n"
       "Debug Sending zebra\n"
       "Debug Receiving zebra\n"
      )
{
  unsigned char level = 0;

  if (argc)
    {
      if (! strncmp (argv[0], "s", 1))
        level = OSPF6_DEBUG_ZEBRA_SEND;
      else if (! strncmp (argv[0], "r", 1))
        level = OSPF6_DEBUG_ZEBRA_RECV;
    }
  else
    level = OSPF6_DEBUG_ZEBRA_SEND | OSPF6_DEBUG_ZEBRA_RECV;

  OSPF6_DEBUG_ZEBRA_ON (level);
  return CMD_SUCCESS;
}

ALIAS (debug_ospf6_zebra_sendrecv,
       debug_ospf6_zebra_cmd,
       "debug ospf6 zebra",
       DEBUG_STR
       OSPF6_STR
       "Debug connection between zebra\n"
      );


DEFUN (no_debug_ospf6_zebra_sendrecv,
       no_debug_ospf6_zebra_sendrecv_cmd,
       "no debug ospf6 zebra (send|recv)",
       NO_STR
       DEBUG_STR
       OSPF6_STR
       "Debug connection between zebra\n"
       "Debug Sending zebra\n"
       "Debug Receiving zebra\n"
      )
{
  unsigned char level = 0;

  if (argc)
    {
      if (! strncmp (argv[0], "s", 1))
        level = OSPF6_DEBUG_ZEBRA_SEND;
      else if (! strncmp (argv[0], "r", 1))
        level = OSPF6_DEBUG_ZEBRA_RECV;
    }
  else
    level = OSPF6_DEBUG_ZEBRA_SEND | OSPF6_DEBUG_ZEBRA_RECV;

  OSPF6_DEBUG_ZEBRA_OFF (level);
  return CMD_SUCCESS;
}

ALIAS (no_debug_ospf6_zebra_sendrecv,
       no_debug_ospf6_zebra_cmd,
       "no debug ospf6 zebra",
       NO_STR
       DEBUG_STR
       OSPF6_STR
       "Debug connection between zebra\n"
      );

int
config_write_ospf6_debug_zebra (struct vty *vty)
{
  if (IS_OSPF6_DEBUG_ZEBRA (SEND) && IS_OSPF6_DEBUG_ZEBRA (RECV))
    vty_out (vty, "debug ospf6 zebra%s", VNL);
  else
    {
      if (IS_OSPF6_DEBUG_ZEBRA (SEND))
        vty_out (vty, "debug ospf6 zebra send%s", VNL);
      if (IS_OSPF6_DEBUG_ZEBRA (RECV))
        vty_out (vty, "debug ospf6 zebra recv%s", VNL);
    }
  return 0;
}

void
install_element_ospf6_debug_zebra ()
{
  install_element (ENABLE_NODE, &debug_ospf6_zebra_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf6_zebra_cmd);
  install_element (ENABLE_NODE, &debug_ospf6_zebra_sendrecv_cmd);
  install_element (ENABLE_NODE, &no_debug_ospf6_zebra_sendrecv_cmd);
  install_element (CONFIG_NODE, &debug_ospf6_zebra_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf6_zebra_cmd);
  install_element (CONFIG_NODE, &debug_ospf6_zebra_sendrecv_cmd);
  install_element (CONFIG_NODE, &no_debug_ospf6_zebra_sendrecv_cmd);
}


