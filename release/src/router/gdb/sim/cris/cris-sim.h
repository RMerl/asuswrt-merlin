/* Collection of junk for CRIS.
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

/* For other arch:s, this file is described as a "collection of junk", so
   let's collect some nice junk of our own.  Keep it; it might be useful
   some day!  */

#ifndef CRIS_SIM_H
#define CRIS_SIM_H

typedef struct {
  /* Whether the branch for the current insn was taken.  Placed first
     here, in hope it'll get closer to the main simulator data.  */
  USI branch_taken;

  /* PC of the insn of the branch.  */
  USI old_pc;

  /* Static cycle count for all insns executed so far, including
     non-context-specific stall cycles, for example when adding to PC.  */
  unsigned64 basic_cycle_count;

  /* Stall cycles for unaligned access of memory operands.  FIXME:
     Should or should not include unaligned [PC+] operands?  */
  unsigned64 unaligned_mem_dword_count;

  /* Context-specific stall cycles.  */
  unsigned64 memsrc_stall_count;
  unsigned64 memraw_stall_count;
  unsigned64 movemsrc_stall_count;
  unsigned64 movemaddr_stall_count;
  unsigned64 movemdst_stall_count;
  unsigned64 mulsrc_stall_count;
  unsigned64 jumpsrc_stall_count;
  unsigned64 branch_stall_count;
  unsigned64 jumptarget_stall_count;

  /* What kind of target-specific trace to perform.  */
  int flags;

  /* Just the basic cycle count.  */
#define FLAG_CRIS_MISC_PROFILE_SIMPLE 1

  /* Show unaligned accesses.  */
#define FLAG_CRIS_MISC_PROFILE_UNALIGNED 2

  /* Show schedulable entities.  */
#define FLAG_CRIS_MISC_PROFILE_SCHEDULABLE 4

  /* Show everything.  */
#define FLAG_CRIS_MISC_PROFILE_ALL		\
 (FLAG_CRIS_MISC_PROFILE_SIMPLE			\
  | FLAG_CRIS_MISC_PROFILE_UNALIGNED		\
  | FLAG_CRIS_MISC_PROFILE_SCHEDULABLE)

  /* Emit trace of each insn, xsim style.  */
#define FLAG_CRIS_MISC_PROFILE_XSIM_TRACE 8

#define N_CRISV32_BRANCH_PREDICTORS 256
  unsigned char branch_predictors[N_CRISV32_BRANCH_PREDICTORS];

} CRIS_MISC_PROFILE;

/* Handler prototypes for functions called from the CGEN description.  */

extern USI cris_bmod_handler (SIM_CPU *, UINT, USI);
extern void cris_flush_simulator_decode_cache (SIM_CPU *, USI);
extern USI crisv10f_break_handler (SIM_CPU *, USI, USI);
extern USI crisv32f_break_handler (SIM_CPU *, USI, USI);
extern USI cris_break_13_handler (SIM_CPU *, USI, USI, USI, USI, USI, USI,
				  USI, USI);
extern char cris_have_900000xxif;
enum cris_unknown_syscall_action_type
  { CRIS_USYSC_MSG_STOP, CRIS_USYSC_MSG_ENOSYS, CRIS_USYSC_QUIET_ENOSYS };
extern enum cris_unknown_syscall_action_type cris_unknown_syscall_action;
enum cris_interrupt_type { CRIS_INT_NMI, CRIS_INT_RESET, CRIS_INT_INT };
extern int crisv10deliver_interrupt (SIM_CPU *,
				     enum cris_interrupt_type,
				       unsigned int);
extern int crisv32deliver_interrupt (SIM_CPU *,
				     enum cris_interrupt_type,
				     unsigned int);

/* Using GNU syntax (not C99) so we can compile this on RH 6.2
   (egcs-1.1.2/gcc-2.91.66).  */
#define cris_trace_printf(SD, CPU, FMT...)			\
  do								\
    {								\
      if (TRACE_FILE (STATE_TRACE_DATA (SD)) != NULL)		\
	fprintf (TRACE_FILE (CPU_TRACE_DATA (CPU)), FMT);	\
      else							\
	sim_io_printf (SD, FMT);				\
    }								\
  while (0)

#if WITH_PROFILE_MODEL_P
#define crisv32f_branch_taken(cpu, oldpc, newpc, taken)	\
  do								\
    {								\
      CPU_CRIS_MISC_PROFILE (cpu)->old_pc = oldpc;		\
      CPU_CRIS_MISC_PROFILE (cpu)->branch_taken = taken;	\
    }								\
  while (0)
#else
#define crisv32f_branch_taken(cpu, oldpc, newpc, taken)
#endif

#define crisv10f_branch_taken(cpu, oldpc, newpc, taken)

#define crisv32f_read_supr(cpu, index)				\
 (cgen_rtx_error (current_cpu,					\
		  "Read of support register is unimplemented"),	\
  0)

#define crisv32f_write_supr(cpu, index, val)			\
 cgen_rtx_error (current_cpu,					\
		 "Write to support register is unimplemented")	\

#define crisv32f_rfg_handler(cpu, pc)				\
 cgen_rtx_error (current_cpu, "RFG isn't implemented")

#define crisv32f_halt_handler(cpu, pc)				\
 (cgen_rtx_error (current_cpu, "HALT isn't implemented"), 0)

#define crisv32f_fidxi_handler(cpu, pc, indx)			\
 (cgen_rtx_error (current_cpu, "FIDXI isn't implemented"), 0)

#define crisv32f_ftagi_handler(cpu, pc, indx)			\
 (cgen_rtx_error (current_cpu, "FTAGI isn't implemented"), 0)

#define crisv32f_fidxd_handler(cpu, pc, indx)			\
 (cgen_rtx_error (current_cpu, "FIDXD isn't implemented"), 0)

#define crisv32f_ftagd_handler(cpu, pc, indx)			\
 (cgen_rtx_error (current_cpu, "FTAGD isn't implemented"), 0)

/* We have nothing special to do when interrupts or NMI are enabled
   after having been disabled, so empty macros are enough for these
   hooks.  */
#define crisv32f_interrupts_enabled(cpu)
#define crisv32f_nmi_enabled(cpu)

/* Better warn for this case here, because everything needed is
   somewhere within the CPU.  Compare to trying to use interrupts and
   NMI, which would fail earlier, when trying to make nonexistent
   external components generate those exceptions.  */
#define crisv32f_single_step_enabled(cpu)			\
 ((crisv32f_h_qbit_get (cpu) != 0				\
   || (crisv32f_h_sr_get (cpu, H_SR_SPC) & ~1) != 0)		\
  ? (cgen_rtx_error (cpu,					\
		     "single-stepping isn't implemented"), 0)	\
  : 0)

/* We don't need to track the value of the PID register here.  */
#define crisv32f_write_pid_handler(cpu, val)

/* Neither do we need to know of transitions to user mode.  */
#define crisv32f_usermode_enabled(cpu)

/* House-keeping exported from traps.c  */
extern void cris_set_callbacks (host_callback *);

/* FIXME: Add more junk.  */
#endif
