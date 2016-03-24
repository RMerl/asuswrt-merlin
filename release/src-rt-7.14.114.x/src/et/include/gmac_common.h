/*
 * gmacdefs - Broadcom gmac (Unimac) specific definitions
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
 * $Id: gmac_common.h 376342 2012-12-24 21:02:49Z $
 */

#ifndef _gmac_common_core_h_
#define _gmac_common_core_h_

#ifndef PAD
#define     _PADLINE(line)    pad ## line
#define     _XSTR(line)     _PADLINE(line)
#define     PAD     XSTR(__LINE__)
#endif

typedef volatile struct _gmac_commonregs {
    uint32 	stag0;
    uint32 	stag1;
    uint32 	stag2;
    uint32 	stag3;
    uint32 	PAD[4];
    uint32 	parsercontrol;
    uint32 	mib_max_len;
    uint32 	PAD[54];
    uint32 	phyaccess;
    uint32 	phycontrol;
    uint32 	PAD[2];
    uint32 	gmac0_rgmii_cntl;
    uint32 	PAD[59];
    uint32 	cfp_access;
    uint32 	PAD[3];
    uint32 	cfp_tcam_data0;
    uint32 	cfp_tcam_data1;
    uint32 	cfp_tcam_data2;
    uint32 	cfp_tcam_data3;
    uint32 	cfp_tcam_data4;
    uint32 	cfp_tcam_data5;
    uint32 	cfp_tcam_data6;
    uint32 	cfp_tcam_data7;
    uint32 	cfp_tcam_mask0;
    uint32 	cfp_tcam_mask1;
    uint32 	cfp_tcam_mask2;
    uint32 	cfp_tcam_mask3;
    uint32 	cfp_tcam_mask4;
    uint32 	cfp_tcam_mask5;
    uint32 	cfp_tcam_mask6;
    uint32 	cfp_tcam_mask7;
    uint32 	cfp_action_data;
    uint32 	PAD[19];
    uint32 	tcam_bist_cntl;
    uint32 	tcam_bist_status;
    uint32 	tcam_cmp_status;
    uint32 	tcam_disable;
    uint32 	PAD[16];
    uint32 	tcam_test_cntl;
    uint32 	PAD[3];
    uint32 	udf_0_a3_a0;
    uint32 	udf_0_a7_a4;
    uint32 	udf_0_a8;
    uint32 	PAD[1];
    uint32 	udf_1_a3_a0;
    uint32 	udf_1_a7_a4;
    uint32 	udf_1_a8;
    uint32 	PAD[1];
    uint32 	udf_2_a3_a0;
    uint32 	udf_2_a7_a4;
    uint32 	udf_2_a8;
    uint32 	PAD[1];
    uint32 	udf_0_b3_b0;
    uint32 	udf_0_b7_b4;
    uint32 	udf_0_b8;
    uint32 	PAD[1];
    uint32 	udf_1_b3_b0;
    uint32 	udf_1_b7_b4;
    uint32 	udf_1_b8;
    uint32 	PAD[1];
    uint32 	udf_2_b3_b0;
    uint32 	udf_2_b7_b4;
    uint32 	udf_2_b8;
    uint32 	PAD[1];
    uint32 	udf_0_c3_c0;
    uint32 	udf_0_c7_c4;
    uint32 	udf_0_c8;
    uint32 	PAD[1];
    uint32 	udf_1_c3_c0;
    uint32 	udf_1_c7_c4;
    uint32 	udf_1_c8;
    uint32 	PAD[1];
    uint32 	udf_2_c3_c0;
    uint32 	udf_2_c7_c4;
    uint32 	udf_2_c8;
    uint32 	PAD[1];
    uint32 	udf_0_d3_d0;
    uint32 	udf_0_d7_d4;
    uint32 	udf_0_d11_d8;
} gmac_commonregs_t;

/*  stag0 offset0x0  */
#define 	STAG0_TPID_SHIFT	0
#define 	STAG0_TPID_MASK	0xffff

/*  stag1 offset0x4  */
#define 	STAG1_TPID_SHIFT	0
#define 	STAG1_TPID_MASK	0xffff

/*  stag2 offset0x8  */
#define 	STAG2_TPID_SHIFT	0
#define 	STAG2_TPID_MASK	0xffff

/*  stag3 offset0xc  */
#define 	STAG3_TPID_SHIFT	0
#define 	STAG3_TPID_MASK	0xffff

/*  parsercontrol offset0x20  */
#define 	PARSERCONTROL_MAX_PARSER_LEN_TH_SHIFT	0
#define 	PARSERCONTROL_MAX_PARSER_LEN_TH_MASK	0x3fff

/*  mib_max_len offset0x24  */
#define 	MIB_MAX_LEN_MIB_MAX_LEN_SHIFT	0
#define 	MIB_MAX_LEN_MIB_MAX_LEN_MASK	0x3fff

/*  phyaccess offset0x100  */
#define 	PHYACCESS_TRIGGER_SHIFT	30
#define 	PHYACCESS_TRIGGER_MASK	0x40000000
#define 	PHYACCESS_WR_CMD_SHIFT	29
#define 	PHYACCESS_WR_CMD_MASK	0x20000000
#define 	PHYACCESS_CPU_REG_ADDR_SHIFT	24
#define 	PHYACCESS_CPU_REG_ADDR_MASK	0x1f000000
#define 	PHYACCESS_CPU_PHY_ADDR_SHIFT	16
#define 	PHYACCESS_CPU_PHY_ADDR_MASK	0x1f0000
#define 	PHYACCESS_ACC_DATA_SHIFT	0
#define 	PHYACCESS_ACC_DATA_MASK	0xffff

/*  phycontrol offset0x104  */
#define 	PHYCONTROL_SD_ACCESS_EN_SHIFT	25
#define 	PHYCONTROL_SD_ACCESS_EN_MASK	0x2000000
#define 	PHYCONTROL_NWAY_AUTO_POLLING_EN_SHIFT	24
#define 	PHYCONTROL_NWAY_AUTO_POLLING_EN_MASK	0x1000000
#define 	PHYCONTROL_MDC_TRANSITION_EN_SHIFT	23
#define 	PHYCONTROL_MDC_TRANSITION_EN_MASK	0x800000
#define 	PHYCONTROL_MDC_CYCLE_TH_SHIFT	16
#define 	PHYCONTROL_MDC_CYCLE_TH_MASK	0x7f0000
#define 	PHYCONTROL_EXT_PHY_ADDR_SHIFT	0
#define 	PHYCONTROL_EXT_PHY_ADDR_MASK	0x1f

/*  gmac0_rgmii_cntl offset0x110  */
#define 	GMAC0_RGMII_CNTL_TIMING_SEL_SHIFT	0
#define 	GMAC0_RGMII_CNTL_TIMING_SEL_MASK	0x1
#define 	GMAC0_RGMII_CNTL_RGMII_DLL_RXC_BYPASS_SHIFT	1
#define 	GMAC0_RGMII_CNTL_RGMII_DLL_RXC_BYPASS_MASK	0x2
#define 	GMAC0_RGMII_CNTL_BYPASS_2NS_DEL_SHIFT	2
#define 	GMAC0_RGMII_CNTL_BYPASS_2NS_DEL_MASK	0x4
#define 	GMAC0_RGMII_CNTL_DEL_STRB_SHIFT	3
#define 	GMAC0_RGMII_CNTL_DEL_STRB_MASK	0x8
#define 	GMAC0_RGMII_CNTL_DEL_VALUE_SHIFT	4
#define 	GMAC0_RGMII_CNTL_DEL_VALUE_MASK	0x70
#define 	GMAC0_RGMII_CNTL_DEL_ADDR_SHIFT	7
#define 	GMAC0_RGMII_CNTL_DEL_ADDR_MASK	0x780

/*  cfp_access offset0x200  */
#define 	CFP_ACCESS_OP_START_DONE_SHIFT	0
#define 	CFP_ACCESS_OP_START_DONE_MASK	0x1
#define 	CFP_ACCESS_OP_SEL_SHIFT	1
#define 	CFP_ACCESS_OP_SEL_MASK	0xe
#define 	CFP_ACCESS_CFP_RAM_CLEAR_SHIFT	4
#define 	CFP_ACCESS_CFP_RAM_CLEAR_MASK	0x10
#define 	CFP_ACCESS_RESERVED1_SHIFT	5
#define 	CFP_ACCESS_RESERVED1_MASK	0x3e0
#define 	CFP_ACCESS_RAM_SEL_SHIFT	10
#define 	CFP_ACCESS_RAM_SEL_MASK	0x7c00
#define 	CFP_ACCESS_TCAM_RESET_SHIFT	15
#define 	CFP_ACCESS_TCAM_RESET_MASK	0x8000
#define 	CFP_ACCESS_XCESS_ADDR_SHIFT	16
#define 	CFP_ACCESS_XCESS_ADDR_MASK	0x1ff0000
#define 	CFP_ACCESS_RESERVED0_SHIFT	25
#define 	CFP_ACCESS_RESERVED0_MASK	0xe000000
#define 	CFP_ACCESS_RD_STATUS_SHIFT	28
#define 	CFP_ACCESS_RD_STATUS_MASK	0xf0000000

/*  cfp_tcam_data0 offset0x210  */
#define 	CFP_TCAM_DATA0_DATA_SHIFT	0
#define 	CFP_TCAM_DATA0_DATA_MASK	0xffffffff

/*  cfp_tcam_data1 offset0x214  */
#define 	CFP_TCAM_DATA1_DATA_SHIFT	0
#define 	CFP_TCAM_DATA1_DATA_MASK	0xffffffff

/*  cfp_tcam_data2 offset0x218  */
#define 	CFP_TCAM_DATA2_DATA_SHIFT	0
#define 	CFP_TCAM_DATA2_DATA_MASK	0xffffffff

/*  cfp_tcam_data3 offset0x21c  */
#define 	CFP_TCAM_DATA3_DATA_SHIFT	0
#define 	CFP_TCAM_DATA3_DATA_MASK	0xffffffff

/*  cfp_tcam_data4 offset0x220  */
#define 	CFP_TCAM_DATA4_DATA_SHIFT	0
#define 	CFP_TCAM_DATA4_DATA_MASK	0xffffffff

/*  cfp_tcam_data5 offset0x224  */
#define 	CFP_TCAM_DATA5_DATA_SHIFT	0
#define 	CFP_TCAM_DATA5_DATA_MASK	0xffffffff

/*  cfp_tcam_data6 offset0x228  */
#define 	CFP_TCAM_DATA6_DATA_SHIFT	0
#define 	CFP_TCAM_DATA6_DATA_MASK	0xffffffff

/*  cfp_tcam_data7 offset0x22c  */
#define 	CFP_TCAM_DATA7_DATA_SHIFT	0
#define 	CFP_TCAM_DATA7_DATA_MASK	0xffffffff

/*  cfp_tcam_mask0 offset0x230  */
#define 	CFP_TCAM_MASK0_DATA_SHIFT	0
#define 	CFP_TCAM_MASK0_DATA_MASK	0xffffffff

/*  cfp_tcam_mask1 offset0x234  */
#define 	CFP_TCAM_MASK1_DATA_SHIFT	0
#define 	CFP_TCAM_MASK1_DATA_MASK	0xffffffff

/*  cfp_tcam_mask2 offset0x238  */
#define 	CFP_TCAM_MASK2_DATA_SHIFT	0
#define 	CFP_TCAM_MASK2_DATA_MASK	0xffffffff

/*  cfp_tcam_mask3 offset0x23c  */
#define 	CFP_TCAM_MASK3_DATA_SHIFT	0
#define 	CFP_TCAM_MASK3_DATA_MASK	0xffffffff

/*  cfp_tcam_mask4 offset0x240  */
#define 	CFP_TCAM_MASK4_DATA_SHIFT	0
#define 	CFP_TCAM_MASK4_DATA_MASK	0xffffffff

/*  cfp_tcam_mask5 offset0x244  */
#define 	CFP_TCAM_MASK5_DATA_SHIFT	0
#define 	CFP_TCAM_MASK5_DATA_MASK	0xffffffff

/*  cfp_tcam_mask6 offset0x248  */
#define 	CFP_TCAM_MASK6_DATA_SHIFT	0
#define 	CFP_TCAM_MASK6_DATA_MASK	0xffffffff

/*  cfp_tcam_mask7 offset0x24c  */
#define 	CFP_TCAM_MASK7_DATA_SHIFT	0
#define 	CFP_TCAM_MASK7_DATA_MASK	0xffffffff

/*  cfp_action_data offset0x250  */
#define 	CFP_ACTION_DATA_CHAINID_SHIFT	0
#define 	CFP_ACTION_DATA_CHAINID_MASK	0xff
#define 	CFP_ACTION_DATA_CHANNELID_SHIFT	8
#define 	CFP_ACTION_DATA_CHANNELID_MASK	0xf00
#define 	CFP_ACTION_DATA_DROP_SHIFT	12
#define 	CFP_ACTION_DATA_DROP_MASK	0x1000
#define 	CFP_ACTION_DATA_RESERVED_SHIFT	13
#define 	CFP_ACTION_DATA_RESERVED_MASK	0xffffe000

/*  tcam_bist_cntl offset0x2a0  */
#define 	TCAM_BIST_CNTL_TCAM_BIST_EN_SHIFT	0
#define 	TCAM_BIST_CNTL_TCAM_BIST_EN_MASK	0x1
#define 	TCAM_BIST_CNTL_TCAM_BIST_TCAM_SEL_SHIFT	1
#define 	TCAM_BIST_CNTL_TCAM_BIST_TCAM_SEL_MASK	0x6
#define 	TCAM_BIST_CNTL_RESERVED1_SHIFT	3
#define 	TCAM_BIST_CNTL_RESERVED1_MASK	0x8
#define 	TCAM_BIST_CNTL_TCAM_BIST_STATUS_SEL_SHIFT	4
#define 	TCAM_BIST_CNTL_TCAM_BIST_STATUS_SEL_MASK	0xf0
#define 	TCAM_BIST_CNTL_TCAM_BIST_SKIP_ERR_CNT_SHIFT	8
#define 	TCAM_BIST_CNTL_TCAM_BIST_SKIP_ERR_CNT_MASK	0xff00
#define 	TCAM_BIST_CNTL_TCAM_TEST_COMPARE_SHIFT	16
#define 	TCAM_BIST_CNTL_TCAM_TEST_COMPARE_MASK	0x10000
#define 	TCAM_BIST_CNTL_RESERVED_SHIFT	17
#define 	TCAM_BIST_CNTL_RESERVED_MASK	0x7ffe0000
#define 	TCAM_BIST_CNTL_TCAM_BIST_DONE_SHIFT	31
#define 	TCAM_BIST_CNTL_TCAM_BIST_DONE_MASK	0x80000000

/*  tcam_bist_status offset0x2a4  */
#define 	TCAM_BIST_STATUS_TCAM_BIST_STATUS_SHIFT	0
#define 	TCAM_BIST_STATUS_TCAM_BIST_STATUS_MASK	0xffff
#define 	TCAM_BIST_STATUS_RESERVED_SHIFT	16
#define 	TCAM_BIST_STATUS_RESERVED_MASK	0xffff0000

/*  tcam_cmp_status offset0x2a8  */
#define 	TCAM_CMP_STATUS_TCAM_HIT_ADDR_SHIFT	0
#define 	TCAM_CMP_STATUS_TCAM_HIT_ADDR_MASK	0x1ff
#define 	TCAM_CMP_STATUS_RESERVED2_SHIFT	9
#define 	TCAM_CMP_STATUS_RESERVED2_MASK	0x7e00
#define 	TCAM_CMP_STATUS_TCAM_HIT_SHIFT	15
#define 	TCAM_CMP_STATUS_TCAM_HIT_MASK	0x8000
#define 	TCAM_CMP_STATUS_RESERVED1_SHIFT	16
#define 	TCAM_CMP_STATUS_RESERVED1_MASK	0xffff0000

/*  tcam_disable offset0x2ac  */
#define 	TCAM_DISABLE_TCAM_DISABLE_SHIFT	0
#define 	TCAM_DISABLE_TCAM_DISABLE_MASK	0xf
#define 	TCAM_DISABLE_RESERVED_SHIFT	4
#define 	TCAM_DISABLE_RESERVED_MASK	0xfffffff0

/*  tcam_test_cntl offset0x2f0  */
#define 	TCAM_TEST_CNTL_TCAM_TEST_CNTL_SHIFT	0
#define 	TCAM_TEST_CNTL_TCAM_TEST_CNTL_MASK	0x7ff
#define 	TCAM_TEST_CNTL_RESERVED_SHIFT	11
#define 	TCAM_TEST_CNTL_RESERVED_MASK	0xfffff800

/*  udf_0_a3_a0 offset0x300  */
#define 	UDF_0_A3_A0_CFG_UDF_0_A0_SHIFT	0
#define 	UDF_0_A3_A0_CFG_UDF_0_A0_MASK	0xff
#define 	UDF_0_A3_A0_CFG_UDF_0_A1_SHIFT	8
#define 	UDF_0_A3_A0_CFG_UDF_0_A1_MASK	0xff00
#define 	UDF_0_A3_A0_CFG_UDF_0_A2_SHIFT	16
#define 	UDF_0_A3_A0_CFG_UDF_0_A2_MASK	0xff0000
#define 	UDF_0_A3_A0_CFG_UDF_0_A3_SHIFT	24
#define 	UDF_0_A3_A0_CFG_UDF_0_A3_MASK	0xff000000

/*  udf_0_a7_a4 offset0x304  */
#define 	UDF_0_A7_A4_CFG_UDF_0_A4_SHIFT	0
#define 	UDF_0_A7_A4_CFG_UDF_0_A4_MASK	0xff
#define 	UDF_0_A7_A4_CFG_UDF_0_A5_SHIFT	8
#define 	UDF_0_A7_A4_CFG_UDF_0_A5_MASK	0xff00
#define 	UDF_0_A7_A4_CFG_UDF_0_A6_SHIFT	16
#define 	UDF_0_A7_A4_CFG_UDF_0_A6_MASK	0xff0000
#define 	UDF_0_A7_A4_CFG_UDF_0_A7_SHIFT	24
#define 	UDF_0_A7_A4_CFG_UDF_0_A7_MASK	0xff000000

/*  udf_0_a8 offset0x308  */
#define 	UDF_0_A8_CFG_UDF_0_A8_SHIFT	0
#define 	UDF_0_A8_CFG_UDF_0_A8_MASK	0xff

/*  udf_1_a3_a0 offset0x310  */
#define 	UDF_1_A3_A0_CFG_UDF_1_A0_SHIFT	0
#define 	UDF_1_A3_A0_CFG_UDF_1_A0_MASK	0xff
#define 	UDF_1_A3_A0_CFG_UDF_1_A1_SHIFT	8
#define 	UDF_1_A3_A0_CFG_UDF_1_A1_MASK	0xff00
#define 	UDF_1_A3_A0_CFG_UDF_1_A2_SHIFT	16
#define 	UDF_1_A3_A0_CFG_UDF_1_A2_MASK	0xff0000
#define 	UDF_1_A3_A0_CFG_UDF_1_A3_SHIFT	24
#define 	UDF_1_A3_A0_CFG_UDF_1_A3_MASK	0xff000000

/*  udf_1_a7_a4 offset0x314  */
#define 	UDF_1_A7_A4_CFG_UDF_1_A4_SHIFT	0
#define 	UDF_1_A7_A4_CFG_UDF_1_A4_MASK	0xff
#define 	UDF_1_A7_A4_CFG_UDF_1_A5_SHIFT	8
#define 	UDF_1_A7_A4_CFG_UDF_1_A5_MASK	0xff00
#define 	UDF_1_A7_A4_CFG_UDF_1_A6_SHIFT	16
#define 	UDF_1_A7_A4_CFG_UDF_1_A6_MASK	0xff0000
#define 	UDF_1_A7_A4_CFG_UDF_1_A7_SHIFT	24
#define 	UDF_1_A7_A4_CFG_UDF_1_A7_MASK	0xff000000

/*  udf_1_a8 offset0x318  */
#define 	UDF_1_A8_CFG_UDF_1_A8_SHIFT	0
#define 	UDF_1_A8_CFG_UDF_1_A8_MASK	0xff

/*  udf_2_a3_a0 offset0x320  */
#define 	UDF_2_A3_A0_CFG_UDF_2_A0_SHIFT	0
#define 	UDF_2_A3_A0_CFG_UDF_2_A0_MASK	0xff
#define 	UDF_2_A3_A0_CFG_UDF_2_A1_SHIFT	8
#define 	UDF_2_A3_A0_CFG_UDF_2_A1_MASK	0xff00
#define 	UDF_2_A3_A0_CFG_UDF_2_A2_SHIFT	16
#define 	UDF_2_A3_A0_CFG_UDF_2_A2_MASK	0xff0000
#define 	UDF_2_A3_A0_CFG_UDF_2_A3_SHIFT	24
#define 	UDF_2_A3_A0_CFG_UDF_2_A3_MASK	0xff000000

/*  udf_2_a7_a4 offset0x324  */
#define 	UDF_2_A7_A4_CFG_UDF_2_A4_SHIFT	0
#define 	UDF_2_A7_A4_CFG_UDF_2_A4_MASK	0xff
#define 	UDF_2_A7_A4_CFG_UDF_2_A5_SHIFT	8
#define 	UDF_2_A7_A4_CFG_UDF_2_A5_MASK	0xff00
#define 	UDF_2_A7_A4_CFG_UDF_2_A6_SHIFT	16
#define 	UDF_2_A7_A4_CFG_UDF_2_A6_MASK	0xff0000
#define 	UDF_2_A7_A4_CFG_UDF_2_A7_SHIFT	24
#define 	UDF_2_A7_A4_CFG_UDF_2_A7_MASK	0xff000000

/*  udf_2_a8 offset0x328  */
#define 	UDF_2_A8_CFG_UDF_2_A8_SHIFT	0
#define 	UDF_2_A8_CFG_UDF_2_A8_MASK	0xff

/*  udf_0_b3_b0 offset0x330  */
#define 	UDF_0_B3_B0_CFG_UDF_0_B0_SHIFT	0
#define 	UDF_0_B3_B0_CFG_UDF_0_B0_MASK	0xff
#define 	UDF_0_B3_B0_CFG_UDF_0_B1_SHIFT	8
#define 	UDF_0_B3_B0_CFG_UDF_0_B1_MASK	0xff00
#define 	UDF_0_B3_B0_CFG_UDF_0_B2_SHIFT	16
#define 	UDF_0_B3_B0_CFG_UDF_0_B2_MASK	0xff0000
#define 	UDF_0_B3_B0_CFG_UDF_0_B3_SHIFT	24
#define 	UDF_0_B3_B0_CFG_UDF_0_B3_MASK	0xff000000

/*  udf_0_b7_b4 offset0x334  */
#define 	UDF_0_B7_B4_CFG_UDF_0_B4_SHIFT	0
#define 	UDF_0_B7_B4_CFG_UDF_0_B4_MASK	0xff
#define 	UDF_0_B7_B4_CFG_UDF_0_B5_SHIFT	8
#define 	UDF_0_B7_B4_CFG_UDF_0_B5_MASK	0xff00
#define 	UDF_0_B7_B4_CFG_UDF_0_B6_SHIFT	16
#define 	UDF_0_B7_B4_CFG_UDF_0_B6_MASK	0xff0000
#define 	UDF_0_B7_B4_CFG_UDF_0_B7_SHIFT	24
#define 	UDF_0_B7_B4_CFG_UDF_0_B7_MASK	0xff000000

/*  udf_0_b8 offset0x338  */
#define 	UDF_0_B8_CFG_UDF_0_B8_SHIFT	0
#define 	UDF_0_B8_CFG_UDF_0_B8_MASK	0xff

/*  udf_1_b3_b0 offset0x340  */
#define 	UDF_1_B3_B0_CFG_UDF_1_B0_SHIFT	0
#define 	UDF_1_B3_B0_CFG_UDF_1_B0_MASK	0xff
#define 	UDF_1_B3_B0_CFG_UDF_1_B1_SHIFT	8
#define 	UDF_1_B3_B0_CFG_UDF_1_B1_MASK	0xff00
#define 	UDF_1_B3_B0_CFG_UDF_1_B2_SHIFT	16
#define 	UDF_1_B3_B0_CFG_UDF_1_B2_MASK	0xff0000
#define 	UDF_1_B3_B0_CFG_UDF_1_B3_SHIFT	24
#define 	UDF_1_B3_B0_CFG_UDF_1_B3_MASK	0xff000000

/*  udf_1_b7_b4 offset0x344  */
#define 	UDF_1_B7_B4_CFG_UDF_1_B4_SHIFT	0
#define 	UDF_1_B7_B4_CFG_UDF_1_B4_MASK	0xff
#define 	UDF_1_B7_B4_CFG_UDF_1_B5_SHIFT	8
#define 	UDF_1_B7_B4_CFG_UDF_1_B5_MASK	0xff00
#define 	UDF_1_B7_B4_CFG_UDF_1_B6_SHIFT	16
#define 	UDF_1_B7_B4_CFG_UDF_1_B6_MASK	0xff0000
#define 	UDF_1_B7_B4_CFG_UDF_1_B7_SHIFT	24
#define 	UDF_1_B7_B4_CFG_UDF_1_B7_MASK	0xff000000

/*  udf_1_b8 offset0x348  */
#define 	UDF_1_B8_CFG_UDF_1_B8_SHIFT	0
#define 	UDF_1_B8_CFG_UDF_1_B8_MASK	0xff

/*  udf_2_b3_b0 offset0x350  */
#define 	UDF_2_B3_B0_CFG_UDF_2_B0_SHIFT	0
#define 	UDF_2_B3_B0_CFG_UDF_2_B0_MASK	0xff
#define 	UDF_2_B3_B0_CFG_UDF_2_B1_SHIFT	8
#define 	UDF_2_B3_B0_CFG_UDF_2_B1_MASK	0xff00
#define 	UDF_2_B3_B0_CFG_UDF_2_B2_SHIFT	16
#define 	UDF_2_B3_B0_CFG_UDF_2_B2_MASK	0xff0000
#define 	UDF_2_B3_B0_CFG_UDF_2_B3_SHIFT	24
#define 	UDF_2_B3_B0_CFG_UDF_2_B3_MASK	0xff000000

/*  udf_2_b7_b4 offset0x354  */
#define 	UDF_2_B7_B4_CFG_UDF_2_B4_SHIFT	0
#define 	UDF_2_B7_B4_CFG_UDF_2_B4_MASK	0xff
#define 	UDF_2_B7_B4_CFG_UDF_2_B5_SHIFT	8
#define 	UDF_2_B7_B4_CFG_UDF_2_B5_MASK	0xff00
#define 	UDF_2_B7_B4_CFG_UDF_2_B6_SHIFT	16
#define 	UDF_2_B7_B4_CFG_UDF_2_B6_MASK	0xff0000
#define 	UDF_2_B7_B4_CFG_UDF_2_B7_SHIFT	24
#define 	UDF_2_B7_B4_CFG_UDF_2_B7_MASK	0xff000000

/*  udf_2_b8 offset0x358  */
#define 	UDF_2_B8_CFG_UDF_2_B8_SHIFT	0
#define 	UDF_2_B8_CFG_UDF_2_B8_MASK	0xff

/*  udf_0_c3_c0 offset0x360  */
#define 	UDF_0_C3_C0_CFG_UDF_0_C0_SHIFT	0
#define 	UDF_0_C3_C0_CFG_UDF_0_C0_MASK	0xff
#define 	UDF_0_C3_C0_CFG_UDF_0_C1_SHIFT	8
#define 	UDF_0_C3_C0_CFG_UDF_0_C1_MASK	0xff00
#define 	UDF_0_C3_C0_CFG_UDF_0_C2_SHIFT	16
#define 	UDF_0_C3_C0_CFG_UDF_0_C2_MASK	0xff0000
#define 	UDF_0_C3_C0_CFG_UDF_0_C3_SHIFT	24
#define 	UDF_0_C3_C0_CFG_UDF_0_C3_MASK	0xff000000

/*  udf_0_c7_c4 offset0x364  */
#define 	UDF_0_C7_C4_CFG_UDF_0_C4_SHIFT	0
#define 	UDF_0_C7_C4_CFG_UDF_0_C4_MASK	0xff
#define 	UDF_0_C7_C4_CFG_UDF_0_C5_SHIFT	8
#define 	UDF_0_C7_C4_CFG_UDF_0_C5_MASK	0xff00
#define 	UDF_0_C7_C4_CFG_UDF_0_C6_SHIFT	16
#define 	UDF_0_C7_C4_CFG_UDF_0_C6_MASK	0xff0000
#define 	UDF_0_C7_C4_CFG_UDF_0_C7_SHIFT	24
#define 	UDF_0_C7_C4_CFG_UDF_0_C7_MASK	0xff000000

/*  udf_0_c8 offset0x368  */
#define 	UDF_0_C8_CFG_UDF_0_C8_SHIFT	0
#define 	UDF_0_C8_CFG_UDF_0_C8_MASK	0xff

/*  udf_1_c3_c0 offset0x370  */
#define 	UDF_1_C3_C0_CFG_UDF_1_C0_SHIFT	0
#define 	UDF_1_C3_C0_CFG_UDF_1_C0_MASK	0xff
#define 	UDF_1_C3_C0_CFG_UDF_1_C1_SHIFT	8
#define 	UDF_1_C3_C0_CFG_UDF_1_C1_MASK	0xff00
#define 	UDF_1_C3_C0_CFG_UDF_1_C2_SHIFT	16
#define 	UDF_1_C3_C0_CFG_UDF_1_C2_MASK	0xff0000
#define 	UDF_1_C3_C0_CFG_UDF_1_C3_SHIFT	24
#define 	UDF_1_C3_C0_CFG_UDF_1_C3_MASK	0xff000000

/*  udf_1_c7_c4 offset0x374  */
#define 	UDF_1_C7_C4_CFG_UDF_1_C4_SHIFT	0
#define 	UDF_1_C7_C4_CFG_UDF_1_C4_MASK	0xff
#define 	UDF_1_C7_C4_CFG_UDF_1_C5_SHIFT	8
#define 	UDF_1_C7_C4_CFG_UDF_1_C5_MASK	0xff00
#define 	UDF_1_C7_C4_CFG_UDF_1_C6_SHIFT	16
#define 	UDF_1_C7_C4_CFG_UDF_1_C6_MASK	0xff0000
#define 	UDF_1_C7_C4_CFG_UDF_1_C7_SHIFT	24
#define 	UDF_1_C7_C4_CFG_UDF_1_C7_MASK	0xff000000

/*  udf_1_c8 offset0x378  */
#define 	UDF_1_C8_CFG_UDF_1_C8_SHIFT	0
#define 	UDF_1_C8_CFG_UDF_1_C8_MASK	0xff

/*  udf_2_c3_c0 offset0x380  */
#define 	UDF_2_C3_C0_CFG_UDF_2_C0_SHIFT	0
#define 	UDF_2_C3_C0_CFG_UDF_2_C0_MASK	0xff
#define 	UDF_2_C3_C0_CFG_UDF_2_C1_SHIFT	8
#define 	UDF_2_C3_C0_CFG_UDF_2_C1_MASK	0xff00
#define 	UDF_2_C3_C0_CFG_UDF_2_C2_SHIFT	16
#define 	UDF_2_C3_C0_CFG_UDF_2_C2_MASK	0xff0000
#define 	UDF_2_C3_C0_CFG_UDF_2_C3_SHIFT	24
#define 	UDF_2_C3_C0_CFG_UDF_2_C3_MASK	0xff000000

/*  udf_2_c7_c4 offset0x384  */
#define 	UDF_2_C7_C4_CFG_UDF_2_C4_SHIFT	0
#define 	UDF_2_C7_C4_CFG_UDF_2_C4_MASK	0xff
#define 	UDF_2_C7_C4_CFG_UDF_2_C5_SHIFT	8
#define 	UDF_2_C7_C4_CFG_UDF_2_C5_MASK	0xff00
#define 	UDF_2_C7_C4_CFG_UDF_2_C6_SHIFT	16
#define 	UDF_2_C7_C4_CFG_UDF_2_C6_MASK	0xff0000
#define 	UDF_2_C7_C4_CFG_UDF_2_C7_SHIFT	24
#define 	UDF_2_C7_C4_CFG_UDF_2_C7_MASK	0xff000000

/*  udf_2_c8 offset0x388  */
#define 	UDF_2_C8_CFG_UDF_2_C8_SHIFT	0
#define 	UDF_2_C8_CFG_UDF_2_C8_MASK	0xff

/*  udf_0_d3_d0 offset0x390  */
#define 	UDF_0_D3_D0_CFG_UDF_0_D0_SHIFT	0
#define 	UDF_0_D3_D0_CFG_UDF_0_D0_MASK	0xff
#define 	UDF_0_D3_D0_CFG_UDF_0_D1_SHIFT	8
#define 	UDF_0_D3_D0_CFG_UDF_0_D1_MASK	0xff00
#define 	UDF_0_D3_D0_CFG_UDF_0_D2_SHIFT	16
#define 	UDF_0_D3_D0_CFG_UDF_0_D2_MASK	0xff0000
#define 	UDF_0_D3_D0_CFG_UDF_0_D3_SHIFT	24
#define 	UDF_0_D3_D0_CFG_UDF_0_D3_MASK	0xff000000

/*  udf_0_d7_d4 offset0x394  */
#define 	UDF_0_D7_D4_CFG_UDF_0_D4_SHIFT	0
#define 	UDF_0_D7_D4_CFG_UDF_0_D4_MASK	0xff
#define 	UDF_0_D7_D4_CFG_UDF_0_D5_SHIFT	8
#define 	UDF_0_D7_D4_CFG_UDF_0_D5_MASK	0xff00
#define 	UDF_0_D7_D4_CFG_UDF_0_D6_SHIFT	16
#define 	UDF_0_D7_D4_CFG_UDF_0_D6_MASK	0xff0000
#define 	UDF_0_D7_D4_CFG_UDF_0_D7_SHIFT	24
#define 	UDF_0_D7_D4_CFG_UDF_0_D7_MASK	0xff000000

/*  udf_0_d11_d8 offset0x398  */
#define 	UDF_0_D11_D8_CFG_UDF_0_D8_SHIFT	0
#define 	UDF_0_D11_D8_CFG_UDF_0_D8_MASK	0xff
#define 	UDF_0_D11_D8_CFG_UDF_0_D9_SHIFT	8
#define 	UDF_0_D11_D8_CFG_UDF_0_D9_MASK	0xff00
#define 	UDF_0_D11_D8_CFG_UDF_0_D10_SHIFT	16
#define 	UDF_0_D11_D8_CFG_UDF_0_D10_MASK	0xff0000
#define 	UDF_0_D11_D8_CFG_UDF_0_D11_SHIFT	24
#define 	UDF_0_D11_D8_CFG_UDF_0_D11_MASK	0xff000000

#endif /* _gmac_common_core_h_ */
