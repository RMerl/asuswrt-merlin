/* Target-dependent code for the Matsushita MN10300 for GDB, the GNU debugger.

   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007 Free Software Foundation, Inc.

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
#include "gdbtypes.h"
#include "regcache.h"
#include "gdb_string.h"
#include "gdb_assert.h"
#include "gdbcore.h"	/* for write_memory_unsigned_integer */
#include "value.h"
#include "gdbtypes.h"
#include "frame.h"
#include "frame-unwind.h"
#include "frame-base.h"
#include "trad-frame.h"
#include "symtab.h"
#include "dwarf2-frame.h"
#include "osabi.h"

#include "mn10300-tdep.h"

/* Forward decl.  */
extern struct trad_frame_cache *mn10300_frame_unwind_cache (struct frame_info*,
							    void **);

/* Compute the alignment required by a type.  */

static int
mn10300_type_align (struct type *type)
{
  int i, align = 1;

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_INT:
    case TYPE_CODE_ENUM:
    case TYPE_CODE_SET:
    case TYPE_CODE_RANGE:
    case TYPE_CODE_CHAR:
    case TYPE_CODE_BOOL:
    case TYPE_CODE_FLT:
    case TYPE_CODE_PTR:
    case TYPE_CODE_REF:
      return TYPE_LENGTH (type);

    case TYPE_CODE_COMPLEX:
      return TYPE_LENGTH (type) / 2;

    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      for (i = 0; i < TYPE_NFIELDS (type); i++)
	{
	  int falign = mn10300_type_align (TYPE_FIELD_TYPE (type, i));
	  while (align < falign)
	    align <<= 1;
	}
      return align;

    case TYPE_CODE_ARRAY:
      /* HACK!  Structures containing arrays, even small ones, are not
	 elligible for returning in registers.  */
      return 256;

    case TYPE_CODE_TYPEDEF:
      return mn10300_type_align (check_typedef (type));

    default:
      internal_error (__FILE__, __LINE__, _("bad switch"));
    }
}

/* Should call_function allocate stack space for a struct return?  */
static int
mn10300_use_struct_convention (struct type *type)
{
  /* Structures bigger than a pair of words can't be returned in
     registers.  */
  if (TYPE_LENGTH (type) > 8)
    return 1;

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      /* Structures with a single field are handled as the field
	 itself.  */
      if (TYPE_NFIELDS (type) == 1)
	return mn10300_use_struct_convention (TYPE_FIELD_TYPE (type, 0));

      /* Structures with word or double-word size are passed in memory, as
	 long as they require at least word alignment.  */
      if (mn10300_type_align (type) >= 4)
	return 0;

      return 1;

      /* Arrays are addressable, so they're never returned in
	 registers.  This condition can only hold when the array is
	 the only field of a struct or union.  */
    case TYPE_CODE_ARRAY:
      return 1;

    case TYPE_CODE_TYPEDEF:
      return mn10300_use_struct_convention (check_typedef (type));

    default:
      return 0;
    }
}

static void
mn10300_store_return_value (struct gdbarch *gdbarch, struct type *type,
			    struct regcache *regcache, const void *valbuf)
{
  int len = TYPE_LENGTH (type);
  int reg, regsz;
  
  if (TYPE_CODE (type) == TYPE_CODE_PTR)
    reg = 4;
  else
    reg = 0;

  regsz = register_size (gdbarch, reg);

  if (len <= regsz)
    regcache_raw_write_part (regcache, reg, 0, len, valbuf);
  else if (len <= 2 * regsz)
    {
      regcache_raw_write (regcache, reg, valbuf);
      gdb_assert (regsz == register_size (gdbarch, reg + 1));
      regcache_raw_write_part (regcache, reg+1, 0,
			       len - regsz, (char *) valbuf + regsz);
    }
  else
    internal_error (__FILE__, __LINE__,
		    _("Cannot store return value %d bytes long."), len);
}

static void
mn10300_extract_return_value (struct gdbarch *gdbarch, struct type *type,
			      struct regcache *regcache, void *valbuf)
{
  char buf[MAX_REGISTER_SIZE];
  int len = TYPE_LENGTH (type);
  int reg, regsz;

  if (TYPE_CODE (type) == TYPE_CODE_PTR)
    reg = 4;
  else
    reg = 0;

  regsz = register_size (gdbarch, reg);
  if (len <= regsz)
    {
      regcache_raw_read (regcache, reg, buf);
      memcpy (valbuf, buf, len);
    }
  else if (len <= 2 * regsz)
    {
      regcache_raw_read (regcache, reg, buf);
      memcpy (valbuf, buf, regsz);
      gdb_assert (regsz == register_size (gdbarch, reg + 1));
      regcache_raw_read (regcache, reg + 1, buf);
      memcpy ((char *) valbuf + regsz, buf, len - regsz);
    }
  else
    internal_error (__FILE__, __LINE__,
		    _("Cannot extract return value %d bytes long."), len);
}

/* Determine, for architecture GDBARCH, how a return value of TYPE
   should be returned.  If it is supposed to be returned in registers,
   and READBUF is non-zero, read the appropriate value from REGCACHE,
   and copy it into READBUF.  If WRITEBUF is non-zero, write the value
   from WRITEBUF into REGCACHE.  */

static enum return_value_convention
mn10300_return_value (struct gdbarch *gdbarch, struct type *type,
		      struct regcache *regcache, gdb_byte *readbuf,
		      const gdb_byte *writebuf)
{
  if (mn10300_use_struct_convention (type))
    return RETURN_VALUE_STRUCT_CONVENTION;

  if (readbuf)
    mn10300_extract_return_value (gdbarch, type, regcache, readbuf);
  if (writebuf)
    mn10300_store_return_value (gdbarch, type, regcache, writebuf);

  return RETURN_VALUE_REGISTER_CONVENTION;
}

static char *
register_name (int reg, char **regs, long sizeof_regs)
{
  if (reg < 0 || reg >= sizeof_regs / sizeof (regs[0]))
    return NULL;
  else
    return regs[reg];
}

static const char *
mn10300_generic_register_name (int reg)
{
  static char *regs[] =
  { "d0", "d1", "d2", "d3", "a0", "a1", "a2", "a3",
    "sp", "pc", "mdr", "psw", "lir", "lar", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "fp"
  };
  return register_name (reg, regs, sizeof regs);
}


static const char *
am33_register_name (int reg)
{
  static char *regs[] =
  { "d0", "d1", "d2", "d3", "a0", "a1", "a2", "a3",
    "sp", "pc", "mdr", "psw", "lir", "lar", "",
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "ssp", "msp", "usp", "mcrh", "mcrl", "mcvf", "", "", ""
  };
  return register_name (reg, regs, sizeof regs);
}

static const char *
am33_2_register_name (int reg)
{
  static char *regs[] =
  {
    "d0", "d1", "d2", "d3", "a0", "a1", "a2", "a3",
    "sp", "pc", "mdr", "psw", "lir", "lar", "mdrq", "r0",
    "r1", "r2", "r3", "r4", "r5", "r6", "r7", "ssp",
    "msp", "usp", "mcrh", "mcrl", "mcvf", "fpcr", "", "",
    "fs0", "fs1", "fs2", "fs3", "fs4", "fs5", "fs6", "fs7",
    "fs8", "fs9", "fs10", "fs11", "fs12", "fs13", "fs14", "fs15",
    "fs16", "fs17", "fs18", "fs19", "fs20", "fs21", "fs22", "fs23",
    "fs24", "fs25", "fs26", "fs27", "fs28", "fs29", "fs30", "fs31"
  };
  return register_name (reg, regs, sizeof regs);
}

static struct type *
mn10300_register_type (struct gdbarch *gdbarch, int reg)
{
  return builtin_type_int;
}

static CORE_ADDR
mn10300_read_pc (struct regcache *regcache)
{
  ULONGEST val;
  regcache_cooked_read_unsigned (regcache, E_PC_REGNUM, &val);
  return val;
}

static void
mn10300_write_pc (struct regcache *regcache, CORE_ADDR val)
{
  regcache_cooked_write_unsigned (regcache, E_PC_REGNUM, val);
}

/* The breakpoint instruction must be the same size as the smallest
   instruction in the instruction set.

   The Matsushita mn10x00 processors have single byte instructions
   so we need a single byte breakpoint.  Matsushita hasn't defined
   one, so we defined it ourselves.  */

const static unsigned char *
mn10300_breakpoint_from_pc (CORE_ADDR *bp_addr, int *bp_size)
{
  static char breakpoint[] = {0xff};
  *bp_size = 1;
  return breakpoint;
}

/* Set offsets of saved registers.
   This is a helper function for mn10300_analyze_prologue.  */

static void
set_reg_offsets (struct frame_info *fi, 
		  void **this_cache, 
		  int movm_args,
		  int fpregmask,
		  int stack_extra_size,
		  int frame_in_fp)
{
  struct trad_frame_cache *cache;
  int offset = 0;
  CORE_ADDR base;

  if (fi == NULL || this_cache == NULL)
    return;

  cache = mn10300_frame_unwind_cache (fi, this_cache);
  if (cache == NULL)
    return;

  if (frame_in_fp)
    {
      base = frame_unwind_register_unsigned (fi, E_A3_REGNUM);
    }
  else
    {
      base = frame_unwind_register_unsigned (fi, E_SP_REGNUM) + stack_extra_size;
    }

  trad_frame_set_this_base (cache, base);

  if (AM33_MODE == 2)
    {
      /* If bit N is set in fpregmask, fsN is saved on the stack.
	 The floating point registers are saved in ascending order.
	 For example:  fs16 <- Frame Pointer
	               fs17    Frame Pointer + 4 */
      if (fpregmask != 0)
	{
	  int i;
	  for (i = 0; i < 32; i++)
	    {
	      if (fpregmask & (1 << i))
		{
		  trad_frame_set_reg_addr (cache, E_FS0_REGNUM + i, base + offset);
		  offset += 4;
		}
	    }
	}
    }


  if (movm_args & movm_other_bit)
    {
      /* The `other' bit leaves a blank area of four bytes at the
         beginning of its block of saved registers, making it 32 bytes
         long in total.  */
      trad_frame_set_reg_addr (cache, E_LAR_REGNUM,    base + offset + 4);
      trad_frame_set_reg_addr (cache, E_LIR_REGNUM,    base + offset + 8);
      trad_frame_set_reg_addr (cache, E_MDR_REGNUM,    base + offset + 12);
      trad_frame_set_reg_addr (cache, E_A0_REGNUM + 1, base + offset + 16);
      trad_frame_set_reg_addr (cache, E_A0_REGNUM,     base + offset + 20);
      trad_frame_set_reg_addr (cache, E_D0_REGNUM + 1, base + offset + 24);
      trad_frame_set_reg_addr (cache, E_D0_REGNUM,     base + offset + 28);
      offset += 32;
    }

  if (movm_args & movm_a3_bit)
    {
      trad_frame_set_reg_addr (cache, E_A3_REGNUM, base + offset);
      offset += 4;
    }
  if (movm_args & movm_a2_bit)
    {
      trad_frame_set_reg_addr (cache, E_A2_REGNUM, base + offset);
      offset += 4;
    }
  if (movm_args & movm_d3_bit)
    {
      trad_frame_set_reg_addr (cache, E_D3_REGNUM, base + offset);
      offset += 4;
    }
  if (movm_args & movm_d2_bit)
    {
      trad_frame_set_reg_addr (cache, E_D2_REGNUM, base + offset);
      offset += 4;
    }
  if (AM33_MODE)
    {
      if (movm_args & movm_exother_bit)
        {
	  trad_frame_set_reg_addr (cache, E_MCVF_REGNUM, base + offset);
	  trad_frame_set_reg_addr (cache, E_MCRL_REGNUM, base + offset + 4);
	  trad_frame_set_reg_addr (cache, E_MCRH_REGNUM, base + offset + 8);
	  trad_frame_set_reg_addr (cache, E_MDRQ_REGNUM, base + offset + 12);
	  trad_frame_set_reg_addr (cache, E_E1_REGNUM,   base + offset + 16);
	  trad_frame_set_reg_addr (cache, E_E0_REGNUM,   base + offset + 20);
          offset += 24;
        }
      if (movm_args & movm_exreg1_bit)
        {
	  trad_frame_set_reg_addr (cache, E_E7_REGNUM, base + offset);
	  trad_frame_set_reg_addr (cache, E_E6_REGNUM, base + offset + 4);
	  trad_frame_set_reg_addr (cache, E_E5_REGNUM, base + offset + 8);
	  trad_frame_set_reg_addr (cache, E_E4_REGNUM, base + offset + 12);
          offset += 16;
        }
      if (movm_args & movm_exreg0_bit)
        {
	  trad_frame_set_reg_addr (cache, E_E3_REGNUM, base + offset);
	  trad_frame_set_reg_addr (cache, E_E2_REGNUM, base + offset + 4);
          offset += 8;
        }
    }
  /* The last (or first) thing on the stack will be the PC.  */
  trad_frame_set_reg_addr (cache, E_PC_REGNUM, base + offset);
  /* Save the SP in the 'traditional' way.  
     This will be the same location where the PC is saved.  */
  trad_frame_set_reg_value (cache, E_SP_REGNUM, base + offset);
}

/* The main purpose of this file is dealing with prologues to extract
   information about stack frames and saved registers.

   In gcc/config/mn13000/mn10300.c, the expand_prologue prologue
   function is pretty readable, and has a nice explanation of how the
   prologue is generated.  The prologues generated by that code will
   have the following form (NOTE: the current code doesn't handle all
   this!):

   + If this is an old-style varargs function, then its arguments
     need to be flushed back to the stack:
     
        mov d0,(4,sp)
        mov d1,(4,sp)

   + If we use any of the callee-saved registers, save them now.
     
        movm [some callee-saved registers],(sp)

   + If we have any floating-point registers to save:

     - Decrement the stack pointer to reserve space for the registers.
       If the function doesn't need a frame pointer, we may combine
       this with the adjustment that reserves space for the frame.

        add -SIZE, sp

     - Save the floating-point registers.  We have two possible
       strategies:

       . Save them at fixed offset from the SP:

        fmov fsN,(OFFSETN,sp)
        fmov fsM,(OFFSETM,sp)
        ...

       Note that, if OFFSETN happens to be zero, you'll get the
       different opcode: fmov fsN,(sp)

       . Or, set a0 to the start of the save area, and then use
       post-increment addressing to save the FP registers.

        mov sp, a0
        add SIZE, a0
        fmov fsN,(a0+)
        fmov fsM,(a0+)
        ...

   + If the function needs a frame pointer, we set it here.

        mov sp, a3

   + Now we reserve space for the stack frame proper.  This could be
     merged into the `add -SIZE, sp' instruction for FP saves up
     above, unless we needed to set the frame pointer in the previous
     step, or the frame is so large that allocating the whole thing at
     once would put the FP register save slots out of reach of the
     addressing mode (128 bytes).
      
        add -SIZE, sp        

   One day we might keep the stack pointer constant, that won't
   change the code for prologues, but it will make the frame
   pointerless case much more common.  */

/* Analyze the prologue to determine where registers are saved,
   the end of the prologue, etc etc.  Return the end of the prologue
   scanned.

   We store into FI (if non-null) several tidbits of information:

   * stack_size -- size of this stack frame.  Note that if we stop in
   certain parts of the prologue/epilogue we may claim the size of the
   current frame is zero.  This happens when the current frame has
   not been allocated yet or has already been deallocated.

   * fsr -- Addresses of registers saved in the stack by this frame.

   * status -- A (relatively) generic status indicator.  It's a bitmask
   with the following bits: 

   MY_FRAME_IN_SP: The base of the current frame is actually in
   the stack pointer.  This can happen for frame pointerless
   functions, or cases where we're stopped in the prologue/epilogue
   itself.  For these cases mn10300_analyze_prologue will need up
   update fi->frame before returning or analyzing the register
   save instructions.

   MY_FRAME_IN_FP: The base of the current frame is in the
   frame pointer register ($a3).

   NO_MORE_FRAMES: Set this if the current frame is "start" or
   if the first instruction looks like mov <imm>,sp.  This tells
   frame chain to not bother trying to unwind past this frame.  */

static CORE_ADDR
mn10300_analyze_prologue (struct frame_info *fi, 
			  void **this_cache, 
			  CORE_ADDR pc)
{
  CORE_ADDR func_addr, func_end, addr, stop;
  long stack_extra_size = 0;
  int imm_size;
  unsigned char buf[4];
  int status;
  int movm_args = 0;
  int fpregmask = 0;
  char *name;
  int frame_in_fp = 0;

  /* Use the PC in the frame if it's provided to look up the
     start of this function.

     Note: kevinb/2003-07-16: We used to do the following here:
	pc = (fi ? get_frame_pc (fi) : pc);
     But this is (now) badly broken when called from analyze_dummy_frame().
  */
  if (fi)
    {
      pc = (pc ? pc : get_frame_pc (fi));
    }

  /* Find the start of this function.  */
  status = find_pc_partial_function (pc, &name, &func_addr, &func_end);

  /* Do nothing if we couldn't find the start of this function 

     MVS: comment went on to say "or if we're stopped at the first
     instruction in the prologue" -- but code doesn't reflect that, 
     and I don't want to do that anyway.  */
  if (status == 0)
    {
      addr = pc;
      goto finish_prologue;
    }

  /* If we're in start, then give up.  */
  if (strcmp (name, "start") == 0)
    {
      addr = pc;
      goto finish_prologue;
    }

  /* Figure out where to stop scanning.  */
  stop = fi ? pc : func_end;

  /* Don't walk off the end of the function.  */
  stop = stop > func_end ? func_end : stop;

  /* Start scanning on the first instruction of this function.  */
  addr = func_addr;

  /* Suck in two bytes.  */
  if (addr + 2 > stop || !safe_frame_unwind_memory (fi, addr, buf, 2))
    goto finish_prologue;

  /* First see if this insn sets the stack pointer from a register; if
     so, it's probably the initialization of the stack pointer in _start,
     so mark this as the bottom-most frame.  */
  if (buf[0] == 0xf2 && (buf[1] & 0xf3) == 0xf0)
    {
      goto finish_prologue;
    }

  /* Now look for movm [regs],sp, which saves the callee saved registers.

     At this time we don't know if fi->frame is valid, so we only note
     that we encountered a movm instruction.  Later, we'll set the entries
     in fsr.regs as needed.  */
  if (buf[0] == 0xcf)
    {
      /* Extract the register list for the movm instruction.  */
      movm_args = buf[1];

      addr += 2;

      /* Quit now if we're beyond the stop point.  */
      if (addr >= stop)
	goto finish_prologue;

      /* Get the next two bytes so the prologue scan can continue.  */
      if (!safe_frame_unwind_memory (fi, addr, buf, 2))
	goto finish_prologue;
    }

  if (AM33_MODE == 2)
    {
      /* Determine if any floating point registers are to be saved.
	 Look for one of the following three prologue formats:

	[movm [regs],(sp)] [movm [regs],(sp)] [movm [regs],(sp)]

	 add -SIZE,sp       add -SIZE,sp       add -SIZE,sp
	 fmov fs#,(sp)      mov sp,a0/a1       mov sp,a0/a1
	 fmov fs#,(#,sp)    fmov fs#,(a0/a1+)  add SIZE2,a0/a1
	 ...                ...                fmov fs#,(a0/a1+)
	 ...                ...                ...
	 fmov fs#,(#,sp)    fmov fs#,(a0/a1+)  fmov fs#,(a0/a1+)

	[mov sp,a3]        [mov sp,a3]
	[add -SIZE2,sp]    [add -SIZE2,sp]                                 */

      /* Remember the address at which we started in the event that we
	 don't ultimately find an fmov instruction.  Once we're certain
	 that we matched one of the above patterns, we'll set
	 ``restore_addr'' to the appropriate value.  Note: At one time
	 in the past, this code attempted to not adjust ``addr'' until
	 there was a fair degree of certainty that the pattern would be
	 matched.  However, that code did not wait until an fmov instruction
	 was actually encountered.  As a consequence, ``addr'' would
	 sometimes be advanced even when no fmov instructions were found.  */
      CORE_ADDR restore_addr = addr;

      /* First, look for add -SIZE,sp (i.e. add imm8,sp  (0xf8feXX)
                                         or add imm16,sp (0xfafeXXXX)
                                         or add imm32,sp (0xfcfeXXXXXXXX)) */
      imm_size = 0;
      if (buf[0] == 0xf8 && buf[1] == 0xfe)
	imm_size = 1;
      else if (buf[0] == 0xfa && buf[1] == 0xfe)
	imm_size = 2;
      else if (buf[0] == 0xfc && buf[1] == 0xfe)
	imm_size = 4;
      if (imm_size != 0)
	{
	  /* An "add -#,sp" instruction has been found. "addr + 2 + imm_size"
	     is the address of the next instruction. Don't modify "addr" until
	     the next "floating point prologue" instruction is found. If this
	     is not a prologue that saves floating point registers we need to
	     be able to back out of this bit of code and continue with the
	     prologue analysis. */
	  if (addr + 2 + imm_size < stop)
	    {
	      if (!safe_frame_unwind_memory (fi, addr + 2 + imm_size, buf, 3))
		goto finish_prologue;
	      if ((buf[0] & 0xfc) == 0x3c)
		{
		  /* Occasionally, especially with C++ code, the "fmov"
		     instructions will be preceded by "mov sp,aN"
		     (aN => a0, a1, a2, or a3).

		     This is a one byte instruction:  mov sp,aN = 0011 11XX
		     where XX is the register number.

		     Skip this instruction by incrementing addr.  The "fmov"
		     instructions will have the form "fmov fs#,(aN+)" in this
		     case, but that will not necessitate a change in the
		     "fmov" parsing logic below. */

		  addr++;

		  if ((buf[1] & 0xfc) == 0x20)
		    {
		      /* Occasionally, especially with C++ code compiled with
			 the -fomit-frame-pointer or -O3 options, the
			 "mov sp,aN" instruction will be followed by an
			 "add #,aN" instruction. This indicates the
			 "stack_size", the size of the portion of the stack
			 containing the arguments. This instruction format is:
			 add #,aN = 0010 00XX YYYY YYYY
			 where XX        is the register number
			       YYYY YYYY is the constant.
			 Note the size of the stack (as a negative number) in
			 the frame info structure. */
		      if (fi)
			stack_extra_size += -buf[2];

		      addr += 2;
		    }
		}

	      if ((buf[0] & 0xfc) == 0x3c ||
		  buf[0] == 0xf9 || buf[0] == 0xfb)
		{
		  /* An "fmov" instruction has been found indicating that this
		     prologue saves floating point registers (or, as described
		     above, a "mov sp,aN" and possible "add #,aN" have been
		     found and we will assume an "fmov" follows). Process the
		     consecutive "fmov" instructions. */
		  for (addr += 2 + imm_size;;addr += imm_size)
		    {
		      int regnum;

		      /* Read the "fmov" instruction. */
		      if (addr >= stop ||
			  !safe_frame_unwind_memory (fi, addr, buf, 4))
			goto finish_prologue;

		      if (buf[0] != 0xf9 && buf[0] != 0xfb)
			break;

		      /* An fmov instruction has just been seen.  We can
		         now really commit to the pattern match.  Set the
			 address to restore at the end of this speculative
			 bit of code to the actually address that we've
			 been incrementing (or not) throughout the
			 speculation.  */
		      restore_addr = addr;

		      /* Get the floating point register number from the 
			 2nd and 3rd bytes of the "fmov" instruction:
			 Machine Code: 0000 00X0 YYYY 0000 =>
			 Regnum: 000X YYYY */
		      regnum = (buf[1] & 0x02) << 3;
		      regnum |= ((buf[2] & 0xf0) >> 4) & 0x0f;

		      /* Add this register number to the bit mask of floating
			 point registers that have been saved. */
		      fpregmask |= 1 << regnum;
		  
		      /* Determine the length of this "fmov" instruction.
			 fmov fs#,(sp)   => 3 byte instruction
			 fmov fs#,(#,sp) => 4 byte instruction */
		      imm_size = (buf[0] == 0xf9) ? 3 : 4;
		    }
		}
	      else
		{
		  /* No "fmov" was found. Reread the two bytes at the original
		     "addr" to reset the state. */
		  addr = restore_addr;
		  if (!safe_frame_unwind_memory (fi, addr, buf, 2))
		    goto finish_prologue;
		}
	    }
	  /* else the prologue consists entirely of an "add -SIZE,sp"
	     instruction. Handle this below. */
	}
      /* else no "add -SIZE,sp" was found indicating no floating point
	 registers are saved in this prologue.  */

      /* In the pattern match code contained within this block, `restore_addr'
	 is set to the starting address at the very beginning and then
	 iteratively to the next address to start scanning at once the
	 pattern match has succeeded.  Thus `restore_addr' will contain
	 the address to rewind to if the pattern match failed.  If the
	 match succeeded, `restore_addr' and `addr' will already have the
	 same value.  */
      addr = restore_addr;
    }

  /* Now see if we set up a frame pointer via "mov sp,a3" */
  if (buf[0] == 0x3f)
    {
      addr += 1;

      /* The frame pointer is now valid.  */
      if (fi)
	{
	  frame_in_fp = 1;
	}

      /* Quit now if we're beyond the stop point.  */
      if (addr >= stop)
	goto finish_prologue;

      /* Get two more bytes so scanning can continue.  */
      if (!safe_frame_unwind_memory (fi, addr, buf, 2))
	goto finish_prologue;
    }

  /* Next we should allocate the local frame.  No more prologue insns
     are found after allocating the local frame.

     Search for add imm8,sp (0xf8feXX)
     or add imm16,sp (0xfafeXXXX)
     or add imm32,sp (0xfcfeXXXXXXXX).

     If none of the above was found, then this prologue has no 
     additional stack.  */

  imm_size = 0;
  if (buf[0] == 0xf8 && buf[1] == 0xfe)
    imm_size = 1;
  else if (buf[0] == 0xfa && buf[1] == 0xfe)
    imm_size = 2;
  else if (buf[0] == 0xfc && buf[1] == 0xfe)
    imm_size = 4;

  if (imm_size != 0)
    {
      /* Suck in imm_size more bytes, they'll hold the size of the
         current frame.  */
      if (!safe_frame_unwind_memory (fi, addr + 2, buf, imm_size))
	goto finish_prologue;

      /* Note the size of the stack.  */
      stack_extra_size -= extract_signed_integer (buf, imm_size);

      /* We just consumed 2 + imm_size bytes.  */
      addr += 2 + imm_size;

      /* No more prologue insns follow, so begin preparation to return.  */
      goto finish_prologue;
    }
  /* Do the essentials and get out of here.  */
 finish_prologue:
  /* Note if/where callee saved registers were saved.  */
  if (fi)
    set_reg_offsets (fi, this_cache, movm_args, fpregmask, stack_extra_size, frame_in_fp);
  return addr;
}

/* Function: skip_prologue
   Return the address of the first inst past the prologue of the function.  */

static CORE_ADDR
mn10300_skip_prologue (CORE_ADDR pc)
{
  return mn10300_analyze_prologue (NULL, NULL, pc);
}

/* Simple frame_unwind_cache.  
   This finds the "extra info" for the frame.  */
struct trad_frame_cache *
mn10300_frame_unwind_cache (struct frame_info *next_frame,
			    void **this_prologue_cache)
{
  struct trad_frame_cache *cache;
  CORE_ADDR pc, start, end;

  if (*this_prologue_cache)
    return (*this_prologue_cache);

  cache = trad_frame_cache_zalloc (next_frame);
  pc = gdbarch_unwind_pc (current_gdbarch, next_frame);
  mn10300_analyze_prologue (next_frame, (void **) &cache, pc);
  if (find_pc_partial_function (pc, NULL, &start, &end))
    trad_frame_set_id (cache, 
		       frame_id_build (trad_frame_get_this_base (cache), 
				       start));
  else
    {
      start = frame_func_unwind (next_frame, NORMAL_FRAME);
      trad_frame_set_id (cache,
			 frame_id_build (trad_frame_get_this_base (cache),
					 start));
    }

  (*this_prologue_cache) = cache;
  return cache;
}

/* Here is a dummy implementation.  */
static struct frame_id
mn10300_unwind_dummy_id (struct gdbarch *gdbarch,
			 struct frame_info *next_frame)
{
  return frame_id_build (frame_sp_unwind (next_frame), 
			 frame_pc_unwind (next_frame));
}

/* Trad frame implementation.  */
static void
mn10300_frame_this_id (struct frame_info *next_frame,
		       void **this_prologue_cache,
		       struct frame_id *this_id)
{
  struct trad_frame_cache *cache = 
    mn10300_frame_unwind_cache (next_frame, this_prologue_cache);

  trad_frame_get_id (cache, this_id);
}

static void
mn10300_frame_prev_register (struct frame_info *next_frame,
			     void **this_prologue_cache,
			     int regnum, int *optimizedp,
			     enum lval_type *lvalp, CORE_ADDR *addrp,
			     int *realnump, gdb_byte *bufferp)
{
  struct trad_frame_cache *cache =
    mn10300_frame_unwind_cache (next_frame, this_prologue_cache);

  trad_frame_get_register (cache, next_frame, regnum, optimizedp, 
			   lvalp, addrp, realnump, bufferp);
  /* Or...
  trad_frame_get_prev_register (next_frame, cache->prev_regs, regnum, 
			   optimizedp, lvalp, addrp, realnump, bufferp);
  */
}

static const struct frame_unwind mn10300_frame_unwind = {
  NORMAL_FRAME,
  mn10300_frame_this_id, 
  mn10300_frame_prev_register
};

static CORE_ADDR
mn10300_frame_base_address (struct frame_info *next_frame,
			    void **this_prologue_cache)
{
  struct trad_frame_cache *cache = 
    mn10300_frame_unwind_cache (next_frame, this_prologue_cache);

  return trad_frame_get_this_base (cache);
}

static const struct frame_unwind *
mn10300_frame_sniffer (struct frame_info *next_frame)
{
  return &mn10300_frame_unwind;
}

static const struct frame_base mn10300_frame_base = {
  &mn10300_frame_unwind, 
  mn10300_frame_base_address, 
  mn10300_frame_base_address,
  mn10300_frame_base_address
};

static CORE_ADDR
mn10300_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  ULONGEST pc;

  frame_unwind_unsigned_register (next_frame, E_PC_REGNUM, &pc);
  return pc;
}

static CORE_ADDR
mn10300_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  ULONGEST sp;

  frame_unwind_unsigned_register (next_frame, E_SP_REGNUM, &sp);
  return sp;
}

static void
mn10300_frame_unwind_init (struct gdbarch *gdbarch)
{
  frame_unwind_append_sniffer (gdbarch, dwarf2_frame_sniffer);
  frame_unwind_append_sniffer (gdbarch, mn10300_frame_sniffer);
  frame_base_set_default (gdbarch, &mn10300_frame_base);
  set_gdbarch_unwind_dummy_id (gdbarch, mn10300_unwind_dummy_id);
  set_gdbarch_unwind_pc (gdbarch, mn10300_unwind_pc);
  set_gdbarch_unwind_sp (gdbarch, mn10300_unwind_sp);
}

/* Function: push_dummy_call
 *
 * Set up machine state for a target call, including
 * function arguments, stack, return address, etc.
 *
 */

static CORE_ADDR
mn10300_push_dummy_call (struct gdbarch *gdbarch, 
			 struct value *target_func,
			 struct regcache *regcache,
			 CORE_ADDR bp_addr, 
			 int nargs, struct value **args,
			 CORE_ADDR sp, 
			 int struct_return,
			 CORE_ADDR struct_addr)
{
  const int push_size = register_size (gdbarch, E_PC_REGNUM);
  int regs_used;
  int len, arg_len; 
  int stack_offset = 0;
  int argnum;
  char *val, valbuf[MAX_REGISTER_SIZE];

  /* This should be a nop, but align the stack just in case something
     went wrong.  Stacks are four byte aligned on the mn10300.  */
  sp &= ~3;

  /* Now make space on the stack for the args.

     XXX This doesn't appear to handle pass-by-invisible reference
     arguments.  */
  regs_used = struct_return ? 1 : 0;
  for (len = 0, argnum = 0; argnum < nargs; argnum++)
    {
      arg_len = (TYPE_LENGTH (value_type (args[argnum])) + 3) & ~3;
      while (regs_used < 2 && arg_len > 0)
	{
	  regs_used++;
	  arg_len -= push_size;
	}
      len += arg_len;
    }

  /* Allocate stack space.  */
  sp -= len;

  if (struct_return)
    {
      regs_used = 1;
      regcache_cooked_write_unsigned (regcache, E_D0_REGNUM, struct_addr);
    }
  else
    regs_used = 0;

  /* Push all arguments onto the stack. */
  for (argnum = 0; argnum < nargs; argnum++)
    {
      /* FIXME what about structs?  Unions?  */
      if (TYPE_CODE (value_type (*args)) == TYPE_CODE_STRUCT
	  && TYPE_LENGTH (value_type (*args)) > 8)
	{
	  /* Change to pointer-to-type.  */
	  arg_len = push_size;
	  store_unsigned_integer (valbuf, push_size, 
				  VALUE_ADDRESS (*args));
	  val = &valbuf[0];
	}
      else
	{
	  arg_len = TYPE_LENGTH (value_type (*args));
	  val = (char *) value_contents (*args);
	}

      while (regs_used < 2 && arg_len > 0)
	{
	  regcache_cooked_write_unsigned (regcache, regs_used, 
				  extract_unsigned_integer (val, push_size));
	  val += push_size;
	  arg_len -= push_size;
	  regs_used++;
	}

      while (arg_len > 0)
	{
	  write_memory (sp + stack_offset, val, push_size);
	  arg_len -= push_size;
	  val += push_size;
	  stack_offset += push_size;
	}

      args++;
    }

  /* Make space for the flushback area.  */
  sp -= 8;

  /* Push the return address that contains the magic breakpoint.  */
  sp -= 4;
  write_memory_unsigned_integer (sp, push_size, bp_addr);

  /* The CPU also writes the return address always into the
     MDR register on "call".  */
  regcache_cooked_write_unsigned (regcache, E_MDR_REGNUM, bp_addr);

  /* Update $sp.  */
  regcache_cooked_write_unsigned (regcache, E_SP_REGNUM, sp);
  return sp;
}

/* If DWARF2 is a register number appearing in Dwarf2 debug info, then
   mn10300_dwarf2_reg_to_regnum (DWARF2) is the corresponding GDB
   register number.  Why don't Dwarf2 and GDB use the same numbering?
   Who knows?  But since people have object files lying around with
   the existing Dwarf2 numbering, and other people have written stubs
   to work with the existing GDB, neither of them can change.  So we
   just have to cope.  */
static int
mn10300_dwarf2_reg_to_regnum (int dwarf2)
{
  /* This table is supposed to be shaped like the gdbarch_register_name
     initializer in gcc/config/mn10300/mn10300.h.  Registers which
     appear in GCC's numbering, but have no counterpart in GDB's
     world, are marked with a -1.  */
  static int dwarf2_to_gdb[] = {
    0,  1,  2,  3,  4,  5,  6,  7, -1, 8,
    15, 16, 17, 18, 19, 20, 21, 22,
    32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 58, 59, 60, 61, 62, 63
  };

  if (dwarf2 < 0
      || dwarf2 >= ARRAY_SIZE (dwarf2_to_gdb)
      || dwarf2_to_gdb[dwarf2] == -1)
    {
      warning (_("Bogus register number in debug info: %d"), dwarf2);
      return 0;
    }

  return dwarf2_to_gdb[dwarf2];
}

static struct gdbarch *
mn10300_gdbarch_init (struct gdbarch_info info,
		      struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  struct gdbarch_tdep *tdep;
  int num_regs;

  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  tdep = xmalloc (sizeof (struct gdbarch_tdep));
  gdbarch = gdbarch_alloc (&info, tdep);

  switch (info.bfd_arch_info->mach)
    {
    case 0:
    case bfd_mach_mn10300:
      set_gdbarch_register_name (gdbarch, mn10300_generic_register_name);
      tdep->am33_mode = 0;
      num_regs = 32;
      break;
    case bfd_mach_am33:
      set_gdbarch_register_name (gdbarch, am33_register_name);
      tdep->am33_mode = 1;
      num_regs = 32;
      break;
    case bfd_mach_am33_2:
      set_gdbarch_register_name (gdbarch, am33_2_register_name);
      tdep->am33_mode = 2;
      num_regs = 64;
      set_gdbarch_fp0_regnum (gdbarch, 32);
      break;
    default:
      internal_error (__FILE__, __LINE__,
		      _("mn10300_gdbarch_init: Unknown mn10300 variant"));
      break;
    }

  /* Registers.  */
  set_gdbarch_num_regs (gdbarch, num_regs);
  set_gdbarch_register_type (gdbarch, mn10300_register_type);
  set_gdbarch_skip_prologue (gdbarch, mn10300_skip_prologue);
  set_gdbarch_read_pc (gdbarch, mn10300_read_pc);
  set_gdbarch_write_pc (gdbarch, mn10300_write_pc);
  set_gdbarch_pc_regnum (gdbarch, E_PC_REGNUM);
  set_gdbarch_sp_regnum (gdbarch, E_SP_REGNUM);
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, mn10300_dwarf2_reg_to_regnum);

  /* Stack unwinding.  */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  /* Breakpoints.  */
  set_gdbarch_breakpoint_from_pc (gdbarch, mn10300_breakpoint_from_pc);
  /* decr_pc_after_break? */
  /* Disassembly.  */
  set_gdbarch_print_insn (gdbarch, print_insn_mn10300);

  /* Stage 2 */
  set_gdbarch_return_value (gdbarch, mn10300_return_value);
  
  /* Stage 3 -- get target calls working.  */
  set_gdbarch_push_dummy_call (gdbarch, mn10300_push_dummy_call);
  /* set_gdbarch_return_value (store, extract) */


  mn10300_frame_unwind_init (gdbarch);

  /* Hook in ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch);

  return gdbarch;
}
 
/* Dump out the mn10300 specific architecture information. */

static void
mn10300_dump_tdep (struct gdbarch *current_gdbarch, struct ui_file *file)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  fprintf_unfiltered (file, "mn10300_dump_tdep: am33_mode = %d\n",
		      tdep->am33_mode);
}

void
_initialize_mn10300_tdep (void)
{
  gdbarch_register (bfd_arch_mn10300, mn10300_gdbarch_init, mn10300_dump_tdep);
}

