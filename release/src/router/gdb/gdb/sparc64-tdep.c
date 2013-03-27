/* Target-dependent code for UltraSPARC.

   Copyright (C) 2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

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
#include "dwarf2-frame.h"
#include "floatformat.h"
#include "frame.h"
#include "frame-base.h"
#include "frame-unwind.h"
#include "gdbcore.h"
#include "gdbtypes.h"
#include "inferior.h"
#include "symtab.h"
#include "objfiles.h"
#include "osabi.h"
#include "regcache.h"
#include "target.h"
#include "value.h"

#include "gdb_assert.h"
#include "gdb_string.h"

#include "sparc64-tdep.h"

/* This file implements the The SPARC 64-bit ABI as defined by the
   section "Low-Level System Information" of the SPARC Compliance
   Definition (SCD) 2.4.1, which is the 64-bit System V psABI for
   SPARC.  */

/* Please use the sparc32_-prefix for 32-bit specific code, the
   sparc64_-prefix for 64-bit specific code and the sparc_-prefix for
   code can handle both.  */

/* The functions on this page are intended to be used to classify
   function arguments.  */

/* Check whether TYPE is "Integral or Pointer".  */

static int
sparc64_integral_or_pointer_p (const struct type *type)
{
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_INT:
    case TYPE_CODE_BOOL:
    case TYPE_CODE_CHAR:
    case TYPE_CODE_ENUM:
    case TYPE_CODE_RANGE:
      {
	int len = TYPE_LENGTH (type);
	gdb_assert (len == 1 || len == 2 || len == 4 || len == 8);
      }
      return 1;
    case TYPE_CODE_PTR:
    case TYPE_CODE_REF:
      {
	int len = TYPE_LENGTH (type);
	gdb_assert (len == 8);
      }
      return 1;
    default:
      break;
    }

  return 0;
}

/* Check whether TYPE is "Floating".  */

static int
sparc64_floating_p (const struct type *type)
{
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_FLT:
      {
	int len = TYPE_LENGTH (type);
	gdb_assert (len == 4 || len == 8 || len == 16);
      }
      return 1;
    default:
      break;
    }

  return 0;
}

/* Check whether TYPE is "Structure or Union".  */

static int
sparc64_structure_or_union_p (const struct type *type)
{
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      return 1;
    default:
      break;
    }

  return 0;
}


/* Type for %pstate.  */
struct type *sparc64_pstate_type;

/* Type for %fsr.  */
struct type *sparc64_fsr_type;

/* Type for %fprs.  */
struct type *sparc64_fprs_type;

/* Construct types for ISA-specific registers.  */

static void
sparc64_init_types (void)
{
  struct type *type;

  type = init_flags_type ("builtin_type_sparc64_pstate", 8);
  append_flags_type_flag (type, 0, "AG");
  append_flags_type_flag (type, 1, "IE");
  append_flags_type_flag (type, 2, "PRIV");
  append_flags_type_flag (type, 3, "AM");
  append_flags_type_flag (type, 4, "PEF");
  append_flags_type_flag (type, 5, "RED");
  append_flags_type_flag (type, 8, "TLE");
  append_flags_type_flag (type, 9, "CLE");
  append_flags_type_flag (type, 10, "PID0");
  append_flags_type_flag (type, 11, "PID1");
  sparc64_pstate_type = type;

  type = init_flags_type ("builtin_type_sparc64_fsr", 8);
  append_flags_type_flag (type, 0, "NXA");
  append_flags_type_flag (type, 1, "DZA");
  append_flags_type_flag (type, 2, "UFA");
  append_flags_type_flag (type, 3, "OFA");
  append_flags_type_flag (type, 4, "NVA");
  append_flags_type_flag (type, 5, "NXC");
  append_flags_type_flag (type, 6, "DZC");
  append_flags_type_flag (type, 7, "UFC");
  append_flags_type_flag (type, 8, "OFC");
  append_flags_type_flag (type, 9, "NVC");
  append_flags_type_flag (type, 22, "NS");
  append_flags_type_flag (type, 23, "NXM");
  append_flags_type_flag (type, 24, "DZM");
  append_flags_type_flag (type, 25, "UFM");
  append_flags_type_flag (type, 26, "OFM");
  append_flags_type_flag (type, 27, "NVM");
  sparc64_fsr_type = type;

  type = init_flags_type ("builtin_type_sparc64_fprs", 8);
  append_flags_type_flag (type, 0, "DL");
  append_flags_type_flag (type, 1, "DU");
  append_flags_type_flag (type, 2, "FEF");
  sparc64_fprs_type = type;
}

/* Register information.  */

static const char *sparc64_register_names[] =
{
  "g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",
  "o0", "o1", "o2", "o3", "o4", "o5", "sp", "o7",
  "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7",
  "i0", "i1", "i2", "i3", "i4", "i5", "fp", "i7",

  "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",
  "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",
  "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
  "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",
  "f32", "f34", "f36", "f38", "f40", "f42", "f44", "f46",
  "f48", "f50", "f52", "f54", "f56", "f58", "f60", "f62",

  "pc", "npc",
  
  /* FIXME: Give "state" a name until we start using register groups.  */
  "state",
  "fsr",
  "fprs",
  "y",
};

/* Total number of registers.  */
#define SPARC64_NUM_REGS ARRAY_SIZE (sparc64_register_names)

/* We provide the aliases %d0..%d62 and %q0..%q60 for the floating
   registers as "psuedo" registers.  */

static const char *sparc64_pseudo_register_names[] =
{
  "cwp", "pstate", "asi", "ccr",

  "d0", "d2", "d4", "d6", "d8", "d10", "d12", "d14",
  "d16", "d18", "d20", "d22", "d24", "d26", "d28", "d30",
  "d32", "d34", "d36", "d38", "d40", "d42", "d44", "d46",
  "d48", "d50", "d52", "d54", "d56", "d58", "d60", "d62",

  "q0", "q4", "q8", "q12", "q16", "q20", "q24", "q28",
  "q32", "q36", "q40", "q44", "q48", "q52", "q56", "q60",
};

/* Total number of pseudo registers.  */
#define SPARC64_NUM_PSEUDO_REGS ARRAY_SIZE (sparc64_pseudo_register_names)

/* Return the name of register REGNUM.  */

static const char *
sparc64_register_name (int regnum)
{
  if (regnum >= 0 && regnum < SPARC64_NUM_REGS)
    return sparc64_register_names[regnum];

  if (regnum >= SPARC64_NUM_REGS
      && regnum < SPARC64_NUM_REGS + SPARC64_NUM_PSEUDO_REGS)
    return sparc64_pseudo_register_names[regnum - SPARC64_NUM_REGS];

  return NULL;
}

/* Return the GDB type object for the "standard" data type of data in
   register REGNUM. */

static struct type *
sparc64_register_type (struct gdbarch *gdbarch, int regnum)
{
  /* Raw registers.  */

  if (regnum == SPARC_SP_REGNUM || regnum == SPARC_FP_REGNUM)
    return builtin_type_void_data_ptr;
  if (regnum >= SPARC_G0_REGNUM && regnum <= SPARC_I7_REGNUM)
    return builtin_type_int64;
  if (regnum >= SPARC_F0_REGNUM && regnum <= SPARC_F31_REGNUM)
    return builtin_type_float;
  if (regnum >= SPARC64_F32_REGNUM && regnum <= SPARC64_F62_REGNUM)
    return builtin_type_double;
  if (regnum == SPARC64_PC_REGNUM || regnum == SPARC64_NPC_REGNUM)
    return builtin_type_void_func_ptr;
  /* This raw register contains the contents of %cwp, %pstate, %asi
     and %ccr as laid out in a %tstate register.  */
  if (regnum == SPARC64_STATE_REGNUM)
    return builtin_type_int64;
  if (regnum == SPARC64_FSR_REGNUM)
    return sparc64_fsr_type;
  if (regnum == SPARC64_FPRS_REGNUM)
    return sparc64_fprs_type;
  /* "Although Y is a 64-bit register, its high-order 32 bits are
     reserved and always read as 0."  */
  if (regnum == SPARC64_Y_REGNUM)
    return builtin_type_int64;

  /* Pseudo registers.  */

  if (regnum == SPARC64_CWP_REGNUM)
    return builtin_type_int64;
  if (regnum == SPARC64_PSTATE_REGNUM)
    return sparc64_pstate_type;
  if (regnum == SPARC64_ASI_REGNUM)
    return builtin_type_int64;
  if (regnum == SPARC64_CCR_REGNUM)
    return builtin_type_int64;
  if (regnum >= SPARC64_D0_REGNUM && regnum <= SPARC64_D62_REGNUM)
    return builtin_type_double;
  if (regnum >= SPARC64_Q0_REGNUM && regnum <= SPARC64_Q60_REGNUM)
    return builtin_type_long_double;

  internal_error (__FILE__, __LINE__, _("invalid regnum"));
}

static void
sparc64_pseudo_register_read (struct gdbarch *gdbarch,
			      struct regcache *regcache,
			      int regnum, gdb_byte *buf)
{
  gdb_assert (regnum >= SPARC64_NUM_REGS);

  if (regnum >= SPARC64_D0_REGNUM && regnum <= SPARC64_D30_REGNUM)
    {
      regnum = SPARC_F0_REGNUM + 2 * (regnum - SPARC64_D0_REGNUM);
      regcache_raw_read (regcache, regnum, buf);
      regcache_raw_read (regcache, regnum + 1, buf + 4);
    }
  else if (regnum >= SPARC64_D32_REGNUM && regnum <= SPARC64_D62_REGNUM)
    {
      regnum = SPARC64_F32_REGNUM + (regnum - SPARC64_D32_REGNUM);
      regcache_raw_read (regcache, regnum, buf);
    }
  else if (regnum >= SPARC64_Q0_REGNUM && regnum <= SPARC64_Q28_REGNUM)
    {
      regnum = SPARC_F0_REGNUM + 4 * (regnum - SPARC64_Q0_REGNUM);
      regcache_raw_read (regcache, regnum, buf);
      regcache_raw_read (regcache, regnum + 1, buf + 4);
      regcache_raw_read (regcache, regnum + 2, buf + 8);
      regcache_raw_read (regcache, regnum + 3, buf + 12);
    }
  else if (regnum >= SPARC64_Q32_REGNUM && regnum <= SPARC64_Q60_REGNUM)
    {
      regnum = SPARC64_F32_REGNUM + 2 * (regnum - SPARC64_Q32_REGNUM);
      regcache_raw_read (regcache, regnum, buf);
      regcache_raw_read (regcache, regnum + 1, buf + 8);
    }
  else if (regnum == SPARC64_CWP_REGNUM
	   || regnum == SPARC64_PSTATE_REGNUM
	   || regnum == SPARC64_ASI_REGNUM
	   || regnum == SPARC64_CCR_REGNUM)
    {
      ULONGEST state;

      regcache_raw_read_unsigned (regcache, SPARC64_STATE_REGNUM, &state);
      switch (regnum)
	{
	case SPARC64_CWP_REGNUM:
	  state = (state >> 0) & ((1 << 5) - 1);
	  break;
	case SPARC64_PSTATE_REGNUM:
	  state = (state >> 8) & ((1 << 12) - 1);
	  break;
	case SPARC64_ASI_REGNUM:
	  state = (state >> 24) & ((1 << 8) - 1);
	  break;
	case SPARC64_CCR_REGNUM:
	  state = (state >> 32) & ((1 << 8) - 1);
	  break;
	}
      store_unsigned_integer (buf, 8, state);
    }
}

static void
sparc64_pseudo_register_write (struct gdbarch *gdbarch,
			       struct regcache *regcache,
			       int regnum, const gdb_byte *buf)
{
  gdb_assert (regnum >= SPARC64_NUM_REGS);

  if (regnum >= SPARC64_D0_REGNUM && regnum <= SPARC64_D30_REGNUM)
    {
      regnum = SPARC_F0_REGNUM + 2 * (regnum - SPARC64_D0_REGNUM);
      regcache_raw_write (regcache, regnum, buf);
      regcache_raw_write (regcache, regnum + 1, buf + 4);
    }
  else if (regnum >= SPARC64_D32_REGNUM && regnum <= SPARC64_D62_REGNUM)
    {
      regnum = SPARC64_F32_REGNUM + (regnum - SPARC64_D32_REGNUM);
      regcache_raw_write (regcache, regnum, buf);
    }
  else if (regnum >= SPARC64_Q0_REGNUM && regnum <= SPARC64_Q28_REGNUM)
    {
      regnum = SPARC_F0_REGNUM + 4 * (regnum - SPARC64_Q0_REGNUM);
      regcache_raw_write (regcache, regnum, buf);
      regcache_raw_write (regcache, regnum + 1, buf + 4);
      regcache_raw_write (regcache, regnum + 2, buf + 8);
      regcache_raw_write (regcache, regnum + 3, buf + 12);
    }
  else if (regnum >= SPARC64_Q32_REGNUM && regnum <= SPARC64_Q60_REGNUM)
    {
      regnum = SPARC64_F32_REGNUM + 2 * (regnum - SPARC64_Q32_REGNUM);
      regcache_raw_write (regcache, regnum, buf);
      regcache_raw_write (regcache, regnum + 1, buf + 8);
    }
  else if (regnum == SPARC64_CWP_REGNUM
	   || regnum == SPARC64_PSTATE_REGNUM
	   || regnum == SPARC64_ASI_REGNUM
	   || regnum == SPARC64_CCR_REGNUM)
    {
      ULONGEST state, bits;

      regcache_raw_read_unsigned (regcache, SPARC64_STATE_REGNUM, &state);
      bits = extract_unsigned_integer (buf, 8);
      switch (regnum)
	{
	case SPARC64_CWP_REGNUM:
	  state |= ((bits & ((1 << 5) - 1)) << 0);
	  break;
	case SPARC64_PSTATE_REGNUM:
	  state |= ((bits & ((1 << 12) - 1)) << 8);
	  break;
	case SPARC64_ASI_REGNUM:
	  state |= ((bits & ((1 << 8) - 1)) << 24);
	  break;
	case SPARC64_CCR_REGNUM:
	  state |= ((bits & ((1 << 8) - 1)) << 32);
	  break;
	}
      regcache_raw_write_unsigned (regcache, SPARC64_STATE_REGNUM, state);
    }
}


/* Return PC of first real instruction of the function starting at
   START_PC.  */

static CORE_ADDR
sparc64_skip_prologue (CORE_ADDR start_pc)
{
  struct symtab_and_line sal;
  CORE_ADDR func_start, func_end;
  struct sparc_frame_cache cache;

  /* This is the preferred method, find the end of the prologue by
     using the debugging information.  */
  if (find_pc_partial_function (start_pc, NULL, &func_start, &func_end))
    {
      sal = find_pc_line (func_start, 0);

      if (sal.end < func_end
	  && start_pc <= sal.end)
	return sal.end;
    }

  return sparc_analyze_prologue (start_pc, 0xffffffffffffffffULL, &cache);
}

/* Normal frames.  */

static struct sparc_frame_cache *
sparc64_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  return sparc_frame_cache (next_frame, this_cache);
}

static void
sparc64_frame_this_id (struct frame_info *next_frame, void **this_cache,
		       struct frame_id *this_id)
{
  struct sparc_frame_cache *cache =
    sparc64_frame_cache (next_frame, this_cache);

  /* This marks the outermost frame.  */
  if (cache->base == 0)
    return;

  (*this_id) = frame_id_build (cache->base, cache->pc);
}

static void
sparc64_frame_prev_register (struct frame_info *next_frame, void **this_cache,
			     int regnum, int *optimizedp,
			     enum lval_type *lvalp, CORE_ADDR *addrp,
			     int *realnump, gdb_byte *valuep)
{
  struct sparc_frame_cache *cache =
    sparc64_frame_cache (next_frame, this_cache);

  if (regnum == SPARC64_PC_REGNUM || regnum == SPARC64_NPC_REGNUM)
    {
      *optimizedp = 0;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      if (valuep)
	{
	  CORE_ADDR pc = (regnum == SPARC64_NPC_REGNUM) ? 4 : 0;

	  regnum = cache->frameless_p ? SPARC_O7_REGNUM : SPARC_I7_REGNUM;
	  pc += frame_unwind_register_unsigned (next_frame, regnum) + 8;
	  store_unsigned_integer (valuep, 8, pc);
	}
      return;
    }

  /* Handle StackGhost.  */
  {
    ULONGEST wcookie = sparc_fetch_wcookie ();

    if (wcookie != 0 && !cache->frameless_p && regnum == SPARC_I7_REGNUM)
      {
	*optimizedp = 0;
	*lvalp = not_lval;
	*addrp = 0;
	*realnump = -1;
	if (valuep)
	  {
	    CORE_ADDR addr = cache->base + (regnum - SPARC_L0_REGNUM) * 8;
	    ULONGEST i7;

	    /* Read the value in from memory.  */
	    i7 = get_frame_memory_unsigned (next_frame, addr, 8);
	    store_unsigned_integer (valuep, 8, i7 ^ wcookie);
	  }
	return;
      }
  }

  /* The previous frame's `local' and `in' registers have been saved
     in the register save area.  */
  if (!cache->frameless_p
      && regnum >= SPARC_L0_REGNUM && regnum <= SPARC_I7_REGNUM)
    {
      *optimizedp = 0;
      *lvalp = lval_memory;
      *addrp = cache->base + (regnum - SPARC_L0_REGNUM) * 8;
      *realnump = -1;
      if (valuep)
	{
	  struct gdbarch *gdbarch = get_frame_arch (next_frame);

	  /* Read the value in from memory.  */
	  read_memory (*addrp, valuep, register_size (gdbarch, regnum));
	}
      return;
    }

  /* The previous frame's `out' registers are accessable as the
     current frame's `in' registers.  */
  if (!cache->frameless_p
      && regnum >= SPARC_O0_REGNUM && regnum <= SPARC_O7_REGNUM)
    regnum += (SPARC_I0_REGNUM - SPARC_O0_REGNUM);

  *optimizedp = 0;
  *lvalp = lval_register;
  *addrp = 0;
  *realnump = regnum;
  if (valuep)
    frame_unwind_register (next_frame, regnum, valuep);
}

static const struct frame_unwind sparc64_frame_unwind =
{
  NORMAL_FRAME,
  sparc64_frame_this_id,
  sparc64_frame_prev_register
};

static const struct frame_unwind *
sparc64_frame_sniffer (struct frame_info *next_frame)
{
  return &sparc64_frame_unwind;
}


static CORE_ADDR
sparc64_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct sparc_frame_cache *cache =
    sparc64_frame_cache (next_frame, this_cache);

  return cache->base;
}

static const struct frame_base sparc64_frame_base =
{
  &sparc64_frame_unwind,
  sparc64_frame_base_address,
  sparc64_frame_base_address,
  sparc64_frame_base_address
};

/* Check whether TYPE must be 16-byte aligned.  */

static int
sparc64_16_byte_align_p (struct type *type)
{
  if (sparc64_floating_p (type) && TYPE_LENGTH (type) == 16)
    return 1;

  if (sparc64_structure_or_union_p (type))
    {
      int i;

      for (i = 0; i < TYPE_NFIELDS (type); i++)
	{
	  struct type *subtype = check_typedef (TYPE_FIELD_TYPE (type, i));

	  if (sparc64_16_byte_align_p (subtype))
	    return 1;
	}
    }

  return 0;
}

/* Store floating fields of element ELEMENT of an "parameter array"
   that has type TYPE and is stored at BITPOS in VALBUF in the
   apropriate registers of REGCACHE.  This function can be called
   recursively and therefore handles floating types in addition to
   structures.  */

static void
sparc64_store_floating_fields (struct regcache *regcache, struct type *type,
			       const gdb_byte *valbuf, int element, int bitpos)
{
  gdb_assert (element < 16);

  if (sparc64_floating_p (type))
    {
      int len = TYPE_LENGTH (type);
      int regnum;

      if (len == 16)
	{
	  gdb_assert (bitpos == 0);
	  gdb_assert ((element % 2) == 0);

	  regnum = SPARC64_Q0_REGNUM + element / 2;
	  regcache_cooked_write (regcache, regnum, valbuf);
	}
      else if (len == 8)
	{
	  gdb_assert (bitpos == 0 || bitpos == 64);

	  regnum = SPARC64_D0_REGNUM + element + bitpos / 64;
	  regcache_cooked_write (regcache, regnum, valbuf + (bitpos / 8));
	}
      else
	{
	  gdb_assert (len == 4);
	  gdb_assert (bitpos % 32 == 0 && bitpos >= 0 && bitpos < 128);

	  regnum = SPARC_F0_REGNUM + element * 2 + bitpos / 32;
	  regcache_cooked_write (regcache, regnum, valbuf + (bitpos / 8));
	}
    }
  else if (sparc64_structure_or_union_p (type))
    {
      int i;

      for (i = 0; i < TYPE_NFIELDS (type); i++)
	{
	  struct type *subtype = check_typedef (TYPE_FIELD_TYPE (type, i));
	  int subpos = bitpos + TYPE_FIELD_BITPOS (type, i);

	  sparc64_store_floating_fields (regcache, subtype, valbuf,
					 element, subpos);
	}

      /* GCC has an interesting bug.  If TYPE is a structure that has
         a single `float' member, GCC doesn't treat it as a structure
         at all, but rather as an ordinary `float' argument.  This
         argument will be stored in %f1, as required by the psABI.
         However, as a member of a structure the psABI requires it to
         be stored in %f0.  This bug is present in GCC 3.3.2, but
         probably in older releases to.  To appease GCC, if a
         structure has only a single `float' member, we store its
         value in %f1 too (we already have stored in %f0).  */
      if (TYPE_NFIELDS (type) == 1)
	{
	  struct type *subtype = check_typedef (TYPE_FIELD_TYPE (type, 0));

	  if (sparc64_floating_p (subtype) && TYPE_LENGTH (subtype) == 4)
	    regcache_cooked_write (regcache, SPARC_F1_REGNUM, valbuf);
	}
    }
}

/* Fetch floating fields from a variable of type TYPE from the
   appropriate registers for BITPOS in REGCACHE and store it at BITPOS
   in VALBUF.  This function can be called recursively and therefore
   handles floating types in addition to structures.  */

static void
sparc64_extract_floating_fields (struct regcache *regcache, struct type *type,
				 gdb_byte *valbuf, int bitpos)
{
  if (sparc64_floating_p (type))
    {
      int len = TYPE_LENGTH (type);
      int regnum;

      if (len == 16)
	{
	  gdb_assert (bitpos == 0 || bitpos == 128);

	  regnum = SPARC64_Q0_REGNUM + bitpos / 128;
	  regcache_cooked_read (regcache, regnum, valbuf + (bitpos / 8));
	}
      else if (len == 8)
	{
	  gdb_assert (bitpos % 64 == 0 && bitpos >= 0 && bitpos < 256);

	  regnum = SPARC64_D0_REGNUM + bitpos / 64;
	  regcache_cooked_read (regcache, regnum, valbuf + (bitpos / 8));
	}
      else
	{
	  gdb_assert (len == 4);
	  gdb_assert (bitpos % 32 == 0 && bitpos >= 0 && bitpos < 256);

	  regnum = SPARC_F0_REGNUM + bitpos / 32;
	  regcache_cooked_read (regcache, regnum, valbuf + (bitpos / 8));
	}
    }
  else if (sparc64_structure_or_union_p (type))
    {
      int i;

      for (i = 0; i < TYPE_NFIELDS (type); i++)
	{
	  struct type *subtype = check_typedef (TYPE_FIELD_TYPE (type, i));
	  int subpos = bitpos + TYPE_FIELD_BITPOS (type, i);

	  sparc64_extract_floating_fields (regcache, subtype, valbuf, subpos);
	}
    }
}

/* Store the NARGS arguments ARGS and STRUCT_ADDR (if STRUCT_RETURN is
   non-zero) in REGCACHE and on the stack (starting from address SP).  */

static CORE_ADDR
sparc64_store_arguments (struct regcache *regcache, int nargs,
			 struct value **args, CORE_ADDR sp,
			 int struct_return, CORE_ADDR struct_addr)
{
  /* Number of extended words in the "parameter array".  */
  int num_elements = 0;
  int element = 0;
  int i;

  /* Take BIAS into account.  */
  sp += BIAS;

  /* First we calculate the number of extended words in the "parameter
     array".  While doing so we also convert some of the arguments.  */

  if (struct_return)
    num_elements++;

  for (i = 0; i < nargs; i++)
    {
      struct type *type = value_type (args[i]);
      int len = TYPE_LENGTH (type);

      if (sparc64_structure_or_union_p (type))
	{
	  /* Structure or Union arguments.  */
	  if (len <= 16)
	    {
	      if (num_elements % 2 && sparc64_16_byte_align_p (type))
		num_elements++;
	      num_elements += ((len + 7) / 8);
	    }
	  else
	    {
	      /* The psABI says that "Structures or unions larger than
		 sixteen bytes are copied by the caller and passed
		 indirectly; the caller will pass the address of a
		 correctly aligned structure value.  This sixty-four
		 bit address will occupy one word in the parameter
		 array, and may be promoted to an %o register like any
		 other pointer value."  Allocate memory for these
		 values on the stack.  */
	      sp -= len;

	      /* Use 16-byte alignment for these values.  That's
                 always correct, and wasting a few bytes shouldn't be
                 a problem.  */
	      sp &= ~0xf;

	      write_memory (sp, value_contents (args[i]), len);
	      args[i] = value_from_pointer (lookup_pointer_type (type), sp);
	      num_elements++;
	    }
	}
      else if (sparc64_floating_p (type))
	{
	  /* Floating arguments.  */

	  if (len == 16)
	    {
	      /* The psABI says that "Each quad-precision parameter
                 value will be assigned to two extended words in the
                 parameter array.  */
	      num_elements += 2;

	      /* The psABI says that "Long doubles must be
                 quad-aligned, and thus a hole might be introduced
                 into the parameter array to force alignment."  Skip
                 an element if necessary.  */
	      if (num_elements % 2)
		num_elements++;
	    }
	  else
	    num_elements++;
	}
      else
	{
	  /* Integral and pointer arguments.  */
	  gdb_assert (sparc64_integral_or_pointer_p (type));

	  /* The psABI says that "Each argument value of integral type
	     smaller than an extended word will be widened by the
	     caller to an extended word according to the signed-ness
	     of the argument type."  */
	  if (len < 8)
	    args[i] = value_cast (builtin_type_int64, args[i]);
	  num_elements++;
	}
    }

  /* Allocate the "parameter array".  */
  sp -= num_elements * 8;

  /* The psABI says that "Every stack frame must be 16-byte aligned."  */
  sp &= ~0xf;

  /* Now we store the arguments in to the "paramater array".  Some
     Integer or Pointer arguments and Structure or Union arguments
     will be passed in %o registers.  Some Floating arguments and
     floating members of structures are passed in floating-point
     registers.  However, for functions with variable arguments,
     floating arguments are stored in an %0 register, and for
     functions without a prototype floating arguments are stored in
     both a floating-point and an %o registers, or a floating-point
     register and memory.  To simplify the logic here we always pass
     arguments in memory, an %o register, and a floating-point
     register if appropriate.  This should be no problem since the
     contents of any unused memory or registers in the "parameter
     array" are undefined.  */

  if (struct_return)
    {
      regcache_cooked_write_unsigned (regcache, SPARC_O0_REGNUM, struct_addr);
      element++;
    }

  for (i = 0; i < nargs; i++)
    {
      const gdb_byte *valbuf = value_contents (args[i]);
      struct type *type = value_type (args[i]);
      int len = TYPE_LENGTH (type);
      int regnum = -1;
      gdb_byte buf[16];

      if (sparc64_structure_or_union_p (type))
	{
	  /* Structure or Union arguments.  */
	  gdb_assert (len <= 16);
	  memset (buf, 0, sizeof (buf));
	  valbuf = memcpy (buf, valbuf, len);

	  if (element % 2 && sparc64_16_byte_align_p (type))
	    element++;

	  if (element < 6)
	    {
	      regnum = SPARC_O0_REGNUM + element;
	      if (len > 8 && element < 5)
		regcache_cooked_write (regcache, regnum + 1, valbuf + 8);
	    }

	  if (element < 16)
	    sparc64_store_floating_fields (regcache, type, valbuf, element, 0);
	}
      else if (sparc64_floating_p (type))
	{
	  /* Floating arguments.  */
	  if (len == 16)
	    {
	      if (element % 2)
		element++;
	      if (element < 16)
		regnum = SPARC64_Q0_REGNUM + element / 2;
	    }
	  else if (len == 8)
	    {
	      if (element < 16)
		regnum = SPARC64_D0_REGNUM + element;
	    }
	  else
	    {
	      /* The psABI says "Each single-precision parameter value
                 will be assigned to one extended word in the
                 parameter array, and right-justified within that
                 word; the left half (even floatregister) is
                 undefined."  Even though the psABI says that "the
                 left half is undefined", set it to zero here.  */
	      memset (buf, 0, 4);
	      memcpy (buf + 4, valbuf, 4);
	      valbuf = buf;
	      len = 8;
	      if (element < 16)
		regnum = SPARC64_D0_REGNUM + element;
	    }
	}
      else
	{
	  /* Integral and pointer arguments.  */
	  gdb_assert (len == 8);
	  if (element < 6)
	    regnum = SPARC_O0_REGNUM + element;
	}

      if (regnum != -1)
	{
	  regcache_cooked_write (regcache, regnum, valbuf);

	  /* If we're storing the value in a floating-point register,
             also store it in the corresponding %0 register(s).  */
	  if (regnum >= SPARC64_D0_REGNUM && regnum <= SPARC64_D10_REGNUM)
	    {
	      gdb_assert (element < 6);
	      regnum = SPARC_O0_REGNUM + element;
	      regcache_cooked_write (regcache, regnum, valbuf);
	    }
	  else if (regnum >= SPARC64_Q0_REGNUM && regnum <= SPARC64_Q8_REGNUM)
	    {
	      gdb_assert (element < 6);
	      regnum = SPARC_O0_REGNUM + element;
	      regcache_cooked_write (regcache, regnum, valbuf);
	      regcache_cooked_write (regcache, regnum + 1, valbuf + 8);
	    }
	}

      /* Always store the argument in memory.  */
      write_memory (sp + element * 8, valbuf, len);
      element += ((len + 7) / 8);
    }

  gdb_assert (element == num_elements);

  /* Take BIAS into account.  */
  sp -= BIAS;
  return sp;
}

static CORE_ADDR
sparc64_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
			 struct regcache *regcache, CORE_ADDR bp_addr,
			 int nargs, struct value **args, CORE_ADDR sp,
			 int struct_return, CORE_ADDR struct_addr)
{
  /* Set return address.  */
  regcache_cooked_write_unsigned (regcache, SPARC_O7_REGNUM, bp_addr - 8);

  /* Set up function arguments.  */
  sp = sparc64_store_arguments (regcache, nargs, args, sp,
				struct_return, struct_addr);

  /* Allocate the register save area.  */
  sp -= 16 * 8;

  /* Stack should be 16-byte aligned at this point.  */
  gdb_assert ((sp + BIAS) % 16 == 0);

  /* Finally, update the stack pointer.  */
  regcache_cooked_write_unsigned (regcache, SPARC_SP_REGNUM, sp);

  return sp + BIAS;
}


/* Extract from an array REGBUF containing the (raw) register state, a
   function return value of TYPE, and copy that into VALBUF.  */

static void
sparc64_extract_return_value (struct type *type, struct regcache *regcache,
			      gdb_byte *valbuf)
{
  int len = TYPE_LENGTH (type);
  gdb_byte buf[32];
  int i;

  if (sparc64_structure_or_union_p (type))
    {
      /* Structure or Union return values.  */
      gdb_assert (len <= 32);

      for (i = 0; i < ((len + 7) / 8); i++)
	regcache_cooked_read (regcache, SPARC_O0_REGNUM + i, buf + i * 8);
      if (TYPE_CODE (type) != TYPE_CODE_UNION)
	sparc64_extract_floating_fields (regcache, type, buf, 0);
      memcpy (valbuf, buf, len);
    }
  else if (sparc64_floating_p (type))
    {
      /* Floating return values.  */
      for (i = 0; i < len / 4; i++)
	regcache_cooked_read (regcache, SPARC_F0_REGNUM + i, buf + i * 4);
      memcpy (valbuf, buf, len);
    }
  else if (TYPE_CODE (type) == TYPE_CODE_ARRAY)
    {
      /* Small arrays are returned the same way as small structures.  */
      gdb_assert (len <= 32);

      for (i = 0; i < ((len + 7) / 8); i++)
	regcache_cooked_read (regcache, SPARC_O0_REGNUM + i, buf + i * 8);
      memcpy (valbuf, buf, len);
    }
  else
    {
      /* Integral and pointer return values.  */
      gdb_assert (sparc64_integral_or_pointer_p (type));

      /* Just stripping off any unused bytes should preserve the
         signed-ness just fine.  */
      regcache_cooked_read (regcache, SPARC_O0_REGNUM, buf);
      memcpy (valbuf, buf + 8 - len, len);
    }
}

/* Write into the appropriate registers a function return value stored
   in VALBUF of type TYPE.  */

static void
sparc64_store_return_value (struct type *type, struct regcache *regcache,
			    const gdb_byte *valbuf)
{
  int len = TYPE_LENGTH (type);
  gdb_byte buf[16];
  int i;

  if (sparc64_structure_or_union_p (type))
    {
      /* Structure or Union return values.  */
      gdb_assert (len <= 32);

      /* Simplify matters by storing the complete value (including
         floating members) into %o0 and %o1.  Floating members are
         also store in the appropriate floating-point registers.  */
      memset (buf, 0, sizeof (buf));
      memcpy (buf, valbuf, len);
      for (i = 0; i < ((len + 7) / 8); i++)
	regcache_cooked_write (regcache, SPARC_O0_REGNUM + i, buf + i * 8);
      if (TYPE_CODE (type) != TYPE_CODE_UNION)
	sparc64_store_floating_fields (regcache, type, buf, 0, 0);
    }
  else if (sparc64_floating_p (type))
    {
      /* Floating return values.  */
      memcpy (buf, valbuf, len);
      for (i = 0; i < len / 4; i++)
	regcache_cooked_write (regcache, SPARC_F0_REGNUM + i, buf + i * 4);
    }
  else if (TYPE_CODE (type) == TYPE_CODE_ARRAY)
    {
      /* Small arrays are returned the same way as small structures.  */
      gdb_assert (len <= 32);

      memset (buf, 0, sizeof (buf));
      memcpy (buf, valbuf, len);
      for (i = 0; i < ((len + 7) / 8); i++)
	regcache_cooked_write (regcache, SPARC_O0_REGNUM + i, buf + i * 8);
    }
  else
    {
      /* Integral and pointer return values.  */
      gdb_assert (sparc64_integral_or_pointer_p (type));

      /* ??? Do we need to do any sign-extension here?  */
      memset (buf, 0, 8);
      memcpy (buf + 8 - len, valbuf, len);
      regcache_cooked_write (regcache, SPARC_O0_REGNUM, buf);
    }
}

static enum return_value_convention
sparc64_return_value (struct gdbarch *gdbarch, struct type *type,
		      struct regcache *regcache, gdb_byte *readbuf,
		      const gdb_byte *writebuf)
{
  if (TYPE_LENGTH (type) > 32)
    return RETURN_VALUE_STRUCT_CONVENTION;

  if (readbuf)
    sparc64_extract_return_value (type, regcache, readbuf);
  if (writebuf)
    sparc64_store_return_value (type, regcache, writebuf);

  return RETURN_VALUE_REGISTER_CONVENTION;
}


static void
sparc64_dwarf2_frame_init_reg (struct gdbarch *gdbarch, int regnum,
			       struct dwarf2_frame_state_reg *reg,
			       struct frame_info *next_frame)
{
  switch (regnum)
    {
    case SPARC_G0_REGNUM:
      /* Since %g0 is always zero, there is no point in saving it, and
	 people will be inclined omit it from the CFI.  Make sure we
	 don't warn about that.  */
      reg->how = DWARF2_FRAME_REG_SAME_VALUE;
      break;
    case SPARC_SP_REGNUM:
      reg->how = DWARF2_FRAME_REG_CFA;
      break;
    case SPARC64_PC_REGNUM:
      reg->how = DWARF2_FRAME_REG_RA_OFFSET;
      reg->loc.offset = 8;
      break;
    case SPARC64_NPC_REGNUM:
      reg->how = DWARF2_FRAME_REG_RA_OFFSET;
      reg->loc.offset = 12;
      break;
    }
}

void
sparc64_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->pc_regnum = SPARC64_PC_REGNUM;
  tdep->npc_regnum = SPARC64_NPC_REGNUM;

  /* This is what all the fuss is about.  */
  set_gdbarch_long_bit (gdbarch, 64);
  set_gdbarch_long_long_bit (gdbarch, 64);
  set_gdbarch_ptr_bit (gdbarch, 64);

  set_gdbarch_num_regs (gdbarch, SPARC64_NUM_REGS);
  set_gdbarch_register_name (gdbarch, sparc64_register_name);
  set_gdbarch_register_type (gdbarch, sparc64_register_type);
  set_gdbarch_num_pseudo_regs (gdbarch, SPARC64_NUM_PSEUDO_REGS);
  set_gdbarch_pseudo_register_read (gdbarch, sparc64_pseudo_register_read);
  set_gdbarch_pseudo_register_write (gdbarch, sparc64_pseudo_register_write);

  /* Register numbers of various important registers.  */
  set_gdbarch_pc_regnum (gdbarch, SPARC64_PC_REGNUM); /* %pc */

  /* Call dummy code.  */
  set_gdbarch_call_dummy_location (gdbarch, AT_ENTRY_POINT);
  set_gdbarch_push_dummy_code (gdbarch, NULL);
  set_gdbarch_push_dummy_call (gdbarch, sparc64_push_dummy_call);

  set_gdbarch_return_value (gdbarch, sparc64_return_value);
  set_gdbarch_stabs_argument_has_addr
    (gdbarch, default_stabs_argument_has_addr);

  set_gdbarch_skip_prologue (gdbarch, sparc64_skip_prologue);

  /* Hook in the DWARF CFI frame unwinder.  */
  dwarf2_frame_set_init_reg (gdbarch, sparc64_dwarf2_frame_init_reg);
  /* FIXME: kettenis/20050423: Don't enable the unwinder until the
     StackGhost issues have been resolved.  */

  frame_unwind_append_sniffer (gdbarch, sparc64_frame_sniffer);
  frame_base_set_default (gdbarch, &sparc64_frame_base);
}


/* Helper functions for dealing with register sets.  */

#define TSTATE_CWP	0x000000000000001fULL
#define TSTATE_ICC	0x0000000f00000000ULL
#define TSTATE_XCC	0x000000f000000000ULL

#define PSR_S		0x00000080
#define PSR_ICC		0x00f00000
#define PSR_VERS	0x0f000000
#define PSR_IMPL	0xf0000000
#define PSR_V8PLUS	0xff000000
#define PSR_XCC		0x000f0000

void
sparc64_supply_gregset (const struct sparc_gregset *gregset,
			struct regcache *regcache,
			int regnum, const void *gregs)
{
  int sparc32 = (gdbarch_ptr_bit (current_gdbarch) == 32);
  const gdb_byte *regs = gregs;
  int i;

  if (sparc32)
    {
      if (regnum == SPARC32_PSR_REGNUM || regnum == -1)
	{
	  int offset = gregset->r_tstate_offset;
	  ULONGEST tstate, psr;
	  gdb_byte buf[4];

	  tstate = extract_unsigned_integer (regs + offset, 8);
	  psr = ((tstate & TSTATE_CWP) | PSR_S | ((tstate & TSTATE_ICC) >> 12)
		 | ((tstate & TSTATE_XCC) >> 20) | PSR_V8PLUS);
	  store_unsigned_integer (buf, 4, psr);
	  regcache_raw_supply (regcache, SPARC32_PSR_REGNUM, buf);
	}

      if (regnum == SPARC32_PC_REGNUM || regnum == -1)
	regcache_raw_supply (regcache, SPARC32_PC_REGNUM,
			     regs + gregset->r_pc_offset + 4);

      if (regnum == SPARC32_NPC_REGNUM || regnum == -1)
	regcache_raw_supply (regcache, SPARC32_NPC_REGNUM,
			     regs + gregset->r_npc_offset + 4);

      if (regnum == SPARC32_Y_REGNUM || regnum == -1)
	{
	  int offset = gregset->r_y_offset + 8 - gregset->r_y_size;
	  regcache_raw_supply (regcache, SPARC32_Y_REGNUM, regs + offset);
	}
    }
  else
    {
      if (regnum == SPARC64_STATE_REGNUM || regnum == -1)
	regcache_raw_supply (regcache, SPARC64_STATE_REGNUM,
			     regs + gregset->r_tstate_offset);

      if (regnum == SPARC64_PC_REGNUM || regnum == -1)
	regcache_raw_supply (regcache, SPARC64_PC_REGNUM,
			     regs + gregset->r_pc_offset);

      if (regnum == SPARC64_NPC_REGNUM || regnum == -1)
	regcache_raw_supply (regcache, SPARC64_NPC_REGNUM,
			     regs + gregset->r_npc_offset);

      if (regnum == SPARC64_Y_REGNUM || regnum == -1)
	{
	  gdb_byte buf[8];

	  memset (buf, 0, 8);
	  memcpy (buf + 8 - gregset->r_y_size,
		  regs + gregset->r_y_offset, gregset->r_y_size);
	  regcache_raw_supply (regcache, SPARC64_Y_REGNUM, buf);
	}

      if ((regnum == SPARC64_FPRS_REGNUM || regnum == -1)
	  && gregset->r_fprs_offset != -1)
	regcache_raw_supply (regcache, SPARC64_FPRS_REGNUM,
			     regs + gregset->r_fprs_offset);
    }

  if (regnum == SPARC_G0_REGNUM || regnum == -1)
    regcache_raw_supply (regcache, SPARC_G0_REGNUM, NULL);

  if ((regnum >= SPARC_G1_REGNUM && regnum <= SPARC_O7_REGNUM) || regnum == -1)
    {
      int offset = gregset->r_g1_offset;

      if (sparc32)
	offset += 4;

      for (i = SPARC_G1_REGNUM; i <= SPARC_O7_REGNUM; i++)
	{
	  if (regnum == i || regnum == -1)
	    regcache_raw_supply (regcache, i, regs + offset);
	  offset += 8;
	}
    }

  if ((regnum >= SPARC_L0_REGNUM && regnum <= SPARC_I7_REGNUM) || regnum == -1)
    {
      /* Not all of the register set variants include Locals and
         Inputs.  For those that don't, we read them off the stack.  */
      if (gregset->r_l0_offset == -1)
	{
	  ULONGEST sp;

	  regcache_cooked_read_unsigned (regcache, SPARC_SP_REGNUM, &sp);
	  sparc_supply_rwindow (regcache, sp, regnum);
	}
      else
	{
	  int offset = gregset->r_l0_offset;

	  if (sparc32)
	    offset += 4;

	  for (i = SPARC_L0_REGNUM; i <= SPARC_I7_REGNUM; i++)
	    {
	      if (regnum == i || regnum == -1)
		regcache_raw_supply (regcache, i, regs + offset);
	      offset += 8;
	    }
	}
    }
}

void
sparc64_collect_gregset (const struct sparc_gregset *gregset,
			 const struct regcache *regcache,
			 int regnum, void *gregs)
{
  int sparc32 = (gdbarch_ptr_bit (current_gdbarch) == 32);
  gdb_byte *regs = gregs;
  int i;

  if (sparc32)
    {
      if (regnum == SPARC32_PSR_REGNUM || regnum == -1)
	{
	  int offset = gregset->r_tstate_offset;
	  ULONGEST tstate, psr;
	  gdb_byte buf[8];

	  tstate = extract_unsigned_integer (regs + offset, 8);
	  regcache_raw_collect (regcache, SPARC32_PSR_REGNUM, buf);
	  psr = extract_unsigned_integer (buf, 4);
	  tstate |= (psr & PSR_ICC) << 12;
	  if ((psr & (PSR_VERS | PSR_IMPL)) == PSR_V8PLUS)
	    tstate |= (psr & PSR_XCC) << 20;
	  store_unsigned_integer (buf, 8, tstate);
	  memcpy (regs + offset, buf, 8);
	}

      if (regnum == SPARC32_PC_REGNUM || regnum == -1)
	regcache_raw_collect (regcache, SPARC32_PC_REGNUM,
			      regs + gregset->r_pc_offset + 4);

      if (regnum == SPARC32_NPC_REGNUM || regnum == -1)
	regcache_raw_collect (regcache, SPARC32_NPC_REGNUM,
			      regs + gregset->r_npc_offset + 4);

      if (regnum == SPARC32_Y_REGNUM || regnum == -1)
	{
	  int offset = gregset->r_y_offset + 8 - gregset->r_y_size;
	  regcache_raw_collect (regcache, SPARC32_Y_REGNUM, regs + offset);
	}
    }
  else
    {
      if (regnum == SPARC64_STATE_REGNUM || regnum == -1)
	regcache_raw_collect (regcache, SPARC64_STATE_REGNUM,
			      regs + gregset->r_tstate_offset);

      if (regnum == SPARC64_PC_REGNUM || regnum == -1)
	regcache_raw_collect (regcache, SPARC64_PC_REGNUM,
			      regs + gregset->r_pc_offset);

      if (regnum == SPARC64_NPC_REGNUM || regnum == -1)
	regcache_raw_collect (regcache, SPARC64_NPC_REGNUM,
			      regs + gregset->r_npc_offset);

      if (regnum == SPARC64_Y_REGNUM || regnum == -1)
	{
	  gdb_byte buf[8];

	  regcache_raw_collect (regcache, SPARC64_Y_REGNUM, buf);
	  memcpy (regs + gregset->r_y_offset,
		  buf + 8 - gregset->r_y_size, gregset->r_y_size);
	}

      if ((regnum == SPARC64_FPRS_REGNUM || regnum == -1)
	  && gregset->r_fprs_offset != -1)
	regcache_raw_collect (regcache, SPARC64_FPRS_REGNUM,
			      regs + gregset->r_fprs_offset);

    }

  if ((regnum >= SPARC_G1_REGNUM && regnum <= SPARC_O7_REGNUM) || regnum == -1)
    {
      int offset = gregset->r_g1_offset;

      if (sparc32)
	offset += 4;

      /* %g0 is always zero.  */
      for (i = SPARC_G1_REGNUM; i <= SPARC_O7_REGNUM; i++)
	{
	  if (regnum == i || regnum == -1)
	    regcache_raw_collect (regcache, i, regs + offset);
	  offset += 8;
	}
    }

  if ((regnum >= SPARC_L0_REGNUM && regnum <= SPARC_I7_REGNUM) || regnum == -1)
    {
      /* Not all of the register set variants include Locals and
         Inputs.  For those that don't, we read them off the stack.  */
      if (gregset->r_l0_offset != -1)
	{
	  int offset = gregset->r_l0_offset;

	  if (sparc32)
	    offset += 4;

	  for (i = SPARC_L0_REGNUM; i <= SPARC_I7_REGNUM; i++)
	    {
	      if (regnum == i || regnum == -1)
		regcache_raw_collect (regcache, i, regs + offset);
	      offset += 8;
	    }
	}
    }
}

void
sparc64_supply_fpregset (struct regcache *regcache,
			 int regnum, const void *fpregs)
{
  int sparc32 = (gdbarch_ptr_bit (current_gdbarch) == 32);
  const gdb_byte *regs = fpregs;
  int i;

  for (i = 0; i < 32; i++)
    {
      if (regnum == (SPARC_F0_REGNUM + i) || regnum == -1)
	regcache_raw_supply (regcache, SPARC_F0_REGNUM + i, regs + (i * 4));
    }

  if (sparc32)
    {
      if (regnum == SPARC32_FSR_REGNUM || regnum == -1)
	regcache_raw_supply (regcache, SPARC32_FSR_REGNUM,
			     regs + (32 * 4) + (16 * 8) + 4);
    }
  else
    {
      for (i = 0; i < 16; i++)
	{
	  if (regnum == (SPARC64_F32_REGNUM + i) || regnum == -1)
	    regcache_raw_supply (regcache, SPARC64_F32_REGNUM + i,
				 regs + (32 * 4) + (i * 8));
	}

      if (regnum == SPARC64_FSR_REGNUM || regnum == -1)
	regcache_raw_supply (regcache, SPARC64_FSR_REGNUM,
			     regs + (32 * 4) + (16 * 8));
    }
}

void
sparc64_collect_fpregset (const struct regcache *regcache,
			  int regnum, void *fpregs)
{
  int sparc32 = (gdbarch_ptr_bit (current_gdbarch) == 32);
  gdb_byte *regs = fpregs;
  int i;

  for (i = 0; i < 32; i++)
    {
      if (regnum == (SPARC_F0_REGNUM + i) || regnum == -1)
	regcache_raw_collect (regcache, SPARC_F0_REGNUM + i, regs + (i * 4));
    }

  if (sparc32)
    {
      if (regnum == SPARC32_FSR_REGNUM || regnum == -1)
	regcache_raw_collect (regcache, SPARC32_FSR_REGNUM,
			      regs + (32 * 4) + (16 * 8) + 4);
    }
  else
    {
      for (i = 0; i < 16; i++)
	{
	  if (regnum == (SPARC64_F32_REGNUM + i) || regnum == -1)
	    regcache_raw_collect (regcache, SPARC64_F32_REGNUM + i,
				  regs + (32 * 4) + (i * 8));
	}

      if (regnum == SPARC64_FSR_REGNUM || regnum == -1)
	regcache_raw_collect (regcache, SPARC64_FSR_REGNUM,
			      regs + (32 * 4) + (16 * 8));
    }
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_sparc64_tdep (void);

void
_initialize_sparc64_tdep (void)
{
  /* Initialize the UltraSPARC-specific register types.  */
  sparc64_init_types();
}
