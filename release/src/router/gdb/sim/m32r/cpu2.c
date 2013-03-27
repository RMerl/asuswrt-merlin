/* Misc. support for CPU family m32r2f.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

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

#define WANT_CPU m32r2f
#define WANT_CPU_M32R2F

#include "sim-main.h"
#include "cgen-ops.h"

/* Get the value of h-pc.  */

USI
m32r2f_h_pc_get (SIM_CPU *current_cpu)
{
  return CPU (h_pc);
}

/* Set a value for h-pc.  */

void
m32r2f_h_pc_set (SIM_CPU *current_cpu, USI newval)
{
  CPU (h_pc) = newval;
}

/* Get the value of h-gr.  */

SI
m32r2f_h_gr_get (SIM_CPU *current_cpu, UINT regno)
{
  return CPU (h_gr[regno]);
}

/* Set a value for h-gr.  */

void
m32r2f_h_gr_set (SIM_CPU *current_cpu, UINT regno, SI newval)
{
  CPU (h_gr[regno]) = newval;
}

/* Get the value of h-cr.  */

USI
m32r2f_h_cr_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_CR (regno);
}

/* Set a value for h-cr.  */

void
m32r2f_h_cr_set (SIM_CPU *current_cpu, UINT regno, USI newval)
{
  SET_H_CR (regno, newval);
}

/* Get the value of h-accum.  */

DI
m32r2f_h_accum_get (SIM_CPU *current_cpu)
{
  return GET_H_ACCUM ();
}

/* Set a value for h-accum.  */

void
m32r2f_h_accum_set (SIM_CPU *current_cpu, DI newval)
{
  SET_H_ACCUM (newval);
}

/* Get the value of h-accums.  */

DI
m32r2f_h_accums_get (SIM_CPU *current_cpu, UINT regno)
{
  return GET_H_ACCUMS (regno);
}

/* Set a value for h-accums.  */

void
m32r2f_h_accums_set (SIM_CPU *current_cpu, UINT regno, DI newval)
{
  SET_H_ACCUMS (regno, newval);
}

/* Get the value of h-cond.  */

BI
m32r2f_h_cond_get (SIM_CPU *current_cpu)
{
  return CPU (h_cond);
}

/* Set a value for h-cond.  */

void
m32r2f_h_cond_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_cond) = newval;
}

/* Get the value of h-psw.  */

UQI
m32r2f_h_psw_get (SIM_CPU *current_cpu)
{
  return GET_H_PSW ();
}

/* Set a value for h-psw.  */

void
m32r2f_h_psw_set (SIM_CPU *current_cpu, UQI newval)
{
  SET_H_PSW (newval);
}

/* Get the value of h-bpsw.  */

UQI
m32r2f_h_bpsw_get (SIM_CPU *current_cpu)
{
  return CPU (h_bpsw);
}

/* Set a value for h-bpsw.  */

void
m32r2f_h_bpsw_set (SIM_CPU *current_cpu, UQI newval)
{
  CPU (h_bpsw) = newval;
}

/* Get the value of h-bbpsw.  */

UQI
m32r2f_h_bbpsw_get (SIM_CPU *current_cpu)
{
  return CPU (h_bbpsw);
}

/* Set a value for h-bbpsw.  */

void
m32r2f_h_bbpsw_set (SIM_CPU *current_cpu, UQI newval)
{
  CPU (h_bbpsw) = newval;
}

/* Get the value of h-lock.  */

BI
m32r2f_h_lock_get (SIM_CPU *current_cpu)
{
  return CPU (h_lock);
}

/* Set a value for h-lock.  */

void
m32r2f_h_lock_set (SIM_CPU *current_cpu, BI newval)
{
  CPU (h_lock) = newval;
}

/* Record trace results for INSN.  */

void
m32r2f_record_trace_results (SIM_CPU *current_cpu, CGEN_INSN *insn,
			    int *indices, TRACE_RECORD *tr)
{
}
