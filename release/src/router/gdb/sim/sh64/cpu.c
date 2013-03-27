/* Misc. support for CPU family sh64.

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

#define WANT_CPU sh64
#define WANT_CPU_SH64

#include "sim-main.h"
#include "cgen-ops.h"

/* Get the value of h-pc.  */

UDI
sh64_h_pc_get (SIM_CPU *current_cpu)
{
  return GET_H_PC ();
}

/* Set a value for h-pc.  */

void
sh64_h_pc_set (SIM_CPU *current_cpu, UDI newval)
{
  SET_H_PC (newval);
}

/* Get the value of h-gr.  */

DI
sh64_h_gr_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_GR (regno);
}

/* Set a value for h-gr.  */

void
sh64_h_gr_set (SIM_CPU *current_cpu, UINT regno, DI newval)
{
  SET_H_GR (regno, newval);
}

/* Get the value of h-grc.  */

SI
sh64_h_grc_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_GRC (regno);
}

/* Set a value for h-grc.  */

void
sh64_h_grc_set (SIM_CPU *current_cpu, UINT regno, SI newval)
{
  SET_H_GRC (regno, newval);
}

/* Get the value of h-cr.  */

DI
sh64_h_cr_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_CR (regno);
}

/* Set a value for h-cr.  */

void
sh64_h_cr_set (SIM_CPU *current_cpu, UINT regno, DI newval)
{
  SET_H_CR (regno, newval);
}

/* Get the value of h-sr.  */

SI
sh64_h_sr_get (SIM_CPU *current_cpu)
{
  return CPU (h_sr);
}

/* Set a value for h-sr.  */

void
sh64_h_sr_set (SIM_CPU *current_cpu, SI newval)
{
  CPU (h_sr) = newval;
}

/* Get the value of h-fpscr.  */

SI
sh64_h_fpscr_get (SIM_CPU *current_cpu)
{
  return CPU (h_fpscr);
}

/* Set a value for h-fpscr.  */

void
sh64_h_fpscr_set (SIM_CPU *current_cpu, SI newval)
{
  CPU (h_fpscr) = newval;
}

/* Get the value of h-frbit.  */

BI
sh64_h_frbit_get (SIM_CPU *current_cpu)
{
  return GET_H_FRBIT ();
}

/* Set a value for h-frbit.  */

void
sh64_h_frbit_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_FRBIT (newval);
}

/* Get the value of h-szbit.  */

BI
sh64_h_szbit_get (SIM_CPU *current_cpu)
{
  return GET_H_SZBIT ();
}

/* Set a value for h-szbit.  */

void
sh64_h_szbit_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_SZBIT (newval);
}

/* Get the value of h-prbit.  */

BI
sh64_h_prbit_get (SIM_CPU *current_cpu)
{
  return GET_H_PRBIT ();
}

/* Set a value for h-prbit.  */

void
sh64_h_prbit_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_PRBIT (newval);
}

/* Get the value of h-sbit.  */

BI
sh64_h_sbit_get (SIM_CPU *current_cpu)
{
  return GET_H_SBIT ();
}

/* Set a value for h-sbit.  */

void
sh64_h_sbit_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_SBIT (newval);
}

/* Get the value of h-mbit.  */

BI
sh64_h_mbit_get (SIM_CPU *current_cpu)
{
  return GET_H_MBIT ();
}

/* Set a value for h-mbit.  */

void
sh64_h_mbit_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_MBIT (newval);
}

/* Get the value of h-qbit.  */

BI
sh64_h_qbit_get (SIM_CPU *current_cpu)
{
  return GET_H_QBIT ();
}

/* Set a value for h-qbit.  */

void
sh64_h_qbit_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_QBIT (newval);
}

/* Get the value of h-fr.  */

SF
sh64_h_fr_get (SIM_CPU *current_cpu, UINT regno)
{
  return CPU (h_fr[regno]);
}

/* Set a value for h-fr.  */

void
sh64_h_fr_set (SIM_CPU *current_cpu, UINT regno, SF newval)
{
  CPU (h_fr[regno]) = newval;
}

/* Get the value of h-fp.  */

SF
sh64_h_fp_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_FP (regno);
}

/* Set a value for h-fp.  */

void
sh64_h_fp_set (SIM_CPU *current_cpu, UINT regno, SF newval)
{
  SET_H_FP (regno, newval);
}

/* Get the value of h-fv.  */

SF
sh64_h_fv_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_FV (regno);
}

/* Set a value for h-fv.  */

void
sh64_h_fv_set (SIM_CPU *current_cpu, UINT regno, SF newval)
{
  SET_H_FV (regno, newval);
}

/* Get the value of h-fmtx.  */

SF
sh64_h_fmtx_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_FMTX (regno);
}

/* Set a value for h-fmtx.  */

void
sh64_h_fmtx_set (SIM_CPU *current_cpu, UINT regno, SF newval)
{
  SET_H_FMTX (regno, newval);
}

/* Get the value of h-dr.  */

DF
sh64_h_dr_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_DR (regno);
}

/* Set a value for h-dr.  */

void
sh64_h_dr_set (SIM_CPU *current_cpu, UINT regno, DF newval)
{
  SET_H_DR (regno, newval);
}

/* Get the value of h-fsd.  */

DF
sh64_h_fsd_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_FSD (regno);
}

/* Set a value for h-fsd.  */

void
sh64_h_fsd_set (SIM_CPU *current_cpu, UINT regno, DF newval)
{
  SET_H_FSD (regno, newval);
}

/* Get the value of h-fmov.  */

DF
sh64_h_fmov_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_FMOV (regno);
}

/* Set a value for h-fmov.  */

void
sh64_h_fmov_set (SIM_CPU *current_cpu, UINT regno, DF newval)
{
  SET_H_FMOV (regno, newval);
}

/* Get the value of h-tr.  */

DI
sh64_h_tr_get (SIM_CPU *current_cpu, UINT regno)
{
  return CPU (h_tr[regno]);
}

/* Set a value for h-tr.  */

void
sh64_h_tr_set (SIM_CPU *current_cpu, UINT regno, DI newval)
{
  CPU (h_tr[regno]) = newval;
}

/* Get the value of h-endian.  */

BI
sh64_h_endian_get (SIM_CPU *current_cpu)
{
  return GET_H_ENDIAN ();
}

/* Set a value for h-endian.  */

void
sh64_h_endian_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_ENDIAN (newval);
}

/* Get the value of h-ism.  */

BI
sh64_h_ism_get (SIM_CPU *current_cpu)
{
  return GET_H_ISM ();
}

/* Set a value for h-ism.  */

void
sh64_h_ism_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_ISM (newval);
}

/* Get the value of h-frc.  */

SF
sh64_h_frc_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_FRC (regno);
}

/* Set a value for h-frc.  */

void
sh64_h_frc_set (SIM_CPU *current_cpu, UINT regno, SF newval)
{
  SET_H_FRC (regno, newval);
}

/* Get the value of h-drc.  */

DF
sh64_h_drc_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_DRC (regno);
}

/* Set a value for h-drc.  */

void
sh64_h_drc_set (SIM_CPU *current_cpu, UINT regno, DF newval)
{
  SET_H_DRC (regno, newval);
}

/* Get the value of h-xf.  */

SF
sh64_h_xf_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_XF (regno);
}

/* Set a value for h-xf.  */

void
sh64_h_xf_set (SIM_CPU *current_cpu, UINT regno, SF newval)
{
  SET_H_XF (regno, newval);
}

/* Get the value of h-xd.  */

DF
sh64_h_xd_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_XD (regno);
}

/* Set a value for h-xd.  */

void
sh64_h_xd_set (SIM_CPU *current_cpu, UINT regno, DF newval)
{
  SET_H_XD (regno, newval);
}

/* Get the value of h-fvc.  */

SF
sh64_h_fvc_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_FVC (regno);
}

/* Set a value for h-fvc.  */

void
sh64_h_fvc_set (SIM_CPU *current_cpu, UINT regno, SF newval)
{
  SET_H_FVC (regno, newval);
}

/* Get the value of h-gbr.  */

SI
sh64_h_gbr_get (SIM_CPU *current_cpu)
{
  return GET_H_GBR ();
}

/* Set a value for h-gbr.  */

void
sh64_h_gbr_set (SIM_CPU *current_cpu, SI newval)
{
  SET_H_GBR (newval);
}

/* Get the value of h-vbr.  */

SI
sh64_h_vbr_get (SIM_CPU *current_cpu)
{
  return GET_H_VBR ();
}

/* Set a value for h-vbr.  */

void
sh64_h_vbr_set (SIM_CPU *current_cpu, SI newval)
{
  SET_H_VBR (newval);
}

/* Get the value of h-pr.  */

SI
sh64_h_pr_get (SIM_CPU *current_cpu)
{
  return GET_H_PR ();
}

/* Set a value for h-pr.  */

void
sh64_h_pr_set (SIM_CPU *current_cpu, SI newval)
{
  SET_H_PR (newval);
}

/* Get the value of h-macl.  */

SI
sh64_h_macl_get (SIM_CPU *current_cpu)
{
  return GET_H_MACL ();
}

/* Set a value for h-macl.  */

void
sh64_h_macl_set (SIM_CPU *current_cpu, SI newval)
{
  SET_H_MACL (newval);
}

/* Get the value of h-mach.  */

SI
sh64_h_mach_get (SIM_CPU *current_cpu)
{
  return GET_H_MACH ();
}

/* Set a value for h-mach.  */

void
sh64_h_mach_set (SIM_CPU *current_cpu, SI newval)
{
  SET_H_MACH (newval);
}

/* Get the value of h-tbit.  */

BI
sh64_h_tbit_get (SIM_CPU *current_cpu)
{
  return GET_H_TBIT ();
}

/* Set a value for h-tbit.  */

void
sh64_h_tbit_set (SIM_CPU *current_cpu, BI newval)
{
  SET_H_TBIT (newval);
}

/* Record trace results for INSN.  */

void
sh64_record_trace_results (SIM_CPU *current_cpu, CGEN_INSN *insn,
			    int *indices, TRACE_RECORD *tr)
{
}
