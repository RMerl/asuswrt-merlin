/* BSD user-level threads support.

   Copyright (C) 2005, 2007 Free Software Foundation, Inc.

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

#ifndef BSD_UTHREAD_H
#define BSD_UTHREAD_H 1

/* Set the function that supplies registers for an inactive thread for
   architecture GDBARCH to SUPPLY_UTHREAD.  */

extern void bsd_uthread_set_supply_uthread (struct gdbarch *gdbarch,
				    void (*supply_uthread) (struct regcache *,
							    int, CORE_ADDR));


/* Set the function that collects registers for an inactive thread for
   architecture GDBARCH to SUPPLY_UTHREAD.  */

extern void bsd_uthread_set_collect_uthread (struct gdbarch *gdbarch,
			     void (*collect_uthread) (const struct regcache *,
						      int, CORE_ADDR));

#endif /* bsd-uthread.h */
