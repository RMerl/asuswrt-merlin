/* Target dependent code for CRIS, for GDB, the GNU debugger.

   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006, 2007
   Free Software Foundation, Inc.

   Contributed by Axis Communications AB.
   Written by Hendrik Ruijter, Stefan Andersson, and Orjan Friberg.

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
#include "trad-frame.h"
#include "dwarf2-frame.h"
#include "symtab.h"
#include "inferior.h"
#include "gdbtypes.h"
#include "gdbcore.h"
#include "gdbcmd.h"
#include "target.h"
#include "value.h"
#include "opcode/cris.h"
#include "arch-utils.h"
#include "regcache.h"
#include "gdb_assert.h"

/* To get entry_point_address.  */
#include "objfiles.h"

#include "solib.h"              /* Support for shared libraries.  */
#include "solib-svr4.h"
#include "gdb_string.h"
#include "dis-asm.h"

enum cris_num_regs
{
  /* There are no floating point registers.  Used in gdbserver low-linux.c.  */
  NUM_FREGS = 0,
  
  /* There are 16 general registers.  */
  NUM_GENREGS = 16,
  
  /* There are 16 special registers.  */
  NUM_SPECREGS = 16,

  /* CRISv32 has a pseudo PC register, not noted here.  */
  
  /* CRISv32 has 16 support registers.  */
  NUM_SUPPREGS = 16
};

/* Register numbers of various important registers.
   CRIS_FP_REGNUM   Contains address of executing stack frame.
   STR_REGNUM  Contains the address of structure return values.
   RET_REGNUM  Contains the return value when shorter than or equal to 32 bits
   ARG1_REGNUM Contains the first parameter to a function.
   ARG2_REGNUM Contains the second parameter to a function.
   ARG3_REGNUM Contains the third parameter to a function.
   ARG4_REGNUM Contains the fourth parameter to a function. Rest on stack.
   gdbarch_sp_regnum Contains address of top of stack.
   gdbarch_pc_regnum Contains address of next instruction.
   SRP_REGNUM  Subroutine return pointer register.
   BRP_REGNUM  Breakpoint return pointer register.  */

enum cris_regnums
{
  /* Enums with respect to the general registers, valid for all 
     CRIS versions.  The frame pointer is always in R8.  */
  CRIS_FP_REGNUM = 8,
  /* ABI related registers.  */
  STR_REGNUM  = 9,
  RET_REGNUM  = 10,
  ARG1_REGNUM = 10,
  ARG2_REGNUM = 11,
  ARG3_REGNUM = 12,
  ARG4_REGNUM = 13,
  
  /* Registers which happen to be common.  */
  VR_REGNUM   = 17,
  MOF_REGNUM  = 23,
  SRP_REGNUM  = 27,

  /* CRISv10 et. al. specific registers.  */
  P0_REGNUM   = 16,
  P4_REGNUM   = 20,
  CCR_REGNUM  = 21,
  P8_REGNUM   = 24,
  IBR_REGNUM  = 25,
  IRP_REGNUM  = 26,
  BAR_REGNUM  = 28,
  DCCR_REGNUM = 29,
  BRP_REGNUM  = 30,
  USP_REGNUM  = 31,

  /* CRISv32 specific registers.  */
  ACR_REGNUM  = 15,
  BZ_REGNUM   = 16,
  PID_REGNUM  = 18,
  SRS_REGNUM  = 19,
  WZ_REGNUM   = 20,
  EXS_REGNUM  = 21,
  EDA_REGNUM  = 22,
  DZ_REGNUM   = 24,
  EBP_REGNUM  = 25,
  ERP_REGNUM  = 26,
  NRP_REGNUM  = 28,
  CCS_REGNUM  = 29,
  CRISV32USP_REGNUM  = 30, /* Shares name but not number with CRISv10.  */
  SPC_REGNUM  = 31,
  CRISV32PC_REGNUM   = 32, /* Shares name but not number with CRISv10.  */

  S0_REGNUM = 33,
  S1_REGNUM = 34,
  S2_REGNUM = 35,
  S3_REGNUM = 36,
  S4_REGNUM = 37,
  S5_REGNUM = 38,
  S6_REGNUM = 39,
  S7_REGNUM = 40,
  S8_REGNUM = 41,
  S9_REGNUM = 42,
  S10_REGNUM = 43,
  S11_REGNUM = 44,
  S12_REGNUM = 45,
  S13_REGNUM = 46,
  S14_REGNUM = 47,
  S15_REGNUM = 48,
};

extern const struct cris_spec_reg cris_spec_regs[];

/* CRIS version, set via the user command 'set cris-version'.  Affects
   register names and sizes.  */
static int usr_cmd_cris_version;

/* Indicates whether to trust the above variable.  */
static int usr_cmd_cris_version_valid = 0;

static const char cris_mode_normal[] = "normal";
static const char cris_mode_guru[] = "guru";
static const char *cris_modes[] = {
  cris_mode_normal,
  cris_mode_guru,
  0
};

/* CRIS mode, set via the user command 'set cris-mode'.  Affects
   type of break instruction among other things.  */
static const char *usr_cmd_cris_mode = cris_mode_normal;

/* Whether to make use of Dwarf-2 CFI (default on).  */
static int usr_cmd_cris_dwarf2_cfi = 1;

/* CRIS architecture specific information.  */
struct gdbarch_tdep
{
  int cris_version;
  const char *cris_mode;
  int cris_dwarf2_cfi;
};

/* Functions for accessing target dependent data.  */

static int
cris_version (void)
{
  return (gdbarch_tdep (current_gdbarch)->cris_version);
}

static const char *
cris_mode (void)
{
  return (gdbarch_tdep (current_gdbarch)->cris_mode);
}

/* Sigtramp identification code copied from i386-linux-tdep.c.  */

#define SIGTRAMP_INSN0    0x9c5f  /* movu.w 0xXX, $r9 */
#define SIGTRAMP_OFFSET0  0
#define SIGTRAMP_INSN1    0xe93d  /* break 13 */
#define SIGTRAMP_OFFSET1  4

static const unsigned short sigtramp_code[] =
{
  SIGTRAMP_INSN0, 0x0077,  /* movu.w $0x77, $r9 */
  SIGTRAMP_INSN1           /* break 13 */
};

#define SIGTRAMP_LEN (sizeof sigtramp_code)

/* Note: same length as normal sigtramp code.  */

static const unsigned short rt_sigtramp_code[] =
{
  SIGTRAMP_INSN0, 0x00ad,  /* movu.w $0xad, $r9 */
  SIGTRAMP_INSN1           /* break 13 */
};

/* If PC is in a sigtramp routine, return the address of the start of
   the routine.  Otherwise, return 0.  */

static CORE_ADDR
cris_sigtramp_start (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  gdb_byte buf[SIGTRAMP_LEN];

  if (!safe_frame_unwind_memory (next_frame, pc, buf, SIGTRAMP_LEN))
    return 0;

  if (((buf[1] << 8) + buf[0]) != SIGTRAMP_INSN0)
    {
      if (((buf[1] << 8) + buf[0]) != SIGTRAMP_INSN1)
	return 0;

      pc -= SIGTRAMP_OFFSET1;
      if (!safe_frame_unwind_memory (next_frame, pc, buf, SIGTRAMP_LEN))
	return 0;
    }

  if (memcmp (buf, sigtramp_code, SIGTRAMP_LEN) != 0)
    return 0;

  return pc;
}

/* If PC is in a RT sigtramp routine, return the address of the start of
   the routine.  Otherwise, return 0.  */

static CORE_ADDR
cris_rt_sigtramp_start (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  gdb_byte buf[SIGTRAMP_LEN];

  if (!safe_frame_unwind_memory (next_frame, pc, buf, SIGTRAMP_LEN))
    return 0;

  if (((buf[1] << 8) + buf[0]) != SIGTRAMP_INSN0)
    {
      if (((buf[1] << 8) + buf[0]) != SIGTRAMP_INSN1)
	return 0;

      pc -= SIGTRAMP_OFFSET1;
      if (!safe_frame_unwind_memory (next_frame, pc, buf, SIGTRAMP_LEN))
	return 0;
    }

  if (memcmp (buf, rt_sigtramp_code, SIGTRAMP_LEN) != 0)
    return 0;

  return pc;
}

/* Assuming NEXT_FRAME is a frame following a GNU/Linux sigtramp
   routine, return the address of the associated sigcontext structure.  */

static CORE_ADDR
cris_sigcontext_addr (struct frame_info *next_frame)
{
  CORE_ADDR pc;
  CORE_ADDR sp;
  char buf[4];

  frame_unwind_register (next_frame, gdbarch_sp_regnum (current_gdbarch), buf);
  sp = extract_unsigned_integer (buf, 4);

  /* Look for normal sigtramp frame first.  */
  pc = cris_sigtramp_start (next_frame);
  if (pc)
    {
      /* struct signal_frame (arch/cris/kernel/signal.c) contains
	 struct sigcontext as its first member, meaning the SP points to
	 it already.  */
      return sp;
    }

  pc = cris_rt_sigtramp_start (next_frame);
  if (pc)
    {
      /* struct rt_signal_frame (arch/cris/kernel/signal.c) contains
	 a struct ucontext, which in turn contains a struct sigcontext.
	 Magic digging:
	 4 + 4 + 128 to struct ucontext, then
	 4 + 4 + 12 to struct sigcontext.  */
      return (sp + 156);
    }

  error (_("Couldn't recognize signal trampoline."));
  return 0;
}

struct cris_unwind_cache
{
  /* The previous frame's inner most stack address.  Used as this
     frame ID's stack_addr.  */
  CORE_ADDR prev_sp;
  /* The frame's base, optionally used by the high-level debug info.  */
  CORE_ADDR base;
  int size;
  /* How far the SP and r8 (FP) have been offset from the start of
     the stack frame (as defined by the previous frame's stack
     pointer).  */
  LONGEST sp_offset;
  LONGEST r8_offset;
  int uses_frame;

  /* From old frame_extra_info struct.  */
  CORE_ADDR return_pc;
  int leaf_function;

  /* Table indicating the location of each and every register.  */
  struct trad_frame_saved_reg *saved_regs;
};

static struct cris_unwind_cache *
cris_sigtramp_frame_unwind_cache (struct frame_info *next_frame,
				  void **this_cache)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  struct cris_unwind_cache *info;
  CORE_ADDR pc;
  CORE_ADDR sp;
  CORE_ADDR addr;
  char buf[4];
  int i;

  if ((*this_cache))
    return (*this_cache);

  info = FRAME_OBSTACK_ZALLOC (struct cris_unwind_cache);
  (*this_cache) = info;
  info->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  /* Zero all fields.  */
  info->prev_sp = 0;
  info->base = 0;
  info->size = 0;
  info->sp_offset = 0;
  info->r8_offset = 0;
  info->uses_frame = 0;
  info->return_pc = 0;
  info->leaf_function = 0;

  frame_unwind_register (next_frame, gdbarch_sp_regnum (current_gdbarch), buf);
  info->base = extract_unsigned_integer (buf, 4);

  addr = cris_sigcontext_addr (next_frame);
  
  /* Layout of the sigcontext struct:
     struct sigcontext {
	struct pt_regs regs;
	unsigned long oldmask;
	unsigned long usp;
     }; */
  
  if (tdep->cris_version == 10)
    {
      /* R0 to R13 are stored in reverse order at offset (2 * 4) in 
	 struct pt_regs.  */
      for (i = 0; i <= 13; i++)
	info->saved_regs[i].addr = addr + ((15 - i) * 4);

      info->saved_regs[MOF_REGNUM].addr = addr + (16 * 4);
      info->saved_regs[DCCR_REGNUM].addr = addr + (17 * 4);
      info->saved_regs[SRP_REGNUM].addr = addr + (18 * 4);
      /* Note: IRP is off by 2 at this point.  There's no point in correcting
	 it though since that will mean that the backtrace will show a PC 
	 different from what is shown when stopped.  */
      info->saved_regs[IRP_REGNUM].addr = addr + (19 * 4);
      info->saved_regs[gdbarch_pc_regnum (current_gdbarch)]
	= info->saved_regs[IRP_REGNUM];
      info->saved_regs[gdbarch_sp_regnum (current_gdbarch)].addr
	= addr + (24 * 4);
    }
  else
    {
      /* CRISv32.  */
      /* R0 to R13 are stored in order at offset (1 * 4) in 
	 struct pt_regs.  */
      for (i = 0; i <= 13; i++)
	info->saved_regs[i].addr = addr + ((i + 1) * 4);

      info->saved_regs[ACR_REGNUM].addr = addr + (15 * 4);
      info->saved_regs[SRS_REGNUM].addr = addr + (16 * 4);
      info->saved_regs[MOF_REGNUM].addr = addr + (17 * 4);
      info->saved_regs[SPC_REGNUM].addr = addr + (18 * 4);
      info->saved_regs[CCS_REGNUM].addr = addr + (19 * 4);
      info->saved_regs[SRP_REGNUM].addr = addr + (20 * 4);
      info->saved_regs[ERP_REGNUM].addr = addr + (21 * 4);
      info->saved_regs[EXS_REGNUM].addr = addr + (22 * 4);
      info->saved_regs[EDA_REGNUM].addr = addr + (23 * 4);

      /* FIXME: If ERP is in a delay slot at this point then the PC will
	 be wrong at this point.  This problem manifests itself in the
	 sigaltstack.exp test case, which occasionally generates FAILs when
	 the signal is received while in a delay slot.  
	 
	 This could be solved by a couple of read_memory_unsigned_integer and a
	 trad_frame_set_value.  */
      info->saved_regs[gdbarch_pc_regnum (current_gdbarch)]
	= info->saved_regs[ERP_REGNUM];

      info->saved_regs[gdbarch_sp_regnum (current_gdbarch)].addr
	= addr + (25 * 4);
    }
  
  return info;
}

static void
cris_sigtramp_frame_this_id (struct frame_info *next_frame, void **this_cache,
                             struct frame_id *this_id)
{
  struct cris_unwind_cache *cache =
    cris_sigtramp_frame_unwind_cache (next_frame, this_cache);
  (*this_id) = frame_id_build (cache->base, frame_pc_unwind (next_frame));
}

/* Forward declaration.  */

static void cris_frame_prev_register (struct frame_info *next_frame,
				      void **this_prologue_cache,
				      int regnum, int *optimizedp,
				      enum lval_type *lvalp, CORE_ADDR *addrp,
				      int *realnump, gdb_byte *bufferp);
static void
cris_sigtramp_frame_prev_register (struct frame_info *next_frame,
                                   void **this_cache,
                                   int regnum, int *optimizedp,
                                   enum lval_type *lvalp, CORE_ADDR *addrp,
                                   int *realnump, gdb_byte *valuep)
{
  /* Make sure we've initialized the cache.  */
  cris_sigtramp_frame_unwind_cache (next_frame, this_cache);
  cris_frame_prev_register (next_frame, this_cache, regnum,
                            optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind cris_sigtramp_frame_unwind =
{
  SIGTRAMP_FRAME,
  cris_sigtramp_frame_this_id,
  cris_sigtramp_frame_prev_register
};

static const struct frame_unwind *
cris_sigtramp_frame_sniffer (struct frame_info *next_frame)
{
  if (cris_sigtramp_start (next_frame) 
      || cris_rt_sigtramp_start (next_frame))
    return &cris_sigtramp_frame_unwind;

  return NULL;
}

int
crisv32_single_step_through_delay (struct gdbarch *gdbarch,
				   struct frame_info *this_frame)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  ULONGEST erp;
  int ret = 0;
  char buf[4];

  if (cris_mode () == cris_mode_guru)
    {
      frame_unwind_register (this_frame, NRP_REGNUM, buf);
    }
  else
    {
      frame_unwind_register (this_frame, ERP_REGNUM, buf);
    }

  erp = extract_unsigned_integer (buf, 4);

  if (erp & 0x1)
    {
      /* In delay slot - check if there's a breakpoint at the preceding
	 instruction.  */
      if (breakpoint_here_p (erp & ~0x1))
	ret = 1;
    }
  return ret;
}

/* Hardware watchpoint support.  */

/* We support 6 hardware data watchpoints, but cannot trigger on execute
   (any combination of read/write is fine).  */

int
cris_can_use_hardware_watchpoint (int type, int count, int other)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  /* No bookkeeping is done here; it is handled by the remote debug agent.  */

  if (tdep->cris_version != 32)
    return 0;
  else
    /* CRISv32: Six data watchpoints, one for instructions.  */
    return (((type == bp_read_watchpoint || type == bp_access_watchpoint
	     || type == bp_hardware_watchpoint) && count <= 6) 
	    || (type == bp_hardware_breakpoint && count <= 1));
}

/* The CRISv32 hardware data watchpoints work by specifying ranges,
   which have no alignment or length restrictions.  */

int
cris_region_ok_for_watchpoint (CORE_ADDR addr, int len)
{
  return 1;
}

/* If the inferior has some watchpoint that triggered, return the
   address associated with that watchpoint.  Otherwise, return
   zero.  */

CORE_ADDR
cris_stopped_data_address (void)
{
  CORE_ADDR eda;
  eda = get_frame_register_unsigned (get_current_frame (), EDA_REGNUM);
  return eda;
}

/* The instruction environment needed to find single-step breakpoints.  */

typedef 
struct instruction_environment
{
  unsigned long reg[NUM_GENREGS];
  unsigned long preg[NUM_SPECREGS];
  unsigned long branch_break_address;
  unsigned long delay_slot_pc;
  unsigned long prefix_value;
  int   branch_found;
  int   prefix_found;
  int   invalid;
  int   slot_needed;
  int   delay_slot_pc_active;
  int   xflag_found;
  int   disable_interrupt;
} inst_env_type;

/* Machine-dependencies in CRIS for opcodes.  */

/* Instruction sizes.  */
enum cris_instruction_sizes
{
  INST_BYTE_SIZE  = 0,
  INST_WORD_SIZE  = 1,
  INST_DWORD_SIZE = 2
};

/* Addressing modes.  */
enum cris_addressing_modes
{
  REGISTER_MODE = 1,
  INDIRECT_MODE = 2,
  AUTOINC_MODE  = 3
};

/* Prefix addressing modes.  */
enum cris_prefix_addressing_modes
{
  PREFIX_INDEX_MODE  = 2,
  PREFIX_ASSIGN_MODE = 3,

  /* Handle immediate byte offset addressing mode prefix format.  */
  PREFIX_OFFSET_MODE = 2
};

/* Masks for opcodes.  */
enum cris_opcode_masks
{
  BRANCH_SIGNED_SHORT_OFFSET_MASK = 0x1,
  SIGNED_EXTEND_BIT_MASK          = 0x2,
  SIGNED_BYTE_MASK                = 0x80,
  SIGNED_BYTE_EXTEND_MASK         = 0xFFFFFF00,
  SIGNED_WORD_MASK                = 0x8000,
  SIGNED_WORD_EXTEND_MASK         = 0xFFFF0000,
  SIGNED_DWORD_MASK               = 0x80000000,
  SIGNED_QUICK_VALUE_MASK         = 0x20,
  SIGNED_QUICK_VALUE_EXTEND_MASK  = 0xFFFFFFC0
};

/* Functions for opcodes.  The general form of the ETRAX 16-bit instruction:
   Bit 15 - 12   Operand2
       11 - 10   Mode
        9 -  6   Opcode
        5 -  4   Size
        3 -  0   Operand1  */

static int 
cris_get_operand2 (unsigned short insn)
{
  return ((insn & 0xF000) >> 12);
}

static int
cris_get_mode (unsigned short insn)
{
  return ((insn & 0x0C00) >> 10);
}

static int
cris_get_opcode (unsigned short insn)
{
  return ((insn & 0x03C0) >> 6);
}

static int
cris_get_size (unsigned short insn)
{
  return ((insn & 0x0030) >> 4);
}

static int
cris_get_operand1 (unsigned short insn)
{
  return (insn & 0x000F);
}

/* Additional functions in order to handle opcodes.  */

static int
cris_get_quick_value (unsigned short insn)
{
  return (insn & 0x003F);
}

static int
cris_get_bdap_quick_offset (unsigned short insn)
{
  return (insn & 0x00FF);
}

static int
cris_get_branch_short_offset (unsigned short insn)
{
  return (insn & 0x00FF);
}

static int
cris_get_asr_shift_steps (unsigned long value)
{
  return (value & 0x3F);
}

static int
cris_get_clear_size (unsigned short insn)
{
  return ((insn) & 0xC000);
}

static int
cris_is_signed_extend_bit_on (unsigned short insn)
{
  return (((insn) & 0x20) == 0x20);
}

static int
cris_is_xflag_bit_on (unsigned short insn)
{
  return (((insn) & 0x1000) == 0x1000);
}

static void
cris_set_size_to_dword (unsigned short *insn)
{
  *insn &= 0xFFCF; 
  *insn |= 0x20; 
}

static signed char
cris_get_signed_offset (unsigned short insn)
{
  return ((signed char) (insn & 0x00FF));
}

/* Calls an op function given the op-type, working on the insn and the
   inst_env.  */
static void cris_gdb_func (enum cris_op_type, unsigned short, inst_env_type *);

static struct gdbarch *cris_gdbarch_init (struct gdbarch_info,
                                          struct gdbarch_list *);

static void cris_dump_tdep (struct gdbarch *, struct ui_file *);

static void set_cris_version (char *ignore_args, int from_tty, 
			      struct cmd_list_element *c);

static void set_cris_mode (char *ignore_args, int from_tty, 
			   struct cmd_list_element *c);

static void set_cris_dwarf2_cfi (char *ignore_args, int from_tty, 
				 struct cmd_list_element *c);

static CORE_ADDR cris_scan_prologue (CORE_ADDR pc, 
				     struct frame_info *next_frame,
				     struct cris_unwind_cache *info);

static CORE_ADDR crisv32_scan_prologue (CORE_ADDR pc, 
					struct frame_info *next_frame,
					struct cris_unwind_cache *info);

static CORE_ADDR cris_unwind_pc (struct gdbarch *gdbarch, 
				 struct frame_info *next_frame);

static CORE_ADDR cris_unwind_sp (struct gdbarch *gdbarch, 
				 struct frame_info *next_frame);

/* When arguments must be pushed onto the stack, they go on in reverse
   order.  The below implements a FILO (stack) to do this.  
   Copied from d10v-tdep.c.  */

struct stack_item
{
  int len;
  struct stack_item *prev;
  void *data;
};

static struct stack_item *
push_stack_item (struct stack_item *prev, void *contents, int len)
{
  struct stack_item *si;
  si = xmalloc (sizeof (struct stack_item));
  si->data = xmalloc (len);
  si->len = len;
  si->prev = prev;
  memcpy (si->data, contents, len);
  return si;
}

static struct stack_item *
pop_stack_item (struct stack_item *si)
{
  struct stack_item *dead = si;
  si = si->prev;
  xfree (dead->data);
  xfree (dead);
  return si;
}

/* Put here the code to store, into fi->saved_regs, the addresses of
   the saved registers of frame described by FRAME_INFO.  This
   includes special registers such as pc and fp saved in special ways
   in the stack frame.  sp is even more special: the address we return
   for it IS the sp for the next frame.  */

struct cris_unwind_cache *
cris_frame_unwind_cache (struct frame_info *next_frame,
			 void **this_prologue_cache)
{
  CORE_ADDR pc;
  struct cris_unwind_cache *info;
  int i;

  if ((*this_prologue_cache))
    return (*this_prologue_cache);

  info = FRAME_OBSTACK_ZALLOC (struct cris_unwind_cache);
  (*this_prologue_cache) = info;
  info->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  /* Zero all fields.  */
  info->prev_sp = 0;
  info->base = 0;
  info->size = 0;
  info->sp_offset = 0;
  info->r8_offset = 0;
  info->uses_frame = 0;
  info->return_pc = 0;
  info->leaf_function = 0;

  /* Prologue analysis does the rest...  */
  if (cris_version () == 32)
    crisv32_scan_prologue (frame_func_unwind (next_frame, NORMAL_FRAME),
			   next_frame, info);
  else
    cris_scan_prologue (frame_func_unwind (next_frame, NORMAL_FRAME),
			next_frame, info);

  return info;
}

/* Given a GDB frame, determine the address of the calling function's
   frame.  This will be used to create a new GDB frame struct.  */

static void
cris_frame_this_id (struct frame_info *next_frame,
		    void **this_prologue_cache,
		    struct frame_id *this_id)
{
  struct cris_unwind_cache *info
    = cris_frame_unwind_cache (next_frame, this_prologue_cache);
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
cris_frame_prev_register (struct frame_info *next_frame,
			  void **this_prologue_cache,
			  int regnum, int *optimizedp,
			  enum lval_type *lvalp, CORE_ADDR *addrp,
			  int *realnump, gdb_byte *bufferp)
{
  struct cris_unwind_cache *info
    = cris_frame_unwind_cache (next_frame, this_prologue_cache);
  trad_frame_get_prev_register (next_frame, info->saved_regs, regnum,
				optimizedp, lvalp, addrp, realnump, bufferp);
}

/* Assuming NEXT_FRAME->prev is a dummy, return the frame ID of that
   dummy frame.  The frame ID's base needs to match the TOS value
   saved by save_dummy_frame_tos(), and the PC match the dummy frame's
   breakpoint.  */

static struct frame_id
cris_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_id_build (cris_unwind_sp (gdbarch, next_frame),
			 frame_pc_unwind (next_frame));
}

static CORE_ADDR
cris_frame_align (struct gdbarch *gdbarch, CORE_ADDR sp)
{
  /* Align to the size of an instruction (so that they can safely be
     pushed onto the stack).  */
  return sp & ~3;
}

static CORE_ADDR
cris_push_dummy_code (struct gdbarch *gdbarch,
                      CORE_ADDR sp, CORE_ADDR funaddr, int using_gcc,
                      struct value **args, int nargs,
                      struct type *value_type,
                      CORE_ADDR *real_pc, CORE_ADDR *bp_addr,
		      struct regcache *regcache)
{
  /* Allocate space sufficient for a breakpoint.  */
  sp = (sp - 4) & ~3;
  /* Store the address of that breakpoint */
  *bp_addr = sp;
  /* CRIS always starts the call at the callee's entry point.  */
  *real_pc = funaddr;
  return sp;
}

static CORE_ADDR
cris_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		      struct regcache *regcache, CORE_ADDR bp_addr,
		      int nargs, struct value **args, CORE_ADDR sp,
		      int struct_return, CORE_ADDR struct_addr)
{
  int stack_alloc;
  int stack_offset;
  int argreg;
  int argnum;

  CORE_ADDR regval;

  /* The function's arguments and memory allocated by gdb for the arguments to
     point at reside in separate areas on the stack.
     Both frame pointers grow toward higher addresses.  */
  CORE_ADDR fp_arg;
  CORE_ADDR fp_mem;

  struct stack_item *si = NULL;

  /* Push the return address.  */
  regcache_cooked_write_unsigned (regcache, SRP_REGNUM, bp_addr);

  /* Are we returning a value using a structure return or a normal value
     return?  struct_addr is the address of the reserved space for the return
     structure to be written on the stack.  */
  if (struct_return)
    {
      regcache_cooked_write_unsigned (regcache, STR_REGNUM, struct_addr);
    }

  /* Now load as many as possible of the first arguments into registers,
     and push the rest onto the stack.  */
  argreg = ARG1_REGNUM;
  stack_offset = 0;

  for (argnum = 0; argnum < nargs; argnum++)
    {
      int len;
      char *val;
      int reg_demand;
      int i;
      
      len = TYPE_LENGTH (value_type (args[argnum]));
      val = (char *) value_contents (args[argnum]);
      
      /* How may registers worth of storage do we need for this argument?  */
      reg_demand = (len / 4) + (len % 4 != 0 ? 1 : 0);
        
      if (len <= (2 * 4) && (argreg + reg_demand - 1 <= ARG4_REGNUM))
        {
          /* Data passed by value.  Fits in available register(s).  */
          for (i = 0; i < reg_demand; i++)
            {
              regcache_cooked_write_unsigned (regcache, argreg, 
					      *(unsigned long *) val);
              argreg++;
              val += 4;
            }
        }
      else if (len <= (2 * 4) && argreg <= ARG4_REGNUM)
        {
          /* Data passed by value. Does not fit in available register(s).
             Use the register(s) first, then the stack.  */
          for (i = 0; i < reg_demand; i++)
            {
              if (argreg <= ARG4_REGNUM)
                {
		  regcache_cooked_write_unsigned (regcache, argreg, 
						  *(unsigned long *) val);
                  argreg++;
                  val += 4;
                }
              else
                {
		  /* Push item for later so that pushed arguments
		     come in the right order.  */
		  si = push_stack_item (si, val, 4);
                  val += 4;
                }
            }
        }
      else if (len > (2 * 4))
        {
	  /* FIXME */
	  internal_error (__FILE__, __LINE__, _("We don't do this"));
        }
      else
        {
          /* Data passed by value.  No available registers.  Put it on
             the stack.  */
	   si = push_stack_item (si, val, len);
        }
    }

  while (si)
    {
      /* fp_arg must be word-aligned (i.e., don't += len) to match
	 the function prologue.  */
      sp = (sp - si->len) & ~3;
      write_memory (sp, si->data, si->len);
      si = pop_stack_item (si);
    }

  /* Finally, update the SP register.  */
  regcache_cooked_write_unsigned (regcache,
				  gdbarch_sp_regnum (current_gdbarch), sp);

  return sp;
}

static const struct frame_unwind cris_frame_unwind = 
{
  NORMAL_FRAME,
  cris_frame_this_id,
  cris_frame_prev_register
};

const struct frame_unwind *
cris_frame_sniffer (struct frame_info *next_frame)
{
  return &cris_frame_unwind;
}

static CORE_ADDR
cris_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct cris_unwind_cache *info
    = cris_frame_unwind_cache (next_frame, this_cache);
  return info->base;
}

static const struct frame_base cris_frame_base = 
{
  &cris_frame_unwind,
  cris_frame_base_address,
  cris_frame_base_address,
  cris_frame_base_address
};

/* Frames information. The definition of the struct frame_info is

   CORE_ADDR frame
   CORE_ADDR pc
   enum frame_type type;
   CORE_ADDR return_pc
   int leaf_function

   If the compilation option -fno-omit-frame-pointer is present the
   variable frame will be set to the content of R8 which is the frame
   pointer register.

   The variable pc contains the address where execution is performed
   in the present frame.  The innermost frame contains the current content
   of the register PC.  All other frames contain the content of the
   register PC in the next frame.

   The variable `type' indicates the frame's type: normal, SIGTRAMP
   (associated with a signal handler), dummy (associated with a dummy
   frame).

   The variable return_pc contains the address where execution should be
   resumed when the present frame has finished, the return address.

   The variable leaf_function is 1 if the return address is in the register
   SRP, and 0 if it is on the stack.

   Prologue instructions C-code.
   The prologue may consist of (-fno-omit-frame-pointer)
   1)                2)
   push   srp
   push   r8         push   r8
   move.d sp,r8      move.d sp,r8
   subq   X,sp       subq   X,sp
   movem  rY,[sp]    movem  rY,[sp]
   move.S rZ,[r8-U]  move.S rZ,[r8-U]

   where 1 is a non-terminal function, and 2 is a leaf-function.

   Note that this assumption is extremely brittle, and will break at the
   slightest change in GCC's prologue.

   If local variables are declared or register contents are saved on stack
   the subq-instruction will be present with X as the number of bytes
   needed for storage.  The reshuffle with respect to r8 may be performed
   with any size S (b, w, d) and any of the general registers Z={0..13}. 
   The offset U should be representable by a signed 8-bit value in all cases. 
   Thus, the prefix word is assumed to be immediate byte offset mode followed
   by another word containing the instruction.

   Degenerate cases:
   3)
   push   r8
   move.d sp,r8
   move.d r8,sp
   pop    r8   

   Prologue instructions C++-code.
   Case 1) and 2) in the C-code may be followed by

   move.d r10,rS    ; this
   move.d r11,rT    ; P1
   move.d r12,rU    ; P2
   move.d r13,rV    ; P3
   move.S [r8+U],rZ ; P4

   if any of the call parameters are stored. The host expects these 
   instructions to be executed in order to get the call parameters right.  */

/* Examine the prologue of a function.  The variable ip is the address of 
   the first instruction of the prologue.  The variable limit is the address 
   of the first instruction after the prologue.  The variable fi contains the 
   information in struct frame_info.  The variable frameless_p controls whether
   the entire prologue is examined (0) or just enough instructions to 
   determine that it is a prologue (1).  */

static CORE_ADDR 
cris_scan_prologue (CORE_ADDR pc, struct frame_info *next_frame,
		    struct cris_unwind_cache *info)
{
  /* Present instruction.  */
  unsigned short insn;

  /* Next instruction, lookahead.  */
  unsigned short insn_next; 
  int regno;

  /* Is there a push fp?  */
  int have_fp; 

  /* Number of byte on stack used for local variables and movem.  */
  int val; 

  /* Highest register number in a movem.  */
  int regsave;

  /* move.d r<source_register>,rS */
  short source_register; 

  /* Scan limit.  */
  int limit;

  /* This frame is with respect to a leaf until a push srp is found.  */
  if (info)
    {
      info->leaf_function = 1;
    }

  /* Assume nothing on stack.  */
  val = 0;
  regsave = -1;

  /* If we were called without a next_frame, that means we were called
     from cris_skip_prologue which already tried to find the end of the
     prologue through the symbol information.  64 instructions past current
     pc is arbitrarily chosen, but at least it means we'll stop eventually.  */
  limit = next_frame ? frame_pc_unwind (next_frame) : pc + 64;

  /* Find the prologue instructions.  */
  while (pc > 0 && pc < limit)
    {
      insn = read_memory_unsigned_integer (pc, 2);
      pc += 2;
      if (insn == 0xE1FC)
        {
          /* push <reg> 32 bit instruction */
          insn_next = read_memory_unsigned_integer (pc, 2);
          pc += 2;
          regno = cris_get_operand2 (insn_next);
	  if (info)
	    {
	      info->sp_offset += 4;
	    }
          /* This check, meant to recognize srp, used to be regno == 
             (SRP_REGNUM - NUM_GENREGS), but that covers r11 also.  */
          if (insn_next == 0xBE7E)
            {
	      if (info)
		{
		  info->leaf_function = 0;
		}
            }
	  else if (insn_next == 0x8FEE)
            {
	      /* push $r8 */
	      if (info)
		{
		  info->r8_offset = info->sp_offset;
		}
            }
        }
      else if (insn == 0x866E)
        {
          /* move.d sp,r8 */
	  if (info)
	    {
	      info->uses_frame = 1;
	    }
          continue;
        }
      else if (cris_get_operand2 (insn) == gdbarch_sp_regnum (current_gdbarch)
               && cris_get_mode (insn) == 0x0000
               && cris_get_opcode (insn) == 0x000A)
        {
          /* subq <val>,sp */
	  if (info)
	    {
	      info->sp_offset += cris_get_quick_value (insn);
	    }
        }
      else if (cris_get_mode (insn) == 0x0002 
               && cris_get_opcode (insn) == 0x000F
               && cris_get_size (insn) == 0x0003
               && cris_get_operand1 (insn) == gdbarch_sp_regnum
					      (current_gdbarch))
        {
          /* movem r<regsave>,[sp] */
          regsave = cris_get_operand2 (insn);
        }
      else if (cris_get_operand2 (insn) == gdbarch_sp_regnum (current_gdbarch)
               && ((insn & 0x0F00) >> 8) == 0x0001
               && (cris_get_signed_offset (insn) < 0))
        {
          /* Immediate byte offset addressing prefix word with sp as base 
             register.  Used for CRIS v8 i.e. ETRAX 100 and newer if <val> 
             is between 64 and 128. 
             movem r<regsave>,[sp=sp-<val>] */
	  if (info)
	    {
	      info->sp_offset += -cris_get_signed_offset (insn);
	    }
	  insn_next = read_memory_unsigned_integer (pc, 2);
          pc += 2;
          if (cris_get_mode (insn_next) == PREFIX_ASSIGN_MODE
              && cris_get_opcode (insn_next) == 0x000F
              && cris_get_size (insn_next) == 0x0003
              && cris_get_operand1 (insn_next) == gdbarch_sp_regnum
						  (current_gdbarch))
            {
              regsave = cris_get_operand2 (insn_next);
            }
          else
            {
              /* The prologue ended before the limit was reached.  */
              pc -= 4;
              break;
            }
        }
      else if (cris_get_mode (insn) == 0x0001
               && cris_get_opcode (insn) == 0x0009
               && cris_get_size (insn) == 0x0002)
        {
          /* move.d r<10..13>,r<0..15> */
          source_register = cris_get_operand1 (insn);

          /* FIXME?  In the glibc solibs, the prologue might contain something
             like (this example taken from relocate_doit):
             move.d $pc,$r0
             sub.d 0xfffef426,$r0
             which isn't covered by the source_register check below.  Question
             is whether to add a check for this combo, or make better use of
             the limit variable instead.  */
          if (source_register < ARG1_REGNUM || source_register > ARG4_REGNUM)
            {
              /* The prologue ended before the limit was reached.  */
              pc -= 2;
              break;
            }
        }
      else if (cris_get_operand2 (insn) == CRIS_FP_REGNUM 
               /* The size is a fixed-size.  */
               && ((insn & 0x0F00) >> 8) == 0x0001 
               /* A negative offset.  */
               && (cris_get_signed_offset (insn) < 0))  
        {
          /* move.S rZ,[r8-U] (?) */
          insn_next = read_memory_unsigned_integer (pc, 2);
          pc += 2;
          regno = cris_get_operand2 (insn_next);
          if ((regno >= 0 && regno < gdbarch_sp_regnum (current_gdbarch))
              && cris_get_mode (insn_next) == PREFIX_OFFSET_MODE
              && cris_get_opcode (insn_next) == 0x000F)
            {
              /* move.S rZ,[r8-U] */
              continue;
            }
          else
            {
              /* The prologue ended before the limit was reached.  */
              pc -= 4;
              break;
            }
        }
      else if (cris_get_operand2 (insn) == CRIS_FP_REGNUM 
               /* The size is a fixed-size.  */
               && ((insn & 0x0F00) >> 8) == 0x0001 
               /* A positive offset.  */
               && (cris_get_signed_offset (insn) > 0))  
        {
          /* move.S [r8+U],rZ (?) */
	  insn_next = read_memory_unsigned_integer (pc, 2);
          pc += 2;
          regno = cris_get_operand2 (insn_next);
          if ((regno >= 0 && regno < gdbarch_sp_regnum (current_gdbarch))
              && cris_get_mode (insn_next) == PREFIX_OFFSET_MODE
              && cris_get_opcode (insn_next) == 0x0009
              && cris_get_operand1 (insn_next) == regno)
            {
              /* move.S [r8+U],rZ */
              continue;
            }
          else
            {
              /* The prologue ended before the limit was reached.  */
              pc -= 4;
              break;
            }
        }
      else
        {
          /* The prologue ended before the limit was reached.  */
          pc -= 2;
          break;
        }
    }

  /* We only want to know the end of the prologue when next_frame and info
     are NULL (called from cris_skip_prologue i.e.).  */
  if (next_frame == NULL && info == NULL)
    {
      return pc;
    }

  info->size = info->sp_offset;

  /* Compute the previous frame's stack pointer (which is also the
     frame's ID's stack address), and this frame's base pointer.  */
  if (info->uses_frame)
    {
      ULONGEST this_base;
      /* The SP was moved to the FP.  This indicates that a new frame
         was created.  Get THIS frame's FP value by unwinding it from
         the next frame.  */
      frame_unwind_unsigned_register (next_frame, CRIS_FP_REGNUM, 
				      &this_base);
      info->base = this_base;
      info->saved_regs[CRIS_FP_REGNUM].addr = info->base;
  
      /* The FP points at the last saved register.  Adjust the FP back
         to before the first saved register giving the SP.  */
      info->prev_sp = info->base + info->r8_offset;
    }
  else
    {
      ULONGEST this_base;      
      /* Assume that the FP is this frame's SP but with that pushed
         stack space added back.  */
      frame_unwind_unsigned_register (next_frame,
				      gdbarch_sp_regnum (current_gdbarch),
				      &this_base);
      info->base = this_base;
      info->prev_sp = info->base + info->size;
    }
      
  /* Calculate the addresses for the saved registers on the stack.  */
  /* FIXME: The address calculation should really be done on the fly while
     we're analyzing the prologue (we only hold one regsave value as it is 
     now).  */
  val = info->sp_offset;

  for (regno = regsave; regno >= 0; regno--)
    {
      info->saved_regs[regno].addr = info->base + info->r8_offset - val;
      val -= 4;
    }

  /* The previous frame's SP needed to be computed.  Save the computed
     value.  */
  trad_frame_set_value (info->saved_regs,
			gdbarch_sp_regnum (current_gdbarch), info->prev_sp);

  if (!info->leaf_function)
    {
      /* SRP saved on the stack.  But where?  */
      if (info->r8_offset == 0)
	{
	  /* R8 not pushed yet.  */
	  info->saved_regs[SRP_REGNUM].addr = info->base;
	}
      else
	{
	  /* R8 pushed, but SP may or may not be moved to R8 yet.  */
	  info->saved_regs[SRP_REGNUM].addr = info->base + 4;
	}
    }

  /* The PC is found in SRP (the actual register or located on the stack).  */
  info->saved_regs[gdbarch_pc_regnum (current_gdbarch)]
    = info->saved_regs[SRP_REGNUM];

  return pc;
}

static CORE_ADDR 
crisv32_scan_prologue (CORE_ADDR pc, struct frame_info *next_frame,
		    struct cris_unwind_cache *info)
{
  ULONGEST this_base;

  /* Unlike the CRISv10 prologue scanner (cris_scan_prologue), this is not
     meant to be a full-fledged prologue scanner.  It is only needed for 
     the cases where we end up in code always lacking DWARF-2 CFI, notably:

       * PLT stubs (library calls)
       * call dummys
       * signal trampolines

     For those cases, it is assumed that there is no actual prologue; that 
     the stack pointer is not adjusted, and (as a consequence) the return
     address is not pushed onto the stack.  */

  /* We only want to know the end of the prologue when next_frame and info
     are NULL (called from cris_skip_prologue i.e.).  */
  if (next_frame == NULL && info == NULL)
    {
      return pc;
    }

  /* The SP is assumed to be unaltered.  */
  frame_unwind_unsigned_register (next_frame,
				  gdbarch_sp_regnum (current_gdbarch),
				   &this_base);
  info->base = this_base;
  info->prev_sp = this_base;
      
  /* The PC is assumed to be found in SRP.  */
  info->saved_regs[gdbarch_pc_regnum (current_gdbarch)]
    = info->saved_regs[SRP_REGNUM];

  return pc;
}

/* Advance pc beyond any function entry prologue instructions at pc
   to reach some "real" code.  */

/* Given a PC value corresponding to the start of a function, return the PC
   of the first instruction after the function prologue.  */

static CORE_ADDR
cris_skip_prologue (CORE_ADDR pc)
{
  CORE_ADDR func_addr, func_end;
  struct symtab_and_line sal;
  CORE_ADDR pc_after_prologue;
  
  /* If we have line debugging information, then the end of the prologue
     should the first assembly instruction of the first source line.  */
  if (find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    {
      sal = find_pc_line (func_addr, 0);
      if (sal.end > 0 && sal.end < func_end)
	return sal.end;
    }

  if (cris_version () == 32)
    pc_after_prologue = crisv32_scan_prologue (pc, NULL, NULL);
  else
    pc_after_prologue = cris_scan_prologue (pc, NULL, NULL);

  return pc_after_prologue;
}

static CORE_ADDR
cris_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  ULONGEST pc;
  frame_unwind_unsigned_register (next_frame,
				  gdbarch_pc_regnum (current_gdbarch), &pc);
  return pc;
}

static CORE_ADDR
cris_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  ULONGEST sp;
  frame_unwind_unsigned_register (next_frame,
				  gdbarch_sp_regnum (current_gdbarch), &sp);
  return sp;
}

/* Use the program counter to determine the contents and size of a breakpoint
   instruction.  It returns a pointer to a string of bytes that encode a
   breakpoint instruction, stores the length of the string to *lenptr, and
   adjusts pcptr (if necessary) to point to the actual memory location where
   the breakpoint should be inserted.  */

static const unsigned char *
cris_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  static unsigned char break8_insn[] = {0x38, 0xe9};
  static unsigned char break15_insn[] = {0x3f, 0xe9};
  *lenptr = 2;

  if (cris_mode () == cris_mode_guru)
    return break15_insn;
  else
    return break8_insn;
}

/* Returns 1 if spec_reg is applicable to the current gdbarch's CRIS version,
   0 otherwise.  */

static int
cris_spec_reg_applicable (struct cris_spec_reg spec_reg)
{
  int version = cris_version ();
  
  switch (spec_reg.applicable_version)
    {
    case cris_ver_version_all:
      return 1;
    case cris_ver_warning:
      /* Indeterminate/obsolete.  */
      return 0;
    case cris_ver_v0_3:
      return (version >= 0 && version <= 3);
    case cris_ver_v3p:
      return (version >= 3);
    case cris_ver_v8:
      return (version == 8 || version == 9);
    case cris_ver_v8p:
      return (version >= 8);
    case cris_ver_v0_10:
      return (version >= 0 && version <= 10);
    case cris_ver_v3_10:
      return (version >= 3 && version <= 10);
    case cris_ver_v8_10:
      return (version >= 8 && version <= 10);
    case cris_ver_v10:
      return (version == 10);
    case cris_ver_v10p:
      return (version >= 10);
    case cris_ver_v32p:
      return (version >= 32);
    default:
      /* Invalid cris version.  */
      return 0;
    }
}

/* Returns the register size in unit byte.  Returns 0 for an unimplemented
   register, -1 for an invalid register.  */

static int
cris_register_size (int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int i;
  int spec_regno;
  
  if (regno >= 0 && regno < NUM_GENREGS)
    {
      /* General registers (R0 - R15) are 32 bits.  */
      return 4;
    }
  else if (regno >= NUM_GENREGS && regno < (NUM_GENREGS + NUM_SPECREGS))
    {
      /* Special register (R16 - R31).  cris_spec_regs is zero-based. 
         Adjust regno accordingly.  */
      spec_regno = regno - NUM_GENREGS;
      
      for (i = 0; cris_spec_regs[i].name != NULL; i++)
        {
          if (cris_spec_regs[i].number == spec_regno 
              && cris_spec_reg_applicable (cris_spec_regs[i]))
            /* Go with the first applicable register.  */
            return cris_spec_regs[i].reg_size;
        }
      /* Special register not applicable to this CRIS version.  */
      return 0;
    }
  else if (regno >= gdbarch_pc_regnum (current_gdbarch)
	   && regno < gdbarch_num_regs (current_gdbarch))
    {
      /* This will apply to CRISv32 only where there are additional registers
	 after the special registers (pseudo PC and support registers).  */
      return 4;
    }

  
  return -1;
}

/* Nonzero if regno should not be fetched from the target.  This is the case
   for unimplemented (size 0) and non-existant registers.  */

static int
cris_cannot_fetch_register (int regno)
{
  return ((regno < 0 || regno >= gdbarch_num_regs (current_gdbarch))
          || (cris_register_size (regno) == 0));
}

/* Nonzero if regno should not be written to the target, for various 
   reasons.  */

static int
cris_cannot_store_register (int regno)
{
  /* There are three kinds of registers we refuse to write to.
     1. Those that not implemented.
     2. Those that are read-only (depends on the processor mode).
     3. Those registers to which a write has no effect.
  */

  if (regno < 0
      || regno >= gdbarch_num_regs (current_gdbarch)
      || cris_register_size (regno) == 0)
    /* Not implemented.  */
    return 1;

  else if  (regno == VR_REGNUM)
    /* Read-only.  */
    return 1;

  else if  (regno == P0_REGNUM || regno == P4_REGNUM || regno == P8_REGNUM)
    /* Writing has no effect.  */
    return 1;

  /* IBR, BAR, BRP and IRP are read-only in user mode.  Let the debug
     agent decide whether they are writable.  */
  
  return 0;
}

/* Nonzero if regno should not be fetched from the target.  This is the case
   for unimplemented (size 0) and non-existant registers.  */

static int
crisv32_cannot_fetch_register (int regno)
{
  return ((regno < 0 || regno >= gdbarch_num_regs (current_gdbarch))
          || (cris_register_size (regno) == 0));
}

/* Nonzero if regno should not be written to the target, for various 
   reasons.  */

static int
crisv32_cannot_store_register (int regno)
{
  /* There are three kinds of registers we refuse to write to.
     1. Those that not implemented.
     2. Those that are read-only (depends on the processor mode).
     3. Those registers to which a write has no effect.
  */

  if (regno < 0
      || regno >= gdbarch_num_regs (current_gdbarch)
      || cris_register_size (regno) == 0)
    /* Not implemented.  */
    return 1;

  else if  (regno == VR_REGNUM)
    /* Read-only.  */
    return 1;

  else if  (regno == BZ_REGNUM || regno == WZ_REGNUM || regno == DZ_REGNUM)
    /* Writing has no effect.  */
    return 1;

  /* Many special registers are read-only in user mode.  Let the debug
     agent decide whether they are writable.  */
  
  return 0;
}

/* Return the GDB type (defined in gdbtypes.c) for the "standard" data type
   of data in register regno.  */

static struct type *
cris_register_type (struct gdbarch *gdbarch, int regno)
{
  if (regno == gdbarch_pc_regnum (current_gdbarch))
    return builtin_type_void_func_ptr;
  else if (regno == gdbarch_sp_regnum (current_gdbarch)
	   || regno == CRIS_FP_REGNUM)
    return builtin_type_void_data_ptr;
  else if ((regno >= 0 && regno < gdbarch_sp_regnum (current_gdbarch))
	   || (regno >= MOF_REGNUM && regno <= USP_REGNUM))
    /* Note: R8 taken care of previous clause.  */
    return builtin_type_uint32;
  else if (regno >= P4_REGNUM && regno <= CCR_REGNUM)
      return builtin_type_uint16;
  else if (regno >= P0_REGNUM && regno <= VR_REGNUM)
      return builtin_type_uint8;
  else
      /* Invalid (unimplemented) register.  */
      return builtin_type_int0;
}

static struct type *
crisv32_register_type (struct gdbarch *gdbarch, int regno)
{
  if (regno == gdbarch_pc_regnum (current_gdbarch))
    return builtin_type_void_func_ptr;
  else if (regno == gdbarch_sp_regnum (current_gdbarch)
	   || regno == CRIS_FP_REGNUM)
    return builtin_type_void_data_ptr;
  else if ((regno >= 0 && regno <= ACR_REGNUM)
	   || (regno >= EXS_REGNUM && regno <= SPC_REGNUM)
	   || (regno == PID_REGNUM)
	   || (regno >= S0_REGNUM && regno <= S15_REGNUM))
    /* Note: R8 and SP taken care of by previous clause.  */
    return builtin_type_uint32;
  else if (regno == WZ_REGNUM)
      return builtin_type_uint16;
  else if (regno == BZ_REGNUM || regno == VR_REGNUM || regno == SRS_REGNUM)
      return builtin_type_uint8;
  else
    {
      /* Invalid (unimplemented) register.  Should not happen as there are
	 no unimplemented CRISv32 registers.  */
      warning (_("crisv32_register_type: unknown regno %d"), regno);
      return builtin_type_int0;
    }
}

/* Stores a function return value of type type, where valbuf is the address 
   of the value to be stored.  */

/* In the CRIS ABI, R10 and R11 are used to store return values.  */

static void
cris_store_return_value (struct type *type, struct regcache *regcache,
			 const void *valbuf)
{
  ULONGEST val;
  int len = TYPE_LENGTH (type);
  
  if (len <= 4)
    {
      /* Put the return value in R10.  */
      val = extract_unsigned_integer (valbuf, len);
      regcache_cooked_write_unsigned (regcache, ARG1_REGNUM, val);
    }
  else if (len <= 8)
    {
      /* Put the return value in R10 and R11.  */
      val = extract_unsigned_integer (valbuf, 4);
      regcache_cooked_write_unsigned (regcache, ARG1_REGNUM, val);
      val = extract_unsigned_integer ((char *)valbuf + 4, len - 4);
      regcache_cooked_write_unsigned (regcache, ARG2_REGNUM, val);
    }
  else
    error (_("cris_store_return_value: type length too large."));
}

/* Return the name of register regno as a string. Return NULL for an invalid or
   unimplemented register.  */

static const char *
cris_special_register_name (int regno)
{
  int spec_regno;
  int i;

  /* Special register (R16 - R31).  cris_spec_regs is zero-based. 
     Adjust regno accordingly.  */
  spec_regno = regno - NUM_GENREGS;
  
  /* Assume nothing about the layout of the cris_spec_regs struct
     when searching.  */
  for (i = 0; cris_spec_regs[i].name != NULL; i++)
    {
      if (cris_spec_regs[i].number == spec_regno 
	  && cris_spec_reg_applicable (cris_spec_regs[i]))
	/* Go with the first applicable register.  */
	return cris_spec_regs[i].name;
    }
  /* Special register not applicable to this CRIS version.  */
  return NULL;
}

static const char *
cris_register_name (int regno)
{
  static char *cris_genreg_names[] =
  { "r0",  "r1",  "r2",  "r3", \
    "r4",  "r5",  "r6",  "r7", \
    "r8",  "r9",  "r10", "r11", \
    "r12", "r13", "sp",  "pc" };

  if (regno >= 0 && regno < NUM_GENREGS)
    {
      /* General register.  */
      return cris_genreg_names[regno];
    }
  else if (regno >= NUM_GENREGS && regno < gdbarch_num_regs (current_gdbarch))
    {
      return cris_special_register_name (regno);
    }
  else
    {
      /* Invalid register.  */
      return NULL;
    }
}

static const char *
crisv32_register_name (int regno)
{
  static char *crisv32_genreg_names[] =
    { "r0",  "r1",  "r2",  "r3", \
      "r4",  "r5",  "r6",  "r7", \
      "r8",  "r9",  "r10", "r11", \
      "r12", "r13", "sp",  "acr"
    };

  static char *crisv32_sreg_names[] =
    { "s0",  "s1",  "s2",  "s3", \
      "s4",  "s5",  "s6",  "s7", \
      "s8",  "s9",  "s10", "s11", \
      "s12", "s13", "s14",  "s15"
    };

  if (regno >= 0 && regno < NUM_GENREGS)
    {
      /* General register.  */
      return crisv32_genreg_names[regno];
    }
  else if (regno >= NUM_GENREGS && regno < (NUM_GENREGS + NUM_SPECREGS))
    {
      return cris_special_register_name (regno);
    }
  else if (regno == gdbarch_pc_regnum (current_gdbarch))
    {
      return "pc";
    }
  else if (regno >= S0_REGNUM && regno <= S15_REGNUM)
    {
      return crisv32_sreg_names[regno - S0_REGNUM];
    }
  else
    {
      /* Invalid register.  */
      return NULL;
    }
}

/* Convert DWARF register number REG to the appropriate register
   number used by GDB.  */

static int
cris_dwarf2_reg_to_regnum (int reg)
{
  /* We need to re-map a couple of registers (SRP is 16 in Dwarf-2 register
     numbering, MOF is 18).
     Adapted from gcc/config/cris/cris.h.  */
  static int cris_dwarf_regmap[] = {
    0,  1,  2,  3,
    4,  5,  6,  7,
    8,  9,  10, 11,
    12, 13, 14, 15,
    27, -1, -1, -1,
    -1, -1, -1, 23,
    -1, -1, -1, 27,
    -1, -1, -1, -1
  };
  int regnum = -1;

  if (reg >= 0 && reg < ARRAY_SIZE (cris_dwarf_regmap))
    regnum = cris_dwarf_regmap[reg];

  if (regnum == -1)
    warning (_("Unmapped DWARF Register #%d encountered."), reg);

  return regnum;
}

/* DWARF-2 frame support.  */

static void
cris_dwarf2_frame_init_reg (struct gdbarch *gdbarch, int regnum,
                            struct dwarf2_frame_state_reg *reg,
			    struct frame_info *next_frame)
{
  /* The return address column.  */
  if (regnum == gdbarch_pc_regnum (current_gdbarch))
    reg->how = DWARF2_FRAME_REG_RA;

  /* The call frame address.  */
  else if (regnum == gdbarch_sp_regnum (current_gdbarch))
    reg->how = DWARF2_FRAME_REG_CFA;
}

/* Extract from an array regbuf containing the raw register state a function
   return value of type type, and copy that, in virtual format, into 
   valbuf.  */

/* In the CRIS ABI, R10 and R11 are used to store return values.  */

static void
cris_extract_return_value (struct type *type, struct regcache *regcache,
			   void *valbuf)
{
  ULONGEST val;
  int len = TYPE_LENGTH (type);
  
  if (len <= 4)
    {
      /* Get the return value from R10.  */
      regcache_cooked_read_unsigned (regcache, ARG1_REGNUM, &val);
      store_unsigned_integer (valbuf, len, val);
    }
  else if (len <= 8)
    {
      /* Get the return value from R10 and R11.  */
      regcache_cooked_read_unsigned (regcache, ARG1_REGNUM, &val);
      store_unsigned_integer (valbuf, 4, val);
      regcache_cooked_read_unsigned (regcache, ARG2_REGNUM, &val);
      store_unsigned_integer ((char *)valbuf + 4, len - 4, val);
    }
  else
    error (_("cris_extract_return_value: type length too large"));
}

/* Handle the CRIS return value convention.  */

static enum return_value_convention
cris_return_value (struct gdbarch *gdbarch, struct type *type,
		   struct regcache *regcache, gdb_byte *readbuf,
		   const gdb_byte *writebuf)
{
  if (TYPE_CODE (type) == TYPE_CODE_STRUCT 
      || TYPE_CODE (type) == TYPE_CODE_UNION
      || TYPE_LENGTH (type) > 8)
    /* Structs, unions, and anything larger than 8 bytes (2 registers)
       goes on the stack.  */
    return RETURN_VALUE_STRUCT_CONVENTION;

  if (readbuf)
    cris_extract_return_value (type, regcache, readbuf);
  if (writebuf)
    cris_store_return_value (type, regcache, writebuf);

  return RETURN_VALUE_REGISTER_CONVENTION;
}

/* Returns 1 if the given type will be passed by pointer rather than 
   directly.  */

/* In the CRIS ABI, arguments shorter than or equal to 64 bits are passed
   by value.  */

static int 
cris_reg_struct_has_addr (int gcc_p, struct type *type)
{ 
  return (TYPE_LENGTH (type) > 8);
}

/* Calculates a value that measures how good inst_args constraints an 
   instruction.  It stems from cris_constraint, found in cris-dis.c.  */

static int
constraint (unsigned int insn, const signed char *inst_args, 
            inst_env_type *inst_env)
{
  int retval = 0;
  int tmp, i;

  const char *s = inst_args;

  for (; *s; s++)
    switch (*s) 
      {
      case 'm':
        if ((insn & 0x30) == 0x30)
          return -1;
        break;
        
      case 'S':
        /* A prefix operand.  */
        if (inst_env->prefix_found)
          break;
        else
          return -1;

      case 'B':
        /* A "push" prefix.  (This check was REMOVED by san 970921.)  Check for
           valid "push" size.  In case of special register, it may be != 4.  */
        if (inst_env->prefix_found)
          break;
        else
          return -1;

      case 'D':
        retval = (((insn >> 0xC) & 0xF) == (insn & 0xF));
        if (!retval)
          return -1;
        else 
          retval += 4;
        break;

      case 'P':
        tmp = (insn >> 0xC) & 0xF;

        for (i = 0; cris_spec_regs[i].name != NULL; i++)
          {
            /* Since we match four bits, we will give a value of
               4 - 1 = 3 in a match.  If there is a corresponding
               exact match of a special register in another pattern, it
               will get a value of 4, which will be higher.  This should
               be correct in that an exact pattern would match better that
               a general pattern.
               Note that there is a reason for not returning zero; the
               pattern for "clear" is partly  matched in the bit-pattern
               (the two lower bits must be zero), while the bit-pattern
               for a move from a special register is matched in the
               register constraint.
               This also means we will will have a race condition if
               there is a partly match in three bits in the bit pattern.  */
            if (tmp == cris_spec_regs[i].number)
              {
                retval += 3;
                break;
              }
          }
        
        if (cris_spec_regs[i].name == NULL)
          return -1;
        break;
      }
  return retval;
}

/* Returns the number of bits set in the variable value.  */

static int
number_of_bits (unsigned int value)
{
  int number_of_bits = 0;
  
  while (value != 0)
    {
      number_of_bits += 1;
      value &= (value - 1);
    }
  return number_of_bits;
}

/* Finds the address that should contain the single step breakpoint(s). 
   It stems from code in cris-dis.c.  */

static int
find_cris_op (unsigned short insn, inst_env_type *inst_env)
{
  int i;
  int max_level_of_match = -1;
  int max_matched = -1;
  int level_of_match;

  for (i = 0; cris_opcodes[i].name != NULL; i++)
    {
      if (((cris_opcodes[i].match & insn) == cris_opcodes[i].match) 
          && ((cris_opcodes[i].lose & insn) == 0)
	  /* Only CRISv10 instructions, please.  */
	  && (cris_opcodes[i].applicable_version != cris_ver_v32p))
        {
          level_of_match = constraint (insn, cris_opcodes[i].args, inst_env);
          if (level_of_match >= 0)
            {
              level_of_match +=
                number_of_bits (cris_opcodes[i].match | cris_opcodes[i].lose);
              if (level_of_match > max_level_of_match)
                {
                  max_matched = i;
                  max_level_of_match = level_of_match;
                  if (level_of_match == 16)
                    {
                      /* All bits matched, cannot find better.  */
                      break;
                    }
                }
            }
        }
    }
  return max_matched;
}

/* Attempts to find single-step breakpoints.  Returns -1 on failure which is
   actually an internal error.  */

static int
find_step_target (struct frame_info *frame, inst_env_type *inst_env)
{
  int i;
  int offset;
  unsigned short insn;

  /* Create a local register image and set the initial state.  */
  for (i = 0; i < NUM_GENREGS; i++)
    {
      inst_env->reg[i] = 
	(unsigned long) get_frame_register_unsigned (frame, i);
    }
  offset = NUM_GENREGS;
  for (i = 0; i < NUM_SPECREGS; i++)
    {
      inst_env->preg[i] = 
	(unsigned long) get_frame_register_unsigned (frame, offset + i);
    }
  inst_env->branch_found = 0;
  inst_env->slot_needed = 0;
  inst_env->delay_slot_pc_active = 0;
  inst_env->prefix_found = 0;
  inst_env->invalid = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;

  /* Look for a step target.  */
  do
    {
      /* Read an instruction from the client.  */
      insn = read_memory_unsigned_integer
	     (inst_env->reg[gdbarch_pc_regnum (current_gdbarch)], 2);

      /* If the instruction is not in a delay slot the new content of the
         PC is [PC] + 2.  If the instruction is in a delay slot it is not
         that simple.  Since a instruction in a delay slot cannot change 
         the content of the PC, it does not matter what value PC will have. 
         Just make sure it is a valid instruction.  */
      if (!inst_env->delay_slot_pc_active)
        {
          inst_env->reg[gdbarch_pc_regnum (current_gdbarch)] += 2;
        }
      else
        {
          inst_env->delay_slot_pc_active = 0;
          inst_env->reg[gdbarch_pc_regnum (current_gdbarch)]
	    = inst_env->delay_slot_pc;
        }
      /* Analyse the present instruction.  */
      i = find_cris_op (insn, inst_env);
      if (i == -1)
        {
          inst_env->invalid = 1;
        }
      else
        {
          cris_gdb_func (cris_opcodes[i].op, insn, inst_env);
        }
    } while (!inst_env->invalid 
             && (inst_env->prefix_found || inst_env->xflag_found 
                 || inst_env->slot_needed));
  return i;
}

/* There is no hardware single-step support.  The function find_step_target
   digs through the opcodes in order to find all possible targets. 
   Either one ordinary target or two targets for branches may be found.  */

static int
cris_software_single_step (struct frame_info *frame)
{
  inst_env_type inst_env;

  /* Analyse the present instruction environment and insert 
     breakpoints.  */
  int status = find_step_target (frame, &inst_env);
  if (status == -1)
    {
      /* Could not find a target.  Things are likely to go downhill 
	 from here.  */
      warning (_("CRIS software single step could not find a step target."));
    }
  else
    {
      /* Insert at most two breakpoints.  One for the next PC content
         and possibly another one for a branch, jump, etc.  */
      CORE_ADDR next_pc =
	(CORE_ADDR) inst_env.reg[gdbarch_pc_regnum (current_gdbarch)];
      insert_single_step_breakpoint (next_pc);
      if (inst_env.branch_found 
	  && (CORE_ADDR) inst_env.branch_break_address != next_pc)
	{
	  CORE_ADDR branch_target_address
		= (CORE_ADDR) inst_env.branch_break_address;
	  insert_single_step_breakpoint (branch_target_address);
	}
    }

  return 1;
}

/* Calculates the prefix value for quick offset addressing mode.  */

static void
quick_mode_bdap_prefix (unsigned short inst, inst_env_type *inst_env)
{
  /* It's invalid to be in a delay slot.  You can't have a prefix to this
     instruction (not 100% sure).  */
  if (inst_env->slot_needed || inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return; 
    }
 
  inst_env->prefix_value = inst_env->reg[cris_get_operand2 (inst)];
  inst_env->prefix_value += cris_get_bdap_quick_offset (inst);

  /* A prefix doesn't change the xflag_found.  But the rest of the flags
     need updating.  */
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 1;
}

/* Updates the autoincrement register.  The size of the increment is derived 
   from the size of the operation.  The PC is always kept aligned on even
   word addresses.  */

static void 
process_autoincrement (int size, unsigned short inst, inst_env_type *inst_env)
{
  if (size == INST_BYTE_SIZE)
    {
      inst_env->reg[cris_get_operand1 (inst)] += 1;

      /* The PC must be word aligned, so increase the PC with one
         word even if the size is byte.  */
      if (cris_get_operand1 (inst) == REG_PC)
        {
          inst_env->reg[REG_PC] += 1;
        }
    }
  else if (size == INST_WORD_SIZE)
    {
      inst_env->reg[cris_get_operand1 (inst)] += 2;
    }
  else if (size == INST_DWORD_SIZE)
    {
      inst_env->reg[cris_get_operand1 (inst)] += 4;
    }
  else
    {
      /* Invalid size.  */
      inst_env->invalid = 1;
    }
}

/* Just a forward declaration.  */

static unsigned long get_data_from_address (unsigned short *inst,
					    CORE_ADDR address);

/* Calculates the prefix value for the general case of offset addressing 
   mode.  */

static void
bdap_prefix (unsigned short inst, inst_env_type *inst_env)
{

  long offset;

  /* It's invalid to be in a delay slot.  */
  if (inst_env->slot_needed || inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return; 
    }

  /* The calculation of prefix_value used to be after process_autoincrement,
     but that fails for an instruction such as jsr [$r0+12] which is encoded
     as 5f0d 0c00 30b9 when compiled with -fpic.  Since PC is operand1 it
     mustn't be incremented until we have read it and what it points at.  */
  inst_env->prefix_value = inst_env->reg[cris_get_operand2 (inst)];

  /* The offset is an indirection of the contents of the operand1 register.  */
  inst_env->prefix_value += 
    get_data_from_address (&inst, inst_env->reg[cris_get_operand1 (inst)]);
  
  if (cris_get_mode (inst) == AUTOINC_MODE)
    {
      process_autoincrement (cris_get_size (inst), inst, inst_env); 
    }
   
  /* A prefix doesn't change the xflag_found.  But the rest of the flags
     need updating.  */
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 1;
}

/* Calculates the prefix value for the index addressing mode.  */

static void
biap_prefix (unsigned short inst, inst_env_type *inst_env)
{
  /* It's invalid to be in a delay slot.  I can't see that it's possible to
     have a prefix to this instruction.  So I will treat this as invalid.  */
  if (inst_env->slot_needed || inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }
  
  inst_env->prefix_value = inst_env->reg[cris_get_operand1 (inst)];

  /* The offset is the operand2 value shifted the size of the instruction 
     to the left.  */
  inst_env->prefix_value += 
    inst_env->reg[cris_get_operand2 (inst)] << cris_get_size (inst);
  
  /* If the PC is operand1 (base) the address used is the address after 
     the main instruction, i.e. address + 2 (the PC is already compensated
     for the prefix operation).  */
  if (cris_get_operand1 (inst) == REG_PC)
    {
      inst_env->prefix_value += 2;
    }

  /* A prefix doesn't change the xflag_found.  But the rest of the flags
     need updating.  */
  inst_env->slot_needed = 0;
  inst_env->xflag_found = 0;
  inst_env->prefix_found = 1;
}

/* Calculates the prefix value for the double indirect addressing mode.  */

static void 
dip_prefix (unsigned short inst, inst_env_type *inst_env)
{

  CORE_ADDR address;

  /* It's invalid to be in a delay slot.  */
  if (inst_env->slot_needed || inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }
  
  /* The prefix value is one dereference of the contents of the operand1
     register.  */
  address = (CORE_ADDR) inst_env->reg[cris_get_operand1 (inst)];
  inst_env->prefix_value = read_memory_unsigned_integer (address, 4);
    
  /* Check if the mode is autoincrement.  */
  if (cris_get_mode (inst) == AUTOINC_MODE)
    {
      inst_env->reg[cris_get_operand1 (inst)] += 4;
    }

  /* A prefix doesn't change the xflag_found.  But the rest of the flags
     need updating.  */
  inst_env->slot_needed = 0;
  inst_env->xflag_found = 0;
  inst_env->prefix_found = 1;
}

/* Finds the destination for a branch with 8-bits offset.  */

static void
eight_bit_offset_branch_op (unsigned short inst, inst_env_type *inst_env)
{

  short offset;

  /* If we have a prefix or are in a delay slot it's bad.  */
  if (inst_env->slot_needed || inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }
  
  /* We have a branch, find out where the branch will land.  */
  offset = cris_get_branch_short_offset (inst);

  /* Check if the offset is signed.  */
  if (offset & BRANCH_SIGNED_SHORT_OFFSET_MASK)
    {
      offset |= 0xFF00;
    }
  
  /* The offset ends with the sign bit, set it to zero.  The address
     should always be word aligned.  */
  offset &= ~BRANCH_SIGNED_SHORT_OFFSET_MASK;
  
  inst_env->branch_found = 1;
  inst_env->branch_break_address = inst_env->reg[REG_PC] + offset;

  inst_env->slot_needed = 1;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 1;
}

/* Finds the destination for a branch with 16-bits offset.  */

static void 
sixteen_bit_offset_branch_op (unsigned short inst, inst_env_type *inst_env)
{
  short offset;

  /* If we have a prefix or is in a delay slot it's bad.  */
  if (inst_env->slot_needed || inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }

  /* We have a branch, find out the offset for the branch.  */
  offset = read_memory_integer (inst_env->reg[REG_PC], 2);

  /* The instruction is one word longer than normal, so add one word
     to the PC.  */
  inst_env->reg[REG_PC] += 2;

  inst_env->branch_found = 1;
  inst_env->branch_break_address = inst_env->reg[REG_PC] + offset;


  inst_env->slot_needed = 1;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 1;
}

/* Handles the ABS instruction.  */

static void 
abs_op (unsigned short inst, inst_env_type *inst_env)
{

  long value;
  
  /* ABS can't have a prefix, so it's bad if it does.  */
  if (inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }

  /* Check if the operation affects the PC.  */
  if (cris_get_operand2 (inst) == REG_PC)
    {
    
      /* It's invalid to change to the PC if we are in a delay slot.  */
      if (inst_env->slot_needed)
        {
          inst_env->invalid = 1;
          return;
        }

      value = (long) inst_env->reg[REG_PC];

      /* The value of abs (SIGNED_DWORD_MASK) is SIGNED_DWORD_MASK.  */
      if (value != SIGNED_DWORD_MASK)
        {
          value = -value;
          inst_env->reg[REG_PC] = (long) value;
        }
    }

  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Handles the ADDI instruction.  */

static void 
addi_op (unsigned short inst, inst_env_type *inst_env)
{
  /* It's invalid to have the PC as base register.  And ADDI can't have
     a prefix.  */
  if (inst_env->prefix_found || (cris_get_operand1 (inst) == REG_PC))
    {
      inst_env->invalid = 1;
      return;
    }

  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Handles the ASR instruction.  */

static void 
asr_op (unsigned short inst, inst_env_type *inst_env)
{
  int shift_steps;
  unsigned long value;
  unsigned long signed_extend_mask = 0;

  /* ASR can't have a prefix, so check that it doesn't.  */
  if (inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }

  /* Check if the PC is the target register.  */
  if (cris_get_operand2 (inst) == REG_PC)
    {
      /* It's invalid to change the PC in a delay slot.  */
      if (inst_env->slot_needed)
        {
          inst_env->invalid = 1;
          return;
        }
      /* Get the number of bits to shift.  */
      shift_steps = cris_get_asr_shift_steps (inst_env->reg[cris_get_operand1 (inst)]);
      value = inst_env->reg[REG_PC];

      /* Find out how many bits the operation should apply to.  */
      if (cris_get_size (inst) == INST_BYTE_SIZE)
        {
          if (value & SIGNED_BYTE_MASK)
            {
              signed_extend_mask = 0xFF;
              signed_extend_mask = signed_extend_mask >> shift_steps;
              signed_extend_mask = ~signed_extend_mask;
            }
          value = value >> shift_steps;
          value |= signed_extend_mask;
          value &= 0xFF;
          inst_env->reg[REG_PC] &= 0xFFFFFF00;
          inst_env->reg[REG_PC] |= value;
        }
      else if (cris_get_size (inst) == INST_WORD_SIZE)
        {
          if (value & SIGNED_WORD_MASK)
            {
              signed_extend_mask = 0xFFFF;
              signed_extend_mask = signed_extend_mask >> shift_steps;
              signed_extend_mask = ~signed_extend_mask;
            }
          value = value >> shift_steps;
          value |= signed_extend_mask;
          value &= 0xFFFF;
          inst_env->reg[REG_PC] &= 0xFFFF0000;
          inst_env->reg[REG_PC] |= value;
        }
      else if (cris_get_size (inst) == INST_DWORD_SIZE)
        {
          if (value & SIGNED_DWORD_MASK)
            {
              signed_extend_mask = 0xFFFFFFFF;
              signed_extend_mask = signed_extend_mask >> shift_steps;
              signed_extend_mask = ~signed_extend_mask;
            }
          value = value >> shift_steps;
          value |= signed_extend_mask;
          inst_env->reg[REG_PC]  = value;
        }
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Handles the ASRQ instruction.  */

static void 
asrq_op (unsigned short inst, inst_env_type *inst_env)
{

  int shift_steps;
  unsigned long value;
  unsigned long signed_extend_mask = 0;
  
  /* ASRQ can't have a prefix, so check that it doesn't.  */
  if (inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }

  /* Check if the PC is the target register.  */
  if (cris_get_operand2 (inst) == REG_PC)
    {

      /* It's invalid to change the PC in a delay slot.  */
      if (inst_env->slot_needed)
        {
          inst_env->invalid = 1;
          return;
        }
      /* The shift size is given as a 5 bit quick value, i.e. we don't
         want the the sign bit of the quick value.  */
      shift_steps = cris_get_asr_shift_steps (inst);
      value = inst_env->reg[REG_PC];
      if (value & SIGNED_DWORD_MASK)
        {
          signed_extend_mask = 0xFFFFFFFF;
          signed_extend_mask = signed_extend_mask >> shift_steps;
          signed_extend_mask = ~signed_extend_mask;
        }
      value = value >> shift_steps;
      value |= signed_extend_mask;
      inst_env->reg[REG_PC]  = value;
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Handles the AX, EI and SETF instruction.  */

static void 
ax_ei_setf_op (unsigned short inst, inst_env_type *inst_env)
{
  if (inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }
  /* Check if the instruction is setting the X flag.  */
  if (cris_is_xflag_bit_on (inst))
    {
      inst_env->xflag_found = 1;
    }
  else
    {
      inst_env->xflag_found = 0;
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->disable_interrupt = 1;
}

/* Checks if the instruction is in assign mode.  If so, it updates the assign 
   register.  Note that check_assign assumes that the caller has checked that
   there is a prefix to this instruction.  The mode check depends on this.  */

static void 
check_assign (unsigned short inst, inst_env_type *inst_env)
{
  /* Check if it's an assign addressing mode.  */
  if (cris_get_mode (inst) == PREFIX_ASSIGN_MODE)
    {
      /* Assign the prefix value to operand 1.  */
      inst_env->reg[cris_get_operand1 (inst)] = inst_env->prefix_value;
    }
}

/* Handles the 2-operand BOUND instruction.  */

static void 
two_operand_bound_op (unsigned short inst, inst_env_type *inst_env)
{
  /* It's invalid to have the PC as the index operand.  */
  if (cris_get_operand2 (inst) == REG_PC)
    {
      inst_env->invalid = 1;
      return;
    }
  /* Check if we have a prefix.  */
  if (inst_env->prefix_found)
    {
      check_assign (inst, inst_env);
    }
  /* Check if this is an autoincrement mode.  */
  else if (cris_get_mode (inst) == AUTOINC_MODE)
    {
      /* It's invalid to change the PC in a delay slot.  */
      if (inst_env->slot_needed)
        {
          inst_env->invalid = 1;
          return;
        }
      process_autoincrement (cris_get_size (inst), inst, inst_env);
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Handles the 3-operand BOUND instruction.  */

static void 
three_operand_bound_op (unsigned short inst, inst_env_type *inst_env)
{
  /* It's an error if we haven't got a prefix.  And it's also an error
     if the PC is the destination register.  */
  if ((!inst_env->prefix_found) || (cris_get_operand1 (inst) == REG_PC))
    {
      inst_env->invalid = 1;
      return;
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Clears the status flags in inst_env.  */

static void 
btst_nop_op (unsigned short inst, inst_env_type *inst_env)
{
  /* It's an error if we have got a prefix.  */
  if (inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }

  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Clears the status flags in inst_env.  */

static void 
clearf_di_op (unsigned short inst, inst_env_type *inst_env)
{
  /* It's an error if we have got a prefix.  */
  if (inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }

  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 1;
}

/* Handles the CLEAR instruction if it's in register mode.  */

static void 
reg_mode_clear_op (unsigned short inst, inst_env_type *inst_env)
{
  /* Check if the target is the PC.  */
  if (cris_get_operand2 (inst) == REG_PC)
    {
      /* The instruction will clear the instruction's size bits.  */
      int clear_size = cris_get_clear_size (inst);
      if (clear_size == INST_BYTE_SIZE)
        {
          inst_env->delay_slot_pc = inst_env->reg[REG_PC] & 0xFFFFFF00;
        }
      if (clear_size == INST_WORD_SIZE)
        {
          inst_env->delay_slot_pc = inst_env->reg[REG_PC] & 0xFFFF0000;
        }
      if (clear_size == INST_DWORD_SIZE)
        {
          inst_env->delay_slot_pc = 0x0;
        }
      /* The jump will be delayed with one delay slot.  So we need a delay 
         slot.  */
      inst_env->slot_needed = 1;
      inst_env->delay_slot_pc_active = 1;
    }
  else
    {
      /* The PC will not change => no delay slot.  */
      inst_env->slot_needed = 0;
    }
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Handles the TEST instruction if it's in register mode.  */

static void
reg_mode_test_op (unsigned short inst, inst_env_type *inst_env)
{
  /* It's an error if we have got a prefix.  */
  if (inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;

}

/* Handles the CLEAR and TEST instruction if the instruction isn't 
   in register mode.  */

static void 
none_reg_mode_clear_test_op (unsigned short inst, inst_env_type *inst_env)
{
  /* Check if we are in a prefix mode.  */
  if (inst_env->prefix_found)
    {
      /* The only way the PC can change is if this instruction is in
         assign addressing mode.  */
      check_assign (inst, inst_env);
    }
  /* Indirect mode can't change the PC so just check if the mode is
     autoincrement.  */
  else if (cris_get_mode (inst) == AUTOINC_MODE)
    {
      process_autoincrement (cris_get_size (inst), inst, inst_env);
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Checks that the PC isn't the destination register or the instructions has
   a prefix.  */

static void 
dstep_logshift_mstep_neg_not_op (unsigned short inst, inst_env_type *inst_env)
{
  /* It's invalid to have the PC as the destination.  The instruction can't
     have a prefix.  */
  if ((cris_get_operand2 (inst) == REG_PC) || inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }

  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Checks that the instruction doesn't have a prefix.  */

static void
break_op (unsigned short inst, inst_env_type *inst_env)
{
  /* The instruction can't have a prefix.  */
  if (inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }

  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 1;
}

/* Checks that the PC isn't the destination register and that the instruction
   doesn't have a prefix.  */

static void
scc_op (unsigned short inst, inst_env_type *inst_env)
{
  /* It's invalid to have the PC as the destination.  The instruction can't
     have a prefix.  */
  if ((cris_get_operand2 (inst) == REG_PC) || inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }

  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 1;
}

/* Handles the register mode JUMP instruction.  */

static void 
reg_mode_jump_op (unsigned short inst, inst_env_type *inst_env)
{
  /* It's invalid to do a JUMP in a delay slot.  The mode is register, so 
     you can't have a prefix.  */
  if ((inst_env->slot_needed) || (inst_env->prefix_found))
    {
      inst_env->invalid = 1;
      return;
    }
  
  /* Just change the PC.  */
  inst_env->reg[REG_PC] = inst_env->reg[cris_get_operand1 (inst)];
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 1;
}

/* Handles the JUMP instruction for all modes except register.  */

static void
none_reg_mode_jump_op (unsigned short inst, inst_env_type *inst_env)
{
  unsigned long newpc;
  CORE_ADDR address;

  /* It's invalid to do a JUMP in a delay slot.  */
  if (inst_env->slot_needed)
    {
      inst_env->invalid = 1;
    }
  else
    {
      /* Check if we have a prefix.  */
      if (inst_env->prefix_found)
        {
          check_assign (inst, inst_env);

          /* Get the new value for the the PC.  */
          newpc = 
            read_memory_unsigned_integer ((CORE_ADDR) inst_env->prefix_value,
                                          4);
        }
      else
        {
          /* Get the new value for the PC.  */
          address = (CORE_ADDR) inst_env->reg[cris_get_operand1 (inst)];
          newpc = read_memory_unsigned_integer (address, 4);

          /* Check if we should increment a register.  */
          if (cris_get_mode (inst) == AUTOINC_MODE)
            {
              inst_env->reg[cris_get_operand1 (inst)] += 4;
            }
        }
      inst_env->reg[REG_PC] = newpc;
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 1;
}

/* Handles moves to special registers (aka P-register) for all modes.  */

static void 
move_to_preg_op (unsigned short inst, inst_env_type *inst_env)
{
  if (inst_env->prefix_found)
    {
      /* The instruction has a prefix that means we are only interested if
         the instruction is in assign mode.  */
      if (cris_get_mode (inst) == PREFIX_ASSIGN_MODE)
        {
          /* The prefix handles the problem if we are in a delay slot.  */
          if (cris_get_operand1 (inst) == REG_PC)
            {
              /* Just take care of the assign.  */
              check_assign (inst, inst_env);
            }
        }
    }
  else if (cris_get_mode (inst) == AUTOINC_MODE)
    {
      /* The instruction doesn't have a prefix, the only case left that we
         are interested in is the autoincrement mode.  */
      if (cris_get_operand1 (inst) == REG_PC)
        {
          /* If the PC is to be incremented it's invalid to be in a 
             delay slot.  */
          if (inst_env->slot_needed)
            {
              inst_env->invalid = 1;
              return;
            }

          /* The increment depends on the size of the special register.  */
          if (cris_register_size (cris_get_operand2 (inst)) == 1)
            {
              process_autoincrement (INST_BYTE_SIZE, inst, inst_env);
            }
          else if (cris_register_size (cris_get_operand2 (inst)) == 2)
            {
              process_autoincrement (INST_WORD_SIZE, inst, inst_env);
            }
          else
            {
              process_autoincrement (INST_DWORD_SIZE, inst, inst_env);
            }
        }
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 1;
}

/* Handles moves from special registers (aka P-register) for all modes
   except register.  */

static void 
none_reg_mode_move_from_preg_op (unsigned short inst, inst_env_type *inst_env)
{
  if (inst_env->prefix_found)
    {
      /* The instruction has a prefix that means we are only interested if
         the instruction is in assign mode.  */
      if (cris_get_mode (inst) == PREFIX_ASSIGN_MODE)
        {
          /* The prefix handles the problem if we are in a delay slot.  */
          if (cris_get_operand1 (inst) == REG_PC)
            {
              /* Just take care of the assign.  */
              check_assign (inst, inst_env);
            }
        }
    }    
  /* The instruction doesn't have a prefix, the only case left that we
     are interested in is the autoincrement mode.  */
  else if (cris_get_mode (inst) == AUTOINC_MODE)
    {
      if (cris_get_operand1 (inst) == REG_PC)
        {
          /* If the PC is to be incremented it's invalid to be in a 
             delay slot.  */
          if (inst_env->slot_needed)
            {
              inst_env->invalid = 1;
              return;
            }
          
          /* The increment depends on the size of the special register.  */
          if (cris_register_size (cris_get_operand2 (inst)) == 1)
            {
              process_autoincrement (INST_BYTE_SIZE, inst, inst_env);
            }
          else if (cris_register_size (cris_get_operand2 (inst)) == 2)
            {
              process_autoincrement (INST_WORD_SIZE, inst, inst_env);
            }
          else
            {
              process_autoincrement (INST_DWORD_SIZE, inst, inst_env);
            }
        }
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 1;
}

/* Handles moves from special registers (aka P-register) when the mode
   is register.  */

static void 
reg_mode_move_from_preg_op (unsigned short inst, inst_env_type *inst_env)
{
  /* Register mode move from special register can't have a prefix.  */
  if (inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }

  if (cris_get_operand1 (inst) == REG_PC)
    {
      /* It's invalid to change the PC in a delay slot.  */
      if (inst_env->slot_needed)
        {
          inst_env->invalid = 1;
          return;
        }
      /* The destination is the PC, the jump will have a delay slot.  */
      inst_env->delay_slot_pc = inst_env->preg[cris_get_operand2 (inst)];
      inst_env->slot_needed = 1;
      inst_env->delay_slot_pc_active = 1;
    }
  else
    {
      /* If the destination isn't PC, there will be no jump.  */
      inst_env->slot_needed = 0;
    }
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 1;
}

/* Handles the MOVEM from memory to general register instruction.  */

static void 
move_mem_to_reg_movem_op (unsigned short inst, inst_env_type *inst_env)
{
  if (inst_env->prefix_found)
    {
      /* The prefix handles the problem if we are in a delay slot.  Is the
         MOVEM instruction going to change the PC?  */
      if (cris_get_operand2 (inst) >= REG_PC)
        {
          inst_env->reg[REG_PC] = 
            read_memory_unsigned_integer (inst_env->prefix_value, 4);
        }
      /* The assign value is the value after the increment.  Normally, the   
         assign value is the value before the increment.  */
      if ((cris_get_operand1 (inst) == REG_PC) 
          && (cris_get_mode (inst) == PREFIX_ASSIGN_MODE))
        {
          inst_env->reg[REG_PC] = inst_env->prefix_value;
          inst_env->reg[REG_PC] += 4 * (cris_get_operand2 (inst) + 1);
        }
    }
  else
    {
      /* Is the MOVEM instruction going to change the PC?  */
      if (cris_get_operand2 (inst) == REG_PC)
        {
          /* It's invalid to change the PC in a delay slot.  */
          if (inst_env->slot_needed)
            {
              inst_env->invalid = 1;
              return;
            }
          inst_env->reg[REG_PC] =
            read_memory_unsigned_integer (inst_env->reg[cris_get_operand1 (inst)], 
                                          4);
        }
      /* The increment is not depending on the size, instead it's depending
         on the number of registers loaded from memory.  */
      if ((cris_get_operand1 (inst) == REG_PC) && (cris_get_mode (inst) == AUTOINC_MODE))
        {
          /* It's invalid to change the PC in a delay slot.  */
          if (inst_env->slot_needed)
            {
              inst_env->invalid = 1;
              return;
            }
          inst_env->reg[REG_PC] += 4 * (cris_get_operand2 (inst) + 1); 
        }
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Handles the MOVEM to memory from general register instruction.  */

static void 
move_reg_to_mem_movem_op (unsigned short inst, inst_env_type *inst_env)
{
  if (inst_env->prefix_found)
    {
      /* The assign value is the value after the increment.  Normally, the
         assign value is the value before the increment.  */
      if ((cris_get_operand1 (inst) == REG_PC) &&
          (cris_get_mode (inst) == PREFIX_ASSIGN_MODE))
        {
          /* The prefix handles the problem if we are in a delay slot.  */
          inst_env->reg[REG_PC] = inst_env->prefix_value;
          inst_env->reg[REG_PC] += 4 * (cris_get_operand2 (inst) + 1);
        }
    }
  else
    {
      /* The increment is not depending on the size, instead it's depending
         on the number of registers loaded to memory.  */
      if ((cris_get_operand1 (inst) == REG_PC) && (cris_get_mode (inst) == AUTOINC_MODE))
        {
          /* It's invalid to change the PC in a delay slot.  */
          if (inst_env->slot_needed)
            {
              inst_env->invalid = 1;
              return;
            }
          inst_env->reg[REG_PC] += 4 * (cris_get_operand2 (inst) + 1);
        }
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Handles the intructions that's not yet implemented, by setting 
   inst_env->invalid to true.  */

static void 
not_implemented_op (unsigned short inst, inst_env_type *inst_env)
{
  inst_env->invalid = 1;
}

/* Handles the XOR instruction.  */

static void 
xor_op (unsigned short inst, inst_env_type *inst_env)
{
  /* XOR can't have a prefix.  */
  if (inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }

  /* Check if the PC is the target.  */
  if (cris_get_operand2 (inst) == REG_PC)
    {
      /* It's invalid to change the PC in a delay slot.  */
      if (inst_env->slot_needed)
        {
          inst_env->invalid = 1;
          return;
        }
      inst_env->reg[REG_PC] ^= inst_env->reg[cris_get_operand1 (inst)];
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Handles the MULS instruction.  */

static void 
muls_op (unsigned short inst, inst_env_type *inst_env)
{
  /* MULS/U can't have a prefix.  */
  if (inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }

  /* Consider it invalid if the PC is the target.  */
  if (cris_get_operand2 (inst) == REG_PC)
    {
      inst_env->invalid = 1;
      return;
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Handles the MULU instruction.  */

static void 
mulu_op (unsigned short inst, inst_env_type *inst_env)
{
  /* MULS/U can't have a prefix.  */
  if (inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }

  /* Consider it invalid if the PC is the target.  */
  if (cris_get_operand2 (inst) == REG_PC)
    {
      inst_env->invalid = 1;
      return;
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Calculate the result of the instruction for ADD, SUB, CMP AND, OR and MOVE. 
   The MOVE instruction is the move from source to register.  */

static void 
add_sub_cmp_and_or_move_action (unsigned short inst, inst_env_type *inst_env, 
                                unsigned long source1, unsigned long source2)
{
  unsigned long pc_mask;
  unsigned long operation_mask;
  
  /* Find out how many bits the operation should apply to.  */
  if (cris_get_size (inst) == INST_BYTE_SIZE)
    {
      pc_mask = 0xFFFFFF00; 
      operation_mask = 0xFF;
    }
  else if (cris_get_size (inst) == INST_WORD_SIZE)
    {
      pc_mask = 0xFFFF0000;
      operation_mask = 0xFFFF;
    }
  else if (cris_get_size (inst) == INST_DWORD_SIZE)
    {
      pc_mask = 0x0;
      operation_mask = 0xFFFFFFFF;
    }
  else
    {
      /* The size is out of range.  */
      inst_env->invalid = 1;
      return;
    }

  /* The instruction just works on uw_operation_mask bits.  */
  source2 &= operation_mask;
  source1 &= operation_mask;

  /* Now calculate the result.  The opcode's 3 first bits separates
     the different actions.  */
  switch (cris_get_opcode (inst) & 7)
    {
    case 0:  /* add */
      source1 += source2;
      break;

    case 1:  /* move */
      source1 = source2;
      break;

    case 2:  /* subtract */
      source1 -= source2;
      break;

    case 3:  /* compare */
      break;

    case 4:  /* and */
      source1 &= source2;
      break;

    case 5:  /* or */
      source1 |= source2;
      break;

    default:
      inst_env->invalid = 1;
      return;

      break;
    }

  /* Make sure that the result doesn't contain more than the instruction
     size bits.  */
  source2 &= operation_mask;

  /* Calculate the new breakpoint address.  */
  inst_env->reg[REG_PC] &= pc_mask;
  inst_env->reg[REG_PC] |= source1;

}

/* Extends the value from either byte or word size to a dword.  If the mode
   is zero extend then the value is extended with zero.  If instead the mode
   is signed extend the sign bit of the value is taken into consideration.  */

static unsigned long 
do_sign_or_zero_extend (unsigned long value, unsigned short *inst)
{
  /* The size can be either byte or word, check which one it is. 
     Don't check the highest bit, it's indicating if it's a zero
     or sign extend.  */
  if (cris_get_size (*inst) & INST_WORD_SIZE)
    {
      /* Word size.  */
      value &= 0xFFFF;

      /* Check if the instruction is signed extend.  If so, check if value has
         the sign bit on.  */
      if (cris_is_signed_extend_bit_on (*inst) && (value & SIGNED_WORD_MASK))
        {
          value |= SIGNED_WORD_EXTEND_MASK;
        } 
    }
  else
    {
      /* Byte size.  */
      value &= 0xFF;

      /* Check if the instruction is signed extend.  If so, check if value has
         the sign bit on.  */
      if (cris_is_signed_extend_bit_on (*inst) && (value & SIGNED_BYTE_MASK))
        {
          value |= SIGNED_BYTE_EXTEND_MASK;
        }
    }
  /* The size should now be dword.  */
  cris_set_size_to_dword (inst);
  return value;
}

/* Handles the register mode for the ADD, SUB, CMP, AND, OR and MOVE
   instruction.  The MOVE instruction is the move from source to register.  */

static void 
reg_mode_add_sub_cmp_and_or_move_op (unsigned short inst,
                                     inst_env_type *inst_env)
{
  unsigned long operand1;
  unsigned long operand2;

  /* It's invalid to have a prefix to the instruction.  This is a register 
     mode instruction and can't have a prefix.  */
  if (inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }
  /* Check if the instruction has PC as its target.  */
  if (cris_get_operand2 (inst) == REG_PC)
    {
      if (inst_env->slot_needed)
        {
          inst_env->invalid = 1;
          return;
        }
      /* The instruction has the PC as its target register.  */
      operand1 = inst_env->reg[cris_get_operand1 (inst)]; 
      operand2 = inst_env->reg[REG_PC];

      /* Check if it's a extend, signed or zero instruction.  */
      if (cris_get_opcode (inst) < 4)
        {
          operand1 = do_sign_or_zero_extend (operand1, &inst);
        }
      /* Calculate the PC value after the instruction, i.e. where the
         breakpoint should be.  The order of the udw_operands is vital.  */
      add_sub_cmp_and_or_move_action (inst, inst_env, operand2, operand1); 
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Returns the data contained at address.  The size of the data is derived from
   the size of the operation.  If the instruction is a zero or signed
   extend instruction, the size field is changed in instruction.  */

static unsigned long 
get_data_from_address (unsigned short *inst, CORE_ADDR address)
{
  int size = cris_get_size (*inst);
  unsigned long value;

  /* If it's an extend instruction we don't want the signed extend bit,
     because it influences the size.  */
  if (cris_get_opcode (*inst) < 4)
    {
      size &= ~SIGNED_EXTEND_BIT_MASK;
    }
  /* Is there a need for checking the size?  Size should contain the number of
     bytes to read.  */
  size = 1 << size;
  value = read_memory_unsigned_integer (address, size);

  /* Check if it's an extend, signed or zero instruction.  */
  if (cris_get_opcode (*inst) < 4)
    {
      value = do_sign_or_zero_extend (value, inst);
    }
  return value;
}

/* Handles the assign addresing mode for the ADD, SUB, CMP, AND, OR and MOVE 
   instructions.  The MOVE instruction is the move from source to register.  */

static void 
handle_prefix_assign_mode_for_aritm_op (unsigned short inst, 
                                        inst_env_type *inst_env)
{
  unsigned long operand2;
  unsigned long operand3;

  check_assign (inst, inst_env);
  if (cris_get_operand2 (inst) == REG_PC)
    {
      operand2 = inst_env->reg[REG_PC];

      /* Get the value of the third operand.  */
      operand3 = get_data_from_address (&inst, inst_env->prefix_value);

      /* Calculate the PC value after the instruction, i.e. where the
         breakpoint should be.  The order of the udw_operands is vital.  */
      add_sub_cmp_and_or_move_action (inst, inst_env, operand2, operand3);
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Handles the three-operand addressing mode for the ADD, SUB, CMP, AND and
   OR instructions.  Note that for this to work as expected, the calling
   function must have made sure that there is a prefix to this instruction.  */

static void 
three_operand_add_sub_cmp_and_or_op (unsigned short inst, 
                                     inst_env_type *inst_env)
{
  unsigned long operand2;
  unsigned long operand3;

  if (cris_get_operand1 (inst) == REG_PC)
    {
      /* The PC will be changed by the instruction.  */
      operand2 = inst_env->reg[cris_get_operand2 (inst)];

      /* Get the value of the third operand.  */
      operand3 = get_data_from_address (&inst, inst_env->prefix_value);

      /* Calculate the PC value after the instruction, i.e. where the
         breakpoint should be.  */
      add_sub_cmp_and_or_move_action (inst, inst_env, operand2, operand3);
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Handles the index addresing mode for the ADD, SUB, CMP, AND, OR and MOVE
   instructions.  The MOVE instruction is the move from source to register.  */

static void 
handle_prefix_index_mode_for_aritm_op (unsigned short inst, 
                                       inst_env_type *inst_env)
{
  if (cris_get_operand1 (inst) != cris_get_operand2 (inst))
    {
      /* If the instruction is MOVE it's invalid.  If the instruction is ADD,
         SUB, AND or OR something weird is going on (if everything works these
         instructions should end up in the three operand version).  */
      inst_env->invalid = 1;
      return;
    }
  else
    {
      /* three_operand_add_sub_cmp_and_or does the same as we should do here
         so use it.  */
      three_operand_add_sub_cmp_and_or_op (inst, inst_env);
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Handles the autoincrement and indirect addresing mode for the ADD, SUB,
   CMP, AND OR and MOVE instruction.  The MOVE instruction is the move from
   source to register.  */

static void 
handle_inc_and_index_mode_for_aritm_op (unsigned short inst, 
                                        inst_env_type *inst_env)
{
  unsigned long operand1;
  unsigned long operand2;
  unsigned long operand3;
  int size;

  /* The instruction is either an indirect or autoincrement addressing mode. 
     Check if the destination register is the PC.  */
  if (cris_get_operand2 (inst) == REG_PC)
    {
      /* Must be done here, get_data_from_address may change the size 
         field.  */
      size = cris_get_size (inst);
      operand2 = inst_env->reg[REG_PC];

      /* Get the value of the third operand, i.e. the indirect operand.  */
      operand1 = inst_env->reg[cris_get_operand1 (inst)];
      operand3 = get_data_from_address (&inst, operand1);

      /* Calculate the PC value after the instruction, i.e. where the
         breakpoint should be.  The order of the udw_operands is vital.  */
      add_sub_cmp_and_or_move_action (inst, inst_env, operand2, operand3); 
    }
  /* If this is an autoincrement addressing mode, check if the increment
     changes the PC.  */
  if ((cris_get_operand1 (inst) == REG_PC) && (cris_get_mode (inst) == AUTOINC_MODE))
    {
      /* Get the size field.  */
      size = cris_get_size (inst);

      /* If it's an extend instruction we don't want the signed extend bit,
         because it influences the size.  */
      if (cris_get_opcode (inst) < 4)
        {
          size &= ~SIGNED_EXTEND_BIT_MASK;
        }
      process_autoincrement (size, inst, inst_env);
    } 
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Handles the two-operand addressing mode, all modes except register, for
   the ADD, SUB CMP, AND and OR instruction.  */

static void 
none_reg_mode_add_sub_cmp_and_or_move_op (unsigned short inst, 
                                          inst_env_type *inst_env)
{
  if (inst_env->prefix_found)
    {
      if (cris_get_mode (inst) == PREFIX_INDEX_MODE)
        {
          handle_prefix_index_mode_for_aritm_op (inst, inst_env);
        }
      else if (cris_get_mode (inst) == PREFIX_ASSIGN_MODE)
        {
          handle_prefix_assign_mode_for_aritm_op (inst, inst_env);
        }
      else
        {
          /* The mode is invalid for a prefixed base instruction.  */
          inst_env->invalid = 1;
          return;
        }
    }
  else
    {
      handle_inc_and_index_mode_for_aritm_op (inst, inst_env);
    }
}

/* Handles the quick addressing mode for the ADD and SUB instruction.  */

static void 
quick_mode_add_sub_op (unsigned short inst, inst_env_type *inst_env)
{
  unsigned long operand1;
  unsigned long operand2;

  /* It's a bad idea to be in a prefix instruction now.  This is a quick mode
     instruction and can't have a prefix.  */
  if (inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }

  /* Check if the instruction has PC as its target.  */
  if (cris_get_operand2 (inst) == REG_PC)
    {
      if (inst_env->slot_needed)
        {
          inst_env->invalid = 1;
          return;
        }
      operand1 = cris_get_quick_value (inst);
      operand2 = inst_env->reg[REG_PC];

      /* The size should now be dword.  */
      cris_set_size_to_dword (&inst);

      /* Calculate the PC value after the instruction, i.e. where the
         breakpoint should be.  */
      add_sub_cmp_and_or_move_action (inst, inst_env, operand2, operand1);
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Handles the quick addressing mode for the CMP, AND and OR instruction.  */

static void 
quick_mode_and_cmp_move_or_op (unsigned short inst, inst_env_type *inst_env)
{
  unsigned long operand1;
  unsigned long operand2;

  /* It's a bad idea to be in a prefix instruction now.  This is a quick mode
     instruction and can't have a prefix.  */
  if (inst_env->prefix_found)
    {
      inst_env->invalid = 1;
      return;
    }
  /* Check if the instruction has PC as its target.  */
  if (cris_get_operand2 (inst) == REG_PC)
    {
      if (inst_env->slot_needed)
        {
          inst_env->invalid = 1;
          return;
        }
      /* The instruction has the PC as its target register.  */
      operand1 = cris_get_quick_value (inst);
      operand2 = inst_env->reg[REG_PC];

      /* The quick value is signed, so check if we must do a signed extend.  */
      if (operand1 & SIGNED_QUICK_VALUE_MASK)
        {
          /* sign extend  */
          operand1 |= SIGNED_QUICK_VALUE_EXTEND_MASK;
        }
      /* The size should now be dword.  */
      cris_set_size_to_dword (&inst);

      /* Calculate the PC value after the instruction, i.e. where the
         breakpoint should be.  */
      add_sub_cmp_and_or_move_action (inst, inst_env, operand2, operand1);
    }
  inst_env->slot_needed = 0;
  inst_env->prefix_found = 0;
  inst_env->xflag_found = 0;
  inst_env->disable_interrupt = 0;
}

/* Translate op_type to a function and call it.  */

static void
cris_gdb_func (enum cris_op_type op_type, unsigned short inst, 
	       inst_env_type *inst_env)
{
  switch (op_type)
    {
    case cris_not_implemented_op:
      not_implemented_op (inst, inst_env);
      break;

    case cris_abs_op:
      abs_op (inst, inst_env);
      break;

    case cris_addi_op:
      addi_op (inst, inst_env);
      break;

    case cris_asr_op:
      asr_op (inst, inst_env);
      break;

    case cris_asrq_op:
      asrq_op (inst, inst_env);
      break;

    case cris_ax_ei_setf_op:
      ax_ei_setf_op (inst, inst_env);
      break;

    case cris_bdap_prefix:
      bdap_prefix (inst, inst_env);
      break;

    case cris_biap_prefix:
      biap_prefix (inst, inst_env);
      break;

    case cris_break_op:
      break_op (inst, inst_env);
      break;

    case cris_btst_nop_op:
      btst_nop_op (inst, inst_env);
      break;

    case cris_clearf_di_op:
      clearf_di_op (inst, inst_env);
      break;

    case cris_dip_prefix:
      dip_prefix (inst, inst_env);
      break;

    case cris_dstep_logshift_mstep_neg_not_op:
      dstep_logshift_mstep_neg_not_op (inst, inst_env);
      break;

    case cris_eight_bit_offset_branch_op:
      eight_bit_offset_branch_op (inst, inst_env);
      break;

    case cris_move_mem_to_reg_movem_op:
      move_mem_to_reg_movem_op (inst, inst_env);
      break;

    case cris_move_reg_to_mem_movem_op:
      move_reg_to_mem_movem_op (inst, inst_env);
      break;

    case cris_move_to_preg_op:
      move_to_preg_op (inst, inst_env);
      break;

    case cris_muls_op:
      muls_op (inst, inst_env);
      break;

    case cris_mulu_op:
      mulu_op (inst, inst_env);
      break;

    case cris_none_reg_mode_add_sub_cmp_and_or_move_op:
      none_reg_mode_add_sub_cmp_and_or_move_op (inst, inst_env);
      break;

    case cris_none_reg_mode_clear_test_op:
      none_reg_mode_clear_test_op (inst, inst_env);
      break;

    case cris_none_reg_mode_jump_op:
      none_reg_mode_jump_op (inst, inst_env);
      break;

    case cris_none_reg_mode_move_from_preg_op:
      none_reg_mode_move_from_preg_op (inst, inst_env);
      break;

    case cris_quick_mode_add_sub_op:
      quick_mode_add_sub_op (inst, inst_env);
      break;

    case cris_quick_mode_and_cmp_move_or_op:
      quick_mode_and_cmp_move_or_op (inst, inst_env);
      break;

    case cris_quick_mode_bdap_prefix:
      quick_mode_bdap_prefix (inst, inst_env);
      break;

    case cris_reg_mode_add_sub_cmp_and_or_move_op:
      reg_mode_add_sub_cmp_and_or_move_op (inst, inst_env);
      break;

    case cris_reg_mode_clear_op:
      reg_mode_clear_op (inst, inst_env);
      break;

    case cris_reg_mode_jump_op:
      reg_mode_jump_op (inst, inst_env);
      break;

    case cris_reg_mode_move_from_preg_op:
      reg_mode_move_from_preg_op (inst, inst_env);
      break;

    case cris_reg_mode_test_op:
      reg_mode_test_op (inst, inst_env);
      break;

    case cris_scc_op:
      scc_op (inst, inst_env);
      break;

    case cris_sixteen_bit_offset_branch_op:
      sixteen_bit_offset_branch_op (inst, inst_env);
      break;

    case cris_three_operand_add_sub_cmp_and_or_op:
      three_operand_add_sub_cmp_and_or_op (inst, inst_env);
      break;

    case cris_three_operand_bound_op:
      three_operand_bound_op (inst, inst_env);
      break;

    case cris_two_operand_bound_op:
      two_operand_bound_op (inst, inst_env);
      break;

    case cris_xor_op:
      xor_op (inst, inst_env);
      break;
    }
}

/* This wrapper is to avoid cris_get_assembler being called before 
   exec_bfd has been set.  */

static int
cris_delayed_get_disassembler (bfd_vma addr, struct disassemble_info *info)
{
  int (*print_insn) (bfd_vma addr, struct disassemble_info *info);
  /* FIXME: cagney/2003-08-27: It should be possible to select a CRIS
     disassembler, even when there is no BFD.  Does something like
     "gdb; target remote; disassmeble *0x123" work?  */
  gdb_assert (exec_bfd != NULL);
  print_insn = cris_get_disassembler (exec_bfd);
  gdb_assert (print_insn != NULL);
  return print_insn (addr, info);
}

/* Copied from <asm/elf.h>.  */
typedef unsigned long elf_greg_t;

/* Same as user_regs_struct struct in <asm/user.h>.  */
#define CRISV10_ELF_NGREG 35
typedef elf_greg_t elf_gregset_t[CRISV10_ELF_NGREG];

#define CRISV32_ELF_NGREG 32
typedef elf_greg_t crisv32_elf_gregset_t[CRISV32_ELF_NGREG];

/* Unpack an elf_gregset_t into GDB's register cache.  */

static void 
cris_supply_gregset (struct regcache *regcache, elf_gregset_t *gregsetp)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int i;
  elf_greg_t *regp = *gregsetp;
  static char zerobuf[4] = {0};

  /* The kernel dumps all 32 registers as unsigned longs, but supply_register
     knows about the actual size of each register so that's no problem.  */
  for (i = 0; i < NUM_GENREGS + NUM_SPECREGS; i++)
    {
      regcache_raw_supply (regcache, i, (char *)&regp[i]);
    }

  if (tdep->cris_version == 32)
    {
      /* Needed to set pseudo-register PC for CRISv32.  */
      /* FIXME: If ERP is in a delay slot at this point then the PC will
	 be wrong.  Issue a warning to alert the user.  */
      regcache_raw_supply (regcache, gdbarch_pc_regnum (current_gdbarch),
			   (char *)&regp[ERP_REGNUM]);

      if (*(char *)&regp[ERP_REGNUM] & 0x1)
	fprintf_unfiltered (gdb_stderr, "Warning: PC in delay slot\n");
    }
}

/*  Use a local version of this function to get the correct types for
    regsets, until multi-arch core support is ready.  */

static void
fetch_core_registers (struct regcache *regcache,
		      char *core_reg_sect, unsigned core_reg_size,
                      int which, CORE_ADDR reg_addr)
{
  elf_gregset_t gregset;

  switch (which)
    {
    case 0:
      if (core_reg_size != sizeof (elf_gregset_t) 
	  && core_reg_size != sizeof (crisv32_elf_gregset_t))
        {
          warning (_("wrong size gregset struct in core file"));
        }
      else
        {
          memcpy (&gregset, core_reg_sect, sizeof (gregset));
          cris_supply_gregset (regcache, &gregset);
        }

    default:
      /* We've covered all the kinds of registers we know about here,
         so this must be something we wouldn't know what to do with
         anyway.  Just ignore it.  */
      break;
    }
}

static struct core_fns cris_elf_core_fns =
{
  bfd_target_elf_flavour,               /* core_flavour */
  default_check_format,                 /* check_format */
  default_core_sniffer,                 /* core_sniffer */
  fetch_core_registers,                 /* core_read_registers */
  NULL                                  /* next */
};

extern initialize_file_ftype _initialize_cris_tdep; /* -Wmissing-prototypes */

void
_initialize_cris_tdep (void)
{
  static struct cmd_list_element *cris_set_cmdlist;
  static struct cmd_list_element *cris_show_cmdlist;

  struct cmd_list_element *c;

  gdbarch_register (bfd_arch_cris, cris_gdbarch_init, cris_dump_tdep);
  
  /* CRIS-specific user-commands.  */
  add_setshow_uinteger_cmd ("cris-version", class_support, 
			    &usr_cmd_cris_version, 
			    _("Set the current CRIS version."),
			    _("Show the current CRIS version."),
			    _("\
Set to 10 for CRISv10 or 32 for CRISv32 if autodetection fails.\n\
Defaults to 10. "),
			    set_cris_version,
			    NULL, /* FIXME: i18n: Current CRIS version is %s.  */
			    &setlist, &showlist);

  add_setshow_enum_cmd ("cris-mode", class_support, 
			cris_modes, &usr_cmd_cris_mode, 
			_("Set the current CRIS mode."),
			_("Show the current CRIS mode."),
			_("\
Set to CRIS_MODE_GURU when debugging in guru mode.\n\
Makes GDB use the NRP register instead of the ERP register in certain cases."),
			set_cris_mode,
			NULL, /* FIXME: i18n: Current CRIS version is %s.  */
			&setlist, &showlist);
  
  add_setshow_boolean_cmd ("cris-dwarf2-cfi", class_support,
			   &usr_cmd_cris_dwarf2_cfi,
			   _("Set the usage of Dwarf-2 CFI for CRIS."),
			   _("Show the usage of Dwarf-2 CFI for CRIS."),
			   _("Set this to \"off\" if using gcc-cris < R59."),
			   set_cris_dwarf2_cfi,
			   NULL, /* FIXME: i18n: Usage of Dwarf-2 CFI for CRIS is %d.  */
			   &setlist, &showlist);

  deprecated_add_core_fns (&cris_elf_core_fns);
}

/* Prints out all target specific values.  */

static void
cris_dump_tdep (struct gdbarch *gdbarch, struct ui_file *file)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  if (tdep != NULL)
    {
      fprintf_unfiltered (file, "cris_dump_tdep: tdep->cris_version = %i\n",
                          tdep->cris_version);
      fprintf_unfiltered (file, "cris_dump_tdep: tdep->cris_mode = %s\n",
                          tdep->cris_mode);
      fprintf_unfiltered (file, "cris_dump_tdep: tdep->cris_dwarf2_cfi = %i\n",
                          tdep->cris_dwarf2_cfi);
    }
}

static void
set_cris_version (char *ignore_args, int from_tty, 
		  struct cmd_list_element *c)
{
  struct gdbarch_info info;

  usr_cmd_cris_version_valid = 1;
  
  /* Update the current architecture, if needed.  */
  gdbarch_info_init (&info);
  if (!gdbarch_update_p (info))
    internal_error (__FILE__, __LINE__, 
		    _("cris_gdbarch_update: failed to update architecture."));
}

static void
set_cris_mode (char *ignore_args, int from_tty, 
	       struct cmd_list_element *c)
{
  struct gdbarch_info info;

  /* Update the current architecture, if needed.  */
  gdbarch_info_init (&info);
  if (!gdbarch_update_p (info))
    internal_error (__FILE__, __LINE__, 
		    "cris_gdbarch_update: failed to update architecture.");
}

static void
set_cris_dwarf2_cfi (char *ignore_args, int from_tty, 
		     struct cmd_list_element *c)
{
  struct gdbarch_info info;

  /* Update the current architecture, if needed.  */
  gdbarch_info_init (&info);
  if (!gdbarch_update_p (info))
    internal_error (__FILE__, __LINE__, 
		    _("cris_gdbarch_update: failed to update architecture."));
}

static struct gdbarch *
cris_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  struct gdbarch_tdep *tdep;
  int cris_version;

  if (usr_cmd_cris_version_valid)
    {
      /* Trust the user's CRIS version setting.  */ 
      cris_version = usr_cmd_cris_version;
    }
  else if (info.abfd && bfd_get_mach (info.abfd) == bfd_mach_cris_v32)
    {
      cris_version = 32;
    }
  else
    {
      /* Assume it's CRIS version 10.  */
      cris_version = 10;
    }

  /* Make the current settings visible to the user.  */
  usr_cmd_cris_version = cris_version;
  
  /* Find a candidate among the list of pre-declared architectures.  */
  for (arches = gdbarch_list_lookup_by_info (arches, &info); 
       arches != NULL;
       arches = gdbarch_list_lookup_by_info (arches->next, &info))
    {
      if ((gdbarch_tdep (arches->gdbarch)->cris_version 
	   == usr_cmd_cris_version)
	  && (gdbarch_tdep (arches->gdbarch)->cris_mode 
	   == usr_cmd_cris_mode)
	  && (gdbarch_tdep (arches->gdbarch)->cris_dwarf2_cfi 
	      == usr_cmd_cris_dwarf2_cfi))
        return arches->gdbarch;
    }

  /* No matching architecture was found.  Create a new one.  */
  tdep = (struct gdbarch_tdep *) xmalloc (sizeof (struct gdbarch_tdep));
  gdbarch = gdbarch_alloc (&info, tdep);

  tdep->cris_version = usr_cmd_cris_version;
  tdep->cris_mode = usr_cmd_cris_mode;
  tdep->cris_dwarf2_cfi = usr_cmd_cris_dwarf2_cfi;

  /* INIT shall ensure that the INFO.BYTE_ORDER is non-zero.  */
  switch (info.byte_order)
    {
    case BFD_ENDIAN_LITTLE:
      /* Ok.  */
      break;

    case BFD_ENDIAN_BIG:
      internal_error (__FILE__, __LINE__, _("cris_gdbarch_init: big endian byte order in info"));
      break;
    
    default:
      internal_error (__FILE__, __LINE__, _("cris_gdbarch_init: unknown byte order in info"));
    }

  set_gdbarch_return_value (gdbarch, cris_return_value);
  set_gdbarch_deprecated_reg_struct_has_addr (gdbarch, 
					      cris_reg_struct_has_addr);
  set_gdbarch_deprecated_use_struct_convention (gdbarch, always_use_struct_convention);

  set_gdbarch_sp_regnum (gdbarch, 14);
  
  /* Length of ordinary registers used in push_word and a few other
     places.  register_size() is the real way to know how big a
     register is.  */

  set_gdbarch_double_bit (gdbarch, 64);
  /* The default definition of a long double is 2 * gdbarch_double_bit,
     which means we have to set this explicitly.  */
  set_gdbarch_long_double_bit (gdbarch, 64);

  /* The total amount of space needed to store (in an array called registers)
     GDB's copy of the machine's register state.  Note: We can not use
     cris_register_size at this point, since it relies on current_gdbarch
     being set.  */
  switch (tdep->cris_version)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 8:
    case 9:
      /* Old versions; not supported.  */
      internal_error (__FILE__, __LINE__, 
		      _("cris_gdbarch_init: unsupported CRIS version"));
      break;

    case 10:
    case 11: 
      /* CRIS v10 and v11, a.k.a. ETRAX 100LX.  In addition to ETRAX 100, 
         P7 (32 bits), and P15 (32 bits) have been implemented.  */
      set_gdbarch_pc_regnum (gdbarch, 15);
      set_gdbarch_register_type (gdbarch, cris_register_type);
      /* There are 32 registers (some of which may not be implemented).  */
      set_gdbarch_num_regs (gdbarch, 32);
      set_gdbarch_register_name (gdbarch, cris_register_name);
      set_gdbarch_cannot_store_register (gdbarch, cris_cannot_store_register);
      set_gdbarch_cannot_fetch_register (gdbarch, cris_cannot_fetch_register);

      set_gdbarch_software_single_step (gdbarch, cris_software_single_step);
      break;

    case 32:
      /* CRIS v32.  General registers R0 - R15 (32 bits), special registers 
	 P0 - P15 (32 bits) except P0, P1, P3 (8 bits) and P4 (16 bits)
	 and pseudo-register PC (32 bits).  */
      set_gdbarch_pc_regnum (gdbarch, 32);
      set_gdbarch_register_type (gdbarch, crisv32_register_type);
      /* 32 registers + pseudo-register PC + 16 support registers.  */
      set_gdbarch_num_regs (gdbarch, 32 + 1 + 16);
      set_gdbarch_register_name (gdbarch, crisv32_register_name);

      set_gdbarch_cannot_store_register 
	(gdbarch, crisv32_cannot_store_register);
      set_gdbarch_cannot_fetch_register
	(gdbarch, crisv32_cannot_fetch_register);

      set_gdbarch_have_nonsteppable_watchpoint (gdbarch, 1);

      set_gdbarch_single_step_through_delay 
	(gdbarch, crisv32_single_step_through_delay);

      break;

    default:
      internal_error (__FILE__, __LINE__, 
		      _("cris_gdbarch_init: unknown CRIS version"));
    }

  /* Dummy frame functions (shared between CRISv10 and CRISv32 since they
     have the same ABI).  */
  set_gdbarch_push_dummy_code (gdbarch, cris_push_dummy_code);
  set_gdbarch_push_dummy_call (gdbarch, cris_push_dummy_call);
  set_gdbarch_frame_align (gdbarch, cris_frame_align);
  set_gdbarch_skip_prologue (gdbarch, cris_skip_prologue);
  
  /* The stack grows downward.  */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  set_gdbarch_breakpoint_from_pc (gdbarch, cris_breakpoint_from_pc);
  
  set_gdbarch_unwind_pc (gdbarch, cris_unwind_pc);
  set_gdbarch_unwind_sp (gdbarch, cris_unwind_sp);
  set_gdbarch_unwind_dummy_id (gdbarch, cris_unwind_dummy_id);

  if (tdep->cris_dwarf2_cfi == 1)
    {
      /* Hook in the Dwarf-2 frame sniffer.  */
      set_gdbarch_dwarf2_reg_to_regnum (gdbarch, cris_dwarf2_reg_to_regnum);
      dwarf2_frame_set_init_reg (gdbarch, cris_dwarf2_frame_init_reg);
      frame_unwind_append_sniffer (gdbarch, dwarf2_frame_sniffer);
    }

  if (tdep->cris_mode != cris_mode_guru)
    {
      frame_unwind_append_sniffer (gdbarch, cris_sigtramp_frame_sniffer);
    }

  frame_unwind_append_sniffer (gdbarch, cris_frame_sniffer);
  frame_base_set_default (gdbarch, &cris_frame_base);

  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);
  
  /* FIXME: cagney/2003-08-27: It should be possible to select a CRIS
     disassembler, even when there is no BFD.  Does something like
     "gdb; target remote; disassmeble *0x123" work?  */
  set_gdbarch_print_insn (gdbarch, cris_delayed_get_disassembler);

  return gdbarch;
}
