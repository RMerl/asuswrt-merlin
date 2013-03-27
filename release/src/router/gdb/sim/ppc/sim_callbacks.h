/*  This file is part of the program psim.

    Copyright (C) 1994-1995,1998, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#ifndef _SIM_CALLBACKS_H_
#define _SIM_CALLBACKS_H_

/* Simulator output:

   Functions to report diagnostic information to the user. */

#define printf_filtered sim_io_printf_filtered
void sim_io_printf_filtered
(const char *msg, ...) __attribute__ ((format (printf, 1, 2)));

void NORETURN error
(const char *msg, ...);


/* External environment:

   Some HOST OS's require regular polling so that external events such
   as GUI io can be handled. */

void sim_io_poll_quit
(void);



/* Model I/O:

   These functions provide the interface between the modeled targets
   standard input and output streams and the hosts external
   environment.

   The underlying model is responsible for co-ordinating I/O
   interactions such as:

        main()
        {
          const char mess[] = "Hello World\r\n";
          int out;
          out = dup(1);
          close(1);
          write(out, mess, sizeof(mess))
        }

   That is to say, the underlying model would, in implementing dup()
   recorded the fact that any output directed at fid-OUT should be
   displayed using sim_io_write_stdout().  While for the code stub:

        main()
        {
          const char mess[] = "Hello World\r\n";
          int out;
          close(1);
          out = open("output", 0x0001|0x0200, 0); // write+create
          out = dup(1);
          write(out, mess, sizeof(mess))
        }

   would result in output to fid-1 being directed towards the file
   "output". */


int
sim_io_write_stdout
(const char *buf,
 int sizeof_buf);

int
sim_io_write_stderr
(const char *buf,
 int sizeof_buf);

/* read results */
enum { sim_io_eof = -1, sim_io_not_ready = -2, }; /* See: IEEE 1275 */
  
int
sim_io_read_stdin
(char *buf,
 int sizeof_buf);

#define flush_stdoutput sim_io_flush_stdoutput
void sim_io_flush_stdoutput
(void);


/* Simulator instance.  */
extern psim *simulator;


/* Memory management with an allocator that clears memory before use. */

void *zalloc
(long size);

#define ZALLOC(TYPE) (TYPE*)zalloc(sizeof (TYPE))

void zfree(void*);

#endif
