/* frv simulator fr450 dependent profiling code.

   Copyright (C) 2001, 2004, 2007 Free Software Foundation, Inc.
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

int
frvbf_model_fr450_u_exec (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced)
{
  return idesc->timing->units[unit_num].done;
}

int
frvbf_model_fr450_u_integer (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_GRi, INT in_GRj, INT out_GRk,
			     INT out_ICCi_1)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_integer (cpu, idesc, unit_num, referenced,
				      in_GRi, in_GRj, out_GRk, out_ICCi_1);
}

int
frvbf_model_fr450_u_imul (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj, INT out_GRk, INT out_ICCi_1)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* Pass 1 is the same as for fr500.  */
      return frvbf_model_fr500_u_imul (cpu, idesc, unit_num, referenced,
				       in_GRi, in_GRj, out_GRk, out_ICCi_1);
    }

  /* icc0-icc4 are the upper 4 fields of the CCR.  */
  if (out_ICCi_1 >= 0)
    out_ICCi_1 += 4;

  /* GRk and IACCi_1 have a latency of 1 cycle.  */
  cycles = idesc->timing->units[unit_num].done;
  update_GRdouble_latency (cpu, out_GRk, cycles + 1);
  update_CCR_latency (cpu, out_ICCi_1, cycles + 1);

  return cycles;
}

int
frvbf_model_fr450_u_idiv (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj, INT out_GRk, INT out_ICCi_1)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* Pass 1 is the same as for fr500.  */
      return frvbf_model_fr500_u_idiv (cpu, idesc, unit_num, referenced,
				       in_GRi, in_GRj, out_GRk, out_ICCi_1);
    }

  /* icc0-icc4 are the upper 4 fields of the CCR.  */
  if (out_ICCi_1 >= 0)
    out_ICCi_1 += 4;

  /* GRk, ICCi_1 and the divider have a latency of 18 cycles  */
  cycles = idesc->timing->units[unit_num].done;
  update_GR_latency (cpu, out_GRk, cycles + 18);
  update_CCR_latency (cpu, out_ICCi_1, cycles + 18);
  update_idiv_resource_latency (cpu, 0, cycles + 18);

  return cycles;
}

int
frvbf_model_fr450_u_branch (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced,
			    INT in_GRi, INT in_GRj,
			    INT in_ICCi_2, INT in_ICCi_3)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_branch (cpu, idesc, unit_num, referenced,
				     in_GRi, in_GRj, in_ICCi_2, in_ICCi_3);
}

int
frvbf_model_fr450_u_trap (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj,
			  INT in_ICCi_2, INT in_FCCi_2)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_trap (cpu, idesc, unit_num, referenced,
				   in_GRi, in_GRj, in_ICCi_2, in_FCCi_2);
}

int
frvbf_model_fr450_u_check (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_ICCi_3, INT in_FCCi_3)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_check (cpu, idesc, unit_num, referenced,
				    in_ICCi_3, in_FCCi_3);
}

int
frvbf_model_fr450_u_set_hilo (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT out_GRkhi, INT out_GRklo)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_set_hilo (cpu, idesc, unit_num, referenced,
				       out_GRkhi, out_GRklo);
}

int
frvbf_model_fr450_u_gr_load (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_GRi, INT in_GRj,
			     INT out_GRk, INT out_GRdoublek)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* Pass 1 is the same as for fr500.  */
      return frvbf_model_fr500_u_fr_load (cpu, idesc, unit_num, referenced,
					  in_GRi, in_GRj, out_GRk,
					  out_GRdoublek);
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
frvbf_model_fr450_u_gr_store (SIM_CPU *cpu, const IDESC *idesc,
			      int unit_num, int referenced,
			      INT in_GRi, INT in_GRj,
			      INT in_GRk, INT in_GRdoublek)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_gr_store (cpu, idesc, unit_num, referenced,
				       in_GRi, in_GRj, in_GRk, in_GRdoublek);
}

int
frvbf_model_fr450_u_fr_load (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_GRi, INT in_GRj,
			     INT out_FRk, INT out_FRdoublek)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_fr_load (cpu, idesc, unit_num, referenced,
				      in_GRi, in_GRj, out_FRk, out_FRdoublek);
}

int
frvbf_model_fr450_u_fr_store (SIM_CPU *cpu, const IDESC *idesc,
			      int unit_num, int referenced,
			      INT in_GRi, INT in_GRj,
			      INT in_FRk, INT in_FRdoublek)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_fr_load (cpu, idesc, unit_num, referenced,
				      in_GRi, in_GRj, in_FRk, in_FRdoublek);
}

int
frvbf_model_fr450_u_swap (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj, INT out_GRk)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_swap (cpu, idesc, unit_num, referenced,
				   in_GRi, in_GRj, out_GRk);
}

int
frvbf_model_fr450_u_fr2gr (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_FRk, INT out_GRj)
{
  int cycles;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    {
      /* Pass 1 is the same as for fr400.  */
      return frvbf_model_fr500_u_fr2gr (cpu, idesc, unit_num, referenced,
					in_FRk, out_GRj);
    }

  /* The latency of GRj is 1 cycle.  */
  cycles = idesc->timing->units[unit_num].done;
  update_GR_latency (cpu, out_GRj, cycles + 1);

  return cycles;
}

int
frvbf_model_fr450_u_spr2gr (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_spr, INT out_GRj)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_spr2gr (cpu, idesc, unit_num, referenced,
				     in_spr, out_GRj);
}

int
frvbf_model_fr450_u_gr2fr (SIM_CPU *cpu, const IDESC *idesc,
			   int unit_num, int referenced,
			   INT in_GRj, INT out_FRk)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_gr2fr (cpu, idesc, unit_num, referenced,
				    in_GRj, out_FRk);
}

int
frvbf_model_fr450_u_gr2spr (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced,
			    INT in_GRj, INT out_spr)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_gr2spr (cpu, idesc, unit_num, referenced,
				     in_GRj, out_spr);
}

int
frvbf_model_fr450_u_media_1 (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_FRi, INT in_FRj,
			     INT out_FRk)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_media_1 (cpu, idesc, unit_num, referenced,
				      in_FRi, in_FRj, out_FRk);
}

int
frvbf_model_fr450_u_media_1_quad (SIM_CPU *cpu, const IDESC *idesc,
				  int unit_num, int referenced,
				  INT in_FRi, INT in_FRj,
				  INT out_FRk)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_media_1_quad (cpu, idesc, unit_num, referenced,
					   in_FRi, in_FRj, out_FRk);
}

int
frvbf_model_fr450_u_media_hilo (SIM_CPU *cpu, const IDESC *idesc,
				int unit_num, int referenced,
				INT out_FRkhi, INT out_FRklo)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_media_hilo (cpu, idesc, unit_num, referenced,
					 out_FRkhi, out_FRklo);
}

int
frvbf_model_fr450_u_media_2 (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_FRi, INT in_FRj,
			     INT out_ACC40Sk, INT out_ACC40Uk)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_media_2 (cpu, idesc, unit_num, referenced,
				      in_FRi, in_FRj, out_ACC40Sk,
				      out_ACC40Uk);
}

int
frvbf_model_fr450_u_media_2_quad (SIM_CPU *cpu, const IDESC *idesc,
				  int unit_num, int referenced,
				  INT in_FRi, INT in_FRj,
				  INT out_ACC40Sk, INT out_ACC40Uk)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_media_2_quad (cpu, idesc, unit_num, referenced,
					   in_FRi, in_FRj, out_ACC40Sk,
					   out_ACC40Uk);
}

int
frvbf_model_fr450_u_media_2_acc (SIM_CPU *cpu, const IDESC *idesc,
				 int unit_num, int referenced,
				 INT in_ACC40Si, INT out_ACC40Sk)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_media_2_acc (cpu, idesc, unit_num, referenced,
					  in_ACC40Si, out_ACC40Sk);
}

int
frvbf_model_fr450_u_media_2_acc_dual (SIM_CPU *cpu, const IDESC *idesc,
				      int unit_num, int referenced,
				      INT in_ACC40Si, INT out_ACC40Sk)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_media_2_acc_dual (cpu, idesc, unit_num,
					       referenced, in_ACC40Si,
					       out_ACC40Sk);
}

int
frvbf_model_fr450_u_media_2_add_sub (SIM_CPU *cpu, const IDESC *idesc,
				     int unit_num, int referenced,
				     INT in_ACC40Si, INT out_ACC40Sk)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_media_2_add_sub (cpu, idesc, unit_num,
					      referenced, in_ACC40Si,
					      out_ACC40Sk);
}

int
frvbf_model_fr450_u_media_2_add_sub_dual (SIM_CPU *cpu, const IDESC *idesc,
					  int unit_num, int referenced,
					  INT in_ACC40Si, INT out_ACC40Sk)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_media_2_add_sub_dual (cpu, idesc, unit_num,
						   referenced, in_ACC40Si,
						   out_ACC40Sk);
}

int
frvbf_model_fr450_u_media_3 (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_FRi, INT in_FRj,
			     INT out_FRk)
{
  /* Modelling is the same as media unit 1.  */
  return frvbf_model_fr450_u_media_1 (cpu, idesc, unit_num, referenced,
				      in_FRi, in_FRj, out_FRk);
}

int
frvbf_model_fr450_u_media_3_dual (SIM_CPU *cpu, const IDESC *idesc,
				  int unit_num, int referenced,
				  INT in_FRi, INT out_FRk)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_media_3_dual (cpu, idesc, unit_num, referenced,
					   in_FRi, out_FRk);
}

int
frvbf_model_fr450_u_media_3_quad (SIM_CPU *cpu, const IDESC *idesc,
				  int unit_num, int referenced,
				  INT in_FRi, INT in_FRj,
				  INT out_FRk)
{
  /* Modelling is the same as media unit 1.  */
  return frvbf_model_fr450_u_media_1_quad (cpu, idesc, unit_num, referenced,
					   in_FRi, in_FRj, out_FRk);
}

int
frvbf_model_fr450_u_media_4 (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_ACC40Si, INT in_FRj,
			     INT out_ACC40Sk, INT out_FRk)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_media_4 (cpu, idesc, unit_num, referenced,
				      in_ACC40Si, in_FRj,
				      out_ACC40Sk, out_FRk);
}

int
frvbf_model_fr450_u_media_4_accg (SIM_CPU *cpu, const IDESC *idesc,
				  int unit_num, int referenced,
				  INT in_ACCGi, INT in_FRinti,
				  INT out_ACCGk, INT out_FRintk)
{
  /* Modelling is the same as media-4 unit except use accumulator guards
     as input instead of accumulators.  */
  return frvbf_model_fr450_u_media_4 (cpu, idesc, unit_num, referenced,
				      in_ACCGi, in_FRinti,
				      out_ACCGk, out_FRintk);
}

int
frvbf_model_fr450_u_media_4_acc_dual (SIM_CPU *cpu, const IDESC *idesc,
				      int unit_num, int referenced,
				      INT in_ACC40Si, INT out_FRk)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_media_4_acc_dual (cpu, idesc, unit_num,
					       referenced, in_ACC40Si,
					       out_FRk);
}

int
frvbf_model_fr450_u_media_4_mclracca (SIM_CPU *cpu, const IDESC *idesc,
				      int unit_num, int referenced)
{
  int cycles;
  int acc;
  FRV_PROFILE_STATE *ps;

  if (model_insn == FRV_INSN_MODEL_PASS_1)
    return 0;

  /* The preprocessing can execute right away.  */
  cycles = idesc->timing->units[unit_num].done;

  ps = CPU_PROFILE_STATE (cpu);

  /* The post processing must wait for any pending ACC writes.  */
  ps->post_wait = cycles;
  for (acc = 0; acc < 4; acc++)
    post_wait_for_ACC (cpu, acc);
  for (acc = 8; acc < 12; acc++)
    post_wait_for_ACC (cpu, acc);

  for (acc = 0; acc < 4; acc++)
    {
      update_ACC_latency (cpu, acc, ps->post_wait);
      update_ACC_ptime (cpu, acc, 2);
    }
  for (acc = 8; acc < 12; acc++)
    {
      update_ACC_latency (cpu, acc, ps->post_wait);
      update_ACC_ptime (cpu, acc, 2);
    }

  return cycles;
}

int
frvbf_model_fr450_u_media_6 (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_FRi, INT out_FRk)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_media_6 (cpu, idesc, unit_num, referenced,
				      in_FRi, out_FRk);
}

int
frvbf_model_fr450_u_media_7 (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced,
			     INT in_FRinti, INT in_FRintj,
			     INT out_FCCk)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_media_7 (cpu, idesc, unit_num, referenced,
				      in_FRinti, in_FRintj, out_FCCk);
}

int
frvbf_model_fr450_u_media_dual_expand (SIM_CPU *cpu, const IDESC *idesc,
				       int unit_num, int referenced,
				       INT in_FRi,
				       INT out_FRk)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_media_dual_expand (cpu, idesc, unit_num,
						referenced, in_FRi, out_FRk);
}

int
frvbf_model_fr450_u_media_dual_htob (SIM_CPU *cpu, const IDESC *idesc,
				     int unit_num, int referenced,
				     INT in_FRj,
				     INT out_FRk)
{
  /* Modelling for this unit is the same as for fr400.  */
  return frvbf_model_fr400_u_media_dual_htob (cpu, idesc, unit_num,
					      referenced, in_FRj, out_FRk);
}

int
frvbf_model_fr450_u_ici (SIM_CPU *cpu, const IDESC *idesc,
			 int unit_num, int referenced,
			 INT in_GRi, INT in_GRj)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_ici (cpu, idesc, unit_num, referenced,
				  in_GRi, in_GRj);
}

int
frvbf_model_fr450_u_dci (SIM_CPU *cpu, const IDESC *idesc,
			 int unit_num, int referenced,
			 INT in_GRi, INT in_GRj)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_dci (cpu, idesc, unit_num, referenced,
				  in_GRi, in_GRj);
}

int
frvbf_model_fr450_u_dcf (SIM_CPU *cpu, const IDESC *idesc,
			 int unit_num, int referenced,
			 INT in_GRi, INT in_GRj)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_dcf (cpu, idesc, unit_num, referenced,
				  in_GRi, in_GRj);
}

int
frvbf_model_fr450_u_icpl (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_icpl (cpu, idesc, unit_num, referenced,
				   in_GRi, in_GRj);
}

int
frvbf_model_fr450_u_dcpl (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_dcpl (cpu, idesc, unit_num, referenced,
				   in_GRi, in_GRj);
}

int
frvbf_model_fr450_u_icul (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_icul (cpu, idesc, unit_num, referenced,
				   in_GRi, in_GRj);
}

int
frvbf_model_fr450_u_dcul (SIM_CPU *cpu, const IDESC *idesc,
			  int unit_num, int referenced,
			  INT in_GRi, INT in_GRj)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_dcul (cpu, idesc, unit_num, referenced,
				   in_GRi, in_GRj);
}

int
frvbf_model_fr450_u_barrier (SIM_CPU *cpu, const IDESC *idesc,
			     int unit_num, int referenced)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_barrier (cpu, idesc, unit_num, referenced);
}

int
frvbf_model_fr450_u_membar (SIM_CPU *cpu, const IDESC *idesc,
			    int unit_num, int referenced)
{
  /* Modelling for this unit is the same as for fr500.  */
  return frvbf_model_fr500_u_membar (cpu, idesc, unit_num, referenced);
}

#endif /* WITH_PROFILE_MODEL_P */
