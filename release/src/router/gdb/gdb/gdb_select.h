/* Slightly more portable version of <sys/select.h>.

   Copyright (C) 2006, 2007 Free Software Foundation, Inc.

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

#if !defined(GDB_SELECT_H)
#define GDB_SELECT_H

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef USE_WIN32API
#include <winsock2.h>
#endif

extern int gdb_select (int n, fd_set *readfds, fd_set *writefds,
		       fd_set *exceptfds, struct timeval *timeout);

#endif /* !defined(GDB_SELECT_H) */
