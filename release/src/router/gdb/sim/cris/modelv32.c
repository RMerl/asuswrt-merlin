/* Simulator model support for crisv32f.

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

/* The profiling data is recorded here, but is accessed via the profiling
   mechanism.  After all, this is information for profiling.  */

#if WITH_PROFILE_MODEL_P

/* Model handlers for each insn.  */

static int
model_crisv32_move_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_moveq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_moveq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_movs_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_movs_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_movu_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_movu_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_movecbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcbr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_movecwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcwr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_movecdr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cd.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_movscbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_movscwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_movucbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_movucwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_subq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_cmp_r_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_cmp_r_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_cmp_r_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_cmp_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_cmp_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_cmp_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_cmpcbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_cmpcwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_cmpcdr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cd.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_cmpq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_andq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_cmps_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_cmps_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_cmpscbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_cmpscwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_cmpu_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_cmpu_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_cmpucbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_cmpucwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_movs_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movs_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 1;
    if (insn_referenced & (1 << 7)) referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_movs_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movs_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 1;
    if (insn_referenced & (1 << 7)) referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_movu_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movs_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 1;
    if (insn_referenced & (1 << 7)) referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_movu_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movs_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 1;
    if (insn_referenced & (1 << 7)) referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_r_sprv32 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_m_sprv32.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT out_Pd = -1;
    in_Rs = FLD (in_Rs);
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 2)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec_to_sr (current_cpu, idesc, 0, referenced, in_Rs, out_Pd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_spr_rv32 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mcp.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_m_sprv32 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_m_sprv32.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT out_Pd = -1;
    in_Rs = FLD (in_Rs);
    out_Pd = FLD (out_Pd);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec_to_sr (current_cpu, idesc, 2, referenced, in_Rs, out_Pd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_c_sprv32_p2 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT out_Pd = -1;
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec_to_sr (current_cpu, idesc, 1, referenced, in_Rs, out_Pd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_c_sprv32_p3 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT out_Pd = -1;
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec_to_sr (current_cpu, idesc, 1, referenced, in_Rs, out_Pd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_c_sprv32_p5 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT out_Pd = -1;
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec_to_sr (current_cpu, idesc, 1, referenced, in_Rs, out_Pd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_c_sprv32_p6 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT out_Pd = -1;
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec_to_sr (current_cpu, idesc, 1, referenced, in_Rs, out_Pd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_c_sprv32_p7 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT out_Pd = -1;
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec_to_sr (current_cpu, idesc, 1, referenced, in_Rs, out_Pd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_c_sprv32_p9 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT out_Pd = -1;
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec_to_sr (current_cpu, idesc, 1, referenced, in_Rs, out_Pd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_c_sprv32_p10 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT out_Pd = -1;
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec_to_sr (current_cpu, idesc, 1, referenced, in_Rs, out_Pd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_c_sprv32_p11 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT out_Pd = -1;
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec_to_sr (current_cpu, idesc, 1, referenced, in_Rs, out_Pd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_c_sprv32_p12 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT out_Pd = -1;
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec_to_sr (current_cpu, idesc, 1, referenced, in_Rs, out_Pd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_c_sprv32_p13 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT out_Pd = -1;
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec_to_sr (current_cpu, idesc, 1, referenced, in_Rs, out_Pd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_c_sprv32_p14 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT out_Pd = -1;
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec_to_sr (current_cpu, idesc, 1, referenced, in_Rs, out_Pd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_c_sprv32_p15 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT out_Pd = -1;
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec_to_sr (current_cpu, idesc, 1, referenced, in_Rs, out_Pd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_spr_mv32 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_spr_mv32.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_w (current_cpu, idesc, 2, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_ss_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_spr_mv32.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_r_ss (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mcp.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_movem_r_m_v32 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movem_r_m_v32.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT in_Rd = -1;
    in_Rs = FLD (in_Rs);
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_movem_rtom (current_cpu, idesc, 1, referenced, in_Rs, in_Rd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec_movem (current_cpu, idesc, 2, referenced, in_Rs, out_Rd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_w (current_cpu, idesc, 3, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_movem_m_r_v32 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movem_m_r_v32.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT in_Rd = -1;
    in_Rs = FLD (in_Rs);
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_movem_mtor (current_cpu, idesc, 2, referenced, in_Rs, in_Rd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec_movem (current_cpu, idesc, 3, referenced, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_add_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_add_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_add_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_add_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_add_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_add_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addcbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcbr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addcwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcwr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addcdr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcdr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_adds_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_adds_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_adds_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_adds_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addscbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcbr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addscwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcwr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addu_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addu_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addu_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addu_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_adducbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcbr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_adducwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcwr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_sub_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_sub_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_sub_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_sub_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_sub_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_sub_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_subcbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcbr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_subcwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcwr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_subcdr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcdr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_subs_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_subs_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_subs_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_subs_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_subscbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcbr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_subscwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcwr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_subu_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_subu_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_subu_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_subu_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_subucbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcbr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_subucwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcwr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addc_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addc_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addc_c (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcdr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_lapc_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_lapc_d.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_lapcq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_lapcq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addi_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addi_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addi_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_neg_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_neg_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_neg_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_test_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_spr_mv32.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_test_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_spr_mv32.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_test_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_spr_mv32.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_r_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_w (current_cpu, idesc, 2, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_r_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_w (current_cpu, idesc, 2, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_move_r_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_w (current_cpu, idesc, 2, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_muls_b (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT in_Rd = -1;
    in_Rs = FLD (in_Rs);
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_multiply (current_cpu, idesc, 0, referenced, in_Rs, in_Rd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_muls_w (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT in_Rd = -1;
    in_Rs = FLD (in_Rs);
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_multiply (current_cpu, idesc, 0, referenced, in_Rs, in_Rd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_muls_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT in_Rd = -1;
    in_Rs = FLD (in_Rs);
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_multiply (current_cpu, idesc, 0, referenced, in_Rs, in_Rd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_mulu_b (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT in_Rd = -1;
    in_Rs = FLD (in_Rs);
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_multiply (current_cpu, idesc, 0, referenced, in_Rs, in_Rd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_mulu_w (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT in_Rd = -1;
    in_Rs = FLD (in_Rs);
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_multiply (current_cpu, idesc, 0, referenced, in_Rs, in_Rd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_mulu_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    INT in_Rd = -1;
    in_Rs = FLD (in_Rs);
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_multiply (current_cpu, idesc, 0, referenced, in_Rs, in_Rd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_mcp (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mcp.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_dstep (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_abs (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_and_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_and_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_and_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_and_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_and_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_and_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_andcbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcbr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_andcwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcwr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_andcdr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcdr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_andq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_andq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_orr_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_orr_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_orr_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_or_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_or_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_or_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_orcbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcbr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_orcwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcwr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_orcdr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcdr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_orq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_andq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_xor (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_swap (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_spr_mv32.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_asrr_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_asrr_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_asrr_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_asrq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_asrq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_lsrr_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_lsrr_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_lsrr_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_lsrq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_asrq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_lslr_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_lslr_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_lslr_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 0)) referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_lslq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_asrq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_btst (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_btstq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_asrq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_setf (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_setf.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_clearf (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_setf.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_rfe (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_rfe.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_sfe (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_rfe.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_rfg (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_rfn (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_rfe.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_halt (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_bcc_b (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bcc_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_branch (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_ba_b (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bcc_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT out_Pd = -1;
    cycles += crisv32f_model_crisv32_u_jump (current_cpu, idesc, 0, referenced, out_Pd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_bcc_w (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bcc_w.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_branch (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_ba_w (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bcc_w.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT out_Pd = -1;
    cycles += crisv32f_model_crisv32_u_jump (current_cpu, idesc, 1, referenced, out_Pd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_jas_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_m_sprv32.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_jump_r (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT out_Pd = -1;
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_jump (current_cpu, idesc, 1, referenced, out_Pd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_jas_c (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT out_Pd = -1;
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_jump (current_cpu, idesc, 1, referenced, out_Pd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_jump_p (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mcp.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Ps = -1;
    in_Ps = FLD (in_Ps);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_jump_sr (current_cpu, idesc, 0, referenced, in_Ps);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_bas_c (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bas_c.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT out_Pd = -1;
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_jump (current_cpu, idesc, 1, referenced, out_Pd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_jasc_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_m_sprv32.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_jump_r (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_skip4 (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT out_Pd = -1;
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_jump (current_cpu, idesc, 2, referenced, out_Pd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 3, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_jasc_c (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv32_p2.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_skip4 (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT out_Pd = -1;
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_jump (current_cpu, idesc, 2, referenced, out_Pd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 3, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_basc_c (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bas_c.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_skip4 (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT out_Pd = -1;
    out_Pd = FLD (out_Pd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_jump (current_cpu, idesc, 2, referenced, out_Pd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 3, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_break (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_break.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_bound_r_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_bound_r_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_bound_r_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_bound_cb (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_bound_cw (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_bound_cd (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cd.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_scc (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_spr_mv32.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_lz (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    out_Rd = FLD (out_Rd);
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addoq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addoq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addo_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addo_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addo_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addc_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rs = -1;
    in_Rs = FLD (in_Rs);
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_mem (current_cpu, idesc, 0, referenced, in_Rs);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_mem_r (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 1)) referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 2, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addo_cb (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addo_cw (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addo_cd (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cd.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv32f_model_crisv32_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 1, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addi_acr_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addi_acr_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_addi_acr_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rd = FLD (in_Rd);
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_fidxi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mcp.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_ftagi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mcp.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_fidxd (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mcp.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

static int
model_crisv32_ftagd (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mcp.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    INT in_Rs = -1;
    INT out_Rd = -1;
    in_Rs = FLD (in_Rs);
    referenced |= 1 << 1;
    cycles += crisv32f_model_crisv32_u_exec (current_cpu, idesc, 0, referenced, in_Rd, in_Rs, out_Rd);
  }
  return cycles;
#undef FLD
}

/* We assume UNIT_NONE == 0 because the tables don't always terminate
   entries with it.  */

/* Model timing data for `crisv32'.  */

static const INSN_TIMING crisv32_timing[] = {
  { CRISV32F_INSN_X_INVALID, 0, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_X_AFTER, 0, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_X_BEFORE, 0, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_X_CTI_CHAIN, 0, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_X_CHAIN, 0, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_X_BEGIN, 0, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVE_B_R, model_crisv32_move_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVE_W_R, model_crisv32_move_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVE_D_R, model_crisv32_move_d_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVEQ, model_crisv32_moveq, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVS_B_R, model_crisv32_movs_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVS_W_R, model_crisv32_movs_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVU_B_R, model_crisv32_movu_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVU_W_R, model_crisv32_movu_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVECBR, model_crisv32_movecbr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVECWR, model_crisv32_movecwr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVECDR, model_crisv32_movecdr, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVSCBR, model_crisv32_movscbr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVSCWR, model_crisv32_movscwr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVUCBR, model_crisv32_movucbr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVUCWR, model_crisv32_movucwr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDQ, model_crisv32_addq, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUBQ, model_crisv32_subq, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CMP_R_B_R, model_crisv32_cmp_r_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CMP_R_W_R, model_crisv32_cmp_r_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CMP_R_D_R, model_crisv32_cmp_r_d_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CMP_M_B_M, model_crisv32_cmp_m_b_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CMP_M_W_M, model_crisv32_cmp_m_w_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CMP_M_D_M, model_crisv32_cmp_m_d_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CMPCBR, model_crisv32_cmpcbr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CMPCWR, model_crisv32_cmpcwr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CMPCDR, model_crisv32_cmpcdr, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CMPQ, model_crisv32_cmpq, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CMPS_M_B_M, model_crisv32_cmps_m_b_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CMPS_M_W_M, model_crisv32_cmps_m_w_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CMPSCBR, model_crisv32_cmpscbr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CMPSCWR, model_crisv32_cmpscwr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CMPU_M_B_M, model_crisv32_cmpu_m_b_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CMPU_M_W_M, model_crisv32_cmpu_m_w_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CMPUCBR, model_crisv32_cmpucbr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CMPUCWR, model_crisv32_cmpucwr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVE_M_B_M, model_crisv32_move_m_b_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVE_M_W_M, model_crisv32_move_m_w_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVE_M_D_M, model_crisv32_move_m_d_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVS_M_B_M, model_crisv32_movs_m_b_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVS_M_W_M, model_crisv32_movs_m_w_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVU_M_B_M, model_crisv32_movu_m_b_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVU_M_W_M, model_crisv32_movu_m_w_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVE_R_SPRV32, model_crisv32_move_r_sprv32, { { (int) UNIT_CRISV32_U_EXEC_TO_SR, 1, 1 } } },
  { CRISV32F_INSN_MOVE_SPR_RV32, model_crisv32_move_spr_rv32, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVE_M_SPRV32, model_crisv32_move_m_sprv32, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC_TO_SR, 1, 1 } } },
  { CRISV32F_INSN_MOVE_C_SPRV32_P2, model_crisv32_move_c_sprv32_p2, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC_TO_SR, 1, 1 } } },
  { CRISV32F_INSN_MOVE_C_SPRV32_P3, model_crisv32_move_c_sprv32_p3, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC_TO_SR, 1, 1 } } },
  { CRISV32F_INSN_MOVE_C_SPRV32_P5, model_crisv32_move_c_sprv32_p5, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC_TO_SR, 1, 1 } } },
  { CRISV32F_INSN_MOVE_C_SPRV32_P6, model_crisv32_move_c_sprv32_p6, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC_TO_SR, 1, 1 } } },
  { CRISV32F_INSN_MOVE_C_SPRV32_P7, model_crisv32_move_c_sprv32_p7, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC_TO_SR, 1, 1 } } },
  { CRISV32F_INSN_MOVE_C_SPRV32_P9, model_crisv32_move_c_sprv32_p9, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC_TO_SR, 1, 1 } } },
  { CRISV32F_INSN_MOVE_C_SPRV32_P10, model_crisv32_move_c_sprv32_p10, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC_TO_SR, 1, 1 } } },
  { CRISV32F_INSN_MOVE_C_SPRV32_P11, model_crisv32_move_c_sprv32_p11, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC_TO_SR, 1, 1 } } },
  { CRISV32F_INSN_MOVE_C_SPRV32_P12, model_crisv32_move_c_sprv32_p12, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC_TO_SR, 1, 1 } } },
  { CRISV32F_INSN_MOVE_C_SPRV32_P13, model_crisv32_move_c_sprv32_p13, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC_TO_SR, 1, 1 } } },
  { CRISV32F_INSN_MOVE_C_SPRV32_P14, model_crisv32_move_c_sprv32_p14, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC_TO_SR, 1, 1 } } },
  { CRISV32F_INSN_MOVE_C_SPRV32_P15, model_crisv32_move_c_sprv32_p15, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC_TO_SR, 1, 1 } } },
  { CRISV32F_INSN_MOVE_SPR_MV32, model_crisv32_move_spr_mv32, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_W, 1, 1 } } },
  { CRISV32F_INSN_MOVE_SS_R, model_crisv32_move_ss_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVE_R_SS, model_crisv32_move_r_ss, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVEM_R_M_V32, model_crisv32_movem_r_m_v32, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MOVEM_RTOM, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC_MOVEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_W, 1, 1 } } },
  { CRISV32F_INSN_MOVEM_M_R_V32, model_crisv32_movem_m_r_v32, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_MOVEM_MTOR, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC_MOVEM, 1, 1 } } },
  { CRISV32F_INSN_ADD_B_R, model_crisv32_add_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADD_W_R, model_crisv32_add_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADD_D_R, model_crisv32_add_d_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADD_M_B_M, model_crisv32_add_m_b_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADD_M_W_M, model_crisv32_add_m_w_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADD_M_D_M, model_crisv32_add_m_d_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDCBR, model_crisv32_addcbr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDCWR, model_crisv32_addcwr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDCDR, model_crisv32_addcdr, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDS_B_R, model_crisv32_adds_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDS_W_R, model_crisv32_adds_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDS_M_B_M, model_crisv32_adds_m_b_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDS_M_W_M, model_crisv32_adds_m_w_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDSCBR, model_crisv32_addscbr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDSCWR, model_crisv32_addscwr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDU_B_R, model_crisv32_addu_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDU_W_R, model_crisv32_addu_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDU_M_B_M, model_crisv32_addu_m_b_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDU_M_W_M, model_crisv32_addu_m_w_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDUCBR, model_crisv32_adducbr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDUCWR, model_crisv32_adducwr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUB_B_R, model_crisv32_sub_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUB_W_R, model_crisv32_sub_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUB_D_R, model_crisv32_sub_d_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUB_M_B_M, model_crisv32_sub_m_b_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUB_M_W_M, model_crisv32_sub_m_w_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUB_M_D_M, model_crisv32_sub_m_d_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUBCBR, model_crisv32_subcbr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUBCWR, model_crisv32_subcwr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUBCDR, model_crisv32_subcdr, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUBS_B_R, model_crisv32_subs_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUBS_W_R, model_crisv32_subs_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUBS_M_B_M, model_crisv32_subs_m_b_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUBS_M_W_M, model_crisv32_subs_m_w_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUBSCBR, model_crisv32_subscbr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUBSCWR, model_crisv32_subscwr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUBU_B_R, model_crisv32_subu_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUBU_W_R, model_crisv32_subu_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUBU_M_B_M, model_crisv32_subu_m_b_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUBU_M_W_M, model_crisv32_subu_m_w_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUBUCBR, model_crisv32_subucbr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SUBUCWR, model_crisv32_subucwr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDC_R, model_crisv32_addc_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDC_M, model_crisv32_addc_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDC_C, model_crisv32_addc_c, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_LAPC_D, model_crisv32_lapc_d, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_LAPCQ, model_crisv32_lapcq, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDI_B_R, model_crisv32_addi_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDI_W_R, model_crisv32_addi_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDI_D_R, model_crisv32_addi_d_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_NEG_B_R, model_crisv32_neg_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_NEG_W_R, model_crisv32_neg_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_NEG_D_R, model_crisv32_neg_d_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_TEST_M_B_M, model_crisv32_test_m_b_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_TEST_M_W_M, model_crisv32_test_m_w_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_TEST_M_D_M, model_crisv32_test_m_d_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MOVE_R_M_B_M, model_crisv32_move_r_m_b_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_W, 1, 1 } } },
  { CRISV32F_INSN_MOVE_R_M_W_M, model_crisv32_move_r_m_w_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_W, 1, 1 } } },
  { CRISV32F_INSN_MOVE_R_M_D_M, model_crisv32_move_r_m_d_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_W, 1, 1 } } },
  { CRISV32F_INSN_MULS_B, model_crisv32_muls_b, { { (int) UNIT_CRISV32_U_MULTIPLY, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MULS_W, model_crisv32_muls_w, { { (int) UNIT_CRISV32_U_MULTIPLY, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MULS_D, model_crisv32_muls_d, { { (int) UNIT_CRISV32_U_MULTIPLY, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MULU_B, model_crisv32_mulu_b, { { (int) UNIT_CRISV32_U_MULTIPLY, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MULU_W, model_crisv32_mulu_w, { { (int) UNIT_CRISV32_U_MULTIPLY, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MULU_D, model_crisv32_mulu_d, { { (int) UNIT_CRISV32_U_MULTIPLY, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_MCP, model_crisv32_mcp, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_DSTEP, model_crisv32_dstep, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ABS, model_crisv32_abs, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_AND_B_R, model_crisv32_and_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_AND_W_R, model_crisv32_and_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_AND_D_R, model_crisv32_and_d_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_AND_M_B_M, model_crisv32_and_m_b_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_AND_M_W_M, model_crisv32_and_m_w_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_AND_M_D_M, model_crisv32_and_m_d_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ANDCBR, model_crisv32_andcbr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ANDCWR, model_crisv32_andcwr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ANDCDR, model_crisv32_andcdr, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ANDQ, model_crisv32_andq, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ORR_B_R, model_crisv32_orr_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ORR_W_R, model_crisv32_orr_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ORR_D_R, model_crisv32_orr_d_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_OR_M_B_M, model_crisv32_or_m_b_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_OR_M_W_M, model_crisv32_or_m_w_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_OR_M_D_M, model_crisv32_or_m_d_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ORCBR, model_crisv32_orcbr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ORCWR, model_crisv32_orcwr, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ORCDR, model_crisv32_orcdr, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ORQ, model_crisv32_orq, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_XOR, model_crisv32_xor, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SWAP, model_crisv32_swap, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ASRR_B_R, model_crisv32_asrr_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ASRR_W_R, model_crisv32_asrr_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ASRR_D_R, model_crisv32_asrr_d_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ASRQ, model_crisv32_asrq, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_LSRR_B_R, model_crisv32_lsrr_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_LSRR_W_R, model_crisv32_lsrr_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_LSRR_D_R, model_crisv32_lsrr_d_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_LSRQ, model_crisv32_lsrq, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_LSLR_B_R, model_crisv32_lslr_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_LSLR_W_R, model_crisv32_lslr_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_LSLR_D_R, model_crisv32_lslr_d_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_LSLQ, model_crisv32_lslq, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_BTST, model_crisv32_btst, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_BTSTQ, model_crisv32_btstq, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SETF, model_crisv32_setf, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_CLEARF, model_crisv32_clearf, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_RFE, model_crisv32_rfe, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SFE, model_crisv32_sfe, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_RFG, model_crisv32_rfg, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_RFN, model_crisv32_rfn, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_HALT, model_crisv32_halt, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_BCC_B, model_crisv32_bcc_b, { { (int) UNIT_CRISV32_U_BRANCH, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_BA_B, model_crisv32_ba_b, { { (int) UNIT_CRISV32_U_JUMP, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_BCC_W, model_crisv32_bcc_w, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_BRANCH, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_BA_W, model_crisv32_ba_w, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_JUMP, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_JAS_R, model_crisv32_jas_r, { { (int) UNIT_CRISV32_U_JUMP_R, 1, 1 }, { (int) UNIT_CRISV32_U_JUMP, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_JAS_C, model_crisv32_jas_c, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_JUMP, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_JUMP_P, model_crisv32_jump_p, { { (int) UNIT_CRISV32_U_JUMP_SR, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_BAS_C, model_crisv32_bas_c, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_JUMP, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_JASC_R, model_crisv32_jasc_r, { { (int) UNIT_CRISV32_U_JUMP_R, 1, 1 }, { (int) UNIT_CRISV32_U_SKIP4, 1, 1 }, { (int) UNIT_CRISV32_U_JUMP, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_JASC_C, model_crisv32_jasc_c, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_SKIP4, 1, 1 }, { (int) UNIT_CRISV32_U_JUMP, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_BASC_C, model_crisv32_basc_c, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_SKIP4, 1, 1 }, { (int) UNIT_CRISV32_U_JUMP, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_BREAK, model_crisv32_break, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_BOUND_R_B_R, model_crisv32_bound_r_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_BOUND_R_W_R, model_crisv32_bound_r_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_BOUND_R_D_R, model_crisv32_bound_r_d_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_BOUND_CB, model_crisv32_bound_cb, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_BOUND_CW, model_crisv32_bound_cw, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_BOUND_CD, model_crisv32_bound_cd, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_SCC, model_crisv32_scc, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_LZ, model_crisv32_lz, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDOQ, model_crisv32_addoq, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDO_M_B_M, model_crisv32_addo_m_b_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDO_M_W_M, model_crisv32_addo_m_w_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDO_M_D_M, model_crisv32_addo_m_d_m, { { (int) UNIT_CRISV32_U_MEM, 1, 1 }, { (int) UNIT_CRISV32_U_MEM_R, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDO_CB, model_crisv32_addo_cb, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDO_CW, model_crisv32_addo_cw, { { (int) UNIT_CRISV32_U_CONST16, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDO_CD, model_crisv32_addo_cd, { { (int) UNIT_CRISV32_U_CONST32, 1, 1 }, { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDI_ACR_B_R, model_crisv32_addi_acr_b_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDI_ACR_W_R, model_crisv32_addi_acr_w_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_ADDI_ACR_D_R, model_crisv32_addi_acr_d_r, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_FIDXI, model_crisv32_fidxi, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_FTAGI, model_crisv32_ftagi, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_FIDXD, model_crisv32_fidxd, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
  { CRISV32F_INSN_FTAGD, model_crisv32_ftagd, { { (int) UNIT_CRISV32_U_EXEC, 1, 1 } } },
};

#endif /* WITH_PROFILE_MODEL_P */

static void
crisv32_model_init (SIM_CPU *cpu)
{
  CPU_MODEL_DATA (cpu) = (void *) zalloc (sizeof (MODEL_CRISV32_DATA));
}

#if WITH_PROFILE_MODEL_P
#define TIMING_DATA(td) td
#else
#define TIMING_DATA(td) 0
#endif

static const MODEL crisv32_models[] =
{
  { "crisv32", & crisv32_mach, MODEL_CRISV32, TIMING_DATA (& crisv32_timing[0]), crisv32_model_init },
  { 0 }
};

/* The properties of this cpu's implementation.  */

static const MACH_IMP_PROPERTIES crisv32f_imp_properties =
{
  sizeof (SIM_CPU),
#if WITH_SCACHE
  sizeof (SCACHE)
#else
  0
#endif
};


static void
crisv32f_prepare_run (SIM_CPU *cpu)
{
  if (CPU_IDESC (cpu) == NULL)
    crisv32f_init_idesc_table (cpu);
}

static const CGEN_INSN *
crisv32f_get_idata (SIM_CPU *cpu, int inum)
{
  return CPU_IDESC (cpu) [inum].idata;
}

static void
crisv32_init_cpu (SIM_CPU *cpu)
{
  CPU_REG_FETCH (cpu) = crisv32f_fetch_register;
  CPU_REG_STORE (cpu) = crisv32f_store_register;
  CPU_PC_FETCH (cpu) = crisv32f_h_pc_get;
  CPU_PC_STORE (cpu) = crisv32f_h_pc_set;
  CPU_GET_IDATA (cpu) = crisv32f_get_idata;
  CPU_MAX_INSNS (cpu) = CRISV32F_INSN__MAX;
  CPU_INSN_NAME (cpu) = cgen_insn_name;
  CPU_FULL_ENGINE_FN (cpu) = crisv32f_engine_run_full;
#if WITH_FAST
  CPU_FAST_ENGINE_FN (cpu) = crisv32f_engine_run_fast;
#else
  CPU_FAST_ENGINE_FN (cpu) = crisv32f_engine_run_full;
#endif
}

const MACH crisv32_mach =
{
  "crisv32", "crisv32", MACH_CRISV32,
  32, 32, & crisv32_models[0], & crisv32f_imp_properties,
  crisv32_init_cpu,
  crisv32f_prepare_run
};

