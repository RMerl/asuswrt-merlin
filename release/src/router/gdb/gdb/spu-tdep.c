/* SPU target-dependent code for GDB, the GNU debugger.
   Copyright (C) 2006, 2007 Free Software Foundation, Inc.

   Contributed by Ulrich Weigand <uweigand@de.ibm.com>.
   Based on a port by Sid Manning <sid@us.ibm.com>.

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
#include "gdbtypes.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "gdb_string.h"
#include "gdb_assert.h"
#include "frame.h"
#include "frame-unwind.h"
#include "frame-base.h"
#include "trad-frame.h"
#include "symtab.h"
#include "symfile.h"
#include "value.h"
#include "inferior.h"
#include "dis-asm.h"
#include "objfiles.h"
#include "language.h"
#include "regcache.h"
#include "reggroups.h"
#include "floatformat.h"
#include "observer.h"

#include "spu-tdep.h"


/* The tdep structure.  */
struct gdbarch_tdep
{
  /* SPU-specific vector type.  */
  struct type *spu_builtin_type_vec128;
};


/* SPU-specific vector type.  */
static struct type *
spu_builtin_type_vec128 (struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  if (!tdep->spu_builtin_type_vec128)
    {
      struct type *t;

      t = init_composite_type ("__spu_builtin_type_vec128", TYPE_CODE_UNION);
      append_composite_type_field (t, "uint128", builtin_type_int128);
      append_composite_type_field (t, "v2_int64",
				   init_vector_type (builtin_type_int64, 2));
      append_composite_type_field (t, "v4_int32",
				   init_vector_type (builtin_type_int32, 4));
      append_composite_type_field (t, "v8_int16",
				   init_vector_type (builtin_type_int16, 8));
      append_composite_type_field (t, "v16_int8",
				   init_vector_type (builtin_type_int8, 16));
      append_composite_type_field (t, "v2_double",
				   init_vector_type (builtin_type_double, 2));
      append_composite_type_field (t, "v4_float",
				   init_vector_type (builtin_type_float, 4));

      TYPE_FLAGS (t) |= TYPE_FLAG_VECTOR;
      TYPE_NAME (t) = "spu_builtin_type_vec128";

      tdep->spu_builtin_type_vec128 = t;
    }

  return tdep->spu_builtin_type_vec128;
}


/* The list of available "info spu " commands.  */
static struct cmd_list_element *infospucmdlist = NULL;

/* Registers.  */

static const char *
spu_register_name (int reg_nr)
{
  static char *register_names[] = 
    {
      "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
      "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
      "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
      "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
      "r32", "r33", "r34", "r35", "r36", "r37", "r38", "r39",
      "r40", "r41", "r42", "r43", "r44", "r45", "r46", "r47",
      "r48", "r49", "r50", "r51", "r52", "r53", "r54", "r55",
      "r56", "r57", "r58", "r59", "r60", "r61", "r62", "r63",
      "r64", "r65", "r66", "r67", "r68", "r69", "r70", "r71",
      "r72", "r73", "r74", "r75", "r76", "r77", "r78", "r79",
      "r80", "r81", "r82", "r83", "r84", "r85", "r86", "r87",
      "r88", "r89", "r90", "r91", "r92", "r93", "r94", "r95",
      "r96", "r97", "r98", "r99", "r100", "r101", "r102", "r103",
      "r104", "r105", "r106", "r107", "r108", "r109", "r110", "r111",
      "r112", "r113", "r114", "r115", "r116", "r117", "r118", "r119",
      "r120", "r121", "r122", "r123", "r124", "r125", "r126", "r127",
      "id", "pc", "sp", "fpscr", "srr0", "lslr", "decr", "decr_status"
    };

  if (reg_nr < 0)
    return NULL;
  if (reg_nr >= sizeof register_names / sizeof *register_names)
    return NULL;

  return register_names[reg_nr];
}

static struct type *
spu_register_type (struct gdbarch *gdbarch, int reg_nr)
{
  if (reg_nr < SPU_NUM_GPRS)
    return spu_builtin_type_vec128 (gdbarch);

  switch (reg_nr)
    {
    case SPU_ID_REGNUM:
      return builtin_type_uint32;

    case SPU_PC_REGNUM:
      return builtin_type_void_func_ptr;

    case SPU_SP_REGNUM:
      return builtin_type_void_data_ptr;

    case SPU_FPSCR_REGNUM:
      return builtin_type_uint128;

    case SPU_SRR0_REGNUM:
      return builtin_type_uint32;

    case SPU_LSLR_REGNUM:
      return builtin_type_uint32;

    case SPU_DECR_REGNUM:
      return builtin_type_uint32;

    case SPU_DECR_STATUS_REGNUM:
      return builtin_type_uint32;

    default:
      internal_error (__FILE__, __LINE__, "invalid regnum");
    }
}

/* Pseudo registers for preferred slots - stack pointer.  */

static void
spu_pseudo_register_read_spu (struct regcache *regcache, const char *regname,
			      gdb_byte *buf)
{
  gdb_byte reg[32];
  char annex[32];
  ULONGEST id;

  regcache_raw_read_unsigned (regcache, SPU_ID_REGNUM, &id);
  xsnprintf (annex, sizeof annex, "%d/%s", (int) id, regname);
  memset (reg, 0, sizeof reg);
  target_read (&current_target, TARGET_OBJECT_SPU, annex,
	       reg, 0, sizeof reg);

  store_unsigned_integer (buf, 4, strtoulst (reg, NULL, 16));
}

static void
spu_pseudo_register_read (struct gdbarch *gdbarch, struct regcache *regcache,
                          int regnum, gdb_byte *buf)
{
  gdb_byte reg[16];
  char annex[32];
  ULONGEST id;

  switch (regnum)
    {
    case SPU_SP_REGNUM:
      regcache_raw_read (regcache, SPU_RAW_SP_REGNUM, reg);
      memcpy (buf, reg, 4);
      break;

    case SPU_FPSCR_REGNUM:
      regcache_raw_read_unsigned (regcache, SPU_ID_REGNUM, &id);
      xsnprintf (annex, sizeof annex, "%d/fpcr", (int) id);
      target_read (&current_target, TARGET_OBJECT_SPU, annex, buf, 0, 16);
      break;

    case SPU_SRR0_REGNUM:
      spu_pseudo_register_read_spu (regcache, "srr0", buf);
      break;

    case SPU_LSLR_REGNUM:
      spu_pseudo_register_read_spu (regcache, "lslr", buf);
      break;

    case SPU_DECR_REGNUM:
      spu_pseudo_register_read_spu (regcache, "decr", buf);
      break;

    case SPU_DECR_STATUS_REGNUM:
      spu_pseudo_register_read_spu (regcache, "decr_status", buf);
      break;

    default:
      internal_error (__FILE__, __LINE__, _("invalid regnum"));
    }
}

static void
spu_pseudo_register_write_spu (struct regcache *regcache, const char *regname,
			       const gdb_byte *buf)
{
  gdb_byte reg[32];
  char annex[32];
  ULONGEST id;

  regcache_raw_read_unsigned (regcache, SPU_ID_REGNUM, &id);
  xsnprintf (annex, sizeof annex, "%d/%s", (int) id, regname);
  xsnprintf (reg, sizeof reg, "0x%s",
	     phex_nz (extract_unsigned_integer (buf, 4), 4));
  target_write (&current_target, TARGET_OBJECT_SPU, annex,
		reg, 0, strlen (reg));
}

static void
spu_pseudo_register_write (struct gdbarch *gdbarch, struct regcache *regcache,
                           int regnum, const gdb_byte *buf)
{
  gdb_byte reg[16];
  char annex[32];
  ULONGEST id;

  switch (regnum)
    {
    case SPU_SP_REGNUM:
      regcache_raw_read (regcache, SPU_RAW_SP_REGNUM, reg);
      memcpy (reg, buf, 4);
      regcache_raw_write (regcache, SPU_RAW_SP_REGNUM, reg);
      break;

    case SPU_FPSCR_REGNUM:
      regcache_raw_read_unsigned (regcache, SPU_ID_REGNUM, &id);
      xsnprintf (annex, sizeof annex, "%d/fpcr", (int) id);
      target_write (&current_target, TARGET_OBJECT_SPU, annex, buf, 0, 16);
      break;

    case SPU_SRR0_REGNUM:
      spu_pseudo_register_write_spu (regcache, "srr0", buf);
      break;

    case SPU_LSLR_REGNUM:
      spu_pseudo_register_write_spu (regcache, "lslr", buf);
      break;

    case SPU_DECR_REGNUM:
      spu_pseudo_register_write_spu (regcache, "decr", buf);
      break;

    case SPU_DECR_STATUS_REGNUM:
      spu_pseudo_register_write_spu (regcache, "decr_status", buf);
      break;

    default:
      internal_error (__FILE__, __LINE__, _("invalid regnum"));
    }
}

/* Value conversion -- access scalar values at the preferred slot.  */

static struct value *
spu_value_from_register (struct type *type, int regnum,
			 struct frame_info *frame)
{
  struct value *value = default_value_from_register (type, regnum, frame);
  int len = TYPE_LENGTH (type);

  if (regnum < SPU_NUM_GPRS && len < 16)
    {
      int preferred_slot = len < 4 ? 4 - len : 0;
      set_value_offset (value, preferred_slot);
    }

  return value;
}

/* Register groups.  */

static int
spu_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
			 struct reggroup *group)
{
  /* Registers displayed via 'info regs'.  */
  if (group == general_reggroup)
    return 1;

  /* Registers displayed via 'info float'.  */
  if (group == float_reggroup)
    return 0;

  /* Registers that need to be saved/restored in order to
     push or pop frames.  */
  if (group == save_reggroup || group == restore_reggroup)
    return 1;

  return default_register_reggroup_p (gdbarch, regnum, group);
}

/* Address conversion.  */

static CORE_ADDR
spu_pointer_to_address (struct type *type, const gdb_byte *buf)
{
  ULONGEST addr = extract_unsigned_integer (buf, TYPE_LENGTH (type));
  ULONGEST lslr = SPU_LS_SIZE - 1; /* Hard-wired LS size.  */

  if (target_has_registers && target_has_stack && target_has_memory)
    lslr = get_frame_register_unsigned (get_selected_frame (NULL),
					SPU_LSLR_REGNUM);

  return addr & lslr;
}

static CORE_ADDR
spu_integer_to_address (struct gdbarch *gdbarch,
			struct type *type, const gdb_byte *buf)
{
  ULONGEST addr = unpack_long (type, buf);
  ULONGEST lslr = SPU_LS_SIZE - 1; /* Hard-wired LS size.  */

  if (target_has_registers && target_has_stack && target_has_memory)
    lslr = get_frame_register_unsigned (get_selected_frame (NULL),
					SPU_LSLR_REGNUM);

  return addr & lslr;
}


/* Decoding SPU instructions.  */

enum
  {
    op_lqd   = 0x34,
    op_lqx   = 0x3c4,
    op_lqa   = 0x61,
    op_lqr   = 0x67,
    op_stqd  = 0x24,
    op_stqx  = 0x144,
    op_stqa  = 0x41,
    op_stqr  = 0x47,

    op_il    = 0x081,
    op_ila   = 0x21,
    op_a     = 0x0c0,
    op_ai    = 0x1c,

    op_selb  = 0x4,

    op_br    = 0x64,
    op_bra   = 0x60,
    op_brsl  = 0x66,
    op_brasl = 0x62,
    op_brnz  = 0x42,
    op_brz   = 0x40,
    op_brhnz = 0x46,
    op_brhz  = 0x44,
    op_bi    = 0x1a8,
    op_bisl  = 0x1a9,
    op_biz   = 0x128,
    op_binz  = 0x129,
    op_bihz  = 0x12a,
    op_bihnz = 0x12b,
  };

static int
is_rr (unsigned int insn, int op, int *rt, int *ra, int *rb)
{
  if ((insn >> 21) == op)
    {
      *rt = insn & 127;
      *ra = (insn >> 7) & 127;
      *rb = (insn >> 14) & 127;
      return 1;
    }

  return 0;
}

static int
is_rrr (unsigned int insn, int op, int *rt, int *ra, int *rb, int *rc)
{
  if ((insn >> 28) == op)
    {
      *rt = (insn >> 21) & 127;
      *ra = (insn >> 7) & 127;
      *rb = (insn >> 14) & 127;
      *rc = insn & 127;
      return 1;
    }

  return 0;
}

static int
is_ri7 (unsigned int insn, int op, int *rt, int *ra, int *i7)
{
  if ((insn >> 21) == op)
    {
      *rt = insn & 127;
      *ra = (insn >> 7) & 127;
      *i7 = (((insn >> 14) & 127) ^ 0x40) - 0x40;
      return 1;
    }

  return 0;
}

static int
is_ri10 (unsigned int insn, int op, int *rt, int *ra, int *i10)
{
  if ((insn >> 24) == op)
    {
      *rt = insn & 127;
      *ra = (insn >> 7) & 127;
      *i10 = (((insn >> 14) & 0x3ff) ^ 0x200) - 0x200;
      return 1;
    }

  return 0;
}

static int
is_ri16 (unsigned int insn, int op, int *rt, int *i16)
{
  if ((insn >> 23) == op)
    {
      *rt = insn & 127;
      *i16 = (((insn >> 7) & 0xffff) ^ 0x8000) - 0x8000;
      return 1;
    }

  return 0;
}

static int
is_ri18 (unsigned int insn, int op, int *rt, int *i18)
{
  if ((insn >> 25) == op)
    {
      *rt = insn & 127;
      *i18 = (((insn >> 7) & 0x3ffff) ^ 0x20000) - 0x20000;
      return 1;
    }

  return 0;
}

static int
is_branch (unsigned int insn, int *offset, int *reg)
{
  int rt, i7, i16;

  if (is_ri16 (insn, op_br, &rt, &i16)
      || is_ri16 (insn, op_brsl, &rt, &i16)
      || is_ri16 (insn, op_brnz, &rt, &i16)
      || is_ri16 (insn, op_brz, &rt, &i16)
      || is_ri16 (insn, op_brhnz, &rt, &i16)
      || is_ri16 (insn, op_brhz, &rt, &i16))
    {
      *reg = SPU_PC_REGNUM;
      *offset = i16 << 2;
      return 1;
    }

  if (is_ri16 (insn, op_bra, &rt, &i16)
      || is_ri16 (insn, op_brasl, &rt, &i16))
    {
      *reg = -1;
      *offset = i16 << 2;
      return 1;
    }

  if (is_ri7 (insn, op_bi, &rt, reg, &i7)
      || is_ri7 (insn, op_bisl, &rt, reg, &i7)
      || is_ri7 (insn, op_biz, &rt, reg, &i7)
      || is_ri7 (insn, op_binz, &rt, reg, &i7)
      || is_ri7 (insn, op_bihz, &rt, reg, &i7)
      || is_ri7 (insn, op_bihnz, &rt, reg, &i7))
    {
      *offset = 0;
      return 1;
    }

  return 0;
}


/* Prolog parsing.  */

struct spu_prologue_data
  {
    /* Stack frame size.  -1 if analysis was unsuccessful.  */
    int size;

    /* How to find the CFA.  The CFA is equal to SP at function entry.  */
    int cfa_reg;
    int cfa_offset;

    /* Offset relative to CFA where a register is saved.  -1 if invalid.  */
    int reg_offset[SPU_NUM_GPRS];
  };

static CORE_ADDR
spu_analyze_prologue (CORE_ADDR start_pc, CORE_ADDR end_pc,
                      struct spu_prologue_data *data)
{
  int found_sp = 0;
  int found_fp = 0;
  int found_lr = 0;
  int reg_immed[SPU_NUM_GPRS];
  gdb_byte buf[16];
  CORE_ADDR prolog_pc = start_pc;
  CORE_ADDR pc;
  int i;


  /* Initialize DATA to default values.  */
  data->size = -1;

  data->cfa_reg = SPU_RAW_SP_REGNUM;
  data->cfa_offset = 0;

  for (i = 0; i < SPU_NUM_GPRS; i++)
    data->reg_offset[i] = -1;

  /* Set up REG_IMMED array.  This is non-zero for a register if we know its
     preferred slot currently holds this immediate value.  */
  for (i = 0; i < SPU_NUM_GPRS; i++)
      reg_immed[i] = 0;

  /* Scan instructions until the first branch.

     The following instructions are important prolog components:

	- The first instruction to set up the stack pointer.
	- The first instruction to set up the frame pointer.
	- The first instruction to save the link register.

     We return the instruction after the latest of these three,
     or the incoming PC if none is found.  The first instruction
     to set up the stack pointer also defines the frame size.

     Note that instructions saving incoming arguments to their stack
     slots are not counted as important, because they are hard to
     identify with certainty.  This should not matter much, because
     arguments are relevant only in code compiled with debug data,
     and in such code the GDB core will advance until the first source
     line anyway, using SAL data.

     For purposes of stack unwinding, we analyze the following types
     of instructions in addition:

      - Any instruction adding to the current frame pointer.
      - Any instruction loading an immediate constant into a register.
      - Any instruction storing a register onto the stack.

     These are used to compute the CFA and REG_OFFSET output.  */

  for (pc = start_pc; pc < end_pc; pc += 4)
    {
      unsigned int insn;
      int rt, ra, rb, rc, immed;

      if (target_read_memory (pc, buf, 4))
	break;
      insn = extract_unsigned_integer (buf, 4);

      /* AI is the typical instruction to set up a stack frame.
         It is also used to initialize the frame pointer.  */
      if (is_ri10 (insn, op_ai, &rt, &ra, &immed))
	{
	  if (rt == data->cfa_reg && ra == data->cfa_reg)
	    data->cfa_offset -= immed;

	  if (rt == SPU_RAW_SP_REGNUM && ra == SPU_RAW_SP_REGNUM
	      && !found_sp)
	    {
	      found_sp = 1;
	      prolog_pc = pc + 4;

	      data->size = -immed;
	    }
	  else if (rt == SPU_FP_REGNUM && ra == SPU_RAW_SP_REGNUM
		   && !found_fp)
	    {
	      found_fp = 1;
	      prolog_pc = pc + 4;

	      data->cfa_reg = SPU_FP_REGNUM;
	      data->cfa_offset -= immed;
	    }
	}

      /* A is used to set up stack frames of size >= 512 bytes.
         If we have tracked the contents of the addend register,
         we can handle this as well.  */
      else if (is_rr (insn, op_a, &rt, &ra, &rb))
	{
	  if (rt == data->cfa_reg && ra == data->cfa_reg)
	    {
	      if (reg_immed[rb] != 0)
		data->cfa_offset -= reg_immed[rb];
	      else
		data->cfa_reg = -1;  /* We don't know the CFA any more.  */
	    }

	  if (rt == SPU_RAW_SP_REGNUM && ra == SPU_RAW_SP_REGNUM
	      && !found_sp)
	    {
	      found_sp = 1;
	      prolog_pc = pc + 4;

	      if (reg_immed[rb] != 0)
		data->size = -reg_immed[rb];
	    }
	}

      /* We need to track IL and ILA used to load immediate constants
         in case they are later used as input to an A instruction.  */
      else if (is_ri16 (insn, op_il, &rt, &immed))
	{
	  reg_immed[rt] = immed;

	  if (rt == SPU_RAW_SP_REGNUM && !found_sp)
	    found_sp = 1;
	}

      else if (is_ri18 (insn, op_ila, &rt, &immed))
	{
	  reg_immed[rt] = immed & 0x3ffff;

	  if (rt == SPU_RAW_SP_REGNUM && !found_sp)
	    found_sp = 1;
	}

      /* STQD is used to save registers to the stack.  */
      else if (is_ri10 (insn, op_stqd, &rt, &ra, &immed))
	{
	  if (ra == data->cfa_reg)
	    data->reg_offset[rt] = data->cfa_offset - (immed << 4);

	  if (ra == data->cfa_reg && rt == SPU_LR_REGNUM
              && !found_lr)
	    {
	      found_lr = 1;
	      prolog_pc = pc + 4;
	    }
	}

      /* _start uses SELB to set up the stack pointer.  */
      else if (is_rrr (insn, op_selb, &rt, &ra, &rb, &rc))
	{
	  if (rt == SPU_RAW_SP_REGNUM && !found_sp)
	    found_sp = 1;
	}

      /* We terminate if we find a branch.  */
      else if (is_branch (insn, &immed, &ra))
	break;
    }


  /* If we successfully parsed until here, and didn't find any instruction
     modifying SP, we assume we have a frameless function.  */
  if (!found_sp)
    data->size = 0;

  /* Return cooked instead of raw SP.  */
  if (data->cfa_reg == SPU_RAW_SP_REGNUM)
    data->cfa_reg = SPU_SP_REGNUM;

  return prolog_pc;
}

/* Return the first instruction after the prologue starting at PC.  */
static CORE_ADDR
spu_skip_prologue (CORE_ADDR pc)
{
  struct spu_prologue_data data;
  return spu_analyze_prologue (pc, (CORE_ADDR)-1, &data);
}

/* Return the frame pointer in use at address PC.  */
static void
spu_virtual_frame_pointer (CORE_ADDR pc, int *reg, LONGEST *offset)
{
  struct spu_prologue_data data;
  spu_analyze_prologue (pc, (CORE_ADDR)-1, &data);

  if (data.size != -1 && data.cfa_reg != -1)
    {
      /* The 'frame pointer' address is CFA minus frame size.  */
      *reg = data.cfa_reg;
      *offset = data.cfa_offset - data.size;
    }
  else
    {
      /* ??? We don't really know ... */
      *reg = SPU_SP_REGNUM;
      *offset = 0;
    }
}

/* Return true if we are in the function's epilogue, i.e. after the
   instruction that destroyed the function's stack frame.

   1) scan forward from the point of execution:
       a) If you find an instruction that modifies the stack pointer
          or transfers control (except a return), execution is not in
          an epilogue, return.
       b) Stop scanning if you find a return instruction or reach the
          end of the function or reach the hard limit for the size of
          an epilogue.
   2) scan backward from the point of execution:
        a) If you find an instruction that modifies the stack pointer,
            execution *is* in an epilogue, return.
        b) Stop scanning if you reach an instruction that transfers
           control or the beginning of the function or reach the hard
           limit for the size of an epilogue.  */

static int
spu_in_function_epilogue_p (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  CORE_ADDR scan_pc, func_start, func_end, epilogue_start, epilogue_end;
  bfd_byte buf[4];
  unsigned int insn;
  int rt, ra, rb, rc, immed;

  /* Find the search limits based on function boundaries and hard limit.
     We assume the epilogue can be up to 64 instructions long.  */

  const int spu_max_epilogue_size = 64 * 4;

  if (!find_pc_partial_function (pc, NULL, &func_start, &func_end))
    return 0;

  if (pc - func_start < spu_max_epilogue_size)
    epilogue_start = func_start;
  else
    epilogue_start = pc - spu_max_epilogue_size;

  if (func_end - pc < spu_max_epilogue_size)
    epilogue_end = func_end;
  else
    epilogue_end = pc + spu_max_epilogue_size;

  /* Scan forward until next 'bi $0'.  */

  for (scan_pc = pc; scan_pc < epilogue_end; scan_pc += 4)
    {
      if (target_read_memory (scan_pc, buf, 4))
	return 0;
      insn = extract_unsigned_integer (buf, 4);

      if (is_branch (insn, &immed, &ra))
	{
	  if (immed == 0 && ra == SPU_LR_REGNUM)
	    break;

	  return 0;
	}

      if (is_ri10 (insn, op_ai, &rt, &ra, &immed)
	  || is_rr (insn, op_a, &rt, &ra, &rb)
	  || is_ri10 (insn, op_lqd, &rt, &ra, &immed))
	{
	  if (rt == SPU_RAW_SP_REGNUM)
	    return 0;
	}
    }

  if (scan_pc >= epilogue_end)
    return 0;

  /* Scan backward until adjustment to stack pointer (R1).  */

  for (scan_pc = pc - 4; scan_pc >= epilogue_start; scan_pc -= 4)
    {
      if (target_read_memory (scan_pc, buf, 4))
	return 0;
      insn = extract_unsigned_integer (buf, 4);

      if (is_branch (insn, &immed, &ra))
	return 0;

      if (is_ri10 (insn, op_ai, &rt, &ra, &immed)
	  || is_rr (insn, op_a, &rt, &ra, &rb)
	  || is_ri10 (insn, op_lqd, &rt, &ra, &immed))
	{
	  if (rt == SPU_RAW_SP_REGNUM)
	    return 1;
	}
    }

  return 0;
}


/* Normal stack frames.  */

struct spu_unwind_cache
{
  CORE_ADDR func;
  CORE_ADDR frame_base;
  CORE_ADDR local_base;

  struct trad_frame_saved_reg *saved_regs;
};

static struct spu_unwind_cache *
spu_frame_unwind_cache (struct frame_info *next_frame,
			void **this_prologue_cache)
{
  struct spu_unwind_cache *info;
  struct spu_prologue_data data;
  gdb_byte buf[16];

  if (*this_prologue_cache)
    return *this_prologue_cache;

  info = FRAME_OBSTACK_ZALLOC (struct spu_unwind_cache);
  *this_prologue_cache = info;
  info->saved_regs = trad_frame_alloc_saved_regs (next_frame);
  info->frame_base = 0;
  info->local_base = 0;

  /* Find the start of the current function, and analyze its prologue.  */
  info->func = frame_func_unwind (next_frame, NORMAL_FRAME);
  if (info->func == 0)
    {
      /* Fall back to using the current PC as frame ID.  */
      info->func = frame_pc_unwind (next_frame);
      data.size = -1;
    }
  else
    spu_analyze_prologue (info->func, frame_pc_unwind (next_frame), &data);


  /* If successful, use prologue analysis data.  */
  if (data.size != -1 && data.cfa_reg != -1)
    {
      CORE_ADDR cfa;
      int i;

      /* Determine CFA via unwound CFA_REG plus CFA_OFFSET.  */
      frame_unwind_register (next_frame, data.cfa_reg, buf);
      cfa = extract_unsigned_integer (buf, 4) + data.cfa_offset;

      /* Call-saved register slots.  */
      for (i = 0; i < SPU_NUM_GPRS; i++)
	if (i == SPU_LR_REGNUM
	    || (i >= SPU_SAVED1_REGNUM && i <= SPU_SAVEDN_REGNUM))
	  if (data.reg_offset[i] != -1)
	    info->saved_regs[i].addr = cfa - data.reg_offset[i];

      /* Frame bases.  */
      info->frame_base = cfa;
      info->local_base = cfa - data.size;
    }

  /* Otherwise, fall back to reading the backchain link.  */
  else
    {
      CORE_ADDR reg, backchain;

      /* Get the backchain.  */
      reg = frame_unwind_register_unsigned (next_frame, SPU_SP_REGNUM);
      backchain = read_memory_unsigned_integer (reg, 4);

      /* A zero backchain terminates the frame chain.  Also, sanity
         check against the local store size limit.  */
      if (backchain != 0 && backchain < SPU_LS_SIZE)
	{
	  /* Assume the link register is saved into its slot.  */
	  if (backchain + 16 < SPU_LS_SIZE)
	    info->saved_regs[SPU_LR_REGNUM].addr = backchain + 16;

          /* Frame bases.  */
	  info->frame_base = backchain;
	  info->local_base = reg;
	}
    }

  /* The previous SP is equal to the CFA.  */
  trad_frame_set_value (info->saved_regs, SPU_SP_REGNUM, info->frame_base);

  /* Read full contents of the unwound link register in order to
     be able to determine the return address.  */
  if (trad_frame_addr_p (info->saved_regs, SPU_LR_REGNUM))
    target_read_memory (info->saved_regs[SPU_LR_REGNUM].addr, buf, 16);
  else
    frame_unwind_register (next_frame, SPU_LR_REGNUM, buf);

  /* Normally, the return address is contained in the slot 0 of the
     link register, and slots 1-3 are zero.  For an overlay return,
     slot 0 contains the address of the overlay manager return stub,
     slot 1 contains the partition number of the overlay section to
     be returned to, and slot 2 contains the return address within
     that section.  Return the latter address in that case.  */
  if (extract_unsigned_integer (buf + 8, 4) != 0)
    trad_frame_set_value (info->saved_regs, SPU_PC_REGNUM,
			  extract_unsigned_integer (buf + 8, 4));
  else
    trad_frame_set_value (info->saved_regs, SPU_PC_REGNUM,
			  extract_unsigned_integer (buf, 4));
 
  return info;
}

static void
spu_frame_this_id (struct frame_info *next_frame,
		   void **this_prologue_cache, struct frame_id *this_id)
{
  struct spu_unwind_cache *info =
    spu_frame_unwind_cache (next_frame, this_prologue_cache);

  if (info->frame_base == 0)
    return;

  *this_id = frame_id_build (info->frame_base, info->func);
}

static void
spu_frame_prev_register (struct frame_info *next_frame,
			 void **this_prologue_cache,
			 int regnum, int *optimizedp,
			 enum lval_type *lvalp, CORE_ADDR * addrp,
			 int *realnump, gdb_byte *bufferp)
{
  struct spu_unwind_cache *info
    = spu_frame_unwind_cache (next_frame, this_prologue_cache);

  /* Special-case the stack pointer.  */
  if (regnum == SPU_RAW_SP_REGNUM)
    regnum = SPU_SP_REGNUM;

  trad_frame_get_prev_register (next_frame, info->saved_regs, regnum,
				optimizedp, lvalp, addrp, realnump, bufferp);
}

static const struct frame_unwind spu_frame_unwind = {
  NORMAL_FRAME,
  spu_frame_this_id,
  spu_frame_prev_register
};

const struct frame_unwind *
spu_frame_sniffer (struct frame_info *next_frame)
{
  return &spu_frame_unwind;
}

static CORE_ADDR
spu_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct spu_unwind_cache *info
    = spu_frame_unwind_cache (next_frame, this_cache);
  return info->local_base;
}

static const struct frame_base spu_frame_base = {
  &spu_frame_unwind,
  spu_frame_base_address,
  spu_frame_base_address,
  spu_frame_base_address
};

static CORE_ADDR
spu_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_unwind_register_unsigned (next_frame, SPU_PC_REGNUM);
  /* Mask off interrupt enable bit.  */
  return pc & -4;
}

static CORE_ADDR
spu_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, SPU_SP_REGNUM);
}

static CORE_ADDR
spu_read_pc (struct regcache *regcache)
{
  ULONGEST pc;
  regcache_cooked_read_unsigned (regcache, SPU_PC_REGNUM, &pc);
  /* Mask off interrupt enable bit.  */
  return pc & -4;
}

static void
spu_write_pc (struct regcache *regcache, CORE_ADDR pc)
{
  /* Keep interrupt enabled state unchanged.  */
  ULONGEST old_pc;
  regcache_cooked_read_unsigned (regcache, SPU_PC_REGNUM, &old_pc);
  regcache_cooked_write_unsigned (regcache, SPU_PC_REGNUM,
				  (pc & -4) | (old_pc & 3));
}


/* Function calling convention.  */

static CORE_ADDR
spu_frame_align (struct gdbarch *gdbarch, CORE_ADDR sp)
{
  return sp & ~15;
}

static int
spu_scalar_value_p (struct type *type)
{
  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_INT:
    case TYPE_CODE_ENUM:
    case TYPE_CODE_RANGE:
    case TYPE_CODE_CHAR:
    case TYPE_CODE_BOOL:
    case TYPE_CODE_PTR:
    case TYPE_CODE_REF:
      return TYPE_LENGTH (type) <= 16;

    default:
      return 0;
    }
}

static void
spu_value_to_regcache (struct regcache *regcache, int regnum,
		       struct type *type, const gdb_byte *in)
{
  int len = TYPE_LENGTH (type);

  if (spu_scalar_value_p (type))
    {
      int preferred_slot = len < 4 ? 4 - len : 0;
      regcache_cooked_write_part (regcache, regnum, preferred_slot, len, in);
    }
  else
    {
      while (len >= 16)
	{
	  regcache_cooked_write (regcache, regnum++, in);
	  in += 16;
	  len -= 16;
	}

      if (len > 0)
	regcache_cooked_write_part (regcache, regnum, 0, len, in);
    }
}

static void
spu_regcache_to_value (struct regcache *regcache, int regnum,
		       struct type *type, gdb_byte *out)
{
  int len = TYPE_LENGTH (type);

  if (spu_scalar_value_p (type))
    {
      int preferred_slot = len < 4 ? 4 - len : 0;
      regcache_cooked_read_part (regcache, regnum, preferred_slot, len, out);
    }
  else
    {
      while (len >= 16)
	{
	  regcache_cooked_read (regcache, regnum++, out);
	  out += 16;
	  len -= 16;
	}

      if (len > 0)
	regcache_cooked_read_part (regcache, regnum, 0, len, out);
    }
}

static CORE_ADDR
spu_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		     struct regcache *regcache, CORE_ADDR bp_addr,
		     int nargs, struct value **args, CORE_ADDR sp,
		     int struct_return, CORE_ADDR struct_addr)
{
  int i;
  int regnum = SPU_ARG1_REGNUM;
  int stack_arg = -1;
  gdb_byte buf[16];

  /* Set the return address.  */
  memset (buf, 0, sizeof buf);
  store_unsigned_integer (buf, 4, bp_addr);
  regcache_cooked_write (regcache, SPU_LR_REGNUM, buf);

  /* If STRUCT_RETURN is true, then the struct return address (in
     STRUCT_ADDR) will consume the first argument-passing register.
     Both adjust the register count and store that value.  */
  if (struct_return)
    {
      memset (buf, 0, sizeof buf);
      store_unsigned_integer (buf, 4, struct_addr);
      regcache_cooked_write (regcache, regnum++, buf);
    }

  /* Fill in argument registers.  */
  for (i = 0; i < nargs; i++)
    {
      struct value *arg = args[i];
      struct type *type = check_typedef (value_type (arg));
      const gdb_byte *contents = value_contents (arg);
      int len = TYPE_LENGTH (type);
      int n_regs = align_up (len, 16) / 16;

      /* If the argument doesn't wholly fit into registers, it and
	 all subsequent arguments go to the stack.  */
      if (regnum + n_regs - 1 > SPU_ARGN_REGNUM)
	{
	  stack_arg = i;
	  break;
	}

      spu_value_to_regcache (regcache, regnum, type, contents);
      regnum += n_regs;
    }

  /* Overflow arguments go to the stack.  */
  if (stack_arg != -1)
    {
      CORE_ADDR ap;

      /* Allocate all required stack size.  */
      for (i = stack_arg; i < nargs; i++)
	{
	  struct type *type = check_typedef (value_type (args[i]));
	  sp -= align_up (TYPE_LENGTH (type), 16);
	}

      /* Fill in stack arguments.  */
      ap = sp;
      for (i = stack_arg; i < nargs; i++)
	{
	  struct value *arg = args[i];
	  struct type *type = check_typedef (value_type (arg));
	  int len = TYPE_LENGTH (type);
	  int preferred_slot;
	  
	  if (spu_scalar_value_p (type))
	    preferred_slot = len < 4 ? 4 - len : 0;
	  else
	    preferred_slot = 0;

	  target_write_memory (ap + preferred_slot, value_contents (arg), len);
	  ap += align_up (TYPE_LENGTH (type), 16);
	}
    }

  /* Allocate stack frame header.  */
  sp -= 32;

  /* Store stack back chain.  */
  regcache_cooked_read (regcache, SPU_RAW_SP_REGNUM, buf);
  target_write_memory (sp, buf, 16);

  /* Finally, update the SP register.  */
  regcache_cooked_write_unsigned (regcache, SPU_SP_REGNUM, sp);

  return sp;
}

static struct frame_id
spu_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_id_build (spu_unwind_sp (gdbarch, next_frame),
			 spu_unwind_pc (gdbarch, next_frame));
}

/* Function return value access.  */

static enum return_value_convention
spu_return_value (struct gdbarch *gdbarch, struct type *type,
                  struct regcache *regcache, gdb_byte *out, const gdb_byte *in)
{
  enum return_value_convention rvc;

  if (TYPE_LENGTH (type) <= (SPU_ARGN_REGNUM - SPU_ARG1_REGNUM + 1) * 16)
    rvc = RETURN_VALUE_REGISTER_CONVENTION;
  else
    rvc = RETURN_VALUE_STRUCT_CONVENTION;

  if (in)
    {
      switch (rvc)
	{
	case RETURN_VALUE_REGISTER_CONVENTION:
	  spu_value_to_regcache (regcache, SPU_ARG1_REGNUM, type, in);
	  break;

	case RETURN_VALUE_STRUCT_CONVENTION:
	  error ("Cannot set function return value.");
	  break;
	}
    }
  else if (out)
    {
      switch (rvc)
	{
	case RETURN_VALUE_REGISTER_CONVENTION:
	  spu_regcache_to_value (regcache, SPU_ARG1_REGNUM, type, out);
	  break;

	case RETURN_VALUE_STRUCT_CONVENTION:
	  error ("Function return value unknown.");
	  break;
	}
    }

  return rvc;
}


/* Breakpoints.  */

static const gdb_byte *
spu_breakpoint_from_pc (CORE_ADDR * pcptr, int *lenptr)
{
  static const gdb_byte breakpoint[] = { 0x00, 0x00, 0x3f, 0xff };

  *lenptr = sizeof breakpoint;
  return breakpoint;
}


/* Software single-stepping support.  */

int
spu_software_single_step (struct frame_info *frame)
{
  CORE_ADDR pc, next_pc;
  unsigned int insn;
  int offset, reg;
  gdb_byte buf[4];

  pc = get_frame_pc (frame);

  if (target_read_memory (pc, buf, 4))
    return 1;
  insn = extract_unsigned_integer (buf, 4);

  /* Next sequential instruction is at PC + 4, except if the current
     instruction is a PPE-assisted call, in which case it is at PC + 8.
     Wrap around LS limit to be on the safe side.  */
  if ((insn & 0xffffff00) == 0x00002100)
    next_pc = (pc + 8) & (SPU_LS_SIZE - 1);
  else
    next_pc = (pc + 4) & (SPU_LS_SIZE - 1);

  insert_single_step_breakpoint (next_pc);

  if (is_branch (insn, &offset, &reg))
    {
      CORE_ADDR target = offset;

      if (reg == SPU_PC_REGNUM)
	target += pc;
      else if (reg != -1)
	{
	  get_frame_register_bytes (frame, reg, 0, 4, buf);
	  target += extract_unsigned_integer (buf, 4) & -4;
	}

      target = target & (SPU_LS_SIZE - 1);
      if (target != next_pc)
	insert_single_step_breakpoint (target);
    }

  return 1;
}

/* Target overlays for the SPU overlay manager.

   See the documentation of simple_overlay_update for how the
   interface is supposed to work.

   Data structures used by the overlay manager:

   struct ovly_table
     {
        u32 vma;
        u32 size;
        u32 pos;
        u32 buf;
     } _ovly_table[];   -- one entry per overlay section

   struct ovly_buf_table
     {
        u32 mapped;
     } _ovly_buf_table[];  -- one entry per overlay buffer

   _ovly_table should never change.

   Both tables are aligned to a 16-byte boundary, the symbols _ovly_table
   and _ovly_buf_table are of type STT_OBJECT and their size set to the size
   of the respective array. buf in _ovly_table is an index into _ovly_buf_table.

   mapped is an index into _ovly_table. Both the mapped and buf indices start
   from one to reference the first entry in their respective tables.  */

/* Using the per-objfile private data mechanism, we store for each
   objfile an array of "struct spu_overlay_table" structures, one
   for each obj_section of the objfile.  This structure holds two
   fields, MAPPED_PTR and MAPPED_VAL.  If MAPPED_PTR is zero, this
   is *not* an overlay section.  If it is non-zero, it represents
   a target address.  The overlay section is mapped iff the target
   integer at this location equals MAPPED_VAL.  */

static const struct objfile_data *spu_overlay_data;

struct spu_overlay_table
  {
    CORE_ADDR mapped_ptr;
    CORE_ADDR mapped_val;
  };

/* Retrieve the overlay table for OBJFILE.  If not already cached, read
   the _ovly_table data structure from the target and initialize the
   spu_overlay_table data structure from it.  */
static struct spu_overlay_table *
spu_get_overlay_table (struct objfile *objfile)
{
  struct minimal_symbol *ovly_table_msym, *ovly_buf_table_msym;
  CORE_ADDR ovly_table_base, ovly_buf_table_base;
  unsigned ovly_table_size, ovly_buf_table_size;
  struct spu_overlay_table *tbl;
  struct obj_section *osect;
  char *ovly_table;
  int i;

  tbl = objfile_data (objfile, spu_overlay_data);
  if (tbl)
    return tbl;

  ovly_table_msym = lookup_minimal_symbol ("_ovly_table", NULL, objfile);
  if (!ovly_table_msym)
    return NULL;

  ovly_buf_table_msym = lookup_minimal_symbol ("_ovly_buf_table", NULL, objfile);
  if (!ovly_buf_table_msym)
    return NULL;

  ovly_table_base = SYMBOL_VALUE_ADDRESS (ovly_table_msym);
  ovly_table_size = MSYMBOL_SIZE (ovly_table_msym);

  ovly_buf_table_base = SYMBOL_VALUE_ADDRESS (ovly_buf_table_msym);
  ovly_buf_table_size = MSYMBOL_SIZE (ovly_buf_table_msym);

  ovly_table = xmalloc (ovly_table_size);
  read_memory (ovly_table_base, ovly_table, ovly_table_size);

  tbl = OBSTACK_CALLOC (&objfile->objfile_obstack,
			objfile->sections_end - objfile->sections,
			struct spu_overlay_table);

  for (i = 0; i < ovly_table_size / 16; i++)
    {
      CORE_ADDR vma  = extract_unsigned_integer (ovly_table + 16*i + 0, 4);
      CORE_ADDR size = extract_unsigned_integer (ovly_table + 16*i + 4, 4);
      CORE_ADDR pos  = extract_unsigned_integer (ovly_table + 16*i + 8, 4);
      CORE_ADDR buf  = extract_unsigned_integer (ovly_table + 16*i + 12, 4);

      if (buf == 0 || (buf - 1) * 4 >= ovly_buf_table_size)
	continue;

      ALL_OBJFILE_OSECTIONS (objfile, osect)
	if (vma == bfd_section_vma (objfile->obfd, osect->the_bfd_section)
	    && pos == osect->the_bfd_section->filepos)
	  {
	    int ndx = osect - objfile->sections;
	    tbl[ndx].mapped_ptr = ovly_buf_table_base + (buf - 1) * 4;
	    tbl[ndx].mapped_val = i + 1;
	    break;
	  }
    }

  xfree (ovly_table);
  set_objfile_data (objfile, spu_overlay_data, tbl);
  return tbl;
}

/* Read _ovly_buf_table entry from the target to dermine whether
   OSECT is currently mapped, and update the mapped state.  */
static void
spu_overlay_update_osect (struct obj_section *osect)
{
  struct spu_overlay_table *ovly_table;
  CORE_ADDR val;

  ovly_table = spu_get_overlay_table (osect->objfile);
  if (!ovly_table)
    return;

  ovly_table += osect - osect->objfile->sections;
  if (ovly_table->mapped_ptr == 0)
    return;

  val = read_memory_unsigned_integer (ovly_table->mapped_ptr, 4);
  osect->ovly_mapped = (val == ovly_table->mapped_val);
}

/* If OSECT is NULL, then update all sections' mapped state.
   If OSECT is non-NULL, then update only OSECT's mapped state.  */
static void
spu_overlay_update (struct obj_section *osect)
{
  /* Just one section.  */
  if (osect)
    spu_overlay_update_osect (osect);

  /* All sections.  */
  else
    {
      struct objfile *objfile;

      ALL_OBJSECTIONS (objfile, osect)
	if (section_is_overlay (osect->the_bfd_section))
	  spu_overlay_update_osect (osect);
    }
}

/* Whenever a new objfile is loaded, read the target's _ovly_table.
   If there is one, go through all sections and make sure for non-
   overlay sections LMA equals VMA, while for overlay sections LMA
   is larger than local store size.  */
static void
spu_overlay_new_objfile (struct objfile *objfile)
{
  struct spu_overlay_table *ovly_table;
  struct obj_section *osect;

  /* If we've already touched this file, do nothing.  */
  if (!objfile || objfile_data (objfile, spu_overlay_data) != NULL)
    return;

  /* Check if this objfile has overlays.  */
  ovly_table = spu_get_overlay_table (objfile);
  if (!ovly_table)
    return;

  /* Now go and fiddle with all the LMAs.  */
  ALL_OBJFILE_OSECTIONS (objfile, osect)
    {
      bfd *obfd = objfile->obfd;
      asection *bsect = osect->the_bfd_section;
      int ndx = osect - objfile->sections;

      if (ovly_table[ndx].mapped_ptr == 0)
	bfd_section_lma (obfd, bsect) = bfd_section_vma (obfd, bsect);
      else
	bfd_section_lma (obfd, bsect) = bsect->filepos + SPU_LS_SIZE;
    }
}


/* "info spu" commands.  */

static void
info_spu_event_command (char *args, int from_tty)
{
  struct frame_info *frame = get_selected_frame (NULL);
  ULONGEST event_status = 0;
  ULONGEST event_mask = 0;
  struct cleanup *chain;
  gdb_byte buf[100];
  char annex[32];
  LONGEST len;
  int rc, id;

  id = get_frame_register_unsigned (frame, SPU_ID_REGNUM);

  xsnprintf (annex, sizeof annex, "%d/event_status", id);
  len = target_read (&current_target, TARGET_OBJECT_SPU, annex,
		     buf, 0, sizeof buf);
  if (len <= 0)
    error (_("Could not read event_status."));
  event_status = strtoulst (buf, NULL, 16);
 
  xsnprintf (annex, sizeof annex, "%d/event_mask", id);
  len = target_read (&current_target, TARGET_OBJECT_SPU, annex,
		     buf, 0, sizeof buf);
  if (len <= 0)
    error (_("Could not read event_mask."));
  event_mask = strtoulst (buf, NULL, 16);
 
  chain = make_cleanup_ui_out_tuple_begin_end (uiout, "SPUInfoEvent");

  if (ui_out_is_mi_like_p (uiout))
    {
      ui_out_field_fmt (uiout, "event_status",
			"0x%s", phex_nz (event_status, 4));
      ui_out_field_fmt (uiout, "event_mask",
			"0x%s", phex_nz (event_mask, 4));
    }
  else
    {
      printf_filtered (_("Event Status 0x%s\n"), phex (event_status, 4));
      printf_filtered (_("Event Mask   0x%s\n"), phex (event_mask, 4));
    }

  do_cleanups (chain);
}

static void
info_spu_signal_command (char *args, int from_tty)
{
  struct frame_info *frame = get_selected_frame (NULL);
  ULONGEST signal1 = 0;
  ULONGEST signal1_type = 0;
  int signal1_pending = 0;
  ULONGEST signal2 = 0;
  ULONGEST signal2_type = 0;
  int signal2_pending = 0;
  struct cleanup *chain;
  char annex[32];
  gdb_byte buf[100];
  LONGEST len;
  int rc, id;

  id = get_frame_register_unsigned (frame, SPU_ID_REGNUM);

  xsnprintf (annex, sizeof annex, "%d/signal1", id);
  len = target_read (&current_target, TARGET_OBJECT_SPU, annex, buf, 0, 4);
  if (len < 0)
    error (_("Could not read signal1."));
  else if (len == 4)
    {
      signal1 = extract_unsigned_integer (buf, 4);
      signal1_pending = 1;
    }
    
  xsnprintf (annex, sizeof annex, "%d/signal1_type", id);
  len = target_read (&current_target, TARGET_OBJECT_SPU, annex,
		     buf, 0, sizeof buf);
  if (len <= 0)
    error (_("Could not read signal1_type."));
  signal1_type = strtoulst (buf, NULL, 16);

  xsnprintf (annex, sizeof annex, "%d/signal2", id);
  len = target_read (&current_target, TARGET_OBJECT_SPU, annex, buf, 0, 4);
  if (len < 0)
    error (_("Could not read signal2."));
  else if (len == 4)
    {
      signal2 = extract_unsigned_integer (buf, 4);
      signal2_pending = 1;
    }
    
  xsnprintf (annex, sizeof annex, "%d/signal2_type", id);
  len = target_read (&current_target, TARGET_OBJECT_SPU, annex,
		     buf, 0, sizeof buf);
  if (len <= 0)
    error (_("Could not read signal2_type."));
  signal2_type = strtoulst (buf, NULL, 16);

  chain = make_cleanup_ui_out_tuple_begin_end (uiout, "SPUInfoSignal");

  if (ui_out_is_mi_like_p (uiout))
    {
      ui_out_field_int (uiout, "signal1_pending", signal1_pending);
      ui_out_field_fmt (uiout, "signal1", "0x%s", phex_nz (signal1, 4));
      ui_out_field_int (uiout, "signal1_type", signal1_type);
      ui_out_field_int (uiout, "signal2_pending", signal2_pending);
      ui_out_field_fmt (uiout, "signal2", "0x%s", phex_nz (signal2, 4));
      ui_out_field_int (uiout, "signal2_type", signal2_type);
    }
  else
    {
      if (signal1_pending)
	printf_filtered (_("Signal 1 control word 0x%s "), phex (signal1, 4));
      else
	printf_filtered (_("Signal 1 not pending "));

      if (signal1_type)
	printf_filtered (_("(Type Or)\n"));
      else
	printf_filtered (_("(Type Overwrite)\n"));

      if (signal2_pending)
	printf_filtered (_("Signal 2 control word 0x%s "), phex (signal2, 4));
      else
	printf_filtered (_("Signal 2 not pending "));

      if (signal2_type)
	printf_filtered (_("(Type Or)\n"));
      else
	printf_filtered (_("(Type Overwrite)\n"));
    }

  do_cleanups (chain);
}

static void
info_spu_mailbox_list (gdb_byte *buf, int nr,
		       const char *field, const char *msg)
{
  struct cleanup *chain;
  int i;

  if (nr <= 0)
    return;

  chain = make_cleanup_ui_out_table_begin_end (uiout, 1, nr, "mbox");

  ui_out_table_header (uiout, 32, ui_left, field, msg);
  ui_out_table_body (uiout);

  for (i = 0; i < nr; i++)
    {
      struct cleanup *val_chain;
      ULONGEST val;
      val_chain = make_cleanup_ui_out_tuple_begin_end (uiout, "mbox");
      val = extract_unsigned_integer (buf + 4*i, 4);
      ui_out_field_fmt (uiout, field, "0x%s", phex (val, 4));
      do_cleanups (val_chain);

      if (!ui_out_is_mi_like_p (uiout))
	printf_filtered ("\n");
    }

  do_cleanups (chain);
}

static void
info_spu_mailbox_command (char *args, int from_tty)
{
  struct frame_info *frame = get_selected_frame (NULL);
  struct cleanup *chain;
  char annex[32];
  gdb_byte buf[1024];
  LONGEST len;
  int i, id;

  id = get_frame_register_unsigned (frame, SPU_ID_REGNUM);

  chain = make_cleanup_ui_out_tuple_begin_end (uiout, "SPUInfoMailbox");

  xsnprintf (annex, sizeof annex, "%d/mbox_info", id);
  len = target_read (&current_target, TARGET_OBJECT_SPU, annex,
		     buf, 0, sizeof buf);
  if (len < 0)
    error (_("Could not read mbox_info."));

  info_spu_mailbox_list (buf, len / 4, "mbox", "SPU Outbound Mailbox");

  xsnprintf (annex, sizeof annex, "%d/ibox_info", id);
  len = target_read (&current_target, TARGET_OBJECT_SPU, annex,
		     buf, 0, sizeof buf);
  if (len < 0)
    error (_("Could not read ibox_info."));

  info_spu_mailbox_list (buf, len / 4, "ibox", "SPU Outbound Interrupt Mailbox");

  xsnprintf (annex, sizeof annex, "%d/wbox_info", id);
  len = target_read (&current_target, TARGET_OBJECT_SPU, annex,
		     buf, 0, sizeof buf);
  if (len < 0)
    error (_("Could not read wbox_info."));

  info_spu_mailbox_list (buf, len / 4, "wbox", "SPU Inbound Mailbox");

  do_cleanups (chain);
}

static ULONGEST
spu_mfc_get_bitfield (ULONGEST word, int first, int last)
{
  ULONGEST mask = ~(~(ULONGEST)0 << (last - first + 1));
  return (word >> (63 - last)) & mask;
}

static void
info_spu_dma_cmdlist (gdb_byte *buf, int nr)
{
  static char *spu_mfc_opcode[256] =
    {
    /* 00 */ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    /* 10 */ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    /* 20 */ "put", "putb", "putf", NULL, "putl", "putlb", "putlf", NULL,
             "puts", "putbs", "putfs", NULL, NULL, NULL, NULL, NULL,
    /* 30 */ "putr", "putrb", "putrf", NULL, "putrl", "putrlb", "putrlf", NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    /* 40 */ "get", "getb", "getf", NULL, "getl", "getlb", "getlf", NULL,
             "gets", "getbs", "getfs", NULL, NULL, NULL, NULL, NULL,
    /* 50 */ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    /* 60 */ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    /* 70 */ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    /* 80 */ "sdcrt", "sdcrtst", NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, "sdcrz", NULL, NULL, NULL, "sdcrst", NULL, "sdcrf",
    /* 90 */ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    /* a0 */ "sndsig", "sndsigb", "sndsigf", NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    /* b0 */ "putlluc", NULL, NULL, NULL, "putllc", NULL, NULL, NULL,
             "putqlluc", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    /* c0 */ "barrier", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             "mfceieio", NULL, NULL, NULL, "mfcsync", NULL, NULL, NULL,
    /* d0 */ "getllar", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    /* e0 */ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    /* f0 */ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
             NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    };

  struct cleanup *chain;
  int i;

  chain = make_cleanup_ui_out_table_begin_end (uiout, 10, nr, "dma_cmd");

  ui_out_table_header (uiout, 7, ui_left, "opcode", "Opcode");
  ui_out_table_header (uiout, 3, ui_left, "tag", "Tag");
  ui_out_table_header (uiout, 3, ui_left, "tid", "TId");
  ui_out_table_header (uiout, 3, ui_left, "rid", "RId");
  ui_out_table_header (uiout, 18, ui_left, "ea", "EA");
  ui_out_table_header (uiout, 7, ui_left, "lsa", "LSA");
  ui_out_table_header (uiout, 7, ui_left, "size", "Size");
  ui_out_table_header (uiout, 7, ui_left, "lstaddr", "LstAddr");
  ui_out_table_header (uiout, 7, ui_left, "lstsize", "LstSize");
  ui_out_table_header (uiout, 1, ui_left, "error_p", "E");

  ui_out_table_body (uiout);

  for (i = 0; i < nr; i++)
    {
      struct cleanup *cmd_chain;
      ULONGEST mfc_cq_dw0;
      ULONGEST mfc_cq_dw1;
      ULONGEST mfc_cq_dw2;
      ULONGEST mfc_cq_dw3;
      int mfc_cmd_opcode, mfc_cmd_tag, rclass_id, tclass_id;
      int lsa, size, list_lsa, list_size, mfc_lsa, mfc_size;
      ULONGEST mfc_ea;
      int list_valid_p, noop_valid_p, qw_valid_p, ea_valid_p, cmd_error_p;

      /* Decode contents of MFC Command Queue Context Save/Restore Registers.
	 See "Cell Broadband Engine Registers V1.3", section 3.3.2.1.  */

      mfc_cq_dw0 = extract_unsigned_integer (buf + 32*i, 8);
      mfc_cq_dw1 = extract_unsigned_integer (buf + 32*i + 8, 8);
      mfc_cq_dw2 = extract_unsigned_integer (buf + 32*i + 16, 8);
      mfc_cq_dw3 = extract_unsigned_integer (buf + 32*i + 24, 8);

      list_lsa = spu_mfc_get_bitfield (mfc_cq_dw0, 0, 14);
      list_size = spu_mfc_get_bitfield (mfc_cq_dw0, 15, 26);
      mfc_cmd_opcode = spu_mfc_get_bitfield (mfc_cq_dw0, 27, 34);
      mfc_cmd_tag = spu_mfc_get_bitfield (mfc_cq_dw0, 35, 39);
      list_valid_p = spu_mfc_get_bitfield (mfc_cq_dw0, 40, 40);
      rclass_id = spu_mfc_get_bitfield (mfc_cq_dw0, 41, 43);
      tclass_id = spu_mfc_get_bitfield (mfc_cq_dw0, 44, 46);

      mfc_ea = spu_mfc_get_bitfield (mfc_cq_dw1, 0, 51) << 12
		| spu_mfc_get_bitfield (mfc_cq_dw2, 25, 36);

      mfc_lsa = spu_mfc_get_bitfield (mfc_cq_dw2, 0, 13);
      mfc_size = spu_mfc_get_bitfield (mfc_cq_dw2, 14, 24);
      noop_valid_p = spu_mfc_get_bitfield (mfc_cq_dw2, 37, 37);
      qw_valid_p = spu_mfc_get_bitfield (mfc_cq_dw2, 38, 38);
      ea_valid_p = spu_mfc_get_bitfield (mfc_cq_dw2, 39, 39);
      cmd_error_p = spu_mfc_get_bitfield (mfc_cq_dw2, 40, 40);

      cmd_chain = make_cleanup_ui_out_tuple_begin_end (uiout, "cmd");

      if (spu_mfc_opcode[mfc_cmd_opcode])
	ui_out_field_string (uiout, "opcode", spu_mfc_opcode[mfc_cmd_opcode]);
      else
	ui_out_field_int (uiout, "opcode", mfc_cmd_opcode);

      ui_out_field_int (uiout, "tag", mfc_cmd_tag);
      ui_out_field_int (uiout, "tid", tclass_id);
      ui_out_field_int (uiout, "rid", rclass_id);

      if (ea_valid_p)
	ui_out_field_fmt (uiout, "ea", "0x%s", phex (mfc_ea, 8));
      else
	ui_out_field_skip (uiout, "ea");

      ui_out_field_fmt (uiout, "lsa", "0x%05x", mfc_lsa << 4);
      if (qw_valid_p)
	ui_out_field_fmt (uiout, "size", "0x%05x", mfc_size << 4);
      else
	ui_out_field_fmt (uiout, "size", "0x%05x", mfc_size);

      if (list_valid_p)
	{
	  ui_out_field_fmt (uiout, "lstaddr", "0x%05x", list_lsa << 3);
	  ui_out_field_fmt (uiout, "lstsize", "0x%05x", list_size << 3);
	}
      else
	{
	  ui_out_field_skip (uiout, "lstaddr");
	  ui_out_field_skip (uiout, "lstsize");
	}

      if (cmd_error_p)
	ui_out_field_string (uiout, "error_p", "*");
      else
	ui_out_field_skip (uiout, "error_p");

      do_cleanups (cmd_chain);

      if (!ui_out_is_mi_like_p (uiout))
	printf_filtered ("\n");
    }

  do_cleanups (chain);
}

static void
info_spu_dma_command (char *args, int from_tty)
{
  struct frame_info *frame = get_selected_frame (NULL);
  ULONGEST dma_info_type;
  ULONGEST dma_info_mask;
  ULONGEST dma_info_status;
  ULONGEST dma_info_stall_and_notify;
  ULONGEST dma_info_atomic_command_status;
  struct cleanup *chain;
  char annex[32];
  gdb_byte buf[1024];
  LONGEST len;
  int i, id;

  id = get_frame_register_unsigned (frame, SPU_ID_REGNUM);

  xsnprintf (annex, sizeof annex, "%d/dma_info", id);
  len = target_read (&current_target, TARGET_OBJECT_SPU, annex,
		     buf, 0, 40 + 16 * 32);
  if (len <= 0)
    error (_("Could not read dma_info."));

  dma_info_type = extract_unsigned_integer (buf, 8);
  dma_info_mask = extract_unsigned_integer (buf + 8, 8);
  dma_info_status = extract_unsigned_integer (buf + 16, 8);
  dma_info_stall_and_notify = extract_unsigned_integer (buf + 24, 8);
  dma_info_atomic_command_status = extract_unsigned_integer (buf + 32, 8);
  
  chain = make_cleanup_ui_out_tuple_begin_end (uiout, "SPUInfoDMA");

  if (ui_out_is_mi_like_p (uiout))
    {
      ui_out_field_fmt (uiout, "dma_info_type", "0x%s",
			phex_nz (dma_info_type, 4));
      ui_out_field_fmt (uiout, "dma_info_mask", "0x%s",
			phex_nz (dma_info_mask, 4));
      ui_out_field_fmt (uiout, "dma_info_status", "0x%s",
			phex_nz (dma_info_status, 4));
      ui_out_field_fmt (uiout, "dma_info_stall_and_notify", "0x%s",
			phex_nz (dma_info_stall_and_notify, 4));
      ui_out_field_fmt (uiout, "dma_info_atomic_command_status", "0x%s",
			phex_nz (dma_info_atomic_command_status, 4));
    }
  else
    {
      const char *query_msg;

      switch (dma_info_type)
	{
	case 0: query_msg = _("no query pending"); break;
	case 1: query_msg = _("'any' query pending"); break;
	case 2: query_msg = _("'all' query pending"); break;
	default: query_msg = _("undefined query type"); break;
	}

      printf_filtered (_("Tag-Group Status  0x%s\n"),
		       phex (dma_info_status, 4));
      printf_filtered (_("Tag-Group Mask    0x%s (%s)\n"),
		       phex (dma_info_mask, 4), query_msg);
      printf_filtered (_("Stall-and-Notify  0x%s\n"),
		       phex (dma_info_stall_and_notify, 4));
      printf_filtered (_("Atomic Cmd Status 0x%s\n"),
		       phex (dma_info_atomic_command_status, 4));
      printf_filtered ("\n");
    }

  info_spu_dma_cmdlist (buf + 40, 16);
  do_cleanups (chain);
}

static void
info_spu_proxydma_command (char *args, int from_tty)
{
  struct frame_info *frame = get_selected_frame (NULL);
  ULONGEST dma_info_type;
  ULONGEST dma_info_mask;
  ULONGEST dma_info_status;
  struct cleanup *chain;
  char annex[32];
  gdb_byte buf[1024];
  LONGEST len;
  int i, id;

  id = get_frame_register_unsigned (frame, SPU_ID_REGNUM);

  xsnprintf (annex, sizeof annex, "%d/proxydma_info", id);
  len = target_read (&current_target, TARGET_OBJECT_SPU, annex,
		     buf, 0, 24 + 8 * 32);
  if (len <= 0)
    error (_("Could not read proxydma_info."));

  dma_info_type = extract_unsigned_integer (buf, 8);
  dma_info_mask = extract_unsigned_integer (buf + 8, 8);
  dma_info_status = extract_unsigned_integer (buf + 16, 8);
  
  chain = make_cleanup_ui_out_tuple_begin_end (uiout, "SPUInfoProxyDMA");

  if (ui_out_is_mi_like_p (uiout))
    {
      ui_out_field_fmt (uiout, "proxydma_info_type", "0x%s",
			phex_nz (dma_info_type, 4));
      ui_out_field_fmt (uiout, "proxydma_info_mask", "0x%s",
			phex_nz (dma_info_mask, 4));
      ui_out_field_fmt (uiout, "proxydma_info_status", "0x%s",
			phex_nz (dma_info_status, 4));
    }
  else
    {
      const char *query_msg;

      switch (dma_info_type)
	{
	case 0: query_msg = _("no query pending"); break;
	case 1: query_msg = _("'any' query pending"); break;
	case 2: query_msg = _("'all' query pending"); break;
	default: query_msg = _("undefined query type"); break;
	}

      printf_filtered (_("Tag-Group Status  0x%s\n"),
		       phex (dma_info_status, 4));
      printf_filtered (_("Tag-Group Mask    0x%s (%s)\n"),
		       phex (dma_info_mask, 4), query_msg);
      printf_filtered ("\n");
    }

  info_spu_dma_cmdlist (buf + 24, 8);
  do_cleanups (chain);
}

static void
info_spu_command (char *args, int from_tty)
{
  printf_unfiltered (_("\"info spu\" must be followed by the name of an SPU facility.\n"));
  help_list (infospucmdlist, "info spu ", -1, gdb_stdout);
}


/* Set up gdbarch struct.  */

static struct gdbarch *
spu_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  struct gdbarch_tdep *tdep;

  /* Find a candidate among the list of pre-declared architectures.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  /* Is is for us?  */
  if (info.bfd_arch_info->mach != bfd_mach_spu)
    return NULL;

  /* Yes, create a new architecture.  */
  tdep = XCALLOC (1, struct gdbarch_tdep);
  gdbarch = gdbarch_alloc (&info, tdep);

  /* Disassembler.  */
  set_gdbarch_print_insn (gdbarch, print_insn_spu);

  /* Registers.  */
  set_gdbarch_num_regs (gdbarch, SPU_NUM_REGS);
  set_gdbarch_num_pseudo_regs (gdbarch, SPU_NUM_PSEUDO_REGS);
  set_gdbarch_sp_regnum (gdbarch, SPU_SP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, SPU_PC_REGNUM);
  set_gdbarch_read_pc (gdbarch, spu_read_pc);
  set_gdbarch_write_pc (gdbarch, spu_write_pc);
  set_gdbarch_register_name (gdbarch, spu_register_name);
  set_gdbarch_register_type (gdbarch, spu_register_type);
  set_gdbarch_pseudo_register_read (gdbarch, spu_pseudo_register_read);
  set_gdbarch_pseudo_register_write (gdbarch, spu_pseudo_register_write);
  set_gdbarch_value_from_register (gdbarch, spu_value_from_register);
  set_gdbarch_register_reggroup_p (gdbarch, spu_register_reggroup_p);

  /* Data types.  */
  set_gdbarch_char_signed (gdbarch, 0);
  set_gdbarch_ptr_bit (gdbarch, 32);
  set_gdbarch_addr_bit (gdbarch, 32);
  set_gdbarch_short_bit (gdbarch, 16);
  set_gdbarch_int_bit (gdbarch, 32);
  set_gdbarch_long_bit (gdbarch, 32);
  set_gdbarch_long_long_bit (gdbarch, 64);
  set_gdbarch_float_bit (gdbarch, 32);
  set_gdbarch_double_bit (gdbarch, 64);
  set_gdbarch_long_double_bit (gdbarch, 64);
  set_gdbarch_float_format (gdbarch, floatformats_ieee_single);
  set_gdbarch_double_format (gdbarch, floatformats_ieee_double);
  set_gdbarch_long_double_format (gdbarch, floatformats_ieee_double);

  /* Address conversion.  */
  set_gdbarch_pointer_to_address (gdbarch, spu_pointer_to_address);
  set_gdbarch_integer_to_address (gdbarch, spu_integer_to_address);

  /* Inferior function calls.  */
  set_gdbarch_call_dummy_location (gdbarch, ON_STACK);
  set_gdbarch_frame_align (gdbarch, spu_frame_align);
  set_gdbarch_push_dummy_call (gdbarch, spu_push_dummy_call);
  set_gdbarch_unwind_dummy_id (gdbarch, spu_unwind_dummy_id);
  set_gdbarch_return_value (gdbarch, spu_return_value);

  /* Frame handling.  */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  frame_unwind_append_sniffer (gdbarch, spu_frame_sniffer);
  frame_base_set_default (gdbarch, &spu_frame_base);
  set_gdbarch_unwind_pc (gdbarch, spu_unwind_pc);
  set_gdbarch_unwind_sp (gdbarch, spu_unwind_sp);
  set_gdbarch_virtual_frame_pointer (gdbarch, spu_virtual_frame_pointer);
  set_gdbarch_frame_args_skip (gdbarch, 0);
  set_gdbarch_skip_prologue (gdbarch, spu_skip_prologue);
  set_gdbarch_in_function_epilogue_p (gdbarch, spu_in_function_epilogue_p);

  /* Breakpoints.  */
  set_gdbarch_decr_pc_after_break (gdbarch, 4);
  set_gdbarch_breakpoint_from_pc (gdbarch, spu_breakpoint_from_pc);
  set_gdbarch_cannot_step_breakpoint (gdbarch, 1);
  set_gdbarch_software_single_step (gdbarch, spu_software_single_step);

  /* Overlays.  */
  set_gdbarch_overlay_update (gdbarch, spu_overlay_update);

  return gdbarch;
}

void
_initialize_spu_tdep (void)
{
  register_gdbarch_init (bfd_arch_spu, spu_gdbarch_init);

  /* Add ourselves to objfile event chain.  */
  observer_attach_new_objfile (spu_overlay_new_objfile);
  spu_overlay_data = register_objfile_data ();

  /* Add root prefix command for all "info spu" commands.  */
  add_prefix_cmd ("spu", class_info, info_spu_command,
		  _("Various SPU specific commands."),
		  &infospucmdlist, "info spu ", 0, &infolist);

  /* Add various "info spu" commands.  */
  add_cmd ("event", class_info, info_spu_event_command,
	   _("Display SPU event facility status.\n"),
	   &infospucmdlist);
  add_cmd ("signal", class_info, info_spu_signal_command,
	   _("Display SPU signal notification facility status.\n"),
	   &infospucmdlist);
  add_cmd ("mailbox", class_info, info_spu_mailbox_command,
	   _("Display SPU mailbox facility status.\n"),
	   &infospucmdlist);
  add_cmd ("dma", class_info, info_spu_dma_command,
	   _("Display MFC DMA status.\n"),
	   &infospucmdlist);
  add_cmd ("proxydma", class_info, info_spu_proxydma_command,
	   _("Display MFC Proxy-DMA status.\n"),
	   &infospucmdlist);
}
