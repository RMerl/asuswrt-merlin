/* m32rx simulator support code
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

#define WANT_CPU m32rxf
#define WANT_CPU_M32RXF

#include "sim-main.h"
#include "cgen-mem.h"
#include "cgen-ops.h"

/* The contents of BUF are in target byte order.  */

int
m32rxf_fetch_register (SIM_CPU *current_cpu, int rn, unsigned char *buf, int len)
{
  return m32rbf_fetch_register (current_cpu, rn, buf, len);
}

/* The contents of BUF are in target byte order.  */

int
m32rxf_store_register (SIM_CPU *current_cpu, int rn, unsigned char *buf, int len)
{
  return m32rbf_store_register (current_cpu, rn, buf, len);
}

/* Cover fns to get/set the control registers.
   FIXME: Duplicated from m32r.c.  The issue is structure offsets.  */

USI
m32rxf_h_cr_get_handler (SIM_CPU *current_cpu, UINT cr)
{
  switch (cr)
    {
    case H_CR_PSW : /* psw */
      return (((CPU (h_bpsw) & 0xc1) << 8)
	      | ((CPU (h_psw) & 0xc0) << 0)
	      | GET_H_COND ());
    case H_CR_BBPSW : /* backup backup psw */
      return CPU (h_bbpsw) & 0xc1;
    case H_CR_CBR : /* condition bit */
      return GET_H_COND ();
    case H_CR_SPI : /* interrupt stack pointer */
      if (! GET_H_SM ())
	return CPU (h_gr[H_GR_SP]);
      else
	return CPU (h_cr[H_CR_SPI]);
    case H_CR_SPU : /* user stack pointer */
      if (GET_H_SM ())
	return CPU (h_gr[H_GR_SP]);
      else
	return CPU (h_cr[H_CR_SPU]);
    case H_CR_BPC : /* backup pc */
      return CPU (h_cr[H_CR_BPC]) & 0xfffffffe;
    case H_CR_BBPC : /* backup backup pc */
      return CPU (h_cr[H_CR_BBPC]) & 0xfffffffe;
    case 4 : /* ??? unspecified, but apparently available */
    case 5 : /* ??? unspecified, but apparently available */
      return CPU (h_cr[cr]);
    default :
      return 0;
    }
}

void
m32rxf_h_cr_set_handler (SIM_CPU *current_cpu, UINT cr, USI newval)
{
  switch (cr)
    {
    case H_CR_PSW : /* psw */
      {
	int old_sm = (CPU (h_psw) & 0x80) != 0;
	int new_sm = (newval & 0x80) != 0;
	CPU (h_bpsw) = (newval >> 8) & 0xff;
	CPU (h_psw) = newval & 0xff;
	SET_H_COND (newval & 1);
	/* When switching stack modes, update the registers.  */
	if (old_sm != new_sm)
	  {
	    if (old_sm)
	      {
		/* Switching user -> system.  */
		CPU (h_cr[H_CR_SPU]) = CPU (h_gr[H_GR_SP]);
		CPU (h_gr[H_GR_SP]) = CPU (h_cr[H_CR_SPI]);
	      }
	    else
	      {
		/* Switching system -> user.  */
		CPU (h_cr[H_CR_SPI]) = CPU (h_gr[H_GR_SP]);
		CPU (h_gr[H_GR_SP]) = CPU (h_cr[H_CR_SPU]);
	      }
	  }
	break;
      }
    case H_CR_BBPSW : /* backup backup psw */
      CPU (h_bbpsw) = newval & 0xff;
      break;
    case H_CR_CBR : /* condition bit */
      SET_H_COND (newval & 1);
      break;
    case H_CR_SPI : /* interrupt stack pointer */
      if (! GET_H_SM ())
	CPU (h_gr[H_GR_SP]) = newval;
      else
	CPU (h_cr[H_CR_SPI]) = newval;
      break;
    case H_CR_SPU : /* user stack pointer */
      if (GET_H_SM ())
	CPU (h_gr[H_GR_SP]) = newval;
      else
	CPU (h_cr[H_CR_SPU]) = newval;
      break;
    case H_CR_BPC : /* backup pc */
      CPU (h_cr[H_CR_BPC]) = newval;
      break;
    case H_CR_BBPC : /* backup backup pc */
      CPU (h_cr[H_CR_BBPC]) = newval;
      break;
    case 4 : /* ??? unspecified, but apparently available */
    case 5 : /* ??? unspecified, but apparently available */
      CPU (h_cr[cr]) = newval;
      break;
    default :
      /* ignore */
      break;
    }
}

/* Cover fns to access h-psw.  */

UQI
m32rxf_h_psw_get_handler (SIM_CPU *current_cpu)
{
  return (CPU (h_psw) & 0xfe) | (CPU (h_cond) & 1);
}

void
m32rxf_h_psw_set_handler (SIM_CPU *current_cpu, UQI newval)
{
  CPU (h_psw) = newval;
  CPU (h_cond) = newval & 1;
}

/* Cover fns to access h-accum.  */

DI
m32rxf_h_accum_get_handler (SIM_CPU *current_cpu)
{
  /* Sign extend the top 8 bits.  */
  DI r;
  r = ANDDI (CPU (h_accum), MAKEDI (0xffffff, 0xffffffff));
  r = XORDI (r, MAKEDI (0x800000, 0));
  r = SUBDI (r, MAKEDI (0x800000, 0));
  return r;
}

void
m32rxf_h_accum_set_handler (SIM_CPU *current_cpu, DI newval)
{
  CPU (h_accum) = newval;
}

/* Cover fns to access h-accums.  */

DI
m32rxf_h_accums_get_handler (SIM_CPU *current_cpu, UINT regno)
{
  /* FIXME: Yes, this is just a quick hack.  */
  DI r;
  if (regno == 0)
    r = CPU (h_accum);
  else
    r = CPU (h_accums[1]);
  /* Sign extend the top 8 bits.  */
  r = ANDDI (r, MAKEDI (0xffffff, 0xffffffff));
  r = XORDI (r, MAKEDI (0x800000, 0));
  r = SUBDI (r, MAKEDI (0x800000, 0));
  return r;
}

void
m32rxf_h_accums_set_handler (SIM_CPU *current_cpu, UINT regno, DI newval)
{
  /* FIXME: Yes, this is just a quick hack.  */
  if (regno == 0)
    CPU (h_accum) = newval;
  else
    CPU (h_accums[1]) = newval;
}

#if WITH_PROFILE_MODEL_P

/* Initialize cycle counting for an insn.
   FIRST_P is non-zero if this is the first insn in a set of parallel
   insns.  */

void
m32rxf_model_insn_before (SIM_CPU *cpu, int first_p)
{
  m32rbf_model_insn_before (cpu, first_p);
}

/* Record the cycles computed for an insn.
   LAST_P is non-zero if this is the last insn in a set of parallel insns,
   and we update the total cycle count.
   CYCLES is the cycle count of the insn.  */

void
m32rxf_model_insn_after (SIM_CPU *cpu, int last_p, int cycles)
{
  m32rbf_model_insn_after (cpu, last_p, cycles);
}

static INLINE void
check_load_stall (SIM_CPU *cpu, int regno)
{
  UINT h_gr = CPU_M32R_MISC_PROFILE (cpu)->load_regs;

  if (regno != -1
      && (h_gr & (1 << regno)) != 0)
    {
      CPU_M32R_MISC_PROFILE (cpu)->load_stall += 2;
      if (TRACE_INSN_P (cpu))
	cgen_trace_printf (cpu, " ; Load stall of 2 cycles.");
    }
}

int
m32rxf_model_m32rx_u_exec (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT sr, INT sr2, INT dr)
{
  check_load_stall (cpu, sr);
  check_load_stall (cpu, sr2);
  return idesc->timing->units[unit_num].done;
}

int
m32rxf_model_m32rx_u_cmp (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT src1, INT src2)
{
  check_load_stall (cpu, src1);
  check_load_stall (cpu, src2);
  return idesc->timing->units[unit_num].done;
}

int
m32rxf_model_m32rx_u_mac (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT src1, INT src2)
{
  check_load_stall (cpu, src1);
  check_load_stall (cpu, src2);
  return idesc->timing->units[unit_num].done;
}

int
m32rxf_model_m32rx_u_cti (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT sr)
{
  PROFILE_DATA *profile = CPU_PROFILE_DATA (cpu);
  int taken_p = (referenced & (1 << 1)) != 0;

  check_load_stall (cpu, sr);
  if (taken_p)
    {
      CPU_M32R_MISC_PROFILE (cpu)->cti_stall += 2;
      PROFILE_MODEL_TAKEN_COUNT (profile) += 1;
    }
  else
    PROFILE_MODEL_UNTAKEN_COUNT (profile) += 1;
  return idesc->timing->units[unit_num].done;
}

int
m32rxf_model_m32rx_u_load (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT sr, INT dr)
{
  CPU_M32R_MISC_PROFILE (cpu)->load_regs_pending |= (1 << dr);
  return idesc->timing->units[unit_num].done;
}

int
m32rxf_model_m32rx_u_store (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced,
			    INT src1, INT src2)
{
  return idesc->timing->units[unit_num].done;
}

#endif /* WITH_PROFILE_MODEL_P */
