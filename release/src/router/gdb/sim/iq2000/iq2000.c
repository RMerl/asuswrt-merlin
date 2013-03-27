/* IQ2000 simulator support code
   Copyright (C) 2000, 2004, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

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

#define WANT_CPU
#define WANT_CPU_IQ2000BF

#include "sim-main.h"
#include "cgen-mem.h"
#include "cgen-ops.h"

enum
{
  GPR0_REGNUM = 0,
  NR_GPR = 32,
  PC_REGNUM = 32
};

enum libgloss_syscall
{
  SYS_exit = 1,
  SYS_open = 2, 
  SYS_close = 3, 
  SYS_read = 4,
  SYS_write = 5, 
  SYS_lseek = 6, 
  SYS_unlink = 7,
  SYS_getpid = 8,
  SYS_kill = 9,
  SYS_fstat = 10, 
  SYS_argvlen = 12, 
  SYS_argv = 13,
  SYS_chdir = 14, 
  SYS_stat = 15, 
  SYS_chmod = 16, 
  SYS_utime = 17,
  SYS_time = 18,
  SYS_gettimeofday = 19,
  SYS_times = 20
};

/* Read a null terminated string from memory, return in a buffer */
static char *
fetch_str (current_cpu, pc, addr)
     SIM_CPU *current_cpu;
     PCADDR pc;
     DI addr;
{
  char *buf;
  int nr = 0;
  while (sim_core_read_1 (current_cpu,
                          pc, read_map, CPU2DATA(addr + nr)) != 0)
    nr++;
  buf = NZALLOC (char, nr + 1);
  sim_read (CPU_STATE (current_cpu), CPU2DATA(addr), buf, nr);
  return buf;
}

void
do_syscall (SIM_CPU *current_cpu, PCADDR pc)
{
#if 0
  int syscall = H2T_4 (iq2000bf_h_gr_get (current_cpu, 11));
#endif
  int syscall_function = iq2000bf_h_gr_get (current_cpu, 4);
  int i;
  char *buf;
  int PARM1 = iq2000bf_h_gr_get (current_cpu, 5);
  int PARM2 = iq2000bf_h_gr_get (current_cpu, 6);
  int PARM3 = iq2000bf_h_gr_get (current_cpu, 7);
  const int ret_reg = 2;
	
  switch (syscall_function)
    {
    case 0:
      switch (H2T_4 (iq2000bf_h_gr_get (current_cpu, 11)))
	{
	case 0:
	  /* Pass.  */
	  puts ("pass");
	  exit (0);
	case 1:
	  /* Fail.  */
	  puts ("fail");
	  exit (1);
	}

    case SYS_write:
      buf = zalloc (PARM3);
      sim_read (CPU_STATE (current_cpu), CPU2DATA(PARM2), buf, PARM3);
      SET_H_GR (ret_reg,
		sim_io_write (CPU_STATE (current_cpu),
			      PARM1, buf, PARM3));
      zfree (buf);
      break;

    case SYS_lseek:
      SET_H_GR (ret_reg,
		sim_io_lseek (CPU_STATE (current_cpu),
			      PARM1, PARM2, PARM3));
      break;
	    
    case SYS_exit:
      sim_engine_halt (CPU_STATE (current_cpu), current_cpu,
		       NULL, pc, sim_exited, PARM1);
      break;

    case SYS_read:
      buf = zalloc (PARM3);
      SET_H_GR (ret_reg,
		sim_io_read (CPU_STATE (current_cpu),
			     PARM1, buf, PARM3));
      sim_write (CPU_STATE (current_cpu), CPU2DATA(PARM2), buf, PARM3);
      zfree (buf);
      break;
	    
    case SYS_open:
      buf = fetch_str (current_cpu, pc, PARM1);
      SET_H_GR (ret_reg,
		sim_io_open (CPU_STATE (current_cpu),
			     buf, PARM2));
      zfree (buf);
      break;

    case SYS_close:
      SET_H_GR (ret_reg,
		sim_io_close (CPU_STATE (current_cpu), PARM1));
      break;

    case SYS_time:
      SET_H_GR (ret_reg, time (0));
      break;

    default:
      SET_H_GR (ret_reg, -1);
    }
}

void 
do_break (SIM_CPU *current_cpu, PCADDR pc)
{
  SIM_DESC sd = CPU_STATE (current_cpu);
  sim_engine_halt (sd, current_cpu, NULL, pc, sim_stopped, SIM_SIGTRAP);
}

/* The semantic code invokes this for invalid (unrecognized) instructions.  */

SEM_PC
sim_engine_invalid_insn (SIM_CPU *current_cpu, IADDR cia, SEM_PC vpc)
{
  SIM_DESC sd = CPU_STATE (current_cpu);
  sim_engine_halt (sd, current_cpu, NULL, cia, sim_stopped, SIM_SIGILL);

  return vpc;
}


/* Process an address exception.  */

void
iq2000_core_signal (SIM_DESC sd, SIM_CPU *current_cpu, sim_cia cia,
                  unsigned int map, int nr_bytes, address_word addr,
                  transfer_type transfer, sim_core_signals sig)
{
  sim_core_signal (sd, current_cpu, cia, map, nr_bytes, addr,
		   transfer, sig);
}


/* Initialize cycle counting for an insn.
   FIRST_P is non-zero if this is the first insn in a set of parallel
   insns.  */

void
iq2000bf_model_insn_before (SIM_CPU *cpu, int first_p)
{
  /* Do nothing.  */
}


/* Record the cycles computed for an insn.
   LAST_P is non-zero if this is the last insn in a set of parallel insns,
   and we update the total cycle count.
   CYCLES is the cycle count of the insn.  */

void
iq2000bf_model_insn_after(SIM_CPU *cpu, int last_p, int cycles)
{
  /* Do nothing.  */
}


int
iq2000bf_model_iq2000_u_exec (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced)
{
  return idesc->timing->units[unit_num].done;
}

PCADDR
get_h_pc (SIM_CPU *cpu)
{
  return CPU_CGEN_HW(cpu)->h_pc;
}

void
set_h_pc (SIM_CPU *cpu, PCADDR addr)
{
  CPU_CGEN_HW(cpu)->h_pc = addr | IQ2000_INSN_MASK;
}

int
iq2000bf_fetch_register (SIM_CPU *cpu, int nr, unsigned char *buf, int len)
{
  if (nr >= GPR0_REGNUM
      && nr < (GPR0_REGNUM + NR_GPR)
      && len == 4)
    {
      *((unsigned32*)buf) =
	H2T_4 (iq2000bf_h_gr_get (cpu, nr - GPR0_REGNUM));
      return 4;
    }
  else if (nr == PC_REGNUM
	   && len == 4)
    {
      *((unsigned32*)buf) = H2T_4 (get_h_pc (cpu));
      return 4;
    }
  else
    return 0;
}

int
iq2000bf_store_register (SIM_CPU *cpu, int nr, unsigned char *buf, int len)
{
  if (nr >= GPR0_REGNUM
      && nr < (GPR0_REGNUM + NR_GPR)
      && len == 4)
    {
      iq2000bf_h_gr_set (cpu, nr - GPR0_REGNUM, T2H_4 (*((unsigned32*)buf)));
      return 4;
    }
  else if (nr == PC_REGNUM
	   && len == 4)
    {
      set_h_pc (cpu, T2H_4 (*((unsigned32*)buf)));
      return 4;
    }
  else
    return 0;
}
