/* main.c --- main function for stand-alone M32C simulator.

Copyright (C) 2005, 2007 Free Software Foundation, Inc.
Contributed by Red Hat, Inc.

This file is part of the GNU simulators.

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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <setjmp.h>
#include <signal.h>

#include "bfd.h"

#include "cpu.h"
#include "mem.h"
#include "misc.h"
#include "load.h"
#include "trace.h"

static int disassemble = 0;
static unsigned int cycles = 0;

static void
done (int exit_code)
{
  if (verbose)
    {
      stack_heap_stats ();
      mem_usage_stats ();
      printf ("insns: %14s\n", comma (cycles));
    }
  exit (exit_code);
}

int
main (int argc, char **argv)
{
  int o;
  int save_trace;
  bfd *prog;

  while ((o = getopt (argc, argv, "tvdm:")) != -1)
    switch (o)
      {
      case 't':
	trace++;
	break;
      case 'v':
	verbose++;
	break;
      case 'd':
	disassemble++;
	break;
      case 'm':
	if (strcmp (optarg, "r8c") == 0 || strcmp (optarg, "m16c") == 0)
	  default_machine = bfd_mach_m16c;
	else if (strcmp (optarg, "m32cm") == 0
		 || strcmp (optarg, "m32c") == 0)
	  default_machine = bfd_mach_m32c;
	else
	  {
	    fprintf (stderr, "Invalid machine: %s\n", optarg);
	    exit (1);
	  }
	break;
      case '?':
	fprintf (stderr,
                 "usage: run [-v] [-t] [-d] [-m r8c|m16c|m32cm|m32c]"
                 " program\n");
	exit (1);
      }

  prog = bfd_openr (argv[optind], 0);
  if (!prog)
    {
      fprintf (stderr, "Can't read %s\n", argv[optind]);
      exit (1);
    }

  if (!bfd_check_format (prog, bfd_object))
    {
      fprintf (stderr, "%s not a m32c program\n", argv[optind]);
      exit (1);
    }

  save_trace = trace;
  trace = 0;
  m32c_load (prog);
  trace = save_trace;

  if (disassemble)
    sim_disasm_init (prog);

  while (1)
    {
      int rc;

      if (trace)
	printf ("\n");

      if (disassemble)
	sim_disasm_one ();

      enable_counting = verbose;
      cycles++;
      rc = decode_opcode ();
      enable_counting = 0;

      if (M32C_HIT_BREAK (rc))
	done (1);
      else if (M32C_EXITED (rc))
	done (M32C_EXIT_STATUS (rc));
      else
	assert (M32C_STEPPED (rc));

      trace_register_changes ();
    }
}
