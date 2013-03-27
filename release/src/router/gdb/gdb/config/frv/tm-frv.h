/* Target definitions for the Fujitsu FR-V, for GDB, the GNU Debugger.
   Copyright 2000, 2004, 2007 Free Software Foundation, Inc.

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

/* This target uses an architecture vector for most architecture methods.  */

#define TARGET_CAN_USE_HARDWARE_WATCHPOINT(type, cnt, ot) \
	frv_check_watch_resources (type, cnt, ot)
extern int frv_check_watch_resources (int type, int cnt, int ot);

/* When a hardware watchpoint fires off the PC will be left at the
   instruction which caused the watchpoint.  It will be necessary for
   GDB to step over the watchpoint. */

/*#define HAVE_STEPPABLE_WATCHPOINT 1*/

#define STOPPED_BY_WATCHPOINT(W) \
   ((W).kind == TARGET_WAITKIND_STOPPED \
   && (W).value.sig == TARGET_SIGNAL_TRAP \
   && frv_have_stopped_data_address())
extern int frv_have_stopped_data_address(void);

/* Use these macros for watchpoint insertion/deletion.  */
#define target_stopped_data_address(target, x) frv_stopped_data_address(x)
extern int frv_stopped_data_address(CORE_ADDR *addr_p);

