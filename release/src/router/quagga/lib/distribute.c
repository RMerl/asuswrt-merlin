/* Distribute list functions
 * Copyright (C) 1998, 1999 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
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

#include "hash.h"
#include "if.h"
#include "filter.h"
#include "command.h"
#include "distribute.h"
#include "memory.h"

/* Hash of distribute list. */
struct hash *disthash;

/* Hook functions. */
void (*distribute_add_hook) (struct distribute *);
void (*distribute_delete_hook) (struct distribute *);

static struct distribute *
distribute_new (void)
{
  return XCALLOC (MTYPE_DISTRIBUTE, sizeof (struct distribute));
}

/* Free distribute object. */
static void
distribute_free (struct distribute *dist)
{
  if (dist->ifname)
    XFREE (MTYPE_DISTRIBUTE_IFNAME, dist->ifname);

  if (dist->list[DISTRIBUTE_IN])
    free (dist->list[DISTRIBUTE_IN]);
  if (dist->list[DISTRIBUTE_OUT])
    free (dist->list[DISTRIBUTE_OUT]);

  if (dist->prefix[DISTRIBUTE_IN])
    free (dist->prefix[DISTRIBUTE_IN]);
  if (dist->prefix[DISTRIBUTE_OUT])
    free (dist->prefix[DISTRIBUTE_OUT]);

  XFREE (MTYPE_DISTRIBUTE, dist);
}

/* Lookup interface's distribute list. */
struct distribute *
distribute_lookup (const char *ifname)
{
  struct distribute key;
  struct distribute *dist;

  /* temporary reference */
  key.ifname = (char *)ifname;

  dist = hash_lookup (disthash, &key);
  
  return dist;
}

void
distribute_list_add_hook (void (*func) (struct distribute *))
{
  distribute_add_hook = func;
}

void
distribute_list_delete_hook (void (*func) (struct distribute *))
{
  distribute_delete_hook = func;
}

static void *
distribute_hash_alloc (struct distribute *arg)
{
  struct distribute *dist;

  dist = distribute_new ();
  if (arg->ifname)
    dist->ifname = XSTRDUP (MTYPE_DISTRIBUTE_IFNAME, arg->ifname);
  else
    dist->ifname = NULL;
  return dist;
}

/* Make new distribute list and push into hash. */
static struct distribute *
distribute_get (const char *ifname)
{
  struct distribute key;

  /* temporary reference */
  key.ifname = (char *)ifname;
  
  return hash_get (disthash, &key, (void * (*) (void *))distribute_hash_alloc);
}

static unsigned int
distribute_hash_make (void *arg)
{
  const struct distribute *dist = arg;

  return dist->ifname ? string_hash_make (dist->ifname) : 0;
}

/* If two distribute-list have same value then return 1 else return
   0. This function is used by hash package. */
static int
distribute_cmp (const struct distribute *dist1, const struct distribute *dist2)
{
  if (dist1->ifname && dist2->ifname)
    if (strcmp (dist1->ifname, dist2->ifname) == 0)
      return 1;
  if (! dist1->ifname && ! dist2->ifname)
    return 1;
  return 0;
}

/* Set access-list name to the distribute list. */
static struct distribute *
distribute_list_set (const char *ifname, enum distribute_type type, 
                     const char *alist_name)
{
  struct distribute *dist;

  dist = distribute_get (ifname);

  if (type == DISTRIBUTE_IN)
    {
      if (dist->list[DISTRIBUTE_IN])
	free (dist->list[DISTRIBUTE_IN]);
      dist->list[DISTRIBUTE_IN] = strdup (alist_name);
    }
  if (type == DISTRIBUTE_OUT)
    {
      if (dist->list[DISTRIBUTE_OUT])
	free (dist->list[DISTRIBUTE_OUT]);
      dist->list[DISTRIBUTE_OUT] = strdup (alist_name);
    }

  /* Apply this distribute-list to the interface. */
  (*distribute_add_hook) (dist);
  
  return dist;
}

/* Unset distribute-list.  If matched distribute-list exist then
   return 1. */
static int
distribute_list_unset (const char *ifname, enum distribute_type type, 
		       const char *alist_name)
{
  struct distribute *dist;

  dist = distribute_lookup (ifname);
  if (!dist)
    return 0;

  if (type == DISTRIBUTE_IN)
    {
      if (!dist->list[DISTRIBUTE_IN])
	return 0;
      if (strcmp (dist->list[DISTRIBUTE_IN], alist_name) != 0)
	return 0;

      free (dist->list[DISTRIBUTE_IN]);
      dist->list[DISTRIBUTE_IN] = NULL;      
    }

  if (type == DISTRIBUTE_OUT)
    {
      if (!dist->list[DISTRIBUTE_OUT])
	return 0;
      if (strcmp (dist->list[DISTRIBUTE_OUT], alist_name) != 0)
	return 0;

      free (dist->list[DISTRIBUTE_OUT]);
      dist->list[DISTRIBUTE_OUT] = NULL;      
    }

  /* Apply this distribute-list to the interface. */
  (*distribute_delete_hook) (dist);

  /* If both out and in is NULL then free distribute list. */
  if (dist->list[DISTRIBUTE_IN] == NULL &&
      dist->list[DISTRIBUTE_OUT] == NULL &&
      dist->prefix[DISTRIBUTE_IN] == NULL &&
      dist->prefix[DISTRIBUTE_OUT] == NULL)
    {
      hash_release (disthash, dist);
      distribute_free (dist);
    }

  return 1;
}

/* Set access-list name to the distribute list. */
static struct distribute *
distribute_list_prefix_set (const char *ifname, enum distribute_type type,
			    const char *plist_name)
{
  struct distribute *dist;

  dist = distribute_get (ifname);

  if (type == DISTRIBUTE_IN)
    {
      if (dist->prefix[DISTRIBUTE_IN])
	free (dist->prefix[DISTRIBUTE_IN]);
      dist->prefix[DISTRIBUTE_IN] = strdup (plist_name);
    }
  if (type == DISTRIBUTE_OUT)
    {
      if (dist->prefix[DISTRIBUTE_OUT])
	free (dist->prefix[DISTRIBUTE_OUT]);
      dist->prefix[DISTRIBUTE_OUT] = strdup (plist_name);
    }

  /* Apply this distribute-list to the interface. */
  (*distribute_add_hook) (dist);
  
  return dist;
}

/* Unset distribute-list.  If matched distribute-list exist then
   return 1. */
static int
distribute_list_prefix_unset (const char *ifname, enum distribute_type type,
			      const char *plist_name)
{
  struct distribute *dist;

  dist = distribute_lookup (ifname);
  if (!dist)
    return 0;

  if (type == DISTRIBUTE_IN)
    {
      if (!dist->prefix[DISTRIBUTE_IN])
	return 0;
      if (strcmp (dist->prefix[DISTRIBUTE_IN], plist_name) != 0)
	return 0;

      free (dist->prefix[DISTRIBUTE_IN]);
      dist->prefix[DISTRIBUTE_IN] = NULL;      
    }

  if (type == DISTRIBUTE_OUT)
    {
      if (!dist->prefix[DISTRIBUTE_OUT])
	return 0;
      if (strcmp (dist->prefix[DISTRIBUTE_OUT], plist_name) != 0)
	return 0;

      free (dist->prefix[DISTRIBUTE_OUT]);
      dist->prefix[DISTRIBUTE_OUT] = NULL;      
    }

  /* Apply this distribute-list to the interface. */
  (*distribute_delete_hook) (dist);

  /* If both out and in is NULL then free distribute list. */
  if (dist->list[DISTRIBUTE_IN] == NULL &&
      dist->list[DISTRIBUTE_OUT] == NULL &&
      dist->prefix[DISTRIBUTE_IN] == NULL &&
      dist->prefix[DISTRIBUTE_OUT] == NULL)
    {
      hash_release (disthash, dist);
      distribute_free (dist);
    }

  return 1;
}

DEFUN (distribute_list_all,
       distribute_list_all_cmd,
       "distribute-list WORD (in|out)",
       "Filter networks in routing updates\n"
       "Access-list name\n"
       "Filter incoming routing updates\n"
       "Filter outgoing routing updates\n")
{
  enum distribute_type type;

  /* Check of distribute list type. */
  if (strncmp (argv[1], "i", 1) == 0)
    type = DISTRIBUTE_IN;
  else if (strncmp (argv[1], "o", 1) == 0)
    type = DISTRIBUTE_OUT;
  else
    {
      vty_out (vty, "distribute list direction must be [in|out]%s",
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Get interface name corresponding distribute list. */
  distribute_list_set (NULL, type, argv[0]);

  return CMD_SUCCESS;
}

ALIAS (distribute_list_all,
       ipv6_distribute_list_all_cmd,
       "distribute-list WORD (in|out)",
       "Filter networks in routing updates\n"
       "Access-list name\n"
       "Filter incoming routing updates\n"
       "Filter outgoing routing updates\n")

DEFUN (no_distribute_list_all,
       no_distribute_list_all_cmd,
       "no distribute-list WORD (in|out)",
       NO_STR
       "Filter networks in routing updates\n"
       "Access-list name\n"
       "Filter incoming routing updates\n"
       "Filter outgoing routing updates\n")
{
  int ret;
  enum distribute_type type;

  /* Check of distribute list type. */
  if (strncmp (argv[1], "i", 1) == 0)
    type = DISTRIBUTE_IN;
  else if (strncmp (argv[1], "o", 1) == 0)
    type = DISTRIBUTE_OUT;
  else
    {
      vty_out (vty, "distribute list direction must be [in|out]%s",
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  ret = distribute_list_unset (NULL, type, argv[0]);
  if (! ret)
    {
      vty_out (vty, "distribute list doesn't exist%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  return CMD_SUCCESS;
}

ALIAS (no_distribute_list_all,
       no_ipv6_distribute_list_all_cmd,
       "no distribute-list WORD (in|out)",
       NO_STR
       "Filter networks in routing updates\n"
       "Access-list name\n"
       "Filter incoming routing updates\n"
       "Filter outgoing routing updates\n")

DEFUN (distribute_list,
       distribute_list_cmd,
       "distribute-list WORD (in|out) WORD",
       "Filter networks in routing updates\n"
       "Access-list name\n"
       "Filter incoming routing updates\n"
       "Filter outgoing routing updates\n"
       "Interface name\n")
{
  enum distribute_type type;

  /* Check of distribute list type. */
  if (strncmp (argv[1], "i", 1) == 0)
    type = DISTRIBUTE_IN;
  else if (strncmp (argv[1], "o", 1) == 0)
    type = DISTRIBUTE_OUT;
  else
    {
      vty_out (vty, "distribute list direction must be [in|out]%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Get interface name corresponding distribute list. */
  distribute_list_set (argv[2], type, argv[0]);

  return CMD_SUCCESS;
}       

ALIAS (distribute_list,
       ipv6_distribute_list_cmd,
       "distribute-list WORD (in|out) WORD",
       "Filter networks in routing updates\n"
       "Access-list name\n"
       "Filter incoming routing updates\n"
       "Filter outgoing routing updates\n"
       "Interface name\n")

DEFUN (no_distribute_list, no_distribute_list_cmd,
       "no distribute-list WORD (in|out) WORD",
       NO_STR
       "Filter networks in routing updates\n"
       "Access-list name\n"
       "Filter incoming routing updates\n"
       "Filter outgoing routing updates\n"
       "Interface name\n")
{
  int ret;
  enum distribute_type type;

  /* Check of distribute list type. */
  if (strncmp (argv[1], "i", 1) == 0)
    type = DISTRIBUTE_IN;
  else if (strncmp (argv[1], "o", 1) == 0)
    type = DISTRIBUTE_OUT;
  else
    {
      vty_out (vty, "distribute list direction must be [in|out]%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  ret = distribute_list_unset (argv[2], type, argv[0]);
  if (! ret)
    {
      vty_out (vty, "distribute list doesn't exist%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  return CMD_SUCCESS;
}       

ALIAS (no_distribute_list, no_ipv6_distribute_list_cmd,
       "no distribute-list WORD (in|out) WORD",
       NO_STR
       "Filter networks in routing updates\n"
       "Access-list name\n"
       "Filter incoming routing updates\n"
       "Filter outgoing routing updates\n"
       "Interface name\n")

DEFUN (distribute_list_prefix_all,
       distribute_list_prefix_all_cmd,
       "distribute-list prefix WORD (in|out)",
       "Filter networks in routing updates\n"
       "Filter prefixes in routing updates\n"
       "Name of an IP prefix-list\n"
       "Filter incoming routing updates\n"
       "Filter outgoing routing updates\n")
{
  enum distribute_type type;

  /* Check of distribute list type. */
  if (strncmp (argv[1], "i", 1) == 0)
    type = DISTRIBUTE_IN;
  else if (strncmp (argv[1], "o", 1) == 0)
    type = DISTRIBUTE_OUT;
  else
    {
      vty_out (vty, "distribute list direction must be [in|out]%s", 
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Get interface name corresponding distribute list. */
  distribute_list_prefix_set (NULL, type, argv[0]);

  return CMD_SUCCESS;
}       

ALIAS (distribute_list_prefix_all,
       ipv6_distribute_list_prefix_all_cmd,
       "distribute-list prefix WORD (in|out)",
       "Filter networks in routing updates\n"
       "Filter prefixes in routing updates\n"
       "Name of an IP prefix-list\n"
       "Filter incoming routing updates\n"
       "Filter outgoing routing updates\n")

DEFUN (no_distribute_list_prefix_all,
       no_distribute_list_prefix_all_cmd,
       "no distribute-list prefix WORD (in|out)",
       NO_STR
       "Filter networks in routing updates\n"
       "Filter prefixes in routing updates\n"
       "Name of an IP prefix-list\n"
       "Filter incoming routing updates\n"
       "Filter outgoing routing updates\n")
{
  int ret;
  enum distribute_type type;

  /* Check of distribute list type. */
  if (strncmp (argv[1], "i", 1) == 0)
    type = DISTRIBUTE_IN;
  else if (strncmp (argv[1], "o", 1) == 0)
    type = DISTRIBUTE_OUT;
  else
    {
      vty_out (vty, "distribute list direction must be [in|out]%s", 
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  ret = distribute_list_prefix_unset (NULL, type, argv[0]);
  if (! ret)
    {
      vty_out (vty, "distribute list doesn't exist%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  return CMD_SUCCESS;
}       

ALIAS (no_distribute_list_prefix_all,
       no_ipv6_distribute_list_prefix_all_cmd,
       "no distribute-list prefix WORD (in|out)",
       NO_STR
       "Filter networks in routing updates\n"
       "Filter prefixes in routing updates\n"
       "Name of an IP prefix-list\n"
       "Filter incoming routing updates\n"
       "Filter outgoing routing updates\n")

DEFUN (distribute_list_prefix, distribute_list_prefix_cmd,
       "distribute-list prefix WORD (in|out) WORD",
       "Filter networks in routing updates\n"
       "Filter prefixes in routing updates\n"
       "Name of an IP prefix-list\n"
       "Filter incoming routing updates\n"
       "Filter outgoing routing updates\n"
       "Interface name\n")
{
  enum distribute_type type;

  /* Check of distribute list type. */
  if (strncmp (argv[1], "i", 1) == 0)
    type = DISTRIBUTE_IN;
  else if (strncmp (argv[1], "o", 1) == 0)
    type = DISTRIBUTE_OUT;
  else
    {
      vty_out (vty, "distribute list direction must be [in|out]%s", 
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Get interface name corresponding distribute list. */
  distribute_list_prefix_set (argv[2], type, argv[0]);

  return CMD_SUCCESS;
}       

ALIAS (distribute_list_prefix, ipv6_distribute_list_prefix_cmd,
       "distribute-list prefix WORD (in|out) WORD",
       "Filter networks in routing updates\n"
       "Filter prefixes in routing updates\n"
       "Name of an IP prefix-list\n"
       "Filter incoming routing updates\n"
       "Filter outgoing routing updates\n"
       "Interface name\n")

DEFUN (no_distribute_list_prefix, no_distribute_list_prefix_cmd,
       "no distribute-list prefix WORD (in|out) WORD",
       NO_STR
       "Filter networks in routing updates\n"
       "Filter prefixes in routing updates\n"
       "Name of an IP prefix-list\n"
       "Filter incoming routing updates\n"
       "Filter outgoing routing updates\n"
       "Interface name\n")
{
  int ret;
  enum distribute_type type;

  /* Check of distribute list type. */
  if (strncmp (argv[1], "i", 1) == 0)
    type = DISTRIBUTE_IN;
  else if (strncmp (argv[1], "o", 1) == 0)
    type = DISTRIBUTE_OUT;
  else
    {
      vty_out (vty, "distribute list direction must be [in|out]%s", 
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  ret = distribute_list_prefix_unset (argv[2], type, argv[0]);
  if (! ret)
    {
      vty_out (vty, "distribute list doesn't exist%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  return CMD_SUCCESS;
}       

ALIAS (no_distribute_list_prefix, no_ipv6_distribute_list_prefix_cmd,
       "no distribute-list prefix WORD (in|out) WORD",
       NO_STR
       "Filter networks in routing updates\n"
       "Filter prefixes in routing updates\n"
       "Name of an IP prefix-list\n"
       "Filter incoming routing updates\n"
       "Filter outgoing routing updates\n"
       "Interface name\n")

int
config_show_distribute (struct vty *vty)
{
  unsigned int i;
  struct hash_backet *mp;
  struct distribute *dist;

  /* Output filter configuration. */
  dist = distribute_lookup (NULL);
  if (dist && (dist->list[DISTRIBUTE_OUT] || dist->prefix[DISTRIBUTE_OUT]))
    {
      vty_out (vty, "  Outgoing update filter list for all interface is");
      if (dist->list[DISTRIBUTE_OUT])
	vty_out (vty, " %s", dist->list[DISTRIBUTE_OUT]);
      if (dist->prefix[DISTRIBUTE_OUT])
	vty_out (vty, "%s (prefix-list) %s",
		 dist->list[DISTRIBUTE_OUT] ? "," : "",
		 dist->prefix[DISTRIBUTE_OUT]);
      vty_out (vty, "%s", VTY_NEWLINE);
    }
  else
    vty_out (vty, "  Outgoing update filter list for all interface is not set%s", VTY_NEWLINE);

  for (i = 0; i < disthash->size; i++)
    for (mp = disthash->index[i]; mp; mp = mp->next)
      {
	dist = mp->data;
	if (dist->ifname)
	  if (dist->list[DISTRIBUTE_OUT] || dist->prefix[DISTRIBUTE_OUT])
	    {
	      vty_out (vty, "    %s filtered by", dist->ifname);
	      if (dist->list[DISTRIBUTE_OUT])
		vty_out (vty, " %s", dist->list[DISTRIBUTE_OUT]);
	      if (dist->prefix[DISTRIBUTE_OUT])
		vty_out (vty, "%s (prefix-list) %s",
			 dist->list[DISTRIBUTE_OUT] ? "," : "",
			 dist->prefix[DISTRIBUTE_OUT]);
	      vty_out (vty, "%s", VTY_NEWLINE);
	    }
      }


  /* Input filter configuration. */
  dist = distribute_lookup (NULL);
  if (dist && (dist->list[DISTRIBUTE_IN] || dist->prefix[DISTRIBUTE_IN]))
    {
      vty_out (vty, "  Incoming update filter list for all interface is");
      if (dist->list[DISTRIBUTE_IN])
	vty_out (vty, " %s", dist->list[DISTRIBUTE_IN]);
      if (dist->prefix[DISTRIBUTE_IN])
	vty_out (vty, "%s (prefix-list) %s",
		 dist->list[DISTRIBUTE_IN] ? "," : "",
		 dist->prefix[DISTRIBUTE_IN]);
      vty_out (vty, "%s", VTY_NEWLINE);
    }
  else
    vty_out (vty, "  Incoming update filter list for all interface is not set%s", VTY_NEWLINE);

  for (i = 0; i < disthash->size; i++)
    for (mp = disthash->index[i]; mp; mp = mp->next)
      {
	dist = mp->data;
	if (dist->ifname)
	  if (dist->list[DISTRIBUTE_IN] || dist->prefix[DISTRIBUTE_IN])
	    {
	      vty_out (vty, "    %s filtered by", dist->ifname);
	      if (dist->list[DISTRIBUTE_IN])
		vty_out (vty, " %s", dist->list[DISTRIBUTE_IN]);
	      if (dist->prefix[DISTRIBUTE_IN])
		vty_out (vty, "%s (prefix-list) %s",
			 dist->list[DISTRIBUTE_IN] ? "," : "",
			 dist->prefix[DISTRIBUTE_IN]);
	      vty_out (vty, "%s", VTY_NEWLINE);
	    }
      }
  return 0;
}

/* Configuration write function. */
int
config_write_distribute (struct vty *vty)
{
  unsigned int i;
  struct hash_backet *mp;
  int write = 0;

  for (i = 0; i < disthash->size; i++)
    for (mp = disthash->index[i]; mp; mp = mp->next)
      {
	struct distribute *dist;

	dist = mp->data;

	if (dist->list[DISTRIBUTE_IN])
	  {
	    vty_out (vty, " distribute-list %s in %s%s", 
		     dist->list[DISTRIBUTE_IN],
		     dist->ifname ? dist->ifname : "",
		     VTY_NEWLINE);
	    write++;
	  }

	if (dist->list[DISTRIBUTE_OUT])
	  {
	    vty_out (vty, " distribute-list %s out %s%s", 

		     dist->list[DISTRIBUTE_OUT],
		     dist->ifname ? dist->ifname : "",
		     VTY_NEWLINE);
	    write++;
	  }

	if (dist->prefix[DISTRIBUTE_IN])
	  {
	    vty_out (vty, " distribute-list prefix %s in %s%s",
		     dist->prefix[DISTRIBUTE_IN],
		     dist->ifname ? dist->ifname : "",
		     VTY_NEWLINE);
	    write++;
	  }

	if (dist->prefix[DISTRIBUTE_OUT])
	  {
	    vty_out (vty, " distribute-list prefix %s out %s%s",
		     dist->prefix[DISTRIBUTE_OUT],
		     dist->ifname ? dist->ifname : "",
		     VTY_NEWLINE);
	    write++;
	  }
      }
  return write;
}

/* Clear all distribute list. */
void
distribute_list_reset ()
{
  hash_clean (disthash, (void (*) (void *)) distribute_free);
}

/* Initialize distribute list related hash. */
void
distribute_list_init (int node)
{
  disthash = hash_create (distribute_hash_make,
                          (int (*) (const void *, const void *)) distribute_cmp);

  if(node==RIP_NODE) {
    install_element (node, &distribute_list_all_cmd);
    install_element (node, &no_distribute_list_all_cmd);
    install_element (node, &distribute_list_cmd);
    install_element (node, &no_distribute_list_cmd);
    install_element (node, &distribute_list_prefix_all_cmd);
    install_element (node, &no_distribute_list_prefix_all_cmd);
    install_element (node, &distribute_list_prefix_cmd);
    install_element (node, &no_distribute_list_prefix_cmd);
  } else if (node == RIPNG_NODE || node == BABEL_NODE) {
    /* WARNING: two identical commands installed do a crash, so be worry with
     aliases. For this reason, and because all these commands are aliases, Babel
     is not set with RIP. */
    install_element (node, &ipv6_distribute_list_all_cmd);
    install_element (node, &no_ipv6_distribute_list_all_cmd);
    install_element (node, &ipv6_distribute_list_cmd);
    install_element (node, &no_ipv6_distribute_list_cmd);
    install_element (node, &ipv6_distribute_list_prefix_all_cmd);
    install_element (node, &no_ipv6_distribute_list_prefix_all_cmd);
    install_element (node, &ipv6_distribute_list_prefix_cmd);
    install_element (node, &no_ipv6_distribute_list_prefix_cmd);
  }
}
