/* Main simulator loop for CGEN-based simulators.
   Copyright (C) 1998, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Solutions.

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

/* ??? These are old notes, kept around for now.
   Collecting profile data and tracing slow us down so we don't do them in
   "fast mode".
   There are 6 possibilities on 2 axes:
   - no-scaching, insn-scaching, basic-block-scaching
   - run with full features or run fast
   Supporting all six possibilities in one executable is a bit much but
   supporting full/fast seems reasonable.
   If the scache is configured in it is always used.
   If pbb-scaching is configured in it is always used.
   ??? Sometimes supporting more than one set of semantic functions will make
   the simulator too large - this should be configurable.  Blah blah blah.
   ??? Supporting full/fast can be more modular, blah blah blah.
   When the framework is more modular, this can be.
*/

#include "sim-main.h"
#include "sim-assert.h"

#ifndef SIM_ENGINE_PREFIX_HOOK
#define SIM_ENGINE_PREFIX_HOOK(sd)
#endif
#ifndef SIM_ENGINE_POSTFIX_HOOK
#define SIM_ENGINE_POSTFIX_HOOK(sd)
#endif

static sim_event_handler has_stepped;
static void prime_cpu (SIM_CPU *, int);
static void engine_run_1 (SIM_DESC, int, int);
static void engine_run_n (SIM_DESC, int, int, int, int);

/* sim_resume for cgen */

void
sim_resume (SIM_DESC sd, int step, int siggnal)
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

#if WITH_SCACHE
  if (USING_SCACHE_P (sd))
    scache_flush (sd);
#endif

  /* run/resume the simulator */

  sim_engine_set_run_state (sd, sim_running, 0);

  engine->jmpbuf = &buf;
  jmpval = setjmp (buf);
  if (jmpval == sim_engine_start_jmpval
      || jmpval == sim_engine_restart_jmpval)
    {
      int last_cpu_nr = sim_engine_last_cpu_nr (sd);
      int next_cpu_nr = sim_engine_next_cpu_nr (sd);
      int nr_cpus = sim_engine_nr_cpus (sd);
      /* ??? Setting max_insns to 0 allows pbb/jit code to run wild and is
	 useful if all one wants to do is run a benchmark.  Need some better
	 way to identify this case.  */
      int max_insns = (step
		       ? 1
		       : (nr_cpus == 1
			  /*&& wip:no-events*/
			  /* Don't do this if running under gdb, need to
			     poll ui for events.  */
			  && STATE_OPEN_KIND (sd) == SIM_OPEN_STANDALONE)
		       ? 0
		       : 8); /*FIXME: magic number*/
      int fast_p = STATE_RUN_FAST_P (sd);

      sim_events_preprocess (sd, last_cpu_nr >= nr_cpus, next_cpu_nr >= nr_cpus);
      if (next_cpu_nr >= nr_cpus)
	next_cpu_nr = 0;
      if (nr_cpus == 1)
	engine_run_1 (sd, max_insns, fast_p);
      else
	engine_run_n (sd, next_cpu_nr, nr_cpus, max_insns, fast_p);
    }
#if 1 /*wip*/
  else
    {
      /* Account for the last insn executed.  */
      SIM_CPU *cpu = STATE_CPU (sd, sim_engine_last_cpu_nr (sd));
      ++ CPU_INSN_COUNT (cpu);
      TRACE_INSN_FINI (cpu, NULL, 1);
    }
#endif

  engine->jmpbuf = NULL;

  {
    int i;
    int nr_cpus = sim_engine_nr_cpus (sd);

#if 0 /*wip,ignore*/
    /* If the loop exits, either we single-stepped or @cpu@_engine_stop
       was called.  */
    if (step)
      sim_engine_set_run_state (sd, sim_stopped, SIM_SIGTRAP);
    else
      sim_engine_set_run_state (sd, pending_reason, pending_sigrc);
#endif

    for (i = 0; i < nr_cpus; ++i)
      {
	SIM_CPU *cpu = STATE_CPU (sd, i);

	PROFILE_TOTAL_INSN_COUNT (CPU_PROFILE_DATA (cpu)) += CPU_INSN_COUNT (cpu);
      }
  }

  sim_module_suspend (sd);
}

/* Halt the simulator after just one instruction.  */

static void
has_stepped (SIM_DESC sd, void *data)
{
  ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  sim_engine_halt (sd, NULL, NULL, NULL_CIA, sim_stopped, SIM_SIGTRAP);
}

/* Prepare a cpu for running.
   MAX_INSNS is the number of insns to execute per time slice.
   If 0 it means the cpu can run as long as it wants (e.g. until the
   program completes).
   ??? Perhaps this should be an argument to the engine_fn.  */

static void
prime_cpu (SIM_CPU *cpu, int max_insns)
{
  CPU_MAX_SLICE_INSNS (cpu) = max_insns;
  CPU_INSN_COUNT (cpu) = 0;

  /* Initialize the insn descriptor table.
     This has to be done after all initialization so we just defer it to
     here.  */

  if (MACH_PREPARE_RUN (CPU_MACH (cpu)))
    (* MACH_PREPARE_RUN (CPU_MACH (cpu))) (cpu);
}

/* Main loop, for 1 cpu.  */

static void
engine_run_1 (SIM_DESC sd, int max_insns, int fast_p)
{
  sim_cpu *cpu = STATE_CPU (sd, 0);
  ENGINE_FN *fn = fast_p ? CPU_FAST_ENGINE_FN (cpu) : CPU_FULL_ENGINE_FN (cpu);

  prime_cpu (cpu, max_insns);

  while (1)
    {
      SIM_ENGINE_PREFIX_HOOK (sd);

      (*fn) (cpu);

      SIM_ENGINE_POSTFIX_HOOK (sd);

      /* process any events */
      if (sim_events_tick (sd))
	sim_events_process (sd);
    }
}

/* Main loop, for multiple cpus.  */

static void
engine_run_n (SIM_DESC sd, int next_cpu_nr, int nr_cpus, int max_insns, int fast_p)
{
  int i;
  ENGINE_FN *engine_fns[MAX_NR_PROCESSORS];

  for (i = 0; i < nr_cpus; ++i)
    {
      SIM_CPU *cpu = STATE_CPU (sd, i);

      engine_fns[i] = fast_p ? CPU_FAST_ENGINE_FN (cpu) : CPU_FULL_ENGINE_FN (cpu);
      prime_cpu (cpu, max_insns);
    }

  while (1)
    {
      SIM_ENGINE_PREFIX_HOOK (sd);

      /* FIXME: proper cycling of all of them, blah blah blah.  */
      while (next_cpu_nr != nr_cpus)
	{
	  SIM_CPU *cpu = STATE_CPU (sd, next_cpu_nr);

	  (* engine_fns[next_cpu_nr]) (cpu);
	  ++next_cpu_nr;
	}

      SIM_ENGINE_POSTFIX_HOOK (sd);

      /* process any events */
      if (sim_events_tick (sd))
	sim_events_process (sd);
    }
}
