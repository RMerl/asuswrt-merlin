/* Motorola m68k target-dependent support for GNU/Linux.

   Copyright (C) 1996, 1998, 2000, 2001, 2002, 2003, 2004, 2007
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
#include "gdbcore.h"
#include "doublest.h"
#include "floatformat.h"
#include "frame.h"
#include "target.h"
#include "gdb_string.h"
#include "gdbtypes.h"
#include "osabi.h"
#include "regcache.h"
#include "objfiles.h"
#include "symtab.h"
#include "m68k-tdep.h"
#include "trad-frame.h"
#include "frame-unwind.h"
#include "glibc-tdep.h"
#include "solib-svr4.h"
#include "auxv.h"
#include "observer.h"
#include "elf/common.h"

/* Offsets (in target ints) into jmp_buf.  */

#define M68K_LINUX_JB_ELEMENT_SIZE 4
#define M68K_LINUX_JB_PC 7

/* Check whether insn1 and insn2 are parts of a signal trampoline.  */

#define IS_SIGTRAMP(insn1, insn2)					\
  (/* addaw #20,sp; moveq #119,d0; trap #0 */				\
   (insn1 == 0xdefc0014 && insn2 == 0x70774e40)				\
   /* moveq #119,d0; trap #0 */						\
   || insn1 == 0x70774e40)

#define IS_RT_SIGTRAMP(insn1, insn2)					\
  (/* movel #173,d0; trap #0 */						\
   (insn1 == 0x203c0000 && insn2 == 0x00ad4e40)				\
   /* moveq #82,d0; notb d0; trap #0 */					\
   || (insn1 == 0x70524600 && (insn2 >> 16) == 0x4e40))

/* Return non-zero if PC points into the signal trampoline.  For the
   sake of m68k_linux_get_sigtramp_info we also distinguish between
   non-RT and RT signal trampolines.  */

static int
m68k_linux_pc_in_sigtramp (CORE_ADDR pc, char *name)
{
  CORE_ADDR sp;
  char buf[12];
  unsigned long insn0, insn1, insn2;

  if (read_memory_nobpt (pc - 4, buf, sizeof (buf)))
    return 0;
  insn1 = extract_unsigned_integer (buf + 4, 4);
  insn2 = extract_unsigned_integer (buf + 8, 4);
  if (IS_SIGTRAMP (insn1, insn2))
    return 1;
  if (IS_RT_SIGTRAMP (insn1, insn2))
    return 2;

  insn0 = extract_unsigned_integer (buf, 4);
  if (IS_SIGTRAMP (insn0, insn1))
    return 1;
  if (IS_RT_SIGTRAMP (insn0, insn1))
    return 2;

  insn0 = ((insn0 << 16) & 0xffffffff) | (insn1 >> 16);
  insn1 = ((insn1 << 16) & 0xffffffff) | (insn2 >> 16);
  if (IS_SIGTRAMP (insn0, insn1))
    return 1;
  if (IS_RT_SIGTRAMP (insn0, insn1))
    return 2;

  return 0;
}

/* From <asm/sigcontext.h>.  */
static int m68k_linux_sigcontext_reg_offset[M68K_NUM_REGS] =
{
  2 * 4,			/* %d0 */
  3 * 4,			/* %d1 */
  -1,				/* %d2 */
  -1,				/* %d3 */
  -1,				/* %d4 */
  -1,				/* %d5 */
  -1,				/* %d6 */
  -1,				/* %d7 */
  4 * 4,			/* %a0 */
  5 * 4,			/* %a1 */
  -1,				/* %a2 */
  -1,				/* %a3 */
  -1,				/* %a4 */
  -1,				/* %a5 */
  -1,				/* %fp */
  1 * 4,			/* %sp */
  6 * 4,			/* %sr */
  6 * 4 + 2,			/* %pc */
  8 * 4,			/* %fp0 */
  11 * 4,			/* %fp1 */
  -1,				/* %fp2 */
  -1,				/* %fp3 */
  -1,				/* %fp4 */
  -1,				/* %fp5 */
  -1,				/* %fp6 */
  -1,				/* %fp7 */
  14 * 4,			/* %fpcr */
  15 * 4,			/* %fpsr */
  16 * 4			/* %fpiaddr */
};

static int m68k_uclinux_sigcontext_reg_offset[M68K_NUM_REGS] =
{
  2 * 4,			/* %d0 */
  3 * 4,			/* %d1 */
  -1,				/* %d2 */
  -1,				/* %d3 */
  -1,				/* %d4 */
  -1,				/* %d5 */
  -1,				/* %d6 */
  -1,				/* %d7 */
  4 * 4,			/* %a0 */
  5 * 4,			/* %a1 */
  -1,				/* %a2 */
  -1,				/* %a3 */
  -1,				/* %a4 */
  6 * 4,			/* %a5 */
  -1,				/* %fp */
  1 * 4,			/* %sp */
  7 * 4,			/* %sr */
  7 * 4 + 2,			/* %pc */
  -1,				/* %fp0 */
  -1,				/* %fp1 */
  -1,				/* %fp2 */
  -1,				/* %fp3 */
  -1,				/* %fp4 */
  -1,				/* %fp5 */
  -1,				/* %fp6 */
  -1,				/* %fp7 */
  -1,				/* %fpcr */
  -1,				/* %fpsr */
  -1				/* %fpiaddr */
};

/* From <asm/ucontext.h>.  */
static int m68k_linux_ucontext_reg_offset[M68K_NUM_REGS] =
{
  6 * 4,			/* %d0 */
  7 * 4,			/* %d1 */
  8 * 4,			/* %d2 */
  9 * 4,			/* %d3 */
  10 * 4,			/* %d4 */
  11 * 4,			/* %d5 */
  12 * 4,			/* %d6 */
  13 * 4,			/* %d7 */
  14 * 4,			/* %a0 */
  15 * 4,			/* %a1 */
  16 * 4,			/* %a2 */
  17 * 4,			/* %a3 */
  18 * 4,			/* %a4 */
  19 * 4,			/* %a5 */
  20 * 4,			/* %fp */
  21 * 4,			/* %sp */
  23 * 4,			/* %sr */
  22 * 4,			/* %pc */
  27 * 4,			/* %fp0 */
  30 * 4,			/* %fp1 */
  33 * 4,			/* %fp2 */
  36 * 4,			/* %fp3 */
  39 * 4,			/* %fp4 */
  42 * 4,			/* %fp5 */
  45 * 4,			/* %fp6 */
  48 * 4,			/* %fp7 */
  24 * 4,			/* %fpcr */
  25 * 4,			/* %fpsr */
  26 * 4			/* %fpiaddr */
};


/* Get info about saved registers in sigtramp.  */

struct m68k_linux_sigtramp_info
{
  /* Address of sigcontext.  */
  CORE_ADDR sigcontext_addr;

  /* Offset of registers in `struct sigcontext'.  */
  int *sc_reg_offset;
};

/* Nonzero if running on uClinux.  */
static int target_is_uclinux;

static void
m68k_linux_inferior_created (struct target_ops *objfile, int from_tty)
{
  /* Record that we will need to re-evaluate whether we are running on
     a uClinux or normal Linux target (see m68k_linux_get_sigtramp_info).  */
  target_is_uclinux = -1;
}

static struct m68k_linux_sigtramp_info
m68k_linux_get_sigtramp_info (struct frame_info *next_frame)
{
  CORE_ADDR sp;
  char buf[4];
  struct m68k_linux_sigtramp_info info;

  if (target_is_uclinux == -1)
    {
      /* Determine whether we are running on a uClinux or normal Linux
         target so we can use the correct sigcontext layouts.  */
    
      CORE_ADDR dummy;
    
      target_is_uclinux
        = target_auxv_search (&current_target, AT_NULL, &dummy) > 0
          && target_auxv_search (&current_target, AT_PAGESZ, &dummy) == 0;
    }

  frame_unwind_register (next_frame, M68K_SP_REGNUM, buf);
  sp = extract_unsigned_integer (buf, 4);

  /* Get sigcontext address, it is the third parameter on the stack.  */
  info.sigcontext_addr = read_memory_unsigned_integer (sp + 8, 4);

  if (m68k_linux_pc_in_sigtramp (frame_pc_unwind (next_frame), 0) == 2)
    info.sc_reg_offset = m68k_linux_ucontext_reg_offset;
  else
    info.sc_reg_offset
      = target_is_uclinux ? m68k_uclinux_sigcontext_reg_offset
			  : m68k_linux_sigcontext_reg_offset;
  return info;
}

/* Signal trampolines.  */

static struct trad_frame_cache *
m68k_linux_sigtramp_frame_cache (struct frame_info *next_frame,
				 void **this_cache)
{
  struct frame_id this_id;
  struct trad_frame_cache *cache;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  struct m68k_linux_sigtramp_info info;
  char buf[4];
  int i;

  if (*this_cache)
    return *this_cache;

  cache = trad_frame_cache_zalloc (next_frame);

  /* FIXME: cagney/2004-05-01: This is is long standing broken code.
     The frame ID's code address should be the start-address of the
     signal trampoline and not the current PC within that
     trampoline.  */
  frame_unwind_register (next_frame, M68K_SP_REGNUM, buf);
  /* See the end of m68k_push_dummy_call.  */
  this_id = frame_id_build (extract_unsigned_integer (buf, 4) - 4 + 8,
			    frame_pc_unwind (next_frame));
  trad_frame_set_id (cache, this_id);

  info = m68k_linux_get_sigtramp_info (next_frame);

  for (i = 0; i < M68K_NUM_REGS; i++)
    if (info.sc_reg_offset[i] != -1)
      trad_frame_set_reg_addr (cache, i,
			       info.sigcontext_addr + info.sc_reg_offset[i]);

  *this_cache = cache;
  return cache;
}

static void
m68k_linux_sigtramp_frame_this_id (struct frame_info *next_frame,
				   void **this_cache,
				   struct frame_id *this_id)
{
  struct trad_frame_cache *cache =
    m68k_linux_sigtramp_frame_cache (next_frame, this_cache);
  trad_frame_get_id (cache, this_id);
}

static void
m68k_linux_sigtramp_frame_prev_register (struct frame_info *next_frame,
					 void **this_cache,
					 int regnum, int *optimizedp,
					 enum lval_type *lvalp,
					 CORE_ADDR *addrp,
					 int *realnump, gdb_byte *valuep)
{
  /* Make sure we've initialized the cache.  */
  struct trad_frame_cache *cache =
    m68k_linux_sigtramp_frame_cache (next_frame, this_cache);
  trad_frame_get_register (cache, next_frame, regnum, optimizedp, lvalp,
			   addrp, realnump, valuep);
}

static const struct frame_unwind m68k_linux_sigtramp_frame_unwind =
{
  SIGTRAMP_FRAME,
  m68k_linux_sigtramp_frame_this_id,
  m68k_linux_sigtramp_frame_prev_register
};

static const struct frame_unwind *
m68k_linux_sigtramp_frame_sniffer (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  char *name;

  find_pc_partial_function (pc, &name, NULL, NULL);
  if (m68k_linux_pc_in_sigtramp (pc, name))
    return &m68k_linux_sigtramp_frame_unwind;

  return NULL;
}

static void
m68k_linux_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->jb_pc = M68K_LINUX_JB_PC;
  tdep->jb_elt_size = M68K_LINUX_JB_ELEMENT_SIZE;

  /* GNU/Linux uses a calling convention that's similar to SVR4.  It
     returns integer values in %d0/%d1, pointer values in %a0 and
     floating values in %fp0, just like SVR4, but uses %a1 to pass the
     address to store a structure value.  It also returns small
     structures in registers instead of memory.  */
  m68k_svr4_init_abi (info, gdbarch);
  tdep->struct_value_regnum = M68K_A1_REGNUM;
  tdep->struct_return = reg_struct_return;

  set_gdbarch_decr_pc_after_break (gdbarch, 2);

  frame_unwind_append_sniffer (gdbarch, m68k_linux_sigtramp_frame_sniffer);

  /* Shared library handling.  */

  /* GNU/Linux uses SVR4-style shared libraries.  */
  set_solib_svr4_fetch_link_map_offsets (gdbarch,
					 svr4_ilp32_fetch_link_map_offsets);

  /* GNU/Linux uses the dynamic linker included in the GNU C Library.  */
  set_gdbarch_skip_solib_resolver (gdbarch, glibc_skip_solib_resolver);

  set_gdbarch_skip_trampoline_code (gdbarch, find_solib_trampoline_target);

  /* Enable TLS support.  */
  set_gdbarch_fetch_tls_load_module_address (gdbarch,
                                             svr4_fetch_objfile_link_map);
}

void
_initialize_m68k_linux_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_m68k, 0, GDB_OSABI_LINUX,
			  m68k_linux_init_abi);
  observer_attach_inferior_created (m68k_linux_inferior_created);
}
