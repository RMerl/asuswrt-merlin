/* Hardware memory allocator.
   Copyright (C) 1998, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

This file is part of GDB, the GNU debugger.

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


#include "hw-main.h"
#include "hw-base.h"


#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

struct hw_alloc_data {
  void *alloc;
  int zalloc_p;
  struct hw_alloc_data *next;
};

void
create_hw_alloc_data (struct hw *me)
{
  /* NULL */
}

void
delete_hw_alloc_data (struct hw *me)
{
  while (me->alloc_of_hw != NULL)
    {
      hw_free (me, me->alloc_of_hw->alloc);
    }
}



void *
hw_zalloc (struct hw *me, unsigned long size)
{
  struct hw_alloc_data *memory = ZALLOC (struct hw_alloc_data);
  memory->alloc = zalloc (size);
  memory->zalloc_p = 1;
  memory->next = me->alloc_of_hw;
  me->alloc_of_hw = memory;
  return memory->alloc;
}

void *
hw_malloc (struct hw *me, unsigned long size)
{
  struct hw_alloc_data *memory = ZALLOC (struct hw_alloc_data);
  memory->alloc = zalloc (size);
  memory->zalloc_p = 0;
  memory->next = me->alloc_of_hw;
  me->alloc_of_hw = memory;
  return memory->alloc;
}

void
hw_free (struct hw *me,
	 void *alloc)
{
  struct hw_alloc_data **memory;
  for (memory = &me->alloc_of_hw;
       *memory != NULL;
       memory = &(*memory)->next)
    {
      if ((*memory)->alloc == alloc)
	{
	  struct hw_alloc_data *die = (*memory);
	  (*memory) = die->next;
	  if (die->zalloc_p)
	    zfree (die->alloc);
	  else
	    free (die->alloc);
	  zfree (die);
	  return;
	}
    }
  hw_abort (me, "free of memory not belonging to a device");
}
