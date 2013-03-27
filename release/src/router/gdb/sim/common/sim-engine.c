/* Generic simulator halt/restart.
   Copyright (C) 1997, 1998, 2007 Free Software Foundation, Inc.
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

#include <stdio.h>

#include "sim-main.h"
#include "sim-assert.h"

/* Get the run state.
   REASON/SIGRC are the values returned by sim_stop_reason.
   ??? Should each cpu have its own copy?  */

void
sim_engine_get_run_state (SIM_DESC sd, enum sim_stop *reason, int *sigrc)
{
  sim_engine *engine = STATE_ENGINE (sd);
  ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  *reason = engine->reason;
  *sigrc = engine->sigrc;
}

/* Set the run state to REASON/SIGRC.
   REASON/SIGRC are the values returned by sim_stop_reason.
   ??? Should each cpu have its own copy?  */

void
sim_engine_set_run_state (SIM_DESC sd, enum sim_stop reason, int sigrc)
{
  sim_engine *engine = STATE_ENGINE (sd);
  ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  engine->reason = reason;
  engine->sigrc = sigrc;
}

/* Generic halt */

void
sim_engine_halt (SIM_DESC sd,
		 sim_cpu *last_cpu,
		 sim_cpu *next_cpu, /* NULL - use default */
		 sim_cia cia,
		 enum sim_stop reason,
		 int sigrc)
{
  sim_engine *engine = STATE_ENGINE (sd);
  ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  if (engine->jmpbuf != NULL)
    {
      jmp_buf *halt_buf = engine->jmpbuf;
      engine->last_cpu = last_cpu;
      engine->next_cpu = next_cpu;
      engine->reason = reason;
      engine->sigrc = sigrc;

      SIM_ENGINE_HALT_HOOK (sd, last_cpu, cia);

#ifdef SIM_CPU_EXCEPTION_SUSPEND
      if (last_cpu != NULL && reason != sim_exited)
	SIM_CPU_EXCEPTION_SUSPEND (sd, last_cpu, sim_signal_to_host (sd, sigrc));
#endif

      longjmp (*halt_buf, sim_engine_halt_jmpval);
    }
  else
    {
      sim_io_error (sd, "sim_halt - bad long jump");
      abort ();
    }
}


/* Generic restart */

void
sim_engine_restart (SIM_DESC sd,
		    sim_cpu *last_cpu,
		    sim_cpu *next_cpu,
		    sim_cia cia)
{
  sim_engine *engine = STATE_ENGINE (sd);
  ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  if (engine->jmpbuf != NULL)
    {
      jmp_buf *halt_buf = engine->jmpbuf;
      engine->last_cpu = last_cpu;
      engine->next_cpu = next_cpu;
      SIM_ENGINE_RESTART_HOOK (sd, last_cpu, cia);
      longjmp (*halt_buf, sim_engine_restart_jmpval);
    }
  else
    sim_io_error (sd, "sim_restart - bad long jump");
}


/* Generic error code */

void
sim_engine_vabort (SIM_DESC sd,
		   sim_cpu *cpu,
		   sim_cia cia,
		   const char *fmt,
		   va_list ap)
{
  ASSERT (sd == NULL || STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  if (sd == NULL)
    {
      vfprintf (stderr, fmt, ap);
      fprintf (stderr, "\nQuit\n");
      abort ();
    }
  else if (STATE_ENGINE (sd)->jmpbuf == NULL)
    {
      sim_io_evprintf (sd, fmt, ap);
      sim_io_eprintf (sd, "\n");
      sim_io_error (sd, "Quit Simulator");
      abort ();
    }
  else
    {
      sim_io_evprintf (sd, fmt, ap);
      sim_io_eprintf (sd, "\n");
      sim_engine_halt (sd, cpu, NULL, cia, sim_stopped, SIM_SIGABRT);
    }
}

void
sim_engine_abort (SIM_DESC sd,
		  sim_cpu *cpu,
		  sim_cia cia,
		  const char *fmt,
		  ...)
{
  va_list ap;
  ASSERT (sd == NULL || STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  va_start(ap, fmt);
  sim_engine_vabort (sd, cpu, cia, fmt, ap);
  va_end (ap);
}


/* Generic next/last cpu */

int
sim_engine_last_cpu_nr (SIM_DESC sd)
{
  sim_engine *engine = STATE_ENGINE (sd);
  if (engine->last_cpu != NULL)
    return engine->last_cpu - STATE_CPU (sd, 0);
  else
    return MAX_NR_PROCESSORS;
}

int
sim_engine_next_cpu_nr (SIM_DESC sd)
{
  sim_engine *engine = STATE_ENGINE (sd);
  if (engine->next_cpu != NULL)
    return engine->next_cpu - STATE_CPU (sd, 0);
  else
    return sim_engine_last_cpu_nr (sd) + 1;
}

int
sim_engine_nr_cpus (SIM_DESC sd)
{
  sim_engine *engine = STATE_ENGINE (sd);
  return engine->nr_cpus;
}




/* Initialization */

static SIM_RC
sim_engine_init (SIM_DESC sd)
{
  /* initialize the start/stop/resume engine */
  sim_engine *engine = STATE_ENGINE (sd);
  engine->jmpbuf = NULL;
  engine->last_cpu = NULL;
  engine->next_cpu = NULL;
  engine->nr_cpus = MAX_NR_PROCESSORS;
  engine->reason = sim_running;
  engine->sigrc = 0;
  engine->stepper = NULL; /* sim_events_init will clean it up */
  return SIM_RC_OK;
}


SIM_RC
sim_engine_install (SIM_DESC sd)
{
  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  sim_module_add_init_fn (sd, sim_engine_init);
  return SIM_RC_OK;
}
