/* Miscellaneous simulator utilities.
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

#ifndef SIM_UTILS_H
#define SIM_UTILS_H

/* Memory management with an allocator that clears memory before use. */

void *zalloc (unsigned long size);

#define ZALLOC(TYPE) (TYPE*)zalloc(sizeof (TYPE))
#define NZALLOC(TYPE,N) (TYPE*)zalloc(sizeof (TYPE) * (N))

void zfree(void*);

/* Turn VALUE into a string with commas.  */
char *sim_add_commas (char *, int, unsigned long);

/* Utilities for elapsed time reporting.  */

/* Opaque type, known only inside sim_elapsed_time_foo fns. Externally
   it is known to never have the value zero. */
typedef unsigned long SIM_ELAPSED_TIME;


/* Get reference point for future call to sim_time_elapsed.  */
SIM_ELAPSED_TIME sim_elapsed_time_get (void);

/* Elapsed time in milliseconds since START.  */
unsigned long sim_elapsed_time_since (SIM_ELAPSED_TIME start);

/* Utilities for manipulating the load image.  */

SIM_RC sim_analyze_program (SIM_DESC sd, char *prog_name,
			    struct bfd *prog_bfd);

/* Load program PROG into the simulator using the function DO_LOAD.
   If PROG_BFD is non-NULL, the file has already been opened.
   If VERBOSE_P is non-zero statistics are printed of each loaded section
   and the transfer rate (for consistency with gdb).
   If LMA_P is non-zero the program sections are loaded at the LMA
   rather than the VMA
   If this fails an error message is printed and NULL is returned.
   If it succeeds the bfd is returned.
   NOTE: For historical reasons, older hardware simulators incorrectly
   write the program sections at LMA interpreted as a virtual address.
   This is still accommodated for backward compatibility reasons. */

typedef int sim_write_fn PARAMS ((SIM_DESC sd, SIM_ADDR mem,
				      unsigned char *buf, int length));
struct bfd *sim_load_file (SIM_DESC sd, const char *myname,
			   host_callback *callback, char *prog,
			   struct bfd *prog_bfd, int verbose_p,
			   int lma_p, sim_write_fn do_load);

/* Internal version of sim_do_command, include formatting */
void sim_do_commandf (SIM_DESC sd, const char *fmt, ...);


/* These are defined in callback.c as cover functions to the vprintf
   callbacks.  */

void sim_cb_printf (host_callback *, const char *, ...);
void sim_cb_eprintf (host_callback *, const char *, ...);


/* sim-basics.h defines a number of enumerations, convert each of them
   to a string representation */
const char *map_to_str (unsigned map);
const char *access_to_str (unsigned access);
const char *transfer_to_str (unsigned transfer);

#endif
