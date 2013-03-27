/* emulos.c -- Small OS emulation
   Copyright 1999, 2000, 2007 Free Software Foundation, Inc.
   Written by Stephane Carrez (stcarrez@worldnet.fr)

This file is part of GDB, GAS, and the GNU binutils.

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

#include "sim-main.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef WIN32
#include <sys/types.h>
#include <sys/time.h>

/* This file emulates some OS system calls.
   It's basically used to give access to the host OS facilities
   like: stdin, stdout, files, time of day.  */
static int bench_mode = -1;
static struct timeval bench_start;
static struct timeval bench_stop;

void
emul_bench (struct _sim_cpu* cpu)
{
  int op;

  op = cpu_get_d (cpu);
  switch (op)
    {
    case 0:
      bench_mode = 0;
      gettimeofday (&bench_start, 0);
      break;

    case 1:
      gettimeofday (&bench_stop, 0);
      if (bench_mode != 0)
        printf ("bench start not called...\n");
      bench_mode = 1;
      break;

    case 2:
      {
        int sz = 0;
        int addr = cpu_get_x (cpu);
        double t_start, t_stop, t;
        char buf[1024];

        op = cpu_get_y (cpu);
        t_start = (double) (bench_start.tv_sec) * 1.0e6;
        t_start += (double) (bench_start.tv_usec);
        t_stop  = (double) (bench_stop.tv_sec) * 1.0e6;
        t_stop  += (double) (bench_stop.tv_usec);
        
        while (sz < 1024)
          {
            buf[sz] = memory_read8 (cpu, addr);
            if (buf[sz] == 0)
              break;

            sz ++;
            addr++;
          }
        buf[1023] = 0;

        if (bench_mode != 1)
          printf ("bench_stop not called");

        bench_mode = -1;
        t = t_stop - t_start;
        printf ("%-40.40s [%6d] %3.3f us\n", buf,
                op, t / (double) (op));
        break;
      }
    }
}
#endif

void
emul_write(struct _sim_cpu* state)
{
  int addr = cpu_get_x (state) & 0x0FFFF;
  int size = cpu_get_d (state) & 0x0FFFF;

  if (addr + size > 0x0FFFF) {
    size = 0x0FFFF - addr;
  }
  state->cpu_running = 0;
  while (size)
    {
      uint8 val = memory_read8 (state, addr);
        
      write(0, &val, 1);
      addr ++;
      size--;
    }
}

/* emul_exit () is used by the default startup code of GCC to implement
   the exit ().  For a real target, this will create an ILLEGAL fault.
   But doing an exit () on a real target is really a non-sense.
   exit () is important for the validation of GCC.  The exit status
   is passed in 'D' register.  */
void
emul_exit (sim_cpu *cpu)
{
  sim_engine_halt (CPU_STATE (cpu), cpu,
		   NULL, NULL_CIA, sim_exited,
		   cpu_get_d (cpu));
}

void
emul_os (int code, sim_cpu *proc)
{
  proc->cpu_current_cycle = 8;
  switch (code)
    {
    case 0x0:
      break;

      /* 0xCD 0x01 */
    case 0x01:
      emul_write (proc);
      break;

      /* 0xCD 0x02 */
    case 0x02:
      break;

      /* 0xCD 0x03 */
    case 0x03:
      emul_exit (proc);
      break;

      /* 0xCD 0x04 */
    case 0x04:
#ifndef WIN32
      emul_bench (proc);
#endif
      break;
        
    default:
      break;
    }
}

