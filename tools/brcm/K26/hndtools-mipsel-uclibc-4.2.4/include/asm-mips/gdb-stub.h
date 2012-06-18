/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995 Andreas Busse
 * Copyright (C) 2003, 2008 Ralf Baechle (ralf@linux-mips.org)
 * Copyright (C) 2008 Wind River Systems, Inc.
 *   written by Ralf Baechle
 */
#ifndef _ASM_GDB_STUB_H
#define _ASM_GDB_STUB_H

/*
 * GDB interface constants.
 */
#define REG_EPC		37
#define REG_FP		72
#define REG_SP		29

/*
 * Stack layout for the GDB exception handler
 * Derived from the stack layout described in asm-mips/stackframe.h
 */

struct gdb_regs {
#ifdef CONFIG_32BIT
	/*
	 * Pad bytes for argument save space on the stack
	 * 24/48 Bytes for 32/64 bit code
	 */
	unsigned long pad0[6];
#endif

	/*
	 * saved main processor registers
	 */
	long	 reg0,  reg1,  reg2,  reg3,  reg4,  reg5,  reg6,  reg7;
	long	 reg8,  reg9, reg10, reg11, reg12, reg13, reg14, reg15;
	long	reg16, reg17, reg18, reg19, reg20, reg21, reg22, reg23;
	long	reg24, reg25, reg26, reg27, reg28, reg29, reg30, reg31;

	/*
	 * Saved special registers
	 */
	long	cp0_status;
	long	hi;
	long	lo;
#ifdef CONFIG_CPU_HAS_SMARTMIPS
	long	acx;
#endif
	long	cp0_badvaddr;
	long	cp0_cause;
	long	cp0_epc;

	/*
	 * Saved floating point registers
	 */
	long	fpr0,  fpr1,  fpr2,  fpr3,  fpr4,  fpr5,  fpr6,  fpr7;
	long	fpr8,  fpr9,  fpr10, fpr11, fpr12, fpr13, fpr14, fpr15;
	long	fpr16, fpr17, fpr18, fpr19, fpr20, fpr21, fpr22, fpr23;
	long	fpr24, fpr25, fpr26, fpr27, fpr28, fpr29, fpr30, fpr31;

	long	cp1_fsr;
	long	cp1_fir;

	/*
	 * Frame pointer
	 */
	long	frame_ptr;
	long    dummy;		/* unused */

	/*
	 * Saved cp0 registers
	 */
	long	cp0_index;
	long	cp0_random;
	long	cp0_entrylo0;
	long	cp0_entrylo1;
	long	cp0_context;
	long	cp0_pagemask;
	long	cp0_wired;
	long	cp0_reg7;
	long	cp0_reg8;
	long	cp0_reg9;
	long	cp0_entryhi;
	long	cp0_reg11;
	long	cp0_reg12;
	long	cp0_reg13;
	long	cp0_reg14;
	long	cp0_prid;
};

extern int kgdb_enabled;
extern void set_debug_traps(void);
extern void set_async_breakpoint(unsigned long *epc);

#endif /* _ASM_GDB_STUB_H */
