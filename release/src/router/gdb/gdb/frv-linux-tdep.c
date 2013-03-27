/* Target-dependent code for GNU/Linux running on the Fujitsu FR-V,
   for GDB.

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
#include "target.h"
#include "frame.h"
#include "osabi.h"
#include "regcache.h"
#include "elf-bfd.h"
#include "elf/frv.h"
#include "frv-tdep.h"
#include "trad-frame.h"
#include "frame-unwind.h"
#include "regset.h"
#include "gdb_string.h"

/* Define the size (in bytes) of an FR-V instruction.  */
static const int frv_instr_size = 4;

enum {
  NORMAL_SIGTRAMP = 1,
  RT_SIGTRAMP = 2
};

static int
frv_linux_pc_in_sigtramp (CORE_ADDR pc, char *name)
{
  char buf[frv_instr_size];
  LONGEST instr;
  int retval = 0;

  if (target_read_memory (pc, buf, sizeof buf) != 0)
    return 0;

  instr = extract_unsigned_integer (buf, sizeof buf);

  if (instr == 0x8efc0077)	/* setlos #__NR_sigreturn, gr7 */
    retval = NORMAL_SIGTRAMP;
  else if (instr -= 0x8efc00ad)	/* setlos #__NR_rt_sigreturn, gr7 */
    retval = RT_SIGTRAMP;
  else
    return 0;

  if (target_read_memory (pc + frv_instr_size, buf, sizeof buf) != 0)
    return 0;
  instr = extract_unsigned_integer (buf, sizeof buf);
  if (instr != 0xc0700000)	/* tira	gr0, 0 */
    return 0;

  /* If we get this far, we'll return a non-zero value, either
     NORMAL_SIGTRAMP (1) or RT_SIGTRAMP (2).  */
  return retval;
}

/* Given NEXT_FRAME, the "callee" frame of the sigtramp frame that we
   wish to decode, and REGNO, one of the frv register numbers defined
   in frv-tdep.h, return the address of the saved register (corresponding
   to REGNO) in the sigtramp frame.  Return -1 if the register is not
   found in the sigtramp frame.  The magic numbers in the code below
   were computed by examining the following kernel structs:

   From arch/frv/kernel/signal.c:

      struct sigframe
      {
	      void (*pretcode)(void);
	      int sig;
	      struct sigcontext sc;
	      unsigned long extramask[_NSIG_WORDS-1];
	      uint32_t retcode[2];
      };

      struct rt_sigframe
      {
	      void (*pretcode)(void);
	      int sig;
	      struct siginfo *pinfo;
	      void *puc;
	      struct siginfo info;
	      struct ucontext uc;
	      uint32_t retcode[2];
      };

   From include/asm-frv/ucontext.h:

      struct ucontext {
	      unsigned long		uc_flags;
	      struct ucontext		*uc_link;
	      stack_t			uc_stack;
	      struct sigcontext	uc_mcontext;
	      sigset_t		uc_sigmask;
      };

   From include/asm-frv/signal.h:

      typedef struct sigaltstack {
	      void *ss_sp;
	      int ss_flags;
	      size_t ss_size;
      } stack_t;

   From include/asm-frv/sigcontext.h:

      struct sigcontext {
	      struct user_context	sc_context;
	      unsigned long		sc_oldmask;
      } __attribute__((aligned(8)));

   From include/asm-frv/registers.h:
      struct user_int_regs
      {
	      unsigned long		psr;
	      unsigned long		isr;
	      unsigned long		ccr;
	      unsigned long		cccr;
	      unsigned long		lr;
	      unsigned long		lcr;
	      unsigned long		pc;
	      unsigned long		__status;
	      unsigned long		syscallno;
	      unsigned long		orig_gr8;
	      unsigned long		gner[2];
	      unsigned long long	iacc[1];

	      union {
		      unsigned long	tbr;
		      unsigned long	gr[64];
	      };
      };

      struct user_fpmedia_regs
      {
	      unsigned long	fr[64];
	      unsigned long	fner[2];
	      unsigned long	msr[2];
	      unsigned long	acc[8];
	      unsigned char	accg[8];
	      unsigned long	fsr[1];
      };

      struct user_context
      {
	      struct user_int_regs		i;
	      struct user_fpmedia_regs	f;

	      void *extension;
      } __attribute__((aligned(8)));  */

static LONGEST
frv_linux_sigcontext_reg_addr (struct frame_info *next_frame, int regno,
                               CORE_ADDR *sc_addr_cache_ptr)
{
  CORE_ADDR sc_addr;

  if (sc_addr_cache_ptr && *sc_addr_cache_ptr)
    {
      sc_addr = *sc_addr_cache_ptr;
    }
  else
    {
      CORE_ADDR pc, sp;
      char buf[4];
      int tramp_type;

      pc = frame_pc_unwind (next_frame);
      tramp_type = frv_linux_pc_in_sigtramp (pc, 0);

      frame_unwind_register (next_frame, sp_regnum, buf);
      sp = extract_unsigned_integer (buf, sizeof buf);

      if (tramp_type == NORMAL_SIGTRAMP)
	{
	  /* For a normal sigtramp frame, the sigcontext struct starts
	     at SP + 8.  */
	  sc_addr = sp + 8;
	}
      else if (tramp_type == RT_SIGTRAMP)
	{
	  /* For a realtime sigtramp frame, SP + 12 contains a pointer
 	     to a ucontext struct.  The ucontext struct contains a
 	     sigcontext struct starting 24 bytes in.  (The offset of
 	     uc_mcontext within struct ucontext is derived as follows: 
 	     stack_t is a 12-byte struct and struct sigcontext is
 	     8-byte aligned.  This gives an offset of 8 + 12 + 4 (for
 	     padding) = 24.) */
	  if (target_read_memory (sp + 12, buf, sizeof buf) != 0)
	    {
	      warning (_("Can't read realtime sigtramp frame."));
	      return 0;
	    }
	  sc_addr = extract_unsigned_integer (buf, sizeof buf);
 	  sc_addr += 24;
	}
      else
	internal_error (__FILE__, __LINE__, _("not a signal trampoline"));

      if (sc_addr_cache_ptr)
	*sc_addr_cache_ptr = sc_addr;
    }

  switch (regno)
    {
    case psr_regnum :
      return sc_addr + 0;
    /* sc_addr + 4 has "isr", the Integer Status Register.  */
    case ccr_regnum :
      return sc_addr + 8;
    case cccr_regnum :
      return sc_addr + 12;
    case lr_regnum :
      return sc_addr + 16;
    case lcr_regnum :
      return sc_addr + 20;
    case pc_regnum :
      return sc_addr + 24;
    /* sc_addr + 28 is __status, the exception status.
       sc_addr + 32 is syscallno, the syscall number or -1.
       sc_addr + 36 is orig_gr8, the original syscall arg #1.
       sc_addr + 40 is gner[0].
       sc_addr + 44 is gner[1]. */
    case iacc0h_regnum :
      return sc_addr + 48;
    case iacc0l_regnum :
      return sc_addr + 52;
    default : 
      if (first_gpr_regnum <= regno && regno <= last_gpr_regnum)
	return sc_addr + 56 + 4 * (regno - first_gpr_regnum);
      else if (first_fpr_regnum <= regno && regno <= last_fpr_regnum)
	return sc_addr + 312 + 4 * (regno - first_fpr_regnum);
      else
	return -1;  /* not saved. */
    }
}

/* Signal trampolines.  */

static struct trad_frame_cache *
frv_linux_sigtramp_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct trad_frame_cache *cache;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  CORE_ADDR addr;
  char buf[4];
  int regnum;
  CORE_ADDR sc_addr_cache_val = 0;
  struct frame_id this_id;

  if (*this_cache)
    return *this_cache;

  cache = trad_frame_cache_zalloc (next_frame);

  /* FIXME: cagney/2004-05-01: This is is long standing broken code.
     The frame ID's code address should be the start-address of the
     signal trampoline and not the current PC within that
     trampoline.  */
  frame_unwind_register (next_frame, sp_regnum, buf);
  this_id = frame_id_build (extract_unsigned_integer (buf, sizeof buf),
			    frame_pc_unwind (next_frame));
  trad_frame_set_id (cache, this_id);

  for (regnum = 0; regnum < frv_num_regs; regnum++)
    {
      LONGEST reg_addr = frv_linux_sigcontext_reg_addr (next_frame, regnum,
							&sc_addr_cache_val);
      if (reg_addr != -1)
	trad_frame_set_reg_addr (cache, regnum, reg_addr);
    }

  *this_cache = cache;
  return cache;
}

static void
frv_linux_sigtramp_frame_this_id (struct frame_info *next_frame, void **this_cache,
			     struct frame_id *this_id)
{
  struct trad_frame_cache *cache =
    frv_linux_sigtramp_frame_cache (next_frame, this_cache);
  trad_frame_get_id (cache, this_id);
}

static void
frv_linux_sigtramp_frame_prev_register (struct frame_info *next_frame,
				   void **this_cache,
				   int regnum, int *optimizedp,
				   enum lval_type *lvalp, CORE_ADDR *addrp,
				   int *realnump, gdb_byte *valuep)
{
  /* Make sure we've initialized the cache.  */
  struct trad_frame_cache *cache =
    frv_linux_sigtramp_frame_cache (next_frame, this_cache);
  trad_frame_get_register (cache, next_frame, regnum, optimizedp, lvalp,
			   addrp, realnump, valuep);
}

static const struct frame_unwind frv_linux_sigtramp_frame_unwind =
{
  SIGTRAMP_FRAME,
  frv_linux_sigtramp_frame_this_id,
  frv_linux_sigtramp_frame_prev_register
};

static const struct frame_unwind *
frv_linux_sigtramp_frame_sniffer (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  char *name;

  find_pc_partial_function (pc, &name, NULL, NULL);
  if (frv_linux_pc_in_sigtramp (pc, name))
    return &frv_linux_sigtramp_frame_unwind;

  return NULL;
}


/* The FRV kernel defines ELF_NGREG as 46.  We add 2 in order to include
   the loadmap addresses in the register set.  (See below for more info.)  */
#define FRV_ELF_NGREG (46 + 2)
typedef unsigned char frv_elf_greg_t[4];
typedef struct { frv_elf_greg_t reg[FRV_ELF_NGREG]; } frv_elf_gregset_t;

typedef unsigned char frv_elf_fpreg_t[4];
typedef struct
{
  frv_elf_fpreg_t fr[64];
  frv_elf_fpreg_t fner[2];
  frv_elf_fpreg_t msr[2];
  frv_elf_fpreg_t acc[8];
  unsigned char accg[8];
  frv_elf_fpreg_t fsr[1];
} frv_elf_fpregset_t;

/* Constants for accessing elements of frv_elf_gregset_t.  */

#define FRV_PT_PSR 0
#define	FRV_PT_ISR 1
#define FRV_PT_CCR 2
#define FRV_PT_CCCR 3
#define FRV_PT_LR 4
#define FRV_PT_LCR 5
#define FRV_PT_PC 6
#define FRV_PT_GNER0 10
#define FRV_PT_GNER1 11
#define FRV_PT_IACC0H 12
#define FRV_PT_IACC0L 13

/* Note: Only 32 of the GRs will be found in the corefile.  */
#define FRV_PT_GR(j)	( 14 + (j))	/* GRj for 0<=j<=63. */

#define FRV_PT_TBR FRV_PT_GR(0)		/* gr0 is always 0, so TBR is stuffed
					   there.  */

/* Technically, the loadmap addresses are not part of `pr_reg' as
   found in the elf_prstatus struct.  The fields which communicate the
   loadmap address appear (by design) immediately after `pr_reg'
   though, and the BFD function elf32_frv_grok_prstatus() has been
   implemented to include these fields in the register section that it
   extracts from the core file.  So, for our purposes, they may be
   viewed as registers.  */

#define FRV_PT_EXEC_FDPIC_LOADMAP 46
#define FRV_PT_INTERP_FDPIC_LOADMAP 47


/* Unpack an frv_elf_gregset_t into GDB's register cache.  */

static void 
frv_linux_supply_gregset (const struct regset *regset,
                          struct regcache *regcache,
			  int regnum, const void *gregs, size_t len)
{
  int regi;
  char zerobuf[MAX_REGISTER_SIZE];
  const frv_elf_gregset_t *gregsetp = gregs;

  memset (zerobuf, 0, MAX_REGISTER_SIZE);

  /* gr0 always contains 0.  Also, the kernel passes the TBR value in
     this slot.  */
  regcache_raw_supply (regcache, first_gpr_regnum, zerobuf);

  for (regi = first_gpr_regnum + 1; regi <= last_gpr_regnum; regi++)
    {
      if (regi >= first_gpr_regnum + 32)
	regcache_raw_supply (regcache, regi, zerobuf);
      else
	regcache_raw_supply (regcache, regi,
			     gregsetp->reg[FRV_PT_GR (regi - first_gpr_regnum)]);
    }

  regcache_raw_supply (regcache, pc_regnum, gregsetp->reg[FRV_PT_PC]);
  regcache_raw_supply (regcache, psr_regnum, gregsetp->reg[FRV_PT_PSR]);
  regcache_raw_supply (regcache, ccr_regnum, gregsetp->reg[FRV_PT_CCR]);
  regcache_raw_supply (regcache, cccr_regnum, gregsetp->reg[FRV_PT_CCCR]);
  regcache_raw_supply (regcache, lr_regnum, gregsetp->reg[FRV_PT_LR]);
  regcache_raw_supply (regcache, lcr_regnum, gregsetp->reg[FRV_PT_LCR]);
  regcache_raw_supply (regcache, gner0_regnum, gregsetp->reg[FRV_PT_GNER0]);
  regcache_raw_supply (regcache, gner1_regnum, gregsetp->reg[FRV_PT_GNER1]);
  regcache_raw_supply (regcache, tbr_regnum, gregsetp->reg[FRV_PT_TBR]);
  regcache_raw_supply (regcache, fdpic_loadmap_exec_regnum,
                       gregsetp->reg[FRV_PT_EXEC_FDPIC_LOADMAP]);
  regcache_raw_supply (regcache, fdpic_loadmap_interp_regnum,
                       gregsetp->reg[FRV_PT_INTERP_FDPIC_LOADMAP]);
}

/* Unpack an frv_elf_fpregset_t into GDB's register cache.  */

static void
frv_linux_supply_fpregset (const struct regset *regset,
                           struct regcache *regcache,
			   int regnum, const void *gregs, size_t len)
{
  int regi;
  const frv_elf_fpregset_t *fpregsetp = gregs;

  for (regi = first_fpr_regnum; regi <= last_fpr_regnum; regi++)
    regcache_raw_supply (regcache, regi, fpregsetp->fr[regi - first_fpr_regnum]);

  regcache_raw_supply (regcache, fner0_regnum, fpregsetp->fner[0]);
  regcache_raw_supply (regcache, fner1_regnum, fpregsetp->fner[1]);

  regcache_raw_supply (regcache, msr0_regnum, fpregsetp->msr[0]);
  regcache_raw_supply (regcache, msr1_regnum, fpregsetp->msr[1]);

  for (regi = acc0_regnum; regi <= acc7_regnum; regi++)
    regcache_raw_supply (regcache, regi, fpregsetp->acc[regi - acc0_regnum]);

  regcache_raw_supply (regcache, accg0123_regnum, fpregsetp->accg);
  regcache_raw_supply (regcache, accg4567_regnum, fpregsetp->accg + 4);

  regcache_raw_supply (regcache, fsr0_regnum, fpregsetp->fsr[0]);
}

/* FRV Linux kernel register sets.  */

static struct regset frv_linux_gregset =
{
  NULL,
  frv_linux_supply_gregset
};

static struct regset frv_linux_fpregset =
{
  NULL,
  frv_linux_supply_fpregset
};

static const struct regset *
frv_linux_regset_from_core_section (struct gdbarch *gdbarch,
				    const char *sect_name, size_t sect_size)
{
  if (strcmp (sect_name, ".reg") == 0 
      && sect_size >= sizeof (frv_elf_gregset_t))
    return &frv_linux_gregset;

  if (strcmp (sect_name, ".reg2") == 0 
      && sect_size >= sizeof (frv_elf_fpregset_t))
    return &frv_linux_fpregset;

  return NULL;
}


static void
frv_linux_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  /* Set the sigtramp frame sniffer.  */
  frame_unwind_append_sniffer (gdbarch, frv_linux_sigtramp_frame_sniffer); 
  set_gdbarch_regset_from_core_section (gdbarch,
                                        frv_linux_regset_from_core_section);
}

static enum gdb_osabi
frv_linux_elf_osabi_sniffer (bfd *abfd)
{
  int elf_flags;

  elf_flags = elf_elfheader (abfd)->e_flags;

  /* Assume GNU/Linux if using the FDPIC ABI.  If/when another OS shows
     up that uses this ABI, we'll need to start using .note sections
     or some such.  */
  if (elf_flags & EF_FRV_FDPIC)
    return GDB_OSABI_LINUX;
  else
    return GDB_OSABI_UNKNOWN;
}

/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_frv_linux_tdep (void);

void
_initialize_frv_linux_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_frv, 0, GDB_OSABI_LINUX, frv_linux_init_abi);
  gdbarch_register_osabi_sniffer (bfd_arch_frv,
				  bfd_target_elf_flavour,
				  frv_linux_elf_osabi_sniffer);
}
