/* Target-dependent header for the MIPS architecture, for GDB, the GNU Debugger.

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

#ifndef MIPS_TDEP_H
#define MIPS_TDEP_H

struct gdbarch;

/* All the possible MIPS ABIs. */
enum mips_abi
  {
    MIPS_ABI_UNKNOWN = 0,
    MIPS_ABI_N32,
    MIPS_ABI_O32,
    MIPS_ABI_N64,
    MIPS_ABI_O64,
    MIPS_ABI_EABI32,
    MIPS_ABI_EABI64,
    MIPS_ABI_LAST
  };

/* Return the MIPS ABI associated with GDBARCH.  */
enum mips_abi mips_abi (struct gdbarch *gdbarch);

/* Return the MIPS ISA's register size.  Just a short cut to the BFD
   architecture's word size.  */
extern int mips_isa_regsize (struct gdbarch *gdbarch);

/* Return the current index for various MIPS registers.  */
struct mips_regnum
{
  int pc;
  int fp0;
  int fp_implementation_revision;
  int fp_control_status;
  int badvaddr;		/* Bad vaddr for addressing exception.  */
  int cause;		/* Describes last exception.  */
  int hi;		/* Multiply/divide temp.  */
  int lo;		/* ...  */
};
extern const struct mips_regnum *mips_regnum (struct gdbarch *gdbarch);

/* Register numbers of various important registers.  */

enum
{
  MIPS_ZERO_REGNUM = 0,		/* Read-only register, always 0.  */
  MIPS_AT_REGNUM = 1,
  MIPS_V0_REGNUM = 2,		/* Function integer return value.  */
  MIPS_A0_REGNUM = 4,		/* Loc of first arg during a subr call */
  MIPS_T9_REGNUM = 25,		/* Contains address of callee in PIC.  */
  MIPS_SP_REGNUM = 29,
  MIPS_RA_REGNUM = 31,
  MIPS_PS_REGNUM = 32,		/* Contains processor status.  */
  MIPS_EMBED_LO_REGNUM = 33,
  MIPS_EMBED_HI_REGNUM = 34,
  MIPS_EMBED_BADVADDR_REGNUM = 35,
  MIPS_EMBED_CAUSE_REGNUM = 36,
  MIPS_EMBED_PC_REGNUM = 37,
  MIPS_EMBED_FP0_REGNUM = 38,
  MIPS_UNUSED_REGNUM = 73,	/* Never used, FIXME */
  MIPS_FIRST_EMBED_REGNUM = 74,	/* First CP0 register for embedded use.  */
  MIPS_PRID_REGNUM = 89,	/* Processor ID.  */
  MIPS_LAST_EMBED_REGNUM = 89	/* Last one.  */
};

/* Defined in mips-tdep.c and used in remote-mips.c */
extern void deprecated_mips_set_processor_regs_hack (void);

/* Instruction sizes and other useful constants.  */
enum
{
  MIPS_INSN16_SIZE = 2,
  MIPS_INSN32_SIZE = 4,
  /* The number of floating-point or integer registers.  */
  MIPS_NUMREGS = 32
};

/* Single step based on where the current instruction will take us.  */
extern int mips_software_single_step (struct frame_info *frame);

/* Tell if the program counter value in MEMADDR is in a MIPS16
   function.  */
extern int mips_pc_is_mips16 (bfd_vma memaddr);

/* Return the currently configured (or set) saved register size. */
extern unsigned int mips_abi_regsize (struct gdbarch *gdbarch);

#endif /* MIPS_TDEP_H */
