/* Definitions for frame unwinder, for GDB, the GNU debugger.

   Copyright (C) 2003, 2004, 2007 Free Software Foundation, Inc.

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

#include "defs.h"
#include "frame.h"
#include "frame-unwind.h"
#include "gdb_assert.h"
#include "dummy-frame.h"
#include "gdb_obstack.h"

static struct gdbarch_data *frame_unwind_data;

struct frame_unwind_table_entry
{
  frame_unwind_sniffer_ftype *sniffer;
  const struct frame_unwind *unwinder;
  struct frame_unwind_table_entry *next;
};

struct frame_unwind_table
{
  struct frame_unwind_table_entry *list;
  /* The head of the OSABI part of the search list.  */
  struct frame_unwind_table_entry **osabi_head;
};

static void *
frame_unwind_init (struct obstack *obstack)
{
  struct frame_unwind_table *table
    = OBSTACK_ZALLOC (obstack, struct frame_unwind_table);
  /* Start the table out with a few default sniffers.  OSABI code
     can't override this.  */
  table->list = OBSTACK_ZALLOC (obstack, struct frame_unwind_table_entry);
  table->list->unwinder = dummy_frame_unwind;
  /* The insertion point for OSABI sniffers.  */
  table->osabi_head = &table->list->next;
  return table;
}

void
frame_unwind_append_sniffer (struct gdbarch *gdbarch,
			     frame_unwind_sniffer_ftype *sniffer)
{
  struct frame_unwind_table *table = gdbarch_data (gdbarch, frame_unwind_data);
  struct frame_unwind_table_entry **ip;

  /* Find the end of the list and insert the new entry there.  */
  for (ip = table->osabi_head; (*ip) != NULL; ip = &(*ip)->next);
  (*ip) = GDBARCH_OBSTACK_ZALLOC (gdbarch, struct frame_unwind_table_entry);
  (*ip)->sniffer = sniffer;
}

void
frame_unwind_prepend_unwinder (struct gdbarch *gdbarch,
				const struct frame_unwind *unwinder)
{
  struct frame_unwind_table *table = gdbarch_data (gdbarch, frame_unwind_data);
  struct frame_unwind_table_entry *entry;

  /* Insert the new entry at the start of the list.  */
  entry = GDBARCH_OBSTACK_ZALLOC (gdbarch, struct frame_unwind_table_entry);
  entry->unwinder = unwinder;
  entry->next = (*table->osabi_head);
  (*table->osabi_head) = entry;
}

const struct frame_unwind *
frame_unwind_find_by_frame (struct frame_info *next_frame, void **this_cache)
{
  int i;
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  struct frame_unwind_table *table = gdbarch_data (gdbarch, frame_unwind_data);
  struct frame_unwind_table_entry *entry;
  for (entry = table->list; entry != NULL; entry = entry->next)
    {
      if (entry->sniffer != NULL)
	{
	  const struct frame_unwind *desc = NULL;
	  desc = entry->sniffer (next_frame);
	  if (desc != NULL)
	    return desc;
	}
      if (entry->unwinder != NULL)
	{
	  if (entry->unwinder->sniffer (entry->unwinder, next_frame,
					this_cache))
	    return entry->unwinder;
	}
    }
  internal_error (__FILE__, __LINE__, _("frame_unwind_find_by_frame failed"));
}

extern initialize_file_ftype _initialize_frame_unwind; /* -Wmissing-prototypes */

void
_initialize_frame_unwind (void)
{
  frame_unwind_data = gdbarch_data_register_pre_init (frame_unwind_init);
}
