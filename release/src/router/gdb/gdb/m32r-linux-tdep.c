/* Target-dependent code for GNU/Linux m32r.

   Copyright (C) 2004, 2007 Free Software Foundation, Inc.

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
#include "frame.h"
#include "value.h"
#include "regcache.h"
#include "inferior.h"
#include "osabi.h"
#include "reggroups.h"
#include "regset.h"

#include "gdb_string.h"

#include "glibc-tdep.h"
#include "solib-svr4.h"
#include "symtab.h"

#include "trad-frame.h"
#include "frame-unwind.h"

#include "m32r-tdep.h"


/* Recognizing signal handler frames.  */

/* GNU/Linux has two flavors of signals.  Normal signal handlers, and
   "realtime" (RT) signals.  The RT signals can provide additional
   information to the signal handler if the SA_SIGINFO flag is set
   when establishing a signal handler using `sigaction'.  It is not
   unlikely that future versions of GNU/Linux will support SA_SIGINFO
   for normal signals too.  */

/* When the m32r Linux kernel calls a signal handler and the
   SA_RESTORER flag isn't set, the return address points to a bit of
   code on the stack.  This function returns whether the PC appears to
   be within this bit of code.

   The instruction sequence for normal signals is
       ldi    r7, #__NR_sigreturn
       trap   #2
   or 0x67 0x77 0x10 0xf2.

   Checking for the code sequence should be somewhat reliable, because
   the effect is to call the system call sigreturn.  This is unlikely
   to occur anywhere other than in a signal trampoline.

   It kind of sucks that we have to read memory from the process in
   order to identify a signal trampoline, but there doesn't seem to be
   any other way.  Therefore we only do the memory reads if no
   function name could be identified, which should be the case since
   the code is on the stack.

   Detection of signal trampolines for handlers that set the
   SA_RESTORER flag is in general not possible.  Unfortunately this is
   what the GNU C Library has been doing for quite some time now.
   However, as of version 2.1.2, the GNU C Library uses signal
   trampolines (named __restore and __restore_rt) that are identical
   to the ones used by the kernel.  Therefore, these trampolines are
   supported too.  */

static const gdb_byte linux_sigtramp_code[] = {
  0x67, 0x77, 0x10, 0xf2,
};

/* If PC is in a sigtramp routine, return the address of the start of
   the routine.  Otherwise, return 0.  */

static CORE_ADDR
m32r_linux_sigtramp_start (CORE_ADDR pc, struct frame_info *next_frame)
{
  gdb_byte buf[4];

  /* We only recognize a signal trampoline if PC is at the start of
     one of the instructions.  We optimize for finding the PC at the
     start of the instruction sequence, as will be the case when the
     trampoline is not the first frame on the stack.  We assume that
     in the case where the PC is not at the start of the instruction
     sequence, there will be a few trailing readable bytes on the
     stack.  */

  if (pc % 2 != 0)
    {
      if (!safe_frame_unwind_memory (next_frame, pc, buf, 2))
	return 0;

      if (memcmp (buf, linux_sigtramp_code, 2) == 0)
	pc -= 2;
      else
	return 0;
    }

  if (!safe_frame_unwind_memory (next_frame, pc, buf, 4))
    return 0;

  if (memcmp (buf, linux_sigtramp_code, 4) != 0)
    return 0;

  return pc;
}

/* This function does the same for RT signals.  Here the instruction
   sequence is
       ldi    r7, #__NR_rt_sigreturn
       trap   #2
   or 0x97 0xf0 0x00 0xad 0x10 0xf2 0xf0 0x00.

   The effect is to call the system call rt_sigreturn.  */

static const gdb_byte linux_rt_sigtramp_code[] = {
  0x97, 0xf0, 0x00, 0xad, 0x10, 0xf2, 0xf0, 0x00,
};

/* If PC is in a RT sigtramp routine, return the address of the start
   of the routine.  Otherwise, return 0.  */

static CORE_ADDR
m32r_linux_rt_sigtramp_start (CORE_ADDR pc, struct frame_info *next_frame)
{
  gdb_byte buf[4];

  /* We only recognize a signal trampoline if PC is at the start of
     one of the instructions.  We optimize for finding the PC at the
     start of the instruction sequence, as will be the case when the
     trampoline is not the first frame on the stack.  We assume that
     in the case where the PC is not at the start of the instruction
     sequence, there will be a few trailing readable bytes on the
     stack.  */

  if (pc % 2 != 0)
    return 0;

  if (!safe_frame_unwind_memory (next_frame, pc, buf, 4))
    return 0;

  if (memcmp (buf, linux_rt_sigtramp_code, 4) == 0)
    {
      if (!safe_frame_unwind_memory (next_frame, pc + 4, buf, 4))
	return 0;

      if (memcmp (buf, linux_rt_sigtramp_code + 4, 4) == 0)
	return pc;
    }
  else if (memcmp (buf, linux_rt_sigtramp_code + 4, 4) == 0)
    {
      if (!safe_frame_unwind_memory (next_frame, pc - 4, buf, 4))
	return 0;

      if (memcmp (buf, linux_rt_sigtramp_code, 4) == 0)
	return pc - 4;
    }

  return 0;
}

static int
m32r_linux_pc_in_sigtramp (CORE_ADDR pc, char *name,
			   struct frame_info *next_frame)
{
  /* If we have NAME, we can optimize the search.  The trampolines are
     named __restore and __restore_rt.  However, they aren't dynamically
     exported from the shared C library, so the trampoline may appear to
     be part of the preceding function.  This should always be sigaction,
     __sigaction, or __libc_sigaction (all aliases to the same function).  */
  if (name == NULL || strstr (name, "sigaction") != NULL)
    return (m32r_linux_sigtramp_start (pc, next_frame) != 0
	    || m32r_linux_rt_sigtramp_start (pc, next_frame) != 0);

  return (strcmp ("__restore", name) == 0
	  || strcmp ("__restore_rt", name) == 0);
}

/* From <asm/sigcontext.h>.  */
static int m32r_linux_sc_reg_offset[] = {
  4 * 4,			/* r0 */
  5 * 4,			/* r1 */
  6 * 4,			/* r2 */
  7 * 4,			/* r3 */
  0 * 4,			/* r4 */
  1 * 4,			/* r5 */
  2 * 4,			/* r6 */
  8 * 4,			/* r7 */
  9 * 4,			/* r8 */
  10 * 4,			/* r9 */
  11 * 4,			/* r10 */
  12 * 4,			/* r11 */
  13 * 4,			/* r12 */
  21 * 4,			/* fp */
  22 * 4,			/* lr */
  -1 * 4,			/* sp */
  16 * 4,			/* psw */
  -1 * 4,			/* cbr */
  23 * 4,			/* spi */
  20 * 4,			/* spu */
  19 * 4,			/* bpc */
  17 * 4,			/* pc */
  15 * 4,			/* accl */
  14 * 4			/* acch */
};

struct m32r_frame_cache
{
  CORE_ADDR base, pc;
  struct trad_frame_saved_reg *saved_regs;
};

static struct m32r_frame_cache *
m32r_linux_sigtramp_frame_cache (struct frame_info *next_frame,
				 void **this_cache)
{
  struct m32r_frame_cache *cache;
  CORE_ADDR sigcontext_addr, addr;
  int regnum;

  if ((*this_cache) != NULL)
    return (*this_cache);
  cache = FRAME_OBSTACK_ZALLOC (struct m32r_frame_cache);
  (*this_cache) = cache;
  cache->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  cache->base = frame_unwind_register_unsigned (next_frame, M32R_SP_REGNUM);
  sigcontext_addr = cache->base + 4;

  cache->pc = frame_pc_unwind (next_frame);
  addr = m32r_linux_sigtramp_start (cache->pc, next_frame);
  if (addr == 0)
    {
      /* If this is a RT signal trampoline, adjust SIGCONTEXT_ADDR
         accordingly.  */
      addr = m32r_linux_rt_sigtramp_start (cache->pc, next_frame);
      if (addr)
	sigcontext_addr += 128;
      else
	addr = frame_func_unwind (next_frame, NORMAL_FRAME);
    }
  cache->pc = addr;

  cache->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  for (regnum = 0; regnum < sizeof (m32r_linux_sc_reg_offset) / 4; regnum++)
    {
      if (m32r_linux_sc_reg_offset[regnum] >= 0)
	cache->saved_regs[regnum].addr =
	  sigcontext_addr + m32r_linux_sc_reg_offset[regnum];
    }

  return cache;
}

static void
m32r_linux_sigtramp_frame_this_id (struct frame_info *next_frame,
				   void **this_cache,
				   struct frame_id *this_id)
{
  struct m32r_frame_cache *cache =
    m32r_linux_sigtramp_frame_cache (next_frame, this_cache);

  (*this_id) = frame_id_build (cache->base, cache->pc);
}

static void
m32r_linux_sigtramp_frame_prev_register (struct frame_info *next_frame,
					 void **this_cache,
					 int regnum, int *optimizedp,
					 enum lval_type *lvalp,
					 CORE_ADDR *addrp,
					 int *realnump, gdb_byte *valuep)
{
  struct m32r_frame_cache *cache =
    m32r_linux_sigtramp_frame_cache (next_frame, this_cache);

  trad_frame_get_prev_register (next_frame, cache->saved_regs, regnum,
				optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind m32r_linux_sigtramp_frame_unwind = {
  SIGTRAMP_FRAME,
  m32r_linux_sigtramp_frame_this_id,
  m32r_linux_sigtramp_frame_prev_register
};

static const struct frame_unwind *
m32r_linux_sigtramp_frame_sniffer (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  char *name;

  find_pc_partial_function (pc, &name, NULL, NULL);
  if (m32r_linux_pc_in_sigtramp (pc, name, next_frame))
    return &m32r_linux_sigtramp_frame_unwind;

  return NULL;
}

/* Mapping between the registers in `struct pt_regs'
   format and GDB's register array layout.  */

static int m32r_pt_regs_offset[] = {
  4 * 4,			/* r0 */
  4 * 5,			/* r1 */
  4 * 6,			/* r2 */
  4 * 7,			/* r3 */
  4 * 0,			/* r4 */
  4 * 1,			/* r5 */
  4 * 2,			/* r6 */
  4 * 8,			/* r7 */
  4 * 9,			/* r8 */
  4 * 10,			/* r9 */
  4 * 11,			/* r10 */
  4 * 12,			/* r11 */
  4 * 13,			/* r12 */
  4 * 24,			/* fp */
  4 * 25,			/* lr */
  4 * 23,			/* sp */
  4 * 19,			/* psw */
  4 * 19,			/* cbr */
  4 * 26,			/* spi */
  4 * 23,			/* spu */
  4 * 22,			/* bpc */
  4 * 20,			/* pc */
  4 * 16,			/* accl */
  4 * 15			/* acch */
};

#define PSW_OFFSET (4 * 19)
#define BBPSW_OFFSET (4 * 21)
#define SPU_OFFSET (4 * 23)
#define SPI_OFFSET (4 * 26)

static void
m32r_linux_supply_gregset (const struct regset *regset,
			   struct regcache *regcache, int regnum,
			   const void *gregs, size_t size)
{
  const char *regs = gregs;
  unsigned long psw, bbpsw;
  int i;

  psw = *((unsigned long *) (regs + PSW_OFFSET));
  bbpsw = *((unsigned long *) (regs + BBPSW_OFFSET));

  for (i = 0; i < sizeof (m32r_pt_regs_offset) / 4; i++)
    {
      if (regnum != -1 && regnum != i)
	continue;

      switch (i)
	{
	case PSW_REGNUM:
	  *((unsigned long *) (regs + m32r_pt_regs_offset[i])) =
	    ((0x00c1 & bbpsw) << 8) | ((0xc100 & psw) >> 8);
	  break;
	case CBR_REGNUM:
	  *((unsigned long *) (regs + m32r_pt_regs_offset[i])) =
	    ((psw >> 8) & 1);
	  break;
	case M32R_SP_REGNUM:
	  if (psw & 0x8000)
	    *((unsigned long *) (regs + m32r_pt_regs_offset[i])) =
	      *((unsigned long *) (regs + SPU_OFFSET));
	  else
	    *((unsigned long *) (regs + m32r_pt_regs_offset[i])) =
	      *((unsigned long *) (regs + SPI_OFFSET));
	  break;
	}

      regcache_raw_supply (regcache, i,
			   regs + m32r_pt_regs_offset[i]);
    }
}

static struct regset m32r_linux_gregset = {
  NULL, m32r_linux_supply_gregset
};

static const struct regset *
m32r_linux_regset_from_core_section (struct gdbarch *core_arch,
				     const char *sect_name, size_t sect_size)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (core_arch);
  if (strcmp (sect_name, ".reg") == 0)
    return &m32r_linux_gregset;
  return NULL;
}

static void
m32r_linux_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* Since EVB register is not available for native debug, we reduce
     the number of registers.  */
  set_gdbarch_num_regs (gdbarch, M32R_NUM_REGS - 1);

  frame_unwind_append_sniffer (gdbarch, m32r_linux_sigtramp_frame_sniffer);

  /* GNU/Linux uses SVR4-style shared libraries.  */
  set_gdbarch_skip_trampoline_code (gdbarch, find_solib_trampoline_target);
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);

  /* Core file support.  */
  set_gdbarch_regset_from_core_section
    (gdbarch, m32r_linux_regset_from_core_section);

  /* Enable TLS support.  */
  set_gdbarch_fetch_tls_load_module_address (gdbarch,
                                             svr4_fetch_objfile_link_map);
}

/* Provide a prototype to silence -Wmissing-prototypes.  */
extern void _initialize_m32r_linux_tdep (void);

void
_initialize_m32r_linux_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_m32r, 0, GDB_OSABI_LINUX,
			  m32r_linux_init_abi);
}
