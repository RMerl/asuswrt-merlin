/* Address ranges.
   Copyright (C) 1998, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Solutions.

This file is part of the GNU Simulators.

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

/* Tell sim-arange.h it's us.  */
#define SIM_ARANGE_C

#include "libiberty.h"
#include "sim-basics.h"
#include "sim-assert.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#define DEFINE_INLINE_P (! defined (SIM_ARANGE_C_INCLUDED))
#define DEFINE_NON_INLINE_P defined (SIM_ARANGE_C_INCLUDED)

#if DEFINE_NON_INLINE_P

/* Insert a range.  */

static void
insert_range (ADDR_SUBRANGE **pos, ADDR_SUBRANGE *asr)
{
  asr->next = *pos;
  *pos = asr;
}

/* Delete a range.  */

static void
delete_range (ADDR_SUBRANGE **thisasrp)
{
  ADDR_SUBRANGE *thisasr;

  thisasr = *thisasrp;
  *thisasrp = thisasr->next;

  free (thisasr);
}

/* Add or delete an address range.
   This code was borrowed from linux's locks.c:posix_lock_file().
   ??? Todo: Given our simpler needs this could be simplified
   (split into two fns).  */

static void
frob_range (ADDR_RANGE *ar, address_word start, address_word end, int delete_p)
{
  ADDR_SUBRANGE *asr;
  ADDR_SUBRANGE *new_asr, *new_asr2;
  ADDR_SUBRANGE *left = NULL;
  ADDR_SUBRANGE *right = NULL;
  ADDR_SUBRANGE **before;
  ADDR_SUBRANGE init_caller;
  ADDR_SUBRANGE *caller = &init_caller;
  int added_p = 0;

  memset (caller, 0, sizeof (ADDR_SUBRANGE));
  new_asr = ZALLOC (ADDR_SUBRANGE);
  new_asr2 = ZALLOC (ADDR_SUBRANGE);

  caller->start = start;
  caller->end = end;
  before = &ar->ranges;

  while ((asr = *before) != NULL)
    {
      if (! delete_p)
	{
	  /* Try next range if current range preceeds new one and not
	     adjacent or overlapping.  */
	  if (asr->end < caller->start - 1)
	    goto next_range;

	  /* Break out if new range preceeds current one and not
	     adjacent or overlapping.  */
	  if (asr->start > caller->end + 1)
	    break;

	  /* If we come here, the new and current ranges are adjacent or
	     overlapping. Make one range yielding from the lower start address
	     of both ranges to the higher end address.  */
	  if (asr->start > caller->start)
	    asr->start = caller->start;
	  else
	    caller->start = asr->start;
	  if (asr->end < caller->end)
	    asr->end = caller->end;
	  else
	    caller->end = asr->end;

	  if (added_p)
	    {
	      delete_range (before);
	      continue;
	    }
	  caller = asr;
	  added_p = 1;
	}
      else /* deleting a range */
	{
	  /* Try next range if current range preceeds new one.  */
	  if (asr->end < caller->start)
	    goto next_range;

	  /* Break out if new range preceeds current one.  */
	  if (asr->start > caller->end)
	    break;

	  added_p = 1;

	  if (asr->start < caller->start)
	    left = asr;

	  /* If the next range in the list has a higher end
	     address than the new one, insert the new one here.  */
	  if (asr->end > caller->end)
	    {
	      right = asr;
	      break;
	    }
	  if (asr->start >= caller->start)
	    {
	      /* The new range completely replaces an old
	         one (This may happen several times).  */
	      if (added_p)
		{
		  delete_range (before);
		  continue;
		}

	      /* Replace the old range with the new one.  */
	      asr->start = caller->start;
	      asr->end = caller->end;
	      caller = asr;
	      added_p = 1;
	    }
	}

      /* Go on to next range.  */
    next_range:
      before = &asr->next;
    }

  if (!added_p)
    {
      if (delete_p)
	goto out;
      new_asr->start = caller->start;
      new_asr->end = caller->end;
      insert_range (before, new_asr);
      new_asr = NULL;
    }
  if (right)
    {
      if (left == right)
	{
	  /* The new range breaks the old one in two pieces,
	     so we have to use the second new range.  */
	  new_asr2->start = right->start;
	  new_asr2->end = right->end;
	  left = new_asr2;
	  insert_range (before, left);
	  new_asr2 = NULL;
	}
      right->start = caller->end + 1;
    }
  if (left)
    {
      left->end = caller->start - 1;
    }

 out:
  if (new_asr)
    free(new_asr);
  if (new_asr2)
    free(new_asr2);
}

/* Free T and all subtrees.  */

static void
free_search_tree (ADDR_RANGE_TREE *t)
{
  if (t != NULL)
    {
      free_search_tree (t->lower);
      free_search_tree (t->higher);
      free (t);
    }
}

/* Subroutine of build_search_tree to recursively build a balanced tree.
   ??? It's not an optimum tree though.  */

static ADDR_RANGE_TREE *
build_tree_1 (ADDR_SUBRANGE **asrtab, unsigned int n)
{
  unsigned int mid = n / 2;
  ADDR_RANGE_TREE *t;

  if (n == 0)
    return NULL;
  t = (ADDR_RANGE_TREE *) xmalloc (sizeof (ADDR_RANGE_TREE));
  t->start = asrtab[mid]->start;
  t->end = asrtab[mid]->end;
  if (mid != 0)
    t->lower = build_tree_1 (asrtab, mid);
  else
    t->lower = NULL;
  if (n > mid + 1)
    t->higher = build_tree_1 (asrtab + mid + 1, n - mid - 1);
  else
    t->higher = NULL;
  return t;
}

/* Build a search tree for address range AR.  */

static void
build_search_tree (ADDR_RANGE *ar)
{
  /* ??? Simple version for now.  */
  ADDR_SUBRANGE *asr,**asrtab;
  unsigned int i, n;

  for (n = 0, asr = ar->ranges; asr != NULL; ++n, asr = asr->next)
    continue;
  asrtab = (ADDR_SUBRANGE **) xmalloc (n * sizeof (ADDR_SUBRANGE *));
  for (i = 0, asr = ar->ranges; i < n; ++i, asr = asr->next)
    asrtab[i] = asr;
  ar->range_tree = build_tree_1 (asrtab, n);
  free (asrtab);
}

void
sim_addr_range_add (ADDR_RANGE *ar, address_word start, address_word end)
{
  frob_range (ar, start, end, 0);

  /* Rebuild the search tree.  */
  /* ??? Instead of rebuilding it here it could be done in a module resume
     handler, say by first checking for a `changed' flag, assuming of course
     this would never be done while the simulation is running.  */
  free_search_tree (ar->range_tree);
  build_search_tree (ar);
}

void
sim_addr_range_delete (ADDR_RANGE *ar, address_word start, address_word end)
{
  frob_range (ar, start, end, 1);

  /* Rebuild the search tree.  */
  /* ??? Instead of rebuilding it here it could be done in a module resume
     handler, say by first checking for a `changed' flag, assuming of course
     this would never be done while the simulation is running.  */
  free_search_tree (ar->range_tree);
  build_search_tree (ar);
}

#endif /* DEFINE_NON_INLINE_P */

#if DEFINE_INLINE_P

SIM_ARANGE_INLINE int
sim_addr_range_hit_p (ADDR_RANGE *ar, address_word addr)
{
  ADDR_RANGE_TREE *t = ar->range_tree;

  while (t != NULL)
    {
      if (addr < t->start)
	t = t->lower;
      else if (addr > t->end)
	t = t->higher;
      else
	return 1;
    }
  return 0;
}

#endif /* DEFINE_INLINE_P */
