/* Simulator cache routines for CGEN simulators (and maybe others).
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

#define SCACHE_DEFINE_INLINE

#include "sim-main.h"
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include "libiberty.h"
#include "sim-options.h"
#include "sim-io.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))

/* Unused address.  */
#define UNUSED_ADDR 0xffffffff

/* Scache configuration parameters.
   ??? Experiments to determine reasonable values is wip.
   These are just guesses.  */

/* Default number of scache elements.
   The size of an element is typically 32-64 bytes, so the size of the
   default scache will be between 512K and 1M bytes.  */
#ifdef CONFIG_SIM_CACHE_SIZE
#define SCACHE_DEFAULT_CACHE_SIZE CONFIG_SIM_CACHE_SIZE
#else
#define SCACHE_DEFAULT_CACHE_SIZE 16384
#endif

/* Minimum cache size.
   The m32r port assumes a cache size of at least 2 so it can decode both 16
   bit insns.  When compiling we need an extra for the chain entry.  And this
   must be a multiple of 2.  Hence 4 is the minimum (though, for those with
   featuritis or itchy pedantic bits, we could make this conditional on
   WITH_SCACHE_PBB).  */
#define MIN_SCACHE_SIZE 4

/* Ratio of size of text section to size of scache.
   When compiling, we don't want to flush the scache more than we have to
   but we also don't want it to be exorbitantly(sp?) large.  So we pick a high
   default value, then reduce it by the size of the program being simulated,
   but we don't override any value specified on the command line.
   If not specified on the command line, the size to use is computed as
   max (MIN_SCACHE_SIZE,
        min (DEFAULT_SCACHE_SIZE,
             text_size / (base_insn_size * INSN_SCACHE_RATIO))).  */
/* ??? Interesting idea but not currently used.  */
#define INSN_SCACHE_RATIO 4

/* Default maximum insn chain length.
   The only reason for a maximum is so we can place a maximum size on the
   profiling table.  Chain lengths are determined by cti's.
   32 is a more reasonable number, but when profiling, the before/after
   handlers take up that much more space.  The scache is filled from front to
   back so all this determines is when the scache needs to be flushed.  */
#define MAX_CHAIN_LENGTH 64

/* Default maximum hash list length.  */
#define MAX_HASH_CHAIN_LENGTH 4

/* Minimum hash table size.  */
#define MIN_HASH_CHAINS 32

/* Ratio of number of scache elements to number of hash lists.
   Since the user can only specify the size of the scache, we compute the
   size of the hash table as
   max (MIN_HASH_CHAINS, scache_size / SCACHE_HASH_RATIO).  */
#define SCACHE_HASH_RATIO 8

/* Hash a PC value.
   FIXME: May wish to make the hashing architecture specific.
   FIXME: revisit */
#define HASH_PC(pc) (((pc) >> 2) + ((pc) >> 5))

static MODULE_INIT_FN scache_init;
static MODULE_UNINSTALL_FN scache_uninstall;

static DECLARE_OPTION_HANDLER (scache_option_handler);

#define OPTION_PROFILE_SCACHE	(OPTION_START + 0)

static const OPTION scache_options[] = {
  { {"scache-size", optional_argument, NULL, 'c'},
      'c', "[SIZE]", "Specify size of simulator execution cache",
      scache_option_handler },
#if WITH_SCACHE_PBB
  /* ??? It might be nice to allow the user to specify the size of the hash
     table, the maximum hash list length, and the maximum chain length, but
     for now that might be more akin to featuritis.  */
#endif
  { {"profile-scache", optional_argument, NULL, OPTION_PROFILE_SCACHE},
      '\0', "on|off", "Perform simulator execution cache profiling",
      scache_option_handler },
  { {NULL, no_argument, NULL, 0}, '\0', NULL, NULL, NULL }
};

static SIM_RC
scache_option_handler (SIM_DESC sd, sim_cpu *cpu, int opt,
		       char *arg, int is_command)
{
  switch (opt)
    {
    case 'c' :
      if (WITH_SCACHE)
	{
	  if (arg != NULL)
	    {
	      int n = strtol (arg, NULL, 0);
	      if (n < MIN_SCACHE_SIZE)
		{
		  sim_io_eprintf (sd, "invalid scache size `%d', must be at least 4", n);
		  return SIM_RC_FAIL;
		}
	      /* Ensure it's a multiple of 2.  */
	      if ((n & (n - 1)) != 0)
		{
		  sim_io_eprintf (sd, "scache size `%d' not a multiple of 2\n", n);
		  {
		    /* round up to nearest multiple of 2 */
		    int i;
		    for (i = 1; i < n; i <<= 1)
		      continue;
		    n = i;
		  }
		  sim_io_eprintf (sd, "rounding scache size up to %d\n", n);
		}
	      if (cpu == NULL)
		STATE_SCACHE_SIZE (sd) = n;
	      else
		CPU_SCACHE_SIZE (cpu) = n;
	    }
	  else
	    {
	      if (cpu == NULL)
		STATE_SCACHE_SIZE (sd) = SCACHE_DEFAULT_CACHE_SIZE;
	      else
		CPU_SCACHE_SIZE (cpu) = SCACHE_DEFAULT_CACHE_SIZE;
	    }
	}
      else
	sim_io_eprintf (sd, "Simulator execution cache not enabled, `--scache-size' ignored\n");
      break;

    case OPTION_PROFILE_SCACHE :
      if (WITH_SCACHE && WITH_PROFILE_SCACHE_P)
	{
	  /* FIXME: handle cpu != NULL.  */
	  return sim_profile_set_option (sd, "-scache", PROFILE_SCACHE_IDX,
					 arg);
	}
      else
	sim_io_eprintf (sd, "Simulator cache profiling not compiled in, `--profile-scache' ignored\n");
      break;
    }

  return SIM_RC_OK;
}

SIM_RC
scache_install (SIM_DESC sd)
{
  sim_add_option_table (sd, NULL, scache_options);
  sim_module_add_init_fn (sd, scache_init);
  sim_module_add_uninstall_fn (sd, scache_uninstall);

  /* This is the default, it may be overridden on the command line.  */
  STATE_SCACHE_SIZE (sd) = WITH_SCACHE;

  return SIM_RC_OK;
}

static SIM_RC
scache_init (SIM_DESC sd)
{
  int c;

  for (c = 0; c < MAX_NR_PROCESSORS; ++c)
    {
      SIM_CPU *cpu = STATE_CPU (sd, c);
      int elm_size = IMP_PROPS_SCACHE_ELM_SIZE (MACH_IMP_PROPS (CPU_MACH (cpu)));

      /* elm_size is 0 if the cpu doesn't not have scache support */
      if (elm_size == 0)
	{
	  CPU_SCACHE_SIZE (cpu) = 0;
	  CPU_SCACHE_CACHE (cpu) = NULL;
	}
      else
	{
	  if (CPU_SCACHE_SIZE (cpu) == 0)
	    CPU_SCACHE_SIZE (cpu) = STATE_SCACHE_SIZE (sd);
	  CPU_SCACHE_CACHE (cpu) =
	    (SCACHE *) xmalloc (CPU_SCACHE_SIZE (cpu) * elm_size);
#if WITH_SCACHE_PBB
	  CPU_SCACHE_MAX_CHAIN_LENGTH (cpu) = MAX_CHAIN_LENGTH;
	  CPU_SCACHE_NUM_HASH_CHAIN_ENTRIES (cpu) = MAX_HASH_CHAIN_LENGTH;
	  CPU_SCACHE_NUM_HASH_CHAINS (cpu) = MAX (MIN_HASH_CHAINS,
						  CPU_SCACHE_SIZE (cpu)
						  / SCACHE_HASH_RATIO);
	  CPU_SCACHE_HASH_TABLE (cpu) =
	    (SCACHE_MAP *) xmalloc (CPU_SCACHE_NUM_HASH_CHAINS (cpu)
				    * CPU_SCACHE_NUM_HASH_CHAIN_ENTRIES (cpu)
				    * sizeof (SCACHE_MAP));
	  CPU_SCACHE_PBB_BEGIN (cpu) = (SCACHE *) zalloc (elm_size);
	  CPU_SCACHE_CHAIN_LENGTHS (cpu) =
	    (unsigned long *) zalloc ((CPU_SCACHE_MAX_CHAIN_LENGTH (cpu) + 1)
				      * sizeof (long));
#endif
	}
    }

  scache_flush (sd);

  return SIM_RC_OK;
}

static void
scache_uninstall (SIM_DESC sd)
{
  int c;

  for (c = 0; c < MAX_NR_PROCESSORS; ++c)
    {
      SIM_CPU *cpu = STATE_CPU (sd, c);

      if (CPU_SCACHE_CACHE (cpu) != NULL)
	free (CPU_SCACHE_CACHE (cpu));
#if WITH_SCACHE_PBB
      if (CPU_SCACHE_HASH_TABLE (cpu) != NULL)
	free (CPU_SCACHE_HASH_TABLE (cpu));
      if (CPU_SCACHE_PBB_BEGIN (cpu) != NULL)
	free (CPU_SCACHE_PBB_BEGIN (cpu));
      if (CPU_SCACHE_CHAIN_LENGTHS (cpu) != NULL)
	free (CPU_SCACHE_CHAIN_LENGTHS (cpu));
#endif
    }
}

void
scache_flush (SIM_DESC sd)
{
  int c;

  for (c = 0; c < MAX_NR_PROCESSORS; ++c)
    {
      SIM_CPU *cpu = STATE_CPU (sd, c);
      scache_flush_cpu (cpu);
    }
}

void
scache_flush_cpu (SIM_CPU *cpu)
{
  int i,n;

  /* Don't bother if cache not in use.  */
  if (CPU_SCACHE_SIZE (cpu) == 0)
    return;

#if WITH_SCACHE_PBB
  /* It's important that this be reasonably fast as this can be done when
     the simulation is running.  */
  CPU_SCACHE_NEXT_FREE (cpu) = CPU_SCACHE_CACHE (cpu);
  n = CPU_SCACHE_NUM_HASH_CHAINS (cpu) * CPU_SCACHE_NUM_HASH_CHAIN_ENTRIES (cpu);
  /* ??? Might be faster to just set the first entry, then update the
     "last entry" marker during allocation.  */
  for (i = 0; i < n; ++i)
    CPU_SCACHE_HASH_TABLE (cpu) [i] . pc = UNUSED_ADDR;
#else
  {
    int elm_size = IMP_PROPS_SCACHE_ELM_SIZE (MACH_IMP_PROPS (CPU_MACH (cpu)));
    SCACHE *sc;

    /* Technically, this may not be necessary, but it helps debugging.  */
    memset (CPU_SCACHE_CACHE (cpu), 0,
	    CPU_SCACHE_SIZE (cpu) * elm_size);

    for (i = 0, sc = CPU_SCACHE_CACHE (cpu); i < CPU_SCACHE_SIZE (cpu);
	 ++i, sc = (SCACHE *) ((char *) sc + elm_size))
      {
	sc->argbuf.addr = UNUSED_ADDR;
      }
  }
#endif
}

#if WITH_SCACHE_PBB

/* Look up PC in the hash table of scache entry points.
   Returns the entry or NULL if not found.  */

SCACHE *
scache_lookup (SIM_CPU *cpu, IADDR pc)
{
  /* FIXME: hash computation is wrong, doesn't take into account
     NUM_HASH_CHAIN_ENTRIES.  A lot of the hash table will be unused!  */
  unsigned int slot = HASH_PC (pc) & (CPU_SCACHE_NUM_HASH_CHAINS (cpu) - 1);
  int i, max_i = CPU_SCACHE_NUM_HASH_CHAIN_ENTRIES (cpu);
  SCACHE_MAP *scm;

  /* We don't update hit/miss statistics as this is only used when recording
     branch target addresses.  */

  scm = & CPU_SCACHE_HASH_TABLE (cpu) [slot];
  for (i = 0; i < max_i && scm->pc != UNUSED_ADDR; ++i, ++scm)
    {
      if (scm->pc == pc)
	return scm->sc;
    }
  return 0;
}

/* Look up PC and if not found create an entry for it.
   If found the result is a pointer to the SCACHE entry.
   If not found the result is NULL, and the address of a buffer of at least
   N entries is stored in BUFP.
   It's done this way so the caller can still distinguish found/not-found.
   If the table is full, it is emptied to make room.
   If the maximum length of a hash list is reached a random entry is thrown out
   to make room.
   ??? One might want to try to make this smarter, but let's see some
   measurable benefit first.  */

SCACHE *
scache_lookup_or_alloc (SIM_CPU *cpu, IADDR pc, int n, SCACHE **bufp)
{
  /* FIXME: hash computation is wrong, doesn't take into account
     NUM_HASH_CHAIN_ENTRIES.  A lot of the hash table will be unused!  */
  unsigned int slot = HASH_PC (pc) & (CPU_SCACHE_NUM_HASH_CHAINS (cpu) - 1);
  int i, max_i = CPU_SCACHE_NUM_HASH_CHAIN_ENTRIES (cpu);
  SCACHE_MAP *scm;
  SCACHE *sc;

  scm = & CPU_SCACHE_HASH_TABLE (cpu) [slot];
  for (i = 0; i < max_i && scm->pc != UNUSED_ADDR; ++i, ++scm)
    {
      if (scm->pc == pc)
	{
	  PROFILE_COUNT_SCACHE_HIT (cpu);
	  return scm->sc;
	}
    }
  PROFILE_COUNT_SCACHE_MISS (cpu);

  /* The address we want isn't cached.  Bummer.
     If the hash chain we have for this address is full, throw out an entry
     to make room.  */

  if (i == max_i)
    {
      /* Rather than do something sophisticated like LRU, we just throw out
	 a semi-random entry.  Let someone else have the joy of saying how
	 wrong this is.  NEXT_FREE is the entry to throw out and cycles
	 through all possibilities.  */
      static int next_free = 0;

      scm = & CPU_SCACHE_HASH_TABLE (cpu) [slot];
      /* FIXME: This seems rather clumsy.  */
      for (i = 0; i < next_free; ++i, ++scm)
	continue;
      ++next_free;
      if (next_free == CPU_SCACHE_NUM_HASH_CHAIN_ENTRIES (cpu))
	next_free = 0;
    }

  /* At this point SCM points to the hash table entry to use.
     Now make sure there's room in the cache.  */
  /* FIXME: Kinda weird to use a next_free adjusted scm when cache is
     flushed.  */

  {
    int elm_size = IMP_PROPS_SCACHE_ELM_SIZE (MACH_IMP_PROPS (CPU_MACH (cpu)));
    int elms_used = (((char *) CPU_SCACHE_NEXT_FREE (cpu)
		      - (char *) CPU_SCACHE_CACHE (cpu))
		     / elm_size);
    int elms_left = CPU_SCACHE_SIZE (cpu) - elms_used;

    if (elms_left < n)
      {
	PROFILE_COUNT_SCACHE_FULL_FLUSH (cpu);
	scache_flush_cpu (cpu);
      }
  }

  sc = CPU_SCACHE_NEXT_FREE (cpu);
  scm->pc = pc;
  scm->sc = sc;

  *bufp = sc;
  return NULL;
}

#endif /* WITH_SCACHE_PBB */

/* Print cache access statics for CPU.  */

void
scache_print_profile (SIM_CPU *cpu, int verbose)
{
  SIM_DESC sd = CPU_STATE (cpu);
  unsigned long hits = CPU_SCACHE_HITS (cpu);
  unsigned long misses = CPU_SCACHE_MISSES (cpu);
  char buf[20];
  unsigned long max_val;
  unsigned long *lengths;
  int i;

  if (CPU_SCACHE_SIZE (cpu) == 0)
    return;

  sim_io_printf (sd, "Simulator Cache Statistics\n\n");

  /* One could use PROFILE_LABEL_WIDTH here.  I chose not to.  */
  sim_io_printf (sd, "  Cache size: %s\n",
		 sim_add_commas (buf, sizeof (buf), CPU_SCACHE_SIZE (cpu)));
  sim_io_printf (sd, "  Hits:       %s\n",
		 sim_add_commas (buf, sizeof (buf), hits));
  sim_io_printf (sd, "  Misses:     %s\n",
		 sim_add_commas (buf, sizeof (buf), misses));
  if (hits + misses != 0)
    sim_io_printf (sd, "  Hit rate:   %.2f%%\n",
		   ((double) hits / ((double) hits + (double) misses)) * 100);

#if WITH_SCACHE_PBB
  sim_io_printf (sd, "\n");
  sim_io_printf (sd, "  Hash table size:       %s\n",
		 sim_add_commas (buf, sizeof (buf), CPU_SCACHE_NUM_HASH_CHAINS (cpu)));
  sim_io_printf (sd, "  Max hash list length:  %s\n",
		 sim_add_commas (buf, sizeof (buf), CPU_SCACHE_NUM_HASH_CHAIN_ENTRIES (cpu)));
  sim_io_printf (sd, "  Max insn chain length: %s\n",
		 sim_add_commas (buf, sizeof (buf), CPU_SCACHE_MAX_CHAIN_LENGTH (cpu)));
  sim_io_printf (sd, "  Cache full flushes:    %s\n",
		 sim_add_commas (buf, sizeof (buf), CPU_SCACHE_FULL_FLUSHES (cpu)));
  sim_io_printf (sd, "\n");

  if (verbose)
    {
      sim_io_printf (sd, "  Insn chain lengths:\n\n");
      max_val = 0;
      lengths = CPU_SCACHE_CHAIN_LENGTHS (cpu);
      for (i = 1; i < CPU_SCACHE_MAX_CHAIN_LENGTH (cpu); ++i)
	if (lengths[i] > max_val)
	  max_val = lengths[i];
      for (i = 1; i < CPU_SCACHE_MAX_CHAIN_LENGTH (cpu); ++i)
	{
	  sim_io_printf (sd, "  %2d: %*s: ",
			 i,
			 max_val < 10000 ? 5 : 10,
			 sim_add_commas (buf, sizeof (buf), lengths[i]));
	  sim_profile_print_bar (sd, PROFILE_HISTOGRAM_WIDTH,
				 lengths[i], max_val);
	  sim_io_printf (sd, "\n");
	}
      sim_io_printf (sd, "\n");
    }
#endif /* WITH_SCACHE_PBB */
}
