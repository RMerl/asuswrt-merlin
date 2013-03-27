/* Profile header for simulators using common framework.
   Copyright (C) 1996, 1997, 1998, 2007 Free Software Foundation, Inc.
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

#ifndef SIM_PROFILE_H
#define SIM_PROFILE_H

#ifndef WITH_PROFILE
Error, WITH_PROFILE not defined.
#endif

/* Standard profilable entities.  */

enum {
  /* Profile insn usage.  */
  PROFILE_INSN_IDX = 1,

  /* Profile memory usage.  */
  PROFILE_MEMORY_IDX,

  /* Profile the cpu model (cycles, etc.).  */
  PROFILE_MODEL_IDX,

  /* Profile the simulator's execution cache.  */
  PROFILE_SCACHE_IDX,

  /* Profile the PC.  */
  PROFILE_PC_IDX,

  /* Profile sim-core.c stuff.  */
  /* ??? The difference between this and PROFILE_MEMORY_IDX is ... ?  */
  PROFILE_CORE_IDX,

  /* Simulator specific profile bits begin here.  */
  PROFILE_NEXT_IDX
};

/* Maximum number of profilable entities.  */
#ifndef MAX_PROFILE_VALUES
#define MAX_PROFILE_VALUES 32
#endif

/* The -p option only prints useful values.  It's easy to type and shouldn't
   splat on the screen everything under the sun making nothing easy to
   find.  */
#define PROFILE_USEFUL_MASK \
((1 << PROFILE_INSN_IDX) \
 | (1 << PROFILE_MEMORY_IDX) \
 | (1 << PROFILE_MODEL_IDX) \
 | (1 << PROFILE_CORE_IDX))

/* Utility to set profile options.  */
SIM_RC set_profile_option_mask (SIM_DESC sd_, const char *name_, int mask_,
				const char *arg_);

/* Utility to parse a --profile-<foo> option.  */
/* ??? On the one hand all calls could be confined to sim-profile.c, but
   on the other hand keeping a module's profiling option with the module's
   source is cleaner.   */

SIM_RC sim_profile_set_option (SIM_DESC sd_, const char *name_, int idx_,
			       const char *arg_);

/* Masks so WITH_PROFILE can have symbolic values.
   The case choice here is on purpose.  The lowercase parts are args to
   --with-profile.  */
#define PROFILE_insn   (1 << PROFILE_INSN_IDX)
#define PROFILE_memory (1 << PROFILE_MEMORY_IDX)
#define PROFILE_model  (1 << PROFILE_MODEL_IDX)
#define PROFILE_scache (1 << PROFILE_SCACHE_IDX)
#define PROFILE_pc     (1 << PROFILE_PC_IDX)
#define PROFILE_core   (1 << PROFILE_CORE_IDX)

/* Preprocessor macros to simplify tests of WITH_PROFILE.  */
#define WITH_PROFILE_INSN_P (WITH_PROFILE & PROFILE_insn)
#define WITH_PROFILE_MEMORY_P (WITH_PROFILE & PROFILE_memory)
#define WITH_PROFILE_MODEL_P (WITH_PROFILE & PROFILE_model)
#define WITH_PROFILE_SCACHE_P (WITH_PROFILE & PROFILE_scache)
#define WITH_PROFILE_PC_P (WITH_PROFILE & PROFILE_pc)
#define WITH_PROFILE_CORE_P (WITH_PROFILE & PROFILE_core)

/* If MAX_TARGET_MODES isn't defined, we can't do memory profiling.
   ??? It is intended that this is a temporary occurrence.  Normally
   MAX_TARGET_MODES is defined.  */
#ifndef MAX_TARGET_MODES
#undef WITH_PROFILE_MEMORY_P
#define WITH_PROFILE_MEMORY_P 0
#endif

/* Only build MODEL code when the target simulator has support for it */
#ifndef SIM_HAVE_MODEL
#undef WITH_PROFILE_MODEL_P
#define WITH_PROFILE_MODEL_P 0
#endif

/* Profiling install handler.  */
MODULE_INSTALL_FN profile_install;

/* Output format macros.  */
#ifndef PROFILE_HISTOGRAM_WIDTH
#define PROFILE_HISTOGRAM_WIDTH 40
#endif
#ifndef PROFILE_LABEL_WIDTH
#define PROFILE_LABEL_WIDTH 32
#endif

/* Callbacks for internal profile_info.
   The callbacks may be NULL meaning there isn't one.
   Note that results are indented two spaces to distinguish them from
   section titles.
   If non-NULL, PROFILE_CALLBACK is called to print extra non-cpu related data.
   If non-NULL, PROFILE_CPU_CALLBACK is called to print extra cpu related data.
   */

typedef void (PROFILE_INFO_CALLBACK_FN) (SIM_DESC, int);
struct _sim_cpu; /* forward reference */
typedef void (PROFILE_INFO_CPU_CALLBACK_FN) (struct _sim_cpu *cpu, int verbose);


/* Struct containing most profiling data.
   It doesn't contain all profiling data because for example scache data
   is kept with the rest of scache support.  */

typedef struct {
  /* Global summary of all the current profiling options.  */
  char profile_any_p;

  /* Boolean array of specified profiling flags.  */
  char profile_flags[MAX_PROFILE_VALUES];
#define PROFILE_FLAGS(p) ((p)->profile_flags)

  /* The total insn count is tracked separately.
     It is always computed, regardless of insn profiling.  */
  unsigned long total_insn_count;
#define PROFILE_TOTAL_INSN_COUNT(p) ((p)->total_insn_count)

  /* CPU frequency.  Always accepted, regardless of profiling options.  */
  unsigned long cpu_freq;
#define PROFILE_CPU_FREQ(p) ((p)->cpu_freq)

#if WITH_PROFILE_INSN_P
  unsigned int *insn_count;
#define PROFILE_INSN_COUNT(p) ((p)->insn_count)
#endif

#if WITH_PROFILE_MEMORY_P
  unsigned int read_count[MAX_TARGET_MODES];
#define PROFILE_READ_COUNT(p) ((p)->read_count)
  unsigned int write_count[MAX_TARGET_MODES];
#define PROFILE_WRITE_COUNT(p) ((p)->write_count)
#endif

#if WITH_PROFILE_CORE_P
  /* Count read/write/exec accesses separatly. */
  unsigned int core_count[nr_maps];
#define PROFILE_CORE_COUNT(p) ((p)->core_count)
#endif

#if WITH_PROFILE_MODEL_P
  /* ??? Quick hack until more elaborate scheme is finished.  */
  /* Total cycle count, including stalls.  */
  unsigned long total_cycles;
#define PROFILE_MODEL_TOTAL_CYCLES(p) ((p)->total_cycles)
  /* Stalls due to branches.  */
  unsigned long cti_stall_cycles;
#define PROFILE_MODEL_CTI_STALL_CYCLES(p) ((p)->cti_stall_cycles)
  unsigned long load_stall_cycles;
#define PROFILE_MODEL_LOAD_STALL_CYCLES(p) ((p)->load_stall_cycles)
  /* Number of cycles the current instruction took.  */
  unsigned long cur_insn_cycles;
#define PROFILE_MODEL_CUR_INSN_CYCLES(p) ((p)->cur_insn_cycles)

  /* Taken and not-taken branches (and other cti's).  */
  unsigned long taken_count, untaken_count;
#define PROFILE_MODEL_TAKEN_COUNT(p) ((p)->taken_count)
#define PROFILE_MODEL_UNTAKEN_COUNT(p) ((p)->untaken_count)
#endif

#if WITH_PROFILE_PC_P
  /* PC profiling attempts to determine function usage by sampling the PC
     every so many instructions.  */
  unsigned int profile_pc_freq;
#define PROFILE_PC_FREQ(p) ((p)->profile_pc_freq)
  unsigned int profile_pc_nr_buckets;
#define PROFILE_PC_NR_BUCKETS(p) ((p)->profile_pc_nr_buckets)
  address_word profile_pc_start;
#define PROFILE_PC_START(p) ((p)->profile_pc_start)
  address_word profile_pc_end;
#define PROFILE_PC_END(p) ((p)->profile_pc_end)
  unsigned profile_pc_shift;
#define PROFILE_PC_SHIFT(p) ((p)->profile_pc_shift)
#define PROFILE_PC_BUCKET_SIZE(p) (PROFILE_PC_SHIFT (p) ? (1 << PROFILE_PC_SHIFT (p)) : 0)
  unsigned *profile_pc_count;
#define PROFILE_PC_COUNT(p) ((p)->profile_pc_count)
  sim_event *profile_pc_event;
#define PROFILE_PC_EVENT(p) ((p)->profile_pc_event)
#endif

  /* Profile output goes to this or stderr if NULL.
     We can't store `stderr' here as stderr goes through a callback.  */
  FILE *profile_file;
#define PROFILE_FILE(p) ((p)->profile_file)

  /* When reporting a profile summary, hook to include per-processor
     target specific profile information */
  PROFILE_INFO_CPU_CALLBACK_FN *info_cpu_callback;
#define PROFILE_INFO_CPU_CALLBACK(p) ((p)->info_cpu_callback)

  /* When reporting a profile summary, hook to include common target
     specific profile information */
  PROFILE_INFO_CALLBACK_FN *info_callback;
#define STATE_PROFILE_INFO_CALLBACK(sd) \
(CPU_PROFILE_DATA (STATE_CPU (sd, 0))->info_callback)

  /* Profile range.
     ??? Not all cpu's support this.  */
  ADDR_RANGE range;
#define PROFILE_RANGE(p) (& (p)->range)
} PROFILE_DATA;

/* Predicates.  */

#define CPU_PROFILE_FLAGS(cpu) PROFILE_FLAGS (CPU_PROFILE_DATA (cpu))

/* Return non-zero if tracing of IDX is enabled for CPU.  */
#define PROFILE_P(cpu,idx) \
((WITH_PROFILE & (1 << (idx))) != 0 \
 && CPU_PROFILE_FLAGS (cpu)[idx] != 0)

/* Non-zero if --profile-<xxxx> was specified for CPU.  */
#define PROFILE_ANY_P(cpu)	((WITH_PROFILE) && (CPU_PROFILE_DATA (cpu)->profile_any_p))
#define PROFILE_INSN_P(cpu)	PROFILE_P (cpu, PROFILE_INSN_IDX)
#define PROFILE_MEMORY_P(cpu)	PROFILE_P (cpu, PROFILE_MEMORY_IDX)
#define PROFILE_MODEL_P(cpu)	PROFILE_P (cpu, PROFILE_MODEL_IDX)
#define PROFILE_SCACHE_P(cpu)	PROFILE_P (cpu, PROFILE_SCACHE_IDX)
#define PROFILE_PC_P(cpu)	PROFILE_P (cpu, PROFILE_PC_IDX)
#define PROFILE_CORE_P(cpu)	PROFILE_P (cpu, PROFILE_CORE_IDX)

/* Usage macros.  */

#if WITH_PROFILE_INSN_P
#define PROFILE_COUNT_INSN(cpu, pc, insn_num) \
do { \
  if (PROFILE_INSN_P (cpu)) \
    ++ PROFILE_INSN_COUNT (CPU_PROFILE_DATA (cpu)) [insn_num]; \
} while (0)
#else
#define PROFILE_COUNT_INSN(cpu, pc, insn_num)
#endif /* ! insn */

#if WITH_PROFILE_MEMORY_P
#define PROFILE_COUNT_READ(cpu, addr, mode_num) \
do { \
  if (PROFILE_MEMORY_P (cpu)) \
    ++ PROFILE_READ_COUNT (CPU_PROFILE_DATA (cpu)) [mode_num]; \
} while (0)
#define PROFILE_COUNT_WRITE(cpu, addr, mode_num) \
do { \
  if (PROFILE_MEMORY_P (cpu)) \
    ++ PROFILE_WRITE_COUNT (CPU_PROFILE_DATA (cpu)) [mode_num]; \
} while (0)
#else
#define PROFILE_COUNT_READ(cpu, addr, mode_num)
#define PROFILE_COUNT_WRITE(cpu, addr, mode_num)
#endif /* ! memory */

#if WITH_PROFILE_CORE_P
#define PROFILE_COUNT_CORE(cpu, addr, size, map) \
do { \
  if (PROFILE_CORE_P (cpu)) \
    PROFILE_CORE_COUNT (CPU_PROFILE_DATA (cpu)) [map] += 1; \
} while (0)
#else
#define PROFILE_COUNT_CORE(cpu, addr, size, map)
#endif /* ! core */

/* Misc. utilities.  */

extern void sim_profile_print_bar (SIM_DESC, unsigned int, unsigned int, unsigned int);

#endif /* SIM_PROFILE_H */
