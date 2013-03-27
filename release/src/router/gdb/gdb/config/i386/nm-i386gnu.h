/* Native-dependent definitions for Intel 386 running the GNU Hurd
   Copyright 1994, 1995, 1996, 2002, 2007 Free Software Foundation, Inc.

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

#ifndef NM_I386GNU_H
#define NM_I386GNU_H

#include <unistd.h>
#include <mach.h>
#include <mach/exception.h>
#include "regcache.h"

extern char *gnu_target_pid_to_str (int pid);

/* Don't do wait_for_inferior on attach.  */
#define ATTACH_NO_WAIT

/* Thread flavors used in re-setting the T bit.  */
#define THREAD_STATE_FLAVOR		i386_REGS_SEGS_STATE
#define THREAD_STATE_SIZE		i386_THREAD_STATE_COUNT
#define THREAD_STATE_SET_TRACED(state) \
  	((struct i386_thread_state *) (state))->efl |= 0x100
#define THREAD_STATE_CLEAR_TRACED(state) \
  	((((struct i386_thread_state *) (state))->efl &= ~0x100), 1)

#endif /* nm-i386gnu.h */
