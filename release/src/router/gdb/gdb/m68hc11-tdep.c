/* Target-dependent code for Motorola 68HC11 & 68HC12

   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2007
   Free Software Foundation, Inc.

   Contributed by Stephane Carrez, stcarrez@nerim.fr

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
#include "dwarf2-frame.h"
#include "trad-frame.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "gdb_string.h"
#include "value.h"
#include "inferior.h"
#include "dis-asm.h"  
#include "symfile.h"
#include "objfiles.h"
#include "arch-utils.h"
#include "regcache.h"
#include "reggroups.h"

#include "target.h"
#include "opcode/m68hc11.h"
#include "elf/m68hc11.h"
#include "elf-bfd.h"

/* Macros for setting and testing a bit in a minimal symbol.
   For 68HC11/68HC12 we have two flags that tell which return
   type the function is using.  This is used for prologue and frame
   analysis to compute correct stack frame layout.
   
   The MSB of the minimal symbol's "info" field is used for this purpose.

   MSYMBOL_SET_RTC	Actually sets the "RTC" bit.
   MSYMBOL_SET_RTI	Actually sets the "RTI" bit.
   MSYMBOL_IS_RTC       Tests the "RTC" bit in a minimal symbol.
   MSYMBOL_IS_RTI       Tests the "RTC" bit in a minimal symbol.  */

#define MSYMBOL_SET_RTC(msym)                                           \
        MSYMBOL_INFO (msym) = (char *) (((long) MSYMBOL_INFO (msym))	\
					| 0x80000000)

#define MSYMBOL_SET_RTI(msym)                                           \
        MSYMBOL_INFO (msym) = (char *) (((long) MSYMBOL_INFO (msym))	\
					| 0x40000000)

#define MSYMBOL_IS_RTC(msym)				\
	(((long) MSYMBOL_INFO (msym) & 0x80000000) != 0)

#define MSYMBOL_IS_RTI(msym)				\
	(((long) MSYMBOL_INFO (msym) & 0x40000000) != 0)

enum insn_return_kind {
  RETURN_RTS,
  RETURN_RTC,
  RETURN_RTI
};

  
/* Register numbers of various important registers.  */

#define HARD_X_REGNUM 	0
#define HARD_D_REGNUM	1
#define HARD_Y_REGNUM   2
#define HARD_SP_REGNUM 	3
#define HARD_PC_REGNUM 	4

#define HARD_A_REGNUM   5
#define HARD_B_REGNUM   6
#define HARD_CCR_REGNUM 7

/* 68HC12 page number register.
   Note: to keep a compatibility with gcc register naming, we must
   not have to rename FP and other soft registers.  The page register
   is a real hard register and must therefore be counted by gdbarch_num_regs.
   For this it has the same number as Z register (which is not used).  */
#define HARD_PAGE_REGNUM 8
#define M68HC11_LAST_HARD_REG (HARD_PAGE_REGNUM)

/* Z is replaced by X or Y by gcc during machine reorg.
   ??? There is no way to get it and even know whether
   it's in X or Y or in ZS.  */
#define SOFT_Z_REGNUM        8

/* Soft registers.  These registers are special.  There are treated
   like normal hard registers by gcc and gdb (ie, within dwarf2 info).
   They are physically located in memory.  */
#define SOFT_FP_REGNUM       9
#define SOFT_TMP_REGNUM     10
#define SOFT_ZS_REGNUM      11
#define SOFT_XY_REGNUM      12
#define SOFT_UNUSED_REGNUM  13
#define SOFT_D1_REGNUM      14
#define SOFT_D32_REGNUM     (SOFT_D1_REGNUM+31)
#define M68HC11_MAX_SOFT_REGS 32

#define M68HC11_NUM_REGS        (8)
#define M68HC11_NUM_PSEUDO_REGS (M68HC11_MAX_SOFT_REGS+5)
#define M68HC11_ALL_REGS        (M68HC11_NUM_REGS+M68HC11_NUM_PSEUDO_REGS)

#define M68HC11_REG_SIZE    (2)

#define M68HC12_NUM_REGS        (9)
#define M68HC12_NUM_PSEUDO_REGS ((M68HC11_MAX_SOFT_REGS+5)+1-1)
#define M68HC12_HARD_PC_REGNUM  (SOFT_D32_REGNUM+1)

struct insn_sequence;
struct gdbarch_tdep
  {
    /* Stack pointer correction value.  For 68hc11, the stack pointer points
       to the next push location.  An offset of 1 must be applied to obtain
       the address where the last value is saved.  For 68hc12, the stack
       pointer points to the last value pushed.  No offset is necessary.  */
    int stack_correction;

    /* Description of instructions in the prologue.  */
    struct insn_sequence *prologue;

    /* True if the page memory bank register is available
       and must be used.  */
    int use_page_register;

    /* ELF flags for ABI.  */
    int elf_flags;
  };

#define M6811_TDEP gdbarch_tdep (current_gdbarch)
#define STACK_CORRECTION (M6811_TDEP->stack_correction)
#define USE_PAGE_REGISTER (M6811_TDEP->use_page_register)

struct m68hc11_unwind_cache
{
  /* The previous frame's inner most stack address.  Used as this
     frame ID's stack_addr.  */
  CORE_ADDR prev_sp;
  /* The frame's base, optionally used by the high-level debug info.  */
  CORE_ADDR base;
  CORE_ADDR pc;
  int size;
  int prologue_type;
  CORE_ADDR return_pc;
  CORE_ADDR sp_offset;
  int frameless;
  enum insn_return_kind return_kind;

  /* Table indicating the location of each and every register.  */
  struct trad_frame_saved_reg *saved_regs;
};

/* Table of registers for 68HC11.  This includes the hard registers
   and the soft registers used by GCC.  */
static char *
m68hc11_register_names[] =
{
  "x",    "d",    "y",    "sp",   "pc",   "a",    "b",
  "ccr",  "page", "frame","tmp",  "zs",   "xy",   0,
  "d1",   "d2",   "d3",   "d4",   "d5",   "d6",   "d7",
  "d8",   "d9",   "d10",  "d11",  "d12",  "d13",  "d14",
  "d15",  "d16",  "d17",  "d18",  "d19",  "d20",  "d21",
  "d22",  "d23",  "d24",  "d25",  "d26",  "d27",  "d28",
  "d29",  "d30",  "d31",  "d32"
};

struct m68hc11_soft_reg 
{
  const char *name;
  CORE_ADDR   addr;
};

static struct m68hc11_soft_reg soft_regs[M68HC11_ALL_REGS];

#define M68HC11_FP_ADDR soft_regs[SOFT_FP_REGNUM].addr

static int soft_min_addr;
static int soft_max_addr;
static int soft_reg_initialized = 0;

/* Look in the symbol table for the address of a pseudo register
   in memory.  If we don't find it, pretend the register is not used
   and not available.  */
static void
m68hc11_get_register_info (struct m68hc11_soft_reg *reg, const char *name)
{
  struct minimal_symbol *msymbol;

  msymbol = lookup_minimal_symbol (name, NULL, NULL);
  if (msymbol)
    {
      reg->addr = SYMBOL_VALUE_ADDRESS (msymbol);
      reg->name = xstrdup (name);

      /* Keep track of the address range for soft registers.  */
      if (reg->addr < (CORE_ADDR) soft_min_addr)
        soft_min_addr = reg->addr;
      if (reg->addr > (CORE_ADDR) soft_max_addr)
        soft_max_addr = reg->addr;
    }
  else
    {
      reg->name = 0;
      reg->addr = 0;
    }
}

/* Initialize the table of soft register addresses according
   to the symbol table.  */
  static void
m68hc11_initialize_register_info (void)
{
  int i;

  if (soft_reg_initialized)
    return;
  
  soft_min_addr = INT_MAX;
  soft_max_addr = 0;
  for (i = 0; i < M68HC11_ALL_REGS; i++)
    {
      soft_regs[i].name = 0;
    }
  
  m68hc11_get_register_info (&soft_regs[SOFT_FP_REGNUM], "_.frame");
  m68hc11_get_register_info (&soft_regs[SOFT_TMP_REGNUM], "_.tmp");
  m68hc11_get_register_info (&soft_regs[SOFT_ZS_REGNUM], "_.z");
  soft_regs[SOFT_Z_REGNUM] = soft_regs[SOFT_ZS_REGNUM];
  m68hc11_get_register_info (&soft_regs[SOFT_XY_REGNUM], "_.xy");

  for (i = SOFT_D1_REGNUM; i < M68HC11_MAX_SOFT_REGS; i++)
    {
      char buf[10];

      sprintf (buf, "_.d%d", i - SOFT_D1_REGNUM + 1);
      m68hc11_get_register_info (&soft_regs[i], buf);
    }

  if (soft_regs[SOFT_FP_REGNUM].name == 0)
    warning (_("No frame soft register found in the symbol table.\n"
	       "Stack backtrace will not work."));
  soft_reg_initialized = 1;
}

/* Given an address in memory, return the soft register number if
   that address corresponds to a soft register.  Returns -1 if not.  */
static int
m68hc11_which_soft_register (CORE_ADDR addr)
{
  int i;
  
  if (addr < soft_min_addr || addr > soft_max_addr)
    return -1;
  
  for (i = SOFT_FP_REGNUM; i < M68HC11_ALL_REGS; i++)
    {
      if (soft_regs[i].name && soft_regs[i].addr == addr)
        return i;
    }
  return -1;
}

/* Fetch a pseudo register.  The 68hc11 soft registers are treated like
   pseudo registers.  They are located in memory.  Translate the register
   fetch into a memory read.  */
static void
m68hc11_pseudo_register_read (struct gdbarch *gdbarch,
			      struct regcache *regcache,
			      int regno, gdb_byte *buf)
{
  /* The PC is a pseudo reg only for 68HC12 with the memory bank
     addressing mode.  */
  if (regno == M68HC12_HARD_PC_REGNUM)
    {
      ULONGEST pc;
      const int regsize = TYPE_LENGTH (builtin_type_uint32);

      regcache_cooked_read_unsigned (regcache, HARD_PC_REGNUM, &pc);
      if (pc >= 0x8000 && pc < 0xc000)
        {
          ULONGEST page;

          regcache_cooked_read_unsigned (regcache, HARD_PAGE_REGNUM, &page);
          pc -= 0x8000;
          pc += (page << 14);
          pc += 0x1000000;
        }
      store_unsigned_integer (buf, regsize, pc);
      return;
    }

  m68hc11_initialize_register_info ();
  
  /* Fetch a soft register: translate into a memory read.  */
  if (soft_regs[regno].name)
    {
      target_read_memory (soft_regs[regno].addr, buf, 2);
    }
  else
    {
      memset (buf, 0, 2);
    }
}

/* Store a pseudo register.  Translate the register store
   into a memory write.  */
static void
m68hc11_pseudo_register_write (struct gdbarch *gdbarch,
			       struct regcache *regcache,
			       int regno, const gdb_byte *buf)
{
  /* The PC is a pseudo reg only for 68HC12 with the memory bank
     addressing mode.  */
  if (regno == M68HC12_HARD_PC_REGNUM)
    {
      const int regsize = TYPE_LENGTH (builtin_type_uint32);
      char *tmp = alloca (regsize);
      CORE_ADDR pc;

      memcpy (tmp, buf, regsize);
      pc = extract_unsigned_integer (tmp, regsize);
      if (pc >= 0x1000000)
        {
          pc -= 0x1000000;
          regcache_cooked_write_unsigned (regcache, HARD_PAGE_REGNUM,
                                          (pc >> 14) & 0x0ff);
          pc &= 0x03fff;
          regcache_cooked_write_unsigned (regcache, HARD_PC_REGNUM,
                                          pc + 0x8000);
        }
      else
        regcache_cooked_write_unsigned (regcache, HARD_PC_REGNUM, pc);
      return;
    }
  
  m68hc11_initialize_register_info ();

  /* Store a soft register: translate into a memory write.  */
  if (soft_regs[regno].name)
    {
      const int regsize = 2;
      char *tmp = alloca (regsize);
      memcpy (tmp, buf, regsize);
      target_write_memory (soft_regs[regno].addr, tmp, regsize);
    }
}

static const char *
m68hc11_register_name (int reg_nr)
{
  if (reg_nr == M68HC12_HARD_PC_REGNUM && USE_PAGE_REGISTER)
    return "pc";
  if (reg_nr == HARD_PC_REGNUM && USE_PAGE_REGISTER)
    return "ppc";
  
  if (reg_nr < 0)
    return NULL;
  if (reg_nr >= M68HC11_ALL_REGS)
    return NULL;

  m68hc11_initialize_register_info ();

  /* If we don't know the address of a soft register, pretend it
     does not exist.  */
  if (reg_nr > M68HC11_LAST_HARD_REG && soft_regs[reg_nr].name == 0)
    return NULL;
  return m68hc11_register_names[reg_nr];
}

static const unsigned char *
m68hc11_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  static unsigned char breakpoint[] = {0x0};
  
  *lenptr = sizeof (breakpoint);
  return breakpoint;
}


/* 68HC11 & 68HC12 prologue analysis.

 */
#define MAX_CODES 12

/* 68HC11 opcodes.  */
#undef M6811_OP_PAGE2
#define M6811_OP_PAGE2   (0x18)
#define M6811_OP_LDX     (0xde)
#define M6811_OP_LDX_EXT (0xfe)
#define M6811_OP_PSHX    (0x3c)
#define M6811_OP_STS     (0x9f)
#define M6811_OP_STS_EXT (0xbf)
#define M6811_OP_TSX     (0x30)
#define M6811_OP_XGDX    (0x8f)
#define M6811_OP_ADDD    (0xc3)
#define M6811_OP_TXS     (0x35)
#define M6811_OP_DES     (0x34)

/* 68HC12 opcodes.  */
#define M6812_OP_PAGE2   (0x18)
#define M6812_OP_MOVW    (0x01)
#define M6812_PB_PSHW    (0xae)
#define M6812_OP_STS     (0x5f)
#define M6812_OP_STS_EXT (0x7f)
#define M6812_OP_LEAS    (0x1b)
#define M6812_OP_PSHX    (0x34)
#define M6812_OP_PSHY    (0x35)

/* Operand extraction.  */
#define OP_DIRECT      (0x100) /* 8-byte direct addressing.  */
#define OP_IMM_LOW     (0x200) /* Low part of 16-bit constant/address.  */
#define OP_IMM_HIGH    (0x300) /* High part of 16-bit constant/address.  */
#define OP_PBYTE       (0x400) /* 68HC12 indexed operand.  */

/* Identification of the sequence.  */
enum m6811_seq_type
{
  P_LAST = 0,
  P_SAVE_REG,  /* Save a register on the stack.  */
  P_SET_FRAME, /* Setup the frame pointer.  */
  P_LOCAL_1,   /* Allocate 1 byte for locals.  */
  P_LOCAL_2,   /* Allocate 2 bytes for locals.  */
  P_LOCAL_N    /* Allocate N bytes for locals.  */
};

struct insn_sequence {
  enum m6811_seq_type type;
  unsigned length;
  unsigned short code[MAX_CODES];
};

/* Sequence of instructions in the 68HC11 function prologue.  */
static struct insn_sequence m6811_prologue[] = {
  /* Sequences to save a soft-register.  */
  { P_SAVE_REG, 3, { M6811_OP_LDX, OP_DIRECT,
                     M6811_OP_PSHX } },
  { P_SAVE_REG, 5, { M6811_OP_PAGE2, M6811_OP_LDX, OP_DIRECT,
                     M6811_OP_PAGE2, M6811_OP_PSHX } },
  { P_SAVE_REG, 4, { M6811_OP_LDX_EXT, OP_IMM_HIGH, OP_IMM_LOW,
                     M6811_OP_PSHX } },
  { P_SAVE_REG, 6, { M6811_OP_PAGE2, M6811_OP_LDX_EXT, OP_IMM_HIGH, OP_IMM_LOW,
                     M6811_OP_PAGE2, M6811_OP_PSHX } },

  /* Sequences to allocate local variables.  */
  { P_LOCAL_N,  7, { M6811_OP_TSX,
                     M6811_OP_XGDX,
                     M6811_OP_ADDD, OP_IMM_HIGH, OP_IMM_LOW,
                     M6811_OP_XGDX,
                     M6811_OP_TXS } },
  { P_LOCAL_N, 11, { M6811_OP_PAGE2, M6811_OP_TSX,
                     M6811_OP_PAGE2, M6811_OP_XGDX,
                     M6811_OP_ADDD, OP_IMM_HIGH, OP_IMM_LOW,
                     M6811_OP_PAGE2, M6811_OP_XGDX,
                     M6811_OP_PAGE2, M6811_OP_TXS } },
  { P_LOCAL_1,  1, { M6811_OP_DES } },
  { P_LOCAL_2,  1, { M6811_OP_PSHX } },
  { P_LOCAL_2,  2, { M6811_OP_PAGE2, M6811_OP_PSHX } },

  /* Initialize the frame pointer.  */
  { P_SET_FRAME, 2, { M6811_OP_STS, OP_DIRECT } },
  { P_SET_FRAME, 3, { M6811_OP_STS_EXT, OP_IMM_HIGH, OP_IMM_LOW } },
  { P_LAST, 0, { 0 } }
};


/* Sequence of instructions in the 68HC12 function prologue.  */
static struct insn_sequence m6812_prologue[] = {  
  { P_SAVE_REG,  5, { M6812_OP_PAGE2, M6812_OP_MOVW, M6812_PB_PSHW,
                      OP_IMM_HIGH, OP_IMM_LOW } },
  { P_SET_FRAME, 2, { M6812_OP_STS, OP_DIRECT } },
  { P_SET_FRAME, 3, { M6812_OP_STS_EXT, OP_IMM_HIGH, OP_IMM_LOW } },
  { P_LOCAL_N,   2, { M6812_OP_LEAS, OP_PBYTE } },
  { P_LOCAL_2,   1, { M6812_OP_PSHX } },
  { P_LOCAL_2,   1, { M6812_OP_PSHY } },
  { P_LAST, 0 }
};


/* Analyze the sequence of instructions starting at the given address.
   Returns a pointer to the sequence when it is recognized and
   the optional value (constant/address) associated with it.  */
static struct insn_sequence *
m68hc11_analyze_instruction (struct insn_sequence *seq, CORE_ADDR pc,
                             CORE_ADDR *val)
{
  unsigned char buffer[MAX_CODES];
  unsigned bufsize;
  unsigned j;
  CORE_ADDR cur_val;
  short v = 0;

  bufsize = 0;
  for (; seq->type != P_LAST; seq++)
    {
      cur_val = 0;
      for (j = 0; j < seq->length; j++)
        {
          if (bufsize < j + 1)
            {
              buffer[bufsize] = read_memory_unsigned_integer (pc + bufsize,
                                                              1);
              bufsize++;
            }
          /* Continue while we match the opcode.  */
          if (seq->code[j] == buffer[j])
            continue;
          
          if ((seq->code[j] & 0xf00) == 0)
            break;
          
          /* Extract a sequence parameter (address or constant).  */
          switch (seq->code[j])
            {
            case OP_DIRECT:
              cur_val = (CORE_ADDR) buffer[j];
              break;

            case OP_IMM_HIGH:
              cur_val = cur_val & 0x0ff;
              cur_val |= (buffer[j] << 8);
              break;

            case OP_IMM_LOW:
              cur_val &= 0x0ff00;
              cur_val |= buffer[j];
              break;

            case OP_PBYTE:
              if ((buffer[j] & 0xE0) == 0x80)
                {
                  v = buffer[j] & 0x1f;
                  if (v & 0x10)
                    v |= 0xfff0;
                }
              else if ((buffer[j] & 0xfe) == 0xf0)
                {
                  v = read_memory_unsigned_integer (pc + j + 1, 1);
                  if (buffer[j] & 1)
                    v |= 0xff00;
                }
              else if (buffer[j] == 0xf2)
                {
                  v = read_memory_unsigned_integer (pc + j + 1, 2);
                }
              cur_val = v;
              break;
            }
        }

      /* We have a full match.  */
      if (j == seq->length)
        {
          *val = cur_val;
          return seq;
        }
    }
  return 0;
}

/* Return the instruction that the function at the PC is using.  */
static enum insn_return_kind
m68hc11_get_return_insn (CORE_ADDR pc)
{
  struct minimal_symbol *sym;

  /* A flag indicating that this is a STO_M68HC12_FAR or STO_M68HC12_INTERRUPT
     function is stored by elfread.c in the high bit of the info field.
     Use this to decide which instruction the function uses to return.  */
  sym = lookup_minimal_symbol_by_pc (pc);
  if (sym == 0)
    return RETURN_RTS;

  if (MSYMBOL_IS_RTC (sym))
    return RETURN_RTC;
  else if (MSYMBOL_IS_RTI (sym))
    return RETURN_RTI;
  else
    return RETURN_RTS;
}

/* Analyze the function prologue to find some information
   about the function:
    - the PC of the first line (for m68hc11_skip_prologue)
    - the offset of the previous frame saved address (from current frame)
    - the soft registers which are pushed.  */
static CORE_ADDR
m68hc11_scan_prologue (CORE_ADDR pc, CORE_ADDR current_pc,
                       struct m68hc11_unwind_cache *info)
{
  LONGEST save_addr;
  CORE_ADDR func_end;
  int size;
  int found_frame_point;
  int saved_reg;
  int done = 0;
  struct insn_sequence *seq_table;

  info->size = 0;
  info->sp_offset = 0;
  if (pc >= current_pc)
    return current_pc;

  size = 0;

  m68hc11_initialize_register_info ();
  if (pc == 0)
    {
      info->size = 0;
      return pc;
    }

  seq_table = gdbarch_tdep (current_gdbarch)->prologue;
  
  /* The 68hc11 stack is as follows:


     |           |
     +-----------+
     |           |
     | args      |
     |           |
     +-----------+
     | PC-return |
     +-----------+
     | Old frame |
     +-----------+
     |           |
     | Locals    |
     |           |
     +-----------+ <--- current frame
     |           |

     With most processors (like 68K) the previous frame can be computed
     easily because it is always at a fixed offset (see link/unlink).
     That is, locals are accessed with negative offsets, arguments are
     accessed with positive ones.  Since 68hc11 only supports offsets
     in the range [0..255], the frame is defined at the bottom of
     locals (see picture).

     The purpose of the analysis made here is to find out the size
     of locals in this function.  An alternative to this is to use
     DWARF2 info.  This would be better but I don't know how to
     access dwarf2 debug from this function.
     
     Walk from the function entry point to the point where we save
     the frame.  While walking instructions, compute the size of bytes
     which are pushed.  This gives us the index to access the previous
     frame.

     We limit the search to 128 bytes so that the algorithm is bounded
     in case of random and wrong code.  We also stop and abort if
     we find an instruction which is not supposed to appear in the
     prologue (as generated by gcc 2.95, 2.96).
  */
  func_end = pc + 128;
  found_frame_point = 0;
  info->size = 0;
  save_addr = 0;
  while (!done && pc + 2 < func_end)
    {
      struct insn_sequence *seq;
      CORE_ADDR val;

      seq = m68hc11_analyze_instruction (seq_table, pc, &val);
      if (seq == 0)
        break;

      /* If we are within the instruction group, we can't advance the
         pc nor the stack offset.  Otherwise the caller's stack computed
         from the current stack can be wrong.  */
      if (pc + seq->length > current_pc)
        break;

      pc = pc + seq->length;
      if (seq->type == P_SAVE_REG)
        {
          if (found_frame_point)
            {
              saved_reg = m68hc11_which_soft_register (val);
              if (saved_reg < 0)
                break;

              save_addr -= 2;
              if (info->saved_regs)
                info->saved_regs[saved_reg].addr = save_addr;
            }
          else
            {
              size += 2;
            }
        }
      else if (seq->type == P_SET_FRAME)
        {
          found_frame_point = 1;
          info->size = size;
        }
      else if (seq->type == P_LOCAL_1)
        {
          size += 1;
        }
      else if (seq->type == P_LOCAL_2)
        {
          size += 2;
        }
      else if (seq->type == P_LOCAL_N)
        {
          /* Stack pointer is decremented for the allocation.  */
          if (val & 0x8000)
            size -= (int) (val) | 0xffff0000;
          else
            size -= val;
        }
    }
  if (found_frame_point == 0)
    info->sp_offset = size;
  else
    info->sp_offset = -1;
  return pc;
}

static CORE_ADDR
m68hc11_skip_prologue (CORE_ADDR pc)
{
  CORE_ADDR func_addr, func_end;
  struct symtab_and_line sal;
  struct m68hc11_unwind_cache tmp_cache = { 0 };

  /* If we have line debugging information, then the end of the
     prologue should be the first assembly instruction of the
     first source line.  */
  if (find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    {
      sal = find_pc_line (func_addr, 0);
      if (sal.end && sal.end < func_end)
	return sal.end;
    }

  pc = m68hc11_scan_prologue (pc, (CORE_ADDR) -1, &tmp_cache);
  return pc;
}

static CORE_ADDR
m68hc11_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  ULONGEST pc;

  frame_unwind_unsigned_register (next_frame, gdbarch_pc_regnum (gdbarch),
                                  &pc);
  return pc;
}

/* Put here the code to store, into fi->saved_regs, the addresses of
   the saved registers of frame described by FRAME_INFO.  This
   includes special registers such as pc and fp saved in special ways
   in the stack frame.  sp is even more special: the address we return
   for it IS the sp for the next frame. */

struct m68hc11_unwind_cache *
m68hc11_frame_unwind_cache (struct frame_info *next_frame,
                            void **this_prologue_cache)
{
  ULONGEST prev_sp;
  ULONGEST this_base;
  struct m68hc11_unwind_cache *info;
  CORE_ADDR current_pc;
  int i;

  if ((*this_prologue_cache))
    return (*this_prologue_cache);

  info = FRAME_OBSTACK_ZALLOC (struct m68hc11_unwind_cache);
  (*this_prologue_cache) = info;
  info->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  info->pc = frame_func_unwind (next_frame, NORMAL_FRAME);

  info->size = 0;
  info->return_kind = m68hc11_get_return_insn (info->pc);

  /* The SP was moved to the FP.  This indicates that a new frame
     was created.  Get THIS frame's FP value by unwinding it from
     the next frame.  */
  frame_unwind_unsigned_register (next_frame, SOFT_FP_REGNUM, &this_base);
  if (this_base == 0)
    {
      info->base = 0;
      return info;
    }

  current_pc = frame_pc_unwind (next_frame);
  if (info->pc != 0)
    m68hc11_scan_prologue (info->pc, current_pc, info);

  info->saved_regs[HARD_PC_REGNUM].addr = info->size;

  if (info->sp_offset != (CORE_ADDR) -1)
    {
      info->saved_regs[HARD_PC_REGNUM].addr = info->sp_offset;
      frame_unwind_unsigned_register (next_frame, HARD_SP_REGNUM, &this_base);
      prev_sp = this_base + info->sp_offset + 2;
      this_base += STACK_CORRECTION;
    }
  else
    {
      /* The FP points at the last saved register.  Adjust the FP back
         to before the first saved register giving the SP.  */
      prev_sp = this_base + info->size + 2;

      this_base += STACK_CORRECTION;
      if (soft_regs[SOFT_FP_REGNUM].name)
        info->saved_regs[SOFT_FP_REGNUM].addr = info->size - 2;
   }

  if (info->return_kind == RETURN_RTC)
    {
      prev_sp += 1;
      info->saved_regs[HARD_PAGE_REGNUM].addr = info->size;
      info->saved_regs[HARD_PC_REGNUM].addr = info->size + 1;
    }
  else if (info->return_kind == RETURN_RTI)
    {
      prev_sp += 7;
      info->saved_regs[HARD_CCR_REGNUM].addr = info->size;
      info->saved_regs[HARD_D_REGNUM].addr = info->size + 1;
      info->saved_regs[HARD_X_REGNUM].addr = info->size + 3;
      info->saved_regs[HARD_Y_REGNUM].addr = info->size + 5;
      info->saved_regs[HARD_PC_REGNUM].addr = info->size + 7;
    }

  /* Add 1 here to adjust for the post-decrement nature of the push
     instruction.*/
  info->prev_sp = prev_sp;

  info->base = this_base;

  /* Adjust all the saved registers so that they contain addresses and not
     offsets.  */
  for (i = 0;
       i < gdbarch_num_regs (current_gdbarch)
	   + gdbarch_num_pseudo_regs (current_gdbarch) - 1;
       i++)
    if (trad_frame_addr_p (info->saved_regs, i))
      {
        info->saved_regs[i].addr += this_base;
      }

  /* The previous frame's SP needed to be computed.  Save the computed
     value.  */
  trad_frame_set_value (info->saved_regs, HARD_SP_REGNUM, info->prev_sp);

  return info;
}

/* Given a GDB frame, determine the address of the calling function's
   frame.  This will be used to create a new GDB frame struct.  */

static void
m68hc11_frame_this_id (struct frame_info *next_frame,
                       void **this_prologue_cache,
                       struct frame_id *this_id)
{
  struct m68hc11_unwind_cache *info
    = m68hc11_frame_unwind_cache (next_frame, this_prologue_cache);
  CORE_ADDR base;
  CORE_ADDR func;
  struct frame_id id;

  /* The FUNC is easy.  */
  func = frame_func_unwind (next_frame, NORMAL_FRAME);

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
m68hc11_frame_prev_register (struct frame_info *next_frame,
                             void **this_prologue_cache,
                             int regnum, int *optimizedp,
                             enum lval_type *lvalp, CORE_ADDR *addrp,
                             int *realnump, gdb_byte *bufferp)
{
  struct m68hc11_unwind_cache *info
    = m68hc11_frame_unwind_cache (next_frame, this_prologue_cache);

  trad_frame_get_prev_register (next_frame, info->saved_regs, regnum,
				optimizedp, lvalp, addrp, realnump, bufferp);

  if (regnum == HARD_PC_REGNUM)
    {
      /* Take into account the 68HC12 specific call (PC + page).  */
      if (info->return_kind == RETURN_RTC
          && *addrp >= 0x08000 && *addrp < 0x0c000
          && USE_PAGE_REGISTER)
        {
          int page_optimized;

          CORE_ADDR page;

          trad_frame_get_prev_register (next_frame, info->saved_regs,
					HARD_PAGE_REGNUM, &page_optimized,
					0, &page, 0, 0);
          *addrp -= 0x08000;
          *addrp += ((page & 0x0ff) << 14);
          *addrp += 0x1000000;
        }
    }
}

static const struct frame_unwind m68hc11_frame_unwind = {
  NORMAL_FRAME,
  m68hc11_frame_this_id,
  m68hc11_frame_prev_register
};

const struct frame_unwind *
m68hc11_frame_sniffer (struct frame_info *next_frame)
{
  return &m68hc11_frame_unwind;
}

static CORE_ADDR
m68hc11_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct m68hc11_unwind_cache *info
    = m68hc11_frame_unwind_cache (next_frame, this_cache);

  return info->base;
}

static CORE_ADDR
m68hc11_frame_args_address (struct frame_info *next_frame, void **this_cache)
{
  CORE_ADDR addr;
  struct m68hc11_unwind_cache *info
    = m68hc11_frame_unwind_cache (next_frame, this_cache);

  addr = info->base + info->size;
  if (info->return_kind == RETURN_RTC)
    addr += 1;
  else if (info->return_kind == RETURN_RTI)
    addr += 7;

  return addr;
}

static const struct frame_base m68hc11_frame_base = {
  &m68hc11_frame_unwind,
  m68hc11_frame_base_address,
  m68hc11_frame_base_address,
  m68hc11_frame_args_address
};

static CORE_ADDR
m68hc11_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  ULONGEST sp;
  frame_unwind_unsigned_register (next_frame, HARD_SP_REGNUM, &sp);
  return sp;
}

/* Assuming NEXT_FRAME->prev is a dummy, return the frame ID of that
   dummy frame.  The frame ID's base needs to match the TOS value
   saved by save_dummy_frame_tos(), and the PC match the dummy frame's
   breakpoint.  */

static struct frame_id
m68hc11_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  ULONGEST tos;
  CORE_ADDR pc = frame_pc_unwind (next_frame);

  frame_unwind_unsigned_register (next_frame, SOFT_FP_REGNUM, &tos);
  tos += 2;
  return frame_id_build (tos, pc);
}


/* Get and print the register from the given frame.  */
static void
m68hc11_print_register (struct gdbarch *gdbarch, struct ui_file *file,
                        struct frame_info *frame, int regno)
{
  LONGEST rval;

  if (regno == HARD_PC_REGNUM || regno == HARD_SP_REGNUM
      || regno == SOFT_FP_REGNUM || regno == M68HC12_HARD_PC_REGNUM)
    rval = get_frame_register_unsigned (frame, regno);
  else
    rval = get_frame_register_signed (frame, regno);

  if (regno == HARD_A_REGNUM || regno == HARD_B_REGNUM
      || regno == HARD_CCR_REGNUM || regno == HARD_PAGE_REGNUM)
    {
      fprintf_filtered (file, "0x%02x   ", (unsigned char) rval);
      if (regno != HARD_CCR_REGNUM)
        print_longest (file, 'd', 1, rval);
    }
  else
    {
      if (regno == HARD_PC_REGNUM && gdbarch_tdep (gdbarch)->use_page_register)
        {
          ULONGEST page;

          page = get_frame_register_unsigned (frame, HARD_PAGE_REGNUM);
          fprintf_filtered (file, "0x%02x:%04x ", (unsigned) page,
                            (unsigned) rval);
        }
      else
        {
          fprintf_filtered (file, "0x%04x ", (unsigned) rval);
          if (regno != HARD_PC_REGNUM && regno != HARD_SP_REGNUM
              && regno != SOFT_FP_REGNUM && regno != M68HC12_HARD_PC_REGNUM)
            print_longest (file, 'd', 1, rval);
        }
    }

  if (regno == HARD_CCR_REGNUM)
    {
      /* CCR register */
      int C, Z, N, V;
      unsigned char l = rval & 0xff;

      fprintf_filtered (file, "%c%c%c%c%c%c%c%c   ",
                        l & M6811_S_BIT ? 'S' : '-',
                        l & M6811_X_BIT ? 'X' : '-',
                        l & M6811_H_BIT ? 'H' : '-',
                        l & M6811_I_BIT ? 'I' : '-',
                        l & M6811_N_BIT ? 'N' : '-',
                        l & M6811_Z_BIT ? 'Z' : '-',
                        l & M6811_V_BIT ? 'V' : '-',
                        l & M6811_C_BIT ? 'C' : '-');
      N = (l & M6811_N_BIT) != 0;
      Z = (l & M6811_Z_BIT) != 0;
      V = (l & M6811_V_BIT) != 0;
      C = (l & M6811_C_BIT) != 0;

      /* Print flags following the h8300  */
      if ((C | Z) == 0)
	fprintf_filtered (file, "u> ");
      else if ((C | Z) == 1)
	fprintf_filtered (file, "u<= ");
      else if (C == 0)
	fprintf_filtered (file, "u< ");

      if (Z == 0)
	fprintf_filtered (file, "!= ");
      else
	fprintf_filtered (file, "== ");

      if ((N ^ V) == 0)
	fprintf_filtered (file, ">= ");
      else
	fprintf_filtered (file, "< ");

      if ((Z | (N ^ V)) == 0)
	fprintf_filtered (file, "> ");
      else
	fprintf_filtered (file, "<= ");
    }
}

/* Same as 'info reg' but prints the registers in a different way.  */
static void
m68hc11_print_registers_info (struct gdbarch *gdbarch, struct ui_file *file,
                              struct frame_info *frame, int regno, int cpregs)
{
  if (regno >= 0)
    {
      const char *name = gdbarch_register_name (gdbarch, regno);

      if (!name || !*name)
        return;

      fprintf_filtered (file, "%-10s ", name);
      m68hc11_print_register (gdbarch, file, frame, regno);
      fprintf_filtered (file, "\n");
    }
  else
    {
      int i, nr;

      fprintf_filtered (file, "PC=");
      m68hc11_print_register (gdbarch, file, frame, HARD_PC_REGNUM);

      fprintf_filtered (file, " SP=");
      m68hc11_print_register (gdbarch, file, frame, HARD_SP_REGNUM);

      fprintf_filtered (file, " FP=");
      m68hc11_print_register (gdbarch, file, frame, SOFT_FP_REGNUM);

      fprintf_filtered (file, "\nCCR=");
      m68hc11_print_register (gdbarch, file, frame, HARD_CCR_REGNUM);
      
      fprintf_filtered (file, "\nD=");
      m68hc11_print_register (gdbarch, file, frame, HARD_D_REGNUM);

      fprintf_filtered (file, " X=");
      m68hc11_print_register (gdbarch, file, frame, HARD_X_REGNUM);

      fprintf_filtered (file, " Y=");
      m68hc11_print_register (gdbarch, file, frame, HARD_Y_REGNUM);
  
      if (gdbarch_tdep (gdbarch)->use_page_register)
        {
          fprintf_filtered (file, "\nPage=");
          m68hc11_print_register (gdbarch, file, frame, HARD_PAGE_REGNUM);
        }
      fprintf_filtered (file, "\n");

      nr = 0;
      for (i = SOFT_D1_REGNUM; i < M68HC11_ALL_REGS; i++)
        {
          /* Skip registers which are not defined in the symbol table.  */
          if (soft_regs[i].name == 0)
            continue;
          
          fprintf_filtered (file, "D%d=", i - SOFT_D1_REGNUM + 1);
          m68hc11_print_register (gdbarch, file, frame, i);
          nr++;
          if ((nr % 8) == 7)
            fprintf_filtered (file, "\n");
          else
            fprintf_filtered (file, " ");
        }
      if (nr && (nr % 8) != 7)
        fprintf_filtered (file, "\n");
    }
}

static CORE_ADDR
m68hc11_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
                         struct regcache *regcache, CORE_ADDR bp_addr,
                         int nargs, struct value **args, CORE_ADDR sp,
                         int struct_return, CORE_ADDR struct_addr)
{
  int argnum;
  int first_stack_argnum;
  struct type *type;
  char *val;
  int len;
  char buf[2];
  
  first_stack_argnum = 0;
  if (struct_return)
    {
      regcache_cooked_write_unsigned (regcache, HARD_D_REGNUM, struct_addr);
    }
  else if (nargs > 0)
    {
      type = value_type (args[0]);
      len = TYPE_LENGTH (type);

      /* First argument is passed in D and X registers.  */
      if (len <= 4)
        {
          ULONGEST v;

          v = extract_unsigned_integer (value_contents (args[0]), len);
          first_stack_argnum = 1;

          regcache_cooked_write_unsigned (regcache, HARD_D_REGNUM, v);
          if (len > 2)
            {
              v >>= 16;
              regcache_cooked_write_unsigned (regcache, HARD_X_REGNUM, v);
            }
        }
    }

  for (argnum = nargs - 1; argnum >= first_stack_argnum; argnum--)
    {
      type = value_type (args[argnum]);
      len = TYPE_LENGTH (type);

      if (len & 1)
        {
          static char zero = 0;

          sp--;
          write_memory (sp, &zero, 1);
        }
      val = (char*) value_contents (args[argnum]);
      sp -= len;
      write_memory (sp, val, len);
    }

  /* Store return address.  */
  sp -= 2;
  store_unsigned_integer (buf, 2, bp_addr);
  write_memory (sp, buf, 2);

  /* Finally, update the stack pointer...  */
  sp -= STACK_CORRECTION;
  regcache_cooked_write_unsigned (regcache, HARD_SP_REGNUM, sp);

  /* ...and fake a frame pointer.  */
  regcache_cooked_write_unsigned (regcache, SOFT_FP_REGNUM, sp);

  /* DWARF2/GCC uses the stack address *before* the function call as a
     frame's CFA.  */
  return sp + 2;
}


/* Return the GDB type object for the "standard" data type
   of data in register N.  */

static struct type *
m68hc11_register_type (struct gdbarch *gdbarch, int reg_nr)
{
  switch (reg_nr)
    {
    case HARD_PAGE_REGNUM:
    case HARD_A_REGNUM:
    case HARD_B_REGNUM:
    case HARD_CCR_REGNUM:
      return builtin_type_uint8;

    case M68HC12_HARD_PC_REGNUM:
      return builtin_type_uint32;

    default:
      return builtin_type_uint16;
    }
}

static void
m68hc11_store_return_value (struct type *type, struct regcache *regcache,
                            const void *valbuf)
{
  int len;

  len = TYPE_LENGTH (type);

  /* First argument is passed in D and X registers.  */
  if (len <= 2)
    regcache_raw_write_part (regcache, HARD_D_REGNUM, 2 - len, len, valbuf);
  else if (len <= 4)
    {
      regcache_raw_write_part (regcache, HARD_X_REGNUM, 4 - len,
                               len - 2, valbuf);
      regcache_raw_write (regcache, HARD_D_REGNUM, (char*) valbuf + (len - 2));
    }
  else
    error (_("return of value > 4 is not supported."));
}


/* Given a return value in `regcache' with a type `type', 
   extract and copy its value into `valbuf'.  */

static void
m68hc11_extract_return_value (struct type *type, struct regcache *regcache,
                              void *valbuf)
{
  int len = TYPE_LENGTH (type);
  char buf[M68HC11_REG_SIZE];

  regcache_raw_read (regcache, HARD_D_REGNUM, buf);
  switch (len)
    {
    case 1:
      memcpy (valbuf, buf + 1, 1);
      break;

    case 2:
      memcpy (valbuf, buf, 2);
      break;

    case 3:
      memcpy ((char*) valbuf + 1, buf, 2);
      regcache_raw_read (regcache, HARD_X_REGNUM, buf);
      memcpy (valbuf, buf + 1, 1);
      break;

    case 4:
      memcpy ((char*) valbuf + 2, buf, 2);
      regcache_raw_read (regcache, HARD_X_REGNUM, buf);
      memcpy (valbuf, buf, 2);
      break;

    default:
      error (_("bad size for return value"));
    }
}

enum return_value_convention
m68hc11_return_value (struct gdbarch *gdbarch, struct type *valtype,
		      struct regcache *regcache, gdb_byte *readbuf,
		      const gdb_byte *writebuf)
{
  if (TYPE_CODE (valtype) == TYPE_CODE_STRUCT
      || TYPE_CODE (valtype) == TYPE_CODE_UNION
      || TYPE_CODE (valtype) == TYPE_CODE_ARRAY 
      || TYPE_LENGTH (valtype) > 4)
    return RETURN_VALUE_STRUCT_CONVENTION;
  else
    {
      if (readbuf != NULL)
	m68hc11_extract_return_value (valtype, regcache, readbuf);
      if (writebuf != NULL)
	m68hc11_store_return_value (valtype, regcache, writebuf);
      return RETURN_VALUE_REGISTER_CONVENTION;
    }
}

/* Test whether the ELF symbol corresponds to a function using rtc or
   rti to return.  */
   
static void
m68hc11_elf_make_msymbol_special (asymbol *sym, struct minimal_symbol *msym)
{
  unsigned char flags;

  flags = ((elf_symbol_type *)sym)->internal_elf_sym.st_other;
  if (flags & STO_M68HC12_FAR)
    MSYMBOL_SET_RTC (msym);
  if (flags & STO_M68HC12_INTERRUPT)
    MSYMBOL_SET_RTI (msym);
}

static int
gdb_print_insn_m68hc11 (bfd_vma memaddr, disassemble_info *info)
{
  if (gdbarch_bfd_arch_info (current_gdbarch)->arch == bfd_arch_m68hc11)
    return print_insn_m68hc11 (memaddr, info);
  else
    return print_insn_m68hc12 (memaddr, info);
}



/* 68HC11/68HC12 register groups.
   Identify real hard registers and soft registers used by gcc.  */

static struct reggroup *m68hc11_soft_reggroup;
static struct reggroup *m68hc11_hard_reggroup;

static void
m68hc11_init_reggroups (void)
{
  m68hc11_hard_reggroup = reggroup_new ("hard", USER_REGGROUP);
  m68hc11_soft_reggroup = reggroup_new ("soft", USER_REGGROUP);
}

static void
m68hc11_add_reggroups (struct gdbarch *gdbarch)
{
  reggroup_add (gdbarch, m68hc11_hard_reggroup);
  reggroup_add (gdbarch, m68hc11_soft_reggroup);
  reggroup_add (gdbarch, general_reggroup);
  reggroup_add (gdbarch, float_reggroup);
  reggroup_add (gdbarch, all_reggroup);
  reggroup_add (gdbarch, save_reggroup);
  reggroup_add (gdbarch, restore_reggroup);
  reggroup_add (gdbarch, vector_reggroup);
  reggroup_add (gdbarch, system_reggroup);
}

static int
m68hc11_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
                             struct reggroup *group)
{
  /* We must save the real hard register as well as gcc
     soft registers including the frame pointer.  */
  if (group == save_reggroup || group == restore_reggroup)
    {
      return (regnum <= gdbarch_num_regs (gdbarch)
              || ((regnum == SOFT_FP_REGNUM
                   || regnum == SOFT_TMP_REGNUM
                   || regnum == SOFT_ZS_REGNUM
                   || regnum == SOFT_XY_REGNUM)
                  && m68hc11_register_name (regnum)));
    }

  /* Group to identify gcc soft registers (d1..dN).  */
  if (group == m68hc11_soft_reggroup)
    {
      return regnum >= SOFT_D1_REGNUM && m68hc11_register_name (regnum);
    }

  if (group == m68hc11_hard_reggroup)
    {
      return regnum == HARD_PC_REGNUM || regnum == HARD_SP_REGNUM
        || regnum == HARD_X_REGNUM || regnum == HARD_D_REGNUM
        || regnum == HARD_Y_REGNUM || regnum == HARD_CCR_REGNUM;
    }
  return default_register_reggroup_p (gdbarch, regnum, group);
}

static struct gdbarch *
m68hc11_gdbarch_init (struct gdbarch_info info,
                      struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  struct gdbarch_tdep *tdep;
  int elf_flags;

  soft_reg_initialized = 0;

  /* Extract the elf_flags if available.  */
  if (info.abfd != NULL
      && bfd_get_flavour (info.abfd) == bfd_target_elf_flavour)
    elf_flags = elf_elfheader (info.abfd)->e_flags;
  else
    elf_flags = 0;

  /* try to find a pre-existing architecture */
  for (arches = gdbarch_list_lookup_by_info (arches, &info);
       arches != NULL;
       arches = gdbarch_list_lookup_by_info (arches->next, &info))
    {
      if (gdbarch_tdep (arches->gdbarch)->elf_flags != elf_flags)
	continue;

      return arches->gdbarch;
    }

  /* Need a new architecture. Fill in a target specific vector.  */
  tdep = (struct gdbarch_tdep *) xmalloc (sizeof (struct gdbarch_tdep));
  gdbarch = gdbarch_alloc (&info, tdep);
  tdep->elf_flags = elf_flags;

  switch (info.bfd_arch_info->arch)
    {
    case bfd_arch_m68hc11:
      tdep->stack_correction = 1;
      tdep->use_page_register = 0;
      tdep->prologue = m6811_prologue;
      set_gdbarch_addr_bit (gdbarch, 16);
      set_gdbarch_num_pseudo_regs (gdbarch, M68HC11_NUM_PSEUDO_REGS);
      set_gdbarch_pc_regnum (gdbarch, HARD_PC_REGNUM);
      set_gdbarch_num_regs (gdbarch, M68HC11_NUM_REGS);
      break;

    case bfd_arch_m68hc12:
      tdep->stack_correction = 0;
      tdep->use_page_register = elf_flags & E_M68HC12_BANKS;
      tdep->prologue = m6812_prologue;
      set_gdbarch_addr_bit (gdbarch, elf_flags & E_M68HC12_BANKS ? 32 : 16);
      set_gdbarch_num_pseudo_regs (gdbarch,
                                   elf_flags & E_M68HC12_BANKS
                                   ? M68HC12_NUM_PSEUDO_REGS
                                   : M68HC11_NUM_PSEUDO_REGS);
      set_gdbarch_pc_regnum (gdbarch, elf_flags & E_M68HC12_BANKS
                             ? M68HC12_HARD_PC_REGNUM : HARD_PC_REGNUM);
      set_gdbarch_num_regs (gdbarch, elf_flags & E_M68HC12_BANKS
                            ? M68HC12_NUM_REGS : M68HC11_NUM_REGS);
      break;

    default:
      break;
    }

  /* Initially set everything according to the ABI.
     Use 16-bit integers since it will be the case for most
     programs.  The size of these types should normally be set
     according to the dwarf2 debug information.  */
  set_gdbarch_short_bit (gdbarch, 16);
  set_gdbarch_int_bit (gdbarch, elf_flags & E_M68HC11_I32 ? 32 : 16);
  set_gdbarch_float_bit (gdbarch, 32);
  set_gdbarch_double_bit (gdbarch, elf_flags & E_M68HC11_F64 ? 64 : 32);
  set_gdbarch_long_double_bit (gdbarch, 64);
  set_gdbarch_long_bit (gdbarch, 32);
  set_gdbarch_ptr_bit (gdbarch, 16);
  set_gdbarch_long_long_bit (gdbarch, 64);

  /* Characters are unsigned.  */
  set_gdbarch_char_signed (gdbarch, 0);

  set_gdbarch_unwind_pc (gdbarch, m68hc11_unwind_pc);
  set_gdbarch_unwind_sp (gdbarch, m68hc11_unwind_sp);

  /* Set register info.  */
  set_gdbarch_fp0_regnum (gdbarch, -1);

  set_gdbarch_sp_regnum (gdbarch, HARD_SP_REGNUM);
  set_gdbarch_register_name (gdbarch, m68hc11_register_name);
  set_gdbarch_register_type (gdbarch, m68hc11_register_type);
  set_gdbarch_pseudo_register_read (gdbarch, m68hc11_pseudo_register_read);
  set_gdbarch_pseudo_register_write (gdbarch, m68hc11_pseudo_register_write);

  set_gdbarch_push_dummy_call (gdbarch, m68hc11_push_dummy_call);

  set_gdbarch_return_value (gdbarch, m68hc11_return_value);
  set_gdbarch_skip_prologue (gdbarch, m68hc11_skip_prologue);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_breakpoint_from_pc (gdbarch, m68hc11_breakpoint_from_pc);
  set_gdbarch_print_insn (gdbarch, gdb_print_insn_m68hc11);

  m68hc11_add_reggroups (gdbarch);
  set_gdbarch_register_reggroup_p (gdbarch, m68hc11_register_reggroup_p);
  set_gdbarch_print_registers_info (gdbarch, m68hc11_print_registers_info);

  /* Hook in the DWARF CFI frame unwinder.  */
  frame_unwind_append_sniffer (gdbarch, dwarf2_frame_sniffer);

  frame_unwind_append_sniffer (gdbarch, m68hc11_frame_sniffer);
  frame_base_set_default (gdbarch, &m68hc11_frame_base);
  
  /* Methods for saving / extracting a dummy frame's ID.  The ID's
     stack address must match the SP value returned by
     PUSH_DUMMY_CALL, and saved by generic_save_dummy_frame_tos.  */
  set_gdbarch_unwind_dummy_id (gdbarch, m68hc11_unwind_dummy_id);

  /* Return the unwound PC value.  */
  set_gdbarch_unwind_pc (gdbarch, m68hc11_unwind_pc);

  /* Minsymbol frobbing.  */
  set_gdbarch_elf_make_msymbol_special (gdbarch,
                                        m68hc11_elf_make_msymbol_special);

  set_gdbarch_believe_pcc_promotion (gdbarch, 1);

  return gdbarch;
}

extern initialize_file_ftype _initialize_m68hc11_tdep; /* -Wmissing-prototypes */

void
_initialize_m68hc11_tdep (void)
{
  register_gdbarch_init (bfd_arch_m68hc11, m68hc11_gdbarch_init);
  register_gdbarch_init (bfd_arch_m68hc12, m68hc11_gdbarch_init);
  m68hc11_init_reggroups ();
} 

