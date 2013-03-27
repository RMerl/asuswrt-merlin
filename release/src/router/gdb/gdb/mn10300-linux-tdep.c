/* Target-dependent code for the Matsushita MN10300 for GDB, the GNU debugger.

   Copyright (C) 2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

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
#include "gdb_string.h"
#include "regcache.h"
#include "mn10300-tdep.h"
#include "gdb_assert.h"
#include "bfd.h"
#include "elf-bfd.h"
#include "osabi.h"
#include "regset.h"
#include "solib-svr4.h"
#include "frame.h"
#include "trad-frame.h"
#include "tramp-frame.h"

#include <stdlib.h>

/* Transliterated from <asm-mn10300/elf.h>...  */
#define MN10300_ELF_NGREG 28
#define MN10300_ELF_NFPREG 32

typedef gdb_byte   mn10300_elf_greg_t[4];
typedef mn10300_elf_greg_t mn10300_elf_gregset_t[MN10300_ELF_NGREG];

typedef gdb_byte   mn10300_elf_fpreg_t[4];
typedef struct
{
  mn10300_elf_fpreg_t fpregs[MN10300_ELF_NFPREG];
  gdb_byte    fpcr[4];
} mn10300_elf_fpregset_t;

/* elf_gregset_t register indices stolen from include/asm-mn10300/ptrace.h.  */
#define MN10300_ELF_GREGSET_T_REG_INDEX_A3	0
#define MN10300_ELF_GREGSET_T_REG_INDEX_A2	1
#define MN10300_ELF_GREGSET_T_REG_INDEX_D3	2
#define	MN10300_ELF_GREGSET_T_REG_INDEX_D2	3
#define MN10300_ELF_GREGSET_T_REG_INDEX_MCVF	4
#define	MN10300_ELF_GREGSET_T_REG_INDEX_MCRL	5
#define MN10300_ELF_GREGSET_T_REG_INDEX_MCRH	6
#define	MN10300_ELF_GREGSET_T_REG_INDEX_MDRQ	7
#define	MN10300_ELF_GREGSET_T_REG_INDEX_E1	8
#define	MN10300_ELF_GREGSET_T_REG_INDEX_E0	9
#define	MN10300_ELF_GREGSET_T_REG_INDEX_E7	10
#define	MN10300_ELF_GREGSET_T_REG_INDEX_E6	11
#define	MN10300_ELF_GREGSET_T_REG_INDEX_E5	12
#define	MN10300_ELF_GREGSET_T_REG_INDEX_E4	13
#define	MN10300_ELF_GREGSET_T_REG_INDEX_E3	14
#define	MN10300_ELF_GREGSET_T_REG_INDEX_E2	15
#define	MN10300_ELF_GREGSET_T_REG_INDEX_SP	16
#define	MN10300_ELF_GREGSET_T_REG_INDEX_LAR	17
#define	MN10300_ELF_GREGSET_T_REG_INDEX_LIR	18
#define	MN10300_ELF_GREGSET_T_REG_INDEX_MDR	19
#define	MN10300_ELF_GREGSET_T_REG_INDEX_A1	20
#define	MN10300_ELF_GREGSET_T_REG_INDEX_A0	21
#define	MN10300_ELF_GREGSET_T_REG_INDEX_D1	22
#define	MN10300_ELF_GREGSET_T_REG_INDEX_D0	23
#define MN10300_ELF_GREGSET_T_REG_INDEX_ORIG_D0	24
#define	MN10300_ELF_GREGSET_T_REG_INDEX_EPSW	25
#define	MN10300_ELF_GREGSET_T_REG_INDEX_PC	26

/* New gdbarch API for corefile registers.
   Given a section name and size, create a struct reg object
   with a supply_register and a collect_register method.  */

/* Copy register value of REGNUM from regset to regcache.  
   If REGNUM is -1, do this for all gp registers in regset.  */

static void
am33_supply_gregset_method (const struct regset *regset, 
			    struct regcache *regcache, 
			    int regnum, const void *gregs, size_t len)
{
  char zerobuf[MAX_REGISTER_SIZE];
  const mn10300_elf_greg_t *regp = (const mn10300_elf_greg_t *) gregs;
  int i;

  gdb_assert (len == sizeof (mn10300_elf_gregset_t));

  switch (regnum) {
  case E_D0_REGNUM:
    regcache_raw_supply (regcache, E_D0_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_D0));
    break;
  case E_D1_REGNUM:
    regcache_raw_supply (regcache, E_D1_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_D1));
    break;
  case E_D2_REGNUM:
    regcache_raw_supply (regcache, E_D2_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_D2));
    break;
  case E_D3_REGNUM:
    regcache_raw_supply (regcache, E_D3_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_D3));
    break;
  case E_A0_REGNUM:
    regcache_raw_supply (regcache, E_A0_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_A0));
    break;
  case E_A1_REGNUM:
    regcache_raw_supply (regcache, E_A1_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_A1));
    break;
  case E_A2_REGNUM:
    regcache_raw_supply (regcache, E_A2_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_A2));
    break;
  case E_A3_REGNUM:
    regcache_raw_supply (regcache, E_A3_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_A3));
    break;
  case E_SP_REGNUM:
    regcache_raw_supply (regcache, E_SP_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_SP));
    break;
  case E_PC_REGNUM:
    regcache_raw_supply (regcache, E_PC_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_PC));
    break;
  case E_MDR_REGNUM:
    regcache_raw_supply (regcache, E_MDR_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_MDR));
    break;
  case E_PSW_REGNUM:
    regcache_raw_supply (regcache, E_PSW_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_EPSW));
    break;
  case E_LIR_REGNUM:
    regcache_raw_supply (regcache, E_LIR_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_LIR));
    break;
  case E_LAR_REGNUM:
    regcache_raw_supply (regcache, E_LAR_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_LAR));
    break;
  case E_MDRQ_REGNUM:
    regcache_raw_supply (regcache, E_MDRQ_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_MDRQ));
    break;
  case E_E0_REGNUM:
    regcache_raw_supply (regcache, E_E0_REGNUM,   
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_E0));
    break;
  case E_E1_REGNUM:
    regcache_raw_supply (regcache, E_E1_REGNUM,
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_E1));
    break;
  case E_E2_REGNUM:
    regcache_raw_supply (regcache, E_E2_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_E2));
    break;
  case E_E3_REGNUM:
    regcache_raw_supply (regcache, E_E3_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_E3));
    break;
  case E_E4_REGNUM:
    regcache_raw_supply (regcache, E_E4_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_E4));
    break;
  case E_E5_REGNUM:
    regcache_raw_supply (regcache, E_E5_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_E5));
    break;
  case E_E6_REGNUM:
    regcache_raw_supply (regcache, E_E6_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_E6));
    break;
  case E_E7_REGNUM:
    regcache_raw_supply (regcache, E_E7_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_E7));
    break;

    /* ssp, msp, and usp are inaccessible.  */
  case E_E8_REGNUM:
    memset (zerobuf, 0, MAX_REGISTER_SIZE);
    regcache_raw_supply (regcache, E_E8_REGNUM, zerobuf);
    break;
  case E_E9_REGNUM:
    memset (zerobuf, 0, MAX_REGISTER_SIZE);
    regcache_raw_supply (regcache, E_E9_REGNUM, zerobuf);
    break;
  case E_E10_REGNUM:
    memset (zerobuf, 0, MAX_REGISTER_SIZE);
    regcache_raw_supply (regcache, E_E10_REGNUM, zerobuf);

    break;
  case E_MCRH_REGNUM:
    regcache_raw_supply (regcache, E_MCRH_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_MCRH));
    break;
  case E_MCRL_REGNUM:
    regcache_raw_supply (regcache, E_MCRL_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_MCRL));
    break;
  case E_MCVF_REGNUM:
    regcache_raw_supply (regcache, E_MCVF_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_MCVF));
    break;
  case E_FPCR_REGNUM:
    /* FPCR is numbered among the GP regs, but handled as an FP reg.
       Do nothing.  */
    break;
  case E_FPCR_REGNUM + 1:
    /* The two unused registers beyond fpcr are inaccessible.  */
    memset (zerobuf, 0, MAX_REGISTER_SIZE);
    regcache_raw_supply (regcache, E_FPCR_REGNUM + 1, zerobuf);
    break;
  case E_FPCR_REGNUM + 2:
    memset (zerobuf, 0, MAX_REGISTER_SIZE);
    regcache_raw_supply (regcache, E_FPCR_REGNUM + 2, zerobuf);
    break;
  default:	/* An error, obviously, but should we error out?  */
    break;
  case -1:
    for (i = 0; i < MN10300_ELF_NGREG; i++)
      am33_supply_gregset_method (regset, regcache, i, gregs, len);
    break;
  }
  return;
}

/* Copy fp register value of REGNUM from regset to regcache.  
   If REGNUM is -1, do this for all fp registers in regset. */

static void
am33_supply_fpregset_method (const struct regset *regset, 
			     struct regcache *regcache, 
			     int regnum, const void *fpregs, size_t len)
{
  const mn10300_elf_fpregset_t *fpregset = fpregs;

  gdb_assert (len == sizeof (mn10300_elf_fpregset_t));

  if (regnum == -1)
    {
      int i;

      for (i = 0; i < MN10300_ELF_NFPREG; i++)
	am33_supply_fpregset_method (regset, regcache,
	                             E_FS0_REGNUM + i, fpregs, len);
      am33_supply_fpregset_method (regset, regcache, 
				   E_FPCR_REGNUM, fpregs, len);
    }
  else if (regnum == E_FPCR_REGNUM)
    regcache_raw_supply (regcache, E_FPCR_REGNUM, 
			 &fpregset->fpcr);
  else if (E_FS0_REGNUM <= regnum && regnum < E_FS0_REGNUM + MN10300_ELF_NFPREG)
    regcache_raw_supply (regcache, regnum, 
			 &fpregset->fpregs[regnum - E_FS0_REGNUM]);

  return;
}

/* Copy register values from regcache to regset.  */

static void
am33_collect_gregset_method (const struct regset *regset, 
			     const struct regcache *regcache, 
			     int regnum, void *gregs, size_t len)
{
  mn10300_elf_gregset_t *regp = gregs;
  int i;

  gdb_assert (len == sizeof (mn10300_elf_gregset_t));

  switch (regnum) {
  case E_D0_REGNUM:
    regcache_raw_collect (regcache, E_D0_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_D0));
    break;
  case E_D1_REGNUM:
    regcache_raw_collect (regcache, E_D1_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_D1));
    break;
  case E_D2_REGNUM:
    regcache_raw_collect (regcache, E_D2_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_D2));
    break;
  case E_D3_REGNUM:
    regcache_raw_collect (regcache, E_D3_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_D3));
    break;
  case E_A0_REGNUM:
    regcache_raw_collect (regcache, E_A0_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_A0));
    break;
  case E_A1_REGNUM:
    regcache_raw_collect (regcache, E_A1_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_A1));
    break;
  case E_A2_REGNUM:
    regcache_raw_collect (regcache, E_A2_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_A2));
    break;
  case E_A3_REGNUM:
    regcache_raw_collect (regcache, E_A3_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_A3));
    break;
  case E_SP_REGNUM:
    regcache_raw_collect (regcache, E_SP_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_SP));
    break;
  case E_PC_REGNUM:
    regcache_raw_collect (regcache, E_PC_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_PC));
    break;
  case E_MDR_REGNUM:
    regcache_raw_collect (regcache, E_MDR_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_MDR));
    break;
  case E_PSW_REGNUM:
    regcache_raw_collect (regcache, E_PSW_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_EPSW));
    break;
  case E_LIR_REGNUM:
    regcache_raw_collect (regcache, E_LIR_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_LIR));
    break;
  case E_LAR_REGNUM:
    regcache_raw_collect (regcache, E_LAR_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_LAR));
    break;
  case E_MDRQ_REGNUM:
    regcache_raw_collect (regcache, E_MDRQ_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_MDRQ));
    break;
  case E_E0_REGNUM:
    regcache_raw_collect (regcache, E_E0_REGNUM,   
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_E0));
    break;
  case E_E1_REGNUM:
    regcache_raw_collect (regcache, E_E1_REGNUM,
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_E1));
    break;
  case E_E2_REGNUM:
    regcache_raw_collect (regcache, E_E2_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_E2));
    break;
  case E_E3_REGNUM:
    regcache_raw_collect (regcache, E_E3_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_E3));
    break;
  case E_E4_REGNUM:
    regcache_raw_collect (regcache, E_E4_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_E4));
    break;
  case E_E5_REGNUM:
    regcache_raw_collect (regcache, E_E5_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_E5));
    break;
  case E_E6_REGNUM:
    regcache_raw_collect (regcache, E_E6_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_E6));
    break;
  case E_E7_REGNUM:
    regcache_raw_collect (regcache, E_E7_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_E7));
    break;

    /* ssp, msp, and usp are inaccessible.  */
  case E_E8_REGNUM:
    /* The gregset struct has noplace to put this: do nothing.  */
    break;
  case E_E9_REGNUM:
    /* The gregset struct has noplace to put this: do nothing.  */
    break;
  case E_E10_REGNUM:
    /* The gregset struct has noplace to put this: do nothing.  */
    break;
  case E_MCRH_REGNUM:
    regcache_raw_collect (regcache, E_MCRH_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_MCRH));
    break;
  case E_MCRL_REGNUM:
    regcache_raw_collect (regcache, E_MCRL_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_MCRL));
    break;
  case E_MCVF_REGNUM:
    regcache_raw_collect (regcache, E_MCVF_REGNUM, 
			 (regp + MN10300_ELF_GREGSET_T_REG_INDEX_MCVF));
    break;
  case E_FPCR_REGNUM:
    /* FPCR is numbered among the GP regs, but handled as an FP reg.
       Do nothing.  */
    break;
  case E_FPCR_REGNUM + 1:
    /* The gregset struct has noplace to put this: do nothing.  */
    break;
  case E_FPCR_REGNUM + 2:
    /* The gregset struct has noplace to put this: do nothing.  */
    break;
  default:	/* An error, obviously, but should we error out?  */
    break;
  case -1:
    for (i = 0; i < MN10300_ELF_NGREG; i++)
      am33_collect_gregset_method (regset, regcache, i, gregs, len);
    break;
  }
  return;
}

/* Copy fp register values from regcache to regset.  */

static void
am33_collect_fpregset_method (const struct regset *regset, 
			      const struct regcache *regcache, 
			      int regnum, void *fpregs, size_t len)
{
  mn10300_elf_fpregset_t *fpregset = fpregs;

  gdb_assert (len == sizeof (mn10300_elf_fpregset_t));

  if (regnum == -1)
    {
      int i;
      for (i = 0; i < MN10300_ELF_NFPREG; i++)
	am33_collect_fpregset_method (regset, regcache, E_FS0_REGNUM + i,
	                              fpregs, len);
      am33_collect_fpregset_method (regset, regcache, 
				    E_FPCR_REGNUM, fpregs, len);
    }
  else if (regnum == E_FPCR_REGNUM)
    regcache_raw_collect (regcache, E_FPCR_REGNUM, 
			  &fpregset->fpcr);
  else if (E_FS0_REGNUM <= regnum
           && regnum < E_FS0_REGNUM + MN10300_ELF_NFPREG)
    regcache_raw_collect (regcache, regnum, 
			  &fpregset->fpregs[regnum - E_FS0_REGNUM]);

  return;
}

/* Create a struct regset from a corefile register section.  */

static const struct regset *
am33_regset_from_core_section (struct gdbarch *gdbarch, 
			       const char *sect_name, 
			       size_t sect_size)
{
  /* We will call regset_alloc, and pass the names of the supply and
     collect methods.  */

  if (sect_size == sizeof (mn10300_elf_fpregset_t))
    return regset_alloc (gdbarch, 
			 am33_supply_fpregset_method,
			 am33_collect_fpregset_method);
  else
    return regset_alloc (gdbarch, 
			 am33_supply_gregset_method,
			 am33_collect_gregset_method);
}

static void
am33_linux_sigframe_cache_init (const struct tramp_frame *self,
                                struct frame_info *next_frame,
			        struct trad_frame_cache *this_cache,
			        CORE_ADDR func);

static const struct tramp_frame am33_linux_sigframe = {
  SIGTRAMP_FRAME,
  1,
  {
    /* mov     119,d0 */
    { 0x2c, -1 },
    { 0x77, -1 },
    { 0x00, -1 },
    /* syscall 0 */
    { 0xf0, -1 },
    { 0xe0, -1 },
    { TRAMP_SENTINEL_INSN, -1 }
  },
  am33_linux_sigframe_cache_init
};

static const struct tramp_frame am33_linux_rt_sigframe = {
  SIGTRAMP_FRAME,
  1,
  {
    /* mov     173,d0 */
    { 0x2c, -1 },
    { 0xad, -1 },
    { 0x00, -1 },
    /* syscall 0 */
    { 0xf0, -1 },
    { 0xe0, -1 },
    { TRAMP_SENTINEL_INSN, -1 }
  },
  am33_linux_sigframe_cache_init
};

/* Relevant struct definitions for signal handling...

From arch/mn10300/kernel/sigframe.h:

struct sigframe
{
	void (*pretcode)(void);
	int sig;
	struct sigcontext sc;
	struct fpucontext fpuctx;
	unsigned long extramask[_NSIG_WORDS-1];
	char retcode[8];
};

struct rt_sigframe
{
	void (*pretcode)(void);
	int sig;
	struct siginfo *pinfo;
	void *puc;
	struct siginfo info;
	struct ucontext uc;
	struct fpucontext fpuctx;
	char retcode[8];
};

From include/asm-mn10300/ucontext.h:

struct ucontext {
	unsigned long	  uc_flags;
	struct ucontext  *uc_link;
	stack_t		  uc_stack;
	struct sigcontext uc_mcontext;
	sigset_t	  uc_sigmask;
};

From include/asm-mn10300/sigcontext.h:

struct fpucontext {
	unsigned long	fs[32];
	unsigned long	fpcr;
};

struct sigcontext {
	unsigned long	d0;
	unsigned long	d1;
	unsigned long	d2;
	unsigned long	d3;
	unsigned long	a0;
	unsigned long	a1;
	unsigned long	a2;
	unsigned long	a3;
	unsigned long	e0;
	unsigned long	e1;
	unsigned long	e2;
	unsigned long	e3;
	unsigned long	e4;
	unsigned long	e5;
	unsigned long	e6;
	unsigned long	e7;
	unsigned long	lar;
	unsigned long	lir;
	unsigned long	mdr;
	unsigned long	mcvf;
	unsigned long	mcrl;
	unsigned long	mcrh;
	unsigned long	mdrq;
	unsigned long	sp;
	unsigned long	epsw;
	unsigned long	pc;
	struct fpucontext *fpucontext;
	unsigned long	oldmask;
}; */


#define AM33_SIGCONTEXT_D0 0
#define AM33_SIGCONTEXT_D1 4
#define AM33_SIGCONTEXT_D2 8
#define AM33_SIGCONTEXT_D3 12
#define AM33_SIGCONTEXT_A0 16
#define AM33_SIGCONTEXT_A1 20
#define AM33_SIGCONTEXT_A2 24
#define AM33_SIGCONTEXT_A3 28
#define AM33_SIGCONTEXT_E0 32
#define AM33_SIGCONTEXT_E1 36
#define AM33_SIGCONTEXT_E2 40
#define AM33_SIGCONTEXT_E3 44
#define AM33_SIGCONTEXT_E4 48
#define AM33_SIGCONTEXT_E5 52
#define AM33_SIGCONTEXT_E6 56
#define AM33_SIGCONTEXT_E7 60
#define AM33_SIGCONTEXT_LAR 64
#define AM33_SIGCONTEXT_LIR 68
#define AM33_SIGCONTEXT_MDR 72
#define AM33_SIGCONTEXT_MCVF 76
#define AM33_SIGCONTEXT_MCRL 80
#define AM33_SIGCONTEXT_MCRH 84
#define AM33_SIGCONTEXT_MDRQ 88
#define AM33_SIGCONTEXT_SP 92
#define AM33_SIGCONTEXT_EPSW 96
#define AM33_SIGCONTEXT_PC 100
#define AM33_SIGCONTEXT_FPUCONTEXT 104


static void
am33_linux_sigframe_cache_init (const struct tramp_frame *self,
                                struct frame_info *next_frame,
			        struct trad_frame_cache *this_cache,
			        CORE_ADDR func)
{
  CORE_ADDR sc_base, fpubase;
  int i;

  sc_base = frame_unwind_register_unsigned (next_frame, E_SP_REGNUM);
  if (self == &am33_linux_sigframe)
    {
      sc_base += 8;
    }
  else
    {
      sc_base += 12;
      sc_base = get_frame_memory_unsigned (next_frame, sc_base, 4);
      sc_base += 20;
    }

  trad_frame_set_reg_addr (this_cache, E_D0_REGNUM,
                           sc_base + AM33_SIGCONTEXT_D0);
  trad_frame_set_reg_addr (this_cache, E_D1_REGNUM,
                           sc_base + AM33_SIGCONTEXT_D1);
  trad_frame_set_reg_addr (this_cache, E_D2_REGNUM,
                           sc_base + AM33_SIGCONTEXT_D2);
  trad_frame_set_reg_addr (this_cache, E_D3_REGNUM,
                           sc_base + AM33_SIGCONTEXT_D3);

  trad_frame_set_reg_addr (this_cache, E_A0_REGNUM,
                           sc_base + AM33_SIGCONTEXT_A0);
  trad_frame_set_reg_addr (this_cache, E_A1_REGNUM,
                           sc_base + AM33_SIGCONTEXT_A1);
  trad_frame_set_reg_addr (this_cache, E_A2_REGNUM,
                           sc_base + AM33_SIGCONTEXT_A2);
  trad_frame_set_reg_addr (this_cache, E_A3_REGNUM,
                           sc_base + AM33_SIGCONTEXT_A3);

  trad_frame_set_reg_addr (this_cache, E_E0_REGNUM,
                           sc_base + AM33_SIGCONTEXT_E0);
  trad_frame_set_reg_addr (this_cache, E_E1_REGNUM,
                           sc_base + AM33_SIGCONTEXT_E1);
  trad_frame_set_reg_addr (this_cache, E_E2_REGNUM,
                           sc_base + AM33_SIGCONTEXT_E2);
  trad_frame_set_reg_addr (this_cache, E_E3_REGNUM,
                           sc_base + AM33_SIGCONTEXT_E3);
  trad_frame_set_reg_addr (this_cache, E_E4_REGNUM,
                           sc_base + AM33_SIGCONTEXT_E4);
  trad_frame_set_reg_addr (this_cache, E_E5_REGNUM,
                           sc_base + AM33_SIGCONTEXT_E5);
  trad_frame_set_reg_addr (this_cache, E_E6_REGNUM,
                           sc_base + AM33_SIGCONTEXT_E6);
  trad_frame_set_reg_addr (this_cache, E_E7_REGNUM,
                           sc_base + AM33_SIGCONTEXT_E7);

  trad_frame_set_reg_addr (this_cache, E_LAR_REGNUM,
                           sc_base + AM33_SIGCONTEXT_LAR);
  trad_frame_set_reg_addr (this_cache, E_LIR_REGNUM,
                           sc_base + AM33_SIGCONTEXT_LIR);
  trad_frame_set_reg_addr (this_cache, E_MDR_REGNUM,
                           sc_base + AM33_SIGCONTEXT_MDR);
  trad_frame_set_reg_addr (this_cache, E_MCVF_REGNUM,
                           sc_base + AM33_SIGCONTEXT_MCVF);
  trad_frame_set_reg_addr (this_cache, E_MCRL_REGNUM,
                           sc_base + AM33_SIGCONTEXT_MCRL);
  trad_frame_set_reg_addr (this_cache, E_MDRQ_REGNUM,
                           sc_base + AM33_SIGCONTEXT_MDRQ);

  trad_frame_set_reg_addr (this_cache, E_SP_REGNUM,
                           sc_base + AM33_SIGCONTEXT_SP);
  trad_frame_set_reg_addr (this_cache, E_PSW_REGNUM,
                           sc_base + AM33_SIGCONTEXT_EPSW);
  trad_frame_set_reg_addr (this_cache, E_PC_REGNUM,
                           sc_base + AM33_SIGCONTEXT_PC);

  fpubase = get_frame_memory_unsigned (next_frame,
                                       sc_base + AM33_SIGCONTEXT_FPUCONTEXT, 4);
  if (fpubase)
    {
      for (i = 0; i < 32; i++)
	{
	  trad_frame_set_reg_addr (this_cache, E_FS0_REGNUM + i,
	                           fpubase + 4 * i);
	}
      trad_frame_set_reg_addr (this_cache, E_FPCR_REGNUM, fpubase + 4 * 32);
    }

  trad_frame_set_id (this_cache, frame_id_build (sc_base, func));
}

/* AM33 GNU/Linux osabi has been recognized.
   Now's our chance to register our corefile handling.  */

static void
am33_linux_init_osabi (struct gdbarch_info gdbinfo, struct gdbarch *gdbarch)
{
  set_gdbarch_regset_from_core_section (gdbarch, 
					am33_regset_from_core_section);
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);

  tramp_frame_prepend_unwinder (gdbarch, &am33_linux_sigframe);
  tramp_frame_prepend_unwinder (gdbarch, &am33_linux_rt_sigframe);
}

void
_initialize_mn10300_linux_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_mn10300, 0,
			  GDB_OSABI_LINUX, am33_linux_init_osabi);
}

