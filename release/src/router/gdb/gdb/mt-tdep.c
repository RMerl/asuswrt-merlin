/* Target-dependent code for Morpho mt processor, for GDB.

   Copyright (C) 2005, 2007 Free Software Foundation, Inc.

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

/* Contributed by Michael Snyder, msnyder@redhat.com.  */

#include "defs.h"
#include "frame.h"
#include "frame-unwind.h"
#include "frame-base.h"
#include "symtab.h"
#include "dis-asm.h"
#include "arch-utils.h"
#include "gdbtypes.h"
#include "gdb_string.h"
#include "regcache.h"
#include "reggroups.h"
#include "gdbcore.h"
#include "trad-frame.h"
#include "inferior.h"
#include "dwarf2-frame.h"
#include "infcall.h"
#include "gdb_assert.h"

enum mt_arch_constants
{
  MT_MAX_STRUCT_SIZE = 16
};

enum mt_gdb_regnums
{
  MT_R0_REGNUM,			/* 32 bit regs.  */
  MT_R1_REGNUM,
  MT_1ST_ARGREG = MT_R1_REGNUM,
  MT_R2_REGNUM,
  MT_R3_REGNUM,
  MT_R4_REGNUM,
  MT_LAST_ARGREG = MT_R4_REGNUM,
  MT_R5_REGNUM,
  MT_R6_REGNUM,
  MT_R7_REGNUM,
  MT_R8_REGNUM,
  MT_R9_REGNUM,
  MT_R10_REGNUM,
  MT_R11_REGNUM,
  MT_R12_REGNUM,
  MT_FP_REGNUM = MT_R12_REGNUM,
  MT_R13_REGNUM,
  MT_SP_REGNUM = MT_R13_REGNUM,
  MT_R14_REGNUM,
  MT_RA_REGNUM = MT_R14_REGNUM,
  MT_R15_REGNUM,
  MT_IRA_REGNUM = MT_R15_REGNUM,
  MT_PC_REGNUM,

  /* Interrupt Enable pseudo-register, exported by SID.  */
  MT_INT_ENABLE_REGNUM,
  /* End of CPU regs.  */

  MT_NUM_CPU_REGS,

  /* Co-processor registers.  */
  MT_COPRO_REGNUM = MT_NUM_CPU_REGS,	/* 16 bit regs.  */
  MT_CPR0_REGNUM,
  MT_CPR1_REGNUM,
  MT_CPR2_REGNUM,
  MT_CPR3_REGNUM,
  MT_CPR4_REGNUM,
  MT_CPR5_REGNUM,
  MT_CPR6_REGNUM,
  MT_CPR7_REGNUM,
  MT_CPR8_REGNUM,
  MT_CPR9_REGNUM,
  MT_CPR10_REGNUM,
  MT_CPR11_REGNUM,
  MT_CPR12_REGNUM,
  MT_CPR13_REGNUM,
  MT_CPR14_REGNUM,
  MT_CPR15_REGNUM,
  MT_BYPA_REGNUM,		/* 32 bit regs.  */
  MT_BYPB_REGNUM,
  MT_BYPC_REGNUM,
  MT_FLAG_REGNUM,
  MT_CONTEXT_REGNUM,		/* 38 bits (treat as array of
				   six bytes).  */
  MT_MAC_REGNUM,			/* 32 bits.  */
  MT_Z1_REGNUM,			/* 16 bits.  */
  MT_Z2_REGNUM,			/* 16 bits.  */
  MT_ICHANNEL_REGNUM,		/* 32 bits.  */
  MT_ISCRAMB_REGNUM,		/* 32 bits.  */
  MT_QSCRAMB_REGNUM,		/* 32 bits.  */
  MT_OUT_REGNUM,			/* 16 bits.  */
  MT_EXMAC_REGNUM,		/* 32 bits (8 used).  */
  MT_QCHANNEL_REGNUM,		/* 32 bits.  */
  MT_ZI2_REGNUM,                /* 16 bits.  */
  MT_ZQ2_REGNUM,                /* 16 bits.  */
  MT_CHANNEL2_REGNUM,           /* 32 bits.  */
  MT_ISCRAMB2_REGNUM,           /* 32 bits.  */
  MT_QSCRAMB2_REGNUM,           /* 32 bits.  */
  MT_QCHANNEL2_REGNUM,          /* 32 bits.  */

  /* Number of real registers.  */
  MT_NUM_REGS,

  /* Pseudo-registers.  */
  MT_COPRO_PSEUDOREG_REGNUM = MT_NUM_REGS,
  MT_MAC_PSEUDOREG_REGNUM,
  MT_COPRO_PSEUDOREG_ARRAY,

  MT_COPRO_PSEUDOREG_DIM_1 = 2,
  MT_COPRO_PSEUDOREG_DIM_2 = 8,
  /* The number of pseudo-registers for each coprocessor.  These
     include the real coprocessor registers, the pseudo-registe for
     the coprocessor number, and the pseudo-register for the MAC.  */
  MT_COPRO_PSEUDOREG_REGS = MT_NUM_REGS - MT_NUM_CPU_REGS + 2,
  /* The register number of the MAC, relative to a given coprocessor.  */
  MT_COPRO_PSEUDOREG_MAC_REGNUM = MT_COPRO_PSEUDOREG_REGS - 1,

  /* Two pseudo-regs ('coprocessor' and 'mac').  */
  MT_NUM_PSEUDO_REGS = 2 + (MT_COPRO_PSEUDOREG_REGS
			    * MT_COPRO_PSEUDOREG_DIM_1
			    * MT_COPRO_PSEUDOREG_DIM_2)
};

/* Return name of register number specified by REGNUM.  */

static const char *
mt_register_name (int regnum)
{
  static const char *const register_names[] = {
    /* CPU regs.  */
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "pc", "IE",
    /* Co-processor regs.  */
    "",				/* copro register.  */
    "cr0", "cr1", "cr2", "cr3", "cr4", "cr5", "cr6", "cr7",
    "cr8", "cr9", "cr10", "cr11", "cr12", "cr13", "cr14", "cr15",
    "bypa", "bypb", "bypc", "flag", "context", "" /* mac.  */ , "z1", "z2",
    "Ichannel", "Iscramb", "Qscramb", "out", "" /* ex-mac.  */ , "Qchannel",
    "zi2", "zq2", "Ichannel2", "Iscramb2", "Qscramb2", "Qchannel2",
    /* Pseudo-registers.  */
    "coprocessor", "MAC"
  };
  static const char *array_names[MT_COPRO_PSEUDOREG_REGS
				 * MT_COPRO_PSEUDOREG_DIM_1
				 * MT_COPRO_PSEUDOREG_DIM_2];

  if (regnum < 0)
    return "";
  if (regnum < ARRAY_SIZE (register_names))
    return register_names[regnum];
  if (array_names[regnum - MT_COPRO_PSEUDOREG_ARRAY])
    return array_names[regnum - MT_COPRO_PSEUDOREG_ARRAY];
  
  {
    char *name;
    const char *stub;
    unsigned dim_1;
    unsigned dim_2;
    unsigned index;
    
    regnum -= MT_COPRO_PSEUDOREG_ARRAY;
    index = regnum % MT_COPRO_PSEUDOREG_REGS;
    dim_2 = (regnum / MT_COPRO_PSEUDOREG_REGS) % MT_COPRO_PSEUDOREG_DIM_2;
    dim_1 = ((regnum / MT_COPRO_PSEUDOREG_REGS / MT_COPRO_PSEUDOREG_DIM_2)
	     %  MT_COPRO_PSEUDOREG_DIM_1);
    
    if (index == MT_COPRO_PSEUDOREG_MAC_REGNUM)
      stub = register_names[MT_MAC_PSEUDOREG_REGNUM];
    else if (index >= MT_NUM_REGS - MT_CPR0_REGNUM)
      stub = "";
    else
      stub = register_names[index + MT_CPR0_REGNUM];
    if (!*stub)
      {
	array_names[regnum] = stub;
	return stub;
      }
    name = xmalloc (30);
    sprintf (name, "copro_%d_%d_%s", dim_1, dim_2, stub);
    array_names[regnum] = name;
    return name;
  }
}

/* Return the type of a coprocessor register.  */

static struct type *
mt_copro_register_type (struct gdbarch *arch, int regnum)
{
  switch (regnum)
    {
    case MT_INT_ENABLE_REGNUM:
    case MT_ICHANNEL_REGNUM:
    case MT_QCHANNEL_REGNUM:
    case MT_ISCRAMB_REGNUM:
    case MT_QSCRAMB_REGNUM:
      return builtin_type_int32;
    case MT_BYPA_REGNUM:
    case MT_BYPB_REGNUM:
    case MT_BYPC_REGNUM:
    case MT_Z1_REGNUM:
    case MT_Z2_REGNUM:
    case MT_OUT_REGNUM:
    case MT_ZI2_REGNUM:
    case MT_ZQ2_REGNUM:
      return builtin_type_int16;
    case MT_EXMAC_REGNUM:
    case MT_MAC_REGNUM:
      return builtin_type_uint32;
    case MT_CONTEXT_REGNUM:
      return builtin_type_long_long;
    case MT_FLAG_REGNUM:
      return builtin_type_unsigned_char;
    default:
      if (regnum >= MT_CPR0_REGNUM && regnum <= MT_CPR15_REGNUM)
	return builtin_type_int16;
      else if (regnum == MT_CPR0_REGNUM + MT_COPRO_PSEUDOREG_MAC_REGNUM)
	{
	  if (gdbarch_bfd_arch_info (arch)->mach == bfd_mach_mrisc2
	      || gdbarch_bfd_arch_info (arch)->mach == bfd_mach_ms2)
	    return builtin_type_uint64;
	  else
	    return builtin_type_uint32;
	}
      else
	return builtin_type_uint32;
    }
}

/* Given ARCH and a register number specified by REGNUM, return the
   type of that register.  */

static struct type *
mt_register_type (struct gdbarch *arch, int regnum)
{
  static struct type *void_func_ptr = NULL;
  static struct type *void_ptr = NULL;
  static struct type *copro_type;

  if (regnum >= 0 && regnum < MT_NUM_REGS + MT_NUM_PSEUDO_REGS)
    {
      if (void_func_ptr == NULL)
	{
	  struct type *temp;

	  void_ptr = lookup_pointer_type (builtin_type_void);
	  void_func_ptr =
	    lookup_pointer_type (lookup_function_type (builtin_type_void));
	  temp = create_range_type (NULL, builtin_type_unsigned_int, 0, 1);
	  copro_type = create_array_type (NULL, builtin_type_int16, temp);
	}
      switch (regnum)
	{
	case MT_PC_REGNUM:
	case MT_RA_REGNUM:
	case MT_IRA_REGNUM:
	  return void_func_ptr;
	case MT_SP_REGNUM:
	case MT_FP_REGNUM:
	  return void_ptr;
	case MT_COPRO_REGNUM:
	case MT_COPRO_PSEUDOREG_REGNUM:
	  return copro_type;
	case MT_MAC_PSEUDOREG_REGNUM:
	  return mt_copro_register_type (arch,
					 MT_CPR0_REGNUM
					 + MT_COPRO_PSEUDOREG_MAC_REGNUM);
	default:
	  if (regnum >= MT_R0_REGNUM && regnum <= MT_R15_REGNUM)
	    return builtin_type_int32;
	  else if (regnum < MT_COPRO_PSEUDOREG_ARRAY)
	    return mt_copro_register_type (arch, regnum);
	  else
	    {
	      regnum -= MT_COPRO_PSEUDOREG_ARRAY;
	      regnum %= MT_COPRO_PSEUDOREG_REGS;
	      regnum += MT_CPR0_REGNUM;
	      return mt_copro_register_type (arch, regnum);
	    }
	}
    }
  internal_error (__FILE__, __LINE__,
		  _("mt_register_type: illegal register number %d"), regnum);
}

/* Return true if register REGNUM is a member of the register group
   specified by GROUP.  */

static int
mt_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
			 struct reggroup *group)
{
  /* Groups of registers that can be displayed via "info reg".  */
  if (group == all_reggroup)
    return (regnum >= 0
	    && regnum < MT_NUM_REGS + MT_NUM_PSEUDO_REGS
	    && mt_register_name (regnum)[0] != '\0');

  if (group == general_reggroup)
    return (regnum >= MT_R0_REGNUM && regnum <= MT_R15_REGNUM);

  if (group == float_reggroup)
    return 0;			/* No float regs.  */

  if (group == vector_reggroup)
    return 0;			/* No vector regs.  */

  /* For any that are not handled above.  */
  return default_register_reggroup_p (gdbarch, regnum, group);
}

/* Return the return value convention used for a given type TYPE.
   Optionally, fetch or set the return value via READBUF or
   WRITEBUF respectively using REGCACHE for the register
   values.  */

static enum return_value_convention
mt_return_value (struct gdbarch *gdbarch, struct type *type,
		  struct regcache *regcache, gdb_byte *readbuf,
		  const gdb_byte *writebuf)
{
  if (TYPE_LENGTH (type) > 4)
    {
      /* Return values > 4 bytes are returned in memory, 
         pointed to by R11.  */
      if (readbuf)
	{
	  ULONGEST addr;

	  regcache_cooked_read_unsigned (regcache, MT_R11_REGNUM, &addr);
	  read_memory (addr, readbuf, TYPE_LENGTH (type));
	}

      if (writebuf)
	{
	  ULONGEST addr;

	  regcache_cooked_read_unsigned (regcache, MT_R11_REGNUM, &addr);
	  write_memory (addr, writebuf, TYPE_LENGTH (type));
	}

      return RETURN_VALUE_ABI_RETURNS_ADDRESS;
    }
  else
    {
      if (readbuf)
	{
	  ULONGEST temp;

	  /* Return values of <= 4 bytes are returned in R11.  */
	  regcache_cooked_read_unsigned (regcache, MT_R11_REGNUM, &temp);
	  store_unsigned_integer (readbuf, TYPE_LENGTH (type), temp);
	}

      if (writebuf)
	{
	  if (TYPE_LENGTH (type) < 4)
	    {
	      gdb_byte buf[4];
	      /* Add leading zeros to the value.  */
	      memset (buf, 0, sizeof (buf));
	      memcpy (buf + sizeof (buf) - TYPE_LENGTH (type),
		      writebuf, TYPE_LENGTH (type));
	      regcache_cooked_write (regcache, MT_R11_REGNUM, buf);
	    }
	  else			/* (TYPE_LENGTH (type) == 4 */
	    regcache_cooked_write (regcache, MT_R11_REGNUM, writebuf);
	}

      return RETURN_VALUE_REGISTER_CONVENTION;
    }
}

/* If the input address, PC, is in a function prologue, return the
   address of the end of the prologue, otherwise return the input
   address.

   Note:  PC is likely to be the function start, since this function
   is mainly used for advancing a breakpoint to the first line, or
   stepping to the first line when we have stepped into a function
   call.  */

static CORE_ADDR
mt_skip_prologue (CORE_ADDR pc)
{
  CORE_ADDR func_addr = 0, func_end = 0;
  char *func_name;
  unsigned long instr;

  if (find_pc_partial_function (pc, &func_name, &func_addr, &func_end))
    {
      struct symtab_and_line sal;
      struct symbol *sym;

      /* Found a function.  */
      sym = lookup_symbol (func_name, NULL, VAR_DOMAIN, NULL, NULL);
      if (sym && SYMBOL_LANGUAGE (sym) != language_asm)
	{
	  /* Don't use this trick for assembly source files.  */
	  sal = find_pc_line (func_addr, 0);

	  if (sal.end && sal.end < func_end)
	    {
	      /* Found a line number, use it as end of prologue.  */
	      return sal.end;
	    }
	}
    }

  /* No function symbol, or no line symbol.  Use prologue scanning method.  */
  for (;; pc += 4)
    {
      instr = read_memory_unsigned_integer (pc, 4);
      if (instr == 0x12000000)	/* nop */
	continue;
      if (instr == 0x12ddc000)	/* copy sp into fp */
	continue;
      instr >>= 16;
      if (instr == 0x05dd)	/* subi sp, sp, imm */
	continue;
      if (instr >= 0x43c0 && instr <= 0x43df)	/* push */
	continue;
      /* Not an obvious prologue instruction.  */
      break;
    }

  return pc;
}

/* The breakpoint instruction must be the same size as the smallest
   instruction in the instruction set.

   The BP for ms1 is defined as 0x68000000 (BREAK).
   The BP for ms2 is defined as 0x69000000 (illegal)  */

static const gdb_byte *
mt_breakpoint_from_pc (CORE_ADDR *bp_addr, int *bp_size)
{
  static gdb_byte ms1_breakpoint[] = { 0x68, 0, 0, 0 };
  static gdb_byte ms2_breakpoint[] = { 0x69, 0, 0, 0 };

  *bp_size = 4;
  if (gdbarch_bfd_arch_info (current_gdbarch)->mach == bfd_mach_ms2)
    return ms2_breakpoint;
  
  return ms1_breakpoint;
}

/* Select the correct coprocessor register bank.  Return the pseudo
   regnum we really want to read.  */

static int
mt_select_coprocessor (struct gdbarch *gdbarch,
			struct regcache *regcache, int regno)
{
  unsigned index, base;
  gdb_byte copro[4];

  /* Get the copro pseudo regnum. */
  regcache_raw_read (regcache, MT_COPRO_REGNUM, copro);
  base = (extract_signed_integer (&copro[0], 2) * MT_COPRO_PSEUDOREG_DIM_2
	  + extract_signed_integer (&copro[2], 2));

  regno -= MT_COPRO_PSEUDOREG_ARRAY;
  index = regno % MT_COPRO_PSEUDOREG_REGS;
  regno /= MT_COPRO_PSEUDOREG_REGS;
  if (base != regno)
    {
      /* Select the correct coprocessor register bank.  Invalidate the
	 coprocessor register cache.  */
      unsigned ix;

      store_signed_integer (&copro[0], 2, regno / MT_COPRO_PSEUDOREG_DIM_2);
      store_signed_integer (&copro[2], 2, regno % MT_COPRO_PSEUDOREG_DIM_2);
      regcache_raw_write (regcache, MT_COPRO_REGNUM, copro);
      
      /* We must flush the cache, as it is now invalid.  */
      for (ix = MT_NUM_CPU_REGS; ix != MT_NUM_REGS; ix++)
	regcache_invalidate (regcache, ix);
    }
  
  return index;
}

/* Fetch the pseudo registers:

   There are two regular pseudo-registers:
   1) The 'coprocessor' pseudo-register (which mirrors the 
   "real" coprocessor register sent by the target), and
   2) The 'MAC' pseudo-register (which represents the union
   of the original 32 bit target MAC register and the new
   8-bit extended-MAC register).

   Additionally there is an array of coprocessor registers which track
   the coprocessor registers for each coprocessor.  */

static void
mt_pseudo_register_read (struct gdbarch *gdbarch,
			  struct regcache *regcache, int regno, gdb_byte *buf)
{
  switch (regno)
    {
    case MT_COPRO_REGNUM:
    case MT_COPRO_PSEUDOREG_REGNUM:
      regcache_raw_read (regcache, MT_COPRO_REGNUM, buf);
      break;
    case MT_MAC_REGNUM:
    case MT_MAC_PSEUDOREG_REGNUM:
      if (gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_mrisc2
	  || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_ms2)
	{
	  ULONGEST oldmac = 0, ext_mac = 0;
	  ULONGEST newmac;

	  regcache_cooked_read_unsigned (regcache, MT_MAC_REGNUM, &oldmac);
	  regcache_cooked_read_unsigned (regcache, MT_EXMAC_REGNUM, &ext_mac);
	  newmac =
	    (oldmac & 0xffffffff) | ((long long) (ext_mac & 0xff) << 32);
	  store_signed_integer (buf, 8, newmac);
	}
      else
	regcache_raw_read (regcache, MT_MAC_REGNUM, buf);
      break;
    default:
      {
	unsigned index = mt_select_coprocessor (gdbarch, regcache, regno);
	
	if (index == MT_COPRO_PSEUDOREG_MAC_REGNUM)
	  mt_pseudo_register_read (gdbarch, regcache,
				   MT_MAC_PSEUDOREG_REGNUM, buf);
	else if (index < MT_NUM_REGS - MT_CPR0_REGNUM)
	  regcache_raw_read (regcache, index + MT_CPR0_REGNUM, buf);
      }
      break;
    }
}

/* Write the pseudo registers:

   Mt pseudo-registers are stored directly to the target.  The
   'coprocessor' register is special, because when it is modified, all
   the other coprocessor regs must be flushed from the reg cache.  */

static void
mt_pseudo_register_write (struct gdbarch *gdbarch,
			   struct regcache *regcache,
			   int regno, const gdb_byte *buf)
{
  int i;

  switch (regno)
    {
    case MT_COPRO_REGNUM:
    case MT_COPRO_PSEUDOREG_REGNUM:
      regcache_raw_write (regcache, MT_COPRO_REGNUM, buf);
      for (i = MT_NUM_CPU_REGS; i < MT_NUM_REGS; i++)
	regcache_invalidate (regcache, i);
      break;
    case MT_MAC_REGNUM:
    case MT_MAC_PSEUDOREG_REGNUM:
      if (gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_mrisc2
	  || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_ms2)
	{
	  /* The 8-byte MAC pseudo-register must be broken down into two
	     32-byte registers.  */
	  unsigned int oldmac, ext_mac;
	  ULONGEST newmac;

	  newmac = extract_unsigned_integer (buf, 8);
	  oldmac = newmac & 0xffffffff;
	  ext_mac = (newmac >> 32) & 0xff;
	  regcache_cooked_write_unsigned (regcache, MT_MAC_REGNUM, oldmac);
	  regcache_cooked_write_unsigned (regcache, MT_EXMAC_REGNUM, ext_mac);
	}
      else
	regcache_raw_write (regcache, MT_MAC_REGNUM, buf);
      break;
    default:
      {
	unsigned index = mt_select_coprocessor (gdbarch, regcache, regno);
	
	if (index == MT_COPRO_PSEUDOREG_MAC_REGNUM)
	  mt_pseudo_register_write (gdbarch, regcache,
				    MT_MAC_PSEUDOREG_REGNUM, buf);
	else if (index < MT_NUM_REGS - MT_CPR0_REGNUM)
	  regcache_raw_write (regcache, index + MT_CPR0_REGNUM, buf);
      }
      break;
    }
}

static CORE_ADDR
mt_frame_align (struct gdbarch *gdbarch, CORE_ADDR sp)
{
  /* Register size is 4 bytes.  */
  return align_down (sp, 4);
}

/* Implements the "info registers" command.   When ``all'' is non-zero,
   the coprocessor registers will be printed in addition to the rest
   of the registers.  */

static void
mt_registers_info (struct gdbarch *gdbarch,
		    struct ui_file *file,
		    struct frame_info *frame, int regnum, int all)
{
  if (regnum == -1)
    {
      int lim;

      lim = all ? MT_NUM_REGS : MT_NUM_CPU_REGS;

      for (regnum = 0; regnum < lim; regnum++)
	{
	  /* Don't display the Qchannel register since it will be displayed
	     along with Ichannel.  (See below.)  */
	  if (regnum == MT_QCHANNEL_REGNUM)
	    continue;

	  mt_registers_info (gdbarch, file, frame, regnum, all);

	  /* Display the Qchannel register immediately after Ichannel.  */
	  if (regnum == MT_ICHANNEL_REGNUM)
	    mt_registers_info (gdbarch, file, frame, MT_QCHANNEL_REGNUM, all);
	}
    }
  else
    {
      if (regnum == MT_EXMAC_REGNUM)
	return;
      else if (regnum == MT_CONTEXT_REGNUM)
	{
	  /* Special output handling for 38-bit context register.  */
	  unsigned char *buff;
	  unsigned int *bytes, i, regsize;

	  regsize = register_size (gdbarch, regnum);

	  buff = alloca (regsize);
	  bytes = alloca (regsize * sizeof (*bytes));

	  frame_register_read (frame, regnum, buff);

	  fputs_filtered (gdbarch_register_name
			  (current_gdbarch, regnum), file);
	  print_spaces_filtered (15 - strlen (gdbarch_register_name
					        (current_gdbarch, regnum)),
				 file);
	  fputs_filtered ("0x", file);

	  for (i = 0; i < regsize; i++)
	    fprintf_filtered (file, "%02x", (unsigned int)
			      extract_unsigned_integer (buff + i, 1));
	  fputs_filtered ("\t", file);
	  print_longest (file, 'd', 0,
			 extract_unsigned_integer (buff, regsize));
	  fputs_filtered ("\n", file);
	}
      else if (regnum == MT_COPRO_REGNUM
               || regnum == MT_COPRO_PSEUDOREG_REGNUM)
	{
	  /* Special output handling for the 'coprocessor' register.  */
	  gdb_byte *buf;

	  buf = alloca (register_size (gdbarch, MT_COPRO_REGNUM));
	  frame_register_read (frame, MT_COPRO_REGNUM, buf);
	  /* And print.  */
	  regnum = MT_COPRO_PSEUDOREG_REGNUM;
	  fputs_filtered (gdbarch_register_name (current_gdbarch, regnum),
			  file);
	  print_spaces_filtered (15 - strlen (gdbarch_register_name
					        (current_gdbarch, regnum)),
				 file);
	  val_print (register_type (gdbarch, regnum), buf,
		     0, 0, file, 0, 1, 0, Val_no_prettyprint);
	  fputs_filtered ("\n", file);
	}
      else if (regnum == MT_MAC_REGNUM || regnum == MT_MAC_PSEUDOREG_REGNUM)
	{
	  ULONGEST oldmac, ext_mac, newmac;
	  gdb_byte buf[3 * sizeof (LONGEST)];

	  /* Get the two "real" mac registers.  */
	  frame_register_read (frame, MT_MAC_REGNUM, buf);
	  oldmac = extract_unsigned_integer
	    (buf, register_size (gdbarch, MT_MAC_REGNUM));
	  if (gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_mrisc2
	      || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_ms2)
	    {
	      frame_register_read (frame, MT_EXMAC_REGNUM, buf);
	      ext_mac = extract_unsigned_integer
		(buf, register_size (gdbarch, MT_EXMAC_REGNUM));
	    }
	  else
	    ext_mac = 0;

	  /* Add them together.  */
	  newmac = (oldmac & 0xffffffff) + ((ext_mac & 0xff) << 32);

	  /* And print.  */
	  regnum = MT_MAC_PSEUDOREG_REGNUM;
	  fputs_filtered (gdbarch_register_name (current_gdbarch, regnum),
			  file);
	  print_spaces_filtered (15 - strlen (gdbarch_register_name
					      (current_gdbarch, regnum)),
				 file);
	  fputs_filtered ("0x", file);
	  print_longest (file, 'x', 0, newmac);
	  fputs_filtered ("\t", file);
	  print_longest (file, 'u', 0, newmac);
	  fputs_filtered ("\n", file);
	}
      else
	default_print_registers_info (gdbarch, file, frame, regnum, all);
    }
}

/* Set up the callee's arguments for an inferior function call.  The
   arguments are pushed on the stack or are placed in registers as
   appropriate.  It also sets up the return address (which points to
   the call dummy breakpoint).

   Returns the updated (and aligned) stack pointer.  */

static CORE_ADDR
mt_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		     struct regcache *regcache, CORE_ADDR bp_addr,
		     int nargs, struct value **args, CORE_ADDR sp,
		     int struct_return, CORE_ADDR struct_addr)
{
#define wordsize 4
  gdb_byte buf[MT_MAX_STRUCT_SIZE];
  int argreg = MT_1ST_ARGREG;
  int split_param_len = 0;
  int stack_dest = sp;
  int slacklen;
  int typelen;
  int i, j;

  /* First handle however many args we can fit into MT_1ST_ARGREG thru
     MT_LAST_ARGREG.  */
  for (i = 0; i < nargs && argreg <= MT_LAST_ARGREG; i++)
    {
      const gdb_byte *val;
      typelen = TYPE_LENGTH (value_type (args[i]));
      switch (typelen)
	{
	case 1:
	case 2:
	case 3:
	case 4:
	  regcache_cooked_write_unsigned (regcache, argreg++,
					  extract_unsigned_integer
					  (value_contents (args[i]),
					   wordsize));
	  break;
	case 8:
	case 12:
	case 16:
	  val = value_contents (args[i]);
	  while (typelen > 0)
	    {
	      if (argreg <= MT_LAST_ARGREG)
		{
		  /* This word of the argument is passed in a register.  */
		  regcache_cooked_write_unsigned (regcache, argreg++,
						  extract_unsigned_integer
						  (val, wordsize));
		  typelen -= wordsize;
		  val += wordsize;
		}
	      else
		{
		  /* Remainder of this arg must be passed on the stack
		     (deferred to do later).  */
		  split_param_len = typelen;
		  memcpy (buf, val, typelen);
		  break;	/* No more args can be handled in regs.  */
		}
	    }
	  break;
	default:
	  /* By reverse engineering of gcc output, args bigger than
	     16 bytes go on the stack, and their address is passed
	     in the argreg.  */
	  stack_dest -= typelen;
	  write_memory (stack_dest, value_contents (args[i]), typelen);
	  regcache_cooked_write_unsigned (regcache, argreg++, stack_dest);
	  break;
	}
    }

  /* Next, the rest of the arguments go onto the stack, in reverse order.  */
  for (j = nargs - 1; j >= i; j--)
    {
      gdb_byte *val;
      
      /* Right-justify the value in an aligned-length buffer.  */
      typelen = TYPE_LENGTH (value_type (args[j]));
      slacklen = (wordsize - (typelen % wordsize)) % wordsize;
      val = alloca (typelen + slacklen);
      memcpy (val, value_contents (args[j]), typelen);
      memset (val + typelen, 0, slacklen);
      /* Now write this data to the stack.  */
      stack_dest -= typelen + slacklen;
      write_memory (stack_dest, val, typelen + slacklen);
    }

  /* Finally, if a param needs to be split between registers and stack, 
     write the second half to the stack now.  */
  if (split_param_len != 0)
    {
      stack_dest -= split_param_len;
      write_memory (stack_dest, buf, split_param_len);
    }

  /* Set up return address (provided to us as bp_addr).  */
  regcache_cooked_write_unsigned (regcache, MT_RA_REGNUM, bp_addr);

  /* Store struct return address, if given.  */
  if (struct_return && struct_addr != 0)
    regcache_cooked_write_unsigned (regcache, MT_R11_REGNUM, struct_addr);

  /* Set aside 16 bytes for the callee to save regs 1-4.  */
  stack_dest -= 16;

  /* Update the stack pointer.  */
  regcache_cooked_write_unsigned (regcache, MT_SP_REGNUM, stack_dest);

  /* And that should do it.  Return the new stack pointer.  */
  return stack_dest;
}


/* The 'unwind_cache' data structure.  */

struct mt_unwind_cache
{
  /* The previous frame's inner most stack address.  
     Used as this frame ID's stack_addr.  */
  CORE_ADDR prev_sp;
  CORE_ADDR frame_base;
  int framesize;
  int frameless_p;

  /* Table indicating the location of each and every register.  */
  struct trad_frame_saved_reg *saved_regs;
};

/* Initialize an unwind_cache.  Build up the saved_regs table etc. for
   the frame.  */

static struct mt_unwind_cache *
mt_frame_unwind_cache (struct frame_info *next_frame,
			void **this_prologue_cache)
{
  struct gdbarch *gdbarch;
  struct mt_unwind_cache *info;
  CORE_ADDR next_addr, start_addr, end_addr, prologue_end_addr;
  unsigned long instr, upper_half, delayed_store = 0;
  int regnum, offset;
  ULONGEST sp, fp;

  if ((*this_prologue_cache))
    return (*this_prologue_cache);

  gdbarch = get_frame_arch (next_frame);
  info = FRAME_OBSTACK_ZALLOC (struct mt_unwind_cache);
  (*this_prologue_cache) = info;

  info->prev_sp = 0;
  info->framesize = 0;
  info->frame_base = 0;
  info->frameless_p = 1;
  info->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  /* Grab the frame-relative values of SP and FP, needed below. 
     The frame_saved_register function will find them on the
     stack or in the registers as appropriate.  */
  frame_unwind_unsigned_register (next_frame, MT_SP_REGNUM, &sp);
  frame_unwind_unsigned_register (next_frame, MT_FP_REGNUM, &fp);

  start_addr = frame_func_unwind (next_frame, NORMAL_FRAME);

  /* Return early if GDB couldn't find the function.  */
  if (start_addr == 0)
    return info;

  end_addr = frame_pc_unwind (next_frame);
  prologue_end_addr = skip_prologue_using_sal (start_addr);
  if (end_addr == 0)
  for (next_addr = start_addr; next_addr < end_addr; next_addr += 4)
    {
      instr = get_frame_memory_unsigned (next_frame, next_addr, 4);
      if (delayed_store)	/* previous instr was a push */
	{
	  upper_half = delayed_store >> 16;
	  regnum = upper_half & 0xf;
	  offset = delayed_store & 0xffff;
	  switch (upper_half & 0xfff0)
	    {
	    case 0x43c0:	/* push using frame pointer */
	      info->saved_regs[regnum].addr = offset;
	      break;
	    case 0x43d0:	/* push using stack pointer */
	      info->saved_regs[regnum].addr = offset;
	      break;
	    default:		/* lint */
	      break;
	    }
	  delayed_store = 0;
	}

      switch (instr)
	{
	case 0x12000000:	/* NO-OP */
	  continue;
	case 0x12ddc000:	/* copy sp into fp */
	  info->frameless_p = 0;	/* Record that the frame pointer is in use.  */
	  continue;
	default:
	  upper_half = instr >> 16;
	  if (upper_half == 0x05dd ||	/* subi  sp, sp, imm */
	      upper_half == 0x07dd)	/* subui sp, sp, imm */
	    {
	      /* Record the frame size.  */
	      info->framesize = instr & 0xffff;
	      continue;
	    }
	  if ((upper_half & 0xfff0) == 0x43c0 ||	/* frame push */
	      (upper_half & 0xfff0) == 0x43d0)	/* stack push */
	    {
	      /* Save this instruction, but don't record the 
	         pushed register as 'saved' until we see the
	         next instruction.  That's because of deferred stores
	         on this target -- GDB won't be able to read the register
	         from the stack until one instruction later.  */
	      delayed_store = instr;
	      continue;
	    }
	  /* Not a prologue instruction.  Is this the end of the prologue?
	     This is the most difficult decision; when to stop scanning. 

	     If we have no line symbol, then the best thing we can do
	     is to stop scanning when we encounter an instruction that
	     is not likely to be a part of the prologue. 

	     But if we do have a line symbol, then we should 
	     keep scanning until we reach it (or we reach end_addr).  */

	  if (prologue_end_addr && (prologue_end_addr > (next_addr + 4)))
	    continue;		/* Keep scanning, recording saved_regs etc.  */
	  else
	    break;		/* Quit scanning: breakpoint can be set here.  */
	}
    }

  /* Special handling for the "saved" address of the SP:
     The SP is of course never saved on the stack at all, so
     by convention what we put here is simply the previous 
     _value_ of the SP (as opposed to an address where the
     previous value would have been pushed).  This will also
     give us the frame base address.  */

  if (info->frameless_p)
    {
      info->frame_base = sp + info->framesize;
      info->prev_sp = sp + info->framesize;
    }
  else
    {
      info->frame_base = fp + info->framesize;
      info->prev_sp = fp + info->framesize;
    }
  /* Save prev_sp in saved_regs as a value, not as an address.  */
  trad_frame_set_value (info->saved_regs, MT_SP_REGNUM, info->prev_sp);

  /* Now convert frame offsets to actual addresses (not offsets).  */
  for (regnum = 0; regnum < MT_NUM_REGS; regnum++)
    if (trad_frame_addr_p (info->saved_regs, regnum))
      info->saved_regs[regnum].addr += info->frame_base - info->framesize;

  /* The call instruction moves the caller's PC in the callee's RA reg.
     Since this is an unwind, do the reverse.  Copy the location of RA
     into PC (the address / regnum) so that a request for PC will be
     converted into a request for the RA.  */
  info->saved_regs[MT_PC_REGNUM] = info->saved_regs[MT_RA_REGNUM];

  return info;
}

static CORE_ADDR
mt_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  ULONGEST pc;

  frame_unwind_unsigned_register (next_frame, MT_PC_REGNUM, &pc);
  return pc;
}

static CORE_ADDR
mt_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  ULONGEST sp;

  frame_unwind_unsigned_register (next_frame, MT_SP_REGNUM, &sp);
  return sp;
}

/* Assuming NEXT_FRAME->prev is a dummy, return the frame ID of that
   dummy frame.  The frame ID's base needs to match the TOS value
   saved by save_dummy_frame_tos(), and the PC match the dummy frame's
   breakpoint.  */

static struct frame_id
mt_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_id_build (mt_unwind_sp (gdbarch, next_frame),
			 frame_pc_unwind (next_frame));
}

/* Given a GDB frame, determine the address of the calling function's
   frame.  This will be used to create a new GDB frame struct.  */

static void
mt_frame_this_id (struct frame_info *next_frame,
		   void **this_prologue_cache, struct frame_id *this_id)
{
  struct mt_unwind_cache *info =
    mt_frame_unwind_cache (next_frame, this_prologue_cache);

  if (!(info == NULL || info->prev_sp == 0))
    (*this_id) = frame_id_build (info->prev_sp,
				 frame_func_unwind (next_frame, NORMAL_FRAME));

  return;
}

static void
mt_frame_prev_register (struct frame_info *next_frame,
			 void **this_prologue_cache,
			 int regnum, int *optimizedp,
			 enum lval_type *lvalp, CORE_ADDR *addrp,
			 int *realnump, gdb_byte *bufferp)
{
  struct mt_unwind_cache *info =
    mt_frame_unwind_cache (next_frame, this_prologue_cache);

  trad_frame_get_prev_register (next_frame, info->saved_regs, regnum,
				optimizedp, lvalp, addrp, realnump, bufferp);
}

static CORE_ADDR
mt_frame_base_address (struct frame_info *next_frame,
			void **this_prologue_cache)
{
  struct mt_unwind_cache *info =
    mt_frame_unwind_cache (next_frame, this_prologue_cache);

  return info->frame_base;
}

/* This is a shared interface:  the 'frame_unwind' object is what's
   returned by the 'sniffer' function, and in turn specifies how to
   get a frame's ID and prev_regs.

   This exports the 'prev_register' and 'this_id' methods.  */

static const struct frame_unwind mt_frame_unwind = {
  NORMAL_FRAME,
  mt_frame_this_id,
  mt_frame_prev_register
};

/* The sniffer is a registered function that identifies our family of
   frame unwind functions (this_id and prev_register).  */

static const struct frame_unwind *
mt_frame_sniffer (struct frame_info *next_frame)
{
  return &mt_frame_unwind;
}

/* Another shared interface:  the 'frame_base' object specifies how to
   unwind a frame and secure the base addresses for frame objects
   (locals, args).  */

static struct frame_base mt_frame_base = {
  &mt_frame_unwind,
  mt_frame_base_address,
  mt_frame_base_address,
  mt_frame_base_address
};

static struct gdbarch *
mt_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;

  /* Find a candidate among the list of pre-declared architectures.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  /* None found, create a new architecture from the information
     provided.  */
  gdbarch = gdbarch_alloc (&info, NULL);

  set_gdbarch_float_format (gdbarch, floatformats_ieee_single);
  set_gdbarch_double_format (gdbarch, floatformats_ieee_double);
  set_gdbarch_long_double_format (gdbarch, floatformats_ieee_double);

  set_gdbarch_register_name (gdbarch, mt_register_name);
  set_gdbarch_num_regs (gdbarch, MT_NUM_REGS);
  set_gdbarch_num_pseudo_regs (gdbarch, MT_NUM_PSEUDO_REGS);
  set_gdbarch_pc_regnum (gdbarch, MT_PC_REGNUM);
  set_gdbarch_sp_regnum (gdbarch, MT_SP_REGNUM);
  set_gdbarch_pseudo_register_read (gdbarch, mt_pseudo_register_read);
  set_gdbarch_pseudo_register_write (gdbarch, mt_pseudo_register_write);
  set_gdbarch_skip_prologue (gdbarch, mt_skip_prologue);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_breakpoint_from_pc (gdbarch, mt_breakpoint_from_pc);
  set_gdbarch_decr_pc_after_break (gdbarch, 0);
  set_gdbarch_frame_args_skip (gdbarch, 0);
  set_gdbarch_print_insn (gdbarch, print_insn_mt);
  set_gdbarch_register_type (gdbarch, mt_register_type);
  set_gdbarch_register_reggroup_p (gdbarch, mt_register_reggroup_p);

  set_gdbarch_return_value (gdbarch, mt_return_value);
  set_gdbarch_sp_regnum (gdbarch, MT_SP_REGNUM);

  set_gdbarch_frame_align (gdbarch, mt_frame_align);

  set_gdbarch_print_registers_info (gdbarch, mt_registers_info);

  set_gdbarch_push_dummy_call (gdbarch, mt_push_dummy_call);

  /* Target builtin data types.  */
  set_gdbarch_short_bit (gdbarch, 16);
  set_gdbarch_int_bit (gdbarch, 32);
  set_gdbarch_long_bit (gdbarch, 32);
  set_gdbarch_long_long_bit (gdbarch, 64);
  set_gdbarch_float_bit (gdbarch, 32);
  set_gdbarch_double_bit (gdbarch, 64);
  set_gdbarch_long_double_bit (gdbarch, 64);
  set_gdbarch_ptr_bit (gdbarch, 32);

  /* Register the DWARF 2 sniffer first, and then the traditional prologue
     based sniffer.  */
  frame_unwind_append_sniffer (gdbarch, dwarf2_frame_sniffer);
  frame_unwind_append_sniffer (gdbarch, mt_frame_sniffer);
  frame_base_set_default (gdbarch, &mt_frame_base);

  /* Register the 'unwind_pc' method.  */
  set_gdbarch_unwind_pc (gdbarch, mt_unwind_pc);
  set_gdbarch_unwind_sp (gdbarch, mt_unwind_sp);

  /* Methods for saving / extracting a dummy frame's ID.  
     The ID's stack address must match the SP value returned by
     PUSH_DUMMY_CALL, and saved by generic_save_dummy_frame_tos.  */
  set_gdbarch_unwind_dummy_id (gdbarch, mt_unwind_dummy_id);

  return gdbarch;
}

void
_initialize_mt_tdep (void)
{
  register_gdbarch_init (bfd_arch_mt, mt_gdbarch_init);
}
