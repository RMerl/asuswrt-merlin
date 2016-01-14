/* RIP offset-list
 * Copyright (C) 2000 Kunihiro Ishiguro <kunihiro@zebra.org>
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
#include "prefix.h"
#include "filter.h"
#include "command.h"
#include "linklist.h"
#include "memory.h"

#include "ripd/ripd.h"

#define RIP_OFFSET_LIST_IN  0
#define RIP_OFFSET_LIST_OUT 1
#define RIP_OFFSET_LIST_MAX 2

struct rip_offset_list
{
  char *ifname;

  struct 
  {
    char *alist_name;
    /* struct access_list *alist; */
    int metric;
  } direct[RIP_OFFSET_LIST_MAX];
};

static struct list *rip_offset_list_master;

static int
strcmp_safe (const char *s1, const char *s2)
{
  if (s1 == NULL && s2 == NULL)
    return 0;
  if (s1 == NULL)
    return -1;
  if (s2 == NULL)
    return 1;
  return strcmp (s1, s2);
}

static struct rip_offset_list *
rip_offset_list_new (void)
{
  return XCALLOC (MTYPE_RIP_OFFSET_LIST, sizeof (struct rip_offset_list));
}

static void
rip_offset_list_free (struct rip_offset_list *offset)
{
  XFREE (MTYPE_RIP_OFFSET_LIST, offset);
}

static struct rip_offset_list *
rip_offset_list_lookup (const char *ifname)
{
  struct rip_offset_list *offset;
  struct listnode *node, *nnode;

  for (ALL_LIST_ELEMENTS (rip_offset_list_master, node, nnode, offset))
    {
      if (strcmp_safe (offset->ifname, ifname) == 0)
	return offset;
    }
  return NULL;
}

static struct rip_offset_list *
rip_offset_list_get (const char *ifname)
{
  struct rip_offset_list *offset;
  
  offset = rip_offset_list_lookup (ifname);
  if (offset)
    return offset;

  offset = rip_offset_list_new ();
  if (ifname)
    offset->ifname = strdup (ifname);
  listnode_add_sort (rip_offset_list_master, offset);

  return offset;
}

static int
rip_offset_list_set (struct vty *vty, const char *alist, const char *direct_str,
		     const char *metric_str, const char *ifname)
{
  int direct;
  int metric;
  struct rip_offset_list *offset;

  /* Check direction. */
  if (strncmp (direct_str, "i", 1) == 0)
    direct = RIP_OFFSET_LIST_IN;
  else if (strncmp (direct_str, "o", 1) == 0)
    direct = RIP_OFFSET_LIST_OUT;
  else
    {
      vty_out (vty, "Invalid direction: %s%s", direct_str, VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Check metric. */
  metric = atoi (metric_str);
  if (metric < 0 || metric > 16)
    {
      vty_out (vty, "Invalid metric: %s%s", metric_str, VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Get offset-list structure with interface name. */
  offset = rip_offset_list_get (ifname);

  if (offset->direct[direct].alist_name)
    free (offset->direct[direct].alist_name);
  offset->direct[direct].alist_name = strdup (alist);
  offset->direct[direct].metric = metric;

  return CMD_SUCCESS;
}

static int
rip_offset_list_unset (struct vty *vty, const char *alist,
		       const char *direct_str, const char *metric_str,
		       const char *ifname)
{
  int direct;
  int metric;
  struct rip_offset_list *offset;

  /* Check direction. */
  if (strncmp (direct_str, "i", 1) == 0)
    direct = RIP_OFFSET_LIST_IN;
  else if (strncmp (direct_str, "o", 1) == 0)
    direct = RIP_OFFSET_LIST_OUT;
  else
    {
      vty_out (vty, "Invalid direction: %s%s", direct_str, VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Check metric. */
  metric = atoi (metric_str);
  if (metric < 0 || metric > 16)
    {
      vty_out (vty, "Invalid metric: %s%s", metric_str, VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Get offset-list structure with interface name. */
  offset = rip_offset_list_lookup (ifname);

  if (offset)
    {
      if (offset->direct[direct].alist_name)
	free (offset->direct[direct].alist_name);
      offset->direct[direct].alist_name = NULL;

      if (offset->direct[RIP_OFFSET_LIST_IN].alist_name == NULL &&
	  offset->direct[RIP_OFFSET_LIST_OUT].alist_name == NULL)
	{
	  listnode_delete (rip_offset_list_master, offset);
	  if (offset->ifname)
	    free (offset->ifname);
	  rip_offset_list_free (offset);
	}
    }
  else
    {
      vty_out (vty, "Can't find offset-list%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  return CMD_SUCCESS;
}

#define OFFSET_LIST_IN_NAME(O)  ((O)->direct[RIP_OFFSET_LIST_IN].alist_name)
#define OFFSET_LIST_IN_METRIC(O)  ((O)->direct[RIP_OFFSET_LIST_IN].metric)

#define OFFSET_LIST_OUT_NAME(O)  ((O)->direct[RIP_OFFSET_LIST_OUT].alist_name)
#define OFFSET_LIST_OUT_METRIC(O)  ((O)->direct[RIP_OFFSET_LIST_OUT].metric)

/* If metric is modifed return 1. */
int
rip_offset_list_apply_in (struct prefix_ipv4 *p, struct interface *ifp,
			  u_int32_t *metric)
{
  struct rip_offset_list *offset;
  struct access_list *alist;

  /* Look up offset-list with interface name. */
  offset = rip_offset_list_lookup (ifp->name);
  if (offset && OFFSET_LIST_IN_NAME (offset))
    {
      alist = access_list_lookup (AFI_IP, OFFSET_LIST_IN_NAME (offset));

      if (alist 
	  && access_list_apply (alist, (struct prefix *)p) == FILTER_PERMIT)
	{
	  *metric += OFFSET_LIST_IN_METRIC (offset);
	  return 1;
	}
      return 0;
    }
  /* Look up offset-list without interface name. */
  offset = rip_offset_list_lookup (NULL);
  if (offset && OFFSET_LIST_IN_NAME (offset))
    {
      alist = access_list_lookup (AFI_IP, OFFSET_LIST_IN_NAME (offset));

      if (alist 
	  && access_list_apply (alist, (struct prefix *)p) == FILTER_PERMIT)
	{
	  *metric += OFFSET_LIST_IN_METRIC (offset);
	  return 1;
	}
      return 0;
    }
  return 0;
}

/* If metric is modifed return 1. */
int
rip_offset_list_apply_out (struct prefix_ipv4 *p, struct interface *ifp,
			  u_int32_t *metric)
{
  struct rip_offset_list *offset;
  struct access_list *alist;

  /* Look up offset-list with interface name. */
  offset = rip_offset_list_lookup (ifp->name);
  if (offset && OFFSET_LIST_OUT_NAME (offset))
    {
      alist = access_list_lookup (AFI_IP, OFFSET_LIST_OUT_NAME (offset));

      if (alist 
	  && access_list_apply (alist, (struct prefix *)p) == FILTER_PERMIT)
	{
	  *metric += OFFSET_LIST_OUT_METRIC (offset);
	  return 1;
	}
      return 0;
    }

  /* Look up offset-list without interface name. */
  offset = rip_offset_list_lookup (NULL);
  if (offset && OFFSET_LIST_OUT_NAME (offset))
    {
      alist = access_list_lookup (AFI_IP, OFFSET_LIST_OUT_NAME (offset));

      if (alist 
	  && access_list_apply (alist, (struct prefix *)p) == FILTER_PERMIT)
	{
	  *metric += OFFSET_LIST_OUT_METRIC (offset);
	  return 1;
	}
      return 0;
    }
  return 0;
}

DEFUN (rip_offset_list,
       rip_offset_list_cmd,
       "offset-list WORD (in|out) <0-16>",
       "Modify RIP metric\n"
       "Access-list name\n"
       "For incoming updates\n"
       "For outgoing updates\n"
       "Metric value\n")
{
  return rip_offset_list_set (vty, argv[0], argv[1], argv[2], NULL);
}

DEFUN (rip_offset_list_ifname,
       rip_offset_list_ifname_cmd,
       "offset-list WORD (in|out) <0-16> IFNAME",
       "Modify RIP metric\n"
       "Access-list name\n"
       "For incoming updates\n"
       "For outgoing updates\n"
       "Metric value\n"
       "Interface to match\n")
{
  return rip_offset_list_set (vty, argv[0], argv[1], argv[2], argv[3]);
}

DEFUN (no_rip_offset_list,
       no_rip_offset_list_cmd,
       "no offset-list WORD (in|out) <0-16>",
       NO_STR
       "Modify RIP metric\n"
       "Access-list name\n"
       "For incoming updates\n"
       "For outgoing updates\n"
       "Metric value\n")
{
  return rip_offset_list_unset (vty, argv[0], argv[1], argv[2], NULL);
}

DEFUN (no_rip_offset_list_ifname,
       no_rip_offset_list_ifname_cmd,
       "no offset-list WORD (in|out) <0-16> IFNAME",
       NO_STR
       "Modify RIP metric\n"
       "Access-list name\n"
       "For incoming updates\n"
       "For outgoing updates\n"
       "Metric value\n"
       "Interface to match\n")
{
  return rip_offset_list_unset (vty, argv[0], argv[1], argv[2], argv[3]);
}

static int
offset_list_cmp (struct rip_offset_list *o1, struct rip_offset_list *o2)
{
  return strcmp_safe (o1->ifname, o2->ifname);
}

static void
offset_list_del (struct rip_offset_list *offset)
{
  if (OFFSET_LIST_IN_NAME (offset))
    free (OFFSET_LIST_IN_NAME (offset));
  if (OFFSET_LIST_OUT_NAME (offset))
    free (OFFSET_LIST_OUT_NAME (offset));
  if (offset->ifname)
    free (offset->ifname);
  rip_offset_list_free (offset);
}

void
rip_offset_init ()
{
  rip_offset_list_master = list_new ();
  rip_offset_list_master->cmp = (int (*)(void *, void *)) offset_list_cmp;
  rip_offset_list_master->del = (void (*)(void *)) offset_list_del;

  install_element (RIP_NODE, &rip_offset_list_cmd);
  install_element (RIP_NODE, &rip_offset_list_ifname_cmd);
  install_element (RIP_NODE, &no_rip_offset_list_cmd);
  install_element (RIP_NODE, &no_rip_offset_list_ifname_cmd);
}

void
rip_offset_clean ()
{
  list_delete (rip_offset_list_master);

  rip_offset_list_master = list_new ();
  rip_offset_list_master->cmp = (int (*)(void *, void *)) offset_list_cmp;
  rip_offset_list_master->del = (void (*)(void *)) offset_list_del;
}

int
config_write_rip_offset_list (struct vty *vty)
{
  struct listnode *node, *nnode;
  struct rip_offset_list *offset;

  for (ALL_LIST_ELEMENTS (rip_offset_list_master, node, nnode, offset))
    {
      if (! offset->ifname)
	{
	  if (offset->direct[RIP_OFFSET_LIST_IN].alist_name)
	    vty_out (vty, " offset-list %s in %d%s",
		     offset->direct[RIP_OFFSET_LIST_IN].alist_name,
		     offset->direct[RIP_OFFSET_LIST_IN].metric,
		     VTY_NEWLINE);
	  if (offset->direct[RIP_OFFSET_LIST_OUT].alist_name)
	    vty_out (vty, " offset-list %s out %d%s",
		     offset->direct[RIP_OFFSET_LIST_OUT].alist_name,
		     offset->direct[RIP_OFFSET_LIST_OUT].metric,
		     VTY_NEWLINE);
	}
      else
	{
	  if (offset->direct[RIP_OFFSET_LIST_IN].alist_name)
	    vty_out (vty, " offset-list %s in %d %s%s",
		     offset->direct[RIP_OFFSET_LIST_IN].alist_name,
		     offset->direct[RIP_OFFSET_LIST_IN].metric,
		     offset->ifname, VTY_NEWLINE);
	  if (offset->direct[RIP_OFFSET_LIST_OUT].alist_name)
	    vty_out (vty, " offset-list %s out %d %s%s",
		     offset->direct[RIP_OFFSET_LIST_OUT].alist_name,
		     offset->direct[RIP_OFFSET_LIST_OUT].metric,
		     offset->ifname, VTY_NEWLINE);
	}
    }

  return 0;
}
