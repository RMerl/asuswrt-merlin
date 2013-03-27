/* Target-dependent code for OpenBSD/sparc.

   Copyright (C) 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

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
#include "floatformat.h"
#include "frame.h"
#include "frame-unwind.h"
#include "gdbcore.h"
#include "osabi.h"
#include "regcache.h"
#include "symtab.h"
#include "trad-frame.h"

#include "gdb_assert.h"

#include "obsd-tdep.h"
#include "sparc-tdep.h"
#include "solib-svr4.h"
#include "bsd-uthread.h"

/* Signal trampolines.  */

/* The OpenBSD kernel maps the signal trampoline at some random
   location in user space, which means that the traditional BSD way of
   detecting it won't work.

   The signal trampoline will be mapped at an address that is page
   aligned.  We recognize the signal trampoline by looking for the
   sigreturn system call.  */

static const int sparc32obsd_page_size = 4096;

static int
sparc32obsd_pc_in_sigtramp (CORE_ADDR pc, char *name)
{
  CORE_ADDR start_pc = (pc & ~(sparc32obsd_page_size - 1));
  unsigned long insn;

  if (name)
    return 0;

  /* Check for "restore %g0, SYS_sigreturn, %g1".  */
  insn = sparc_fetch_instruction (start_pc + 0xec);
  if (insn != 0x83e82067)
    return 0;

  /* Check for "t ST_SYSCALL".  */
  insn = sparc_fetch_instruction (start_pc + 0xf4);
  if (insn != 0x91d02000)
    return 0;

  return 1;
}

static struct sparc_frame_cache *
sparc32obsd_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct sparc_frame_cache *cache;
  CORE_ADDR addr;

  if (*this_cache)
    return *this_cache;

  cache = sparc_frame_cache (next_frame, this_cache);
  gdb_assert (cache == *this_cache);

  /* If we couldn't find the frame's function, we're probably dealing
     with an on-stack signal trampoline.  */
  if (cache->pc == 0)
    {
      cache->pc = frame_pc_unwind (next_frame);
      cache->pc &= ~(sparc32obsd_page_size - 1);

      /* Since we couldn't find the frame's function, the cache was
         initialized under the assumption that we're frameless.  */
      cache->frameless_p = 0;
      addr = frame_unwind_register_unsigned (next_frame, SPARC_FP_REGNUM);
      cache->base = addr;
    }

  cache->saved_regs = sparc32nbsd_sigcontext_saved_regs (next_frame);

  return cache;
}

static void
sparc32obsd_frame_this_id (struct frame_info *next_frame, void **this_cache,
			   struct frame_id *this_id)
{
  struct sparc_frame_cache *cache =
    sparc32obsd_frame_cache (next_frame, this_cache);

  (*this_id) = frame_id_build (cache->base, cache->pc);
}

static void
sparc32obsd_frame_prev_register (struct frame_info *next_frame,
				 void **this_cache,
				 int regnum, int *optimizedp,
				 enum lval_type *lvalp, CORE_ADDR *addrp,
				 int *realnump, gdb_byte *valuep)
{
  struct sparc_frame_cache *cache =
    sparc32obsd_frame_cache (next_frame, this_cache);

  trad_frame_get_prev_register (next_frame, cache->saved_regs, regnum,
				optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind sparc32obsd_frame_unwind =
{
  SIGTRAMP_FRAME,
  sparc32obsd_frame_this_id,
  sparc32obsd_frame_prev_register
};

static const struct frame_unwind *
sparc32obsd_sigtramp_frame_sniffer (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  char *name;

  find_pc_partial_function (pc, &name, NULL, NULL);
  if (sparc32obsd_pc_in_sigtramp (pc, name))
    return &sparc32obsd_frame_unwind;

  return NULL;
}


/* Offset wthin the thread structure where we can find %fp and %i7.  */
#define SPARC32OBSD_UTHREAD_FP_OFFSET	128
#define SPARC32OBSD_UTHREAD_PC_OFFSET	132

static void
sparc32obsd_supply_uthread (struct regcache *regcache,
			    int regnum, CORE_ADDR addr)
{
  CORE_ADDR fp, fp_addr = addr + SPARC32OBSD_UTHREAD_FP_OFFSET;
  gdb_byte buf[4];

  gdb_assert (regnum >= -1);

  fp = read_memory_unsigned_integer (fp_addr, 4);
  if (regnum == SPARC_SP_REGNUM || regnum == -1)
    {
      store_unsigned_integer (buf, 4, fp);
      regcache_raw_supply (regcache, SPARC_SP_REGNUM, buf);

      if (regnum == SPARC_SP_REGNUM)
	return;
    }

  if (regnum == SPARC32_PC_REGNUM || regnum == SPARC32_NPC_REGNUM
      || regnum == -1)
    {
      CORE_ADDR i7, i7_addr = addr + SPARC32OBSD_UTHREAD_PC_OFFSET;

      i7 = read_memory_unsigned_integer (i7_addr, 4);
      if (regnum == SPARC32_PC_REGNUM || regnum == -1)
	{
	  store_unsigned_integer (buf, 4, i7 + 8);
	  regcache_raw_supply (regcache, SPARC32_PC_REGNUM, buf);
	}
      if (regnum == SPARC32_NPC_REGNUM || regnum == -1)
	{
	  store_unsigned_integer (buf, 4, i7 + 12);
	  regcache_raw_supply (regcache, SPARC32_NPC_REGNUM, buf);
	}

      if (regnum == SPARC32_PC_REGNUM || regnum == SPARC32_NPC_REGNUM)
	return;
    }

  sparc_supply_rwindow (regcache, fp, regnum);
}

static void
sparc32obsd_collect_uthread(const struct regcache *regcache,
			    int regnum, CORE_ADDR addr)
{
  CORE_ADDR sp;
  gdb_byte buf[4];

  gdb_assert (regnum >= -1);

  if (regnum == SPARC_SP_REGNUM || regnum == -1)
    {
      CORE_ADDR fp_addr = addr + SPARC32OBSD_UTHREAD_FP_OFFSET;

      regcache_raw_collect (regcache, SPARC_SP_REGNUM, buf);
      write_memory (fp_addr,buf, 4);
    }

  if (regnum == SPARC32_PC_REGNUM || regnum == -1)
    {
      CORE_ADDR i7, i7_addr = addr + SPARC32OBSD_UTHREAD_PC_OFFSET;

      regcache_raw_collect (regcache, SPARC32_PC_REGNUM, buf);
      i7 = extract_unsigned_integer (buf, 4) - 8;
      write_memory_unsigned_integer (i7_addr, 4, i7);

      if (regnum == SPARC32_PC_REGNUM)
	return;
    }

  regcache_raw_collect (regcache, SPARC_SP_REGNUM, buf);
  sp = extract_unsigned_integer (buf, 4);
  sparc_collect_rwindow (regcache, sp, regnum);
}


static void
sparc32obsd_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* OpenBSD/sparc is very similar to NetBSD/sparc ELF.  */
  sparc32nbsd_elf_init_abi (info, gdbarch);

  set_gdbarch_skip_solib_resolver (gdbarch, obsd_skip_solib_resolver);

  frame_unwind_append_sniffer (gdbarch, sparc32obsd_sigtramp_frame_sniffer);

  /* OpenBSD provides a user-level threads implementation.  */
  bsd_uthread_set_supply_uthread (gdbarch, sparc32obsd_supply_uthread);
  bsd_uthread_set_collect_uthread (gdbarch, sparc32obsd_collect_uthread);
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_sparc32obsd_tdep (void);

void
_initialize_sparc32obsd_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_sparc, 0, GDB_OSABI_OPENBSD_ELF,
			  sparc32obsd_init_abi);
}
