/* RIPd and zebra interface.
 * Copyright (C) 1997, 1999 Kunihiro Ishiguro <kunihiro@zebra.org>
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

#include "command.h"
#include "prefix.h"
#include "stream.h"
#include "routemap.h"
#include "zclient.h"
#include "log.h"
#include "ripd/ripd.h"
#include "ripd/rip_debug.h"

/* All information about zebra. */
struct zclient *zclient = NULL;

/* Callback prototypes for zebra client service. */
int rip_interface_add (int, struct zclient *, zebra_size_t);
int rip_interface_delete (int, struct zclient *, zebra_size_t);
int rip_interface_address_add (int, struct zclient *, zebra_size_t);
int rip_interface_address_delete (int, struct zclient *, zebra_size_t);
int rip_interface_up (int, struct zclient *, zebra_size_t);
int rip_interface_down (int, struct zclient *, zebra_size_t);

/* RIPd to zebra command interface. */
void
rip_zebra_ipv4_add (struct prefix_ipv4 *p, struct in_addr *nexthop, 
		    u_int32_t metric, u_char distance)
{
  struct zapi_ipv4 api;

  if (zclient->redist[ZEBRA_ROUTE_RIP])
    {
      api.type = ZEBRA_ROUTE_RIP;
      api.flags = 0;
      api.message = 0;
      SET_FLAG (api.message, ZAPI_MESSAGE_NEXTHOP);
      api.nexthop_num = 1;
      api.nexthop = &nexthop;
      api.ifindex_num = 0;
      SET_FLAG (api.message, ZAPI_MESSAGE_METRIC);

      /* This modify is for the routing table hop count show on web,
         actually we should use "api.metric = metric - ifp->metric",
         but it is too complicated(^..^) and "- 1" is enough to use for now!
         Lili. 2007-07-06 */
#if 0
      api.metric = metric;
#else
      api.metric = metric - 1;
#endif

      if (distance && distance != ZEBRA_RIP_DISTANCE_DEFAULT)
	{
	  SET_FLAG (api.message, ZAPI_MESSAGE_DISTANCE);
	  api.distance = distance;
	}

      zapi_ipv4_add (zclient, p, &api);

      rip_global_route_changes++;
    }
}

void
rip_zebra_ipv4_delete (struct prefix_ipv4 *p, struct in_addr *nexthop, 
		       u_int32_t metric)
{
  struct zapi_ipv4 api;

  if (zclient->redist[ZEBRA_ROUTE_RIP])
    {
      api.type = ZEBRA_ROUTE_RIP;
      api.flags = 0;
      api.message = 0;
      SET_FLAG (api.message, ZAPI_MESSAGE_NEXTHOP);
      api.nexthop_num = 1;
      api.nexthop = &nexthop;
      api.ifindex_num = 0;
      SET_FLAG (api.message, ZAPI_MESSAGE_METRIC);
      api.metric = metric;

      zapi_ipv4_delete (zclient, p, &api);

      rip_global_route_changes++;
    }
}

/* Zebra route add and delete treatment. */
int
rip_zebra_read_ipv4 (int command, struct zclient *zclient, zebra_size_t length)
{
  struct stream *s;
  struct zapi_ipv4 api;
  unsigned long ifindex;
  struct in_addr nexthop;
  struct prefix_ipv4 p;
  
  s = zclient->ibuf;
  ifindex = 0;
  nexthop.s_addr = 0;

  /* Type, flags, message. */
  api.type = stream_getc (s);
  api.flags = stream_getc (s);
  api.message = stream_getc (s);

  /* IPv4 prefix. */
  memset (&p, 0, sizeof (struct prefix_ipv4));
  p.family = AF_INET;
  p.prefixlen = stream_getc (s);
  stream_get (&p.prefix, s, PSIZE (p.prefixlen));

  /* Nexthop, ifindex, distance, metric. */
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_NEXTHOP))
    {
      api.nexthop_num = stream_getc (s);
      nexthop.s_addr = stream_get_ipv4 (s);
    }
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_IFINDEX))
    {
      api.ifindex_num = stream_getc (s);
      ifindex = stream_getl (s);
    }
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_DISTANCE))
    api.distance = stream_getc (s);
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_METRIC))
    api.metric = stream_getl (s);

  /* Then fetch IPv4 prefixes. */
  if (command == ZEBRA_IPV4_ROUTE_ADD)
    rip_redistribute_add (api.type, RIP_ROUTE_REDISTRIBUTE, &p, ifindex, &nexthop);
  else 
    rip_redistribute_delete (api.type, RIP_ROUTE_REDISTRIBUTE, &p, ifindex);

  return 0;
}

void
rip_zclient_reset ()
{
  zclient_reset (zclient);
}

/* RIP route-map set for redistribution */
void
rip_routemap_set (int type, char *name)
{
  if (rip->route_map[type].name)
    free(rip->route_map[type].name);

  rip->route_map[type].name = strdup (name);
  rip->route_map[type].map = route_map_lookup_by_name (name);
}

void
rip_redistribute_metric_set (int type, int metric)
{
  rip->route_map[type].metric_config = 1;
  rip->route_map[type].metric = metric;
}

int
rip_metric_unset (int type,int metric)
{
#define DONT_CARE_METRIC_RIP 17  
  if (metric != DONT_CARE_METRIC_RIP &&
      rip->route_map[type].metric != metric)
    return 1;
  rip->route_map[type].metric_config = 0;
  rip->route_map[type].metric = 0;
  return 0;
}

/* RIP route-map unset for redistribution */
int
rip_routemap_unset (int type,char *name)
{
  if (! rip->route_map[type].name ||
      (name != NULL && strcmp(rip->route_map[type].name,name)))
    return 1;

  free (rip->route_map[type].name);
  rip->route_map[type].name = NULL;
  rip->route_map[type].map = NULL;

  return 0;
}

/* Redistribution types */
static struct {
  int type;
  int str_min_len;
  char *str;
} redist_type[] = {
  {ZEBRA_ROUTE_KERNEL,  1, "kernel"},
  {ZEBRA_ROUTE_CONNECT, 1, "connected"},
  {ZEBRA_ROUTE_STATIC,  1, "static"},
  {ZEBRA_ROUTE_OSPF,    1, "ospf"},
  {ZEBRA_ROUTE_BGP,     1, "bgp"},
  {0, 0, NULL}
};

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
       "Enable a routing process\n"
       "Make connection to zebra daemon\n")
{
  zclient->enable = 0;
  zclient_stop (zclient);
  return CMD_SUCCESS;
}

int
rip_redistribute_set (int type)
{
  if (zclient->redist[type])
    return CMD_SUCCESS;

  zclient->redist[type] = 1;

  if (zclient->sock > 0)
    zebra_redistribute_send (ZEBRA_REDISTRIBUTE_ADD, zclient->sock, type);

  return CMD_SUCCESS;
}

int
rip_redistribute_unset (int type)
{
  if (! zclient->redist[type])
    return CMD_SUCCESS;

  zclient->redist[type] = 0;

  if (zclient->sock > 0)
    zebra_redistribute_send (ZEBRA_REDISTRIBUTE_DELETE, zclient->sock, type);

  /* Remove the routes from RIP table. */
  rip_redistribute_withdraw (type);

  return CMD_SUCCESS;
}

int
rip_redistribute_check (int type)
{
  return (zclient->redist[type]);
}

void
rip_redistribute_clean ()
{
  int i;

  for (i = 0; redist_type[i].str; i++)
    {
      if (zclient->redist[redist_type[i].type])
	{
	  if (zclient->sock > 0)
	    zebra_redistribute_send (ZEBRA_REDISTRIBUTE_DELETE,
				     zclient->sock, redist_type[i].type);

	  zclient->redist[redist_type[i].type] = 0;

	  /* Remove the routes from RIP table. */
	  rip_redistribute_withdraw (redist_type[i].type);
	}
    }
}

DEFUN (rip_redistribute_rip,
       rip_redistribute_rip_cmd,
       "redistribute rip",
       "Redistribute information from another routing protocol\n"
       "Routing Information Protocol (RIP)\n")
{
  zclient->redist[ZEBRA_ROUTE_RIP] = 1;
  return CMD_SUCCESS;
}

DEFUN (no_rip_redistribute_rip,
       no_rip_redistribute_rip_cmd,
       "no redistribute rip",
       NO_STR
       "Redistribute information from another routing protocol\n"
       "Routing Information Protocol (RIP)\n")
{
  zclient->redist[ZEBRA_ROUTE_RIP] = 0;
  return CMD_SUCCESS;
}

DEFUN (rip_redistribute_type,
       rip_redistribute_type_cmd,
       "redistribute (kernel|connected|static|ospf|bgp)",
       "Redistribute information from another routing protocol\n"
       "Kernel routes\n"
       "Connected\n"
       "Static routes\n"
       "Open Shortest Path First (OSPF)\n"
       "Border Gateway Protocol (BGP)\n")
{
  int i;

  for(i = 0; redist_type[i].str; i++) 
    {
      if (strncmp (redist_type[i].str, argv[0], 
		   redist_type[i].str_min_len) == 0) 
	{
	  zclient_redistribute_set (zclient, redist_type[i].type);
	  return CMD_SUCCESS;
	}
    }

  vty_out(vty, "Invalid type %s%s", argv[0],
	  VTY_NEWLINE);

  return CMD_WARNING;
}

DEFUN (no_rip_redistribute_type,
       no_rip_redistribute_type_cmd,
       "no redistribute (kernel|connected|static|ospf|bgp)",
       NO_STR
       "Redistribute information from another routing protocol\n"
       "Kernel routes\n"
       "Connected\n"
       "Static routes\n"
       "Open Shortest Path First (OSPF)\n"
       "Border Gateway Protocol (BGP)\n")
{
  int i;

  for (i = 0; redist_type[i].str; i++) 
    {
      if (strncmp(redist_type[i].str, argv[0], 
		  redist_type[i].str_min_len) == 0) 
	{
	  rip_metric_unset (redist_type[i].type, DONT_CARE_METRIC_RIP);
	  rip_routemap_unset (redist_type[i].type,NULL);
	  rip_redistribute_unset (redist_type[i].type);
	  return CMD_SUCCESS;
        }
    }

  vty_out(vty, "Invalid type %s%s", argv[0],
	  VTY_NEWLINE);

  return CMD_WARNING;
}

DEFUN (rip_redistribute_type_routemap,
       rip_redistribute_type_routemap_cmd,
       "redistribute (kernel|connected|static|ospf|bgp) route-map WORD",
       "Redistribute information from another routing protocol\n"
       "Kernel routes\n"
       "Connected\n"
       "Static routes\n"
       "Open Shortest Path First (OSPF)\n"
       "Border Gateway Protocol (BGP)\n"
       "Route map reference\n"
       "Pointer to route-map entries\n")
{
  int i;

  for (i = 0; redist_type[i].str; i++) {
    if (strncmp(redist_type[i].str, argv[0],
		redist_type[i].str_min_len) == 0) 
      {
	rip_routemap_set (redist_type[i].type, argv[1]);
	zclient_redistribute_set (zclient, redist_type[i].type);
	return CMD_SUCCESS;
      }
  }

  vty_out(vty, "Invalid type %s%s", argv[0],
	  VTY_NEWLINE);

  return CMD_WARNING;
}

DEFUN (no_rip_redistribute_type_routemap,
       no_rip_redistribute_type_routemap_cmd,
       "no redistribute (kernel|connected|static|ospf|bgp) route-map WORD",
       NO_STR
       "Redistribute information from another routing protocol\n"
       "Kernel routes\n"
       "Connected\n"
       "Static routes\n"
       "Open Shortest Path First (OSPF)\n"
       "Border Gateway Protocol (BGP)\n"
       "Route map reference\n"
       "Pointer to route-map entries\n")
{
  int i;

  for (i = 0; redist_type[i].str; i++) 
    {
      if (strncmp(redist_type[i].str, argv[0], 
		  redist_type[i].str_min_len) == 0) 
	{
	  if (rip_routemap_unset (redist_type[i].type,argv[1]))
	    return CMD_WARNING;
	  rip_redistribute_unset (redist_type[i].type);
	  return CMD_SUCCESS;
        }
    }

  vty_out(vty, "Invalid type %s%s", argv[0],
	  VTY_NEWLINE);

  return CMD_WARNING;
}

DEFUN (rip_redistribute_type_metric,
       rip_redistribute_type_metric_cmd,
       "redistribute (kernel|connected|static|ospf|bgp) metric <0-16>",
       "Redistribute information from another routing protocol\n"
       "Kernel routes\n"
       "Connected\n"
       "Static routes\n"
       "Open Shortest Path First (OSPF)\n"
       "Border Gateway Protocol (BGP)\n"
       "Metric\n"
       "Metric value\n")
{
  int i;
  int metric;

  metric = atoi (argv[1]);

  for (i = 0; redist_type[i].str; i++) {
    if (strncmp(redist_type[i].str, argv[0],
		redist_type[i].str_min_len) == 0) 
      {
	rip_redistribute_metric_set (redist_type[i].type, metric);
	zclient_redistribute_set (zclient, redist_type[i].type);
	return CMD_SUCCESS;
      }
  }

  vty_out(vty, "Invalid type %s%s", argv[0],
	  VTY_NEWLINE);

  return CMD_WARNING;
}

DEFUN (no_rip_redistribute_type_metric,
       no_rip_redistribute_type_metric_cmd,
       "no redistribute (kernel|connected|static|ospf|bgp) metric <0-16>",
       NO_STR
       "Redistribute information from another routing protocol\n"
       "Kernel routes\n"
       "Connected\n"
       "Static routes\n"
       "Open Shortest Path First (OSPF)\n"
       "Border Gateway Protocol (BGP)\n"
       "Metric\n"
       "Metric value\n")
{
  int i;

  for (i = 0; redist_type[i].str; i++) 
    {
      if (strncmp(redist_type[i].str, argv[0], 
		  redist_type[i].str_min_len) == 0) 
	{
	  if (rip_metric_unset (redist_type[i].type, atoi(argv[1])))
	    return CMD_WARNING;
	  rip_redistribute_unset (redist_type[i].type);
	  return CMD_SUCCESS;
        }
    }

  vty_out(vty, "Invalid type %s%s", argv[0],
	  VTY_NEWLINE);

  return CMD_WARNING;
}

DEFUN (no_rip_redistribute_type_metric_routemap,
       no_rip_redistribute_type_metric_routemap_cmd,
       "no redistribute (kernel|connected|static|ospf|bgp) metric <0-16> route-map WORD",
       NO_STR
       "Redistribute information from another routing protocol\n"
       "Kernel routes\n"
       "Connected\n"
       "Static routes\n"
       "Open Shortest Path First (OSPF)\n"
       "Border Gateway Protocol (BGP)\n"
       "Metric\n"
       "Metric value\n"
       "Route map reference\n"
       "Pointer to route-map entries\n")
{
  int i;

  for (i = 0; redist_type[i].str; i++) 
    {
      if (strncmp(redist_type[i].str, argv[0], 
		  redist_type[i].str_min_len) == 0) 
	{
	  if (rip_metric_unset (redist_type[i].type, atoi(argv[1])))
	    return CMD_WARNING;
	  if (rip_routemap_unset (redist_type[i].type, argv[2]))
	    {
	      rip_redistribute_metric_set(redist_type[i].type, atoi(argv[1]));   
	      return CMD_WARNING;
	    }
	  rip_redistribute_unset (redist_type[i].type);
	  return CMD_SUCCESS;
        }
    }

  vty_out(vty, "Invalid type %s%s", argv[0],
	  VTY_NEWLINE);

  return CMD_WARNING;
}

/* Default information originate. */

DEFUN (rip_default_information_originate,
       rip_default_information_originate_cmd,
       "default-information originate",
       "Control distribution of default route\n"
       "Distribute a default route\n")
{
  struct prefix_ipv4 p;

  if (! rip->default_information)
    {
      memset (&p, 0, sizeof (struct prefix_ipv4));
      p.family = AF_INET;

      rip->default_information = 1;
  
      rip_redistribute_add (ZEBRA_ROUTE_RIP, RIP_ROUTE_STATIC, &p, 0, NULL);
    }

  return CMD_SUCCESS;
}

DEFUN (no_rip_default_information_originate,
       no_rip_default_information_originate_cmd,
       "no default-information originate",
       NO_STR
       "Control distribution of default route\n"
       "Distribute a default route\n")
{
  struct prefix_ipv4 p;

  if (rip->default_information)
    {
      memset (&p, 0, sizeof (struct prefix_ipv4));
      p.family = AF_INET;

      rip->default_information = 0;
  
      rip_redistribute_delete (ZEBRA_ROUTE_RIP, RIP_ROUTE_STATIC, &p, 0);
    }

  return CMD_SUCCESS;
}

/* RIP configuration write function. */
int
config_write_zebra (struct vty *vty)
{
  if (! zclient->enable)
    {
      vty_out (vty, "no router zebra%s", VTY_NEWLINE);
      return 1;
    }
  else if (! zclient->redist[ZEBRA_ROUTE_RIP])
    {
      vty_out (vty, "router zebra%s", VTY_NEWLINE);
      vty_out (vty, " no redistribute rip%s", VTY_NEWLINE);
      return 1;
    }
  return 0;
}

int
config_write_rip_redistribute (struct vty *vty, int config_mode)
{
  int i;
  char *str[] = { "system", "kernel", "connected", "static", "rip",
		  "ripng", "ospf", "ospf6", "bgp"};

  for (i = 0; i < ZEBRA_ROUTE_MAX; i++)
    if (i != zclient->redist_default && zclient->redist[i])
      {
	if (config_mode)
	  {
	    if (rip->route_map[i].metric_config)
	      {
		if (rip->route_map[i].name)
		  vty_out (vty, " redistribute %s metric %d route-map %s%s",
			   str[i], rip->route_map[i].metric,
			   rip->route_map[i].name,
			   VTY_NEWLINE);
		else
		  vty_out (vty, " redistribute %s metric %d%s",
			   str[i], rip->route_map[i].metric,
			   VTY_NEWLINE);
	      }
	    else
	      {
		if (rip->route_map[i].name)
		  vty_out (vty, " redistribute %s route-map %s%s",
			   str[i], rip->route_map[i].name,
			   VTY_NEWLINE);
		else
		  vty_out (vty, " redistribute %s%s", str[i],
			   VTY_NEWLINE);
	      }
	  }
	else
	  vty_out (vty, " %s", str[i]);
      }
  return 0;
}

/* Zebra node structure. */
struct cmd_node zebra_node =
{
  ZEBRA_NODE,
  "%s(config-router)# ",
};

void
rip_zclient_init ()
{
  /* Set default value to the zebra client structure. */
  zclient = zclient_new ();
  zclient_init (zclient, ZEBRA_ROUTE_RIP);
  zclient->interface_add = rip_interface_add;
  zclient->interface_delete = rip_interface_delete;
  zclient->interface_address_add = rip_interface_address_add;
  zclient->interface_address_delete = rip_interface_address_delete;
  zclient->ipv4_route_add = rip_zebra_read_ipv4;
  zclient->ipv4_route_delete = rip_zebra_read_ipv4;
  zclient->interface_up = rip_interface_up;
  zclient->interface_down = rip_interface_down;
  
  /* Install zebra node. */
  install_node (&zebra_node, config_write_zebra);

  /* Install command elements to zebra node. */ 
  install_element (CONFIG_NODE, &router_zebra_cmd);
  install_element (CONFIG_NODE, &no_router_zebra_cmd);
  install_default (ZEBRA_NODE);
  install_element (ZEBRA_NODE, &rip_redistribute_rip_cmd);
  install_element (ZEBRA_NODE, &no_rip_redistribute_rip_cmd);

  /* Install command elements to rip node. */
  install_element (RIP_NODE, &rip_redistribute_type_cmd);
  install_element (RIP_NODE, &rip_redistribute_type_routemap_cmd);
  install_element (RIP_NODE, &rip_redistribute_type_metric_cmd);
  install_element (RIP_NODE, &no_rip_redistribute_type_cmd);
  install_element (RIP_NODE, &no_rip_redistribute_type_routemap_cmd);
  install_element (RIP_NODE, &no_rip_redistribute_type_metric_cmd);
  install_element (RIP_NODE, &no_rip_redistribute_type_metric_routemap_cmd);
  install_element (RIP_NODE, &rip_default_information_originate_cmd);
  install_element (RIP_NODE, &no_rip_default_information_originate_cmd);
}
