/* Simulator header for cgen cpus.
   Copyright (C) 1998, 1999, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

This file is part of GDB, the GNU debugger.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef CGEN_CPU_H
#define CGEN_CPU_H

/* Type of function that is ultimately called by sim_resume.  */
typedef void (ENGINE_FN) (SIM_CPU *);

/* Type of function to do disassembly.  */
typedef void (CGEN_DISASSEMBLER) (SIM_CPU *, const CGEN_INSN *,
				  const ARGBUF *, IADDR pc_, char *buf_);

/* Additional non-machine generated per-cpu data to go in SIM_CPU.
   The member's name must be `cgen_cpu'.  */

typedef struct {
  /* Non-zero while cpu simulation is running.  */
  int running_p;
#define CPU_RUNNING_P(cpu) ((cpu)->cgen_cpu.running_p)

  /* Instruction count.  This is maintained even in fast mode to keep track
     of simulator speed.  */
  unsigned long insn_count;
#define CPU_INSN_COUNT(cpu) ((cpu)->cgen_cpu.insn_count)

  /* sim_resume handlers */
  ENGINE_FN *fast_engine_fn;
#define CPU_FAST_ENGINE_FN(cpu) ((cpu)->cgen_cpu.fast_engine_fn)
  ENGINE_FN *full_engine_fn;
#define CPU_FULL_ENGINE_FN(cpu) ((cpu)->cgen_cpu.full_engine_fn)

  /* Maximum number of instructions per time slice.
     When single stepping this is 1.  If using the pbb model, this can be
     more than 1.  0 means "as long as you want".  */
  unsigned int max_slice_insns;
#define CPU_MAX_SLICE_INSNS(cpu) ((cpu)->cgen_cpu.max_slice_insns)

  /* Simulator's execution cache.
     Allocate space for this even if not used as some simulators may have
     one machine variant that uses the scache and another that doesn't and
     we don't want members in this struct to move about.  */
  CPU_SCACHE scache;

  /* Instruction descriptor table.  */
  IDESC *idesc;
#define CPU_IDESC(cpu) ((cpu)->cgen_cpu.idesc)

  /* Whether the read,write,semantic entries (function pointers or computed
     goto labels) have been initialized or not.  */
  int idesc_read_init_p;
#define CPU_IDESC_READ_INIT_P(cpu) ((cpu)->cgen_cpu.idesc_read_init_p)
  int idesc_write_init_p;
#define CPU_IDESC_WRITE_INIT_P(cpu) ((cpu)->cgen_cpu.idesc_write_init_p)
  int idesc_sem_init_p;
#define CPU_IDESC_SEM_INIT_P(cpu) ((cpu)->cgen_cpu.idesc_sem_init_p)

  /* Cpu descriptor table.
     This is a CGEN created entity that contains the description file
     turned into C code and tables for our use.  */
  CGEN_CPU_DESC cpu_desc;
#define CPU_CPU_DESC(cpu) ((cpu)->cgen_cpu.cpu_desc)

  /* Function to fetch the insn data entry in the IDESC.  */
  const CGEN_INSN * (*get_idata) (SIM_CPU *, int);
#define CPU_GET_IDATA(cpu) ((cpu)->cgen_cpu.get_idata)

  /* Floating point support.  */
  CGEN_FPU fpu;
#define CGEN_CPU_FPU(cpu) (& (cpu)->cgen_cpu.fpu)

  /* Disassembler.  */
  CGEN_DISASSEMBLER *disassembler;
#define CPU_DISASSEMBLER(cpu) ((cpu)->cgen_cpu.disassembler)

  /* Queued writes for parallel write-after support.  */
  CGEN_WRITE_QUEUE write_queue;
#define CPU_WRITE_QUEUE(cpu) (& (cpu)->cgen_cpu.write_queue)

  /* Allow slop in size calcs for case where multiple cpu types are supported
     and space for the specified cpu is malloc'd at run time.  */
  double slop;
} CGEN_CPU;

/* Shorthand macro for fetching registers.
   CPU_CGEN_HW is defined in cpu.h.  */
#define CPU(x) (CPU_CGEN_HW (current_cpu)->x)

#endif /* CGEN_CPU_H */
