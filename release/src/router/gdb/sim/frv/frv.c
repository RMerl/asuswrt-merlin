/* frv simulator support code
   Copyright (C) 1998, 1999, 2000, 2001, 2003, 2004, 2007
   Free Software Foundation, Inc.
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

#define WANT_CPU
#define WANT_CPU_FRVBF

#include "sim-main.h"
#include "cgen-mem.h"
#include "cgen-ops.h"
#include "cgen-engine.h"
#include "cgen-par.h"
#include "bfd.h"
#include "gdb/sim-frv.h"
#include <math.h>

/* Maintain a flag in order to know when to write the address of the next
   VLIW instruction into the LR register.  Used by JMPL. JMPIL, and CALL
   insns.  */
int frvbf_write_next_vliw_addr_to_LR;

/* The contents of BUF are in target byte order.  */
int
frvbf_fetch_register (SIM_CPU *current_cpu, int rn, unsigned char *buf, int len)
{
  if (SIM_FRV_GR0_REGNUM <= rn && rn <= SIM_FRV_GR63_REGNUM)
    {
      int hi_available, lo_available;
      int grn = rn - SIM_FRV_GR0_REGNUM;

      frv_gr_registers_available (current_cpu, &hi_available, &lo_available);

      if ((grn < 32 && !lo_available) || (grn >= 32 && !hi_available))
	return 0;
      else
	SETTSI (buf, GET_H_GR (grn));
    }
  else if (SIM_FRV_FR0_REGNUM <= rn && rn <= SIM_FRV_FR63_REGNUM)
    {
      int hi_available, lo_available;
      int frn = rn - SIM_FRV_FR0_REGNUM;

      frv_fr_registers_available (current_cpu, &hi_available, &lo_available);

      if ((frn < 32 && !lo_available) || (frn >= 32 && !hi_available))
	return 0;
      else
	SETTSI (buf, GET_H_FR (frn));
    }
  else if (rn == SIM_FRV_PC_REGNUM)
    SETTSI (buf, GET_H_PC ());
  else if (SIM_FRV_SPR0_REGNUM <= rn && rn <= SIM_FRV_SPR4095_REGNUM)
    {
      /* Make sure the register is implemented.  */
      FRV_REGISTER_CONTROL *control = CPU_REGISTER_CONTROL (current_cpu);
      int spr = rn - SIM_FRV_SPR0_REGNUM;
      if (! control->spr[spr].implemented)
	return 0;
      SETTSI (buf, GET_H_SPR (spr));
    }
  else
    {
      SETTSI (buf, 0xdeadbeef);
      return 0;
    }

  return len;
}

/* The contents of BUF are in target byte order.  */

int
frvbf_store_register (SIM_CPU *current_cpu, int rn, unsigned char *buf, int len)
{
  if (SIM_FRV_GR0_REGNUM <= rn && rn <= SIM_FRV_GR63_REGNUM)
    {
      int hi_available, lo_available;
      int grn = rn - SIM_FRV_GR0_REGNUM;

      frv_gr_registers_available (current_cpu, &hi_available, &lo_available);

      if ((grn < 32 && !lo_available) || (grn >= 32 && !hi_available))
	return 0;
      else
	SET_H_GR (grn, GETTSI (buf));
    }
  else if (SIM_FRV_FR0_REGNUM <= rn && rn <= SIM_FRV_FR63_REGNUM)
    {
      int hi_available, lo_available;
      int frn = rn - SIM_FRV_FR0_REGNUM;

      frv_fr_registers_available (current_cpu, &hi_available, &lo_available);

      if ((frn < 32 && !lo_available) || (frn >= 32 && !hi_available))
	return 0;
      else
	SET_H_FR (frn, GETTSI (buf));
    }
  else if (rn == SIM_FRV_PC_REGNUM)
    SET_H_PC (GETTSI (buf));
  else if (SIM_FRV_SPR0_REGNUM <= rn && rn <= SIM_FRV_SPR4095_REGNUM)
    {
      /* Make sure the register is implemented.  */
      FRV_REGISTER_CONTROL *control = CPU_REGISTER_CONTROL (current_cpu);
      int spr = rn - SIM_FRV_SPR0_REGNUM;
      if (! control->spr[spr].implemented)
	return 0;
      SET_H_SPR (spr, GETTSI (buf));
    }
  else
    return 0;

  return len;
}

/* Cover fns to access the general registers.  */
USI
frvbf_h_gr_get_handler (SIM_CPU *current_cpu, UINT gr)
{
  frv_check_gr_access (current_cpu, gr);
  return CPU (h_gr[gr]);
}

void
frvbf_h_gr_set_handler (SIM_CPU *current_cpu, UINT gr, USI newval)
{
  frv_check_gr_access (current_cpu, gr);

  if (gr == 0)
    return; /* Storing into gr0 has no effect.  */

  CPU (h_gr[gr]) = newval;
}

/* Cover fns to access the floating point registers.  */
SF
frvbf_h_fr_get_handler (SIM_CPU *current_cpu, UINT fr)
{
  frv_check_fr_access (current_cpu, fr);
  return CPU (h_fr[fr]);
}

void
frvbf_h_fr_set_handler (SIM_CPU *current_cpu, UINT fr, SF newval)
{
  frv_check_fr_access (current_cpu, fr);
  CPU (h_fr[fr]) = newval;
}

/* Cover fns to access the general registers as double words.  */
static UINT
check_register_alignment (SIM_CPU *current_cpu, UINT reg, int align_mask)
{
  if (reg & align_mask)
    {
      SIM_DESC sd = CPU_STATE (current_cpu);
      switch (STATE_ARCHITECTURE (sd)->mach)
	{
	  /* Note: there is a discrepancy between V2.2 of the FR400 
	     instruction manual and the various FR4xx LSI specs.
	     The former claims that unaligned registers cause a
	     register_exception while the latter say it's an
	     illegal_instruction.  The LSI specs appear to be
	     correct; in fact, the FR4xx series is not documented
	     as having a register_exception.  */
	case bfd_mach_fr400:
	case bfd_mach_fr450:
	case bfd_mach_fr550:
	  frv_queue_program_interrupt (current_cpu, FRV_ILLEGAL_INSTRUCTION);
	  break;
	case bfd_mach_frvtomcat:
	case bfd_mach_fr500:
	case bfd_mach_frv:
	  frv_queue_register_exception_interrupt (current_cpu,
						  FRV_REC_UNALIGNED);
	  break;
	default:
	  break;
	}

      reg &= ~align_mask;
    }

  return reg;
}

static UINT
check_fr_register_alignment (SIM_CPU *current_cpu, UINT reg, int align_mask)
{
  if (reg & align_mask)
    {
      SIM_DESC sd = CPU_STATE (current_cpu);
      switch (STATE_ARCHITECTURE (sd)->mach)
	{
	  /* See comment in check_register_alignment().  */
	case bfd_mach_fr400:
	case bfd_mach_fr450:
	case bfd_mach_fr550:
	  frv_queue_program_interrupt (current_cpu, FRV_ILLEGAL_INSTRUCTION);
	  break;
	case bfd_mach_frvtomcat:
	case bfd_mach_fr500:
	case bfd_mach_frv:
	  {
	    struct frv_fp_exception_info fp_info = {
	      FSR_NO_EXCEPTION, FTT_INVALID_FR
	    };
	    frv_queue_fp_exception_interrupt (current_cpu, & fp_info);
	  }
	  break;
	default:
	  break;
	}

      reg &= ~align_mask;
    }

  return reg;
}

static UINT
check_memory_alignment (SIM_CPU *current_cpu, SI address, int align_mask)
{
  if (address & align_mask)
    {
      SIM_DESC sd = CPU_STATE (current_cpu);
      switch (STATE_ARCHITECTURE (sd)->mach)
	{
	  /* See comment in check_register_alignment().  */
	case bfd_mach_fr400:
	case bfd_mach_fr450:
	  frv_queue_data_access_error_interrupt (current_cpu, address);
	  break;
	case bfd_mach_frvtomcat:
	case bfd_mach_fr500:
	case bfd_mach_frv:
	  frv_queue_mem_address_not_aligned_interrupt (current_cpu, address);
	  break;
	default:
	  break;
	}

      address &= ~align_mask;
    }

  return address;
}

DI
frvbf_h_gr_double_get_handler (SIM_CPU *current_cpu, UINT gr)
{
  DI value;

  if (gr == 0)
    return 0; /* gr0 is always 0.  */

  /* Check the register alignment.  */
  gr = check_register_alignment (current_cpu, gr, 1);

  value = GET_H_GR (gr);
  value <<= 32;
  value |=  (USI) GET_H_GR (gr + 1);
  return value;
}

void
frvbf_h_gr_double_set_handler (SIM_CPU *current_cpu, UINT gr, DI newval)
{
  if (gr == 0)
    return; /* Storing into gr0 has no effect.  */

  /* Check the register alignment.  */
  gr = check_register_alignment (current_cpu, gr, 1);

  SET_H_GR (gr    , (newval >> 32) & 0xffffffff);
  SET_H_GR (gr + 1, (newval      ) & 0xffffffff);
}

/* Cover fns to access the floating point register as double words.  */
DF
frvbf_h_fr_double_get_handler (SIM_CPU *current_cpu, UINT fr)
{
  union {
    SF as_sf[2];
    DF as_df;
  } value;

  /* Check the register alignment.  */
  fr = check_fr_register_alignment (current_cpu, fr, 1);

  if (CURRENT_HOST_BYTE_ORDER == LITTLE_ENDIAN)
    {
      value.as_sf[1] = GET_H_FR (fr);
      value.as_sf[0] = GET_H_FR (fr + 1);
    }
  else
    {
      value.as_sf[0] = GET_H_FR (fr);
      value.as_sf[1] = GET_H_FR (fr + 1);
    }

  return value.as_df;
}

void
frvbf_h_fr_double_set_handler (SIM_CPU *current_cpu, UINT fr, DF newval)
{
  union {
    SF as_sf[2];
    DF as_df;
  } value;

  /* Check the register alignment.  */
  fr = check_fr_register_alignment (current_cpu, fr, 1);

  value.as_df = newval;
  if (CURRENT_HOST_BYTE_ORDER == LITTLE_ENDIAN)
    {
      SET_H_FR (fr    , value.as_sf[1]);
      SET_H_FR (fr + 1, value.as_sf[0]);
    }
  else
    {
      SET_H_FR (fr    , value.as_sf[0]);
      SET_H_FR (fr + 1, value.as_sf[1]);
    }
}

/* Cover fns to access the floating point register as integer words.  */
USI
frvbf_h_fr_int_get_handler (SIM_CPU *current_cpu, UINT fr)
{
  union {
    SF  as_sf;
    USI as_usi;
  } value;

  value.as_sf = GET_H_FR (fr);
  return value.as_usi;
}

void
frvbf_h_fr_int_set_handler (SIM_CPU *current_cpu, UINT fr, USI newval)
{
  union {
    SF  as_sf;
    USI as_usi;
  } value;

  value.as_usi = newval;
  SET_H_FR (fr, value.as_sf);
}

/* Cover fns to access the coprocessor registers as double words.  */
DI
frvbf_h_cpr_double_get_handler (SIM_CPU *current_cpu, UINT cpr)
{
  DI value;

  /* Check the register alignment.  */
  cpr = check_register_alignment (current_cpu, cpr, 1);

  value = GET_H_CPR (cpr);
  value <<= 32;
  value |=  (USI) GET_H_CPR (cpr + 1);
  return value;
}

void
frvbf_h_cpr_double_set_handler (SIM_CPU *current_cpu, UINT cpr, DI newval)
{
  /* Check the register alignment.  */
  cpr = check_register_alignment (current_cpu, cpr, 1);

  SET_H_CPR (cpr    , (newval >> 32) & 0xffffffff);
  SET_H_CPR (cpr + 1, (newval      ) & 0xffffffff);
}

/* Cover fns to write registers as quad words.  */
void
frvbf_h_gr_quad_set_handler (SIM_CPU *current_cpu, UINT gr, SI *newval)
{
  if (gr == 0)
    return; /* Storing into gr0 has no effect.  */

  /* Check the register alignment.  */
  gr = check_register_alignment (current_cpu, gr, 3);

  SET_H_GR (gr    , newval[0]);
  SET_H_GR (gr + 1, newval[1]);
  SET_H_GR (gr + 2, newval[2]);
  SET_H_GR (gr + 3, newval[3]);
}

void
frvbf_h_fr_quad_set_handler (SIM_CPU *current_cpu, UINT fr, SI *newval)
{
  /* Check the register alignment.  */
  fr = check_fr_register_alignment (current_cpu, fr, 3);

  SET_H_FR (fr    , newval[0]);
  SET_H_FR (fr + 1, newval[1]);
  SET_H_FR (fr + 2, newval[2]);
  SET_H_FR (fr + 3, newval[3]);
}

void
frvbf_h_cpr_quad_set_handler (SIM_CPU *current_cpu, UINT cpr, SI *newval)
{
  /* Check the register alignment.  */
  cpr = check_register_alignment (current_cpu, cpr, 3);

  SET_H_CPR (cpr    , newval[0]);
  SET_H_CPR (cpr + 1, newval[1]);
  SET_H_CPR (cpr + 2, newval[2]);
  SET_H_CPR (cpr + 3, newval[3]);
}

/* Cover fns to access the special purpose registers.  */
USI
frvbf_h_spr_get_handler (SIM_CPU *current_cpu, UINT spr)
{
  /* Check access restrictions.  */
  frv_check_spr_read_access (current_cpu, spr);

  switch (spr)
    {
    case H_SPR_PSR:
      return spr_psr_get_handler (current_cpu);
    case H_SPR_TBR:
      return spr_tbr_get_handler (current_cpu);
    case H_SPR_BPSR:
      return spr_bpsr_get_handler (current_cpu);
    case H_SPR_CCR:
      return spr_ccr_get_handler (current_cpu);
    case H_SPR_CCCR:
      return spr_cccr_get_handler (current_cpu);
    case H_SPR_SR0:
    case H_SPR_SR1:
    case H_SPR_SR2:
    case H_SPR_SR3:
      return spr_sr_get_handler (current_cpu, spr);
      break;
    default:
      return CPU (h_spr[spr]);
    }
  return 0;
}

void
frvbf_h_spr_set_handler (SIM_CPU *current_cpu, UINT spr, USI newval)
{
  FRV_REGISTER_CONTROL *control;
  USI mask;
  USI oldval;

  /* Check access restrictions.  */
  frv_check_spr_write_access (current_cpu, spr);

  /* Only set those fields which are writeable.  */
  control = CPU_REGISTER_CONTROL (current_cpu);
  mask = control->spr[spr].read_only_mask;
  oldval = GET_H_SPR (spr);

  newval = (newval & ~mask) | (oldval & mask);

  /* Some registers are represented by individual components which are
     referenced more often than the register itself.  */
  switch (spr)
    {
    case H_SPR_PSR:
      spr_psr_set_handler (current_cpu, newval);
      break;
    case H_SPR_TBR:
      spr_tbr_set_handler (current_cpu, newval);
      break;
    case H_SPR_BPSR:
      spr_bpsr_set_handler (current_cpu, newval);
      break;
    case H_SPR_CCR:
      spr_ccr_set_handler (current_cpu, newval);
      break;
    case H_SPR_CCCR:
      spr_cccr_set_handler (current_cpu, newval);
      break;
    case H_SPR_SR0:
    case H_SPR_SR1:
    case H_SPR_SR2:
    case H_SPR_SR3:
      spr_sr_set_handler (current_cpu, spr, newval);
      break;
    case H_SPR_IHSR8:
      frv_cache_reconfigure (current_cpu, CPU_INSN_CACHE (current_cpu));
      break;
    default:
      CPU (h_spr[spr]) = newval;
      break;
    }
}

/* Cover fns to access the gr_hi and gr_lo registers.  */
UHI
frvbf_h_gr_hi_get_handler (SIM_CPU *current_cpu, UINT gr)
{
  return (GET_H_GR(gr) >> 16) & 0xffff;
}

void
frvbf_h_gr_hi_set_handler (SIM_CPU *current_cpu, UINT gr, UHI newval)
{
  USI value = (GET_H_GR (gr) & 0xffff) | (newval << 16);
  SET_H_GR (gr, value);
}

UHI
frvbf_h_gr_lo_get_handler (SIM_CPU *current_cpu, UINT gr)
{
  return GET_H_GR(gr) & 0xffff;
}

void
frvbf_h_gr_lo_set_handler (SIM_CPU *current_cpu, UINT gr, UHI newval)
{
  USI value = (GET_H_GR (gr) & 0xffff0000) | (newval & 0xffff);
  SET_H_GR (gr, value);
}

/* Cover fns to access the tbr bits.  */
USI
spr_tbr_get_handler (SIM_CPU *current_cpu)
{
  int tbr = ((GET_H_TBR_TBA () & 0xfffff) << 12) |
            ((GET_H_TBR_TT  () &  0xff) <<  4);

  return tbr;
}

void
spr_tbr_set_handler (SIM_CPU *current_cpu, USI newval)
{
  int tbr = newval;

  SET_H_TBR_TBA ((tbr >> 12) & 0xfffff) ;
  SET_H_TBR_TT  ((tbr >>  4) & 0xff) ;
}

/* Cover fns to access the bpsr bits.  */
USI
spr_bpsr_get_handler (SIM_CPU *current_cpu)
{
  int bpsr = ((GET_H_BPSR_BS  () & 0x1) << 12) |
             ((GET_H_BPSR_BET () & 0x1)      );

  return bpsr;
}

void
spr_bpsr_set_handler (SIM_CPU *current_cpu, USI newval)
{
  int bpsr = newval;

  SET_H_BPSR_BS  ((bpsr >> 12) & 1);
  SET_H_BPSR_BET ((bpsr      ) & 1);
}

/* Cover fns to access the psr bits.  */
USI
spr_psr_get_handler (SIM_CPU *current_cpu)
{
  int psr = ((GET_H_PSR_IMPLE () & 0xf) << 28) |
            ((GET_H_PSR_VER   () & 0xf) << 24) |
            ((GET_H_PSR_ICE   () & 0x1) << 16) |
            ((GET_H_PSR_NEM   () & 0x1) << 14) |
            ((GET_H_PSR_CM    () & 0x1) << 13) |
            ((GET_H_PSR_BE    () & 0x1) << 12) |
            ((GET_H_PSR_ESR   () & 0x1) << 11) |
            ((GET_H_PSR_EF    () & 0x1) <<  8) |
            ((GET_H_PSR_EM    () & 0x1) <<  7) |
            ((GET_H_PSR_PIL   () & 0xf) <<  3) |
            ((GET_H_PSR_S     () & 0x1) <<  2) |
            ((GET_H_PSR_PS    () & 0x1) <<  1) |
            ((GET_H_PSR_ET    () & 0x1)      );

  return psr;
}

void
spr_psr_set_handler (SIM_CPU *current_cpu, USI newval)
{
  /* The handler for PSR.S references the value of PSR.ESR, so set PSR.S
     first.  */
  SET_H_PSR_S ((newval >>  2) & 1);

  SET_H_PSR_IMPLE ((newval >> 28) & 0xf);
  SET_H_PSR_VER   ((newval >> 24) & 0xf);
  SET_H_PSR_ICE   ((newval >> 16) & 1);
  SET_H_PSR_NEM   ((newval >> 14) & 1);
  SET_H_PSR_CM    ((newval >> 13) & 1);
  SET_H_PSR_BE    ((newval >> 12) & 1);
  SET_H_PSR_ESR   ((newval >> 11) & 1);
  SET_H_PSR_EF    ((newval >>  8) & 1);
  SET_H_PSR_EM    ((newval >>  7) & 1);
  SET_H_PSR_PIL   ((newval >>  3) & 0xf);
  SET_H_PSR_PS    ((newval >>  1) & 1);
  SET_H_PSR_ET    ((newval      ) & 1);
}

void
frvbf_h_psr_s_set_handler (SIM_CPU *current_cpu, BI newval)
{
  /* If switching from user to supervisor mode, or vice-versa, then switch
     the supervisor/user context.  */
  int psr_s = GET_H_PSR_S ();
  if (psr_s != (newval & 1))
    {
      frvbf_switch_supervisor_user_context (current_cpu);
      CPU (h_psr_s) = newval & 1;
    }
}

/* Cover fns to access the ccr bits.  */
USI
spr_ccr_get_handler (SIM_CPU *current_cpu)
{
  int ccr = ((GET_H_ICCR (H_ICCR_ICC3) & 0xf) << 28) |
            ((GET_H_ICCR (H_ICCR_ICC2) & 0xf) << 24) |
            ((GET_H_ICCR (H_ICCR_ICC1) & 0xf) << 20) |
            ((GET_H_ICCR (H_ICCR_ICC0) & 0xf) << 16) |
            ((GET_H_FCCR (H_FCCR_FCC3) & 0xf) << 12) |
            ((GET_H_FCCR (H_FCCR_FCC2) & 0xf) <<  8) |
            ((GET_H_FCCR (H_FCCR_FCC1) & 0xf) <<  4) |
            ((GET_H_FCCR (H_FCCR_FCC0) & 0xf)      );

  return ccr;
}

void
spr_ccr_set_handler (SIM_CPU *current_cpu, USI newval)
{
  int ccr = newval;

  SET_H_ICCR (H_ICCR_ICC3, (newval >> 28) & 0xf);
  SET_H_ICCR (H_ICCR_ICC2, (newval >> 24) & 0xf);
  SET_H_ICCR (H_ICCR_ICC1, (newval >> 20) & 0xf);
  SET_H_ICCR (H_ICCR_ICC0, (newval >> 16) & 0xf);
  SET_H_FCCR (H_FCCR_FCC3, (newval >> 12) & 0xf);
  SET_H_FCCR (H_FCCR_FCC2, (newval >>  8) & 0xf);
  SET_H_FCCR (H_FCCR_FCC1, (newval >>  4) & 0xf);
  SET_H_FCCR (H_FCCR_FCC0, (newval      ) & 0xf);
}

QI
frvbf_set_icc_for_shift_right (
  SIM_CPU *current_cpu, SI value, SI shift, QI icc
)
{
  /* Set the C flag of the given icc to the logical OR of the bits shifted
     out.  */
  int mask = (1 << shift) - 1;
  if ((value & mask) != 0)
    return icc | 0x1;

  return icc & 0xe;
}

QI
frvbf_set_icc_for_shift_left (
  SIM_CPU *current_cpu, SI value, SI shift, QI icc
)
{
  /* Set the V flag of the given icc to the logical OR of the bits shifted
     out.  */
  int mask = ((1 << shift) - 1) << (32 - shift);
  if ((value & mask) != 0)
    return icc | 0x2;

  return icc & 0xd;
}

/* Cover fns to access the cccr bits.  */
USI
spr_cccr_get_handler (SIM_CPU *current_cpu)
{
  int cccr = ((GET_H_CCCR (H_CCCR_CC7) & 0x3) << 14) |
             ((GET_H_CCCR (H_CCCR_CC6) & 0x3) << 12) |
             ((GET_H_CCCR (H_CCCR_CC5) & 0x3) << 10) |
             ((GET_H_CCCR (H_CCCR_CC4) & 0x3) <<  8) |
             ((GET_H_CCCR (H_CCCR_CC3) & 0x3) <<  6) |
             ((GET_H_CCCR (H_CCCR_CC2) & 0x3) <<  4) |
             ((GET_H_CCCR (H_CCCR_CC1) & 0x3) <<  2) |
             ((GET_H_CCCR (H_CCCR_CC0) & 0x3)      );

  return cccr;
}

void
spr_cccr_set_handler (SIM_CPU *current_cpu, USI newval)
{
  int cccr = newval;

  SET_H_CCCR (H_CCCR_CC7, (newval >> 14) & 0x3);
  SET_H_CCCR (H_CCCR_CC6, (newval >> 12) & 0x3);
  SET_H_CCCR (H_CCCR_CC5, (newval >> 10) & 0x3);
  SET_H_CCCR (H_CCCR_CC4, (newval >>  8) & 0x3);
  SET_H_CCCR (H_CCCR_CC3, (newval >>  6) & 0x3);
  SET_H_CCCR (H_CCCR_CC2, (newval >>  4) & 0x3);
  SET_H_CCCR (H_CCCR_CC1, (newval >>  2) & 0x3);
  SET_H_CCCR (H_CCCR_CC0, (newval      ) & 0x3);
}

/* Cover fns to access the sr bits.  */
USI
spr_sr_get_handler (SIM_CPU *current_cpu, UINT spr)
{
  /* If PSR.ESR is not set, then SR0-3 map onto SGR4-7 which will be GR4-7,
     otherwise the correct mapping of USG4-7 or SGR4-7 will be in SR0-3.  */
  int psr_esr = GET_H_PSR_ESR ();
  if (! psr_esr)
    return GET_H_GR (4 + (spr - H_SPR_SR0));

  return CPU (h_spr[spr]);
}

void
spr_sr_set_handler (SIM_CPU *current_cpu, UINT spr, USI newval)
{
  /* If PSR.ESR is not set, then SR0-3 map onto SGR4-7 which will be GR4-7,
     otherwise the correct mapping of USG4-7 or SGR4-7 will be in SR0-3.  */
  int psr_esr = GET_H_PSR_ESR ();
  if (! psr_esr)
    SET_H_GR (4 + (spr - H_SPR_SR0), newval);
  else
    CPU (h_spr[spr]) = newval;
}

/* Switch SR0-SR4 with GR4-GR7 if PSR.ESR is set.  */
void
frvbf_switch_supervisor_user_context (SIM_CPU *current_cpu)
{
  if (GET_H_PSR_ESR ())
    {
      /* We need to be in supervisor mode to swap the registers. Access the
	 PSR.S directly in order to avoid recursive context switches.  */
      int i;
      int save_psr_s = CPU (h_psr_s);
      CPU (h_psr_s) = 1;
      for (i = 0; i < 4; ++i)
	{
	  int gr = i + 4;
	  int spr = i + H_SPR_SR0;
	  SI tmp = GET_H_SPR (spr);
	  SET_H_SPR (spr, GET_H_GR (gr));
	  SET_H_GR (gr, tmp);
	}
      CPU (h_psr_s) = save_psr_s;
    }
}

/* Handle load/store of quad registers.  */
void
frvbf_load_quad_GR (SIM_CPU *current_cpu, PCADDR pc, SI address, SI targ_ix)
{
  int i;
  SI value[4];

  /* Check memory alignment */
  address = check_memory_alignment (current_cpu, address, 0xf);

  /* If we need to count cycles, then the cache operation will be
     initiated from the model profiling functions.
     See frvbf_model_....  */
  if (model_insn)
    {
      CPU_LOAD_ADDRESS (current_cpu) = address;
      CPU_LOAD_LENGTH (current_cpu) = 16;
    }
  else
    {
      for (i = 0; i < 4; ++i)
	{
	  value[i] = frvbf_read_mem_SI (current_cpu, pc, address);
	  address += 4;
	}
      sim_queue_fn_xi_write (current_cpu, frvbf_h_gr_quad_set_handler, targ_ix,
			     value);
    }
}

void
frvbf_store_quad_GR (SIM_CPU *current_cpu, PCADDR pc, SI address, SI src_ix)
{
  int i;
  SI value[4];
  USI hsr0;

  /* Check register and memory alignment.  */
  src_ix = check_register_alignment (current_cpu, src_ix, 3);
  address = check_memory_alignment (current_cpu, address, 0xf);

  for (i = 0; i < 4; ++i)
    {
      /* GR0 is always 0.  */
      if (src_ix == 0)
	value[i] = 0;
      else
	value[i] = GET_H_GR (src_ix + i);
    }
  hsr0 = GET_HSR0 ();
  if (GET_HSR0_DCE (hsr0))
    sim_queue_fn_mem_xi_write (current_cpu, frvbf_mem_set_XI, address, value);
  else
    sim_queue_mem_xi_write (current_cpu, address, value);
}

void
frvbf_load_quad_FRint (SIM_CPU *current_cpu, PCADDR pc, SI address, SI targ_ix)
{
  int i;
  SI value[4];

  /* Check memory alignment */
  address = check_memory_alignment (current_cpu, address, 0xf);

  /* If we need to count cycles, then the cache operation will be
     initiated from the model profiling functions.
     See frvbf_model_....  */
  if (model_insn)
    {
      CPU_LOAD_ADDRESS (current_cpu) = address;
      CPU_LOAD_LENGTH (current_cpu) = 16;
    }
  else
    {
      for (i = 0; i < 4; ++i)
	{
	  value[i] = frvbf_read_mem_SI (current_cpu, pc, address);
	  address += 4;
	}
      sim_queue_fn_xi_write (current_cpu, frvbf_h_fr_quad_set_handler, targ_ix,
			     value);
    }
}

void
frvbf_store_quad_FRint (SIM_CPU *current_cpu, PCADDR pc, SI address, SI src_ix)
{
  int i;
  SI value[4];
  USI hsr0;

  /* Check register and memory alignment.  */
  src_ix = check_fr_register_alignment (current_cpu, src_ix, 3);
  address = check_memory_alignment (current_cpu, address, 0xf);

  for (i = 0; i < 4; ++i)
    value[i] = GET_H_FR (src_ix + i);

  hsr0 = GET_HSR0 ();
  if (GET_HSR0_DCE (hsr0))
    sim_queue_fn_mem_xi_write (current_cpu, frvbf_mem_set_XI, address, value);
  else
    sim_queue_mem_xi_write (current_cpu, address, value);
}

void
frvbf_load_quad_CPR (SIM_CPU *current_cpu, PCADDR pc, SI address, SI targ_ix)
{
  int i;
  SI value[4];

  /* Check memory alignment */
  address = check_memory_alignment (current_cpu, address, 0xf);

  /* If we need to count cycles, then the cache operation will be
     initiated from the model profiling functions.
     See frvbf_model_....  */
  if (model_insn)
    {
      CPU_LOAD_ADDRESS (current_cpu) = address;
      CPU_LOAD_LENGTH (current_cpu) = 16;
    }
  else
    {
      for (i = 0; i < 4; ++i)
	{
	  value[i] = frvbf_read_mem_SI (current_cpu, pc, address);
	  address += 4;
	}
      sim_queue_fn_xi_write (current_cpu, frvbf_h_cpr_quad_set_handler, targ_ix,
			     value);
    }
}

void
frvbf_store_quad_CPR (SIM_CPU *current_cpu, PCADDR pc, SI address, SI src_ix)
{
  int i;
  SI value[4];
  USI hsr0;

  /* Check register and memory alignment.  */
  src_ix = check_register_alignment (current_cpu, src_ix, 3);
  address = check_memory_alignment (current_cpu, address, 0xf);

  for (i = 0; i < 4; ++i)
    value[i] = GET_H_CPR (src_ix + i);

  hsr0 = GET_HSR0 ();
  if (GET_HSR0_DCE (hsr0))
    sim_queue_fn_mem_xi_write (current_cpu, frvbf_mem_set_XI, address, value);
  else
    sim_queue_mem_xi_write (current_cpu, address, value);
}

void
frvbf_signed_integer_divide (
  SIM_CPU *current_cpu, SI arg1, SI arg2, int target_index, int non_excepting
)
{
  enum frv_dtt dtt = FRV_DTT_NO_EXCEPTION;
  if (arg1 == 0x80000000 && arg2 == -1)
    {
      /* 0x80000000/(-1) must result in 0x7fffffff when ISR.EDE is set
	 otherwise it may result in 0x7fffffff (sparc compatibility) or
	 0x80000000 (C language compatibility). */
      USI isr;
      dtt = FRV_DTT_OVERFLOW;

      isr = GET_ISR ();
      if (GET_ISR_EDE (isr))
	sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, target_index,
			       0x7fffffff);
      else
	sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, target_index,
			       0x80000000);
      frvbf_force_update (current_cpu); /* Force update of target register.  */
    }
  else if (arg2 == 0)
    dtt = FRV_DTT_DIVISION_BY_ZERO;
  else
    sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, target_index,
			   arg1 / arg2);

  /* Check for exceptions.  */
  if (dtt != FRV_DTT_NO_EXCEPTION)
    dtt = frvbf_division_exception (current_cpu, dtt, target_index,
				    non_excepting);
  if (non_excepting && dtt == FRV_DTT_NO_EXCEPTION)
    {
      /* Non excepting instruction. Clear the NE flag for the target
	 register.  */
      SI NE_flags[2];
      GET_NE_FLAGS (NE_flags, H_SPR_GNER0);
      CLEAR_NE_FLAG (NE_flags, target_index);
      SET_NE_FLAGS (H_SPR_GNER0, NE_flags);
    }
}

void
frvbf_unsigned_integer_divide (
  SIM_CPU *current_cpu, USI arg1, USI arg2, int target_index, int non_excepting
)
{
  if (arg2 == 0)
    frvbf_division_exception (current_cpu, FRV_DTT_DIVISION_BY_ZERO,
			      target_index, non_excepting);
  else
    {
      sim_queue_fn_si_write (current_cpu, frvbf_h_gr_set, target_index,
			     arg1 / arg2);
      if (non_excepting)
	{
	  /* Non excepting instruction. Clear the NE flag for the target
	     register.  */
	  SI NE_flags[2];
	  GET_NE_FLAGS (NE_flags, H_SPR_GNER0);
	  CLEAR_NE_FLAG (NE_flags, target_index);
	  SET_NE_FLAGS (H_SPR_GNER0, NE_flags);
	}
    }
}

/* Clear accumulators.  */
void
frvbf_clear_accumulators (SIM_CPU *current_cpu, SI acc_ix, int A)
{
  SIM_DESC sd = CPU_STATE (current_cpu);
  int acc_mask =
    (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr500) ? 7 :
    (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr550) ? 7 :
    (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr450) ? 11 :
    (STATE_ARCHITECTURE (sd)->mach == bfd_mach_fr400) ? 3 :
    63;
  FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (current_cpu);

  ps->mclracc_acc = acc_ix;
  ps->mclracc_A   = A;
  if (A == 0 || acc_ix != 0) /* Clear 1 accumuator?  */
    {
      /* This instruction is a nop if the referenced accumulator is not
	 implemented. */
      if ((acc_ix & acc_mask) == acc_ix)
	sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, acc_ix, 0);
    }
  else
    {
      /* Clear all implemented accumulators.  */
      int i;
      for (i = 0; i <= acc_mask; ++i)
	if ((i & acc_mask) == i)
	  sim_queue_fn_di_write (current_cpu, frvbf_h_acc40S_set, i, 0);
    }
}

/* Functions to aid insn semantics.  */

/* Compute the result of the SCAN and SCANI insns after the shift and xor.  */
SI
frvbf_scan_result (SIM_CPU *current_cpu, SI value)
{
  SI i;
  SI mask;

  if (value == 0)
    return 63;

  /* Find the position of the first non-zero bit.
     The loop will terminate since there is guaranteed to be at least one
     non-zero bit.  */
  mask = 1 << (sizeof (mask) * 8 - 1);
  for (i = 0; (value & mask) == 0; ++i)
    value <<= 1;

  return i;
}

/* Compute the result of the cut insns.  */
SI
frvbf_cut (SIM_CPU *current_cpu, SI reg1, SI reg2, SI cut_point)
{
  SI result;
  if (cut_point < 32)
    {
      result = reg1 << cut_point;
      result |= (reg2 >> (32 - cut_point)) & ((1 << cut_point) - 1);
    }
  else
    result = reg2 << (cut_point - 32);

  return result;
}

/* Compute the result of the cut insns.  */
SI
frvbf_media_cut (SIM_CPU *current_cpu, DI acc, SI cut_point)
{
  /* The cut point is the lower 6 bits (signed) of what we are passed.  */
  cut_point = cut_point << 26 >> 26;

  /* The cut_point is relative to bit 40 of 64 bits.  */
  if (cut_point >= 0)
    return (acc << (cut_point + 24)) >> 32;

  /* Extend the sign bit (bit 40) for negative cuts.  */
  if (cut_point == -32)
    return (acc << 24) >> 63; /* Special case for full shiftout.  */

  return (acc << 24) >> (32 + -cut_point);
}

/* Compute the result of the cut insns.  */
SI
frvbf_media_cut_ss (SIM_CPU *current_cpu, DI acc, SI cut_point)
{
  /* The cut point is the lower 6 bits (signed) of what we are passed.  */
  cut_point = cut_point << 26 >> 26;

  if (cut_point >= 0)
    {
      /* The cut_point is relative to bit 40 of 64 bits.  */
      DI shifted = acc << (cut_point + 24);
      DI unshifted = shifted >> (cut_point + 24);

      /* The result will be saturated if significant bits are shifted out.  */
      if (unshifted != acc)
	{
	  if (acc < 0)
	    return 0x80000000;
	  return 0x7fffffff;
	}
    }

  /* The result will not be saturated, so use the code for the normal cut.  */
  return frvbf_media_cut (current_cpu, acc, cut_point);
}

/* Compute the result of int accumulator cut (SCUTSS).  */
SI
frvbf_iacc_cut (SIM_CPU *current_cpu, DI acc, SI cut_point)
{
  DI lower, upper;

  /* The cut point is the lower 7 bits (signed) of what we are passed.  */
  cut_point = cut_point << 25 >> 25;

  /* Conceptually, the operation is on a 128-bit sign-extension of ACC.
     The top bit of the return value corresponds to bit (63 - CUT_POINT)
     of this 128-bit value.

     Since we can't deal with 128-bit values very easily, convert the
     operation into an equivalent 64-bit one.  */
  if (cut_point < 0)
    {
      /* Avoid an undefined shift operation.  */
      if (cut_point == -64)
	acc >>= 63;
      else
	acc >>= -cut_point;
      cut_point = 0;
    }

  /* Get the shifted but unsaturated result.  Set LOWER to the lowest
     32 bits of the result and UPPER to the result >> 31.  */
  if (cut_point < 32)
    {
      /* The cut loses the (32 - CUT_POINT) least significant bits.
	 Round the result up if the most significant of these lost bits
	 is 1.  */
      lower = acc >> (32 - cut_point);
      if (lower < 0x7fffffff)
	if (acc & LSBIT64 (32 - cut_point - 1))
	  lower++;
      upper = lower >> 31;
    }
  else
    {
      lower = acc << (cut_point - 32);
      upper = acc >> (63 - cut_point);
    }

  /* Saturate the result.  */
  if (upper < -1)
    return ~0x7fffffff;
  else if (upper > 0)
    return 0x7fffffff;
  else
    return lower;
}

/* Compute the result of shift-left-arithmetic-with-saturation (SLASS).  */
SI
frvbf_shift_left_arith_saturate (SIM_CPU *current_cpu, SI arg1, SI arg2)
{
  int neg_arg1;

  /* FIXME: what to do with negative shift amt?  */
  if (arg2 <= 0)
    return arg1;

  if (arg1 == 0)
    return 0;

  /* Signed shift by 31 or greater saturates by definition.  */
  if (arg2 >= 31)
    if (arg1 > 0)
      return (SI) 0x7fffffff;
    else
      return (SI) 0x80000000;

  /* OK, arg2 is between 1 and 31.  */
  neg_arg1 = (arg1 < 0);
  do {
    arg1 <<= 1;
    /* Check for sign bit change (saturation).  */
    if (neg_arg1 && (arg1 >= 0))
      return (SI) 0x80000000;
    else if (!neg_arg1 && (arg1 < 0))
      return (SI) 0x7fffffff;
  } while (--arg2 > 0);

  return arg1;
}

/* Simulate the media custom insns.  */
void
frvbf_media_cop (SIM_CPU *current_cpu, int cop_num)
{
  /* The semantics of the insn are a nop, since it is implementation defined.
     We do need to check whether it's implemented and set up for MTRAP
     if it's not.  */
  USI msr0 = GET_MSR (0);
  if (GET_MSR_EMCI (msr0) == 0)
    {
      /* no interrupt queued at this time.  */
      frv_set_mp_exception_registers (current_cpu, MTT_UNIMPLEMENTED_MPOP, 0);
    }
}

/* Simulate the media average (MAVEH) insn.  */
static HI
do_media_average (SIM_CPU *current_cpu, HI arg1, HI arg2)
{
  SIM_DESC sd = CPU_STATE (current_cpu);
  SI sum = (arg1 + arg2);
  HI result = sum >> 1;
  int rounding_value;

  /* On fr4xx and fr550, check the rounding mode.  On other machines
     rounding is always toward negative infinity and the result is
     already correctly rounded.  */
  switch (STATE_ARCHITECTURE (sd)->mach)
    {
      /* Need to check rounding mode. */
    case bfd_mach_fr400:
    case bfd_mach_fr450:
    case bfd_mach_fr550:
      /* Check whether rounding will be required.  Rounding will be required
	 if the sum is an odd number.  */
      rounding_value = sum & 1;
      if (rounding_value)
	{
	  USI msr0 = GET_MSR (0);
	  /* Check MSR0.SRDAV to determine which bits control the rounding.  */
	  if (GET_MSR_SRDAV (msr0))
	    {
	      /* MSR0.RD controls rounding.  */
	      switch (GET_MSR_RD (msr0))
		{
		case 0:
		  /* Round to nearest.  */
		  if (result >= 0)
		    ++result;
		  break;
		case 1:
		  /* Round toward 0. */
		  if (result < 0)
		    ++result;
		  break;
		case 2:
		  /* Round toward positive infinity.  */
		  ++result;
		  break;
		case 3:
		  /* Round toward negative infinity.  The result is already
		     correctly rounded.  */
		  break;
		default:
		  abort ();
		  break;
		}
	    }
	  else
	    {
	      /* MSR0.RDAV controls rounding.  If set, round toward positive
		 infinity.  Otherwise the result is already rounded correctly
		 toward negative infinity.  */
	      if (GET_MSR_RDAV (msr0))
		++result;
	    }
	}
      break;
    default:
      break;
    }

  return result;
}

SI
frvbf_media_average (SIM_CPU *current_cpu, SI reg1, SI reg2)
{
  SI result;
  result  = do_media_average (current_cpu, reg1 & 0xffff, reg2 & 0xffff);
  result &= 0xffff;
  result |= do_media_average (current_cpu, (reg1 >> 16) & 0xffff,
			      (reg2 >> 16) & 0xffff) << 16;
  return result;
}

/* Maintain a flag in order to know when to write the address of the next
   VLIW instruction into the LR register.  Used by JMPL. JMPIL, and CALL.  */
void
frvbf_set_write_next_vliw_addr_to_LR (SIM_CPU *current_cpu, int value)
{
  frvbf_write_next_vliw_addr_to_LR = value;
}

void
frvbf_set_ne_index (SIM_CPU *current_cpu, int index)
{
  USI NE_flags[2];

  /* Save the target register so interrupt processing can set its NE flag
     in the event of an exception.  */
  frv_interrupt_state.ne_index = index;

  /* Clear the NE flag of the target register. It will be reset if necessary
     in the event of an exception.  */
  GET_NE_FLAGS (NE_flags, H_SPR_FNER0);
  CLEAR_NE_FLAG (NE_flags, index);
  SET_NE_FLAGS (H_SPR_FNER0, NE_flags);
}

void
frvbf_force_update (SIM_CPU *current_cpu)
{
  CGEN_WRITE_QUEUE *q = CPU_WRITE_QUEUE (current_cpu);
  int ix = CGEN_WRITE_QUEUE_INDEX (q);
  if (ix > 0)
    {
      CGEN_WRITE_QUEUE_ELEMENT *item = CGEN_WRITE_QUEUE_ELEMENT (q, ix - 1);
      item->flags |= FRV_WRITE_QUEUE_FORCE_WRITE;
    }
}

/* Condition code logic.  */
enum cr_ops {
  andcr, orcr, xorcr, nandcr, norcr, andncr, orncr, nandncr, norncr,
  num_cr_ops
};

enum cr_result {cr_undefined, cr_undefined1, cr_false, cr_true};

static enum cr_result
cr_logic[num_cr_ops][4][4] = {
  /* andcr */
  {
    /*                undefined     undefined       false         true */
    /* undefined */ {cr_undefined, cr_undefined, cr_undefined, cr_undefined},
    /* undefined */ {cr_undefined, cr_undefined, cr_undefined, cr_undefined},
    /* false     */ {cr_undefined, cr_undefined, cr_undefined, cr_undefined},
    /* true      */ {cr_undefined, cr_undefined, cr_false,     cr_true     }
  },
  /* orcr */
  {
    /*                undefined     undefined       false         true */
    /* undefined */ {cr_undefined, cr_undefined, cr_false,     cr_true     },
    /* undefined */ {cr_undefined, cr_undefined, cr_false,     cr_true     },
    /* false     */ {cr_false,     cr_false,     cr_false,     cr_true     },
    /* true      */ {cr_true,      cr_true,      cr_true,      cr_true     }
  },
  /* xorcr */
  {
    /*                undefined     undefined       false         true */
    /* undefined */ {cr_undefined, cr_undefined, cr_undefined, cr_undefined},
    /* undefined */ {cr_undefined, cr_undefined, cr_undefined, cr_undefined},
    /* false     */ {cr_undefined, cr_undefined, cr_false,     cr_true     },
    /* true      */ {cr_true,      cr_true,      cr_true,      cr_false    }
  },
  /* nandcr */
  {
    /*                undefined     undefined       false         true */
    /* undefined */ {cr_undefined, cr_undefined, cr_undefined, cr_undefined},
    /* undefined */ {cr_undefined, cr_undefined, cr_undefined, cr_undefined},
    /* false     */ {cr_undefined, cr_undefined, cr_undefined, cr_undefined},
    /* true      */ {cr_undefined, cr_undefined, cr_true,      cr_false    }
  },
  /* norcr */
  {
    /*                undefined     undefined       false         true */
    /* undefined */ {cr_undefined, cr_undefined, cr_true,      cr_false    },
    /* undefined */ {cr_undefined, cr_undefined, cr_true,      cr_false    },
    /* false     */ {cr_true,      cr_true,      cr_true,      cr_false    },
    /* true      */ {cr_false,     cr_false,     cr_false,     cr_false    }
  },
  /* andncr */
  {
    /*                undefined     undefined       false         true */
    /* undefined */ {cr_undefined, cr_undefined, cr_undefined, cr_undefined},
    /* undefined */ {cr_undefined, cr_undefined, cr_undefined, cr_undefined},
    /* false     */ {cr_undefined, cr_undefined, cr_false,     cr_true     },
    /* true      */ {cr_undefined, cr_undefined, cr_undefined, cr_undefined}
  },
  /* orncr */
  {
    /*                undefined     undefined       false         true */
    /* undefined */ {cr_undefined, cr_undefined, cr_false,     cr_true     },
    /* undefined */ {cr_undefined, cr_undefined, cr_false,     cr_true     },
    /* false     */ {cr_true,      cr_true,      cr_true,      cr_true     },
    /* true      */ {cr_false,     cr_false,     cr_false,     cr_true     }
  },
  /* nandncr */
  {
    /*                undefined     undefined       false         true */
    /* undefined */ {cr_undefined, cr_undefined, cr_undefined, cr_undefined},
    /* undefined */ {cr_undefined, cr_undefined, cr_undefined, cr_undefined},
    /* false     */ {cr_undefined, cr_undefined, cr_true,      cr_false    },
    /* true      */ {cr_undefined, cr_undefined, cr_undefined, cr_undefined}
  },
  /* norncr */
  {
    /*                undefined     undefined       false         true */
    /* undefined */ {cr_undefined, cr_undefined, cr_true,      cr_false    },
    /* undefined */ {cr_undefined, cr_undefined, cr_true,      cr_false    },
    /* false     */ {cr_false,     cr_false,     cr_false,     cr_false    },
    /* true      */ {cr_true,      cr_true,      cr_true,      cr_false    }
  }
};

UQI
frvbf_cr_logic (SIM_CPU *current_cpu, SI operation, UQI arg1, UQI arg2)
{
  return cr_logic[operation][arg1][arg2];
}

/* Cache Manipulation.  */
void
frvbf_insn_cache_preload (SIM_CPU *current_cpu, SI address, USI length, int lock)
{
  /* If we need to count cycles, then the cache operation will be
     initiated from the model profiling functions.
     See frvbf_model_....  */
  int hsr0 = GET_HSR0 ();
  if (GET_HSR0_ICE (hsr0))
    {
      if (model_insn)
	{
	  CPU_LOAD_ADDRESS (current_cpu) = address;
	  CPU_LOAD_LENGTH (current_cpu) = length;
	  CPU_LOAD_LOCK (current_cpu) = lock;
	}
      else
	{
	  FRV_CACHE *cache = CPU_INSN_CACHE (current_cpu);
	  frv_cache_preload (cache, address, length, lock);
	}
    }
}

void
frvbf_data_cache_preload (SIM_CPU *current_cpu, SI address, USI length, int lock)
{
  /* If we need to count cycles, then the cache operation will be
     initiated from the model profiling functions.
     See frvbf_model_....  */
  int hsr0 = GET_HSR0 ();
  if (GET_HSR0_DCE (hsr0))
    {
      if (model_insn)
	{
	  CPU_LOAD_ADDRESS (current_cpu) = address;
	  CPU_LOAD_LENGTH (current_cpu) = length;
	  CPU_LOAD_LOCK (current_cpu) = lock;
	}
      else
	{
	  FRV_CACHE *cache = CPU_DATA_CACHE (current_cpu);
	  frv_cache_preload (cache, address, length, lock);
	}
    }
}

void
frvbf_insn_cache_unlock (SIM_CPU *current_cpu, SI address)
{
  /* If we need to count cycles, then the cache operation will be
     initiated from the model profiling functions.
     See frvbf_model_....  */
  int hsr0 = GET_HSR0 ();
  if (GET_HSR0_ICE (hsr0))
    {
      if (model_insn)
	CPU_LOAD_ADDRESS (current_cpu) = address;
      else
	{
	  FRV_CACHE *cache = CPU_INSN_CACHE (current_cpu);
	  frv_cache_unlock (cache, address);
	}
    }
}

void
frvbf_data_cache_unlock (SIM_CPU *current_cpu, SI address)
{
  /* If we need to count cycles, then the cache operation will be
     initiated from the model profiling functions.
     See frvbf_model_....  */
  int hsr0 = GET_HSR0 ();
  if (GET_HSR0_DCE (hsr0))
    {
      if (model_insn)
	CPU_LOAD_ADDRESS (current_cpu) = address;
      else
	{
	  FRV_CACHE *cache = CPU_DATA_CACHE (current_cpu);
	  frv_cache_unlock (cache, address);
	}
    }
}

void
frvbf_insn_cache_invalidate (SIM_CPU *current_cpu, SI address, int all)
{
  /* Make sure the insn was specified properly.  -1 will be passed for ALL
     for a icei with A=0.  */
  if (all == -1)
    {
      frv_queue_program_interrupt (current_cpu, FRV_ILLEGAL_INSTRUCTION);
      return;
    }

  /* If we need to count cycles, then the cache operation will be
     initiated from the model profiling functions.
     See frvbf_model_....  */
  if (model_insn)
    {
      /* Record the all-entries flag for use in profiling.  */
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (current_cpu);
      ps->all_cache_entries = all;
      CPU_LOAD_ADDRESS (current_cpu) = address;
    }
  else
    {
      FRV_CACHE *cache = CPU_INSN_CACHE (current_cpu);
      if (all)
	frv_cache_invalidate_all (cache, 0/* flush? */);
      else
	frv_cache_invalidate (cache, address, 0/* flush? */);
    }
}

void
frvbf_data_cache_invalidate (SIM_CPU *current_cpu, SI address, int all)
{
  /* Make sure the insn was specified properly.  -1 will be passed for ALL
     for a dcei with A=0.  */
  if (all == -1)
    {
      frv_queue_program_interrupt (current_cpu, FRV_ILLEGAL_INSTRUCTION);
      return;
    }

  /* If we need to count cycles, then the cache operation will be
     initiated from the model profiling functions.
     See frvbf_model_....  */
  if (model_insn)
    {
      /* Record the all-entries flag for use in profiling.  */
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (current_cpu);
      ps->all_cache_entries = all;
      CPU_LOAD_ADDRESS (current_cpu) = address;
    }
  else
    {
      FRV_CACHE *cache = CPU_DATA_CACHE (current_cpu);
      if (all)
	frv_cache_invalidate_all (cache, 0/* flush? */);
      else
	frv_cache_invalidate (cache, address, 0/* flush? */);
    }
}

void
frvbf_data_cache_flush (SIM_CPU *current_cpu, SI address, int all)
{
  /* Make sure the insn was specified properly.  -1 will be passed for ALL
     for a dcef with A=0.  */
  if (all == -1)
    {
      frv_queue_program_interrupt (current_cpu, FRV_ILLEGAL_INSTRUCTION);
      return;
    }

  /* If we need to count cycles, then the cache operation will be
     initiated from the model profiling functions.
     See frvbf_model_....  */
  if (model_insn)
    {
      /* Record the all-entries flag for use in profiling.  */
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (current_cpu);
      ps->all_cache_entries = all;
      CPU_LOAD_ADDRESS (current_cpu) = address;
    }
  else
    {
      FRV_CACHE *cache = CPU_DATA_CACHE (current_cpu);
      if (all)
	frv_cache_invalidate_all (cache, 1/* flush? */);
      else
	frv_cache_invalidate (cache, address, 1/* flush? */);
    }
}
