/* Target-dependent code for the VAX.

   Copyright (C) 1986, 1989, 1991, 1992, 1995, 1996, 1998, 1999, 2000, 2002,
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

#include "defs.h"
#include "arch-utils.h"
#include "dis-asm.h"
#include "floatformat.h"
#include "frame.h"
#include "frame-base.h"
#include "frame-unwind.h"
#include "gdbcore.h"
#include "gdbtypes.h"
#include "osabi.h"
#include "regcache.h"
#include "regset.h"
#include "trad-frame.h"
#include "value.h"

#include "gdb_string.h"

#include "vax-tdep.h"

/* Return the name of register REGNUM.  */

static const char *
vax_register_name (int regnum)
{
  static char *register_names[] =
  {
    "r0", "r1", "r2",  "r3",  "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "ap", "fp", "sp", "pc",
    "ps",
  };

  if (regnum >= 0 && regnum < ARRAY_SIZE (register_names))
    return register_names[regnum];

  return NULL;
}

/* Return the GDB type object for the "standard" data type of data in
   register REGNUM. */

static struct type *
vax_register_type (struct gdbarch *gdbarch, int regnum)
{
  return builtin_type_int;
}

/* Core file support.  */

/* Supply register REGNUM from the buffer specified by GREGS and LEN
   in the general-purpose register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
vax_supply_gregset (const struct regset *regset, struct regcache *regcache,
		    int regnum, const void *gregs, size_t len)
{
  const gdb_byte *regs = gregs;
  int i;

  for (i = 0; i < VAX_NUM_REGS; i++)
    {
      if (regnum == i || regnum == -1)
	regcache_raw_supply (regcache, i, regs + i * 4);
    }
}

/* VAX register set.  */

static struct regset vax_gregset =
{
  NULL,
  vax_supply_gregset
};

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

static const struct regset *
vax_regset_from_core_section (struct gdbarch *gdbarch,
			      const char *sect_name, size_t sect_size)
{
  if (strcmp (sect_name, ".reg") == 0 && sect_size >= VAX_NUM_REGS * 4)
    return &vax_gregset;

  return NULL;
}

/* The VAX UNIX calling convention uses R1 to pass a structure return
   value address instead of passing it as a first (hidden) argument as
   the VMS calling convention suggests.  */

static CORE_ADDR
vax_store_arguments (struct regcache *regcache, int nargs,
		     struct value **args, CORE_ADDR sp)
{
  gdb_byte buf[4];
  int count = 0;
  int i;

  /* We create an argument list on the stack, and make the argument
     pointer to it.  */

  /* Push arguments in reverse order.  */
  for (i = nargs - 1; i >= 0; i--)
    {
      int len = TYPE_LENGTH (value_enclosing_type (args[i]));

      sp -= (len + 3) & ~3;
      count += (len + 3) / 4;
      write_memory (sp, value_contents_all (args[i]), len);
    }

  /* Push argument count.  */
  sp -= 4;
  store_unsigned_integer (buf, 4, count);
  write_memory (sp, buf, 4);

  /* Update the argument pointer.  */
  store_unsigned_integer (buf, 4, sp);
  regcache_cooked_write (regcache, VAX_AP_REGNUM, buf);

  return sp;
}

static CORE_ADDR
vax_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		     struct regcache *regcache, CORE_ADDR bp_addr, int nargs,
		     struct value **args, CORE_ADDR sp, int struct_return,
		     CORE_ADDR struct_addr)
{
  CORE_ADDR fp = sp;
  gdb_byte buf[4];

  /* Set up the function arguments.  */
  sp = vax_store_arguments (regcache, nargs, args, sp);

  /* Store return value address.  */
  if (struct_return)
    regcache_cooked_write_unsigned (regcache, VAX_R1_REGNUM, struct_addr);

  /* Store return address in the PC slot.  */
  sp -= 4;
  store_unsigned_integer (buf, 4, bp_addr);
  write_memory (sp, buf, 4);

  /* Store the (fake) frame pointer in the FP slot.  */
  sp -= 4;
  store_unsigned_integer (buf, 4, fp);
  write_memory (sp, buf, 4);

  /* Skip the AP slot.  */
  sp -= 4;

  /* Store register save mask and control bits.  */
  sp -= 4;
  store_unsigned_integer (buf, 4, 0);
  write_memory (sp, buf, 4);

  /* Store condition handler.  */
  sp -= 4;
  store_unsigned_integer (buf, 4, 0);
  write_memory (sp, buf, 4);

  /* Update the stack pointer and frame pointer.  */
  store_unsigned_integer (buf, 4, sp);
  regcache_cooked_write (regcache, VAX_SP_REGNUM, buf);
  regcache_cooked_write (regcache, VAX_FP_REGNUM, buf);

  /* Return the saved (fake) frame pointer.  */
  return fp;
}

static struct frame_id
vax_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  CORE_ADDR fp;

  fp = frame_unwind_register_unsigned (next_frame, VAX_FP_REGNUM);
  return frame_id_build (fp, frame_pc_unwind (next_frame));
}


static enum return_value_convention
vax_return_value (struct gdbarch *gdbarch, struct type *type,
		  struct regcache *regcache, gdb_byte *readbuf,
		  const gdb_byte *writebuf)
{
  int len = TYPE_LENGTH (type);
  gdb_byte buf[8];

  if (TYPE_CODE (type) == TYPE_CODE_STRUCT
      || TYPE_CODE (type) == TYPE_CODE_UNION
      || TYPE_CODE (type) == TYPE_CODE_ARRAY)
    {
      /* The default on VAX is to return structures in static memory.
         Consequently a function must return the address where we can
         find the return value.  */

      if (readbuf)
	{
	  ULONGEST addr;

	  regcache_raw_read_unsigned (regcache, VAX_R0_REGNUM, &addr);
	  read_memory (addr, readbuf, len);
	}

      return RETURN_VALUE_ABI_RETURNS_ADDRESS;
    }

  if (readbuf)
    {
      /* Read the contents of R0 and (if necessary) R1.  */
      regcache_cooked_read (regcache, VAX_R0_REGNUM, buf);
      if (len > 4)
	regcache_cooked_read (regcache, VAX_R1_REGNUM, buf + 4);
      memcpy (readbuf, buf, len);
    }
  if (writebuf)
    {
      /* Read the contents to R0 and (if necessary) R1.  */
      memcpy (buf, writebuf, len);
      regcache_cooked_write (regcache, VAX_R0_REGNUM, buf);
      if (len > 4)
	regcache_cooked_write (regcache, VAX_R1_REGNUM, buf + 4);
    }

  return RETURN_VALUE_REGISTER_CONVENTION;
}


/* Use the program counter to determine the contents and size of a
   breakpoint instruction.  Return a pointer to a string of bytes that
   encode a breakpoint instruction, store the length of the string in
   *LEN and optionally adjust *PC to point to the correct memory
   location for inserting the breakpoint.  */
   
static const gdb_byte *
vax_breakpoint_from_pc (CORE_ADDR *pc, int *len)
{
  static gdb_byte break_insn[] = { 3 };

  *len = sizeof (break_insn);
  return break_insn;
}

/* Advance PC across any function entry prologue instructions
   to reach some "real" code.  */

static CORE_ADDR
vax_skip_prologue (CORE_ADDR pc)
{
  gdb_byte op = read_memory_unsigned_integer (pc, 1);

  if (op == 0x11)
    pc += 2;			/* skip brb */
  if (op == 0x31)
    pc += 3;			/* skip brw */
  if (op == 0xC2
      && (read_memory_unsigned_integer (pc + 2, 1)) == 0x5E)
    pc += 3;			/* skip subl2 */
  if (op == 0x9E
      && (read_memory_unsigned_integer (pc + 1, 1)) == 0xAE
      && (read_memory_unsigned_integer (pc + 3, 1)) == 0x5E)
    pc += 4;			/* skip movab */
  if (op == 0x9E
      && (read_memory_unsigned_integer (pc + 1, 1)) == 0xCE
      && (read_memory_unsigned_integer (pc + 4, 1)) == 0x5E)
    pc += 5;			/* skip movab */
  if (op == 0x9E
      && (read_memory_unsigned_integer (pc + 1, 1)) == 0xEE
      && (read_memory_unsigned_integer (pc + 6, 1)) == 0x5E)
    pc += 7;			/* skip movab */

  return pc;
}


/* Unwinding the stack is relatively easy since the VAX has a
   dedicated frame pointer, and frames are set up automatically as the
   result of a function call.  Most of the relevant information can be
   inferred from the documentation of the Procedure Call Instructions
   in the VAX MACRO and Instruction Set Reference Manual.  */

struct vax_frame_cache
{
  /* Base address.  */
  CORE_ADDR base;

  /* Table of saved registers.  */
  struct trad_frame_saved_reg *saved_regs;
};

struct vax_frame_cache *
vax_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct vax_frame_cache *cache;
  CORE_ADDR addr;
  ULONGEST mask;
  int regnum;

  if (*this_cache)
    return *this_cache;

  /* Allocate a new cache.  */
  cache = FRAME_OBSTACK_ZALLOC (struct vax_frame_cache);
  cache->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  /* The frame pointer is used as the base for the frame.  */
  cache->base = frame_unwind_register_unsigned (next_frame, VAX_FP_REGNUM);
  if (cache->base == 0)
    return cache;

  /* The register save mask and control bits determine the layout of
     the stack frame.  */
  mask = get_frame_memory_unsigned (next_frame, cache->base + 4, 4) >> 16;

  /* These are always saved.  */
  cache->saved_regs[VAX_PC_REGNUM].addr = cache->base + 16;
  cache->saved_regs[VAX_FP_REGNUM].addr = cache->base + 12;
  cache->saved_regs[VAX_AP_REGNUM].addr = cache->base + 8;
  cache->saved_regs[VAX_PS_REGNUM].addr = cache->base + 4;

  /* Scan the register save mask and record the location of the saved
     registers.  */
  addr = cache->base + 20;
  for (regnum = 0; regnum < VAX_AP_REGNUM; regnum++)
    {
      if (mask & (1 << regnum))
	{
	  cache->saved_regs[regnum].addr = addr;
	  addr += 4;
	}
    }

  /* The CALLS/CALLG flag determines whether this frame has a General
     Argument List or a Stack Argument List.  */
  if (mask & (1 << 13))
    {
      ULONGEST numarg;

      /* This is a procedure with Stack Argument List.  Adjust the
         stack address for the arguments that were pushed onto the
         stack.  The return instruction will automatically pop the
         arguments from the stack.  */
      numarg = get_frame_memory_unsigned (next_frame, addr, 1);
      addr += 4 + numarg * 4;
    }

  /* Bits 1:0 of the stack pointer were saved in the control bits.  */
  trad_frame_set_value (cache->saved_regs, VAX_SP_REGNUM, addr + (mask >> 14));

  return cache;
}

static void
vax_frame_this_id (struct frame_info *next_frame, void **this_cache,
		   struct frame_id *this_id)
{
  struct vax_frame_cache *cache = vax_frame_cache (next_frame, this_cache);

  /* This marks the outermost frame.  */
  if (cache->base == 0)
    return;

  (*this_id) = frame_id_build (cache->base,
			       frame_func_unwind (next_frame, NORMAL_FRAME));
}

static void
vax_frame_prev_register (struct frame_info *next_frame, void **this_cache,
			 int regnum, int *optimizedp,
			 enum lval_type *lvalp, CORE_ADDR *addrp,
			 int *realnump, gdb_byte *valuep)
{
  struct vax_frame_cache *cache = vax_frame_cache (next_frame, this_cache);

  trad_frame_get_prev_register (next_frame, cache->saved_regs, regnum,
				optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind vax_frame_unwind =
{
  NORMAL_FRAME,
  vax_frame_this_id,
  vax_frame_prev_register
};

static const struct frame_unwind *
vax_frame_sniffer (struct frame_info *next_frame)
{
  return &vax_frame_unwind;
}


static CORE_ADDR
vax_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct vax_frame_cache *cache = vax_frame_cache (next_frame, this_cache);

  return cache->base;
}

static CORE_ADDR
vax_frame_args_address (struct frame_info *next_frame, void **this_cache)
{
  return frame_unwind_register_unsigned (next_frame, VAX_AP_REGNUM);
}

static const struct frame_base vax_frame_base =
{
  &vax_frame_unwind,
  vax_frame_base_address,
  vax_frame_base_address,
  vax_frame_args_address
};

/* Return number of arguments for FRAME.  */

static int
vax_frame_num_args (struct frame_info *frame)
{
  CORE_ADDR args;

  /* Assume that the argument pointer for the outermost frame is
     hosed, as is the case on NetBSD/vax ELF.  */
  if (get_frame_base_address (frame) == 0)
    return 0;

  args = get_frame_register_unsigned (frame, VAX_AP_REGNUM);
  return get_frame_memory_unsigned (frame, args, 1);
}

static CORE_ADDR
vax_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, VAX_PC_REGNUM);
}


/* Initialize the current architecture based on INFO.  If possible, re-use an
   architecture from ARCHES, which is a list of architectures already created
   during this debugging session.

   Called e.g. at program startup, when reading a core file, and when reading
   a binary file.  */

static struct gdbarch *
vax_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;

  /* If there is already a candidate, use it.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  gdbarch = gdbarch_alloc (&info, NULL);

  set_gdbarch_float_format (gdbarch, floatformats_vax_f);
  set_gdbarch_double_format (gdbarch, floatformats_vax_d);
  set_gdbarch_long_double_format (gdbarch, floatformats_vax_d);
  set_gdbarch_long_double_bit (gdbarch, 64);

  /* Register info */
  set_gdbarch_num_regs (gdbarch, VAX_NUM_REGS);
  set_gdbarch_register_name (gdbarch, vax_register_name);
  set_gdbarch_register_type (gdbarch, vax_register_type);
  set_gdbarch_sp_regnum (gdbarch, VAX_SP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, VAX_PC_REGNUM);
  set_gdbarch_ps_regnum (gdbarch, VAX_PS_REGNUM);

  set_gdbarch_regset_from_core_section
    (gdbarch, vax_regset_from_core_section);

  /* Frame and stack info */
  set_gdbarch_skip_prologue (gdbarch, vax_skip_prologue);
  set_gdbarch_frame_num_args (gdbarch, vax_frame_num_args);
  set_gdbarch_frame_args_skip (gdbarch, 4);

  /* Stack grows downward.  */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  /* Return value info */
  set_gdbarch_return_value (gdbarch, vax_return_value);

  /* Call dummy code.  */
  set_gdbarch_push_dummy_call (gdbarch, vax_push_dummy_call);
  set_gdbarch_unwind_dummy_id (gdbarch, vax_unwind_dummy_id);

  /* Breakpoint info */
  set_gdbarch_breakpoint_from_pc (gdbarch, vax_breakpoint_from_pc);

  /* Misc info */
  set_gdbarch_deprecated_function_start_offset (gdbarch, 2);
  set_gdbarch_believe_pcc_promotion (gdbarch, 1);

  set_gdbarch_print_insn (gdbarch, print_insn_vax);

  set_gdbarch_unwind_pc (gdbarch, vax_unwind_pc);

  frame_base_set_default (gdbarch, &vax_frame_base);

  /* Hook in ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch);

  frame_unwind_append_sniffer (gdbarch, vax_frame_sniffer);

  return (gdbarch);
}

/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_vax_tdep (void);

void
_initialize_vax_tdep (void)
{
  gdbarch_register (bfd_arch_vax, vax_gdbarch_init, NULL);
}
