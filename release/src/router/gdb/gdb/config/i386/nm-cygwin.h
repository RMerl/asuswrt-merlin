/* Native definitions for Intel x86 running CYGWIN.
   Copyright 2002, 2006, 2007 Free Software Foundation, Inc.

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

#define ATTACH_NO_WAIT
#define ADD_SHARED_SYMBOL_FILES dll_symbol_command
void dll_symbol_command (char *, int);

#define I386_USE_GENERIC_WATCHPOINTS

#include "i386/nm-i386.h"

/* Support for hardware-assisted breakpoints and watchpoints.  */

#define I386_DR_LOW_SET_CONTROL(VAL)	cygwin_set_dr7 (VAL)
extern void cygwin_set_dr7 (unsigned);

#define I386_DR_LOW_SET_ADDR(N,ADDR)	cygwin_set_dr (N,ADDR)
extern void cygwin_set_dr (int, CORE_ADDR);

#define I386_DR_LOW_RESET_ADDR(N)

#define I386_DR_LOW_GET_STATUS()	cygwin_get_dr6 ()
extern unsigned cygwin_get_dr6 (void);
