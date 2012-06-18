/* BGP community-list and extcommunity-list.
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
#include "prefix.h"
#include "memory.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_community.h"
#include "bgpd/bgp_ecommunity.h"
#include "bgpd/bgp_aspath.h"
#include "bgpd/bgp_regex.h"
#include "bgpd/bgp_clist.h"

/* Lookup master structure for community-list or
   extcommunity-list.  */
struct community_list_master *
community_list_master_lookup (struct community_list_handler *ch, int master)
{
  if (ch)
    switch (master)
      {
      case COMMUNITY_LIST_MASTER:
	return &ch->community_list;
	break;
      case EXTCOMMUNITY_LIST_MASTER:
	return &ch->extcommunity_list;
      }
  return NULL;
}

/* Allocate a new community list entry.  */
struct community_entry *
community_entry_new ()
{
  struct community_entry *new;

  new = XMALLOC (MTYPE_COMMUNITY_LIST_ENTRY, sizeof (struct community_entry));
  memset (new, 0, sizeof (struct community_entry));
  return new;
}

/* Free community list entry.  */
void
community_entry_free (struct community_entry *entry)
{
  switch (entry->style)
    {
    case COMMUNITY_LIST_STANDARD:
      if (entry->u.com)
	community_free (entry->u.com);
      break;
    case EXTCOMMUNITY_LIST_STANDARD:
      /* In case of standard extcommunity-list, configuration string
	 is made by ecommunity_ecom2str().  */
      if (entry->config)
	XFREE (MTYPE_ECOMMUNITY_STR, entry->config);
      if (entry->u.ecom)
	ecommunity_free (entry->u.ecom);
      break;
    case COMMUNITY_LIST_EXPANDED:
    case EXTCOMMUNITY_LIST_EXPANDED:
      if (entry->config)
	XFREE (MTYPE_COMMUNITY_LIST_CONFIG, entry->config);
      if (entry->reg)
	bgp_regex_free (entry->reg);
    default:
      break;
    }
  XFREE (MTYPE_COMMUNITY_LIST_ENTRY, entry);
}

/* Allocate a new community-list.  */
struct community_list *
community_list_new ()
{
  struct community_list *new;

  new = XMALLOC (MTYPE_COMMUNITY_LIST, sizeof (struct community_list));
  memset (new, 0, sizeof (struct community_list));
  return new;
}

/* Free community-list.  */
void
community_list_free (struct community_list *list)
{
  if (list->name)
    XFREE (MTYPE_COMMUNITY_LIST_NAME, list->name);
  XFREE (MTYPE_COMMUNITY_LIST, list);
}

struct community_list *
community_list_insert (struct community_list_handler *ch,
		       char *name, int master)
{
  int i;
  long number;
  struct community_list *new;
  struct community_list *point;
  struct community_list_list *list;
  struct community_list_master *cm;

  /* Lookup community-list master.  */
  cm = community_list_master_lookup (ch, master);
  if (! cm)
    return NULL;

  /* Allocate new community_list and copy given name. */
  new = community_list_new ();
  new->name = XSTRDUP (MTYPE_COMMUNITY_LIST_NAME, name);

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
      new->sort = COMMUNITY_LIST_NUMBER;

      /* Set access_list to number list. */
      list = &cm->num;

      for (point = list->head; point; point = point->next)
	if (atol (point->name) >= number)
	  break;
    }
  else
    {
      new->sort = COMMUNITY_LIST_STRING;

      /* Set access_list to string list. */
      list = &cm->str;
  
      /* Set point to insertion point. */
      for (point = list->head; point; point = point->next)
	if (strcmp (point->name, name) >= 0)
	  break;
    }

  /* Link to upper list.  */
  new->parent = list;

  /* In case of this is the first element of master. */
  if (list->head == NULL)
    {
      list->head = list->tail = new;
      return new;
    }

  /* In case of insertion is made at the tail of access_list. */
  if (point == NULL)
    {
      new->prev = list->tail;
      list->tail->next = new;
      list->tail = new;
      return new;
    }

  /* In case of insertion is made at the head of access_list. */
  if (point == list->head)
    {
      new->next = list->head;
      list->head->prev = new;
      list->head = new;
      return new;
    }

  /* Insertion is made at middle of the access_list. */
  new->next = point;
  new->prev = point->prev;

  if (point->prev)
    point->prev->next = new;
  point->prev = new;

  return new;
}

struct community_list *
community_list_lookup (struct community_list_handler *ch,
		       char *name, int master)
{
  struct community_list *list;
  struct community_list_master *cm;

  if (! name)
    return NULL;

  cm = community_list_master_lookup (ch, master);
  if (! cm)
    return NULL;

  for (list = cm->num.head; list; list = list->next)
    if (strcmp (list->name, name) == 0)
      return list;
  for (list = cm->str.head; list; list = list->next)
    if (strcmp (list->name, name) == 0)
      return list;

  return NULL;
}

struct community_list *
community_list_get (struct community_list_handler *ch, char *name, int master)
{
  struct community_list *list;

  list = community_list_lookup (ch, name, master);
  if (! list)
    list = community_list_insert (ch, name, master);
  return list;
}

void
community_list_delete (struct community_list *list)
{
  struct community_list_list *clist;
  struct community_entry *entry, *next;

  for (entry = list->head; entry; entry = next)
    {
      next = entry->next;
      community_entry_free (entry);
    }

  clist = list->parent;

  if (list->next)
    list->next->prev = list->prev;
  else
    clist->tail = list->prev;

  if (list->prev)
    list->prev->next = list->next;
  else
    clist->head = list->next;

  community_list_free (list);
}

int
community_list_empty_p (struct community_list *list)
{
  return (list->head == NULL && list->tail == NULL) ? 1 : 0;
}

/* Add community-list entry to the list.  */
static void
community_list_entry_add (struct community_list *list, 
			  struct community_entry *entry)
{
  entry->next = NULL;
  entry->prev = list->tail;

  if (list->tail)
    list->tail->next = entry;
  else
    list->head = entry;
  list->tail = entry;
}

/* Delete community-list entry from the list.  */
static void
community_list_entry_delete (struct community_list *list,
			     struct community_entry *entry, int style)
{
  if (entry->next)
    entry->next->prev = entry->prev;
  else
    list->tail = entry->prev;

  if (entry->prev)
    entry->prev->next = entry->next;
  else
    list->head = entry->next;

  community_entry_free (entry);

  if (community_list_empty_p (list))
    community_list_delete (list);
}

/* Lookup community-list entry from the list.  */
static struct community_entry *
community_list_entry_lookup (struct community_list *list, void *arg,
			     int direct)
{
  struct community_entry *entry;

  for (entry = list->head; entry; entry = entry->next)
    {
      switch (entry->style)
	{
	case COMMUNITY_LIST_STANDARD:
	  if (community_cmp (entry->u.com, arg))
	    return entry;
	  break;
	case EXTCOMMUNITY_LIST_STANDARD:
	  if (ecommunity_cmp (entry->u.ecom, arg))
	    return entry;
	  break;
	case COMMUNITY_LIST_EXPANDED:
	case EXTCOMMUNITY_LIST_EXPANDED:
	  if (strcmp (entry->config, arg) == 0)
	    return entry;
	  break;
	default:
	  break;
	}
    }
  return NULL;
}

/* Internal function to perform regular expression match for community
   attribute.  */
static int
community_regexp_match (struct community *com, regex_t *reg)
{
  char *str;

  /* When there is no communities attribute it is treated as empty
     string.  */
  if (com == NULL || com->size == 0)
    str = "";
  else
    str = community_str (com);

  /* Regular expression match.  */
  if (regexec (reg, str, 0, NULL, 0) == 0)
    return 1;

  /* No match.  */
  return 0;
}

static int
ecommunity_regexp_match (struct ecommunity *ecom, regex_t *reg)
{
  char *str;

  /* When there is no communities attribute it is treated as empty
     string.  */
  if (ecom == NULL || ecom->size == 0)
    str = "";
  else
    str = ecommunity_str (ecom);

  /* Regular expression match.  */
  if (regexec (reg, str, 0, NULL, 0) == 0)
    return 1;

  /* No match.  */
  return 0;
}

/* When given community attribute matches to the community-list return
   1 else return 0.  */
int
community_list_match (struct community *com, struct community_list *list)
{
  struct community_entry *entry;

  for (entry = list->head; entry; entry = entry->next)
    {
      if (entry->any)
	return entry->direct == COMMUNITY_PERMIT ? 1 : 0;

      if (entry->style == COMMUNITY_LIST_STANDARD)
	{
	  if (community_include (entry->u.com, COMMUNITY_INTERNET))
	    return entry->direct == COMMUNITY_PERMIT ? 1 : 0;

	  if (community_match (com, entry->u.com))
	    return entry->direct == COMMUNITY_PERMIT ? 1 : 0;
	}
      else if (entry->style == COMMUNITY_LIST_EXPANDED)
	{
	  if (community_regexp_match (com, entry->reg))
	    return entry->direct == COMMUNITY_PERMIT ? 1 : 0;
	}
    }
  return 0;
}

int
ecommunity_list_match (struct ecommunity *ecom, struct community_list *list)
{
  struct community_entry *entry;

  for (entry = list->head; entry; entry = entry->next)
    {
      if (entry->any)
	return entry->direct == COMMUNITY_PERMIT ? 1 : 0;

      if (entry->style == EXTCOMMUNITY_LIST_STANDARD)
	{
	  if (ecommunity_match (ecom, entry->u.ecom))
	    return entry->direct == COMMUNITY_PERMIT ? 1 : 0;
	}
      else if (entry->style == EXTCOMMUNITY_LIST_EXPANDED)
	{
	  if (ecommunity_regexp_match (ecom, entry->reg))
	    return entry->direct == COMMUNITY_PERMIT ? 1 : 0;
	}
    }
  return 0;
}

/* Perform exact matching.  In case of expanded community-list, do
   same thing as community_list_match().  */
int
community_list_exact_match (struct community *com, struct community_list *list)
{
  struct community_entry *entry;

  for (entry = list->head; entry; entry = entry->next)
    {
      if (entry->any)
	return entry->direct == COMMUNITY_PERMIT ? 1 : 0;

      if (entry->style == COMMUNITY_LIST_STANDARD)
	{
	  if (community_include (entry->u.com, COMMUNITY_INTERNET))
	    return entry->direct == COMMUNITY_PERMIT ? 1 : 0;

	  if (community_cmp (com, entry->u.com))
	    return entry->direct == COMMUNITY_PERMIT ? 1 : 0;
	}
      else if (entry->style == COMMUNITY_LIST_EXPANDED)
	{
	  if (community_regexp_match (com, entry->reg))
	    return entry->direct == COMMUNITY_PERMIT ? 1 : 0;
	}
    }
  return 0;
}

/* Do regular expression matching with single community val.  */
static int
comval_regexp_match (u_int32_t comval, regex_t *reg)
{
  /* Maximum is "65535:65535" + '\0'. */
  char c[12];
  char *str;

  switch (comval)
    {
    case COMMUNITY_INTERNET:
      str = "internet";
      break;
    case COMMUNITY_NO_EXPORT:
      str = "no-export";
      break;
    case COMMUNITY_NO_ADVERTISE:
      str = "no-advertise";
      break;
    case COMMUNITY_LOCAL_AS:
      str = "local-AS";
      break;
    default:
      snprintf (c, sizeof c,
		"%d:%d", (comval >> 16) & 0xFFFF, comval & 0xFFFF);
      str = c;
      break;
    }

  if (regexec (reg, str, 0, NULL, 0) == 0)
    return 1;
  
  return 0;
}

/* Delete all permitted communities in the list from com.  */
struct community *
community_list_match_delete (struct community *com,
                             struct community_list *clist)
{
  int i;
  u_int32_t comval;
  struct community *merge;
  struct community_entry *entry;

  /* Empty community value check.  */
  if (! com)
    return NULL;

  /* Duplicate communities value.  */
  merge = community_dup (com);

  /* For each communities value, we have to check each
     community-list.  */
  for (i = 0; i < com->size; i ++)
    {
      /* Get one communities value.  */
      memcpy (&comval, com_nthval (com, i), sizeof (u_int32_t));
      comval = ntohl (comval);

      /* Loop community-list.  */
      for (entry = clist->head; entry; entry = entry->next)
	{
	  /* Various match condition check.  */
	  if (entry->any
	      || (entry->style == COMMUNITY_LIST_STANDARD
		  && entry->u.com
		  && community_include (entry->u.com, comval))
	      || (entry->style == COMMUNITY_LIST_EXPANDED
		  && entry->reg
		  && comval_regexp_match (comval, entry->reg)))
	    {
	      /* If the rule is "permit", delete this community value. */
	      if (entry->direct == COMMUNITY_PERMIT)
		community_del_val (merge, com_nthval (com, i));

	      /* Exit community-list loop, goto next communities
		 value.  */
	      break;
	    }
        }		  
    }
  return merge;
}

/* To avoid duplicated entry in the community-list, this function
   compares specified entry to existing entry.  */
int
community_list_dup_check (struct community_list *list, 
			  struct community_entry *new)
{
  struct community_entry *entry;
  
  for (entry = list->head; entry; entry = entry->next)
    {
      if (entry->style != new->style)
	continue;

      if (entry->direct != new->direct)
	continue;

      if (entry->any != new->any)
	continue;

      if (entry->any)
	return 1;

      switch (entry->style)
	{
	case COMMUNITY_LIST_STANDARD:
	  if (community_cmp (entry->u.com, new->u.com))
	    return 1;
	  break;
	case EXTCOMMUNITY_LIST_STANDARD:
	  if (ecommunity_cmp (entry->u.ecom, new->u.ecom))
	    return 1;
	  break;
	case COMMUNITY_LIST_EXPANDED:
	case EXTCOMMUNITY_LIST_EXPANDED:
	  if (strcmp (entry->config, new->config) == 0)
	    return 1;
	  break;
	default:
	  break;
	}
    }
  return 0;
}

/* Set community-list.  */
int
community_list_set (struct community_list_handler *ch,
		    char *name, char *str, int direct, int style)
{
  struct community_entry *entry = NULL;
  struct community_list *list;
  struct community *com = NULL;
  regex_t *regex = NULL;

  /* Get community list. */
  list = community_list_get (ch, name, COMMUNITY_LIST_MASTER);

  /* When community-list already has entry, new entry should have same
     style.  If you want to have mixed style community-list, you can
     comment out this check.  */
  if (! community_list_empty_p (list))
    {
      struct community_entry *first;

      first = list->head;

      if (style != first->style)
	{
	  return (first->style == COMMUNITY_LIST_STANDARD
		  ? COMMUNITY_LIST_ERR_STANDARD_CONFLICT
		  : COMMUNITY_LIST_ERR_EXPANDED_CONFLICT);
	}
    }

  if (str)
    {
      if (style == COMMUNITY_LIST_STANDARD)
	com = community_str2com (str);
      else
	regex = bgp_regcomp (str);

      if (! com && ! regex)
	return COMMUNITY_LIST_ERR_MALFORMED_VAL;
    }

  entry = community_entry_new ();
  entry->direct = direct;
  entry->style = style;
  entry->any = (str ? 0 : 1);
  entry->u.com = com;
  entry->reg = regex;
  entry->config = (regex ? XSTRDUP (MTYPE_COMMUNITY_LIST_CONFIG, str) : NULL);

  /* Do not put duplicated community entry.  */
  if (community_list_dup_check (list, entry))
    community_entry_free (entry);
  else
    community_list_entry_add (list, entry);

  return 0;
}

/* Unset community-list.  When str is NULL, delete all of
   community-list entry belongs to the specified name.  */
int
community_list_unset (struct community_list_handler *ch,
		      char *name, char *str, int direct, int style)
{
  struct community_entry *entry = NULL;
  struct community_list *list;
  struct community *com = NULL;
  regex_t *regex = NULL;

  /* Lookup community list.  */
  list = community_list_lookup (ch, name, COMMUNITY_LIST_MASTER);
  if (list == NULL)
    return COMMUNITY_LIST_ERR_CANT_FIND_LIST;

  /* Delete all of entry belongs to this community-list.  */
  if (! str)
    {
      community_list_delete (list);
      return 0;
    }

  if (style == COMMUNITY_LIST_STANDARD)
    com = community_str2com (str);
  else
    regex = bgp_regcomp (str);

  if (! com && ! regex)
    return COMMUNITY_LIST_ERR_MALFORMED_VAL;

  if (com)
    entry = community_list_entry_lookup (list, com, direct);
  else
    entry = community_list_entry_lookup (list, str, direct);

  if (com)
    community_free (com);
  if (regex)
    bgp_regex_free (regex);

  if (! entry)
    return COMMUNITY_LIST_ERR_CANT_FIND_LIST;

  community_list_entry_delete (list, entry, style);

  return 0;
}

/* Set extcommunity-list.  */
int
extcommunity_list_set (struct community_list_handler *ch,
		       char *name, char *str, int direct, int style)
{
  struct community_entry *entry = NULL;
  struct community_list *list;
  struct ecommunity *ecom = NULL;
  regex_t *regex = NULL;

  entry = NULL;

  /* Get community list. */
  list = community_list_get (ch, name, EXTCOMMUNITY_LIST_MASTER);

  /* When community-list already has entry, new entry should have same
     style.  If you want to have mixed style community-list, you can
     comment out this check.  */
  if (! community_list_empty_p (list))
    {
      struct community_entry *first;

      first = list->head;

      if (style != first->style)
	{
	  return (first->style == EXTCOMMUNITY_LIST_STANDARD
		  ? COMMUNITY_LIST_ERR_STANDARD_CONFLICT
		  : COMMUNITY_LIST_ERR_EXPANDED_CONFLICT);
	}
    }

  if (str)
    {
      if (style == EXTCOMMUNITY_LIST_STANDARD)
	ecom = ecommunity_str2com (str, 0, 1);
      else
	regex = bgp_regcomp (str);

      if (! ecom && ! regex)
	return COMMUNITY_LIST_ERR_MALFORMED_VAL;
    }

  if (ecom)
    ecom->str = ecommunity_ecom2str (ecom, ECOMMUNITY_FORMAT_DISPLAY);

  entry = community_entry_new ();
  entry->direct = direct;
  entry->style = style;
  entry->any = (str ? 0 : 1);
  if (ecom)
    entry->config = ecommunity_ecom2str (ecom, ECOMMUNITY_FORMAT_CONFIG);
  else if (regex)
    entry->config = XSTRDUP (MTYPE_COMMUNITY_LIST_CONFIG, str);
  else
    entry->config = NULL;
  entry->u.ecom = ecom;
  entry->reg = regex;

  /* Do not put duplicated community entry.  */
  if (community_list_dup_check (list, entry))
    community_entry_free (entry);
  else
    community_list_entry_add (list, entry);

  return 0;
}

/* Unset extcommunity-list.  When str is NULL, delete all of
   extcommunity-list entry belongs to the specified name.  */
int
extcommunity_list_unset (struct community_list_handler *ch,
			 char *name, char *str, int direct, int style)
{
  struct community_entry *entry = NULL;
  struct community_list *list;
  struct ecommunity *ecom = NULL;
  regex_t *regex = NULL;

  /* Lookup extcommunity list.  */
  list = community_list_lookup (ch, name, EXTCOMMUNITY_LIST_MASTER);
  if (list == NULL)
    return COMMUNITY_LIST_ERR_CANT_FIND_LIST;

  /* Delete all of entry belongs to this extcommunity-list.  */
  if (! str)
    {
      community_list_delete (list);
      return 0;
    }

  if (style == EXTCOMMUNITY_LIST_STANDARD)
    ecom = ecommunity_str2com (str, 0, 1);
  else
    regex = bgp_regcomp (str);

  if (! ecom && ! regex)
    return COMMUNITY_LIST_ERR_MALFORMED_VAL;

  if (ecom)
    entry = community_list_entry_lookup (list, ecom, direct);
  else
    entry = community_list_entry_lookup (list, str, direct);

  if (ecom)
    ecommunity_free (ecom);
  if (regex)
    bgp_regex_free (regex);

  if (! entry)
    return COMMUNITY_LIST_ERR_CANT_FIND_LIST;

  community_list_entry_delete (list, entry, style);

  return 0;
}

/* Initializa community-list.  Return community-list handler.  */
struct community_list_handler *
community_list_init ()
{
  struct community_list_handler *ch;
  ch = XCALLOC (MTYPE_COMMUNITY_LIST_HANDLER,
		sizeof (struct community_list_handler));
  return ch;
}

/* Terminate community-list.  */
void
community_list_terminate (struct community_list_handler *ch)
{
  struct community_list_master *cm;
  struct community_list *list;

  cm = &ch->community_list;
  while ((list = cm->num.head) != NULL)
    community_list_delete (list);
  while ((list = cm->str.head) != NULL)
    community_list_delete (list);

  cm = &ch->extcommunity_list;
  while ((list = cm->num.head) != NULL)
    community_list_delete (list);
  while ((list = cm->str.head) != NULL)
    community_list_delete (list);

  XFREE (MTYPE_COMMUNITY_LIST_HANDLER, ch);
}
