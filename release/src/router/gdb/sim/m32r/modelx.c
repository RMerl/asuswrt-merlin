/* Simulator model support for m32rxf.

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

#define WANT_CPU m32rxf
#define WANT_CPU_M32RXF

#include "sim-main.h"

/* The profiling data is recorded here, but is accessed via the profiling
   mechanism.  After all, this is information for profiling.  */

#if WITH_PROFILE_MODEL_P

/* Model handlers for each insn.  */

static int
model_m32rx_add (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_add3 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_and (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_and3 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_or (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_or3 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_xor (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_xor3 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_addi (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_addv (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_addv3 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_addx (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bc8 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bc24 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_beq (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
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
    cycles += m32rxf_model_m32rx_u_cmp (current_cpu, idesc, 1, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_beqz (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src2 = FLD (in_src2);
    referenced |= 1 << 1;
    cycles += m32rxf_model_m32rx_u_cmp (current_cpu, idesc, 1, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bgez (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src2 = FLD (in_src2);
    referenced |= 1 << 1;
    cycles += m32rxf_model_m32rx_u_cmp (current_cpu, idesc, 1, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bgtz (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src2 = FLD (in_src2);
    referenced |= 1 << 1;
    cycles += m32rxf_model_m32rx_u_cmp (current_cpu, idesc, 1, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_blez (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src2 = FLD (in_src2);
    referenced |= 1 << 1;
    cycles += m32rxf_model_m32rx_u_cmp (current_cpu, idesc, 1, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bltz (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src2 = FLD (in_src2);
    referenced |= 1 << 1;
    cycles += m32rxf_model_m32rx_u_cmp (current_cpu, idesc, 1, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bnez (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    in_src2 = FLD (in_src2);
    referenced |= 1 << 1;
    cycles += m32rxf_model_m32rx_u_cmp (current_cpu, idesc, 1, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bl8 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bl24 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bcl8 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl8.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    if (insn_referenced & (1 << 4)) referenced |= 1 << 1;
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bcl24 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl24.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    if (insn_referenced & (1 << 4)) referenced |= 1 << 1;
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bnc8 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bnc24 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bne (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
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
    cycles += m32rxf_model_m32rx_u_cmp (current_cpu, idesc, 1, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bra8 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bra24 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bncl8 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl8.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    if (insn_referenced & (1 << 4)) referenced |= 1 << 1;
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bncl24 (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_bl24.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    if (insn_referenced & (1 << 4)) referenced |= 1 << 1;
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_cmp (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cmp (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_cmpi (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cmp (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_cmpu (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cmp (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_cmpui (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cmp (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_cmpeq (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cmp (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_cmpz (SIM_CPU *current_cpu, void *sem_arg)
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
    in_src2 = FLD (in_src2);
    referenced |= 1 << 1;
    cycles += m32rxf_model_m32rx_u_cmp (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_div (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_divu (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_rem (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_remu (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_divh (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_jc (SIM_CPU *current_cpu, void *sem_arg)
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
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    if (insn_referenced & (1 << 2)) referenced |= 1 << 1;
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_jnc (SIM_CPU *current_cpu, void *sem_arg)
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
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    if (insn_referenced & (1 << 2)) referenced |= 1 << 1;
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_jl (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_jmp (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_cti (current_cpu, idesc, 0, referenced, in_sr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_ld (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_ld_d (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_ldb (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_ldb_d (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_ldh (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_ldh_d (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_ldub (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_ldub_d (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_lduh (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_lduh_d (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_ld_plus (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 1, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_ld24 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_ldi8 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_ldi16 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_lock (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_machi_a (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_machi_a.f
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
    cycles += m32rxf_model_m32rx_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_maclo_a (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_machi_a.f
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
    cycles += m32rxf_model_m32rx_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_macwhi_a (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_machi_a.f
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
    cycles += m32rxf_model_m32rx_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_macwlo_a (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_machi_a.f
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
    cycles += m32rxf_model_m32rx_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_mul (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_mulhi_a (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_machi_a.f
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
    cycles += m32rxf_model_m32rx_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_mullo_a (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_machi_a.f
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
    cycles += m32rxf_model_m32rx_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_mulwhi_a (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_machi_a.f
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
    cycles += m32rxf_model_m32rx_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_mulwlo_a (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_machi_a.f
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
    cycles += m32rxf_model_m32rx_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_mv (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_mvfachi_a (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mvfachi_a.f
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_mvfaclo_a (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mvfachi_a.f
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_mvfacmi_a (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mvfachi_a.f
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_mvfc (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_mvtachi_a (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mvtachi_a.f
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_mvtaclo_a (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_mvtachi_a.f
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_mvtc (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_neg (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_nop (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_not (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_rac_dsi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_rac_dsi.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    cycles += m32rxf_model_m32rx_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_rach_dsi (SIM_CPU *current_cpu, void *sem_arg)
{
#define FLD(f) abuf->fields.sfmt_rac_dsi.f
  const ARGBUF * UNUSED abuf = SEM_ARGBUF ((SEM_ARG) sem_arg);
  const IDESC * UNUSED idesc = abuf->idesc;
  int cycles = 0;
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_src1 = -1;
    INT in_src2 = -1;
    cycles += m32rxf_model_m32rx_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_rte (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_seth (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_sll (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_sll3 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_slli (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_sra (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_sra3 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_srai (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_srl (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_srl3 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_srli (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_st (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_store (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_st_d (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_store (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_stb (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_store (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_stb_d (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_store (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_sth (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_store (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_sth_d (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_store (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_st_plus (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_store (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_dr = FLD (in_src2);
    out_dr = FLD (out_src2);
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 1, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_sth_plus (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_store (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_dr = FLD (in_src2);
    out_dr = FLD (out_src2);
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 1, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_stb_plus (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_store (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_dr = FLD (in_src2);
    out_dr = FLD (out_src2);
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 1, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_st_minus (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_store (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  {
    int referenced = 0;
    int UNUSED insn_referenced = abuf->written;
    INT in_sr = -1;
    INT in_dr = -1;
    INT out_dr = -1;
    in_dr = FLD (in_src2);
    out_dr = FLD (out_src2);
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 1, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_sub (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_subv (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_subx (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_trap (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_unlock (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_load (current_cpu, idesc, 0, referenced, in_sr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_satb (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_sath (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_sat (SIM_CPU *current_cpu, void *sem_arg)
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
    if (insn_referenced & (1 << 1)) referenced |= 1 << 0;
    referenced |= 1 << 2;
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_pcmpbz (SIM_CPU *current_cpu, void *sem_arg)
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
    in_src2 = FLD (in_src2);
    referenced |= 1 << 1;
    cycles += m32rxf_model_m32rx_u_cmp (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_sadd (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_macwu1 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_msblo (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_mulwu1 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_maclh1 (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_mac (current_cpu, idesc, 0, referenced, in_src1, in_src2);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_sc (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_snc (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_clrpsw (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_setpsw (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bset (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_bclr (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

static int
model_m32rx_btst (SIM_CPU *current_cpu, void *sem_arg)
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
    cycles += m32rxf_model_m32rx_u_exec (current_cpu, idesc, 0, referenced, in_sr, in_dr, out_dr);
  }
  return cycles;
#undef FLD
}

/* We assume UNIT_NONE == 0 because the tables don't always terminate
   entries with it.  */

/* Model timing data for `m32rx'.  */

static const INSN_TIMING m32rx_timing[] = {
  { M32RXF_INSN_X_INVALID, 0, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_X_AFTER, 0, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_X_BEFORE, 0, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_X_CTI_CHAIN, 0, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_X_CHAIN, 0, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_X_BEGIN, 0, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_ADD, model_m32rx_add, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_ADD3, model_m32rx_add3, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_AND, model_m32rx_and, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_AND3, model_m32rx_and3, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_OR, model_m32rx_or, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_OR3, model_m32rx_or3, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_XOR, model_m32rx_xor, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_XOR3, model_m32rx_xor3, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_ADDI, model_m32rx_addi, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_ADDV, model_m32rx_addv, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_ADDV3, model_m32rx_addv3, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_ADDX, model_m32rx_addx, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_BC8, model_m32rx_bc8, { { (int) UNIT_M32RX_U_CTI, 1, 1 } } },
  { M32RXF_INSN_BC24, model_m32rx_bc24, { { (int) UNIT_M32RX_U_CTI, 1, 1 } } },
  { M32RXF_INSN_BEQ, model_m32rx_beq, { { (int) UNIT_M32RX_U_CTI, 1, 1 }, { (int) UNIT_M32RX_U_CMP, 1, 0 } } },
  { M32RXF_INSN_BEQZ, model_m32rx_beqz, { { (int) UNIT_M32RX_U_CTI, 1, 1 }, { (int) UNIT_M32RX_U_CMP, 1, 0 } } },
  { M32RXF_INSN_BGEZ, model_m32rx_bgez, { { (int) UNIT_M32RX_U_CTI, 1, 1 }, { (int) UNIT_M32RX_U_CMP, 1, 0 } } },
  { M32RXF_INSN_BGTZ, model_m32rx_bgtz, { { (int) UNIT_M32RX_U_CTI, 1, 1 }, { (int) UNIT_M32RX_U_CMP, 1, 0 } } },
  { M32RXF_INSN_BLEZ, model_m32rx_blez, { { (int) UNIT_M32RX_U_CTI, 1, 1 }, { (int) UNIT_M32RX_U_CMP, 1, 0 } } },
  { M32RXF_INSN_BLTZ, model_m32rx_bltz, { { (int) UNIT_M32RX_U_CTI, 1, 1 }, { (int) UNIT_M32RX_U_CMP, 1, 0 } } },
  { M32RXF_INSN_BNEZ, model_m32rx_bnez, { { (int) UNIT_M32RX_U_CTI, 1, 1 }, { (int) UNIT_M32RX_U_CMP, 1, 0 } } },
  { M32RXF_INSN_BL8, model_m32rx_bl8, { { (int) UNIT_M32RX_U_CTI, 1, 1 } } },
  { M32RXF_INSN_BL24, model_m32rx_bl24, { { (int) UNIT_M32RX_U_CTI, 1, 1 } } },
  { M32RXF_INSN_BCL8, model_m32rx_bcl8, { { (int) UNIT_M32RX_U_CTI, 1, 1 } } },
  { M32RXF_INSN_BCL24, model_m32rx_bcl24, { { (int) UNIT_M32RX_U_CTI, 1, 1 } } },
  { M32RXF_INSN_BNC8, model_m32rx_bnc8, { { (int) UNIT_M32RX_U_CTI, 1, 1 } } },
  { M32RXF_INSN_BNC24, model_m32rx_bnc24, { { (int) UNIT_M32RX_U_CTI, 1, 1 } } },
  { M32RXF_INSN_BNE, model_m32rx_bne, { { (int) UNIT_M32RX_U_CTI, 1, 1 }, { (int) UNIT_M32RX_U_CMP, 1, 0 } } },
  { M32RXF_INSN_BRA8, model_m32rx_bra8, { { (int) UNIT_M32RX_U_CTI, 1, 1 } } },
  { M32RXF_INSN_BRA24, model_m32rx_bra24, { { (int) UNIT_M32RX_U_CTI, 1, 1 } } },
  { M32RXF_INSN_BNCL8, model_m32rx_bncl8, { { (int) UNIT_M32RX_U_CTI, 1, 1 } } },
  { M32RXF_INSN_BNCL24, model_m32rx_bncl24, { { (int) UNIT_M32RX_U_CTI, 1, 1 } } },
  { M32RXF_INSN_CMP, model_m32rx_cmp, { { (int) UNIT_M32RX_U_CMP, 1, 1 } } },
  { M32RXF_INSN_CMPI, model_m32rx_cmpi, { { (int) UNIT_M32RX_U_CMP, 1, 1 } } },
  { M32RXF_INSN_CMPU, model_m32rx_cmpu, { { (int) UNIT_M32RX_U_CMP, 1, 1 } } },
  { M32RXF_INSN_CMPUI, model_m32rx_cmpui, { { (int) UNIT_M32RX_U_CMP, 1, 1 } } },
  { M32RXF_INSN_CMPEQ, model_m32rx_cmpeq, { { (int) UNIT_M32RX_U_CMP, 1, 1 } } },
  { M32RXF_INSN_CMPZ, model_m32rx_cmpz, { { (int) UNIT_M32RX_U_CMP, 1, 1 } } },
  { M32RXF_INSN_DIV, model_m32rx_div, { { (int) UNIT_M32RX_U_EXEC, 1, 37 } } },
  { M32RXF_INSN_DIVU, model_m32rx_divu, { { (int) UNIT_M32RX_U_EXEC, 1, 37 } } },
  { M32RXF_INSN_REM, model_m32rx_rem, { { (int) UNIT_M32RX_U_EXEC, 1, 37 } } },
  { M32RXF_INSN_REMU, model_m32rx_remu, { { (int) UNIT_M32RX_U_EXEC, 1, 37 } } },
  { M32RXF_INSN_DIVH, model_m32rx_divh, { { (int) UNIT_M32RX_U_EXEC, 1, 21 } } },
  { M32RXF_INSN_JC, model_m32rx_jc, { { (int) UNIT_M32RX_U_CTI, 1, 1 } } },
  { M32RXF_INSN_JNC, model_m32rx_jnc, { { (int) UNIT_M32RX_U_CTI, 1, 1 } } },
  { M32RXF_INSN_JL, model_m32rx_jl, { { (int) UNIT_M32RX_U_CTI, 1, 1 } } },
  { M32RXF_INSN_JMP, model_m32rx_jmp, { { (int) UNIT_M32RX_U_CTI, 1, 1 } } },
  { M32RXF_INSN_LD, model_m32rx_ld, { { (int) UNIT_M32RX_U_LOAD, 1, 1 } } },
  { M32RXF_INSN_LD_D, model_m32rx_ld_d, { { (int) UNIT_M32RX_U_LOAD, 1, 2 } } },
  { M32RXF_INSN_LDB, model_m32rx_ldb, { { (int) UNIT_M32RX_U_LOAD, 1, 1 } } },
  { M32RXF_INSN_LDB_D, model_m32rx_ldb_d, { { (int) UNIT_M32RX_U_LOAD, 1, 2 } } },
  { M32RXF_INSN_LDH, model_m32rx_ldh, { { (int) UNIT_M32RX_U_LOAD, 1, 1 } } },
  { M32RXF_INSN_LDH_D, model_m32rx_ldh_d, { { (int) UNIT_M32RX_U_LOAD, 1, 2 } } },
  { M32RXF_INSN_LDUB, model_m32rx_ldub, { { (int) UNIT_M32RX_U_LOAD, 1, 1 } } },
  { M32RXF_INSN_LDUB_D, model_m32rx_ldub_d, { { (int) UNIT_M32RX_U_LOAD, 1, 2 } } },
  { M32RXF_INSN_LDUH, model_m32rx_lduh, { { (int) UNIT_M32RX_U_LOAD, 1, 1 } } },
  { M32RXF_INSN_LDUH_D, model_m32rx_lduh_d, { { (int) UNIT_M32RX_U_LOAD, 1, 2 } } },
  { M32RXF_INSN_LD_PLUS, model_m32rx_ld_plus, { { (int) UNIT_M32RX_U_LOAD, 1, 1 }, { (int) UNIT_M32RX_U_EXEC, 1, 0 } } },
  { M32RXF_INSN_LD24, model_m32rx_ld24, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_LDI8, model_m32rx_ldi8, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_LDI16, model_m32rx_ldi16, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_LOCK, model_m32rx_lock, { { (int) UNIT_M32RX_U_LOAD, 1, 1 } } },
  { M32RXF_INSN_MACHI_A, model_m32rx_machi_a, { { (int) UNIT_M32RX_U_MAC, 1, 1 } } },
  { M32RXF_INSN_MACLO_A, model_m32rx_maclo_a, { { (int) UNIT_M32RX_U_MAC, 1, 1 } } },
  { M32RXF_INSN_MACWHI_A, model_m32rx_macwhi_a, { { (int) UNIT_M32RX_U_MAC, 1, 1 } } },
  { M32RXF_INSN_MACWLO_A, model_m32rx_macwlo_a, { { (int) UNIT_M32RX_U_MAC, 1, 1 } } },
  { M32RXF_INSN_MUL, model_m32rx_mul, { { (int) UNIT_M32RX_U_EXEC, 1, 4 } } },
  { M32RXF_INSN_MULHI_A, model_m32rx_mulhi_a, { { (int) UNIT_M32RX_U_MAC, 1, 1 } } },
  { M32RXF_INSN_MULLO_A, model_m32rx_mullo_a, { { (int) UNIT_M32RX_U_MAC, 1, 1 } } },
  { M32RXF_INSN_MULWHI_A, model_m32rx_mulwhi_a, { { (int) UNIT_M32RX_U_MAC, 1, 1 } } },
  { M32RXF_INSN_MULWLO_A, model_m32rx_mulwlo_a, { { (int) UNIT_M32RX_U_MAC, 1, 1 } } },
  { M32RXF_INSN_MV, model_m32rx_mv, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_MVFACHI_A, model_m32rx_mvfachi_a, { { (int) UNIT_M32RX_U_EXEC, 1, 2 } } },
  { M32RXF_INSN_MVFACLO_A, model_m32rx_mvfaclo_a, { { (int) UNIT_M32RX_U_EXEC, 1, 2 } } },
  { M32RXF_INSN_MVFACMI_A, model_m32rx_mvfacmi_a, { { (int) UNIT_M32RX_U_EXEC, 1, 2 } } },
  { M32RXF_INSN_MVFC, model_m32rx_mvfc, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_MVTACHI_A, model_m32rx_mvtachi_a, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_MVTACLO_A, model_m32rx_mvtaclo_a, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_MVTC, model_m32rx_mvtc, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_NEG, model_m32rx_neg, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_NOP, model_m32rx_nop, { { (int) UNIT_M32RX_U_EXEC, 1, 0 } } },
  { M32RXF_INSN_NOT, model_m32rx_not, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_RAC_DSI, model_m32rx_rac_dsi, { { (int) UNIT_M32RX_U_MAC, 1, 1 } } },
  { M32RXF_INSN_RACH_DSI, model_m32rx_rach_dsi, { { (int) UNIT_M32RX_U_MAC, 1, 1 } } },
  { M32RXF_INSN_RTE, model_m32rx_rte, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_SETH, model_m32rx_seth, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_SLL, model_m32rx_sll, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_SLL3, model_m32rx_sll3, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_SLLI, model_m32rx_slli, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_SRA, model_m32rx_sra, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_SRA3, model_m32rx_sra3, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_SRAI, model_m32rx_srai, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_SRL, model_m32rx_srl, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_SRL3, model_m32rx_srl3, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_SRLI, model_m32rx_srli, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_ST, model_m32rx_st, { { (int) UNIT_M32RX_U_STORE, 1, 1 } } },
  { M32RXF_INSN_ST_D, model_m32rx_st_d, { { (int) UNIT_M32RX_U_STORE, 1, 2 } } },
  { M32RXF_INSN_STB, model_m32rx_stb, { { (int) UNIT_M32RX_U_STORE, 1, 1 } } },
  { M32RXF_INSN_STB_D, model_m32rx_stb_d, { { (int) UNIT_M32RX_U_STORE, 1, 2 } } },
  { M32RXF_INSN_STH, model_m32rx_sth, { { (int) UNIT_M32RX_U_STORE, 1, 1 } } },
  { M32RXF_INSN_STH_D, model_m32rx_sth_d, { { (int) UNIT_M32RX_U_STORE, 1, 2 } } },
  { M32RXF_INSN_ST_PLUS, model_m32rx_st_plus, { { (int) UNIT_M32RX_U_STORE, 1, 1 }, { (int) UNIT_M32RX_U_EXEC, 1, 0 } } },
  { M32RXF_INSN_STH_PLUS, model_m32rx_sth_plus, { { (int) UNIT_M32RX_U_STORE, 1, 1 }, { (int) UNIT_M32RX_U_EXEC, 1, 0 } } },
  { M32RXF_INSN_STB_PLUS, model_m32rx_stb_plus, { { (int) UNIT_M32RX_U_STORE, 1, 1 }, { (int) UNIT_M32RX_U_EXEC, 1, 0 } } },
  { M32RXF_INSN_ST_MINUS, model_m32rx_st_minus, { { (int) UNIT_M32RX_U_STORE, 1, 1 }, { (int) UNIT_M32RX_U_EXEC, 1, 0 } } },
  { M32RXF_INSN_SUB, model_m32rx_sub, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_SUBV, model_m32rx_subv, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_SUBX, model_m32rx_subx, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_TRAP, model_m32rx_trap, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_UNLOCK, model_m32rx_unlock, { { (int) UNIT_M32RX_U_LOAD, 1, 1 } } },
  { M32RXF_INSN_SATB, model_m32rx_satb, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_SATH, model_m32rx_sath, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_SAT, model_m32rx_sat, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_PCMPBZ, model_m32rx_pcmpbz, { { (int) UNIT_M32RX_U_CMP, 1, 1 } } },
  { M32RXF_INSN_SADD, model_m32rx_sadd, { { (int) UNIT_M32RX_U_MAC, 1, 1 } } },
  { M32RXF_INSN_MACWU1, model_m32rx_macwu1, { { (int) UNIT_M32RX_U_MAC, 1, 1 } } },
  { M32RXF_INSN_MSBLO, model_m32rx_msblo, { { (int) UNIT_M32RX_U_MAC, 1, 1 } } },
  { M32RXF_INSN_MULWU1, model_m32rx_mulwu1, { { (int) UNIT_M32RX_U_MAC, 1, 1 } } },
  { M32RXF_INSN_MACLH1, model_m32rx_maclh1, { { (int) UNIT_M32RX_U_MAC, 1, 1 } } },
  { M32RXF_INSN_SC, model_m32rx_sc, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_SNC, model_m32rx_snc, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_CLRPSW, model_m32rx_clrpsw, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_SETPSW, model_m32rx_setpsw, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_BSET, model_m32rx_bset, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_BCLR, model_m32rx_bclr, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
  { M32RXF_INSN_BTST, model_m32rx_btst, { { (int) UNIT_M32RX_U_EXEC, 1, 1 } } },
};

#endif /* WITH_PROFILE_MODEL_P */

static void
m32rx_model_init (SIM_CPU *cpu)
{
  CPU_MODEL_DATA (cpu) = (void *) zalloc (sizeof (MODEL_M32RX_DATA));
}

#if WITH_PROFILE_MODEL_P
#define TIMING_DATA(td) td
#else
#define TIMING_DATA(td) 0
#endif

static const MODEL m32rx_models[] =
{
  { "m32rx", & m32rx_mach, MODEL_M32RX, TIMING_DATA (& m32rx_timing[0]), m32rx_model_init },
  { 0 }
};

/* The properties of this cpu's implementation.  */

static const MACH_IMP_PROPERTIES m32rxf_imp_properties =
{
  sizeof (SIM_CPU),
#if WITH_SCACHE
  sizeof (SCACHE)
#else
  0
#endif
};


static void
m32rxf_prepare_run (SIM_CPU *cpu)
{
  if (CPU_IDESC (cpu) == NULL)
    m32rxf_init_idesc_table (cpu);
}

static const CGEN_INSN *
m32rxf_get_idata (SIM_CPU *cpu, int inum)
{
  return CPU_IDESC (cpu) [inum].idata;
}

static void
m32rx_init_cpu (SIM_CPU *cpu)
{
  CPU_REG_FETCH (cpu) = m32rxf_fetch_register;
  CPU_REG_STORE (cpu) = m32rxf_store_register;
  CPU_PC_FETCH (cpu) = m32rxf_h_pc_get;
  CPU_PC_STORE (cpu) = m32rxf_h_pc_set;
  CPU_GET_IDATA (cpu) = m32rxf_get_idata;
  CPU_MAX_INSNS (cpu) = M32RXF_INSN__MAX;
  CPU_INSN_NAME (cpu) = cgen_insn_name;
  CPU_FULL_ENGINE_FN (cpu) = m32rxf_engine_run_full;
#if WITH_FAST
  CPU_FAST_ENGINE_FN (cpu) = m32rxf_engine_run_fast;
#else
  CPU_FAST_ENGINE_FN (cpu) = m32rxf_engine_run_full;
#endif
}

const MACH m32rx_mach =
{
  "m32rx", "m32rx", MACH_M32RX,
  32, 32, & m32rx_models[0], & m32rxf_imp_properties,
  m32rx_init_cpu,
  m32rxf_prepare_run
};

