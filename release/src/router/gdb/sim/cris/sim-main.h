/* Main header for the CRIS simulator, based on the m32r header.
   Copyright (C) 2004, 2005, 2006, 2007 Free Software Foundation, Inc.
   Contributed by Axis Communications.

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
along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* All FIXME:s present in m32r apply here too; I just refuse to blindly
   carry them over, as I don't know if they're really things that need
   fixing.  */

#ifndef SIM_MAIN_H
#define SIM_MAIN_H

#define USING_SIM_BASE_H

struct _sim_cpu;
typedef struct _sim_cpu SIM_CPU;

#include "symcat.h"
#include "sim-basics.h"
#include "cgen-types.h"
#include "cris-desc.h"
#include "cris-opc.h"
#include "arch.h"

/* These must be defined before sim-base.h.  */
typedef USI sim_cia;

#define CIA_GET(cpu)     CPU_PC_GET (cpu)
#define CIA_SET(cpu,val) CPU_PC_SET ((cpu), (val))

#define SIM_ENGINE_HALT_HOOK(sd, cpu, cia) \
do { \
  if (cpu) /* Null if ctrl-c.  */ \
    sim_pc_set ((cpu), (cia)); \
} while (0)
#define SIM_ENGINE_RESTART_HOOK(sd, cpu, cia) \
do { \
  sim_pc_set ((cpu), (cia)); \
} while (0)

#include "sim-base.h"
#include "cgen-sim.h"
#include "cris-sim.h"

struct cris_sim_mmapped_page {
  USI addr;
  struct cris_sim_mmapped_page *prev;
};

struct cris_thread_info {
  /* Identifier for this thread.  */
  unsigned int threadid;

  /* Identifier for parent thread.  */
  unsigned int parent_threadid;

  /* Signal to send to parent at exit.  */
  int exitsig;

  /* Exit status.  */
  int exitval;

  /* Only as storage to return the "set" value to the "get" method.
     I'm not sure whether this is useful per-thread.  */
  USI priority;

  struct
  {
    USI altstack;
    USI options;

    char action;
    char pending;
    char blocked;
    char blocked_suspendsave;
    /* The handler stub unblocks the signal, so we don't need a separate
       "temporary save" for that.  */
  } sigdata[64];

  /* Register context, swapped with _sim_cpu.cpu_data.  */
  void *cpu_context;

  /* Similar, temporary copy for the state at a signal call.   */
  void *cpu_context_atsignal;

  /* The number of the reading and writing ends of a pipe if waiting for
     the reader, else 0.  */
  int pipe_read_fd;
  int pipe_write_fd;

  /* System time at last context switch when this thread ran.  */
  USI last_execution;

  /* Nonzero if we just executed a syscall.  */
  char at_syscall;

  /* Nonzero if any of sigaction[0..64].pending is true.  */
  char sigpending;

  /* Nonzero if in (rt_)sigsuspend call.  Cleared at every sighandler
     call.  */
  char sigsuspended;
};

typedef int (*cris_interrupt_delivery_fn) (SIM_CPU *,
					   enum cris_interrupt_type,
					   unsigned int);

struct _sim_cpu {
  /* sim/common cpu base.  */
  sim_cpu_base base;

  /* Static parts of cgen.  */
  CGEN_CPU cgen_cpu;

  CRIS_MISC_PROFILE cris_misc_profile;
#define CPU_CRIS_MISC_PROFILE(cpu) (& (cpu)->cris_misc_profile)

  /* Copy of previous data; only valid when emitting trace-data after
     each insn.  */
  CRIS_MISC_PROFILE cris_prev_misc_profile;
#define CPU_CRIS_PREV_MISC_PROFILE(cpu) (& (cpu)->cris_prev_misc_profile)

#if WITH_HW
  cris_interrupt_delivery_fn deliver_interrupt;
#define CPU_CRIS_DELIVER_INTERRUPT(cpu) (cpu->deliver_interrupt)
#endif

  /* Simulator environment data.  */
  USI endmem;
  USI endbrk;
  USI stack_low;
  struct cris_sim_mmapped_page *highest_mmapped_page;

  /* Number of syscalls performed or in progress, counting once extra
     for every time a blocked thread (internally, when threading) polls
     the (pipe) blockage.  By default, this is also a time counter: to
     minimize performance noise from minor compiler changes,
     instructions take no time and syscalls always take 1ms.  */
  USI syscalls;

  /* Number of execution contexts minus one.  */
  int m1threads;

  /* Current thread number; index into thread_data when m1threads != 0.  */
  int threadno;

  /* When a new thread is created, it gets a unique number, which we
     count here.  */
  int max_threadid;

  /* Thread-specific info, for simulator thread support, created at
     "clone" call.  Vector of [threads+1] when m1threads > 0.  */
  struct cris_thread_info *thread_data;

  /* "If CLONE_SIGHAND is set, the calling process and the child pro-
     cesses share the same table of signal handlers." ... "However, the
     calling process and child processes still have distinct signal
     masks and sets of pending signals."  See struct cris_thread_info
     for sigmasks and sigpendings. */
  USI sighandler[64];

  /* This is a hack to implement just the parts of fcntl F_GETFL that
     are used in open+fdopen calls for the standard scenario: for such
     a call we check that the last syscall was open, we check that the
     passed fd is the same returned then, and so we return the same
     flags passed to open.  This way, we avoid complicating the
     generic sim callback machinery by introducing fcntl
     mechanisms.  */
  USI last_syscall;
  USI last_open_fd;
  USI last_open_flags;

  /* Function for initializing CPU thread context, which varies in size
     with each CPU model.  They should be in some constant parts or
     initialized in *_init_cpu, but we can't modify that for now.  */
  void* (*make_thread_cpu_data) (SIM_CPU *, void *);
  size_t thread_cpu_data_size;

  /* CPU-model specific parts go here.
     Note that in files that don't need to access these pieces WANT_CPU_FOO
     won't be defined and thus these parts won't appear.  This is ok in the
     sense that things work.  It is a source of bugs though.
     One has to of course be careful to not take the size of this
     struct and no structure members accessed in non-cpu specific files can
     go after here.  */
#if defined (WANT_CPU_CRISV0F)
  CRISV0F_CPU_DATA cpu_data;
#elif defined (WANT_CPU_CRISV3F)
  CRISV3F_CPU_DATA cpu_data;
#elif defined (WANT_CPU_CRISV8F)
  CRISV8F_CPU_DATA cpu_data;
#elif defined (WANT_CPU_CRISV10F)
  CRISV10F_CPU_DATA cpu_data;
#elif defined (WANT_CPU_CRISV32F)
  CRISV32F_CPU_DATA cpu_data;
#else
  /* Let's assume all cpu_data have the same alignment requirements, so
     they all are laid out at the same address.  Since we can't get the
     exact definition, we also assume that it has no higher alignment
     requirements than a vector of, say, 16 pointers.  (A single member
     is often special-cased, and possibly two as well so we don't want
     that).  */
  union { void *dummy[16]; } cpu_data_placeholder;
#endif
};

/* The sim_state struct.  */

struct sim_state {
  sim_cpu *cpu;
#define STATE_CPU(sd, n) (/*&*/ (sd)->cpu)

  CGEN_STATE cgen_state;

  sim_state_base base;
};

/* Misc.  */

/* Catch address exceptions.  */
extern SIM_CORE_SIGNAL_FN cris_core_signal;
#define SIM_CORE_SIGNAL(SD,CPU,CIA,MAP,NR_BYTES,ADDR,TRANSFER,ERROR) \
cris_core_signal ((SD), (CPU), (CIA), (MAP), (NR_BYTES), (ADDR), \
		  (TRANSFER), (ERROR))

/* Default memory size.  */
#define CRIS_DEFAULT_MEM_SIZE 0x800000 /* 8M */

extern device cris_devices;

#endif /* SIM_MAIN_H */
