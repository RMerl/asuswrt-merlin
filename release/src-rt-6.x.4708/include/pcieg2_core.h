/*
 * BCM4707/8/9 PCIE Gen 2 core definitions
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
 * $Id$
 */
#ifndef _PCIEG2_CORE_H
#define _PCIEG2_CORE_H

#define PAXB_INDR_ADDR		0x120
#define PAXB_INDR_DATA		0x124
#define PAXB_FUNC0_IMAP1	0xc80
#define PAXB_IARR_1_LOWER	0xd08
#define PAXB_IARR_1_UPPER	0xd0c


#define REG_BAR2_CONFIG_F0	0x4e0

/* cpp contortions to concatenate w/arg prescan */
#ifndef PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif

/* PCIE Enumeration space offsets */
#define  PCIE_MAP_CONFIG_OFFSET		0xC00

#if !defined(_LANGUAGE_ASSEMBLY)
/* SB side: PCIE core and host control registers */
typedef struct nspcieregs {
	uint32 clk_control;	/* 0x000 */
	uint32 rc_pm_control;	/* 0x004 */
	uint32 rc_pm_status;	/* 0x008 */
	uint32 ep_pm_control;	/* 0x00C */
	uint32 ep_pm_status;	/* 0x010 */
	uint32 ep_ltr_control;	/* 0x014 */
	uint32 ep_ltr_status;	/* 0x018 */
	uint32 ep_obff_status;	/* 0x01C */
	uint32 pcie_err_status;	/* 0x020 */
	uint32 PAD[55];
	uint32 rc_axi_config;	/* 0x100 */
	uint32 ep_axi_config;	/* 0x104 */
	uint32 rxdebug_status0;	/* 0x108 */
	uint32 rxdebug_control0;	/* 0x10C */
	uint32 PAD[4];

	/* pcie core supports in direct access to config space */
	uint32 configindaddr;	/* pcie config space access: Address field: 0x120 */
	uint32 configinddata;	/* pcie config space access: Data field: 0x124 */

	uint32 PAD[52];
	uint32 cfg_addr;		/* 0x1F8: bus, dev, func, reg */
	uint32 cfg_data;		/* 0x1FC */
	uint32 sys_eq_page;	/* memory page for event queues: 0x200 */
	uint32 sys_msi_page;	/* 0x204 */
	uint32 sys_msi_intren;	/* 0x208 */
	uint32 PAD[1];
	uint32 sys_msi_ctrl0;	/* 0x210 */
	uint32 sys_msi_ctrl1;	/* 0x214 */
	uint32 sys_msi_ctrl2;	/* 0x218 */
	uint32 sys_msi_ctrl3;	/* 0x21C */
	uint32 sys_msi_ctrl4;	/* 0x220 */
	uint32 sys_msi_ctrl5;	/* 0x224 */
	uint32 PAD[10];
	uint32 sys_eq_head0;	/* 0x250 */
	uint32 sys_eq_tail0;	/* 0x254 */
	uint32 sys_eq_head1;	/* 0x258 */
	uint32 sys_eq_tail1;	/* 0x25C */
	uint32 sys_eq_head2;	/* 0x260 */
	uint32 sys_eq_tail2;	/* 0x264 */
	uint32 sys_eq_head3;	/* 0x268 */
	uint32 sys_eq_tail3;	/* 0x26C */
	uint32 sys_eq_head4;	/* 0x270 */
	uint32 sys_eq_tail4;	/* 0x274 */
	uint32 sys_eq_head5;	/* 0x278 */
	uint32 sys_eq_tail5;	/* 0x27C */
	uint32 PAD[44];
	uint32 sys_rc_intx_en;	/* 0x330 */
	uint32 sys_rc_intx_csr;	/* 0x334 */
	uint32 PAD[2];
	uint32 sys_msi_req;	/* 0x340 */
	uint32 sys_host_intr_en;	/* 0x344 */
	uint32 sys_host_intr_csr;	/* 0x348 */
	uint32 PAD[1];
	uint32 sys_host_intr0;	/* 0x350 */
	uint32 sys_host_intr1;	/* 0x354 */
	uint32 sys_host_intr2;	/* 0x358 */
	uint32 sys_host_intr3;	/* 0x35C */
	uint32 sys_ep_int_en0;	/* 0x360 */
	uint32 sys_ep_int_en1;	/* 0x364 */
	uint32 PAD[2];
	uint32 sys_ep_int_csr0;	/* 0x370 */
	uint32 sys_ep_int_csr1;	/* 0x374 */
	uint32 PAD[34];
	uint32 pciecfg[4][64];	/* 0x400 - 0x7FF, PCIE Cfg Space */
	uint16 sprom[64];	/* SPROM shadow Area */
	uint32 PAD[224];
	uint32 func0_imap0_0;	/* 0xC00 */
	uint32 func0_imap0_1;	/* 0xC04 */
	uint32 func0_imap0_2;	/* 0xC08 */
	uint32 func0_imap0_3;	/* 0xC0C */
	uint32 func0_imap0_4;	/* 0xC10 */
	uint32 func0_imap0_5;	/* 0xC14 */
	uint32 func0_imap0_6;	/* 0xC18 */
	uint32 func0_imap0_7;	/* 0xC1C */
	uint32 func1_imap0_0;	/* 0xC20 */
	uint32 func1_imap0_1;	/* 0xC24 */
	uint32 func1_imap0_2;	/* 0xC28 */
	uint32 func1_imap0_3;	/* 0xC2C */
	uint32 func1_imap0_4;	/* 0xC30 */
	uint32 func1_imap0_5;	/* 0xC34 */
	uint32 func1_imap0_6;	/* 0xC38 */
	uint32 func1_imap0_7;	/* 0xC3C */
	uint32 PAD[16];
	uint32 func0_imap1;	/* 0xC80 */
	uint32 PAD[1];
	uint32 func1_imap1;	/* 0xC88 */
	uint32 PAD[13];
	uint32 func0_imap2;	/* 0xCC0 */
	uint32 PAD[1];
	uint32 func1_imap2;	/* 0xCC8 */
	uint32 PAD[13];
	uint32 iarr0_lower;	/* 0xD00 */
	uint32 iarr0_upper;	/* 0xD04 */
	uint32 iarr1_lower;	/* 0xD08 */
	uint32 iarr1_upper;	/* 0xD0C */
	uint32 iarr2_lower;	/* 0xD10 */
	uint32 iarr2_upper;	/* 0xD14 */
	uint32 PAD[2];
	uint32 oarr0;		/* 0xD20 */
	uint32 PAD[1];
	uint32 oarr1;		/* 0xD28 */
	uint32 PAD[1];
	uint32 oarr2;		/* 0xD30 */
	uint32 PAD[3];
	uint32 omap0_lower;	/* 0xD40 */
	uint32 omap0_upper;	/* 0xD44 */
	uint32 omap1_lower;	/* 0xD48 */
	uint32 omap1_upper;	/* 0xD4C */
	uint32 omap2_lower;	/* 0xD50 */
	uint32 omap2_upper;	/* 0xD54 */
	uint32 func1_iarr1_size;	/* 0xD58 */
	uint32 func1_iarr2_size;	/* 0xD5C */
	uint32 PAD[104];
	uint32 mem_control;	/* 0xF00 */
	uint32 mem_ecc_errlog0;	/* 0xF04 */
	uint32 mem_ecc_errlog1;	/* 0xF08 */
	uint32 link_status;	/* 0xF0C */
	uint32 strap_status;	/* 0xF10 */
	uint32 reset_status;	/* 0xF14 */
	uint32 reseten_in_linkdown;	/* 0xF18 */
	uint32 misc_intr_en;	/* 0xF1C */
	uint32 tx_debug_cfg;	/* 0xF20 */
	uint32 misc_config;	/* 0xF24 */
	uint32 misc_status;	/* 0xF28 */
	uint32 PAD[1];
	uint32 intr_en;		/* 0xF30 */
	uint32 intr_clear;	/* 0xF34 */
	uint32 intr_status;	/* 0xF38 */
	uint32 PAD[49];
} nspcieregs_t;

#endif /* !defined(_LANGUAGE_ASSEMBLY) */

#endif /* _PCIEG2_CORE_H */
