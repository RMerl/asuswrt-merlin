/* Simulator header for cgen scache support.
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

#ifndef CGEN_SCACHE_H
#define CGEN_SCACHE_H

#ifndef WITH_SCACHE
#define WITH_SCACHE 0
#endif

/* When caching bb's, instructions are extracted into "chains".
   SCACHE_MAP is a hash table into these chains.  */

typedef struct {
  IADDR pc;
  SCACHE *sc;
} SCACHE_MAP;

typedef struct cpu_scache {
  /* Simulator cache size.  Must be a power of 2.
     This is the number of elements in the `cache' member.  */
  unsigned int size;
#define CPU_SCACHE_SIZE(cpu) ((cpu) -> cgen_cpu.scache.size)
  /* The cache.  */
  SCACHE *cache;
#define CPU_SCACHE_CACHE(cpu) ((cpu) -> cgen_cpu.scache.cache)

#if WITH_SCACHE_PBB
  /* Number of hash chains.  Must be a power of 2.  */
  unsigned int num_hash_chains;
#define CPU_SCACHE_NUM_HASH_CHAINS(cpu) ((cpu) -> cgen_cpu.scache.num_hash_chains)
  /* Number of entries in each hash chain.
     The hash table is a statically allocated NxM array where
     N = num_hash_chains
     M = num_hash_chain_entries.  */
  unsigned int num_hash_chain_entries;
#define CPU_SCACHE_NUM_HASH_CHAIN_ENTRIES(cpu) ((cpu) -> cgen_cpu.scache.num_hash_chain_entries)
  /* Maximum number of instructions in a chain.
     ??? This just let's us set a static size of chain_lengths table.
     In a simulation that handles more than just the cpu, this might also be
     used to keep too many instructions from being executed before checking
     for events (or some such).  */
  unsigned int max_chain_length;
#define CPU_SCACHE_MAX_CHAIN_LENGTH(cpu) ((cpu) -> cgen_cpu.scache.max_chain_length)
  /* Special scache entry for (re)starting bb extraction.  */
  SCACHE *pbb_begin;
#define CPU_SCACHE_PBB_BEGIN(cpu) ((cpu) -> cgen_cpu.scache.pbb_begin)
  /* Hash table into cached chains.  */
  SCACHE_MAP *hash_table;
#define CPU_SCACHE_HASH_TABLE(cpu) ((cpu) -> cgen_cpu.scache.hash_table)
  /* Next free entry in cache.  */
  SCACHE *next_free;
#define CPU_SCACHE_NEXT_FREE(cpu) ((cpu) -> cgen_cpu.scache.next_free)

  /* Kind of branch being taken.
     Only used by functional semantics, not switch form.  */
  SEM_BRANCH_TYPE pbb_br_type;
#define CPU_PBB_BR_TYPE(cpu) ((cpu) -> cgen_cpu.scache.pbb_br_type)
  /* Target's branch address.  */
  IADDR pbb_br_npc;
#define CPU_PBB_BR_NPC(cpu) ((cpu) -> cgen_cpu.scache.pbb_br_npc)
#endif /* WITH_SCACHE_PBB */

#if WITH_PROFILE_SCACHE_P
  /* Cache hits, misses.  */
  unsigned long hits, misses;
#define CPU_SCACHE_HITS(cpu) ((cpu) -> cgen_cpu.scache.hits)
#define CPU_SCACHE_MISSES(cpu) ((cpu) -> cgen_cpu.scache.misses)

#if WITH_SCACHE_PBB
  /* Chain length counts.
     Each element is a count of the number of chains created with that
     length.  */
  unsigned long *chain_lengths;
#define CPU_SCACHE_CHAIN_LENGTHS(cpu) ((cpu) -> cgen_cpu.scache.chain_lengths)
  /* Number of times cache was flushed due to its being full.  */
  unsigned long full_flushes;
#define CPU_SCACHE_FULL_FLUSHES(cpu) ((cpu) -> cgen_cpu.scache.full_flushes)
#endif
#endif
} CPU_SCACHE;

/* Hash a PC value.
   This is split into two parts to help with moving as much of the
   computation out of the main loop.  */
#define CPU_SCACHE_HASH_MASK(cpu) (CPU_SCACHE_SIZE (cpu) - 1)
#define SCACHE_HASH_PC(pc, mask) \
((CGEN_MIN_INSN_SIZE == 2 ? ((pc) >> 1) \
  : CGEN_MIN_INSN_SIZE == 4 ? ((pc) >> 2) \
  : (pc)) \
 & (mask))

/* Non-zero if cache is in use.  */
#define USING_SCACHE_P(sd) (STATE_SCACHE_SIZE (sd) > 0)

/* Install the simulator cache into the simulator.  */
MODULE_INSTALL_FN scache_install;

/* Lookup a PC value in the scache [compilation only].  */
extern SCACHE * scache_lookup (SIM_CPU *, IADDR);
/* Return a pointer to at least N buffers.  */
extern SCACHE *scache_lookup_or_alloc (SIM_CPU *, IADDR, int, SCACHE **);
/* Flush all cpu's scaches.  */
extern void scache_flush (SIM_DESC);
/* Flush a cpu's scache.  */
extern void scache_flush_cpu (SIM_CPU *);

/* Scache profiling support.  */

/* Print summary scache usage information.  */
extern void scache_print_profile (SIM_CPU *cpu, int verbose);

#if WITH_PROFILE_SCACHE_P

#define PROFILE_COUNT_SCACHE_HIT(cpu) \
do { \
  if (CPU_PROFILE_FLAGS (cpu) [PROFILE_SCACHE_IDX]) \
    ++ CPU_SCACHE_HITS (cpu); \
} while (0)
#define PROFILE_COUNT_SCACHE_MISS(cpu) \
do { \
  if (CPU_PROFILE_FLAGS (cpu) [PROFILE_SCACHE_IDX]) \
    ++ CPU_SCACHE_MISSES (cpu); \
} while (0)
#define PROFILE_COUNT_SCACHE_CHAIN_LENGTH(cpu,length) \
do { \
  if (CPU_PROFILE_FLAGS (cpu) [PROFILE_SCACHE_IDX]) \
    ++ CPU_SCACHE_CHAIN_LENGTHS (cpu) [length]; \
} while (0)
#define PROFILE_COUNT_SCACHE_FULL_FLUSH(cpu) \
do { \
  if (CPU_PROFILE_FLAGS (cpu) [PROFILE_SCACHE_IDX]) \
    ++ CPU_SCACHE_FULL_FLUSHES (cpu); \
} while (0)

#else

#define PROFILE_COUNT_SCACHE_HIT(cpu)
#define PROFILE_COUNT_SCACHE_MISS(cpu)
#define PROFILE_COUNT_SCACHE_CHAIN_LENGTH(cpu,length)
#define PROFILE_COUNT_SCACHE_FULL_FLUSH(cpu)

#endif

#endif /* CGEN_SCACHE_H */
