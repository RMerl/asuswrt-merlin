/* MIPS Simulator definition.
   Copyright (C) 1997, 1998, 2003, 2007 Free Software Foundation, Inc.
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

#ifndef SIM_MAIN_H
#define SIM_MAIN_H

/* This simulator doesn't cache the Current Instruction Address */
/* #define SIM_ENGINE_HALT_HOOK(SD, LAST_CPU, CIA) */
/* #define SIM_ENGINE_RESUME_HOOK(SD, LAST_CPU, CIA) */

#define SIM_HAVE_BIENDIAN


/* hobble some common features for moment */
#define WITH_WATCHPOINTS 1
#define WITH_MODULO_MEMORY 1


#define SIM_CORE_SIGNAL(SD,CPU,CIA,MAP,NR_BYTES,ADDR,TRANSFER,ERROR) \
mips_core_signal ((SD), (CPU), (CIA), (MAP), (NR_BYTES), (ADDR), (TRANSFER), (ERROR))

#include "sim-basics.h"

typedef address_word sim_cia;

#include "sim-base.h"
#include "bfd.h"

/* Deprecated macros and types for manipulating 64bit values.  Use
   ../common/sim-bits.h and ../common/sim-endian.h macros instead. */

typedef signed64 word64;
typedef unsigned64 uword64;

#define WORD64LO(t)     (unsigned int)((t)&0xFFFFFFFF)
#define WORD64HI(t)     (unsigned int)(((uword64)(t))>>32)
#define SET64LO(t)      (((uword64)(t))&0xFFFFFFFF)
#define SET64HI(t)	(((uword64)(t))<<32)
#define WORD64(h,l)     ((word64)((SET64HI(h)|SET64LO(l))))
#define UWORD64(h,l)     (SET64HI(h)|SET64LO(l))

/* Check if a value will fit within a halfword: */
#define NOTHALFWORDVALUE(v) ((((((uword64)(v)>>16) == 0) && !((v) & ((unsigned)1 << 15))) || (((((uword64)(v)>>32) == 0xFFFFFFFF) && ((((uword64)(v)>>16) & 0xFFFF) == 0xFFFF)) && ((v) & ((unsigned)1 << 15)))) ? (1 == 0) : (1 == 1))



/* Floating-point operations: */

#include "sim-fpu.h"
#include "cp1.h"

/* FPU registers must be one of the following types. All other values
   are reserved (and undefined). */
typedef enum {
 fmt_single  = 0,
 fmt_double  = 1,
 fmt_word    = 4,
 fmt_long    = 5,
 fmt_ps      = 6,
 /* The following are well outside the normal acceptable format
    range, and are used in the register status vector. */
 fmt_unknown       = 0x10000000,
 fmt_uninterpreted = 0x20000000,
 fmt_uninterpreted_32 = 0x40000000,
 fmt_uninterpreted_64 = 0x80000000U,
} FP_formats;

/* For paired word (pw) operations, the opcode representation is fmt_word,
   but register transfers (StoreFPR, ValueFPR, etc.) are done as fmt_long.  */
#define fmt_pw fmt_long

/* This should be the COC1 value at the start of the preceding
   instruction: */
#define PREVCOC1() ((STATE & simPCOC1) ? 1 : 0)

#ifdef TARGET_ENABLE_FR
/* FIXME: this should be enabled for all targets, but needs testing first. */
#define SizeFGR() (((WITH_TARGET_FLOATING_POINT_BITSIZE) == 64) \
   ? ((SR & status_FR) ? 64 : 32) \
   : (WITH_TARGET_FLOATING_POINT_BITSIZE))
#else
#define SizeFGR() (WITH_TARGET_FLOATING_POINT_BITSIZE)
#endif





/* HI/LO register accesses */

/* For some MIPS targets, the HI/LO registers have certain timing
   restrictions in that, for instance, a read of a HI register must be
   separated by at least three instructions from a preceeding read.

   The struct below is used to record the last access by each of A MT,
   MF or other OP instruction to a HI/LO register.  See mips.igen for
   more details. */

typedef struct _hilo_access {
  signed64 timestamp;
  address_word cia;
} hilo_access;

typedef struct _hilo_history {
  hilo_access mt;
  hilo_access mf;
  hilo_access op;
} hilo_history;




/* Integer ALU operations: */

#include "sim-alu.h"

#define ALU32_END(ANS) \
  if (ALU32_HAD_OVERFLOW) \
    SignalExceptionIntegerOverflow (); \
  (ANS) = (signed32) ALU32_OVERFLOW_RESULT


#define ALU64_END(ANS) \
  if (ALU64_HAD_OVERFLOW) \
    SignalExceptionIntegerOverflow (); \
  (ANS) = ALU64_OVERFLOW_RESULT;





/* The following is probably not used for MIPS IV onwards: */
/* Slots for delayed register updates. For the moment we just have a
   fixed number of slots (rather than a more generic, dynamic
   system). This keeps the simulator fast. However, we only allow
   for the register update to be delayed for a single instruction
   cycle. */
#define PSLOTS (8) /* Maximum number of instruction cycles */

typedef struct _pending_write_queue {
  int in;
  int out;
  int total;
  int slot_delay[PSLOTS];
  int slot_size[PSLOTS];
  int slot_bit[PSLOTS];
  void *slot_dest[PSLOTS];
  unsigned64 slot_value[PSLOTS];
} pending_write_queue;

#ifndef PENDING_TRACE
#define PENDING_TRACE 0
#endif
#define PENDING_IN ((CPU)->pending.in)
#define PENDING_OUT ((CPU)->pending.out)
#define PENDING_TOTAL ((CPU)->pending.total)
#define PENDING_SLOT_SIZE ((CPU)->pending.slot_size)
#define PENDING_SLOT_BIT ((CPU)->pending.slot_bit)
#define PENDING_SLOT_DELAY ((CPU)->pending.slot_delay)
#define PENDING_SLOT_DEST ((CPU)->pending.slot_dest)
#define PENDING_SLOT_VALUE ((CPU)->pending.slot_value)

/* Invalidate the pending write queue, all pending writes are
   discarded. */

#define PENDING_INVALIDATE() \
memset (&(CPU)->pending, 0, sizeof ((CPU)->pending))

/* Schedule a write to DEST for N cycles time.  For 64 bit
   destinations, schedule two writes.  For floating point registers,
   the caller should schedule a write to both the dest register and
   the FPR_STATE register.  When BIT is non-negative, only BIT of DEST
   is updated. */

#define PENDING_SCHED(DEST,VAL,DELAY,BIT)				\
  do {									\
    if (PENDING_SLOT_DEST[PENDING_IN] != NULL)				\
      sim_engine_abort (SD, CPU, cia,					\
		        "PENDING_SCHED - buffer overflow\n");		\
    if (PENDING_TRACE)							\
      sim_io_eprintf (SD, "PENDING_SCHED - 0x%lx - dest 0x%lx, val 0x%lx, bit %d, size %d, pending_in %d, pending_out %d, pending_total %d\n",			\
		      (unsigned long) cia, (unsigned long) &(DEST),	\
		      (unsigned long) (VAL), (BIT), (int) sizeof (DEST),\
		      PENDING_IN, PENDING_OUT, PENDING_TOTAL);		\
    PENDING_SLOT_DELAY[PENDING_IN] = (DELAY) + 1;			\
    PENDING_SLOT_DEST[PENDING_IN] = &(DEST);				\
    PENDING_SLOT_VALUE[PENDING_IN] = (VAL);				\
    PENDING_SLOT_SIZE[PENDING_IN] = sizeof (DEST);			\
    PENDING_SLOT_BIT[PENDING_IN] = (BIT);				\
    PENDING_IN = (PENDING_IN + 1) % PSLOTS;                             \
    PENDING_TOTAL += 1;			                                \
  } while (0)

#define PENDING_WRITE(DEST,VAL,DELAY) PENDING_SCHED(DEST,VAL,DELAY,-1)
#define PENDING_BIT(DEST,VAL,DELAY,BIT) PENDING_SCHED(DEST,VAL,DELAY,BIT)

#define PENDING_TICK() pending_tick (SD, CPU, cia)

#define PENDING_FLUSH() abort () /* think about this one */
#define PENDING_FP() abort () /* think about this one */

/* For backward compatibility */
#define PENDING_FILL(R,VAL) 						\
do {									\
  if ((R) >= FGR_BASE && (R) < FGR_BASE + NR_FGR)			\
    {									\
      PENDING_SCHED(FGR[(R) - FGR_BASE], VAL, 1, -1);			\
      PENDING_SCHED(FPR_STATE[(R) - FGR_BASE], fmt_uninterpreted, 1, -1); \
    }									\
  else									\
    PENDING_SCHED(GPR[(R)], VAL, 1, -1);				\
} while (0)


enum float_operation
  {
    FLOP_ADD,    FLOP_SUB,    FLOP_MUL,    FLOP_MADD,
    FLOP_MSUB,   FLOP_MAX=10, FLOP_MIN,    FLOP_ABS,
    FLOP_ITOF0=14, FLOP_FTOI0=18, FLOP_NEG=23
  };


/* The internal representation of an MDMX accumulator. 
   Note that 24 and 48 bit accumulator elements are represented in
   32 or 64 bits.  Since the accumulators are 2's complement with
   overflow suppressed, high-order bits can be ignored in most contexts.  */

typedef signed32 signed24;
typedef signed64 signed48;

typedef union { 
  signed24  ob[8];
  signed48  qh[4]; 
} MDMX_accumulator;


/* Conventional system arguments.  */ 
#define SIM_STATE  sim_cpu *cpu, address_word cia
#define SIM_ARGS   CPU, cia

struct _sim_cpu {


  /* The following are internal simulator state variables: */
#define CIA_GET(CPU) ((CPU)->registers[PCIDX] + 0)
#define CIA_SET(CPU,CIA) ((CPU)->registers[PCIDX] = (CIA))
  address_word dspc;  /* delay-slot PC */
#define DSPC ((CPU)->dspc)

#define DELAY_SLOT(TARGET) NIA = delayslot32 (SD_, (TARGET))
#define NULLIFY_NEXT_INSTRUCTION() NIA = nullify_next_insn32 (SD_)


  /* State of the simulator */
  unsigned int state;
  unsigned int dsstate;
#define STATE ((CPU)->state)
#define DSSTATE ((CPU)->dsstate)

/* Flags in the "state" variable: */
#define simHALTEX       (1 << 2)  /* 0 = run; 1 = halt on exception */
#define simHALTIN       (1 << 3)  /* 0 = run; 1 = halt on interrupt */
#define simTRACE        (1 << 8)  /* 0 = do nothing; 1 = trace address activity */
#define simPCOC0        (1 << 17) /* COC[1] from current */
#define simPCOC1        (1 << 18) /* COC[1] from previous */
#define simDELAYSLOT    (1 << 24) /* 0 = do nothing; 1 = delay slot entry exists */
#define simSKIPNEXT     (1 << 25) /* 0 = do nothing; 1 = skip instruction */
#define simSIGINT	(1 << 28)  /* 0 = do nothing; 1 = SIGINT has occured */
#define simJALDELAYSLOT	(1 << 29) /* 1 = in jal delay slot */

#ifndef ENGINE_ISSUE_PREFIX_HOOK
#define ENGINE_ISSUE_PREFIX_HOOK() \
  { \
    /* Perform any pending writes */ \
    PENDING_TICK(); \
    /* Set previous flag, depending on current: */ \
    if (STATE & simPCOC0) \
     STATE |= simPCOC1; \
    else \
     STATE &= ~simPCOC1; \
    /* and update the current value: */ \
    if (GETFCC(0)) \
     STATE |= simPCOC0; \
    else \
     STATE &= ~simPCOC0; \
  }
#endif /* ENGINE_ISSUE_PREFIX_HOOK */


/* This is nasty, since we have to rely on matching the register
   numbers used by GDB. Unfortunately, depending on the MIPS target
   GDB uses different register numbers. We cannot just include the
   relevant "gdb/tm.h" link, since GDB may not be configured before
   the sim world, and also the GDB header file requires too much other
   state. */

#ifndef TM_MIPS_H
#define LAST_EMBED_REGNUM (96)
#define NUM_REGS (LAST_EMBED_REGNUM + 1)

#define FP0_REGNUM 38           /* Floating point register 0 (single float) */
#define FCRCS_REGNUM 70         /* FP control/status */
#define FCRIR_REGNUM 71         /* FP implementation/revision */
#endif


/* To keep this default simulator simple, and fast, we use a direct
   vector of registers. The internal simulator engine then uses
   manifests to access the correct slot. */

  unsigned_word registers[LAST_EMBED_REGNUM + 1];

  int register_widths[NUM_REGS];
#define REGISTERS       ((CPU)->registers)

#define GPR     (&REGISTERS[0])
#define GPR_SET(N,VAL) (REGISTERS[(N)] = (VAL))

#define LO      (REGISTERS[33])
#define HI      (REGISTERS[34])
#define PCIDX	37
#define PC      (REGISTERS[PCIDX])
#define CAUSE   (REGISTERS[36])
#define SRIDX   (32)
#define SR      (REGISTERS[SRIDX])      /* CPU status register */
#define FCR0IDX  (71)
#define FCR0    (REGISTERS[FCR0IDX])    /* really a 32bit register */
#define FCR31IDX (70)
#define FCR31   (REGISTERS[FCR31IDX])   /* really a 32bit register */
#define FCSR    (FCR31)
#define Debug	(REGISTERS[86])
#define DEPC	(REGISTERS[87])
#define EPC	(REGISTERS[88])
#define ACX	(REGISTERS[89])

#define AC0LOIDX	(33)	/* Must be the same register as LO */
#define AC0HIIDX	(34)	/* Must be the same register as HI */
#define AC1LOIDX	(90)
#define AC1HIIDX	(91)
#define AC2LOIDX	(92)
#define AC2HIIDX	(93)
#define AC3LOIDX	(94)
#define AC3HIIDX	(95)

#define DSPLO(N)	(REGISTERS[DSPLO_REGNUM[N]])
#define DSPHI(N)	(REGISTERS[DSPHI_REGNUM[N]])

#define DSPCRIDX	(96)	/* DSP control register */
#define DSPCR		(REGISTERS[DSPCRIDX])

#define DSPCR_POS_SHIFT		(0)
#define DSPCR_POS_MASK		(0x3f)
#define DSPCR_POS_SMASK		(DSPCR_POS_MASK << DSPCR_POS_SHIFT)

#define DSPCR_SCOUNT_SHIFT	(7)
#define DSPCR_SCOUNT_MASK	(0x3f)
#define DSPCR_SCOUNT_SMASK	(DSPCR_SCOUNT_MASK << DSPCR_SCOUNT_SHIFT)

#define DSPCR_CARRY_SHIFT	(13)
#define DSPCR_CARRY_MASK	(1)
#define DSPCR_CARRY_SMASK	(DSPCR_CARRY_MASK << DSPCR_CARRY_SHIFT)
#define DSPCR_CARRY		(1 << DSPCR_CARRY_SHIFT)

#define DSPCR_EFI_SHIFT		(14)
#define DSPCR_EFI_MASK		(1)
#define DSPCR_EFI_SMASK		(DSPCR_EFI_MASK << DSPCR_EFI_SHIFT)
#define DSPCR_EFI		(1 << DSPCR_EFI_MASK)

#define DSPCR_OUFLAG_SHIFT	(16)
#define DSPCR_OUFLAG_MASK	(0xff)
#define DSPCR_OUFLAG_SMASK	(DSPCR_OUFLAG_MASK << DSPCR_OUFLAG_SHIFT)
#define DSPCR_OUFLAG4		(1 << (DSPCR_OUFLAG_SHIFT + 4))
#define DSPCR_OUFLAG5		(1 << (DSPCR_OUFLAG_SHIFT + 5))
#define DSPCR_OUFLAG6		(1 << (DSPCR_OUFLAG_SHIFT + 6))
#define DSPCR_OUFLAG7		(1 << (DSPCR_OUFLAG_SHIFT + 7))

#define DSPCR_CCOND_SHIFT	(24)
#define DSPCR_CCOND_MASK	(0xf)
#define DSPCR_CCOND_SMASK	(DSPCR_CCOND_MASK << DSPCR_CCOND_SHIFT)

  /* All internal state modified by signal_exception() that may need to be
     rolled back for passing moment-of-exception image back to gdb. */
  unsigned_word exc_trigger_registers[LAST_EMBED_REGNUM + 1];
  unsigned_word exc_suspend_registers[LAST_EMBED_REGNUM + 1];
  int exc_suspended;

#define SIM_CPU_EXCEPTION_TRIGGER(SD,CPU,CIA) mips_cpu_exception_trigger(SD,CPU,CIA)
#define SIM_CPU_EXCEPTION_SUSPEND(SD,CPU,EXC) mips_cpu_exception_suspend(SD,CPU,EXC)
#define SIM_CPU_EXCEPTION_RESUME(SD,CPU,EXC) mips_cpu_exception_resume(SD,CPU,EXC)

  unsigned_word c0_config_reg;
#define C0_CONFIG ((CPU)->c0_config_reg)

/* The following are pseudonyms for standard registers */
#define ZERO    (REGISTERS[0])
#define V0      (REGISTERS[2])
#define A0      (REGISTERS[4])
#define A1      (REGISTERS[5])
#define A2      (REGISTERS[6])
#define A3      (REGISTERS[7])
#define T8IDX   24
#define T8	(REGISTERS[T8IDX])
#define SPIDX   29
#define SP      (REGISTERS[SPIDX])
#define RAIDX   31
#define RA      (REGISTERS[RAIDX])

  /* While space is allocated in the main registers arrray for some of
     the COP0 registers, that space isn't sufficient.  Unknown COP0
     registers overflow into the array below */

#define NR_COP0_GPR	32
  unsigned_word cop0_gpr[NR_COP0_GPR];
#define COP0_GPR	((CPU)->cop0_gpr)
#define COP0_BADVADDR	(COP0_GPR[8])

  /* While space is allocated for the floating point registers in the
     main registers array, they are stored separatly.  This is because
     their size may not necessarily match the size of either the
     general-purpose or system specific registers.  */
#define NR_FGR    (32)
#define FGR_BASE  FP0_REGNUM
  fp_word fgr[NR_FGR];
#define FGR       ((CPU)->fgr)

  /* Keep the current format state for each register: */
  FP_formats fpr_state[32];
#define FPR_STATE ((CPU)->fpr_state)

  pending_write_queue pending;

  /* The MDMX accumulator (used only for MDMX ASE).  */
  MDMX_accumulator acc; 
#define ACC             ((CPU)->acc)

  /* LLBIT = Load-Linked bit. A bit of "virtual" state used by atomic
     read-write instructions. It is set when a linked load occurs. It
     is tested and cleared by the conditional store. It is cleared
     (during other CPU operations) when a store to the location would
     no longer be atomic. In particular, it is cleared by exception
     return instructions. */
  int llbit;
#define LLBIT ((CPU)->llbit)


/* The HIHISTORY and LOHISTORY timestamps are used to ensure that
   corruptions caused by using the HI or LO register too close to a
   following operation is spotted. See mips.igen for more details. */

  hilo_history hi_history;
#define HIHISTORY (&(CPU)->hi_history)
  hilo_history lo_history;
#define LOHISTORY (&(CPU)->lo_history)


  sim_cpu_base base;
};


/* MIPS specific simulator watch config */

void watch_options_install PARAMS ((SIM_DESC sd));

struct swatch {
  sim_event *pc;
  sim_event *clock;
  sim_event *cycles;
};


/* FIXME: At present much of the simulator is still static */
struct sim_state {

  struct swatch watch;

  sim_cpu cpu[MAX_NR_PROCESSORS];
#if (WITH_SMP)
#define STATE_CPU(sd,n) (&(sd)->cpu[n])
#else
#define STATE_CPU(sd,n) (&(sd)->cpu[0])
#endif


  sim_state_base base;
};



/* Status information: */

/* TODO : these should be the bitmasks for these bits within the
   status register. At the moment the following are VR4300
   bit-positions: */
#define status_KSU_mask  (0x18)         /* mask for KSU bits */
#define status_KSU_shift (3)            /* shift for field */
#define ksu_kernel       (0x0)
#define ksu_supervisor   (0x1)
#define ksu_user         (0x2)
#define ksu_unknown      (0x3)

#define SR_KSU		 ((SR & status_KSU_mask) >> status_KSU_shift)

#define status_IE	 (1 <<  0)      /* Interrupt enable */
#define status_EIE	 (1 << 16)      /* Enable Interrupt Enable */
#define status_EXL	 (1 <<  1)	/* Exception level */
#define status_RE        (1 << 25)      /* Reverse Endian in user mode */
#define status_FR        (1 << 26)      /* enables MIPS III additional FP registers */
#define status_SR        (1 << 20)      /* soft reset or NMI */
#define status_BEV       (1 << 22)      /* Location of general exception vectors */
#define status_TS        (1 << 21)      /* TLB shutdown has occurred */
#define status_ERL       (1 <<  2)      /* Error level */
#define status_IM7       (1 << 15)      /* Timer Interrupt Mask */
#define status_RP        (1 << 27)      /* Reduced Power mode */

/* Specializations for TX39 family */
#define status_IEc       (1 << 0)       /* Interrupt enable (current) */
#define status_KUc       (1 << 1)       /* Kernel/User mode */
#define status_IEp       (1 << 2)       /* Interrupt enable (previous) */
#define status_KUp       (1 << 3)       /* Kernel/User mode */
#define status_IEo       (1 << 4)       /* Interrupt enable (old) */
#define status_KUo       (1 << 5)       /* Kernel/User mode */
#define status_IM_mask   (0xff)         /* Interrupt mask */
#define status_IM_shift  (8)
#define status_NMI       (1 << 20)      /* NMI */
#define status_NMI       (1 << 20)      /* NMI */

/* Status bits used by MIPS32/MIPS64.  */
#define status_UX        (1 <<  5)      /* 64-bit user addrs */
#define status_SX        (1 <<  6)      /* 64-bit supervisor addrs */
#define status_KX        (1 <<  7)      /* 64-bit kernel addrs */
#define status_TS        (1 << 21)      /* TLB shutdown has occurred */
#define status_PX        (1 << 23)      /* Enable 64 bit operations */
#define status_MX        (1 << 24)      /* Enable MDMX resources */
#define status_CU0       (1 << 28)      /* Coprocessor 0 usable */
#define status_CU1       (1 << 29)      /* Coprocessor 1 usable */
#define status_CU2       (1 << 30)      /* Coprocessor 2 usable */
#define status_CU3       (1 << 31)      /* Coprocessor 3 usable */
/* Bits reserved for implementations:  */
#define status_SBX       (1 << 16)      /* Enable SiByte SB-1 extensions.  */

#define cause_BD ((unsigned)1 << 31)    /* L1 Exception in branch delay slot */
#define cause_BD2         (1 << 30)     /* L2 Exception in branch delay slot */
#define cause_CE_mask     0x30000000	/* Coprocessor exception */
#define cause_CE_shift    28
#define cause_EXC2_mask   0x00070000
#define cause_EXC2_shift  16
#define cause_IP7 	  (1 << 15)	/* Interrupt pending */
#define cause_SIOP        (1 << 12)     /* SIO pending */
#define cause_IP3 	  (1 << 11)	/* Int 0 pending */
#define cause_IP2 	  (1 << 10)	/* Int 1 pending */

#define cause_EXC_mask  (0x1c)          /* Exception code */
#define cause_EXC_shift (2)

#define cause_SW0       (1 << 8)        /* Software interrupt 0 */
#define cause_SW1       (1 << 9)        /* Software interrupt 1 */
#define cause_IP_mask   (0x3f)          /* Interrupt pending field */
#define cause_IP_shift  (10)

#define cause_set_EXC(x)  CAUSE = (CAUSE & ~cause_EXC_mask)  | ((x << cause_EXC_shift)  & cause_EXC_mask)
#define cause_set_EXC2(x) CAUSE = (CAUSE & ~cause_EXC2_mask) | ((x << cause_EXC2_shift) & cause_EXC2_mask)


/* NOTE: We keep the following status flags as bit values (1 for true,
   0 for false). This allows them to be used in binary boolean
   operations without worrying about what exactly the non-zero true
   value is. */

/* UserMode */
#ifdef SUBTARGET_R3900
#define UserMode        ((SR & status_KUc) ? 1 : 0)
#else
#define UserMode	((((SR & status_KSU_mask) >> status_KSU_shift) == ksu_user) ? 1 : 0)
#endif /* SUBTARGET_R3900 */

/* BigEndianMem */
/* Hardware configuration. Affects endianness of LoadMemory and
   StoreMemory and the endianness of Kernel and Supervisor mode
   execution. The value is 0 for little-endian; 1 for big-endian. */
#define BigEndianMem    (CURRENT_TARGET_BYTE_ORDER == BIG_ENDIAN)
/*(state & simBE) ? 1 : 0)*/

/* ReverseEndian */
/* This mode is selected if in User mode with the RE bit being set in
   SR (Status Register). It reverses the endianness of load and store
   instructions. */
#define ReverseEndian   (((SR & status_RE) && UserMode) ? 1 : 0)

/* BigEndianCPU */
/* The endianness for load and store instructions (0=little;1=big). In
   User mode this endianness may be switched by setting the state_RE
   bit in the SR register. Thus, BigEndianCPU may be computed as
   (BigEndianMem EOR ReverseEndian). */
#define BigEndianCPU    (BigEndianMem ^ ReverseEndian) /* Already bits */



/* Exceptions: */

/* NOTE: These numbers depend on the processor architecture being
   simulated: */
enum ExceptionCause {
  Interrupt               = 0,
  TLBModification         = 1,
  TLBLoad                 = 2,
  TLBStore                = 3,
  AddressLoad             = 4,
  AddressStore            = 5,
  InstructionFetch        = 6,
  DataReference           = 7,
  SystemCall              = 8,
  BreakPoint              = 9,
  ReservedInstruction     = 10,
  CoProcessorUnusable     = 11,
  IntegerOverflow         = 12,    /* Arithmetic overflow (IDT monitor raises SIGFPE) */
  Trap                    = 13,
  FPE                     = 15,
  DebugBreakPoint         = 16,    /* Impl. dep. in MIPS32/MIPS64.  */
  MDMX                    = 22,
  Watch                   = 23,
  MCheck                  = 24,
  CacheErr                = 30,
  NMIReset                = 31,    /* Reserved in MIPS32/MIPS64.  */


/* The following exception code is actually private to the simulator
   world. It is *NOT* a processor feature, and is used to signal
   run-time errors in the simulator. */
  SimulatorFault      	  = 0xFFFFFFFF
};

#define TLB_REFILL  (0)
#define TLB_INVALID (1)


/* The following break instructions are reserved for use by the
   simulator.  The first is used to halt the simulation.  The second
   is used by gdb for break-points.  NOTE: Care must be taken, since 
   this value may be used in later revisions of the MIPS ISA. */
#define HALT_INSTRUCTION_MASK   (0x03FFFFC0)

#define HALT_INSTRUCTION        (0x03ff000d)
#define HALT_INSTRUCTION2       (0x0000ffcd)


#define BREAKPOINT_INSTRUCTION  (0x0005000d)
#define BREAKPOINT_INSTRUCTION2 (0x0000014d)



void interrupt_event (SIM_DESC sd, void *data);

void signal_exception (SIM_DESC sd, sim_cpu *cpu, address_word cia, int exception, ...);
#define SignalException(exc,instruction)     signal_exception (SD, CPU, cia, (exc), (instruction))
#define SignalExceptionInterrupt(level)      signal_exception (SD, CPU, cia, Interrupt, level)
#define SignalExceptionInstructionFetch()    signal_exception (SD, CPU, cia, InstructionFetch)
#define SignalExceptionAddressStore()        signal_exception (SD, CPU, cia, AddressStore)
#define SignalExceptionAddressLoad()         signal_exception (SD, CPU, cia, AddressLoad)
#define SignalExceptionDataReference()       signal_exception (SD, CPU, cia, DataReference)
#define SignalExceptionSimulatorFault(buf)   signal_exception (SD, CPU, cia, SimulatorFault, buf)
#define SignalExceptionFPE()                 signal_exception (SD, CPU, cia, FPE)
#define SignalExceptionIntegerOverflow()     signal_exception (SD, CPU, cia, IntegerOverflow)
#define SignalExceptionCoProcessorUnusable(cop) signal_exception (SD, CPU, cia, CoProcessorUnusable)
#define SignalExceptionNMIReset()            signal_exception (SD, CPU, cia, NMIReset)
#define SignalExceptionTLBRefillStore()      signal_exception (SD, CPU, cia, TLBStore, TLB_REFILL)
#define SignalExceptionTLBRefillLoad()       signal_exception (SD, CPU, cia, TLBLoad, TLB_REFILL)
#define SignalExceptionTLBInvalidStore()     signal_exception (SD, CPU, cia, TLBStore, TLB_INVALID)
#define SignalExceptionTLBInvalidLoad()      signal_exception (SD, CPU, cia, TLBLoad, TLB_INVALID)
#define SignalExceptionTLBModification()     signal_exception (SD, CPU, cia, TLBModification)
#define SignalExceptionMDMX()                signal_exception (SD, CPU, cia, MDMX)
#define SignalExceptionWatch()               signal_exception (SD, CPU, cia, Watch)
#define SignalExceptionMCheck()              signal_exception (SD, CPU, cia, MCheck)
#define SignalExceptionCacheErr()            signal_exception (SD, CPU, cia, CacheErr)

/* Co-processor accesses */

/* XXX FIXME: For now, assume that FPU (cp1) is always usable.  */
#define COP_Usable(coproc_num)		(coproc_num == 1)

void cop_lw  PARAMS ((SIM_DESC sd, sim_cpu *cpu, address_word cia, int coproc_num, int coproc_reg, unsigned int memword));
void cop_ld  PARAMS ((SIM_DESC sd, sim_cpu *cpu, address_word cia, int coproc_num, int coproc_reg, uword64 memword));
unsigned int cop_sw PARAMS ((SIM_DESC sd, sim_cpu *cpu, address_word cia, int coproc_num, int coproc_reg));
uword64 cop_sd PARAMS ((SIM_DESC sd, sim_cpu *cpu, address_word cia, int coproc_num, int coproc_reg));

#define COP_LW(coproc_num,coproc_reg,memword) \
cop_lw (SD, CPU, cia, coproc_num, coproc_reg, memword)
#define COP_LD(coproc_num,coproc_reg,memword) \
cop_ld (SD, CPU, cia, coproc_num, coproc_reg, memword)
#define COP_SW(coproc_num,coproc_reg) \
cop_sw (SD, CPU, cia, coproc_num, coproc_reg)
#define COP_SD(coproc_num,coproc_reg) \
cop_sd (SD, CPU, cia, coproc_num, coproc_reg)


void decode_coproc PARAMS ((SIM_DESC sd, sim_cpu *cpu, address_word cia, unsigned int instruction));
#define DecodeCoproc(instruction) \
decode_coproc (SD, CPU, cia, (instruction))

int sim_monitor (SIM_DESC sd, sim_cpu *cpu, address_word cia, unsigned int arg);
  

/* FPR access.  */
unsigned64 value_fpr (SIM_STATE, int fpr, FP_formats);
#define ValueFPR(FPR,FMT) value_fpr (SIM_ARGS, (FPR), (FMT))
void store_fpr (SIM_STATE, int fpr, FP_formats fmt, unsigned64 value);
#define StoreFPR(FPR,FMT,VALUE) store_fpr (SIM_ARGS, (FPR), (FMT), (VALUE))
unsigned64 ps_lower (SIM_STATE, unsigned64 op);
#define PSLower(op) ps_lower (SIM_ARGS, op)
unsigned64 ps_upper (SIM_STATE, unsigned64 op);
#define PSUpper(op) ps_upper (SIM_ARGS, op)
unsigned64 pack_ps (SIM_STATE, unsigned64 op1, unsigned64 op2, FP_formats from);
#define PackPS(op1,op2) pack_ps (SIM_ARGS, op1, op2, fmt_single)


/* FCR access.  */
unsigned_word value_fcr (SIM_STATE, int fcr);
#define ValueFCR(FCR) value_fcr (SIM_ARGS, (FCR))
void store_fcr (SIM_STATE, int fcr, unsigned_word value);
#define StoreFCR(FCR,VALUE) store_fcr (SIM_ARGS, (FCR), (VALUE))
void test_fcsr (SIM_STATE);
#define TestFCSR() test_fcsr (SIM_ARGS)


/* FPU operations.  */
void fp_cmp (SIM_STATE, unsigned64 op1, unsigned64 op2, FP_formats fmt, int abs, int cond, int cc);
#define Compare(op1,op2,fmt,cond,cc) fp_cmp(SIM_ARGS, op1, op2, fmt, 0, cond, cc)
unsigned64 fp_abs (SIM_STATE, unsigned64 op, FP_formats fmt);
#define AbsoluteValue(op,fmt) fp_abs(SIM_ARGS, op, fmt)
unsigned64 fp_neg (SIM_STATE, unsigned64 op, FP_formats fmt);
#define Negate(op,fmt) fp_neg(SIM_ARGS, op, fmt)
unsigned64 fp_add (SIM_STATE, unsigned64 op1, unsigned64 op2, FP_formats fmt);
#define Add(op1,op2,fmt) fp_add(SIM_ARGS, op1, op2, fmt)
unsigned64 fp_sub (SIM_STATE, unsigned64 op1, unsigned64 op2, FP_formats fmt);
#define Sub(op1,op2,fmt) fp_sub(SIM_ARGS, op1, op2, fmt)
unsigned64 fp_mul (SIM_STATE, unsigned64 op1, unsigned64 op2, FP_formats fmt);
#define Multiply(op1,op2,fmt) fp_mul(SIM_ARGS, op1, op2, fmt)
unsigned64 fp_div (SIM_STATE, unsigned64 op1, unsigned64 op2, FP_formats fmt);
#define Divide(op1,op2,fmt) fp_div(SIM_ARGS, op1, op2, fmt)
unsigned64 fp_recip (SIM_STATE, unsigned64 op, FP_formats fmt);
#define Recip(op,fmt) fp_recip(SIM_ARGS, op, fmt)
unsigned64 fp_sqrt (SIM_STATE, unsigned64 op, FP_formats fmt);
#define SquareRoot(op,fmt) fp_sqrt(SIM_ARGS, op, fmt)
unsigned64 fp_rsqrt (SIM_STATE, unsigned64 op, FP_formats fmt);
#define RSquareRoot(op,fmt) fp_rsqrt(SIM_ARGS, op, fmt)
unsigned64 fp_madd (SIM_STATE, unsigned64 op1, unsigned64 op2,
		    unsigned64 op3, FP_formats fmt);
#define MultiplyAdd(op1,op2,op3,fmt) fp_madd(SIM_ARGS, op1, op2, op3, fmt)
unsigned64 fp_msub (SIM_STATE, unsigned64 op1, unsigned64 op2,
		    unsigned64 op3, FP_formats fmt);
#define MultiplySub(op1,op2,op3,fmt) fp_msub(SIM_ARGS, op1, op2, op3, fmt)
unsigned64 fp_nmadd (SIM_STATE, unsigned64 op1, unsigned64 op2,
		     unsigned64 op3, FP_formats fmt);
#define NegMultiplyAdd(op1,op2,op3,fmt) fp_nmadd(SIM_ARGS, op1, op2, op3, fmt)
unsigned64 fp_nmsub (SIM_STATE, unsigned64 op1, unsigned64 op2,
		     unsigned64 op3, FP_formats fmt);
#define NegMultiplySub(op1,op2,op3,fmt) fp_nmsub(SIM_ARGS, op1, op2, op3, fmt)
unsigned64 convert (SIM_STATE, int rm, unsigned64 op, FP_formats from, FP_formats to);
#define Convert(rm,op,from,to) convert (SIM_ARGS, rm, op, from, to)
unsigned64 convert_ps (SIM_STATE, int rm, unsigned64 op, FP_formats from,
		       FP_formats to);
#define ConvertPS(rm,op,from,to) convert_ps (SIM_ARGS, rm, op, from, to)


/* MIPS-3D ASE operations.  */
#define CompareAbs(op1,op2,fmt,cond,cc) \
fp_cmp(SIM_ARGS, op1, op2, fmt, 1, cond, cc)
unsigned64 fp_add_r (SIM_STATE, unsigned64 op1, unsigned64 op2, FP_formats fmt);
#define AddR(op1,op2,fmt) fp_add_r(SIM_ARGS, op1, op2, fmt)
unsigned64 fp_mul_r (SIM_STATE, unsigned64 op1, unsigned64 op2, FP_formats fmt);
#define MultiplyR(op1,op2,fmt) fp_mul_r(SIM_ARGS, op1, op2, fmt)
unsigned64 fp_recip1 (SIM_STATE, unsigned64 op, FP_formats fmt);
#define Recip1(op,fmt) fp_recip1(SIM_ARGS, op, fmt)
unsigned64 fp_recip2 (SIM_STATE, unsigned64 op1, unsigned64 op2, FP_formats fmt);
#define Recip2(op1,op2,fmt) fp_recip2(SIM_ARGS, op1, op2, fmt)
unsigned64 fp_rsqrt1 (SIM_STATE, unsigned64 op, FP_formats fmt);
#define RSquareRoot1(op,fmt) fp_rsqrt1(SIM_ARGS, op, fmt)
unsigned64 fp_rsqrt2 (SIM_STATE, unsigned64 op1, unsigned64 op2, FP_formats fmt);
#define RSquareRoot2(op1,op2,fmt) fp_rsqrt2(SIM_ARGS, op1, op2, fmt)


/* MDMX access.  */

typedef unsigned int MX_fmtsel;   /* MDMX format select field (5 bits).  */
#define ob_fmtsel(sel) (((sel)<<1)|0x0)
#define qh_fmtsel(sel) (((sel)<<2)|0x1)

#define fmt_mdmx fmt_uninterpreted

#define MX_VECT_AND  (0)
#define MX_VECT_NOR  (1)
#define MX_VECT_OR   (2)
#define MX_VECT_XOR  (3)
#define MX_VECT_SLL  (4)
#define MX_VECT_SRL  (5)
#define MX_VECT_ADD  (6)
#define MX_VECT_SUB  (7)
#define MX_VECT_MIN  (8)
#define MX_VECT_MAX  (9)
#define MX_VECT_MUL  (10)
#define MX_VECT_MSGN (11)
#define MX_VECT_SRA  (12)
#define MX_VECT_ABSD (13)		/* SB-1 only.  */
#define MX_VECT_AVG  (14)		/* SB-1 only.  */

unsigned64 mdmx_cpr_op (SIM_STATE, int op, unsigned64 op1, int vt, MX_fmtsel fmtsel);
#define MX_Add(op1,vt,fmtsel) mdmx_cpr_op(SIM_ARGS, MX_VECT_ADD, op1, vt, fmtsel)
#define MX_And(op1,vt,fmtsel) mdmx_cpr_op(SIM_ARGS, MX_VECT_AND, op1, vt, fmtsel)
#define MX_Max(op1,vt,fmtsel) mdmx_cpr_op(SIM_ARGS, MX_VECT_MAX, op1, vt, fmtsel)
#define MX_Min(op1,vt,fmtsel) mdmx_cpr_op(SIM_ARGS, MX_VECT_MIN, op1, vt, fmtsel)
#define MX_Msgn(op1,vt,fmtsel) mdmx_cpr_op(SIM_ARGS, MX_VECT_MSGN, op1, vt, fmtsel)
#define MX_Mul(op1,vt,fmtsel) mdmx_cpr_op(SIM_ARGS, MX_VECT_MUL, op1, vt, fmtsel)
#define MX_Nor(op1,vt,fmtsel) mdmx_cpr_op(SIM_ARGS, MX_VECT_NOR, op1, vt, fmtsel)
#define MX_Or(op1,vt,fmtsel)  mdmx_cpr_op(SIM_ARGS, MX_VECT_OR,  op1, vt, fmtsel)
#define MX_ShiftLeftLogical(op1,vt,fmtsel) mdmx_cpr_op(SIM_ARGS, MX_VECT_SLL, op1, vt, fmtsel)
#define MX_ShiftRightArith(op1,vt,fmtsel) mdmx_cpr_op(SIM_ARGS, MX_VECT_SRA, op1, vt, fmtsel)
#define MX_ShiftRightLogical(op1,vt,fmtsel) mdmx_cpr_op(SIM_ARGS, MX_VECT_SRL, op1, vt, fmtsel)
#define MX_Sub(op1,vt,fmtsel) mdmx_cpr_op(SIM_ARGS, MX_VECT_SUB, op1, vt, fmtsel)
#define MX_Xor(op1,vt,fmtsel) mdmx_cpr_op(SIM_ARGS, MX_VECT_XOR, op1, vt, fmtsel)
#define MX_AbsDiff(op1,vt,fmtsel) mdmx_cpr_op(SIM_ARGS, MX_VECT_ABSD, op1, vt, fmtsel)
#define MX_Avg(op1,vt,fmtsel) mdmx_cpr_op(SIM_ARGS, MX_VECT_AVG, op1, vt, fmtsel)

#define MX_C_EQ  0x1
#define MX_C_LT  0x4

void mdmx_cc_op (SIM_STATE, int cond, unsigned64 op1, int vt, MX_fmtsel fmtsel);
#define MX_Comp(op1,cond,vt,fmtsel) mdmx_cc_op(SIM_ARGS, cond, op1, vt, fmtsel)

unsigned64 mdmx_pick_op (SIM_STATE, int tf, unsigned64 op1, int vt, MX_fmtsel fmtsel);
#define MX_Pick(tf,op1,vt,fmtsel) mdmx_pick_op(SIM_ARGS, tf, op1, vt, fmtsel)

#define MX_VECT_ADDA  (0)
#define MX_VECT_ADDL  (1)
#define MX_VECT_MULA  (2)
#define MX_VECT_MULL  (3)
#define MX_VECT_MULS  (4)
#define MX_VECT_MULSL (5)
#define MX_VECT_SUBA  (6)
#define MX_VECT_SUBL  (7)
#define MX_VECT_ABSDA (8)		/* SB-1 only.  */

void mdmx_acc_op (SIM_STATE, int op, unsigned64 op1, int vt, MX_fmtsel fmtsel);
#define MX_AddA(op1,vt,fmtsel) mdmx_acc_op(SIM_ARGS, MX_VECT_ADDA, op1, vt, fmtsel)
#define MX_AddL(op1,vt,fmtsel) mdmx_acc_op(SIM_ARGS, MX_VECT_ADDL, op1, vt, fmtsel)
#define MX_MulA(op1,vt,fmtsel) mdmx_acc_op(SIM_ARGS, MX_VECT_MULA, op1, vt, fmtsel)
#define MX_MulL(op1,vt,fmtsel) mdmx_acc_op(SIM_ARGS, MX_VECT_MULL, op1, vt, fmtsel)
#define MX_MulS(op1,vt,fmtsel) mdmx_acc_op(SIM_ARGS, MX_VECT_MULS, op1, vt, fmtsel)
#define MX_MulSL(op1,vt,fmtsel) mdmx_acc_op(SIM_ARGS, MX_VECT_MULSL, op1, vt, fmtsel)
#define MX_SubA(op1,vt,fmtsel) mdmx_acc_op(SIM_ARGS, MX_VECT_SUBA, op1, vt, fmtsel)
#define MX_SubL(op1,vt,fmtsel) mdmx_acc_op(SIM_ARGS, MX_VECT_SUBL, op1, vt, fmtsel)
#define MX_AbsDiffC(op1,vt,fmtsel) mdmx_acc_op(SIM_ARGS, MX_VECT_ABSDA, op1, vt, fmtsel)

#define MX_FMT_OB   (0)
#define MX_FMT_QH   (1)

/* The following codes chosen to indicate the units of shift.  */
#define MX_RAC_L    (0)
#define MX_RAC_M    (1)
#define MX_RAC_H    (2)

unsigned64 mdmx_rac_op (SIM_STATE, int, int);
#define MX_RAC(op,fmt) mdmx_rac_op(SIM_ARGS, op, fmt)

void mdmx_wacl (SIM_STATE, int, unsigned64, unsigned64);
#define MX_WACL(fmt,vs,vt) mdmx_wacl(SIM_ARGS, fmt, vs, vt)
void mdmx_wach (SIM_STATE, int, unsigned64);
#define MX_WACH(fmt,vs) mdmx_wach(SIM_ARGS, fmt, vs)

#define MX_RND_AS   (0)
#define MX_RND_AU   (1)
#define MX_RND_ES   (2)
#define MX_RND_EU   (3)
#define MX_RND_ZS   (4)
#define MX_RND_ZU   (5)

unsigned64 mdmx_round_op (SIM_STATE, int, int, MX_fmtsel);
#define MX_RNAS(vt,fmt) mdmx_round_op(SIM_ARGS, MX_RND_AS, vt, fmt)
#define MX_RNAU(vt,fmt) mdmx_round_op(SIM_ARGS, MX_RND_AU, vt, fmt)
#define MX_RNES(vt,fmt) mdmx_round_op(SIM_ARGS, MX_RND_ES, vt, fmt)
#define MX_RNEU(vt,fmt) mdmx_round_op(SIM_ARGS, MX_RND_EU, vt, fmt)
#define MX_RZS(vt,fmt)  mdmx_round_op(SIM_ARGS, MX_RND_ZS, vt, fmt)
#define MX_RZU(vt,fmt)  mdmx_round_op(SIM_ARGS, MX_RND_ZU, vt, fmt)

unsigned64 mdmx_shuffle (SIM_STATE, int, unsigned64, unsigned64);
#define MX_SHFL(shop,op1,op2) mdmx_shuffle(SIM_ARGS, shop, op1, op2)



/* Memory accesses */

/* The following are generic to all versions of the MIPS architecture
   to date: */

/* Memory Access Types (for CCA): */
#define Uncached                (0)
#define CachedNoncoherent       (1)
#define CachedCoherent          (2)
#define Cached                  (3)

#define isINSTRUCTION   (1 == 0) /* FALSE */
#define isDATA          (1 == 1) /* TRUE */
#define isLOAD          (1 == 0) /* FALSE */
#define isSTORE         (1 == 1) /* TRUE */
#define isREAL          (1 == 0) /* FALSE */
#define isRAW           (1 == 1) /* TRUE */
/* The parameter HOST (isTARGET / isHOST) is ignored */
#define isTARGET        (1 == 0) /* FALSE */
/* #define isHOST          (1 == 1) TRUE */

/* The "AccessLength" specifications for Loads and Stores. NOTE: This
   is the number of bytes minus 1. */
#define AccessLength_BYTE       (0)
#define AccessLength_HALFWORD   (1)
#define AccessLength_TRIPLEBYTE (2)
#define AccessLength_WORD       (3)
#define AccessLength_QUINTIBYTE (4)
#define AccessLength_SEXTIBYTE  (5)
#define AccessLength_SEPTIBYTE  (6)
#define AccessLength_DOUBLEWORD (7)
#define AccessLength_QUADWORD   (15)

#define LOADDRMASK (WITH_TARGET_WORD_BITSIZE == 64 \
		    ? AccessLength_DOUBLEWORD /*7*/ \
		    : AccessLength_WORD /*3*/)
#define PSIZE (WITH_TARGET_ADDRESS_BITSIZE)


INLINE_SIM_MAIN (int) address_translation PARAMS ((SIM_DESC sd, sim_cpu *, address_word cia, address_word vAddr, int IorD, int LorS, address_word *pAddr, int *CCA, int raw));
#define AddressTranslation(vAddr,IorD,LorS,pAddr,CCA,host,raw) \
address_translation (SD, CPU, cia, vAddr, IorD, LorS, pAddr, CCA, raw)

INLINE_SIM_MAIN (void) load_memory PARAMS ((SIM_DESC sd, sim_cpu *cpu, address_word cia, uword64* memvalp, uword64* memval1p, int CCA, unsigned int AccessLength, address_word pAddr, address_word vAddr, int IorD));
#define LoadMemory(memvalp,memval1p,CCA,AccessLength,pAddr,vAddr,IorD,raw) \
load_memory (SD, CPU, cia, memvalp, memval1p, CCA, AccessLength, pAddr, vAddr, IorD)

INLINE_SIM_MAIN (void) store_memory PARAMS ((SIM_DESC sd, sim_cpu *cpu, address_word cia, int CCA, unsigned int AccessLength, uword64 MemElem, uword64 MemElem1, address_word pAddr, address_word vAddr));
#define StoreMemory(CCA,AccessLength,MemElem,MemElem1,pAddr,vAddr,raw) \
store_memory (SD, CPU, cia, CCA, AccessLength, MemElem, MemElem1, pAddr, vAddr)

INLINE_SIM_MAIN (void) cache_op PARAMS ((SIM_DESC sd, sim_cpu *cpu, address_word cia, int op, address_word pAddr, address_word vAddr, unsigned int instruction));
#define CacheOp(op,pAddr,vAddr,instruction) \
cache_op (SD, CPU, cia, op, pAddr, vAddr, instruction)

INLINE_SIM_MAIN (void) sync_operation PARAMS ((SIM_DESC sd, sim_cpu *cpu, address_word cia, int stype));
#define SyncOperation(stype) \
sync_operation (SD, CPU, cia, (stype))

INLINE_SIM_MAIN (void) prefetch PARAMS ((SIM_DESC sd, sim_cpu *cpu, address_word cia, int CCA, address_word pAddr, address_word vAddr, int DATA, int hint));
#define Prefetch(CCA,pAddr,vAddr,DATA,hint) \
prefetch (SD, CPU, cia, CCA, pAddr, vAddr, DATA, hint)

void unpredictable_action (sim_cpu *cpu, address_word cia);
#define NotWordValue(val)	not_word_value (SD_, (val))
#define Unpredictable()		unpredictable (SD_)
#define UnpredictableResult()	/* For now, do nothing.  */

INLINE_SIM_MAIN (unsigned32) ifetch32 PARAMS ((SIM_DESC sd, sim_cpu *cpu, address_word cia, address_word vaddr));
#define IMEM32(CIA) ifetch32 (SD, CPU, (CIA), (CIA))
INLINE_SIM_MAIN (unsigned16) ifetch16 PARAMS ((SIM_DESC sd, sim_cpu *cpu, address_word cia, address_word vaddr));
#define IMEM16(CIA) ifetch16 (SD, CPU, (CIA), ((CIA) & ~1))
#define IMEM16_IMMED(CIA,NR) ifetch16 (SD, CPU, (CIA), ((CIA) & ~1) + 2 * (NR))

void dotrace PARAMS ((SIM_DESC sd, sim_cpu *cpu, FILE *tracefh, int type, SIM_ADDR address, int width, char *comment, ...));
extern FILE *tracefh;

extern int DSPLO_REGNUM[4];
extern int DSPHI_REGNUM[4];

INLINE_SIM_MAIN (void) pending_tick PARAMS ((SIM_DESC sd, sim_cpu *cpu, address_word cia));
extern SIM_CORE_SIGNAL_FN mips_core_signal;

char* pr_addr PARAMS ((SIM_ADDR addr));
char* pr_uword64 PARAMS ((uword64 addr));


#define GPR_CLEAR(N) do { GPR_SET((N),0); } while (0)

void mips_cpu_exception_trigger(SIM_DESC sd, sim_cpu* cpu, address_word pc);
void mips_cpu_exception_suspend(SIM_DESC sd, sim_cpu* cpu, int exception);
void mips_cpu_exception_resume(SIM_DESC sd, sim_cpu* cpu, int exception);

#ifdef MIPS_MACH_MULTI
extern int mips_mach_multi(SIM_DESC sd);
#define MIPS_MACH(SD)	mips_mach_multi(SD)
#else
#define	MIPS_MACH(SD)	MIPS_MACH_DEFAULT
#endif

/* Macros for determining whether a MIPS IV or MIPS V part is subject
   to the hi/lo restrictions described in mips.igen.  */

#define MIPS_MACH_HAS_MT_HILO_HAZARD(SD) \
  (MIPS_MACH (SD) != bfd_mach_mips5500)

#define MIPS_MACH_HAS_MULT_HILO_HAZARD(SD) \
  (MIPS_MACH (SD) != bfd_mach_mips5500)

#define MIPS_MACH_HAS_DIV_HILO_HAZARD(SD) \
  (MIPS_MACH (SD) != bfd_mach_mips5500)

#if H_REVEALS_MODULE_P (SIM_MAIN_INLINE)
#include "sim-main.c"
#endif

#endif
