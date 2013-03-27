/* Target-dependent code for OpenBSD/vax.

   Copyright (C) 2005, 2007 Free Software Foundation, Inc.

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
#include "frame-unwind.h"
#include "osabi.h"
#include "symtab.h"
#include "trad-frame.h"

#include "vax-tdep.h"

#include "gdb_string.h"

/* Signal trampolines.  */

/* Since OpenBSD 3.2, the sigtramp routine is mapped at a random page
   in virtual memory.  The randomness makes it somewhat tricky to
   detect it, but fortunately we can rely on the fact that the start
   of the sigtramp routine is page-aligned.  We recognize the
   trampoline by looking for the code that invokes the sigreturn
   system call.  The offset where we can find that code varies from
   release to release.

   By the way, the mapping mentioned above is read-only, so you cannot
   place a breakpoint in the signal trampoline.  */

/* Default page size.  */
static const int vaxobsd_page_size = 4096;

/* Offset for sigreturn(2).  */
static const int vaxobsd_sigreturn_offset = 0x11;

/* Instruction sequence for sigreturn(2).  VAX doesn't have
   fixed-length instructions so we include the ensuing exit(2) to
   reduce the chance of spurious matches.  */
static const gdb_byte vaxobsd_sigreturn[] = {
  0xbc, 0x8f, 0x67, 0x00,	/* chmk $SYS_sigreturn */
  0xbc, 0x01			/* chmk $SYS_exit */
};

static int
vaxobsd_sigtramp_p (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  CORE_ADDR start_pc = (pc & ~(vaxobsd_page_size - 1));
  CORE_ADDR sigreturn_addr = start_pc + vaxobsd_sigreturn_offset;
  gdb_byte *buf;
  char *name;

  find_pc_partial_function (pc, &name, NULL, NULL);
  if (name)
    return 0;

  buf = alloca(sizeof vaxobsd_sigreturn);
  if (!safe_frame_unwind_memory (next_frame, sigreturn_addr,
				 buf, sizeof vaxobsd_sigreturn))
    return 0;

  if (memcmp(buf, vaxobsd_sigreturn, sizeof vaxobsd_sigreturn) == 0)
    return 1;

  return 0;
}

static struct trad_frame_cache *
vaxobsd_sigtramp_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct trad_frame_cache *cache;
  CORE_ADDR addr, base, func;

  if (*this_cache)
    return *this_cache;

  cache = trad_frame_cache_zalloc (next_frame);
  *this_cache = cache;

  func = frame_pc_unwind (next_frame);
  func &= ~(vaxobsd_page_size - 1);

  base = frame_unwind_register_unsigned (next_frame, VAX_SP_REGNUM);
  addr = get_frame_memory_unsigned (next_frame, base - 4, 4);

  trad_frame_set_reg_addr (cache, VAX_SP_REGNUM, addr + 8);
  trad_frame_set_reg_addr (cache, VAX_FP_REGNUM, addr + 12);
  trad_frame_set_reg_addr (cache, VAX_AP_REGNUM, addr + 16);
  trad_frame_set_reg_addr (cache, VAX_PC_REGNUM, addr + 20);
  trad_frame_set_reg_addr (cache, VAX_PS_REGNUM, addr + 24);

  /* Construct the frame ID using the function start.  */
  trad_frame_set_id (cache, frame_id_build (base, func));

  return cache;
}

static void
vaxobsd_sigtramp_frame_this_id (struct frame_info *next_frame,
				void **this_cache, struct frame_id *this_id)
{
  struct trad_frame_cache *cache =
    vaxobsd_sigtramp_frame_cache (next_frame, this_cache);

  trad_frame_get_id (cache, this_id);
}

static void
vaxobsd_sigtramp_frame_prev_register (struct frame_info *next_frame,
				      void **this_cache, int regnum,
				      int *optimizedp, enum lval_type *lvalp,
				      CORE_ADDR *addrp, int *realnump,
				      gdb_byte *valuep)
{
  struct trad_frame_cache *cache =
    vaxobsd_sigtramp_frame_cache (next_frame, this_cache);

  trad_frame_get_register (cache, next_frame, regnum,
			   optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind vaxobsd_sigtramp_frame_unwind = {
  SIGTRAMP_FRAME,
  vaxobsd_sigtramp_frame_this_id,
  vaxobsd_sigtramp_frame_prev_register
};

static const struct frame_unwind *
vaxobsd_sigtramp_frame_sniffer (struct frame_info *next_frame)
{
  if (vaxobsd_sigtramp_p (next_frame))
    return &vaxobsd_sigtramp_frame_unwind;

  return NULL;
}


/* OpenBSD a.out.  */

static void
vaxobsd_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  frame_unwind_append_sniffer (gdbarch, vaxobsd_sigtramp_frame_sniffer);
}

/* FIXME: kettenis/20050821: Since OpenBSD/vax binaries are
   indistingushable from NetBSD/vax a.out binaries, building a GDB
   that should support both these targets will probably not work as
   expected.  */
#define GDB_OSABI_OPENBSD_AOUT GDB_OSABI_NETBSD_AOUT

static enum gdb_osabi
vaxobsd_aout_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "a.out-vax-netbsd") == 0)
    return GDB_OSABI_OPENBSD_AOUT;

  return GDB_OSABI_UNKNOWN;
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_vaxobsd_tdep (void);

void
_initialize_vaxobsd_tdep (void)
{
  gdbarch_register_osabi_sniffer (bfd_arch_vax, bfd_target_aout_flavour,
				  vaxobsd_aout_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_vax, 0, GDB_OSABI_OPENBSD_AOUT,
			  vaxobsd_init_abi);
}
