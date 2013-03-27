/* Target-dependent code for the Sanyo Xstormy16a (LC590000) processor.

   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2007
   Free Software Foundation, Inc.

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
#include "objfiles.h"

enum gdb_regnum
{
  /* Xstormy16 has 16 general purpose registers (R0-R15) plus PC.
     Functions will return their values in register R2-R7 as they fit.
     Otherwise a hidden pointer to an big enough area is given as argument
     to the function in r2. Further arguments are beginning in r3 then.
     R13 is used as frame pointer when GCC compiles w/o optimization
     R14 is used as "PSW", displaying the CPU status.
     R15 is used implicitely as stack pointer. */
  E_R0_REGNUM,
  E_R1_REGNUM,
  E_R2_REGNUM, E_1ST_ARG_REGNUM = E_R2_REGNUM, E_PTR_RET_REGNUM = E_R2_REGNUM,
  E_R3_REGNUM,
  E_R4_REGNUM,
  E_R5_REGNUM,
  E_R6_REGNUM,
  E_R7_REGNUM, E_LST_ARG_REGNUM = E_R7_REGNUM,
  E_R8_REGNUM,
  E_R9_REGNUM,
  E_R10_REGNUM,
  E_R11_REGNUM,
  E_R12_REGNUM,
  E_R13_REGNUM, E_FP_REGNUM = E_R13_REGNUM,
  E_R14_REGNUM, E_PSW_REGNUM = E_R14_REGNUM,
  E_R15_REGNUM, E_SP_REGNUM = E_R15_REGNUM,
  E_PC_REGNUM,
  E_NUM_REGS
};

/* Use an invalid address value as 'not available' marker.  */
enum { REG_UNAVAIL = (CORE_ADDR) -1 };

struct xstormy16_frame_cache
{
  /* Base address.  */
  CORE_ADDR base;
  CORE_ADDR pc;
  LONGEST framesize;
  int uses_fp;
  CORE_ADDR saved_regs[E_NUM_REGS];
  CORE_ADDR saved_sp;
};

/* Size of instructions, registers, etc. */
enum
{
  xstormy16_inst_size = 2,
  xstormy16_reg_size = 2,
  xstormy16_pc_size = 4
};

/* Size of return datatype which fits into the remaining return registers. */
#define E_MAX_RETTYPE_SIZE(regnum)	((E_LST_ARG_REGNUM - (regnum) + 1) \
					* xstormy16_reg_size)

/* Size of return datatype which fits into all return registers. */
enum
{
  E_MAX_RETTYPE_SIZE_IN_REGS = E_MAX_RETTYPE_SIZE (E_R2_REGNUM)
};

/* Function: xstormy16_register_name
   Returns the name of the standard Xstormy16 register N.  */

static const char *
xstormy16_register_name (int regnum)
{
  static char *register_names[] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "r8", "r9", "r10", "r11", "r12", "r13",
    "psw", "sp", "pc"
  };

  if (regnum < 0 || regnum >= E_NUM_REGS)
    internal_error (__FILE__, __LINE__,
		    _("xstormy16_register_name: illegal register number %d"),
		    regnum);
  else
    return register_names[regnum];

}

static struct type *
xstormy16_register_type (struct gdbarch *gdbarch, int regnum)
{
  if (regnum == E_PC_REGNUM)
    return builtin_type_uint32;
  else
    return builtin_type_uint16;
}

/* Function: xstormy16_type_is_scalar
   Makes the decision if a given type is a scalar types.  Scalar
   types are returned in the registers r2-r7 as they fit.  */

static int
xstormy16_type_is_scalar (struct type *t)
{
  return (TYPE_CODE(t) != TYPE_CODE_STRUCT
	  && TYPE_CODE(t) != TYPE_CODE_UNION
	  && TYPE_CODE(t) != TYPE_CODE_ARRAY);
}

/* Function: xstormy16_use_struct_convention 
   Returns non-zero if the given struct type will be returned using
   a special convention, rather than the normal function return method.
   7sed in the contexts of the "return" command, and of
   target function calls from the debugger.  */ 

static int
xstormy16_use_struct_convention (struct type *type)
{
  return !xstormy16_type_is_scalar (type)
	 || TYPE_LENGTH (type) > E_MAX_RETTYPE_SIZE_IN_REGS;
} 

/* Function: xstormy16_extract_return_value
   Find a function's return value in the appropriate registers (in
   regbuf), and copy it into valbuf.  */

static void
xstormy16_extract_return_value (struct type *type, struct regcache *regcache,
				void *valbuf)
{
  int len = TYPE_LENGTH (type);
  int i, regnum = E_1ST_ARG_REGNUM;

  for (i = 0; i < len; i += xstormy16_reg_size)
    regcache_raw_read (regcache, regnum++, (char *) valbuf + i);
}

/* Function: xstormy16_store_return_value
   Copy the function return value from VALBUF into the
   proper location for a function return. 
   Called only in the context of the "return" command.  */

static void 
xstormy16_store_return_value (struct type *type, struct regcache *regcache,
			      const void *valbuf)
{
  if (TYPE_LENGTH (type) == 1)
    {    
      /* Add leading zeros to the value. */
      char buf[xstormy16_reg_size];
      memset (buf, 0, xstormy16_reg_size);
      memcpy (buf, valbuf, 1);
      regcache_raw_write (regcache, E_1ST_ARG_REGNUM, buf);
    }
  else
    {
      int len = TYPE_LENGTH (type);
      int i, regnum = E_1ST_ARG_REGNUM;

      for (i = 0; i < len; i += xstormy16_reg_size)
        regcache_raw_write (regcache, regnum++, (char *) valbuf + i);
    }
}

static enum return_value_convention
xstormy16_return_value (struct gdbarch *gdbarch, struct type *type,
			struct regcache *regcache,
			gdb_byte *readbuf, const gdb_byte *writebuf)
{
  if (xstormy16_use_struct_convention (type))
    return RETURN_VALUE_STRUCT_CONVENTION;
  if (writebuf)
    xstormy16_store_return_value (type, regcache, writebuf);
  else if (readbuf)
    xstormy16_extract_return_value (type, regcache, readbuf);
  return RETURN_VALUE_REGISTER_CONVENTION;
}

static CORE_ADDR
xstormy16_frame_align (struct gdbarch *gdbarch, CORE_ADDR addr)
{
  if (addr & 1)
    ++addr;
  return addr;
}

/* Function: xstormy16_push_dummy_call
   Setup the function arguments for GDB to call a function in the inferior.
   Called only in the context of a target function call from the debugger.
   Returns the value of the SP register after the args are pushed.  */

static CORE_ADDR
xstormy16_push_dummy_call (struct gdbarch *gdbarch,
			   struct value *function,
			   struct regcache *regcache,
			   CORE_ADDR bp_addr, int nargs,
			   struct value **args,
			   CORE_ADDR sp, int struct_return,
			   CORE_ADDR struct_addr)
{
  CORE_ADDR stack_dest = sp;
  int argreg = E_1ST_ARG_REGNUM;
  int i, j;
  int typelen, slacklen;
  const gdb_byte *val;
  char buf[xstormy16_pc_size];

  /* If struct_return is true, then the struct return address will
     consume one argument-passing register.  */
  if (struct_return)
    {
      regcache_cooked_write_unsigned (regcache, E_PTR_RET_REGNUM, struct_addr);
      argreg++;
    }

  /* Arguments are passed in R2-R7 as they fit. If an argument doesn't
     fit in the remaining registers we're switching over to the stack.
     No argument is put on stack partially and as soon as we switched
     over to stack no further argument is put in a register even if it
     would fit in the remaining unused registers.  */
  for (i = 0; i < nargs && argreg <= E_LST_ARG_REGNUM; i++)
    {
      typelen = TYPE_LENGTH (value_enclosing_type (args[i]));
      if (typelen > E_MAX_RETTYPE_SIZE (argreg))
	break;

      /* Put argument into registers wordwise. */
      val = value_contents (args[i]);
      for (j = 0; j < typelen; j += xstormy16_reg_size)
	regcache_cooked_write_unsigned (regcache, argreg++,
			extract_unsigned_integer (val + j,
						  typelen - j ==
						  1 ? 1 :
						  xstormy16_reg_size));
    }

  /* Align SP */
  stack_dest = xstormy16_frame_align (gdbarch, stack_dest);

  /* Loop backwards through remaining arguments and push them on the stack,
     wordaligned.  */
  for (j = nargs - 1; j >= i; j--)
    {
      char *val;

      typelen = TYPE_LENGTH (value_enclosing_type (args[j]));
      slacklen = typelen & 1;
      val = alloca (typelen + slacklen);
      memcpy (val, value_contents (args[j]), typelen);
      memset (val + typelen, 0, slacklen);

      /* Now write this data to the stack. The stack grows upwards. */
      write_memory (stack_dest, val, typelen + slacklen);
      stack_dest += typelen + slacklen;
    }

  store_unsigned_integer (buf, xstormy16_pc_size, bp_addr);
  write_memory (stack_dest, buf, xstormy16_pc_size);
  stack_dest += xstormy16_pc_size;

  /* Update stack pointer.  */
  regcache_cooked_write_unsigned (regcache, E_SP_REGNUM, stack_dest);

  /* Return the new stack pointer minus the return address slot since
     that's what DWARF2/GCC uses as the frame's CFA.  */
  return stack_dest - xstormy16_pc_size;
}

/* Function: xstormy16_scan_prologue
   Decode the instructions within the given address range.
   Decide when we must have reached the end of the function prologue.
   If a frame_info pointer is provided, fill in its saved_regs etc.

   Returns the address of the first instruction after the prologue.  */

static CORE_ADDR
xstormy16_analyze_prologue (CORE_ADDR start_addr, CORE_ADDR end_addr,
			    struct xstormy16_frame_cache *cache,
			    struct frame_info *next_frame)
{
  CORE_ADDR next_addr;
  ULONGEST inst, inst2;
  LONGEST offset;
  int regnum;

  /* Initialize framesize with size of PC put on stack by CALLF inst. */
  cache->saved_regs[E_PC_REGNUM] = 0;
  cache->framesize = xstormy16_pc_size;

  if (start_addr >= end_addr)
    return end_addr;

  for (next_addr = start_addr;
       next_addr < end_addr; next_addr += xstormy16_inst_size)
    {
      inst = read_memory_unsigned_integer (next_addr, xstormy16_inst_size);
      inst2 = read_memory_unsigned_integer (next_addr + xstormy16_inst_size,
					    xstormy16_inst_size);

      if (inst >= 0x0082 && inst <= 0x008d)	/* push r2 .. push r13 */
	{
	  regnum = inst & 0x000f;
	  cache->saved_regs[regnum] = cache->framesize;
	  cache->framesize += xstormy16_reg_size;
	}

      /* optional stack allocation for args and local vars <= 4 byte */
      else if (inst == 0x301f || inst == 0x303f)	/* inc r15, #0x1/#0x3 */
	{
	  cache->framesize += ((inst & 0x0030) >> 4) + 1;
	}

      /* optional stack allocation for args and local vars > 4 && < 16 byte */
      else if ((inst & 0xff0f) == 0x510f)	/* 51Hf   add r15, #0xH */
	{
	  cache->framesize += (inst & 0x00f0) >> 4;
	}

      /* optional stack allocation for args and local vars >= 16 byte */
      else if (inst == 0x314f && inst2 >= 0x0010)	/* 314f HHHH  add r15, #0xH */
	{
	  cache->framesize += inst2;
	  next_addr += xstormy16_inst_size;
	}

      else if (inst == 0x46fd)	/* mov r13, r15 */
	{
	  cache->uses_fp = 1;
	}

      /* optional copying of args in r2-r7 to r10-r13 */
      /* Probably only in optimized case but legal action for prologue */
      else if ((inst & 0xff00) == 0x4600	/* 46SD   mov rD, rS */
	       && (inst & 0x00f0) >= 0x0020 && (inst & 0x00f0) <= 0x0070
	       && (inst & 0x000f) >= 0x00a0 && (inst & 0x000f) <= 0x000d)
	;

      /* optional copying of args in r2-r7 to stack */
      /* 72DS HHHH   mov.b (rD, 0xHHHH), r(S-8) (bit3 always 1, bit2-0 = reg) */
      /* 73DS HHHH   mov.w (rD, 0xHHHH), r(S-8) */
      else if ((inst & 0xfed8) == 0x72d8 && (inst & 0x0007) >= 2)
	{
	  regnum = inst & 0x0007;
	  /* Only 12 of 16 bits of the argument are used for the
	     signed offset. */
	  offset = (LONGEST) (inst2 & 0x0fff);
	  if (offset & 0x0800)
	    offset -= 0x1000;

	  cache->saved_regs[regnum] = cache->framesize + offset;
	  next_addr += xstormy16_inst_size;
	}

      else			/* Not a prologue instruction. */
	break;
    }

  return next_addr;
}

/* Function: xstormy16_skip_prologue
   If the input address is in a function prologue, 
   returns the address of the end of the prologue;
   else returns the input address.

   Note: the input address is likely to be the function start, 
   since this function is mainly used for advancing a breakpoint
   to the first line, or stepping to the first line when we have
   stepped into a function call.  */

static CORE_ADDR
xstormy16_skip_prologue (CORE_ADDR pc)
{
  CORE_ADDR func_addr = 0, func_end = 0;
  char *func_name;

  if (find_pc_partial_function (pc, &func_name, &func_addr, &func_end))
    {
      struct symtab_and_line sal;
      struct symbol *sym;
      struct xstormy16_frame_cache cache;
      CORE_ADDR plg_end;

      memset (&cache, 0, sizeof cache);

      /* Don't trust line number debug info in frameless functions. */
      plg_end = xstormy16_analyze_prologue (func_addr, func_end, &cache, NULL);
      if (!cache.uses_fp)
        return plg_end;

      /* Found a function.  */
      sym = lookup_symbol (func_name, NULL, VAR_DOMAIN, NULL, NULL);
      /* Don't use line number debug info for assembly source files. */
      if (sym && SYMBOL_LANGUAGE (sym) != language_asm)
	{
	  sal = find_pc_line (func_addr, 0);
	  if (sal.end && sal.end < func_end)
	    {
	      /* Found a line number, use it as end of prologue.  */
	      return sal.end;
	    }
	}
      /* No useable line symbol.  Use result of prologue parsing method. */
      return plg_end;
    }

  /* No function symbol -- just return the PC. */

  return (CORE_ADDR) pc;
}

/* The epilogue is defined here as the area at the end of a function,
   either on the `ret' instruction itself or after an instruction which
   destroys the function's stack frame. */
static int
xstormy16_in_function_epilogue_p (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  CORE_ADDR func_addr = 0, func_end = 0;

  if (find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    {
      ULONGEST inst, inst2;
      CORE_ADDR addr = func_end - xstormy16_inst_size;

      /* The Xstormy16 epilogue is max. 14 bytes long. */
      if (pc < func_end - 7 * xstormy16_inst_size)
	return 0;

      /* Check if we're on a `ret' instruction.  Otherwise it's
         too dangerous to proceed. */
      inst = read_memory_unsigned_integer (addr, xstormy16_inst_size);
      if (inst != 0x0003)
	return 0;

      while ((addr -= xstormy16_inst_size) >= func_addr)
	{
	  inst = read_memory_unsigned_integer (addr, xstormy16_inst_size);
	  if (inst >= 0x009a && inst <= 0x009d)	/* pop r10...r13 */
	    continue;
	  if (inst == 0x305f || inst == 0x307f)	/* dec r15, #0x1/#0x3 */
	    break;
	  inst2 = read_memory_unsigned_integer (addr - xstormy16_inst_size,
						xstormy16_inst_size);
	  if (inst2 == 0x314f && inst >= 0x8000)	/* add r15, neg. value */
	    {
	      addr -= xstormy16_inst_size;
	      break;
	    }
	  return 0;
	}
      if (pc > addr)
	return 1;
    }
  return 0;
}

const static unsigned char *
xstormy16_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  static unsigned char breakpoint[] = { 0x06, 0x0 };
  *lenptr = sizeof (breakpoint);
  return breakpoint;
}

/* Given a pointer to a jump table entry, return the address
   of the function it jumps to.  Return 0 if not found. */
static CORE_ADDR
xstormy16_resolve_jmp_table_entry (CORE_ADDR faddr)
{
  struct obj_section *faddr_sect = find_pc_section (faddr);

  if (faddr_sect)
    {
      LONGEST inst, inst2, addr;
      char buf[2 * xstormy16_inst_size];

      /* Return faddr if it's not pointing into the jump table. */
      if (strcmp (faddr_sect->the_bfd_section->name, ".plt"))
	return faddr;

      if (!target_read_memory (faddr, buf, sizeof buf))
	{
	  inst = extract_unsigned_integer (buf, xstormy16_inst_size);
	  inst2 = extract_unsigned_integer (buf + xstormy16_inst_size,
					    xstormy16_inst_size);
	  addr = inst2 << 8 | (inst & 0xff);
	  return addr;
	}
    }
  return 0;
}

/* Given a function's address, attempt to find (and return) the
   address of the corresponding jump table entry.  Return 0 if
   not found. */
static CORE_ADDR
xstormy16_find_jmp_table_entry (CORE_ADDR faddr)
{
  struct obj_section *faddr_sect = find_pc_section (faddr);

  if (faddr_sect)
    {
      struct obj_section *osect;

      /* Return faddr if it's already a pointer to a jump table entry. */
      if (!strcmp (faddr_sect->the_bfd_section->name, ".plt"))
	return faddr;

      ALL_OBJFILE_OSECTIONS (faddr_sect->objfile, osect)
      {
	if (!strcmp (osect->the_bfd_section->name, ".plt"))
	  break;
      }

      if (osect < faddr_sect->objfile->sections_end)
	{
	  CORE_ADDR addr;
	  for (addr = osect->addr;
	       addr < osect->endaddr; addr += 2 * xstormy16_inst_size)
	    {
	      LONGEST inst, inst2, faddr2;
	      char buf[2 * xstormy16_inst_size];

	      if (target_read_memory (addr, buf, sizeof buf))
		return 0;
	      inst = extract_unsigned_integer (buf, xstormy16_inst_size);
	      inst2 = extract_unsigned_integer (buf + xstormy16_inst_size,
						xstormy16_inst_size);
	      faddr2 = inst2 << 8 | (inst & 0xff);
	      if (faddr == faddr2)
		return addr;
	    }
	}
    }
  return 0;
}

static CORE_ADDR
xstormy16_skip_trampoline_code (struct frame_info *frame, CORE_ADDR pc)
{
  CORE_ADDR tmp = xstormy16_resolve_jmp_table_entry (pc);

  if (tmp && tmp != pc)
    return tmp;
  return 0;
}

/* Function pointers are 16 bit.  The address space is 24 bit, using
   32 bit addresses.  Pointers to functions on the XStormy16 are implemented
   by using 16 bit pointers, which are either direct pointers in case the
   function begins below 0x10000, or indirect pointers into a jump table.
   The next two functions convert 16 bit pointers into 24 (32) bit addresses
   and vice versa.  */

static CORE_ADDR
xstormy16_pointer_to_address (struct type *type, const gdb_byte *buf)
{
  enum type_code target = TYPE_CODE (TYPE_TARGET_TYPE (type));
  CORE_ADDR addr = extract_unsigned_integer (buf, TYPE_LENGTH (type));

  if (target == TYPE_CODE_FUNC || target == TYPE_CODE_METHOD)
    {
      CORE_ADDR addr2 = xstormy16_resolve_jmp_table_entry (addr);
      if (addr2)
	addr = addr2;
    }

  return addr;
}

static void
xstormy16_address_to_pointer (struct type *type, gdb_byte *buf, CORE_ADDR addr)
{
  enum type_code target = TYPE_CODE (TYPE_TARGET_TYPE (type));

  if (target == TYPE_CODE_FUNC || target == TYPE_CODE_METHOD)
    {
      CORE_ADDR addr2 = xstormy16_find_jmp_table_entry (addr);
      if (addr2)
	addr = addr2;
    }
  store_unsigned_integer (buf, TYPE_LENGTH (type), addr);
}

static struct xstormy16_frame_cache *
xstormy16_alloc_frame_cache (void)
{
  struct xstormy16_frame_cache *cache;
  int i;

  cache = FRAME_OBSTACK_ZALLOC (struct xstormy16_frame_cache);

  cache->base = 0;
  cache->saved_sp = 0;
  cache->pc = 0;
  cache->uses_fp = 0;
  cache->framesize = 0;
  for (i = 0; i < E_NUM_REGS; ++i)
    cache->saved_regs[i] = REG_UNAVAIL;

  return cache;
}

static struct xstormy16_frame_cache *
xstormy16_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct xstormy16_frame_cache *cache;
  CORE_ADDR current_pc;
  int i;

  if (*this_cache)
    return *this_cache;

  cache = xstormy16_alloc_frame_cache ();
  *this_cache = cache;

  cache->base = frame_unwind_register_unsigned (next_frame, E_FP_REGNUM);
  if (cache->base == 0)
    return cache;

  cache->pc = frame_func_unwind (next_frame, NORMAL_FRAME);
  current_pc = frame_pc_unwind (next_frame);
  if (cache->pc)
    xstormy16_analyze_prologue (cache->pc, current_pc, cache, next_frame);

  if (!cache->uses_fp)
    cache->base = frame_unwind_register_unsigned (next_frame, E_SP_REGNUM);

  cache->saved_sp = cache->base - cache->framesize;

  for (i = 0; i < E_NUM_REGS; ++i)
    if (cache->saved_regs[i] != REG_UNAVAIL)
      cache->saved_regs[i] += cache->saved_sp;

  return cache;
}

static void
xstormy16_frame_prev_register (struct frame_info *next_frame, 
			       void **this_cache,
			       int regnum, int *optimizedp,
			       enum lval_type *lvalp, CORE_ADDR *addrp,
			       int *realnump, gdb_byte *valuep)
{
  struct xstormy16_frame_cache *cache = xstormy16_frame_cache (next_frame,
                                                               this_cache);
  gdb_assert (regnum >= 0);

  if (regnum == E_SP_REGNUM && cache->saved_sp)
    {
      *optimizedp = 0;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      if (valuep)
        {
          /* Store the value.  */
          store_unsigned_integer (valuep, xstormy16_reg_size, cache->saved_sp);
        }
      return;
    }

  if (regnum < E_NUM_REGS && cache->saved_regs[regnum] != REG_UNAVAIL)
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
xstormy16_frame_this_id (struct frame_info *next_frame, void **this_cache,
			 struct frame_id *this_id)
{
  struct xstormy16_frame_cache *cache = xstormy16_frame_cache (next_frame,
							       this_cache);

  /* This marks the outermost frame.  */
  if (cache->base == 0)
    return;

  *this_id = frame_id_build (cache->saved_sp, cache->pc);
}

static CORE_ADDR
xstormy16_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct xstormy16_frame_cache *cache = xstormy16_frame_cache (next_frame,
							       this_cache);
  return cache->base;
}

static const struct frame_unwind xstormy16_frame_unwind = {
  NORMAL_FRAME,
  xstormy16_frame_this_id,
  xstormy16_frame_prev_register
};

static const struct frame_base xstormy16_frame_base = {
  &xstormy16_frame_unwind,
  xstormy16_frame_base_address,
  xstormy16_frame_base_address,
  xstormy16_frame_base_address
};

static const struct frame_unwind *
xstormy16_frame_sniffer (struct frame_info *next_frame)
{
  return &xstormy16_frame_unwind;
}

static CORE_ADDR
xstormy16_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, E_SP_REGNUM);
}

static CORE_ADDR
xstormy16_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, E_PC_REGNUM);
}

static struct frame_id
xstormy16_unwind_dummy_id (struct gdbarch *gdbarch,
			   struct frame_info *next_frame)
{
  return frame_id_build (xstormy16_unwind_sp (gdbarch, next_frame),
			 frame_pc_unwind (next_frame));
}


/* Function: xstormy16_gdbarch_init
   Initializer function for the xstormy16 gdbarch vector.
   Called by gdbarch.  Sets up the gdbarch vector(s) for this target. */

static struct gdbarch *
xstormy16_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;

  /* find a candidate among the list of pre-declared architectures. */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return (arches->gdbarch);

  gdbarch = gdbarch_alloc (&info, NULL);

  /*
   * Basic register fields and methods, datatype sizes and stuff.
   */

  set_gdbarch_num_regs (gdbarch, E_NUM_REGS);
  set_gdbarch_num_pseudo_regs (gdbarch, 0);
  set_gdbarch_sp_regnum (gdbarch, E_SP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, E_PC_REGNUM);
  set_gdbarch_register_name (gdbarch, xstormy16_register_name);
  set_gdbarch_register_type (gdbarch, xstormy16_register_type);

  set_gdbarch_char_signed (gdbarch, 0);
  set_gdbarch_short_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_int_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_long_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_long_bit (gdbarch, 8 * TARGET_CHAR_BIT);

  set_gdbarch_float_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_long_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);

  set_gdbarch_ptr_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_addr_bit (gdbarch, 4 * TARGET_CHAR_BIT);

  set_gdbarch_address_to_pointer (gdbarch, xstormy16_address_to_pointer);
  set_gdbarch_pointer_to_address (gdbarch, xstormy16_pointer_to_address);

  /* Stack grows up. */
  set_gdbarch_inner_than (gdbarch, core_addr_greaterthan);

  /*
   * Frame Info
   */
  set_gdbarch_unwind_sp (gdbarch, xstormy16_unwind_sp);
  set_gdbarch_unwind_pc (gdbarch, xstormy16_unwind_pc);
  set_gdbarch_unwind_dummy_id (gdbarch, xstormy16_unwind_dummy_id);
  set_gdbarch_frame_align (gdbarch, xstormy16_frame_align);
  frame_base_set_default (gdbarch, &xstormy16_frame_base);

  set_gdbarch_skip_prologue (gdbarch, xstormy16_skip_prologue);
  set_gdbarch_in_function_epilogue_p (gdbarch,
				      xstormy16_in_function_epilogue_p);

  /* These values and methods are used when gdb calls a target function.  */
  set_gdbarch_push_dummy_call (gdbarch, xstormy16_push_dummy_call);
  set_gdbarch_breakpoint_from_pc (gdbarch, xstormy16_breakpoint_from_pc);
  set_gdbarch_return_value (gdbarch, xstormy16_return_value);

  set_gdbarch_skip_trampoline_code (gdbarch, xstormy16_skip_trampoline_code);

  set_gdbarch_print_insn (gdbarch, print_insn_xstormy16);

  gdbarch_init_osabi (info, gdbarch);

  frame_unwind_append_sniffer (gdbarch, dwarf2_frame_sniffer);
  frame_unwind_append_sniffer (gdbarch, xstormy16_frame_sniffer);

  return gdbarch;
}

/* Function: _initialize_xstormy16_tdep
   Initializer function for the Sanyo Xstormy16a module.
   Called by gdb at start-up. */

extern initialize_file_ftype _initialize_xstormy16_tdep; /* -Wmissing-prototypes */

void
_initialize_xstormy16_tdep (void)
{
  register_gdbarch_init (bfd_arch_xstormy16, xstormy16_gdbarch_init);
}
