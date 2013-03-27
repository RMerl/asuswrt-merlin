/* Simulator watchpoint support.
   Copyright (C) 1997, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

This file is part of GDB, the GNU debugger.

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


#ifndef SIM_WATCH_H
#define SIM_WATCH_H

typedef enum {
  invalid_watchpoint = -1,
  pc_watchpoint,
  clock_watchpoint,
  cycles_watchpoint,
  nr_watchpoint_types,
} watchpoint_type;

typedef struct _sim_watch_point sim_watch_point;
struct _sim_watch_point {
  int ident;
  watchpoint_type type;
  int interrupt_nr; /* == nr_interrupts -> breakpoint */
  int is_periodic;
  int is_within;
  unsigned long arg0;
  unsigned long arg1;
  sim_event *event;
  sim_watch_point *next;
};


typedef struct _sim_watchpoints {

  /* Pointer into the host's data structures specifying the
     address/size of the program-counter */
  /* FIXME: In the future this shall be generalized so that any of the
     N processors M registers can be watched */
  void *pc;
  int sizeof_pc;

  /* Pointer to the handler for interrupt watchpoints */
  /* FIXME: can this be done better? */
  /* NOTE: For the DATA arg, the handler is passed a (char**) pointer
     that is an offset into the INTERRUPT_NAMES vector.  Use
     arithmetic to determine the interrupt-nr. */
  sim_event_handler *interrupt_handler;

  /* Pointer to a null terminated list of interrupt names */
  /* FIXME: can this be done better?  Look at the PPC's interrupt
     mechanism and table for a rough idea of where it will go next */
  int nr_interrupts;
  char **interrupt_names;

  /* active watchpoints */
  int last_point_nr;
  sim_watch_point *points;

} sim_watchpoints;

/* Watch install handler.  */
MODULE_INSTALL_FN sim_watchpoint_install;

#endif /* SIM_WATCH_H */
