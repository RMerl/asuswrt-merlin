/* Model support.
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

#include "sim-main.h"
#include "libiberty.h"
#include "sim-options.h"
#include "sim-io.h"
#include "sim-assert.h"
#include "bfd.h"

static void model_set (sim_cpu *, const MODEL *);

static DECLARE_OPTION_HANDLER (model_option_handler);

static MODULE_INIT_FN sim_model_init;

#define OPTION_MODEL (OPTION_START + 0)

static const OPTION model_options[] = {
  { {"model", required_argument, NULL, OPTION_MODEL},
      '\0', "MODEL", "Specify model to simulate",
      model_option_handler },
  { {NULL, no_argument, NULL, 0}, '\0', NULL, NULL, NULL }
};

static SIM_RC
model_option_handler (SIM_DESC sd, sim_cpu *cpu, int opt,
		      char *arg, int is_command)
{
  switch (opt)
    {
    case OPTION_MODEL :
      {
	const MODEL *model = sim_model_lookup (arg);
	if (! model)
	  {
	    sim_io_eprintf (sd, "unknown model `%s'\n", arg);
	    return SIM_RC_FAIL;
	  }
	sim_model_set (sd, cpu, model);
	break;
      }
    }

  return SIM_RC_OK;
}

SIM_RC
sim_model_install (SIM_DESC sd)
{
  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);

  sim_add_option_table (sd, NULL, model_options);
  sim_module_add_init_fn (sd, sim_model_init);

  return SIM_RC_OK;
}

/* Subroutine of sim_model_set to set the model for one cpu.  */

static void
model_set (sim_cpu *cpu, const MODEL *model)
{
  CPU_MACH (cpu) = MODEL_MACH (model);
  CPU_MODEL (cpu) = model;
  (* MACH_INIT_CPU (MODEL_MACH (model))) (cpu);
  (* MODEL_INIT (model)) (cpu);
}

/* Set the current model of CPU to MODEL.
   If CPU is NULL, all cpus are set to MODEL.  */

void
sim_model_set (SIM_DESC sd, sim_cpu *cpu, const MODEL *model)
{
  if (! cpu)
    {
      int c;

      for (c = 0; c < MAX_NR_PROCESSORS; ++c)
	if (STATE_CPU (sd, c))
	  model_set (STATE_CPU (sd, c), model);
    }
  else
    {
      model_set (cpu, model);
    }
}

/* Look up model named NAME.
   Result is pointer to MODEL entry or NULL if not found.  */

const MODEL *
sim_model_lookup (const char *name)
{
  const MACH **machp;
  const MODEL *model;

  for (machp = & sim_machs[0]; *machp != NULL; ++machp)
    {
      for (model = MACH_MODELS (*machp); MODEL_NAME (model) != NULL; ++model)
	{
	  if (strcmp (MODEL_NAME (model), name) == 0)
	    return model;
	}
    }
  return NULL;
}

/* Look up machine named NAME.
   Result is pointer to MACH entry or NULL if not found.  */

const MACH *
sim_mach_lookup (const char *name)
{
  const MACH **machp;

  for (machp = & sim_machs[0]; *machp != NULL; ++machp)
    {
      if (strcmp (MACH_NAME (*machp), name) == 0)
	return *machp;
    }
  return NULL;
}

/* Look up a machine via its bfd name.
   Result is pointer to MACH entry or NULL if not found.  */

const MACH *
sim_mach_lookup_bfd_name (const char *name)
{
  const MACH **machp;

  for (machp = & sim_machs[0]; *machp != NULL; ++machp)
    {
      if (strcmp (MACH_BFD_NAME (*machp), name) == 0)
	return *machp;
    }
  return NULL;
}

/* Initialize model support.  */

static SIM_RC
sim_model_init (SIM_DESC sd)
{
  SIM_CPU *cpu;

  /* If both cpu model and state architecture are set, ensure they're
     compatible.  If only one is set, set the other.  If neither are set,
     use the default model.  STATE_ARCHITECTURE is the bfd_arch_info data
     for the selected "mach" (bfd terminology).  */

  /* Only check cpu 0.  STATE_ARCHITECTURE is for that one only.  */
  /* ??? At present this only supports homogeneous multiprocessors.  */
  cpu = STATE_CPU (sd, 0);

  if (! STATE_ARCHITECTURE (sd)
      && ! CPU_MACH (cpu))
    {
      /* Set the default model.  */
      const MODEL *model = sim_model_lookup (WITH_DEFAULT_MODEL);
      sim_model_set (sd, NULL, model);
    }

  if (STATE_ARCHITECTURE (sd)
      && CPU_MACH (cpu))
    {
      if (strcmp (STATE_ARCHITECTURE (sd)->printable_name,
		  MACH_BFD_NAME (CPU_MACH (cpu))) != 0)
	{
	  sim_io_eprintf (sd, "invalid model `%s' for `%s'\n",
			  MODEL_NAME (CPU_MODEL (cpu)),
			  STATE_ARCHITECTURE (sd)->printable_name);
	  return SIM_RC_FAIL;
	}
    }
  else if (STATE_ARCHITECTURE (sd))
    {
      /* Use the default model for the selected machine.
	 The default model is the first one in the list.  */
      const MACH *mach = sim_mach_lookup_bfd_name (STATE_ARCHITECTURE (sd)->printable_name);

      if (mach == NULL)
	{
	  sim_io_eprintf (sd, "unsupported machine `%s'\n",
			  STATE_ARCHITECTURE (sd)->printable_name);
	  return SIM_RC_FAIL;
	}
      sim_model_set (sd, NULL, MACH_MODELS (mach));
    }
  else
    {
      STATE_ARCHITECTURE (sd) = bfd_scan_arch (MACH_BFD_NAME (CPU_MACH (cpu)));
    }

  return SIM_RC_OK;
}
