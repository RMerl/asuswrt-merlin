/* Target-dependent code for the IQ2000 architecture, for GDB, the GNU
   Debugger.

   Copyright (C) 2000, 2004, 2005, 2007 Free Software Foundation, Inc.

   Contributed by Red Hat.

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
#include "gdbtypes.h"
#include "value.h"
#include "dis-asm.h"
#include "gdb_string.h"
#include "arch-utils.h"
#include "regcache.h"
#include "osabi.h"
#include "gdbcore.h"

enum gdb_regnum
{
  E_R0_REGNUM,  E_R1_REGNUM,  E_R2_REGNUM,  E_R3_REGNUM, 
  E_R4_REGNUM,  E_R5_REGNUM,  E_R6_REGNUM,  E_R7_REGNUM, 
  E_R8_REGNUM,  E_R9_REGNUM,  E_R10_REGNUM, E_R11_REGNUM, 
  E_R12_REGNUM, E_R13_REGNUM, E_R14_REGNUM, E_R15_REGNUM, 
  E_R16_REGNUM, E_R17_REGNUM, E_R18_REGNUM, E_R19_REGNUM, 
  E_R20_REGNUM, E_R21_REGNUM, E_R22_REGNUM, E_R23_REGNUM, 
  E_R24_REGNUM, E_R25_REGNUM, E_R26_REGNUM, E_R27_REGNUM, 
  E_R28_REGNUM, E_R29_REGNUM, E_R30_REGNUM, E_R31_REGNUM, 
  E_PC_REGNUM, 
  E_LR_REGNUM        = E_R31_REGNUM, /* Link register.  */
  E_SP_REGNUM        = E_R29_REGNUM, /* Stack pointer.  */
  E_FP_REGNUM        = E_R27_REGNUM, /* Frame pointer.  */
  E_FN_RETURN_REGNUM = E_R2_REGNUM,  /* Function return value register.  */
  E_1ST_ARGREG       = E_R4_REGNUM,  /* 1st  function arg register.  */
  E_LAST_ARGREG      = E_R11_REGNUM, /* Last function arg register.  */
  E_NUM_REGS         = E_PC_REGNUM + 1
};

/* Use an invalid address value as 'not available' marker.  */
enum { REG_UNAVAIL = (CORE_ADDR) -1 };

struct iq2000_frame_cache
{
  /* Base address.  */
  CORE_ADDR  base;
  CORE_ADDR  pc;
  LONGEST    framesize;
  int        using_fp;
  CORE_ADDR  saved_sp;
  CORE_ADDR  saved_regs [E_NUM_REGS];
};

/* Harvard methods: */

static CORE_ADDR
insn_ptr_from_addr (CORE_ADDR addr)	/* CORE_ADDR to target pointer.  */
{
  return addr & 0x7fffffffL;
}

static CORE_ADDR
insn_addr_from_ptr (CORE_ADDR ptr)	/* target_pointer to CORE_ADDR.  */
{
  return (ptr & 0x7fffffffL) | 0x80000000L;
}

/* Function: pointer_to_address
   Convert a target pointer to an address in host (CORE_ADDR) format. */

static CORE_ADDR
iq2000_pointer_to_address (struct type * type, const void * buf)
{
  enum type_code target = TYPE_CODE (TYPE_TARGET_TYPE (type));
  CORE_ADDR addr = extract_unsigned_integer (buf, TYPE_LENGTH (type));

  if (target == TYPE_CODE_FUNC
      || target == TYPE_CODE_METHOD
      || (TYPE_FLAGS (TYPE_TARGET_TYPE (type)) & TYPE_FLAG_CODE_SPACE) != 0)
    addr = insn_addr_from_ptr (addr);

  return addr;
}

/* Function: address_to_pointer
   Convert a host-format address (CORE_ADDR) into a target pointer.  */

static void
iq2000_address_to_pointer (struct type *type, void *buf, CORE_ADDR addr)
{
  enum type_code target = TYPE_CODE (TYPE_TARGET_TYPE (type));

  if (target == TYPE_CODE_FUNC || target == TYPE_CODE_METHOD)
    addr = insn_ptr_from_addr (addr);
  store_unsigned_integer (buf, TYPE_LENGTH (type), addr);
}

/* Real register methods: */

/* Function: register_name
   Returns the name of the iq2000 register number N.  */

static const char *
iq2000_register_name (int regnum)
{
  static const char * names[E_NUM_REGS] =
    {
      "r0",  "r1",  "r2",  "r3",  "r4",
      "r5",  "r6",  "r7",  "r8",  "r9",
      "r10", "r11", "r12", "r13", "r14",
      "r15", "r16", "r17", "r18", "r19",
      "r20", "r21", "r22", "r23", "r24",
      "r25", "r26", "r27", "r28", "r29",
      "r30", "r31",
      "pc"
    };
  if (regnum < 0 || regnum >= E_NUM_REGS)
    return NULL;
  return names[regnum];
}

/* Prologue analysis methods:  */

/* ADDIU insn (001001 rs(5) rt(5) imm(16)).  */
#define INSN_IS_ADDIU(X)	(((X) & 0xfc000000) == 0x24000000) 
#define ADDIU_REG_SRC(X)	(((X) & 0x03e00000) >> 21)
#define ADDIU_REG_TGT(X)	(((X) & 0x001f0000) >> 16)
#define ADDIU_IMMEDIATE(X)	((signed short) ((X) & 0x0000ffff))

/* "MOVE" (OR) insn (000000 rs(5) rt(5) rd(5) 00000 100101).  */
#define INSN_IS_MOVE(X)		(((X) & 0xffe007ff) == 0x00000025)
#define MOVE_REG_SRC(X)		(((X) & 0x001f0000) >> 16)
#define MOVE_REG_TGT(X)		(((X) & 0x0000f800) >> 11)

/* STORE WORD insn (101011 rs(5) rt(5) offset(16)).  */
#define INSN_IS_STORE_WORD(X)	(((X) & 0xfc000000) == 0xac000000)
#define SW_REG_INDEX(X)		(((X) & 0x03e00000) >> 21)
#define SW_REG_SRC(X)		(((X) & 0x001f0000) >> 16)
#define SW_OFFSET(X)		((signed short) ((X) & 0x0000ffff))

/* Function: find_last_line_symbol

   Given an address range, first find a line symbol corresponding to
   the starting address.  Then find the last line symbol within the 
   range that has a line number less than or equal to the first line.

   For optimized code with code motion, this finds the last address
   for the lowest-numbered line within the address range.  */

static struct symtab_and_line
find_last_line_symbol (CORE_ADDR start, CORE_ADDR end, int notcurrent)
{
  struct symtab_and_line sal = find_pc_line (start, notcurrent);
  struct symtab_and_line best_sal = sal;

  if (sal.pc == 0 || sal.line == 0 || sal.end == 0)
    return sal;

  do
    {
      if (sal.line && sal.line <= best_sal.line)
	best_sal = sal;
      sal = find_pc_line (sal.end, notcurrent);
    }
  while (sal.pc && sal.pc < end);

  return best_sal;
}

/* Function: scan_prologue
   Decode the instructions within the given address range.
   Decide when we must have reached the end of the function prologue.
   If a frame_info pointer is provided, fill in its prologue information.

   Returns the address of the first instruction after the prologue.  */

static CORE_ADDR
iq2000_scan_prologue (CORE_ADDR scan_start,
		      CORE_ADDR scan_end,
		      struct frame_info *fi,
		      struct iq2000_frame_cache *cache)
{
  struct symtab_and_line sal;
  CORE_ADDR pc;
  CORE_ADDR loop_end;
  int found_store_lr = 0;
  int found_decr_sp = 0;
  int srcreg;
  int tgtreg;
  signed short offset;

  if (scan_end == (CORE_ADDR) 0)
    {
      loop_end = scan_start + 100;
      sal.end = sal.pc = 0;
    }
  else
    {
      loop_end = scan_end;
      if (fi)
	sal = find_last_line_symbol (scan_start, scan_end, 0);
    }

  /* Saved registers:
     We first have to save the saved register's offset, and 
     only later do we compute its actual address.  Since the
     offset can be zero, we must first initialize all the 
     saved regs to minus one (so we can later distinguish 
     between one that's not saved, and one that's saved at zero). */
  for (srcreg = 0; srcreg < E_NUM_REGS; srcreg ++)
    cache->saved_regs[srcreg] = -1;
  cache->using_fp = 0;
  cache->framesize = 0;

  for (pc = scan_start; pc < loop_end; pc += 4)
    {
      LONGEST insn = read_memory_unsigned_integer (pc, 4);
      /* Skip any instructions writing to (sp) or decrementing the
         SP. */
      if ((insn & 0xffe00000) == 0xac200000)
	{
	  /* sw using SP/%1 as base.  */
	  /* LEGACY -- from assembly-only port.  */
	  tgtreg = ((insn >> 16) & 0x1f);
	  if (tgtreg >= 0 && tgtreg < E_NUM_REGS)
	    cache->saved_regs[tgtreg] = -((signed short) (insn & 0xffff));

	  if (tgtreg == E_LR_REGNUM)
	    found_store_lr = 1;
	  continue;
	}

      if ((insn & 0xffff8000) == 0x20218000)
	{
	  /* addi %1, %1, -N == addi %sp, %sp, -N */
	  /* LEGACY -- from assembly-only port */
	  found_decr_sp = 1;
	  cache->framesize = -((signed short) (insn & 0xffff));
	  continue;
	}

      if (INSN_IS_ADDIU (insn))
	{
	  srcreg = ADDIU_REG_SRC (insn);
	  tgtreg = ADDIU_REG_TGT (insn);
	  offset = ADDIU_IMMEDIATE (insn);
	  if (srcreg == E_SP_REGNUM && tgtreg == E_SP_REGNUM)
	    cache->framesize = -offset;
	  continue;
	}

      if (INSN_IS_STORE_WORD (insn))
	{
	  srcreg = SW_REG_SRC (insn);
	  tgtreg = SW_REG_INDEX (insn);
	  offset = SW_OFFSET (insn);

	  if (tgtreg == E_SP_REGNUM || tgtreg == E_FP_REGNUM)
	    {
	      /* "push" to stack (via SP or FP reg) */
	      if (cache->saved_regs[srcreg] == -1) /* Don't save twice.  */
		cache->saved_regs[srcreg] = offset;
	      continue;
	    }
	}

      if (INSN_IS_MOVE (insn))
	{
	  srcreg = MOVE_REG_SRC (insn);
	  tgtreg = MOVE_REG_TGT (insn);

	  if (srcreg == E_SP_REGNUM && tgtreg == E_FP_REGNUM)
	    {
	      /* Copy sp to fp.  */
	      cache->using_fp = 1;
	      continue;
	    }
	}

      /* Unknown instruction encountered in frame.  Bail out?
         1) If we have a subsequent line symbol, we can keep going.
         2) If not, we need to bail out and quit scanning instructions.  */

      if (fi && sal.end && (pc < sal.end)) /* Keep scanning.  */
	continue;
      else /* bail */
	break;
    }

  return pc;
}

static void
iq2000_init_frame_cache (struct iq2000_frame_cache *cache)
{
  int i;

  cache->base = 0;
  cache->framesize = 0;
  cache->using_fp = 0;
  cache->saved_sp = 0;
  for (i = 0; i < E_NUM_REGS; i++)
    cache->saved_regs[i] = -1;
}

/* Function: iq2000_skip_prologue
   If the input address is in a function prologue, 
   returns the address of the end of the prologue;
   else returns the input address.

   Note: the input address is likely to be the function start, 
   since this function is mainly used for advancing a breakpoint
   to the first line, or stepping to the first line when we have
   stepped into a function call.  */

static CORE_ADDR
iq2000_skip_prologue (CORE_ADDR pc)
{
  CORE_ADDR func_addr = 0 , func_end = 0;

  if (find_pc_partial_function (pc, NULL, & func_addr, & func_end))
    {
      struct symtab_and_line sal;
      struct iq2000_frame_cache cache;

      /* Found a function.  */
      sal = find_pc_line (func_addr, 0);
      if (sal.end && sal.end < func_end)
	/* Found a line number, use it as end of prologue.  */
	return sal.end;

      /* No useable line symbol.  Use prologue parsing method.  */
      iq2000_init_frame_cache (&cache);
      return iq2000_scan_prologue (func_addr, func_end, NULL, &cache);
    }

  /* No function symbol -- just return the PC.  */
  return (CORE_ADDR) pc;
}

static struct iq2000_frame_cache *
iq2000_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct iq2000_frame_cache *cache;
  CORE_ADDR current_pc;
  int i;

  if (*this_cache)
    return *this_cache;

  cache = FRAME_OBSTACK_ZALLOC (struct iq2000_frame_cache);
  iq2000_init_frame_cache (cache);
  *this_cache = cache;

  cache->base = frame_unwind_register_unsigned (next_frame, E_FP_REGNUM);
  //if (cache->base == 0)
    //return cache;

  current_pc = frame_pc_unwind (next_frame);
  find_pc_partial_function (current_pc, NULL, &cache->pc, NULL);
  if (cache->pc != 0)
    iq2000_scan_prologue (cache->pc, current_pc, next_frame, cache);
  if (!cache->using_fp)
    cache->base = frame_unwind_register_unsigned (next_frame, E_SP_REGNUM);

  cache->saved_sp = cache->base + cache->framesize;

  for (i = 0; i < E_NUM_REGS; i++)
    if (cache->saved_regs[i] != -1)
      cache->saved_regs[i] += cache->base;

  return cache;
}

static void
iq2000_frame_prev_register (struct frame_info *next_frame, void **this_cache,
			    int regnum, int *optimizedp,
			    enum lval_type *lvalp, CORE_ADDR *addrp,
			    int *realnump, void *valuep)
{
  struct iq2000_frame_cache *cache = iq2000_frame_cache (next_frame, this_cache);
  if (regnum == E_SP_REGNUM && cache->saved_sp)
    {
      *optimizedp = 0;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      if (valuep)
        store_unsigned_integer (valuep, 4, cache->saved_sp);
      return;
    }

  if (regnum == E_PC_REGNUM)
    regnum = E_LR_REGNUM;

  if (regnum < E_NUM_REGS && cache->saved_regs[regnum] != -1)
    {
      *optimizedp = 0;
      *lvalp = lval_memory;
      *addrp = cache->saved_regs[regnum];
      *realnump = -1;
      if (valuep)
        read_memory (*addrp, valuep, register_size (current_gdbarch, regnum));
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
iq2000_frame_this_id (struct frame_info *next_frame, void **this_cache,
		      struct frame_id *this_id)
{
  struct iq2000_frame_cache *cache = iq2000_frame_cache (next_frame, this_cache);

  /* This marks the outermost frame.  */
  if (cache->base == 0) 
    return;

  *this_id = frame_id_build (cache->saved_sp, cache->pc);
}

static const struct frame_unwind iq2000_frame_unwind = {
  NORMAL_FRAME,
  iq2000_frame_this_id,
  iq2000_frame_prev_register
};

static const struct frame_unwind *
iq2000_frame_sniffer (struct frame_info *next_frame)
{
  return &iq2000_frame_unwind;
}

static CORE_ADDR
iq2000_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, E_SP_REGNUM);
}   

static CORE_ADDR
iq2000_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, E_PC_REGNUM);
}

static struct frame_id
iq2000_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_id_build (iq2000_unwind_sp (gdbarch, next_frame),
                         frame_pc_unwind (next_frame));
}

static CORE_ADDR
iq2000_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct iq2000_frame_cache *cache = iq2000_frame_cache (next_frame, this_cache);

  return cache->base;
}
  
static const struct frame_base iq2000_frame_base = {
  &iq2000_frame_unwind,
  iq2000_frame_base_address,
  iq2000_frame_base_address, 
  iq2000_frame_base_address
};

static const unsigned char *
iq2000_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  static const unsigned char big_breakpoint[] = { 0x00, 0x00, 0x00, 0x0d };
  static const unsigned char little_breakpoint[] = { 0x0d, 0x00, 0x00, 0x00 };

  if ((*pcptr & 3) != 0)
    error ("breakpoint_from_pc: invalid breakpoint address 0x%lx",
	   (long) *pcptr);

  *lenptr = 4;
  return (gdbarch_byte_order (current_gdbarch)
	  == BFD_ENDIAN_BIG) ? big_breakpoint
					       : little_breakpoint;
}

/* Target function return value methods: */

/* Function: store_return_value
   Copy the function return value from VALBUF into the 
   proper location for a function return.  */

static void
iq2000_store_return_value (struct type *type, struct regcache *regcache,
			   const void *valbuf)
{
  int len = TYPE_LENGTH (type);
  int regno = E_FN_RETURN_REGNUM;

  while (len > 0)
    {
      char buf[4];
      int size = len % 4 ?: 4;

      memset (buf, 0, 4);
      memcpy (buf + 4 - size, valbuf, size);
      regcache_raw_write (regcache, regno++, buf);
      len -= size;
      valbuf = ((char *) valbuf) + size;
    }
}

/* Function: use_struct_convention 
   Returns non-zero if the given struct type will be returned using
   a special convention, rather than the normal function return method.  */

static int
iq2000_use_struct_convention (struct type *type)
{
  return ((TYPE_CODE (type) == TYPE_CODE_STRUCT)
	  || (TYPE_CODE (type) == TYPE_CODE_UNION))
	 && TYPE_LENGTH (type) > 8;
}

/* Function: extract_return_value
   Copy the function's return value into VALBUF. 
   This function is called only in the context of "target function calls",
   ie. when the debugger forces a function to be called in the child, and
   when the debugger forces a function to return prematurely via the
   "return" command.  */

static void
iq2000_extract_return_value (struct type *type, struct regcache *regcache,
			     void *valbuf)
{
  /* If the function's return value is 8 bytes or less, it is
     returned in a register, and if larger than 8 bytes, it is 
     returned in a stack location which is pointed to by the same
     register.  */
  CORE_ADDR return_buffer;
  int len = TYPE_LENGTH (type);

  if (len <= (2 * 4))
    {
      int regno = E_FN_RETURN_REGNUM;

      /* Return values of <= 8 bytes are returned in 
	 FN_RETURN_REGNUM.  */
      while (len > 0)
	{
	  ULONGEST tmp;
	  int size = len % 4 ?: 4;

	  /* By using store_unsigned_integer we avoid having to
	     do anything special for small big-endian values.  */
	  regcache_cooked_read_unsigned (regcache, regno++, &tmp);
	  store_unsigned_integer (valbuf, size, tmp);
	  len -= size;
	  valbuf = ((char *) valbuf) + size;
	}
    }
  else
    {
      /* Return values > 8 bytes are returned in memory,
	 pointed to by FN_RETURN_REGNUM.  */
      regcache_cooked_read (regcache, E_FN_RETURN_REGNUM, & return_buffer);
      read_memory (return_buffer, valbuf, TYPE_LENGTH (type));
    }
}

static enum return_value_convention
iq2000_return_value (struct gdbarch *gdbarch, struct type *type,
		     struct regcache *regcache,
		     void *readbuf, const void *writebuf)
{
  if (iq2000_use_struct_convention (type))
    return RETURN_VALUE_STRUCT_CONVENTION;
  if (writebuf)
    iq2000_store_return_value (type, regcache, writebuf);
  else if (readbuf)
    iq2000_extract_return_value (type, regcache, readbuf);
  return RETURN_VALUE_REGISTER_CONVENTION;
}

/* Function: register_virtual_type
   Returns the default type for register N.  */

static struct type *
iq2000_register_type (struct gdbarch *gdbarch, int regnum)
{
  return builtin_type_int32;
}

static CORE_ADDR
iq2000_frame_align (struct gdbarch *ignore, CORE_ADDR sp)
{
  /* This is the same frame alignment used by gcc.  */
  return ((sp + 7) & ~7);
}

/* Convenience function to check 8-byte types for being a scalar type
   or a struct with only one long long or double member. */
static int
iq2000_pass_8bytetype_by_address (struct type *type)
{
  struct type *ftype;

  /* Skip typedefs.  */
  while (TYPE_CODE (type) == TYPE_CODE_TYPEDEF)
    type = TYPE_TARGET_TYPE (type);
  /* Non-struct and non-union types are always passed by value.  */
  if (TYPE_CODE (type) != TYPE_CODE_STRUCT
      && TYPE_CODE (type) != TYPE_CODE_UNION)
    return 0;
  /* Structs with more than 1 field are always passed by address.  */
  if (TYPE_NFIELDS (type) != 1)
    return 1;
  /* Get field type.  */
  ftype = (TYPE_FIELDS (type))[0].type;
  /* The field type must have size 8, otherwise pass by address.  */
  if (TYPE_LENGTH (ftype) != 8)
    return 1;
  /* Skip typedefs of field type.  */
  while (TYPE_CODE (ftype) == TYPE_CODE_TYPEDEF)
    ftype = TYPE_TARGET_TYPE (ftype);
  /* If field is int or float, pass by value.  */
  if (TYPE_CODE (ftype) == TYPE_CODE_FLT
      || TYPE_CODE (ftype) == TYPE_CODE_INT)
    return 0;
  /* Everything else, pass by address. */
  return 1;
}

static CORE_ADDR
iq2000_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		        struct regcache *regcache, CORE_ADDR bp_addr,
		        int nargs, struct value **args, CORE_ADDR sp,
		        int struct_return, CORE_ADDR struct_addr)
{
  const bfd_byte *val;
  bfd_byte buf[4];
  struct type *type;
  int i, argreg, typelen, slacklen;
  int stackspace = 0;
  /* Used to copy struct arguments into the stack. */
  CORE_ADDR struct_ptr;

  /* First determine how much stack space we will need. */
  for (i = 0, argreg = E_1ST_ARGREG + (struct_return != 0); i < nargs; i++)
    {
      type = value_type (args[i]);
      typelen = TYPE_LENGTH (type);
      if (typelen <= 4)
        {
          /* Scalars of up to 4 bytes, 
             structs of up to 4 bytes, and
             pointers.  */
          if (argreg <= E_LAST_ARGREG)
            argreg++;
          else
            stackspace += 4;
        }
      else if (typelen == 8 && !iq2000_pass_8bytetype_by_address (type))
        {
          /* long long, 
             double, and possibly
             structs with a single field of long long or double. */
          if (argreg <= E_LAST_ARGREG - 1)
            {
              /* 8-byte arg goes into a register pair
                 (must start with an even-numbered reg) */
              if (((argreg - E_1ST_ARGREG) % 2) != 0)
                argreg ++;
              argreg += 2;
            }
          else
            {
              argreg = E_LAST_ARGREG + 1;       /* no more argregs. */
              /* 8-byte arg goes on stack, must be 8-byte aligned. */
              stackspace = ((stackspace + 7) & ~7);
              stackspace += 8;
            }
        }
      else
	{
	  /* Structs are passed as pointer to a copy of the struct.
	     So we need room on the stack for a copy of the struct
	     plus for the argument pointer. */
          if (argreg <= E_LAST_ARGREG)
            argreg++;
          else
            stackspace += 4;
	  /* Care for 8-byte alignment of structs saved on stack.  */
	  stackspace += ((typelen + 7) & ~7);
	}
    }

  /* Now copy params, in ascending order, into their assigned location
     (either in a register or on the stack). */

  sp -= (sp % 8);       /* align */
  struct_ptr = sp;
  sp -= stackspace;
  sp -= (sp % 8);       /* align again */
  stackspace = 0;

  argreg = E_1ST_ARGREG;
  if (struct_return)
    {
      /* A function that returns a struct will consume one argreg to do so. 
       */
      regcache_cooked_write_unsigned (regcache, argreg++, struct_addr);
    }

  for (i = 0; i < nargs; i++)
    {
      type = value_type (args[i]);
      typelen = TYPE_LENGTH (type);
      val = value_contents (args[i]);
      if (typelen <= 4)
        {
          /* Char, short, int, float, pointer, and structs <= four bytes. */
	  slacklen = (4 - (typelen % 4)) % 4;
	  memset (buf, 0, sizeof (buf));
	  memcpy (buf + slacklen, val, typelen);
          if (argreg <= E_LAST_ARGREG)
            {
              /* Passed in a register. */
	      regcache_raw_write (regcache, argreg++, buf);
            }
          else
            {
              /* Passed on the stack. */
              write_memory (sp + stackspace, buf, 4);
              stackspace += 4;
            }
        }
      else if (typelen == 8 && !iq2000_pass_8bytetype_by_address (type))
        {
          /* (long long), (double), or struct consisting of 
             a single (long long) or (double). */
          if (argreg <= E_LAST_ARGREG - 1)
            {
              /* 8-byte arg goes into a register pair
                 (must start with an even-numbered reg) */
              if (((argreg - E_1ST_ARGREG) % 2) != 0)
                argreg++;
	      regcache_raw_write (regcache, argreg++, val);
	      regcache_raw_write (regcache, argreg++, val + 4);
            }
          else
            {
              /* 8-byte arg goes on stack, must be 8-byte aligned. */
              argreg = E_LAST_ARGREG + 1;       /* no more argregs. */
              stackspace = ((stackspace + 7) & ~7);
              write_memory (sp + stackspace, val, typelen);
              stackspace += 8;
            }
        }
      else
        {
	  /* Store struct beginning at the upper end of the previously
	     computed stack space.  Then store the address of the struct
	     using the usual rules for a 4 byte value.  */
	  struct_ptr -= ((typelen + 7) & ~7);
	  write_memory (struct_ptr, val, typelen);
	  if (argreg <= E_LAST_ARGREG)
	    regcache_cooked_write_unsigned (regcache, argreg++, struct_ptr);
	  else
	    {
	      store_unsigned_integer (buf, 4, struct_ptr);
	      write_memory (sp + stackspace, buf, 4);
	      stackspace += 4;
	    }
        }
    }

  /* Store return address. */
  regcache_cooked_write_unsigned (regcache, E_LR_REGNUM, bp_addr);

  /* Update stack pointer.  */
  regcache_cooked_write_unsigned (regcache, E_SP_REGNUM, sp);

  /* And that should do it.  Return the new stack pointer. */
  return sp;
}

/* Function: gdbarch_init
   Initializer function for the iq2000 gdbarch vector.
   Called by gdbarch.  Sets up the gdbarch vector(s) for this target.  */

static struct gdbarch *
iq2000_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;

  /* Look up list for candidates - only one.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  gdbarch = gdbarch_alloc (&info, NULL);

  set_gdbarch_num_regs             (gdbarch, E_NUM_REGS);
  set_gdbarch_num_pseudo_regs      (gdbarch, 0);
  set_gdbarch_sp_regnum            (gdbarch, E_SP_REGNUM);
  set_gdbarch_pc_regnum            (gdbarch, E_PC_REGNUM);
  set_gdbarch_register_name        (gdbarch, iq2000_register_name);
  set_gdbarch_address_to_pointer   (gdbarch, iq2000_address_to_pointer);
  set_gdbarch_pointer_to_address   (gdbarch, iq2000_pointer_to_address);
  set_gdbarch_ptr_bit              (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_short_bit            (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_int_bit              (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_bit             (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_long_bit        (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_float_bit            (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_double_bit           (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_long_double_bit      (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_float_format         (gdbarch, floatformats_ieee_single);
  set_gdbarch_double_format        (gdbarch, floatformats_ieee_double);
  set_gdbarch_long_double_format   (gdbarch, floatformats_ieee_double);
  set_gdbarch_return_value	   (gdbarch, iq2000_return_value);
  set_gdbarch_breakpoint_from_pc   (gdbarch, iq2000_breakpoint_from_pc);
  set_gdbarch_frame_args_skip      (gdbarch, 0);
  set_gdbarch_skip_prologue        (gdbarch, iq2000_skip_prologue);
  set_gdbarch_inner_than           (gdbarch, core_addr_lessthan);
  set_gdbarch_print_insn           (gdbarch, print_insn_iq2000);
  set_gdbarch_register_type (gdbarch, iq2000_register_type);
  set_gdbarch_frame_align (gdbarch, iq2000_frame_align);
  set_gdbarch_unwind_sp (gdbarch, iq2000_unwind_sp);
  set_gdbarch_unwind_pc (gdbarch, iq2000_unwind_pc);
  set_gdbarch_unwind_dummy_id (gdbarch, iq2000_unwind_dummy_id);
  frame_base_set_default (gdbarch, &iq2000_frame_base);
  set_gdbarch_push_dummy_call (gdbarch, iq2000_push_dummy_call);

  gdbarch_init_osabi (info, gdbarch);

  frame_unwind_append_sniffer (gdbarch, dwarf2_frame_sniffer);
  frame_unwind_append_sniffer (gdbarch, iq2000_frame_sniffer);

  return gdbarch;
}

/* Function: _initialize_iq2000_tdep
   Initializer function for the iq2000 module.
   Called by gdb at start-up. */

void
_initialize_iq2000_tdep (void)
{
  register_gdbarch_init (bfd_arch_iq2000, iq2000_gdbarch_init);
}
