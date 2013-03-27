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
#include "regcache.h"
#include "osabi.h"

#include "elf-bfd.h"

/* sh flags */
#include "elf/sh.h"
/* registers numbers shared with the simulator */
#include "gdb/sim-sh.h"

/* Information that is dependent on the processor variant.  */
enum sh_abi
  {
    SH_ABI_UNKNOWN,
    SH_ABI_32,
    SH_ABI_64
  };

struct gdbarch_tdep
  {
    enum sh_abi sh_abi;
  };

struct sh64_frame_cache
{
  /* Base address.  */
  CORE_ADDR base;
  LONGEST sp_offset;
  CORE_ADDR pc;

  /* Flag showing that a frame has been created in the prologue code. */
  int uses_fp;

  int media_mode;

  /* Saved registers.  */
  CORE_ADDR saved_regs[SIM_SH64_NR_REGS];
  CORE_ADDR saved_sp;
};

/* Registers of SH5 */
enum
  {
    R0_REGNUM = 0,
    DEFAULT_RETURN_REGNUM = 2,
    STRUCT_RETURN_REGNUM = 2,
    ARG0_REGNUM = 2,
    ARGLAST_REGNUM = 9,
    FLOAT_ARGLAST_REGNUM = 11,
    MEDIA_FP_REGNUM = 14,
    PR_REGNUM = 18,
    SR_REGNUM = 65,
    DR0_REGNUM = 141,
    DR_LAST_REGNUM = 172,
    /* FPP stands for Floating Point Pair, to avoid confusion with
       GDB's gdbarch_fp0_regnum, which is the number of the first Floating
       point register. Unfortunately on the sh5, the floating point
       registers are called FR, and the floating point pairs are called FP.  */
    FPP0_REGNUM = 173,
    FPP_LAST_REGNUM = 204,
    FV0_REGNUM = 205,
    FV_LAST_REGNUM = 220,
    R0_C_REGNUM = 221,
    R_LAST_C_REGNUM = 236,
    PC_C_REGNUM = 237,
    GBR_C_REGNUM = 238,
    MACH_C_REGNUM = 239,
    MACL_C_REGNUM = 240,
    PR_C_REGNUM = 241,
    T_C_REGNUM = 242,
    FPSCR_C_REGNUM = 243,
    FPUL_C_REGNUM = 244,
    FP0_C_REGNUM = 245,
    FP_LAST_C_REGNUM = 260,
    DR0_C_REGNUM = 261,
    DR_LAST_C_REGNUM = 268,
    FV0_C_REGNUM = 269,
    FV_LAST_C_REGNUM = 272,
    FPSCR_REGNUM = SIM_SH64_FPCSR_REGNUM,
    SSR_REGNUM = SIM_SH64_SSR_REGNUM,
    SPC_REGNUM = SIM_SH64_SPC_REGNUM,
    TR7_REGNUM = SIM_SH64_TR0_REGNUM + 7,
    FP_LAST_REGNUM = SIM_SH64_FR0_REGNUM + SIM_SH64_NR_FP_REGS - 1
  };

static const char *
sh64_register_name (int reg_nr)
{
  static char *register_names[] =
  {
    /* SH MEDIA MODE (ISA 32) */
    /* general registers (64-bit) 0-63 */
    "r0",   "r1",   "r2",   "r3",   "r4",   "r5",   "r6",   "r7",
    "r8",   "r9",   "r10",  "r11",  "r12",  "r13",  "r14",  "r15",
    "r16",  "r17",  "r18",  "r19",  "r20",  "r21",  "r22",  "r23",
    "r24",  "r25",  "r26",  "r27",  "r28",  "r29",  "r30",  "r31",
    "r32",  "r33",  "r34",  "r35",  "r36",  "r37",  "r38",  "r39",
    "r40",  "r41",  "r42",  "r43",  "r44",  "r45",  "r46",  "r47",
    "r48",  "r49",  "r50",  "r51",  "r52",  "r53",  "r54",  "r55",
    "r56",  "r57",  "r58",  "r59",  "r60",  "r61",  "r62",  "r63",

    /* pc (64-bit) 64 */
    "pc",   

    /* status reg., saved status reg., saved pc reg. (64-bit) 65-67 */
    "sr",  "ssr",  "spc", 

    /* target registers (64-bit) 68-75*/
    "tr0",  "tr1",  "tr2",  "tr3",  "tr4",  "tr5",  "tr6",  "tr7",

    /* floating point state control register (32-bit) 76 */
    "fpscr",

    /* single precision floating point registers (32-bit) 77-140*/
    "fr0",  "fr1",  "fr2",  "fr3",  "fr4",  "fr5",  "fr6",  "fr7",
    "fr8",  "fr9",  "fr10", "fr11", "fr12", "fr13", "fr14", "fr15",
    "fr16", "fr17", "fr18", "fr19", "fr20", "fr21", "fr22", "fr23",
    "fr24", "fr25", "fr26", "fr27", "fr28", "fr29", "fr30", "fr31",
    "fr32", "fr33", "fr34", "fr35", "fr36", "fr37", "fr38", "fr39",
    "fr40", "fr41", "fr42", "fr43", "fr44", "fr45", "fr46", "fr47",
    "fr48", "fr49", "fr50", "fr51", "fr52", "fr53", "fr54", "fr55",
    "fr56", "fr57", "fr58", "fr59", "fr60", "fr61", "fr62", "fr63",

    /* double precision registers (pseudo) 141-172 */
    "dr0",  "dr2",  "dr4",  "dr6",  "dr8",  "dr10", "dr12", "dr14",
    "dr16", "dr18", "dr20", "dr22", "dr24", "dr26", "dr28", "dr30",
    "dr32", "dr34", "dr36", "dr38", "dr40", "dr42", "dr44", "dr46",
    "dr48", "dr50", "dr52", "dr54", "dr56", "dr58", "dr60", "dr62",

    /* floating point pairs (pseudo) 173-204*/
    "fp0",  "fp2",  "fp4",  "fp6",  "fp8",  "fp10", "fp12", "fp14",
    "fp16", "fp18", "fp20", "fp22", "fp24", "fp26", "fp28", "fp30",
    "fp32", "fp34", "fp36", "fp38", "fp40", "fp42", "fp44", "fp46",
    "fp48", "fp50", "fp52", "fp54", "fp56", "fp58", "fp60", "fp62",

    /* floating point vectors (4 floating point regs) (pseudo) 205-220*/
    "fv0",  "fv4",  "fv8",  "fv12", "fv16", "fv20", "fv24", "fv28",
    "fv32", "fv36", "fv40", "fv44", "fv48", "fv52", "fv56", "fv60",

    /* SH COMPACT MODE (ISA 16) (all pseudo) 221-272*/
    "r0_c", "r1_c", "r2_c",  "r3_c",  "r4_c",  "r5_c",  "r6_c",  "r7_c",
    "r8_c", "r9_c", "r10_c", "r11_c", "r12_c", "r13_c", "r14_c", "r15_c",
    "pc_c",
    "gbr_c", "mach_c", "macl_c", "pr_c", "t_c",
    "fpscr_c", "fpul_c",
    "fr0_c", "fr1_c", "fr2_c",  "fr3_c",  "fr4_c",  "fr5_c",  "fr6_c",  "fr7_c",
    "fr8_c", "fr9_c", "fr10_c", "fr11_c", "fr12_c", "fr13_c", "fr14_c", "fr15_c",
    "dr0_c", "dr2_c", "dr4_c",  "dr6_c",  "dr8_c",  "dr10_c", "dr12_c", "dr14_c",
    "fv0_c", "fv4_c", "fv8_c",  "fv12_c",
    /* FIXME!!!! XF0 XF15, XD0 XD14 ?????*/
  };

  if (reg_nr < 0)
    return NULL;
  if (reg_nr >= (sizeof (register_names) / sizeof (*register_names)))
    return NULL;
  return register_names[reg_nr];
}

#define NUM_PSEUDO_REGS_SH_MEDIA 80
#define NUM_PSEUDO_REGS_SH_COMPACT 51

/* Macros and functions for setting and testing a bit in a minimal
   symbol that marks it as 32-bit function.  The MSB of the minimal
   symbol's "info" field is used for this purpose.

   gdbarch_elf_make_msymbol_special tests whether an ELF symbol is "special",
   i.e. refers to a 32-bit function, and sets a "special" bit in a
   minimal symbol to mark it as a 32-bit function
   MSYMBOL_IS_SPECIAL   tests the "special" bit in a minimal symbol  */

#define MSYMBOL_IS_SPECIAL(msym) \
  (((long) MSYMBOL_INFO (msym) & 0x80000000) != 0)

static void
sh64_elf_make_msymbol_special (asymbol *sym, struct minimal_symbol *msym)
{
  if (msym == NULL)
    return;

  if (((elf_symbol_type *)(sym))->internal_elf_sym.st_other == STO_SH5_ISA32)
    {
      MSYMBOL_INFO (msym) = (char *) (((long) MSYMBOL_INFO (msym)) | 0x80000000);
      SYMBOL_VALUE_ADDRESS (msym) |= 1;
    }
}

/* ISA32 (shmedia) function addresses are odd (bit 0 is set).  Here
   are some macros to test, set, or clear bit 0 of addresses.  */
#define IS_ISA32_ADDR(addr)	 ((addr) & 1)
#define MAKE_ISA32_ADDR(addr)	 ((addr) | 1)
#define UNMAKE_ISA32_ADDR(addr)  ((addr) & ~1)

static int
pc_is_isa32 (bfd_vma memaddr)
{
  struct minimal_symbol *sym;

  /* If bit 0 of the address is set, assume this is a
     ISA32 (shmedia) address.  */
  if (IS_ISA32_ADDR (memaddr))
    return 1;

  /* A flag indicating that this is a ISA32 function is stored by elfread.c in
     the high bit of the info field.  Use this to decide if the function is
     ISA16 or ISA32.  */
  sym = lookup_minimal_symbol_by_pc (memaddr);
  if (sym)
    return MSYMBOL_IS_SPECIAL (sym);
  else
    return 0;
}

static const unsigned char *
sh64_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  /* The BRK instruction for shmedia is 
     01101111 11110101 11111111 11110000
     which translates in big endian mode to 0x6f, 0xf5, 0xff, 0xf0
     and in little endian mode to 0xf0, 0xff, 0xf5, 0x6f */

  /* The BRK instruction for shcompact is
     00000000 00111011
     which translates in big endian mode to 0x0, 0x3b
     and in little endian mode to 0x3b, 0x0*/

  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
    {
      if (pc_is_isa32 (*pcptr))
	{
	  static unsigned char big_breakpoint_media[] = {0x6f, 0xf5, 0xff, 0xf0};
	  *pcptr = UNMAKE_ISA32_ADDR (*pcptr);
	  *lenptr = sizeof (big_breakpoint_media);
	  return big_breakpoint_media;
	}
      else
	{
	  static unsigned char big_breakpoint_compact[] = {0x0, 0x3b};
	  *lenptr = sizeof (big_breakpoint_compact);
	  return big_breakpoint_compact;
	}
    }
  else
    {
      if (pc_is_isa32 (*pcptr))
	{
	  static unsigned char little_breakpoint_media[] = {0xf0, 0xff, 0xf5, 0x6f};
	  *pcptr = UNMAKE_ISA32_ADDR (*pcptr);
	  *lenptr = sizeof (little_breakpoint_media);
	  return little_breakpoint_media;
	}
      else
	{
	  static unsigned char little_breakpoint_compact[] = {0x3b, 0x0};
	  *lenptr = sizeof (little_breakpoint_compact);
	  return little_breakpoint_compact;
	}
    }
}

/* Prologue looks like
   [mov.l       <regs>,@-r15]...
   [sts.l       pr,@-r15]
   [mov.l       r14,@-r15]
   [mov         r15,r14]

   Actually it can be more complicated than this.  For instance, with
   newer gcc's:

   mov.l   r14,@-r15
   add     #-12,r15
   mov     r15,r14
   mov     r4,r1
   mov     r5,r2
   mov.l   r6,@(4,r14)
   mov.l   r7,@(8,r14)
   mov.b   r1,@r14
   mov     r14,r1
   mov     r14,r1
   add     #2,r1
   mov.w   r2,@r1

 */

/* PTABS/L Rn, TRa       0110101111110001nnnnnnl00aaa0000 
   with l=1 and n = 18   0110101111110001010010100aaa0000 */
#define IS_PTABSL_R18(x)  (((x) & 0xffffff8f) == 0x6bf14a00)

/* STS.L PR,@-r0   0100000000100010
   r0-4-->r0, PR-->(r0) */
#define IS_STS_R0(x)  		((x) == 0x4022)

/* STS PR, Rm      0000mmmm00101010
   PR-->Rm */
#define IS_STS_PR(x)            (((x) & 0xf0ff) == 0x2a)

/* MOV.L Rm,@(disp,r15)  00011111mmmmdddd
   Rm-->(dispx4+r15) */
#define IS_MOV_TO_R15(x)              (((x) & 0xff00) == 0x1f00)

/* MOV.L R14,@(disp,r15)  000111111110dddd
   R14-->(dispx4+r15) */
#define IS_MOV_R14(x)              (((x) & 0xfff0) == 0x1fe0)

/* ST.Q R14, disp, R18    101011001110dddddddddd0100100000
   R18-->(dispx8+R14) */
#define IS_STQ_R18_R14(x)          (((x) & 0xfff003ff) == 0xace00120)

/* ST.Q R15, disp, R18    101011001111dddddddddd0100100000
   R18-->(dispx8+R15) */
#define IS_STQ_R18_R15(x)          (((x) & 0xfff003ff) == 0xacf00120)

/* ST.L R15, disp, R18    101010001111dddddddddd0100100000
   R18-->(dispx4+R15) */
#define IS_STL_R18_R15(x)          (((x) & 0xfff003ff) == 0xa8f00120)

/* ST.Q R15, disp, R14    1010 1100 1111 dddd dddd dd00 1110 0000
   R14-->(dispx8+R15) */
#define IS_STQ_R14_R15(x)          (((x) & 0xfff003ff) == 0xacf000e0)

/* ST.L R15, disp, R14    1010 1000 1111 dddd dddd dd00 1110 0000
   R14-->(dispx4+R15) */
#define IS_STL_R14_R15(x)          (((x) & 0xfff003ff) == 0xa8f000e0)

/* ADDI.L R15,imm,R15     1101 0100 1111 ssss ssss ss00 1111 0000
   R15 + imm --> R15 */
#define IS_ADDIL_SP_MEDIA(x)         (((x) & 0xfff003ff) == 0xd4f000f0)

/* ADDI R15,imm,R15     1101 0000 1111 ssss ssss ss00 1111 0000
   R15 + imm --> R15 */
#define IS_ADDI_SP_MEDIA(x)         (((x) & 0xfff003ff) == 0xd0f000f0)

/* ADD.L R15,R63,R14    0000 0000 1111 1000 1111 1100 1110 0000 
   R15 + R63 --> R14 */
#define IS_ADDL_SP_FP_MEDIA(x)  	((x) == 0x00f8fce0)

/* ADD R15,R63,R14    0000 0000 1111 1001 1111 1100 1110 0000 
   R15 + R63 --> R14 */
#define IS_ADD_SP_FP_MEDIA(x)  	((x) == 0x00f9fce0)

#define IS_MOV_SP_FP_MEDIA(x)  	(IS_ADDL_SP_FP_MEDIA(x) || IS_ADD_SP_FP_MEDIA(x))

/* MOV #imm, R0    1110 0000 ssss ssss 
   #imm-->R0 */
#define IS_MOV_R0(x) 		(((x) & 0xff00) == 0xe000)

/* MOV.L @(disp,PC), R0    1101 0000 iiii iiii  */
#define IS_MOVL_R0(x) 		(((x) & 0xff00) == 0xd000)

/* ADD r15,r0      0011 0000 1111 1100
   r15+r0-->r0 */
#define IS_ADD_SP_R0(x)	        ((x) == 0x30fc)

/* MOV.L R14 @-R0  0010 0000 1110 0110
   R14-->(R0-4), R0-4-->R0 */
#define IS_MOV_R14_R0(x)        ((x) == 0x20e6)

/* ADD Rm,R63,Rn  Rm+R63-->Rn  0000 00mm mmmm 1001 1111 11nn nnnn 0000
   where Rm is one of r2-r9 which are the argument registers.  */
/* FIXME: Recognize the float and double register moves too! */
#define IS_MEDIA_IND_ARG_MOV(x) \
((((x) & 0xfc0ffc0f) == 0x0009fc00) && (((x) & 0x03f00000) >= 0x00200000 && ((x) & 0x03f00000) <= 0x00900000))

/* ST.Q Rn,0,Rm  Rm-->Rn+0  1010 11nn nnnn 0000 0000 00mm mmmm 0000
   or ST.L Rn,0,Rm  Rm-->Rn+0  1010 10nn nnnn 0000 0000 00mm mmmm 0000
   where Rm is one of r2-r9 which are the argument registers.  */
#define IS_MEDIA_ARG_MOV(x) \
(((((x) & 0xfc0ffc0f) == 0xac000000) || (((x) & 0xfc0ffc0f) == 0xa8000000)) \
   && (((x) & 0x000003f0) >= 0x00000020 && ((x) & 0x000003f0) <= 0x00000090))

/* ST.B R14,0,Rn     Rn-->(R14+0) 1010 0000 1110 0000 0000 00nn nnnn 0000*/
/* ST.W R14,0,Rn     Rn-->(R14+0) 1010 0100 1110 0000 0000 00nn nnnn 0000*/
/* ST.L R14,0,Rn     Rn-->(R14+0) 1010 1000 1110 0000 0000 00nn nnnn 0000*/
/* FST.S R14,0,FRn   Rn-->(R14+0) 1011 0100 1110 0000 0000 00nn nnnn 0000*/
/* FST.D R14,0,DRn   Rn-->(R14+0) 1011 1100 1110 0000 0000 00nn nnnn 0000*/
#define IS_MEDIA_MOV_TO_R14(x)  \
((((x) & 0xfffffc0f) == 0xa0e00000) \
|| (((x) & 0xfffffc0f) == 0xa4e00000) \
|| (((x) & 0xfffffc0f) == 0xa8e00000) \
|| (((x) & 0xfffffc0f) == 0xb4e00000) \
|| (((x) & 0xfffffc0f) == 0xbce00000))

/* MOV Rm, Rn  Rm-->Rn 0110 nnnn mmmm 0011
   where Rm is r2-r9 */
#define IS_COMPACT_IND_ARG_MOV(x) \
((((x) & 0xf00f) == 0x6003) && (((x) & 0x00f0) >= 0x0020) && (((x) & 0x00f0) <= 0x0090))

/* compact direct arg move! 
   MOV.L Rn, @r14     0010 1110 mmmm 0010 */
#define IS_COMPACT_ARG_MOV(x) \
(((((x) & 0xff0f) == 0x2e02) && (((x) & 0x00f0) >= 0x0020) && ((x) & 0x00f0) <= 0x0090))

/* MOV.B Rm, @R14     0010 1110 mmmm 0000 
   MOV.W Rm, @R14     0010 1110 mmmm 0001 */
#define IS_COMPACT_MOV_TO_R14(x) \
((((x) & 0xff0f) == 0x2e00) || (((x) & 0xff0f) == 0x2e01))

#define IS_JSR_R0(x)           ((x) == 0x400b)
#define IS_NOP(x)              ((x) == 0x0009)


/* MOV r15,r14     0110111011110011
   r15-->r14  */
#define IS_MOV_SP_FP(x)  	((x) == 0x6ef3)

/* ADD #imm,r15    01111111iiiiiiii
   r15+imm-->r15 */
#define IS_ADD_SP(x) 		(((x) & 0xff00) == 0x7f00)

/* Skip any prologue before the guts of a function */

/* Skip the prologue using the debug information.  If this fails we'll
   fall back on the 'guess' method below.  */
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
look_for_args_moves (CORE_ADDR start_pc, int media_mode)
{
  CORE_ADDR here, end;
  int w;
  int insn_size = (media_mode ? 4 : 2);

  for (here = start_pc, end = start_pc + (insn_size * 28); here < end;)
    {
      if (media_mode)
	{
	  w = read_memory_integer (UNMAKE_ISA32_ADDR (here), insn_size);
	  here += insn_size;
	  if (IS_MEDIA_IND_ARG_MOV (w))
	    {
	      /* This must be followed by a store to r14, so the argument
		 is where the debug info says it is. This can happen after
		 the SP has been saved, unfortunately.  */
	 
	      int next_insn = read_memory_integer (UNMAKE_ISA32_ADDR (here),
						   insn_size);
	      here += insn_size;
	      if (IS_MEDIA_MOV_TO_R14 (next_insn))
		start_pc = here;	  
	    }
	  else if (IS_MEDIA_ARG_MOV (w))
	    {
	      /* These instructions store directly the argument in r14.  */
	      start_pc = here;
	    }
	  else
	    break;
	}
      else
	{
	  w = read_memory_integer (here, insn_size);
	  w = w & 0xffff;
	  here += insn_size;
	  if (IS_COMPACT_IND_ARG_MOV (w))
	    {
	      /* This must be followed by a store to r14, so the argument
		 is where the debug info says it is. This can happen after
		 the SP has been saved, unfortunately.  */
	 
	      int next_insn = 0xffff & read_memory_integer (here, insn_size);
	      here += insn_size;
	      if (IS_COMPACT_MOV_TO_R14 (next_insn))
		start_pc = here;
	    }
	  else if (IS_COMPACT_ARG_MOV (w))
	    {
	      /* These instructions store directly the argument in r14.  */
	      start_pc = here;
	    }
	  else if (IS_MOVL_R0 (w))
	    {
	      /* There is a function that gcc calls to get the arguments
		 passed correctly to the function. Only after this
		 function call the arguments will be found at the place
		 where they are supposed to be. This happens in case the
		 argument has to be stored into a 64-bit register (for
		 instance doubles, long longs).  SHcompact doesn't have
		 access to the full 64-bits, so we store the register in
		 stack slot and store the address of the stack slot in
		 the register, then do a call through a wrapper that
		 loads the memory value into the register.  A SHcompact
		 callee calls an argument decoder
		 (GCC_shcompact_incoming_args) that stores the 64-bit
		 value in a stack slot and stores the address of the
		 stack slot in the register.  GCC thinks the argument is
		 just passed by transparent reference, but this is only
		 true after the argument decoder is called. Such a call
		 needs to be considered part of the prologue.  */

	      /* This must be followed by a JSR @r0 instruction and by
                 a NOP instruction. After these, the prologue is over!  */
	 
	      int next_insn = 0xffff & read_memory_integer (here, insn_size);
	      here += insn_size;
	      if (IS_JSR_R0 (next_insn))
		{
		  next_insn = 0xffff & read_memory_integer (here, insn_size);
		  here += insn_size;

		  if (IS_NOP (next_insn))
		    start_pc = here;
		}
	    }
	  else
	    break;
	}
    }

  return start_pc;
}

static CORE_ADDR
sh64_skip_prologue_hard_way (CORE_ADDR start_pc)
{
  CORE_ADDR here, end;
  int updated_fp = 0;
  int insn_size = 4;
  int media_mode = 1;

  if (!start_pc)
    return 0;

  if (pc_is_isa32 (start_pc) == 0)
    {
      insn_size = 2;
      media_mode = 0;
    }

  for (here = start_pc, end = start_pc + (insn_size * 28); here < end;)
    {

      if (media_mode)
	{
	  int w = read_memory_integer (UNMAKE_ISA32_ADDR (here), insn_size);
	  here += insn_size;
	  if (IS_STQ_R18_R14 (w) || IS_STQ_R18_R15 (w) || IS_STQ_R14_R15 (w)
	      || IS_STL_R14_R15 (w) || IS_STL_R18_R15 (w)
	      || IS_ADDIL_SP_MEDIA (w) || IS_ADDI_SP_MEDIA (w) || IS_PTABSL_R18 (w))
	    {
	      start_pc = here;
	    }
	  else if (IS_MOV_SP_FP (w) || IS_MOV_SP_FP_MEDIA(w))
	    {
	      start_pc = here;
	      updated_fp = 1;
	    }
	  else
	    if (updated_fp)
	      {
		/* Don't bail out yet, we may have arguments stored in
		   registers here, according to the debug info, so that
		   gdb can print the frames correctly.  */
		start_pc = look_for_args_moves (here - insn_size, media_mode);
		break;
	      }
	}
      else
	{
	  int w = 0xffff & read_memory_integer (here, insn_size);
	  here += insn_size;

	  if (IS_STS_R0 (w) || IS_STS_PR (w)
	      || IS_MOV_TO_R15 (w) || IS_MOV_R14 (w) 
	      || IS_MOV_R0 (w) || IS_ADD_SP_R0 (w) || IS_MOV_R14_R0 (w))
	    {
	      start_pc = here;
	    }
	  else if (IS_MOV_SP_FP (w))
	    {
	      start_pc = here;
	      updated_fp = 1;
	    }
	  else
	    if (updated_fp)
	      {
		/* Don't bail out yet, we may have arguments stored in
		   registers here, according to the debug info, so that
		   gdb can print the frames correctly.  */
		start_pc = look_for_args_moves (here - insn_size, media_mode);
		break;
	      }
	}
    }

  return start_pc;
}

static CORE_ADDR
sh64_skip_prologue (CORE_ADDR pc)
{
  CORE_ADDR post_prologue_pc;

  /* See if we can determine the end of the prologue via the symbol table.
     If so, then return either PC, or the PC after the prologue, whichever
     is greater.  */
  post_prologue_pc = after_prologue (pc);

  /* If after_prologue returned a useful address, then use it.  Else
     fall back on the instruction skipping code.  */
  if (post_prologue_pc != 0)
    return max (pc, post_prologue_pc);
  else
    return sh64_skip_prologue_hard_way (pc);
}

/* Should call_function allocate stack space for a struct return?  */
static int
sh64_use_struct_convention (struct type *type)
{
  return (TYPE_LENGTH (type) > 8);
}

/* Disassemble an instruction.  */
static int
gdb_print_insn_sh64 (bfd_vma memaddr, disassemble_info *info)
{
  info->endian = gdbarch_byte_order (current_gdbarch);
  return print_insn_sh (memaddr, info);
}

/* For vectors of 4 floating point registers.  */
static int
sh64_fv_reg_base_num (int fv_regnum)
{
  int fp_regnum;

  fp_regnum = gdbarch_fp0_regnum (current_gdbarch) +
    (fv_regnum - FV0_REGNUM) * 4;
  return fp_regnum;
}

/* For double precision floating point registers, i.e 2 fp regs.*/
static int
sh64_dr_reg_base_num (int dr_regnum)
{
  int fp_regnum;

  fp_regnum = gdbarch_fp0_regnum (current_gdbarch) +
    (dr_regnum - DR0_REGNUM) * 2;
  return fp_regnum;
}

/* For pairs of floating point registers */
static int
sh64_fpp_reg_base_num (int fpp_regnum)
{
  int fp_regnum;

  fp_regnum = gdbarch_fp0_regnum (current_gdbarch) +
    (fpp_regnum - FPP0_REGNUM) * 2;
  return fp_regnum;
}

/* *INDENT-OFF* */
/*
    SH COMPACT MODE (ISA 16) (all pseudo) 221-272
       GDB_REGNUM  BASE_REGNUM
 r0_c       221      0
 r1_c       222      1
 r2_c       223      2
 r3_c       224      3
 r4_c       225      4
 r5_c       226      5
 r6_c       227      6
 r7_c       228      7
 r8_c       229      8
 r9_c       230      9
 r10_c      231      10
 r11_c      232      11
 r12_c      233      12
 r13_c      234      13
 r14_c      235      14
 r15_c      236      15

 pc_c       237      64
 gbr_c      238      16
 mach_c     239      17
 macl_c     240      17
 pr_c       241      18
 t_c        242      19
 fpscr_c    243      76
 fpul_c     244      109

 fr0_c      245      77
 fr1_c      246      78
 fr2_c      247      79
 fr3_c      248      80
 fr4_c      249      81
 fr5_c      250      82
 fr6_c      251      83
 fr7_c      252      84
 fr8_c      253      85
 fr9_c      254      86
 fr10_c     255      87
 fr11_c     256      88
 fr12_c     257      89
 fr13_c     258      90
 fr14_c     259      91
 fr15_c     260      92

 dr0_c      261      77
 dr2_c      262      79
 dr4_c      263      81
 dr6_c      264      83
 dr8_c      265      85
 dr10_c     266      87
 dr12_c     267      89
 dr14_c     268      91

 fv0_c      269      77
 fv4_c      270      81
 fv8_c      271      85
 fv12_c     272      91
*/
/* *INDENT-ON* */
static int
sh64_compact_reg_base_num (int reg_nr)
{
  int base_regnum = reg_nr;

  /* general register N maps to general register N */
  if (reg_nr >= R0_C_REGNUM 
      && reg_nr <= R_LAST_C_REGNUM)
    base_regnum = reg_nr - R0_C_REGNUM;

  /* floating point register N maps to floating point register N */
  else if (reg_nr >= FP0_C_REGNUM 
	    && reg_nr <= FP_LAST_C_REGNUM)
    base_regnum = reg_nr - FP0_C_REGNUM + gdbarch_fp0_regnum (current_gdbarch);

  /* double prec register N maps to base regnum for double prec register N */
  else if (reg_nr >= DR0_C_REGNUM 
	    && reg_nr <= DR_LAST_C_REGNUM)
    base_regnum = sh64_dr_reg_base_num (DR0_REGNUM + reg_nr - DR0_C_REGNUM);

  /* vector N maps to base regnum for vector register N */
  else if (reg_nr >= FV0_C_REGNUM 
	    && reg_nr <= FV_LAST_C_REGNUM)
    base_regnum = sh64_fv_reg_base_num (FV0_REGNUM + reg_nr - FV0_C_REGNUM);

  else if (reg_nr == PC_C_REGNUM)
    base_regnum = gdbarch_pc_regnum (current_gdbarch);

  else if (reg_nr == GBR_C_REGNUM) 
    base_regnum = 16;

  else if (reg_nr == MACH_C_REGNUM
	   || reg_nr == MACL_C_REGNUM)
    base_regnum = 17;

  else if (reg_nr == PR_C_REGNUM) 
    base_regnum = PR_REGNUM;

  else if (reg_nr == T_C_REGNUM) 
    base_regnum = 19;

  else if (reg_nr == FPSCR_C_REGNUM) 
    base_regnum = FPSCR_REGNUM; /*???? this register is a mess.  */

  else if (reg_nr == FPUL_C_REGNUM) 
    base_regnum = gdbarch_fp0_regnum (current_gdbarch) + 32;
  
  return base_regnum;
}

static int
sign_extend (int value, int bits)
{
  value = value & ((1 << bits) - 1);
  return (value & (1 << (bits - 1))
	  ? value | (~((1 << bits) - 1))
	  : value);
}

static void
sh64_analyze_prologue (struct gdbarch *gdbarch,
		       struct sh64_frame_cache *cache,
		       CORE_ADDR func_pc,
		       CORE_ADDR current_pc)
{
  int reg_nr;
  int pc;
  int opc;
  int insn;
  int r0_val = 0;
  int insn_size;
  int gdb_register_number;
  int register_number;
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  
  cache->sp_offset = 0;

  /* Loop around examining the prologue insns until we find something
     that does not appear to be part of the prologue.  But give up
     after 20 of them, since we're getting silly then.  */

  pc = func_pc;

  if (cache->media_mode)
    insn_size = 4;
  else
    insn_size = 2;

  opc = pc + (insn_size * 28);
  if (opc > current_pc)
    opc = current_pc;
  for ( ; pc <= opc; pc += insn_size)
    {
      insn = read_memory_integer (cache->media_mode ? UNMAKE_ISA32_ADDR (pc)
						    : pc,
				  insn_size);

      if (!cache->media_mode)
	{
	  if (IS_STS_PR (insn))
	    {
	      int next_insn = read_memory_integer (pc + insn_size, insn_size);
	      if (IS_MOV_TO_R15 (next_insn))
		{
		  cache->saved_regs[PR_REGNUM] =
		    cache->sp_offset - ((((next_insn & 0xf) ^ 0x8) - 0x8) << 2);
		  pc += insn_size;
		}
	    }

	  else if (IS_MOV_R14 (insn))
	    cache->saved_regs[MEDIA_FP_REGNUM] =
	      cache->sp_offset - ((((insn & 0xf) ^ 0x8) - 0x8) << 2);

	  else if (IS_MOV_R0 (insn))
	    {
	      /* Put in R0 the offset from SP at which to store some
		 registers. We are interested in this value, because it
		 will tell us where the given registers are stored within
		 the frame.  */
	      r0_val = ((insn & 0xff) ^ 0x80) - 0x80;
	    }

	  else if (IS_ADD_SP_R0 (insn))
	    {
	      /* This instruction still prepares r0, but we don't care.
		 We already have the offset in r0_val.  */
	    }

	  else if (IS_STS_R0 (insn))
	    {
	      /* Store PR at r0_val-4 from SP. Decrement r0 by 4*/
	      cache->saved_regs[PR_REGNUM] = cache->sp_offset - (r0_val - 4);
	      r0_val -= 4;
	    }

	  else if (IS_MOV_R14_R0 (insn))
	    {
	      /* Store R14 at r0_val-4 from SP. Decrement r0 by 4 */
	      cache->saved_regs[MEDIA_FP_REGNUM] = cache->sp_offset
	      					   - (r0_val - 4);
	      r0_val -= 4;
	    }

	  else if (IS_ADD_SP (insn))
	    cache->sp_offset -= ((insn & 0xff) ^ 0x80) - 0x80;

	  else if (IS_MOV_SP_FP (insn))
	    break;
	}
      else
	{
	  if (IS_ADDIL_SP_MEDIA (insn) || IS_ADDI_SP_MEDIA (insn))
	    cache->sp_offset -=
	      sign_extend ((((insn & 0xffc00) ^ 0x80000) - 0x80000) >> 10, 9);

	  else if (IS_STQ_R18_R15 (insn))
	    cache->saved_regs[PR_REGNUM] = 
	      cache->sp_offset - (sign_extend ((insn & 0xffc00) >> 10, 9) << 3);

	  else if (IS_STL_R18_R15 (insn))
	    cache->saved_regs[PR_REGNUM] = 
	      cache->sp_offset - (sign_extend ((insn & 0xffc00) >> 10, 9) << 2);

	  else if (IS_STQ_R14_R15 (insn))
	    cache->saved_regs[MEDIA_FP_REGNUM] =
	      cache->sp_offset - (sign_extend ((insn & 0xffc00) >> 10, 9) << 3);

	  else if (IS_STL_R14_R15 (insn))
	    cache->saved_regs[MEDIA_FP_REGNUM] =
	      cache->sp_offset - (sign_extend ((insn & 0xffc00) >> 10, 9) << 2);

	  else if (IS_MOV_SP_FP_MEDIA (insn))
	    break;
	}
    }

  if (cache->saved_regs[MEDIA_FP_REGNUM] >= 0)
    cache->uses_fp = 1;
}

static CORE_ADDR
sh64_frame_align (struct gdbarch *ignore, CORE_ADDR sp)
{
  return sp & ~7;
}

/* Function: push_dummy_call
   Setup the function arguments for calling a function in the inferior.

   On the Renesas SH architecture, there are four registers (R4 to R7)
   which are dedicated for passing function arguments.  Up to the first
   four arguments (depending on size) may go into these registers.
   The rest go on the stack.

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

/* R2-R9 for integer types and integer equivalent (char, pointers) and
   non-scalar (struct, union) elements (even if the elements are
   floats).  
   FR0-FR11 for single precision floating point (float)
   DR0-DR10 for double precision floating point (double) 
   
   If a float is argument number 3 (for instance) and arguments number
   1,2, and 4 are integer, the mapping will be:
   arg1 -->R2, arg2 --> R3, arg3 -->FR0, arg4 --> R5. I.e. R4 is not used.
   
   If a float is argument number 10 (for instance) and arguments number
   1 through 10 are integer, the mapping will be:
   arg1->R2, arg2->R3, arg3->R4, arg4->R5, arg5->R6, arg6->R7, arg7->R8,
   arg8->R9, arg9->(0,SP)stack(8-byte aligned), arg10->FR0, arg11->stack(16,SP).
   I.e. there is hole in the stack.

   Different rules apply for variable arguments functions, and for functions
   for which the prototype is not known.  */

static CORE_ADDR
sh64_push_dummy_call (struct gdbarch *gdbarch,
		      struct value *function,
		      struct regcache *regcache,
		      CORE_ADDR bp_addr,
		      int nargs, struct value **args,
		      CORE_ADDR sp, int struct_return,
		      CORE_ADDR struct_addr)
{
  int stack_offset, stack_alloc;
  int int_argreg;
  int float_argreg;
  int double_argreg;
  int float_arg_index = 0;
  int double_arg_index = 0;
  int argnum;
  struct type *type;
  CORE_ADDR regval;
  char *val;
  char valbuf[8];
  char valbuf_tmp[8];
  int len;
  int argreg_size;
  int fp_args[12];

  memset (fp_args, 0, sizeof (fp_args));

  /* first force sp to a 8-byte alignment */
  sp = sh64_frame_align (gdbarch, sp);

  /* The "struct return pointer" pseudo-argument has its own dedicated 
     register */

  if (struct_return)
    regcache_cooked_write_unsigned (regcache, 
				    STRUCT_RETURN_REGNUM, struct_addr);

  /* Now make sure there's space on the stack */
  for (argnum = 0, stack_alloc = 0; argnum < nargs; argnum++)
    stack_alloc += ((TYPE_LENGTH (value_type (args[argnum])) + 7) & ~7);
  sp -= stack_alloc;		/* make room on stack for args */

  /* Now load as many as possible of the first arguments into
     registers, and push the rest onto the stack.  There are 64 bytes
     in eight registers available.  Loop thru args from first to last.  */

  int_argreg = ARG0_REGNUM;
  float_argreg = gdbarch_fp0_regnum (current_gdbarch);
  double_argreg = DR0_REGNUM;

  for (argnum = 0, stack_offset = 0; argnum < nargs; argnum++)
    {
      type = value_type (args[argnum]);
      len = TYPE_LENGTH (type);
      memset (valbuf, 0, sizeof (valbuf));
      
      if (TYPE_CODE (type) != TYPE_CODE_FLT)
	{
	  argreg_size = register_size (current_gdbarch, int_argreg);

	  if (len < argreg_size)
	    {
	      /* value gets right-justified in the register or stack word */
	      if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
		memcpy (valbuf + argreg_size - len,
			(char *) value_contents (args[argnum]), len);
	      else
		memcpy (valbuf, (char *) value_contents (args[argnum]), len);

	      val = valbuf;
	    }
	  else
	    val = (char *) value_contents (args[argnum]);

	  while (len > 0)
	    {
	      if (int_argreg > ARGLAST_REGNUM)
		{			
		  /* must go on the stack */
		  write_memory (sp + stack_offset, (const bfd_byte *) val,
		  		argreg_size);
		  stack_offset += 8;/*argreg_size;*/
		}
	      /* NOTE WELL!!!!!  This is not an "else if" clause!!!
		 That's because some *&^%$ things get passed on the stack
		 AND in the registers!   */
	      if (int_argreg <= ARGLAST_REGNUM)
		{			
		  /* there's room in a register */
		  regval = extract_unsigned_integer (val, argreg_size);
		  regcache_cooked_write_unsigned (regcache, int_argreg, regval);
		}
	      /* Store the value 8 bytes at a time.  This means that
		 things larger than 8 bytes may go partly in registers
		 and partly on the stack. FIXME: argreg is incremented
		 before we use its size.  */
	      len -= argreg_size;
	      val += argreg_size;
	      int_argreg++;
	    }
	}
      else
	{
	  val = (char *) value_contents (args[argnum]);
	  if (len == 4)
	    {
	      /* Where is it going to be stored? */
	      while (fp_args[float_arg_index])
		float_arg_index ++;

	      /* Now float_argreg points to the register where it
		 should be stored.  Are we still within the allowed
		 register set? */
	      if (float_arg_index <= FLOAT_ARGLAST_REGNUM)
		{
		  /* Goes in FR0...FR11 */
		  regcache_cooked_write (regcache,
					 gdbarch_fp0_regnum (current_gdbarch)
					 + float_arg_index,
					 val);
		  fp_args[float_arg_index] = 1;
		  /* Skip the corresponding general argument register.  */
		  int_argreg ++;
		}
	      else 
		;
		/* Store it as the integers, 8 bytes at the time, if
		   necessary spilling on the stack.  */
	      
	    }
	    else if (len == 8)
	      {
		/* Where is it going to be stored? */
		while (fp_args[double_arg_index])
		  double_arg_index += 2;
		/* Now double_argreg points to the register
		   where it should be stored.
		   Are we still within the allowed register set? */
		if (double_arg_index < FLOAT_ARGLAST_REGNUM)
		  {
		    /* Goes in DR0...DR10 */
		    /* The numbering of the DRi registers is consecutive,
		       i.e. includes odd numbers.  */
		    int double_register_offset = double_arg_index / 2;
		    int regnum = DR0_REGNUM + double_register_offset;
		    regcache_cooked_write (regcache, regnum, val);
		    fp_args[double_arg_index] = 1;
		    fp_args[double_arg_index + 1] = 1;
		    /* Skip the corresponding general argument register.  */
		    int_argreg ++;
		  }
		else
		  ;
		  /* Store it as the integers, 8 bytes at the time, if
                     necessary spilling on the stack.  */
	      }
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
sh64_extract_return_value (struct type *type, struct regcache *regcache,
			   void *valbuf)
{
  int len = TYPE_LENGTH (type);
  
  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      if (len == 4)
	{
	  /* Return value stored in gdbarch_fp0_regnum */
	  regcache_raw_read (regcache,
			     gdbarch_fp0_regnum (current_gdbarch), valbuf);
	}
      else if (len == 8)
	{
	  /* return value stored in DR0_REGNUM */
	  DOUBLEST val;
	  gdb_byte buf[8];

	  regcache_cooked_read (regcache, DR0_REGNUM, buf);
	  
	  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_LITTLE)
	    floatformat_to_doublest (&floatformat_ieee_double_littlebyte_bigword,
				     buf, &val);
	  else
	    floatformat_to_doublest (&floatformat_ieee_double_big,
				     buf, &val);
	  store_typed_floating (valbuf, type, val);
	}
    }
  else
    { 
      if (len <= 8)
	{
	  int offset;
	  char buf[8];
	  /* Result is in register 2. If smaller than 8 bytes, it is padded 
	     at the most significant end.  */
	  regcache_raw_read (regcache, DEFAULT_RETURN_REGNUM, buf);

	  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
	    offset = register_size (current_gdbarch, DEFAULT_RETURN_REGNUM)
		     - len;
	  else
	    offset = 0;
	  memcpy (valbuf, buf + offset, len);
	}
      else
	error ("bad size for return value");
    }
}

/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format.
   If the architecture is sh4 or sh3e, store a function's return value
   in the R0 general register or in the FP0 floating point register,
   depending on the type of the return value. In all the other cases
   the result is stored in r0, left-justified.  */

static void
sh64_store_return_value (struct type *type, struct regcache *regcache,
			 const void *valbuf)
{
  char buf[64];	/* more than enough...  */
  int len = TYPE_LENGTH (type);

  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      int i, regnum = gdbarch_fp0_regnum (current_gdbarch);
      for (i = 0; i < len; i += 4)
	if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_LITTLE)
	  regcache_raw_write (regcache, regnum++,
			      (char *) valbuf + len - 4 - i);
	else
	  regcache_raw_write (regcache, regnum++, (char *) valbuf + i);
    }
  else
    {
      int return_register = DEFAULT_RETURN_REGNUM;
      int offset = 0;

      if (len <= register_size (current_gdbarch, return_register))
	{
	  /* Pad with zeros.  */
	  memset (buf, 0, register_size (current_gdbarch, return_register));
	  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_LITTLE)
	    offset = 0; /*register_size (current_gdbarch, 
			  return_register) - len;*/
	  else
	    offset = register_size (current_gdbarch, return_register) - len;

	  memcpy (buf + offset, valbuf, len);
	  regcache_raw_write (regcache, return_register, buf);
	}
      else
	regcache_raw_write (regcache, return_register, valbuf);
    }
}

static enum return_value_convention
sh64_return_value (struct gdbarch *gdbarch, struct type *type,
		   struct regcache *regcache,
		   gdb_byte *readbuf, const gdb_byte *writebuf)
{
  if (sh64_use_struct_convention (type))
    return RETURN_VALUE_STRUCT_CONVENTION;
  if (writebuf)
    sh64_store_return_value (type, regcache, writebuf);
  else if (readbuf)
    sh64_extract_return_value (type, regcache, readbuf);
  return RETURN_VALUE_REGISTER_CONVENTION;
}

static void
sh64_show_media_regs (struct frame_info *frame)
{
  int i;

  printf_filtered
    ("PC=%s SR=%016llx \n",
     paddr (get_frame_register_unsigned (frame,
					 gdbarch_pc_regnum (current_gdbarch))),
     (long long) get_frame_register_unsigned (frame, SR_REGNUM));

  printf_filtered
    ("SSR=%016llx SPC=%016llx \n",
     (long long) get_frame_register_unsigned (frame, SSR_REGNUM),
     (long long) get_frame_register_unsigned (frame, SPC_REGNUM));
  printf_filtered
    ("FPSCR=%016lx\n ",
     (long) get_frame_register_unsigned (frame, FPSCR_REGNUM));

  for (i = 0; i < 64; i = i + 4)
    printf_filtered
      ("\nR%d-R%d  %016llx %016llx %016llx %016llx\n",
       i, i + 3,
      (long long) get_frame_register_unsigned (frame, i + 0),
      (long long) get_frame_register_unsigned (frame, i + 1),
      (long long) get_frame_register_unsigned (frame, i + 2),
      (long long) get_frame_register_unsigned (frame, i + 3));

  printf_filtered ("\n");
  
  for (i = 0; i < 64; i = i + 8)
    printf_filtered
      ("FR%d-FR%d  %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
       i, i + 7,
       (long) get_frame_register_unsigned
	        (frame, gdbarch_fp0_regnum (current_gdbarch) + i + 0),
       (long) get_frame_register_unsigned
	        (frame, gdbarch_fp0_regnum (current_gdbarch) + i + 1),
       (long) get_frame_register_unsigned
	        (frame, gdbarch_fp0_regnum (current_gdbarch) + i + 2),
       (long) get_frame_register_unsigned
	        (frame, gdbarch_fp0_regnum (current_gdbarch) + i + 3),
       (long) get_frame_register_unsigned
	        (frame, gdbarch_fp0_regnum (current_gdbarch) + i + 4),
       (long) get_frame_register_unsigned
	        (frame, gdbarch_fp0_regnum (current_gdbarch) + i + 5),
       (long) get_frame_register_unsigned
	        (frame, gdbarch_fp0_regnum (current_gdbarch) + i + 6),
       (long) get_frame_register_unsigned
	        (frame, gdbarch_fp0_regnum (current_gdbarch) + i + 7));
}

static void
sh64_show_compact_regs (struct frame_info *frame)
{
  int i;

  printf_filtered
    ("PC=%s \n",
     paddr (get_frame_register_unsigned (frame, PC_C_REGNUM)));

  printf_filtered
    ("GBR=%08lx MACH=%08lx MACL=%08lx PR=%08lx T=%08lx\n",
     (long) get_frame_register_unsigned (frame, GBR_C_REGNUM),
     (long) get_frame_register_unsigned (frame, MACH_C_REGNUM),
     (long) get_frame_register_unsigned (frame, MACL_C_REGNUM),
     (long) get_frame_register_unsigned (frame, PR_C_REGNUM),
     (long) get_frame_register_unsigned (frame, T_C_REGNUM));
  printf_filtered
    ("FPSCR=%08lx FPUL=%08lx\n",
     (long) get_frame_register_unsigned (frame, FPSCR_C_REGNUM),
     (long) get_frame_register_unsigned (frame, FPUL_C_REGNUM));

  for (i = 0; i < 16; i = i + 4)
    printf_filtered
      ("\nR%d-R%d  %08lx %08lx %08lx %08lx\n",
       i, i + 3,
       (long) get_frame_register_unsigned (frame, i + 0),
       (long) get_frame_register_unsigned (frame, i + 1),
       (long) get_frame_register_unsigned (frame, i + 2),
       (long) get_frame_register_unsigned (frame, i + 3));

  printf_filtered ("\n");
  
  for (i = 0; i < 16; i = i + 8)
    printf_filtered
      ("FR%d-FR%d  %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n",
       i, i + 7,
       (long) get_frame_register_unsigned
		(frame, gdbarch_fp0_regnum (current_gdbarch) + i + 0),
       (long) get_frame_register_unsigned
		(frame, gdbarch_fp0_regnum (current_gdbarch) + i + 1),
       (long) get_frame_register_unsigned
		(frame, gdbarch_fp0_regnum (current_gdbarch) + i + 2),
       (long) get_frame_register_unsigned
		(frame, gdbarch_fp0_regnum (current_gdbarch) + i + 3),
       (long) get_frame_register_unsigned
		(frame, gdbarch_fp0_regnum (current_gdbarch) + i + 4),
       (long) get_frame_register_unsigned
		(frame, gdbarch_fp0_regnum (current_gdbarch) + i + 5),
       (long) get_frame_register_unsigned
		(frame, gdbarch_fp0_regnum (current_gdbarch) + i + 6),
       (long) get_frame_register_unsigned
		(frame, gdbarch_fp0_regnum (current_gdbarch) + i + 7));
}

/* FIXME!!! This only shows the registers for shmedia, excluding the
   pseudo registers.  */
void
sh64_show_regs (struct frame_info *frame)
{
  if (pc_is_isa32 (get_frame_pc (frame)))
    sh64_show_media_regs (frame);
  else
    sh64_show_compact_regs (frame);
}

/* *INDENT-OFF* */
/*
    SH MEDIA MODE (ISA 32)
    general registers (64-bit) 0-63
0    r0,   r1,   r2,   r3,   r4,   r5,   r6,   r7,
64   r8,   r9,   r10,  r11,  r12,  r13,  r14,  r15,
128  r16,  r17,  r18,  r19,  r20,  r21,  r22,  r23,
192  r24,  r25,  r26,  r27,  r28,  r29,  r30,  r31,
256  r32,  r33,  r34,  r35,  r36,  r37,  r38,  r39,
320  r40,  r41,  r42,  r43,  r44,  r45,  r46,  r47,
384  r48,  r49,  r50,  r51,  r52,  r53,  r54,  r55,
448  r56,  r57,  r58,  r59,  r60,  r61,  r62,  r63,

    pc (64-bit) 64
512  pc,

    status reg., saved status reg., saved pc reg. (64-bit) 65-67
520  sr,  ssr,  spc,

    target registers (64-bit) 68-75
544  tr0,  tr1,  tr2,  tr3,  tr4,  tr5,  tr6,  tr7,

    floating point state control register (32-bit) 76
608  fpscr,

    single precision floating point registers (32-bit) 77-140
612  fr0,  fr1,  fr2,  fr3,  fr4,  fr5,  fr6,  fr7,
644  fr8,  fr9,  fr10, fr11, fr12, fr13, fr14, fr15,
676  fr16, fr17, fr18, fr19, fr20, fr21, fr22, fr23,
708  fr24, fr25, fr26, fr27, fr28, fr29, fr30, fr31,
740  fr32, fr33, fr34, fr35, fr36, fr37, fr38, fr39,
772  fr40, fr41, fr42, fr43, fr44, fr45, fr46, fr47,
804  fr48, fr49, fr50, fr51, fr52, fr53, fr54, fr55,
836  fr56, fr57, fr58, fr59, fr60, fr61, fr62, fr63,

TOTAL SPACE FOR REGISTERS: 868 bytes

From here on they are all pseudo registers: no memory allocated.
REGISTER_BYTE returns the register byte for the base register.

    double precision registers (pseudo) 141-172
     dr0,  dr2,  dr4,  dr6,  dr8,  dr10, dr12, dr14,
     dr16, dr18, dr20, dr22, dr24, dr26, dr28, dr30,
     dr32, dr34, dr36, dr38, dr40, dr42, dr44, dr46,
     dr48, dr50, dr52, dr54, dr56, dr58, dr60, dr62,
 
    floating point pairs (pseudo) 173-204
     fp0,  fp2,  fp4,  fp6,  fp8,  fp10, fp12, fp14,
     fp16, fp18, fp20, fp22, fp24, fp26, fp28, fp30,
     fp32, fp34, fp36, fp38, fp40, fp42, fp44, fp46,
     fp48, fp50, fp52, fp54, fp56, fp58, fp60, fp62,
 
    floating point vectors (4 floating point regs) (pseudo) 205-220
     fv0,  fv4,  fv8,  fv12, fv16, fv20, fv24, fv28,
     fv32, fv36, fv40, fv44, fv48, fv52, fv56, fv60,
 
    SH COMPACT MODE (ISA 16) (all pseudo) 221-272
     r0_c, r1_c, r2_c,  r3_c,  r4_c,  r5_c,  r6_c,  r7_c,
     r8_c, r9_c, r10_c, r11_c, r12_c, r13_c, r14_c, r15_c,
     pc_c,
     gbr_c, mach_c, macl_c, pr_c, t_c,
     fpscr_c, fpul_c,
     fr0_c, fr1_c, fr2_c,  fr3_c,  fr4_c,  fr5_c,  fr6_c,  fr7_c,
     fr8_c, fr9_c, fr10_c, fr11_c, fr12_c, fr13_c, fr14_c, fr15_c
     dr0_c, dr2_c, dr4_c,  dr6_c,  dr8_c,  dr10_c, dr12_c, dr14_c
     fv0_c, fv4_c, fv8_c,  fv12_c
*/

static struct type *
sh64_build_float_register_type (int high)
{
  struct type *temp;

  temp = create_range_type (NULL, builtin_type_int, 0, high);
  return create_array_type (NULL, builtin_type_float, temp);
}

/* Return the GDB type object for the "standard" data type
   of data in register REG_NR.  */
static struct type *
sh64_register_type (struct gdbarch *gdbarch, int reg_nr)
{
  if ((reg_nr >= gdbarch_fp0_regnum (current_gdbarch)
       && reg_nr <= FP_LAST_REGNUM)
      || (reg_nr >= FP0_C_REGNUM
	  && reg_nr <= FP_LAST_C_REGNUM))
    return builtin_type_float;
  else if ((reg_nr >= DR0_REGNUM 
	    && reg_nr <= DR_LAST_REGNUM)
	   || (reg_nr >= DR0_C_REGNUM 
	       && reg_nr <= DR_LAST_C_REGNUM))
    return builtin_type_double;
  else if  (reg_nr >= FPP0_REGNUM 
	    && reg_nr <= FPP_LAST_REGNUM)
    return sh64_build_float_register_type (1);
  else if ((reg_nr >= FV0_REGNUM
	    && reg_nr <= FV_LAST_REGNUM)
	   ||(reg_nr >= FV0_C_REGNUM 
	      && reg_nr <= FV_LAST_C_REGNUM))
    return sh64_build_float_register_type (3);
  else if (reg_nr == FPSCR_REGNUM)
    return builtin_type_int;
  else if (reg_nr >= R0_C_REGNUM
	   && reg_nr < FP0_C_REGNUM)
    return builtin_type_int;
  else
    return builtin_type_long_long;
}

static void
sh64_register_convert_to_virtual (int regnum, struct type *type,
				     char *from, char *to)
{
  if (gdbarch_byte_order (current_gdbarch) != BFD_ENDIAN_LITTLE)
    {
      /* It is a no-op.  */
      memcpy (to, from, register_size (current_gdbarch, regnum));
      return;
    }

  if ((regnum >= DR0_REGNUM 
       && regnum <= DR_LAST_REGNUM)
      || (regnum >= DR0_C_REGNUM 
	  && regnum <= DR_LAST_C_REGNUM))
    {
      DOUBLEST val;
      floatformat_to_doublest (&floatformat_ieee_double_littlebyte_bigword, 
			       from, &val);
      store_typed_floating (to, type, val);
    }
  else
    error ("sh64_register_convert_to_virtual called with non DR register number");
}

static void
sh64_register_convert_to_raw (struct type *type, int regnum,
				 const void *from, void *to)
{
  if (gdbarch_byte_order (current_gdbarch) != BFD_ENDIAN_LITTLE)
    {
      /* It is a no-op.  */
      memcpy (to, from, register_size (current_gdbarch, regnum));
      return;
    }

  if ((regnum >= DR0_REGNUM 
       && regnum <= DR_LAST_REGNUM)
      || (regnum >= DR0_C_REGNUM 
	  && regnum <= DR_LAST_C_REGNUM))
    {
      DOUBLEST val = deprecated_extract_floating (from, TYPE_LENGTH(type));
      floatformat_from_doublest (&floatformat_ieee_double_littlebyte_bigword, 
				 &val, to);
    }
  else
    error ("sh64_register_convert_to_raw called with non DR register number");
}

static void
sh64_pseudo_register_read (struct gdbarch *gdbarch, struct regcache *regcache,
			   int reg_nr, gdb_byte *buffer)
{
  int base_regnum;
  int portion;
  int offset = 0;
  char temp_buffer[MAX_REGISTER_SIZE];

  if (reg_nr >= DR0_REGNUM 
      && reg_nr <= DR_LAST_REGNUM)
    {
      base_regnum = sh64_dr_reg_base_num (reg_nr);

      /* Build the value in the provided buffer.  */ 
      /* DR regs are double precision registers obtained by
	 concatenating 2 single precision floating point registers.  */
      for (portion = 0; portion < 2; portion++)
	regcache_raw_read (regcache, base_regnum + portion, 
			   (temp_buffer
			    + register_size (gdbarch, base_regnum) * portion));

      /* We must pay attention to the endianness.  */
      sh64_register_convert_to_virtual (reg_nr, 
					register_type (gdbarch, reg_nr),
					temp_buffer, buffer);

    }

  else if (reg_nr >= FPP0_REGNUM 
	   && reg_nr <= FPP_LAST_REGNUM)
    {
      base_regnum = sh64_fpp_reg_base_num (reg_nr);

      /* Build the value in the provided buffer.  */ 
      /* FPP regs are pairs of single precision registers obtained by
	 concatenating 2 single precision floating point registers.  */
      for (portion = 0; portion < 2; portion++)
	regcache_raw_read (regcache, base_regnum + portion, 
			   ((char *) buffer
			    + register_size (gdbarch, base_regnum) * portion));
    }

  else if (reg_nr >= FV0_REGNUM 
	   && reg_nr <= FV_LAST_REGNUM)
    {
      base_regnum = sh64_fv_reg_base_num (reg_nr);

      /* Build the value in the provided buffer.  */ 
      /* FV regs are vectors of single precision registers obtained by
	 concatenating 4 single precision floating point registers.  */
      for (portion = 0; portion < 4; portion++)
	regcache_raw_read (regcache, base_regnum + portion, 
			   ((char *) buffer
			    + register_size (gdbarch, base_regnum) * portion));
    }

  /* sh compact pseudo registers. 1-to-1 with a shmedia register */
  else if (reg_nr >= R0_C_REGNUM 
	   && reg_nr <= T_C_REGNUM)
    {
      base_regnum = sh64_compact_reg_base_num (reg_nr);

      /* Build the value in the provided buffer.  */ 
      regcache_raw_read (regcache, base_regnum, temp_buffer);
      if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
	offset = 4;
      memcpy (buffer, temp_buffer + offset, 4); /* get LOWER 32 bits only????*/
    }

  else if (reg_nr >= FP0_C_REGNUM
	   && reg_nr <= FP_LAST_C_REGNUM)
    {
      base_regnum = sh64_compact_reg_base_num (reg_nr);

      /* Build the value in the provided buffer.  */ 
      /* Floating point registers map 1-1 to the media fp regs,
	 they have the same size and endianness.  */
      regcache_raw_read (regcache, base_regnum, buffer);
    }

  else if (reg_nr >= DR0_C_REGNUM 
	   && reg_nr <= DR_LAST_C_REGNUM)
    {
      base_regnum = sh64_compact_reg_base_num (reg_nr);

      /* DR_C regs are double precision registers obtained by
	 concatenating 2 single precision floating point registers.  */
      for (portion = 0; portion < 2; portion++)
	regcache_raw_read (regcache, base_regnum + portion, 
			   (temp_buffer
			    + register_size (gdbarch, base_regnum) * portion));

      /* We must pay attention to the endianness.  */
      sh64_register_convert_to_virtual (reg_nr, 
					register_type (gdbarch, reg_nr),
					temp_buffer, buffer);
    }

  else if (reg_nr >= FV0_C_REGNUM 
	   && reg_nr <= FV_LAST_C_REGNUM)
    {
      base_regnum = sh64_compact_reg_base_num (reg_nr);

      /* Build the value in the provided buffer.  */ 
      /* FV_C regs are vectors of single precision registers obtained by
	 concatenating 4 single precision floating point registers.  */
      for (portion = 0; portion < 4; portion++)
	regcache_raw_read (regcache, base_regnum + portion, 
			   ((char *) buffer
			    + register_size (gdbarch, base_regnum) * portion));
    }

  else if (reg_nr == FPSCR_C_REGNUM)
    {
      int fpscr_base_regnum;
      int sr_base_regnum;
      unsigned int fpscr_value;
      unsigned int sr_value;
      unsigned int fpscr_c_value;
      unsigned int fpscr_c_part1_value;
      unsigned int fpscr_c_part2_value;

      fpscr_base_regnum = FPSCR_REGNUM;
      sr_base_regnum = SR_REGNUM;

      /* Build the value in the provided buffer.  */ 
      /* FPSCR_C is a very weird register that contains sparse bits
	 from the FPSCR and the SR architectural registers.
	 Specifically: */
      /* *INDENT-OFF* */
      /*
	 FPSRC_C bit
            0         Bit 0 of FPSCR
            1         reserved
            2-17      Bit 2-18 of FPSCR
            18-20     Bits 12,13,14 of SR
            21-31     reserved
       */
      /* *INDENT-ON* */
      /* Get FPSCR into a local buffer */
      regcache_raw_read (regcache, fpscr_base_regnum, temp_buffer);
      /* Get value as an int.  */
      fpscr_value = extract_unsigned_integer (temp_buffer, 4);
      /* Get SR into a local buffer */
      regcache_raw_read (regcache, sr_base_regnum, temp_buffer);
      /* Get value as an int.  */
      sr_value = extract_unsigned_integer (temp_buffer, 4);
      /* Build the new value.  */
      fpscr_c_part1_value = fpscr_value & 0x3fffd;
      fpscr_c_part2_value = (sr_value & 0x7000) << 6;
      fpscr_c_value = fpscr_c_part1_value | fpscr_c_part2_value;
      /* Store that in out buffer!!! */
      store_unsigned_integer (buffer, 4, fpscr_c_value);
      /* FIXME There is surely an endianness gotcha here.  */
    }

  else if (reg_nr == FPUL_C_REGNUM)
    {
      base_regnum = sh64_compact_reg_base_num (reg_nr);

      /* FPUL_C register is floating point register 32,
	 same size, same endianness.  */
      regcache_raw_read (regcache, base_regnum, buffer);
    }
}

static void
sh64_pseudo_register_write (struct gdbarch *gdbarch, struct regcache *regcache,
			    int reg_nr, const gdb_byte *buffer)
{
  int base_regnum, portion;
  int offset;
  char temp_buffer[MAX_REGISTER_SIZE];

  if (reg_nr >= DR0_REGNUM
      && reg_nr <= DR_LAST_REGNUM)
    {
      base_regnum = sh64_dr_reg_base_num (reg_nr);
      /* We must pay attention to the endianness.  */
      sh64_register_convert_to_raw (register_type (gdbarch, reg_nr),
				    reg_nr,
				    buffer, temp_buffer);

      /* Write the real regs for which this one is an alias.  */
      for (portion = 0; portion < 2; portion++)
	regcache_raw_write (regcache, base_regnum + portion, 
			    (temp_buffer
			     + register_size (gdbarch, 
					      base_regnum) * portion));
    }

  else if (reg_nr >= FPP0_REGNUM 
	   && reg_nr <= FPP_LAST_REGNUM)
    {
      base_regnum = sh64_fpp_reg_base_num (reg_nr);

      /* Write the real regs for which this one is an alias.  */
      for (portion = 0; portion < 2; portion++)
	regcache_raw_write (regcache, base_regnum + portion,
			    ((char *) buffer
			     + register_size (gdbarch, 
					      base_regnum) * portion));
    }

  else if (reg_nr >= FV0_REGNUM
	   && reg_nr <= FV_LAST_REGNUM)
    {
      base_regnum = sh64_fv_reg_base_num (reg_nr);

      /* Write the real regs for which this one is an alias.  */
      for (portion = 0; portion < 4; portion++)
	regcache_raw_write (regcache, base_regnum + portion,
			    ((char *) buffer
			     + register_size (gdbarch, 
					      base_regnum) * portion));
    }

  /* sh compact general pseudo registers. 1-to-1 with a shmedia
     register but only 4 bytes of it.  */
  else if (reg_nr >= R0_C_REGNUM 
	   && reg_nr <= T_C_REGNUM)
    {
      base_regnum = sh64_compact_reg_base_num (reg_nr);
      /* reg_nr is 32 bit here, and base_regnum is 64 bits.  */
      if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
	offset = 4;
      else 
	offset = 0;
      /* Let's read the value of the base register into a temporary
	 buffer, so that overwriting the last four bytes with the new
	 value of the pseudo will leave the upper 4 bytes unchanged.  */
      regcache_raw_read (regcache, base_regnum, temp_buffer);
      /* Write as an 8 byte quantity */
      memcpy (temp_buffer + offset, buffer, 4);
      regcache_raw_write (regcache, base_regnum, temp_buffer);
    }

  /* sh floating point compact pseudo registers. 1-to-1 with a shmedia
     registers. Both are 4 bytes.  */
  else if (reg_nr >= FP0_C_REGNUM
	       && reg_nr <= FP_LAST_C_REGNUM)
    {
      base_regnum = sh64_compact_reg_base_num (reg_nr);
      regcache_raw_write (regcache, base_regnum, buffer);
    }

  else if (reg_nr >= DR0_C_REGNUM 
	   && reg_nr <= DR_LAST_C_REGNUM)
    {
      base_regnum = sh64_compact_reg_base_num (reg_nr);
      for (portion = 0; portion < 2; portion++)
	{
	  /* We must pay attention to the endianness.  */
	  sh64_register_convert_to_raw (register_type (gdbarch, reg_nr),
					reg_nr,
					buffer, temp_buffer);

	  regcache_raw_write (regcache, base_regnum + portion,
			      (temp_buffer
			       + register_size (gdbarch, 
						base_regnum) * portion));
	}
    }

  else if (reg_nr >= FV0_C_REGNUM 
	   && reg_nr <= FV_LAST_C_REGNUM)
    {
      base_regnum = sh64_compact_reg_base_num (reg_nr);
     
      for (portion = 0; portion < 4; portion++)
	{
	  regcache_raw_write (regcache, base_regnum + portion,
			      ((char *) buffer
			       + register_size (gdbarch, 
						base_regnum) * portion));
	}
    }

  else if (reg_nr == FPSCR_C_REGNUM)
    {      
      int fpscr_base_regnum;
      int sr_base_regnum;
      unsigned int fpscr_value;
      unsigned int sr_value;
      unsigned int old_fpscr_value;
      unsigned int old_sr_value;
      unsigned int fpscr_c_value;
      unsigned int fpscr_mask;
      unsigned int sr_mask;

      fpscr_base_regnum = FPSCR_REGNUM;
      sr_base_regnum = SR_REGNUM;

      /* FPSCR_C is a very weird register that contains sparse bits
	 from the FPSCR and the SR architectural registers.
	 Specifically: */
      /* *INDENT-OFF* */
      /*
	 FPSRC_C bit
            0         Bit 0 of FPSCR
            1         reserved
            2-17      Bit 2-18 of FPSCR
            18-20     Bits 12,13,14 of SR
            21-31     reserved
       */
      /* *INDENT-ON* */
      /* Get value as an int.  */
      fpscr_c_value = extract_unsigned_integer (buffer, 4);

      /* Build the new values.  */
      fpscr_mask = 0x0003fffd;
      sr_mask = 0x001c0000;
       
      fpscr_value = fpscr_c_value & fpscr_mask;
      sr_value = (fpscr_value & sr_mask) >> 6;
      
      regcache_raw_read (regcache, fpscr_base_regnum, temp_buffer);
      old_fpscr_value = extract_unsigned_integer (temp_buffer, 4);
      old_fpscr_value &= 0xfffc0002;
      fpscr_value |= old_fpscr_value;
      store_unsigned_integer (temp_buffer, 4, fpscr_value);
      regcache_raw_write (regcache, fpscr_base_regnum, temp_buffer);
      
      regcache_raw_read (regcache, sr_base_regnum, temp_buffer);
      old_sr_value = extract_unsigned_integer (temp_buffer, 4);
      old_sr_value &= 0xffff8fff;
      sr_value |= old_sr_value;
      store_unsigned_integer (temp_buffer, 4, sr_value);
      regcache_raw_write (regcache, sr_base_regnum, temp_buffer);
    }

  else if (reg_nr == FPUL_C_REGNUM)
    {
      base_regnum = sh64_compact_reg_base_num (reg_nr);
      regcache_raw_write (regcache, base_regnum, buffer);
    }
}

/* FIXME:!! THIS SHOULD TAKE CARE OF GETTING THE RIGHT PORTION OF THE
   shmedia REGISTERS.  */
/* Control registers, compact mode.  */
static void
sh64_do_cr_c_register_info (struct ui_file *file, struct frame_info *frame,
			    int cr_c_regnum)
{
  switch (cr_c_regnum)
    {
    case PC_C_REGNUM:
      fprintf_filtered (file, "pc_c\t0x%08x\n",
	  (int) get_frame_register_unsigned (frame, cr_c_regnum));
      break;
    case GBR_C_REGNUM: 
      fprintf_filtered (file, "gbr_c\t0x%08x\n",
      	  (int) get_frame_register_unsigned (frame, cr_c_regnum));
      break;
    case MACH_C_REGNUM: 
      fprintf_filtered (file, "mach_c\t0x%08x\n",
      	  (int) get_frame_register_unsigned (frame, cr_c_regnum));
      break;
    case MACL_C_REGNUM: 
      fprintf_filtered (file, "macl_c\t0x%08x\n",
      	  (int) get_frame_register_unsigned (frame, cr_c_regnum));
      break;
    case PR_C_REGNUM: 
      fprintf_filtered (file, "pr_c\t0x%08x\n",
      	  (int) get_frame_register_unsigned (frame, cr_c_regnum));
      break;
    case T_C_REGNUM: 
      fprintf_filtered (file, "t_c\t0x%08x\n",
      	  (int) get_frame_register_unsigned (frame, cr_c_regnum));
      break;
    case FPSCR_C_REGNUM: 
      fprintf_filtered (file, "fpscr_c\t0x%08x\n",
      	  (int) get_frame_register_unsigned (frame, cr_c_regnum));
      break;
    case FPUL_C_REGNUM:
      fprintf_filtered (file, "fpul_c\t0x%08x\n",
          (int) get_frame_register_unsigned (frame, cr_c_regnum));
      break;
    }
}

static void
sh64_do_fp_register (struct gdbarch *gdbarch, struct ui_file *file,
		     struct frame_info *frame, int regnum)
{				/* do values for FP (float) regs */
  unsigned char *raw_buffer;
  double flt;	/* double extracted from raw hex data */
  int inv;
  int j;

  /* Allocate space for the float.  */
  raw_buffer = (unsigned char *) alloca
				 (register_size (gdbarch,
						 gdbarch_fp0_regnum
						 (current_gdbarch)));

  /* Get the data in raw format.  */
  if (!frame_register_read (frame, regnum, raw_buffer))
    error ("can't read register %d (%s)",
	   regnum, gdbarch_register_name (current_gdbarch, regnum));

  /* Get the register as a number */ 
  flt = unpack_double (builtin_type_float, raw_buffer, &inv);

  /* Print the name and some spaces.  */
  fputs_filtered (gdbarch_register_name (current_gdbarch, regnum), file);
  print_spaces_filtered (15 - strlen (gdbarch_register_name
					(current_gdbarch, regnum)), file);

  /* Print the value.  */
  if (inv)
    fprintf_filtered (file, "<invalid float>");
  else
    fprintf_filtered (file, "%-10.9g", flt);

  /* Print the fp register as hex.  */
  fprintf_filtered (file, "\t(raw 0x");
  for (j = 0; j < register_size (gdbarch, regnum); j++)
    {
      int idx = gdbarch_byte_order (current_gdbarch)
		== BFD_ENDIAN_BIG ? j : register_size
		(gdbarch, regnum) - 1 - j;
      fprintf_filtered (file, "%02x", raw_buffer[idx]);
    }
  fprintf_filtered (file, ")");
  fprintf_filtered (file, "\n");
}

static void
sh64_do_pseudo_register (struct gdbarch *gdbarch, struct ui_file *file,
			 struct frame_info *frame, int regnum)
{
  /* All the sh64-compact mode registers are pseudo registers.  */

  if (regnum < gdbarch_num_regs (current_gdbarch)
      || regnum >= gdbarch_num_regs (current_gdbarch)
		   + NUM_PSEUDO_REGS_SH_MEDIA
		   + NUM_PSEUDO_REGS_SH_COMPACT)
    internal_error (__FILE__, __LINE__,
		    _("Invalid pseudo register number %d\n"), regnum);

  else if ((regnum >= DR0_REGNUM && regnum <= DR_LAST_REGNUM))
    {
      int fp_regnum = sh64_dr_reg_base_num (regnum);
      fprintf_filtered (file, "dr%d\t0x%08x%08x\n", regnum - DR0_REGNUM, 
	  (unsigned) get_frame_register_unsigned (frame, fp_regnum),
	  (unsigned) get_frame_register_unsigned (frame, fp_regnum + 1));
    }

  else if ((regnum >= DR0_C_REGNUM && regnum <= DR_LAST_C_REGNUM))
    {
      int fp_regnum = sh64_compact_reg_base_num (regnum);
      fprintf_filtered (file, "dr%d_c\t0x%08x%08x\n", regnum - DR0_C_REGNUM,
	  (unsigned) get_frame_register_unsigned (frame, fp_regnum),
	  (unsigned) get_frame_register_unsigned (frame, fp_regnum + 1));
    }

  else if ((regnum >= FV0_REGNUM && regnum <= FV_LAST_REGNUM))
    {
      int fp_regnum = sh64_fv_reg_base_num (regnum);
      fprintf_filtered (file, "fv%d\t0x%08x\t0x%08x\t0x%08x\t0x%08x\n", 
	   regnum - FV0_REGNUM, 
	   (unsigned) get_frame_register_unsigned (frame, fp_regnum),
	   (unsigned) get_frame_register_unsigned (frame, fp_regnum + 1),
	   (unsigned) get_frame_register_unsigned (frame, fp_regnum + 2),
	   (unsigned) get_frame_register_unsigned (frame, fp_regnum + 3));
    }
	   
  else if ((regnum >= FV0_C_REGNUM && regnum <= FV_LAST_C_REGNUM))
    {
      int fp_regnum = sh64_compact_reg_base_num (regnum);
      fprintf_filtered (file, "fv%d_c\t0x%08x\t0x%08x\t0x%08x\t0x%08x\n", 
	   regnum - FV0_C_REGNUM, 
	   (unsigned) get_frame_register_unsigned (frame, fp_regnum),
	   (unsigned) get_frame_register_unsigned (frame, fp_regnum + 1),
	   (unsigned) get_frame_register_unsigned (frame, fp_regnum + 2),
	   (unsigned) get_frame_register_unsigned (frame, fp_regnum + 3));
    }

  else if (regnum >= FPP0_REGNUM && regnum <= FPP_LAST_REGNUM)
    {
      int fp_regnum = sh64_fpp_reg_base_num (regnum);
      fprintf_filtered (file, "fpp%d\t0x%08x\t0x%08x\n", regnum - FPP0_REGNUM, 
	  (unsigned) get_frame_register_unsigned (frame, fp_regnum),
	  (unsigned) get_frame_register_unsigned (frame, fp_regnum + 1));
    }

  else if (regnum >= R0_C_REGNUM && regnum <= R_LAST_C_REGNUM)
    {
      int c_regnum = sh64_compact_reg_base_num (regnum);
      fprintf_filtered (file, "r%d_c\t0x%08x\n", regnum - R0_C_REGNUM, 
	   (unsigned) get_frame_register_unsigned (frame, c_regnum));
    }
  else if (regnum >= FP0_C_REGNUM && regnum <= FP_LAST_C_REGNUM)
    /* This should work also for pseudoregs.  */
    sh64_do_fp_register (gdbarch, file, frame, regnum);
  else if (regnum >= PC_C_REGNUM && regnum <= FPUL_C_REGNUM)
    sh64_do_cr_c_register_info (file, frame, regnum);
}

static void
sh64_do_register (struct gdbarch *gdbarch, struct ui_file *file,
		  struct frame_info *frame, int regnum)
{
  unsigned char raw_buffer[MAX_REGISTER_SIZE];

  fputs_filtered (gdbarch_register_name (current_gdbarch, regnum), file);
  print_spaces_filtered (15 - strlen (gdbarch_register_name
				      (current_gdbarch, regnum)), file);

  /* Get the data in raw format.  */
  if (!frame_register_read (frame, regnum, raw_buffer))
    fprintf_filtered (file, "*value not available*\n");
      
  val_print (register_type (gdbarch, regnum), raw_buffer, 0, 0,
	     file, 'x', 1, 0, Val_pretty_default);
  fprintf_filtered (file, "\t");
  val_print (register_type (gdbarch, regnum), raw_buffer, 0, 0,
	     file, 0, 1, 0, Val_pretty_default);
  fprintf_filtered (file, "\n");
}

static void
sh64_print_register (struct gdbarch *gdbarch, struct ui_file *file,
		     struct frame_info *frame, int regnum)
{
  if (regnum < 0 || regnum >= gdbarch_num_regs (current_gdbarch)
			      + gdbarch_num_pseudo_regs (current_gdbarch))
    internal_error (__FILE__, __LINE__,
		    _("Invalid register number %d\n"), regnum);

  else if (regnum >= 0 && regnum < gdbarch_num_regs (current_gdbarch))
    {
      if (TYPE_CODE (register_type (gdbarch, regnum)) == TYPE_CODE_FLT)
	sh64_do_fp_register (gdbarch, file, frame, regnum);	/* FP regs */
      else
	sh64_do_register (gdbarch, file, frame, regnum);
    }

  else if (regnum < gdbarch_num_regs (current_gdbarch)
		    + gdbarch_num_pseudo_regs (current_gdbarch))
    sh64_do_pseudo_register (gdbarch, file, frame, regnum);
}

static void
sh64_media_print_registers_info (struct gdbarch *gdbarch, struct ui_file *file,
				 struct frame_info *frame, int regnum,
				 int fpregs)
{
  if (regnum != -1)		/* do one specified register */
    {
      if (*(gdbarch_register_name (current_gdbarch, regnum)) == '\0')
	error ("Not a valid register for the current processor type");

      sh64_print_register (gdbarch, file, frame, regnum);
    }
  else
    /* do all (or most) registers */
    {
      regnum = 0;
      while (regnum < gdbarch_num_regs (current_gdbarch))
	{
	  /* If the register name is empty, it is undefined for this
	     processor, so don't display anything.  */
	  if (gdbarch_register_name (current_gdbarch, regnum) == NULL
	      || *(gdbarch_register_name (current_gdbarch, regnum)) == '\0')
	    { 
	      regnum++;
	      continue;
	    }

	  if (TYPE_CODE (register_type (gdbarch, regnum))
	      == TYPE_CODE_FLT)
	    {
	      if (fpregs)
		{
		  /* true for "INFO ALL-REGISTERS" command */
		  sh64_do_fp_register (gdbarch, file, frame, regnum);
		  regnum ++;
		}
	      else
		regnum += FP_LAST_REGNUM - gdbarch_fp0_regnum (current_gdbarch);
		/* skip FP regs */
	    }
	  else
	    {
	      sh64_do_register (gdbarch, file, frame, regnum);
	      regnum++;
	    }
	}

      if (fpregs)
	while (regnum < gdbarch_num_regs (current_gdbarch)
			+ gdbarch_num_pseudo_regs (current_gdbarch))
	  {
	    sh64_do_pseudo_register (gdbarch, file, frame, regnum);
	    regnum++;
	  }
    }
}

static void
sh64_compact_print_registers_info (struct gdbarch *gdbarch,
				   struct ui_file *file,
				   struct frame_info *frame, int regnum,
				   int fpregs)
{
  if (regnum != -1)		/* do one specified register */
    {
      if (*(gdbarch_register_name (current_gdbarch, regnum)) == '\0')
	error ("Not a valid register for the current processor type");

      if (regnum >= 0 && regnum < R0_C_REGNUM)
        error ("Not a valid register for the current processor mode.");

      sh64_print_register (gdbarch, file, frame, regnum);
    }
  else
    /* do all compact registers */
    {
      regnum = R0_C_REGNUM;
      while (regnum < gdbarch_num_regs (current_gdbarch)
		      + gdbarch_num_pseudo_regs (current_gdbarch))
        {
          sh64_do_pseudo_register (gdbarch, file, frame, regnum);
          regnum++;
        }
    }
}

static void
sh64_print_registers_info (struct gdbarch *gdbarch, struct ui_file *file,
			   struct frame_info *frame, int regnum, int fpregs)
{
  if (pc_is_isa32 (get_frame_pc (frame)))
    sh64_media_print_registers_info (gdbarch, file, frame, regnum, fpregs);
  else
    sh64_compact_print_registers_info (gdbarch, file, frame, regnum, fpregs);
}

static struct sh64_frame_cache *
sh64_alloc_frame_cache (void)
{
  struct sh64_frame_cache *cache;
  int i;

  cache = FRAME_OBSTACK_ZALLOC (struct sh64_frame_cache);

  /* Base address.  */
  cache->base = 0;
  cache->saved_sp = 0;
  cache->sp_offset = 0;
  cache->pc = 0;

  /* Frameless until proven otherwise.  */
  cache->uses_fp = 0;

  /* Saved registers.  We initialize these to -1 since zero is a valid
     offset (that's where fp is supposed to be stored).  */
  for (i = 0; i < SIM_SH64_NR_REGS; i++)
    {
      cache->saved_regs[i] = -1;
    }

  return cache;
}

static struct sh64_frame_cache *
sh64_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct sh64_frame_cache *cache;
  CORE_ADDR current_pc;
  int i;

  if (*this_cache)
    return *this_cache;

  cache = sh64_alloc_frame_cache ();
  *this_cache = cache;

  current_pc = frame_pc_unwind (next_frame);
  cache->media_mode = pc_is_isa32 (current_pc);

  /* In principle, for normal frames, fp holds the frame pointer,
     which holds the base address for the current stack frame.
     However, for functions that don't need it, the frame pointer is
     optional.  For these "frameless" functions the frame pointer is
     actually the frame pointer of the calling frame. */
  cache->base = frame_unwind_register_unsigned (next_frame, MEDIA_FP_REGNUM);
  if (cache->base == 0)
    return cache;

  cache->pc = frame_func_unwind (next_frame, NORMAL_FRAME);
  if (cache->pc != 0)
    sh64_analyze_prologue (current_gdbarch, cache, cache->pc, current_pc);

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
  for (i = 0; i < SIM_SH64_NR_REGS; i++)
    if (cache->saved_regs[i] != -1)
      cache->saved_regs[i] = cache->saved_sp - cache->saved_regs[i];

  return cache;
}

static void
sh64_frame_prev_register (struct frame_info *next_frame, void **this_cache,
			  int regnum, int *optimizedp,
			  enum lval_type *lvalp, CORE_ADDR *addrp,
			  int *realnump, gdb_byte *valuep)
{
  struct sh64_frame_cache *cache = sh64_frame_cache (next_frame, this_cache);

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
          store_unsigned_integer (valuep,
	  			  register_size (current_gdbarch,
				  gdbarch_sp_regnum (current_gdbarch)),
				  cache->saved_sp);
        }
      return;
    }

  /* The PC of the previous frame is stored in the PR register of
     the current frame.  Frob regnum so that we pull the value from
     the correct place.  */
  if (regnum == gdbarch_pc_regnum (current_gdbarch))
    regnum = PR_REGNUM;

  if (regnum < SIM_SH64_NR_REGS && cache->saved_regs[regnum] != -1)
    {
      int reg_size = register_size (current_gdbarch, regnum);
      int size;

      *optimizedp = 0;
      *lvalp = lval_memory;
      *addrp = cache->saved_regs[regnum];
      *realnump = -1;
      if (gdbarch_tdep (current_gdbarch)->sh_abi == SH_ABI_32
          && (regnum == MEDIA_FP_REGNUM || regnum == PR_REGNUM))
	size = 4;
      else
        size = reg_size;
      if (valuep)
        {
	  memset (valuep, 0, reg_size);
	  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_LITTLE)
	    read_memory (*addrp, valuep, size);
	  else
	    read_memory (*addrp, (char *) valuep + reg_size - size, size);
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
sh64_frame_this_id (struct frame_info *next_frame, void **this_cache,
		    struct frame_id *this_id)
{
  struct sh64_frame_cache *cache = sh64_frame_cache (next_frame, this_cache);

  /* This marks the outermost frame.  */
  if (cache->base == 0)
    return;

  *this_id = frame_id_build (cache->saved_sp, cache->pc);
}

static const struct frame_unwind sh64_frame_unwind = {
  NORMAL_FRAME,
  sh64_frame_this_id,
  sh64_frame_prev_register
};

static const struct frame_unwind *
sh64_frame_sniffer (struct frame_info *next_frame)
{
  return &sh64_frame_unwind;
}

static CORE_ADDR
sh64_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame,
					 gdbarch_sp_regnum (current_gdbarch));
}

static CORE_ADDR
sh64_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame,
					 gdbarch_pc_regnum (current_gdbarch));
}

static struct frame_id
sh64_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_id_build (sh64_unwind_sp (gdbarch, next_frame),
                         frame_pc_unwind (next_frame));
}

static CORE_ADDR
sh64_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct sh64_frame_cache *cache = sh64_frame_cache (next_frame, this_cache);

  return cache->base;
}

static const struct frame_base sh64_frame_base = {
  &sh64_frame_unwind,
  sh64_frame_base_address,
  sh64_frame_base_address,
  sh64_frame_base_address
};


struct gdbarch *
sh64_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  struct gdbarch_tdep *tdep;

  /* If there is already a candidate, use it.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  /* None found, create a new architecture from the information
     provided.  */
  tdep = XMALLOC (struct gdbarch_tdep);
  gdbarch = gdbarch_alloc (&info, tdep);

  /* Determine the ABI */
  if (info.abfd && bfd_get_arch_size (info.abfd) == 64)
    {
      /* If the ABI is the 64-bit one, it can only be sh-media.  */
      tdep->sh_abi = SH_ABI_64;
      set_gdbarch_ptr_bit (gdbarch, 8 * TARGET_CHAR_BIT);
      set_gdbarch_long_bit (gdbarch, 8 * TARGET_CHAR_BIT);
    }
  else
    {
      /* If the ABI is the 32-bit one it could be either media or
	 compact.  */
      tdep->sh_abi = SH_ABI_32;
      set_gdbarch_ptr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
      set_gdbarch_long_bit (gdbarch, 4 * TARGET_CHAR_BIT);
    }

  set_gdbarch_short_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_int_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_long_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_float_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_long_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);

  /* The number of real registers is the same whether we are in 
     ISA16(compact) or ISA32(media).  */
  set_gdbarch_num_regs (gdbarch, SIM_SH64_NR_REGS);
  set_gdbarch_sp_regnum (gdbarch, 15);
  set_gdbarch_pc_regnum (gdbarch, 64);
  set_gdbarch_fp0_regnum (gdbarch, SIM_SH64_FR0_REGNUM);
  set_gdbarch_num_pseudo_regs (gdbarch, NUM_PSEUDO_REGS_SH_MEDIA
  					+ NUM_PSEUDO_REGS_SH_COMPACT);

  set_gdbarch_register_name (gdbarch, sh64_register_name);
  set_gdbarch_register_type (gdbarch, sh64_register_type);

  set_gdbarch_pseudo_register_read (gdbarch, sh64_pseudo_register_read);
  set_gdbarch_pseudo_register_write (gdbarch, sh64_pseudo_register_write);

  set_gdbarch_breakpoint_from_pc (gdbarch, sh64_breakpoint_from_pc);

  set_gdbarch_print_insn (gdbarch, gdb_print_insn_sh64);
  set_gdbarch_register_sim_regno (gdbarch, legacy_register_sim_regno);

  set_gdbarch_return_value (gdbarch, sh64_return_value);

  set_gdbarch_skip_prologue (gdbarch, sh64_skip_prologue);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  set_gdbarch_push_dummy_call (gdbarch, sh64_push_dummy_call);

  set_gdbarch_believe_pcc_promotion (gdbarch, 1);

  set_gdbarch_frame_align (gdbarch, sh64_frame_align);
  set_gdbarch_unwind_sp (gdbarch, sh64_unwind_sp);
  set_gdbarch_unwind_pc (gdbarch, sh64_unwind_pc);
  set_gdbarch_unwind_dummy_id (gdbarch, sh64_unwind_dummy_id);
  frame_base_set_default (gdbarch, &sh64_frame_base);

  set_gdbarch_print_registers_info (gdbarch, sh64_print_registers_info);

  set_gdbarch_elf_make_msymbol_special (gdbarch,
					sh64_elf_make_msymbol_special);

  /* Hook in ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch);

  frame_unwind_append_sniffer (gdbarch, dwarf2_frame_sniffer);
  frame_unwind_append_sniffer (gdbarch, sh64_frame_sniffer);

  return gdbarch;
}
