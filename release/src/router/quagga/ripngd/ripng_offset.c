/* RIPng offset-list
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

 /* RIPng support by Vincent Jardin <vincent.jardin@6wind.com>
  * Copyright (C) 2002 6WIND
  */

#include <zebra.h>

#include "if.h"
#include "prefix.h"
#include "filter.h"
#include "command.h"
#include "linklist.h"
#include "memory.h"

#include "ripngd/ripngd.h"

#define RIPNG_OFFSET_LIST_IN  0
#define RIPNG_OFFSET_LIST_OUT 1
#define RIPNG_OFFSET_LIST_MAX 2

struct ripng_offset_list
{
  char *ifname;

  struct 
  {
    char *alist_name;
    /* struct access_list *alist; */
    int metric;
  } direct[RIPNG_OFFSET_LIST_MAX];
};

static struct list *ripng_offset_list_master;

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

static struct ripng_offset_list *
ripng_offset_list_new ()
{
  struct ripng_offset_list *new;

  new = XCALLOC (MTYPE_RIPNG_OFFSET_LIST, sizeof (struct ripng_offset_list));
  return new;
}

static void
ripng_offset_list_free (struct ripng_offset_list *offset)
{
  XFREE (MTYPE_RIPNG_OFFSET_LIST, offset);
}

static struct ripng_offset_list *
ripng_offset_list_lookup (const char *ifname)
{
  struct ripng_offset_list *offset;
  struct listnode *node, *nnode;

  for (ALL_LIST_ELEMENTS (ripng_offset_list_master, node, nnode, offset))
    {
      if (strcmp_safe (offset->ifname, ifname) == 0)
	return offset;
    }
  return NULL;
}

static struct ripng_offset_list *
ripng_offset_list_get (const char *ifname)
{
  struct ripng_offset_list *offset;
  
  offset = ripng_offset_list_lookup (ifname);
  if (offset)
    return offset;

  offset = ripng_offset_list_new ();
  if (ifname)
    offset->ifname = strdup (ifname);
  listnode_add_sort (ripng_offset_list_master, offset);

  return offset;
}

static int
ripng_offset_list_set (struct vty *vty, const char *alist,
		       const char *direct_str, const char *metric_str,
		       const char *ifname)
{
  int direct;
  int metric;
  struct ripng_offset_list *offset;

  /* Check direction. */
  if (strncmp (direct_str, "i", 1) == 0)
    direct = RIPNG_OFFSET_LIST_IN;
  else if (strncmp (direct_str, "o", 1) == 0)
    direct = RIPNG_OFFSET_LIST_OUT;
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
  offset = ripng_offset_list_get (ifname);

  if (offset->direct[direct].alist_name)
    free (offset->direct[direct].alist_name);
  offset->direct[direct].alist_name = strdup (alist);
  offset->direct[direct].metric = metric;

  return CMD_SUCCESS;
}

static int
ripng_offset_list_unset (struct vty *vty, const char *alist,
			 const char *direct_str, const char *metric_str,
			 const char *ifname)
{
  int direct;
  int metric;
  struct ripng_offset_list *offset;

  /* Check direction. */
  if (strncmp (direct_str, "i", 1) == 0)
    direct = RIPNG_OFFSET_LIST_IN;
  else if (strncmp (direct_str, "o", 1) == 0)
    direct = RIPNG_OFFSET_LIST_OUT;
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
  offset = ripng_offset_list_lookup (ifname);

  if (offset)
    {
      if (offset->direct[direct].alist_name)
	free (offset->direct[direct].alist_name);
      offset->direct[direct].alist_name = NULL;

      if (offset->direct[RIPNG_OFFSET_LIST_IN].alist_name == NULL &&
	  offset->direct[RIPNG_OFFSET_LIST_OUT].alist_name == NULL)
	{
	  listnode_delete (ripng_offset_list_master, offset);
	  if (offset->ifname)
	    free (offset->ifname);
	  ripng_offset_list_free (offset);
	}
    }
  else
    {
      vty_out (vty, "Can't find offset-list%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  return CMD_SUCCESS;
}

#define OFFSET_LIST_IN_NAME(O)  ((O)->direct[RIPNG_OFFSET_LIST_IN].alist_name)
#define OFFSET_LIST_IN_METRIC(O)  ((O)->direct[RIPNG_OFFSET_LIST_IN].metric)

#define OFFSET_LIST_OUT_NAME(O)  ((O)->direct[RIPNG_OFFSET_LIST_OUT].alist_name)
#define OFFSET_LIST_OUT_METRIC(O)  ((O)->direct[RIPNG_OFFSET_LIST_OUT].metric)

/* If metric is modifed return 1. */
int
ripng_offset_list_apply_in (struct prefix_ipv6 *p, struct interface *ifp,
			    u_char *metric)
{
  struct ripng_offset_list *offset;
  struct access_list *alist;

  /* Look up offset-list with interface name. */
  offset = ripng_offset_list_lookup (ifp->name);
  if (offset && OFFSET_LIST_IN_NAME (offset))
    {
      alist = access_list_lookup (AFI_IP6, OFFSET_LIST_IN_NAME (offset));

      if (alist 
	  && access_list_apply (alist, (struct prefix *)p) == FILTER_PERMIT)
	{
	  *metric += OFFSET_LIST_IN_METRIC (offset);
	  return 1;
	}
      return 0;
    }
  /* Look up offset-list without interface name. */
  offset = ripng_offset_list_lookup (NULL);
  if (offset && OFFSET_LIST_IN_NAME (offset))
    {
      alist = access_list_lookup (AFI_IP6, OFFSET_LIST_IN_NAME (offset));

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
ripng_offset_list_apply_out (struct prefix_ipv6 *p, struct interface *ifp,
			     u_char *metric)
{
  struct ripng_offset_list *offset;
  struct access_list *alist;

  /* Look up offset-list with interface name. */
  offset = ripng_offset_list_lookup (ifp->name);
  if (offset && OFFSET_LIST_OUT_NAME (offset))
    {
      alist = access_list_lookup (AFI_IP6, OFFSET_LIST_OUT_NAME (offset));

      if (alist 
	  && access_list_apply (alist, (struct prefix *)p) == FILTER_PERMIT)
	{
	  *metric += OFFSET_LIST_OUT_METRIC (offset);
	  return 1;
	}
      return 0;
    }

  /* Look up offset-list without interface name. */
  offset = ripng_offset_list_lookup (NULL);
  if (offset && OFFSET_LIST_OUT_NAME (offset))
    {
      alist = access_list_lookup (AFI_IP6, OFFSET_LIST_OUT_NAME (offset));

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

DEFUN (ripng_offset_list,
       ripng_offset_list_cmd,
       "offset-list WORD (in|out) <0-16>",
       "Modify RIPng metric\n"
       "Access-list name\n"
       "For incoming updates\n"
       "For outgoing updates\n"
       "Metric value\n")
{
  return ripng_offset_list_set (vty, argv[0], argv[1], argv[2], NULL);
}

DEFUN (ripng_offset_list_ifname,
       ripng_offset_list_ifname_cmd,
       "offset-list WORD (in|out) <0-16> IFNAME",
       "Modify RIPng metric\n"
       "Access-list name\n"
       "For incoming updates\n"
       "For outgoing updates\n"
       "Metric value\n"
       "Interface to match\n")
{
  return ripng_offset_list_set (vty, argv[0], argv[1], argv[2], argv[3]);
}

DEFUN (no_ripng_offset_list,
       no_ripng_offset_list_cmd,
       "no offset-list WORD (in|out) <0-16>",
       NO_STR
       "Modify RIPng metric\n"
       "Access-list name\n"
       "For incoming updates\n"
       "For outgoing updates\n"
       "Metric value\n")
{
  return ripng_offset_list_unset (vty, argv[0], argv[1], argv[2], NULL);
}

DEFUN (no_ripng_offset_list_ifname,
       no_ripng_offset_list_ifname_cmd,
       "no offset-list WORD (in|out) <0-16> IFNAME",
       NO_STR
       "Modify RIPng metric\n"
       "Access-list name\n"
       "For incoming updates\n"
       "For outgoing updates\n"
       "Metric value\n"
       "Interface to match\n")
{
  return ripng_offset_list_unset (vty, argv[0], argv[1], argv[2], argv[3]);
}

static int
offset_list_cmp (struct ripng_offset_list *o1, struct ripng_offset_list *o2)
{
  return strcmp_safe (o1->ifname, o2->ifname);
}

static void
offset_list_del (struct ripng_offset_list *offset)
{
  if (OFFSET_LIST_IN_NAME (offset))
    free (OFFSET_LIST_IN_NAME (offset));
  if (OFFSET_LIST_OUT_NAME (offset))
    free (OFFSET_LIST_OUT_NAME (offset));
  if (offset->ifname)
    free (offset->ifname);
  ripng_offset_list_free (offset);
}

void
ripng_offset_init (void)
{
  ripng_offset_list_master = list_new ();
  ripng_offset_list_master->cmp = (int (*)(void *, void *)) offset_list_cmp;
  ripng_offset_list_master->del = (void (*)(void *)) offset_list_del;

  install_element (RIPNG_NODE, &ripng_offset_list_cmd);
  install_element (RIPNG_NODE, &ripng_offset_list_ifname_cmd);
  install_element (RIPNG_NODE, &no_ripng_offset_list_cmd);
  install_element (RIPNG_NODE, &no_ripng_offset_list_ifname_cmd);
}

void
ripng_offset_clean (void)
{
  list_delete (ripng_offset_list_master);

  ripng_offset_list_master = list_new ();
  ripng_offset_list_master->cmp = (int (*)(void *, void *)) offset_list_cmp;
  ripng_offset_list_master->del = (void (*)(void *)) offset_list_del;
}

int
config_write_ripng_offset_list (struct vty *vty)
{
  struct listnode *node, *nnode;
  struct ripng_offset_list *offset;

  for (ALL_LIST_ELEMENTS (ripng_offset_list_master, node, nnode, offset))
    {
      if (! offset->ifname)
	{
	  if (offset->direct[RIPNG_OFFSET_LIST_IN].alist_name)
	    vty_out (vty, " offset-list %s in %d%s",
		     offset->direct[RIPNG_OFFSET_LIST_IN].alist_name,
		     offset->direct[RIPNG_OFFSET_LIST_IN].metric,
		     VTY_NEWLINE);
	  if (offset->direct[RIPNG_OFFSET_LIST_OUT].alist_name)
	    vty_out (vty, " offset-list %s out %d%s",
		     offset->direct[RIPNG_OFFSET_LIST_OUT].alist_name,
		     offset->direct[RIPNG_OFFSET_LIST_OUT].metric,
		     VTY_NEWLINE);
	}
      else
	{
	  if (offset->direct[RIPNG_OFFSET_LIST_IN].alist_name)
	    vty_out (vty, " offset-list %s in %d %s%s",
		     offset->direct[RIPNG_OFFSET_LIST_IN].alist_name,
		     offset->direct[RIPNG_OFFSET_LIST_IN].metric,
		     offset->ifname, VTY_NEWLINE);
	  if (offset->direct[RIPNG_OFFSET_LIST_OUT].alist_name)
	    vty_out (vty, " offset-list %s out %d %s%s",
		     offset->direct[RIPNG_OFFSET_LIST_OUT].alist_name,
		     offset->direct[RIPNG_OFFSET_LIST_OUT].metric,
		     offset->ifname, VTY_NEWLINE);
	}
    }

  return 0;
}
