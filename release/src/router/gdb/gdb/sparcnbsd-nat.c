/* Native-dependent code for NetBSD/sparc.

   Copyright (C) 2002, 2003, 2004, 2007 Free Software Foundation, Inc.

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

#include "sparc-tdep.h"
#include "sparc-nat.h"

/* Support for debugging kernel virtual memory images.  */

#include <sys/types.h>
#include <machine/pcb.h>

#include "bsd-kvm.h"

static int
sparc32nbsd_supply_pcb (struct regcache *regcache, struct pcb *pcb)
{
  /* The following is true for NetBSD 1.6.2:

     The pcb contains %sp, %pc, %psr and %wim.  From this information
     we reconstruct the register state as it would look when we just
     returned from cpu_switch().  */

  /* The stack pointer shouldn't be zero.  */
  if (pcb->pcb_sp == 0)
    return 0;

  regcache_raw_supply (regcache, SPARC_SP_REGNUM, &pcb->pcb_sp);
  regcache_raw_supply (regcache, SPARC_O7_REGNUM, &pcb->pcb_pc);
  regcache_raw_supply (regcache, SPARC32_PSR_REGNUM, &pcb->pcb_psr);
  regcache_raw_supply (regcache, SPARC32_WIM_REGNUM, &pcb->pcb_wim);
  regcache_raw_supply (regcache, SPARC32_PC_REGNUM, &pcb->pcb_pc);

  sparc_supply_rwindow (regcache, pcb->pcb_sp, -1);

  return 1;
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_sparcnbsd_nat (void);

void
_initialize_sparcnbsd_nat (void)
{
  sparc_gregset = &sparc32nbsd_gregset;

  /* We've got nothing to add to the generic SPARC target.  */
  add_target (sparc_target ());

  /* Support debugging kernel virtual memory images.  */
  bsd_kvm_add_target (sparc32nbsd_supply_pcb);
}
