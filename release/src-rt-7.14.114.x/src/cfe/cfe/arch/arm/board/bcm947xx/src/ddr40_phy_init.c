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
**  $Id:: ddr40_phy_init.c 1504 2012-07-18 18:30:39Z gennady      $:
**  $Rev::file =  : Global SVN Revision = 1780                    $:
**
*/

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>


#include <platform.h>

#include <ddr40_phy_registers.h>
/* detect whether this is 32b phy */
#include <ddr40_variant.h>
#include <ddr40_phy_init.h>

/* uint32_t ddr40_phy_setup_pll(uint32_t speed, ddr40_addr_t offset) */
/*     speed - DDR clock speed, as number */
/*     offset - Address of beginning of PHY register space in the chip register space */

/* Return codes: */
/*     DDR40_PHY_RETURN_OK - PLL has been setup correctly */
/*     DDR40_PHY_RETURN_PLL_NOLOCK	 - PLL did not lock within expected time frame. */
FUNC_PREFIX uint32_t ddr40_phy_setup_pll(uint32_t speed, ddr40_addr_t offset) FUNC_SUFFIX
{
	uint32_t data, tmp;
	int vco_freq, timeout;

	DDR40_PHY_Print("offset = 0x%lX\n", offset);
	vco_freq = speed * 2;  /* VCO is bit clock, i.e. twice faster than DDR clock */
	/* enable div-by-2 post divider for low frequencies */
	if (vco_freq < 500) {
		DDR40_PHY_Print("VCO_FREQ is %0d which is less than 500 Mhz.\n", vco_freq);
		data = DDR40_PHY_RegRd(DDR40_CORE_PHY_CONTROL_REGS_PLL_DIVIDERS + offset);
		SET_FIELD(data, DDR40_CORE_PHY_CONTROL_REGS, PLL_DIVIDERS, NDIV, 64);
		SET_FIELD(data, DDR40_CORE_PHY_CONTROL_REGS, PLL_DIVIDERS, POST_DIV, 4);
		DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_PLL_DIVIDERS + offset, data);
	}
	else if (vco_freq < 1000) {
		DDR40_PHY_Print("VCO_FREQ is %0d which is less than 1Ghz.\n", vco_freq);
		data = DDR40_PHY_RegRd(DDR40_CORE_PHY_CONTROL_REGS_PLL_DIVIDERS + offset);
		SET_FIELD(data, DDR40_CORE_PHY_CONTROL_REGS, PLL_DIVIDERS, NDIV, 32);
		SET_FIELD(data, DDR40_CORE_PHY_CONTROL_REGS, PLL_DIVIDERS, POST_DIV, 2);
		DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_PLL_DIVIDERS + offset, data);
	}
	else  {
		DDR40_PHY_Print("VCO_FREQ is %0d which is greater than 1Ghz.\n", vco_freq);
		data = DDR40_PHY_RegRd(DDR40_CORE_PHY_CONTROL_REGS_PLL_DIVIDERS + offset);
		SET_FIELD(data, DDR40_CORE_PHY_CONTROL_REGS, PLL_DIVIDERS, NDIV, 16);
		SET_FIELD(data, DDR40_CORE_PHY_CONTROL_REGS, PLL_DIVIDERS, POST_DIV, 1);
		DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_PLL_DIVIDERS + offset, data);
	}

	/* release PLL reset */
	data = DDR40_PHY_RegRd(DDR40_CORE_PHY_CONTROL_REGS_PLL_CONFIG + offset);
	SET_FIELD(data, DDR40_CORE_PHY_CONTROL_REGS, PLL_CONFIG, RESET, 0);
	DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_PLL_CONFIG + offset, data);


	DDR40_PHY_Print("DDR Phy PLL polling for lock \n");

	timeout = 100000;
	tmp = DDR40_PHY_RegRd(DDR40_CORE_PHY_CONTROL_REGS_PLL_STATUS + offset);
	while ((timeout > 0) &&
		((tmp & DDR40_CORE_PHY_CONTROL_REGS_PLL_STATUS_LOCK_MASK) == 0))
	{
		DDR40_PHY_Timeout(1);
		tmp = DDR40_PHY_RegRd(DDR40_CORE_PHY_CONTROL_REGS_PLL_STATUS + offset);
		timeout--;
	}
	if (timeout <= 0) {
		DDR40_PHY_Fatal("ddr40_phy_init.c: Timed out waiting for DDR Phy PLL to lock\n");
		return (DDR40_PHY_RETURN_PLL_NOLOCK);
	}
	DDR40_PHY_Print("DDR Phy PLL locked.\n");
	return (DDR40_PHY_RETURN_OK);
}

/* void ddr40_phy_addr_ctl_adjust(uint32_t total_steps, ddr40_addr_t offset) */
/*     total_steps - Desired delay in steps for addr/ctl adjustment */
/*     offset - Address beginning of PHY register space in the chip register space */
FUNC_PREFIX void ddr40_phy_addr_ctl_adjust(uint32_t total_steps, ddr40_addr_t offset) FUNC_SUFFIX
{
	uint32_t tmp, data;

	data = DDR40_PHY_RegRd(DDR40_CORE_PHY_CONTROL_REGS_VDL_WR_CHAN_CALIB_STATUS + offset);
	if (GET_FIELD(data, DDR40_CORE_PHY_CONTROL_REGS, VDL_WR_CHAN_CALIB_STATUS,
		wr_chan_calib_byte_sel) == 0)
	{
		/* we don't do adjustment if we are in BYTE mode, because it means the clock
		 * is very slow and no adjustment is needed
		 */
		tmp = 0;
		SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, VDL_OVRIDE_BIT_CTL, ovr_step,
		GET_FIELD(data, DDR40_CORE_PHY_CONTROL_REGS, VDL_WR_CHAN_CALIB_STATUS,
			wr_chan_calib_total) >> 4);
		SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, VDL_OVRIDE_BIT_CTL, ovr_en, 1);
		/* use BYTE VDLs for adjustment, so switch to BYTE mode */
		SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, VDL_OVRIDE_BIT_CTL, byte_sel, 1);
		DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BIT_CTL + offset, tmp);
		tmp = 0;
		SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, VDL_OVRIDE_BYTE_CTL, ovr_step,
			(total_steps > 10) ? (total_steps - 10)/2 : 0); /* avoid negative */
		SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, VDL_OVRIDE_BYTE_CTL, ovr_en, 1);
		DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_VDL_OVRIDE_BYTE_CTL + offset, tmp);
	}
}

#ifdef SV_SIM

/* void ddr40_phy_rd_en_adjust(uint32_t total_steps0, uint32_t total_steps1,
 *		uint32_t rd_en_byte_mode, ddr40_addr_t offset, uint32_t wl_offset)
 *     total_steps0 - Desired delay for byte lane 0 within the word lane in steps
 *     total_steps1 - Desired delay for byte lane 1 within the word lane in steps
 *     rd_en_byte_mode - Byte (pair) mode vs. bit (single) mode for RD_EN VDL
 *     offset - Address beginning of PHY register space in the chip register space
 *     wl_offset - Offset of this word lane relative to word lane 0
 */
FUNC_PREFIX void ddr40_phy_rd_en_adjust(uint32_t total_steps0, uint32_t total_steps1,
	uint32_t rd_en_byte_mode, ddr40_addr_t offset, uint32_t wl_offset) FUNC_SUFFIX
{
	uint32_t tmp, bit_vdl_steps0, bit_vdl_steps1, byte_vdl_steps;
	uint32_t fixed_steps, adj_steps0, adj_steps1;

	/* Compute new Read Enable VDL settings.
	 *
	 * The C0 PHY contains up to 4 VDL's on the read enable path. In normal mode, the
	 * read enable VDL path contains 3 VDL's arranged in a "byte/bit" structure. The
	 * auto-init/override modes enable the 4-VDL path (two common for each byte lane
	 * and two individual for each byte lane). This logic takes that into account when
	 * overriding the VDL settings.
	 */

	fixed_steps = ((rd_en_byte_mode) ? 8 : 24);
	adj_steps0 = ((total_steps0 < fixed_steps) ? 0 : (total_steps0 - fixed_steps));
	adj_steps1 = ((total_steps1 < fixed_steps) ? 0 : (total_steps1 - fixed_steps));

	/* The total number of steps are being applied across 4 VDL's. The smaller 1/4
	 * is applied to the common VDL pair and the remaining 1/4 that is unique to
	 * each byte lane is applied to the byte lane specific VDL pair.
	 */

	if (adj_steps0 < adj_steps1) {
		byte_vdl_steps = (adj_steps0 >> 2);
	} else {
		byte_vdl_steps = (adj_steps1 >> 2);
	}

	if (byte_vdl_steps > 63)
		byte_vdl_steps = 63;

	bit_vdl_steps0 = (((adj_steps0 - (byte_vdl_steps << 1)) >> 1));
	bit_vdl_steps1 = (((adj_steps1 - (byte_vdl_steps << 1)) >> 1));

	tmp = 0;
	SET_FIELD(tmp, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE_RD_EN, ovr_en, 1);
	SET_FIELD(tmp, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE_RD_EN, ovr_force, 1);
	SET_FIELD(tmp, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE_RD_EN, ovr_step,
		byte_vdl_steps);
	DDR40_PHY_RegWr((DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE_RD_EN + offset +
		wl_offset), tmp);
	DDR40_PHY_Print("ddr40_phy_init:: VDL_OVRIDE_BYTEx_RD_EN set to: 0x%02X (%d)\n",
		tmp, tmp & 0x3F);

	tmp = 0;
	SET_FIELD(tmp, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_en, 1);
	SET_FIELD(tmp, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_force, 1);
	SET_FIELD(tmp, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE0_BIT_RD_EN, ovr_step,
		bit_vdl_steps0);
	DDR40_PHY_RegWr((DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_BIT_RD_EN + offset +
		wl_offset), tmp);
	DDR40_PHY_Print("ddr40_phy_init:: VDL_OVRIDE_BYTEx_BIT_RD_EN set to: 0x%02X (%d)\n",
		tmp, tmp & 0x3F);

	tmp = 0;
	SET_FIELD(tmp, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE1_BIT_RD_EN, ovr_en, 1);
	SET_FIELD(tmp, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE1_BIT_RD_EN, ovr_force, 1);
	SET_FIELD(tmp, DDR40_CORE_PHY_WORD_LANE_0, VDL_OVRIDE_BYTE1_BIT_RD_EN, ovr_step,
		bit_vdl_steps1);
	DDR40_PHY_RegWr((DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE1_BIT_RD_EN + offset +
		wl_offset), tmp);
	DDR40_PHY_Print("ddr40_phy_init:: VDL_OVRIDE_BYTEx_BIT_RD_EN set to: 0x%02X (%d)\n",
		tmp, tmp & 0x3F);
}

#ifdef DDR40_INCLUDE_ECC
/* void ddr40_phy_ecc_rd_en_adjust(uint32_t total_steps0, uint32_t rd_en_byte_mode,
 *	ddr40_addr_t offset)
 *     total_steps0 - Desired delay for byte lane 0 within the word lane in steps
 *     rd_en_byte_mode - Byte (pair) mode vs. bit (single) mode for RD_EN VDL
 *     offset - Address beginning of PHY register space in the chip register space
 */
FUNC_PREFIX void ddr40_phy_ecc_rd_en_adjust(uint32_t total_steps0, uint32_t rd_en_byte_mode,
	ddr40_addr_t offset) FUNC_SUFFIX
{
	uint32_t tmp, bit_vdl_steps0, byte_vdl_steps, fixed_steps, adj_steps0;

	/* Compute new Read Enable VDL settings.
	 *
	 * The C0 PHY contains up to 4 VDL's on the read enable path. In normal mode, the
	 * read enable VDL path contains 3 VDL's arranged in a "byte/bit" structure. The
	 * auto-init/override modes enable the 4-VDL path (two common for each byte lane
	 * and two individual for each byte lane). This logic takes that into account when
	 * overriding the VDL settings.
	 */

	fixed_steps = ((rd_en_byte_mode) ? 8 : 24);
	adj_steps0 = ((total_steps0 < fixed_steps) ? 0 : (total_steps0 - fixed_steps));

	/* The total number of steps are being applied across 4 VDL's. The smaller 1/4
	 * is applied to the common VDL pair and the remaining 1/4 that is unique to
	 * each byte lane is applied to the byte lane specific VDL pair.
	 */

	byte_vdl_steps = (adj_steps0 >> 2);
	if (byte_vdl_steps > 63)
		byte_vdl_steps = 63;
	bit_vdl_steps0 = (((adj_steps0 - (byte_vdl_steps << 1)) >> 1));

	tmp = 0;
	SET_FIELD(tmp, DDR40_CORE_PHY_ECC_LANE, VDL_OVRIDE_BYTE_RD_EN, ovr_en, 1);
	SET_FIELD(tmp, DDR40_CORE_PHY_ECC_LANE, VDL_OVRIDE_BYTE_RD_EN, ovr_force, 1);
	SET_FIELD(tmp, DDR40_CORE_PHY_ECC_LANE, VDL_OVRIDE_BYTE_RD_EN, ovr_step, byte_vdl_steps);
	DDR40_PHY_RegWr((DDR40_CORE_PHY_ECC_LANE_VDL_OVRIDE_BYTE_RD_EN + offset), tmp);
	DDR40_PHY_Print("ddr40_phy_init:: VDL_OVRIDE_BYTEx_RD_EN set to: 0x%02X (%d)\n",
		tmp, tmp & 0x3F);

	tmp = 0;
	SET_FIELD(tmp, DDR40_CORE_PHY_ECC_LANE, VDL_OVRIDE_BYTE_BIT_RD_EN, ovr_en, 1);
	SET_FIELD(tmp, DDR40_CORE_PHY_ECC_LANE, VDL_OVRIDE_BYTE_BIT_RD_EN, ovr_force, 1);
	SET_FIELD(tmp, DDR40_CORE_PHY_ECC_LANE, VDL_OVRIDE_BYTE_BIT_RD_EN, ovr_step,
		bit_vdl_steps0);
	DDR40_PHY_RegWr((DDR40_CORE_PHY_ECC_LANE_VDL_OVRIDE_BYTE_BIT_RD_EN + offset), tmp);
	DDR40_PHY_Print("ddr40_phy_init:: VDL_OVRIDE_BYTEx_BIT_RD_EN set to: 0x%02X (%d)\n",
		tmp, tmp & 0x3F);
}
#endif /* DDR40_INCLUDE_ECC */
#endif /* SV_SIM */

/* uint32_t ddr40_phy_vdl_normal(uint32_t vdl_no_lock, ddr40_addr_t offset)
 *     vdl_no_lock - Allow VDL not to lock
 *     offset - Address beginning of PHY register space in the chip register space
 *
 * Return codes:
 *     DDR40_PHY_RETURN_OK - function finished normally
 *     DDR40_PHY_RETURN_VDL_CALIB_FAIL - VDL calibration failed (timed out)
 *     DDR40_PHY_RETURN_VDL_CALIB_NOLOCK - VDL calibration did not lock
 */
FUNC_PREFIX uint32_t ddr40_phy_vdl_normal(uint32_t vdl_no_lock, ddr40_addr_t offset) FUNC_SUFFIX
{
	uint32_t tmp;
	int timeout;
	uint32_t return_code;

	return_code = DDR40_PHY_RETURN_OK;

	DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIBRATE + offset, 0);

	tmp = 0;
	/* TBD : Talk to Efim. JAL. */
	SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, VDL_CALIBRATE, calib_fast, 1);
	SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, VDL_CALIBRATE, calib_once, 1);
	SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, VDL_CALIBRATE, calib_auto, 1);
	DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIBRATE + offset, tmp);

	timeout = 100;
	tmp = DDR40_PHY_RegRd(DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIB_STATUS + offset);
	while ((timeout > 0) && (GET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS,
		VDL_CALIB_STATUS, calib_idle) == 0))
	{
		DDR40_PHY_Timeout(1);
		timeout--;
		tmp = DDR40_PHY_RegRd(DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIB_STATUS + offset);
	}

	if (timeout <= 0) {
		DDR40_PHY_Fatal("ddr40_phy_init.c: DDR PHY VDL Calibration failed\n");
		return (DDR40_PHY_RETURN_VDL_CALIB_FAIL);
	}

	if (GET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, VDL_CALIB_STATUS, calib_lock) == 0) {
		if (vdl_no_lock)
			DDR40_PHY_Print(
				"DDR PHY VDL calibration complete but did not lock! Step = %d\n",
				GET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, VDL_CALIB_STATUS,
					calib_total) >> 4);
		else {
			DDR40_PHY_Error(
				"DDR PHY VDL calibration complete but did not lock! Step = %d\n",
				GET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, VDL_CALIB_STATUS,
					calib_total) >> 4);
			return_code = DDR40_PHY_RETURN_VDL_CALIB_NOLOCK;
		}
	}
#ifdef SV_SIM
	tmp = DDR40_PHY_RegRd(DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIB_STATUS + offset);
	cal_steps = GET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, VDL_CALIB_STATUS, calib_total) >> 4;

	DDR40_PHY_Print("ddr40_phy_init:: VDL calibration result: 0x%02X (%d)\n", tmp, cal_steps);

	DDR40_PHY_RegRd(DDR40_CORE_PHY_CONTROL_REGS_VDL_WR_CHAN_CALIB_STATUS + offset);
	DDR40_PHY_RegRd(DDR40_CORE_PHY_CONTROL_REGS_VDL_RD_EN_CALIB_STATUS + offset);
	DDR40_PHY_RegRd(DDR40_CORE_PHY_CONTROL_REGS_VDL_DQ_CALIB_STATUS + offset);
#endif
	/* clear VDL calib control */
	DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIBRATE + offset, 0);

	return (return_code);
}

/* void ddr40_phy_vtt_on(uint32_t connect, uint32_t override, ddr40_addr_t offset)
 *     connect - Mask of the signals connected to the Virtual VTT capacitor
 *     override - Mask of the signals used to control voltage on the Virtual VTT capacitor
 *     offset - Address beginning of PHY register space in the chip register space
 */
FUNC_PREFIX void ddr40_phy_vtt_on(uint32_t connect, uint32_t override,
	ddr40_addr_t offset) FUNC_SUFFIX
{
	uint32_t tmp;

	DDR40_PHY_Print("ddr40_phy_init:: Virtual VttSetup onm CONNECT=0x%08X, OVERRIDE=0x%08X\n",
		connect, override);
	DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_VIRTUAL_VTT_CONNECTIONS + offset, connect);
	DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_VIRTUAL_VTT_OVERRIDE + offset, override);
	/* use RAS CAS WE to determine idle cycles */  
	tmp = DDR40_CORE_PHY_CONTROL_REGS_VIRTUAL_VTT_CONTROL_enable_ctl_idle_MASK;
	DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_VIRTUAL_VTT_CONTROL + offset, tmp);
	DDR40_PHY_Print("ddr40_phy_init:: Virtual Vtt Enabled\n");
}

/* uint32_t ddr40_phy_calib_zq(uint32_t params, ddr40_addr_t offset)
 *     params - Set of flags
 *     offset - Address beginning of PHY register space in the chip register space
 *
 * Return codes:
 *     DDR40_PHY_RETURN_OK - function finished normally
 *     DDR40_PHY_RETURN_ZQ_CALIB_FAIL - ZQ calibration failed (timed out)
 */
FUNC_PREFIX uint32_t ddr40_phy_calib_zq(uint32_t params, ddr40_addr_t offset) FUNC_SUFFIX
{
	uint32_t tmp;
	int timeout;

	if (params & DDR40_PHY_PARAM_MAX_ZQ) {
		tmp = 0;
		SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL,
			dq_nd_override_val, 7);
		SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL,
			dq_pd_override_val, 7);
		SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL,
			addr_nd_override_val, 7);
		SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL,
			addr_pd_override_val, 7);
		SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL,
			dq_ovr_en, 1);
		SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL,
			addr_ovr_en, 1);
		DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_ZQ_PVT_COMP_CTL + offset, tmp);

		return (DDR40_PHY_RETURN_OK);
	}

	tmp = 0;
	SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL, sample_en, 0);
	DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_ZQ_PVT_COMP_CTL + offset, tmp);
	tmp = 0;
	SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL, sample_en, 1);
	DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_ZQ_PVT_COMP_CTL + offset, tmp);

	timeout = 100;
	tmp = DDR40_PHY_RegRd(DDR40_CORE_PHY_CONTROL_REGS_ZQ_PVT_COMP_CTL + offset);
	while ((timeout > 0) &&
		(GET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL, sample_done) == 0))
	{
		DDR40_PHY_Timeout(1);
		timeout--;
		tmp = DDR40_PHY_RegRd(DDR40_CORE_PHY_CONTROL_REGS_ZQ_PVT_COMP_CTL + offset);
	}

	if (timeout <= 0) {
		DDR40_PHY_Fatal(
			"ddr40_phy_init.c: ddr40_phy_init: DDR PHY ZQ Calibration failed\n");
		return (DDR40_PHY_RETURN_ZQ_CALIB_FAIL);
	}
	else
		DDR40_PHY_Print("ddr40_phy_init: ZQ Cal complete.  ND_COMP = %d, PD_COMP = %d,"
			" ND_DONE = %d, PD_DONE = %d\n",
			GET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL, nd_comp),
			GET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL, pd_comp),
			GET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL, nd_done),
			GET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL, pd_done));

	return (DDR40_PHY_RETURN_OK);
}

#ifdef SV_SIM
/* void ddr40_phy_force_tmode(ddr40_addr_t offset)
 *     offset - Address beginning of PHY register space in the chip register space
 */
FUNC_PREFIX void ddr40_phy_force_tmode(ddr40_addr_t offset) FUNC_SUFFIX
{
	uint32_t tmp;

	DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIBRATE + offset,
		DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIBRATE_calib_ftm_MASK);

	DDR40_PHY_Timeout(1);

	tmp = 0;
	SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL, addr_ovr_en, 1);
	SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL, dq_ovr_en, 1);
	SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL, addr_pd_override_val, 5);
	SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL, addr_nd_override_val, 5);
	SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL, dq_pd_override_val, 5);
	SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, ZQ_PVT_COMP_CTL, dq_nd_override_val, 5);
	DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_ZQ_PVT_COMP_CTL + offset, tmp);
}
#endif /* SV_SIM */

/* void ddr40_phy_rdly_odt(uint32_t speed, uint32_t params, ddr40_addr_t offset)
 *     speed - DDR clock speed
 *     params - Set of flags
 *     offset - Address beginning of PHY register space in the chip register space
 */
FUNC_PREFIX void ddr40_phy_rdly_odt(uint32_t speed, uint32_t params,
	ddr40_addr_t offset) FUNC_SUFFIX
{
	uint32_t data, dly;

	data = 0;
#ifdef POSTLAYOUT
#ifdef DDR40_PROCESS_TYPE_IS_40LP
	dly = (speed <= 400)? 4: (speed <= 900) ? 5 : 6;
#else
	dly = (speed <= 400)? 2: (speed <= 900) ? 3 : 4;
#endif /* DDR40_PROCESS_TYPE_IS_40LP */
#else
	dly = (speed <= 400)? 1: (speed <= 667) ? 2 : 3;
#endif /* POSTLAYOUT */

	/* two versions, for backwards compatibility */
#ifdef DDR40_CORE_PHY_WORD_LANE_0_READ_DATA_DLY_rd_data_dly_MASK
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, READ_DATA_DLY, rd_data_dly, dly);
	DDR40_PHY_RegWr(DDR40_CORE_PHY_WORD_LANE_0_READ_DATA_DLY + offset, data);
#ifdef DDR40_WIDTH_IS_32
	DDR40_PHY_RegWr(DDR40_CORE_PHY_WORD_LANE_1_READ_DATA_DLY + offset, data);
#endif
#ifdef DDR40_INCLUDE_ECC
	DDR40_PHY_RegWr(DDR40_CORE_PHY_ECC_LANE_READ_DATA_DLY + offset, data);
#endif
#else
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, READ_CONTROL, rd_data_dly, dly);
#endif /* DDR40_CORE_PHY_WORD_LANE_0_READ_DATA_DLY_rd_data_dly_MASK */
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, READ_CONTROL, dq_odt_enable,
		(params & (DDR40_PHY_PARAM_DIS_DQS_ODT | DDR40_PHY_PARAM_DIS_ODT)) ? 0:1);
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, READ_CONTROL, dq_odt_le_adj,
		(params & DDR40_PHY_PARAM_ODT_EARLY) ? 1 : 0);
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, READ_CONTROL, dq_odt_te_adj,
		(params & DDR40_PHY_PARAM_ODT_LATE) ? 1 : 0);

	DDR40_PHY_RegWr(DDR40_CORE_PHY_WORD_LANE_0_READ_CONTROL + offset, data);
#ifdef DDR40_WIDTH_IS_32
	DDR40_PHY_RegWr(DDR40_CORE_PHY_WORD_LANE_1_READ_CONTROL + offset, data);
#endif
#ifdef DDR40_INCLUDE_ECC
	DDR40_PHY_RegWr(DDR40_CORE_PHY_ECC_LANE_READ_CONTROL + offset, data);
#endif
}

/* void ddr40_phy_ddr3_misc(uint32_t speed, uint32_t params, ddr40_addr_t offset)
 *     speed - DDR clock speed
 *     params - Set of flags
 *     offset - Address beginning of PHY register space in the chip register space
 */
FUNC_PREFIX void ddr40_phy_ddr3_misc(uint32_t speed, uint32_t params,
	ddr40_addr_t offset) FUNC_SUFFIX
{
	uint32_t data;
	uint32_t vddo_volts;

	vddo_volts = ((params & DDR40_PHY_PARAM_VDDO_VOLT_0)? 1: 0) +
		((params & DDR40_PHY_PARAM_VDDO_VOLT_1)? 2: 0);

	data = 0;
	SET_FIELD(data, DDR40_CORE_PHY_CONTROL_REGS, DRIVE_PAD_CTL, vddo_volts, vddo_volts);
	SET_FIELD(data, DDR40_CORE_PHY_CONTROL_REGS, DRIVE_PAD_CTL, rt120b_g, 1);
	DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_DRIVE_PAD_CTL + offset, data);
	data = 0;
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, DRIVE_PAD_CTL, vddo_volts, vddo_volts);
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, DRIVE_PAD_CTL, rt120b_g, 1);
	DDR40_PHY_RegWr(DDR40_CORE_PHY_WORD_LANE_0_DRIVE_PAD_CTL + offset, data);
#ifdef DDR40_WIDTH_IS_32
	DDR40_PHY_RegWr(DDR40_CORE_PHY_WORD_LANE_1_DRIVE_PAD_CTL + offset, data);
#endif
#ifdef DDR40_INCLUDE_ECC
	DDR40_PHY_RegWr(DDR40_CORE_PHY_ECC_LANE_DRIVE_PAD_CTL + offset, data);
#endif
	ddr40_phy_rdly_odt(speed, params, offset);

	data = DDR40_CORE_PHY_WORD_LANE_0_WR_PREAMBLE_MODE_mode_MASK; /* DDR3 */
	if (params & DDR40_PHY_PARAM_LONG_PREAMBLE)
		data |= DDR40_CORE_PHY_WORD_LANE_0_WR_PREAMBLE_MODE_long_MASK;
	DDR40_PHY_RegWr(DDR40_CORE_PHY_WORD_LANE_0_WR_PREAMBLE_MODE + offset, data);
#ifdef DDR40_WIDTH_IS_32
	DDR40_PHY_RegWr(DDR40_CORE_PHY_WORD_LANE_1_WR_PREAMBLE_MODE + offset, data);
#endif
#ifdef DDR40_INCLUDE_ECC
	DDR40_PHY_RegWr(DDR40_CORE_PHY_ECC_LANE_WR_PREAMBLE_MODE + offset, data);
#endif
}

/* void ddr40_phy_ddr2_misc(uint32_t speed, uint32_t params, ddr40_addr_t offset)
 *     speed - DDR clock speed
 *     params - Set of flags
 *     offset - Address beginning of PHY register space in the chip register space
 */
FUNC_PREFIX void ddr40_phy_ddr2_misc(uint32_t speed, uint32_t params,
	ddr40_addr_t offset) FUNC_SUFFIX
{
	uint32_t vddo_volts;
	uint32_t data;

	vddo_volts = ((params & DDR40_PHY_PARAM_VDDO_VOLT_0)? 1: 0) +
		((params & DDR40_PHY_PARAM_VDDO_VOLT_1)? 2: 0);

	data = 0;
	SET_FIELD(data, DDR40_CORE_PHY_CONTROL_REGS, DRIVE_PAD_CTL, vddo_volts, vddo_volts);
	DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_DRIVE_PAD_CTL + offset, data);
	data = 0;
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, DRIVE_PAD_CTL, vddo_volts, vddo_volts);
	DDR40_PHY_RegWr(DDR40_CORE_PHY_WORD_LANE_0_DRIVE_PAD_CTL + offset, data);
#ifdef DDR40_WIDTH_IS_32
	DDR40_PHY_RegWr(DDR40_CORE_PHY_WORD_LANE_1_DRIVE_PAD_CTL + offset, data);
#endif
#ifdef DDR40_INCLUDE_ECC
	DDR40_PHY_RegWr(DDR40_CORE_PHY_ECC_LANE_DRIVE_PAD_CTL + offset, data);
#endif

	ddr40_phy_rdly_odt(speed, params, offset);

	data = (params & DDR40_PHY_PARAM_LONG_PREAMBLE) ?
		DDR40_CORE_PHY_WORD_LANE_0_WR_PREAMBLE_MODE_long_MASK : 0; /* DDR2 */

	DDR40_PHY_RegWr(DDR40_CORE_PHY_WORD_LANE_0_WR_PREAMBLE_MODE + offset, data);
#ifdef DDR40_WIDTH_IS_32
	DDR40_PHY_RegWr(DDR40_CORE_PHY_WORD_LANE_1_WR_PREAMBLE_MODE + offset, data);
#endif
#ifdef DDR40_INCLUDE_ECC
	DDR40_PHY_RegWr(DDR40_CORE_PHY_ECC_LANE_WR_PREAMBLE_MODE + offset, data);
#endif
}

/* void ddr40_phy_autoidle_on(uint32_t params, ddr40_addr_t offset) */
/*     params - Set of flags */
/*     offset - Address beginning of PHY register space in the chip register space */
FUNC_PREFIX void ddr40_phy_set_autoidle(uint32_t params, ddr40_addr_t offset) FUNC_SUFFIX
{
	uint32_t data;
	uint32_t iddq_code;
	uint32_t rxenb_code;

	iddq_code = ((params & DDR40_PHY_PARAM_AUTO_IDDQ_VALID)? 1:0) +
		((params & DDR40_PHY_PARAM_AUTO_IDDQ_CMD)? 2:0);
	rxenb_code = ((params & DDR40_PHY_PARAM_AUTO_RXENB_VALID)? 1:0) +
		((params & DDR40_PHY_PARAM_AUTO_RXENB_CMD)? 2:0);

	data = DDR40_PHY_RegRd(DDR40_CORE_PHY_WORD_LANE_0_IDLE_PAD_CONTROL + offset);
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, IDLE_PAD_CONTROL,
		auto_dq_iddq_mode, iddq_code);
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_0, IDLE_PAD_CONTROL,
		auto_dq_rxenb_mode, rxenb_code);
	DDR40_PHY_RegWr(DDR40_CORE_PHY_WORD_LANE_0_IDLE_PAD_CONTROL + offset, data);

#ifdef DDR40_WIDTH_IS_32
	data = DDR40_PHY_RegRd(DDR40_CORE_PHY_WORD_LANE_1_IDLE_PAD_CONTROL + offset);
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_1, IDLE_PAD_CONTROL,
		auto_dq_iddq_mode, iddq_code);
	SET_FIELD(data, DDR40_CORE_PHY_WORD_LANE_1, IDLE_PAD_CONTROL,
		auto_dq_rxenb_mode, rxenb_code);
	DDR40_PHY_RegWr(DDR40_CORE_PHY_WORD_LANE_1_IDLE_PAD_CONTROL + offset, data);
#endif
#ifdef DDR40_INCLUDE_ECC
	data = DDR40_PHY_RegRd(DDR40_CORE_PHY_ECC_LANE_IDLE_PAD_CONTROL + offset);
	SET_FIELD(data, DDR40_CORE_PHY_ECC_LANE, IDLE_PAD_CONTROL,
		auto_dq_iddq_mode, iddq_code);
	SET_FIELD(data, DDR40_CORE_PHY_ECC_LANE, IDLE_PAD_CONTROL,
		auto_dq_rxenb_mode, rxenb_code);
	DDR40_PHY_RegWr(DDR40_CORE_PHY_ECC_LANE_IDLE_PAD_CONTROL + offset, data);
#endif
}


/* uint32_t ddr40_phy_init(uint32_t ddr_clk, uint32_t params, int ddr_type,
 *	uint32_t * wire_dly, uint32_t connect, uint32_t override, ddr40_addr_t offset)
 *     ddr_clk - DDR clock speed, as number (Important: DDR clock speed in MHz, not a bit rate).
 *     params - Set of flags
 *     ddr_type - 0: DDR2, 1: DDR3
 *     wire_dly - Array of wire delays for all byte lanes (CLK/DQS roundtrip in ps)
 *     connect - Mask of the signals connected to the Virtual VTT capacitor
 *	(see RDB for specific bits)
 *     override - Mask of the signals used to control voltage on the Virtual VTT capacitor
 *	(see RDB for specific bits)
 *     offset - Address of beginning of PHY register space in the chip register space
 */

/* Return codes:
 *     DDR40_PHY_RETURN_OK - PHY has been setup correctly
 *     DDR40_PHY_RETURN_PLL_NOLOCK	 - PLL did not lock within expected time frame.
 *     DDR40_PHY_RETURN_VDL_CALIB_FAIL - VDL calibration failed (timed out)
 *     DDR40_PHY_RETURN_VDL_CALIB_NOLOCK - VDL calibration did not lock
 *     DDR40_PHY_RETURN_ZQ_CALIB_FAIL - ZQ calibration failed (timed out)
 *     DDR40_PHY_RETURN_RDEN_CALIB_FAIL - RD_EN calibration failed (timed out)
 *     DDR40_PHY_RETURN_RDEN_CALIB_NOLOCK - RD_EN calibration did not lock
 *     DDR40_PHY_RETURN_STEP_CALIB_FAIL - Step calibration failed (timed out)
 */
FUNC_PREFIX unsigned int ddr40_phy_init(uint32_t ddr_clk, uint32_t params, int ddr_type,
	uint32_t *wire_dly, uint32_t connect, uint32_t override, ddr40_addr_t offset) FUNC_SUFFIX
{
	/* This coding style does not work in MIPS boot loader, as it requires that the memory that
	 * holds the offset be initialized before the program starts.
	 * This data segment runs out of SID SRAM and must not be initialize by the compiler,
	 * as this would force a pre-load of memory
	 * Look at the alternative coding style in this file.
	 */
	uint32_t tmp, ddr_clk_per, cal_steps, step_size, timeout;
	uint32_t total_steps0;
	uint32_t tmode;
	uint32_t return_code = (uint32_t)wire_dly;

	return_code = DDR40_PHY_RETURN_OK;

	tmode = (params & DDR40_PHY_PARAM_TESTMODE) ? 1 : 0;

	/* Emulation mode does NOT init PLL or VDL */
#ifdef BCU_EMU
	return;
#endif
#ifdef CONFIG_DDR_LONG_PREAMBLE
	params |= DDR40_PHY_PARAM_LONG_PREAMBLE;
#endif

	if (params & DDR40_PHY_PARAM_SKIP_PLL_VDL)
		return (DDR40_PHY_RETURN_OK);

	if (!tmode) {
		DDR40_PHY_Print("ddr40_phy_init.c: Configuring DDR Controller PLLs\n");
		if ((return_code = ddr40_phy_setup_pll(ddr_clk, offset)) != DDR40_PHY_RETURN_OK)
			return (return_code);

		return_code |= ddr40_phy_vdl_normal(
			(params & DDR40_PHY_PARAM_ALLOW_VDL_NO_LOCK)? 1 : 0, offset);

		/* TBD : I updated this code to support per byte lane trace lengths. The part
		 *       where separate trace lengths are specified needs to be added. JAL.
		 */
		/* 'ddr_clk' is the DDR bus clock period in Mhz. Convert to ps. */
		ddr_clk_per = ((1000 * 1000) / ddr_clk);
		/* Perform a step size calibration. */
		tmp = 0;
		DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIBRATE + offset, tmp);
		SET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, VDL_CALIBRATE, calib_steps, 1);
		DDR40_PHY_RegWr(DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIBRATE + offset, tmp);

		timeout = 100;
		tmp = DDR40_PHY_RegRd(DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIB_STATUS + offset);

		while ((timeout > 0) &&
			((tmp & DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIB_STATUS_calib_idle_MASK) == 0))
		{
			DDR40_PHY_Timeout(1);
			tmp = DDR40_PHY_RegRd(DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIB_STATUS
				+ offset);
			timeout--;
		}

		if ((timeout <= 0) ||
			((tmp & DDR40_CORE_PHY_CONTROL_REGS_VDL_CALIB_STATUS_calib_lock_MASK) == 0))
		{
			DDR40_PHY_Error(
				"ddr40_phy_init.c: DDR PHY step size calibration failed.\n");
			step_size = DDR40_PHY_DEFAULT_STEP_SIZE;
			cal_steps = ddr_clk_per/step_size;
			return_code |= DDR40_PHY_RETURN_STEP_CALIB_FAIL;
		} else {
			DDR40_PHY_Print(
				"ddr40_phy_init::DDR PHY step size calibration complete.\n");
			cal_steps = GET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS, VDL_CALIB_STATUS,
				calib_total);
			step_size = (ddr_clk_per / cal_steps);
		}

		tmp = ((params & DDR40_PHY_PARAM_ADDR_CTL_ADJUST_1) ? 2 : 0) + ((params &
			DDR40_PHY_PARAM_ADDR_CTL_ADJUST_0) ? 1 : 0);
		total_steps0 = (cal_steps >> 4) * tmp;
		if (tmp > 0)
			ddr40_phy_addr_ctl_adjust(total_steps0, offset);

#ifdef SV_SIM
		if (!(params & DDR40_PHY_PARAM_SKIP_RD_EN_ADJUST)) {
			/* The READ enable VDL calibration tracks the DQS setting
			 * (half bit period or full bit period).
			 */
			tmp = DDR40_PHY_RegRd(DDR40_CORE_PHY_CONTROL_REGS_VDL_RD_EN_CALIB_STATUS +
				offset);
			rd_en_byte_mode = GET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS,
				VDL_RD_EN_CALIB_STATUS, rd_en_calib_byte_sel);
			rd_en_byte_vdl_steps = GET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS,
				VDL_RD_EN_CALIB_STATUS, rd_en_calib_total) >> 4;
			rd_en_bit_vdl_offset = GET_FIELD(tmp, DDR40_CORE_PHY_CONTROL_REGS,
				VDL_RD_EN_CALIB_STATUS, rd_en_calib_bit_offset);

			/* This is where the separate 'wire_dly' values are used to create
			 * per-byte-lane read enable VDL adjusts.
			 */
			tmp = ((rd_en_byte_vdl_steps << rd_en_byte_mode) + rd_en_bit_vdl_offset);
			DDR40_PHY_Print(
				"ddr40_phy_init:: Calibrated Read Enable VDL Steps =  0x%X (%d)\n",
				tmp, tmp);

			total_steps0 = ((rd_en_byte_vdl_steps << rd_en_byte_mode) +
				(wire_dly[0] / step_size) + rd_en_bit_vdl_offset);
			total_steps1 = ((rd_en_byte_vdl_steps << rd_en_byte_mode) +
				(wire_dly[1] / step_size) + rd_en_bit_vdl_offset);

			DDR40_PHY_Print("ddr40_phy_init:: BL0 RD_EN adjustment (%d ps): "
				"Total Steps =  0x%X (%d)\n",
				wire_dly[0], total_steps0, total_steps0);
			DDR40_PHY_Print("ddr40_phy_init:: BL1 RD_EN adjustment (%d ps): "
				"Total Steps =  0x%X (%d)\n",
				wire_dly[1], total_steps1, total_steps1);

			ddr40_phy_rd_en_adjust(total_steps0, total_steps1, rd_en_byte_mode,
				offset, (DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_W -
				DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_W));

#ifdef DDR40_WIDTH_IS_32
			total_steps0 = ((rd_en_byte_vdl_steps << rd_en_byte_mode) +
				(wire_dly[2] / step_size) + rd_en_bit_vdl_offset);
			total_steps1 = ((rd_en_byte_vdl_steps << rd_en_byte_mode) +
				(wire_dly[3] / step_size) + rd_en_bit_vdl_offset);
			DDR40_PHY_Print("ddr40_phy_init:: BL2 RD_EN adjustment (%d ps): "
				"Total Steps =  0x%X (%d)\n",
				wire_dly[2], total_steps0, total_steps0);
			DDR40_PHY_Print("ddr40_phy_init:: BL3 RD_EN adjustment (%d ps): "
				"Total Steps =  0x%X (%d)\n",
				wire_dly[3], total_steps1, total_steps1);
			ddr40_phy_rd_en_adjust(total_steps0, total_steps1, rd_en_byte_mode,
				offset, (DDR40_CORE_PHY_WORD_LANE_1_VDL_OVRIDE_BYTE0_W -
				DDR40_CORE_PHY_WORD_LANE_0_VDL_OVRIDE_BYTE0_W));
#endif
#ifdef DDR40_INCLUDE_ECC
			total_steps0 = ((rd_en_byte_vdl_steps << rd_en_byte_mode) +
				(wire_dly[4] / step_size) + rd_en_bit_vdl_offset);
			DDR40_PHY_Print("ddr40_phy_init:: ECC RD_EN adjustment (%d ps): "
				"Total Steps =  0x%X (%d)\n",
				wire_dly[4], total_steps0, total_steps0);
			ddr40_phy_ecc_rd_en_adjust(total_steps0, rd_en_byte_mode, offset);
#endif

		} else {
			DDR40_PHY_Print("ddr40_phy_init:: RD_EN VDL adjustment has been skipped\n");
		}
#endif /* SV_SIM */
		if (params & DDR40_PHY_PARAM_USE_VTT)
			ddr40_phy_vtt_on(connect, override, offset);

		/* run ZQ cal */
		return_code |= ddr40_phy_calib_zq(params, offset);
	}

#ifdef SV_SIM
	if (tmode) {
		ddr40_phy_force_tmode(offset);
	}
#endif

	if ((ddr_type == 1)) {  /* DDR3 */
		ddr40_phy_ddr3_misc(ddr_clk, params, offset);
	}
	else {	/* DDR2 */
		/* DDR-2 (type == 0) requires ODT_EARLY. */
		ddr40_phy_ddr2_misc(ddr_clk, params | DDR40_PHY_PARAM_ODT_EARLY, offset);
	}

	ddr40_phy_set_autoidle(params, offset);

#ifdef SV_SIM
	/* required some delay (really 500 us) from PAD_CTL to CKE enable */
	DDR40_PHY_Timeout(1);
#else
	/* required some delay (really 500 us) from PAD_CTL to CKE enable */
	DDR40_PHY_Timeout(500);
#endif

	DDR40_PHY_Print("DDR Controller PLL Configuration Complete\n");

	return (return_code);
}
