/* Target-dependent code for FreeBSD/amd64.

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
#include "arch-utils.h"
#include "frame.h"
#include "gdbcore.h"
#include "regcache.h"
#include "osabi.h"

#include "gdb_assert.h"
#include "gdb_string.h"

#include "amd64-tdep.h"
#include "bsd-uthread.h"
#include "solib-svr4.h"

/* Support for signal handlers.  */

/* Assuming NEXT_FRAME is for a frame following a BSD sigtramp
   routine, return the address of the associated sigcontext structure.  */

static CORE_ADDR
amd64fbsd_sigcontext_addr (struct frame_info *next_frame)
{
  CORE_ADDR sp;

  /* The `struct sigcontext' (which really is an `ucontext_t' on
     FreeBSD/amd64) lives at a fixed offset in the signal frame.  See
     <machine/sigframe.h>.  */
  sp = frame_unwind_register_unsigned (next_frame, AMD64_RSP_REGNUM);
  return sp + 16;
}

/* FreeBSD 5.1-RELEASE or later.  */

/* Mapping between the general-purpose registers in `struct reg'
   format and GDB's register cache layout.

   Note that some registers are 32-bit, but since we're little-endian
   we get away with that.  */

/* From <machine/reg.h>.  */
static int amd64fbsd_r_reg_offset[] =
{
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
  19 * 8,			/* %eflags */
  18 * 8,			/* %cs */
  21 * 8,			/* %ss */
  -1,				/* %ds */
  -1,				/* %es */
  -1,				/* %fs */
  -1				/* %gs */
};

/* Location of the signal trampoline.  */
CORE_ADDR amd64fbsd_sigtramp_start_addr = 0x7fffffffffc0;
CORE_ADDR amd64fbsd_sigtramp_end_addr = 0x7fffffffffe0;

/* From <machine/signal.h>.  */
int amd64fbsd_sc_reg_offset[] =
{
  24 + 6 * 8,			/* %rax */
  24 + 7 * 8,			/* %rbx */
  24 + 3 * 8,			/* %rcx */
  24 + 2 * 8,			/* %rdx */
  24 + 1 * 8,			/* %rsi */
  24 + 0 * 8,			/* %rdi */
  24 + 8 * 8,			/* %rbp */
  24 + 22 * 8,			/* %rsp */
  24 + 4 * 8,			/* %r8 ... */
  24 + 5 * 8,
  24 + 9 * 8,
  24 + 10 * 8,
  24 + 11 * 8,
  24 + 12 * 8,
  24 + 13 * 8,
  24 + 14 * 8,			/* ... %r15 */
  24 + 19 * 8,			/* %rip */
  24 + 21 * 8,			/* %eflags */
  24 + 20 * 8,			/* %cs */
  24 + 23 * 8,			/* %ss */
  -1,				/* %ds */
  -1,				/* %es */
  -1,				/* %fs */
  -1				/* %gs */
};

/* From /usr/src/lib/libc/amd64/gen/_setjmp.S.  */
static int amd64fbsd_jmp_buf_reg_offset[] =
{
  -1,				/* %rax */
  1 * 8,			/* %rbx */
  -1,				/* %rcx */
  -1,				/* %rdx */
  -1,				/* %rsi */
  -1,				/* %rdi */
  3 * 8,			/* %rbp */
  2 * 8,			/* %rsp */
  -1,				/* %r8 ... */
  -1,
  -1,
  -1,				/* ... %r11 */
  4 * 8,			/* %r12 ... */
  5 * 8,
  6 * 8,
  7 * 8,			/* ... %r15 */
  0 * 8				/* %rip */
};

static void
amd64fbsd_supply_uthread (struct regcache *regcache,
			  int regnum, CORE_ADDR addr)
{
  gdb_byte buf[8];
  int i;

  gdb_assert (regnum >= -1);

  for (i = 0; i < ARRAY_SIZE (amd64fbsd_jmp_buf_reg_offset); i++)
    {
      if (amd64fbsd_jmp_buf_reg_offset[i] != -1
	  && (regnum == -1 || regnum == i))
	{
	  read_memory (addr + amd64fbsd_jmp_buf_reg_offset[i], buf, 8);
	  regcache_raw_supply (regcache, i, buf);
	}
    }
}

static void
amd64fbsd_collect_uthread (const struct regcache *regcache,
			   int regnum, CORE_ADDR addr)
{
  gdb_byte buf[8];
  int i;

  gdb_assert (regnum >= -1);

  for (i = 0; i < ARRAY_SIZE (amd64fbsd_jmp_buf_reg_offset); i++)
    {
      if (amd64fbsd_jmp_buf_reg_offset[i] != -1
	  && (regnum == -1 || regnum == i))
	{
	  regcache_raw_collect (regcache, i, buf);
	  write_memory (addr + amd64fbsd_jmp_buf_reg_offset[i], buf, 8);
	}
    }
}

void
amd64fbsd_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* Obviously FreeBSD is BSD-based.  */
  i386bsd_init_abi (info, gdbarch);

  tdep->gregset_reg_offset = amd64fbsd_r_reg_offset;
  tdep->gregset_num_regs = ARRAY_SIZE (amd64fbsd_r_reg_offset);
  tdep->sizeof_gregset = 22 * 8;

  amd64_init_abi (info, gdbarch);

  tdep->sigtramp_start = amd64fbsd_sigtramp_start_addr;
  tdep->sigtramp_end = amd64fbsd_sigtramp_end_addr;
  tdep->sigcontext_addr = amd64fbsd_sigcontext_addr;
  tdep->sc_reg_offset = amd64fbsd_sc_reg_offset;
  tdep->sc_num_regs = ARRAY_SIZE (amd64fbsd_sc_reg_offset);

  /* FreeBSD provides a user-level threads implementation.  */
  bsd_uthread_set_supply_uthread (gdbarch, amd64fbsd_supply_uthread);
  bsd_uthread_set_collect_uthread (gdbarch, amd64fbsd_collect_uthread);

  /* FreeBSD uses SVR4-style shared libraries.  */
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_lp64_fetch_link_map_offsets);
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_amd64fbsd_tdep (void);

void
_initialize_amd64fbsd_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_i386, bfd_mach_x86_64,
			  GDB_OSABI_FREEBSD_ELF, amd64fbsd_init_abi);
}
