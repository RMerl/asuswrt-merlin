/* Misc. support for CPU family crisv10f.

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

#define WANT_CPU crisv10f
#define WANT_CPU_CRISV10F

#include "sim-main.h"
#include "cgen-ops.h"

/* Get the value of h-v32-non-v32.  */

BI
crisv10f_h_v32_non_v32_get (SIM_CPU *current_cpu)
{
  return GET_H_V32_NON_V32 ();
}

/* Set a value for h-v32-non-v32.  */

void
crisv10f_h_v32_non_v32_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_V32_NON_V32 (newval);
}

/* Get the value of h-pc.  */

USI
crisv10f_h_pc_get (SIM_CPU *current_cpu)
{
  return CPU (h_pc);
}

/* Set a value for h-pc.  */

void
crisv10f_h_pc_set (SIM_CPU *current_cpu, USI newval)
{
  SET_H_PC (newval);
}

/* Get the value of h-gr.  */

SI
crisv10f_h_gr_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_GR (regno);
}

/* Set a value for h-gr.  */

void
crisv10f_h_gr_set (SIM_CPU *current_cpu, UINT regno, SI newval)
{
  SET_H_GR (regno, newval);
}

/* Get the value of h-gr-pc.  */

SI
crisv10f_h_gr_pc_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_GR_PC (regno);
}

/* Set a value for h-gr-pc.  */

void
crisv10f_h_gr_pc_set (SIM_CPU *current_cpu, UINT regno, SI newval)
{
  SET_H_GR_PC (regno, newval);
}

/* Get the value of h-gr-real-pc.  */

SI
crisv10f_h_gr_real_pc_get (SIM_CPU *current_cpu, UINT regno)
{
  return CPU (h_gr_real_pc[regno]);
}

/* Set a value for h-gr-real-pc.  */

void
crisv10f_h_gr_real_pc_set (SIM_CPU *current_cpu, UINT regno, SI newval)
{
  CPU (h_gr_real_pc[regno]) = newval;
}

/* Get the value of h-raw-gr-pc.  */

SI
crisv10f_h_raw_gr_pc_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_RAW_GR_PC (regno);
}

/* Set a value for h-raw-gr-pc.  */

void
crisv10f_h_raw_gr_pc_set (SIM_CPU *current_cpu, UINT regno, SI newval)
{
  SET_H_RAW_GR_PC (regno, newval);
}

/* Get the value of h-sr.  */

SI
crisv10f_h_sr_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_SR (regno);
}

/* Set a value for h-sr.  */

void
crisv10f_h_sr_set (SIM_CPU *current_cpu, UINT regno, SI newval)
{
  SET_H_SR (regno, newval);
}

/* Get the value of h-sr-v10.  */

SI
crisv10f_h_sr_v10_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_SR_V10 (regno);
}

/* Set a value for h-sr-v10.  */

void
crisv10f_h_sr_v10_set (SIM_CPU *current_cpu, UINT regno, SI newval)
{
  SET_H_SR_V10 (regno, newval);
}

/* Get the value of h-cbit.  */

BI
crisv10f_h_cbit_get (SIM_CPU *current_cpu)
{
  return CPU (h_cbit);
}

/* Set a value for h-cbit.  */

void
crisv10f_h_cbit_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_cbit) = newval;
}

/* Get the value of h-cbit-move.  */

BI
crisv10f_h_cbit_move_get (SIM_CPU *current_cpu)
{
  return GET_H_CBIT_MOVE ();
}

/* Set a value for h-cbit-move.  */

void
crisv10f_h_cbit_move_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_CBIT_MOVE (newval);
}

/* Get the value of h-cbit-move-pre-v32.  */

BI
crisv10f_h_cbit_move_pre_v32_get (SIM_CPU *current_cpu)
{
  return GET_H_CBIT_MOVE_PRE_V32 ();
}

/* Set a value for h-cbit-move-pre-v32.  */

void
crisv10f_h_cbit_move_pre_v32_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_CBIT_MOVE_PRE_V32 (newval);
}

/* Get the value of h-vbit.  */

BI
crisv10f_h_vbit_get (SIM_CPU *current_cpu)
{
  return CPU (h_vbit);
}

/* Set a value for h-vbit.  */

void
crisv10f_h_vbit_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_vbit) = newval;
}

/* Get the value of h-vbit-move.  */

BI
crisv10f_h_vbit_move_get (SIM_CPU *current_cpu)
{
  return GET_H_VBIT_MOVE ();
}

/* Set a value for h-vbit-move.  */

void
crisv10f_h_vbit_move_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_VBIT_MOVE (newval);
}

/* Get the value of h-vbit-move-pre-v32.  */

BI
crisv10f_h_vbit_move_pre_v32_get (SIM_CPU *current_cpu)
{
  return GET_H_VBIT_MOVE_PRE_V32 ();
}

/* Set a value for h-vbit-move-pre-v32.  */

void
crisv10f_h_vbit_move_pre_v32_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_VBIT_MOVE_PRE_V32 (newval);
}

/* Get the value of h-zbit.  */

BI
crisv10f_h_zbit_get (SIM_CPU *current_cpu)
{
  return CPU (h_zbit);
}

/* Set a value for h-zbit.  */

void
crisv10f_h_zbit_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_zbit) = newval;
}

/* Get the value of h-zbit-move.  */

BI
crisv10f_h_zbit_move_get (SIM_CPU *current_cpu)
{
  return GET_H_ZBIT_MOVE ();
}

/* Set a value for h-zbit-move.  */

void
crisv10f_h_zbit_move_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_ZBIT_MOVE (newval);
}

/* Get the value of h-zbit-move-pre-v32.  */

BI
crisv10f_h_zbit_move_pre_v32_get (SIM_CPU *current_cpu)
{
  return GET_H_ZBIT_MOVE_PRE_V32 ();
}

/* Set a value for h-zbit-move-pre-v32.  */

void
crisv10f_h_zbit_move_pre_v32_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_ZBIT_MOVE_PRE_V32 (newval);
}

/* Get the value of h-nbit.  */

BI
crisv10f_h_nbit_get (SIM_CPU *current_cpu)
{
  return CPU (h_nbit);
}

/* Set a value for h-nbit.  */

void
crisv10f_h_nbit_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_nbit) = newval;
}

/* Get the value of h-nbit-move.  */

BI
crisv10f_h_nbit_move_get (SIM_CPU *current_cpu)
{
  return GET_H_NBIT_MOVE ();
}

/* Set a value for h-nbit-move.  */

void
crisv10f_h_nbit_move_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_NBIT_MOVE (newval);
}

/* Get the value of h-nbit-move-pre-v32.  */

BI
crisv10f_h_nbit_move_pre_v32_get (SIM_CPU *current_cpu)
{
  return GET_H_NBIT_MOVE_PRE_V32 ();
}

/* Set a value for h-nbit-move-pre-v32.  */

void
crisv10f_h_nbit_move_pre_v32_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_NBIT_MOVE_PRE_V32 (newval);
}

/* Get the value of h-xbit.  */

BI
crisv10f_h_xbit_get (SIM_CPU *current_cpu)
{
  return CPU (h_xbit);
}

/* Set a value for h-xbit.  */

void
crisv10f_h_xbit_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_xbit) = newval;
}

/* Get the value of h-ibit.  */

BI
crisv10f_h_ibit_get (SIM_CPU *current_cpu)
{
  return GET_H_IBIT ();
}

/* Set a value for h-ibit.  */

void
crisv10f_h_ibit_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_IBIT (newval);
}

/* Get the value of h-ibit-pre-v32.  */

BI
crisv10f_h_ibit_pre_v32_get (SIM_CPU *current_cpu)
{
  return CPU (h_ibit_pre_v32);
}

/* Set a value for h-ibit-pre-v32.  */

void
crisv10f_h_ibit_pre_v32_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_ibit_pre_v32) = newval;
}

/* Get the value of h-pbit.  */

BI
crisv10f_h_pbit_get (SIM_CPU *current_cpu)
{
  return CPU (h_pbit);
}

/* Set a value for h-pbit.  */

void
crisv10f_h_pbit_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_pbit) = newval;
}

/* Get the value of h-ubit.  */

BI
crisv10f_h_ubit_get (SIM_CPU *current_cpu)
{
  return GET_H_UBIT ();
}

/* Set a value for h-ubit.  */

void
crisv10f_h_ubit_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_UBIT (newval);
}

/* Get the value of h-ubit-pre-v32.  */

BI
crisv10f_h_ubit_pre_v32_get (SIM_CPU *current_cpu)
{
  return CPU (h_ubit_pre_v32);
}

/* Set a value for h-ubit-pre-v32.  */

void
crisv10f_h_ubit_pre_v32_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_ubit_pre_v32) = newval;
}

/* Get the value of h-insn-prefixed-p.  */

BI
crisv10f_h_insn_prefixed_p_get (SIM_CPU *current_cpu)
{
  return GET_H_INSN_PREFIXED_P ();
}

/* Set a value for h-insn-prefixed-p.  */

void
crisv10f_h_insn_prefixed_p_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_INSN_PREFIXED_P (newval);
}

/* Get the value of h-insn-prefixed-p-pre-v32.  */

BI
crisv10f_h_insn_prefixed_p_pre_v32_get (SIM_CPU *current_cpu)
{
  return CPU (h_insn_prefixed_p_pre_v32);
}

/* Set a value for h-insn-prefixed-p-pre-v32.  */

void
crisv10f_h_insn_prefixed_p_pre_v32_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_insn_prefixed_p_pre_v32) = newval;
}

/* Get the value of h-prefixreg-pre-v32.  */

SI
crisv10f_h_prefixreg_pre_v32_get (SIM_CPU *current_cpu)
{
  return CPU (h_prefixreg_pre_v32);
}

/* Set a value for h-prefixreg-pre-v32.  */

void
crisv10f_h_prefixreg_pre_v32_set (SIM_CPU *current_cpu, SI newval)
{
  CPU (h_prefixreg_pre_v32) = newval;
}

/* Record trace results for INSN.  */

void
crisv10f_record_trace_results (SIM_CPU *current_cpu, CGEN_INSN *insn,
			    int *indices, TRACE_RECORD *tr)
{
}
