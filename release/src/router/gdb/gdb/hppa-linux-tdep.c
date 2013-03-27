/* Target-dependent code for GNU/Linux running on PA-RISC, for GDB.

   Copyright (C) 2004, 2006, 2007 Free Software Foundation, Inc.

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
#include "osabi.h"
#include "target.h"
#include "objfiles.h"
#include "solib-svr4.h"
#include "glibc-tdep.h"
#include "frame-unwind.h"
#include "trad-frame.h"
#include "dwarf2-frame.h"
#include "value.h"
#include "regset.h"
#include "regcache.h"
#include "hppa-tdep.h"

#include "elf/common.h"

#if 0
/* Convert DWARF register number REG to the appropriate register
   number used by GDB.  */
static int
hppa_dwarf_reg_to_regnum (int reg)
{
  /* registers 0 - 31 are the same in both sets */
  if (reg < 32)
    return reg;

  /* dwarf regs 32 to 85 are fpregs 4 - 31 */
  if (reg >= 32 && reg <= 85)
    return HPPA_FP4_REGNUM + (reg - 32);

  warning (_("Unmapped DWARF Register #%d encountered."), reg);
  return -1;
}
#endif

static void
hppa_linux_target_write_pc (struct regcache *regcache, CORE_ADDR v)
{
  /* Probably this should be done by the kernel, but it isn't.  */
  regcache_cooked_write_unsigned (regcache, HPPA_PCOQ_HEAD_REGNUM, v | 0x3);
  regcache_cooked_write_unsigned (regcache, HPPA_PCOQ_TAIL_REGNUM, (v + 4) | 0x3);
}

/* An instruction to match.  */
struct insn_pattern
{
  unsigned int data;            /* See if it matches this....  */
  unsigned int mask;            /* ... with this mask.  */
};

static struct insn_pattern hppa_sigtramp[] = {
  /* ldi 0, %r25 or ldi 1, %r25 */
  { 0x34190000, 0xfffffffd },
  /* ldi __NR_rt_sigreturn, %r20 */
  { 0x3414015a, 0xffffffff },
  /* be,l 0x100(%sr2, %r0), %sr0, %r31 */
  { 0xe4008200, 0xffffffff },
  /* nop */
  { 0x08000240, 0xffffffff },
  { 0, 0 }
};

#define HPPA_MAX_INSN_PATTERN_LEN (4)

/* Return non-zero if the instructions at PC match the series
   described in PATTERN, or zero otherwise.  PATTERN is an array of
   'struct insn_pattern' objects, terminated by an entry whose mask is
   zero.

   When the match is successful, fill INSN[i] with what PATTERN[i]
   matched.  */
static int
insns_match_pattern (CORE_ADDR pc,
                     struct insn_pattern *pattern,
                     unsigned int *insn)
{
  int i;
  CORE_ADDR npc = pc;

  for (i = 0; pattern[i].mask; i++)
    {
      char buf[4];

      read_memory_nobpt (npc, buf, 4);
      insn[i] = extract_unsigned_integer (buf, 4);
      if ((insn[i] & pattern[i].mask) == pattern[i].data)
        npc += 4;
      else
        return 0;
    }
  return 1;
}

/* Signal frames.  */

/* (This is derived from MD_FALLBACK_FRAME_STATE_FOR in gcc.)
 
   Unfortunately, because of various bugs and changes to the kernel,
   we have several cases to deal with.

   In 2.4, the signal trampoline is 4 bytes, and pc should point directly at 
   the beginning of the trampoline and struct rt_sigframe.

   In <= 2.6.5-rc2-pa3, the signal trampoline is 9 bytes, and pc points at
   the 4th word in the trampoline structure.  This is wrong, it should point 
   at the 5th word.  This is fixed in 2.6.5-rc2-pa4.

   To detect these cases, we first take pc, align it to 64-bytes
   to get the beginning of the signal frame, and then check offsets 0, 4
   and 5 to see if we found the beginning of the trampoline.  This will
   tell us how to locate the sigcontext structure.

   Note that with a 2.4 64-bit kernel, the signal context is not properly
   passed back to userspace so the unwind will not work correctly.  */
static CORE_ADDR
hppa_linux_sigtramp_find_sigcontext (CORE_ADDR pc)
{
  unsigned int dummy[HPPA_MAX_INSN_PATTERN_LEN];
  int offs = 0;
  int try;
  /* offsets to try to find the trampoline */
  static int pcoffs[] = { 0, 4*4, 5*4 };
  /* offsets to the rt_sigframe structure */
  static int sfoffs[] = { 4*4, 10*4, 10*4 };
  CORE_ADDR sp;

  /* Most of the time, this will be correct.  The one case when this will
     fail is if the user defined an alternate stack, in which case the
     beginning of the stack will not be align_down (pc, 64).  */
  sp = align_down (pc, 64);

  /* rt_sigreturn trampoline:
     3419000x ldi 0, %r25 or ldi 1, %r25   (x = 0 or 2)
     3414015a ldi __NR_rt_sigreturn, %r20 
     e4008200 be,l 0x100(%sr2, %r0), %sr0, %r31
     08000240 nop  */

  for (try = 0; try < ARRAY_SIZE (pcoffs); try++)
    {
      if (insns_match_pattern (sp + pcoffs[try], hppa_sigtramp, dummy))
	{
          offs = sfoffs[try];
	  break;
	}
    }

  if (offs == 0)
    {
      if (insns_match_pattern (pc, hppa_sigtramp, dummy))
	{
	  /* sigaltstack case: we have no way of knowing which offset to 
	     use in this case; default to new kernel handling. If this is
	     wrong the unwinding will fail.  */
	  try = 2;
	  sp = pc - pcoffs[try];
	}
      else
      {
        return 0;
      }
    }

  /* sp + sfoffs[try] points to a struct rt_sigframe, which contains
     a struct siginfo and a struct ucontext.  struct ucontext contains
     a struct sigcontext. Return an offset to this sigcontext here.  Too 
     bad we cannot include system specific headers :-(.  
     sizeof(struct siginfo) == 128
     offsetof(struct ucontext, uc_mcontext) == 24.  */
  return sp + sfoffs[try] + 128 + 24;
}

struct hppa_linux_sigtramp_unwind_cache
{
  CORE_ADDR base;
  struct trad_frame_saved_reg *saved_regs;
};

static struct hppa_linux_sigtramp_unwind_cache *
hppa_linux_sigtramp_frame_unwind_cache (struct frame_info *next_frame,
					void **this_cache)
{
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  struct hppa_linux_sigtramp_unwind_cache *info;
  CORE_ADDR pc, scptr;
  int i;

  if (*this_cache)
    return *this_cache;

  info = FRAME_OBSTACK_ZALLOC (struct hppa_linux_sigtramp_unwind_cache);
  *this_cache = info;
  info->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  pc = frame_pc_unwind (next_frame);
  scptr = hppa_linux_sigtramp_find_sigcontext (pc);

  /* structure of struct sigcontext:
   
     struct sigcontext {
	unsigned long sc_flags;
	unsigned long sc_gr[32]; 
	unsigned long long sc_fr[32];
	unsigned long sc_iasq[2];
	unsigned long sc_iaoq[2];
	unsigned long sc_sar;           */

  /* Skip sc_flags.  */
  scptr += 4;

  /* GR[0] is the psw, we don't restore that.  */
  scptr += 4;

  /* General registers.  */
  for (i = 1; i < 32; i++)
    {
      info->saved_regs[HPPA_R0_REGNUM + i].addr = scptr;
      scptr += 4;
    }

  /* Pad.  */
  scptr += 4;

  /* FP regs; FP0-3 are not restored.  */
  scptr += (8 * 4);

  for (i = 4; i < 32; i++)
    {
      info->saved_regs[HPPA_FP0_REGNUM + (i * 2)].addr = scptr;
      scptr += 4;
      info->saved_regs[HPPA_FP0_REGNUM + (i * 2) + 1].addr = scptr;
      scptr += 4;
    }

  /* IASQ/IAOQ. */
  info->saved_regs[HPPA_PCSQ_HEAD_REGNUM].addr = scptr;
  scptr += 4;
  info->saved_regs[HPPA_PCSQ_TAIL_REGNUM].addr = scptr;
  scptr += 4;

  info->saved_regs[HPPA_PCOQ_HEAD_REGNUM].addr = scptr;
  scptr += 4;
  info->saved_regs[HPPA_PCOQ_TAIL_REGNUM].addr = scptr;
  scptr += 4;

  info->base = frame_unwind_register_unsigned (next_frame, HPPA_SP_REGNUM);

  return info;
}

static void
hppa_linux_sigtramp_frame_this_id (struct frame_info *next_frame,
				   void **this_prologue_cache,
				   struct frame_id *this_id)
{
  struct hppa_linux_sigtramp_unwind_cache *info
    = hppa_linux_sigtramp_frame_unwind_cache (next_frame, this_prologue_cache);
  *this_id = frame_id_build (info->base, frame_pc_unwind (next_frame));
}

static void
hppa_linux_sigtramp_frame_prev_register (struct frame_info *next_frame,
					 void **this_prologue_cache,
					 int regnum, int *optimizedp,
					 enum lval_type *lvalp, 
					 CORE_ADDR *addrp,
					 int *realnump, gdb_byte *valuep)
{
  struct hppa_linux_sigtramp_unwind_cache *info
    = hppa_linux_sigtramp_frame_unwind_cache (next_frame, this_prologue_cache);
  hppa_frame_prev_register_helper (next_frame, info->saved_regs, regnum,
		                   optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind hppa_linux_sigtramp_frame_unwind = {
  SIGTRAMP_FRAME,
  hppa_linux_sigtramp_frame_this_id,
  hppa_linux_sigtramp_frame_prev_register
};

/* hppa-linux always uses "new-style" rt-signals.  The signal handler's return
   address should point to a signal trampoline on the stack.  The signal
   trampoline is embedded in a rt_sigframe structure that is aligned on
   the stack.  We take advantage of the fact that sp must be 64-byte aligned,
   and the trampoline is small, so by rounding down the trampoline address
   we can find the beginning of the struct rt_sigframe.  */
static const struct frame_unwind *
hppa_linux_sigtramp_unwind_sniffer (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);

  if (hppa_linux_sigtramp_find_sigcontext (pc))
    return &hppa_linux_sigtramp_frame_unwind;

  return NULL;
}

/* Attempt to find (and return) the global pointer for the given
   function.

   This is a rather nasty bit of code searchs for the .dynamic section
   in the objfile corresponding to the pc of the function we're trying
   to call.  Once it finds the addresses at which the .dynamic section
   lives in the child process, it scans the Elf32_Dyn entries for a
   DT_PLTGOT tag.  If it finds one of these, the corresponding
   d_un.d_ptr value is the global pointer.  */

static CORE_ADDR
hppa_linux_find_global_pointer (struct value *function)
{
  struct obj_section *faddr_sect;
  CORE_ADDR faddr;
  
  faddr = value_as_address (function);

  /* Is this a plabel? If so, dereference it to get the gp value.  */
  if (faddr & 2)
    {
      int status;
      char buf[4];

      faddr &= ~3;

      status = target_read_memory (faddr + 4, buf, sizeof (buf));
      if (status == 0)
	return extract_unsigned_integer (buf, sizeof (buf));
    }

  /* If the address is in the plt section, then the real function hasn't 
     yet been fixed up by the linker so we cannot determine the gp of 
     that function.  */
  if (in_plt_section (faddr, NULL))
    return 0;

  faddr_sect = find_pc_section (faddr);
  if (faddr_sect != NULL)
    {
      struct obj_section *osect;

      ALL_OBJFILE_OSECTIONS (faddr_sect->objfile, osect)
	{
	  if (strcmp (osect->the_bfd_section->name, ".dynamic") == 0)
	    break;
	}

      if (osect < faddr_sect->objfile->sections_end)
	{
	  CORE_ADDR addr;

	  addr = osect->addr;
	  while (addr < osect->endaddr)
	    {
	      int status;
	      LONGEST tag;
	      char buf[4];

	      status = target_read_memory (addr, buf, sizeof (buf));
	      if (status != 0)
		break;
	      tag = extract_signed_integer (buf, sizeof (buf));

	      if (tag == DT_PLTGOT)
		{
		  CORE_ADDR global_pointer;

		  status = target_read_memory (addr + 4, buf, sizeof (buf));
		  if (status != 0)
		    break;
		  global_pointer = extract_unsigned_integer (buf, sizeof (buf));

		  /* The payoff... */
		  return global_pointer;
		}

	      if (tag == DT_NULL)
		break;

	      addr += 8;
	    }
	}
    }
  return 0;
}

/*
 * Registers saved in a coredump:
 * gr0..gr31
 * sr0..sr7
 * iaoq0..iaoq1
 * iasq0..iasq1
 * sar, iir, isr, ior, ipsw
 * cr0, cr24..cr31
 * cr8,9,12,13
 * cr10, cr15
 */

#define GR_REGNUM(_n)	(HPPA_R0_REGNUM+_n)
#define TR_REGNUM(_n)	(HPPA_TR0_REGNUM+_n)
static const int greg_map[] =
  {
    GR_REGNUM(0), GR_REGNUM(1), GR_REGNUM(2), GR_REGNUM(3),
    GR_REGNUM(4), GR_REGNUM(5), GR_REGNUM(6), GR_REGNUM(7),
    GR_REGNUM(8), GR_REGNUM(9), GR_REGNUM(10), GR_REGNUM(11),
    GR_REGNUM(12), GR_REGNUM(13), GR_REGNUM(14), GR_REGNUM(15),
    GR_REGNUM(16), GR_REGNUM(17), GR_REGNUM(18), GR_REGNUM(19),
    GR_REGNUM(20), GR_REGNUM(21), GR_REGNUM(22), GR_REGNUM(23),
    GR_REGNUM(24), GR_REGNUM(25), GR_REGNUM(26), GR_REGNUM(27),
    GR_REGNUM(28), GR_REGNUM(29), GR_REGNUM(30), GR_REGNUM(31),

    HPPA_SR4_REGNUM+1, HPPA_SR4_REGNUM+2, HPPA_SR4_REGNUM+3, HPPA_SR4_REGNUM+4,
    HPPA_SR4_REGNUM, HPPA_SR4_REGNUM+5, HPPA_SR4_REGNUM+6, HPPA_SR4_REGNUM+7,

    HPPA_PCOQ_HEAD_REGNUM, HPPA_PCOQ_TAIL_REGNUM,
    HPPA_PCSQ_HEAD_REGNUM, HPPA_PCSQ_TAIL_REGNUM,

    HPPA_SAR_REGNUM, HPPA_IIR_REGNUM, HPPA_ISR_REGNUM, HPPA_IOR_REGNUM,
    HPPA_IPSW_REGNUM, HPPA_RCR_REGNUM,

    TR_REGNUM(0), TR_REGNUM(1), TR_REGNUM(2), TR_REGNUM(3),
    TR_REGNUM(4), TR_REGNUM(5), TR_REGNUM(6), TR_REGNUM(7),

    HPPA_PID0_REGNUM, HPPA_PID1_REGNUM, HPPA_PID2_REGNUM, HPPA_PID3_REGNUM,
    HPPA_CCR_REGNUM, HPPA_EIEM_REGNUM,
  };

static void
hppa_linux_supply_regset (const struct regset *regset,
			  struct regcache *regcache,
			  int regnum, const void *regs, size_t len)
{
  struct gdbarch *arch = get_regcache_arch (regcache);
  struct gdbarch_tdep *tdep = gdbarch_tdep (arch);
  const char *buf = regs;
  int i, offset;

  offset = 0;
  for (i = 0; i < ARRAY_SIZE (greg_map); i++)
    {
      if (regnum == greg_map[i] || regnum == -1)
        regcache_raw_supply (regcache, greg_map[i], buf + offset);

      offset += tdep->bytes_per_address;
    }
}

static void
hppa_linux_supply_fpregset (const struct regset *regset,
			    struct regcache *regcache,
			    int regnum, const void *regs, size_t len)
{
  const char *buf = regs;
  int i, offset;

  offset = 0;
  for (i = 0; i < 31; i++)
    {
      if (regnum == HPPA_FP0_REGNUM + i || regnum == -1)
        regcache_raw_supply (regcache, HPPA_FP0_REGNUM + i, 
			     buf + offset);
      offset += 8;
    }
}

/* HPPA Linux kernel register set.  */
static struct regset hppa_linux_regset =
{
  NULL,
  hppa_linux_supply_regset
};

static struct regset hppa_linux_fpregset =
{
  NULL,
  hppa_linux_supply_fpregset
};

static const struct regset *
hppa_linux_regset_from_core_section (struct gdbarch *gdbarch,
				     const char *sect_name,
				     size_t sect_size)
{
  if (strcmp (sect_name, ".reg") == 0)
    return &hppa_linux_regset;
  else if (strcmp (sect_name, ".reg2") == 0)
    return &hppa_linux_fpregset;

  return NULL;
}


/* Forward declarations.  */
extern initialize_file_ftype _initialize_hppa_linux_tdep;

static void
hppa_linux_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* GNU/Linux is always ELF.  */
  tdep->is_elf = 1;

  tdep->find_global_pointer = hppa_linux_find_global_pointer;

  set_gdbarch_write_pc (gdbarch, hppa_linux_target_write_pc);

  frame_unwind_append_sniffer (gdbarch, hppa_linux_sigtramp_unwind_sniffer);

  /* GNU/Linux uses SVR4-style shared libraries.  */
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);

  tdep->in_solib_call_trampoline = hppa_in_solib_call_trampoline;
  set_gdbarch_skip_trampoline_code (gdbarch, hppa_skip_trampoline_code);

  /* GNU/Linux uses the dynamic linker included in the GNU C Library.  */
  set_gdbarch_skip_solib_resolver (gdbarch, glibc_skip_solib_resolver);

  /* On hppa-linux, currently, sizeof(long double) == 8.  There has been
     some discussions to support 128-bit long double, but it requires some
     more work in gcc and glibc first.  */
  set_gdbarch_long_double_bit (gdbarch, 64);

  set_gdbarch_regset_from_core_section
    (gdbarch, hppa_linux_regset_from_core_section);

#if 0
  /* Dwarf-2 unwinding support.  Not yet working.  */
  set_gdbarch_dwarf_reg_to_regnum (gdbarch, hppa_dwarf_reg_to_regnum);
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, hppa_dwarf_reg_to_regnum);
  frame_unwind_append_sniffer (gdbarch, dwarf2_frame_sniffer);
  frame_base_append_sniffer (gdbarch, dwarf2_frame_base_sniffer);
#endif

  /* Enable TLS support.  */
  set_gdbarch_fetch_tls_load_module_address (gdbarch,
                                             svr4_fetch_objfile_link_map);
}

void
_initialize_hppa_linux_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_hppa, 0, GDB_OSABI_LINUX, hppa_linux_init_abi);
  gdbarch_register_osabi (bfd_arch_hppa, bfd_mach_hppa20w, GDB_OSABI_LINUX, hppa_linux_init_abi);
}
