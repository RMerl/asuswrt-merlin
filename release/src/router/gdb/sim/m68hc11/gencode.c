/* gencode.c -- Motorola 68HC11 & 68HC12 Emulator Generator
   Copyright 1999, 2000, 2001, 2002, 2007 Free Software Foundation, Inc.
   Written by Stephane Carrez (stcarrez@nerim.fr)

This file is part of GDB, GAS, and the GNU binutils.

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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "ansidecl.h"
#include "opcode/m68hc11.h"

#define TABLE_SIZE(X)	    (sizeof(X) / sizeof(X[0]))

/* Combination of CCR flags.  */
#define M6811_ZC_BIT	M6811_Z_BIT|M6811_C_BIT
#define M6811_NZ_BIT	M6811_N_BIT|M6811_Z_BIT
#define M6811_NZV_BIT	M6811_N_BIT|M6811_Z_BIT|M6811_V_BIT
#define M6811_NZC_BIT	M6811_N_BIT|M6811_Z_BIT|M6811_C_BIT
#define M6811_NVC_BIT	M6811_N_BIT|M6811_V_BIT|M6811_C_BIT
#define M6811_ZVC_BIT	M6811_Z_BIT|M6811_V_BIT|M6811_C_BIT
#define M6811_NZVC_BIT	M6811_ZVC_BIT|M6811_N_BIT
#define M6811_HNZVC_BIT M6811_NZVC_BIT|M6811_H_BIT
#define M6811_HNVC_BIT  M6811_NVC_BIT|M6811_H_BIT
#define M6811_VC_BIT    M6811_V_BIT|M6811_C_BIT

/* Flags when the insn only changes some CCR flags.  */
#define CHG_NONE	0,0,0
#define CHG_Z		0,0,M6811_Z_BIT
#define CHG_C		0,0,M6811_C_BIT
#define CHG_ZVC		0,0,M6811_ZVC_BIT
#define CHG_NZC         0,0,M6811_NZC_BIT
#define CHG_NZV		0,0,M6811_NZV_BIT
#define CHG_NZVC	0,0,M6811_NZVC_BIT
#define CHG_HNZVC	0,0,M6811_HNZVC_BIT
#define CHG_ALL		0,0,0xff

/* The insn clears and changes some flags.  */
#define CLR_I		0,M6811_I_BIT,0
#define CLR_C		0,M6811_C_BIT,0
#define CLR_V		0,M6811_V_BIT,0
#define CLR_V_CHG_ZC	0,M6811_V_BIT,M6811_ZC_BIT
#define CLR_V_CHG_NZ	0,M6811_V_BIT,M6811_NZ_BIT
#define CLR_V_CHG_ZVC	0,M6811_V_BIT,M6811_ZVC_BIT
#define CLR_N_CHG_ZVC	0,M6811_N_BIT,M6811_ZVC_BIT /* Used by lsr */
#define CLR_VC_CHG_NZ   0,M6811_VC_BIT,M6811_NZ_BIT

/* The insn sets some flags.  */
#define SET_I		M6811_I_BIT,0,0
#define SET_C		M6811_C_BIT,0,0
#define SET_V		M6811_V_BIT,0,0
#define SET_Z_CLR_NVC	M6811_Z_BIT,M6811_NVC_BIT,0
#define SET_C_CLR_V_CHG_NZ M6811_C_BIT,M6811_V_BIT,M6811_NZ_BIT
#define SET_Z_CHG_HNVC  M6811_Z_BIT,0,M6811_HNVC_BIT

#define _M 0xff

static int cpu_type;

struct m6811_opcode_pattern 
{
  const char *name;
  const char *pattern;
  const char *ccr_update;
};

/*
 *  { "test", M6811_OP_NONE, 1, 0x00, 5, _M,  CHG_NONE },
 * Name -+					 +---- Insn CCR changes
 * Format  ------+			   +---------- Max # cycles
 * Size	    -----------------+	      +--------------- Min # cycles
 *				 +-------------------- Opcode
 */
struct m6811_opcode_pattern m6811_opcode_patterns[] = {
  /* Move 8 and 16 bits.  We need two implementations: one that sets the
     flags and one that preserve them.	*/
  { "movtst8",	"dst8 = src8",	 "cpu_ccr_update_tst8 (proc, dst8)" },
  { "movtst16", "dst16 = src16", "cpu_ccr_update_tst16 (proc, dst16)" },
  { "mov8",	"dst8 = src8" },
  { "mov16",	"dst16 = src16" },
  { "lea16",	"dst16 = addr" },

  /* Conditional branches.  'addr' is the address of the branch.  */
  { "bra", "cpu_set_pc (proc, addr)" },
  { "bhi",
   "if ((cpu_get_ccr (proc) & (M6811_C_BIT|M6811_Z_BIT)) == 0)\n@ \
     cpu_set_pc (proc, addr)" },
  { "bls",
    "if ((cpu_get_ccr (proc) & (M6811_C_BIT|M6811_Z_BIT)))\n@ \
     cpu_set_pc (proc, addr)" },
  { "bcc", "if (!cpu_get_ccr_C (proc))\n@ cpu_set_pc (proc, addr)" },
  { "bcs", "if (cpu_get_ccr_C (proc))\n@ cpu_set_pc (proc, addr)" },
  { "bne", "if (!cpu_get_ccr_Z (proc))\n@ cpu_set_pc (proc, addr)" },
  { "beq", "if (cpu_get_ccr_Z (proc))\n@ cpu_set_pc (proc, addr)" },
  { "bvc", "if (!cpu_get_ccr_V (proc))\n@ cpu_set_pc (proc, addr)" },
  { "bvs", "if (cpu_get_ccr_V (proc))\n@ cpu_set_pc (proc, addr)" },
  { "bpl", "if (!cpu_get_ccr_N (proc))\n@ cpu_set_pc (proc, addr)" },
  { "bmi", "if (cpu_get_ccr_N (proc))\n@ cpu_set_pc (proc, addr)" },
  { "bge", "if ((cpu_get_ccr_N (proc) ^ cpu_get_ccr_V (proc)) == 0)\n@ cpu_set_pc (proc, addr)" },
  { "blt", "if ((cpu_get_ccr_N (proc) ^ cpu_get_ccr_V (proc)))\n@ cpu_set_pc (proc, addr)" },
  { "bgt",
    "if ((cpu_get_ccr_Z (proc) | (cpu_get_ccr_N (proc) ^ cpu_get_ccr_V (proc))) == 0)\n@ \
     cpu_set_pc (proc, addr)" },
  { "ble",
    "if ((cpu_get_ccr_Z (proc) | (cpu_get_ccr_N (proc) ^ cpu_get_ccr_V (proc))))\n@ \
     cpu_set_pc (proc, addr)" },

  /* brclr and brset perform a test and a conditional jump at the same
     time.  Flags are not changed.  */
  { "brclr8",
    "if ((src8 & dst8) == 0)\n@	 cpu_set_pc (proc, addr)" },
  { "brset8",
    "if (((~src8) & dst8) == 0)\n@  cpu_set_pc (proc, addr)" },
  

  { "rts11",  "addr = cpu_m68hc11_pop_uint16 (proc); cpu_set_pc (proc, addr); cpu_return(proc)" },
  { "rts12",  "addr = cpu_m68hc12_pop_uint16 (proc); cpu_set_pc (proc, addr); cpu_return(proc)" },

  { "mul16", "dst16 = ((uint16) src8 & 0x0FF) * ((uint16) dst8 & 0x0FF)",
    "cpu_set_ccr_C (proc, src8 & 0x80)" },
  { "neg8", "dst8 = - src8",
    "cpu_set_ccr_C (proc, src8 == 0); cpu_ccr_update_tst8 (proc, dst8)" },
  { "com8", "dst8 = ~src8",
    "cpu_set_ccr_C (proc, 1); cpu_ccr_update_tst8 (proc, dst8);" },
  { "clr8", "dst8 = 0",
    "cpu_set_ccr (proc, (cpu_get_ccr (proc) & (M6811_S_BIT|M6811_X_BIT|M6811_H_BIT| \
M6811_I_BIT)) | M6811_Z_BIT)"},
  { "clr16","dst16 = 0",
    "cpu_set_ccr (proc, (cpu_get_ccr (proc) & (M6811_S_BIT|M6811_X_BIT|M6811_H_BIT| \
M6811_I_BIR)) | M6811_Z_BIT)"},

  /* 8-bits shift and rotation.	 */
  { "lsr8",  "dst8 = src8 >> 1",
    "cpu_set_ccr_C (proc, src8 & 1); cpu_ccr_update_shift8 (proc, dst8)" },
  { "lsl8",  "dst8 = src8 << 1",
    "cpu_set_ccr_C (proc, (src8 & 0x80) >> 7); cpu_ccr_update_shift8 (proc, dst8)" },
  { "asr8",  "dst8 = (src8 >> 1) | (src8 & 0x80)",
    "cpu_set_ccr_C (proc, src8 & 1); cpu_ccr_update_shift8 (proc, dst8)" },
  { "ror8",  "dst8 = (src8 >> 1) | (cpu_get_ccr_C (proc) << 7)",
    "cpu_set_ccr_C (proc, src8 & 1); cpu_ccr_update_shift8 (proc, dst8)" },
  { "rol8",  "dst8 = (src8 << 1) | (cpu_get_ccr_C (proc))",
    "cpu_set_ccr_C (proc, (src8 & 0x80) >> 7); cpu_ccr_update_shift8 (proc, dst8)" },

  /* 16-bits shift instructions.  */
  { "lsl16",  "dst16 = src16 << 1",
    "cpu_set_ccr_C (proc, (src16&0x8000) >> 15); cpu_ccr_update_shift16 (proc, dst16)"},
  { "lsr16",  "dst16 = src16 >> 1",
    "cpu_set_ccr_C (proc, src16 & 1); cpu_ccr_update_shift16 (proc, dst16)"},

  { "dec8", "dst8 = src8 - 1", "cpu_ccr_update_tst8 (proc, dst8)" },
  { "inc8", "dst8 = src8 + 1", "cpu_ccr_update_tst8 (proc, dst8)" },
  { "tst8", 0, "cpu_set_ccr_C (proc, 0); cpu_ccr_update_tst8 (proc, src8)" },

  { "sub8", "cpu_ccr_update_sub8 (proc, dst8 - src8, dst8, src8);\
dst8 = dst8 - src8", 0 },
  { "add8", "cpu_ccr_update_add8 (proc, dst8 + src8, dst8, src8);\
dst8 = dst8 + src8", 0 },
  { "sbc8", "if (cpu_get_ccr_C (proc))\n@ \
{\n\
  cpu_ccr_update_sub8 (proc, dst8 - src8 - 1, dst8, src8);\n\
  dst8 = dst8 - src8 - 1;\n\
}\n\
else\n\
{\n\
  cpu_ccr_update_sub8 (proc, dst8 - src8, dst8, src8);\n\
  dst8 = dst8 - src8;\n\
}", 0 },
  { "adc8", "if (cpu_get_ccr_C (proc))\n@ \
{\n\
  cpu_ccr_update_add8 (proc, dst8 + src8 + 1, dst8, src8);\n\
  dst8 = dst8 + src8 + 1;\n\
}\n\
else\n\
{\n\
  cpu_ccr_update_add8 (proc, dst8 + src8, dst8, src8);\n\
  dst8 = dst8 + src8;\n\
}",
    0 },

  /* 8-bits logical operations.	 */
  { "and8", "dst8 = dst8 & src8", "cpu_ccr_update_tst8 (proc, dst8)" },
  { "eor8", "dst8 = dst8 ^ src8", "cpu_ccr_update_tst8 (proc, dst8)" },
  { "or8",  "dst8 = dst8 | src8", "cpu_ccr_update_tst8 (proc, dst8)" },
  { "bclr8","dst8 = (~dst8) & src8", "cpu_ccr_update_tst8 (proc, dst8)" },

  /* 16-bits add and subtract instructions.  */
  { "sub16", "cpu_ccr_update_sub16 (proc, dst16 - src16, dst16, src16);\
dst16 = dst16 - src16", 0 },
  { "add16", "cpu_ccr_update_add16 (proc, dst16 + src16, dst16, src16);\
dst16 = dst16 + src16", 0 },
  { "inc16", "dst16 = src16 + 1", "cpu_set_ccr_Z (proc, dst16 == 0)" },
  { "dec16", "dst16 = src16 - 1", "cpu_set_ccr_Z (proc, dst16 == 0)" },

  /* Special increment/decrement for the stack pointer:
     flags are not changed.  */
  { "ins16", "dst16 = src16 + 1" },
  { "des16", "dst16 = src16 - 1" },
  
  { "jsr_11_16", "cpu_m68hc11_push_uint16 (proc, cpu_get_pc (proc)); cpu_call (proc, addr)"},
  { "jsr_12_16", "cpu_m68hc12_push_uint16 (proc, cpu_get_pc (proc)); cpu_call (proc, addr)"},

  /* xgdx and xgdx patterns. Flags are not changed.  */
  { "xgdxy16", "dst16 = cpu_get_d (proc); cpu_set_d (proc, src16)"},
  { "stop", "cpu_special (proc, M6811_STOP)"},

  /* tsx, tsy, txs, tys don't affect the flags.	 Sp value is corrected
     by +/- 1.	*/
  { "tsxy16", "dst16 = src16 + 1;"},
  { "txys16", "dst16 = src16 - 1;"},

  /* Add b to X or Y with an unsigned extension 8->16.	Flags not changed.  */
  { "abxy16","dst16 = dst16 + (uint16) src8"},

  /* After 'daa', the Z flag is undefined. Mark it as changed.	*/
  { "daa8",  "cpu_special (proc, M6811_DAA)" },
  { "nop",  0 },


  /* Integer divide:
     (parallel (set IX (div D IX))
	       (set D  (mod D IX)))  */
  { "idiv16", "if (src16 == 0)\n{\n\
dst16 = 0xffff;\
}\nelse\n{\n\
cpu_set_d (proc, dst16 % src16);\
dst16 = dst16 / src16;\
}",
  "cpu_set_ccr_Z (proc, dst16 == 0); cpu_set_ccr_V (proc, 0);\
cpu_set_ccr_C (proc, src16 == 0)" },

  /* Fractional divide:
     (parallel (set IX (div (mul D 65536) IX)
	       (set D  (mod (mul D 65536) IX))))  */
  { "fdiv16", "if (src16 <= dst16 )\n{\n\
dst16 = 0xffff;\n\
cpu_set_ccr_Z (proc, 0);\n\
cpu_set_ccr_V (proc, 1);\n\
cpu_set_ccr_C (proc, dst16 == 0);\n\
}\nelse\n{\n\
unsigned long l = (unsigned long) (dst16) << 16;\n\
cpu_set_d (proc, (uint16) (l % (unsigned long) (src16)));\n\
dst16 = (uint16) (l / (unsigned long) (src16));\n\
cpu_set_ccr_V (proc, 0);\n\
cpu_set_ccr_C (proc, 0);\n\
cpu_set_ccr_Z (proc, dst16 == 0);\n\
}", 0 },

  /* Operations to get/set the CCR.  */
  { "clv",  0, "cpu_set_ccr_V (proc, 0)" },
  { "sev",  0, "cpu_set_ccr_V (proc, 1)" },
  { "clc",  0, "cpu_set_ccr_C (proc, 0)" },
  { "sec",  0, "cpu_set_ccr_C (proc, 1)" },
  { "cli",  0, "cpu_set_ccr_I (proc, 0)" },
  { "sei",  0, "cpu_set_ccr_I (proc, 1)" },

  /* Some special instructions are implemented by 'cpu_special'.  */
  { "rti11",  "cpu_special (proc, M6811_RTI)" },
  { "rti12",  "cpu_special (proc, M6812_RTI)" },
  { "wai",  "cpu_special (proc, M6811_WAI)" },
  { "test", "cpu_special (proc, M6811_TEST)" },
  { "swi",  "cpu_special (proc, M6811_SWI)" },
  { "syscall","cpu_special (proc, M6811_EMUL_SYSCALL)" },

  { "page2", "cpu_page2_interp (proc)", 0 },
  { "page3", "cpu_page3_interp (proc)", 0 },
  { "page4", "cpu_page4_interp (proc)", 0 },

  /* 68HC12 special instructions.  */
  { "bgnd",  "cpu_special (proc, M6812_BGND)" },
  { "call8", "cpu_special (proc, M6812_CALL)" },
  { "call_ind", "cpu_special (proc, M6812_CALL_INDIRECT)" },
  { "dbcc8", "cpu_dbcc (proc)" },
  { "ediv",  "cpu_special (proc, M6812_EDIV)" },
  { "emul",  "{ uint32 src1 = (uint32) cpu_get_d (proc);\
  uint32 src2 = (uint32) cpu_get_y (proc);\
  src1 *= src2;\
  cpu_set_d (proc, src1);\
  cpu_set_y (proc, src1 >> 16);\
  cpu_set_ccr_Z (proc, src1 == 0);\
  cpu_set_ccr_C (proc, src1 & 0x08000);\
  cpu_set_ccr_N (proc, src1 & 0x80000000);}" },
  { "emuls",  "cpu_special (proc, M6812_EMULS)" },
  { "mem",   "cpu_special (proc, M6812_MEM)" },
  { "rtc",   "cpu_special (proc, M6812_RTC)" },
  { "emacs", "cpu_special (proc, M6812_EMACS)" },
  { "idivs", "cpu_special (proc, M6812_IDIVS)" },
  { "edivs", "cpu_special (proc, M6812_EDIVS)" },
  { "exg8",  "cpu_exg (proc, src8)" },
  { "move8", "cpu_move8 (proc, op)" },
  { "move16","cpu_move16 (proc, op)" },

  { "max8",  "cpu_ccr_update_sub8 (proc, dst8 - src8, dst8, src8);\
              if (dst8 < src8) dst8 = src8" },
  { "min8",  "cpu_ccr_update_sub8 (proc, dst8 - src8, dst8, src8);\
              if (dst8 > src8) dst8 = src8" },
  { "max16", "cpu_ccr_update_sub16 (proc, dst16 - src16, dst16, src16);\
              if (dst16 < src16) dst16 = src16" },
  { "min16", "cpu_ccr_update_sub16 (proc, dst16 - src16, dst16, src16);\
              if (dst16 > src16) dst16 = src16" },

  { "rev",   "cpu_special (proc, M6812_REV);" },
  { "revw",  "cpu_special (proc, M6812_REVW);" },
  { "wav",   "cpu_special (proc, M6812_WAV);" },
  { "tbl8",  "cpu_special (proc, M6812_ETBL);" },
  { "tbl16", "cpu_special (proc, M6812_ETBL);" }
};

/* Definition of an opcode of the 68HC11.  */
struct m6811_opcode_def
{
  const char	 *name;
  const char	 *operands;
  const char	 *insn_pattern;
  unsigned char	 insn_size;
  unsigned char	 insn_code;
  unsigned char	 insn_min_cycles;
  unsigned char	 insn_max_cycles;
  unsigned char	 set_flags_mask;
  unsigned char	 clr_flags_mask;
  unsigned char	 chg_flags_mask;
};


/*
 *  { "dex", "x->x", "dec16", 1, 0x00, 5, _M,  CHG_NONE },
 * Name -+					 +----- Insn CCR changes
 * Operands  ---+			  +------------ Max # cycles
 * Pattern   -----------+	       +--------------- Min # cycles
 * Size	     -----------------+	  +-------------------- Opcode
 *
 * Operands   Fetch operand		Save result
 * -------    --------------		------------
 * x->x	      src16 = x			x = dst16
 * d->d	      src16 = d			d = dst16
 * b,a->a     src8 = b dst8 = a		a = dst8
 * sp->x      src16 = sp		x = dst16
 * (sp)->a    src8 = pop8		a = dst8
 * a->(sp)    src8 = a			push8 dst8
 * (x)->(x)   src8 = (IND, X)		(IND, X) = dst8
 * (y)->a     src8 = (IND, Y)		a = dst8
 * ()->b      src8 = (EXT)		b = dst8
 */
struct m6811_opcode_def m6811_page1_opcodes[] = {
  { "test", 0,		0,	     1, 0x00,  5, _M,  CHG_NONE },
  { "nop",  0,		0,	     1, 0x01,  2,  2,  CHG_NONE },
  { "idiv", "x,d->x",	"idiv16",    1, 0x02,  3, 41,  CLR_V_CHG_ZC},
  { "fdiv", "x,d->x",	"fdiv16",    1, 0x03,  3, 41,  CHG_ZVC},
  { "lsrd", "d->d",	"lsr16",     1, 0x04,  3,  3,  CLR_N_CHG_ZVC },
  { "asld", "d->d",	"lsl16",     1, 0x05,  3,  3,  CHG_NZVC },
  { "tap",  "a->ccr",	"mov8",	     1, 0x06,  2,  2,  CHG_ALL},
  { "tpa",  "ccr->a",	"mov8",	     1, 0x07,  2,  2,  CHG_NONE },
  { "inx",  "x->x",	"inc16",     1, 0x08,  3,  3,  CHG_Z },
  { "dex",  "x->x",	"dec16",     1, 0x09,  3,  3,  CHG_Z },
  { "clv",  0,		0,	     1, 0x0a,  2,  2,  CLR_V },
  { "sev",  0,		0,	     1, 0x0b,  2,  2,  SET_V },
  { "clc",  0,		0,	     1, 0x0c,  2,  2,  CLR_C },
  { "sec",  0,		0,	     1, 0x0d,  2,  2,  SET_C },
  { "cli",  0,		0,	     1, 0x0e,  2,  2,  CLR_I },
  { "sei",  0,		0,	     1, 0x0f,  2,  2,  SET_I },
  { "sba",  "b,a->a",	"sub8",	     1, 0x10,  2,  2,  CHG_NZVC },
  { "cba",  "b,a",	"sub8",	     1, 0x11,  2,  2,  CHG_NZVC },
  { "brset","*,#,r",	"brset8",    4, 0x12,  6,  6, CHG_NONE },
  { "brclr","*,#,r",	"brclr8",    4, 0x13,  6,  6, CHG_NONE },
  { "bset", "*,#->*",	"or8",	     3, 0x14,  6,  6, CLR_V_CHG_NZ },
  { "bclr", "*,#->*",	"bclr8",     3, 0x15,  6,  6, CLR_V_CHG_NZ },
  { "tab",  "a->b",	"movtst8",   1, 0x16,  2,  2, CLR_V_CHG_NZ },
  { "tba",  "b->a",	"movtst8",   1, 0x17,  2,  2, CLR_V_CHG_NZ },
  { "page2", 0,		"page2",     1, 0x18,  0,  0, CHG_NONE },
  { "page3", 0,		"page3",     1, 0x1a,  0,  0, CHG_NONE },

  /* After 'daa', the Z flag is undefined.  Mark it as changed.	 */
  { "daa",  "",	        "daa8",	     1, 0x19,  2,  2, CHG_NZVC },
  { "aba",  "b,a->a",	"add8",	     1, 0x1b,  2,  2, CHG_HNZVC},
  { "bset", "(x),#->(x)","or8",	     3, 0x1c,  7,  7, CLR_V_CHG_NZ },
  { "bclr", "(x),#->(x)","bclr8",    3, 0x1d,  7,  7, CLR_V_CHG_NZ },
  { "brset","(x),#,r",	"brset8",    4, 0x1e,  7,  7, CHG_NONE },
  { "brclr","(x),#,r",	"brclr8",    4, 0x1f,  7,  7, CHG_NONE },

  /* Relative branch.  All of them take 3 bytes.  Flags not changed.  */
  { "bra",  "r",	0,	     2, 0x20,  3,  3, CHG_NONE },
  { "brn",  "r",	"nop",	     2, 0x21,  3,  3, CHG_NONE },
  { "bhi",  "r",	0,	     2, 0x22,  3,  3, CHG_NONE },
  { "bls",  "r",	0,	     2, 0x23,  3,  3, CHG_NONE },
  { "bcc",  "r",	0,	     2, 0x24,  3,  3, CHG_NONE },
  { "bcs",  "r",	0,	     2, 0x25,  3,  3, CHG_NONE },
  { "bne",  "r",	0,	     2, 0x26,  3,  3, CHG_NONE },
  { "beq",  "r",	0,	     2, 0x27,  3,  3, CHG_NONE },
  { "bvc",  "r",	0,	     2, 0x28,  3,  3, CHG_NONE },
  { "bvs",  "r",	0,	     2, 0x29,  3,  3, CHG_NONE },
  { "bpl",  "r",	0,	     2, 0x2a,  3,  3, CHG_NONE },
  { "bmi",  "r",	0,	     2, 0x2b,  3,  3, CHG_NONE },
  { "bge",  "r",	0,	     2, 0x2c,  3,  3, CHG_NONE },
  { "blt",  "r",	0,	     2, 0x2d,  3,  3, CHG_NONE },
  { "bgt",  "r",	0,	     2, 0x2e,  3,  3, CHG_NONE },
  { "ble",  "r",	0,	     2, 0x2f,  3,  3, CHG_NONE },

  { "tsx",  "sp->x",	"tsxy16",    1, 0x30,  3,  3, CHG_NONE },
  { "ins",  "sp->sp",	"ins16",     1, 0x31,  3,  3, CHG_NONE },
  { "pula", "(sp)->a",	"mov8",	     1, 0x32,  4,  4, CHG_NONE },
  { "pulb", "(sp)->b",	"mov8",	     1, 0x33,  4,  4, CHG_NONE },
  { "des",  "sp->sp",	"des16",     1, 0x34,  3,  3, CHG_NONE },
  { "txs",  "x->sp",	"txys16",    1, 0x35,  3,  3, CHG_NONE },
  { "psha", "a->(sp)",	"mov8",	     1, 0x36,  3,  3, CHG_NONE },
  { "pshb", "b->(sp)",	"mov8",	     1, 0x37,  3,  3, CHG_NONE },
  { "pulx", "(sp)->x",	"mov16",     1, 0x38,  5,  5, CHG_NONE },
  { "rts",  0,		"rts11",     1, 0x39,  5,  5, CHG_NONE },
  { "abx",  "b,x->x",	"abxy16",    1, 0x3a,  3,  3, CHG_NONE },
  { "rti",  0,		"rti11",     1, 0x3b, 12, 12, CHG_ALL},
  { "pshx", "x->(sp)",	"mov16",     1, 0x3c,  4,  4, CHG_NONE },
  { "mul",  "b,a->d",	"mul16",     1, 0x3d,  3, 10, CHG_C },
  { "wai",  0,		0,	     1, 0x3e, 14, _M, CHG_NONE },
  { "swi",  0,		0,	     1, 0x3f, 14, _M, CHG_NONE },
  { "nega", "a->a",	"neg8",	     1, 0x40,  2,  2, CHG_NZVC },
  { "syscall", "",	"syscall",   1, 0x41,  2,  2, CHG_NONE },
  { "coma", "a->a",	"com8",	     1, 0x43,  2,  2, SET_C_CLR_V_CHG_NZ },
  { "lsra", "a->a",	"lsr8",	     1, 0x44,  2,  2, CLR_N_CHG_ZVC},
  { "rora", "a->a",	"ror8",	     1, 0x46,  2,  2, CHG_NZVC },
  { "asra", "a->a",	"asr8",	     1, 0x47,  2,  2, CHG_NZVC },
  { "asla", "a->a",	"lsl8",	     1, 0x48,  2,  2, CHG_NZVC },
  { "rola", "a->a",	"rol8",	     1, 0x49,  2,  2, CHG_NZVC },
  { "deca", "a->a",	"dec8",	     1, 0x4a,  2,  2, CHG_NZV },
  { "inca", "a->a",	"inc8",	     1, 0x4c,  2,  2, CHG_NZV },
  { "tsta", "a",	"tst8",	     1, 0x4d,  2,  2, CLR_V_CHG_NZ },
  { "clra", "->a",	"clr8",	     1, 0x4f,  2,  2, SET_Z_CLR_NVC },
  { "negb", "b->b",	"neg8",	     1, 0x50,  2,  2, CHG_NZVC },
  { "comb", "b->b",	"com8",	     1, 0x53,  2,  2, SET_C_CLR_V_CHG_NZ },
  { "lsrb", "b->b",	"lsr8",	     1, 0x54,  2,  2, CLR_N_CHG_ZVC },
  { "rorb", "b->b",	"ror8",	     1, 0x56,  2,  2, CHG_NZVC },
  { "asrb", "b->b",	"asr8",	     1, 0x57,  2,  2, CHG_NZVC },
  { "aslb", "b->b",	"lsl8",	     1, 0x58,  2,  2, CHG_NZVC },
  { "rolb", "b->b",	"rol8",	     1, 0x59,  2,  2, CHG_NZVC },
  { "decb", "b->b",	"dec8",	     1, 0x5a,  2,  2, CHG_NZV },
  { "incb", "b->b",	"inc8",	     1, 0x5c,  2,  2, CHG_NZV },
  { "tstb", "b",	"tst8",	     1, 0x5d,  2,  2, CLR_V_CHG_NZ },
  { "clrb", "->b",	"clr8",	     1, 0x5f,  2,  2, SET_Z_CLR_NVC },
  { "neg",  "(x)->(x)", "neg8",	     2, 0x60,  6,  6, CHG_NZVC },
  { "com",  "(x)->(x)", "com8",	     2, 0x63,  6,  6, SET_C_CLR_V_CHG_NZ },
  { "lsr",  "(x)->(x)", "lsr8",	     2, 0x64,  6,  6, CLR_N_CHG_ZVC },
  { "ror",  "(x)->(x)", "ror8",	     2, 0x66,  6,  6, CHG_NZVC },
  { "asr",  "(x)->(x)", "asr8",	     2, 0x67,  6,  6, CHG_NZVC },
  { "asl",  "(x)->(x)", "lsl8",	     2, 0x68,  6,  6, CHG_NZVC },
  { "rol",  "(x)->(x)", "rol8",	     2, 0x69,  6,  6, CHG_NZVC },
  { "dec",  "(x)->(x)", "dec8",	     2, 0x6a,  6,  6, CHG_NZV },
  { "inc",  "(x)->(x)", "inc8",	     2, 0x6c,  6,  6, CHG_NZV },
  { "tst",  "(x)",	"tst8",	     2, 0x6d,  6,  6, CLR_V_CHG_NZ },
  { "jmp",  "&(x)",	"bra",	     2, 0x6e,  3,  3, CHG_NONE },
  { "clr",  "->(x)",	"clr8",	     2, 0x6f,  6,  6, SET_Z_CLR_NVC },
  { "neg",  "()->()",	"neg8",	     3, 0x70,  6,  6, CHG_NZVC },
  { "com",  "()->()",	"com8",	     3, 0x73,  6,  6, SET_C_CLR_V_CHG_NZ },
  { "lsr",  "()->()",	"lsr8",	     3, 0x74,  6,  6, CLR_V_CHG_ZVC },
  { "ror",  "()->()",	"ror8",	     3, 0x76,  6,  6, CHG_NZVC },
  { "asr",  "()->()",	"asr8",	     3, 0x77,  6,  6, CHG_NZVC },
  { "asl",  "()->()",	"lsl8",	     3, 0x78,  6,  6, CHG_NZVC },
  { "rol",  "()->()",	"rol8",	     3, 0x79,  6,  6, CHG_NZVC },
  { "dec",  "()->()",	"dec8",	     3, 0x7a,  6,  6, CHG_NZV },
  { "inc",  "()->()",	"inc8",	     3, 0x7c,  6,  6, CHG_NZV },
  { "tst",  "()",	"tst8",	     3, 0x7d,  6,  6, CLR_V_CHG_NZ },
  { "jmp",  "&()",	"bra",	     3, 0x7e,  3,  3, CHG_NONE },
  { "clr",  "->()",	"clr8",	     3, 0x7f,  6,  6, SET_Z_CLR_NVC },
  { "suba", "#,a->a",	"sub8",	     2, 0x80,  2,  2, CHG_NZVC },
  { "cmpa", "#,a",	"sub8",	     2, 0x81,  2,  2, CHG_NZVC },
  { "sbca", "#,a->a",	"sbc8",	     2, 0x82,  2,  2, CHG_NZVC },
  { "subd", "#,d->d",	"sub16",     3, 0x83,  4,  4, CHG_NZVC },
  { "anda", "#,a->a",	"and8",	     2, 0x84,  2,  2, CLR_V_CHG_NZ },
  { "bita", "#,a",	"and8",	     2, 0x85,  2,  2, CLR_V_CHG_NZ },
  { "ldaa", "#->a",	"movtst8",   2, 0x86,  2,  2, CLR_V_CHG_NZ },
  { "eora", "#,a->a",	"eor8",	     2, 0x88,  2,  2, CLR_V_CHG_NZ },
  { "adca", "#,a->a",	"adc8",	     2, 0x89,  2,  2, CHG_HNZVC },
  { "oraa", "#,a->a",	"or8",	     2, 0x8a,  2,  2, CLR_V_CHG_NZ },
  { "adda", "#,a->a",	"add8",	     2, 0x8b,  2,  2, CHG_HNZVC },
  { "cmpx", "#,x",	"sub16",     3, 0x8c,  4,  4, CHG_NZVC },
  { "bsr",  "r",	"jsr_11_16", 2, 0x8d,  6,  6, CHG_NONE },
  { "lds",  "#->sp",	"movtst16",  3, 0x8e,  3,  3, CLR_V_CHG_NZ },
  { "xgdx", "x->x",	"xgdxy16",   1, 0x8f,  3,  3, CHG_NONE },
  { "suba", "*,a->a",	"sub8",	     2, 0x90,  3,  3, CHG_NZVC },
  { "cmpa", "*,a",	"sub8",	     2, 0x91,  3,  3, CHG_NZVC },
  { "sbca", "*,a->a",	"sbc8",	     2, 0x92,  3,  3, CHG_NZVC },
  { "subd", "*,d->d",	"sub16",     2, 0x93,  5,  5, CHG_NZVC },
  { "anda", "*,a->a",	"and8",	     2, 0x94,  3,  3, CLR_V_CHG_NZ },
  { "bita", "*,a",	"and8",	     2, 0x95,  3,  3, CLR_V_CHG_NZ },
  { "ldaa", "*->a",	"movtst8",   2, 0x96,  3,  3, CLR_V_CHG_NZ },
  { "staa", "a->*",	"movtst8",   2, 0x97,  3,  3, CLR_V_CHG_NZ },
  { "eora", "*,a->a",	"eor8",	     2, 0x98,  3,  3, CLR_V_CHG_NZ },
  { "adca", "*,a->a",	"adc8",	     2, 0x99,  3,  3, CHG_HNZVC },
  { "oraa", "*,a->a",	"or8",	     2, 0x9a,  3,  3, CLR_V_CHG_NZ },
  { "adda", "*,a->a",	"add8",	     2, 0x9b,  3,  3, CHG_HNZVC },
  { "cmpx", "*,x",	"sub16",     2, 0x9c,  5,  5, CHG_NZVC },
  { "jsr",  "*",	"jsr_11_16", 2, 0x9d,  5,  5, CHG_NONE },
  { "lds",  "*->sp",	"movtst16",  2, 0x9e,  4,  4, CLR_V_CHG_NZ },
  { "sts",  "sp->*",	"movtst16",  2, 0x9f,  4,  4, CLR_V_CHG_NZ },
  { "suba", "(x),a->a", "sub8",	     2, 0xa0,  4,  4, CHG_NZVC },
  { "cmpa", "(x),a",	"sub8",	     2, 0xa1,  4,  4, CHG_NZVC },
  { "sbca", "(x),a->a", "sbc8",	     2, 0xa2,  4,  4, CHG_NZVC },
  { "subd", "(x),d->d", "sub16",     2, 0xa3,  6,  6, CHG_NZVC },
  { "anda", "(x),a->a", "and8",	     2, 0xa4,  4,  4, CLR_V_CHG_NZ },
  { "bita", "(x),a",	"and8",	     2, 0xa5,  4,  4, CLR_V_CHG_NZ },
  { "ldaa", "(x)->a",	"movtst8",   2, 0xa6,  4,  4, CLR_V_CHG_NZ },
  { "staa", "a->(x)",	"movtst8",   2, 0xa7,  4,  4, CLR_V_CHG_NZ },
  { "eora", "(x),a->a", "eor8",	     2, 0xa8,  4,  4, CLR_V_CHG_NZ },
  { "adca", "(x),a->a", "adc8",	     2, 0xa9,  4,  4, CHG_HNZVC },
  { "oraa", "(x),a->a", "or8",	     2, 0xaa,  4,  4, CLR_V_CHG_NZ },
  { "adda", "(x),a->a", "add8",	     2, 0xab,  4,  4, CHG_HNZVC },
  { "cmpx", "(x),x",	"sub16",     2, 0xac,  6,  6, CHG_NZVC },
  { "jsr",  "&(x)",	"jsr_11_16", 2, 0xad,  6,  6, CHG_NONE },
  { "lds",  "(x)->sp",	"movtst16",  2, 0xae,  5,  5, CLR_V_CHG_NZ },
  { "sts",  "sp->(x)",	"movtst16",  2, 0xaf,  5,  5, CLR_V_CHG_NZ },
  { "suba", "(),a->a",	"sub8",	     3, 0xb0,  4,  4, CHG_NZVC },
  { "cmpa", "(),a",	"sub8",	     3, 0xb1,  4,  4, CHG_NZVC },
  { "sbca", "(),a->a",	"sbc8",	     3, 0xb2,  4,  4, CHG_NZVC },
  { "subd", "(),d->d",	"sub16",     3, 0xb3,  6,  6, CHG_NZVC },
  { "anda", "(),a->a",	"and8",	     3, 0xb4,  4,  4, CLR_V_CHG_NZ },
  { "bita", "(),a",	"and8",	     3, 0xb5,  4,  4, CLR_V_CHG_NZ },
  { "ldaa", "()->a",	"movtst8",   3, 0xb6,  4,  4, CLR_V_CHG_NZ },
  { "staa", "a->()",	"movtst8",   3, 0xb7,  4,  4, CLR_V_CHG_NZ },
  { "eora", "(),a->a",	"eor8",	     3, 0xb8,  4,  4, CLR_V_CHG_NZ },
  { "adca", "(),a->a",	"adc8",	     3, 0xb9,  4,  4, CHG_HNZVC },
  { "oraa", "(),a->a",	"or8",	     3, 0xba,  4,  4, CLR_V_CHG_NZ },
  { "adda", "(),a->a",	"add8",	     3, 0xbb,  4,  4, CHG_HNZVC },
  { "cmpx", "(),x",	"sub16",     3, 0xbc,  5,  5, CHG_NZVC },
  { "jsr",  "&()",	"jsr_11_16", 3, 0xbd,  6,  6, CHG_NONE },
  { "lds",  "()->sp",	"movtst16",  3, 0xbe,  5,  5, CLR_V_CHG_NZ },
  { "sts",  "sp->()",	"movtst16",  3, 0xbf,  5,  5, CLR_V_CHG_NZ },
  { "subb", "#,b->b",	"sub8",	     2, 0xc0,  2,  2, CHG_NZVC },
  { "cmpb", "#,b",	"sub8",	     2, 0xc1,  2,  2, CHG_NZVC },
  { "sbcb", "#,b->b",	"sbc8",	     2, 0xc2,  2,  2, CHG_NZVC },
  { "addd", "#,d->d",	"add16",     3, 0xc3,  4,  4, CHG_NZVC },
  { "andb", "#,b->b",	"and8",	     2, 0xc4,  2,  2, CLR_V_CHG_NZ },
  { "bitb", "#,b",	"and8",	     2, 0xc5,  2,  2, CLR_V_CHG_NZ },
  { "ldab", "#->b",	"movtst8",   2, 0xc6,  2,  2, CLR_V_CHG_NZ },
  { "eorb", "#,b->b",	"eor8",	     2, 0xc8,  2,  2, CLR_V_CHG_NZ },
  { "adcb", "#,b->b",	"adc8",	     2, 0xc9,  2,  2, CHG_HNZVC },
  { "orab", "#,b->b",	"or8",	     2, 0xca,  2,  2, CLR_V_CHG_NZ },
  { "addb", "#,b->b",	"add8",	     2, 0xcb,  2,  2, CHG_HNZVC },
  { "ldd",  "#->d",	"movtst16",  3, 0xcc,  3,  3, CLR_V_CHG_NZ },
  { "page4",0,		"page4",     1, 0xcd,  0,  0, CHG_NONE },
  { "ldx",  "#->x",	"movtst16",  3, 0xce,  3,  3, CLR_V_CHG_NZ },
  { "stop", 0,		0,	     1, 0xcf,  2,  2, CHG_NONE },
  { "subb", "*,b->b",	"sub8",	     2, 0xd0,  3,  3, CHG_NZVC },
  { "cmpb", "*,b",	"sub8",	     2, 0xd1,  3,  3, CHG_NZVC },
  { "sbcb", "*,b->b",	"sbc8",	     2, 0xd2,  3,  3, CHG_NZVC },
  { "addd", "*,d->d",	"add16",     2, 0xd3,  5,  5, CHG_NZVC },
  { "andb", "*,b->b",	"and8",	     2, 0xd4,  3,  3, CLR_V_CHG_NZ },
  { "bitb", "*,b",	"and8",	     2, 0xd5,  3,  3, CLR_V_CHG_NZ },
  { "ldab", "*->b",	"movtst8",   2, 0xd6,  3,  3, CLR_V_CHG_NZ },
  { "stab", "b->*",	"movtst8",   2, 0xd7,  3,  3, CLR_V_CHG_NZ },
  { "eorb", "*,b->b",	"eor8",	     2, 0xd8,  3,  3, CLR_V_CHG_NZ },
  { "adcb", "*,b->b",	"adc8",	     2, 0xd9,  3,  3, CHG_HNZVC },
  { "orab", "*,b->b",	"or8",	     2, 0xda,  3,  3, CLR_V_CHG_NZ },
  { "addb", "*,b->b",	"add8",	     2, 0xdb,  3,  3, CHG_HNZVC },
  { "ldd",  "*->d",	"movtst16",  2, 0xdc,  4,  4, CLR_V_CHG_NZ },
  { "std",  "d->*",	"movtst16",  2, 0xdd,  4,  4, CLR_V_CHG_NZ },
  { "ldx",  "*->x",	"movtst16",  2, 0xde,  4,  4, CLR_V_CHG_NZ },
  { "stx",  "x->*",	"movtst16",  2, 0xdf,  4,  4, CLR_V_CHG_NZ },
  { "subb", "(x),b->b", "sub8",	     2, 0xe0,  4,  4, CHG_NZVC },
  { "cmpb", "(x),b",	"sub8",	     2, 0xe1,  4,  4, CHG_NZVC },
  { "sbcb", "(x),b->b", "sbc8",	     2, 0xe2,  4,  4, CHG_NZVC },
  { "addd", "(x),d->d", "add16",     2, 0xe3,  6,  6, CHG_NZVC },
  { "andb", "(x),b->b", "and8",	     2, 0xe4,  4,  4, CLR_V_CHG_NZ },
  { "bitb", "(x),b",	"and8",	     2, 0xe5,  4,  4, CLR_V_CHG_NZ },
  { "ldab", "(x)->b",	"movtst8",   2, 0xe6,  4,  4, CLR_V_CHG_NZ },
  { "stab", "b->(x)",	"movtst8",   2, 0xe7,  4,  4, CLR_V_CHG_NZ },
  { "eorb", "(x),b->b", "eor8",	     2, 0xe8,  4,  4, CLR_V_CHG_NZ },
  { "adcb", "(x),b->b", "adc8",	     2, 0xe9,  4,  4, CHG_HNZVC },
  { "orab", "(x),b->b", "or8",	     2, 0xea,  4,  4, CLR_V_CHG_NZ },
  { "addb", "(x),b->b", "add8",	     2, 0xeb,  4,  4, CHG_HNZVC },
  { "ldd",  "(x)->d",	"movtst16",  2, 0xec,  5,  5, CLR_V_CHG_NZ },
  { "std",  "d->(x)",	"movtst16",  2, 0xed,  5,  5, CLR_V_CHG_NZ },
  { "ldx",  "(x)->x",	"movtst16",  2, 0xee,  5,  5, CLR_V_CHG_NZ },
  { "stx",  "x->(x)",	"movtst16",  2, 0xef,  5,  5, CLR_V_CHG_NZ },
  { "subb", "(),b->b",	"sub8",	     3, 0xf0,  4,  4, CHG_NZVC },
  { "cmpb", "(),b",	"sub8",	     3, 0xf1,  4,  4, CHG_NZVC },
  { "sbcb", "(),b->b",	"sbc8",	     3, 0xf2,  4,  4, CHG_NZVC },
  { "addd", "(),d->d",	"add16",     3, 0xf3,  6,  6, CHG_NZVC },
  { "andb", "(),b->b",	"and8",	     3, 0xf4,  4,  4, CLR_V_CHG_NZ },
  { "bitb", "(),b",	"and8",	     3, 0xf5,  4,  4, CLR_V_CHG_NZ },
  { "ldab", "()->b",	"movtst8",   3, 0xf6,  4,  4, CLR_V_CHG_NZ },
  { "stab", "b->()",	"movtst8",   3, 0xf7,  4,  4, CLR_V_CHG_NZ },
  { "eorb", "(),b->b",	"eor8",	     3, 0xf8,  4,  4, CLR_V_CHG_NZ },
  { "adcb", "(),b->b",	"eor8",	     3, 0xf9,  4,  4, CHG_HNZVC },
  { "orab", "(),b->b",	"or8",	     3, 0xfa,  4,  4, CLR_V_CHG_NZ },
  { "addb", "(),b->b",	"add8",	     3, 0xfb,  4,  4, CHG_HNZVC },
  { "ldd",  "()->d",	"movtst16",  3, 0xfc,  5,  5, CLR_V_CHG_NZ },
  { "std",  "d->()",	"movtst16",  3, 0xfd,  5,  5, CLR_V_CHG_NZ },
  { "ldx",  "()->x",	"movtst16",  3, 0xfe,  5,  5, CLR_V_CHG_NZ },
  { "stx",  "x->()",	"movtst16",  3, 0xff,  5,  5, CLR_V_CHG_NZ }
};


/* Page 2 opcodes */
/*
 *  { "dex", "x->x", "dec16", 1, 0x00, 5, _M,  CHG_NONE },
 * Name -+					 +----- Insn CCR changes
 * Operands  ---+			  +------------ Max # cycles
 * Pattern   -----------+	       +--------------- Min # cycles
 * Size	     -----------------+	  +-------------------- Opcode
 */
struct m6811_opcode_def m6811_page2_opcodes[] = {
  { "iny",  "y->y",	"inc16",     2, 0x08, 4, 4, CHG_Z },
  { "dey",  "y->y",	"dec16",     2, 0x09, 4, 4, CHG_Z },
  { "bset", "(y),#->(y)","or8",	     4, 0x1c, 8, 8, CLR_V_CHG_NZ },
  { "bclr", "(y),#->(y)","bclr8",    4, 0x1d, 8, 8, CLR_V_CHG_NZ },
  { "brset","(y),#,r",	 "brset8",   5, 0x1e, 8, 8, CHG_NONE },
  { "brclr","(y),#,r",	"brclr8",    5, 0x1f, 8, 8, CHG_NONE },
  { "tsy",  "sp->y",	"tsxy16",    2, 0x30, 4, 4, CHG_NONE },
  { "tys",  "y->sp",	"txys16",    2, 0x35, 4, 4, CHG_NONE },
  { "puly", "(sp)->y",	"mov16",     2, 0x38, 6, 6, CHG_NONE },
  { "aby",  "b,y->y",	"abxy16",    2, 0x3a, 4, 4, CHG_NONE },
  { "pshy", "y->(sp)",	"mov16",     2, 0x3c, 5, 5, CHG_NONE },
  { "neg",  "(y)->(y)", "neg8",	     3, 0x60, 7, 7, CHG_NZVC },
  { "com",  "(y)->(y)", "com8",	     3, 0x63, 7, 7, SET_C_CLR_V_CHG_NZ},
  { "lsr",  "(y)->(y)", "lsr8",	     3, 0x64, 7, 7, CLR_V_CHG_ZVC },
  { "ror",  "(y)->(y)", "ror8",	     3, 0x66, 7, 7, CHG_NZVC },
  { "asr",  "(y)->(y)", "asr8",	     3, 0x67, 7, 7, CHG_NZVC },
  { "asl",  "(y)->(y)", "lsl8",	     3, 0x68, 7, 7, CHG_NZVC },
  { "rol",  "(y)->(y)", "rol8",	     3, 0x69, 7, 7, CHG_NZVC },
  { "dec",  "(y)->(y)", "dec8",	     3, 0x6a, 7, 7, CHG_NZV },
  { "inc",  "(y)->(y)", "inc8",	     3, 0x6c, 7, 7, CHG_NZV },
  { "tst",  "(y)",	"tst8",	     3, 0x6d, 7, 7, CLR_V_CHG_NZ },
  { "jmp",  "&(y)",	"bra",	     3, 0x6e, 4, 4, CHG_NONE },
  { "clr",  "->(y)",	"clr8",	     3, 0x6f, 7, 7, SET_Z_CLR_NVC },
  { "cmpy", "#,y",	"sub16",     4, 0x8c, 5, 5, CHG_NZVC },
  { "xgdy", "y->y",	"xgdxy16",   2, 0x8f, 4, 4, CHG_NONE },
  { "cmpy", "*,y",	"sub16",     3, 0x9c, 6, 6, CHG_NZVC },
  { "suba", "(y),a->a", "sub8",	     3, 0xa0, 5, 5, CHG_NZVC },
  { "cmpa", "(y),a",	"sub8",	     3, 0xa1, 5, 5, CHG_NZVC },
  { "sbca", "(y),a->a", "sbc8",	     3, 0xa2, 5, 5, CHG_NZVC },
  { "subd", "(y),d->d", "sub16",     3, 0xa3, 7, 7, CHG_NZVC },
  { "anda", "(y),a->a", "and8",	     3, 0xa4, 5, 5, CLR_V_CHG_NZ },
  { "bita", "(y),a",	"and8",	     3, 0xa5, 5, 5, CLR_V_CHG_NZ },
  { "ldaa", "(y)->a",	"movtst8",   3, 0xa6, 5, 5, CLR_V_CHG_NZ },
  { "staa", "a->(y)",	"movtst8",   3, 0xa7, 5, 5, CLR_V_CHG_NZ },
  { "eora", "(y),a->a", "eor8",	     3, 0xa8, 5, 5, CLR_V_CHG_NZ },
  { "adca", "(y),a->a", "adc8",	     3, 0xa9, 5, 5, CHG_HNZVC },
  { "oraa", "(y),a->a", "or8",	     3, 0xaa, 5, 5, CLR_V_CHG_NZ },
  { "adda", "(y),a->a", "add8",	     3, 0xab, 5, 5, CHG_HNZVC },
  { "cmpy", "(y),y",	"sub16",     3, 0xac, 7, 7, CHG_NZVC },
  { "jsr",  "&(y)",	"jsr_11_16", 3, 0xad, 6, 6, CHG_NONE },
  { "lds",  "(y)->sp",	"movtst16",  3, 0xae, 6, 6, CLR_V_CHG_NZ },
  { "sts",  "sp->(y)",	"movtst16",  3, 0xaf, 6, 6, CLR_V_CHG_NZ },
  { "cmpy", "(),y",	"sub16",     4, 0xbc, 7, 7, CHG_NZVC },
  { "ldy",  "#->y",	"movtst16",  4, 0xce, 4, 4, CLR_V_CHG_NZ },
  { "ldy",  "*->y",	"movtst16",  3, 0xde, 5, 5, CLR_V_CHG_NZ },
  { "sty",  "y->*",	"movtst16",  3, 0xdf, 5, 5, CLR_V_CHG_NZ },
  { "subb", "(y),b->b", "sub8",	     3, 0xe0, 5, 5, CHG_NZVC },
  { "cmpb", "(y),b",	"sub8",	     3, 0xe1, 5, 5, CHG_NZVC },
  { "sbcb", "(y),b->b", "sbc8",	     3, 0xe2, 5, 5, CHG_NZVC },
  { "addd", "(y),d->d", "add16",     3, 0xe3, 7, 7, CHG_NZVC },
  { "andb", "(y),b->b", "and8",	     3, 0xe4, 5, 5, CLR_V_CHG_NZ },
  { "bitb", "(y),b",	"and8",	     3, 0xe5, 5, 5, CLR_V_CHG_NZ },
  { "ldab", "(y)->b",	"movtst8",   3, 0xe6, 5, 5, CLR_V_CHG_NZ },
  { "stab", "b->(y)",	"movtst8",   3, 0xe7, 5, 5, CLR_V_CHG_NZ },
  { "eorb", "(y),b->b", "eor8",	     3, 0xe8, 5, 5, CLR_V_CHG_NZ },
  { "adcb", "(y),b->b", "adc8",	     3, 0xe9, 5, 5, CHG_HNZVC },
  { "orab", "(y),b->b", "or8",	     3, 0xea, 5, 5, CLR_V_CHG_NZ },
  { "addb", "(y),b->b", "add8",	     3, 0xeb, 5, 5, CHG_HNZVC },
  { "ldd",  "(y)->d",	"movtst16",  3, 0xec, 6, 6, CLR_V_CHG_NZ },
  { "std",  "d->(y)",	"movtst16",  3, 0xed, 6, 6, CLR_V_CHG_NZ },
  { "ldy",  "(y)->y",	"movtst16",  3, 0xee, 6, 6, CLR_V_CHG_NZ },
  { "sty",  "y->(y)",	"movtst16",  3, 0xef, 6, 6, CLR_V_CHG_NZ },
  { "ldy",  "()->y",	"movtst16",  4, 0xfe, 6, 6, CLR_V_CHG_NZ },
  { "sty",  "y->()",	"movtst16",  4, 0xff, 6, 6, CLR_V_CHG_NZ }
};

/* Page 3 opcodes */
/*
 *  { "dex", "x->x", "dec16", 1, 0x00, 5, _M,  CHG_NONE },
 * Name -+					 +----- Insn CCR changes
 * Operands  ---+			  +------------ Max # cycles
 * Pattern   -----------+	       +--------------- Min # cycles
 * Size	     -----------------+	  +-------------------- Opcode
 */
struct m6811_opcode_def m6811_page3_opcodes[] = {
  { "cmpd", "#,d",	"sub16",     4, 0x83, 5, 5, CHG_NZVC },
  { "cmpd", "*,d",	"sub16",     3, 0x93, 6, 6, CHG_NZVC },
  { "cmpd", "(x),d",	"sub16",     3, 0xa3, 7, 7, CHG_NZVC },
  { "cmpy", "(x),y",	"sub16",     3, 0xac, 7, 7, CHG_NZVC },
  { "cmpd", "(),d",	"sub16",     4, 0xb3, 7, 7, CHG_NZVC },
  { "ldy",  "(x)->y",	"movtst16",  3, 0xee, 6, 6, CLR_V_CHG_NZ },
  { "sty",  "y->(x)",	"movtst16",  3, 0xef, 6, 6, CLR_V_CHG_NZ }
};

/* Page 4 opcodes */
/*
 *  { "dex", "x->x", "dec16", 1, 0x00, 5, _M,  CHG_NONE },
 * Name -+					 +----- Insn CCR changes
 * Operands  ---+			  +------------ Max # cycles
 * Pattern   -----------+	       +--------------- Min # cycles
 * Size	     -----------------+	  +-------------------- Opcode
 */
struct m6811_opcode_def m6811_page4_opcodes[] = {
  { "syscall", "",	"syscall",   2, 0x03, 6, 6, CHG_NONE },
  { "cmpd", "(y),d",	"sub16",     3, 0xa3, 7, 7, CHG_NZVC },
  { "cmpx", "(y),x",	"sub16",     3, 0xac, 7, 7, CHG_NZVC },
  { "ldx",  "(y)->x",	"movtst16",  3, 0xee, 6, 6, CLR_V_CHG_NZ },
  { "stx",  "x->(y)",	"movtst16",  3, 0xef, 6, 6, CLR_V_CHG_NZ }
};

/* 68HC12 opcodes */
/*
 *  { "dex", "x->x", "dec16", 1, 0x00, 5, _M,  CHG_NONE },
 * Name -+					 +----- Insn CCR changes
 * Operands  ---+			  +------------ Max # cycles
 * Pattern   -----------+	       +--------------- Min # cycles
 * Size	     -----------------+	  +-------------------- Opcode
 */
struct m6811_opcode_def m6812_page1_opcodes[] = {
  { "adca", "#,a->a",    "adc8",     2, 0x89,  1,  1,  CHG_HNZVC },
  { "adca", "*,a->a",    "adc8",     2, 0x99,  3,  3,  CHG_HNZVC },
  { "adca", "(),a->a",   "adc8",     3, 0xb9,  3,  3,  CHG_HNZVC },
  { "adca", "[],a->a",   "adc8",     2, 0xa9,  3,  3,  CHG_HNZVC },

  { "adcb", "#,b->b",    "adc8",     2, 0xc9,  1,  1,  CHG_HNZVC },
  { "adcb", "*,b->b",    "adc8",     3, 0xd9,  3,  3,  CHG_HNZVC },
  { "adcb", "(),b->b",   "adc8",     3, 0xf9,  3,  3,  CHG_HNZVC },
  { "adcb", "[],b->b",   "adc8",     2, 0xe9,  3,  3,  CHG_HNZVC },

  { "adda", "#,a->a",    "add8",     2, 0x8b,  1,  1,  CHG_HNZVC },
  { "adda", "*,a->a",    "add8",     3, 0x9b,  3,  3,  CHG_HNZVC },
  { "adda", "(),a->a",   "add8",     3, 0xbb,  3,  3,  CHG_HNZVC },
  { "adda", "[],a->a",   "add8",     2, 0xab,  3,  3,  CHG_HNZVC },

  { "addb", "#,b->b",    "add8",     2, 0xcb,  1,  1,  CHG_HNZVC },
  { "addb", "*,b->b",    "add8",     3, 0xdb,  3,  3,  CHG_HNZVC },
  { "addb", "(),b->b",   "add8",     3, 0xfb,  3,  3,  CHG_HNZVC },
  { "addb", "[],b->b",   "add8",     2, 0xeb,  3,  3,  CHG_HNZVC },

  { "addd", "#,d->d",    "add16",    3, 0xc3,  2,  2,  CHG_NZVC },
  { "addd", "*,d->d",    "add16",    2, 0xd3,  3,  3,  CHG_NZVC },
  { "addd", "(),d->d",   "add16",    3, 0xf3,  3,  3,  CHG_NZVC },
  { "addd", "[],d->d",   "add16",    2, 0xe3,  3,  3,  CHG_NZVC },

  { "anda", "#,a->a",    "and8",     2, 0x84,  1,  1,  CLR_V_CHG_NZ },
  { "anda", "*,a->a",    "and8",     2, 0x94,  3,  3,  CLR_V_CHG_NZ },
  { "anda", "(),a->a",   "and8",     3, 0xb4,  3,  3,  CLR_V_CHG_NZ },
  { "anda", "[],a->a",   "and8",     2, 0xa4,  3,  3,  CLR_V_CHG_NZ },

  { "andb", "#,b->b",    "and8",     2, 0xc4,  1,  1,  CLR_V_CHG_NZ },
  { "andb", "*,b->b",    "and8",     2, 0xd4,  3,  3,  CLR_V_CHG_NZ },
  { "andb", "(),b->b",   "and8",     3, 0xf4,  3,  3,  CLR_V_CHG_NZ },
  { "andb", "[],b->b",   "and8",     2, 0xe4,  3,  3,  CLR_V_CHG_NZ },

  { "andcc", "#,ccr->ccr", "and8",   2, 0x10,  1,  1,  CHG_ALL },

  { "asl",  "()->()",    "lsl8",     3, 0x78,  4,  4,  CHG_NZVC },
  { "asl",  "[]->[]",    "lsl8",     2, 0x68,  3,  3,  CHG_NZVC },

  { "asla", "a->a",      "lsl8",     1, 0x48,  1,  1,  CHG_NZVC },
  { "aslb", "b->b",      "lsl8",     1, 0x58,  1,  1,  CHG_NZVC },
  { "asld", "d->d",      "lsl16",    1, 0x59,  1,  1,  CHG_NZVC },

  { "asr",  "()->()",    "asr8",     3, 0x77,  4,  4,  CHG_NZVC },
  { "asr",  "[]->[]",    "asr8",     2, 0x67,  3,  3,  CHG_NZVC },

  { "asra", "a->a",      "asr8",     1, 0x47,  1,  1,  CHG_NZVC },
  { "asrb", "b->b",      "asr8",     1, 0x57,  1,  1,  CHG_NZVC },

  { "bcc",  "r",         0,          2, 0x24,  1,  3,  CHG_NONE },

  { "bclr", "*,#->*",    "bclr8",    3, 0x4d,  4,  4,  CLR_V_CHG_NZ },
  { "bclr", "(),#->()",  "bclr8",    4, 0x1d,  4,  4,  CLR_V_CHG_NZ },
  { "bclr", "[],#->[]",  "bclr8",    3, 0x0d,  4,  4,  CLR_V_CHG_NZ },

  { "bcs",  "r",         0,          2, 0x25,  1,  3, CHG_NONE },
  { "beq",  "r",         0,          2, 0x27,  1,  3, CHG_NONE },
  { "bge",  "r",         0,          2, 0x2c,  1,  3, CHG_NONE },

  { "bgnd",  0,          0,          1, 0x00,  5,  5, CHG_NONE },

  { "bgt",  "r",         0,          2, 0x2e,  1,  3, CHG_NONE },
  { "bhi",  "r",         0,          2, 0x22,  1,  3, CHG_NONE },
  
  { "bita", "#,a",       "and8",     2, 0x85,  1,  1, CLR_V_CHG_NZ },
  { "bita", "*,a",       "and8",     2, 0x95,  3,  3, CLR_V_CHG_NZ },
  { "bita", "(),a",      "and8",     3, 0xb5,  3,  3, CLR_V_CHG_NZ },
  { "bita", "[],a",      "and8",     2, 0xa5,  3,  3,  CLR_V_CHG_NZ },

  { "bitb", "#,b",       "and8",     2, 0xc5,  1,  1, CLR_V_CHG_NZ },
  { "bitb", "*,b",       "and8",     2, 0xd5,  3,  3, CLR_V_CHG_NZ },
  { "bitb", "(),b",      "and8",     3, 0xf5,  3,  3, CLR_V_CHG_NZ },
  { "bitb", "[],b",      "and8",     2, 0xe5,  3,  3,  CLR_V_CHG_NZ },

  { "ble",  "r",          0,         2, 0x2f,  1,  3, CHG_NONE },
  { "bls",  "r",          0,         2, 0x23,  1,  3, CHG_NONE },
  { "blt",  "r",          0,         2, 0x2d,  1,  3, CHG_NONE },
  { "bmi",  "r",          0,         2, 0x2b,  1,  3, CHG_NONE },
  { "bne",  "r",          0,         2, 0x26,  1,  3, CHG_NONE },
  { "bpl",  "r",          0,         2, 0x2a,  1,  3, CHG_NONE },
  { "bra",  "r",          0,         2, 0x20,  1,  3, CHG_NONE },

  { "brclr", "*,#,r",     "brclr8",  4, 0x4f,  4,  4,  CHG_NONE },
  { "brclr", "(),#,r",    "brclr8",  5, 0x1f,  5,  5,  CHG_NONE },
  { "brclr", "[],#,r",    "brclr8",  4, 0x0f,  4,  4,  CHG_NONE },

  { "brn",  "r",          "nop",     2, 0x21,  1,  3,  CHG_NONE },

  { "brset", "*,#,r",     "brset8",  4, 0x4e,  4,  4,  CHG_NONE },
  { "brset", "(),#,r",    "brset8",  5, 0x1e,  5,  5,  CHG_NONE },
  { "brset", "[],#,r",    "brset8",  4, 0x0e,  4,  4,  CHG_NONE },

  { "bset",  "*,#->*",    "or8",     3, 0x4c,  4,  4,  CLR_V_CHG_NZ },
  { "bset",  "(),#->()",  "or8",     4, 0x1c,  4,  4,  CLR_V_CHG_NZ },
  { "bset",  "[],#->[]",  "or8",     3, 0x0c,  4,  4,  CLR_V_CHG_NZ },

  { "bsr",   "r",         "jsr_12_16", 2, 0x07,  4,  4, CHG_NONE },

  { "bvc",   "r",         0,         2, 0x28,  1,  3, CHG_NONE },
  { "bvs",   "r",         0,         2, 0x29,  1,  3, CHG_NONE },

  { "call",  "",          "call8",   4, 0x4a,  8,  8,  CHG_NONE },
  { "call",  "",          "call_ind",2, 0x4b,  8,  8,  CHG_NONE },

  { "clr",   "->()",      "clr8",    3, 0x79,  3,  3,  SET_Z_CLR_NVC },
  { "clr",   "->[]",      "clr8",    2, 0x69,  2,  2,  SET_Z_CLR_NVC },

  { "clra",  "->a",       "clr8",    1, 0x87,  1,  1,  SET_Z_CLR_NVC },
  { "clrb",  "->b",       "clr8",    1, 0xc7,  1,  1,  SET_Z_CLR_NVC },

  { "cpa",  "#,a",        "sub8",    2, 0x81,  1,  1,  CHG_NZVC },
  { "cpa",  "*,a",        "sub8",    2, 0x91,  3,  3,  CHG_NZVC },
  { "cpa",  "(),a",       "sub8",    3, 0xb1,  3,  3,  CHG_NZVC },
  { "cpa",  "[],a",       "sub8",    2, 0xa1,  3,  3,  CHG_NZVC },

  { "cpb",  "#,b",        "sub8",    2, 0xc1,  1,  1,  CHG_NZVC },
  { "cpb",  "*,b",        "sub8",    2, 0xd1,  3,  3,  CHG_NZVC },
  { "cpb",  "(),b",       "sub8",    3, 0xf1,  3,  3,  CHG_NZVC },
  { "cpb",  "[],b",       "sub8",    2, 0xe1,  3,  3,  CHG_NZVC },

  { "com",   "()->()",    "com8",    3, 0x71,  4,  4,  SET_C_CLR_V_CHG_NZ },
  { "com",   "[]->[]",    "com8",    2, 0x61,  3,  3,  SET_C_CLR_V_CHG_NZ },

  { "coma",  "a->a",      "com8",    1, 0x41,  1,  1,  SET_C_CLR_V_CHG_NZ },
  { "comb",  "b->b",      "com8",    1, 0x51,  1,  1,  SET_C_CLR_V_CHG_NZ },

  { "cpd",   "#,d",       "sub16",   3, 0x8c,  2,  2,  CHG_NZVC },
  { "cpd",   "*,d",       "sub16",   2, 0x9c,  3,  3,  CHG_NZVC },
  { "cpd",   "(),d",      "sub16",   3, 0xbc,  3,  3,  CHG_NZVC },
  { "cpd",   "[],d",      "sub16",   2, 0xac,  3,  3,  CHG_NZVC },

  { "cps",   "#,sp",      "sub16",   3, 0x8f,  2,  2,  CHG_NZVC },
  { "cps",   "*,sp",      "sub16",   2, 0x9f,  3,  3,  CHG_NZVC },
  { "cps",   "(),sp",     "sub16",   3, 0xbf,  3,  3,  CHG_NZVC },
  { "cps",   "[],sp",     "sub16",   2, 0xaf,  3,  3,  CHG_NZVC },

  { "cpx",   "#,x",       "sub16",   3, 0x8e,  2,  2,  CHG_NZVC },
  { "cpx",   "*,x",       "sub16",   2, 0x9e,  3,  3,  CHG_NZVC },
  { "cpx",   "(),x",      "sub16",   3, 0xbe,  3,  3,  CHG_NZVC },
  { "cpx",   "[],x",      "sub16",   2, 0xae,  3,  3,  CHG_NZVC },

  { "cpy",   "#,y",       "sub16",   3, 0x8d,  2,  2,  CHG_NZVC },
  { "cpy",   "*,y",       "sub16",   2, 0x9d,  3,  3,  CHG_NZVC },
  { "cpy",   "(),y",      "sub16",   3, 0xbd,  3,  3,  CHG_NZVC },
  { "cpy",   "[],y",      "sub16",   2, 0xad,  3,  3,  CHG_NZVC },

  /* dbeq, dbne, ibeq, ibne, tbeq, tbne */
  { "dbeq",   0,          "dbcc8",   3, 0x04,  3,  3, CHG_NONE },

  { "dec",   "()->()",    "dec8",    3, 0x73,  4,  4,  CHG_NZV },
  { "dec",   "[]->[]",    "dec8",    2, 0x63,  3,  3,  CHG_NZV },

  { "deca",  "a->a",      "dec8",    1, 0x43,  1,  1,  CHG_NZV },
  { "decb",  "b->b",      "dec8",    1, 0x53,  1,  1,  CHG_NZV },

  { "dex",   "x->x",      "dec16",   1, 0x09,  1,  1,  CHG_Z },
  { "dey",   "y->y",      "dec16",   1, 0x03,  1,  1,  CHG_Z },

  { "ediv",  0,           0,         1, 0x11,  11,  11,  CHG_NZVC },
  { "emul",  0,           0,         1, 0x13,  3,  3,  CHG_NZC },

  { "eora",  "#,a->a",    "eor8",    2, 0x88,  1,  1,  CLR_V_CHG_NZ },
  { "eora",  "*,a->a",    "eor8",    2, 0x98,  3,  3,  CLR_V_CHG_NZ },
  { "eora",  "(),a->a",   "eor8",    3, 0xb8,  3,  3,  CLR_V_CHG_NZ },
  { "eora",  "[],a->a",   "eor8",    2, 0xa8,  3,  3,  CLR_V_CHG_NZ },

  { "eorb",  "#,b->b",    "eor8",    2, 0xc8,  1,  1,  CLR_V_CHG_NZ },
  { "eorb",  "*,b->b",    "eor8",    2, 0xd8,  3,  3,  CLR_V_CHG_NZ },
  { "eorb",  "(),b->b",   "eor8",    3, 0xf8,  3,  3,  CLR_V_CHG_NZ },
  { "eorb",  "[],b->b",   "eor8",    2, 0xe8,  3,  3,  CLR_V_CHG_NZ },

  /* exg, sex, tfr */
  { "exg",   "#",         "exg8",    2, 0xb7,  1,  1,  CHG_NONE },

  { "inc",   "()->()",    "inc8",    3, 0x72,  4,  4,  CHG_NZV },
  { "inc",   "[]->[]",    "inc8",    2, 0x62,  3,  3,  CHG_NZV },

  { "inca",  "a->a",      "inc8",    1, 0x42,  1,  1,  CHG_NZV },
  { "incb",  "b->b",      "inc8",    1, 0x52,  1,  1,  CHG_NZV },

  { "inx",   "x->x",      "inc16",   1, 0x08,  1,  1,  CHG_Z },
  { "iny",   "y->y",      "inc16",   1, 0x02,  1,  1,  CHG_Z },

  { "jmp",   "&()",       "bra",     3, 0x06,  3,  3,  CHG_NONE },
  { "jmp",   "&[]",       "bra",     2, 0x05,  3,  3,  CHG_NONE },

  { "jsr",   "*",         "jsr_12_16",   2, 0x17,  4,  4,  CHG_NONE },
  { "jsr",   "&()",       "jsr_12_16",   3, 0x16,  4,  4,  CHG_NONE },
  { "jsr",   "&[]",       "jsr_12_16",   2, 0x15,  4,  4,  CHG_NONE },

  { "ldaa", "#->a",       "movtst8", 2, 0x86,  1,  1,  CLR_V_CHG_NZ },
  { "ldaa", "*->a",       "movtst8", 2, 0x96,  3,  3,  CLR_V_CHG_NZ },
  { "ldaa", "()->a",      "movtst8", 3, 0xb6,  3,  3,  CLR_V_CHG_NZ },
  { "ldaa", "[]->a",      "movtst8", 2, 0xa6,  3,  3,  CLR_V_CHG_NZ },

  { "ldab", "#->b",       "movtst8", 2, 0xc6,  1,  1,  CLR_V_CHG_NZ },
  { "ldab", "*->b",       "movtst8", 2, 0xd6,  3,  3,  CLR_V_CHG_NZ },
  { "ldab", "()->b",      "movtst8", 3, 0xf6,  3,  3,  CLR_V_CHG_NZ },
  { "ldab", "[]->b",      "movtst8", 2, 0xe6,  3,  3,  CLR_V_CHG_NZ },

  { "ldd",  "#->d",       "movtst16", 3, 0xcc,  2,  2,  CLR_V_CHG_NZ },
  { "ldd",  "*->d",       "movtst16", 2, 0xdc,  3,  3,  CLR_V_CHG_NZ },
  { "ldd",  "()->d",      "movtst16", 3, 0xfc,  3,  3,  CLR_V_CHG_NZ },
  { "ldd",  "[]->d",      "movtst16", 2, 0xec,  3,  3,  CLR_V_CHG_NZ },

  { "lds",  "#->sp",      "movtst16", 3, 0xcf,  2,  2,  CLR_V_CHG_NZ },
  { "lds",  "*->sp",      "movtst16", 2, 0xdf,  3,  3,  CLR_V_CHG_NZ },
  { "lds",  "()->sp",     "movtst16", 3, 0xff,  3,  3,  CLR_V_CHG_NZ },
  { "lds",  "[]->sp",     "movtst16", 2, 0xef,  3,  3,  CLR_V_CHG_NZ },

  { "ldx",  "#->x",       "movtst16", 3, 0xce,  2,  2,  CLR_V_CHG_NZ },
  { "ldx",  "*->x",       "movtst16", 2, 0xde,  3,  3,  CLR_V_CHG_NZ },
  { "ldx",  "()->x",      "movtst16", 3, 0xfe,  3,  3,  CLR_V_CHG_NZ },
  { "ldx",  "[]->x",      "movtst16", 2, 0xee,  3,  3,  CLR_V_CHG_NZ },

  { "ldy",  "#->y",       "movtst16", 3, 0xcd,  2,  2,  CLR_V_CHG_NZ },
  { "ldy",  "*->y",       "movtst16", 2, 0xdd,  3,  3,  CLR_V_CHG_NZ },
  { "ldy",  "()->y",      "movtst16", 3, 0xfd,  3,  3,  CLR_V_CHG_NZ },
  { "ldy",  "[]->y",      "movtst16", 2, 0xed,  3,  3,  CLR_V_CHG_NZ },

  { "leas", "&[]->sp",    "lea16",   2, 0x1b,  2,  2,  CHG_NONE },
  { "leax", "&[]->x",     "lea16",   2, 0x1a,  2,  2,  CHG_NONE },
  { "leay", "&[]->y",     "lea16",   2, 0x19,  2,  2,  CHG_NONE },

  { "lsr",  "()->()",     "lsr8",    3, 0x74,  4,  4,  CLR_N_CHG_ZVC },
  { "lsr",  "[]->[]",     "lsr8",    2, 0x64,  3,  3,  CLR_N_CHG_ZVC },

  { "lsra", "a->a",       "lsr8",    1, 0x44,  1,  1,  CLR_N_CHG_ZVC },
  { "lsrb", "b->b",       "lsr8",    1, 0x54,  1,  1,  CLR_N_CHG_ZVC },
  { "lsrd", "d->d",       "lsr16",   1, 0x49,  1,  1,  CLR_N_CHG_ZVC },

  { "mem",  0,            0,         1, 0x01,  5,  5,  CHG_HNZVC },

  { "mul",  "b,a->d",     "mul16",   1, 0x12,  3,  3,  CHG_C },

  { "neg",  "()->()",     "neg8",    3, 0x70,  4,  4,  CHG_NZVC },
  { "neg",  "[]->[]",     "neg8",    2, 0x60,  3,  3,  CHG_NZVC },

  { "nega", "a->a",       "neg8",    1, 0x40,  1,  1,  CHG_NZVC },
  { "negb", "b->b",       "neg8",    1, 0x50,  1,  1,  CHG_NZVC },

  { "nop",  "",           "nop",     1, 0xa7,  1,  1,  CHG_NONE },

  { "oraa", "#,a->a",     "or8",     2, 0x8a,  1,  1,  CLR_V_CHG_NZ },
  { "oraa", "*,a->a",     "or8",     2, 0x9a,  3,  3,  CLR_V_CHG_NZ },
  { "oraa", "(),a->a",    "or8",     3, 0xba,  3,  3,  CLR_V_CHG_NZ },
  { "oraa", "[],a->a",    "or8",     2, 0xaa,  3,  3,  CLR_V_CHG_NZ },

  { "orab", "#,b->b",     "or8",     2, 0xca,  1,  1,  CLR_V_CHG_NZ },
  { "orab", "*,b->b",     "or8",     2, 0xda,  3,  3,  CLR_V_CHG_NZ },
  { "orab", "(),b->b",    "or8",     3, 0xfa,  3,  3,  CLR_V_CHG_NZ },
  { "orab", "[],b->b",    "or8",     2, 0xea,  3,  3,  CLR_V_CHG_NZ },

  { "orcc", "#,ccr->ccr", "or8",     2, 0x14,  1,  1,  CHG_ALL },

  { "page2", 0,		  "page2",   1, 0x18,  0,  0,  CHG_NONE },

  { "psha", "a->(sp)",    "mov8",    1, 0x36,  2,  2,  CHG_NONE },
  { "pshb", "b->(sp)",    "mov8",    1, 0x37,  2,  2,  CHG_NONE },
  { "pshc", "ccr->(sp)",  "mov8",    1, 0x39,  2,  2,  CHG_NONE },
  { "pshd", "d->(sp)",    "mov16",   1, 0x3b,  2,  2,  CHG_NONE },
  { "pshx", "x->(sp)",    "mov16",   1, 0x34,  2,  2,  CHG_NONE },
  { "pshy", "y->(sp)",    "mov16",   1, 0x35,  2,  2,  CHG_NONE },

  { "pula", "(sp)->a",    "mov8",    1, 0x32,  3,  3,  CHG_NONE },
  { "pulb", "(sp)->b",    "mov8",    1, 0x33,  3,  3,  CHG_NONE },
  { "pulc", "(sp)->ccr",  "mov8",    1, 0x38,  3,  3,  CHG_ALL },
  { "puld", "(sp)->d",    "mov16",   1, 0x3a,  3,  3,  CHG_NONE },
  { "pulx", "(sp)->x",    "mov16",   1, 0x30,  3,  3,  CHG_NONE },
  { "puly", "(sp)->y",    "mov16",   1, 0x31,  3,  3,  CHG_NONE },

  { "rol",  "()->()",     "rol8",    3, 0x75,  4,  4,  CHG_NZVC },
  { "rol",  "[]->[]",     "rol8",    2, 0x65,  3,  3,  CHG_NZVC },

  { "rola", "a->a",       "rol8",    1, 0x45,  1,  1,  CHG_NZVC },
  { "rolb", "b->b",       "rol8",    1, 0x55,  1,  1,  CHG_NZVC },

  { "ror",  "()->()",     "ror8",    3, 0x76,  4,  4,  CHG_NZVC },
  { "ror",  "[]->[]",     "ror8",    2, 0x66,  3,  3,  CHG_NZVC },

  { "rora", "a->a",       "ror8",    1, 0x46,  1,  1,  CHG_NZVC },
  { "rorb", "b->b",       "ror8",    1, 0x56,  1,  1,  CHG_NZVC },

  { "rtc",  0,            0,         1, 0x0a,  6,  6,  CHG_NONE },
  { "rti",  0,            "rti12",   1, 0x0b,  8, 10,  CHG_ALL},
  { "rts",  0,            "rts12",   1, 0x3d,  5,  5,  CHG_NONE },

  { "sbca", "#,a->a",     "sbc8",    2, 0x82,  1,  1,  CHG_NZVC },
  { "sbca", "*,a->a",     "sbc8",    2, 0x92,  3,  3,  CHG_NZVC },
  { "sbca", "(),a->a",    "sbc8",    3, 0xb2,  3,  3,  CHG_NZVC },
  { "sbca", "[],a->a",    "sbc8",    2, 0xa2,  3,  3,  CHG_NZVC },

  { "sbcb", "#,b->b",     "sbc8",    2, 0xc2,  1,  1,  CHG_NZVC },
  { "sbcb", "*,b->b",     "sbc8",    2, 0xd2,  3,  3,  CHG_NZVC },
  { "sbcb", "(),b->b",    "sbc8",    3, 0xf2,  3,  3,  CHG_NZVC },
  { "sbcb", "[],b->b",    "sbc8",    2, 0xe2,  3,  3,  CHG_NZVC },

  { "staa", "a->*",       "movtst8", 2, 0x5a,  2,  2,  CLR_V_CHG_NZ },
  { "staa", "a->()",      "movtst8", 3, 0x7a,  3,  3,  CLR_V_CHG_NZ },
  { "staa", "a->[]",      "movtst8", 2, 0x6a,  2,  2,  CLR_V_CHG_NZ },

  { "stab", "b->*",       "movtst8", 2, 0x5b,  2,  2,  CLR_V_CHG_NZ },
  { "stab", "b->()",      "movtst8", 3, 0x7b,  3,  3,  CLR_V_CHG_NZ },
  { "stab", "b->[]",      "movtst8", 2, 0x6b,  2,  2,  CLR_V_CHG_NZ },

  { "std",  "d->*",       "movtst16", 2, 0x5c,  2,  2,  CLR_V_CHG_NZ },
  { "std",  "d->()",      "movtst16", 3, 0x7c,  3,  3,  CLR_V_CHG_NZ },
  { "std",  "d->[]",      "movtst16", 2, 0x6c,  2,  2,  CLR_V_CHG_NZ },

  { "sts",  "sp->*",      "movtst16", 2, 0x5f,  2,  2,  CLR_V_CHG_NZ },
  { "sts",  "sp->()",     "movtst16", 3, 0x7f,  3,  3,  CLR_V_CHG_NZ },
  { "sts",  "sp->[]",     "movtst16", 2, 0x6f,  2,  2,  CLR_V_CHG_NZ },

  { "stx",  "x->*",       "movtst16", 2, 0x5e,  2,  2,  CLR_V_CHG_NZ },
  { "stx",  "x->()",      "movtst16", 3, 0x7e,  3,  3,  CLR_V_CHG_NZ },
  { "stx",  "x->[]",      "movtst16", 2, 0x6e,  2,  2,  CLR_V_CHG_NZ },

  { "sty",  "y->*",       "movtst16", 2, 0x5d,  2,  2,  CLR_V_CHG_NZ },
  { "sty",  "y->()",      "movtst16", 3, 0x7d,  3,  3,  CLR_V_CHG_NZ },
  { "sty",  "y->[]",      "movtst16", 2, 0x6d,  2,  2,  CLR_V_CHG_NZ },

  { "suba", "#,a->a",     "sub8",     2, 0x80,  1,  1,  CHG_NZVC },
  { "suba", "*,a->a",     "sub8",     2, 0x90,  3,  3,  CHG_NZVC },
  { "suba", "(),a->a",    "sub8",     3, 0xb0,  3,  3,  CHG_NZVC },
  { "suba", "[],a->a",    "sub8",     2, 0xa0,  3,  3,  CHG_NZVC },

  { "subb", "#,b->b",     "sub8",     2, 0xc0,  1,  1,  CHG_NZVC },
  { "subb", "*,b->b",     "sub8",     2, 0xd0,  3,  3,  CHG_NZVC },
  { "subb", "(),b->b",    "sub8",     3, 0xf0,  3,  3,  CHG_NZVC },
  { "subb", "[],b->b",    "sub8",     2, 0xe0,  3,  3,  CHG_NZVC },

  { "subd", "#,d->d",     "sub16",    3, 0x83,  2,  2,  CHG_NZVC },
  { "subd", "*,d->d",     "sub16",    2, 0x93,  3,  3,  CHG_NZVC },
  { "subd", "(),d->d",    "sub16",    3, 0xb3,  3,  3,  CHG_NZVC },
  { "subd", "[],d->d",    "sub16",    2, 0xa3,  3,  3,  CHG_NZVC },

  { "swi",  0,            0,          1, 0x3f,  9,  9,  CHG_NONE },

  { "tst",  "()",         "tst8",     3, 0xf7,  3,  3,  CLR_VC_CHG_NZ },
  { "tst",  "[]",         "tst8",     2, 0xe7,  3,  3,  CLR_VC_CHG_NZ },

  { "tsta", "a",          "tst8",     1, 0x97,  1,  1,  CLR_VC_CHG_NZ },
  { "tstb", "b",          "tst8",     1, 0xd7,  1,  1,  CLR_VC_CHG_NZ },

  { "wai",  0,            0,          1, 0x3e,  8,  _M, CHG_NONE }
};

struct m6811_opcode_def m6812_page2_opcodes[] = {
  { "cba",  "b,a",        "sub8",     2, 0x17,  2,  2,  CHG_NZVC },

  /* After 'daa', the Z flag is undefined. Mark it as changed.  */
  { "daa",  0,            "daa8",     2, 0x07,  3,  3,  CHG_NZVC },

  { "edivs", 0,           0,          2, 0x14,  12,  12,  CHG_NZVC },
  { "emacs", 0,           0,          2, 0x12,  13,  13,  CHG_NZVC },

  { "emaxd", "[],d->d",   "max16",    3, 0x1a,  4,  4,  CHG_NZVC },
  { "emaxm", "[],d->[]",  "max16",    3, 0x1e,  4,  4,  CHG_NZVC },
  { "emind", "[],d->d",   "min16",    3, 0x1b,  4,  4,  CHG_NZVC },
  { "eminm", "[],d->[]",  "min16",    3, 0x1f,  4,  4,  CHG_NZVC },

  { "emuls", 0,           0,          2, 0x13,  3,  3,  CHG_NZC },
  { "etbl",  "[]",        "tbl16",    3, 0x3f, 10, 10,  CHG_NZC },
  { "fdiv",  "x,d->x",    "fdiv16",   2, 0x11, 12, 12,  CHG_ZVC },
  { "idiv",  "x,d->x",    "idiv16",   2, 0x10, 12, 12,  CLR_V_CHG_ZC },
  { "idivs", 0,           0,          2, 0x15, 12, 12,  CHG_NZVC },

  { "lbcc",  "R",         "bcc",      4, 0x24,  3,  4,  CHG_NONE },
  { "lbcs",  "R",         "bcs",      4, 0x25,  3,  4,  CHG_NONE },
  { "lbeq",  "R",         "beq",      4, 0x27,  3,  4,  CHG_NONE },
  { "lbge",  "R",         "bge",      4, 0x2c,  3,  4,  CHG_NONE },
  { "lbgt",  "R",         "bgt",      4, 0x2e,  3,  4,  CHG_NONE },
  { "lbhi",  "R",         "bhi",      4, 0x22,  3,  4,  CHG_NONE },
  { "lble",  "R",         "ble",      4, 0x2f,  3,  4,  CHG_NONE },
  { "lbls",  "R",         "bls",      4, 0x23,  3,  4,  CHG_NONE },
  { "lblt",  "R",         "blt",      4, 0x2d,  3,  4,  CHG_NONE },
  { "lbmi",  "R",         "bmi",      4, 0x2b,  3,  4,  CHG_NONE },
  { "lbne",  "R",         "bne",      4, 0x26,  3,  4,  CHG_NONE },
  { "lbpl",  "R",         "bpl",      4, 0x2a,  3,  4,  CHG_NONE },
  { "lbra",  "R",         "bra",      4, 0x20,  4,  4,  CHG_NONE },
  { "lbrn",  "R",         "nop",      4, 0x21,  3,  3,  CHG_NONE },
  { "lbvc",  "R",         "bvc",      4, 0x28,  3,  4,  CHG_NONE },
  { "lbvs",  "R",         "bvs",      4, 0x29,  3,  4,  CHG_NONE },

  { "maxa",  "[],a->a",   "max8",     3, 0x18,  4,  4,  CHG_NZVC },
  { "maxm",  "[],a->[]",  "max8",     3, 0x1c,  4,  4,  CHG_NZVC },
  { "mina",  "[],a->a",   "min8",     3, 0x19,  4,  4,  CHG_NZVC },
  { "minm",  "[],a->[]",  "min8",     3, 0x1d,  4,  4,  CHG_NZVC },

  { "movb",  0,           "move8",    5, 0x0b,  4,  4,  CHG_NONE },
  { "movb",  0,           "move8",    4, 0x08,  4,  4,  CHG_NONE },
  { "movb",  0,           "move8",    6, 0x0c,  6,  6,  CHG_NONE },
  { "movb",  0,           "move8",    5, 0x09,  5,  5,  CHG_NONE },
  { "movb",  0,           "move8",    5, 0x0d,  5,  5,  CHG_NONE },
  { "movb",  0,           "move8",    4, 0x0a,  5,  5,  CHG_NONE },

  { "movw",  0,           "move16",   6, 0x03,  5,  5,  CHG_NONE },
  { "movw",  0,           "move16",   5, 0x00,  4,  4,  CHG_NONE },
  { "movw",  0,           "move16",   6, 0x04,  6,  6,  CHG_NONE },
  { "movw",  0,           "move16",   5, 0x01,  5,  5,  CHG_NONE },
  { "movw",  0,           "move16",   5, 0x05,  5,  5,  CHG_NONE },
  { "movw",  0,           "move16",   4, 0x02,  5,  5,  CHG_NONE },

  { "rev",  0,            0,          2, 0x3a,  _M, _M, CHG_HNZVC },
  { "revw", 0,            0,          2, 0x3b,  _M, _M, CHG_HNZVC },
  { "sba",  "b,a->a",     "sub8",     2, 0x16,  2,  2,  CHG_NZVC },

  { "stop", 0,            0,          2, 0x3e,  2,  9,  CHG_NONE },

  { "tab",  "a->b",       "movtst8",  2, 0x0e,  2,  2,  CLR_V_CHG_NZ },
  { "tba",  "b->a",       "movtst8",  2, 0x0f,  2,  2,  CLR_V_CHG_NZ },

  { "wav",  0,            0,          2, 0x3c,  8,  _M, SET_Z_CHG_HNVC }
};

void fatal_error (const struct m6811_opcode_def*, const char*, ...);
void print (FILE*, int, const char*,...);
int gen_fetch_operands (FILE*, int, const struct m6811_opcode_def*,
			const char*);
void gen_save_result (FILE*, int, const struct m6811_opcode_def*,
		      int, const char*);
const struct m6811_opcode_pattern*
find_opcode_pattern (const struct m6811_opcode_def*);
void gen_interp (FILE*, int, const struct m6811_opcode_def*);
void gen_interpreter_for_table (FILE*, int,
				const struct m6811_opcode_def*,
				int, const char*);
void gen_interpreter (FILE*);


static int indent_level = 2;
static int current_insn_size = 0;

/* Fatal error message and exit.  This method is called when an inconsistency
   is detected in the generation table.	 */
void
fatal_error (const struct m6811_opcode_def *opcode, const char *msg, ...)
{
  va_list argp;

  fprintf (stderr, "Fatal error: ");
  va_start (argp, msg);
  vfprintf (stderr,  msg, argp);
  va_end (argp);
  fprintf (stderr, "\n");
  if (opcode)
    {
      fprintf (stderr, "Opcode: 0x%02x %s %s\n",
	       opcode->insn_code,
	       opcode->name ? opcode->name : "(null)",
	       opcode->operands ? opcode->operands : "(null)");
    }
  exit (1);
}


/* Format and pretty print for the code generation.  (printf like format).  */
void
print (FILE *fp, int col, const char *msg, ...)
{
  va_list argp;
  char buf[1024];
  int cur_col = -1;
  int i;

  /* Format in a buffer.  */
  va_start (argp, msg);
  vsprintf (buf, msg, argp);
  va_end (argp);

  /* Basic pretty print:
     - Every line is indented at column 'col',
     - Indentation is updated when '{' and '}' are found,
     - Indentation is incremented by the special character '@' (not displayed).
     - New lines inserted automatically after ';'  */
  for (i = 0; buf[i]; i++)
    {
      if (buf[i] == '{')
	col += indent_level;
      else if (buf[i] == '}')
	col -= indent_level;
      else if (buf[i] == '@')
	{
	  col += indent_level;
	  continue;
	}
      if (cur_col == -1 && buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\n')
	{
	  cur_col = 0;
	  while (cur_col < col)
	    {
	      fputc (' ', fp);
	      cur_col++;
	    }
	}
      if (buf[i] == '}')
	col -= indent_level;
      else if (buf[i] == '{')
	col += indent_level;
      else if (buf[i] == '\n')
	cur_col = -1;

      if (cur_col != -1 || buf[i] == '\n')
	fputc (buf[i], fp);

      if (buf[i] == ';')
	{
	  fputc ('\n', fp);
	  cur_col = -1;
	}
    }
}


/* Generate the code to obtain the operands before execution of the
   instruction.	 Operands are copied in local variables.  This allows to
   have the same instruction pattern and different operand formats.
   There is a maximum of 3 variables:
 
		       8-bits	       16-bits
   1st operand:		src8		src16
   2nd operand:		dst8		dst16
   alt operand:		addr		addr
  
   The operand string is interpreted as follows:
 
   a	Copy A register in the local 8-bits variable.
   b	"    B "
   ccr	"    ccr "
   d	"    D "	"      "    16-bits variable.
   x	"    X "
   y	"    Y "
   sp	"    SP "
   pc   "    PC "
   *	68HC11 page0 memory pointer.
	Get 8-bits page0 offset from program, set up 'addr' local
	variable to refer to the location in page0.
	Copy the 8/16-bits value pointed to by 'addr' in a 8/16-bits variable.
   (x)	68HC11 indirect access with X register.
	Get 8-bits unsigned offset from program, set up 'addr' = X + offset.
	Copy the 8/16-bits value pointed to by 'addr' in a 8/16-bits variable.
   (y)	Same as (x) with Y register.
   ()	68HC11 extended address mode (global variable).
	Get 16-bits address from program and set 'addr'.
	Copy the 8/16-bits value pointed to by 'addr' in a 8/16-bits variable.
   []   68HC12 indexed addressing mode
   (sp) Pop
	Pop a 8/16-bits value from stack and set in a 8/16-bits variable.
   r	Relative branch
	Get 8-bits relative branch, compute absolute address and set 'addr'
   #	68HC11 immediate value
	Get a 8/16-bits value from program and set a 8/16-bits variable.
   &(x)
   &(y)
   &()	Similar to (x), (y) and () except that we don't read the
	value pointed to by 'addr' (ie, only 'addr' is setup). Used by jmp/jsr.
   &[]  Similar to [] but don't read the value pointed to by the address.
   ,	Operand separator.
   -	End of input operands.
  
   Example:
       (x),a->a	      addr = x + (uint16) (fetch8 (proc));
		      src8 = a
       *,#,r	      addr = (uint16) (fetch8 (proc))  <- Temporary 'addr'
		      src8 = read_mem8 (proc, addr)
		      dst8 = fetch8 (proc)
		      addr = fetch_relbranch (proc)    <- Final 'addr'
  
   Returns 1 if the 'addr' operand is set, 0 otherwise.	 */
int
gen_fetch_operands (FILE *fp, int col,
		    const struct m6811_opcode_def *opcode,
		    const char *operand_size)
{
  static char *vars[2] = {
    "src",
    "dst"
  };
  char c;
  int addr_set = 0;
  int cur_var = 0;
  const char *operands = opcode->operands;
  
  if (operands == 0)
    operands = "";

  while ((c = *operands++) != 0)
    {
      switch (c)
	{
	case 'a':
	  if (cur_var >= 2)
	    fatal_error (opcode, "Too many locals");
	  
	  print (fp, col, "%s8 = cpu_get_a (proc);", vars[cur_var]);
	  break;

	case 'b':
	  if (cur_var >= 2)
	    fatal_error (opcode, "Too many locals");

	  print (fp, col, "%s8 = cpu_get_b (proc);", vars[cur_var]);
	  break;

	case 'd':
	  if (cur_var >= 2)
	    fatal_error (opcode, "Too many locals");

	  print (fp, col, "%s16 = cpu_get_d (proc);", vars[cur_var]);
	  break;

	case 'x':
	  if (cur_var >= 2)
	    fatal_error (opcode, "Too many locals");

	  print (fp, col, "%s16 = cpu_get_x (proc);", vars[cur_var]);
	  break;

	case 'y':
	  if (cur_var >= 2)
	    fatal_error (opcode, "Too many locals");

	  print (fp, col, "%s16 = cpu_get_y (proc);", vars[cur_var]);
	  break;

	case '*':
	  if (cur_var >= 2)
	    fatal_error (opcode, "Too many locals");

	  if (addr_set)
	    fatal_error (opcode, "Wrong use of '*', 'addr' already used");
	  
	  addr_set = 1;
	  current_insn_size += 1;
	  print (fp, col, "addr = (uint16) cpu_fetch8 (proc);");
	  print (fp, col, "%s%s = memory_read%s (proc, addr);",
		 vars[cur_var], operand_size, operand_size);
	  break;

	case '&':
	  if (addr_set)
	    fatal_error (opcode, "Wrong use of '&', 'addr' already used");
	  
	  addr_set = 1;
	  if (strncmp (operands, "(x)", 3) == 0)
	    {
	      current_insn_size += 1;
	      print (fp, col, "addr = cpu_get_x (proc) + (uint16) cpu_fetch8 (proc);");
	      operands += 3;
	    }
	  else if (strncmp (operands, "(y)", 3) == 0)
	    {
	      current_insn_size += 1;
	      print (fp, col, "addr = cpu_get_y (proc) + (uint16) cpu_fetch8 (proc);");
	      operands += 3;
	    }
	  else if (strncmp (operands, "()", 2) == 0)
	    {
	      current_insn_size += 2;
	      print (fp, col, "addr = cpu_fetch16 (proc);");
	      operands += 2;
	    }
	  else if (strncmp (operands, "[]", 2) == 0)
	    {
	      current_insn_size += 1;
	      print (fp, col, "addr = cpu_get_indexed_operand_addr (proc, 0);");
	      operands += 2;
	    }
	  else
	    {
	      fatal_error (opcode, "Unknown operand");
	    }
	  break;
	  
	case '(':
	  if (cur_var >= 2)
	    fatal_error (opcode, "Too many locals");
	  
	  if (addr_set)
	    fatal_error (opcode, "Wrong use of '(', 'addr' already used");
	  
	  if (strncmp (operands, "x)", 2) == 0)
	    {
	      addr_set = 1;
	      current_insn_size += 1;
	      print (fp, col, "addr = cpu_get_x (proc) + (uint16) cpu_fetch8 (proc);");
	      print (fp, col, "%s%s = memory_read%s (proc, addr);",
		     vars[cur_var], operand_size, operand_size);
	      operands += 2;
	    }
	  else if (strncmp (operands, "y)", 2) == 0)
	    {
	      addr_set = 1;
	      current_insn_size += 1;
	      print (fp, col, "addr = cpu_get_y (proc) + (uint16) cpu_fetch8 (proc);");
	      print (fp, col, "%s%s = memory_read%s (proc, addr);",
		     vars[cur_var], operand_size, operand_size);
	      operands += 2;
	    }
	  else if (strncmp (operands, ")", 1) == 0)
	    {
	      addr_set = 1;
	      current_insn_size += 2;
	      print (fp, col, "addr = cpu_fetch16 (proc);");
	      print (fp, col, "%s%s = memory_read%s (proc, addr);",
		     vars[cur_var], operand_size, operand_size);
	      operands++;
	    }
	  else if (strncmp (operands, "@)", 2) == 0)
	    {
	      current_insn_size += 2;
	      print (fp, col, "addr = cpu_fetch16 (proc);");
	      print (fp, col, "%s%s = memory_read%s (proc, addr);",
		     vars[cur_var], operand_size, operand_size);
	      operands += 2;
	    }
	  else if (strncmp (operands, "sp)", 3) == 0)
	    {
	      print (fp, col, "%s%s = cpu_%s_pop_uint%s (proc);",
		     vars[cur_var], operand_size,
                     cpu_type == cpu6811 ? "m68hc11" : "m68hc12",
                     operand_size);
	      operands += 3;
	    }
	  else
	    {
	      fatal_error (opcode, "Unknown operand");
	    }
	  break;

	case '[':
	  if (cur_var >= 2)
	    fatal_error (opcode, "Too many locals");
	  
	  if (addr_set)
	    fatal_error (opcode, "Wrong use of '[', 'addr' already used");
	  
	  if (strncmp (operands, "]", 1) == 0)
	    {
	      addr_set = 1;
	      current_insn_size += 1;
	      print (fp, col, "addr = cpu_get_indexed_operand_addr (proc,0);");
	      print (fp, col, "%s%s = memory_read%s (proc, addr);",
		     vars[cur_var], operand_size, operand_size);
	      operands += 1;
	    }
	  else if (strncmp (operands, "]", 1) == 0)
	    {
	      current_insn_size += 1;
	      print (fp, col, "%s%s = cpu_get_indexed_operand%s (proc,0);",
		     vars[cur_var], operand_size, operand_size);
	      operands += 1;
	    }
	  else
	    {
	      fatal_error (opcode, "Unknown operand");
	    }
	  break;

	case '{':
	  if (cur_var >= 2)
	    fatal_error (opcode, "Too many locals");
	  
	  if (addr_set)
	    fatal_error (opcode, "Wrong use of '{', 'addr' already used");
	  
	  if (strncmp (operands, "}", 1) == 0)
	    {
	      current_insn_size += 1;
	      print (fp, col, "%s%s = cpu_get_indexed_operand%s (proc, 1);",
		     vars[cur_var], operand_size, operand_size);
	      operands += 1;
	    }
	  else
	    {
	      fatal_error (opcode, "Unknown operand");
	    }
	  break;

	case 's':
	  if (cur_var >= 2)
	    fatal_error (opcode, "Too many locals");
	  
	  if (strncmp (operands, "p", 1) == 0)
	    {
	      print (fp, col, "%s16 = cpu_get_sp (proc);", vars[cur_var]);
	      operands++;
	    }
	  else
	    {
	      fatal_error (opcode, "Unknown operands");
	    }
	  break;

	case 'c':
	  if (strncmp (operands, "cr", 2) == 0)
	    {
	      print (fp, col, "%s8 = cpu_get_ccr (proc);", vars[cur_var]);
	      operands += 2;
	    }
	  else
	    {
	      fatal_error (opcode, "Unknown operands");
	    }
	  break;
	  
	case 'r':
	  if (addr_set && cur_var != 2)
	    fatal_error (opcode, "Wrong use of 'r'");

	  addr_set = 1;
	  current_insn_size += 1;
	  print (fp, col, "addr = cpu_fetch_relbranch (proc);");
	  break;

	case 'R':
	  if (addr_set && cur_var != 2)
	    fatal_error (opcode, "Wrong use of 'R'");

	  addr_set = 1;
	  current_insn_size += 2;
	  print (fp, col, "addr = cpu_fetch_relbranch16 (proc);");
	  break;

	case '#':
	  if (strcmp (operand_size, "8") == 0)
	    {
	      current_insn_size += 1;
	    }
	  else
	    {
	      current_insn_size += 2;
	    }
	  print (fp, col, "%s%s = cpu_fetch%s (proc);", vars[cur_var],
		 operand_size, operand_size);
	  break;
	  
	case ',':
	  cur_var ++;
	  break;

	case '-':
	  return addr_set;

	default:
	  fatal_error (opcode, "Invalid operands");
	  break;
	}
    }
  return addr_set;
}


/* Generate the code to save the instruction result.  The result is in
   a local variable: either 'dst8' or 'dst16'.
   There may be only one result.  Instructions with 2 results (ie idiv
   and fdiv), take care of saving the first value.
  
   The operand string is the same as for 'gen_fetch_operands'.
   Everything before '->' is ignored.  If the '->' is not found, it
   is assumed that there is nothing to save.  After '->', the operand
   string is interpreted as follows:
  
   a	Save 'dst8' in A register
   b	"	       B "
   ccr	"	       CCR "
   d	"    'dst16'   D "
   x	"	       X "
   y	"	       Y "
   sp	"	       SP "
   *	68HC11 page0 memory pointer.
   (x)	68HC11 indirect access with X register.
   (y)	Same as (x) with Y register.
   ()	68HC11 extended address mode (global variable).
	For these modes, if they were used as an input operand,
	the 'addr' variable contains the address of memory where
	the result must be saved.
	If they were not used an input operand, 'addr' is computed
	(as in gen_fetch_operands()), and the result is saved.
   []   68HC12 indexed indirect
   (sp) Push
	Push the 8/16-bits result on the stack.	 */
void
gen_save_result (FILE *fp, int col,
		 const struct m6811_opcode_def *opcode,
		 int addr_set,
		 const char *operand_size)
{
  char c;
  const char *operands = opcode->operands;

  /* When the result is saved, 'result_size' is a string which
     indicates the size of the saved result ("8" or "16").  This
     is a sanity check with 'operand_size' to detect inconsistencies
     in the different tables.  */
  const char *result_size = 0;
  
  if (operands == 0)
    operands = "";

  operands = strchr (operands, '-');
  if (operands == 0)
    return;

  operands++;
  if (*operands++ != '>')
    {
      fatal_error (opcode, "Invalid operand");
    }

  c = *operands++;
  switch (c)
    {
    case 'a':
      result_size = "8";
      print (fp, col, "cpu_set_a (proc, dst8);");
      break;

    case 'b':
      result_size = "8";
      print (fp, col, "cpu_set_b (proc, dst8);");
      break;

    case 'd':
      result_size = "16";
      print (fp, col, "cpu_set_d (proc, dst16);");
      break;

    case 'x':
      result_size = "16";
      print (fp, col, "cpu_set_x (proc, dst16);");
      break;

    case 'y':
      result_size = "16";
      print (fp, col, "cpu_set_y (proc, dst16);");
      break;

    case '*':
      if (addr_set == 0)
	{
	  current_insn_size += 1;
	  print (fp, col, "addr = (uint16) cpu_fetch8 (proc);");
	}
      result_size = operand_size;
      print (fp, col, "memory_write%s (proc, addr, dst%s);",
	     operand_size, operand_size);
      break;

    case '(':
      if (strncmp (operands, "x)", 2) == 0)
	{
	  if (addr_set == 0)
	    {
	      current_insn_size += 1;
	      print (fp, col, "addr = cpu_get_x (proc) + cpu_fetch8 (proc);");
	    }
	  print (fp, col, "memory_write%s (proc, addr, dst%s);",
		 operand_size, operand_size);
	  operands += 2;
	  result_size = operand_size;
	}
      else if (strncmp (operands, "y)", 2) == 0)
	{
	  if (addr_set == 0)
	    {
	      current_insn_size += 1;
	      print (fp, col, "addr = cpu_get_y (proc) + cpu_fetch8 (proc);");
	    }
	  print (fp, col, "memory_write%s (proc, addr, dst%s);",
		 operand_size, operand_size);
	  operands += 2;
	  result_size = operand_size;
	}
      else if (strncmp (operands, ")", 1) == 0)
	{
	  if (addr_set == 0)
	    {
	      current_insn_size += 2;
	      print (fp, col, "addr = cpu_fetch16 (proc);");
	    }
	  print (fp, col, "memory_write%s (proc, addr, dst%s);",
		 operand_size, operand_size);
	  operands++;
	  result_size = operand_size;
	}
      else if (strncmp (operands, "sp)", 3) == 0)
	{
	  print (fp, col, "cpu_%s_push_uint%s (proc, dst%s);",
                 cpu_type == cpu6811 ? "m68hc11" : "m68hc12",
		 operand_size, operand_size);
	  operands += 3;
	  result_size = operand_size;
	}
      else
	{
	  fatal_error (opcode, "Invalid operand");
	}
      break;

    case '[':
      if (strncmp (operands, "]", 1) == 0)
	{
	  if (addr_set == 0)
	    {
	      current_insn_size += 1;
	      print (fp, col, "addr = cpu_get_indexed_operand_addr (proc,0);");
	    }
	  print (fp, col, "memory_write%s (proc, addr, dst%s);",
		 operand_size, operand_size);
	  operands++;
	  result_size = operand_size;
	}
      else
	{
	  fatal_error (opcode, "Invalid operand");
	}
      break;
      
    case '{':
      if (strncmp (operands, "}", 1) == 0)
	{
	  current_insn_size += 1;
	  print (fp, col, "addr = cpu_get_indexed_operand_addr (proc, 1);");
	  print (fp, col, "memory_write%s (proc, addr, dst%s);",
		 operand_size, operand_size);
	  operands++;
	  result_size = operand_size;
	}
      else
	{
	  fatal_error (opcode, "Invalid operand");
	}
      break;
      
    case 's':
      if (strncmp (operands, "p", 1) == 0)
	{
	  print (fp, col, "cpu_set_sp (proc, dst16);");
	  operands++;
	  result_size = "16";
	}
      else
	{
	  fatal_error (opcode, "Invalid operand");
	}
      break;

    case 'c':
      if (strncmp (operands, "cr", 2) == 0)
	{
	  print (fp, col, "cpu_set_ccr (proc, dst8);");
	  operands += 2;
	  result_size = "8";
	}
      else
	{
	  fatal_error (opcode, "Invalid operand");
	}
      break;
	  
    default:
      fatal_error (opcode, "Invalid operand");
      break;
    }

  if (*operands != 0)
    fatal_error (opcode, "Garbage at end of operand");
  
  if (result_size == 0)
    fatal_error (opcode, "? No result seems to be saved");

  if (strcmp (result_size, operand_size) != 0)
    fatal_error (opcode, "Result saved different than pattern size");
}


/* Find the instruction pattern for a given instruction.  */
const struct m6811_opcode_pattern*
find_opcode_pattern (const struct m6811_opcode_def *opcode)
{
  int i;
  const char *pattern = opcode->insn_pattern;
  
  if (pattern == 0)
    {
      pattern = opcode->name;
    }
  for (i = 0; i < TABLE_SIZE(m6811_opcode_patterns); i++)
    {
      if (strcmp (m6811_opcode_patterns[i].name, pattern) == 0)
	{
	  return &m6811_opcode_patterns[i];
	}
    }
  fatal_error (opcode, "Unknown instruction pattern");
  return 0;
}

/* Generate the code for interpretation of instruction 'opcode'.  */
void
gen_interp (FILE *fp, int col, const struct m6811_opcode_def *opcode)
{
  const char *operands = opcode->operands;
  int addr_set;
  const char *pattern = opcode->insn_pattern;
  const struct m6811_opcode_pattern *op;
  const char *operand_size;
  
  if (pattern == 0)
    {
      pattern = opcode->name;
    }

  /* Find out the size of the operands: 8 or 16-bits.  */
  if (strcmp(&pattern[strlen(pattern) - 1], "8") == 0)
    {
      operand_size = "8";
    }
  else if (strcmp (&pattern[strlen(pattern) - 2], "16") == 0)
    {
      operand_size = "16";
    }
  else
    {
      operand_size = "";
    }
  
  if (operands == 0)
    operands = "";

  /* Generate entry point for the instruction.	*/
  print (fp, col, "case 0x%02x: /* %s %s */\n", opcode->insn_code,
	 opcode->name, operands);
  col += indent_level;

  /* Generate the code to get the instruction operands.	 */
  addr_set = gen_fetch_operands (fp, col, opcode, operand_size);

  /* Generate instruction interpretation.  */
  op = find_opcode_pattern (opcode);
  if (op->pattern)
    {
      print (fp, col, "%s;", op->pattern);
    }

  /* Generate the code to save the result.  */
  gen_save_result (fp, col, opcode, addr_set, operand_size);

  /* For some instructions, generate the code to update the flags.  */
  if (op && op->ccr_update)
    {
      print (fp, col, "%s;", op->ccr_update);
    }
  print (fp, col, "break;");
}


/* Generate the interpretor for a given 68HC11 page set.  */
void
gen_interpreter_for_table (FILE *fp, int col,
			   const struct m6811_opcode_def *table,
			   int size,
			   const char *cycles_table_name)
{
  int i;
  int init_size;

  init_size = table == m6811_page1_opcodes
    || table == m6812_page1_opcodes? 1 : 2;
  
  /* Get the opcode and dispatch directly.  */
  print (fp, col, "op = cpu_fetch8 (proc);");
  print (fp, col, "cpu_add_cycles (proc, %s[op]);", cycles_table_name);
  
  print (fp, col, "switch (op)\n");
  col += indent_level;
  print (fp, col, "{\n");
  
  for (i = 0; i < size; i++)
    {
      /* The table contains duplicate entries (ie, instruction aliases).  */
      if (i > 0 && table[i].insn_code == table[i - 1].insn_code)
	continue;

      current_insn_size = init_size;
      gen_interp (fp, col, &table[i]);
#if 0
      if (current_insn_size != table[i].insn_size)
	{
	  fatal_error (&table[i], "Insn size %ld inconsistent with %ld",
		       current_insn_size, table[i].insn_size);
	}
#endif
    }

  print (fp, col, "default:\n");
  print (fp, col + indent_level, "cpu_special (proc, M6811_ILLEGAL);");
  print (fp, col + indent_level, "break;");
  print (fp, col, "}\n");
}

/* Generate the table of instruction cycle.  These tables are indexed
   by the opcode number to allow a fast cycle time computation.	 */
void
gen_cycle_table (FILE *fp, const char *name,
		 const struct m6811_opcode_def *table,
		 int size)
{
  int i;
  char cycles[256];
  int page1;

  page1 = table == m6811_page1_opcodes;

  /* Build the cycles table.  The table is indexed by the opcode.  */
  memset (cycles, 0, sizeof (cycles));
  while (--size >= 0)
    {
      if (table->insn_min_cycles > table->insn_max_cycles)
	fatal_error (table, "Wrong insn cycles");

      if (table->insn_max_cycles == _M)
	cycles[table->insn_code] = table->insn_min_cycles;
      else
	cycles[table->insn_code] = table->insn_max_cycles;

      table++;
    }

  /* Some check: for the page1 opcode, the cycle type of the page2/3/4
     opcode must be 0.	*/
  if (page1 && (cycles[M6811_OPCODE_PAGE2] != 0
		|| cycles[M6811_OPCODE_PAGE3] != 0
		|| cycles[M6811_OPCODE_PAGE4] != 0))
      fatal_error (0, "Invalid cycle table");

  /* Generates the cycles table.  */
  print (fp, 0, "static const unsigned char %s[256] = {\n", name);
  for (i = 0; i < 256; i++)
    {
      if ((i % 16) == 0)
	{
	  print (fp, indent_level, "/* %3d */ ", i);
	}
      fprintf (fp, "%2d", cycles[i]);
      if (i != 255)
	fprintf (fp, ",");

      if ((i % 16) != 15)
	fprintf (fp, " ");
      else
	fprintf (fp, "\n");
    }
  print (fp, 0, "};\n\n");
}

#define USE_SRC8 1
#define USE_DST8 2

void
gen_function_entry (FILE *fp, const char *name, int locals)
{
  /* Generate interpretor entry point.	*/
  print (fp, 0, "%s (proc)\n", name);
  print (fp, indent_level, "struct _sim_cpu* proc;");
  print (fp, indent_level, "{\n");

  /* Interpretor local variables.  */
  print (fp, indent_level, "unsigned char op;");
  print (fp, indent_level, "uint16 addr, src16, dst16;");
  if (locals & USE_SRC8)
    print (fp, indent_level, "uint8 src8;\n");
  if (locals & USE_DST8)
    print (fp, indent_level, "uint8 dst8;\n");
}

void
gen_function_close (FILE *fp)
{
  print (fp, 0, "}\n");
}

int
cmp_opcode (void* e1, void* e2)
{
  struct m6811_opcode_def* op1 = (struct m6811_opcode_def*) e1;
  struct m6811_opcode_def* op2 = (struct m6811_opcode_def*) e2;
  
  return (int) (op1->insn_code) - (int) (op2->insn_code);
}

void
prepare_table (struct m6811_opcode_def* table, int size)
{
  int i;
  
  qsort (table, size, sizeof (table[0]), cmp_opcode);
  for (i = 1; i < size; i++)
    {
      if (table[i].insn_code == table[i-1].insn_code)
	{
	  fprintf (stderr, "Two insns with code 0x%02x\n",
		   table[i].insn_code);
	}
    }
}

void
gen_interpreter (FILE *fp)
{
  int col = 0;

  prepare_table (m6811_page1_opcodes, TABLE_SIZE (m6811_page1_opcodes));
  prepare_table (m6811_page2_opcodes, TABLE_SIZE (m6811_page2_opcodes));
  prepare_table (m6811_page3_opcodes, TABLE_SIZE (m6811_page3_opcodes));
  prepare_table (m6811_page4_opcodes, TABLE_SIZE (m6811_page4_opcodes));

  prepare_table (m6812_page1_opcodes, TABLE_SIZE (m6812_page1_opcodes));
  prepare_table (m6812_page2_opcodes, TABLE_SIZE (m6812_page2_opcodes));

  /* Generate header of interpretor.  */
  print (fp, col, "/* File generated automatically by gencode. */\n");
  print (fp, col, "#include \"sim-main.h\"\n\n");

  if (cpu_type & cpu6811)
    {
      gen_cycle_table (fp, "cycles_page1", m6811_page1_opcodes,
		       TABLE_SIZE (m6811_page1_opcodes));
      gen_cycle_table (fp, "cycles_page2", m6811_page2_opcodes,
		       TABLE_SIZE (m6811_page2_opcodes));
      gen_cycle_table (fp, "cycles_page3", m6811_page3_opcodes,
		       TABLE_SIZE (m6811_page3_opcodes));
      gen_cycle_table (fp, "cycles_page4", m6811_page4_opcodes,
		       TABLE_SIZE (m6811_page4_opcodes));

      gen_function_entry (fp, "static void\ncpu_page3_interp", 0);
      gen_interpreter_for_table (fp, indent_level,
				 m6811_page3_opcodes,
				 TABLE_SIZE(m6811_page3_opcodes),
				 "cycles_page3");
      gen_function_close (fp);
  
      gen_function_entry (fp, "static void\ncpu_page4_interp", 0);
      gen_interpreter_for_table (fp, indent_level,
				 m6811_page4_opcodes,
				 TABLE_SIZE(m6811_page4_opcodes),
				 "cycles_page4");
      gen_function_close (fp);

      /* Generate the page 2, 3 and 4 handlers.  */
      gen_function_entry (fp, "static void\ncpu_page2_interp",
                          USE_SRC8 | USE_DST8);
      gen_interpreter_for_table (fp, indent_level,
				 m6811_page2_opcodes,
				 TABLE_SIZE(m6811_page2_opcodes),
				 "cycles_page2");
      gen_function_close (fp);

      /* Generate the interpretor entry point.  */
      gen_function_entry (fp, "void\ncpu_interp_m6811",
                          USE_SRC8 | USE_DST8);

      gen_interpreter_for_table (fp, indent_level, m6811_page1_opcodes,
				 TABLE_SIZE(m6811_page1_opcodes),
				 "cycles_page1");
      gen_function_close (fp);
    }
  else
    {
      gen_cycle_table (fp, "cycles_page1", m6812_page1_opcodes,
		       TABLE_SIZE (m6812_page1_opcodes));
      gen_cycle_table (fp, "cycles_page2", m6812_page2_opcodes,
		       TABLE_SIZE (m6812_page2_opcodes));

      gen_function_entry (fp, "static void\ncpu_page2_interp",
                          USE_SRC8 | USE_DST8);
      gen_interpreter_for_table (fp, indent_level,
				 m6812_page2_opcodes,
				 TABLE_SIZE(m6812_page2_opcodes),
				 "cycles_page2");
      gen_function_close (fp);

      /* Generate the interpretor entry point.  */
      gen_function_entry (fp, "void\ncpu_interp_m6812",
                          USE_SRC8 | USE_DST8);

      gen_interpreter_for_table (fp, indent_level, m6812_page1_opcodes,
				 TABLE_SIZE(m6812_page1_opcodes),
				 "cycles_page1");
      gen_function_close (fp);
    }
}

void
usage (char* prog)
{
  fprintf (stderr, "Usage: %s {-m6811|-m6812}\n", prog);
  exit (2);
}

int
main (int argc, char *argv[])
{
  int i;

  for (i = 1; i < argc; i++)
    {
      if (strcmp (argv[i], "-m6811") == 0)
	cpu_type = cpu6811;
      else if (strcmp (argv[i], "-m6812") == 0)
	cpu_type = cpu6812;
      else
	{
	  usage (argv[0]);
	}
    }
  if (cpu_type == 0)
    usage (argv[0]);
  
  gen_interpreter (stdout);
  if (fclose (stdout) != 0)
    {
      fprintf (stderr, "Error while generating the interpreter: %d\n",
	       errno);
      return 1;
    }
  return 0;
}
