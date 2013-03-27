/* Target-dependent code for Renesas M32R, for GDB.

   Copyright (C) 1996, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2007
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
#include "frame-unwind.h"
#include "frame-base.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "gdb_string.h"
#include "value.h"
#include "inferior.h"
#include "symfile.h"
#include "objfiles.h"
#include "osabi.h"
#include "language.h"
#include "arch-utils.h"
#include "regcache.h"
#include "trad-frame.h"
#include "dis-asm.h"

#include "gdb_assert.h"

#include "m32r-tdep.h"

/* Local functions */

extern void _initialize_m32r_tdep (void);

static CORE_ADDR
m32r_frame_align (struct gdbarch *gdbarch, CORE_ADDR sp)
{
  /* Align to the size of an instruction (so that they can safely be
     pushed onto the stack.  */
  return sp & ~3;
}


/* Breakpoints
 
   The little endian mode of M32R is unique. In most of architectures,
   two 16-bit instructions, A and B, are placed as the following:
  
   Big endian:
   A0 A1 B0 B1
  
   Little endian:
   A1 A0 B1 B0
  
   In M32R, they are placed like this:
  
   Big endian:
   A0 A1 B0 B1
  
   Little endian:
   B1 B0 A1 A0
  
   This is because M32R always fetches instructions in 32-bit.
  
   The following functions take care of this behavior. */

static int
m32r_memory_insert_breakpoint (struct bp_target_info *bp_tgt)
{
  CORE_ADDR addr = bp_tgt->placed_address;
  int val;
  gdb_byte buf[4];
  gdb_byte *contents_cache = bp_tgt->shadow_contents;
  gdb_byte bp_entry[] = { 0x10, 0xf1 };	/* dpt */

  /* Save the memory contents.  */
  val = target_read_memory (addr & 0xfffffffc, contents_cache, 4);
  if (val != 0)
    return val;			/* return error */

  bp_tgt->placed_size = bp_tgt->shadow_len = 4;

  /* Determine appropriate breakpoint contents and size for this address.  */
  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
    {
      if ((addr & 3) == 0)
	{
	  buf[0] = bp_entry[0];
	  buf[1] = bp_entry[1];
	  buf[2] = contents_cache[2] & 0x7f;
	  buf[3] = contents_cache[3];
	}
      else
	{
	  buf[0] = contents_cache[0];
	  buf[1] = contents_cache[1];
	  buf[2] = bp_entry[0];
	  buf[3] = bp_entry[1];
	}
    }
  else				/* little-endian */
    {
      if ((addr & 3) == 0)
	{
	  buf[0] = contents_cache[0];
	  buf[1] = contents_cache[1] & 0x7f;
	  buf[2] = bp_entry[1];
	  buf[3] = bp_entry[0];
	}
      else
	{
	  buf[0] = bp_entry[1];
	  buf[1] = bp_entry[0];
	  buf[2] = contents_cache[2];
	  buf[3] = contents_cache[3];
	}
    }

  /* Write the breakpoint.  */
  val = target_write_memory (addr & 0xfffffffc, buf, 4);
  return val;
}

static int
m32r_memory_remove_breakpoint (struct bp_target_info *bp_tgt)
{
  CORE_ADDR addr = bp_tgt->placed_address;
  int val;
  gdb_byte buf[4];
  gdb_byte *contents_cache = bp_tgt->shadow_contents;

  buf[0] = contents_cache[0];
  buf[1] = contents_cache[1];
  buf[2] = contents_cache[2];
  buf[3] = contents_cache[3];

  /* Remove parallel bit.  */
  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
    {
      if ((buf[0] & 0x80) == 0 && (buf[2] & 0x80) != 0)
	buf[2] &= 0x7f;
    }
  else				/* little-endian */
    {
      if ((buf[3] & 0x80) == 0 && (buf[1] & 0x80) != 0)
	buf[1] &= 0x7f;
    }

  /* Write contents.  */
  val = target_write_memory (addr & 0xfffffffc, buf, 4);
  return val;
}

static const gdb_byte *
m32r_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  static gdb_byte be_bp_entry[] = { 0x10, 0xf1, 0x70, 0x00 };	/* dpt -> nop */
  static gdb_byte le_bp_entry[] = { 0x00, 0x70, 0xf1, 0x10 };	/* dpt -> nop */
  gdb_byte *bp;

  /* Determine appropriate breakpoint.  */
  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
    {
      if ((*pcptr & 3) == 0)
	{
	  bp = be_bp_entry;
	  *lenptr = 4;
	}
      else
	{
	  bp = be_bp_entry;
	  *lenptr = 2;
	}
    }
  else
    {
      if ((*pcptr & 3) == 0)
	{
	  bp = le_bp_entry;
	  *lenptr = 4;
	}
      else
	{
	  bp = le_bp_entry + 2;
	  *lenptr = 2;
	}
    }

  return bp;
}


char *m32r_register_names[] = {
  "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
  "r8", "r9", "r10", "r11", "r12", "fp", "lr", "sp",
  "psw", "cbr", "spi", "spu", "bpc", "pc", "accl", "acch",
  "evb"
};

static const char *
m32r_register_name (int reg_nr)
{
  if (reg_nr < 0)
    return NULL;
  if (reg_nr >= M32R_NUM_REGS)
    return NULL;
  return m32r_register_names[reg_nr];
}


/* Return the GDB type object for the "standard" data type
   of data in register N.  */

static struct type *
m32r_register_type (struct gdbarch *gdbarch, int reg_nr)
{
  if (reg_nr == M32R_PC_REGNUM)
    return builtin_type_void_func_ptr;
  else if (reg_nr == M32R_SP_REGNUM || reg_nr == M32R_FP_REGNUM)
    return builtin_type_void_data_ptr;
  else
    return builtin_type_int32;
}


/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format.  

   Things always get returned in RET1_REGNUM, RET2_REGNUM. */

static void
m32r_store_return_value (struct type *type, struct regcache *regcache,
			 const void *valbuf)
{
  CORE_ADDR regval;
  int len = TYPE_LENGTH (type);

  regval = extract_unsigned_integer (valbuf, len > 4 ? 4 : len);
  regcache_cooked_write_unsigned (regcache, RET1_REGNUM, regval);

  if (len > 4)
    {
      regval = extract_unsigned_integer ((gdb_byte *) valbuf + 4, len - 4);
      regcache_cooked_write_unsigned (regcache, RET1_REGNUM + 1, regval);
    }
}

/* This is required by skip_prologue. The results of decoding a prologue
   should be cached because this thrashing is getting nuts.  */

static int
decode_prologue (CORE_ADDR start_pc, CORE_ADDR scan_limit,
		 CORE_ADDR *pl_endptr, unsigned long *framelength)
{
  unsigned long framesize;
  int insn;
  int op1;
  CORE_ADDR after_prologue = 0;
  CORE_ADDR after_push = 0;
  CORE_ADDR after_stack_adjust = 0;
  CORE_ADDR current_pc;
  LONGEST return_value;

  framesize = 0;
  after_prologue = 0;

  for (current_pc = start_pc; current_pc < scan_limit; current_pc += 2)
    {
      /* Check if current pc's location is readable. */
      if (!safe_read_memory_integer (current_pc, 2, &return_value))
	return -1;

      insn = read_memory_unsigned_integer (current_pc, 2);

      if (insn == 0x0000)
	break;

      /* If this is a 32 bit instruction, we dont want to examine its
         immediate data as though it were an instruction */
      if (current_pc & 0x02)
	{
	  /* decode this instruction further */
	  insn &= 0x7fff;
	}
      else
	{
	  if (insn & 0x8000)
	    {
	      if (current_pc == scan_limit)
		scan_limit += 2;	/* extend the search */

	      current_pc += 2;	/* skip the immediate data */

	      /* Check if current pc's location is readable. */
	      if (!safe_read_memory_integer (current_pc, 2, &return_value))
		return -1;

	      if (insn == 0x8faf)	/* add3 sp, sp, xxxx */
		/* add 16 bit sign-extended offset */
		{
		  framesize +=
		    -((short) read_memory_unsigned_integer (current_pc, 2));
		}
	      else
		{
		  if (((insn >> 8) == 0xe4)	/* ld24 r4, xxxxxx; sub sp, r4 */
		      && safe_read_memory_integer (current_pc + 2, 2,
						   &return_value)
		      && read_memory_unsigned_integer (current_pc + 2,
						       2) == 0x0f24)
		    /* subtract 24 bit sign-extended negative-offset */
		    {
		      insn = read_memory_unsigned_integer (current_pc - 2, 4);
		      if (insn & 0x00800000)	/* sign extend */
			insn |= 0xff000000;	/* negative */
		      else
			insn &= 0x00ffffff;	/* positive */
		      framesize += insn;
		    }
		}
	      after_push = current_pc + 2;
	      continue;
	    }
	}
      op1 = insn & 0xf000;	/* isolate just the first nibble */

      if ((insn & 0xf0ff) == 0x207f)
	{			/* st reg, @-sp */
	  int regno;
	  framesize += 4;
	  regno = ((insn >> 8) & 0xf);
	  after_prologue = 0;
	  continue;
	}
      if ((insn >> 8) == 0x4f)	/* addi sp, xx */
	/* add 8 bit sign-extended offset */
	{
	  int stack_adjust = (gdb_byte) (insn & 0xff);

	  /* there are probably two of these stack adjustments:
	     1) A negative one in the prologue, and
	     2) A positive one in the epilogue.
	     We are only interested in the first one.  */

	  if (stack_adjust < 0)
	    {
	      framesize -= stack_adjust;
	      after_prologue = 0;
	      /* A frameless function may have no "mv fp, sp".
	         In that case, this is the end of the prologue.  */
	      after_stack_adjust = current_pc + 2;
	    }
	  continue;
	}
      if (insn == 0x1d8f)
	{			/* mv fp, sp */
	  after_prologue = current_pc + 2;
	  break;		/* end of stack adjustments */
	}

      /* Nop looks like a branch, continue explicitly */
      if (insn == 0x7000)
	{
	  after_prologue = current_pc + 2;
	  continue;		/* nop occurs between pushes */
	}
      /* End of prolog if any of these are trap instructions */
      if ((insn & 0xfff0) == 0x10f0)
	{
	  after_prologue = current_pc;
	  break;
	}
      /* End of prolog if any of these are branch instructions */
      if ((op1 == 0x7000) || (op1 == 0xb000) || (op1 == 0xf000))
	{
	  after_prologue = current_pc;
	  continue;
	}
      /* Some of the branch instructions are mixed with other types */
      if (op1 == 0x1000)
	{
	  int subop = insn & 0x0ff0;
	  if ((subop == 0x0ec0) || (subop == 0x0fc0))
	    {
	      after_prologue = current_pc;
	      continue;		/* jmp , jl */
	    }
	}
    }

  if (framelength)
    *framelength = framesize;

  if (current_pc >= scan_limit)
    {
      if (pl_endptr)
	{
	  if (after_stack_adjust != 0)
	    /* We did not find a "mv fp,sp", but we DID find
	       a stack_adjust.  Is it safe to use that as the
	       end of the prologue?  I just don't know. */
	    {
	      *pl_endptr = after_stack_adjust;
	    }
	  else if (after_push != 0)
	    /* We did not find a "mv fp,sp", but we DID find
	       a push.  Is it safe to use that as the
	       end of the prologue?  I just don't know. */
	    {
	      *pl_endptr = after_push;
	    }
	  else
	    /* We reached the end of the loop without finding the end
	       of the prologue.  No way to win -- we should report failure.  
	       The way we do that is to return the original start_pc.
	       GDB will set a breakpoint at the start of the function (etc.) */
	    *pl_endptr = start_pc;
	}
      return 0;
    }

  if (after_prologue == 0)
    after_prologue = current_pc;

  if (pl_endptr)
    *pl_endptr = after_prologue;

  return 0;
}				/*  decode_prologue */

/* Function: skip_prologue
   Find end of function prologue */

#define DEFAULT_SEARCH_LIMIT 128

CORE_ADDR
m32r_skip_prologue (CORE_ADDR pc)
{
  CORE_ADDR func_addr, func_end;
  struct symtab_and_line sal;
  LONGEST return_value;

  /* See what the symbol table says */

  if (find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    {
      sal = find_pc_line (func_addr, 0);

      if (sal.line != 0 && sal.end <= func_end)
	{
	  func_end = sal.end;
	}
      else
	/* Either there's no line info, or the line after the prologue is after
	   the end of the function.  In this case, there probably isn't a
	   prologue.  */
	{
	  func_end = min (func_end, func_addr + DEFAULT_SEARCH_LIMIT);
	}
    }
  else
    func_end = pc + DEFAULT_SEARCH_LIMIT;

  /* If pc's location is not readable, just quit. */
  if (!safe_read_memory_integer (pc, 4, &return_value))
    return pc;

  /* Find the end of prologue.  */
  if (decode_prologue (pc, func_end, &sal.end, NULL) < 0)
    return pc;

  return sal.end;
}

struct m32r_unwind_cache
{
  /* The previous frame's inner most stack address.  Used as this
     frame ID's stack_addr.  */
  CORE_ADDR prev_sp;
  /* The frame's base, optionally used by the high-level debug info.  */
  CORE_ADDR base;
  int size;
  /* How far the SP and r13 (FP) have been offset from the start of
     the stack frame (as defined by the previous frame's stack
     pointer).  */
  LONGEST sp_offset;
  LONGEST r13_offset;
  int uses_frame;
  /* Table indicating the location of each and every register.  */
  struct trad_frame_saved_reg *saved_regs;
};

/* Put here the code to store, into fi->saved_regs, the addresses of
   the saved registers of frame described by FRAME_INFO.  This
   includes special registers such as pc and fp saved in special ways
   in the stack frame.  sp is even more special: the address we return
   for it IS the sp for the next frame. */

static struct m32r_unwind_cache *
m32r_frame_unwind_cache (struct frame_info *next_frame,
			 void **this_prologue_cache)
{
  CORE_ADDR pc, scan_limit;
  ULONGEST prev_sp;
  ULONGEST this_base;
  unsigned long op, op2;
  int i;
  struct m32r_unwind_cache *info;


  if ((*this_prologue_cache))
    return (*this_prologue_cache);

  info = FRAME_OBSTACK_ZALLOC (struct m32r_unwind_cache);
  (*this_prologue_cache) = info;
  info->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  info->size = 0;
  info->sp_offset = 0;
  info->uses_frame = 0;

  scan_limit = frame_pc_unwind (next_frame);
  for (pc = frame_func_unwind (next_frame, NORMAL_FRAME);
       pc > 0 && pc < scan_limit; pc += 2)
    {
      if ((pc & 2) == 0)
	{
	  op = get_frame_memory_unsigned (next_frame, pc, 4);
	  if ((op & 0x80000000) == 0x80000000)
	    {
	      /* 32-bit instruction */
	      if ((op & 0xffff0000) == 0x8faf0000)
		{
		  /* add3 sp,sp,xxxx */
		  short n = op & 0xffff;
		  info->sp_offset += n;
		}
	      else if (((op >> 8) == 0xe4)
		       && get_frame_memory_unsigned (next_frame, pc + 2,
						     2) == 0x0f24)
		{
		  /* ld24 r4, xxxxxx; sub sp, r4 */
		  unsigned long n = op & 0xffffff;
		  info->sp_offset += n;
		  pc += 2;	/* skip sub instruction */
		}

	      if (pc == scan_limit)
		scan_limit += 2;	/* extend the search */
	      pc += 2;		/* skip the immediate data */
	      continue;
	    }
	}

      /* 16-bit instructions */
      op = get_frame_memory_unsigned (next_frame, pc, 2) & 0x7fff;
      if ((op & 0xf0ff) == 0x207f)
	{
	  /* st rn, @-sp */
	  int regno = ((op >> 8) & 0xf);
	  info->sp_offset -= 4;
	  info->saved_regs[regno].addr = info->sp_offset;
	}
      else if ((op & 0xff00) == 0x4f00)
	{
	  /* addi sp, xx */
	  int n = (gdb_byte) (op & 0xff);
	  info->sp_offset += n;
	}
      else if (op == 0x1d8f)
	{
	  /* mv fp, sp */
	  info->uses_frame = 1;
	  info->r13_offset = info->sp_offset;
	  break;		/* end of stack adjustments */
	}
      else if ((op & 0xfff0) == 0x10f0)
	{
	  /* end of prologue if this is a trap instruction */
	  break;		/* end of stack adjustments */
	}
    }

  info->size = -info->sp_offset;

  /* Compute the previous frame's stack pointer (which is also the
     frame's ID's stack address), and this frame's base pointer.  */
  if (info->uses_frame)
    {
      /* The SP was moved to the FP.  This indicates that a new frame
         was created.  Get THIS frame's FP value by unwinding it from
         the next frame.  */
      this_base = frame_unwind_register_unsigned (next_frame, M32R_FP_REGNUM);
      /* The FP points at the last saved register.  Adjust the FP back
         to before the first saved register giving the SP.  */
      prev_sp = this_base + info->size;
    }
  else
    {
      /* Assume that the FP is this frame's SP but with that pushed
         stack space added back.  */
      this_base = frame_unwind_register_unsigned (next_frame, M32R_SP_REGNUM);
      prev_sp = this_base + info->size;
    }

  /* Convert that SP/BASE into real addresses.  */
  info->prev_sp = prev_sp;
  info->base = this_base;

  /* Adjust all the saved registers so that they contain addresses and
     not offsets.  */
  for (i = 0; i < gdbarch_num_regs (current_gdbarch) - 1; i++)
    if (trad_frame_addr_p (info->saved_regs, i))
      info->saved_regs[i].addr = (info->prev_sp + info->saved_regs[i].addr);

  /* The call instruction moves the caller's PC in the callee's LR.
     Since this is an unwind, do the reverse.  Copy the location of LR
     into PC (the address / regnum) so that a request for PC will be
     converted into a request for the LR.  */
  info->saved_regs[M32R_PC_REGNUM] = info->saved_regs[LR_REGNUM];

  /* The previous frame's SP needed to be computed.  Save the computed
     value.  */
  trad_frame_set_value (info->saved_regs, M32R_SP_REGNUM, prev_sp);

  return info;
}

static CORE_ADDR
m32r_read_pc (struct regcache *regcache)
{
  ULONGEST pc;
  regcache_cooked_read_unsigned (regcache, M32R_PC_REGNUM, &pc);
  return pc;
}

static void
m32r_write_pc (struct regcache *regcache, CORE_ADDR val)
{
  regcache_cooked_write_unsigned (regcache, M32R_PC_REGNUM, val);
}

static CORE_ADDR
m32r_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, M32R_SP_REGNUM);
}


static CORE_ADDR
m32r_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		      struct regcache *regcache, CORE_ADDR bp_addr, int nargs,
		      struct value **args, CORE_ADDR sp, int struct_return,
		      CORE_ADDR struct_addr)
{
  int stack_offset, stack_alloc;
  int argreg = ARG1_REGNUM;
  int argnum;
  struct type *type;
  enum type_code typecode;
  CORE_ADDR regval;
  gdb_byte *val;
  gdb_byte valbuf[MAX_REGISTER_SIZE];
  int len;
  int odd_sized_struct;

  /* first force sp to a 4-byte alignment */
  sp = sp & ~3;

  /* Set the return address.  For the m32r, the return breakpoint is
     always at BP_ADDR.  */
  regcache_cooked_write_unsigned (regcache, LR_REGNUM, bp_addr);

  /* If STRUCT_RETURN is true, then the struct return address (in
     STRUCT_ADDR) will consume the first argument-passing register.
     Both adjust the register count and store that value.  */
  if (struct_return)
    {
      regcache_cooked_write_unsigned (regcache, argreg, struct_addr);
      argreg++;
    }

  /* Now make sure there's space on the stack */
  for (argnum = 0, stack_alloc = 0; argnum < nargs; argnum++)
    stack_alloc += ((TYPE_LENGTH (value_type (args[argnum])) + 3) & ~3);
  sp -= stack_alloc;		/* make room on stack for args */

  for (argnum = 0, stack_offset = 0; argnum < nargs; argnum++)
    {
      type = value_type (args[argnum]);
      typecode = TYPE_CODE (type);
      len = TYPE_LENGTH (type);

      memset (valbuf, 0, sizeof (valbuf));

      /* Passes structures that do not fit in 2 registers by reference.  */
      if (len > 8
	  && (typecode == TYPE_CODE_STRUCT || typecode == TYPE_CODE_UNION))
	{
	  store_unsigned_integer (valbuf, 4, VALUE_ADDRESS (args[argnum]));
	  typecode = TYPE_CODE_PTR;
	  len = 4;
	  val = valbuf;
	}
      else if (len < 4)
	{
	  /* value gets right-justified in the register or stack word */
	  memcpy (valbuf + (register_size (gdbarch, argreg) - len),
		  (gdb_byte *) value_contents (args[argnum]), len);
	  val = valbuf;
	}
      else
	val = (gdb_byte *) value_contents (args[argnum]);

      while (len > 0)
	{
	  if (argreg > ARGN_REGNUM)
	    {
	      /* must go on the stack */
	      write_memory (sp + stack_offset, val, 4);
	      stack_offset += 4;
	    }
	  else if (argreg <= ARGN_REGNUM)
	    {
	      /* there's room in a register */
	      regval =
		extract_unsigned_integer (val,
					  register_size (gdbarch, argreg));
	      regcache_cooked_write_unsigned (regcache, argreg++, regval);
	    }

	  /* Store the value 4 bytes at a time.  This means that things
	     larger than 4 bytes may go partly in registers and partly
	     on the stack.  */
	  len -= register_size (gdbarch, argreg);
	  val += register_size (gdbarch, argreg);
	}
    }

  /* Finally, update the SP register.  */
  regcache_cooked_write_unsigned (regcache, M32R_SP_REGNUM, sp);

  return sp;
}


/* Given a return value in `regbuf' with a type `valtype', 
   extract and copy its value into `valbuf'.  */

static void
m32r_extract_return_value (struct type *type, struct regcache *regcache,
			   void *dst)
{
  bfd_byte *valbuf = dst;
  int len = TYPE_LENGTH (type);
  ULONGEST tmp;

  /* By using store_unsigned_integer we avoid having to do
     anything special for small big-endian values.  */
  regcache_cooked_read_unsigned (regcache, RET1_REGNUM, &tmp);
  store_unsigned_integer (valbuf, (len > 4 ? len - 4 : len), tmp);

  /* Ignore return values more than 8 bytes in size because the m32r
     returns anything more than 8 bytes in the stack. */
  if (len > 4)
    {
      regcache_cooked_read_unsigned (regcache, RET1_REGNUM + 1, &tmp);
      store_unsigned_integer (valbuf + len - 4, 4, tmp);
    }
}

enum return_value_convention
m32r_return_value (struct gdbarch *gdbarch, struct type *valtype,
		   struct regcache *regcache, gdb_byte *readbuf,
		   const gdb_byte *writebuf)
{
  if (TYPE_LENGTH (valtype) > 8)
    return RETURN_VALUE_STRUCT_CONVENTION;
  else
    {
      if (readbuf != NULL)
	m32r_extract_return_value (valtype, regcache, readbuf);
      if (writebuf != NULL)
	m32r_store_return_value (valtype, regcache, writebuf);
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
}



static CORE_ADDR
m32r_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, M32R_PC_REGNUM);
}

/* Given a GDB frame, determine the address of the calling function's
   frame.  This will be used to create a new GDB frame struct.  */

static void
m32r_frame_this_id (struct frame_info *next_frame,
		    void **this_prologue_cache, struct frame_id *this_id)
{
  struct m32r_unwind_cache *info
    = m32r_frame_unwind_cache (next_frame, this_prologue_cache);
  CORE_ADDR base;
  CORE_ADDR func;
  struct minimal_symbol *msym_stack;
  struct frame_id id;

  /* The FUNC is easy.  */
  func = frame_func_unwind (next_frame, NORMAL_FRAME);

  /* Check if the stack is empty.  */
  msym_stack = lookup_minimal_symbol ("_stack", NULL, NULL);
  if (msym_stack && info->base == SYMBOL_VALUE_ADDRESS (msym_stack))
    return;

  /* Hopefully the prologue analysis either correctly determined the
     frame's base (which is the SP from the previous frame), or set
     that base to "NULL".  */
  base = info->prev_sp;
  if (base == 0)
    return;

  id = frame_id_build (base, func);
  (*this_id) = id;
}

static void
m32r_frame_prev_register (struct frame_info *next_frame,
			  void **this_prologue_cache,
			  int regnum, int *optimizedp,
			  enum lval_type *lvalp, CORE_ADDR *addrp,
			  int *realnump, gdb_byte *bufferp)
{
  struct m32r_unwind_cache *info
    = m32r_frame_unwind_cache (next_frame, this_prologue_cache);
  trad_frame_get_prev_register (next_frame, info->saved_regs, regnum,
				optimizedp, lvalp, addrp, realnump, bufferp);
}

static const struct frame_unwind m32r_frame_unwind = {
  NORMAL_FRAME,
  m32r_frame_this_id,
  m32r_frame_prev_register
};

static const struct frame_unwind *
m32r_frame_sniffer (struct frame_info *next_frame)
{
  return &m32r_frame_unwind;
}

static CORE_ADDR
m32r_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct m32r_unwind_cache *info
    = m32r_frame_unwind_cache (next_frame, this_cache);
  return info->base;
}

static const struct frame_base m32r_frame_base = {
  &m32r_frame_unwind,
  m32r_frame_base_address,
  m32r_frame_base_address,
  m32r_frame_base_address
};

/* Assuming NEXT_FRAME->prev is a dummy, return the frame ID of that
   dummy frame.  The frame ID's base needs to match the TOS value
   saved by save_dummy_frame_tos(), and the PC match the dummy frame's
   breakpoint.  */

static struct frame_id
m32r_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_id_build (m32r_unwind_sp (gdbarch, next_frame),
			 frame_pc_unwind (next_frame));
}


static gdbarch_init_ftype m32r_gdbarch_init;

static struct gdbarch *
m32r_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  struct gdbarch_tdep *tdep;

  /* If there is already a candidate, use it.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  /* Allocate space for the new architecture.  */
  tdep = XMALLOC (struct gdbarch_tdep);
  gdbarch = gdbarch_alloc (&info, tdep);

  set_gdbarch_read_pc (gdbarch, m32r_read_pc);
  set_gdbarch_write_pc (gdbarch, m32r_write_pc);
  set_gdbarch_unwind_sp (gdbarch, m32r_unwind_sp);

  set_gdbarch_num_regs (gdbarch, M32R_NUM_REGS);
  set_gdbarch_sp_regnum (gdbarch, M32R_SP_REGNUM);
  set_gdbarch_register_name (gdbarch, m32r_register_name);
  set_gdbarch_register_type (gdbarch, m32r_register_type);

  set_gdbarch_push_dummy_call (gdbarch, m32r_push_dummy_call);
  set_gdbarch_return_value (gdbarch, m32r_return_value);

  set_gdbarch_skip_prologue (gdbarch, m32r_skip_prologue);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_breakpoint_from_pc (gdbarch, m32r_breakpoint_from_pc);
  set_gdbarch_memory_insert_breakpoint (gdbarch,
					m32r_memory_insert_breakpoint);
  set_gdbarch_memory_remove_breakpoint (gdbarch,
					m32r_memory_remove_breakpoint);

  set_gdbarch_frame_align (gdbarch, m32r_frame_align);

  frame_base_set_default (gdbarch, &m32r_frame_base);

  /* Methods for saving / extracting a dummy frame's ID.  The ID's
     stack address must match the SP value returned by
     PUSH_DUMMY_CALL, and saved by generic_save_dummy_frame_tos.  */
  set_gdbarch_unwind_dummy_id (gdbarch, m32r_unwind_dummy_id);

  /* Return the unwound PC value.  */
  set_gdbarch_unwind_pc (gdbarch, m32r_unwind_pc);

  set_gdbarch_print_insn (gdbarch, print_insn_m32r);

  /* Hook in ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch);

  /* Hook in the default unwinders.  */
  frame_unwind_append_sniffer (gdbarch, m32r_frame_sniffer);

  /* Support simple overlay manager.  */
  set_gdbarch_overlay_update (gdbarch, simple_overlay_update);

  return gdbarch;
}

void
_initialize_m32r_tdep (void)
{
  register_gdbarch_init (bfd_arch_m32r, m32r_gdbarch_init);
}
