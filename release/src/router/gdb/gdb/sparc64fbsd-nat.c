/* Native-dependent code for FreeBSD/sparc64.

   Copyright (C) 2003, 2004, 2005, 2007 Free Software Foundation, Inc.

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

#include "defs.h"
#include "regcache.h"
#include "target.h"

#include "fbsd-nat.h"
#include "sparc64-tdep.h"
#include "sparc-nat.h"


/* Support for debugging kernel virtual memory images.  */

#include <sys/types.h>
#include <machine/pcb.h>

#include "bsd-kvm.h"

static int
sparc64fbsd_kvm_supply_pcb (struct regcache *regcache, struct pcb *pcb)
{
  /* The following is true for FreeBSD 5.4:

     The pcb contains %sp and %pc.  Since the register windows are
     explicitly flushed, we can find the `local' and `in' registers on
     the stack.  */

  /* The stack pointer shouldn't be zero.  */
  if (pcb->pcb_sp == 0)
    return 0;

  regcache_raw_supply (regcache, SPARC_SP_REGNUM, &pcb->pcb_sp);
  regcache_raw_supply (regcache, SPARC64_PC_REGNUM, &pcb->pcb_pc);

  /* Synthesize %npc.  */
  pcb->pcb_pc += 4;
  regcache_raw_supply (regcache, SPARC64_NPC_REGNUM, &pcb->pcb_pc);

  /* Read `local' and `in' registers from the stack.  */
  sparc_supply_rwindow (regcache, pcb->pcb_sp, -1);

  return 1;
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_sparc64fbsd_nat (void);

void
_initialize_sparc64fbsd_nat (void)
{
  struct target_ops *t;

  /* Add some extra features to the generic SPARC target.  */
  t = sparc_target ();
  t->to_pid_to_exec_file = fbsd_pid_to_exec_file;
  t->to_find_memory_regions = fbsd_find_memory_regions;
  t->to_make_corefile_notes = fbsd_make_corefile_notes;
  add_target (t);

  sparc_gregset = &sparc64fbsd_gregset;

  /* Support debugging kernel virtual memory images.  */
  bsd_kvm_add_target (sparc64fbsd_kvm_supply_pcb);
}
