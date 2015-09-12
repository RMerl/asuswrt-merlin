/*
 * BCM5301X Denali DDR2/DDR3 memory controlers.
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

#ifndef	_DDRC_H
#define	_DDRC_H

#ifndef PAD
#define	_PADLINE(line)	pad ## line
#define	_XSTR(line)	_PADLINE(line)
#define	PAD		_XSTR(__LINE__)
#endif	/* PAD */

#ifdef _LANGUAGE_ASSEMBLY

#define	DDRC_CONTROL00		0x000
#define	DDRC_CONTROL01		0x004
#define	DDRC_CONTROL02		0x008
#define	DDRC_CONTROL03		0x00c
#define	DDRC_CONTROL04		0x010
#define	DDRC_CONTROL05		0x014
#define	DDRC_CONTROL06		0x018
#define	DDRC_CONTROL07		0x01c
#define	DDRC_CONTROL08		0x020
#define	DDRC_CONTROL09		0x024
#define	DDRC_CONTROL10		0x028
#define	DDRC_CONTROL11		0x02c
#define	DDRC_CONTROL12		0x030
#define	DDRC_CONTROL13		0x034
#define	DDRC_CONTROL14		0x038
#define	DDRC_CONTROL15		0x03c
#define	DDRC_CONTROL16		0x040
#define	DDRC_CONTROL17		0x044
#define	DDRC_CONTROL18		0x048
#define	DDRC_CONTROL19		0x04c
#define	DDRC_CONTROL20		0x050
#define	DDRC_CONTROL21		0x054
#define	DDRC_CONTROL22		0x058
#define	DDRC_CONTROL23		0x05c
#define	DDRC_CONTROL24		0x060
#define	DDRC_CONTROL25		0x064
#define	DDRC_CONTROL26		0x068
#define	DDRC_CONTROL27		0x06c
#define	DDRC_CONTROL28		0x070
#define	DDRC_CONTROL29		0x074
#define	DDRC_CONTROL30		0x078
#define	DDRC_CONTROL31		0x07c
#define	DDRC_CONTROL32		0x080
#define	DDRC_CONTROL33		0x084
#define	DDRC_CONTROL34		0x088
#define	DDRC_CONTROL35		0x08c
#define	DDRC_CONTROL36		0x090
#define	DDRC_CONTROL37		0x094
#define	DDRC_CONTROL38		0x098
#define	DDRC_CONTROL39		0x09c
#define	DDRC_CONTROL40		0x0a0
#define	DDRC_CONTROL41		0x0a4
#define	DDRC_CONTROL42		0x0a8
#define	DDRC_CONTROL43		0x0ac
#define	DDRC_CONTROL44		0x0b0
#define	DDRC_CONTROL45		0x0b4
#define	DDRC_CONTROL46		0x0b8
#define	DDRC_CONTROL47		0x0bc
#define	DDRC_CONTROL48		0x0c0
#define	DDRC_CONTROL49		0x0c4
#define	DDRC_CONTROL50		0x0c8
#define	DDRC_CONTROL51		0x0cc
#define	DDRC_CONTROL52		0x0d0
#define	DDRC_CONTROL53		0x0d4
#define	DDRC_CONTROL54		0x0d8
#define	DDRC_CONTROL55		0x0dc
#define	DDRC_CONTROL56		0x0e0
#define	DDRC_CONTROL57		0x0e4
#define	DDRC_CONTROL58		0x0e8
#define	DDRC_CONTROL59		0x0ec
#define	DDRC_CONTROL60		0x0f0
#define	DDRC_CONTROL61		0x0f4
#define	DDRC_CONTROL62		0x0f8
#define	DDRC_CONTROL63		0x0fc
#define	DDRC_CONTROL64		0x100
#define	DDRC_CONTROL65		0x104
#define	DDRC_CONTROL66		0x108
#define	DDRC_CONTROL67		0x10c
#define	DDRC_CONTROL68		0x110
#define	DDRC_CONTROL69		0x114
#define	DDRC_CONTROL70		0x118
#define	DDRC_CONTROL71		0x11c
#define	DDRC_CONTROL72		0x120
#define	DDRC_CONTROL73		0x124
#define	DDRC_CONTROL74		0x128
#define	DDRC_CONTROL75		0x12c
#define	DDRC_CONTROL76		0x130
#define	DDRC_CONTROL77		0x134
#define	DDRC_CONTROL78		0x138
#define	DDRC_CONTROL79		0x13c
#define	DDRC_CONTROL80		0x140
#define	DDRC_CONTROL81		0x144
#define	DDRC_CONTROL82		0x148
#define	DDRC_CONTROL83		0x14c
#define	DDRC_CONTROL84		0x150
#define	DDRC_CONTROL85		0x154
#define	DDRC_CONTROL86		0x158
#define	DDRC_CONTROL87		0x15c
#define	DDRC_CONTROL88		0x160
#define	DDRC_CONTROL89		0x164
#define	DDRC_CONTROL90		0x168
#define	DDRC_CONTROL91		0x16c
#define	DDRC_CONTROL92		0x170
#define	DDRC_CONTROL93		0x174
#define	DDRC_CONTROL94		0x178
#define	DDRC_CONTROL95		0x17c
#define	DDRC_CONTROL96		0x180
#define	DDRC_CONTROL97		0x184
#define	DDRC_CONTROL98		0x188
#define	DDRC_CONTROL99		0x18c
#define	DDRC_CONTROL100		0x190
#define	DDRC_CONTROL101		0x194
#define	DDRC_CONTROL102		0x198
#define	DDRC_CONTROL103		0x19c
#define	DDRC_CONTROL104		0x1a0
#define	DDRC_CONTROL105		0x1a4
#define	DDRC_CONTROL106		0x1a8
#define	DDRC_CONTROL107		0x1ac
#define	DDRC_CONTROL108		0x1b0
#define	DDRC_CONTROL109		0x1b4
#define	DDRC_CONTROL110		0x1b8
#define	DDRC_CONTROL111		0x1bc
#define	DDRC_CONTROL112		0x1c0
#define	DDRC_CONTROL113		0x1c4
#define	DDRC_CONTROL114		0x1c8
#define	DDRC_CONTROL115		0x1cc
#define	DDRC_CONTROL116		0x1d0
#define	DDRC_CONTROL117		0x1d4
#define	DDRC_CONTROL118		0x1d8
#define	DDRC_CONTROL119		0x1dc
#define	DDRC_CONTROL120		0x1e0
#define	DDRC_CONTROL121		0x1e4
#define	DDRC_CONTROL122		0x1e8
#define	DDRC_CONTROL123		0x1ec
#define	DDRC_CONTROL124		0x1f0
#define DDRC_CONTROL125		0x1f4
#define DDRC_CONTROL126		0x1f8
#define DDRC_CONTROL127		0x1fc
#define DDRC_CONTROL128		0x200
#define DDRC_CONTROL129		0x204
#define DDRC_CONTROL130		0x208
#define DDRC_CONTROL131		0x20c
#define DDRC_CONTROL132		0x210
#define DDRC_CONTROL133		0x214
#define DDRC_CONTROL134		0x218
#define DDRC_CONTROL135		0x21c
#define DDRC_CONTROL136		0x220
#define DDRC_CONTROL137		0x224
#define DDRC_CONTROL138		0x228
#define DDRC_CONTROL139		0x22c
#define DDRC_CONTROL140		0x230
#define DDRC_CONTROL141		0x234
#define DDRC_CONTROL142		0x238
#define DDRC_CONTROL143		0x23c
#define DDRC_CONTROL144		0x240
#define DDRC_CONTROL145		0x244
#define DDRC_CONTROL146		0x248
#define DDRC_CONTROL147		0x24c
#define DDRC_CONTROL148		0x250
#define DDRC_CONTROL149		0x254
#define DDRC_CONTROL150		0x258
#define DDRC_CONTROL151		0x25c
#define DDRC_CONTROL152		0x260
#define DDRC_CONTROL153		0x264
#define DDRC_CONTROL154		0x268
#define DDRC_CONTROL155		0x26c
#define DDRC_CONTROL156		0x270
#define DDRC_CONTROL157		0x274
#define DDRC_CONTROL158		0x278
#define DDRC_CONTROL159		0x27c
#define DDRC_CONTROL160		0x280
#define DDRC_CONTROL161		0x284
#define DDRC_CONTROL162		0x288
#define DDRC_CONTROL163		0x28c
#define DDRC_CONTROL164		0x290
#define DDRC_CONTROL165		0x294
#define DDRC_CONTROL166		0x298
#define DDRC_CONTROL167		0x29c
#define DDRC_CONTROL168		0x2a0
#define DDRC_CONTROL169		0x2a4
#define DDRC_CONTROL170		0x2a8
#define DDRC_CONTROL171		0x2ac
#define DDRC_CONTROL172		0x2b0
#define DDRC_CONTROL173		0x2b4
#define DDRC_CONTROL174		0x2b8
#define DDRC_CONTROL175		0x2bc
#define DDRC_CONTROL176		0x2c0
#define DDRC_CONTROL177		0x2c4
#define DDRC_CONTROL178		0x2c8
#define DDRC_CONTROL179		0x2cc
#define DDRC_CONTROL180		0x2d0
#define DDRC_CONTROL181		0x2d4
#define DDRC_CONTROL182		0x2d8
#define DDRC_CONTROL183		0x2dc
#define DDRC_CONTROL184		0x2e0
#define DDRC_CONTROL185		0x2e4
#define DDRC_CONTROL186		0x2e8
#define DDRC_CONTROL187		0x2ec
#define DDRC_CONTROL188		0x2f0
#define DDRC_CONTROL189		0x2f4
#define DDRC_CONTROL190		0x2f8
#define DDRC_CONTROL191		0x2fc
#define DDRC_CONTROL192		0x300
#define DDRC_CONTROL193		0x304
#define DDRC_CONTROL194		0x308
#define DDRC_CONTROL195		0x30c
#define DDRC_CONTROL196		0x310
#define DDRC_CONTROL197		0x314
#define DDRC_CONTROL198		0x318
#define DDRC_CONTROL199		0x31c
#define DDRC_CONTROL200		0x320
#define DDRC_CONTROL201		0x324
#define DDRC_CONTROL202		0x328
#define DDRC_CONTROL203		0x32c
#define DDRC_CONTROL204		0x330
#define DDRC_CONTROL205		0x334
#define DDRC_CONTROL206		0x338
#define DDRC_CONTROL207		0x33c
#define DDRC_CONTROL208		0x340

#define DDRC_PHY_CONFIG			0x400
#define DDRC_PCONTROL_REV		0x800
#define DDRC_PCONTROL_PM_CONTROL	0x804
#define DDRC_PCONTROL_PLL_STATUS	0x810
#define DDRC_PCONTROL_PLL_CONFIG	0x814
#define DDRC_PCONTROL_PLL_CONTROL	0x818
#define DDRC_PCONTROL_PLL_DIVIDERS	0x81c
#define DDRC_PCONTROL_AUX_CONTROL	0x820

#define DDRC_PCONTROL_ZQ_PVT_COMPCTL	0x83c
#define DDRC_PCONTROL_VDL_CALIBRATE	0x848
#define DDRC_PHY_STRP_STAT		0x8b8
#define DDRC_PHY_LN0_WR_PREMB_MODE	0xbac

#else	/* !_LANGUAGE_ASSEMBLY */

#define DDRC_MAXREG		209

typedef struct ddrcregs {
	uint32	control[DDRC_MAXREG];		/* 0x000 -- 0x340 */
	uint32	PAD[47];
	uint32	phy_config;			/* 0x400 */
	uint32	PAD[255];
	uint32	phy_control_rev;		/* 0x800 */
	uint32	phy_control_pmcontrol;		/* 0x804 */
	uint32	PAD[2];
	uint32	phy_control_pllstatus;		/* 0x810 */
	uint32	phy_control_pllconfig;		/* 0x814 */
	uint32	phy_control_pllcontrol;		/* 0x818 */
	uint32	phy_control_plldividers;	/* 0x81c */
	uint32	phy_control_auxcontrol;		/* 0x820 */
	uint32	PAD[4];
	uint32	phy_control_vdl_ovride_bitctl;	/* 0x834 */
	uint32	PAD[1];
	uint32	phy_control_zq_pvt_compctl;	/* 0x83c */
	uint32	PAD[2];
	uint32	phy_control_vdl_calibrate;	/* 0x848 */
	uint32	phy_control_vdl_calibsts;	/* 0x84c */
	uint32	PAD[26];
	uint32	phy_strp_stat;			/* 0x8b8 */
	uint32	PAD[81];
	uint32	phy_ln0_vdl_ovride_byte_rd_en;		/* 0xa00 */
	uint32	phy_ln0_vdl_ovride_byte0_w;		/* 0xa04 */
	uint32	phy_ln0_vdl_ovride_byte0_r_p;		/* 0xa08 */
	uint32	phy_ln0_vdl_ovride_byte0_r_n;		/* 0xa0c */
	uint32	phy_ln0_vdl_ovride_byte0_bit0_w;	/* 0xa10 */
	uint32	phy_ln0_vdl_ovride_byte0_bit1_w;	/* 0xa14 */
	uint32	phy_ln0_vdl_ovride_byte0_bit2_w;	/* 0xa18 */
	uint32	phy_ln0_vdl_ovride_byte0_bit3_w;	/* 0xa1c */
	uint32	phy_ln0_vdl_ovride_byte0_bit4_w;	/* 0xa20 */
	uint32	phy_ln0_vdl_ovride_byte0_bit5_w;	/* 0xa24 */
	uint32	phy_ln0_vdl_ovride_byte0_bit6_w;	/* 0xa28 */
	uint32	phy_ln0_vdl_ovride_byte0_bit7_w;	/* 0xa2c */
	uint32	phy_ln0_vdl_ovride_byte0_dm_w;		/* 0xa30 */
	uint32	PAD[16];
	uint32	phy_ln0_vdl_ovride_byte0_bit_rd_en;	/* 0xa74 */
	uint32	PAD[11];
	uint32	phy_ln0_vdl_ovride_byte1_w;		/* 0xaa4 */
	uint32	phy_ln0_vdl_ovride_byte1_r_p;		/* 0xaa8 */
	uint32	phy_ln0_vdl_ovride_byte1_r_n;		/* 0xaac */
	uint32	phy_ln0_vdl_ovride_byte1_bit0_w;	/* 0xab0 */
	uint32	phy_ln0_vdl_ovride_byte1_bit1_w;	/* 0xab4 */
	uint32	phy_ln0_vdl_ovride_byte1_bit2_w;	/* 0xab8 */
	uint32	phy_ln0_vdl_ovride_byte1_bit3_w;	/* 0xabc */
	uint32	phy_ln0_vdl_ovride_byte1_bit4_w;	/* 0xac0 */
	uint32	phy_ln0_vdl_ovride_byte1_bit5_w;	/* 0xac4 */
	uint32	phy_ln0_vdl_ovride_byte1_bit6_w;	/* 0xac8 */
	uint32	phy_ln0_vdl_ovride_byte1_bit7_w;	/* 0xacc */
	uint32	phy_ln0_vdl_ovride_byte1_dm_w;		/* 0xad0 */
	uint32	PAD[16];
	uint32	phy_ln0_vdl_ovride_byte1_bit_rd_en;	/* 0xb14 */
	uint32	PAD[18];
	uint32	phy_ln0_rddata_dly;		/* 0xb60 */
	uint32	PAD[18];
	uint32	phy_ln0_wr_premb_mode;		/* 0xbac */
} _ddrcregs_t;

typedef volatile _ddrcregs_t ddrcregs_t;

#endif	/* _LANGUAGE_ASSEMBLY */

#define DDR_TABLE_END		0xffffffff

#define DDRC00_START		0x00000001
#define DDR_INT_INIT_DONE	0x200

#define DDR_PHY_DMP_OFFSET	0x00001000

#define DDR_TYPE_MASK		(0x1 << 0)
#define DDR_STAT_DDR3		0x00000001	/* bit 0 of iostatus */

#define DDR_PHY_CONTROL_REGS_REVISION	0x18010800
#define DDR_S1_IDM_RESET_CONTROL		0x18108800
#define DDR_S1_IDM_IO_CONTROL_DIRECT	0x18108408
#define DDR_S2_IDM_RESET_CONTROL		0x18109800

#endif	/* _DDRC_H */
