/* The IGEN simulator generator for GDB, the GNU Debugger.

   Copyright 2002, 2007 Free Software Foundation, Inc.

   Contributed by Andrew Cagney.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


#include <stdio.h>

#include "config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#include "misc.h"
#include "lf.h"
#include "filter.h"

struct _filter
{
  char *member;
  filter *next;
};


void
filter_parse (filter **filters, const char *filt)
{
  while (strlen (filt) > 0)
    {
      filter *new_filter;
      filter **last;
      /* break out a member of the filter list */
      const char *flag = filt;
      unsigned /*size_t */ len;
      filt = strchr (filt, ',');
      if (filt == NULL)
	{
	  filt = strchr (flag, '\0');
	  len = strlen (flag);
	}
      else
	{
	  len = filt - flag;
	  filt = filt + 1;
	}
      /* find an insertion point - sorted order */
      last = filters;
      while (*last != NULL && strncmp (flag, (*last)->member, len) > 0)
	last = &(*last)->next;
      if (*last != NULL
	  && strncmp (flag, (*last)->member, len) == 0
	  && strlen ((*last)->member) == len)
	continue;		/* duplicate */
      /* create an entry for that member */
      new_filter = ZALLOC (filter);
      new_filter->member = NZALLOC (char, len + 1);
      strncpy (new_filter->member, flag, len);
      /* insert it */
      new_filter->next = *last;
      *last = new_filter;
    }
}


void
filter_add (filter **set, filter *add)
{
  while (add != NULL)
    {
      int cmp;
      if (*set == NULL)
	cmp = 1;		/* set->member > add->member */
      else
	cmp = strcmp ((*set)->member, add->member);
      if (cmp > 0)
	{
	  /* insert it here */
	  filter *new = ZALLOC (filter);
	  new->member = NZALLOC (char, strlen (add->member) + 1);
	  strcpy (new->member, add->member);
	  new->next = *set;
	  *set = new;
	  add = add->next;
	}
      else if (cmp == 0)
	{
	  /* already in set */
	  add = add->next;
	}
      else			/* cmp < 0 */
	{
	  /* not reached insertion point */
	  set = &(*set)->next;
	}
    }
}


int
filter_is_subset (filter *superset, filter *subset)
{
  while (1)
    {
      int cmp;
      if (subset == NULL)
	return 1;
      if (superset == NULL)
	return 0;		/* subset isn't finished */
      cmp = strcmp (subset->member, superset->member);
      if (cmp < 0)
	return 0;		/* not found */
      else if (cmp == 0)
	subset = subset->next;	/* found */
      else if (cmp > 0)
	superset = superset->next;	/* later in list? */
    }
}


int
filter_is_common (filter *l, filter *r)
{
  while (1)
    {
      int cmp;
      if (l == NULL)
	return 0;
      if (r == NULL)
	return 0;
      cmp = strcmp (l->member, r->member);
      if (cmp < 0)
	l = l->next;
      else if (cmp == 0)
	return 1;		/* common member */
      else if (cmp > 0)
	r = r->next;
    }
}


int
filter_is_member (filter *filt, const char *flag)
{
  int index = 1;
  while (filt != NULL)
    {
      if (strcmp (flag, filt->member) == 0)
	return index;
      filt = filt->next;
      index++;
    }
  return 0;
}


int
is_filtered_out (filter *filters, const char *flags)
{
  while (strlen (flags) > 0)
    {
      int present;
      filter *filt = filters;
      /* break the string up */
      char *end = strchr (flags, ',');
      char *next;
      unsigned /*size_t */ len;
      if (end == NULL)
	{
	  end = strchr (flags, '\0');
	  next = end;
	}
      else
	{
	  next = end + 1;
	}
      len = end - flags;
      /* check that it is present */
      present = 0;
      filt = filters;
      while (filt != NULL)
	{
	  if (strncmp (flags, filt->member, len) == 0
	      && strlen (filt->member) == len)
	    {
	      present = 1;
	      break;
	    }
	  filt = filt->next;
	}
      if (!present)
	return 1;
      flags = next;
    }
  return 0;
}


#if 0
int
it_is (const char *flag, const char *flags)
{
  int flag_len = strlen (flag);
  while (*flags != '\0')
    {
      if (!strncmp (flags, flag, flag_len)
	  && (flags[flag_len] == ',' || flags[flag_len] == '\0'))
	return 1;
      while (*flags != ',')
	{
	  if (*flags == '\0')
	    return 0;
	  flags++;
	}
      flags++;
    }
  return 0;
}
#endif


char *
filter_next (filter *set, char *member)
{
  while (set != NULL)
    {
      if (strcmp (set->member, member) > 0)
	return set->member;
      set = set->next;
    }
  return NULL;
}


void
dump_filter (lf *file, char *prefix, filter *set, char *suffix)
{
  char *member;
  lf_printf (file, "%s", prefix);
  member = filter_next (set, "");
  if (member != NULL)
    {
      while (1)
	{
	  lf_printf (file, "%s", member);
	  member = filter_next (set, member);
	  if (member == NULL)
	    break;
	  lf_printf (file, ",");
	}
    }
  lf_printf (file, "%s", suffix);
}


#ifdef MAIN
int
main (int argc, char **argv)
{
  filter *subset = NULL;
  filter *superset = NULL;
  lf *l;
  int i;
  if (argc < 2)
    {
      printf ("Usage: filter <subset> <filter> ...\n");
      exit (1);
    }

  /* load the filter up */
  filter_parse (&subset, argv[1]);
  for (i = 2; i < argc; i++)
    filter_parse (&superset, argv[i]);

  /* dump various info */
  l = lf_open ("-", "stdout", lf_omit_references, lf_is_text, "tmp-filter");
#if 0
  if (is_filtered_out (argv[1], superset))
    lf_printf (l, "excluded\n");
  else
    lf_printf (l, "included\n");
#endif
  /* subset */
  {
    dump_filter (l, "{", subset, " }");
    if (filter_is_subset (superset, subset))
      lf_printf (l, " subset of ");
    else
      lf_printf (l, " !subset of ");
    dump_filter (l, "{", superset, " }");
    lf_printf (l, "\n");
  }
  /* intersection */
  {
    dump_filter (l, "{", subset, " }");
    if (filter_is_common (subset, superset))
      lf_printf (l, " intersects ");
    else
      lf_printf (l, " !intersects ");
    dump_filter (l, "{", superset, " }");
    lf_printf (l, "\n");
  }
  /* membership */
  {
    filter *memb = subset;
    while (memb != NULL)
      {
	lf_printf (l, "%s", memb->member);
	if (filter_is_member (superset, memb->member))
	  lf_printf (l, " in ");
	else
	  lf_printf (l, " !in ");
	dump_filter (l, "{", superset, " }");
	lf_printf (l, "\n");
	memb = memb->next;
      }
  }
  /* addition */
  {
    filter *add = NULL;
    filter_add (&add, superset);
    filter_add (&add, subset);
    dump_filter (l, "{", add, " }");
    lf_printf (l, " = ");
    dump_filter (l, "{", subset, " }");
    lf_printf (l, " + ");
    dump_filter (l, "{", superset, " }");
    lf_printf (l, "\n");
  }

  return 0;
}
#endif
