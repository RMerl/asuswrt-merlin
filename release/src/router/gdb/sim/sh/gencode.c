/* Simulator/Opcode generator for the Renesas
   (formerly Hitachi) / SuperH Inc. Super-H architecture.

   Written by Steve Chamberlain of Cygnus Support.
   sac@cygnus.com

   This file is part of SH sim.


		THIS SOFTWARE IS NOT COPYRIGHTED

   Cygnus offers the following for use in the public domain.  Cygnus
   makes no warranty with regard to the software or it's performance
   and the user accepts the software "AS IS" with all faults.

   CYGNUS DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD TO
   THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

*/

/* This program generates the opcode table for the assembler and
   the simulator code.

   -t		prints a pretty table for the assembler manual
   -s		generates the simulator code jump table
   -d		generates a define table
   -x		generates the simulator code switch statement
   default 	used to generate the opcode tables

*/

#include <stdio.h>

#define MAX_NR_STUFF 42

typedef struct
{
  char *defs;
  char *refs;
  char *name;
  char *code;
  char *stuff[MAX_NR_STUFF];
  int index;
} op;


op tab[] =
{

  { "n", "", "add #<imm>,<REG_N>", "0111nnnni8*1....",
    "R[n] += SEXT (i);",
    "if (i == 0) {",
    "  UNDEF(n); /* see #ifdef PARANOID */",
    "  break;",
    "}",
  },
  { "n", "mn", "add <REG_M>,<REG_N>", "0011nnnnmmmm1100",
    "R[n] += R[m];",
  },

  { "n", "mn", "addc <REG_M>,<REG_N>", "0011nnnnmmmm1110",
    "ult = R[n] + T;",
    "SET_SR_T (ult < R[n]);",
    "R[n] = ult + R[m];",
    "SET_SR_T (T || (R[n] < ult));",
  },

  { "n", "mn", "addv <REG_M>,<REG_N>", "0011nnnnmmmm1111",
    "ult = R[n] + R[m];",
    "SET_SR_T ((~(R[n] ^ R[m]) & (ult ^ R[n])) >> 31);",
    "R[n] = ult;",
  },

  { "0", "0", "and #<imm>,R0", "11001001i8*1....",
    "R0 &= i;",
  },
  { "n", "nm", "and <REG_M>,<REG_N>", "0010nnnnmmmm1001",
    "R[n] &= R[m];",
  },
  { "", "0", "and.b #<imm>,@(R0,GBR)", "11001101i8*1....",
    "MA (1);",
    "WBAT (GBR + R0, RBAT (GBR + R0) & i);",
  },

  { "", "", "bf <bdisp8>", "10001011i8p1....",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "if (!T) {",
    "  SET_NIP (PC + 4 + (SEXT (i) * 2));",
    "  cycles += 2;",
    "}",
  },

  { "", "", "bf.s <bdisp8>", "10001111i8p1....",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "if (!T) {",
    "  SET_NIP (PC + 4 + (SEXT (i) * 2));",
    "  cycles += 2;",
    "  Delay_Slot (PC + 2);",
    "}",
  },

  { "", "n", "bit32 #imm3,@(disp12,<REG_N>)", "0011nnnni8*11001",
    "/* 32-bit logical bit-manipulation instructions.  */",
    "int word2 = RIAT (nip);",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "i >>= 4;	/* BOGUS: Using only three bits of 'i'.  */",
    "/* MSB of 'i' must be zero.  */",
    "if (i > 7)",
    "  RAISE_EXCEPTION (SIGILL);",
    "MA (1);",
    "do_blog_insn (1 << i, (word2 & 0xfff) + R[n], ",
    "              (word2 >> 12) & 0xf, memory, maskb);",
    "SET_NIP (nip + 2);	/* Consume 2 more bytes.  */",
  },
  { "", "", "bra <bdisp12>", "1010i12.........",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "SET_NIP (PC + 4 + (SEXT12 (i) * 2));",
    "cycles += 2;",
    "Delay_Slot (PC + 2);",
  },

  { "", "n", "braf <REG_N>", "0000nnnn00100011",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "SET_NIP (PC + 4 + R[n]);",
    "cycles += 2;",
    "Delay_Slot (PC + 2);",
  },

  { "", "", "bsr <bdisp12>", "1011i12.........",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "PR = PH2T (PC + 4);",
    "SET_NIP (PC + 4 + (SEXT12 (i) * 2));",
    "cycles += 2;",
    "Delay_Slot (PC + 2);",
  },

  { "", "n", "bsrf <REG_N>", "0000nnnn00000011",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "PR = PH2T (PC) + 4;",
    "SET_NIP (PC + 4 + R[n]);",
    "cycles += 2;",
    "Delay_Slot (PC + 2);",
  },

  { "", "", "bt <bdisp8>", "10001001i8p1....",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "if (T) {",
    "  SET_NIP (PC + 4 + (SEXT (i) * 2));",
    "  cycles += 2;",
    "}",
  },
  
  { "", "m", "bld/st #<imm>, <REG_M>", "10000111mmmmi4*1",
    "/* MSB of 'i' is true for load, false for store.  */",
    "if (i <= 7)",
    "  if (T)",
    "    R[m] |= (1 << i);",
    "  else",
    "    R[m] &= ~(1 << i);",
    "else",
    "  SET_SR_T ((R[m] & (1 << (i - 8))) != 0);",
  },
  { "m", "m", "bset/clr #<imm>, <REG_M>", "10000110mmmmi4*1",
    "/* MSB of 'i' is true for set, false for clear.  */",
    "if (i <= 7)",
    "  R[m] &= ~(1 << i);",
    "else",
    "  R[m] |= (1 << (i - 8));",
  },
  { "n", "n", "clips.b <REG_N>", "0100nnnn10010001",
    "if (R[n] < -128 || R[n] > 127) {",
    "  L (n);",
    "  SET_SR_CS (1);",
    "  if (R[n] > 127)",
    "    R[n] = 127;",
    "  else if (R[n] < -128)",
    "    R[n] = -128;",
    "}",
  },
  { "n", "n", "clips.w <REG_N>", "0100nnnn10010101",
    "if (R[n] < -32768 || R[n] > 32767) {",
    "  L (n);",
    "  SET_SR_CS (1);",
    "  if (R[n] > 32767)",
    "    R[n] = 32767;",
    "  else if (R[n] < -32768)",
    "    R[n] = -32768;",
    "}",
  },
  { "n", "n", "clipu.b <REG_N>", "0100nnnn10000001",
    "if (R[n] < -256 || R[n] > 255) {",
    "  L (n);",
    "  SET_SR_CS (1);",
    "  R[n] = 255;",
    "}",
  },
  { "n", "n", "clipu.w <REG_N>", "0100nnnn10000101",
    "if (R[n] < -65536 || R[n] > 65535) {",
    "  L (n);",
    "  SET_SR_CS (1);",
    "  R[n] = 65535;",
    "}",
  },
  { "n", "0n", "divs R0,<REG_N>", "0100nnnn10010100",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "if (R0 == 0)",
    "  R[n] = 0x7fffffff;",
    "else if (R0 == -1 && R[n] == 0x80000000)",
    "  R[n] = 0x7fffffff;",
    "else R[n] /= R0;",
    "L (n);",
  },
  { "n", "0n", "divu R0,<REG_N>", "0100nnnn10000100",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "if (R0 == 0)",
    "  R[n] = 0xffffffff;",
    "/* FIXME: The result may be implementation-defined if it is outside */",
    "/* the range of signed int (i.e. if R[n] was negative and R0 == 1).  */",
    "else R[n] = R[n] / (unsigned int) R0;",
    "L (n);",
  },
  { "n", "0n", "mulr R0,<REG_N>", "0100nnnn10000000",
    "R[n] = (R[n] * R0) & 0xffffffff;",
    "L (n);",
  },
  { "0", "n", "ldbank @<REG_N>,R0", "0100nnnn11100101",
    "int regn = (R[n] >> 2) & 0x1f;",
    "int bankn = (R[n] >> 7) & 0x1ff;",
    "if (regn > 19)",
    "  regn = 19;	/* FIXME what should happen? */",
    "R0 = saved_state.asregs.regstack[bankn].regs[regn];",
    "L (0);",
  },
  { "", "0n", "stbank R0,@<REG_N>", "0100nnnn11100001",
    "int regn = (R[n] >> 2) & 0x1f;",
    "int bankn = (R[n] >> 7) & 0x1ff;",
    "if (regn > 19)",
    "  regn = 19;	/* FIXME what should happen? */",
    "saved_state.asregs.regstack[bankn].regs[regn] = R0;",
  },
  { "", "", "resbank", "0000000001011011",
    "int i;",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    /* FIXME: cdef all */
    "if (BO) {	/* Bank Overflow */",
    /* FIXME: how do we know when to reset BO?  */
    "  for (i = 0; i <= 14; i++) {",
    "    R[i] = RLAT (R[15]);",
    "    MA (1);",
    "    R[15] += 4;",
    "  }",
    "  PR = RLAT (R[15]);",
    "  R[15] += 4;",
    "  MA (1);",
    "  GBR = RLAT (R[15]);",
    "  R[15] += 4;",
    "  MA (1);",
    "  MACH = RLAT (R[15]);",
    "  R[15] += 4;",
    "  MA (1);",
    "  MACL = RLAT (R[15]);",
    "  R[15] += 4;",
    "  MA (1);",
    "}",
    "else if (BANKN == 0)	/* Bank Underflow */",
    "  RAISE_EXCEPTION (SIGILL);",	/* FIXME: what exception? */
    "else {",
    "  SET_BANKN (BANKN - 1);",
    "  for (i = 0; i <= 14; i++)",
    "    R[i] = saved_state.asregs.regstack[BANKN].regs[i];",
    "  MACH = saved_state.asregs.regstack[BANKN].regs[15];",
    "  PR   = saved_state.asregs.regstack[BANKN].regs[17];",
    "  GBR  = saved_state.asregs.regstack[BANKN].regs[18];",
    "  MACL = saved_state.asregs.regstack[BANKN].regs[19];",
    "}",
  },
  { "f", "f-", "movml.l <REG_N>,@-R15", "0100nnnn11110001",
    "/* Push Rn...R0 (if n==15, push pr and R14...R0).  */",
    "do {",
    "  MA (1);",
    "  R[15] -= 4;",
    "  if (n == 15)",
    "    WLAT (R[15], PR);",
    "  else",
    "    WLAT (R[15], R[n]);",
    "} while (n-- > 0);",    
  },
  { "f", "f+", "movml.l @R15+,<REG_N>", "0100nnnn11110101",
    "/* Pop R0...Rn (if n==15, pop R0...R14 and pr).  */",
    "int i = 0;\n",
    "do {",
    "  MA (1);",
    "  if (i == 15)",
    "    PR = RLAT (R[15]);",
    "  else",
    "    R[i] = RLAT (R[15]);",
    "  R[15] += 4;",
    "} while (i++ < n);",    
  },
  { "f", "f-", "movmu.l <REG_N>,@-R15", "0100nnnn11110000",
    "/* Push pr, R14...Rn (if n==15, push pr).  */",	/* FIXME */
    "int i = 15;\n",
    "do {",
    "  MA (1);",
    "  R[15] -= 4;",
    "  if (i == 15)",
    "    WLAT (R[15], PR);",
    "  else",
    "    WLAT (R[15], R[i]);",
    "} while (i-- > n);",    
  },
  { "f", "f+", "movmu.l @R15+,<REG_N>", "0100nnnn11110100",
    "/* Pop Rn...R14, pr (if n==15, pop pr).  */",	/* FIXME */
    "do {",
    "  MA (1);",
    "  if (n == 15)",
    "    PR = RLAT (R[15]);",
    "  else",
    "    R[n] = RLAT (R[15]);",
    "  R[15] += 4;",
    "} while (n++ < 15);",    
  },
  { "", "", "nott", "0000000001101000",
    "SET_SR_T (T == 0);",	
  },

  { "", "", "bt.s <bdisp8>", "10001101i8p1....",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "if (T) {",
    "  SET_NIP (PC + 4 + (SEXT (i) * 2));",
    "  cycles += 2;",
    "  Delay_Slot (PC + 2);",
    "}",
  },

  { "", "", "clrmac", "0000000000101000",
    "MACH = 0;",
    "MACL = 0;",
  },

  { "", "", "clrs", "0000000001001000",
    "SET_SR_S (0);",
  },

  { "", "", "clrt", "0000000000001000",
    "SET_SR_T (0);",
  },

  /* sh4a */
  { "", "", "clrdmxy", "0000000010001000",
    "saved_state.asregs.cregs.named.sr &= ~(SR_MASK_DMX | SR_MASK_DMY);"
  },

  { "", "0", "cmp/eq #<imm>,R0", "10001000i8*1....",
    "SET_SR_T (R0 == SEXT (i));",
  },
  { "", "mn", "cmp/eq <REG_M>,<REG_N>", "0011nnnnmmmm0000",
    "SET_SR_T (R[n] == R[m]);",
  },
  { "", "mn", "cmp/ge <REG_M>,<REG_N>", "0011nnnnmmmm0011",
    "SET_SR_T (R[n] >= R[m]);",
  },
  { "", "mn", "cmp/gt <REG_M>,<REG_N>", "0011nnnnmmmm0111",
    "SET_SR_T (R[n] > R[m]);",
  },
  { "", "mn", "cmp/hi <REG_M>,<REG_N>", "0011nnnnmmmm0110",
    "SET_SR_T (UR[n] > UR[m]);",
  },
  { "", "mn", "cmp/hs <REG_M>,<REG_N>", "0011nnnnmmmm0010",
    "SET_SR_T (UR[n] >= UR[m]);",
  },
  { "", "n", "cmp/pl <REG_N>", "0100nnnn00010101",
    "SET_SR_T (R[n] > 0);",
  },
  { "", "n", "cmp/pz <REG_N>", "0100nnnn00010001",
    "SET_SR_T (R[n] >= 0);",
  },
  { "", "mn", "cmp/str <REG_M>,<REG_N>", "0010nnnnmmmm1100",
    "ult = R[n] ^ R[m];",
    "SET_SR_T (((ult & 0xff000000) == 0)",
    "          | ((ult & 0xff0000) == 0)",
    "          | ((ult & 0xff00) == 0)",
    "          | ((ult & 0xff) == 0));",
  },

  { "", "mn", "div0s <REG_M>,<REG_N>", "0010nnnnmmmm0111",
    "SET_SR_Q ((R[n] & sbit) != 0);",
    "SET_SR_M ((R[m] & sbit) != 0);",
    "SET_SR_T (M != Q);",
  },

  { "", "", "div0u", "0000000000011001",
    "SET_SR_M (0);",
    "SET_SR_Q (0);",
    "SET_SR_T (0);",
  },

  { "n", "nm", "div1 <REG_M>,<REG_N>", "0011nnnnmmmm0100",
    "div1 (&R0, m, n/*, T*/);",
  },

  { "", "nm", "dmuls.l <REG_M>,<REG_N>", "0011nnnnmmmm1101",
    "dmul (1/*signed*/, R[n], R[m]);",
  },

  { "", "nm", "dmulu.l <REG_M>,<REG_N>", "0011nnnnmmmm0101",
    "dmul (0/*unsigned*/, R[n], R[m]);",
  },

  { "n", "n", "dt <REG_N>", "0100nnnn00010000",
    "R[n]--;",
    "SET_SR_T (R[n] == 0);",
  },

  { "n", "m", "exts.b <REG_M>,<REG_N>", "0110nnnnmmmm1110",
    "R[n] = SEXT (R[m]);",
  },
  { "n", "m", "exts.w <REG_M>,<REG_N>", "0110nnnnmmmm1111",
    "R[n] = SEXTW (R[m]);",
  },

  { "n", "m", "extu.b <REG_M>,<REG_N>", "0110nnnnmmmm1100",
    "R[n] = (R[m] & 0xff);",
  },
  { "n", "m", "extu.w <REG_M>,<REG_N>", "0110nnnnmmmm1101",
    "R[n] = (R[m] & 0xffff);",
  },

  /* sh2e */
  { "", "", "fabs <FREG_N>", "1111nnnn01011101",
    "FP_UNARY (n, fabs);",
    "/* FIXME: FR (n) &= 0x7fffffff; */",
  },

  /* sh2e */
  { "", "", "fadd <FREG_M>,<FREG_N>", "1111nnnnmmmm0000",
    "FP_OP (n, +, m);",
  },

  /* sh2e */
  { "", "", "fcmp/eq <FREG_M>,<FREG_N>", "1111nnnnmmmm0100",
    "FP_CMP (n, ==, m);",
  },
  /* sh2e */
  { "", "", "fcmp/gt <FREG_M>,<FREG_N>", "1111nnnnmmmm0101",
    "FP_CMP (n, >, m);",
  },

  /* sh4 */
  { "", "", "fcnvds <DR_N>,FPUL", "1111nnnn10111101",
    "if (! FPSCR_PR || n & 1)",
    "  RAISE_EXCEPTION (SIGILL);",
    "else",
    "{",
    "  union",
    "  {",
    "    int i;",
    "    float f;",
    "  } u;",
    "  u.f = DR (n);",
    "  FPUL = u.i;",
    "}",
  },

  /* sh4 */
  { "", "", "fcnvsd FPUL,<DR_N>", "1111nnnn10101101",
    "if (! FPSCR_PR || n & 1)",
    "  RAISE_EXCEPTION (SIGILL);",
    "else",
    "{",
    "  union",
    "  {",
    "    int i;",
    "    float f;",
    "  } u;",
    "  u.i = FPUL;",
    "  SET_DR (n, u.f);",
    "}",
  },

  /* sh2e */
  { "", "", "fdiv <FREG_M>,<FREG_N>", "1111nnnnmmmm0011",
    "FP_OP (n, /, m);",
    "/* FIXME: check for DP and (n & 1) == 0?  */",
  },

  /* sh4 */
  { "", "", "fipr <FV_M>,<FV_N>", "1111vvVV11101101",
    "if (FPSCR_PR)", 
    "  RAISE_EXCEPTION (SIGILL);",
    "else",
    "{",
    "  double fsum = 0;",
    "  if (saved_state.asregs.bfd_mach == bfd_mach_sh2a)",
    "    RAISE_EXCEPTION (SIGILL);",
    "  /* FIXME: check for nans and infinities.  */",
    "  fsum += FR (v1+0) * FR (v2+0);",
    "  fsum += FR (v1+1) * FR (v2+1);",
    "  fsum += FR (v1+2) * FR (v2+2);",
    "  fsum += FR (v1+3) * FR (v2+3);",
    "  SET_FR (v1+3, fsum);",
    "}",
  },

  /* sh2e */
  { "", "", "fldi0 <FREG_N>", "1111nnnn10001101",
    "SET_FR (n, (float) 0.0);",
    "/* FIXME: check for DP and (n & 1) == 0?  */",
  },

  /* sh2e */
  { "", "", "fldi1 <FREG_N>", "1111nnnn10011101",
    "SET_FR (n, (float) 1.0);",
    "/* FIXME: check for DP and (n & 1) == 0?  */",
  },

  /* sh2e */
  { "", "", "flds <FREG_N>,FPUL", "1111nnnn00011101",
    "  union",
    "  {",
    "    int i;",
    "    float f;",
    "  } u;",
    "  u.f = FR (n);",
    "  FPUL = u.i;",
  },

  /* sh2e */
  { "", "", "float FPUL,<FREG_N>", "1111nnnn00101101",
    /* sh4 */
    "if (FPSCR_PR)",
    "  SET_DR (n, (double) FPUL);",
    "else",
    "{",
    "  SET_FR (n, (float) FPUL);",
    "}",
  },

  /* sh2e */
  { "", "", "fmac <FREG_0>,<FREG_M>,<FREG_N>", "1111nnnnmmmm1110",
    "SET_FR (n, FR (m) * FR (0) + FR (n));",
    "/* FIXME: check for DP and (n & 1) == 0? */",
  },

  /* sh2e */
  { "", "", "fmov <FREG_M>,<FREG_N>", "1111nnnnmmmm1100",
    /* sh4 */
    "if (FPSCR_SZ) {",
    "  int ni = XD_TO_XF (n);",
    "  int mi = XD_TO_XF (m);",
    "  SET_XF (ni + 0, XF (mi + 0));",
    "  SET_XF (ni + 1, XF (mi + 1));",
    "}",
    "else",
    "{",
    "  SET_FR (n, FR (m));",
    "}",
  },
  /* sh2e */
  { "", "n", "fmov.s <FREG_M>,@<REG_N>", "1111nnnnmmmm1010",
    /* sh4 */
    "if (FPSCR_SZ) {",
    "  MA (2);",
    "  WDAT (R[n], m);",
    "}",
    "else",
    "{",
    "  MA (1);",
    "  WLAT (R[n], FI (m));",
    "}",
  },
  /* sh2e */
  { "", "m", "fmov.s @<REG_M>,<FREG_N>", "1111nnnnmmmm1000",
    /* sh4 */
    "if (FPSCR_SZ) {",
    "  MA (2);",
    "  RDAT (R[m], n);",
    "}",
    "else",
    "{",
    "  MA (1);",
    "  SET_FI (n, RLAT (R[m]));",
    "}",
  },
  /* sh2a */
  { "", "n", "fmov.s @(disp12,<REG_N>), <FREG_M>", "0011nnnnmmmm0001",
    "/* and fmov.s <FREG_N>, @(disp12,<FREG_M>)",
    "   and mov.bwl <REG_N>, @(disp12,<REG_M>)",
    "   and mov.bwl @(disp12,<REG_N>),<REG_M>",
    "   and movu.bw @(disp12,<REG_N>),<REG_M>.  */",
    "int word2 = RIAT (nip);",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "SET_NIP (nip + 2);	/* Consume 2 more bytes.  */",
    "MA (1);",
    "do_long_move_insn (word2 & 0xf000, word2 & 0x0fff, m, n, &thislock);",
  },
  /* sh2e */
  { "m", "m", "fmov.s @<REG_M>+,<FREG_N>", "1111nnnnmmmm1001",
    /* sh4 */
    "if (FPSCR_SZ) {",
    "  MA (2);",
    "  RDAT (R[m], n);",
    "  R[m] += 8;",
    "}",
    "else",
    "{",
    "  MA (1);",
    "  SET_FI (n, RLAT (R[m]));",
    "  R[m] += 4;",
    "}",
  },
  /* sh2e */
  { "n", "n", "fmov.s <FREG_M>,@-<REG_N>", "1111nnnnmmmm1011",
    /* sh4 */
    "if (FPSCR_SZ) {",
    "  MA (2);",
    "  R[n] -= 8;",
    "  WDAT (R[n], m);",
    "}",
    "else",
    "{",
    "  MA (1);",
    "  R[n] -= 4;",
    "  WLAT (R[n], FI (m));",
    "}",
  },
  /* sh2e */
  { "", "0m", "fmov.s @(R0,<REG_M>),<FREG_N>", "1111nnnnmmmm0110",
    /* sh4 */
    "if (FPSCR_SZ) {",
    "  MA (2);",
    "  RDAT (R[0]+R[m], n);",
    "}",
    "else",
    "{",
    "  MA (1);",
    "  SET_FI (n, RLAT (R[0] + R[m]));",
    "}",
  },
  /* sh2e */
  { "", "0n", "fmov.s <FREG_M>,@(R0,<REG_N>)", "1111nnnnmmmm0111",
    /* sh4 */
    "if (FPSCR_SZ) {",
    "  MA (2);",
    "  WDAT (R[0]+R[n], m);",
    "}",
    "else",
    "{",
    "  MA (1);",
    "  WLAT ((R[0]+R[n]), FI (m));",
    "}",
  },

  /* sh4: 
     See fmov instructions above for move to/from extended fp registers.  */

  /* sh2e */
  { "", "", "fmul <FREG_M>,<FREG_N>", "1111nnnnmmmm0010",
    "FP_OP (n, *, m);",
  },

  /* sh2e */
  { "", "", "fneg <FREG_N>", "1111nnnn01001101",
    "FP_UNARY (n, -);",
  },

  /* sh4a */
  { "", "", "fpchg", "1111011111111101",
    "SET_FPSCR (GET_FPSCR () ^ FPSCR_MASK_PR);",
  },

  /* sh4 */
  { "", "", "frchg", "1111101111111101",
    "if (FPSCR_PR)",
    "  RAISE_EXCEPTION (SIGILL);",
    "else if (saved_state.asregs.bfd_mach == bfd_mach_sh2a)",
    "  RAISE_EXCEPTION (SIGILL);",
    "else",
    "  SET_FPSCR (GET_FPSCR () ^ FPSCR_MASK_FR);",
  },

  /* sh4 */
  { "", "", "fsca", "1111eeee11111101",
    "if (FPSCR_PR)",
    "  RAISE_EXCEPTION (SIGILL);",
    "else if (saved_state.asregs.bfd_mach == bfd_mach_sh2a)",
    "  RAISE_EXCEPTION (SIGILL);",
    "else",
    "  {",
    "    SET_FR (n, fsca_s (FPUL, &sin));",
    "    SET_FR (n+1, fsca_s (FPUL, &cos));",
    "  }",
  },

  /* sh4 */
  { "", "", "fschg", "1111001111111101",
    "SET_FPSCR (GET_FPSCR () ^ FPSCR_MASK_SZ);",
  },

  /* sh3e */
  { "", "", "fsqrt <FREG_N>", "1111nnnn01101101",
    "FP_UNARY (n, sqrt);",
  },

  /* sh4 */
  { "", "", "fsrra <FREG_N>", "1111nnnn01111101",
    "if (FPSCR_PR)",
    "  RAISE_EXCEPTION (SIGILL);",
    "else if (saved_state.asregs.bfd_mach == bfd_mach_sh2a)",
    "  RAISE_EXCEPTION (SIGILL);",
    "else",
    "  SET_FR (n, fsrra_s (FR (n)));",
  },

  /* sh2e */
  { "", "", "fsub <FREG_M>,<FREG_N>", "1111nnnnmmmm0001",
    "FP_OP (n, -, m);",
  },

  /* sh2e */
  { "", "", "ftrc <FREG_N>, FPUL", "1111nnnn00111101",
    /* sh4 */
    "if (FPSCR_PR) {",
    "  if (DR (n) != DR (n)) /* NaN */",
    "    FPUL = 0x80000000;",
    "  else",
    "    FPUL =  (int) DR (n);",
    "}",
    "else",
    "if (FR (n) != FR (n)) /* NaN */",
    "  FPUL = 0x80000000;",
    "else",
    "  FPUL = (int) FR (n);",
  },

  /* sh4 */
  { "", "", "ftrv <FV_N>", "1111vv0111111101",
    "if (FPSCR_PR)",
    "  RAISE_EXCEPTION (SIGILL);",
    "else",
    "{", 
    "  if (saved_state.asregs.bfd_mach == bfd_mach_sh2a)",
    "    RAISE_EXCEPTION (SIGILL);",
    "  /* FIXME not implemented.  */",
    "  printf (\"ftrv xmtrx, FV%d\\n\", v1);",
    "}", 
  },

  /* sh2e */
  { "", "", "fsts FPUL,<FREG_N>", "1111nnnn00001101",
    "  union",
    "  {",
    "    int i;",
    "    float f;",
    "  } u;",
    "  u.i = FPUL;",
    "  SET_FR (n, u.f);",
  },

  { "", "n", "jmp @<REG_N>", "0100nnnn00101011",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "SET_NIP (PT2H (R[n]));",
    "cycles += 2;",
    "Delay_Slot (PC + 2);",
  },

  { "", "n", "jsr @<REG_N>", "0100nnnn00001011",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "PR = PH2T (PC + 4);",
    "if (~doprofile)",
    "  gotcall (PR, R[n]);",
    "SET_NIP (PT2H (R[n]));",
    "cycles += 2;",
    "Delay_Slot (PC + 2);",
  },
  { "", "n", "jsr/n @<REG_N>", "0100nnnn01001011",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "PR = PH2T (PC + 2);",
    "if (~doprofile)",
    "  gotcall (PR, R[n]);",
    "SET_NIP (PT2H (R[n]));",
  },
  { "", "", "jsr/n @@(<disp>,TBR)", "10000011i8p4....",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "PR = PH2T (PC + 2);",
    "if (~doprofile)",
    "  gotcall (PR, i + TBR);",
    "SET_NIP (PT2H (i + TBR));",
  },

  { "", "n", "ldc <REG_N>,<CREG_M>", "0100nnnnmmmm1110",
    "CREG (m) = R[n];",
    "/* FIXME: user mode */",
  },
  { "", "n", "ldc <REG_N>,SR", "0100nnnn00001110",
    "SET_SR (R[n]);",
    "/* FIXME: user mode */",
  },
  { "", "n", "ldc <REG_N>,MOD", "0100nnnn01011110",
    "SET_MOD (R[n]);",
  },
  { "", "n", "ldc <REG_N>,DBR", "0100nnnn11111010",
    "if (SR_MD)",
    "  DBR = R[n]; /* priv mode */",
    "else",
    "  RAISE_EXCEPTION (SIGILL); /* user mode */",
  },
  { "", "n", "ldc <REG_N>,SGR", "0100nnnn00111010",
    "if (SR_MD)",
    "  SGR = R[n]; /* priv mode */",
    "else",
    "  RAISE_EXCEPTION (SIGILL); /* user mode */",
  },
  { "", "n", "ldc <REG_N>,TBR", "0100nnnn01001010",
    "if (SR_MD)",	/* FIXME? */
    "  TBR = R[n]; /* priv mode */",
    "else",
    "  RAISE_EXCEPTION (SIGILL); /* user mode */",
  },
  { "n", "n", "ldc.l @<REG_N>+,<CREG_M>", "0100nnnnmmmm0111",
    "MA (1);",
    "CREG (m) = RLAT (R[n]);",
    "R[n] += 4;",
    "/* FIXME: user mode */",
  },
  { "n", "n", "ldc.l @<REG_N>+,SR", "0100nnnn00000111",
    "MA (1);",
    "SET_SR (RLAT (R[n]));",
    "R[n] += 4;",
    "/* FIXME: user mode */",
  },
  { "n", "n", "ldc.l @<REG_N>+,MOD", "0100nnnn01010111",
    "MA (1);",
    "SET_MOD (RLAT (R[n]));",
    "R[n] += 4;",
  },
  { "n", "n", "ldc.l @<REG_N>+,DBR", "0100nnnn11110110",
    "if (SR_MD)",
    "{ /* priv mode */",
    "  MA (1);",
    "  DBR = RLAT (R[n]);",
    "  R[n] += 4;",
    "}",
    "else",
    "  RAISE_EXCEPTION (SIGILL); /* user mode */",
  },
  { "n", "n", "ldc.l @<REG_N>+,SGR", "0100nnnn00110110",
    "if (SR_MD)",
    "{ /* priv mode */",
    "  MA (1);",
    "  SGR = RLAT (R[n]);",
    "  R[n] += 4;",
    "}",
    "else",
    "  RAISE_EXCEPTION (SIGILL); /* user mode */",
  },

  /* sh-dsp */
  { "", "", "ldre @(<disp>,PC)", "10001110i8p1....",
    "RE = SEXT (i) * 2 + 4 + PH2T (PC);",
  },
  { "", "", "ldrs @(<disp>,PC)", "10001100i8p1....",
    "RS = SEXT (i) * 2 + 4 + PH2T (PC);",
  },

  /* sh4a */
  { "", "n", "ldrc <REG_N>", "0100nnnn00110100",
    "SET_RC (R[n]);",
    "loop = get_loop_bounds_ext (RS, RE, memory, mem_end, maskw, endianw);",
    "CHECK_INSN_PTR (insn_ptr);",
    "RE |= 1;",
  },
  { "", "", "ldrc #<imm>", "10001010i8*1....",
    "SET_RC (i);",
    "loop = get_loop_bounds_ext (RS, RE, memory, mem_end, maskw, endianw);",
    "CHECK_INSN_PTR (insn_ptr);",
    "RE |= 1;",
  },

  { "", "n", "lds <REG_N>,<SREG_M>", "0100nnnnssss1010",
    "SREG (m) = R[n];",
  },
  { "n", "n", "lds.l @<REG_N>+,<SREG_M>", "0100nnnnssss0110",
    "MA (1);",
    "SREG (m) = RLAT (R[n]);",
    "R[n] += 4;",
  },
  /* sh2e / sh-dsp (lds <REG_N>,DSR) */
  { "", "n", "lds <REG_N>,FPSCR", "0100nnnn01101010",
    "SET_FPSCR (R[n]);",
  },
  /* sh2e / sh-dsp (lds.l @<REG_N>+,DSR) */
  { "n", "n", "lds.l @<REG_N>+,FPSCR", "0100nnnn01100110",
    "MA (1);",
    "SET_FPSCR (RLAT (R[n]));",
    "R[n] += 4;",
  },

  { "", "", "ldtlb", "0000000000111000",
    "/* We don't implement cache or tlb, so this is a noop.  */",
  },

  { "nm", "nm", "mac.l @<REG_M>+,@<REG_N>+", "0000nnnnmmmm1111",
    "macl (&R0, memory, n, m);",
  },

  { "nm", "nm", "mac.w @<REG_M>+,@<REG_N>+", "0100nnnnmmmm1111",
    "macw (&R0, memory, n, m, endianw);",
  },

  { "n", "", "mov #<imm>,<REG_N>", "1110nnnni8*1....",
    "R[n] = SEXT (i);",
  },
  { "n", "", "movi20 #<imm20>,<REG_N>", "0000nnnni8*10000",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "R[n] = ((i << 24) >> 12) | RIAT (nip);",
    "SET_NIP (nip + 2);	/* Consume 2 more bytes.  */",
  },
  { "n", "", "movi20s #<imm20>,<REG_N>", "0000nnnni8*10001",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "R[n] = ((((i & 0xf0) << 24) >> 12) | RIAT (nip)) << 8;",
    "SET_NIP (nip + 2);	/* Consume 2 more bytes.  */",
  },
  { "n", "m", "mov <REG_M>,<REG_N>", "0110nnnnmmmm0011",
    "R[n] = R[m];",
  },

  { "0", "", "mov.b @(<disp>,GBR),R0", "11000100i8*1....",
    "MA (1);",
    "R0 = RSBAT (i + GBR);",
    "L (0);",
  },
  { "0", "m", "mov.b @(<disp>,<REG_M>),R0", "10000100mmmmi4*1",
    "MA (1);",
    "R0 = RSBAT (i + R[m]);",
    "L (0);",
  },
  { "n", "0m", "mov.b @(R0,<REG_M>),<REG_N>", "0000nnnnmmmm1100",
    "MA (1);",
    "R[n] = RSBAT (R0 + R[m]);",
    "L (n);",
  },
  { "nm", "m", "mov.b @<REG_M>+,<REG_N>", "0110nnnnmmmm0100",
    "MA (1);",
    "R[n] = RSBAT (R[m]);",
    "R[m] += 1;",
    "L (n);",
  },
  { "0n", "n", "mov.b @-<REG_N>,R0", "0100nnnn11001011",
    "MA (1);",
    "R[n] -= 1;",
    "R0 = RSBAT (R[n]);",
    "L (0);",
  },
  { "", "mn", "mov.b <REG_M>,@<REG_N>", "0010nnnnmmmm0000",
    "MA (1);",
    "WBAT (R[n], R[m]);",
  },
  { "", "0", "mov.b R0,@(<disp>,GBR)", "11000000i8*1....",
    "MA (1);",
    "WBAT (i + GBR, R0);",
  },
  { "", "m0", "mov.b R0,@(<disp>,<REG_M>)", "10000000mmmmi4*1",
    "MA (1);",
    "WBAT (i + R[m], R0);",
  },
  { "", "mn0", "mov.b <REG_M>,@(R0,<REG_N>)", "0000nnnnmmmm0100",
    "MA (1);",
    "WBAT (R[n] + R0, R[m]);",
  },
  { "n", "nm", "mov.b <REG_M>,@-<REG_N>", "0010nnnnmmmm0100",
    /* Allow for the case where m == n.  */
    "int t = R[m];",
    "MA (1);",
    "R[n] -= 1;",
    "WBAT (R[n], t);",
  },
  { "n", "n0", "mov.b R0,@<REG_N>+", "0100nnnn10001011",
    "MA (1);",
    "WBAT (R[n], R0);",
    "R[n] += 1;",
  },
  { "n", "m", "mov.b @<REG_M>,<REG_N>", "0110nnnnmmmm0000",
    "MA (1);",
    "R[n] = RSBAT (R[m]);",
    "L (n);",
  },

  { "0", "", "mov.l @(<disp>,GBR),R0", "11000110i8*4....",
    "MA (1);",
    "R0 = RLAT (i + GBR);",
    "L (0);",
  },
  { "n", "", "mov.l @(<disp>,PC),<REG_N>", "1101nnnni8p4....",
    "MA (1);",
    "R[n] = RLAT ((PH2T (PC) & ~3) + 4 + i);",
    "L (n);",
  },
  { "n", "m", "mov.l @(<disp>,<REG_M>),<REG_N>", "0101nnnnmmmmi4*4",
    "MA (1);",
    "R[n] = RLAT (i + R[m]);",
    "L (n);",
  },
  { "n", "m0", "mov.l @(R0,<REG_M>),<REG_N>", "0000nnnnmmmm1110",
    "MA (1);",
    "R[n] = RLAT (R0 + R[m]);",
    "L (n);",
  },
  { "nm", "m", "mov.l @<REG_M>+,<REG_N>", "0110nnnnmmmm0110",
    "MA (1);",
    "R[n] = RLAT (R[m]);",
    "R[m] += 4;",
    "L (n);",
  },
  { "0n", "n", "mov.l @-<REG_N>,R0", "0100nnnn11101011",
    "MA (1);",
    "R[n] -= 4;",
    "R0 = RLAT (R[n]);",
    "L (0);",
  },
  { "n", "m", "mov.l @<REG_M>,<REG_N>", "0110nnnnmmmm0010",
    "MA (1);",
    "R[n] = RLAT (R[m]);",
    "L (n);",
  },
  { "", "0", "mov.l R0,@(<disp>,GBR)", "11000010i8*4....",
    "MA (1);",
    "WLAT (i + GBR, R0);",
  },
  { "", "nm", "mov.l <REG_M>,@(<disp>,<REG_N>)", "0001nnnnmmmmi4*4",
    "MA (1);",
    "WLAT (i + R[n], R[m]);",
  },
  { "", "nm0", "mov.l <REG_M>,@(R0,<REG_N>)", "0000nnnnmmmm0110",
    "MA (1);",
    "WLAT (R0 + R[n], R[m]);",
  },
  { "n", "nm", "mov.l <REG_M>,@-<REG_N>", "0010nnnnmmmm0110",
    /* Allow for the case where m == n.  */
    "int t = R[m];",
    "MA (1) ;",
    "R[n] -= 4;",
    "WLAT (R[n], t);",
  },
  { "n", "n0", "mov.l R0,@<REG_N>+", "0100nnnn10101011",
    "MA (1) ;",
    "WLAT (R[n], R0);",
    "R[n] += 4;",
  },
  { "", "nm", "mov.l <REG_M>,@<REG_N>", "0010nnnnmmmm0010",
    "MA (1);",
    "WLAT (R[n], R[m]);",
  },

  { "0", "", "mov.w @(<disp>,GBR),R0", "11000101i8*2....",
    "MA (1);",
    "R0 = RSWAT (i + GBR);",
    "L (0);",
  },
  { "n", "", "mov.w @(<disp>,PC),<REG_N>", "1001nnnni8p2....",
    "MA (1);",
    "R[n] = RSWAT (PH2T (PC + 4 + i));",
    "L (n);",
  },
  { "0", "m", "mov.w @(<disp>,<REG_M>),R0", "10000101mmmmi4*2",
    "MA (1);",
    "R0 = RSWAT (i + R[m]);",
    "L (0);",
  },
  { "n", "m0", "mov.w @(R0,<REG_M>),<REG_N>", "0000nnnnmmmm1101",
    "MA (1);",
    "R[n] = RSWAT (R0 + R[m]);",
    "L (n);",
  },
  { "nm", "n", "mov.w @<REG_M>+,<REG_N>", "0110nnnnmmmm0101",
    "MA (1);",
    "R[n] = RSWAT (R[m]);",
    "R[m] += 2;",
    "L (n);",
  },
  { "0n", "n", "mov.w @-<REG_N>,R0", "0100nnnn11011011",
    "MA (1);",
    "R[n] -= 2;",
    "R0 = RSWAT (R[n]);",
    "L (0);",
  },
  { "n", "m", "mov.w @<REG_M>,<REG_N>", "0110nnnnmmmm0001",
    "MA (1);",
    "R[n] = RSWAT (R[m]);",
    "L (n);",
  },
  { "", "0", "mov.w R0,@(<disp>,GBR)", "11000001i8*2....",
    "MA (1);",
    "WWAT (i + GBR, R0);",
  },
  { "", "0m", "mov.w R0,@(<disp>,<REG_M>)", "10000001mmmmi4*2",
    "MA (1);",
    "WWAT (i + R[m], R0);",
  },
  { "", "m0n", "mov.w <REG_M>,@(R0,<REG_N>)", "0000nnnnmmmm0101",
    "MA (1);",
    "WWAT (R0 + R[n], R[m]);",
  },
  { "n", "mn", "mov.w <REG_M>,@-<REG_N>", "0010nnnnmmmm0101",
    /* Allow for the case where m == n.  */
    "int t = R[m];",
    "MA (1);",
    "R[n] -= 2;",
    "WWAT (R[n], t);",
  },
  { "n", "0n", "mov.w R0,@<REG_N>+", "0100nnnn10011011",
    "MA (1);",
    "WWAT (R[n], R0);",
    "R[n] += 2;",
  },
  { "", "nm", "mov.w <REG_M>,@<REG_N>", "0010nnnnmmmm0001",
    "MA (1);",
    "WWAT (R[n], R[m]);",
  },

  { "0", "", "mova @(<disp>,PC),R0", "11000111i8p4....",
    "R0 = ((i + 4 + PH2T (PC)) & ~0x3);",
  },

  { "", "n0", "movca.l R0, @<REG_N>", "0000nnnn11000011",
    "/* We don't simulate cache, so this insn is identical to mov.  */",
    "MA (1);",
    "WLAT (R[n], R[0]);",
  },

  { "", "n0", "movco.l R0, @<REG_N>", "0000nnnn01110011", 
    "/* LDST -> T */",
    "SET_SR_T (LDST);",
    "/* if (T) R0 -> (Rn) */",
    "if (T)",
    "  WLAT (R[n], R[0]);",
    "/* 0 -> LDST */",
    "SET_LDST (0);",
  },

  { "0", "n", "movli.l @<REG_N>, R0", "0000nnnn01100011", 
    "/* 1 -> LDST */",
    "SET_LDST (1);",
    "/* (Rn) -> R0 */",
    "R[0] = RLAT (R[n]);",
    "/* if (interrupt/exception) 0 -> LDST */",
    "/* (we don't simulate asynchronous interrupts/exceptions) */",
  },

  { "n", "", "movt <REG_N>", "0000nnnn00101001",
    "R[n] = T;",
  },
  { "", "", "movrt <REG_N>", "0000nnnn00111001",
    "R[n] = (T == 0);",	
  },
  { "0", "n", "movua.l @<REG_N>,R0", "0100nnnn10101001",
    "int regn = R[n];",
    "int e = target_little_endian ? 3 : 0;",
    "MA (1);",
    "R[0] = (RBAT (regn + (0^e)) << 24) + (RBAT (regn + (1^e)) << 16) + ",
    "  (RBAT (regn + (2^e)) << 8) + RBAT (regn + (3^e));",
    "L (0);",
  },
  { "0n", "n", "movua.l @<REG_N>+,R0", "0100nnnn11101001",
    "int regn = R[n];",
    "int e = target_little_endian ? 3 : 0;",
    "MA (1);",
    "R[0] = (RBAT (regn + (0^e)) << 24) + (RBAT (regn + (1^e)) << 16) + ",
    "  (RBAT (regn + (2^e)) << 8) + RBAT (regn + (3^e));",
    "R[n] += 4;",
    "L (0);",
  },
  { "", "mn", "mul.l <REG_M>,<REG_N>", "0000nnnnmmmm0111",
    "MACL = ((int) R[n]) * ((int) R[m]);",
  },
#if 0  /* FIXME: The above cast to int is not really portable.
	  It should be replaced by a SEXT32 macro.  */
  { "", "nm", "mul.l <REG_M>,<REG_N>", "0000nnnnmmmm0111",
    "MACL = R[n] * R[m];",
  },
#endif

  /* muls.w - see muls */
  { "", "mn", "muls <REG_M>,<REG_N>", "0010nnnnmmmm1111",
    "MACL = ((int) (short) R[n]) * ((int) (short) R[m]);",
  },

  /* mulu.w - see mulu */
  { "", "mn", "mulu <REG_M>,<REG_N>", "0010nnnnmmmm1110",
    "MACL = (((unsigned int) (unsigned short) R[n])",
    "        * ((unsigned int) (unsigned short) R[m]));",
  },

  { "n", "m", "neg <REG_M>,<REG_N>", "0110nnnnmmmm1011",
    "R[n] = - R[m];",
  },

  { "n", "m", "negc <REG_M>,<REG_N>", "0110nnnnmmmm1010",
    "ult = -T;",
    "SET_SR_T (ult > 0);",
    "R[n] = ult - R[m];",
    "SET_SR_T (T || (R[n] > ult));",
  },

  { "", "", "nop", "0000000000001001",
    "/* nop */",
  },

  { "n", "m", "not <REG_M>,<REG_N>", "0110nnnnmmmm0111",
    "R[n] = ~R[m];",
  },

  /* sh4a */
  { "", "n", "icbi @<REG_N>", "0000nnnn11100011",
    "/* Except for the effect on the cache - which is not simulated -",
    "   this is like a nop.  */",
  },

  { "", "n", "ocbi @<REG_N>", "0000nnnn10010011",
    "RSBAT (R[n]); /* Take exceptions like byte load, otherwise noop.  */",
    "/* FIXME: Cache not implemented */",
  },

  { "", "n", "ocbp @<REG_N>", "0000nnnn10100011",
    "RSBAT (R[n]); /* Take exceptions like byte load, otherwise noop.  */",
    "/* FIXME: Cache not implemented */",
  },

  { "", "n", "ocbwb @<REG_N>", "0000nnnn10110011",
    "RSBAT (R[n]); /* Take exceptions like byte load, otherwise noop.  */",
    "/* FIXME: Cache not implemented */",
  },

  { "0", "", "or #<imm>,R0", "11001011i8*1....",
    "R0 |= i;",
  },
  { "n", "m", "or <REG_M>,<REG_N>", "0010nnnnmmmm1011",
    "R[n] |= R[m];",
  },
  { "", "0", "or.b #<imm>,@(R0,GBR)", "11001111i8*1....",
    "MA (1);",
    "WBAT (R0 + GBR, (RBAT (R0 + GBR) | i));",
  },

  { "", "n", "pref @<REG_N>", "0000nnnn10000011",
    "/* Except for the effect on the cache - which is not simulated -",
    "   this is like a nop.  */",
  },

  /* sh4a */
  { "", "n", "prefi @<REG_N>", "0000nnnn11010011",
    "/* Except for the effect on the cache - which is not simulated -",
    "   this is like a nop.  */",
  },

  /* sh4a */
  { "", "", "synco", "0000000010101011", 
    "/* Except for the effect on the pipeline - which is not simulated -", 
    "   this is like a nop.  */",
  },

  { "n", "n", "rotcl <REG_N>", "0100nnnn00100100",
    "ult = R[n] < 0;",
    "R[n] = (R[n] << 1) | T;",
    "SET_SR_T (ult);",
  },

  { "n", "n", "rotcr <REG_N>", "0100nnnn00100101",
    "ult = R[n] & 1;",
    "R[n] = (UR[n] >> 1) | (T << 31);",
    "SET_SR_T (ult);",
  },

  { "n", "n", "rotl <REG_N>", "0100nnnn00000100",
    "SET_SR_T (R[n] < 0);",
    "R[n] <<= 1;",
    "R[n] |= T;",
  },

  { "n", "n", "rotr <REG_N>", "0100nnnn00000101",
    "SET_SR_T (R[n] & 1);",
    "R[n] = UR[n] >> 1;",
    "R[n] |= (T << 31);",
  },

  { "", "", "rte", "0000000000101011", 
#if 0
    /* SH-[12] */
    "int tmp = PC;",
    "SET_NIP (PT2H (RLAT (R[15]) + 2));",
    "R[15] += 4;",
    "SET_SR (RLAT (R[15]) & 0x3f3);",
    "R[15] += 4;",
    "Delay_Slot (PC + 2);",
#else
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "SET_SR (SSR);",
    "SET_NIP (PT2H (SPC));",
    "cycles += 2;",
    "Delay_Slot (PC + 2);",
#endif
  },

  { "", "", "rts", "0000000000001011",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "SET_NIP (PT2H (PR));",
    "cycles += 2;",
    "Delay_Slot (PC + 2);",
  },
  { "", "", "rts/n", "0000000001101011",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "SET_NIP (PT2H (PR));",
  },
  { "0", "n", "rtv/n <REG_N>", "0000nnnn01111011",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "R0 = R[n];",
    "L (0);",
    "SET_NIP (PT2H (PR));",
  },

  /* sh4a */
  { "", "", "setdmx", "0000000010011000",
    "saved_state.asregs.cregs.named.sr |=  SR_MASK_DMX;"
    "saved_state.asregs.cregs.named.sr &= ~SR_MASK_DMY;"
  },

  /* sh4a */
  { "", "", "setdmy", "0000000011001000",
    "saved_state.asregs.cregs.named.sr |=  SR_MASK_DMY;"
    "saved_state.asregs.cregs.named.sr &= ~SR_MASK_DMX;"
  },

  /* sh-dsp */
  { "", "n", "setrc <REG_N>", "0100nnnn00010100",
    "SET_RC (R[n]);",
  },
  { "", "", "setrc #<imm>", "10000010i8*1....",
    /* It would be more realistic to let loop_start point to some static
       memory that contains an illegal opcode and then give a bus error when
       the loop is eventually encountered, but it seems not only simpler,
       but also more debugging-friendly to just catch the failure here.  */
    "if (BUSERROR (RS | RE, maskw))",
    "  RAISE_EXCEPTION (SIGILL);",
    "else {",
    "  SET_RC (i);",
    "  loop = get_loop_bounds (RS, RE, memory, mem_end, maskw, endianw);",
    "  CHECK_INSN_PTR (insn_ptr);",
    "}",
  },

  { "", "", "sets", "0000000001011000",
    "SET_SR_S (1);",
  },

  { "", "", "sett", "0000000000011000",
    "SET_SR_T (1);",
  },

  { "n", "mn", "shad <REG_M>,<REG_N>", "0100nnnnmmmm1100",
    "R[n] = (R[m] < 0) ? (R[m]&0x1f ? R[n] >> ((-R[m])&0x1f) : R[n] >> 31) : (R[n] << (R[m] & 0x1f));",
  },

  { "n", "n", "shal <REG_N>", "0100nnnn00100000",
    "SET_SR_T (R[n] < 0);",
    "R[n] <<= 1;",
  },

  { "n", "n", "shar <REG_N>", "0100nnnn00100001",
    "SET_SR_T (R[n] & 1);",
    "R[n] = R[n] >> 1;",
  },

  { "n", "mn", "shld <REG_M>,<REG_N>", "0100nnnnmmmm1101",
    "R[n] = (R[m] < 0) ? (R[m]&0x1f ? UR[n] >> ((-R[m])&0x1f) : 0): (R[n] << (R[m] & 0x1f));",
  },

  { "n", "n", "shll <REG_N>", "0100nnnn00000000",
    "SET_SR_T (R[n] < 0);",
    "R[n] <<= 1;",
  },

  { "n", "n", "shll2 <REG_N>", "0100nnnn00001000",
    "R[n] <<= 2;",
  },
  { "n", "n", "shll8 <REG_N>", "0100nnnn00011000",
    "R[n] <<= 8;",
  },
  { "n", "n", "shll16 <REG_N>", "0100nnnn00101000",
    "R[n] <<= 16;",
  },

  { "n", "n", "shlr <REG_N>", "0100nnnn00000001",
    "SET_SR_T (R[n] & 1);",
    "R[n] = UR[n] >> 1;",
  },

  { "n", "n", "shlr2 <REG_N>", "0100nnnn00001001",
    "R[n] = UR[n] >> 2;",
  },
  { "n", "n", "shlr8 <REG_N>", "0100nnnn00011001",
    "R[n] = UR[n] >> 8;",
  },
  { "n", "n", "shlr16 <REG_N>", "0100nnnn00101001",
    "R[n] = UR[n] >> 16;",
  },

  { "", "", "sleep", "0000000000011011",
    "nip += trap (0xc3, &R0, PC, memory, maskl, maskw, endianw);",
  },

  { "n", "", "stc <CREG_M>,<REG_N>", "0000nnnnmmmm0010",
    "R[n] = CREG (m);",
  },

  { "n", "", "stc SGR,<REG_N>", "0000nnnn00111010",
    "if (SR_MD)",
    "  R[n] = SGR; /* priv mode */",
    "else",
    "  RAISE_EXCEPTION (SIGILL); /* user mode */",
  },
  { "n", "", "stc DBR,<REG_N>", "0000nnnn11111010",
    "if (SR_MD)",
    "  R[n] = DBR; /* priv mode */",
    "else",
    "  RAISE_EXCEPTION (SIGILL); /* user mode */",
  },
  { "n", "", "stc TBR,<REG_N>", "0000nnnn01001010",
    "if (SR_MD)",	/* FIXME? */
    "  R[n] = TBR; /* priv mode */",
    "else",
    "  RAISE_EXCEPTION (SIGILL); /* user mode */",
  },
  { "n", "n", "stc.l <CREG_M>,@-<REG_N>", "0100nnnnmmmm0011",
    "MA (1);",
    "R[n] -= 4;",
    "WLAT (R[n], CREG (m));",
  },
  { "n", "n", "stc.l SGR,@-<REG_N>", "0100nnnn00110010",
    "if (SR_MD)",
    "{ /* priv mode */",
    "  MA (1);",
    "  R[n] -= 4;",
    "  WLAT (R[n], SGR);",
    "}",
    "else",
    "  RAISE_EXCEPTION (SIGILL); /* user mode */",
  },
  { "n", "n", "stc.l DBR,@-<REG_N>", "0100nnnn11110010",
    "if (SR_MD)",
    "{ /* priv mode */",
    "  MA (1);",
    "  R[n] -= 4;",
    "  WLAT (R[n], DBR);",
    "}",
    "else",
    "  RAISE_EXCEPTION (SIGILL); /* user mode */",
  },

  { "n", "", "sts <SREG_M>,<REG_N>", "0000nnnnssss1010",
    "R[n] = SREG (m);",
  },
  { "n", "n", "sts.l <SREG_M>,@-<REG_N>", "0100nnnnssss0010",
    "MA (1);",
    "R[n] -= 4;",
    "WLAT (R[n], SREG (m));",
  },

  { "n", "nm", "sub <REG_M>,<REG_N>", "0011nnnnmmmm1000",
    "R[n] -= R[m];",
  },

  { "n", "nm", "subc <REG_M>,<REG_N>", "0011nnnnmmmm1010",
    "ult = R[n] - T;",
    "SET_SR_T (ult > R[n]);",
    "R[n] = ult - R[m];",
    "SET_SR_T (T || (R[n] > ult));",
  },

  { "n", "nm", "subv <REG_M>,<REG_N>", "0011nnnnmmmm1011",
    "ult = R[n] - R[m];",
    "SET_SR_T (((R[n] ^ R[m]) & (ult ^ R[n])) >> 31);",
    "R[n] = ult;",
  },

  { "n", "nm", "swap.b <REG_M>,<REG_N>", "0110nnnnmmmm1000",
    "R[n] = ((R[m] & 0xffff0000)",
    "        | ((R[m] << 8) & 0xff00)",
    "        | ((R[m] >> 8) & 0x00ff));",
  },
  { "n", "nm", "swap.w <REG_M>,<REG_N>", "0110nnnnmmmm1001",
    "R[n] = (((R[m] << 16) & 0xffff0000)",
    "        | ((R[m] >> 16) & 0x00ffff));",
  },

  { "", "n", "tas.b @<REG_N>", "0100nnnn00011011",
    "MA (1);",
    "ult = RBAT (R[n]);",
    "SET_SR_T (ult == 0);",
    "WBAT (R[n],ult|0x80);",
  },

  { "0", "", "trapa #<imm>", "11000011i8*1....", 
    "long imm = 0xff & i;",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "if (i < 20 || i == 33 || i == 34 || i == 0xc3)",
    "  nip += trap (i, &R0, PC, memory, maskl, maskw, endianw);",
#if 0
    "else {",
    /* SH-[12] */
    "  R[15] -= 4;",
    "  WLAT (R[15], GET_SR ());",
    "  R[15] -= 4;",
    "  WLAT (R[15], PH2T (PC + 2));",
#else
    "else if (!SR_BL) {",
    "  SSR = GET_SR ();",
    "  SPC = PH2T (PC + 2);",
    "  SET_SR (GET_SR () | SR_MASK_MD | SR_MASK_BL | SR_MASK_RB);",
    "  /* FIXME: EXPEVT = 0x00000160; */",
#endif
    "  SET_NIP (PT2H (RLAT (VBR + (imm<<2))));",
    "}",
  },

  { "", "mn", "tst <REG_M>,<REG_N>", "0010nnnnmmmm1000",
    "SET_SR_T ((R[n] & R[m]) == 0);",
  },
  { "", "0", "tst #<imm>,R0", "11001000i8*1....",
    "SET_SR_T ((R0 & i) == 0);",
  },
  { "", "0", "tst.b #<imm>,@(R0,GBR)", "11001100i8*1....",
    "MA (1);",
    "SET_SR_T ((RBAT (GBR+R0) & i) == 0);",
  },

  { "", "0", "xor #<imm>,R0", "11001010i8*1....",
    "R0 ^= i;",
  },
  { "n", "mn", "xor <REG_M>,<REG_N>", "0010nnnnmmmm1010",
    "R[n] ^= R[m];",
  },
  { "", "0", "xor.b #<imm>,@(R0,GBR)", "11001110i8*1....",
    "MA (1);",
    "ult = RBAT (GBR+R0);",
    "ult ^= i;",
    "WBAT (GBR + R0, ult);",
  },

  { "n", "nm", "xtrct <REG_M>,<REG_N>", "0010nnnnmmmm1101",
    "R[n] = (((R[n] >> 16) & 0xffff)",
    "        | ((R[m] << 16) & 0xffff0000));",
  },

#if 0
  { "divs.l <REG_M>,<REG_N>", "0100nnnnmmmm1110",
    "divl (0, R[n], R[m]);",
  },
  { "divu.l <REG_M>,<REG_N>", "0100nnnnmmmm1101",
    "divl (0, R[n], R[m]);",
  },
#endif

  {0, 0}};

op movsxy_tab[] =
{
/* If this is disabled, the simulator speeds up by about 12% on a
   450 MHz PIII - 9% with ACE_FAST.
   Maybe we should have separate simulator loops?  */
#if 1
  { "n", "n", "movs.w @-<REG_N>,<DSP_REG_M>", "111101NNMMMM0000",
    "MA (1);",
    "R[n] -= 2;",
    "DSP_R (m) = RSWAT (R[n]) << 16;",
    "DSP_GRD (m) = SIGN32 (DSP_R (m));",
  },
  { "", "n",  "movs.w @<REG_N>,<DSP_REG_M>",  "111101NNMMMM0100",
    "MA (1);",
    "DSP_R (m) = RSWAT (R[n]) << 16;",
    "DSP_GRD (m) = SIGN32 (DSP_R (m));",
  },
  { "n", "n", "movs.w @<REG_N>+,<DSP_REG_M>", "111101NNMMMM1000",
    "MA (1);",
    "DSP_R (m) = RSWAT (R[n]) << 16;",
    "DSP_GRD (m) = SIGN32 (DSP_R (m));",
    "R[n] += 2;",
  },
  { "n", "n8","movs.w @<REG_N>+REG_8,<DSP_REG_M>", "111101NNMMMM1100",
    "MA (1);",
    "DSP_R (m) = RSWAT (R[n]) << 16;",
    "DSP_GRD (m) = SIGN32 (DSP_R (m));",
    "R[n] += R[8];",
  },
  { "n", "n", "movs.w @-<REG_N>,<DSP_GRD_M>", "111101NNGGGG0000",
    "MA (1);",
    "R[n] -= 2;",
    "DSP_R (m) = RSWAT (R[n]);",
  },
  { "", "n",  "movs.w @<REG_N>,<DSP_GRD_M>",  "111101NNGGGG0100",
    "MA (1);",
    "DSP_R (m) = RSWAT (R[n]);",
  },
  { "n", "n", "movs.w @<REG_N>+,<DSP_GRD_M>", "111101NNGGGG1000",
    "MA (1);",
    "DSP_R (m) = RSWAT (R[n]);",
    "R[n] += 2;",
  },
  { "n", "n8","movs.w @<REG_N>+REG_8,<DSP_GRD_M>", "111101NNGGGG1100",
    "MA (1);",
    "DSP_R (m) = RSWAT (R[n]);",
    "R[n] += R[8];",
  },
  { "n", "n", "movs.w <DSP_REG_M>,@-<REG_N>", "111101NNMMMM0001",
    "MA (1);",
    "R[n] -= 2;",
    "WWAT (R[n], DSP_R (m) >> 16);",
  },
  { "", "n",  "movs.w <DSP_REG_M>,@<REG_N>",  "111101NNMMMM0101",
    "MA (1);",
    "WWAT (R[n], DSP_R (m) >> 16);",
  },
  { "n", "n", "movs.w <DSP_REG_M>,@<REG_N>+", "111101NNMMMM1001",
    "MA (1);",
    "WWAT (R[n], DSP_R (m) >> 16);",
    "R[n] += 2;",
  },
  { "n", "n8","movs.w <DSP_REG_M>,@<REG_N>+REG_8", "111101NNMMMM1101",
    "MA (1);",
    "WWAT (R[n], DSP_R (m) >> 16);",
    "R[n] += R[8];",
  },
  { "n", "n", "movs.w <DSP_GRD_M>,@-<REG_N>", "111101NNGGGG0001",
    "MA (1);",
    "R[n] -= 2;",
    "WWAT (R[n], SEXT (DSP_R (m)));",
  },
  { "", "n",  "movs.w <DSP_GRD_M>,@<REG_N>",  "111101NNGGGG0101",
    "MA (1);",
    "WWAT (R[n], SEXT (DSP_R (m)));",
  },
  { "n", "n", "movs.w <DSP_GRD_M>,@<REG_N>+", "111101NNGGGG1001",
    "MA (1);",
    "WWAT (R[n], SEXT (DSP_R (m)));",
    "R[n] += 2;",
  },
  { "n", "n8","movs.w <DSP_GRD_M>,@<REG_N>+REG_8", "111101NNGGGG1101",
    "MA (1);",
    "WWAT (R[n], SEXT (DSP_R (m)));",
    "R[n] += R[8];",
  },
  { "n", "n", "movs.l @-<REG_N>,<DSP_REG_M>", "111101NNMMMM0010",
    "MA (1);",
    "R[n] -= 4;",
    "DSP_R (m) = RLAT (R[n]);",
    "DSP_GRD (m) = SIGN32 (DSP_R (m));",
  },
  { "", "n",  "movs.l @<REG_N>,<DSP_REG_M>",  "111101NNMMMM0110",
    "MA (1);",
    "DSP_R (m) = RLAT (R[n]);",
    "DSP_GRD (m) = SIGN32 (DSP_R (m));",
  },
  { "n", "n", "movs.l @<REG_N>+,<DSP_REG_M>", "111101NNMMMM1010",
    "MA (1);",
    "DSP_R (m) = RLAT (R[n]);",
    "DSP_GRD (m) = SIGN32 (DSP_R (m));",
    "R[n] += 4;",
  },
  { "n", "n8","movs.l @<REG_N>+REG_8,<DSP_REG_M>", "111101NNMMMM1110",
    "MA (1);",
    "DSP_R (m) = RLAT (R[n]);",
    "DSP_GRD (m) = SIGN32 (DSP_R (m));",
    "R[n] += R[8];",
  },
  { "n", "n", "movs.l <DSP_REG_M>,@-<REG_N>", "111101NNMMMM0011",
    "MA (1);",
    "R[n] -= 4;",
    "WLAT (R[n], DSP_R (m));",
  },
  { "", "n",  "movs.l <DSP_REG_M>,@<REG_N>",  "111101NNMMMM0111",
    "MA (1);",
    "WLAT (R[n], DSP_R (m));",
  },
  { "n", "n", "movs.l <DSP_REG_M>,@<REG_N>+", "111101NNMMMM1011",
    "MA (1);",
    "WLAT (R[n], DSP_R (m));",
    "R[n] += 4;",
  },
  { "n", "n8","movs.l <DSP_REG_M>,@<REG_N>+REG_8", "111101NNMMMM1111",
    "MA (1);",
    "WLAT (R[n], DSP_R (m));",
    "R[n] += R[8];",
  },
  { "n", "n", "movs.l <DSP_GRD_M>,@-<REG_N>", "111101NNGGGG0011",
    "MA (1);",
    "R[n] -= 4;",
    "WLAT (R[n], SEXT (DSP_R (m)));",
  },
  { "", "n",  "movs.l <DSP_GRD_M>,@<REG_N>",  "111101NNGGGG0111",
    "MA (1);",
    "WLAT (R[n], SEXT (DSP_R (m)));",
  },
  { "n", "n", "movs.l <DSP_GRD_M>,@<REG_N>+", "111101NNGGGG1011",
    "MA (1);",
    "WLAT (R[n], SEXT (DSP_R (m)));",
    "R[n] += 4;",
  },
  { "n", "n8","movs.l <DSP_GRD_M>,@<REG_N>+REG_8", "111101NNGGGG1111",
    "MA (1);",
    "WLAT (R[n], SEXT (DSP_R (m)));",
    "R[n] += R[8];",
  },
  { "", "n", "movx.w @<REG_xy>,<DSP_XY>",   "111100xyXY0001??",
    "DSP_R (m) = RSWAT (R[n]) << 16;",
    "if (iword & 3)",
    "  {",
    "    iword &= 0xfd53; goto top;",
    "  }",
  },
  { "", "n", "movx.l @<REG_xy>,<DSP_XY>",   "111100xyXY010100",
    "DSP_R (m) = RLAT (R[n]);",
  },
  { "n", "n", "movx.w @<REG_xy>+,<DSP_XY>", "111100xyXY0010??",
    "DSP_R (m) = RSWAT (R[n]) << 16;",
    "R[n] += ((R[n] & 0xffff) == MOD_ME) ? MOD_DELTA : 2;",
    "if (iword & 3)",
    "  {",
    "    iword &= 0xfd53; goto top;",
    "  }",
  },
  { "n", "n", "movx.l @<REG_xy>+,<DSP_XY>", "111100xyXY011000",
    "DSP_R (m) = RLAT (R[n]);",
    "R[n] += ((R[n] & 0xffff) == MOD_ME) ? MOD_DELTA : 4;",
  },
  { "n", "n8","movx.w @<REG_xy>+REG_8,<DSP_XY>", "111100xyXY0011??",
    "DSP_R (m) = RSWAT (R[n]) << 16;",
    "R[n] += ((R[n] & 0xffff) == MOD_ME) ? MOD_DELTA : R[8];",
    "if (iword & 3)",
    "  {",
    "    iword &= 0xfd53; goto top;",
    "  }",
  },
  { "n", "n8","movx.l @<REG_xy>+REG_8,<DSP_XY>", "111100xyXY011100",
    "DSP_R (m) = RLAT (R[n]);",
    "R[n] += ((R[n] & 0xffff) == MOD_ME) ? MOD_DELTA : R[8];",
  },
  { "", "n", "movx.w <DSP_Ax>,@<REG_xy>",   "111100xyax1001??",
    "WWAT (R[n], DSP_R (m) >> 16);",
    "if (iword & 3)",
    "  {",
    "    iword &= 0xfd53; goto top;",
    "  }",
  },
  { "", "n", "movx.l <DSP_Ax>,@<REG_xy>",   "111100xyax110100",
    "WLAT (R[n], DSP_R (m));",
  },
  { "n", "n", "movx.w <DSP_Ax>,@<REG_xy>+", "111100xyax1010??",
    "WWAT (R[n], DSP_R (m) >> 16);",
    "R[n] += ((R[n] & 0xffff) == MOD_ME) ? MOD_DELTA : 2;",
    "if (iword & 3)",
    "  {",
    "    iword &= 0xfd53; goto top;",
    "  }",
  },
  { "n", "n", "movx.l <DSP_Ax>,@<REG_xy>+", "111100xyax111000",
    "WLAT (R[n], DSP_R (m));",
    "R[n] += ((R[n] & 0xffff) == MOD_ME) ? MOD_DELTA : 4;",
  },
  { "n", "n8","movx.w <DSP_Ax>,@<REG_xy>+REG_8","111100xyax1011??",
    "WWAT (R[n], DSP_R (m) >> 16);",
    "R[n] += ((R[n] & 0xffff) == MOD_ME) ? MOD_DELTA : R[8];",
    "if (iword & 3)",
    "  {",
    "    iword &= 0xfd53; goto top;",
    "  }",
  },
  { "n", "n8","movx.l <DSP_Ax>,@<REG_xy>+REG_8","111100xyax111100",
    "WLAT (R[n], DSP_R (m));",
    "R[n] += ((R[n] & 0xffff) == MOD_ME) ? MOD_DELTA : R[8];",
  },
  { "", "n", "movy.w @<REG_yx>,<DSP_YX>",   "111100yxYX000001",
    "DSP_R (m) = RSWAT (R[n]) << 16;",
  },
  { "n", "n", "movy.w @<REG_yx>+,<DSP_YX>", "111100yxYX000010",
    "DSP_R (m) = RSWAT (R[n]) << 16;",
    "R[n] += ((R[n] | ~0xffff) == MOD_ME) ? MOD_DELTA : 2;",
  },
  { "n", "n9","movy.w @<REG_yx>+REG_9,<DSP_YX>", "111100yxYX000011",
    "DSP_R (m) = RSWAT (R[n]) << 16;",
    "R[n] += ((R[n] | ~0xffff) == MOD_ME) ? MOD_DELTA : R[9];",
  },
  { "", "n", "movy.w <DSP_Ay>,@<REG_yx>",   "111100yxAY010001",
    "WWAT (R[n], DSP_R (m) >> 16);",
  },
  { "n", "n", "movy.w <DSP_Ay>,@<REG_yx>+", "111100yxAY010010",
    "WWAT (R[n], DSP_R (m) >> 16);",
    "R[n] += ((R[n] | ~0xffff) == MOD_ME) ? MOD_DELTA : 2;",
  },
  { "n", "n9", "movy.w <DSP_Ay>,@<REG_yx>+REG_9", "111100yxAY010011",
    "WWAT (R[n], DSP_R (m) >> 16);",
    "R[n] += ((R[n] | ~0xffff) == MOD_ME) ? MOD_DELTA : R[9];",
  },
  { "", "n", "movy.l @<REG_yx>,<DSP_YX>",   "111100yxYX100001",
    "DSP_R (m) = RLAT (R[n]);",
  },
  { "n", "n", "movy.l @<REG_yx>+,<DSP_YX>", "111100yxYX100010",
    "DSP_R (m) = RLAT (R[n]);",
    "R[n] += ((R[n] | ~0xffff) == MOD_ME) ? MOD_DELTA : 4;",
  },
  { "n", "n9","movy.l @<REG_yx>+REG_9,<DSP_YX>", "111100yxYX100011",
    "DSP_R (m) = RLAT (R[n]);",
    "R[n] += ((R[n] | ~0xffff) == MOD_ME) ? MOD_DELTA : R[9];",
  },
  { "", "n", "movy.l <DSP_Ay>,@<REG_yx>",   "111100yxAY110001",
    "WLAT (R[n], DSP_R (m));",
  },
  { "n", "n", "movy.l <DSP_Ay>,@<REG_yx>+", "111100yxAY110010",
    "WLAT (R[n], DSP_R (m));",
    "R[n] += ((R[n] | ~0xffff) == MOD_ME) ? MOD_DELTA : 4;",
  },
  { "n", "n9", "movy.l <DSP_Ay>,@<REG_yx>+REG_9", "111100yxAY110011",
    "WLAT (R[n], DSP_R (m));",
    "R[n] += ((R[n] | ~0xffff) == MOD_ME) ? MOD_DELTA : R[9];",
  },
  { "", "", "nopx nopy", "1111000000000000",
    "/* nop */",
  },
  { "", "", "ppi", "1111100000000000",
    "RAISE_EXCEPTION_IF_IN_DELAY_SLOT ();",
    "ppi_insn (RIAT (nip));",
    "SET_NIP (nip + 2);",
    "iword &= 0xf7ff; goto top;",
  },
#endif
  {0, 0}};

op ppi_tab[] =
{
  { "","", "pshl #<imm>,dz",	"00000iiim16.zzzz",
    "int Sz = DSP_R (z) & 0xffff0000;",
    "",
    "if (i <= 16)",
    "  res = Sz << i;",
    "else if (i >= 128 - 16)",
    "  res = (unsigned) Sz >> 128 - i;	/* no sign extension */",
    "else",
    "  {",
    "    RAISE_EXCEPTION (SIGILL);",
    "    return;",
    "  }",
    "res &= 0xffff0000;",
    "res_grd = 0;",
    "goto logical;",
  },
  { "","", "psha #<imm>,dz",	"00010iiim32.zzzz",
    "int Sz = DSP_R (z);",
    "int Sz_grd = GET_DSP_GRD (z);",
    "",
    "if (i <= 32)",
    "  {",
    "    if (i == 32)",
    "      {",
    "        res = 0;",
    "        res_grd = Sz;",
    "      }",
    "    else",
    "      {",
    "        res = Sz << i;",
    "        res_grd = Sz_grd << i | (unsigned) Sz >> 32 - i;",
    "      }",
    "    res_grd = SEXT (res_grd);",
    "    carry = res_grd & 1;",
    "  }",
    "else if (i >= 96)",
    "  {",
    "    i = 128 - i;",
    "    if (i == 32)",
    "      {",
    "        res_grd = SIGN32 (Sz_grd);",
    "        res = Sz_grd;",
    "      }",
    "    else",
    "      {",
    "        res = Sz >> i | Sz_grd << 32 - i;",
    "        res_grd = Sz_grd >> i;",
    "      }",
    "    carry = Sz >> (i - 1) & 1;",
    "  }",
    "else",
    "  {",
    "    RAISE_EXCEPTION (SIGILL);",
    "    return;",
    "  }",
    "COMPUTE_OVERFLOW;",
    "greater_equal = 0;",
  },
  { "","", "pmuls Se,Sf,Dg",	"0100eeffxxyygguu",
    "res = (DSP_R (e) >> 16) * (DSP_R (f) >> 16) * 2;",
    "if (res == 0x80000000)",
    "  res = 0x7fffffff;",
    "DSP_R (g) = res;",
    "DSP_GRD (g) = SIGN32 (res);",
    "return;",
  },
  { "","", "psub Sx,Sy,Du pmuls Se,Sf,Dg",	"0110eeffxxyygguu",
    "int Sx = DSP_R (x);",
    "int Sx_grd = GET_DSP_GRD (x);",
    "int Sy = DSP_R (y);",
    "int Sy_grd = SIGN32 (Sy);",
    "",
    "res = (DSP_R (e) >> 16) * (DSP_R (f) >> 16) * 2;",
    "if (res == 0x80000000)",
    "  res = 0x7fffffff;",
    "DSP_R (g) = res;",
    "DSP_GRD (g) = SIGN32 (res);",
    "",
    "z = u;",
    "res = Sx - Sy;",
    "carry = (unsigned) res > (unsigned) Sx;",
    "res_grd = Sx_grd - Sy_grd - carry;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
  },
  { "","", "padd Sx,Sy,Du pmuls Se,Sf,Dg",	"0111eeffxxyygguu",
    "int Sx = DSP_R (x);",
    "int Sx_grd = GET_DSP_GRD (x);",
    "int Sy = DSP_R (y);",
    "int Sy_grd = SIGN32 (Sy);",
    "",
    "res = (DSP_R (e) >> 16) * (DSP_R (f) >> 16) * 2;",
    "if (res == 0x80000000)",
    "  res = 0x7fffffff;",
    "DSP_R (g) = res;",
    "DSP_GRD (g) = SIGN32 (res);",
    "",
    "z = u;",
    "res = Sx + Sy;",
    "carry = (unsigned) res < (unsigned) Sx;",
    "res_grd = Sx_grd + Sy_grd + carry;",
    "COMPUTE_OVERFLOW;",
  },
  { "","", "psubc Sx,Sy,Dz",		"10100000xxyyzzzz",
    "int Sx = DSP_R (x);",
    "int Sx_grd = GET_DSP_GRD (x);",
    "int Sy = DSP_R (y);",
    "int Sy_grd = SIGN32 (Sy);",
    "",
    "res = Sx - Sy - (DSR & 1);",
    "carry = (unsigned) res > (unsigned) Sx || (res == Sx && Sy);",
    "res_grd = Sx_grd + Sy_grd + carry;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
    "DSR &= ~0xf1;\n",
    "if (res || res_grd)\n",
    "  DSR |= greater_equal | res_grd >> 2 & DSR_MASK_N | overflow;\n",
    "else\n",
    "  DSR |= DSR_MASK_Z | overflow;\n",
    "DSR |= carry;\n",
    "goto assign_z;\n",
  },
  { "","", "paddc Sx,Sy,Dz",	"10110000xxyyzzzz",
    "int Sx = DSP_R (x);",
    "int Sx_grd = GET_DSP_GRD (x);",
    "int Sy = DSP_R (y);",
    "int Sy_grd = SIGN32 (Sy);",
    "",
    "res = Sx + Sy + (DSR & 1);",
    "carry = (unsigned) res < (unsigned) Sx || (res == Sx && Sy);",
    "res_grd = Sx_grd + Sy_grd + carry;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
    "DSR &= ~0xf1;\n",
    "if (res || res_grd)\n",
    "  DSR |= greater_equal | res_grd >> 2 & DSR_MASK_N | overflow;\n",
    "else\n",
    "  DSR |= DSR_MASK_Z | overflow;\n",
    "DSR |= carry;\n",
    "goto assign_z;\n",
  },
  { "","", "pcmp Sx,Sy",	"10000100xxyyzzzz",
    "int Sx = DSP_R (x);",
    "int Sx_grd = GET_DSP_GRD (x);",
    "int Sy = DSP_R (y);",
    "int Sy_grd = SIGN32 (Sy);",
    "",
    "z = 17; /* Ignore result.  */",
    "res = Sx - Sy;",
    "carry = (unsigned) res > (unsigned) Sx;",
    "res_grd = Sx_grd - Sy_grd - carry;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
  },
  { "","", "pwsb Sx,Sy,Dz",	"10100100xxyyzzzz",
  },
  { "","", "pwad Sx,Sy,Dz",	"10110100xxyyzzzz",
  },
  { "","", "(if cc) pabs Sx,Dz",	"100010ccxx01zzzz",
    "/* FIXME: duplicate code pabs.  */",
    "res = DSP_R (x);",
    "res_grd = GET_DSP_GRD (x);",
    "if (res >= 0)",
    "  carry = 0;",
    "else",
    "  {",
    "    res = -res;",
    "    carry = (res != 0); /* The manual has a bug here.  */", 
    "    res_grd = -res_grd - carry;", 
    "  }",
    "COMPUTE_OVERFLOW;",
    "/* ??? The re-computing of overflow after",
    "   saturation processing is specific to pabs.  */",
    "overflow = res_grd != SIGN32 (res) ? DSR_MASK_V : 0;",
    "ADD_SUB_GE;",
  },
  { "","", "pabs Sx,Dz",	"10001000xx..zzzz",
    "res = DSP_R (x);",
    "res_grd = GET_DSP_GRD (x);",
    "if (res >= 0)",
    "  carry = 0;",
    "else",
    "  {",
    "    res = -res;",
    "    carry = (res != 0); /* The manual has a bug here.  */", 
    "    res_grd = -res_grd - carry;", 
    "  }",
    "COMPUTE_OVERFLOW;",
    "/* ??? The re-computing of overflow after",
    "   saturation processing is specific to pabs.  */",
    "overflow = res_grd != SIGN32 (res) ? DSR_MASK_V : 0;",
    "ADD_SUB_GE;",
  },

  { "","", "(if cc) prnd Sx,Dz",	"100110ccxx01zzzz",
    "/* FIXME: duplicate code prnd.  */",
    "int Sx = DSP_R (x);",
    "int Sx_grd = GET_DSP_GRD (x);",
    "",
    "res = (Sx + 0x8000) & 0xffff0000;",
    "carry = (unsigned) res < (unsigned) Sx;",
    "res_grd = Sx_grd + carry;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
  },
  { "","", "prnd Sx,Dz",	"10011000xx..zzzz",
    "int Sx = DSP_R (x);",
    "int Sx_grd = GET_DSP_GRD (x);",
    "",
    "res = (Sx + 0x8000) & 0xffff0000;",
    "carry = (unsigned) res < (unsigned) Sx;",
    "res_grd = Sx_grd + carry;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
  },

  { "","", "(if cc) pabs Sy,Dz",	"101010cc01yyzzzz",
    "/* FIXME: duplicate code pabs.  */",
    "res = DSP_R (y);",
    "res_grd = 0;",
    "overflow = 0;",
    "greater_equal = DSR_MASK_G;",
    "if (res >= 0)",
    "  carry = 0;",
    "else",
    "  {",
    "    res = -res;",
    "    carry = 1;",
    "    if (res < 0)",
    "      {",
    "        if (S)",
    "          res = 0x7fffffff;",
    "        else",
    "          {",
    "            overflow = DSR_MASK_V;",
    "            greater_equal = 0;",
    "          }",
    "      }",
    "  }",
  },
  { "","", "pabs Sy,Dz",	"10101000..yyzzzz",
    "res = DSP_R (y);",
    "res_grd = 0;",
    "overflow = 0;",
    "greater_equal = DSR_MASK_G;",
    "if (res >= 0)",
    "  carry = 0;",
    "else",
    "  {",
    "    res = -res;",
    "    carry = 1;",
    "    if (res < 0)",
    "      {",
    "        if (S)",
    "          res = 0x7fffffff;",
    "        else",
    "          {",
    "            overflow = DSR_MASK_V;",
    "            greater_equal = 0;",
    "          }",
    "      }",
    "  }",
  },
  { "","", "(if cc) prnd Sy,Dz",	"101110cc01yyzzzz",
    "/* FIXME: duplicate code prnd.  */",
    "int Sy = DSP_R (y);",
    "int Sy_grd = SIGN32 (Sy);",
    "",
    "res = (Sy + 0x8000) & 0xffff0000;",
    "carry = (unsigned) res < (unsigned) Sy;",
    "res_grd = Sy_grd + carry;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
  },
  { "","", "prnd Sy,Dz",	"10111000..yyzzzz",
    "int Sy = DSP_R (y);",
    "int Sy_grd = SIGN32 (Sy);",
    "",
    "res = (Sy + 0x8000) & 0xffff0000;",
    "carry = (unsigned) res < (unsigned) Sy;",
    "res_grd = Sy_grd + carry;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
  },
  { "","", "(if cc) pshl Sx,Sy,Dz",	"100000ccxxyyzzzz",
    "int Sx = DSP_R (x) & 0xffff0000;",
    "int Sy = DSP_R (y) >> 16 & 0x7f;",
    "",
    "if (Sy <= 16)",
    "  res = Sx << Sy;",
    "else if (Sy >= 128 - 16)",
    "  res = (unsigned) Sx >> 128 - Sy;	/* no sign extension */",
    "else",
    "  {",
    "    RAISE_EXCEPTION (SIGILL);",
    "    return;",
    "  }",
    "goto cond_logical;",
  },
  { "","", "(if cc) psha Sx,Sy,Dz",	"100100ccxxyyzzzz",
    "int Sx = DSP_R (x);",
    "int Sx_grd = GET_DSP_GRD (x);",
    "int Sy = DSP_R (y) >> 16 & 0x7f;",
    "",
    "if (Sy <= 32)",
    "  {",
    "    if (Sy == 32)",
    "      {",
    "        res = 0;",
    "        res_grd = Sx;",
    "      }",
    "    else",
    "      {",
    "        res = Sx << Sy;",
    "        res_grd = Sx_grd << Sy | (unsigned) Sx >> 32 - Sy;",
    "      }",
    "    res_grd = SEXT (res_grd);",
    "    carry = res_grd & 1;",
    "  }",
    "else if (Sy >= 96)",
    "  {",
    "    Sy = 128 - Sy;",
    "    if (Sy == 32)",
    "      {",
    "        res_grd = SIGN32 (Sx_grd);",
    "        res = Sx_grd;",
    "      }",
    "    else",
    "      {",
    "        res = Sx >> Sy | Sx_grd << 32 - Sy;",
    "        res_grd = Sx_grd >> Sy;",
    "      }",
    "    carry = Sx >> (Sy - 1) & 1;",
    "  }",
    "else",
    "  {",
    "    RAISE_EXCEPTION (SIGILL);",
    "    return;",
    "  }",
    "COMPUTE_OVERFLOW;",
    "greater_equal = 0;",
  },
  { "","", "(if cc) psub Sx,Sy,Dz",	"101000ccxxyyzzzz",
    "int Sx = DSP_R (x);",
    "int Sx_grd = GET_DSP_GRD (x);",
    "int Sy = DSP_R (y);",
    "int Sy_grd = SIGN32 (Sy);",
    "",
    "res = Sx - Sy;",
    "carry = (unsigned) res > (unsigned) Sx;",
    "res_grd = Sx_grd - Sy_grd - carry;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
  },
  { "","", "(if cc) psub Sy,Sx,Dz",	"100001ccxxyyzzzz",
    "int Sx = DSP_R (x);",
    "int Sx_grd = GET_DSP_GRD (x);",
    "int Sy = DSP_R (y);",
    "int Sy_grd = SIGN32 (Sy);",
    "",
    "res = Sy - Sx;",
    "carry = (unsigned) res > (unsigned) Sy;",
    "res_grd = Sy_grd - Sx_grd - carry;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
  },
  { "","", "(if cc) padd Sx,Sy,Dz",	"101100ccxxyyzzzz",
    "int Sx = DSP_R (x);",
    "int Sx_grd = GET_DSP_GRD (x);",
    "int Sy = DSP_R (y);",
    "int Sy_grd = SIGN32 (Sy);",
    "",
    "res = Sx + Sy;",
    "carry = (unsigned) res < (unsigned) Sx;",
    "res_grd = Sx_grd + Sy_grd + carry;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
  },
  { "","", "(if cc) pand Sx,Sy,Dz",	"100101ccxxyyzzzz",
    "res = DSP_R (x) & DSP_R (y);",
  "cond_logical:",
    "res &= 0xffff0000;",
    "res_grd = 0;",
    "if (iword & 0x200)\n",
    "  goto assign_z;\n",
  "logical:",
    "carry = 0;",
    "overflow = 0;",
    "greater_equal = 0;",
    "DSR &= ~0xf1;\n",
    "if (res)\n",
    "  DSR |= res >> 26 & DSR_MASK_N;\n",
    "else\n",
    "  DSR |= DSR_MASK_Z;\n",
    "goto assign_dc;\n",
  },
  { "","", "(if cc) pxor Sx,Sy,Dz",	"101001ccxxyyzzzz",
    "res = DSP_R (x) ^ DSP_R (y);",
    "goto cond_logical;",
  },
  { "","", "(if cc) por Sx,Sy,Dz",	"101101ccxxyyzzzz",
    "res = DSP_R (x) | DSP_R (y);",
    "goto cond_logical;",
  },
  { "","", "(if cc) pdec Sx,Dz",	"100010ccxx..zzzz",
    "int Sx = DSP_R (x);",
    "int Sx_grd = GET_DSP_GRD (x);",
    "",
    "res = Sx - 0x10000;",
    "carry = res > Sx;",
    "res_grd = Sx_grd - carry;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
    "res &= 0xffff0000;",
  },
  { "","", "(if cc) pinc Sx,Dz",	"100110ccxx..zzzz",
    "int Sx = DSP_R (x);",
    "int Sx_grd = GET_DSP_GRD (x);",
    "",
    "res = Sx + 0x10000;",
    "carry = res < Sx;",
    "res_grd = Sx_grd + carry;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
    "res &= 0xffff0000;",
  },
  { "","", "(if cc) pdec Sy,Dz",	"101010cc..yyzzzz",
    "int Sy = DSP_R (y);",
    "int Sy_grd = SIGN32 (Sy);",
    "",
    "res = Sy - 0x10000;",
    "carry = res > Sy;",
    "res_grd = Sy_grd - carry;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
    "res &= 0xffff0000;",
  },
  { "","", "(if cc) pinc Sy,Dz",	"101110cc..yyzzzz",
    "int Sy = DSP_R (y);",
    "int Sy_grd = SIGN32 (Sy);",
    "",
    "res = Sy + 0x10000;",
    "carry = res < Sy;",
    "res_grd = Sy_grd + carry;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
    "res &= 0xffff0000;",
  },
  { "","", "(if cc) pclr Dz",		"100011cc....zzzz",
    "res = 0;",
    "res_grd = 0;",
    "carry = 0;",
    "overflow = 0;",
    "greater_equal = 1;",
  },
  { "","", "pclr Du pmuls Se,Sf,Dg",	"0100eeff0001gguu",
    "/* Do multiply.  */",
    "res = (DSP_R (e) >> 16) * (DSP_R (f) >> 16) * 2;",
    "if (res == 0x80000000)",
    "  res = 0x7fffffff;",
    "DSP_R (g) = res;",
    "DSP_GRD (g) = SIGN32 (res);",
    "/* FIXME: update DSR based on results of multiply!  */",
    "",
    "/* Do clr.  */",
    "z = u;",
    "res = 0;",
    "res_grd = 0;",
    "goto assign_z;",
  },
  { "","", "(if cc) pdmsb Sx,Dz",	"100111ccxx..zzzz",
    "unsigned Sx = DSP_R (x);",
    "int Sx_grd = GET_DSP_GRD (x);",
    "int i = 16;",
    "",
    "if (Sx_grd < 0)",
    "  {",
    "    Sx_grd = ~Sx_grd;",
    "    Sx = ~Sx;",
    "  }",
    "if (Sx_grd)",
    "  {",
    "    Sx = Sx_grd;",
    "    res = -2;",
    "  }",
    "else if (Sx)",
    "  res = 30;",
    "else",
    "  res = 31;",
    "do",
    "  {",
    "    if (Sx & ~0 << i)",
    "      {",
    "        res -= i;",
    "        Sx >>= i;",
    "      }",
    "  }",
    "while (i >>= 1);",
    "res <<= 16;",
    "res_grd = SIGN32 (res);",
    "carry = 0;",
    "overflow = 0;",
    "ADD_SUB_GE;",
  },
  { "","", "(if cc) pdmsb Sy,Dz",	"101111cc..yyzzzz",
    "unsigned Sy = DSP_R (y);",
    "int i;",
    "",
    "if (Sy < 0)",
    "  Sy = ~Sy;",
    "Sy <<= 1;",
    "res = 31;",
    "do",
    "  {",
    "    if (Sy & ~0 << i)",
    "      {",
    "        res -= i;",
    "        Sy >>= i;",
    "      }",
    "  }",
    "while (i >>= 1);",
    "res <<= 16;",
    "res_grd = SIGN32 (res);",
    "carry = 0;",
    "overflow = 0;",
    "ADD_SUB_GE;",
  },
  { "","", "(if cc) pneg Sx,Dz",	"110010ccxx..zzzz",
    "int Sx = DSP_R (x);",
    "int Sx_grd = GET_DSP_GRD (x);",
    "",
    "res = 0 - Sx;",
    "carry = res != 0;",
    "res_grd = 0 - Sx_grd - carry;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
  },
  { "","", "(if cc) pcopy Sx,Dz",	"110110ccxx..zzzz",
    "res = DSP_R (x);",
    "res_grd = GET_DSP_GRD (x);",
    "carry = 0;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
  },
  { "","", "(if cc) pneg Sy,Dz",	"111010cc..yyzzzz",
    "int Sy = DSP_R (y);",
    "int Sy_grd = SIGN32 (Sy);",
    "",
    "res = 0 - Sy;",
    "carry = res != 0;",
    "res_grd = 0 - Sy_grd - carry;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
  },
  { "","", "(if cc) pcopy Sy,Dz",	"111110cc..yyzzzz",
    "res = DSP_R (y);",
    "res_grd = SIGN32 (res);",
    "carry = 0;",
    "COMPUTE_OVERFLOW;",
    "ADD_SUB_GE;",
  },
  { "","", "(if cc) psts MACH,Dz",	"110011cc....zzzz",
    "res = MACH;",
    "res_grd = SIGN32 (res);",
    "goto assign_z;",
  },
  { "","", "(if cc) psts MACL,Dz",	"110111cc....zzzz",
    "res = MACL;",
    "res_grd = SIGN32 (res);",
    "goto assign_z;",
  },
  { "","", "(if cc) plds Dz,MACH",	"111011cc....zzzz",
    "if (0xa05f >> z & 1)",
    "  RAISE_EXCEPTION (SIGILL);",
    "else",
    "  MACH = DSP_R (z);",
    "return;",
  },
  { "","", "(if cc) plds Dz,MACL",	"111111cc....zzzz",
    "if (0xa05f >> z & 1)",
    "  RAISE_EXCEPTION (SIGILL);",
    "else",
    "  MACL = DSP_R (z) = res;",
    "return;",
  },
  /* sh4a */
  { "","", "(if cc) pswap Sx,Dz",	"100111ccxx01zzzz",
    "int Sx = DSP_R (x);",
    "",
    "res = ((Sx & 0xffff) * 65536) + ((Sx >> 16) & 0xffff);",
    "res_grd = GET_DSP_GRD (x);",
    "carry = 0;",
    "overflow = 0;",
    "greater_equal = res & 0x80000000 ? 0 : DSR_MASK_G;",
  },
  /* sh4a */
  { "","", "(if cc) pswap Sy,Dz",	"101111cc01yyzzzz",
    "int Sy = DSP_R (y);",
    "",
    "res = ((Sy & 0xffff) * 65536) + ((Sy >> 16) & 0xffff);",
    "res_grd = SIGN32 (Sy);",
    "carry = 0;",
    "overflow = 0;",
    "greater_equal = res & 0x80000000 ? 0 : DSR_MASK_G;",
  },

  {0, 0}
};

/* Tables of things to put into enums for sh-opc.h */
static char *nibble_type_list[] =
{
  "HEX_0",
  "HEX_1",
  "HEX_2",
  "HEX_3",
  "HEX_4",
  "HEX_5",
  "HEX_6",
  "HEX_7",
  "HEX_8",
  "HEX_9",
  "HEX_A",
  "HEX_B",
  "HEX_C",
  "HEX_D",
  "HEX_E",
  "HEX_F",
  "REG_N",
  "REG_M",
  "BRANCH_12",
  "BRANCH_8",
  "DISP_8",
  "DISP_4",
  "IMM_4",
  "IMM_4BY2",
  "IMM_4BY4",
  "PCRELIMM_8BY2",
  "PCRELIMM_8BY4",
  "IMM_8",
  "IMM_8BY2",
  "IMM_8BY4",
  0
};
static
char *arg_type_list[] =
{
  "A_END",
  "A_BDISP12",
  "A_BDISP8",
  "A_DEC_M",
  "A_DEC_N",
  "A_DISP_GBR",
  "A_DISP_PC",
  "A_DISP_REG_M",
  "A_DISP_REG_N",
  "A_GBR",
  "A_IMM",
  "A_INC_M",
  "A_INC_N",
  "A_IND_M",
  "A_IND_N",
  "A_IND_R0_REG_M",
  "A_IND_R0_REG_N",
  "A_MACH",
  "A_MACL",
  "A_PR",
  "A_R0",
  "A_R0_GBR",
  "A_REG_M",
  "A_REG_N",
  "A_SR",
  "A_VBR",
  "A_SSR",
  "A_SPC",
  0,
};

static void
make_enum_list (name, s)
     char *name;
     char **s;
{
  int i = 1;
  printf ("typedef enum {\n");
  while (*s)
    {
      printf ("\t%s,\n", *s);
      s++;
      i++;
    }
  printf ("} %s;\n", name);
}

static int
qfunc (a, b)
     op *a;
     op *b;
{
  char bufa[9];
  char bufb[9];
  int diff;

  memcpy (bufa, a->code, 4);
  memcpy (bufa + 4, a->code + 12, 4);
  bufa[8] = 0;

  memcpy (bufb, b->code, 4);
  memcpy (bufb + 4, b->code + 12, 4);
  bufb[8] = 0;
  diff = strcmp (bufa, bufb);
  /* Stabilize the sort, so that later entries can override more general
     preceding entries.  */
  return diff ? diff : a - b;
}

static void
sorttab ()
{
  op *p = tab;
  int len = 0;

  while (p->name)
    {
      p++;
      len++;
    }
  qsort (tab, len, sizeof (*p), qfunc);
}

static void
gengastab ()
{
  op *p;
  sorttab ();
  for (p = tab; p->name; p++)
    {
      printf ("%s %-30s\n", p->code, p->name);
    }
}

static unsigned short table[1 << 16];

static int warn_conflicts = 0;

static void
conflict_warn (val, i)
     int val;
     int i;
{
  int ix, key;
  int j = table[val];

  fprintf (stderr, "Warning: opcode table conflict: 0x%04x (idx %d && %d)\n",
	   val, i, table[val]);

  for (ix = sizeof (tab) / sizeof (tab[0]); ix >= 0; ix--)
    if (tab[ix].index == i || tab[ix].index == j)
      {
	key = ((tab[ix].code[0] - '0') << 3) + 
	  ((tab[ix].code[1] - '0') << 2) + 
	  ((tab[ix].code[2] - '0') << 1) + 
	  ((tab[ix].code[3] - '0'));

	if (val >> 12 == key)
	  fprintf (stderr, "  %s -- %s\n", tab[ix].code, tab[ix].name);
      }

  for (ix = sizeof (movsxy_tab) / sizeof (movsxy_tab[0]); ix >= 0; ix--)
    if (movsxy_tab[ix].index == i || movsxy_tab[ix].index == j)
      {
	key = ((movsxy_tab[ix].code[0] - '0') << 3) + 
	  ((movsxy_tab[ix].code[1] - '0') << 2) + 
	  ((movsxy_tab[ix].code[2] - '0') << 1) + 
	  ((movsxy_tab[ix].code[3] - '0'));

	if (val >> 12 == key)
	  fprintf (stderr, "  %s -- %s\n", 
		   movsxy_tab[ix].code, movsxy_tab[ix].name);
      }

  for (ix = sizeof (ppi_tab) / sizeof (ppi_tab[0]); ix >= 0; ix--)
    if (ppi_tab[ix].index == i || ppi_tab[ix].index == j)
      {
	key = ((ppi_tab[ix].code[0] - '0') << 3) + 
	  ((ppi_tab[ix].code[1] - '0') << 2) + 
	  ((ppi_tab[ix].code[2] - '0') << 1) + 
	  ((ppi_tab[ix].code[3] - '0'));

	if (val >> 12 == key)
	  fprintf (stderr, "  %s -- %s\n", 
		   ppi_tab[ix].code, ppi_tab[ix].name);
      }
}

/* Take an opcode, expand all varying fields in it out and fill all the
   right entries in 'table' with the opcode index.  */

static void
expand_opcode (val, i, s)
     int val;
     int i;
     char *s;
{
  if (*s == 0)
    {
      if (warn_conflicts && table[val] != 0)
	conflict_warn (val, i);
      table[val] = i;
    }
  else
    {
      int j = 0, m = 0;

      switch (s[0])
	{
	default:
	  fprintf (stderr, "expand_opcode: illegal char '%c'\n", s[0]);
	  exit (1);
	case '0':
	case '1':
	  /* Consume an arbitrary number of ones and zeros.  */
	  do {
	    j = (j << 1) + (s[m++] - '0');
	  } while (s[m] == '0' || s[m] == '1');
	  expand_opcode ((val << m) | j, i, s + m);
	  break;
	case 'N':	/* NN -- four-way fork */
	  for (j = 0; j < 4; j++)
	    expand_opcode ((val << 2) | j, i, s + 2);
	  break;
	case 'x':	/* xx or xy -- two-way or four-way fork */
	  for (j = 0; j < 4; j += (s[1] == 'x' ? 2 : 1))
	    expand_opcode ((val << 2) | j, i, s + 2);
	  break;
	case 'y':	/* yy or yx -- two-way or four-way fork */
	  for (j = 0; j < (s[1] == 'x' ? 4 : 2); j++)
	    expand_opcode ((val << 2) | j, i, s + 2);
	  break;
	case '?':	/* Seven-way "wildcard" fork for movxy */
	  expand_opcode ((val << 2), i, s + 2);
	  for (j = 1; j < 4; j++)
	    {
	      expand_opcode ((val << 2) | j, i, s + 2);
	      expand_opcode ((val << 2) | (j + 16), i, s + 2);
	    }
	  break;
	case 'i':	/* eg. "i8*1" */
	case '.':	/* "...." is a wildcard */
	case 'n':
	case 'm':
	  /* nnnn, mmmm, i#*#, .... -- 16-way fork.  */
	  for (j = 0; j < 16; j++)
	    expand_opcode ((val << 4) | j, i, s + 4);
	  break;
	case 'e':
	  /* eeee -- even numbered register:
	     8 way fork.  */
	  for (j = 0; j < 15; j += 2)
	    expand_opcode ((val << 4) | j, i, s + 4);
	  break;
	case 'M':
	  /* A0, A1, X0, X1, Y0, Y1, M0, M1, A0G, A1G:
	     MMMM -- 10-way fork */
	  expand_opcode ((val << 4) | 5, i, s + 4);
	  for (j = 7; j < 16; j++)
	    expand_opcode ((val << 4) | j, i, s + 4);
	  break;
	case 'G':
	  /* A1G, A0G: 
	     GGGG -- two-way fork */
	  for (j = 13; j <= 15; j +=2)
	    expand_opcode ((val << 4) | j, i, s + 4);
	  break;
	case 's':
	  /* ssss -- 10-way fork */
	  /* System registers mach, macl, pr: */
	  for (j = 0; j < 3; j++)
	    expand_opcode ((val << 4) | j, i, s + 4);
	  /* System registers fpul, fpscr/dsr, a0, x0, x1, y0, y1: */
	  for (j = 5; j < 12; j++)
	    expand_opcode ((val << 4) | j, i, s + 4);
	  break;
	case 'X':
	  /* XX/XY -- 2/4 way fork.  */
	  for (j = 0; j < 4; j += (s[1] == 'X' ? 2 : 1))
	    expand_opcode ((val << 2) | j, i, s + 2);
	  break;
	case 'a':
	  /* aa/ax -- 2/4 way fork.  */
	  for (j = 0; j < 4; j += (s[1] == 'a' ? 2 : 1))
	    expand_opcode ((val << 2) | j, i, s + 2);
	  break;
	case 'Y':
	  /* YY/YX -- 2/4 way fork.  */
	  for (j = 0; j < (s[1] == 'Y' ? 2 : 4); j += 1)
	    expand_opcode ((val << 2) | j, i, s + 2);
	  break;
	case 'A':
	  /* AA/AY: 2/4 way fork.  */
	  for (j = 0; j < (s[1] == 'A' ? 2 : 4); j += 1)
	    expand_opcode ((val << 2) | j, i, s + 2);
	  break;
	case 'v':
	  /* vv(VV) -- 4(16) way fork.  */
	  /* Vector register fv0/4/8/12.  */
	  if (s[2] == 'V')
	    {
	      /* 2 vector registers.  */
	      for (j = 0; j < 15; j++)
		expand_opcode ((val << 4) | j, i, s + 4);
	    }
	  else
	    {
	      /* 1 vector register.  */
	      for (j = 0; j < 4; j += 1)
		expand_opcode ((val << 2) | j, i, s + 2);
	    }
	  break;
	}
    }
}

/* Print the jump table used to index an opcode into a switch
   statement entry.  */

static void
dumptable (name, size, start)
     char *name;
     int size;
     int start;
{
  int lump = 256;
  int online = 16;

  int i = start;

  printf ("unsigned short %s[%d]={\n", name, size);
  while (i < start + size)
    {
      int j = 0;

      printf ("/* 0x%x */\n", i);

      while (j < lump)
	{
	  int k = 0;
	  while (k < online)
	    {
	      printf ("%2d", table[i + j + k]);
	      if (j + k < lump)
		printf (",");

	      k++;
	    }
	  j += k;
	  printf ("\n");
	}
      i += j;
    }
  printf ("};\n");
}


static void
filltable (p)
     op *p;
{
  static int index = 1;

  sorttab ();
  for (; p->name; p++)
    {
      p->index = index++;
      expand_opcode (0, p->index, p->code);
    }
}

/* Table already contains all the switch case tags for 16-bit opcode double
   data transfer (ddt) insns, and the switch case tag for processing parallel
   processing insns (ppi) for code 0xf800 (ppi nopx nopy).  Copy the
   latter tag to represent all combinations of ppi with ddt.  */
static void
expand_ppi_movxy ()
{
  int i;

  for (i = 0xf000; i < 0xf400; i++)
    if (table[i])
      table[i + 0x800] = table[0xf800];
}

static void
gensim_caselist (p)
     op *p;
{
  for (; p->name; p++)
    {
      int j;
      int sextbit = -1;
      int needm = 0;
      int needn = 0;
      
      char *s = p->code;

      printf ("  /* %s %s */\n", p->name, p->code);
      printf ("  case %d:      \n", p->index);

      printf ("    {\n");
      while (*s)
	{
	  switch (*s)
	    {
	    default:
	      fprintf (stderr, "gencode/gensim_caselist: illegal char '%c'\n",
		       *s);
	      exit (1);
	      break;
	    case '?':
	      /* Wildcard expansion, nothing to do here.  */
	      s += 2;
	      break;
	    case 'v':
	      printf ("      int v1 = ((iword >> 10) & 3) * 4;\n");
	      s += 2;
	      break;
	    case 'V':
	      printf ("      int v2 = ((iword >> 8)  & 3) * 4;\n");
	      s += 2;
	      break;
	    case '0':
	    case '1':
	      s += 2;
	      break;
	    case '.':
	      s += 4;
	      break;
	    case 'n':
	    case 'e':
	      printf ("      int n = (iword >> 8) & 0xf;\n");
	      needn = 1;
	      s += 4;
	      break;
	    case 'N':
	      printf ("      int n = (((iword >> 8) - 2) & 0x3) + 2;\n");
	      s += 2;
	      break;
	    case 'x':
	      if (s[1] == 'y')	/* xy */
		{
		  printf ("      int n = (iword & 3) ? \n");
		  printf ("              ((iword >> 9) & 1) + 4 : \n");
		  printf ("              REG_xy ((iword >> 8) & 3);\n");
		}
	      else
		printf ("      int n = ((iword >> 9) & 1) + 4;\n");
	      needn = 1;
	      s += 2;
	      break;
	    case 'y':
	      if (s[1] == 'x')	/* yx */
		{
		  printf ("      int n = (iword & 0xc) ? \n");
		  printf ("              ((iword >> 8) & 1) + 6 : \n");
		  printf ("              REG_yx ((iword >> 8) & 3);\n");
		}
	      else
		printf ("      int n = ((iword >> 8) & 1) + 6;\n");
	      needn = 1;
	      s += 2;
	      break;
	    case 'm':
	      needm = 1;
	    case 's':
	    case 'M':
	    case 'G':
	      printf ("      int m = (iword >> 4) & 0xf;\n");
	      s += 4;
	      break;
	    case 'X':
	      if (s[1] == 'Y')	/* XY */
		{
		  printf ("      int m = (iword & 3) ? \n");
		  printf ("              ((iword >> 7) & 1) + 8 : \n");
		  printf ("              DSP_xy ((iword >> 6) & 3);\n");
		}
	      else
		printf ("      int m = ((iword >> 7) & 1) + 8;\n");
	      s += 2;
	      break;
	    case 'a':
	      if (s[1] == 'x')	/* ax */
		{
		  printf ("      int m = (iword & 3) ? \n");
		  printf ("              7 - ((iword >> 6) & 2) : \n");
		  printf ("              DSP_ax ((iword >> 6) & 3);\n");
		}
	      else
		printf ("      int m = 7 - ((iword >> 6) & 2);\n");
	      s += 2;
	      break;
	    case 'Y':
	      if (s[1] == 'X')	/* YX */
		{
		  printf ("      int m = (iword & 0xc) ? \n");
		  printf ("              ((iword >> 6) & 1) + 10 : \n");
		  printf ("              DSP_yx ((iword >> 6) & 3);\n");
		}
	      else
		printf ("      int m = ((iword >> 6) & 1) + 10;\n");
	      s += 2;
	      break;
	    case 'A':
	      if (s[1] == 'Y')	/* AY */
		{
		  printf ("      int m = (iword & 0xc) ? \n");
		  printf ("              7 - ((iword >> 5) & 2) : \n");
		  printf ("              DSP_ay ((iword >> 6) & 3);\n");
		}
	      else
		printf ("      int m = 7 - ((iword >> 5) & 2);\n");
	      s += 2;
	      break;

	    case 'i':
	      printf ("      int i = (iword & 0x");

	      switch (s[1])
		{
		default:
		  fprintf (stderr, 
			   "gensim_caselist: Unknown char '%c' in %s\n",
			   s[1], s);
		  exit (1);
		  break;
		case '4':
		  printf ("f");
		  break;
		case '8':
		  printf ("ff");
		  break;
		case '1':
		  sextbit = 12;
		  printf ("fff");
		  break;
		}
	      printf (")");

	      switch (s[3])
		{
		default:
		  fprintf (stderr, 
			   "gensim_caselist: Unknown char '%c' in %s\n",
			   s[3], s);
		  exit (1);
		  break;
		case '.':	/* eg. "i12." */
		  break;
		case '1':
		  break;
		case '2':
		  printf (" << 1");
		  break;
		case '4':
		  printf (" << 2");
		  break;
		}
	      printf (";\n");
	      s += 4;
	    }
	}
      if (sextbit > 0)
	{
	  printf ("      i = (i ^ (1 << %d)) - (1 << %d);\n",
		  sextbit - 1, sextbit - 1);
	}

      if (needm && needn)
	printf ("      TB (m,n);\n");  
      else if (needm)
	printf ("      TL (m);\n");
      else if (needn)
	printf ("      TL (n);\n");

      {
	/* Do the refs.  */
	char *r;
	for (r = p->refs; *r; r++)
	  {
	    if (*r == 'f') printf ("      CREF (15);\n");
	    if (*r == '-') 
	      {
		printf ("      {\n");
		printf ("        int i = n;\n");
		printf ("        do {\n");
		printf ("          CREF (i);\n");
		printf ("        } while (i-- > 0);\n");
		printf ("      }\n");
	      }
	    if (*r == '+') 
	      {
		printf ("      {\n");
		printf ("        int i = n;\n");
		printf ("        do {\n");
		printf ("          CREF (i);\n");
		printf ("        } while (i++ < 14);\n");
		printf ("      }\n");
	      }
	    if (*r == '0') printf ("      CREF (0);\n"); 
	    if (*r == '8') printf ("      CREF (8);\n"); 
	    if (*r == '9') printf ("      CREF (9);\n"); 
	    if (*r == 'n') printf ("      CREF (n);\n"); 
	    if (*r == 'm') printf ("      CREF (m);\n"); 
	  }
      }

      printf ("      {\n");
      for (j = 0; j < MAX_NR_STUFF; j++)
	{
	  if (p->stuff[j])
	    {
	      printf ("        %s\n", p->stuff[j]);
	    }
	}
      printf ("      }\n");

      {
	/* Do the defs.  */
	char *r;
	for (r = p->defs; *r; r++) 
	  {
	    if (*r == 'f') printf ("      CDEF (15);\n");
	    if (*r == '-') 
	      {
		printf ("      {\n");
		printf ("        int i = n;\n");
		printf ("        do {\n");
		printf ("          CDEF (i);\n");
		printf ("        } while (i-- > 0);\n");
		printf ("      }\n");
	      }
	    if (*r == '+') 
	      {
		printf ("      {\n");
		printf ("        int i = n;\n");
		printf ("        do {\n");
		printf ("          CDEF (i);\n");
		printf ("        } while (i++ < 14);\n");
		printf ("      }\n");
	      }
	    if (*r == '0') printf ("      CDEF (0);\n"); 
	    if (*r == 'n') printf ("      CDEF (n);\n"); 
	    if (*r == 'm') printf ("      CDEF (m);\n"); 
	  }
      }

      printf ("      break;\n");
      printf ("    }\n");
    }
}

static void
gensim ()
{
  printf ("{\n");
  printf ("/* REG_xy = [r4, r5, r0, r1].  */\n");
  printf ("#define REG_xy(R) ((R)==0 ? 4 : (R)==2 ? 5 : (R)==1 ?  0 :  1)\n");
  printf ("/* REG_yx = [r6, r7, r2, r3].  */\n");
  printf ("#define REG_yx(R) ((R)==0 ? 6 : (R)==1 ? 7 : (R)==2 ?  2 :  3)\n");
  printf ("/* DSP_ax = [a0, a1, x0, x1].  */\n");
  printf ("#define DSP_ax(R) ((R)==0 ? 7 : (R)==2 ? 5 : (R)==1 ?  8 :  9)\n");
  printf ("/* DSP_ay = [a0, a1, y0, y1].  */\n");
  printf ("#define DSP_ay(R) ((R)==0 ? 7 : (R)==1 ? 5 : (R)==2 ? 10 : 11)\n");
  printf ("/* DSP_xy = [x0, x1, y0, y1].  */\n");
  printf ("#define DSP_xy(R) ((R)==0 ? 8 : (R)==2 ? 9 : (R)==1 ? 10 : 11)\n");
  printf ("/* DSP_yx = [y0, y1, x0, x1].  */\n");
  printf ("#define DSP_yx(R) ((R)==0 ? 10 : (R)==1 ? 11 : (R)==2 ? 8 : 9)\n");
  printf ("  switch (jump_table[iword]) {\n");

  gensim_caselist (tab);
  gensim_caselist (movsxy_tab);

  printf ("  default:\n");
  printf ("    {\n");
  printf ("      RAISE_EXCEPTION (SIGILL);\n");
  printf ("    }\n");
  printf ("  }\n");
  printf ("}\n");
}

static void
gendefines ()
{
  op *p;
  filltable (tab);
  for (p = tab; p->name; p++)
    {
      char *s = p->name;
      printf ("#define OPC_");
      while (*s) {
	if (isupper (*s)) 
	  *s = tolower (*s);
	if (isalpha (*s))
	  printf ("%c", *s);
	if (*s == ' ')
	  printf ("_");
	if (*s == '@')
	  printf ("ind_");
	if (*s == ',')
	  printf ("_");
	s++;
      }
      printf (" %d\n",p->index);
    }
}

static int ppi_index;

/* Take a ppi code, expand all varying fields in it and fill all the
   right entries in 'table' with the opcode index.
   NOTE: tail recursion optimization removed for simplicity.  */

static void
expand_ppi_code (val, i, s)
     int val;
     int i;
     char *s;
{
  int j;

  switch (s[0])
    {
    default:
      fprintf (stderr, "gencode/expand_ppi_code: Illegal char '%c'\n", s[0]);
      exit (2);
      break;
    case 'g':
    case 'z':
      if (warn_conflicts && table[val] != 0)
	conflict_warn (val, i);

      /* The last four bits are disregarded for the switch table.  */
      table[val] = i;
      return;
    case 'm':
      /* Four-bit expansion.  */
      for (j = 0; j < 16; j++)
	expand_ppi_code ((val << 4) + j, i, s + 4);
      break;
    case '.':
    case '0':
      expand_ppi_code ((val << 1), i, s + 1);
      break;
    case '1':
      expand_ppi_code ((val << 1) + 1, i, s + 1);
      break;
    case 'i':
    case 'e': case 'f':
    case 'x': case 'y':
      expand_ppi_code ((val << 1), i, s + 1);
      expand_ppi_code ((val << 1) + 1, i, s + 1);
      break;
    case 'c':
      expand_ppi_code ((val << 2) + 1, ppi_index++, s + 2);
      expand_ppi_code ((val << 2) + 2, i, s + 2);
      expand_ppi_code ((val << 2) + 3, i, s + 2);
      break;
    }
}

static void
ppi_filltable ()
{
  op *p;
  ppi_index = 1;

  for (p = ppi_tab; p->name; p++)
    {
      p->index = ppi_index++;
      expand_ppi_code (0, p->index, p->code);
    }
}

static void
ppi_gensim ()
{
  op *p = ppi_tab;

  printf ("#define DSR_MASK_G 0x80\n");
  printf ("#define DSR_MASK_Z 0x40\n");
  printf ("#define DSR_MASK_N 0x20\n");
  printf ("#define DSR_MASK_V 0x10\n");
  printf ("\n");
  printf ("#define COMPUTE_OVERFLOW do {\\\n");
  printf ("  overflow = res_grd != SIGN32 (res) ? DSR_MASK_V : 0; \\\n");
  printf ("  if (overflow && S) \\\n");
  printf ("    { \\\n");
  printf ("      if (res_grd & 0x80) \\\n");
  printf ("        { \\\n");
  printf ("          res = 0x80000000; \\\n");
  printf ("          res_grd |=  0xff; \\\n");
  printf ("        } \\\n");
  printf ("      else \\\n");
  printf ("        { \\\n");
  printf ("          res = 0x7fffffff; \\\n");
  printf ("          res_grd &= ~0xff; \\\n");
  printf ("        } \\\n");
  printf ("      overflow = 0; \\\n");
  printf ("    } \\\n");
  printf ("} while (0)\n");
  printf ("\n");
  printf ("#define ADD_SUB_GE \\\n");
  printf ("  (greater_equal = ~(overflow << 3 & res_grd) & DSR_MASK_G)\n");
  printf ("\n");
  printf ("static void\n");
  printf ("ppi_insn (iword)\n");
  printf ("     int iword;\n");
  printf ("{\n");
  printf ("  /* 'ee' = [x0, x1, y0, a1] */\n");
  printf ("  static char e_tab[] = { 8,  9, 10,  5};\n");
  printf ("  /* 'ff' = [y0, y1, x0, a1] */\n");
  printf ("  static char f_tab[] = {10, 11,  8,  5};\n");
  printf ("  /* 'xx' = [x0, x1, a0, a1]  */\n");
  printf ("  static char x_tab[] = { 8,  9,  7,  5};\n");
  printf ("  /* 'yy' = [y0, y1, m0, m1]  */\n");
  printf ("  static char y_tab[] = {10, 11, 12, 14};\n");
  printf ("  /* 'gg' = [m0, m1, a0, a1]  */\n");
  printf ("  static char g_tab[] = {12, 14,  7,  5};\n");
  printf ("  /* 'uu' = [x0, y0, a0, a1]  */\n");
  printf ("  static char u_tab[] = { 8, 10,  7,  5};\n");
  printf ("\n");
  printf ("  int z;\n");
  printf ("  int res, res_grd;\n");
  printf ("  int carry, overflow, greater_equal;\n");
  printf ("\n");
  printf ("  switch (ppi_table[iword >> 4]) {\n");

  for (; p->name; p++)
    {
      int shift, j;
      int cond = 0;
      int havedecl = 0;
      
      char *s = p->code;

      printf ("  /* %s %s */\n", p->name, p->code);
      printf ("  case %d:      \n", p->index);

      printf ("    {\n");
      for (shift = 16; *s; )
	{
	  switch (*s)
	    {
	    case 'i':
	      printf ("      int i = (iword >> 4) & 0x7f;\n");
	      s += 6;
	      break;
	    case 'e':
	    case 'f':
	    case 'x':
	    case 'y':
	    case 'g':
	    case 'u':
	      shift -= 2;
	      printf ("      int %c = %c_tab[(iword >> %d) & 3];\n",
		      *s, *s, shift);
	      havedecl = 1;
	      s += 2;
	      break;
	    case 'c':
	      printf ("      if ((((iword >> 8) ^ DSR) & 1) == 0)\n");
	      printf ("\treturn;\n");
	      printf ("    }\n");
	      printf ("  case %d:      \n", p->index + 1);
	      printf ("    {\n");
	      cond = 1;
	    case '0':
	    case '1':
	    case '.':
	      shift -= 2;
	      s += 2;
	      break;
	    case 'z':
	      if (havedecl)
		printf ("\n");
	      printf ("      z = iword & 0xf;\n");
	      havedecl = 2;
	      s += 4;
	      break;
	    }
	}
      if (havedecl == 1)
	printf ("\n");
      else if (havedecl == 2)
	printf ("      {\n");
      for (j = 0; j < MAX_NR_STUFF; j++)
	{
	  if (p->stuff[j])
	    {
	      printf ("      %s%s\n",
		      (havedecl == 2 ? "  " : ""),
		      p->stuff[j]);
	    }
	}
      if (havedecl == 2)
	printf ("      }\n");
      if (cond)
	{
	  printf ("      if (iword & 0x200)\n");
	  printf ("        goto assign_z;\n");
	}
      printf ("      break;\n");
      printf ("    }\n");
    }

  printf ("  default:\n");
  printf ("    {\n");
  printf ("      RAISE_EXCEPTION (SIGILL);\n");
  printf ("      return;\n");
  printf ("    }\n");
  printf ("  }\n");
  printf ("  DSR &= ~0xf1;\n");
  printf ("  if (res || res_grd)\n");
  printf ("    DSR |= greater_equal | res_grd >> 2 & DSR_MASK_N | overflow;\n");
  printf ("  else\n");
  printf ("    DSR |= DSR_MASK_Z | overflow;\n");
  printf (" assign_dc:\n");
  printf ("  switch (DSR >> 1 & 7)\n");
  printf ("    {\n");
  printf ("    case 0: /* Carry Mode */\n");
  printf ("      DSR |= carry;\n");
  printf ("    case 1: /* Negative Value Mode */\n");
  printf ("      DSR |= res_grd >> 7 & 1;\n");
  printf ("    case 2: /* Zero Value Mode */\n");
  printf ("      DSR |= DSR >> 6 & 1;\n");
  printf ("    case 3: /* Overflow mode\n");
  printf ("      DSR |= overflow >> 4;\n");
  printf ("    case 4: /* Signed Greater Than Mode */\n");
  printf ("      DSR |= DSR >> 7 & 1;\n");
  printf ("    case 4: /* Signed Greater Than Or Equal Mode */\n");
  printf ("      DSR |= greater_equal >> 7;\n");
  printf ("    }\n");
  printf (" assign_z:\n");
  printf ("  if (0xa05f >> z & 1)\n");
  printf ("    {\n");
  printf ("      RAISE_EXCEPTION (SIGILL);\n");
  printf ("      return;\n");
  printf ("    }\n");
  printf ("  DSP_R (z) = res;\n");
  printf ("  DSP_GRD (z) = res_grd;\n");
  printf ("}\n");
}

int
main (ac, av)
     int ac;
     char **av;
{
  /* Verify the table before anything else.  */
  {
    op *p;
    for (p = tab; p->name; p++)
      {
	/* Check that the code field contains 16 bits.  */
	if (strlen (p->code) != 16)
	  {
	    fprintf (stderr, "Code `%s' length wrong (%d) for `%s'\n",
		     p->code, strlen (p->code), p->name);
	    abort ();
	  }
      }
  }

  /* Now generate the requested data.  */
  if (ac > 1)
    {
      if (ac > 2 && strcmp (av[2], "-w") == 0)
	{
	  warn_conflicts = 1;
	}
      if (strcmp (av[1], "-t") == 0)
	{
	  gengastab ();
	}
      else if (strcmp (av[1], "-d") == 0)
	{
	  gendefines ();
	}
      else if (strcmp (av[1], "-s") == 0)
	{
	  filltable (tab);
	  dumptable ("sh_jump_table", 1 << 16, 0);

	  memset (table, 0, sizeof table);
	  filltable (movsxy_tab);
	  expand_ppi_movxy ();
	  dumptable ("sh_dsp_table", 1 << 12, 0xf000);

	  memset (table, 0, sizeof table);
	  ppi_filltable ();
	  dumptable ("ppi_table", 1 << 12, 0);
	}
      else if (strcmp (av[1], "-x") == 0)
	{
	  filltable (tab);
	  filltable (movsxy_tab);
	  gensim ();
	}
      else if (strcmp (av[1], "-p") == 0)
	{
	  ppi_filltable ();
	  ppi_gensim ();
	}
    }
  else
    fprintf (stderr, "Opcode table generation no longer supported.\n");
  return 0;
}
