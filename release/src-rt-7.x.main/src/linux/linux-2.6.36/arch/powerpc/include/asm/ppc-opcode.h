/*
 * Copyright 2009 Freescale Semicondutor, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * provides masks and opcode images for use by code generation, emulation
 * and for instructions that older assemblers might not know about
 */
#ifndef _ASM_POWERPC_PPC_OPCODE_H
#define _ASM_POWERPC_PPC_OPCODE_H

#include <linux/stringify.h>
#include <asm/asm-compat.h>

/* sorted alphabetically */
#define PPC_INST_DCBA			0x7c0005ec
#define PPC_INST_DCBA_MASK		0xfc0007fe
#define PPC_INST_DCBAL			0x7c2005ec
#define PPC_INST_DCBZL			0x7c2007ec
#define PPC_INST_ISEL			0x7c00001e
#define PPC_INST_ISEL_MASK		0xfc00003e
#define PPC_INST_LDARX			0x7c0000a8
#define PPC_INST_LSWI			0x7c0004aa
#define PPC_INST_LSWX			0x7c00042a
#define PPC_INST_LWARX			0x7c000028
#define PPC_INST_LWSYNC			0x7c2004ac
#define PPC_INST_LXVD2X			0x7c000698
#define PPC_INST_MCRXR			0x7c000400
#define PPC_INST_MCRXR_MASK		0xfc0007fe
#define PPC_INST_MFSPR_PVR		0x7c1f42a6
#define PPC_INST_MFSPR_PVR_MASK		0xfc1fffff
#define PPC_INST_MSGSND			0x7c00019c
#define PPC_INST_NOP			0x60000000
#define PPC_INST_POPCNTB		0x7c0000f4
#define PPC_INST_POPCNTB_MASK		0xfc0007fe
#define PPC_INST_RFCI			0x4c000066
#define PPC_INST_RFDI			0x4c00004e
#define PPC_INST_RFMCI			0x4c00004c

#define PPC_INST_STRING			0x7c00042a
#define PPC_INST_STRING_MASK		0xfc0007fe
#define PPC_INST_STRING_GEN_MASK	0xfc00067e

#define PPC_INST_STSWI			0x7c0005aa
#define PPC_INST_STSWX			0x7c00052a
#define PPC_INST_STXVD2X		0x7c000798
#define PPC_INST_TLBIE			0x7c000264
#define PPC_INST_TLBILX			0x7c000024
#define PPC_INST_WAIT			0x7c00007c
#define PPC_INST_TLBIVAX		0x7c000624
#define PPC_INST_TLBSRX_DOT		0x7c0006a5
#define PPC_INST_XXLOR			0xf0000510

/* macros to insert fields into opcodes */
#define __PPC_RA(a)	(((a) & 0x1f) << 16)
#define __PPC_RB(b)	(((b) & 0x1f) << 11)
#define __PPC_RS(s)	(((s) & 0x1f) << 21)
#define __PPC_RT(s)	__PPC_RS(s)
#define __PPC_XA(a)	((((a) & 0x1f) << 16) | (((a) & 0x20) >> 3))
#define __PPC_XB(b)	((((b) & 0x1f) << 11) | (((b) & 0x20) >> 4))
#define __PPC_XS(s)	((((s) & 0x1f) << 21) | (((s) & 0x20) >> 5))
#define __PPC_XT(s)	__PPC_XS(s)
#define __PPC_T_TLB(t)	(((t) & 0x3) << 21)
#define __PPC_WC(w)	(((w) & 0x3) << 21)
/*
 * Only use the larx hint bit on 64bit CPUs. e500v1/v2 based CPUs will treat a
 * larx with EH set as an illegal instruction.
 */
#ifdef CONFIG_PPC64
#define __PPC_EH(eh)	(((eh) & 0x1) << 0)
#else
#define __PPC_EH(eh)	0
#endif

/* Deal with instructions that older assemblers aren't aware of */
#define	PPC_DCBAL(a, b)		stringify_in_c(.long PPC_INST_DCBAL | \
					__PPC_RA(a) | __PPC_RB(b))
#define	PPC_DCBZL(a, b)		stringify_in_c(.long PPC_INST_DCBZL | \
					__PPC_RA(a) | __PPC_RB(b))
#define PPC_LDARX(t, a, b, eh)	stringify_in_c(.long PPC_INST_LDARX | \
					__PPC_RT(t) | __PPC_RA(a) | \
					__PPC_RB(b) | __PPC_EH(eh))
#define PPC_LWARX(t, a, b, eh)	stringify_in_c(.long PPC_INST_LWARX | \
					__PPC_RT(t) | __PPC_RA(a) | \
					__PPC_RB(b) | __PPC_EH(eh))
#define PPC_MSGSND(b)		stringify_in_c(.long PPC_INST_MSGSND | \
					__PPC_RB(b))
#define PPC_RFCI		stringify_in_c(.long PPC_INST_RFCI)
#define PPC_RFDI		stringify_in_c(.long PPC_INST_RFDI)
#define PPC_RFMCI		stringify_in_c(.long PPC_INST_RFMCI)
#define PPC_TLBILX(t, a, b)	stringify_in_c(.long PPC_INST_TLBILX | \
					__PPC_T_TLB(t) | __PPC_RA(a) | __PPC_RB(b))
#define PPC_TLBILX_ALL(a, b)	PPC_TLBILX(0, a, b)
#define PPC_TLBILX_PID(a, b)	PPC_TLBILX(1, a, b)
#define PPC_TLBILX_VA(a, b)	PPC_TLBILX(3, a, b)
#define PPC_WAIT(w)		stringify_in_c(.long PPC_INST_WAIT | \
					__PPC_WC(w))
#define PPC_TLBIE(lp,a) 	stringify_in_c(.long PPC_INST_TLBIE | \
					       __PPC_RB(a) | __PPC_RS(lp))
#define PPC_TLBSRX_DOT(a,b)	stringify_in_c(.long PPC_INST_TLBSRX_DOT | \
					__PPC_RA(a) | __PPC_RB(b))
#define PPC_TLBIVAX(a,b)	stringify_in_c(.long PPC_INST_TLBIVAX | \
					__PPC_RA(a) | __PPC_RB(b))

/*
 * Define what the VSX XX1 form instructions will look like, then add
 * the 128 bit load store instructions based on that.
 */
#define VSX_XX1(s, a, b)	(__PPC_XS(s) | __PPC_RA(a) | __PPC_RB(b))
#define VSX_XX3(t, a, b)	(__PPC_XT(t) | __PPC_XA(a) | __PPC_XB(b))
#define STXVD2X(s, a, b)	stringify_in_c(.long PPC_INST_STXVD2X | \
					       VSX_XX1((s), (a), (b)))
#define LXVD2X(s, a, b)		stringify_in_c(.long PPC_INST_LXVD2X | \
					       VSX_XX1((s), (a), (b)))
#define XXLOR(t, a, b)		stringify_in_c(.long PPC_INST_XXLOR | \
					       VSX_XX3((t), (a), (b)))

#endif /* _ASM_POWERPC_PPC_OPCODE_H */
