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

#ifndef TRAD_FRAME_H
#define TRAD_FRAME_H

#include "frame.h"		/* For "struct frame_id".  */

struct frame_info;
struct trad_frame_cache;

/* A simple, or traditional frame cache.

   The entire cache is populated in a single pass and then generic
   routines are used to extract the various cache values.  */

struct trad_frame_cache *trad_frame_cache_zalloc (struct frame_info *next_frame);

/* This frame's ID.  */
void trad_frame_set_id (struct trad_frame_cache *this_trad_cache,
			struct frame_id this_id);
void trad_frame_get_id (struct trad_frame_cache *this_trad_cache,
			struct frame_id *this_id);
void trad_frame_set_this_base (struct trad_frame_cache *this_trad_cache,
			       CORE_ADDR this_base);
CORE_ADDR trad_frame_get_this_base (struct trad_frame_cache *this_trad_cache);

void trad_frame_set_reg_realreg (struct trad_frame_cache *this_trad_cache,
				 int regnum, int realreg);
void trad_frame_set_reg_unknown (struct trad_frame_cache *this_trad_cache,
				 int regnum, CORE_ADDR addr);
void trad_frame_set_reg_addr (struct trad_frame_cache *this_trad_cache,
			      int regnum, CORE_ADDR addr);
void trad_frame_set_reg_value (struct trad_frame_cache *this_cache,
			       int regnum, LONGEST val);

void trad_frame_get_register (struct trad_frame_cache *this_trad_cache,
			      struct frame_info *next_frame,
			      int regnum, int *optimizedp,
			      enum lval_type *lvalp, CORE_ADDR *addrp,
			      int *realregp, gdb_byte *bufferp);

/* A traditional saved regs table, indexed by REGNUM, encoding where
   the value of REGNUM for the previous frame can be found in this
   frame.

   The table is initialized with an identity encoding (ADDR == -1,
   REALREG == REGNUM) indicating that the value of REGNUM in the
   previous frame can be found in register REGNUM (== REALREG) in this
   frame.

   The initial encoding can then be changed:

   Modify ADDR (REALREG >= 0, ADDR != -1) to indicate that the value
   of register REGNUM in the previous frame can be found in memory at
   ADDR in this frame (addr_p, !realreg_p, !value_p).

   Modify REALREG (REALREG >= 0, ADDR == -1) to indicate that the
   value of register REGNUM in the previous frame is found in register
   REALREG in this frame (!addr_p, realreg_p, !value_p).

   Call trad_frame_set_value (REALREG == -1) to indicate that the
   value of register REGNUM in the previous frame is found in ADDR
   (!addr_p, !realreg_p, value_p).

   Call trad_frame_set_unknown (REALREG == -2) to indicate that the
   register's value is not known.  */

struct trad_frame_saved_reg
{
  LONGEST addr; /* A CORE_ADDR fits in a longest.  */
  int realreg;
};

/* Encode REGNUM value in the trad-frame.  */
void trad_frame_set_value (struct trad_frame_saved_reg this_saved_regs[],
			   int regnum, LONGEST val);

/* Mark REGNUM as unknown.  */
void trad_frame_set_unknown (struct trad_frame_saved_reg this_saved_regs[],
			     int regnum);

/* Convenience functions, return non-zero if the register has been
   encoded as specified.  */
int trad_frame_value_p (struct trad_frame_saved_reg this_saved_regs[],
			int regnum);
int trad_frame_addr_p (struct trad_frame_saved_reg this_saved_regs[],
		       int regnum);
int trad_frame_realreg_p (struct trad_frame_saved_reg this_saved_regs[],
			  int regnum);


/* Return a freshly allocated (and initialized) trad_frame array.  */
struct trad_frame_saved_reg *trad_frame_alloc_saved_regs (struct frame_info *next_frame);

/* Given the trad_frame info, return the location of the specified
   register.  */
void trad_frame_get_prev_register (struct frame_info *next_frame,
				   struct trad_frame_saved_reg this_saved_regs[],
				   int regnum, int *optimizedp,
				   enum lval_type *lvalp, CORE_ADDR *addrp,
				   int *realregp, gdb_byte *bufferp);

#endif
