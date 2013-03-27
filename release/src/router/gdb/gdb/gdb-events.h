/* User Interface Events.

   Copyright (C) 1999, 2001, 2002, 2004, 2007 Free Software Foundation, Inc.

   Contributed by Cygnus Solutions.

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

/* Work in progress */

/* This file was created with the aid of ``gdb-events.sh''.

   The bourn shell script ``gdb-events.sh'' creates the files
   ``new-gdb-events.c'' and ``new-gdb-events.h and then compares
   them against the existing ``gdb-events.[hc]''.  Any differences
   found being reported.

   If editing this file, please also run gdb-events.sh and merge any
   changes into that script. Conversely, when making sweeping changes
   to this file, modifying gdb-events.sh and using its output may
   prove easier. */


#ifndef GDB_EVENTS_H
#define GDB_EVENTS_H


/* COMPAT: pointer variables for old, unconverted events.
   A call to set_gdb_events() will automatically update these. */



/* Type definition of all hook functions.  Recommended pratice is to
   first declare each hook function using the below ftype and then
   define it.  */

typedef void (gdb_events_breakpoint_create_ftype) (int b);
typedef void (gdb_events_breakpoint_delete_ftype) (int b);
typedef void (gdb_events_breakpoint_modify_ftype) (int b);
typedef void (gdb_events_tracepoint_create_ftype) (int number);
typedef void (gdb_events_tracepoint_delete_ftype) (int number);
typedef void (gdb_events_tracepoint_modify_ftype) (int number);
typedef void (gdb_events_architecture_changed_ftype) (void);


/* gdb-events: object. */

struct gdb_events
  {
    gdb_events_breakpoint_create_ftype *breakpoint_create;
    gdb_events_breakpoint_delete_ftype *breakpoint_delete;
    gdb_events_breakpoint_modify_ftype *breakpoint_modify;
    gdb_events_tracepoint_create_ftype *tracepoint_create;
    gdb_events_tracepoint_delete_ftype *tracepoint_delete;
    gdb_events_tracepoint_modify_ftype *tracepoint_modify;
    gdb_events_architecture_changed_ftype *architecture_changed;
  };


/* Interface into events functions.
   Where a *_p() predicate is present, it must be called before
   calling the hook proper.  */
extern void breakpoint_create_event (int b);
extern void breakpoint_delete_event (int b);
extern void breakpoint_modify_event (int b);
extern void tracepoint_create_event (int number);
extern void tracepoint_delete_event (int number);
extern void tracepoint_modify_event (int number);
extern void architecture_changed_event (void);

/* Install custom gdb-events hooks.  */
extern struct gdb_events *deprecated_set_gdb_event_hooks (struct gdb_events *vector);

/* Deliver any pending events.  */
extern void gdb_events_deliver (struct gdb_events *vector);

/* Clear event handlers.  */
extern void clear_gdb_event_hooks (void);

#endif
