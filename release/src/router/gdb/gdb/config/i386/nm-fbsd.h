/* Native-dependent definitions for FreeBSD/i386.

   Copyright 1986, 1987, 1989, 1992, 1994, 1996, 1997, 2000, 2001, 2004, 2005,
   2007 Free Software Foundation, Inc.

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

#ifndef NM_FBSD_H
#define NM_FBSD_H

#ifdef HAVE_PT_GETDBREGS
#define I386_USE_GENERIC_WATCHPOINTS
#endif

#include "i386/nm-i386.h"

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

/* Provide access to the i386 hardware debugging registers.  */

#define I386_DR_LOW_SET_CONTROL(control) \
  i386bsd_dr_set_control (control)
extern void i386bsd_dr_set_control (unsigned long control);

#define I386_DR_LOW_SET_ADDR(regnum, addr) \
  i386bsd_dr_set_addr (regnum, addr)
extern void i386bsd_dr_set_addr (int regnum, CORE_ADDR addr);

#define I386_DR_LOW_RESET_ADDR(regnum) \
  i386bsd_dr_reset_addr (regnum)
extern void i386bsd_dr_reset_addr (int regnum);

#define I386_DR_LOW_GET_STATUS() \
  i386bsd_dr_get_status ()
extern unsigned long i386bsd_dr_get_status (void);


#endif /* nm-fbsd.h */
