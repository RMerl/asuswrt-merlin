/* Low level interface to ptrace, for GDB when running under Unix.

   Copyright (C) 2003, 2005, 2007 Free Software Foundation, Inc.

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

#ifndef INFLOW_H
#define INFLOW_H

#include "terminal.h"		/* For HAVE_TERMIOS et.al.  */

#ifdef HAVE_TERMIOS
# define PROCESS_GROUP_TYPE pid_t
#elif defined (HAVE_TERMIO) || defined (HAVE_SGTTY)
# define PROCESS_GROUP_TYPE int
#endif

#ifdef PROCESS_GROUP_TYPE
/* Process group for us and the inferior.  Saved and restored just like
   {our,inferior}_ttystate.  */
extern PROCESS_GROUP_TYPE our_process_group;
extern PROCESS_GROUP_TYPE inferior_process_group;
#endif

#endif /* inflow.h */
