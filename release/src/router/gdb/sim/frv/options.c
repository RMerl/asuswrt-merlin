/* FRV simulator memory option handling.
   Copyright (C) 1999, 2000, 2007 Free Software Foundation, Inc.
   Contributed by Red Hat.

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

#define WANT_CPU frvbf
#define WANT_CPU_FRVBF

#include "sim-main.h"
#include "sim-assert.h"
#include "sim-options.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

/* FRV specific command line options. */

enum {
  OPTION_FRV_DATA_CACHE = OPTION_START,
  OPTION_FRV_INSN_CACHE,
  OPTION_FRV_PROFILE_CACHE,
  OPTION_FRV_PROFILE_PARALLEL,
  OPTION_FRV_TIMER,
  OPTION_FRV_MEMORY_LATENCY
};

static DECLARE_OPTION_HANDLER (frv_option_handler);

const OPTION frv_options[] =
{
  { {"profile", optional_argument, NULL, 'p'},
      'p', "on|off", "Perform profiling",
      frv_option_handler },
  { {"data-cache", optional_argument, NULL, OPTION_FRV_DATA_CACHE },
      '\0', "WAYS[,SETS[,LINESIZE]]", "Enable data cache",
      frv_option_handler },
  { {"insn-cache", optional_argument, NULL, OPTION_FRV_INSN_CACHE },
      '\0', "WAYS[,SETS[,LINESIZE]]", "Enable instruction cache",
      frv_option_handler },
  { {"profile-cache", optional_argument, NULL, OPTION_FRV_PROFILE_CACHE },
      '\0', "on|off", "Profile caches",
      frv_option_handler },
  { {"profile-parallel", optional_argument, NULL, OPTION_FRV_PROFILE_PARALLEL },
      '\0', "on|off", "Profile parallelism",
      frv_option_handler },
  { {"timer", required_argument, NULL, OPTION_FRV_TIMER },
      '\0', "CYCLES,INTERRUPT", "Set Interrupt Timer",
      frv_option_handler },
  { {"memory-latency", required_argument, NULL, OPTION_FRV_MEMORY_LATENCY },
      '\0', "CYCLES", "Set Latency of memory",
      frv_option_handler },
  { {NULL, no_argument, NULL, 0}, '\0', NULL, NULL, NULL }
};

static char *
parse_size (char *chp, address_word *nr_bytes)
{
  /* <nr_bytes> */
  *nr_bytes = strtoul (chp, &chp, 0);
  return chp;
}

static address_word
check_pow2 (address_word value, char *argname, char *optname, SIM_DESC sd)
{
  if ((value & (value - 1)) != 0)
    {
      sim_io_eprintf (sd, "%s argument to %s must be a power of 2\n",
		      argname, optname);
      return 0; /* will enable default value.  */
    }

  return value;
}

static void
parse_cache_option (SIM_DESC sd, char *arg, char *cache_name, int is_data_cache)
{
  int i;
  address_word ways = 0, sets = 0, linesize = 0;
  if (arg != NULL)
    {
      char *chp = arg;
      /* parse the arguments */
      chp = parse_size (chp, &ways);
      ways = check_pow2 (ways, "WAYS", cache_name, sd);
      if (*chp == ',')
	{
	  chp = parse_size (chp + 1, &sets);
	  sets = check_pow2 (sets, "SETS", cache_name, sd);
	  if (*chp == ',')
	    {
	      chp = parse_size (chp + 1, &linesize);
	      linesize = check_pow2 (linesize, "LINESIZE", cache_name, sd);
	    }
	}
    }
  for (i = 0; i < MAX_NR_PROCESSORS; ++i)
    {
      SIM_CPU *current_cpu = STATE_CPU (sd, i);
      FRV_CACHE *cache = is_data_cache ? CPU_DATA_CACHE (current_cpu)
	                               : CPU_INSN_CACHE (current_cpu);
      cache->ways = ways;
      cache->sets = sets;
      cache->line_size = linesize;
      frv_cache_init (current_cpu, cache);
    }
}

static SIM_RC
frv_option_handler (SIM_DESC sd, sim_cpu *current_cpu, int opt,
		    char *arg, int is_command)
{
  switch (opt)
    {
    case 'p' :
      if (! WITH_PROFILE)
	sim_io_eprintf (sd, "Profiling not compiled in, `-p' ignored\n");
      else
	{
	  unsigned mask = PROFILE_USEFUL_MASK;
	  if (WITH_PROFILE_CACHE_P)
	    mask |= (1 << PROFILE_CACHE_IDX);
	  if (WITH_PROFILE_PARALLEL_P)
	    mask |= (1 << PROFILE_PARALLEL_IDX);
	  return set_profile_option_mask (sd, "profile", mask, arg);
	}
      break;

    case OPTION_FRV_DATA_CACHE:
      parse_cache_option (sd, arg, "data_cache", 1/*is_data_cache*/);
      return SIM_RC_OK;

    case OPTION_FRV_INSN_CACHE:
      parse_cache_option (sd, arg, "insn_cache", 0/*is_data_cache*/);
      return SIM_RC_OK;

    case OPTION_FRV_PROFILE_CACHE:
      if (WITH_PROFILE_CACHE_P)
	return sim_profile_set_option (sd, "-cache", PROFILE_CACHE_IDX, arg);
      else
	sim_io_eprintf (sd, "Cache profiling not compiled in, `--profile-cache' ignored\n");
      break;

    case OPTION_FRV_PROFILE_PARALLEL:
      if (WITH_PROFILE_PARALLEL_P)
	{
	  unsigned mask
	    = (1 << PROFILE_MODEL_IDX) | (1 << PROFILE_PARALLEL_IDX);
	  return set_profile_option_mask (sd, "-parallel", mask, arg);
	}
      else
	sim_io_eprintf (sd, "Parallel profiling not compiled in, `--profile-parallel' ignored\n");
      break;

    case OPTION_FRV_TIMER:
      {
	char *chp = arg;
	address_word cycles, interrupt;
	chp = parse_size (chp, &cycles);
	if (chp == arg)
	  {
	    sim_io_eprintf (sd, "Cycle count required for --timer\n");
	    return SIM_RC_FAIL;
	  }
	if (*chp != ',')
	  {
	    sim_io_eprintf (sd, "Interrupt number required for --timer\n");
	    return SIM_RC_FAIL;
	  }
	chp = parse_size (chp + 1, &interrupt);
	if (interrupt < 1 || interrupt > 15)
	  {
	    sim_io_eprintf (sd, "Interrupt number for --timer must be greater than 0 and less that 16\n");
	    return SIM_RC_FAIL;
	  }
	frv_interrupt_state.timer.enabled = 1;
	frv_interrupt_state.timer.value = cycles;
	frv_interrupt_state.timer.current = 0;
	frv_interrupt_state.timer.interrupt =
	  FRV_INTERRUPT_LEVEL_1 + interrupt - 1;
      }
      return SIM_RC_OK;

    case OPTION_FRV_MEMORY_LATENCY:
      {
	int i;
	char *chp = arg;
	address_word cycles;
	chp = parse_size (chp, &cycles);
	if (chp == arg)
	  {
	    sim_io_eprintf (sd, "Cycle count required for --memory-latency\n");
	    return SIM_RC_FAIL;
	  }
	for (i = 0; i < MAX_NR_PROCESSORS; ++i)
	  {
	    SIM_CPU *current_cpu = STATE_CPU (sd, i);
	    FRV_CACHE *insn_cache = CPU_INSN_CACHE (current_cpu);
	    FRV_CACHE *data_cache = CPU_DATA_CACHE (current_cpu);
	    insn_cache->memory_latency = cycles;
	    data_cache->memory_latency = cycles;
	  }
      }
      return SIM_RC_OK;

    default:
      sim_io_eprintf (sd, "Unknown FRV option %d\n", opt);
      return SIM_RC_FAIL;

    }

  return SIM_RC_FAIL;
}
