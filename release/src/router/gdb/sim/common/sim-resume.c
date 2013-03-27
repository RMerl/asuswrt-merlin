/* Generic simulator resume.
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

#include "sim-main.h"
#include "sim-assert.h"

/* Halt the simulator after just one instruction */

static void
has_stepped (SIM_DESC sd,
	     void *data)
{
  ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  sim_engine_halt (sd, NULL, NULL, NULL_CIA, sim_stopped, SIM_SIGTRAP);
}


/* Generic resume - assumes the existance of sim_engine_run */

void
sim_resume (SIM_DESC sd,
	    int step,
	    int siggnal)
{
  sim_engine *engine = STATE_ENGINE (sd);
  jmp_buf buf;
  int jmpval;

  ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);

  /* we only want to be single stepping the simulator once */
  if (engine->stepper != NULL)
    {
      sim_events_deschedule (sd, engine->stepper);
      engine->stepper = NULL;
    }
  if (step)
    engine->stepper = sim_events_schedule (sd, 1, has_stepped, sd);

  sim_module_resume (sd);

  /* run/resume the simulator */
  engine->jmpbuf = &buf;
  jmpval = setjmp (buf);
  if (jmpval == sim_engine_start_jmpval
      || jmpval == sim_engine_restart_jmpval)
    {
      int last_cpu_nr = sim_engine_last_cpu_nr (sd);
      int next_cpu_nr = sim_engine_next_cpu_nr (sd);
      int nr_cpus = sim_engine_nr_cpus (sd);
      int sig_to_deliver;

      sim_events_preprocess (sd, last_cpu_nr >= nr_cpus, next_cpu_nr >= nr_cpus);
      if (next_cpu_nr >= nr_cpus)
	next_cpu_nr = 0;

      /* Only deliver the SIGGNAL [sic] the first time through - don't
         re-deliver any SIGGNAL during a restart.  NOTE: A new local
         variable is used to avoid problems with the automatic
         variable ``siggnal'' being trashed by a long jump.  */
      if (jmpval == sim_engine_start_jmpval)
	sig_to_deliver = siggnal;
      else
	sig_to_deliver = 0;

#ifdef SIM_CPU_EXCEPTION_RESUME
      {
	sim_cpu* cpu = STATE_CPU (sd, next_cpu_nr);
	SIM_CPU_EXCEPTION_RESUME(sd, cpu, sig_to_deliver);
      }
#endif

      sim_engine_run (sd, next_cpu_nr, nr_cpus, sig_to_deliver);
    }
  engine->jmpbuf = NULL;

  sim_module_suspend (sd);
}
