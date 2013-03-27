/* Obstack wrapper for GDB.

   Copyright (C) 2002, 2007 Free Software Foundation, Inc.

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

#if !defined (GDB_OBSTACK_H)
#define GDB_OBSTACK_H 1

#include "obstack.h"

/* Utility macros - wrap obstack alloc into something more robust.  */

#define OBSTACK_ZALLOC(OBSTACK,TYPE) (memset (obstack_alloc ((OBSTACK), sizeof (TYPE)), 0, sizeof (TYPE)))

#define OBSTACK_CALLOC(OBSTACK,NUMBER,TYPE) (memset (obstack_alloc ((OBSTACK), (NUMBER) * sizeof (TYPE)), 0, (NUMBER) * sizeof (TYPE)))

/* Unless explicitly specified, GDB obstacks always use xmalloc() and
   xfree().  */
/* Note: ezannoni 2004-02-09: One could also specify the allocation
   functions using a special init function for each obstack,
   obstack_specify_allocation.  However we just use obstack_init and
   let these defines here do the job.  While one could argue the
   superiority of one approach over the other, we just chose one
   throughout.  */

#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free xfree

#define obstack_grow_str(OBSTACK,STRING) \
  obstack_grow (OBSTACK, STRING, strlen (STRING))
#define obstack_grow_str0(OBSTACK,STRING) \
  obstack_grow0 (OBSTACK, STRING, strlen (STRING))

#endif
