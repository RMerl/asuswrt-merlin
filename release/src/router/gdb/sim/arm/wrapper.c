/* run front end support for arm
   Copyright (C) 1995, 1996, 1997, 2000, 2001, 2002, 2007
   Free Software Foundation, Inc.

   This file is part of ARM SIM.

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

/* This file provides the interface between the simulator and
   run.c and gdb (when the simulator is linked with gdb).
   All simulator interaction should go through this file.  */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <bfd.h>
#include <signal.h>
#include "gdb/callback.h"
#include "gdb/remote-sim.h"
#include "armdefs.h"
#include "armemu.h"
#include "dbg_rdi.h"
#include "ansidecl.h"
#include "sim-utils.h"
#include "run-sim.h"
#include "gdb/sim-arm.h"
#include "gdb/signals.h"

host_callback *sim_callback;

static struct ARMul_State *state;

/* Who is using the simulator.  */
static SIM_OPEN_KIND sim_kind;

/* argv[0] */
static char *myname;

/* Memory size in bytes.  */
static int mem_size = (1 << 21);

/* Non-zero to display start up banner, and maybe other things.  */
static int verbosity;

/* Non-zero to set big endian mode.  */
static int big_endian;

int stop_simulator;

/* Cirrus DSP registers.

   We need to define these registers outside of maverick.c because
   maverick.c might not be linked in unless --target=arm9e-* in which
   case wrapper.c will not compile because it tries to access Cirrus
   registers.  This should all go away once we get the Cirrus and ARM
   Coprocessor to coexist in armcopro.c-- aldyh.  */

struct maverick_regs
{
  union
  {
    int i;
    float f;
  } upper;
  
  union
  {
    int i;
    float f;
  } lower;
};

union maverick_acc_regs
{
  long double ld;		/* Acc registers are 72-bits.  */
};

struct maverick_regs     DSPregs[16];
union maverick_acc_regs  DSPacc[4];
ARMword DSPsc;

static void
init ()
{
  static int done;

  if (!done)
    {
      ARMul_EmulateInit ();
      state = ARMul_NewState ();
      state->bigendSig = (big_endian ? HIGH : LOW);
      ARMul_MemoryInit (state, mem_size);
      ARMul_OSInit (state);
      state->verbose = verbosity;
      done = 1;
    }
}

/* Set verbosity level of simulator.
   This is not intended to produce detailed tracing or debugging information.
   Just summaries.  */
/* FIXME: common/run.c doesn't do this yet.  */

void
sim_set_verbose (v)
     int v;
{
  verbosity = v;
}

/* Set the memory size to SIZE bytes.
   Must be called before initializing simulator.  */
/* FIXME: Rename to sim_set_mem_size.  */

void
sim_size (size)
     int size;
{
  mem_size = size;
}

void
ARMul_ConsolePrint VPARAMS ((ARMul_State * state,
			     const char * format,
			     ...))
{
  va_list ap;

  if (state->verbose)
    {
      va_start (ap, format);
      vprintf (format, ap);
      va_end (ap);
    }
}

ARMword
ARMul_Debug (state, pc, instr)
     ARMul_State * state ATTRIBUTE_UNUSED;
     ARMword       pc    ATTRIBUTE_UNUSED;
     ARMword       instr ATTRIBUTE_UNUSED;
{
  return 0;
}

int
sim_write (sd, addr, buffer, size)
     SIM_DESC sd ATTRIBUTE_UNUSED;
     SIM_ADDR addr;
     unsigned char * buffer;
     int size;
{
  int i;

  init ();

  for (i = 0; i < size; i++)
    ARMul_SafeWriteByte (state, addr + i, buffer[i]);

  return size;
}

int
sim_read (sd, addr, buffer, size)
     SIM_DESC sd ATTRIBUTE_UNUSED;
     SIM_ADDR addr;
     unsigned char * buffer;
     int size;
{
  int i;

  init ();

  for (i = 0; i < size; i++)
    buffer[i] = ARMul_SafeReadByte (state, addr + i);

  return size;
}

int
sim_trace (sd)
     SIM_DESC sd ATTRIBUTE_UNUSED;
{  
  (*sim_callback->printf_filtered)
    (sim_callback,
     "This simulator does not support tracing\n");
  return 1;
}

int
sim_stop (sd)
     SIM_DESC sd ATTRIBUTE_UNUSED;
{
  state->Emulate = STOP;
  stop_simulator = 1;
  return 1;
}

void
sim_resume (sd, step, siggnal)
     SIM_DESC sd ATTRIBUTE_UNUSED;
     int step;
     int siggnal ATTRIBUTE_UNUSED;
{
  state->EndCondition = 0;
  stop_simulator = 0;

  if (step)
    {
      state->Reg[15] = ARMul_DoInstr (state);
      if (state->EndCondition == 0)
	state->EndCondition = RDIError_BreakpointReached;
    }
  else
    {
      state->NextInstr = RESUME;	/* treat as PC change */
      state->Reg[15] = ARMul_DoProg (state);
    }

  FLUSHPIPE;
}

SIM_RC
sim_create_inferior (sd, abfd, argv, env)
     SIM_DESC sd ATTRIBUTE_UNUSED;
     struct bfd * abfd;
     char ** argv;
     char ** env;
{
  int argvlen = 0;
  int mach;
  char **arg;

  if (abfd != NULL)
    ARMul_SetPC (state, bfd_get_start_address (abfd));
  else
    ARMul_SetPC (state, 0);	/* ??? */

  mach = bfd_get_mach (abfd);

  switch (mach)
    {
    default:
      (*sim_callback->printf_filtered)
	(sim_callback,
	 "Unknown machine type '%d'; please update sim_create_inferior.\n",
	 mach);
      /* fall through */

    case 0:
      /* We wouldn't set the machine type with earlier toolchains, so we
	 explicitly select a processor capable of supporting all ARMs in
	 32bit mode.  */
      /* We choose the XScale rather than the iWMMXt, because the iWMMXt
	 removes the FPE emulator, since it conflicts with its coprocessors.
	 For the most generic ARM support, we want the FPE emulator in place.  */
    case bfd_mach_arm_XScale:
      ARMul_SelectProcessor (state, ARM_v5_Prop | ARM_v5e_Prop | ARM_XScale_Prop | ARM_v6_Prop);
      break;

    case bfd_mach_arm_iWMMXt:
      {
	extern int SWI_vector_installed;
	ARMword i;

	if (! SWI_vector_installed)
	  {
	    /* Intialise the hardware vectors to zero.  */
	    if (! SWI_vector_installed)
	      for (i = ARMul_ResetV; i <= ARMFIQV; i += 4)
		ARMul_WriteWord (state, i, 0);

	    /* ARM_WriteWord will have detected the write to the SWI vector,
	       but we want SWI_vector_installed to remain at 0 so that thumb
	       mode breakpoints will work.  */
	    SWI_vector_installed = 0;
	  }
      }
      ARMul_SelectProcessor (state, ARM_v5_Prop | ARM_v5e_Prop | ARM_XScale_Prop | ARM_iWMMXt_Prop);
      break;

    case bfd_mach_arm_ep9312:
      ARMul_SelectProcessor (state, ARM_v4_Prop | ARM_ep9312_Prop);
      break;

    case bfd_mach_arm_5:
      if (bfd_family_coff (abfd))
	{
	  /* This is a special case in order to support COFF based ARM toolchains.
	     The COFF header does not have enough room to store all the different
	     kinds of ARM cpu, so the XScale, v5T and v5TE architectures all default
	     to v5.  (See coff_set_flags() in bdf/coffcode.h).  So if we see a v5
	     machine type here, we assume it could be any of the above architectures
	     and so select the most feature-full.  */
	  ARMul_SelectProcessor (state, ARM_v5_Prop | ARM_v5e_Prop | ARM_XScale_Prop);
	  break;
	}
      /* Otherwise drop through.  */

    case bfd_mach_arm_5T:
      ARMul_SelectProcessor (state, ARM_v5_Prop);
      break;

    case bfd_mach_arm_5TE:
      ARMul_SelectProcessor (state, ARM_v5_Prop | ARM_v5e_Prop);
      break;

    case bfd_mach_arm_4:
    case bfd_mach_arm_4T:
      ARMul_SelectProcessor (state, ARM_v4_Prop);
      break;

    case bfd_mach_arm_3:
    case bfd_mach_arm_3M:
      ARMul_SelectProcessor (state, ARM_Lock_Prop);
      break;

    case bfd_mach_arm_2:
    case bfd_mach_arm_2a:
      ARMul_SelectProcessor (state, ARM_Fix26_Prop);
      break;
    }

  if (   mach != bfd_mach_arm_3
      && mach != bfd_mach_arm_3M
      && mach != bfd_mach_arm_2
      && mach != bfd_mach_arm_2a)
    {
      /* Reset mode to ARM.  A gdb user may rerun a program that had entered
	 THUMB mode from the start and cause the ARM-mode startup code to be
	 executed in THUMB mode.  */
      ARMul_SetCPSR (state, SVC32MODE);
    }
  
  if (argv != NULL)
    {
      /* Set up the command line by laboriously stringing together
	 the environment carefully picked apart by our caller.  */

      /* Free any old stuff.  */
      if (state->CommandLine != NULL)
	{
	  free (state->CommandLine);
	  state->CommandLine = NULL;
	}

      /* See how much we need.  */
      for (arg = argv; *arg != NULL; arg++)
	argvlen += strlen (*arg) + 1;

      /* Allocate it.  */
      state->CommandLine = malloc (argvlen + 1);
      if (state->CommandLine != NULL)
	{
	  arg = argv;
	  state->CommandLine[0] = '\0';

	  for (arg = argv; *arg != NULL; arg++)
	    {
	      strcat (state->CommandLine, *arg);
	      strcat (state->CommandLine, " ");
	    }
	}
    }

  if (env != NULL)
    {
      /* Now see if there's a MEMSIZE spec in the environment.  */
      while (*env)
	{
	  if (strncmp (*env, "MEMSIZE=", sizeof ("MEMSIZE=") - 1) == 0)
	    {
	      char *end_of_num;

	      /* Set up memory limit.  */
	      state->MemSize =
		strtoul (*env + sizeof ("MEMSIZE=") - 1, &end_of_num, 0);
	    }
	  env++;
	}
    }

  return SIM_RC_OK;
}

void
sim_info (sd, verbose)
     SIM_DESC sd ATTRIBUTE_UNUSED;
     int verbose ATTRIBUTE_UNUSED;
{
}

static int
frommem (state, memory)
     struct ARMul_State *state;
     unsigned char *memory;
{
  if (state->bigendSig == HIGH)
    return (memory[0] << 24) | (memory[1] << 16)
      | (memory[2] << 8) | (memory[3] << 0);
  else
    return (memory[3] << 24) | (memory[2] << 16)
      | (memory[1] << 8) | (memory[0] << 0);
}

static void
tomem (state, memory, val)
     struct ARMul_State *state;
     unsigned char *memory;
     int val;
{
  if (state->bigendSig == HIGH)
    {
      memory[0] = val >> 24;
      memory[1] = val >> 16;
      memory[2] = val >> 8;
      memory[3] = val >> 0;
    }
  else
    {
      memory[3] = val >> 24;
      memory[2] = val >> 16;
      memory[1] = val >> 8;
      memory[0] = val >> 0;
    }
}

int
sim_store_register (sd, rn, memory, length)
     SIM_DESC sd ATTRIBUTE_UNUSED;
     int rn;
     unsigned char *memory;
     int length ATTRIBUTE_UNUSED;
{
  init ();

  switch ((enum sim_arm_regs) rn)
    {
    case SIM_ARM_R0_REGNUM:
    case SIM_ARM_R1_REGNUM:
    case SIM_ARM_R2_REGNUM:
    case SIM_ARM_R3_REGNUM:
    case SIM_ARM_R4_REGNUM:
    case SIM_ARM_R5_REGNUM:
    case SIM_ARM_R6_REGNUM:
    case SIM_ARM_R7_REGNUM:
    case SIM_ARM_R8_REGNUM:
    case SIM_ARM_R9_REGNUM:
    case SIM_ARM_R10_REGNUM:
    case SIM_ARM_R11_REGNUM:
    case SIM_ARM_R12_REGNUM:
    case SIM_ARM_R13_REGNUM:
    case SIM_ARM_R14_REGNUM:
    case SIM_ARM_R15_REGNUM: /* PC */
    case SIM_ARM_FP0_REGNUM:
    case SIM_ARM_FP1_REGNUM:
    case SIM_ARM_FP2_REGNUM:
    case SIM_ARM_FP3_REGNUM:
    case SIM_ARM_FP4_REGNUM:
    case SIM_ARM_FP5_REGNUM:
    case SIM_ARM_FP6_REGNUM:
    case SIM_ARM_FP7_REGNUM:
    case SIM_ARM_FPS_REGNUM:
      ARMul_SetReg (state, state->Mode, rn, frommem (state, memory));
      break;

    case SIM_ARM_PS_REGNUM:
      state->Cpsr = frommem (state, memory);
      ARMul_CPSRAltered (state);
      break;

    case SIM_ARM_MAVERIC_COP0R0_REGNUM:
    case SIM_ARM_MAVERIC_COP0R1_REGNUM:
    case SIM_ARM_MAVERIC_COP0R2_REGNUM:
    case SIM_ARM_MAVERIC_COP0R3_REGNUM:
    case SIM_ARM_MAVERIC_COP0R4_REGNUM:
    case SIM_ARM_MAVERIC_COP0R5_REGNUM:
    case SIM_ARM_MAVERIC_COP0R6_REGNUM:
    case SIM_ARM_MAVERIC_COP0R7_REGNUM:
    case SIM_ARM_MAVERIC_COP0R8_REGNUM:
    case SIM_ARM_MAVERIC_COP0R9_REGNUM:
    case SIM_ARM_MAVERIC_COP0R10_REGNUM:
    case SIM_ARM_MAVERIC_COP0R11_REGNUM:
    case SIM_ARM_MAVERIC_COP0R12_REGNUM:
    case SIM_ARM_MAVERIC_COP0R13_REGNUM:
    case SIM_ARM_MAVERIC_COP0R14_REGNUM:
    case SIM_ARM_MAVERIC_COP0R15_REGNUM:
      memcpy (& DSPregs [rn - SIM_ARM_MAVERIC_COP0R0_REGNUM],
	      memory, sizeof (struct maverick_regs));
      return sizeof (struct maverick_regs);

    case SIM_ARM_MAVERIC_DSPSC_REGNUM:
      memcpy (&DSPsc, memory, sizeof DSPsc);
      return sizeof DSPsc;

    case SIM_ARM_IWMMXT_COP0R0_REGNUM:
    case SIM_ARM_IWMMXT_COP0R1_REGNUM:
    case SIM_ARM_IWMMXT_COP0R2_REGNUM:
    case SIM_ARM_IWMMXT_COP0R3_REGNUM:
    case SIM_ARM_IWMMXT_COP0R4_REGNUM:
    case SIM_ARM_IWMMXT_COP0R5_REGNUM:
    case SIM_ARM_IWMMXT_COP0R6_REGNUM:
    case SIM_ARM_IWMMXT_COP0R7_REGNUM:
    case SIM_ARM_IWMMXT_COP0R8_REGNUM:
    case SIM_ARM_IWMMXT_COP0R9_REGNUM:
    case SIM_ARM_IWMMXT_COP0R10_REGNUM:
    case SIM_ARM_IWMMXT_COP0R11_REGNUM:
    case SIM_ARM_IWMMXT_COP0R12_REGNUM:
    case SIM_ARM_IWMMXT_COP0R13_REGNUM:
    case SIM_ARM_IWMMXT_COP0R14_REGNUM:
    case SIM_ARM_IWMMXT_COP0R15_REGNUM:
    case SIM_ARM_IWMMXT_COP1R0_REGNUM:
    case SIM_ARM_IWMMXT_COP1R1_REGNUM:
    case SIM_ARM_IWMMXT_COP1R2_REGNUM:
    case SIM_ARM_IWMMXT_COP1R3_REGNUM:
    case SIM_ARM_IWMMXT_COP1R4_REGNUM:
    case SIM_ARM_IWMMXT_COP1R5_REGNUM:
    case SIM_ARM_IWMMXT_COP1R6_REGNUM:
    case SIM_ARM_IWMMXT_COP1R7_REGNUM:
    case SIM_ARM_IWMMXT_COP1R8_REGNUM:
    case SIM_ARM_IWMMXT_COP1R9_REGNUM:
    case SIM_ARM_IWMMXT_COP1R10_REGNUM:
    case SIM_ARM_IWMMXT_COP1R11_REGNUM:
    case SIM_ARM_IWMMXT_COP1R12_REGNUM:
    case SIM_ARM_IWMMXT_COP1R13_REGNUM:
    case SIM_ARM_IWMMXT_COP1R14_REGNUM:
    case SIM_ARM_IWMMXT_COP1R15_REGNUM:
      return Store_Iwmmxt_Register (rn - SIM_ARM_IWMMXT_COP0R0_REGNUM, memory);

    default:
      return 0;
    }

  return -1;
}

int
sim_fetch_register (sd, rn, memory, length)
     SIM_DESC sd ATTRIBUTE_UNUSED;
     int rn;
     unsigned char *memory;
     int length ATTRIBUTE_UNUSED;
{
  ARMword regval;

  init ();

  switch ((enum sim_arm_regs) rn)
    {
    case SIM_ARM_R0_REGNUM:
    case SIM_ARM_R1_REGNUM:
    case SIM_ARM_R2_REGNUM:
    case SIM_ARM_R3_REGNUM:
    case SIM_ARM_R4_REGNUM:
    case SIM_ARM_R5_REGNUM:
    case SIM_ARM_R6_REGNUM:
    case SIM_ARM_R7_REGNUM:
    case SIM_ARM_R8_REGNUM:
    case SIM_ARM_R9_REGNUM:
    case SIM_ARM_R10_REGNUM:
    case SIM_ARM_R11_REGNUM:
    case SIM_ARM_R12_REGNUM:
    case SIM_ARM_R13_REGNUM:
    case SIM_ARM_R14_REGNUM:
    case SIM_ARM_R15_REGNUM: /* PC */
      regval = ARMul_GetReg (state, state->Mode, rn);
      break;

    case SIM_ARM_FP0_REGNUM:
    case SIM_ARM_FP1_REGNUM:
    case SIM_ARM_FP2_REGNUM:
    case SIM_ARM_FP3_REGNUM:
    case SIM_ARM_FP4_REGNUM:
    case SIM_ARM_FP5_REGNUM:
    case SIM_ARM_FP6_REGNUM:
    case SIM_ARM_FP7_REGNUM:
    case SIM_ARM_FPS_REGNUM:
      memset (memory, 0, length);
      return 0;

    case SIM_ARM_PS_REGNUM:
      regval = ARMul_GetCPSR (state);
      break;

    case SIM_ARM_MAVERIC_COP0R0_REGNUM:
    case SIM_ARM_MAVERIC_COP0R1_REGNUM:
    case SIM_ARM_MAVERIC_COP0R2_REGNUM:
    case SIM_ARM_MAVERIC_COP0R3_REGNUM:
    case SIM_ARM_MAVERIC_COP0R4_REGNUM:
    case SIM_ARM_MAVERIC_COP0R5_REGNUM:
    case SIM_ARM_MAVERIC_COP0R6_REGNUM:
    case SIM_ARM_MAVERIC_COP0R7_REGNUM:
    case SIM_ARM_MAVERIC_COP0R8_REGNUM:
    case SIM_ARM_MAVERIC_COP0R9_REGNUM:
    case SIM_ARM_MAVERIC_COP0R10_REGNUM:
    case SIM_ARM_MAVERIC_COP0R11_REGNUM:
    case SIM_ARM_MAVERIC_COP0R12_REGNUM:
    case SIM_ARM_MAVERIC_COP0R13_REGNUM:
    case SIM_ARM_MAVERIC_COP0R14_REGNUM:
    case SIM_ARM_MAVERIC_COP0R15_REGNUM:
      memcpy (memory, & DSPregs [rn - SIM_ARM_MAVERIC_COP0R0_REGNUM],
	      sizeof (struct maverick_regs));
      return sizeof (struct maverick_regs);

    case SIM_ARM_MAVERIC_DSPSC_REGNUM:
      memcpy (memory, & DSPsc, sizeof DSPsc);
      return sizeof DSPsc;

    case SIM_ARM_IWMMXT_COP0R0_REGNUM:
    case SIM_ARM_IWMMXT_COP0R1_REGNUM:
    case SIM_ARM_IWMMXT_COP0R2_REGNUM:
    case SIM_ARM_IWMMXT_COP0R3_REGNUM:
    case SIM_ARM_IWMMXT_COP0R4_REGNUM:
    case SIM_ARM_IWMMXT_COP0R5_REGNUM:
    case SIM_ARM_IWMMXT_COP0R6_REGNUM:
    case SIM_ARM_IWMMXT_COP0R7_REGNUM:
    case SIM_ARM_IWMMXT_COP0R8_REGNUM:
    case SIM_ARM_IWMMXT_COP0R9_REGNUM:
    case SIM_ARM_IWMMXT_COP0R10_REGNUM:
    case SIM_ARM_IWMMXT_COP0R11_REGNUM:
    case SIM_ARM_IWMMXT_COP0R12_REGNUM:
    case SIM_ARM_IWMMXT_COP0R13_REGNUM:
    case SIM_ARM_IWMMXT_COP0R14_REGNUM:
    case SIM_ARM_IWMMXT_COP0R15_REGNUM:
    case SIM_ARM_IWMMXT_COP1R0_REGNUM:
    case SIM_ARM_IWMMXT_COP1R1_REGNUM:
    case SIM_ARM_IWMMXT_COP1R2_REGNUM:
    case SIM_ARM_IWMMXT_COP1R3_REGNUM:
    case SIM_ARM_IWMMXT_COP1R4_REGNUM:
    case SIM_ARM_IWMMXT_COP1R5_REGNUM:
    case SIM_ARM_IWMMXT_COP1R6_REGNUM:
    case SIM_ARM_IWMMXT_COP1R7_REGNUM:
    case SIM_ARM_IWMMXT_COP1R8_REGNUM:
    case SIM_ARM_IWMMXT_COP1R9_REGNUM:
    case SIM_ARM_IWMMXT_COP1R10_REGNUM:
    case SIM_ARM_IWMMXT_COP1R11_REGNUM:
    case SIM_ARM_IWMMXT_COP1R12_REGNUM:
    case SIM_ARM_IWMMXT_COP1R13_REGNUM:
    case SIM_ARM_IWMMXT_COP1R14_REGNUM:
    case SIM_ARM_IWMMXT_COP1R15_REGNUM:
      return Fetch_Iwmmxt_Register (rn - SIM_ARM_IWMMXT_COP0R0_REGNUM, memory);

    default:
      return 0;
    }

  while (length)
    {
      tomem (state, memory, regval);

      length -= 4;
      memory += 4;
      regval = 0;
    }  

  return -1;
}

#ifdef SIM_TARGET_SWITCHES

static void sim_target_parse_arg_array PARAMS ((char **));

typedef struct
{
  char * 	swi_option;
  unsigned int	swi_mask;
} swi_options;

#define SWI_SWITCH	"--swi-support"

static swi_options options[] =
  {
    { "none",    0 },
    { "demon",   SWI_MASK_DEMON },
    { "angel",   SWI_MASK_ANGEL },
    { "redboot", SWI_MASK_REDBOOT },
    { "all",     -1 },
    { "NONE",    0 },
    { "DEMON",   SWI_MASK_DEMON },
    { "ANGEL",   SWI_MASK_ANGEL },
    { "REDBOOT", SWI_MASK_REDBOOT },
    { "ALL",     -1 }
  };


int
sim_target_parse_command_line (argc, argv)
     int argc;
     char ** argv;
{
  int i;

  for (i = 1; i < argc; i++)
    {
      char * ptr = argv[i];
      int arg;

      if ((ptr == NULL) || (* ptr != '-'))
	break;

      if (strncmp (ptr, SWI_SWITCH, sizeof SWI_SWITCH - 1) != 0)
	continue;

      if (ptr[sizeof SWI_SWITCH - 1] == 0)
	{
	  /* Remove this option from the argv array.  */
	  for (arg = i; arg < argc; arg ++)
	    argv[arg] = argv[arg + 1];
	  argc --;
	  
	  ptr = argv[i];
	}
      else
	ptr += sizeof SWI_SWITCH;

      swi_mask = 0;
      
      while (* ptr)
	{
	  int i;

	  for (i = sizeof options / sizeof options[0]; i--;)
	    if (strncmp (ptr, options[i].swi_option,
			 strlen (options[i].swi_option)) == 0)
	      {
		swi_mask |= options[i].swi_mask;
		ptr += strlen (options[i].swi_option);

		if (* ptr == ',')
		  ++ ptr;

		break;
	      }

	  if (i < 0)
	    break;
	}

      if (* ptr != 0)
	fprintf (stderr, "Ignoring swi options: %s\n", ptr);
      
      /* Remove this option from the argv array.  */
      for (arg = i; arg < argc; arg ++)
	argv[arg] = argv[arg + 1];
      argc --;
      i --;
    }
  return argc;
}

static void
sim_target_parse_arg_array (argv)
     char ** argv;
{
  int i;

  for (i = 0; argv[i]; i++)
    ;

  sim_target_parse_command_line (i, argv);
}

void
sim_target_display_usage ()
{
  fprintf (stderr, "%s=<list>  Comma seperated list of SWI protocols to supoport.\n\
                This list can contain: NONE, DEMON, ANGEL, REDBOOT and/or ALL.\n",
	   SWI_SWITCH);
}
#endif

SIM_DESC
sim_open (kind, ptr, abfd, argv)
     SIM_OPEN_KIND kind;
     host_callback *ptr;
     struct bfd *abfd;
     char **argv;
{
  sim_kind = kind;
  if (myname) free (myname);
  myname = (char *) xstrdup (argv[0]);
  sim_callback = ptr;

#ifdef SIM_TARGET_SWITCHES
  sim_target_parse_arg_array (argv);
#endif
  
  /* Decide upon the endian-ness of the processor.
     If we can, get the information from the bfd itself.
     Otherwise look to see if we have been given a command
     line switch that tells us.  Otherwise default to little endian.  */
  if (abfd != NULL)
    big_endian = bfd_big_endian (abfd);
  else if (argv[1] != NULL)
    {
      int i;

      /* Scan for endian-ness and memory-size switches.  */
      for (i = 0; (argv[i] != NULL) && (argv[i][0] != 0); i++)
	if (argv[i][0] == '-' && argv[i][1] == 'E')
	  {
	    char c;

	    if ((c = argv[i][2]) == 0)
	      {
		++i;
		c = argv[i][0];
	      }

	    switch (c)
	      {
	      case 0:
		sim_callback->printf_filtered
		  (sim_callback, "No argument to -E option provided\n");
		break;

	      case 'b':
	      case 'B':
		big_endian = 1;
		break;

	      case 'l':
	      case 'L':
		big_endian = 0;
		break;

	      default:
		sim_callback->printf_filtered
		  (sim_callback, "Unrecognised argument to -E option\n");
		break;
	      }
	  }
	else if (argv[i][0] == '-' && argv[i][1] == 'm')
	  {
	    if (argv[i][2] != '\0')
	      sim_size (atoi (&argv[i][2]));
	    else if (argv[i + 1] != NULL)
	      {
		sim_size (atoi (argv[i + 1]));
		i++;
	      }
	    else
	      {
		sim_callback->printf_filtered (sim_callback,
					       "Missing argument to -m option\n");
		return NULL;
	      }
	      
	  }
    }

  return (SIM_DESC) 1;
}

void
sim_close (sd, quitting)
     SIM_DESC sd ATTRIBUTE_UNUSED;
     int quitting ATTRIBUTE_UNUSED;
{
  if (myname)
    free (myname);
  myname = NULL;
}

SIM_RC
sim_load (sd, prog, abfd, from_tty)
     SIM_DESC sd;
     char *prog;
     bfd *abfd;
     int from_tty ATTRIBUTE_UNUSED;
{
  bfd *prog_bfd;

  prog_bfd = sim_load_file (sd, myname, sim_callback, prog, abfd,
			    sim_kind == SIM_OPEN_DEBUG, 0, sim_write);
  if (prog_bfd == NULL)
    return SIM_RC_FAIL;
  ARMul_SetPC (state, bfd_get_start_address (prog_bfd));
  if (abfd == NULL)
    bfd_close (prog_bfd);
  return SIM_RC_OK;
}

void
sim_stop_reason (sd, reason, sigrc)
     SIM_DESC sd ATTRIBUTE_UNUSED;
     enum sim_stop *reason;
     int *sigrc;
{
  if (stop_simulator)
    {
      *reason = sim_stopped;
      *sigrc = TARGET_SIGNAL_INT;
    }
  else if (state->EndCondition == 0)
    {
      *reason = sim_exited;
      *sigrc = state->Reg[0] & 255;
    }
  else
    {
      *reason = sim_stopped;
      if (state->EndCondition == RDIError_BreakpointReached)
	*sigrc = TARGET_SIGNAL_TRAP;
      else if (   state->EndCondition == RDIError_DataAbort
	       || state->EndCondition == RDIError_AddressException)
	*sigrc = TARGET_SIGNAL_BUS;
      else
	*sigrc = 0;
    }
}

void
sim_do_command (sd, cmd)
     SIM_DESC sd ATTRIBUTE_UNUSED;
     char *cmd ATTRIBUTE_UNUSED;
{  
  (*sim_callback->printf_filtered)
    (sim_callback,
     "This simulator does not accept any commands.\n");
}

void
sim_set_callbacks (ptr)
     host_callback *ptr;
{
  sim_callback = ptr;
}
