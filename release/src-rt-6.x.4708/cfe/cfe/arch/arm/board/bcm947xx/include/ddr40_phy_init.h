/*
** Copyright 2000, 2001  Broadcom Corporation
** All Rights Reserved
**
** No portions of this material may be reproduced in any form 
** without the written permission of:
**
**   Broadcom Corporation
**   5300 California Avenue
**   Irvine, California 92617
**
** All information contained in this document is Broadcom 
** Corporation company private proprietary, and trade secret.
**
** ----------------------------------------------------------
**
** 
**
*  $Id:: ddr40_phy_init.h 1758 2012-08-17 15:14:03Z gennady       $:
*  $Rev::file =  : Global SVN Revision = 1780                     $:
**
*/

#ifndef __DDR40_PHY_INIT_H__
#define __DDR40_PHY_INIT_H__

#include <ddr40_phy_custom.h>

/*  !!!  VALUES ARE IMPORTANT, DO NOT CHANGE !!! */
#define DDR40_PHY_PARAM_TESTMODE          0x00000004
#define DDR40_PHY_PARAM_SKIP_PLL_VDL      0x00000008
#define DDR40_PHY_PARAM_USE_VTT           0x00000100
#define DDR40_PHY_PARAM_DIS_ODT           0x00000200
#define DDR40_PHY_PARAM_DIS_DQS_ODT       0x00000400
#define DDR40_PHY_PARAM_ODT_EARLY         0x00000800
#define DDR40_PHY_PARAM_ODT_LATE          0x00001000
#define DDR40_PHY_PARAM_AUTO_IDDQ_VALID   0x00100000
#define DDR40_PHY_PARAM_AUTO_IDDQ_CMD     0x00200000
#define DDR40_PHY_PARAM_AUTO_RXENB_VALID  0x00400000
#define DDR40_PHY_PARAM_AUTO_RXENB_CMD    0x00800000
#define DDR40_PHY_PARAM_ALLOW_VDL_NO_LOCK 0x40000000
#define DDR40_PHY_PARAM_SKIP_RD_EN_ADJUST 0x80000000
#define DDR40_PHY_PARAM_ADDR_CTL_ADJUST_0 0x00010000
#define DDR40_PHY_PARAM_ADDR_CTL_ADJUST_1 0x00020000
#define DDR40_PHY_PARAM_MAX_ZQ            0x00080000
#define DDR40_PHY_PARAM_VDDO_VOLT_0       0x01000000
#define DDR40_PHY_PARAM_VDDO_VOLT_1       0x02000000
#define DDR40_PHY_PARAM_LONG_PREAMBLE     0x04000000

#define DDR40_PHY_RETURN_OK                0
#define DDR40_PHY_RETURN_VDL_CALIB_NOLOCK  1
#define DDR40_PHY_RETURN_RDEN_CALIB_NOLOCK 2
#define DDR40_PHY_RETURN_STEP_CALIB_FAIL   3
#define DDR40_PHY_RETURN_ZQ_CALIB_FAIL     4
#define DDR40_PHY_RETURN_PLL_NOLOCK        0x110
#define DDR40_PHY_RETURN_VDL_CALIB_FAIL    0x120
#define DDR40_PHY_RETURN_RDEN_CALIB_FAIL   0x140

#define DDR40_PHY_DEFAULT_STEP_SIZE   13 /* ps */

/* main entry point */
FUNC_PROTOTYPE_PREFIX uint32_t ddr40_phy_init(
	uint32_t ddr_clk, uint32_t params, int ddr_type, uint32_t * wire_dly,
	uint32_t connect, uint32_t override, ddr40_addr_t offset) FUNC_PROTOTYPE_SUFFIX;

/* other functions */
FUNC_PROTOTYPE_PREFIX uint32_t ddr40_phy_setup_pll(
	uint32_t speed, ddr40_addr_t offset) FUNC_PROTOTYPE_SUFFIX;
FUNC_PROTOTYPE_PREFIX void ddr40_phy_addr_ctl_adjust(
	uint32_t total_steps, ddr40_addr_t offset) FUNC_PROTOTYPE_SUFFIX;

#ifdef SV_SIM
FUNC_PROTOTYPE_PREFIX void ddr40_phy_force_tmode(
	ddr40_addr_t offset) FUNC_PROTOTYPE_SUFFIX;
FUNC_PROTOTYPE_PREFIX void ddr40_phy_rd_en_adjust(
	uint32_t total_steps0, uint32_t total_steps1, uint32_t rd_en_byte_mode,
	ddr40_addr_t offset, uint32_t wl_offset) FUNC_PROTOTYPE_SUFFIX;
#ifdef DDR40_INCLUDE_ECC
FUNC_PROTOTYPE_PREFIX void ddr40_phy_ecc_rd_en_adjust(
	uint32_t total_steps0, uint32_t rd_en_byte_mode,
	ddr40_addr_t offset) FUNC_PROTOTYPE_SUFFIX;
#endif
#endif /* SV_SIM */

FUNC_PROTOTYPE_PREFIX uint32_t ddr40_phy_vdl_normal(
	uint32_t vdl_no_lock, ddr40_addr_t offset) FUNC_PROTOTYPE_SUFFIX;
FUNC_PROTOTYPE_PREFIX void ddr40_phy_vtt_on(
	uint32_t connect, uint32_t override, ddr40_addr_t offset) FUNC_PROTOTYPE_SUFFIX;
FUNC_PROTOTYPE_PREFIX uint32_t ddr40_phy_calib_zq(
	uint32_t params, ddr40_addr_t offset) FUNC_PROTOTYPE_SUFFIX;
FUNC_PROTOTYPE_PREFIX void ddr40_phy_rdly_odt(
	uint32_t speed, uint32_t params, ddr40_addr_t offset) FUNC_PROTOTYPE_SUFFIX;
FUNC_PROTOTYPE_PREFIX void ddr40_phy_ddr3_misc(
	uint32_t speed, uint32_t params, ddr40_addr_t offset) FUNC_PROTOTYPE_SUFFIX;
FUNC_PROTOTYPE_PREFIX void ddr40_phy_ddr2_misc(
	uint32_t speed, uint32_t params, ddr40_addr_t offset) FUNC_PROTOTYPE_SUFFIX;
FUNC_PROTOTYPE_PREFIX void ddr40_phy_set_autoidle(
	uint32_t params, ddr40_addr_t offset) FUNC_PROTOTYPE_SUFFIX;

#define ddr40_phy_rd_en_calib(args...)

#define  DDR40_PHY_SVN_REVISION  2701  /* DO  NOT CHANGE ! */

#endif /* __DDR40_PHY_INIT_H__ */


/*
**
** $Log: $
**
*/
