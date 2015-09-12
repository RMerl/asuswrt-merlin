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
#include "table.h"
#include "stream.h"
#include "memory.h"
#include "routemap.h"
#include "zclient.h"
#include "log.h"
#include "ripd/ripd.h"
#include "ripd/rip_debug.h"
#include "ripd/rip_interface.h"

/* All information about zebra. */
struct zclient *zclient = NULL;

/* Send ECMP routes to zebra. */
static void
rip_zebra_ipv4_send (struct route_node *rp, u_char cmd)
{
  static struct in_addr **nexthops = NULL;
  static unsigned int nexthops_len = 0;

  struct list *list = (struct list *)rp->info;
  struct zapi_ipv4 api;
  struct listnode *listnode = NULL;
  struct rip_info *rinfo = NULL;
  int count = 0;

  if (zclient->redist[ZEBRA_ROUTE_RIP])
    {
      api.type = ZEBRA_ROUTE_RIP;
      api.flags = 0;
      api.message = 0;
      api.safi = SAFI_UNICAST;

      if (nexthops_len < listcount (list))
        {
          nexthops_len = listcount (list);
          nexthops = XREALLOC (MTYPE_TMP, nexthops,
                               nexthops_len * sizeof (struct in_addr *));
        }

      SET_FLAG (api.message, ZAPI_MESSAGE_NEXTHOP);
      for (ALL_LIST_ELEMENTS_RO (list, listnode, rinfo))
        {
          nexthops[count++] = &rinfo->nexthop;
          if (cmd == ZEBRA_IPV4_ROUTE_ADD)
            SET_FLAG (rinfo->flags, RIP_RTF_FIB);
          else
            UNSET_FLAG (rinfo->flags, RIP_RTF_FIB);
        }

      api.nexthop = nexthops;
      api.nexthop_num = count;
      api.ifindex_num = 0;

      rinfo = listgetdata (listhead (list));

      SET_FLAG (api.message, ZAPI_MESSAGE_METRIC);
      api.metric = rinfo->metric;

      if (rinfo->distance && rinfo->distance != ZEBRA_RIP_DISTANCE_DEFAULT)
        {
          SET_FLAG (api.message, ZAPI_MESSAGE_DISTANCE);
          api.distance = rinfo->distance;
        }

      zapi_ipv4_route (cmd, zclient,
                       (struct prefix_ipv4 *)&rp->p, &api);

      if (IS_RIP_DEBUG_ZEBRA)
        {
          if (rip->ecmp)
            zlog_debug ("%s: %s/%d nexthops %d",
                        (cmd == ZEBRA_IPV4_ROUTE_ADD) ? \
                            "Install into zebra" : "Delete from zebra",
                        inet_ntoa (rp->p.u.prefix4), rp->p.prefixlen, count);
          else
            zlog_debug ("%s: %s/%d",
                        (cmd == ZEBRA_IPV4_ROUTE_ADD) ? \
                            "Install into zebra" : "Delete from zebra",
                        inet_ntoa (rp->p.u.prefix4), rp->p.prefixlen);
        }

      rip_global_route_changes++;
    }
}

/* Add/update ECMP routes to zebra. */
void
rip_zebra_ipv4_add (struct route_node *rp)
{
  rip_zebra_ipv4_send (rp, ZEBRA_IPV4_ROUTE_ADD);
}

/* Delete ECMP routes from zebra. */
void
rip_zebra_ipv4_delete (struct route_node *rp)
{
  rip_zebra_ipv4_send (rp, ZEBRA_IPV4_ROUTE_DELETE);
}

/* Zebra route add and delete treatment. */
static int
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
  else
    api.distance = 255;
  if (CHECK_FLAG (api.message, ZAPI_MESSAGE_METRIC))
    api.metric = stream_getl (s);
  else
    api.metric = 0;

  /* Then fetch IPv4 prefixes. */
  if (command == ZEBRA_IPV4_ROUTE_ADD)
    rip_redistribute_add (api.type, RIP_ROUTE_REDISTRIBUTE, &p, ifindex, 
                          &nexthop, api.metric, api.distance);
  else 
    rip_redistribute_delete (api.type, RIP_ROUTE_REDISTRIBUTE, &p, ifindex);

  return 0;
}

void
rip_zclient_reset (void)
{
  zclient_reset (zclient);
}

/* RIP route-map set for redistribution */
static void
rip_routemap_set (int type, const char *name)
{
  if (rip->route_map[type].name)
    free(rip->route_map[type].name);

  rip->route_map[type].name = strdup (name);
  rip->route_map[type].map = route_map_lookup_by_name (name);
}

static void
rip_redistribute_metric_set (int type, unsigned int metric)
{
  rip->route_map[type].metric_config = 1;
  rip->route_map[type].metric = metric;
}

static int
rip_metric_unset (int type, unsigned int metric)
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
static int
rip_routemap_unset (int type, const char *name)
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
  const char *str;
} redist_type[] = {
  {ZEBRA_ROUTE_KERNEL,  1, "kernel"},
  {ZEBRA_ROUTE_CONNECT, 1, "connected"},
  {ZEBRA_ROUTE_STATIC,  1, "static"},
  {ZEBRA_ROUTE_OSPF,    1, "ospf"},
  {ZEBRA_ROUTE_BGP,     2, "bgp"},
  {ZEBRA_ROUTE_BABEL,   2, "babel"},
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

#if 0
static int
rip_redistribute_set (int type)
{
  if (zclient->redist[type])
    return CMD_SUCCESS;

  zclient->redist[type] = 1;

  if (zclient->sock > 0)
    zebra_redistribute_send (ZEBRA_REDISTRIBUTE_ADD, zclient, type);

  return CMD_SUCCESS;
}
#endif

static int
rip_redistribute_unset (int type)
{
  if (! zclient->redist[type])
    return CMD_SUCCESS;

  zclient->redist[type] = 0;

  if (zclient->sock > 0)
    zebra_redistribute_send (ZEBRA_REDISTRIBUTE_DELETE, zclient, type);

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
rip_redistribute_clean (void)
{
  int i;

  for (i = 0; redist_type[i].str; i++)
    {
      if (zclient->redist[redist_type[i].type])
	{
	  if (zclient->sock > 0)
	    zebra_redistribute_send (ZEBRA_REDISTRIBUTE_DELETE,
				     zclient, redist_type[i].type);

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
       "redistribute " QUAGGA_REDIST_STR_RIPD,
       REDIST_STR
       QUAGGA_REDIST_HELP_STR_RIPD)
{
  int i;

  for(i = 0; redist_type[i].str; i++) 
    {
      if (strncmp (redist_type[i].str, argv[0], 
		   redist_type[i].str_min_len) == 0) 
	{
	  zclient_redistribute (ZEBRA_REDISTRIBUTE_ADD, zclient, 
	                        redist_type[i].type);
	  return CMD_SUCCESS;
	}
    }

  vty_out(vty, "Invalid type %s%s", argv[0],
	  VTY_NEWLINE);

  return CMD_WARNING;
}

DEFUN (no_rip_redistribute_type,
       no_rip_redistribute_type_cmd,
       "no redistribute " QUAGGA_REDIST_STR_RIPD,
       NO_STR
       REDIST_STR
       QUAGGA_REDIST_HELP_STR_RIPD)
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
       "redistribute " QUAGGA_REDIST_STR_RIPD " route-map WORD",
       REDIST_STR
       QUAGGA_REDIST_HELP_STR_RIPD
       "Route map reference\n"
       "Pointer to route-map entries\n")
{
  int i;

  for (i = 0; redist_type[i].str; i++) {
    if (strncmp(redist_type[i].str, argv[0],
		redist_type[i].str_min_len) == 0) 
      {
	rip_routemap_set (redist_type[i].type, argv[1]);
	zclient_redistribute (ZEBRA_REDISTRIBUTE_ADD, zclient, redist_type[i].type);
	return CMD_SUCCESS;
      }
  }

  vty_out(vty, "Invalid type %s%s", argv[0],
	  VTY_NEWLINE);

  return CMD_WARNING;
}

DEFUN (no_rip_redistribute_type_routemap,
       no_rip_redistribute_type_routemap_cmd,
       "no redistribute " QUAGGA_REDIST_STR_RIPD " route-map WORD",
       NO_STR
       REDIST_STR
       QUAGGA_REDIST_HELP_STR_RIPD
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
       "redistribute " QUAGGA_REDIST_STR_RIPD " metric <0-16>",
       REDIST_STR
       QUAGGA_REDIST_HELP_STR_RIPD
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
	zclient_redistribute (ZEBRA_REDISTRIBUTE_ADD, zclient, redist_type[i].type);
	return CMD_SUCCESS;
      }
  }

  vty_out(vty, "Invalid type %s%s", argv[0],
	  VTY_NEWLINE);

  return CMD_WARNING;
}

DEFUN (no_rip_redistribute_type_metric,
       no_rip_redistribute_type_metric_cmd,
       "no redistribute " QUAGGA_REDIST_STR_RIPD " metric <0-16>",
       NO_STR
       REDIST_STR
       QUAGGA_REDIST_HELP_STR_RIPD
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

DEFUN (rip_redistribute_type_metric_routemap,
       rip_redistribute_type_metric_routemap_cmd,
       "redistribute " QUAGGA_REDIST_STR_RIPD " metric <0-16> route-map WORD",
       REDIST_STR
       QUAGGA_REDIST_HELP_STR_RIPD
       "Metric\n"
       "Metric value\n"
       "Route map reference\n"
       "Pointer to route-map entries\n")
{
  int i;
  int metric;

  metric = atoi (argv[1]);

  for (i = 0; redist_type[i].str; i++) {
    if (strncmp(redist_type[i].str, argv[0],
		redist_type[i].str_min_len) == 0) 
      {
	rip_redistribute_metric_set (redist_type[i].type, metric);
	rip_routemap_set (redist_type[i].type, argv[2]);
	zclient_redistribute (ZEBRA_REDISTRIBUTE_ADD, zclient, redist_type[i].type);
	return CMD_SUCCESS;
      }
  }

  vty_out(vty, "Invalid type %s%s", argv[0],
	  VTY_NEWLINE);

  return CMD_WARNING;
}


DEFUN (no_rip_redistribute_type_metric_routemap,
       no_rip_redistribute_type_metric_routemap_cmd,
       "no redistribute " QUAGGA_REDIST_STR_RIPD
       " metric <0-16> route-map WORD",
       NO_STR
       REDIST_STR
       QUAGGA_REDIST_HELP_STR_RIPD
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
  
      rip_redistribute_add (ZEBRA_ROUTE_RIP, RIP_ROUTE_DEFAULT, &p, 0, 
                            NULL, 0, 0);
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
  
      rip_redistribute_delete (ZEBRA_ROUTE_RIP, RIP_ROUTE_DEFAULT, &p, 0);
    }

  return CMD_SUCCESS;
}

/* RIP configuration write function. */
static int
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

  for (i = 0; i < ZEBRA_ROUTE_MAX; i++)
    if (i != zclient->redist_default && zclient->redist[i])
      {
	if (config_mode)
	  {
	    if (rip->route_map[i].metric_config)
	      {
		if (rip->route_map[i].name)
		  vty_out (vty, " redistribute %s metric %d route-map %s%s",
			   zebra_route_string(i), rip->route_map[i].metric,
			   rip->route_map[i].name,
			   VTY_NEWLINE);
		else
		  vty_out (vty, " redistribute %s metric %d%s",
			   zebra_route_string(i), rip->route_map[i].metric,
			   VTY_NEWLINE);
	      }
	    else
	      {
		if (rip->route_map[i].name)
		  vty_out (vty, " redistribute %s route-map %s%s",
			   zebra_route_string(i), rip->route_map[i].name,
			   VTY_NEWLINE);
		else
		  vty_out (vty, " redistribute %s%s", zebra_route_string(i),
			   VTY_NEWLINE);
	      }
	  }
	else
	  vty_out (vty, " %s", zebra_route_string(i));
      }
  return 0;
}

/* Zebra node structure. */
static struct cmd_node zebra_node =
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
  install_element (RIP_NODE, &rip_redistribute_type_metric_routemap_cmd);
  install_element (RIP_NODE, &no_rip_redistribute_type_cmd);
  install_element (RIP_NODE, &no_rip_redistribute_type_routemap_cmd);
  install_element (RIP_NODE, &no_rip_redistribute_type_metric_cmd);
  install_element (RIP_NODE, &no_rip_redistribute_type_metric_routemap_cmd);
  install_element (RIP_NODE, &rip_default_information_originate_cmd);
  install_element (RIP_NODE, &no_rip_default_information_originate_cmd);
}
