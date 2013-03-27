/* Target-dependent code for the i386.

   Copyright (C) 2001, 2002, 2003, 2004, 2006, 2007
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

#ifndef I386_TDEP_H
#define I386_TDEP_H

struct frame_info;
struct gdbarch;
struct reggroup;
struct regset;
struct regcache;

/* GDB's i386 target supports both the 32-bit Intel Architecture
   (IA-32) and the 64-bit AMD x86-64 architecture.  Internally it uses
   a similar register layout for both.

   - General purpose registers
   - FPU data registers
   - FPU control registers
   - SSE data registers
   - SSE control register

   The general purpose registers for the x86-64 architecture are quite
   different from IA-32.  Therefore, gdbarch_fp0_regnum
   determines the register number at which the FPU data registers
   start.  The number of FPU data and control registers is the same
   for both architectures.  The number of SSE registers however,
   differs and is determined by the num_xmm_regs member of `struct
   gdbarch_tdep'.  */

/* Convention for returning structures.  */

enum struct_return
{
  pcc_struct_return,		/* Return "short" structures in memory.  */
  reg_struct_return		/* Return "short" structures in registers.  */
};

/* i386 architecture specific information.  */
struct gdbarch_tdep
{
  /* General-purpose registers.  */
  struct regset *gregset;
  int *gregset_reg_offset;
  int gregset_num_regs;
  size_t sizeof_gregset;

  /* Floating-point registers.  */
  struct regset *fpregset;
  size_t sizeof_fpregset;

  /* Register number for %st(0).  The register numbers for the other
     registers follow from this one.  Set this to -1 to indicate the
     absence of an FPU.  */
  int st0_regnum;

  /* Register number for %mm0.  Set this to -1 to indicate the absence
     of MMX support.  */
  int mm0_regnum;

  /* Number of SSE registers.  */
  int num_xmm_regs;

  /* Offset of saved PC in jmp_buf.  */
  int jb_pc_offset;

  /* Convention for returning structures.  */
  enum struct_return struct_return;

  /* Address range where sigtramp lives.  */
  CORE_ADDR sigtramp_start;
  CORE_ADDR sigtramp_end;

  /* Detect sigtramp.  */
  int (*sigtramp_p) (struct frame_info *);

  /* Get address of sigcontext for sigtramp.  */
  CORE_ADDR (*sigcontext_addr) (struct frame_info *);

  /* Offset of registers in `struct sigcontext'.  */
  int *sc_reg_offset;
  int sc_num_regs;

  /* Offset of saved PC and SP in `struct sigcontext'.  Usage of these
     is deprecated, please use `sc_reg_offset' instead.  */
  int sc_pc_offset;
  int sc_sp_offset;

  /* ISA-specific data types.  */
  struct type *i386_mmx_type;
  struct type *i386_sse_type;
};

/* Floating-point registers.  */

/* All FPU control regusters (except for FIOFF and FOOFF) are 16-bit
   (at most) in the FPU, but are zero-extended to 32 bits in GDB's
   register cache.  */

/* Return non-zero if REGNUM matches the FP register and the FP
   register set is active.  */
extern int i386_fp_regnum_p (int regnum);
extern int i386_fpc_regnum_p (int regnum);

/* Register numbers of various important registers.  */

enum i386_regnum
{
  I386_EAX_REGNUM,		/* %eax */
  I386_ECX_REGNUM,		/* %ecx */
  I386_EDX_REGNUM,		/* %edx */
  I386_EBX_REGNUM,		/* %ebx */
  I386_ESP_REGNUM,		/* %esp */
  I386_EBP_REGNUM,		/* %ebp */
  I386_ESI_REGNUM,		/* %esi */
  I386_EDI_REGNUM,		/* %edi */
  I386_EIP_REGNUM,		/* %eip */
  I386_EFLAGS_REGNUM,		/* %eflags */
  I386_CS_REGNUM,		/* %cs */
  I386_SS_REGNUM,		/* %ss */
  I386_DS_REGNUM,		/* %ds */
  I386_ES_REGNUM,		/* %es */
  I386_FS_REGNUM,		/* %fs */
  I386_GS_REGNUM,		/* %gs */
  I386_ST0_REGNUM		/* %st(0) */
};

#define I386_NUM_GREGS	16
#define I386_NUM_FREGS	16
#define I386_NUM_XREGS  9

#define I386_SSE_NUM_REGS	(I386_NUM_GREGS + I386_NUM_FREGS \
				 + I386_NUM_XREGS)

/* Size of the largest register.  */
#define I386_MAX_REGISTER_SIZE	16

/* Types for i386-specific registers.  */
extern struct type *i386_eflags_type;
extern struct type *i386_mxcsr_type;

extern struct type *i386_mmx_type (struct gdbarch *gdbarch);
extern struct type *i386_sse_type (struct gdbarch *gdbarch);

/* Segment selectors.  */
#define I386_SEL_RPL	0x0003  /* Requester's Privilege Level mask.  */
#define I386_SEL_UPL	0x0003	/* User Privilige Level. */
#define I386_SEL_KPL	0x0000	/* Kernel Privilige Level. */

/* Functions exported from i386-tdep.c.  */
extern CORE_ADDR i386_pe_skip_trampoline_code (CORE_ADDR pc, char *name);

/* Return the name of register REGNUM.  */
extern char const *i386_register_name (int regnum);

/* Return non-zero if REGNUM is a member of the specified group.  */
extern int i386_register_reggroup_p (struct gdbarch *gdbarch, int regnum,
				     struct reggroup *group);

/* Supply register REGNUM from the general-purpose register set REGSET
   to register cache REGCACHE.  If REGNUM is -1, do this for all
   registers in REGSET.  */
extern void i386_supply_gregset (const struct regset *regset,
				 struct regcache *regcache, int regnum,
				 const void *gregs, size_t len);

/* Collect register REGNUM from the register cache REGCACHE and store
   it in the buffer specified by GREGS and LEN as described by the
   general-purpose register set REGSET.  If REGNUM is -1, do this for
   all registers in REGSET.  */
extern void i386_collect_gregset (const struct regset *regset,
				  const struct regcache *regcache,
				  int regnum, void *gregs, size_t len);

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */
extern const struct regset *
  i386_regset_from_core_section (struct gdbarch *gdbarch,
				 const char *sect_name, size_t sect_size);

/* Initialize a basic ELF architecture variant.  */
extern void i386_elf_init_abi (struct gdbarch_info, struct gdbarch *);

/* Initialize a SVR4 architecture variant.  */
extern void i386_svr4_init_abi (struct gdbarch_info, struct gdbarch *);


/* Functions and variables exported from i386bsd-tdep.c.  */

extern void i386bsd_init_abi (struct gdbarch_info, struct gdbarch *);
extern CORE_ADDR i386fbsd_sigtramp_start_addr;
extern CORE_ADDR i386fbsd_sigtramp_end_addr;
extern CORE_ADDR i386obsd_sigtramp_start_addr;
extern CORE_ADDR i386obsd_sigtramp_end_addr;
extern int i386fbsd4_sc_reg_offset[];
extern int i386fbsd_sc_reg_offset[];
extern int i386nbsd_sc_reg_offset[];
extern int i386obsd_sc_reg_offset[];
extern int i386bsd_sc_reg_offset[];

#endif /* i386-tdep.h */
