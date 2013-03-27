/* SH5 simulator support code
   Copyright (C) 2000, 2001, 2006 Free Software Foundation, Inc.
   Contributed by Red Hat, Inc.

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
#define WANT_CPU_SH64

#include "sim-main.h"
#include "sim-fpu.h"
#include "cgen-mem.h"
#include "cgen-ops.h"

#include "gdb/callback.h"
#include "defs-compact.h"

#include "bfd.h"
/* From include/gdb/.  */
#include "gdb/sim-sh.h"

#define SYS_exit        1
#define	SYS_read	3
#define SYS_write       4
#define	SYS_open	5
#define	SYS_close	6
#define	SYS_lseek	19
#define	SYS_time	23
#define	SYS_argc	172
#define	SYS_argnlen	173
#define	SYS_argn	174

IDESC * sh64_idesc_media;
IDESC * sh64_idesc_compact;

BI
sh64_endian (SIM_CPU *current_cpu)
{
  return (CURRENT_TARGET_BYTE_ORDER == BIG_ENDIAN);
}

SF
sh64_fldi0 (SIM_CPU *current_cpu)
{
  SF result;
  sim_fpu_to32 (&result, &sim_fpu_zero);
  return result;
}

SF
sh64_fldi1 (SIM_CPU *current_cpu)
{
  SF result;
  sim_fpu_to32 (&result, &sim_fpu_one);
  return result;
}

DF
sh64_fabsd(SIM_CPU *current_cpu, DF drgh)
{
  DF result;
  sim_fpu f, fres;

  sim_fpu_64to (&f, drgh);
  sim_fpu_abs (&fres, &f);
  sim_fpu_to64 (&result, &fres);
  return result;
}

SF
sh64_fabss(SIM_CPU *current_cpu, SF frgh)
{
  SF result;
  sim_fpu f, fres;

  sim_fpu_32to (&f, frgh);
  sim_fpu_abs (&fres, &f);
  sim_fpu_to32 (&result, &fres);
  return result;
}

DF
sh64_faddd(SIM_CPU *current_cpu, DF drg, DF drh)
{
  DF result;
  sim_fpu f1, f2, fres;

  sim_fpu_64to (&f1, drg);
  sim_fpu_64to (&f2, drh);
  sim_fpu_add (&fres, &f1, &f2);
  sim_fpu_to64 (&result, &fres);
  return result;
}

SF
sh64_fadds(SIM_CPU *current_cpu, SF frg, SF frh)
{
  SF result;
  sim_fpu f1, f2, fres;

  sim_fpu_32to (&f1, frg);
  sim_fpu_32to (&f2, frh);
  sim_fpu_add (&fres, &f1, &f2);
  sim_fpu_to32 (&result, &fres);
  return result;
}

BI
sh64_fcmpeqd(SIM_CPU *current_cpu, DF drg, DF drh)
{
  sim_fpu f1, f2;

  sim_fpu_64to (&f1, drg);
  sim_fpu_64to (&f2, drh);
  return sim_fpu_is_eq (&f1, &f2);
}

BI
sh64_fcmpeqs(SIM_CPU *current_cpu, SF frg, SF frh)
{
  sim_fpu f1, f2;

  sim_fpu_32to (&f1, frg);
  sim_fpu_32to (&f2, frh);
  return sim_fpu_is_eq (&f1, &f2);
}

BI
sh64_fcmpged(SIM_CPU *current_cpu, DF drg, DF drh)
{
  sim_fpu f1, f2;

  sim_fpu_64to (&f1, drg);
  sim_fpu_64to (&f2, drh);
  return sim_fpu_is_ge (&f1, &f2);
}

BI
sh64_fcmpges(SIM_CPU *current_cpu, SF frg, SF frh)
{
  sim_fpu f1, f2;

  sim_fpu_32to (&f1, frg);
  sim_fpu_32to (&f2, frh);
  return sim_fpu_is_ge (&f1, &f2);
}

BI
sh64_fcmpgtd(SIM_CPU *current_cpu, DF drg, DF drh)
{
  sim_fpu f1, f2;

  sim_fpu_64to (&f1, drg);
  sim_fpu_64to (&f2, drh);
  return sim_fpu_is_gt (&f1, &f2);
}

BI
sh64_fcmpgts(SIM_CPU *current_cpu, SF frg, SF frh)
{
  sim_fpu f1, f2;

  sim_fpu_32to (&f1, frg);
  sim_fpu_32to (&f2, frh);
  return sim_fpu_is_gt (&f1, &f2);
}

BI
sh64_fcmpund(SIM_CPU *current_cpu, DF drg, DF drh)
{
  sim_fpu f1, f2;

  sim_fpu_64to (&f1, drg);
  sim_fpu_64to (&f2, drh);
  return (sim_fpu_is_nan (&f1) || sim_fpu_is_nan (&f2));
}

BI
sh64_fcmpuns(SIM_CPU *current_cpu, SF frg, SF frh)
{
  sim_fpu f1, f2;

  sim_fpu_32to (&f1, frg);
  sim_fpu_32to (&f2, frh); 
  return (sim_fpu_is_nan (&f1) || sim_fpu_is_nan (&f2));
}  

SF
sh64_fcnvds(SIM_CPU *current_cpu, DF drgh)
{
  union {
    unsigned long long ll;
    double d;
  } f1;

  union {
    unsigned long l;
    float f;
  } f2;

  f1.ll = drgh;
  f2.f = (float) f1.d;
  
  return (SF) f2.l;
}

DF
sh64_fcnvsd(SIM_CPU *current_cpu, SF frgh)
{
  DF result;
  sim_fpu f;

  sim_fpu_32to (&f, frgh);
  sim_fpu_to64 (&result, &f);
  return result;
}

DF
sh64_fdivd(SIM_CPU *current_cpu, DF drg, DF drh)
{
  DF result;
  sim_fpu f1, f2, fres;

  sim_fpu_64to (&f1, drg);
  sim_fpu_64to (&f2, drh);
  sim_fpu_div (&fres, &f1, &f2);
  sim_fpu_to64 (&result, &fres);
  return result;
}

SF
sh64_fdivs(SIM_CPU *current_cpu, SF frg, SF frh)
{
  SF result;
  sim_fpu f1, f2, fres;

  sim_fpu_32to (&f1, frg);
  sim_fpu_32to (&f2, frh);
  sim_fpu_div (&fres, &f1, &f2);
  sim_fpu_to32 (&result, &fres);
  return result;
}

DF
sh64_floatld(SIM_CPU *current_cpu, SF frgh)
{
  DF result;
  sim_fpu f;

  sim_fpu_i32to (&f, frgh, sim_fpu_round_default); 
  sim_fpu_to64 (&result, &f);
  return result;
}

SF
sh64_floatls(SIM_CPU *current_cpu, SF frgh)
{
  SF result;
  sim_fpu f;

  sim_fpu_i32to (&f, frgh, sim_fpu_round_default);
  sim_fpu_to32 (&result, &f);
  return result;
}

DF
sh64_floatqd(SIM_CPU *current_cpu, DF drgh)
{
  DF result;
  sim_fpu f;

  sim_fpu_i64to (&f, drgh, sim_fpu_round_default);
  sim_fpu_to64 (&result, &f);
  return result;
}

SF
sh64_floatqs(SIM_CPU *current_cpu, DF drgh)
{
  SF result;
  sim_fpu f;

  sim_fpu_i64to (&f, drgh, sim_fpu_round_default);
  sim_fpu_to32 (&result, &f);
  return result;
}

SF
sh64_fmacs(SIM_CPU *current_cpu, SF fr0, SF frm, SF frn)
{
  SF result;
  sim_fpu m1, m2, a1, fres;

  sim_fpu_32to (&m1, fr0);
  sim_fpu_32to (&m2, frm);
  sim_fpu_32to (&a1, frn);

  sim_fpu_mul (&fres, &m1, &m2);
  sim_fpu_add (&fres, &fres, &a1);
  
  sim_fpu_to32 (&result, &fres);
  return result;
}

DF
sh64_fmuld(SIM_CPU *current_cpu, DF drg, DF drh)
{
  DF result;
  sim_fpu f1, f2, fres;

  sim_fpu_64to (&f1, drg);
  sim_fpu_64to (&f2, drh);
  sim_fpu_mul (&fres, &f1, &f2);
  sim_fpu_to64 (&result, &fres);
  return result;
}

SF
sh64_fmuls(SIM_CPU *current_cpu, SF frg, SF frh)
{
  SF result;
  sim_fpu f1, f2, fres;

  sim_fpu_32to (&f1, frg);
  sim_fpu_32to (&f2, frh);
  sim_fpu_mul (&fres, &f1, &f2);
  sim_fpu_to32 (&result, &fres);
  return result;
}

DF
sh64_fnegd(SIM_CPU *current_cpu, DF drgh)
{
  DF result;
  sim_fpu f1, f2;

  sim_fpu_64to (&f1, drgh);
  sim_fpu_neg (&f2, &f1);
  sim_fpu_to64 (&result, &f2);
  return result;
}

SF
sh64_fnegs(SIM_CPU *current_cpu, SF frgh)
{
  SF result;
  sim_fpu f, fres;

  sim_fpu_32to (&f, frgh);
  sim_fpu_neg (&fres, &f);
  sim_fpu_to32 (&result, &fres);
  return result;
}

DF
sh64_fsqrtd(SIM_CPU *current_cpu, DF drgh)
{
  DF result;
  sim_fpu f, fres;

  sim_fpu_64to (&f, drgh);
  sim_fpu_sqrt (&fres, &f);
  sim_fpu_to64 (&result, &fres);
  return result;
}

SF
sh64_fsqrts(SIM_CPU *current_cpu, SF frgh)
{
  SF result;
  sim_fpu f, fres;

  sim_fpu_32to (&f, frgh);
  sim_fpu_sqrt (&fres, &f);
  sim_fpu_to32 (&result, &fres);
  return result;
}

DF
sh64_fsubd(SIM_CPU *current_cpu, DF drg, DF drh)
{
  DF result;
  sim_fpu f1, f2, fres;

  sim_fpu_64to (&f1, drg);
  sim_fpu_64to (&f2, drh);
  sim_fpu_sub (&fres, &f1, &f2);
  sim_fpu_to64 (&result, &fres);
  return result;
}

SF
sh64_fsubs(SIM_CPU *current_cpu, SF frg, SF frh)
{
  SF result;
  sim_fpu f1, f2, fres;

  sim_fpu_32to (&f1, frg);
  sim_fpu_32to (&f2, frh);
  sim_fpu_sub (&fres, &f1, &f2);
  sim_fpu_to32 (&result, &fres);
  return result;
}

SF
sh64_ftrcdl(SIM_CPU *current_cpu, DF drgh)
{
  SI result;
  sim_fpu f;

  sim_fpu_64to (&f, drgh);
  sim_fpu_to32i (&result, &f, sim_fpu_round_zero);
  return (SF) result;
}

SF
sh64_ftrcsl(SIM_CPU *current_cpu, SF frgh)
{
  SI result;
  sim_fpu f;

  sim_fpu_32to (&f, frgh);
  sim_fpu_to32i (&result, &f, sim_fpu_round_zero);
  return (SF) result;
}

DF
sh64_ftrcdq(SIM_CPU *current_cpu, DF drgh)
{
  DI result;
  sim_fpu f;

  sim_fpu_64to (&f, drgh);
  sim_fpu_to64i (&result, &f, sim_fpu_round_zero);
  return (DF) result;
}

DF
sh64_ftrcsq(SIM_CPU *current_cpu, SF frgh)
{
  DI result;
  sim_fpu f;

  sim_fpu_32to (&f, frgh);
  sim_fpu_to64i (&result, &f, sim_fpu_round_zero);
  return (DF) result;
}

VOID
sh64_ftrvs(SIM_CPU *cpu, unsigned g, unsigned h, unsigned f)
{
  int i, j;

  for (i = 0; i < 4; i++)
    {
      SF result;
      sim_fpu sum;
      sim_fpu_32to (&sum, 0);

      for (j = 0; j < 4; j++)
	{
	  sim_fpu f1, f2, temp;
	  sim_fpu_32to (&f1, sh64_h_fr_get (cpu, (g + i) + (j * 4)));
	  sim_fpu_32to (&f2, sh64_h_fr_get (cpu, h + j));
	  sim_fpu_mul (&temp, &f1, &f2);
	  sim_fpu_add (&sum, &sum, &temp);
	}
      sim_fpu_to32 (&result, &sum);
      sh64_h_fr_set (cpu, f + i, result);
    }
}

VOID
sh64_fipr (SIM_CPU *cpu, unsigned m, unsigned n)
{
  SF result = sh64_fmuls (cpu, sh64_h_fvc_get (cpu, m), sh64_h_fvc_get (cpu, n));
  result = sh64_fadds (cpu, result, sh64_fmuls (cpu, sh64_h_frc_get (cpu, m + 1), sh64_h_frc_get (cpu, n + 1)));
  result = sh64_fadds (cpu, result, sh64_fmuls (cpu, sh64_h_frc_get (cpu, m + 2), sh64_h_frc_get (cpu, n + 2)));
  result = sh64_fadds (cpu, result, sh64_fmuls (cpu, sh64_h_frc_get (cpu, m + 3), sh64_h_frc_get (cpu, n + 3)));
  sh64_h_frc_set (cpu, n + 3, result);
}

SF
sh64_fiprs (SIM_CPU *cpu, unsigned g, unsigned h)
{
  SF temp = sh64_fmuls (cpu, sh64_h_fr_get (cpu, g), sh64_h_fr_get (cpu, h));
  temp = sh64_fadds (cpu, temp, sh64_fmuls (cpu, sh64_h_fr_get (cpu, g + 1), sh64_h_fr_get (cpu, h + 1)));
  temp = sh64_fadds (cpu, temp, sh64_fmuls (cpu, sh64_h_fr_get (cpu, g + 2), sh64_h_fr_get (cpu, h + 2)));
  temp = sh64_fadds (cpu, temp, sh64_fmuls (cpu, sh64_h_fr_get (cpu, g + 3), sh64_h_fr_get (cpu, h + 3)));
  return temp;
}

VOID
sh64_fldp (SIM_CPU *cpu, PCADDR pc, DI rm, DI rn, unsigned f)
{
  sh64_h_fr_set (cpu, f,     GETMEMSF (cpu, pc, rm + rn));
  sh64_h_fr_set (cpu, f + 1, GETMEMSF (cpu, pc, rm + rn + 4));
}

VOID
sh64_fstp (SIM_CPU *cpu, PCADDR pc, DI rm, DI rn, unsigned f)
{
  SETMEMSF (cpu, pc, rm + rn,     sh64_h_fr_get (cpu, f));
  SETMEMSF (cpu, pc, rm + rn + 4, sh64_h_fr_get (cpu, f + 1));
}

VOID
sh64_ftrv (SIM_CPU *cpu, UINT ignored)
{
  /* TODO: Unimplemented.  */
}

VOID
sh64_pref (SIM_CPU *cpu, SI addr)
{
  /* TODO: Unimplemented.  */
}

/* Count the number of arguments.  */
static int
count_argc (cpu)
     SIM_CPU *cpu;
{
  int i = 0;

  if (! STATE_PROG_ARGV (CPU_STATE (cpu)))
    return -1;
  
  while (STATE_PROG_ARGV (CPU_STATE (cpu)) [i] != NULL)
    ++i;

  return i;
}

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
			  pc, read_map, addr + nr) != 0)
    nr++;
  buf = NZALLOC (char, nr + 1);
  sim_read (CPU_STATE (current_cpu), addr, buf, nr);
  return buf;
}

static void
trap_handler (SIM_CPU *current_cpu, int shmedia_abi_p, UQI trapnum, PCADDR pc)
{
  char ch;
  switch (trapnum)
    {
    case 1:
      ch = GET_H_GRC (0);
      sim_io_write_stdout (CPU_STATE (current_cpu), &ch, 1);
      fflush (stdout);
      break;
    case 2:
      sim_engine_halt (CPU_STATE (current_cpu), current_cpu, NULL, pc, sim_stopped, SIM_SIGTRAP);
      break;
    case 34:
      {
	int i;
	int ret_reg = (shmedia_abi_p) ? 2 : 0;
	char *buf;
	DI PARM1 = GET_H_GR ((shmedia_abi_p) ? 3 : 5);
	DI PARM2 = GET_H_GR ((shmedia_abi_p) ? 4 : 6);
	DI PARM3 = GET_H_GR ((shmedia_abi_p) ? 5 : 7);
	
	switch (GET_H_GR ((shmedia_abi_p) ? 2 : 4))
	  {
	  case SYS_write:
	    buf = zalloc (PARM3);
	    sim_read (CPU_STATE (current_cpu), PARM2, buf, PARM3);
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
	    sim_write (CPU_STATE (current_cpu), PARM2, buf, PARM3);
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

	  case SYS_argc:
	    SET_H_GR (ret_reg, count_argc (current_cpu));
	    break;

	  case SYS_argnlen:
	    if (PARM1 < count_argc (current_cpu))
	      SET_H_GR (ret_reg,
			strlen (STATE_PROG_ARGV (CPU_STATE (current_cpu)) [PARM1]));
	    else
	      SET_H_GR (ret_reg, -1);
	    break;

	  case SYS_argn:
	    if (PARM1 < count_argc (current_cpu))
	      {
		/* Include the NULL byte.  */
		i = strlen (STATE_PROG_ARGV (CPU_STATE (current_cpu)) [PARM1]) + 1;
		sim_write (CPU_STATE (current_cpu),
			   PARM2,
			   STATE_PROG_ARGV (CPU_STATE (current_cpu)) [PARM1],
			   i);

		/* Just for good measure.  */
		SET_H_GR (ret_reg, i);
		break;
	      }
	    else
	      SET_H_GR (ret_reg, -1);
	    break;

	  default:
	    SET_H_GR (ret_reg, -1);
	  }
      }
      break;
    case 253:
      puts ("pass");
      exit (0);
    case 254:
      puts ("fail");
      exit (1);
    case 0xc3:
      /* fall through.  */
    case 255:
      sim_engine_halt (CPU_STATE (current_cpu), current_cpu, NULL, pc, sim_stopped, SIM_SIGTRAP);
      break;
    }
}

void
sh64_trapa (SIM_CPU *current_cpu, DI rm, PCADDR pc)
{
  trap_handler (current_cpu, 1, (UQI) rm & 0xff, pc);
}

void
sh64_compact_trapa (SIM_CPU *current_cpu, UQI trapnum, PCADDR pc)
{
  int mach_sh5_p;

  /* If this is an SH5 executable, this is SHcompact code running in
     the SHmedia ABI.  */

  mach_sh5_p =
    (bfd_get_mach (STATE_PROG_BFD (CPU_STATE (current_cpu))) == bfd_mach_sh5);

  trap_handler (current_cpu, mach_sh5_p, trapnum, pc);
}

DI
sh64_nsb (SIM_CPU *current_cpu, DI rm)
{
  int result = 0, count;
  UDI source = (UDI) rm;

  if ((source >> 63))
    source = ~source;
  source <<= 1;

  for (count = 32; count; count >>= 1)
    {
      UDI newval = source << count;

      if ((newval >> count) == source)
	{
	  result |= count;
	  source = newval;
	}
    }
  
  return result;
}

void
sh64_break (SIM_CPU *current_cpu, PCADDR pc)
{
  SIM_DESC sd = CPU_STATE (current_cpu);
  sim_engine_halt (sd, current_cpu, NULL, pc, sim_stopped, SIM_SIGTRAP);
}

SI
sh64_movua (SIM_CPU *current_cpu, PCADDR pc, SI rn)
{
  SI v;
  int i;

  /* Move the data one byte at a time to avoid alignment problems.
     Be aware of endianness.  */
  v = 0;
  for (i = 0; i < 4; ++i)
    v = (v << 8) | (GETMEMQI (current_cpu, pc, rn + i) & 0xff);

  v = T2H_4 (v);
  return v;
}

void
set_isa (SIM_CPU *current_cpu, int mode)
{
  /* Do nothing.  */
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
sh64_core_signal (SIM_DESC sd, SIM_CPU *current_cpu, sim_cia cia,
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
sh64_compact_model_insn_before (SIM_CPU *cpu, int first_p)
{
  /* Do nothing.  */
}

void
sh64_media_model_insn_before (SIM_CPU *cpu, int first_p)
{
  /* Do nothing.  */
}

/* Record the cycles computed for an insn.
   LAST_P is non-zero if this is the last insn in a set of parallel insns,
   and we update the total cycle count.
   CYCLES is the cycle count of the insn.  */

void
sh64_compact_model_insn_after(SIM_CPU *cpu, int last_p, int cycles)
{
  /* Do nothing.  */
}

void
sh64_media_model_insn_after(SIM_CPU *cpu, int last_p, int cycles)
{
  /* Do nothing.  */
}

int
sh64_fetch_register (SIM_CPU *cpu, int nr, unsigned char *buf, int len)
{
  /* Fetch general purpose registers. */
  if (nr >= SIM_SH64_R0_REGNUM
      && nr < (SIM_SH64_R0_REGNUM + SIM_SH64_NR_R_REGS)
      && len == 8)
    {
      *((unsigned64*) buf) =
	H2T_8 (sh64_h_gr_get (cpu, nr - SIM_SH64_R0_REGNUM));
      return len;
    }

  /* Fetch PC.  */
  if (nr == SIM_SH64_PC_REGNUM && len == 8)
    {
      *((unsigned64*) buf) = H2T_8 (sh64_h_pc_get (cpu) | sh64_h_ism_get (cpu));
      return len;
    }

  /* Fetch status register (SR).  */
  if (nr == SIM_SH64_SR_REGNUM && len == 8)
    {
      *((unsigned64*) buf) = H2T_8 (sh64_h_sr_get (cpu));
      return len;
    }
      
  /* Fetch saved status register (SSR) and PC (SPC).  */
  if ((nr == SIM_SH64_SSR_REGNUM || nr == SIM_SH64_SPC_REGNUM)
      && len == 8)
    {
      *((unsigned64*) buf) = 0;
      return len;
    }

  /* Fetch target registers.  */
  if (nr >= SIM_SH64_TR0_REGNUM
      && nr < (SIM_SH64_TR0_REGNUM + SIM_SH64_NR_TR_REGS)
      && len == 8)
    {
      *((unsigned64*) buf) =
	H2T_8 (sh64_h_tr_get (cpu, nr - SIM_SH64_TR0_REGNUM));
      return len;
    }

  /* Fetch floating point registers.  */
  if (nr >= SIM_SH64_FR0_REGNUM
      && nr < (SIM_SH64_FR0_REGNUM + SIM_SH64_NR_FP_REGS)
      && len == 4)
    {
      *((unsigned32*) buf) =
	H2T_4 (sh64_h_fr_get (cpu, nr - SIM_SH64_FR0_REGNUM));
      return len;
    }

  /* We should never get here.  */
  return 0;
}

int
sh64_store_register (SIM_CPU *cpu, int nr, unsigned char *buf, int len)
{
  /* Store general purpose registers. */
  if (nr >= SIM_SH64_R0_REGNUM
      && nr < (SIM_SH64_R0_REGNUM + SIM_SH64_NR_R_REGS)
      && len == 8)
    {
      sh64_h_gr_set (cpu, nr - SIM_SH64_R0_REGNUM, T2H_8 (*((unsigned64*)buf)));
      return len;
    }

  /* Store PC.  */
  if (nr == SIM_SH64_PC_REGNUM && len == 8)
    {
      unsigned64 new_pc = T2H_8 (*((unsigned64*)buf));
      sh64_h_pc_set (cpu, new_pc);
      return len;
    }

  /* Store status register (SR).  */
  if (nr == SIM_SH64_SR_REGNUM && len == 8)
    {
      sh64_h_sr_set (cpu, T2H_8 (*((unsigned64*)buf)));
      return len;
    }

  /* Store saved status register (SSR) and PC (SPC).  */
  if (nr == SIM_SH64_SSR_REGNUM || nr == SIM_SH64_SPC_REGNUM)
    {
      /* Do nothing.  */
      return len;
    }

  /* Store target registers.  */
  if (nr >= SIM_SH64_TR0_REGNUM
      && nr < (SIM_SH64_TR0_REGNUM + SIM_SH64_NR_TR_REGS)
      && len == 8)
    {
      sh64_h_tr_set (cpu, nr - SIM_SH64_TR0_REGNUM, T2H_8 (*((unsigned64*)buf)));
      return len;
    }

  /* Store floating point registers.  */
  if (nr >= SIM_SH64_FR0_REGNUM
      && nr < (SIM_SH64_FR0_REGNUM + SIM_SH64_NR_FP_REGS)
      && len == 4)
    {
      sh64_h_fr_set (cpu, nr - SIM_SH64_FR0_REGNUM, T2H_4 (*((unsigned32*)buf)));
      return len;
    }

  /* We should never get here.  */
  return 0;
}

void
sh64_engine_run_full(SIM_CPU *cpu)
{
  if (sh64_h_ism_get (cpu) == ISM_MEDIA)
    {
      if (!sh64_idesc_media)
	{
	  sh64_media_init_idesc_table (cpu);
	  sh64_idesc_media = CPU_IDESC (cpu);
	}
      else
	CPU_IDESC (cpu) = sh64_idesc_media;
      sh64_media_engine_run_full (cpu);
    }
  else
    {
      if (!sh64_idesc_compact)
	{
	  sh64_compact_init_idesc_table (cpu);
	  sh64_idesc_compact = CPU_IDESC (cpu);
	}
      else
	CPU_IDESC (cpu) = sh64_idesc_compact;
      sh64_compact_engine_run_full (cpu);
    }
}

void
sh64_engine_run_fast (SIM_CPU *cpu)
{
  if (sh64_h_ism_get (cpu) == ISM_MEDIA)
    {
      if (!sh64_idesc_media)
	{
	  sh64_media_init_idesc_table (cpu);
	  sh64_idesc_media = CPU_IDESC (cpu);
	}
      else
	CPU_IDESC (cpu) = sh64_idesc_media;
      sh64_media_engine_run_fast (cpu);
    }
  else
    {
      if (!sh64_idesc_compact)
	{
	  sh64_compact_init_idesc_table (cpu);
	  sh64_idesc_compact = CPU_IDESC (cpu);
	}
      else
	CPU_IDESC (cpu) = sh64_idesc_compact;
      sh64_compact_engine_run_fast (cpu);
    }
}

static void
sh64_prepare_run (SIM_CPU *cpu)
{
  /* Nothing.  */
}

static const CGEN_INSN *
sh64_get_idata (SIM_CPU *cpu, int inum)
{
  return CPU_IDESC (cpu) [inum].idata;
}

static void
sh64_init_cpu (SIM_CPU *cpu)
{
  CPU_REG_FETCH (cpu) = sh64_fetch_register;
  CPU_REG_STORE (cpu) = sh64_store_register;
  CPU_PC_FETCH (cpu) = sh64_h_pc_get;
  CPU_PC_STORE (cpu) = sh64_h_pc_set;
  CPU_GET_IDATA (cpu) = sh64_get_idata;
  /* Only used by profiling.  0 disables it. */
  CPU_MAX_INSNS (cpu) = 0;
  CPU_INSN_NAME (cpu) = cgen_insn_name;
  CPU_FULL_ENGINE_FN (cpu) = sh64_engine_run_full;
#if WITH_FAST
  CPU_FAST_ENGINE_FN (cpu) = sh64_engine_run_fast;
#else
  CPU_FAST_ENGINE_FN (cpu) = sh64_engine_run_full;
#endif
}

static void
shmedia_init_cpu (SIM_CPU *cpu)
{
  sh64_init_cpu (cpu);
}

static void
shcompact_init_cpu (SIM_CPU *cpu)
{ 
  sh64_init_cpu (cpu);
}

static void
sh64_model_init()
{
  /* Do nothing.  */
}

static const MODEL sh_models [] =
{
  { "sh2",        & sh2_mach,         MODEL_SH5, NULL, sh64_model_init },
  { "sh2e",       & sh2e_mach,        MODEL_SH5, NULL, sh64_model_init },
  { "sh2a",       & sh2a_fpu_mach,    MODEL_SH5, NULL, sh64_model_init },
  { "sh2a_nofpu", & sh2a_nofpu_mach,  MODEL_SH5, NULL, sh64_model_init },
  { "sh3",        & sh3_mach,         MODEL_SH5, NULL, sh64_model_init },
  { "sh3e",       & sh3_mach,         MODEL_SH5, NULL, sh64_model_init },
  { "sh4",        & sh4_mach,         MODEL_SH5, NULL, sh64_model_init },
  { "sh4_nofpu",  & sh4_nofpu_mach,   MODEL_SH5, NULL, sh64_model_init },
  { "sh4a",       & sh4a_mach,        MODEL_SH5, NULL, sh64_model_init },
  { "sh4a_nofpu", & sh4a_nofpu_mach,  MODEL_SH5, NULL, sh64_model_init },
  { "sh4al",      & sh4al_mach,       MODEL_SH5, NULL, sh64_model_init },
  { "sh5",        & sh5_mach,         MODEL_SH5, NULL, sh64_model_init },
  { 0 }
};

static const MACH_IMP_PROPERTIES sh5_imp_properties =
{
  sizeof (SIM_CPU),
#if WITH_SCACHE
  sizeof (SCACHE)
#else
  0
#endif
};

const MACH sh2_mach =
{
  "sh2", "sh2", MACH_SH5,
  16, 16, &sh_models[0], &sh5_imp_properties,
  shcompact_init_cpu,
  sh64_prepare_run
};

const MACH sh2e_mach =
{
  "sh2e", "sh2e", MACH_SH5,
  16, 16, &sh_models[1], &sh5_imp_properties,
  shcompact_init_cpu,
  sh64_prepare_run
};

const MACH sh2a_fpu_mach =
{
  "sh2a", "sh2a", MACH_SH5,
  16, 16, &sh_models[2], &sh5_imp_properties,
  shcompact_init_cpu,
  sh64_prepare_run
};

const MACH sh2a_nofpu_mach =
{
  "sh2a_nofpu", "sh2a_nofpu", MACH_SH5,
  16, 16, &sh_models[3], &sh5_imp_properties,
  shcompact_init_cpu,
  sh64_prepare_run
};

const MACH sh3_mach =
{
  "sh3", "sh3", MACH_SH5,
  16, 16, &sh_models[4], &sh5_imp_properties,
  shcompact_init_cpu,
  sh64_prepare_run
};

const MACH sh3e_mach =
{
  "sh3e", "sh3e", MACH_SH5,
  16, 16, &sh_models[5], &sh5_imp_properties,
  shcompact_init_cpu,
  sh64_prepare_run
};

const MACH sh4_mach =
{
  "sh4", "sh4", MACH_SH5,
  16, 16, &sh_models[6], &sh5_imp_properties,
  shcompact_init_cpu,
  sh64_prepare_run
};

const MACH sh4_nofpu_mach =
{
  "sh4_nofpu", "sh4_nofpu", MACH_SH5,
  16, 16, &sh_models[7], &sh5_imp_properties,
  shcompact_init_cpu,
  sh64_prepare_run
};

const MACH sh4a_mach =
{
  "sh4a", "sh4a", MACH_SH5,
  16, 16, &sh_models[8], &sh5_imp_properties,
  shcompact_init_cpu,
  sh64_prepare_run
};

const MACH sh4a_nofpu_mach =
{
  "sh4a_nofpu", "sh4a_nofpu", MACH_SH5,
  16, 16, &sh_models[9], &sh5_imp_properties,
  shcompact_init_cpu,
  sh64_prepare_run
};

const MACH sh4al_mach =
{
  "sh4al", "sh4al", MACH_SH5,
  16, 16, &sh_models[10], &sh5_imp_properties,
  shcompact_init_cpu,
  sh64_prepare_run
};

const MACH sh5_mach =
{
  "sh5", "sh5", MACH_SH5,
  32, 32, &sh_models[11], &sh5_imp_properties,
  shmedia_init_cpu,
  sh64_prepare_run
};
