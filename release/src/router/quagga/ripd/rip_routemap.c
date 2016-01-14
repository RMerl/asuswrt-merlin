/* RIPv2 routemap.
 * Copyright (C) 2005 6WIND <alain.ritoux@6wind.com>
 * Copyright (C) 1999 Kunihiro Ishiguro <kunihiro@zebra.org>
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
#include "routemap.h"
#include "command.h"
#include "filter.h"
#include "log.h"
#include "sockunion.h"		/* for inet_aton () */
#include "plist.h"

#include "ripd/ripd.h"

struct rip_metric_modifier
{
  enum 
  {
    metric_increment,
    metric_decrement,
    metric_absolute
  } type;

  u_char metric;
};

/* Add rip route map rule. */
static int
rip_route_match_add (struct vty *vty, struct route_map_index *index,
		     const char *command, const char *arg)
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
	case RMAP_COMPILE_ERROR:
	  vty_out (vty, "%% Argument is malformed.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	}
    }
  return CMD_SUCCESS;
}

/* Delete rip route map rule. */
static int
rip_route_match_delete (struct vty *vty, struct route_map_index *index,
			const char *command, const char *arg)
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
	case RMAP_COMPILE_ERROR:
	  vty_out (vty, "%% Argument is malformed.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	}
    }
  return CMD_SUCCESS;
}

/* Add rip route map rule. */
static int
rip_route_set_add (struct vty *vty, struct route_map_index *index,
		   const char *command, const char *arg)
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
	case RMAP_COMPILE_ERROR:
	  /* rip, ripng and other protocols share the set metric command
	     but only values from 0 to 16 are valid for rip and ripng
	     if metric is out of range for rip and ripng, it is not for
	     other protocols. Do not return an error */
	  if (strcmp(command, "metric")) {
	     vty_out (vty, "%% Argument is malformed.%s", VTY_NEWLINE);
	     return CMD_WARNING;
	  }
	}
    }
  return CMD_SUCCESS;
}

/* Delete rip route map rule. */
static int
rip_route_set_delete (struct vty *vty, struct route_map_index *index,
		      const char *command, const char *arg)
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
	case RMAP_COMPILE_ERROR:
	  vty_out (vty, "%% Argument is malformed.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	}
    }
  return CMD_SUCCESS;
}

/* Hook function for updating route_map assignment. */
/* ARGSUSED */
static void
rip_route_map_update (const char *notused)
{
  int i;

  if (rip) 
    {
      for (i = 0; i < ZEBRA_ROUTE_MAX; i++) 
	{
	  if (rip->route_map[i].name)
	    rip->route_map[i].map = 
	      route_map_lookup_by_name (rip->route_map[i].name);
	}
    }
}

/* `match metric METRIC' */
/* Match function return 1 if match is success else return zero. */
static route_map_result_t
route_match_metric (void *rule, struct prefix *prefix, 
		    route_map_object_t type, void *object)
{
  u_int32_t *metric;
  u_int32_t  check;
  struct rip_info *rinfo;

  if (type == RMAP_RIP)
    {
      metric = rule;
      rinfo = object;
    
      /* If external metric is available, the route-map should
         work on this one (for redistribute purpose)  */
      check = (rinfo->external_metric) ? rinfo->external_metric :
                                         rinfo->metric;
      if (check == *metric)
	return RMAP_MATCH;
      else
	return RMAP_NOMATCH;
    }
  return RMAP_NOMATCH;
}

/* Route map `match metric' match statement. `arg' is METRIC value */
static void *
route_match_metric_compile (const char *arg)
{
  u_int32_t *metric;

  metric = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (u_int32_t));
  *metric = atoi (arg);

  if(*metric > 0)
    return metric;

  XFREE (MTYPE_ROUTE_MAP_COMPILED, metric);
  return NULL;
}

/* Free route map's compiled `match metric' value. */
static void
route_match_metric_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for metric matching. */
struct route_map_rule_cmd route_match_metric_cmd =
{
  "metric",
  route_match_metric,
  route_match_metric_compile,
  route_match_metric_free
};

/* `match interface IFNAME' */
/* Match function return 1 if match is success else return zero. */
static route_map_result_t
route_match_interface (void *rule, struct prefix *prefix,
		       route_map_object_t type, void *object)
{
  struct rip_info *rinfo;
  struct interface *ifp;
  char *ifname;

  if (type == RMAP_RIP)
    {
      ifname = rule;
      ifp = if_lookup_by_name(ifname);

      if (!ifp)
	return RMAP_NOMATCH;

      rinfo = object;

      if (rinfo->ifindex_out == ifp->ifindex || rinfo->ifindex == ifp->ifindex)
	return RMAP_MATCH;
      else
	return RMAP_NOMATCH;
    }
  return RMAP_NOMATCH;
}

/* Route map `match interface' match statement. `arg' is IFNAME value */
/* XXX I don`t know if I need to check does interface exist? */
static void *
route_match_interface_compile (const char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

/* Free route map's compiled `match interface' value. */
static void
route_match_interface_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for interface matching. */
struct route_map_rule_cmd route_match_interface_cmd =
{
  "interface",
  route_match_interface,
  route_match_interface_compile,
  route_match_interface_free
};

/* `match ip next-hop IP_ACCESS_LIST' */

/* Match function return 1 if match is success else return zero. */
static route_map_result_t
route_match_ip_next_hop (void *rule, struct prefix *prefix,
			route_map_object_t type, void *object)
{
  struct access_list *alist;
  struct rip_info *rinfo;
  struct prefix_ipv4 p;

  if (type == RMAP_RIP)
    {
      rinfo = object;
      p.family = AF_INET;
      p.prefix = (rinfo->nexthop.s_addr) ? rinfo->nexthop : rinfo->from;
      p.prefixlen = IPV4_MAX_BITLEN;

      alist = access_list_lookup (AFI_IP, (char *) rule);
      if (alist == NULL)
	return RMAP_NOMATCH;

      return (access_list_apply (alist, &p) == FILTER_DENY ?
	      RMAP_NOMATCH : RMAP_MATCH);
    }
  return RMAP_NOMATCH;
}

/* Route map `ip next-hop' match statement.  `arg' should be
   access-list name. */
static void *
route_match_ip_next_hop_compile (const char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

/* Free route map's compiled `. */
static void
route_match_ip_next_hop_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for ip next-hop matching. */
static struct route_map_rule_cmd route_match_ip_next_hop_cmd =
{
  "ip next-hop",
  route_match_ip_next_hop,
  route_match_ip_next_hop_compile,
  route_match_ip_next_hop_free
};

/* `match ip next-hop prefix-list PREFIX_LIST' */

static route_map_result_t
route_match_ip_next_hop_prefix_list (void *rule, struct prefix *prefix,
                                    route_map_object_t type, void *object)
{
  struct prefix_list *plist;
  struct rip_info *rinfo;
  struct prefix_ipv4 p;

  if (type == RMAP_RIP)
    {
      rinfo = object;
      p.family = AF_INET;
      p.prefix = (rinfo->nexthop.s_addr) ? rinfo->nexthop : rinfo->from;
      p.prefixlen = IPV4_MAX_BITLEN;

      plist = prefix_list_lookup (AFI_IP, (char *) rule);
      if (plist == NULL)
        return RMAP_NOMATCH;

      return (prefix_list_apply (plist, &p) == PREFIX_DENY ?
              RMAP_NOMATCH : RMAP_MATCH);
    }
  return RMAP_NOMATCH;
}

static void *
route_match_ip_next_hop_prefix_list_compile (const char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

static void
route_match_ip_next_hop_prefix_list_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

static struct route_map_rule_cmd route_match_ip_next_hop_prefix_list_cmd =
{
  "ip next-hop prefix-list",
  route_match_ip_next_hop_prefix_list,
  route_match_ip_next_hop_prefix_list_compile,
  route_match_ip_next_hop_prefix_list_free
};

/* `match ip address IP_ACCESS_LIST' */

/* Match function should return 1 if match is success else return
   zero. */
static route_map_result_t
route_match_ip_address (void *rule, struct prefix *prefix, 
			route_map_object_t type, void *object)
{
  struct access_list *alist;

  if (type == RMAP_RIP)
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
static void *
route_match_ip_address_compile (const char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

/* Free route map's compiled `ip address' value. */
static void
route_match_ip_address_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for ip address matching. */
static struct route_map_rule_cmd route_match_ip_address_cmd =
{
  "ip address",
  route_match_ip_address,
  route_match_ip_address_compile,
  route_match_ip_address_free
};

/* `match ip address prefix-list PREFIX_LIST' */

static route_map_result_t
route_match_ip_address_prefix_list (void *rule, struct prefix *prefix, 
				    route_map_object_t type, void *object)
{
  struct prefix_list *plist;

  if (type == RMAP_RIP)
    {
      plist = prefix_list_lookup (AFI_IP, (char *) rule);
      if (plist == NULL)
	return RMAP_NOMATCH;
    
      return (prefix_list_apply (plist, prefix) == PREFIX_DENY ?
	      RMAP_NOMATCH : RMAP_MATCH);
    }
  return RMAP_NOMATCH;
}

static void *
route_match_ip_address_prefix_list_compile (const char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

static void
route_match_ip_address_prefix_list_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

static struct route_map_rule_cmd route_match_ip_address_prefix_list_cmd =
{
  "ip address prefix-list",
  route_match_ip_address_prefix_list,
  route_match_ip_address_prefix_list_compile,
  route_match_ip_address_prefix_list_free
};

/* `match tag TAG' */
/* Match function return 1 if match is success else return zero. */
static route_map_result_t
route_match_tag (void *rule, struct prefix *prefix, 
		    route_map_object_t type, void *object)
{
  u_short *tag;
  struct rip_info *rinfo;

  if (type == RMAP_RIP)
    {
      tag = rule;
      rinfo = object;

      /* The information stored by rinfo is host ordered. */
      if (rinfo->tag == *tag)
	return RMAP_MATCH;
      else
	return RMAP_NOMATCH;
    }
  return RMAP_NOMATCH;
}

/* Route map `match tag' match statement. `arg' is TAG value */
static void *
route_match_tag_compile (const char *arg)
{
  u_short *tag;

  tag = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (u_short));
  *tag = atoi (arg);

  return tag;
}

/* Free route map's compiled `match tag' value. */
static void
route_match_tag_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for tag matching. */
struct route_map_rule_cmd route_match_tag_cmd =
{
  "tag",
  route_match_tag,
  route_match_tag_compile,
  route_match_tag_free
};

/* `set metric METRIC' */

/* Set metric to attribute. */
static route_map_result_t
route_set_metric (void *rule, struct prefix *prefix, 
		  route_map_object_t type, void *object)
{
  if (type == RMAP_RIP)
    {
      struct rip_metric_modifier *mod;
      struct rip_info *rinfo;

      mod = rule;
      rinfo = object;

      if (mod->type == metric_increment)
	rinfo->metric_out += mod->metric;
      else if (mod->type == metric_decrement)
	rinfo->metric_out -= mod->metric;
      else if (mod->type == metric_absolute)
	rinfo->metric_out = mod->metric;

      if ((signed int)rinfo->metric_out < 1)
	rinfo->metric_out = 1;
      if (rinfo->metric_out > RIP_METRIC_INFINITY)
	rinfo->metric_out = RIP_METRIC_INFINITY;

      rinfo->metric_set = 1;
    }
  return RMAP_OKAY;
}

/* set metric compilation. */
static void *
route_set_metric_compile (const char *arg)
{
  int len;
  const char *pnt;
  int type;
  long metric;
  char *endptr = NULL;
  struct rip_metric_modifier *mod;

  len = strlen (arg);
  pnt = arg;

  if (len == 0)
    return NULL;

  /* Examine first character. */
  if (arg[0] == '+')
    {
      type = metric_increment;
      pnt++;
    }
  else if (arg[0] == '-')
    {
      type = metric_decrement;
      pnt++;
    }
  else
    type = metric_absolute;

  /* Check beginning with digit string. */
  if (*pnt < '0' || *pnt > '9')
    return NULL;

  /* Convert string to integer. */
  metric = strtol (pnt, &endptr, 10);

  if (metric == LONG_MAX || *endptr != '\0')
    return NULL;
  if (metric < 0 || metric > RIP_METRIC_INFINITY)
    return NULL;

  mod = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, 
		 sizeof (struct rip_metric_modifier));
  mod->type = type;
  mod->metric = metric;

  return mod;
}

/* Free route map's compiled `set metric' value. */
static void
route_set_metric_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Set metric rule structure. */
static struct route_map_rule_cmd route_set_metric_cmd = 
{
  "metric",
  route_set_metric,
  route_set_metric_compile,
  route_set_metric_free,
};

/* `set ip next-hop IP_ADDRESS' */

/* Set nexthop to object.  ojbect must be pointer to struct attr. */
static route_map_result_t
route_set_ip_nexthop (void *rule, struct prefix *prefix, 
		      route_map_object_t type, void *object)
{
  struct in_addr *address;
  struct rip_info *rinfo;

  if(type == RMAP_RIP)
    {
      /* Fetch routemap's rule information. */
      address = rule;
      rinfo = object;
    
      /* Set next hop value. */ 
      rinfo->nexthop_out = *address;
    }

  return RMAP_OKAY;
}

/* Route map `ip nexthop' compile function.  Given string is converted
   to struct in_addr structure. */
static void *
route_set_ip_nexthop_compile (const char *arg)
{
  int ret;
  struct in_addr *address;

  address = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (struct in_addr));

  ret = inet_aton (arg, address);

  if (ret == 0)
    {
      XFREE (MTYPE_ROUTE_MAP_COMPILED, address);
      return NULL;
    }

  return address;
}

/* Free route map's compiled `ip nexthop' value. */
static void
route_set_ip_nexthop_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for ip nexthop set. */
static struct route_map_rule_cmd route_set_ip_nexthop_cmd =
{
  "ip next-hop",
  route_set_ip_nexthop,
  route_set_ip_nexthop_compile,
  route_set_ip_nexthop_free
};

/* `set tag TAG' */

/* Set tag to object.  ojbect must be pointer to struct attr. */
static route_map_result_t
route_set_tag (void *rule, struct prefix *prefix, 
		      route_map_object_t type, void *object)
{
  u_short *tag;
  struct rip_info *rinfo;

  if(type == RMAP_RIP)
    {
      /* Fetch routemap's rule information. */
      tag = rule;
      rinfo = object;
    
      /* Set next hop value. */ 
      rinfo->tag_out = *tag;
    }

  return RMAP_OKAY;
}

/* Route map `tag' compile function.  Given string is converted
   to u_short. */
static void *
route_set_tag_compile (const char *arg)
{
  u_short *tag;

  tag = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, sizeof (u_short));
  *tag = atoi (arg);

  return tag;
}

/* Free route map's compiled `ip nexthop' value. */
static void
route_set_tag_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

/* Route map commands for tag set. */
static struct route_map_rule_cmd route_set_tag_cmd =
{
  "tag",
  route_set_tag,
  route_set_tag_compile,
  route_set_tag_free
};

#define MATCH_STR "Match values from routing table\n"
#define SET_STR "Set values in destination routing protocol\n"

DEFUN (match_metric, 
       match_metric_cmd,
       "match metric <0-4294967295>",
       MATCH_STR
       "Match metric of route\n"
       "Metric value\n")
{
  return rip_route_match_add (vty, vty->index, "metric", argv[0]);
}

DEFUN (no_match_metric,
       no_match_metric_cmd,
       "no match metric",
       NO_STR
       MATCH_STR
       "Match metric of route\n")
{
  if (argc == 0)
    return rip_route_match_delete (vty, vty->index, "metric", NULL);

  return rip_route_match_delete (vty, vty->index, "metric", argv[0]);
}

ALIAS (no_match_metric,
       no_match_metric_val_cmd,
       "no match metric <0-4294967295>",
       NO_STR
       MATCH_STR
       "Match metric of route\n"
       "Metric value\n")

DEFUN (match_interface,
       match_interface_cmd,
       "match interface WORD",
       MATCH_STR
       "Match first hop interface of route\n"
       "Interface name\n")
{
  return rip_route_match_add (vty, vty->index, "interface", argv[0]);
}

DEFUN (no_match_interface,
       no_match_interface_cmd,
       "no match interface",
       NO_STR
       MATCH_STR
       "Match first hop interface of route\n")
{
  if (argc == 0)
    return rip_route_match_delete (vty, vty->index, "interface", NULL);

  return rip_route_match_delete (vty, vty->index, "interface", argv[0]);
}

ALIAS (no_match_interface,
       no_match_interface_val_cmd,
       "no match interface WORD",
       NO_STR
       MATCH_STR
       "Match first hop interface of route\n"
       "Interface name\n")

DEFUN (match_ip_next_hop,
       match_ip_next_hop_cmd,
       "match ip next-hop (<1-199>|<1300-2699>|WORD)",
       MATCH_STR
       IP_STR
       "Match next-hop address of route\n"
       "IP access-list number\n"
       "IP access-list number (expanded range)\n"
       "IP Access-list name\n")
{
  return rip_route_match_add (vty, vty->index, "ip next-hop", argv[0]);
}

DEFUN (no_match_ip_next_hop,
       no_match_ip_next_hop_cmd,
       "no match ip next-hop",
       NO_STR
       MATCH_STR
       IP_STR
       "Match next-hop address of route\n")
{
  if (argc == 0)
    return rip_route_match_delete (vty, vty->index, "ip next-hop", NULL);

  return rip_route_match_delete (vty, vty->index, "ip next-hop", argv[0]);
}

ALIAS (no_match_ip_next_hop,
       no_match_ip_next_hop_val_cmd,
       "no match ip next-hop (<1-199>|<1300-2699>|WORD)",
       NO_STR
       MATCH_STR
       IP_STR
       "Match next-hop address of route\n"
       "IP access-list number\n"
       "IP access-list number (expanded range)\n"
       "IP Access-list name\n")

DEFUN (match_ip_next_hop_prefix_list,
       match_ip_next_hop_prefix_list_cmd,
       "match ip next-hop prefix-list WORD",
       MATCH_STR
       IP_STR
       "Match next-hop address of route\n"
       "Match entries of prefix-lists\n"
       "IP prefix-list name\n")
{
  return rip_route_match_add (vty, vty->index, "ip next-hop prefix-list", argv[0]);
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
    return rip_route_match_delete (vty, vty->index, "ip next-hop prefix-list", NULL);

  return rip_route_match_delete (vty, vty->index, "ip next-hop prefix-list", argv[0]);
}

ALIAS (no_match_ip_next_hop_prefix_list,
       no_match_ip_next_hop_prefix_list_val_cmd,
       "no match ip next-hop prefix-list WORD",
       NO_STR
       MATCH_STR
       IP_STR
       "Match next-hop address of route\n"
       "Match entries of prefix-lists\n"
       "IP prefix-list name\n")

DEFUN (match_ip_address,
       match_ip_address_cmd,
       "match ip address (<1-199>|<1300-2699>|WORD)",
       MATCH_STR
       IP_STR
       "Match address of route\n"
       "IP access-list number\n"
       "IP access-list number (expanded range)\n"
       "IP Access-list name\n")

{
  return rip_route_match_add (vty, vty->index, "ip address", argv[0]);
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
    return rip_route_match_delete (vty, vty->index, "ip address", NULL);

  return rip_route_match_delete (vty, vty->index, "ip address", argv[0]);
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
       "IP Access-list name\n")

DEFUN (match_ip_address_prefix_list, 
       match_ip_address_prefix_list_cmd,
       "match ip address prefix-list WORD",
       MATCH_STR
       IP_STR
       "Match address of route\n"
       "Match entries of prefix-lists\n"
       "IP prefix-list name\n")
{
  return rip_route_match_add (vty, vty->index, "ip address prefix-list", argv[0]);
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
    return rip_route_match_delete (vty, vty->index, "ip address prefix-list", NULL);

  return rip_route_match_delete (vty, vty->index, "ip address prefix-list", argv[0]);
}

ALIAS (no_match_ip_address_prefix_list,
       no_match_ip_address_prefix_list_val_cmd,
       "no match ip address prefix-list WORD",
       NO_STR
       MATCH_STR
       IP_STR
       "Match address of route\n"
       "Match entries of prefix-lists\n"
       "IP prefix-list name\n")

DEFUN (match_tag, 
       match_tag_cmd,
       "match tag <0-65535>",
       MATCH_STR
       "Match tag of route\n"
       "Metric value\n")
{
  return rip_route_match_add (vty, vty->index, "tag", argv[0]);
}

DEFUN (no_match_tag,
       no_match_tag_cmd,
       "no match tag",
       NO_STR
       MATCH_STR
       "Match tag of route\n")
{
  if (argc == 0)
    return rip_route_match_delete (vty, vty->index, "tag", NULL);

  return rip_route_match_delete (vty, vty->index, "tag", argv[0]);
}

ALIAS (no_match_tag,
       no_match_tag_val_cmd,
       "no match tag <0-65535>",
       NO_STR
       MATCH_STR
       "Match tag of route\n"
       "Metric value\n")

/* set functions */

DEFUN (set_metric,
       set_metric_cmd,
       "set metric <0-4294967295>",
       SET_STR
       "Metric value for destination routing protocol\n"
       "Metric value\n")
{
  return rip_route_set_add (vty, vty->index, "metric", argv[0]);
}

ALIAS (set_metric,
       set_metric_addsub_cmd,
       "set metric <+/-metric>",
       SET_STR
       "Metric value for destination routing protocol\n"
       "Add or subtract metric\n")

DEFUN (no_set_metric,
       no_set_metric_cmd,
       "no set metric",
       NO_STR
       SET_STR
       "Metric value for destination routing protocol\n")
{
  if (argc == 0)
    return rip_route_set_delete (vty, vty->index, "metric", NULL);

  return rip_route_set_delete (vty, vty->index, "metric", argv[0]);
}

ALIAS (no_set_metric,
       no_set_metric_val_cmd,
       "no set metric (<0-4294967295>|<+/-metric>)",
       NO_STR
       SET_STR
       "Metric value for destination routing protocol\n"
       "Metric value\n"
       "Add or subtract metric\n")

DEFUN (set_ip_nexthop,
       set_ip_nexthop_cmd,
       "set ip next-hop A.B.C.D",
       SET_STR
       IP_STR
       "Next hop address\n"
       "IP address of next hop\n")
{
  union sockunion su;
  int ret;

  ret = str2sockunion (argv[0], &su);
  if (ret < 0)
    {
      vty_out (vty, "%% Malformed next-hop address%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  return rip_route_set_add (vty, vty->index, "ip next-hop", argv[0]);
}

DEFUN (no_set_ip_nexthop,
       no_set_ip_nexthop_cmd,
       "no set ip next-hop",
       NO_STR
       SET_STR
       IP_STR
       "Next hop address\n")
{
  if (argc == 0)
    return rip_route_set_delete (vty, vty->index, "ip next-hop", NULL);
  
  return rip_route_set_delete (vty, vty->index, "ip next-hop", argv[0]);
}

ALIAS (no_set_ip_nexthop,
       no_set_ip_nexthop_val_cmd,
       "no set ip next-hop A.B.C.D",
       NO_STR
       SET_STR
       IP_STR
       "Next hop address\n"
       "IP address of next hop\n")

DEFUN (set_tag,
       set_tag_cmd,
       "set tag <0-65535>",
       SET_STR
       "Tag value for routing protocol\n"
       "Tag value\n")
{
  return rip_route_set_add (vty, vty->index, "tag", argv[0]);
}

DEFUN (no_set_tag,
       no_set_tag_cmd,
       "no set tag",
       NO_STR
       SET_STR
       "Tag value for routing protocol\n")
{
  if (argc == 0)
    return rip_route_set_delete (vty, vty->index, "tag", NULL);
  
  return rip_route_set_delete (vty, vty->index, "tag", argv[0]);
}

ALIAS (no_set_tag,
       no_set_tag_val_cmd,
       "no set tag <0-65535>",
       NO_STR
       SET_STR
       "Tag value for routing protocol\n"
       "Tag value\n")

void
rip_route_map_reset ()
{
  ;
}

/* Route-map init */
void
rip_route_map_init ()
{
  route_map_init ();
  route_map_init_vty ();
  route_map_add_hook (rip_route_map_update);
  route_map_delete_hook (rip_route_map_update);

  route_map_install_match (&route_match_metric_cmd);
  route_map_install_match (&route_match_interface_cmd);
  route_map_install_match (&route_match_ip_next_hop_cmd);
  route_map_install_match (&route_match_ip_next_hop_prefix_list_cmd);
  route_map_install_match (&route_match_ip_address_cmd);
  route_map_install_match (&route_match_ip_address_prefix_list_cmd);
  route_map_install_match (&route_match_tag_cmd);

  route_map_install_set (&route_set_metric_cmd);
  route_map_install_set (&route_set_ip_nexthop_cmd);
  route_map_install_set (&route_set_tag_cmd);

  install_element (RMAP_NODE, &match_metric_cmd);
  install_element (RMAP_NODE, &no_match_metric_cmd);
  install_element (RMAP_NODE, &no_match_metric_val_cmd);
  install_element (RMAP_NODE, &match_interface_cmd);
  install_element (RMAP_NODE, &no_match_interface_cmd);
  install_element (RMAP_NODE, &no_match_interface_val_cmd);
  install_element (RMAP_NODE, &match_ip_next_hop_cmd);
  install_element (RMAP_NODE, &no_match_ip_next_hop_cmd);
  install_element (RMAP_NODE, &no_match_ip_next_hop_val_cmd);
  install_element (RMAP_NODE, &match_ip_next_hop_prefix_list_cmd);
  install_element (RMAP_NODE, &no_match_ip_next_hop_prefix_list_cmd);
  install_element (RMAP_NODE, &no_match_ip_next_hop_prefix_list_val_cmd);
  install_element (RMAP_NODE, &match_ip_address_cmd);
  install_element (RMAP_NODE, &no_match_ip_address_cmd);
  install_element (RMAP_NODE, &no_match_ip_address_val_cmd);
  install_element (RMAP_NODE, &match_ip_address_prefix_list_cmd);
  install_element (RMAP_NODE, &no_match_ip_address_prefix_list_cmd);
  install_element (RMAP_NODE, &no_match_ip_address_prefix_list_val_cmd);
  install_element (RMAP_NODE, &match_tag_cmd);
  install_element (RMAP_NODE, &no_match_tag_cmd);
  install_element (RMAP_NODE, &no_match_tag_val_cmd);

  install_element (RMAP_NODE, &set_metric_cmd);
  install_element (RMAP_NODE, &set_metric_addsub_cmd);
  install_element (RMAP_NODE, &no_set_metric_cmd);
  install_element (RMAP_NODE, &no_set_metric_val_cmd);
  install_element (RMAP_NODE, &set_ip_nexthop_cmd);
  install_element (RMAP_NODE, &no_set_ip_nexthop_cmd);
  install_element (RMAP_NODE, &no_set_ip_nexthop_val_cmd);
  install_element (RMAP_NODE, &set_tag_cmd);
  install_element (RMAP_NODE, &no_set_tag_cmd);
  install_element (RMAP_NODE, &no_set_tag_val_cmd);
}
