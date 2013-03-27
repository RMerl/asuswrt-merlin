/* Target-dependent code for GNU/Linux SPARC.

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
#include "dwarf2-frame.h"
#include "frame.h"
#include "frame-unwind.h"
#include "gdbtypes.h"
#include "regset.h"
#include "gdbarch.h"
#include "gdbcore.h"
#include "osabi.h"
#include "regcache.h"
#include "solib-svr4.h"
#include "symtab.h"
#include "trad-frame.h"
#include "tramp-frame.h"

#include "sparc-tdep.h"

/* Signal trampoline support.  */

static void sparc32_linux_sigframe_init (const struct tramp_frame *self,
					 struct frame_info *next_frame,
					 struct trad_frame_cache *this_cache,
					 CORE_ADDR func);

/* GNU/Linux has two flavors of signals.  Normal signal handlers, and
   "realtime" (RT) signals.  The RT signals can provide additional
   information to the signal handler if the SA_SIGINFO flag is set
   when establishing a signal handler using `sigaction'.  It is not
   unlikely that future versions of GNU/Linux will support SA_SIGINFO
   for normal signals too.  */

/* When the sparc Linux kernel calls a signal handler and the
   SA_RESTORER flag isn't set, the return address points to a bit of
   code on the stack.  This code checks whether the PC appears to be
   within this bit of code.

   The instruction sequence for normal signals is encoded below.
   Checking for the code sequence should be somewhat reliable, because
   the effect is to call the system call sigreturn.  This is unlikely
   to occur anywhere other than a signal trampoline.  */

static const struct tramp_frame sparc32_linux_sigframe =
{
  SIGTRAMP_FRAME,
  4,
  {
    { 0x821020d8, -1 },		/* mov __NR_sugreturn, %g1 */
    { 0x91d02010, -1 },		/* ta  0x10 */
    { TRAMP_SENTINEL_INSN, -1 }
  },
  sparc32_linux_sigframe_init
};

/* The instruction sequence for RT signals is slightly different.  The
   effect is to call the system call rt_sigreturn.  */

static const struct tramp_frame sparc32_linux_rt_sigframe =
{
  SIGTRAMP_FRAME,
  4,
  {
    { 0x82102065, -1 },		/* mov __NR_rt_sigreturn, %g1 */
    { 0x91d02010, -1 },		/* ta  0x10 */
    { TRAMP_SENTINEL_INSN, -1 }
  },
  sparc32_linux_sigframe_init
};

static void
sparc32_linux_sigframe_init (const struct tramp_frame *self,
			     struct frame_info *next_frame,
			     struct trad_frame_cache *this_cache,
			     CORE_ADDR func)
{
  CORE_ADDR base, addr, sp_addr;
  int regnum;

  base = frame_unwind_register_unsigned (next_frame, SPARC_O1_REGNUM);
  if (self == &sparc32_linux_rt_sigframe)
    base += 128;

  /* Offsets from <bits/sigcontext.h>.  */

  trad_frame_set_reg_addr (this_cache, SPARC32_PSR_REGNUM, base + 0);
  trad_frame_set_reg_addr (this_cache, SPARC32_PC_REGNUM, base + 4);
  trad_frame_set_reg_addr (this_cache, SPARC32_NPC_REGNUM, base + 8);
  trad_frame_set_reg_addr (this_cache, SPARC32_Y_REGNUM, base + 12);

  /* Since %g0 is always zero, keep the identity encoding.  */
  addr = base + 20;
  sp_addr = base + 16 + ((SPARC_SP_REGNUM - SPARC_G0_REGNUM) * 4);
  for (regnum = SPARC_G1_REGNUM; regnum <= SPARC_O7_REGNUM; regnum++)
    {
      trad_frame_set_reg_addr (this_cache, regnum, addr);
      addr += 4;
    }

  base = frame_unwind_register_unsigned (next_frame, SPARC_SP_REGNUM);
  addr = get_frame_memory_unsigned (next_frame, sp_addr, 4);

  for (regnum = SPARC_L0_REGNUM; regnum <= SPARC_I7_REGNUM; regnum++)
    {
      trad_frame_set_reg_addr (this_cache, regnum, addr);
      addr += 4;
    }
  trad_frame_set_id (this_cache, frame_id_build (base, func));
}

/* Return the address of a system call's alternative return
   address.  */

static CORE_ADDR
sparc32_linux_step_trap (struct frame_info *frame, unsigned long insn)
{
  if (insn == 0x91d02010)
    {
      ULONGEST sc_num = get_frame_register_unsigned (frame, SPARC_G1_REGNUM);

      /* __NR_rt_sigreturn is 101 and __NR_sigreturn is 216  */
      if (sc_num == 101 || sc_num == 216)
	{
	  ULONGEST sp, pc_offset;

	  sp = get_frame_register_unsigned (frame, SPARC_SP_REGNUM);

	  /* The kernel puts the sigreturn registers on the stack,
	     and this is where the signal unwinding state is take from
	     when returning from a signal.

	     For __NR_sigreturn, this register area sits 96 bytes from
	     the base of the stack.  The saved PC sits 4 bytes into the
	     sigreturn register save area.

	     For __NR_rt_sigreturn a siginfo_t, which is 128 bytes, sits
	     right before the sigreturn register save area.  */

	  pc_offset = 96 + 4;
	  if (sc_num == 101)
	    pc_offset += 128;

	  return read_memory_unsigned_integer (sp + pc_offset, 4);
	}
    }

  return 0;
}


const struct sparc_gregset sparc32_linux_core_gregset =
{
  32 * 4,			/* %psr */
  33 * 4,			/* %pc */
  34 * 4,			/* %npc */
  35 * 4,			/* %y */
  -1,				/* %wim */
  -1,				/* %tbr */
  1 * 4,			/* %g1 */
  16 * 4,			/* %l0 */
  4,				/* y size */
};


static void
sparc32_linux_supply_core_gregset (const struct regset *regset,
				   struct regcache *regcache,
				   int regnum, const void *gregs, size_t len)
{
  sparc32_supply_gregset (&sparc32_linux_core_gregset, regcache, regnum, gregs);
}

static void
sparc32_linux_collect_core_gregset (const struct regset *regset,
				    const struct regcache *regcache,
				    int regnum, void *gregs, size_t len)
{
  sparc32_collect_gregset (&sparc32_linux_core_gregset, regcache, regnum, gregs);
}

static void
sparc32_linux_supply_core_fpregset (const struct regset *regset,
				    struct regcache *regcache,
				    int regnum, const void *fpregs, size_t len)
{
  sparc32_supply_fpregset (regcache, regnum, fpregs);
}

static void
sparc32_linux_collect_core_fpregset (const struct regset *regset,
				     const struct regcache *regcache,
				     int regnum, void *fpregs, size_t len)
{
  sparc32_collect_fpregset (regcache, regnum, fpregs);
}



static void
sparc32_linux_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->gregset = regset_alloc (gdbarch, sparc32_linux_supply_core_gregset,
				sparc32_linux_collect_core_gregset);
  tdep->sizeof_gregset = 152;

  tdep->fpregset = regset_alloc (gdbarch, sparc32_linux_supply_core_fpregset,
				 sparc32_linux_collect_core_fpregset);
  tdep->sizeof_fpregset = 396;

  tramp_frame_prepend_unwinder (gdbarch, &sparc32_linux_sigframe);
  tramp_frame_prepend_unwinder (gdbarch, &sparc32_linux_rt_sigframe);

  /* GNU/Linux has SVR4-style shared libraries...  */
  set_gdbarch_skip_trampoline_code (gdbarch, find_solib_trampoline_target);
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);

  /* ...which means that we need some special handling when doing
     prologue analysis.  */
  tdep->plt_entry_size = 12;

  /* GNU/Linux doesn't support the 128-bit `long double' from the psABI.  */
  set_gdbarch_long_double_bit (gdbarch, 64);
  set_gdbarch_long_double_format (gdbarch, floatformats_ieee_double);

  /* Enable TLS support.  */
  set_gdbarch_fetch_tls_load_module_address (gdbarch,
                                             svr4_fetch_objfile_link_map);

  /* Make sure we can single-step over signal return system calls.  */
  tdep->step_trap = sparc32_linux_step_trap;

  /* Hook in the DWARF CFI frame unwinder.  */
  frame_unwind_append_sniffer (gdbarch, dwarf2_frame_sniffer);
}

/* Provide a prototype to silence -Wmissing-prototypes.  */
extern void _initialize_sparc_linux_tdep (void);

void
_initialize_sparc_linux_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_sparc, 0, GDB_OSABI_LINUX,
			  sparc32_linux_init_abi);
}
