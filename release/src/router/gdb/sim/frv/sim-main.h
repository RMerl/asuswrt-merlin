/* frv simulator support code
   Copyright (C) 1998, 2000, 2001, 2007 Free Software Foundation, Inc.
   Contributed by Red Hat.

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

/* Main header for the frv.  */

#define USING_SIM_BASE_H /* FIXME: quick hack */

struct _sim_cpu; /* FIXME: should be in sim-basics.h */
typedef struct _sim_cpu SIM_CPU;

/* Set the mask of unsupported traces.  */
#define WITH_TRACE \
  (~(TRACE_alu | TRACE_decode | TRACE_memory | TRACE_model | TRACE_fpu \
     | TRACE_branch | TRACE_debug))

/* sim-basics.h includes config.h but cgen-types.h must be included before
   sim-basics.h and cgen-types.h needs config.h.  */
#include "config.h"

#include "symcat.h"
#include "sim-basics.h"
#include "cgen-types.h"
#include "frv-desc.h"
#include "frv-opc.h"
#include "arch.h"

/* These must be defined before sim-base.h.  */
typedef USI sim_cia;

#define CIA_GET(cpu)     CPU_PC_GET (cpu)
#define CIA_SET(cpu,val) CPU_PC_SET ((cpu), (val))

void frv_sim_engine_halt_hook (SIM_DESC, SIM_CPU *, sim_cia);
#define SIM_ENGINE_HALT_HOOK(SD, LAST_CPU, CIA) \
  frv_sim_engine_halt_hook ((SD), (LAST_CPU), (CIA))

#define SIM_ENGINE_RESTART_HOOK(SD, LAST_CPU, CIA) 0

#include "sim-base.h"
#include "cgen-sim.h"
#include "frv-sim.h"
#include "cache.h"
#include "registers.h"
#include "profile.h"

/* The _sim_cpu struct.  */

struct _sim_cpu {
  /* sim/common cpu base.  */
  sim_cpu_base base;

  /* Static parts of cgen.  */
  CGEN_CPU cgen_cpu;

  /* CPU specific parts go here.
     Note that in files that don't need to access these pieces WANT_CPU_FOO
     won't be defined and thus these parts won't appear.  This is ok in the
     sense that things work.  It is a source of bugs though.
     One has to of course be careful to not take the size of this
     struct and no structure members accessed in non-cpu specific files can
     go after here.  Oh for a better language.  */
#if defined (WANT_CPU_FRVBF)
  FRVBF_CPU_DATA cpu_data;

  /* Control information for registers */
  FRV_REGISTER_CONTROL register_control;
#define CPU_REGISTER_CONTROL(cpu) (& (cpu)->register_control)

  FRV_VLIW vliw;
#define CPU_VLIW(cpu) (& (cpu)->vliw)

  FRV_CACHE insn_cache;
#define CPU_INSN_CACHE(cpu) (& (cpu)->insn_cache)

  FRV_CACHE data_cache;
#define CPU_DATA_CACHE(cpu) (& (cpu)->data_cache)

  FRV_PROFILE_STATE profile_state;
#define CPU_PROFILE_STATE(cpu) (& (cpu)->profile_state)

  int debug_state;
#define CPU_DEBUG_STATE(cpu) ((cpu)->debug_state)

  SI load_address;
#define CPU_LOAD_ADDRESS(cpu) ((cpu)->load_address)

  SI load_length;
#define CPU_LOAD_LENGTH(cpu) ((cpu)->load_length)

  SI load_flag;
#define CPU_LOAD_SIGNED(cpu) ((cpu)->load_flag)
#define CPU_LOAD_LOCK(cpu) ((cpu)->load_flag)

  SI store_flag;
#define CPU_RSTR_INVALIDATE(cpu) ((cpu)->store_flag)

  unsigned long elf_flags;
#define CPU_ELF_FLAGS(cpu) ((cpu)->elf_flags)
#endif /* defined (WANT_CPU_FRVBF) */
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
extern SIM_CORE_SIGNAL_FN frv_core_signal;
#define SIM_CORE_SIGNAL(SD,CPU,CIA,MAP,NR_BYTES,ADDR,TRANSFER,ERROR) \
frv_core_signal ((SD), (CPU), (CIA), (MAP), (NR_BYTES), (ADDR), \
		  (TRANSFER), (ERROR))

/* Default memory size.  */
#define FRV_DEFAULT_MEM_SIZE 0x800000 /* 8M */
