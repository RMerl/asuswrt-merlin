/* frv simulator fr500 dependent profiling code.

   Copyright (C) 1998, 1999, 2000, 2001, 2003, 2007
   Free Software Foundation, Inc.
   Contributed by Red Hat

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
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/
#define WANT_CPU
#define WANT_CPU_FRVBF

#include "sim-main.h"
#include "bfd.h"

#if WITH_PROFILE_MODEL_P

#include "profile.h"
#include "profile-fr500.h"

/* Initialize cycle counting for an insn.
   FIRST_P is non-zero if this is the first insn in a set of parallel
   insns.  */
void
fr500_model_insn_before (SIM_CPU *cpu, int first_p)
{
  if (first_p)
    {
      MODEL_FR500_DATA *d = CPU_MODEL_DATA (cpu);
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      ps->cur_gr_complex = ps->prev_gr_complex;
      d->cur_fpop     = d->prev_fpop;
      d->cur_media    = d->prev_media;
      d->cur_cc_complex = d->prev_cc_complex;
    }
}

/* Record the cycles computed for an insn.
   LAST_P is non-zero if this is the last insn in a set of parallel insns,
   and we update the total cycle count.
   CYCLES is the cycle count of the insn.  */
void
fr500_model_insn_after (SIM_CPU *cpu, int last_p, int cycles)
{
  if (last_p)
    {
      MODEL_FR500_DATA *d = CPU_MODEL_DATA (cpu);
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      ps->prev_gr_complex = ps->cur_gr_complex;
      d->prev_fpop     = d->cur_fpop;
      d->prev_media    = d->cur_media;
      d->prev_cc_complex = d->cur_cc_complex;
    }
}

static void
set_use_is_fpop (SIM_CPU *cpu, INT fr)
{
  MODEL_FR500_DATA *d = CPU_MODEL_DATA (cpu);
  fr500_reset_fr_flags (cpu, (fr));
  d->cur_fpop |=  (((DI)1) << (fr));
}

static void
set_use_not_fpop (SIM_CPU *cpu, INT fr)
{
  MODEL_FR500_DATA *d = CPU_MODEL_DATA (cpu);
  d->cur_fpop &= ~(((DI)1) << (fr));
}

static int
use_is_fpop (SIM_CPU *cpu, INT fr)
{
  MODEL_FR500_DATA *d = CPU_MODEL_DATA (cpu);
  return d->prev_fpop & (((DI)1) << (fr));
}

static void
set_use_is_media ( SIM_CPU *cpu, INT fr)
{
  MODEL_FR500_DATA *d = CPU_MODEL_DATA (cpu);
  fr500_reset_fr_flags (cpu, (fr));
  d->cur_media |=  (((DI)1) << (fr));
}

static void
set_use_not_media (SIM_CPU *cpu, INT fr)
{
  MODEL_FR500_DATA *d = CPU_MODEL_DATA (cpu);
  d->cur_media &= ~(((DI)1) << (fr));
}

static int
use_is_media (SIM_CPU *cpu, INT fr)
{
  MODEL_FR500_DATA *d = CPU_MODEL_DATA (cpu);
  return d->prev_media & (((DI)1) << (fr));
}

static void
set_use_is_cc_complex (SIM_CPU *cpu, INT cc)
{
  MODEL_FR500_DATA *d = CPU_MODEL_DATA (cpu);
  fr500_reset_cc_flags (cpu, cc);
  d->cur_cc_complex |= (((DI)1) << (cc));
}

static void
set_use_not_cc_complex (SIM_CPU *cpu, INT cc)
{
  MODEL_FR500_DATA *d = CPU_MODEL_DATA (cpu);
  d->cur_cc_complex &= ~(((DI)1) << (cc));
}

static int
use_is_cc_complex (SIM_CPU *cpu, INT cc)
{
  MODEL_FR500_DATA *d = CPU_MODEL_DATA (cpu);
  return d->prev_cc_complex &   (((DI)1) << (cc));
}

void
fr500_reset_fr_flags (SIM_CPU *cpu, INT fr)
{
  set_use_not_fpop (cpu, fr);
  set_use_not_media (cpu, fr);
}

void
fr500_reset_cc_flags (SIM_CPU *cpu, INT cc)
{
  set_use_not_cc_complex (cpu, cc);
}

/* Latency of floating point registers may be less than recorded when followed
   by another floating point insn.  */
static void
adjust_float_register_busy (SIM_CPU *cpu, INT in_FRi, INT in_FRj, INT out_FRk,
			    int cycles)
{
  /* If the registers were previously used in a floating point op,
     then their latency will be less than previously recorded.
     See Table 13-13 in the LSI.  */
  if (in_FRi >= 0)
    if (use_is_fpop (cpu, in_FRi))
      decrease_FR_busy (cpu, in_FRi, cycles);
    else
      enforce_full_fr_latency (cpu, in_FRi);
  
  if (in_FRj >= 0 && in_FRj != in_FRi)
    if (use_is_fpop (cpu, in_FRj))
      decrease_FR_busy (cpu, in_FRj, cycles);
    else
      enforce_full_fr_latency (cpu, in_FRj);

  if (out_FRk >= 0 && out_FRk != in_FRi && out_FRk != in_FRj)
    if (use_is_fpop (cpu, out_FRk))
      decrease_FR_busy (cpu, out_FRk, cycles);
    else
      enforce_full_fr_latency (cpu, out_FRk);
}

/* Latency of floating point registers may be less than recorded when followed
   by another floating point insn.  */
static void
adjust_double_register_busy (SIM_CPU *cpu, INT in_FRi, INT in_FRj, INT out_FRk,
			    int cycles)
{
  /* If the registers were previously used in a floating point op,
     then their latency will be less than previously recorded.
     See Table 13-13 in the LSI.  */
  adjust_float_register_busy (cpu, in_FRi, in_FRj, out_FRk, cycles);
  if (in_FRi >= 0)  ++in_FRi;
  if (in_FRj >= 0)  ++in_FRj;
  if (out_FRk >= 0) ++out_FRk;
  adjust_float_register_busy (cpu, in_FRi, in_FRj, out_FRk, cycles);
}

/* Latency of floating point registers is less than recorded when followed
   by another floating point insn.  */
static void
restore_float_register_busy (SIM_CPU *cpu, INT in_FRi, INT in_FRj, INT out_FRk,
			     int cycles)
{
  /* If the registers were previously used in a floating point op,
     then their latency will be less than previously recorded.
     See Table 13-13 in the LSI.  */
  if (in_FRi >= 0 && use_is_fpop (cpu, in_FRi))
    increase_FR_busy (cpu, in_FRi, cycles);
  if (in_FRj != in_FRi && use_is_fpop (cpu, in_FRj))
    increase_FR_busy (cpu, in_FRj, cycles);
  if (out_FRk != in_FRi && out_FRk != in_FRj && use_is_fpop (cpu, out_FRk))
    increase_FR_busy (cpu, out_FRk, cycles);
}

/* Latency of floating point registers is less than recorded when followed
   by another floating point insn.  */
static void
restore_double_register_busy (SIM_CPU *cpu, INT in_FRi, INT in_FRj, INT out_FRk,
			    int cycles)
{
  /* If the registers were previously used in a floating point op,
     then their latency will be less than previously recorded.
     See Table 13-13 in the LSI.  */
  restore_float_register_busy (cpu, in_FRi, in_FRj, out_FRk, cycles);
  if (in_FRi >= 0)  ++in_FRi;
  if (in_FRj >= 0)  ++in_FRj;
  if (out_FRk >= 0) ++out_FRk;
  restore_float_register_busy (cpu, in_FRi, in_FRj, out_FRk, cycles);
}

int
frvbf_model_fr500_u_exec (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced)
{
  return idesc->timing->units[unit_num].done;
}

int
frvbf_model_fr500_u_integer (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_GRi, INT in_GRj, INT out_GRk,
			     INT out_ICCi_1)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* icc0-icc4 are the upper 4 fields of the CCR.  */
      if (out_ICCi_1 >= 0)
	out_ICCi_1 += 4;

      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRi != out_GRk && in_GRi >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRi))
	    decrease_GR_busy (cpu, in_GRi, 1);
	}
      if (in_GRj != out_GRk && in_GRj != in_GRi && in_GRj >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      vliw_wait_for_GR (cpu, in_GRi);
      vliw_wait_for_GR (cpu, in_GRj);
      vliw_wait_for_GR (cpu, out_GRk);
      vliw_wait_for_CCR (cpu, out_ICCi_1);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRi);
      load_wait_for_GR (cpu, in_GRj);
      load_wait_for_GR (cpu, out_GRk);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  /* GRk is available immediately to the next VLIW insn as is ICCi_1.  */
  cycles = idesc->timing->units[unit_num].done;
  return cycles;
}

int
frvbf_model_fr500_u_imul (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj, INT out_GRk, INT out_ICCi_1)
{
  int cycles;
  /* icc0-icc4 are the upper 4 fields of the CCR.  */
  if (out_ICCi_1 >= 0)
    out_ICCi_1 += 4;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRi != out_GRk && in_GRi >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRi))
	    decrease_GR_busy (cpu, in_GRi, 1);
	}
      if (in_GRj != out_GRk && in_GRj != in_GRi && in_GRj >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      vliw_wait_for_GR (cpu, in_GRi);
      vliw_wait_for_GR (cpu, in_GRj);
      vliw_wait_for_GRdouble (cpu, out_GRk);
      vliw_wait_for_CCR (cpu, out_ICCi_1);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRi);
      load_wait_for_GR (cpu, in_GRj);
      load_wait_for_GRdouble (cpu, out_GRk);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  /* GRk has a latency of 2 cycles.  */
  cycles = idesc->timing->units[unit_num].done;
  update_GRdouble_latency (cpu, out_GRk, cycles + 2);
  set_use_is_gr_complex (cpu, out_GRk);
  set_use_is_gr_complex (cpu, out_GRk + 1);

  /* ICCi_1 has a latency of 1 cycle.  */
  update_CCR_latency (cpu, out_ICCi_1, cycles + 1);

  return cycles;
}

int
frvbf_model_fr500_u_idiv (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj, INT out_GRk, INT out_ICCi_1)
{
  int cycles;
  FRV_VLIW *vliw;
  int slot;

  /* icc0-icc4 are the upper 4 fields of the CCR.  */
  if (out_ICCi_1 >= 0)
    out_ICCi_1 += 4;

  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_I0;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRi != out_GRk && in_GRi >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRi))
	    decrease_GR_busy (cpu, in_GRi, 1);
	}
      if (in_GRj != out_GRk && in_GRj != in_GRi && in_GRj >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      vliw_wait_for_GR (cpu, in_GRi);
      vliw_wait_for_GR (cpu, in_GRj);
      vliw_wait_for_GR (cpu, out_GRk);
      vliw_wait_for_CCR (cpu, out_ICCi_1);
      vliw_wait_for_idiv_resource (cpu, slot);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRi);
      load_wait_for_GR (cpu, in_GRj);
      load_wait_for_GR (cpu, out_GRk);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  /* GRk has a latency of 19 cycles!  */
  cycles = idesc->timing->units[unit_num].done;
  update_GR_latency (cpu, out_GRk, cycles + 19);
  set_use_is_gr_complex (cpu, out_GRk);

  /* ICCi_1 has a latency of 19 cycles.  */
  update_CCR_latency (cpu, out_ICCi_1, cycles + 19);
  set_use_is_cc_complex (cpu, out_ICCi_1);

  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      /* GNER has a latency of 18 cycles.  */
      update_SPR_latency (cpu, GNER_FOR_GR (out_GRk), cycles + 18);
    }

  /* the idiv resource has a latency of 18 cycles!  */
  update_idiv_resource_latency (cpu, slot, cycles + 18);

  return cycles;
}

int
frvbf_model_fr500_u_branch (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced,
			    INT in_GRi, INT in_GRj,
			    INT in_ICCi_2, INT in_FCCi_2)
{
  int cycles;
  FRV_PROFILE_STATE *ps;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* icc0-icc4 are the upper 4 fields of the CCR.  */
      if (in_ICCi_2 >= 0)
	in_ICCi_2 += 4;

      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRi >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRi))
	    decrease_GR_busy (cpu, in_GRi, 1);
	}
      if (in_GRj != in_GRi && in_GRj >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      vliw_wait_for_GR (cpu, in_GRi);
      vliw_wait_for_GR (cpu, in_GRj);
      vliw_wait_for_CCR (cpu, in_ICCi_2);
      vliw_wait_for_CCR (cpu, in_FCCi_2);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRi);
      load_wait_for_GR (cpu, in_GRj);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  /* When counting branches taken or not taken, don't consider branches after
     the first taken branch in a vliw insn.  */
  ps = CPU_PROFILE_STATE (cpu);
  if (! ps->vliw_branch_taken)
    {
      /* (1 << 4): The pc is the 5th element in inputs, outputs.
	 ??? can be cleaned up */
      PROFILE_DATA *p = CPU_PROFILE_DATA (cpu);
      int taken = (referenced & (1 << 4)) != 0;
      if (taken)
	{
	  ++PROFILE_MODEL_TAKEN_COUNT (p);
	  ps->vliw_branch_taken = 1;
	}
      else
	++PROFILE_MODEL_UNTAKEN_COUNT (p);
    }

  cycles = idesc->timing->units[unit_num].done;
  return cycles;
}

int
frvbf_model_fr500_u_trap (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj,
			  INT in_ICCi_2, INT in_FCCi_2)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* icc0-icc4 are the upper 4 fields of the CCR.  */
      if (in_ICCi_2 >= 0)
	in_ICCi_2 += 4;

      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRi >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRi))
	    decrease_GR_busy (cpu, in_GRi, 1);
	}
      if (in_GRj != in_GRi && in_GRj >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      vliw_wait_for_GR (cpu, in_GRi);
      vliw_wait_for_GR (cpu, in_GRj);
      vliw_wait_for_CCR (cpu, in_ICCi_2);
      vliw_wait_for_CCR (cpu, in_FCCi_2);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRi);
      load_wait_for_GR (cpu, in_GRj);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;
  return cycles;
}

int
frvbf_model_fr500_u_check (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_ICCi_3, INT in_FCCi_3)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* icc0-icc4 are the upper 4 fields of the CCR.  */
      if (in_ICCi_3 >= 0)
	in_ICCi_3 += 4;

      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.  */
      vliw_wait_for_CCR (cpu, in_ICCi_3);
      vliw_wait_for_CCR (cpu, in_FCCi_3);
      handle_resource_wait (cpu);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;
  return cycles;
}

int
frvbf_model_fr500_u_clrgr (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_GRk)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* Wait for both GNER registers or just the one specified.  */
      if (in_GRk == -1)
	{
	  vliw_wait_for_SPR (cpu, H_SPR_GNER0);
	  vliw_wait_for_SPR (cpu, H_SPR_GNER1);
	}
      else
	vliw_wait_for_SPR (cpu, GNER_FOR_GR (in_GRk));
      handle_resource_wait (cpu);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;
  return cycles;
}

int
frvbf_model_fr500_u_clrfr (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_FRk)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* Wait for both GNER registers or just the one specified.  */
      if (in_FRk == -1)
	{
	  vliw_wait_for_SPR (cpu, H_SPR_FNER0);
	  vliw_wait_for_SPR (cpu, H_SPR_FNER1);
	}
      else
	vliw_wait_for_SPR (cpu, FNER_FOR_FR (in_FRk));
      handle_resource_wait (cpu);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;
  return cycles;
}

int
frvbf_model_fr500_u_commit (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced,
			    INT in_GRk, INT in_FRk)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* If GR is specified, then FR is not and vice-versa. If neither is
	 then it's a commitga or commitfa. Check the insn attribute to
	 figure out which.  */
      if (in_GRk != -1)
	vliw_wait_for_SPR (cpu, GNER_FOR_GR (in_GRk));
      else if (in_FRk != -1)
	vliw_wait_for_SPR (cpu, FNER_FOR_FR (in_FRk));
      else if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_FR_ACCESS))
	{
	  vliw_wait_for_SPR (cpu, H_SPR_FNER0);
	  vliw_wait_for_SPR (cpu, H_SPR_FNER1);
	}
      else
	{
	  vliw_wait_for_SPR (cpu, H_SPR_GNER0);
	  vliw_wait_for_SPR (cpu, H_SPR_GNER1);
	}
      handle_resource_wait (cpu);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;
  return cycles;
}

int
frvbf_model_fr500_u_set_hilo (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT out_GRkhi, INT out_GRklo)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a GR
	 which is not ready yet.  */
      vliw_wait_for_GR (cpu, out_GRkhi);
      vliw_wait_for_GR (cpu, out_GRklo);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, out_GRkhi);
      load_wait_for_GR (cpu, out_GRklo);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  /* GRk is available immediately to the next VLIW insn.  */
  cycles = idesc->timing->units[unit_num].done;

  set_use_not_gr_complex (cpu, out_GRkhi);
  set_use_not_gr_complex (cpu, out_GRklo);

  return cycles;
}

int
frvbf_model_fr500_u_gr_load (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_GRi, INT in_GRj,
			     INT out_GRk, INT out_GRdoublek)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRi != out_GRk && in_GRi != out_GRdoublek
	  && in_GRi != out_GRdoublek + 1 && in_GRi >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRi))
	    decrease_GR_busy (cpu, in_GRi, 1);
	}
      if (in_GRj != in_GRi && in_GRj != out_GRk && in_GRj != out_GRdoublek
	  && in_GRj != out_GRdoublek + 1 && in_GRj >= 0)

	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      vliw_wait_for_GR (cpu, in_GRi);
      vliw_wait_for_GR (cpu, in_GRj);
      vliw_wait_for_GR (cpu, out_GRk);
      vliw_wait_for_GRdouble (cpu, out_GRdoublek);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRi);
      load_wait_for_GR (cpu, in_GRj);
      load_wait_for_GR (cpu, out_GRk);
      load_wait_for_GRdouble (cpu, out_GRdoublek);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;

  /* The latency of GRk for a load will depend on how long it takes to retrieve
     the the data from the cache or memory.  */
  update_GR_latency_for_load (cpu, out_GRk, cycles);
  update_GRdouble_latency_for_load (cpu, out_GRdoublek, cycles);

  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      /* GNER has a latency of 2 cycles.  */
      update_SPR_latency (cpu, GNER_FOR_GR (out_GRk), cycles + 2);
      update_SPR_latency (cpu, GNER_FOR_GR (out_GRdoublek), cycles + 2);
    }

  if (out_GRk >= 0)
    set_use_is_gr_complex (cpu, out_GRk);
  if (out_GRdoublek != -1)
    {
      set_use_is_gr_complex (cpu, out_GRdoublek);
      set_use_is_gr_complex (cpu, out_GRdoublek + 1);
    }

  return cycles;
}

int
frvbf_model_fr500_u_gr_store (SIM_CPU *cpu, const IDESC *idesc,
			      int unit_num, int referenced,
			      INT in_GRi, INT in_GRj,
			      INT in_GRk, INT in_GRdoublek)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRi >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRi))
	    decrease_GR_busy (cpu, in_GRi, 1);
	}
      if (in_GRj != in_GRi && in_GRj >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      if (in_GRk != in_GRi && in_GRk != in_GRj && in_GRk >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRk))
	    decrease_GR_busy (cpu, in_GRk, 1);
	}
      if (in_GRdoublek != in_GRi && in_GRdoublek != in_GRj
          && in_GRdoublek + 1 != in_GRi && in_GRdoublek + 1 != in_GRj
	  && in_GRdoublek >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRdoublek))
	    decrease_GR_busy (cpu, in_GRdoublek, 1);
	  if (use_is_gr_complex (cpu, in_GRdoublek + 1))
	    decrease_GR_busy (cpu, in_GRdoublek + 1, 1);
	}
      vliw_wait_for_GR (cpu, in_GRi);
      vliw_wait_for_GR (cpu, in_GRj);
      vliw_wait_for_GR (cpu, in_GRk);
      vliw_wait_for_GRdouble (cpu, in_GRdoublek);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRi);
      load_wait_for_GR (cpu, in_GRj);
      load_wait_for_GR (cpu, in_GRk);
      load_wait_for_GRdouble (cpu, in_GRdoublek);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;

  return cycles;
}

int
frvbf_model_fr500_u_gr_r_store (SIM_CPU *cpu, const IDESC *idesc,
				int unit_num, int referenced,
				INT in_GRi, INT in_GRj,
				INT in_GRk, INT in_GRdoublek)
{
  int cycles = frvbf_model_fr500_u_gr_store (cpu, idesc, unit_num, referenced,
					     in_GRi, in_GRj, in_GRk,
					     in_GRdoublek);

  if (model_insn == FRV_INSN_MODEL_PASS_2)
    {
      if (CPU_RSTR_INVALIDATE(cpu))
	request_cache_invalidate (cpu, CPU_DATA_CACHE (cpu), cycles);
    }

  return cycles;
}

int
frvbf_model_fr500_u_fr_load (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_GRi, INT in_GRj,
			     INT out_FRk, INT out_FRdoublek)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRi >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRi))
	    decrease_GR_busy (cpu, in_GRi, 1);
	}
      if (in_GRj != in_GRi && in_GRj >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      if (out_FRk >= 0)
	{
	  if (use_is_media (cpu, out_FRk))
	    decrease_FR_busy (cpu, out_FRk, 1);
	  else
	    adjust_float_register_busy (cpu, -1, -1, out_FRk, 1);
	}
      if (out_FRdoublek >= 0)
	{
	  if (use_is_media (cpu, out_FRdoublek))
	    decrease_FR_busy (cpu, out_FRdoublek, 1);
	  else
	    adjust_float_register_busy (cpu, -1, -1, out_FRdoublek, 1);
	  if (use_is_media (cpu, out_FRdoublek + 1))
	    decrease_FR_busy (cpu, out_FRdoublek + 1, 1);
	  else
	    adjust_float_register_busy (cpu, -1, -1, out_FRdoublek + 1, 1);
	}
      vliw_wait_for_GR (cpu, in_GRi);
      vliw_wait_for_GR (cpu, in_GRj);
      vliw_wait_for_FR (cpu, out_FRk);
      vliw_wait_for_FRdouble (cpu, out_FRdoublek);
      if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
	{
	  vliw_wait_for_SPR (cpu, FNER_FOR_FR (out_FRk));
	  vliw_wait_for_SPR (cpu, FNER_FOR_FR (out_FRdoublek));
	}
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRi);
      load_wait_for_GR (cpu, in_GRj);
      load_wait_for_FR (cpu, out_FRk);
      load_wait_for_FRdouble (cpu, out_FRdoublek);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;

  /* The latency of FRk for a load will depend on how long it takes to retrieve
     the the data from the cache or memory.  */
  update_FR_latency_for_load (cpu, out_FRk, cycles);
  update_FRdouble_latency_for_load (cpu, out_FRdoublek, cycles);

  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      /* FNER has a latency of 3 cycles.  */
      update_SPR_latency (cpu, FNER_FOR_FR (out_FRk), cycles + 3);
      update_SPR_latency (cpu, FNER_FOR_FR (out_FRdoublek), cycles + 3);
    }

  fr500_reset_fr_flags (cpu, out_FRk);

  return cycles;
}

int
frvbf_model_fr500_u_fr_store (SIM_CPU *cpu, const IDESC *idesc,
			      int unit_num, int referenced,
			      INT in_GRi, INT in_GRj,
			      INT in_FRk, INT in_FRdoublek)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRi >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRi))
	    decrease_GR_busy (cpu, in_GRi, 1);
	}
      if (in_GRj != in_GRi && in_GRj >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      if (in_FRk >= 0)
	{
	  if (use_is_media (cpu, in_FRk))
	    decrease_FR_busy (cpu, in_FRk, 1);
	  else
	    adjust_float_register_busy (cpu, -1, -1, in_FRk, 1);
	}
      if (in_FRdoublek >= 0)
	{
	  if (use_is_media (cpu, in_FRdoublek))
	    decrease_FR_busy (cpu, in_FRdoublek, 1);
	  else
	    adjust_float_register_busy (cpu, -1, -1, in_FRdoublek, 1);
	  if (use_is_media (cpu, in_FRdoublek + 1))
	    decrease_FR_busy (cpu, in_FRdoublek + 1, 1);
	  else
	    adjust_float_register_busy (cpu, -1, -1, in_FRdoublek + 1, 1);
	}
      vliw_wait_for_GR (cpu, in_GRi);
      vliw_wait_for_GR (cpu, in_GRj);
      vliw_wait_for_FR (cpu, in_FRk);
      vliw_wait_for_FRdouble (cpu, in_FRdoublek);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRi);
      load_wait_for_GR (cpu, in_GRj);
      load_wait_for_FR (cpu, in_FRk);
      load_wait_for_FRdouble (cpu, in_FRdoublek);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;

  return cycles;
}

int
frvbf_model_fr500_u_fr_r_store (SIM_CPU *cpu, const IDESC *idesc,
				int unit_num, int referenced,
				INT in_GRi, INT in_GRj,
				INT in_FRk, INT in_FRdoublek)
{
  int cycles = frvbf_model_fr500_u_fr_store (cpu, idesc, unit_num, referenced,
					     in_GRi, in_GRj, in_FRk,
					     in_FRdoublek);

  if (model_insn == FRV_INSN_MODEL_PASS_2)
    {
      if (CPU_RSTR_INVALIDATE(cpu))
	request_cache_invalidate (cpu, CPU_DATA_CACHE (cpu), cycles);
    }

  return cycles;
}

int
frvbf_model_fr500_u_swap (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj, INT out_GRk)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRi != out_GRk && in_GRi >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRi))
	    decrease_GR_busy (cpu, in_GRi, 1);
	}
      if (in_GRj != out_GRk && in_GRj != in_GRi && in_GRj >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      vliw_wait_for_GR (cpu, in_GRi);
      vliw_wait_for_GR (cpu, in_GRj);
      vliw_wait_for_GR (cpu, out_GRk);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRi);
      load_wait_for_GR (cpu, in_GRj);
      load_wait_for_GR (cpu, out_GRk);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;

  /* The latency of GRk will depend on how long it takes to swap
     the the data from the cache or memory.  */
  update_GR_latency_for_swap (cpu, out_GRk, cycles);
  set_use_is_gr_complex (cpu, out_GRk);

  return cycles;
}

int
frvbf_model_fr500_u_fr2fr (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_FRj, INT out_FRk)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.  */
      if (in_FRj >= 0)
	{
	  if (use_is_media (cpu, in_FRj))
	    decrease_FR_busy (cpu, in_FRj, 1);
	  else
	    adjust_float_register_busy (cpu, -1, in_FRj, -1, 1);
	}
      if (out_FRk >= 0 && out_FRk != in_FRj)
	{
	  if (use_is_media (cpu, out_FRk))
	    decrease_FR_busy (cpu, out_FRk, 1);
	  else
	    adjust_float_register_busy (cpu, -1, -1, out_FRk, 1);
	}
      vliw_wait_for_FR (cpu, in_FRj);
      vliw_wait_for_FR (cpu, out_FRk);
      handle_resource_wait (cpu);
      load_wait_for_FR (cpu, in_FRj);
      load_wait_for_FR (cpu, out_FRk);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  /* The latency of FRj is 3 cycles.  */
  cycles = idesc->timing->units[unit_num].done;
  update_FR_latency (cpu, out_FRk, cycles + 3);

  return cycles;
}

int
frvbf_model_fr500_u_fr2gr (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_FRk, INT out_GRj)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.  */
      if (in_FRk >= 0)
	{
	  if (use_is_media (cpu, in_FRk))
	    decrease_FR_busy (cpu, in_FRk, 1);
	  else
	    adjust_float_register_busy (cpu, -1, in_FRk, -1, 1);
	}
      vliw_wait_for_FR (cpu, in_FRk);
      vliw_wait_for_GR (cpu, out_GRj);
      handle_resource_wait (cpu);
      load_wait_for_FR (cpu, in_FRk);
      load_wait_for_GR (cpu, out_GRj);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  /* The latency of GRj is 2 cycles.  */
  cycles = idesc->timing->units[unit_num].done;
  update_GR_latency (cpu, out_GRj, cycles + 2);
  set_use_is_gr_complex (cpu, out_GRj);

  return cycles;
}

int
frvbf_model_fr500_u_spr2gr (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_spr, INT out_GRj)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.  */
      vliw_wait_for_SPR (cpu, in_spr);
      vliw_wait_for_GR (cpu, out_GRj);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, out_GRj);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;

#if 0 /* no latency?  */
  /* The latency of GRj is 2 cycles.  */
  update_GR_latency (cpu, out_GRj, cycles + 2);
#endif

  return cycles;
}

int
frvbf_model_fr500_u_gr2fr (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_GRj, INT out_FRk)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRj >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      if (out_FRk >= 0)
	{
	  if (use_is_media (cpu, out_FRk))
	    decrease_FR_busy (cpu, out_FRk, 1);
	  else
	    adjust_float_register_busy (cpu, -1, -1, out_FRk, 1);
	}
      vliw_wait_for_GR (cpu, in_GRj);
      vliw_wait_for_FR (cpu, out_FRk);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRj);
      load_wait_for_FR (cpu, out_FRk);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  /* The latency of FRk is 2 cycles.  */
  cycles = idesc->timing->units[unit_num].done;
  update_FR_latency (cpu, out_FRk, cycles + 2);

  /* Mark this use of the register as NOT a floating point op.  */
  fr500_reset_fr_flags (cpu, out_FRk);

  return cycles;
}

int
frvbf_model_fr500_u_gr2spr (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced,
			    INT in_GRj, INT out_spr)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRj >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      vliw_wait_for_GR (cpu, in_GRj);
      vliw_wait_for_SPR (cpu, out_spr);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRj);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;

#if 0
  /* The latency of spr is ? cycles.  */
  update_SPR_latency (cpu, out_spr, cycles + ?);
#endif

  return cycles;
}

int
frvbf_model_fr500_u_ici (SIM_CPU *cpu, const IDESC *idesc,
			 int unit_num, int referenced,
			 INT in_GRi, INT in_GRj)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRi >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRi))
	    decrease_GR_busy (cpu, in_GRi, 1);
	}
      if (in_GRj != in_GRi && in_GRj >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      vliw_wait_for_GR (cpu, in_GRi);
      vliw_wait_for_GR (cpu, in_GRj);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRi);
      load_wait_for_GR (cpu, in_GRj);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;
  request_cache_invalidate (cpu, CPU_INSN_CACHE (cpu), cycles);
  return cycles;
}

int
frvbf_model_fr500_u_dci (SIM_CPU *cpu, const IDESC *idesc,
			 int unit_num, int referenced,
			 INT in_GRi, INT in_GRj)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRi >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRi))
	    decrease_GR_busy (cpu, in_GRi, 1);
	}
      if (in_GRj != in_GRi && in_GRj >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      vliw_wait_for_GR (cpu, in_GRi);
      vliw_wait_for_GR (cpu, in_GRj);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRi);
      load_wait_for_GR (cpu, in_GRj);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;
  request_cache_invalidate (cpu, CPU_DATA_CACHE (cpu), cycles);
  return cycles;
}

int
frvbf_model_fr500_u_dcf (SIM_CPU *cpu, const IDESC *idesc,
			 int unit_num, int referenced,
			 INT in_GRi, INT in_GRj)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRi >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRi))
	    decrease_GR_busy (cpu, in_GRi, 1);
	}
      if (in_GRj != in_GRi && in_GRj >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      vliw_wait_for_GR (cpu, in_GRi);
      vliw_wait_for_GR (cpu, in_GRj);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRi);
      load_wait_for_GR (cpu, in_GRj);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;
  request_cache_flush (cpu, CPU_DATA_CACHE (cpu), cycles);
  return cycles;
}

int
frvbf_model_fr500_u_icpl (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRi >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRi))
	    decrease_GR_busy (cpu, in_GRi, 1);
	}
      if (in_GRj != in_GRi && in_GRj >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      vliw_wait_for_GR (cpu, in_GRi);
      vliw_wait_for_GR (cpu, in_GRj);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRi);
      load_wait_for_GR (cpu, in_GRj);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;
  request_cache_preload (cpu, CPU_INSN_CACHE (cpu), cycles);
  return cycles;
}

int
frvbf_model_fr500_u_dcpl (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRi >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRi))
	    decrease_GR_busy (cpu, in_GRi, 1);
	}
      if (in_GRj != in_GRi && in_GRj >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      vliw_wait_for_GR (cpu, in_GRi);
      vliw_wait_for_GR (cpu, in_GRj);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRi);
      load_wait_for_GR (cpu, in_GRj);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;
  request_cache_preload (cpu, CPU_DATA_CACHE (cpu), cycles);
  return cycles;
}

int
frvbf_model_fr500_u_icul (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRi >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRi))
	    decrease_GR_busy (cpu, in_GRi, 1);
	}
      if (in_GRj != in_GRi && in_GRj >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      vliw_wait_for_GR (cpu, in_GRi);
      vliw_wait_for_GR (cpu, in_GRj);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRi);
      load_wait_for_GR (cpu, in_GRj);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;
  request_cache_unlock (cpu, CPU_INSN_CACHE (cpu), cycles);
  return cycles;
}

int
frvbf_model_fr500_u_dcul (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 13-8 in the LSI.  */
      if (in_GRi >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRi))
	    decrease_GR_busy (cpu, in_GRi, 1);
	}
      if (in_GRj != in_GRi && in_GRj >= 0)
	{
	  if (use_is_gr_complex (cpu, in_GRj))
	    decrease_GR_busy (cpu, in_GRj, 1);
	}
      vliw_wait_for_GR (cpu, in_GRi);
      vliw_wait_for_GR (cpu, in_GRj);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRi);
      load_wait_for_GR (cpu, in_GRj);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;
  request_cache_unlock (cpu, CPU_DATA_CACHE (cpu), cycles);
  return cycles;
}

int
frvbf_model_fr500_u_float_arith (SIM_CPU *cpu, const IDESC *idesc,
				 int unit_num, int referenced,
				 INT in_FRi, INT in_FRj,
				 INT in_FRdoublei, INT in_FRdoublej,
				 INT out_FRk, INT out_FRdoublek)
{
  int cycles;
  FRV_PROFILE_STATE *ps;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  adjust_float_register_busy (cpu, in_FRi, in_FRj, out_FRk, 1);
  adjust_double_register_busy (cpu, in_FRdoublei, in_FRdoublej, out_FRdoublek,
			       1);
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, out_FRk);
  post_wait_for_FRdouble (cpu, in_FRdoublei);
  post_wait_for_FRdouble (cpu, in_FRdoublej);
  post_wait_for_FRdouble (cpu, out_FRdoublek);
  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      post_wait_for_SPR (cpu, FNER_FOR_FR (out_FRk));
      post_wait_for_SPR (cpu, FNER_FOR_FR (out_FRdoublek));
    }
  restore_float_register_busy (cpu, in_FRi, in_FRj, out_FRk, 1);
  restore_double_register_busy (cpu, in_FRdoublei, in_FRdoublej, out_FRdoublek,
				1);

  /* The latency of FRk will be at least the latency of the other inputs.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FRdouble_latency (cpu, out_FRdoublek, ps->post_wait);

  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      update_SPR_latency (cpu, FNER_FOR_FR (out_FRk), ps->post_wait);
      update_SPR_latency (cpu, FNER_FOR_FR (out_FRdoublek), ps->post_wait);
    }

  /* Once initiated, post-processing will take 3 cycles.  */
  update_FR_ptime (cpu, out_FRk, 3);
  update_FRdouble_ptime (cpu, out_FRdoublek, 3);

  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      update_SPR_ptime (cpu, FNER_FOR_FR (out_FRk), 3);
      update_SPR_ptime (cpu, FNER_FOR_FR (out_FRdoublek), 3);
    }

  /* Mark this use of the register as a floating point op.  */
  if (out_FRk >= 0)
    set_use_is_fpop (cpu, out_FRk);
  if (out_FRdoublek >= 0)
    {
      set_use_is_fpop (cpu, out_FRdoublek);
      if (out_FRdoublek < 63)
	set_use_is_fpop (cpu, out_FRdoublek + 1);
    }

  return cycles;
}

int
frvbf_model_fr500_u_float_dual_arith (SIM_CPU *cpu, const IDESC *idesc,
				      int unit_num, int referenced,
				      INT in_FRi, INT in_FRj,
				      INT in_FRdoublei, INT in_FRdoublej,
				      INT out_FRk, INT out_FRdoublek)
{
  int cycles;
  INT dual_FRi;
  INT dual_FRj;
  INT dual_FRk;
  INT dual_FRdoublei;
  INT dual_FRdoublej;
  INT dual_FRdoublek;
  FRV_PROFILE_STATE *ps;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  dual_FRi = DUAL_REG (in_FRi);
  dual_FRj = DUAL_REG (in_FRj);
  dual_FRk = DUAL_REG (out_FRk);
  dual_FRdoublei = DUAL_DOUBLE (in_FRdoublei);
  dual_FRdoublej = DUAL_DOUBLE (in_FRdoublej);
  dual_FRdoublek = DUAL_DOUBLE (out_FRdoublek);

  adjust_float_register_busy (cpu, in_FRi, in_FRj, out_FRk, 1);
  adjust_float_register_busy (cpu, dual_FRi, dual_FRj, dual_FRk, 1);
  adjust_double_register_busy (cpu, in_FRdoublei, in_FRdoublej, out_FRdoublek,
			       1);
  adjust_double_register_busy (cpu, dual_FRdoublei, dual_FRdoublej,
			       dual_FRdoublek, 1);
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, out_FRk);
  post_wait_for_FR (cpu, dual_FRi);
  post_wait_for_FR (cpu, dual_FRj);
  post_wait_for_FR (cpu, dual_FRk);
  post_wait_for_FRdouble (cpu, in_FRdoublei);
  post_wait_for_FRdouble (cpu, in_FRdoublej);
  post_wait_for_FRdouble (cpu, out_FRdoublek);
  post_wait_for_FRdouble (cpu, dual_FRdoublei);
  post_wait_for_FRdouble (cpu, dual_FRdoublej);
  post_wait_for_FRdouble (cpu, dual_FRdoublek);
  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      post_wait_for_SPR (cpu, FNER_FOR_FR (out_FRk));
      post_wait_for_SPR (cpu, FNER_FOR_FR (dual_FRk));
      post_wait_for_SPR (cpu, FNER_FOR_FR (out_FRdoublek));
      post_wait_for_SPR (cpu, FNER_FOR_FR (dual_FRdoublek));
    }
  restore_float_register_busy (cpu, in_FRi, in_FRj, out_FRk, 1);
  restore_float_register_busy (cpu, dual_FRi, dual_FRj, dual_FRk, 1);
  restore_double_register_busy (cpu, in_FRdoublei, in_FRdoublej, out_FRdoublek,
				1);
  restore_double_register_busy (cpu, dual_FRdoublei, dual_FRdoublej,
				dual_FRdoublek, 1);

  /* The latency of FRk will be at least the latency of the other inputs.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FR_latency (cpu, dual_FRk, ps->post_wait);
  update_FRdouble_latency (cpu, out_FRdoublek, ps->post_wait);
  update_FRdouble_latency (cpu, dual_FRdoublek, ps->post_wait);

  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      update_SPR_latency (cpu, FNER_FOR_FR (out_FRk), ps->post_wait);
      update_SPR_latency (cpu, FNER_FOR_FR (dual_FRk), ps->post_wait);
      update_SPR_latency (cpu, FNER_FOR_FR (out_FRdoublek), ps->post_wait);
      update_SPR_latency (cpu, FNER_FOR_FR (dual_FRdoublek), ps->post_wait);
    }

  /* Once initiated, post-processing will take 3 cycles.  */
  update_FR_ptime (cpu, out_FRk, 3);
  update_FR_ptime (cpu, dual_FRk, 3);
  update_FRdouble_ptime (cpu, out_FRdoublek, 3);
  update_FRdouble_ptime (cpu, dual_FRdoublek, 3);

  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      update_SPR_ptime (cpu, FNER_FOR_FR (out_FRk), 3);
      update_SPR_ptime (cpu, FNER_FOR_FR (dual_FRk), 3);
      update_SPR_ptime (cpu, FNER_FOR_FR (out_FRdoublek), 3);
      update_SPR_ptime (cpu, FNER_FOR_FR (dual_FRdoublek), 3);
    }

  /* Mark this use of the register as a floating point op.  */
  if (out_FRk >= 0)
    set_use_is_fpop (cpu, out_FRk);
  if (dual_FRk >= 0)
    set_use_is_fpop (cpu, dual_FRk);
  if (out_FRdoublek >= 0)
    {
      set_use_is_fpop (cpu, out_FRdoublek);
      if (out_FRdoublek < 63)
	set_use_is_fpop (cpu, out_FRdoublek + 1);
    }
  if (dual_FRdoublek >= 0)
    {
      set_use_is_fpop (cpu, dual_FRdoublek);
      if (dual_FRdoublek < 63)
	set_use_is_fpop (cpu, dual_FRdoublek + 1);
    }

  return cycles;
}

int
frvbf_model_fr500_u_float_div (SIM_CPU *cpu, const IDESC *idesc,
			       int unit_num, int referenced,
			       INT in_FRi, INT in_FRj, INT out_FRk)
{
  int cycles;
  FRV_VLIW *vliw;
  int slot;
  FRV_PROFILE_STATE *ps;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  cycles = idesc->timing->units[unit_num].done;

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  adjust_float_register_busy (cpu, in_FRi, in_FRj, out_FRk, 1);
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, out_FRk);
  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    post_wait_for_SPR (cpu, FNER_FOR_FR (out_FRk));
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_fdiv (cpu, slot);
  restore_float_register_busy (cpu, in_FRi, in_FRj, out_FRk, 1);

  /* The latency of FRk will be at least the latency of the other inputs.  */
  /* Once initiated, post-processing will take 10 cycles.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FR_ptime (cpu, out_FRk, 10);

  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      /* FNER has a latency of 10 cycles.  */
      update_SPR_latency (cpu, FNER_FOR_FR (out_FRk), ps->post_wait);
      update_SPR_ptime (cpu, FNER_FOR_FR (out_FRk), 10);
    }

  /* The latency of the fdiv unit will be at least the latency of the other
     inputs.  Once initiated, post-processing will take 9 cycles.  */
  update_fdiv_resource_latency (cpu, slot, ps->post_wait + 9);

  /* Mark this use of the register as a floating point op.  */
  set_use_is_fpop (cpu, out_FRk);

  return cycles;
}

int
frvbf_model_fr500_u_float_sqrt (SIM_CPU *cpu, const IDESC *idesc,
				int unit_num, int referenced,
				INT in_FRj, INT in_FRdoublej,
				INT out_FRk, INT out_FRdoublek)
{
  int cycles;
  FRV_VLIW *vliw;
  int slot;
  FRV_PROFILE_STATE *ps;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  cycles = idesc->timing->units[unit_num].done;

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  adjust_float_register_busy (cpu, -1, in_FRj, out_FRk, 1);
  adjust_double_register_busy (cpu, -1, in_FRdoublej, out_FRdoublek, 1);
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, out_FRk);
  post_wait_for_FRdouble (cpu, in_FRdoublej);
  post_wait_for_FRdouble (cpu, out_FRdoublek);
  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    post_wait_for_SPR (cpu, FNER_FOR_FR (out_FRk));
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_fsqrt (cpu, slot);
  restore_float_register_busy (cpu, -1, in_FRj, out_FRk, 1);
  restore_double_register_busy (cpu, -1, in_FRdoublej, out_FRdoublek, 1);

  /* The latency of FRk will be at least the latency of the other inputs.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FRdouble_latency (cpu, out_FRdoublek, ps->post_wait);
  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    update_SPR_latency (cpu, FNER_FOR_FR (out_FRk), ps->post_wait);

  /* Once initiated, post-processing will take 15 cycles.  */
  update_FR_ptime (cpu, out_FRk, 15);
  update_FRdouble_ptime (cpu, out_FRdoublek, 15);

  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    update_SPR_ptime (cpu, FNER_FOR_FR (out_FRk), 15);

  /* The latency of the sqrt unit will be the latency of the other
     inputs plus 14 cycles.  */
  update_fsqrt_resource_latency (cpu, slot, ps->post_wait + 14);

  /* Mark this use of the register as a floating point op.  */
  if (out_FRk >= 0)
    set_use_is_fpop (cpu, out_FRk);
  if (out_FRdoublek >= 0)
    {
      set_use_is_fpop (cpu, out_FRdoublek);
      if (out_FRdoublek < 63)
	set_use_is_fpop (cpu, out_FRdoublek + 1);
    }

  return cycles;
}

int
frvbf_model_fr500_u_float_dual_sqrt (SIM_CPU *cpu, const IDESC *idesc,
				     int unit_num, int referenced,
				     INT in_FRj, INT out_FRk)
{
  int cycles;
  FRV_VLIW *vliw;
  int slot;
  INT dual_FRj;
  INT dual_FRk;
  FRV_PROFILE_STATE *ps;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  cycles = idesc->timing->units[unit_num].done;

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  dual_FRj = DUAL_REG (in_FRj);
  dual_FRk = DUAL_REG (out_FRk);
  adjust_float_register_busy (cpu, -1, in_FRj, out_FRk, 1);
  adjust_float_register_busy (cpu, -1, dual_FRj, dual_FRk, 1);
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, out_FRk);
  post_wait_for_FR (cpu, dual_FRj);
  post_wait_for_FR (cpu, dual_FRk);

  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_fsqrt (cpu, slot);
  restore_float_register_busy (cpu, -1, in_FRj, out_FRk, 1);
  restore_float_register_busy (cpu, -1, dual_FRj, dual_FRk, 1);

  /* The latency of FRk will be at least the latency of the other inputs.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FR_latency (cpu, dual_FRk, ps->post_wait);

  /* Once initiated, post-processing will take 15 cycles.  */
  update_FR_ptime (cpu, out_FRk, 15);
  update_FR_ptime (cpu, dual_FRk, 15);

  /* The latency of the sqrt unit will be at least the latency of the other
     inputs.  */
  update_fsqrt_resource_latency (cpu, slot, ps->post_wait + 14);

  /* Mark this use of the register as a floating point op.  */
  if (out_FRk >= 0)
    set_use_is_fpop (cpu, out_FRk);
  if (dual_FRk >= 0)
    set_use_is_fpop (cpu, dual_FRk);

  return cycles;
}

int
frvbf_model_fr500_u_float_compare (SIM_CPU *cpu, const IDESC *idesc,
				   int unit_num, int referenced,
				   INT in_FRi, INT in_FRj,
				   INT in_FRdoublei, INT in_FRdoublej,
				   INT out_FCCi_2)
{
  int cycles;
  FRV_PROFILE_STATE *ps;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  adjust_double_register_busy (cpu, in_FRdoublei, in_FRdoublej, -1, 1);
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FRdouble (cpu, in_FRdoublei);
  post_wait_for_FRdouble (cpu, in_FRdoublej);
  post_wait_for_CCR (cpu, out_FCCi_2);
  restore_double_register_busy (cpu, in_FRdoublei, in_FRdoublej, -1, 1);

  /* The latency of FCCi_2 will be the latency of the other inputs plus 3
     cycles.  */
  update_CCR_latency (cpu, out_FCCi_2, ps->post_wait + 3);

  return cycles;
}

int
frvbf_model_fr500_u_float_dual_compare (SIM_CPU *cpu, const IDESC *idesc,
					int unit_num, int referenced,
					INT in_FRi, INT in_FRj,
					INT out_FCCi_2)
{
  int cycles;
  INT dual_FRi;
  INT dual_FRj;
  INT dual_FCCi_2;
  FRV_PROFILE_STATE *ps;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  dual_FRi = DUAL_REG (in_FRi);
  dual_FRj = DUAL_REG (in_FRj);
  dual_FCCi_2 = out_FCCi_2 + 1;
  adjust_float_register_busy (cpu, in_FRi, in_FRj, -1, 1);
  adjust_float_register_busy (cpu, dual_FRi, dual_FRj, -1, 1);
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, dual_FRi);
  post_wait_for_FR (cpu, dual_FRj);
  post_wait_for_CCR (cpu, out_FCCi_2);
  post_wait_for_CCR (cpu, dual_FCCi_2);
  restore_float_register_busy (cpu, in_FRi, in_FRj, -1, 1);
  restore_float_register_busy (cpu, dual_FRi, dual_FRj, -1, 1);

  /* The latency of FCCi_2 will be the latency of the other inputs plus 3
     cycles.  */
  update_CCR_latency (cpu, out_FCCi_2, ps->post_wait + 3);
  update_CCR_latency (cpu, dual_FCCi_2, ps->post_wait + 3);

  return cycles;
}

int
frvbf_model_fr500_u_float_convert (SIM_CPU *cpu, const IDESC *idesc,
				   int unit_num, int referenced,
				   INT in_FRj, INT in_FRintj, INT in_FRdoublej,
				   INT out_FRk, INT out_FRintk,
				   INT out_FRdoublek)
{
  int cycles;
  FRV_PROFILE_STATE *ps;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  adjust_float_register_busy (cpu, -1, in_FRj, out_FRk, 1);
  adjust_float_register_busy (cpu, -1, in_FRintj, out_FRintk, 1);
  adjust_double_register_busy (cpu, -1, in_FRdoublej, out_FRdoublek, 1);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, in_FRintj);
  post_wait_for_FRdouble (cpu, in_FRdoublej);
  post_wait_for_FR (cpu, out_FRk);
  post_wait_for_FR (cpu, out_FRintk);
  post_wait_for_FRdouble (cpu, out_FRdoublek);
  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      post_wait_for_SPR (cpu, FNER_FOR_FR (out_FRk));
      post_wait_for_SPR (cpu, FNER_FOR_FR (out_FRintk));
      post_wait_for_SPR (cpu, FNER_FOR_FR (out_FRdoublek));
    }
  restore_float_register_busy (cpu, -1, in_FRj, out_FRk, 1);
  restore_float_register_busy (cpu, -1, in_FRintj, out_FRintk, 1);
  restore_double_register_busy (cpu, -1, in_FRdoublej, out_FRdoublek, 1);

  /* The latency of FRk will be at least the latency of the other inputs.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FR_latency (cpu, out_FRintk, ps->post_wait);
  update_FRdouble_latency (cpu, out_FRdoublek, ps->post_wait);

  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      update_SPR_latency (cpu, FNER_FOR_FR (out_FRk), ps->post_wait);
      update_SPR_latency (cpu, FNER_FOR_FR (out_FRintk), ps->post_wait);
      update_SPR_latency (cpu, FNER_FOR_FR (out_FRdoublek), ps->post_wait);
    }

  /* Once initiated, post-processing will take 3 cycles.  */
  update_FR_ptime (cpu, out_FRk, 3);
  update_FR_ptime (cpu, out_FRintk, 3);
  update_FRdouble_ptime (cpu, out_FRdoublek, 3);

  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      update_SPR_ptime (cpu, FNER_FOR_FR (out_FRk), 3);
      update_SPR_ptime (cpu, FNER_FOR_FR (out_FRintk), 3);
      update_SPR_ptime (cpu, FNER_FOR_FR (out_FRdoublek), 3);
    }

  /* Mark this use of the register as a floating point op.  */
  if (out_FRk >= 0)
    set_use_is_fpop (cpu, out_FRk);
  if (out_FRintk >= 0)
    set_use_is_fpop (cpu, out_FRintk);
  if (out_FRdoublek >= 0)
    {
      set_use_is_fpop (cpu, out_FRdoublek);
      set_use_is_fpop (cpu, out_FRdoublek + 1);
    }

  return cycles;
}

int
frvbf_model_fr500_u_float_dual_convert (SIM_CPU *cpu, const IDESC *idesc,
					int unit_num, int referenced,
					INT in_FRj, INT in_FRintj,
					INT out_FRk, INT out_FRintk)
{
  int cycles;
  INT dual_FRj;
  INT dual_FRintj;
  INT dual_FRk;
  INT dual_FRintk;
  FRV_PROFILE_STATE *ps;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  dual_FRj = DUAL_REG (in_FRj);
  dual_FRintj = DUAL_REG (in_FRintj);
  dual_FRk = DUAL_REG (out_FRk);
  dual_FRintk = DUAL_REG (out_FRintk);
  adjust_float_register_busy (cpu, -1, in_FRj, out_FRk, 1);
  adjust_float_register_busy (cpu, -1, dual_FRj, dual_FRk, 1);
  adjust_float_register_busy (cpu, -1, in_FRintj, out_FRintk, 1);
  adjust_float_register_busy (cpu, -1, dual_FRintj, dual_FRintk, 1);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, in_FRintj);
  post_wait_for_FR (cpu, out_FRk);
  post_wait_for_FR (cpu, out_FRintk);
  post_wait_for_FR (cpu, dual_FRj);
  post_wait_for_FR (cpu, dual_FRintj);
  post_wait_for_FR (cpu, dual_FRk);
  post_wait_for_FR (cpu, dual_FRintk);
  restore_float_register_busy (cpu, -1, in_FRj, out_FRk, 1);
  restore_float_register_busy (cpu, -1, dual_FRj, dual_FRk, 1);
  restore_float_register_busy (cpu, -1, in_FRintj, out_FRintk, 1);
  restore_float_register_busy (cpu, -1, dual_FRintj, dual_FRintk, 1);

  /* The latency of FRk will be at least the latency of the other inputs.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FR_latency (cpu, out_FRintk, ps->post_wait);
  update_FR_latency (cpu, dual_FRk, ps->post_wait);
  update_FR_latency (cpu, dual_FRintk, ps->post_wait);

  /* Once initiated, post-processing will take 3 cycles.  */
  update_FR_ptime (cpu, out_FRk, 3);
  update_FR_ptime (cpu, out_FRintk, 3);
  update_FR_ptime (cpu, dual_FRk, 3);
  update_FR_ptime (cpu, dual_FRintk, 3);

  /* Mark this use of the register as a floating point op.  */
  if (out_FRk >= 0)
    set_use_is_fpop (cpu, out_FRk);
  if (out_FRintk >= 0)
    set_use_is_fpop (cpu, out_FRintk);

  return cycles;
}

int
frvbf_model_fr500_u_media (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_FRi, INT in_FRj, INT in_ACC40Si, INT in_ACCGi,
			   INT out_FRk,
			   INT out_ACC40Sk, INT out_ACC40Uk, INT out_ACCGk)
{
  int cycles;
  FRV_PROFILE_STATE *ps;
  const CGEN_INSN *insn;
  int is_media_s1;
  int is_media_s2;
  int busy_adjustment[] = {0, 0, 0};
  int *fr;
  int *acc;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ps = CPU_PROFILE_STATE (cpu);
  insn = idesc->idata;

  /* If the previous use of the registers was a media op,
     then their latency will be less than previously recorded.
     See Table 13-13 in the LSI.  */
  if (in_FRi >= 0)
    {
      if (use_is_media (cpu, in_FRi))
	{
	  busy_adjustment[0] = 2;
	  decrease_FR_busy (cpu, in_FRi, busy_adjustment[0]);
	}
      else
	enforce_full_fr_latency (cpu, in_FRi);
    }
  if (in_FRj >= 0 && in_FRj != in_FRi)
    {
      if (use_is_media (cpu, in_FRj))
	{
	  busy_adjustment[1] = 2;
	  decrease_FR_busy (cpu, in_FRj, busy_adjustment[1]);
	}
      else
	enforce_full_fr_latency (cpu, in_FRj);
    }
  if (out_FRk >= 0 && out_FRk != in_FRi && out_FRk != in_FRj)
    {
      if (use_is_media (cpu, out_FRk))
	{
	  busy_adjustment[2] = 2;
	  decrease_FR_busy (cpu, out_FRk, busy_adjustment[2]);
	}
      else
	enforce_full_fr_latency (cpu, out_FRk);
    }

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, out_FRk);
  post_wait_for_ACC (cpu, in_ACC40Si);
  post_wait_for_ACC (cpu, in_ACCGi);
  post_wait_for_ACC (cpu, out_ACC40Sk);
  post_wait_for_ACC (cpu, out_ACC40Uk);
  post_wait_for_ACC (cpu, out_ACCGk);

  /* Restore the busy cycles of the registers we used.  */
  fr = ps->fr_busy;
  if (in_FRi >= 0)
    fr[in_FRi] += busy_adjustment[0];
  if (in_FRj >= 0)
    fr[in_FRj] += busy_adjustment[1];
  if (out_FRk >= 0)
    fr[out_FRk] += busy_adjustment[2];

  /* The latency of tht output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 3 cycles.  */
  if (out_FRk >= 0)
    {
      update_FR_latency (cpu, out_FRk, ps->post_wait);
      update_FR_ptime (cpu, out_FRk, 3);
      /* Mark this use of the register as a media op.  */
      set_use_is_media (cpu, out_FRk);
    }
  /* The latency of tht output accumulator will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  if (out_ACC40Sk >= 0)
    update_ACC_latency (cpu, out_ACC40Sk, ps->post_wait + 1);
  if (out_ACC40Uk >= 0)
    update_ACC_latency (cpu, out_ACC40Uk, ps->post_wait + 1);
  if (out_ACCGk >= 0)
    update_ACC_latency (cpu, out_ACCGk, ps->post_wait + 1);

  return cycles;
}

int
frvbf_model_fr500_u_media_quad_arith (SIM_CPU *cpu, const IDESC *idesc,
				       int unit_num, int referenced,
				       INT in_FRi, INT in_FRj,
				       INT out_FRk)
{
  int cycles;
  INT dual_FRi;
  INT dual_FRj;
  INT dual_FRk;
  FRV_PROFILE_STATE *ps;
  int busy_adjustment[] = {0, 0, 0, 0, 0, 0};
  int *fr;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ps = CPU_PROFILE_STATE (cpu);
  dual_FRi = DUAL_REG (in_FRi);
  dual_FRj = DUAL_REG (in_FRj);
  dual_FRk = DUAL_REG (out_FRk);

  /* If the previous use of the registers was a media op,
     then their latency will be less than previously recorded.
     See Table 13-13 in the LSI.  */
  if (use_is_media (cpu, in_FRi))
    {
      busy_adjustment[0] = 2;
      decrease_FR_busy (cpu, in_FRi, busy_adjustment[0]);
    }
  else
    enforce_full_fr_latency (cpu, in_FRi);
  if (dual_FRi >= 0 && use_is_media (cpu, dual_FRi))
    {
      busy_adjustment[1] = 2;
      decrease_FR_busy (cpu, dual_FRi, busy_adjustment[1]);
    }
  else
    enforce_full_fr_latency (cpu, dual_FRi);
  if (in_FRj != in_FRi)
    {
      if (use_is_media (cpu, in_FRj))
	{
	  busy_adjustment[2] = 2;
	  decrease_FR_busy (cpu, in_FRj, busy_adjustment[2]);
	}
      else
	enforce_full_fr_latency (cpu, in_FRj);
      if (dual_FRj >= 0 && use_is_media (cpu, dual_FRj))
	{
	  busy_adjustment[3] = 2;
	  decrease_FR_busy (cpu, dual_FRj, busy_adjustment[3]);
	}
      else
	enforce_full_fr_latency (cpu, dual_FRj + 1);
    }
  if (out_FRk != in_FRi && out_FRk != in_FRj)
    {
      if (use_is_media (cpu, out_FRk))
	{
	  busy_adjustment[4] = 2;
	  decrease_FR_busy (cpu, out_FRk, busy_adjustment[4]);
	}
      else
	enforce_full_fr_latency (cpu, out_FRk);
      if (dual_FRk >= 0 && use_is_media (cpu, dual_FRk))
	{
	  busy_adjustment[5] = 2;
	  decrease_FR_busy (cpu, dual_FRk, busy_adjustment[5]);
	}
      else
	enforce_full_fr_latency (cpu, dual_FRk);
    }

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, dual_FRi);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, dual_FRj);
  post_wait_for_FR (cpu, out_FRk);
  post_wait_for_FR (cpu, dual_FRk);

  /* Restore the busy cycles of the registers we used.  */
  fr = ps->fr_busy;
  fr[in_FRi] += busy_adjustment[0];
  if (dual_FRi >= 0)
    fr[dual_FRi] += busy_adjustment[1];
  fr[in_FRj] += busy_adjustment[2];
  if (dual_FRj >= 0)
    fr[dual_FRj] += busy_adjustment[3];
  fr[out_FRk] += busy_adjustment[4];
  if (dual_FRk >= 0)
    fr[dual_FRk] += busy_adjustment[5];

  /* The latency of tht output register will be at least the latency of the
     other inputs.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);

  /* Once initiated, post-processing will take 3 cycles.  */
  update_FR_ptime (cpu, out_FRk, 3);

  /* Mark this use of the register as a media op.  */
  set_use_is_media (cpu, out_FRk);
  if (dual_FRk >= 0)
    {
      update_FR_latency (cpu, dual_FRk, ps->post_wait);
      update_FR_ptime (cpu, dual_FRk, 3);
      /* Mark this use of the register as a media op.  */
      set_use_is_media (cpu, dual_FRk);
    }

  return cycles;
}

int
frvbf_model_fr500_u_media_dual_mul (SIM_CPU *cpu, const IDESC *idesc,
				    int unit_num, int referenced,
				    INT in_FRi, INT in_FRj,
				    INT out_ACC40Sk, INT out_ACC40Uk)
{
  int cycles;
  INT dual_ACC40Sk;
  INT dual_ACC40Uk;
  FRV_PROFILE_STATE *ps;
  int busy_adjustment[] = {0, 0, 0, 0, 0, 0};
  int *fr;
  int *acc;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ps = CPU_PROFILE_STATE (cpu);
  dual_ACC40Sk = DUAL_REG (out_ACC40Sk);
  dual_ACC40Uk = DUAL_REG (out_ACC40Uk);

  /* If the previous use of the registers was a media op,
     then their latency will be less than previously recorded.
     See Table 13-13 in the LSI.  */
  if (use_is_media (cpu, in_FRi))
    {
      busy_adjustment[0] = 2;
      decrease_FR_busy (cpu, in_FRi, busy_adjustment[0]);
    }
  else
    enforce_full_fr_latency (cpu, in_FRi);
  if (in_FRj != in_FRi)
    {
      if (use_is_media (cpu, in_FRj))
	{
	  busy_adjustment[1] = 2;
	  decrease_FR_busy (cpu, in_FRj, busy_adjustment[1]);
	}
      else
	enforce_full_fr_latency (cpu, in_FRj);
    }
  if (out_ACC40Sk >= 0)
    {
      busy_adjustment[2] = 1;
      decrease_ACC_busy (cpu, out_ACC40Sk, busy_adjustment[2]);
    }
  if (dual_ACC40Sk >= 0)
    {
      busy_adjustment[3] = 1;
      decrease_ACC_busy (cpu, dual_ACC40Sk, busy_adjustment[3]);
    }
  if (out_ACC40Uk >= 0)
    {
      busy_adjustment[4] = 1;
      decrease_ACC_busy (cpu, out_ACC40Uk, busy_adjustment[4]);
    }
  if (dual_ACC40Uk >= 0)
    {
      busy_adjustment[5] = 1;
      decrease_ACC_busy (cpu, dual_ACC40Uk, busy_adjustment[5]);
    }

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_ACC (cpu, out_ACC40Sk);
  post_wait_for_ACC (cpu, dual_ACC40Sk);
  post_wait_for_ACC (cpu, out_ACC40Uk);
  post_wait_for_ACC (cpu, dual_ACC40Uk);

  /* Restore the busy cycles of the registers we used.  */
  fr = ps->fr_busy;
  acc = ps->acc_busy;
  fr[in_FRi] += busy_adjustment[0];
  fr[in_FRj] += busy_adjustment[1];
  if (out_ACC40Sk >= 0)
    acc[out_ACC40Sk] += busy_adjustment[2];
  if (dual_ACC40Sk >= 0)
    acc[dual_ACC40Sk] += busy_adjustment[3];
  if (out_ACC40Uk >= 0)
    acc[out_ACC40Uk] += busy_adjustment[4];
  if (dual_ACC40Uk >= 0)
    acc[dual_ACC40Uk] += busy_adjustment[5];

  /* The latency of tht output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  if (out_ACC40Sk >= 0)
    update_ACC_latency (cpu, out_ACC40Sk, ps->post_wait + 1);
  if (dual_ACC40Sk >= 0)
    update_ACC_latency (cpu, dual_ACC40Sk, ps->post_wait + 1);
  if (out_ACC40Uk >= 0)
    update_ACC_latency (cpu, out_ACC40Uk, ps->post_wait + 1);
  if (dual_ACC40Uk >= 0)
    update_ACC_latency (cpu, dual_ACC40Uk, ps->post_wait + 1);

  return cycles;
}

int
frvbf_model_fr500_u_media_quad_mul (SIM_CPU *cpu, const IDESC *idesc,
				    int unit_num, int referenced,
				    INT in_FRi, INT in_FRj,
				    INT out_ACC40Sk, INT out_ACC40Uk)
{
  int cycles;
  INT FRi_1;
  INT FRj_1;
  INT ACC40Sk_1;
  INT ACC40Sk_2;
  INT ACC40Sk_3;
  INT ACC40Uk_1;
  INT ACC40Uk_2;
  INT ACC40Uk_3;
  FRV_PROFILE_STATE *ps;
  int busy_adjustment[] = {0, 0, 0, 0, 0, 0, 0 ,0};
  int *fr;
  int *acc;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  FRi_1 = DUAL_REG (in_FRi);
  FRj_1 = DUAL_REG (in_FRj);
  ACC40Sk_1 = DUAL_REG (out_ACC40Sk);
  ACC40Sk_2 = DUAL_REG (ACC40Sk_1);
  ACC40Sk_3 = DUAL_REG (ACC40Sk_2);
  ACC40Uk_1 = DUAL_REG (out_ACC40Uk);
  ACC40Uk_2 = DUAL_REG (ACC40Uk_1);
  ACC40Uk_3 = DUAL_REG (ACC40Uk_2);

  /* If the previous use of the registers was a media op,
     then their latency will be less than previously recorded.
     See Table 13-13 in the LSI.  */
  ps = CPU_PROFILE_STATE (cpu);
  if (use_is_media (cpu, in_FRi))
    {
      busy_adjustment[0] = 2;
      decrease_FR_busy (cpu, in_FRi, busy_adjustment[0]);
    }
  else
    enforce_full_fr_latency (cpu, in_FRi);
  if (FRi_1 >= 0)
    {
      if (use_is_media (cpu, FRi_1))
	{
	  busy_adjustment[1] = 2;
	  decrease_FR_busy (cpu, FRi_1, busy_adjustment[1]);
	}
      else
	enforce_full_fr_latency (cpu, FRi_1);
    }
  if (in_FRj != in_FRi)
    {
      if (use_is_media (cpu, in_FRj))
	{
	  busy_adjustment[2] = 2;
	  decrease_FR_busy (cpu, in_FRj, busy_adjustment[2]);
	}
      else
	enforce_full_fr_latency (cpu, in_FRj);
      if (FRj_1 >= 0)
	{
	  if (use_is_media (cpu, FRj_1))
	    {
	      busy_adjustment[3] = 2;
	      decrease_FR_busy (cpu, FRj_1, busy_adjustment[3]);
	    }
	  else
	    enforce_full_fr_latency (cpu, FRj_1);
	}
    }
  if (out_ACC40Sk >= 0)
    {
      busy_adjustment[4] = 1;
      decrease_ACC_busy (cpu, out_ACC40Sk, busy_adjustment[4]);

      if (ACC40Sk_1 >= 0)
	{
	  busy_adjustment[5] = 1;
	  decrease_ACC_busy (cpu, ACC40Sk_1, busy_adjustment[5]);
	}
      if (ACC40Sk_2 >= 0)
	{
	  busy_adjustment[6] = 1;
	  decrease_ACC_busy (cpu, ACC40Sk_2, busy_adjustment[6]);
	}
      if (ACC40Sk_3 >= 0)
	{
	  busy_adjustment[7] = 1;
	  decrease_ACC_busy (cpu, ACC40Sk_3, busy_adjustment[7]);
	}
    }
  else if (out_ACC40Uk >= 0)
    {
      busy_adjustment[4] = 1;
      decrease_ACC_busy (cpu, out_ACC40Uk, busy_adjustment[4]);

      if (ACC40Uk_1 >= 0)
	{
	  busy_adjustment[5] = 1;
	  decrease_ACC_busy (cpu, ACC40Uk_1, busy_adjustment[5]);
	}
      if (ACC40Uk_2 >= 0)
	{
	  busy_adjustment[6] = 1;
	  decrease_ACC_busy (cpu, ACC40Uk_2, busy_adjustment[6]);
	}
      if (ACC40Uk_3 >= 0)
	{
	  busy_adjustment[7] = 1;
	  decrease_ACC_busy (cpu, ACC40Uk_3, busy_adjustment[7]);
	}
    }

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, FRi_1);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, FRj_1);
  post_wait_for_ACC (cpu, out_ACC40Sk);
  post_wait_for_ACC (cpu, ACC40Sk_1);
  post_wait_for_ACC (cpu, ACC40Sk_2);
  post_wait_for_ACC (cpu, ACC40Sk_3);
  post_wait_for_ACC (cpu, out_ACC40Uk);
  post_wait_for_ACC (cpu, ACC40Uk_1);
  post_wait_for_ACC (cpu, ACC40Uk_2);
  post_wait_for_ACC (cpu, ACC40Uk_3);

  /* Restore the busy cycles of the registers we used.  */
  fr = ps->fr_busy;
  acc = ps->acc_busy;
  fr[in_FRi] += busy_adjustment[0];
  if (FRi_1 >= 0)
    fr[FRi_1] += busy_adjustment[1];
  fr[in_FRj] += busy_adjustment[2];
  if (FRj_1 > 0)
    fr[FRj_1] += busy_adjustment[3];
  if (out_ACC40Sk >= 0)
    {
      acc[out_ACC40Sk] += busy_adjustment[4];
      if (ACC40Sk_1 >= 0)
	acc[ACC40Sk_1] += busy_adjustment[5];
      if (ACC40Sk_2 >= 0)
	acc[ACC40Sk_2] += busy_adjustment[6];
      if (ACC40Sk_3 >= 0)
	acc[ACC40Sk_3] += busy_adjustment[7];
    }
  else if (out_ACC40Uk >= 0)
    {
      acc[out_ACC40Uk] += busy_adjustment[4];
      if (ACC40Uk_1 >= 0)
	acc[ACC40Uk_1] += busy_adjustment[5];
      if (ACC40Uk_2 >= 0)
	acc[ACC40Uk_2] += busy_adjustment[6];
      if (ACC40Uk_3 >= 0)
	acc[ACC40Uk_3] += busy_adjustment[7];
    }

  /* The latency of tht output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  if (out_ACC40Sk >= 0)
    {
      update_ACC_latency (cpu, out_ACC40Sk, ps->post_wait + 1);
      if (ACC40Sk_1 >= 0)
	update_ACC_latency (cpu, ACC40Sk_1, ps->post_wait + 1);
      if (ACC40Sk_2 >= 0)
	update_ACC_latency (cpu, ACC40Sk_2, ps->post_wait + 1);
      if (ACC40Sk_3 >= 0)
	update_ACC_latency (cpu, ACC40Sk_3, ps->post_wait + 1);
    }
  else if (out_ACC40Uk >= 0)
    {
      update_ACC_latency (cpu, out_ACC40Uk, ps->post_wait + 1);
      if (ACC40Uk_1 >= 0)
	update_ACC_latency (cpu, ACC40Uk_1, ps->post_wait + 1);
      if (ACC40Uk_2 >= 0)
	update_ACC_latency (cpu, ACC40Uk_2, ps->post_wait + 1);
      if (ACC40Uk_3 >= 0)
	update_ACC_latency (cpu, ACC40Uk_3, ps->post_wait + 1);
    }

  return cycles;
}

int
frvbf_model_fr500_u_media_quad_complex (SIM_CPU *cpu, const IDESC *idesc,
					int unit_num, int referenced,
					INT in_FRi, INT in_FRj,
					INT out_ACC40Sk)
{
  int cycles;
  INT FRi_1;
  INT FRj_1;
  INT ACC40Sk_1;
  FRV_PROFILE_STATE *ps;
  int busy_adjustment[] = {0, 0, 0, 0, 0, 0};
  int *fr;
  int *acc;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  FRi_1 = DUAL_REG (in_FRi);
  FRj_1 = DUAL_REG (in_FRj);
  ACC40Sk_1 = DUAL_REG (out_ACC40Sk);

  /* If the previous use of the registers was a media op,
     then their latency will be less than previously recorded.
     See Table 13-13 in the LSI.  */
  ps = CPU_PROFILE_STATE (cpu);
  if (use_is_media (cpu, in_FRi))
    {
      busy_adjustment[0] = 2;
      decrease_FR_busy (cpu, in_FRi, busy_adjustment[0]);
    }
  else
    enforce_full_fr_latency (cpu, in_FRi);
  if (FRi_1 >= 0)
    {
      if (use_is_media (cpu, FRi_1))
	{
	  busy_adjustment[1] = 2;
	  decrease_FR_busy (cpu, FRi_1, busy_adjustment[1]);
	}
      else
	enforce_full_fr_latency (cpu, FRi_1);
    }
  if (in_FRj != in_FRi)
    {
      if (use_is_media (cpu, in_FRj))
	{
	  busy_adjustment[2] = 2;
	  decrease_FR_busy (cpu, in_FRj, busy_adjustment[2]);
	}
      else
	enforce_full_fr_latency (cpu, in_FRj);
      if (FRj_1 >= 0)
	{
	  if (use_is_media (cpu, FRj_1))
	    {
	      busy_adjustment[3] = 2;
	      decrease_FR_busy (cpu, FRj_1, busy_adjustment[3]);
	    }
	  else
	    enforce_full_fr_latency (cpu, FRj_1);
	}
    }
  if (out_ACC40Sk >= 0)
    {
      busy_adjustment[4] = 1;
      decrease_ACC_busy (cpu, out_ACC40Sk, busy_adjustment[4]);

      if (ACC40Sk_1 >= 0)
	{
	  busy_adjustment[5] = 1;
	  decrease_ACC_busy (cpu, ACC40Sk_1, busy_adjustment[5]);
	}
    }

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, FRi_1);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, FRj_1);
  post_wait_for_ACC (cpu, out_ACC40Sk);
  post_wait_for_ACC (cpu, ACC40Sk_1);

  /* Restore the busy cycles of the registers we used.  */
  fr = ps->fr_busy;
  acc = ps->acc_busy;
  fr[in_FRi] += busy_adjustment[0];
  if (FRi_1 >= 0)
    fr[FRi_1] += busy_adjustment[1];
  fr[in_FRj] += busy_adjustment[2];
  if (FRj_1 > 0)
    fr[FRj_1] += busy_adjustment[3];
  if (out_ACC40Sk >= 0)
    {
      acc[out_ACC40Sk] += busy_adjustment[4];
      if (ACC40Sk_1 >= 0)
	acc[ACC40Sk_1] += busy_adjustment[5];
    }

  /* The latency of tht output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  if (out_ACC40Sk >= 0)
    {
      update_ACC_latency (cpu, out_ACC40Sk, ps->post_wait + 1);
      if (ACC40Sk_1 >= 0)
	update_ACC_latency (cpu, ACC40Sk_1, ps->post_wait + 1);
    }

  return cycles;
}

int
frvbf_model_fr500_u_media_dual_expand (SIM_CPU *cpu, const IDESC *idesc,
				       int unit_num, int referenced,
				       INT in_FRi,
				       INT out_FRk)
{
  int cycles;
  INT dual_FRk;
  FRV_PROFILE_STATE *ps;
  int busy_adjustment[] = {0, 0, 0};
  int *fr;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  /* If the previous use of the registers was a media op,
     then their latency will be less than previously recorded.
     See Table 13-13 in the LSI.  */
  dual_FRk = DUAL_REG (out_FRk);
  ps = CPU_PROFILE_STATE (cpu);
  if (use_is_media (cpu, in_FRi))
    {
      busy_adjustment[0] = 2;
      decrease_FR_busy (cpu, in_FRi, busy_adjustment[0]);
    }
  else
    enforce_full_fr_latency (cpu, in_FRi);
  if (out_FRk != in_FRi)
    {
      if (use_is_media (cpu, out_FRk))
	{
	  busy_adjustment[1] = 2;
	  decrease_FR_busy (cpu, out_FRk, busy_adjustment[1]);
	}
      else
	enforce_full_fr_latency (cpu, out_FRk);
    }
  if (dual_FRk >= 0 && dual_FRk != in_FRi)
    {
      if (use_is_media (cpu, dual_FRk))
	{
	  busy_adjustment[2] = 2;
	  decrease_FR_busy (cpu, dual_FRk, busy_adjustment[2]);
	}
      else
	enforce_full_fr_latency (cpu, dual_FRk);
    }

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, out_FRk);
  post_wait_for_FR (cpu, dual_FRk);

  /* Restore the busy cycles of the registers we used.  */
  fr = ps->fr_busy;
  fr[in_FRi] += busy_adjustment[0];
  fr[out_FRk] += busy_adjustment[1];
  if (dual_FRk >= 0)
    fr[dual_FRk] += busy_adjustment[2];

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 3 cycles.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FR_ptime (cpu, out_FRk, 3);

  /* Mark this use of the register as a media op.  */
  set_use_is_media (cpu, out_FRk);
  if (dual_FRk >= 0)
    {
      update_FR_latency (cpu, dual_FRk, ps->post_wait);
      update_FR_ptime (cpu, dual_FRk, 3);

      /* Mark this use of the register as a media op.  */
      set_use_is_media (cpu, dual_FRk);
    }

  return cycles;
}

int
frvbf_model_fr500_u_media_dual_unpack (SIM_CPU *cpu, const IDESC *idesc,
				       int unit_num, int referenced,
				       INT in_FRi,
				       INT out_FRk)
{
  int cycles;
  INT FRi_1;
  INT FRk_1;
  INT FRk_2;
  INT FRk_3;
  FRV_PROFILE_STATE *ps;
  int busy_adjustment[] = {0, 0, 0, 0, 0, 0};
  int *fr;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  FRi_1 = DUAL_REG (in_FRi);
  FRk_1 = DUAL_REG (out_FRk);
  FRk_2 = DUAL_REG (FRk_1);
  FRk_3 = DUAL_REG (FRk_2);

  /* If the previous use of the registers was a media op,
     then their latency will be less than previously recorded.
     See Table 13-13 in the LSI.  */
  ps = CPU_PROFILE_STATE (cpu);
  if (use_is_media (cpu, in_FRi))
    {
      busy_adjustment[0] = 2;
      decrease_FR_busy (cpu, in_FRi, busy_adjustment[0]);
    }
  else
    enforce_full_fr_latency (cpu, in_FRi);
  if (FRi_1 >= 0 && use_is_media (cpu, FRi_1))
    {
      busy_adjustment[1] = 2;
      decrease_FR_busy (cpu, FRi_1, busy_adjustment[1]);
    }
  else
    enforce_full_fr_latency (cpu, FRi_1);
  if (out_FRk != in_FRi)
    {
      if (use_is_media (cpu, out_FRk))
	{
	  busy_adjustment[2] = 2;
	  decrease_FR_busy (cpu, out_FRk, busy_adjustment[2]);
	}
      else
	enforce_full_fr_latency (cpu, out_FRk);
      if (FRk_1 >= 0 && FRk_1 != in_FRi)
	{
	  if (use_is_media (cpu, FRk_1))
	    {
	      busy_adjustment[3] = 2;
	      decrease_FR_busy (cpu, FRk_1, busy_adjustment[3]);
	    }
	  else
	    enforce_full_fr_latency (cpu, FRk_1);
	}
      if (FRk_2 >= 0 && FRk_2 != in_FRi)
	{
	  if (use_is_media (cpu, FRk_2))
	    {
	      busy_adjustment[4] = 2;
	      decrease_FR_busy (cpu, FRk_2, busy_adjustment[4]);
	    }
	  else
	    enforce_full_fr_latency (cpu, FRk_2);
	}
      if (FRk_3 >= 0 && FRk_3 != in_FRi)
	{
	  if (use_is_media (cpu, FRk_3))
	    {
	      busy_adjustment[5] = 2;
	      decrease_FR_busy (cpu, FRk_3, busy_adjustment[5]);
	    }
	  else
	    enforce_full_fr_latency (cpu, FRk_3);
	}
    }

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, FRi_1);
  post_wait_for_FR (cpu, out_FRk);
  post_wait_for_FR (cpu, FRk_1);
  post_wait_for_FR (cpu, FRk_2);
  post_wait_for_FR (cpu, FRk_3);

  /* Restore the busy cycles of the registers we used.  */
  fr = ps->fr_busy;
  fr[in_FRi] += busy_adjustment[0];
  if (FRi_1 >= 0)
    fr[FRi_1] += busy_adjustment[1];
  fr[out_FRk] += busy_adjustment[2];
  if (FRk_1 >= 0)
    fr[FRk_1] += busy_adjustment[3];
  if (FRk_2 >= 0)
    fr[FRk_2] += busy_adjustment[4];
  if (FRk_3 >= 0)
    fr[FRk_3] += busy_adjustment[5];

  /* The latency of tht output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 3 cycles.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FR_ptime (cpu, out_FRk, 3);

  /* Mark this use of the register as a media op.  */
  set_use_is_media (cpu, out_FRk);
  if (FRk_1 >= 0)
    {
      update_FR_latency (cpu, FRk_1, ps->post_wait);
      update_FR_ptime (cpu, FRk_1, 3);

      /* Mark this use of the register as a media op.  */
      set_use_is_media (cpu, FRk_1);
    }
  if (FRk_2 >= 0)
    {
      update_FR_latency (cpu, FRk_2, ps->post_wait);
      update_FR_ptime (cpu, FRk_2, 3);

      /* Mark this use of the register as a media op.  */
      set_use_is_media (cpu, FRk_2);
    }
  if (FRk_3 >= 0)
    {
      update_FR_latency (cpu, FRk_3, ps->post_wait);
      update_FR_ptime (cpu, FRk_3, 3);

      /* Mark this use of the register as a media op.  */
      set_use_is_media (cpu, FRk_3);
    }

  return cycles;
}

int
frvbf_model_fr500_u_media_dual_btoh (SIM_CPU *cpu, const IDESC *idesc,
				     int unit_num, int referenced,
				     INT in_FRj,
				     INT out_FRk)
{
  return frvbf_model_fr500_u_media_dual_expand (cpu, idesc, unit_num,
						referenced, in_FRj, out_FRk);
}

int
frvbf_model_fr500_u_media_dual_htob (SIM_CPU *cpu, const IDESC *idesc,
				     int unit_num, int referenced,
				     INT in_FRj,
				     INT out_FRk)
{
  int cycles;
  INT dual_FRj;
  FRV_PROFILE_STATE *ps;
  int busy_adjustment[] = {0, 0, 0};
  int *fr;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  /* If the previous use of the registers was a media op,
     then their latency will be less than previously recorded.
     See Table 13-13 in the LSI.  */
  dual_FRj = DUAL_REG (in_FRj);
  ps = CPU_PROFILE_STATE (cpu);
  if (use_is_media (cpu, in_FRj))
    {
      busy_adjustment[0] = 2;
      decrease_FR_busy (cpu, in_FRj, busy_adjustment[0]);
    }
  else
    enforce_full_fr_latency (cpu, in_FRj);
  if (dual_FRj >= 0)
    {
      if (use_is_media (cpu, dual_FRj))
	{
	  busy_adjustment[1] = 2;
	  decrease_FR_busy (cpu, dual_FRj, busy_adjustment[1]);
	}
      else
	enforce_full_fr_latency (cpu, dual_FRj);
    }
  if (out_FRk != in_FRj)
    {
      if (use_is_media (cpu, out_FRk))
	{
	  busy_adjustment[2] = 2;
	  decrease_FR_busy (cpu, out_FRk, busy_adjustment[2]);
	}
      else
	enforce_full_fr_latency (cpu, out_FRk);
    }

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, dual_FRj);
  post_wait_for_FR (cpu, out_FRk);

  /* Restore the busy cycles of the registers we used.  */
  fr = ps->fr_busy;
  fr[in_FRj] += busy_adjustment[0];
  if (dual_FRj >= 0)
    fr[dual_FRj] += busy_adjustment[1];
  fr[out_FRk] += busy_adjustment[2];

  /* The latency of tht output register will be at least the latency of the
     other inputs.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);

  /* Once initiated, post-processing will take 3 cycles.  */
  update_FR_ptime (cpu, out_FRk, 3);

  /* Mark this use of the register as a media op.  */
  set_use_is_media (cpu, out_FRk);

  return cycles;
}

int
frvbf_model_fr500_u_media_dual_btohe (SIM_CPU *cpu, const IDESC *idesc,
				      int unit_num, int referenced,
				      INT in_FRj,
				      INT out_FRk)
{
  int cycles;
  INT FRk_1;
  INT FRk_2;
  INT FRk_3;
  FRV_PROFILE_STATE *ps;
  int busy_adjustment[] = {0, 0, 0, 0, 0};
  int *fr;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  FRk_1 = DUAL_REG (out_FRk);
  FRk_2 = DUAL_REG (FRk_1);
  FRk_3 = DUAL_REG (FRk_2);

  /* If the previous use of the registers was a media op,
     then their latency will be less than previously recorded.
     See Table 13-13 in the LSI.  */
  ps = CPU_PROFILE_STATE (cpu);
  if (use_is_media (cpu, in_FRj))
    {
      busy_adjustment[0] = 2;
      decrease_FR_busy (cpu, in_FRj, busy_adjustment[0]);
    }
  else
    enforce_full_fr_latency (cpu, in_FRj);
  if (out_FRk != in_FRj)
    {
      if (use_is_media (cpu, out_FRk))
	{
	  busy_adjustment[1] = 2;
	  decrease_FR_busy (cpu, out_FRk, busy_adjustment[1]);
	}
      else
	enforce_full_fr_latency (cpu, out_FRk);
      if (FRk_1 >= 0 && FRk_1 != in_FRj)
	{
	  if (use_is_media (cpu, FRk_1))
	    {
	      busy_adjustment[2] = 2;
	      decrease_FR_busy (cpu, FRk_1, busy_adjustment[2]);
	    }
	  else
	    enforce_full_fr_latency (cpu, FRk_1);
	}
      if (FRk_2 >= 0 && FRk_2 != in_FRj)
	{
	  if (use_is_media (cpu, FRk_2))
	    {
	      busy_adjustment[3] = 2;
	      decrease_FR_busy (cpu, FRk_2, busy_adjustment[3]);
	    }
	  else
	    enforce_full_fr_latency (cpu, FRk_2);
	}
      if (FRk_3 >= 0 && FRk_3 != in_FRj)
	{
	  if (use_is_media (cpu, FRk_3))
	    {
	      busy_adjustment[4] = 2;
	      decrease_FR_busy (cpu, FRk_3, busy_adjustment[4]);
	    }
	  else
	    enforce_full_fr_latency (cpu, FRk_3);
	}
    }

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, out_FRk);
  post_wait_for_FR (cpu, FRk_1);
  post_wait_for_FR (cpu, FRk_2);
  post_wait_for_FR (cpu, FRk_3);

  /* Restore the busy cycles of the registers we used.  */
  fr = ps->fr_busy;
  fr[in_FRj] += busy_adjustment[0];
  fr[out_FRk] += busy_adjustment[1];
  if (FRk_1 >= 0)
    fr[FRk_1] += busy_adjustment[2];
  if (FRk_2 >= 0)
    fr[FRk_2] += busy_adjustment[3];
  if (FRk_3 >= 0)
    fr[FRk_3] += busy_adjustment[4];

  /* The latency of tht output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 3 cycles.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FR_ptime (cpu, out_FRk, 3);

  /* Mark this use of the register as a media op.  */
  set_use_is_media (cpu, out_FRk);
  if (FRk_1 >= 0)
    {
      update_FR_latency (cpu, FRk_1, ps->post_wait);
      update_FR_ptime (cpu, FRk_1, 3);

      /* Mark this use of the register as a media op.  */
      set_use_is_media (cpu, FRk_1);
    }
  if (FRk_2 >= 0)
    {
      update_FR_latency (cpu, FRk_2, ps->post_wait);
      update_FR_ptime (cpu, FRk_2, 3);

      /* Mark this use of the register as a media op.  */
      set_use_is_media (cpu, FRk_2);
    }
  if (FRk_3 >= 0)
    {
      update_FR_latency (cpu, FRk_3, ps->post_wait);
      update_FR_ptime (cpu, FRk_3, 3);

      /* Mark this use of the register as a media op.  */
      set_use_is_media (cpu, FRk_3);
    }

  return cycles;
}

int
frvbf_model_fr500_u_barrier (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced)
{
  int cycles;
  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      int i;
      /* Wait for ALL resources.  */
      for (i = 0; i < 64; ++i)
	{
	  enforce_full_fr_latency (cpu, i);
	  vliw_wait_for_GR (cpu, i);
	  vliw_wait_for_FR (cpu, i);
	  vliw_wait_for_ACC (cpu, i);
	}
      for (i = 0; i < 8; ++i)
	vliw_wait_for_CCR (cpu, i);
      for (i = 0; i < 2; ++i)
	{
	  vliw_wait_for_idiv_resource (cpu, i);
	  vliw_wait_for_fdiv_resource (cpu, i);
	  vliw_wait_for_fsqrt_resource (cpu, i);
	}
      handle_resource_wait (cpu);
      for (i = 0; i < 64; ++i)
	{
	  load_wait_for_GR (cpu, i);
	  load_wait_for_FR (cpu, i);
	}
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;
  return cycles;
}

int
frvbf_model_fr500_u_membar (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced)
{
  int cycles;
  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      int i;
      /* Wait for ALL resources, except GR and ICC.  */
      for (i = 0; i < 64; ++i)
	{
	  enforce_full_fr_latency (cpu, i);
	  vliw_wait_for_FR (cpu, i);
	  vliw_wait_for_ACC (cpu, i);
	}
      for (i = 0; i < 4; ++i)
	vliw_wait_for_CCR (cpu, i);
      for (i = 0; i < 2; ++i)
	{
	  vliw_wait_for_idiv_resource (cpu, i);
	  vliw_wait_for_fdiv_resource (cpu, i);
	  vliw_wait_for_fsqrt_resource (cpu, i);
	}
      handle_resource_wait (cpu);
      for (i = 0; i < 64; ++i)
	{
	  load_wait_for_FR (cpu, i);
	}
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  cycles = idesc->timing->units[unit_num].done;
  return cycles;
}

/* The frv machine is a fictional implementation of the fr500 which implements
   all frv architectural features.  */
int
frvbf_model_frv_u_exec (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced)
{
  return idesc->timing->units[unit_num].done;
}

/* The simple machine is a fictional implementation of the fr500 which
   implements limited frv architectural features.  */
int
frvbf_model_simple_u_exec (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced)
{
  return idesc->timing->units[unit_num].done;
}

/* The tomcat machine is models a prototype fr500 machine which had a few
   bugs and restrictions to work around.  */
int
frvbf_model_tomcat_u_exec (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced)
{
  return idesc->timing->units[unit_num].done;
}

#endif /* WITH_PROFILE_MODEL_P */
