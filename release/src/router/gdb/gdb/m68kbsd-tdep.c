/* Target-dependent code for Motorola 68000 BSD's.

   Copyright (C) 2004, 2005, 2007 Free Software Foundation, Inc.

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
#include "osabi.h"
#include "regcache.h"
#include "regset.h"
#include "trad-frame.h"
#include "tramp-frame.h"

#include "gdb_assert.h"
#include "gdb_string.h"

#include "m68k-tdep.h"
#include "solib-svr4.h"

/* Core file support.  */

/* Sizeof `struct reg' in <machine/reg.h>.  */
#define M68KBSD_SIZEOF_GREGS	(18 * 4)

/* Sizeof `struct fpreg' in <machine/reg.h.  */
#define M68KBSD_SIZEOF_FPREGS	(((8 * 3) + 3) * 4)

int
m68kbsd_fpreg_offset (int regnum)
{
  int fp_len = TYPE_LENGTH (gdbarch_register_type (current_gdbarch, regnum));
  
  if (regnum >= M68K_FPC_REGNUM)
    return 8 * fp_len + (regnum - M68K_FPC_REGNUM) * 4;

  return (regnum - M68K_FP0_REGNUM) * fp_len;
}

/* Supply register REGNUM from the buffer specified by FPREGS and LEN
   in the floating-point register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
m68kbsd_supply_fpregset (const struct regset *regset,
			 struct regcache *regcache,
			 int regnum, const void *fpregs, size_t len)
{
  const gdb_byte *regs = fpregs;
  int i;

  gdb_assert (len >= M68KBSD_SIZEOF_FPREGS);

  for (i = M68K_FP0_REGNUM; i <= M68K_PC_REGNUM; i++)
    {
      if (regnum == i || regnum == -1)
	regcache_raw_supply (regcache, i, regs + m68kbsd_fpreg_offset (i));
    }
}

/* Supply register REGNUM from the buffer specified by GREGS and LEN
   in the general-purpose register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
m68kbsd_supply_gregset (const struct regset *regset,
			struct regcache *regcache,
			int regnum, const void *gregs, size_t len)
{
  const gdb_byte *regs = gregs;
  int i;

  gdb_assert (len >= M68KBSD_SIZEOF_GREGS);

  for (i = M68K_D0_REGNUM; i <= M68K_PC_REGNUM; i++)
    {
      if (regnum == i || regnum == -1)
	regcache_raw_supply (regcache, i, regs + i * 4);
    }

  if (len >= M68KBSD_SIZEOF_GREGS + M68KBSD_SIZEOF_FPREGS)
    {
      regs += M68KBSD_SIZEOF_GREGS;
      len -= M68KBSD_SIZEOF_GREGS;
      m68kbsd_supply_fpregset (regset, regcache, regnum, regs, len);
    }
}

/* Motorola 68000 register sets.  */

static struct regset m68kbsd_gregset =
{
  NULL,
  m68kbsd_supply_gregset
};

static struct regset m68kbsd_fpregset =
{
  NULL,
  m68kbsd_supply_fpregset
};

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

static const struct regset *
m68kbsd_regset_from_core_section (struct gdbarch *gdbarch,
				  const char *sect_name, size_t sect_size)
{
  if (strcmp (sect_name, ".reg") == 0 && sect_size >= M68KBSD_SIZEOF_GREGS)
    return &m68kbsd_gregset;

  if (strcmp (sect_name, ".reg2") == 0 && sect_size >= M68KBSD_SIZEOF_FPREGS)
    return &m68kbsd_fpregset;

  return NULL;
}


/* Signal trampolines.  */

static void
m68kobsd_sigtramp_cache_init (const struct tramp_frame *self,
			      struct frame_info *next_frame,
			      struct trad_frame_cache *this_cache,
			      CORE_ADDR func)
{
  CORE_ADDR addr, base, pc;
  int regnum;

  base = frame_unwind_register_unsigned (next_frame, M68K_SP_REGNUM);

  /* The 'addql #4,%sp' instruction at offset 8 adjusts the stack
     pointer.  Adjust the frame base accordingly.  */
  pc = frame_unwind_register_unsigned (next_frame, M68K_PC_REGNUM);
  if ((pc - func) > 8)
    base -= 4;

  /* Get frame pointer, stack pointer, program counter and processor
     state from `struct sigcontext'.  */
  addr = get_frame_memory_unsigned (next_frame, base + 8, 4);
  trad_frame_set_reg_addr (this_cache, M68K_FP_REGNUM, addr + 8);
  trad_frame_set_reg_addr (this_cache, M68K_SP_REGNUM, addr + 12);
  trad_frame_set_reg_addr (this_cache, M68K_PC_REGNUM, addr + 20);
  trad_frame_set_reg_addr (this_cache, M68K_PS_REGNUM, addr + 24);

  /* The sc_ap member of `struct sigcontext' points to additional
     hardware state.  Here we find the missing registers.  */
  addr = get_frame_memory_unsigned (next_frame, addr + 16, 4) + 4;
  for (regnum = M68K_D0_REGNUM; regnum < M68K_FP_REGNUM; regnum++, addr += 4)
    trad_frame_set_reg_addr (this_cache, regnum, addr);

  /* Construct the frame ID using the function start.  */
  trad_frame_set_id (this_cache, frame_id_build (base, func));
}

static const struct tramp_frame m68kobsd_sigtramp = {
  SIGTRAMP_FRAME,
  2,
  {
    { 0x206f, -1 }, { 0x000c, -1},	/* moveal %sp@(12),%a0 */
    { 0x4e90, -1 },			/* jsr %a0@ */
    { 0x588f, -1 },			/* addql #4,%sp */
    { 0x4e41, -1 },			/* trap #1 */
    { 0x2f40, -1 }, { 0x0004, -1 },	/* moveal %d0,%sp@(4) */
    { 0x7001, -1 },			/* moveq #SYS_exit,%d0 */
    { 0x4e40, -1 },			/* trap #0 */
    { TRAMP_SENTINEL_INSN, -1 }
  },
  m68kobsd_sigtramp_cache_init
};


static void
m68kbsd_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->jb_pc = 5;
  tdep->jb_elt_size = 4;

  set_gdbarch_decr_pc_after_break (gdbarch, 2);

  set_gdbarch_regset_from_core_section
    (gdbarch, m68kbsd_regset_from_core_section);
}

/* OpenBSD and NetBSD a.out.  */

static void
m68kbsd_aout_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  m68kbsd_init_abi (info, gdbarch);

  tdep->struct_return = reg_struct_return;

  tramp_frame_prepend_unwinder (gdbarch, &m68kobsd_sigtramp);
}

/* NetBSD ELF.  */

static void
m68kbsd_elf_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  m68kbsd_init_abi (info, gdbarch);

  /* NetBSD ELF uses the SVR4 ABI.  */
  m68k_svr4_init_abi (info, gdbarch);
  tdep->struct_return = pcc_struct_return;

  /* NetBSD ELF uses SVR4-style shared libraries.  */
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);
}


static enum gdb_osabi
m68kbsd_aout_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "a.out-m68k-netbsd") == 0
      || strcmp (bfd_get_target (abfd), "a.out-m68k4k-netbsd") == 0)
    return GDB_OSABI_NETBSD_AOUT;

  return GDB_OSABI_UNKNOWN;
}

static enum gdb_osabi
m68kbsd_core_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "netbsd-core") == 0)
    return GDB_OSABI_NETBSD_AOUT;

  return GDB_OSABI_UNKNOWN;
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_m68kbsd_tdep (void);

void
_initialize_m68kbsd_tdep (void)
{
  gdbarch_register_osabi_sniffer (bfd_arch_m68k, bfd_target_aout_flavour,
				  m68kbsd_aout_osabi_sniffer);

  /* BFD doesn't set a flavour for NetBSD style a.out core files.  */
  gdbarch_register_osabi_sniffer (bfd_arch_m68k, bfd_target_unknown_flavour,
				  m68kbsd_core_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_m68k, 0, GDB_OSABI_NETBSD_AOUT,
			  m68kbsd_aout_init_abi);
  gdbarch_register_osabi (bfd_arch_m68k, 0, GDB_OSABI_NETBSD_ELF,
			  m68kbsd_elf_init_abi);
}
