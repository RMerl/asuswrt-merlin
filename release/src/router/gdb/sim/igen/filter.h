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


/* NB, an empty filter is NULL */
typedef struct _filter filter;


/* parse the list merging any flags into the filter */

extern void filter_parse (filter **filters, const char *filt);


/* add the second filter to the first */

extern void filter_add (filter **filters, filter *add);



/* returns true if SUB is a strict subset of SUPER.  For an empty set
   is a member of any set */

extern int filter_is_subset (filter *superset, filter *subset);


/* return true if there is at least one member common to the two
   filters */

extern int filter_is_common (filter *l, filter *r);


/* returns the index (pos + 1) if the name is in the filter.  */

extern int filter_is_member (filter *set, const char *flag);


/* returns true if one of the flags is not present in the filter.
   === !filter_is_subset (filter_parse (NULL, flags), filters) */
int is_filtered_out (filter *filters, const char *flags);


/* returns the next member of the filter set that follows MEMBER.
   Member does not need to be an elememt of the filter set.  Next of
   "" is the first non-empty member */
char *filter_next (filter *set, char *member);



/* for debugging */

extern void dump_filter (lf *file, char *prefix, filter *filt, char *suffix);
