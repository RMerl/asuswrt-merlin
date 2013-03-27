/* frv simulator fr400 dependent profiling code.

   Copyright (C) 2001, 2007 Free Software Foundation, Inc.
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
#include "profile-fr400.h"

/* These functions get and set flags representing the use of
   registers/resources.  */
static void set_use_not_fp_load (SIM_CPU *, INT);
static void set_use_not_media_p4 (SIM_CPU *, INT);
static void set_use_not_media_p6 (SIM_CPU *, INT);

static void set_acc_use_not_media_p2 (SIM_CPU *, INT);
static void set_acc_use_not_media_p4 (SIM_CPU *, INT);

void
fr400_reset_gr_flags (SIM_CPU *cpu, INT fr)
{
  set_use_not_gr_complex (cpu, fr);
}

void
fr400_reset_fr_flags (SIM_CPU *cpu, INT fr)
{
  set_use_not_fp_load  (cpu, fr);
  set_use_not_media_p4 (cpu, fr);
  set_use_not_media_p6 (cpu, fr);
}

void
fr400_reset_acc_flags (SIM_CPU *cpu, INT acc)
{
  set_acc_use_not_media_p2 (cpu, acc);
  set_acc_use_not_media_p4 (cpu, acc);
}

static void
set_use_is_fp_load (SIM_CPU *cpu, INT fr, INT fr_double)
{
  MODEL_FR400_DATA *d = CPU_MODEL_DATA (cpu);
  if (fr != -1)
    {
      fr400_reset_fr_flags (cpu, fr);
      d->cur_fp_load |= (((DI)1) << fr);
    }
  if (fr_double != -1)
    {
      fr400_reset_fr_flags (cpu, fr_double);
      d->cur_fp_load |= (((DI)1) << fr_double);
      if (fr_double < 63)
	{
	  fr400_reset_fr_flags (cpu, fr_double + 1);
	  d->cur_fp_load |= (((DI)1) << (fr_double + 1));
	}
    }

}

static void
set_use_not_fp_load (SIM_CPU *cpu, INT fr)
{
  MODEL_FR400_DATA *d = CPU_MODEL_DATA (cpu);
  if (fr != -1)
    d->cur_fp_load &= ~(((DI)1) << fr);
}

static int
use_is_fp_load (SIM_CPU *cpu, INT fr)
{
  MODEL_FR400_DATA *d = CPU_MODEL_DATA (cpu);
  if (fr != -1)
    return (d->prev_fp_load >> fr) & 1;
  return 0;
}

static void
set_acc_use_is_media_p2 (SIM_CPU *cpu, INT acc)
{
  MODEL_FR400_DATA *d = CPU_MODEL_DATA (cpu);
  if (acc != -1)
    {
      fr400_reset_acc_flags (cpu, acc);
      d->cur_acc_p2 |= (((DI)1) << acc);
    }
}

static void
set_acc_use_not_media_p2 (SIM_CPU *cpu, INT acc)
{
  MODEL_FR400_DATA *d = CPU_MODEL_DATA (cpu);
  if (acc != -1)
    d->cur_acc_p2 &= ~(((DI)1) << acc);
}

static int
acc_use_is_media_p2 (SIM_CPU *cpu, INT acc)
{
  MODEL_FR400_DATA *d = CPU_MODEL_DATA (cpu);
  if (acc != -1)
    return d->cur_acc_p2 & (((DI)1) << acc);
  return 0;
}

static void
set_use_is_media_p4 (SIM_CPU *cpu, INT fr)
{
  MODEL_FR400_DATA *d = CPU_MODEL_DATA (cpu);
  if (fr != -1)
    {
      fr400_reset_fr_flags (cpu, fr);
      d->cur_fr_p4 |= (((DI)1) << fr);
    }
}

static void
set_use_not_media_p4 (SIM_CPU *cpu, INT fr)
{
  MODEL_FR400_DATA *d = CPU_MODEL_DATA (cpu);
  if (fr != -1)
    d->cur_fr_p4 &= ~(((DI)1) << fr);
}

static int
use_is_media_p4 (SIM_CPU *cpu, INT fr)
{
  MODEL_FR400_DATA *d = CPU_MODEL_DATA (cpu);
  if (fr != -1)
    return d->cur_fr_p4 & (((DI)1) << fr);
  return 0;
}

static void
set_acc_use_is_media_p4 (SIM_CPU *cpu, INT acc)
{
  MODEL_FR400_DATA *d = CPU_MODEL_DATA (cpu);
  if (acc != -1)
    {
      fr400_reset_acc_flags (cpu, acc);
      d->cur_acc_p4 |= (((DI)1) << acc);
    }
}

static void
set_acc_use_not_media_p4 (SIM_CPU *cpu, INT acc)
{
  MODEL_FR400_DATA *d = CPU_MODEL_DATA (cpu);
  if (acc != -1)
    d->cur_acc_p4 &= ~(((DI)1) << acc);
}

static int
acc_use_is_media_p4 (SIM_CPU *cpu, INT acc)
{
  MODEL_FR400_DATA *d = CPU_MODEL_DATA (cpu);
  if (acc != -1)
    return d->cur_acc_p4 & (((DI)1) << acc);
  return 0;
}

static void
set_use_is_media_p6 (SIM_CPU *cpu, INT fr)
{
  MODEL_FR400_DATA *d = CPU_MODEL_DATA (cpu);
  if (fr != -1)
    {
      fr400_reset_fr_flags (cpu, fr);
      d->cur_fr_p6 |= (((DI)1) << fr);
    }
}

static void
set_use_not_media_p6 (SIM_CPU *cpu, INT fr)
{
  MODEL_FR400_DATA *d = CPU_MODEL_DATA (cpu);
  if (fr != -1)
    d->cur_fr_p6 &= ~(((DI)1) << fr);
}

static int
use_is_media_p6 (SIM_CPU *cpu, INT fr)
{
  MODEL_FR400_DATA *d = CPU_MODEL_DATA (cpu);
  if (fr != -1)
    return d->cur_fr_p6 & (((DI)1) << fr);
  return 0;
}

/* Initialize cycle counting for an insn.
   FIRST_P is non-zero if this is the first insn in a set of parallel
   insns.  */
void
fr400_model_insn_before (SIM_CPU *cpu, int first_p)
{
  if (first_p)
    {
      MODEL_FR400_DATA *d = CPU_MODEL_DATA (cpu);
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      ps->cur_gr_complex = ps->prev_gr_complex;
      d->cur_fp_load = d->prev_fp_load;
      d->cur_fr_p4 = d->prev_fr_p4;
      d->cur_fr_p6 = d->prev_fr_p6;
      d->cur_acc_p2 = d->prev_acc_p2;
      d->cur_acc_p4 = d->prev_acc_p4;
    }
}

/* Record the cycles computed for an insn.
   LAST_P is non-zero if this is the last insn in a set of parallel insns,
   and we update the total cycle count.
   CYCLES is the cycle count of the insn.  */
void
fr400_model_insn_after (SIM_CPU *cpu, int last_p, int cycles)
{
  if (last_p)
    {
      MODEL_FR400_DATA *d = CPU_MODEL_DATA (cpu);
      FRV_PROFILE_STATE *ps = CPU_PROFILE_STATE (cpu);
      ps->prev_gr_complex = ps->cur_gr_complex;
      d->prev_fp_load = d->cur_fp_load;
      d->prev_fr_p4 = d->cur_fr_p4;
      d->prev_fr_p6 = d->cur_fr_p6;
      d->prev_acc_p2 = d->cur_acc_p2;
      d->prev_acc_p4 = d->cur_acc_p4;
    }
}

int
frvbf_model_fr400_u_exec (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced)
{
  return idesc->timing->units[unit_num].done;
}

int
frvbf_model_fr400_u_integer (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_GRi, INT in_GRj, INT out_GRk,
			     INT out_ICCi_1)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_integer (cpu, idesc, unit_num, referenced,
				      in_GRi, in_GRj, out_GRk, out_ICCi_1);
}

int
frvbf_model_fr400_u_imul (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj, INT out_GRk, INT out_ICCi_1)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_imul (cpu, idesc, unit_num, referenced,
				   in_GRi, in_GRj, out_GRk, out_ICCi_1);
}

int
frvbf_model_fr400_u_idiv (SIM_CPU *cpu, const IDESC *idesc,
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

  /* ICCi_1 has a latency of 18 cycles.  */
  update_CCR_latency (cpu, out_ICCi_1, cycles + 18);

  /* the idiv resource has a latency of 18 cycles!  */
  update_idiv_resource_latency (cpu, slot, cycles + 18);

  return cycles;
}

int
frvbf_model_fr400_u_branch (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced,
			    INT in_GRi, INT in_GRj,
			    INT in_ICCi_2, INT in_ICCi_3)
{
#define BRANCH_PREDICTED(ps) ((ps)->branch_hint & 2)
  FRV_PROFILE_STATE *ps;
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* Modelling for this unit is the same as for fr500 in pass 1.  */
      return frvbf_model_fr500_u_branch (cpu, idesc, unit_num, referenced,
					 in_GRi, in_GRj, in_ICCi_2, in_ICCi_3);
    }

  cycles = idesc->timing->units[unit_num].done;

  /* Compute the branch penalty, based on the the prediction and the out
     come.  When counting branches taken or not taken, don't consider branches
     after the first taken branch in a vliw insn.  */
  ps = CPU_PROFILE_STATE (cpu);
  if (! ps->vliw_branch_taken)
    {
      int penalty;
      /* (1 << 4): The pc is the 5th element in inputs, outputs.
	 ??? can be cleaned up */
      PROFILE_DATA *p = CPU_PROFILE_DATA (cpu);
      int taken = (referenced & (1 << 4)) != 0;
      if (taken)
	{
	  ++PROFILE_MODEL_TAKEN_COUNT (p);
	  ps->vliw_branch_taken = 1;
	  if (BRANCH_PREDICTED (ps))
	    penalty = 1;
	  else
	    penalty = 3;
	}
      else
	{
	  ++PROFILE_MODEL_UNTAKEN_COUNT (p);
	  if (BRANCH_PREDICTED (ps))
	    penalty = 3;
	  else
	    penalty = 0;
	}
      if (penalty > 0)
	{
	  /* Additional 1 cycle penalty if the branch address is not 8 byte
	     aligned.  */
	  if (ps->branch_address & 7)
	    ++penalty;
	  update_branch_penalty (cpu, penalty);
	  PROFILE_MODEL_CTI_STALL_CYCLES (p) += penalty;
	}
    }

  return cycles;
}

int
frvbf_model_fr400_u_trap (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj,
			  INT in_ICCi_2, INT in_FCCi_2)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_trap (cpu, idesc, unit_num, referenced,
				   in_GRi, in_GRj, in_ICCi_2, in_FCCi_2);
}

int
frvbf_model_fr400_u_check (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_ICCi_3, INT in_FCCi_3)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_check (cpu, idesc, unit_num, referenced,
				    in_ICCi_3, in_FCCi_3);
}

int
frvbf_model_fr400_u_set_hilo (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT out_GRkhi, INT out_GRklo)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_set_hilo (cpu, idesc, unit_num, referenced,
				       out_GRkhi, out_GRklo);
}

int
frvbf_model_fr400_u_gr_load (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_GRi, INT in_GRj,
			     INT out_GRk, INT out_GRdoublek)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_gr_load (cpu, idesc, unit_num, referenced,
				      in_GRi, in_GRj, out_GRk, out_GRdoublek);
}

int
frvbf_model_fr400_u_gr_store (SIM_CPU *cpu, const IDESC *idesc,
			      int unit_num, int referenced,
			      INT in_GRi, INT in_GRj,
			      INT in_GRk, INT in_GRdoublek)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_gr_store (cpu, idesc, unit_num, referenced,
				       in_GRi, in_GRj, in_GRk, in_GRdoublek);
}

int
frvbf_model_fr400_u_fr_load (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_GRi, INT in_GRj,
			     INT out_FRk, INT out_FRdoublek)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* Pass 1 is the same as for fr500.  */
      return frvbf_model_fr500_u_fr_load (cpu, idesc, unit_num, referenced,
					  in_GRi, in_GRj, out_FRk,
					  out_FRdoublek);
    }

  cycles = idesc->timing->units[unit_num].done;

  /* The latency of FRk for a load will depend on how long it takes to retrieve
     the the data from the cache or memory.  */
  update_FR_latency_for_load (cpu, out_FRk, cycles);
  update_FRdouble_latency_for_load (cpu, out_FRdoublek, cycles);

  set_use_is_fp_load (cpu, out_FRk, out_FRdoublek);

  return cycles;
}

int
frvbf_model_fr400_u_fr_store (SIM_CPU *cpu, const IDESC *idesc,
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
	  if (use_is_media_p4 (cpu, in_FRk) || use_is_media_p6 (cpu, in_FRk))
	    decrease_FR_busy (cpu, in_FRk, 1);
	  else
	    enforce_full_fr_latency (cpu, in_FRk);
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
frvbf_model_fr400_u_swap (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj, INT out_GRk)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_swap (cpu, idesc, unit_num, referenced,
				   in_GRi, in_GRj, out_GRk);
}

int
frvbf_model_fr400_u_fr2gr (SIM_CPU *cpu, const IDESC *idesc,
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
	 See Table 13-8 in the LSI.  */
      if (in_FRk >= 0)
	{
	  if (use_is_media_p4 (cpu, in_FRk) || use_is_media_p6 (cpu, in_FRk))
	    decrease_FR_busy (cpu, in_FRk, 1);
	  else
	    enforce_full_fr_latency (cpu, in_FRk);
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
frvbf_model_fr400_u_spr2gr (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_spr, INT out_GRj)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_spr2gr (cpu, idesc, unit_num, referenced,
				     in_spr, out_GRj);
}

int
frvbf_model_fr400_u_gr2fr (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_GRj, INT out_FRk)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* Pass 1 is the same as for fr500.  */
      frvbf_model_fr500_u_gr2fr (cpu, idesc, unit_num, referenced,
				 in_GRj, out_FRk);
    }

  /* The latency of FRk is 1 cycles.  */
  cycles = idesc->timing->units[unit_num].done;
  update_FR_latency (cpu, out_FRk, cycles + 1);

  return cycles;
}

int
frvbf_model_fr400_u_gr2spr (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced,
			    INT in_GRj, INT out_spr)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_gr2spr (cpu, idesc, unit_num, referenced,
				     in_GRj, out_spr);
}

int
frvbf_model_fr400_u_media_1 (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_FRi, INT in_FRj,
			     INT out_FRk)
{
  int cycles;
  FRV_PROFILE_STATE *ps;
  const CGEN_INSN *insn;
  int busy_adjustment[] = {0, 0};
  int *fr;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ps = CPU_PROFILE_STATE (cpu);
  insn = idesc->idata;

  /* The latency of the registers may be less than previously recorded,
     depending on how they were used previously.
     See Table 13-8 in the LSI.  */
  if (in_FRi >= 0)
    {
      if (use_is_fp_load (cpu, in_FRi))
	{
	  busy_adjustment[0] = 1;
	  decrease_FR_busy (cpu, in_FRi, busy_adjustment[0]);
	}
      else
	enforce_full_fr_latency (cpu, in_FRi);
    }
  if (in_FRj >= 0 && in_FRj != in_FRi)
    {
      if (use_is_fp_load (cpu, in_FRj))
	{
	  busy_adjustment[1] = 1;
	  decrease_FR_busy (cpu, in_FRj, busy_adjustment[1]);
	}
      else
	enforce_full_fr_latency (cpu, in_FRj);
    }

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, out_FRk);

  /* Restore the busy cycles of the registers we used.  */
  fr = ps->fr_busy;
  if (in_FRi >= 0)
    fr[in_FRi] += busy_adjustment[0];
  if (in_FRj >= 0)
    fr[in_FRj] += busy_adjustment[1];

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing has no latency.  */
  if (out_FRk >= 0)
    {
      update_FR_latency (cpu, out_FRk, ps->post_wait);
      update_FR_ptime (cpu, out_FRk, 0);
    }

  return cycles;
}

int
frvbf_model_fr400_u_media_1_quad (SIM_CPU *cpu, const IDESC *idesc,
				  int unit_num, int referenced,
				  INT in_FRi, INT in_FRj,
				  INT out_FRk)
{
  int cycles;
  INT dual_FRi;
  INT dual_FRj;
  INT dual_FRk;
  FRV_PROFILE_STATE *ps;
  int busy_adjustment[] = {0, 0, 0, 0};
  int *fr;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ps = CPU_PROFILE_STATE (cpu);
  dual_FRi = DUAL_REG (in_FRi);
  dual_FRj = DUAL_REG (in_FRj);
  dual_FRk = DUAL_REG (out_FRk);

  /* The latency of the registers may be less than previously recorded,
     depending on how they were used previously.
     See Table 13-8 in the LSI.  */
  if (use_is_fp_load (cpu, in_FRi))
    {
      busy_adjustment[0] = 1;
      decrease_FR_busy (cpu, in_FRi, busy_adjustment[0]);
    }
  else
    enforce_full_fr_latency (cpu, in_FRi);
  if (dual_FRi >= 0 && use_is_fp_load (cpu, dual_FRi))
    {
      busy_adjustment[1] = 1;
      decrease_FR_busy (cpu, dual_FRi, busy_adjustment[1]);
    }
  else
    enforce_full_fr_latency (cpu, dual_FRi);
  if (in_FRj != in_FRi)
    {
      if (use_is_fp_load (cpu, in_FRj))
	{
	  busy_adjustment[2] = 1;
	  decrease_FR_busy (cpu, in_FRj, busy_adjustment[2]);
	}
      else
	enforce_full_fr_latency (cpu, in_FRj);
      if (dual_FRj >= 0 && use_is_fp_load (cpu, dual_FRj))
	{
	  busy_adjustment[3] = 1;
	  decrease_FR_busy (cpu, dual_FRj, busy_adjustment[3]);
	}
      else
	enforce_full_fr_latency (cpu, dual_FRj);
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

  /* The latency of the output register will be at least the latency of the
     other inputs.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);

  /* Once initiated, post-processing has no latency.  */
  update_FR_ptime (cpu, out_FRk, 0);

  if (dual_FRk >= 0)
    {
      update_FR_latency (cpu, dual_FRk, ps->post_wait);
      update_FR_ptime (cpu, dual_FRk, 0);
    }

  return cycles;
}

int
frvbf_model_fr400_u_media_hilo (SIM_CPU *cpu, const IDESC *idesc,
				int unit_num, int referenced,
				INT out_FRkhi, INT out_FRklo)
{
  int cycles;
  FRV_PROFILE_STATE *ps;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ps = CPU_PROFILE_STATE (cpu);

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, out_FRkhi);
  post_wait_for_FR (cpu, out_FRklo);

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing has no latency.  */
  if (out_FRkhi >= 0)
    {
      update_FR_latency (cpu, out_FRkhi, ps->post_wait);
      update_FR_ptime (cpu, out_FRkhi, 0);
    }
  if (out_FRklo >= 0)
    {
      update_FR_latency (cpu, out_FRklo, ps->post_wait);
      update_FR_ptime (cpu, out_FRklo, 0);
    }

  return cycles;
}

int
frvbf_model_fr400_u_media_2 (SIM_CPU *cpu, const IDESC *idesc,
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

  /* The latency of the registers may be less than previously recorded,
     depending on how they were used previously.
     See Table 13-8 in the LSI.  */
  if (in_FRi >= 0)
    {
      if (use_is_fp_load (cpu, in_FRi))
	{
	  busy_adjustment[0] = 1;
	  decrease_FR_busy (cpu, in_FRi, busy_adjustment[0]);
	}
      else
	enforce_full_fr_latency (cpu, in_FRi);
    }
  if (in_FRj >= 0 && in_FRj != in_FRi)
    {
      if (use_is_fp_load (cpu, in_FRj))
	{
	  busy_adjustment[1] = 1;
	  decrease_FR_busy (cpu, in_FRj, busy_adjustment[1]);
	}
      else
	enforce_full_fr_latency (cpu, in_FRj);
    }
  if (out_ACC40Sk >= 0)
    {
      if (acc_use_is_media_p2 (cpu, out_ACC40Sk))
	{
	  busy_adjustment[2] = 1;
	  decrease_ACC_busy (cpu, out_ACC40Sk, busy_adjustment[2]);
	}
    }
  if (dual_ACC40Sk >= 0)
    {
      if (acc_use_is_media_p2 (cpu, dual_ACC40Sk))
	{
	  busy_adjustment[3] = 1;
	  decrease_ACC_busy (cpu, dual_ACC40Sk, busy_adjustment[3]);
	}
    }
  if (out_ACC40Uk >= 0)
    {
      if (acc_use_is_media_p2 (cpu, out_ACC40Uk))
	{
	  busy_adjustment[4] = 1;
	  decrease_ACC_busy (cpu, out_ACC40Uk, busy_adjustment[4]);
	}
    }
  if (dual_ACC40Uk >= 0)
    {
      if (acc_use_is_media_p2 (cpu, dual_ACC40Uk))
	{
	  busy_adjustment[5] = 1;
	  decrease_ACC_busy (cpu, dual_ACC40Uk, busy_adjustment[5]);
	}
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

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycles.  */
  if (out_ACC40Sk >= 0)
    {
      update_ACC_latency (cpu, out_ACC40Sk, ps->post_wait + 1);
      set_acc_use_is_media_p2 (cpu, out_ACC40Sk);
    }
  if (dual_ACC40Sk >= 0)
    {
      update_ACC_latency (cpu, dual_ACC40Sk, ps->post_wait + 1);
      set_acc_use_is_media_p2 (cpu, dual_ACC40Sk);
    }
  if (out_ACC40Uk >= 0)
    {
      update_ACC_latency (cpu, out_ACC40Uk, ps->post_wait + 1);
      set_acc_use_is_media_p2 (cpu, out_ACC40Uk);
    }
  if (dual_ACC40Uk >= 0)
    {
      update_ACC_latency (cpu, dual_ACC40Uk, ps->post_wait + 1);
      set_acc_use_is_media_p2 (cpu, dual_ACC40Uk);
    }

  return cycles;
}

int
frvbf_model_fr400_u_media_2_quad (SIM_CPU *cpu, const IDESC *idesc,
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
  int busy_adjustment[] = {0, 0, 0, 0, 0, 0, 0 ,0};
  int *fr;
  int *acc;

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
     See Table 13-8 in the LSI.  */
  if (use_is_fp_load (cpu, in_FRi))
    {
      busy_adjustment[0] = 1;
      decrease_FR_busy (cpu, in_FRi, busy_adjustment[0]);
    }
  else
    enforce_full_fr_latency (cpu, in_FRi);
  if (dual_FRi >= 0 && use_is_fp_load (cpu, dual_FRi))
    {
      busy_adjustment[1] = 1;
      decrease_FR_busy (cpu, dual_FRi, busy_adjustment[1]);
    }
  else
    enforce_full_fr_latency (cpu, dual_FRi);
  if (in_FRj != in_FRi)
    {
      if (use_is_fp_load (cpu, in_FRj))
	{
	  busy_adjustment[2] = 1;
	  decrease_FR_busy (cpu, in_FRj, busy_adjustment[2]);
	}
      else
	enforce_full_fr_latency (cpu, in_FRj);
      if (dual_FRj >= 0 && use_is_fp_load (cpu, dual_FRj))
	{
	  busy_adjustment[3] = 1;
	  decrease_FR_busy (cpu, dual_FRj, busy_adjustment[3]);
	}
      else
	enforce_full_fr_latency (cpu, dual_FRj);
    }
  if (out_ACC40Sk >= 0)
    {
      if (acc_use_is_media_p2 (cpu, out_ACC40Sk))
	{
	  busy_adjustment[4] = 1;
	  decrease_ACC_busy (cpu, out_ACC40Sk, busy_adjustment[4]);
	}
      if (ACC40Sk_1 >= 0)
	{
	  if (acc_use_is_media_p2 (cpu, ACC40Sk_1))
	    {
	      busy_adjustment[5] = 1;
	      decrease_ACC_busy (cpu, ACC40Sk_1, busy_adjustment[5]);
	    }
	}
      if (ACC40Sk_2 >= 0)
	{
	  if (acc_use_is_media_p2 (cpu, ACC40Sk_2))
	    {
	      busy_adjustment[6] = 1;
	      decrease_ACC_busy (cpu, ACC40Sk_2, busy_adjustment[6]);
	    }
	}
      if (ACC40Sk_3 >= 0)
	{
	  if (acc_use_is_media_p2 (cpu, ACC40Sk_3))
	    {
	      busy_adjustment[7] = 1;
	      decrease_ACC_busy (cpu, ACC40Sk_3, busy_adjustment[7]);
	    }
	}
    }
  else if (out_ACC40Uk >= 0)
    {
      if (acc_use_is_media_p2 (cpu, out_ACC40Uk))
	{
	  busy_adjustment[4] = 1;
	  decrease_ACC_busy (cpu, out_ACC40Uk, busy_adjustment[4]);
	}
      if (ACC40Uk_1 >= 0)
	{
	  if (acc_use_is_media_p2 (cpu, ACC40Uk_1))
	    {
	      busy_adjustment[5] = 1;
	      decrease_ACC_busy (cpu, ACC40Uk_1, busy_adjustment[5]);
	    }
	}
      if (ACC40Uk_2 >= 0)
	{
	  if (acc_use_is_media_p2 (cpu, ACC40Uk_2))
	    {
	      busy_adjustment[6] = 1;
	      decrease_ACC_busy (cpu, ACC40Uk_2, busy_adjustment[6]);
	    }
	}
      if (ACC40Uk_3 >= 0)
	{
	  if (acc_use_is_media_p2 (cpu, ACC40Uk_3))
	    {
	      busy_adjustment[7] = 1;
	      decrease_ACC_busy (cpu, ACC40Uk_3, busy_adjustment[7]);
	    }
	}
    }

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
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
  fr = ps->fr_busy;
  acc = ps->acc_busy;
  fr[in_FRi] += busy_adjustment[0];
  if (dual_FRi >= 0)
    fr[dual_FRi] += busy_adjustment[1];
  fr[in_FRj] += busy_adjustment[2];
  if (dual_FRj > 0)
    fr[dual_FRj] += busy_adjustment[3];
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

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  if (out_ACC40Sk >= 0)
    {
      update_ACC_latency (cpu, out_ACC40Sk, ps->post_wait + 1);

      set_acc_use_is_media_p2 (cpu, out_ACC40Sk);
      if (ACC40Sk_1 >= 0)
	{
	  update_ACC_latency (cpu, ACC40Sk_1, ps->post_wait + 1);

	  set_acc_use_is_media_p2 (cpu, ACC40Sk_1);
	}
      if (ACC40Sk_2 >= 0)
	{
	  update_ACC_latency (cpu, ACC40Sk_2, ps->post_wait + 1);

	  set_acc_use_is_media_p2 (cpu, ACC40Sk_2);
	}
      if (ACC40Sk_3 >= 0)
	{
	  update_ACC_latency (cpu, ACC40Sk_3, ps->post_wait + 1);

	  set_acc_use_is_media_p2 (cpu, ACC40Sk_3);
	}
    }
  else if (out_ACC40Uk >= 0)
    {
      update_ACC_latency (cpu, out_ACC40Uk, ps->post_wait + 1);

      set_acc_use_is_media_p2 (cpu, out_ACC40Uk);
      if (ACC40Uk_1 >= 0)
	{
	  update_ACC_latency (cpu, ACC40Uk_1, ps->post_wait + 1);

	  set_acc_use_is_media_p2 (cpu, ACC40Uk_1);
	}
      if (ACC40Uk_2 >= 0)
	{
	  update_ACC_latency (cpu, ACC40Uk_2, ps->post_wait + 1);

	  set_acc_use_is_media_p2 (cpu, ACC40Uk_2);
	}
      if (ACC40Uk_3 >= 0)
	{
	  update_ACC_latency (cpu, ACC40Uk_3, ps->post_wait + 1);

	  set_acc_use_is_media_p2 (cpu, ACC40Uk_3);
	}
    }

  return cycles;
}

int
frvbf_model_fr400_u_media_2_acc (SIM_CPU *cpu, const IDESC *idesc,
				 int unit_num, int referenced,
				 INT in_ACC40Si, INT out_ACC40Sk)
{
  int cycles;
  INT ACC40Si_1;
  FRV_PROFILE_STATE *ps;
  int busy_adjustment[] = {0, 0, 0};
  int *acc;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ACC40Si_1 = DUAL_REG (in_ACC40Si);

  ps = CPU_PROFILE_STATE (cpu);
  /* The latency of the registers may be less than previously recorded,
     depending on how they were used previously.
     See Table 13-8 in the LSI.  */
  if (acc_use_is_media_p2 (cpu, in_ACC40Si))
    {
      busy_adjustment[0] = 1;
      decrease_ACC_busy (cpu, in_ACC40Si, busy_adjustment[0]);
    }
  if (ACC40Si_1 >= 0 && acc_use_is_media_p2 (cpu, ACC40Si_1))
    {
      busy_adjustment[1] = 1;
      decrease_ACC_busy (cpu, ACC40Si_1, busy_adjustment[1]);
    }
  if (out_ACC40Sk != in_ACC40Si && out_ACC40Sk != ACC40Si_1
      && acc_use_is_media_p2 (cpu, out_ACC40Sk))
    {
      busy_adjustment[2] = 1;
      decrease_ACC_busy (cpu, out_ACC40Sk, busy_adjustment[2]);
    }

  /* The post processing must wait if there is a dependency on a register
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_ACC (cpu, in_ACC40Si);
  post_wait_for_ACC (cpu, ACC40Si_1);
  post_wait_for_ACC (cpu, out_ACC40Sk);

  /* Restore the busy cycles of the registers we used.  */
  acc = ps->acc_busy;
  acc[in_ACC40Si] += busy_adjustment[0];
  if (ACC40Si_1 >= 0)
    acc[ACC40Si_1] += busy_adjustment[1];
  acc[out_ACC40Sk] += busy_adjustment[2];

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  update_ACC_latency (cpu, out_ACC40Sk, ps->post_wait + 1);
  set_acc_use_is_media_p2 (cpu, out_ACC40Sk);

  return cycles;
}

int
frvbf_model_fr400_u_media_2_acc_dual (SIM_CPU *cpu, const IDESC *idesc,
				      int unit_num, int referenced,
				      INT in_ACC40Si, INT out_ACC40Sk)
{
  int cycles;
  INT ACC40Si_1;
  INT ACC40Si_2;
  INT ACC40Si_3;
  INT ACC40Sk_1;
  FRV_PROFILE_STATE *ps;
  int busy_adjustment[] = {0, 0, 0, 0, 0, 0};
  int *acc;

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
     See Table 13-8 in the LSI.  */
  if (acc_use_is_media_p2 (cpu, in_ACC40Si))
    {
      busy_adjustment[0] = 1;
      decrease_ACC_busy (cpu, in_ACC40Si, busy_adjustment[0]);
    }
  if (ACC40Si_1 >= 0 && acc_use_is_media_p2 (cpu, ACC40Si_1))
    {
      busy_adjustment[1] = 1;
      decrease_ACC_busy (cpu, ACC40Si_1, busy_adjustment[1]);
    }
  if (ACC40Si_2 >= 0 && acc_use_is_media_p2 (cpu, ACC40Si_2))
    {
      busy_adjustment[2] = 1;
      decrease_ACC_busy (cpu, ACC40Si_2, busy_adjustment[2]);
    }
  if (ACC40Si_3 >= 0 && acc_use_is_media_p2 (cpu, ACC40Si_3))
    {
      busy_adjustment[3] = 1;
      decrease_ACC_busy (cpu, ACC40Si_3, busy_adjustment[3]);
    }
  if (out_ACC40Sk != in_ACC40Si && out_ACC40Sk != ACC40Si_1
      && out_ACC40Sk != ACC40Si_2 && out_ACC40Sk != ACC40Si_3)
    {
      if (acc_use_is_media_p2 (cpu, out_ACC40Sk))
	{
	  busy_adjustment[4] = 1;
	  decrease_ACC_busy (cpu, out_ACC40Sk, busy_adjustment[4]);
	}
    }
  if (ACC40Sk_1 != in_ACC40Si && ACC40Sk_1 != ACC40Si_1
      && ACC40Sk_1 != ACC40Si_2 && ACC40Sk_1 != ACC40Si_3)
    {
      if (acc_use_is_media_p2 (cpu, ACC40Sk_1))
	{
	  busy_adjustment[5] = 1;
	  decrease_ACC_busy (cpu, ACC40Sk_1, busy_adjustment[5]);
	}
    }

  /* The post processing must wait if there is a dependency on a register
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_ACC (cpu, in_ACC40Si);
  post_wait_for_ACC (cpu, ACC40Si_1);
  post_wait_for_ACC (cpu, ACC40Si_2);
  post_wait_for_ACC (cpu, ACC40Si_3);
  post_wait_for_ACC (cpu, out_ACC40Sk);
  post_wait_for_ACC (cpu, ACC40Sk_1);

  /* Restore the busy cycles of the registers we used.  */
  acc = ps->acc_busy;
  acc[in_ACC40Si] += busy_adjustment[0];
  if (ACC40Si_1 >= 0)
    acc[ACC40Si_1] += busy_adjustment[1];
  if (ACC40Si_2 >= 0)
    acc[ACC40Si_2] += busy_adjustment[2];
  if (ACC40Si_3 >= 0)
    acc[ACC40Si_3] += busy_adjustment[3];
  acc[out_ACC40Sk] += busy_adjustment[4];
  if (ACC40Sk_1 >= 0)
    acc[ACC40Sk_1] += busy_adjustment[5];

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  update_ACC_latency (cpu, out_ACC40Sk, ps->post_wait + 1);
  set_acc_use_is_media_p2 (cpu, out_ACC40Sk);
  if (ACC40Sk_1 >= 0)
    {
      update_ACC_latency (cpu, ACC40Sk_1, ps->post_wait + 1);
      set_acc_use_is_media_p2 (cpu, ACC40Sk_1);
    }

  return cycles;
}

int
frvbf_model_fr400_u_media_2_add_sub (SIM_CPU *cpu, const IDESC *idesc,
				     int unit_num, int referenced,
				     INT in_ACC40Si, INT out_ACC40Sk)
{
  int cycles;
  INT ACC40Si_1;
  INT ACC40Sk_1;
  FRV_PROFILE_STATE *ps;
  int busy_adjustment[] = {0, 0, 0, 0};
  int *acc;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ACC40Si_1 = DUAL_REG (in_ACC40Si);
  ACC40Sk_1 = DUAL_REG (out_ACC40Sk);

  ps = CPU_PROFILE_STATE (cpu);
  /* The latency of the registers may be less than previously recorded,
     depending on how they were used previously.
     See Table 13-8 in the LSI.  */
  if (acc_use_is_media_p2 (cpu, in_ACC40Si))
    {
      busy_adjustment[0] = 1;
      decrease_ACC_busy (cpu, in_ACC40Si, busy_adjustment[0]);
    }
  if (ACC40Si_1 >= 0 && acc_use_is_media_p2 (cpu, ACC40Si_1))
    {
      busy_adjustment[1] = 1;
      decrease_ACC_busy (cpu, ACC40Si_1, busy_adjustment[1]);
    }
  if (out_ACC40Sk != in_ACC40Si && out_ACC40Sk != ACC40Si_1)
    {
      if (acc_use_is_media_p2 (cpu, out_ACC40Sk))
	{
	  busy_adjustment[2] = 1;
	  decrease_ACC_busy (cpu, out_ACC40Sk, busy_adjustment[2]);
	}
    }
  if (ACC40Sk_1 != in_ACC40Si && ACC40Sk_1 != ACC40Si_1)
    {
      if (acc_use_is_media_p2 (cpu, ACC40Sk_1))
	{
	  busy_adjustment[3] = 1;
	  decrease_ACC_busy (cpu, ACC40Sk_1, busy_adjustment[3]);
	}
    }

  /* The post processing must wait if there is a dependency on a register
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_ACC (cpu, in_ACC40Si);
  post_wait_for_ACC (cpu, ACC40Si_1);
  post_wait_for_ACC (cpu, out_ACC40Sk);
  post_wait_for_ACC (cpu, ACC40Sk_1);

  /* Restore the busy cycles of the registers we used.  */
  acc = ps->acc_busy;
  acc[in_ACC40Si] += busy_adjustment[0];
  if (ACC40Si_1 >= 0)
    acc[ACC40Si_1] += busy_adjustment[1];
  acc[out_ACC40Sk] += busy_adjustment[2];
  if (ACC40Sk_1 >= 0)
    acc[ACC40Sk_1] += busy_adjustment[3];

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  update_ACC_latency (cpu, out_ACC40Sk, ps->post_wait + 1);
  set_acc_use_is_media_p2 (cpu, out_ACC40Sk);
  if (ACC40Sk_1 >= 0)
    {
      update_ACC_latency (cpu, ACC40Sk_1, ps->post_wait + 1);
      set_acc_use_is_media_p2 (cpu, ACC40Sk_1);
    }

  return cycles;
}

int
frvbf_model_fr400_u_media_2_add_sub_dual (SIM_CPU *cpu, const IDESC *idesc,
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
  int busy_adjustment[] = {0, 0, 0, 0, 0, 0, 0, 0};
  int *acc;

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
     See Table 13-8 in the LSI.  */
  if (acc_use_is_media_p2 (cpu, in_ACC40Si))
    {
      busy_adjustment[0] = 1;
      decrease_ACC_busy (cpu, in_ACC40Si, busy_adjustment[0]);
    }
  if (ACC40Si_1 >= 0 && acc_use_is_media_p2 (cpu, ACC40Si_1))
    {
      busy_adjustment[1] = 1;
      decrease_ACC_busy (cpu, ACC40Si_1, busy_adjustment[1]);
    }
  if (ACC40Si_2 >= 0 && acc_use_is_media_p2 (cpu, ACC40Si_2))
    {
      busy_adjustment[2] = 1;
      decrease_ACC_busy (cpu, ACC40Si_2, busy_adjustment[2]);
    }
  if (ACC40Si_3 >= 0 && acc_use_is_media_p2 (cpu, ACC40Si_3))
    {
      busy_adjustment[3] = 1;
      decrease_ACC_busy (cpu, ACC40Si_3, busy_adjustment[3]);
    }
  if (out_ACC40Sk != in_ACC40Si && out_ACC40Sk != ACC40Si_1
      && out_ACC40Sk != ACC40Si_2 && out_ACC40Sk != ACC40Si_3)
    {
      if (acc_use_is_media_p2 (cpu, out_ACC40Sk))
	{
	  busy_adjustment[4] = 1;
	  decrease_ACC_busy (cpu, out_ACC40Sk, busy_adjustment[4]);
	}
    }
  if (ACC40Sk_1 != in_ACC40Si && ACC40Sk_1 != ACC40Si_1
      && ACC40Sk_1 != ACC40Si_2 && ACC40Sk_1 != ACC40Si_3)
    {
      if (acc_use_is_media_p2 (cpu, ACC40Sk_1))
	{
	  busy_adjustment[5] = 1;
	  decrease_ACC_busy (cpu, ACC40Sk_1, busy_adjustment[5]);
	}
    }
  if (ACC40Sk_2 != in_ACC40Si && ACC40Sk_2 != ACC40Si_1
      && ACC40Sk_2 != ACC40Si_2 && ACC40Sk_2 != ACC40Si_3)
    {
      if (acc_use_is_media_p2 (cpu, ACC40Sk_2))
	{
	  busy_adjustment[6] = 1;
	  decrease_ACC_busy (cpu, ACC40Sk_2, busy_adjustment[6]);
	}
    }
  if (ACC40Sk_3 != in_ACC40Si && ACC40Sk_3 != ACC40Si_1
      && ACC40Sk_3 != ACC40Si_2 && ACC40Sk_3 != ACC40Si_3)
    {
      if (acc_use_is_media_p2 (cpu, ACC40Sk_3))
	{
	  busy_adjustment[7] = 1;
	  decrease_ACC_busy (cpu, ACC40Sk_3, busy_adjustment[7]);
	}
    }

  /* The post processing must wait if there is a dependency on a register
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_ACC (cpu, in_ACC40Si);
  post_wait_for_ACC (cpu, ACC40Si_1);
  post_wait_for_ACC (cpu, ACC40Si_2);
  post_wait_for_ACC (cpu, ACC40Si_3);
  post_wait_for_ACC (cpu, out_ACC40Sk);
  post_wait_for_ACC (cpu, ACC40Sk_1);
  post_wait_for_ACC (cpu, ACC40Sk_2);
  post_wait_for_ACC (cpu, ACC40Sk_3);

  /* Restore the busy cycles of the registers we used.  */
  acc = ps->acc_busy;
  acc[in_ACC40Si] += busy_adjustment[0];
  if (ACC40Si_1 >= 0)
    acc[ACC40Si_1] += busy_adjustment[1];
  if (ACC40Si_2 >= 0)
    acc[ACC40Si_2] += busy_adjustment[2];
  if (ACC40Si_3 >= 0)
    acc[ACC40Si_3] += busy_adjustment[3];
  acc[out_ACC40Sk] += busy_adjustment[4];
  if (ACC40Sk_1 >= 0)
    acc[ACC40Sk_1] += busy_adjustment[5];
  if (ACC40Sk_2 >= 0)
    acc[ACC40Sk_2] += busy_adjustment[6];
  if (ACC40Sk_3 >= 0)
    acc[ACC40Sk_3] += busy_adjustment[7];

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  update_ACC_latency (cpu, out_ACC40Sk, ps->post_wait + 1);
  set_acc_use_is_media_p2 (cpu, out_ACC40Sk);
  if (ACC40Sk_1 >= 0)
    {
      update_ACC_latency (cpu, ACC40Sk_1, ps->post_wait + 1);
      set_acc_use_is_media_p2 (cpu, ACC40Sk_1);
    }
  if (ACC40Sk_2 >= 0)
    {
      update_ACC_latency (cpu, ACC40Sk_2, ps->post_wait + 1);
      set_acc_use_is_media_p2 (cpu, ACC40Sk_2);
    }
  if (ACC40Sk_3 >= 0)
    {
      update_ACC_latency (cpu, ACC40Sk_3, ps->post_wait + 1);
      set_acc_use_is_media_p2 (cpu, ACC40Sk_3);
    }

  return cycles;
}

int
frvbf_model_fr400_u_media_3 (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_FRi, INT in_FRj,
			     INT out_FRk)
{
  /* Modelling is the same as media unit 1.  */
  return frvbf_model_fr400_u_media_1 (cpu, idesc, unit_num, referenced,
				      in_FRi, in_FRj, out_FRk);
}

int
frvbf_model_fr400_u_media_3_dual (SIM_CPU *cpu, const IDESC *idesc,
				  int unit_num, int referenced,
				  INT in_FRi, INT out_FRk)
{
  int cycles;
  INT dual_FRi;
  FRV_PROFILE_STATE *ps;
  int busy_adjustment[] = {0, 0};
  int *fr;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ps = CPU_PROFILE_STATE (cpu);
  dual_FRi = DUAL_REG (in_FRi);

  /* The latency of the registers may be less than previously recorded,
     depending on how they were used previously.
     See Table 13-8 in the LSI.  */
  if (use_is_fp_load (cpu, in_FRi))
    {
      busy_adjustment[0] = 1;
      decrease_FR_busy (cpu, in_FRi, busy_adjustment[0]);
    }
  else
    enforce_full_fr_latency (cpu, in_FRi);
  if (dual_FRi >= 0 && use_is_fp_load (cpu, dual_FRi))
    {
      busy_adjustment[1] = 1;
      decrease_FR_busy (cpu, dual_FRi, busy_adjustment[1]);
    }
  else
    enforce_full_fr_latency (cpu, dual_FRi);

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, dual_FRi);
  post_wait_for_FR (cpu, out_FRk);

  /* Restore the busy cycles of the registers we used.  */
  fr = ps->fr_busy;
  fr[in_FRi] += busy_adjustment[0];
  if (dual_FRi >= 0)
    fr[dual_FRi] += busy_adjustment[1];

  /* The latency of the output register will be at least the latency of the
     other inputs.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);

  /* Once initiated, post-processing has no latency.  */
  update_FR_ptime (cpu, out_FRk, 0);

  return cycles;
}

int
frvbf_model_fr400_u_media_3_quad (SIM_CPU *cpu, const IDESC *idesc,
				  int unit_num, int referenced,
				  INT in_FRi, INT in_FRj,
				  INT out_FRk)
{
  /* Modelling is the same as media unit 1.  */
  return frvbf_model_fr400_u_media_1_quad (cpu, idesc, unit_num, referenced,
					   in_FRi, in_FRj, out_FRk);
}

int
frvbf_model_fr400_u_media_4 (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_ACC40Si, INT in_FRj,
			     INT out_ACC40Sk, INT out_FRk)
{
  int cycles;
  FRV_PROFILE_STATE *ps;
  const CGEN_INSN *insn;
  int busy_adjustment[] = {0};
  int *fr;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ps = CPU_PROFILE_STATE (cpu);
  insn = idesc->idata;

  /* The latency of the registers may be less than previously recorded,
     depending on how they were used previously.
     See Table 13-8 in the LSI.  */
  if (in_FRj >= 0)
    {
      if (use_is_fp_load (cpu, in_FRj))
	{
	  busy_adjustment[0] = 1;
	  decrease_FR_busy (cpu, in_FRj, busy_adjustment[0]);
	}
      else
	enforce_full_fr_latency (cpu, in_FRj);
    }

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_ACC (cpu, in_ACC40Si);
  post_wait_for_ACC (cpu, out_ACC40Sk);
  post_wait_for_FR (cpu, in_FRj);
  post_wait_for_FR (cpu, out_FRk);

  /* Restore the busy cycles of the registers we used.  */
  fr = ps->fr_busy;

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  if (out_FRk >= 0)
    {
      update_FR_latency (cpu, out_FRk, ps->post_wait);
      update_FR_ptime (cpu, out_FRk, 1);
      /* Mark this use of the register as media unit 4.  */
      set_use_is_media_p4 (cpu, out_FRk);
    }
  else if (out_ACC40Sk >= 0)
    {
      update_ACC_latency (cpu, out_ACC40Sk, ps->post_wait);
      update_ACC_ptime (cpu, out_ACC40Sk, 1);
      /* Mark this use of the register as media unit 4.  */
      set_acc_use_is_media_p4 (cpu, out_ACC40Sk);
    }

  return cycles;
}

int
frvbf_model_fr400_u_media_4_accg (SIM_CPU *cpu, const IDESC *idesc,
				  int unit_num, int referenced,
				  INT in_ACCGi, INT in_FRinti,
				  INT out_ACCGk, INT out_FRintk)
{
  /* Modelling is the same as media-4 unit except use accumulator guards
     as input instead of accumulators.  */
  return frvbf_model_fr400_u_media_4 (cpu, idesc, unit_num, referenced,
				      in_ACCGi, in_FRinti,
				      out_ACCGk, out_FRintk);
}

int
frvbf_model_fr400_u_media_4_acc_dual (SIM_CPU *cpu, const IDESC *idesc,
				      int unit_num, int referenced,
				      INT in_ACC40Si, INT out_FRk)
{
  int cycles;
  FRV_PROFILE_STATE *ps;
  const CGEN_INSN *insn;
  INT ACC40Si_1;
  INT FRk_1;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ps = CPU_PROFILE_STATE (cpu);
  ACC40Si_1 = DUAL_REG (in_ACC40Si);
  FRk_1 = DUAL_REG (out_FRk);

  insn = idesc->idata;

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_ACC (cpu, in_ACC40Si);
  post_wait_for_ACC (cpu, ACC40Si_1);
  post_wait_for_FR (cpu, out_FRk);
  post_wait_for_FR (cpu, FRk_1);

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  if (out_FRk >= 0)
    {
      update_FR_latency (cpu, out_FRk, ps->post_wait);
      update_FR_ptime (cpu, out_FRk, 1);
      /* Mark this use of the register as media unit 4.  */
      set_use_is_media_p4 (cpu, out_FRk);
    }
  if (FRk_1 >= 0)
    {
      update_FR_latency (cpu, FRk_1, ps->post_wait);
      update_FR_ptime (cpu, FRk_1, 1);
      /* Mark this use of the register as media unit 4.  */
      set_use_is_media_p4 (cpu, FRk_1);
    }

  return cycles;
}

int
frvbf_model_fr400_u_media_6 (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_FRi, INT out_FRk)
{
  int cycles;
  FRV_PROFILE_STATE *ps;
  const CGEN_INSN *insn;
  int busy_adjustment[] = {0};
  int *fr;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ps = CPU_PROFILE_STATE (cpu);
  insn = idesc->idata;

  /* The latency of the registers may be less than previously recorded,
     depending on how they were used previously.
     See Table 13-8 in the LSI.  */
  if (in_FRi >= 0)
    {
      if (use_is_fp_load (cpu, in_FRi))
	{
	  busy_adjustment[0] = 1;
	  decrease_FR_busy (cpu, in_FRi, busy_adjustment[0]);
	}
      else
	enforce_full_fr_latency (cpu, in_FRi);
    }

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, out_FRk);

  /* Restore the busy cycles of the registers we used.  */
  fr = ps->fr_busy;
  if (in_FRi >= 0)
    fr[in_FRi] += busy_adjustment[0];

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing will take 1 cycle.  */
  if (out_FRk >= 0)
    {
      update_FR_latency (cpu, out_FRk, ps->post_wait);
      update_FR_ptime (cpu, out_FRk, 1);

      /* Mark this use of the register as media unit 1.  */
      set_use_is_media_p6 (cpu, out_FRk);
    }

  return cycles;
}

int
frvbf_model_fr400_u_media_7 (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_FRinti, INT in_FRintj,
			     INT out_FCCk)
{
  int cycles;
  FRV_PROFILE_STATE *ps;
  int busy_adjustment[] = {0, 0};
  int *fr;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps = CPU_PROFILE_STATE (cpu);

  /* The latency of the registers may be less than previously recorded,
     depending on how they were used previously.
     See Table 13-8 in the LSI.  */
  if (in_FRinti >= 0)
    {
      if (use_is_fp_load (cpu, in_FRinti))
	{
	  busy_adjustment[0] = 1;
	  decrease_FR_busy (cpu, in_FRinti, busy_adjustment[0]);
	}
      else
	enforce_full_fr_latency (cpu, in_FRinti);
    }
  if (in_FRintj >= 0 && in_FRintj != in_FRinti)
    {
      if (use_is_fp_load (cpu, in_FRintj))
	{
	  busy_adjustment[1] = 1;
	  decrease_FR_busy (cpu, in_FRintj, busy_adjustment[1]);
	}
      else
	enforce_full_fr_latency (cpu, in_FRintj);
    }

  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRinti);
  post_wait_for_FR (cpu, in_FRintj);
  post_wait_for_CCR (cpu, out_FCCk);

  /* Restore the busy cycles of the registers we used.  */
  fr = ps->fr_busy;
  if (in_FRinti >= 0)
    fr[in_FRinti] += busy_adjustment[0];
  if (in_FRintj >= 0)
    fr[in_FRintj] += busy_adjustment[1];

  /* The latency of FCCi_2 will be the latency of the other inputs plus 1
     cycle.  */
  update_CCR_latency (cpu, out_FCCk, ps->post_wait + 1);

  return cycles;
}

int
frvbf_model_fr400_u_media_dual_expand (SIM_CPU *cpu, const IDESC *idesc,
				       int unit_num, int referenced,
				       INT in_FRi,
				       INT out_FRk)
{
  /* Insns using this unit are media-3 class insns, with a dual FRk output.  */
  int cycles;
  INT dual_FRk;
  FRV_PROFILE_STATE *ps;
  int busy_adjustment[] = {0};
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
  if (use_is_fp_load (cpu, in_FRi))
    {
      busy_adjustment[0] = 1;
      decrease_FR_busy (cpu, in_FRi, busy_adjustment[0]);
    }
  else
    enforce_full_fr_latency (cpu, in_FRi);

  /* The post processing must wait if there is a dependency on a FR
     which is not ready yet.  */
  ps->post_wait = cycles;
  post_wait_for_FR (cpu, in_FRi);
  post_wait_for_FR (cpu, out_FRk);
  post_wait_for_FR (cpu, dual_FRk);

  /* Restore the busy cycles of the registers we used.  */
  fr = ps->fr_busy;
  fr[in_FRi] += busy_adjustment[0];

  /* The latency of the output register will be at least the latency of the
     other inputs.  Once initiated, post-processing has no latency.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);
  update_FR_ptime (cpu, out_FRk, 0);

  if (dual_FRk >= 0)
    {
      update_FR_latency (cpu, dual_FRk, ps->post_wait);
      update_FR_ptime (cpu, dual_FRk, 0);
    }

  return cycles;
}

int
frvbf_model_fr400_u_media_dual_htob (SIM_CPU *cpu, const IDESC *idesc,
				     int unit_num, int referenced,
				     INT in_FRj,
				     INT out_FRk)
{
  /* Insns using this unit are media-3 class insns, with a dual FRj input.  */
  int cycles;
  INT dual_FRj;
  FRV_PROFILE_STATE *ps;
  int busy_adjustment[] = {0, 0};
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
  if (use_is_fp_load (cpu, in_FRj))
    {
      busy_adjustment[0] = 1;
      decrease_FR_busy (cpu, in_FRj, busy_adjustment[0]);
    }
  else
    enforce_full_fr_latency (cpu, in_FRj);
  if (dual_FRj >= 0)
    {
      if (use_is_fp_load (cpu, dual_FRj))
	{
	  busy_adjustment[1] = 1;
	  decrease_FR_busy (cpu, dual_FRj, busy_adjustment[1]);
	}
      else
	enforce_full_fr_latency (cpu, dual_FRj);
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

  /* The latency of the output register will be at least the latency of the
     other inputs.  */
  update_FR_latency (cpu, out_FRk, ps->post_wait);

  /* Once initiated, post-processing has no latency.  */
  update_FR_ptime (cpu, out_FRk, 0);

  return cycles;
}

int
frvbf_model_fr400_u_ici (SIM_CPU *cpu, const IDESC *idesc,
			 int unit_num, int referenced,
			 INT in_GRi, INT in_GRj)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_ici (cpu, idesc, unit_num, referenced,
				  in_GRi, in_GRj);
}

int
frvbf_model_fr400_u_dci (SIM_CPU *cpu, const IDESC *idesc,
			 int unit_num, int referenced,
			 INT in_GRi, INT in_GRj)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_dci (cpu, idesc, unit_num, referenced,
				  in_GRi, in_GRj);
}

int
frvbf_model_fr400_u_dcf (SIM_CPU *cpu, const IDESC *idesc,
			 int unit_num, int referenced,
			 INT in_GRi, INT in_GRj)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_dcf (cpu, idesc, unit_num, referenced,
				  in_GRi, in_GRj);
}

int
frvbf_model_fr400_u_icpl (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_icpl (cpu, idesc, unit_num, referenced,
				   in_GRi, in_GRj);
}

int
frvbf_model_fr400_u_dcpl (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_dcpl (cpu, idesc, unit_num, referenced,
				   in_GRi, in_GRj);
}

int
frvbf_model_fr400_u_icul (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_icul (cpu, idesc, unit_num, referenced,
				   in_GRi, in_GRj);
}

int
frvbf_model_fr400_u_dcul (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_dcul (cpu, idesc, unit_num, referenced,
				   in_GRi, in_GRj);
}

int
frvbf_model_fr400_u_barrier (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_barrier (cpu, idesc, unit_num, referenced);
}

int
frvbf_model_fr400_u_membar (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_membar (cpu, idesc, unit_num, referenced);
}

#endif /* WITH_PROFILE_MODEL_P */
