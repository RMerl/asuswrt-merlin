/* frv simulator fr550 dependent profiling code.

   Copyright (C) 2003, 2007 Free Software Foundation, Inc.
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
#include "profile-fr550.h"

/* Initialize cycle counting for an insn.
   FIRST_P is non-zero if this is the first insn in a set of parallel
   insns.  */
void
fr550_model_insn_before (SIM_CPU *cpu, int first_p)
{
  if (first_p)
    {
      MODEL_FR550_DATA *d = CPU_MODEL_DATA (cpu);
      d->cur_fr_load      = d->prev_fr_load;
      d->cur_fr_complex_1 = d->prev_fr_complex_1;
      d->cur_fr_complex_2 = d->prev_fr_complex_2;
      d->cur_ccr_complex  = d->prev_ccr_complex;
      d->cur_acc_mmac     = d->prev_acc_mmac;
    }
}

/* Record the cycles computed for an insn.
   LAST_P is non-zero if this is the last insn in a set of parallel insns,
   and we update the total cycle count.
   CYCLES is the cycle count of the insn.  */
void
fr550_model_insn_after (SIM_CPU *cpu, int last_p, int cycles)
{
  if (last_p)
    {
      MODEL_FR550_DATA *d = CPU_MODEL_DATA (cpu);
      d->prev_fr_load      = d->cur_fr_load;
      d->prev_fr_complex_1 = d->cur_fr_complex_1;
      d->prev_fr_complex_2 = d->cur_fr_complex_2;
      d->prev_ccr_complex  = d->cur_ccr_complex;
      d->prev_acc_mmac     = d->cur_acc_mmac;
    }
}

static void fr550_reset_fr_flags (SIM_CPU *cpu, INT fr);
static void fr550_reset_ccr_flags (SIM_CPU *cpu, INT ccr);
static void fr550_reset_acc_flags (SIM_CPU *cpu, INT acc);

static void
set_use_is_fr_load (SIM_CPU *cpu, INT fr)
{
  MODEL_FR550_DATA *d = CPU_MODEL_DATA (cpu);
  fr550_reset_fr_flags (cpu, (fr));
  d->cur_fr_load |= (((DI)1) << (fr));
}

static void
set_use_not_fr_load (SIM_CPU *cpu, INT fr)
{
  MODEL_FR550_DATA *d = CPU_MODEL_DATA (cpu);
  d->cur_fr_load &= ~(((DI)1) << (fr));
}

static int
use_is_fr_load (SIM_CPU *cpu, INT fr)
{
  MODEL_FR550_DATA *d = CPU_MODEL_DATA (cpu);
  return d->prev_fr_load & (((DI)1) << (fr));
}

static void
set_use_is_fr_complex_1 (SIM_CPU *cpu, INT fr)
{
  MODEL_FR550_DATA *d = CPU_MODEL_DATA (cpu);
  fr550_reset_fr_flags (cpu, (fr));
  d->cur_fr_complex_1 |= (((DI)1) << (fr));
}

static void
set_use_not_fr_complex_1 (SIM_CPU *cpu, INT fr)
{
  MODEL_FR550_DATA *d = CPU_MODEL_DATA (cpu);
  d->cur_fr_complex_1 &= ~(((DI)1) << (fr));
}

static int
use_is_fr_complex_1 (SIM_CPU *cpu, INT fr)
{
  MODEL_FR550_DATA *d = CPU_MODEL_DATA (cpu);
  return d->prev_fr_complex_1 & (((DI)1) << (fr));
}

static void
set_use_is_fr_complex_2 (SIM_CPU *cpu, INT fr)
{
  MODEL_FR550_DATA *d = CPU_MODEL_DATA (cpu);
  fr550_reset_fr_flags (cpu, (fr));
  d->cur_fr_complex_2 |= (((DI)1) << (fr));
}

static void
set_use_not_fr_complex_2 (SIM_CPU *cpu, INT fr)
{
  MODEL_FR550_DATA *d = CPU_MODEL_DATA (cpu);
  d->cur_fr_complex_2 &= ~(((DI)1) << (fr));
}

static int
use_is_fr_complex_2 (SIM_CPU *cpu, INT fr)
{
  MODEL_FR550_DATA *d = CPU_MODEL_DATA (cpu);
  return d->prev_fr_complex_2 & (((DI)1) << (fr));
}

static void
set_use_is_ccr_complex (SIM_CPU *cpu, INT ccr)
{
  MODEL_FR550_DATA *d = CPU_MODEL_DATA (cpu);
  fr550_reset_ccr_flags (cpu, (ccr));
  d->cur_ccr_complex |= (((SI)1) << (ccr));
}

static void
set_use_not_ccr_complex (SIM_CPU *cpu, INT ccr)
{
  MODEL_FR550_DATA *d = CPU_MODEL_DATA (cpu);
  d->cur_ccr_complex &= ~(((SI)1) << (ccr));
}

static int
use_is_ccr_complex (SIM_CPU *cpu, INT ccr)
{
  MODEL_FR550_DATA *d = CPU_MODEL_DATA (cpu);
  return d->prev_ccr_complex & (((SI)1) << (ccr));
}

static void
set_use_is_acc_mmac (SIM_CPU *cpu, INT acc)
{
  MODEL_FR550_DATA *d = CPU_MODEL_DATA (cpu);
  fr550_reset_acc_flags (cpu, (acc));
  d->cur_acc_mmac |= (((DI)1) << (acc));
}

static void
set_use_not_acc_mmac (SIM_CPU *cpu, INT acc)
{
  MODEL_FR550_DATA *d = CPU_MODEL_DATA (cpu);
  d->cur_acc_mmac &= ~(((DI)1) << (acc));
}

static int
use_is_acc_mmac (SIM_CPU *cpu, INT acc)
{
  MODEL_FR550_DATA *d = CPU_MODEL_DATA (cpu);
  return d->prev_acc_mmac & (((DI)1) << (acc));
}

static void
fr550_reset_fr_flags (SIM_CPU *cpu, INT fr)
{
  set_use_not_fr_load (cpu, fr);
  set_use_not_fr_complex_1 (cpu, fr);
  set_use_not_fr_complex_2 (cpu, fr);
}

static void
fr550_reset_ccr_flags (SIM_CPU *cpu, INT ccr)
{
  set_use_not_ccr_complex (cpu, ccr);
}

static void
fr550_reset_acc_flags (SIM_CPU *cpu, INT acc)
{
  set_use_not_acc_mmac (cpu, acc);
}

/* Detect overlap between two register ranges. Works if one of the registers
   is -1 with width 1 (i.e. undefined), but not both.  */
#define REG_OVERLAP(r1, w1, r2, w2) ( \
  (r1) + (w1) - 1 >= (r2) && (r2) + (w2) - 1 >= (r1) \
)

/* Latency of floating point registers may be less than recorded when followed
   by another floating point insn.  */
static void
adjust_float_register_busy (SIM_CPU *cpu,
			    INT in_FRi, int iwidth,
			    INT in_FRj, int jwidth,
			    INT out_FRk, int kwidth)
{
  int i;
  /* The latency of FRk may be less than previously recorded.
     See Table 14-15 in the LSI.  */
  if (in_FRi >= 0)
    {
      for (i = 0; i < iwidth; ++i)
	{
	  if (! REG_OVERLAP (in_FRi + i, 1, out_FRk, kwidth))
	    if (use_is_fr_load (cpu, in_FRi + i))
	      decrease_FR_busy (cpu, in_FRi + i, 1);
	    else
	      enforce_full_fr_latency (cpu, in_FRi + i);
	}
    }

  if (in_FRj >= 0)
    {
      for (i = 0; i < jwidth; ++i)
	{
	  if (! REG_OVERLAP (in_FRj + i, 1, in_FRi, iwidth)
	      && ! REG_OVERLAP (in_FRj + i, 1, out_FRk, kwidth))
	    if (use_is_fr_load (cpu, in_FRj + i))
	      decrease_FR_busy (cpu, in_FRj + i, 1);
	    else
	      enforce_full_fr_latency (cpu, in_FRj + i);
	}
    }

  if (out_FRk >= 0)
    {
      for (i = 0; i < kwidth; ++i)
	{
	  if (! REG_OVERLAP (out_FRk + i, 1, in_FRi, iwidth)
	      && ! REG_OVERLAP (out_FRk + i, 1, in_FRj, jwidth))
	    {
	      if (use_is_fr_complex_1 (cpu, out_FRk + i))
		decrease_FR_busy (cpu, out_FRk + i, 1);
	      else if (use_is_fr_complex_2 (cpu, out_FRk + i))
		decrease_FR_busy (cpu, out_FRk + i, 2);
	      else
		enforce_full_fr_latency (cpu, out_FRk + i);
	    }
	}
    }
}

static void
restore_float_register_busy (SIM_CPU *cpu,
			     INT in_FRi, int iwidth,
			     INT in_FRj, int jwidth,
			     INT out_FRk, int kwidth)
{
  int i;
  /* The latency of FRk may be less than previously recorded.
     See Table 14-15 in the LSI.  */
  if (in_FRi >= 0)
    {
      for (i = 0; i < iwidth; ++i)
	{
	  if (! REG_OVERLAP (in_FRi + i, 1, out_FRk, kwidth))
	    if (use_is_fr_load (cpu, in_FRi + i))
	      increase_FR_busy (cpu, in_FRi + i, 1);
	}
    }

  if (in_FRj >= 0)
    {
      for (i = 0; i < jwidth; ++i)
	{
	  if (! REG_OVERLAP (in_FRj + i, 1, in_FRi, iwidth)
	      && ! REG_OVERLAP (in_FRj + i, 1, out_FRk, kwidth))
	    if (use_is_fr_load (cpu, in_FRj + i))
	      increase_FR_busy (cpu, in_FRj + i, 1);
	}
    }

  if (out_FRk >= 0)
    {
      for (i = 0; i < kwidth; ++i)
	{
	  if (! REG_OVERLAP (out_FRk + i, 1, in_FRi, iwidth)
	      && ! REG_OVERLAP (out_FRk + i, 1, in_FRj, jwidth))
	    {
	      if (use_is_fr_complex_1 (cpu, out_FRk + i))
		increase_FR_busy (cpu, out_FRk + i, 1);
	      else if (use_is_fr_complex_2 (cpu, out_FRk + i))
		increase_FR_busy (cpu, out_FRk + i, 2);
	    }
	}
    }
}

/* Latency of floating point registers may be less than recorded when used in a
   media insns and followed by another media insn.  */
static void
adjust_float_register_busy_for_media (SIM_CPU *cpu,
				      INT in_FRi, int iwidth,
				      INT in_FRj, int jwidth,
				      INT out_FRk, int kwidth)
{
  int i;
  /* The latency of FRk may be less than previously recorded.
     See Table 14-15 in the LSI.  */
  if (out_FRk >= 0)
    {
      for (i = 0; i < kwidth; ++i)
	{
	  if (! REG_OVERLAP (out_FRk + i, 1, in_FRi, iwidth)
	      && ! REG_OVERLAP (out_FRk + i, 1, in_FRj, jwidth))
	    {
	      if (use_is_fr_complex_1 (cpu, out_FRk + i))
		decrease_FR_busy (cpu, out_FRk + i, 1);
	      else
		enforce_full_fr_latency (cpu, out_FRk + i);
	    }
	}
    }
}

static void
restore_float_register_busy_for_media (SIM_CPU *cpu,
				       INT in_FRi, int iwidth,
				       INT in_FRj, int jwidth,
				       INT out_FRk, int kwidth)
{
  int i;
  if (out_FRk >= 0)
    {
      for (i = 0; i < kwidth; ++i)
	{
	  if (! REG_OVERLAP (out_FRk + i, 1, in_FRi, iwidth)
	      && ! REG_OVERLAP (out_FRk + i, 1, in_FRj, jwidth))
	    {
	      if (use_is_fr_complex_1 (cpu, out_FRk + i))
		increase_FR_busy (cpu, out_FRk + i, 1);
	    }
	}
    }
}

/* Latency of accumulator registers may be less than recorded when used in a
   media insns and followed by another media insn.  */
static void
adjust_acc_busy_for_mmac (SIM_CPU *cpu,
			  INT in_ACC, int inwidth,
			  INT out_ACC, int outwidth)
{
  int i;
  /* The latency of an accumulator may be less than previously recorded.
     See Table 14-15 in the LSI.  */
  if (in_ACC >= 0)
    {
      for (i = 0; i < inwidth; ++i)
	{
	  if (use_is_acc_mmac (cpu, in_ACC + i))
	    decrease_ACC_busy (cpu, in_ACC + i, 1);
	  else
	    enforce_full_acc_latency (cpu, in_ACC + i);
	}
    }
  if (out_ACC >= 0)
    {
      for (i = 0; i < outwidth; ++i)
	{
	  if (! REG_OVERLAP (out_ACC + i, 1, in_ACC, inwidth))
	    {
	      if (use_is_acc_mmac (cpu, out_ACC + i))
		decrease_ACC_busy (cpu, out_ACC + i, 1);
	      else
		enforce_full_acc_latency (cpu, out_ACC + i);
	    }
	}
    }
}

static void
restore_acc_busy_for_mmac (SIM_CPU *cpu,
			   INT in_ACC, int inwidth,
			   INT out_ACC, int outwidth)
{
  int i;
  if (in_ACC >= 0)
    {
      for (i = 0; i < inwidth; ++i)
	{
	  if (use_is_acc_mmac (cpu, in_ACC + i))
	    increase_ACC_busy (cpu, in_ACC + i, 1);
	}
    }
  if (out_ACC >= 0)
    {
      for (i = 0; i < outwidth; ++i)
	{
	  if (! REG_OVERLAP (out_ACC + i, 1, in_ACC, inwidth))
	    {
	      if (use_is_acc_mmac (cpu, out_ACC + i))
		increase_ACC_busy (cpu, out_ACC + i, 1);
	    }
	}
    }
}

int
frvbf_model_fr550_u_exec (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced)
{
  return idesc->timing->units[unit_num].done;
}

int
frvbf_model_fr550_u_integer (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_GRi, INT in_GRj, INT out_GRk,
			     INT out_ICCi_1)
{
  int cycles;

  /* icc0-icc4 are the upper 4 fields of the CCR.  */
  if (out_ICCi_1 >= 0)
    out_ICCi_1 += 4;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.  */
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

  fr550_reset_ccr_flags (cpu, out_ICCi_1);

  /* GRk is available immediately to the next VLIW insn as is ICCi_1.  */
  cycles = idesc->timing->units[unit_num].done;
  return cycles;
}

int
frvbf_model_fr550_u_imul (SIM_CPU *cpu, const IDESC *idesc,
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
	 which is not ready yet.  */
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

  /* GRk has a latency of 1 cycles.  */
  cycles = idesc->timing->units[unit_num].done;
  update_GRdouble_latency (cpu, out_GRk, cycles + 1);

  /* ICCi_1 has a latency of 1 cycle.  */
  update_CCR_latency (cpu, out_ICCi_1, cycles + 1);

  fr550_reset_ccr_flags (cpu, out_ICCi_1);

  return cycles;
}

int
frvbf_model_fr550_u_idiv (SIM_CPU *cpu, const IDESC *idesc,
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
	 which is not ready yet.  */
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

  /* GRk has a latency of 18 cycles!  */
  cycles = idesc->timing->units[unit_num].done;
  update_GR_latency (cpu, out_GRk, cycles + 18);

  /* ICCi_1 has a latency of 18 cycles.  */
  update_CCR_latency (cpu, out_ICCi_1, cycles + 18);

  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      /* GNER has a latency of 18 cycles.  */
      update_SPR_latency (cpu, GNER_FOR_GR (out_GRk), cycles + 18);
    }

  /* the idiv resource has a latency of 18 cycles!  */
  update_idiv_resource_latency (cpu, slot, cycles + 18);

  fr550_reset_ccr_flags (cpu, out_ICCi_1);

  return cycles;
}

int
frvbf_model_fr550_u_branch (SIM_CPU *cpu, const IDESC *idesc,
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
	 which is not ready yet.  */
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
frvbf_model_fr550_u_trap (SIM_CPU *cpu, const IDESC *idesc,
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
	 which is not ready yet.  */
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
frvbf_model_fr550_u_check (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_ICCi_3, INT in_FCCi_3)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_check (cpu, idesc, unit_num, referenced,
				    in_ICCi_3, in_FCCi_3);
}

int
frvbf_model_fr550_u_set_hilo (SIM_CPU *cpu, const IDESC *idesc,
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

  return cycles;
}

int
frvbf_model_fr550_u_gr_load (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_GRi, INT in_GRj,
			     INT out_GRk, INT out_GRdoublek)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.  */
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

  return cycles;
}

int
frvbf_model_fr550_u_gr_store (SIM_CPU *cpu, const IDESC *idesc,
			      int unit_num, int referenced,
			      INT in_GRi, INT in_GRj,
			      INT in_GRk, INT in_GRdoublek)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.  */
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

  /* The target register is available immediately.  */
  cycles = idesc->timing->units[unit_num].done;

  return cycles;
}

int
frvbf_model_fr550_u_fr_load (SIM_CPU *cpu, const IDESC *idesc,
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
      adjust_float_register_busy (cpu, -1, 1, -1, 1, out_FRk, 1);
      adjust_float_register_busy (cpu, -1, 1, -1, 1, out_FRdoublek, 2);
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

  if (out_FRk >= 0)
    set_use_is_fr_load (cpu, out_FRk);
  if (out_FRdoublek >= 0)
    {
      set_use_is_fr_load (cpu, out_FRdoublek);
      set_use_is_fr_load (cpu, out_FRdoublek + 1);
    }

  return cycles;
}

int
frvbf_model_fr550_u_fr_store (SIM_CPU *cpu, const IDESC *idesc,
			      int unit_num, int referenced,
			      INT in_GRi, INT in_GRj,
			      INT in_FRk, INT in_FRdoublek)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.  */
      adjust_float_register_busy (cpu, in_FRk, 1, -1, 1, -1, 1);
      adjust_float_register_busy (cpu, in_FRdoublek, 2, -1, 1, -1, 1);
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

  /* The target register is available immediately.  */
  cycles = idesc->timing->units[unit_num].done;

  return cycles;
}

int
frvbf_model_fr550_u_ici (SIM_CPU *cpu, const IDESC *idesc,
			 int unit_num, int referenced,
			 INT in_GRi, INT in_GRj)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.  */
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
frvbf_model_fr550_u_dci (SIM_CPU *cpu, const IDESC *idesc,
			 int unit_num, int referenced,
			 INT in_GRi, INT in_GRj)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.  */
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
frvbf_model_fr550_u_dcf (SIM_CPU *cpu, const IDESC *idesc,
			 int unit_num, int referenced,
			 INT in_GRi, INT in_GRj)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.  */
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
frvbf_model_fr550_u_icpl (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.  */
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
frvbf_model_fr550_u_dcpl (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.  */
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
frvbf_model_fr550_u_icul (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.  */
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
frvbf_model_fr550_u_dcul (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.  */
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
frvbf_model_fr550_u_float_arith (SIM_CPU *cpu, const IDESC *idesc,
				 int unit_num, int referenced,
				 INT in_FRi, INT in_FRj,
				 INT in_FRdoublei, INT in_FRdoublej,
				 INT out_FRk, INT out_FRdoublek)
{
  int cycles;
  FRV_PROFILE_STATE *ps;
  FRV_VLIW *vliw;
  int slot;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  adjust_float_register_busy (cpu, in_FRi, 1, in_FRj, 1, out_FRk, 1);
  adjust_float_register_busy (cpu, in_FRdoublei, 2, in_FRdoublej, 2, out_FRdoublek, 2);
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_float (cpu, slot);
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
  restore_float_register_busy (cpu, in_FRi, 1, in_FRj, 1, out_FRk, 1);
  restore_float_register_busy (cpu, in_FRdoublei, 2, in_FRdoublej, 2, out_FRdoublek, 2);

  /* The latency of FRk will be at least the latency of the other inputs.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FRdouble_latency (cpu, out_FRdoublek, ps->post_wait);

  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      update_SPR_latency (cpu, FNER_FOR_FR (out_FRk), ps->post_wait);
      update_SPR_latency (cpu, FNER_FOR_FR (out_FRdoublek), ps->post_wait);
    }

  /* Once initiated, post-processing will take 2 cycles.  */
  update_FR_ptime (cpu, out_FRk, 2);
  update_FRdouble_ptime (cpu, out_FRdoublek, 2);
  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      update_SPR_ptime (cpu, FNER_FOR_FR (out_FRk), 2);
      update_SPR_ptime (cpu, FNER_FOR_FR (out_FRdoublek), 2);
    }

  /* Mark this use of the register as a floating point op.  */
  if (out_FRk >= 0)
    set_use_is_fr_complex_2 (cpu, out_FRk);
  if (out_FRdoublek >= 0)
    {
      set_use_is_fr_complex_2 (cpu, out_FRdoublek);
      if (out_FRdoublek < 63)
	set_use_is_fr_complex_2 (cpu, out_FRdoublek + 1);
    }

  /* the media point unit resource has a latency of 4 cycles  */
  update_media_resource_latency (cpu, slot, cycles + 4);

  return cycles;
}

int
frvbf_model_fr550_u_float_dual_arith (SIM_CPU *cpu, const IDESC *idesc,
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
  FRV_VLIW *vliw;
  int slot;

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

  adjust_float_register_busy (cpu, in_FRi, 2, in_FRj, 2, out_FRk, 2);
  adjust_float_register_busy (cpu, in_FRdoublei, 4, in_FRdoublej, 4, out_FRdoublek, 4);
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_float (cpu, slot);
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
  restore_float_register_busy (cpu, in_FRi, 2, in_FRj, 2, out_FRk, 2);
  restore_float_register_busy (cpu, in_FRdoublei, 4, in_FRdoublej, 4, out_FRdoublek, 4);

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
    fr550_reset_fr_flags (cpu, out_FRk);
  if (dual_FRk >= 0)
    fr550_reset_fr_flags (cpu, dual_FRk);
  if (out_FRdoublek >= 0)
    {
      fr550_reset_fr_flags (cpu, out_FRdoublek);
      if (out_FRdoublek < 63)
	fr550_reset_fr_flags (cpu, out_FRdoublek + 1);
    }
  if (dual_FRdoublek >= 0)
    {
      fr550_reset_fr_flags (cpu, dual_FRdoublek);
      if (dual_FRdoublek < 63)
	fr550_reset_fr_flags (cpu, dual_FRdoublek + 1);
    }

  /* the media point unit resource has a latency of 5 cycles  */
  update_media_resource_latency (cpu, slot, cycles + 5);

  return cycles;
}

int
frvbf_model_fr550_u_float_div (SIM_CPU *cpu, const IDESC *idesc,
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
  adjust_float_register_busy (cpu, in_FRi, 1, in_FRj, 1, out_FRk, 1);
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_float (cpu, slot);
  post_wait_for_fdiv (cpu, slot);
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, out_FRk);
  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    post_wait_for_SPR (cpu, FNER_FOR_FR (out_FRk));
  restore_float_register_busy (cpu, in_FRi, 1, in_FRj, 1, out_FRk, 1);

  /* The latency of FRk will be at least the latency of the other inputs.  */
  /* Once initiated, post-processing will take 9 cycles.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FR_ptime (cpu, out_FRk, 9);

  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      /* FNER has a latency of 9 cycles.  */
      update_SPR_latency (cpu, FNER_FOR_FR (out_FRk), ps->post_wait);
      update_SPR_ptime (cpu, FNER_FOR_FR (out_FRk), 9);
    }

  /* The latency of the fdiv unit will be at least the latency of the other
     inputs.  Once initiated, post-processing will take 9 cycles.  */
  update_fdiv_resource_latency (cpu, slot, ps->post_wait + 9);

  /* the media point unit resource has a latency of 11 cycles  */
  update_media_resource_latency (cpu, slot, cycles + 11);

  fr550_reset_fr_flags (cpu, out_FRk);

  return cycles;
}

int
frvbf_model_fr550_u_float_sqrt (SIM_CPU *cpu, const IDESC *idesc,
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
  adjust_float_register_busy (cpu, -1, 1, in_FRj, 1, out_FRk, 1);
  adjust_float_register_busy (cpu, -1, 1, in_FRdoublej, 2, out_FRdoublek, 2);
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_float (cpu, slot);
  post_wait_for_fsqrt (cpu, slot);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, out_FRk);
  post_wait_for_FRdouble (cpu, in_FRdoublej);
  post_wait_for_FRdouble (cpu, out_FRdoublek);
  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      post_wait_for_SPR (cpu, FNER_FOR_FR (out_FRk));
      post_wait_for_SPR (cpu, FNER_FOR_FR (out_FRdoublek));
    }
  restore_float_register_busy (cpu, -1, 1, in_FRj, 1, out_FRk, 1);
  restore_float_register_busy (cpu, -1, 1, in_FRdoublej, 2, out_FRdoublek, 2);

  /* The latency of FRk will be at least the latency of the other inputs.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FRdouble_latency (cpu, out_FRdoublek, ps->post_wait);

  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      /* FNER has a latency of 14 cycles.  */
      update_SPR_latency (cpu, FNER_FOR_FR (out_FRk), ps->post_wait);
      update_SPR_latency (cpu, FNER_FOR_FR (out_FRdoublek), ps->post_wait);
    }

  /* Once initiated, post-processing will take 14 cycles.  */
  update_FR_ptime (cpu, out_FRk, 14);
  update_FRdouble_ptime (cpu, out_FRdoublek, 14);

  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      /* FNER has a latency of 14 cycles.  */
      update_SPR_ptime (cpu, FNER_FOR_FR (out_FRk), 14);
      update_SPR_ptime (cpu, FNER_FOR_FR (out_FRdoublek), 14);
    }

  /* The latency of the sqrt unit will be the latency of the other
     inputs plus 14 cycles.  */
  update_fsqrt_resource_latency (cpu, slot, ps->post_wait + 14);

  fr550_reset_fr_flags (cpu, out_FRk);
  if (out_FRdoublek != -1)
    {
      fr550_reset_fr_flags (cpu, out_FRdoublek);
      fr550_reset_fr_flags (cpu, out_FRdoublek + 1);
    }

  /* the media point unit resource has a latency of 16 cycles  */
  update_media_resource_latency (cpu, slot, cycles + 16);

  return cycles;
}

int
frvbf_model_fr550_u_float_compare (SIM_CPU *cpu, const IDESC *idesc,
				   int unit_num, int referenced,
				   INT in_FRi, INT in_FRj,
				   INT in_FRdoublei, INT in_FRdoublej,
				   INT out_FCCi_2)
{
  int cycles;
  FRV_PROFILE_STATE *ps;
  FRV_VLIW *vliw;
  int slot;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  adjust_float_register_busy (cpu, in_FRi, 1, in_FRj, 1, -1, 1);
  adjust_float_register_busy (cpu, in_FRdoublei, 2, in_FRdoublej, 2, -1, 1);
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_float (cpu, slot);
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FRdouble (cpu, in_FRdoublei);
  post_wait_for_FRdouble (cpu, in_FRdoublej);
  post_wait_for_CCR (cpu, out_FCCi_2);
  restore_float_register_busy (cpu, in_FRi, 1, in_FRj, 1, -1, 1);
  restore_float_register_busy (cpu, in_FRdoublei, 2, in_FRdoublej, 2, -1, 1);

  /* The latency of FCCi_2 will be the latency of the other inputs plus 2
     cycles.  */
  update_CCR_latency (cpu, out_FCCi_2, ps->post_wait + 2);

  /* the media point unit resource has a latency of 4 cycles  */
  update_media_resource_latency (cpu, slot, cycles + 4);

  set_use_is_ccr_complex (cpu, out_FCCi_2);

  return cycles;
}

int
frvbf_model_fr550_u_float_dual_compare (SIM_CPU *cpu, const IDESC *idesc,
					int unit_num, int referenced,
					INT in_FRi, INT in_FRj,
					INT out_FCCi_2)
{
  int cycles;
  INT dual_FRi;
  INT dual_FRj;
  INT dual_FCCi_2;
  FRV_PROFILE_STATE *ps;
  FRV_VLIW *vliw;
  int slot;

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
  adjust_float_register_busy (cpu, in_FRi, 2, in_FRj, 2, -1, 1);
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_float (cpu, slot);
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, dual_FRi);
  post_wait_for_FR (cpu, dual_FRj);
  post_wait_for_CCR (cpu, out_FCCi_2);
  post_wait_for_CCR (cpu, dual_FCCi_2);
  restore_float_register_busy (cpu, in_FRi, 2, in_FRj, 2, -1, 1);

  /* The latency of FCCi_2 will be the latency of the other inputs plus 3
     cycles.  */
  update_CCR_latency (cpu, out_FCCi_2, ps->post_wait + 3);
  update_CCR_latency (cpu, dual_FCCi_2, ps->post_wait + 3);

  set_use_is_ccr_complex (cpu, out_FCCi_2);
  if (dual_FCCi_2 >= 0)
    set_use_is_ccr_complex (cpu, dual_FCCi_2);

  /* the media point unit resource has a latency of 5 cycles  */
  update_media_resource_latency (cpu, slot, cycles + 5);

  return cycles;
}

int
frvbf_model_fr550_u_float_convert (SIM_CPU *cpu, const IDESC *idesc,
				   int unit_num, int referenced,
				   INT in_FRj, INT in_FRintj, INT in_FRdoublej,
				   INT out_FRk, INT out_FRintk,
				   INT out_FRdoublek)
{
  int cycles;
  FRV_PROFILE_STATE *ps;
  FRV_VLIW *vliw;
  int slot;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  adjust_float_register_busy (cpu, -1, 1, in_FRj, 1, out_FRk, 1);
  adjust_float_register_busy (cpu, -1, 1, in_FRintj, 1, out_FRintk, 1);
  adjust_float_register_busy (cpu, -1, 1, in_FRdoublej, 2, out_FRdoublek, 2);
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_float (cpu, slot);
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
  restore_float_register_busy (cpu, -1, 1, in_FRj, 1, out_FRk, 1);
  restore_float_register_busy (cpu, -1, 1, in_FRintj, 1, out_FRintk, 1);
  restore_float_register_busy (cpu, -1, 1, in_FRdoublej, 2, out_FRdoublek, 2);

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

  /* Once initiated, post-processing will take 2 cycles.  */
  update_FR_ptime (cpu, out_FRk, 2);
  update_FR_ptime (cpu, out_FRintk, 2);
  update_FRdouble_ptime (cpu, out_FRdoublek, 2);

  if (CGEN_ATTR_VALUE(idesc, idesc->attrs, CGEN_INSN_NON_EXCEPTING))
    {
      update_SPR_ptime (cpu, FNER_FOR_FR (out_FRk), 2);
      update_SPR_ptime (cpu, FNER_FOR_FR (out_FRintk), 2);
      update_SPR_ptime (cpu, FNER_FOR_FR (out_FRdoublek), 2);
    }

  /* Mark this use of the register as a floating point op.  */
  if (out_FRk >= 0)
    set_use_is_fr_complex_2 (cpu, out_FRk);
  if (out_FRintk >= 0)
    set_use_is_fr_complex_2 (cpu, out_FRintk);
  if (out_FRdoublek >= 0)
    {
      set_use_is_fr_complex_2 (cpu, out_FRdoublek);
      set_use_is_fr_complex_2 (cpu, out_FRdoublek + 1);
    }

  /* the media point unit resource has a latency of 4 cycles  */
  update_media_resource_latency (cpu, slot, cycles + 4);

  return cycles;
}

int
frvbf_model_fr550_u_spr2gr (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced,
			    INT in_spr, INT out_GRj)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_spr2gr (cpu, idesc, unit_num, referenced,
				     in_spr, out_GRj);
}

int
frvbf_model_fr550_u_gr2spr (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced,
			    INT in_GRj, INT out_spr)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.  */
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
frvbf_model_fr550_u_gr2fr (SIM_CPU *cpu, const IDESC *idesc,
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
	 See Table 14-15 in the LSI.  */
      adjust_float_register_busy (cpu, -1, 1, -1, 1, out_FRk, 1);
      vliw_wait_for_GR (cpu, in_GRj);
      vliw_wait_for_FR (cpu, out_FRk);
      handle_resource_wait (cpu);
      load_wait_for_GR (cpu, in_GRj);
      load_wait_for_FR (cpu, out_FRk);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  /* The latency of FRk is 1 cycles.  */
  cycles = idesc->timing->units[unit_num].done;
  update_FR_latency (cpu, out_FRk, cycles + 1);

  set_use_is_fr_complex_1 (cpu, out_FRk);

  return cycles;
}

int
frvbf_model_fr550_u_swap (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj, INT out_GRk)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.  */
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

  return cycles;
}

int
frvbf_model_fr550_u_fr2fr (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_FRj, INT out_FRk)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 14-15 in the LSI.  */
      adjust_float_register_busy (cpu, -1, 1, in_FRj, 1, out_FRk, 1);
      vliw_wait_for_FR (cpu, in_FRj);
      vliw_wait_for_FR (cpu, out_FRk);
      handle_resource_wait (cpu);
      load_wait_for_FR (cpu, in_FRj);
      load_wait_for_FR (cpu, out_FRk);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  /* The latency of FRj is 2 cycles.  */
  cycles = idesc->timing->units[unit_num].done;
  update_FR_latency (cpu, out_FRk, cycles + 2);

  set_use_is_fr_complex_2 (cpu, out_FRk);

  return cycles;
}

int
frvbf_model_fr550_u_fr2gr (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_FRk, INT out_GRj)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* The entire VLIW insn must wait if there is a dependency on a register
	 which is not ready yet.
	 The latency of the registers may be less than previously recorded,
	 depending on how they were used previously.
	 See Table 14-15 in the LSI.  */
      adjust_float_register_busy (cpu, in_FRk, 1, -1, 1, -1, 1);
      vliw_wait_for_FR (cpu, in_FRk);
      vliw_wait_for_GR (cpu, out_GRj);
      handle_resource_wait (cpu);
      load_wait_for_FR (cpu, in_FRk);
      load_wait_for_GR (cpu, out_GRj);
      trace_vliw_wait_cycles (cpu);
      return 0;
    }

  /* The latency of GRj is 1 cycle.  */
  cycles = idesc->timing->units[unit_num].done;
  update_GR_latency (cpu, out_GRj, cycles + 1);

  return cycles;
}

int
frvbf_model_fr550_u_clrgr (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_GRk)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_clrgr (cpu, idesc, unit_num, referenced, in_GRk);
}

int
frvbf_model_fr550_u_clrfr (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_FRk)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_clrfr (cpu, idesc, unit_num, referenced, in_FRk);
}

int
frvbf_model_fr550_u_commit (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced,
			    INT in_GRk, INT in_FRk)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_commit (cpu, idesc, unit_num, referenced,
				     in_GRk, in_FRk);
}

int
frvbf_model_fr550_u_media (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_FRi, INT in_FRj, INT out_FRk)
{
  int cycles;
  FRV_PROFILE_STATE *ps;
  FRV_VLIW *vliw;
  int slot;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  /* If the previous use of the registers was a media op,
     then their latency may be less than previously recorded.
     See Table 14-15 in the LSI.  */
  adjust_float_register_busy_for_media (cpu, in_FRi, 1, in_FRj, 1, out_FRk, 1);

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_media (cpu, slot);
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, out_FRk);

  /* Restore the busy cycles of the registers we used.  */
  restore_float_register_busy_for_media (cpu, in_FRi, 1, in_FRj, 1, out_FRk, 1);

  /* The latency of tht output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  if (out_FRk >= 0)
    {
      update_FR_latency (cpu, out_FRk, ps->post_wait);
      update_FR_ptime (cpu, out_FRk, 1);
      /* Mark this use of the register as a media op.  */
      set_use_is_fr_complex_1 (cpu, out_FRk);
    }

  /* the floating point unit resource has a latency of 3 cycles  */
  update_float_resource_latency (cpu, slot, cycles + 3);

  return cycles;
}

int
frvbf_model_fr550_u_media_quad (SIM_CPU *cpu, const IDESC *idesc,
				int unit_num, int referenced,
				INT in_FRi, INT in_FRj,
				INT out_FRk)
{
  int cycles;
  INT dual_FRi;
  INT dual_FRj;
  INT dual_FRk;
  FRV_PROFILE_STATE *ps;
  FRV_VLIW *vliw;
  int slot;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  dual_FRi = DUAL_REG (in_FRi);
  dual_FRj = DUAL_REG (in_FRj);
  dual_FRk = DUAL_REG (out_FRk);

  /* The latency of the registers may be less than previously recorded,
     depending on how they were used previously.
     See Table 14-15 in the LSI.  */
  adjust_float_register_busy_for_media (cpu, in_FRi, 2, in_FRj, 2, out_FRk, 2);

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_media (cpu, slot);
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, dual_FRi);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, dual_FRj);
  post_wait_for_FR (cpu, out_FRk);
  post_wait_for_FR (cpu, dual_FRk);

  /* Restore the busy cycles of the registers we used.  */
  restore_float_register_busy_for_media (cpu, in_FRi, 2, in_FRj, 2, out_FRk, 2);

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing take 1 cycle.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FR_ptime (cpu, out_FRk, 1);
  set_use_is_fr_complex_1 (cpu, out_FRk);

  if (dual_FRk >= 0)
    {
      update_FR_latency (cpu, dual_FRk, ps->post_wait);
      update_FR_ptime (cpu, dual_FRk, 1);
      set_use_is_fr_complex_1 (cpu, dual_FRk);
    }

  /* the floating point unit resource has a latency of 3 cycles  */
  update_float_resource_latency (cpu, slot, cycles + 3);

  return cycles;
}

int
frvbf_model_fr550_u_media_dual_expand (SIM_CPU *cpu, const IDESC *idesc,
				       int unit_num, int referenced,
				       INT in_FRi, INT out_FRk)
{
  int cycles;
  INT dual_FRk;
  FRV_PROFILE_STATE *ps;
  FRV_VLIW *vliw;
  int slot;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  /* If the previous use of the registers was a media op,
     then their latency will be less than previously recorded.
     See Table 14-15 in the LSI.  */
  dual_FRk = DUAL_REG (out_FRk);
  adjust_float_register_busy_for_media (cpu, in_FRi, 1, -1, 1, out_FRk, 2);

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_media (cpu, slot);
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, out_FRk);
  post_wait_for_FR (cpu, dual_FRk);

  /* Restore the busy cycles of the registers we used.  */
  restore_float_register_busy_for_media (cpu, in_FRi, 1, -1, 1, out_FRk, 2);

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FR_ptime (cpu, out_FRk, 1);
  set_use_is_fr_complex_1 (cpu, out_FRk);

  if (dual_FRk >= 0)
    {
      update_FR_latency (cpu, dual_FRk, ps->post_wait);
      update_FR_ptime (cpu, dual_FRk, 1);
      set_use_is_fr_complex_1 (cpu, dual_FRk);
    }

  /* the floating point unit resource has a latency of 3 cycles  */
  update_float_resource_latency (cpu, slot, cycles + 3);

  return cycles;
}

int
frvbf_model_fr550_u_media_3_dual (SIM_CPU *cpu, const IDESC *idesc,
				  int unit_num, int referenced,
				  INT in_FRi, INT out_FRk)
{
  int cycles;
  INT dual_FRi;
  FRV_PROFILE_STATE *ps;
  FRV_VLIW *vliw;
  int slot;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  dual_FRi = DUAL_REG (in_FRi);

  /* The latency of the registers may be less than previously recorded,
     depending on how they were used previously.
     See Table 14-15 in the LSI.  */
  adjust_float_register_busy_for_media (cpu, in_FRi, 2, -1, 1, out_FRk, 1);

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_media (cpu, slot);
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, dual_FRi);
  post_wait_for_FR (cpu, out_FRk);

  /* Restore the busy cycles of the registers we used.  */
  restore_float_register_busy_for_media (cpu, in_FRi, 2, -1, 1, out_FRk, 1);

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing takes 1 cycle.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FR_ptime (cpu, out_FRk, 1);

  set_use_is_fr_complex_1 (cpu, out_FRk);

  /* the floating point unit resource has a latency of 3 cycles  */
  update_float_resource_latency (cpu, slot, cycles + 3);

  return cycles;
}

int
frvbf_model_fr550_u_media_3_acc (SIM_CPU *cpu, const IDESC *idesc,
				 int unit_num, int referenced,
				 INT in_FRj, INT in_ACC40Si,
				 INT out_FRk)
{
  int cycles;
  FRV_PROFILE_STATE *ps;
  FRV_VLIW *vliw;
  int slot;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  /* If the previous use of the registers was a media op,
     then their latency will be less than previously recorded.
     See Table 14-15 in the LSI.  */
  adjust_float_register_busy_for_media (cpu, -1, 1, in_FRj, 1, out_FRk, 1);

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_media (cpu, slot);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, out_FRk);
  post_wait_for_ACC (cpu, in_ACC40Si);

  /* Restore the busy cycles of the registers we used.  */
  restore_float_register_busy_for_media (cpu, -1, 1, in_FRj, 1, out_FRk, 1);

  /* The latency of tht output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FR_ptime (cpu, out_FRk, 1);

  set_use_is_fr_complex_1 (cpu, out_FRk);

  /* the floating point unit resource has a latency of 3 cycles  */
  update_float_resource_latency (cpu, slot, cycles + 3);

  return cycles;
}

int
frvbf_model_fr550_u_media_3_acc_dual (SIM_CPU *cpu, const IDESC *idesc,
				      int unit_num, int referenced,
				      INT in_ACC40Si, INT out_FRk)
{
  int cycles;
  FRV_PROFILE_STATE *ps;
  INT ACC40Si_1;
  INT dual_FRk;
  FRV_VLIW *vliw;
  int slot;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ACC40Si_1 = DUAL_REG (in_ACC40Si);
  dual_FRk = DUAL_REG (out_FRk);

  /* If the previous use of the registers was a media op,
     then their latency will be less than previously recorded.
     See Table 14-15 in the LSI.  */
  adjust_float_register_busy_for_media (cpu, -1, 1, -1, 1, out_FRk, 2);

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_media (cpu, slot);
  post_wait_for_ACC (cpu, in_ACC40Si);
  post_wait_for_ACC (cpu, ACC40Si_1);
  post_wait_for_FR (cpu, out_FRk);
  post_wait_for_FR (cpu, dual_FRk);

  /* Restore the busy cycles of the registers we used.  */
  restore_float_register_busy_for_media (cpu, -1, 1, -1, 1, out_FRk, 2);

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FR_ptime (cpu, out_FRk, 1);
  set_use_is_fr_complex_1 (cpu, out_FRk);
  if (dual_FRk >= 0)
    {
      update_FR_latency (cpu, dual_FRk, ps->post_wait);
      update_FR_ptime (cpu, dual_FRk, 1);
      set_use_is_fr_complex_1 (cpu, dual_FRk);
    }

  /* the floating point unit resource has a latency of 3 cycles  */
  update_float_resource_latency (cpu, slot, cycles + 3);

  return cycles;
}

int
frvbf_model_fr550_u_media_3_wtacc (SIM_CPU *cpu, const IDESC *idesc,
				   int unit_num, int referenced,
				   INT in_FRi, INT out_ACC40Sk)
{
  int cycles;
  FRV_PROFILE_STATE *ps;
  FRV_VLIW *vliw;
  int slot;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ps = CPU_PROFILE_STATE (cpu);

  /* The latency of the registers may be less than previously recorded,
     depending on how they were used previously.
     See Table 14-15 in the LSI.  */
  adjust_float_register_busy_for_media (cpu, in_FRi, 1, -1, 1, -1, 1);

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_media (cpu, slot);
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_ACC (cpu, out_ACC40Sk);

  /* Restore the busy cycles of the registers we used.  */
  restore_float_register_busy_for_media (cpu, in_FRi, 1, -1, 1, -1, 1);

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  update_ACC_latency (cpu, out_ACC40Sk, ps->post_wait);
  update_ACC_ptime (cpu, out_ACC40Sk, 1);
  set_use_is_acc_mmac (cpu, out_ACC40Sk);

  /* the floating point unit resource has a latency of 3 cycles  */
  update_float_resource_latency (cpu, slot, cycles + 3);

  return cycles;
}

int
frvbf_model_fr550_u_media_3_mclracc (SIM_CPU *cpu, const IDESC *idesc,
				     int unit_num, int referenced)
{
  int cycles;
  FRV_PROFILE_STATE *ps;
  FRV_VLIW *vliw;
  int slot;
  int i;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ps = CPU_PROFILE_STATE (cpu);

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_media (cpu, slot);

  /* If A was 1 and the accumulator was ACC0, then we must check all
     accumulators. Otherwise just wait for the specified accumulator.  */
  if (ps->mclracc_A && ps->mclracc_acc == 0)
    {
      for (i = 0; i < 8; ++i)
	post_wait_for_ACC (cpu, i);
    }
  else
    post_wait_for_ACC (cpu, ps->mclracc_acc);

  /* The latency of the output registers will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  if (ps->mclracc_A && ps->mclracc_acc == 0)
    {
      for (i = 0; i < 8; ++i)
	{
	  update_ACC_latency (cpu, i, ps->post_wait);
	  update_ACC_ptime (cpu, i, 1);
	  set_use_is_acc_mmac (cpu, i);
	}
    }
  else
    {
      update_ACC_latency (cpu, ps->mclracc_acc, ps->post_wait);
      update_ACC_ptime (cpu, ps->mclracc_acc, 1);
      set_use_is_acc_mmac (cpu, ps->mclracc_acc);
    }

  /* the floating point unit resource has a latency of 3 cycles  */
  update_float_resource_latency (cpu, slot, cycles + 3);

  return cycles;
}

int
frvbf_model_fr550_u_media_set (SIM_CPU *cpu, const IDESC *idesc,
			       int unit_num, int referenced,
			       INT out_FRk)
{
  int cycles;
  FRV_PROFILE_STATE *ps;
  FRV_VLIW *vliw;
  int slot;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  /* If the previous use of the registers was a media op,
     then their latency will be less than previously recorded.
     See Table 14-15 in the LSI.  */
  adjust_float_register_busy_for_media (cpu, -1, 1, -1, 1, out_FRk, 1);

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps = CPU_PROFILE_STATE (cpu);
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_media (cpu, slot);
  post_wait_for_FR (cpu, out_FRk);

  /* Restore the busy cycles of the registers we used.  */
  restore_float_register_busy_for_media (cpu, -1, 1, -1, 1, out_FRk, 1);

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing takes 1 cycle.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FR_ptime (cpu, out_FRk, 1);
  fr550_reset_acc_flags (cpu, out_FRk);

  /* the floating point unit resource has a latency of 3 cycles  */
  update_float_resource_latency (cpu, slot, cycles + 3);

  return cycles;
}

int
frvbf_model_fr550_u_media_4 (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_FRi, INT in_FRj,
			     INT out_ACC40Sk, INT out_ACC40Uk)
{
  int cycles;
  INT dual_ACC40Sk;
  INT dual_ACC40Uk;
  FRV_PROFILE_STATE *ps;
  FRV_VLIW *vliw;
  int slot;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ps = CPU_PROFILE_STATE (cpu);
  dual_ACC40Sk = DUAL_REG (out_ACC40Sk);
  dual_ACC40Uk = DUAL_REG (out_ACC40Uk);

  /* The latency of the registers may be less than previously recorded,
     depending on how they were used previously.
     See Table 14-15 in the LSI.  */
  adjust_acc_busy_for_mmac (cpu, -1, 1, out_ACC40Sk, 2);
  adjust_acc_busy_for_mmac (cpu, -1, 1, out_ACC40Uk, 2);

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_media (cpu, slot);
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_ACC (cpu, out_ACC40Sk);
  post_wait_for_ACC (cpu, dual_ACC40Sk);
  post_wait_for_ACC (cpu, out_ACC40Uk);
  post_wait_for_ACC (cpu, dual_ACC40Uk);

  /* Restore the busy cycles of the registers we used.  */
  restore_acc_busy_for_mmac (cpu, -1, 1, out_ACC40Sk, 2);
  restore_acc_busy_for_mmac (cpu, -1, 1, out_ACC40Uk, 2);

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycles.  */
  if (out_ACC40Sk >= 0)
    {
      update_ACC_latency (cpu, out_ACC40Sk, ps->post_wait + 1);
      set_use_is_acc_mmac (cpu, out_ACC40Sk);
    }
  if (dual_ACC40Sk >= 0)
    {
      update_ACC_latency (cpu, dual_ACC40Sk, ps->post_wait + 1);
      set_use_is_acc_mmac (cpu, dual_ACC40Sk);
    }
  if (out_ACC40Uk >= 0)
    {
      update_ACC_latency (cpu, out_ACC40Uk, ps->post_wait + 1);
      set_use_is_acc_mmac (cpu, out_ACC40Uk);
    }
  if (dual_ACC40Uk >= 0)
    {
      update_ACC_latency (cpu, dual_ACC40Uk, ps->post_wait + 1);
      set_use_is_acc_mmac (cpu, dual_ACC40Uk);
    }

  /* the floating point unit resource has a latency of 3 cycles  */
  update_float_resource_latency (cpu, slot, cycles + 3);

  return cycles;
}

int
frvbf_model_fr550_u_media_4_acc (SIM_CPU *cpu, const IDESC *idesc,
				 int unit_num, int referenced,
				 INT in_ACC40Si, INT out_ACC40Sk)
{
  int cycles;
  INT ACC40Si_1;
  FRV_PROFILE_STATE *ps;
  FRV_VLIW *vliw;
  int slot;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ACC40Si_1 = DUAL_REG (in_ACC40Si);

  ps = CPU_PROFILE_STATE (cpu);
  /* The latency of the registers may be less than previously recorded,
     depending on how they were used previously.
     See Table 14-15 in the LSI.  */
  adjust_acc_busy_for_mmac (cpu, in_ACC40Si, 2, out_ACC40Sk, 1);

  /* The post processing must wait if there is a dependency on a register
     which is not ready yet.  */
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_media (cpu, slot);
  post_wait_for_ACC (cpu, in_ACC40Si);
  post_wait_for_ACC (cpu, ACC40Si_1);
  post_wait_for_ACC (cpu, out_ACC40Sk);

  /* Restore the busy cycles of the registers we used.  */
  restore_acc_busy_for_mmac (cpu, in_ACC40Si, 2, out_ACC40Sk, 1);

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  update_ACC_latency (cpu, out_ACC40Sk, ps->post_wait + 1);
  set_use_is_acc_mmac (cpu, out_ACC40Sk);

  /* the floating point unit resource has a latency of 3 cycles  */
  update_float_resource_latency (cpu, slot, cycles + 3);

  return cycles;
}

int
frvbf_model_fr550_u_media_4_acc_dual (SIM_CPU *cpu, const IDESC *idesc,
				      int unit_num, int referenced,
				      INT in_ACC40Si, INT out_ACC40Sk)
{
  int cycles;
  INT ACC40Si_1;
  INT ACC40Si_2;
  INT ACC40Si_3;
  INT ACC40Sk_1;
  FRV_PROFILE_STATE *ps;
  FRV_VLIW *vliw;
  int slot;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ACC40Si_1 = DUAL_REG (in_ACC40Si);
  ACC40Si_2 = DUAL_REG (ACC40Si_1);
  ACC40Si_3 = DUAL_REG (ACC40Si_2);
  ACC40Sk_1 = DUAL_REG (out_ACC40Sk);

  ps = CPU_PROFILE_STATE (cpu);
  /* The latency of the registers may be less than previously recorded,
     depending on how they were used previously.
     See Table 14-15 in the LSI.  */
  adjust_acc_busy_for_mmac (cpu, in_ACC40Si, 4, out_ACC40Sk, 2);

  /* The post processing must wait if there is a dependency on a register
     which is not ready yet.  */
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_media (cpu, slot);
  post_wait_for_ACC (cpu, in_ACC40Si);
  post_wait_for_ACC (cpu, ACC40Si_1);
  post_wait_for_ACC (cpu, ACC40Si_2);
  post_wait_for_ACC (cpu, ACC40Si_3);
  post_wait_for_ACC (cpu, out_ACC40Sk);
  post_wait_for_ACC (cpu, ACC40Sk_1);

  /* Restore the busy cycles of the registers we used.  */
  restore_acc_busy_for_mmac (cpu, in_ACC40Si, 4, out_ACC40Sk, 2);

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  update_ACC_latency (cpu, out_ACC40Sk, ps->post_wait + 1);
  set_use_is_acc_mmac (cpu, out_ACC40Sk);
  if (ACC40Sk_1 >= 0)
    {
      update_ACC_latency (cpu, ACC40Sk_1, ps->post_wait + 1);
      set_use_is_acc_mmac (cpu, ACC40Sk_1);
    }

  /* the floating point unit resource has a latency of 3 cycles  */
  update_float_resource_latency (cpu, slot, cycles + 3);

  return cycles;
}

int
frvbf_model_fr550_u_media_4_add_sub (SIM_CPU *cpu, const IDESC *idesc,
				     int unit_num, int referenced,
				     INT in_ACC40Si, INT out_ACC40Sk)
{
  int cycles;
  INT ACC40Si_1;
  INT ACC40Sk_1;
  FRV_PROFILE_STATE *ps;
  FRV_VLIW *vliw;
  int slot;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ACC40Si_1 = DUAL_REG (in_ACC40Si);
  ACC40Sk_1 = DUAL_REG (out_ACC40Sk);

  ps = CPU_PROFILE_STATE (cpu);
  /* The latency of the registers may be less than previously recorded,
     depending on how they were used previously.
     See Table 14-15 in the LSI.  */
  adjust_acc_busy_for_mmac (cpu, in_ACC40Si, 2, out_ACC40Sk, 2);

  /* The post processing must wait if there is a dependency on a register
     which is not ready yet.  */
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_media (cpu, slot);
  post_wait_for_ACC (cpu, in_ACC40Si);
  post_wait_for_ACC (cpu, ACC40Si_1);
  post_wait_for_ACC (cpu, out_ACC40Sk);
  post_wait_for_ACC (cpu, ACC40Sk_1);

  /* Restore the busy cycles of the registers we used.  */
  restore_acc_busy_for_mmac (cpu, in_ACC40Si, 2, out_ACC40Sk, 2);

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  update_ACC_latency (cpu, out_ACC40Sk, ps->post_wait + 1);
  set_use_is_acc_mmac (cpu, out_ACC40Sk);
  if (ACC40Sk_1 >= 0)
    {
      update_ACC_latency (cpu, ACC40Sk_1, ps->post_wait + 1);
      set_use_is_acc_mmac (cpu, ACC40Sk_1);
    }

  /* the floating point unit resource has a latency of 3 cycles  */
  update_float_resource_latency (cpu, slot, cycles + 3);

  return cycles;
}

int
frvbf_model_fr550_u_media_4_add_sub_dual (SIM_CPU *cpu, const IDESC *idesc,
					  int unit_num, int referenced,
					  INT in_ACC40Si, INT out_ACC40Sk)
{
  int cycles;
  INT ACC40Si_1;
  INT ACC40Si_2;
  INT ACC40Si_3;
  INT ACC40Sk_1;
  INT ACC40Sk_2;
  INT ACC40Sk_3;
  FRV_PROFILE_STATE *ps;
  FRV_VLIW *vliw;
  int slot;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ACC40Si_1 = DUAL_REG (in_ACC40Si);
  ACC40Si_2 = DUAL_REG (ACC40Si_1);
  ACC40Si_3 = DUAL_REG (ACC40Si_2);
  ACC40Sk_1 = DUAL_REG (out_ACC40Sk);
  ACC40Sk_2 = DUAL_REG (ACC40Sk_1);
  ACC40Sk_3 = DUAL_REG (ACC40Sk_2);

  ps = CPU_PROFILE_STATE (cpu);
  /* The latency of the registers may be less than previously recorded,
     depending on how they were used previously.
     See Table 14-15 in the LSI.  */
  adjust_acc_busy_for_mmac (cpu, in_ACC40Si, 4, out_ACC40Sk, 4);

  /* The post processing must wait if there is a dependency on a register
     which is not ready yet.  */
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_media (cpu, slot);
  post_wait_for_ACC (cpu, in_ACC40Si);
  post_wait_for_ACC (cpu, ACC40Si_1);
  post_wait_for_ACC (cpu, ACC40Si_2);
  post_wait_for_ACC (cpu, ACC40Si_3);
  post_wait_for_ACC (cpu, out_ACC40Sk);
  post_wait_for_ACC (cpu, ACC40Sk_1);
  post_wait_for_ACC (cpu, ACC40Sk_2);
  post_wait_for_ACC (cpu, ACC40Sk_3);

  /* Restore the busy cycles of the registers we used.  */
  restore_acc_busy_for_mmac (cpu, in_ACC40Si, 4, out_ACC40Sk, 4);

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  update_ACC_latency (cpu, out_ACC40Sk, ps->post_wait + 1);
  set_use_is_acc_mmac (cpu, out_ACC40Sk);
  if (ACC40Sk_1 >= 0)
    {
      update_ACC_latency (cpu, ACC40Sk_1, ps->post_wait + 1);
      set_use_is_acc_mmac (cpu, ACC40Sk_1);
    }
  if (ACC40Sk_2 >= 0)
    {
      update_ACC_latency (cpu, ACC40Sk_2, ps->post_wait + 1);
      set_use_is_acc_mmac (cpu, ACC40Sk_2);
    }
  if (ACC40Sk_3 >= 0)
    {
      update_ACC_latency (cpu, ACC40Sk_3, ps->post_wait + 1);
      set_use_is_acc_mmac (cpu, ACC40Sk_3);
    }

  /* the floating point unit resource has a latency of 3 cycles  */
  update_float_resource_latency (cpu, slot, cycles + 3);

  return cycles;
}

int
frvbf_model_fr550_u_media_4_quad (SIM_CPU *cpu, const IDESC *idesc,
				  int unit_num, int referenced,
				  INT in_FRi, INT in_FRj,
				  INT out_ACC40Sk, INT out_ACC40Uk)
{
  int cycles;
  INT dual_FRi;
  INT dual_FRj;
  INT ACC40Sk_1;
  INT ACC40Sk_2;
  INT ACC40Sk_3;
  INT ACC40Uk_1;
  INT ACC40Uk_2;
  INT ACC40Uk_3;
  FRV_PROFILE_STATE *ps;
  FRV_VLIW *vliw;
  int slot;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  dual_FRi = DUAL_REG (in_FRi);
  dual_FRj = DUAL_REG (in_FRj);
  ACC40Sk_1 = DUAL_REG (out_ACC40Sk);
  ACC40Sk_2 = DUAL_REG (ACC40Sk_1);
  ACC40Sk_3 = DUAL_REG (ACC40Sk_2);
  ACC40Uk_1 = DUAL_REG (out_ACC40Uk);
  ACC40Uk_2 = DUAL_REG (ACC40Uk_1);
  ACC40Uk_3 = DUAL_REG (ACC40Uk_2);

  ps = CPU_PROFILE_STATE (cpu);
  /* The latency of the registers may be less than previously recorded,
     depending on how they were used previously.
     See Table 14-15 in the LSI.  */
  adjust_float_register_busy_for_media (cpu, in_FRi, 2, in_FRj, 2, -1, 1);
  adjust_acc_busy_for_mmac (cpu, -1, 1, out_ACC40Sk, 4);
  adjust_acc_busy_for_mmac (cpu, -1, 1, out_ACC40Uk, 4);

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  vliw = CPU_VLIW (cpu);
  slot = vliw->next_slot - 1;
  slot = (*vliw->current_vliw)[slot] - UNIT_FM0;
  post_wait_for_media (cpu, slot);
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, dual_FRi);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, dual_FRj);
  post_wait_for_ACC (cpu, out_ACC40Sk);
  post_wait_for_ACC (cpu, ACC40Sk_1);
  post_wait_for_ACC (cpu, ACC40Sk_2);
  post_wait_for_ACC (cpu, ACC40Sk_3);
  post_wait_for_ACC (cpu, out_ACC40Uk);
  post_wait_for_ACC (cpu, ACC40Uk_1);
  post_wait_for_ACC (cpu, ACC40Uk_2);
  post_wait_for_ACC (cpu, ACC40Uk_3);

  /* Restore the busy cycles of the registers we used.  */
  restore_float_register_busy_for_media (cpu, in_FRi, 2, in_FRj, 2, -1, 1);
  restore_acc_busy_for_mmac (cpu, -1, 1, out_ACC40Sk, 4);
  restore_acc_busy_for_mmac (cpu, -1, 1, out_ACC40Uk, 4);

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  if (out_ACC40Sk >= 0)
    {
      update_ACC_latency (cpu, out_ACC40Sk, ps->post_wait + 1);

      set_use_is_acc_mmac (cpu, out_ACC40Sk);
      if (ACC40Sk_1 >= 0)
	{
	  update_ACC_latency (cpu, ACC40Sk_1, ps->post_wait + 1);

	  set_use_is_acc_mmac (cpu, ACC40Sk_1);
	}
      if (ACC40Sk_2 >= 0)
	{
	  update_ACC_latency (cpu, ACC40Sk_2, ps->post_wait + 1);

	  set_use_is_acc_mmac (cpu, ACC40Sk_2);
	}
      if (ACC40Sk_3 >= 0)
	{
	  update_ACC_latency (cpu, ACC40Sk_3, ps->post_wait + 1);

	  set_use_is_acc_mmac (cpu, ACC40Sk_3);
	}
    }
  else if (out_ACC40Uk >= 0)
    {
      update_ACC_latency (cpu, out_ACC40Uk, ps->post_wait + 1);

      set_use_is_acc_mmac (cpu, out_ACC40Uk);
      if (ACC40Uk_1 >= 0)
	{
	  update_ACC_latency (cpu, ACC40Uk_1, ps->post_wait + 1);

	  set_use_is_acc_mmac (cpu, ACC40Uk_1);
	}
      if (ACC40Uk_2 >= 0)
	{
	  update_ACC_latency (cpu, ACC40Uk_2, ps->post_wait + 1);

	  set_use_is_acc_mmac (cpu, ACC40Uk_2);
	}
      if (ACC40Uk_3 >= 0)
	{
	  update_ACC_latency (cpu, ACC40Uk_3, ps->post_wait + 1);

	  set_use_is_acc_mmac (cpu, ACC40Uk_3);
	}
    }

  /* the floating point unit resource has a latency of 3 cycles  */
  update_float_resource_latency (cpu, slot, cycles + 3);

  return cycles;
}

#endif /* WITH_PROFILE_MODEL_P */
