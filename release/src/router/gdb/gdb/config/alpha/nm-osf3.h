/* Native definitions for alpha running OSF/1-3.x and higher, using procfs.
   Copyright 1995, 2007 Free Software Foundation, Inc.

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

/* Number of traps that happen between exec'ing the shell
   to run an inferior, and when we finally get to
   the inferior code.  This is 2 on most implementations.  */
#define START_INFERIOR_TRAPS_EXPECTED 3

/* Don't trace faults under OSF/1, rely on the posting of the appropriate
   signal if fault tracing is disabled.
   Tracing T_IFAULT under Alpha OSF/1 causes a `floating point enable'
   fault from which we cannot continue (except by disabling the
   tracing).
   And as OSF/1 doesn't provide the standard fault definitions, the
   mapping of faults to appropriate signals in procfs_wait is difficult.  */
#define PROCFS_DONT_TRACE_FAULTS

/* Work around some peculiarities in the OSF/1 procfs implementation.  */
#define PROCFS_NEED_CLEAR_CURSIG_FOR_KILL
