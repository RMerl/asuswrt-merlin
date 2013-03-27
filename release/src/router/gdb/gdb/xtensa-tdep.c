/* Target-dependent code for the Xtensa port of GDB, the GNU debugger.

   Copyright (C) 2003, 2005, 2006, 2007 Free Software Foundation, Inc.

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
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbtypes.h"
#include "gdbcore.h"
#include "value.h"
#include "dis-asm.h"
#include "inferior.h"
#include "floatformat.h"
#include "regcache.h"
#include "reggroups.h"
#include "regset.h"

#include "dummy-frame.h"
#include "elf/dwarf2.h"
#include "dwarf2-frame.h"
#include "dwarf2loc.h"
#include "frame.h"
#include "frame-base.h"
#include "frame-unwind.h"

#include "arch-utils.h"
#include "gdbarch.h"
#include "remote.h"
#include "serial.h"

#include "command.h"
#include "gdbcmd.h"
#include "gdb_assert.h"

#include "xtensa-isa.h"
#include "xtensa-tdep.h"


static int xtensa_debug_level = 0;

#define DEBUGWARN(args...) \
  if (xtensa_debug_level > 0) \
    fprintf_unfiltered (gdb_stdlog, "(warn ) " args)

#define DEBUGINFO(args...) \
  if (xtensa_debug_level > 1) \
    fprintf_unfiltered (gdb_stdlog, "(info ) " args)

#define DEBUGTRACE(args...) \
  if (xtensa_debug_level > 2) \
    fprintf_unfiltered (gdb_stdlog, "(trace) " args)

#define DEBUGVERB(args...) \
  if (xtensa_debug_level > 3) \
    fprintf_unfiltered (gdb_stdlog, "(verb ) " args)


/* According to the ABI, the SP must be aligned to 16-byte boundaries.  */
#define SP_ALIGNMENT 16


/* On Windowed ABI, we use a6 through a11 for passing arguments
   to a function called by GDB because CALL4 is used.  */
#define ARGS_FIRST_REG		A6_REGNUM
#define ARGS_NUM_REGS		6
#define REGISTER_SIZE		4


/* Extract the call size from the return address or PS register.  */
#define PS_CALLINC_SHIFT	16
#define PS_CALLINC_MASK		0x00030000
#define CALLINC(ps)		(((ps) & PS_CALLINC_MASK) >> PS_CALLINC_SHIFT)
#define WINSIZE(ra)		(4 * (( (ra) >> 30) & 0x3))


/* Convert a live Ax register number to the corresponding Areg number.  */
#define AREG_NUMBER(r, wb) \
  ((((r) - A0_REGNUM + (((wb) & WB_MASK) << WB_SHIFT)) & AREGS_MASK) + AR_BASE)

/* ABI-independent macros.  */
#define ARG_NOF	    (CALL_ABI == CallAbiCall0Only ? C0_NARGS : (ARGS_NUM_REGS))
#define ARG_1ST	    (CALL_ABI == CallAbiCall0Only \
		     ? (A0_REGNUM) + C0_ARGS : (ARGS_FIRST_REG))

extern struct gdbarch_tdep *xtensa_config_tdep (struct gdbarch_info *);
extern int xtensa_config_byte_order (struct gdbarch_info *);


/* XTENSA_IS_ENTRY tests whether the first byte of an instruction
   indicates that the instruction is an ENTRY instruction.  */

#define XTENSA_IS_ENTRY(op1) \
  ((gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG) \
   ? ((op1) == 0x6c) : ((op1) == 0x36))

#define XTENSA_ENTRY_LENGTH	3

/* windowing_enabled() returns true, if windowing is enabled.
   WOE must be set to 1; EXCM to 0.
   Note: We assume that EXCM is always 0 for XEA1.  */

#define PS_WOE			(1<<18)
#define PS_EXC			(1<<4)

static inline int
windowing_enabled (CORE_ADDR ps)
{
  return ((ps & PS_EXC) == 0 && (ps & PS_WOE) != 0);
}

/* Return the window size of the previous call to the function from which we
   have just returned.

   This function is used to extract the return value after a called function
   has returned to the caller.  On Xtensa, the register that holds the return
   value (from the perspective of the caller) depends on what call
   instruction was used.  For now, we are assuming that the call instruction
   precedes the current address, so we simply analyze the call instruction.
   If we are in a dummy frame, we simply return 4 as we used a 'pseudo-call4'
   method to call the inferior function.  */

static int
extract_call_winsize (CORE_ADDR pc)
{
  int winsize = 4;
  int insn;
  gdb_byte buf[4];

  DEBUGTRACE ("extract_call_winsize (pc = 0x%08x)\n", (int) pc);

  /* Read the previous instruction (should be a call[x]{4|8|12}.  */
  read_memory (pc-3, buf, 3);
  insn = extract_unsigned_integer (buf, 3);

  /* Decode call instruction:
     Little Endian
       call{0,4,8,12}   OFFSET || {00,01,10,11} || 0101
       callx{0,4,8,12}  OFFSET || 11 || {00,01,10,11} || 0000
     Big Endian
       call{0,4,8,12}   0101 || {00,01,10,11} || OFFSET
       callx{0,4,8,12}  0000 || {00,01,10,11} || 11 || OFFSET.  */

  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_LITTLE)
    {
      if (((insn & 0xf) == 0x5) || ((insn & 0xcf) == 0xc0))
	winsize = (insn & 0x30) >> 2;   /* 0, 4, 8, 12.  */
    }
  else
    {
      if (((insn >> 20) == 0x5) || (((insn >> 16) & 0xf3) == 0x03))
	winsize = (insn >> 16) & 0xc;   /* 0, 4, 8, 12.  */
    }
  return winsize;
}


/* REGISTER INFORMATION */

/* Returns the name of a register.  */
static const char *
xtensa_register_name (int regnum)
{
  /* Return the name stored in the register map.  */
  if (regnum >= 0 && regnum < gdbarch_num_regs (current_gdbarch)
			      + gdbarch_num_pseudo_regs (current_gdbarch))
    return REGMAP[regnum].name;

  internal_error (__FILE__, __LINE__, _("invalid register %d"), regnum);
  return 0;
}

static unsigned long
xtensa_read_register (int regnum)
{
  ULONGEST value;

  regcache_raw_read_unsigned (get_current_regcache (), regnum, &value);
  return (unsigned long) value;
}

/* Return the type of a register.  Create a new type, if necessary.  */

static struct ctype_cache
{
  struct ctype_cache *next;
  int size;
  struct type *virtual_type;
} *type_entries = NULL;

static struct type *
xtensa_register_type (struct gdbarch *gdbarch, int regnum)
{
  /* Return signed integer for ARx and Ax registers.  */
  if ((regnum >= AR_BASE && regnum < AR_BASE + NUM_AREGS)
      || (regnum >= A0_BASE && regnum < A0_BASE + 16))
    return builtin_type_int;

  if (regnum == gdbarch_pc_regnum (current_gdbarch) || regnum == A1_REGNUM)
    return lookup_pointer_type (builtin_type_void);

  /* Return the stored type for all other registers.  */
  else if (regnum >= 0 && regnum < gdbarch_num_regs (current_gdbarch)
				   + gdbarch_num_pseudo_regs (current_gdbarch))
    {
      xtensa_register_t* reg = &REGMAP[regnum];

      /* Set ctype for this register (only the first time).  */

      if (reg->ctype == 0)
	{
	  struct ctype_cache *tp;
	  int size = reg->byte_size;

	  /* We always use the memory representation,
	     even if the register width is smaller.  */
	  switch (size)
	    {
	    case 1:
	      reg->ctype = builtin_type_uint8;
	      break;

	    case 2:
	      reg->ctype = builtin_type_uint16;
	      break;

	    case 4:
	      reg->ctype = builtin_type_uint32;
	      break;

	    case 8:
	      reg->ctype = builtin_type_uint64;
	      break;

	    case 16:
	      reg->ctype = builtin_type_uint128;
	      break;

	    default:
	      for (tp = type_entries; tp != NULL; tp = tp->next)
		if (tp->size == size)
		  break;

	      if (tp == NULL)
		{
		  char *name = xmalloc (16);
		  tp = xmalloc (sizeof (struct ctype_cache));
		  tp->next = type_entries;
		  type_entries = tp;
		  tp->size = size;

		  sprintf (name, "int%d", size * 8);
		  tp->virtual_type = init_type (TYPE_CODE_INT, size,
						TYPE_FLAG_UNSIGNED, name,
						NULL);
		}

	      reg->ctype = tp->virtual_type;
	    }
	}
      return reg->ctype;
    }

  internal_error (__FILE__, __LINE__, _("invalid register number %d"), regnum);
  return 0;
}


/* Return the 'local' register number for stubs, dwarf2, etc.
   The debugging information enumerates registers starting from 0 for A0
   to n for An.  So, we only have to add the base number for A0.  */

static int
xtensa_reg_to_regnum (int regnum)
{
  int i;

  if (regnum >= 0 && regnum < 16)
    return A0_BASE + regnum;

  for (i = 0;
       i < gdbarch_num_regs (current_gdbarch)
	   + gdbarch_num_pseudo_regs (current_gdbarch);
       i++)
    if (regnum == REGMAP[i].target_number)
      return i;

  internal_error (__FILE__, __LINE__,
		  _("invalid dwarf/stabs register number %d"), regnum);
  return 0;
}


/* Write the bits of a masked register to the various registers.
   Only the masked areas of these registers are modified; the other
   fields are untouched.  The size of masked registers is always less
   than or equal to 32 bits.  */

static void
xtensa_register_write_masked (struct regcache *regcache,
			      xtensa_register_t *reg, const gdb_byte *buffer)
{
  unsigned int value[(MAX_REGISTER_SIZE + 3) / 4];
  const xtensa_mask_t *mask = reg->mask;

  int shift = 0;		/* Shift for next mask (mod 32).  */
  int start, size;		/* Start bit and size of current mask.  */

  unsigned int *ptr = value;
  unsigned int regval, m, mem = 0;

  int bytesize = reg->byte_size;
  int bitsize = bytesize * 8;
  int i, r;

  DEBUGTRACE ("xtensa_register_write_masked ()\n");

  /* Copy the masked register to host byte-order.  */
  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
    for (i = 0; i < bytesize; i++)
      {
	mem >>= 8;
	mem |= (buffer[bytesize - i - 1] << 24);
	if ((i & 3) == 3)
	  *ptr++ = mem;
      }
  else
    for (i = 0; i < bytesize; i++)
      {
	mem >>= 8;
	mem |= (buffer[i] << 24);
	if ((i & 3) == 3)
	  *ptr++ = mem;
      }

  /* We might have to shift the final value:
     bytesize & 3 == 0 -> nothing to do, we use the full 32 bits,
     bytesize & 3 == x -> shift (4-x) * 8.  */

  *ptr = mem >> (((0 - bytesize) & 3) * 8);
  ptr = value;
  mem = *ptr;

  /* Write the bits to the masked areas of the other registers.  */
  for (i = 0; i < mask->count; i++)
    {
      start = mask->mask[i].bit_start;
      size = mask->mask[i].bit_size;
      regval = mem >> shift;

      if ((shift += size) > bitsize)
	error (_("size of all masks is larger than the register"));

      if (shift >= 32)
	{
	  mem = *(++ptr);
	  shift -= 32;
	  bitsize -= 32;

	  if (shift > 0)
	    regval |= mem << (size - shift);
	}

      /* Make sure we have a valid register.  */
      r = mask->mask[i].reg_num;
      if (r >= 0 && size > 0)
	{
	  /* Don't overwrite the unmasked areas.  */
	  ULONGEST old_val;
	  regcache_cooked_read_unsigned (regcache, r, &old_val);
	  m = 0xffffffff >> (32 - size) << start;
	  regval <<= start;
	  regval = (regval & m) | (old_val & ~m);
	  regcache_cooked_write_unsigned (regcache, r, regval);
	}
    }
}


/* Read a tie state or mapped registers.  Read the masked areas
   of the registers and assemble them into a single value.  */

static void
xtensa_register_read_masked (struct regcache *regcache,
			     xtensa_register_t *reg, gdb_byte *buffer)
{
  unsigned int value[(MAX_REGISTER_SIZE + 3) / 4];
  const xtensa_mask_t *mask = reg->mask;

  int shift = 0;
  int start, size;

  unsigned int *ptr = value;
  unsigned int regval, mem = 0;

  int bytesize = reg->byte_size;
  int bitsize = bytesize * 8;
  int i;

  DEBUGTRACE ("xtensa_register_read_masked (reg \"%s\", ...)\n",
	      reg->name == 0 ? "" : reg->name);

  /* Assemble the register from the masked areas of other registers.  */
  for (i = 0; i < mask->count; i++)
    {
      int r = mask->mask[i].reg_num;
      if (r >= 0)
	{
	  ULONGEST val;
	  regcache_cooked_read_unsigned (regcache, r, &val);
	  regval = (unsigned int) val;
	}
      else
	regval = 0;

      start = mask->mask[i].bit_start;
      size = mask->mask[i].bit_size;

      regval >>= start;

      if (size < 32)
	regval &= (0xffffffff >> (32 - size));

      mem |= regval << shift;

      if ((shift += size) > bitsize)
	error (_("size of all masks is larger than the register"));

      if (shift >= 32)
	{
	  *ptr++ = mem;
	  bitsize -= 32;
	  shift -= 32;

	  if (shift == 0)
	    mem = 0;
	  else
	    mem = regval >> (size - shift);
	}
    }

  if (shift > 0)
    *ptr = mem;

  /* Copy value to target byte order.  */
  ptr = value;
  mem = *ptr;

  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
    for (i = 0; i < bytesize; i++)
      {
	if ((i & 3) == 0)
	  mem = *ptr++;
	buffer[bytesize - i - 1] = mem & 0xff;
	mem >>= 8;
      }
  else
    for (i = 0; i < bytesize; i++)
      {
	if ((i & 3) == 0)
	  mem = *ptr++;
	buffer[i] = mem & 0xff;
	mem >>= 8;
      }
}


/* Read pseudo registers.  */

static void
xtensa_pseudo_register_read (struct gdbarch *gdbarch,
			     struct regcache *regcache,
			     int regnum,
			     gdb_byte *buffer)
{
  DEBUGTRACE ("xtensa_pseudo_register_read (... regnum = %d (%s) ...)\n",
	      regnum, xtensa_register_name (regnum));

  if (regnum == FP_ALIAS)
     regnum = A1_REGNUM;

  /* Read aliases a0..a15, if this is a Windowed ABI.  */
  if (ISA_USE_WINDOWED_REGISTERS
      && (regnum >= A0_REGNUM) && (regnum <= A15_REGNUM))
    {
      gdb_byte *buf = (gdb_byte *) alloca (MAX_REGISTER_SIZE);

      regcache_raw_read (regcache, WB_REGNUM, buf);
      regnum = AREG_NUMBER (regnum, extract_unsigned_integer (buf, 4));
    }

  /* We can always read non-pseudo registers.  */
  if (regnum >= 0 && regnum < gdbarch_num_regs (current_gdbarch))
    regcache_raw_read (regcache, regnum, buffer);

  /* Pseudo registers.  */
  else if (regnum >= 0
	    && regnum < gdbarch_num_regs (current_gdbarch)
			+ gdbarch_num_pseudo_regs (current_gdbarch))
    {
      xtensa_register_t *reg = &REGMAP[regnum];
      xtensa_register_type_t type = reg->type;
      int flags = XTENSA_TARGET_FLAGS;

      /* We cannot read Unknown or Unmapped registers.  */
      if (type == xtRegisterTypeUnmapped || type == xtRegisterTypeUnknown)
	{
	  if ((flags & xtTargetFlagsNonVisibleRegs) == 0)
	    {
	      warning (_("cannot read register %s"),
		       xtensa_register_name (regnum));
	      return;
	    }
	}

      /* Some targets cannot read TIE register files.  */
      else if (type == xtRegisterTypeTieRegfile)
        {
	  /* Use 'fetch' to get register?  */
	  if (flags & xtTargetFlagsUseFetchStore)
	    {
	      warning (_("cannot read register"));
	      return;
	    }

	  /* On some targets (esp. simulators), we can always read the reg.  */
	  else if ((flags & xtTargetFlagsNonVisibleRegs) == 0)
	    {
	      warning (_("cannot read register"));
	      return;
	    }
	}

      /* We can always read mapped registers.  */
      else if (type == xtRegisterTypeMapped || type == xtRegisterTypeTieState)
        {
	  xtensa_register_read_masked (regcache, reg, buffer);
	  return;
	}

      /* Assume that we can read the register.  */
      regcache_raw_read (regcache, regnum, buffer);
    }
  else
    internal_error (__FILE__, __LINE__,
		    _("invalid register number %d"), regnum);
}


/* Write pseudo registers.  */

static void
xtensa_pseudo_register_write (struct gdbarch *gdbarch,
			      struct regcache *regcache,
			      int regnum,
			      const gdb_byte *buffer)
{
  DEBUGTRACE ("xtensa_pseudo_register_write (... regnum = %d (%s) ...)\n",
	      regnum, xtensa_register_name (regnum));

  if (regnum == FP_ALIAS)
     regnum = A1_REGNUM;

  /* Renumber register, if aliase a0..a15 on Windowed ABI.  */
  if (ISA_USE_WINDOWED_REGISTERS
      && (regnum >= A0_REGNUM) && (regnum <= A15_REGNUM))
    {
      gdb_byte *buf = (gdb_byte *) alloca (MAX_REGISTER_SIZE);
      unsigned int wb;

      regcache_raw_read (regcache, WB_REGNUM, buf);
      regnum = AREG_NUMBER (regnum, extract_unsigned_integer (buf, 4));
    }

  /* We can always write 'core' registers.
     Note: We might have converted Ax->ARy.  */
  if (regnum >= 0 && regnum < gdbarch_num_regs (current_gdbarch))
    regcache_raw_write (regcache, regnum, buffer);

  /* Pseudo registers.  */
  else if (regnum >= 0
	   && regnum < gdbarch_num_regs (current_gdbarch)
		       + gdbarch_num_pseudo_regs (current_gdbarch))
    {
      xtensa_register_t *reg = &REGMAP[regnum];
      xtensa_register_type_t type = reg->type;
      int flags = XTENSA_TARGET_FLAGS;

      /* On most targets, we cannot write registers
	 of type "Unknown" or "Unmapped".  */
      if (type == xtRegisterTypeUnmapped || type == xtRegisterTypeUnknown)
        {
	  if ((flags & xtTargetFlagsNonVisibleRegs) == 0)
	    {
	      warning (_("cannot write register %s"),
		       xtensa_register_name (regnum));
	      return;
	    }
	}

      /* Some targets cannot read TIE register files.  */
      else if (type == xtRegisterTypeTieRegfile)
        {
	  /* Use 'store' to get register?  */
	  if (flags & xtTargetFlagsUseFetchStore)
	    {
	      warning (_("cannot write register"));
	      return;
	    }

	  /* On some targets (esp. simulators), we can always write
	     the register.  */
	  else if ((flags & xtTargetFlagsNonVisibleRegs) == 0)
	    {
	      warning (_("cannot write register"));
	      return;
	    }
	}

      /* We can always write mapped registers.  */
      else if (type == xtRegisterTypeMapped || type == xtRegisterTypeTieState)
        {
	  xtensa_register_write_masked (regcache, reg, buffer);
	  return;
	}

      /* Assume that we can write the register.  */
      regcache_raw_write (regcache, regnum, buffer);
    }
  else
    internal_error (__FILE__, __LINE__,
		    _("invalid register number %d"), regnum);
}

static struct reggroup *xtensa_ar_reggroup;
static struct reggroup *xtensa_user_reggroup;
static struct reggroup *xtensa_vectra_reggroup;
static struct reggroup *xtensa_cp[XTENSA_MAX_COPROCESSOR];

static void
xtensa_init_reggroups (void)
{
  xtensa_ar_reggroup = reggroup_new ("ar", USER_REGGROUP);
  xtensa_user_reggroup = reggroup_new ("user", USER_REGGROUP);
  xtensa_vectra_reggroup = reggroup_new ("vectra", USER_REGGROUP);

  xtensa_cp[0] = reggroup_new ("cp0", USER_REGGROUP);
  xtensa_cp[1] = reggroup_new ("cp1", USER_REGGROUP);
  xtensa_cp[2] = reggroup_new ("cp2", USER_REGGROUP);
  xtensa_cp[3] = reggroup_new ("cp3", USER_REGGROUP);
  xtensa_cp[4] = reggroup_new ("cp4", USER_REGGROUP);
  xtensa_cp[5] = reggroup_new ("cp5", USER_REGGROUP);
  xtensa_cp[6] = reggroup_new ("cp6", USER_REGGROUP);
  xtensa_cp[7] = reggroup_new ("cp7", USER_REGGROUP);
}

static void
xtensa_add_reggroups (struct gdbarch *gdbarch)
{
  int i;

  /* Predefined groups.  */
  reggroup_add (gdbarch, all_reggroup);
  reggroup_add (gdbarch, save_reggroup);
  reggroup_add (gdbarch, restore_reggroup);
  reggroup_add (gdbarch, system_reggroup);
  reggroup_add (gdbarch, vector_reggroup);
  reggroup_add (gdbarch, general_reggroup);
  reggroup_add (gdbarch, float_reggroup);

  /* Xtensa-specific groups.  */
  reggroup_add (gdbarch, xtensa_ar_reggroup);
  reggroup_add (gdbarch, xtensa_user_reggroup);
  reggroup_add (gdbarch, xtensa_vectra_reggroup);

  for (i = 0; i < XTENSA_MAX_COPROCESSOR; i++)
    reggroup_add (gdbarch, xtensa_cp[i]);
}

static int 
xtensa_coprocessor_register_group (struct reggroup *group)
{
  int i;

  for (i = 0; i < XTENSA_MAX_COPROCESSOR; i++)
    if (group == xtensa_cp[i])
      return i;

  return -1;
}

#define SAVE_REST_FLAGS	(XTENSA_REGISTER_FLAGS_READABLE \
			| XTENSA_REGISTER_FLAGS_WRITABLE \
			| XTENSA_REGISTER_FLAGS_VOLATILE)

#define SAVE_REST_VALID	(XTENSA_REGISTER_FLAGS_READABLE \
			| XTENSA_REGISTER_FLAGS_WRITABLE)

static int
xtensa_register_reggroup_p (struct gdbarch *gdbarch,
			    int regnum,
    			    struct reggroup *group)
{
  xtensa_register_t* reg = &REGMAP[regnum];
  xtensa_register_type_t type = reg->type;
  xtensa_register_group_t rg = reg->group;
  int cp_number;

  /* First, skip registers that are not visible to this target
     (unknown and unmapped registers when not using ISS).  */

  if (type == xtRegisterTypeUnmapped || type == xtRegisterTypeUnknown)
    return 0;
  if (group == all_reggroup)
    return 1;
  if (group == xtensa_ar_reggroup)
    return rg & xtRegisterGroupAddrReg;
  if (group == xtensa_user_reggroup)
    return rg & xtRegisterGroupUser;
  if (group == float_reggroup)
    return rg & xtRegisterGroupFloat;
  if (group == general_reggroup)
    return rg & xtRegisterGroupGeneral;
  if (group == float_reggroup)
    return rg & xtRegisterGroupFloat;
  if (group == system_reggroup)
    return rg & xtRegisterGroupState;
  if (group == vector_reggroup || group == xtensa_vectra_reggroup)
    return rg & xtRegisterGroupVectra;
  if (group == save_reggroup || group == restore_reggroup)
    return (regnum < gdbarch_num_regs (current_gdbarch)
	    && (reg->flags & SAVE_REST_FLAGS) == SAVE_REST_VALID);
  if ((cp_number = xtensa_coprocessor_register_group (group)) >= 0)
    return rg & (xtRegisterGroupCP0 << cp_number);
  else
    return 1;
}


/* Supply register REGNUM from the buffer specified by GREGS and LEN
   in the general-purpose register set REGSET to register cache
   REGCACHE.  If REGNUM is -1 do this for all registers in REGSET.  */

static void
xtensa_supply_gregset (const struct regset *regset,
		       struct regcache *rc,
		       int regnum,
		       const void *gregs,
		       size_t len)
{
  const xtensa_elf_gregset_t *regs = gregs;
  int i;

  DEBUGTRACE ("xtensa_supply_gregset (..., regnum==%d, ...) \n", regnum);

  if (regnum == gdbarch_pc_regnum (current_gdbarch) || regnum == -1)
    regcache_raw_supply (rc,
			 gdbarch_pc_regnum (current_gdbarch),
			 (char *) &regs->pc);
  if (regnum == gdbarch_ps_regnum (current_gdbarch) || regnum == -1)
    regcache_raw_supply (rc, gdbarch_ps_regnum (current_gdbarch),
			 (char *) &regs->ps);
  if (regnum == WB_REGNUM || regnum == -1)
    regcache_raw_supply (rc, WB_REGNUM, (char *) &regs->windowbase);
  if (regnum == WS_REGNUM || regnum == -1)
    regcache_raw_supply (rc, WS_REGNUM, (char *) &regs->windowstart);
  if (regnum == LBEG_REGNUM || regnum == -1)
    regcache_raw_supply (rc, LBEG_REGNUM, (char *) &regs->lbeg);
  if (regnum == LEND_REGNUM || regnum == -1)
    regcache_raw_supply (rc, LEND_REGNUM, (char *) &regs->lend);
  if (regnum == LCOUNT_REGNUM || regnum == -1)
    regcache_raw_supply (rc, LCOUNT_REGNUM, (char *) &regs->lcount);
  if (regnum == SAR_REGNUM || regnum == -1)
    regcache_raw_supply (rc, SAR_REGNUM, (char *) &regs->sar);
  if (regnum == EXCCAUSE_REGNUM || regnum == -1)
    regcache_raw_supply (rc, EXCCAUSE_REGNUM, (char *) &regs->exccause);
  if (regnum == EXCVADDR_REGNUM || regnum == -1)
    regcache_raw_supply (rc, EXCVADDR_REGNUM, (char *) &regs->excvaddr);
  if (regnum >= AR_BASE && regnum < AR_BASE + NUM_AREGS)
    regcache_raw_supply (rc, regnum, (char *) &regs->ar[regnum - AR_BASE]);
  else if (regnum == -1)
    {
      for (i = 0; i < NUM_AREGS; ++i)
	regcache_raw_supply (rc, AR_BASE + i, (char *) &regs->ar[i]);
    }
}


/* Xtensa register set.  */

static struct regset
xtensa_gregset =
{
  NULL,
  xtensa_supply_gregset
};


/* Return the appropriate register set for the core
   section identified by SECT_NAME and SECT_SIZE.  */

static const struct regset *
xtensa_regset_from_core_section (struct gdbarch *core_arch,
				 const char *sect_name,
				 size_t sect_size)
{
  DEBUGTRACE ("xtensa_regset_from_core_section "
	      "(..., sect_name==\"%s\", sect_size==%x) \n",
	      sect_name, sect_size);

  if (strcmp (sect_name, ".reg") == 0
      && sect_size >= sizeof(xtensa_elf_gregset_t))
    return &xtensa_gregset;

  return NULL;
}


/* Handling frames.  */

/* Number of registers to save in case of Windowed ABI.  */
#define XTENSA_NUM_SAVED_AREGS		12

/* Frame cache part for Windowed ABI.  */
typedef struct xtensa_windowed_frame_cache
{
  int wb;		/* Base for this frame; -1 if not in regfile.  */
  int callsize;		/* Call size to next frame.  */
  int ws;
  CORE_ADDR aregs[XTENSA_NUM_SAVED_AREGS];
} xtensa_windowed_frame_cache_t;

/* Call0 ABI Definitions.  */

#define C0_MAXOPDS  3	/* Maximum number of operands for prologue analysis.  */
#define C0_NREGS   16	/* Number of A-registers to track.  */
#define C0_CLESV   12	/* Callee-saved registers are here and up.  */
#define C0_SP	    1	/* Register used as SP.  */
#define C0_FP	   15	/* Register used as FP.  */
#define C0_RA	    0	/* Register used as return address.  */
#define C0_ARGS	    2	/* Register used as first arg/retval.  */
#define C0_NARGS    6	/* Number of A-regs for args/retvals.  */

/* Each element of xtensa_call0_frame_cache.c0_rt[] describes for each
   A-register where the current content of the reg came from (in terms
   of an original reg and a constant).  Negative values of c0_rt[n].fp_reg
   mean that the orignal content of the register was saved to the stack.
   c0_rt[n].fr.ofs is NOT the offset from the frame base because we don't 
   know where SP will end up until the entire prologue has been analyzed.  */

#define C0_CONST   -1	/* fr_reg value if register contains a constant.  */
#define C0_INEXP   -2	/* fr_reg value if inexpressible as reg + offset.  */
#define C0_NOSTK   -1	/* to_stk value if register has not been stored.  */

extern xtensa_isa xtensa_default_isa;

typedef struct xtensa_c0reg
{
    int	    fr_reg;	/* original register from which register content
			   is derived, or C0_CONST, or C0_INEXP.  */
    int	    fr_ofs;	/* constant offset from reg, or immediate value.  */
    int	    to_stk;	/* offset from original SP to register (4-byte aligned),
			   or C0_NOSTK if register has not been saved.  */
} xtensa_c0reg_t;


/* Frame cache part for Call0 ABI.  */
typedef struct xtensa_call0_frame_cache
{
  int c0_frmsz;				/* Stack frame size.  */
  int c0_hasfp;				/* Current frame uses frame pointer.  */
  int fp_regnum;			/* A-register used as FP.  */
  int c0_fp;				/* Actual value of frame pointer.  */
  xtensa_c0reg_t c0_rt[C0_NREGS];	/* Register tracking information.  */
} xtensa_call0_frame_cache_t;

typedef struct xtensa_frame_cache
{
  CORE_ADDR base;	/* Stack pointer of the next frame.  */
  CORE_ADDR pc;		/* PC at the entry point to the function.  */
  CORE_ADDR ra;		/* The raw return address.  */
  CORE_ADDR ps;		/* The PS register of the frame.  */
  CORE_ADDR prev_sp;	/* Stack Pointer of the frame.  */
  int call0;		/* It's a call0 framework (else windowed).  */
  union
    {
      xtensa_windowed_frame_cache_t	wd;	/* call0 == false.  */
      xtensa_call0_frame_cache_t       	c0;	/* call0 == true.  */
    };
} xtensa_frame_cache_t;


static struct xtensa_frame_cache *
xtensa_alloc_frame_cache (int windowed)
{
  xtensa_frame_cache_t *cache;
  int i;

  DEBUGTRACE ("xtensa_alloc_frame_cache ()\n");

  cache = FRAME_OBSTACK_ZALLOC (xtensa_frame_cache_t);

  cache->base = 0;
  cache->pc = 0;
  cache->ra = 0;
  cache->ps = 0;
  cache->prev_sp = 0;
  cache->call0 = !windowed;
  if (cache->call0)
    {
      cache->c0.c0_frmsz  = -1;
      cache->c0.c0_hasfp  =  0;
      cache->c0.fp_regnum = -1;
      cache->c0.c0_fp     = -1;

      for (i = 0; i < C0_NREGS; i++)
	{
	  cache->c0.c0_rt[i].fr_reg = i;
	  cache->c0.c0_rt[i].fr_ofs = 0;
	  cache->c0.c0_rt[i].to_stk = C0_NOSTK;
	}
    }
  else
    {
      cache->wd.wb = 0;
      cache->wd.callsize = -1;

      for (i = 0; i < XTENSA_NUM_SAVED_AREGS; i++)
	cache->wd.aregs[i] = -1;
    }
  return cache;
}


static CORE_ADDR
xtensa_frame_align (struct gdbarch *gdbarch, CORE_ADDR address)
{
  return address & ~15;
}


static CORE_ADDR
xtensa_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  gdb_byte buf[8];

  DEBUGTRACE ("xtensa_unwind_pc (next_frame = %p)\n", next_frame);

  frame_unwind_register (next_frame, gdbarch_pc_regnum (current_gdbarch), buf);

  DEBUGINFO ("[xtensa_unwind_pc] pc = 0x%08x\n", (unsigned int)
	     extract_typed_address (buf, builtin_type_void_func_ptr));

  return extract_typed_address (buf, builtin_type_void_func_ptr);
}


static struct frame_id
xtensa_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  CORE_ADDR pc, fp;

  /* next_frame->prev is a dummy frame.  Return a frame ID of that frame.  */

  DEBUGTRACE ("xtensa_unwind_dummy_id ()\n");

  pc = frame_pc_unwind (next_frame);
  fp = frame_unwind_register_unsigned (next_frame, A1_REGNUM);

  /* Make dummy frame ID unique by adding a constant.  */
  return frame_id_build (fp + SP_ALIGNMENT, pc);
}

/* The key values to identify the frame using "cache" are 

	cache->base    = SP of this frame;
	cache->pc      = entry-PC (entry point of the frame function);
	cache->prev_sp = SP of the previous frame.
*/

static void
call0_frame_cache (struct frame_info *next_frame,
		   xtensa_frame_cache_t *cache,
		   CORE_ADDR pc);

static struct xtensa_frame_cache *
xtensa_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  xtensa_frame_cache_t *cache;
  CORE_ADDR ra, wb, ws, pc, sp, ps;
  unsigned int ps_regnum = gdbarch_ps_regnum (current_gdbarch);
  char op1;
  int  windowed;

  DEBUGTRACE ("xtensa_frame_cache (next_frame %p, *this_cache %p)\n",
	      next_frame, this_cache ? *this_cache : (void*)0xdeadbeef);

  if (*this_cache)
    return *this_cache;

  windowed = windowing_enabled (xtensa_read_register (ps_regnum));

  /* Get pristine xtensa-frame.  */
  cache = xtensa_alloc_frame_cache (windowed);
  *this_cache = cache;

  pc = frame_unwind_register_unsigned (next_frame,
				       gdbarch_pc_regnum (current_gdbarch));

  if (windowed)
    {
      /* Get WINDOWBASE, WINDOWSTART, and PS registers.  */
      wb = frame_unwind_register_unsigned (next_frame, WB_REGNUM);
      ws = frame_unwind_register_unsigned (next_frame, WS_REGNUM);
      ps = frame_unwind_register_unsigned (next_frame, ps_regnum);

      op1 = read_memory_integer (pc, 1);
      if (XTENSA_IS_ENTRY (op1))
	{
	  int callinc = CALLINC (ps);
	  ra = frame_unwind_register_unsigned (next_frame,
					       A0_REGNUM + callinc * 4);
	  
	  DEBUGINFO("[xtensa_frame_cache] 'entry' at 0x%08x\n (callinc = %d)",
		    (int)pc, callinc);
	  
	  /* ENTRY hasn't been executed yet, therefore callsize is still 0.  */
	  cache->wd.callsize = 0;
	  cache->wd.wb = wb;
	  cache->wd.ws = ws;
	  cache->prev_sp = frame_unwind_register_unsigned (next_frame,
							   A1_REGNUM);
	}
      else
	{
	  ra = frame_unwind_register_unsigned (next_frame, A0_REGNUM);
	  cache->wd.callsize = WINSIZE (ra);
	  cache->wd.wb = (wb - cache->wd.callsize / 4) & (NUM_AREGS / 4 - 1);
	  cache->wd.ws = ws & ~(1 << wb);
	}

      cache->pc = frame_func_unwind (next_frame, NORMAL_FRAME);
      cache->ra = (cache->pc & 0xc0000000) | (ra & 0x3fffffff);
      cache->ps = (ps & ~PS_CALLINC_MASK)
	| ((WINSIZE(ra)/4) << PS_CALLINC_SHIFT);

      if (cache->wd.ws == 0)
	{
	  int i;

	  /* Set A0...A3.  */
	  sp = frame_unwind_register_unsigned (next_frame, A1_REGNUM) - 16;
	  
	  for (i = 0; i < 4; i++, sp += 4)
	    {
	      cache->wd.aregs[i] = sp;
	    }

	  if (cache->wd.callsize > 4)
	    {
	      /* Set A4...A7/A11.  */
	      /* Read an SP of the previous frame.  */
	      sp = (CORE_ADDR) read_memory_integer (sp - 12, 4);
	      sp -= cache->wd.callsize * 4;

	      for ( /* i=4  */ ; i < cache->wd.callsize; i++, sp += 4)
		{
		  cache->wd.aregs[i] = sp;
		}
	    }
	}

      if ((cache->prev_sp == 0) && ( ra != 0 ))
	/* If RA is equal to 0 this frame is an outermost frame.  Leave
	   cache->prev_sp unchanged marking the boundary of the frame stack.  */
	{
	  if (cache->wd.ws == 0)
	    {
	      /* Register window overflow already happened.
		 We can read caller's SP from the proper spill loction.  */
	      cache->prev_sp =
		read_memory_integer (cache->wd.aregs[1],
				     register_size (current_gdbarch,
						    A1_REGNUM));
	    }
	  else
	    {
	      /* Read caller's frame SP directly from the previous window.  */
	      int regnum = AREG_NUMBER (A1_REGNUM, cache->wd.wb);

	      cache->prev_sp = xtensa_read_register (regnum);
	    }
	}
    }
  else	/* Call0 framework.  */
    {
      call0_frame_cache (next_frame, cache, pc);
    }

  cache->base = frame_unwind_register_unsigned (next_frame,A1_REGNUM);

  return cache;
}

static void
xtensa_frame_this_id (struct frame_info *next_frame,
		      void **this_cache,
		      struct frame_id *this_id)
{
  struct xtensa_frame_cache *cache =
    xtensa_frame_cache (next_frame, this_cache);
  struct frame_id id;

  DEBUGTRACE ("xtensa_frame_this_id (next 0x%08x, *this 0x%08x)\n",
	      (unsigned int) next_frame, (unsigned int) *this_cache);

  if (cache->prev_sp == 0)
    return;

  id = frame_id_build (cache->prev_sp, cache->pc);
  if (frame_id_eq (id, get_frame_id(next_frame)))
    {
      warning(_("\
Frame stack is corrupted. That could happen because of \
setting register(s) from GDB or stopping execution \
inside exception handler. Frame backtracing has stopped. \
It can make some GDB commands work inappropriately.\n"));
      cache->prev_sp = 0;
      return;
    }
  (*this_id) = id;
}

static int
call0_frame_get_reg_at_entry (struct frame_info *next_frame,
			      struct xtensa_frame_cache *cache,
			      int regnum, 
			      CORE_ADDR *addrp,
			      enum lval_type *lval,
			      gdb_byte *valuep)
{
  CORE_ADDR fp, spe;
  int stkofs;
  int reg = (regnum >= AR_BASE && regnum <= (AR_BASE + C0_NREGS))
    		? regnum - AR_BASE : regnum;

  /* Determine stack pointer on entry to this function, based on FP.  */
  spe = cache->c0.c0_fp - cache->c0.c0_rt[cache->c0.fp_regnum].fr_ofs;

  /* If register was saved to the stack frame in the prologue, retrieve it.  */
  stkofs = cache->c0.c0_rt[reg].to_stk;
  if (stkofs != C0_NOSTK)
    {
      *lval = lval_memory;
      *addrp = spe + stkofs;

      if (valuep)
	read_memory (*addrp, valuep, register_size (current_gdbarch, regnum));

      return 1;
    }

  /* If not callee-saved or if known to have been overwritten, give up.  */
  if (reg < C0_CLESV 
      || cache->c0.c0_rt[reg].fr_reg != reg
      || cache->c0.c0_rt[reg].fr_ofs != 0)
    return 0;

  if (get_frame_type (next_frame) != NORMAL_FRAME)
    /* TODO: Do we need a special case for DUMMY_FRAME here?  */
    return 0;

  return call0_frame_get_reg_at_entry (get_next_frame(next_frame),
				       cache, regnum, addrp, lval, valuep);
}

static void
xtensa_frame_prev_register (struct frame_info *next_frame,
			    void **this_cache,
			    int regnum,
			    int *optimizedp,
			    enum lval_type *lvalp,
			    CORE_ADDR *addrp,
			    int *realnump,
			    gdb_byte *valuep)
{
  struct xtensa_frame_cache *cache =
    xtensa_frame_cache (next_frame, this_cache);
  CORE_ADDR saved_reg = 0;
  int done = 1;

  DEBUGTRACE ("xtensa_frame_prev_register (next 0x%08x, "
	      "*this 0x%08x, regnum %d (%s), ...)\n",
	      (unsigned int) next_frame,
	      *this_cache ? (unsigned int) *this_cache : 0, regnum,
	      xtensa_register_name (regnum));

  if (regnum ==gdbarch_pc_regnum (current_gdbarch))
    saved_reg = cache->ra;
  else if (regnum == A1_REGNUM)
    saved_reg = cache->prev_sp;
  else if (!cache->call0)
    {
      if (regnum == WS_REGNUM)
	{
	  if (cache->wd.ws != 0)
	    saved_reg = cache->wd.ws;
	  else
	    saved_reg = 1 << cache->wd.wb;
	}
      else if (regnum == WB_REGNUM)
	saved_reg = cache->wd.wb;
      else if (regnum == gdbarch_ps_regnum (current_gdbarch))
	saved_reg = cache->ps;
      else
	done = 0;
    }
  else
    done = 0;

  if (done)
    {
      *optimizedp = 0;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      if (valuep)
	store_unsigned_integer (valuep, 4, saved_reg);

      return;
    }

  if (!cache->call0) /* Windowed ABI.  */
    {
      /* Convert A-register numbers to AR-register numbers.  */
      if (regnum >= A0_REGNUM && regnum <= A15_REGNUM)
	regnum = AREG_NUMBER (regnum, cache->wd.wb);

      /* Check if AR-register has been saved to stack.  */
      if (regnum >= AR_BASE && regnum <= (AR_BASE + NUM_AREGS))
	{
	  int areg = regnum - AR_BASE - (cache->wd.wb * 4);

	  if (areg >= 0
	      && areg < XTENSA_NUM_SAVED_AREGS
	      && cache->wd.aregs[areg] != -1)
	    {
	      *optimizedp = 0;
	      *lvalp = lval_memory;
	      *addrp = cache->wd.aregs[areg];
	      *realnump = -1;

	      if (valuep)
		read_memory (*addrp, valuep,
			     register_size (current_gdbarch, regnum));

	      DEBUGINFO ("[xtensa_frame_prev_register] register on stack\n");
	      return;
	    }
	}
    }
  else /* Call0 ABI.  */
    {
      int reg = (regnum >= AR_BASE && regnum <= (AR_BASE + C0_NREGS))
			? regnum - AR_BASE : regnum;

      if (reg < C0_NREGS)
	{
	  CORE_ADDR spe;
	  int stkofs;

	  /* If register was saved in the prologue, retrieve it.  */
	  stkofs = cache->c0.c0_rt[reg].to_stk;
	  if (stkofs != C0_NOSTK)
	    {
	      /* Determine SP on entry based on FP.  */
	      spe = cache->c0.c0_fp
		- cache->c0.c0_rt[cache->c0.fp_regnum].fr_ofs;
	      *optimizedp = 0;
	      *lvalp = lval_memory;
	      *addrp = spe + stkofs;
	      *realnump = -1;
	  
	      if (valuep)
		read_memory (*addrp, valuep,
			     register_size (current_gdbarch, regnum));
	  
	      DEBUGINFO ("[xtensa_frame_prev_register] register on stack\n");
	      return;
	    }
	}
    }

  /* All other registers have been either saved to
     the stack or are still alive in the processor.  */

  *optimizedp = 0;
  *lvalp = lval_register;
  *addrp = 0;
  *realnump = regnum;
  if (valuep)
    frame_unwind_register (next_frame, (*realnump), valuep);
}


static const struct frame_unwind
xtensa_frame_unwind =
{
  NORMAL_FRAME,
  xtensa_frame_this_id,
  xtensa_frame_prev_register
};

static const struct frame_unwind *
xtensa_frame_sniffer (struct frame_info *next_frame)
{
  return &xtensa_frame_unwind;
}

static CORE_ADDR
xtensa_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct xtensa_frame_cache *cache =
    xtensa_frame_cache (next_frame, this_cache);

  return cache->base;
}

static const struct frame_base
xtensa_frame_base =
{
  &xtensa_frame_unwind,
  xtensa_frame_base_address,
  xtensa_frame_base_address,
  xtensa_frame_base_address
};


static void
xtensa_extract_return_value (struct type *type,
			     struct regcache *regcache,
			     void *dst)
{
  bfd_byte *valbuf = dst;
  int len = TYPE_LENGTH (type);
  ULONGEST pc, wb;
  int callsize, areg;
  int offset = 0;

  DEBUGTRACE ("xtensa_extract_return_value (...)\n");

  gdb_assert(len > 0);

  if (CALL_ABI != CallAbiCall0Only)
    {
      /* First, we have to find the caller window in the register file.  */
      regcache_raw_read_unsigned (regcache,
			      gdbarch_pc_regnum (current_gdbarch), &pc);
      callsize = extract_call_winsize (pc);

      /* On Xtensa, we can return up to 4 words (or 2 for call12).  */
      if (len > (callsize > 8 ? 8 : 16))
	internal_error (__FILE__, __LINE__,
			_("cannot extract return value of %d bytes long"), len);

      /* Get the register offset of the return
	 register (A2) in the caller window.  */
      regcache_raw_read_unsigned (regcache, WB_REGNUM, &wb);
      areg = AREG_NUMBER(A2_REGNUM + callsize, wb);
    }
  else
    {
      /* No windowing hardware - Call0 ABI.  */
      areg = A0_REGNUM + C0_ARGS;
    }

  DEBUGINFO ("[xtensa_extract_return_value] areg %d len %d\n", areg, len);

  if (len < 4 && gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
    offset = 4 - len;

  for (; len > 0; len -= 4, areg++, valbuf += 4)
    {
      if (len < 4)
	regcache_raw_read_part (regcache, areg, offset, len, valbuf);
      else
	regcache_raw_read (regcache, areg, valbuf);
    }
}


static void
xtensa_store_return_value (struct type *type,
			   struct regcache *regcache,
			   const void *dst)
{
  const bfd_byte *valbuf = dst;
  unsigned int areg;
  ULONGEST pc, wb;
  int callsize;
  int len = TYPE_LENGTH (type);
  int offset = 0;

  DEBUGTRACE ("xtensa_store_return_value (...)\n");

  if (CALL_ABI != CallAbiCall0Only)
    {
      regcache_raw_read_unsigned (regcache, WB_REGNUM, &wb);
  regcache_raw_read_unsigned (regcache,
			      gdbarch_pc_regnum (current_gdbarch), &pc);
      callsize = extract_call_winsize (pc);

      if (len > (callsize > 8 ? 8 : 16))
	internal_error (__FILE__, __LINE__,
			_("unimplemented for this length: %d"),
			TYPE_LENGTH (type));
      areg = AREG_NUMBER (A2_REGNUM + callsize, wb);

      DEBUGTRACE ("[xtensa_store_return_value] callsize %d wb %d\n",
              callsize, (int) wb);
    }
  else
    {
      areg = A0_REGNUM + C0_ARGS;
    }

  if (len < 4 && gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
    offset = 4 - len;

  for (; len > 0; len -= 4, areg++, valbuf += 4)
    {
      if (len < 4)
	regcache_raw_write_part (regcache, areg, offset, len, valbuf);
      else
	regcache_raw_write (regcache, areg, valbuf);
    }
}


static enum return_value_convention
xtensa_return_value (struct gdbarch *gdbarch,
		     struct type *valtype,
		     struct regcache *regcache,
		     gdb_byte *readbuf,
		     const gdb_byte *writebuf)
{
  /* Structures up to 16 bytes are returned in registers.  */

  int struct_return = ((TYPE_CODE (valtype) == TYPE_CODE_STRUCT
			|| TYPE_CODE (valtype) == TYPE_CODE_UNION
			|| TYPE_CODE (valtype) == TYPE_CODE_ARRAY)
		       && TYPE_LENGTH (valtype) > 16);

  if (struct_return)
    return RETURN_VALUE_STRUCT_CONVENTION;

  DEBUGTRACE ("xtensa_return_value(...)\n");

  if (writebuf != NULL)
    {
      xtensa_store_return_value (valtype, regcache, writebuf);
    }

  if (readbuf != NULL)
    {
      gdb_assert (!struct_return);
      xtensa_extract_return_value (valtype, regcache, readbuf);
    }
  return RETURN_VALUE_REGISTER_CONVENTION;
}


/* DUMMY FRAME */

static CORE_ADDR
xtensa_push_dummy_call (struct gdbarch *gdbarch,
			struct value *function,
			struct regcache *regcache,
			CORE_ADDR bp_addr,
			int nargs,
			struct value **args,
			CORE_ADDR sp,
			int struct_return,
			CORE_ADDR struct_addr)
{
  int i;
  int size, onstack_size;
  gdb_byte *buf = (gdb_byte *) alloca (16);
  CORE_ADDR ra, ps;
  struct argument_info
  {
    const bfd_byte *contents;
    int length;
    int onstack;		/* onstack == 0 => in reg */
    int align;			/* alignment */
    union
    {
      int offset;		/* stack offset if on stack */
      int regno;		/* regno if in register */
    } u;
  };

  struct argument_info *arg_info =
    (struct argument_info *) alloca (nargs * sizeof (struct argument_info));

  CORE_ADDR osp = sp;

  DEBUGTRACE ("xtensa_push_dummy_call (...)\n");

  if (xtensa_debug_level > 3)
    {
      int i;
      DEBUGINFO ("[xtensa_push_dummy_call] nargs = %d\n", nargs);
      DEBUGINFO ("[xtensa_push_dummy_call] sp=0x%x, struct_return=%d, "
		 "struct_addr=0x%x\n",
		 (int) sp, (int) struct_return, (int) struct_addr);

      for (i = 0; i < nargs; i++)
        {
	  struct value *arg = args[i];
	  struct type *arg_type = check_typedef (value_type (arg));
	  fprintf_unfiltered (gdb_stdlog, "%2d: 0x%08x %3d ",
			      i, (int) arg, TYPE_LENGTH (arg_type));
	  switch (TYPE_CODE (arg_type))
	    {
	    case TYPE_CODE_INT:
	      fprintf_unfiltered (gdb_stdlog, "int");
	      break;
	    case TYPE_CODE_STRUCT:
	      fprintf_unfiltered (gdb_stdlog, "struct");
	      break;
	    default:
	      fprintf_unfiltered (gdb_stdlog, "%3d", TYPE_CODE (arg_type));
	      break;
	    }
	  fprintf_unfiltered (gdb_stdlog, " 0x%08x\n",
			      (unsigned int) value_contents (arg));
	}
    }

  /* First loop: collect information.
     Cast into type_long.  (This shouldn't happen often for C because
     GDB already does this earlier.)  It's possible that GDB could
     do it all the time but it's harmless to leave this code here.  */

  size = 0;
  onstack_size = 0;
  i = 0;

  if (struct_return)
    size = REGISTER_SIZE;

  for (i = 0; i < nargs; i++)
    {
      struct argument_info *info = &arg_info[i];
      struct value *arg = args[i];
      struct type *arg_type = check_typedef (value_type (arg));

      switch (TYPE_CODE (arg_type))
	{
	case TYPE_CODE_INT:
	case TYPE_CODE_BOOL:
	case TYPE_CODE_CHAR:
	case TYPE_CODE_RANGE:
	case TYPE_CODE_ENUM:

	  /* Cast argument to long if necessary as the mask does it too.  */
	  if (TYPE_LENGTH (arg_type) < TYPE_LENGTH (builtin_type_long))
	    {
	      arg_type = builtin_type_long;
	      arg = value_cast (arg_type, arg);
	    }
	  /* Aligment is equal to the type length for the basic types.  */
	  info->align = TYPE_LENGTH (arg_type);
	  break;

	case TYPE_CODE_FLT:

	  /* Align doubles correctly.  */
	  if (TYPE_LENGTH (arg_type) == TYPE_LENGTH (builtin_type_double))
	    info->align = TYPE_LENGTH (builtin_type_double);
	  else
	    info->align = TYPE_LENGTH (builtin_type_long);
	  break;

	case TYPE_CODE_STRUCT:
	default:
	  info->align = TYPE_LENGTH (builtin_type_long);
	  break;
	}
      info->length = TYPE_LENGTH (arg_type);
      info->contents = value_contents (arg);

      /* Align size and onstack_size.  */
      size = (size + info->align - 1) & ~(info->align - 1);
      onstack_size = (onstack_size + info->align - 1) & ~(info->align - 1);

      if (size + info->length > REGISTER_SIZE * ARG_NOF)
	{
	  info->onstack = 1;
	  info->u.offset = onstack_size;
	  onstack_size += info->length;
	}
      else
	{
	  info->onstack = 0;
	  info->u.regno = ARG_1ST + size / REGISTER_SIZE;
	}
      size += info->length;
    }

  /* Adjust the stack pointer and align it.  */
  sp = align_down (sp - onstack_size, SP_ALIGNMENT);

  /* Simulate MOVSP, if Windowed ABI.  */
  if ((CALL_ABI != CallAbiCall0Only) && (sp != osp))
    {
      read_memory (osp - 16, buf, 16);
      write_memory (sp - 16, buf, 16);
    }

  /* Second Loop: Load arguments.  */

  if (struct_return)
    {
      store_unsigned_integer (buf, REGISTER_SIZE, struct_addr);
      regcache_cooked_write (regcache, ARG_1ST, buf);
    }

  for (i = 0; i < nargs; i++)
    {
      struct argument_info *info = &arg_info[i];

      if (info->onstack)
	{
	  int n = info->length;
	  CORE_ADDR offset = sp + info->u.offset;

	  /* Odd-sized structs are aligned to the lower side of a memory
	     word in big-endian mode and require a shift.  This only
	     applies for structures smaller than one word.  */

	  if (n < REGISTER_SIZE
	      && gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
	    offset += (REGISTER_SIZE - n);

	  write_memory (offset, info->contents, info->length);

	}
      else
	{
	  int n = info->length;
	  const bfd_byte *cp = info->contents;
	  int r = info->u.regno;

	  /* Odd-sized structs are aligned to the lower side of registers in
	     big-endian mode and require a shift.  The odd-sized leftover will
	     be at the end.  Note that this is only true for structures smaller
	     than REGISTER_SIZE; for larger odd-sized structures the excess
	     will be left-aligned in the register on both endiannesses.  */

	  if (n < REGISTER_SIZE
	      && gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
	    {
	      ULONGEST v = extract_unsigned_integer (cp, REGISTER_SIZE);
	      v = v >> ((REGISTER_SIZE - n) * TARGET_CHAR_BIT);

	      store_unsigned_integer (buf, REGISTER_SIZE, v);
	      regcache_cooked_write (regcache, r, buf);

	      cp += REGISTER_SIZE;
	      n -= REGISTER_SIZE;
	      r++;
	    }
	  else
	    while (n > 0)
	      {
		regcache_cooked_write (regcache, r, cp);

		cp += REGISTER_SIZE;
		n -= REGISTER_SIZE;
		r++;
	      }
	}
    }

  /* Set the return address of dummy frame to the dummy address.
     The return address for the current function (in A0) is
     saved in the dummy frame, so we can savely overwrite A0 here.  */

  if (CALL_ABI != CallAbiCall0Only)
    {
      ra = (bp_addr & 0x3fffffff) | 0x40000000;
      regcache_raw_read (regcache, gdbarch_ps_regnum (current_gdbarch), buf);
      ps = extract_unsigned_integer (buf, 4) & ~0x00030000;
      regcache_cooked_write_unsigned (regcache, A4_REGNUM, ra);
      regcache_cooked_write_unsigned (regcache,
				      gdbarch_ps_regnum (current_gdbarch),
				      ps | 0x00010000);
    }
  else
    {
      /* Simulate CALL0: write RA into A0 register.  */
      regcache_cooked_write_unsigned (regcache, A0_REGNUM, bp_addr);
    }

  /* Set new stack pointer and return it.  */
  regcache_cooked_write_unsigned (regcache, A1_REGNUM, sp);
  /* Make dummy frame ID unique by adding a constant.  */
  return sp + SP_ALIGNMENT;
}


/* Return a breakpoint for the current location of PC.  We always use
   the density version if we have density instructions (regardless of the
   current instruction at PC), and use regular instructions otherwise.  */

#define BIG_BREAKPOINT { 0x00, 0x04, 0x00 }
#define LITTLE_BREAKPOINT { 0x00, 0x40, 0x00 }
#define DENSITY_BIG_BREAKPOINT { 0xd2, 0x0f }
#define DENSITY_LITTLE_BREAKPOINT { 0x2d, 0xf0 }

static const unsigned char *
xtensa_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  static unsigned char big_breakpoint[] = BIG_BREAKPOINT;
  static unsigned char little_breakpoint[] = LITTLE_BREAKPOINT;
  static unsigned char density_big_breakpoint[] = DENSITY_BIG_BREAKPOINT;
  static unsigned char density_little_breakpoint[] = DENSITY_LITTLE_BREAKPOINT;

  DEBUGTRACE ("xtensa_breakpoint_from_pc (pc = 0x%08x)\n", (int) *pcptr);

  if (ISA_USE_DENSITY_INSTRUCTIONS)
    {
      if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
	{
	  *lenptr = sizeof (density_big_breakpoint);
	  return density_big_breakpoint;
	}
      else
	{
	  *lenptr = sizeof (density_little_breakpoint);
	  return density_little_breakpoint;
	}
    }
  else
    {
      if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
	{
	  *lenptr = sizeof (big_breakpoint);
	  return big_breakpoint;
	}
      else
	{
	  *lenptr = sizeof (little_breakpoint);
	  return little_breakpoint;
	}
    }
}

/* Call0 ABI support routines.  */

/* Call0 opcode class.  Opcodes are preclassified according to what they
   mean for Call0 prologue analysis, and their number of significant operands.
   The purpose of this is to simplify prologue analysis by separating 
   instruction decoding (libisa) from the semantics of prologue analysis.  */

typedef enum {
  c0opc_illegal,       /* Unknown to libisa (invalid) or 'ill' opcode.  */
  c0opc_uninteresting, /* Not interesting for Call0 prologue analysis.  */
  c0opc_flow,	       /* Flow control insn.  */
  c0opc_entry,	       /* ENTRY indicates non-Call0 prologue.  */
  c0opc_break,	       /* Debugger software breakpoints.  */
  c0opc_add,	       /* Adding two registers.  */
  c0opc_addi,	       /* Adding a register and an immediate.  */
  c0opc_sub,	       /* Subtracting a register from a register.  */
  c0opc_mov,	       /* Moving a register to a register.  */
  c0opc_movi,	       /* Moving an immediate to a register.  */
  c0opc_l32r,	       /* Loading a literal.  */
  c0opc_s32i,	       /* Storing word at fixed offset from a base register.  */
  c0opc_NrOf	       /* Number of opcode classifications.  */
} xtensa_insn_kind;


/* Classify an opcode based on what it means for Call0 prologue analysis.  */

static xtensa_insn_kind
call0_classify_opcode (xtensa_isa isa, xtensa_opcode opc)
{
  const char *opcname;
  xtensa_insn_kind opclass = c0opc_uninteresting;

  DEBUGTRACE ("call0_classify_opcode (..., opc = %d)\n", opc);

  /* Get opcode name and handle special classifications.  */

  opcname = xtensa_opcode_name (isa, opc);

  if (opcname == NULL 
      || strcasecmp (opcname, "ill") == 0
      || strcasecmp (opcname, "ill.n") == 0)
    opclass = c0opc_illegal;
  else if (strcasecmp (opcname, "break") == 0
	   || strcasecmp (opcname, "break.n") == 0)
     opclass = c0opc_break;
  else if (strcasecmp (opcname, "entry") == 0)
    opclass = c0opc_entry;
  else if (xtensa_opcode_is_branch (isa, opc) > 0
	   || xtensa_opcode_is_jump   (isa, opc) > 0
	   || xtensa_opcode_is_loop   (isa, opc) > 0
	   || xtensa_opcode_is_call   (isa, opc) > 0
	   || strcasecmp (opcname, "simcall") == 0
	   || strcasecmp (opcname, "syscall") == 0)
    opclass = c0opc_flow;

  /* Also, classify specific opcodes that need to be tracked.  */
  else if (strcasecmp (opcname, "add") == 0 
	   || strcasecmp (opcname, "add.n") == 0)
    opclass = c0opc_add;
  else if (strcasecmp (opcname, "addi") == 0 
	   || strcasecmp (opcname, "addi.n") == 0
	   || strcasecmp (opcname, "addmi") == 0)
    opclass = c0opc_addi;
  else if (strcasecmp (opcname, "sub") == 0)
    opclass = c0opc_sub;
  else if (strcasecmp (opcname, "mov.n") == 0
	   || strcasecmp (opcname, "or") == 0) /* Could be 'mov' asm macro.  */
    opclass = c0opc_mov;
  else if (strcasecmp (opcname, "movi") == 0 
	   || strcasecmp (opcname, "movi.n") == 0)
    opclass = c0opc_movi;
  else if (strcasecmp (opcname, "l32r") == 0)
    opclass = c0opc_l32r;
  else if (strcasecmp (opcname, "s32i") == 0 
	   || strcasecmp (opcname, "s32i.n") == 0)
    opclass = c0opc_s32i;

  return opclass;
}

/* Tracks register movement/mutation for a given operation, which may
   be within a bundle.  Updates the destination register tracking info
   accordingly.  The pc is needed only for pc-relative load instructions
   (eg. l32r).  The SP register number is needed to identify stores to
   the stack frame.  */

static void
call0_track_op (xtensa_c0reg_t dst[], xtensa_c0reg_t src[],
		xtensa_insn_kind opclass, int nods, unsigned odv[],
		CORE_ADDR pc, int spreg)
{
  unsigned litbase, litaddr, litval;

  switch (opclass)
    {
    case c0opc_addi:
      /* 3 operands: dst, src, imm.  */
      gdb_assert (nods == 3);
      dst[odv[0]].fr_reg = src[odv[1]].fr_reg;
      dst[odv[0]].fr_ofs = src[odv[1]].fr_ofs + odv[2];
      break;
    case c0opc_add:
      /* 3 operands: dst, src1, src2.  */
      gdb_assert (nods == 3);
      if      (src[odv[1]].fr_reg == C0_CONST)
        {
	  dst[odv[0]].fr_reg = src[odv[2]].fr_reg;
	  dst[odv[0]].fr_ofs = src[odv[2]].fr_ofs + src[odv[1]].fr_ofs;
	}
      else if (src[odv[2]].fr_reg == C0_CONST)
        {
	  dst[odv[0]].fr_reg = src[odv[1]].fr_reg;
	  dst[odv[0]].fr_ofs = src[odv[1]].fr_ofs + src[odv[2]].fr_ofs;
	}
      else dst[odv[0]].fr_reg = C0_INEXP;
      break;
    case c0opc_sub:
      /* 3 operands: dst, src1, src2.  */
      gdb_assert (nods == 3);
      if      (src[odv[2]].fr_reg == C0_CONST)
        {
	  dst[odv[0]].fr_reg = src[odv[1]].fr_reg;
	  dst[odv[0]].fr_ofs = src[odv[1]].fr_ofs - src[odv[2]].fr_ofs;
	}
      else dst[odv[0]].fr_reg = C0_INEXP;
      break;
    case c0opc_mov:
      /* 2 operands: dst, src [, src].  */
      gdb_assert (nods == 2);
      dst[odv[0]].fr_reg = src[odv[1]].fr_reg;
      dst[odv[0]].fr_ofs = src[odv[1]].fr_ofs;
      break;
    case c0opc_movi:
      /* 2 operands: dst, imm.  */
      gdb_assert (nods == 2);
      dst[odv[0]].fr_reg = C0_CONST;
      dst[odv[0]].fr_ofs = odv[1];
      break;
    case c0opc_l32r:
      /* 2 operands: dst, literal offset.  */
      gdb_assert (nods == 2);
      /* litbase = xtensa_get_litbase (pc); can be also used.  */
      litbase = (LITBASE_REGNUM == -1)
	? 0 : xtensa_read_register (LITBASE_REGNUM);
      litaddr = litbase & 1
		  ? (litbase & ~1) + (signed)odv[1]
		  : (pc + 3  + (signed)odv[1]) & ~3;
      litval = read_memory_integer(litaddr, 4);
      dst[odv[0]].fr_reg = C0_CONST;
      dst[odv[0]].fr_ofs = litval;
      break;
    case c0opc_s32i:
      /* 3 operands: value, base, offset.  */
      gdb_assert (nods == 3 && spreg >= 0 && spreg < C0_NREGS);
      if (src[odv[1]].fr_reg == spreg	     /* Store to stack frame.  */
	  && (src[odv[1]].fr_ofs & 3) == 0   /* Alignment preserved.  */
	  &&  src[odv[0]].fr_reg >= 0	     /* Value is from a register.  */
	  &&  src[odv[0]].fr_ofs == 0	     /* Value hasn't been modified.  */
	  &&  src[src[odv[0]].fr_reg].to_stk == C0_NOSTK) /* First time.  */
        {
	  /* ISA encoding guarantees alignment.  But, check it anyway.  */
	  gdb_assert ((odv[2] & 3) == 0);
	  dst[src[odv[0]].fr_reg].to_stk = src[odv[1]].fr_ofs + odv[2];
	}
      break;
    default:
	gdb_assert (0);
    }
}

/* Analyze prologue of the function at start address to determine if it uses 
   the Call0 ABI, and if so track register moves and linear modifications
   in the prologue up to the PC or just beyond the prologue, whichever is first.
   An 'entry' instruction indicates non-Call0 ABI and the end of the prologue.
   The prologue may overlap non-prologue instructions but is guaranteed to end
   by the first flow-control instruction (jump, branch, call or return).
   Since an optimized function may move information around and change the
   stack frame arbitrarily during the prologue, the information is guaranteed
   valid only at the point in the function indicated by the PC.
   May be used to skip the prologue or identify the ABI, w/o tracking.

   Returns:   Address of first instruction after prologue, or PC (whichever 
	      is first), or 0, if decoding failed (in libisa).
   Input args:
      start   Start address of function/prologue.
      pc      Program counter to stop at.  Use 0 to continue to end of prologue.
	      If 0, avoids infinite run-on in corrupt code memory by bounding
	      the scan to the end of the function if that can be determined.
      nregs   Number of general registers to track (size of rt[] array).
   InOut args:
      rt[]    Array[nregs] of xtensa_c0reg structures for register tracking info.
	      If NULL, registers are not tracked.
   Output args:
      call0   If != NULL, *call0 is set non-zero if Call0 ABI used, else 0
	      (more accurately, non-zero until 'entry' insn is encountered).

      Note that these may produce useful results even if decoding fails
      because they begin with default assumptions that analysis may change.  */

static CORE_ADDR
call0_analyze_prologue (CORE_ADDR start, CORE_ADDR pc,
			int nregs, xtensa_c0reg_t rt[], int *call0)
{
  CORE_ADDR ia;		    /* Current insn address in prologue.  */
  CORE_ADDR ba = 0;	    /* Current address at base of insn buffer.  */
  CORE_ADDR bt;		    /* Current address at top+1 of insn buffer.  */
  #define BSZ 32	    /* Instruction buffer size.  */
  char ibuf[BSZ];	    /* Instruction buffer for decoding prologue.  */
  xtensa_isa isa;	    /* libisa ISA handle.  */
  xtensa_insnbuf ins, slot; /* libisa handle to decoded insn, slot.  */
  xtensa_format ifmt;	    /* libisa instruction format.  */
  int ilen, islots, is;	    /* Instruction length, nbr slots, current slot.  */
  xtensa_opcode opc;	    /* Opcode in current slot.  */
  xtensa_insn_kind opclass; /* Opcode class for Call0 prologue analysis.  */
  int nods;		    /* Opcode number of operands.  */
  unsigned odv[C0_MAXOPDS]; /* Operand values in order provided by libisa.  */
  xtensa_c0reg_t *rtmp;	    /* Register tracking info snapshot.  */
  int j;		    /* General loop counter.  */
  int fail = 0;		    /* Set non-zero and exit, if decoding fails.  */
  CORE_ADDR body_pc;	    /* The PC for the first non-prologue insn.  */
  CORE_ADDR end_pc;	    /* The PC for the lust function insn.  */

  struct symtab_and_line prologue_sal;

  DEBUGTRACE ("call0_analyze_prologue (start = 0x%08x, pc = 0x%08x, ...)\n", 
	      (int)start, (int)pc);

  /* Try to limit the scan to the end of the function if a non-zero pc
     arg was not supplied to avoid probing beyond the end of valid memory.
     If memory is full of garbage that classifies as c0opc_uninteresting.
     If this fails (eg. if no symbols) pc ends up 0 as it was.
     Intialize the Call0 frame and register tracking info.
     Assume it's Call0 until an 'entry' instruction is encountered.
     Assume we may be in the prologue until we hit a flow control instr.  */

  rtmp = NULL;
  body_pc = INT_MAX;
  end_pc = 0;

  /* Find out, if we have an information about the prologue from DWARF.  */
  prologue_sal = find_pc_line (start, 0);
  if (prologue_sal.line != 0) /* Found debug info.  */
    body_pc = prologue_sal.end;

  /* If we are going to analyze the prologue in general without knowing about
     the current PC, make the best assumtion for the end of the prologue.  */
  if (pc == 0)
    {
      find_pc_partial_function (start, 0, NULL, &end_pc);
      body_pc = min (end_pc, body_pc);
    }
  else
    body_pc = min (pc, body_pc);

  if (call0 != NULL)
      *call0 = 1;

  if (rt != NULL)
    {
      rtmp = (xtensa_c0reg_t*) alloca(nregs * sizeof(xtensa_c0reg_t));
      /* rt is already initialized in xtensa_alloc_frame_cache().  */
    }
  else nregs = 0;

  isa = xtensa_default_isa;
  gdb_assert (BSZ >= xtensa_isa_maxlength (isa));
  ins = xtensa_insnbuf_alloc (isa);
  slot = xtensa_insnbuf_alloc (isa);

  for (ia = start, bt = ia; ia < body_pc ; ia += ilen)
    {
      /* (Re)fill instruction buffer from memory if necessary, but do not
         read memory beyond PC to be sure we stay within text section
	 (this protection only works if a non-zero pc is supplied).  */

      if (ia + xtensa_isa_maxlength (isa) > bt)
        {
	  ba = ia;
	  bt = (ba + BSZ) < body_pc ? ba + BSZ : body_pc;
	  read_memory (ba, ibuf, bt - ba);
	}

      /* Decode format information.  */

      xtensa_insnbuf_from_chars (isa, ins, &ibuf[ia-ba], 0);
      ifmt = xtensa_format_decode (isa, ins);
      if (ifmt == XTENSA_UNDEFINED)
	{
	  fail = 1;
	  goto done;
	}
      ilen = xtensa_format_length (isa, ifmt);
      if (ilen == XTENSA_UNDEFINED)
	{
	  fail = 1;
	  goto done;
	}
      islots = xtensa_format_num_slots (isa, ifmt);
      if (islots == XTENSA_UNDEFINED)
	{
	  fail = 1;
	  goto done;
	}

      /* Analyze a bundle or a single instruction, using a snapshot of 
         the register tracking info as input for the entire bundle so that
	 register changes do not take effect within this bundle.  */

      for (j = 0; j < nregs; ++j)
	rtmp[j] = rt[j];

      for (is = 0; is < islots; ++is)
        {
	  /* Decode a slot and classify the opcode.  */

	  fail = xtensa_format_get_slot (isa, ifmt, is, ins, slot);
	  if (fail)
	    goto done;

	  opc = xtensa_opcode_decode (isa, ifmt, is, slot);
	  DEBUGVERB ("[call0_analyze_prologue] instr addr = 0x%08x, opc = %d\n", 
		     (unsigned)ia, opc);
	  if (opc == XTENSA_UNDEFINED) 
	    opclass = c0opc_illegal;
	  else
	    opclass = call0_classify_opcode (isa, opc);

	  /* Decide whether to track this opcode, ignore it, or bail out.  */

	  switch (opclass)
	    {
	    case c0opc_illegal:
	    case c0opc_break:
	      fail = 1;
	      goto done;

	    case c0opc_uninteresting:
	      continue;

	    case c0opc_flow:
	      goto done;

	    case c0opc_entry:
	      if (call0 != NULL)
		*call0 = 0;
	      ia += ilen;	       	/* Skip over 'entry' insn.  */
	      goto done;

	    default:
	      if (call0 != NULL)
		*call0 = 1;
	    }

	  /* Only expected opcodes should get this far.  */
	  if (rt == NULL)
	    continue;

	  /* Extract and decode the operands.  */
	  nods = xtensa_opcode_num_operands (isa, opc);
	  if (nods == XTENSA_UNDEFINED)
	    {
	      fail = 1;
	      goto done;
	    }

	  for (j = 0; j < nods && j < C0_MAXOPDS; ++j)
	    {
	      fail = xtensa_operand_get_field (isa, opc, j, ifmt, 
					       is, slot, &odv[j]);
	      if (fail)
		goto done;

	      fail = xtensa_operand_decode (isa, opc, j, &odv[j]);
	      if (fail)
		goto done;
	    }

	  /* Check operands to verify use of 'mov' assembler macro.  */
	  if (opclass == c0opc_mov && nods == 3)
	    {
	      if (odv[2] == odv[1])
		nods = 2;
	      else
		{
		  opclass = c0opc_uninteresting;
		  continue;
		}
	    }

	  /* Track register movement and modification for this operation.  */
	  call0_track_op (rt, rtmp, opclass, nods, odv, ia, 1);
	}
    }
done:
  DEBUGVERB ("[call0_analyze_prologue] stopped at instr addr 0x%08x, %s\n",
	     (unsigned)ia, fail ? "failed" : "succeeded");
  xtensa_insnbuf_free(isa, slot);
  xtensa_insnbuf_free(isa, ins);
  return fail ? 0 : ia;
}

/* Initialize frame cache for the current frame.  The "next_frame" is the next
   one relative to current frame.  "cache" is the pointer to the data structure
   we have to initialize.  "pc" is curretnt PC.  */

static void
call0_frame_cache (struct frame_info *next_frame,
		   xtensa_frame_cache_t *cache, CORE_ADDR pc)
{
  CORE_ADDR start_pc;		/* The beginning of the function.  */
  CORE_ADDR body_pc=UINT_MAX;	/* PC, where prologue analysis stopped.  */
  CORE_ADDR sp, fp, ra;
  int fp_regnum, c0_hasfp, c0_frmsz, prev_sp, to_stk;
 
  /* Find the beginning of the prologue of the function containing the PC
     and analyze it up to the PC or the end of the prologue.  */

  if (find_pc_partial_function (pc, NULL, &start_pc, NULL))
    {
      body_pc = call0_analyze_prologue (start_pc, pc, C0_NREGS,
					&cache->c0.c0_rt[0],
					&cache->call0);
    }
  
  sp = frame_unwind_register_unsigned (next_frame, A1_REGNUM);
  fp = sp; /* Assume FP == SP until proven otherwise.  */

  /* Get the frame information and FP (if used) at the current PC.
     If PC is in the prologue, the prologue analysis is more reliable
     than DWARF info.  We don't not know for sure if PC is in the prologue,
     but we know no calls have yet taken place, so we can almost
     certainly rely on the prologue analysis.  */

  if (body_pc <= pc)
    {
      /* Prologue analysis was successful up to the PC.
         It includes the cases when PC == START_PC.  */
      c0_hasfp = cache->c0.c0_rt[C0_FP].fr_reg == C0_SP;
      /* c0_hasfp == true means there is a frame pointer because
	 we analyzed the prologue and found that cache->c0.c0_rt[C0_FP]
	 was derived from SP.  Otherwise, it would be C0_FP.  */
      fp_regnum = c0_hasfp ? C0_FP : C0_SP;
      c0_frmsz = - cache->c0.c0_rt[fp_regnum].fr_ofs;
      fp_regnum += A0_BASE;
    }
  else  /* No data from the prologue analysis.  */
    {
      c0_hasfp = 0;
      fp_regnum = A0_BASE + C0_SP;
      c0_frmsz = 0;
      start_pc = pc;
   }

  prev_sp = fp + c0_frmsz;

  /* Frame size from debug info or prologue tracking does not account for 
     alloca() and other dynamic allocations.  Adjust frame size by FP - SP.  */
  if (c0_hasfp)
    {
      fp = frame_unwind_register_unsigned (next_frame, fp_regnum);

      /* Recalculate previous SP.  */
      prev_sp = fp + c0_frmsz;
      /* Update the stack frame size.  */
      c0_frmsz += fp - sp;
    }

  /* Get the return address (RA) from the stack if saved,
     or try to get it from a register.  */

  to_stk = cache->c0.c0_rt[C0_RA].to_stk;
  if (to_stk != C0_NOSTK)
    ra = (CORE_ADDR) 
      read_memory_integer (sp + c0_frmsz + cache->c0.c0_rt[C0_RA].to_stk, 4);

  else if (cache->c0.c0_rt[C0_RA].fr_reg == C0_CONST
	   && cache->c0.c0_rt[C0_RA].fr_ofs == 0)
    {
      /* Special case for terminating backtrace at a function that wants to
	 be seen as the outermost.  Such a function will clear it's RA (A0)
	 register to 0 in the prologue instead of saving its original value.  */
      ra = 0;
    }
  else
    {
      /* RA was copied to another register or (before any function call) may
	 still be in the original RA register.  This is not always reliable:
	 even in a leaf function, register tracking stops after prologue, and
	 even in prologue, non-prologue instructions (not tracked) may overwrite
	 RA or any register it was copied to.  If likely in prologue or before
	 any call, use retracking info and hope for the best (compiler should
	 have saved RA in stack if not in a leaf function).  If not in prologue,
	 too bad.  */

      int i;
      for (i = 0; 
	   (i < C0_NREGS) &&
	     (i == C0_RA || cache->c0.c0_rt[i].fr_reg != C0_RA);
	   ++i);
      if (i >= C0_NREGS && cache->c0.c0_rt[C0_RA].fr_reg == C0_RA)
	i = C0_RA;
      if (i < C0_NREGS) /* Read from the next_frame.  */
	{
	  ra = frame_unwind_register_unsigned (next_frame,
					       A0_REGNUM + cache->c0.c0_rt[i].fr_reg);
	}
      else ra = 0;
    }
  
  cache->pc = start_pc;
  cache->ra = ra;
  /* RA == 0 marks the outermost frame.  Do not go past it.  */
  cache->prev_sp = (ra != 0) ?  prev_sp : 0;
  cache->c0.fp_regnum = fp_regnum;
  cache->c0.c0_frmsz = c0_frmsz;
  cache->c0.c0_hasfp = c0_hasfp;
  cache->c0.c0_fp = fp;
}


/* Skip function prologue.

   Return the pc of the first instruction after prologue.  GDB calls this to
   find the address of the first line of the function or (if there is no line
   number information) to skip the prologue for planting breakpoints on 
   function entries.  Use debug info (if present) or prologue analysis to skip 
   the prologue to achieve reliable debugging behavior.  For windowed ABI, 
   only the 'entry' instruction is skipped.  It is not strictly necessary to 
   skip the prologue (Call0) or 'entry' (Windowed) because xt-gdb knows how to
   backtrace at any point in the prologue, however certain potential hazards 
   are avoided and a more "normal" debugging experience is ensured by 
   skipping the prologue (can be disabled by defining DONT_SKIP_PROLOG).
   For example, if we don't skip the prologue:
   - Some args may not yet have been saved to the stack where the debug
     info expects to find them (true anyway when only 'entry' is skipped);
   - Software breakpoints ('break' instrs) may not have been unplanted 
     when the prologue analysis is done on initializing the frame cache, 
     and breaks in the prologue will throw off the analysis.

   If we have debug info ( line-number info, in particular ) we simply skip
   the code associated with the first function line effectively skipping
   the prologue code.  It works even in cases like

   int main()
   {	int local_var = 1;
   	....
   }

   because, for this source code, both Xtensa compilers will generate two
   separate entries ( with the same line number ) in dwarf line-number
   section to make sure there is a boundary between the prologue code and
   the rest of the function.

   If there is no debug info, we need to analyze the code.  */

/* #define DONT_SKIP_PROLOGUE  */

CORE_ADDR
xtensa_skip_prologue (CORE_ADDR start_pc)
{
  struct symtab_and_line prologue_sal;
  CORE_ADDR body_pc;

  DEBUGTRACE ("xtensa_skip_prologue (start_pc = 0x%08x)\n", (int) start_pc);

#if DONT_SKIP_PROLOGUE
  return start_pc;
#endif

 /* Try to find first body line from debug info.  */

  prologue_sal = find_pc_line (start_pc, 0);
  if (prologue_sal.line != 0) /* Found debug info.  */
    {
      /* In Call0, it is possible to have a function with only one instruction
	 ('ret') resulting from a 1-line optimized function that does nothing.
	 In that case, prologue_sal.end may actually point to the start of the
	 next function in the text section, causing a breakpoint to be set at
	 the wrong place.  Check if the end address is in a different function,
	 and if so return the start PC.  We know we have symbol info.  */

      CORE_ADDR end_func;

      find_pc_partial_function (prologue_sal.end, NULL, &end_func, NULL);
      if (end_func != start_pc)
	return start_pc;

      return prologue_sal.end;
    }

  /* No debug line info.  Analyze prologue for Call0 or simply skip ENTRY.  */
  body_pc = call0_analyze_prologue(start_pc, 0, 0, NULL, NULL);
  return body_pc != 0 ? body_pc : start_pc;
}

/* Verify the current configuration.  */
static void
xtensa_verify_config (struct gdbarch *gdbarch)
{
  struct ui_file *log;
  struct cleanup *cleanups;
  struct gdbarch_tdep *tdep;
  long dummy;
  char *buf;

  tdep = gdbarch_tdep (gdbarch);
  log = mem_fileopen ();
  cleanups = make_cleanup_ui_file_delete (log);

  /* Verify that we got a reasonable number of AREGS.  */
  if ((tdep->num_aregs & -tdep->num_aregs) != tdep->num_aregs)
    fprintf_unfiltered (log, _("\
\n\tnum_aregs: Number of AR registers (%d) is not a power of two!"),
			tdep->num_aregs);

  /* Verify that certain registers exist.  */

  if (tdep->pc_regnum == -1)
    fprintf_unfiltered (log, _("\n\tpc_regnum: No PC register"));
  if (tdep->isa_use_exceptions && tdep->ps_regnum == -1)
    fprintf_unfiltered (log, _("\n\tps_regnum: No PS register"));

  if (tdep->isa_use_windowed_registers)
    {
      if (tdep->wb_regnum == -1)
	fprintf_unfiltered (log, _("\n\twb_regnum: No WB register"));
      if (tdep->ws_regnum == -1)
	fprintf_unfiltered (log, _("\n\tws_regnum: No WS register"));
      if (tdep->ar_base == -1)
	fprintf_unfiltered (log, _("\n\tar_base: No AR registers"));
    }

  if (tdep->a0_base == -1)
    fprintf_unfiltered (log, _("\n\ta0_base: No Ax registers"));

  buf = ui_file_xstrdup (log, &dummy);
  make_cleanup (xfree, buf);
  if (strlen (buf) > 0)
    internal_error (__FILE__, __LINE__,
		    _("the following are invalid: %s"), buf);
  do_cleanups (cleanups);
}

/* Module "constructor" function.  */

static struct gdbarch *
xtensa_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch_tdep *tdep;
  struct gdbarch *gdbarch;
  struct xtensa_abi_handler *abi_handler;

  DEBUGTRACE ("gdbarch_init()\n");

  /* We have to set the byte order before we call gdbarch_alloc.  */
  info.byte_order = xtensa_config_byte_order (&info);

  tdep = xtensa_config_tdep (&info);
  gdbarch = gdbarch_alloc (&info, tdep);

  /* Verify our configuration.  */
  xtensa_verify_config (gdbarch);

  /* Pseudo-Register read/write.  */
  set_gdbarch_pseudo_register_read (gdbarch, xtensa_pseudo_register_read);
  set_gdbarch_pseudo_register_write (gdbarch, xtensa_pseudo_register_write);

  /* Set target information.  */
  set_gdbarch_num_regs (gdbarch, tdep->num_regs);
  set_gdbarch_num_pseudo_regs (gdbarch, tdep->num_pseudo_regs);
  set_gdbarch_sp_regnum (gdbarch, tdep->a0_base + 1);
  set_gdbarch_pc_regnum (gdbarch, tdep->pc_regnum);
  set_gdbarch_ps_regnum (gdbarch, tdep->ps_regnum);

  /* Renumber registers for known formats (stab, dwarf, and dwarf2).  */
  set_gdbarch_stab_reg_to_regnum (gdbarch, xtensa_reg_to_regnum);
  set_gdbarch_dwarf_reg_to_regnum (gdbarch, xtensa_reg_to_regnum);
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, xtensa_reg_to_regnum);

  /* We provide our own function to get register information.  */
  set_gdbarch_register_name (gdbarch, xtensa_register_name);
  set_gdbarch_register_type (gdbarch, xtensa_register_type);

  /* To call functions from GDB using dummy frame */
  set_gdbarch_push_dummy_call (gdbarch, xtensa_push_dummy_call);

  set_gdbarch_believe_pcc_promotion (gdbarch, 1);

  set_gdbarch_return_value (gdbarch, xtensa_return_value);

  /* Advance PC across any prologue instructions to reach "real" code.  */
  set_gdbarch_skip_prologue (gdbarch, xtensa_skip_prologue);

  /* Stack grows downward.  */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  /* Set breakpoints.  */
  set_gdbarch_breakpoint_from_pc (gdbarch, xtensa_breakpoint_from_pc);

  /* After breakpoint instruction or illegal instruction, pc still
     points at break instruction, so don't decrement.  */
  set_gdbarch_decr_pc_after_break (gdbarch, 0);

  /* We don't skip args.  */
  set_gdbarch_frame_args_skip (gdbarch, 0);

  set_gdbarch_unwind_pc (gdbarch, xtensa_unwind_pc);

  set_gdbarch_frame_align (gdbarch, xtensa_frame_align);

  set_gdbarch_unwind_dummy_id (gdbarch, xtensa_unwind_dummy_id);

  /* Frame handling.  */
  frame_base_set_default (gdbarch, &xtensa_frame_base);
  frame_unwind_append_sniffer (gdbarch, xtensa_frame_sniffer);

  set_gdbarch_print_insn (gdbarch, print_insn_xtensa);

  set_gdbarch_have_nonsteppable_watchpoint (gdbarch, 1);

  xtensa_add_reggroups (gdbarch);
  set_gdbarch_register_reggroup_p (gdbarch, xtensa_register_reggroup_p);

  set_gdbarch_regset_from_core_section (gdbarch,
					xtensa_regset_from_core_section);

  return gdbarch;
}

static void
xtensa_dump_tdep (struct gdbarch *current_gdbarch, struct ui_file *file)
{
  error (_("xtensa_dump_tdep(): not implemented"));
}

void
_initialize_xtensa_tdep (void)
{
  struct cmd_list_element *c;

  gdbarch_register (bfd_arch_xtensa, xtensa_gdbarch_init, xtensa_dump_tdep);
  xtensa_init_reggroups ();

  add_setshow_zinteger_cmd ("xtensa",
			    class_maintenance,
			    &xtensa_debug_level, _("\
Set Xtensa debugging."), _("\
Show Xtensa debugging."), _("\
When non-zero, Xtensa-specific debugging is enabled. \
Can be 1, 2, 3, or 4 indicating the level of debugging."),
			    NULL,
			    NULL,
			    &setdebuglist, &showdebuglist);
}
