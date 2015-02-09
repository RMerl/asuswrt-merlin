/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995, 2003 by Ralf Baechle
 * Copyright (C) 1999 Silicon Graphics, Inc.
 */
#ifndef __ASM_BREAK_H
#define __ASM_BREAK_H

/*
 * The following break codes are or were in use for specific purposes in
 * other MIPS operating systems.  Linux/MIPS doesn't use all of them.  The
 * unused ones are here as placeholders; we might encounter them in
 * non-Linux/MIPS object files or make use of them in the future.
 */
#define BRK_USERBP	0	/* User bp (used by debuggers) */
#define BRK_KERNELBP	1	/* Break in the kernel */
#define BRK_ABORT	2	/* Sometimes used by abort(3) to SIGIOT */
#define BRK_BD_TAKEN	3	/* For bd slot emulation - not implemented */
#define BRK_BD_NOTTAKEN	4	/* For bd slot emulation - not implemented */
#define BRK_SSTEPBP	5	/* User bp (used by debuggers) */
#define BRK_OVERFLOW	6	/* Overflow check */
#define BRK_DIVZERO	7	/* Divide by zero check */
#define BRK_RANGE	8	/* Range error check */
#define BRK_STACKOVERFLOW 9	/* For Ada stackchecking */
#define BRK_NORLD	10	/* No rld found - not used by Linux/MIPS */
#define _BRK_THREADBP	11	/* For threads, user bp (used by debuggers) */

/* uMIPS definitions */
#define MM_BRK_BUG	12	/* Used by BUG() */
#define MM_BRK_KDB	13	/* Used in KDB_ENTER() */
#define MM_BRK_MEMU	14	/* Used by FPU emulator */
#define MM_BRK_MULOVF	15	/* Multiply overflow */

/* MIPS32/64 definitions */
#define BRK_BUG		512	/* Used by BUG() */
#define BRK_KDB		513	/* Used in KDB_ENTER() */
#define BRK_MEMU	514	/* Used by FPU emulator */
#define BRK_KPROBE_BP	515	/* Kprobe break */
#define BRK_KPROBE_SSTEPBP 516	/* Kprobe single step software implementation */
#define BRK_MULOVF	1023	/* Multiply overflow */

#endif /* __ASM_BREAK_H */
