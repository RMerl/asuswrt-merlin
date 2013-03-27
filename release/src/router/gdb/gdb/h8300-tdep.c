/* Target-machine dependent code for Renesas H8/300, for GDB.

   Copyright (C) 1988, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1998, 1999,
   2000, 2001, 2002, 2003, 2005, 2007 Free Software Foundation, Inc.

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

/*
   Contributed by Steve Chamberlain
   sac@cygnus.com
 */

#include "defs.h"
#include "value.h"
#include "arch-utils.h"
#include "regcache.h"
#include "gdbcore.h"
#include "objfiles.h"
#include "gdb_assert.h"
#include "dis-asm.h"
#include "dwarf2-frame.h"
#include "frame-base.h"
#include "frame-unwind.h"

enum gdb_regnum
{
  E_R0_REGNUM, E_ER0_REGNUM = E_R0_REGNUM, E_ARG0_REGNUM = E_R0_REGNUM,
  E_RET0_REGNUM = E_R0_REGNUM,
  E_R1_REGNUM, E_ER1_REGNUM = E_R1_REGNUM, E_RET1_REGNUM = E_R1_REGNUM,
  E_R2_REGNUM, E_ER2_REGNUM = E_R2_REGNUM, E_ARGLAST_REGNUM = E_R2_REGNUM,
  E_R3_REGNUM, E_ER3_REGNUM = E_R3_REGNUM,
  E_R4_REGNUM, E_ER4_REGNUM = E_R4_REGNUM,
  E_R5_REGNUM, E_ER5_REGNUM = E_R5_REGNUM,
  E_R6_REGNUM, E_ER6_REGNUM = E_R6_REGNUM, E_FP_REGNUM = E_R6_REGNUM,
  E_SP_REGNUM,
  E_CCR_REGNUM,
  E_PC_REGNUM,
  E_CYCLES_REGNUM,
  E_TICK_REGNUM, E_EXR_REGNUM = E_TICK_REGNUM,
  E_INST_REGNUM, E_TICKS_REGNUM = E_INST_REGNUM,
  E_INSTS_REGNUM,
  E_MACH_REGNUM,
  E_MACL_REGNUM,
  E_SBR_REGNUM,
  E_VBR_REGNUM
};

#define H8300_MAX_NUM_REGS 18

#define E_PSEUDO_CCR_REGNUM (gdbarch_num_regs (current_gdbarch))
#define E_PSEUDO_EXR_REGNUM (gdbarch_num_regs (current_gdbarch)+1)

struct h8300_frame_cache
{
  /* Base address.  */
  CORE_ADDR base;
  CORE_ADDR sp_offset;
  CORE_ADDR pc;

  /* Flag showing that a frame has been created in the prologue code. */
  int uses_fp;

  /* Saved registers.  */
  CORE_ADDR saved_regs[H8300_MAX_NUM_REGS];
  CORE_ADDR saved_sp;
};

enum
{
  h8300_reg_size = 2,
  h8300h_reg_size = 4,
  h8300_max_reg_size = 4,
};

static int is_h8300hmode (struct gdbarch *gdbarch);
static int is_h8300smode (struct gdbarch *gdbarch);
static int is_h8300sxmode (struct gdbarch *gdbarch);
static int is_h8300_normal_mode (struct gdbarch *gdbarch);

#define BINWORD ((is_h8300hmode (current_gdbarch) \
		  && !is_h8300_normal_mode (current_gdbarch)) \
		 ? h8300h_reg_size : h8300_reg_size)

static CORE_ADDR
h8300_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, E_PC_REGNUM);
}

static CORE_ADDR
h8300_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, E_SP_REGNUM);
}

static struct frame_id
h8300_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_id_build (h8300_unwind_sp (gdbarch, next_frame),
  			 frame_pc_unwind (next_frame));
}

/* Normal frames.  */

/* Allocate and initialize a frame cache.  */

static void
h8300_init_frame_cache (struct h8300_frame_cache *cache)
{
  int i;

  /* Base address.  */
  cache->base = 0;
  cache->sp_offset = 0;
  cache->pc = 0;

  /* Frameless until proven otherwise.  */
  cache->uses_fp = 0;

  /* Saved registers.  We initialize these to -1 since zero is a valid
     offset (that's where %fp is supposed to be stored).  */
  for (i = 0; i < gdbarch_num_regs (current_gdbarch); i++)
    cache->saved_regs[i] = -1;
}

#define IS_MOVB_RnRm(x)		(((x) & 0xff88) == 0x0c88)
#define IS_MOVW_RnRm(x)		(((x) & 0xff88) == 0x0d00)
#define IS_MOVL_RnRm(x)		(((x) & 0xff88) == 0x0f80)
#define IS_MOVB_Rn16_SP(x)	(((x) & 0xfff0) == 0x6ee0)
#define IS_MOVB_EXT(x)		((x) == 0x7860)
#define IS_MOVB_Rn24_SP(x)	(((x) & 0xfff0) == 0x6aa0)
#define IS_MOVW_Rn16_SP(x)	(((x) & 0xfff0) == 0x6fe0)
#define IS_MOVW_EXT(x)		((x) == 0x78e0)
#define IS_MOVW_Rn24_SP(x)	(((x) & 0xfff0) == 0x6ba0)
/* Same instructions as mov.w, just prefixed with 0x0100 */
#define IS_MOVL_PRE(x)		((x) == 0x0100)
#define IS_MOVL_Rn16_SP(x)	(((x) & 0xfff0) == 0x6fe0)
#define IS_MOVL_EXT(x)		((x) == 0x78e0)
#define IS_MOVL_Rn24_SP(x)	(((x) & 0xfff0) == 0x6ba0)

#define IS_PUSHFP_MOVESPFP(x)	((x) == 0x6df60d76)
#define IS_PUSH_FP(x)		((x) == 0x01006df6)
#define IS_MOV_SP_FP(x)		((x) == 0x0ff6)
#define IS_SUB2_SP(x)		((x) == 0x1b87)
#define IS_SUB4_SP(x)		((x) == 0x1b97)
#define IS_ADD_IMM_SP(x)	((x) == 0x7a1f)
#define IS_SUB_IMM_SP(x)	((x) == 0x7a3f)
#define IS_SUBL4_SP(x)		((x) == 0x1acf)
#define IS_MOV_IMM_Rn(x)	(((x) & 0xfff0) == 0x7905)
#define IS_SUB_RnSP(x)		(((x) & 0xff0f) == 0x1907)
#define IS_ADD_RnSP(x)		(((x) & 0xff0f) == 0x0907)
#define IS_PUSH(x)		(((x) & 0xfff0) == 0x6df0)

/* If the instruction at PC is an argument register spill, return its
   length.  Otherwise, return zero.

   An argument register spill is an instruction that moves an argument
   from the register in which it was passed to the stack slot in which
   it really lives.  It is a byte, word, or longword move from an
   argument register to a negative offset from the frame pointer.
   
   CV, 2003-06-16: Or, in optimized code or when the `register' qualifier
   is used, it could be a byte, word or long move to registers r3-r5.  */

static int
h8300_is_argument_spill (CORE_ADDR pc)
{
  int w = read_memory_unsigned_integer (pc, 2);

  if ((IS_MOVB_RnRm (w) || IS_MOVW_RnRm (w) || IS_MOVL_RnRm (w))
      && (w & 0x70) <= 0x20	/* Rs is R0, R1 or R2 */
      && (w & 0x7) >= 0x3 && (w & 0x7) <= 0x5)	/* Rd is R3, R4 or R5 */
    return 2;

  if (IS_MOVB_Rn16_SP (w)
      && 8 <= (w & 0xf) && (w & 0xf) <= 10)	/* Rs is R0L, R1L, or R2L  */
    {
      if (read_memory_integer (pc + 2, 2) < 0)	/* ... and d:16 is negative.  */
	return 4;
    }
  else if (IS_MOVB_EXT (w))
    {
      if (IS_MOVB_Rn24_SP (read_memory_unsigned_integer (pc + 2, 2)))
	{
	  LONGEST disp = read_memory_integer (pc + 4, 4);

	  /* ... and d:24 is negative.  */
	  if (disp < 0 && disp > 0xffffff)
	    return 8;
	}
    }
  else if (IS_MOVW_Rn16_SP (w)
	   && (w & 0xf) <= 2)	/* Rs is R0, R1, or R2 */
    {
      /* ... and d:16 is negative.  */
      if (read_memory_integer (pc + 2, 2) < 0)
	return 4;
    }
  else if (IS_MOVW_EXT (w))
    {
      if (IS_MOVW_Rn24_SP (read_memory_unsigned_integer (pc + 2, 2)))
	{
	  LONGEST disp = read_memory_integer (pc + 4, 4);

	  /* ... and d:24 is negative.  */
	  if (disp < 0 && disp > 0xffffff)
	    return 8;
	}
    }
  else if (IS_MOVL_PRE (w))
    {
      int w2 = read_memory_integer (pc + 2, 2);

      if (IS_MOVL_Rn16_SP (w2)
	  && (w2 & 0xf) <= 2)	/* Rs is ER0, ER1, or ER2 */
	{
	  /* ... and d:16 is negative.  */
	  if (read_memory_integer (pc + 4, 2) < 0)
	    return 6;
	}
      else if (IS_MOVL_EXT (w2))
	{
	  int w3 = read_memory_integer (pc + 4, 2);

	  if (IS_MOVL_Rn24_SP (read_memory_integer (pc + 4, 2)))
	    {
	      LONGEST disp = read_memory_integer (pc + 6, 4);

	      /* ... and d:24 is negative.  */
	      if (disp < 0 && disp > 0xffffff)
		return 10;
	    }
	}
    }

  return 0;
}

/* Do a full analysis of the prologue at PC and update CACHE
   accordingly.  Bail out early if CURRENT_PC is reached.  Return the
   address where the analysis stopped.

   We handle all cases that can be generated by gcc.

   For allocating a stack frame:

   mov.w r6,@-sp
   mov.w sp,r6
   mov.w #-n,rN
   add.w rN,sp

   mov.w r6,@-sp
   mov.w sp,r6
   subs  #2,sp
   (repeat)

   mov.l er6,@-sp
   mov.l sp,er6
   add.l #-n,sp

   mov.w r6,@-sp
   mov.w sp,r6
   subs  #4,sp
   (repeat)

   For saving registers:

   mov.w rN,@-sp
   mov.l erN,@-sp
   stm.l reglist,@-sp

   */

static CORE_ADDR
h8300_analyze_prologue (CORE_ADDR pc, CORE_ADDR current_pc,
			struct h8300_frame_cache *cache)
{
  unsigned int op;
  int regno, i, spill_size;

  cache->sp_offset = 0;

  if (pc >= current_pc)
    return current_pc;

  op = read_memory_unsigned_integer (pc, 4);

  if (IS_PUSHFP_MOVESPFP (op))
    {
      cache->saved_regs[E_FP_REGNUM] = 0;
      cache->uses_fp = 1;
      pc += 4;
    }
  else if (IS_PUSH_FP (op))
    {
      cache->saved_regs[E_FP_REGNUM] = 0;
      pc += 4;
      if (pc >= current_pc)
        return current_pc;
      op = read_memory_unsigned_integer (pc, 2);
      if (IS_MOV_SP_FP (op))
	{
	  cache->uses_fp = 1;
	  pc += 2;
	}
    }

  while (pc < current_pc)
    {
      op = read_memory_unsigned_integer (pc, 2);
      if (IS_SUB2_SP (op))
	{
	  cache->sp_offset += 2;
	  pc += 2;
	}
      else if (IS_SUB4_SP (op))
	{
	  cache->sp_offset += 4;
	  pc += 2;
	}
      else if (IS_ADD_IMM_SP (op))
	{
	  cache->sp_offset += -read_memory_integer (pc + 2, 2);
	  pc += 4;
	}
      else if (IS_SUB_IMM_SP (op))
	{
	  cache->sp_offset += read_memory_integer (pc + 2, 2);
	  pc += 4;
	}
      else if (IS_SUBL4_SP (op))
	{
	  cache->sp_offset += 4;
	  pc += 2;
	}
      else if (IS_MOV_IMM_Rn (op))
        {
	  int offset = read_memory_integer (pc + 2, 2);
	  regno = op & 0x000f;
	  op = read_memory_unsigned_integer (pc + 4, 2);
	  if (IS_ADD_RnSP (op) && (op & 0x00f0) == regno)
	    {
	      cache->sp_offset -= offset;
	      pc += 6;
	    }
	  else if (IS_SUB_RnSP (op) && (op & 0x00f0) == regno)
	    {
	      cache->sp_offset += offset;
	      pc += 6;
	    }
	  else
	    break;
	}
      else if (IS_PUSH (op))
	{
	  regno = op & 0x000f;
	  cache->sp_offset += 2;
	  cache->saved_regs[regno] = cache->sp_offset;
	  pc += 2;
	}
      else if (op == 0x0100)
	{
	  op = read_memory_unsigned_integer (pc + 2, 2);
	  if (IS_PUSH (op))
	    {
	      regno = op & 0x000f;
	      cache->sp_offset += 4;
	      cache->saved_regs[regno] = cache->sp_offset;
	      pc += 4;
	    }
	  else
	    break;
	}
      else if ((op & 0xffcf) == 0x0100)
	{
	  int op1;
	  op1 = read_memory_unsigned_integer (pc + 2, 2);
	  if (IS_PUSH (op1))
	    {
	      /* Since the prefix is 0x01x0, this is not a simple pushm but a
	         stm.l reglist,@-sp */
	      i = ((op & 0x0030) >> 4) + 1;
	      regno = op1 & 0x000f;
	      for (; i > 0; regno++, --i)
		{
		  cache->sp_offset += 4;
		  cache->saved_regs[regno] = cache->sp_offset;
		}
	      pc += 4;
	    }
	  else
	    break;
	}
      else
	break;
    }

  /* Check for spilling an argument register to the stack frame.
     This could also be an initializing store from non-prologue code,
     but I don't think there's any harm in skipping that.  */
  while ((spill_size = h8300_is_argument_spill (pc)) > 0
         && pc + spill_size <= current_pc)
    pc += spill_size;

  return pc;
}

static struct h8300_frame_cache *
h8300_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct h8300_frame_cache *cache;
  char buf[4];
  int i;
  CORE_ADDR current_pc;

  if (*this_cache)
    return *this_cache;

  cache = FRAME_OBSTACK_ZALLOC (struct h8300_frame_cache);
  h8300_init_frame_cache (cache);
  *this_cache = cache;

  /* In principle, for normal frames, %fp holds the frame pointer,
     which holds the base address for the current stack frame.
     However, for functions that don't need it, the frame pointer is
     optional.  For these "frameless" functions the frame pointer is
     actually the frame pointer of the calling frame.  */

  cache->base = frame_unwind_register_unsigned (next_frame, E_FP_REGNUM);
  if (cache->base == 0)
    return cache;

  cache->saved_regs[E_PC_REGNUM] = -BINWORD;

  cache->pc = frame_func_unwind (next_frame, NORMAL_FRAME);
  current_pc = frame_pc_unwind (next_frame);
  if (cache->pc != 0)
    h8300_analyze_prologue (cache->pc, current_pc, cache);

  if (!cache->uses_fp)
    {
      /* We didn't find a valid frame, which means that CACHE->base
         currently holds the frame pointer for our calling frame.  If
         we're at the start of a function, or somewhere half-way its
         prologue, the function's frame probably hasn't been fully
         setup yet.  Try to reconstruct the base address for the stack
         frame by looking at the stack pointer.  For truly "frameless"
         functions this might work too.  */

      cache->base = frame_unwind_register_unsigned (next_frame, E_SP_REGNUM)
		    + cache->sp_offset;
      cache->saved_sp = cache->base + BINWORD;
      cache->saved_regs[E_PC_REGNUM] = 0;
    }
  else
    {
      cache->saved_sp = cache->base + 2 * BINWORD;
      cache->saved_regs[E_PC_REGNUM] = -BINWORD;
    }

  /* Adjust all the saved registers such that they contain addresses
     instead of offsets.  */
  for (i = 0; i < gdbarch_num_regs (current_gdbarch); i++)
    if (cache->saved_regs[i] != -1)
      cache->saved_regs[i] = cache->base - cache->saved_regs[i];

  return cache;
}

static void
h8300_frame_this_id (struct frame_info *next_frame, void **this_cache,
		     struct frame_id *this_id)
{
  struct h8300_frame_cache *cache =
    h8300_frame_cache (next_frame, this_cache);

  /* This marks the outermost frame.  */
  if (cache->base == 0)
    return;

  *this_id = frame_id_build (cache->saved_sp, cache->pc);
}

static void
h8300_frame_prev_register (struct frame_info *next_frame, void **this_cache,
			   int regnum, int *optimizedp,
			   enum lval_type *lvalp, CORE_ADDR *addrp,
			   int *realnump, gdb_byte *valuep)
{
  struct h8300_frame_cache *cache =
    h8300_frame_cache (next_frame, this_cache);

  gdb_assert (regnum >= 0);

  if (regnum == E_SP_REGNUM && cache->saved_sp)
    {
      *optimizedp = 0;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      if (valuep)
	store_unsigned_integer (valuep, BINWORD, cache->saved_sp);
      return;
    }

  if (regnum < gdbarch_num_regs (current_gdbarch)
      && cache->saved_regs[regnum] != -1)
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
    frame_unwind_register (next_frame, *realnump, valuep);
}

static const struct frame_unwind h8300_frame_unwind = {
  NORMAL_FRAME,
  h8300_frame_this_id,
  h8300_frame_prev_register
};

static const struct frame_unwind *
h8300_frame_sniffer (struct frame_info *next_frame)
{
  return &h8300_frame_unwind;
}

static CORE_ADDR
h8300_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct h8300_frame_cache *cache = h8300_frame_cache (next_frame, this_cache);
  return cache->base;
}

static const struct frame_base h8300_frame_base = {
  &h8300_frame_unwind,
  h8300_frame_base_address,
  h8300_frame_base_address,
  h8300_frame_base_address
};

static CORE_ADDR
h8300_skip_prologue (CORE_ADDR pc)
{
  CORE_ADDR func_addr = 0 , func_end = 0;

  if (find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    {
      struct symtab_and_line sal;
      struct h8300_frame_cache cache;

      /* Found a function.  */
      sal = find_pc_line (func_addr, 0);
      if (sal.end && sal.end < func_end)
        /* Found a line number, use it as end of prologue.  */
        return sal.end;

      /* No useable line symbol.  Use prologue parsing method.  */
      h8300_init_frame_cache (&cache);
      return h8300_analyze_prologue (func_addr, func_end, &cache);
    }

  /* No function symbol -- just return the PC.  */
  return (CORE_ADDR) pc;
}

/* Function: push_dummy_call
   Setup the function arguments for calling a function in the inferior.
   In this discussion, a `word' is 16 bits on the H8/300s, and 32 bits
   on the H8/300H.

   There are actually two ABI's here: -mquickcall (the default) and
   -mno-quickcall.  With -mno-quickcall, all arguments are passed on
   the stack after the return address, word-aligned.  With
   -mquickcall, GCC tries to use r0 -- r2 to pass registers.  Since
   GCC doesn't indicate in the object file which ABI was used to
   compile it, GDB only supports the default --- -mquickcall.

   Here are the rules for -mquickcall, in detail:

   Each argument, whether scalar or aggregate, is padded to occupy a
   whole number of words.  Arguments smaller than a word are padded at
   the most significant end; those larger than a word are padded at
   the least significant end.

   The initial arguments are passed in r0 -- r2.  Earlier arguments go in
   lower-numbered registers.  Multi-word arguments are passed in
   consecutive registers, with the most significant end in the
   lower-numbered register.

   If an argument doesn't fit entirely in the remaining registers, it
   is passed entirely on the stack.  Stack arguments begin just after
   the return address.  Once an argument has overflowed onto the stack
   this way, all subsequent arguments are passed on the stack.

   The above rule has odd consequences.  For example, on the h8/300s,
   if a function takes two longs and an int as arguments:
   - the first long will be passed in r0/r1,
   - the second long will be passed entirely on the stack, since it
     doesn't fit in r2,
   - and the int will be passed on the stack, even though it could fit
     in r2.

   A weird exception: if an argument is larger than a word, but not a
   whole number of words in length (before padding), it is passed on
   the stack following the rules for stack arguments above, even if
   there are sufficient registers available to hold it.  Stranger
   still, the argument registers are still `used up' --- even though
   there's nothing in them.

   So, for example, on the h8/300s, if a function expects a three-byte
   structure and an int, the structure will go on the stack, and the
   int will go in r2, not r0.
  
   If the function returns an aggregate type (struct, union, or class)
   by value, the caller must allocate space to hold the return value,
   and pass the callee a pointer to this space as an invisible first
   argument, in R0.

   For varargs functions, the last fixed argument and all the variable
   arguments are always passed on the stack.  This means that calls to
   varargs functions don't work properly unless there is a prototype
   in scope.

   Basically, this ABI is not good, for the following reasons:
   - You can't call vararg functions properly unless a prototype is in scope.
   - Structure passing is inconsistent, to no purpose I can see.
   - It often wastes argument registers, of which there are only three
     to begin with.  */

static CORE_ADDR
h8300_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		       struct regcache *regcache, CORE_ADDR bp_addr,
		       int nargs, struct value **args, CORE_ADDR sp,
		       int struct_return, CORE_ADDR struct_addr)
{
  int stack_alloc = 0, stack_offset = 0;
  int wordsize = BINWORD;
  int reg = E_ARG0_REGNUM;
  int argument;

  /* First, make sure the stack is properly aligned.  */
  sp = align_down (sp, wordsize);

  /* Now make sure there's space on the stack for the arguments.  We
     may over-allocate a little here, but that won't hurt anything.  */
  for (argument = 0; argument < nargs; argument++)
    stack_alloc += align_up (TYPE_LENGTH (value_type (args[argument])),
			     wordsize);
  sp -= stack_alloc;

  /* Now load as many arguments as possible into registers, and push
     the rest onto the stack.
     If we're returning a structure by value, then we must pass a
     pointer to the buffer for the return value as an invisible first
     argument.  */
  if (struct_return)
    regcache_cooked_write_unsigned (regcache, reg++, struct_addr);

  for (argument = 0; argument < nargs; argument++)
    {
      struct type *type = value_type (args[argument]);
      int len = TYPE_LENGTH (type);
      char *contents = (char *) value_contents (args[argument]);

      /* Pad the argument appropriately.  */
      int padded_len = align_up (len, wordsize);
      gdb_byte *padded = alloca (padded_len);

      memset (padded, 0, padded_len);
      memcpy (len < wordsize ? padded + padded_len - len : padded,
	      contents, len);

      /* Could the argument fit in the remaining registers?  */
      if (padded_len <= (E_ARGLAST_REGNUM - reg + 1) * wordsize)
	{
	  /* Are we going to pass it on the stack anyway, for no good
	     reason?  */
	  if (len > wordsize && len % wordsize)
	    {
	      /* I feel so unclean.  */
	      write_memory (sp + stack_offset, padded, padded_len);
	      stack_offset += padded_len;

	      /* That's right --- even though we passed the argument
	         on the stack, we consume the registers anyway!  Love
	         me, love my dog.  */
	      reg += padded_len / wordsize;
	    }
	  else
	    {
	      /* Heavens to Betsy --- it's really going in registers!
	         It would be nice if we could use write_register_bytes
	         here, but on the h8/300s, there are gaps between
	         the registers in the register file.  */
	      int offset;

	      for (offset = 0; offset < padded_len; offset += wordsize)
		{
		  ULONGEST word = extract_unsigned_integer (padded + offset,
							    wordsize);
		  regcache_cooked_write_unsigned (regcache, reg++, word);
		}
	    }
	}
      else
	{
	  /* It doesn't fit in registers!  Onto the stack it goes.  */
	  write_memory (sp + stack_offset, padded, padded_len);
	  stack_offset += padded_len;

	  /* Once one argument has spilled onto the stack, all
	     subsequent arguments go on the stack.  */
	  reg = E_ARGLAST_REGNUM + 1;
	}
    }

  /* Store return address.  */
  sp -= wordsize;
  write_memory_unsigned_integer (sp, wordsize, bp_addr);

  /* Update stack pointer.  */
  regcache_cooked_write_unsigned (regcache, E_SP_REGNUM, sp);

  /* Return the new stack pointer minus the return address slot since
     that's what DWARF2/GCC uses as the frame's CFA.  */
  return sp + wordsize;
}

/* Function: extract_return_value
   Figure out where in REGBUF the called function has left its return value.
   Copy that into VALBUF.  Be sure to account for CPU type.   */

static void
h8300_extract_return_value (struct type *type, struct regcache *regcache,
			    void *valbuf)
{
  int len = TYPE_LENGTH (type);
  ULONGEST c, addr;

  switch (len)
    {
    case 1:
    case 2:
      regcache_cooked_read_unsigned (regcache, E_RET0_REGNUM, &c);
      store_unsigned_integer (valbuf, len, c);
      break;
    case 4:			/* Needs two registers on plain H8/300 */
      regcache_cooked_read_unsigned (regcache, E_RET0_REGNUM, &c);
      store_unsigned_integer (valbuf, 2, c);
      regcache_cooked_read_unsigned (regcache, E_RET1_REGNUM, &c);
      store_unsigned_integer ((void *) ((char *) valbuf + 2), 2, c);
      break;
    case 8:			/* long long is now 8 bytes.  */
      if (TYPE_CODE (type) == TYPE_CODE_INT)
	{
	  regcache_cooked_read_unsigned (regcache, E_RET0_REGNUM, &addr);
	  c = read_memory_unsigned_integer ((CORE_ADDR) addr, len);
	  store_unsigned_integer (valbuf, len, c);
	}
      else
	{
	  error ("I don't know how this 8 byte value is returned.");
	}
      break;
    }
}

static void
h8300h_extract_return_value (struct type *type, struct regcache *regcache,
			     void *valbuf)
{
  int len = TYPE_LENGTH (type);
  ULONGEST c, addr;

  switch (len)
    {
    case 1:
    case 2:
    case 4:
      regcache_cooked_read_unsigned (regcache, E_RET0_REGNUM, &c);
      store_unsigned_integer (valbuf, len, c);
      break;
    case 8:			/* long long is now 8 bytes.  */
      if (TYPE_CODE (type) == TYPE_CODE_INT)
	{
	  regcache_cooked_read_unsigned (regcache, E_RET0_REGNUM, &c);
	  store_unsigned_integer (valbuf, 4, c);
	  regcache_cooked_read_unsigned (regcache, E_RET1_REGNUM, &c);
	  store_unsigned_integer ((void *) ((char *) valbuf + 4), 4, c);
	}
      else
	{
	  error ("I don't know how this 8 byte value is returned.");
	}
      break;
    }
}

int
h8300_use_struct_convention (struct type *value_type)
{
  /* Types of 1, 2 or 4 bytes are returned in R0/R1, everything else on the
     stack. */

  if (TYPE_CODE (value_type) == TYPE_CODE_STRUCT
      || TYPE_CODE (value_type) == TYPE_CODE_UNION)
    return 1;
  return !(TYPE_LENGTH (value_type) == 1
	   || TYPE_LENGTH (value_type) == 2
	   || TYPE_LENGTH (value_type) == 4);
}

int
h8300h_use_struct_convention (struct type *value_type)
{
  /* Types of 1, 2 or 4 bytes are returned in R0, INT types of 8 bytes are
     returned in R0/R1, everything else on the stack. */
  if (TYPE_CODE (value_type) == TYPE_CODE_STRUCT
      || TYPE_CODE (value_type) == TYPE_CODE_UNION)
    return 1;
  return !(TYPE_LENGTH (value_type) == 1
	   || TYPE_LENGTH (value_type) == 2
	   || TYPE_LENGTH (value_type) == 4
	   || (TYPE_LENGTH (value_type) == 8
	       && TYPE_CODE (value_type) == TYPE_CODE_INT));
}

/* Function: store_return_value
   Place the appropriate value in the appropriate registers.
   Primarily used by the RETURN command.  */

static void
h8300_store_return_value (struct type *type, struct regcache *regcache,
			  const void *valbuf)
{
  int len = TYPE_LENGTH (type);
  ULONGEST val;

  switch (len)
    {
    case 1:
    case 2:			/* short... */
      val = extract_unsigned_integer (valbuf, len);
      regcache_cooked_write_unsigned (regcache, E_RET0_REGNUM, val);
      break;
    case 4:			/* long, float */
      val = extract_unsigned_integer (valbuf, len);
      regcache_cooked_write_unsigned (regcache, E_RET0_REGNUM,
				      (val >> 16) & 0xffff);
      regcache_cooked_write_unsigned (regcache, E_RET1_REGNUM, val & 0xffff);
      break;
    case 8:			/* long long, double and long double are all defined
				   as 4 byte types so far so this shouldn't happen.  */
      error ("I don't know how to return an 8 byte value.");
      break;
    }
}

static void
h8300h_store_return_value (struct type *type, struct regcache *regcache,
			   const void *valbuf)
{
  int len = TYPE_LENGTH (type);
  ULONGEST val;

  switch (len)
    {
    case 1:
    case 2:
    case 4:			/* long, float */
      val = extract_unsigned_integer (valbuf, len);
      regcache_cooked_write_unsigned (regcache, E_RET0_REGNUM, val);
      break;
    case 8:
      val = extract_unsigned_integer (valbuf, len);
      regcache_cooked_write_unsigned (regcache, E_RET0_REGNUM,
				      (val >> 32) & 0xffffffff);
      regcache_cooked_write_unsigned (regcache, E_RET1_REGNUM,
				      val & 0xffffffff);
      break;
    }
}

static enum return_value_convention
h8300_return_value (struct gdbarch *gdbarch, struct type *type,
		    struct regcache *regcache,
		    gdb_byte *readbuf, const gdb_byte *writebuf)
{
  if (h8300_use_struct_convention (type))
    return RETURN_VALUE_STRUCT_CONVENTION;
  if (writebuf)
    h8300_store_return_value (type, regcache, writebuf);
  else if (readbuf)
    h8300_extract_return_value (type, regcache, readbuf);
  return RETURN_VALUE_REGISTER_CONVENTION;
}

static enum return_value_convention
h8300h_return_value (struct gdbarch *gdbarch, struct type *type,
		     struct regcache *regcache,
		     gdb_byte *readbuf, const gdb_byte *writebuf)
{
  if (h8300h_use_struct_convention (type))
    {
      if (readbuf)
	{
	  ULONGEST addr;

	  regcache_raw_read_unsigned (regcache, E_R0_REGNUM, &addr);
	  read_memory (addr, readbuf, TYPE_LENGTH (type));
	}

      return RETURN_VALUE_ABI_RETURNS_ADDRESS;
    }
  if (writebuf)
    h8300h_store_return_value (type, regcache, writebuf);
  else if (readbuf)
    h8300h_extract_return_value (type, regcache, readbuf);
  return RETURN_VALUE_REGISTER_CONVENTION;
}

static struct cmd_list_element *setmachinelist;

static const char *
h8300_register_name (int regno)
{
  /* The register names change depending on which h8300 processor
     type is selected. */
  static char *register_names[] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6",
    "sp", "", "pc", "cycles", "tick", "inst",
    "ccr",			/* pseudo register */
  };
  if (regno < 0
      || regno >= (sizeof (register_names) / sizeof (*register_names)))
    internal_error (__FILE__, __LINE__,
		    "h8300_register_name: illegal register number %d", regno);
  else
    return register_names[regno];
}

static const char *
h8300s_register_name (int regno)
{
  static char *register_names[] = {
    "er0", "er1", "er2", "er3", "er4", "er5", "er6",
    "sp", "", "pc", "cycles", "", "tick", "inst",
    "mach", "macl",
    "ccr", "exr"		/* pseudo registers */
  };
  if (regno < 0
      || regno >= (sizeof (register_names) / sizeof (*register_names)))
    internal_error (__FILE__, __LINE__,
		    "h8300s_register_name: illegal register number %d",
		    regno);
  else
    return register_names[regno];
}

static const char *
h8300sx_register_name (int regno)
{
  static char *register_names[] = {
    "er0", "er1", "er2", "er3", "er4", "er5", "er6",
    "sp", "", "pc", "cycles", "", "tick", "inst",
    "mach", "macl", "sbr", "vbr",
    "ccr", "exr"		/* pseudo registers */
  };
  if (regno < 0
      || regno >= (sizeof (register_names) / sizeof (*register_names)))
    internal_error (__FILE__, __LINE__,
		    "h8300sx_register_name: illegal register number %d",
		    regno);
  else
    return register_names[regno];
}

static void
h8300_print_register (struct gdbarch *gdbarch, struct ui_file *file,
		      struct frame_info *frame, int regno)
{
  LONGEST rval;
  const char *name = gdbarch_register_name (gdbarch, regno);

  if (!name || !*name)
    return;

  rval = get_frame_register_signed (frame, regno);

  fprintf_filtered (file, "%-14s ", name);
  if ((regno == E_PSEUDO_CCR_REGNUM) || \
      (regno == E_PSEUDO_EXR_REGNUM && is_h8300smode (current_gdbarch)))
    {
      fprintf_filtered (file, "0x%02x        ", (unsigned char) rval);
      print_longest (file, 'u', 1, rval);
    }
  else
    {
      fprintf_filtered (file, "0x%s  ", phex ((ULONGEST) rval, BINWORD));
      print_longest (file, 'd', 1, rval);
    }
  if (regno == E_PSEUDO_CCR_REGNUM)
    {
      /* CCR register */
      int C, Z, N, V;
      unsigned char l = rval & 0xff;
      fprintf_filtered (file, "\t");
      fprintf_filtered (file, "I-%d ", (l & 0x80) != 0);
      fprintf_filtered (file, "UI-%d ", (l & 0x40) != 0);
      fprintf_filtered (file, "H-%d ", (l & 0x20) != 0);
      fprintf_filtered (file, "U-%d ", (l & 0x10) != 0);
      N = (l & 0x8) != 0;
      Z = (l & 0x4) != 0;
      V = (l & 0x2) != 0;
      C = (l & 0x1) != 0;
      fprintf_filtered (file, "N-%d ", N);
      fprintf_filtered (file, "Z-%d ", Z);
      fprintf_filtered (file, "V-%d ", V);
      fprintf_filtered (file, "C-%d ", C);
      if ((C | Z) == 0)
	fprintf_filtered (file, "u> ");
      if ((C | Z) == 1)
	fprintf_filtered (file, "u<= ");
      if ((C == 0))
	fprintf_filtered (file, "u>= ");
      if (C == 1)
	fprintf_filtered (file, "u< ");
      if (Z == 0)
	fprintf_filtered (file, "!= ");
      if (Z == 1)
	fprintf_filtered (file, "== ");
      if ((N ^ V) == 0)
	fprintf_filtered (file, ">= ");
      if ((N ^ V) == 1)
	fprintf_filtered (file, "< ");
      if ((Z | (N ^ V)) == 0)
	fprintf_filtered (file, "> ");
      if ((Z | (N ^ V)) == 1)
	fprintf_filtered (file, "<= ");
    }
  else if (regno == E_PSEUDO_EXR_REGNUM && is_h8300smode (current_gdbarch))
    {
      /* EXR register */
      unsigned char l = rval & 0xff;
      fprintf_filtered (file, "\t");
      fprintf_filtered (file, "T-%d - - - ", (l & 0x80) != 0);
      fprintf_filtered (file, "I2-%d ", (l & 4) != 0);
      fprintf_filtered (file, "I1-%d ", (l & 2) != 0);
      fprintf_filtered (file, "I0-%d", (l & 1) != 0);
    }
  fprintf_filtered (file, "\n");
}

static void
h8300_print_registers_info (struct gdbarch *gdbarch, struct ui_file *file,
			    struct frame_info *frame, int regno, int cpregs)
{
  if (regno < 0)
    {
      for (regno = E_R0_REGNUM; regno <= E_SP_REGNUM; ++regno)
	h8300_print_register (gdbarch, file, frame, regno);
      h8300_print_register (gdbarch, file, frame, E_PSEUDO_CCR_REGNUM);
      h8300_print_register (gdbarch, file, frame, E_PC_REGNUM);
      if (is_h8300smode (current_gdbarch))
	{
	  h8300_print_register (gdbarch, file, frame, E_PSEUDO_EXR_REGNUM);
	  if (is_h8300sxmode (current_gdbarch))
	    {
	      h8300_print_register (gdbarch, file, frame, E_SBR_REGNUM);
	      h8300_print_register (gdbarch, file, frame, E_VBR_REGNUM);
	    }
	  h8300_print_register (gdbarch, file, frame, E_MACH_REGNUM);
	  h8300_print_register (gdbarch, file, frame, E_MACL_REGNUM);
	  h8300_print_register (gdbarch, file, frame, E_CYCLES_REGNUM);
	  h8300_print_register (gdbarch, file, frame, E_TICKS_REGNUM);
	  h8300_print_register (gdbarch, file, frame, E_INSTS_REGNUM);
	}
      else
	{
	  h8300_print_register (gdbarch, file, frame, E_CYCLES_REGNUM);
	  h8300_print_register (gdbarch, file, frame, E_TICK_REGNUM);
	  h8300_print_register (gdbarch, file, frame, E_INST_REGNUM);
	}
    }
  else
    {
      if (regno == E_CCR_REGNUM)
	h8300_print_register (gdbarch, file, frame, E_PSEUDO_CCR_REGNUM);
      else if (regno == E_PSEUDO_EXR_REGNUM
	       && is_h8300smode (current_gdbarch))
	h8300_print_register (gdbarch, file, frame, E_PSEUDO_EXR_REGNUM);
      else
	h8300_print_register (gdbarch, file, frame, regno);
    }
}

static struct type *
h8300_register_type (struct gdbarch *gdbarch, int regno)
{
  if (regno < 0 || regno >= gdbarch_num_regs (current_gdbarch)
			    + gdbarch_num_pseudo_regs (current_gdbarch))
    internal_error (__FILE__, __LINE__,
		    "h8300_register_type: illegal register number %d", regno);
  else
    {
      switch (regno)
	{
	case E_PC_REGNUM:
	  return builtin_type_void_func_ptr;
	case E_SP_REGNUM:
	case E_FP_REGNUM:
	  return builtin_type_void_data_ptr;
	default:
	  if (regno == E_PSEUDO_CCR_REGNUM)
	    return builtin_type_uint8;
	  else if (regno == E_PSEUDO_EXR_REGNUM)
	    return builtin_type_uint8;
	  else if (is_h8300hmode (current_gdbarch))
	    return builtin_type_int32;
	  else
	    return builtin_type_int16;
	}
    }
}

static void
h8300_pseudo_register_read (struct gdbarch *gdbarch,
			    struct regcache *regcache, int regno,
			    gdb_byte *buf)
{
  if (regno == E_PSEUDO_CCR_REGNUM)
    regcache_raw_read (regcache, E_CCR_REGNUM, buf);
  else if (regno == E_PSEUDO_EXR_REGNUM)
    regcache_raw_read (regcache, E_EXR_REGNUM, buf);
  else
    regcache_raw_read (regcache, regno, buf);
}

static void
h8300_pseudo_register_write (struct gdbarch *gdbarch,
			     struct regcache *regcache, int regno,
			     const gdb_byte *buf)
{
  if (regno == E_PSEUDO_CCR_REGNUM)
    regcache_raw_write (regcache, E_CCR_REGNUM, buf);
  else if (regno == E_PSEUDO_EXR_REGNUM)
    regcache_raw_write (regcache, E_EXR_REGNUM, buf);
  else
    regcache_raw_write (regcache, regno, buf);
}

static int
h8300_dbg_reg_to_regnum (int regno)
{
  if (regno == E_CCR_REGNUM)
    return E_PSEUDO_CCR_REGNUM;
  return regno;
}

static int
h8300s_dbg_reg_to_regnum (int regno)
{
  if (regno == E_CCR_REGNUM)
    return E_PSEUDO_CCR_REGNUM;
  if (regno == E_EXR_REGNUM)
    return E_PSEUDO_EXR_REGNUM;
  return regno;
}

const static unsigned char *
h8300_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  /*static unsigned char breakpoint[] = { 0x7A, 0xFF }; *//* ??? */
  static unsigned char breakpoint[] = { 0x01, 0x80 };	/* Sleep */

  *lenptr = sizeof (breakpoint);
  return breakpoint;
}

static void
h8300_print_float_info (struct gdbarch *gdbarch, struct ui_file *file,
			struct frame_info *frame, const char *args)
{
  fprintf_filtered (file, "\
No floating-point info available for this processor.\n");
}

static struct gdbarch *
h8300_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch_tdep *tdep = NULL;
  struct gdbarch *gdbarch;

  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

#if 0
  tdep = (struct gdbarch_tdep *) xmalloc (sizeof (struct gdbarch_tdep));
#endif

  if (info.bfd_arch_info->arch != bfd_arch_h8300)
    return NULL;

  gdbarch = gdbarch_alloc (&info, 0);

  switch (info.bfd_arch_info->mach)
    {
    case bfd_mach_h8300:
      set_gdbarch_num_regs (gdbarch, 13);
      set_gdbarch_num_pseudo_regs (gdbarch, 1);
      set_gdbarch_ecoff_reg_to_regnum (gdbarch, h8300_dbg_reg_to_regnum);
      set_gdbarch_dwarf_reg_to_regnum (gdbarch, h8300_dbg_reg_to_regnum);
      set_gdbarch_dwarf2_reg_to_regnum (gdbarch, h8300_dbg_reg_to_regnum);
      set_gdbarch_stab_reg_to_regnum (gdbarch, h8300_dbg_reg_to_regnum);
      set_gdbarch_register_name (gdbarch, h8300_register_name);
      set_gdbarch_ptr_bit (gdbarch, 2 * TARGET_CHAR_BIT);
      set_gdbarch_addr_bit (gdbarch, 2 * TARGET_CHAR_BIT);
      set_gdbarch_return_value (gdbarch, h8300_return_value);
      set_gdbarch_print_insn (gdbarch, print_insn_h8300);
      break;
    case bfd_mach_h8300h:
    case bfd_mach_h8300hn:
      set_gdbarch_num_regs (gdbarch, 13);
      set_gdbarch_num_pseudo_regs (gdbarch, 1);
      set_gdbarch_ecoff_reg_to_regnum (gdbarch, h8300_dbg_reg_to_regnum);
      set_gdbarch_dwarf_reg_to_regnum (gdbarch, h8300_dbg_reg_to_regnum);
      set_gdbarch_dwarf2_reg_to_regnum (gdbarch, h8300_dbg_reg_to_regnum);
      set_gdbarch_stab_reg_to_regnum (gdbarch, h8300_dbg_reg_to_regnum);
      set_gdbarch_register_name (gdbarch, h8300_register_name);
      if (info.bfd_arch_info->mach != bfd_mach_h8300hn)
	{
	  set_gdbarch_ptr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
	  set_gdbarch_addr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
	}
      else
	{
	  set_gdbarch_ptr_bit (gdbarch, 2 * TARGET_CHAR_BIT);
	  set_gdbarch_addr_bit (gdbarch, 2 * TARGET_CHAR_BIT);
	}
      set_gdbarch_return_value (gdbarch, h8300h_return_value);
      set_gdbarch_print_insn (gdbarch, print_insn_h8300h);
      break;
    case bfd_mach_h8300s:
    case bfd_mach_h8300sn:
      set_gdbarch_num_regs (gdbarch, 16);
      set_gdbarch_num_pseudo_regs (gdbarch, 2);
      set_gdbarch_ecoff_reg_to_regnum (gdbarch, h8300s_dbg_reg_to_regnum);
      set_gdbarch_dwarf_reg_to_regnum (gdbarch, h8300s_dbg_reg_to_regnum);
      set_gdbarch_dwarf2_reg_to_regnum (gdbarch, h8300s_dbg_reg_to_regnum);
      set_gdbarch_stab_reg_to_regnum (gdbarch, h8300s_dbg_reg_to_regnum);
      set_gdbarch_register_name (gdbarch, h8300s_register_name);
      if (info.bfd_arch_info->mach != bfd_mach_h8300sn)
	{
	  set_gdbarch_ptr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
	  set_gdbarch_addr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
	}
      else
	{
	  set_gdbarch_ptr_bit (gdbarch, 2 * TARGET_CHAR_BIT);
	  set_gdbarch_addr_bit (gdbarch, 2 * TARGET_CHAR_BIT);
	}
      set_gdbarch_return_value (gdbarch, h8300h_return_value);
      set_gdbarch_print_insn (gdbarch, print_insn_h8300s);
      break;
    case bfd_mach_h8300sx:
    case bfd_mach_h8300sxn:
      set_gdbarch_num_regs (gdbarch, 18);
      set_gdbarch_num_pseudo_regs (gdbarch, 2);
      set_gdbarch_ecoff_reg_to_regnum (gdbarch, h8300s_dbg_reg_to_regnum);
      set_gdbarch_dwarf_reg_to_regnum (gdbarch, h8300s_dbg_reg_to_regnum);
      set_gdbarch_dwarf2_reg_to_regnum (gdbarch, h8300s_dbg_reg_to_regnum);
      set_gdbarch_stab_reg_to_regnum (gdbarch, h8300s_dbg_reg_to_regnum);
      set_gdbarch_register_name (gdbarch, h8300sx_register_name);
      if (info.bfd_arch_info->mach != bfd_mach_h8300sxn)
	{
	  set_gdbarch_ptr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
	  set_gdbarch_addr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
	}
      else
	{
	  set_gdbarch_ptr_bit (gdbarch, 2 * TARGET_CHAR_BIT);
	  set_gdbarch_addr_bit (gdbarch, 2 * TARGET_CHAR_BIT);
	}
      set_gdbarch_return_value (gdbarch, h8300h_return_value);
      set_gdbarch_print_insn (gdbarch, print_insn_h8300s);
      break;
    }

  set_gdbarch_pseudo_register_read (gdbarch, h8300_pseudo_register_read);
  set_gdbarch_pseudo_register_write (gdbarch, h8300_pseudo_register_write);

  /*
   * Basic register fields and methods.
   */

  set_gdbarch_sp_regnum (gdbarch, E_SP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, E_PC_REGNUM);
  set_gdbarch_register_type (gdbarch, h8300_register_type);
  set_gdbarch_print_registers_info (gdbarch, h8300_print_registers_info);
  set_gdbarch_print_float_info (gdbarch, h8300_print_float_info);

  /*
   * Frame Info
   */
  set_gdbarch_skip_prologue (gdbarch, h8300_skip_prologue);

  /* Frame unwinder.  */
  set_gdbarch_unwind_pc (gdbarch, h8300_unwind_pc);
  set_gdbarch_unwind_sp (gdbarch, h8300_unwind_sp);
  set_gdbarch_unwind_dummy_id (gdbarch, h8300_unwind_dummy_id);
  frame_base_set_default (gdbarch, &h8300_frame_base);

  /* 
   * Miscelany
   */
  /* Stack grows up. */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  set_gdbarch_breakpoint_from_pc (gdbarch, h8300_breakpoint_from_pc);
  set_gdbarch_push_dummy_call (gdbarch, h8300_push_dummy_call);

  set_gdbarch_char_signed (gdbarch, 0);
  set_gdbarch_int_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_long_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_long_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_double_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_double_bit (gdbarch, 4 * TARGET_CHAR_BIT);

  set_gdbarch_believe_pcc_promotion (gdbarch, 1);

  /* Hook in the DWARF CFI frame unwinder.  */
  frame_unwind_append_sniffer (gdbarch, dwarf2_frame_sniffer);
  frame_unwind_append_sniffer (gdbarch, h8300_frame_sniffer);

  return gdbarch;

}

extern initialize_file_ftype _initialize_h8300_tdep;	/* -Wmissing-prototypes */

void
_initialize_h8300_tdep (void)
{
  register_gdbarch_init (bfd_arch_h8300, h8300_gdbarch_init);
}

static int
is_h8300hmode (struct gdbarch *gdbarch)
{
  return gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sx
    || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sxn
    || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300s
    || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sn
    || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300h
    || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300hn;
}

static int
is_h8300smode (struct gdbarch *gdbarch)
{
  return gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sx
    || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sxn
    || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300s
    || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sn;
}

static int
is_h8300sxmode (struct gdbarch *gdbarch)
{
  return gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sx
    || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sxn;
}

static int
is_h8300_normal_mode (struct gdbarch *gdbarch)
{
  return gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sxn
    || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sn
    || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300hn;
}
