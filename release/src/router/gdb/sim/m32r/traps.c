/* m32r exception, interrupt, and trap (EIT) support
   Copyright (C) 1998, 2003, 2007 Free Software Foundation, Inc.
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

#include "sim-main.h"
#include "targ-vals.h"

#define TRAP_FLUSH_CACHE 12
/* The semantic code invokes this for invalid (unrecognized) instructions.  */

SEM_PC
sim_engine_invalid_insn (SIM_CPU *current_cpu, IADDR cia, SEM_PC pc)
{
  SIM_DESC sd = CPU_STATE (current_cpu);

#if 0
  if (STATE_ENVIRONMENT (sd) == OPERATING_ENVIRONMENT)
    {
      h_bsm_set (current_cpu, h_sm_get (current_cpu));
      h_bie_set (current_cpu, h_ie_get (current_cpu));
      h_bcond_set (current_cpu, h_cond_get (current_cpu));
      /* sm not changed */
      h_ie_set (current_cpu, 0);
      h_cond_set (current_cpu, 0);

      h_bpc_set (current_cpu, cia);

      sim_engine_restart (CPU_STATE (current_cpu), current_cpu, NULL,
			  EIT_RSVD_INSN_ADDR);
    }
  else
#endif
    sim_engine_halt (sd, current_cpu, NULL, cia, sim_stopped, SIM_SIGILL);

  return pc;
}

/* Process an address exception.  */

void
m32r_core_signal (SIM_DESC sd, SIM_CPU *current_cpu, sim_cia cia,
		  unsigned int map, int nr_bytes, address_word addr,
		  transfer_type transfer, sim_core_signals sig)
{
  if (STATE_ENVIRONMENT (sd) == OPERATING_ENVIRONMENT)
    {
      m32rbf_h_cr_set (current_cpu, H_CR_BBPC,
		       m32rbf_h_cr_get (current_cpu, H_CR_BPC));
      switch (MACH_NUM (CPU_MACH (current_cpu)))
	{
	case MACH_M32R:
	  m32rbf_h_bpsw_set (current_cpu, m32rbf_h_psw_get (current_cpu));
	  /* sm not changed.  */
	  m32rbf_h_psw_set (current_cpu, m32rbf_h_psw_get (current_cpu) & 0x80);
	  break;
	case MACH_M32RX:
  	  m32rxf_h_bpsw_set (current_cpu, m32rxf_h_psw_get (current_cpu));
  	  /* sm not changed.  */
  	  m32rxf_h_psw_set (current_cpu, m32rxf_h_psw_get (current_cpu) & 0x80);
	  break;
	case MACH_M32R2:
	  m32r2f_h_bpsw_set (current_cpu, m32r2f_h_psw_get (current_cpu));
	  /* sm not changed.  */
	  m32r2f_h_psw_set (current_cpu, m32r2f_h_psw_get (current_cpu) & 0x80);
	  break;
	default:
	  abort ();
	}
	    
      m32rbf_h_cr_set (current_cpu, H_CR_BPC, cia);

      sim_engine_restart (CPU_STATE (current_cpu), current_cpu, NULL,
			  EIT_ADDR_EXCP_ADDR);
    }
  else
    sim_core_signal (sd, current_cpu, cia, map, nr_bytes, addr,
		     transfer, sig);
}

/* Read/write functions for system call interface.  */

static int
syscall_read_mem (host_callback *cb, struct cb_syscall *sc,
		  unsigned long taddr, char *buf, int bytes)
{
  SIM_DESC sd = (SIM_DESC) sc->p1;
  SIM_CPU *cpu = (SIM_CPU *) sc->p2;

  return sim_core_read_buffer (sd, cpu, read_map, buf, taddr, bytes);
}

static int
syscall_write_mem (host_callback *cb, struct cb_syscall *sc,
		   unsigned long taddr, const char *buf, int bytes)
{
  SIM_DESC sd = (SIM_DESC) sc->p1;
  SIM_CPU *cpu = (SIM_CPU *) sc->p2;

  return sim_core_write_buffer (sd, cpu, write_map, buf, taddr, bytes);
}

/* Trap support.
   The result is the pc address to continue at.
   Preprocessing like saving the various registers has already been done.  */

USI
m32r_trap (SIM_CPU *current_cpu, PCADDR pc, int num)
{
  SIM_DESC sd = CPU_STATE (current_cpu);
  host_callback *cb = STATE_CALLBACK (sd);

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
      /* The new pc is the trap vector entry.
	 We assume there's a branch there to some handler.
         Use cr5 as EVB (EIT Vector Base) register.  */
      /* USI new_pc = EIT_TRAP_BASE_ADDR + num * 4; */
      USI new_pc = m32rbf_h_cr_get (current_cpu, 5) + 0x40 + num * 4;
      return new_pc;
    }

  switch (num)
    {
    case TRAP_SYSCALL :
      {
	CB_SYSCALL s;

	CB_SYSCALL_INIT (&s);
	s.func = m32rbf_h_gr_get (current_cpu, 0);
	s.arg1 = m32rbf_h_gr_get (current_cpu, 1);
	s.arg2 = m32rbf_h_gr_get (current_cpu, 2);
	s.arg3 = m32rbf_h_gr_get (current_cpu, 3);

	if (s.func == TARGET_SYS_exit)
	  {
	    sim_engine_halt (sd, current_cpu, NULL, pc, sim_exited, s.arg1);
	  }

	s.p1 = (PTR) sd;
	s.p2 = (PTR) current_cpu;
	s.read_mem = syscall_read_mem;
	s.write_mem = syscall_write_mem;
	cb_syscall (cb, &s);
	m32rbf_h_gr_set (current_cpu, 2, s.errcode);
	m32rbf_h_gr_set (current_cpu, 0, s.result);
	m32rbf_h_gr_set (current_cpu, 1, s.result2);
	break;
      }

    case TRAP_BREAKPOINT:
      sim_engine_halt (sd, current_cpu, NULL, pc,
		       sim_stopped, SIM_SIGTRAP);
      break;

    case TRAP_FLUSH_CACHE:
      /* Do nothing.  */
      break;

    default :
      {
	/* USI new_pc = EIT_TRAP_BASE_ADDR + num * 4; */
        /* Use cr5 as EVB (EIT Vector Base) register.  */
        USI new_pc = m32rbf_h_cr_get (current_cpu, 5) + 0x40 + num * 4;
	return new_pc;
      }
    }

  /* Fake an "rte" insn.  */
  /* FIXME: Should duplicate all of rte processing.  */
  return (pc & -4) + 4;
}
