/* RIPng routemap.
 * Copyright (C) 1999 Kunihiro Ishiguro
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

#include "if.h"
#include "memory.h"
#include "prefix.h"
#include "routemap.h"
#include "command.h"

#include "ripngd/ripngd.h"

#if 0
/* `match interface IFNAME' */
route_map_result_t
route_match_interface (void *rule, struct prefix *prefix,
		       route_map_object_t type, void *object)
{
  struct ripng_info *rinfo;
  struct interface *ifp;
  char *ifname;

  if (type == ROUTE_MAP_RIPNG)
    {
      ifname = rule;
      ifp = if_lookup_by_name(ifname);

      if (!ifp)
	return RM_NOMATCH;

      rinfo = object;

      if (rinfo->ifindex == ifp->ifindex)
	return RM_MATCH;
      else
	return RM_NOMATCH;
    }
  return RM_NOMATCH;
}

void *
route_match_interface_compile (char *arg)
{
  return XSTRDUP (MTYPE_ROUTE_MAP_COMPILED, arg);
}

void
route_match_interface_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

struct route_map_rule_cmd route_match_interface_cmd =
{
  "interface",
  route_match_interface,
  route_match_interface_compile,
  route_match_interface_free
};
#endif /* 0 */

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

route_map_result_t
route_set_metric (void *rule, struct prefix *prefix, 
		  route_map_object_t type, void *object)
{
  if (type == RMAP_RIPNG)
    {
      struct rip_metric_modifier *mod;
      struct ripng_info *rinfo;

      mod = rule;
      rinfo = object;

      if (mod->type == metric_increment)
	rinfo->metric += mod->metric;
      else if (mod->type == metric_decrement)
	rinfo->metric -= mod->metric;
      else if (mod->type == metric_absolute)
	rinfo->metric = mod->metric;

      if (rinfo->metric < 1)
	rinfo->metric = 1;
      if (rinfo->metric > RIPNG_METRIC_INFINITY)
	rinfo->metric = RIPNG_METRIC_INFINITY;

      rinfo->metric_set = 1;
    }
  return RMAP_OKAY;
}

void *
route_set_metric_compile (char *arg)
{
  int len;
  char *pnt;
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
  /* Commented out by Hasso Tepper, to avoid problems in vtysh. */
  /* if (metric < 0 || metric > RIPNG_METRIC_INFINITY) */
  if (metric < 0)
    return NULL;

  mod = XMALLOC (MTYPE_ROUTE_MAP_COMPILED, 
		 sizeof (struct rip_metric_modifier));
  mod->type = type;
  mod->metric = metric;

  return mod;
}

void
route_set_metric_free (void *rule)
{
  XFREE (MTYPE_ROUTE_MAP_COMPILED, rule);
}

struct route_map_rule_cmd route_set_metric_cmd = 
{
  "metric",
  route_set_metric,
  route_set_metric_compile,
  route_set_metric_free,
};

int
ripng_route_match_add (struct vty *vty, struct route_map_index *index,
		       char *command, char *arg)
{
  int ret;

  ret = route_map_add_match (index, command, arg);
  if (ret)
    {
      switch (ret)
	{
	case RMAP_RULE_MISSING:
	  vty_out (vty, "Can't find rule.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	  break;
	case RMAP_COMPILE_ERROR:
	  vty_out (vty, "Argument is malformed.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	  break;
	}
    }
  return CMD_SUCCESS;
}

int
ripng_route_match_delete (struct vty *vty, struct route_map_index *index,
			  char *command, char *arg)
{
  int ret;

  ret = route_map_delete_match (index, command, arg);
  if (ret)
    {
      switch (ret)
	{
	case RMAP_RULE_MISSING:
	  vty_out (vty, "Can't find rule.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	  break;
	case RMAP_COMPILE_ERROR:
	  vty_out (vty, "Argument is malformed.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	  break;
	}
    }
  return CMD_SUCCESS;
}

int
ripng_route_set_add (struct vty *vty, struct route_map_index *index,
		     char *command, char *arg)
{
  int ret;

  ret = route_map_add_set (index, command, arg);
  if (ret)
    {
      switch (ret)
	{
	case RMAP_RULE_MISSING:
	  vty_out (vty, "Can't find rule.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	  break;
	case RMAP_COMPILE_ERROR:
	  vty_out (vty, "Argument is malformed.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	  break;
	}
    }
  return CMD_SUCCESS;
}

int
ripng_route_set_delete (struct vty *vty, struct route_map_index *index,
			char *command, char *arg)
{
  int ret;

  ret = route_map_delete_set (index, command, arg);
  if (ret)
    {
      switch (ret)
	{
	case RMAP_RULE_MISSING:
	  vty_out (vty, "Can't find rule.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	  break;
	case RMAP_COMPILE_ERROR:
	  vty_out (vty, "Argument is malformed.%s", VTY_NEWLINE);
	  return CMD_WARNING;
	  break;
	}
    }
  return CMD_SUCCESS;
}

#if 0
DEFUN (match_interface,
       match_interface_cmd,
       "match interface WORD",
       "Match value\n"
       "Interface\n"
       "Interface name\n")
{
  return ripng_route_match_add (vty, vty->index, "interface", argv[0]);
}

DEFUN (no_match_interface,
       no_match_interface_cmd,
       "no match interface WORD",
       NO_STR
       "Match value\n"
       "Interface\n"
       "Interface name\n")
{
  return ripng_route_match_delete (vty, vty->index, "interface", argv[0]);
}
#endif /* 0 */

DEFUN (set_metric,
       set_metric_cmd,
       "set metric <0-4294967295>",
       "Set value\n"
       "Metric\n"
       "METRIC value\n")
{
  return ripng_route_set_add (vty, vty->index, "metric", argv[0]);
}

DEFUN (no_set_metric,
       no_set_metric_cmd,
       "no set metric",
       NO_STR
       SET_STR
       "Metric value for destination routing protocol\n")
{
  if (argc == 0)
    return ripng_route_set_delete (vty, vty->index, "metric", NULL);

  return ripng_route_set_delete (vty, vty->index, "metric", argv[0]);
}

ALIAS (no_set_metric,
       no_set_metric_val_cmd,
       "no set metric <0-4294967295>",
       NO_STR
       SET_STR
       "Metric value for destination routing protocol\n"
       "Metric value\n");

void
ripng_route_map_init ()
{
  route_map_init ();
  route_map_init_vty ();

  /* route_map_install_match (&route_match_interface_cmd); */
  route_map_install_set (&route_set_metric_cmd);

  /*
  install_element (RMAP_NODE, &match_interface_cmd);
  install_element (RMAP_NODE, &no_match_interface_cmd);
  */

  install_element (RMAP_NODE, &set_metric_cmd);
  install_element (RMAP_NODE, &no_set_metric_cmd);
}
