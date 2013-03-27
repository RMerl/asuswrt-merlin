
/* Main header for the Vitesse IQ2000.  */

#ifndef SIM_MAIN_H
#define SIM_MAIN_H

#define USING_SIM_BASE_H /* FIXME: quick hack */

struct _sim_cpu; /* FIXME: should be in sim-basics.h */
typedef struct _sim_cpu SIM_CPU;

/* sim-basics.h includes config.h but cgen-types.h must be included before
   sim-basics.h and cgen-types.h needs config.h.  */
#include "config.h"

#include "symcat.h"
#include "sim-basics.h"
#include "cgen-types.h"
#include "iq2000-desc.h"
#include "iq2000-opc.h"
#include "arch.h"

/* Pull in IQ2000_{DATA,INSN}_{MASK,VALUE}.  */
#include "elf/iq2000.h"

/* These must be defined before sim-base.h.  */
typedef USI sim_cia;

#define CIA_GET(cpu)     CPU_PC_GET (cpu)
#define CIA_SET(cpu,val) CPU_PC_SET ((cpu), (val))

#include "sim-base.h"
#include "cgen-sim.h"
#include "iq2000-sim.h"

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
#if defined (WANT_CPU_IQ2000BF)
  IQ2000BF_CPU_DATA cpu_data;
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
extern SIM_CORE_SIGNAL_FN iq2000_core_signal;
#define SIM_CORE_SIGNAL(SD,CPU,CIA,MAP,NR_BYTES,ADDR,TRANSFER,ERROR) \
iq2000_core_signal ((SD), (CPU), (CIA), (MAP), (NR_BYTES), (ADDR), \
		  (TRANSFER), (ERROR))

/* Convert between CPU-internal addresses and sim_core addresses.  */
#define CPU2DATA(addr) (IQ2000_DATA_VALUE + (addr))
#define DATA2CPU(addr) ((addr) - IQ2000_DATA_VALUE)
#define CPU2INSN(addr) (IQ2000_INSN_VALUE + ((addr) & ~IQ2000_INSN_MASK))
#define INSN2CPU(addr) ((addr) - IQ2000_INSN_VALUE)
#define IQ2000_INSN_MEM_SIZE (CPU2INSN(0x800000) - CPU2INSN(0x0000))
#define IQ2000_DATA_MEM_SIZE (CPU2DATA(0x800000) - CPU2DATA(0x0000))

#endif /* SIM_MAIN_H */
