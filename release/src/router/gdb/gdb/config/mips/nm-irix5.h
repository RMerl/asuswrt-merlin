/* Definitions for native support of irix5.

   Copyright 1993, 1996, 1998, 1999, 2000, 2007 Free Software Foundation, Inc.

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

#undef IN_SOLIB_DYNSYM_RESOLVE_CODE

#define TARGET_HAS_HARDWARE_WATCHPOINTS

/* TARGET_CAN_USE_HARDWARE_WATCHPOINT is now defined to go through
   the target vector.  For Irix5, procfs_can_use_hw_watchpoint()
   should be invoked.  */

/* When a hardware watchpoint fires off the PC will be left at the
   instruction which caused the watchpoint.  It will be necessary for
   GDB to step over the watchpoint. */

#define STOPPED_BY_WATCHPOINT(W) \
     procfs_stopped_by_watchpoint(inferior_ptid)
extern int procfs_stopped_by_watchpoint (ptid_t);

/* Use these macros for watchpoint insertion/deletion.  */
/* type can be 0: write watch, 1: read watch, 2: access watch (read/write) */
#define target_insert_watchpoint(ADDR, LEN, TYPE) \
     procfs_set_watchpoint (inferior_ptid, ADDR, LEN, TYPE, 0)
#define target_remove_watchpoint(ADDR, LEN, TYPE) \
     procfs_set_watchpoint (inferior_ptid, ADDR, 0, 0, 0)
extern int procfs_set_watchpoint (ptid_t, CORE_ADDR, int, int, int);

#define TARGET_REGION_SIZE_OK_FOR_HW_WATCHPOINT(SIZE) 1

