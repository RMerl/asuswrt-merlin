/* GNU/Linux on ARM target support.

   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
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
#include "target.h"
#include "value.h"
#include "gdbtypes.h"
#include "floatformat.h"
#include "gdbcore.h"
#include "frame.h"
#include "regcache.h"
#include "doublest.h"
#include "solib-svr4.h"
#include "osabi.h"
#include "regset.h"
#include "trad-frame.h"
#include "tramp-frame.h"

#include "arm-tdep.h"
#include "arm-linux-tdep.h"
#include "glibc-tdep.h"

#include "gdb_string.h"

extern int arm_apcs_32;

/* Under ARM GNU/Linux the traditional way of performing a breakpoint
   is to execute a particular software interrupt, rather than use a
   particular undefined instruction to provoke a trap.  Upon exection
   of the software interrupt the kernel stops the inferior with a
   SIGTRAP, and wakes the debugger.  */

static const char arm_linux_arm_le_breakpoint[] = { 0x01, 0x00, 0x9f, 0xef };

static const char arm_linux_arm_be_breakpoint[] = { 0xef, 0x9f, 0x00, 0x01 };

/* However, the EABI syscall interface (new in Nov. 2005) does not look at
   the operand of the swi if old-ABI compatibility is disabled.  Therefore,
   use an undefined instruction instead.  This is supported as of kernel
   version 2.5.70 (May 2003), so should be a safe assumption for EABI
   binaries.  */

static const char eabi_linux_arm_le_breakpoint[] = { 0xf0, 0x01, 0xf0, 0xe7 };

static const char eabi_linux_arm_be_breakpoint[] = { 0xe7, 0xf0, 0x01, 0xf0 };

/* All the kernels which support Thumb support using a specific undefined
   instruction for the Thumb breakpoint.  */

static const char arm_linux_thumb_be_breakpoint[] = {0xde, 0x01};

static const char arm_linux_thumb_le_breakpoint[] = {0x01, 0xde};

/* Description of the longjmp buffer.  */
#define ARM_LINUX_JB_ELEMENT_SIZE	INT_REGISTER_SIZE
#define ARM_LINUX_JB_PC			21

/*
   Dynamic Linking on ARM GNU/Linux
   --------------------------------

   Note: PLT = procedure linkage table
   GOT = global offset table

   As much as possible, ELF dynamic linking defers the resolution of
   jump/call addresses until the last minute. The technique used is
   inspired by the i386 ELF design, and is based on the following
   constraints.

   1) The calling technique should not force a change in the assembly
   code produced for apps; it MAY cause changes in the way assembly
   code is produced for position independent code (i.e. shared
   libraries).

   2) The technique must be such that all executable areas must not be
   modified; and any modified areas must not be executed.

   To do this, there are three steps involved in a typical jump:

   1) in the code
   2) through the PLT
   3) using a pointer from the GOT

   When the executable or library is first loaded, each GOT entry is
   initialized to point to the code which implements dynamic name
   resolution and code finding.  This is normally a function in the
   program interpreter (on ARM GNU/Linux this is usually
   ld-linux.so.2, but it does not have to be).  On the first
   invocation, the function is located and the GOT entry is replaced
   with the real function address.  Subsequent calls go through steps
   1, 2 and 3 and end up calling the real code.

   1) In the code: 

   b    function_call
   bl   function_call

   This is typical ARM code using the 26 bit relative branch or branch
   and link instructions.  The target of the instruction
   (function_call is usually the address of the function to be called.
   In position independent code, the target of the instruction is
   actually an entry in the PLT when calling functions in a shared
   library.  Note that this call is identical to a normal function
   call, only the target differs.

   2) In the PLT:

   The PLT is a synthetic area, created by the linker. It exists in
   both executables and libraries. It is an array of stubs, one per
   imported function call. It looks like this:

   PLT[0]:
   str     lr, [sp, #-4]!       @push the return address (lr)
   ldr     lr, [pc, #16]   @load from 6 words ahead
   add     lr, pc, lr      @form an address for GOT[0]
   ldr     pc, [lr, #8]!   @jump to the contents of that addr

   The return address (lr) is pushed on the stack and used for
   calculations.  The load on the second line loads the lr with
   &GOT[3] - . - 20.  The addition on the third leaves:

   lr = (&GOT[3] - . - 20) + (. + 8)
   lr = (&GOT[3] - 12)
   lr = &GOT[0]

   On the fourth line, the pc and lr are both updated, so that:

   pc = GOT[2]
   lr = &GOT[0] + 8
   = &GOT[2]

   NOTE: PLT[0] borrows an offset .word from PLT[1]. This is a little
   "tight", but allows us to keep all the PLT entries the same size.

   PLT[n+1]:
   ldr     ip, [pc, #4]    @load offset from gotoff
   add     ip, pc, ip      @add the offset to the pc
   ldr     pc, [ip]        @jump to that address
   gotoff: .word   GOT[n+3] - .

   The load on the first line, gets an offset from the fourth word of
   the PLT entry.  The add on the second line makes ip = &GOT[n+3],
   which contains either a pointer to PLT[0] (the fixup trampoline) or
   a pointer to the actual code.

   3) In the GOT:

   The GOT contains helper pointers for both code (PLT) fixups and
   data fixups.  The first 3 entries of the GOT are special. The next
   M entries (where M is the number of entries in the PLT) belong to
   the PLT fixups. The next D (all remaining) entries belong to
   various data fixups. The actual size of the GOT is 3 + M + D.

   The GOT is also a synthetic area, created by the linker. It exists
   in both executables and libraries.  When the GOT is first
   initialized , all the GOT entries relating to PLT fixups are
   pointing to code back at PLT[0].

   The special entries in the GOT are:

   GOT[0] = linked list pointer used by the dynamic loader
   GOT[1] = pointer to the reloc table for this module
   GOT[2] = pointer to the fixup/resolver code

   The first invocation of function call comes through and uses the
   fixup/resolver code.  On the entry to the fixup/resolver code:

   ip = &GOT[n+3]
   lr = &GOT[2]
   stack[0] = return address (lr) of the function call
   [r0, r1, r2, r3] are still the arguments to the function call

   This is enough information for the fixup/resolver code to work
   with.  Before the fixup/resolver code returns, it actually calls
   the requested function and repairs &GOT[n+3].  */

/* The constants below were determined by examining the following files
   in the linux kernel sources:

      arch/arm/kernel/signal.c
	  - see SWI_SYS_SIGRETURN and SWI_SYS_RT_SIGRETURN
      include/asm-arm/unistd.h
	  - see __NR_sigreturn, __NR_rt_sigreturn, and __NR_SYSCALL_BASE */

#define ARM_LINUX_SIGRETURN_INSTR	0xef900077
#define ARM_LINUX_RT_SIGRETURN_INSTR	0xef9000ad

/* For ARM EABI, the syscall number is not in the SWI instruction
   (instead it is loaded into r7).  We recognize the pattern that
   glibc uses...  alternatively, we could arrange to do this by
   function name, but they are not always exported.  */
#define ARM_SET_R7_SIGRETURN		0xe3a07077
#define ARM_SET_R7_RT_SIGRETURN		0xe3a070ad
#define ARM_EABI_SYSCALL		0xef000000

static void
arm_linux_sigtramp_cache (struct frame_info *next_frame,
			  struct trad_frame_cache *this_cache,
			  CORE_ADDR func, int regs_offset)
{
  CORE_ADDR sp = frame_unwind_register_unsigned (next_frame, ARM_SP_REGNUM);
  CORE_ADDR base = sp + regs_offset;
  int i;

  for (i = 0; i < 16; i++)
    trad_frame_set_reg_addr (this_cache, i, base + i * 4);

  trad_frame_set_reg_addr (this_cache, ARM_PS_REGNUM, base + 16 * 4);

  /* The VFP or iWMMXt registers may be saved on the stack, but there's
     no reliable way to restore them (yet).  */

  /* Save a frame ID.  */
  trad_frame_set_id (this_cache, frame_id_build (sp, func));
}

/* There are a couple of different possible stack layouts that
   we need to support.

   Before version 2.6.18, the kernel used completely independent
   layouts for non-RT and RT signals.  For non-RT signals the stack
   began directly with a struct sigcontext.  For RT signals the stack
   began with two redundant pointers (to the siginfo and ucontext),
   and then the siginfo and ucontext.

   As of version 2.6.18, the non-RT signal frame layout starts with
   a ucontext and the RT signal frame starts with a siginfo and then
   a ucontext.  Also, the ucontext now has a designated save area
   for coprocessor registers.

   For RT signals, it's easy to tell the difference: we look for
   pinfo, the pointer to the siginfo.  If it has the expected
   value, we have an old layout.  If it doesn't, we have the new
   layout.

   For non-RT signals, it's a bit harder.  We need something in one
   layout or the other with a recognizable offset and value.  We can't
   use the return trampoline, because ARM usually uses SA_RESTORER,
   in which case the stack return trampoline is not filled in.
   We can't use the saved stack pointer, because sigaltstack might
   be in use.  So for now we guess the new layout...  */

/* There are three words (trap_no, error_code, oldmask) in
   struct sigcontext before r0.  */
#define ARM_SIGCONTEXT_R0 0xc

/* There are five words (uc_flags, uc_link, and three for uc_stack)
   in the ucontext_t before the sigcontext.  */
#define ARM_UCONTEXT_SIGCONTEXT 0x14

/* There are three elements in an rt_sigframe before the ucontext:
   pinfo, puc, and info.  The first two are pointers and the third
   is a struct siginfo, with size 128 bytes.  We could follow puc
   to the ucontext, but it's simpler to skip the whole thing.  */
#define ARM_OLD_RT_SIGFRAME_SIGINFO 0x8
#define ARM_OLD_RT_SIGFRAME_UCONTEXT 0x88

#define ARM_NEW_RT_SIGFRAME_UCONTEXT 0x80

#define ARM_NEW_SIGFRAME_MAGIC 0x5ac3c35a

static void
arm_linux_sigreturn_init (const struct tramp_frame *self,
			  struct frame_info *next_frame,
			  struct trad_frame_cache *this_cache,
			  CORE_ADDR func)
{
  CORE_ADDR sp = frame_unwind_register_unsigned (next_frame, ARM_SP_REGNUM);
  ULONGEST uc_flags = read_memory_unsigned_integer (sp, 4);

  if (uc_flags == ARM_NEW_SIGFRAME_MAGIC)
    arm_linux_sigtramp_cache (next_frame, this_cache, func,
			      ARM_UCONTEXT_SIGCONTEXT
			      + ARM_SIGCONTEXT_R0);
  else
    arm_linux_sigtramp_cache (next_frame, this_cache, func,
			      ARM_SIGCONTEXT_R0);
}

static void
arm_linux_rt_sigreturn_init (const struct tramp_frame *self,
			  struct frame_info *next_frame,
			  struct trad_frame_cache *this_cache,
			  CORE_ADDR func)
{
  CORE_ADDR sp = frame_unwind_register_unsigned (next_frame, ARM_SP_REGNUM);
  ULONGEST pinfo = read_memory_unsigned_integer (sp, 4);

  if (pinfo == sp + ARM_OLD_RT_SIGFRAME_SIGINFO)
    arm_linux_sigtramp_cache (next_frame, this_cache, func,
			      ARM_OLD_RT_SIGFRAME_UCONTEXT
			      + ARM_UCONTEXT_SIGCONTEXT
			      + ARM_SIGCONTEXT_R0);
  else
    arm_linux_sigtramp_cache (next_frame, this_cache, func,
			      ARM_NEW_RT_SIGFRAME_UCONTEXT
			      + ARM_UCONTEXT_SIGCONTEXT
			      + ARM_SIGCONTEXT_R0);
}

static struct tramp_frame arm_linux_sigreturn_tramp_frame = {
  SIGTRAMP_FRAME,
  4,
  {
    { ARM_LINUX_SIGRETURN_INSTR, -1 },
    { TRAMP_SENTINEL_INSN }
  },
  arm_linux_sigreturn_init
};

static struct tramp_frame arm_linux_rt_sigreturn_tramp_frame = {
  SIGTRAMP_FRAME,
  4,
  {
    { ARM_LINUX_RT_SIGRETURN_INSTR, -1 },
    { TRAMP_SENTINEL_INSN }
  },
  arm_linux_rt_sigreturn_init
};

static struct tramp_frame arm_eabi_linux_sigreturn_tramp_frame = {
  SIGTRAMP_FRAME,
  4,
  {
    { ARM_SET_R7_SIGRETURN, -1 },
    { ARM_EABI_SYSCALL, -1 },
    { TRAMP_SENTINEL_INSN }
  },
  arm_linux_sigreturn_init
};

static struct tramp_frame arm_eabi_linux_rt_sigreturn_tramp_frame = {
  SIGTRAMP_FRAME,
  4,
  {
    { ARM_SET_R7_RT_SIGRETURN, -1 },
    { ARM_EABI_SYSCALL, -1 },
    { TRAMP_SENTINEL_INSN }
  },
  arm_linux_rt_sigreturn_init
};

/* Core file and register set support.  */

#define ARM_LINUX_SIZEOF_GREGSET (18 * INT_REGISTER_SIZE)

void
arm_linux_supply_gregset (const struct regset *regset,
			  struct regcache *regcache,
			  int regnum, const void *gregs_buf, size_t len)
{
  const gdb_byte *gregs = gregs_buf;
  int regno;
  CORE_ADDR reg_pc;
  gdb_byte pc_buf[INT_REGISTER_SIZE];

  for (regno = ARM_A1_REGNUM; regno < ARM_PC_REGNUM; regno++)
    if (regnum == -1 || regnum == regno)
      regcache_raw_supply (regcache, regno,
			   gregs + INT_REGISTER_SIZE * regno);

  if (regnum == ARM_PS_REGNUM || regnum == -1)
    {
      if (arm_apcs_32)
	regcache_raw_supply (regcache, ARM_PS_REGNUM,
			     gregs + INT_REGISTER_SIZE * ARM_CPSR_REGNUM);
      else
	regcache_raw_supply (regcache, ARM_PS_REGNUM,
			     gregs + INT_REGISTER_SIZE * ARM_PC_REGNUM);
    }

  if (regnum == ARM_PC_REGNUM || regnum == -1)
    {
      reg_pc = extract_unsigned_integer (gregs
					 + INT_REGISTER_SIZE * ARM_PC_REGNUM,
					 INT_REGISTER_SIZE);
      reg_pc = gdbarch_addr_bits_remove (current_gdbarch, reg_pc);
      store_unsigned_integer (pc_buf, INT_REGISTER_SIZE, reg_pc);
      regcache_raw_supply (regcache, ARM_PC_REGNUM, pc_buf);
    }
}

void
arm_linux_collect_gregset (const struct regset *regset,
			   const struct regcache *regcache,
			   int regnum, void *gregs_buf, size_t len)
{
  gdb_byte *gregs = gregs_buf;
  int regno;

  for (regno = ARM_A1_REGNUM; regno < ARM_PC_REGNUM; regno++)
    if (regnum == -1 || regnum == regno)
      regcache_raw_collect (regcache, regno,
			    gregs + INT_REGISTER_SIZE * regno);

  if (regnum == ARM_PS_REGNUM || regnum == -1)
    {
      if (arm_apcs_32)
	regcache_raw_collect (regcache, ARM_PS_REGNUM,
			      gregs + INT_REGISTER_SIZE * ARM_CPSR_REGNUM);
      else
	regcache_raw_collect (regcache, ARM_PS_REGNUM,
			      gregs + INT_REGISTER_SIZE * ARM_PC_REGNUM);
    }

  if (regnum == ARM_PC_REGNUM || regnum == -1)
    regcache_raw_collect (regcache, ARM_PC_REGNUM,
			  gregs + INT_REGISTER_SIZE * ARM_PC_REGNUM);
}

/* Support for register format used by the NWFPE FPA emulator.  */

#define typeNone		0x00
#define typeSingle		0x01
#define typeDouble		0x02
#define typeExtended		0x03

void
supply_nwfpe_register (struct regcache *regcache, int regno,
		       const gdb_byte *regs)
{
  const gdb_byte *reg_data;
  gdb_byte reg_tag;
  gdb_byte buf[FP_REGISTER_SIZE];

  reg_data = regs + (regno - ARM_F0_REGNUM) * FP_REGISTER_SIZE;
  reg_tag = regs[(regno - ARM_F0_REGNUM) + NWFPE_TAGS_OFFSET];
  memset (buf, 0, FP_REGISTER_SIZE);

  switch (reg_tag)
    {
    case typeSingle:
      memcpy (buf, reg_data, 4);
      break;
    case typeDouble:
      memcpy (buf, reg_data + 4, 4);
      memcpy (buf + 4, reg_data, 4);
      break;
    case typeExtended:
      /* We want sign and exponent, then least significant bits,
	 then most significant.  NWFPE does sign, most, least.  */
      memcpy (buf, reg_data, 4);
      memcpy (buf + 4, reg_data + 8, 4);
      memcpy (buf + 8, reg_data + 4, 4);
      break;
    default:
      break;
    }

  regcache_raw_supply (regcache, regno, buf);
}

void
collect_nwfpe_register (const struct regcache *regcache, int regno,
			gdb_byte *regs)
{
  gdb_byte *reg_data;
  gdb_byte reg_tag;
  gdb_byte buf[FP_REGISTER_SIZE];

  regcache_raw_collect (regcache, regno, buf);

  /* NOTE drow/2006-06-07: This code uses the tag already in the
     register buffer.  I've preserved that when moving the code
     from the native file to the target file.  But this doesn't
     always make sense.  */

  reg_data = regs + (regno - ARM_F0_REGNUM) * FP_REGISTER_SIZE;
  reg_tag = regs[(regno - ARM_F0_REGNUM) + NWFPE_TAGS_OFFSET];

  switch (reg_tag)
    {
    case typeSingle:
      memcpy (reg_data, buf, 4);
      break;
    case typeDouble:
      memcpy (reg_data, buf + 4, 4);
      memcpy (reg_data + 4, buf, 4);
      break;
    case typeExtended:
      memcpy (reg_data, buf, 4);
      memcpy (reg_data + 4, buf + 8, 4);
      memcpy (reg_data + 8, buf + 4, 4);
      break;
    default:
      break;
    }
}

void
arm_linux_supply_nwfpe (const struct regset *regset,
			struct regcache *regcache,
			int regnum, const void *regs_buf, size_t len)
{
  const gdb_byte *regs = regs_buf;
  int regno;

  if (regnum == ARM_FPS_REGNUM || regnum == -1)
    regcache_raw_supply (regcache, ARM_FPS_REGNUM,
			 regs + NWFPE_FPSR_OFFSET);

  for (regno = ARM_F0_REGNUM; regno <= ARM_F7_REGNUM; regno++)
    if (regnum == -1 || regnum == regno)
      supply_nwfpe_register (regcache, regno, regs);
}

void
arm_linux_collect_nwfpe (const struct regset *regset,
			 const struct regcache *regcache,
			 int regnum, void *regs_buf, size_t len)
{
  gdb_byte *regs = regs_buf;
  int regno;

  for (regno = ARM_F0_REGNUM; regno <= ARM_F7_REGNUM; regno++)
    if (regnum == -1 || regnum == regno)
      collect_nwfpe_register (regcache, regno, regs);

  if (regnum == ARM_FPS_REGNUM || regnum == -1)
    regcache_raw_collect (regcache, ARM_FPS_REGNUM,
			  regs + INT_REGISTER_SIZE * ARM_FPS_REGNUM);
}

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

static const struct regset *
arm_linux_regset_from_core_section (struct gdbarch *gdbarch,
				    const char *sect_name, size_t sect_size)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  if (strcmp (sect_name, ".reg") == 0
      && sect_size == ARM_LINUX_SIZEOF_GREGSET)
    {
      if (tdep->gregset == NULL)
        tdep->gregset = regset_alloc (gdbarch, arm_linux_supply_gregset,
                                      arm_linux_collect_gregset);
      return tdep->gregset;
    }

  if (strcmp (sect_name, ".reg2") == 0
      && sect_size == ARM_LINUX_SIZEOF_NWFPE)
    {
      if (tdep->fpregset == NULL)
        tdep->fpregset = regset_alloc (gdbarch, arm_linux_supply_nwfpe,
                                       arm_linux_collect_nwfpe);
      return tdep->fpregset;
    }

  return NULL;
}

static void
arm_linux_init_abi (struct gdbarch_info info,
		    struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->lowest_pc = 0x8000;
  if (info.byte_order == BFD_ENDIAN_BIG)
    {
      if (tdep->arm_abi == ARM_ABI_AAPCS)
	tdep->arm_breakpoint = eabi_linux_arm_be_breakpoint;
      else
	tdep->arm_breakpoint = arm_linux_arm_be_breakpoint;
      tdep->thumb_breakpoint = arm_linux_thumb_be_breakpoint;
    }
  else
    {
      if (tdep->arm_abi == ARM_ABI_AAPCS)
	tdep->arm_breakpoint = eabi_linux_arm_le_breakpoint;
      else
	tdep->arm_breakpoint = arm_linux_arm_le_breakpoint;
      tdep->thumb_breakpoint = arm_linux_thumb_le_breakpoint;
    }
  tdep->arm_breakpoint_size = sizeof (arm_linux_arm_le_breakpoint);
  tdep->thumb_breakpoint_size = sizeof (arm_linux_thumb_le_breakpoint);

  if (tdep->fp_model == ARM_FLOAT_AUTO)
    tdep->fp_model = ARM_FLOAT_FPA;

  tdep->jb_pc = ARM_LINUX_JB_PC;
  tdep->jb_elt_size = ARM_LINUX_JB_ELEMENT_SIZE;

  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);

  /* Single stepping.  */
  set_gdbarch_software_single_step (gdbarch, arm_software_single_step);

  /* Shared library handling.  */
  set_gdbarch_skip_trampoline_code (gdbarch, find_solib_trampoline_target);
  set_gdbarch_skip_solib_resolver (gdbarch, glibc_skip_solib_resolver);

  /* Enable TLS support.  */
  set_gdbarch_fetch_tls_load_module_address (gdbarch,
                                             svr4_fetch_objfile_link_map);

  tramp_frame_prepend_unwinder (gdbarch,
				&arm_linux_sigreturn_tramp_frame);
  tramp_frame_prepend_unwinder (gdbarch,
				&arm_linux_rt_sigreturn_tramp_frame);
  tramp_frame_prepend_unwinder (gdbarch,
				&arm_eabi_linux_sigreturn_tramp_frame);
  tramp_frame_prepend_unwinder (gdbarch,
				&arm_eabi_linux_rt_sigreturn_tramp_frame);

  /* Core file support.  */
  set_gdbarch_regset_from_core_section (gdbarch,
					arm_linux_regset_from_core_section);
}

void
_initialize_arm_linux_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_arm, 0, GDB_OSABI_LINUX,
			  arm_linux_init_abi);
}
