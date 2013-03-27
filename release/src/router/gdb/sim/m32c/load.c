/* load.c --- loading object files into the M32C simulator.

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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bfd.h"

#include "cpu.h"
#include "mem.h"

int (*decode_opcode) () = 0;
int default_machine = 0;

void
m32c_set_mach (unsigned long mach)
{
  switch (mach)
    {
    case bfd_mach_m16c:
      m32c_set_cpu (CPU_M16C);
      if (verbose)
	fprintf (stderr, "[cpu: r8c/m16c]\n");
      break;
    case bfd_mach_m32c:
      m32c_set_cpu (CPU_M32C);
      if (verbose)
	fprintf (stderr, "[cpu: m32cm/m32c]\n");
      break;
    default:
      fprintf (stderr, "unknown m32c machine type 0x%lx\n", mach);
      decode_opcode = 0;
      break;
    }
}

void
m32c_load (bfd *prog)
{
  asection *s;
  unsigned long mach = bfd_get_mach (prog);
  unsigned long highest_addr_loaded = 0;

  if (mach == 0 && default_machine != 0)
    mach = default_machine;

  m32c_set_mach (mach);

  for (s = prog->sections; s; s = s->next)
    {
#if 0
      /* This was a good idea until we started storing the RAM data in
         ROM, at which point everything was all messed up.  The code
         remains as a reminder.  */
      if ((s->flags & SEC_ALLOC) && !(s->flags & SEC_READONLY))
	{
	  if (strcmp (bfd_get_section_name (prog, s), ".stack"))
	    {
	      int secend =
		bfd_get_section_size (s) + bfd_section_lma (prog, s);
	      if (heaptop < secend && bfd_section_lma (prog, s) < 0x10000)
		{
		  heaptop = heapbottom = secend;
		}
	    }
	}
#endif
      if (s->flags & SEC_LOAD)
	{
	  char *buf;
	  bfd_size_type size;

	  size = bfd_get_section_size (s);
	  if (size <= 0)
	    continue;

	  bfd_vma base = bfd_section_lma (prog, s);
	  if (verbose)
	    fprintf (stderr, "[load a=%08x s=%08x %s]\n",
		     (int) base, (int) size, bfd_get_section_name (prog, s));
	  buf = (char *) malloc (size);
	  bfd_get_section_contents (prog, s, buf, 0, size);
	  mem_put_blk (base, buf, size);
	  free (buf);
	  if (highest_addr_loaded < base + size - 1 && size >= 4)
	    highest_addr_loaded = base + size - 1;
	}
    }

  if (strcmp (bfd_get_target (prog), "srec") == 0)
    {
      heaptop = heapbottom = 0;
      switch (mach)
	{
	case bfd_mach_m16c:
	  if (highest_addr_loaded > 0x10000)
	    regs.r_pc = mem_get_si (0x000ffffc) & membus_mask;
	  else
	    regs.r_pc = mem_get_si (0x000fffc) & membus_mask;
	  break;
	case bfd_mach_m32c:
	  regs.r_pc = mem_get_si (0x00fffffc) & membus_mask;
	  break;
	}
    }
  else
    regs.r_pc = prog->start_address;
  if (verbose)
    fprintf (stderr, "[start pc=%08x]\n", (unsigned int) regs.r_pc);
}
