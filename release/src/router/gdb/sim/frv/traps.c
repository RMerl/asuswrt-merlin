/* frv trap support
   Copyright (C) 1999, 2000, 2001, 2003, 2007 Free Software Foundation, Inc.
   Contributed by Red Hat.

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

#define WANT_CPU frvbf
#define WANT_CPU_FRVBF

#include "sim-main.h"
#include "targ-vals.h"
#include "cgen-engine.h"
#include "cgen-par.h"
#include "sim-fpu.h"

#include "bfd.h"
#include "libiberty.h"

CGEN_ATTR_VALUE_ENUM_TYPE frv_current_fm_slot;

/* The semantic code invokes this for invalid (unrecognized) instructions.  */

SEM_PC
sim_engine_invalid_insn (SIM_CPU *current_cpu, IADDR cia, SEM_PC vpc)
{
  frv_queue_program_interrupt (current_cpu, FRV_ILLEGAL_INSTRUCTION);
  return vpc;
}

/* Process an address exception.  */

void
frv_core_signal (SIM_DESC sd, SIM_CPU *current_cpu, sim_cia cia,
		  unsigned int map, int nr_bytes, address_word addr,
		  transfer_type transfer, sim_core_signals sig)
{
  if (sig == sim_core_unaligned_signal)
    {
      if (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr400
	  || STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr450)
	frv_queue_data_access_error_interrupt (current_cpu, addr);
      else
	frv_queue_mem_address_not_aligned_interrupt (current_cpu, addr);
    }

  frv_term (sd);
  sim_core_signal (sd, current_cpu, cia, map, nr_bytes, addr, transfer, sig);
}

void
frv_sim_engine_halt_hook (SIM_DESC sd, SIM_CPU *current_cpu, sim_cia cia)
{
  int i;
  if (current_cpu != NULL)
    CIA_SET (current_cpu, cia);

  /* Invalidate the insn and data caches of all cpus.  */
  for (i = 0; i < MAX_NR_PROCESSORS; ++i)
    {
      current_cpu = STATE_CPU (sd, i);
      frv_cache_invalidate_all (CPU_INSN_CACHE (current_cpu), 0);
      frv_cache_invalidate_all (CPU_DATA_CACHE (current_cpu), 1);
    }
  frv_term (sd);
}

/* Read/write functions for system call interface.  */

static int
syscall_read_mem (host_callback *cb, struct cb_syscall *sc,
		  unsigned long taddr, char *buf, int bytes)
{
  SIM_DESC sd = (SIM_DESC) sc->p1;
  SIM_CPU *cpu = (SIM_CPU *) sc->p2;

  frv_cache_invalidate_all (CPU_DATA_CACHE (cpu), 1);
  return sim_core_read_buffer (sd, cpu, read_map, buf, taddr, bytes);
}

static int
syscall_write_mem (host_callback *cb, struct cb_syscall *sc,
		   unsigned long taddr, const char *buf, int bytes)
{
  SIM_DESC sd = (SIM_DESC) sc->p1;
  SIM_CPU *cpu = (SIM_CPU *) sc->p2;

  frv_cache_invalidate_all (CPU_INSN_CACHE (cpu), 0);
  frv_cache_invalidate_all (CPU_DATA_CACHE (cpu), 1);
  return sim_core_write_buffer (sd, cpu, write_map, buf, taddr, bytes);
}

/* Handle TRA and TIRA insns.  */
void
frv_itrap (SIM_CPU *current_cpu, PCADDR pc, USI base, SI offset)
{
  SIM_DESC sd = CPU_STATE (current_cpu);
  host_callback *cb = STATE_CALLBACK (sd);
  USI num = ((base + offset) & 0x7f) + 0x80;

#ifdef SIM_HAVE_BREAKPOINTS
  /* Check for breakpoints "owned" by the simulator first, regardless
     of --environment.  */
  if (num == TRAP_BREAKPOINT)
    {
      /* First try sim-break.c.  If it's a breakpoint the simulator "owns"
	 it doesn't return.  Otherwise it returns and let's us try.  */
      sim_handle_breakpoint (sd, current_cpu, pc);
      /* Fall through.  */
    }
#endif

  if (STATE_ENVIRONMENT (sd) == OPERATING_ENVIRONMENT)
    {
      frv_queue_software_interrupt (current_cpu, num);
      return;
    }

  switch (num)
    {
    case TRAP_SYSCALL :
      {
	CB_SYSCALL s;
	CB_SYSCALL_INIT (&s);
	s.func = GET_H_GR (7);
	s.arg1 = GET_H_GR (8);
	s.arg2 = GET_H_GR (9);
	s.arg3 = GET_H_GR (10);

	if (s.func == TARGET_SYS_exit)
	  {
	    sim_engine_halt (sd, current_cpu, NULL, pc, sim_exited, s.arg1);
	  }

	s.p1 = (PTR) sd;
	s.p2 = (PTR) current_cpu;
	s.read_mem = syscall_read_mem;
	s.write_mem = syscall_write_mem;
	cb_syscall (cb, &s);
	SET_H_GR (8, s.result);
	SET_H_GR (9, s.result2);
	SET_H_GR (10, s.errcode);
	break;
      }

    case TRAP_BREAKPOINT:
      sim_engine_halt (sd, current_cpu, NULL, pc, sim_stopped, SIM_SIGTRAP);
      break;

      /* Add support for dumping registers, either at fixed traps, or all
	 unknown traps if configured with --enable-sim-trapdump.  */
    default:
#if !TRAPDUMP
      frv_queue_software_interrupt (current_cpu, num);
      return;
#endif

#ifdef TRAP_REGDUMP1
    case TRAP_REGDUMP1:
#endif

#ifdef TRAP_REGDUMP2
    case TRAP_REGDUMP2:
#endif

#if TRAPDUMP || (defined (TRAP_REGDUMP1)) || (defined (TRAP_REGDUMP2))
      {
	char buf[256];
	int i, j;

	buf[0] = 0;
	if (STATE_TEXT_SECTION (sd)
	    && pc >= STATE_TEXT_START (sd)
	    && pc < STATE_TEXT_END (sd))
	  {
	    const char *pc_filename = (const char *)0;
	    const char *pc_function = (const char *)0;
	    unsigned int pc_linenum = 0;

	    if (bfd_find_nearest_line (STATE_PROG_BFD (sd),
				       STATE_TEXT_SECTION (sd),
				       (struct bfd_symbol **) 0,
				       pc - STATE_TEXT_START (sd),
				       &pc_filename, &pc_function, &pc_linenum)
		&& (pc_function || pc_filename))
	      {
		char *p = buf+2;
		buf[0] = ' ';
		buf[1] = '(';
		if (pc_function)
		  {
		    strcpy (p, pc_function);
		    p += strlen (p);
		  }
		else
		  {
		    char *q = (char *) strrchr (pc_filename, '/');
		    strcpy (p, (q) ? q+1 : pc_filename);
		    p += strlen (p);
		  }

		if (pc_linenum)
		  {
		    sprintf (p, " line %d", pc_linenum);
		    p += strlen (p);
		  }

		p[0] = ')';
		p[1] = '\0';
		if ((p+1) - buf > sizeof (buf))
		  abort ();
	      }
	  }

	sim_io_printf (sd,
		       "\nRegister dump,    pc = 0x%.8x%s, base = %u, offset = %d\n",
		       (unsigned)pc, buf, (unsigned)base, (int)offset);

	for (i = 0; i < 64; i += 8)
	  {
	    long g0 = (long)GET_H_GR (i);
	    long g1 = (long)GET_H_GR (i+1);
	    long g2 = (long)GET_H_GR (i+2);
	    long g3 = (long)GET_H_GR (i+3);
	    long g4 = (long)GET_H_GR (i+4);
	    long g5 = (long)GET_H_GR (i+5);
	    long g6 = (long)GET_H_GR (i+6);
	    long g7 = (long)GET_H_GR (i+7);

	    if ((g0 | g1 | g2 | g3 | g4 | g5 | g6 | g7) != 0)
	      sim_io_printf (sd,
			     "\tgr%02d - gr%02d:   0x%.8lx 0x%.8lx 0x%.8lx 0x%.8lx 0x%.8lx 0x%.8lx 0x%.8lx 0x%.8lx\n",
			     i, i+7, g0, g1, g2, g3, g4, g5, g6, g7);
	  }

	for (i = 0; i < 64; i += 8)
	  {
	    long f0 = (long)GET_H_FR (i);
	    long f1 = (long)GET_H_FR (i+1);
	    long f2 = (long)GET_H_FR (i+2);
	    long f3 = (long)GET_H_FR (i+3);
	    long f4 = (long)GET_H_FR (i+4);
	    long f5 = (long)GET_H_FR (i+5);
	    long f6 = (long)GET_H_FR (i+6);
	    long f7 = (long)GET_H_FR (i+7);

	    if ((f0 | f1 | f2 | f3 | f4 | f5 | f6 | f7) != 0)
	      sim_io_printf (sd,
			     "\tfr%02d - fr%02d:   0x%.8lx 0x%.8lx 0x%.8lx 0x%.8lx 0x%.8lx 0x%.8lx 0x%.8lx 0x%.8lx\n",
			     i, i+7, f0, f1, f2, f3, f4, f5, f6, f7);
	  }

	sim_io_printf (sd,
		       "\tlr/lcr/cc/ccc: 0x%.8lx 0x%.8lx 0x%.8lx 0x%.8lx\n",
		       (long)GET_H_SPR (272),
		       (long)GET_H_SPR (273),
		       (long)GET_H_SPR (256),
		       (long)GET_H_SPR (263));
      }
      break;
#endif
    }
}

/* Handle the MTRAP insn.  */
void
frv_mtrap (SIM_CPU *current_cpu)
{
  SIM_DESC sd = CPU_STATE (current_cpu);

  /* Check the status of media exceptions in MSR0.  */
  SI msr = GET_MSR (0);
  if (GET_MSR_AOVF (msr) || GET_MSR_MTT (msr) && STATE_ARCHITECTURE (sd)->mach != bfd_mach_fr550)
    frv_queue_program_interrupt (current_cpu, FRV_MP_EXCEPTION);
}

/* Handle the BREAK insn.  */
void
frv_break (SIM_CPU *current_cpu)
{
  IADDR pc;
  SIM_DESC sd = CPU_STATE (current_cpu);

#ifdef SIM_HAVE_BREAKPOINTS
  /* First try sim-break.c.  If it's a breakpoint the simulator "owns"
     it doesn't return.  Otherwise it returns and let's us try.  */
  pc = GET_H_PC ();
  sim_handle_breakpoint (sd, current_cpu, pc);
  /* Fall through.  */
#endif

  if (STATE_ENVIRONMENT (sd) != OPERATING_ENVIRONMENT)
    {
      /* Invalidate the insn cache because the debugger will presumably
	 replace the breakpoint insn with the real one.  */
#ifndef SIM_HAVE_BREAKPOINTS
      pc = GET_H_PC ();
#endif
      sim_engine_halt (sd, current_cpu, NULL, pc, sim_stopped, SIM_SIGTRAP);
    }

  frv_queue_break_interrupt (current_cpu);
}

/* Return from trap.  */
USI
frv_rett (SIM_CPU *current_cpu, PCADDR pc, BI debug_field)
{
  USI new_pc;
  /* if (normal running mode and debug_field==0
       PC=PCSR
       PSR.ET=1
       PSR.S=PSR.PS
     else if (debug running mode and debug_field==1)
       PC=(BPCSR)
       PSR.ET=BPSR.BET
       PSR.S=BPSR.BS
       change to normal running mode
  */
  int psr_s = GET_H_PSR_S ();
  int psr_et = GET_H_PSR_ET ();

  /* Check for exceptions in the priority order listed in the FRV Architecture
     Volume 2.  */
  if (! psr_s)
    {
      /* Halt if PSR.ET is not set.  See chapter 6 of the LSI.  */
      if (! psr_et)
	{
	  SIM_DESC sd = CPU_STATE (current_cpu);
	  sim_engine_halt (sd, current_cpu, NULL, pc, sim_stopped, SIM_SIGTRAP);
	}

      /* privileged_instruction interrupt will have already been queued by
	 frv_detect_insn_access_interrupts.  */
      new_pc = pc + 4;
    }
  else if (psr_et)
    {
      /* Halt if PSR.S is set.  See chapter 6 of the LSI.  */
      if (psr_s)
	{
	  SIM_DESC sd = CPU_STATE (current_cpu);
	  sim_engine_halt (sd, current_cpu, NULL, pc, sim_stopped, SIM_SIGTRAP);
	}

      frv_queue_program_interrupt (current_cpu, FRV_ILLEGAL_INSTRUCTION);
      new_pc = pc + 4;
    }
  else if (! CPU_DEBUG_STATE (current_cpu) && debug_field == 0)
    {
      USI psr = GET_PSR ();
      /* Return from normal running state.  */
      new_pc = GET_H_SPR (H_SPR_PCSR);
      SET_PSR_ET (psr, 1);
      SET_PSR_S (psr, GET_PSR_PS (psr));
      sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, H_SPR_PSR, psr);
    }
  else if (CPU_DEBUG_STATE (current_cpu) && debug_field == 1)
    {
      USI psr = GET_PSR ();
      /* Return from debug state.  */
      new_pc = GET_H_SPR (H_SPR_BPCSR);
      SET_PSR_ET (psr, GET_H_BPSR_BET ());
      SET_PSR_S (psr, GET_H_BPSR_BS ());
      sim_queue_fn_si_write (current_cpu, frvbf_h_spr_set, H_SPR_PSR, psr);
      CPU_DEBUG_STATE (current_cpu) = 0;
    }
  else
    new_pc = pc + 4;

  return new_pc;
}

/* Functions for handling non-excepting instruction side effects.  */
static SI next_available_nesr (SIM_CPU *current_cpu, SI current_index)
{
  FRV_REGISTER_CONTROL *control = CPU_REGISTER_CONTROL (current_cpu);
  if (control->spr[H_SPR_NECR].implemented)
    {
      int limit;
      USI necr = GET_NECR ();

      /* See if any NESRs are implemented. First need to check the validity of
	 the NECR.  */
      if (! GET_NECR_VALID (necr))
	return NO_NESR;

      limit = GET_NECR_NEN (necr);
      for (++current_index; current_index < limit; ++current_index)
	{
	  SI nesr = GET_NESR (current_index);
	  if (! GET_NESR_VALID (nesr))
	    return current_index;
	}
    }
  return NO_NESR;
}

static SI next_valid_nesr (SIM_CPU *current_cpu, SI current_index)
{
  FRV_REGISTER_CONTROL *control = CPU_REGISTER_CONTROL (current_cpu);
  if (control->spr[H_SPR_NECR].implemented)
    {
      int limit;
      USI necr = GET_NECR ();

      /* See if any NESRs are implemented. First need to check the validity of
	 the NECR.  */
      if (! GET_NECR_VALID (necr))
	return NO_NESR;

      limit = GET_NECR_NEN (necr);
      for (++current_index; current_index < limit; ++current_index)
	{
	  SI nesr = GET_NESR (current_index);
	  if (GET_NESR_VALID (nesr))
	    return current_index;
	}
    }
  return NO_NESR;
}

BI
frvbf_check_non_excepting_load (
  SIM_CPU *current_cpu, SI base_index, SI disp_index, SI target_index,
  SI immediate_disp, QI data_size, BI is_float
)
{
  BI rc = 1; /* perform the load.  */
  SIM_DESC sd = CPU_STATE (current_cpu);
  int daec = 0;
  int rec  = 0;
  int ec   = 0;
  USI necr;
  int do_elos;
  SI NE_flags[2];
  SI NE_base;
  SI nesr;
  SI ne_index;
  FRV_REGISTER_CONTROL *control;

  SI address = GET_H_GR (base_index);
  if (disp_index >= 0)
    address += GET_H_GR (disp_index);
  else
    address += immediate_disp;

  /* Check for interrupt factors.  */
  switch (data_size)
    {
    case NESR_UQI_SIZE:
    case NESR_QI_SIZE:
      break;
    case NESR_UHI_SIZE:
    case NESR_HI_SIZE:
      if (address & 1)
	ec = 1;
      break;
    case NESR_SI_SIZE:
      if (address & 3)
	ec = 1;
      break;
    case NESR_DI_SIZE:
      if (address & 7)
	ec = 1;
      if (target_index & 1)
	rec = 1;
      break;
    case NESR_XI_SIZE:
      if (address & 0xf)
	ec = 1;
      if (target_index & 3)
	rec = 1;
      break;
    default:
      {
	IADDR pc = GET_H_PC ();
	sim_engine_abort (sd, current_cpu, pc, 
			  "check_non_excepting_load: Incorrect data_size\n");
	break;
      }
    }

  control = CPU_REGISTER_CONTROL (current_cpu);
  if (control->spr[H_SPR_NECR].implemented)
    {
      necr = GET_NECR ();
      do_elos = GET_NECR_VALID (necr) && GET_NECR_ELOS (necr);
    }
  else
    do_elos = 0;

  /* NECR, NESR, NEEAR are only implemented for the full frv machine.  */
  if (do_elos)
    {
      ne_index = next_available_nesr (current_cpu, NO_NESR);
      if (ne_index == NO_NESR)
	{
	  IADDR pc = GET_H_PC ();
	  sim_engine_abort (sd, current_cpu, pc, 
			    "No available NESR register\n");
	}

      /* Fill in the basic fields of the NESR.  */
      nesr = GET_NESR (ne_index);
      SET_NESR_VALID (nesr);
      SET_NESR_EAV (nesr);
      SET_NESR_DRN (nesr, target_index);
      SET_NESR_SIZE (nesr, data_size);
      SET_NESR_NEAN (nesr, ne_index);
      if (is_float)
	SET_NESR_FR (nesr);
      else
	CLEAR_NESR_FR (nesr);

      /* Set the corresponding NEEAR.  */
      SET_NEEAR (ne_index, address);
  
      SET_NESR_DAEC (nesr, 0);
      SET_NESR_REC (nesr, 0);
      SET_NESR_EC (nesr, 0);
    }

  /* Set the NE flag corresponding to the target register if an interrupt
     factor was detected. 
     daec is not checked here yet, but is declared for future reference.  */
  if (is_float)
    NE_base = H_SPR_FNER0;
  else
    NE_base = H_SPR_GNER0;

  GET_NE_FLAGS (NE_flags, NE_base);
  if (rec)
    {
      SET_NE_FLAG (NE_flags, target_index);
      if (do_elos)
	SET_NESR_REC (nesr, NESR_REGISTER_NOT_ALIGNED);
    }

  if (ec)
    {
      SET_NE_FLAG (NE_flags, target_index);
      if (do_elos)
	SET_NESR_EC (nesr, NESR_MEM_ADDRESS_NOT_ALIGNED);
    }

  if (do_elos)
    SET_NESR (ne_index, nesr);

  /* If no interrupt factor was detected then set the NE flag on the
     target register if the NE flag on one of the input registers
     is already set.  */
  if (! rec && ! ec && ! daec)
    {
      BI ne_flag = GET_NE_FLAG (NE_flags, base_index);
      if (disp_index >= 0)
	ne_flag |= GET_NE_FLAG (NE_flags, disp_index);
      if (ne_flag)
	{
	  SET_NE_FLAG (NE_flags, target_index);
	  rc = 0; /* Do not perform the load.  */
	}
      else
	CLEAR_NE_FLAG (NE_flags, target_index);
    }

  SET_NE_FLAGS (NE_base, NE_flags);

  return rc; /* perform the load?  */
}

/* Record state for media exception: media_cr_not_aligned.  */
void
frvbf_media_cr_not_aligned (SIM_CPU *current_cpu)
{
  SIM_DESC sd = CPU_STATE (current_cpu);

  /* On some machines this generates an illegal_instruction interrupt.  */
  switch (STATE_ARCHITECTURE (sd)->mach)
    {
      /* Note: there is a discrepancy between V2.2 of the FR400
	 instruction manual and the various FR4xx LSI specs.  The former
	 claims that unaligned registers cause an mp_exception while the
	 latter say it's an illegal_instruction.  The LSI specs appear
	 to be correct since MTT is fixed at 1.  */
    case bfd_mach_fr400:
    case bfd_mach_fr450:
    case bfd_mach_fr550:
      frv_queue_program_interrupt (current_cpu, FRV_ILLEGAL_INSTRUCTION);
      break;
    default:
      frv_set_mp_exception_registers (current_cpu, MTT_CR_NOT_ALIGNED, 0);
      break;
    }
}

/* Record state for media exception: media_acc_not_aligned.  */
void
frvbf_media_acc_not_aligned (SIM_CPU *current_cpu)
{
  SIM_DESC sd = CPU_STATE (current_cpu);

  /* On some machines this generates an illegal_instruction interrupt.  */
  switch (STATE_ARCHITECTURE (sd)->mach)
    {
      /* See comment in frvbf_cr_not_aligned().  */
    case bfd_mach_fr400:
    case bfd_mach_fr450:
    case bfd_mach_fr550:
      frv_queue_program_interrupt (current_cpu, FRV_ILLEGAL_INSTRUCTION);
      break;
    default:
      frv_set_mp_exception_registers (current_cpu, MTT_ACC_NOT_ALIGNED, 0);
      break;
    }
}

/* Record state for media exception: media_register_not_aligned.  */
void
frvbf_media_register_not_aligned (SIM_CPU *current_cpu)
{
  SIM_DESC sd = CPU_STATE (current_cpu);

  /* On some machines this generates an illegal_instruction interrupt.  */
  switch (STATE_ARCHITECTURE (sd)->mach)
    {
      /* See comment in frvbf_cr_not_aligned().  */
    case bfd_mach_fr400:
    case bfd_mach_fr450:
    case bfd_mach_fr550:
      frv_queue_program_interrupt (current_cpu, FRV_ILLEGAL_INSTRUCTION);
      break;
    default:
      frv_set_mp_exception_registers (current_cpu, MTT_INVALID_FR, 0);
      break;
    }
}

/* Record state for media exception: media_overflow.  */
void
frvbf_media_overflow (SIM_CPU *current_cpu, int sie)
{
  frv_set_mp_exception_registers (current_cpu, MTT_OVERFLOW, sie);
}

/* Queue a division exception.  */
enum frv_dtt
frvbf_division_exception (SIM_CPU *current_cpu, enum frv_dtt dtt,
			  int target_index, int non_excepting)
{
  /* If there was an overflow and it is masked, then record it in
     ISR.AEXC.  */
  USI isr = GET_ISR ();
  if ((dtt & FRV_DTT_OVERFLOW) && GET_ISR_EDE (isr))
    {
      dtt &= ~FRV_DTT_OVERFLOW;
      SET_ISR_AEXC (isr);
      SET_ISR (isr);
    }
  if (dtt != FRV_DTT_NO_EXCEPTION)
    {
      if (non_excepting)
	{
	  /* Non excepting instruction, simply set the NE flag for the target
	     register.  */
	  SI NE_flags[2];
	  GET_NE_FLAGS (NE_flags, H_SPR_GNER0);
	  SET_NE_FLAG (NE_flags, target_index);
	  SET_NE_FLAGS (H_SPR_GNER0, NE_flags);
	}
      else
	frv_queue_division_exception_interrupt (current_cpu, dtt);
    }
  return dtt;
}

void
frvbf_check_recovering_store (
  SIM_CPU *current_cpu, PCADDR address, SI regno, int size, int is_float
)
{
  FRV_CACHE *cache = CPU_DATA_CACHE (current_cpu);
  int reg_ix;

  CPU_RSTR_INVALIDATE(current_cpu) = 0;

  for (reg_ix = next_valid_nesr (current_cpu, NO_NESR);
       reg_ix != NO_NESR;
       reg_ix = next_valid_nesr (current_cpu, reg_ix))
    {
      if (address == GET_H_SPR (H_SPR_NEEAR0 + reg_ix))
	{
	  SI nesr = GET_NESR (reg_ix);
	  int nesr_drn = GET_NESR_DRN (nesr);
	  BI nesr_fr = GET_NESR_FR (nesr);
	  SI remain;

	  /* Invalidate cache block containing this address.
	     If we need to count cycles, then the cache operation will be
	     initiated from the model profiling functions.
	     See frvbf_model_....  */
	  if (model_insn)
	    {
	      CPU_RSTR_INVALIDATE(current_cpu) = 1;
	      CPU_LOAD_ADDRESS (current_cpu) = address;
	    }
	  else
	    frv_cache_invalidate (cache, address, 1/* flush */);

	  /* Copy the stored value to the register indicated by NESR.DRN.  */
	  for (remain = size; remain > 0; remain -= 4)
	    {
	      SI value;

	      if (is_float)
		value = GET_H_FR (regno);
	      else
		value = GET_H_GR (regno);

	      switch (size)
		{
		case 1:
		  value &= 0xff;
		  break;
		case 2:
		  value &= 0xffff;
		  break;
		default:
		  break;
		}

	      if (nesr_fr)
		sim_queue_fn_sf_write (current_cpu, frvbf_h_fr_set, nesr_drn,
				       value);
	      else
		sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, nesr_drn,
				       value);

	      nesr_drn++;
	      regno++;
	    }
	  break; /* Only consider the first matching register.  */
	}
    } /* loop over active neear registers.  */
}

SI
frvbf_check_acc_range (SIM_CPU *current_cpu, SI regno)
{
  /* Only applicable to fr550 */
  SIM_DESC sd = CPU_STATE (current_cpu);
  if (STATE_ARCHITECTURE (sd)->mach != bfd_mach_fr550)
    return;

  /* On the fr550, media insns in slots 0 and 2 can only access
     accumulators acc0-acc3. Insns in slots 1 and 3 can only access
     accumulators acc4-acc7 */
  switch (frv_current_fm_slot)
    {
    case UNIT_FM0:
    case UNIT_FM2:
      if (regno <= 3)
	return 1; /* all is ok */
      break;
    case UNIT_FM1:
    case UNIT_FM3:
      if (regno >= 4)
	return 1; /* all is ok */
      break;
    }
  
  /* The specified accumulator is out of range. Queue an illegal_instruction
     interrupt.  */
  frv_queue_program_interrupt (current_cpu, FRV_ILLEGAL_INSTRUCTION);
  return 0;
}

void
frvbf_check_swap_address (SIM_CPU *current_cpu, SI address)
{
  /* Only applicable to fr550 */
  SIM_DESC sd = CPU_STATE (current_cpu);
  if (STATE_ARCHITECTURE (sd)->mach != bfd_mach_fr550)
    return;

  /* Adress must be aligned on a word boundary.  */
  if (address & 0x3)
    frv_queue_data_access_exception_interrupt (current_cpu);
}

static void
clear_nesr_neear (SIM_CPU *current_cpu, SI target_index, BI is_float)
{
  int reg_ix;

  /* Only implemented for full frv.  */
  SIM_DESC sd = CPU_STATE (current_cpu);
  if (STATE_ARCHITECTURE (sd)->mach != bfd_mach_frv)
    return;

  /* Clear the appropriate NESR and NEEAR registers.  */
  for (reg_ix = next_valid_nesr (current_cpu, NO_NESR);
       reg_ix != NO_NESR;
       reg_ix = next_valid_nesr (current_cpu, reg_ix))
    {
      SI nesr;
      /* The register is available, now check if it is active.  */
      nesr = GET_NESR (reg_ix);
      if (GET_NESR_FR (nesr) == is_float)
	{
	  if (target_index < 0 || GET_NESR_DRN (nesr) == target_index)
	    {
	      SET_NESR (reg_ix, 0);
	      SET_NEEAR (reg_ix, 0);
	    }
	}
    }
}

static void
clear_ne_flags (
  SIM_CPU *current_cpu,
  SI target_index,
  int hi_available,
  int lo_available,
  SI NE_base
)
{
  SI NE_flags[2];
  int exception;

  GET_NE_FLAGS (NE_flags, NE_base);
  if (target_index >= 0)
    CLEAR_NE_FLAG (NE_flags, target_index);
  else
    {
      if (lo_available)
	NE_flags[1] = 0;
      if (hi_available)
	NE_flags[0] = 0;
    }
  SET_NE_FLAGS (NE_base, NE_flags);
}

/* Return 1 if the given register is available, 0 otherwise.  TARGET_INDEX==-1
   means to check for any register available.  */
static void
which_registers_available (
  SIM_CPU *current_cpu, int *hi_available, int *lo_available, int is_float
)
{
  if (is_float)
    frv_fr_registers_available (current_cpu, hi_available, lo_available);
  else
    frv_gr_registers_available (current_cpu, hi_available, lo_available);
}

void
frvbf_clear_ne_flags (SIM_CPU *current_cpu, SI target_index, BI is_float)
{
  int hi_available;
  int lo_available;
  int exception;
  SI NE_base;
  USI necr;
  FRV_REGISTER_CONTROL *control;

  /* Check for availability of the target register(s).  */
  which_registers_available (current_cpu, & hi_available, & lo_available,
			     is_float);

  /* Check to make sure that the target register is available.  */
  if (! frv_check_register_access (current_cpu, target_index,
				   hi_available, lo_available))
    return;

  /* Determine whether we're working with GR or FR registers.  */
  if (is_float)
    NE_base = H_SPR_FNER0;
  else
    NE_base = H_SPR_GNER0;

  /* Always clear the appropriate NE flags.  */
  clear_ne_flags (current_cpu, target_index, hi_available, lo_available,
		  NE_base);

  /* Clear the appropriate NESR and NEEAR registers.  */
  control = CPU_REGISTER_CONTROL (current_cpu);
  if (control->spr[H_SPR_NECR].implemented)
    {
      necr = GET_NECR ();
      if (GET_NECR_VALID (necr) && GET_NECR_ELOS (necr))
	clear_nesr_neear (current_cpu, target_index, is_float);
    }
}

void
frvbf_commit (SIM_CPU *current_cpu, SI target_index, BI is_float)
{
  SI NE_base;
  SI NE_flags[2];
  BI NE_flag;
  int exception;
  int hi_available;
  int lo_available;
  USI necr;
  FRV_REGISTER_CONTROL *control;

  /* Check for availability of the target register(s).  */
  which_registers_available (current_cpu, & hi_available, & lo_available,
			     is_float);

  /* Check to make sure that the target register is available.  */
  if (! frv_check_register_access (current_cpu, target_index,
				   hi_available, lo_available))
    return;

  /* Determine whether we're working with GR or FR registers.  */
  if (is_float)
    NE_base = H_SPR_FNER0;
  else
    NE_base = H_SPR_GNER0;

  /* Determine whether a ne exception is pending.  */
  GET_NE_FLAGS (NE_flags, NE_base);
  if (target_index >= 0)
    NE_flag = GET_NE_FLAG (NE_flags, target_index);
  else
    {
      NE_flag =
	hi_available && NE_flags[0] != 0 || lo_available && NE_flags[1] != 0;
    }

  /* Always clear the appropriate NE flags.  */
  clear_ne_flags (current_cpu, target_index, hi_available, lo_available,
		  NE_base);

  control = CPU_REGISTER_CONTROL (current_cpu);
  if (control->spr[H_SPR_NECR].implemented)
    {
      necr = GET_NECR ();
      if (GET_NECR_VALID (necr) && GET_NECR_ELOS (necr) && NE_flag)
	{
	  /* Clear the appropriate NESR and NEEAR registers.  */
	  clear_nesr_neear (current_cpu, target_index, is_float);
	  frv_queue_program_interrupt (current_cpu, FRV_COMMIT_EXCEPTION);
	}
    }
}

/* Generate the appropriate fp_exception(s) based on the given status code.  */
void
frvbf_fpu_error (CGEN_FPU* fpu, int status)
{
  struct frv_fp_exception_info fp_info = {
    FSR_NO_EXCEPTION, FTT_IEEE_754_EXCEPTION
  };

  if (status &
      (sim_fpu_status_invalid_snan |
       sim_fpu_status_invalid_qnan |
       sim_fpu_status_invalid_isi |
       sim_fpu_status_invalid_idi |
       sim_fpu_status_invalid_zdz |
       sim_fpu_status_invalid_imz |
       sim_fpu_status_invalid_cvi |
       sim_fpu_status_invalid_cmp |
       sim_fpu_status_invalid_sqrt))
    fp_info.fsr_mask |= FSR_INVALID_OPERATION;

  if (status & sim_fpu_status_invalid_div0)
    fp_info.fsr_mask |= FSR_DIVISION_BY_ZERO;

  if (status & sim_fpu_status_inexact)
    fp_info.fsr_mask |= FSR_INEXACT;

  if (status & sim_fpu_status_overflow)
    fp_info.fsr_mask |= FSR_OVERFLOW;

  if (status & sim_fpu_status_underflow)
    fp_info.fsr_mask |= FSR_UNDERFLOW;

  if (status & sim_fpu_status_denorm)
    {
      fp_info.fsr_mask |= FSR_DENORMAL_INPUT;
      fp_info.ftt = FTT_DENORMAL_INPUT;
    }

  if (fp_info.fsr_mask != FSR_NO_EXCEPTION)
    {
      SIM_CPU *current_cpu = (SIM_CPU *)fpu->owner;
      frv_queue_fp_exception_interrupt (current_cpu, & fp_info);
    }
}
