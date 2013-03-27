/* Target-dependent code for OpenBSD/powerpc.

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
#include "arch-utils.h"
#include "frame.h"
#include "frame-unwind.h"
#include "gdbtypes.h"
#include "osabi.h"
#include "regcache.h"
#include "regset.h"
#include "symtab.h"
#include "trad-frame.h"

#include "gdb_assert.h"
#include "gdb_string.h"

#include "ppc-tdep.h"
#include "ppcobsd-tdep.h"
#include "solib-svr4.h"

/* Register offsets from <machine/reg.h>.  */
struct ppc_reg_offsets ppcobsd_reg_offsets;
struct ppc_reg_offsets ppcobsd_fpreg_offsets;


/* Core file support.  */

/* Supply register REGNUM in the general-purpose register set REGSET
   from the buffer specified by GREGS and LEN to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

void
ppcobsd_supply_gregset (const struct regset *regset,
			struct regcache *regcache, int regnum,
			const void *gregs, size_t len)
{
  ppc_supply_gregset (regset, regcache, regnum, gregs, len);
  ppc_supply_fpregset (regset, regcache, regnum, gregs, len);
}

/* Collect register REGNUM in the general-purpose register set
   REGSET. from register cache REGCACHE into the buffer specified by
   GREGS and LEN.  If REGNUM is -1, do this for all registers in
   REGSET.  */

void
ppcobsd_collect_gregset (const struct regset *regset,
			 const struct regcache *regcache, int regnum,
			 void *gregs, size_t len)
{
  ppc_collect_gregset (regset, regcache, regnum, gregs, len);
  ppc_collect_fpregset (regset, regcache, regnum, gregs, len);
}

/* OpenBSD/powerpc register set.  */

struct regset ppcobsd_gregset =
{
  &ppcobsd_reg_offsets,
  ppcobsd_supply_gregset
};

struct regset ppcobsd_fpregset =
{
  &ppcobsd_fpreg_offsets,
  ppc_supply_fpregset
};

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

static const struct regset *
ppcobsd_regset_from_core_section (struct gdbarch *gdbarch,
				  const char *sect_name, size_t sect_size)
{
  if (strcmp (sect_name, ".reg") == 0 && sect_size >= 412)
    return &ppcobsd_gregset;

  return NULL;
}


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
static const int ppcobsd_page_size = 4096;

/* Offset for sigreturn(2).  */
static const int ppcobsd_sigreturn_offset[] = {
  0x98,				/* OpenBSD 3.8 */
  0x0c,				/* OpenBSD 3.2 */
  -1
};

static int
ppcobsd_sigtramp_p (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  CORE_ADDR start_pc = (pc & ~(ppcobsd_page_size - 1));
  const int *offset;
  char *name;

  find_pc_partial_function (pc, &name, NULL, NULL);
  if (name)
    return 0;

  for (offset = ppcobsd_sigreturn_offset; *offset != -1; offset++)
    {
      gdb_byte buf[2 * PPC_INSN_SIZE];
      unsigned long insn;

      if (!safe_frame_unwind_memory (next_frame, start_pc + *offset,
				     buf, sizeof buf))
	continue;

      /* Check for "li r0,SYS_sigreturn".  */
      insn = extract_unsigned_integer (buf, PPC_INSN_SIZE);
      if (insn != 0x38000067)
	continue;

      /* Check for "sc".  */
      insn = extract_unsigned_integer (buf + PPC_INSN_SIZE, PPC_INSN_SIZE);
      if (insn != 0x44000002)
	continue;

      return 1;
    }

  return 0;
}

static struct trad_frame_cache *
ppcobsd_sigtramp_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  struct trad_frame_cache *cache;
  CORE_ADDR addr, base, func;
  gdb_byte buf[PPC_INSN_SIZE];
  unsigned long insn, sigcontext_offset;
  int i;

  if (*this_cache)
    return *this_cache;

  cache = trad_frame_cache_zalloc (next_frame);
  *this_cache = cache;

  func = frame_pc_unwind (next_frame);
  func &= ~(ppcobsd_page_size - 1);
  if (!safe_frame_unwind_memory (next_frame, func, buf, sizeof buf))
    return cache;

  /* Calculate the offset where we can find `struct sigcontext'.  We
     base our calculation on the amount of stack space reserved by the
     first instruction of the signal trampoline.  */
  insn = extract_unsigned_integer (buf, PPC_INSN_SIZE);
  sigcontext_offset = (0x10000 - (insn & 0x0000ffff)) + 8;

  base = frame_unwind_register_unsigned (next_frame,
					 gdbarch_sp_regnum (current_gdbarch));
  addr = base + sigcontext_offset + 2 * tdep->wordsize;
  for (i = 0; i < ppc_num_gprs; i++, addr += tdep->wordsize)
    {
      int regnum = i + tdep->ppc_gp0_regnum;
      trad_frame_set_reg_addr (cache, regnum, addr);
    }
  trad_frame_set_reg_addr (cache, tdep->ppc_lr_regnum, addr);
  addr += tdep->wordsize;
  trad_frame_set_reg_addr (cache, tdep->ppc_cr_regnum, addr);
  addr += tdep->wordsize;
  trad_frame_set_reg_addr (cache, tdep->ppc_xer_regnum, addr);
  addr += tdep->wordsize;
  trad_frame_set_reg_addr (cache, tdep->ppc_ctr_regnum, addr);
  addr += tdep->wordsize;
  trad_frame_set_reg_addr (cache, gdbarch_pc_regnum (current_gdbarch), addr);
  /* SRR0? */
  addr += tdep->wordsize;

  /* Construct the frame ID using the function start.  */
  trad_frame_set_id (cache, frame_id_build (base, func));

  return cache;
}

static void
ppcobsd_sigtramp_frame_this_id (struct frame_info *next_frame,
				void **this_cache, struct frame_id *this_id)
{
  struct trad_frame_cache *cache =
    ppcobsd_sigtramp_frame_cache (next_frame, this_cache);

  trad_frame_get_id (cache, this_id);
}

static void
ppcobsd_sigtramp_frame_prev_register (struct frame_info *next_frame,
				      void **this_cache, int regnum,
				      int *optimizedp, enum lval_type *lvalp,
				      CORE_ADDR *addrp, int *realnump,
				      gdb_byte *valuep)
{
  struct trad_frame_cache *cache =
    ppcobsd_sigtramp_frame_cache (next_frame, this_cache);

  trad_frame_get_register (cache, next_frame, regnum,
			   optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind ppcobsd_sigtramp_frame_unwind = {
  SIGTRAMP_FRAME,
  ppcobsd_sigtramp_frame_this_id,
  ppcobsd_sigtramp_frame_prev_register
};

static const struct frame_unwind *
ppcobsd_sigtramp_frame_sniffer (struct frame_info *next_frame)
{
  if (ppcobsd_sigtramp_p (next_frame))
    return &ppcobsd_sigtramp_frame_unwind;

  return NULL;
}


static void
ppcobsd_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  /* OpenBSD doesn't support the 128-bit `long double' from the psABI.  */
  set_gdbarch_long_double_bit (gdbarch, 64);
  set_gdbarch_long_double_format (gdbarch, floatformats_ieee_double);

  /* OpenBSD currently uses a broken GCC.  */
  set_gdbarch_return_value (gdbarch, ppc_sysv_abi_broken_return_value);

  /* OpenBSD uses SVR4-style shared libraries.  */
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);

  set_gdbarch_regset_from_core_section
    (gdbarch, ppcobsd_regset_from_core_section);

  frame_unwind_append_sniffer (gdbarch, ppcobsd_sigtramp_frame_sniffer);
}


/* OpenBSD uses uses the traditional NetBSD core file format, even for
   ports that use ELF.  */
#define GDB_OSABI_NETBSD_CORE GDB_OSABI_OPENBSD_ELF

static enum gdb_osabi
ppcobsd_core_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "netbsd-core") == 0)
    return GDB_OSABI_NETBSD_CORE;

  return GDB_OSABI_UNKNOWN;
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_ppcobsd_tdep (void);

void
_initialize_ppcobsd_tdep (void)
{
  /* BFD doesn't set a flavour for NetBSD style a.out core files.  */
  gdbarch_register_osabi_sniffer (bfd_arch_powerpc, bfd_target_unknown_flavour,
                                  ppcobsd_core_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_rs6000, 0, GDB_OSABI_OPENBSD_ELF,
			  ppcobsd_init_abi);
  gdbarch_register_osabi (bfd_arch_powerpc, 0, GDB_OSABI_OPENBSD_ELF,
			  ppcobsd_init_abi);

  /* Avoid initializing the register offsets again if they were
     already initailized by ppcobsd-nat.c.  */
  if (ppcobsd_reg_offsets.pc_offset == 0)
    {
      /* General-purpose registers.  */
      ppcobsd_reg_offsets.r0_offset = 0;
      ppcobsd_reg_offsets.gpr_size = 4;
      ppcobsd_reg_offsets.xr_size = 4;
      ppcobsd_reg_offsets.pc_offset = 384;
      ppcobsd_reg_offsets.ps_offset = 388;
      ppcobsd_reg_offsets.cr_offset = 392;
      ppcobsd_reg_offsets.lr_offset = 396;
      ppcobsd_reg_offsets.ctr_offset = 400;
      ppcobsd_reg_offsets.xer_offset = 404;
      ppcobsd_reg_offsets.mq_offset = 408;

      /* Floating-point registers.  */
      ppcobsd_reg_offsets.f0_offset = 128;
      ppcobsd_reg_offsets.fpscr_offset = -1;

      /* AltiVec registers.  */
      ppcobsd_reg_offsets.vr0_offset = 0;
      ppcobsd_reg_offsets.vscr_offset = 512;
      ppcobsd_reg_offsets.vrsave_offset = 520;
    }

  if (ppcobsd_fpreg_offsets.fpscr_offset == 0)
    {
      /* Floating-point registers.  */
      ppcobsd_reg_offsets.f0_offset = 0;
      ppcobsd_reg_offsets.fpscr_offset = 256;
      ppcobsd_reg_offsets.fpscr_size = 4;
    }
}
