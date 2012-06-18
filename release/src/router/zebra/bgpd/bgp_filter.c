/* AS path filter list.
   Copyright (C) 1999 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include <zebra.h>

#include "command.h"
#include "log.h"
#include "memory.h"
#include "buffer.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_aspath.h"
#include "bgpd/bgp_regex.h"
#include "bgpd/bgp_filter.h"

/* List of AS filter list. */
struct as_list_list
{
  struct as_list *head;
  struct as_list *tail;
};

/* AS path filter master. */
struct as_list_master
{
  /* List of access_list which name is number. */
  struct as_list_list num;

  /* List of access_list which name is string. */
  struct as_list_list str;

  /* Hook function which is executed when new access_list is added. */
  void (*add_hook) ();

  /* Hook function which is executed when access_list is deleted. */
  void (*delete_hook) ();
};

/* Element of AS path filter. */
struct as_filter
{
  struct as_filter *next;
  struct as_filter *prev;

  enum as_filter_type type;

  regex_t *reg;
  char *reg_str;
};

enum as_list_type
{
  ACCESS_TYPE_STRING,
  ACCESS_TYPE_NUMBER
};

/* AS path filter list. */
struct as_list
{
  char *name;

  enum as_list_type type;

  struct as_list *next;
  struct as_list *prev;

  struct as_filter *head;
  struct as_filter *tail;
};

/* ip as-path access-list 10 permit AS1. */

static struct as_list_master as_list_master =
{
  {NULL, NULL},
  {NULL, NULL},
  NULL,
  NULL
};

/* Allocate new AS filter. */
struct as_filter *
as_filter_new ()
{
  struct as_filter *new;

  new = XMALLOC (MTYPE_AS_FILTER, sizeof (struct as_filter));
  memset (new, 0, sizeof (struct as_filter));
  return new;
}

/* Free allocated AS filter. */
void
as_filter_free (struct as_filter *asfilter)
{
  if (asfilter->reg)
    bgp_regex_free (asfilter->reg);
  if (asfilter->reg_str)
    XFREE (MTYPE_AS_FILTER_STR, asfilter->reg_str);
  XFREE (MTYPE_AS_FILTER, asfilter);
}

/* Make new AS filter. */
struct as_filter *
as_filter_make (regex_t *reg, char *reg_str, enum as_filter_type type)
{
  struct as_filter *asfilter;

  asfilter = as_filter_new ();
  asfilter->reg = reg;
  asfilter->type = type;
  asfilter->reg_str = XSTRDUP (MTYPE_AS_FILTER_STR, reg_str);

  return asfilter;
}

struct as_filter *
as_filter_lookup (struct as_list *aslist, char *reg_str,
		  enum as_filter_type type)
{
  struct as_filter *asfilter;

  for (asfilter = aslist->head; asfilter; asfilter = asfilter->next)
    if (strcmp (reg_str, asfilter->reg_str) == 0)
      return asfilter;
  return NULL;
}

void
as_list_filter_add (struct as_list *aslist, struct as_filter *asfilter)
{
  asfilter->next = NULL;
  asfilter->prev = aslist->tail;

  if (aslist->tail)
    aslist->tail->next = asfilter;
  else
    aslist->head = asfilter;
  aslist->tail = asfilter;
}

/* Lookup as_list from list of as_list by name. */
struct as_list *
as_list_lookup (char *name)
{
  struct as_list *aslist;

  if (name == NULL)
    return NULL;

  for (aslist = as_list_master.num.head; aslist; aslist = aslist->next)
    if (strcmp (aslist->name, name) == 0)
      return aslist;

  for (aslist = as_list_master.str.head; aslist; aslist = aslist->next)
    if (strcmp (aslist->name, name) == 0)
      return aslist;

  return NULL;
}

struct as_list *
as_list_new ()
{
  struct as_list *new;

  new = XMALLOC (MTYPE_AS_LIST, sizeof (struct as_list));
  memset (new, 0, sizeof (struct as_list));
  return new;
}

void
as_list_free (struct as_list *aslist)
{
  XFREE (MTYPE_AS_LIST, aslist);
}

/* Insert new AS list to list of as_list.  Each as_list is sorted by
   the name. */
struct as_list *
as_list_insert (char *name)
{
  int i;
  long number;
  struct as_list *aslist;
  struct as_list *point;
  struct as_list_list *list;

  /* Allocate new access_list and copy given name. */
  aslist = as_list_new ();
  aslist->name = strdup (name);

  /* If name is made by all digit character.  We treat it as
     number. */
  for (number = 0, i = 0; i < strlen (name); i++)
    {
      if (isdigit ((int) name[i]))
	number = (number * 10) + (name[i] - '0');
      else
	break;
    }

  /* In case of name is all digit character */
  if (i == strlen (name))
    {
      aslist->type = ACCESS_TYPE_NUMBER;

      /* Set access_list to number list. */
      list = &as_list_master.num;

      for (point = list->head; point; point = point->next)
	if (atol (point->name) >= number)
	  break;
    }
  else
    {
      aslist->type = ACCESS_TYPE_STRING;

      /* Set access_list to string list. */
      list = &as_list_master.str;
  
      /* Set point to insertion point. */
      for (point = list->head; point; point = point->next)
	if (strcmp (point->name, name) >= 0)
	  break;
    }

  /* In case of this is the first element of master. */
  if (list->head == NULL)
    {
      list->head = list->tail = aslist;
      return aslist;
    }

  /* In case of insertion is made at the tail of access_list. */
  if (point == NULL)
    {
      aslist->prev = list->tail;
      list->tail->next = aslist;
      list->tail = aslist;
      return aslist;
    }

  /* In case of insertion is made at the head of access_list. */
  if (point == list->head)
    {
      aslist->next = list->head;
      list->head->prev = aslist;
      list->head = aslist;
      return aslist;
    }

  /* Insertion is made at middle of the access_list. */
  aslist->next = point;
  aslist->prev = point->prev;

  if (point->prev)
    point->prev->next = aslist;
  point->prev = aslist;

  return aslist;
}

struct as_list *
as_list_get (char *name)
{
  struct as_list *aslist;

  aslist = as_list_lookup (name);
  if (aslist == NULL)
    {
      aslist = as_list_insert (name);

      /* Run hook function. */
      if (as_list_master.add_hook)
	(*as_list_master.add_hook) ();
    }

  return aslist;
}

static char *
filter_type_str (enum as_filter_type type)
{
  switch (type)
    {
    case AS_FILTER_PERMIT:
      return "permit";
      break;
    case AS_FILTER_DENY:
      return "deny";
      break;
    default:
      return "";
      break;
    }
}

void
as_list_delete (struct as_list *aslist)
{
  struct as_list_list *list;
  struct as_filter *filter, *next;

  for (filter = aslist->head; filter; filter = next)
    {
      next = filter->next;
      as_filter_free (filter);
    }

  if (aslist->type == ACCESS_TYPE_NUMBER)
    list = &as_list_master.num;
  else
    list = &as_list_master.str;

  if (aslist->next)
    aslist->next->prev = aslist->prev;
  else
    list->tail = aslist->prev;

  if (aslist->prev)
    aslist->prev->next = aslist->next;
  else
    list->head = aslist->next;

  as_list_free (aslist);
}

static int
as_list_empty (struct as_list *aslist)
{
  if (aslist->head == NULL && aslist->tail == NULL)
    return 1;
  else
    return 0;
}

void
as_list_filter_delete (struct as_list *aslist, struct as_filter *asfilter)
{
  if (asfilter->next)
    asfilter->next->prev = asfilter->prev;
  else
    aslist->tail = asfilter->prev;

  if (asfilter->prev)
    asfilter->prev->next = asfilter->next;
  else
    aslist->head = asfilter->next;

  as_filter_free (asfilter);

  /* If access_list becomes empty delete it from access_master. */
  if (as_list_empty (aslist))
    as_list_delete (aslist);

  /* Run hook function. */
  if (as_list_master.delete_hook)
    (*as_list_master.delete_hook) ();
}

static int
as_filter_match (struct as_filter *asfilter, struct aspath *aspath)
{
  if (bgp_regexec (asfilter->reg, aspath) != REG_NOMATCH)
    return 1;
  return 0;
}

/* Apply AS path filter to AS. */
enum as_filter_type
as_list_apply (struct as_list *aslist, void *object)
{
  struct as_filter *asfilter;
  struct aspath *aspath;

  aspath = (struct aspath *) object;

  if (aslist == NULL)
    return AS_FILTER_DENY;

  for (asfilter = aslist->head; asfilter; asfilter = asfilter->next)
    {
      if (as_filter_match (asfilter, aspath))
	return asfilter->type;
    }
  return AS_FILTER_DENY;
}

/* Add hook function. */
void
as_list_add_hook (void (*func) ())
{
  as_list_master.add_hook = func;
}

/* Delete hook function. */
void
as_list_delete_hook (void (*func) ())
{
  as_list_master.delete_hook = func;
}

int
as_list_dup_check (struct as_list *aslist, struct as_filter *new)
{
  struct as_filter *asfilter;

  for (asfilter = aslist->head; asfilter; asfilter = asfilter->next)
    {
      if (asfilter->type == new->type
	  && strcmp (asfilter->reg_str, new->reg_str) == 0)
	return 1;
    }
  return 0;
}

DEFUN (ip_as_path, ip_as_path_cmd,
       "ip as-path access-list WORD (deny|permit) .LINE",
       IP_STR
       "BGP autonomous system path filter\n"
       "Specify an access list name\n"
       "Regular expression access list name\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "A regular-expression to match the BGP AS paths\n")
{
  enum as_filter_type type;
  struct as_filter *asfilter;
  struct as_list *aslist;
  regex_t *regex;
  struct buffer *b;
  int i;
  char *regstr;
  int first = 0;

  /* Check the filter type. */
  if (strncmp (argv[1], "p", 1) == 0)
    type = AS_FILTER_PERMIT;
  else if (strncmp (argv[1], "d", 1) == 0)
    type = AS_FILTER_DENY;
  else
    {
      vty_out (vty, "filter type must be [permit|deny]%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Check AS path regex. */
  b = buffer_new (1024);
  for (i = 2; i < argc; i++)
    {
      if (first)
	buffer_putc (b, ' ');
      else
	first = 1;

      buffer_putstr (b, argv[i]);
    }
  buffer_putc (b, '\0');

  regstr = buffer_getstr (b);
  buffer_free (b);

  regex = bgp_regcomp (regstr);
  if (!regex)
    {
      free (regstr);
      vty_out (vty, "can't compile regexp %s%s", argv[0],
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  asfilter = as_filter_make (regex, regstr, type);
  
  free (regstr);

  /* Install new filter to the access_list. */
  aslist = as_list_get (argv[0]);

  /* Duplicate insertion check. */;
  if (as_list_dup_check (aslist, asfilter))
    as_filter_free (asfilter);
  else
    as_list_filter_add (aslist, asfilter);

  return CMD_SUCCESS;
}

DEFUN (no_ip_as_path,
       no_ip_as_path_cmd,
       "no ip as-path access-list WORD (deny|permit) .LINE",
       NO_STR
       IP_STR
       "BGP autonomous system path filter\n"
       "Specify an access list name\n"
       "Regular expression access list name\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "A regular-expression to match the BGP AS paths\n")
{
  enum as_filter_type type;
  struct as_filter *asfilter;
  struct as_list *aslist;
  struct buffer *b;
  int i;
  int first = 0;
  char *regstr;
  regex_t *regex;

  /* Lookup AS list from AS path list. */
  aslist = as_list_lookup (argv[0]);
  if (aslist == NULL)
    {
      vty_out (vty, "ip as-path access-list %s doesn't exist%s", argv[0],
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Check the filter type. */
  if (strncmp (argv[1], "p", 1) == 0)
    type = AS_FILTER_PERMIT;
  else if (strncmp (argv[1], "d", 1) == 0)
    type = AS_FILTER_DENY;
  else
    {
      vty_out (vty, "filter type must be [permit|deny]%s", VTY_NEWLINE);
      return CMD_WARNING;
    }
  
  /* Compile AS path. */
  b = buffer_new (1024);
  for (i = 2; i < argc; i++)
    {
      if (first)
	buffer_putc (b, ' ');
      else
	first = 1;

      buffer_putstr (b, argv[i]);
    }
  buffer_putc (b, '\0');

  regstr = buffer_getstr (b);
  buffer_free (b);

  regex = bgp_regcomp (regstr);
  if (!regex)
    {
      free (regstr);
      vty_out (vty, "can't compile regexp %s%s", argv[0],
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Lookup asfilter. */
  asfilter = as_filter_lookup (aslist, regstr, type);

  free (regstr);
  bgp_regex_free (regex);

  if (asfilter == NULL)
    {
      vty_out (vty, "%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  as_list_filter_delete (aslist, asfilter);

  return CMD_SUCCESS;
}

DEFUN (no_ip_as_path_all,
       no_ip_as_path_all_cmd,
       "no ip as-path access-list WORD",
       NO_STR
       IP_STR
       "BGP autonomous system path filter\n"
       "Specify an access list name\n"
       "Regular expression access list name\n")
{
  struct as_list *aslist;

  aslist = as_list_lookup (argv[0]);
  if (aslist == NULL)
    {
      vty_out (vty, "ip as-path access-list %s doesn't exist%s", argv[0],
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  as_list_delete (aslist);

  /* Run hook function. */
  if (as_list_master.delete_hook)
    (*as_list_master.delete_hook) ();

  return CMD_SUCCESS;
}

void
as_list_show (struct vty *vty, struct as_list *aslist)
{
  struct as_filter *asfilter;

  vty_out (vty, "AS path access list %s%s", aslist->name, VTY_NEWLINE);

  for (asfilter = aslist->head; asfilter; asfilter = asfilter->next)
    {
      vty_out (vty, "    %s %s%s", filter_type_str (asfilter->type),
	       asfilter->reg_str, VTY_NEWLINE);
    }
}

void
as_list_show_all (struct vty *vty)
{
  struct as_list *aslist;
  struct as_filter *asfilter;

  for (aslist = as_list_master.num.head; aslist; aslist = aslist->next)
    {
      vty_out (vty, "AS path access list %s%s", aslist->name, VTY_NEWLINE);

      for (asfilter = aslist->head; asfilter; asfilter = asfilter->next)
	{
	  vty_out (vty, "    %s %s%s", filter_type_str (asfilter->type),
		   asfilter->reg_str, VTY_NEWLINE);
	}
    }

  for (aslist = as_list_master.str.head; aslist; aslist = aslist->next)
    {
      vty_out (vty, "AS path access list %s%s", aslist->name, VTY_NEWLINE);

      for (asfilter = aslist->head; asfilter; asfilter = asfilter->next)
	{
	  vty_out (vty, "    %s %s%s", filter_type_str (asfilter->type),
		   asfilter->reg_str, VTY_NEWLINE);
	}
    }
}

DEFUN (show_ip_as_path_access_list,
       show_ip_as_path_access_list_cmd,
       "show ip as-path-access-list WORD",
       SHOW_STR
       IP_STR
       "List AS path access lists\n"
       "AS path access list name\n")
{
  struct as_list *aslist;

  aslist = as_list_lookup (argv[0]);
  if (aslist)
    as_list_show (vty, aslist);

  return CMD_SUCCESS;
}

DEFUN (show_ip_as_path_access_list_all,
       show_ip_as_path_access_list_all_cmd,
       "show ip as-path-access-list",
       SHOW_STR
       IP_STR
       "List AS path access lists\n")
{
  as_list_show_all (vty);
  return CMD_SUCCESS;
}

int
config_write_as_list (struct vty *vty)
{
  struct as_list *aslist;
  struct as_filter *asfilter;
  int write = 0;

  for (aslist = as_list_master.num.head; aslist; aslist = aslist->next)
    for (asfilter = aslist->head; asfilter; asfilter = asfilter->next)
      {
	vty_out (vty, "ip as-path access-list %s %s %s%s",
		 aslist->name, filter_type_str (asfilter->type), 
		 asfilter->reg_str,
		 VTY_NEWLINE);
	write++;
      }

  for (aslist = as_list_master.str.head; aslist; aslist = aslist->next)
    for (asfilter = aslist->head; asfilter; asfilter = asfilter->next)
      {
	vty_out (vty, "ip as-path access-list %s %s %s%s",
		 aslist->name, filter_type_str (asfilter->type), 
		 asfilter->reg_str,
		 VTY_NEWLINE);
	write++;
      }
  return write;
}

struct cmd_node as_list_node =
{
  AS_LIST_NODE,
  "",
  1
};

/* Register functions. */
void
bgp_filter_init ()
{
  install_node (&as_list_node, config_write_as_list);

  install_element (CONFIG_NODE, &ip_as_path_cmd);
  install_element (CONFIG_NODE, &no_ip_as_path_cmd);
  install_element (CONFIG_NODE, &no_ip_as_path_all_cmd);

  install_element (VIEW_NODE, &show_ip_as_path_access_list_cmd);
  install_element (VIEW_NODE, &show_ip_as_path_access_list_all_cmd);
  install_element (ENABLE_NODE, &show_ip_as_path_access_list_cmd);
  install_element (ENABLE_NODE, &show_ip_as_path_access_list_all_cmd);
}
