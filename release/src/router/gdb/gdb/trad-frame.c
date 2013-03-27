/* Traditional frame unwind support, for GDB the GNU Debugger.

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
#include "trad-frame.h"
#include "regcache.h"

struct trad_frame_cache
{
  struct frame_info *next_frame;
  CORE_ADDR this_base;
  struct trad_frame_saved_reg *prev_regs;
  struct frame_id this_id;
};

struct trad_frame_cache *
trad_frame_cache_zalloc (struct frame_info *next_frame)
{
  struct trad_frame_cache *this_trad_cache;

  this_trad_cache = FRAME_OBSTACK_ZALLOC (struct trad_frame_cache);
  this_trad_cache->prev_regs = trad_frame_alloc_saved_regs (next_frame);
  this_trad_cache->next_frame = next_frame;
  return this_trad_cache;
}

/* A traditional frame is unwound by analysing the function prologue
   and using the information gathered to track registers.  For
   non-optimized frames, the technique is reliable (just need to check
   for all potential instruction sequences).  */

struct trad_frame_saved_reg *
trad_frame_alloc_saved_regs (struct frame_info *next_frame)
{
  int regnum;
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  int numregs = gdbarch_num_regs (current_gdbarch)
		+ gdbarch_num_pseudo_regs (current_gdbarch);
  struct trad_frame_saved_reg *this_saved_regs
    = FRAME_OBSTACK_CALLOC (numregs, struct trad_frame_saved_reg);
  for (regnum = 0; regnum < numregs; regnum++)
    {
      this_saved_regs[regnum].realreg = regnum;
      this_saved_regs[regnum].addr = -1;
    }      
  return this_saved_regs;
}

enum { REG_VALUE = -1, REG_UNKNOWN = -2 };

int
trad_frame_value_p (struct trad_frame_saved_reg this_saved_regs[], int regnum)
{
  return (this_saved_regs[regnum].realreg == REG_VALUE);
}

int
trad_frame_addr_p (struct trad_frame_saved_reg this_saved_regs[], int regnum)
{
  return (this_saved_regs[regnum].realreg >= 0
	  && this_saved_regs[regnum].addr != -1);
}

int
trad_frame_realreg_p (struct trad_frame_saved_reg this_saved_regs[],
		      int regnum)
{
  return (this_saved_regs[regnum].realreg >= 0
	  && this_saved_regs[regnum].addr == -1);
}

void
trad_frame_set_value (struct trad_frame_saved_reg this_saved_regs[],
		      int regnum, LONGEST val)
{
  /* Make the REALREG invalid, indicating that the ADDR contains the
     register's value.  */
  this_saved_regs[regnum].realreg = REG_VALUE;
  this_saved_regs[regnum].addr = val;
}

void
trad_frame_set_reg_value (struct trad_frame_cache *this_trad_cache,
			  int regnum, LONGEST val)
{
  /* External interface for users of trad_frame_cache
     (who cannot access the prev_regs object directly).  */
  trad_frame_set_value (this_trad_cache->prev_regs, regnum, val);
}

void
trad_frame_set_reg_realreg (struct trad_frame_cache *this_trad_cache,
			    int regnum, int realreg)
{
  this_trad_cache->prev_regs[regnum].realreg = realreg;
  this_trad_cache->prev_regs[regnum].addr = -1;
}

void
trad_frame_set_reg_addr (struct trad_frame_cache *this_trad_cache,
			 int regnum, CORE_ADDR addr)
{
  this_trad_cache->prev_regs[regnum].addr = addr;
}

void
trad_frame_set_unknown (struct trad_frame_saved_reg this_saved_regs[],
			int regnum)
{
  /* Make the REALREG invalid, indicating that the value is not known.  */
  this_saved_regs[regnum].realreg = REG_UNKNOWN;
  this_saved_regs[regnum].addr = -1;
}

void
trad_frame_get_prev_register (struct frame_info *next_frame,
			      struct trad_frame_saved_reg this_saved_regs[],
			      int regnum, int *optimizedp,
			      enum lval_type *lvalp, CORE_ADDR *addrp,
			      int *realregp, gdb_byte *bufferp)
{
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  if (trad_frame_addr_p (this_saved_regs, regnum))
    {
      /* The register was saved in memory.  */
      *optimizedp = 0;
      *lvalp = lval_memory;
      *addrp = this_saved_regs[regnum].addr;
      *realregp = -1;
      if (bufferp != NULL)
	{
	  /* Read the value in from memory.  */
	  get_frame_memory (next_frame, this_saved_regs[regnum].addr, bufferp,
			    register_size (gdbarch, regnum));
	}
    }
  else if (trad_frame_realreg_p (this_saved_regs, regnum))
    {
      *optimizedp = 0;
      *lvalp = lval_register;
      *addrp = 0;
      *realregp = this_saved_regs[regnum].realreg;
      /* Ask the next frame to return the value of the register.  */
      if (bufferp)
	frame_unwind_register (next_frame, (*realregp), bufferp);
    }
  else if (trad_frame_value_p (this_saved_regs, regnum))
    {
      /* The register's value is available.  */
      *optimizedp = 0;
      *lvalp = not_lval;
      *addrp = 0;
      *realregp = -1;
      if (bufferp != NULL)
	store_unsigned_integer (bufferp, register_size (gdbarch, regnum),
				this_saved_regs[regnum].addr);
    }
  else
    {
      error (_("Register %s not available"),
	     gdbarch_register_name (gdbarch, regnum));
    }
}

void
trad_frame_get_register (struct trad_frame_cache *this_trad_cache,
			 struct frame_info *next_frame,
			 int regnum, int *optimizedp,
			 enum lval_type *lvalp, CORE_ADDR *addrp,
			 int *realregp, gdb_byte *bufferp)
{
  trad_frame_get_prev_register (next_frame, this_trad_cache->prev_regs,
				regnum, optimizedp, lvalp, addrp, realregp,
				bufferp);
}

void
trad_frame_set_id (struct trad_frame_cache *this_trad_cache,
		   struct frame_id this_id)
{
  this_trad_cache->this_id = this_id;
}

void
trad_frame_get_id (struct trad_frame_cache *this_trad_cache,
		   struct frame_id *this_id)
{
  (*this_id) = this_trad_cache->this_id;
}

void
trad_frame_set_this_base (struct trad_frame_cache *this_trad_cache,
			  CORE_ADDR this_base)
{
  this_trad_cache->this_base = this_base;
}

CORE_ADDR
trad_frame_get_this_base (struct trad_frame_cache *this_trad_cache)
{
  return this_trad_cache->this_base;
}
