/*
 * BCM947xx ChipcommonB Definitions.
 *
 * $Id$
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

#ifndef	_CHIPCOMMONB_H
#define	_CHIPCOMMONB_H

#ifndef _LANGUAGE_ASSEMBLY

/* cpp contortions to concatenate w/arg prescan */
#ifndef PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif	/* PAD */

typedef volatile struct {
	uint32	PAD[1024];	/* 0x1000 */
	uint32	PAD[1024];
	uint32	PAD[1024];
	uint32	PAD[1024];
	uint32	PAD[1024];
	uint32	PAD[1024];
	uint32	PAD[1024];	/* 0x7000 */
	uint32	PAD[1024];	/* 0x8000 */
	uint32	PAD[1024];
	uint32	PAD[1024];
	uint32	cru_control;	/* 0xb000 */
	uint32	PAD[1023];
	uint32	pcu_mdio_mgt;	/* 0xc000 */
	uint32	pcu_mdio_cmd;	/* 0xc004 */
	uint32	PAD[6];
	uint32	pcu_aopc_control;	/* 0xc020 */
	uint32	pcu_status;		/* 0xc024 */
	uint32	pcu_1v8_vregcntl;	/* 0xc028 */
	uint32	pcu_1v8_1v5_vregcntl;	/* 0xc02c */
	uint32	PAD[52];
	uint32	cru_lcpll_control0;	/* 0xc100 */
	uint32	cru_lcpll_control1;
	uint32	cru_lcpll_control2;
	uint32	cru_lcpll_control3;
	uint32	cru_lcpll_status;
	uint32	PAD[11];
	uint32	cru_genpll_control0;	/* 0xc140 */
	uint32	cru_genpll_control1;
	uint32	cru_genpll_control2;
	uint32	cru_genpll_control3;
	uint32	cru_genpll_control4;
	uint32	cru_genpll_control5;
	uint32	cru_genpll_control6;
	uint32	cru_genpll_control7;
	uint32	cru_genpll_status;
	uint32	PAD[7];
	uint32	cru_clkset_key;		/* 0xc180 */
	uint32	cru_reset;
	uint32	cru_period_sample;
	uint32	cru_interrupt;
	uint32	cru_mdio_control;	/* 0xc190 */
	uint32	PAD[11];
	uint32	cru_gpio_control0;	/* 0xc1c0 */
	uint32	cru_gpio_control1;
	uint32	cru_gpio_control2;
	uint32	cru_gpio_control3;
	uint32	cru_gpio_control4;
	uint32	cru_gpio_control5;
	uint32	cru_gpio_control6;
	uint32	cru_gpio_control7;
	uint32	cru_gpio_control8;

} chipcommonbregs_t;

#endif /* _LANGUAGE_ASSEMBLY */

/* PCU_AOPC_CONTROL */
#define PCU_AOPC_PWRDIS_SEL		(1 << 31)
#define PCU_AOPC_PWRDIS_MASK		(0xf << 1)
#define PCU_AOPC_PWRDIS_WIFI		(1 << 1)
#define PCU_AOPC_PWRDIS_DDR		(1 << 2)
#define PCU_AOPC_PWRDIS_SDIO		(1 << 3)
#define PCU_AOPC_PWRDIS_RGMII		(1 << 4)
#define PCU_AOPC_PWRDIS_LATCHEN		(1 << 0)

/* PCU_STATUS */
#define PCU_PWR_EN_STRAPS_MASK		(0xf << 0)
#define PCU_PWR_EN_STRAPS_WIF1_EN	(1 << 0)
#define PCU_PWR_EN_STRAPS_DDR_EN	(1 << 1)
#define PCU_PWR_EN_STRAPS_SDIO_EN	(1 << 2)
#define PCU_PWR_EN_STRAPS_RGMII_EN	(1 << 3)


/* PCU_1V8_1V5_VREGCNTL */
#define PCU_VOLTAGE_SELECT_MASK		(1 << 9)
#define PCU_VOLTAGE_SELECT_1V8		(0 << 9)
#define PCU_VOLTAGE_SELECT_1V5		(1 << 9)

/* DMU tunings for DDR clocks */
#define CRU_CLKSET_KEY					0x1800c180
#define LCPLL_NDIV_INT					0x1800c104
#define LCPLL_CHX_MDIV					0x1800c108
#define LCPLL_LOAD_EN_CH				0x1800c100

/* SRAB registers */
#define CHIPCB_SRAB_BASE				0x18007000
#define CHIPCB_SRAB_CMDSTAT_OFFSET			0x0000002c
#define CHIPCB_SRAB_RDL_OFFSET				0x0000003c

#define CHIPCB_DMU_BASE					0x1800c000
#define CHIPCB_CRU_STRAPS_CTRL_OFFSET			0x2a0
#define CHIPCB_CRU_STRAPS_4BYTE				(1 << 15)

#endif	/* _CHIPCOMMONB_H */
