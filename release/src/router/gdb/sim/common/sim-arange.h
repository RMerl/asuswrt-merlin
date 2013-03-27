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

/* This file is meant to be included by sim-basics.h.  */

#ifndef SIM_ARANGE_H
#define SIM_ARANGE_H

/* A list of address ranges.  */

typedef struct _addr_subrange {
  struct _addr_subrange *next;

  /* Range of addresses to be traced is [start,end].  */
  address_word start,end;
} ADDR_SUBRANGE;

/* For speed, searching is done on a tree.  */

typedef struct _addr_range_tree {
  struct _addr_range_tree *lower;
  struct _addr_range_tree *higher;

  /* Range of addresses to be traced is [start,end].  */
  address_word start,end;
} ADDR_RANGE_TREE;

/* The top level struct.  */

typedef struct _addr_range {
  ADDR_SUBRANGE *ranges;
#define ADDR_RANGE_RANGES(ar) ((ar)->ranges)
  ADDR_RANGE_TREE *range_tree;
#define ADDR_RANGE_TREE(ar) ((ar)->range_tree)
} ADDR_RANGE;

/* Add address range START,END to AR.  */
extern void sim_addr_range_add (ADDR_RANGE * /*ar*/,
				address_word /*start*/,
				address_word /*end*/);

/* Delete address range START,END from AR.  */
extern void sim_addr_range_delete (ADDR_RANGE * /*ar*/,
				   address_word /*start*/,
				   address_word /*end*/);

/* Return non-zero if ADDR is in range AR, traversing the entire tree.
   If no range is specified, that is defined to mean "everything".  */
extern INLINE int
sim_addr_range_hit_p (ADDR_RANGE * /*ar*/, address_word /*addr*/);
#define ADDR_RANGE_HIT_P(ar, addr) \
  ((ar)->range_tree == NULL || sim_addr_range_hit_p ((ar), (addr)))

#ifdef HAVE_INLINE
#ifdef SIM_ARANGE_C
#define SIM_ARANGE_INLINE INLINE
#else
#define SIM_ARANGE_INLINE EXTERN_INLINE
#endif
#include "sim-arange.c"
#else
#define SIM_ARANGE_INLINE
#endif
#define SIM_ARANGE_C_INCLUDED

#endif /* SIM_ARANGE_H */
