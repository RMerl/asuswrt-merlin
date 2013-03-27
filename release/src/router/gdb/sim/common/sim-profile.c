/* Default profiling support.
   Copyright (C) 1996, 1997, 1998, 2000, 2001, 2007
   Free Software Foundation, Inc.
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

#include "sim-main.h"
#include "sim-io.h"
#include "sim-options.h"
#include "sim-assert.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif
#include <ctype.h>

#define COMMAS(n) sim_add_commas (comma_buf, sizeof (comma_buf), (n))

static MODULE_INIT_FN profile_init;
static MODULE_UNINSTALL_FN profile_uninstall;

static DECLARE_OPTION_HANDLER (profile_option_handler);

enum {
  OPTION_PROFILE_INSN = OPTION_START,
  OPTION_PROFILE_MEMORY,
  OPTION_PROFILE_MODEL,
  OPTION_PROFILE_FILE,
  OPTION_PROFILE_CORE,
  OPTION_PROFILE_CPU_FREQUENCY,
  OPTION_PROFILE_PC,
  OPTION_PROFILE_PC_RANGE,
  OPTION_PROFILE_PC_GRANULARITY,
  OPTION_PROFILE_RANGE,
  OPTION_PROFILE_FUNCTION
};

static const OPTION profile_options[] = {
  { {"profile", optional_argument, NULL, 'p'},
      'p', "on|off", "Perform profiling",
      profile_option_handler },
  { {"profile-insn", optional_argument, NULL, OPTION_PROFILE_INSN},
      '\0', "on|off", "Perform instruction profiling",
      profile_option_handler },
  { {"profile-memory", optional_argument, NULL, OPTION_PROFILE_MEMORY},
      '\0', "on|off", "Perform memory profiling",
      profile_option_handler },
  { {"profile-core", optional_argument, NULL, OPTION_PROFILE_CORE},
      '\0', "on|off", "Perform CORE profiling",
      profile_option_handler },
  { {"profile-model", optional_argument, NULL, OPTION_PROFILE_MODEL},
      '\0', "on|off", "Perform model profiling",
      profile_option_handler },
  { {"profile-cpu-frequency", required_argument, NULL,
     OPTION_PROFILE_CPU_FREQUENCY},
      '\0', "CPU FREQUENCY", "Specify the speed of the simulated cpu clock",
      profile_option_handler },

  { {"profile-file", required_argument, NULL, OPTION_PROFILE_FILE},
      '\0', "FILE NAME", "Specify profile output file",
      profile_option_handler },

  { {"profile-pc", optional_argument, NULL, OPTION_PROFILE_PC},
      '\0', "on|off", "Perform PC profiling",
      profile_option_handler },
  { {"profile-pc-frequency", required_argument, NULL, 'F'},
      'F', "PC PROFILE FREQUENCY", "Specified PC profiling frequency",
      profile_option_handler },
  { {"profile-pc-size", required_argument, NULL, 'S'},
      'S', "PC PROFILE SIZE", "Specify PC profiling size",
      profile_option_handler },
  { {"profile-pc-granularity", required_argument, NULL, OPTION_PROFILE_PC_GRANULARITY},
      '\0', "PC PROFILE GRANULARITY", "Specify PC profiling sample coverage",
      profile_option_handler },
  { {"profile-pc-range", required_argument, NULL, OPTION_PROFILE_PC_RANGE},
      '\0', "BASE,BOUND", "Specify PC profiling address range",
      profile_option_handler },

#ifdef SIM_HAVE_ADDR_RANGE
  { {"profile-range", required_argument, NULL, OPTION_PROFILE_RANGE},
      '\0', "START,END", "Specify range of addresses for instruction and model profiling",
      profile_option_handler },
#if 0 /*wip*/
  { {"profile-function", required_argument, NULL, OPTION_PROFILE_FUNCTION},
      '\0', "FUNCTION", "Specify function to profile",
      profile_option_handler },
#endif
#endif

  { {NULL, no_argument, NULL, 0}, '\0', NULL, NULL, NULL }
};

/* Set/reset the profile options indicated in MASK.  */

SIM_RC
set_profile_option_mask (SIM_DESC sd, const char *name, int mask, const char *arg)
{
  int profile_nr;
  int cpu_nr;
  int profile_val = 1;

  if (arg != NULL)
    {
      if (strcmp (arg, "yes") == 0
	  || strcmp (arg, "on") == 0
	  || strcmp (arg, "1") == 0)
	profile_val = 1;
      else if (strcmp (arg, "no") == 0
	       || strcmp (arg, "off") == 0
	       || strcmp (arg, "0") == 0)
	profile_val = 0;
      else
	{
	  sim_io_eprintf (sd, "Argument `%s' for `--profile%s' invalid, one of `on', `off', `yes', `no' expected\n", arg, name);
	  return SIM_RC_FAIL;
	}
    }

  /* update applicable profile bits */
  for (profile_nr = 0; profile_nr < MAX_PROFILE_VALUES; ++profile_nr)
    {
      if ((mask & (1 << profile_nr)) == 0)
	continue;

#if 0 /* see sim-trace.c, set flags in STATE here if/when there are any */
      /* Set non-cpu specific values.  */
      switch (profile_nr)
	{
	case ??? :
	  break;
	}
#endif

      /* Set cpu values.  */
      for (cpu_nr = 0; cpu_nr < MAX_NR_PROCESSORS; cpu_nr++)
	{
	  CPU_PROFILE_FLAGS (STATE_CPU (sd, cpu_nr))[profile_nr] = profile_val;
	}
    }

  /* Re-compute the cpu profile summary.  */
  if (profile_val)
    {
      for (cpu_nr = 0; cpu_nr < MAX_NR_PROCESSORS; cpu_nr++)
	CPU_PROFILE_DATA (STATE_CPU (sd, cpu_nr))->profile_any_p = 1;
    }
  else
    {
      for (cpu_nr = 0; cpu_nr < MAX_NR_PROCESSORS; cpu_nr++)
	{
	  CPU_PROFILE_DATA (STATE_CPU (sd, cpu_nr))->profile_any_p = 0;
	  for (profile_nr = 0; profile_nr < MAX_PROFILE_VALUES; ++profile_nr)
	    {
	      if (CPU_PROFILE_FLAGS (STATE_CPU (sd, cpu_nr))[profile_nr])
		{
		  CPU_PROFILE_DATA (STATE_CPU (sd, cpu_nr))->profile_any_p = 1;
		  break;
		}
	    }
	}
    }  

  return SIM_RC_OK;
}

/* Set one profile option based on its IDX value.
   Not static as cgen-scache.c uses it.  */

SIM_RC
sim_profile_set_option (SIM_DESC sd, const char *name, int idx, const char *arg)
{
  return set_profile_option_mask (sd, name, 1 << idx, arg);
}

static SIM_RC
parse_frequency (SIM_DESC sd, const char *arg, unsigned long *freq)
{
  const char *ch;
  /* First, parse a decimal number.  */
  *freq = 0;
  ch = arg;
  if (isdigit (*arg))
    {
      for (/**/; *ch != '\0'; ++ch)
	{
	  if (! isdigit (*ch))
	    break;
	  *freq = *freq * 10 + (*ch - '0');
	}

      /* Accept KHz, MHz or Hz as a suffix.  */
      if (tolower (*ch) == 'm')
	{
	  *freq *= 1000000;
	  ++ch;
	}
      else if (tolower (*ch) == 'k')
	{
	  *freq *= 1000;
	  ++ch;
	}

      if (tolower (*ch) == 'h')
	{
	  ++ch;
	  if (tolower (*ch) == 'z')
	    ++ch;
	}
    }

  if (*ch != '\0')
    {
      sim_io_eprintf (sd, "Invalid argument for --profile-cpu-frequency: %s\n",
		      arg);
      *freq = 0;
      return SIM_RC_FAIL;
    }

  return SIM_RC_OK;
}

static SIM_RC
profile_option_handler (SIM_DESC sd,
			sim_cpu *cpu,
			int opt,
			char *arg,
			int is_command)
{
  int cpu_nr;

  /* FIXME: Need to handle `cpu' arg.  */

  switch (opt)
    {
    case 'p' :
      if (! WITH_PROFILE)
	sim_io_eprintf (sd, "Profiling not compiled in, `-p' ignored\n");
      else
	return set_profile_option_mask (sd, "profile", PROFILE_USEFUL_MASK,
					arg);
      break;

    case OPTION_PROFILE_INSN :
      if (WITH_PROFILE_INSN_P)
	return sim_profile_set_option (sd, "-insn", PROFILE_INSN_IDX, arg);
      else
	sim_io_eprintf (sd, "Instruction profiling not compiled in, `--profile-insn' ignored\n");
      break;

    case OPTION_PROFILE_MEMORY :
      if (WITH_PROFILE_MEMORY_P)
	return sim_profile_set_option (sd, "-memory", PROFILE_MEMORY_IDX, arg);
      else
	sim_io_eprintf (sd, "Memory profiling not compiled in, `--profile-memory' ignored\n");
      break;

    case OPTION_PROFILE_CORE :
      if (WITH_PROFILE_CORE_P)
	return sim_profile_set_option (sd, "-core", PROFILE_CORE_IDX, arg);
      else
	sim_io_eprintf (sd, "CORE profiling not compiled in, `--profile-core' ignored\n");
      break;

    case OPTION_PROFILE_MODEL :
      if (WITH_PROFILE_MODEL_P)
	return sim_profile_set_option (sd, "-model", PROFILE_MODEL_IDX, arg);
      else
	sim_io_eprintf (sd, "Model profiling not compiled in, `--profile-model' ignored\n");
      break;

    case OPTION_PROFILE_CPU_FREQUENCY :
      {
	unsigned long val;
	SIM_RC rc = parse_frequency (sd, arg, &val);
	if (rc == SIM_RC_OK)
	  {
	    for (cpu_nr = 0; cpu_nr < MAX_NR_PROCESSORS; ++cpu_nr)
	      PROFILE_CPU_FREQ (CPU_PROFILE_DATA (STATE_CPU (sd,cpu_nr))) = val;
	  }
	return rc;
      }

    case OPTION_PROFILE_FILE :
      /* FIXME: Might want this to apply to pc profiling only,
	 or have two profile file options.  */
      if (! WITH_PROFILE)
	sim_io_eprintf (sd, "Profiling not compiled in, `--profile-file' ignored\n");
      else
	{
	  FILE *f = fopen (arg, "w");

	  if (f == NULL)
	    {
	      sim_io_eprintf (sd, "Unable to open profile output file `%s'\n", arg);
	      return SIM_RC_FAIL;
	    }
	  for (cpu_nr = 0; cpu_nr < MAX_NR_PROCESSORS; ++cpu_nr)
	    PROFILE_FILE (CPU_PROFILE_DATA (STATE_CPU (sd, cpu_nr))) = f;
	}
      break;

    case OPTION_PROFILE_PC:
      if (WITH_PROFILE_PC_P)
	return sim_profile_set_option (sd, "-pc", PROFILE_PC_IDX, arg);
      else
	sim_io_eprintf (sd, "PC profiling not compiled in, `--profile-pc' ignored\n");
      break;

    case 'F' :
      if (WITH_PROFILE_PC_P)
	{
	  /* FIXME: Validate arg.  */
	  int val = atoi (arg);
	  for (cpu_nr = 0; cpu_nr < MAX_NR_PROCESSORS; ++cpu_nr)
	    PROFILE_PC_FREQ (CPU_PROFILE_DATA (STATE_CPU (sd, cpu_nr))) = val;
	  for (cpu_nr = 0; cpu_nr < MAX_NR_PROCESSORS; ++cpu_nr)
	    CPU_PROFILE_FLAGS (STATE_CPU (sd, cpu_nr))[PROFILE_PC_IDX] = 1;
	}
      else
	sim_io_eprintf (sd, "PC profiling not compiled in, `--profile-pc-frequency' ignored\n");
      break;

    case 'S' :
      if (WITH_PROFILE_PC_P)
	{
	  /* FIXME: Validate arg.  */
	  int val = atoi (arg);
	  for (cpu_nr = 0; cpu_nr < MAX_NR_PROCESSORS; ++cpu_nr)
	    PROFILE_PC_NR_BUCKETS (CPU_PROFILE_DATA (STATE_CPU (sd, cpu_nr))) = val;
	  for (cpu_nr = 0; cpu_nr < MAX_NR_PROCESSORS; ++cpu_nr)
	    CPU_PROFILE_FLAGS (STATE_CPU (sd, cpu_nr))[PROFILE_PC_IDX] = 1;
	}
      else
	sim_io_eprintf (sd, "PC profiling not compiled in, `--profile-pc-size' ignored\n");
      break;

    case OPTION_PROFILE_PC_GRANULARITY:
      if (WITH_PROFILE_PC_P)
	{
	  int shift;
	  int val = atoi (arg);
	  /* check that the granularity is a power of two */
	  shift = 0;
	  while (val > (1 << shift))
	    {
	      shift += 1;
	    }
	  if (val != (1 << shift))
	    {
	      sim_io_eprintf (sd, "PC profiling granularity not a power of two\n");
	      return SIM_RC_FAIL;
	    }
	  if (shift == 0)
	    {
	      sim_io_eprintf (sd, "PC profiling granularity too small");
	      return SIM_RC_FAIL;
	    }
	  for (cpu_nr = 0; cpu_nr < MAX_NR_PROCESSORS; ++cpu_nr)
	    PROFILE_PC_SHIFT (CPU_PROFILE_DATA (STATE_CPU (sd, cpu_nr))) = shift;
	  for (cpu_nr = 0; cpu_nr < MAX_NR_PROCESSORS; ++cpu_nr)
	    CPU_PROFILE_FLAGS (STATE_CPU (sd, cpu_nr))[PROFILE_PC_IDX] = 1;
	}
      else
	sim_io_eprintf (sd, "PC profiling not compiled in, `--profile-pc-granularity' ignored\n");
      break;

    case OPTION_PROFILE_PC_RANGE:
      if (WITH_PROFILE_PC_P)
	{
	  /* FIXME: Validate args */
	  char *chp = arg;
	  unsigned long base;
	  unsigned long bound;
	  base = strtoul (chp, &chp, 0);
	  if (*chp != ',')
	    {
	      sim_io_eprintf (sd, "--profile-pc-range missing BOUND argument\n");
	      return SIM_RC_FAIL;
	    }
	  bound = strtoul (chp + 1, NULL, 0);
	  for (cpu_nr = 0; cpu_nr < MAX_NR_PROCESSORS; ++cpu_nr)
	    {
	      PROFILE_PC_START (CPU_PROFILE_DATA (STATE_CPU (sd, cpu_nr))) = base;
	      PROFILE_PC_END (CPU_PROFILE_DATA (STATE_CPU (sd, cpu_nr))) = bound;
	    }	      
	  for (cpu_nr = 0; cpu_nr < MAX_NR_PROCESSORS; ++cpu_nr)
	    CPU_PROFILE_FLAGS (STATE_CPU (sd, cpu_nr))[PROFILE_PC_IDX] = 1;
	}
      else
	sim_io_eprintf (sd, "PC profiling not compiled in, `--profile-pc-range' ignored\n");
      break;

#ifdef SIM_HAVE_ADDR_RANGE
    case OPTION_PROFILE_RANGE :
      if (WITH_PROFILE)
	{
	  char *chp = arg;
	  unsigned long start,end;
	  start = strtoul (chp, &chp, 0);
	  if (*chp != ',')
	    {
	      sim_io_eprintf (sd, "--profile-range missing END argument\n");
	      return SIM_RC_FAIL;
	    }
	  end = strtoul (chp + 1, NULL, 0);
	  /* FIXME: Argument validation.  */
	  if (cpu != NULL)
	    sim_addr_range_add (PROFILE_RANGE (CPU_PROFILE_DATA (cpu)),
				start, end);
	  else
	    for (cpu_nr = 0; cpu_nr < MAX_NR_PROCESSORS; ++cpu_nr)
	      sim_addr_range_add (PROFILE_RANGE (CPU_PROFILE_DATA (STATE_CPU (sd, cpu_nr))),
				  start, end);
	}
      else
	sim_io_eprintf (sd, "Profiling not compiled in, `--profile-range' ignored\n");
      break;

    case OPTION_PROFILE_FUNCTION :
      if (WITH_PROFILE)
	{
	  /*wip: need to compute function range given name*/
	}
      else
	sim_io_eprintf (sd, "Profiling not compiled in, `--profile-function' ignored\n");
      break;
#endif /* SIM_HAVE_ADDR_RANGE */
    }

  return SIM_RC_OK;
}

/* PC profiling support */

#if WITH_PROFILE_PC_P

static void
profile_pc_cleanup (SIM_DESC sd)
{
  int n;
  for (n = 0; n < MAX_NR_PROCESSORS; n++)
    {
      sim_cpu *cpu = STATE_CPU (sd, n);
      PROFILE_DATA *data = CPU_PROFILE_DATA (cpu);
      if (PROFILE_PC_COUNT (data) != NULL)
	zfree (PROFILE_PC_COUNT (data));
      PROFILE_PC_COUNT (data) = NULL;
      if (PROFILE_PC_EVENT (data) != NULL)
	sim_events_deschedule (sd, PROFILE_PC_EVENT (data));
      PROFILE_PC_EVENT (data) = NULL;
    }
}


static void
profile_pc_uninstall (SIM_DESC sd)
{
  profile_pc_cleanup (sd);
}

static void
profile_pc_event (SIM_DESC sd,
		  void *data)
{
  sim_cpu *cpu = (sim_cpu*) data;
  PROFILE_DATA *profile = CPU_PROFILE_DATA (cpu);
  address_word pc;
  unsigned i;
  switch (STATE_WATCHPOINTS (sd)->sizeof_pc)
    {
    case 2: pc = *(unsigned_2*)(STATE_WATCHPOINTS (sd)->pc) ; break;
    case 4: pc = *(unsigned_4*)(STATE_WATCHPOINTS (sd)->pc) ; break;
    case 8: pc = *(unsigned_8*)(STATE_WATCHPOINTS (sd)->pc) ; break;
    default: pc = 0;
    }
  i = (pc - PROFILE_PC_START (profile)) >> PROFILE_PC_SHIFT (profile);
  if (i < PROFILE_PC_NR_BUCKETS (profile))
    PROFILE_PC_COUNT (profile) [i] += 1; /* Overflow? */
  else
    PROFILE_PC_COUNT (profile) [PROFILE_PC_NR_BUCKETS (profile)] += 1;
  PROFILE_PC_EVENT (profile) = 
    sim_events_schedule (sd, PROFILE_PC_FREQ (profile), profile_pc_event, cpu);
}

static SIM_RC
profile_pc_init (SIM_DESC sd)
{
  int n;
  profile_pc_cleanup (sd);
  for (n = 0; n < MAX_NR_PROCESSORS; n++)
    {
      sim_cpu *cpu = STATE_CPU (sd, n);
      PROFILE_DATA *data = CPU_PROFILE_DATA (cpu);
      if (CPU_PROFILE_FLAGS (STATE_CPU (sd, n))[PROFILE_PC_IDX]
	  && STATE_WATCHPOINTS (sd)->pc != NULL)
	{
	  int bucket_size;
	  /* fill in the frequency if not specified */
	  if (PROFILE_PC_FREQ (data) == 0)
	    PROFILE_PC_FREQ (data) = 257;
	  /* fill in the start/end if not specified */
	  if (PROFILE_PC_END (data) == 0)
	    {
	      PROFILE_PC_START (data) = STATE_TEXT_START (sd);
	      PROFILE_PC_END (data) = STATE_TEXT_END (sd);
	    }
	  /* Compute the number of buckets if not specified. */
	  if (PROFILE_PC_NR_BUCKETS (data) == 0)
	    {
	      if (PROFILE_PC_BUCKET_SIZE (data) == 0)
		PROFILE_PC_NR_BUCKETS (data) = 16;
	      else
		{
		  if (PROFILE_PC_END (data) == 0)
		    {
		      /* nr_buckets = (full-address-range / 2) / (bucket_size / 2) */
		      PROFILE_PC_NR_BUCKETS (data) =
			((1 << (STATE_WATCHPOINTS (sd)->sizeof_pc) * (8 - 1))
			 / (PROFILE_PC_BUCKET_SIZE (data) / 2));
		    }
		  else
		    {
		      PROFILE_PC_NR_BUCKETS (data) =
			((PROFILE_PC_END (data)
			  - PROFILE_PC_START (data)
			  + PROFILE_PC_BUCKET_SIZE (data) - 1)
			 / PROFILE_PC_BUCKET_SIZE (data));
		    }
		}
	    }
	  /* Compute the bucket size if not specified.  Ensure that it
             is rounded up to the next power of two */
	  if (PROFILE_PC_BUCKET_SIZE (data) == 0)
	    {
	      if (PROFILE_PC_END (data) == 0)
		/* bucket_size = (full-address-range / 2) / (nr_buckets / 2) */
		bucket_size = ((1 << ((STATE_WATCHPOINTS (sd)->sizeof_pc * 8) - 1))
			       / (PROFILE_PC_NR_BUCKETS (data) / 2));
	      else
		bucket_size = ((PROFILE_PC_END (data)
				- PROFILE_PC_START (data)
				+ PROFILE_PC_NR_BUCKETS (data) - 1)
			       / PROFILE_PC_NR_BUCKETS (data));
	      PROFILE_PC_SHIFT (data) = 0;
	      while (bucket_size > PROFILE_PC_BUCKET_SIZE (data))
		{
		  PROFILE_PC_SHIFT (data) += 1;
		}
	    }
	  /* Align the end address with bucket size */
	  if (PROFILE_PC_END (data) != 0)
	    PROFILE_PC_END (data) = (PROFILE_PC_START (data)
				     + (PROFILE_PC_BUCKET_SIZE (data)
					* PROFILE_PC_NR_BUCKETS (data)));
	  /* create the relevant buffers */
	  PROFILE_PC_COUNT (data) =
	    NZALLOC (unsigned, PROFILE_PC_NR_BUCKETS (data) + 1);
	  PROFILE_PC_EVENT (data) =
	    sim_events_schedule (sd,
				 PROFILE_PC_FREQ (data),
				 profile_pc_event,
				 cpu);
	}
    }
  return SIM_RC_OK;
}

static void
profile_print_pc (sim_cpu *cpu, int verbose)
{
  SIM_DESC sd = CPU_STATE (cpu);
  PROFILE_DATA *profile = CPU_PROFILE_DATA (cpu);
  char comma_buf[20];
  unsigned max_val;
  unsigned total;
  unsigned i;

  if (PROFILE_PC_COUNT (profile) == 0)
    return;

  sim_io_printf (sd, "Program Counter Statistics:\n\n");

  /* First pass over data computes various things.  */
  max_val = 0;
  total = 0;
  for (i = 0; i <= PROFILE_PC_NR_BUCKETS (profile); ++i)
    {
      total += PROFILE_PC_COUNT (profile) [i];
      if (PROFILE_PC_COUNT (profile) [i] > max_val)
	max_val = PROFILE_PC_COUNT (profile) [i];
    }

  sim_io_printf (sd, "  Total samples: %s\n",
		 COMMAS (total));
  sim_io_printf (sd, "  Granularity: %s bytes per bucket\n",
		 COMMAS (PROFILE_PC_BUCKET_SIZE (profile)));
  sim_io_printf (sd, "  Size: %s buckets\n",
		 COMMAS (PROFILE_PC_NR_BUCKETS (profile)));
  sim_io_printf (sd, "  Frequency: %s cycles per sample\n",
		 COMMAS (PROFILE_PC_FREQ (profile)));

  if (PROFILE_PC_END (profile) != 0)
    sim_io_printf (sd, "  Range: 0x%lx 0x%lx\n",
		   (long) PROFILE_PC_START (profile),
		   (long) PROFILE_PC_END (profile));

  if (verbose && max_val != 0)
    {
      /* Now we can print the histogram.  */
      sim_io_printf (sd, "\n");
      for (i = 0; i <= PROFILE_PC_NR_BUCKETS (profile); ++i)
	{
	  if (PROFILE_PC_COUNT (profile) [i] != 0)
	    {
	      sim_io_printf (sd, "  ");
	      if (i == PROFILE_PC_NR_BUCKETS (profile))
		sim_io_printf (sd, "%10s:", "overflow");
	      else
		sim_io_printf (sd, "0x%08lx:",
			       (long) (PROFILE_PC_START (profile)
				       + (i * PROFILE_PC_BUCKET_SIZE (profile))));
	      sim_io_printf (sd, " %*s",
			     max_val < 10000 ? 5 : 10,
			     COMMAS (PROFILE_PC_COUNT (profile) [i]));
	      sim_io_printf (sd, " %4.1f",
			     (PROFILE_PC_COUNT (profile) [i] * 100.0) / total);
	      sim_io_printf (sd, ": ");
	      sim_profile_print_bar (sd, PROFILE_HISTOGRAM_WIDTH,
				     PROFILE_PC_COUNT (profile) [i],
				     max_val);
	      sim_io_printf (sd, "\n");
	    }
	}
    }

  /* dump the histogram to the file "gmon.out" using BSD's gprof file
     format */
  /* Since a profile data file is in the native format of the host on
     which the profile is being, endian issues are not considered in
     the code below. */
  /* FIXME: Is this the best place for this code? */
  {
    FILE *pf = fopen ("gmon.out", "wb");
    
    if (pf == NULL)
      sim_io_eprintf (sd, "Failed to open \"gmon.out\" profile file\n");
    else
      {
	int ok;
	/* FIXME: what if the target has a 64 bit PC? */
	unsigned32 header[3];
	unsigned loop;
	if (PROFILE_PC_END (profile) != 0)
	  {
	    header[0] = PROFILE_PC_START (profile);
	    header[1] = PROFILE_PC_END (profile);
	  }
	else
	  {
	    header[0] = 0;
	    header[1] = 0;
	  }
	/* size of sample buffer (+ header) */
	header[2] = PROFILE_PC_NR_BUCKETS (profile) * 2 + sizeof (header);

	/* Header must be written out in target byte order.  */
	H2T (header[0]);
	H2T (header[1]);
	H2T (header[2]);

	ok = fwrite (&header, sizeof (header), 1, pf);
	for (loop = 0;
	     ok && (loop < PROFILE_PC_NR_BUCKETS (profile));
	     loop++)
	  {
	    signed16 sample;
	    if (PROFILE_PC_COUNT (profile) [loop] >= 0xffff)
	      sample = 0xffff;
	    else
	      sample = PROFILE_PC_COUNT (profile) [loop];
 	    H2T (sample);
	    ok = fwrite (&sample, sizeof (sample), 1, pf);
	  }
	if (ok == 0)
	  sim_io_eprintf (sd, "Failed to write to \"gmon.out\" profile file\n");
	fclose(pf);
      }
  }

  sim_io_printf (sd, "\n");
}

#endif

/* Summary printing support.  */

#if WITH_PROFILE_INSN_P

static SIM_RC
profile_insn_init (SIM_DESC sd)
{
  int c;

  for (c = 0; c < MAX_NR_PROCESSORS; ++c)
    {
      sim_cpu *cpu = STATE_CPU (sd, c);

      if (CPU_MAX_INSNS (cpu) > 0)
	PROFILE_INSN_COUNT (CPU_PROFILE_DATA (cpu)) = NZALLOC (unsigned int, CPU_MAX_INSNS (cpu));
    }

  return SIM_RC_OK;
}

static void
profile_print_insn (sim_cpu *cpu, int verbose)
{
  unsigned int i, n, total, max_val, max_name_len;
  SIM_DESC sd = CPU_STATE (cpu);
  PROFILE_DATA *data = CPU_PROFILE_DATA (cpu);
  char comma_buf[20];

  /* If MAX_INSNS not set, insn profiling isn't supported.  */
  if (CPU_MAX_INSNS (cpu) == 0)
    return;

  sim_io_printf (sd, "Instruction Statistics");
#ifdef SIM_HAVE_ADDR_RANGE
  if (PROFILE_RANGE (data)->ranges)
    sim_io_printf (sd, " (for selected address range(s))");
#endif
  sim_io_printf (sd, "\n\n");

  /* First pass over data computes various things.  */
  max_val = 0;
  total = 0;
  max_name_len = 0;
  for (i = 0; i < CPU_MAX_INSNS (cpu); ++i)
    {
      const char *name = (*CPU_INSN_NAME (cpu)) (cpu, i);

      if (name == NULL)
	continue;
      total += PROFILE_INSN_COUNT (data) [i];
      if (PROFILE_INSN_COUNT (data) [i] > max_val)
	max_val = PROFILE_INSN_COUNT (data) [i];
      n = strlen (name);
      if (n > max_name_len)
	max_name_len = n;
    }
  /* set the total insn count, in case client is being lazy */
  if (! PROFILE_TOTAL_INSN_COUNT (data))
    PROFILE_TOTAL_INSN_COUNT (data) = total;

  sim_io_printf (sd, "  Total: %s insns\n", COMMAS (total));

  if (verbose && max_val != 0)
    {
      /* Now we can print the histogram.  */
      sim_io_printf (sd, "\n");
      for (i = 0; i < CPU_MAX_INSNS (cpu); ++i)
	{
	  const char *name = (*CPU_INSN_NAME (cpu)) (cpu, i);

	  if (name == NULL)
	    continue;
	  if (PROFILE_INSN_COUNT (data) [i] != 0)
	    {
	      sim_io_printf (sd, "   %*s: %*s: ",
			     max_name_len, name,
			     max_val < 10000 ? 5 : 10,
			     COMMAS (PROFILE_INSN_COUNT (data) [i]));
	      sim_profile_print_bar (sd, PROFILE_HISTOGRAM_WIDTH,
				     PROFILE_INSN_COUNT (data) [i],
				     max_val);
	      sim_io_printf (sd, "\n");
	    }
	}
    }

  sim_io_printf (sd, "\n");
}

#endif

#if WITH_PROFILE_MEMORY_P

static void
profile_print_memory (sim_cpu *cpu, int verbose)
{
  unsigned int i, n;
  unsigned int total_read, total_write;
  unsigned int max_val, max_name_len;
  /* FIXME: Need to add smp support.  */
  SIM_DESC sd = CPU_STATE (cpu);
  PROFILE_DATA *data = CPU_PROFILE_DATA (cpu);
  char comma_buf[20];

  sim_io_printf (sd, "Memory Access Statistics\n\n");

  /* First pass over data computes various things.  */
  max_val = total_read = total_write = max_name_len = 0;
  for (i = 0; i < MODE_TARGET_MAX; ++i)
    {
      total_read += PROFILE_READ_COUNT (data) [i];
      total_write += PROFILE_WRITE_COUNT (data) [i];
      if (PROFILE_READ_COUNT (data) [i] > max_val)
	max_val = PROFILE_READ_COUNT (data) [i];
      if (PROFILE_WRITE_COUNT (data) [i] > max_val)
	max_val = PROFILE_WRITE_COUNT (data) [i];
      n = strlen (MODE_NAME (i));
      if (n > max_name_len)
	max_name_len = n;
    }

  /* One could use PROFILE_LABEL_WIDTH here.  I chose not to.  */
  sim_io_printf (sd, "  Total read:  %s accesses\n",
		 COMMAS (total_read));
  sim_io_printf (sd, "  Total write: %s accesses\n",
		 COMMAS (total_write));

  if (verbose && max_val != 0)
    {
      /* FIXME: Need to separate instruction fetches from data fetches
	 as the former swamps the latter.  */
      /* Now we can print the histogram.  */
      sim_io_printf (sd, "\n");
      for (i = 0; i < MODE_TARGET_MAX; ++i)
	{
	  if (PROFILE_READ_COUNT (data) [i] != 0)
	    {
	      sim_io_printf (sd, "   %*s read:  %*s: ",
			     max_name_len, MODE_NAME (i),
			     max_val < 10000 ? 5 : 10,
			     COMMAS (PROFILE_READ_COUNT (data) [i]));
	      sim_profile_print_bar (sd, PROFILE_HISTOGRAM_WIDTH,
				     PROFILE_READ_COUNT (data) [i],
				     max_val);
	      sim_io_printf (sd, "\n");
	    }
	  if (PROFILE_WRITE_COUNT (data) [i] != 0)
	    {
	      sim_io_printf (sd, "   %*s write: %*s: ",
			     max_name_len, MODE_NAME (i),
			     max_val < 10000 ? 5 : 10,
			     COMMAS (PROFILE_WRITE_COUNT (data) [i]));
	      sim_profile_print_bar (sd, PROFILE_HISTOGRAM_WIDTH,
				     PROFILE_WRITE_COUNT (data) [i],
				     max_val);
	      sim_io_printf (sd, "\n");
	    }
	}
    }

  sim_io_printf (sd, "\n");
}

#endif

#if WITH_PROFILE_CORE_P

static void
profile_print_core (sim_cpu *cpu, int verbose)
{
  unsigned int total;
  unsigned int max_val;
  /* FIXME: Need to add smp support.  */
  SIM_DESC sd = CPU_STATE (cpu);
  PROFILE_DATA *data = CPU_PROFILE_DATA (cpu);
  char comma_buf[20];

  sim_io_printf (sd, "CORE Statistics\n\n");

  /* First pass over data computes various things.  */
  {
    unsigned map;
    total = 0;
    max_val = 0;
    for (map = 0; map < nr_maps; map++)
      {
	total += PROFILE_CORE_COUNT (data) [map];
	if (PROFILE_CORE_COUNT (data) [map] > max_val)
	  max_val = PROFILE_CORE_COUNT (data) [map];
      }
  }

  /* One could use PROFILE_LABEL_WIDTH here.  I chose not to.  */
  sim_io_printf (sd, "  Total:  %s accesses\n",
		 COMMAS (total));

  if (verbose && max_val != 0)
    {
      unsigned map;
      /* Now we can print the histogram.  */
      sim_io_printf (sd, "\n");
      for (map = 0; map < nr_maps; map++)
	{
	  if (PROFILE_CORE_COUNT (data) [map] != 0)
	    {
	      sim_io_printf (sd, "%10s:", map_to_str (map));
	      sim_io_printf (sd, "%*s: ",
			     max_val < 10000 ? 5 : 10,
			     COMMAS (PROFILE_CORE_COUNT (data) [map]));
	      sim_profile_print_bar (sd, PROFILE_HISTOGRAM_WIDTH,
				     PROFILE_CORE_COUNT (data) [map],
				     max_val);
	      sim_io_printf (sd, "\n");
	    }
	}
    }

  sim_io_printf (sd, "\n");
}

#endif

#if WITH_PROFILE_MODEL_P

static void
profile_print_model (sim_cpu *cpu, int verbose)
{
  SIM_DESC sd = CPU_STATE (cpu);
  PROFILE_DATA *data = CPU_PROFILE_DATA (cpu);
  unsigned long cti_stall_cycles = PROFILE_MODEL_CTI_STALL_CYCLES (data);
  unsigned long load_stall_cycles = PROFILE_MODEL_LOAD_STALL_CYCLES (data);
  unsigned long total_cycles = PROFILE_MODEL_TOTAL_CYCLES (data);
  char comma_buf[20];

  sim_io_printf (sd, "Model %s Timing Information",
		 MODEL_NAME (CPU_MODEL (cpu)));
#ifdef SIM_HAVE_ADDR_RANGE
  if (PROFILE_RANGE (data)->ranges)
    sim_io_printf (sd, " (for selected address range(s))");
#endif
  sim_io_printf (sd, "\n\n");
  sim_io_printf (sd, "  %-*s %s\n",
		 PROFILE_LABEL_WIDTH, "Taken branches:",
		 COMMAS (PROFILE_MODEL_TAKEN_COUNT (data)));
  sim_io_printf (sd, "  %-*s %s\n",
		 PROFILE_LABEL_WIDTH, "Untaken branches:",
		 COMMAS (PROFILE_MODEL_UNTAKEN_COUNT (data)));
  sim_io_printf (sd, "  %-*s %s\n",
		 PROFILE_LABEL_WIDTH, "Cycles stalled due to branches:",
		 COMMAS (cti_stall_cycles));
  sim_io_printf (sd, "  %-*s %s\n",
		 PROFILE_LABEL_WIDTH, "Cycles stalled due to loads:",
		 COMMAS (load_stall_cycles));
  sim_io_printf (sd, "  %-*s %s\n",
		 PROFILE_LABEL_WIDTH, "Total cycles (*approximate*):",
		 COMMAS (total_cycles));
  sim_io_printf (sd, "\n");
}

#endif

void
sim_profile_print_bar (SIM_DESC sd, unsigned int width,
		       unsigned int val, unsigned int max_val)
{
  unsigned int i, count;

  count = ((double) val / (double) max_val) * (double) width;

  for (i = 0; i < count; ++i)
    sim_io_printf (sd, "*");
}

/* Print the simulator's execution speed for CPU.  */

static void
profile_print_speed (sim_cpu *cpu)
{
  SIM_DESC sd = CPU_STATE (cpu);
  PROFILE_DATA *data = CPU_PROFILE_DATA (cpu);
  unsigned long milliseconds = sim_events_elapsed_time (sd);
  unsigned long total = PROFILE_TOTAL_INSN_COUNT (data);
  double clock;
  double secs;
  char comma_buf[20];

  sim_io_printf (sd, "Simulator Execution Speed\n\n");

  if (total != 0)
    sim_io_printf (sd, "  Total instructions:      %s\n", COMMAS (total));

  if (milliseconds < 1000)
    sim_io_printf (sd, "  Total execution time:    < 1 second\n\n");
  else
    {
      /* The printing of the time rounded to 2 decimal places makes the speed
	 calculation seem incorrect [even though it is correct].  So round
	 MILLISECONDS first. This can marginally affect the result, but it's
	 better that the user not perceive there's a math error.  */
      secs = (double) milliseconds / 1000;
      secs = ((double) (unsigned long) (secs * 100 + .5)) / 100;
      sim_io_printf (sd, "  Total execution time   : %.2f seconds\n", secs);
      /* Don't confuse things with data that isn't useful.
	 If we ran for less than 2 seconds, only use the data if we
	 executed more than 100,000 insns.  */
      if (secs >= 2 || total >= 100000)
	sim_io_printf (sd, "  Simulator speed:         %s insns/second\n",
		       COMMAS ((unsigned long) ((double) total / secs)));
    }

  /* Print simulated execution time if the cpu frequency has been specified.  */
  clock = PROFILE_CPU_FREQ (data);
  if (clock != 0)
    {
      if (clock >= 1000000)
	sim_io_printf (sd, "  Simulated cpu frequency: %.2f MHz\n",
		       clock / 1000000);
      else
	sim_io_printf (sd, "  Simulated cpu frequency: %.2f Hz\n", clock);

#if WITH_PROFILE_MODEL_P
      if (PROFILE_FLAGS (data) [PROFILE_MODEL_IDX])
	{
	  /* The printing of the time rounded to 2 decimal places makes the
	     speed calculation seem incorrect [even though it is correct].
	     So round 	 SECS first. This can marginally affect the result,
	     but it's 	 better that the user not perceive there's a math
	     error.  */
	  secs = PROFILE_MODEL_TOTAL_CYCLES (data) / clock;
	  secs = ((double) (unsigned long) (secs * 100 + .5)) / 100;
	  sim_io_printf (sd, "  Simulated execution time: %.2f seconds\n",
			 secs);
	}
#endif /* WITH_PROFILE_MODEL_P */
    }
}

/* Print selected address ranges.  */

static void
profile_print_addr_ranges (sim_cpu *cpu)
{
  ADDR_SUBRANGE *asr = PROFILE_RANGE (CPU_PROFILE_DATA (cpu))->ranges;
  SIM_DESC sd = CPU_STATE (cpu);

  if (asr)
    {
      sim_io_printf (sd, "Selected address ranges\n\n");
      while (asr != NULL)
	{
	  sim_io_printf (sd, "  0x%lx - 0x%lx\n",
			 (long) asr->start, (long) asr->end);
	  asr = asr->next;
	}
      sim_io_printf (sd, "\n");
    }
}

/* Top level function to print all summary profile information.
   It is [currently] intended that all such data is printed by this function.
   I'd rather keep it all in one place for now.  To that end, MISC_CPU and
   MISC are callbacks used to print any miscellaneous data.

   One might want to add a user option that allows printing by type or by cpu
   (i.e. print all insn data for each cpu first, or print data cpu by cpu).
   This may be a case of featuritis so it's currently left out.

   Note that results are indented two spaces to distinguish them from
   section titles.  */

static void
profile_info (SIM_DESC sd, int verbose)
{
  int i,c;
  int print_title_p = 0;

  /* Only print the title if some data has been collected.  */
  /* ??? Why don't we just exit if no data collected?  */
  /* FIXME: If the number of processors can be selected on the command line,
     then MAX_NR_PROCESSORS will need to take an argument of `sd'.  */

  for (c = 0; c < MAX_NR_PROCESSORS; ++c)
    {
      sim_cpu *cpu = STATE_CPU (sd, c);
      PROFILE_DATA *data = CPU_PROFILE_DATA (cpu);

      for (i = 0; i < MAX_PROFILE_VALUES; ++i)
	if (PROFILE_FLAGS (data) [i])
	  print_title_p = 1;
      /* One could break out early if print_title_p is set.  */
    }
  if (print_title_p)
    sim_io_printf (sd, "Summary profiling results:\n\n");

  /* Loop, cpu by cpu, printing results.  */

  for (c = 0; c < MAX_NR_PROCESSORS; ++c)
    {
      sim_cpu *cpu = STATE_CPU (sd, c);
      PROFILE_DATA *data = CPU_PROFILE_DATA (cpu);

      if (MAX_NR_PROCESSORS > 1
	  && (0
#if WITH_PROFILE_INSN_P
	      || PROFILE_FLAGS (data) [PROFILE_INSN_IDX]
#endif
#if WITH_PROFILE_MEMORY_P
	      || PROFILE_FLAGS (data) [PROFILE_MEMORY_IDX]
#endif
#if WITH_PROFILE_CORE_P
	      || PROFILE_FLAGS (data) [PROFILE_CORE_IDX]
#endif
#if WITH_PROFILE_MODEL_P
	      || PROFILE_FLAGS (data) [PROFILE_MODEL_IDX]
#endif
#if WITH_PROFILE_SCACHE_P && WITH_SCACHE
	      || PROFILE_FLAGS (data) [PROFILE_SCACHE_IDX]
#endif
#if WITH_PROFILE_PC_P
	      || PROFILE_FLAGS (data) [PROFILE_PC_IDX]
#endif
	      ))
	{
	  sim_io_printf (sd, "CPU %d\n\n", c);
	}

#ifdef SIM_HAVE_ADDR_RANGE
      if (print_title_p
	  && (PROFILE_INSN_P (cpu)
	      || PROFILE_MODEL_P (cpu)))
	profile_print_addr_ranges (cpu);
#endif

#if WITH_PROFILE_INSN_P
      if (PROFILE_FLAGS (data) [PROFILE_INSN_IDX])
	profile_print_insn (cpu, verbose);
#endif

#if WITH_PROFILE_MEMORY_P
      if (PROFILE_FLAGS (data) [PROFILE_MEMORY_IDX])
	profile_print_memory (cpu, verbose);
#endif

#if WITH_PROFILE_CORE_P
      if (PROFILE_FLAGS (data) [PROFILE_CORE_IDX])
	profile_print_core (cpu, verbose);
#endif

#if WITH_PROFILE_MODEL_P
      if (PROFILE_FLAGS (data) [PROFILE_MODEL_IDX])
	profile_print_model (cpu, verbose);
#endif

#if WITH_PROFILE_SCACHE_P && WITH_SCACHE
      if (PROFILE_FLAGS (data) [PROFILE_SCACHE_IDX])
	scache_print_profile (cpu, verbose);
#endif

#if WITH_PROFILE_PC_P
      if (PROFILE_FLAGS (data) [PROFILE_PC_IDX])
	profile_print_pc (cpu, verbose);
#endif

      /* Print cpu-specific data before the execution speed.  */
      if (PROFILE_INFO_CPU_CALLBACK (data) != NULL)
	PROFILE_INFO_CPU_CALLBACK (data) (cpu, verbose);

      /* Always try to print execution time and speed.  */
      if (verbose
	  || PROFILE_FLAGS (data) [PROFILE_INSN_IDX])
	profile_print_speed (cpu);
    }

  /* Finally print non-cpu specific miscellaneous data.  */
  if (STATE_PROFILE_INFO_CALLBACK (sd))
    STATE_PROFILE_INFO_CALLBACK (sd) (sd, verbose);

}

/* Install profiling support in the simulator.  */

SIM_RC
profile_install (SIM_DESC sd)
{
  int i;

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  sim_add_option_table (sd, NULL, profile_options);
  for (i = 0; i < MAX_NR_PROCESSORS; ++i)
    memset (CPU_PROFILE_DATA (STATE_CPU (sd, i)), 0,
	    sizeof (* CPU_PROFILE_DATA (STATE_CPU (sd, i))));
#if WITH_PROFILE_INSN_P
  sim_module_add_init_fn (sd, profile_insn_init);
#endif
#if WITH_PROFILE_PC_P
  sim_module_add_uninstall_fn (sd, profile_pc_uninstall);
  sim_module_add_init_fn (sd, profile_pc_init);
#endif
  sim_module_add_init_fn (sd, profile_init);
  sim_module_add_uninstall_fn (sd, profile_uninstall);
  sim_module_add_info_fn (sd, profile_info);
  return SIM_RC_OK;
}

static SIM_RC
profile_init (SIM_DESC sd)
{
#ifdef SIM_HAVE_ADDR_RANGE
  /* Check if a range has been specified without specifying what to
     collect.  */
  {
    int i;

    for (i = 0; i < MAX_NR_PROCESSORS; ++i)
      {
	sim_cpu *cpu = STATE_CPU (sd, i);

	if (ADDR_RANGE_RANGES (PROFILE_RANGE (CPU_PROFILE_DATA (cpu)))
	    && ! (PROFILE_INSN_P (cpu)
		  || PROFILE_MODEL_P (cpu)))
	  {
	    sim_io_eprintf_cpu (cpu, "Profiling address range specified without --profile-insn or --profile-model.\n");
	    sim_io_eprintf_cpu (cpu, "Address range ignored.\n");
	    sim_addr_range_delete (PROFILE_RANGE (CPU_PROFILE_DATA (cpu)),
				   0, ~ (address_word) 0);
	  }
      }
  }
#endif

  return SIM_RC_OK;
}

static void
profile_uninstall (SIM_DESC sd)
{
  int i,j;

  for (i = 0; i < MAX_NR_PROCESSORS; ++i)
    {
      sim_cpu *cpu = STATE_CPU (sd, i);
      PROFILE_DATA *data = CPU_PROFILE_DATA (cpu);

      if (PROFILE_FILE (data) != NULL)
	{
	  /* If output from different cpus is going to the same file,
	     avoid closing the file twice.  */
	  for (j = 0; j < i; ++j)
	    if (PROFILE_FILE (CPU_PROFILE_DATA (STATE_CPU (sd, j)))
		== PROFILE_FILE (data))
	      break;
	  if (i == j)
	    fclose (PROFILE_FILE (data));
	}

      if (PROFILE_INSN_COUNT (data) != NULL)
	zfree (PROFILE_INSN_COUNT (data));
    }
}
