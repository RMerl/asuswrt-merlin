/* Target-dependent code for the Motorola 68000 series.

   Copyright (C) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1999, 2000, 2001,
   2002, 2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

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
#include "dwarf2-frame.h"
#include "frame.h"
#include "frame-base.h"
#include "frame-unwind.h"
#include "gdbtypes.h"
#include "symtab.h"
#include "gdbcore.h"
#include "value.h"
#include "gdb_string.h"
#include "gdb_assert.h"
#include "inferior.h"
#include "regcache.h"
#include "arch-utils.h"
#include "osabi.h"
#include "dis-asm.h"
#include "target-descriptions.h"

#include "m68k-tdep.h"


#define P_LINKL_FP	0x480e
#define P_LINKW_FP	0x4e56
#define P_PEA_FP	0x4856
#define P_MOVEAL_SP_FP	0x2c4f
#define P_ADDAW_SP	0xdefc
#define P_ADDAL_SP	0xdffc
#define P_SUBQW_SP	0x514f
#define P_SUBQL_SP	0x518f
#define P_LEA_SP_SP	0x4fef
#define P_LEA_PC_A5	0x4bfb0170
#define P_FMOVEMX_SP	0xf227
#define P_MOVEL_SP	0x2f00
#define P_MOVEML_SP	0x48e7

/* Offset from SP to first arg on stack at first instruction of a function */
#define SP_ARG0 (1 * 4)

#if !defined (BPT_VECTOR)
#define BPT_VECTOR 0xf
#endif

static const gdb_byte *
m68k_local_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  static gdb_byte break_insn[] = {0x4e, (0x40 | BPT_VECTOR)};
  *lenptr = sizeof (break_insn);
  return break_insn;
}


/* Type for %ps.  */
struct type *m68k_ps_type;

/* Construct types for ISA-specific registers.  */
static void
m68k_init_types (void)
{
  struct type *type;

  type = init_flags_type ("builtin_type_m68k_ps", 4);
  append_flags_type_flag (type, 0, "C");
  append_flags_type_flag (type, 1, "V");
  append_flags_type_flag (type, 2, "Z");
  append_flags_type_flag (type, 3, "N");
  append_flags_type_flag (type, 4, "X");
  append_flags_type_flag (type, 8, "I0");
  append_flags_type_flag (type, 9, "I1");
  append_flags_type_flag (type, 10, "I2");
  append_flags_type_flag (type, 12, "M");
  append_flags_type_flag (type, 13, "S");
  append_flags_type_flag (type, 14, "T0");
  append_flags_type_flag (type, 15, "T1");
  m68k_ps_type = type;
}

/* Return the GDB type object for the "standard" data type of data in
   register N.  This should be int for D0-D7, SR, FPCONTROL and
   FPSTATUS, long double for FP0-FP7, and void pointer for all others
   (A0-A7, PC, FPIADDR).  Note, for registers which contain
   addresses return pointer to void, not pointer to char, because we
   don't want to attempt to print the string after printing the
   address.  */

static struct type *
m68k_register_type (struct gdbarch *gdbarch, int regnum)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  if (tdep->fpregs_present)
    {
      if (regnum >= gdbarch_fp0_regnum (current_gdbarch)
	  && regnum <= gdbarch_fp0_regnum (current_gdbarch) + 7)
	{
	  if (tdep->flavour == m68k_coldfire_flavour)
	    return builtin_type (gdbarch)->builtin_double;
	  else
	    return builtin_type_m68881_ext;
	}

      if (regnum == M68K_FPI_REGNUM)
	return builtin_type_void_func_ptr;

      if (regnum == M68K_FPC_REGNUM || regnum == M68K_FPS_REGNUM)
	return builtin_type_int32;
    }
  else
    {
      if (regnum >= M68K_FP0_REGNUM && regnum <= M68K_FPI_REGNUM)
	return builtin_type_int0;
    }

  if (regnum == gdbarch_pc_regnum (current_gdbarch))
    return builtin_type_void_func_ptr;

  if (regnum >= M68K_A0_REGNUM && regnum <= M68K_A0_REGNUM + 7)
    return builtin_type_void_data_ptr;

  if (regnum == M68K_PS_REGNUM)
    return m68k_ps_type;

  return builtin_type_int32;
}

static const char *m68k_register_names[] = {
    "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
    "a0", "a1", "a2", "a3", "a4", "a5", "fp", "sp",
    "ps", "pc",
    "fp0", "fp1", "fp2", "fp3", "fp4", "fp5", "fp6", "fp7",
    "fpcontrol", "fpstatus", "fpiaddr"
  };

/* Function: m68k_register_name
   Returns the name of the standard m68k register regnum. */

static const char *
m68k_register_name (int regnum)
{
  if (regnum < 0 || regnum >= ARRAY_SIZE (m68k_register_names))
    internal_error (__FILE__, __LINE__,
		    _("m68k_register_name: illegal register number %d"), regnum);
  else
    return m68k_register_names[regnum];
}

/* Return nonzero if a value of type TYPE stored in register REGNUM
   needs any special handling.  */

static int
m68k_convert_register_p (int regnum, struct type *type)
{
  if (!gdbarch_tdep (current_gdbarch)->fpregs_present)
    return 0;
  return (regnum >= M68K_FP0_REGNUM && regnum <= M68K_FP0_REGNUM + 7);
}

/* Read a value of type TYPE from register REGNUM in frame FRAME, and
   return its contents in TO.  */

static void
m68k_register_to_value (struct frame_info *frame, int regnum,
			struct type *type, gdb_byte *to)
{
  gdb_byte from[M68K_MAX_REGISTER_SIZE];
  struct type *fpreg_type = register_type (current_gdbarch, M68K_FP0_REGNUM);

  /* We only support floating-point values.  */
  if (TYPE_CODE (type) != TYPE_CODE_FLT)
    {
      warning (_("Cannot convert floating-point register value "
	       "to non-floating-point type."));
      return;
    }

  /* Convert to TYPE.  This should be a no-op if TYPE is equivalent to
     the extended floating-point format used by the FPU.  */
  get_frame_register (frame, regnum, from);
  convert_typed_floating (from, fpreg_type, to, type);
}

/* Write the contents FROM of a value of type TYPE into register
   REGNUM in frame FRAME.  */

static void
m68k_value_to_register (struct frame_info *frame, int regnum,
			struct type *type, const gdb_byte *from)
{
  gdb_byte to[M68K_MAX_REGISTER_SIZE];
  struct type *fpreg_type = register_type (current_gdbarch, M68K_FP0_REGNUM);

  /* We only support floating-point values.  */
  if (TYPE_CODE (type) != TYPE_CODE_FLT)
    {
      warning (_("Cannot convert non-floating-point type "
	       "to floating-point register value."));
      return;
    }

  /* Convert from TYPE.  This should be a no-op if TYPE is equivalent
     to the extended floating-point format used by the FPU.  */
  convert_typed_floating (from, type, to, fpreg_type);
  put_frame_register (frame, regnum, to);
}


/* There is a fair number of calling conventions that are in somewhat
   wide use.  The 68000/08/10 don't support an FPU, not even as a
   coprocessor.  All function return values are stored in %d0/%d1.
   Structures are returned in a static buffer, a pointer to which is
   returned in %d0.  This means that functions returning a structure
   are not re-entrant.  To avoid this problem some systems use a
   convention where the caller passes a pointer to a buffer in %a1
   where the return values is to be stored.  This convention is the
   default, and is implemented in the function m68k_return_value.

   The 68020/030/040/060 do support an FPU, either as a coprocessor
   (68881/2) or built-in (68040/68060).  That's why System V release 4
   (SVR4) instroduces a new calling convention specified by the SVR4
   psABI.  Integer values are returned in %d0/%d1, pointer return
   values in %a0 and floating values in %fp0.  When calling functions
   returning a structure the caller should pass a pointer to a buffer
   for the return value in %a0.  This convention is implemented in the
   function m68k_svr4_return_value, and by appropriately setting the
   struct_value_regnum member of `struct gdbarch_tdep'.

   GNU/Linux returns values in the same way as SVR4 does, but uses %a1
   for passing the structure return value buffer.

   GCC can also generate code where small structures are returned in
   %d0/%d1 instead of in memory by using -freg-struct-return.  This is
   the default on NetBSD a.out, OpenBSD and GNU/Linux and several
   embedded systems.  This convention is implemented by setting the
   struct_return member of `struct gdbarch_tdep' to reg_struct_return.  */

/* Read a function return value of TYPE from REGCACHE, and copy that
   into VALBUF.  */

static void
m68k_extract_return_value (struct type *type, struct regcache *regcache,
			   gdb_byte *valbuf)
{
  int len = TYPE_LENGTH (type);
  gdb_byte buf[M68K_MAX_REGISTER_SIZE];

  if (len <= 4)
    {
      regcache_raw_read (regcache, M68K_D0_REGNUM, buf);
      memcpy (valbuf, buf + (4 - len), len);
    }
  else if (len <= 8)
    {
      regcache_raw_read (regcache, M68K_D0_REGNUM, buf);
      memcpy (valbuf, buf + (8 - len), len - 4);
      regcache_raw_read (regcache, M68K_D1_REGNUM, valbuf + (len - 4));
    }
  else
    internal_error (__FILE__, __LINE__,
		    _("Cannot extract return value of %d bytes long."), len);
}

static void
m68k_svr4_extract_return_value (struct type *type, struct regcache *regcache,
				gdb_byte *valbuf)
{
  int len = TYPE_LENGTH (type);
  gdb_byte buf[M68K_MAX_REGISTER_SIZE];
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  if (tdep->float_return && TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      struct type *fpreg_type = register_type 
	(current_gdbarch, M68K_FP0_REGNUM);
      regcache_raw_read (regcache, M68K_FP0_REGNUM, buf);
      convert_typed_floating (buf, fpreg_type, valbuf, type);
    }
  else if (TYPE_CODE (type) == TYPE_CODE_PTR && len == 4)
    regcache_raw_read (regcache, M68K_A0_REGNUM, valbuf);
  else
    m68k_extract_return_value (type, regcache, valbuf);
}

/* Write a function return value of TYPE from VALBUF into REGCACHE.  */

static void
m68k_store_return_value (struct type *type, struct regcache *regcache,
			 const gdb_byte *valbuf)
{
  int len = TYPE_LENGTH (type);

  if (len <= 4)
    regcache_raw_write_part (regcache, M68K_D0_REGNUM, 4 - len, len, valbuf);
  else if (len <= 8)
    {
      regcache_raw_write_part (regcache, M68K_D0_REGNUM, 8 - len,
			       len - 4, valbuf);
      regcache_raw_write (regcache, M68K_D1_REGNUM, valbuf + (len - 4));
    }
  else
    internal_error (__FILE__, __LINE__,
		    _("Cannot store return value of %d bytes long."), len);
}

static void
m68k_svr4_store_return_value (struct type *type, struct regcache *regcache,
			      const gdb_byte *valbuf)
{
  int len = TYPE_LENGTH (type);
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  if (tdep->float_return && TYPE_CODE (type) == TYPE_CODE_FLT)
    {
      struct type *fpreg_type = register_type 
	(current_gdbarch, M68K_FP0_REGNUM);
      gdb_byte buf[M68K_MAX_REGISTER_SIZE];
      convert_typed_floating (valbuf, type, buf, fpreg_type);
      regcache_raw_write (regcache, M68K_FP0_REGNUM, buf);
    }
  else if (TYPE_CODE (type) == TYPE_CODE_PTR && len == 4)
    {
      regcache_raw_write (regcache, M68K_A0_REGNUM, valbuf);
      regcache_raw_write (regcache, M68K_D0_REGNUM, valbuf);
    }
  else
    m68k_store_return_value (type, regcache, valbuf);
}

/* Return non-zero if TYPE, which is assumed to be a structure or
   union type, should be returned in registers for architecture
   GDBARCH.  */

static int
m68k_reg_struct_return_p (struct gdbarch *gdbarch, struct type *type)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  enum type_code code = TYPE_CODE (type);
  int len = TYPE_LENGTH (type);

  gdb_assert (code == TYPE_CODE_STRUCT || code == TYPE_CODE_UNION);

  if (tdep->struct_return == pcc_struct_return)
    return 0;

  return (len == 1 || len == 2 || len == 4 || len == 8);
}

/* Determine, for architecture GDBARCH, how a return value of TYPE
   should be returned.  If it is supposed to be returned in registers,
   and READBUF is non-zero, read the appropriate value from REGCACHE,
   and copy it into READBUF.  If WRITEBUF is non-zero, write the value
   from WRITEBUF into REGCACHE.  */

static enum return_value_convention
m68k_return_value (struct gdbarch *gdbarch, struct type *type,
		   struct regcache *regcache, gdb_byte *readbuf,
		   const gdb_byte *writebuf)
{
  enum type_code code = TYPE_CODE (type);

  /* GCC returns a `long double' in memory too.  */
  if (((code == TYPE_CODE_STRUCT || code == TYPE_CODE_UNION)
       && !m68k_reg_struct_return_p (gdbarch, type))
      || (code == TYPE_CODE_FLT && TYPE_LENGTH (type) == 12))
    {
      /* The default on m68k is to return structures in static memory.
         Consequently a function must return the address where we can
         find the return value.  */

      if (readbuf)
	{
	  ULONGEST addr;

	  regcache_raw_read_unsigned (regcache, M68K_D0_REGNUM, &addr);
	  read_memory (addr, readbuf, TYPE_LENGTH (type));
	}

      return RETURN_VALUE_ABI_RETURNS_ADDRESS;
    }

  if (readbuf)
    m68k_extract_return_value (type, regcache, readbuf);
  if (writebuf)
    m68k_store_return_value (type, regcache, writebuf);

  return RETURN_VALUE_REGISTER_CONVENTION;
}

static enum return_value_convention
m68k_svr4_return_value (struct gdbarch *gdbarch, struct type *type,
			struct regcache *regcache, gdb_byte *readbuf,
			const gdb_byte *writebuf)
{
  enum type_code code = TYPE_CODE (type);

  if ((code == TYPE_CODE_STRUCT || code == TYPE_CODE_UNION)
      && !m68k_reg_struct_return_p (gdbarch, type))
    {
      /* The System V ABI says that:

	 "A function returning a structure or union also sets %a0 to
	 the value it finds in %a0.  Thus when the caller receives
	 control again, the address of the returned object resides in
	 register %a0."

	 So the ABI guarantees that we can always find the return
	 value just after the function has returned.  */

      if (readbuf)
	{
	  ULONGEST addr;

	  regcache_raw_read_unsigned (regcache, M68K_A0_REGNUM, &addr);
	  read_memory (addr, readbuf, TYPE_LENGTH (type));
	}

      return RETURN_VALUE_ABI_RETURNS_ADDRESS;
    }

  /* This special case is for structures consisting of a single
     `float' or `double' member.  These structures are returned in
     %fp0.  For these structures, we call ourselves recursively,
     changing TYPE into the type of the first member of the structure.
     Since that should work for all structures that have only one
     member, we don't bother to check the member's type here.  */
  if (code == TYPE_CODE_STRUCT && TYPE_NFIELDS (type) == 1)
    {
      type = check_typedef (TYPE_FIELD_TYPE (type, 0));
      return m68k_svr4_return_value (gdbarch, type, regcache,
				     readbuf, writebuf);
    }

  if (readbuf)
    m68k_svr4_extract_return_value (type, regcache, readbuf);
  if (writebuf)
    m68k_svr4_store_return_value (type, regcache, writebuf);

  return RETURN_VALUE_REGISTER_CONVENTION;
}


/* Always align the frame to a 4-byte boundary.  This is required on
   coldfire and harmless on the rest.  */

static CORE_ADDR
m68k_frame_align (struct gdbarch *gdbarch, CORE_ADDR sp)
{
  /* Align the stack to four bytes.  */
  return sp & ~3;
}

static CORE_ADDR
m68k_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		      struct regcache *regcache, CORE_ADDR bp_addr, int nargs,
		      struct value **args, CORE_ADDR sp, int struct_return,
		      CORE_ADDR struct_addr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  gdb_byte buf[4];
  int i;

  /* Push arguments in reverse order.  */
  for (i = nargs - 1; i >= 0; i--)
    {
      struct type *value_type = value_enclosing_type (args[i]);
      int len = TYPE_LENGTH (value_type);
      int container_len = (len + 3) & ~3;
      int offset;

      /* Non-scalars bigger than 4 bytes are left aligned, others are
	 right aligned.  */
      if ((TYPE_CODE (value_type) == TYPE_CODE_STRUCT
	   || TYPE_CODE (value_type) == TYPE_CODE_UNION
	   || TYPE_CODE (value_type) == TYPE_CODE_ARRAY)
	  && len > 4)
	offset = 0;
      else
	offset = container_len - len;
      sp -= container_len;
      write_memory (sp + offset, value_contents_all (args[i]), len);
    }

  /* Store struct value address.  */
  if (struct_return)
    {
      store_unsigned_integer (buf, 4, struct_addr);
      regcache_cooked_write (regcache, tdep->struct_value_regnum, buf);
    }

  /* Store return address.  */
  sp -= 4;
  store_unsigned_integer (buf, 4, bp_addr);
  write_memory (sp, buf, 4);

  /* Finally, update the stack pointer...  */
  store_unsigned_integer (buf, 4, sp);
  regcache_cooked_write (regcache, M68K_SP_REGNUM, buf);

  /* ...and fake a frame pointer.  */
  regcache_cooked_write (regcache, M68K_FP_REGNUM, buf);

  /* DWARF2/GCC uses the stack address *before* the function call as a
     frame's CFA.  */
  return sp + 8;
}

/* Convert a dwarf or dwarf2 regnumber to a GDB regnum.  */

static int
m68k_dwarf_reg_to_regnum (int num)
{
  if (num < 8)
    /* d0..7 */
    return (num - 0) + M68K_D0_REGNUM;
  else if (num < 16)
    /* a0..7 */
    return (num - 8) + M68K_A0_REGNUM;
  else if (num < 24 && gdbarch_tdep (current_gdbarch)->fpregs_present)
    /* fp0..7 */
    return (num - 16) + M68K_FP0_REGNUM;
  else if (num == 25)
    /* pc */
    return M68K_PC_REGNUM;
  else
    return gdbarch_num_regs (current_gdbarch)
	   + gdbarch_num_pseudo_regs (current_gdbarch);
}


struct m68k_frame_cache
{
  /* Base address.  */
  CORE_ADDR base;
  CORE_ADDR sp_offset;
  CORE_ADDR pc;

  /* Saved registers.  */
  CORE_ADDR saved_regs[M68K_NUM_REGS];
  CORE_ADDR saved_sp;

  /* Stack space reserved for local variables.  */
  long locals;
};

/* Allocate and initialize a frame cache.  */

static struct m68k_frame_cache *
m68k_alloc_frame_cache (void)
{
  struct m68k_frame_cache *cache;
  int i;

  cache = FRAME_OBSTACK_ZALLOC (struct m68k_frame_cache);

  /* Base address.  */
  cache->base = 0;
  cache->sp_offset = -4;
  cache->pc = 0;

  /* Saved registers.  We initialize these to -1 since zero is a valid
     offset (that's where %fp is supposed to be stored).  */
  for (i = 0; i < M68K_NUM_REGS; i++)
    cache->saved_regs[i] = -1;

  /* Frameless until proven otherwise.  */
  cache->locals = -1;

  return cache;
}

/* Check whether PC points at a code that sets up a new stack frame.
   If so, it updates CACHE and returns the address of the first
   instruction after the sequence that sets removes the "hidden"
   argument from the stack or CURRENT_PC, whichever is smaller.
   Otherwise, return PC.  */

static CORE_ADDR
m68k_analyze_frame_setup (CORE_ADDR pc, CORE_ADDR current_pc,
			  struct m68k_frame_cache *cache)
{
  int op;

  if (pc >= current_pc)
    return current_pc;

  op = read_memory_unsigned_integer (pc, 2);

  if (op == P_LINKW_FP || op == P_LINKL_FP || op == P_PEA_FP)
    {
      cache->saved_regs[M68K_FP_REGNUM] = 0;
      cache->sp_offset += 4;
      if (op == P_LINKW_FP)
	{
	  /* link.w %fp, #-N */
	  /* link.w %fp, #0; adda.l #-N, %sp */
	  cache->locals = -read_memory_integer (pc + 2, 2);

	  if (pc + 4 < current_pc && cache->locals == 0)
	    {
	      op = read_memory_unsigned_integer (pc + 4, 2);
	      if (op == P_ADDAL_SP)
		{
		  cache->locals = read_memory_integer (pc + 6, 4);
		  return pc + 10;
		}
	    }

	  return pc + 4;
	}
      else if (op == P_LINKL_FP)
	{
	  /* link.l %fp, #-N */
	  cache->locals = -read_memory_integer (pc + 2, 4);
	  return pc + 6;
	}
      else
	{
	  /* pea (%fp); movea.l %sp, %fp */
	  cache->locals = 0;

	  if (pc + 2 < current_pc)
	    {
	      op = read_memory_unsigned_integer (pc + 2, 2);

	      if (op == P_MOVEAL_SP_FP)
		{
		  /* move.l %sp, %fp */
		  return pc + 4;
		}
	    }

	  return pc + 2;
	}
    }
  else if ((op & 0170777) == P_SUBQW_SP || (op & 0170777) == P_SUBQL_SP)
    {
      /* subq.[wl] #N,%sp */
      /* subq.[wl] #8,%sp; subq.[wl] #N,%sp */
      cache->locals = (op & 07000) == 0 ? 8 : (op & 07000) >> 9;
      if (pc + 2 < current_pc)
	{
	  op = read_memory_unsigned_integer (pc + 2, 2);
	  if ((op & 0170777) == P_SUBQW_SP || (op & 0170777) == P_SUBQL_SP)
	    {
	      cache->locals += (op & 07000) == 0 ? 8 : (op & 07000) >> 9;
	      return pc + 4;
	    }
	}
      return pc + 2;
    }
  else if (op == P_ADDAW_SP || op == P_LEA_SP_SP)
    {
      /* adda.w #-N,%sp */
      /* lea (-N,%sp),%sp */
      cache->locals = -read_memory_integer (pc + 2, 2);
      return pc + 4;
    }
  else if (op == P_ADDAL_SP)
    {
      /* adda.l #-N,%sp */
      cache->locals = -read_memory_integer (pc + 2, 4);
      return pc + 6;
    }

  return pc;
}

/* Check whether PC points at code that saves registers on the stack.
   If so, it updates CACHE and returns the address of the first
   instruction after the register saves or CURRENT_PC, whichever is
   smaller.  Otherwise, return PC.  */

static CORE_ADDR
m68k_analyze_register_saves (CORE_ADDR pc, CORE_ADDR current_pc,
			     struct m68k_frame_cache *cache)
{
  if (cache->locals >= 0)
    {
      CORE_ADDR offset;
      int op;
      int i, mask, regno;

      offset = -4 - cache->locals;
      while (pc < current_pc)
	{
	  op = read_memory_unsigned_integer (pc, 2);
	  if (op == P_FMOVEMX_SP
	      && gdbarch_tdep (current_gdbarch)->fpregs_present)
	    {
	      /* fmovem.x REGS,-(%sp) */
	      op = read_memory_unsigned_integer (pc + 2, 2);
	      if ((op & 0xff00) == 0xe000)
		{
		  mask = op & 0xff;
		  for (i = 0; i < 16; i++, mask >>= 1)
		    {
		      if (mask & 1)
			{
			  cache->saved_regs[i + M68K_FP0_REGNUM] = offset;
			  offset -= 12;
			}
		    }
		  pc += 4;
		}
	      else
		break;
	    }
	  else if ((op & 0177760) == P_MOVEL_SP)
	    {
	      /* move.l %R,-(%sp) */
	      regno = op & 017;
	      cache->saved_regs[regno] = offset;
	      offset -= 4;
	      pc += 2;
	    }
	  else if (op == P_MOVEML_SP)
	    {
	      /* movem.l REGS,-(%sp) */
	      mask = read_memory_unsigned_integer (pc + 2, 2);
	      for (i = 0; i < 16; i++, mask >>= 1)
		{
		  if (mask & 1)
		    {
		      cache->saved_regs[15 - i] = offset;
		      offset -= 4;
		    }
		}
	      pc += 4;
	    }
	  else
	    break;
	}
    }

  return pc;
}


/* Do a full analysis of the prologue at PC and update CACHE
   accordingly.  Bail out early if CURRENT_PC is reached.  Return the
   address where the analysis stopped.

   We handle all cases that can be generated by gcc.

   For allocating a stack frame:

   link.w %a6,#-N
   link.l %a6,#-N
   pea (%fp); move.l %sp,%fp
   link.w %a6,#0; add.l #-N,%sp
   subq.l #N,%sp
   subq.w #N,%sp
   subq.w #8,%sp; subq.w #N-8,%sp
   add.w #-N,%sp
   lea (-N,%sp),%sp
   add.l #-N,%sp

   For saving registers:

   fmovem.x REGS,-(%sp)
   move.l R1,-(%sp)
   move.l R1,-(%sp); move.l R2,-(%sp)
   movem.l REGS,-(%sp)

   For setting up the PIC register:

   lea (%pc,N),%a5

   */

static CORE_ADDR
m68k_analyze_prologue (CORE_ADDR pc, CORE_ADDR current_pc,
		       struct m68k_frame_cache *cache)
{
  unsigned int op;

  pc = m68k_analyze_frame_setup (pc, current_pc, cache);
  pc = m68k_analyze_register_saves (pc, current_pc, cache);
  if (pc >= current_pc)
    return current_pc;

  /* Check for GOT setup.  */
  op = read_memory_unsigned_integer (pc, 4);
  if (op == P_LEA_PC_A5)
    {
      /* lea (%pc,N),%a5 */
      return pc + 6;
    }

  return pc;
}

/* Return PC of first real instruction.  */

static CORE_ADDR
m68k_skip_prologue (CORE_ADDR start_pc)
{
  struct m68k_frame_cache cache;
  CORE_ADDR pc;
  int op;

  cache.locals = -1;
  pc = m68k_analyze_prologue (start_pc, (CORE_ADDR) -1, &cache);
  if (cache.locals < 0)
    return start_pc;
  return pc;
}

static CORE_ADDR
m68k_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  gdb_byte buf[8];

  frame_unwind_register (next_frame, gdbarch_pc_regnum (current_gdbarch), buf);
  return extract_typed_address (buf, builtin_type_void_func_ptr);
}

/* Normal frames.  */

static struct m68k_frame_cache *
m68k_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct m68k_frame_cache *cache;
  gdb_byte buf[4];
  int i;

  if (*this_cache)
    return *this_cache;

  cache = m68k_alloc_frame_cache ();
  *this_cache = cache;

  /* In principle, for normal frames, %fp holds the frame pointer,
     which holds the base address for the current stack frame.
     However, for functions that don't need it, the frame pointer is
     optional.  For these "frameless" functions the frame pointer is
     actually the frame pointer of the calling frame.  Signal
     trampolines are just a special case of a "frameless" function.
     They (usually) share their frame pointer with the frame that was
     in progress when the signal occurred.  */

  frame_unwind_register (next_frame, M68K_FP_REGNUM, buf);
  cache->base = extract_unsigned_integer (buf, 4);
  if (cache->base == 0)
    return cache;

  /* For normal frames, %pc is stored at 4(%fp).  */
  cache->saved_regs[M68K_PC_REGNUM] = 4;

  cache->pc = frame_func_unwind (next_frame, NORMAL_FRAME);
  if (cache->pc != 0)
    m68k_analyze_prologue (cache->pc, frame_pc_unwind (next_frame), cache);

  if (cache->locals < 0)
    {
      /* We didn't find a valid frame, which means that CACHE->base
	 currently holds the frame pointer for our calling frame.  If
	 we're at the start of a function, or somewhere half-way its
	 prologue, the function's frame probably hasn't been fully
	 setup yet.  Try to reconstruct the base address for the stack
	 frame by looking at the stack pointer.  For truly "frameless"
	 functions this might work too.  */

      frame_unwind_register (next_frame, M68K_SP_REGNUM, buf);
      cache->base = extract_unsigned_integer (buf, 4) + cache->sp_offset;
    }

  /* Now that we have the base address for the stack frame we can
     calculate the value of %sp in the calling frame.  */
  cache->saved_sp = cache->base + 8;

  /* Adjust all the saved registers such that they contain addresses
     instead of offsets.  */
  for (i = 0; i < M68K_NUM_REGS; i++)
    if (cache->saved_regs[i] != -1)
      cache->saved_regs[i] += cache->base;

  return cache;
}

static void
m68k_frame_this_id (struct frame_info *next_frame, void **this_cache,
		    struct frame_id *this_id)
{
  struct m68k_frame_cache *cache = m68k_frame_cache (next_frame, this_cache);

  /* This marks the outermost frame.  */
  if (cache->base == 0)
    return;

  /* See the end of m68k_push_dummy_call.  */
  *this_id = frame_id_build (cache->base + 8, cache->pc);
}

static void
m68k_frame_prev_register (struct frame_info *next_frame, void **this_cache,
			  int regnum, int *optimizedp,
			  enum lval_type *lvalp, CORE_ADDR *addrp,
			  int *realnump, gdb_byte *valuep)
{
  struct m68k_frame_cache *cache = m68k_frame_cache (next_frame, this_cache);

  gdb_assert (regnum >= 0);

  if (regnum == M68K_SP_REGNUM && cache->saved_sp)
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

  if (regnum < M68K_NUM_REGS && cache->saved_regs[regnum] != -1)
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

static const struct frame_unwind m68k_frame_unwind =
{
  NORMAL_FRAME,
  m68k_frame_this_id,
  m68k_frame_prev_register
};

static const struct frame_unwind *
m68k_frame_sniffer (struct frame_info *next_frame)
{
  return &m68k_frame_unwind;
}

static CORE_ADDR
m68k_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct m68k_frame_cache *cache = m68k_frame_cache (next_frame, this_cache);

  return cache->base;
}

static const struct frame_base m68k_frame_base =
{
  &m68k_frame_unwind,
  m68k_frame_base_address,
  m68k_frame_base_address,
  m68k_frame_base_address
};

static struct frame_id
m68k_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  gdb_byte buf[4];
  CORE_ADDR fp;

  frame_unwind_register (next_frame, M68K_FP_REGNUM, buf);
  fp = extract_unsigned_integer (buf, 4);

  /* See the end of m68k_push_dummy_call.  */
  return frame_id_build (fp + 8, frame_pc_unwind (next_frame));
}


/* Figure out where the longjmp will land.  Slurp the args out of the stack.
   We expect the first arg to be a pointer to the jmp_buf structure from which
   we extract the pc (JB_PC) that we will land at.  The pc is copied into PC.
   This routine returns true on success. */

static int
m68k_get_longjmp_target (struct frame_info *frame, CORE_ADDR *pc)
{
  gdb_byte *buf;
  CORE_ADDR sp, jb_addr;
  struct gdbarch_tdep *tdep = gdbarch_tdep (get_frame_arch (frame));

  if (tdep->jb_pc < 0)
    {
      internal_error (__FILE__, __LINE__,
		      _("m68k_get_longjmp_target: not implemented"));
      return 0;
    }

  buf = alloca (gdbarch_ptr_bit (current_gdbarch) / TARGET_CHAR_BIT);
  sp = get_frame_register_unsigned (frame, gdbarch_sp_regnum (current_gdbarch));

  if (target_read_memory (sp + SP_ARG0,	/* Offset of first arg on stack */
			  buf,
			  gdbarch_ptr_bit (current_gdbarch) / TARGET_CHAR_BIT))
    return 0;

  jb_addr = extract_unsigned_integer (buf, gdbarch_ptr_bit (current_gdbarch)
					     / TARGET_CHAR_BIT);

  if (target_read_memory (jb_addr + tdep->jb_pc * tdep->jb_elt_size, buf,
			  gdbarch_ptr_bit (current_gdbarch) / TARGET_CHAR_BIT))
    return 0;

  *pc = extract_unsigned_integer (buf, gdbarch_ptr_bit (current_gdbarch)
					 / TARGET_CHAR_BIT);
  return 1;
}


/* System V Release 4 (SVR4).  */

void
m68k_svr4_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* SVR4 uses a different calling convention.  */
  set_gdbarch_return_value (gdbarch, m68k_svr4_return_value);

  /* SVR4 uses %a0 instead of %a1.  */
  tdep->struct_value_regnum = M68K_A0_REGNUM;
}


/* Function: m68k_gdbarch_init
   Initializer function for the m68k gdbarch vector.
   Called by gdbarch.  Sets up the gdbarch vector(s) for this target. */

static struct gdbarch *
m68k_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch_tdep *tdep = NULL;
  struct gdbarch *gdbarch;
  struct gdbarch_list *best_arch;
  struct tdesc_arch_data *tdesc_data = NULL;
  int i;
  enum m68k_flavour flavour = m68k_no_flavour;
  int has_fp = 1;
  const struct floatformat **long_double_format = floatformats_m68881_ext;

  /* Check any target description for validity.  */
  if (tdesc_has_registers (info.target_desc))
    {
      const struct tdesc_feature *feature;
      int valid_p;

      feature = tdesc_find_feature (info.target_desc,
				    "org.gnu.gdb.m68k.core");
      if (feature != NULL)
	/* Do nothing.  */
	;

      if (feature == NULL)
	{
	  feature = tdesc_find_feature (info.target_desc,
					"org.gnu.gdb.coldfire.core");
	  if (feature != NULL)
	    flavour = m68k_coldfire_flavour;
	}

      if (feature == NULL)
	{
	  feature = tdesc_find_feature (info.target_desc,
					"org.gnu.gdb.fido.core");
	  if (feature != NULL)
	    flavour = m68k_fido_flavour;
	}

      if (feature == NULL)
	return NULL;

      tdesc_data = tdesc_data_alloc ();

      valid_p = 1;
      for (i = 0; i <= M68K_PC_REGNUM; i++)
	valid_p &= tdesc_numbered_register (feature, tdesc_data, i,
					    m68k_register_names[i]);

      if (!valid_p)
	{
	  tdesc_data_cleanup (tdesc_data);
	  return NULL;
	}

      feature = tdesc_find_feature (info.target_desc,
				    "org.gnu.gdb.coldfire.fp");
      if (feature != NULL)
	{
	  valid_p = 1;
	  for (i = M68K_FP0_REGNUM; i <= M68K_FPI_REGNUM; i++)
	    valid_p &= tdesc_numbered_register (feature, tdesc_data, i,
						m68k_register_names[i]);
	  if (!valid_p)
	    {
	      tdesc_data_cleanup (tdesc_data);
	      return NULL;
	    }
	}
      else
	has_fp = 0;
    }

  /* The mechanism for returning floating values from function
     and the type of long double depend on whether we're
     on ColdFire or standard m68k. */

  if (info.bfd_arch_info && info.bfd_arch_info->mach != 0)
    {
      const bfd_arch_info_type *coldfire_arch = 
	bfd_lookup_arch (bfd_arch_m68k, bfd_mach_mcf_isa_a_nodiv);

      if (coldfire_arch
	  && ((*info.bfd_arch_info->compatible) 
	      (info.bfd_arch_info, coldfire_arch)))
	flavour = m68k_coldfire_flavour;
    }
  
  /* If there is already a candidate, use it.  */
  for (best_arch = gdbarch_list_lookup_by_info (arches, &info);
       best_arch != NULL;
       best_arch = gdbarch_list_lookup_by_info (best_arch->next, &info))
    {
      if (flavour != gdbarch_tdep (best_arch->gdbarch)->flavour)
	continue;

      if (has_fp != gdbarch_tdep (best_arch->gdbarch)->fpregs_present)
	continue;

      break;
    }

  tdep = xmalloc (sizeof (struct gdbarch_tdep));
  gdbarch = gdbarch_alloc (&info, tdep);
  tdep->fpregs_present = has_fp;
  tdep->flavour = flavour;

  if (flavour == m68k_coldfire_flavour || flavour == m68k_fido_flavour)
    long_double_format = floatformats_ieee_double;
  set_gdbarch_long_double_format (gdbarch, long_double_format);
  set_gdbarch_long_double_bit (gdbarch, long_double_format[0]->totalsize);

  set_gdbarch_skip_prologue (gdbarch, m68k_skip_prologue);
  set_gdbarch_breakpoint_from_pc (gdbarch, m68k_local_breakpoint_from_pc);

  /* Stack grows down. */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_frame_align (gdbarch, m68k_frame_align);

  set_gdbarch_believe_pcc_promotion (gdbarch, 1);
  if (flavour == m68k_coldfire_flavour || flavour == m68k_fido_flavour)
    set_gdbarch_decr_pc_after_break (gdbarch, 2);

  set_gdbarch_frame_args_skip (gdbarch, 8);
  set_gdbarch_dwarf_reg_to_regnum (gdbarch, m68k_dwarf_reg_to_regnum);
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, m68k_dwarf_reg_to_regnum);

  set_gdbarch_register_type (gdbarch, m68k_register_type);
  set_gdbarch_register_name (gdbarch, m68k_register_name);
  set_gdbarch_num_regs (gdbarch, M68K_NUM_REGS);
  set_gdbarch_sp_regnum (gdbarch, M68K_SP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, M68K_PC_REGNUM);
  set_gdbarch_ps_regnum (gdbarch, M68K_PS_REGNUM);
  set_gdbarch_fp0_regnum (gdbarch, M68K_FP0_REGNUM);
  set_gdbarch_convert_register_p (gdbarch, m68k_convert_register_p);
  set_gdbarch_register_to_value (gdbarch,  m68k_register_to_value);
  set_gdbarch_value_to_register (gdbarch, m68k_value_to_register);

  if (has_fp)
    set_gdbarch_fp0_regnum (gdbarch, M68K_FP0_REGNUM);

  /* Try to figure out if the arch uses floating registers to return
     floating point values from functions.  */
  if (has_fp)
    {
      /* On ColdFire, floating point values are returned in D0.  */
      if (flavour == m68k_coldfire_flavour)
	tdep->float_return = 0;
      else
	tdep->float_return = 1;
    }
  else
    {
      /* No floating registers, so can't use them for returning values.  */
      tdep->float_return = 0;
    }

  /* Function call & return */
  set_gdbarch_push_dummy_call (gdbarch, m68k_push_dummy_call);
  set_gdbarch_return_value (gdbarch, m68k_return_value);


  /* Disassembler.  */
  set_gdbarch_print_insn (gdbarch, print_insn_m68k);

#if defined JB_PC && defined JB_ELEMENT_SIZE
  tdep->jb_pc = JB_PC;
  tdep->jb_elt_size = JB_ELEMENT_SIZE;
#else
  tdep->jb_pc = -1;
#endif
  tdep->struct_value_regnum = M68K_A1_REGNUM;
  tdep->struct_return = reg_struct_return;

  /* Frame unwinder.  */
  set_gdbarch_unwind_dummy_id (gdbarch, m68k_unwind_dummy_id);
  set_gdbarch_unwind_pc (gdbarch, m68k_unwind_pc);

  /* Hook in the DWARF CFI frame unwinder.  */
  frame_unwind_append_sniffer (gdbarch, dwarf2_frame_sniffer);

  frame_base_set_default (gdbarch, &m68k_frame_base);

  /* Hook in ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch);

  /* Now we have tuned the configuration, set a few final things,
     based on what the OS ABI has told us.  */

  if (tdep->jb_pc >= 0)
    set_gdbarch_get_longjmp_target (gdbarch, m68k_get_longjmp_target);

  frame_unwind_append_sniffer (gdbarch, m68k_frame_sniffer);

  if (tdesc_data)
    tdesc_use_registers (gdbarch, tdesc_data);

  return gdbarch;
}


static void
m68k_dump_tdep (struct gdbarch *current_gdbarch, struct ui_file *file)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  if (tdep == NULL)
    return;
}

extern initialize_file_ftype _initialize_m68k_tdep; /* -Wmissing-prototypes */

void
_initialize_m68k_tdep (void)
{
  gdbarch_register (bfd_arch_m68k, m68k_gdbarch_init, m68k_dump_tdep);

  /* Initialize the m68k-specific register types.  */
  m68k_init_types ();
}
