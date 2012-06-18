/* Route filtering function.
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

#include "prefix.h"
#include "filter.h"
#include "memory.h"
#include "command.h"
#include "sockunion.h"
#include "buffer.h"

struct filter_cisco
{
  /* Cisco access-list */
  int extended;
  struct in_addr addr;
  struct in_addr addr_mask;
  struct in_addr mask;
  struct in_addr mask_mask;
};

struct filter_zebra
{
  /* If this filter is "exact" match then this flag is set. */
  int exact;

  /* Prefix information. */
  struct prefix prefix;
};

/* Filter element of access list */
struct filter
{
  /* For doubly linked list. */
  struct filter *next;
  struct filter *prev;

  /* Filter type information. */
  enum filter_type type;

  /* Cisco access-list */
  int cisco;

  union
    {
      struct filter_cisco cfilter;
      struct filter_zebra zfilter;
    } u;
};

/* List of access_list. */
struct access_list_list
{
  struct access_list *head;
  struct access_list *tail;
};

/* Master structure of access_list. */
struct access_master
{
  /* List of access_list which name is number. */
  struct access_list_list num;

  /* List of access_list which name is string. */
  struct access_list_list str;

  /* Hook function which is executed when new access_list is added. */
  void (*add_hook) ();

  /* Hook function which is executed when access_list is deleted. */
  void (*delete_hook) ();
};

/* Static structure for IPv4 access_list's master. */
static struct access_master access_master_ipv4 = 
{ 
  {NULL, NULL},
  {NULL, NULL},
  NULL,
  NULL,
};

#ifdef HAVE_IPV6
/* Static structure for IPv6 access_list's master. */
static struct access_master access_master_ipv6 = 
{ 
  {NULL, NULL},
  {NULL, NULL},
  NULL,
  NULL,
};
#endif /* HAVE_IPV6 */

struct access_master *
access_master_get (afi_t afi)
{
  if (afi == AFI_IP)
    return &access_master_ipv4;
#ifdef HAVE_IPV6
  else if (afi == AFI_IP6)
    return &access_master_ipv6;
#endif /* HAVE_IPV6 */
  return NULL;
}

/* Allocate new filter structure. */
struct filter *
filter_new ()
{
  return (struct filter *) XCALLOC (MTYPE_ACCESS_FILTER,
				    sizeof (struct filter));
}

void
filter_free (struct filter *filter)
{
  XFREE (MTYPE_ACCESS_FILTER, filter);
}

/* Return string of filter_type. */
static char *
filter_type_str (struct filter *filter)
{
  switch (filter->type)
    {
    case FILTER_PERMIT:
      return "permit";
      break;
    case FILTER_DENY:
      return "deny";
      break;
    case FILTER_DYNAMIC:
      return "dynamic";
      break;
    default:
      return "";
      break;
    }
}

/* If filter match to the prefix then return 1. */
static int
filter_match_cisco (struct filter *mfilter, struct prefix *p)
{
  struct filter_cisco *filter;
  struct in_addr mask;
  u_int32_t check_addr;
  u_int32_t check_mask;

  filter = &mfilter->u.cfilter;
  check_addr = p->u.prefix4.s_addr & ~filter->addr_mask.s_addr;

  if (filter->extended)
    {
      masklen2ip (p->prefixlen, &mask);
      check_mask = mask.s_addr & ~filter->mask_mask.s_addr;

      if (memcmp (&check_addr, &filter->addr.s_addr, 4) == 0
          && memcmp (&check_mask, &filter->mask.s_addr, 4) == 0)
	return 1;
    }
  else if (memcmp (&check_addr, &filter->addr.s_addr, 4) == 0)
    return 1;

  return 0;
}

/* If filter match to the prefix then return 1. */
static int
filter_match_zebra (struct filter *mfilter, struct prefix *p)
{
  struct filter_zebra *filter;

  filter = &mfilter->u.zfilter;

  if (filter->prefix.family == p->family)
    {
      if (filter->exact)
	{
	  if (filter->prefix.prefixlen == p->prefixlen)
	    return prefix_match (&filter->prefix, p);
	  else
	    return 0;
	}
      else
	return prefix_match (&filter->prefix, p);
    }
  else
    return 0;
}

/* Allocate new access list structure. */
struct access_list *
access_list_new ()
{
  return (struct access_list *) XCALLOC (MTYPE_ACCESS_LIST,
					 sizeof (struct access_list));
}

/* Free allocated access_list. */
void
access_list_free (struct access_list *access)
{
  XFREE (MTYPE_ACCESS_LIST, access);
}

/* Delete access_list from access_master and free it. */
void
access_list_delete (struct access_list *access)
{
  struct filter *filter;
  struct filter *next;
  struct access_list_list *list;
  struct access_master *master;

  for (filter = access->head; filter; filter = next)
    {
      next = filter->next;
      filter_free (filter);
    }

  master = access->master;

  if (access->type == ACCESS_TYPE_NUMBER)
    list = &master->num;
  else
    list = &master->str;

  if (access->next)
    access->next->prev = access->prev;
  else
    list->tail = access->prev;

  if (access->prev)
    access->prev->next = access->next;
  else
    list->head = access->next;

  if (access->name)
    XFREE (MTYPE_ACCESS_LIST_STR, access->name);

  if (access->remark)
    XFREE (MTYPE_TMP, access->remark);

  access_list_free (access);
}

/* Insert new access list to list of access_list.  Each acceess_list
   is sorted by the name. */
struct access_list *
access_list_insert (afi_t afi, char *name)
{
  int i;
  long number;
  struct access_list *access;
  struct access_list *point;
  struct access_list_list *alist;
  struct access_master *master;

  master = access_master_get (afi);
  if (master == NULL)
    return NULL;

  /* Allocate new access_list and copy given name. */
  access = access_list_new ();
  access->name = XSTRDUP (MTYPE_ACCESS_LIST_STR, name);
  access->master = master;

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
      access->type = ACCESS_TYPE_NUMBER;

      /* Set access_list to number list. */
      alist = &master->num;

      for (point = alist->head; point; point = point->next)
	if (atol (point->name) >= number)
	  break;
    }
  else
    {
      access->type = ACCESS_TYPE_STRING;

      /* Set access_list to string list. */
      alist = &master->str;
  
      /* Set point to insertion point. */
      for (point = alist->head; point; point = point->next)
	if (strcmp (point->name, name) >= 0)
	  break;
    }

  /* In case of this is the first element of master. */
  if (alist->head == NULL)
    {
      alist->head = alist->tail = access;
      return access;
    }

  /* In case of insertion is made at the tail of access_list. */
  if (point == NULL)
    {
      access->prev = alist->tail;
      alist->tail->next = access;
      alist->tail = access;
      return access;
    }

  /* In case of insertion is made at the head of access_list. */
  if (point == alist->head)
    {
      access->next = alist->head;
      alist->head->prev = access;
      alist->head = access;
      return access;
    }

  /* Insertion is made at middle of the access_list. */
  access->next = point;
  access->prev = point->prev;

  if (point->prev)
    point->prev->next = access;
  point->prev = access;

  return access;
}

/* Lookup access_list from list of access_list by name. */
struct access_list *
access_list_lookup (afi_t afi, char *name)
{
  struct access_list *access;
  struct access_master *master;

  if (name == NULL)
    return NULL;

  master = access_master_get (afi);
  if (master == NULL)
    return NULL;

  for (access = master->num.head; access; access = access->next)
    if (strcmp (access->name, name) == 0)
      return access;

  for (access = master->str.head; access; access = access->next)
    if (strcmp (access->name, name) == 0)
      return access;

  return NULL;
}

/* Get access list from list of access_list.  If there isn't matched
   access_list create new one and return it. */
struct access_list *
access_list_get (afi_t afi, char *name)
{
  struct access_list *access;

  access = access_list_lookup (afi, name);
  if (access == NULL)
    access = access_list_insert (afi, name);
  return access;
}

/* Apply access list to object (which should be struct prefix *). */
enum filter_type
access_list_apply (struct access_list *access, void *object)
{
  struct filter *filter;
  struct prefix *p;

  p = (struct prefix *) object;

  if (access == NULL)
    return FILTER_DENY;

  for (filter = access->head; filter; filter = filter->next)
    {
      if (filter->cisco)
	{
	  if (filter_match_cisco (filter, p))
	    return filter->type;
	}
      else
	{
	  if (filter_match_zebra (filter, p))
	    return filter->type;
	}
    }

  return FILTER_DENY;
}

/* Add hook function. */
void
access_list_add_hook (void (*func) (struct access_list *access))
{
  access_master_ipv4.add_hook = func;
#ifdef HAVE_IPV6
  access_master_ipv6.add_hook = func;
#endif /* HAVE_IPV6 */
}

/* Delete hook function. */
void
access_list_delete_hook (void (*func) (struct access_list *access))
{
  access_master_ipv4.delete_hook = func;
#ifdef HAVE_IPV6
  access_master_ipv6.delete_hook = func;
#endif /* HAVE_IPV6 */
}

/* Add new filter to the end of specified access_list. */
void
access_list_filter_add (struct access_list *access, struct filter *filter)
{
  filter->next = NULL;
  filter->prev = access->tail;

  if (access->tail)
    access->tail->next = filter;
  else
    access->head = filter;
  access->tail = filter;

  /* Run hook function. */
  if (access->master->add_hook)
    (*access->master->add_hook) (access);
}

/* If access_list has no filter then return 1. */
static int
access_list_empty (struct access_list *access)
{
  if (access->head == NULL && access->tail == NULL)
    return 1;
  else
    return 0;
}

/* Delete filter from specified access_list.  If there is hook
   function execute it. */
void
access_list_filter_delete (struct access_list *access, struct filter *filter)
{
  struct access_master *master;

  master = access->master;

  if (filter->next)
    filter->next->prev = filter->prev;
  else
    access->tail = filter->prev;

  if (filter->prev)
    filter->prev->next = filter->next;
  else
    access->head = filter->next;

  filter_free (filter);

  /* If access_list becomes empty delete it from access_master. */
  if (access_list_empty (access))
    access_list_delete (access);

  /* Run hook function. */
  if (master->delete_hook)
    (*master->delete_hook) (access);
}

/*
  deny    Specify packets to reject
  permit  Specify packets to forward
  dynamic ?
*/

/*
  Hostname or A.B.C.D  Address to match
  any                  Any source host
  host                 A single host address
*/

struct filter *
filter_lookup_cisco (struct access_list *access, struct filter *mnew)
{
  struct filter *mfilter;
  struct filter_cisco *filter;
  struct filter_cisco *new;

  new = &mnew->u.cfilter;

  for (mfilter = access->head; mfilter; mfilter = mfilter->next)
    {
      filter = &mfilter->u.cfilter;

      if (filter->extended)
	{
	  if (mfilter->type == mnew->type
	      && filter->addr.s_addr == new->addr.s_addr
	      && filter->addr_mask.s_addr == new->addr_mask.s_addr
	      && filter->mask.s_addr == new->mask.s_addr
	      && filter->mask_mask.s_addr == new->mask_mask.s_addr)
	    return mfilter;
	}
      else
	{
	  if (mfilter->type == mnew->type
	      && filter->addr.s_addr == new->addr.s_addr
	      && filter->addr_mask.s_addr == new->addr_mask.s_addr)
	    return mfilter;
	}
    }

  return NULL;
}

struct filter *
filter_lookup_zebra (struct access_list *access, struct filter *mnew)
{
  struct filter *mfilter;
  struct filter_zebra *filter;
  struct filter_zebra *new;

  new = &mnew->u.zfilter;

  for (mfilter = access->head; mfilter; mfilter = mfilter->next)
    {
      filter = &mfilter->u.zfilter;

      if (filter->exact == new->exact
	  && mfilter->type == mnew->type
	  && prefix_same (&filter->prefix, &new->prefix))
	return mfilter;
    }
  return NULL;
}

int
vty_access_list_remark_unset (struct vty *vty, afi_t afi, char *name)
{
  struct access_list *access;

  access = access_list_lookup (afi, name);
  if (! access)
    {
      vty_out (vty, "%% access-list %s doesn't exist%s", name,
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (access->remark)
    {
      XFREE (MTYPE_TMP, access->remark);
      access->remark = NULL;
    }

  if (access->head == NULL && access->tail == NULL && access->remark == NULL)
    access_list_delete (access);

  return CMD_SUCCESS;
}

int
filter_set_cisco (struct vty *vty, char *name_str, char *type_str,
		  char *addr_str, char *addr_mask_str,
		  char *mask_str, char *mask_mask_str,
		  int extended, int set)
{
  int ret;
  enum filter_type type;
  struct filter *mfilter;
  struct filter_cisco *filter;
  struct access_list *access;
  struct in_addr addr;
  struct in_addr addr_mask;
  struct in_addr mask;
  struct in_addr mask_mask;

  /* Check of filter type. */
  if (strncmp (type_str, "p", 1) == 0)
    type = FILTER_PERMIT;
  else if (strncmp (type_str, "d", 1) == 0)
    type = FILTER_DENY;
  else
    {
      vty_out (vty, "%% filter type must be permit or deny%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  ret = inet_aton (addr_str, &addr);
  if (ret <= 0)
    {
      vty_out (vty, "%%Inconsistent address and mask%s",
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  ret = inet_aton (addr_mask_str, &addr_mask);
  if (ret <= 0)
    {
      vty_out (vty, "%%Inconsistent address and mask%s",
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  if (extended)
    {
      ret = inet_aton (mask_str, &mask);
      if (ret <= 0)
	{
	  vty_out (vty, "%%Inconsistent address and mask%s",
		   VTY_NEWLINE);
	  return CMD_WARNING;
	}

      ret = inet_aton (mask_mask_str, &mask_mask);
      if (ret <= 0)
	{
	  vty_out (vty, "%%Inconsistent address and mask%s",
		   VTY_NEWLINE);
	  return CMD_WARNING;
	}
    }

  mfilter = filter_new();
  mfilter->type = type;
  mfilter->cisco = 1;
  filter = &mfilter->u.cfilter;
  filter->extended = extended;
  filter->addr.s_addr = addr.s_addr & ~addr_mask.s_addr;
  filter->addr_mask.s_addr = addr_mask.s_addr;

  if (extended)
    {
      filter->mask.s_addr = mask.s_addr & ~mask_mask.s_addr;
      filter->mask_mask.s_addr = mask_mask.s_addr;
    }

  /* Install new filter to the access_list. */
  access = access_list_get (AFI_IP, name_str);

  if (set)
    {
      if (filter_lookup_cisco (access, mfilter))
	filter_free (mfilter);
      else
	access_list_filter_add (access, mfilter);
    }
  else
    {
      struct filter *delete_filter;

      delete_filter = filter_lookup_cisco (access, mfilter);
      if (delete_filter)
	access_list_filter_delete (access, delete_filter);

      filter_free (mfilter);
    }

  return CMD_SUCCESS;
}

/* Standard access-list */
DEFUN (access_list_standard,
       access_list_standard_cmd,
       "access-list (<1-99>|<1300-1999>) (deny|permit) A.B.C.D A.B.C.D",
       "Add an access list entry\n"
       "IP standard access list\n"
       "IP standard access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Address to match\n"
       "Wildcard bits\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], argv[2], argv[3],
			   NULL, NULL, 0, 1);
}

DEFUN (access_list_standard_nomask,
       access_list_standard_nomask_cmd,
       "access-list (<1-99>|<1300-1999>) (deny|permit) A.B.C.D",
       "Add an access list entry\n"
       "IP standard access list\n"
       "IP standard access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Address to match\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], argv[2], "0.0.0.0",
			   NULL, NULL, 0, 1);
}

DEFUN (access_list_standard_host,
       access_list_standard_host_cmd,
       "access-list (<1-99>|<1300-1999>) (deny|permit) host A.B.C.D",
       "Add an access list entry\n"
       "IP standard access list\n"
       "IP standard access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "A single host address\n"
       "Address to match\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], argv[2], "0.0.0.0",
			   NULL, NULL, 0, 1);
}

DEFUN (access_list_standard_any,
       access_list_standard_any_cmd,
       "access-list (<1-99>|<1300-1999>) (deny|permit) any",
       "Add an access list entry\n"
       "IP standard access list\n"
       "IP standard access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any source host\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], "0.0.0.0",
			   "255.255.255.255", NULL, NULL, 0, 1);
}

DEFUN (no_access_list_standard,
       no_access_list_standard_cmd,
       "no access-list (<1-99>|<1300-1999>) (deny|permit) A.B.C.D A.B.C.D",
       NO_STR
       "Add an access list entry\n"
       "IP standard access list\n"
       "IP standard access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Address to match\n"
       "Wildcard bits\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], argv[2], argv[3],
			   NULL, NULL, 0, 0);
}

DEFUN (no_access_list_standard_nomask,
       no_access_list_standard_nomask_cmd,
       "no access-list (<1-99>|<1300-1999>) (deny|permit) A.B.C.D",
       NO_STR
       "Add an access list entry\n"
       "IP standard access list\n"
       "IP standard access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Address to match\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], argv[2], "0.0.0.0",
			   NULL, NULL, 0, 0);
}

DEFUN (no_access_list_standard_host,
       no_access_list_standard_host_cmd,
       "no access-list (<1-99>|<1300-1999>) (deny|permit) host A.B.C.D",
       NO_STR
       "Add an access list entry\n"
       "IP standard access list\n"
       "IP standard access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "A single host address\n"
       "Address to match\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], argv[2], "0.0.0.0",
			   NULL, NULL, 0, 0);
}

DEFUN (no_access_list_standard_any,
       no_access_list_standard_any_cmd,
       "no access-list (<1-99>|<1300-1999>) (deny|permit) any",
       NO_STR
       "Add an access list entry\n"
       "IP standard access list\n"
       "IP standard access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any source host\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], "0.0.0.0",
			   "255.255.255.255", NULL, NULL, 0, 0);
}

/* Extended access-list */
DEFUN (access_list_extended,
       access_list_extended_cmd,
       "access-list (<100-199>|<2000-2699>) (deny|permit) ip A.B.C.D A.B.C.D A.B.C.D A.B.C.D",
       "Add an access list entry\n"
       "IP extended access list\n"
       "IP extended access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any Internet Protocol\n"
       "Source address\n"
       "Source wildcard bits\n"
       "Destination address\n"
       "Destination Wildcard bits\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], argv[2],
			   argv[3], argv[4], argv[5], 1 ,1);
}

DEFUN (access_list_extended_mask_any,
       access_list_extended_mask_any_cmd,
       "access-list (<100-199>|<2000-2699>) (deny|permit) ip A.B.C.D A.B.C.D any",
       "Add an access list entry\n"
       "IP extended access list\n"
       "IP extended access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any Internet Protocol\n"
       "Source address\n"
       "Source wildcard bits\n"
       "Any destination host\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], argv[2],
			   argv[3], "0.0.0.0",
			   "255.255.255.255", 1, 1);
}

DEFUN (access_list_extended_any_mask,
       access_list_extended_any_mask_cmd,
       "access-list (<100-199>|<2000-2699>) (deny|permit) ip any A.B.C.D A.B.C.D",
       "Add an access list entry\n"
       "IP extended access list\n"
       "IP extended access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any Internet Protocol\n"
       "Any source host\n"
       "Destination address\n"
       "Destination Wildcard bits\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], "0.0.0.0",
			   "255.255.255.255", argv[2],
			   argv[3], 1, 1);
}

DEFUN (access_list_extended_any_any,
       access_list_extended_any_any_cmd,
       "access-list (<100-199>|<2000-2699>) (deny|permit) ip any any",
       "Add an access list entry\n"
       "IP extended access list\n"
       "IP extended access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any Internet Protocol\n"
       "Any source host\n"
       "Any destination host\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], "0.0.0.0",
			   "255.255.255.255", "0.0.0.0",
			   "255.255.255.255", 1, 1);
}

DEFUN (access_list_extended_mask_host,
       access_list_extended_mask_host_cmd,
       "access-list (<100-199>|<2000-2699>) (deny|permit) ip A.B.C.D A.B.C.D host A.B.C.D",
       "Add an access list entry\n"
       "IP extended access list\n"
       "IP extended access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any Internet Protocol\n"
       "Source address\n"
       "Source wildcard bits\n"
       "A single destination host\n"
       "Destination address\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], argv[2],
			   argv[3], argv[4],
			   "0.0.0.0", 1, 1);
}

DEFUN (access_list_extended_host_mask,
       access_list_extended_host_mask_cmd,
       "access-list (<100-199>|<2000-2699>) (deny|permit) ip host A.B.C.D A.B.C.D A.B.C.D",
       "Add an access list entry\n"
       "IP extended access list\n"
       "IP extended access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any Internet Protocol\n"
       "A single source host\n"
       "Source address\n"
       "Destination address\n"
       "Destination Wildcard bits\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], argv[2],
			   "0.0.0.0", argv[3],
			   argv[4], 1, 1);
}

DEFUN (access_list_extended_host_host,
       access_list_extended_host_host_cmd,
       "access-list (<100-199>|<2000-2699>) (deny|permit) ip host A.B.C.D host A.B.C.D",
       "Add an access list entry\n"
       "IP extended access list\n"
       "IP extended access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any Internet Protocol\n"
       "A single source host\n"
       "Source address\n"
       "A single destination host\n"
       "Destination address\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], argv[2],
			   "0.0.0.0", argv[3],
			   "0.0.0.0", 1, 1);
}

DEFUN (access_list_extended_any_host,
       access_list_extended_any_host_cmd,
       "access-list (<100-199>|<2000-2699>) (deny|permit) ip any host A.B.C.D",
       "Add an access list entry\n"
       "IP extended access list\n"
       "IP extended access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any Internet Protocol\n"
       "Any source host\n"
       "A single destination host\n"
       "Destination address\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], "0.0.0.0",
			   "255.255.255.255", argv[2],
			   "0.0.0.0", 1, 1);
}

DEFUN (access_list_extended_host_any,
       access_list_extended_host_any_cmd,
       "access-list (<100-199>|<2000-2699>) (deny|permit) ip host A.B.C.D any",
       "Add an access list entry\n"
       "IP extended access list\n"
       "IP extended access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any Internet Protocol\n"
       "A single source host\n"
       "Source address\n"
       "Any destination host\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], argv[2],
			   "0.0.0.0", "0.0.0.0",
			   "255.255.255.255", 1, 1);
}

DEFUN (no_access_list_extended,
       no_access_list_extended_cmd,
       "no access-list (<100-199>|<2000-2699>) (deny|permit) ip A.B.C.D A.B.C.D A.B.C.D A.B.C.D",
       NO_STR
       "Add an access list entry\n"
       "IP extended access list\n"
       "IP extended access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any Internet Protocol\n"
       "Source address\n"
       "Source wildcard bits\n"
       "Destination address\n"
       "Destination Wildcard bits\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], argv[2],
			   argv[3], argv[4], argv[5], 1, 0);
}

DEFUN (no_access_list_extended_mask_any,
       no_access_list_extended_mask_any_cmd,
       "no access-list (<100-199>|<2000-2699>) (deny|permit) ip A.B.C.D A.B.C.D any",
       NO_STR
       "Add an access list entry\n"
       "IP extended access list\n"
       "IP extended access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any Internet Protocol\n"
       "Source address\n"
       "Source wildcard bits\n"
       "Any destination host\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], argv[2],
			   argv[3], "0.0.0.0",
			   "255.255.255.255", 1, 0);
}

DEFUN (no_access_list_extended_any_mask,
       no_access_list_extended_any_mask_cmd,
       "no access-list (<100-199>|<2000-2699>) (deny|permit) ip any A.B.C.D A.B.C.D",
       NO_STR
       "Add an access list entry\n"
       "IP extended access list\n"
       "IP extended access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any Internet Protocol\n"
       "Any source host\n"
       "Destination address\n"
       "Destination Wildcard bits\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], "0.0.0.0",
			   "255.255.255.255", argv[2],
			   argv[3], 1, 0);
}

DEFUN (no_access_list_extended_any_any,
       no_access_list_extended_any_any_cmd,
       "no access-list (<100-199>|<2000-2699>) (deny|permit) ip any any",
       NO_STR
       "Add an access list entry\n"
       "IP extended access list\n"
       "IP extended access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any Internet Protocol\n"
       "Any source host\n"
       "Any destination host\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], "0.0.0.0",
			   "255.255.255.255", "0.0.0.0",
			   "255.255.255.255", 1, 0);
}

DEFUN (no_access_list_extended_mask_host,
       no_access_list_extended_mask_host_cmd,
       "no access-list (<100-199>|<2000-2699>) (deny|permit) ip A.B.C.D A.B.C.D host A.B.C.D",
       NO_STR
       "Add an access list entry\n"
       "IP extended access list\n"
       "IP extended access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any Internet Protocol\n"
       "Source address\n"
       "Source wildcard bits\n"
       "A single destination host\n"
       "Destination address\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], argv[2],
			   argv[3], argv[4],
			   "0.0.0.0", 1, 0);
}

DEFUN (no_access_list_extended_host_mask,
       no_access_list_extended_host_mask_cmd,
       "no access-list (<100-199>|<2000-2699>) (deny|permit) ip host A.B.C.D A.B.C.D A.B.C.D",
       NO_STR
       "Add an access list entry\n"
       "IP extended access list\n"
       "IP extended access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any Internet Protocol\n"
       "A single source host\n"
       "Source address\n"
       "Destination address\n"
       "Destination Wildcard bits\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], argv[2],
			   "0.0.0.0", argv[3],
			   argv[4], 1, 0);
}

DEFUN (no_access_list_extended_host_host,
       no_access_list_extended_host_host_cmd,
       "no access-list (<100-199>|<2000-2699>) (deny|permit) ip host A.B.C.D host A.B.C.D",
       NO_STR
       "Add an access list entry\n"
       "IP extended access list\n"
       "IP extended access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any Internet Protocol\n"
       "A single source host\n"
       "Source address\n"
       "A single destination host\n"
       "Destination address\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], argv[2],
			   "0.0.0.0", argv[3],
			   "0.0.0.0", 1, 0);
}

DEFUN (no_access_list_extended_any_host,
       no_access_list_extended_any_host_cmd,
       "no access-list (<100-199>|<2000-2699>) (deny|permit) ip any host A.B.C.D",
       NO_STR
       "Add an access list entry\n"
       "IP extended access list\n"
       "IP extended access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any Internet Protocol\n"
       "Any source host\n"
       "A single destination host\n"
       "Destination address\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], "0.0.0.0",
			   "255.255.255.255", argv[2],
			   "0.0.0.0", 1, 0);
}

DEFUN (no_access_list_extended_host_any,
       no_access_list_extended_host_any_cmd,
       "no access-list (<100-199>|<2000-2699>) (deny|permit) ip host A.B.C.D any",
       NO_STR
       "Add an access list entry\n"
       "IP extended access list\n"
       "IP extended access list (expanded range)\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any Internet Protocol\n"
       "A single source host\n"
       "Source address\n"
       "Any destination host\n")
{
  return filter_set_cisco (vty, argv[0], argv[1], argv[2],
			   "0.0.0.0", "0.0.0.0",
			   "255.255.255.255", 1, 0);
}

int
filter_set_zebra (struct vty *vty, char *name_str, char *type_str,
		  afi_t afi, char *prefix_str, int exact, int set)
{
  int ret;
  enum filter_type type;
  struct filter *mfilter;
  struct filter_zebra *filter;
  struct access_list *access;
  struct prefix p;

  /* Check of filter type. */
  if (strncmp (type_str, "p", 1) == 0)
    type = FILTER_PERMIT;
  else if (strncmp (type_str, "d", 1) == 0)
    type = FILTER_DENY;
  else
    {
      vty_out (vty, "filter type must be [permit|deny]%s", VTY_NEWLINE);
      return CMD_WARNING;
    }

  /* Check string format of prefix and prefixlen. */
  if (afi == AFI_IP)
    {
      ret = str2prefix_ipv4 (prefix_str, (struct prefix_ipv4 *)&p);
      if (ret <= 0)
	{
	  vty_out (vty, "IP address prefix/prefixlen is malformed%s",
		   VTY_NEWLINE);
	  return CMD_WARNING;
	}
    }
#ifdef HAVE_IPV6
  else if (afi == AFI_IP6)
    {
      ret = str2prefix_ipv6 (prefix_str, (struct prefix_ipv6 *) &p);
      if (ret <= 0)
	{
	  vty_out (vty, "IPv6 address prefix/prefixlen is malformed%s",
		   VTY_NEWLINE);
		   return CMD_WARNING;
	}
    }
#endif /* HAVE_IPV6 */
  else
    return CMD_WARNING;

  mfilter = filter_new ();
  mfilter->type = type;
  filter = &mfilter->u.zfilter;
  prefix_copy (&filter->prefix, &p);

  /* "exact-match" */
  if (exact)
    filter->exact = 1;

  /* Install new filter to the access_list. */
  access = access_list_get (afi, name_str);

  if (set)
    {
      if (filter_lookup_zebra (access, mfilter))
	filter_free (mfilter);
      else
	access_list_filter_add (access, mfilter);
    }
  else
    {
      struct filter *delete_filter;

      delete_filter = filter_lookup_zebra (access, mfilter);
      if (delete_filter)
        access_list_filter_delete (access, delete_filter);

      filter_free (mfilter);
    }

  return CMD_SUCCESS;
}

/* Zebra access-list */
DEFUN (access_list,
       access_list_cmd,
       "access-list WORD (deny|permit) A.B.C.D/M",
       "Add an access list entry\n"
       "IP zebra access-list name\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Prefix to match. e.g. 10.0.0.0/8\n")
{
  return filter_set_zebra (vty, argv[0], argv[1], AFI_IP, argv[2], 0, 1);
}

DEFUN (access_list_exact,
       access_list_exact_cmd,
       "access-list WORD (deny|permit) A.B.C.D/M exact-match",
       "Add an access list entry\n"
       "IP zebra access-list name\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Prefix to match. e.g. 10.0.0.0/8\n"
       "Exact match of the prefixes\n")
{
  return filter_set_zebra (vty, argv[0], argv[1], AFI_IP, argv[2], 1, 1);
}

DEFUN (access_list_any,
       access_list_any_cmd,
       "access-list WORD (deny|permit) any",
       "Add an access list entry\n"
       "IP zebra access-list name\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Prefix to match. e.g. 10.0.0.0/8\n")
{
  return filter_set_zebra (vty, argv[0], argv[1], AFI_IP, "0.0.0.0/0", 0, 1);
}

DEFUN (no_access_list,
       no_access_list_cmd,
       "no access-list WORD (deny|permit) A.B.C.D/M",
       NO_STR
       "Add an access list entry\n"
       "IP zebra access-list name\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Prefix to match. e.g. 10.0.0.0/8\n")
{
  return filter_set_zebra (vty, argv[0], argv[1], AFI_IP, argv[2], 0, 0);
}

DEFUN (no_access_list_exact,
       no_access_list_exact_cmd,
       "no access-list WORD (deny|permit) A.B.C.D/M exact-match",
       NO_STR
       "Add an access list entry\n"
       "IP zebra access-list name\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Prefix to match. e.g. 10.0.0.0/8\n"
       "Exact match of the prefixes\n")
{
  return filter_set_zebra (vty, argv[0], argv[1], AFI_IP, argv[2], 1, 0);
}

DEFUN (no_access_list_any,
       no_access_list_any_cmd,
       "no access-list WORD (deny|permit) any",
       NO_STR
       "Add an access list entry\n"
       "IP zebra access-list name\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Prefix to match. e.g. 10.0.0.0/8\n")
{
  return filter_set_zebra (vty, argv[0], argv[1], AFI_IP, "0.0.0.0/0", 0, 0);
}

DEFUN (no_access_list_all,
       no_access_list_all_cmd,
       "no access-list (<1-99>|<100-199>|<1300-1999>|<2000-2699>|WORD)",
       NO_STR
       "Add an access list entry\n"
       "IP standard access list\n"
       "IP extended access list\n"
       "IP standard access list (expanded range)\n"
       "IP extended access list (expanded range)\n"
       "IP zebra access-list name\n")
{
  struct access_list *access;
  struct access_master *master;

  /* Looking up access_list. */
  access = access_list_lookup (AFI_IP, argv[0]);
  if (access == NULL)
    {
      vty_out (vty, "%% access-list %s doesn't exist%s", argv[0],
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  master = access->master;

  /* Delete all filter from access-list. */
  access_list_delete (access);

  /* Run hook function. */
  if (master->delete_hook)
    (*master->delete_hook) (access);
 
  return CMD_SUCCESS;
}

DEFUN (access_list_remark,
       access_list_remark_cmd,
       "access-list (<1-99>|<100-199>|<1300-1999>|<2000-2699>|WORD) remark .LINE",
       "Add an access list entry\n"
       "IP standard access list\n"
       "IP extended access list\n"
       "IP standard access list (expanded range)\n"
       "IP extended access list (expanded range)\n"
       "IP zebra access-list\n"
       "Access list entry comment\n"
       "Comment up to 100 characters\n")
{
  struct access_list *access;
  struct buffer *b;
  int i;

  access = access_list_get (AFI_IP, argv[0]);

  if (access->remark)
    {
      XFREE (MTYPE_TMP, access->remark);
      access->remark = NULL;
    }

  /* Below is remark get codes. */
  b = buffer_new (1024);
  for (i = 1; i < argc; i++)
    {
      buffer_putstr (b, (u_char *)argv[i]);
      buffer_putc (b, ' ');
    }
  buffer_putc (b, '\0');

  access->remark = buffer_getstr (b);

  buffer_free (b);

  return CMD_SUCCESS;
}

DEFUN (no_access_list_remark,
       no_access_list_remark_cmd,
       "no access-list (<1-99>|<100-199>|<1300-1999>|<2000-2699>|WORD) remark",
       NO_STR
       "Add an access list entry\n"
       "IP standard access list\n"
       "IP extended access list\n"
       "IP standard access list (expanded range)\n"
       "IP extended access list (expanded range)\n"
       "IP zebra access-list\n"
       "Access list entry comment\n")
{
  return vty_access_list_remark_unset (vty, AFI_IP, argv[0]);
}
	
ALIAS (no_access_list_remark,
       no_access_list_remark_arg_cmd,
       "no access-list (<1-99>|<100-199>|<1300-1999>|<2000-2699>|WORD) remark .LINE",
       NO_STR
       "Add an access list entry\n"
       "IP standard access list\n"
       "IP extended access list\n"
       "IP standard access list (expanded range)\n"
       "IP extended access list (expanded range)\n"
       "IP zebra access-list\n"
       "Access list entry comment\n"
       "Comment up to 100 characters\n");

#ifdef HAVE_IPV6
DEFUN (ipv6_access_list,
       ipv6_access_list_cmd,
       "ipv6 access-list WORD (deny|permit) X:X::X:X/M",
       IPV6_STR
       "Add an access list entry\n"
       "IPv6 zebra access-list\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Prefix to match. e.g. 3ffe:506::/32\n")
{
  return filter_set_zebra (vty, argv[0], argv[1], AFI_IP6, argv[2], 0, 1);
}

DEFUN (ipv6_access_list_exact,
       ipv6_access_list_exact_cmd,
       "ipv6 access-list WORD (deny|permit) X:X::X:X/M exact-match",
       IPV6_STR
       "Add an access list entry\n"
       "IPv6 zebra access-list\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Prefix to match. e.g. 3ffe:506::/32\n"
       "Exact match of the prefixes\n")
{
  return filter_set_zebra (vty, argv[0], argv[1], AFI_IP6, argv[2], 1, 1);
}

DEFUN (ipv6_access_list_any,
       ipv6_access_list_any_cmd,
       "ipv6 access-list WORD (deny|permit) any",
       IPV6_STR
       "Add an access list entry\n"
       "IPv6 zebra access-list\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any prefixi to match\n")
{
  return filter_set_zebra (vty, argv[0], argv[1], AFI_IP6, "::/0", 0, 1);
}

DEFUN (no_ipv6_access_list,
       no_ipv6_access_list_cmd,
       "no ipv6 access-list WORD (deny|permit) X:X::X:X/M",
       NO_STR
       IPV6_STR
       "Add an access list entry\n"
       "IPv6 zebra access-list\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Prefix to match. e.g. 3ffe:506::/32\n")
{
  return filter_set_zebra (vty, argv[0], argv[1], AFI_IP6, argv[2], 0, 0);
}

DEFUN (no_ipv6_access_list_exact,
       no_ipv6_access_list_exact_cmd,
       "no ipv6 access-list WORD (deny|permit) X:X::X:X/M exact-match",
       NO_STR
       IPV6_STR
       "Add an access list entry\n"
       "IPv6 zebra access-list\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Prefix to match. e.g. 3ffe:506::/32\n"
       "Exact match of the prefixes\n")
{
  return filter_set_zebra (vty, argv[0], argv[1], AFI_IP6, argv[2], 1, 0);
}

DEFUN (no_ipv6_access_list_any,
       no_ipv6_access_list_any_cmd,
       "no ipv6 access-list WORD (deny|permit) any",
       NO_STR
       IPV6_STR
       "Add an access list entry\n"
       "IPv6 zebra access-list\n"
       "Specify packets to reject\n"
       "Specify packets to forward\n"
       "Any prefixi to match\n")
{
  return filter_set_zebra (vty, argv[0], argv[1], AFI_IP6, "::/0", 0, 0);
}


DEFUN (no_ipv6_access_list_all,
       no_ipv6_access_list_all_cmd,
       "no ipv6 access-list WORD",
       NO_STR
       IPV6_STR
       "Add an access list entry\n"
       "IPv6 zebra access-list\n")
{
  struct access_list *access;
  struct access_master *master;

  /* Looking up access_list. */
  access = access_list_lookup (AFI_IP6, argv[0]);
  if (access == NULL)
    {
      vty_out (vty, "%% access-list %s doesn't exist%s", argv[0],
	       VTY_NEWLINE);
      return CMD_WARNING;
    }

  master = access->master;

  /* Delete all filter from access-list. */
  access_list_delete (access);

  /* Run hook function. */
  if (master->delete_hook)
    (*master->delete_hook) (access);

  return CMD_SUCCESS;
}

DEFUN (ipv6_access_list_remark,
       ipv6_access_list_remark_cmd,
       "ipv6 access-list WORD remark .LINE",
       IPV6_STR
       "Add an access list entry\n"
       "IPv6 zebra access-list\n"
       "Access list entry comment\n"
       "Comment up to 100 characters\n")
{
  struct access_list *access;
  struct buffer *b;
  int i;

  access = access_list_get (AFI_IP6, argv[0]);

  if (access->remark)
    {
      XFREE (MTYPE_TMP, access->remark);
      access->remark = NULL;
    }

  /* Below is remark get codes. */
  b = buffer_new (1024);
  for (i = 1; i < argc; i++)
    {
      buffer_putstr (b, (u_char *)argv[i]);
      buffer_putc (b, ' ');
    }
  buffer_putc (b, '\0');

  access->remark = buffer_getstr (b);

  buffer_free (b);

  return CMD_SUCCESS;
}

DEFUN (no_ipv6_access_list_remark,
       no_ipv6_access_list_remark_cmd,
       "no ipv6 access-list WORD remark",
       NO_STR
       IPV6_STR
       "Add an access list entry\n"
       "IPv6 zebra access-list\n"
       "Access list entry comment\n")
{
  return vty_access_list_remark_unset (vty, AFI_IP6, argv[0]);
}
	
ALIAS (no_ipv6_access_list_remark,
       no_ipv6_access_list_remark_arg_cmd,
       "no ipv6 access-list WORD remark .LINE",
       NO_STR
       IPV6_STR
       "Add an access list entry\n"
       "IPv6 zebra access-list\n"
       "Access list entry comment\n"
       "Comment up to 100 characters\n");
#endif /* HAVE_IPV6 */

void config_write_access_zebra (struct vty *, struct filter *);
void config_write_access_cisco (struct vty *, struct filter *);

/* show access-list command. */
int
filter_show (struct vty *vty, char *name, afi_t afi)
{
  struct access_list *access;
  struct access_master *master;
  struct filter *mfilter;
  struct filter_cisco *filter;
  int write = 0;

  master = access_master_get (afi);
  if (master == NULL)
    return 0;

  for (access = master->num.head; access; access = access->next)
    {
      if (name && strcmp (access->name, name) != 0)
	continue;

      write = 1;

      for (mfilter = access->head; mfilter; mfilter = mfilter->next)
	{
	  filter = &mfilter->u.cfilter;

	  if (write)
	    {
	      vty_out (vty, "%s IP%s access list %s%s",
		       mfilter->cisco ? 
		       (filter->extended ? "Extended" : "Standard") : "Zebra",
		       afi == AFI_IP6 ? "v6" : "",
		       access->name, VTY_NEWLINE);
	      write = 0;
	    }

	  vty_out (vty, "    %s%s", filter_type_str (mfilter),
		   mfilter->type == FILTER_DENY ? "  " : "");

	  if (! mfilter->cisco)
	    config_write_access_zebra (vty, mfilter);
	  else if (filter->extended)
	    config_write_access_cisco (vty, mfilter);
	  else
	    {
	      if (filter->addr_mask.s_addr == 0xffffffff)
		vty_out (vty, " any%s", VTY_NEWLINE);
	      else
		{
		  vty_out (vty, " %s", inet_ntoa (filter->addr));
		  if (filter->addr_mask.s_addr != 0)
		    vty_out (vty, ", wildcard bits %s", inet_ntoa (filter->addr_mask));
		  vty_out (vty, "%s", VTY_NEWLINE);
		}
	    }
	}
    }

  for (access = master->str.head; access; access = access->next)
    {
      if (name && strcmp (access->name, name) != 0)
	continue;

      write = 1;

      for (mfilter = access->head; mfilter; mfilter = mfilter->next)
	{
	  filter = &mfilter->u.cfilter;

	  if (write)
	    {
	      vty_out (vty, "%s IP%s access list %s%s",
		       mfilter->cisco ? 
		       (filter->extended ? "Extended" : "Standard") : "Zebra",
		       afi == AFI_IP6 ? "v6" : "",
		       access->name, VTY_NEWLINE);
	      write = 0;
	    }

	  vty_out (vty, "    %s%s", filter_type_str (mfilter),
		   mfilter->type == FILTER_DENY ? "  " : "");

	  if (! mfilter->cisco)
	    config_write_access_zebra (vty, mfilter);
	  else if (filter->extended)
	    config_write_access_cisco (vty, mfilter);
	  else
	    {
	      if (filter->addr_mask.s_addr == 0xffffffff)
		vty_out (vty, " any%s", VTY_NEWLINE);
	      else
		{
		  vty_out (vty, " %s", inet_ntoa (filter->addr));
		  if (filter->addr_mask.s_addr != 0)
		    vty_out (vty, ", wildcard bits %s", inet_ntoa (filter->addr_mask));
		  vty_out (vty, "%s", VTY_NEWLINE);
		}
	    }
	}
    }
  return CMD_SUCCESS;
}

DEFUN (show_ip_access_list,
       show_ip_access_list_cmd,
       "show ip access-list",
       SHOW_STR
       IP_STR
       "List IP access lists\n")
{
  return filter_show (vty, NULL, AFI_IP);
}

DEFUN (show_ip_access_list_name,
       show_ip_access_list_name_cmd,
       "show ip access-list (<1-99>|<100-199>|<1300-1999>|<2000-2699>|WORD)",
       SHOW_STR
       IP_STR
       "List IP access lists\n"
       "IP standard access list\n"
       "IP extended access list\n"
       "IP standard access list (expanded range)\n"
       "IP extended access list (expanded range)\n"
       "IP zebra access-list\n")
{
  return filter_show (vty, argv[0], AFI_IP);
}

#ifdef HAVE_IPV6
DEFUN (show_ipv6_access_list,
       show_ipv6_access_list_cmd,
       "show ipv6 access-list",
       SHOW_STR
       IPV6_STR
       "List IPv6 access lists\n")
{
  return filter_show (vty, NULL, AFI_IP6);
}

DEFUN (show_ipv6_access_list_name,
       show_ipv6_access_list_name_cmd,
       "show ipv6 access-list WORD",
       SHOW_STR
       IPV6_STR
       "List IPv6 access lists\n"
       "IPv6 zebra access-list\n")
{
  return filter_show (vty, argv[0], AFI_IP6);
}
#endif /* HAVE_IPV6 */

void
config_write_access_cisco (struct vty *vty, struct filter *mfilter)
{
  struct filter_cisco *filter;

  filter = &mfilter->u.cfilter;

  if (filter->extended)
    {
      vty_out (vty, " ip");
      if (filter->addr_mask.s_addr == 0xffffffff)
	vty_out (vty, " any");
      else if (filter->addr_mask.s_addr == 0)
	vty_out (vty, " host %s", inet_ntoa (filter->addr));
      else
	{
	  vty_out (vty, " %s", inet_ntoa (filter->addr));
	  vty_out (vty, " %s", inet_ntoa (filter->addr_mask));
        }

      if (filter->mask_mask.s_addr == 0xffffffff)
	vty_out (vty, " any");
      else if (filter->mask_mask.s_addr == 0)
	vty_out (vty, " host %s", inet_ntoa (filter->mask));
      else
	{
	  vty_out (vty, " %s", inet_ntoa (filter->mask));
	  vty_out (vty, " %s", inet_ntoa (filter->mask_mask));
	}
      vty_out (vty, "%s", VTY_NEWLINE);
    }
  else
    {
      if (filter->addr_mask.s_addr == 0xffffffff)
	vty_out (vty, " any%s", VTY_NEWLINE);
      else
	{
	  vty_out (vty, " %s", inet_ntoa (filter->addr));
	  if (filter->addr_mask.s_addr != 0)
	    vty_out (vty, " %s", inet_ntoa (filter->addr_mask));
	  vty_out (vty, "%s", VTY_NEWLINE);
	}
    }
}

void
config_write_access_zebra (struct vty *vty, struct filter *mfilter)
{
  struct filter_zebra *filter;
  struct prefix *p;
  char buf[BUFSIZ];

  filter = &mfilter->u.zfilter;
  p = &filter->prefix;

  if (p->prefixlen == 0 && ! filter->exact)
    vty_out (vty, " any");
  else
    vty_out (vty, " %s/%d%s",
	     inet_ntop (p->family, &p->u.prefix, buf, BUFSIZ),
	     p->prefixlen,
	     filter->exact ? " exact-match" : "");

  vty_out (vty, "%s", VTY_NEWLINE);
}

int
config_write_access (struct vty *vty, afi_t afi)
{
  struct access_list *access;
  struct access_master *master;
  struct filter *mfilter;
  int write = 0;

  master = access_master_get (afi);
  if (master == NULL)
    return 0;

  for (access = master->num.head; access; access = access->next)
    {
      if (access->remark)
	{
	  vty_out (vty, "%saccess-list %s remark %s%s",
		   afi == AFI_IP ? "" : "ipv6 ",
		   access->name, access->remark,
		   VTY_NEWLINE);
	  write++;
	}

      for (mfilter = access->head; mfilter; mfilter = mfilter->next)
	{
	  vty_out (vty, "%saccess-list %s %s",
	     afi == AFI_IP ? "" : "ipv6 ",
	     access->name,
	     filter_type_str (mfilter));

	  if (mfilter->cisco)
	    config_write_access_cisco (vty, mfilter);
	  else
	    config_write_access_zebra (vty, mfilter);

	  write++;
	}
    }

  for (access = master->str.head; access; access = access->next)
    {
      if (access->remark)
	{
	  vty_out (vty, "%saccess-list %s remark %s%s",
		   afi == AFI_IP ? "" : "ipv6 ",
		   access->name, access->remark,
		   VTY_NEWLINE);
	  write++;
	}

      for (mfilter = access->head; mfilter; mfilter = mfilter->next)
	{
	  vty_out (vty, "%saccess-list %s %s",
	     afi == AFI_IP ? "" : "ipv6 ",
	     access->name,
	     filter_type_str (mfilter));

	  if (mfilter->cisco)
	    config_write_access_cisco (vty, mfilter);
	  else
	    config_write_access_zebra (vty, mfilter);

	  write++;
	}
    }
  return write;
}

/* Access-list node. */
struct cmd_node access_node =
{
  ACCESS_NODE,
  "",				/* Access list has no interface. */
  1
};

int
config_write_access_ipv4 (struct vty *vty)
{
  return config_write_access (vty, AFI_IP);
}

void
access_list_reset_ipv4 ()
{
  struct access_list *access;
  struct access_list *next;
  struct access_master *master;

  master = access_master_get (AFI_IP);
  if (master == NULL)
    return;

  for (access = master->num.head; access; access = next)
    {
      next = access->next;
      access_list_delete (access);
    }
  for (access = master->str.head; access; access = next)
    {
      next = access->next;
      access_list_delete (access);
    }

  assert (master->num.head == NULL);
  assert (master->num.tail == NULL);

  assert (master->str.head == NULL);
  assert (master->str.tail == NULL);
}

/* Install vty related command. */
void
access_list_init_ipv4 ()
{
  install_node (&access_node, config_write_access_ipv4);

  install_element (ENABLE_NODE, &show_ip_access_list_cmd);
  install_element (ENABLE_NODE, &show_ip_access_list_name_cmd);

  /* Zebra access-list */
  install_element (CONFIG_NODE, &access_list_cmd);
  install_element (CONFIG_NODE, &access_list_exact_cmd);
  install_element (CONFIG_NODE, &access_list_any_cmd);
  install_element (CONFIG_NODE, &no_access_list_cmd);
  install_element (CONFIG_NODE, &no_access_list_exact_cmd);
  install_element (CONFIG_NODE, &no_access_list_any_cmd);

  /* Standard access-list */
  install_element (CONFIG_NODE, &access_list_standard_cmd);
  install_element (CONFIG_NODE, &access_list_standard_nomask_cmd);
  install_element (CONFIG_NODE, &access_list_standard_host_cmd);
  install_element (CONFIG_NODE, &access_list_standard_any_cmd);
  install_element (CONFIG_NODE, &no_access_list_standard_cmd);
  install_element (CONFIG_NODE, &no_access_list_standard_nomask_cmd);
  install_element (CONFIG_NODE, &no_access_list_standard_host_cmd);
  install_element (CONFIG_NODE, &no_access_list_standard_any_cmd);

  /* Extended access-list */
  install_element (CONFIG_NODE, &access_list_extended_cmd);
  install_element (CONFIG_NODE, &access_list_extended_any_mask_cmd);
  install_element (CONFIG_NODE, &access_list_extended_mask_any_cmd);
  install_element (CONFIG_NODE, &access_list_extended_any_any_cmd);
  install_element (CONFIG_NODE, &access_list_extended_host_mask_cmd);
  install_element (CONFIG_NODE, &access_list_extended_mask_host_cmd);
  install_element (CONFIG_NODE, &access_list_extended_host_host_cmd);
  install_element (CONFIG_NODE, &access_list_extended_any_host_cmd);
  install_element (CONFIG_NODE, &access_list_extended_host_any_cmd);
  install_element (CONFIG_NODE, &no_access_list_extended_cmd);
  install_element (CONFIG_NODE, &no_access_list_extended_any_mask_cmd);
  install_element (CONFIG_NODE, &no_access_list_extended_mask_any_cmd);
  install_element (CONFIG_NODE, &no_access_list_extended_any_any_cmd);
  install_element (CONFIG_NODE, &no_access_list_extended_host_mask_cmd);
  install_element (CONFIG_NODE, &no_access_list_extended_mask_host_cmd);
  install_element (CONFIG_NODE, &no_access_list_extended_host_host_cmd);
  install_element (CONFIG_NODE, &no_access_list_extended_any_host_cmd);
  install_element (CONFIG_NODE, &no_access_list_extended_host_any_cmd);

  install_element (CONFIG_NODE, &access_list_remark_cmd);
  install_element (CONFIG_NODE, &no_access_list_all_cmd);
  install_element (CONFIG_NODE, &no_access_list_remark_cmd);
  install_element (CONFIG_NODE, &no_access_list_remark_arg_cmd);
}

#ifdef HAVE_IPV6
struct cmd_node access_ipv6_node =
{
  ACCESS_IPV6_NODE,
  "",
  1
};

int
config_write_access_ipv6 (struct vty *vty)
{
  return config_write_access (vty, AFI_IP6);
}

void
access_list_reset_ipv6 ()
{
  struct access_list *access;
  struct access_list *next;
  struct access_master *master;

  master = access_master_get (AFI_IP6);
  if (master == NULL)
    return;

  for (access = master->num.head; access; access = next)
    {
      next = access->next;
      access_list_delete (access);
    }
  for (access = master->str.head; access; access = next)
    {
      next = access->next;
      access_list_delete (access);
    }

  assert (master->num.head == NULL);
  assert (master->num.tail == NULL);

  assert (master->str.head == NULL);
  assert (master->str.tail == NULL);
}

void
access_list_init_ipv6 ()
{
  install_node (&access_ipv6_node, config_write_access_ipv6);

  install_element (ENABLE_NODE, &show_ipv6_access_list_cmd);
  install_element (ENABLE_NODE, &show_ipv6_access_list_name_cmd);

  install_element (CONFIG_NODE, &ipv6_access_list_cmd);
  install_element (CONFIG_NODE, &ipv6_access_list_exact_cmd);
  install_element (CONFIG_NODE, &ipv6_access_list_any_cmd);
  install_element (CONFIG_NODE, &no_ipv6_access_list_exact_cmd);
  install_element (CONFIG_NODE, &no_ipv6_access_list_cmd);
  install_element (CONFIG_NODE, &no_ipv6_access_list_any_cmd);

  install_element (CONFIG_NODE, &no_ipv6_access_list_all_cmd);
  install_element (CONFIG_NODE, &ipv6_access_list_remark_cmd);
  install_element (CONFIG_NODE, &no_ipv6_access_list_remark_cmd);
  install_element (CONFIG_NODE, &no_ipv6_access_list_remark_arg_cmd);
}
#endif /* HAVE_IPV6 */

void
access_list_init ()
{
  access_list_init_ipv4 ();
#ifdef HAVE_IPV6
  access_list_init_ipv6();
#endif /* HAVE_IPV6 */
}

void
access_list_reset ()
{
  access_list_reset_ipv4 ();
#ifdef HAVE_IPV6
  access_list_reset_ipv6();
#endif /* HAVE_IPV6 */
}
