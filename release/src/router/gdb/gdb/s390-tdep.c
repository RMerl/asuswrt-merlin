/* Target-dependent code for GDB, the GNU debugger.

   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007
   Free Software Foundation, Inc.

   Contributed by D.J. Barrow (djbarrow@de.ibm.com,barrow_dj@yahoo.com)
   for IBM Deutschland Entwicklung GmbH, IBM Corporation.

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
#include "inferior.h"
#include "symtab.h"
#include "target.h"
#include "gdbcore.h"
#include "gdbcmd.h"
#include "objfiles.h"
#include "floatformat.h"
#include "regcache.h"
#include "trad-frame.h"
#include "frame-base.h"
#include "frame-unwind.h"
#include "dwarf2-frame.h"
#include "reggroups.h"
#include "regset.h"
#include "value.h"
#include "gdb_assert.h"
#include "dis-asm.h"
#include "solib-svr4.h"
#include "prologue-value.h"

#include "s390-tdep.h"


/* The tdep structure.  */

struct gdbarch_tdep
{
  /* ABI version.  */
  enum { ABI_LINUX_S390, ABI_LINUX_ZSERIES } abi;

  /* Core file register sets.  */
  const struct regset *gregset;
  int sizeof_gregset;

  const struct regset *fpregset;
  int sizeof_fpregset;
};


/* Return the name of register REGNUM.  */
static const char *
s390_register_name (int regnum)
{
  static const char *register_names[S390_NUM_TOTAL_REGS] =
    {
      /* Program Status Word.  */
      "pswm", "pswa",
      /* General Purpose Registers.  */
      "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
      "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
      /* Access Registers.  */
      "acr0", "acr1", "acr2", "acr3", "acr4", "acr5", "acr6", "acr7",
      "acr8", "acr9", "acr10", "acr11", "acr12", "acr13", "acr14", "acr15",
      /* Floating Point Control Word.  */
      "fpc",
      /* Floating Point Registers.  */
      "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
      "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",
      /* Pseudo registers.  */
      "pc", "cc",
    };

  gdb_assert (regnum >= 0 && regnum < S390_NUM_TOTAL_REGS);
  return register_names[regnum];
}

/* Return the GDB type object for the "standard" data type of data in
   register REGNUM.  */
static struct type *
s390_register_type (struct gdbarch *gdbarch, int regnum)
{
  if (regnum == S390_PSWM_REGNUM || regnum == S390_PSWA_REGNUM)
    return builtin_type_long;
  if (regnum >= S390_R0_REGNUM && regnum <= S390_R15_REGNUM)
    return builtin_type_long;
  if (regnum >= S390_A0_REGNUM && regnum <= S390_A15_REGNUM)
    return builtin_type_int;
  if (regnum == S390_FPC_REGNUM)
    return builtin_type_int;
  if (regnum >= S390_F0_REGNUM && regnum <= S390_F15_REGNUM)
    return builtin_type_double;
  if (regnum == S390_PC_REGNUM)
    return builtin_type_void_func_ptr;
  if (regnum == S390_CC_REGNUM)
    return builtin_type_int;

  internal_error (__FILE__, __LINE__, _("invalid regnum"));
}

/* DWARF Register Mapping.  */

static int s390_dwarf_regmap[] =
{
  /* General Purpose Registers.  */
  S390_R0_REGNUM, S390_R1_REGNUM, S390_R2_REGNUM, S390_R3_REGNUM,
  S390_R4_REGNUM, S390_R5_REGNUM, S390_R6_REGNUM, S390_R7_REGNUM,
  S390_R8_REGNUM, S390_R9_REGNUM, S390_R10_REGNUM, S390_R11_REGNUM,
  S390_R12_REGNUM, S390_R13_REGNUM, S390_R14_REGNUM, S390_R15_REGNUM,

  /* Floating Point Registers.  */
  S390_F0_REGNUM, S390_F2_REGNUM, S390_F4_REGNUM, S390_F6_REGNUM,
  S390_F1_REGNUM, S390_F3_REGNUM, S390_F5_REGNUM, S390_F7_REGNUM,
  S390_F8_REGNUM, S390_F10_REGNUM, S390_F12_REGNUM, S390_F14_REGNUM,
  S390_F9_REGNUM, S390_F11_REGNUM, S390_F13_REGNUM, S390_F15_REGNUM,

  /* Control Registers (not mapped).  */
  -1, -1, -1, -1, -1, -1, -1, -1, 
  -1, -1, -1, -1, -1, -1, -1, -1, 

  /* Access Registers.  */
  S390_A0_REGNUM, S390_A1_REGNUM, S390_A2_REGNUM, S390_A3_REGNUM,
  S390_A4_REGNUM, S390_A5_REGNUM, S390_A6_REGNUM, S390_A7_REGNUM,
  S390_A8_REGNUM, S390_A9_REGNUM, S390_A10_REGNUM, S390_A11_REGNUM,
  S390_A12_REGNUM, S390_A13_REGNUM, S390_A14_REGNUM, S390_A15_REGNUM,

  /* Program Status Word.  */
  S390_PSWM_REGNUM,
  S390_PSWA_REGNUM
};

/* Convert DWARF register number REG to the appropriate register
   number used by GDB.  */
static int
s390_dwarf_reg_to_regnum (int reg)
{
  int regnum = -1;

  if (reg >= 0 && reg < ARRAY_SIZE (s390_dwarf_regmap))
    regnum = s390_dwarf_regmap[reg];

  if (regnum == -1)
    warning (_("Unmapped DWARF Register #%d encountered."), reg);

  return regnum;
}

/* Pseudo registers - PC and condition code.  */

static void
s390_pseudo_register_read (struct gdbarch *gdbarch, struct regcache *regcache,
			   int regnum, gdb_byte *buf)
{
  ULONGEST val;

  switch (regnum)
    {
    case S390_PC_REGNUM:
      regcache_raw_read_unsigned (regcache, S390_PSWA_REGNUM, &val);
      store_unsigned_integer (buf, 4, val & 0x7fffffff);
      break;

    case S390_CC_REGNUM:
      regcache_raw_read_unsigned (regcache, S390_PSWM_REGNUM, &val);
      store_unsigned_integer (buf, 4, (val >> 12) & 3);
      break;

    default:
      internal_error (__FILE__, __LINE__, _("invalid regnum"));
    }
}

static void
s390_pseudo_register_write (struct gdbarch *gdbarch, struct regcache *regcache,
			    int regnum, const gdb_byte *buf)
{
  ULONGEST val, psw;

  switch (regnum)
    {
    case S390_PC_REGNUM:
      val = extract_unsigned_integer (buf, 4);
      regcache_raw_read_unsigned (regcache, S390_PSWA_REGNUM, &psw);
      psw = (psw & 0x80000000) | (val & 0x7fffffff);
      regcache_raw_write_unsigned (regcache, S390_PSWA_REGNUM, psw);
      break;

    case S390_CC_REGNUM:
      val = extract_unsigned_integer (buf, 4);
      regcache_raw_read_unsigned (regcache, S390_PSWM_REGNUM, &psw);
      psw = (psw & ~((ULONGEST)3 << 12)) | ((val & 3) << 12);
      regcache_raw_write_unsigned (regcache, S390_PSWM_REGNUM, psw);
      break;

    default:
      internal_error (__FILE__, __LINE__, _("invalid regnum"));
    }
}

static void
s390x_pseudo_register_read (struct gdbarch *gdbarch, struct regcache *regcache,
			    int regnum, gdb_byte *buf)
{
  ULONGEST val;

  switch (regnum)
    {
    case S390_PC_REGNUM:
      regcache_raw_read (regcache, S390_PSWA_REGNUM, buf);
      break;

    case S390_CC_REGNUM:
      regcache_raw_read_unsigned (regcache, S390_PSWM_REGNUM, &val);
      store_unsigned_integer (buf, 4, (val >> 44) & 3);
      break;

    default:
      internal_error (__FILE__, __LINE__, _("invalid regnum"));
    }
}

static void
s390x_pseudo_register_write (struct gdbarch *gdbarch, struct regcache *regcache,
			     int regnum, const gdb_byte *buf)
{
  ULONGEST val, psw;

  switch (regnum)
    {
    case S390_PC_REGNUM:
      regcache_raw_write (regcache, S390_PSWA_REGNUM, buf);
      break;

    case S390_CC_REGNUM:
      val = extract_unsigned_integer (buf, 4);
      regcache_raw_read_unsigned (regcache, S390_PSWM_REGNUM, &psw);
      psw = (psw & ~((ULONGEST)3 << 44)) | ((val & 3) << 44);
      regcache_raw_write_unsigned (regcache, S390_PSWM_REGNUM, psw);
      break;

    default:
      internal_error (__FILE__, __LINE__, _("invalid regnum"));
    }
}

/* 'float' values are stored in the upper half of floating-point
   registers, even though we are otherwise a big-endian platform.  */

static struct value *
s390_value_from_register (struct type *type, int regnum,
			  struct frame_info *frame)
{
  struct value *value = default_value_from_register (type, regnum, frame);
  int len = TYPE_LENGTH (type);

  if (regnum >= S390_F0_REGNUM && regnum <= S390_F15_REGNUM && len < 8)
    set_value_offset (value, 0);

  return value;
}

/* Register groups.  */

static int
s390_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
			  struct reggroup *group)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* Registers displayed via 'info regs'.  */
  if (group == general_reggroup)
    return (regnum >= S390_R0_REGNUM && regnum <= S390_R15_REGNUM)
	   || regnum == S390_PC_REGNUM
	   || regnum == S390_CC_REGNUM;

  /* Registers displayed via 'info float'.  */
  if (group == float_reggroup)
    return (regnum >= S390_F0_REGNUM && regnum <= S390_F15_REGNUM)
	   || regnum == S390_FPC_REGNUM;

  /* Registers that need to be saved/restored in order to
     push or pop frames.  */
  if (group == save_reggroup || group == restore_reggroup)
    return regnum != S390_PSWM_REGNUM && regnum != S390_PSWA_REGNUM;

  return default_register_reggroup_p (gdbarch, regnum, group);
}


/* Core file register sets.  */

int s390_regmap_gregset[S390_NUM_REGS] =
{
  /* Program Status Word.  */
  0x00, 0x04,
  /* General Purpose Registers.  */
  0x08, 0x0c, 0x10, 0x14,
  0x18, 0x1c, 0x20, 0x24,
  0x28, 0x2c, 0x30, 0x34,
  0x38, 0x3c, 0x40, 0x44,
  /* Access Registers.  */
  0x48, 0x4c, 0x50, 0x54,
  0x58, 0x5c, 0x60, 0x64,
  0x68, 0x6c, 0x70, 0x74,
  0x78, 0x7c, 0x80, 0x84,
  /* Floating Point Control Word.  */
  -1,
  /* Floating Point Registers.  */
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,
};

int s390x_regmap_gregset[S390_NUM_REGS] =
{
  0x00, 0x08,
  /* General Purpose Registers.  */
  0x10, 0x18, 0x20, 0x28,
  0x30, 0x38, 0x40, 0x48,
  0x50, 0x58, 0x60, 0x68,
  0x70, 0x78, 0x80, 0x88,
  /* Access Registers.  */
  0x90, 0x94, 0x98, 0x9c,
  0xa0, 0xa4, 0xa8, 0xac,
  0xb0, 0xb4, 0xb8, 0xbc,
  0xc0, 0xc4, 0xc8, 0xcc,
  /* Floating Point Control Word.  */
  -1,
  /* Floating Point Registers.  */
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,
};

int s390_regmap_fpregset[S390_NUM_REGS] =
{
  /* Program Status Word.  */
  -1, -1,
  /* General Purpose Registers.  */
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,
  /* Access Registers.  */
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,
  /* Floating Point Control Word.  */
  0x00,
  /* Floating Point Registers.  */
  0x08, 0x10, 0x18, 0x20,
  0x28, 0x30, 0x38, 0x40,
  0x48, 0x50, 0x58, 0x60,
  0x68, 0x70, 0x78, 0x80,
};

/* Supply register REGNUM from the register set REGSET to register cache 
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */
static void
s390_supply_regset (const struct regset *regset, struct regcache *regcache,
		    int regnum, const void *regs, size_t len)
{
  const int *offset = regset->descr;
  int i;

  for (i = 0; i < S390_NUM_REGS; i++)
    {
      if ((regnum == i || regnum == -1) && offset[i] != -1)
	regcache_raw_supply (regcache, i, (const char *)regs + offset[i]);
    }
}

/* Collect register REGNUM from the register cache REGCACHE and store
   it in the buffer specified by REGS and LEN as described by the
   general-purpose register set REGSET.  If REGNUM is -1, do this for
   all registers in REGSET.  */
static void
s390_collect_regset (const struct regset *regset,
		     const struct regcache *regcache,
		     int regnum, void *regs, size_t len)
{
  const int *offset = regset->descr;
  int i;

  for (i = 0; i < S390_NUM_REGS; i++)
    {
      if ((regnum == i || regnum == -1) && offset[i] != -1)
	regcache_raw_collect (regcache, i, (char *)regs + offset[i]);
    }
}

static const struct regset s390_gregset = {
  s390_regmap_gregset, 
  s390_supply_regset,
  s390_collect_regset
};

static const struct regset s390x_gregset = {
  s390x_regmap_gregset, 
  s390_supply_regset,
  s390_collect_regset
};

static const struct regset s390_fpregset = {
  s390_regmap_fpregset, 
  s390_supply_regset,
  s390_collect_regset
};

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */
const struct regset *
s390_regset_from_core_section (struct gdbarch *gdbarch,
			       const char *sect_name, size_t sect_size)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  if (strcmp (sect_name, ".reg") == 0 && sect_size >= tdep->sizeof_gregset)
    return tdep->gregset;

  if (strcmp (sect_name, ".reg2") == 0 && sect_size >= tdep->sizeof_fpregset)
    return tdep->fpregset;

  return NULL;
}


/* Decoding S/390 instructions.  */

/* Named opcode values for the S/390 instructions we recognize.  Some
   instructions have their opcode split across two fields; those are the
   op1_* and op2_* enums.  */
enum
  {
    op1_lhi  = 0xa7,   op2_lhi  = 0x08,
    op1_lghi = 0xa7,   op2_lghi = 0x09,
    op1_lgfi = 0xc0,   op2_lgfi = 0x01,
    op_lr    = 0x18,
    op_lgr   = 0xb904,
    op_l     = 0x58,
    op1_ly   = 0xe3,   op2_ly   = 0x58,
    op1_lg   = 0xe3,   op2_lg   = 0x04,
    op_lm    = 0x98,
    op1_lmy  = 0xeb,   op2_lmy  = 0x98,
    op1_lmg  = 0xeb,   op2_lmg  = 0x04,
    op_st    = 0x50,
    op1_sty  = 0xe3,   op2_sty  = 0x50,
    op1_stg  = 0xe3,   op2_stg  = 0x24,
    op_std   = 0x60,
    op_stm   = 0x90,
    op1_stmy = 0xeb,   op2_stmy = 0x90,
    op1_stmg = 0xeb,   op2_stmg = 0x24,
    op1_aghi = 0xa7,   op2_aghi = 0x0b,
    op1_ahi  = 0xa7,   op2_ahi  = 0x0a,
    op1_agfi = 0xc2,   op2_agfi = 0x08,
    op1_afi  = 0xc2,   op2_afi  = 0x09,
    op1_algfi= 0xc2,   op2_algfi= 0x0a,
    op1_alfi = 0xc2,   op2_alfi = 0x0b,
    op_ar    = 0x1a,
    op_agr   = 0xb908,
    op_a     = 0x5a,
    op1_ay   = 0xe3,   op2_ay   = 0x5a,
    op1_ag   = 0xe3,   op2_ag   = 0x08,
    op1_slgfi= 0xc2,   op2_slgfi= 0x04,
    op1_slfi = 0xc2,   op2_slfi = 0x05,
    op_sr    = 0x1b,
    op_sgr   = 0xb909,
    op_s     = 0x5b,
    op1_sy   = 0xe3,   op2_sy   = 0x5b,
    op1_sg   = 0xe3,   op2_sg   = 0x09,
    op_nr    = 0x14,
    op_ngr   = 0xb980,
    op_la    = 0x41,
    op1_lay  = 0xe3,   op2_lay  = 0x71,
    op1_larl = 0xc0,   op2_larl = 0x00,
    op_basr  = 0x0d,
    op_bas   = 0x4d,
    op_bcr   = 0x07,
    op_bc    = 0x0d,
    op1_bras = 0xa7,   op2_bras = 0x05,
    op1_brasl= 0xc0,   op2_brasl= 0x05,
    op1_brc  = 0xa7,   op2_brc  = 0x04,
    op1_brcl = 0xc0,   op2_brcl = 0x04,
  };


/* Read a single instruction from address AT.  */

#define S390_MAX_INSTR_SIZE 6
static int
s390_readinstruction (bfd_byte instr[], CORE_ADDR at)
{
  static int s390_instrlen[] = { 2, 4, 4, 6 };
  int instrlen;

  if (read_memory_nobpt (at, &instr[0], 2))
    return -1;
  instrlen = s390_instrlen[instr[0] >> 6];
  if (instrlen > 2)
    {
      if (read_memory_nobpt (at + 2, &instr[2], instrlen - 2))
        return -1;
    }
  return instrlen;
}


/* The functions below are for recognizing and decoding S/390
   instructions of various formats.  Each of them checks whether INSN
   is an instruction of the given format, with the specified opcodes.
   If it is, it sets the remaining arguments to the values of the
   instruction's fields, and returns a non-zero value; otherwise, it
   returns zero.

   These functions' arguments appear in the order they appear in the
   instruction, not in the machine-language form.  So, opcodes always
   come first, even though they're sometimes scattered around the
   instructions.  And displacements appear before base and extension
   registers, as they do in the assembly syntax, not at the end, as
   they do in the machine language.  */
static int
is_ri (bfd_byte *insn, int op1, int op2, unsigned int *r1, int *i2)
{
  if (insn[0] == op1 && (insn[1] & 0xf) == op2)
    {
      *r1 = (insn[1] >> 4) & 0xf;
      /* i2 is a 16-bit signed quantity.  */
      *i2 = (((insn[2] << 8) | insn[3]) ^ 0x8000) - 0x8000;
      return 1;
    }
  else
    return 0;
}


static int
is_ril (bfd_byte *insn, int op1, int op2,
        unsigned int *r1, int *i2)
{
  if (insn[0] == op1 && (insn[1] & 0xf) == op2)
    {
      *r1 = (insn[1] >> 4) & 0xf;
      /* i2 is a signed quantity.  If the host 'int' is 32 bits long,
         no sign extension is necessary, but we don't want to assume
         that.  */
      *i2 = (((insn[2] << 24)
              | (insn[3] << 16)
              | (insn[4] << 8)
              | (insn[5])) ^ 0x80000000) - 0x80000000;
      return 1;
    }
  else
    return 0;
}


static int
is_rr (bfd_byte *insn, int op, unsigned int *r1, unsigned int *r2)
{
  if (insn[0] == op)
    {
      *r1 = (insn[1] >> 4) & 0xf;
      *r2 = insn[1] & 0xf;
      return 1;
    }
  else
    return 0;
}


static int
is_rre (bfd_byte *insn, int op, unsigned int *r1, unsigned int *r2)
{
  if (((insn[0] << 8) | insn[1]) == op)
    {
      /* Yes, insn[3].  insn[2] is unused in RRE format.  */
      *r1 = (insn[3] >> 4) & 0xf;
      *r2 = insn[3] & 0xf;
      return 1;
    }
  else
    return 0;
}


static int
is_rs (bfd_byte *insn, int op,
       unsigned int *r1, unsigned int *r3, unsigned int *d2, unsigned int *b2)
{
  if (insn[0] == op)
    {
      *r1 = (insn[1] >> 4) & 0xf;
      *r3 = insn[1] & 0xf;
      *b2 = (insn[2] >> 4) & 0xf;
      *d2 = ((insn[2] & 0xf) << 8) | insn[3];
      return 1;
    }
  else
    return 0;
}


static int
is_rsy (bfd_byte *insn, int op1, int op2,
        unsigned int *r1, unsigned int *r3, unsigned int *d2, unsigned int *b2)
{
  if (insn[0] == op1
      && insn[5] == op2)
    {
      *r1 = (insn[1] >> 4) & 0xf;
      *r3 = insn[1] & 0xf;
      *b2 = (insn[2] >> 4) & 0xf;
      /* The 'long displacement' is a 20-bit signed integer.  */
      *d2 = ((((insn[2] & 0xf) << 8) | insn[3] | (insn[4] << 12)) 
		^ 0x80000) - 0x80000;
      return 1;
    }
  else
    return 0;
}


static int
is_rx (bfd_byte *insn, int op,
       unsigned int *r1, unsigned int *d2, unsigned int *x2, unsigned int *b2)
{
  if (insn[0] == op)
    {
      *r1 = (insn[1] >> 4) & 0xf;
      *x2 = insn[1] & 0xf;
      *b2 = (insn[2] >> 4) & 0xf;
      *d2 = ((insn[2] & 0xf) << 8) | insn[3];
      return 1;
    }
  else
    return 0;
}


static int
is_rxy (bfd_byte *insn, int op1, int op2,
        unsigned int *r1, unsigned int *d2, unsigned int *x2, unsigned int *b2)
{
  if (insn[0] == op1
      && insn[5] == op2)
    {
      *r1 = (insn[1] >> 4) & 0xf;
      *x2 = insn[1] & 0xf;
      *b2 = (insn[2] >> 4) & 0xf;
      /* The 'long displacement' is a 20-bit signed integer.  */
      *d2 = ((((insn[2] & 0xf) << 8) | insn[3] | (insn[4] << 12)) 
		^ 0x80000) - 0x80000;
      return 1;
    }
  else
    return 0;
}


/* Prologue analysis.  */

#define S390_NUM_GPRS 16
#define S390_NUM_FPRS 16

struct s390_prologue_data {

  /* The stack.  */
  struct pv_area *stack;

  /* The size of a GPR or FPR.  */
  int gpr_size;
  int fpr_size;

  /* The general-purpose registers.  */
  pv_t gpr[S390_NUM_GPRS];

  /* The floating-point registers.  */
  pv_t fpr[S390_NUM_FPRS];

  /* The offset relative to the CFA where the incoming GPR N was saved
     by the function prologue.  0 if not saved or unknown.  */
  int gpr_slot[S390_NUM_GPRS];

  /* Likewise for FPRs.  */
  int fpr_slot[S390_NUM_FPRS];

  /* Nonzero if the backchain was saved.  This is assumed to be the
     case when the incoming SP is saved at the current SP location.  */
  int back_chain_saved_p;
};

/* Return the effective address for an X-style instruction, like:

        L R1, D2(X2, B2)

   Here, X2 and B2 are registers, and D2 is a signed 20-bit
   constant; the effective address is the sum of all three.  If either
   X2 or B2 are zero, then it doesn't contribute to the sum --- this
   means that r0 can't be used as either X2 or B2.  */
static pv_t
s390_addr (struct s390_prologue_data *data,
	   int d2, unsigned int x2, unsigned int b2)
{
  pv_t result;

  result = pv_constant (d2);
  if (x2)
    result = pv_add (result, data->gpr[x2]);
  if (b2)
    result = pv_add (result, data->gpr[b2]);

  return result;
}

/* Do a SIZE-byte store of VALUE to D2(X2,B2).  */
static void
s390_store (struct s390_prologue_data *data,
	    int d2, unsigned int x2, unsigned int b2, CORE_ADDR size,
	    pv_t value)
{
  pv_t addr = s390_addr (data, d2, x2, b2);
  pv_t offset;

  /* Check whether we are storing the backchain.  */
  offset = pv_subtract (data->gpr[S390_SP_REGNUM - S390_R0_REGNUM], addr);

  if (pv_is_constant (offset) && offset.k == 0)
    if (size == data->gpr_size
	&& pv_is_register_k (value, S390_SP_REGNUM, 0))
      {
	data->back_chain_saved_p = 1;
	return;
      }


  /* Check whether we are storing a register into the stack.  */
  if (!pv_area_store_would_trash (data->stack, addr))
    pv_area_store (data->stack, addr, size, value);


  /* Note: If this is some store we cannot identify, you might think we
     should forget our cached values, as any of those might have been hit.

     However, we make the assumption that the register save areas are only
     ever stored to once in any given function, and we do recognize these
     stores.  Thus every store we cannot recognize does not hit our data.  */
}

/* Do a SIZE-byte load from D2(X2,B2).  */
static pv_t
s390_load (struct s390_prologue_data *data,
	   int d2, unsigned int x2, unsigned int b2, CORE_ADDR size)
	   
{
  pv_t addr = s390_addr (data, d2, x2, b2);
  pv_t offset;

  /* If it's a load from an in-line constant pool, then we can
     simulate that, under the assumption that the code isn't
     going to change between the time the processor actually
     executed it creating the current frame, and the time when
     we're analyzing the code to unwind past that frame.  */
  if (pv_is_constant (addr))
    {
      struct section_table *secp;
      secp = target_section_by_addr (&current_target, addr.k);
      if (secp != NULL
          && (bfd_get_section_flags (secp->bfd, secp->the_bfd_section)
              & SEC_READONLY))
        return pv_constant (read_memory_integer (addr.k, size));
    }

  /* Check whether we are accessing one of our save slots.  */
  return pv_area_fetch (data->stack, addr, size);
}

/* Function for finding saved registers in a 'struct pv_area'; we pass
   this to pv_area_scan.

   If VALUE is a saved register, ADDR says it was saved at a constant
   offset from the frame base, and SIZE indicates that the whole
   register was saved, record its offset in the reg_offset table in
   PROLOGUE_UNTYPED.  */
static void
s390_check_for_saved (void *data_untyped, pv_t addr, CORE_ADDR size, pv_t value)
{
  struct s390_prologue_data *data = data_untyped;
  int i, offset;

  if (!pv_is_register (addr, S390_SP_REGNUM))
    return;

  offset = 16 * data->gpr_size + 32 - addr.k;

  /* If we are storing the original value of a register, we want to
     record the CFA offset.  If the same register is stored multiple
     times, the stack slot with the highest address counts.  */
 
  for (i = 0; i < S390_NUM_GPRS; i++)
    if (size == data->gpr_size
	&& pv_is_register_k (value, S390_R0_REGNUM + i, 0))
      if (data->gpr_slot[i] == 0
	  || data->gpr_slot[i] > offset)
	{
	  data->gpr_slot[i] = offset;
	  return;
	}

  for (i = 0; i < S390_NUM_FPRS; i++)
    if (size == data->fpr_size
	&& pv_is_register_k (value, S390_F0_REGNUM + i, 0))
      if (data->fpr_slot[i] == 0
	  || data->fpr_slot[i] > offset)
	{
	  data->fpr_slot[i] = offset;
	  return;
	}
}

/* Analyze the prologue of the function starting at START_PC,
   continuing at most until CURRENT_PC.  Initialize DATA to
   hold all information we find out about the state of the registers
   and stack slots.  Return the address of the instruction after
   the last one that changed the SP, FP, or back chain; or zero
   on error.  */
static CORE_ADDR
s390_analyze_prologue (struct gdbarch *gdbarch,
		       CORE_ADDR start_pc,
		       CORE_ADDR current_pc,
		       struct s390_prologue_data *data)
{
  int word_size = gdbarch_ptr_bit (gdbarch) / 8;

  /* Our return value:
     The address of the instruction after the last one that changed
     the SP, FP, or back chain;  zero if we got an error trying to 
     read memory.  */
  CORE_ADDR result = start_pc;

  /* The current PC for our abstract interpretation.  */
  CORE_ADDR pc;

  /* The address of the next instruction after that.  */
  CORE_ADDR next_pc;
  
  /* Set up everything's initial value.  */
  {
    int i;

    data->stack = make_pv_area (S390_SP_REGNUM);

    /* For the purpose of prologue tracking, we consider the GPR size to
       be equal to the ABI word size, even if it is actually larger
       (i.e. when running a 32-bit binary under a 64-bit kernel).  */
    data->gpr_size = word_size;
    data->fpr_size = 8;

    for (i = 0; i < S390_NUM_GPRS; i++)
      data->gpr[i] = pv_register (S390_R0_REGNUM + i, 0);

    for (i = 0; i < S390_NUM_FPRS; i++)
      data->fpr[i] = pv_register (S390_F0_REGNUM + i, 0);

    for (i = 0; i < S390_NUM_GPRS; i++)
      data->gpr_slot[i]  = 0;

    for (i = 0; i < S390_NUM_FPRS; i++)
      data->fpr_slot[i]  = 0;

    data->back_chain_saved_p = 0;
  }

  /* Start interpreting instructions, until we hit the frame's
     current PC or the first branch instruction.  */
  for (pc = start_pc; pc > 0 && pc < current_pc; pc = next_pc)
    {
      bfd_byte insn[S390_MAX_INSTR_SIZE];
      int insn_len = s390_readinstruction (insn, pc);

      bfd_byte dummy[S390_MAX_INSTR_SIZE] = { 0 };
      bfd_byte *insn32 = word_size == 4 ? insn : dummy;
      bfd_byte *insn64 = word_size == 8 ? insn : dummy;

      /* Fields for various kinds of instructions.  */
      unsigned int b2, r1, r2, x2, r3;
      int i2, d2;

      /* The values of SP and FP before this instruction,
         for detecting instructions that change them.  */
      pv_t pre_insn_sp, pre_insn_fp;
      /* Likewise for the flag whether the back chain was saved.  */
      int pre_insn_back_chain_saved_p;

      /* If we got an error trying to read the instruction, report it.  */
      if (insn_len < 0)
        {
          result = 0;
          break;
        }

      next_pc = pc + insn_len;

      pre_insn_sp = data->gpr[S390_SP_REGNUM - S390_R0_REGNUM];
      pre_insn_fp = data->gpr[S390_FRAME_REGNUM - S390_R0_REGNUM];
      pre_insn_back_chain_saved_p = data->back_chain_saved_p;


      /* LHI r1, i2 --- load halfword immediate.  */
      /* LGHI r1, i2 --- load halfword immediate (64-bit version).  */
      /* LGFI r1, i2 --- load fullword immediate.  */
      if (is_ri (insn32, op1_lhi, op2_lhi, &r1, &i2)
          || is_ri (insn64, op1_lghi, op2_lghi, &r1, &i2)
          || is_ril (insn, op1_lgfi, op2_lgfi, &r1, &i2))
	data->gpr[r1] = pv_constant (i2);

      /* LR r1, r2 --- load from register.  */
      /* LGR r1, r2 --- load from register (64-bit version).  */
      else if (is_rr (insn32, op_lr, &r1, &r2)
	       || is_rre (insn64, op_lgr, &r1, &r2))
	data->gpr[r1] = data->gpr[r2];

      /* L r1, d2(x2, b2) --- load.  */
      /* LY r1, d2(x2, b2) --- load (long-displacement version).  */
      /* LG r1, d2(x2, b2) --- load (64-bit version).  */
      else if (is_rx (insn32, op_l, &r1, &d2, &x2, &b2)
	       || is_rxy (insn32, op1_ly, op2_ly, &r1, &d2, &x2, &b2)
	       || is_rxy (insn64, op1_lg, op2_lg, &r1, &d2, &x2, &b2))
	data->gpr[r1] = s390_load (data, d2, x2, b2, data->gpr_size);

      /* ST r1, d2(x2, b2) --- store.  */
      /* STY r1, d2(x2, b2) --- store (long-displacement version).  */
      /* STG r1, d2(x2, b2) --- store (64-bit version).  */
      else if (is_rx (insn32, op_st, &r1, &d2, &x2, &b2)
	       || is_rxy (insn32, op1_sty, op2_sty, &r1, &d2, &x2, &b2)
	       || is_rxy (insn64, op1_stg, op2_stg, &r1, &d2, &x2, &b2))
	s390_store (data, d2, x2, b2, data->gpr_size, data->gpr[r1]);

      /* STD r1, d2(x2,b2) --- store floating-point register.  */
      else if (is_rx (insn, op_std, &r1, &d2, &x2, &b2))
	s390_store (data, d2, x2, b2, data->fpr_size, data->fpr[r1]);

      /* STM r1, r3, d2(b2) --- store multiple.  */
      /* STMY r1, r3, d2(b2) --- store multiple (long-displacement version).  */
      /* STMG r1, r3, d2(b2) --- store multiple (64-bit version).  */
      else if (is_rs (insn32, op_stm, &r1, &r3, &d2, &b2)
	       || is_rsy (insn32, op1_stmy, op2_stmy, &r1, &r3, &d2, &b2)
	       || is_rsy (insn64, op1_stmg, op2_stmg, &r1, &r3, &d2, &b2))
        {
          for (; r1 <= r3; r1++, d2 += data->gpr_size)
	    s390_store (data, d2, 0, b2, data->gpr_size, data->gpr[r1]);
        }

      /* AHI r1, i2 --- add halfword immediate.  */
      /* AGHI r1, i2 --- add halfword immediate (64-bit version).  */
      /* AFI r1, i2 --- add fullword immediate.  */
      /* AGFI r1, i2 --- add fullword immediate (64-bit version).  */
      else if (is_ri (insn32, op1_ahi, op2_ahi, &r1, &i2)
	       || is_ri (insn64, op1_aghi, op2_aghi, &r1, &i2)
	       || is_ril (insn32, op1_afi, op2_afi, &r1, &i2)
	       || is_ril (insn64, op1_agfi, op2_agfi, &r1, &i2))
	data->gpr[r1] = pv_add_constant (data->gpr[r1], i2);

      /* ALFI r1, i2 --- add logical immediate.  */
      /* ALGFI r1, i2 --- add logical immediate (64-bit version).  */
      else if (is_ril (insn32, op1_alfi, op2_alfi, &r1, &i2)
	       || is_ril (insn64, op1_algfi, op2_algfi, &r1, &i2))
	data->gpr[r1] = pv_add_constant (data->gpr[r1],
					 (CORE_ADDR)i2 & 0xffffffff);

      /* AR r1, r2 -- add register.  */
      /* AGR r1, r2 -- add register (64-bit version).  */
      else if (is_rr (insn32, op_ar, &r1, &r2)
	       || is_rre (insn64, op_agr, &r1, &r2))
	data->gpr[r1] = pv_add (data->gpr[r1], data->gpr[r2]);

      /* A r1, d2(x2, b2) -- add.  */
      /* AY r1, d2(x2, b2) -- add (long-displacement version).  */
      /* AG r1, d2(x2, b2) -- add (64-bit version).  */
      else if (is_rx (insn32, op_a, &r1, &d2, &x2, &b2)
	       || is_rxy (insn32, op1_ay, op2_ay, &r1, &d2, &x2, &b2)
	       || is_rxy (insn64, op1_ag, op2_ag, &r1, &d2, &x2, &b2))
	data->gpr[r1] = pv_add (data->gpr[r1],
				s390_load (data, d2, x2, b2, data->gpr_size));

      /* SLFI r1, i2 --- subtract logical immediate.  */
      /* SLGFI r1, i2 --- subtract logical immediate (64-bit version).  */
      else if (is_ril (insn32, op1_slfi, op2_slfi, &r1, &i2)
	       || is_ril (insn64, op1_slgfi, op2_slgfi, &r1, &i2))
	data->gpr[r1] = pv_add_constant (data->gpr[r1],
					 -((CORE_ADDR)i2 & 0xffffffff));

      /* SR r1, r2 -- subtract register.  */
      /* SGR r1, r2 -- subtract register (64-bit version).  */
      else if (is_rr (insn32, op_sr, &r1, &r2)
	       || is_rre (insn64, op_sgr, &r1, &r2))
	data->gpr[r1] = pv_subtract (data->gpr[r1], data->gpr[r2]);

      /* S r1, d2(x2, b2) -- subtract.  */
      /* SY r1, d2(x2, b2) -- subtract (long-displacement version).  */
      /* SG r1, d2(x2, b2) -- subtract (64-bit version).  */
      else if (is_rx (insn32, op_s, &r1, &d2, &x2, &b2)
	       || is_rxy (insn32, op1_sy, op2_sy, &r1, &d2, &x2, &b2)
	       || is_rxy (insn64, op1_sg, op2_sg, &r1, &d2, &x2, &b2))
	data->gpr[r1] = pv_subtract (data->gpr[r1],
				s390_load (data, d2, x2, b2, data->gpr_size));

      /* LA r1, d2(x2, b2) --- load address.  */
      /* LAY r1, d2(x2, b2) --- load address (long-displacement version).  */
      else if (is_rx (insn, op_la, &r1, &d2, &x2, &b2)
               || is_rxy (insn, op1_lay, op2_lay, &r1, &d2, &x2, &b2))
	data->gpr[r1] = s390_addr (data, d2, x2, b2);

      /* LARL r1, i2 --- load address relative long.  */
      else if (is_ril (insn, op1_larl, op2_larl, &r1, &i2))
	data->gpr[r1] = pv_constant (pc + i2 * 2);

      /* BASR r1, 0 --- branch and save.
         Since r2 is zero, this saves the PC in r1, but doesn't branch.  */
      else if (is_rr (insn, op_basr, &r1, &r2)
               && r2 == 0)
	data->gpr[r1] = pv_constant (next_pc);

      /* BRAS r1, i2 --- branch relative and save.  */
      else if (is_ri (insn, op1_bras, op2_bras, &r1, &i2))
        {
          data->gpr[r1] = pv_constant (next_pc);
          next_pc = pc + i2 * 2;

          /* We'd better not interpret any backward branches.  We'll
             never terminate.  */
          if (next_pc <= pc)
            break;
        }

      /* Terminate search when hitting any other branch instruction.  */
      else if (is_rr (insn, op_basr, &r1, &r2)
	       || is_rx (insn, op_bas, &r1, &d2, &x2, &b2)
	       || is_rr (insn, op_bcr, &r1, &r2)
	       || is_rx (insn, op_bc, &r1, &d2, &x2, &b2)
	       || is_ri (insn, op1_brc, op2_brc, &r1, &i2)
	       || is_ril (insn, op1_brcl, op2_brcl, &r1, &i2)
	       || is_ril (insn, op1_brasl, op2_brasl, &r2, &i2))
	break;

      else
        /* An instruction we don't know how to simulate.  The only
           safe thing to do would be to set every value we're tracking
           to 'unknown'.  Instead, we'll be optimistic: we assume that
	   we *can* interpret every instruction that the compiler uses
	   to manipulate any of the data we're interested in here --
	   then we can just ignore anything else.  */
        ;

      /* Record the address after the last instruction that changed
         the FP, SP, or backlink.  Ignore instructions that changed
         them back to their original values --- those are probably
         restore instructions.  (The back chain is never restored,
         just popped.)  */
      {
        pv_t sp = data->gpr[S390_SP_REGNUM - S390_R0_REGNUM];
        pv_t fp = data->gpr[S390_FRAME_REGNUM - S390_R0_REGNUM];
        
        if ((! pv_is_identical (pre_insn_sp, sp)
             && ! pv_is_register_k (sp, S390_SP_REGNUM, 0)
	     && sp.kind != pvk_unknown)
            || (! pv_is_identical (pre_insn_fp, fp)
                && ! pv_is_register_k (fp, S390_FRAME_REGNUM, 0)
		&& fp.kind != pvk_unknown)
            || pre_insn_back_chain_saved_p != data->back_chain_saved_p)
          result = next_pc;
      }
    }

  /* Record where all the registers were saved.  */
  pv_area_scan (data->stack, s390_check_for_saved, data);

  free_pv_area (data->stack);
  data->stack = NULL;

  return result;
}

/* Advance PC across any function entry prologue instructions to reach 
   some "real" code.  */
static CORE_ADDR
s390_skip_prologue (CORE_ADDR pc)
{
  struct s390_prologue_data data;
  CORE_ADDR skip_pc;
  skip_pc = s390_analyze_prologue (current_gdbarch, pc, (CORE_ADDR)-1, &data);
  return skip_pc ? skip_pc : pc;
}

/* Return true if we are in the functin's epilogue, i.e. after the
   instruction that destroyed the function's stack frame.  */
static int
s390_in_function_epilogue_p (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  int word_size = gdbarch_ptr_bit (gdbarch) / 8;

  /* In frameless functions, there's not frame to destroy and thus
     we don't care about the epilogue.

     In functions with frame, the epilogue sequence is a pair of
     a LM-type instruction that restores (amongst others) the
     return register %r14 and the stack pointer %r15, followed
     by a branch 'br %r14' --or equivalent-- that effects the
     actual return.

     In that situation, this function needs to return 'true' in
     exactly one case: when pc points to that branch instruction.

     Thus we try to disassemble the one instructions immediately
     preceeding pc and check whether it is an LM-type instruction
     modifying the stack pointer.

     Note that disassembling backwards is not reliable, so there
     is a slight chance of false positives here ...  */

  bfd_byte insn[6];
  unsigned int r1, r3, b2;
  int d2;

  if (word_size == 4
      && !read_memory_nobpt (pc - 4, insn, 4)
      && is_rs (insn, op_lm, &r1, &r3, &d2, &b2)
      && r3 == S390_SP_REGNUM - S390_R0_REGNUM)
    return 1;

  if (word_size == 4
      && !read_memory_nobpt (pc - 6, insn, 6)
      && is_rsy (insn, op1_lmy, op2_lmy, &r1, &r3, &d2, &b2)
      && r3 == S390_SP_REGNUM - S390_R0_REGNUM)
    return 1;

  if (word_size == 8
      && !read_memory_nobpt (pc - 6, insn, 6)
      && is_rsy (insn, op1_lmg, op2_lmg, &r1, &r3, &d2, &b2)
      && r3 == S390_SP_REGNUM - S390_R0_REGNUM)
    return 1;

  return 0;
}


/* Normal stack frames.  */

struct s390_unwind_cache {

  CORE_ADDR func;
  CORE_ADDR frame_base;
  CORE_ADDR local_base;

  struct trad_frame_saved_reg *saved_regs;
};

static int
s390_prologue_frame_unwind_cache (struct frame_info *next_frame,
				  struct s390_unwind_cache *info)
{
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  int word_size = gdbarch_ptr_bit (gdbarch) / 8;
  struct s390_prologue_data data;
  pv_t *fp = &data.gpr[S390_FRAME_REGNUM - S390_R0_REGNUM];
  pv_t *sp = &data.gpr[S390_SP_REGNUM - S390_R0_REGNUM];
  int i;
  CORE_ADDR cfa;
  CORE_ADDR func;
  CORE_ADDR result;
  ULONGEST reg;
  CORE_ADDR prev_sp;
  int frame_pointer;
  int size;

  /* Try to find the function start address.  If we can't find it, we don't
     bother searching for it -- with modern compilers this would be mostly
     pointless anyway.  Trust that we'll either have valid DWARF-2 CFI data
     or else a valid backchain ...  */
  func = frame_func_unwind (next_frame, NORMAL_FRAME);
  if (!func)
    return 0;

  /* Try to analyze the prologue.  */
  result = s390_analyze_prologue (gdbarch, func,
				  frame_pc_unwind (next_frame), &data);
  if (!result)
    return 0;

  /* If this was successful, we should have found the instruction that
     sets the stack pointer register to the previous value of the stack 
     pointer minus the frame size.  */
  if (!pv_is_register (*sp, S390_SP_REGNUM))
    return 0;

  /* A frame size of zero at this point can mean either a real 
     frameless function, or else a failure to find the prologue.
     Perform some sanity checks to verify we really have a 
     frameless function.  */
  if (sp->k == 0)
    {
      /* If the next frame is a NORMAL_FRAME, this frame *cannot* have frame 
	 size zero.  This is only possible if the next frame is a sentinel 
	 frame, a dummy frame, or a signal trampoline frame.  */
      /* FIXME: cagney/2004-05-01: This sanity check shouldn't be
	 needed, instead the code should simpliy rely on its
	 analysis.  */
      if (get_frame_type (next_frame) == NORMAL_FRAME)
	return 0;

      /* If we really have a frameless function, %r14 must be valid
	 -- in particular, it must point to a different function.  */
      reg = frame_unwind_register_unsigned (next_frame, S390_RETADDR_REGNUM);
      reg = gdbarch_addr_bits_remove (gdbarch, reg) - 1;
      if (get_pc_function_start (reg) == func)
	{
	  /* However, there is one case where it *is* valid for %r14
	     to point to the same function -- if this is a recursive
	     call, and we have stopped in the prologue *before* the
	     stack frame was allocated.

	     Recognize this case by looking ahead a bit ...  */

	  struct s390_prologue_data data2;
	  pv_t *sp = &data2.gpr[S390_SP_REGNUM - S390_R0_REGNUM];

	  if (!(s390_analyze_prologue (gdbarch, func, (CORE_ADDR)-1, &data2)
	        && pv_is_register (*sp, S390_SP_REGNUM)
	        && sp->k != 0))
	    return 0;
	}
    }


  /* OK, we've found valid prologue data.  */
  size = -sp->k;

  /* If the frame pointer originally also holds the same value
     as the stack pointer, we're probably using it.  If it holds
     some other value -- even a constant offset -- it is most
     likely used as temp register.  */
  if (pv_is_identical (*sp, *fp))
    frame_pointer = S390_FRAME_REGNUM;
  else
    frame_pointer = S390_SP_REGNUM;

  /* If we've detected a function with stack frame, we'll still have to 
     treat it as frameless if we're currently within the function epilog 
     code at a point where the frame pointer has already been restored.  
     This can only happen in an innermost frame.  */
  /* FIXME: cagney/2004-05-01: This sanity check shouldn't be needed,
     instead the code should simpliy rely on its analysis.  */
  if (size > 0 && get_frame_type (next_frame) != NORMAL_FRAME)
    {
      /* See the comment in s390_in_function_epilogue_p on why this is
	 not completely reliable ...  */
      if (s390_in_function_epilogue_p (gdbarch, frame_pc_unwind (next_frame)))
	{
	  memset (&data, 0, sizeof (data));
	  size = 0;
	  frame_pointer = S390_SP_REGNUM;
	}
    }

  /* Once we know the frame register and the frame size, we can unwind
     the current value of the frame register from the next frame, and
     add back the frame size to arrive that the previous frame's 
     stack pointer value.  */
  prev_sp = frame_unwind_register_unsigned (next_frame, frame_pointer) + size;
  cfa = prev_sp + 16*word_size + 32;

  /* Record the addresses of all register spill slots the prologue parser
     has recognized.  Consider only registers defined as call-saved by the
     ABI; for call-clobbered registers the parser may have recognized
     spurious stores.  */

  for (i = 6; i <= 15; i++)
    if (data.gpr_slot[i] != 0)
      info->saved_regs[S390_R0_REGNUM + i].addr = cfa - data.gpr_slot[i];

  switch (tdep->abi)
    {
    case ABI_LINUX_S390:
      if (data.fpr_slot[4] != 0)
        info->saved_regs[S390_F4_REGNUM].addr = cfa - data.fpr_slot[4];
      if (data.fpr_slot[6] != 0)
        info->saved_regs[S390_F6_REGNUM].addr = cfa - data.fpr_slot[6];
      break;

    case ABI_LINUX_ZSERIES:
      for (i = 8; i <= 15; i++)
	if (data.fpr_slot[i] != 0)
	  info->saved_regs[S390_F0_REGNUM + i].addr = cfa - data.fpr_slot[i];
      break;
    }

  /* Function return will set PC to %r14.  */
  info->saved_regs[S390_PC_REGNUM] = info->saved_regs[S390_RETADDR_REGNUM];

  /* In frameless functions, we unwind simply by moving the return
     address to the PC.  However, if we actually stored to the
     save area, use that -- we might only think the function frameless
     because we're in the middle of the prologue ...  */
  if (size == 0
      && !trad_frame_addr_p (info->saved_regs, S390_PC_REGNUM))
    {
      info->saved_regs[S390_PC_REGNUM].realreg = S390_RETADDR_REGNUM;
    }

  /* Another sanity check: unless this is a frameless function,
     we should have found spill slots for SP and PC.
     If not, we cannot unwind further -- this happens e.g. in
     libc's thread_start routine.  */
  if (size > 0)
    {
      if (!trad_frame_addr_p (info->saved_regs, S390_SP_REGNUM)
	  || !trad_frame_addr_p (info->saved_regs, S390_PC_REGNUM))
	prev_sp = -1;
    }

  /* We use the current value of the frame register as local_base,
     and the top of the register save area as frame_base.  */
  if (prev_sp != -1)
    {
      info->frame_base = prev_sp + 16*word_size + 32;
      info->local_base = prev_sp - size;
    }

  info->func = func;
  return 1;
}

static void
s390_backchain_frame_unwind_cache (struct frame_info *next_frame,
				   struct s390_unwind_cache *info)
{
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  int word_size = gdbarch_ptr_bit (gdbarch) / 8;
  CORE_ADDR backchain;
  ULONGEST reg;
  LONGEST sp;

  /* Get the backchain.  */
  reg = frame_unwind_register_unsigned (next_frame, S390_SP_REGNUM);
  backchain = read_memory_unsigned_integer (reg, word_size);

  /* A zero backchain terminates the frame chain.  As additional
     sanity check, let's verify that the spill slot for SP in the
     save area pointed to by the backchain in fact links back to
     the save area.  */
  if (backchain != 0
      && safe_read_memory_integer (backchain + 15*word_size, word_size, &sp)
      && (CORE_ADDR)sp == backchain)
    {
      /* We don't know which registers were saved, but it will have
         to be at least %r14 and %r15.  This will allow us to continue
         unwinding, but other prev-frame registers may be incorrect ...  */
      info->saved_regs[S390_SP_REGNUM].addr = backchain + 15*word_size;
      info->saved_regs[S390_RETADDR_REGNUM].addr = backchain + 14*word_size;

      /* Function return will set PC to %r14.  */
      info->saved_regs[S390_PC_REGNUM] = info->saved_regs[S390_RETADDR_REGNUM];

      /* We use the current value of the frame register as local_base,
         and the top of the register save area as frame_base.  */
      info->frame_base = backchain + 16*word_size + 32;
      info->local_base = reg;
    }

  info->func = frame_pc_unwind (next_frame);
}

static struct s390_unwind_cache *
s390_frame_unwind_cache (struct frame_info *next_frame,
			 void **this_prologue_cache)
{
  struct s390_unwind_cache *info;
  if (*this_prologue_cache)
    return *this_prologue_cache;

  info = FRAME_OBSTACK_ZALLOC (struct s390_unwind_cache);
  *this_prologue_cache = info;
  info->saved_regs = trad_frame_alloc_saved_regs (next_frame);
  info->func = -1;
  info->frame_base = -1;
  info->local_base = -1;

  /* Try to use prologue analysis to fill the unwind cache.
     If this fails, fall back to reading the stack backchain.  */
  if (!s390_prologue_frame_unwind_cache (next_frame, info))
    s390_backchain_frame_unwind_cache (next_frame, info);

  return info;
}

static void
s390_frame_this_id (struct frame_info *next_frame,
		    void **this_prologue_cache,
		    struct frame_id *this_id)
{
  struct s390_unwind_cache *info
    = s390_frame_unwind_cache (next_frame, this_prologue_cache);

  if (info->frame_base == -1)
    return;

  *this_id = frame_id_build (info->frame_base, info->func);
}

static void
s390_frame_prev_register (struct frame_info *next_frame,
			  void **this_prologue_cache,
			  int regnum, int *optimizedp,
			  enum lval_type *lvalp, CORE_ADDR *addrp,
			  int *realnump, gdb_byte *bufferp)
{
  struct s390_unwind_cache *info
    = s390_frame_unwind_cache (next_frame, this_prologue_cache);
  trad_frame_get_prev_register (next_frame, info->saved_regs, regnum,
				optimizedp, lvalp, addrp, realnump, bufferp);
}

static const struct frame_unwind s390_frame_unwind = {
  NORMAL_FRAME,
  s390_frame_this_id,
  s390_frame_prev_register
};

static const struct frame_unwind *
s390_frame_sniffer (struct frame_info *next_frame)
{
  return &s390_frame_unwind;
}


/* Code stubs and their stack frames.  For things like PLTs and NULL
   function calls (where there is no true frame and the return address
   is in the RETADDR register).  */

struct s390_stub_unwind_cache
{
  CORE_ADDR frame_base;
  struct trad_frame_saved_reg *saved_regs;
};

static struct s390_stub_unwind_cache *
s390_stub_frame_unwind_cache (struct frame_info *next_frame,
			      void **this_prologue_cache)
{
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  int word_size = gdbarch_ptr_bit (gdbarch) / 8;
  struct s390_stub_unwind_cache *info;
  ULONGEST reg;

  if (*this_prologue_cache)
    return *this_prologue_cache;

  info = FRAME_OBSTACK_ZALLOC (struct s390_stub_unwind_cache);
  *this_prologue_cache = info;
  info->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  /* The return address is in register %r14.  */
  info->saved_regs[S390_PC_REGNUM].realreg = S390_RETADDR_REGNUM;

  /* Retrieve stack pointer and determine our frame base.  */
  reg = frame_unwind_register_unsigned (next_frame, S390_SP_REGNUM);
  info->frame_base = reg + 16*word_size + 32;

  return info;
}

static void
s390_stub_frame_this_id (struct frame_info *next_frame,
			 void **this_prologue_cache,
			 struct frame_id *this_id)
{
  struct s390_stub_unwind_cache *info
    = s390_stub_frame_unwind_cache (next_frame, this_prologue_cache);
  *this_id = frame_id_build (info->frame_base, frame_pc_unwind (next_frame));
}

static void
s390_stub_frame_prev_register (struct frame_info *next_frame,
			       void **this_prologue_cache,
			       int regnum, int *optimizedp,
			       enum lval_type *lvalp, CORE_ADDR *addrp,
			       int *realnump, gdb_byte *bufferp)
{
  struct s390_stub_unwind_cache *info
    = s390_stub_frame_unwind_cache (next_frame, this_prologue_cache);
  trad_frame_get_prev_register (next_frame, info->saved_regs, regnum,
				optimizedp, lvalp, addrp, realnump, bufferp);
}

static const struct frame_unwind s390_stub_frame_unwind = {
  NORMAL_FRAME,
  s390_stub_frame_this_id,
  s390_stub_frame_prev_register
};

static const struct frame_unwind *
s390_stub_frame_sniffer (struct frame_info *next_frame)
{
  CORE_ADDR addr_in_block;
  bfd_byte insn[S390_MAX_INSTR_SIZE];

  /* If the current PC points to non-readable memory, we assume we
     have trapped due to an invalid function pointer call.  We handle
     the non-existing current function like a PLT stub.  */
  addr_in_block = frame_unwind_address_in_block (next_frame, NORMAL_FRAME);
  if (in_plt_section (addr_in_block, NULL)
      || s390_readinstruction (insn, frame_pc_unwind (next_frame)) < 0)
    return &s390_stub_frame_unwind;
  return NULL;
}


/* Signal trampoline stack frames.  */

struct s390_sigtramp_unwind_cache {
  CORE_ADDR frame_base;
  struct trad_frame_saved_reg *saved_regs;
};

static struct s390_sigtramp_unwind_cache *
s390_sigtramp_frame_unwind_cache (struct frame_info *next_frame,
				  void **this_prologue_cache)
{
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  int word_size = gdbarch_ptr_bit (gdbarch) / 8;
  struct s390_sigtramp_unwind_cache *info;
  ULONGEST this_sp, prev_sp;
  CORE_ADDR next_ra, next_cfa, sigreg_ptr;
  int i;

  if (*this_prologue_cache)
    return *this_prologue_cache;

  info = FRAME_OBSTACK_ZALLOC (struct s390_sigtramp_unwind_cache);
  *this_prologue_cache = info;
  info->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  this_sp = frame_unwind_register_unsigned (next_frame, S390_SP_REGNUM);
  next_ra = frame_pc_unwind (next_frame);
  next_cfa = this_sp + 16*word_size + 32;

  /* New-style RT frame:
	retcode + alignment (8 bytes)
	siginfo (128 bytes)
	ucontext (contains sigregs at offset 5 words)  */
  if (next_ra == next_cfa)
    {
      sigreg_ptr = next_cfa + 8 + 128 + align_up (5*word_size, 8);
    }

  /* Old-style RT frame and all non-RT frames:
	old signal mask (8 bytes)
	pointer to sigregs  */
  else
    {
      sigreg_ptr = read_memory_unsigned_integer (next_cfa + 8, word_size);
    }

  /* The sigregs structure looks like this:
            long   psw_mask;
            long   psw_addr;
            long   gprs[16];
            int    acrs[16];
            int    fpc;
            int    __pad;
            double fprs[16];  */

  /* Let's ignore the PSW mask, it will not be restored anyway.  */
  sigreg_ptr += word_size;

  /* Next comes the PSW address.  */
  info->saved_regs[S390_PC_REGNUM].addr = sigreg_ptr;
  sigreg_ptr += word_size;

  /* Then the GPRs.  */
  for (i = 0; i < 16; i++)
    {
      info->saved_regs[S390_R0_REGNUM + i].addr = sigreg_ptr;
      sigreg_ptr += word_size;
    }

  /* Then the ACRs.  */
  for (i = 0; i < 16; i++)
    {
      info->saved_regs[S390_A0_REGNUM + i].addr = sigreg_ptr;
      sigreg_ptr += 4;
    }

  /* The floating-point control word.  */
  info->saved_regs[S390_FPC_REGNUM].addr = sigreg_ptr;
  sigreg_ptr += 8;

  /* And finally the FPRs.  */
  for (i = 0; i < 16; i++)
    {
      info->saved_regs[S390_F0_REGNUM + i].addr = sigreg_ptr;
      sigreg_ptr += 8;
    }

  /* Restore the previous frame's SP.  */
  prev_sp = read_memory_unsigned_integer (
			info->saved_regs[S390_SP_REGNUM].addr,
			word_size);

  /* Determine our frame base.  */
  info->frame_base = prev_sp + 16*word_size + 32;

  return info;
}

static void
s390_sigtramp_frame_this_id (struct frame_info *next_frame,
			     void **this_prologue_cache,
			     struct frame_id *this_id)
{
  struct s390_sigtramp_unwind_cache *info
    = s390_sigtramp_frame_unwind_cache (next_frame, this_prologue_cache);
  *this_id = frame_id_build (info->frame_base, frame_pc_unwind (next_frame));
}

static void
s390_sigtramp_frame_prev_register (struct frame_info *next_frame,
				   void **this_prologue_cache,
				   int regnum, int *optimizedp,
				   enum lval_type *lvalp, CORE_ADDR *addrp,
				   int *realnump, gdb_byte *bufferp)
{
  struct s390_sigtramp_unwind_cache *info
    = s390_sigtramp_frame_unwind_cache (next_frame, this_prologue_cache);
  trad_frame_get_prev_register (next_frame, info->saved_regs, regnum,
				optimizedp, lvalp, addrp, realnump, bufferp);
}

static const struct frame_unwind s390_sigtramp_frame_unwind = {
  SIGTRAMP_FRAME,
  s390_sigtramp_frame_this_id,
  s390_sigtramp_frame_prev_register
};

static const struct frame_unwind *
s390_sigtramp_frame_sniffer (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  bfd_byte sigreturn[2];

  if (read_memory_nobpt (pc, sigreturn, 2))
    return NULL;

  if (sigreturn[0] != 0x0a /* svc */)
    return NULL;

  if (sigreturn[1] != 119 /* sigreturn */
      && sigreturn[1] != 173 /* rt_sigreturn */)
    return NULL;
  
  return &s390_sigtramp_frame_unwind;
}


/* Frame base handling.  */

static CORE_ADDR
s390_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct s390_unwind_cache *info
    = s390_frame_unwind_cache (next_frame, this_cache);
  return info->frame_base;
}

static CORE_ADDR
s390_local_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct s390_unwind_cache *info
    = s390_frame_unwind_cache (next_frame, this_cache);
  return info->local_base;
}

static const struct frame_base s390_frame_base = {
  &s390_frame_unwind,
  s390_frame_base_address,
  s390_local_base_address,
  s390_local_base_address
};

static CORE_ADDR
s390_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  ULONGEST pc;
  pc = frame_unwind_register_unsigned (next_frame, S390_PC_REGNUM);
  return gdbarch_addr_bits_remove (gdbarch, pc);
}

static CORE_ADDR
s390_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  ULONGEST sp;
  sp = frame_unwind_register_unsigned (next_frame, S390_SP_REGNUM);
  return gdbarch_addr_bits_remove (gdbarch, sp);
}


/* DWARF-2 frame support.  */

static void
s390_dwarf2_frame_init_reg (struct gdbarch *gdbarch, int regnum,
                            struct dwarf2_frame_state_reg *reg,
			    struct frame_info *next_frame)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  switch (tdep->abi)
    {
    case ABI_LINUX_S390:
      /* Call-saved registers.  */
      if ((regnum >= S390_R6_REGNUM && regnum <= S390_R15_REGNUM)
	  || regnum == S390_F4_REGNUM
	  || regnum == S390_F6_REGNUM)
	reg->how = DWARF2_FRAME_REG_SAME_VALUE;

      /* Call-clobbered registers.  */
      else if ((regnum >= S390_R0_REGNUM && regnum <= S390_R5_REGNUM)
	       || (regnum >= S390_F0_REGNUM && regnum <= S390_F15_REGNUM
		   && regnum != S390_F4_REGNUM && regnum != S390_F6_REGNUM))
	reg->how = DWARF2_FRAME_REG_UNDEFINED;

      /* The return address column.  */
      else if (regnum == S390_PC_REGNUM)
	reg->how = DWARF2_FRAME_REG_RA;
      break;

    case ABI_LINUX_ZSERIES:
      /* Call-saved registers.  */
      if ((regnum >= S390_R6_REGNUM && regnum <= S390_R15_REGNUM)
	  || (regnum >= S390_F8_REGNUM && regnum <= S390_F15_REGNUM))
	reg->how = DWARF2_FRAME_REG_SAME_VALUE;

      /* Call-clobbered registers.  */
      else if ((regnum >= S390_R0_REGNUM && regnum <= S390_R5_REGNUM)
	       || (regnum >= S390_F0_REGNUM && regnum <= S390_F7_REGNUM))
	reg->how = DWARF2_FRAME_REG_UNDEFINED;

      /* The return address column.  */
      else if (regnum == S390_PC_REGNUM)
	reg->how = DWARF2_FRAME_REG_RA;
      break;
    }
}


/* Dummy function calls.  */

/* Return non-zero if TYPE is an integer-like type, zero otherwise.
   "Integer-like" types are those that should be passed the way
   integers are: integers, enums, ranges, characters, and booleans.  */
static int
is_integer_like (struct type *type)
{
  enum type_code code = TYPE_CODE (type);

  return (code == TYPE_CODE_INT
          || code == TYPE_CODE_ENUM
          || code == TYPE_CODE_RANGE
          || code == TYPE_CODE_CHAR
          || code == TYPE_CODE_BOOL);
}

/* Return non-zero if TYPE is a pointer-like type, zero otherwise.
   "Pointer-like" types are those that should be passed the way
   pointers are: pointers and references.  */
static int
is_pointer_like (struct type *type)
{
  enum type_code code = TYPE_CODE (type);

  return (code == TYPE_CODE_PTR
          || code == TYPE_CODE_REF);
}


/* Return non-zero if TYPE is a `float singleton' or `double
   singleton', zero otherwise.

   A `T singleton' is a struct type with one member, whose type is
   either T or a `T singleton'.  So, the following are all float
   singletons:

   struct { float x };
   struct { struct { float x; } x; };
   struct { struct { struct { float x; } x; } x; };

   ... and so on.

   All such structures are passed as if they were floats or doubles,
   as the (revised) ABI says.  */
static int
is_float_singleton (struct type *type)
{
  if (TYPE_CODE (type) == TYPE_CODE_STRUCT && TYPE_NFIELDS (type) == 1)
    {
      struct type *singleton_type = TYPE_FIELD_TYPE (type, 0);
      CHECK_TYPEDEF (singleton_type);

      return (TYPE_CODE (singleton_type) == TYPE_CODE_FLT
	      || is_float_singleton (singleton_type));
    }

  return 0;
}


/* Return non-zero if TYPE is a struct-like type, zero otherwise.
   "Struct-like" types are those that should be passed as structs are:
   structs and unions.

   As an odd quirk, not mentioned in the ABI, GCC passes float and
   double singletons as if they were a plain float, double, etc.  (The
   corresponding union types are handled normally.)  So we exclude
   those types here.  *shrug* */
static int
is_struct_like (struct type *type)
{
  enum type_code code = TYPE_CODE (type);

  return (code == TYPE_CODE_UNION
          || (code == TYPE_CODE_STRUCT && ! is_float_singleton (type)));
}


/* Return non-zero if TYPE is a float-like type, zero otherwise.
   "Float-like" types are those that should be passed as
   floating-point values are.

   You'd think this would just be floats, doubles, long doubles, etc.
   But as an odd quirk, not mentioned in the ABI, GCC passes float and
   double singletons as if they were a plain float, double, etc.  (The
   corresponding union types are handled normally.)  So we include
   those types here.  *shrug* */
static int
is_float_like (struct type *type)
{
  return (TYPE_CODE (type) == TYPE_CODE_FLT
          || is_float_singleton (type));
}


static int
is_power_of_two (unsigned int n)
{
  return ((n & (n - 1)) == 0);
}

/* Return non-zero if TYPE should be passed as a pointer to a copy,
   zero otherwise.  */
static int
s390_function_arg_pass_by_reference (struct type *type)
{
  unsigned length = TYPE_LENGTH (type);
  if (length > 8)
    return 1;

  /* FIXME: All complex and vector types are also returned by reference.  */
  return is_struct_like (type) && !is_power_of_two (length);
}

/* Return non-zero if TYPE should be passed in a float register
   if possible.  */
static int
s390_function_arg_float (struct type *type)
{
  unsigned length = TYPE_LENGTH (type);
  if (length > 8)
    return 0;

  return is_float_like (type);
}

/* Return non-zero if TYPE should be passed in an integer register
   (or a pair of integer registers) if possible.  */
static int
s390_function_arg_integer (struct type *type)
{
  unsigned length = TYPE_LENGTH (type);
  if (length > 8)
    return 0;

   return is_integer_like (type)
	  || is_pointer_like (type)
	  || (is_struct_like (type) && is_power_of_two (length));
}

/* Return ARG, a `SIMPLE_ARG', sign-extended or zero-extended to a full
   word as required for the ABI.  */
static LONGEST
extend_simple_arg (struct value *arg)
{
  struct type *type = value_type (arg);

  /* Even structs get passed in the least significant bits of the
     register / memory word.  It's not really right to extract them as
     an integer, but it does take care of the extension.  */
  if (TYPE_UNSIGNED (type))
    return extract_unsigned_integer (value_contents (arg),
                                     TYPE_LENGTH (type));
  else
    return extract_signed_integer (value_contents (arg),
                                   TYPE_LENGTH (type));
}


/* Return the alignment required by TYPE.  */
static int
alignment_of (struct type *type)
{
  int alignment;

  if (is_integer_like (type)
      || is_pointer_like (type)
      || TYPE_CODE (type) == TYPE_CODE_FLT)
    alignment = TYPE_LENGTH (type);
  else if (TYPE_CODE (type) == TYPE_CODE_STRUCT
           || TYPE_CODE (type) == TYPE_CODE_UNION)
    {
      int i;

      alignment = 1;
      for (i = 0; i < TYPE_NFIELDS (type); i++)
        {
          int field_alignment = alignment_of (TYPE_FIELD_TYPE (type, i));

          if (field_alignment > alignment)
            alignment = field_alignment;
        }
    }
  else
    alignment = 1;

  /* Check that everything we ever return is a power of two.  Lots of
     code doesn't want to deal with aligning things to arbitrary
     boundaries.  */
  gdb_assert ((alignment & (alignment - 1)) == 0);

  return alignment;
}


/* Put the actual parameter values pointed to by ARGS[0..NARGS-1] in
   place to be passed to a function, as specified by the "GNU/Linux
   for S/390 ELF Application Binary Interface Supplement".

   SP is the current stack pointer.  We must put arguments, links,
   padding, etc. whereever they belong, and return the new stack
   pointer value.
   
   If STRUCT_RETURN is non-zero, then the function we're calling is
   going to return a structure by value; STRUCT_ADDR is the address of
   a block we've allocated for it on the stack.

   Our caller has taken care of any type promotions needed to satisfy
   prototypes or the old K&R argument-passing rules.  */
static CORE_ADDR
s390_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		      struct regcache *regcache, CORE_ADDR bp_addr,
		      int nargs, struct value **args, CORE_ADDR sp,
		      int struct_return, CORE_ADDR struct_addr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  int word_size = gdbarch_ptr_bit (gdbarch) / 8;
  ULONGEST orig_sp;
  int i;

  /* If the i'th argument is passed as a reference to a copy, then
     copy_addr[i] is the address of the copy we made.  */
  CORE_ADDR *copy_addr = alloca (nargs * sizeof (CORE_ADDR));

  /* Build the reference-to-copy area.  */
  for (i = 0; i < nargs; i++)
    {
      struct value *arg = args[i];
      struct type *type = value_type (arg);
      unsigned length = TYPE_LENGTH (type);

      if (s390_function_arg_pass_by_reference (type))
        {
          sp -= length;
          sp = align_down (sp, alignment_of (type));
          write_memory (sp, value_contents (arg), length);
          copy_addr[i] = sp;
        }
    }

  /* Reserve space for the parameter area.  As a conservative
     simplification, we assume that everything will be passed on the
     stack.  Since every argument larger than 8 bytes will be 
     passed by reference, we use this simple upper bound.  */
  sp -= nargs * 8;

  /* After all that, make sure it's still aligned on an eight-byte
     boundary.  */
  sp = align_down (sp, 8);

  /* Finally, place the actual parameters, working from SP towards
     higher addresses.  The code above is supposed to reserve enough
     space for this.  */
  {
    int fr = 0;
    int gr = 2;
    CORE_ADDR starg = sp;

    /* A struct is returned using general register 2.  */
    if (struct_return)
      {
	regcache_cooked_write_unsigned (regcache, S390_R0_REGNUM + gr,
				        struct_addr);
	gr++;
      }

    for (i = 0; i < nargs; i++)
      {
        struct value *arg = args[i];
        struct type *type = value_type (arg);
        unsigned length = TYPE_LENGTH (type);

	if (s390_function_arg_pass_by_reference (type))
	  {
	    if (gr <= 6)
	      {
		regcache_cooked_write_unsigned (regcache, S390_R0_REGNUM + gr,
					        copy_addr[i]);
		gr++;
	      }
	    else
	      {
		write_memory_unsigned_integer (starg, word_size, copy_addr[i]);
		starg += word_size;
	      }
	  }
	else if (s390_function_arg_float (type))
	  {
	    /* The GNU/Linux for S/390 ABI uses FPRs 0 and 2 to pass arguments,
	       the GNU/Linux for zSeries ABI uses 0, 2, 4, and 6.  */
	    if (fr <= (tdep->abi == ABI_LINUX_S390 ? 2 : 6))
	      {
		/* When we store a single-precision value in an FP register,
		   it occupies the leftmost bits.  */
		regcache_cooked_write_part (regcache, S390_F0_REGNUM + fr,
					    0, length, value_contents (arg));
		fr += 2;
	      }
	    else
	      {
		/* When we store a single-precision value in a stack slot,
		   it occupies the rightmost bits.  */
		starg = align_up (starg + length, word_size);
                write_memory (starg - length, value_contents (arg), length);
	      }
	  }
	else if (s390_function_arg_integer (type) && length <= word_size)
	  {
	    if (gr <= 6)
	      {
		/* Integer arguments are always extended to word size.  */
		regcache_cooked_write_signed (regcache, S390_R0_REGNUM + gr,
					      extend_simple_arg (arg));
		gr++;
	      }
	    else
	      {
		/* Integer arguments are always extended to word size.  */
		write_memory_signed_integer (starg, word_size,
                                             extend_simple_arg (arg));
                starg += word_size;
	      }
	  }
	else if (s390_function_arg_integer (type) && length == 2*word_size)
	  {
	    if (gr <= 5)
	      {
		regcache_cooked_write (regcache, S390_R0_REGNUM + gr,
				       value_contents (arg));
		regcache_cooked_write (regcache, S390_R0_REGNUM + gr + 1,
				       value_contents (arg) + word_size);
		gr += 2;
	      }
	    else
	      {
		/* If we skipped r6 because we couldn't fit a DOUBLE_ARG
		   in it, then don't go back and use it again later.  */
		gr = 7;

		write_memory (starg, value_contents (arg), length);
		starg += length;
	      }
	  }
	else
	  internal_error (__FILE__, __LINE__, _("unknown argument type"));
      }
  }

  /* Allocate the standard frame areas: the register save area, the
     word reserved for the compiler (which seems kind of meaningless),
     and the back chain pointer.  */
  sp -= 16*word_size + 32;

  /* Store return address.  */
  regcache_cooked_write_unsigned (regcache, S390_RETADDR_REGNUM, bp_addr);
  
  /* Store updated stack pointer.  */
  regcache_cooked_write_unsigned (regcache, S390_SP_REGNUM, sp);

  /* We need to return the 'stack part' of the frame ID,
     which is actually the top of the register save area.  */
  return sp + 16*word_size + 32;
}

/* Assuming NEXT_FRAME->prev is a dummy, return the frame ID of that
   dummy frame.  The frame ID's base needs to match the TOS value
   returned by push_dummy_call, and the PC match the dummy frame's
   breakpoint.  */
static struct frame_id
s390_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  int word_size = gdbarch_ptr_bit (gdbarch) / 8;
  CORE_ADDR sp = s390_unwind_sp (gdbarch, next_frame);

  return frame_id_build (sp + 16*word_size + 32,
                         frame_pc_unwind (next_frame));
}

static CORE_ADDR
s390_frame_align (struct gdbarch *gdbarch, CORE_ADDR addr)
{
  /* Both the 32- and 64-bit ABI's say that the stack pointer should
     always be aligned on an eight-byte boundary.  */
  return (addr & -8);
}


/* Function return value access.  */

static enum return_value_convention
s390_return_value_convention (struct gdbarch *gdbarch, struct type *type)
{
  int length = TYPE_LENGTH (type);
  if (length > 8)
    return RETURN_VALUE_STRUCT_CONVENTION;

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
    case TYPE_CODE_ARRAY:
      return RETURN_VALUE_STRUCT_CONVENTION;

    default:
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
}

static enum return_value_convention
s390_return_value (struct gdbarch *gdbarch, struct type *type, 
		   struct regcache *regcache, gdb_byte *out,
		   const gdb_byte *in)
{
  int word_size = gdbarch_ptr_bit (gdbarch) / 8;
  int length = TYPE_LENGTH (type);
  enum return_value_convention rvc = 
			s390_return_value_convention (gdbarch, type);
  if (in)
    {
      switch (rvc)
	{
	case RETURN_VALUE_REGISTER_CONVENTION:
	  if (TYPE_CODE (type) == TYPE_CODE_FLT)
	    {
	      /* When we store a single-precision value in an FP register,
		 it occupies the leftmost bits.  */
	      regcache_cooked_write_part (regcache, S390_F0_REGNUM, 
					  0, length, in);
	    }
	  else if (length <= word_size)
	    {
	      /* Integer arguments are always extended to word size.  */
	      if (TYPE_UNSIGNED (type))
		regcache_cooked_write_unsigned (regcache, S390_R2_REGNUM,
			extract_unsigned_integer (in, length));
	      else
		regcache_cooked_write_signed (regcache, S390_R2_REGNUM,
			extract_signed_integer (in, length));
	    }
	  else if (length == 2*word_size)
	    {
	      regcache_cooked_write (regcache, S390_R2_REGNUM, in);
	      regcache_cooked_write (regcache, S390_R3_REGNUM, in + word_size);
	    }
	  else
	    internal_error (__FILE__, __LINE__, _("invalid return type"));
	  break;

	case RETURN_VALUE_STRUCT_CONVENTION:
	  error (_("Cannot set function return value."));
	  break;
	}
    }
  else if (out)
    {
      switch (rvc)
	{
	case RETURN_VALUE_REGISTER_CONVENTION:
	  if (TYPE_CODE (type) == TYPE_CODE_FLT)
	    {
	      /* When we store a single-precision value in an FP register,
		 it occupies the leftmost bits.  */
	      regcache_cooked_read_part (regcache, S390_F0_REGNUM, 
					 0, length, out);
	    }
	  else if (length <= word_size)
	    {
	      /* Integer arguments occupy the rightmost bits.  */
	      regcache_cooked_read_part (regcache, S390_R2_REGNUM, 
					 word_size - length, length, out);
	    }
	  else if (length == 2*word_size)
	    {
	      regcache_cooked_read (regcache, S390_R2_REGNUM, out);
	      regcache_cooked_read (regcache, S390_R3_REGNUM, out + word_size);
	    }
	  else
	    internal_error (__FILE__, __LINE__, _("invalid return type"));
	  break;

	case RETURN_VALUE_STRUCT_CONVENTION:
	  error (_("Function return value unknown."));
	  break;
	}
    }

  return rvc;
}


/* Breakpoints.  */

static const gdb_byte *
s390_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  static const gdb_byte breakpoint[] = { 0x0, 0x1 };

  *lenptr = sizeof (breakpoint);
  return breakpoint;
}


/* Address handling.  */

static CORE_ADDR
s390_addr_bits_remove (CORE_ADDR addr)
{
  return addr & 0x7fffffff;
}

static int
s390_address_class_type_flags (int byte_size, int dwarf2_addr_class)
{
  if (byte_size == 4)
    return TYPE_FLAG_ADDRESS_CLASS_1;
  else
    return 0;
}

static const char *
s390_address_class_type_flags_to_name (struct gdbarch *gdbarch, int type_flags)
{
  if (type_flags & TYPE_FLAG_ADDRESS_CLASS_1)
    return "mode32";
  else
    return NULL;
}

static int
s390_address_class_name_to_type_flags (struct gdbarch *gdbarch, const char *name,
				       int *type_flags_ptr)
{
  if (strcmp (name, "mode32") == 0)
    {
      *type_flags_ptr = TYPE_FLAG_ADDRESS_CLASS_1;
      return 1;
    }
  else
    return 0;
}

/* Set up gdbarch struct.  */

static struct gdbarch *
s390_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  struct gdbarch_tdep *tdep;

  /* First see if there is already a gdbarch that can satisfy the request.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  /* None found: is the request for a s390 architecture? */
  if (info.bfd_arch_info->arch != bfd_arch_s390)
    return NULL;		/* No; then it's not for us.  */

  /* Yes: create a new gdbarch for the specified machine type.  */
  tdep = XCALLOC (1, struct gdbarch_tdep);
  gdbarch = gdbarch_alloc (&info, tdep);

  set_gdbarch_believe_pcc_promotion (gdbarch, 0);
  set_gdbarch_char_signed (gdbarch, 0);

  /* Amount PC must be decremented by after a breakpoint.  This is
     often the number of bytes returned by gdbarch_breakpoint_from_pc but not
     always.  */
  set_gdbarch_decr_pc_after_break (gdbarch, 2);
  /* Stack grows downward.  */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_breakpoint_from_pc (gdbarch, s390_breakpoint_from_pc);
  set_gdbarch_skip_prologue (gdbarch, s390_skip_prologue);
  set_gdbarch_in_function_epilogue_p (gdbarch, s390_in_function_epilogue_p);

  set_gdbarch_pc_regnum (gdbarch, S390_PC_REGNUM);
  set_gdbarch_sp_regnum (gdbarch, S390_SP_REGNUM);
  set_gdbarch_fp0_regnum (gdbarch, S390_F0_REGNUM);
  set_gdbarch_num_regs (gdbarch, S390_NUM_REGS);
  set_gdbarch_num_pseudo_regs (gdbarch, S390_NUM_PSEUDO_REGS);
  set_gdbarch_register_name (gdbarch, s390_register_name);
  set_gdbarch_register_type (gdbarch, s390_register_type);
  set_gdbarch_stab_reg_to_regnum (gdbarch, s390_dwarf_reg_to_regnum);
  set_gdbarch_dwarf_reg_to_regnum (gdbarch, s390_dwarf_reg_to_regnum);
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, s390_dwarf_reg_to_regnum);
  set_gdbarch_value_from_register (gdbarch, s390_value_from_register);
  set_gdbarch_register_reggroup_p (gdbarch, s390_register_reggroup_p);
  set_gdbarch_regset_from_core_section (gdbarch,
                                        s390_regset_from_core_section);

  /* Inferior function calls.  */
  set_gdbarch_push_dummy_call (gdbarch, s390_push_dummy_call);
  set_gdbarch_unwind_dummy_id (gdbarch, s390_unwind_dummy_id);
  set_gdbarch_frame_align (gdbarch, s390_frame_align);
  set_gdbarch_return_value (gdbarch, s390_return_value);

  /* Frame handling.  */
  dwarf2_frame_set_init_reg (gdbarch, s390_dwarf2_frame_init_reg);
  frame_unwind_append_sniffer (gdbarch, dwarf2_frame_sniffer);
  frame_base_append_sniffer (gdbarch, dwarf2_frame_base_sniffer);
  frame_unwind_append_sniffer (gdbarch, s390_stub_frame_sniffer);
  frame_unwind_append_sniffer (gdbarch, s390_sigtramp_frame_sniffer);
  frame_unwind_append_sniffer (gdbarch, s390_frame_sniffer);
  frame_base_set_default (gdbarch, &s390_frame_base);
  set_gdbarch_unwind_pc (gdbarch, s390_unwind_pc);
  set_gdbarch_unwind_sp (gdbarch, s390_unwind_sp);

  switch (info.bfd_arch_info->mach)
    {
    case bfd_mach_s390_31:
      tdep->abi = ABI_LINUX_S390;

      tdep->gregset = &s390_gregset;
      tdep->sizeof_gregset = s390_sizeof_gregset;
      tdep->fpregset = &s390_fpregset;
      tdep->sizeof_fpregset = s390_sizeof_fpregset;

      set_gdbarch_addr_bits_remove (gdbarch, s390_addr_bits_remove);
      set_gdbarch_pseudo_register_read (gdbarch, s390_pseudo_register_read);
      set_gdbarch_pseudo_register_write (gdbarch, s390_pseudo_register_write);
      set_solib_svr4_fetch_link_map_offsets
	(gdbarch, svr4_ilp32_fetch_link_map_offsets);

      break;
    case bfd_mach_s390_64:
      tdep->abi = ABI_LINUX_ZSERIES;

      tdep->gregset = &s390x_gregset;
      tdep->sizeof_gregset = s390x_sizeof_gregset;
      tdep->fpregset = &s390_fpregset;
      tdep->sizeof_fpregset = s390_sizeof_fpregset;

      set_gdbarch_long_bit (gdbarch, 64);
      set_gdbarch_long_long_bit (gdbarch, 64);
      set_gdbarch_ptr_bit (gdbarch, 64);
      set_gdbarch_pseudo_register_read (gdbarch, s390x_pseudo_register_read);
      set_gdbarch_pseudo_register_write (gdbarch, s390x_pseudo_register_write);
      set_solib_svr4_fetch_link_map_offsets
	(gdbarch, svr4_lp64_fetch_link_map_offsets);
      set_gdbarch_address_class_type_flags (gdbarch,
                                            s390_address_class_type_flags);
      set_gdbarch_address_class_type_flags_to_name (gdbarch,
                                                    s390_address_class_type_flags_to_name);
      set_gdbarch_address_class_name_to_type_flags (gdbarch,
                                                    s390_address_class_name_to_type_flags);
      break;
    }

  set_gdbarch_print_insn (gdbarch, print_insn_s390);

  set_gdbarch_skip_trampoline_code (gdbarch, find_solib_trampoline_target);

  /* Enable TLS support.  */
  set_gdbarch_fetch_tls_load_module_address (gdbarch,
                                             svr4_fetch_objfile_link_map);

  return gdbarch;
}



extern initialize_file_ftype _initialize_s390_tdep; /* -Wmissing-prototypes */

void
_initialize_s390_tdep (void)
{

  /* Hook us into the gdbarch mechanism.  */
  register_gdbarch_init (bfd_arch_s390, s390_gdbarch_init);
}
