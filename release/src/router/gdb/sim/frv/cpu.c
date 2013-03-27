/* Misc. support for CPU family frvbf.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996-2005 Free Software Foundation, Inc.

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

#define WANT_CPU frvbf
#define WANT_CPU_FRVBF

#include "sim-main.h"
#include "cgen-ops.h"

/* Get the value of h-reloc-ann.  */

BI
frvbf_h_reloc_ann_get (SIM_CPU *current_cpu)
{
  return CPU (h_reloc_ann);
}

/* Set a value for h-reloc-ann.  */

void
frvbf_h_reloc_ann_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_reloc_ann) = newval;
}

/* Get the value of h-pc.  */

USI
frvbf_h_pc_get (SIM_CPU *current_cpu)
{
  return CPU (h_pc);
}

/* Set a value for h-pc.  */

void
frvbf_h_pc_set (SIM_CPU *current_cpu, USI newval)
{
  CPU (h_pc) = newval;
}

/* Get the value of h-psr_imple.  */

UQI
frvbf_h_psr_imple_get (SIM_CPU *current_cpu)
{
  return CPU (h_psr_imple);
}

/* Set a value for h-psr_imple.  */

void
frvbf_h_psr_imple_set (SIM_CPU *current_cpu, UQI newval)
{
  CPU (h_psr_imple) = newval;
}

/* Get the value of h-psr_ver.  */

UQI
frvbf_h_psr_ver_get (SIM_CPU *current_cpu)
{
  return CPU (h_psr_ver);
}

/* Set a value for h-psr_ver.  */

void
frvbf_h_psr_ver_set (SIM_CPU *current_cpu, UQI newval)
{
  CPU (h_psr_ver) = newval;
}

/* Get the value of h-psr_ice.  */

BI
frvbf_h_psr_ice_get (SIM_CPU *current_cpu)
{
  return CPU (h_psr_ice);
}

/* Set a value for h-psr_ice.  */

void
frvbf_h_psr_ice_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_psr_ice) = newval;
}

/* Get the value of h-psr_nem.  */

BI
frvbf_h_psr_nem_get (SIM_CPU *current_cpu)
{
  return CPU (h_psr_nem);
}

/* Set a value for h-psr_nem.  */

void
frvbf_h_psr_nem_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_psr_nem) = newval;
}

/* Get the value of h-psr_cm.  */

BI
frvbf_h_psr_cm_get (SIM_CPU *current_cpu)
{
  return CPU (h_psr_cm);
}

/* Set a value for h-psr_cm.  */

void
frvbf_h_psr_cm_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_psr_cm) = newval;
}

/* Get the value of h-psr_be.  */

BI
frvbf_h_psr_be_get (SIM_CPU *current_cpu)
{
  return CPU (h_psr_be);
}

/* Set a value for h-psr_be.  */

void
frvbf_h_psr_be_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_psr_be) = newval;
}

/* Get the value of h-psr_esr.  */

BI
frvbf_h_psr_esr_get (SIM_CPU *current_cpu)
{
  return CPU (h_psr_esr);
}

/* Set a value for h-psr_esr.  */

void
frvbf_h_psr_esr_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_psr_esr) = newval;
}

/* Get the value of h-psr_ef.  */

BI
frvbf_h_psr_ef_get (SIM_CPU *current_cpu)
{
  return CPU (h_psr_ef);
}

/* Set a value for h-psr_ef.  */

void
frvbf_h_psr_ef_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_psr_ef) = newval;
}

/* Get the value of h-psr_em.  */

BI
frvbf_h_psr_em_get (SIM_CPU *current_cpu)
{
  return CPU (h_psr_em);
}

/* Set a value for h-psr_em.  */

void
frvbf_h_psr_em_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_psr_em) = newval;
}

/* Get the value of h-psr_pil.  */

UQI
frvbf_h_psr_pil_get (SIM_CPU *current_cpu)
{
  return CPU (h_psr_pil);
}

/* Set a value for h-psr_pil.  */

void
frvbf_h_psr_pil_set (SIM_CPU *current_cpu, UQI newval)
{
  CPU (h_psr_pil) = newval;
}

/* Get the value of h-psr_ps.  */

BI
frvbf_h_psr_ps_get (SIM_CPU *current_cpu)
{
  return CPU (h_psr_ps);
}

/* Set a value for h-psr_ps.  */

void
frvbf_h_psr_ps_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_psr_ps) = newval;
}

/* Get the value of h-psr_et.  */

BI
frvbf_h_psr_et_get (SIM_CPU *current_cpu)
{
  return CPU (h_psr_et);
}

/* Set a value for h-psr_et.  */

void
frvbf_h_psr_et_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_psr_et) = newval;
}

/* Get the value of h-psr_s.  */

BI
frvbf_h_psr_s_get (SIM_CPU *current_cpu)
{
  return CPU (h_psr_s);
}

/* Set a value for h-psr_s.  */

void
frvbf_h_psr_s_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_PSR_S (newval);
}

/* Get the value of h-tbr_tba.  */

USI
frvbf_h_tbr_tba_get (SIM_CPU *current_cpu)
{
  return CPU (h_tbr_tba);
}

/* Set a value for h-tbr_tba.  */

void
frvbf_h_tbr_tba_set (SIM_CPU *current_cpu, USI newval)
{
  CPU (h_tbr_tba) = newval;
}

/* Get the value of h-tbr_tt.  */

UQI
frvbf_h_tbr_tt_get (SIM_CPU *current_cpu)
{
  return CPU (h_tbr_tt);
}

/* Set a value for h-tbr_tt.  */

void
frvbf_h_tbr_tt_set (SIM_CPU *current_cpu, UQI newval)
{
  CPU (h_tbr_tt) = newval;
}

/* Get the value of h-bpsr_bs.  */

BI
frvbf_h_bpsr_bs_get (SIM_CPU *current_cpu)
{
  return CPU (h_bpsr_bs);
}

/* Set a value for h-bpsr_bs.  */

void
frvbf_h_bpsr_bs_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_bpsr_bs) = newval;
}

/* Get the value of h-bpsr_bet.  */

BI
frvbf_h_bpsr_bet_get (SIM_CPU *current_cpu)
{
  return CPU (h_bpsr_bet);
}

/* Set a value for h-bpsr_bet.  */

void
frvbf_h_bpsr_bet_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_bpsr_bet) = newval;
}

/* Get the value of h-gr.  */

USI
frvbf_h_gr_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_GR (regno);
}

/* Set a value for h-gr.  */

void
frvbf_h_gr_set (SIM_CPU *current_cpu, UINT regno, USI newval)
{
  SET_H_GR (regno, newval);
}

/* Get the value of h-gr_double.  */

DI
frvbf_h_gr_double_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_GR_DOUBLE (regno);
}

/* Set a value for h-gr_double.  */

void
frvbf_h_gr_double_set (SIM_CPU *current_cpu, UINT regno, DI newval)
{
  SET_H_GR_DOUBLE (regno, newval);
}

/* Get the value of h-gr_hi.  */

UHI
frvbf_h_gr_hi_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_GR_HI (regno);
}

/* Set a value for h-gr_hi.  */

void
frvbf_h_gr_hi_set (SIM_CPU *current_cpu, UINT regno, UHI newval)
{
  SET_H_GR_HI (regno, newval);
}

/* Get the value of h-gr_lo.  */

UHI
frvbf_h_gr_lo_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_GR_LO (regno);
}

/* Set a value for h-gr_lo.  */

void
frvbf_h_gr_lo_set (SIM_CPU *current_cpu, UINT regno, UHI newval)
{
  SET_H_GR_LO (regno, newval);
}

/* Get the value of h-fr.  */

SF
frvbf_h_fr_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_FR (regno);
}

/* Set a value for h-fr.  */

void
frvbf_h_fr_set (SIM_CPU *current_cpu, UINT regno, SF newval)
{
  SET_H_FR (regno, newval);
}

/* Get the value of h-fr_double.  */

DF
frvbf_h_fr_double_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_FR_DOUBLE (regno);
}

/* Set a value for h-fr_double.  */

void
frvbf_h_fr_double_set (SIM_CPU *current_cpu, UINT regno, DF newval)
{
  SET_H_FR_DOUBLE (regno, newval);
}

/* Get the value of h-fr_int.  */

USI
frvbf_h_fr_int_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_FR_INT (regno);
}

/* Set a value for h-fr_int.  */

void
frvbf_h_fr_int_set (SIM_CPU *current_cpu, UINT regno, USI newval)
{
  SET_H_FR_INT (regno, newval);
}

/* Get the value of h-fr_hi.  */

UHI
frvbf_h_fr_hi_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_FR_HI (regno);
}

/* Set a value for h-fr_hi.  */

void
frvbf_h_fr_hi_set (SIM_CPU *current_cpu, UINT regno, UHI newval)
{
  SET_H_FR_HI (regno, newval);
}

/* Get the value of h-fr_lo.  */

UHI
frvbf_h_fr_lo_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_FR_LO (regno);
}

/* Set a value for h-fr_lo.  */

void
frvbf_h_fr_lo_set (SIM_CPU *current_cpu, UINT regno, UHI newval)
{
  SET_H_FR_LO (regno, newval);
}

/* Get the value of h-fr_0.  */

UHI
frvbf_h_fr_0_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_FR_0 (regno);
}

/* Set a value for h-fr_0.  */

void
frvbf_h_fr_0_set (SIM_CPU *current_cpu, UINT regno, UHI newval)
{
  SET_H_FR_0 (regno, newval);
}

/* Get the value of h-fr_1.  */

UHI
frvbf_h_fr_1_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_FR_1 (regno);
}

/* Set a value for h-fr_1.  */

void
frvbf_h_fr_1_set (SIM_CPU *current_cpu, UINT regno, UHI newval)
{
  SET_H_FR_1 (regno, newval);
}

/* Get the value of h-fr_2.  */

UHI
frvbf_h_fr_2_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_FR_2 (regno);
}

/* Set a value for h-fr_2.  */

void
frvbf_h_fr_2_set (SIM_CPU *current_cpu, UINT regno, UHI newval)
{
  SET_H_FR_2 (regno, newval);
}

/* Get the value of h-fr_3.  */

UHI
frvbf_h_fr_3_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_FR_3 (regno);
}

/* Set a value for h-fr_3.  */

void
frvbf_h_fr_3_set (SIM_CPU *current_cpu, UINT regno, UHI newval)
{
  SET_H_FR_3 (regno, newval);
}

/* Get the value of h-cpr.  */

SI
frvbf_h_cpr_get (SIM_CPU *current_cpu, UINT regno)
{
  return CPU (h_cpr[regno]);
}

/* Set a value for h-cpr.  */

void
frvbf_h_cpr_set (SIM_CPU *current_cpu, UINT regno, SI newval)
{
  CPU (h_cpr[regno]) = newval;
}

/* Get the value of h-cpr_double.  */

DI
frvbf_h_cpr_double_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_CPR_DOUBLE (regno);
}

/* Set a value for h-cpr_double.  */

void
frvbf_h_cpr_double_set (SIM_CPU *current_cpu, UINT regno, DI newval)
{
  SET_H_CPR_DOUBLE (regno, newval);
}

/* Get the value of h-spr.  */

USI
frvbf_h_spr_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_SPR (regno);
}

/* Set a value for h-spr.  */

void
frvbf_h_spr_set (SIM_CPU *current_cpu, UINT regno, USI newval)
{
  SET_H_SPR (regno, newval);
}

/* Get the value of h-accg.  */

USI
frvbf_h_accg_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_ACCG (regno);
}

/* Set a value for h-accg.  */

void
frvbf_h_accg_set (SIM_CPU *current_cpu, UINT regno, USI newval)
{
  SET_H_ACCG (regno, newval);
}

/* Get the value of h-acc40S.  */

DI
frvbf_h_acc40S_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_ACC40S (regno);
}

/* Set a value for h-acc40S.  */

void
frvbf_h_acc40S_set (SIM_CPU *current_cpu, UINT regno, DI newval)
{
  SET_H_ACC40S (regno, newval);
}

/* Get the value of h-acc40U.  */

UDI
frvbf_h_acc40U_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_ACC40U (regno);
}

/* Set a value for h-acc40U.  */

void
frvbf_h_acc40U_set (SIM_CPU *current_cpu, UINT regno, UDI newval)
{
  SET_H_ACC40U (regno, newval);
}

/* Get the value of h-iacc0.  */

DI
frvbf_h_iacc0_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_IACC0 (regno);
}

/* Set a value for h-iacc0.  */

void
frvbf_h_iacc0_set (SIM_CPU *current_cpu, UINT regno, DI newval)
{
  SET_H_IACC0 (regno, newval);
}

/* Get the value of h-iccr.  */

UQI
frvbf_h_iccr_get (SIM_CPU *current_cpu, UINT regno)
{
  return CPU (h_iccr[regno]);
}

/* Set a value for h-iccr.  */

void
frvbf_h_iccr_set (SIM_CPU *current_cpu, UINT regno, UQI newval)
{
  CPU (h_iccr[regno]) = newval;
}

/* Get the value of h-fccr.  */

UQI
frvbf_h_fccr_get (SIM_CPU *current_cpu, UINT regno)
{
  return CPU (h_fccr[regno]);
}

/* Set a value for h-fccr.  */

void
frvbf_h_fccr_set (SIM_CPU *current_cpu, UINT regno, UQI newval)
{
  CPU (h_fccr[regno]) = newval;
}

/* Get the value of h-cccr.  */

UQI
frvbf_h_cccr_get (SIM_CPU *current_cpu, UINT regno)
{
  return CPU (h_cccr[regno]);
}

/* Set a value for h-cccr.  */

void
frvbf_h_cccr_set (SIM_CPU *current_cpu, UINT regno, UQI newval)
{
  CPU (h_cccr[regno]) = newval;
}

/* Record trace results for INSN.  */

void
frvbf_record_trace_results (SIM_CPU *current_cpu, CGEN_INSN *insn,
			    int *indices, TRACE_RECORD *tr)
{
}
