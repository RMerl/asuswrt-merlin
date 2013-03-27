/* Engine header for Cpu tools GENerated simulators.
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

/* This file must be included after eng.h and before ${cpu}.h.
   ??? A lot of this could be moved to genmloop.sh to be put in eng.h
   and thus remove some conditional compilation.  Worth it?  */

/* Semantic functions come in six versions on two axes:
   fast/full-featured, and using one of the simple/scache/compilation engines.
   A full featured simulator is always provided.  --enable-sim-fast includes
   support for fast execution by duplicating the semantic code but leaving
   out all features like tracing and profiling.
   Using the scache is selected with --enable-sim-scache.  */
/* FIXME: --enable-sim-fast not implemented yet.  */
/* FIXME: undecided how to handle WITH_SCACHE_PBB.  */

/* There are several styles of engines, all generally supported by the
   same code:

   WITH_SCACHE && WITH_SCACHE_PBB - pseudo-basic-block scaching
   WITH_SCACHE && !WITH_SCACHE_PBB - scaching on an insn by insn basis
   !WITH_SCACHE - simple engine: fetch an insn, execute an insn

   The !WITH_SCACHE case can also be broken up into two flavours:
   extract the fields of the insn into an ARGBUF struct, or defer the
   extraction to the semantic handler.  The former can be viewed as the
   WITH_SCACHE case with a cache size of 1 (thus there's no need for a
   WITH_EXTRACTION macro).  The WITH_SCACHE case always extracts the fields
   into an ARGBUF struct.  */

#ifndef CGEN_ENGINE_H
#define CGEN_ENGINE_H

/* Instruction field support macros.  */

#define EXTRACT_MSB0_INT(val, total, start, length) \
(((INT) (val) << ((sizeof (INT) * 8) - (total) + (start))) \
 >> ((sizeof (INT) * 8) - (length)))
#define EXTRACT_MSB0_UINT(val, total, start, length) \
(((UINT) (val) << ((sizeof (UINT) * 8) - (total) + (start))) \
 >> ((sizeof (UINT) * 8) - (length)))

#define EXTRACT_LSB0_INT(val, total, start, length) \
(((INT) (val) << ((sizeof (INT) * 8) - (start) - 1)) \
 >> ((sizeof (INT) * 8) - (length)))
#define EXTRACT_LSB0_UINT(val, total, start, length) \
(((UINT) (val) << ((sizeof (UINT) * 8) - (start) - 1)) \
 >> ((sizeof (UINT) * 8) - (length)))

/* Semantic routines.  */

/* Type of the machine generated extraction fns.  */
/* ??? No longer used.  */
typedef void (EXTRACT_FN) (SIM_CPU *, IADDR, CGEN_INSN_INT, ARGBUF *);

/* Type of the machine generated semantic fns.  */

#if WITH_SCACHE

/* Instruction fields are extracted into ARGBUF before calling the
   semantic routine.  */
#if HAVE_PARALLEL_INSNS && ! WITH_PARALLEL_GENWRITE
typedef SEM_PC (SEMANTIC_FN) (SIM_CPU *, SEM_ARG, PAREXEC *);
#else
typedef SEM_PC (SEMANTIC_FN) (SIM_CPU *, SEM_ARG);
#endif

#else

/* Result of semantic routines is a status indicator (wip).  */
typedef unsigned int SEM_STATUS;

/* Instruction fields are extracted by the semantic routine.
   ??? TODO: multi word insns.  */
#if HAVE_PARALLEL_INSNS && ! WITH_PARALLEL_GENWRITE
typedef SEM_STATUS (SEMANTIC_FN) (SIM_CPU *, SEM_ARG, PAREXEC *, CGEN_INSN_INT);
#else
typedef SEM_STATUS (SEMANTIC_FN) (SIM_CPU *, SEM_ARG, CGEN_INSN_INT);
#endif

#endif

/* In the ARGBUF struct, a pointer to the semantic routine for the insn.  */

union sem {
#if ! WITH_SEM_SWITCH_FULL
  SEMANTIC_FN *sem_full;
#endif
#if ! WITH_SEM_SWITCH_FAST
  SEMANTIC_FN *sem_fast;
#endif
#if WITH_SEM_SWITCH_FULL || WITH_SEM_SWITCH_FAST
#ifdef __GNUC__
  void *sem_case;
#else
  int sem_case;
#endif
#endif
};

/* Set the appropriate semantic handler in ABUF.  */

#if WITH_SEM_SWITCH_FULL
#ifdef __GNUC__
#define SEM_SET_FULL_CODE(abuf, idesc) \
  do { (abuf)->semantic.sem_case = (idesc)->sem_full_lab; } while (0)
#else 
#define SEM_SET_FULL_CODE(abuf, idesc) \
  do { (abuf)->semantic.sem_case = (idesc)->num; } while (0)
#endif
#else
#define SEM_SET_FULL_CODE(abuf, idesc) \
  do { (abuf)->semantic.sem_full = (idesc)->sem_full; } while (0)
#endif

#if WITH_SEM_SWITCH_FAST
#ifdef __GNUC__
#define SEM_SET_FAST_CODE(abuf, idesc) \
  do { (abuf)->semantic.sem_case = (idesc)->sem_fast_lab; } while (0)
#else 
#define SEM_SET_FAST_CODE(abuf, idesc) \
  do { (abuf)->semantic.sem_case = (idesc)->num; } while (0)
#endif
#else
#define SEM_SET_FAST_CODE(abuf, idesc) \
  do { (abuf)->semantic.sem_fast = (idesc)->sem_fast; } while (0)
#endif

#define SEM_SET_CODE(abuf, idesc, fast_p) \
do { \
  if (fast_p) \
    SEM_SET_FAST_CODE ((abuf), (idesc)); \
  else \
    SEM_SET_FULL_CODE ((abuf), (idesc)); \
} while (0)

/* Return non-zero if IDESC is a conditional or unconditional CTI.  */

#define IDESC_CTI_P(idesc) \
     ((CGEN_ATTR_BOOLS (CGEN_INSN_ATTRS ((idesc)->idata)) \
       & (CGEN_ATTR_MASK (CGEN_INSN_COND_CTI) \
	  | CGEN_ATTR_MASK (CGEN_INSN_UNCOND_CTI))) \
      != 0)

/* Return non-zero if IDESC is a skip insn.  */

#define IDESC_SKIP_P(idesc) \
     ((CGEN_ATTR_BOOLS (CGEN_INSN_ATTRS ((idesc)->idata)) \
       & CGEN_ATTR_MASK (CGEN_INSN_SKIP_CTI)) \
      != 0)

/* Return pointer to ARGBUF given ptr to SCACHE.  */
#define SEM_ARGBUF(sem_arg) (& (sem_arg) -> argbuf)

/* There are several styles of engines, all generally supported by the
   same code:

   WITH_SCACHE && WITH_SCACHE_PBB - pseudo-basic-block scaching
   WITH_SCACHE && !WITH_SCACHE_PBB - scaching on an insn by insn basis
   !WITH_SCACHE - simple engine: fetch an insn, execute an insn

   ??? The !WITH_SCACHE case can also be broken up into two flavours:
   extract the fields of the insn into an ARGBUF struct, or defer the
   extraction to the semantic handler.  The WITH_SCACHE case always
   extracts the fields into an ARGBUF struct.  */

#if WITH_SCACHE

#define CIA_ADDR(cia) (cia)

#if WITH_SCACHE_PBB

/* Return the scache pointer of the current insn.  */
#define SEM_SEM_ARG(vpc, sc) (vpc)

/* Return the virtual pc of the next insn to execute
   (assuming this isn't a cti or the branch isn't taken).  */
#define SEM_NEXT_VPC(sem_arg, pc, len) ((sem_arg) + 1)

/* Update the instruction counter.  */
#define PBB_UPDATE_INSN_COUNT(cpu,sc) \
  (CPU_INSN_COUNT (cpu) += SEM_ARGBUF (sc) -> fields.chain.insn_count)

/* Do not append a `;' to invocations of this.
   npc,br_type are for communication between the cti insn and cti-chain.  */
#define SEM_BRANCH_INIT \
  IADDR npc = 0; /* assign a value for -Wall */ \
  SEM_BRANCH_TYPE br_type = SEM_BRANCH_UNTAKEN;

/* SEM_IN_SWITCH is defined at the top of the mainloop.c files
   generated by genmloop.sh.  It exists so generated semantic code needn't
   care whether it's being put in a switch or in a function.  */
#ifdef SEM_IN_SWITCH
#define SEM_BRANCH_FINI(pcvar) \
do { \
  pbb_br_npc = npc; \
  pbb_br_type = br_type; \
} while (0)
#else /* 1 semantic function per instruction */
#define SEM_BRANCH_FINI(pcvar) \
do { \
  CPU_PBB_BR_NPC (current_cpu) = npc; \
  CPU_PBB_BR_TYPE (current_cpu) = br_type; \
} while (0)
#endif

#define SEM_BRANCH_VIA_CACHE(cpu, sc, newval, pcvar) \
do { \
  npc = (newval); \
  br_type = SEM_BRANCH_CACHEABLE; \
} while (0)

#define SEM_BRANCH_VIA_ADDR(cpu, sc, newval, pcvar) \
do { \
  npc = (newval); \
  br_type = SEM_BRANCH_UNCACHEABLE; \
} while (0)

#define SEM_SKIP_COMPILE(cpu, sc, skip) \
do { \
  SEM_ARGBUF (sc) -> skip_count = (skip); \
} while (0)

#define SEM_SKIP_INSN(cpu, sc, vpcvar) \
do { \
  (vpcvar) += SEM_ARGBUF (sc) -> skip_count; \
} while (0)

#else /* ! WITH_SCACHE_PBB */

#define SEM_SEM_ARG(vpc, sc) (sc)

#define SEM_NEXT_VPC(sem_arg, pc, len) ((pc) + (len))

/* ??? May wish to move taken_p out of here and make it explicit.  */
#define SEM_BRANCH_INIT \
  int taken_p = 0;

#ifndef TARGET_SEM_BRANCH_FINI
#define TARGET_SEM_BRANCH_FINI(pcvar, taken_p)
#endif
#define SEM_BRANCH_FINI(pcvar) \
  do { TARGET_SEM_BRANCH_FINI (pcvar, taken_p); } while (0)

#define SEM_BRANCH_VIA_CACHE(cpu, sc, newval, pcvar) \
do { \
  (pcvar) = (newval); \
  taken_p = 1; \
} while (0)

#define SEM_BRANCH_VIA_ADDR(cpu, sc, newval, pcvar) \
do { \
  (pcvar) = (newval); \
  taken_p = 1; \
} while (0)

#endif /* ! WITH_SCACHE_PBB */

#else /* ! WITH_SCACHE */

/* This is the "simple" engine case.  */

#define CIA_ADDR(cia) (cia)

#define SEM_SEM_ARG(vpc, sc) (sc)

#define SEM_NEXT_VPC(sem_arg, pc, len) ((pc) + (len))

#define SEM_BRANCH_INIT \
  int taken_p = 0;

#define SEM_BRANCH_VIA_CACHE(cpu, abuf, newval, pcvar) \
do { \
  (pcvar) = (newval); \
  taken_p = 1; \
} while (0)

#define SEM_BRANCH_VIA_ADDR(cpu, abuf, newval, pcvar) \
do { \
  (pcvar) = (newval); \
  taken_p = 1; \
} while (0)

/* Finish off branch insns.
   The target must define TARGET_SEM_BRANCH_FINI.
   ??? This can probably go away when define-execute is finished.  */
#define SEM_BRANCH_FINI(pcvar, bool_attrs) \
  do { TARGET_SEM_BRANCH_FINI ((pcvar), (bool_attrs), taken_p); } while (0)

/* Finish off non-branch insns.
   The target must define TARGET_SEM_NBRANCH_FINI.
   ??? This can probably go away when define-execute is finished.  */
#define SEM_NBRANCH_FINI(pcvar, bool_attrs) \
  do { TARGET_SEM_NBRANCH_FINI ((pcvar), (bool_attrs)); } while (0)

#endif /* ! WITH_SCACHE */

/* Instruction information.  */

/* Sanity check, at most one of these may be true.  */
#if WITH_PARALLEL_READ + WITH_PARALLEL_WRITE + WITH_PARALLEL_GENWRITE > 1
#error "At most one of WITH_PARALLEL_{READ,WRITE,GENWRITE} can be true."
#endif

/* Compile time computable instruction data.  */

struct insn_sem {
  /* The instruction type (a number that identifies each insn over the
     entire architecture).  */
  CGEN_INSN_TYPE type;

  /* Index in IDESC table.  */
  int index;

  /* Semantic format number.  */
  int sfmt;

#if HAVE_PARALLEL_INSNS && ! WITH_PARALLEL_ONLY
  /* Index in IDESC table of parallel handler.  */
  int par_index;
#endif

#if WITH_PARALLEL_READ
  /* Index in IDESC table of read handler.  */
  int read_index;
#endif

#if WITH_PARALLEL_WRITE
  /* Index in IDESC table of writeback handler.  */
  int write_index;
#endif
};

/* Entry in semantic function table.
   This information is copied to the insn descriptor table at run-time.  */

struct sem_fn_desc {
  /* Index in IDESC table.  */
  int index;

  /* Function to perform the semantics of the insn.  */
  SEMANTIC_FN *fn;
};

/* Run-time computed instruction descriptor.  */

struct idesc {
#if WITH_SEM_SWITCH_FAST
#ifdef __GNUC__
  void *sem_fast_lab;
#else
  /* nothing needed, switch's on `num' member */
#endif
#else
  SEMANTIC_FN *sem_fast;
#endif

#if WITH_SEM_SWITCH_FULL
#ifdef __GNUC__
  void *sem_full_lab;
#else
  /* nothing needed, switch's on `num' member */
#endif
#else
  SEMANTIC_FN *sem_full;
#endif

  /* Parallel support.  */
#if HAVE_PARALLEL_INSNS && (! WITH_PARALLEL_ONLY || (WITH_PARALLEL_ONLY && ! WITH_PARALLEL_GENWRITE))
  /* Pointer to parallel handler if serial insn.
     Pointer to readahead/writeback handler if parallel insn.  */
  struct idesc *par_idesc;
#endif

  /* Instruction number (index in IDESC table, profile table).
     Also used to switch on in non-gcc semantic switches.  */
  int num;

  /* Semantic format id.  */
  int sfmt;

  /* instruction data (name, attributes, size, etc.) */
  const CGEN_INSN *idata;

  /* instruction attributes, copied from `idata' for speed */
  const CGEN_INSN_ATTR_TYPE *attrs;

  /* instruction length in bytes, copied from `idata' for speed */
  int length;

  /* profiling/modelling support */
  const INSN_TIMING *timing;
};

/* Tracing/profiling.  */

/* Return non-zero if a before/after handler is needed.
   When tracing/profiling a selected range there's no need to slow
   down simulation of the other insns (except to get more accurate data!).

   ??? May wish to profile all insns if doing insn tracing, or to
   get more accurate cycle data.

   First test ANY_P so we avoid a potentially expensive HIT_P call
   [if there are lots of address ranges].  */

#define PC_IN_TRACE_RANGE_P(cpu, pc) \
  (TRACE_ANY_P (cpu) \
   && ADDR_RANGE_HIT_P (TRACE_RANGE (CPU_TRACE_DATA (cpu)), (pc)))
#define PC_IN_PROFILE_RANGE_P(cpu, pc) \
  (PROFILE_ANY_P (cpu) \
   && ADDR_RANGE_HIT_P (PROFILE_RANGE (CPU_PROFILE_DATA (cpu)), (pc)))

#endif /* CGEN_ENGINE_H */
