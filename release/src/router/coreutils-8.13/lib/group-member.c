/* group-member.c -- determine whether group id is in calling user's group list

   Copyright (C) 1994, 1997-1998, 2003, 2005-2006, 2009-2011 Free Software
   Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include <unistd.h>

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>

#include "xalloc.h"

struct group_info
  {
    int n_groups;
    gid_t *group;
  };

static void
free_group_info (struct group_info const *g)
{
  free (g->group);
}

static bool
get_group_info (struct group_info *gi)
{
  int n_groups;
  int n_group_slots = getgroups (0, NULL);
  gid_t *group;

  if (n_group_slots < 0)
    return false;

  /* Avoid xnmalloc, as it goes awry when SIZE_MAX < n_group_slots.  */
  if (xalloc_oversized (n_group_slots, sizeof *group))
    xalloc_die ();
  group = xmalloc (n_group_slots * sizeof *group);
  n_groups = getgroups (n_group_slots, group);

  /* In case of error, the user loses. */
  if (n_groups < 0)
    {
      free (group);
      return false;
    }

  gi->n_groups = n_groups;
  gi->group = group;

  return true;
}

/* Return non-zero if GID is one that we have in our groups list.
   Note that the groups list is not guaranteed to contain the current
   or effective group ID, so they should generally be checked
   separately.  */

int
group_member (gid_t gid)
{
  int i;
  int found;
  struct group_info gi;

  if (! get_group_info (&gi))
    return 0;

  /* Search through the list looking for GID. */
  found = 0;
  for (i = 0; i < gi.n_groups; i++)
    {
      if (gid == gi.group[i])
        {
          found = 1;
          break;
        }
    }

  free_group_info (&gi);

  return found;
}

#ifdef TEST

char *program_name;

int
main (int argc, char **argv)
{
  int i;

  program_name = argv[0];

  for (i = 1; i < argc; i++)
    {
      gid_t gid;

      gid = atoi (argv[i]);
      printf ("%d: %s\n", gid, group_member (gid) ? "yes" : "no");
    }
  exit (0);
}

#endif /* TEST */
