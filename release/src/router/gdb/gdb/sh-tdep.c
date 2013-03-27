/* Target-dependent code for Renesas Super-H, for GDB.

   Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002,
   2003, 2004, 2005, 2007 Free Software Foundation, Inc.

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

/*
   Contributed by Steve Chamberlain
   sac@cygnus.com
 */

#include "defs.h"
#include "frame.h"
#include "frame-base.h"
#include "frame-unwind.h"
#include "dwarf2-frame.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "value.h"
#include "dis-asm.h"
#include "inferior.h"
#include "gdb_string.h"
#include "gdb_assert.h"
#include "arch-utils.h"
#include "floatformat.h"
#include "regcache.h"
#include "doublest.h"
#include "osabi.h"
#include "reggroups.h"

#include "sh-tdep.h"

#include "elf-bfd.h"
#include "solib-svr4.h"

/* sh flags */
#include "elf/sh.h"
/* registers numbers shared with the simulator */
#include "gdb/sim-sh.h"

static void (*sh_show_regs) (struct frame_info *);

#define SH_NUM_REGS 67

struct sh_frame_cache
{
  /* Base address.  */
  CORE_ADDR base;
  LONGEST sp_offset;
  CORE_ADDR pc;

  /* Flag showing that a frame has been created in the prologue code. */
  int uses_fp;

  /* Saved registers.  */
  CORE_ADDR saved_regs[SH_NUM_REGS];
  CORE_ADDR saved_sp;
};

static const char *
sh_sh_register_name (int reg_nr)
{
  static char *register_names[] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "pc", "pr", "gbr", "vbr", "mach", "macl", "sr",
    "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
  };
  if (reg_nr < 0)
    return NULL;
  if (reg_nr >= (sizeof (register_names) / sizeof (*register_names)))
    return NULL;
  return register_names[reg_nr];
}

static const char *
sh_sh3_register_name (int reg_nr)
{
  static char *register_names[] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "pc", "pr", "gbr", "vbr", "mach", "macl", "sr",
    "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "ssr", "spc",
    "r0b0", "r1b0", "r2b0", "r3b0", "r4b0", "r5b0", "r6b0", "r7b0",
    "r0b1", "r1b1", "r2b1", "r3b1", "r4b1", "r5b1", "r6b1", "r7b1"
    "", "", "", "", "", "", "", "",
  };
  if (reg_nr < 0)
    return NULL;
  if (reg_nr >= (sizeof (register_names) / sizeof (*register_names)))
    return NULL;
  return register_names[reg_nr];
}

static const char *
sh_sh3e_register_name (int reg_nr)
{
  static char *register_names[] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "pc", "pr", "gbr", "vbr", "mach", "macl", "sr",
    "fpul", "fpscr",
    "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7",
    "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15",
    "ssr", "spc",
    "r0b0", "r1b0", "r2b0", "r3b0", "r4b0", "r5b0", "r6b0", "r7b0",
    "r0b1", "r1b1", "r2b1", "r3b1", "r4b1", "r5b1", "r6b1", "r7b1",
    "", "", "", "", "", "", "", "",
  };
  if (reg_nr < 0)
    return NULL;
  if (reg_nr >= (sizeof (register_names) / sizeof (*register_names)))
    return NULL;
  return register_names[reg_nr];
}

static const char *
sh_sh2e_register_name (int reg_nr)
{
  static char *register_names[] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "pc", "pr", "gbr", "vbr", "mach", "macl", "sr",
    "fpul", "fpscr",
    "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7",
    "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15",
    "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
  };
  if (reg_nr < 0)
    return NULL;
  if (reg_nr >= (sizeof (register_names) / sizeof (*register_names)))
    return NULL;
  return register_names[reg_nr];
}

static const char *
sh_sh2a_register_name (int reg_nr)
{
  static char *register_names[] = {
    /* general registers 0-15 */
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    /* 16 - 22 */
    "pc", "pr", "gbr", "vbr", "mach", "macl", "sr",
    /* 23, 24 */
    "fpul", "fpscr",
    /* floating point registers 25 - 40 */
    "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7",
    "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15",
    /* 41, 42 */
    "", "",
    /* 43 - 62.  Banked registers.  The bank number used is determined by
       the bank register (63). */
    "r0b", "r1b", "r2b", "r3b", "r4b", "r5b", "r6b", "r7b",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b",
    "machb", "ivnb", "prb", "gbrb", "maclb",
    /* 63: register bank number, not a real register but used to
       communicate the register bank currently get/set.  This register
       is hidden to the user, who manipulates it using the pseudo
       register called "bank" (67).  See below.  */
    "",
    /* 64 - 66 */
    "ibcr", "ibnr", "tbr",
    /* 67: register bank number, the user visible pseudo register.  */
    "bank",
    /* double precision (pseudo) 68 - 75 */
    "dr0", "dr2", "dr4", "dr6", "dr8", "dr10", "dr12", "dr14",
  };
  if (reg_nr < 0)
    return NULL;
  if (reg_nr >= (sizeof (register_names) / sizeof (*register_names)))
    return NULL;
  return register_names[reg_nr];
}

static const char *
sh_sh2a_nofpu_register_name (int reg_nr)
{
  static char *register_names[] = {
    /* general registers 0-15 */
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    /* 16 - 22 */
    "pc", "pr", "gbr", "vbr", "mach", "macl", "sr",
    /* 23, 24 */
    "", "",
    /* floating point registers 25 - 40 */
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    /* 41, 42 */
    "", "",
    /* 43 - 62.  Banked registers.  The bank number used is determined by
       the bank register (63). */
    "r0b", "r1b", "r2b", "r3b", "r4b", "r5b", "r6b", "r7b",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b",
    "machb", "ivnb", "prb", "gbrb", "maclb",
    /* 63: register bank number, not a real register but used to
       communicate the register bank currently get/set.  This register
       is hidden to the user, who manipulates it using the pseudo
       register called "bank" (67).  See below.  */
    "",
    /* 64 - 66 */
    "ibcr", "ibnr", "tbr",
    /* 67: register bank number, the user visible pseudo register.  */
    "bank",
    /* double precision (pseudo) 68 - 75 */
    "", "", "", "", "", "", "", "",
  };
  if (reg_nr < 0)
    return NULL;
  if (reg_nr >= (sizeof (register_names) / sizeof (*register_names)))
    return NULL;
  return register_names[reg_nr];
}

static const char *
sh_sh_dsp_register_name (int reg_nr)
{
  static char *register_names[] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "pc", "pr", "gbr", "vbr", "mach", "macl", "sr",
    "", "dsr",
    "a0g", "a0", "a1g", "a1", "m0", "m1", "x0", "x1",
    "y0", "y1", "", "", "", "", "", "mod",
    "", "",
    "rs", "re", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
  };
  if (reg_nr < 0)
    return NULL;
  if (reg_nr >= (sizeof (register_names) / sizeof (*register_names)))
    return NULL;
  return register_names[reg_nr];
}

static const char *
sh_sh3_dsp_register_name (int reg_nr)
{
  static char *register_names[] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "pc", "pr", "gbr", "vbr", "mach", "macl", "sr",
    "", "dsr",
    "a0g", "a0", "a1g", "a1", "m0", "m1", "x0", "x1",
    "y0", "y1", "", "", "", "", "", "mod",
    "ssr", "spc",
    "rs", "re", "", "", "", "", "", "",
    "r0b", "r1b", "r2b", "r3b", "r4b", "r5b", "r6b", "r7b",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
  };
  if (reg_nr < 0)
    return NULL;
  if (reg_nr >= (sizeof (register_names) / sizeof (*register_names)))
    return NULL;
  return register_names[reg_nr];
}

static const char *
sh_sh4_register_name (int reg_nr)
{
  static char *register_names[] = {
    /* general registers 0-15 */
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    /* 16 - 22 */
    "pc", "pr", "gbr", "vbr", "mach", "macl", "sr",
    /* 23, 24 */
    "fpul", "fpscr",
    /* floating point registers 25 - 40 */
    "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7",
    "fr8", "fr9", "fr10", "fr11", "fr12", "fr13", "fr14", "fr15",
    /* 41, 42 */
    "ssr", "spc",
    /* bank 0 43 - 50 */
    "r0b0", "r1b0", "r2b0", "r3b0", "r4b0", "r5b0", "r6b0", "r7b0",
    /* bank 1 51 - 58 */
    "r0b1", "r1b1", "r2b1", "r3b1", "r4b1", "r5b1", "r6b1", "r7b1",
    "", "", "", "", "", "", "", "",
    /* pseudo bank register. */
    "",
    /* double precision (pseudo) 59 - 66 */
    "dr0", "dr2", "dr4", "dr6", "dr8", "dr10", "dr12", "dr14",
    /* vectors (pseudo) 67 - 70 */
    "fv0", "fv4", "fv8", "fv12",
    /* FIXME: missing XF 71 - 86 */
    /* FIXME: missing XD 87 - 94 */
  };
  if (reg_nr < 0)
    return NULL;
  if (reg_nr >= (sizeof (register_names) / sizeof (*register_names)))
    return NULL;
  return register_names[reg_nr];
}

static const char *
sh_sh4_nofpu_register_name (int reg_nr)
{
  static char *register_names[] = {
    /* general registers 0-15 */
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    /* 16 - 22 */
    "pc", "pr", "gbr", "vbr", "mach", "macl", "sr",
    /* 23, 24 */
    "", "",
    /* floating point registers 25 - 40 -- not for nofpu target */
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    /* 41, 42 */
    "ssr", "spc",
    /* bank 0 43 - 50 */
    "r0b0", "r1b0", "r2b0", "r3b0", "r4b0", "r5b0", "r6b0", "r7b0",
    /* bank 1 51 - 58 */
    "r0b1", "r1b1", "r2b1", "r3b1", "r4b1", "r5b1", "r6b1", "r7b1",
    "", "", "", "", "", "", "", "",
    /* pseudo bank register. */
    "",
    /* double precision (pseudo) 59 - 66 -- not for nofpu target */
    "", "", "", "", "", "", "", "",
    /* vectors (pseudo) 67 - 70 -- not for nofpu target */
    "", "", "", "",
  };
  if (reg_nr < 0)
    return NULL;
  if (reg_nr >= (sizeof (register_names) / sizeof (*register_names)))
    return NULL;
  return register_names[reg_nr];
}

static const char *
sh_sh4al_dsp_register_name (int reg_nr)
{
  static char *register_names[] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "pc", "pr", "gbr", "vbr", "mach", "macl", "sr",
    "", "dsr",
    "a0g", "a0", "a1g", "a1", "m0", "m1", "x0", "x1",
    "y0", "y1", "", "", "", "", "", "mod",
    "ssr", "spc",
    "rs", "re", "", "", "", "", "", "",
    "r0b", "r1b", "r2b", "r3b", "r4b", "r5b", "r6b", "r7b",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
  };
  if (reg_nr < 0)
    return NULL;
  if (reg_nr >= (sizeof (register_names) / sizeof (*register_names)))
    return NULL;
  return register_names[reg_nr];
}

static const unsigned char *
sh_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  /* 0xc3c3 is trapa #c3, and it works in big and little endian modes */
  static unsigned char breakpoint[] = { 0xc3, 0xc3 };

  /* For remote stub targets, trapa #20 is used.  */
  if (strcmp (target_shortname, "remote") == 0)
    {
      static unsigned char big_remote_breakpoint[] = { 0xc3, 0x20 };
      static unsigned char little_remote_breakpoint[] = { 0x20, 0xc3 };

      if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
	{
	  *lenptr = sizeof (big_remote_breakpoint);
	  return big_remote_breakpoint;
	}
      else
	{
	  *lenptr = sizeof (little_remote_breakpoint);
	  return little_remote_breakpoint;
	}
    }

  *lenptr = sizeof (breakpoint);
  return breakpoint;
}

/* Prologue looks like
   mov.l	r14,@-r15
   sts.l	pr,@-r15
   mov.l	<regs>,@-r15
   sub		<room_for_loca_vars>,r15
   mov		r15,r14

   Actually it can be more complicated than this but that's it, basically.
 */

#define GET_SOURCE_REG(x)  	(((x) >> 4) & 0xf)
#define GET_TARGET_REG(x)  	(((x) >> 8) & 0xf)

/* JSR @Rm         0100mmmm00001011 */
#define IS_JSR(x)		(((x) & 0xf0ff) == 0x400b)

/* STS.L PR,@-r15  0100111100100010
   r15-4-->r15, PR-->(r15) */
#define IS_STS(x)  		((x) == 0x4f22)

/* STS.L MACL,@-r15  0100111100010010
   r15-4-->r15, MACL-->(r15) */
#define IS_MACL_STS(x)  	((x) == 0x4f12)

/* MOV.L Rm,@-r15  00101111mmmm0110
   r15-4-->r15, Rm-->(R15) */
#define IS_PUSH(x) 		(((x) & 0xff0f) == 0x2f06)

/* MOV r15,r14     0110111011110011
   r15-->r14  */
#define IS_MOV_SP_FP(x)  	((x) == 0x6ef3)

/* ADD #imm,r15    01111111iiiiiiii
   r15+imm-->r15 */
#define IS_ADD_IMM_SP(x) 	(((x) & 0xff00) == 0x7f00)

#define IS_MOV_R3(x) 		(((x) & 0xff00) == 0x1a00)
#define IS_SHLL_R3(x)		((x) == 0x4300)

/* ADD r3,r15      0011111100111100
   r15+r3-->r15 */
#define IS_ADD_R3SP(x)		((x) == 0x3f3c)

/* FMOV.S FRm,@-Rn  Rn-4-->Rn, FRm-->(Rn)     1111nnnnmmmm1011
   FMOV DRm,@-Rn    Rn-8-->Rn, DRm-->(Rn)     1111nnnnmmm01011
   FMOV XDm,@-Rn    Rn-8-->Rn, XDm-->(Rn)     1111nnnnmmm11011 */
/* CV, 2003-08-28: Only suitable with Rn == SP, therefore name changed to
		   make this entirely clear. */
/* #define IS_FMOV(x)		(((x) & 0xf00f) == 0xf00b) */
#define IS_FPUSH(x)		(((x) & 0xff0f) == 0xff0b)

/* MOV Rm,Rn          Rm-->Rn        0110nnnnmmmm0011  4 <= m <= 7 */
#define IS_MOV_ARG_TO_REG(x) \
	(((x) & 0xf00f) == 0x6003 && \
	 ((x) & 0x00f0) >= 0x0040 && \
	 ((x) & 0x00f0) <= 0x0070)
/* MOV.L Rm,@Rn               0010nnnnmmmm0010  n = 14, 4 <= m <= 7 */
#define IS_MOV_ARG_TO_IND_R14(x) \
	(((x) & 0xff0f) == 0x2e02 && \
	 ((x) & 0x00f0) >= 0x0040 && \
	 ((x) & 0x00f0) <= 0x0070)
/* MOV.L Rm,@(disp*4,Rn)      00011110mmmmdddd  n = 14, 4 <= m <= 7 */
#define IS_MOV_ARG_TO_IND_R14_WITH_DISP(x) \
	(((x) & 0xff00) == 0x1e00 && \
	 ((x) & 0x00f0) >= 0x0040 && \
	 ((x) & 0x00f0) <= 0x0070)

/* MOV.W @(disp*2,PC),Rn      1001nnnndddddddd */
#define IS_MOVW_PCREL_TO_REG(x)	(((x) & 0xf000) == 0x9000)
/* MOV.L @(disp*4,PC),Rn      1101nnnndddddddd */
#define IS_MOVL_PCREL_TO_REG(x)	(((x) & 0xf000) == 0xd000)
/* MOVI20 #imm20,Rn           0000nnnniiii0000 */
#define IS_MOVI20(x)		(((x) & 0xf00f) == 0x0000)
/* SUB Rn,R15                 00111111nnnn1000 */
#define IS_SUB_REG_FROM_SP(x)	(((x) & 0xff0f) == 0x3f08)

#define FPSCR_SZ		(1 << 20)

/* The following instructions are used for epilogue testing. */
#define IS_RESTORE_FP(x)	((x) == 0x6ef6)
#define IS_RTS(x)		((x) == 0x000b)
#define IS_LDS(x)  		((x) == 0x4f26)
#define IS_MACL_LDS(x)  	((x) == 0x4f16)
#define IS_MOV_FP_SP(x)  	((x) == 0x6fe3)
#define IS_ADD_REG_TO_FP(x)	(((x) & 0xff0f) == 0x3e0c)
#define IS_ADD_IMM_FP(x) 	(((x) & 0xff00) == 0x7e00)

/* Disassemble an instruction.  */
static int
gdb_print_insn_sh (bfd_vma memaddr, disassemble_info * info)
{
  info->endian = gdbarch_byte_order (current_gdbarch);
  return print_insn_sh (memaddr, info);
}

static CORE_ADDR
sh_analyze_prologue (CORE_ADDR pc, CORE_ADDR current_pc,
		     struct sh_frame_cache *cache, ULONGEST fpscr)
{
  ULONGEST inst;
  CORE_ADDR opc;
  int offset;
  int sav_offset = 0;
  int r3_val = 0;
  int reg, sav_reg = -1;

  if (pc >= current_pc)
    return current_pc;

  cache->uses_fp = 0;
  for (opc = pc + (2 * 28); pc < opc; pc += 2)
    {
      inst = read_memory_unsigned_integer (pc, 2);
      /* See where the registers will be saved to */
      if (IS_PUSH (inst))
	{
	  cache->saved_regs[GET_SOURCE_REG (inst)] = cache->sp_offset;
	  cache->sp_offset += 4;
	}
      else if (IS_STS (inst))
	{
	  cache->saved_regs[PR_REGNUM] = cache->sp_offset;
	  cache->sp_offset += 4;
	}
      else if (IS_MACL_STS (inst))
	{
	  cache->saved_regs[MACL_REGNUM] = cache->sp_offset;
	  cache->sp_offset += 4;
	}
      else if (IS_MOV_R3 (inst))
	{
	  r3_val = ((inst & 0xff) ^ 0x80) - 0x80;
	}
      else if (IS_SHLL_R3 (inst))
	{
	  r3_val <<= 1;
	}
      else if (IS_ADD_R3SP (inst))
	{
	  cache->sp_offset += -r3_val;
	}
      else if (IS_ADD_IMM_SP (inst))
	{
	  offset = ((inst & 0xff) ^ 0x80) - 0x80;
	  cache->sp_offset -= offset;
	}
      else if (IS_MOVW_PCREL_TO_REG (inst))
	{
	  if (sav_reg < 0)
	    {
	      reg = GET_TARGET_REG (inst);
	      if (reg < 14)
		{
		  sav_reg = reg;
		  offset = (inst & 0xff) << 1;
		  sav_offset =
		    read_memory_integer ((pc + 4) + offset, 2);
		}
	    }
	}
      else if (IS_MOVL_PCREL_TO_REG (inst))
	{
	  if (sav_reg < 0)
	    {
	      reg = GET_TARGET_REG (inst);
	      if (reg < 14)
		{
		  sav_reg = reg;
		  offset = (inst & 0xff) << 2;
		  sav_offset =
		    read_memory_integer (((pc & 0xfffffffc) + 4) + offset, 4);
		}
	    }
	}
      else if (IS_MOVI20 (inst))
        {
	  if (sav_reg < 0)
	    {
	      reg = GET_TARGET_REG (inst);
	      if (reg < 14)
	        {
		  sav_reg = reg;
		  sav_offset = GET_SOURCE_REG (inst) << 16;
		  /* MOVI20 is a 32 bit instruction! */
		  pc += 2;
		  sav_offset |= read_memory_unsigned_integer (pc, 2);
		  /* Now sav_offset contains an unsigned 20 bit value.
		     It must still get sign extended.  */
		  if (sav_offset & 0x00080000)
		    sav_offset |= 0xfff00000;
		}
	    }
	}
      else if (IS_SUB_REG_FROM_SP (inst))
	{
	  reg = GET_SOURCE_REG (inst);
	  if (sav_reg > 0 && reg == sav_reg)
	    {
	      sav_reg = -1;
	    }
	  cache->sp_offset += sav_offset;
	}
      else if (IS_FPUSH (inst))
	{
	  if (fpscr & FPSCR_SZ)
	    {
	      cache->sp_offset += 8;
	    }
	  else
	    {
	      cache->sp_offset += 4;
	    }
	}
      else if (IS_MOV_SP_FP (inst))
	{
	  cache->uses_fp = 1;
	  /* At this point, only allow argument register moves to other
	     registers or argument register moves to @(X,fp) which are
	     moving the register arguments onto the stack area allocated
	     by a former add somenumber to SP call.  Don't allow moving
	     to an fp indirect address above fp + cache->sp_offset. */
	  pc += 2;
	  for (opc = pc + 12; pc < opc; pc += 2)
	    {
	      inst = read_memory_integer (pc, 2);
	      if (IS_MOV_ARG_TO_IND_R14 (inst))
		{
		  reg = GET_SOURCE_REG (inst);
		  if (cache->sp_offset > 0)
		    cache->saved_regs[reg] = cache->sp_offset;
		}
	      else if (IS_MOV_ARG_TO_IND_R14_WITH_DISP (inst))
		{
		  reg = GET_SOURCE_REG (inst);
		  offset = (inst & 0xf) * 4;
		  if (cache->sp_offset > offset)
		    cache->saved_regs[reg] = cache->sp_offset - offset;
		}
	      else if (IS_MOV_ARG_TO_REG (inst))
		continue;
	      else
		break;
	    }
	  break;
	}
      else if (IS_JSR (inst))
	{
	  /* We have found a jsr that has been scheduled into the prologue.
	     If we continue the scan and return a pc someplace after this,
	     then setting a breakpoint on this function will cause it to
	     appear to be called after the function it is calling via the
	     jsr, which will be very confusing.  Most likely the next
	     instruction is going to be IS_MOV_SP_FP in the delay slot.  If
	     so, note that before returning the current pc. */
	  inst = read_memory_integer (pc + 2, 2);
	  if (IS_MOV_SP_FP (inst))
	    cache->uses_fp = 1;
	  break;
	}
#if 0				/* This used to just stop when it found an instruction that
				   was not considered part of the prologue.  Now, we just
				   keep going looking for likely instructions. */
      else
	break;
#endif
    }

  return pc;
}

/* Skip any prologue before the guts of a function */

/* Skip the prologue using the debug information. If this fails we'll
   fall back on the 'guess' method below. */
static CORE_ADDR
after_prologue (CORE_ADDR pc)
{
  struct symtab_and_line sal;
  CORE_ADDR func_addr, func_end;

  /* If we can not find the symbol in the partial symbol table, then
     there is no hope we can determine the function's start address
     with this code.  */
  if (!find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    return 0;

  /* Get the line associated with FUNC_ADDR.  */
  sal = find_pc_line (func_addr, 0);

  /* There are only two cases to consider.  First, the end of the source line
     is within the function bounds.  In that case we return the end of the
     source line.  Second is the end of the source line extends beyond the
     bounds of the current function.  We need to use the slow code to
     examine instructions in that case.  */
  if (sal.end < func_end)
    return sal.end;
  else
    return 0;
}

static CORE_ADDR
sh_skip_prologue (CORE_ADDR start_pc)
{
  CORE_ADDR pc;
  struct sh_frame_cache cache;

  /* See if we can determine the end of the prologue via the symbol table.
     If so, then return either PC, or the PC after the prologue, whichever
     is greater.  */
  pc = after_prologue (start_pc);

  /* If after_prologue returned a useful address, then use it.  Else
     fall back on the instruction skipping code. */
  if (pc)
    return max (pc, start_pc);

  cache.sp_offset = -4;
  pc = sh_analyze_prologue (start_pc, (CORE_ADDR) -1, &cache, 0);
  if (!cache.uses_fp)
    return start_pc;

  return pc;
}

/* The ABI says:

   Aggregate types not bigger than 8 bytes that have the same size and
   alignment as one of the integer scalar types are returned in the
   same registers as the integer type they match.

   For example, a 2-byte aligned structure with size 2 bytes has the
   same size and alignment as a short int, and will be returned in R0.
   A 4-byte aligned structure with size 8 bytes has the same size and
   alignment as a long long int, and will be returned in R0 and R1.

   When an aggregate type is returned in R0 and R1, R0 contains the
   first four bytes of the aggregate, and R1 contains the
   remainder. If the size of the aggregate type is not a multiple of 4
   bytes, the aggregate is tail-padded up to a multiple of 4
   bytes. The value of the padding is undefined. For little-endian
   targets the padding will appear at the most significant end of the
   last element, for big-endian targets the padding appears at the
   least significant end of the last element.

   All other aggregate types are returned by address. The caller
   function passes the address of an area large enough to hold the
   aggregate value in R2. The called function stores the result in
   this location.

   To reiterate, structs smaller than 8 bytes could also be returned
   in memory, if they don't pass the "same size and alignment as an
   integer type" rule.

   For example, in

   struct s { char c[3]; } wibble;
   struct s foo(void) {  return wibble; }

   the return value from foo() will be in memory, not
   in R0, because there is no 3-byte integer type.

   Similarly, in 

   struct s { char c[2]; } wibble;
   struct s foo(void) {  return wibble; }

   because a struct containing two chars has alignment 1, that matches
   type char, but size 2, that matches type short.  There's no integer
   type that has alignment 1 and size 2, so the struct is returned in
   memory.

*/

static int
sh_use_struct_convention (int gcc_p, struct type *type)
{
  int len = TYPE_LENGTH (type);
  int nelem = TYPE_NFIELDS (type);

  /* Non-power of 2 length types and types bigger than 8 bytes (which don't
     fit in two registers anyway) use struct convention.  */
  if (len != 1 && len != 2 && len != 4 && len != 8)
    return 1;

  /* Scalar types and aggregate types with exactly one field are aligned
     by definition.  They are returned in registers.  */
  if (nelem <= 1)
    return 0;

  /* If the first field in the aggregate has the same length as the entire
     aggregate type, the type is returned in registers.  */
  if (TYPE_LENGTH (TYPE_FIELD_TYPE (type, 0)) == len)
    return 0;

  /* If the size of the aggregate is 8 bytes and the first field is
     of size 4 bytes its alignment is equal to long long's alignment,
     so it's returned in registers.  */
  if (len == 8 && TYPE_LENGTH (TYPE_FIELD_TYPE (type, 0)) == 4)
    return 0;

  /* Otherwise use struct convention.  */
  return 1;
}

static CORE_ADDR
sh_frame_align (struct gdbarch *ignore, CORE_ADDR sp)
{
  return sp & ~3;
}

/* Function: push_dummy_call (formerly push_arguments)
   Setup the function arguments for calling a function in the inferior.

   On the Renesas SH architecture, there are four registers (R4 to R7)
   which are dedicated for passing function arguments.  Up to the first
   four arguments (depending on size) may go into these registers.
   The rest go on the stack.

   MVS: Except on SH variants that have floating point registers.
   In that case, float and double arguments are passed in the same
   manner, but using FP registers instead of GP registers.

   Arguments that are smaller than 4 bytes will still take up a whole
   register or a whole 32-bit word on the stack, and will be 
   right-justified in the register or the stack word.  This includes
   chars, shorts, and small aggregate types.

   Arguments that are larger than 4 bytes may be split between two or 
   more registers.  If there are not enough registers free, an argument
   may be passed partly in a register (or registers), and partly on the
   stack.  This includes doubles, long longs, and larger aggregates. 
   As far as I know, there is no upper limit to the size of aggregates 
   that will be passed in this way; in other words, the convention of 
   passing a pointer to a large aggregate instead of a copy is not used.

   MVS: The above appears to be true for the SH variants that do not
   have an FPU, however those that have an FPU appear to copy the
   aggregate argument onto the stack (and not place it in registers)
   if it is larger than 16 bytes (four GP registers).

   An exceptional case exists for struct arguments (and possibly other
   aggregates such as arrays) if the size is larger than 4 bytes but 
   not a multiple of 4 bytes.  In this case the argument is never split 
   between the registers and the stack, but instead is copied in its
   entirety onto the stack, AND also copied into as many registers as 
   there is room for.  In other words, space in registers permitting, 
   two copies of the same argument are passed in.  As far as I can tell,
   only the one on the stack is used, although that may be a function 
   of the level of compiler optimization.  I suspect this is a compiler
   bug.  Arguments of these odd sizes are left-justified within the 
   word (as opposed to arguments smaller than 4 bytes, which are 
   right-justified).

   If the function is to return an aggregate type such as a struct, it 
   is either returned in the normal return value register R0 (if its 
   size is no greater than one byte), or else the caller must allocate
   space into which the callee will copy the return value (if the size
   is greater than one byte).  In this case, a pointer to the return 
   value location is passed into the callee in register R2, which does 
   not displace any of the other arguments passed in via registers R4
   to R7.   */

/* Helper function to justify value in register according to endianess. */
static char *
sh_justify_value_in_reg (struct value *val, int len)
{
  static char valbuf[4];

  memset (valbuf, 0, sizeof (valbuf));
  if (len < 4)
    {
      /* value gets right-justified in the register or stack word */
      if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
	memcpy (valbuf + (4 - len), (char *) value_contents (val), len);
      else
	memcpy (valbuf, (char *) value_contents (val), len);
      return valbuf;
    }
  return (char *) value_contents (val);
}

/* Helper function to eval number of bytes to allocate on stack. */
static CORE_ADDR
sh_stack_allocsize (int nargs, struct value **args)
{
  int stack_alloc = 0;
  while (nargs-- > 0)
    stack_alloc += ((TYPE_LENGTH (value_type (args[nargs])) + 3) & ~3);
  return stack_alloc;
}

/* Helper functions for getting the float arguments right.  Registers usage
   depends on the ABI and the endianess.  The comments should enlighten how
   it's intended to work. */

/* This array stores which of the float arg registers are already in use. */
static int flt_argreg_array[FLOAT_ARGLAST_REGNUM - FLOAT_ARG0_REGNUM + 1];

/* This function just resets the above array to "no reg used so far". */
static void
sh_init_flt_argreg (void)
{
  memset (flt_argreg_array, 0, sizeof flt_argreg_array);
}

/* This function returns the next register to use for float arg passing.
   It returns either a valid value between FLOAT_ARG0_REGNUM and
   FLOAT_ARGLAST_REGNUM if a register is available, otherwise it returns 
   FLOAT_ARGLAST_REGNUM + 1 to indicate that no register is available.

   Note that register number 0 in flt_argreg_array corresponds with the
   real float register fr4.  In contrast to FLOAT_ARG0_REGNUM (value is
   29) the parity of the register number is preserved, which is important
   for the double register passing test (see the "argreg & 1" test below). */
static int
sh_next_flt_argreg (int len)
{
  int argreg;

  /* First search for the next free register. */
  for (argreg = 0; argreg <= FLOAT_ARGLAST_REGNUM - FLOAT_ARG0_REGNUM;
       ++argreg)
    if (!flt_argreg_array[argreg])
      break;

  /* No register left? */
  if (argreg > FLOAT_ARGLAST_REGNUM - FLOAT_ARG0_REGNUM)
    return FLOAT_ARGLAST_REGNUM + 1;

  if (len == 8)
    {
      /* Doubles are always starting in a even register number. */
      if (argreg & 1)
	{
	  flt_argreg_array[argreg] = 1;

	  ++argreg;

	  /* No register left? */
	  if (argreg > FLOAT_ARGLAST_REGNUM - FLOAT_ARG0_REGNUM)
	    return FLOAT_ARGLAST_REGNUM + 1;
	}
      /* Also mark the next register as used. */
      flt_argreg_array[argreg + 1] = 1;
    }
  else if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_LITTLE)
    {
      /* In little endian, gcc passes floats like this: f5, f4, f7, f6, ... */
      if (!flt_argreg_array[argreg + 1])
	++argreg;
    }
  flt_argreg_array[argreg] = 1;
  return FLOAT_ARG0_REGNUM + argreg;
}

/* Helper function which figures out, if a type is treated like a float type.

   The FPU ABIs have a special way how to treat types as float types.
   Structures with exactly one member, which is of type float or double, are
   treated exactly as the base types float or double:

     struct sf {
       float f;
     };

     struct sd {
       double d;
     };

   are handled the same way as just

     float f;

     double d;

   As a result, arguments of these struct types are pushed into floating point
   registers exactly as floats or doubles, using the same decision algorithm.

   The same is valid if these types are used as function return types.  The
   above structs are returned in fr0 resp. fr0,fr1 instead of in r0, r0,r1
   or even using struct convention as it is for other structs.  */

static int
sh_treat_as_flt_p (struct type *type)
{
  int len = TYPE_LENGTH (type);

  /* Ordinary float types are obviously treated as float.  */
  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    return 1;
  /* Otherwise non-struct types are not treated as float.  */
  if (TYPE_CODE (type) != TYPE_CODE_STRUCT)
    return 0;
  /* Otherwise structs with more than one memeber are not treated as float.  */
  if (TYPE_NFIELDS (type) != 1)
    return 0;
  /* Otherwise if the type of that member is float, the whole type is
     treated as float.  */
  if (TYPE_CODE (TYPE_FIELD_TYPE (type, 0)) == TYPE_CODE_FLT)
    return 1;
  /* Otherwise it's not treated as float.  */
  return 0;
}

static CORE_ADDR
sh_push_dummy_call_fpu (struct gdbarch *gdbarch,
			struct value *function,
			struct regcache *regcache,
			CORE_ADDR bp_addr, int nargs,
			struct value **args,
			CORE_ADDR sp, int struct_return,
			CORE_ADDR struct_addr)
{
  int stack_offset = 0;
  int argreg = ARG0_REGNUM;
  int flt_argreg = 0;
  int argnum;
  struct type *type;
  CORE_ADDR regval;
  char *val;
  int len, reg_size = 0;
  int pass_on_stack = 0;
  int treat_as_flt;

  /* first force sp to a 4-byte alignment */
  sp = sh_frame_align (gdbarch, sp);

  if (struct_return)
    regcache_cooked_write_unsigned (regcache,
				    STRUCT_RETURN_REGNUM, struct_addr);

  /* make room on stack for args */
  sp -= sh_stack_allocsize (nargs, args);

  /* Initialize float argument mechanism. */
  sh_init_flt_argreg ();

  /* Now load as many as possible of the first arguments into
     registers, and push the rest onto the stack.  There are 16 bytes
     in four registers available.  Loop thru args from first to last.  */
  for (argnum = 0; argnum < nargs; argnum++)
    {
      type = value_type (args[argnum]);
      len = TYPE_LENGTH (type);
      val = sh_justify_value_in_reg (args[argnum], len);

      /* Some decisions have to be made how various types are handled.
         This also differs in different ABIs. */
      pass_on_stack = 0;

      /* Find out the next register to use for a floating point value. */
      treat_as_flt = sh_treat_as_flt_p (type);
      if (treat_as_flt)
	flt_argreg = sh_next_flt_argreg (len);
      /* In contrast to non-FPU CPUs, arguments are never split between
	 registers and stack.  If an argument doesn't fit in the remaining
	 registers it's always pushed entirely on the stack.  */
      else if (len > ((ARGLAST_REGNUM - argreg + 1) * 4))
	pass_on_stack = 1;

      while (len > 0)
	{
	  if ((treat_as_flt && flt_argreg > FLOAT_ARGLAST_REGNUM)
	      || (!treat_as_flt && (argreg > ARGLAST_REGNUM
	                            || pass_on_stack)))
	    {
	      /* The data goes entirely on the stack, 4-byte aligned. */
	      reg_size = (len + 3) & ~3;
	      write_memory (sp + stack_offset, val, reg_size);
	      stack_offset += reg_size;
	    }
	  else if (treat_as_flt && flt_argreg <= FLOAT_ARGLAST_REGNUM)
	    {
	      /* Argument goes in a float argument register.  */
	      reg_size = register_size (gdbarch, flt_argreg);
	      regval = extract_unsigned_integer (val, reg_size);
	      /* In little endian mode, float types taking two registers
	         (doubles on sh4, long doubles on sh2e, sh3e and sh4) must
		 be stored swapped in the argument registers.  The below
		 code first writes the first 32 bits in the next but one
		 register, increments the val and len values accordingly
		 and then proceeds as normal by writing the second 32 bits
		 into the next register. */
	      if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_LITTLE
	          && TYPE_LENGTH (type) == 2 * reg_size)
	        {
		  regcache_cooked_write_unsigned (regcache, flt_argreg + 1,
						  regval);
		  val += reg_size;
		  len -= reg_size;
		  regval = extract_unsigned_integer (val, reg_size);
		}
	      regcache_cooked_write_unsigned (regcache, flt_argreg++, regval);
	    }
	  else if (!treat_as_flt && argreg <= ARGLAST_REGNUM)
	    {
	      /* there's room in a register */
	      reg_size = register_size (gdbarch, argreg);
	      regval = extract_unsigned_integer (val, reg_size);
	      regcache_cooked_write_unsigned (regcache, argreg++, regval);
	    }
	  /* Store the value one register at a time or in one step on stack.  */
	  len -= reg_size;
	  val += reg_size;
	}
    }

  /* Store return address. */
  regcache_cooked_write_unsigned (regcache, PR_REGNUM, bp_addr);

  /* Update stack pointer.  */
  regcache_cooked_write_unsigned (regcache,
				  gdbarch_sp_regnum (current_gdbarch), sp);

  return sp;
}

static CORE_ADDR
sh_push_dummy_call_nofpu (struct gdbarch *gdbarch,
			  struct value *function,
			  struct regcache *regcache,
			  CORE_ADDR bp_addr,
			  int nargs, struct value **args,
			  CORE_ADDR sp, int struct_return,
			  CORE_ADDR struct_addr)
{
  int stack_offset = 0;
  int argreg = ARG0_REGNUM;
  int argnum;
  struct type *type;
  CORE_ADDR regval;
  char *val;
  int len, reg_size;

  /* first force sp to a 4-byte alignment */
  sp = sh_frame_align (gdbarch, sp);

  if (struct_return)
    regcache_cooked_write_unsigned (regcache,
				    STRUCT_RETURN_REGNUM, struct_addr);

  /* make room on stack for args */
  sp -= sh_stack_allocsize (nargs, args);

  /* Now load as many as possible of the first arguments into
     registers, and push the rest onto the stack.  There are 16 bytes
     in four registers available.  Loop thru args from first to last.  */
  for (argnum = 0; argnum < nargs; argnum++)
    {
      type = value_type (args[argnum]);
      len = TYPE_LENGTH (type);
      val = sh_justify_value_in_reg (args[argnum], len);

      while (len > 0)
	{
	  if (argreg > ARGLAST_REGNUM)
	    {
	      /* The remainder of the data goes entirely on the stack,
	         4-byte aligned. */
	      reg_size = (len + 3) & ~3;
	      write_memory (sp + stack_offset, val, reg_size);
	      stack_offset += reg_size;
	    }
	  else if (argreg <= ARGLAST_REGNUM)
	    {
	      /* there's room in a register */
	      reg_size = register_size (gdbarch, argreg);
	      regval = extract_unsigned_integer (val, reg_size);
	      regcache_cooked_write_unsigned (regcache, argreg++, regval);
	    }
	  /* Store the value reg_size bytes at a time.  This means that things
	     larger than reg_size bytes may go partly in registers and partly
	     on the stack.  */
	  len -= reg_size;
	  val += reg_size;
	}
    }

  /* Store return address. */
  regcache_cooked_write_unsigned (regcache, PR_REGNUM, bp_addr);

  /* Update stack pointer.  */
  regcache_cooked_write_unsigned (regcache,
				  gdbarch_sp_regnum (current_gdbarch), sp);

  return sp;
}

/* Find a function's return value in the appropriate registers (in
   regbuf), and copy it into valbuf.  Extract from an array REGBUF
   containing the (raw) register state a function return value of type
   TYPE, and copy that, in virtual format, into VALBUF.  */
static void
sh_extract_return_value_nofpu (struct type *type, struct regcache *regcache,
			       void *valbuf)
{
  int len = TYPE_LENGTH (type);
  int return_register = R0_REGNUM;
  int offset;

  if (len <= 4)
    {
      ULONGEST c;

      regcache_cooked_read_unsigned (regcache, R0_REGNUM, &c);
      store_unsigned_integer (valbuf, len, c);
    }
  else if (len == 8)
    {
      int i, regnum = R0_REGNUM;
      for (i = 0; i < len; i += 4)
	regcache_raw_read (regcache, regnum++, (char *) valbuf + i);
    }
  else
    error (_("bad size for return value"));
}

static void
sh_extract_return_value_fpu (struct type *type, struct regcache *regcache,
			     void *valbuf)
{
  if (sh_treat_as_flt_p (type))
    {
      int len = TYPE_LENGTH (type);
      int i, regnum = gdbarch_fp0_regnum (current_gdbarch);
      for (i = 0; i < len; i += 4)
	if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_LITTLE)
	  regcache_raw_read (regcache, regnum++, (char *) valbuf + len - 4 - i);
	else
	  regcache_raw_read (regcache, regnum++, (char *) valbuf + i);
    }
  else
    sh_extract_return_value_nofpu (type, regcache, valbuf);
}

/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format.
   If the architecture is sh4 or sh3e, store a function's return value
   in the R0 general register or in the FP0 floating point register,
   depending on the type of the return value. In all the other cases
   the result is stored in r0, left-justified. */
static void
sh_store_return_value_nofpu (struct type *type, struct regcache *regcache,
			     const void *valbuf)
{
  ULONGEST val;
  int len = TYPE_LENGTH (type);

  if (len <= 4)
    {
      val = extract_unsigned_integer (valbuf, len);
      regcache_cooked_write_unsigned (regcache, R0_REGNUM, val);
    }
  else
    {
      int i, regnum = R0_REGNUM;
      for (i = 0; i < len; i += 4)
	regcache_raw_write (regcache, regnum++, (char *) valbuf + i);
    }
}

static void
sh_store_return_value_fpu (struct type *type, struct regcache *regcache,
			   const void *valbuf)
{
  if (sh_treat_as_flt_p (type))
    {
      int len = TYPE_LENGTH (type);
      int i, regnum = gdbarch_fp0_regnum (current_gdbarch);
      for (i = 0; i < len; i += 4)
	if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_LITTLE)
	  regcache_raw_write (regcache, regnum++,
			      (char *) valbuf + len - 4 - i);
	else
	  regcache_raw_write (regcache, regnum++, (char *) valbuf + i);
    }
  else
    sh_store_return_value_nofpu (type, regcache, valbuf);
}

static enum return_value_convention
sh_return_value_nofpu (struct gdbarch *gdbarch, struct type *type,
		       struct regcache *regcache,
		       gdb_byte *readbuf, const gdb_byte *writebuf)
{
  if (sh_use_struct_convention (0, type))
    return RETURN_VALUE_STRUCT_CONVENTION;
  if (writebuf)
    sh_store_return_value_nofpu (type, regcache, writebuf);
  else if (readbuf)
    sh_extract_return_value_nofpu (type, regcache, readbuf);
  return RETURN_VALUE_REGISTER_CONVENTION;
}

static enum return_value_convention
sh_return_value_fpu (struct gdbarch *gdbarch, struct type *type,
		     struct regcache *regcache,
		     gdb_byte *readbuf, const gdb_byte *writebuf)
{
  if (sh_use_struct_convention (0, type))
    return RETURN_VALUE_STRUCT_CONVENTION;
  if (writebuf)
    sh_store_return_value_fpu (type, regcache, writebuf);
  else if (readbuf)
    sh_extract_return_value_fpu (type, regcache, readbuf);
  return RETURN_VALUE_REGISTER_CONVENTION;
}

/* Print the registers in a form similar to the E7000 */

static void
sh_generic_show_regs (struct frame_info *frame)
{
  printf_filtered
    ("      PC %s       SR %08lx       PR %08lx     MACH %08lx\n",
     paddr (get_frame_register_unsigned (frame,
					 gdbarch_pc_regnum (current_gdbarch))),
     (long) get_frame_register_unsigned (frame, SR_REGNUM),
     (long) get_frame_register_unsigned (frame, PR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACH_REGNUM));

  printf_filtered
    ("     GBR %08lx      VBR %08lx                       MACL %08lx\n",
     (long) get_frame_register_unsigned (frame, GBR_REGNUM),
     (long) get_frame_register_unsigned (frame, VBR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACL_REGNUM));

  printf_filtered
    ("R0-R7    %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 0),
     (long) get_frame_register_unsigned (frame, 1),
     (long) get_frame_register_unsigned (frame, 2),
     (long) get_frame_register_unsigned (frame, 3),
     (long) get_frame_register_unsigned (frame, 4),
     (long) get_frame_register_unsigned (frame, 5),
     (long) get_frame_register_unsigned (frame, 6),
     (long) get_frame_register_unsigned (frame, 7));
  printf_filtered
    ("R8-R15   %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 8),
     (long) get_frame_register_unsigned (frame, 9),
     (long) get_frame_register_unsigned (frame, 10),
     (long) get_frame_register_unsigned (frame, 11),
     (long) get_frame_register_unsigned (frame, 12),
     (long) get_frame_register_unsigned (frame, 13),
     (long) get_frame_register_unsigned (frame, 14),
     (long) get_frame_register_unsigned (frame, 15));
}

static void
sh3_show_regs (struct frame_info *frame)
{
  printf_filtered
    ("      PC %s       SR %08lx       PR %08lx     MACH %08lx\n",
     paddr (get_frame_register_unsigned (frame,
					 gdbarch_pc_regnum (current_gdbarch))),
     (long) get_frame_register_unsigned (frame, SR_REGNUM),
     (long) get_frame_register_unsigned (frame, PR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACH_REGNUM));

  printf_filtered
    ("     GBR %08lx      VBR %08lx                       MACL %08lx\n",
     (long) get_frame_register_unsigned (frame, GBR_REGNUM),
     (long) get_frame_register_unsigned (frame, VBR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACL_REGNUM));
  printf_filtered
    ("     SSR %08lx      SPC %08lx\n",
     (long) get_frame_register_unsigned (frame, SSR_REGNUM),
     (long) get_frame_register_unsigned (frame, SPC_REGNUM));

  printf_filtered
    ("R0-R7    %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 0),
     (long) get_frame_register_unsigned (frame, 1),
     (long) get_frame_register_unsigned (frame, 2),
     (long) get_frame_register_unsigned (frame, 3),
     (long) get_frame_register_unsigned (frame, 4),
     (long) get_frame_register_unsigned (frame, 5),
     (long) get_frame_register_unsigned (frame, 6),
     (long) get_frame_register_unsigned (frame, 7));
  printf_filtered
    ("R8-R15   %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 8),
     (long) get_frame_register_unsigned (frame, 9),
     (long) get_frame_register_unsigned (frame, 10),
     (long) get_frame_register_unsigned (frame, 11),
     (long) get_frame_register_unsigned (frame, 12),
     (long) get_frame_register_unsigned (frame, 13),
     (long) get_frame_register_unsigned (frame, 14),
     (long) get_frame_register_unsigned (frame, 15));
}

static void
sh2e_show_regs (struct frame_info *frame)
{
  printf_filtered
    ("      PC %s       SR %08lx       PR %08lx     MACH %08lx\n",
     paddr (get_frame_register_unsigned (frame,
					 gdbarch_pc_regnum (current_gdbarch))),
     (long) get_frame_register_unsigned (frame, SR_REGNUM),
     (long) get_frame_register_unsigned (frame, PR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACH_REGNUM));

  printf_filtered
    ("     GBR %08lx      VBR %08lx                       MACL %08lx\n",
     (long) get_frame_register_unsigned (frame, GBR_REGNUM),
     (long) get_frame_register_unsigned (frame, VBR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACL_REGNUM));
  printf_filtered
    ("     SSR %08lx      SPC %08lx     FPUL %08lx    FPSCR %08lx\n",
     (long) get_frame_register_unsigned (frame, SSR_REGNUM),
     (long) get_frame_register_unsigned (frame, SPC_REGNUM),
     (long) get_frame_register_unsigned (frame, FPUL_REGNUM),
     (long) get_frame_register_unsigned (frame, FPSCR_REGNUM));

  printf_filtered
    ("R0-R7    %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 0),
     (long) get_frame_register_unsigned (frame, 1),
     (long) get_frame_register_unsigned (frame, 2),
     (long) get_frame_register_unsigned (frame, 3),
     (long) get_frame_register_unsigned (frame, 4),
     (long) get_frame_register_unsigned (frame, 5),
     (long) get_frame_register_unsigned (frame, 6),
     (long) get_frame_register_unsigned (frame, 7));
  printf_filtered
    ("R8-R15   %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 8),
     (long) get_frame_register_unsigned (frame, 9),
     (long) get_frame_register_unsigned (frame, 10),
     (long) get_frame_register_unsigned (frame, 11),
     (long) get_frame_register_unsigned (frame, 12),
     (long) get_frame_register_unsigned (frame, 13),
     (long) get_frame_register_unsigned (frame, 14),
     (long) get_frame_register_unsigned (frame, 15));

  printf_filtered
    ("FP0-FP7  %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 0),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 1),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 2),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 3),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 4),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 5),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 6),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 7));
  printf_filtered
    ("FP8-FP15 %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 8),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 9),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 10),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 11),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 12),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 13),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 14),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 15));
}

static void
sh2a_show_regs (struct frame_info *frame)
{
  int pr = get_frame_register_unsigned (frame, FPSCR_REGNUM) & 0x80000;

  printf_filtered
    ("      PC %s       SR %08lx       PR %08lx     MACH %08lx\n",
     paddr (get_frame_register_unsigned (frame,
					 gdbarch_pc_regnum (current_gdbarch))),
     (long) get_frame_register_unsigned (frame, SR_REGNUM),
     (long) get_frame_register_unsigned (frame, PR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACH_REGNUM));

  printf_filtered
    ("     GBR %08lx      VBR %08lx      TBR %08lx     MACL %08lx\n",
     (long) get_frame_register_unsigned (frame, GBR_REGNUM),
     (long) get_frame_register_unsigned (frame, VBR_REGNUM),
     (long) get_frame_register_unsigned (frame, TBR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACL_REGNUM));
  printf_filtered
    ("     SSR %08lx      SPC %08lx     FPUL %08lx    FPSCR %08lx\n",
     (long) get_frame_register_unsigned (frame, SSR_REGNUM),
     (long) get_frame_register_unsigned (frame, SPC_REGNUM),
     (long) get_frame_register_unsigned (frame, FPUL_REGNUM),
     (long) get_frame_register_unsigned (frame, FPSCR_REGNUM));

  printf_filtered
    ("R0-R7    %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 0),
     (long) get_frame_register_unsigned (frame, 1),
     (long) get_frame_register_unsigned (frame, 2),
     (long) get_frame_register_unsigned (frame, 3),
     (long) get_frame_register_unsigned (frame, 4),
     (long) get_frame_register_unsigned (frame, 5),
     (long) get_frame_register_unsigned (frame, 6),
     (long) get_frame_register_unsigned (frame, 7));
  printf_filtered
    ("R8-R15   %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 8),
     (long) get_frame_register_unsigned (frame, 9),
     (long) get_frame_register_unsigned (frame, 10),
     (long) get_frame_register_unsigned (frame, 11),
     (long) get_frame_register_unsigned (frame, 12),
     (long) get_frame_register_unsigned (frame, 13),
     (long) get_frame_register_unsigned (frame, 14),
     (long) get_frame_register_unsigned (frame, 15));

  printf_filtered
    (pr ? "DR0-DR6  %08lx%08lx  %08lx%08lx  %08lx%08lx  %08lx%08lx\n"
	: "FP0-FP7  %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 0),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 1),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 2),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 3),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 4),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 5),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 6),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 7));
  printf_filtered
    (pr ? "DR8-DR14 %08lx%08lx  %08lx%08lx  %08lx%08lx  %08lx%08lx\n"
	: "FP8-FP15 %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 8),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 9),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 10),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 11),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 12),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 13),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 14),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 15));
  printf_filtered
    ("BANK=%-3d\n", (int) get_frame_register_unsigned (frame, BANK_REGNUM));
  printf_filtered
    ("R0b-R7b  %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 0),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 1),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 2),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 3),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 4),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 5),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 6),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 7));
  printf_filtered
    ("R8b-R14b %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 8),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 9),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 10),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 11),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 12),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 13),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 14));
  printf_filtered
    ("MACHb=%08lx IVNb=%08lx PRb=%08lx GBRb=%08lx MACLb=%08lx\n",
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 15),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 16),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 17),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 18),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 19));
}

static void
sh2a_nofpu_show_regs (struct frame_info *frame)
{
  int pr = get_frame_register_unsigned (frame, FPSCR_REGNUM) & 0x80000;

  printf_filtered
    ("      PC %s       SR %08lx       PR %08lx     MACH %08lx\n",
     paddr (get_frame_register_unsigned (frame,
					 gdbarch_pc_regnum (current_gdbarch))),
     (long) get_frame_register_unsigned (frame, SR_REGNUM),
     (long) get_frame_register_unsigned (frame, PR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACH_REGNUM));

  printf_filtered
    ("     GBR %08lx      VBR %08lx      TBR %08lx     MACL %08lx\n",
     (long) get_frame_register_unsigned (frame, GBR_REGNUM),
     (long) get_frame_register_unsigned (frame, VBR_REGNUM),
     (long) get_frame_register_unsigned (frame, TBR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACL_REGNUM));
  printf_filtered
    ("     SSR %08lx      SPC %08lx     FPUL %08lx    FPSCR %08lx\n",
     (long) get_frame_register_unsigned (frame, SSR_REGNUM),
     (long) get_frame_register_unsigned (frame, SPC_REGNUM),
     (long) get_frame_register_unsigned (frame, FPUL_REGNUM),
     (long) get_frame_register_unsigned (frame, FPSCR_REGNUM));

  printf_filtered
    ("R0-R7    %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 0),
     (long) get_frame_register_unsigned (frame, 1),
     (long) get_frame_register_unsigned (frame, 2),
     (long) get_frame_register_unsigned (frame, 3),
     (long) get_frame_register_unsigned (frame, 4),
     (long) get_frame_register_unsigned (frame, 5),
     (long) get_frame_register_unsigned (frame, 6),
     (long) get_frame_register_unsigned (frame, 7));
  printf_filtered
    ("R8-R15   %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 8),
     (long) get_frame_register_unsigned (frame, 9),
     (long) get_frame_register_unsigned (frame, 10),
     (long) get_frame_register_unsigned (frame, 11),
     (long) get_frame_register_unsigned (frame, 12),
     (long) get_frame_register_unsigned (frame, 13),
     (long) get_frame_register_unsigned (frame, 14),
     (long) get_frame_register_unsigned (frame, 15));

  printf_filtered
    ("BANK=%-3d\n", (int) get_frame_register_unsigned (frame, BANK_REGNUM));
  printf_filtered
    ("R0b-R7b  %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 0),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 1),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 2),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 3),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 4),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 5),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 6),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 7));
  printf_filtered
    ("R8b-R14b %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 8),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 9),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 10),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 11),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 12),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 13),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 14));
  printf_filtered
    ("MACHb=%08lx IVNb=%08lx PRb=%08lx GBRb=%08lx MACLb=%08lx\n",
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 15),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 16),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 17),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 18),
     (long) get_frame_register_unsigned (frame, R0_BANK0_REGNUM + 19));
}

static void
sh3e_show_regs (struct frame_info *frame)
{
  printf_filtered
    ("      PC %s       SR %08lx       PR %08lx     MACH %08lx\n",
     paddr (get_frame_register_unsigned (frame,
					 gdbarch_pc_regnum (current_gdbarch))),
     (long) get_frame_register_unsigned (frame, SR_REGNUM),
     (long) get_frame_register_unsigned (frame, PR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACH_REGNUM));

  printf_filtered
    ("     GBR %08lx      VBR %08lx                       MACL %08lx\n",
     (long) get_frame_register_unsigned (frame, GBR_REGNUM),
     (long) get_frame_register_unsigned (frame, VBR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACL_REGNUM));
  printf_filtered
    ("     SSR %08lx      SPC %08lx     FPUL %08lx    FPSCR %08lx\n",
     (long) get_frame_register_unsigned (frame, SSR_REGNUM),
     (long) get_frame_register_unsigned (frame, SPC_REGNUM),
     (long) get_frame_register_unsigned (frame, FPUL_REGNUM),
     (long) get_frame_register_unsigned (frame, FPSCR_REGNUM));

  printf_filtered
    ("R0-R7    %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 0),
     (long) get_frame_register_unsigned (frame, 1),
     (long) get_frame_register_unsigned (frame, 2),
     (long) get_frame_register_unsigned (frame, 3),
     (long) get_frame_register_unsigned (frame, 4),
     (long) get_frame_register_unsigned (frame, 5),
     (long) get_frame_register_unsigned (frame, 6),
     (long) get_frame_register_unsigned (frame, 7));
  printf_filtered
    ("R8-R15   %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 8),
     (long) get_frame_register_unsigned (frame, 9),
     (long) get_frame_register_unsigned (frame, 10),
     (long) get_frame_register_unsigned (frame, 11),
     (long) get_frame_register_unsigned (frame, 12),
     (long) get_frame_register_unsigned (frame, 13),
     (long) get_frame_register_unsigned (frame, 14),
     (long) get_frame_register_unsigned (frame, 15));

  printf_filtered
    ("FP0-FP7  %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 0),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 1),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 2),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 3),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 4),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 5),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 6),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 7));
  printf_filtered
    ("FP8-FP15 %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 8),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 9),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 10),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 11),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 12),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 13),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 14),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 15));
}

static void
sh3_dsp_show_regs (struct frame_info *frame)
{
  printf_filtered
    ("      PC %s       SR %08lx       PR %08lx     MACH %08lx\n",
     paddr (get_frame_register_unsigned (frame,
					 gdbarch_pc_regnum (current_gdbarch))),
     (long) get_frame_register_unsigned (frame, SR_REGNUM),
     (long) get_frame_register_unsigned (frame, PR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACH_REGNUM));

  printf_filtered
    ("     GBR %08lx      VBR %08lx                       MACL %08lx\n",
     (long) get_frame_register_unsigned (frame, GBR_REGNUM),
     (long) get_frame_register_unsigned (frame, VBR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACL_REGNUM));

  printf_filtered
    ("     SSR %08lx      SPC %08lx      DSR %08lx\n",
     (long) get_frame_register_unsigned (frame, SSR_REGNUM),
     (long) get_frame_register_unsigned (frame, SPC_REGNUM),
     (long) get_frame_register_unsigned (frame, DSR_REGNUM));

  printf_filtered
    ("R0-R7    %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 0),
     (long) get_frame_register_unsigned (frame, 1),
     (long) get_frame_register_unsigned (frame, 2),
     (long) get_frame_register_unsigned (frame, 3),
     (long) get_frame_register_unsigned (frame, 4),
     (long) get_frame_register_unsigned (frame, 5),
     (long) get_frame_register_unsigned (frame, 6),
     (long) get_frame_register_unsigned (frame, 7));
  printf_filtered
    ("R8-R15   %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 8),
     (long) get_frame_register_unsigned (frame, 9),
     (long) get_frame_register_unsigned (frame, 10),
     (long) get_frame_register_unsigned (frame, 11),
     (long) get_frame_register_unsigned (frame, 12),
     (long) get_frame_register_unsigned (frame, 13),
     (long) get_frame_register_unsigned (frame, 14),
     (long) get_frame_register_unsigned (frame, 15));

  printf_filtered
    ("A0G=%02lx A0=%08lx M0=%08lx X0=%08lx Y0=%08lx RS=%08lx MOD=%08lx\n",
     (long) get_frame_register_unsigned (frame, A0G_REGNUM) & 0xff,
     (long) get_frame_register_unsigned (frame, A0_REGNUM),
     (long) get_frame_register_unsigned (frame, M0_REGNUM),
     (long) get_frame_register_unsigned (frame, X0_REGNUM),
     (long) get_frame_register_unsigned (frame, Y0_REGNUM),
     (long) get_frame_register_unsigned (frame, RS_REGNUM),
     (long) get_frame_register_unsigned (frame, MOD_REGNUM));
  printf_filtered
    ("A1G=%02lx A1=%08lx M1=%08lx X1=%08lx Y1=%08lx RE=%08lx\n",
     (long) get_frame_register_unsigned (frame, A1G_REGNUM) & 0xff,
     (long) get_frame_register_unsigned (frame, A1_REGNUM),
     (long) get_frame_register_unsigned (frame, M1_REGNUM),
     (long) get_frame_register_unsigned (frame, X1_REGNUM),
     (long) get_frame_register_unsigned (frame, Y1_REGNUM),
     (long) get_frame_register_unsigned (frame, RE_REGNUM));
}

static void
sh4_show_regs (struct frame_info *frame)
{
  int pr = get_frame_register_unsigned (frame, FPSCR_REGNUM) & 0x80000;

  printf_filtered
    ("      PC %s       SR %08lx       PR %08lx     MACH %08lx\n",
     paddr (get_frame_register_unsigned (frame,
					 gdbarch_pc_regnum (current_gdbarch))),
     (long) get_frame_register_unsigned (frame, SR_REGNUM),
     (long) get_frame_register_unsigned (frame, PR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACH_REGNUM));

  printf_filtered
    ("     GBR %08lx      VBR %08lx                       MACL %08lx\n",
     (long) get_frame_register_unsigned (frame, GBR_REGNUM),
     (long) get_frame_register_unsigned (frame, VBR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACL_REGNUM));
  printf_filtered
    ("     SSR %08lx      SPC %08lx     FPUL %08lx    FPSCR %08lx\n",
     (long) get_frame_register_unsigned (frame, SSR_REGNUM),
     (long) get_frame_register_unsigned (frame, SPC_REGNUM),
     (long) get_frame_register_unsigned (frame, FPUL_REGNUM),
     (long) get_frame_register_unsigned (frame, FPSCR_REGNUM));

  printf_filtered
    ("R0-R7    %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 0),
     (long) get_frame_register_unsigned (frame, 1),
     (long) get_frame_register_unsigned (frame, 2),
     (long) get_frame_register_unsigned (frame, 3),
     (long) get_frame_register_unsigned (frame, 4),
     (long) get_frame_register_unsigned (frame, 5),
     (long) get_frame_register_unsigned (frame, 6),
     (long) get_frame_register_unsigned (frame, 7));
  printf_filtered
    ("R8-R15   %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 8),
     (long) get_frame_register_unsigned (frame, 9),
     (long) get_frame_register_unsigned (frame, 10),
     (long) get_frame_register_unsigned (frame, 11),
     (long) get_frame_register_unsigned (frame, 12),
     (long) get_frame_register_unsigned (frame, 13),
     (long) get_frame_register_unsigned (frame, 14),
     (long) get_frame_register_unsigned (frame, 15));

  printf_filtered
    (pr ? "DR0-DR6  %08lx%08lx  %08lx%08lx  %08lx%08lx  %08lx%08lx\n"
	: "FP0-FP7  %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 0),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 1),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 2),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 3),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 4),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 5),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 6),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 7));
  printf_filtered
    (pr ? "DR8-DR14 %08lx%08lx  %08lx%08lx  %08lx%08lx  %08lx%08lx\n"
	: "FP8-FP15 %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 8),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 9),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 10),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 11),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 12),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 13),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 14),
     (long) get_frame_register_unsigned
	      (frame, gdbarch_fp0_regnum (current_gdbarch) + 15));
}

static void
sh4_nofpu_show_regs (struct frame_info *frame)
{
  printf_filtered
    ("      PC %s       SR %08lx       PR %08lx     MACH %08lx\n",
     paddr (get_frame_register_unsigned (frame,
					 gdbarch_pc_regnum (current_gdbarch))),
     (long) get_frame_register_unsigned (frame, SR_REGNUM),
     (long) get_frame_register_unsigned (frame, PR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACH_REGNUM));

  printf_filtered
    ("     GBR %08lx      VBR %08lx                       MACL %08lx\n",
     (long) get_frame_register_unsigned (frame, GBR_REGNUM),
     (long) get_frame_register_unsigned (frame, VBR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACL_REGNUM));
  printf_filtered
    ("     SSR %08lx      SPC %08lx     FPUL %08lx    FPSCR %08lx\n",
     (long) get_frame_register_unsigned (frame, SSR_REGNUM),
     (long) get_frame_register_unsigned (frame, SPC_REGNUM),
     (long) get_frame_register_unsigned (frame, FPUL_REGNUM),
     (long) get_frame_register_unsigned (frame, FPSCR_REGNUM));

  printf_filtered
    ("R0-R7    %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 0),
     (long) get_frame_register_unsigned (frame, 1),
     (long) get_frame_register_unsigned (frame, 2),
     (long) get_frame_register_unsigned (frame, 3),
     (long) get_frame_register_unsigned (frame, 4),
     (long) get_frame_register_unsigned (frame, 5),
     (long) get_frame_register_unsigned (frame, 6),
     (long) get_frame_register_unsigned (frame, 7));
  printf_filtered
    ("R8-R15   %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 8),
     (long) get_frame_register_unsigned (frame, 9),
     (long) get_frame_register_unsigned (frame, 10),
     (long) get_frame_register_unsigned (frame, 11),
     (long) get_frame_register_unsigned (frame, 12),
     (long) get_frame_register_unsigned (frame, 13),
     (long) get_frame_register_unsigned (frame, 14),
     (long) get_frame_register_unsigned (frame, 15));
}

static void
sh_dsp_show_regs (struct frame_info *frame)
{
  printf_filtered
    ("      PC %s       SR %08lx       PR %08lx     MACH %08lx\n",
     paddr (get_frame_register_unsigned (frame,
					 gdbarch_pc_regnum (current_gdbarch))),
     (long) get_frame_register_unsigned (frame, SR_REGNUM),
     (long) get_frame_register_unsigned (frame, PR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACH_REGNUM));

  printf_filtered
    ("     GBR %08lx      VBR %08lx      DSR %08lx     MACL %08lx\n",
     (long) get_frame_register_unsigned (frame, GBR_REGNUM),
     (long) get_frame_register_unsigned (frame, VBR_REGNUM),
     (long) get_frame_register_unsigned (frame, DSR_REGNUM),
     (long) get_frame_register_unsigned (frame, MACL_REGNUM));

  printf_filtered
    ("R0-R7    %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 0),
     (long) get_frame_register_unsigned (frame, 1),
     (long) get_frame_register_unsigned (frame, 2),
     (long) get_frame_register_unsigned (frame, 3),
     (long) get_frame_register_unsigned (frame, 4),
     (long) get_frame_register_unsigned (frame, 5),
     (long) get_frame_register_unsigned (frame, 6),
     (long) get_frame_register_unsigned (frame, 7));
  printf_filtered
    ("R8-R15   %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
     (long) get_frame_register_unsigned (frame, 8),
     (long) get_frame_register_unsigned (frame, 9),
     (long) get_frame_register_unsigned (frame, 10),
     (long) get_frame_register_unsigned (frame, 11),
     (long) get_frame_register_unsigned (frame, 12),
     (long) get_frame_register_unsigned (frame, 13),
     (long) get_frame_register_unsigned (frame, 14),
     (long) get_frame_register_unsigned (frame, 15));

  printf_filtered
    ("A0G=%02lx A0=%08lx M0=%08lx X0=%08lx Y0=%08lx RS=%08lx MOD=%08lx\n",
     (long) get_frame_register_unsigned (frame, A0G_REGNUM) & 0xff,
     (long) get_frame_register_unsigned (frame, A0_REGNUM),
     (long) get_frame_register_unsigned (frame, M0_REGNUM),
     (long) get_frame_register_unsigned (frame, X0_REGNUM),
     (long) get_frame_register_unsigned (frame, Y0_REGNUM),
     (long) get_frame_register_unsigned (frame, RS_REGNUM),
     (long) get_frame_register_unsigned (frame, MOD_REGNUM));
  printf_filtered ("A1G=%02lx A1=%08lx M1=%08lx X1=%08lx Y1=%08lx RE=%08lx\n",
     (long) get_frame_register_unsigned (frame, A1G_REGNUM) & 0xff,
     (long) get_frame_register_unsigned (frame, A1_REGNUM),
     (long) get_frame_register_unsigned (frame, M1_REGNUM),
     (long) get_frame_register_unsigned (frame, X1_REGNUM),
     (long) get_frame_register_unsigned (frame, Y1_REGNUM),
     (long) get_frame_register_unsigned (frame, RE_REGNUM));
}

static void
sh_show_regs_command (char *args, int from_tty)
{
  if (sh_show_regs)
    (*sh_show_regs) (get_current_frame ());
}

static struct type *
sh_sh2a_register_type (struct gdbarch *gdbarch, int reg_nr)
{
  if ((reg_nr >= gdbarch_fp0_regnum (current_gdbarch)
       && (reg_nr <= FP_LAST_REGNUM)) || (reg_nr == FPUL_REGNUM))
    return builtin_type_float;
  else if (reg_nr >= DR0_REGNUM && reg_nr <= DR_LAST_REGNUM)
    return builtin_type_double;
  else
    return builtin_type_int;
}

/* Return the GDB type object for the "standard" data type
   of data in register N.  */
static struct type *
sh_sh3e_register_type (struct gdbarch *gdbarch, int reg_nr)
{
  if ((reg_nr >= gdbarch_fp0_regnum (current_gdbarch)
       && (reg_nr <= FP_LAST_REGNUM)) || (reg_nr == FPUL_REGNUM))
    return builtin_type_float;
  else
    return builtin_type_int;
}

static struct type *
sh_sh4_build_float_register_type (int high)
{
  struct type *temp;

  temp = create_range_type (NULL, builtin_type_int, 0, high);
  return create_array_type (NULL, builtin_type_float, temp);
}

static struct type *
sh_sh4_register_type (struct gdbarch *gdbarch, int reg_nr)
{
  if ((reg_nr >= gdbarch_fp0_regnum (current_gdbarch)
       && (reg_nr <= FP_LAST_REGNUM)) || (reg_nr == FPUL_REGNUM))
    return builtin_type_float;
  else if (reg_nr >= DR0_REGNUM && reg_nr <= DR_LAST_REGNUM)
    return builtin_type_double;
  else if (reg_nr >= FV0_REGNUM && reg_nr <= FV_LAST_REGNUM)
    return sh_sh4_build_float_register_type (3);
  else
    return builtin_type_int;
}

static struct type *
sh_default_register_type (struct gdbarch *gdbarch, int reg_nr)
{
  return builtin_type_int;
}

/* Is a register in a reggroup?
   The default code in reggroup.c doesn't identify system registers, some
   float registers or any of the vector registers.
   TODO: sh2a and dsp registers.  */
int
sh_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
			struct reggroup *reggroup)
{
  if (gdbarch_register_name (current_gdbarch, regnum) == NULL
      || *gdbarch_register_name (current_gdbarch, regnum) == '\0')
    return 0;

  if (reggroup == float_reggroup
      && (regnum == FPUL_REGNUM
	  || regnum == FPSCR_REGNUM))
    return 1;

  if (regnum >= FV0_REGNUM && regnum <= FV_LAST_REGNUM)
    {
      if (reggroup == vector_reggroup || reggroup == float_reggroup)
	return 1;
      if (reggroup == general_reggroup)
	return 0;
    }

  if (regnum == VBR_REGNUM
      || regnum == SR_REGNUM
      || regnum == FPSCR_REGNUM
      || regnum == SSR_REGNUM
      || regnum == SPC_REGNUM)
    {
      if (reggroup == system_reggroup)
	return 1;
      if (reggroup == general_reggroup)
	return 0;
    }

  /* The default code can cope with any other registers.  */
  return default_register_reggroup_p (gdbarch, regnum, reggroup);
}

/* On the sh4, the DRi pseudo registers are problematic if the target
   is little endian. When the user writes one of those registers, for
   instance with 'ser var $dr0=1', we want the double to be stored
   like this: 
   fr0 = 0x00 0x00 0x00 0x00 0x00 0xf0 0x3f 
   fr1 = 0x00 0x00 0x00 0x00 0x00 0x00 0x00 

   This corresponds to little endian byte order & big endian word
   order.  However if we let gdb write the register w/o conversion, it
   will write fr0 and fr1 this way:
   fr0 = 0x00 0x00 0x00 0x00 0x00 0x00 0x00
   fr1 = 0x00 0x00 0x00 0x00 0x00 0xf0 0x3f
   because it will consider fr0 and fr1 as a single LE stretch of memory.
   
   To achieve what we want we must force gdb to store things in
   floatformat_ieee_double_littlebyte_bigword (which is defined in
   include/floatformat.h and libiberty/floatformat.c.

   In case the target is big endian, there is no problem, the
   raw bytes will look like:
   fr0 = 0x3f 0xf0 0x00 0x00 0x00 0x00 0x00
   fr1 = 0x00 0x00 0x00 0x00 0x00 0x00 0x00 

   The other pseudo registers (the FVs) also don't pose a problem
   because they are stored as 4 individual FP elements. */

static void
sh_register_convert_to_virtual (int regnum, struct type *type,
				char *from, char *to)
{
  if (regnum >= DR0_REGNUM && regnum <= DR_LAST_REGNUM)
    {
      DOUBLEST val;
      floatformat_to_doublest (&floatformat_ieee_double_littlebyte_bigword,
			       from, &val);
      store_typed_floating (to, type, val);
    }
  else
    error
      ("sh_register_convert_to_virtual called with non DR register number");
}

static void
sh_register_convert_to_raw (struct type *type, int regnum,
			    const void *from, void *to)
{
  if (regnum >= DR0_REGNUM && regnum <= DR_LAST_REGNUM)
    {
      DOUBLEST val = extract_typed_floating (from, type);
      floatformat_from_doublest (&floatformat_ieee_double_littlebyte_bigword,
				 &val, to);
    }
  else
    error (_("sh_register_convert_to_raw called with non DR register number"));
}

/* For vectors of 4 floating point registers. */
static int
fv_reg_base_num (int fv_regnum)
{
  int fp_regnum;

  fp_regnum = gdbarch_fp0_regnum (current_gdbarch)
	      + (fv_regnum - FV0_REGNUM) * 4;
  return fp_regnum;
}

/* For double precision floating point registers, i.e 2 fp regs.*/
static int
dr_reg_base_num (int dr_regnum)
{
  int fp_regnum;

  fp_regnum = gdbarch_fp0_regnum (current_gdbarch)
	      + (dr_regnum - DR0_REGNUM) * 2;
  return fp_regnum;
}

static void
sh_pseudo_register_read (struct gdbarch *gdbarch, struct regcache *regcache,
			 int reg_nr, gdb_byte *buffer)
{
  int base_regnum, portion;
  char temp_buffer[MAX_REGISTER_SIZE];

  if (reg_nr == PSEUDO_BANK_REGNUM)
    regcache_raw_read (regcache, BANK_REGNUM, buffer);
  else
  if (reg_nr >= DR0_REGNUM && reg_nr <= DR_LAST_REGNUM)
    {
      base_regnum = dr_reg_base_num (reg_nr);

      /* Build the value in the provided buffer. */
      /* Read the real regs for which this one is an alias.  */
      for (portion = 0; portion < 2; portion++)
	regcache_raw_read (regcache, base_regnum + portion,
			   (temp_buffer
			    + register_size (gdbarch,
					     base_regnum) * portion));
      /* We must pay attention to the endiannes. */
      sh_register_convert_to_virtual (reg_nr,
				      register_type (gdbarch, reg_nr),
				      temp_buffer, buffer);
    }
  else if (reg_nr >= FV0_REGNUM && reg_nr <= FV_LAST_REGNUM)
    {
      base_regnum = fv_reg_base_num (reg_nr);

      /* Read the real regs for which this one is an alias.  */
      for (portion = 0; portion < 4; portion++)
	regcache_raw_read (regcache, base_regnum + portion,
			   ((char *) buffer
			    + register_size (gdbarch,
					     base_regnum) * portion));
    }
}

static void
sh_pseudo_register_write (struct gdbarch *gdbarch, struct regcache *regcache,
			  int reg_nr, const gdb_byte *buffer)
{
  int base_regnum, portion;
  char temp_buffer[MAX_REGISTER_SIZE];

  if (reg_nr == PSEUDO_BANK_REGNUM)
    {
      /* When the bank register is written to, the whole register bank
         is switched and all values in the bank registers must be read
	 from the target/sim again. We're just invalidating the regcache
	 so that a re-read happens next time it's necessary.  */
      int bregnum;

      regcache_raw_write (regcache, BANK_REGNUM, buffer);
      for (bregnum = R0_BANK0_REGNUM; bregnum < MACLB_REGNUM; ++bregnum)
        regcache_invalidate (regcache, bregnum);
    }
  else if (reg_nr >= DR0_REGNUM && reg_nr <= DR_LAST_REGNUM)
    {
      base_regnum = dr_reg_base_num (reg_nr);

      /* We must pay attention to the endiannes. */
      sh_register_convert_to_raw (register_type (gdbarch, reg_nr),
				  reg_nr, buffer, temp_buffer);

      /* Write the real regs for which this one is an alias.  */
      for (portion = 0; portion < 2; portion++)
	regcache_raw_write (regcache, base_regnum + portion,
			    (temp_buffer
			     + register_size (gdbarch,
					      base_regnum) * portion));
    }
  else if (reg_nr >= FV0_REGNUM && reg_nr <= FV_LAST_REGNUM)
    {
      base_regnum = fv_reg_base_num (reg_nr);

      /* Write the real regs for which this one is an alias.  */
      for (portion = 0; portion < 4; portion++)
	regcache_raw_write (regcache, base_regnum + portion,
			    ((char *) buffer
			     + register_size (gdbarch,
					      base_regnum) * portion));
    }
}

static int
sh_dsp_register_sim_regno (int nr)
{
  if (legacy_register_sim_regno (nr) < 0)
    return legacy_register_sim_regno (nr);
  if (nr >= DSR_REGNUM && nr <= Y1_REGNUM)
    return nr - DSR_REGNUM + SIM_SH_DSR_REGNUM;
  if (nr == MOD_REGNUM)
    return SIM_SH_MOD_REGNUM;
  if (nr == RS_REGNUM)
    return SIM_SH_RS_REGNUM;
  if (nr == RE_REGNUM)
    return SIM_SH_RE_REGNUM;
  if (nr >= DSP_R0_BANK_REGNUM && nr <= DSP_R7_BANK_REGNUM)
    return nr - DSP_R0_BANK_REGNUM + SIM_SH_R0_BANK_REGNUM;
  return nr;
}

static int
sh_sh2a_register_sim_regno (int nr)
{
  switch (nr)
    {
      case TBR_REGNUM:
        return SIM_SH_TBR_REGNUM;
      case IBNR_REGNUM:
        return SIM_SH_IBNR_REGNUM;
      case IBCR_REGNUM:
        return SIM_SH_IBCR_REGNUM;
      case BANK_REGNUM:
        return SIM_SH_BANK_REGNUM;
      case MACLB_REGNUM:
        return SIM_SH_BANK_MACL_REGNUM;
      case GBRB_REGNUM:
        return SIM_SH_BANK_GBR_REGNUM;
      case PRB_REGNUM:
        return SIM_SH_BANK_PR_REGNUM;
      case IVNB_REGNUM:
        return SIM_SH_BANK_IVN_REGNUM;
      case MACHB_REGNUM:
        return SIM_SH_BANK_MACH_REGNUM;
      default:
        break;
    }
  return legacy_register_sim_regno (nr);
}

/* Set up the register unwinding such that call-clobbered registers are
   not displayed in frames >0 because the true value is not certain.
   The 'undefined' registers will show up as 'not available' unless the
   CFI says otherwise.

   This function is currently set up for SH4 and compatible only.  */

static void
sh_dwarf2_frame_init_reg (struct gdbarch *gdbarch, int regnum,
                          struct dwarf2_frame_state_reg *reg,
			  struct frame_info *next_frame)
{
  /* Mark the PC as the destination for the return address.  */
  if (regnum == gdbarch_pc_regnum (current_gdbarch))
    reg->how = DWARF2_FRAME_REG_RA;

  /* Mark the stack pointer as the call frame address.  */
  else if (regnum == gdbarch_sp_regnum (current_gdbarch))
    reg->how = DWARF2_FRAME_REG_CFA;

  /* The above was taken from the default init_reg in dwarf2-frame.c
     while the below is SH specific.  */

  /* Caller save registers.  */
  else if ((regnum >= R0_REGNUM && regnum <= R0_REGNUM+7)
	   || (regnum >= FR0_REGNUM && regnum <= FR0_REGNUM+11)
	   || (regnum >= DR0_REGNUM && regnum <= DR0_REGNUM+5)
	   || (regnum >= FV0_REGNUM && regnum <= FV0_REGNUM+2)
	   || (regnum == MACH_REGNUM)
	   || (regnum == MACL_REGNUM)
	   || (regnum == FPUL_REGNUM)
	   || (regnum == SR_REGNUM))
    reg->how = DWARF2_FRAME_REG_UNDEFINED;

  /* Callee save registers.  */
  else if ((regnum >= R0_REGNUM+8 && regnum <= R0_REGNUM+15)
	   || (regnum >= FR0_REGNUM+12 && regnum <= FR0_REGNUM+15)
	   || (regnum >= DR0_REGNUM+6 && regnum <= DR0_REGNUM+8)
	   || (regnum == FV0_REGNUM+3))
    reg->how = DWARF2_FRAME_REG_SAME_VALUE;

  /* Other registers.  These are not in the ABI and may or may not
     mean anything in frames >0 so don't show them.  */
  else if ((regnum >= R0_BANK0_REGNUM && regnum <= R0_BANK0_REGNUM+15)
	   || (regnum == GBR_REGNUM)
	   || (regnum == VBR_REGNUM)
	   || (regnum == FPSCR_REGNUM)
	   || (regnum == SSR_REGNUM)
	   || (regnum == SPC_REGNUM))
    reg->how = DWARF2_FRAME_REG_UNDEFINED;
}

static struct sh_frame_cache *
sh_alloc_frame_cache (void)
{
  struct sh_frame_cache *cache;
  int i;

  cache = FRAME_OBSTACK_ZALLOC (struct sh_frame_cache);

  /* Base address.  */
  cache->base = 0;
  cache->saved_sp = 0;
  cache->sp_offset = 0;
  cache->pc = 0;

  /* Frameless until proven otherwise.  */
  cache->uses_fp = 0;

  /* Saved registers.  We initialize these to -1 since zero is a valid
     offset (that's where fp is supposed to be stored).  */
  for (i = 0; i < SH_NUM_REGS; i++)
    {
      cache->saved_regs[i] = -1;
    }

  return cache;
}

static struct sh_frame_cache *
sh_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct sh_frame_cache *cache;
  CORE_ADDR current_pc;
  int i;

  if (*this_cache)
    return *this_cache;

  cache = sh_alloc_frame_cache ();
  *this_cache = cache;

  /* In principle, for normal frames, fp holds the frame pointer,
     which holds the base address for the current stack frame.
     However, for functions that don't need it, the frame pointer is
     optional.  For these "frameless" functions the frame pointer is
     actually the frame pointer of the calling frame. */
  cache->base = frame_unwind_register_unsigned (next_frame, FP_REGNUM);
  if (cache->base == 0)
    return cache;

  cache->pc = frame_func_unwind (next_frame, NORMAL_FRAME);
  current_pc = frame_pc_unwind (next_frame);
  if (cache->pc != 0)
    {
      ULONGEST fpscr;
      fpscr = frame_unwind_register_unsigned (next_frame, FPSCR_REGNUM);
      sh_analyze_prologue (cache->pc, current_pc, cache, fpscr);
    }

  if (!cache->uses_fp)
    {
      /* We didn't find a valid frame, which means that CACHE->base
         currently holds the frame pointer for our calling frame.  If
         we're at the start of a function, or somewhere half-way its
         prologue, the function's frame probably hasn't been fully
         setup yet.  Try to reconstruct the base address for the stack
         frame by looking at the stack pointer.  For truly "frameless"
         functions this might work too.  */
      cache->base = frame_unwind_register_unsigned
		    (next_frame, gdbarch_sp_regnum (current_gdbarch));
    }

  /* Now that we have the base address for the stack frame we can
     calculate the value of sp in the calling frame.  */
  cache->saved_sp = cache->base + cache->sp_offset;

  /* Adjust all the saved registers such that they contain addresses
     instead of offsets.  */
  for (i = 0; i < SH_NUM_REGS; i++)
    if (cache->saved_regs[i] != -1)
      cache->saved_regs[i] = cache->saved_sp - cache->saved_regs[i] - 4;

  return cache;
}

static void
sh_frame_prev_register (struct frame_info *next_frame, void **this_cache,
			int regnum, int *optimizedp,
			enum lval_type *lvalp, CORE_ADDR *addrp,
			int *realnump, gdb_byte *valuep)
{
  struct sh_frame_cache *cache = sh_frame_cache (next_frame, this_cache);

  gdb_assert (regnum >= 0);

  if (regnum == gdbarch_sp_regnum (current_gdbarch) && cache->saved_sp)
    {
      *optimizedp = 0;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      if (valuep)
	{
	  /* Store the value.  */
	  store_unsigned_integer (valuep, 4, cache->saved_sp);
	}
      return;
    }

  /* The PC of the previous frame is stored in the PR register of
     the current frame.  Frob regnum so that we pull the value from
     the correct place.  */
  if (regnum == gdbarch_pc_regnum (current_gdbarch))
    regnum = PR_REGNUM;

  if (regnum < SH_NUM_REGS && cache->saved_regs[regnum] != -1)
    {
      *optimizedp = 0;
      *lvalp = lval_memory;
      *addrp = cache->saved_regs[regnum];
      *realnump = -1;
      if (valuep)
	{
	  /* Read the value in from memory.  */
	  read_memory (*addrp, valuep,
		       register_size (current_gdbarch, regnum));
	}
      return;
    }

  *optimizedp = 0;
  *lvalp = lval_register;
  *addrp = 0;
  *realnump = regnum;
  if (valuep)
    frame_unwind_register (next_frame, (*realnump), valuep);
}

static void
sh_frame_this_id (struct frame_info *next_frame, void **this_cache,
		  struct frame_id *this_id)
{
  struct sh_frame_cache *cache = sh_frame_cache (next_frame, this_cache);

  /* This marks the outermost frame.  */
  if (cache->base == 0)
    return;

  *this_id = frame_id_build (cache->saved_sp, cache->pc);
}

static const struct frame_unwind sh_frame_unwind = {
  NORMAL_FRAME,
  sh_frame_this_id,
  sh_frame_prev_register
};

static const struct frame_unwind *
sh_frame_sniffer (struct frame_info *next_frame)
{
  return &sh_frame_unwind;
}

static CORE_ADDR
sh_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame,
					 gdbarch_sp_regnum (current_gdbarch));
}

static CORE_ADDR
sh_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame,
					 gdbarch_pc_regnum (current_gdbarch));
}

static struct frame_id
sh_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_id_build (sh_unwind_sp (gdbarch, next_frame),
			 frame_pc_unwind (next_frame));
}

static CORE_ADDR
sh_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct sh_frame_cache *cache = sh_frame_cache (next_frame, this_cache);

  return cache->base;
}

static const struct frame_base sh_frame_base = {
  &sh_frame_unwind,
  sh_frame_base_address,
  sh_frame_base_address,
  sh_frame_base_address
};

/* The epilogue is defined here as the area at the end of a function,
   either on the `ret' instruction itself or after an instruction which
   destroys the function's stack frame. */
static int
sh_in_function_epilogue_p (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  CORE_ADDR func_addr = 0, func_end = 0;

  if (find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    {
      ULONGEST inst;
      /* The sh epilogue is max. 14 bytes long.  Give another 14 bytes
         for a nop and some fixed data (e.g. big offsets) which are
         unfortunately also treated as part of the function (which
         means, they are below func_end. */
      CORE_ADDR addr = func_end - 28;
      if (addr < func_addr + 4)
	addr = func_addr + 4;
      if (pc < addr)
	return 0;

      /* First search forward until hitting an rts. */
      while (addr < func_end
	     && !IS_RTS (read_memory_unsigned_integer (addr, 2)))
	addr += 2;
      if (addr >= func_end)
	return 0;

      /* At this point we should find a mov.l @r15+,r14 instruction,
         either before or after the rts.  If not, then the function has
         probably no "normal" epilogue and we bail out here. */
      inst = read_memory_unsigned_integer (addr - 2, 2);
      if (IS_RESTORE_FP (read_memory_unsigned_integer (addr - 2, 2)))
	addr -= 2;
      else if (!IS_RESTORE_FP (read_memory_unsigned_integer (addr + 2, 2)))
	return 0;

      inst = read_memory_unsigned_integer (addr - 2, 2);

      /* Step over possible lds.l @r15+,macl. */
      if (IS_MACL_LDS (inst))
	{
	  addr -= 2;
	  inst = read_memory_unsigned_integer (addr - 2, 2);
	}

      /* Step over possible lds.l @r15+,pr. */
      if (IS_LDS (inst))
	{
	  addr -= 2;
	  inst = read_memory_unsigned_integer (addr - 2, 2);
	}

      /* Step over possible mov r14,r15. */
      if (IS_MOV_FP_SP (inst))
	{
	  addr -= 2;
	  inst = read_memory_unsigned_integer (addr - 2, 2);
	}

      /* Now check for FP adjustments, using add #imm,r14 or add rX, r14
         instructions. */
      while (addr > func_addr + 4
	     && (IS_ADD_REG_TO_FP (inst) || IS_ADD_IMM_FP (inst)))
	{
	  addr -= 2;
	  inst = read_memory_unsigned_integer (addr - 2, 2);
	}

      /* On SH2a check if the previous instruction was perhaps a MOVI20.
         That's allowed for the epilogue.  */
      if ((gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_sh2a
           || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_sh2a_nofpu)
          && addr > func_addr + 6
	  && IS_MOVI20 (read_memory_unsigned_integer (addr - 4, 2)))
	addr -= 4;

      if (pc >= addr)
	return 1;
    }
  return 0;
}


static struct gdbarch *
sh_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;

  sh_show_regs = sh_generic_show_regs;
  switch (info.bfd_arch_info->mach)
    {
    case bfd_mach_sh2e:
      sh_show_regs = sh2e_show_regs;
      break;
    case bfd_mach_sh2a:
      sh_show_regs = sh2a_show_regs;
      break;
    case bfd_mach_sh2a_nofpu:
      sh_show_regs = sh2a_nofpu_show_regs;
      break;
    case bfd_mach_sh_dsp:
      sh_show_regs = sh_dsp_show_regs;
      break;

    case bfd_mach_sh3:
      sh_show_regs = sh3_show_regs;
      break;

    case bfd_mach_sh3e:
      sh_show_regs = sh3e_show_regs;
      break;

    case bfd_mach_sh3_dsp:
    case bfd_mach_sh4al_dsp:
      sh_show_regs = sh3_dsp_show_regs;
      break;

    case bfd_mach_sh4:
    case bfd_mach_sh4a:
      sh_show_regs = sh4_show_regs;
      break;

    case bfd_mach_sh4_nofpu:
    case bfd_mach_sh4a_nofpu:
      sh_show_regs = sh4_nofpu_show_regs;
      break;

    case bfd_mach_sh5:
      sh_show_regs = sh64_show_regs;
      /* SH5 is handled entirely in sh64-tdep.c */
      return sh64_gdbarch_init (info, arches);
    }

  /* If there is already a candidate, use it.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  /* None found, create a new architecture from the information
     provided. */
  gdbarch = gdbarch_alloc (&info, NULL);

  set_gdbarch_short_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_int_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_long_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_float_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_long_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_ptr_bit (gdbarch, 4 * TARGET_CHAR_BIT);

  set_gdbarch_num_regs (gdbarch, SH_NUM_REGS);
  set_gdbarch_sp_regnum (gdbarch, 15);
  set_gdbarch_pc_regnum (gdbarch, 16);
  set_gdbarch_fp0_regnum (gdbarch, -1);
  set_gdbarch_num_pseudo_regs (gdbarch, 0);

  set_gdbarch_register_type (gdbarch, sh_default_register_type);
  set_gdbarch_register_reggroup_p (gdbarch, sh_register_reggroup_p);

  set_gdbarch_breakpoint_from_pc (gdbarch, sh_breakpoint_from_pc);

  set_gdbarch_print_insn (gdbarch, gdb_print_insn_sh);
  set_gdbarch_register_sim_regno (gdbarch, legacy_register_sim_regno);

  set_gdbarch_return_value (gdbarch, sh_return_value_nofpu);

  set_gdbarch_skip_prologue (gdbarch, sh_skip_prologue);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  set_gdbarch_push_dummy_call (gdbarch, sh_push_dummy_call_nofpu);

  set_gdbarch_believe_pcc_promotion (gdbarch, 1);

  set_gdbarch_frame_align (gdbarch, sh_frame_align);
  set_gdbarch_unwind_sp (gdbarch, sh_unwind_sp);
  set_gdbarch_unwind_pc (gdbarch, sh_unwind_pc);
  set_gdbarch_unwind_dummy_id (gdbarch, sh_unwind_dummy_id);
  frame_base_set_default (gdbarch, &sh_frame_base);

  set_gdbarch_in_function_epilogue_p (gdbarch, sh_in_function_epilogue_p);

  dwarf2_frame_set_init_reg (gdbarch, sh_dwarf2_frame_init_reg);

  switch (info.bfd_arch_info->mach)
    {
    case bfd_mach_sh:
      set_gdbarch_register_name (gdbarch, sh_sh_register_name);
      break;

    case bfd_mach_sh2:
      set_gdbarch_register_name (gdbarch, sh_sh_register_name);
      break;

    case bfd_mach_sh2e:
      /* doubles on sh2e and sh3e are actually 4 byte. */
      set_gdbarch_double_bit (gdbarch, 4 * TARGET_CHAR_BIT);

      set_gdbarch_register_name (gdbarch, sh_sh2e_register_name);
      set_gdbarch_register_type (gdbarch, sh_sh3e_register_type);
      set_gdbarch_fp0_regnum (gdbarch, 25);
      set_gdbarch_return_value (gdbarch, sh_return_value_fpu);
      set_gdbarch_push_dummy_call (gdbarch, sh_push_dummy_call_fpu);
      break;

    case bfd_mach_sh2a:
      set_gdbarch_register_name (gdbarch, sh_sh2a_register_name);
      set_gdbarch_register_type (gdbarch, sh_sh2a_register_type);
      set_gdbarch_register_sim_regno (gdbarch, sh_sh2a_register_sim_regno);

      set_gdbarch_fp0_regnum (gdbarch, 25);
      set_gdbarch_num_pseudo_regs (gdbarch, 9);
      set_gdbarch_pseudo_register_read (gdbarch, sh_pseudo_register_read);
      set_gdbarch_pseudo_register_write (gdbarch, sh_pseudo_register_write);
      set_gdbarch_return_value (gdbarch, sh_return_value_fpu);
      set_gdbarch_push_dummy_call (gdbarch, sh_push_dummy_call_fpu);
      break;

    case bfd_mach_sh2a_nofpu:
      set_gdbarch_register_name (gdbarch, sh_sh2a_nofpu_register_name);
      set_gdbarch_register_sim_regno (gdbarch, sh_sh2a_register_sim_regno);

      set_gdbarch_num_pseudo_regs (gdbarch, 1);
      set_gdbarch_pseudo_register_read (gdbarch, sh_pseudo_register_read);
      set_gdbarch_pseudo_register_write (gdbarch, sh_pseudo_register_write);
      break;

    case bfd_mach_sh_dsp:
      set_gdbarch_register_name (gdbarch, sh_sh_dsp_register_name);
      set_gdbarch_register_sim_regno (gdbarch, sh_dsp_register_sim_regno);
      break;

    case bfd_mach_sh3:
    case bfd_mach_sh3_nommu:
    case bfd_mach_sh2a_nofpu_or_sh3_nommu:
      set_gdbarch_register_name (gdbarch, sh_sh3_register_name);
      break;

    case bfd_mach_sh3e:
    case bfd_mach_sh2a_or_sh3e:
      /* doubles on sh2e and sh3e are actually 4 byte. */
      set_gdbarch_double_bit (gdbarch, 4 * TARGET_CHAR_BIT);

      set_gdbarch_register_name (gdbarch, sh_sh3e_register_name);
      set_gdbarch_register_type (gdbarch, sh_sh3e_register_type);
      set_gdbarch_fp0_regnum (gdbarch, 25);
      set_gdbarch_return_value (gdbarch, sh_return_value_fpu);
      set_gdbarch_push_dummy_call (gdbarch, sh_push_dummy_call_fpu);
      break;

    case bfd_mach_sh3_dsp:
      set_gdbarch_register_name (gdbarch, sh_sh3_dsp_register_name);
      set_gdbarch_register_sim_regno (gdbarch, sh_dsp_register_sim_regno);
      break;

    case bfd_mach_sh4:
    case bfd_mach_sh4a:
      set_gdbarch_register_name (gdbarch, sh_sh4_register_name);
      set_gdbarch_register_type (gdbarch, sh_sh4_register_type);
      set_gdbarch_fp0_regnum (gdbarch, 25);
      set_gdbarch_num_pseudo_regs (gdbarch, 13);
      set_gdbarch_pseudo_register_read (gdbarch, sh_pseudo_register_read);
      set_gdbarch_pseudo_register_write (gdbarch, sh_pseudo_register_write);
      set_gdbarch_return_value (gdbarch, sh_return_value_fpu);
      set_gdbarch_push_dummy_call (gdbarch, sh_push_dummy_call_fpu);
      break;

    case bfd_mach_sh4_nofpu:
    case bfd_mach_sh4a_nofpu:
    case bfd_mach_sh4_nommu_nofpu:
    case bfd_mach_sh2a_nofpu_or_sh4_nommu_nofpu:
    case bfd_mach_sh2a_or_sh4:
      set_gdbarch_register_name (gdbarch, sh_sh4_nofpu_register_name);
      break;

    case bfd_mach_sh4al_dsp:
      set_gdbarch_register_name (gdbarch, sh_sh4al_dsp_register_name);
      set_gdbarch_register_sim_regno (gdbarch, sh_dsp_register_sim_regno);
      break;

    default:
      set_gdbarch_register_name (gdbarch, sh_sh_register_name);
      break;
    }

  /* Hook in ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch);

  frame_unwind_append_sniffer (gdbarch, dwarf2_frame_sniffer);
  frame_unwind_append_sniffer (gdbarch, sh_frame_sniffer);

  return gdbarch;
}

extern initialize_file_ftype _initialize_sh_tdep;	/* -Wmissing-prototypes */

void
_initialize_sh_tdep (void)
{
  struct cmd_list_element *c;

  gdbarch_register (bfd_arch_sh, sh_gdbarch_init, NULL);

  add_com ("regs", class_vars, sh_show_regs_command, _("Print all registers"));
}
