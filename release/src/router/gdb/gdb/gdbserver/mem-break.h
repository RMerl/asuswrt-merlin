/* Memory breakpoint interfaces for the remote server for GDB.
   Copyright (C) 2002, 2005, 2007 Free Software Foundation, Inc.

   Contributed by MontaVista Software.

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

#ifndef MEM_BREAK_H
#define MEM_BREAK_H

/* Breakpoints are opaque.  */

/* Create a new breakpoint at WHERE, and call HANDLER when
   it is hit.  */

void set_breakpoint_at (CORE_ADDR where,
			void (*handler) (CORE_ADDR));

/* Delete a breakpoint previously inserted at ADDR with
   set_breakpoint_at.  */

void delete_breakpoint_at (CORE_ADDR addr);

/* Create a reinsertion breakpoint at STOP_AT for the breakpoint
   currently at STOP_PC (and temporarily remove the breakpoint at
   STOP_PC).  */

void reinsert_breakpoint_by_bp (CORE_ADDR stop_pc, CORE_ADDR stop_at);

/* Change the status of the breakpoint at WHERE to inserted.  */

void reinsert_breakpoint (CORE_ADDR where);

/* Change the status of the breakpoint at WHERE to uninserted.  */

void uninsert_breakpoint (CORE_ADDR where);

/* See if any breakpoint claims ownership of STOP_PC.  Call the handler for
   the breakpoint, if found.  */

int check_breakpoints (CORE_ADDR stop_pc);

/* See if any breakpoints shadow the target memory area from MEM_ADDR
   to MEM_ADDR + MEM_LEN.  Update the data already read from the target
   (in BUF) if necessary.  */

void check_mem_read (CORE_ADDR mem_addr, unsigned char *buf, int mem_len);

/* See if any breakpoints shadow the target memory area from MEM_ADDR
   to MEM_ADDR + MEM_LEN.  Update the data to be written to the target
   (in BUF) if necessary, as well as the original data for any breakpoints.  */

void check_mem_write (CORE_ADDR mem_addr, unsigned char *buf, int mem_len);

/* Set the byte pattern to insert for memory breakpoints.  This function
   must be called before any breakpoints are set.  */

void set_breakpoint_data (const unsigned char *bp_data, int bp_len);

/* Delete all breakpoints.  */

void delete_all_breakpoints (void);

#endif /* MEM_BREAK_H */
