/* Target-dependent code for the Motorola 88000 series.

   Copyright (C) 2004, 2005, 2007 Free Software Foundation, Inc.

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
#include "frame.h"
#include "frame-base.h"
#include "frame-unwind.h"
#include "gdbcore.h"
#include "gdbtypes.h"
#include "regcache.h"
#include "regset.h"
#include "symtab.h"
#include "trad-frame.h"
#include "value.h"

#include "gdb_assert.h"
#include "gdb_string.h"

#include "m88k-tdep.h"

/* Fetch the instruction at PC.  */

static unsigned long
m88k_fetch_instruction (CORE_ADDR pc)
{
  return read_memory_unsigned_integer (pc, 4);
}

/* Register information.  */

/* Return the name of register REGNUM.  */

static const char *
m88k_register_name (int regnum)
{
  static char *register_names[] =
  {
    "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
    "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",
    "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
    "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
    "epsr", "fpsr", "fpcr", "sxip", "snip", "sfip"
  };

  if (regnum >= 0 && regnum < ARRAY_SIZE (register_names))
    return register_names[regnum];

  return NULL;
}

/* Return the GDB type object for the "standard" data type of data in
   register REGNUM. */

static struct type *
m88k_register_type (struct gdbarch *gdbarch, int regnum)
{
  /* SXIP, SNIP, SFIP and R1 contain code addresses.  */
  if ((regnum >= M88K_SXIP_REGNUM && regnum <= M88K_SFIP_REGNUM)
      || regnum == M88K_R1_REGNUM)
    return builtin_type_void_func_ptr;

  /* R30 and R31 typically contains data addresses.  */
  if (regnum == M88K_R30_REGNUM || regnum == M88K_R31_REGNUM)
    return builtin_type_void_data_ptr;

  return builtin_type_int32;
}


static CORE_ADDR
m88k_addr_bits_remove (CORE_ADDR addr)
{
  /* All instructures are 4-byte aligned.  The lower 2 bits of SXIP,
     SNIP and SFIP are used for special purposes: bit 0 is the
     exception bit and bit 1 is the valid bit.  */
  return addr & ~0x3;
}

/* Use the program counter to determine the contents and size of a
   breakpoint instruction.  Return a pointer to a string of bytes that
   encode a breakpoint instruction, store the length of the string in
   *LEN and optionally adjust *PC to point to the correct memory
   location for inserting the breakpoint.  */
   
static const gdb_byte *
m88k_breakpoint_from_pc (CORE_ADDR *pc, int *len)
{
  /* tb 0,r0,511 */
  static gdb_byte break_insn[] = { 0xf0, 0x00, 0xd1, 0xff };

  *len = sizeof (break_insn);
  return break_insn;
}

static CORE_ADDR
m88k_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  CORE_ADDR pc;

  pc = frame_unwind_register_unsigned (next_frame, M88K_SXIP_REGNUM);
  return m88k_addr_bits_remove (pc);
}

static void
m88k_write_pc (struct regcache *regcache, CORE_ADDR pc)
{
  /* According to the MC88100 RISC Microprocessor User's Manual,
     section 6.4.3.1.2:

     "... can be made to return to a particular instruction by placing
     a valid instruction address in the SNIP and the next sequential
     instruction address in the SFIP (with V bits set and E bits
     clear).  The rte resumes execution at the instruction pointed to
     by the SNIP, then the SFIP."

     The E bit is the least significant bit (bit 0).  The V (valid)
     bit is bit 1.  This is why we logical or 2 into the values we are
     writing below.  It turns out that SXIP plays no role when
     returning from an exception so nothing special has to be done
     with it.  We could even (presumably) give it a totally bogus
     value.  */

  regcache_cooked_write_unsigned (regcache, M88K_SXIP_REGNUM, pc);
  regcache_cooked_write_unsigned (regcache, M88K_SNIP_REGNUM, pc | 2);
  regcache_cooked_write_unsigned (regcache, M88K_SFIP_REGNUM, (pc + 4) | 2);
}


/* The functions on this page are intended to be used to classify
   function arguments.  */

/* Check whether TYPE is "Integral or Pointer".  */

static int
m88k_integral_or_pointer_p (const struct type *type)
{
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_INT:
    case TYPE_CODE_BOOL:
    case TYPE_CODE_CHAR:
    case TYPE_CODE_ENUM:
    case TYPE_CODE_RANGE:
      {
	/* We have byte, half-word, word and extended-word/doubleword
           integral types.  */
	int len = TYPE_LENGTH (type);
	return (len == 1 || len == 2 || len == 4 || len == 8);
      }
      return 1;
    case TYPE_CODE_PTR:
    case TYPE_CODE_REF:
      {
	/* Allow only 32-bit pointers.  */
	return (TYPE_LENGTH (type) == 4);
      }
      return 1;
    default:
      break;
    }

  return 0;
}

/* Check whether TYPE is "Floating".  */

static int
m88k_floating_p (const struct type *type)
{
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_FLT:
      {
	int len = TYPE_LENGTH (type);
	return (len == 4 || len == 8);
      }
    default:
      break;
    }

  return 0;
}

/* Check whether TYPE is "Structure or Union".  */

static int
m88k_structure_or_union_p (const struct type *type)
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

/* Check whether TYPE has 8-byte alignment.  */

static int
m88k_8_byte_align_p (struct type *type)
{
  if (m88k_structure_or_union_p (type))
    {
      int i;

      for (i = 0; i < TYPE_NFIELDS (type); i++)
	{
	  struct type *subtype = check_typedef (TYPE_FIELD_TYPE (type, i));

	  if (m88k_8_byte_align_p (subtype))
	    return 1;
	}
    }

  if (m88k_integral_or_pointer_p (type) || m88k_floating_p (type))
    return (TYPE_LENGTH (type) == 8);

  return 0;
}

/* Check whether TYPE can be passed in a register.  */

static int
m88k_in_register_p (struct type *type)
{
  if (m88k_integral_or_pointer_p (type) || m88k_floating_p (type))
    return 1;

  if (m88k_structure_or_union_p (type) && TYPE_LENGTH (type) == 4)
    return 1;

  return 0;
}

static CORE_ADDR
m88k_store_arguments (struct regcache *regcache, int nargs,
		      struct value **args, CORE_ADDR sp)
{
  int num_register_words = 0;
  int num_stack_words = 0;
  int i;

  for (i = 0; i < nargs; i++)
    {
      struct type *type = value_type (args[i]);
      int len = TYPE_LENGTH (type);

      if (m88k_integral_or_pointer_p (type) && len < 4)
	{
	  args[i] = value_cast (builtin_type_int32, args[i]);
	  type = value_type (args[i]);
	  len = TYPE_LENGTH (type);
	}

      if (m88k_in_register_p (type))
	{
	  int num_words = 0;

	  if (num_register_words % 2 == 1 && m88k_8_byte_align_p (type))
	    num_words++;

	  num_words += ((len + 3) / 4);
	  if (num_register_words + num_words <= 8)
	    {
	      num_register_words += num_words;
	      continue;
	    }

	  /* We've run out of available registers.  Pass the argument
             on the stack.  */
	}

      if (num_stack_words % 2 == 1 && m88k_8_byte_align_p (type))
	num_stack_words++;

      num_stack_words += ((len + 3) / 4);
    }

  /* Allocate stack space.  */
  sp = align_down (sp - 32 - num_stack_words * 4, 16);
  num_stack_words = num_register_words = 0;

  for (i = 0; i < nargs; i++)
    {
      const bfd_byte *valbuf = value_contents (args[i]);
      struct type *type = value_type (args[i]);
      int len = TYPE_LENGTH (type);
      int stack_word = num_stack_words;

      if (m88k_in_register_p (type))
	{
	  int register_word = num_register_words;

	  if (register_word % 2 == 1 && m88k_8_byte_align_p (type))
	    register_word++;

	  gdb_assert (len == 4 || len == 8);

	  if (register_word + len / 8 < 8)
	    {
	      int regnum = M88K_R2_REGNUM + register_word;

	      regcache_raw_write (regcache, regnum, valbuf);
	      if (len > 4)
		regcache_raw_write (regcache, regnum + 1, valbuf + 4);

	      num_register_words = (register_word + len / 4);
	      continue;
	    }
	}

      if (stack_word % 2 == -1 && m88k_8_byte_align_p (type))
	stack_word++;

      write_memory (sp + stack_word * 4, valbuf, len);
      num_stack_words = (stack_word + (len + 3) / 4);
    }

  return sp;
}

static CORE_ADDR
m88k_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		      struct regcache *regcache, CORE_ADDR bp_addr, int nargs,
		      struct value **args, CORE_ADDR sp, int struct_return,
		      CORE_ADDR struct_addr)
{
  /* Set up the function arguments.  */
  sp = m88k_store_arguments (regcache, nargs, args, sp);
  gdb_assert (sp % 16 == 0);

  /* Store return value address.  */
  if (struct_return)
    regcache_raw_write_unsigned (regcache, M88K_R12_REGNUM, struct_addr);

  /* Store the stack pointer and return address in the appropriate
     registers.  */
  regcache_raw_write_unsigned (regcache, M88K_R31_REGNUM, sp);
  regcache_raw_write_unsigned (regcache, M88K_R1_REGNUM, bp_addr);

  /* Return the stack pointer.  */
  return sp;
}

static struct frame_id
m88k_unwind_dummy_id (struct gdbarch *arch, struct frame_info *next_frame)
{
  CORE_ADDR sp;

  sp = frame_unwind_register_unsigned (next_frame, M88K_R31_REGNUM);
  return frame_id_build (sp, frame_pc_unwind (next_frame));
}


/* Determine, for architecture GDBARCH, how a return value of TYPE
   should be returned.  If it is supposed to be returned in registers,
   and READBUF is non-zero, read the appropriate value from REGCACHE,
   and copy it into READBUF.  If WRITEBUF is non-zero, write the value
   from WRITEBUF into REGCACHE.  */

static enum return_value_convention
m88k_return_value (struct gdbarch *gdbarch, struct type *type,
		   struct regcache *regcache, gdb_byte *readbuf,
		   const gdb_byte *writebuf)
{
  int len = TYPE_LENGTH (type);
  gdb_byte buf[8];

  if (!m88k_integral_or_pointer_p (type) && !m88k_floating_p (type))
    return RETURN_VALUE_STRUCT_CONVENTION;

  if (readbuf)
    {
      /* Read the contents of R2 and (if necessary) R3.  */
      regcache_cooked_read (regcache, M88K_R2_REGNUM, buf);
      if (len > 4)
	{
	  regcache_cooked_read (regcache, M88K_R3_REGNUM, buf + 4);
	  gdb_assert (len == 8);
	  memcpy (readbuf, buf, len);
	}
      else
	{
	  /* Just stripping off any unused bytes should preserve the
             signed-ness just fine.  */
	  memcpy (readbuf, buf + 4 - len, len);
	}
    }

  if (writebuf)
    {
      /* Read the contents to R2 and (if necessary) R3.  */
      if (len > 4)
	{
	  gdb_assert (len == 8);
	  memcpy (buf, writebuf, 8);
	  regcache_cooked_write (regcache, M88K_R3_REGNUM, buf + 4);
	}
      else
	{
	  /* ??? Do we need to do any sign-extension here?  */
	  memcpy (buf + 4 - len, writebuf, len);
	}
      regcache_cooked_write (regcache, M88K_R2_REGNUM, buf);
    }

  return RETURN_VALUE_REGISTER_CONVENTION;
}

/* Default frame unwinder.  */

struct m88k_frame_cache
{
  /* Base address.  */
  CORE_ADDR base;
  CORE_ADDR pc;

  int sp_offset;
  int fp_offset;

  /* Table of saved registers.  */
  struct trad_frame_saved_reg *saved_regs;
};

/* Prologue analysis.  */

/* Macros for extracting fields from instructions.  */

#define BITMASK(pos, width) (((0x1 << (width)) - 1) << (pos))
#define EXTRACT_FIELD(val, pos, width) ((val) >> (pos) & BITMASK (0, width))
#define	SUBU_OFFSET(x)	((unsigned)(x & 0xFFFF))
#define	ST_OFFSET(x)	((unsigned)((x) & 0xFFFF))
#define	ST_SRC(x)	EXTRACT_FIELD ((x), 21, 5)
#define	ADDU_OFFSET(x)	((unsigned)(x & 0xFFFF))

/* Possible actions to be taken by the prologue analyzer for the
   instructions it encounters.  */

enum m88k_prologue_insn_action
{
  M88K_PIA_SKIP,		/* Ignore.  */
  M88K_PIA_NOTE_ST,		/* Note register store.  */
  M88K_PIA_NOTE_STD,		/* Note register pair store.  */
  M88K_PIA_NOTE_SP_ADJUSTMENT,	/* Note stack pointer adjustment.  */
  M88K_PIA_NOTE_FP_ASSIGNMENT,	/* Note frame pointer assignment.  */
  M88K_PIA_NOTE_BRANCH,		/* Note branch.  */
  M88K_PIA_NOTE_PROLOGUE_END	/* Note end of prologue.  */
};

/* Table of instructions that may comprise a function prologue.  */

struct m88k_prologue_insn
{
  unsigned long insn;
  unsigned long mask;
  enum m88k_prologue_insn_action action;
};

struct m88k_prologue_insn m88k_prologue_insn_table[] =
{
  /* Various register move instructions.  */
  { 0x58000000, 0xf800ffff, M88K_PIA_SKIP },     /* or/or.u with immed of 0 */
  { 0xf4005800, 0xfc1fffe0, M88K_PIA_SKIP },     /* or rd,r0,rs */
  { 0xf4005800, 0xfc00ffff, M88K_PIA_SKIP },     /* or rd,rs,r0 */

  /* Various other instructions.  */
  { 0x58000000, 0xf8000000, M88K_PIA_SKIP },     /* or/or.u */

  /* Stack pointer setup: "subu sp,sp,n" where n is a multiple of 8.  */
  { 0x67ff0000, 0xffff0007, M88K_PIA_NOTE_SP_ADJUSTMENT },

  /* Frame pointer assignment: "addu r30,r31,n".  */
  { 0x63df0000, 0xffff0000, M88K_PIA_NOTE_FP_ASSIGNMENT },

  /* Store to stack instructions; either "st rx,sp,n" or "st.d rx,sp,n".  */
  { 0x241f0000, 0xfc1f0000, M88K_PIA_NOTE_ST },  /* st rx,sp,n */
  { 0x201f0000, 0xfc1f0000, M88K_PIA_NOTE_STD }, /* st.d rs,sp,n */

  /* Instructions needed for setting up r25 for pic code.  */
  { 0x5f200000, 0xffff0000, M88K_PIA_SKIP },     /* or.u r25,r0,offset_high */
  { 0xcc000002, 0xffffffff, M88K_PIA_SKIP },     /* bsr.n Lab */
  { 0x5b390000, 0xffff0000, M88K_PIA_SKIP },     /* or r25,r25,offset_low */
  { 0xf7396001, 0xffffffff, M88K_PIA_SKIP },     /* Lab: addu r25,r25,r1 */

  /* Various branch or jump instructions which have a delay slot --
     these do not form part of the prologue, but the instruction in
     the delay slot might be a store instruction which should be
     noted.  */
  { 0xc4000000, 0xe4000000, M88K_PIA_NOTE_BRANCH },
                                      /* br.n, bsr.n, bb0.n, or bb1.n */
  { 0xec000000, 0xfc000000, M88K_PIA_NOTE_BRANCH }, /* bcnd.n */
  { 0xf400c400, 0xfffff7e0, M88K_PIA_NOTE_BRANCH }, /* jmp.n or jsr.n */

  /* Catch all.  Ends prologue analysis.  */
  { 0x00000000, 0x00000000, M88K_PIA_NOTE_PROLOGUE_END }
};

/* Do a full analysis of the function prologue at PC and update CACHE
   accordingly.  Bail out early if LIMIT is reached.  Return the
   address where the analysis stopped.  If LIMIT points beyond the
   function prologue, the return address should be the end of the
   prologue.  */

static CORE_ADDR
m88k_analyze_prologue (CORE_ADDR pc, CORE_ADDR limit,
		       struct m88k_frame_cache *cache)
{
  CORE_ADDR end = limit;

  /* Provide a dummy cache if necessary.  */
  if (cache == NULL)
    {
      size_t sizeof_saved_regs =
	(M88K_R31_REGNUM + 1) * sizeof (struct trad_frame_saved_reg);

      cache = alloca (sizeof (struct m88k_frame_cache));
      cache->saved_regs = alloca (sizeof_saved_regs);

      /* We only initialize the members we care about.  */
      cache->saved_regs[M88K_R1_REGNUM].addr = -1;
      cache->fp_offset = -1;
    }

  while (pc < limit)
    {
      struct m88k_prologue_insn *pi = m88k_prologue_insn_table;
      unsigned long insn = m88k_fetch_instruction (pc);

      while ((insn & pi->mask) != pi->insn)
	pi++;

      switch (pi->action)
	{
	case M88K_PIA_SKIP:
	  /* If we have a frame pointer, and R1 has been saved,
             consider this instruction as not being part of the
             prologue.  */
	  if (cache->fp_offset != -1
	      && cache->saved_regs[M88K_R1_REGNUM].addr != -1)
	    return min (pc, end);
	  break;

	case M88K_PIA_NOTE_ST:
	case M88K_PIA_NOTE_STD:
	  /* If no frame has been allocated, the stores aren't part of
             the prologue.  */
	  if (cache->sp_offset == 0)
	    return min (pc, end);

	  /* Record location of saved registers.  */
	  {
	    int regnum = ST_SRC (insn) + M88K_R0_REGNUM;
	    ULONGEST offset = ST_OFFSET (insn);

	    cache->saved_regs[regnum].addr = offset;
	    if (pi->action == M88K_PIA_NOTE_STD && regnum < M88K_R31_REGNUM)
	      cache->saved_regs[regnum + 1].addr = offset + 4;
	  }
	  break;

	case M88K_PIA_NOTE_SP_ADJUSTMENT:
	  /* A second stack pointer adjustment isn't part of the
             prologue.  */
	  if (cache->sp_offset != 0)
	    return min (pc, end);

	  /* Store stack pointer adjustment.  */
	  cache->sp_offset = -SUBU_OFFSET (insn);
	  break;

	case M88K_PIA_NOTE_FP_ASSIGNMENT:
	  /* A second frame pointer assignment isn't part of the
             prologue.  */
	  if (cache->fp_offset != -1)
	    return min (pc, end);

	  /* Record frame pointer assignment.  */
	  cache->fp_offset = ADDU_OFFSET (insn);
	  break;

	case M88K_PIA_NOTE_BRANCH:
	  /* The branch instruction isn't part of the prologue, but
             the instruction in the delay slot might be.  Limit the
             prologue analysis to the delay slot and record the branch
             instruction as the end of the prologue.  */
	  limit = min (limit, pc + 2 * M88K_INSN_SIZE);
	  end = pc;
	  break;

	case M88K_PIA_NOTE_PROLOGUE_END:
	  return min (pc, end);
	}

      pc += M88K_INSN_SIZE;
    }

  return end;
}

/* An upper limit to the size of the prologue.  */
const int m88k_max_prologue_size = 128 * M88K_INSN_SIZE;

/* Return the address of first real instruction of the function
   starting at PC.  */

static CORE_ADDR
m88k_skip_prologue (CORE_ADDR pc)
{
  struct symtab_and_line sal;
  CORE_ADDR func_start, func_end;

  /* This is the preferred method, find the end of the prologue by
     using the debugging information.  */
  if (find_pc_partial_function (pc, NULL, &func_start, &func_end))
    {
      sal = find_pc_line (func_start, 0);

      if (sal.end < func_end && pc <= sal.end)
	return sal.end;
    }

  return m88k_analyze_prologue (pc, pc + m88k_max_prologue_size, NULL);
}

struct m88k_frame_cache *
m88k_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct m88k_frame_cache *cache;
  CORE_ADDR frame_sp;

  if (*this_cache)
    return *this_cache;

  cache = FRAME_OBSTACK_ZALLOC (struct m88k_frame_cache);
  cache->saved_regs = trad_frame_alloc_saved_regs (next_frame);
  cache->fp_offset = -1;

  cache->pc = frame_func_unwind (next_frame, NORMAL_FRAME);
  if (cache->pc != 0)
    m88k_analyze_prologue (cache->pc, frame_pc_unwind (next_frame), cache);

  /* Calculate the stack pointer used in the prologue.  */
  if (cache->fp_offset != -1)
    {
      CORE_ADDR fp;

      fp = frame_unwind_register_unsigned (next_frame, M88K_R30_REGNUM);
      frame_sp = fp - cache->fp_offset;
    }
  else
    {
      /* If we know where the return address is saved, we can take a
         solid guess at what the frame pointer should be.  */
      if (cache->saved_regs[M88K_R1_REGNUM].addr != -1)
	cache->fp_offset = cache->saved_regs[M88K_R1_REGNUM].addr - 4;
      frame_sp = frame_unwind_register_unsigned (next_frame, M88K_R31_REGNUM);
    }

  /* Now that we know the stack pointer, adjust the location of the
     saved registers.  */
  {
    int regnum;

    for (regnum = M88K_R0_REGNUM; regnum < M88K_R31_REGNUM; regnum ++)
      if (cache->saved_regs[regnum].addr != -1)
	cache->saved_regs[regnum].addr += frame_sp;
  }

  /* Calculate the frame's base.  */
  cache->base = frame_sp - cache->sp_offset;
  trad_frame_set_value (cache->saved_regs, M88K_R31_REGNUM, cache->base);

  /* Identify SXIP with the return address in R1.  */
  cache->saved_regs[M88K_SXIP_REGNUM] = cache->saved_regs[M88K_R1_REGNUM];

  *this_cache = cache;
  return cache;
}

static void
m88k_frame_this_id (struct frame_info *next_frame, void **this_cache,
		    struct frame_id *this_id)
{
  struct m88k_frame_cache *cache = m88k_frame_cache (next_frame, this_cache);

  /* This marks the outermost frame.  */
  if (cache->base == 0)
    return;

  (*this_id) = frame_id_build (cache->base, cache->pc);
}

static void
m88k_frame_prev_register (struct frame_info *next_frame, void **this_cache,
			  int regnum, int *optimizedp,
			  enum lval_type *lvalp, CORE_ADDR *addrp,
			  int *realnump, gdb_byte *valuep)
{
  struct m88k_frame_cache *cache = m88k_frame_cache (next_frame, this_cache);

  if (regnum == M88K_SNIP_REGNUM || regnum == M88K_SFIP_REGNUM)
    {
      if (valuep)
	{
	  CORE_ADDR pc;

	  trad_frame_get_prev_register (next_frame, cache->saved_regs,
					M88K_SXIP_REGNUM, optimizedp,
					lvalp, addrp, realnump, valuep);

	  pc = extract_unsigned_integer (valuep, 4);
	  if (regnum == M88K_SFIP_REGNUM)
	    pc += 4;
	  store_unsigned_integer (valuep, 4, pc + 4);
	}

      /* It's a computed value.  */
      *optimizedp = 0;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      return;
    }

  trad_frame_get_prev_register (next_frame, cache->saved_regs, regnum,
				optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind m88k_frame_unwind =
{
  NORMAL_FRAME,
  m88k_frame_this_id,
  m88k_frame_prev_register
};

static const struct frame_unwind *
m88k_frame_sniffer (struct frame_info *next_frame)
{
  return &m88k_frame_unwind;
}


static CORE_ADDR
m88k_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct m88k_frame_cache *cache = m88k_frame_cache (next_frame, this_cache);

  if (cache->fp_offset != -1)
    return cache->base + cache->sp_offset + cache->fp_offset;

  return 0;
}

static const struct frame_base m88k_frame_base =
{
  &m88k_frame_unwind,
  m88k_frame_base_address,
  m88k_frame_base_address,
  m88k_frame_base_address
};


/* Core file support.  */

/* Supply register REGNUM from the buffer specified by GREGS and LEN
   in the general-purpose register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
m88k_supply_gregset (const struct regset *regset,
		     struct regcache *regcache,
		     int regnum, const void *gregs, size_t len)
{
  const gdb_byte *regs = gregs;
  int i;

  for (i = 0; i < M88K_NUM_REGS; i++)
    {
      if (regnum == i || regnum == -1)
	regcache_raw_supply (regcache, i, regs + i * 4);
    }
}

/* Motorola 88000 register set.  */

static struct regset m88k_gregset =
{
  NULL,
  m88k_supply_gregset
};

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

static const struct regset *
m88k_regset_from_core_section (struct gdbarch *gdbarch,
			       const char *sect_name, size_t sect_size)
{
  if (strcmp (sect_name, ".reg") == 0 && sect_size >= M88K_NUM_REGS * 4)
    return &m88k_gregset;

  return NULL;
}


static struct gdbarch *
m88k_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;

  /* If there is already a candidate, use it.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  /* Allocate space for the new architecture.  */
  gdbarch = gdbarch_alloc (&info, NULL);

  /* There is no real `long double'.  */
  set_gdbarch_long_double_bit (gdbarch, 64);
  set_gdbarch_long_double_format (gdbarch, floatformats_ieee_double);

  set_gdbarch_num_regs (gdbarch, M88K_NUM_REGS);
  set_gdbarch_register_name (gdbarch, m88k_register_name);
  set_gdbarch_register_type (gdbarch, m88k_register_type);

  /* Register numbers of various important registers.  */
  set_gdbarch_sp_regnum (gdbarch, M88K_R31_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, M88K_SXIP_REGNUM);

  /* Core file support.  */
  set_gdbarch_regset_from_core_section
    (gdbarch, m88k_regset_from_core_section);

  set_gdbarch_print_insn (gdbarch, print_insn_m88k);

  set_gdbarch_skip_prologue (gdbarch, m88k_skip_prologue);

  /* Stack grows downward.  */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  /* Call dummy code.  */
  set_gdbarch_push_dummy_call (gdbarch, m88k_push_dummy_call);
  set_gdbarch_unwind_dummy_id (gdbarch, m88k_unwind_dummy_id);

  /* Return value info */
  set_gdbarch_return_value (gdbarch, m88k_return_value);

  set_gdbarch_addr_bits_remove (gdbarch, m88k_addr_bits_remove);
  set_gdbarch_breakpoint_from_pc (gdbarch, m88k_breakpoint_from_pc);
  set_gdbarch_unwind_pc (gdbarch, m88k_unwind_pc);
  set_gdbarch_write_pc (gdbarch, m88k_write_pc);

  frame_base_set_default (gdbarch, &m88k_frame_base);
  frame_unwind_append_sniffer (gdbarch, m88k_frame_sniffer);

  return gdbarch;
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_m88k_tdep (void);

void
_initialize_m88k_tdep (void)
{
  gdbarch_register (bfd_arch_m88k, m88k_gdbarch_init, NULL);
}
