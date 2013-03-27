/* Native-dependent code for OpenBSD/i386.

   Copyright (C) 2002, 2003, 2004, 2006, 2007 Free Software Foundation, Inc.

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
#include "gdbcore.h"
#include "regcache.h"
#include "target.h"

#include <sys/param.h>
#include <sys/sysctl.h>
#include <machine/frame.h>
#include <machine/pcb.h>

#include "i386-tdep.h"
#include "i386bsd-nat.h"
#include "bsd-kvm.h"

static int
i386obsd_supply_pcb (struct regcache *regcache, struct pcb *pcb)
{
  struct switchframe sf;

  /* The following is true for OpenBSD 3.6:

     The pcb contains %esp and %ebp at the point of the context switch
     in cpu_switch().  At that point we have a stack frame as
     described by `struct switchframe', which for OpenBSD 3.6 has the
     following layout:

     interrupt level
     %edi
     %esi
     %ebx
     %eip

     we reconstruct the register state as it would look when we just
     returned from cpu_switch().  */

  /* The stack pointer shouldn't be zero.  */
  if (pcb->pcb_esp == 0)
    return 0;

  /* Read the stack frame, and check its validity.  We do this by
     checking if the saved interrupt priority level in the stack frame
     looks reasonable..  */
  read_memory (pcb->pcb_esp, (char *) &sf, sizeof sf);
  if ((unsigned int) sf.sf_ppl < 0x100 && (sf.sf_ppl & 0xf) == 0)
    {
      /* Yes, we have a frame that matches cpu_switch().  */
      pcb->pcb_esp += sizeof (struct switchframe);
      regcache_raw_supply (regcache, I386_EDI_REGNUM, &sf.sf_edi);
      regcache_raw_supply (regcache, I386_ESI_REGNUM, &sf.sf_esi);
      regcache_raw_supply (regcache, I386_EBX_REGNUM, &sf.sf_ebx);
      regcache_raw_supply (regcache, I386_EIP_REGNUM, &sf.sf_eip);
    }
  else
    {
      /* No, the pcb must have been last updated by savectx().  */
      pcb->pcb_esp += 4;
      regcache_raw_supply (regcache, I386_EIP_REGNUM, &sf);
    }

  regcache_raw_supply (regcache, I386_EBP_REGNUM, &pcb->pcb_ebp);
  regcache_raw_supply (regcache, I386_ESP_REGNUM, &pcb->pcb_esp);

  return 1;
}


/* Prevent warning from -Wmissing-prototypes.  */
void _initialize_i386obsd_nat (void);

void
_initialize_i386obsd_nat (void)
{
  /* We've got nothing to add to the common *BSD/i386 target.  */
  add_target (i386bsd_target ());

  /* Support debugging kernel virtual memory images.  */
  bsd_kvm_add_target (i386obsd_supply_pcb);

  /* OpenBSD provides a vm.psstrings sysctl that we can use to locate
     the sigtramp.  That way we can still recognize a sigtramp if its
     location is changed in a new kernel.  This is especially
     important for OpenBSD, since it uses a different memory layout
     than NetBSD, yet we cannot distinguish between the two.

     Of course this is still based on the assumption that the sigtramp
     is placed directly under the location where the program arguments
     and environment can be found.  */
#ifdef VM_PSSTRINGS
  {
    struct _ps_strings _ps;
    int mib[2];
    size_t len;

    mib[0] = CTL_VM;
    mib[1] = VM_PSSTRINGS;
    len = sizeof (_ps);
    if (sysctl (mib, 2, &_ps, &len, NULL, 0) == 0)
      {
	i386obsd_sigtramp_start_addr = (u_long) _ps.val - 128;
	i386obsd_sigtramp_end_addr = (u_long) _ps.val;
      }
  }
#endif
}
