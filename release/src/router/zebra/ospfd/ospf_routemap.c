/*
 * Route map function of ospfd.
 * Copyright (C) 2000 IP Infusion Inc.
 *
 * Written by Toshiaki Takada.
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

#include "memory.h"
#include "prefix.h"
#include "table.h"
#include "routemap.h"
#include "command.h"
#include "log.h"
#include "plist.h"

#include "ospfd/ospfd.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_interface.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_route.h"
#include "ospfd/ospf_zebra.h"

/* Hook function for updating route_map assignment. */
void
ospf_route_map_update (char *name)
{
  struct ospf *ospf;
  int type;

  /* If OSPF instatnce does not exist, return right now. */
  ospf = ospf_lookup ();
  if (ospf == NULL)
    return;

  /* Update route-map */
  for (type = 0; type <= ZEBRA_ROUTE_MAX; type++)
    {
      if (ROUTEMAP_NAME (ospf, type)
	  && strcmp (ROUTEMAP_NAME (ospf, type), name) == 0)
	{
	  /* Keep old route-map. */
	  struct route_map *old = ROUTEMAP (ospf, type);

	  /* Update route-map. */
	  ROUTEMAP (ospf, type) =
	    route_map_lookup_by_name (ROUTEMAP_NAME (ospf, type));

	  /* No update for this distribute type. */
	  if (old == NULL && ROUTEMAP (ospf, type) == NULL)
	    continue;

	  ospf_distribute_list_update (ospf, type);
	}
    }
}

void
ospf_route_map_event (route_map_event_t event, char *name)
{
  struct ospf *ospf;
  int type;

  /* If OSPF instatnce does not exist, return right now. */
  ospf = ospf_lookup ();
  if (ospf == NULL)
    return;

  /* Update route-map. */
  for (type = 0; type <= ZEBRA_ROUTE_MAX; type++)
    {
      if (ROUTEMAP_NAME (ospf, type) &&  ROUTEMAP (ospf, type)
	  && !strcmp (ROUTEMAP_NAME (ospf, type), name))
        {
          ospf_distribute_list_update (ospf, type);
        }
    }
}

/* Delete rip route map rule. */
int
ospf_route_match_delete (struct vty *vty, struct route_map_index *index,
			 char *command, char *arg)
{
  int ret;

  ret = route_map_delete_match (index, command, arg);
  if (ret)
    {
      switch (ret)
        {
        case RMAP_RULE_MISSING:
          vty_out (vty, "%% Can't find rule.%s", VTY_NEWLINE);
          return CMD_WARNING;
          break;
        case RMAP_COMPILE_ERROR:
          vty_out (vty, "%% Argument is malformed.%s", VTY_NEWLINE);
          return CMD_WARNING;
          break;
        }
    }

  return CMD_SUCCESS;
}

int
ospf_route_match_add (struct vty *vty, struct route_map_index *index,
		      char *command, char *arg)
{                                                                              
  int ret;

  ret = route_map_add_match (index, command, arg);
  if (ret)
    {
      switch (ret)
        {
        case RMAP_RULE_MISSING:
          vty_out (vty, "%% Can't find rule.%s", VTY_NEWLINE);
          return CMD_WARNING;
          break;
        case RMAP_COMPILE_ERROR:
          vty_out (vty, "%% Argument is malformed.%s", VTY_NEWLINE);
          return CMD_WARNING;
          break;
        }
    }

  return CMD_SUCCESS;
}

int
ospf_route_set_add (struct vty *vty, struct route_map_index *index,
		    char *command, char *arg)
{
  int ret;

  ret = route_map_add_set (index, command, arg);
  if (ret)
    {
      switch (ret)
        {
        case RMAP_RULE_MISSING:
          vty_out (vty, "%% Can't find rule.%s", VTY_NEWLINE);
          return CMD_WARNING;
          break;
        case RMAP_COMPILE_ERROR:
          vty_out (vty, "%% Argument is malformed.%s", VTY_NEWLINE);
          return CMD_WARNING;
          break;
        }
    }

  return CMD_SUCCESS;
}

/* Delete rip route map rule. */
int
ospf_route_set_delete (struct vty *vty, struct route_map_index *index,
		       char *command, char *arg)
{                                              
  int ret;

  ret = route_map_delete_set (index, command, arg);
  if (ret)
    {
      switch (ret)
        {
        case RMAP_RULE_MISSING:
          vty_out (vty, "%% Can't find rule.%s", VTY_NEWLINE);
          return CMD_WARNING;
          break;
        case RMAP_COMPILE_ERROR:
          vty_out (vty, "%% Argument is malformed.%s", VTY_NEWLINE);
          return CMD_WARNING;
          break;
        }
    }

  return CMD_SUCCESS;
}

/* `match ip netxthop ' */
/* Match function return 1 if match is success else return zero. */
route_map_result_t
route_match_ip_nexthop (void *rule, struct prefix *prefix,
			route_map_object_t type, void *object)
{
  struct access_list *alist;
  struct external_info *ei = object;
  struct prefix_ipv4 p;

  if (type == RMAP_OSPF)
    {
      p.family = AF_INET;
      p.prefix = ei->nexthop;
      p.prefixlen = IPV4_MAX_BITLEN;

      alist = access_list_lookup (AFI_IP, (char *) rule);
      if (alist == NULL)
        return RMAP_NOMATCH;

      return (access_list_apply (alist, &p) == FILTER_DENY ?
              RMAP_NOMATCH : RMAP_MATCH);
    }
  return RMAP_NOMATCH;
}

/* Route map `ip next-hop' match statement. `arg' should be
   access-list name. */
void *
route_match_ip_nexthop_compile (char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

/* Free route map's compiled `ip address' value. */
void
route_match_ip_nexthop_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for metric matching. */
struct route_map_rule_cmd route_match_ip_nexthop_cmd =
{
  "ip next-hop",
  route_match_ip_nexthop,
  route_match_ip_nexthop_compile,
  route_match_ip_nexthop_free
};

/* `match ip next-hop prefix-list PREFIX_LIST' */

route_map_result_t
route_match_ip_next_hop_prefix_list (void *rule, struct prefix *prefix,
                                    route_map_object_t type, void *object)
{
  struct prefix_list *plist;
  struct external_info *ei = object;
  struct prefix_ipv4 p;

  if (type == RMAP_OSPF)
    {
      p.family = AF_INET;
      p.prefix = ei->nexthop;
      p.prefixlen = IPV4_MAX_BITLEN;

      plist = prefix_list_lookup (AFI_IP, (char *) rule);
      if (plist == NULL)
        return RMAP_NOMATCH;

      return (prefix_list_apply (plist, &p) == PREFIX_DENY ?
              RMAP_NOMATCH : RMAP_MATCH);
    }
  return RMAP_NOMATCH;
}

void *
route_match_ip_next_hop_prefix_list_compile (char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

void
route_match_ip_next_hop_prefix_list_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

struct route_map_rule_cmd route_match_ip_next_hop_prefix_list_cmd =
{
  "ip next-hop prefix-list",
  route_match_ip_next_hop_prefix_list,
  route_match_ip_next_hop_prefix_list_compile,
  route_match_ip_next_hop_prefix_list_free
};

/* `match ip address IP_ACCESS_LIST' */
/* Match function should return 1 if match is success else return
   zero. */
route_map_result_t
route_match_ip_address (void *rule, struct prefix *prefix,
                        route_map_object_t type, void *object)
{
  struct access_list *alist;
  /* struct prefix_ipv4 match; */

  if (type == RMAP_OSPF)
    {
      alist = access_list_lookup (AFI_IP, (char *) rule);
      if (alist == NULL)
        return RMAP_NOMATCH;

      return (access_list_apply (alist, prefix) == FILTER_DENY ?
              RMAP_NOMATCH : RMAP_MATCH);
    }
  return RMAP_NOMATCH;
}

/* Route map `ip address' match statement.  `arg' should be
   access-list name. */
void *
route_match_ip_address_compile (char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

/* Free route map's compiled `ip address' value. */
void
route_match_ip_address_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for ip address matching. */
struct route_map_rule_cmd route_match_ip_address_cmd =
{
  "ip address",
  route_match_ip_address,
  route_match_ip_address_compile,
  route_match_ip_address_free
};

/* `match ip address prefix-list PREFIX_LIST' */
route_map_result_t
route_match_ip_address_prefix_list (void *rule, struct prefix *prefix,
                                    route_map_object_t type, void *object)
{
  struct prefix_list *plist;

  if (type == RMAP_OSPF)
    {
      plist = prefix_list_lookup (AFI_IP, (char *) rule);
      if (plist == NULL)
        return RMAP_NOMATCH;

      return (prefix_list_apply (plist, prefix) == PREFIX_DENY ?
              RMAP_NOMATCH : RMAP_MATCH);
    }
  return RMAP_NOMATCH;
}

void *
route_match_ip_address_prefix_list_compile (char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

void
route_match_ip_address_prefix_list_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

struct route_map_rule_cmd route_match_ip_address_prefix_list_cmd =
{
  "ip address prefix-list",
  route_match_ip_address_prefix_list,
  route_match_ip_address_prefix_list_compile,
  route_match_ip_address_prefix_list_free
};

/* `match interface IFNAME' */
/* Match function should return 1 if match is success else return
   zero. */
route_map_result_t
route_match_interface (void *rule, struct prefix *prefix,
		       route_map_object_t type, void *object)
{
  struct interface *ifp;
  struct external_info *ei;

  if (type == RMAP_OSPF)
    {
      ei = object;
      ifp = if_lookup_by_name ((char *)rule);

      if (ifp == NULL || ifp->ifindex != ei->ifindex)
	return RMAP_NOMATCH;

      return RMAP_MATCH;
    }
  return RMAP_NOMATCH;
}

/* Route map `interface' match statement.  `arg' should be
   interface name. */
void *
route_match_interface_compile (char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

/* Free route map's compiled `interface' value. */
void
route_match_interface_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for ip address matching. */
struct route_map_rule_cmd route_match_interface_cmd =
{
  "interface",
  route_match_interface,
  route_match_interface_compile,
  route_match_interface_free
};

/* `set metric METRIC' */
/* Set metric to attribute. */
route_map_result_t
route_set_metric (void *rule, struct prefix *prefix,
                  route_map_object_t type, void *object)
{
  u_int32_t *metric;
  struct external_info *ei;

  if (type == RMAP_OSPF)
    {
      /* Fetch routemap's rule information. */
      metric = rule;
      ei = object;

      /* Set metric out value. */
      ei->route_map_set.metric = *metric;
    }
  return RMAP_OKAY;
}

/* set metric compilation. */
void *
route_set_metric_compile (char *arg)
{
  u_int32_t *metric;

  metric = XCALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (u_int32_t));
  *metric = atoi (arg);

  if (*metric >= 0)
    return metric;

  XFREE (MTYPE_ROUTE_MAP_COMPILED, metric);
  return NULL;
}

/* Free route map's compiled `set metric' value. */
void
route_set_metric_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Set metric rule structure. */
struct route_map_rule_cmd route_set_metric_cmd =
{
  "metric",
  route_set_metric,
  route_set_metric_compile,
  route_set_metric_free,
};

/* `set metric-type TYPE' */
/* Set metric-type to attribute. */
route_map_result_t
route_set_metric_type (void *rule, struct prefix *prefix,
		       route_map_object_t type, void *object)
{
  u_int32_t *metric_type;
  struct external_info *ei;

  if (type == RMAP_OSPF)
    {
      /* Fetch routemap's rule information. */
      metric_type = rule;
      ei = object;

      /* Set metric out value. */
      ei->route_map_set.metric_type = *metric_type;
    }
  return RMAP_OKAY;
}

/* set metric-type compilation. */
void *
route_set_metric_type_compile (char *arg)
{
  u_int32_t *metric_type;

  metric_type = XCALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (u_int32_t));
  if (strcmp (arg, "type-1") == 0)
    *metric_type = EXTERNAL_METRIC_TYPE_1;
  else if (strcmp (arg, "type-2") == 0)
    *metric_type = EXTERNAL_METRIC_TYPE_2;

  if (*metric_type == EXTERNAL_METRIC_TYPE_1 ||
      *metric_type == EXTERNAL_METRIC_TYPE_2)
    return metric_type;

  XFREE (MTYPE_ROUTE_MAP_COMPILED, metric_type);
  return NULL;
}

/* Free route map's compiled `set metric-type' value. */
void
route_set_metric_type_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Set metric rule structure. */
struct route_map_rule_cmd route_set_metric_type_cmd =
{
  "metric-type",
  route_set_metric_type,
  route_set_metric_type_compile,
  route_set_metric_type_free,
};

DEFUN (match_ip_nexthop,
       match_ip_nexthop_cmd,
       "match ip next-hop (<1-199>|<1300-2699>|WORD)",
       MATCH_STR
       IP_STR
       "Match next-hop address of route\n"
       "IP access-list number\n"
       "IP access-list number (expanded range)\n"
       "IP access-list name\n")
{
  return ospf_route_match_add (vty, vty->index, "ip next-hop", argv[0]);
}

DEFUN (no_match_ip_nexthop,
       no_match_ip_nexthop_cmd,
       "no match ip next-hop",
       NO_STR
       MATCH_STR
       IP_STR
       "Match next-hop address of route\n")
{
  if (argc == 0)
    return ospf_route_match_delete (vty, vty->index, "ip next-hop", NULL);

  return ospf_route_match_delete (vty, vty->index, "ip next-hop", argv[0]);
}

ALIAS (no_match_ip_nexthop,
       no_match_ip_nexthop_val_cmd,
       "no match ip next-hop (<1-199>|<1300-2699>|WORD)",
       NO_STR
       MATCH_STR
       IP_STR
       "Match next-hop address of route\n"
       "IP access-list number\n"
       "IP access-list number (expanded range)\n"
       "IP access-list name\n");

DEFUN (match_ip_next_hop_prefix_list,
       match_ip_next_hop_prefix_list_cmd,
       "match ip next-hop prefix-list WORD",
       MATCH_STR
       IP_STR
       "Match next-hop address of route\n"
       "Match entries of prefix-lists\n"
       "IP prefix-list name\n")
{
  return ospf_route_match_add (vty, vty->index, "ip next-hop prefix-list",
			       argv[0]);
}

DEFUN (no_match_ip_next_hop_prefix_list,
       no_match_ip_next_hop_prefix_list_cmd,
       "no match ip next-hop prefix-list",
       NO_STR
       MATCH_STR
       IP_STR
       "Match next-hop address of route\n"
       "Match entries of prefix-lists\n")
{
  if (argc == 0)
    return ospf_route_match_delete (vty, vty->index, "ip next-hop prefix-list",
				    NULL);
  return ospf_route_match_delete (vty, vty->index, "ip next-hop prefix-list",
				  argv[0]);
}

ALIAS (no_match_ip_next_hop_prefix_list,
       no_match_ip_next_hop_prefix_list_val_cmd,
       "no match ip next-hop prefix-list WORD",
       NO_STR
       MATCH_STR
       IP_STR
       "Match next-hop address of route\n"
       "Match entries of prefix-lists\n"
       "IP prefix-list name\n");

DEFUN (match_ip_address,
       match_ip_address_cmd,
       "match ip address (<1-199>|<1300-2699>|WORD)",
       MATCH_STR
       IP_STR
       "Match address of route\n"
       "IP access-list number\n"
       "IP access-list number (expanded range)\n"
       "IP access-list name\n")
{
  return ospf_route_match_add (vty, vty->index, "ip address", argv[0]);
}

DEFUN (no_match_ip_address,
       no_match_ip_address_cmd,
       "no match ip address",
       NO_STR
       MATCH_STR
       IP_STR
       "Match address of route\n")
{
  if (argc == 0)
    return ospf_route_match_delete (vty, vty->index, "ip address", NULL);

  return ospf_route_match_delete (vty, vty->index, "ip address", argv[0]);
}

ALIAS (no_match_ip_address,
       no_match_ip_address_val_cmd,
       "no match ip address (<1-199>|<1300-2699>|WORD)",
       NO_STR
       MATCH_STR
       IP_STR
       "Match address of route\n"
       "IP access-list number\n"
       "IP access-list number (expanded range)\n"
       "IP access-list name\n");

DEFUN (match_ip_address_prefix_list,
       match_ip_address_prefix_list_cmd,
       "match ip address prefix-list WORD",
       MATCH_STR
       IP_STR
       "Match address of route\n"
       "Match entries of prefix-lists\n"
       "IP prefix-list name\n")
{
  return ospf_route_match_add (vty, vty->index, "ip address prefix-list",
			       argv[0]);
}

DEFUN (no_match_ip_address_prefix_list,
       no_match_ip_address_prefix_list_cmd,
       "no match ip address prefix-list",
       NO_STR
       MATCH_STR
       IP_STR
       "Match address of route\n"
       "Match entries of prefix-lists\n")
{
  if (argc == 0)
    return ospf_route_match_delete (vty, vty->index, "ip address prefix-list",
				    NULL);
  return ospf_route_match_delete (vty, vty->index, "ip address prefix-list",
				  argv[0]);
}

ALIAS (no_match_ip_address_prefix_list,
       no_match_ip_address_prefix_list_val_cmd,
       "no match ip address prefix-list WORD",
       NO_STR
       MATCH_STR
       IP_STR
       "Match address of route\n"
       "Match entries of prefix-lists\n"
       "IP prefix-list name\n");

DEFUN (match_interface,
       match_interface_cmd,
       "match interface WORD",
       MATCH_STR
       "Match first hop interface of route\n"
       "Interface name\n")
{
  return ospf_route_match_add (vty, vty->index, "interface", argv[0]);
}

DEFUN (no_match_interface,
       no_match_interface_cmd,
       "no match interface",
       NO_STR
       MATCH_STR
       "Match first hop interface of route\n")
{
  if (argc == 0)
    return ospf_route_match_delete (vty, vty->index, "interface", NULL);

  return ospf_route_match_delete (vty, vty->index, "interface", argv[0]);
}

ALIAS (no_match_interface,
       no_match_interface_val_cmd,
       "no match interface WORD",
       NO_STR
       MATCH_STR
       "Match first hop interface of route\n"
       "Interface name\n");

DEFUN (set_metric,
       set_metric_cmd,
       "set metric <0-4294967295>",
       SET_STR
       "Metric value for destination routing protocol\n"
       "Metric value\n")
{
  return ospf_route_set_add (vty, vty->index, "metric", argv[0]);
}

DEFUN (no_set_metric,
       no_set_metric_cmd,
       "no set metric",
       NO_STR
       SET_STR
       "Metric value for destination routing protocol\n")
{
  if (argc == 0)
    return ospf_route_set_delete (vty, vty->index, "metric", NULL);

  return ospf_route_set_delete (vty, vty->index, "metric", argv[0]);
}

ALIAS (no_set_metric,
       no_set_metric_val_cmd,
       "no set metric <0-4294967295>",
       NO_STR
       SET_STR
       "Metric value for destination routing protocol\n"
       "Metric value\n");

DEFUN (set_metric_type,
       set_metric_type_cmd,
       "set metric-type (type-1|type-2)",
       SET_STR
       "Type of metric for destination routing protocol\n"
       "OSPF external type 1 metric\n"
       "OSPF external type 2 metric\n")
{
  if (strcmp (argv[0], "1") == 0)
    return ospf_route_set_add (vty, vty->index, "metric-type", "type-1");
  if (strcmp (argv[0], "2") == 0)
    return ospf_route_set_add (vty, vty->index, "metric-type", "type-2");

  return ospf_route_set_add (vty, vty->index, "metric-type", argv[0]);
}

DEFUN (no_set_metric_type,
       no_set_metric_type_cmd,
       "no set metric-type",
       NO_STR
       SET_STR
       "Type of metric for destination routing protocol\n")
{
  if (argc == 0)
    return ospf_route_set_delete (vty, vty->index, "metric-type", NULL);

  return ospf_route_set_delete (vty, vty->index, "metric-type", argv[0]);
}

ALIAS (no_set_metric_type,
       no_set_metric_type_val_cmd,
       "no set metric-type (type-1|type-2)",
       NO_STR
       SET_STR
       "Type of metric for destination routing protocol\n"
       "OSPF external type 1 metric\n"
       "OSPF external type 2 metric\n");

/* Route-map init */
void
ospf_route_map_init (void)
{
  route_map_init ();
  route_map_init_vty ();

  route_map_add_hook (ospf_route_map_update);
  route_map_delete_hook (ospf_route_map_update);
  route_map_event_hook (ospf_route_map_event);
  
  route_map_install_match (&route_match_ip_nexthop_cmd);
  route_map_install_match (&route_match_ip_next_hop_prefix_list_cmd);
  route_map_install_match (&route_match_ip_address_cmd);
  route_map_install_match (&route_match_ip_address_prefix_list_cmd);
  route_map_install_match (&route_match_interface_cmd);

  route_map_install_set (&route_set_metric_cmd);
  route_map_install_set (&route_set_metric_type_cmd);

  install_element (RMAP_NODE, &match_ip_nexthop_cmd);
  install_element (RMAP_NODE, &no_match_ip_nexthop_cmd);
  install_element (RMAP_NODE, &no_match_ip_nexthop_val_cmd);
  install_element (RMAP_NODE, &match_ip_next_hop_prefix_list_cmd);
  install_element (RMAP_NODE, &no_match_ip_next_hop_prefix_list_cmd);
  install_element (RMAP_NODE, &no_match_ip_next_hop_prefix_list_val_cmd);
  install_element (RMAP_NODE, &match_ip_address_cmd);
  install_element (RMAP_NODE, &no_match_ip_address_cmd);
  install_element (RMAP_NODE, &no_match_ip_address_val_cmd);
  install_element (RMAP_NODE, &match_ip_address_prefix_list_cmd);
  install_element (RMAP_NODE, &no_match_ip_address_prefix_list_cmd);
  install_element (RMAP_NODE, &no_match_ip_address_prefix_list_val_cmd);
  install_element (RMAP_NODE, &match_interface_cmd);
  install_element (RMAP_NODE, &no_match_interface_cmd);
  install_element (RMAP_NODE, &no_match_interface_val_cmd);

  install_element (RMAP_NODE, &set_metric_cmd);
  install_element (RMAP_NODE, &no_set_metric_cmd);
  install_element (RMAP_NODE, &no_set_metric_val_cmd);
  install_element (RMAP_NODE, &set_metric_type_cmd);
  install_element (RMAP_NODE, &no_set_metric_type_cmd);
  install_element (RMAP_NODE, &no_set_metric_type_val_cmd);
}
