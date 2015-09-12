/*
 * Broadcom chipcommon NAND flash interface
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
 *
 * $Id: nflash.h 342936 2012-07-04 16:49:25Z $
 */

#ifndef _nflash_h_
#define _nflash_h_

/* Nand flash AC timing (in unit of ns) for BCM4706 (rev 31) */
#define NFLASH_T_WP					15
#define NFLASH_T_RR					20
#define NFLASH_T_CS					25
#define NFLASH_T_WH					10
#define NFLASH_T_WB					100

#define nflash_ns_to_cycle(ns, clk)	(((ns*1000*clk)/1000000) + 1)

/* Bits definition in nflash_ctrl register for BCM4706 (rev 31) */
#define NFC_OP0						0x000000ff
#define NFC_OP1						0x0000ff00
#define NFC_CMD0					0x00010000
#define NFC_COL						0x00020000
#define NFC_ROW						0x00040000
#define NFC_CMD1W					0x00080000
#define NFC_DREAD					0x00100000
#define NFC_DWRITE					0x00200000
#define NFC_SPECADDR				0x01000000
#define NFC_RDYBUSY					0x04000000
#define NFC_ERROR					0x08000000
#define NFC_BCOUNT					0x30000000
#define NFC_1BYTE					0x00000000
#define NFC_2BYTES					0x10000000
#define NFC_3BYTES					0x20000000
#define NFC_4BYTES					0x30000000
#define NFC_CSA						0x40000000
#define NFC_START					0x80000000

/* nflash control command for BCM4706 */
#define NFCTRL_ID					0x0090
#define NFCTRL_STATUS				0x0070
#define NFCTRL_RESET				0xff00
#define NFCTRL_READ					0x3000
#define NFCTRL_PAGEPROG				0x0080
#define NFCTRL_PROGSTART			0x0010
#define NFCTRL_ERASE				0xd060
#define NFCTRL_RDCOL				0xe005
#define NFCTRL_WRCOL				0x0085
#define NFCTRL_RDCACHERND			0x3100
#define NFCTRL_RDCACHESEQ			0x0031
#define NFCTRL_RDCACHEEND			0x003f
#define NFCTRL_CACHEPROG			0x1580
#define NFCTRL_COPYBACKRD			0x3500
#define NFCTRL_COPYBACKPGM			0x1085

/* Bits definition in nflash_config for BCM4706 (rev 31) */
#define NFCF_DS_MASK				1
#define NFCF_DS_8					0
#define NFCF_DS_16					1
#define NFCF_WE						2
#define NFCF_COLSZ_MASK				0x30
#define NFCF_COLSZ_SHIFT			4
#define NFCF_ROWSZ_MASK				0xc0
#define NFCF_ROWSZ_SHIFT			6

/* Bits definition in nflash_waitcnt0 register for BCM4706 (rev 31) */
#define NFLASH_WAITCOUNT_W0_SHIFT	0
#define NFLASH_WAITCOUNT_W0_MASK	0x3f
#define NFLASH_WAITCOUNT_W1_SHIFT	6
#define NFLASH_WAITCOUNT_W1_MASK	0xfc0
#define NFLASH_WAITCOUNT_W2_SHIFT	12
#define NFLASH_WAITCOUNT_W2_MASK	0x3f000
#define NFLASH_WAITCOUNT_W3_SHIFT	18
#define NFLASH_WAITCOUNT_W3_MASK	0xfc0000
#define NFLASH_WAITCOUNT_W4_SHIFT	24
#define NFLASH_WAITCOUNT_W4_MASK	0x3f000000

/* nand_cmd_start commands */
#define NCMD_NULL			0
#define NCMD_PAGE_RD			1
#define NCMD_SPARE_RD			2
#define NCMD_STATUS_RD			3
#define NCMD_PAGE_PROG			4
#define NCMD_SPARE_PROG			5
#define NCMD_COPY_BACK			6
#define NCMD_ID_RD			7
#define NCMD_BLOCK_ERASE		8
#define NCMD_FLASH_RESET		9
#define NCMD_LOCK			0xa
#define NCMD_LOCK_DOWN			0xb
#define NCMD_UNLOCK			0xc
#define NCMD_LOCK_STATUS		0xd

/* nand_acc_control */
#define	NAC_RD_ECC_EN			0x80000000
#define	NAC_WR_ECC_EN			0x40000000
#define	NAC_RD_ECC_BLK0_EN		0x20000000
#define	NAC_FAST_PGM_RDIN		0x10000000
#define	NAC_RD_ERASED_ECC_EN		0x08000000
#define	NAC_PARTIAL_PAGE_EN		0x04000000
#define	NAC_PAGE_HIT_EN			0x01000000
#define	NAC_ECC_LEVEL0_MASK		0x00f00000
#define	NAC_ECC_LEVEL0_SHIFT		20
#define	NAC_ECC_LEVEL_MASK		0x000f0000
#define	NAC_ECC_LEVEL_SHIFT		16
#define	NAC_SPARE_SIZE0			0x00003f00
#define	NAC_SPARE_SIZE			0x0000003f

/* nand_config */
#define	NCF_CONFIG_LOCK			0x80000000
#define	NCF_BLOCK_SIZE_MASK		0x70000000
#define	NCF_BLOCK_SIZE_SHIFT		28
#define	NCF_DEVICE_SIZE_MASK		0x0f000000
#define	NCF_DEVICE_SIZE_SHIFT		24
#define	NCF_DEVICE_WIDTH		0x00800000
#define	NCF_PAGE_SIZE_MASK		0x00300000
#define	NCF_PAGE_SIZE_SHIFT		20
#define	NCF_FULL_ADDR_BYTES_MASK	0x00070000
#define	NCF_FULL_ADDR_BYTES_SHIFT	16
#define	NCF_COL_ADDR_BYTES_MASK		0x00007000
#define	NCF_COL_ADDR_BYTES_SHIFT	12
#define	NCF_BLK_ADDR_BYTES_MASK		0x00000700
#define	NCF_BLK_ADDR_BYTES_SHIFT	8

/* nand_intfc_status */
#define	NIST_CTRL_READY			0x80000000
#define	NIST_FLASH_READY		0x40000000
#define	NIST_CACHE_VALID		0x20000000
#define	NIST_SPARE_VALID		0x10000000
#define	NIST_ERASED			0x08000000
#define	NIST_STATUS			0x000000ff

#ifndef _LANGUAGE_ASSEMBLY
#include <typedefs.h>
#include <sbchipc.h>
#include <hndnand.h>
#endif /* _LANGUAGE_ASSEMBLY */

#endif /* _nflash_h_ */
