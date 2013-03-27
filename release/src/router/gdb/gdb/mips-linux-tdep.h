/* Target-dependent code for GNU/Linux on MIPS processors.

   Copyright 2006, 2007 Free Software Foundation, Inc.

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

/* Copied from <asm/elf.h>.  */
#define ELF_NGREG       45
#define ELF_NFPREG      33

typedef unsigned char mips_elf_greg_t[4];
typedef mips_elf_greg_t mips_elf_gregset_t[ELF_NGREG];

typedef unsigned char mips_elf_fpreg_t[8];
typedef mips_elf_fpreg_t mips_elf_fpregset_t[ELF_NFPREG];

/* 0 - 31 are integer registers, 32 - 63 are fp registers.  */
#define FPR_BASE        32
#define PC              64
#define CAUSE           65
#define BADVADDR        66
#define MMHI            67
#define MMLO            68
#define FPC_CSR         69
#define FPC_EIR         70

#define EF_REG0			6
#define EF_REG31		37
#define EF_LO			38
#define EF_HI			39
#define EF_CP0_EPC		40
#define EF_CP0_BADVADDR		41
#define EF_CP0_STATUS		42
#define EF_CP0_CAUSE		43

#define EF_SIZE			180

void mips_supply_gregset (struct regcache *, const mips_elf_gregset_t *);
void mips_fill_gregset (const struct regcache *, mips_elf_gregset_t *, int);
void mips_supply_fpregset (struct regcache *, const mips_elf_fpregset_t *);
void mips_fill_fpregset (const struct regcache *, mips_elf_fpregset_t *, int);

/* 64-bit support.  */

/* Copied from <asm/elf.h>.  */
#define MIPS64_ELF_NGREG       45
#define MIPS64_ELF_NFPREG      33

typedef unsigned char mips64_elf_greg_t[8];
typedef mips64_elf_greg_t mips64_elf_gregset_t[MIPS64_ELF_NGREG];

typedef unsigned char mips64_elf_fpreg_t[8];
typedef mips64_elf_fpreg_t mips64_elf_fpregset_t[MIPS64_ELF_NFPREG];

/* 0 - 31 are integer registers, 32 - 63 are fp registers.  */
#define MIPS64_FPR_BASE                 32
#define MIPS64_PC                       64
#define MIPS64_CAUSE                    65
#define MIPS64_BADVADDR                 66
#define MIPS64_MMHI                     67
#define MIPS64_MMLO                     68
#define MIPS64_FPC_CSR                  69
#define MIPS64_FPC_EIR                  70

#define MIPS64_EF_REG0			 0
#define MIPS64_EF_REG31			31
#define MIPS64_EF_LO			32
#define MIPS64_EF_HI			33
#define MIPS64_EF_CP0_EPC		34
#define MIPS64_EF_CP0_BADVADDR		35
#define MIPS64_EF_CP0_STATUS		36
#define MIPS64_EF_CP0_CAUSE		37

#define MIPS64_EF_SIZE			304

void mips64_supply_gregset (struct regcache *, const mips64_elf_gregset_t *);
void mips64_fill_gregset (const struct regcache *, mips64_elf_gregset_t *, int);
void mips64_supply_fpregset (struct regcache *, const mips64_elf_fpregset_t *);
void mips64_fill_fpregset (const struct regcache *, mips64_elf_fpregset_t *, int);

enum {
  /* The Linux kernel stores an error code from any interrupted
     syscall in a "register" (in $0's save slot).  */
  MIPS_RESTART_REGNUM = MIPS_LAST_EMBED_REGNUM + 1
};

/* Return 1 if MIPS_RESTART_REGNUM is usable.  */

int mips_linux_restart_reg_p (struct gdbarch *gdbarch);
