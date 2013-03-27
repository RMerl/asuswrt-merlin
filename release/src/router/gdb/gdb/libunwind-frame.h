/* Frame unwinder for frames with libunwind frame information.

   Copyright (C) 2003, 2006, 2007 Free Software Foundation, Inc.

   Contributed by Jeff Johnston.

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

#ifdef HAVE_LIBUNWIND_H

struct frame_info;
struct frame_id;
struct regcache;
struct gdbarch;

#ifndef LIBUNWIND_FRAME_H
#define LIBUNWIND_FRAME_H 1

#include "libunwind.h"

struct libunwind_descr
{
  int (*gdb2uw) (int);
  int (*uw2gdb) (int);
  int (*is_fpreg) (int);
  void *accessors;
  void *special_accessors;
};

const struct frame_unwind *libunwind_frame_sniffer (struct frame_info *next_frame);
const struct frame_unwind *libunwind_sigtramp_frame_sniffer (struct frame_info *next_frame);

void libunwind_frame_set_descr (struct gdbarch *arch, struct libunwind_descr *descr);

void libunwind_frame_this_id (struct frame_info *next_frame, void **this_cache,
			      struct frame_id *this_id);
void libunwind_frame_prev_register (struct frame_info *next_frame, void **this_cache,
				    int regnum, int *optimizedp,
				    enum lval_type *lvalp, CORE_ADDR *addrp,
				    int *realnump, gdb_byte *valuep);
void libunwind_frame_dealloc_cache (struct frame_info *self, void *cache);
CORE_ADDR libunwind_frame_base_address (struct frame_info *next_frame, void **this_cache);

int libunwind_is_initialized (void);

int libunwind_search_unwind_table (void *as, long ip, void *di,
				   void *pi, int need_unwind_info, void *args);

unw_word_t libunwind_find_dyn_list (unw_addr_space_t, unw_dyn_info_t *,
				    void *);

int libunwind_get_reg_special (struct gdbarch *gdbarch,
			       struct regcache *regcache,
			       int regnum, void *buf);

#endif /* libunwind-frame.h */

#endif /* HAVE_LIBUNWIND_H  */
