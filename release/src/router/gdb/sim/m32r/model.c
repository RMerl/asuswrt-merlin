/* Simulator model support for m32rbf.

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

#define WANT_CPU m32rbf
#define WANT_CPU_M32RBF

#include "sim-main.h"

/* The profiling data is recorded here, but is accessed via the profiling
   mechanism.  After all, this is information for profiling.  */

#if WITH_PROFILE_MODEL_P

/* Model handlers for each insn.  */

static int
model_m32r_d_add (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_add3 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_and (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_and3 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_and3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_or (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_or3 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_and3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_xor (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_xor3 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_and3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_addi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_addv (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_addv3 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_addx (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_bc8 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl8.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    if (insn_referenced & (1 << 2)) referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_bc24 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl24.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    if (insn_referenced & (1 << 2)) referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_beq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    if (insn_referenced & (1 << 3)) referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cmp (current_cpu, idesc, 1, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_beqz (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    if (insn_referenced & (1 << 2)) referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src2 = FLD (in_src2);
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cmp (current_cpu, idesc, 1, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_bgez (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    if (insn_referenced & (1 << 2)) referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src2 = FLD (in_src2);
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cmp (current_cpu, idesc, 1, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_bgtz (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    if (insn_referenced & (1 << 2)) referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src2 = FLD (in_src2);
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cmp (current_cpu, idesc, 1, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_blez (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    if (insn_referenced & (1 << 2)) referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src2 = FLD (in_src2);
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cmp (current_cpu, idesc, 1, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_bltz (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    if (insn_referenced & (1 << 2)) referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src2 = FLD (in_src2);
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cmp (current_cpu, idesc, 1, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_bnez (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    if (insn_referenced & (1 << 2)) referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src2 = FLD (in_src2);
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cmp (current_cpu, idesc, 1, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_bl8 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl8.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_bl24 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl24.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_bnc8 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl8.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    if (insn_referenced & (1 << 2)) referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_bnc24 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl24.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    if (insn_referenced & (1 << 2)) referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_bne (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    if (insn_referenced & (1 << 3)) referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cmp (current_cpu, idesc, 1, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_bra8 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl8.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_bra24 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl24.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_cmp (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cmp (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_cmpi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_d.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src2 = FLD (in_src2);
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cmp (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_cmpu (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cmp (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_cmpui (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_d.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src2 = FLD (in_src2);
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cmp (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_div (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 0)) referenced |= 1 << 1;
    if (insn_referenced & (1 << 2)) referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_divu (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 0)) referenced |= 1 << 1;
    if (insn_referenced & (1 << 2)) referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_rem (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 0)) referenced |= 1 << 1;
    if (insn_referenced & (1 << 2)) referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_remu (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    if (insn_referenced & (1 << 0)) referenced |= 1 << 1;
    if (insn_referenced & (1 << 2)) referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_jl (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_jl.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    in_sr = FLD (in_sr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_jmp (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_jl.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    in_sr = FLD (in_sr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_ld (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = 0;
    INT out_dr = 0;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_ld_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = 0;
    INT out_dr = 0;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_ldb (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = 0;
    INT out_dr = 0;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_ldb_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = 0;
    INT out_dr = 0;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_ldh (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = 0;
    INT out_dr = 0;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_ldh_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = 0;
    INT out_dr = 0;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_ldub (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = 0;
    INT out_dr = 0;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_ldub_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = 0;
    INT out_dr = 0;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_lduh (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = 0;
    INT out_dr = 0;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_lduh_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = 0;
    INT out_dr = 0;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_ld_plus (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = 0;
    INT out_dr = 0;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_dr = FLD (in_sr);
    out_dr = FLD (out_sr);
    referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 1, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_ld24 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld24.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    out_dr = FLD (out_dr);
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_ldi8 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    out_dr = FLD (out_dr);
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_ldi16 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    out_dr = FLD (out_dr);
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_lock (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = 0;
    INT out_dr = 0;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_machi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_maclo (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_macwhi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_macwlo (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_mul (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_mulhi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_mullo (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_mulwhi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_mulwlo (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_mv (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_mvfachi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_seth.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    out_dr = FLD (out_dr);
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_mvfaclo (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_seth.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    out_dr = FLD (out_dr);
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_mvfacmi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_seth.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    out_dr = FLD (out_dr);
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_mvfc (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    out_dr = FLD (out_dr);
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_mvtachi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_src1);
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_mvtaclo (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_src1);
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_mvtc (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    referenced |= 1 << 0;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_neg (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_nop (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_not (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_rac (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    cycles += m32rbf_model_m32r_d_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_rach (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    cycles += m32rbf_model_m32r_d_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_rte (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_seth (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_seth.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    out_dr = FLD (out_dr);
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_sll (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_sll3 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_slli (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_slli.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_sra (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_sra3 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_srai (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_slli.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_srl (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_srl3 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_srli (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_slli.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_st (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = 0;
    INT in_src2 = 0;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_store (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_st_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_d.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = 0;
    INT in_src2 = 0;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_store (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_stb (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = 0;
    INT in_src2 = 0;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_store (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_stb_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_d.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = 0;
    INT in_src2 = 0;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_store (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_sth (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = 0;
    INT in_src2 = 0;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_store (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_sth_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_d.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = 0;
    INT in_src2 = 0;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_store (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_st_plus (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = 0;
    INT in_src2 = 0;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_store (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_dr = FLD (in_src2);
    out_dr = FLD (out_src2);
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 1, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_st_minus (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = 0;
    INT in_src2 = 0;
    in_src1 = FLD (in_src1);
    in_src2 = FLD (in_src2);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    cycles += m32rbf_model_m32r_d_u_store (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_dr = FLD (in_src2);
    out_dr = FLD (out_src2);
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 1, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_sub (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_subv (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_subx (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    in_dr = FLD (in_dr);
    out_dr = FLD (out_dr);
    referenced |= 1 << 0;
    referenced |= 1 << 1;
    referenced |= 1 << 2;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_trap (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_trap.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_unlock (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = 0;
    INT out_dr = 0;
    cycles += m32rbf_model_m32r_d_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_clrpsw (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_clrpsw.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_setpsw (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_clrpsw.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_bset (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bset.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    referenced |= 1 << 0;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_bclr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bset.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    referenced |= 1 << 0;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32r_d_btst (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bset.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_sr = FLD (in_sr);
    referenced |= 1 << 0;
    cycles += m32rbf_model_m32r_d_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_test_add (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_add3 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_and (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_and3 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_and3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_or (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_or3 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_and3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_xor (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_xor3 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_and3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_addi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_addv (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_addv3 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_addx (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_bc8 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl8.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_bc24 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl24.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_beq (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_beqz (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_bgez (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_bgtz (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_blez (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_bltz (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_bnez (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_bl8 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl8.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_bl24 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl24.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_bnc8 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl8.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_bnc24 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl24.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_bne (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_beq.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_bra8 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl8.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_bra24 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl24.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_cmp (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_cmpi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_d.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_cmpu (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_cmpui (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_d.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_div (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_divu (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_rem (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_remu (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_jl (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_jl.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_jmp (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_jl.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_ld (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_ld_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_ldb (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_ldb_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_ldh (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_ldh_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_ldub (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_ldub_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_lduh (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_lduh_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_ld_plus (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_ld24 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld24.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_ldi8 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_addi.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_ldi16 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_lock (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_machi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_maclo (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_macwhi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_macwlo (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_mul (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_mulhi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_mullo (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_mulwhi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_mulwlo (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_mv (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_mvfachi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_seth.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_mvfaclo (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_seth.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_mvfacmi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_seth.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_mvfc (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_mvtachi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_mvtaclo (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_mvtc (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_neg (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_nop (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_not (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_ld_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_rac (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_rach (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_rte (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.fmt_empty.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_seth (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_seth.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_sll (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_sll3 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_slli (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_slli.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_sra (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_sra3 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_srai (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_slli.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_srl (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_srl3 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add3.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_srli (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_slli.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_st (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_st_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_d.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_stb (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_stb_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_d.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_sth (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_sth_d (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_d.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_st_plus (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_st_minus (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_sub (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_subv (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_subx (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_add.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_trap (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_trap.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_unlock (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_st_plus.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_clrpsw (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_clrpsw.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_setpsw (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_clrpsw.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_bset (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bset.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_bclr (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bset.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

static int
model_test_btst (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bset.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    cycles += m32rbf_model_test_u_exec (current_cpu, idesc, 0, referenced);
  }
  return cycles;
#undef FLD
}

/* We assume UNIT_NONE == 0 because the tables don't always terminate
   entries with it.  */

/* Model timing data for `m32r/d'.  */

static const INSN_TIMING m32r_d_timing[] = {
  { M32RBF_INSN_X_INVALID, 0, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_X_AFTER, 0, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_X_BEFORE, 0, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_X_CTI_CHAIN, 0, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_X_CHAIN, 0, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_X_BEGIN, 0, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_ADD, model_m32r_d_add, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_ADD3, model_m32r_d_add3, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_AND, model_m32r_d_and, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_AND3, model_m32r_d_and3, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_OR, model_m32r_d_or, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_OR3, model_m32r_d_or3, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_XOR, model_m32r_d_xor, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_XOR3, model_m32r_d_xor3, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_ADDI, model_m32r_d_addi, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_ADDV, model_m32r_d_addv, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_ADDV3, model_m32r_d_addv3, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_ADDX, model_m32r_d_addx, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BC8, model_m32r_d_bc8, { { (int) UNIT_M32R_D_U_CTI, 1, 1 } } },
  { M32RBF_INSN_BC24, model_m32r_d_bc24, { { (int) UNIT_M32R_D_U_CTI, 1, 1 } } },
  { M32RBF_INSN_BEQ, model_m32r_d_beq, { { (int) UNIT_M32R_D_U_CTI, 1, 1 }, { (int) UNIT_M32R_D_U_CMP, 1, 0 } } },
  { M32RBF_INSN_BEQZ, model_m32r_d_beqz, { { (int) UNIT_M32R_D_U_CTI, 1, 1 }, { (int) UNIT_M32R_D_U_CMP, 1, 0 } } },
  { M32RBF_INSN_BGEZ, model_m32r_d_bgez, { { (int) UNIT_M32R_D_U_CTI, 1, 1 }, { (int) UNIT_M32R_D_U_CMP, 1, 0 } } },
  { M32RBF_INSN_BGTZ, model_m32r_d_bgtz, { { (int) UNIT_M32R_D_U_CTI, 1, 1 }, { (int) UNIT_M32R_D_U_CMP, 1, 0 } } },
  { M32RBF_INSN_BLEZ, model_m32r_d_blez, { { (int) UNIT_M32R_D_U_CTI, 1, 1 }, { (int) UNIT_M32R_D_U_CMP, 1, 0 } } },
  { M32RBF_INSN_BLTZ, model_m32r_d_bltz, { { (int) UNIT_M32R_D_U_CTI, 1, 1 }, { (int) UNIT_M32R_D_U_CMP, 1, 0 } } },
  { M32RBF_INSN_BNEZ, model_m32r_d_bnez, { { (int) UNIT_M32R_D_U_CTI, 1, 1 }, { (int) UNIT_M32R_D_U_CMP, 1, 0 } } },
  { M32RBF_INSN_BL8, model_m32r_d_bl8, { { (int) UNIT_M32R_D_U_CTI, 1, 1 } } },
  { M32RBF_INSN_BL24, model_m32r_d_bl24, { { (int) UNIT_M32R_D_U_CTI, 1, 1 } } },
  { M32RBF_INSN_BNC8, model_m32r_d_bnc8, { { (int) UNIT_M32R_D_U_CTI, 1, 1 } } },
  { M32RBF_INSN_BNC24, model_m32r_d_bnc24, { { (int) UNIT_M32R_D_U_CTI, 1, 1 } } },
  { M32RBF_INSN_BNE, model_m32r_d_bne, { { (int) UNIT_M32R_D_U_CTI, 1, 1 }, { (int) UNIT_M32R_D_U_CMP, 1, 0 } } },
  { M32RBF_INSN_BRA8, model_m32r_d_bra8, { { (int) UNIT_M32R_D_U_CTI, 1, 1 } } },
  { M32RBF_INSN_BRA24, model_m32r_d_bra24, { { (int) UNIT_M32R_D_U_CTI, 1, 1 } } },
  { M32RBF_INSN_CMP, model_m32r_d_cmp, { { (int) UNIT_M32R_D_U_CMP, 1, 1 } } },
  { M32RBF_INSN_CMPI, model_m32r_d_cmpi, { { (int) UNIT_M32R_D_U_CMP, 1, 1 } } },
  { M32RBF_INSN_CMPU, model_m32r_d_cmpu, { { (int) UNIT_M32R_D_U_CMP, 1, 1 } } },
  { M32RBF_INSN_CMPUI, model_m32r_d_cmpui, { { (int) UNIT_M32R_D_U_CMP, 1, 1 } } },
  { M32RBF_INSN_DIV, model_m32r_d_div, { { (int) UNIT_M32R_D_U_EXEC, 1, 37 } } },
  { M32RBF_INSN_DIVU, model_m32r_d_divu, { { (int) UNIT_M32R_D_U_EXEC, 1, 37 } } },
  { M32RBF_INSN_REM, model_m32r_d_rem, { { (int) UNIT_M32R_D_U_EXEC, 1, 37 } } },
  { M32RBF_INSN_REMU, model_m32r_d_remu, { { (int) UNIT_M32R_D_U_EXEC, 1, 37 } } },
  { M32RBF_INSN_JL, model_m32r_d_jl, { { (int) UNIT_M32R_D_U_CTI, 1, 1 } } },
  { M32RBF_INSN_JMP, model_m32r_d_jmp, { { (int) UNIT_M32R_D_U_CTI, 1, 1 } } },
  { M32RBF_INSN_LD, model_m32r_d_ld, { { (int) UNIT_M32R_D_U_LOAD, 1, 1 } } },
  { M32RBF_INSN_LD_D, model_m32r_d_ld_d, { { (int) UNIT_M32R_D_U_LOAD, 1, 2 } } },
  { M32RBF_INSN_LDB, model_m32r_d_ldb, { { (int) UNIT_M32R_D_U_LOAD, 1, 1 } } },
  { M32RBF_INSN_LDB_D, model_m32r_d_ldb_d, { { (int) UNIT_M32R_D_U_LOAD, 1, 2 } } },
  { M32RBF_INSN_LDH, model_m32r_d_ldh, { { (int) UNIT_M32R_D_U_LOAD, 1, 1 } } },
  { M32RBF_INSN_LDH_D, model_m32r_d_ldh_d, { { (int) UNIT_M32R_D_U_LOAD, 1, 2 } } },
  { M32RBF_INSN_LDUB, model_m32r_d_ldub, { { (int) UNIT_M32R_D_U_LOAD, 1, 1 } } },
  { M32RBF_INSN_LDUB_D, model_m32r_d_ldub_d, { { (int) UNIT_M32R_D_U_LOAD, 1, 2 } } },
  { M32RBF_INSN_LDUH, model_m32r_d_lduh, { { (int) UNIT_M32R_D_U_LOAD, 1, 1 } } },
  { M32RBF_INSN_LDUH_D, model_m32r_d_lduh_d, { { (int) UNIT_M32R_D_U_LOAD, 1, 2 } } },
  { M32RBF_INSN_LD_PLUS, model_m32r_d_ld_plus, { { (int) UNIT_M32R_D_U_LOAD, 1, 1 }, { (int) UNIT_M32R_D_U_EXEC, 1, 0 } } },
  { M32RBF_INSN_LD24, model_m32r_d_ld24, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_LDI8, model_m32r_d_ldi8, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_LDI16, model_m32r_d_ldi16, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_LOCK, model_m32r_d_lock, { { (int) UNIT_M32R_D_U_LOAD, 1, 1 } } },
  { M32RBF_INSN_MACHI, model_m32r_d_machi, { { (int) UNIT_M32R_D_U_MAC, 1, 1 } } },
  { M32RBF_INSN_MACLO, model_m32r_d_maclo, { { (int) UNIT_M32R_D_U_MAC, 1, 1 } } },
  { M32RBF_INSN_MACWHI, model_m32r_d_macwhi, { { (int) UNIT_M32R_D_U_MAC, 1, 1 } } },
  { M32RBF_INSN_MACWLO, model_m32r_d_macwlo, { { (int) UNIT_M32R_D_U_MAC, 1, 1 } } },
  { M32RBF_INSN_MUL, model_m32r_d_mul, { { (int) UNIT_M32R_D_U_EXEC, 1, 4 } } },
  { M32RBF_INSN_MULHI, model_m32r_d_mulhi, { { (int) UNIT_M32R_D_U_MAC, 1, 1 } } },
  { M32RBF_INSN_MULLO, model_m32r_d_mullo, { { (int) UNIT_M32R_D_U_MAC, 1, 1 } } },
  { M32RBF_INSN_MULWHI, model_m32r_d_mulwhi, { { (int) UNIT_M32R_D_U_MAC, 1, 1 } } },
  { M32RBF_INSN_MULWLO, model_m32r_d_mulwlo, { { (int) UNIT_M32R_D_U_MAC, 1, 1 } } },
  { M32RBF_INSN_MV, model_m32r_d_mv, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MVFACHI, model_m32r_d_mvfachi, { { (int) UNIT_M32R_D_U_EXEC, 1, 2 } } },
  { M32RBF_INSN_MVFACLO, model_m32r_d_mvfaclo, { { (int) UNIT_M32R_D_U_EXEC, 1, 2 } } },
  { M32RBF_INSN_MVFACMI, model_m32r_d_mvfacmi, { { (int) UNIT_M32R_D_U_EXEC, 1, 2 } } },
  { M32RBF_INSN_MVFC, model_m32r_d_mvfc, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MVTACHI, model_m32r_d_mvtachi, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MVTACLO, model_m32r_d_mvtaclo, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MVTC, model_m32r_d_mvtc, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_NEG, model_m32r_d_neg, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_NOP, model_m32r_d_nop, { { (int) UNIT_M32R_D_U_EXEC, 1, 0 } } },
  { M32RBF_INSN_NOT, model_m32r_d_not, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_RAC, model_m32r_d_rac, { { (int) UNIT_M32R_D_U_MAC, 1, 1 } } },
  { M32RBF_INSN_RACH, model_m32r_d_rach, { { (int) UNIT_M32R_D_U_MAC, 1, 1 } } },
  { M32RBF_INSN_RTE, model_m32r_d_rte, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SETH, model_m32r_d_seth, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SLL, model_m32r_d_sll, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SLL3, model_m32r_d_sll3, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SLLI, model_m32r_d_slli, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SRA, model_m32r_d_sra, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SRA3, model_m32r_d_sra3, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SRAI, model_m32r_d_srai, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SRL, model_m32r_d_srl, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SRL3, model_m32r_d_srl3, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SRLI, model_m32r_d_srli, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_ST, model_m32r_d_st, { { (int) UNIT_M32R_D_U_STORE, 1, 1 } } },
  { M32RBF_INSN_ST_D, model_m32r_d_st_d, { { (int) UNIT_M32R_D_U_STORE, 1, 2 } } },
  { M32RBF_INSN_STB, model_m32r_d_stb, { { (int) UNIT_M32R_D_U_STORE, 1, 1 } } },
  { M32RBF_INSN_STB_D, model_m32r_d_stb_d, { { (int) UNIT_M32R_D_U_STORE, 1, 2 } } },
  { M32RBF_INSN_STH, model_m32r_d_sth, { { (int) UNIT_M32R_D_U_STORE, 1, 1 } } },
  { M32RBF_INSN_STH_D, model_m32r_d_sth_d, { { (int) UNIT_M32R_D_U_STORE, 1, 2 } } },
  { M32RBF_INSN_ST_PLUS, model_m32r_d_st_plus, { { (int) UNIT_M32R_D_U_STORE, 1, 1 }, { (int) UNIT_M32R_D_U_EXEC, 1, 0 } } },
  { M32RBF_INSN_ST_MINUS, model_m32r_d_st_minus, { { (int) UNIT_M32R_D_U_STORE, 1, 1 }, { (int) UNIT_M32R_D_U_EXEC, 1, 0 } } },
  { M32RBF_INSN_SUB, model_m32r_d_sub, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SUBV, model_m32r_d_subv, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SUBX, model_m32r_d_subx, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_TRAP, model_m32r_d_trap, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_UNLOCK, model_m32r_d_unlock, { { (int) UNIT_M32R_D_U_LOAD, 1, 1 } } },
  { M32RBF_INSN_CLRPSW, model_m32r_d_clrpsw, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SETPSW, model_m32r_d_setpsw, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BSET, model_m32r_d_bset, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BCLR, model_m32r_d_bclr, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BTST, model_m32r_d_btst, { { (int) UNIT_M32R_D_U_EXEC, 1, 1 } } },
};

/* Model timing data for `test'.  */

static const INSN_TIMING test_timing[] = {
  { M32RBF_INSN_X_INVALID, 0, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_X_AFTER, 0, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_X_BEFORE, 0, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_X_CTI_CHAIN, 0, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_X_CHAIN, 0, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_X_BEGIN, 0, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_ADD, model_test_add, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_ADD3, model_test_add3, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_AND, model_test_and, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_AND3, model_test_and3, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_OR, model_test_or, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_OR3, model_test_or3, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_XOR, model_test_xor, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_XOR3, model_test_xor3, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_ADDI, model_test_addi, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_ADDV, model_test_addv, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_ADDV3, model_test_addv3, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_ADDX, model_test_addx, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BC8, model_test_bc8, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BC24, model_test_bc24, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BEQ, model_test_beq, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BEQZ, model_test_beqz, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BGEZ, model_test_bgez, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BGTZ, model_test_bgtz, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BLEZ, model_test_blez, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BLTZ, model_test_bltz, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BNEZ, model_test_bnez, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BL8, model_test_bl8, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BL24, model_test_bl24, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BNC8, model_test_bnc8, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BNC24, model_test_bnc24, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BNE, model_test_bne, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BRA8, model_test_bra8, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BRA24, model_test_bra24, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_CMP, model_test_cmp, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_CMPI, model_test_cmpi, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_CMPU, model_test_cmpu, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_CMPUI, model_test_cmpui, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_DIV, model_test_div, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_DIVU, model_test_divu, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_REM, model_test_rem, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_REMU, model_test_remu, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_JL, model_test_jl, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_JMP, model_test_jmp, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_LD, model_test_ld, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_LD_D, model_test_ld_d, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_LDB, model_test_ldb, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_LDB_D, model_test_ldb_d, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_LDH, model_test_ldh, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_LDH_D, model_test_ldh_d, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_LDUB, model_test_ldub, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_LDUB_D, model_test_ldub_d, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_LDUH, model_test_lduh, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_LDUH_D, model_test_lduh_d, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_LD_PLUS, model_test_ld_plus, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_LD24, model_test_ld24, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_LDI8, model_test_ldi8, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_LDI16, model_test_ldi16, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_LOCK, model_test_lock, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MACHI, model_test_machi, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MACLO, model_test_maclo, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MACWHI, model_test_macwhi, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MACWLO, model_test_macwlo, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MUL, model_test_mul, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MULHI, model_test_mulhi, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MULLO, model_test_mullo, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MULWHI, model_test_mulwhi, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MULWLO, model_test_mulwlo, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MV, model_test_mv, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MVFACHI, model_test_mvfachi, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MVFACLO, model_test_mvfaclo, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MVFACMI, model_test_mvfacmi, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MVFC, model_test_mvfc, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MVTACHI, model_test_mvtachi, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MVTACLO, model_test_mvtaclo, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_MVTC, model_test_mvtc, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_NEG, model_test_neg, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_NOP, model_test_nop, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_NOT, model_test_not, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_RAC, model_test_rac, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_RACH, model_test_rach, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_RTE, model_test_rte, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SETH, model_test_seth, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SLL, model_test_sll, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SLL3, model_test_sll3, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SLLI, model_test_slli, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SRA, model_test_sra, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SRA3, model_test_sra3, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SRAI, model_test_srai, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SRL, model_test_srl, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SRL3, model_test_srl3, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SRLI, model_test_srli, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_ST, model_test_st, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_ST_D, model_test_st_d, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_STB, model_test_stb, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_STB_D, model_test_stb_d, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_STH, model_test_sth, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_STH_D, model_test_sth_d, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_ST_PLUS, model_test_st_plus, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_ST_MINUS, model_test_st_minus, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SUB, model_test_sub, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SUBV, model_test_subv, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SUBX, model_test_subx, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_TRAP, model_test_trap, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_UNLOCK, model_test_unlock, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_CLRPSW, model_test_clrpsw, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_SETPSW, model_test_setpsw, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BSET, model_test_bset, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BCLR, model_test_bclr, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
  { M32RBF_INSN_BTST, model_test_btst, { { (int) UNIT_TEST_U_EXEC, 1, 1 } } },
};

#endif /* WITH_PROFILE_MODEL_P */

static void
m32r_d_model_init (SIM_CPU *cpu)
{
  CPU_MODEL_DATA (cpu) = (void *) zalloc (sizeof (MODEL_M32R_D_DATA));
}

static void
test_model_init (SIM_CPU *cpu)
{
  CPU_MODEL_DATA (cpu) = (void *) zalloc (sizeof (MODEL_TEST_DATA));
}

#if WITH_PROFILE_MODEL_P
#define TIMING_DATA(td) td
#else
#define TIMING_DATA(td) 0
#endif

static const MODEL m32r_models[] =
{
  { "m32r/d", & m32r_mach, MODEL_M32R_D, TIMING_DATA (& m32r_d_timing[0]), m32r_d_model_init },
  { "test", & m32r_mach, MODEL_TEST, TIMING_DATA (& test_timing[0]), test_model_init },
  { 0 }
};

/* The properties of this cpu's implementation.  */

static const MACH_IMP_PROPERTIES m32rbf_imp_properties =
{
  sizeof (SIM_CPU),
#if WITH_SCACHE
  sizeof (SCACHE)
#else
  0
#endif
};


static void
m32rbf_prepare_run (SIM_CPU *cpu)
{
  if (CPU_IDESC (cpu) == NULL)
    m32rbf_init_idesc_table (cpu);
}

static const CGEN_INSN *
m32rbf_get_idata (SIM_CPU *cpu, int inum)
{
  return CPU_IDESC (cpu) [inum].idata;
}

static void
m32r_init_cpu (SIM_CPU *cpu)
{
  CPU_REG_FETCH (cpu) = m32rbf_fetch_register;
  CPU_REG_STORE (cpu) = m32rbf_store_register;
  CPU_PC_FETCH (cpu) = m32rbf_h_pc_get;
  CPU_PC_STORE (cpu) = m32rbf_h_pc_set;
  CPU_GET_IDATA (cpu) = m32rbf_get_idata;
  CPU_MAX_INSNS (cpu) = M32RBF_INSN__MAX;
  CPU_INSN_NAME (cpu) = cgen_insn_name;
  CPU_FULL_ENGINE_FN (cpu) = m32rbf_engine_run_full;
#if WITH_FAST
  CPU_FAST_ENGINE_FN (cpu) = m32rbf_engine_run_fast;
#else
  CPU_FAST_ENGINE_FN (cpu) = m32rbf_engine_run_full;
#endif
}

const MACH m32r_mach =
{
  "m32r", "m32r", MACH_M32R,
  32, 32, & m32r_models[0], & m32rbf_imp_properties,
  m32r_init_cpu,
  m32rbf_prepare_run
};

