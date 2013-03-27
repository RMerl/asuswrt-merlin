/* CPU support.
   Copyright (C) 1998, 2007 Free Software Foundation, Inc.
   Contributed by Cygnus Solutions.

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

/* This file is intended to be included by sim-base.h.

   This file provides an interface between the simulator framework and
   the selected cpu.  */

#ifndef SIM_CPU_H
#define SIM_CPU_H

/* Type of function to return an insn name.  */
typedef const char * (CPU_INSN_NAME_FN) (sim_cpu *, int);

/* Types for register access functions.
   These routines implement the sim_{fetch,store}_register interface.  */
typedef int (CPUREG_FETCH_FN) (sim_cpu *, int, unsigned char *, int);
typedef int (CPUREG_STORE_FN) (sim_cpu *, int, unsigned char *, int);

/* Types for PC access functions.
   Some simulators require a functional interface to access the program
   counter [a macro is insufficient as the PC is kept in a cpu-specific part
   of the sim_cpu struct].  */
typedef sim_cia (PC_FETCH_FN) (sim_cpu *);
typedef void (PC_STORE_FN) (sim_cpu *, sim_cia);

/* Pseudo baseclass for each cpu.  */

typedef struct {

  /* Backlink to main state struct.  */
  SIM_DESC state;
#define CPU_STATE(cpu) ((cpu)->base.state)

  /* Processor index within the SD_DESC */
  int index;
#define CPU_INDEX(cpu) ((cpu)->base.index)

  /* The name of the cpu.  */
  const char *name;
#define CPU_NAME(cpu) ((cpu)->base.name)

  /* Options specific to this cpu.  */
  struct option_list *options;
#define CPU_OPTIONS(cpu) ((cpu)->base.options)

  /* Processor specific core data */
  sim_cpu_core core;
#define CPU_CORE(cpu) (& (cpu)->base.core)

  /* Number of instructions (used to iterate over CPU_INSN_NAME).  */
  unsigned int max_insns;
#define CPU_MAX_INSNS(cpu) ((cpu)->base.max_insns)

  /* Function to return the name of an insn.  */
  CPU_INSN_NAME_FN *insn_name;
#define CPU_INSN_NAME(cpu) ((cpu)->base.insn_name)

  /* Trace data.  See sim-trace.h.  */
  TRACE_DATA trace_data;
#define CPU_TRACE_DATA(cpu) (& (cpu)->base.trace_data)

  /* Maximum number of debuggable entities.
     This debugging is not intended for normal use.
     It is only enabled when the simulator is configured with --with-debug
     which shouldn't normally be specified.  */
#ifndef MAX_DEBUG_VALUES
#define MAX_DEBUG_VALUES 4
#endif

  /* Boolean array of specified debugging flags.  */
  char debug_flags[MAX_DEBUG_VALUES];
#define CPU_DEBUG_FLAGS(cpu) ((cpu)->base.debug_flags)
  /* Standard values.  */
#define DEBUG_INSN_IDX 0
#define DEBUG_NEXT_IDX 2 /* simulator specific debug bits begin here */

  /* Debugging output goes to this or stderr if NULL.
     We can't store `stderr' here as stderr goes through a callback.  */
  FILE *debug_file;
#define CPU_DEBUG_FILE(cpu) ((cpu)->base.debug_file)

  /* Profile data.  See sim-profile.h.  */
  PROFILE_DATA profile_data;
#define CPU_PROFILE_DATA(cpu) (& (cpu)->base.profile_data)

#ifdef SIM_HAVE_MODEL
  /* Machine tables for this cpu.  See sim-model.h.  */
  const MACH *mach;
#define CPU_MACH(cpu) ((cpu)->base.mach)
  /* The selected model.  */
  const MODEL *model;
#define CPU_MODEL(cpu) ((cpu)->base.model)
  /* Model data (profiling state, etc.).  */
  void *model_data;
#define CPU_MODEL_DATA(cpu) ((cpu)->base.model_data)
#endif

  /* Routines to fetch/store registers.  */
  CPUREG_FETCH_FN *reg_fetch;
#define CPU_REG_FETCH(c) ((c)->base.reg_fetch)
  CPUREG_STORE_FN *reg_store;
#define CPU_REG_STORE(c) ((c)->base.reg_store)
  PC_FETCH_FN *pc_fetch;
#define CPU_PC_FETCH(c) ((c)->base.pc_fetch)
  PC_STORE_FN *pc_store;
#define CPU_PC_STORE(c) ((c)->base.pc_store)

} sim_cpu_base;

/* Create all cpus.  */
extern SIM_RC sim_cpu_alloc_all (SIM_DESC, int, int);
/* Create a cpu.  */
extern sim_cpu *sim_cpu_alloc (SIM_DESC, int);
/* Release resources held by all cpus.  */
extern void sim_cpu_free_all (SIM_DESC);
/* Release resources held by a cpu.  */
extern void sim_cpu_free (sim_cpu *);

/* Return a pointer to the cpu data for CPU_NAME, or NULL if not found.  */
extern sim_cpu *sim_cpu_lookup (SIM_DESC, const char *);

/* Return prefix to use in cpu specific messages.  */
extern const char *sim_cpu_msg_prefix (sim_cpu *);
/* Cover fn to sim_io_eprintf.  */
extern void sim_io_eprintf_cpu (sim_cpu *, const char *, ...);

/* Get/set a pc value.  */
#define CPU_PC_GET(cpu) ((* CPU_PC_FETCH (cpu)) (cpu))
#define CPU_PC_SET(cpu,newval) ((* CPU_PC_STORE (cpu)) ((cpu), (newval)))
/* External interface to accessing the pc.  */
sim_cia sim_pc_get (sim_cpu *);
void sim_pc_set (sim_cpu *, sim_cia);

#endif /* SIM_CPU_H */
