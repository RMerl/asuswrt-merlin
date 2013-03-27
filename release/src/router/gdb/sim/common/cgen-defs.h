/* General Cpu tools GENerated simulator support.
   Copyright (C) 1996, 1997, 1998, 1999, 2007 Free Software Foundation, Inc.
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

#ifndef CGEN_DEFS_H
#define CGEN_DEFS_H

/* Compute number of longs required to hold N bits.  */
#define HOST_LONGS_FOR_BITS(n) \
  (((n) + sizeof (long) * 8 - 1) / sizeof (long) * 8)

/* Forward decls.  Defined in the machine generated files.  */

/* This holds the contents of the extracted insn.
   There are a few common entries (e.g. pc address), and then one big
   union with an entry for each of the instruction formats.  */
typedef struct argbuf ARGBUF;

/* ARGBUF accessors.  */
#define ARGBUF_ADDR(abuf) ((abuf)->addr)
#define ARGBUF_IDESC(abuf) ((abuf)->idesc)
#define ARGBUF_TRACE_P(abuf) ((abuf)->trace_p)
#define ARGBUF_PROFILE_P(abuf) ((abuf)->profile_p)

/* This is one ARGBUF plus whatever else is needed for WITH_SCACHE support.
   At present there is nothing else, but it also provides a level of
   abstraction.  */
typedef struct scache SCACHE;

/* This is a union with one entry for each instruction format.
   Each entry contains all of the non-constant inputs of the instruction
   in the case of read-before-exec support, or all outputs of the instruction
   in the case of write-after-exec support.  */
typedef struct parexec PAREXEC;

/* An "Instruction DESCriptor".
   This is the main handle on an instruction for the simulator.  */
typedef struct idesc IDESC;

/* Engine support.
   ??? This is here because it's needed before eng.h (built by genmloop.sh)
   which is needed before cgen-engine.h and cpu.h.
   ??? This depends on a cpu family specific type, IADDR, but no machine
   generated headers will have been included yet.  sim/common currently
   requires the typedef of sim_cia in sim-main.h between the inclusion of
   sim-basics.h and sim-base.h so this is no different.  */

/* SEM_ARG is intended to hide whether or not the scache is in use from the
   semantic routines.  In reality for the with-extraction case it is always
   an SCACHE * even when not using the SCACHE since there's no current win to
   making it something else ("not using the SCACHE" is like having a cache
   size of 1).
   The without-extraction case still uses an ARGBUF:
   - consistency with scache version
   - still need to record which operands are written
     This wouldn't be needed if modeling was done in the semantic routines
     but this isn't as general as handling it outside of the semantic routines.
     For example Shade allows calling user-supplied code before/after each
     instruction and this is something that is being planned.
   ??? There is still some clumsiness in how much of ARGBUF to use.  */
typedef SCACHE *SEM_ARG;

/* instruction address
   ??? This was intended to be a struct of two elements in the WITH_SCACHE_PBB
   case.  The first element is the IADDR, the second element is the SCACHE *.
   Haven't found the time yet to make this work, but it seemed a nicer approach
   than the current br_cache stuff.  */
typedef IADDR PCADDR;

/* Current instruction address, used by common. */
typedef IADDR CIA;

/* Semantic routines' version of the PC.  */
#if WITH_SCACHE_PBB
typedef SCACHE *SEM_PC;
#else
typedef IADDR SEM_PC;
#endif

/* Kinds of branches.  */
typedef enum {
  SEM_BRANCH_UNTAKEN,
  /* Branch to an uncacheable address (e.g. j reg).  */
  SEM_BRANCH_UNCACHEABLE,
  /* Branch to a cacheable (fixed) address.  */
  SEM_BRANCH_CACHEABLE
} SEM_BRANCH_TYPE;

/* Virtual insn support.  */

/* Opcode table for virtual insns (only used by the simulator).  */
extern const CGEN_INSN cgen_virtual_insn_table[];

/* -ve of indices of virtual insns in cgen_virtual_insn_table.  */
typedef enum {
  VIRTUAL_INSN_X_INVALID = 0,
  VIRTUAL_INSN_X_BEFORE = -1, VIRTUAL_INSN_X_AFTER = -2,
  VIRTUAL_INSN_X_BEGIN = -3,
  VIRTUAL_INSN_X_CHAIN= -4, VIRTUAL_INSN_X_CTI_CHAIN = -5
} CGEN_INSN_VIRTUAL_TYPE;

/* Return non-zero if CGEN_INSN* INSN is a virtual insn.  */
#define CGEN_INSN_VIRTUAL_P(insn) \
  CGEN_INSN_ATTR_VALUE ((insn), CGEN_INSN_VIRTUAL)

/* GNU C's "computed goto" facility is used to speed things up where
   possible.  These macros provide a portable way to use them.
   Nesting of these switch statements is done by providing an extra argument
   that distinguishes them.  `N' can be a number or symbol.
   Variable `labels_##N' must be initialized with the labels of each case.  */

#ifdef __GNUC__
#define SWITCH(N, X) goto *X;
#define CASE(N, X) case_##N##_##X
#define BREAK(N) goto end_switch_##N
#define DEFAULT(N) default_##N
#define ENDSWITCH(N) end_switch_##N:;
#else
#define SWITCH(N, X) switch (X)
#define CASE(N, X) case X /* FIXME: old sem-switch had (@arch@_,X) here */
#define BREAK(N) break
#define DEFAULT(N) default
#define ENDSWITCH(N)
#endif

/* Simulator state.  */

/* Records simulator descriptor so utilities like @cpu@_dump_regs can be
   called from gdb.  */
extern SIM_DESC current_state;

/* Simulator state.  */

/* CGEN_STATE contains additional state information not present in
   sim_state_base.  */

typedef struct cgen_state {
  /* FIXME: Moved to sim_state_base.  */
  /* argv, env */
  char **argv;
#define STATE_ARGV(s) ((s) -> cgen_state.argv)
  /* FIXME: Move to sim_state_base.  */
  char **envp;
#define STATE_ENVP(s) ((s) -> cgen_state.envp)

  /* Non-zero if no tracing or profiling is selected.  */
  int run_fast_p;
#define STATE_RUN_FAST_P(sd) ((sd) -> cgen_state.run_fast_p)
} CGEN_STATE;

/* Various utilities.  */

/* Called after sim_post_argv_init to do any cgen initialization.  */
extern void cgen_init (SIM_DESC);

/* Return the name of an insn.  */
extern CPU_INSN_NAME_FN cgen_insn_name;

/* Return the maximum number of extra bytes required for a sim_cpu struct.  */
/* ??? Ok, yes, this is less pretty than it should be.  Give me a better
   language [or suggest a better way].  */
extern int cgen_cpu_max_extra_bytes (void);

/* Target supplied routine to process an invalid instruction.  */
extern SEM_PC sim_engine_invalid_insn (SIM_CPU *, IADDR, SEM_PC);

#endif /* CGEN_DEFS_H */
