/* Target-dependent code for GNU/Linux on Alpha.
   Copyright (C) 2002, 2003, 2007 Free Software Foundation, Inc.

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
#include "frame.h"
#include "gdb_assert.h"
#include "gdb_string.h"
#include "osabi.h"
#include "solib-svr4.h"
#include "symtab.h"
#include "regset.h"
#include "regcache.h"

#include "alpha-tdep.h"

/* Under GNU/Linux, signal handler invocations can be identified by
   the designated code sequence that is used to return from a signal
   handler.  In particular, the return address of a signal handler
   points to a sequence that copies $sp to $16, loads $0 with the
   appropriate syscall number, and finally enters the kernel.

   This is somewhat complicated in that:
     (1) the expansion of the "mov" assembler macro has changed over
         time, from "bis src,src,dst" to "bis zero,src,dst",
     (2) the kernel has changed from using "addq" to "lda" to load the
         syscall number,
     (3) there is a "normal" sigreturn and an "rt" sigreturn which
         has a different stack layout.
*/

static long
alpha_linux_sigtramp_offset_1 (CORE_ADDR pc)
{
  switch (alpha_read_insn (pc))
    {
    case 0x47de0410:		/* bis $30,$30,$16 */
    case 0x47fe0410:		/* bis $31,$30,$16 */
      return 0;

    case 0x43ecf400:		/* addq $31,103,$0 */
    case 0x201f0067:		/* lda $0,103($31) */
    case 0x201f015f:		/* lda $0,351($31) */
      return 4;

    case 0x00000083:		/* call_pal callsys */
      return 8;

    default:
      return -1;
    }
}

static LONGEST
alpha_linux_sigtramp_offset (CORE_ADDR pc)
{
  long i, off;

  if (pc & 3)
    return -1;

  /* Guess where we might be in the sequence.  */
  off = alpha_linux_sigtramp_offset_1 (pc);
  if (off < 0)
    return -1;

  /* Verify that the other two insns of the sequence are as we expect.  */
  pc -= off;
  for (i = 0; i < 12; i += 4)
    {
      if (i == off)
	continue;
      if (alpha_linux_sigtramp_offset_1 (pc + i) != i)
	return -1;
    }

  return off;
}

static int
alpha_linux_pc_in_sigtramp (CORE_ADDR pc, char *func_name)
{
  return alpha_linux_sigtramp_offset (pc) >= 0;
}

static CORE_ADDR
alpha_linux_sigcontext_addr (struct frame_info *next_frame)
{
  CORE_ADDR pc;
  ULONGEST sp;
  long off;

  pc = frame_pc_unwind (next_frame);
  frame_unwind_unsigned_register (next_frame, ALPHA_SP_REGNUM, &sp);

  off = alpha_linux_sigtramp_offset (pc);
  gdb_assert (off >= 0);

  /* __NR_rt_sigreturn has a couple of structures on the stack.  This is:

	struct rt_sigframe {
	  struct siginfo info;
	  struct ucontext uc;
        };

	offsetof (struct rt_sigframe, uc.uc_mcontext);
  */
  if (alpha_read_insn (pc - off + 4) == 0x201f015f)
    return sp + 176;

  /* __NR_sigreturn has the sigcontext structure at the top of the stack.  */
  return sp;
}

/* Supply register REGNUM from the buffer specified by GREGS and LEN
   in the general-purpose register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
alpha_linux_supply_gregset (const struct regset *regset,
			    struct regcache *regcache,
			    int regnum, const void *gregs, size_t len)
{
  const gdb_byte *regs = gregs;
  int i;
  gdb_assert (len >= 32 * 8);

  for (i = 0; i < ALPHA_ZERO_REGNUM; i++)
    {
      if (regnum == i || regnum == -1)
	regcache_raw_supply (regcache, i, regs + i * 8);
    }

  if (regnum == ALPHA_PC_REGNUM || regnum == -1)
    regcache_raw_supply (regcache, ALPHA_PC_REGNUM, regs + 31 * 8);

  if (regnum == ALPHA_UNIQUE_REGNUM || regnum == -1)
    regcache_raw_supply (regcache, ALPHA_UNIQUE_REGNUM,
			 len >= 33 * 8 ? regs + 32 * 8 : NULL);
}

/* Supply register REGNUM from the buffer specified by FPREGS and LEN
   in the floating-point register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
alpha_linux_supply_fpregset (const struct regset *regset,
			     struct regcache *regcache,
			     int regnum, const void *fpregs, size_t len)
{
  const gdb_byte *regs = fpregs;
  int i;
  gdb_assert (len >= 32 * 8);

  for (i = ALPHA_FP0_REGNUM; i < ALPHA_FP0_REGNUM + 31; i++)
    {
      if (regnum == i || regnum == -1)
	regcache_raw_supply (regcache, i, regs + (i - ALPHA_FP0_REGNUM) * 8);
    }

  if (regnum == ALPHA_FPCR_REGNUM || regnum == -1)
    regcache_raw_supply (regcache, ALPHA_FPCR_REGNUM, regs + 31 * 8);
}

static struct regset alpha_linux_gregset =
{
  NULL,
  alpha_linux_supply_gregset
};

static struct regset alpha_linux_fpregset =
{
  NULL,
  alpha_linux_supply_fpregset
};

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

const struct regset *
alpha_linux_regset_from_core_section (struct gdbarch *gdbarch,
				      const char *sect_name, size_t sect_size)
{
  if (strcmp (sect_name, ".reg") == 0 && sect_size >= 32 * 8)
    return &alpha_linux_gregset;

  if (strcmp (sect_name, ".reg2") == 0 && sect_size >= 32 * 8)
    return &alpha_linux_fpregset;

  return NULL;
}

static void
alpha_linux_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep;

  /* Hook into the DWARF CFI frame unwinder.  */
  alpha_dwarf2_init_abi (info, gdbarch);

  /* Hook into the MDEBUG frame unwinder.  */
  alpha_mdebug_init_abi (info, gdbarch);

  tdep = gdbarch_tdep (gdbarch);
  tdep->dynamic_sigtramp_offset = alpha_linux_sigtramp_offset;
  tdep->sigcontext_addr = alpha_linux_sigcontext_addr;
  tdep->pc_in_sigtramp = alpha_linux_pc_in_sigtramp;
  tdep->jb_pc = 2;
  tdep->jb_elt_size = 8;

  set_gdbarch_skip_trampoline_code (gdbarch, find_solib_trampoline_target);

  /* Enable TLS support.  */
  set_gdbarch_fetch_tls_load_module_address (gdbarch,
                                             svr4_fetch_objfile_link_map);

  set_gdbarch_regset_from_core_section
    (gdbarch, alpha_linux_regset_from_core_section);
}

void
_initialize_alpha_linux_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_alpha, 0, GDB_OSABI_LINUX,
                          alpha_linux_init_abi);
}
