/* The common simulator framework for GDB, the GNU Debugger.

   Copyright 2002, 2007 Free Software Foundation, Inc.

   Contributed by Andrew Cagney and Red Hat.

   This file is part of GDB.

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
#include "sim-assert.h"
#include "bfd.h"


int current_host_byte_order;
int current_target_byte_order;
int current_stdio;

enum sim_alignments current_alignment;

#if defined (WITH_FLOATING_POINT)
int current_floating_point;
#endif



/* map a byte order onto a textual string */

static const char *
config_byte_order_to_a (int byte_order)
{
  switch (byte_order)
    {
    case LITTLE_ENDIAN:
      return "LITTLE_ENDIAN";
    case BIG_ENDIAN:
      return "BIG_ENDIAN";
    case 0:
      return "0";
    }
  return "UNKNOWN";
}


static const char *
config_stdio_to_a (int stdio)
{
  switch (stdio)
    {
    case DONT_USE_STDIO:
      return "DONT_USE_STDIO";
    case DO_USE_STDIO:
      return "DO_USE_STDIO";
    case 0:
      return "0";
    }
  return "UNKNOWN";
}


static const char *
config_environment_to_a (enum sim_environment environment)
{
  switch (environment)
    {
    case ALL_ENVIRONMENT:
      return "ALL_ENVIRONMENT";
    case USER_ENVIRONMENT:
      return "USER_ENVIRONMENT";
    case VIRTUAL_ENVIRONMENT:
      return "VIRTUAL_ENVIRONMENT";
    case OPERATING_ENVIRONMENT:
      return "OPERATING_ENVIRONMENT";
    }
  return "UNKNOWN";
}


static const char *
config_alignment_to_a (enum sim_alignments alignment)
{
  switch (alignment)
    {
    case MIXED_ALIGNMENT:
      return "MIXED_ALIGNMENT";
    case NONSTRICT_ALIGNMENT:
      return "NONSTRICT_ALIGNMENT";
    case STRICT_ALIGNMENT:
      return "STRICT_ALIGNMENT";
    case FORCED_ALIGNMENT:
      return "FORCED_ALIGNMENT";
    }
  return "UNKNOWN";
}


#if defined (WITH_FLOATING_POINT)
static const char *
config_floating_point_to_a (int floating_point)
{
  switch (floating_point)
    {
    case SOFT_FLOATING_POINT:
      return "SOFT_FLOATING_POINT";
    case HARD_FLOATING_POINT:
      return "HARD_FLOATING_POINT";
    case 0:
      return "0";
    }
  return "UNKNOWN";
}
#endif

/* Set the default environment, prior to parsing argv.  */

void
sim_config_default (SIM_DESC sd)
{
   /* Set the current environment to ALL_ENVIRONMENT to indicate none has been
      selected yet.  This is so that after parsing argv, we know whether the
      environment was explicitly specified or not.  */
  STATE_ENVIRONMENT (sd) = ALL_ENVIRONMENT;
}

/* Complete and verify the simulation environment.  */

SIM_RC
sim_config (SIM_DESC sd)
{
  int prefered_target_byte_order;
  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);

  /* extract all relevant information */
  if (STATE_PROG_BFD (sd) == NULL
      /* If we have a binary input file (presumably with specified
	 "--architecture"), it'll have no endianness.  */
      || (!bfd_little_endian (STATE_PROG_BFD (sd))
	  && !bfd_big_endian (STATE_PROG_BFD (sd))))
    prefered_target_byte_order = 0;
  else
    prefered_target_byte_order = (bfd_little_endian(STATE_PROG_BFD (sd))
				  ? LITTLE_ENDIAN
				  : BIG_ENDIAN);

  /* set the host byte order */
  current_host_byte_order = 1;
  if (*(char*)(&current_host_byte_order))
    current_host_byte_order = LITTLE_ENDIAN;
  else
    current_host_byte_order = BIG_ENDIAN;

  /* verify the host byte order */
  if (CURRENT_HOST_BYTE_ORDER != current_host_byte_order)
    {
      sim_io_eprintf (sd, "host (%s) and configured (%s) byte order in conflict",
		      config_byte_order_to_a (current_host_byte_order),
		      config_byte_order_to_a (CURRENT_HOST_BYTE_ORDER));
      return SIM_RC_FAIL;
    }


  /* set the target byte order */
#if (WITH_TREE_PROPERTIES)
  if (current_target_byte_order == 0)
    current_target_byte_order
      = (tree_find_boolean_property (root, "/options/little-endian?")
	 ? LITTLE_ENDIAN
	 : BIG_ENDIAN);
#endif
  if (current_target_byte_order == 0
      && prefered_target_byte_order != 0)
    current_target_byte_order = prefered_target_byte_order;
  if (current_target_byte_order == 0)
    current_target_byte_order = WITH_TARGET_BYTE_ORDER;
  if (current_target_byte_order == 0)
    current_target_byte_order = WITH_DEFAULT_TARGET_BYTE_ORDER;

  /* verify the target byte order */
  if (CURRENT_TARGET_BYTE_ORDER == 0)
    {
      sim_io_eprintf (sd, "Target byte order unspecified\n");
      return SIM_RC_FAIL;
    }
  if (CURRENT_TARGET_BYTE_ORDER != current_target_byte_order)
    sim_io_eprintf (sd, "Target (%s) and configured (%s) byte order in conflict\n",
		  config_byte_order_to_a (current_target_byte_order),
		  config_byte_order_to_a (CURRENT_TARGET_BYTE_ORDER));
  if (prefered_target_byte_order != 0
      && CURRENT_TARGET_BYTE_ORDER != prefered_target_byte_order)
    sim_io_eprintf (sd, "Target (%s) and specified (%s) byte order in conflict\n",
		  config_byte_order_to_a (CURRENT_TARGET_BYTE_ORDER),
		  config_byte_order_to_a (prefered_target_byte_order));


  /* set the stdio */
  if (current_stdio == 0)
    current_stdio = WITH_STDIO;
  if (current_stdio == 0)
    current_stdio = DO_USE_STDIO;

  /* verify the stdio */
  if (CURRENT_STDIO == 0)
    {
      sim_io_eprintf (sd, "Target standard IO unspecified\n");
      return SIM_RC_FAIL;
    }
  if (CURRENT_STDIO != current_stdio)
    {
      sim_io_eprintf (sd, "Target (%s) and configured (%s) standard IO in conflict\n",
		      config_stdio_to_a (CURRENT_STDIO),
		      config_stdio_to_a (current_stdio));
      return SIM_RC_FAIL;
    }
  
  
  /* check the value of MSB */
  if (WITH_TARGET_WORD_MSB != 0
      && WITH_TARGET_WORD_MSB != (WITH_TARGET_WORD_BITSIZE - 1))
    {
      sim_io_eprintf (sd, "Target bitsize (%d) contradicts target most significant bit (%d)\n",
		      WITH_TARGET_WORD_BITSIZE, WITH_TARGET_WORD_MSB);
      return SIM_RC_FAIL;
    }
  
  
  /* set the environment */
#if (WITH_TREE_PROPERTIES)
  if (STATE_ENVIRONMENT (sd) == ALL_ENVIRONMENT)
    {
      const char *env =
	tree_find_string_property(root, "/openprom/options/env");
      STATE_ENVIRONMENT (sd) = ((strcmp(env, "user") == 0
				 || strcmp(env, "uea") == 0)
				? USER_ENVIRONMENT
				: (strcmp(env, "virtual") == 0
				   || strcmp(env, "vea") == 0)
				? VIRTUAL_ENVIRONMENT
				: (strcmp(env, "operating") == 0
				   || strcmp(env, "oea") == 0)
				? OPERATING_ENVIRONMENT
				: ALL_ENVIRONMENT);
    }
#endif
  if (STATE_ENVIRONMENT (sd) == ALL_ENVIRONMENT)
    STATE_ENVIRONMENT (sd) = DEFAULT_ENVIRONMENT;
  
  
  /* set the alignment */
#if (WITH_TREE_PROPERTIES)
  if (current_alignment == 0)
    current_alignment =
      (tree_find_boolean_property(root, "/openprom/options/strict-alignment?")
       ? STRICT_ALIGNMENT
       : NONSTRICT_ALIGNMENT);
#endif
  if (current_alignment == 0)
    current_alignment = WITH_ALIGNMENT;
  if (current_alignment == 0)
    current_alignment = WITH_DEFAULT_ALIGNMENT;
  
  /* verify the alignment */
  if (CURRENT_ALIGNMENT == 0)
    {
      sim_io_eprintf (sd, "Target alignment unspecified\n");
      return SIM_RC_FAIL;
    }
  if (CURRENT_ALIGNMENT != current_alignment)
    {
      sim_io_eprintf (sd, "Target (%s) and configured (%s) alignment in conflict\n",
		      config_alignment_to_a (CURRENT_ALIGNMENT),
		      config_alignment_to_a (current_alignment));
      return SIM_RC_FAIL;
    }
  
#if defined (WITH_FLOATING_POINT)
  
  /* set the floating point */
  if (current_floating_point == 0)
    current_floating_point = WITH_FLOATING_POINT;
  
  /* verify the floating point */
  if (CURRENT_FLOATING_POINT == 0)
    {
      sim_io_eprintf (sd, "Target floating-point unspecified\n");
      return SIM_RC_FAIL;
    }
  if (CURRENT_FLOATING_POINT != current_floating_point)
    {
      sim_io_eprintf (sd, "Target (%s) and configured (%s) floating-point in conflict\n",
		      config_alignment_to_a (CURRENT_FLOATING_POINT),
		      config_alignment_to_a (current_floating_point));
      return SIM_RC_FAIL;
    }
  
#endif
  return SIM_RC_OK;
}


void
print_sim_config (SIM_DESC sd)
{
#if defined (__GNUC__) && defined (__VERSION__)
  sim_io_printf (sd, "Compiled by GCC %s on %s %s\n",
			  __VERSION__, __DATE__, __TIME__);
#else
  sim_io_printf (sd, "Compiled on %s %s\n", __DATE__, __TIME__);
#endif

  sim_io_printf (sd, "WITH_TARGET_BYTE_ORDER   = %s\n",
		 config_byte_order_to_a (WITH_TARGET_BYTE_ORDER));

  sim_io_printf (sd, "WITH_DEFAULT_TARGET_BYTE_ORDER   = %s\n",
		 config_byte_order_to_a (WITH_DEFAULT_TARGET_BYTE_ORDER));

  sim_io_printf (sd, "WITH_HOST_BYTE_ORDER     = %s\n",
		 config_byte_order_to_a (WITH_HOST_BYTE_ORDER));

  sim_io_printf (sd, "WITH_STDIO               = %s\n",
		 config_stdio_to_a (WITH_STDIO));

  sim_io_printf (sd, "WITH_TARGET_WORD_MSB     = %d\n",
		 WITH_TARGET_WORD_MSB);

  sim_io_printf (sd, "WITH_TARGET_WORD_BITSIZE = %d\n",
		 WITH_TARGET_WORD_BITSIZE);

  sim_io_printf (sd, "WITH_TARGET_ADDRESS_BITSIZE = %d\n",
		 WITH_TARGET_ADDRESS_BITSIZE);

  sim_io_printf (sd, "WITH_TARGET_CELL_BITSIZE = %d\n",
		 WITH_TARGET_CELL_BITSIZE);

  sim_io_printf (sd, "WITH_TARGET_FLOATING_POINT_BITSIZE = %d\n",
		 WITH_TARGET_FLOATING_POINT_BITSIZE);

  sim_io_printf (sd, "WITH_ENVIRONMENT = %s\n",
		 config_environment_to_a (WITH_ENVIRONMENT));

  sim_io_printf (sd, "WITH_ALIGNMENT = %s\n",
		 config_alignment_to_a (WITH_ALIGNMENT));

#if defined (WITH_DEFAULT_ALIGNMENT)
  sim_io_printf (sd, "WITH_DEFAULT_ALIGNMENT = %s\n",
		 config_alignment_to_a (WITH_DEFAULT_ALIGNMENT));
#endif

#if defined (WITH_XOR_ENDIAN)
  sim_io_printf (sd, "WITH_XOR_ENDIAN = %d\n", WITH_XOR_ENDIAN);
#endif

#if defined (WITH_FLOATING_POINT)
  sim_io_printf (sd, "WITH_FLOATING_POINT = %s\n",
		 config_floating_point_to_a (WITH_FLOATING_POINT));
#endif

#if defined (WITH_SMP)
  sim_io_printf (sd, "WITH_SMP = %d\n", WITH_SMP);
#endif

#if defined (WITH_RESERVED_BITS)
  sim_io_printf (sd, "WITH_RESERVED_BITS = %d\n", WITH_RESERVED_BITS);
#endif
		 
#if defined (WITH_PROFILE)
  sim_io_printf (sd, "WITH_PROFILE = %d\n", WITH_PROFILE);
#endif
		 
}
