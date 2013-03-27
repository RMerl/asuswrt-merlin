/* Library interface into GDB.
   Copyright (C) 1999, 2001, 2007 Free Software Foundation, Inc.

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

#ifndef GDB_H
#define GDB_H

struct ui_out;

/* Return-code (RC) from a gdb library call.  (The abreviation RC is
   taken from the sim/common directory.) */

enum gdb_rc {
  /* The operation failed.  The failure message can be fetched by
     calling ``char *error_last_message(void)''.  The value is
     determined by the catch_errors() interface.  The MSG parameter is
     set to a freshly allocated copy of the error message.  */
  /* NOTE: Since ``defs.h:catch_errors()'' does not return an error /
     internal / quit indication it is not possible to return that
     here. */
  GDB_RC_FAIL = 0,
  /* No error occured but nothing happened. Due to the catch_errors()
     interface, this must be non-zero. */
  GDB_RC_NONE = 1,
  /* The operation was successful. Due to the catch_errors()
     interface, this must be non-zero. */
  GDB_RC_OK = 2
};


/* Print the specified breakpoint on GDB_STDOUT. (Eventually this
   function will ``print'' the object on ``output''). */
enum gdb_rc gdb_breakpoint_query (struct ui_out *uiout, int bnum,
				  char **error_message);

/* Create a breakpoint at ADDRESS (a GDB source and line). */
enum gdb_rc gdb_breakpoint (char *address, char *condition,
			    int hardwareflag, int tempflag,
			    int thread, int ignore_count,
			    char **error_message);

/* Switch thread and print notification. */
enum gdb_rc gdb_thread_select (struct ui_out *uiout, char *tidstr,
			       char **error_message);

/* Print a list of known thread ids. */
enum gdb_rc gdb_list_thread_ids (struct ui_out *uiout,
				 char **error_message);

#endif
