/* Signal trampoline unwinder, for GDB the GNU Debugger.

   Copyright (C) 2004, 2007 Free Software Foundation, Inc.

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
#include "tramp-frame.h"
#include "frame-unwind.h"
#include "gdbcore.h"
#include "symtab.h"
#include "objfiles.h"
#include "target.h"
#include "trad-frame.h"
#include "frame-base.h"
#include "gdb_assert.h"

struct frame_data
{
  const struct tramp_frame *tramp_frame;
};

struct tramp_frame_cache
{
  CORE_ADDR func;
  const struct tramp_frame *tramp_frame;
  struct trad_frame_cache *trad_cache;
};

static struct trad_frame_cache *
tramp_frame_cache (struct frame_info *next_frame,
		   void **this_cache)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  struct tramp_frame_cache *tramp_cache = (*this_cache);
  if (tramp_cache->trad_cache == NULL)
    {
      tramp_cache->trad_cache = trad_frame_cache_zalloc (next_frame);
      tramp_cache->tramp_frame->init (tramp_cache->tramp_frame,
				      next_frame,
				      tramp_cache->trad_cache,
				      tramp_cache->func);
    }
  return tramp_cache->trad_cache;
}

static void
tramp_frame_this_id (struct frame_info *next_frame,
		     void **this_cache,
		     struct frame_id *this_id)
{
  struct trad_frame_cache *trad_cache
    = tramp_frame_cache (next_frame, this_cache);
  trad_frame_get_id (trad_cache, this_id);
}

static void
tramp_frame_prev_register (struct frame_info *next_frame,
			   void **this_cache,
			   int prev_regnum,
			   int *optimizedp,
			   enum lval_type * lvalp,
			   CORE_ADDR *addrp,
			   int *realnump, gdb_byte *valuep)
{
  struct trad_frame_cache *trad_cache
    = tramp_frame_cache (next_frame, this_cache);
  trad_frame_get_register (trad_cache, next_frame, prev_regnum, optimizedp,
			   lvalp, addrp, realnump, valuep);
}

static CORE_ADDR
tramp_frame_start (const struct tramp_frame *tramp,
		   struct frame_info *next_frame, CORE_ADDR pc)
{
  int ti;
  /* Search through the trampoline for one that matches the
     instruction sequence around PC.  */
  for (ti = 0; tramp->insn[ti].bytes != TRAMP_SENTINEL_INSN; ti++)
    {
      CORE_ADDR func = pc - tramp->insn_size * ti;
      int i;
      for (i = 0; 1; i++)
	{
	  gdb_byte buf[sizeof (tramp->insn[0])];
	  ULONGEST insn;
	  if (tramp->insn[i].bytes == TRAMP_SENTINEL_INSN)
	    return func;
	  if (!safe_frame_unwind_memory (next_frame,
					 func + i * tramp->insn_size,
					 buf, tramp->insn_size))
	    break;
	  insn = extract_unsigned_integer (buf, tramp->insn_size);
	  if (tramp->insn[i].bytes != (insn & tramp->insn[i].mask))
	    break;
	}
    }
  /* Trampoline doesn't match.  */
  return 0;
}

static int
tramp_frame_sniffer (const struct frame_unwind *self,
		     struct frame_info *next_frame,
		     void **this_cache)
{
  const struct tramp_frame *tramp = self->unwind_data->tramp_frame;
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  CORE_ADDR func;
  struct tramp_frame_cache *tramp_cache;

  /* tausq/2004-12-12: We used to assume if pc has a name or is in a valid 
     section, then this is not a trampoline.  However, this assumption is
     false on HPUX which has a signal trampoline that has a name; it can
     also be false when using an alternative signal stack.  */
  func = tramp_frame_start (tramp, next_frame, pc);
  if (func == 0)
    return 0;
  tramp_cache = FRAME_OBSTACK_ZALLOC (struct tramp_frame_cache);
  tramp_cache->func = func;
  tramp_cache->tramp_frame = tramp;
  (*this_cache) = tramp_cache;
  return 1;
}

void
tramp_frame_prepend_unwinder (struct gdbarch *gdbarch,
			      const struct tramp_frame *tramp_frame)
{
  struct frame_data *data;
  struct frame_unwind *unwinder;
  int i;

  /* Check that the instruction sequence contains a sentinel.  */
  for (i = 0; i < ARRAY_SIZE (tramp_frame->insn); i++)
    {
      if (tramp_frame->insn[i].bytes == TRAMP_SENTINEL_INSN)
	break;
    }
  gdb_assert (i < ARRAY_SIZE (tramp_frame->insn));
  gdb_assert (tramp_frame->insn_size <= sizeof (tramp_frame->insn[0].bytes));

  data = GDBARCH_OBSTACK_ZALLOC (gdbarch, struct frame_data);
  unwinder = GDBARCH_OBSTACK_ZALLOC (gdbarch, struct frame_unwind);

  data->tramp_frame = tramp_frame;
  unwinder->type = tramp_frame->frame_type;
  unwinder->unwind_data = data;
  unwinder->sniffer = tramp_frame_sniffer;
  unwinder->this_id = tramp_frame_this_id;
  unwinder->prev_register = tramp_frame_prev_register;
  frame_unwind_prepend_unwinder (gdbarch, unwinder);
}
