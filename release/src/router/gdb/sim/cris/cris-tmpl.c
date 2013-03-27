/* CRIS base simulator support code
   Copyright (C) 2004, 2005, 2006, 2007 Free Software Foundation, Inc.
   Contributed by Axis Communications.

This file is part of the GNU simulators.

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

/* The infrastructure is based on that of i960.c.  */

#define WANT_CPU

#include "sim-main.h"
#include "cgen-mem.h"
#include "cgen-ops.h"

#define MY(f) XCONCAT3(crisv,BASENUM,f)

/* Dispatcher for break insn.  */

USI
MY (f_break_handler) (SIM_CPU *cpu, USI breaknum, USI pc)
{
  SIM_DESC sd = CPU_STATE (cpu);
  USI ret = pc + 2;

  MY (f_h_pc_set) (cpu, ret);

  /* FIXME: Error out if IBR or ERP set.  */
  switch (breaknum)
    {
    case 13:
      MY (f_h_gr_set (cpu, 10,
		      cris_break_13_handler (cpu,
					     MY (f_h_gr_get (cpu, 9)),
					     MY (f_h_gr_get (cpu, 10)),
					     MY (f_h_gr_get (cpu, 11)),
					     MY (f_h_gr_get (cpu, 12)),
					     MY (f_h_gr_get (cpu, 13)),
					     MY (f_h_sr_get (cpu, 7)),
					     MY (f_h_sr_get (cpu, 11)),
					     pc)));
      break;

    case 14:
      sim_io_printf (sd, "%x\n", MY (f_h_gr_get (cpu, 3)));
      break;

    case 15:
      /* Re-use the Linux exit call.  */
      cris_break_13_handler (cpu, /* TARGET_SYS_exit */ 1, 0,
			     0, 0, 0, 0, 0, pc);

    default:
      abort ();
    }

  return MY (f_h_pc_get) (cpu);
}

/* Accessor function for simulator internal use.
   Note the contents of BUF are in target byte order.  */

int
MY (f_fetch_register) (SIM_CPU *current_cpu, int rn,
		      unsigned char *buf, int len ATTRIBUTE_UNUSED)
{
  SETTSI (buf, XCONCAT3(crisv,BASENUM,f_h_gr_get) (current_cpu, rn));
  return -1;
}

/* Accessor function for simulator internal use.
   Note the contents of BUF are in target byte order.  */

int
MY (f_store_register) (SIM_CPU *current_cpu, int rn,
		      unsigned char *buf, int len ATTRIBUTE_UNUSED)
{
  XCONCAT3(crisv,BASENUM,f_h_gr_set) (current_cpu, rn, GETTSI (buf));
  return -1;
}

#if WITH_PROFILE_MODEL_P

/* FIXME: Some of these should be inline or macros.  Later.  */

/* Initialize cycle counting for an insn.
   FIRST_P is non-zero if this is the first insn in a set of parallel
   insns.  */

void
MY (f_model_insn_before) (SIM_CPU *current_cpu, int first_p ATTRIBUTE_UNUSED)
{
  /* To give the impression that we actually know what PC is, we have to
     dump register contents *before* the *next* insn, not after the
     *previous* insn.  Uhh...  */

  /* FIXME: Move this to separate, overridable function.  */
  if ((CPU_CRIS_MISC_PROFILE (current_cpu)->flags
       & FLAG_CRIS_MISC_PROFILE_XSIM_TRACE)
#ifdef GET_H_INSN_PREFIXED_P
      /* For versions with prefixed insns, trace the combination as
	 one insn.  */
      && !GET_H_INSN_PREFIXED_P ()
#endif
      && 1)
  {
    int i;
    char flags[7];
    unsigned64 cycle_count;

    SIM_DESC sd = CPU_STATE (current_cpu);

    cris_trace_printf (sd, current_cpu, "%lx ",
		       0xffffffffUL & (unsigned long) (CPU (h_pc)));

    for (i = 0; i < 15; i++)
      cris_trace_printf (sd, current_cpu, "%lx ",
			 0xffffffffUL
			 & (unsigned long) (XCONCAT3(crisv,BASENUM,
						     f_h_gr_get) (current_cpu,
								  i)));
    flags[0] = GET_H_IBIT () != 0 ? 'I' : 'i';
    flags[1] = GET_H_XBIT () != 0 ? 'X' : 'x';
    flags[2] = GET_H_NBIT () != 0 ? 'N' : 'n';
    flags[3] = GET_H_ZBIT () != 0 ? 'Z' : 'z';
    flags[4] = GET_H_VBIT () != 0 ? 'V' : 'v';
    flags[5] = GET_H_CBIT () != 0 ? 'C' : 'c';
    flags[6] = 0;

    /* For anything else than basic tracing we'd add stall cycles for
       e.g. unaligned accesses.  FIXME: add --cris-trace=x options to
       match --cris-cycles=x.  */
    cycle_count
      = (CPU_CRIS_MISC_PROFILE (current_cpu)->basic_cycle_count
	 - CPU_CRIS_PREV_MISC_PROFILE (current_cpu)->basic_cycle_count);

    /* Emit ACR after flags and cycle count for this insn.  */
    if (BASENUM == 32)
      cris_trace_printf (sd, current_cpu, "%s %d %lx\n", flags,
			 (int) cycle_count,
			 0xffffffffUL
			 & (unsigned long) (XCONCAT3(crisv,BASENUM,
						     f_h_gr_get) (current_cpu,
								  15)));
    else
      cris_trace_printf (sd, current_cpu, "%s %d\n", flags,
			 (int) cycle_count);

    CPU_CRIS_PREV_MISC_PROFILE (current_cpu)[0]
      = CPU_CRIS_MISC_PROFILE (current_cpu)[0];
  }
}

/* Record the cycles computed for an insn.
   LAST_P is non-zero if this is the last insn in a set of parallel insns,
   and we update the total cycle count.
   CYCLES is the cycle count of the insn.  */

void
MY (f_model_insn_after) (SIM_CPU *current_cpu, int last_p ATTRIBUTE_UNUSED,
			 int cycles)
{
  PROFILE_DATA *p = CPU_PROFILE_DATA (current_cpu);

  PROFILE_MODEL_TOTAL_CYCLES (p) += cycles;
  CPU_CRIS_MISC_PROFILE (current_cpu)->basic_cycle_count += cycles;
  PROFILE_MODEL_CUR_INSN_CYCLES (p) = cycles;

#if WITH_HW
  /* For some reason, we don't get to the sim_events_tick call in
     cgen-run.c:engine_run_1.  Besides, more than one cycle has
     passed, so we want sim_events_tickn anyway.  The "events we want
     to process" is usually to initiate an interrupt, but might also
     be other events.  We can't do the former until the main loop is
     at point where it accepts changing the PC without internal
     inconsistency, so just set a flag and wait.  */
  if (sim_events_tickn (CPU_STATE (current_cpu), cycles))
    STATE_EVENTS (CPU_STATE (current_cpu))->work_pending = 1;
#endif
}

/* Initialize cycle counting for an insn.
   FIRST_P is non-zero if this is the first insn in a set of parallel
   insns.  */

void
MY (f_model_init_insn_cycles) (SIM_CPU *current_cpu ATTRIBUTE_UNUSED,
			       int first_p ATTRIBUTE_UNUSED)
{
  abort ();
}

/* Record the cycles computed for an insn.
   LAST_P is non-zero if this is the last insn in a set of parallel insns,
   and we update the total cycle count.  */

void
MY (f_model_update_insn_cycles) (SIM_CPU *current_cpu ATTRIBUTE_UNUSED,
				 int last_p ATTRIBUTE_UNUSED)
{
  abort ();
}

#if 0
void
MY (f_model_record_cycles) (SIM_CPU *current_cpu, unsigned long cycles)
{
  abort ();
}

void
MY (f_model_mark_get_h_gr) (SIM_CPU *current_cpu, ARGBUF *abuf)
{
  abort ();
}

void
MY (f_model_mark_set_h_gr) (SIM_CPU *current_cpu, ARGBUF *abuf)
{
  abort ();
}
#endif

/* Create the context for a thread.  */

void *
MY (make_thread_cpu_data) (SIM_CPU *current_cpu, void *context)
{
  void *info = xmalloc (current_cpu->thread_cpu_data_size);

  if (context != NULL)
    memcpy (info,
	    context,
	    current_cpu->thread_cpu_data_size);
  else
    memset (info, 0, current_cpu->thread_cpu_data_size),abort();
  return info;
}

/* Hook function for per-cpu simulator initialization.  */

void
MY (f_specific_init) (SIM_CPU *current_cpu)
{
  current_cpu->make_thread_cpu_data = MY (make_thread_cpu_data);
  current_cpu->thread_cpu_data_size = sizeof (current_cpu->cpu_data);
#if WITH_HW
  current_cpu->deliver_interrupt = MY (deliver_interrupt);
#endif
}

/* Model function for arbitrary single stall cycles.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_stall)) (SIM_CPU *current_cpu ATTRIBUTE_UNUSED,
			  const IDESC *idesc,
			  int unit_num,
			  int referenced ATTRIBUTE_UNUSED)
{
  return idesc->timing->units[unit_num].done;
}

#ifndef SPECIFIC_U_SKIP4_FN

/* Model function for u-skip4 unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_skip4)) (SIM_CPU *current_cpu,
			  const IDESC *idesc,
			  int unit_num,
			  int referenced ATTRIBUTE_UNUSED)
{
  /* Handle PC not being updated with pbb.  FIXME: What if not pbb?  */
  CPU (h_pc) += 4;
  return idesc->timing->units[unit_num].done;
}

#endif

#ifndef SPECIFIC_U_EXEC_FN

/* Model function for u-exec unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_exec)) (SIM_CPU *current_cpu,
			 const IDESC *idesc,
			 int unit_num, int referenced ATTRIBUTE_UNUSED)
{
  /* Handle PC not being updated with pbb.  FIXME: What if not pbb?  */
  CPU (h_pc) += 2;
  return idesc->timing->units[unit_num].done;
}
#endif

#ifndef SPECIFIC_U_MEM_FN

/* Model function for u-mem unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_mem)) (SIM_CPU *current_cpu ATTRIBUTE_UNUSED,
			const IDESC *idesc,
			int unit_num,
			int referenced ATTRIBUTE_UNUSED)
{
  return idesc->timing->units[unit_num].done;
}
#endif

#ifndef SPECIFIC_U_CONST16_FN

/* Model function for u-const16 unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_const16)) (SIM_CPU *current_cpu,
			    const IDESC *idesc,
			    int unit_num,
			    int referenced ATTRIBUTE_UNUSED)
{
  CPU (h_pc) += 2;
  return idesc->timing->units[unit_num].done;
}
#endif /* SPECIFIC_U_CONST16_FN */

#ifndef SPECIFIC_U_CONST32_FN

/* This will be incorrect for early models, where a dword always take
   two cycles.  */
#define CRIS_MODEL_MASK_PC_STALL 2

/* Model function for u-const32 unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_const32)) (SIM_CPU *current_cpu,
			    const IDESC *idesc,
			    int unit_num,
			    int referenced ATTRIBUTE_UNUSED)
{
  int unaligned_extra
    = (((CPU (h_pc) + 2) & CRIS_MODEL_MASK_PC_STALL)
       == CRIS_MODEL_MASK_PC_STALL);

  /* Handle PC not being updated with pbb.  FIXME: What if not pbb?  */
  CPU_CRIS_MISC_PROFILE (current_cpu)->unaligned_mem_dword_count
    += unaligned_extra;

  CPU (h_pc) += 4;
  return idesc->timing->units[unit_num].done;
}
#endif /* SPECIFIC_U_CONST32_FN */

#ifndef SPECIFIC_U_MOVEM_FN

/* Model function for u-movem unit.  */

int
MY (XCONCAT3 (f_model_crisv,BASENUM,
	      _u_movem)) (SIM_CPU *current_cpu ATTRIBUTE_UNUSED,
			  const IDESC *idesc ATTRIBUTE_UNUSED,
			  int unit_num ATTRIBUTE_UNUSED,
			  int referenced ATTRIBUTE_UNUSED,
			  INT limreg)
{
  /* FIXME: Add cycles for misalignment.  */

  if (limreg == -1)
    abort ();

  /* We don't record movem move cycles in movemsrc_stall_count since
     those cycles have historically been handled as ordinary cycles.  */
  return limreg + 1;
}
#endif /* SPECIFIC_U_MOVEM_FN */

#endif /* WITH_PROFILE_MODEL_P */
