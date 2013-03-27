/* Simulator model support for crisv10f.

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

/* The profiling data is recorded here, but is accessed via the profiling
   mechanism.  After all, this is information for profiling.  */

#if WITH_PROFILE_MODEL_P

/* Model handlers for each insn.  */

static int
model_crisv10_nop (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movepcr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_moveq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_moveq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_moveq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movs_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movs_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movu_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movu_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movecbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcbr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movecwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcwr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movecdr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cd.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movscbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movscwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movucbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movucwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_subq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_cmp_r_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_cmp_r_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_cmp_r_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_cmp_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_cmp_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_cmp_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_cmpcbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_cmpcwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_cmpcdr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cd.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_cmpq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_andq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_cmps_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_cmps_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_cmpscbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_cmpscwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_cmpu_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_cmpu_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_cmpucbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_cmpucwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movs_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movs_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movu_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movu_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_r_sprv10 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_m_sprv10.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_spr_rv10 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_spr_rv10.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_ret_type (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_spr_rv10.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_m_sprv10 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_m_sprv10.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_c_sprv10_p5 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p5.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_c_sprv10_p9 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p9.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_c_sprv10_p10 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p9.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_c_sprv10_p11 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p9.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_c_sprv10_p12 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p9.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_c_sprv10_p13 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p9.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_c_sprv10_p7 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p9.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_c_sprv10_p14 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p9.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_c_sprv10_p15 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p9.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_spr_mv10 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_spr_mv10.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_sbfs (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movem_r_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movem_r_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv10f_model_crisv10_u_movem (current_cpu, idesc, 0, referenced, in_Rd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movem_m_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movem_m_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_Rd = -1;
    in_Rd = FLD (in_Rd);
    referenced |= 1 << 0;
    cycles += crisv10f_model_crisv10_u_movem (current_cpu, idesc, 0, referenced, in_Rd);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_movem_m_pc (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_movem_m_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_add_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_add_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_add_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_add_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_add_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_add_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addcbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcbr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addcwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcwr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addcdr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcdr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addcpc (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p9.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_stall (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 2, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_adds_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_adds_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_adds_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_adds_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addscbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcbr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addscwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcwr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addspcpc (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_stall (current_cpu, idesc, 1, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 2, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addu_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addu_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addu_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addu_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_adducbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcbr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_adducwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcwr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_sub_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_sub_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_sub_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_sub_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_sub_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_sub_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_subcbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcbr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_subcwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcwr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_subcdr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcdr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_subs_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_subs_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_subs_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_subs_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_subscbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcbr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_subscwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcwr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_subu_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_subu_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_subu_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_subu_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_subucbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcbr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_subucwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcwr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addi_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addi_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addi_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_neg_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_neg_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_neg_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_test_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_spr_mv10.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_test_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_spr_mv10.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_test_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_spr_mv10.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_r_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_r_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_r_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_muls_b (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_multiply (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_muls_w (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_multiply (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_muls_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_multiply (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_mulu_b (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_multiply (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_mulu_w (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_multiply (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_mulu_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_multiply (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_mstep (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_dstep (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_abs (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_and_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_and_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_and_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_and_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_and_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_and_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_andcbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcbr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_andcwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcwr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_andcdr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcdr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_andq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_andq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_orr_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_orr_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_orr_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_or_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_or_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_or_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_orcbr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcbr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_orcwr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcwr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_orcdr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addcdr.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_orq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_andq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_xor (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_swap (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_spr_mv10.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_asrr_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_asrr_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_asrr_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_asrq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_asrq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_lsrr_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_lsrr_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_lsrr_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_lsrq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_asrq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_lslr_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_lslr_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_lslr_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_lslq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_asrq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_btst (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_btstq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_asrq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_setf (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_setf.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_clearf (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_setf.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_bcc_b (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bcc_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_ba_b (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bcc_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_bcc_w (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bcc_w.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_ba_w (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bcc_w.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_jump_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_m_sprv10.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_jump_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_m_sprv10.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_jump_c (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p9.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_break (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_break.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_bound_r_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_bound_r_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_bound_r_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_bound_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_bound_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_bound_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_bound_cb (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_bound_cw (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_bound_cd (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cd.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_scc (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_spr_mv10.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_lz (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_muls_b.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addoq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addoq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_bdapqpc (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addoq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_bdap_32_pc (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p9.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_m_pcplus_p0 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_m_spplus_p8.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_move_m_spplus_p8 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_m_spplus_p8.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addo_m_b_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addo_m_w_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addo_m_d_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_m_b_m.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addo_cb (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cb.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addo_cw (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cw.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const16 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addo_cd (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bound_cd.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_dip_m (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_spr_mv10.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_mem (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_dip_c (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_move_c_sprv10_p9.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_const32 (current_cpu, idesc, 0, referenced);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 1, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addi_acr_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addi_acr_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_addi_acr_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add_b_r.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_biap_pc_b_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addoq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_biap_pc_w_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addoq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_crisv10_biap_pc_d_r (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addoq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += crisv10f_model_crisv10_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

/* We assume UNIT_NONE == 0 because the tables don't always terminate
   entries with it.  */

/* Model timing data for `crisv10'.  */

static const INSN_TIMING crisv10_timing[] = {
  { CRISV10F_INSN_X_INVALID, 0, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_X_AFTER, 0, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_X_BEFORE, 0, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_X_CTI_CHAIN, 0, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_X_CHAIN, 0, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_X_BEGIN, 0, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_NOP, model_crisv10_nop, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_B_R, model_crisv10_move_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_W_R, model_crisv10_move_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_D_R, model_crisv10_move_d_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVEPCR, model_crisv10_movepcr, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVEQ, model_crisv10_moveq, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVS_B_R, model_crisv10_movs_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVS_W_R, model_crisv10_movs_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVU_B_R, model_crisv10_movu_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVU_W_R, model_crisv10_movu_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVECBR, model_crisv10_movecbr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVECWR, model_crisv10_movecwr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVECDR, model_crisv10_movecdr, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVSCBR, model_crisv10_movscbr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVSCWR, model_crisv10_movscwr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVUCBR, model_crisv10_movucbr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVUCWR, model_crisv10_movucwr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDQ, model_crisv10_addq, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUBQ, model_crisv10_subq, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CMP_R_B_R, model_crisv10_cmp_r_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CMP_R_W_R, model_crisv10_cmp_r_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CMP_R_D_R, model_crisv10_cmp_r_d_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CMP_M_B_M, model_crisv10_cmp_m_b_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CMP_M_W_M, model_crisv10_cmp_m_w_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CMP_M_D_M, model_crisv10_cmp_m_d_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CMPCBR, model_crisv10_cmpcbr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CMPCWR, model_crisv10_cmpcwr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CMPCDR, model_crisv10_cmpcdr, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CMPQ, model_crisv10_cmpq, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CMPS_M_B_M, model_crisv10_cmps_m_b_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CMPS_M_W_M, model_crisv10_cmps_m_w_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CMPSCBR, model_crisv10_cmpscbr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CMPSCWR, model_crisv10_cmpscwr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CMPU_M_B_M, model_crisv10_cmpu_m_b_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CMPU_M_W_M, model_crisv10_cmpu_m_w_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CMPUCBR, model_crisv10_cmpucbr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CMPUCWR, model_crisv10_cmpucwr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_M_B_M, model_crisv10_move_m_b_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_M_W_M, model_crisv10_move_m_w_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_M_D_M, model_crisv10_move_m_d_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVS_M_B_M, model_crisv10_movs_m_b_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVS_M_W_M, model_crisv10_movs_m_w_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVU_M_B_M, model_crisv10_movu_m_b_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVU_M_W_M, model_crisv10_movu_m_w_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_R_SPRV10, model_crisv10_move_r_sprv10, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_SPR_RV10, model_crisv10_move_spr_rv10, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_RET_TYPE, model_crisv10_ret_type, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_M_SPRV10, model_crisv10_move_m_sprv10, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_C_SPRV10_P5, model_crisv10_move_c_sprv10_p5, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_C_SPRV10_P9, model_crisv10_move_c_sprv10_p9, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_C_SPRV10_P10, model_crisv10_move_c_sprv10_p10, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_C_SPRV10_P11, model_crisv10_move_c_sprv10_p11, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_C_SPRV10_P12, model_crisv10_move_c_sprv10_p12, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_C_SPRV10_P13, model_crisv10_move_c_sprv10_p13, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_C_SPRV10_P7, model_crisv10_move_c_sprv10_p7, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_C_SPRV10_P14, model_crisv10_move_c_sprv10_p14, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_C_SPRV10_P15, model_crisv10_move_c_sprv10_p15, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_SPR_MV10, model_crisv10_move_spr_mv10, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SBFS, model_crisv10_sbfs, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVEM_R_M, model_crisv10_movem_r_m, { { (int) UNIT_CRISV10_U_MOVEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVEM_M_R, model_crisv10_movem_m_r, { { (int) UNIT_CRISV10_U_MOVEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVEM_M_PC, model_crisv10_movem_m_pc, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADD_B_R, model_crisv10_add_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADD_W_R, model_crisv10_add_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADD_D_R, model_crisv10_add_d_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADD_M_B_M, model_crisv10_add_m_b_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADD_M_W_M, model_crisv10_add_m_w_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADD_M_D_M, model_crisv10_add_m_d_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDCBR, model_crisv10_addcbr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDCWR, model_crisv10_addcwr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDCDR, model_crisv10_addcdr, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDCPC, model_crisv10_addcpc, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_STALL, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDS_B_R, model_crisv10_adds_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDS_W_R, model_crisv10_adds_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDS_M_B_M, model_crisv10_adds_m_b_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDS_M_W_M, model_crisv10_adds_m_w_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDSCBR, model_crisv10_addscbr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDSCWR, model_crisv10_addscwr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDSPCPC, model_crisv10_addspcpc, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_STALL, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDU_B_R, model_crisv10_addu_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDU_W_R, model_crisv10_addu_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDU_M_B_M, model_crisv10_addu_m_b_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDU_M_W_M, model_crisv10_addu_m_w_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDUCBR, model_crisv10_adducbr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDUCWR, model_crisv10_adducwr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUB_B_R, model_crisv10_sub_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUB_W_R, model_crisv10_sub_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUB_D_R, model_crisv10_sub_d_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUB_M_B_M, model_crisv10_sub_m_b_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUB_M_W_M, model_crisv10_sub_m_w_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUB_M_D_M, model_crisv10_sub_m_d_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUBCBR, model_crisv10_subcbr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUBCWR, model_crisv10_subcwr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUBCDR, model_crisv10_subcdr, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUBS_B_R, model_crisv10_subs_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUBS_W_R, model_crisv10_subs_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUBS_M_B_M, model_crisv10_subs_m_b_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUBS_M_W_M, model_crisv10_subs_m_w_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUBSCBR, model_crisv10_subscbr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUBSCWR, model_crisv10_subscwr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUBU_B_R, model_crisv10_subu_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUBU_W_R, model_crisv10_subu_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUBU_M_B_M, model_crisv10_subu_m_b_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUBU_M_W_M, model_crisv10_subu_m_w_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUBUCBR, model_crisv10_subucbr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SUBUCWR, model_crisv10_subucwr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDI_B_R, model_crisv10_addi_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDI_W_R, model_crisv10_addi_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDI_D_R, model_crisv10_addi_d_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_NEG_B_R, model_crisv10_neg_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_NEG_W_R, model_crisv10_neg_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_NEG_D_R, model_crisv10_neg_d_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_TEST_M_B_M, model_crisv10_test_m_b_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_TEST_M_W_M, model_crisv10_test_m_w_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_TEST_M_D_M, model_crisv10_test_m_d_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_R_M_B_M, model_crisv10_move_r_m_b_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_R_M_W_M, model_crisv10_move_r_m_w_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_R_M_D_M, model_crisv10_move_r_m_d_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MULS_B, model_crisv10_muls_b, { { (int) UNIT_CRISV10_U_MULTIPLY, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MULS_W, model_crisv10_muls_w, { { (int) UNIT_CRISV10_U_MULTIPLY, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MULS_D, model_crisv10_muls_d, { { (int) UNIT_CRISV10_U_MULTIPLY, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MULU_B, model_crisv10_mulu_b, { { (int) UNIT_CRISV10_U_MULTIPLY, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MULU_W, model_crisv10_mulu_w, { { (int) UNIT_CRISV10_U_MULTIPLY, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MULU_D, model_crisv10_mulu_d, { { (int) UNIT_CRISV10_U_MULTIPLY, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MSTEP, model_crisv10_mstep, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_DSTEP, model_crisv10_dstep, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ABS, model_crisv10_abs, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_AND_B_R, model_crisv10_and_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_AND_W_R, model_crisv10_and_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_AND_D_R, model_crisv10_and_d_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_AND_M_B_M, model_crisv10_and_m_b_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_AND_M_W_M, model_crisv10_and_m_w_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_AND_M_D_M, model_crisv10_and_m_d_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ANDCBR, model_crisv10_andcbr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ANDCWR, model_crisv10_andcwr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ANDCDR, model_crisv10_andcdr, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ANDQ, model_crisv10_andq, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ORR_B_R, model_crisv10_orr_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ORR_W_R, model_crisv10_orr_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ORR_D_R, model_crisv10_orr_d_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_OR_M_B_M, model_crisv10_or_m_b_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_OR_M_W_M, model_crisv10_or_m_w_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_OR_M_D_M, model_crisv10_or_m_d_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ORCBR, model_crisv10_orcbr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ORCWR, model_crisv10_orcwr, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ORCDR, model_crisv10_orcdr, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ORQ, model_crisv10_orq, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_XOR, model_crisv10_xor, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SWAP, model_crisv10_swap, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ASRR_B_R, model_crisv10_asrr_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ASRR_W_R, model_crisv10_asrr_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ASRR_D_R, model_crisv10_asrr_d_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ASRQ, model_crisv10_asrq, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_LSRR_B_R, model_crisv10_lsrr_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_LSRR_W_R, model_crisv10_lsrr_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_LSRR_D_R, model_crisv10_lsrr_d_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_LSRQ, model_crisv10_lsrq, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_LSLR_B_R, model_crisv10_lslr_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_LSLR_W_R, model_crisv10_lslr_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_LSLR_D_R, model_crisv10_lslr_d_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_LSLQ, model_crisv10_lslq, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BTST, model_crisv10_btst, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BTSTQ, model_crisv10_btstq, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SETF, model_crisv10_setf, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_CLEARF, model_crisv10_clearf, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BCC_B, model_crisv10_bcc_b, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BA_B, model_crisv10_ba_b, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BCC_W, model_crisv10_bcc_w, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BA_W, model_crisv10_ba_w, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_JUMP_R, model_crisv10_jump_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_JUMP_M, model_crisv10_jump_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_JUMP_C, model_crisv10_jump_c, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BREAK, model_crisv10_break, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BOUND_R_B_R, model_crisv10_bound_r_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BOUND_R_W_R, model_crisv10_bound_r_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BOUND_R_D_R, model_crisv10_bound_r_d_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BOUND_M_B_M, model_crisv10_bound_m_b_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BOUND_M_W_M, model_crisv10_bound_m_w_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BOUND_M_D_M, model_crisv10_bound_m_d_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BOUND_CB, model_crisv10_bound_cb, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BOUND_CW, model_crisv10_bound_cw, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BOUND_CD, model_crisv10_bound_cd, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_SCC, model_crisv10_scc, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_LZ, model_crisv10_lz, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDOQ, model_crisv10_addoq, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BDAPQPC, model_crisv10_bdapqpc, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BDAP_32_PC, model_crisv10_bdap_32_pc, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_M_PCPLUS_P0, model_crisv10_move_m_pcplus_p0, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_MOVE_M_SPPLUS_P8, model_crisv10_move_m_spplus_p8, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDO_M_B_M, model_crisv10_addo_m_b_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDO_M_W_M, model_crisv10_addo_m_w_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDO_M_D_M, model_crisv10_addo_m_d_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDO_CB, model_crisv10_addo_cb, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDO_CW, model_crisv10_addo_cw, { { (int) UNIT_CRISV10_U_CONST16, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDO_CD, model_crisv10_addo_cd, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_DIP_M, model_crisv10_dip_m, { { (int) UNIT_CRISV10_U_MEM, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_DIP_C, model_crisv10_dip_c, { { (int) UNIT_CRISV10_U_CONST32, 1, 1 }, { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDI_ACR_B_R, model_crisv10_addi_acr_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDI_ACR_W_R, model_crisv10_addi_acr_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_ADDI_ACR_D_R, model_crisv10_addi_acr_d_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BIAP_PC_B_R, model_crisv10_biap_pc_b_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BIAP_PC_W_R, model_crisv10_biap_pc_w_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
  { CRISV10F_INSN_BIAP_PC_D_R, model_crisv10_biap_pc_d_r, { { (int) UNIT_CRISV10_U_EXEC, 1, 1 } } },
};

#endif /* WITH_PROFILE_MODEL_P */

static void
crisv10_model_init (SIM_CPU *cpu)
{
  CPU_MODEL_DATA (cpu) = (void *) zalloc (sizeof (MODEL_CRISV10_DATA));
}

#if WITH_PROFILE_MODEL_P
#define TIMING_DATA(td) td
#else
#define TIMING_DATA(td) 0
#endif

static const MODEL crisv10_models[] =
{
  { "crisv10", & crisv10_mach, MODEL_CRISV10, TIMING_DATA (& crisv10_timing[0]), crisv10_model_init },
  { 0 }
};

/* The properties of this cpu's implementation.  */

static const MACH_IMP_PROPERTIES crisv10f_imp_properties =
{
  sizeof (SIM_CPU),
#if WITH_SCACHE
  sizeof (SCACHE)
#else
  0
#endif
};


static void
crisv10f_prepare_run (SIM_CPU *cpu)
{
  if (CPU_IDESC (cpu) == NULL)
    crisv10f_init_idesc_table (cpu);
}

static const CGEN_INSN *
crisv10f_get_idata (SIM_CPU *cpu, int inum)
{
  return CPU_IDESC (cpu) [inum].idata;
}

static void
crisv10_init_cpu (SIM_CPU *cpu)
{
  CPU_REG_FETCH (cpu) = crisv10f_fetch_register;
  CPU_REG_STORE (cpu) = crisv10f_store_register;
  CPU_PC_FETCH (cpu) = crisv10f_h_pc_get;
  CPU_PC_STORE (cpu) = crisv10f_h_pc_set;
  CPU_GET_IDATA (cpu) = crisv10f_get_idata;
  CPU_MAX_INSNS (cpu) = CRISV10F_INSN__MAX;
  CPU_INSN_NAME (cpu) = cgen_insn_name;
  CPU_FULL_ENGINE_FN (cpu) = crisv10f_engine_run_full;
#if WITH_FAST
  CPU_FAST_ENGINE_FN (cpu) = crisv10f_engine_run_fast;
#else
  CPU_FAST_ENGINE_FN (cpu) = crisv10f_engine_run_full;
#endif
}

const MACH crisv10_mach =
{
  "crisv10", "cris", MACH_CRISV10,
  32, 32, & crisv10_models[0], & crisv10f_imp_properties,
  crisv10_init_cpu,
  crisv10f_prepare_run
};

