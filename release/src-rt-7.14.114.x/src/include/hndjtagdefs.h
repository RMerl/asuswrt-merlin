/*
 * Definitiosn for Jtag taps in HND chips.
 *
 * $Id: hndjtagdefs.h 400891 2013-05-07 23:25:36Z $
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef	_HNDJTAGDEFS_H
#define	_HNDJTAGDEFS_H

/* Jtag access regs are all 32 bits */
#define	JRBITS			32

/* MIPS mode defines: */

#define	MIPS_IR_SIZE		5
#define	MIPS_DR_SIZE		32

#define	MIPS_IDCODE		1
#define	MIPS_BYPASS		0x1f

/* Register addresses */
#define MIPS_ADDR		0x08
#define MIPS_DATA		0x09
#define MIPS_CTRL		0x0a

/* Bits in the Control register */
#define DMA_SZ1			0x00000000
#define DMA_SZ2			0x00000080
#define DMA_SZ4			0x00000100
#define DMA_SZ3			0x00000180
#define DMA_READ		0x00000200
#define DMA_ERROR		0x00000400
#define DMA_START		0x00000800
#define EJ_BREAK		0x00001000
#define EJ_PREN			0x00008000
#define DMA_ACC			0x00020000
#define EJ_PRACC		0x00040000

/* CC mode defines: */

#define	CCJT_IR_SIZE		8
#define	CCJT_DR_SIZE		32

#define	CCJT_USER_BASE		0x20

#define	CCJT_IDCODE		1
#define	CCJT_BYPASS		0xff

/* Register addresses */
#define CHIPC_ADDR		0x30
#define CHIPC_DATA		0x32
#define CHIPC_CTRL		0x34

#define CHIPC_RO		1		/* Or in this to get the read-only address */

/* Control register bits */
#define CCC_BE0			0x00000001
#define CCC_BE1			0x00000002
#define CCC_BE2			0x00000004
#define CCC_BE3			0x00000008
#define CCC_SZ1			(CCC_BE0)
#define CCC_SZ2			(CCC_BE1 | CCC_BE0)
#define CCC_SZ4			(CCC_BE3 | CCC_BE2 | CCC_BE1 | CCC_BE0)
#define CCC_READ		0x00000010
#define CCC_START		0x00000020
#define CCC_ERROR		0x00000040

/* Bits written into the control register need to be shifted */
#define CCC_WR_SHIFT		25

/* LV mode defines: */

#define	LV_IR_SIZE		32
#define	LV_DR_SIZE		32

#define	LV_BASE			0xfe03ff3a
#define	LV_REG_MASK		0x01f00000
#define	LV_REG_SHIFT		20
#define LV_RO			0x00080000
#define	LV_USER_BASE		0x10

/* 38 bits IR, Reg address is in bits 31:27, WriteEnable is in bit 26 */
#define	LV_38_BASE		0x03efff3a
#define	LV_38_REG_MASK		0xf8000000
#define	LV_38_REG_SHIFT		27
#define LV_38_RO		0x04000000

/* Keystone base */
#define	LV_BASE_KY		0xfe07ff3a

/* Register addresses */
#define LV_CAP			0
#define LV_CHAIN_CTL		1
#define LV_ADDR			2
#define LV_ADDRH		3
#define LV_DATA			4
#define LV_CTRL			5
#define LV_OTP_CTL		6
#define LV_OTP_STAT		7
#define LV_ATEWRITE		8
#define LV_ATEREAD		9
#define LV_ATEREADDATA		10
#define LV_ATEWRITENEXT		11

#define	LV_REG_IR(reg)		(LV_BASE | (((reg) << LV_REG_SHIFT) & LV_REG_MASK))
#define	LV_REG_ROIR(reg)	(LV_BASE | LV_RO | (((reg) << LV_REG_SHIFT) & LV_REG_MASK))
#define	LV_UREG_IR(reg)		(LV_BASE | ((((reg) + LV_USER_BASE) << LV_REG_SHIFT) & LV_REG_MASK))
#define	LV_UREG_ROIR(reg)	\
	(LV_BASE | LV_RO | ((((reg) + LV_USER_BASE) << LV_REG_SHIFT) & LV_REG_MASK))

#define	LV_REG_IR_KY(reg)	(LV_BASE_KY | (((reg) << LV_REG_SHIFT) & LV_REG_MASK))

#define	LV_IDCODE		0xfffffffe
#define	LV_BYPASS		0xffffffff

/* Northstar mode */

#define	NS_BASE			0xe06ff33a
#define	NS_REG_MASK		0x1f000000
#define	NS_REG_SHIFT		24
#define NS_RO			0x00800000

/* Northstar-Lite */
#define	NSL_BASE		0xe81bf33a
#define	NSL_REG_MASK	0x07C00000
#define	NSL_REG_SHIFT	22
#define NSL_RO			0x00200000

#define	NS_REG_IR(reg)		(NS_BASE | (((reg) << NS_REG_SHIFT) & NS_REG_MASK))
#define NS_DEVID		0x002BE17F
#define NS_DEVID11		0x002BF17F
#define NS_DEVID12		0x002C017F
#define NS_DEVID13		0x0031E17F
#define NS_DEVID47082	0x0032D17F

#define	IDC_MFG_MASK		0x00000fff
#define	IDC_PART_MASK		0x0ffff000
#define	IDC_PART_SHIFT		12
#define	IDC_REV_MASK		0xf0000000
#define	IDC_REV_SHIFT		28

#define	ATE_BUSY		1
#define	ATE_RERROR		2
#define	ATE_REJECT		0x10
#define	ATE_WERROR		0x20

#define	JEDEC_BRCM		0x17f

#endif	/* _HNDJTAGDEFS_H */
