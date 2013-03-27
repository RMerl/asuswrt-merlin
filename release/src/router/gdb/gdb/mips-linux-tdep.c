/* Target-dependent code for GNU/Linux on MIPS processors.

   Copyright (C) 2001, 2002, 2004, 2005, 2006, 2007
   Free Software Foundation, Inc.

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
#include "target.h"
#include "solib-svr4.h"
#include "osabi.h"
#include "mips-tdep.h"
#include "gdb_string.h"
#include "gdb_assert.h"
#include "frame.h"
#include "regcache.h"
#include "trad-frame.h"
#include "tramp-frame.h"
#include "gdbtypes.h"
#include "solib.h"
#include "solib-svr4.h"
#include "solist.h"
#include "symtab.h"
#include "target-descriptions.h"
#include "mips-linux-tdep.h"

static struct target_so_ops mips_svr4_so_ops;

/* Figure out where the longjmp will land.
   We expect the first arg to be a pointer to the jmp_buf structure
   from which we extract the pc (MIPS_LINUX_JB_PC) that we will land
   at.  The pc is copied into PC.  This routine returns 1 on
   success.  */

#define MIPS_LINUX_JB_ELEMENT_SIZE 4
#define MIPS_LINUX_JB_PC 0

static int
mips_linux_get_longjmp_target (struct frame_info *frame, CORE_ADDR *pc)
{
  CORE_ADDR jb_addr;
  char buf[gdbarch_ptr_bit (current_gdbarch) / TARGET_CHAR_BIT];

  jb_addr = get_frame_register_unsigned (frame, MIPS_A0_REGNUM);

  if (target_read_memory (jb_addr
			    + MIPS_LINUX_JB_PC * MIPS_LINUX_JB_ELEMENT_SIZE,
			  buf,
			  gdbarch_ptr_bit (current_gdbarch) / TARGET_CHAR_BIT))
    return 0;

  *pc = extract_unsigned_integer (buf,
				  gdbarch_ptr_bit (current_gdbarch)
				    / TARGET_CHAR_BIT);

  return 1;
}

/* Transform the bits comprising a 32-bit register to the right size
   for regcache_raw_supply().  This is needed when mips_isa_regsize()
   is 8.  */

static void
supply_32bit_reg (struct regcache *regcache, int regnum, const void *addr)
{
  gdb_byte buf[MAX_REGISTER_SIZE];
  store_signed_integer (buf, register_size (current_gdbarch, regnum),
                        extract_signed_integer (addr, 4));
  regcache_raw_supply (regcache, regnum, buf);
}

/* Unpack an elf_gregset_t into GDB's register cache.  */

void
mips_supply_gregset (struct regcache *regcache,
		     const mips_elf_gregset_t *gregsetp)
{
  int regi;
  const mips_elf_greg_t *regp = *gregsetp;
  char zerobuf[MAX_REGISTER_SIZE];

  memset (zerobuf, 0, MAX_REGISTER_SIZE);

  for (regi = EF_REG0 + 1; regi <= EF_REG31; regi++)
    supply_32bit_reg (regcache, regi - EF_REG0, regp + regi);

  if (mips_linux_restart_reg_p (current_gdbarch))
    supply_32bit_reg (regcache, MIPS_RESTART_REGNUM, regp + EF_REG0);

  supply_32bit_reg (regcache, mips_regnum (current_gdbarch)->lo,
		    regp + EF_LO);
  supply_32bit_reg (regcache, mips_regnum (current_gdbarch)->hi,
		    regp + EF_HI);

  supply_32bit_reg (regcache, mips_regnum (current_gdbarch)->pc,
		    regp + EF_CP0_EPC);
  supply_32bit_reg (regcache, mips_regnum (current_gdbarch)->badvaddr,
		    regp + EF_CP0_BADVADDR);
  supply_32bit_reg (regcache, MIPS_PS_REGNUM, regp + EF_CP0_STATUS);
  supply_32bit_reg (regcache, mips_regnum (current_gdbarch)->cause,
		    regp + EF_CP0_CAUSE);

  /* Fill inaccessible registers with zero.  */
  regcache_raw_supply (regcache, MIPS_ZERO_REGNUM, zerobuf);
  regcache_raw_supply (regcache, MIPS_UNUSED_REGNUM, zerobuf);
  for (regi = MIPS_FIRST_EMBED_REGNUM;
       regi <= MIPS_LAST_EMBED_REGNUM;
       regi++)
    regcache_raw_supply (regcache, regi, zerobuf);
}

/* Pack our registers (or one register) into an elf_gregset_t.  */

void
mips_fill_gregset (const struct regcache *regcache,
		   mips_elf_gregset_t *gregsetp, int regno)
{
  int regaddr, regi;
  mips_elf_greg_t *regp = *gregsetp;
  void *dst;

  if (regno == -1)
    {
      memset (regp, 0, sizeof (mips_elf_gregset_t));
      for (regi = 1; regi < 32; regi++)
	mips_fill_gregset (regcache, gregsetp, regi);
      mips_fill_gregset (regcache, gregsetp,
			 mips_regnum (current_gdbarch)->lo);
      mips_fill_gregset (regcache, gregsetp,
			 mips_regnum (current_gdbarch)->hi);
      mips_fill_gregset (regcache, gregsetp,
			 mips_regnum (current_gdbarch)->pc);
      mips_fill_gregset (regcache, gregsetp,
			 mips_regnum (current_gdbarch)->badvaddr);
      mips_fill_gregset (regcache, gregsetp, MIPS_PS_REGNUM);
      mips_fill_gregset (regcache, gregsetp,
			 mips_regnum (current_gdbarch)->cause);
      mips_fill_gregset (regcache, gregsetp, MIPS_RESTART_REGNUM);
      return;
   }

  if (regno > 0 && regno < 32)
    {
      dst = regp + regno + EF_REG0;
      regcache_raw_collect (regcache, regno, dst);
      return;
    }

  if (regno == mips_regnum (current_gdbarch)->lo)
    regaddr = EF_LO;
  else if (regno == mips_regnum (current_gdbarch)->hi)
    regaddr = EF_HI;
  else if (regno == mips_regnum (current_gdbarch)->pc)
    regaddr = EF_CP0_EPC;
  else if (regno == mips_regnum (current_gdbarch)->badvaddr)
    regaddr = EF_CP0_BADVADDR;
  else if (regno == MIPS_PS_REGNUM)
    regaddr = EF_CP0_STATUS;
  else if (regno == mips_regnum (current_gdbarch)->cause)
    regaddr = EF_CP0_CAUSE;
  else if (mips_linux_restart_reg_p (current_gdbarch)
	   && regno == MIPS_RESTART_REGNUM)
    regaddr = EF_REG0;
  else
    regaddr = -1;

  if (regaddr != -1)
    {
      dst = regp + regaddr;
      regcache_raw_collect (regcache, regno, dst);
    }
}

/* Likewise, unpack an elf_fpregset_t.  */

void
mips_supply_fpregset (struct regcache *regcache,
		      const mips_elf_fpregset_t *fpregsetp)
{
  int regi;
  char zerobuf[MAX_REGISTER_SIZE];

  memset (zerobuf, 0, MAX_REGISTER_SIZE);

  for (regi = 0; regi < 32; regi++)
    regcache_raw_supply (regcache,
			 gdbarch_fp0_regnum (current_gdbarch) + regi,
			 *fpregsetp + regi);

  regcache_raw_supply (regcache,
		       mips_regnum (current_gdbarch)->fp_control_status,
		       *fpregsetp + 32);

  /* FIXME: how can we supply FCRIR?  The ABI doesn't tell us.  */
  regcache_raw_supply (regcache,
		       mips_regnum (current_gdbarch)->fp_implementation_revision,
		       zerobuf);
}

/* Likewise, pack one or all floating point registers into an
   elf_fpregset_t.  */

void
mips_fill_fpregset (const struct regcache *regcache,
		    mips_elf_fpregset_t *fpregsetp, int regno)
{
  char *from, *to;

  if ((regno >= gdbarch_fp0_regnum (current_gdbarch))
      && (regno < gdbarch_fp0_regnum (current_gdbarch) + 32))
    {
      to = (char *) (*fpregsetp + regno - gdbarch_fp0_regnum (current_gdbarch));
      regcache_raw_collect (regcache, regno, to);
    }
  else if (regno == mips_regnum (current_gdbarch)->fp_control_status)
    {
      to = (char *) (*fpregsetp + 32);
      regcache_raw_collect (regcache, regno, to);
    }
  else if (regno == -1)
    {
      int regi;

      for (regi = 0; regi < 32; regi++)
	mips_fill_fpregset (regcache, fpregsetp,
			    gdbarch_fp0_regnum (current_gdbarch) + regi);
      mips_fill_fpregset (regcache, fpregsetp,
			  mips_regnum (current_gdbarch)->fp_control_status);
    }
}

/* Support for 64-bit ABIs.  */

/* Figure out where the longjmp will land.
   We expect the first arg to be a pointer to the jmp_buf structure
   from which we extract the pc (MIPS_LINUX_JB_PC) that we will land
   at.  The pc is copied into PC.  This routine returns 1 on
   success.  */

/* Details about jmp_buf.  */

#define MIPS64_LINUX_JB_PC 0

static int
mips64_linux_get_longjmp_target (struct frame_info *frame, CORE_ADDR *pc)
{
  CORE_ADDR jb_addr;
  void *buf = alloca (gdbarch_ptr_bit (current_gdbarch) / TARGET_CHAR_BIT);
  int element_size = gdbarch_ptr_bit (current_gdbarch) == 32 ? 4 : 8;

  jb_addr = get_frame_register_unsigned (frame, MIPS_A0_REGNUM);

  if (target_read_memory (jb_addr + MIPS64_LINUX_JB_PC * element_size,
			  buf,
			  gdbarch_ptr_bit (current_gdbarch) / TARGET_CHAR_BIT))
    return 0;

  *pc = extract_unsigned_integer (buf,
				  gdbarch_ptr_bit (current_gdbarch)
				    / TARGET_CHAR_BIT);

  return 1;
}

/* Register set support functions.  These operate on standard 64-bit
   regsets, but work whether the target is 32-bit or 64-bit.  A 32-bit
   target will still use the 64-bit format for PTRACE_GETREGS.  */

/* Supply a 64-bit register.  */

void
supply_64bit_reg (struct regcache *regcache, int regnum,
		  const gdb_byte *buf)
{
  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG
      && register_size (current_gdbarch, regnum) == 4)
    regcache_raw_supply (regcache, regnum, buf + 4);
  else
    regcache_raw_supply (regcache, regnum, buf);
}

/* Unpack a 64-bit elf_gregset_t into GDB's register cache.  */

void
mips64_supply_gregset (struct regcache *regcache,
		       const mips64_elf_gregset_t *gregsetp)
{
  int regi;
  const mips64_elf_greg_t *regp = *gregsetp;
  gdb_byte zerobuf[MAX_REGISTER_SIZE];

  memset (zerobuf, 0, MAX_REGISTER_SIZE);

  for (regi = MIPS64_EF_REG0 + 1; regi <= MIPS64_EF_REG31; regi++)
    supply_64bit_reg (regcache, regi - MIPS64_EF_REG0,
		      (const gdb_byte *)(regp + regi));

  if (mips_linux_restart_reg_p (current_gdbarch))
    supply_64bit_reg (regcache, MIPS_RESTART_REGNUM,
		      (const gdb_byte *)(regp + MIPS64_EF_REG0));

  supply_64bit_reg (regcache, mips_regnum (current_gdbarch)->lo,
		    (const gdb_byte *) (regp + MIPS64_EF_LO));
  supply_64bit_reg (regcache, mips_regnum (current_gdbarch)->hi,
		    (const gdb_byte *) (regp + MIPS64_EF_HI));

  supply_64bit_reg (regcache, mips_regnum (current_gdbarch)->pc,
		    (const gdb_byte *) (regp + MIPS64_EF_CP0_EPC));
  supply_64bit_reg (regcache, mips_regnum (current_gdbarch)->badvaddr,
		    (const gdb_byte *) (regp + MIPS64_EF_CP0_BADVADDR));
  supply_64bit_reg (regcache, MIPS_PS_REGNUM,
		    (const gdb_byte *) (regp + MIPS64_EF_CP0_STATUS));
  supply_64bit_reg (regcache, mips_regnum (current_gdbarch)->cause,
		    (const gdb_byte *) (regp + MIPS64_EF_CP0_CAUSE));

  /* Fill inaccessible registers with zero.  */
  regcache_raw_supply (regcache, MIPS_ZERO_REGNUM, zerobuf);
  regcache_raw_supply (regcache, MIPS_UNUSED_REGNUM, zerobuf);
  for (regi = MIPS_FIRST_EMBED_REGNUM;
       regi <= MIPS_LAST_EMBED_REGNUM;
       regi++)
    regcache_raw_supply (regcache, regi, zerobuf);
}

/* Pack our registers (or one register) into a 64-bit elf_gregset_t.  */

void
mips64_fill_gregset (const struct regcache *regcache,
		     mips64_elf_gregset_t *gregsetp, int regno)
{
  int regaddr, regi;
  mips64_elf_greg_t *regp = *gregsetp;
  void *src, *dst;

  if (regno == -1)
    {
      memset (regp, 0, sizeof (mips64_elf_gregset_t));
      for (regi = 1; regi < 32; regi++)
        mips64_fill_gregset (regcache, gregsetp, regi);
      mips64_fill_gregset (regcache, gregsetp,
			   mips_regnum (current_gdbarch)->lo);
      mips64_fill_gregset (regcache, gregsetp,
			   mips_regnum (current_gdbarch)->hi);
      mips64_fill_gregset (regcache, gregsetp,
			   mips_regnum (current_gdbarch)->pc);
      mips64_fill_gregset (regcache, gregsetp,
			   mips_regnum (current_gdbarch)->badvaddr);
      mips64_fill_gregset (regcache, gregsetp, MIPS_PS_REGNUM);
      mips64_fill_gregset (regcache, gregsetp,
			   mips_regnum (current_gdbarch)->cause);
      mips64_fill_gregset (regcache, gregsetp, MIPS_RESTART_REGNUM);
      return;
   }

  if (regno > 0 && regno < 32)
    regaddr = regno + MIPS64_EF_REG0;
  else if (regno == mips_regnum (current_gdbarch)->lo)
    regaddr = MIPS64_EF_LO;
  else if (regno == mips_regnum (current_gdbarch)->hi)
    regaddr = MIPS64_EF_HI;
  else if (regno == mips_regnum (current_gdbarch)->pc)
    regaddr = MIPS64_EF_CP0_EPC;
  else if (regno == mips_regnum (current_gdbarch)->badvaddr)
    regaddr = MIPS64_EF_CP0_BADVADDR;
  else if (regno == MIPS_PS_REGNUM)
    regaddr = MIPS64_EF_CP0_STATUS;
  else if (regno == mips_regnum (current_gdbarch)->cause)
    regaddr = MIPS64_EF_CP0_CAUSE;
  else if (mips_linux_restart_reg_p (current_gdbarch)
	   && regno == MIPS_RESTART_REGNUM)
    regaddr = MIPS64_EF_REG0;
  else
    regaddr = -1;

  if (regaddr != -1)
    {
      gdb_byte buf[MAX_REGISTER_SIZE];
      LONGEST val;

      regcache_raw_collect (regcache, regno, buf);
      val = extract_signed_integer (buf,
				    register_size (current_gdbarch, regno));
      dst = regp + regaddr;
      store_signed_integer (dst, 8, val);
    }
}

/* Likewise, unpack an elf_fpregset_t.  */

void
mips64_supply_fpregset (struct regcache *regcache,
			const mips64_elf_fpregset_t *fpregsetp)
{
  int regi;

  /* See mips_linux_o32_sigframe_init for a description of the
     peculiar FP register layout.  */
  if (register_size (current_gdbarch,
		     gdbarch_fp0_regnum (current_gdbarch)) == 4)
    for (regi = 0; regi < 32; regi++)
      {
	const gdb_byte *reg_ptr = (const gdb_byte *)(*fpregsetp + (regi & ~1));
	if ((gdbarch_byte_order (current_gdbarch)
	    == BFD_ENDIAN_BIG) != (regi & 1))
	  reg_ptr += 4;
	regcache_raw_supply (regcache,
			     gdbarch_fp0_regnum (current_gdbarch) + regi,
			     reg_ptr);
      }
  else
    for (regi = 0; regi < 32; regi++)
      regcache_raw_supply (regcache,
			   gdbarch_fp0_regnum (current_gdbarch) + regi,
			   (const char *)(*fpregsetp + regi));

  supply_32bit_reg (regcache, mips_regnum (current_gdbarch)->fp_control_status,
		    (const gdb_byte *)(*fpregsetp + 32));

  /* The ABI doesn't tell us how to supply FCRIR, and core dumps don't
     include it - but the result of PTRACE_GETFPREGS does.  The best we
     can do is to assume that its value is present.  */
  supply_32bit_reg (regcache,
		    mips_regnum (current_gdbarch)->fp_implementation_revision,
		    (const gdb_byte *)(*fpregsetp + 32) + 4);
}

/* Likewise, pack one or all floating point registers into an
   elf_fpregset_t.  */

void
mips64_fill_fpregset (const struct regcache *regcache,
		      mips64_elf_fpregset_t *fpregsetp, int regno)
{
  gdb_byte *to;

  if ((regno >= gdbarch_fp0_regnum (current_gdbarch))
      && (regno < gdbarch_fp0_regnum (current_gdbarch) + 32))
    {
      /* See mips_linux_o32_sigframe_init for a description of the
	 peculiar FP register layout.  */
      if (register_size (current_gdbarch, regno) == 4)
	{
	  int regi = regno - gdbarch_fp0_regnum (current_gdbarch);

	  to = (gdb_byte *) (*fpregsetp + (regi & ~1));
	  if ((gdbarch_byte_order (current_gdbarch)
	      == BFD_ENDIAN_BIG) != (regi & 1))
	    to += 4;
	  regcache_raw_collect (regcache, regno, to);
	}
      else
	{
	  to = (gdb_byte *) (*fpregsetp + regno
			     - gdbarch_fp0_regnum (current_gdbarch));
	  regcache_raw_collect (regcache, regno, to);
	}
    }
  else if (regno == mips_regnum (current_gdbarch)->fp_control_status)
    {
      gdb_byte buf[MAX_REGISTER_SIZE];
      LONGEST val;

      regcache_raw_collect (regcache, regno, buf);
      val = extract_signed_integer (buf,
				    register_size (current_gdbarch, regno));
      to = (gdb_byte *) (*fpregsetp + 32);
      store_signed_integer (to, 4, val);
    }
  else if (regno == mips_regnum (current_gdbarch)->fp_implementation_revision)
    {
      gdb_byte buf[MAX_REGISTER_SIZE];
      LONGEST val;

      regcache_raw_collect (regcache, regno, buf);
      val = extract_signed_integer (buf,
				    register_size (current_gdbarch, regno));
      to = (gdb_byte *) (*fpregsetp + 32) + 4;
      store_signed_integer (to, 4, val);
    }
  else if (regno == -1)
    {
      int regi;

      for (regi = 0; regi < 32; regi++)
	mips64_fill_fpregset (regcache, fpregsetp,
			      gdbarch_fp0_regnum (current_gdbarch) + regi);
      mips64_fill_fpregset (regcache, fpregsetp,
			    mips_regnum (current_gdbarch)->fp_control_status);
      mips64_fill_fpregset (regcache, fpregsetp,
			    (mips_regnum (current_gdbarch)
			     ->fp_implementation_revision));
    }
}


/*  Use a local version of this function to get the correct types for
    regsets, until multi-arch core support is ready.  */

static void
fetch_core_registers (struct regcache *regcache,
		      char *core_reg_sect, unsigned core_reg_size,
		      int which, CORE_ADDR reg_addr)
{
  mips_elf_gregset_t gregset;
  mips_elf_fpregset_t fpregset;
  mips64_elf_gregset_t gregset64;
  mips64_elf_fpregset_t fpregset64;

  if (which == 0)
    {
      if (core_reg_size == sizeof (gregset))
	{
	  memcpy ((char *) &gregset, core_reg_sect, sizeof (gregset));
	  mips_supply_gregset (regcache,
			       (const mips_elf_gregset_t *) &gregset);
	}
      else if (core_reg_size == sizeof (gregset64))
	{
	  memcpy ((char *) &gregset64, core_reg_sect, sizeof (gregset64));
	  mips64_supply_gregset (regcache,
				 (const mips64_elf_gregset_t *) &gregset64);
	}
      else
	{
	  warning (_("wrong size gregset struct in core file"));
	}
    }
  else if (which == 2)
    {
      if (core_reg_size == sizeof (fpregset))
	{
	  memcpy ((char *) &fpregset, core_reg_sect, sizeof (fpregset));
	  mips_supply_fpregset (regcache,
				(const mips_elf_fpregset_t *) &fpregset);
	}
      else if (core_reg_size == sizeof (fpregset64))
	{
	  memcpy ((char *) &fpregset64, core_reg_sect,
		  sizeof (fpregset64));
	  mips64_supply_fpregset (regcache,
				  (const mips64_elf_fpregset_t *) &fpregset64);
	}
      else
	{
	  warning (_("wrong size fpregset struct in core file"));
	}
    }
}

/* Register that we are able to handle ELF file formats using standard
   procfs "regset" structures.  */

static struct core_fns regset_core_fns =
{
  bfd_target_elf_flavour,		/* core_flavour */
  default_check_format,			/* check_format */
  default_core_sniffer,			/* core_sniffer */
  fetch_core_registers,			/* core_read_registers */
  NULL					/* next */
};


/* Check the code at PC for a dynamic linker lazy resolution stub.
   Because they aren't in the .plt section, we pattern-match on the
   code generated by GNU ld.  They look like this:

   lw t9,0x8010(gp)
   addu t7,ra
   jalr t9,ra
   addiu t8,zero,INDEX

   (with the appropriate doubleword instructions for N64).  Also
   return the dynamic symbol index used in the last instruction.  */

static int
mips_linux_in_dynsym_stub (CORE_ADDR pc, char *name)
{
  unsigned char buf[28], *p;
  ULONGEST insn, insn1;
  int n64 = (mips_abi (current_gdbarch) == MIPS_ABI_N64);

  read_memory (pc - 12, buf, 28);

  if (n64)
    {
      /* ld t9,0x8010(gp) */
      insn1 = 0xdf998010;
    }
  else
    {
      /* lw t9,0x8010(gp) */
      insn1 = 0x8f998010;
    }

  p = buf + 12;
  while (p >= buf)
    {
      insn = extract_unsigned_integer (p, 4);
      if (insn == insn1)
	break;
      p -= 4;
    }
  if (p < buf)
    return 0;

  insn = extract_unsigned_integer (p + 4, 4);
  if (n64)
    {
      /* daddu t7,ra */
      if (insn != 0x03e0782d)
	return 0;
    }
  else
    {
      /* addu t7,ra */
      if (insn != 0x03e07821)
	return 0;
    }

  insn = extract_unsigned_integer (p + 8, 4);
  /* jalr t9,ra */
  if (insn != 0x0320f809)
    return 0;

  insn = extract_unsigned_integer (p + 12, 4);
  if (n64)
    {
      /* daddiu t8,zero,0 */
      if ((insn & 0xffff0000) != 0x64180000)
	return 0;
    }
  else
    {
      /* addiu t8,zero,0 */
      if ((insn & 0xffff0000) != 0x24180000)
	return 0;
    }

  return (insn & 0xffff);
}

/* Return non-zero iff PC belongs to the dynamic linker resolution
   code or to a stub.  */

static int
mips_linux_in_dynsym_resolve_code (CORE_ADDR pc)
{
  /* Check whether PC is in the dynamic linker.  This also checks
     whether it is in the .plt section, which MIPS does not use.  */
  if (svr4_in_dynsym_resolve_code (pc))
    return 1;

  /* Pattern match for the stub.  It would be nice if there were a
     more efficient way to avoid this check.  */
  if (mips_linux_in_dynsym_stub (pc, NULL))
    return 1;

  return 0;
}

/* See the comments for SKIP_SOLIB_RESOLVER at the top of infrun.c,
   and glibc_skip_solib_resolver in glibc-tdep.c.  The normal glibc
   implementation of this triggers at "fixup" from the same objfile as
   "_dl_runtime_resolve"; MIPS GNU/Linux can trigger at
   "__dl_runtime_resolve" directly.  An unresolved PLT entry will
   point to _dl_runtime_resolve, which will first call
   __dl_runtime_resolve, and then pass control to the resolved
   function.  */

static CORE_ADDR
mips_linux_skip_resolver (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  struct minimal_symbol *resolver;

  resolver = lookup_minimal_symbol ("__dl_runtime_resolve", NULL, NULL);

  if (resolver && SYMBOL_VALUE_ADDRESS (resolver) == pc)
    return frame_pc_unwind (get_current_frame ());

  return 0;
}

/* Signal trampoline support.  There are four supported layouts for a
   signal frame: o32 sigframe, o32 rt_sigframe, n32 rt_sigframe, and
   n64 rt_sigframe.  We handle them all independently; not the most
   efficient way, but simplest.  First, declare all the unwinders.  */

static void mips_linux_o32_sigframe_init (const struct tramp_frame *self,
					  struct frame_info *next_frame,
					  struct trad_frame_cache *this_cache,
					  CORE_ADDR func);

static void mips_linux_n32n64_sigframe_init (const struct tramp_frame *self,
					     struct frame_info *next_frame,
					     struct trad_frame_cache *this_cache,
					     CORE_ADDR func);

#define MIPS_NR_LINUX 4000
#define MIPS_NR_N64_LINUX 5000
#define MIPS_NR_N32_LINUX 6000

#define MIPS_NR_sigreturn MIPS_NR_LINUX + 119
#define MIPS_NR_rt_sigreturn MIPS_NR_LINUX + 193
#define MIPS_NR_N64_rt_sigreturn MIPS_NR_N64_LINUX + 211
#define MIPS_NR_N32_rt_sigreturn MIPS_NR_N32_LINUX + 211

#define MIPS_INST_LI_V0_SIGRETURN 0x24020000 + MIPS_NR_sigreturn
#define MIPS_INST_LI_V0_RT_SIGRETURN 0x24020000 + MIPS_NR_rt_sigreturn
#define MIPS_INST_LI_V0_N64_RT_SIGRETURN 0x24020000 + MIPS_NR_N64_rt_sigreturn
#define MIPS_INST_LI_V0_N32_RT_SIGRETURN 0x24020000 + MIPS_NR_N32_rt_sigreturn
#define MIPS_INST_SYSCALL 0x0000000c

static const struct tramp_frame mips_linux_o32_sigframe = {
  SIGTRAMP_FRAME,
  4,
  {
    { MIPS_INST_LI_V0_SIGRETURN, -1 },
    { MIPS_INST_SYSCALL, -1 },
    { TRAMP_SENTINEL_INSN, -1 }
  },
  mips_linux_o32_sigframe_init
};

static const struct tramp_frame mips_linux_o32_rt_sigframe = {
  SIGTRAMP_FRAME,
  4,
  {
    { MIPS_INST_LI_V0_RT_SIGRETURN, -1 },
    { MIPS_INST_SYSCALL, -1 },
    { TRAMP_SENTINEL_INSN, -1 } },
  mips_linux_o32_sigframe_init
};

static const struct tramp_frame mips_linux_n32_rt_sigframe = {
  SIGTRAMP_FRAME,
  4,
  {
    { MIPS_INST_LI_V0_N32_RT_SIGRETURN, -1 },
    { MIPS_INST_SYSCALL, -1 },
    { TRAMP_SENTINEL_INSN, -1 }
  },
  mips_linux_n32n64_sigframe_init
};

static const struct tramp_frame mips_linux_n64_rt_sigframe = {
  SIGTRAMP_FRAME,
  4,
  {
    { MIPS_INST_LI_V0_N64_RT_SIGRETURN, -1 },
    { MIPS_INST_SYSCALL, -1 },
    { TRAMP_SENTINEL_INSN, -1 }
  },
  mips_linux_n32n64_sigframe_init
};

/* *INDENT-OFF* */
/* The unwinder for o32 signal frames.  The legacy structures look
   like this:

   struct sigframe {
     u32 sf_ass[4];            [argument save space for o32]
     u32 sf_code[2];           [signal trampoline]
     struct sigcontext sf_sc;
     sigset_t sf_mask;
   };

   struct sigcontext {
        unsigned int       sc_regmask;          [Unused]
        unsigned int       sc_status;
        unsigned long long sc_pc;
        unsigned long long sc_regs[32];
        unsigned long long sc_fpregs[32];
        unsigned int       sc_ownedfp;
        unsigned int       sc_fpc_csr;
        unsigned int       sc_fpc_eir;          [Unused]
        unsigned int       sc_used_math;
        unsigned int       sc_ssflags;          [Unused]
	[Alignment hole of four bytes]
        unsigned long long sc_mdhi;
        unsigned long long sc_mdlo;

        unsigned int       sc_cause;            [Unused]
        unsigned int       sc_badvaddr;         [Unused]

        unsigned long      sc_sigset[4];        [kernel's sigset_t]
   };

   The RT signal frames look like this:

   struct rt_sigframe {
     u32 rs_ass[4];            [argument save space for o32]
     u32 rs_code[2]            [signal trampoline]
     struct siginfo rs_info;
     struct ucontext rs_uc;
   };

   struct ucontext {
     unsigned long     uc_flags;
     struct ucontext  *uc_link;
     stack_t           uc_stack;
     [Alignment hole of four bytes]
     struct sigcontext uc_mcontext;
     sigset_t          uc_sigmask;
   };  */
/* *INDENT-ON* */

#define SIGFRAME_CODE_OFFSET         (4 * 4)
#define SIGFRAME_SIGCONTEXT_OFFSET   (6 * 4)

#define RTSIGFRAME_SIGINFO_SIZE      128
#define STACK_T_SIZE                 (3 * 4)
#define UCONTEXT_SIGCONTEXT_OFFSET   (2 * 4 + STACK_T_SIZE + 4)
#define RTSIGFRAME_SIGCONTEXT_OFFSET (SIGFRAME_SIGCONTEXT_OFFSET \
				      + RTSIGFRAME_SIGINFO_SIZE \
				      + UCONTEXT_SIGCONTEXT_OFFSET)

#define SIGCONTEXT_PC       (1 * 8)
#define SIGCONTEXT_REGS     (2 * 8)
#define SIGCONTEXT_FPREGS   (34 * 8)
#define SIGCONTEXT_FPCSR    (66 * 8 + 4)
#define SIGCONTEXT_HI       (69 * 8)
#define SIGCONTEXT_LO       (70 * 8)
#define SIGCONTEXT_CAUSE    (71 * 8 + 0)
#define SIGCONTEXT_BADVADDR (71 * 8 + 4)

#define SIGCONTEXT_REG_SIZE 8

static void
mips_linux_o32_sigframe_init (const struct tramp_frame *self,
			      struct frame_info *next_frame,
			      struct trad_frame_cache *this_cache,
			      CORE_ADDR func)
{
  int ireg, reg_position;
  CORE_ADDR sigcontext_base = func - SIGFRAME_CODE_OFFSET;
  const struct mips_regnum *regs = mips_regnum (current_gdbarch);
  CORE_ADDR regs_base;

  if (self == &mips_linux_o32_sigframe)
    sigcontext_base += SIGFRAME_SIGCONTEXT_OFFSET;
  else
    sigcontext_base += RTSIGFRAME_SIGCONTEXT_OFFSET;

  /* I'm not proud of this hack.  Eventually we will have the
     infrastructure to indicate the size of saved registers on a
     per-frame basis, but right now we don't; the kernel saves eight
     bytes but we only want four.  Use regs_base to access any
     64-bit fields.  */
  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
    regs_base = sigcontext_base + 4;
  else
    regs_base = sigcontext_base;

  if (mips_linux_restart_reg_p (current_gdbarch))
    trad_frame_set_reg_addr (this_cache,
			     (MIPS_RESTART_REGNUM
			      + gdbarch_num_regs (current_gdbarch)),
			     regs_base + SIGCONTEXT_REGS);

  for (ireg = 1; ireg < 32; ireg++)
    trad_frame_set_reg_addr (this_cache,
			     ireg + MIPS_ZERO_REGNUM
			       + gdbarch_num_regs (current_gdbarch),
			     regs_base + SIGCONTEXT_REGS
			     + ireg * SIGCONTEXT_REG_SIZE);

  /* The way that floating point registers are saved, unfortunately,
     depends on the architecture the kernel is built for.  For the r3000 and
     tx39, four bytes of each register are at the beginning of each of the
     32 eight byte slots.  For everything else, the registers are saved
     using double precision; only the even-numbered slots are initialized,
     and the high bits are the odd-numbered register.  Assume the latter
     layout, since we can't tell, and it's much more common.  Which bits are
     the "high" bits depends on endianness.  */
  for (ireg = 0; ireg < 32; ireg++)
    if ((gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG) != (ireg & 1))
      trad_frame_set_reg_addr (this_cache,
			       ireg + regs->fp0 +
				 gdbarch_num_regs (current_gdbarch),
			       sigcontext_base + SIGCONTEXT_FPREGS + 4
			       + (ireg & ~1) * SIGCONTEXT_REG_SIZE);
    else
      trad_frame_set_reg_addr (this_cache,
			       ireg + regs->fp0
				 + gdbarch_num_regs (current_gdbarch),
			       sigcontext_base + SIGCONTEXT_FPREGS
			       + (ireg & ~1) * SIGCONTEXT_REG_SIZE);

  trad_frame_set_reg_addr (this_cache,
			   regs->pc + gdbarch_num_regs (current_gdbarch),
			   regs_base + SIGCONTEXT_PC);

  trad_frame_set_reg_addr (this_cache,
			   regs->fp_control_status
			   + gdbarch_num_regs (current_gdbarch),
			   sigcontext_base + SIGCONTEXT_FPCSR);
  trad_frame_set_reg_addr (this_cache,
			   regs->hi + gdbarch_num_regs (current_gdbarch),
			   regs_base + SIGCONTEXT_HI);
  trad_frame_set_reg_addr (this_cache,
			   regs->lo + gdbarch_num_regs (current_gdbarch),
			   regs_base + SIGCONTEXT_LO);
  trad_frame_set_reg_addr (this_cache,
			   regs->cause + gdbarch_num_regs (current_gdbarch),
			   sigcontext_base + SIGCONTEXT_CAUSE);
  trad_frame_set_reg_addr (this_cache,
			   regs->badvaddr + gdbarch_num_regs (current_gdbarch),
			   sigcontext_base + SIGCONTEXT_BADVADDR);

  /* Choice of the bottom of the sigframe is somewhat arbitrary.  */
  trad_frame_set_id (this_cache,
		     frame_id_build (func - SIGFRAME_CODE_OFFSET,
				     func));
}

/* *INDENT-OFF* */
/* For N32/N64 things look different.  There is no non-rt signal frame.

  struct rt_sigframe_n32 {
    u32 rs_ass[4];                  [ argument save space for o32 ]
    u32 rs_code[2];                 [ signal trampoline ]
    struct siginfo rs_info;
    struct ucontextn32 rs_uc;
  };

  struct ucontextn32 {
    u32                 uc_flags;
    s32                 uc_link;
    stack32_t           uc_stack;
    struct sigcontext   uc_mcontext;
    sigset_t            uc_sigmask;   [ mask last for extensibility ]
  };

  struct rt_sigframe_n32 {
    u32 rs_ass[4];                  [ argument save space for o32 ]
    u32 rs_code[2];                 [ signal trampoline ]
    struct siginfo rs_info;
    struct ucontext rs_uc;
  };

  struct ucontext {
    unsigned long     uc_flags;
    struct ucontext  *uc_link;
    stack_t           uc_stack;
    struct sigcontext uc_mcontext;
    sigset_t          uc_sigmask;   [ mask last for extensibility ]
  };

  And the sigcontext is different (this is for both n32 and n64):

  struct sigcontext {
    unsigned long long sc_regs[32];
    unsigned long long sc_fpregs[32];
    unsigned long long sc_mdhi;
    unsigned long long sc_mdlo;
    unsigned long long sc_pc;
    unsigned int       sc_status;
    unsigned int       sc_fpc_csr;
    unsigned int       sc_fpc_eir;
    unsigned int       sc_used_math;
    unsigned int       sc_cause;
    unsigned int       sc_badvaddr;
  };  */
/* *INDENT-ON* */

#define N32_STACK_T_SIZE		STACK_T_SIZE
#define N64_STACK_T_SIZE		(2 * 8 + 4)
#define N32_UCONTEXT_SIGCONTEXT_OFFSET  (2 * 4 + N32_STACK_T_SIZE + 4)
#define N64_UCONTEXT_SIGCONTEXT_OFFSET  (2 * 8 + N64_STACK_T_SIZE + 4)
#define N32_SIGFRAME_SIGCONTEXT_OFFSET	(SIGFRAME_SIGCONTEXT_OFFSET \
					 + RTSIGFRAME_SIGINFO_SIZE \
					 + N32_UCONTEXT_SIGCONTEXT_OFFSET)
#define N64_SIGFRAME_SIGCONTEXT_OFFSET	(SIGFRAME_SIGCONTEXT_OFFSET \
					 + RTSIGFRAME_SIGINFO_SIZE \
					 + N64_UCONTEXT_SIGCONTEXT_OFFSET)

#define N64_SIGCONTEXT_REGS     (0 * 8)
#define N64_SIGCONTEXT_FPREGS   (32 * 8)
#define N64_SIGCONTEXT_HI       (64 * 8)
#define N64_SIGCONTEXT_LO       (65 * 8)
#define N64_SIGCONTEXT_PC       (66 * 8)
#define N64_SIGCONTEXT_FPCSR    (67 * 8 + 1 * 4)
#define N64_SIGCONTEXT_FIR      (67 * 8 + 2 * 4)
#define N64_SIGCONTEXT_CAUSE    (67 * 8 + 4 * 4)
#define N64_SIGCONTEXT_BADVADDR (67 * 8 + 5 * 4)

#define N64_SIGCONTEXT_REG_SIZE 8

static void
mips_linux_n32n64_sigframe_init (const struct tramp_frame *self,
				 struct frame_info *next_frame,
				 struct trad_frame_cache *this_cache,
				 CORE_ADDR func)
{
  int ireg, reg_position;
  CORE_ADDR sigcontext_base = func - SIGFRAME_CODE_OFFSET;
  const struct mips_regnum *regs = mips_regnum (current_gdbarch);

  if (self == &mips_linux_n32_rt_sigframe)
    sigcontext_base += N32_SIGFRAME_SIGCONTEXT_OFFSET;
  else
    sigcontext_base += N64_SIGFRAME_SIGCONTEXT_OFFSET;

  if (mips_linux_restart_reg_p (current_gdbarch))
    trad_frame_set_reg_addr (this_cache,
			     (MIPS_RESTART_REGNUM
			      + gdbarch_num_regs (current_gdbarch)),
			     sigcontext_base + N64_SIGCONTEXT_REGS);

  for (ireg = 1; ireg < 32; ireg++)
    trad_frame_set_reg_addr (this_cache,
			     ireg + MIPS_ZERO_REGNUM
			     + gdbarch_num_regs (current_gdbarch),
			     sigcontext_base + N64_SIGCONTEXT_REGS
			     + ireg * N64_SIGCONTEXT_REG_SIZE);

  for (ireg = 0; ireg < 32; ireg++)
    trad_frame_set_reg_addr (this_cache,
			     ireg + regs->fp0
			     + gdbarch_num_regs (current_gdbarch),
			     sigcontext_base + N64_SIGCONTEXT_FPREGS
			     + ireg * N64_SIGCONTEXT_REG_SIZE);

  trad_frame_set_reg_addr (this_cache,
			   regs->pc + gdbarch_num_regs (current_gdbarch),
			   sigcontext_base + N64_SIGCONTEXT_PC);

  trad_frame_set_reg_addr (this_cache,
			   regs->fp_control_status
			   + gdbarch_num_regs (current_gdbarch),
			   sigcontext_base + N64_SIGCONTEXT_FPCSR);
  trad_frame_set_reg_addr (this_cache,
			   regs->hi + gdbarch_num_regs (current_gdbarch),
			   sigcontext_base + N64_SIGCONTEXT_HI);
  trad_frame_set_reg_addr (this_cache,
			   regs->lo + gdbarch_num_regs (current_gdbarch),
			   sigcontext_base + N64_SIGCONTEXT_LO);
  trad_frame_set_reg_addr (this_cache,
			   regs->cause + gdbarch_num_regs (current_gdbarch),
			   sigcontext_base + N64_SIGCONTEXT_CAUSE);
  trad_frame_set_reg_addr (this_cache,
			   regs->badvaddr + gdbarch_num_regs (current_gdbarch),
			   sigcontext_base + N64_SIGCONTEXT_BADVADDR);

  /* Choice of the bottom of the sigframe is somewhat arbitrary.  */
  trad_frame_set_id (this_cache,
		     frame_id_build (func - SIGFRAME_CODE_OFFSET,
				     func));
}

static void
mips_linux_write_pc (struct regcache *regcache, CORE_ADDR pc)
{
  regcache_cooked_write_unsigned (regcache,
				  gdbarch_pc_regnum (current_gdbarch), pc);

  /* Clear the syscall restart flag.  */
  if (mips_linux_restart_reg_p (current_gdbarch))
    regcache_cooked_write_unsigned (regcache, MIPS_RESTART_REGNUM, 0);
}

/* Return 1 if MIPS_RESTART_REGNUM is usable.  */

int
mips_linux_restart_reg_p (struct gdbarch *gdbarch)
{
  /* If we do not have a target description with registers, then
     MIPS_RESTART_REGNUM will not be included in the register set.  */
  if (!tdesc_has_registers (gdbarch_target_desc (gdbarch)))
    return 0;

  /* If we do, then MIPS_RESTART_REGNUM is safe to check; it will
     either be GPR-sized or missing.  */
  return register_size (gdbarch, MIPS_RESTART_REGNUM) > 0;
}

/* Initialize one of the GNU/Linux OS ABIs.  */

static void
mips_linux_init_abi (struct gdbarch_info info,
		     struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  enum mips_abi abi = mips_abi (gdbarch);
  struct tdesc_arch_data *tdesc_data = (void *) info.tdep_info;

  switch (abi)
    {
      case MIPS_ABI_O32:
	set_gdbarch_get_longjmp_target (gdbarch,
	                                mips_linux_get_longjmp_target);
	set_solib_svr4_fetch_link_map_offsets
	  (gdbarch, svr4_ilp32_fetch_link_map_offsets);
	tramp_frame_prepend_unwinder (gdbarch, &mips_linux_o32_sigframe);
	tramp_frame_prepend_unwinder (gdbarch, &mips_linux_o32_rt_sigframe);
	break;
      case MIPS_ABI_N32:
	set_gdbarch_get_longjmp_target (gdbarch,
	                                mips_linux_get_longjmp_target);
	set_solib_svr4_fetch_link_map_offsets
	  (gdbarch, svr4_ilp32_fetch_link_map_offsets);
	set_gdbarch_long_double_bit (gdbarch, 128);
	/* These floatformats should probably be renamed.  MIPS uses
	   the same 128-bit IEEE floating point format that IA-64 uses,
	   except that the quiet/signalling NaN bit is reversed (GDB
	   does not distinguish between quiet and signalling NaNs).  */
	set_gdbarch_long_double_format (gdbarch, floatformats_ia64_quad);
	tramp_frame_prepend_unwinder (gdbarch, &mips_linux_n32_rt_sigframe);
	break;
      case MIPS_ABI_N64:
	set_gdbarch_get_longjmp_target (gdbarch,
	                                mips64_linux_get_longjmp_target);
	set_solib_svr4_fetch_link_map_offsets
	  (gdbarch, svr4_lp64_fetch_link_map_offsets);
	set_gdbarch_long_double_bit (gdbarch, 128);
	/* These floatformats should probably be renamed.  MIPS uses
	   the same 128-bit IEEE floating point format that IA-64 uses,
	   except that the quiet/signalling NaN bit is reversed (GDB
	   does not distinguish between quiet and signalling NaNs).  */
	set_gdbarch_long_double_format (gdbarch, floatformats_ia64_quad);
	tramp_frame_prepend_unwinder (gdbarch, &mips_linux_n64_rt_sigframe);
	break;
      default:
	internal_error (__FILE__, __LINE__, _("can't handle ABI"));
	break;
    }

  set_gdbarch_skip_trampoline_code (gdbarch, find_solib_trampoline_target);
  set_gdbarch_skip_solib_resolver (gdbarch, mips_linux_skip_resolver);

  set_gdbarch_software_single_step (gdbarch, mips_software_single_step);

  /* Enable TLS support.  */
  set_gdbarch_fetch_tls_load_module_address (gdbarch,
                                             svr4_fetch_objfile_link_map);

  /* Initialize this lazily, to avoid an initialization order
     dependency on solib-svr4.c's _initialize routine.  */
  if (mips_svr4_so_ops.in_dynsym_resolve_code == NULL)
    {
      mips_svr4_so_ops = svr4_so_ops;
      mips_svr4_so_ops.in_dynsym_resolve_code
	= mips_linux_in_dynsym_resolve_code;
    }
  set_solib_ops (gdbarch, &mips_svr4_so_ops);

  set_gdbarch_write_pc (gdbarch, mips_linux_write_pc);

  if (tdesc_data)
    {
      const struct tdesc_feature *feature;

      /* If we have target-described registers, then we can safely
	 reserve a number for MIPS_RESTART_REGNUM (whether it is
	 described or not).  */
      gdb_assert (gdbarch_num_regs (gdbarch) <= MIPS_RESTART_REGNUM);
      set_gdbarch_num_regs (gdbarch, MIPS_RESTART_REGNUM + 1);

      /* If it's present, then assign it to the reserved number.  */
      feature = tdesc_find_feature (info.target_desc,
				    "org.gnu.gdb.mips.linux");
      if (feature != NULL)
	tdesc_numbered_register (feature, tdesc_data, MIPS_RESTART_REGNUM,
				 "restart");
    }
}

void
_initialize_mips_linux_tdep (void)
{
  const struct bfd_arch_info *arch_info;

  for (arch_info = bfd_lookup_arch (bfd_arch_mips, 0);
       arch_info != NULL;
       arch_info = arch_info->next)
    {
      gdbarch_register_osabi (bfd_arch_mips, arch_info->mach,
			      GDB_OSABI_LINUX,
			      mips_linux_init_abi);
    }

  deprecated_add_core_fns (&regset_core_fns);
}
