/* Target-dependent code for QNX Neutrino x86.

   Copyright (C) 2003, 2004, 2007 Free Software Foundation, Inc.

   Contributed by QNX Software Systems Ltd.

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
#include "osabi.h"
#include "regcache.h"
#include "target.h"

#include "gdb_assert.h"
#include "gdb_string.h"

#include "i386-tdep.h"
#include "i387-tdep.h"
#include "nto-tdep.h"
#include "solib-svr4.h"

/* Target vector for QNX NTO x86.  */
static struct nto_target_ops i386_nto_target;

#ifndef X86_CPU_FXSR
#define X86_CPU_FXSR (1L << 12)
#endif

/* Why 13?  Look in our /usr/include/x86/context.h header at the
   x86_cpu_registers structure and you'll see an 'exx' junk register
   that is just filler.  Don't ask me, ask the kernel guys.  */
#define NUM_GPREGS 13

/* Mapping between the general-purpose registers in `struct xxx'
   format and GDB's register cache layout.  */

/* From <x86/context.h>.  */
static int i386nto_gregset_reg_offset[] =
{
  7 * 4,			/* %eax */
  6 * 4,			/* %ecx */
  5 * 4,			/* %edx */
  4 * 4,			/* %ebx */
  11 * 4,			/* %esp */
  2 * 4,			/* %epb */
  1 * 4,			/* %esi */
  0 * 4,			/* %edi */
  8 * 4,			/* %eip */
  10 * 4,			/* %eflags */
  9 * 4,			/* %cs */
  12 * 4,			/* %ss */
  -1				/* filler */
};

/* Given a GDB register number REGNUM, return the offset into
   Neutrino's register structure or -1 if the register is unknown.  */

static int
nto_reg_offset (int regnum)
{
  if (regnum >= 0 && regnum < ARRAY_SIZE (i386nto_gregset_reg_offset))
    return i386nto_gregset_reg_offset[regnum];

  return -1;
}

static void
i386nto_supply_gregset (struct regcache *regcache, char *gpregs)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  if(tdep->gregset == NULL)
    tdep->gregset = regset_alloc (current_gdbarch, i386_supply_gregset,
				  i386_collect_gregset);

  gdb_assert (tdep->gregset_reg_offset == i386nto_gregset_reg_offset);
  tdep->gregset->supply_regset (tdep->gregset, regcache, -1,
				gpregs, NUM_GPREGS * 4);
}

static void
i386nto_supply_fpregset (struct regcache *regcache, char *fpregs)
{
  if (nto_cpuinfo_valid && nto_cpuinfo_flags | X86_CPU_FXSR)
    i387_supply_fxsave (regcache, -1, fpregs);
  else
    i387_supply_fsave (regcache, -1, fpregs);
}

static void
i386nto_supply_regset (struct regcache *regcache, int regset, char *data)
{
  switch (regset)
    {
    case NTO_REG_GENERAL:
      i386nto_supply_gregset (regcache, data);
      break;
    case NTO_REG_FLOAT:
      i386nto_supply_fpregset (regcache, data);
      break;
    }
}

static int
i386nto_regset_id (int regno)
{
  if (regno == -1)
    return NTO_REG_END;
  else if (regno < I386_NUM_GREGS)
    return NTO_REG_GENERAL;
  else if (regno < I386_NUM_GREGS + I386_NUM_FREGS)
    return NTO_REG_FLOAT;

  return -1;			/* Error.  */
}

static int
i386nto_register_area (int regno, int regset, unsigned *off)
{
  int len;

  *off = 0;
  if (regset == NTO_REG_GENERAL)
    {
      if (regno == -1)
	return NUM_GPREGS * 4;

      *off = nto_reg_offset (regno);
      if (*off == -1)
	return 0;
      return 4;
    }
  else if (regset == NTO_REG_FLOAT)
    {
      unsigned off_adjust, regsize, regset_size;

      if (nto_cpuinfo_valid && nto_cpuinfo_flags | X86_CPU_FXSR)
	{
	  off_adjust = 32;
	  regsize = 16;
	  regset_size = 512;
	}
      else
	{
	  off_adjust = 28;
	  regsize = 10;
	  regset_size = 128;
	}

      if (regno == -1)
	return regset_size;

      *off = (regno - gdbarch_fp0_regnum (current_gdbarch))
	     * regsize + off_adjust;
      return 10;
      /* Why 10 instead of regsize?  GDB only stores 10 bytes per FP
         register so if we're sending a register back to the target,
         we only want pdebug to write 10 bytes so as not to clobber
         the reserved 6 bytes in the fxsave structure.  */
    }
  return -1;
}

static int
i386nto_regset_fill (const struct regcache *regcache, int regset, char *data)
{
  if (regset == NTO_REG_GENERAL)
    {
      int regno;

      for (regno = 0; regno < NUM_GPREGS; regno++)
	{
	  int offset = nto_reg_offset (regno);
	  if (offset != -1)
	    regcache_raw_collect (regcache, regno, data + offset);
	}
    }
  else if (regset == NTO_REG_FLOAT)
    {
      if (nto_cpuinfo_valid && nto_cpuinfo_flags | X86_CPU_FXSR)
	i387_collect_fxsave (regcache, -1, data);
      else
	i387_collect_fsave (regcache, -1, data);
    }
  else
    return -1;

  return 0;
}

/* Return whether the frame preceding NEXT_FRAME corresponds to a QNX
   Neutrino sigtramp routine.  */

static int
i386nto_sigtramp_p (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  char *name;

  find_pc_partial_function (pc, &name, NULL, NULL);
  return name && strcmp ("__signalstub", name) == 0;
}

#define I386_NTO_SIGCONTEXT_OFFSET 136

/* Assuming NEXT_FRAME is a frame following a QNX Neutrino sigtramp
   routine, return the address of the associated sigcontext structure.  */

static CORE_ADDR
i386nto_sigcontext_addr (struct frame_info *next_frame)
{
  char buf[4];
  CORE_ADDR sp;

  frame_unwind_register (next_frame, I386_ESP_REGNUM, buf);
  sp = extract_unsigned_integer (buf, 4);

  return sp + I386_NTO_SIGCONTEXT_OFFSET;
}

static void
init_i386nto_ops (void)
{
  i386_nto_target.regset_id = i386nto_regset_id;
  i386_nto_target.supply_gregset = i386nto_supply_gregset;
  i386_nto_target.supply_fpregset = i386nto_supply_fpregset;
  i386_nto_target.supply_altregset = nto_dummy_supply_regset;
  i386_nto_target.supply_regset = i386nto_supply_regset;
  i386_nto_target.register_area = i386nto_register_area;
  i386_nto_target.regset_fill = i386nto_regset_fill;
  i386_nto_target.fetch_link_map_offsets =
    svr4_ilp32_fetch_link_map_offsets;
}

static void
i386nto_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* Deal with our strange signals.  */
  nto_initialize_signals ();

  /* NTO uses ELF.  */
  i386_elf_init_abi (info, gdbarch);

  /* Neutrino rewinds to look more normal.  Need to override the i386
     default which is [unfortunately] to decrement the PC.  */
  set_gdbarch_decr_pc_after_break (gdbarch, 0);

  tdep->gregset_reg_offset = i386nto_gregset_reg_offset;
  tdep->gregset_num_regs = ARRAY_SIZE (i386nto_gregset_reg_offset);
  tdep->sizeof_gregset = NUM_GPREGS * 4;

  tdep->sigtramp_p = i386nto_sigtramp_p;
  tdep->sigcontext_addr = i386nto_sigcontext_addr;
  tdep->sc_pc_offset = 56;
  tdep->sc_sp_offset = 68;

  /* Setjmp()'s return PC saved in EDX (5).  */
  tdep->jb_pc_offset = 20;	/* 5x32 bit ints in.  */

  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);

  /* Our loader handles solib relocations slightly differently than svr4.  */
  TARGET_SO_RELOCATE_SECTION_ADDRESSES = nto_relocate_section_addresses;

  /* Supply a nice function to find our solibs.  */
  TARGET_SO_FIND_AND_OPEN_SOLIB = nto_find_and_open_solib;

  /* Our linker code is in libc.  */
  TARGET_SO_IN_DYNSYM_RESOLVE_CODE = nto_in_dynsym_resolve_code;

  nto_set_target (&i386_nto_target);
}

void
_initialize_i386nto_tdep (void)
{
  init_i386nto_ops ();
  gdbarch_register_osabi (bfd_arch_i386, 0, GDB_OSABI_QNXNTO,
			  i386nto_init_abi);
  gdbarch_register_osabi_sniffer (bfd_arch_i386, bfd_target_elf_flavour,
				  nto_elf_osabi_sniffer);
}
