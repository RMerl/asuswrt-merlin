/* Target-dependent code for AMD64 Solaris.

   Copyright (C) 2001, 2002, 2003, 2004, 2006, 2007
   Free Software Foundation, Inc.

   Contributed by Joseph Myers, CodeSourcery, LLC.

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
#include "gdbcore.h"
#include "regcache.h"
#include "osabi.h"
#include "symtab.h"

#include "gdb_string.h"

#include "sol2-tdep.h"
#include "amd64-tdep.h"
#include "solib-svr4.h"

/* Mapping between the general-purpose registers in gregset_t format
   and GDB's register cache layout.  */

/* From <sys/regset.h>.  */
static int amd64_sol2_gregset_reg_offset[] = {
  14 * 8,			/* %rax */
  11 * 8,			/* %rbx */
  13 * 8,			/* %rcx */
  12 * 8,			/* %rdx */
  9 * 8,			/* %rsi */
  8 * 8,			/* %rdi */
  10 * 8,			/* %rbp */
  20 * 8,			/* %rsp */
  7 * 8,			/* %r8 ... */
  6 * 8,
  5 * 8,
  4 * 8,
  3 * 8,
  2 * 8,
  1 * 8,
  0 * 8,			/* ... %r15 */
  17 * 8,			/* %rip */
  16 * 8,			/* %eflags */
  18 * 8,			/* %cs */
  21 * 8,			/* %ss */
  25 * 8,			/* %ds */
  24 * 8,			/* %es */
  22 * 8,			/* %fs */
  23 * 8			/* %gs */
};


/* Return whether the frame preceding NEXT_FRAME corresponds to a
   Solaris sigtramp routine.  */

static int
amd64_sol2_sigtramp_p (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  char *name;

  find_pc_partial_function (pc, &name, NULL, NULL);
  return (name && (strcmp ("sigacthandler", name) == 0
		   || strcmp (name, "ucbsigvechandler") == 0));
}

/* Solaris doesn't have a 'struct sigcontext', but it does have a
   'mcontext_t' that contains the saved set of machine registers.  */

static CORE_ADDR
amd64_sol2_mcontext_addr (struct frame_info *next_frame)
{
  CORE_ADDR sp, ucontext_addr;

  sp = frame_unwind_register_unsigned (next_frame, AMD64_RSP_REGNUM);
  ucontext_addr = get_frame_memory_unsigned (next_frame, sp + 8, 8);

  return ucontext_addr + 72;
}

static void
amd64_sol2_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->gregset_reg_offset = amd64_sol2_gregset_reg_offset;
  tdep->gregset_num_regs = ARRAY_SIZE (amd64_sol2_gregset_reg_offset);
  tdep->sizeof_gregset = 28 * 8;

  amd64_init_abi (info, gdbarch);

  tdep->sigtramp_p = amd64_sol2_sigtramp_p;
  tdep->sigcontext_addr = amd64_sol2_mcontext_addr;
  tdep->sc_reg_offset = tdep->gregset_reg_offset;
  tdep->sc_num_regs = tdep->gregset_num_regs;

  /* Solaris uses SVR4-style shared libraries.  */
  set_gdbarch_skip_solib_resolver (gdbarch, sol2_skip_solib_resolver);
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_lp64_fetch_link_map_offsets);
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
extern void _initialize_amd64_sol2_tdep (void);

void
_initialize_amd64_sol2_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_i386, bfd_mach_x86_64,
			  GDB_OSABI_SOLARIS, amd64_sol2_init_abi);
}
