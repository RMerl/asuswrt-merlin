/* Target-dependent code for the S+core architecture, for GDB,
   the GNU Debugger.

   Copyright (C) 2006, 2007 Free Software Foundation, Inc.

   Contributed by Qinwei (qinwei@sunnorth.com.cn)
   Contributed by Ching-Peng Lin (cplin@sunplus.com)

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
#include "gdb_assert.h"
#include "inferior.h"
#include "symtab.h"
#include "objfiles.h"
#include "gdbcore.h"
#include "target.h"
#include "arch-utils.h"
#include "regcache.h"
#include "dis-asm.h"
#include "frame-unwind.h"
#include "frame-base.h"
#include "trad-frame.h"
#include "dwarf2-frame.h"
#include "score-tdep.h"

#define G_FLD(_i,_ms,_ls)   (((_i) << (31 - (_ms))) >> (31 - (_ms) + (_ls)))
#define RM_PBITS(_raw)      ((G_FLD(_raw, 31, 16) << 15) | G_FLD(_raw, 14, 0))

typedef struct{
    unsigned int v;
    unsigned int raw;
    char         is15;
}inst_t;

struct score_frame_cache
{
  CORE_ADDR base;
  CORE_ADDR fp;
  struct trad_frame_saved_reg *saved_regs;
};

#if 0
/* If S+core GCC will generate these instructions in the prologue:

   lw   rx, imm1
   addi rx, -imm2
   mv!  r2, rx

   then .pdr section is used.  */

#define P_SIZE          8
#define PI_SYM          0
#define PI_R_MSK        1
#define PI_R_OFF        2
#define PI_R_LEF        4
#define PI_F_OFF        5
#define PI_F_REG        6
#define PI_RAREG        7

typedef struct frame_extra_info
{
  CORE_ADDR p_frame;
  unsigned int pdr[P_SIZE];
} extra_info_t;

struct obj_priv
{
  bfd_size_type size;
  char *contents;
};

static bfd *the_bfd;

static int
score_compare_pdr_entries (const void *a, const void *b)
{
  CORE_ADDR lhs = bfd_get_32 (the_bfd, (bfd_byte *) a);
  CORE_ADDR rhs = bfd_get_32 (the_bfd, (bfd_byte *) b);
  if (lhs < rhs)
    return -1;
  else if (lhs == rhs)
    return 0;
  else
    return 1;
}

static void
score_analyze_pdr_section (CORE_ADDR startaddr, CORE_ADDR pc,
                           struct frame_info *next_frame,
                           struct score_frame_cache *this_cache)
{
  struct symbol *sym;
  struct obj_section *sec;
  extra_info_t *fci_ext;
  CORE_ADDR leaf_ra_stack_addr = -1;

  gdb_assert (startaddr <= pc);
  gdb_assert (this_cache != NULL);

  fci_ext = frame_obstack_zalloc (sizeof (extra_info_t));
  if ((sec = find_pc_section (pc)) == NULL)
    {
      error ("Error: Can't find section in file:%s, line:%d!",
             __FILE__, __LINE__);
      return;
    }

  /* Anylyze .pdr section and get coresponding fields.  */
  {
    static struct obj_priv *priv = NULL;

    if (priv == NULL)
      {
        asection *bfdsec;
        priv = obstack_alloc (&sec->objfile->objfile_obstack,
                              sizeof (struct obj_priv));
        if ((bfdsec = bfd_get_section_by_name (sec->objfile->obfd, ".pdr")))
          {
            priv->size = bfd_section_size (sec->objfile->obfd, bfdsec);
            priv->contents = obstack_alloc (&sec->objfile->objfile_obstack,
                                            priv->size);
            bfd_get_section_contents (sec->objfile->obfd, bfdsec,
                                      priv->contents, 0, priv->size);
            the_bfd = sec->objfile->obfd;
            qsort (priv->contents, priv->size / 32, 32,
                   score_compare_pdr_entries);
            the_bfd = NULL;
          }
        else
          priv->size = 0;
      }
    if (priv->size != 0)
      {
        int low = 0, mid, high = priv->size / 32;
        char *ptr;
        do
          {
            CORE_ADDR pdr_pc;
            mid = (low + high) / 2;
            ptr = priv->contents + mid * 32;
            pdr_pc = bfd_get_signed_32 (sec->objfile->obfd, ptr);
            pdr_pc += ANOFFSET (sec->objfile->section_offsets,
                                SECT_OFF_TEXT (sec->objfile));
            if (pdr_pc == startaddr)
              break;
            if (pdr_pc > startaddr)
              high = mid;
            else
              low = mid + 1;
          }
        while (low != high);

        if (low != high)
          {
            gdb_assert (bfd_get_32 (sec->objfile->obfd, ptr) == startaddr);
#define EXT_PDR(_pi)    bfd_get_32(sec->objfile->obfd, ptr+((_pi)<<2))
            fci_ext->pdr[PI_SYM] = EXT_PDR (PI_SYM);
            fci_ext->pdr[PI_R_MSK] = EXT_PDR (PI_R_MSK);
            fci_ext->pdr[PI_R_OFF] = EXT_PDR (PI_R_OFF);
            fci_ext->pdr[PI_R_LEF] = EXT_PDR (PI_R_LEF);
            fci_ext->pdr[PI_F_OFF] = EXT_PDR (PI_F_OFF);
            fci_ext->pdr[PI_F_REG] = EXT_PDR (PI_F_REG);
            fci_ext->pdr[PI_RAREG] = EXT_PDR (PI_RAREG);
#undef EXT_PDR
          }
      }
  }
}
#endif

#if 0
/* Open these functions if build with simulator.  */

int
score_target_can_use_watch (int type, int cnt, int othertype)
{
  if (strcmp (current_target.to_shortname, "sim") == 0)
    {      
      return soc_gh_can_use_watch (type, cnt);
    }
  else
    {
      return (*current_target.to_can_use_hw_breakpoint) (type, cnt, othertype);
    }
}

int
score_stopped_by_watch (void)
{
  if (strcmp (current_target.to_shortname, "sim") == 0)
    {      
      return soc_gh_stopped_by_watch ();
    }
  else
    {
      return (*current_target.to_stopped_by_watchpoint) ();
    }
}

int
score_target_insert_watchpoint (CORE_ADDR addr, int len, int type)
{
  if (strcmp (current_target.to_shortname, "sim") == 0)
    {      
      return soc_gh_add_watch (addr, len, type);
    }
  else
    {
      return (*current_target.to_insert_watchpoint) (addr, len, type); 
    }
}

int
score_target_remove_watchpoint (CORE_ADDR addr, int len, int type)
{
  if (strcmp (current_target.to_shortname, "sim") == 0)
    {      
      return soc_gh_del_watch (addr, len, type);
    }
  else
    {
      return (*current_target.to_remove_watchpoint) (addr, len, type); 
    }
}

int
score_target_insert_hw_breakpoint (struct bp_target_info * bp_tgt)
{
  if (strcmp (current_target.to_shortname, "sim") == 0)
    {      
      return soc_gh_add_hardbp (bp_tgt->placed_address);
    }
  else
    {
      return (*current_target.to_insert_hw_breakpoint) (bp_tgt); 
    }
}

int
score_target_remove_hw_breakpoint (struct bp_target_info * bp_tgt)
{
  if (strcmp (current_target.to_shortname, "sim") == 0)
    {      
      return soc_gh_del_hardbp (bp_tgt->placed_address);
    }
  else
    {
      return (*current_target.to_remove_hw_breakpoint) (bp_tgt); 
    }
}
#endif

static struct type *
score_register_type (struct gdbarch *gdbarch, int regnum)
{
  gdb_assert (regnum >= 0 && regnum < SCORE_NUM_REGS);
  return builtin_type_uint32;
}

static CORE_ADDR
score_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, SCORE_PC_REGNUM);
}

static CORE_ADDR
score_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, SCORE_SP_REGNUM);
}

static const char *
score_register_name (int regnum)
{
  const char *score_register_names[] = {
    "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
    "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",
    "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
    "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",

    "PSR",   "COND",   "ECR",    "EXCPVEC",
    "CCR",   "EPC",    "EMA",    "TLBLOCK",
    "TLBPT", "PEADDR", "TLBRPT", "PEVN",
    "PECTX", "LIMPFN", "LDMPFN", "PREV",
    "DREG",  "PC",     "DSAVE",  "COUNTER",
    "LDCR",  "STCR",   "CEH",    "CEL",
  };

  gdb_assert (regnum >= 0 && regnum < SCORE_NUM_REGS);
  return score_register_names[regnum];
}

static int
score_register_sim_regno (int regnum)
{
  gdb_assert (regnum >= 0 && regnum < SCORE_NUM_REGS);
  return regnum;
}

static int
score_print_insn (bfd_vma memaddr, struct disassemble_info *info)
{
  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
    return print_insn_big_score (memaddr, info);
  else
    return print_insn_little_score (memaddr, info);
}

static const gdb_byte *
score_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  gdb_byte buf[SCORE_INSTLEN] = { 0 };
  int ret;
  unsigned int raw;

  if ((ret = target_read_memory (*pcptr & ~0x3, buf, SCORE_INSTLEN)) != 0)
    {
      error ("Error: target_read_memory in file:%s, line:%d!",
             __FILE__, __LINE__);
    }
  raw = extract_unsigned_integer (buf, SCORE_INSTLEN);

  if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG)
    {
      if (!(raw & 0x80008000))
        {
          /* 16bits instruction.  */
          static gdb_byte big_breakpoint16[] = { 0x60, 0x02 };
          *pcptr &= ~0x1;
          *lenptr = sizeof (big_breakpoint16);
          return big_breakpoint16;
        }
      else
        {
          /* 32bits instruction.  */
          static gdb_byte big_breakpoint32[] = { 0x80, 0x00, 0x80, 0x06 };
          *pcptr &= ~0x3;
          *lenptr = sizeof (big_breakpoint32);
          return big_breakpoint32;
        }
    }
  else
    {
      if (!(raw & 0x80008000))
        {
          /* 16bits instruction.  */
          static gdb_byte little_breakpoint16[] = { 0x02, 0x60 };
          *pcptr &= ~0x1;
          *lenptr = sizeof (little_breakpoint16);
          return little_breakpoint16;
        }
      else
        {
          /* 32bits instruction.  */
          static gdb_byte little_breakpoint32[] = { 0x06, 0x80, 0x00, 0x80 };
          *pcptr &= ~0x3;
          *lenptr = sizeof (little_breakpoint32);
          return little_breakpoint32;
        }
    }
}

static CORE_ADDR
score_frame_align (struct gdbarch *gdbarch, CORE_ADDR addr)
{
  return align_down (addr, 16);
}

static void
score_xfer_register (struct regcache *regcache, int regnum, int length,
                     enum bfd_endian endian, gdb_byte *readbuf,
                     const gdb_byte *writebuf, int buf_offset)
{
  int reg_offset = 0;
  gdb_assert (regnum >= 0 && regnum < SCORE_NUM_REGS);

  switch (endian)
    {
    case BFD_ENDIAN_BIG:
      reg_offset = SCORE_REGSIZE - length;
      break;
    case BFD_ENDIAN_LITTLE:
      reg_offset = 0;
      break;
    case BFD_ENDIAN_UNKNOWN:
      reg_offset = 0;
      break;
    default:
      error ("Error: score_xfer_register in file:%s, line:%d!",
             __FILE__, __LINE__);
    }

  if (readbuf != NULL)
    regcache_cooked_read_part (regcache, regnum, reg_offset, length,
                               readbuf + buf_offset);
  if (writebuf != NULL)
    regcache_cooked_write_part (regcache, regnum, reg_offset, length,
                                writebuf + buf_offset);
}

static enum return_value_convention
score_return_value (struct gdbarch *gdbarch, struct type *type,
                    struct regcache *regcache,
                    gdb_byte * readbuf, const gdb_byte * writebuf)
{
  if (TYPE_CODE (type) == TYPE_CODE_STRUCT
      || TYPE_CODE (type) == TYPE_CODE_UNION
      || TYPE_CODE (type) == TYPE_CODE_ARRAY)
    return RETURN_VALUE_STRUCT_CONVENTION;
  else
    {
      int offset;
      int regnum;
      for (offset = 0, regnum = SCORE_A0_REGNUM;
           offset < TYPE_LENGTH (type);
           offset += SCORE_REGSIZE, regnum++)
        {
          int xfer = SCORE_REGSIZE;
          if (offset + xfer > TYPE_LENGTH (type))
            xfer = TYPE_LENGTH (type) - offset;
          score_xfer_register (regcache, regnum, xfer,
			       gdbarch_byte_order (current_gdbarch),
                               readbuf, writebuf, offset);
        }
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
}

static struct frame_id
score_unwind_dummy_id (struct gdbarch *gdbarch,
                       struct frame_info *next_frame)
{
  return frame_id_build (
           frame_unwind_register_unsigned (next_frame, SCORE_SP_REGNUM),
           frame_pc_unwind (next_frame));
}

static int
score_type_needs_double_align (struct type *type)
{
  enum type_code typecode = TYPE_CODE (type);

  if ((typecode == TYPE_CODE_INT && TYPE_LENGTH (type) == 8)
      || (typecode == TYPE_CODE_FLT && TYPE_LENGTH (type) == 8))
    return 1;
  else if (typecode == TYPE_CODE_STRUCT || typecode == TYPE_CODE_UNION)
    {
      int i, n;

      n = TYPE_NFIELDS (type);
      for (i = 0; i < n; i++)
        if (score_type_needs_double_align (TYPE_FIELD_TYPE (type, i)))
          return 1;
      return 0;
    }
  return 0;
}

static CORE_ADDR
score_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
                       struct regcache *regcache, CORE_ADDR bp_addr,
                       int nargs, struct value **args, CORE_ADDR sp,
                       int struct_return, CORE_ADDR struct_addr)
{
  int argnum;
  int argreg;
  int arglen = 0;
  CORE_ADDR stack_offset = 0;
  CORE_ADDR addr = 0;

  /* Step 1, Save RA.  */
  regcache_cooked_write_unsigned (regcache, SCORE_RA_REGNUM, bp_addr);

  /* Step 2, Make space on the stack for the args.  */
  struct_addr = align_down (struct_addr, 16);
  sp = align_down (sp, 16);
  for (argnum = 0; argnum < nargs; argnum++)
    arglen += align_up (TYPE_LENGTH (value_type (args[argnum])),
                        SCORE_REGSIZE);
  sp -= align_up (arglen, 16);

  argreg = SCORE_BEGIN_ARG_REGNUM;

  /* Step 3, Check if struct return then save the struct address to
     r4 and increase the stack_offset by 4.  */
  if (struct_return)
    {
      regcache_cooked_write_unsigned (regcache, argreg++, struct_addr);
      stack_offset += SCORE_REGSIZE;
    }

  /* Step 4, Load arguments:
     If arg length is too long (> 4 bytes), then split the arg and
     save every parts.  */
  for (argnum = 0; argnum < nargs; argnum++)
    {
      struct value *arg = args[argnum];
      struct type *arg_type = check_typedef (value_type (arg));
      enum type_code typecode = TYPE_CODE (arg_type);
      const gdb_byte *val = value_contents (arg);
      int downward_offset = 0;
      int odd_sized_struct_p;
      int arg_last_part_p = 0;

      arglen = TYPE_LENGTH (arg_type);
      odd_sized_struct_p = (arglen > SCORE_REGSIZE
                            && arglen % SCORE_REGSIZE != 0);

      /* If a arg should be aligned to 8 bytes (long long or double),
         the value should be put to even register numbers.  */
      if (score_type_needs_double_align (arg_type))
        {
          if (argreg & 1)
            argreg++;
        }

      /* If sizeof a block < SCORE_REGSIZE, then Score GCC will chose
         the default "downward"/"upward" method:

         Example:

         struct struc
         {
           char a; char b; char c;
         } s = {'a', 'b', 'c'};

         Big endian:    s = {X, 'a', 'b', 'c'}
         Little endian: s = {'a', 'b', 'c', X}

         Where X is a hole.  */

      if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG
          && (typecode == TYPE_CODE_STRUCT
              || typecode == TYPE_CODE_UNION)
          && argreg > SCORE_LAST_ARG_REGNUM
          && arglen < SCORE_REGSIZE)
        downward_offset += (SCORE_REGSIZE - arglen);

      while (arglen > 0)
        {
          int partial_len = arglen < SCORE_REGSIZE ? arglen : SCORE_REGSIZE;
          ULONGEST regval = extract_unsigned_integer (val, partial_len);

          /* The last part of a arg should shift left when
             gdbarch_byte_order is BFD_ENDIAN_BIG.  */
          if (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG
              && arg_last_part_p == 1
              && (typecode == TYPE_CODE_STRUCT
                  || typecode == TYPE_CODE_UNION))
            regval <<= ((SCORE_REGSIZE - partial_len) * TARGET_CHAR_BIT);

          /* Always increase the stack_offset and save args to stack.  */
          addr = sp + stack_offset + downward_offset;
          write_memory (addr, val, partial_len);

          if (argreg <= SCORE_LAST_ARG_REGNUM)
            {
              regcache_cooked_write_unsigned (regcache, argreg++, regval);
              if (arglen > SCORE_REGSIZE && arglen < SCORE_REGSIZE * 2)
                arg_last_part_p = 1;
            }

          val += partial_len;
          arglen -= partial_len;
          stack_offset += align_up (partial_len, SCORE_REGSIZE);
        }
    }

  /* Step 5, Save SP.  */
  regcache_cooked_write_unsigned (regcache, SCORE_SP_REGNUM, sp);

  return sp;
}

static char *
score_malloc_and_get_memblock (CORE_ADDR addr, CORE_ADDR size)
{
  int ret;
  char *memblock = NULL;

  if (size < 0)
    {
      error ("Error: malloc size < 0 in file:%s, line:%d!",
             __FILE__, __LINE__);
      return NULL;
    }
  else if (size == 0)
    return NULL;

  memblock = (char *) xmalloc (size);
  memset (memblock, 0, size);
  ret = target_read_memory (addr & ~0x3, memblock, size);
  if (ret)
    {
      error ("Error: target_read_memory in file:%s, line:%d!",
             __FILE__, __LINE__);
      return NULL;
    }
  return memblock;
}

static void
score_free_memblock (char *memblock)
{
  xfree (memblock);
}

static void
score_adjust_memblock_ptr (char **memblock, CORE_ADDR prev_pc,
                           CORE_ADDR cur_pc)
{
  if (prev_pc == -1)
    {
      /* First time call this function, do nothing.  */
    }
  else if (cur_pc - prev_pc == 2 && (cur_pc & 0x3) == 0)
    {
      /* First 16-bit instruction, then 32-bit instruction.  */
      *memblock += SCORE_INSTLEN;
    }
  else if (cur_pc - prev_pc == 4)
    {
      /* Is 32-bit instruction, increase MEMBLOCK by 4.  */
      *memblock += SCORE_INSTLEN;
    }
}

static inst_t *
score_fetch_inst (CORE_ADDR addr, char *memblock)
{
  static inst_t inst = { 0, 0 };
  char buf[SCORE_INSTLEN] = { 0 };
  int big;
  int ret;

  if (target_has_execution && memblock != NULL)
    {
      /* Fetch instruction from local MEMBLOCK.  */
      memcpy (buf, memblock, SCORE_INSTLEN);
    }
  else
    {
      /* Fetch instruction from target.  */
      ret = target_read_memory (addr & ~0x3, buf, SCORE_INSTLEN);
      if (ret)
        {
          error ("Error: target_read_memory in file:%s, line:%d!",
                  __FILE__, __LINE__);
          return 0;
        }
    }

  inst.raw = extract_unsigned_integer (buf, SCORE_INSTLEN);
  inst.is15 = !(inst.raw & 0x80008000);
  inst.v = RM_PBITS (inst.raw);
  big = (gdbarch_byte_order (current_gdbarch) == BFD_ENDIAN_BIG);
  if (inst.is15)
    {
      if (big ^ ((addr & 0x2) == 2))
        inst.v = G_FLD (inst.v, 29, 15);
      else
        inst.v = G_FLD (inst.v, 14, 0);
    }
  return &inst;
}

static CORE_ADDR
score_skip_prologue (CORE_ADDR pc)
{
  CORE_ADDR cpc = pc;
  int iscan = 32, stack_sub = 0;
  while (iscan-- > 0)
    {
      inst_t *inst = score_fetch_inst (cpc, NULL);
      if (!inst)
        break;
      if (!inst->is15 && !stack_sub
          && (G_FLD (inst->v, 29, 25) == 0x1
              && G_FLD (inst->v, 24, 20) == 0x0))
        {
          /* addi r0, offset */
          pc = stack_sub = cpc + SCORE_INSTLEN;
        }
      else if (!inst->is15
               && inst->v == RM_PBITS (0x8040bc56))
        {
          /* mv r2, r0  */
          pc = cpc + SCORE_INSTLEN;
          break;
        }
      else if (inst->is15
               && inst->v == RM_PBITS (0x0203))
        {
          /* mv! r2, r0 */
          pc = cpc + SCORE16_INSTLEN;
          break;
        }
      else if (inst->is15
               && ((G_FLD (inst->v, 14, 12) == 3)    /* j15 form */
                   || (G_FLD (inst->v, 14, 12) == 4) /* b15 form */
                   || (G_FLD (inst->v, 14, 12) == 0x0
                       && G_FLD (inst->v, 3, 0) == 0x4))) /* br! */
        break;
      else if (!inst->is15
               && ((G_FLD (inst->v, 29, 25) == 2)    /* j32 form */
                   || (G_FLD (inst->v, 29, 25) == 4) /* b32 form */
                   || (G_FLD (inst->v, 29, 25) == 0x0
                       && G_FLD (inst->v, 6, 1) == 0x4)))  /* br */
        break;

      cpc += inst->is15 ? SCORE16_INSTLEN : SCORE_INSTLEN;
    }
  return pc;
}

static int
score_in_function_epilogue_p (struct gdbarch *gdbarch, CORE_ADDR cur_pc)
{
  inst_t *inst = score_fetch_inst (cur_pc, NULL);

  if (inst->v == 0x23)
    return 1;   /* mv! r0, r2 */
  else if (G_FLD (inst->v, 14, 12) == 0x2
           && G_FLD (inst->v, 3, 0) == 0xa)
    return 1;   /* pop! */
  else if (G_FLD (inst->v, 14, 12) == 0x0
           && G_FLD (inst->v, 7, 0) == 0x34)
    return 1;   /* br! r3 */
  else if (G_FLD (inst->v, 29, 15) == 0x2
           && G_FLD (inst->v, 6, 1) == 0x2b)
    return 1;   /* mv r0, r2 */
  else if (G_FLD (inst->v, 29, 25) == 0x0
           && G_FLD (inst->v, 6, 1) == 0x4
           && G_FLD (inst->v, 19, 15) == 0x3)
    return 1;   /* br r3 */
  else
    return 0;
}

static void
score_analyze_prologue (CORE_ADDR startaddr, CORE_ADDR pc,
                        struct frame_info *next_frame,
                        struct score_frame_cache *this_cache)
{
  CORE_ADDR sp;
  CORE_ADDR fp;
  CORE_ADDR cur_pc = startaddr;

  int sp_offset = 0;
  int ra_offset = 0;
  int fp_offset = 0;
  int ra_offset_p = 0;
  int fp_offset_p = 0;
  int inst_len = 0;

  char *memblock = NULL;
  char *memblock_ptr = NULL;
  CORE_ADDR prev_pc = -1;

  /* Allocate MEMBLOCK if PC - STARTADDR > 0.  */
  memblock_ptr = memblock =
    score_malloc_and_get_memblock (startaddr, pc - startaddr);

  sp = frame_unwind_register_unsigned (next_frame, SCORE_SP_REGNUM);
  fp = frame_unwind_register_unsigned (next_frame, SCORE_FP_REGNUM);

  for (; cur_pc < pc; prev_pc = cur_pc, cur_pc += inst_len)
    {
      inst_t *inst = NULL;
      if (memblock != NULL)
        {
          /* Reading memory block from target succefully and got all
             the instructions(from STARTADDR to PC) needed.  */
          score_adjust_memblock_ptr (&memblock, prev_pc, cur_pc);
          inst = score_fetch_inst (cur_pc, memblock);
        }
      else
        {
          /* Otherwise, we fetch 4 bytes from target, and GDB also
             work correctly.  */
          inst = score_fetch_inst (cur_pc, NULL);
        }

      if (inst->is15 == 1)
        {
          inst_len = SCORE16_INSTLEN;

          if (G_FLD (inst->v, 14, 12) == 0x2
              && G_FLD (inst->v, 3, 0) == 0xe)
            {
              /* push! */
              sp_offset += 4;

              if (G_FLD (inst->v, 11, 7) == 0x6
                  && ra_offset_p == 0)
                {
                  /* push! r3, [r0] */
                  ra_offset = sp_offset;
                  ra_offset_p = 1;
                }
              else if (G_FLD (inst->v, 11, 7) == 0x4
                       && fp_offset_p == 0)
                {
                  /* push! r2, [r0] */
                  fp_offset = sp_offset;
                  fp_offset_p = 1;
                }
            }
          else if (G_FLD (inst->v, 14, 12) == 0x2
                   && G_FLD (inst->v, 3, 0) == 0xa)
            {
              /* pop! */
              sp_offset -= 4;
            }
          else if (G_FLD (inst->v, 14, 7) == 0xc1
                   && G_FLD (inst->v, 2, 0) == 0x0)
            {
              /* subei! r0, n */
              sp_offset += (int) pow (2, G_FLD (inst->v, 6, 3));
            }
          else if (G_FLD (inst->v, 14, 7) == 0xc0
                   && G_FLD (inst->v, 2, 0) == 0x0)
            {
              /* addei! r0, n */
              sp_offset -= (int) pow (2, G_FLD (inst->v, 6, 3));
            }
        }
      else
        {
          inst_len = SCORE_INSTLEN;

          if (G_FLD (inst->v, 29, 15) == 0xc60
              && G_FLD (inst->v, 2, 0) == 0x4)
            {
              /* sw r3, [r0, offset]+ */
              sp_offset += SCORE_INSTLEN;
              if (ra_offset_p == 0)
                {
                  ra_offset = sp_offset;
                  ra_offset_p = 1;
                }
            }
          if (G_FLD (inst->v, 29, 15) == 0xc40
              && G_FLD (inst->v, 2, 0) == 0x4)
            {
              /* sw r2, [r0, offset]+ */
              sp_offset += SCORE_INSTLEN;
              if (fp_offset_p == 0)
                {
                  fp_offset = sp_offset;
                  fp_offset_p = 1;
                }
            }
          else if (G_FLD (inst->v, 29, 15) == 0x1c60
                   && G_FLD (inst->v, 2, 0) == 0x0)
            {
              /* lw r3, [r0]+, 4 */
              sp_offset -= SCORE_INSTLEN;
              ra_offset_p = 1;
            }
          else if (G_FLD (inst->v, 29, 15) == 0x1c40
                   && G_FLD (inst->v, 2, 0) == 0x0)
            {
              /* lw r2, [r0]+, 4 */
              sp_offset -= SCORE_INSTLEN;
              fp_offset_p = 1;
            }

          else if (G_FLD (inst->v, 29, 17) == 0x100
                   && G_FLD (inst->v, 0, 0) == 0x0)
            {
              /* addi r0, -offset */
              sp_offset += 65536 - G_FLD (inst->v, 16, 1);
            }
          else if (G_FLD (inst->v, 29, 17) == 0x110
                   && G_FLD (inst->v, 0, 0) == 0x0)
            {
              /* addi r2, offset */
              if (pc - cur_pc > 4)
                {
                  unsigned int save_v = inst->v;
                  inst_t *inst2 =
                    score_fetch_inst (cur_pc + SCORE_INSTLEN, NULL);
                  if (inst2->v == 0x23)
                    {
                      /* mv! r0, r2 */
                      sp_offset -= G_FLD (save_v, 16, 1);
                    }
                }
            }
        }
    }

  /* Save RA.  */
  if (ra_offset_p == 1)
    {
      if (this_cache->saved_regs[SCORE_PC_REGNUM].addr == -1)
        this_cache->saved_regs[SCORE_PC_REGNUM].addr =
          sp + sp_offset - ra_offset;
    }
  else
    {
      this_cache->saved_regs[SCORE_PC_REGNUM] =
        this_cache->saved_regs[SCORE_RA_REGNUM];
    }

  /* Save FP.  */
  if (fp_offset_p == 1)
    {
      if (this_cache->saved_regs[SCORE_FP_REGNUM].addr == -1)
        this_cache->saved_regs[SCORE_FP_REGNUM].addr =
          sp + sp_offset - fp_offset;
    }

  /* Save SP and FP.  */
  this_cache->base = sp + sp_offset;
  this_cache->fp = fp;

  /* Don't forget to free MEMBLOCK if we allocated it.  */
  if (memblock_ptr != NULL)
    score_free_memblock (memblock_ptr);
}

static struct score_frame_cache *
score_make_prologue_cache (struct frame_info *next_frame, void **this_cache)
{
  struct score_frame_cache *cache;

  if ((*this_cache) != NULL)
    return (*this_cache);

  cache = FRAME_OBSTACK_ZALLOC (struct score_frame_cache);
  (*this_cache) = cache;
  cache->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  /* Analyze the prologue.  */
  {
    const CORE_ADDR pc = frame_pc_unwind (next_frame);
    CORE_ADDR start_addr;

    find_pc_partial_function (pc, NULL, &start_addr, NULL);
    if (start_addr == 0)
      return cache;
    score_analyze_prologue (start_addr, pc, next_frame, *this_cache);
  }

  /* Save SP.  */
  trad_frame_set_value (cache->saved_regs, SCORE_SP_REGNUM, cache->base);

  return (*this_cache);
}

static void
score_prologue_this_id (struct frame_info *next_frame, void **this_cache,
                        struct frame_id *this_id)
{
  struct score_frame_cache *info = score_make_prologue_cache (next_frame,
                                                              this_cache);
  (*this_id) = frame_id_build (info->base,
                               frame_func_unwind (next_frame, NORMAL_FRAME));
}

static void
score_prologue_prev_register (struct frame_info *next_frame,
                              void **this_cache,
                              int regnum, int *optimizedp,
                              enum lval_type *lvalp, CORE_ADDR * addrp,
                              int *realnump, gdb_byte * valuep)
{
  struct score_frame_cache *info = score_make_prologue_cache (next_frame,
                                                              this_cache);
  trad_frame_get_prev_register (next_frame, info->saved_regs, regnum,
                                optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind score_prologue_unwind =
{
  NORMAL_FRAME,
  score_prologue_this_id,
  score_prologue_prev_register
};

static const struct frame_unwind *
score_prologue_sniffer (struct frame_info *next_frame)
{
  return &score_prologue_unwind;
}

static CORE_ADDR
score_prologue_frame_base_address (struct frame_info *next_frame,
                                   void **this_cache)
{
  struct score_frame_cache *info =
    score_make_prologue_cache (next_frame, this_cache);
  return info->fp;
}

static const struct frame_base score_prologue_frame_base =
{
  &score_prologue_unwind,
  score_prologue_frame_base_address,
  score_prologue_frame_base_address,
  score_prologue_frame_base_address,
};

static const struct frame_base *
score_prologue_frame_base_sniffer (struct frame_info *next_frame)
{
  return &score_prologue_frame_base;
}

static struct gdbarch *
score_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;

  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    {
      return (arches->gdbarch);
    }
  gdbarch = gdbarch_alloc (&info, 0);

  set_gdbarch_short_bit (gdbarch, 16);
  set_gdbarch_int_bit (gdbarch, 32);
  set_gdbarch_float_bit (gdbarch, 32);
  set_gdbarch_double_bit (gdbarch, 64);
  set_gdbarch_long_double_bit (gdbarch, 64);
  set_gdbarch_register_sim_regno (gdbarch, score_register_sim_regno);
  set_gdbarch_pc_regnum (gdbarch, SCORE_PC_REGNUM);
  set_gdbarch_sp_regnum (gdbarch, SCORE_SP_REGNUM);
  set_gdbarch_num_regs (gdbarch, SCORE_NUM_REGS);
  set_gdbarch_register_name (gdbarch, score_register_name);
  set_gdbarch_breakpoint_from_pc (gdbarch, score_breakpoint_from_pc);
  set_gdbarch_register_type (gdbarch, score_register_type);
  set_gdbarch_frame_align (gdbarch, score_frame_align);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_unwind_pc (gdbarch, score_unwind_pc);
  set_gdbarch_unwind_sp (gdbarch, score_unwind_sp);
  set_gdbarch_print_insn (gdbarch, score_print_insn);
  set_gdbarch_skip_prologue (gdbarch, score_skip_prologue);
  set_gdbarch_in_function_epilogue_p (gdbarch, score_in_function_epilogue_p);

  /* Watchpoint hooks.  */
  set_gdbarch_have_nonsteppable_watchpoint (gdbarch, 1);

  /* Dummy frame hooks.  */
  set_gdbarch_return_value (gdbarch, score_return_value);
  set_gdbarch_call_dummy_location (gdbarch, AT_ENTRY_POINT);
  set_gdbarch_unwind_dummy_id (gdbarch, score_unwind_dummy_id);
  set_gdbarch_push_dummy_call (gdbarch, score_push_dummy_call);

  /* Normal frame hooks.  */
  frame_unwind_append_sniffer (gdbarch, dwarf2_frame_sniffer);
  frame_base_append_sniffer (gdbarch, dwarf2_frame_base_sniffer);
  frame_unwind_append_sniffer (gdbarch, score_prologue_sniffer);
  frame_base_append_sniffer (gdbarch, score_prologue_frame_base_sniffer);

  return gdbarch;
}

extern initialize_file_ftype _initialize_score_tdep;

void
_initialize_score_tdep (void)
{
  gdbarch_register (bfd_arch_score, score_gdbarch_init, NULL);
}
