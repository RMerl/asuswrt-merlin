/* Simulator tracing/debugging support.
   Copyright (C) 1997, 1998, 2001, 2007 Free Software Foundation, Inc.
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

/* This file is meant to be included by sim-basics.h.  */

#ifndef SIM_TRACE_H
#define SIM_TRACE_H

/* Standard traceable entities.  */

enum {
  /* Trace insn execution.  */
  TRACE_INSN_IDX = 1,

  /* Trace insn decoding.
     ??? This is more of a simulator debugging operation and might best be
     moved to --debug-decode.  */
  TRACE_DECODE_IDX,

  /* Trace insn extraction.
     ??? This is more of a simulator debugging operation and might best be
     moved to --debug-extract.  */
  TRACE_EXTRACT_IDX,

  /* Trace insn execution but include line numbers.  */
  TRACE_LINENUM_IDX,

  /* Trace memory operations.
     The difference between this and TRACE_CORE_IDX is (I think) that this
     is intended to apply to a higher level.  TRACE_CORE_IDX applies to the
     low level core operations.  */
  TRACE_MEMORY_IDX,

  /* Include model performance data in tracing output.  */
  TRACE_MODEL_IDX,

  /* Trace ALU operations.  */
  TRACE_ALU_IDX,

  /* Trace memory core operations.  */
  TRACE_CORE_IDX,

  /* Trace events.  */
  TRACE_EVENTS_IDX,

  /* Trace fpu operations.  */
  TRACE_FPU_IDX,

  /* Trace vpu operations.  */
  TRACE_VPU_IDX,

  /* Trace branching.  */
  TRACE_BRANCH_IDX,

  /* Add information useful for debugging the simulator to trace output.  */
  TRACE_DEBUG_IDX,

  /* Simulator specific trace bits begin here.  */
  TRACE_NEXT_IDX,

};
/* Maximum number of traceable entities.  */
#ifndef MAX_TRACE_VALUES
#define MAX_TRACE_VALUES 32
#endif

/* The -t option only prints useful values.  It's easy to type and shouldn't
   splat on the screen everything under the sun making nothing easy to
   find.  */
#define TRACE_USEFUL_MASK \
((1 << TRACE_INSN_IDX) \
 | (1 << TRACE_LINENUM_IDX) \
 | (1 << TRACE_MEMORY_IDX) \
 | (1 << TRACE_MODEL_IDX))

/* Masks so WITH_TRACE can have symbolic values.
   The case choice here is on purpose.  The lowercase parts are args to
   --with-trace.  */
#define TRACE_insn     (1 << TRACE_INSN_IDX)
#define TRACE_decode   (1 << TRACE_DECODE_IDX)
#define TRACE_extract  (1 << TRACE_EXTRACT_IDX)
#define TRACE_linenum  (1 << TRACE_LINENUM_IDX)
#define TRACE_memory   (1 << TRACE_MEMORY_IDX)
#define TRACE_model    (1 << TRACE_MODEL_IDX)
#define TRACE_alu      (1 << TRACE_ALU_IDX)
#define TRACE_core     (1 << TRACE_CORE_IDX)
#define TRACE_events   (1 << TRACE_EVENTS_IDX)
#define TRACE_fpu      (1 << TRACE_FPU_IDX)
#define TRACE_vpu      (1 << TRACE_VPU_IDX)
#define TRACE_branch   (1 << TRACE_BRANCH_IDX)
#define TRACE_debug    (1 << TRACE_DEBUG_IDX)

/* Preprocessor macros to simplify tests of WITH_TRACE.  */
#define WITH_TRACE_INSN_P	(WITH_TRACE & TRACE_insn)
#define WITH_TRACE_DECODE_P	(WITH_TRACE & TRACE_decode)
#define WITH_TRACE_EXTRACT_P	(WITH_TRACE & TRACE_extract)
#define WITH_TRACE_LINENUM_P	(WITH_TRACE & TRACE_linenum)
#define WITH_TRACE_MEMORY_P	(WITH_TRACE & TRACE_memory)
#define WITH_TRACE_MODEL_P	(WITH_TRACE & TRACE_model)
#define WITH_TRACE_ALU_P	(WITH_TRACE & TRACE_alu)
#define WITH_TRACE_CORE_P	(WITH_TRACE & TRACE_core)
#define WITH_TRACE_EVENTS_P	(WITH_TRACE & TRACE_events)
#define WITH_TRACE_FPU_P	(WITH_TRACE & TRACE_fpu)
#define WITH_TRACE_VPU_P	(WITH_TRACE & TRACE_vpu)
#define WITH_TRACE_BRANCH_P	(WITH_TRACE & TRACE_branch)
#define WITH_TRACE_DEBUG_P	(WITH_TRACE & TRACE_debug)

/* Tracing install handler.  */
MODULE_INSTALL_FN trace_install;

/* Struct containing all system and cpu trace data.

   System trace data is stored with the associated module.
   System and cpu tracing must share the same space of bitmasks as they
   are arguments to --with-trace.  One could have --with-trace and
   --with-cpu-trace or some such but that's an over-complication at this point
   in time.  Also, there may be occasions where system and cpu tracing may
   wish to share a name.  */

typedef struct _trace_data {

  /* Global summary of all the current trace options */
  char trace_any_p;

  /* Boolean array of specified tracing flags.  */
  /* ??? It's not clear that using an array vs a bit mask is faster.
     Consider the case where one wants to test whether any of several bits
     are set.  */
  char trace_flags[MAX_TRACE_VALUES];
#define TRACE_FLAGS(t) ((t)->trace_flags)

  /* Tracing output goes to this or stderr if NULL.
     We can't store `stderr' here as stderr goes through a callback.  */
  FILE *trace_file;
#define TRACE_FILE(t) ((t)->trace_file)

  /* Buffer to store the prefix to be printed before any trace line.  */
  char trace_prefix[256];
#define TRACE_PREFIX(t) ((t)->trace_prefix)

  /* Buffer to save the inputs for the current instruction.  Use a
     union to force the buffer into correct alignment */
  union {
    unsigned8 i8;
    unsigned16 i16;
    unsigned32 i32;
    unsigned64 i64;
  } trace_input_data[16];
  unsigned8 trace_input_fmt[16];
  unsigned8 trace_input_size[16];
  int trace_input_idx;
#define TRACE_INPUT_DATA(t) ((t)->trace_input_data)
#define TRACE_INPUT_FMT(t) ((t)->trace_input_fmt)
#define TRACE_INPUT_SIZE(t) ((t)->trace_input_size)
#define TRACE_INPUT_IDX(t) ((t)->trace_input_idx)

  /* Category of trace being performed */
  int trace_idx;
#define TRACE_IDX(t) ((t)->trace_idx)

  /* Trace range.
     ??? Not all cpu's support this.  */
  ADDR_RANGE range;
#define TRACE_RANGE(t) (& (t)->range)
} TRACE_DATA;

/* System tracing support.  */

#define STATE_TRACE_FLAGS(sd) TRACE_FLAGS (STATE_TRACE_DATA (sd))

/* Return non-zero if tracing of IDX is enabled for non-cpu specific
   components.  The "S" in "STRACE" refers to "System".  */
#define STRACE_P(sd,idx) \
((WITH_TRACE & (1 << (idx))) != 0 \
 && STATE_TRACE_FLAGS (sd)[idx] != 0)

/* Non-zero if --trace-<xxxx> was specified for SD.  */
#define STRACE_DEBUG_P(sd)	STRACE_P (sd, TRACE_DEBUG_IDX)

/* CPU tracing support.  */

#define CPU_TRACE_FLAGS(cpu) TRACE_FLAGS (CPU_TRACE_DATA (cpu))

/* Return non-zero if tracing of IDX is enabled for CPU.  */
#define TRACE_P(cpu,idx) \
((WITH_TRACE & (1 << (idx))) != 0 \
 && CPU_TRACE_FLAGS (cpu)[idx] != 0)

/* Non-zero if --trace-<xxxx> was specified for CPU.  */
#define TRACE_ANY_P(cpu)	((WITH_TRACE) && (CPU_TRACE_DATA (cpu)->trace_any_p))
#define TRACE_INSN_P(cpu)	TRACE_P (cpu, TRACE_INSN_IDX)
#define TRACE_DECODE_P(cpu)	TRACE_P (cpu, TRACE_DECODE_IDX)
#define TRACE_EXTRACT_P(cpu)	TRACE_P (cpu, TRACE_EXTRACT_IDX)
#define TRACE_LINENUM_P(cpu)	TRACE_P (cpu, TRACE_LINENUM_IDX)
#define TRACE_MEMORY_P(cpu)	TRACE_P (cpu, TRACE_MEMORY_IDX)
#define TRACE_MODEL_P(cpu)	TRACE_P (cpu, TRACE_MODEL_IDX)
#define TRACE_ALU_P(cpu)	TRACE_P (cpu, TRACE_ALU_IDX)
#define TRACE_CORE_P(cpu)	TRACE_P (cpu, TRACE_CORE_IDX)
#define TRACE_EVENTS_P(cpu)	TRACE_P (cpu, TRACE_EVENTS_IDX)
#define TRACE_FPU_P(cpu)	TRACE_P (cpu, TRACE_FPU_IDX)
#define TRACE_VPU_P(cpu)	TRACE_P (cpu, TRACE_VPU_IDX)
#define TRACE_BRANCH_P(cpu)	TRACE_P (cpu, TRACE_BRANCH_IDX)
#define TRACE_DEBUG_P(cpu)	TRACE_P (cpu, TRACE_DEBUG_IDX)

/* Tracing functions.  */

/* Prime the trace buffers ready for any trace output.
   Must be called prior to any other trace operation */
extern void trace_prefix PARAMS ((SIM_DESC sd,
				  sim_cpu *cpu,
				  sim_cia cia,
				  address_word pc,
				  int print_linenum_p,
				  const char *file_name,
				  int line_nr,
				  const char *fmt,
				  ...))
       __attribute__((format (printf, 8, 9)));

/* Generic trace print, assumes trace_prefix() has been called */

extern void trace_generic PARAMS ((SIM_DESC sd,
				   sim_cpu *cpu,
				   int trace_idx,
				   const char *fmt,
				   ...))
     __attribute__((format (printf, 4, 5)));

/* Trace a varying number of word sized inputs/outputs.  trace_result*
   must be called to close the trace operation. */

extern void trace_input0 PARAMS ((SIM_DESC sd,
				  sim_cpu *cpu,
				  int trace_idx));

extern void trace_input_word1 PARAMS ((SIM_DESC sd,
				       sim_cpu *cpu,
				       int trace_idx,
				       unsigned_word d0));

extern void trace_input_word2 PARAMS ((SIM_DESC sd,
				       sim_cpu *cpu,
				       int trace_idx,
				       unsigned_word d0,
				       unsigned_word d1));

extern void trace_input_word3 PARAMS ((SIM_DESC sd,
				       sim_cpu *cpu,
				       int trace_idx,
				       unsigned_word d0,
				       unsigned_word d1,
				       unsigned_word d2));

extern void trace_input_word4 PARAMS ((SIM_DESC sd,
				       sim_cpu *cpu,
				       int trace_idx,
				       unsigned_word d0,
				       unsigned_word d1,
				       unsigned_word d2,
				       unsigned_word d3));

extern void trace_input_addr1 PARAMS ((SIM_DESC sd,
				       sim_cpu *cpu,
				       int trace_idx,
				       address_word d0));

extern void trace_input_bool1 PARAMS ((SIM_DESC sd,
				       sim_cpu *cpu,
				       int trace_idx,
				       int d0));

extern void trace_input_fp1 PARAMS ((SIM_DESC sd,
				     sim_cpu *cpu,
				     int trace_idx,
				     fp_word f0));

extern void trace_input_fp2 PARAMS ((SIM_DESC sd,
				     sim_cpu *cpu,
				     int trace_idx,
				     fp_word f0,
				     fp_word f1));

extern void trace_input_fp3 PARAMS ((SIM_DESC sd,
				     sim_cpu *cpu,
				     int trace_idx,
				     fp_word f0,
				     fp_word f1,
				     fp_word f2));

extern void trace_input_fpu1 PARAMS ((SIM_DESC sd,
				     sim_cpu *cpu,
				     int trace_idx,
				     struct _sim_fpu *f0));

extern void trace_input_fpu2 PARAMS ((SIM_DESC sd,
				     sim_cpu *cpu,
				     int trace_idx,
				     struct _sim_fpu *f0,
				     struct _sim_fpu *f1));

extern void trace_input_fpu3 PARAMS ((SIM_DESC sd,
				     sim_cpu *cpu,
				     int trace_idx,
				     struct _sim_fpu *f0,
				     struct _sim_fpu *f1,
				     struct _sim_fpu *f2));

/* Other trace_input{_<fmt><nr-inputs>} functions can go here */

extern void trace_result0 PARAMS ((SIM_DESC sd,
				   sim_cpu *cpu,
				   int trace_idx));

extern void trace_result_word1 PARAMS ((SIM_DESC sd,
					sim_cpu *cpu,
					int trace_idx,
					unsigned_word r0));

extern void trace_result_word2 PARAMS ((SIM_DESC sd,
					sim_cpu *cpu,
					int trace_idx,
					unsigned_word r0,
					unsigned_word r1));

extern void trace_result_word4 PARAMS ((SIM_DESC sd,
					sim_cpu *cpu,
					int trace_idx,
					unsigned_word r0,
					unsigned_word r1,
					unsigned_word r2,
					unsigned_word r3));

extern void trace_result_bool1 PARAMS ((SIM_DESC sd,
					sim_cpu *cpu,
					int trace_idx,
					int r0));

extern void trace_result_addr1 PARAMS ((SIM_DESC sd,
					sim_cpu *cpu,
					int trace_idx,
					address_word r0));

extern void trace_result_fp1 PARAMS ((SIM_DESC sd,
				      sim_cpu *cpu,
				      int trace_idx,
				      fp_word f0));

extern void trace_result_fp2 PARAMS ((SIM_DESC sd,
				      sim_cpu *cpu,
				      int trace_idx,
				      fp_word f0,
				      fp_word f1));

extern void trace_result_fpu1 PARAMS ((SIM_DESC sd,
				       sim_cpu *cpu,
				       int trace_idx,
				       struct _sim_fpu *f0));

extern void trace_result_string1 PARAMS ((SIM_DESC sd,
					  sim_cpu *cpu,
					  int trace_idx,
					  char *str0));

extern void trace_result_word1_string1 PARAMS ((SIM_DESC sd,
						sim_cpu *cpu,
						int trace_idx,
						unsigned_word r0,
						char *s0));

/* Other trace_result{_<type><nr-results>} */


/* Macros for tracing ALU instructions */

#define TRACE_ALU_INPUT0() \
do { \
  if (TRACE_ALU_P (CPU)) \
    trace_input0 (SD, CPU, TRACE_ALU_IDX); \
} while (0)
    
#define TRACE_ALU_INPUT1(V0) \
do { \
  if (TRACE_ALU_P (CPU)) \
    trace_input_word1 (SD, CPU, TRACE_ALU_IDX, (V0)); \
} while (0)

#define TRACE_ALU_INPUT2(V0,V1) \
do { \
  if (TRACE_ALU_P (CPU)) \
    trace_input_word2 (SD, CPU, TRACE_ALU_IDX, (V0), (V1)); \
} while (0)

#define TRACE_ALU_INPUT3(V0,V1,V2) \
do { \
  if (TRACE_ALU_P (CPU)) \
    trace_input_word3 (SD, CPU, TRACE_ALU_IDX, (V0), (V1), (V2)); \
} while (0)

#define TRACE_ALU_INPUT4(V0,V1,V2,V3) \
do { \
  if (TRACE_ALU_P (CPU)) \
    trace_input_word4 (SD, CPU, TRACE_ALU_IDX, (V0), (V1), (V2), (V3)); \
} while (0)

#define TRACE_ALU_RESULT(R0) TRACE_ALU_RESULT1(R0)

#define TRACE_ALU_RESULT0() \
do { \
  if (TRACE_ALU_P (CPU)) \
    trace_result0 (SD, CPU, TRACE_ALU_IDX); \
} while (0)

#define TRACE_ALU_RESULT1(R0) \
do { \
  if (TRACE_ALU_P (CPU)) \
    trace_result_word1 (SD, CPU, TRACE_ALU_IDX, (R0)); \
} while (0)

#define TRACE_ALU_RESULT2(R0,R1) \
do { \
  if (TRACE_ALU_P (CPU)) \
    trace_result_word2 (SD, CPU, TRACE_ALU_IDX, (R0), (R1)); \
} while (0)

#define TRACE_ALU_RESULT4(R0,R1,R2,R3) \
do { \
  if (TRACE_ALU_P (CPU)) \
    trace_result_word4 (SD, CPU, TRACE_ALU_IDX, (R0), (R1), (R2), (R3)); \
} while (0)

/* Macros for tracing inputs to comparative branch instructions. */

#define TRACE_BRANCH_INPUT1(V0) \
do { \
  if (TRACE_BRANCH_P (CPU)) \
    trace_input_word1 (SD, CPU, TRACE_BRANCH_IDX, (V0)); \
} while (0)

#define TRACE_BRANCH_INPUT2(V0,V1) \
do { \
  if (TRACE_BRANCH_P (CPU)) \
    trace_input_word2 (SD, CPU, TRACE_BRANCH_IDX, (V0), (V1)); \
} while (0)

/* Macros for tracing FPU instructions */

#define TRACE_FP_INPUT0() \
do { \
  if (TRACE_FPU_P (CPU)) \
    trace_input0 (SD, CPU, TRACE_FPU_IDX); \
} while (0)
    
#define TRACE_FP_INPUT1(V0) \
do { \
  if (TRACE_FPU_P (CPU)) \
    trace_input_fp1 (SD, CPU, TRACE_FPU_IDX, (V0)); \
} while (0)

#define TRACE_FP_INPUT2(V0,V1) \
do { \
  if (TRACE_FPU_P (CPU)) \
    trace_input_fp2 (SD, CPU, TRACE_FPU_IDX, (V0), (V1)); \
} while (0)

#define TRACE_FP_INPUT3(V0,V1,V2) \
do { \
  if (TRACE_FPU_P (CPU)) \
    trace_input_fp3 (SD, CPU, TRACE_FPU_IDX, (V0), (V1), (V2)); \
} while (0)

#define TRACE_FP_INPUT_WORD1(V0) \
do { \
  if (TRACE_FPU_P (CPU)) \
    trace_input_word1 (SD, CPU, TRACE_FPU_IDX, (V0)); \
} while (0)
    
#define TRACE_FP_RESULT(R0) \
do { \
  if (TRACE_FPU_P (CPU)) \
    trace_result_fp1 (SD, CPU, TRACE_FPU_IDX, (R0)); \
} while (0)

#define TRACE_FP_RESULT2(R0,R1) \
do { \
  if (TRACE_FPU_P (CPU)) \
    trace_result_fp2 (SD, CPU, TRACE_FPU_IDX, (R0), (R1)); \
} while (0)

#define TRACE_FP_RESULT_BOOL(R0) \
do { \
  if (TRACE_FPU_P (CPU)) \
    trace_result_bool1 (SD, CPU, TRACE_FPU_IDX, (R0)); \
} while (0)

#define TRACE_FP_RESULT_WORD(R0) \
do { \
  if (TRACE_FPU_P (CPU)) \
    trace_result_word1 (SD, CPU, TRACE_FPU_IDX, (R0)); \
} while (0)


/* Macros for tracing branches */

#define TRACE_BRANCH_INPUT(COND) \
do { \
  if (TRACE_BRANCH_P (CPU)) \
    trace_input_bool1 (SD, CPU, TRACE_BRANCH_IDX, (COND)); \
} while (0)

#define TRACE_BRANCH_RESULT(DEST) \
do { \
  if (TRACE_BRANCH_P (CPU)) \
    trace_result_addr1 (SD, CPU, TRACE_BRANCH_IDX, (DEST)); \
} while (0)


/* The function trace_one_insn has been replaced by the function pair
   trace_prefix() + trace_generic() */
extern void trace_one_insn PARAMS ((SIM_DESC sd,
				    sim_cpu * cpu,
				    address_word cia,
				    int print_linenum_p,
				    const char *file_name,
				    int line_nr,
				    const char *unit,
				    const char *fmt,
				    ...))
     __attribute__((format (printf, 8, 9)));

extern void trace_printf PARAMS ((SIM_DESC, sim_cpu *, const char *, ...))
     __attribute__((format (printf, 3, 4)));

extern void trace_vprintf PARAMS ((SIM_DESC, sim_cpu *, const char *, va_list));

/* Debug support.
   This is included here because there isn't enough of it to justify
   a sim-debug.h.  */

/* Return non-zero if debugging of IDX for CPU is enabled.  */
#define DEBUG_P(cpu, idx) \
((WITH_DEBUG & (1 << (idx))) != 0 \
 && CPU_DEBUG_FLAGS (cpu)[idx] != 0)

/* Non-zero if "--debug-insn" specified.  */
#define DEBUG_INSN_P(cpu) DEBUG_P (cpu, DEBUG_INSN_IDX)

extern void debug_printf PARAMS ((sim_cpu *, const char *, ...))
     __attribute__((format (printf, 2, 3)));

#endif /* SIM_TRACE_H */
