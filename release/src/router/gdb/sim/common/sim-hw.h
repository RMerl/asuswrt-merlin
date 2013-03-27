/* Device definitions.
   Copyright (C) 1998, 2007 Free Software Foundation, Inc.
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

#ifndef SIM_HW_H
#define SIM_HW_H


/* Establish this object */

SIM_RC sim_hw_install
(struct sim_state *sd);


/* Parse a hardware definition */

struct hw *sim_hw_parse
(struct sim_state *sd,
 const char *fmt,
 ...) __attribute__ ((format (printf, 2, 3)));


/* Print the hardware tree */

void sim_hw_print
(struct sim_state *sd,
 void (*print) (struct sim_state *, const char *, va_list ap));


/* Abort the simulation specifying HW as the reason */

void sim_hw_abort
(SIM_DESC sd,
 struct hw *hw,
 const char *fmt,
 ...) __attribute__ ((format (printf, 3, 4)));



/* CPU: The simulation is running and the current CPU/CIA
   initiates a data transfer. */

void sim_cpu_hw_io_read_buffer
(sim_cpu *cpu,
 sim_cia cia,
 struct hw *hw,
 void *dest,
 int space,
 unsigned_word addr,
 unsigned nr_bytes);

void sim_cpu_hw_io_write_buffer
(sim_cpu *cpu,
 sim_cia cia,
 struct hw *hw,
 const void *source,
 int space,
 unsigned_word addr,
 unsigned nr_bytes);



/* SYSTEM: A data transfer is being initiated by the system. */

unsigned sim_hw_io_read_buffer
(struct sim_state *sd,
 struct hw *hw,
 void *dest,
 int space,
 unsigned_word addr,
 unsigned nr_bytes);

unsigned sim_hw_io_write_buffer
(struct sim_state *sd,
 struct hw *hw,
 const void *source,
 int space,
 unsigned_word addr,
 unsigned nr_bytes);


#endif
