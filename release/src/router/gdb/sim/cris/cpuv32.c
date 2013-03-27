/* Misc. support for CPU family crisv32f.

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

#define WANT_CPU crisv32f
#define WANT_CPU_CRISV32F

#include "sim-main.h"
#include "cgen-ops.h"

/* Get the value of h-v32-v32.  */

BI
crisv32f_h_v32_v32_get (SIM_CPU *current_cpu)
{
  return GET_H_V32_V32 ();
}

/* Set a value for h-v32-v32.  */

void
crisv32f_h_v32_v32_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_V32_V32 (newval);
}

/* Get the value of h-pc.  */

USI
crisv32f_h_pc_get (SIM_CPU *current_cpu)
{
  return CPU (h_pc);
}

/* Set a value for h-pc.  */

void
crisv32f_h_pc_set (SIM_CPU *current_cpu, USI newval)
{
  SET_H_PC (newval);
}

/* Get the value of h-gr.  */

SI
crisv32f_h_gr_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_GR (regno);
}

/* Set a value for h-gr.  */

void
crisv32f_h_gr_set (SIM_CPU *current_cpu, UINT regno, SI newval)
{
  SET_H_GR (regno, newval);
}

/* Get the value of h-gr-acr.  */

SI
crisv32f_h_gr_acr_get (SIM_CPU *current_cpu, UINT regno)
{
  return CPU (h_gr_acr[regno]);
}

/* Set a value for h-gr-acr.  */

void
crisv32f_h_gr_acr_set (SIM_CPU *current_cpu, UINT regno, SI newval)
{
  CPU (h_gr_acr[regno]) = newval;
}

/* Get the value of h-raw-gr-acr.  */

SI
crisv32f_h_raw_gr_acr_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_RAW_GR_ACR (regno);
}

/* Set a value for h-raw-gr-acr.  */

void
crisv32f_h_raw_gr_acr_set (SIM_CPU *current_cpu, UINT regno, SI newval)
{
  SET_H_RAW_GR_ACR (regno, newval);
}

/* Get the value of h-sr.  */

SI
crisv32f_h_sr_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_SR (regno);
}

/* Set a value for h-sr.  */

void
crisv32f_h_sr_set (SIM_CPU *current_cpu, UINT regno, SI newval)
{
  SET_H_SR (regno, newval);
}

/* Get the value of h-sr-v32.  */

SI
crisv32f_h_sr_v32_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_SR_V32 (regno);
}

/* Set a value for h-sr-v32.  */

void
crisv32f_h_sr_v32_set (SIM_CPU *current_cpu, UINT regno, SI newval)
{
  SET_H_SR_V32 (regno, newval);
}

/* Get the value of h-supr.  */

SI
crisv32f_h_supr_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_SUPR (regno);
}

/* Set a value for h-supr.  */

void
crisv32f_h_supr_set (SIM_CPU *current_cpu, UINT regno, SI newval)
{
  SET_H_SUPR (regno, newval);
}

/* Get the value of h-cbit.  */

BI
crisv32f_h_cbit_get (SIM_CPU *current_cpu)
{
  return CPU (h_cbit);
}

/* Set a value for h-cbit.  */

void
crisv32f_h_cbit_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_cbit) = newval;
}

/* Get the value of h-cbit-move.  */

BI
crisv32f_h_cbit_move_get (SIM_CPU *current_cpu)
{
  return GET_H_CBIT_MOVE ();
}

/* Set a value for h-cbit-move.  */

void
crisv32f_h_cbit_move_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_CBIT_MOVE (newval);
}

/* Get the value of h-cbit-move-v32.  */

BI
crisv32f_h_cbit_move_v32_get (SIM_CPU *current_cpu)
{
  return GET_H_CBIT_MOVE_V32 ();
}

/* Set a value for h-cbit-move-v32.  */

void
crisv32f_h_cbit_move_v32_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_CBIT_MOVE_V32 (newval);
}

/* Get the value of h-vbit.  */

BI
crisv32f_h_vbit_get (SIM_CPU *current_cpu)
{
  return CPU (h_vbit);
}

/* Set a value for h-vbit.  */

void
crisv32f_h_vbit_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_vbit) = newval;
}

/* Get the value of h-vbit-move.  */

BI
crisv32f_h_vbit_move_get (SIM_CPU *current_cpu)
{
  return GET_H_VBIT_MOVE ();
}

/* Set a value for h-vbit-move.  */

void
crisv32f_h_vbit_move_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_VBIT_MOVE (newval);
}

/* Get the value of h-vbit-move-v32.  */

BI
crisv32f_h_vbit_move_v32_get (SIM_CPU *current_cpu)
{
  return GET_H_VBIT_MOVE_V32 ();
}

/* Set a value for h-vbit-move-v32.  */

void
crisv32f_h_vbit_move_v32_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_VBIT_MOVE_V32 (newval);
}

/* Get the value of h-zbit.  */

BI
crisv32f_h_zbit_get (SIM_CPU *current_cpu)
{
  return CPU (h_zbit);
}

/* Set a value for h-zbit.  */

void
crisv32f_h_zbit_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_zbit) = newval;
}

/* Get the value of h-zbit-move.  */

BI
crisv32f_h_zbit_move_get (SIM_CPU *current_cpu)
{
  return GET_H_ZBIT_MOVE ();
}

/* Set a value for h-zbit-move.  */

void
crisv32f_h_zbit_move_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_ZBIT_MOVE (newval);
}

/* Get the value of h-zbit-move-v32.  */

BI
crisv32f_h_zbit_move_v32_get (SIM_CPU *current_cpu)
{
  return GET_H_ZBIT_MOVE_V32 ();
}

/* Set a value for h-zbit-move-v32.  */

void
crisv32f_h_zbit_move_v32_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_ZBIT_MOVE_V32 (newval);
}

/* Get the value of h-nbit.  */

BI
crisv32f_h_nbit_get (SIM_CPU *current_cpu)
{
  return CPU (h_nbit);
}

/* Set a value for h-nbit.  */

void
crisv32f_h_nbit_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_nbit) = newval;
}

/* Get the value of h-nbit-move.  */

BI
crisv32f_h_nbit_move_get (SIM_CPU *current_cpu)
{
  return GET_H_NBIT_MOVE ();
}

/* Set a value for h-nbit-move.  */

void
crisv32f_h_nbit_move_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_NBIT_MOVE (newval);
}

/* Get the value of h-nbit-move-v32.  */

BI
crisv32f_h_nbit_move_v32_get (SIM_CPU *current_cpu)
{
  return GET_H_NBIT_MOVE_V32 ();
}

/* Set a value for h-nbit-move-v32.  */

void
crisv32f_h_nbit_move_v32_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_NBIT_MOVE_V32 (newval);
}

/* Get the value of h-xbit.  */

BI
crisv32f_h_xbit_get (SIM_CPU *current_cpu)
{
  return CPU (h_xbit);
}

/* Set a value for h-xbit.  */

void
crisv32f_h_xbit_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_xbit) = newval;
}

/* Get the value of h-ibit.  */

BI
crisv32f_h_ibit_get (SIM_CPU *current_cpu)
{
  return GET_H_IBIT ();
}

/* Set a value for h-ibit.  */

void
crisv32f_h_ibit_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_IBIT (newval);
}

/* Get the value of h-pbit.  */

BI
crisv32f_h_pbit_get (SIM_CPU *current_cpu)
{
  return CPU (h_pbit);
}

/* Set a value for h-pbit.  */

void
crisv32f_h_pbit_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_pbit) = newval;
}

/* Get the value of h-rbit.  */

BI
crisv32f_h_rbit_get (SIM_CPU *current_cpu)
{
  return CPU (h_rbit);
}

/* Set a value for h-rbit.  */

void
crisv32f_h_rbit_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_rbit) = newval;
}

/* Get the value of h-ubit.  */

BI
crisv32f_h_ubit_get (SIM_CPU *current_cpu)
{
  return GET_H_UBIT ();
}

/* Set a value for h-ubit.  */

void
crisv32f_h_ubit_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_UBIT (newval);
}

/* Get the value of h-gbit.  */

BI
crisv32f_h_gbit_get (SIM_CPU *current_cpu)
{
  return CPU (h_gbit);
}

/* Set a value for h-gbit.  */

void
crisv32f_h_gbit_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_gbit) = newval;
}

/* Get the value of h-kernel-sp.  */

SI
crisv32f_h_kernel_sp_get (SIM_CPU *current_cpu)
{
  return CPU (h_kernel_sp);
}

/* Set a value for h-kernel-sp.  */

void
crisv32f_h_kernel_sp_set (SIM_CPU *current_cpu, SI newval)
{
  CPU (h_kernel_sp) = newval;
}

/* Get the value of h-ubit-v32.  */

BI
crisv32f_h_ubit_v32_get (SIM_CPU *current_cpu)
{
  return CPU (h_ubit_v32);
}

/* Set a value for h-ubit-v32.  */

void
crisv32f_h_ubit_v32_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_UBIT_V32 (newval);
}

/* Get the value of h-ibit-v32.  */

BI
crisv32f_h_ibit_v32_get (SIM_CPU *current_cpu)
{
  return CPU (h_ibit_v32);
}

/* Set a value for h-ibit-v32.  */

void
crisv32f_h_ibit_v32_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_IBIT_V32 (newval);
}

/* Get the value of h-mbit.  */

BI
crisv32f_h_mbit_get (SIM_CPU *current_cpu)
{
  return CPU (h_mbit);
}

/* Set a value for h-mbit.  */

void
crisv32f_h_mbit_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_MBIT (newval);
}

/* Get the value of h-qbit.  */

BI
crisv32f_h_qbit_get (SIM_CPU *current_cpu)
{
  return CPU (h_qbit);
}

/* Set a value for h-qbit.  */

void
crisv32f_h_qbit_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_QBIT (newval);
}

/* Get the value of h-sbit.  */

BI
crisv32f_h_sbit_get (SIM_CPU *current_cpu)
{
  return CPU (h_sbit);
}

/* Set a value for h-sbit.  */

void
crisv32f_h_sbit_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_SBIT (newval);
}

/* Get the value of h-insn-prefixed-p.  */

BI
crisv32f_h_insn_prefixed_p_get (SIM_CPU *current_cpu)
{
  return GET_H_INSN_PREFIXED_P ();
}

/* Set a value for h-insn-prefixed-p.  */

void
crisv32f_h_insn_prefixed_p_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_INSN_PREFIXED_P (newval);
}

/* Get the value of h-insn-prefixed-p-v32.  */

BI
crisv32f_h_insn_prefixed_p_v32_get (SIM_CPU *current_cpu)
{
  return GET_H_INSN_PREFIXED_P_V32 ();
}

/* Set a value for h-insn-prefixed-p-v32.  */

void
crisv32f_h_insn_prefixed_p_v32_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_INSN_PREFIXED_P_V32 (newval);
}

/* Get the value of h-prefixreg-v32.  */

SI
crisv32f_h_prefixreg_v32_get (SIM_CPU *current_cpu)
{
  return GET_H_PREFIXREG_V32 ();
}

/* Set a value for h-prefixreg-v32.  */

void
crisv32f_h_prefixreg_v32_set (SIM_CPU *current_cpu, SI newval)
{
  SET_H_PREFIXREG_V32 (newval);
}

/* Record trace results for INSN.  */

void
crisv32f_record_trace_results (SIM_CPU *current_cpu, CGEN_INSN *insn,
			    int *indices, TRACE_RECORD *tr)
{
}
