/*
 * Misc utility routines for accessing PMU corerev specific features
 * of the SiliconBackplane-based Broadcom chips.
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
 * $Id: hndpmu.c 530228 2015-01-29 13:35:01Z $
 */


/*
 * Note: this file contains PLL/FLL related functions. A chip can contain multiple PLLs/FLLs.
 * However, in the context of this file the baseband ('BB') PLL/FLL is referred to.
 *
 * Throughout this code, the prefixes 'pmu0_', 'pmu1_' and 'pmu2_' are used.
 * They refer to different revisions of the PMU (which is at revision 18 @ Apr 25, 2012)
 * pmu1_ marks the transition from PLL to ADFLL (Digital Frequency Locked Loop). It supports
 * fractional frequency generation. pmu2_ does not support fractional frequency generation.
 */

#include <bcm_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmdevs.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <hndpmu.h>
#if defined DONGLEBUILD
#include <hndcpu.h>
#endif
#if !defined(BCMDONGLEHOST)
#include <bcmotp.h>
#include "siutils_priv.h"
#endif
#if defined(SAVERESTORE) && !defined(BCMDONGLEHOST)
#include <saverestore.h>
#endif

#ifdef BCMDBG_ERR
#define	PMU_ERROR(args)	printf args
#else
#define	PMU_ERROR(args)
#endif	/* BCMDBG_ERR */

#ifdef BCMDBG
#define	PMU_MSG(args)	printf args
#else
#define	PMU_MSG(args)
#endif	/* BCMDBG */

/* To check in verbose debugging messages not intended
 * to be on except on private builds.
 */
#define	PMU_NONE(args)

/** contains resource bit positions for a specific chip */
struct rsc_per_chip_s {
	uint8 ht_avail;
	uint8 macphy_clkavail;
	uint8 ht_start;
	uint8 otp_pu;
};

typedef struct rsc_per_chip_s rsc_per_chip_t;

#if !defined(BCMDONGLEHOST)
/* PLL controls/clocks */
static void si_pmu0_pllinit0(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal);
static void si_pmu1_pllinit0(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal);
static void si_pmu1_pllinit1(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal);
static void si_pmu2_pllinit0(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal);
static void si_pmu_pll_off(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 *min_mask,
	uint32 *max_mask, uint32 *clk_ctl_st);
static void si_pmu_pll_off_isdone(si_t *sih, osl_t *osh, chipcregs_t *cc);
static void si_pmu_pll_on(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 min_mask,
	uint32 max_mask, uint32 clk_ctl_st);
void si_pmu_otp_pllcontrol(si_t *sih, osl_t *osh);
void si_pmu_otp_regcontrol(si_t *sih, osl_t *osh);
void si_pmu_otp_chipcontrol(si_t *sih, osl_t *osh);
uint32 si_pmu_def_alp_clock(si_t *sih, osl_t *osh);
bool si_pmu_update_pllcontrol(si_t *sih, osl_t *osh, uint32 xtal, bool update_required);
static uint32 si_pmu_htclk_mask(si_t *sih);

static uint32 si_pmu0_alpclk0(si_t *sih, osl_t *osh, chipcregs_t *cc);
static uint32 si_pmu0_cpuclk0(si_t *sih, osl_t *osh, chipcregs_t *cc);
static uint32 si_pmu1_cpuclk0(si_t *sih, osl_t *osh, chipcregs_t *cc);
static uint32 si_pmu1_alpclk0(si_t *sih, osl_t *osh, chipcregs_t *cc);
static uint32 si_pmu2_alpclk0(si_t *sih, osl_t *osh, chipcregs_t *cc);
static uint32 si_pmu2_cpuclk0(si_t *sih, osl_t *osh, chipcregs_t *cc);

/* PMU resources */
static bool si_pmu_res_depfltr_bb(si_t *sih);
static bool si_pmu_res_depfltr_ncb(si_t *sih);
static bool si_pmu_res_depfltr_paldo(si_t *sih);
static bool si_pmu_res_depfltr_npaldo(si_t *sih);
static uint32 si_pmu_res_deps(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 rsrcs, bool all);
static uint si_pmu_res_uptime(si_t *sih, osl_t *osh, chipcregs_t *cc, uint8 rsrc);
static void si_pmu_res_masks(si_t *sih, uint32 *pmin, uint32 *pmax);
static void si_pmu_spuravoid_pllupdate(si_t *sih, chipcregs_t *cc, osl_t *osh, uint8 spuravoid);

void si_pmu_set_4330_plldivs(si_t *sih, uint8 dacrate);
static int8 si_pmu_cbuckout_to_vreg_ctrl(si_t *sih, uint16 cbuck_mv);

int si_pmu_wait_for_steady_state(si_t *sih, osl_t *osh, chipcregs_t *cc);
uint32 si_pmu_get_pmutime_diff(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 *prev);
bool si_pmu_wait_for_res_pending(si_t *sih, osl_t *osh, chipcregs_t *cc, uint usec,
	bool cond, uint32 *elapsed_time);
uint32 si_pmu_get_pmutimer(si_t *sih, osl_t *osh, chipcregs_t *cc);
/* PMU timer ticks once in 32uS */
#define PMU_US_STEPS (32)


void *g_si_pmutmr_lock_arg = NULL;
si_pmu_callback_t g_si_pmutmr_lock_cb = NULL, g_si_pmutmr_unlock_cb = NULL;

/* FVCO frequency */
#define FVCO_880	880000	/* 880MHz */
#define FVCO_1760	1760000	/* 1760MHz */
#define FVCO_1440	1440000	/* 1440MHz */
#define FVCO_960	960000	/* 960MHz */
#define FVCO_963	963000	/* 963MHz */
#define LPO_SEL_TIMEOUT 1000

/* defines to make the code more readable */
#define NO_SUCH_RESOURCE	0	/* means: chip does not have such a PMU resource */

/*
 * access to indirect register interfaces
 */

/** Read/write a chipcontrol reg */
uint32
si_pmu_chipcontrol(si_t *sih, uint reg, uint32 mask, uint32 val)
{
	pmu_corereg(sih, SI_CC_IDX, chipcontrol_addr, ~0, reg);
	return pmu_corereg(sih, SI_CC_IDX, chipcontrol_data, mask, val);
}

/** Read/write a regcontrol reg */
uint32
si_pmu_regcontrol(si_t *sih, uint reg, uint32 mask, uint32 val)
{
	pmu_corereg(sih, SI_CC_IDX, regcontrol_addr, ~0, reg);
	return pmu_corereg(sih, SI_CC_IDX, regcontrol_data, mask, val);
}

/** Read/write a pllcontrol reg */
uint32
si_pmu_pllcontrol(si_t *sih, uint reg, uint32 mask, uint32 val)
{
	pmu_corereg(sih, SI_CC_IDX, pllcontrol_addr, ~0, reg);
	return pmu_corereg(sih, SI_CC_IDX, pllcontrol_data, mask, val);
}

/**
 * The chip has one or more PLLs/FLLs (e.g. baseband PLL, USB PHY PLL). The settings of each PLL are
 * contained within one or more 'PLL control' registers. Since the PLL hardware requires that
 * changes for one PLL are committed at once, the PMU has a provision for 'updating' all PLL control
 * registers at once.
 *
 * When software wants to change the any PLL parameters, it withdraws requests for that PLL clock,
 * updates the PLL control registers being careful not to alter any control signals for the other
 * PLLs, and then writes a 1 to PMUCtl.PllCtnlUpdate to commit the changes. Best usage model would
 * be bring PLL down then update the PLL control register.
 */
void
si_pmu_pllupd(si_t *sih)
{
	pmu_corereg(sih, SI_CC_IDX, pmucontrol,
	           PCTL_PLL_PLLCTL_UPD, PCTL_PLL_PLLCTL_UPD);
}

static rsc_per_chip_t rsc_4313 =  {RES4313_HT_AVAIL_RSRC, RES4313_MACPHY_CLK_AVAIL_RSRC,
	NO_SUCH_RESOURCE,  NO_SUCH_RESOURCE};
static rsc_per_chip_t rsc_4314 =  {RES4314_HT_AVAIL,  RES4314_MACPHY_CLK_AVAIL,
	NO_SUCH_RESOURCE,  RES4314_OTP_PU};
static rsc_per_chip_t rsc_4315 =  {RES4315_HT_AVAIL,  NO_SUCH_RESOURCE,
	NO_SUCH_RESOURCE,  RES4315_OTP_PU};
static rsc_per_chip_t rsc_4319 =  {RES4319_HT_AVAIL,  NO_SUCH_RESOURCE,
	NO_SUCH_RESOURCE,  RES4319_OTP_PU};
static rsc_per_chip_t rsc_4322 =  {NO_SUCH_RESOURCE,  NO_SUCH_RESOURCE,
	NO_SUCH_RESOURCE,  RES4322_OTP_PU};
static rsc_per_chip_t rsc_4324 =  {RES4324_HT_AVAIL,  RES4324_MACPHY_CLKAVAIL,
	NO_SUCH_RESOURCE,  RES4324_OTP_PU};
static rsc_per_chip_t rsc_4325 =  {RES4325_HT_AVAIL,  NO_SUCH_RESOURCE,
	NO_SUCH_RESOURCE,  RES4325_OTP_PU};
static rsc_per_chip_t rsc_4329 =  {RES4329_HT_AVAIL,  NO_SUCH_RESOURCE,
	NO_SUCH_RESOURCE,  RES4329_OTP_PU};
static rsc_per_chip_t rsc_4330 =  {RES4330_HT_AVAIL,  RES4330_MACPHY_CLKAVAIL,
	NO_SUCH_RESOURCE,  RES4330_OTP_PU};
static rsc_per_chip_t rsc_4334 =  {RES4334_HT_AVAIL,  RES4334_MACPHY_CLK_AVAIL,
	NO_SUCH_RESOURCE,  RES4334_OTP_PU};
static rsc_per_chip_t rsc_4335 =  {RES4335_HT_AVAIL,  RES4335_MACPHY_CLKAVAIL,
	RES4335_HT_START,  RES4335_OTP_PU};
static rsc_per_chip_t rsc_4336 =  {RES4336_HT_AVAIL,  RES4336_MACPHY_CLKAVAIL,
	NO_SUCH_RESOURCE,  RES4336_OTP_PU};
static rsc_per_chip_t rsc_4350 =  {RES4350_HT_AVAIL,  RES4350_MACPHY_CLKAVAIL,
	RES4350_HT_START,  RES4350_OTP_PU};
static rsc_per_chip_t rsc_4352 =  {NO_SUCH_RESOURCE,  NO_SUCH_RESOURCE,
	NO_SUCH_RESOURCE,  RES4360_OTP_PU}; /* 4360_OTP_PU is used for 4352, not a typo */
static rsc_per_chip_t rsc_4360 =  {RES4360_HT_AVAIL,  NO_SUCH_RESOURCE,
	NO_SUCH_RESOURCE,  RES4360_OTP_PU};
static rsc_per_chip_t rsc_43143 = {RES43143_HT_AVAIL, RES43143_MACPHY_CLK_AVAIL,
	NO_SUCH_RESOURCE,  RES43143_OTP_PU};
static rsc_per_chip_t rsc_43236 =  {RES43236_HT_SI_AVAIL, NO_SUCH_RESOURCE,
	NO_SUCH_RESOURCE,  NO_SUCH_RESOURCE};
static rsc_per_chip_t rsc_43239 = {RES43239_HT_AVAIL, RES43239_MACPHY_CLKAVAIL,
	NO_SUCH_RESOURCE,  RES43239_OTP_PU};
static rsc_per_chip_t rsc_43602 = {RES43602_HT_AVAIL, RES43602_MACPHY_CLKAVAIL,
	RES43602_HT_START, NO_SUCH_RESOURCE};

/**
* For each chip, location of resource bits (e.g., ht bit) in resource mask registers may differ.
* This function abstracts the bit position of commonly used resources, thus making the rest of the
* code in hndpmu.c cleaner.
*/
static rsc_per_chip_t* si_pmu_get_rsc_positions(si_t *sih)
{
	rsc_per_chip_t *rsc = NULL;

	switch (CHIPID(sih->chip)) {
	case BCM4313_CHIP_ID:
		rsc = &rsc_4313;
		break;
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
		rsc = &rsc_4314;
		break;
	case BCM4315_CHIP_ID:
		rsc = &rsc_4315;
		break;
	case BCM4319_CHIP_ID:
		rsc = &rsc_4319;
		break;
	case BCM4322_CHIP_ID:
	case BCM43221_CHIP_ID:
	case BCM43231_CHIP_ID:
	case BCM4342_CHIP_ID:
		rsc = &rsc_4322;
		break;
	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
		rsc = &rsc_4324;
		break;
	case BCM4325_CHIP_ID:
		rsc = &rsc_4325;
		break;
	case BCM4329_CHIP_ID:
		rsc = &rsc_4329;
		break;
	case BCM4330_CHIP_ID:
		rsc = &rsc_4330;
		break;
	case BCM4331_CHIP_ID: /* fall through */
	case BCM43236_CHIP_ID:
		rsc = &rsc_43236;
		break;
	case BCM4334_CHIP_ID:
	case BCM43341_CHIP_ID:
		rsc = &rsc_4334;
		break;
	case BCM4335_CHIP_ID:
		rsc = &rsc_4335;
		break;
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
		rsc = &rsc_4336;
		break;
	case BCM4345_CHIP_ID:	/* same resource defs for bits in struct rsc_per_chip_s as 4350 */
	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
		rsc = &rsc_4350;
		break;
	case BCM4352_CHIP_ID:
	case BCM43526_CHIP_ID:	/* usb variant of 4352 */
		rsc = &rsc_4352;
		break;
	case BCM4360_CHIP_ID:
	case BCM43460_CHIP_ID:
		rsc = &rsc_4360;
		break;
	case BCM43143_CHIP_ID:
		rsc = &rsc_43143;
		break;
	case BCM43239_CHIP_ID:
		rsc = &rsc_43239;
		break;
	case BCM43602_CHIP_ID:
	case BCM43462_CHIP_ID:
		rsc = &rsc_43602;
		break;
	default:
		ASSERT(0);
		break;
	}

	return rsc;
}; /* si_pmu_get_rsc_positions */


/* This function is not called in at least trunk nor PHOENIX2_BRANCH_6_10. Dead code? */
/** PMU PLL reset */
void
si_pmu_pllreset(si_t *sih)
{
	chipcregs_t *cc;
	uint origidx;
	osl_t *osh;
	uint32 max_res_mask, min_res_mask, clk_ctl_st;

	if (!si_pmu_htclk_mask(sih))
		return;

	osh = si_osh(sih);
	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	si_pmu_pll_off(sih, osh, cc, &min_res_mask, &max_res_mask, &clk_ctl_st);

	OR_REG(osh, PMUREG(sih, pmucontrol), PCTL_PLL_PLLCTL_UPD);
	si_pmu_pll_on(sih, osh, cc, min_res_mask, max_res_mask, clk_ctl_st);

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

static const char BCMATTACHDATA(rstr_pllD)[] = "pll%d";
static const char BCMATTACHDATA(rstr_regD)[] = "reg%d";
static const char BCMATTACHDATA(rstr_chipcD)[] = "chipc%d";
static const char BCMATTACHDATA(rstr_rmin)[] = "rmin";
static const char BCMATTACHDATA(rstr_rmax)[] = "rmax";
static const char BCMATTACHDATA(rstr_rDt)[] = "r%dt";
static const char BCMATTACHDATA(rstr_rDd)[] = "r%dd";
static const char BCMATTACHDATA(rstr_Invalid_Unsupported_xtal_value_D)[] =
	"Invalid/Unsupported xtal value %d";
static const char BCMATTACHDATA(rstr_dacrate2g)[] = "dacrate2g";
static const char BCMATTACHDATA(rstr_clkreq_conf)[] = "clkreq_conf";
static const char BCMATTACHDATA(rstr_cbuckout)[] = "cbuckout";
static const char BCMATTACHDATA(rstr_cldo_ldo2)[] = "cldo_ldo2";
static const char BCMATTACHDATA(rstr_cldo_pwm)[] = "cldo_pwm";
static const char BCMATTACHDATA(rstr_force_pwm_cbuck)[] = "force_pwm_cbuck";
static const char BCMATTACHDATA(rstr_xtalfreq)[] = "xtalfreq";
static const char BCMATTACHDATA(rstr_avs_enab)[] = "avs_enab";

/* The check for OTP parameters for the PLL control registers is done and if found the
 * registers are updated accordingly.
 */

void
BCMATTACHFN(si_pmu_otp_pllcontrol)(si_t *sih, osl_t *osh)
{
	char name[16];
	const char *otp_val;
	uint8 i;
	uint32 val;
	uint8 pll_ctrlcnt = 0;


	if (sih->pmurev >= 5) {
		pll_ctrlcnt = (sih->pmucaps & PCAP5_PC_MASK) >> PCAP5_PC_SHIFT;
	}
	else {
		pll_ctrlcnt = (sih->pmucaps & PCAP_PC_MASK) >> PCAP_PC_SHIFT;
	}

	for (i = 0; i < pll_ctrlcnt; i++) {
		snprintf(name, sizeof(name), rstr_pllD, i);
		if ((otp_val = getvar(NULL, name)) == NULL)
			continue;

		val = (uint32)bcm_strtoul(otp_val, NULL, 0);
		W_REG(osh, PMUREG(sih, pllcontrol_addr), i);
		W_REG(osh, PMUREG(sih, pllcontrol_data), val);
	}
}

/**
 * The check for OTP parameters for the Voltage Regulator registers is done and if found the
 * registers are updated accordingly.
 */
void
BCMATTACHFN(si_pmu_otp_regcontrol)(si_t *sih, osl_t *osh)
{
	char name[16];
	const char *otp_val;
	uint8 i;
	uint32 val;
	uint8 vreg_ctrlcnt = 0;

	if (sih->pmurev >= 5) {
		vreg_ctrlcnt = (sih->pmucaps & PCAP5_VC_MASK) >> PCAP5_VC_SHIFT;
	}
	else {
		vreg_ctrlcnt = (sih->pmucaps & PCAP_VC_MASK) >> PCAP_VC_SHIFT;
	}

	for (i = 0; i < vreg_ctrlcnt; i++) {
		snprintf(name, sizeof(name), rstr_regD, i);
		if ((otp_val = getvar(NULL, name)) == NULL)
			continue;

		val = (uint32)bcm_strtoul(otp_val, NULL, 0);
		W_REG(osh, PMUREG(sih, regcontrol_addr), i);
		W_REG(osh, PMUREG(sih, regcontrol_data), val);
	}
}

/**
 * The check for OTP parameters for the chip control registers is done and if found the
 * registers are updated accordingly.
 */
void
BCMATTACHFN(si_pmu_otp_chipcontrol)(si_t *sih, osl_t *osh)
{
	uint32 val, cc_ctrlcnt, i;
	char name[16];
	const char *otp_val;

	if (sih->pmurev >= 5) {
		cc_ctrlcnt = (sih->pmucaps & PCAP5_CC_MASK) >> PCAP5_CC_SHIFT;
	}
	else {
		cc_ctrlcnt = (sih->pmucaps & PCAP_CC_MASK) >> PCAP_CC_SHIFT;
	}

	for (i = 0; i < cc_ctrlcnt; i++) {
		snprintf(name, sizeof(name), rstr_chipcD, i);
		if ((otp_val = getvar(NULL, name)) == NULL)
			continue;

		val = (uint32)bcm_strtoul(otp_val, NULL, 0);
		W_REG(osh, PMUREG(sih, chipcontrol_addr), i);
		W_REG(osh, PMUREG(sih, chipcontrol_data), val);
	}
}

/** Setup switcher voltage */
void
BCMATTACHFN(si_pmu_set_switcher_voltage)(si_t *sih, osl_t *osh,
                                         uint8 bb_voltage, uint8 rf_voltage)
{
	ASSERT(sih->cccaps & CC_CAP_PMU);

	W_REG(osh, PMUREG(sih, regcontrol_addr), 0x01);
	W_REG(osh, PMUREG(sih, regcontrol_data), (uint32)(bb_voltage & 0x1f) << 22);

	W_REG(osh, PMUREG(sih, regcontrol_addr), 0x00);
	W_REG(osh, PMUREG(sih, regcontrol_data), (uint32)(rf_voltage & 0x1f) << 14);
}

/**
 * A chip contains one or more LDOs (Low Drop Out regulators). During chip bringup, it can turn out
 * that the default (POR) voltage of a regulator is not right or optimal.
 * This function is called only by si_pmu_swreg_init() for specific chips
 */
void
BCMATTACHFN(si_pmu_set_ldo_voltage)(si_t *sih, osl_t *osh, uint8 ldo, uint8 voltage)
{
	uint8 sr_cntl_shift = 0, rc_shift = 0, shift = 0, mask = 0;
	uint8 addr = 0;
	uint8 do_reg2 = 0, rshift2 = 0, mask2 = 0, addr2 = 0;

	ASSERT(sih->cccaps & CC_CAP_PMU);

	switch (CHIPID(sih->chip)) {
	case BCM4328_CHIP_ID:
	case BCM5354_CHIP_ID:
		switch (ldo) {
		case SET_LDO_VOLTAGE_LDO1:
			addr = 2;
			sr_cntl_shift = 8;
			rc_shift = 17;
			mask = 0xf;
			break;
		case SET_LDO_VOLTAGE_LDO2:
			addr = 3;
			rc_shift = 1;
			mask = 0xf;
			break;
		case SET_LDO_VOLTAGE_LDO3:
			addr = 3;
			rc_shift = 9;
			mask = 0xf;
			break;
		case SET_LDO_VOLTAGE_PAREF:
			addr = 3;
			rc_shift = 17;
			mask = 0x3f;
			break;
		default:
			ASSERT(FALSE);
			return;
		}
		break;
	case BCM4312_CHIP_ID:
		switch (ldo) {
		case SET_LDO_VOLTAGE_PAREF:
			addr = 0;
			rc_shift = 21;
			mask = 0x3f;
			break;
		default:
			ASSERT(FALSE);
			return;
		}
		break;
	case BCM4325_CHIP_ID:
		switch (ldo) {
		case SET_LDO_VOLTAGE_CLDO_PWM:
			addr = 5;
			rc_shift = 9;
			mask = 0xf;
			break;
		case SET_LDO_VOLTAGE_CLDO_BURST:
			addr = 5;
			rc_shift = 13;
			mask = 0xf;
			break;
		case SET_LDO_VOLTAGE_CBUCK_PWM:
			addr = 3;
			rc_shift = 20;
			mask = 0x1f;
			/* Bit 116 & 119 are inverted in CLB for opt 2b */
			if (((sih->chipst & CST4325_PMUTOP_2B_MASK) >>
			     CST4325_PMUTOP_2B_SHIFT) == 1)
				voltage ^= 0x9;
			break;
		case SET_LDO_VOLTAGE_CBUCK_BURST:
			addr = 3;
			rc_shift = 25;
			mask = 0x1f;
			/* Bit 121 & 124 are inverted in CLB for opt 2b */
			if (((sih->chipst & CST4325_PMUTOP_2B_MASK) >>
			     CST4325_PMUTOP_2B_SHIFT) == 1)
				voltage ^= 0x9;
			break;
		case SET_LDO_VOLTAGE_LNLDO1:
			addr = 5;
			rc_shift = 17;
			mask = 0x1f;
			break;
		case SET_LDO_VOLTAGE_LNLDO2_SEL:
			addr = 6;
			rc_shift = 0;
			mask = 0x1;
			break;
		default:
			ASSERT(FALSE);
			return;
		}
		break;
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
		switch (ldo) {
		case SET_LDO_VOLTAGE_CLDO_PWM:
			addr = 4;
			rc_shift = 1;
			mask = 0xf;
			break;
		case SET_LDO_VOLTAGE_CLDO_BURST:
			addr = 4;
			rc_shift = 5;
			mask = 0xf;
			break;
		case SET_LDO_VOLTAGE_LNLDO1:
			addr = 4;
			rc_shift = 17;
			mask = 0xf;
			break;
		case SET_LDO_VOLTAGE_CBUCK_PWM:
			addr = 3;
			rc_shift = 0;
			mask = 0x1f;
			break;
		case SET_LDO_VOLTAGE_CBUCK_BURST:
			addr = 3;
			rc_shift = 5;
			mask = 0x1f;
			break;
		case SET_LNLDO_PWERUP_LATCH_CTRL:
			addr = 2;
			rc_shift = 17;
			mask = 0x3;
			break;
		default:
			ASSERT(FALSE);
			return;
		}
		break;
	case BCM4330_CHIP_ID:
		switch (ldo) {
		case SET_LDO_VOLTAGE_CBUCK_PWM:
			addr = 3;
			rc_shift = 0;
			mask = 0x1f;
			break;
		case SET_LDO_VOLTAGE_CBUCK_BURST:
			addr = 3;
			rc_shift = 5;
			mask = 0x1f;
			break;
		default:
			ASSERT(FALSE);
			break;
		}
		break;
	case BCM4331_CHIP_ID:
	case BCM4360_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM4352_CHIP_ID:
	case BCM43526_CHIP_ID:
		switch (ldo) {
		case  SET_LDO_VOLTAGE_PAREF:
			addr = 1;
			rc_shift = 0;
			mask = 0xf;
			break;
		default:
			ASSERT(FALSE);
			break;
		}
		break;
	case BCM43602_CHIP_ID:
	case BCM43462_CHIP_ID:
		switch (ldo) {
		case  SET_LDO_VOLTAGE_PAREF:
			addr = 0;
			rc_shift = 29;
			mask = 0x7;
			do_reg2 = 1;
			addr2 = 1;
			rshift2 = 3;
			mask2 = 0x8;
			break;
		default:
			ASSERT(FALSE);
			break;
		}
		break;
	case BCM4314_CHIP_ID:
		switch (ldo) {
		case  SET_LDO_VOLTAGE_LDO2:
			addr = 4;
			rc_shift = 14;
			mask = 0x7;
			break;
		default:
			ASSERT(FALSE);
			break;
		}
		break;
	case BCM43341_CHIP_ID:
	case BCM4334_CHIP_ID:
		switch (ldo) {
		case SET_LDO_VOLTAGE_LDO1:
			addr = PMU_VREG4_ADDR;
			rc_shift = PMU_VREG4_LPLDO1_SHIFT;
			mask = PMU_VREG4_LPLDO1_MASK;
			break;
		case SET_LDO_VOLTAGE_LDO2:
			addr = PMU_VREG4_ADDR;
			rc_shift = PMU_VREG4_LPLDO2_LVM_SHIFT;
			mask = PMU_VREG4_LPLDO2_LVM_HVM_MASK;
			break;
		case SET_LDO_VOLTAGE_CLDO_PWM:
			addr = PMU_VREG4_ADDR;
			rc_shift = PMU_VREG4_CLDO_PWM_SHIFT;
			mask = PMU_VREG4_CLDO_PWM_MASK;
			break;
		default:
			ASSERT(FALSE);
			break;
		}
		break;
	case BCM43143_CHIP_ID:
		switch (ldo) {
		case SET_LDO_VOLTAGE_CBUCK_PWM:
			addr = 0;
			rc_shift = 5;
			mask = 0xf;
			break;
		case SET_LDO_VOLTAGE_CBUCK_BURST:
			addr = 4;
			rc_shift = 8;
			mask = 0xf;
			break;
		case SET_LDO_VOLTAGE_LNLDO1:
			addr = 4;
			rc_shift = 14;
			mask = 0x7;
			break;
		default:
			ASSERT(FALSE);
			break;
		}
		break;
	default:
		ASSERT(FALSE);
		return;
	}

	shift = sr_cntl_shift + rc_shift;

	pmu_corereg(sih, SI_CC_IDX, regcontrol_addr, /* PMU VREG register */
		~0, addr);
	pmu_corereg(sih, SI_CC_IDX, regcontrol_data,
		mask << shift, (voltage & mask) << shift);
	if (do_reg2) {
		si_pmu_regcontrol(sih, addr2, mask2 >> rshift2, (voltage & mask2) >> rshift2);
	}
} /* si_pmu_set_ldo_voltage */

void
si_pmu_paref_ldo_enable(si_t *sih, osl_t *osh, bool enable)
{
	uint ldo = 0;

	ASSERT(sih->cccaps & CC_CAP_PMU);

	switch (CHIPID(sih->chip)) {
	case BCM4328_CHIP_ID:
		ldo = RES4328_PA_REF_LDO;
		break;
	case BCM5354_CHIP_ID:
		ldo = RES5354_PA_REF_LDO;
		break;
	case BCM4312_CHIP_ID:
		ldo = RES4312_PA_REF_LDO;
		break;
	default:
		return;
	}

	pmu_corereg(sih, SI_CC_IDX, min_res_mask,
	           PMURES_BIT(ldo), enable ? PMURES_BIT(ldo) : 0);
}

/* d11 slow to fast clock transition time in slow clock cycles */
#define D11SCC_SLOW2FAST_TRANSITION	2

/**
 * d11 core has a 'fastpwrup_dly' register that must be written to.
 * This function returns d11 slow to fast clock transition time in [us] units.
 * It does not write to the d11 core.
 */
uint16
BCMINITFN(si_pmu_fast_pwrup_delay)(si_t *sih, osl_t *osh)
{
	uint pmudelay = PMU_MAX_TRANSITION_DLY;
	chipcregs_t *cc;
	uint origidx;
	uint32 ilp;			/* ILP clock frequency in [Hz] */
	rsc_per_chip_t *rsc;		/* chip specific resource bit positions */

	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	if (ISSIM_ENAB(sih)) {
		pmudelay = 70;
	} else {
		switch (CHIPID(sih->chip)) {
		case BCM4312_CHIP_ID:
		case BCM4322_CHIP_ID:	case BCM43221_CHIP_ID:	case BCM43231_CHIP_ID:
		case BCM43222_CHIP_ID:	case BCM43111_CHIP_ID:	case BCM43112_CHIP_ID:
		case BCM43224_CHIP_ID:	case BCM43225_CHIP_ID:  case BCM43420_CHIP_ID:
		case BCM43421_CHIP_ID:
		case BCM43226_CHIP_ID:
		case BCM43235_CHIP_ID:	case BCM43236_CHIP_ID:	case BCM43238_CHIP_ID:
		case BCM43237_CHIP_ID:	case BCM43239_CHIP_ID:
		case BCM43234_CHIP_ID:
		case BCM4331_CHIP_ID:
		case BCM43431_CHIP_ID:
		case BCM43131_CHIP_ID:
		case BCM43217_CHIP_ID:
		case BCM43227_CHIP_ID:
		case BCM43228_CHIP_ID:
		case BCM43428_CHIP_ID:
		case BCM6362_CHIP_ID:
		case BCM4342_CHIP_ID:
		case BCM4313_CHIP_ID:
		case BCM43460_CHIP_ID:
		case BCM43526_CHIP_ID:
		case BCM4319_CHIP_ID:
			pmudelay = 3700;
			break;
		case BCM4360_CHIP_ID:
		case BCM4352_CHIP_ID:
			if (CHIPREV(sih->chiprev) < 4) {
				pmudelay = 1500;
			} else {
				pmudelay = 3000;
			}
			break;
		case BCM4328_CHIP_ID:
			pmudelay = 7000;
			break;
		case BCM4325_CHIP_ID:
		case BCM4329_CHIP_ID:
		case BCM4315_CHIP_ID:
		case BCM4336_CHIP_ID:
		case BCM43362_CHIP_ID:
		case BCM4330_CHIP_ID:
		case BCM4314_CHIP_ID:
		case BCM43142_CHIP_ID:
		case BCM43143_CHIP_ID:
		case BCM43341_CHIP_ID:
		case BCM4334_CHIP_ID:
		case BCM4345_CHIP_ID:
		case BCM43602_CHIP_ID:
		case BCM43462_CHIP_ID:
		case BCM4350_CHIP_ID:
		case BCM4354_CHIP_ID:
		case BCM4356_CHIP_ID:
		case BCM43556_CHIP_ID:
		case BCM43558_CHIP_ID:
		case BCM43566_CHIP_ID:
		case BCM43568_CHIP_ID:
		case BCM43569_CHIP_ID:
		case BCM43570_CHIP_ID:
		case BCM4324_CHIP_ID:
		case BCM43242_CHIP_ID:
		case BCM43243_CHIP_ID:
			rsc = si_pmu_get_rsc_positions(sih);
			/* Retrieve time by reading it out of the hardware */
			ilp = si_ilp_clock(sih);
			pmudelay = (si_pmu_res_uptime(sih, osh, cc, rsc->ht_avail) +
				D11SCC_SLOW2FAST_TRANSITION) * ((1000000 + ilp - 1) / ilp);
			pmudelay = (11 * pmudelay) / 10;
			break;
		case BCM4335_CHIP_ID:
			rsc = si_pmu_get_rsc_positions(sih);
			ilp = si_ilp_clock(sih);
			pmudelay = (si_pmu_res_uptime(sih, osh, cc, rsc->ht_avail) +
				D11SCC_SLOW2FAST_TRANSITION) * ((1000000 + ilp - 1) / ilp);
			/* Adding error margin of fixed 200usec instead of 10% */
			pmudelay = pmudelay + 200;
			break;

		default:
			break;
		}
	} /* if (ISSIM_ENAB(sih)) */

	/* Return to original core */
	si_setcoreidx(sih, origidx);

	return (uint16)pmudelay;
} /* si_pmu_fast_pwrup_delay */

uint32
BCMATTACHFN(si_pmu_force_ilp)(si_t *sih, osl_t *osh, bool force)
{
	uint32 oldpmucontrol;

	ASSERT(sih->cccaps & CC_CAP_PMU);

	oldpmucontrol = R_REG(osh, PMUREG(sih, pmucontrol));
	if (force)
		W_REG(osh, PMUREG(sih, pmucontrol), oldpmucontrol &
			~(PCTL_HT_REQ_EN | PCTL_ALP_REQ_EN));
	else
		W_REG(osh, PMUREG(sih, pmucontrol), oldpmucontrol |
			(PCTL_HT_REQ_EN | PCTL_ALP_REQ_EN));


	return oldpmucontrol;
}

/*
 * During chip bringup, it can turn out that the 'hard wired' PMU dependencies are not fully
 * correct, or that up/down time values can be optimized. The following data structures and arrays
 * deal with that.
 */

/* Setup resource up/down timers */
typedef struct {
	uint8 resnum;
	uint32 updown;
} pmu_res_updown_t;

/* Change resource dependencies masks */
typedef struct {
	uint32 res_mask;		/* resources (chip specific) */
	int8 action;			/* action, e.g. RES_DEPEND_SET */
	uint32 depend_mask;		/* changes to the dependencies mask */
	bool (*filter)(si_t *sih);	/* action is taken when filter is NULL or return TRUE */
} pmu_res_depend_t;

/* Resource dependencies mask change action */
#define RES_DEPEND_SET		0	/* Override the dependencies mask */
#define RES_DEPEND_ADD		1	/* Add to the  dependencies mask */
#define RES_DEPEND_REMOVE	-1	/* Remove from the dependencies mask */

static const pmu_res_updown_t BCMATTACHDATA(bcm4328a0_res_updown)[] = {
	{ RES4328_EXT_SWITCHER_PWM, 0x0101 },
	{ RES4328_BB_SWITCHER_PWM, 0x1f01 },
	{ RES4328_BB_SWITCHER_BURST, 0x010f },
	{ RES4328_BB_EXT_SWITCHER_BURST, 0x0101 },
	{ RES4328_ILP_REQUEST, 0x0202 },
	{ RES4328_RADIO_SWITCHER_PWM, 0x0f01 },
	{ RES4328_RADIO_SWITCHER_BURST, 0x0f01 },
	{ RES4328_ROM_SWITCH, 0x0101 },
	{ RES4328_PA_REF_LDO, 0x0f01 },
	{ RES4328_RADIO_LDO, 0x0f01 },
	{ RES4328_AFE_LDO, 0x0f01 },
	{ RES4328_PLL_LDO, 0x0f01 },
	{ RES4328_BG_FILTBYP, 0x0101 },
	{ RES4328_TX_FILTBYP, 0x0101 },
	{ RES4328_RX_FILTBYP, 0x0101 },
	{ RES4328_XTAL_PU, 0x0101 },
	{ RES4328_XTAL_EN, 0xa001 },
	{ RES4328_BB_PLL_FILTBYP, 0x0101 },
	{ RES4328_RF_PLL_FILTBYP, 0x0101 },
	{ RES4328_BB_PLL_PU, 0x0701 }
};

static const pmu_res_depend_t BCMATTACHDATA(bcm4328a0_res_depend)[] = {
	/* Adjust ILP request resource not to force ext/BB switchers into burst mode */
	{
		PMURES_BIT(RES4328_ILP_REQUEST),
		RES_DEPEND_SET,
		PMURES_BIT(RES4328_EXT_SWITCHER_PWM) | PMURES_BIT(RES4328_BB_SWITCHER_PWM),
		NULL
	}
};

static const pmu_res_updown_t BCMATTACHDATA(bcm4325a0_res_updown_qt)[] = {
	{ RES4325_HT_AVAIL, 0x0300 },
	{ RES4325_BBPLL_PWRSW_PU, 0x0101 },
	{ RES4325_RFPLL_PWRSW_PU, 0x0101 },
	{ RES4325_ALP_AVAIL, 0x0100 },
	{ RES4325_XTAL_PU, 0x1000 },
	{ RES4325_LNLDO1_PU, 0x0800 },
	{ RES4325_CLDO_CBUCK_PWM, 0x0101 },
	{ RES4325_CBUCK_PWM, 0x0803 }
};

static const pmu_res_updown_t BCMATTACHDATA(bcm4325a0_res_updown)[] = {
	{ RES4325_XTAL_PU, 0x1501 }
};

static const pmu_res_depend_t BCMATTACHDATA(bcm4325a0_res_depend)[] = {
	/* Adjust OTP PU resource dependencies - remove BB BURST */
	{
		PMURES_BIT(RES4325_OTP_PU),
		RES_DEPEND_REMOVE,
		PMURES_BIT(RES4325_BUCK_BOOST_BURST),
		NULL
	},
	/* Adjust ALP/HT Avail resource dependencies - bring up BB along if it is used. */
	{
		PMURES_BIT(RES4325_ALP_AVAIL) | PMURES_BIT(RES4325_HT_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4325_BUCK_BOOST_BURST) | PMURES_BIT(RES4325_BUCK_BOOST_PWM),
		si_pmu_res_depfltr_bb
	},
	/* Adjust HT Avail resource dependencies - bring up RF switches along with HT. */
	{
		PMURES_BIT(RES4325_HT_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4325_RX_PWRSW_PU) | PMURES_BIT(RES4325_TX_PWRSW_PU) |
		PMURES_BIT(RES4325_LOGEN_PWRSW_PU) | PMURES_BIT(RES4325_AFE_PWRSW_PU),
		NULL
	},
	/* Adjust ALL resource dependencies - remove CBUCK dependencies if it is not used. */
	{
		PMURES_BIT(RES4325_ILP_REQUEST) | PMURES_BIT(RES4325_ABUCK_BURST) |
		PMURES_BIT(RES4325_ABUCK_PWM) | PMURES_BIT(RES4325_LNLDO1_PU) |
		PMURES_BIT(RES4325C1_LNLDO2_PU) | PMURES_BIT(RES4325_XTAL_PU) |
		PMURES_BIT(RES4325_ALP_AVAIL) | PMURES_BIT(RES4325_RX_PWRSW_PU) |
		PMURES_BIT(RES4325_TX_PWRSW_PU) | PMURES_BIT(RES4325_RFPLL_PWRSW_PU) |
		PMURES_BIT(RES4325_LOGEN_PWRSW_PU) | PMURES_BIT(RES4325_AFE_PWRSW_PU) |
		PMURES_BIT(RES4325_BBPLL_PWRSW_PU) | PMURES_BIT(RES4325_HT_AVAIL),
		RES_DEPEND_REMOVE,
		PMURES_BIT(RES4325B0_CBUCK_LPOM) | PMURES_BIT(RES4325B0_CBUCK_BURST) |
		PMURES_BIT(RES4325B0_CBUCK_PWM),
		si_pmu_res_depfltr_ncb
	}
};

static const pmu_res_updown_t BCMATTACHDATA(bcm4315a0_res_updown_qt)[] = {
	{ RES4315_HT_AVAIL, 0x0101 },
	{ RES4315_XTAL_PU, 0x0100 },
	{ RES4315_LNLDO1_PU, 0x0100 },
	{ RES4315_PALDO_PU, 0x0100 },
	{ RES4315_CLDO_PU, 0x0100 },
	{ RES4315_CBUCK_PWM, 0x0100 },
	{ RES4315_CBUCK_BURST, 0x0100 },
	{ RES4315_CBUCK_LPOM, 0x0100 }
};

static const pmu_res_updown_t BCMATTACHDATA(bcm4315a0_res_updown)[] = {
	{ RES4315_XTAL_PU, 0x2501 }
};

static const pmu_res_depend_t BCMATTACHDATA(bcm4315a0_res_depend)[] = {
	/* Adjust OTP PU resource dependencies - not need PALDO unless write */
	{
		PMURES_BIT(RES4315_OTP_PU),
		RES_DEPEND_REMOVE,
		PMURES_BIT(RES4315_PALDO_PU),
		si_pmu_res_depfltr_npaldo
	},
	/* Adjust ALP/HT Avail resource dependencies - bring up PALDO along if it is used. */
	{
		PMURES_BIT(RES4315_ALP_AVAIL) | PMURES_BIT(RES4315_HT_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4315_PALDO_PU),
		si_pmu_res_depfltr_paldo
	},
	/* Adjust HT Avail resource dependencies - bring up RF switches along with HT. */
	{
		PMURES_BIT(RES4315_HT_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4315_RX_PWRSW_PU) | PMURES_BIT(RES4315_TX_PWRSW_PU) |
		PMURES_BIT(RES4315_LOGEN_PWRSW_PU) | PMURES_BIT(RES4315_AFE_PWRSW_PU),
		NULL
	},
	/* Adjust ALL resource dependencies - remove CBUCK dependencies if it is not used. */
	{
		PMURES_BIT(RES4315_CLDO_PU) | PMURES_BIT(RES4315_ILP_REQUEST) |
		PMURES_BIT(RES4315_LNLDO1_PU) | PMURES_BIT(RES4315_OTP_PU) |
		PMURES_BIT(RES4315_LNLDO2_PU) | PMURES_BIT(RES4315_XTAL_PU) |
		PMURES_BIT(RES4315_ALP_AVAIL) | PMURES_BIT(RES4315_RX_PWRSW_PU) |
		PMURES_BIT(RES4315_TX_PWRSW_PU) | PMURES_BIT(RES4315_RFPLL_PWRSW_PU) |
		PMURES_BIT(RES4315_LOGEN_PWRSW_PU) | PMURES_BIT(RES4315_AFE_PWRSW_PU) |
		PMURES_BIT(RES4315_BBPLL_PWRSW_PU) | PMURES_BIT(RES4315_HT_AVAIL),
		RES_DEPEND_REMOVE,
		PMURES_BIT(RES4315_CBUCK_LPOM) | PMURES_BIT(RES4315_CBUCK_BURST) |
		PMURES_BIT(RES4315_CBUCK_PWM),
		si_pmu_res_depfltr_ncb
	}
};

static const pmu_res_updown_t BCMATTACHDATA(bcm4329_res_updown)[] = {
	{ RES4329_XTAL_PU, 0x3201 },
	{ RES4329_PALDO_PU, 0x3501 }
};

static const pmu_res_depend_t BCMATTACHDATA(bcm4329_res_depend)[] = {
	/* Make lnldo1 independent of CBUCK_PWM and CBUCK_BURST */
	{
		PMURES_BIT(RES4329_LNLDO1_PU),
		RES_DEPEND_REMOVE,
		PMURES_BIT(RES4329_CBUCK_PWM) | PMURES_BIT(RES4329_CBUCK_BURST),
		NULL
	},
	{
		PMURES_BIT(RES4329_CBUCK_BURST),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4329_CBUCK_LPOM) | PMURES_BIT(RES4329_PALDO_PU),
		NULL
	},
	{
		PMURES_BIT(RES4329_BBPLL_PWRSW_PU),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4329_RX_PWRSW_PU) | PMURES_BIT(RES4329_TX_PWRSW_PU) |
		PMURES_BIT(RES4329_LOGEN_PWRSW_PU) | PMURES_BIT(RES4329_AFE_PWRSW_PU),
		NULL
	},
	/* Adjust HT Avail resource dependencies */
	{
		PMURES_BIT(RES4329_HT_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4329_PALDO_PU) |
		PMURES_BIT(RES4329_RX_PWRSW_PU) | PMURES_BIT(RES4329_TX_PWRSW_PU) |
		PMURES_BIT(RES4329_LOGEN_PWRSW_PU) | PMURES_BIT(RES4329_AFE_PWRSW_PU),
		NULL
	}
};

static const pmu_res_updown_t BCMATTACHDATA(bcm4319a0_res_updown_qt)[] = {
	{ RES4319_HT_AVAIL, 0x0101 },
	{ RES4319_XTAL_PU, 0x0100 },
	{ RES4319_LNLDO1_PU, 0x0100 },
	{ RES4319_PALDO_PU, 0x0100 },
	{ RES4319_CLDO_PU, 0x0100 },
	{ RES4319_CBUCK_PWM, 0x0100 },
	{ RES4319_CBUCK_BURST, 0x0100 },
	{ RES4319_CBUCK_LPOM, 0x0100 }
};

static const pmu_res_updown_t BCMATTACHDATA(bcm4319a0_res_updown)[] = {
	{ RES4319_XTAL_PU, 0x3f01 }
};

static const pmu_res_depend_t BCMATTACHDATA(bcm4319a0_res_depend)[] = {
	/* Adjust OTP PU resource dependencies - not need PALDO unless write */
	{
		PMURES_BIT(RES4319_OTP_PU),
		RES_DEPEND_REMOVE,
		PMURES_BIT(RES4319_PALDO_PU),
		si_pmu_res_depfltr_npaldo
	},
	/* Adjust HT Avail resource dependencies - bring up PALDO along if it is used. */
	{
		PMURES_BIT(RES4319_HT_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4319_PALDO_PU),
		si_pmu_res_depfltr_paldo
	},
	/* Adjust HT Avail resource dependencies - bring up RF switches along with HT. */
	{
		PMURES_BIT(RES4319_HT_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4319_RX_PWRSW_PU) | PMURES_BIT(RES4319_TX_PWRSW_PU) |
		PMURES_BIT(RES4319_RFPLL_PWRSW_PU) |
		PMURES_BIT(RES4319_LOGEN_PWRSW_PU) | PMURES_BIT(RES4319_AFE_PWRSW_PU),
		NULL
	}
};

static const pmu_res_updown_t BCMATTACHDATA(bcm4336a0_res_updown_qt)[] = {
	{ RES4336_HT_AVAIL, 0x0101 },
	{ RES4336_XTAL_PU, 0x0100 },
	{ RES4336_CLDO_PU, 0x0100 },
	{ RES4336_CBUCK_PWM, 0x0100 },
	{ RES4336_CBUCK_BURST, 0x0100 },
	{ RES4336_CBUCK_LPOM, 0x0100 }
};

static const pmu_res_updown_t BCMATTACHDATA(bcm4336a0_res_updown)[] = {
	{ RES4336_HT_AVAIL, 0x0D01}
};

static const pmu_res_depend_t BCMATTACHDATA(bcm4336a0_res_depend)[] = {
	/* Just a dummy entry for now */
	{
		PMURES_BIT(RES4336_RSVD),
		RES_DEPEND_ADD,
		0,
		NULL
	}
};

static const pmu_res_updown_t BCMATTACHDATA(bcm4330a0_res_updown_qt)[] = {
	{ RES4330_HT_AVAIL, 0x0101 },
	{ RES4330_XTAL_PU, 0x0100 },
	{ RES4330_CLDO_PU, 0x0100 },
	{ RES4330_CBUCK_PWM, 0x0100 },
	{ RES4330_CBUCK_BURST, 0x0100 },
	{ RES4330_CBUCK_LPOM, 0x0100 }
};

static const pmu_res_updown_t BCMATTACHDATA(bcm4330a0_res_updown)[] = {
	{ RES4330_HT_AVAIL, 0x0e02}
};

static const pmu_res_depend_t BCMATTACHDATA(bcm4330a0_res_depend)[] = {
	/* Just a dummy entry for now */
	{
		PMURES_BIT(RES4330_HT_AVAIL),
		RES_DEPEND_ADD,
		0,
		NULL
	}
};

static const pmu_res_updown_t BCMATTACHDATA(bcm4334a0_res_updown_qt)[] = {{0, 0}};
static const pmu_res_updown_t BCMATTACHDATA(bcm4334a0_res_updown)[] = {{0, 0}};
static const pmu_res_depend_t BCMATTACHDATA(bcm4334a0_res_depend)[] = {{0, 0, 0, NULL}};

static const pmu_res_updown_t BCMATTACHDATA(bcm4324a0_res_updown_qt)[] = {
	{RES4324_SR_SAVE_RESTORE, 0x00320032}
};

static const pmu_res_updown_t BCMATTACHDATA(bcm4324a0_res_updown)[] = {
	{RES4324_SR_SAVE_RESTORE, 0x00020002},
#ifdef BCMUSBDEV_ENABLED
	{RES4324_XTAL_PU, 0x007d0001}
#else
	{RES4324_XTAL_PU, 0x00120001}
#endif
};

static const pmu_res_updown_t BCMATTACHDATA(bcm4324b0_res_updown)[] = {
	{RES4324_SR_SAVE_RESTORE, 0x00050005},
	{RES4324_XTAL_PU, 0x00120001}
};

static const pmu_res_updown_t BCMATTACHDATA(bcm4335_res_updown)[] = {
	{RES4335_XTAL_PU, 0x00260002},
	{RES4335_ALP_AVAIL, 0x00020005},
	{RES4335_WL_CORE_RDY, 0x00020005},
	{RES4335_LQ_START, 0x00060002},
	{RES4335_SR_CLK_START, 0x00060002}
};
static const pmu_res_depend_t BCMATTACHDATA(bcm4335b0_res_depend)[] = {
	{
		PMURES_BIT(RES4335_ALP_AVAIL) |
		PMURES_BIT(RES4335_HT_START) |
		PMURES_BIT(RES4335_HT_AVAIL) |
		PMURES_BIT(RES4335_MACPHY_CLKAVAIL) |
		PMURES_BIT(RES4335_RADIO_PU),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4335_OTP_PU) | PMURES_BIT(RES4335_LDO3P3_PU),
		NULL
	},
	{
		PMURES_BIT(RES4335_MINI_PMU),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4335_XTAL_PU),
		NULL
	}
};

static const pmu_res_updown_t BCMATTACHDATA(bcm4350_res_updown)[] = {
#if defined(SAVERESTORE) && !defined(SAVERESTORE_DISABLED)
	{RES4350_SR_SAVE_RESTORE,	0x00190019},
#endif /* SAVERESTORE && !SAVERESTORE_DISABLED */

	{RES4350_XTAL_PU,		0x00210001},
	{RES4350_LQ_AVAIL,		0x00010001},
	{RES4350_LQ_START,		0x00010001},
	{RES4350_WL_CORE_RDY,		0x00010001},
	{RES4350_ALP_AVAIL,		0x00010001},
	{RES4350_SR_CLK_STABLE,		0x00010001},
	{RES4350_SR_SLEEP,		0x00010001},
	{RES4350_HT_AVAIL,		0x00010001},

#ifndef SRFAST
	{RES4350_SR_PHY_PWRSW,		0x00120002},
	{RES4350_SR_VDDM_PWRSW,		0x00120002},
	{RES4350_SR_SUBCORE_PWRSW,	0x00120002},
#else /* SRFAST */
	{RES4350_PMU_BG_PU,		0x00010001},
	{RES4350_PMU_SLEEP,		0x00100004},
	{RES4350_CBUCK_LPOM_PU,		0x00010001},
	{RES4350_CBUCK_PFM_PU,		0x00010001},
	{RES4350_COLD_START_WAIT,	0x00010001},
	{RES4350_LNLDO_PU,		0x00100001},
	{RES4350_XTALLDO_PU,		0x00050001},
	{RES4350_LDO3P3_PU,		0x00100001},
	{RES4350_OTP_PU,		0x00010001},
	{RES4350_SR_CLK_START,		0x00020001},
	{RES4350_PERST_OVR,		0x00010001},
	{RES4350_WL_CORE_RDY,		0x00010001},
	{RES4350_ALP_AVAIL,		0x00010001},
	{RES4350_MINI_PMU,		0x00010001},
	{RES4350_RADIO_PU,		0x00010001},
	{RES4350_SR_CLK_STABLE,		0x00010001},
	{RES4350_SR_SLEEP,		0x00010001},
	{RES4350_HT_START,		0x00050001},
	{RES4350_HT_AVAIL,		0x00010001},
	{RES4350_MACPHY_CLKAVAIL,	0x00010001},
#endif /* SRFAST */
};

static const pmu_res_updown_t BCMATTACHDATA(bcm43602_res_updown)[] = {
	{RES43602_SR_SAVE_RESTORE,	0x00240024},
	{RES43602_XTAL_PU,		0x00280002},
};

static const pmu_res_depend_t BCMATTACHDATA(bcm4350_res_depend)[] = {
#ifdef SRFAST
	{
		PMURES_BIT(RES4350_LDO3P3_PU),
		RES_DEPEND_REMOVE,
		PMURES_BIT(RES4350_PMU_SLEEP),
		NULL
	},
	{
		PMURES_BIT(RES4350_COLD_START_WAIT) |
		PMURES_BIT(RES4350_XTALLDO_PU) |
		PMURES_BIT(RES4350_XTAL_PU) |
		PMURES_BIT(RES4350_SR_CLK_START) |
		PMURES_BIT(RES4350_WL_CORE_RDY) |
		PMURES_BIT(RES4350_ALP_AVAIL) |
		PMURES_BIT(RES4350_SR_CLK_STABLE) |
		PMURES_BIT(RES4350_SR_SAVE_RESTORE) |
		PMURES_BIT(RES4350_SR_PHY_PWRSW) |
		PMURES_BIT(RES4350_SR_VDDM_PWRSW) |
		PMURES_BIT(RES4350_SR_SUBCORE_PWRSW) |
		PMURES_BIT(RES4350_SR_SLEEP) |
		PMURES_BIT(RES4350_HT_START) |
		PMURES_BIT(RES4350_HT_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4350_LDO3P3_PU),
		NULL
	},
	{
		PMURES_BIT(RES4350_LNLDO_PU),
		RES_DEPEND_REMOVE,
		PMURES_BIT(RES4350_PMU_SLEEP) |
		PMURES_BIT(RES4350_CBUCK_LPOM_PU) |
		PMURES_BIT(RES4350_CBUCK_PFM_PU),
		NULL
	},
	{
		PMURES_BIT(RES4350_ALP_AVAIL) |
		PMURES_BIT(RES4350_RADIO_PU) |
		PMURES_BIT(RES4350_HT_START) |
		PMURES_BIT(RES4350_HT_AVAIL) |
		PMURES_BIT(RES4350_MACPHY_CLKAVAIL),
		RES_DEPEND_REMOVE,
		PMURES_BIT(RES4350_LQ_AVAIL) |
		PMURES_BIT(RES4350_LQ_START),
		NULL
	},
#else
	{0, 0, 0, NULL}
#endif /* SRFAST */
};

#ifndef BCM_BOOTLOADER
static const pmu_res_depend_t BCMATTACHDATA(bcm4350_res_pciewar)[] = {
	{
		PMURES_BIT(RES4350_LQ_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4350_LDO3P3_PU) |
		PMURES_BIT(RES4350_OTP_PU),
		NULL
	},
	{
		PMURES_BIT(RES4350_LQ_START),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4350_LDO3P3_PU) |
		PMURES_BIT(RES4350_OTP_PU),
		NULL
	},
	{
		PMURES_BIT(RES4350_PERST_OVR),
		RES_DEPEND_SET,
		PMURES_BIT(RES4350_LPLDO_PU) |
		PMURES_BIT(RES4350_PMU_BG_PU) |
		PMURES_BIT(RES4350_PMU_SLEEP) |
		PMURES_BIT(RES4350_CBUCK_LPOM_PU) |
		PMURES_BIT(RES4350_CBUCK_PFM_PU) |
		PMURES_BIT(RES4350_COLD_START_WAIT) |
		PMURES_BIT(RES4350_LNLDO_PU) |
		PMURES_BIT(RES4350_XTALLDO_PU) |
		PMURES_BIT(RES4350_XTAL_PU) |
		PMURES_BIT(RES4350_SR_CLK_STABLE) |
		PMURES_BIT(RES4350_SR_SAVE_RESTORE) |
		PMURES_BIT(RES4350_SR_PHY_PWRSW) |
		PMURES_BIT(RES4350_SR_VDDM_PWRSW) |
		PMURES_BIT(RES4350_SR_SUBCORE_PWRSW) |
		PMURES_BIT(RES4350_SR_SLEEP),
		NULL
	},
	{
		PMURES_BIT(RES4350_WL_CORE_RDY),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4350_PERST_OVR) |
		PMURES_BIT(RES4350_LDO3P3_PU) |
		PMURES_BIT(RES4350_OTP_PU),
		NULL
	},
	{
		PMURES_BIT(RES4350_ILP_REQ),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4350_LDO3P3_PU) |
		PMURES_BIT(RES4350_OTP_PU),
		NULL
	},
	{
		PMURES_BIT(RES4350_ALP_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4350_PERST_OVR) |
		PMURES_BIT(RES4350_LDO3P3_PU) |
		PMURES_BIT(RES4350_OTP_PU),
		NULL
	},
	{
		PMURES_BIT(RES4350_RADIO_PU),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4350_PERST_OVR) |
		PMURES_BIT(RES4350_LDO3P3_PU) |
		PMURES_BIT(RES4350_OTP_PU),
		NULL
	},
	{
		PMURES_BIT(RES4350_SR_CLK_STABLE),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4350_LDO3P3_PU) |
		PMURES_BIT(RES4350_OTP_PU),
		NULL
	},
	{
		PMURES_BIT(RES4350_SR_SAVE_RESTORE),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4350_LDO3P3_PU) |
		PMURES_BIT(RES4350_OTP_PU),
		NULL
	},
	{
		PMURES_BIT(RES4350_SR_SUBCORE_PWRSW),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4350_LDO3P3_PU) |
		PMURES_BIT(RES4350_OTP_PU),
		NULL
	},
	{
		PMURES_BIT(RES4350_SR_SLEEP),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4350_LDO3P3_PU) |
		PMURES_BIT(RES4350_OTP_PU),
		NULL
	},
	{
		PMURES_BIT(RES4350_HT_START),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4350_PERST_OVR) |
		PMURES_BIT(RES4350_LDO3P3_PU) |
		PMURES_BIT(RES4350_OTP_PU),
		NULL
	},
	{
		PMURES_BIT(RES4350_HT_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4350_PERST_OVR) |
		PMURES_BIT(RES4350_LDO3P3_PU) |
		PMURES_BIT(RES4350_OTP_PU),
		NULL
	},
	{
		PMURES_BIT(RES4350_MACPHY_CLKAVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4350_PERST_OVR) |
		PMURES_BIT(RES4350_LDO3P3_PU) |
		PMURES_BIT(RES4350_OTP_PU),
		NULL
	}
};
#endif /* !BCM_BOOTLOADER */

static const pmu_res_updown_t BCMATTACHDATA(bcm4360_res_updown)[] = {
	{RES4360_BBPLLPWRSW_PU,		0x00200001}
};

static const pmu_res_updown_t BCMATTACHDATA(bcm4345_res_updown)[] = {
#if defined(SAVERESTORE) && !defined(SAVERESTORE_DISABLED)
#ifndef SRFAST
	{RES4345_SR_SAVE_RESTORE,       0x00170017},
#else
	{RES4345_SR_SAVE_RESTORE,       0x000A000A},
	{RES4345_RSVD_7,                0x000c0001},
	{RES4345_PMU_BG_PU,		0x00010001},
	{RES4345_PMU_SLEEP,		0x000A0004},
	{RES4345_CBUCK_LPOM_PU,		0x00010001},
	{RES4345_CBUCK_PFM_PU,		0x00010001},
	{RES4345_COLD_START_WAIT,	0x00010001},
	{RES4345_LNLDO_PU,		0x000A0001},
	{RES4345_XTALLDO_PU,		0x00050001},
	{RES4345_XTAL_PU,		0x002C0001},
	{RES4345_LDO3P3_PU,		0x000A0001},
	{RES4345_OTP_PU,		0x00010001},
	{RES4345_SR_CLK_START,		0x00020001},
	{RES4345_PERST_OVR,		0x00010001},
	{RES4345_WL_CORE_RDY,		0x00010001},
	{RES4345_ALP_AVAIL,		0x00010001},
	{RES4345_MINI_PMU,		0x00010001},
	{RES4345_RADIO_PU,		0x00010001},
	{RES4345_SR_CLK_STABLE,		0x00010001},
	{RES4345_SR_SLEEP,		0x00010001},
	{RES4345_HT_START,		0x00050001},
	{RES4345_HT_AVAIL,		0x00010001},
	{RES4345_MACPHY_CLKAVAIL,	0x00010001},
#endif /* SRFAST */
#endif /* SAVERESTORE && !SAVERESTORE_DISABLED */

	{RES4345_XTAL_PU,		0x600002},	/* 3 ms uptime for XTAL_PU */
};

static const pmu_res_depend_t BCMATTACHDATA(bcm4345_res_depend)[] = {
#ifdef SRFAST
	{
		PMURES_BIT(RES4345_LDO3P3_PU),
		RES_DEPEND_REMOVE,
		PMURES_BIT(RES4345_PMU_SLEEP),
		NULL
	},
	{
		PMURES_BIT(RES4345_SR_CLK_START) |
		PMURES_BIT(RES4345_WL_CORE_RDY) |
		PMURES_BIT(RES4345_ALP_AVAIL) |
		PMURES_BIT(RES4345_SR_CLK_STABLE) |
		PMURES_BIT(RES4345_SR_SAVE_RESTORE) |
		PMURES_BIT(RES4345_SR_PHY_PWRSW) |
		PMURES_BIT(RES4345_SR_VDDM_PWRSW) |
		PMURES_BIT(RES4345_SR_SUBCORE_PWRSW) |
		PMURES_BIT(RES4345_SR_SLEEP) |
		PMURES_BIT(RES4345_RADIO_PU) |
		PMURES_BIT(RES4345_MACPHY_CLKAVAIL) |
		PMURES_BIT(RES4345_CBUCK_PFM_PU) |
		PMURES_BIT(RES4345_MINI_PMU) |
		PMURES_BIT(RES4345_HT_START) |
		PMURES_BIT(RES4345_HT_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4345_RSVD_7),
		NULL
	},
	{
		PMURES_BIT(RES4345_SR_CLK_START) |
		PMURES_BIT(RES4345_WL_CORE_RDY) |
		PMURES_BIT(RES4345_SR_CLK_STABLE) |
		PMURES_BIT(RES4345_SR_SAVE_RESTORE) |
		PMURES_BIT(RES4345_SR_SLEEP) |
		PMURES_BIT(RES4345_RADIO_PU) |
		PMURES_BIT(RES4345_CBUCK_PFM_PU) |
		PMURES_BIT(RES4345_MINI_PMU),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4345_XTAL_PU),
		NULL
	},
	{
		PMURES_BIT(RES4345_SR_CLK_START) |
		PMURES_BIT(RES4345_WL_CORE_RDY) |
		PMURES_BIT(RES4345_SR_CLK_STABLE) |
		PMURES_BIT(RES4345_SR_SAVE_RESTORE) |
		PMURES_BIT(RES4345_SR_SLEEP) |
		PMURES_BIT(RES4345_SR_PHY_PWRSW) |
		PMURES_BIT(RES4345_SR_VDDM_PWRSW) |
		PMURES_BIT(RES4345_SR_SUBCORE_PWRSW) |
		PMURES_BIT(RES4345_RADIO_PU) |
		PMURES_BIT(RES4345_CBUCK_PFM_PU) |
		PMURES_BIT(RES4345_MINI_PMU),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4345_XTALLDO_PU),
		NULL
	},
	{
		PMURES_BIT(RES4345_COLD_START_WAIT) |
		PMURES_BIT(RES4345_XTALLDO_PU) |
		PMURES_BIT(RES4345_XTAL_PU) |
		PMURES_BIT(RES4345_SR_CLK_START) |
		PMURES_BIT(RES4345_SR_CLK_STABLE) |
		PMURES_BIT(RES4345_SR_PHY_PWRSW) |
		PMURES_BIT(RES4345_SR_VDDM_PWRSW) |
		PMURES_BIT(RES4345_SR_SUBCORE_PWRSW),
		RES_DEPEND_REMOVE,
		PMURES_BIT(RES4345_CBUCK_PFM_PU),
		NULL
	},
	{
		PMURES_BIT(RES4345_RSVD_7),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4345_XTALLDO_PU) |
		PMURES_BIT(RES4345_COLD_START_WAIT) |
		PMURES_BIT(RES4345_LNLDO_PU) |
		PMURES_BIT(RES4345_LDO3P3_PU) |
		PMURES_BIT(RES4345_PMU_SLEEP) |
		PMURES_BIT(RES4345_CBUCK_LPOM_PU) |
		PMURES_BIT(RES4345_PMU_BG_PU) |
		PMURES_BIT(RES4345_LPLDO_PU),
		NULL
	},
	{
		PMURES_BIT(RES4345_COLD_START_WAIT) |
		PMURES_BIT(RES4345_XTALLDO_PU) |
		PMURES_BIT(RES4345_XTAL_PU) |
		PMURES_BIT(RES4345_SR_CLK_START) |
		PMURES_BIT(RES4345_WL_CORE_RDY) |
		PMURES_BIT(RES4345_ALP_AVAIL) |
		PMURES_BIT(RES4345_SR_CLK_STABLE) |
		PMURES_BIT(RES4345_SR_SAVE_RESTORE) |
		PMURES_BIT(RES4345_SR_PHY_PWRSW) |
		PMURES_BIT(RES4345_SR_VDDM_PWRSW) |
		PMURES_BIT(RES4345_SR_SUBCORE_PWRSW) |
		PMURES_BIT(RES4345_SR_SLEEP) |
		PMURES_BIT(RES4345_HT_START) |
		PMURES_BIT(RES4345_HT_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4345_LDO3P3_PU),
		NULL
	},
	{
		PMURES_BIT(RES4345_LNLDO_PU),
		RES_DEPEND_REMOVE,
		PMURES_BIT(RES4345_PMU_SLEEP) |
		PMURES_BIT(RES4345_CBUCK_LPOM_PU) |
		PMURES_BIT(RES4345_CBUCK_PFM_PU),
		NULL
	},
	{
		PMURES_BIT(RES4345_ALP_AVAIL) |
		PMURES_BIT(RES4345_RADIO_PU) |
		PMURES_BIT(RES4345_HT_START) |
		PMURES_BIT(RES4345_HT_AVAIL) |
		PMURES_BIT(RES4345_MACPHY_CLKAVAIL),
		RES_DEPEND_REMOVE,
		PMURES_BIT(RES4345_LQ_AVAIL) |
		PMURES_BIT(RES4345_LQ_START),
		NULL
	},
#ifdef BCMPCIEDEV_ENABLED
	{
		PMURES_BIT(RES4345_PERST_OVR),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4345_XTALLDO_PU) |
		PMURES_BIT(RES4345_XTAL_PU) |
		PMURES_BIT(RES4345_SR_CLK_STABLE) |
		PMURES_BIT(RES4345_SR_SAVE_RESTORE) |
		PMURES_BIT(RES4345_SR_PHY_PWRSW) |
		PMURES_BIT(RES4345_SR_VDDM_PWRSW) |
		PMURES_BIT(RES4345_SR_SUBCORE_PWRSW) |
		PMURES_BIT(RES4345_SR_SLEEP),
		NULL
	},
	{
		PMURES_BIT(RES4345_PERST_OVR),
		RES_DEPEND_REMOVE,
		PMURES_BIT(RES4345_XTALLDO_PU) |
		PMURES_BIT(RES4345_SR_CLK_START) |
		PMURES_BIT(RES4345_LDO3P3_PU) |
		PMURES_BIT(RES4345_OTP_PU),
		NULL
	},
	{
		PMURES_BIT(RES4345_ALP_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4345_PERST_OVR),
		NULL
	},
	{
		PMURES_BIT(RES4345_RADIO_PU),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4345_PERST_OVR),
		NULL
	},
	{
		PMURES_BIT(RES4345_HT_START),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4345_PERST_OVR),
		NULL
	},
	{
		PMURES_BIT(RES4345_HT_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4345_PERST_OVR),
		NULL
	},
	{
		PMURES_BIT(RES4345_MACPHY_CLKAVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4345_PERST_OVR),
		NULL
	},
#endif /* BCMPCIEDEV_ENABLED */
#else
#ifdef BCMPCIEDEV_ENABLED
	{
		PMURES_BIT(RES4345_PERST_OVR),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4345_XTALLDO_PU) |
		PMURES_BIT(RES4345_XTAL_PU) |
		PMURES_BIT(RES4345_SR_CLK_STABLE) |
		PMURES_BIT(RES4345_SR_SAVE_RESTORE) |
		PMURES_BIT(RES4345_SR_PHY_PWRSW) |
		PMURES_BIT(RES4345_SR_VDDM_PWRSW) |
		PMURES_BIT(RES4345_SR_SUBCORE_PWRSW) |
		PMURES_BIT(RES4345_SR_SLEEP),
		NULL
	},
	{
		PMURES_BIT(RES4345_PERST_OVR),
		RES_DEPEND_REMOVE,
		PMURES_BIT(RES4345_XTALLDO_PU) |
		PMURES_BIT(RES4345_SR_CLK_START) |
		PMURES_BIT(RES4345_LDO3P3_PU) |
		PMURES_BIT(RES4345_OTP_PU),
		NULL
	},
	{
		PMURES_BIT(RES4345_WL_CORE_RDY),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4345_PERST_OVR),
		NULL
	},
	{
		PMURES_BIT(RES4345_ALP_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4345_PERST_OVR),
		NULL
	},
	{
		PMURES_BIT(RES4345_RADIO_PU),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4345_PERST_OVR),
		NULL
	},
	{
		PMURES_BIT(RES4345_HT_START),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4345_PERST_OVR),
		NULL
	},
	{
		PMURES_BIT(RES4345_HT_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4345_PERST_OVR),
		NULL
	},
	{
		PMURES_BIT(RES4345_MACPHY_CLKAVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4345_PERST_OVR),
		NULL
	},
#else
	{0, 0, 0, NULL}
#endif /* BCMPCIEDEV_ENABLED */
#endif /* SRFAST */
};

static const pmu_res_depend_t BCMATTACHDATA(bcm43602_res_depend)[] = {
	{
		PMURES_BIT(RES43602_SR_SUBCORE_PWRSW) | PMURES_BIT(RES43602_SR_CLK_STABLE) |
		PMURES_BIT(RES43602_SR_SAVE_RESTORE)  | PMURES_BIT(RES43602_SR_SLEEP) |
		PMURES_BIT(RES43602_LQ_START) | PMURES_BIT(RES43602_LQ_AVAIL) |
		PMURES_BIT(RES43602_WL_CORE_RDY) | PMURES_BIT(RES43602_ILP_REQ) |
		PMURES_BIT(RES43602_ALP_AVAIL) | PMURES_BIT(RES43602_RFLDO_PU) |
		PMURES_BIT(RES43602_HT_START) | PMURES_BIT(RES43602_HT_AVAIL) |
		PMURES_BIT(RES43602_MACPHY_CLKAVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES43602_SERDES_PU),
		NULL
	}
};

#ifndef BCM_BOOTLOADER
static const pmu_res_depend_t BCMATTACHDATA(bcm43602_res_pciewar)[] = {
	{
		PMURES_BIT(RES43602_PERST_OVR),
		RES_DEPEND_SET,
		PMURES_BIT(RES43602_REGULATOR) |
		PMURES_BIT(RES43602_PMU_SLEEP) |
		PMURES_BIT(RES43602_XTALLDO_PU) |
		PMURES_BIT(RES43602_XTAL_PU) |
		PMURES_BIT(RES43602_RADIO_PU),
		NULL
	},
	{
		PMURES_BIT(RES43602_WL_CORE_RDY),
		RES_DEPEND_ADD,
		PMURES_BIT(RES43602_PERST_OVR),
		NULL
	},
	{
		PMURES_BIT(RES43602_LQ_START),
		RES_DEPEND_ADD,
		PMURES_BIT(RES43602_PERST_OVR),
		NULL
	},
	{
		PMURES_BIT(RES43602_LQ_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES43602_PERST_OVR),
		NULL
	},
	{
		PMURES_BIT(RES43602_ALP_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES43602_PERST_OVR),
		NULL
	},
	{
		PMURES_BIT(RES43602_HT_START),
		RES_DEPEND_ADD,
		PMURES_BIT(RES43602_PERST_OVR),
		NULL
	},
	{
		PMURES_BIT(RES43602_HT_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES43602_PERST_OVR),
		NULL
	},
	{
		PMURES_BIT(RES43602_MACPHY_CLKAVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES43602_PERST_OVR),
		NULL
	}
};
#endif /* !BCM_BOOTLOADER */

static const pmu_res_updown_t BCMATTACHDATA(bcm4360B1_res_updown)[] = {
	/* Need to change elements here, should get default values for this - 4360B1 */
	{RES4360_XTAL_PU,		0x00430002}, /* Changed for 4360B1 */
};

static const pmu_res_depend_t BCMATTACHDATA(bcm4324a0_res_depend)[] = {
	{
		PMURES_BIT(RES4324_SR_PHY_PWRSW) | PMURES_BIT(RES4324_SR_PHY_PIC),
		RES_DEPEND_SET,
		0x00000000,
		NULL
	},
	{
		PMURES_BIT(RES4324_WL_CORE_READY) | PMURES_BIT(RES4324_ILP_REQ) |
		PMURES_BIT(RES4324_ALP_AVAIL) | PMURES_BIT(RES4324_RADIO_PU) |
		PMURES_BIT(RES4324_SR_CLK_STABLE) | PMURES_BIT(RES4324_SR_SAVE_RESTORE) |
		PMURES_BIT(RES4324_SR_SUBCORE_PIC) | PMURES_BIT(RES4324_SR_MEM_PM0) |
		PMURES_BIT(RES4324_HT_AVAIL) | PMURES_BIT(RES4324_MACPHY_CLKAVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4324_LPLDO_PU) | PMURES_BIT(RES4324_RESET_PULLDN_DIS) |
		PMURES_BIT(RES4324_PMU_BG_PU) | PMURES_BIT(RES4324_HSIC_LDO_PU) |
		PMURES_BIT(RES4324_CBUCK_LPOM_PU) | PMURES_BIT(RES4324_CBUCK_PFM_PU) |
		PMURES_BIT(RES4324_CLDO_PU) | PMURES_BIT(RES4324_LPLDO2_LVM),
		NULL
	},
	{
		PMURES_BIT(RES4324_WL_CORE_READY) | PMURES_BIT(RES4324_ILP_REQ) |
		PMURES_BIT(RES4324_ALP_AVAIL) | PMURES_BIT(RES4324_RADIO_PU) |
		PMURES_BIT(RES4324_HT_AVAIL) | PMURES_BIT(RES4324_MACPHY_CLKAVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4324_SR_CLK_STABLE) | PMURES_BIT(RES4324_SR_SAVE_RESTORE) |
		PMURES_BIT(RES4324_SR_SUBCORE_PWRSW) | PMURES_BIT(RES4324_SR_SUBCORE_PIC),
		NULL
	},
	{
		PMURES_BIT(RES4324_WL_CORE_READY) | PMURES_BIT(RES4324_ILP_REQ) |
		PMURES_BIT(RES4324_ALP_AVAIL) | PMURES_BIT(RES4324_RADIO_PU) |
		PMURES_BIT(RES4324_SR_CLK_STABLE) | PMURES_BIT(RES4324_SR_SAVE_RESTORE) |
		PMURES_BIT(RES4324_HT_AVAIL) | PMURES_BIT(RES4324_MACPHY_CLKAVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4324_LNLDO1_PU) | PMURES_BIT(RES4324_LNLDO2_PU) |
		PMURES_BIT(RES4324_BBPLL_PU) | PMURES_BIT(RES4324_LQ_AVAIL),
		NULL
	},
	{
		PMURES_BIT(RES4324_WL_CORE_READY) | PMURES_BIT(RES4324_ILP_REQ) |
		PMURES_BIT(RES4324_ALP_AVAIL) | PMURES_BIT(RES4324_RADIO_PU) |
		PMURES_BIT(RES4324_SR_CLK_STABLE) | PMURES_BIT(RES4324_SR_SAVE_RESTORE) |
		PMURES_BIT(RES4324_HT_AVAIL) | PMURES_BIT(RES4324_MACPHY_CLKAVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4324_SR_MEM_PM0),
		NULL
	},
	{
		PMURES_BIT(RES4324_SR_CLK_STABLE) | PMURES_BIT(RES4324_SR_SAVE_RESTORE) |
		PMURES_BIT(RES4324_SR_MEM_PM0),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4324_SR_SUBCORE_PWRSW) | PMURES_BIT(RES4324_SR_SUBCORE_PIC),
		NULL
	},
	{
		PMURES_BIT(RES4324_ILP_REQ) | PMURES_BIT(RES4324_ALP_AVAIL) |
		PMURES_BIT(RES4324_RADIO_PU) | PMURES_BIT(RES4324_HT_AVAIL) |
		PMURES_BIT(RES4324_MACPHY_CLKAVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4324_WL_CORE_READY),
		NULL
	},
	{
		PMURES_BIT(RES4324_ALP_AVAIL) | PMURES_BIT(RES4324_RADIO_PU) |
		PMURES_BIT(RES4324_HT_AVAIL) | PMURES_BIT(RES4324_MACPHY_CLKAVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4324_LQ_AVAIL),
		NULL
	},
	{
		PMURES_BIT(RES4324_SR_SAVE_RESTORE),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4324_SR_CLK_STABLE),
		NULL
	},
	{
		PMURES_BIT(RES4324_SR_SUBCORE_PIC),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4324_SR_SUBCORE_PWRSW),
		NULL
	},
	{
		PMURES_BIT(RES4324_RADIO_PU) | PMURES_BIT(RES4324_HT_AVAIL) |
		PMURES_BIT(RES4324_MACPHY_CLKAVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4324_ALP_AVAIL),
		NULL
	},
	{
		PMURES_BIT(RES4324_RADIO_PU) | PMURES_BIT(RES4324_MACPHY_CLKAVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4324_LDO3P3_PU) | PMURES_BIT(RES4324_PALDO_PU),
		NULL
	},
	{
		PMURES_BIT(RES4324_MACPHY_CLKAVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4324_RADIO_PU) | PMURES_BIT(RES4324_HT_AVAIL),
		NULL
	},
	{
		PMURES_BIT(RES4324_ILP_REQ) | PMURES_BIT(RES4324_ALP_AVAIL) |
		PMURES_BIT(RES4324_RADIO_PU) | PMURES_BIT(RES4324_HT_AVAIL) |
		PMURES_BIT(RES4324_MACPHY_CLKAVAIL),
		RES_DEPEND_REMOVE,
		PMURES_BIT(RES4324_SR_PHY_PWRSW) | PMURES_BIT(RES4324_SR_PHY_PIC),
		NULL
	},
	{
		PMURES_BIT(RES4324_SR_CLK_STABLE) | PMURES_BIT(RES4324_SR_SAVE_RESTORE) |
		PMURES_BIT(RES4324_SR_SUBCORE_PIC) | PMURES_BIT(RES4324_SR_MEM_PM0),
		RES_DEPEND_REMOVE,
		PMURES_BIT(RES4324_WL_CORE_READY),
		NULL
	},
	{
		PMURES_BIT(RES4324_SR_SUBCORE_PIC) | PMURES_BIT(RES4324_SR_MEM_PM0),
		RES_DEPEND_REMOVE,
		PMURES_BIT(RES4324_LNLDO1_PU) | PMURES_BIT(RES4324_LNLDO2_PU) |
		PMURES_BIT(RES4324_BBPLL_PU) | PMURES_BIT(RES4324_LQ_AVAIL),
		NULL
	}
};

static const pmu_res_updown_t BCMATTACHDATA(bcm4334b0_res_updown_qt)[] = {{0, 0}};
static const pmu_res_updown_t BCMATTACHDATA(bcm4334b0_res_updown)[] = {
	/* In ILP clock (32768 Hz) at 30.5us per tick */
#ifndef SRFAST
	/* SAVERESTORE */
	{RES4334_LOGIC_RET, 0x00050005},
	{RES4334_MACPHY_RET, 0x000c000c},
	/* 0x00160001 - corresponds to around 700 uSeconds uptime */
	{RES4334_XTAL_PU, 0x00160001}
#else
	/* Fast SR wakeup */

	/* Make Cbuck LPOM shorter */
	{RES4334_CBUCK_LPOM_PU, 0xc000c},
	{RES4334_CBUCK_PFM_PU, 0x010001},

	/* XTAL up timer */
	{RES4334_XTAL_PU, 0x130001},

	/* Reduce 1 tick for below resources */
	{RES4334_PMU_BG_PU, 0x00060001},

	/* Reduce 2 tick for below resources */
	{RES4334_CLDO_PU, 0x00010001},
	{RES4334_LNLDO_PU, 0x00010001},
	{RES4334_LDO3P3_PU, 0x00020001},
	{RES4334_WL_PMU_PU, 0x00010001},
	{RES4334_HT_AVAIL, 0x00010001},

	/* Make all these resources to have 1 tick ILP clock only */
	{RES4334_HSIC_LDO_PU, 0x10001},
	{RES4334_LPLDO2_LVM, 0},
	{RES4334_WL_PWRSW_PU, 0},
	{RES4334_LQ_AVAIL, 0},
	{RES4334_MEM_SLEEP, 0},
	{RES4334_WL_CORE_READY, 0},
	{RES4334_ALP_AVAIL, 0},

	{RES4334_OTP_PU, 0x10001},
	{RES4334_MISC_PWRSW_PU, 0},
	{RES4334_SYNTH_PWRSW_PU, 0},
	{RES4334_RX_PWRSW_PU, 0},
	{RES4334_RADIO_PU, 0},
	{RES4334_VCO_LDO_PU, 0},
	{RES4334_AFE_LDO_PU, 0},
	{RES4334_RX_LDO_PU, 0},
	{RES4334_TX_LDO_PU, 0},
	{RES4334_MACPHY_CLK_AVAIL, 0},

	/* Reduce up down timer of Retention Mode */
	{RES4334_LOGIC_RET, 0x40004},
	{RES4334_MACPHY_RET, 0x70007},
#endif /* SRFAST */

};

static const pmu_res_depend_t BCMATTACHDATA(bcm4334b0_res_depend)[] = {
#ifndef BCM_BOOTLOADER
	{
		PMURES_BIT(RES4334_MACPHY_CLK_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4334_LDO3P3_PU),
		NULL
	},
#ifdef SRFAST
	{
		PMURES_BIT(RES4334_ALP_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4334_OTP_PU) | PMURES_BIT(RES4334_LDO3P3_PU),
		NULL
	}
#endif /* SRFAST */
#else
	{0, 0}
#endif /* BCM_BOOTLOADER */
};

static const pmu_res_depend_t BCMATTACHDATA(bcm4334b0_res_hsic_depend)[] = {
#ifndef BCM_BOOTLOADER
	{
		PMURES_BIT(RES4334_MACPHY_CLK_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4334_LDO3P3_PU),
		NULL
	},
	{
		PMURES_BIT(RES4334_LPLDO2_LVM),
		RES_DEPEND_REMOVE,
		(1 << RES4334_CBUCK_PFM_PU),
		NULL
	},
	{
		PMURES_BIT(RES4334_LNLDO_PU),
		RES_DEPEND_REMOVE,
		(1 << RES4334_CBUCK_PFM_PU),
		NULL
	},
	{
		PMURES_BIT(RES4334_OTP_PU),
		RES_DEPEND_REMOVE,
		(1 << RES4334_CBUCK_PFM_PU),
		NULL
	},
	{
		PMURES_BIT(RES4334_XTAL_PU),
		RES_DEPEND_REMOVE,
		(1 << RES4334_CBUCK_PFM_PU),
		NULL
	},
	{
		PMURES_BIT(RES4334_WL_PWRSW_PU),
		RES_DEPEND_REMOVE,
		(1 << RES4334_CBUCK_PFM_PU),
		NULL
	},
	{
		PMURES_BIT(RES4334_LQ_AVAIL),
		RES_DEPEND_REMOVE,
		(1 << RES4334_CBUCK_PFM_PU),
		NULL
	},
	{
		PMURES_BIT(RES4334_LOGIC_RET),
		RES_DEPEND_REMOVE,
		(1 << RES4334_CBUCK_PFM_PU),
		NULL
	},
	{
		PMURES_BIT(RES4334_MEM_SLEEP),
		RES_DEPEND_REMOVE,
		(1 << RES4334_CBUCK_PFM_PU),
		NULL
	},
	{
		PMURES_BIT(RES4334_MACPHY_RET),
		RES_DEPEND_REMOVE,
		(1 << RES4334_CBUCK_PFM_PU),
		NULL
	},
	{
		PMURES_BIT(RES4334_WL_CORE_READY),
		RES_DEPEND_REMOVE,
		(1 << RES4334_CBUCK_PFM_PU),
		NULL
	},
	{
		PMURES_BIT(RES4334_ILP_REQ),
		RES_DEPEND_REMOVE,
		(1 << RES4334_CBUCK_PFM_PU),
		NULL
	},

#ifdef SRFAST
	/* Fast SR wakeup */

	/* Make LNLDO and XTAL PU depends on WL_CORE_READY */
	{
		PMURES_BIT(RES4334_WL_CORE_READY),
		RES_DEPEND_ADD,
		(1 << RES4334_XTAL_PU) | (1 << RES4334_LNLDO_PU),
		NULL
	},

	/* remove dependency on CBuck PFM */
	{
		PMURES_BIT(RES4334_LPLDO2_LVM),
		RES_DEPEND_SET,
		0x5f,
		NULL
	},

	/* Make LNLDO depends on CBuck LPOM */
	{
		PMURES_BIT(RES4334_LNLDO_PU),
		RES_DEPEND_SET,
		0x1f,
		NULL
	},

	/* Remove LPLDO2_LVM from WL power switch PU */
	{
		PMURES_BIT(RES4334_WL_PWRSW_PU),
		RES_DEPEND_REMOVE,
		(1 << RES4334_LPLDO2_LVM),
		NULL
	},

	/* Remove rsrc 19 (RES4334_ALP_AVAIL) from all radio resource dependency(20 to 28) */
	{
		PMURES_BIT(RES4334_MISC_PWRSW_PU),
		RES_DEPEND_REMOVE,
		(1 << RES4334_ALP_AVAIL),
		NULL
	},
	{
		PMURES_BIT(RES4334_SYNTH_PWRSW_PU),
		RES_DEPEND_REMOVE,
		(1 << RES4334_ALP_AVAIL),
		NULL
	},
	{
		PMURES_BIT(RES4334_RX_PWRSW_PU),
		RES_DEPEND_REMOVE,
		(1 << RES4334_ALP_AVAIL),
		NULL
	},
	{
		PMURES_BIT(RES4334_RADIO_PU),
		RES_DEPEND_REMOVE,
		(1 << RES4334_ALP_AVAIL),
		NULL
	},
	{
		PMURES_BIT(RES4334_WL_PMU_PU),
		RES_DEPEND_REMOVE,
		(1 << RES4334_ALP_AVAIL),
		NULL
	},
	{
		PMURES_BIT(RES4334_VCO_LDO_PU),
		RES_DEPEND_REMOVE,
		(1 << RES4334_ALP_AVAIL),
		NULL
	},
	{
		PMURES_BIT(RES4334_AFE_LDO_PU),
		RES_DEPEND_REMOVE,
		(1 << RES4334_ALP_AVAIL),
		NULL
	},
	{
		PMURES_BIT(RES4334_RX_LDO_PU),
		RES_DEPEND_REMOVE,
		(1 << RES4334_ALP_AVAIL),
		NULL
	},
	{
		PMURES_BIT(RES4334_TX_LDO_PU),
		RES_DEPEND_REMOVE,
		(1 << RES4334_ALP_AVAIL),
		NULL
	},
	{
		PMURES_BIT(RES4334_CBUCK_PFM_PU),
		RES_DEPEND_ADD,
		(1 << RES4334_XTAL_PU),
		NULL
	},
	{
		PMURES_BIT(RES4334_ALP_AVAIL),
		RES_DEPEND_ADD,
		PMURES_BIT(RES4334_OTP_PU) | PMURES_BIT(RES4334_LDO3P3_PU),
		NULL
	}
#endif /* SRFAST */
#else
	{0, 0}
#endif /* BCM_BOOTLOADER */
};

/* Filter '_depfltr_' functions used by the arrays above */

/* TRUE if the power topology uses the buck boost to provide 3.3V to VDDIO_RF and WLAN PA */
static bool
BCMATTACHFN(si_pmu_res_depfltr_bb)(si_t *sih)
{
	return (sih->boardflags & BFL_BUCKBOOST) != 0;
}

/* TRUE if the power topology doesn't use the cbuck. Key on chiprev also if the chip is BCM4325. */
static bool
BCMATTACHFN(si_pmu_res_depfltr_ncb)(si_t *sih)
{
	if (CHIPID(sih->chip) == BCM4325_CHIP_ID)
		return (CHIPREV(sih->chiprev) >= 2) && ((sih->boardflags & BFL_NOCBUCK) != 0);
	return ((sih->boardflags & BFL_NOCBUCK) != 0);
}

/* TRUE if the power topology uses the PALDO */
static bool
BCMATTACHFN(si_pmu_res_depfltr_paldo)(si_t *sih)
{
	return (sih->boardflags & BFL_PALDO) != 0;
}

/* TRUE if the power topology doesn't use the PALDO */
static bool
BCMATTACHFN(si_pmu_res_depfltr_npaldo)(si_t *sih)
{
	return (sih->boardflags & BFL_PALDO) == 0;
}

#define BCM94325_BBVDDIOSD_BOARDS(sih) (sih->boardtype == BCM94325DEVBU_BOARD || \
					sih->boardtype == BCM94325BGABU_BOARD)

/**
 * Determines min/max rsrc masks. Normally hardware contains these masks, and software reads the
 * masks from hardware. Note that masks are sometimes dependent on chip straps.
 */
static void
si_pmu_res_masks(si_t *sih, uint32 *pmin, uint32 *pmax)
{
	uint32 min_mask = 0, max_mask = 0;
	uint rsrcs;
	si_info_t * sii = SI_INFO(sih);

	/* # resources */
	rsrcs = (sih->pmucaps & PCAP_RC_MASK) >> PCAP_RC_SHIFT;

	/* determine min/max rsrc masks */
	switch (CHIPID(sih->chip)) {
	case BCM4328_CHIP_ID:
		/* Down to ILP request */
		min_mask = PMURES_BIT(RES4328_EXT_SWITCHER_PWM) |
		        PMURES_BIT(RES4328_BB_SWITCHER_PWM) |
		        PMURES_BIT(RES4328_XTAL_EN);
		/* Allow (but don't require) PLL to turn on */
		max_mask = 0xfffff;
		break;
	case BCM5354_CHIP_ID:
		/* Allow (but don't require) PLL to turn on */
		max_mask = 0xfffff;
		break;
	case BCM4325_CHIP_ID:
		ASSERT(CHIPREV(sih->chiprev) >= 2);
		/* Minimum rsrcs to work in sleep mode */
		if (!(sih->boardflags & BFL_NOCBUCK))
			min_mask |= PMURES_BIT(RES4325B0_CBUCK_LPOM);
		if (((sih->chipst & CST4325_PMUTOP_2B_MASK) >>
		     CST4325_PMUTOP_2B_SHIFT) == 1)
			min_mask |= PMURES_BIT(RES4325B0_CLDO_PU);
		if (!si_is_otp_disabled(sih))
			min_mask |= PMURES_BIT(RES4325_OTP_PU);
		/* Leave buck boost on in burst mode for certain boards */
		if ((sih->boardflags & BFL_BUCKBOOST) && (BCM94325_BBVDDIOSD_BOARDS(sih)))
			min_mask |= PMURES_BIT(RES4325_BUCK_BOOST_BURST);
		/* Allow all resources to be turned on upon requests */
		max_mask = ~(~0 << rsrcs);
		break;
	case BCM4312_CHIP_ID:
		/* default min_mask = 0x80000cbb is wrong */
		min_mask = 0xcbb;
		/*
		 * max_mask = 0x7fff;
		 * pmu_res_updown_table_sz = 0;
		 * pmu_res_depend_table_sz = 0;
		 */
		break;
	case BCM4322_CHIP_ID:
	case BCM43221_CHIP_ID:	case BCM43231_CHIP_ID:
	case BCM4342_CHIP_ID:
		if (CHIPREV(sih->chiprev) < 2) {
			/* request ALP(can skip for A1) */
			min_mask = PMURES_BIT(RES4322_RF_LDO) |
			        PMURES_BIT(RES4322_XTAL_PU) |
				PMURES_BIT(RES4322_ALP_AVAIL);
			if (BUSTYPE(sih->bustype) == SI_BUS) {
				min_mask += PMURES_BIT(RES4322_SI_PLL_ON) |
					PMURES_BIT(RES4322_HT_SI_AVAIL) |
					PMURES_BIT(RES4322_PHY_PLL_ON) |
					PMURES_BIT(RES4322_OTP_PU) |
					PMURES_BIT(RES4322_HT_PHY_AVAIL);
				max_mask = 0x1ff;
			}
		}
		break;
	case BCM43222_CHIP_ID:	case BCM43111_CHIP_ID:	case BCM43112_CHIP_ID:
	case BCM43224_CHIP_ID:	case BCM43225_CHIP_ID:	case BCM43421_CHIP_ID:
	case BCM43226_CHIP_ID:  case BCM43420_CHIP_ID:
	case BCM43235_CHIP_ID:	case BCM43236_CHIP_ID:	case BCM43238_CHIP_ID:
	case BCM43237_CHIP_ID:
	case BCM43234_CHIP_ID:
	case BCM4331_CHIP_ID:
	case BCM43431_CHIP_ID:
	case BCM6362_CHIP_ID:

	case BCM4360_CHIP_ID:
	case BCM4352_CHIP_ID:
		if (CHIPREV(sih->chiprev) >= 0x4) {
			min_mask = 0x103;
		}
		/* Continue - Don't break */
	case BCM43460_CHIP_ID:
	case BCM43526_CHIP_ID:
		if (CHIPREV(sih->chiprev) >= 0x3) {
			int cst_ht = CST4360_RSRC_INIT_MODE(sih->chipst) & 0x1;
			if (cst_ht == 0)
				max_mask = 0x1ff;
		}
		break;

	case BCM43602_CHIP_ID:
	case BCM43462_CHIP_ID:
		/* as a bare minimum, have ALP clock running */
		min_mask = PMURES_BIT(RES43602_LPLDO_PU)  | PMURES_BIT(RES43602_REGULATOR)      |
			PMURES_BIT(RES43602_PMU_SLEEP)    | PMURES_BIT(RES43602_XTALLDO_PU)     |
			PMURES_BIT(RES43602_SERDES_PU)    | PMURES_BIT(RES43602_BBPLL_PWRSW_PU) |
			PMURES_BIT(RES43602_SR_CLK_START) | PMURES_BIT(RES43602_SR_PHY_PWRSW)   |
			PMURES_BIT(RES43602_SR_SUBCORE_PWRSW) | PMURES_BIT(RES43602_XTAL_PU)    |
			PMURES_BIT(RES43602_PERST_OVR)    | PMURES_BIT(RES43602_SR_CLK_STABLE)  |
			PMURES_BIT(RES43602_SR_SAVE_RESTORE) | PMURES_BIT(RES43602_SR_SLEEP)    |
			PMURES_BIT(RES43602_LQ_START)     | PMURES_BIT(RES43602_LQ_AVAIL)       |
			PMURES_BIT(RES43602_WL_CORE_RDY)  |
			PMURES_BIT(RES43602_ALP_AVAIL);
		max_mask = (1<<3) | min_mask          | PMURES_BIT(RES43602_RADIO_PU)        |
			PMURES_BIT(RES43602_RFLDO_PU) | PMURES_BIT(RES43602_HT_START)        |
			PMURES_BIT(RES43602_HT_AVAIL) | PMURES_BIT(RES43602_MACPHY_CLKAVAIL);

#if defined(SAVERESTORE)
		/* min_mask is updated after SR code is downloaded to txfifo */
		if (SR_ENAB() && sr_isenab(sih)) {
			min_mask = PMURES_BIT(RES43602_LPLDO_PU);
		}
#endif /* SAVERESTORE */
		break;

	case BCM43143_CHIP_ID:
		max_mask = PMURES_BIT(RES43143_EXT_SWITCHER_PWM) | PMURES_BIT(RES43143_XTAL_PU) |
			PMURES_BIT(RES43143_ILP_REQUEST) | PMURES_BIT(RES43143_ALP_AVAIL) |
			PMURES_BIT(RES43143_WL_CORE_READY) | PMURES_BIT(RES43143_BBPLL_PWRSW_PU) |
			PMURES_BIT(RES43143_HT_AVAIL) | PMURES_BIT(RES43143_RADIO_PU) |
			PMURES_BIT(RES43143_MACPHY_CLK_AVAIL) | PMURES_BIT(RES43143_OTP_PU) |
			PMURES_BIT(RES43143_LQ_AVAIL);
		break;

	case BCM4329_CHIP_ID:

		/* Down to save the power. */
		if (CHIPREV(sih->chiprev) >= 0x2) {
			min_mask = PMURES_BIT(RES4329_CBUCK_LPOM) |
				PMURES_BIT(RES4329_LNLDO1_PU) | PMURES_BIT(RES4329_CLDO_PU);
		} else {
			min_mask = PMURES_BIT(RES4329_CBUCK_LPOM) | PMURES_BIT(RES4329_CLDO_PU);
		}
		if (!si_is_otp_disabled(sih))
			min_mask |= PMURES_BIT(RES4329_OTP_PU);
		/* Allow (but don't require) PLL to turn on */
		max_mask = 0x3ff63e;

		break;
	case BCM4315_CHIP_ID:
		/* We only need a few resources to be kept on all the time */
		if (!(sih->boardflags & BFL_NOCBUCK))
			min_mask = PMURES_BIT(RES4315_CBUCK_LPOM);
		min_mask |= PMURES_BIT(RES4315_CLDO_PU);
		/* Allow everything else to be turned on upon requests */
		max_mask = ~(~0 << rsrcs);
		break;
	case BCM4319_CHIP_ID:
#ifdef	CONFIG_XIP
		/* Initialize to ResInitMode2 for bootloader */
		min_mask = PMURES_BIT(RES4319_CBUCK_LPOM) |
			PMURES_BIT(RES4319_CBUCK_BURST) |
			PMURES_BIT(RES4319_CBUCK_PWM) |
			PMURES_BIT(RES4319_CLDO_PU) |
			PMURES_BIT(RES4319_PALDO_PU) |
			PMURES_BIT(RES4319_LNLDO1_PU) |
			PMURES_BIT(RES4319_OTP_PU) |
			PMURES_BIT(RES4319_XTAL_PU) |
			PMURES_BIT(RES4319_ALP_AVAIL) |
			PMURES_BIT(RES4319_RFPLL_PWRSW_PU);
#else
		/* We only need a few resources to be kept on all the time */
		min_mask = PMURES_BIT(RES4319_CBUCK_LPOM) |
			PMURES_BIT(RES4319_CLDO_PU);
#endif	/* CONFIG_XIP */

		/* Allow everything else to be turned on upon requests */
		max_mask = ~(~0 << rsrcs);
		break;
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
		/* Down to save the power. */
		min_mask = PMURES_BIT(RES4336_CBUCK_LPOM) | PMURES_BIT(RES4336_CLDO_PU) |
			PMURES_BIT(RES4336_LDO3P3_PU) | PMURES_BIT(RES4336_OTP_PU) |
			PMURES_BIT(RES4336_DIS_INT_RESET_PD);
		/* Allow (but don't require) PLL to turn on */
		max_mask = 0x1ffffff;
		break;

	case BCM4330_CHIP_ID:
		/* Down to save the power. */
		min_mask = PMURES_BIT(RES4330_CBUCK_LPOM) | PMURES_BIT(RES4330_CLDO_PU) |
			PMURES_BIT(RES4330_DIS_INT_RESET_PD) | PMURES_BIT(RES4330_LDO3P3_PU) |
			PMURES_BIT(RES4330_OTP_PU);
		/* Allow (but don't require) PLL to turn on */
		max_mask = 0xfffffff;
		break;

	case BCM4313_CHIP_ID:
		min_mask = PMURES_BIT(RES4313_BB_PU_RSRC) |
			PMURES_BIT(RES4313_XTAL_PU_RSRC) |
			PMURES_BIT(RES4313_ALP_AVAIL_RSRC) |
			PMURES_BIT(RES4313_SYNTH_PWRSW_RSRC) |
			PMURES_BIT(RES4313_BB_PLL_PWRSW_RSRC);
		max_mask = 0xffff;
		break;

	case BCM43239_CHIP_ID:
	case BCM4324_CHIP_ID:
		/* Read the min_res_mask register. Set the mask to 0 as we need to only read. */
		min_mask = pmu_corereg(sih, SI_CC_IDX, min_res_mask, 0, 0);
		/* Set the bits for all resources in the max mask except for the SR Engine */
		max_mask = 0x7FFFFFFF;
		break;
	case BCM4335_CHIP_ID:
		/* Read the min_res_mask register. Set the mask to 0 as we need to only read. */
		min_mask = pmu_corereg(sih, SI_CC_IDX, min_res_mask, 0, 0);
		/* For 4335, min res mask is set to 1 after disabling power islanding */
		/* Write a non-zero value in chipcontrol reg 3 */

#ifndef BCMPCIEDEV_ENABLED
		/* shouldnt enable SR feature for pcie full dongle. */
		si_pmu_chipcontrol(sih, 3, 0x1, 0x1);
#endif
		/* Set the bits for all resources in the max mask except for the SR Engine */
		max_mask = 0x7FFFFFFF;
		break;

	case BCM4345_CHIP_ID:
		/* use chip default min resource mask */
		min_mask = PMURES_BIT(RES4345_LPLDO_PU) |
			PMURES_BIT(RES4345_PMU_BG_PU) |
			PMURES_BIT(RES4345_PMU_SLEEP) |
			PMURES_BIT(RES4345_CBUCK_LPOM_PU) |
			PMURES_BIT(RES4345_CBUCK_PFM_PU) |
			PMURES_BIT(RES4345_COLD_START_WAIT) |
			PMURES_BIT(RES4345_LNLDO_PU) |
			PMURES_BIT(RES4345_SR_CLK_START) |
			PMURES_BIT(RES4345_WL_CORE_RDY) |
			PMURES_BIT(RES4345_SR_CLK_STABLE) |
			PMURES_BIT(RES4345_SR_SAVE_RESTORE) |
			PMURES_BIT(RES4345_SR_PHY_PWRSW) |
			PMURES_BIT(RES4345_SR_VDDM_PWRSW) |
			PMURES_BIT(RES4345_SR_SUBCORE_PWRSW) |
			PMURES_BIT(RES4345_SR_SLEEP);

#if defined(SAVERESTORE)
		/* min_mask is updated after SR code is downloaded to txfifo */
		if (SR_ENAB() && sr_isenab(sih))
			min_mask = PMURES_BIT(RES4345_LPLDO_PU);
#endif
		/* Set the bits for all resources in the max mask except for the SR Engine */
		max_mask = 0x7FFFFFFF;
		break;

	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
		/* JIRA: SWWLAN-27486 Power consumption optimization when wireless is down */
#ifndef BCM_BOOTLOADER
		if (CST4350_IFC_MODE(sih->chipst) == CST4350_IFC_MODE_PCIE) {
			int L1substate = si_pcie_get_L1substate(sih);
			if (L1substate & 1)	/* L1.2 is enabled */
				min_mask = PMURES_BIT(RES4350_LPLDO_PU) |
					PMURES_BIT(RES4350_PMU_BG_PU) |
					PMURES_BIT(RES4350_PMU_SLEEP);
			else			/* use chip default min resource mask */
				min_mask = 0xfc22f77;
		} else {
#endif /* BCM_BOOTLOADER */

			/* use chip default min resource mask */
			min_mask = pmu_corereg(sih, SI_CC_IDX,
				min_res_mask, 0, 0);
#ifndef BCM_BOOTLOADER
		}
#endif /* BCM_BOOTLOADER */

#if defined(SAVERESTORE)
		/* min_mask is updated after SR code is downloaded to txfifo */
		if (SR_ENAB() && sr_isenab(sih)) {
			min_mask = PMURES_BIT(RES4350_LPLDO_PU);

			if (CST4350_CHIPMODE_PCIE(sih->chipst) &&
				(CHIPREV(sih->chiprev) <= 5)) {
				min_mask = PMURES_BIT(RES4350_LDO3P3_PU) |
					PMURES_BIT(RES4350_PMU_SLEEP) |
					PMURES_BIT(RES4350_PMU_BG_PU) |
					PMURES_BIT(RES4350_LPLDO_PU);
			}
		}
#endif /* SAVERESTORE */
		/* Set the bits for all resources in the max mask except for the SR Engine */
		max_mask = 0x7FFFFFFF;
		break;

	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
		/* Read the min_res_mask register. Set the mask to 0 as we need to only read. */
		min_mask = pmu_corereg(sih, SI_CC_IDX, min_res_mask, 0, 0);

		/* Set the bits for all resources in the max mask except for the SR Engine */
		max_mask = (1 << rsrcs) - 1;
		break;

	case BCM4314_CHIP_ID:
		/* set 4314 min mask to 0x3_f6ff -- ILP available (DozeMode) */
		if ((sih->chippkg == BCM4314SDIO_PKG_ID) ||
			(sih->chippkg == BCM4314SDIO_ARM_PKG_ID) ||
			(sih->chippkg == BCM4314SDIO_FPBGA_PKG_ID)) {
			min_mask = PMURES_BIT(RES4314_LPLDO_PU) |
				PMURES_BIT(RES4314_PMU_SLEEP_DIS) |
				PMURES_BIT(RES4314_PMU_BG_PU) |
				PMURES_BIT(RES4314_CBUCK_LPOM_PU) |
				PMURES_BIT(RES4314_CBUCK_PFM_PU) |
				PMURES_BIT(RES4314_CLDO_PU) |
				PMURES_BIT(RES4314_OTP_PU);
		} else {
			min_mask = PMURES_BIT(RES4314_LPLDO_PU) |
				PMURES_BIT(RES4314_PMU_SLEEP_DIS) |
				PMURES_BIT(RES4314_PMU_BG_PU) |
				PMURES_BIT(RES4314_CBUCK_LPOM_PU) |
				PMURES_BIT(RES4314_CBUCK_PFM_PU) |
				PMURES_BIT(RES4314_CLDO_PU) |
				PMURES_BIT(RES4314_LPLDO2_LVM) |
				PMURES_BIT(RES4314_WL_PMU_PU) |
				PMURES_BIT(RES4314_LDO3P3_PU) |
				PMURES_BIT(RES4314_OTP_PU) |
				PMURES_BIT(RES4314_WL_PWRSW_PU) |
				PMURES_BIT(RES4314_LQ_AVAIL) |
				PMURES_BIT(RES4314_LOGIC_RET) |
				PMURES_BIT(RES4314_MEM_SLEEP) |
				PMURES_BIT(RES4314_MACPHY_RET) |
				PMURES_BIT(RES4314_WL_CORE_READY) |
				PMURES_BIT(RES4314_SYNTH_PWRSW_PU);
		}
		max_mask = 0x3FFFFFFF;
		break;
	case BCM43142_CHIP_ID:
		/* Only PCIe */
		min_mask = PMURES_BIT(RES4314_LPLDO_PU) |
			PMURES_BIT(RES4314_PMU_SLEEP_DIS) |
			PMURES_BIT(RES4314_PMU_BG_PU) |
			PMURES_BIT(RES4314_CBUCK_LPOM_PU) |
			PMURES_BIT(RES4314_CBUCK_PFM_PU) |
			PMURES_BIT(RES4314_CLDO_PU) |
			PMURES_BIT(RES4314_LPLDO2_LVM) |
			PMURES_BIT(RES4314_WL_PMU_PU) |
			PMURES_BIT(RES4314_LDO3P3_PU) |
			PMURES_BIT(RES4314_OTP_PU) |
			PMURES_BIT(RES4314_WL_PWRSW_PU) |
			PMURES_BIT(RES4314_LQ_AVAIL) |
			PMURES_BIT(RES4314_LOGIC_RET) |
			PMURES_BIT(RES4314_MEM_SLEEP) |
			PMURES_BIT(RES4314_MACPHY_RET) |
			PMURES_BIT(RES4314_WL_CORE_READY);
		max_mask = 0x3FFFFFFF;
		break;
	case BCM4334_CHIP_ID:
		/* Use default for boot loader */
#ifndef BCM_BOOTLOADER
		min_mask = PMURES_BIT(RES4334_LPLDO_PU) | PMURES_BIT(RES4334_RESET_PULLDN_DIS) |
			PMURES_BIT(RES4334_OTP_PU) | PMURES_BIT(RES4334_WL_CORE_READY);
#endif /* !BCM_BOOTLOADER */

		max_mask = 0x7FFFFFFF;
		break;
	case BCM43341_CHIP_ID:
		/* Use default for boot loader */
#ifndef BCM_BOOTLOADER
		min_mask = PMURES_BIT(RES4334_LPLDO_PU);

#endif /* !BCM_BOOTLOADER */

		max_mask = 0x7FFFFFFF;
		break;
	default:
		PMU_ERROR(("MIN and MAX mask is not programmed\n"));
		break;
	}

	/* Apply nvram override to min mask */
	if (sii->min_mask_valid == TRUE) {
		PMU_MSG(("Applying rmin=%d to min_mask\n", sii->nvram_min_mask));
		min_mask = sii->nvram_min_mask;
	}
	/* Apply nvram override to max mask */
	if (sii->max_mask_valid == TRUE) {
		PMU_MSG(("Applying rmax=%d to max_mask\n", sii->nvram_max_mask));
		max_mask = sii->nvram_max_mask;
	}

	*pmin = min_mask;
	*pmax = max_mask;
} /* si_pmu_res_masks */

#if !defined(_CFE_) && !defined(_CFEZ_)
static void
BCMATTACHFN(si_pmu_resdeptbl_upd)(si_t *sih, osl_t *osh,
	const pmu_res_depend_t *restable, uint tablesz)
{
	uint i, rsrcs;

	if (tablesz == 0)
		return;

	ASSERT(restable != NULL);

	rsrcs = (sih->pmucaps & PCAP_RC_MASK) >> PCAP_RC_SHIFT;
	/* Program resource dependencies table */
	while (tablesz--) {
		if (restable[tablesz].filter != NULL &&
		    !(restable[tablesz].filter)(sih))
			continue;
		for (i = 0; i < rsrcs; i ++) {
			if ((restable[tablesz].res_mask &
			     PMURES_BIT(i)) == 0)
				continue;
			W_REG(osh, PMUREG(sih, res_table_sel), i);
			switch (restable[tablesz].action) {
				case RES_DEPEND_SET:
					PMU_MSG(("Changing rsrc %d res_dep_mask to 0x%x\n", i,
						restable[tablesz].depend_mask));
					W_REG(osh, PMUREG(sih, res_dep_mask),
					      restable[tablesz].depend_mask);
					break;
				case RES_DEPEND_ADD:
					PMU_MSG(("Adding 0x%x to rsrc %d res_dep_mask\n",
						restable[tablesz].depend_mask, i));
					OR_REG(osh, PMUREG(sih, res_dep_mask),
					       restable[tablesz].depend_mask);
					break;
				case RES_DEPEND_REMOVE:
					PMU_MSG(("Removing 0x%x from rsrc %d res_dep_mask\n",
						restable[tablesz].depend_mask, i));
					AND_REG(osh, PMUREG(sih, res_dep_mask),
						~restable[tablesz].depend_mask);
					break;
				default:
					ASSERT(0);
					break;
			}
		}
	}
}
#endif /* !defined(_CFE_) && !defined(_CFEZ_) */

/** Initialize PMU hardware resources. */
void
BCMATTACHFN(si_pmu_res_init)(si_t *sih, osl_t *osh)
{
#if !defined(_CFE_) && !defined(_CFEZ_)
	chipcregs_t *cc;
	uint origidx;
	const pmu_res_updown_t *pmu_res_updown_table = NULL;
	uint pmu_res_updown_table_sz = 0;
	const pmu_res_depend_t *pmu_res_depend_table = NULL;
	uint pmu_res_depend_table_sz = 0;
#ifndef BCM_BOOTLOADER
	const pmu_res_depend_t *pmu_res_depend_pciewar_table = NULL;
	uint pmu_res_depend_pciewar_table_sz = 0;
#endif /* BCM_BOOTLOADER */
	uint32 min_mask = 0, max_mask = 0;
	char name[8];
	const char *val;
	uint i, rsrcs;
	si_info_t* sii = SI_INFO(sih);

	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	/* Cache nvram override to min mask */
	if ((val = getvar(NULL, rstr_rmin)) != NULL) {
		sii->min_mask_valid = TRUE;
		sii->nvram_min_mask = (uint32)bcm_strtoul(val, NULL, 0);
	} else sii->min_mask_valid = FALSE;
	/* Cache nvram override to max mask */
	if ((val = getvar(NULL, rstr_rmax)) != NULL) {
		sii->max_mask_valid = TRUE;
		sii->nvram_max_mask = (uint32)bcm_strtoul(val, NULL, 0);
	} else sii->max_mask_valid = FALSE;

	/*
	 * Hardware contains the resource updown and dependency tables. Only if a chip has a
	 * hardware problem, software tables can be used to override hardware tables.
	 */
	switch (CHIPID(sih->chip)) {
	case BCM4328_CHIP_ID:
		pmu_res_updown_table = bcm4328a0_res_updown;
		pmu_res_updown_table_sz = ARRAYSIZE(bcm4328a0_res_updown);
		pmu_res_depend_table = bcm4328a0_res_depend;
		pmu_res_depend_table_sz = ARRAYSIZE(bcm4328a0_res_depend);
		break;
	case BCM4325_CHIP_ID:
		/* Optimize resources up/down timers */
		if (ISSIM_ENAB(sih)) {
			pmu_res_updown_table = bcm4325a0_res_updown_qt;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4325a0_res_updown_qt);
		} else {
			pmu_res_updown_table = bcm4325a0_res_updown;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4325a0_res_updown);
		}
		/* Optimize resources dependencies */
		pmu_res_depend_table = bcm4325a0_res_depend;
		pmu_res_depend_table_sz = ARRAYSIZE(bcm4325a0_res_depend);
		break;
	case BCM4315_CHIP_ID:
		/* Optimize resources up/down timers */
		if (ISSIM_ENAB(sih)) {
			pmu_res_updown_table = bcm4315a0_res_updown_qt;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4315a0_res_updown_qt);
		}
		else {
			pmu_res_updown_table = bcm4315a0_res_updown;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4315a0_res_updown);
		}
		/* Optimize resources dependencies masks */
		pmu_res_depend_table = bcm4315a0_res_depend;
		pmu_res_depend_table_sz = ARRAYSIZE(bcm4315a0_res_depend);
		break;
	case BCM4329_CHIP_ID:
		/* Optimize resources up/down timers */
		if (ISSIM_ENAB(sih)) {
			pmu_res_updown_table = NULL;
			pmu_res_updown_table_sz = 0;
		} else {
			pmu_res_updown_table = bcm4329_res_updown;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4329_res_updown);
		}
		/* Optimize resources dependencies */
		pmu_res_depend_table = bcm4329_res_depend;
		pmu_res_depend_table_sz = ARRAYSIZE(bcm4329_res_depend);
		break;

	case BCM4319_CHIP_ID:
		/* Optimize resources up/down timers */
		if (ISSIM_ENAB(sih)) {
			pmu_res_updown_table = bcm4319a0_res_updown_qt;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4319a0_res_updown_qt);
		}
		else {
			pmu_res_updown_table = bcm4319a0_res_updown;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4319a0_res_updown);
		}
		/* Optimize resources dependencies masks */
		pmu_res_depend_table = bcm4319a0_res_depend;
		pmu_res_depend_table_sz = ARRAYSIZE(bcm4319a0_res_depend);
		break;

	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
		/* Optimize resources up/down timers */
		if (ISSIM_ENAB(sih)) {
			pmu_res_updown_table = bcm4336a0_res_updown_qt;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4336a0_res_updown_qt);
		}
		else {
			pmu_res_updown_table = bcm4336a0_res_updown;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4336a0_res_updown);
		}
		/* Optimize resources dependencies masks */
		pmu_res_depend_table = bcm4336a0_res_depend;
		pmu_res_depend_table_sz = ARRAYSIZE(bcm4336a0_res_depend);
		break;

	case BCM4330_CHIP_ID:
		/* Optimize resources up/down timers */
		if (ISSIM_ENAB(sih)) {
			pmu_res_updown_table = bcm4330a0_res_updown_qt;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4330a0_res_updown_qt);
		}
		else {
			pmu_res_updown_table = bcm4330a0_res_updown;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4330a0_res_updown);
		}
		/* Optimize resources dependencies masks */
		pmu_res_depend_table = bcm4330a0_res_depend;
		pmu_res_depend_table_sz = ARRAYSIZE(bcm4330a0_res_depend);
		break;
	case BCM4314_CHIP_ID:
		/* Enable SDIO wake up */
		if (!((sih->chippkg == BCM4314PCIE_ARM_PKG_ID) ||
			(sih->chippkg == BCM4314PCIE_PKG_ID) ||
			(sih->chippkg == BCM4314DEV_PKG_ID)))
		{
			uint32 tmp;
			W_REG(osh, PMUREG(sih, chipcontrol_addr), PMU_CHIPCTL3);
			tmp = R_REG(osh, PMUREG(sih, chipcontrol_data));
			tmp |= (1 << PMU_CC3_ENABLE_SDIO_WAKEUP_SHIFT);
			W_REG(osh, PMUREG(sih, chipcontrol_data), tmp);
		}
	case BCM43142_CHIP_ID:
	case BCM43341_CHIP_ID:
		/* Optimize resources up/down timers */
		if (ISSIM_ENAB(sih)) {
			pmu_res_updown_table = bcm4334b0_res_updown_qt;
			/* pmu_res_updown_table_sz = ARRAYSIZE(bcm4334b0_res_updown_qt); */
		}
		else {
			pmu_res_updown_table = bcm4334b0_res_updown;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4334b0_res_updown);
		}
		/* Optimize resources dependancies masks */
		if (CST4334_CHIPMODE_HSIC(sih->chipst)) {
			pmu_res_depend_table = bcm4334b0_res_hsic_depend;
			pmu_res_depend_table_sz = ARRAYSIZE(bcm4334b0_res_hsic_depend);
		}
		else {
			pmu_res_depend_table = bcm4334b0_res_depend;
			pmu_res_depend_table_sz = ARRAYSIZE(bcm4334b0_res_depend);
		}
		break;
	case BCM4334_CHIP_ID:
		/* Optimize resources up/down timers */
		if (ISSIM_ENAB(sih)) {
			pmu_res_updown_table = bcm4334a0_res_updown_qt;
			/* pmu_res_updown_table_sz = ARRAYSIZE(bcm4334a0_res_updown_qt); */
		}
		else {
			pmu_res_updown_table = bcm4334a0_res_updown;
			/* pmu_res_updown_table_sz = ARRAYSIZE(bcm4334a0_res_updown); */
		}
		/* Optimize resources dependencies masks */
		pmu_res_depend_table = bcm4334a0_res_depend;
		/* pmu_res_depend_table_sz = ARRAYSIZE(bcm4334a0_res_depend); */
		break;
	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
		/* Need to change the up down timer for SR qt */
		if (ISSIM_ENAB(sih)) {
			pmu_res_updown_table = bcm4324a0_res_updown_qt;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4324a0_res_updown_qt);
		} else if ((CHIPID(sih->chip) == BCM4324_CHIP_ID) && (CHIPREV(sih->chiprev) <= 1)) {
			pmu_res_updown_table = bcm4324a0_res_updown;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4324a0_res_updown);
		} else {
			pmu_res_updown_table = bcm4324b0_res_updown;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4324b0_res_updown);
		}
		pmu_res_depend_table = bcm4324a0_res_depend;
		pmu_res_depend_table_sz = ARRAYSIZE(bcm4324a0_res_depend);
		break;
	case BCM4335_CHIP_ID:
		pmu_res_updown_table = bcm4335_res_updown;
		pmu_res_updown_table_sz = ARRAYSIZE(bcm4335_res_updown);
		pmu_res_depend_table = bcm4335b0_res_depend;
		pmu_res_depend_table_sz = ARRAYSIZE(bcm4335b0_res_depend);
		break;
	case BCM4345_CHIP_ID:
		pmu_res_updown_table = bcm4345_res_updown;
		pmu_res_updown_table_sz = ARRAYSIZE(bcm4345_res_updown);
		pmu_res_depend_table = bcm4345_res_depend;
		pmu_res_depend_table_sz = ARRAYSIZE(bcm4345_res_depend);
		break;
	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
		pmu_res_updown_table = bcm4350_res_updown;
		pmu_res_updown_table_sz = ARRAYSIZE(bcm4350_res_updown);
		if (!CST4350_CHIPMODE_PCIE(sih->chipst)) {
			pmu_res_depend_table = bcm4350_res_depend;
			pmu_res_depend_table_sz = ARRAYSIZE(bcm4350_res_depend);
		}
#ifndef BCM_BOOTLOADER
		pmu_res_depend_pciewar_table = bcm4350_res_pciewar;
		pmu_res_depend_pciewar_table_sz = ARRAYSIZE(bcm4350_res_pciewar);
#endif /* !BCM_BOOTLOADER */
		break;
	case BCM4360_CHIP_ID:
	case BCM4352_CHIP_ID:
		if (CHIPREV(sih->chiprev) < 4) {
			pmu_res_updown_table = bcm4360_res_updown;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4360_res_updown);
		} else {
			/* FOR 4360B1 */
			pmu_res_updown_table = bcm4360B1_res_updown;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4360B1_res_updown);
		}
		break;
	case BCM43602_CHIP_ID:
	case BCM43462_CHIP_ID:
		pmu_res_updown_table = bcm43602_res_updown;
		pmu_res_updown_table_sz = ARRAYSIZE(bcm43602_res_updown);
		pmu_res_depend_table = bcm43602_res_depend;
		pmu_res_depend_table_sz = ARRAYSIZE(bcm43602_res_depend);
#ifndef BCM_BOOTLOADER
		pmu_res_depend_pciewar_table = bcm43602_res_pciewar;
		pmu_res_depend_pciewar_table_sz = ARRAYSIZE(bcm43602_res_pciewar);
#endif /* !BCM_BOOTLOADER */
		break;
	case BCM43143_CHIP_ID:
		/* POR values for up/down and dependency tables are sufficient. */
		/* fall through */
	default:
		break;
	}

	/* # resources */
	rsrcs = (sih->pmucaps & PCAP_RC_MASK) >> PCAP_RC_SHIFT;

	/* Program up/down timers */
	while (pmu_res_updown_table_sz--) {
		ASSERT(pmu_res_updown_table != NULL);
		PMU_MSG(("Changing rsrc %d res_updn_timer to 0x%x\n",
		         pmu_res_updown_table[pmu_res_updown_table_sz].resnum,
		         pmu_res_updown_table[pmu_res_updown_table_sz].updown));
		W_REG(osh, PMUREG(sih, res_table_sel),
		      pmu_res_updown_table[pmu_res_updown_table_sz].resnum);
		W_REG(osh, PMUREG(sih, res_updn_timer),
		      pmu_res_updown_table[pmu_res_updown_table_sz].updown);
	}
	/* Apply nvram overrides to up/down timers */
	for (i = 0; i < rsrcs; i ++) {
		uint32 r_val;
		snprintf(name, sizeof(name), rstr_rDt, i);
		if ((val = getvar(NULL, name)) == NULL)
			continue;
		r_val = (uint32)bcm_strtoul(val, NULL, 0);
		/* PMUrev = 13, pmu resource updown times are 12 bits(0:11 DT, 16:27 UT) */
		if (sih->pmurev >= 13) {
			if (r_val < (1 << 16)) {
				uint16 up_time = (r_val >> 8) & 0xFF;
				r_val &= 0xFF;
				r_val |= (up_time << 16);
			}
		}
		PMU_MSG(("Applying %s=%s to rsrc %d res_updn_timer\n", name, val, i));
		W_REG(osh, PMUREG(sih, res_table_sel), (uint32)i);
		W_REG(osh, PMUREG(sih, res_updn_timer), r_val);
	}

	/* Program resource dependencies table */
	si_pmu_resdeptbl_upd(sih, osh, pmu_res_depend_table, pmu_res_depend_table_sz);

	/* Apply nvram overrides to dependencies masks */
	for (i = 0; i < rsrcs; i ++) {
		snprintf(name, sizeof(name), rstr_rDd, i);
		if ((val = getvar(NULL, name)) == NULL)
			continue;
		PMU_MSG(("Applying %s=%s to rsrc %d res_dep_mask\n", name, val, i));
		W_REG(osh, PMUREG(sih, res_table_sel), (uint32)i);
		W_REG(osh, PMUREG(sih, res_dep_mask), (uint32)bcm_strtoul(val, NULL, 0));
	}

#if !defined(BCM_BOOTLOADER) && !defined(BCM_OL_DEV)
	/* Initial any chip interface dependent PMU rsrc by looking at the
	 * chipstatus register to figure the selected interface
	 */
	if (BUSTYPE(sih->bustype) == PCI_BUS || BUSTYPE(sih->bustype) == SI_BUS) {
		bool is_pciedev = FALSE;

		if ((CHIPID(sih->chip) == BCM4345_CHIP_ID) && CST4345_CHIPMODE_PCIE(sih->chipst))
			is_pciedev = TRUE;
		else if (BCM4350_CHIP(sih->chip) && CST4350_CHIPMODE_PCIE(sih->chipst))
			is_pciedev = TRUE;
		else if (CHIPID(sih->chip) == BCM43602_CHIP_ID)
			is_pciedev = TRUE;

		if (is_pciedev && pmu_res_depend_pciewar_table && pmu_res_depend_pciewar_table_sz) {
			si_pmu_resdeptbl_upd(sih, osh,
				pmu_res_depend_pciewar_table, pmu_res_depend_pciewar_table_sz);
		}
	}
#endif /* !BCM_OL_DEV */
	/* Determine min/max rsrc masks */
	si_pmu_res_masks(sih, &min_mask, &max_mask);

	/* Add min mask dependencies */
	min_mask |= si_pmu_res_deps(sih, osh, cc, min_mask, FALSE);

	/* It is required to program max_mask first and then min_mask */
#ifdef BCM_BOOTLOADER
	if (CHIPID(sih->chip) == BCM4319_CHIP_ID) {
		min_mask |= R_REG(osh, PMUREG(sih, min_res_mask));
		max_mask |= R_REG(osh, PMUREG(sih, max_res_mask));
	}
#endif /* BCM_BOOTLOADER */

#ifdef BCM_BOOTLOADER
	/* Apply nvram override to max mask */
	if ((val = getvar(NULL, "brmax")) != NULL) {
		PMU_MSG(("Applying brmax=%s to max_res_mask\n", val));
		max_mask = (uint32)bcm_strtoul(val, NULL, 0);
	}

	/* Apply nvram override to min mask */
	if ((val = getvar(NULL, "brmin")) != NULL) {
		PMU_MSG(("Applying brmin=%s to min_res_mask\n", val));
		min_mask = (uint32)bcm_strtoul(val, NULL, 0);
	}
#endif /* BCM_BOOTLOADER */

#ifndef BCM_OL_DEV
	if (((CHIPID(sih->chip) == BCM4360_CHIP_ID) || (CHIPID(sih->chip) == BCM4352_CHIP_ID)) &&
	    (CHIPREV(sih->chiprev) < 4) &&
	    ((CST4360_RSRC_INIT_MODE(sih->chipst) & 1) == 0)) {
		/* BBPLL */
		W_REG(osh, PMUREG(sih, pllcontrol_addr), 6);
		W_REG(osh, PMUREG(sih, pllcontrol_data), 0x09048562);
		/* AVB PLL */
		W_REG(osh, PMUREG(sih, pllcontrol_addr), 14);
		W_REG(osh, PMUREG(sih, pllcontrol_data), 0x09048562);
		si_pmu_pllupd(sih);
	} else if (((CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4352_CHIP_ID)) &&
		(CHIPREV(sih->chiprev) >= 4) &&
		((CST4360_RSRC_INIT_MODE(sih->chipst) & 1) == 0)) {
		/* Changes for 4360B1 */

		/* Enable REFCLK bit 11 */
		W_REG(osh, PMUREG(sih, chipcontrol_addr), 1);
		OR_REG(osh, PMUREG(sih, chipcontrol_data), 0x800);

		/* BBPLL */
		W_REG(osh, PMUREG(sih, pllcontrol_addr), 6);
		W_REG(osh, PMUREG(sih, pllcontrol_data), 0x080004e2);

		W_REG(osh, PMUREG(sih, pllcontrol_addr), 7);
		W_REG(osh, PMUREG(sih, pllcontrol_data), 0xE);
		/* AVB PLL */
		W_REG(osh, PMUREG(sih, pllcontrol_addr), 14);
		W_REG(osh, PMUREG(sih, pllcontrol_data), 0x080004e2);

		W_REG(osh, PMUREG(sih, pllcontrol_addr), 15);
		W_REG(osh, PMUREG(sih, pllcontrol_data), 0xE);

		si_pmu_pllupd(sih);
	} else if ((CHIPID(sih->chip) == BCM43602_CHIP_ID ||
		CHIPID(sih->chip) == BCM43462_CHIP_ID) &&
		((CST4360_RSRC_INIT_MODE(sih->chipst) & 1) == 0)) {

	}
#endif /* BCM_OL_DEV */

	if (max_mask) {
		/* Ensure there is no bit set in min_mask which is not set in max_mask */
		max_mask |= min_mask;

		/* First set the bits which change from 0 to 1 in max, then update the
		 * min_mask register and then reset the bits which change from 1 to 0
		 * in max. This is required as the bit in MAX should never go to 0 when
		 * the corresponding bit in min is still 1. Similarly the bit in min cannot
		 * be 1 when the corresponding bit in max is still 0.
		 */
		OR_REG(osh, PMUREG(sih, max_res_mask), max_mask);
	} else {
		/* First set the bits which change from 0 to 1 in max, then update the
		 * min_mask register and then reset the bits which change from 1 to 0
		 * in max. This is required as the bit in MAX should never go to 0 when
		 * the corresponding bit in min is still 1. Similarly the bit in min cannot
		 * be 1 when the corresponding bit in max is still 0.
		 */
		if (min_mask)
			OR_REG(osh, PMUREG(sih, max_res_mask), min_mask);
	}

	/* Program min resource mask */
	if (min_mask) {
		PMU_MSG(("Changing min_res_mask to 0x%x\n", min_mask));
		W_REG(osh, PMUREG(sih, min_res_mask), min_mask);
	}

	/* Program max resource mask */
	if (max_mask) {
		PMU_MSG(("Changing max_res_mask to 0x%x\n", max_mask));
		W_REG(osh, PMUREG(sih, max_res_mask), max_mask);
	}

	/* request htavail thru pcie core */
	if (((CHIPID(sih->chip) == BCM4360_CHIP_ID) || (CHIPID(sih->chip) == BCM4352_CHIP_ID)) &&
	    (BUSTYPE(sih->bustype) == PCI_BUS) &&
	    (CHIPREV(sih->chiprev) < 4)) {
		uint32 pcie_clk_ctl_st;

		pcie_clk_ctl_st = si_corereg(sih, 3, 0x1e0, 0, 0);
		si_corereg(sih, 3, 0x1e0, ~0, (pcie_clk_ctl_st | CCS_HTAREQ));
	}

	si_pmu_wait_for_steady_state(sih, osh, cc);
	/* Add some delay; allow resources to come up and settle. */
	OSL_DELAY(2000);

	/* Return to original core */
	si_setcoreidx(sih, origidx);
#endif /* !_CFE_ && !_CFEZ_ */
} /* si_pmu_res_init */

/* setup pll and query clock speed */
typedef struct {
	uint16	freq;	/* x-tal frequency in [hz] */
	uint8	xf;	/* x-tal index as contained in PMU control reg, see PMU programmers guide */
	uint8	wbint;
	uint32	wbfrac;
} pmu0_xtaltab0_t;

/* the following table is based on 880Mhz fvco */
static const pmu0_xtaltab0_t BCMINITDATA(pmu0_xtaltab0)[] = {
	{ 12000,	1,	73,	349525 },
	{ 13000,	2,	67,	725937 },
	{ 14400,	3,	61,	116508 },
	{ 15360,	4,	57,	305834 },
	{ 16200,	5,	54,	336579 },
	{ 16800,	6,	52,	399457 },
	{ 19200,	7,	45,	873813 },
	{ 19800,	8,	44,	466033 },
	{ 20000,	9,	44,	0 },
	{ 25000,	10,	70,	419430 },
	{ 26000,	11,	67,	725937 },
	{ 30000,	12,	58,	699050 },
	{ 38400,	13,	45,	873813 },
	{ 40000,	14,	45,	0 },
	{ 0,		0,	0,	0 }
};

#ifdef BCMUSBDEV
#define	PMU0_XTAL0_DEFAULT	11
#else
#define PMU0_XTAL0_DEFAULT	8
#endif

#ifdef BCMUSBDEV
/**
 * Set new backplane PLL clock frequency
 */
static void
BCMATTACHFN(si_pmu0_sbclk4328)(si_t *sih, int freq)
{
	uint32 tmp, oldmax, oldmin;
	osl_t *osh = si_osh(sih);


	/* Set new backplane PLL clock */
	W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU0_PLL0_PLLCTL0);
	tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
	tmp &= ~(PMU0_PLL0_PC0_DIV_ARM_MASK);
	tmp |= freq << PMU0_PLL0_PC0_DIV_ARM_SHIFT;
	W_REG(osh, PMUREG(sih, pllcontrol_data), tmp);

	/* Power cycle BB_PLL_PU by disabling/enabling it to take on new freq */
	/* Disable PLL */
	oldmin = R_REG(osh, PMUREG(sih, min_res_mask));
	oldmax = R_REG(osh, PMUREG(sih, max_res_mask));
	W_REG(osh, PMUREG(sih, min_res_mask), oldmin & ~PMURES_BIT(RES4328_BB_PLL_PU));
	W_REG(osh, PMUREG(sih, max_res_mask), oldmax & ~PMURES_BIT(RES4328_BB_PLL_PU));

	/* It takes over several hundred usec to re-enable the PLL since the
	 * sequencer state machines run on ILP clock. Set delay at 450us to be safe.
	 *
	 * Be sure PLL is powered down first before re-enabling it.
	 */

	OSL_DELAY(PLL_DELAY);
	SPINWAIT((R_REG(osh, PMUREG(sih, res_state)) & PMURES_BIT(RES4328_BB_PLL_PU)), PLL_DELAY*3);
	if (R_REG(osh, PMUREG(sih, res_state)) & PMURES_BIT(RES4328_BB_PLL_PU)) {
		/* If BB_PLL not powered down yet, new backplane PLL clock
		 *  may not take effect.
		 *
		 * Still early during bootup so no serial output here.
		 */
		PMU_ERROR(("Fatal: BB_PLL not power down yet!\n"));
		ASSERT(!(R_REG(osh, PMUREG(sih, res_state)) & PMURES_BIT(RES4328_BB_PLL_PU)));
	}

	/* Enable PLL */
	W_REG(osh, PMUREG(sih, max_res_mask), oldmax);
} /* si_pmu0_sbclk4328 */
#endif /* BCMUSBDEV */

/**
 * Set up PLL registers in the PMU as per the crystal speed.
 * Uses xtalfreq variable, or passed-in default.
 */
static void
BCMATTACHFN(si_pmu0_pllinit0)(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal)
{
	uint32 tmp;
	const pmu0_xtaltab0_t *xt;

	/* Find the frequency in the table */
	for (xt = pmu0_xtaltab0; xt->freq; xt ++)
		if (xt->freq == xtal)
			break;
	if (xt->freq == 0)
		xt = &pmu0_xtaltab0[PMU0_XTAL0_DEFAULT];

	PMU_MSG(("XTAL %d.%d MHz (%d)\n", xtal / 1000, xtal % 1000, xt->xf));

	/* Check current PLL state */
	tmp = (R_REG(osh, PMUREG(sih, pmucontrol)) & PCTL_XTALFREQ_MASK) >>
	        PCTL_XTALFREQ_SHIFT;
	if (tmp == xt->xf) {
		PMU_MSG(("PLL already programmed for %d.%d MHz\n",
		         xt->freq / 1000, xt->freq % 1000));
#ifdef BCMUSBDEV
		if (CHIPID(sih->chip) == BCM4328_CHIP_ID)
			si_pmu0_sbclk4328(sih, PMU0_PLL0_PC0_DIV_ARM_88MHZ);
#endif	/* BCMUSBDEV */
		return;
	}

	if (tmp) {
		PMU_MSG(("Reprogramming PLL for %d.%d MHz (was %d.%dMHz)\n",
		         xt->freq / 1000, xt->freq % 1000,
		         pmu0_xtaltab0[tmp-1].freq / 1000, pmu0_xtaltab0[tmp-1].freq % 1000));
	} else {
		PMU_MSG(("Programming PLL for %d.%d MHz\n",
		         xt->freq / 1000, xt->freq % 1000));
	}

	/* Make sure the PLL is off */
	switch (CHIPID(sih->chip)) {
	case BCM4328_CHIP_ID:
		AND_REG(osh, PMUREG(sih, min_res_mask), ~PMURES_BIT(RES4328_BB_PLL_PU));
		AND_REG(osh, PMUREG(sih, max_res_mask), ~PMURES_BIT(RES4328_BB_PLL_PU));
		break;
	case BCM5354_CHIP_ID:
		AND_REG(osh, PMUREG(sih, min_res_mask), ~PMURES_BIT(RES5354_BB_PLL_PU));
		AND_REG(osh, PMUREG(sih, max_res_mask), ~PMURES_BIT(RES5354_BB_PLL_PU));
		break;
	default:
		ASSERT(0);
	}
	SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS0_HTAVAIL, PMU_MAX_TRANSITION_DLY);
	ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS0_HTAVAIL));

	PMU_MSG(("Done masking\n"));

	/* Write PDIV in pllcontrol[0] */
	W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU0_PLL0_PLLCTL0);
	tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
	if (xt->freq >= PMU0_PLL0_PC0_PDIV_FREQ)
		tmp |= PMU0_PLL0_PC0_PDIV_MASK;
	else
		tmp &= ~PMU0_PLL0_PC0_PDIV_MASK;
	W_REG(osh, PMUREG(sih, pllcontrol_data), tmp);

	/* Write WILD in pllcontrol[1] */
	W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU0_PLL0_PLLCTL1);
	tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
	tmp = ((tmp & ~(PMU0_PLL0_PC1_WILD_INT_MASK | PMU0_PLL0_PC1_WILD_FRAC_MASK)) |
	       (((xt->wbint << PMU0_PLL0_PC1_WILD_INT_SHIFT) &
	         PMU0_PLL0_PC1_WILD_INT_MASK) |
	        ((xt->wbfrac << PMU0_PLL0_PC1_WILD_FRAC_SHIFT) &
	         PMU0_PLL0_PC1_WILD_FRAC_MASK)));
	if (xt->wbfrac == 0)
		tmp |= PMU0_PLL0_PC1_STOP_MOD;
	else
		tmp &= ~PMU0_PLL0_PC1_STOP_MOD;
	W_REG(osh, PMUREG(sih, pllcontrol_data), tmp);

	/* Write WILD in pllcontrol[2] */
	W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU0_PLL0_PLLCTL2);
	tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
	tmp = ((tmp & ~PMU0_PLL0_PC2_WILD_INT_MASK) |
	       ((xt->wbint >> PMU0_PLL0_PC2_WILD_INT_SHIFT) &
	        PMU0_PLL0_PC2_WILD_INT_MASK));
	W_REG(osh, PMUREG(sih, pllcontrol_data), tmp);

	PMU_MSG(("Done pll\n"));

	/* Write XtalFreq. Set the divisor also. */
	tmp = R_REG(osh, PMUREG(sih, pmucontrol));
	tmp = ((tmp & ~PCTL_ILP_DIV_MASK) |
	       (((((xt->freq + 127) / 128) - 1) << PCTL_ILP_DIV_SHIFT) & PCTL_ILP_DIV_MASK));
	tmp = ((tmp & ~PCTL_XTALFREQ_MASK) |
	       ((xt->xf << PCTL_XTALFREQ_SHIFT) & PCTL_XTALFREQ_MASK));
	W_REG(osh, PMUREG(sih, pmucontrol), tmp);
} /* si_pmu0_pllinit0 */

/** query alp/xtal clock frequency */
static uint32
BCMINITFN(si_pmu0_alpclk0)(si_t *sih, osl_t *osh, chipcregs_t *cc)
{
	const pmu0_xtaltab0_t *xt;
	uint32 xf;

	/* Find the frequency in the table */
	xf = (R_REG(osh, PMUREG(sih, pmucontrol)) & PCTL_XTALFREQ_MASK) >>
	        PCTL_XTALFREQ_SHIFT;
	for (xt = pmu0_xtaltab0; xt->freq; xt++)
		if (xt->xf == xf)
			break;
	/* PLL must be configured before */
	ASSERT(xt->freq);

	return xt->freq * 1000;
}

/** query CPU clock frequency */
static uint32
BCMINITFN(si_pmu0_cpuclk0)(si_t *sih, osl_t *osh, chipcregs_t *cc)
{
	uint32 tmp, divarm;
#ifdef BCMDBG
	uint32 pdiv, wbint, wbfrac, fvco;
	uint32 freq;
#endif
	uint32 FVCO = FVCO_880;

	/* Read divarm from pllcontrol[0] */
	W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU0_PLL0_PLLCTL0);
	tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
	divarm = (tmp & PMU0_PLL0_PC0_DIV_ARM_MASK) >> PMU0_PLL0_PC0_DIV_ARM_SHIFT;

#ifdef BCMDBG
	/* Calculate fvco based on xtal freq, pdiv, and wild */
	pdiv = tmp & PMU0_PLL0_PC0_PDIV_MASK;

	W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU0_PLL0_PLLCTL1);
	tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
	wbfrac = (tmp & PMU0_PLL0_PC1_WILD_FRAC_MASK) >> PMU0_PLL0_PC1_WILD_FRAC_SHIFT;
	wbint = (tmp & PMU0_PLL0_PC1_WILD_INT_MASK) >> PMU0_PLL0_PC1_WILD_INT_SHIFT;

	W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU0_PLL0_PLLCTL2);
	tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
	wbint += (tmp & PMU0_PLL0_PC2_WILD_INT_MASK) << PMU0_PLL0_PC2_WILD_INT_SHIFT;

	freq = si_pmu0_alpclk0(sih, osh, cc) / 1000;

	fvco = (freq * wbint) << 8;
	fvco += (freq * (wbfrac >> 10)) >> 2;
	fvco += (freq * (wbfrac & 0x3ff)) >> 10;
	fvco >>= 8;
	fvco >>= pdiv;
	fvco /= 1000;
	fvco *= 1000;

	PMU_MSG(("si_pmu0_cpuclk0: wbint %u wbfrac %u fvco %u\n",
	         wbint, wbfrac, fvco));

	FVCO = fvco;
#endif	/* BCMDBG */

	/* Return ARM/SB clock */
	return FVCO / (divarm + PMU0_PLL0_PC0_DIV_ARM_BASE) * 1000;
} /* si_pmu0_cpuclk0 */

/* setup pll and query clock speed */
typedef struct {
	uint16	fref;	/* x-tal frequency in [hz] */
	uint8	xf;	/* x-tal index as contained in PMU control reg, see PMU programmers guide */
	uint8	p1div;
	uint8	p2div;
	uint8	ndiv_int;
	uint32	ndiv_frac;
} pmu1_xtaltab0_t;

static const pmu1_xtaltab0_t BCMINITDATA(pmu1_xtaltab0_880_4329)[] = {
	{12000,	1,	3,	22,	0x9,	0xFFFFEF},
	{13000,	2,	1,	6,	0xb,	0x483483},
	{14400,	3,	1,	10,	0xa,	0x1C71C7},
	{15360,	4,	1,	5,	0xb,	0x755555},
	{16200,	5,	1,	10,	0x5,	0x6E9E06},
	{16800,	6,	1,	10,	0x5,	0x3Cf3Cf},
	{19200,	7,	1,	4,	0xb,	0x755555},
	{19800,	8,	1,	11,	0x4,	0xA57EB},
	{20000,	9,	1,	11,	0x4,	0x0},
	{24000,	10,	3,	11,	0xa,	0x0},
	{25000,	11,	5,	16,	0xb,	0x0},
	{26000,	12,	1,	1,	0x21,	0xD89D89},
	{30000,	13,	3,	8,	0xb,	0x0},
	{37400,	14,	3,	1,	0x46,	0x969696},
	{38400,	15,	1,	1,	0x16,	0xEAAAAA},
	{40000,	16,	1,	2,	0xb,	0},
	{0,	0,	0,	0,	0,	0}
};

/* the following table is based on 880Mhz fvco */
static const pmu1_xtaltab0_t BCMINITDATA(pmu1_xtaltab0_880)[] = {
	{12000,	1,	3,	22,	0x9,	0xFFFFEF},
	{13000,	2,	1,	6,	0xb,	0x483483},
	{14400,	3,	1,	10,	0xa,	0x1C71C7},
	{15360,	4,	1,	5,	0xb,	0x755555},
	{16200,	5,	1,	10,	0x5,	0x6E9E06},
	{16800,	6,	1,	10,	0x5,	0x3Cf3Cf},
	{19200,	7,	1,	4,	0xb,	0x755555},
	{19800,	8,	1,	11,	0x4,	0xA57EB},
	{20000,	9,	1,	11,	0x4,	0x0},
	{24000,	10,	3,	11,	0xa,	0x0},
	{25000,	11,	5,	16,	0xb,	0x0},
	{26000,	12,	1,	2,	0x10,	0xEC4EC4},
	{30000,	13,	3,	8,	0xb,	0x0},
	{33600,	14,	1,	2,	0xd,	0x186186},
	{38400,	15,	1,	2,	0xb,	0x755555},
	{40000,	16,	1,	2,	0xb,	0},
	{0,	0,	0,	0,	0,	0}
};

/* indices into pmu1_xtaltab0_880[] */
#define PMU1_XTALTAB0_880_12000K	0
#define PMU1_XTALTAB0_880_13000K	1
#define PMU1_XTALTAB0_880_14400K	2
#define PMU1_XTALTAB0_880_15360K	3
#define PMU1_XTALTAB0_880_16200K	4
#define PMU1_XTALTAB0_880_16800K	5
#define PMU1_XTALTAB0_880_19200K	6
#define PMU1_XTALTAB0_880_19800K	7
#define PMU1_XTALTAB0_880_20000K	8
#define PMU1_XTALTAB0_880_24000K	9
#define PMU1_XTALTAB0_880_25000K	10
#define PMU1_XTALTAB0_880_26000K	11
#define PMU1_XTALTAB0_880_30000K	12
#define PMU1_XTALTAB0_880_37400K	13
#define PMU1_XTALTAB0_880_38400K	14
#define PMU1_XTALTAB0_880_40000K	15

/* the following table is based on 1760Mhz fvco */
static const pmu1_xtaltab0_t BCMINITDATA(pmu1_xtaltab0_1760)[] = {
	{12000,	1,	3,	44,	0x9,	0xFFFFEF},
	{13000,	2,	1,	12,	0xb,	0x483483},
	{14400,	3,	1,	20,	0xa,	0x1C71C7},
	{15360,	4,	1,	10,	0xb,	0x755555},
	{16200,	5,	1,	20,	0x5,	0x6E9E06},
	{16800,	6,	1,	20,	0x5,	0x3Cf3Cf},
	{19200,	7,	1,	18,	0x5,	0x17B425},
	{19800,	8,	1,	22,	0x4,	0xA57EB},
	{20000,	9,	1,	22,	0x4,	0x0},
	{24000,	10,	3,	22,	0xa,	0x0},
	{25000,	11,	5,	32,	0xb,	0x0},
	{26000,	12,	1,	4,	0x10,	0xEC4EC4},
	{30000,	13,	3,	16,	0xb,	0x0},
	{38400,	14,	1,	10,	0x4,	0x955555},
	{40000,	15,	1,	4,	0xb,	0},
	{0,	0,	0,	0,	0,	0}
};

/* indices into pmu1_xtaltab0_1760[] */
#define PMU1_XTALTAB0_1760_12000K	0
#define PMU1_XTALTAB0_1760_13000K	1
#define PMU1_XTALTAB0_1760_14400K	2
#define PMU1_XTALTAB0_1760_15360K	3
#define PMU1_XTALTAB0_1760_16200K	4
#define PMU1_XTALTAB0_1760_16800K	5
#define PMU1_XTALTAB0_1760_19200K	6
#define PMU1_XTALTAB0_1760_19800K	7
#define PMU1_XTALTAB0_1760_20000K	8
#define PMU1_XTALTAB0_1760_24000K	9
#define PMU1_XTALTAB0_1760_25000K	10
#define PMU1_XTALTAB0_1760_26000K	11
#define PMU1_XTALTAB0_1760_30000K	12
#define PMU1_XTALTAB0_1760_38400K	13
#define PMU1_XTALTAB0_1760_40000K	14

/* the following table is based on 1440Mhz fvco */
static const pmu1_xtaltab0_t BCMINITDATA(pmu1_xtaltab0_1440)[] = {
	{12000,	1,	1,	1,	0x78,	0x0	},
	{13000,	2,	1,	1,	0x6E,	0xC4EC4E},
	{14400,	3,	1,	1,	0x64,	0x0	},
	{15360,	4,	1,	1,	0x5D,	0xC00000},
	{16200,	5,	1,	1,	0x58,	0xE38E38},
	{16800,	6,	1,	1,	0x55,	0xB6DB6D},
	{19200,	7,	1,	1,	0x4B,	0	},
	{19800,	8,	1,	1,	0x48,	0xBA2E8B},
	{20000,	9,	1,	1,	0x48,	0x0	},
	{25000,	10,	1,	1,	0x39,	0x999999},
	{26000, 11,     1,      1,      0x37,   0x627627},
	{30000,	12,	1,	1,	0x30,	0x0	},
	{37400, 13,     2,      1,     	0x4D, 	0x15E76	},
	{38400, 13,     2,      1,     	0x4B, 	0x0	},
	{40000,	14,	2,	1,	0x48,	0x0	},
	{48000,	15,	2,	1,	0x3c,	0x0	},
	{0,	0,	0,	0,	0,	0}
};

/* indices into pmu1_xtaltab0_1440[] */
#define PMU1_XTALTAB0_1440_12000K	0
#define PMU1_XTALTAB0_1440_13000K	1
#define PMU1_XTALTAB0_1440_14400K	2
#define PMU1_XTALTAB0_1440_15360K	3
#define PMU1_XTALTAB0_1440_16200K	4
#define PMU1_XTALTAB0_1440_16800K	5
#define PMU1_XTALTAB0_1440_19200K	6
#define PMU1_XTALTAB0_1440_19800K	7
#define PMU1_XTALTAB0_1440_20000K	8
#define PMU1_XTALTAB0_1440_25000K	9
#define PMU1_XTALTAB0_1440_26000K	10
#define PMU1_XTALTAB0_1440_30000K	11
#define PMU1_XTALTAB0_1440_37400K	12
#define PMU1_XTALTAB0_1440_38400K	13
#define PMU1_XTALTAB0_1440_40000K	14
#define PMU1_XTALTAB0_1440_48000K	15

#define XTAL_FREQ_24000MHZ		24000
#define XTAL_FREQ_30000MHZ		30000
#define XTAL_FREQ_37400MHZ		37400
#define XTAL_FREQ_48000MHZ		48000

/* 'xf' values corresponding to the 'xf' definition in the PMU control register */
enum xtaltab0_960 {
	XTALTAB0_960_12000K = 1,
	XTALTAB0_960_13000K,
	XTALTAB0_960_14400K,
	XTALTAB0_960_15360K,
	XTALTAB0_960_16200K,
	XTALTAB0_960_16800K,
	XTALTAB0_960_19200K,
	XTALTAB0_960_19800K,
	XTALTAB0_960_20000K,
	XTALTAB0_960_24000K,
	XTALTAB0_960_25000K,
	XTALTAB0_960_26000K,
	XTALTAB0_960_30000K,
	XTALTAB0_960_33600K,
	XTALTAB0_960_37400K,
	XTALTAB0_960_38400K,
	XTALTAB0_960_40000K,
	XTALTAB0_960_48000K,
	XTALTAB0_960_52000K
};

/**
 * given an x-tal frequency, this table specifies the PLL params to use to generate a 960Mhz output
 * clock. This output clock feeds the clock divider network. The defines of the form
 * PMU1_XTALTAB0_960_* index into this array.
 */
static const pmu1_xtaltab0_t BCMINITDATA(pmu1_xtaltab0_960)[] = {
/*	fref      xf                        p1div   p2div  ndiv_int  ndiv_frac */
	{12000,   1,       1,      1,     0x50,   0x0     }, /* array index 0 */
	{13000,   2,       1,      1,     0x49,   0xD89D89},
	{14400,   3,       1,      1,     0x42,   0xAAAAAA},
	{15360,   4,       1,      1,     0x3E,   0x800000},
	{16200,   5,       1,      1,     0x3B,   0x425ED0},
	{16800,   6,       1,      1,     0x39,   0x249249},
	{19200,   7,       1,      1,     0x32,   0x0     },
	{19800,   8,       1,      1,     0x30,   0x7C1F07},
	{20000,   9,       1,      1,     0x30,   0x0     },
	{24000,   10,      1,      1,     0x28,   0x0     },
	{25000,   11,      1,      1,     0x26,   0x666666}, /* array index 10 */
	{26000,   12,      1,      1,     0x24,   0xEC4EC4},
	{30000,   13,      1,      1,     0x20,   0x0     },
	{33600,   14,      1,      1,     0x1C,   0x924924},
	{37400,   15,      2,      1,     0x33,   0x563EF9},
	{38400,   16,      2,      1,     0x32,   0x0	  },
	{40000,   17,      2,      1,     0x30,   0x0     },
	{48000,   18,      2,      1,     0x28,   0x0     },
	{52000,   19,      2,      1,     0x24,   0xEC4EC4}, /* array index 18 */
	{0,	      0,       0,      0,     0,      0	      }
};

/* Indices into array pmu1_xtaltab0_960[]. Keep array and these defines synchronized. */
#define PMU1_XTALTAB0_960_12000K	0
#define PMU1_XTALTAB0_960_13000K	1
#define PMU1_XTALTAB0_960_14400K	2
#define PMU1_XTALTAB0_960_15360K	3
#define PMU1_XTALTAB0_960_16200K	4
#define PMU1_XTALTAB0_960_16800K	5
#define PMU1_XTALTAB0_960_19200K	6
#define PMU1_XTALTAB0_960_19800K	7
#define PMU1_XTALTAB0_960_20000K	8
#define PMU1_XTALTAB0_960_24000K	9
#define PMU1_XTALTAB0_960_25000K	10
#define PMU1_XTALTAB0_960_26000K	11
#define PMU1_XTALTAB0_960_30000K	12
#define PMU1_XTALTAB0_960_33600K	13
#define PMU1_XTALTAB0_960_37400K	14
#define PMU1_XTALTAB0_960_38400K	15
#define PMU1_XTALTAB0_960_40000K	16
#define PMU1_XTALTAB0_960_48000K	17
#define PMU1_XTALTAB0_960_52000K	18

#define PMU15_XTALTAB0_12000K	0
#define PMU15_XTALTAB0_20000K	1
#define PMU15_XTALTAB0_26000K	2
#define PMU15_XTALTAB0_37400K	3
#define PMU15_XTALTAB0_52000K	4
#define PMU15_XTALTAB0_END	5

/* For having the pllcontrol data (info)
 * The table with the values of the registers will have one - one mapping.
 */
typedef struct {
	uint16 	clock;	/* x-tal frequency in [KHz] */
	uint8	mode;	/* spur mode */
	uint8	xf;	/* corresponds with xf bitfield in PMU control register */
} pllctrl_data_t;

/*  *****************************  tables for 4335a0 *********************** */

#ifdef BCM_BOOTLOADER
/**
 * PLL control register table giving info about the xtal supported for 4335.
 * There should be a one to one mapping between pmu1_pllctrl_tab_4335_960mhz[] and this table.
 */
static const pllctrl_data_t pmu1_xtaltab0_4335[] = {
	{12000, 0, XTALTAB0_960_12000K},
	{13000, 0, XTALTAB0_960_13000K},
	{14400, 0, XTALTAB0_960_14400K},
	{15360, 0, XTALTAB0_960_15360K},
	{16200, 0, XTALTAB0_960_16200K},
	{16800, 0, XTALTAB0_960_16800K},
	{19200, 0, XTALTAB0_960_19200K},
	{19800, 0, XTALTAB0_960_19800K},
	{20000, 0, XTALTAB0_960_20000K},
	{24000, 0, XTALTAB0_960_24000K},
	{25000, 0, XTALTAB0_960_25000K},
	{26000, 0, XTALTAB0_960_26000K},
	{30000, 0, XTALTAB0_960_30000K},
	{33600, 0, XTALTAB0_960_33600K},
	{37400, 0, XTALTAB0_960_37400K},
	{38400, 0, XTALTAB0_960_38400K},
	{40000, 0, XTALTAB0_960_40000K},
	{48000, 0, XTALTAB0_960_48000K},
	{52000, 0, XTALTAB0_960_52000K},
};

/**
 * PLL control register values(all registers) for the xtal supported for 4335.
 * There should be a one to one mapping between pmu1_xtaltab0_4335[] and this table.
 */
static const uint32	pmu1_pllctrl_tab_4335_960mhz[] = {
/*      PLL 0       PLL 1       PLL 2       PLL 3       PLL 4       PLL 5                */
	0x50800000, 0x0C080803, 0x28010814, 0x61000000, 0x02600005, 0x0004FFFD, /* 12000 */
	0x50800000, 0x0C080803, 0x24B10814, 0x40D89D89, 0x02600005, 0x00049D87, /* 13000 */
	0x50800000, 0x0C080803, 0x21310814, 0x40AAAAAA, 0x02600005, 0x00042AA8, /* 14400 */
	0x50800000, 0x0C080803, 0x1F310814, 0x40800000, 0x02600005, 0x0003E7FE, /* 15360 */
	0x50800000, 0x0C080803, 0x1DB10814, 0x20425ED0, 0x02600005, 0x0003B424, /* 16200 */
	0x50800000, 0x0C080803, 0x1CB10814, 0x20249249, 0x02600005, 0x00039247, /* 16800 */
	0x50800000, 0x0C080803, 0x19010814, 0x01000000, 0x02600005, 0x00031FFE, /* 19200 */
	0x50800000, 0x0C080803, 0x18310814, 0x007C1F07, 0x02600005, 0x000307C0, /* 19800 */
	0x50800000, 0x0C080803, 0x18010814, 0x01000000, 0x02600005, 0x0002FFFE, /* 20000 */
	0x50800000, 0x0C080803, 0x14010814, 0xC1000000, 0x02600004, 0x00027FFE, /* 24000 */
	0x50800000, 0x0C080803, 0x13310814, 0xC0666666, 0x02600004, 0x00026665, /* 25000 */
	0x50800000, 0x0C080803, 0x12310814, 0xC0EC4EC4, 0x02600004, 0x00024EC4, /* 26000 */
	0x50800000, 0x0C080803, 0x10010814, 0xA1000000, 0x02600004, 0x0001FFFF, /* 30000 */
	0x50800000, 0x0C080803, 0x0E310814, 0xA0924924, 0x02600004, 0x0001C923, /* 33600 */
	0x50800000, 0x0C080803, 0x0CB10814, 0x80AB1F7C, 0x02600004, 0x00019AB1, /* 37400 */
	0x50800000, 0x0C080803, 0x0C810814, 0x81000000, 0x02600004, 0x00018FFF, /* 38400 */
	0x50800000, 0x0C080803, 0x0C010814, 0x81000000, 0x02600004, 0x00017FFF, /* 40000 */
	0x50800000, 0x0C080803, 0x0A010814, 0x61000000, 0x02600004, 0x00013FFF, /* 48000 */
	0x50800000, 0x0C080803, 0x09310814, 0x60762762, 0x02600004, 0x00012762, /* 52000 */
};

#else /* BCM_BOOTLOADER */
/**
 * PLL control register table giving info about the xtal supported for 4335.
 * There should be a one to one mapping between pmu1_pllctrl_tab_4335_968mhz[] and this table.
 */
static const pllctrl_data_t pmu1_xtaltab0_4335_drv[] = {
	{37400, 0, XTALTAB0_960_37400K},
	{40000, 0, XTALTAB0_960_40000K},
};


/**
 * PLL control register values(all registers) for the xtal supported for 4335.
 * There should be a one to one mapping between pmu1_xtaltab0_4335_drv[] and this table.
 */
/*
 * This table corresponds to spur mode 8. This bbpll settings will be used for WLBGA Bx
 * as well as Cx
 */
static const uint32	pmu1_pllctrl_tab_4335_968mhz[] = {
/*      PLL 0       PLL 1       PLL 2       PLL 3       PLL 4       PLL 5                */
	0x50800000, 0x0A060803, 0x0CB10806, 0x80E1E1E2, 0x02600004, 0x00019AB1,	/* 37400 KHz */
	0x50800000, 0x0A060803, 0x0C310806, 0x80333333, 0x02600004, 0x00017FFF,	/* 40000 KHz */
};

/* This table corresponds to spur mode 2. This bbpll settings will be used for WLCSP B0 */
static const uint32	pmu1_pllctrl_tab_4335_961mhz[] = {
/*      PLL 0       PLL 1       PLL 2       PLL 3       PLL 4       PLL 5                */
	0x50800000, 0x0A060803, 0x0CB10806, 0x80B1F7C9, 0x02600004, 0x00019AB1,	/* 37400 KHz */
	0x50800000, 0x0A060803, 0x0C310806, 0x80066666, 0x02600004, 0x00017FFF,	/* 40000 KHz */
};

/* This table corresponds to spur mode 0. This bbpll settings will be used for WLCSP C0 */
static const uint32	pmu1_pllctrl_tab_4335_963mhz[] = {
/*      PLL 0       PLL 1       PLL 2       PLL 3       PLL 4       PLL 5                */
	0x50800000, 0x0A060803, 0x0CB10806, 0x80BFA863, 0x02600004, 0x00019AB1,	/* 37400 KHz */
	0x50800000, 0x0A060803, 0x0C310806, 0x80133333, 0x02600004, 0x00017FFF,	/* 40000 KHz */
};

#endif /* BCM_BOOTLOADER */

/*  ************************  tables for 4335a0 END *********************** */

/*  *****************************  tables for 4345 *********************** */

/* PLL control register table giving info about the xtal supported for 4345 series */
static const pllctrl_data_t BCMATTACHDATA(pmu1_xtaltab0_4345)[] = {
	{12000, 0, XTALTAB0_960_12000K},
	{13000, 0, XTALTAB0_960_13000K},
	{14400, 0, XTALTAB0_960_14400K},
	{15360, 0, XTALTAB0_960_15360K},
	{16200, 0, XTALTAB0_960_16200K},
	{16800, 0, XTALTAB0_960_16800K},
	{19200, 0, XTALTAB0_960_19200K},
	{19800, 0, XTALTAB0_960_19800K},
	{20000, 0, XTALTAB0_960_20000K},
	{24000, 0, XTALTAB0_960_24000K},
	{25000, 0, XTALTAB0_960_25000K},
	{26000, 0, XTALTAB0_960_26000K},
	{30000, 0, XTALTAB0_960_30000K},
	{33600, 0, XTALTAB0_960_33600K},
	{37400, 0, XTALTAB0_960_37400K},
	{38400, 0, XTALTAB0_960_38400K},
	{40000, 0, XTALTAB0_960_40000K},
	{48000, 0, XTALTAB0_960_48000K},
	{52000, 0, XTALTAB0_960_52000K},
};

/*  ************************  tables for 4345 END *********************** */

/*  *****************************  tables for 43242a0 *********************** */
/**
 * PLL control register table giving info about the xtal supported for 43242
 * There should be a one to one mapping between pmu1_pllctrl_tab_43242A0[]
 * and pmu1_pllctrl_tab_43242A1[] and this table.
 */
static const pllctrl_data_t BCMATTACHDATA(pmu1_xtaltab0_43242)[] = {
	{37400, 0, XTALTAB0_960_37400K},
};

/*  ************************  tables for 4324a02 END *********************** */

/*  *****************************  tables for 4350a0 *********************** */

#define XTAL_DEFAULT_4350	37400
/**
 * PLL control register table giving info about the xtal supported for 4350
 * There should be a one to one mapping between pmu1_pllctrl_tab_4350_963mhz[] and this table.
 */
static const pllctrl_data_t pmu1_xtaltab0_4350[] = {
/*       clock  mode xf */
	{37400, 0,   XTALTAB0_960_37400K},
	{40000, 0,   XTALTAB0_960_40000K},
};

/**
 * PLL control register table giving info about the xtal supported for 4335.
 * There should be a one to one mapping between pmu1_pllctrl_tab_4335_963mhz[] and this table.
 */
static const uint32	pmu1_pllctrl_tab_4350_963mhz[] = {
/*	PLL 0       PLL 1       PLL 2       PLL 3       PLL 4       PLL 5       PLL6         */
	0x50800000, 0x18060603, 0x0cb10814, 0x80bfaa00, 0x02600004, 0x00019AB1, 0x04a6c181,
	0x50800000, 0x18060603, 0x0C310814, 0x00133333, 0x02600004, 0x00017FFF, 0x04a6c181
};
static const uint32	pmu1_pllctrl_tab_4350C0_963mhz[] = {
/*	PLL 0       PLL 1       PLL 2       PLL 3       PLL 4       PLL 5       PLL6         */
	0x50800000, 0x08060603, 0x0cb10804, 0xe2bfaa00, 0x02600004, 0x00019AB1, 0x02a6c181,
	0x50800000, 0x08060603, 0x0C310804, 0xe2133333, 0x02600004, 0x00017FFF, 0x02a6c181
};
/*  ************************  tables for 4350a0 END *********************** */

/* PLL control register values(all registers) for the xtal supported for 43242.
 * There should be a one to one mapping for "pllctrl_data_t" and the table below.
 * only support 37.4M
 */
static const uint32	BCMATTACHDATA(pmu1_pllctrl_tab_43242A0)[] = {
/*      PLL 0       PLL 1       PLL 2       PLL 3       PLL 4       PLL 5                */
	0xA7400040, 0x10080A06, 0x0CB11408, 0x80AB1F7C, 0x02600004, 0x00A6C4D3, /* 37400 */
};

static const uint32	BCMATTACHDATA(pmu1_pllctrl_tab_43242A1)[] = {
/*      PLL 0       PLL 1       PLL 2       PLL 3       PLL 4       PLL 5                */
	0xA7400040, 0x10080A06, 0x0CB11408, 0x80AB1F7C, 0x02600004, 0x00A6C191, /* 37400 */
};

/* 4334/4314 ADFLL freq target params */
typedef struct {
	uint16  fref;		/* x-tal frequency in [hz] */
	uint8   xf;			/* x-tal index as given by PMU programmers guide */
	uint32  freq_tgt;	/* freq_target: N_divide_ratio bitfield in DFLL */
} pmu2_xtaltab0_t;

/**
 * If a DFLL clock of 480Mhz is desired, use this table to determine xf and freq_tgt for
 * a given x-tal frequency.
 */
static const pmu2_xtaltab0_t BCMINITDATA(pmu2_xtaltab0_adfll_480)[] = {
	{12000,		1,	0x4FFFC},
	{20000,		9,	0x2FFFD},
	{26000,		11,	0x24EC3},
	{37400,		13,	0x19AB1},
	{52000,		17,	0x12761},
	{0,		0,	0},
};

static const pmu2_xtaltab0_t BCMINITDATA(pmu2_xtaltab0_adfll_492)[] = {
	{12000,		1,	0x51FFC},
	{20000,		9,	0x31330},
	{26000,		11,	0x25D88},
	{37400,		13,	0x1A4F5},
	{52000,		17,	0x12EC4},
	{0,		0,	0}
};

static const pmu2_xtaltab0_t BCMINITDATA(pmu2_xtaltab0_adfll_485)[] = {
	{12000,		1,	0x50D52},
	{20000,		9,	0x307FE},
	{26000,		11,	0x254EA},
	{37400,		13,	0x19EF8},
	{52000,		17,	0x12A75},
	{0,		0,	0}
};

/** returns xtal table for each chip */
static const pmu1_xtaltab0_t *
BCMINITFN(si_pmu1_xtaltab0)(si_t *sih)
{
#ifdef BCMDBG
	char chn[8];
#endif
	switch (CHIPID(sih->chip)) {
	case BCM4325_CHIP_ID:
		return pmu1_xtaltab0_880;
	case BCM4329_CHIP_ID:
		return pmu1_xtaltab0_880_4329;
	case BCM4315_CHIP_ID:
		return pmu1_xtaltab0_1760;
	case BCM4319_CHIP_ID:
		return pmu1_xtaltab0_1440;
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
	case BCM43239_CHIP_ID:
	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
	case BCM4335_CHIP_ID:
	case BCM4360_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM4352_CHIP_ID:
	case BCM43526_CHIP_ID:
	case BCM4345_CHIP_ID:
	case BCM43602_CHIP_ID:
	case BCM43462_CHIP_ID:
	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
		return pmu1_xtaltab0_960;
	case BCM4330_CHIP_ID:
		if (CST4330_CHIPMODE_SDIOD(sih->chipst))
			return pmu1_xtaltab0_960;
		else
			return pmu1_xtaltab0_1440;
	default:
		PMU_MSG(("si_pmu1_xtaltab0: Unknown chipid %s\n", bcm_chipname(sih->chip, chn, 8)));
		break;
	}
	ASSERT(0);
	return NULL;
} /* si_pmu1_xtaltab0 */

/** returns chip specific PLL settings for default xtal frequency and VCO output frequency */
static const pmu1_xtaltab0_t *
BCMINITFN(si_pmu1_xtaldef0)(si_t *sih)
{
#ifdef BCMDBG
	char chn[8];
#endif

	switch (CHIPID(sih->chip)) {
	case BCM4325_CHIP_ID:
		/* Default to 26000Khz */
		return &pmu1_xtaltab0_880[PMU1_XTALTAB0_880_26000K];
	case BCM4329_CHIP_ID:
		/* Default to 38400Khz */
		return &pmu1_xtaltab0_880_4329[PMU1_XTALTAB0_880_38400K];
	case BCM4315_CHIP_ID:
#ifdef BCMUSBDEV
		/* Default to 30000Khz */
		return &pmu1_xtaltab0_1760[PMU1_XTALTAB0_1760_30000K];
#else
		/* Default to 26000Khz */
		return &pmu1_xtaltab0_1760[PMU1_XTALTAB0_1760_26000K];
#endif
	case BCM4319_CHIP_ID:
		/* Default to 30000Khz */
		return &pmu1_xtaltab0_1440[PMU1_XTALTAB0_1440_30000K];
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
	case BCM43239_CHIP_ID:
		/* Default to 26000Khz */
		return &pmu1_xtaltab0_960[PMU1_XTALTAB0_960_26000K];
	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
	case BCM4335_CHIP_ID:
	case BCM4360_CHIP_ID:
	case BCM4352_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM43526_CHIP_ID:
	case BCM4345_CHIP_ID:
	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
		/* Default to 37400Khz */
		return &pmu1_xtaltab0_960[PMU1_XTALTAB0_960_37400K];
	case BCM43602_CHIP_ID:
	case BCM43462_CHIP_ID:
		return &pmu1_xtaltab0_960[PMU1_XTALTAB0_960_40000K];

	case BCM4330_CHIP_ID:
		/* Default to 37400Khz */
		if (CST4330_CHIPMODE_SDIOD(sih->chipst))
			return &pmu1_xtaltab0_960[PMU1_XTALTAB0_960_37400K];
		else
			return &pmu1_xtaltab0_1440[PMU1_XTALTAB0_1440_37400K];
	default:
		PMU_MSG(("si_pmu1_xtaldef0: Unknown chipid %s\n", bcm_chipname(sih->chip, chn, 8)));
		break;
	}
	ASSERT(0);
	return NULL;
} /* si_pmu1_xtaldef0 */

/** returns chip specific default pll fvco frequency in [khz] units */
static uint32
BCMINITFN(si_pmu1_pllfvco0)(si_t *sih)
{
#ifdef BCMDBG
	char chn[8];
#endif

	switch (CHIPID(sih->chip)) {
	case BCM4325_CHIP_ID:
		return FVCO_880;
	case BCM4329_CHIP_ID:
		return FVCO_880;
	case BCM4315_CHIP_ID:
		return FVCO_1760;
	case BCM4319_CHIP_ID:
		return FVCO_1440;
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
	case BCM43239_CHIP_ID:
	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
	case BCM4360_CHIP_ID:
	case BCM4352_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM43526_CHIP_ID:
		return FVCO_960;

	case BCM43602_CHIP_ID:
	case BCM43462_CHIP_ID:
		return FVCO_960;

	case BCM4345_CHIP_ID:
	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
		return FVCO_963;
	case BCM4335_CHIP_ID:
	{
		osl_t *osh;

		osh = si_osh(sih);
		return (si_pmu_cal_fvco(sih, osh));
	}
	case BCM4330_CHIP_ID:
		if (CST4330_CHIPMODE_SDIOD(sih->chipst))
			return FVCO_960;
		else
			return FVCO_1440;
	default:
		PMU_MSG(("si_pmu1_pllfvco0: Unknown chipid %s\n", bcm_chipname(sih->chip, chn, 8)));
		break;
	}
	ASSERT(0);
	return 0;
} /* si_pmu1_pllfvco0 */

/** query alp/xtal clock frequency */
static uint32
BCMINITFN(si_pmu1_alpclk0)(si_t *sih, osl_t *osh, chipcregs_t *cc)
{
	const pmu1_xtaltab0_t *xt;
	uint32 xf;

	/* Find the frequency in the table */
	xf = (R_REG(osh, PMUREG(sih, pmucontrol)) & PCTL_XTALFREQ_MASK) >>
	        PCTL_XTALFREQ_SHIFT;
	for (xt = si_pmu1_xtaltab0(sih); xt != NULL && xt->fref != 0; xt ++)
		if (xt->xf == xf)
			break;
	/* Could not find it so assign a default value */
	if (xt == NULL || xt->fref == 0)
		xt = si_pmu1_xtaldef0(sih);
	ASSERT(xt != NULL && xt->fref != 0);

	return xt->fref * 1000;
}

/**
 * Before the PLL is switched off, the HT clocks need to be deactivated, and reactivated
 * when the PLL is switched on again.
 * This function returns the chip specific HT clock resources (HT and MACPHY clocks).
 */
static uint32
si_pmu_htclk_mask(si_t *sih)
{
	/* chip specific bit position of various resources */
	rsc_per_chip_t *rsc = si_pmu_get_rsc_positions(sih);

	uint32 ht_req = (PMURES_BIT(rsc->ht_avail) | PMURES_BIT(rsc->macphy_clkavail));

	switch (CHIPID(sih->chip))
	{
		case BCM4330_CHIP_ID:
			ht_req |= PMURES_BIT(RES4330_BBPLL_PWRSW_PU);
			break;
		case BCM43362_CHIP_ID:  /* Same HT_ vals as 4336 */
		case BCM4336_CHIP_ID:
			ht_req |= PMURES_BIT(RES4336_BBPLL_PWRSW_PU);
			break;
		case BCM4335_CHIP_ID:   /* Same HT_ vals as 4350 */
		case BCM4345_CHIP_ID:	/* Same HT_ vals as 4350 */
		case BCM43602_CHIP_ID:  /* Same HT_ vals as 4350 */
		case BCM43462_CHIP_ID:  /* Same HT_ vals as 4350 */
		case BCM4350_CHIP_ID:
		case BCM4354_CHIP_ID:
		case BCM4356_CHIP_ID:
		case BCM43556_CHIP_ID:
		case BCM43558_CHIP_ID:
		case BCM43566_CHIP_ID:
		case BCM43568_CHIP_ID:
		case BCM43569_CHIP_ID:
		case BCM43570_CHIP_ID:
			ht_req |= PMURES_BIT(rsc->ht_start);
			break;
		case BCM43143_CHIP_ID:
		case BCM43242_CHIP_ID:
			break;
		default:
			ASSERT(0);
			break;
	}

	return ht_req;
} /* si_pmu_htclk_mask */

void
si_pmu_minresmask_htavail_set(si_t *sih, osl_t *osh, bool set_clear)
{
	if (!set_clear) {
		switch (CHIPID(sih->chip)) {
		case BCM4313_CHIP_ID:
			if ((R_REG(osh, PMUREG(sih, min_res_mask))) &
				(PMURES_BIT(RES4313_HT_AVAIL_RSRC)))
				AND_REG(osh, PMUREG(sih, min_res_mask),
					~(PMURES_BIT(RES4313_HT_AVAIL_RSRC)));
			break;
		default:
			break;
		}
	}
}

uint
si_pll_minresmask_reset(si_t *sih, osl_t *osh)
{
	uint err = BCME_OK;

	switch (CHIPID(sih->chip)) {
		case BCM4313_CHIP_ID:
			/* write to min_res_mask 0x200d : clear min_rsrc_mask */
			AND_REG(osh, PMUREG(sih, min_res_mask),
				~(PMURES_BIT(RES4313_HT_AVAIL_RSRC)));
			OSL_DELAY(100);
			/* write to max_res_mask 0xBFFF: clear max_rsrc_mask */
			AND_REG(osh, PMUREG(sih, max_res_mask),
				~(PMURES_BIT(RES4313_HT_AVAIL_RSRC)));
			OSL_DELAY(100);
			/* write to max_res_mask 0xFFFF :set max_rsrc_mask */
			OR_REG(osh, PMUREG(sih, max_res_mask),
				(PMURES_BIT(RES4313_HT_AVAIL_RSRC)));
			break;
		default:
			PMU_ERROR(("%s: PLL reset not supported\n", __FUNCTION__));
			err = BCME_UNSUPPORTED;
			break;
	}
	return err;
}

uint32
BCMATTACHFN(si_pmu_def_alp_clock)(si_t *sih, osl_t *osh)
{
	uint32 clock = ALP_CLOCK;

	switch (CHIPID(sih->chip)) {
	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
	case BCM4335_CHIP_ID:
	case BCM4345_CHIP_ID:
	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
		clock = 37400*1000;
		break;
	case BCM43602_CHIP_ID:
	case BCM43462_CHIP_ID:
		clock = 40000 * 1000;
		break;
	}
	return clock;
}

/**
 * The BBPLL register set needs to be reprogrammed because the x-tal frequency is not known at
 * compile time, or a different spur mode is selected. This function writes appropriate values into
 * the BBPLL registers. It returns the 'xf', corresponding to the 'xf' bitfield in the PMU control
 * register.
 *     'xtal'             : xtal frequency in [KHz]
 *     'pllctrlreg_update': contains info on what entries to use in 'pllctrlreg_val' for the given
 *                          x-tal frequency and spur mode
 *     'pllctrlreg_val'   : contains a superset of the BBPLL values to write
 *
 * Note: if cc is NULL, this function returns xf, without programming PLL registers.
 * This function is only called for pmu1_ type chips, perhaps we should rename it.
 */
static uint8
BCMATTACHFN(si_pmu_pllctrlreg_update)(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal,
            uint8 spur_mode, const pllctrl_data_t *pllctrlreg_update, uint32 array_size,
            const uint32 *pllctrlreg_val)
{
	uint8 indx, reg_offset, xf = 0;
	uint8 pll_ctrlcnt = 0;

	ASSERT(pllctrlreg_update);

	if (sih->pmurev >= 5) {
		pll_ctrlcnt = (sih->pmucaps & PCAP5_PC_MASK) >> PCAP5_PC_SHIFT;
	} else {
		pll_ctrlcnt = (sih->pmucaps & PCAP_PC_MASK) >> PCAP_PC_SHIFT;
	}

	/* Program the PLL control register if the xtal value matches with the table entry value */
	for (indx = 0; indx < array_size; indx++) {
		/* If the entry does not match the xtal and spur_mode just continue the loop */
		if (!((pllctrlreg_update[indx].clock == (uint16)xtal) &&
			(pllctrlreg_update[indx].mode == spur_mode)))
			continue;
		/*
		 * Don't program the PLL registers if register base is NULL.
		 * If NULL just return the xref.
		 */
		if (cc) {
			for (reg_offset = 0; reg_offset < pll_ctrlcnt; reg_offset++) {
				W_REG(osh, PMUREG(sih, pllcontrol_addr), reg_offset);
				W_REG(osh, PMUREG(sih, pllcontrol_data),
					pllctrlreg_val[indx*pll_ctrlcnt + reg_offset]);
			}
		}
		xf = pllctrlreg_update[indx].xf;
		break;
	}
	return xf;
} /* si_pmu_pllctrlreg_update */

static void
BCMATTACHFN(si_pmu_set_4345_pllcontrol_regs)(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal)
{
/* these defaults come from the recommeded values defined on the 4345 confluence PLL page */
/* Backplane/ARM CR4 clock controlled by m3div bits 23:16 of PLL_CONTROL1
 * 120Mhz : m3div = 0x8
 * 160Mhz : m3div = 0x6
 * 240Mhz : m3div = 0x4
 */
#define PLL_4345_CONTROL0_DEFAULT       0x50800000
#define PLL_4345_CONTROL1_DEFAULT       0x0C060803
#define PLL_4345_CONTROL2_DEFAULT       0x0CB10806
#define PLL_4345_CONTROL3_DEFAULT       0xE2BFA862
#define PLL_4345_CONTROL4_DEFAULT       0x02680004
#define PLL_4345_CONTROL5_DEFAULT       0x00019AB1
#define PLL_4345_CONTROL6_DEFAULT       0x005360C9
#define PLL_4345_CONTROL7_DEFAULT       0x000AB1F7

	uint32 PLL_control[8] = {
		PLL_4345_CONTROL0_DEFAULT, PLL_4345_CONTROL1_DEFAULT,
		PLL_4345_CONTROL2_DEFAULT, PLL_4345_CONTROL3_DEFAULT,
		PLL_4345_CONTROL4_DEFAULT, PLL_4345_CONTROL5_DEFAULT,
		PLL_4345_CONTROL6_DEFAULT, PLL_4345_CONTROL7_DEFAULT
	};
	uint32 fvco = si_pmu1_pllfvco0(sih);	/* in [khz] */
	uint32 ndiv_int;
	uint32 ndiv_frac;
	uint32 temp_high, temp_low;
	uint8 p1div;
	uint8 ndiv_mode;
	uint8 i;
	uint32 min_res_mask = 0, max_res_mask = 0, clk_ctl_st = 0;

	ASSERT(cc != NULL);
	ASSERT(xtal <= 0xFFFFFFFF / 1000);

	/* force the HT off  */
	si_pmu_pll_off(sih, osh, cc, &min_res_mask, &max_res_mask, &clk_ctl_st);

#ifdef SRFAST
	/* HW4345-446: PMU4345: Add support for calibrated internal clock oscillator */
	si_pmu_pllcontrol(sih, 1, PMU1_PLL0_PC1_M4DIV_MASK,
		PMU1_PLL0_PC1_M4DIV_BY_60 << PMU1_PLL0_PC1_M4DIV_SHIFT);

	/* Start SR calibration: enable SR ext clk */
	si_pmu_regcontrol(sih, 6, VREG6_4350_SR_EXT_CLKEN_MASK,
		(1 << VREG6_4350_SR_EXT_CLKEN_SHIFT));
	OSL_DELAY(30); /* Wait 30us */
	/* Stop SR calibration: disable SR ext clk */
	si_pmu_regcontrol(sih, 6, VREG6_4350_SR_EXT_CLKEN_MASK,
		(0 << VREG6_4350_SR_EXT_CLKEN_SHIFT));
#endif

	/* xtal and FVCO are in kHz.  xtal/p1div must be <= 50MHz */
	p1div = 1 + (uint8) ((xtal * 1000) / 50000000UL);
	ndiv_int = (fvco * p1div) / xtal;

	/* ndiv_frac = (uint32) (((uint64) (fvco * p1div - xtal * ndiv_int) * (1 << 24)) / xtal) */
	bcm_uint64_multiple_add(&temp_high, &temp_low, fvco * p1div - xtal * ndiv_int, 1 << 24, 0);
	bcm_uint64_divide(&ndiv_frac, temp_high, temp_low, xtal);

	ndiv_mode = (ndiv_frac == 0) ? 0 : 3;

	/* change PLL_control[2] and PLL_control[3] */
	PLL_control[2] = (PLL_4345_CONTROL2_DEFAULT & 0x0000FFFF) |
	                 (p1div << 16) | (ndiv_mode << 20) | (ndiv_int << 23);
	PLL_control[3] = (PLL_4345_CONTROL3_DEFAULT & 0xFF000000) | ndiv_frac;

	/* TODO - set PLL control field in PLL_control[3] & PLL_control[4] */

	/* HSIC DFLL freq_target N_divide_ratio = 4096 * FVCO / xtal */
	fvco = FVCO_960;	/* USB/HSIC FVCO is always 960 MHz, regardless of BB FVCO */
	PLL_control[5] = (PLL_4345_CONTROL5_DEFAULT & 0xFFF00000) |
	                 ((((uint32) fvco << 12) / xtal) & 0x000FFFFF);

	ndiv_int = (fvco * p1div) / xtal;

	/*
	 * ndiv_frac = (uint32) (((uint64) (fvco * p1div - xtal * ndiv_int) * (1 << 20)) /
	 *                       xtal)
	 */
	bcm_uint64_multiple_add(&temp_high, &temp_low, fvco * p1div - xtal * ndiv_int, 1 << 20, 0);
	bcm_uint64_divide(&ndiv_frac, temp_high, temp_low, xtal);

	/* change PLL_control[6] */
	PLL_control[6] = (PLL_4345_CONTROL6_DEFAULT & 0xFFFFE000) | p1div | (ndiv_int << 3);

	/* change PLL_control[7] */
	PLL_control[7] = ndiv_frac;

	/* write PLL Control Regs */
	PMU_MSG(("xtal    PLLCTRL0   PLLCTRL1   PLLCTRL2   PLLCTRL3"));
	PMU_MSG(("   PLLCTRL4   PLLCTRL5   PLLCTRL6   PLLCTRL7\n"));
	PMU_MSG(("%d ", xtal));
	for (i = 0; i < 6; i++) {
		PMU_MSG((" 0x%08X", PLL_control[i]));
		W_REG(osh, PMUREG(sih, pllcontrol_addr), i);
		W_REG(osh, PMUREG(sih, pllcontrol_data), PLL_control[i]);
	}
	PMU_MSG(("\n"));

	/* Now toggle pllctlupdate so the pll sees the new values */
	si_pmu_pllupd(sih);

	/* Need to toggle PLL's dreset_i signal to ensure output clocks are aligned */
	si_pmu_chipcontrol(sih, CHIPCTRLREG1, (1<<6), (0<<6));
	si_pmu_chipcontrol(sih, CHIPCTRLREG1, (1<<6), (1<<6));
	si_pmu_chipcontrol(sih, CHIPCTRLREG1, (1<<6), (0<<6));

	/* enable HT back on  */
	si_pmu_pll_on(sih, osh, cc, min_res_mask, max_res_mask, clk_ctl_st);
} /* si_pmu_set_4345_pllcontrol_regs */

/**
 * Chip-specific overrides to PLLCONTROL registers during init. If certain conditions (dependent on
 * x-tal frequency and current ALP frequency) are met, an update of the PLL is required.
 *
 * This takes less precedence over OTP PLLCONTROL overrides.
 * If update_required=FALSE, it returns TRUE if a update is about to occur.
 * No write happens.
 *
 * Return value: TRUE if the BBPLL registers 'update' field should be written by the caller.
 *
 * This function is only called for pmu1_ type chips, perhaps we should rename it.
 */
bool
BCMATTACHFN(si_pmu_update_pllcontrol)(si_t *sih, osl_t *osh, uint32 xtal, bool update_required)
{
	chipcregs_t *cc;
	uint origidx;
	bool write_en = FALSE;
	uint8 xf = 0;
	const pmu1_xtaltab0_t *xt;
	uint32 tmp, buf_strength = 0;
	const pllctrl_data_t *pllctrlreg_update = NULL;
	uint32 array_size = 0;
	/* points at a set of PLL register values to write for a given x-tal frequency: */
	const uint32 *pllctrlreg_val = NULL;
	uint8 ndiv_mode = PMU1_PLL0_PC2_NDIV_MODE_MASH;
	uint32 xtalfreq = 0;

	/* If there is OTP or NVRAM entry for xtalfreq, program the
	 * PLL control register even if it is default xtal.
	 */
	xtalfreq = getintvar(NULL, rstr_xtalfreq);
	/* CASE1 */
	if (xtalfreq) {
		write_en = TRUE;
		xtal = xtalfreq;
	} else {
		/* There is NO OTP value */
		if (xtal) {
			/* CASE2: If the xtal value was calculated, program the PLL control
			 * registers only if it is not default xtal value.
			 */
			if (xtal != (si_pmu_def_alp_clock(sih, osh)/1000))
				write_en = TRUE;
		} else {
			/* CASE3: If the xtal obtained is "0", ie., clock is not measured, then
			 * leave the PLL control register as it is but program the xf in
			 * pmucontrol register with the default xtal value.
			 */
			xtal = si_pmu_def_alp_clock(sih, osh)/1000;
		}
	}

	switch (CHIPID(sih->chip)) {
	case BCM43239_CHIP_ID:
#ifndef BCM_BOOTLOADER
		write_en = TRUE;
#endif
		break;

	case BCM4335_CHIP_ID:
#ifdef BCM_BOOTLOADER
		pllctrlreg_update = pmu1_xtaltab0_4335;
		array_size = ARRAYSIZE(pmu1_xtaltab0_4335);
		pllctrlreg_val = pmu1_pllctrl_tab_4335_960mhz;
#else /* BCM_BOOTLOADER */
		pllctrlreg_update = pmu1_xtaltab0_4335_drv;
		array_size = ARRAYSIZE(pmu1_xtaltab0_4335_drv);
		if (sih->chippkg == BCM4335_WLBGA_PKG_ID) {
			pllctrlreg_val = pmu1_pllctrl_tab_4335_968mhz;
		} else {
			if (CHIPREV(sih->chiprev) <= 1) {
				/* for 4335 Ax/Bx Chips */
				pllctrlreg_val = pmu1_pllctrl_tab_4335_961mhz;
			} else if (CHIPREV(sih->chiprev) == 2) {
				/* for 4335 Cx chips */
				pllctrlreg_val = pmu1_pllctrl_tab_4335_968mhz;
			}
		}
#endif /* BCM_BOOTLOADER */

#ifndef BCM_BOOTLOADER
		/* If PMU1_PLL0_PC2_MxxDIV_MASKxx have to change,
		 * then set write_en to true.
		 */
		write_en = TRUE;
#endif
		break;

	case BCM4345_CHIP_ID:
		pllctrlreg_update = pmu1_xtaltab0_4345;
		array_size = ARRAYSIZE(pmu1_xtaltab0_4345);
		/* Note: no pllctrlreg_val table, because the PLL ctrl regs are calculated */

#ifndef BCM_BOOTLOADER
		/* If PMU1_PLL0_PC2_MxxDIV_MASKxx have to change,
		 * then set write_en to true.
		 */
		write_en = TRUE;
#endif
		break;

	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
		pllctrlreg_update = pmu1_xtaltab0_4350;
		array_size = ARRAYSIZE(pmu1_xtaltab0_4350);

		if (CHIPID(sih->chip) == BCM4354_CHIP_ID ||
			CHIPID(sih->chip) == BCM4356_CHIP_ID ||
			CHIPID(sih->chip) == BCM43569_CHIP_ID ||
			CHIPID(sih->chip) == BCM43570_CHIP_ID)
			pllctrlreg_val = pmu1_pllctrl_tab_4350C0_963mhz;
		else {
			if (CHIPREV(sih->chiprev) >= 3)
				pllctrlreg_val = pmu1_pllctrl_tab_4350C0_963mhz;
			else
				pllctrlreg_val = pmu1_pllctrl_tab_4350_963mhz;
		}

		/* If PMU1_PLL0_PC2_MxxDIV_MASKxx have to change,
		 * then set write_en to true.
		 */
#ifdef BCMUSB_NODISCONNECT
		write_en = FALSE;
#else
		write_en = TRUE;
#endif
#ifdef BCM_BOOTLOADER
		/* Bootloader need to change pll if it is not default 37.4M */
		if (xtal == XTAL_DEFAULT_4350)
			write_en = FALSE;
#endif /*  BCM_BOOTLOADER */
		break;

	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
		pllctrlreg_update = pmu1_xtaltab0_43242;
		array_size = ARRAYSIZE(pmu1_xtaltab0_43242);
		if (CHIPREV(sih->chiprev) == 0) {
			pllctrlreg_val = pmu1_pllctrl_tab_43242A0;
		} else {
			pllctrlreg_val = pmu1_pllctrl_tab_43242A1;
		}
		write_en = TRUE;
		break;
	case BCM43602_CHIP_ID:
	case BCM43462_CHIP_ID:
		/*
		 * XXX43602 has only 1 x-tal value, possibly insert case when an other BBPLL
		 * frequency than 960Mhz is required (e.g., for spur avoidance)
		 */
		 /* fall through */
	default:
		/* write_en is FALSE in this case. So returns from the function */
		write_en = FALSE;
		break;
	}

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	/* Check if the table has PLL control register values for the requested
	 * xtal. NOTE THAT, THIS IS not DONE FOR 43239,
	 * AS IT HAS ONLY ONE XTAL SUPPORT.
	 */
	if (!update_required && pllctrlreg_update) {
		/* Here the chipcommon register base is passed as NULL, so that we just get
		 * the xf for the xtal being programmed but don't program the registers now
		 * as the PLL is not yet turned OFF.
		 */
		xf = si_pmu_pllctrlreg_update(sih, osh, NULL, xtal, 0, pllctrlreg_update,
			array_size, pllctrlreg_val);

		/* Program the PLL based on the xtal value. */
		if (xf != 0) {
			/* Write XtalFreq. Set the divisor also. */
			tmp = R_REG(osh, PMUREG(sih, pmucontrol)) &
				~(PCTL_ILP_DIV_MASK | PCTL_XTALFREQ_MASK);
			tmp |= (((((xtal + 127) / 128) - 1) << PCTL_ILP_DIV_SHIFT) &
				PCTL_ILP_DIV_MASK) |
				((xf << PCTL_XTALFREQ_SHIFT) & PCTL_XTALFREQ_MASK);
			W_REG(osh, PMUREG(sih, pmucontrol), tmp);
		} else {
			write_en = FALSE;
			printf(rstr_Invalid_Unsupported_xtal_value_D, xtal);
		}
	}

	/* If its a check sequence or if there is nothing to write, return here */
	if ((update_required == FALSE) || (write_en == FALSE)) {
		goto exit;
	}

	/* Update the PLL control register based on the xtal used. */
	if (pllctrlreg_val) {
		si_pmu_pllctrlreg_update(sih, osh, cc, xtal, 0, pllctrlreg_update, array_size,
			pllctrlreg_val);
	}

	/* Chip specific changes to PLL Control registers is done here. */
	switch (CHIPID(sih->chip)) {
	case BCM43239_CHIP_ID:
#ifndef BCM_BOOTLOADER
		/* 43239: Change backplane and dot11mac clock to 120Mhz */
		si_pmu_pllcontrol(sih, PMU1_PLL0_PLLCTL2,
		  (PMU1_PLL0_PC2_M5DIV_MASK | PMU1_PLL0_PC2_M6DIV_MASK),
		  ((8 << PMU1_PLL0_PC2_M5DIV_SHIFT) | (8 << PMU1_PLL0_PC2_M6DIV_SHIFT)));
#endif
		/* To ensure the PLL control registers are not modified from what is default. */
		xtal = 0;
		break;
	case BCM4324_CHIP_ID:
#ifndef BCM_BOOTLOADER
		/* Change backplane clock (ARM input) to 137Mhz */
		si_pmu_pllcontrol(sih, PMU1_PLL0_PLLCTL1, PMU1_PLL0_PC1_M2DIV_MASK,
			(7 << PMU1_PLL0_PC1_M2DIV_SHIFT));
#endif
		break;
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
#ifndef BCM_BOOTLOADER
		/* Change backplane clock (ARM input) to 137Mhz */
		si_pmu_pllcontrol(sih, PMU1_PLL0_PLLCTL1, PMU1_PLL0_PC1_M2DIV_MASK,
			(7 << PMU1_PLL0_PC1_M2DIV_SHIFT));
#endif
		break;
	case BCM4345_CHIP_ID:
		si_pmu_set_4345_pllcontrol_regs(sih, osh, cc, xtal);
		/* To ensure the PLL control registers are not modified from what is default. */
		xtal = 0;
		break;
	default:
		break;
	}

	/* Program the PLL based on the xtal value. */
	if (xtal != 0) {

		/* Find the frequency in the table */
		for (xt = si_pmu1_xtaltab0(sih); xt != NULL && xt->fref != 0; xt ++)
			if (xt->fref == xtal)
				break;

		/* Check current PLL state, bail out if it has been programmed or
		 * we don't know how to program it. But we might still have some programming
		 * like changing the ARM clock, etc. So cannot return from here.
		 */
		if (xt == NULL || xt->fref == 0)
			goto exit;

		/* If the PLL is already programmed exit from here. */
		if (((R_REG(osh, PMUREG(sih, pmucontrol)) &
			PCTL_XTALFREQ_MASK) >> PCTL_XTALFREQ_SHIFT) == xt->xf)
			goto exit;

		PMU_MSG(("XTAL %d.%d MHz (%d)\n", xtal / 1000, xtal % 1000, xt->xf));
		PMU_MSG(("Programming PLL for %d.%d MHz\n", xt->fref / 1000, xt->fref % 1000));

		/* Write p1div and p2div to pllcontrol[0] */
		tmp = ((xt->p1div << PMU1_PLL0_PC0_P1DIV_SHIFT) & PMU1_PLL0_PC0_P1DIV_MASK) |
			((xt->p2div << PMU1_PLL0_PC0_P2DIV_SHIFT) & PMU1_PLL0_PC0_P2DIV_MASK);
		si_pmu_pllcontrol(sih, PMU1_PLL0_PLLCTL0,
			(PMU1_PLL0_PC0_P1DIV_MASK | PMU1_PLL0_PC0_P2DIV_MASK), tmp);

		/* Write ndiv_int and ndiv_mode to pllcontrol[2] */
		tmp = ((xt->ndiv_int << PMU1_PLL0_PC2_NDIV_INT_SHIFT)
				& PMU1_PLL0_PC2_NDIV_INT_MASK) |
				((ndiv_mode << PMU1_PLL0_PC2_NDIV_MODE_SHIFT)
				& PMU1_PLL0_PC2_NDIV_MODE_MASK);
		si_pmu_pllcontrol(sih, PMU1_PLL0_PLLCTL2,
			(PMU1_PLL0_PC2_NDIV_INT_MASK | PMU1_PLL0_PC2_NDIV_MODE_MASK), tmp);

		/* Write ndiv_frac to pllcontrol[3] */
		tmp = ((xt->ndiv_frac << PMU1_PLL0_PC3_NDIV_FRAC_SHIFT) &
			PMU1_PLL0_PC3_NDIV_FRAC_MASK);
		si_pmu_pllcontrol(sih, PMU1_PLL0_PLLCTL3, PMU1_PLL0_PC3_NDIV_FRAC_MASK, tmp);

		/* Write clock driving strength to pllcontrol[5] */
		if (buf_strength) {
			PMU_MSG(("Adjusting PLL buffer drive strength: %x\n", buf_strength));

			tmp = (buf_strength << PMU1_PLL0_PC5_CLK_DRV_SHIFT);
			si_pmu_pllcontrol(sih, PMU1_PLL0_PLLCTL5, PMU1_PLL0_PC5_CLK_DRV_MASK, tmp);
		}

		/* Write XtalFreq. Set the divisor also. */
		tmp = R_REG(osh, PMUREG(sih, pmucontrol)) &
			~(PCTL_ILP_DIV_MASK | PCTL_XTALFREQ_MASK);
		tmp |= (((((xt->fref + 127) / 128) - 1) << PCTL_ILP_DIV_SHIFT) &
			PCTL_ILP_DIV_MASK) |
			((xt->xf << PCTL_XTALFREQ_SHIFT) & PCTL_XTALFREQ_MASK);
		W_REG(osh, PMUREG(sih, pmucontrol), tmp);
	}

exit:
	/* Return to original core */
	si_setcoreidx(sih, origidx);

	return write_en;
} /* si_pmu_update_pllcontrol */

uint32
si_pmu_get_pmutimer(si_t *sih, osl_t *osh, chipcregs_t *cc)
{
	uint32 start;
	start = R_REG(osh, PMUREG(sih, pmutimer));
	if (start != R_REG(osh, PMUREG(sih, pmutimer)))
		start = R_REG(osh, PMUREG(sih, pmutimer));
	return (start);
}

/* returns
 * a) diff between a 'prev' value of pmu timer and current value
 * b) the current pmutime value in 'prev'
 * 	So, 'prev' is an IO parameter.
 */
uint32
si_pmu_get_pmutime_diff(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 *prev)
{
	uint32 pmutime_diff = 0, pmutime_val = 0;
	uint32 prev_val = *prev;

	/* read current value */
	pmutime_val = si_pmu_get_pmutimer(sih, osh, cc);
	/* diff btween prev and current value, take on wraparound case as well. */
	pmutime_diff = (pmutime_val >= prev_val) ?
		(pmutime_val - prev_val) :
		(~prev_val + pmutime_val + 1);
	*prev = pmutime_val;
	return pmutime_diff;
}

/** wait for usec for the res_pending register to change. */
/*
	NOTE: usec SHOULD be > 32uS
	if cond = TRUE, res_pending will be read until it becomes == 0;
	If cond = FALSE, res_pending will be read until it becomes != 0;
	returns TRUE if timedout.
	returns elapsed time in this loop in elapsed_time
*/
bool
si_pmu_wait_for_res_pending(si_t *sih, osl_t *osh, chipcregs_t *cc, uint usec,
	bool cond, uint32 *elapsed_time)
{
	/* add 32uSec more */
	uint countdown = usec;
	uint32 pmutime_prev = 0, pmutime_elapsed = 0, res_pend;
	bool pending = FALSE;

	/* store current time */
	pmutime_prev = si_pmu_get_pmutimer(sih, osh, cc);
	while (1) {
		res_pend = R_REG(osh, PMUREG(sih, res_pending));

		/* based on the condition, check */
		if (cond == TRUE) {
			if (res_pend == 0) break;
		} else {
			if (res_pend != 0) break;
		}

		/* if required time over */
		if ((pmutime_elapsed * PMU_US_STEPS) >= countdown) {
			/* timeout. so return as still pending */
			pending = TRUE;
			break;
		}

		/* get elapsed time after adding diff between prev and current
		* pmutimer value
		*/
		pmutime_elapsed += si_pmu_get_pmutime_diff(sih, osh, cc, &pmutime_prev);
	}

	*elapsed_time = pmutime_elapsed * PMU_US_STEPS;
	return pending;
} /* si_pmu_wait_for_res_pending */

/**
 *	The algorithm for pending check is that,
 *	step1: 	wait till (res_pending !=0) OR pmu_max_trans_timeout.
 *			if max_trans_timeout, flag error and exit.
 *			wait for 1 ILP clk [64uS] based on pmu timer,
 *			polling to see if res_pending again goes high.
 *			if res_pending again goes high, go back to step1.
 *	Note: res_pending is checked repeatedly because, in between switching
 *	of dependent
 *	resources, res_pending resets to 0 for a short duration of time before
 *	it becomes 1 again.
 *	Note: return 0 is GOOD, 1 is BAD [mainly timeout].
 */
int si_pmu_wait_for_steady_state(si_t *sih, osl_t *osh, chipcregs_t *cc)
{
	int stat = 0;
	bool timedout = FALSE;
	uint32 elapsed = 0, pmutime_total_elapsed = 0;

	while (1) {
		/* wait until all resources are settled down [till res_pending becomes 0] */
		timedout = si_pmu_wait_for_res_pending(sih, osh, cc,
			PMU_MAX_TRANSITION_DLY, TRUE, &elapsed);

		if (timedout) {
			stat = 1;
			break;
		}

		pmutime_total_elapsed += elapsed;
		/* wait to check if any resource comes back to non-zero indicating
		* that it pends again. The res_pending goes 0 for 1 ILP clock before
		* getting set for next resource in the sequence , so if res_pending
		* is 0 for more than 1 ILP clk it means nothing is pending
		* to indicate some pending dependency.
		*/
		timedout = si_pmu_wait_for_res_pending(sih, osh, cc,
			64, FALSE, &elapsed);

		pmutime_total_elapsed += elapsed;
		/* Here, we can also check timedout, but we make sure that,
		* we read the res_pending again.
		*/
		if (timedout) {
			stat = 0;
			break;
		}

		/* Total wait time for all the waits above added should be
		* less than  PMU_MAX_TRANSITION_DLY
		*/
		if (pmutime_total_elapsed >= PMU_MAX_TRANSITION_DLY) {
			/* timeout. so return as still pending */
			stat = 1;
			break;
		}
	}
	return stat;
} /* si_pmu_wait_for_steady_state */

/** Turn Off the PLL - Required before setting the PLL registers */
static void
si_pmu_pll_off(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 *min_mask,
	uint32 *max_mask, uint32 *clk_ctl_st)
{
	uint32 ht_req;

	/* Save the original register values */
	*min_mask = R_REG(osh, PMUREG(sih, min_res_mask));
	*max_mask = R_REG(osh, PMUREG(sih, max_res_mask));
	*clk_ctl_st = R_REG(osh, &cc->clk_ctl_st);

	ht_req = si_pmu_htclk_mask(sih);
	if (ht_req == 0)
		return;

	if ((CHIPID(sih->chip) == BCM4335_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4345_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43462_CHIP_ID) ||
		BCM4350_CHIP(sih->chip) ||
	0) {
		/* slightly different way for 4335, but this could be applied for other chips also.
		* If HT_AVAIL is not set, wait to see if any resources are availing HT.
		*/
		if (((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL) != CCS_HTAVAIL))
			si_pmu_wait_for_steady_state(sih, osh, cc);
	} else {
		OR_REG(osh,  PMUREG(sih, max_res_mask), ht_req);
		/* wait for HT to be ready before taking the HT away...HT could be coming up... */
		SPINWAIT(((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL) != CCS_HTAVAIL),
			PMU_MAX_TRANSITION_DLY);
		ASSERT((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
	}

	AND_REG(osh, PMUREG(sih, min_res_mask), ~ht_req);
	AND_REG(osh, PMUREG(sih, max_res_mask), ~ht_req);

	SPINWAIT(((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL) == CCS_HTAVAIL),
		PMU_MAX_TRANSITION_DLY);
	ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
	OSL_DELAY(100);
} /* si_pmu_pll_off */

/* below function are for BBPLL parallel purpose */
/** Turn Off the PLL - Required before setting the PLL registers */
void
si_pmu_pll_off_PARR(si_t *sih, osl_t *osh, uint32 *min_mask,
uint32 *max_mask, uint32 *clk_ctl_st)
{
	chipcregs_t *cc;
	uint origidx, intr_val;
	uint32 ht_req;

	/* Remember original core before switch to chipc */
	cc = (chipcregs_t *)si_switch_core(sih, CC_CORE_ID, &origidx, &intr_val);
	ASSERT(cc != NULL);

	/* Save the original register values */
	*min_mask = R_REG(osh, PMUREG(sih, min_res_mask));
	*max_mask = R_REG(osh, PMUREG(sih, max_res_mask));
	*clk_ctl_st = R_REG(osh, &cc->clk_ctl_st);
	ht_req = si_pmu_htclk_mask(sih);
	if (ht_req == 0)
		return;

	if ((CHIPID(sih->chip) == BCM4335_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4345_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43462_CHIP_ID) ||
		BCM4350_CHIP(sih->chip) ||
	0) {
		/* slightly different way for 4335, but this could be applied for other chips also.
		* If HT_AVAIL is not set, wait to see if any resources are availing HT.
		*/
		if (((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL) != CCS_HTAVAIL))
			si_pmu_wait_for_steady_state(sih, osh, cc);
	} else {
		OR_REG(osh,  PMUREG(sih, max_res_mask), ht_req);
		/* wait for HT to be ready before taking the HT away...HT could be coming up... */
		SPINWAIT(((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL) != CCS_HTAVAIL),
			PMU_MAX_TRANSITION_DLY);
		ASSERT((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
	}

	AND_REG(osh, PMUREG(sih, min_res_mask), ~ht_req);
	AND_REG(osh, PMUREG(sih, max_res_mask), ~ht_req);

	/* Return to original core */
	si_restore_core(sih, origidx, intr_val);
} /* si_pmu_pll_off_PARR */


static void
si_pmu_pll_off_isdone(si_t *sih, osl_t *osh, chipcregs_t *cc)
{
	uint32 ht_req;
	ht_req = si_pmu_htclk_mask(sih);
	SPINWAIT(((R_REG(osh, PMUREG(sih, res_state)) & ht_req) != 0),
	PMU_MAX_TRANSITION_DLY);
	SPINWAIT(((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL) == CCS_HTAVAIL),
		PMU_MAX_TRANSITION_DLY);
	ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));

}

/* above function are for BBPLL parallel purpose */

/** Turn ON/restore the PLL based on the mask received */
static void
si_pmu_pll_on(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 min_mask_mask,
	uint32 max_mask_mask, uint32 clk_ctl_st_mask)
{
	uint32 ht_req;

	ht_req = si_pmu_htclk_mask(sih);
	if (ht_req == 0)
		return;

	max_mask_mask &= ht_req;
	min_mask_mask &= ht_req;

	if (max_mask_mask != 0)
		OR_REG(osh, PMUREG(sih, max_res_mask), max_mask_mask);

	if (min_mask_mask != 0)
		OR_REG(osh, PMUREG(sih, min_res_mask), min_mask_mask);

	if (clk_ctl_st_mask & CCS_HTAVAIL) {
		/* Wait for HT_AVAIL to come back */
		SPINWAIT(((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL) != CCS_HTAVAIL),
			PMU_MAX_TRANSITION_DLY);
		ASSERT((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
	}
}

/**
 * Caller wants to know the default values to program into BB PLL/FLL hardware for a specific chip.
 *
 * The relation between x-tal frequency, HT clock and divisor values to write into the PLL hardware
 * is given by a set of tables, one table per PLL/FLL frequency (480/485 Mhz).
 *
 * This function returns the table entry corresponding with the (chip specific) default x-tal
 * frequency.
 */
static const pmu2_xtaltab0_t *
BCMINITFN(si_pmu2_xtaldef0)(si_t *sih)
{
	switch (CHIPID(sih->chip)) {
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
	case BCM43143_CHIP_ID:
#ifdef BCM_BOOTLOADER
		return &pmu2_xtaltab0_adfll_480[PMU15_XTALTAB0_20000K];
#else
		if (ISSIM_ENAB(sih))
			return &pmu2_xtaltab0_adfll_480[PMU15_XTALTAB0_20000K];
		else
			return &pmu2_xtaltab0_adfll_485[PMU15_XTALTAB0_20000K];
#endif
	case BCM43341_CHIP_ID:
	case BCM4334_CHIP_ID:
#ifdef BCM_BOOTLOADER
		return &pmu2_xtaltab0_adfll_480[PMU15_XTALTAB0_37400K];
#else
		if (ISSIM_ENAB(sih))
			return &pmu2_xtaltab0_adfll_480[PMU15_XTALTAB0_37400K];
		else
			return &pmu2_xtaltab0_adfll_485[PMU15_XTALTAB0_37400K];
#endif
	default:
		break;
	}

	ASSERT(0);
	return NULL;
}

static const pmu2_xtaltab0_t *
BCMINITFN(si_pmu2_xtaltab0)(si_t *sih)
{
	switch (CHIPID(sih->chip)) {
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
	case BCM43143_CHIP_ID:
	case BCM43341_CHIP_ID:
	case BCM4334_CHIP_ID:
#ifdef BCM_BOOTLOADER
		return pmu2_xtaltab0_adfll_480;
#else
		if (ISSIM_ENAB(sih))
			return pmu2_xtaltab0_adfll_480;
		else
			return pmu2_xtaltab0_adfll_485;
#endif
	default:
		break;
	}

	ASSERT(0);
	return NULL;
}

/** For hardware workarounds, OTP can contain an entry to update FLL control registers */
static void
BCMATTACHFN(si_pmu2_pll_vars_init)(si_t *sih, osl_t *osh, chipcregs_t *cc)
{
	char name[16];
	const char *otp_val;
	uint8 i, otp_entry_found = FALSE;
	uint32 pll_ctrlcnt;
	uint32 min_mask = 0, max_mask = 0, clk_ctl_st = 0;

	if (sih->pmurev >= 5) {
		pll_ctrlcnt = (sih->pmucaps & PCAP5_PC_MASK) >> PCAP5_PC_SHIFT;
	}
	else {
		pll_ctrlcnt = (sih->pmucaps & PCAP_PC_MASK) >> PCAP_PC_SHIFT;
	}

	/* Check if there is any otp enter for PLLcontrol registers */
	for (i = 0; i < pll_ctrlcnt; i++) {
		snprintf(name, sizeof(name), rstr_pllD, i);
		if ((otp_val = getvar(NULL, name)) == NULL)
			continue;

		/* If OTP entry is found for PLL register, then turn off the PLL
		 * and set the status of the OTP entry accordingly.
		 */
		otp_entry_found = TRUE;
		break;
	}

	/* If no OTP parameter is found, return. */
	if (otp_entry_found == FALSE)
		return;

	/* Make sure PLL is off */
	si_pmu_pll_off(sih, osh, cc, &min_mask, &max_mask, &clk_ctl_st);

	/* Update the PLL register if there is a OTP entry for PLL registers */
	si_pmu_otp_pllcontrol(sih, osh);

	/* Flush deferred pll control registers writes */
	if (sih->pmurev >= 2)
		OR_REG(osh, PMUREG(sih, pmucontrol), PCTL_PLL_PLLCTL_UPD);

	/* Restore back the register values. This ensures PLL remains on if it
	 * was originally on and remains off if it was originally off.
	 */
	si_pmu_pll_on(sih, osh, cc, min_mask, max_mask, clk_ctl_st);
} /* si_pmu2_pll_vars_init */

/**
 * PLL needs to be initialized to the correct frequency. This function will skip that
 * initialization if the PLL is already at the correct frequency.
 */
static void
BCMATTACHFN(si_pmu2_pllinit0)(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal)
{
	const pmu2_xtaltab0_t *xt;
	int xt_idx;
	uint32 freq_tgt, pll0;
	rsc_per_chip_t *rsc;

	if (xtal == 0) {
		PMU_MSG(("Unspecified xtal frequency, skip PLL configuration\n"));
		goto exit;
	}

	for (xt = si_pmu2_xtaltab0(sih), xt_idx = 0; xt != NULL && xt->fref != 0; xt++, xt_idx++) {
		if (xt->fref == xtal)
			break;
	}

	if (xt == NULL || xt->fref == 0) {
		PMU_MSG(("Unsupported xtal frequency %d.%d MHz, skip PLL configuration\n",
		         xtal / 1000, xtal % 1000));
		goto exit;
	}

	W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU15_PLL_PLLCTL0);
	pll0 = R_REG(osh, PMUREG(sih, pllcontrol_data));

	freq_tgt = (pll0 & PMU15_PLL_PC0_FREQTGT_MASK) >> PMU15_PLL_PC0_FREQTGT_SHIFT;
	if (freq_tgt == xt->freq_tgt) {
		PMU_MSG(("PLL already programmed for %d.%d MHz\n",
			xt->fref / 1000, xt->fref % 1000));
		goto exit;
	}

	PMU_MSG(("XTAL %d.%d MHz (%d)\n", xtal / 1000, xtal % 1000, xt->xf));
	PMU_MSG(("Programming PLL for %d.%d MHz\n", xt->fref / 1000, xt->fref % 1000));

	/* Make sure the PLL is off */
	switch (CHIPID(sih->chip)) {
	case BCM43341_CHIP_ID:
	case BCM4334_CHIP_ID:
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
	case BCM43143_CHIP_ID:
		rsc = si_pmu_get_rsc_positions(sih);
		AND_REG(osh, PMUREG(sih, min_res_mask),
			~(PMURES_BIT(rsc->ht_avail) | PMURES_BIT(rsc->macphy_clkavail)));
		AND_REG(osh, PMUREG(sih, max_res_mask),
			~(PMURES_BIT(rsc->ht_avail) | PMURES_BIT(rsc->macphy_clkavail)));
		SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL, PMU_MAX_TRANSITION_DLY);
		ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
		break;
	default:
		PMU_ERROR(("%s: Turn HT off for 0x%x????\n", __FUNCTION__, CHIPID(sih->chip)));
		break;
	}

	pll0 = (pll0 & ~PMU15_PLL_PC0_FREQTGT_MASK) | (xt->freq_tgt << PMU15_PLL_PC0_FREQTGT_SHIFT);
	W_REG(osh, PMUREG(sih, pllcontrol_data), pll0);

	if (CST4334_CHIPMODE_HSIC(sih->chipst)) {
		uint32 hsic_freq;

		/* Only update target freq from 480Mhz table for HSIC */
		ASSERT(xt_idx < PMU15_XTALTAB0_END);
		hsic_freq = pmu2_xtaltab0_adfll_480[xt_idx].freq_tgt;

		/*
		 * Update new tgt freq for PLL control 5
		 * This is activated when USB/HSIC core is taken out of reset (ch_reset())
		 */
		si_pmu_pllcontrol(sih, PMU15_PLL_PLLCTL5, PMU15_PLL_PC5_FREQTGT_MASK, hsic_freq);
	}

	/* Flush deferred pll control registers writes */
	if (sih->pmurev >= 2)
		OR_REG(osh, PMUREG(sih, pmucontrol), PCTL_PLL_PLLCTL_UPD);

exit:
	/* Vars over-rides */
	si_pmu2_pll_vars_init(sih, osh, cc);
} /* si_pmu2_pllinit0 */

/** returns the clock frequency at which the ARM is running */
static uint32
BCMINITFN(si_pmu2_cpuclk0)(si_t *sih, osl_t *osh, chipcregs_t *cc)
{
	const pmu2_xtaltab0_t *xt;
	uint32 freq_tgt = 0, pll0 = 0;

	switch (CHIPID(sih->chip)) {
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
	case BCM43143_CHIP_ID:
	case BCM43341_CHIP_ID:
	case BCM4334_CHIP_ID:
		W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU15_PLL_PLLCTL0);
		pll0 = R_REG(osh, PMUREG(sih, pllcontrol_data));
		freq_tgt = (pll0 & PMU15_PLL_PC0_FREQTGT_MASK) >> PMU15_PLL_PC0_FREQTGT_SHIFT;
		break;
	default:
		ASSERT(0);
		break;
	}

	for (xt = pmu2_xtaltab0_adfll_480; xt != NULL && xt->fref != 0; xt++) {
		if (xt->freq_tgt == freq_tgt)
			return PMU15_ARM_96MHZ;
	}

#ifndef BCM_BOOTLOADER
	for (xt = pmu2_xtaltab0_adfll_485; xt != NULL && xt->fref != 0; xt++) {
		if (xt->freq_tgt == freq_tgt)
			return PMU15_ARM_97MHZ;
	}
#endif

	/* default */
	return PMU15_ARM_96MHZ;
}

/** Query ALP/xtal clock frequency */
static uint32
BCMINITFN(si_pmu2_alpclk0)(si_t *sih, osl_t *osh, chipcregs_t *cc)
{
	const pmu2_xtaltab0_t *xt;
	uint32 freq_tgt, pll0;

	W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU15_PLL_PLLCTL0);
	pll0 = R_REG(osh, PMUREG(sih, pllcontrol_data));

	freq_tgt = (pll0 & PMU15_PLL_PC0_FREQTGT_MASK) >> PMU15_PLL_PC0_FREQTGT_SHIFT;


	for (xt = pmu2_xtaltab0_adfll_480; xt != NULL && xt->fref != 0; xt++) {
		if (xt->freq_tgt == freq_tgt)
			break;
	}

#ifndef BCM_BOOTLOADER
	if (xt == NULL || xt->fref == 0) {
		for (xt = pmu2_xtaltab0_adfll_485; xt != NULL && xt->fref != 0; xt++) {
			if (xt->freq_tgt == freq_tgt)
				break;
		}
	}
#endif

	/* Could not find it so assign a default value */
	if (xt == NULL || xt->fref == 0)
		xt = si_pmu2_xtaldef0(sih);
	ASSERT(xt != NULL && xt->fref != 0);

	return xt->fref * 1000;
}

/**
 * Set up PLL registers in the PMU as per the (optional) OTP values, or, if no OTP values are
 * present, optionally update with POR override values contained in firmware. Enables the BBPLL
 * when done.
 */
static void
BCMATTACHFN(si_pmu1_pllinit1)(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal)
{
	char name[16];
	const char *otp_val;
	uint8 i, otp_entry_found = FALSE;
	uint32 pll_ctrlcnt;
	uint32 min_mask = 0, max_mask = 0, clk_ctl_st = 0;

	if (sih->pmurev >= 5) {
		pll_ctrlcnt = (sih->pmucaps & PCAP5_PC_MASK) >> PCAP5_PC_SHIFT;
	}
	else {
		pll_ctrlcnt = (sih->pmucaps & PCAP_PC_MASK) >> PCAP_PC_SHIFT;
	}

	/* Check if there is any otp enter for PLLcontrol registers */
	for (i = 0; i < pll_ctrlcnt; i++) {
		snprintf(name, sizeof(name), rstr_pllD, i);
		if ((otp_val = getvar(NULL, name)) == NULL)
			continue;

		/* If OTP entry is found for PLL register, then turn off the PLL
		 * and set the status of the OTP entry accordingly.
		 */
		otp_entry_found = TRUE;
		break;
	}

	/* If no OTP parameter is found and no chip-specific updates are needed, return. */
	if ((otp_entry_found == FALSE) &&
		(si_pmu_update_pllcontrol(sih, osh, xtal, FALSE) == FALSE))
		return;

	/* Make sure PLL is off */
	si_pmu_pll_off(sih, osh, cc, &min_mask, &max_mask, &clk_ctl_st);

	/* Update any chip-specific PLL registers. Does not write PLL 'update' bit yet. */
	si_pmu_update_pllcontrol(sih, osh, xtal, TRUE);

	/* Update the PLL register if there is a OTP entry for PLL registers */
	si_pmu_otp_pllcontrol(sih, osh);

	/* Flush ('update') the deferred pll control registers writes */
	if (sih->pmurev >= 2)
		OR_REG(osh, PMUREG(sih, pmucontrol), PCTL_PLL_PLLCTL_UPD);

	/* Restore back the register values. This ensures PLL remains on if it
	 * was originally on and remains off if it was originally off.
	 */
	si_pmu_pll_on(sih, osh, cc, min_mask, max_mask, clk_ctl_st);
} /* si_pmu1_pllinit1 */

/**
 * Set up PLL registers in the PMU as per the crystal speed.
 * XtalFreq field in pmucontrol register being 0 indicates the PLL
 * is not programmed and the h/w default is assumed to work, in which
 * case the xtal frequency is unknown to the s/w so we need to call
 * si_pmu1_xtaldef0() wherever it is needed to return a default value.
 */
static void
BCMATTACHFN(si_pmu1_pllinit0)(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal)
{
	const pmu1_xtaltab0_t *xt;
	uint32 tmp;
	uint32 buf_strength = 0;
	uint8 ndiv_mode = 1;
	uint8 dacrate;

	/* Use h/w default PLL config */
	if (xtal == 0) {
		PMU_MSG(("Unspecified xtal frequency, skip PLL configuration\n"));
		return;
	}

	/* Find the frequency in the table */
	for (xt = si_pmu1_xtaltab0(sih); xt != NULL && xt->fref != 0; xt ++)
		if (xt->fref == xtal)
			break;

	/* Check current PLL state, bail out if it has been programmed or
	 * we don't know how to program it.
	 */
	if (xt == NULL || xt->fref == 0) {
		PMU_MSG(("Unsupported xtal frequency %d.%d MHz, skip PLL configuration\n",
		         xtal / 1000, xtal % 1000));
		return;
	}
	/*  for 4319 bootloader already programs the PLL but bootloader does not program the
	    PLL4 and PLL5. So Skip this check for 4319
	*/
	if ((((R_REG(osh, PMUREG(sih, pmucontrol)) & PCTL_XTALFREQ_MASK) >>
		PCTL_XTALFREQ_SHIFT) == xt->xf) &&
		!((CHIPID(sih->chip) == BCM4319_CHIP_ID) || (CHIPID(sih->chip) == BCM4330_CHIP_ID)))
	{
		PMU_MSG(("PLL already programmed for %d.%d MHz\n",
			xt->fref / 1000, xt->fref % 1000));
		return;
	}

	PMU_MSG(("XTAL %d.%d MHz (%d)\n", xtal / 1000, xtal % 1000, xt->xf));
	PMU_MSG(("Programming PLL for %d.%d MHz\n", xt->fref / 1000, xt->fref % 1000));

	switch (CHIPID(sih->chip)) {
	case BCM4325_CHIP_ID:
		/* Change the BBPLL drive strength to 2 for all channels */
		buf_strength = 0x222222;
		/* Make sure the PLL is off */
		AND_REG(osh, PMUREG(sih, min_res_mask),
		        ~(PMURES_BIT(RES4325_BBPLL_PWRSW_PU) | PMURES_BIT(RES4325_HT_AVAIL)));
		AND_REG(osh, PMUREG(sih, max_res_mask),
		        ~(PMURES_BIT(RES4325_BBPLL_PWRSW_PU) | PMURES_BIT(RES4325_HT_AVAIL)));
		SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL, PMU_MAX_TRANSITION_DLY);
		ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
		break;
	case BCM4329_CHIP_ID:
		/* Change the BBPLL drive strength to 8 for all channels */
		buf_strength = 0x888888;
		AND_REG(osh, PMUREG(sih, min_res_mask),
		        ~(PMURES_BIT(RES4329_BBPLL_PWRSW_PU) | PMURES_BIT(RES4329_HT_AVAIL)));
		AND_REG(osh, PMUREG(sih, max_res_mask),
		        ~(PMURES_BIT(RES4329_BBPLL_PWRSW_PU) | PMURES_BIT(RES4329_HT_AVAIL)));
		SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL, PMU_MAX_TRANSITION_DLY);
		ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
		W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL4);
		if (xt->fref == 38400)
			tmp = 0x200024C0;
		else if (xt->fref == 37400)
			tmp = 0x20004500;
		else if (xt->fref == 26000)
			tmp = 0x200024C0;
		else
			tmp = 0x200005C0; /* Chip Dflt Settings */
		W_REG(osh, PMUREG(sih, pllcontrol_data), tmp);
		W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL5);
		tmp = R_REG(osh, PMUREG(sih, pllcontrol_data)) & PMU1_PLL0_PC5_CLK_DRV_MASK;
		if ((xt->fref == 38400) || (xt->fref == 37400) || (xt->fref == 26000))
			tmp |= 0x15;
		else
			tmp |= 0x25; /* Chip Dflt Settings */
		W_REG(osh, PMUREG(sih, pllcontrol_data), tmp);
		break;
	case BCM4315_CHIP_ID:
		/* Change the BBPLL drive strength to 2 for all channels */
		buf_strength = 0x222222;
		/* Make sure the PLL is off */
		AND_REG(osh, PMUREG(sih, min_res_mask), ~(PMURES_BIT(RES4315_HT_AVAIL)));
		AND_REG(osh, PMUREG(sih, max_res_mask), ~(PMURES_BIT(RES4315_HT_AVAIL)));
		OSL_DELAY(100);

		AND_REG(osh, PMUREG(sih, min_res_mask), ~(PMURES_BIT(RES4315_BBPLL_PWRSW_PU)));
		AND_REG(osh, PMUREG(sih, max_res_mask), ~(PMURES_BIT(RES4315_BBPLL_PWRSW_PU)));
		OSL_DELAY(100);

		SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL, PMU_MAX_TRANSITION_DLY);
		ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
		break;

	case BCM4319_CHIP_ID:
		/* Change the BBPLL drive strength to 2 for all channels */
		buf_strength = 0x222222;

		/* Make sure the PLL is off */
		/* WAR65104: Disable the HT_AVAIL resource first and then
		 * after a delay (more than downtime for HT_AVAIL) remove the
		 * BBPLL resource; backplane clock moves to ALP from HT.
		 */
		AND_REG(osh, PMUREG(sih, min_res_mask), ~(PMURES_BIT(RES4319_HT_AVAIL)));
		AND_REG(osh, PMUREG(sih, max_res_mask), ~(PMURES_BIT(RES4319_HT_AVAIL)));

		OSL_DELAY(100);
		AND_REG(osh, PMUREG(sih, min_res_mask), ~(PMURES_BIT(RES4319_BBPLL_PWRSW_PU)));
		AND_REG(osh, PMUREG(sih, max_res_mask), ~(PMURES_BIT(RES4319_BBPLL_PWRSW_PU)));

		OSL_DELAY(100);
		SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL, PMU_MAX_TRANSITION_DLY);
		ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
		W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL4);
		tmp = 0x200005c0;
		W_REG(osh, PMUREG(sih, pllcontrol_data), tmp);
		break;

	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
		AND_REG(osh, PMUREG(sih, min_res_mask),
			~(PMURES_BIT(RES4336_HT_AVAIL) | PMURES_BIT(RES4336_MACPHY_CLKAVAIL)));
		AND_REG(osh, PMUREG(sih, max_res_mask),
			~(PMURES_BIT(RES4336_HT_AVAIL) | PMURES_BIT(RES4336_MACPHY_CLKAVAIL)));
		OSL_DELAY(100);
		SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL, PMU_MAX_TRANSITION_DLY);
		ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
		break;

	case BCM4330_CHIP_ID:
		AND_REG(osh, PMUREG(sih, min_res_mask),
			~(PMURES_BIT(RES4330_HT_AVAIL) | PMURES_BIT(RES4330_MACPHY_CLKAVAIL)));
		AND_REG(osh, PMUREG(sih, max_res_mask),
			~(PMURES_BIT(RES4330_HT_AVAIL) | PMURES_BIT(RES4330_MACPHY_CLKAVAIL)));
		OSL_DELAY(100);
		SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL, PMU_MAX_TRANSITION_DLY);
		ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
		break;

	default:
		ASSERT(0);
	}

	PMU_MSG(("Done masking\n"));

	/* Write p1div and p2div to pllcontrol[0] */
	W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL0);
	tmp = R_REG(osh, PMUREG(sih, pllcontrol_data)) &
	        ~(PMU1_PLL0_PC0_P1DIV_MASK | PMU1_PLL0_PC0_P2DIV_MASK);
	tmp |= ((xt->p1div << PMU1_PLL0_PC0_P1DIV_SHIFT) & PMU1_PLL0_PC0_P1DIV_MASK) |
	        ((xt->p2div << PMU1_PLL0_PC0_P2DIV_SHIFT) & PMU1_PLL0_PC0_P2DIV_MASK);
	W_REG(osh, PMUREG(sih, pllcontrol_data), tmp);

	if ((CHIPID(sih->chip) == BCM4330_CHIP_ID)) {
		if (CHIPREV(sih->chiprev) < 2)
			dacrate = 160;
		else {
			if (!(dacrate = (uint8)getintvar(NULL, rstr_dacrate2g)))
				dacrate = 80;
		}
		si_pmu_set_4330_plldivs(sih, dacrate);
	}

	if ((CHIPID(sih->chip) == BCM4329_CHIP_ID) && (CHIPREV(sih->chiprev) == 0)) {

		W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL1);
		tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
		tmp = tmp & (~DOT11MAC_880MHZ_CLK_DIVISOR_MASK);
		tmp = tmp | DOT11MAC_880MHZ_CLK_DIVISOR_VAL;
		W_REG(osh, PMUREG(sih, pllcontrol_data), tmp);
	}
	if ((CHIPID(sih->chip) == BCM4319_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4336_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43362_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4330_CHIP_ID))
		ndiv_mode = PMU1_PLL0_PC2_NDIV_MODE_MFB;
	else
		ndiv_mode = PMU1_PLL0_PC2_NDIV_MODE_MASH;

	/* Write ndiv_int and ndiv_mode to pllcontrol[2] */
	W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL2);
	tmp = R_REG(osh, PMUREG(sih, pllcontrol_data)) &
	        ~(PMU1_PLL0_PC2_NDIV_INT_MASK | PMU1_PLL0_PC2_NDIV_MODE_MASK);
	tmp |= ((xt->ndiv_int << PMU1_PLL0_PC2_NDIV_INT_SHIFT) & PMU1_PLL0_PC2_NDIV_INT_MASK) |
	        ((ndiv_mode << PMU1_PLL0_PC2_NDIV_MODE_SHIFT) & PMU1_PLL0_PC2_NDIV_MODE_MASK);
	W_REG(osh, PMUREG(sih, pllcontrol_data), tmp);

	/* Write ndiv_frac to pllcontrol[3] */
	W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL3);
	tmp = R_REG(osh, PMUREG(sih, pllcontrol_data)) & ~PMU1_PLL0_PC3_NDIV_FRAC_MASK;
	tmp |= ((xt->ndiv_frac << PMU1_PLL0_PC3_NDIV_FRAC_SHIFT) &
	        PMU1_PLL0_PC3_NDIV_FRAC_MASK);
	W_REG(osh, PMUREG(sih, pllcontrol_data), tmp);

	/* Write clock driving strength to pllcontrol[5] */
	if (buf_strength) {
		PMU_MSG(("Adjusting PLL buffer drive strength: %x\n", buf_strength));

		W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL5);
		tmp = R_REG(osh, PMUREG(sih, pllcontrol_data)) & ~PMU1_PLL0_PC5_CLK_DRV_MASK;
		tmp |= (buf_strength << PMU1_PLL0_PC5_CLK_DRV_SHIFT);
		W_REG(osh, PMUREG(sih, pllcontrol_data), tmp);
	}

	PMU_MSG(("Done pll\n"));

	/* to operate the 4319 usb in 24MHz/48MHz; chipcontrol[2][84:83] needs
	 * to be updated.
	 */
	if ((CHIPID(sih->chip) == BCM4319_CHIP_ID) && (xt->fref != XTAL_FREQ_30000MHZ)) {
		W_REG(osh, PMUREG(sih, chipcontrol_addr), PMU1_PLL0_CHIPCTL2);
		tmp = R_REG(osh, PMUREG(sih, chipcontrol_data)) & ~CCTL_4319USB_XTAL_SEL_MASK;
		if (xt->fref == XTAL_FREQ_24000MHZ) {
			tmp |= (CCTL_4319USB_24MHZ_PLL_SEL << CCTL_4319USB_XTAL_SEL_SHIFT);
		} else if (xt->fref == XTAL_FREQ_48000MHZ) {
			tmp |= (CCTL_4319USB_48MHZ_PLL_SEL << CCTL_4319USB_XTAL_SEL_SHIFT);
		}
		W_REG(osh, PMUREG(sih, chipcontrol_data), tmp);
	}

	/* Flush deferred pll control registers writes */
	if (sih->pmurev >= 2)
		OR_REG(osh, PMUREG(sih, pmucontrol), PCTL_PLL_PLLCTL_UPD);

	/* Write XtalFreq. Set the divisor also. */
	tmp = R_REG(osh, PMUREG(sih, pmucontrol)) &
	        ~(PCTL_ILP_DIV_MASK | PCTL_XTALFREQ_MASK);
	tmp |= (((((xt->fref + 127) / 128) - 1) << PCTL_ILP_DIV_SHIFT) &
	        PCTL_ILP_DIV_MASK) |
	       ((xt->xf << PCTL_XTALFREQ_SHIFT) & PCTL_XTALFREQ_MASK);

	if ((CHIPID(sih->chip) == BCM4329_CHIP_ID) && CHIPREV(sih->chiprev) == 0) {
		/* clear the htstretch before clearing HTReqEn */
		AND_REG(osh, PMUREG(sih, clkstretch), ~CSTRETCH_HT);
		tmp &= ~PCTL_HT_REQ_EN;
	}

	W_REG(osh, PMUREG(sih, pmucontrol), tmp);
} /* si_pmu1_pllinit0 */

/**
 * returns the CPU clock frequency. Does this by determining current Fvco and the setting of the
 * clock divider that leads up to the ARM. Returns value in [Hz] units.
 */
static uint32
BCMINITFN(si_pmu1_cpuclk0)(si_t *sih, osl_t *osh, chipcregs_t *cc)
{
	uint32 tmp, mdiv = 1;
#ifndef BCM_OL_DEV
#ifdef BCMDBG
	uint32 ndiv_int, ndiv_frac, p2div, p1div, fvco;
	uint32 fref;
#endif
#endif /* BCM_OL_DEV */
#ifdef BCMDBG
	char chn[8];
#endif
	uint32 FVCO = si_pmu1_pllfvco0(sih);	/* in [khz] units */

	if ((CHIPID(sih->chip) == BCM43602_CHIP_ID ||
	    CHIPID(sih->chip) == BCM43462_CHIP_ID) &&
#ifdef DONGLEBUILD
		(si_arm_clockratio(sih, 0) == 1) &&
#endif
		TRUE) {
		/* CR4 running on backplane_clk */
		return si_pmu_si_clock(sih, osh);	/* in [hz] units */
	}
	switch (CHIPID(sih->chip)) {
	case BCM4325_CHIP_ID:
	case BCM4329_CHIP_ID:
	case BCM4315_CHIP_ID:
	case BCM4319_CHIP_ID:
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
	case BCM4330_CHIP_ID:
		/* Read m1div from pllcontrol[1] */
		W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL1);
		tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
		mdiv = (tmp & PMU1_PLL0_PC1_M1DIV_MASK) >> PMU1_PLL0_PC1_M1DIV_SHIFT;
		break;
	case BCM43239_CHIP_ID:
		/* Read m6div from pllcontrol[2] */
		W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL2);
		tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
		mdiv = (tmp & PMU1_PLL0_PC2_M6DIV_MASK) >> PMU1_PLL0_PC2_M6DIV_SHIFT;
		break;
	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
		/* Read m2div from pllcontrol[1] */
		W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL1);
		tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
		mdiv = (tmp & PMU1_PLL0_PC1_M2DIV_MASK) >> PMU1_PLL0_PC1_M2DIV_SHIFT;
		break;
	case BCM4335_CHIP_ID:
	case BCM4345_CHIP_ID:
		/* Read m3div from pllcontrol[1] */
		W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL1);
		tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
		mdiv = (tmp & PMU1_PLL0_PC1_M3DIV_MASK) >> PMU1_PLL0_PC1_M3DIV_SHIFT;
		break;
	case BCM4350_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
		if (CHIPREV(sih->chiprev) >= 3) {
			W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL2);
			tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
			mdiv = (tmp & PMU1_PLL0_PC2_M5DIV_MASK) >> PMU1_PLL0_PC2_M5DIV_SHIFT;
		}
		else {
			/* Read m3div from pllcontrol[1] */
			W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL1);
			tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
			mdiv = (tmp & PMU1_PLL0_PC1_M3DIV_MASK) >> PMU1_PLL0_PC1_M3DIV_SHIFT;
		}
		break;
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
		tmp = R_REG(osh, &cc->pllcontrol_data);
		mdiv = (tmp & PMU1_PLL0_PC2_M5DIV_MASK) >> PMU1_PLL0_PC2_M5DIV_SHIFT;
		break;
	case BCM4360_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM43526_CHIP_ID:
	case BCM4352_CHIP_ID:
		/* Read m6div from pllcontrol[5] */
		W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL5);
		tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
		mdiv = (tmp & PMU1_PLL0_PC2_M6DIV_MASK) >> PMU1_PLL0_PC2_M6DIV_SHIFT;
		break;
#ifdef DONGLEBUILD
	case BCM43602_CHIP_ID:
	case BCM43462_CHIP_ID:
		ASSERT(si_arm_clockratio(sih, 0) == 2);
		/* CR4 running on armcr4_clk (Ch5). Read 'bbpll_i_m5div' from pllctl[5] */
		W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL5);
		tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
		mdiv = (tmp & PMU1_PLL0_PC2_M5DIV_MASK) >> PMU1_PLL0_PC2_M5DIV_SHIFT;
		break;
#endif /* DONGLEBUILD */
	default:
		PMU_MSG(("si_pmu1_cpuclk0: Unknown chipid %s\n", bcm_chipname(sih->chip, chn, 8)));
		ASSERT(0);
		break;
	}
#ifndef BCM_OL_DEV
#ifdef BCMDBG
	/* Read p2div/p1div from pllcontrol[0] */
	W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL0);
	tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
	p2div = (tmp & PMU1_PLL0_PC0_P2DIV_MASK) >> PMU1_PLL0_PC0_P2DIV_SHIFT;
	p1div = (tmp & PMU1_PLL0_PC0_P1DIV_MASK) >> PMU1_PLL0_PC0_P1DIV_SHIFT;

	/* Calculate fvco based on xtal freq and ndiv and pdiv */
	W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL2);
	tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
	if (CHIPID(sih->chip) == BCM4345_CHIP_ID ||
		BCM4350_CHIP(sih->chip) ||
	    CHIPID(sih->chip) == BCM4335_CHIP_ID) {
		p2div = 1;
		p1div = (tmp & PMU4335_PLL0_PC2_P1DIV_MASK) >> PMU4335_PLL0_PC2_P1DIV_SHIFT;
		ndiv_int = (tmp & PMU4335_PLL0_PC2_NDIV_INT_MASK) >>
		           PMU4335_PLL0_PC2_NDIV_INT_SHIFT;
	} else {
		ndiv_int = (tmp & PMU1_PLL0_PC2_NDIV_INT_MASK) >> PMU1_PLL0_PC2_NDIV_INT_SHIFT;
	}

	W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL3);
	tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
	ndiv_frac = (tmp & PMU1_PLL0_PC3_NDIV_FRAC_MASK) >> PMU1_PLL0_PC3_NDIV_FRAC_SHIFT;

	fref = si_pmu1_alpclk0(sih, osh, cc) / 1000;

	fvco = (fref * ndiv_int) << 8;
	fvco += (fref * (ndiv_frac >> 12)) >> 4;
	fvco += (fref * (ndiv_frac & 0xfff)) >> 12;
	fvco >>= 8;
	fvco *= p2div;
	fvco /= p1div;
	fvco /= 1000;
	fvco *= 1000;

	PMU_MSG(("si_pmu1_cpuclk0: ndiv_int %u ndiv_frac %u p2div %u p1div %u fvco %u\n",
	         ndiv_int, ndiv_frac, p2div, p1div, fvco));

	FVCO = fvco;
#endif	/* BCMDBG */
#endif /* BCM_OL_DEV */
	/* Return ARM/SB clock */
	return FVCO / mdiv * 1000;
} /* si_pmu1_cpuclk0 */


/** 4335 specific code. Returns the MAC clock frequency. */
extern uint32
si_mac_clk(si_t *sih, osl_t *osh)
{
	uint8 mdiv2 = 0;
	uint32 pll_reg, mac_clk = 0;
	chipcregs_t *cc;
	uint origidx, intr_val;
#ifdef BCMDBG
	char chn[8];
#endif

	uint32 FVCO = si_pmu1_pllfvco0(sih);	/* in [khz] units */

	/* Remember original core before switch to chipc */
	cc = (chipcregs_t *)si_switch_core(sih, CC_CORE_ID, &origidx, &intr_val);
	ASSERT(cc != NULL);
	BCM_REFERENCE(cc);

	switch (CHIPID(sih->chip)) {
	case BCM4335_CHIP_ID:
		pll_reg = si_pmu_pllcontrol(sih, PMU1_PLL0_PLLCTL1, 0, 0);

		mdiv2 = (pll_reg & PMU4335_PLL0_PC1_MDIV2_MASK) >>
				PMU4335_PLL0_PC1_MDIV2_SHIFT;
		mac_clk = FVCO / mdiv2;
		break;
	default:
		PMU_MSG(("si_mac_clk: Unknown chipid %s\n", bcm_chipname(sih->chip, chn, 8)));
		ASSERT(0);
		break;
	}

	/* Return to original core */
	si_restore_core(sih, origidx, intr_val);


	return mac_clk;
} /* si_mac_clk */

/* Get chip's FVCO and PLLCTRL1 register value */
extern int
si_pmu_fvco_pllreg(si_t *sih, uint32 *fvco, uint32 *pllreg)
{
	chipcregs_t *cc;
	uint origidx, intr_val;
#ifdef BCMDBG
	char chn[8];
#endif

	if (fvco)
		*fvco = si_pmu1_pllfvco0(sih)/1000;

	/* Remember original core before switch to chipc */
	cc = (chipcregs_t *)si_switch_core(sih, CC_CORE_ID, &origidx, &intr_val);
	ASSERT(cc != NULL);
	BCM_REFERENCE(cc);

	switch (CHIPID(sih->chip)) {
	case BCM4335_CHIP_ID:
	case BCM4345_CHIP_ID:
	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
		if (pllreg)
			*pllreg = si_pmu_pllcontrol(sih, PMU1_PLL0_PLLCTL1, 0, 0);
		break;

	default:
		PMU_MSG(("si_mac_clk: Unknown chipid %s\n", bcm_chipname(sih->chip, chn, 8)));
		ASSERT(0);
		return -1;
	}

	/* Return to original core */
	si_restore_core(sih, origidx, intr_val);

	return 0;
}

/* Is not called in PHOENIX2_BRANCH_6_10. Dead code ? */
bool
si_pmu_is_autoresetphyclk_disabled(si_t *sih, osl_t *osh)
{
	bool disable = FALSE;

	switch (CHIPID(sih->chip)) {
	case BCM43239_CHIP_ID:
		W_REG(osh, PMUREG(sih, chipcontrol_addr), 0);
		if (R_REG(osh, PMUREG(sih, chipcontrol_data)) & 0x00000002)
			disable = TRUE;
		break;
	default:
		break;
	}

	return disable;
}

/* For 43602a0 MCH2/MCH5 boards: power up PA Reference LDO */
void
si_pmu_switch_on_PARLDO(si_t *sih, osl_t *osh)
{
	uint32 mask;

	switch (CHIPID(sih->chip)) {
	case BCM43602_CHIP_ID:
	case BCM43462_CHIP_ID:
		mask = R_REG(osh, PMUREG(sih, min_res_mask)) | PMURES_BIT(RES43602_PARLDO_PU);
		W_REG(osh, PMUREG(sih, min_res_mask), mask);
		mask = R_REG(osh, PMUREG(sih, max_res_mask)) | PMURES_BIT(RES43602_PARLDO_PU);
		W_REG(osh, PMUREG(sih, max_res_mask), mask);
		break;
	default:
		break;
	}
}

/**
 * Change VCO frequency (slightly), e.g. to avoid PHY errors due to spurs.
 */
static void
BCMATTACHFN(si_set_bb_vcofreq_frac)(si_t *sih, osl_t *osh, int vcofreq, int frac, int xtalfreq)
{
	uint32 vcofreq_withfrac, p1div, ndiv_int, fraca, ndiv_mode, reg;
	/* shifts / masks for PMU PLL control register #2 : */
	uint32 ndiv_int_shift, ndiv_mode_shift, p1div_shift, pllctrl2_mask;
	/* shifts / masks for PMU PLL control register #3 : */
	uint32 pllctrl3_mask;

	if ((CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43460_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43462_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43526_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM4352_CHIP_ID)) {
		chipcregs_t *cc;
		cc = si_setcoreidx(sih, SI_CC_IDX);

		/* changing BBPLL frequency would lead to USB interface problem */
		if (CHIPID(sih->chip) == BCM4360_CHIP_ID &&
		    (R_REG(osh, &cc->chipstatus) & CST4360_MODE_USB) &&
		    (R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL)) {
			PMU_MSG(("HTAVAIL is set, so not updating BBPLL Frequency \n"));
			return;
		}

		ndiv_int_shift = 7;
		ndiv_mode_shift = 4;
		p1div_shift = 0;
		pllctrl2_mask = 0xffffffff;
		pllctrl3_mask = 0xffffffff;
	} else if (BCM4350_CHIP(sih->chip) &&
		(CST4350_IFC_MODE(sih->chipst) == CST4350_IFC_MODE_PCIE)) {
		ndiv_int_shift = 23;
		ndiv_mode_shift = 20;
		p1div_shift = 16;
		pllctrl2_mask = 0xffff0000;
		pllctrl3_mask = 0x00ffffff;
	} else {
		/* put more chips here */
		PMU_ERROR(("%s: only work on 4360, 4350\n", __FUNCTION__));
		return;
	}

	vcofreq_withfrac = vcofreq * 10000 + frac;
	p1div = 0x1;
	ndiv_int = vcofreq / xtalfreq;
	ndiv_mode = (vcofreq_withfrac % (xtalfreq * 10000)) ? 3 : 0;
	PMU_ERROR(("ChangeVCO => vco:%d, xtalF:%d, frac: %d, ndivMode: %d, ndivint: %d\n",
		vcofreq, xtalfreq, frac, ndiv_mode, ndiv_int));

	reg = (ndiv_int << ndiv_int_shift) |
	      (ndiv_mode << ndiv_mode_shift) |
	      (p1div << p1div_shift);
	PMU_ERROR(("Data written into the PLL_CNTRL_ADDR2: %08x\n", reg));
	si_pmu_pllcontrol(sih, 2, pllctrl2_mask, reg);

	if (ndiv_mode) {
		/* frac = (vcofreq_withfrac % (xtalfreq * 10000)) * 2^24) / (xtalfreq * 10000) */
		uint32 r1, r0;
		bcm_uint64_multiple_add(
			&r1, &r0, vcofreq_withfrac % (xtalfreq * 10000), 1 << 24, 0);
		bcm_uint64_divide(&fraca, r1, r0, xtalfreq * 10000);
		PMU_ERROR(("Data written into the PLL_CNTRL_ADDR3 (Fractional): %08x\n", fraca));
		si_pmu_pllcontrol(sih, 3, pllctrl3_mask, fraca);
	}

	si_pmu_pllupd(sih);
} /* si_set_bb_vcofreq_frac */

/** given x-tal frequency, returns vcofreq with fraction in 100Hz */
uint32
si_pmu_get_bb_vcofreq(si_t *sih, osl_t *osh, int xtalfreq)
{
	uint32  ndiv_int,	/* 9 bits integer divider */
		ndiv_mode,
		frac = 0,	/* 24 bits fractional divider */
		p1div; 		/* predivider: divides x-tal freq */
	uint32 xtal1, vcofrac = 0, vcofreq;
	uint32 r1, r0, reg;

	if ((CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43460_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43526_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43462_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM4352_CHIP_ID)) {
		reg = si_pmu_pllcontrol(sih, 2, 0, 0);
		ndiv_int = reg >> 7;
		ndiv_mode = (reg >> 4) & 7;
		p1div = 1; /* do not divide x-tal frequency */

		if (ndiv_mode)
			frac = si_pmu_pllcontrol(sih, 3, 0, 0);
	} else if (BCM4350_CHIP(sih->chip) &&
		(CST4350_IFC_MODE(sih->chipst) == CST4350_IFC_MODE_PCIE)) {
		reg = si_pmu_pllcontrol(sih, 2, 0, 0);
		ndiv_int = reg >> 23;
		ndiv_mode = (reg >> 20) & 7;
		p1div = (reg >> 16) & 0xf;

		if (ndiv_mode)
			frac = si_pmu_pllcontrol(sih, 3, 0, 0) & 0x00ffffff;
	} else {
		/* put more chips here */
		PMU_ERROR(("%s: only work on 4360, 4350\n", __FUNCTION__));
		return 0;
	}

	xtal1 = 10000 * xtalfreq / p1div;		/* in [100Hz] units */

	if (ndiv_mode) {
		/* vcofreq fraction = (xtal1 * frac + (1 << 23)) / (1 << 24);
		 * handle overflow
		 */
		bcm_uint64_multiple_add(&r1, &r0, xtal1, frac, 1 << 23);
		vcofrac = (r1 << 8) | (r0 >> 24);
	}

	if ((int)xtal1 > (int)((0xffffffff - vcofrac) / ndiv_int)) {
		PMU_ERROR(("%s: xtalfreq is too big, %d\n", __FUNCTION__, xtalfreq));
		return 0;
	}

	vcofreq = xtal1 * ndiv_int + vcofrac;
	return vcofreq;
} /* si_pmu_get_bb_vcofreq */

/* Enable PMU 1Mhz clock */
static void
si_pmu_enb_slow_clk(si_t *sih, osl_t *osh, uint32 xtalfreq)
{
	uint32 val;

	if ((sih->buscoretype != PCIE2_CORE_ID) ||
	    ((sih->buscorerev != 7) &&
	     (sih->buscorerev != 9) &&
	     (sih->buscorerev != 11)))
		return;

	if (xtalfreq == 37400) {
		val = 0x101B6;
	} else if (xtalfreq == 40000) {
		val = 0x10199;
	} else {
		PMU_ERROR(("%s: xtalfreq is not supported, %d\n", __FUNCTION__, xtalfreq));
		return;
	}
	W_REG(osh, PMUREG(sih, slowclkperiod), val);
}

/**
 * Initializes PLL given an x-tal frequency.
 * Calls si_pmuX_pllinitY() type of functions, where the reasoning behind 'X' and 'Y' is historical
 * rather than logical.
 *
 * xtalfreq : x-tal frequency in [KHz]
 */
void
BCMATTACHFN(si_pmu_pll_init)(si_t *sih, osl_t *osh, uint xtalfreq)
{
	chipcregs_t *cc;
	uint origidx;
#ifdef BCMDBG
	char chn[8];
#endif

	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	switch (CHIPID(sih->chip)) {
	case BCM4328_CHIP_ID:
		si_pmu0_pllinit0(sih, osh, cc, xtalfreq);
		break;
	case BCM5354_CHIP_ID:
		if (xtalfreq == 0)
			xtalfreq = 25000;
		si_pmu0_pllinit0(sih, osh, cc, xtalfreq);
		break;
	case BCM4325_CHIP_ID:
		si_pmu1_pllinit0(sih, osh, cc, xtalfreq);
		break;
	case BCM4329_CHIP_ID:
		if (xtalfreq == 0)
			xtalfreq = 38400;
		si_pmu1_pllinit0(sih, osh, cc, xtalfreq);
		break;
	case BCM4312_CHIP_ID:
		/* assume default works */
		break;
	case BCM4322_CHIP_ID:
	case BCM43221_CHIP_ID:
	case BCM43231_CHIP_ID:
	case BCM4342_CHIP_ID: {
		if (CHIPREV(sih->chiprev) == 0) {
			uint32 minmask, maxmask;

			minmask = R_REG(osh, PMUREG(sih, min_res_mask));
			maxmask = R_REG(osh, PMUREG(sih, max_res_mask));

			/* Make sure the PLL is off: clear bit 4 & 5 of min/max_res_mask */
			/* Have to remove HT Avail request before powering off PLL */
			AND_REG(osh, PMUREG(sih, min_res_mask),	~(PMURES_BIT(RES4322_HT_SI_AVAIL)));
			AND_REG(osh, PMUREG(sih, max_res_mask),	~(PMURES_BIT(RES4322_HT_SI_AVAIL)));
			SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL, PMU_MAX_TRANSITION_DLY);
			AND_REG(osh, PMUREG(sih, min_res_mask),	~(PMURES_BIT(RES4322_SI_PLL_ON)));
			AND_REG(osh, PMUREG(sih, max_res_mask),	~(PMURES_BIT(RES4322_SI_PLL_ON)));
			OSL_DELAY(1000);
			ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));


			W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU2_SI_PLL_PLLCTL);
			W_REG(osh, PMUREG(sih, pllcontrol_data), 0x380005c0);


			OSL_DELAY(100);
			W_REG(osh, PMUREG(sih, max_res_mask), maxmask);
			OSL_DELAY(100);
			W_REG(osh, PMUREG(sih, min_res_mask), minmask);
			OSL_DELAY(100);
		}

		break;
	}

	case BCM4360_CHIP_ID: /* 4360B1 */
	case BCM43460_CHIP_ID:
	case BCM4352_CHIP_ID: {
		if (CHIPREV(sih->chiprev) > 2)
			si_set_bb_vcofreq_frac(sih, osh, 960, 98, 40);
		break;
	}
	case BCM43602_CHIP_ID:
	case BCM43462_CHIP_ID:
		si_set_bb_vcofreq_frac(sih, osh, 960, 98, 40);
		break;
	case BCM4313_CHIP_ID:
	case BCM43222_CHIP_ID:	case BCM43111_CHIP_ID:	case BCM43112_CHIP_ID:
	case BCM43224_CHIP_ID:	case BCM43225_CHIP_ID:  case BCM43420_CHIP_ID:
	case BCM43421_CHIP_ID:
	case BCM43226_CHIP_ID:
	case BCM43235_CHIP_ID:	case BCM43236_CHIP_ID:	case BCM43238_CHIP_ID:
	case BCM43237_CHIP_ID:
	case BCM43234_CHIP_ID:
	case BCM4331_CHIP_ID:
	case BCM43431_CHIP_ID:
	case BCM43131_CHIP_ID:
	case BCM43217_CHIP_ID:
	case BCM43227_CHIP_ID:
	case BCM43228_CHIP_ID:
	case BCM43428_CHIP_ID:
	case BCM6362_CHIP_ID:
		break;
	case BCM4315_CHIP_ID:
	case BCM4319_CHIP_ID:
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
	case BCM4330_CHIP_ID:
		si_pmu1_pllinit0(sih, osh, cc, xtalfreq);
		break;
	case BCM43239_CHIP_ID:
	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
	case BCM4335_CHIP_ID:
	case BCM4345_CHIP_ID:
		si_pmu1_pllinit1(sih, osh, cc, xtalfreq);
		break;
	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
		si_pmu1_pllinit1(sih, osh, cc, xtalfreq);
		if (xtalfreq == 40000)
			si_set_bb_vcofreq_frac(sih, osh, 968, 0, 40);
		break;
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
	case BCM43143_CHIP_ID:
	case BCM43341_CHIP_ID:
		if (xtalfreq == 0)
			xtalfreq = 20000;
		si_pmu2_pllinit0(sih, osh, cc, xtalfreq);
		break;
	case BCM4334_CHIP_ID:
		si_pmu2_pllinit0(sih, osh, cc, xtalfreq);
		break;
	default:
		PMU_MSG(("No PLL init done for chip %s rev %d pmurev %d\n",
		         bcm_chipname(sih->chip, chn, 8), sih->chiprev, sih->pmurev));
		break;
	}

#ifdef BCMDBG_FORCEHT
	OR_REG(osh, &cc->clk_ctl_st, CCS_FORCEHT);
#endif

	si_pmu_enb_slow_clk(sih, osh, xtalfreq);

	/* Return to original core */
	si_setcoreidx(sih, origidx);
} /* si_pmu_pll_init */

/** get alp clock frequency in [Hz] units */
uint32
BCMINITFN(si_pmu_alp_clock)(si_t *sih, osl_t *osh)
{
	chipcregs_t *cc;
	uint origidx;
	uint32 clock = ALP_CLOCK;
#ifdef BCMDBG
	char chn[8];
#endif

	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	switch (CHIPID(sih->chip)) {
	case BCM4328_CHIP_ID:
		clock = si_pmu0_alpclk0(sih, osh, cc);
		break;
	case BCM5354_CHIP_ID:
		clock = si_pmu0_alpclk0(sih, osh, cc);
		break;
	case BCM4325_CHIP_ID:
		clock = si_pmu1_alpclk0(sih, osh, cc);
		break;
	case BCM4360_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM4352_CHIP_ID:
	case BCM43526_CHIP_ID:
		if (sih->chipst & CST4360_XTAL_40MZ)
			clock = 40000 * 1000;
		else
			clock = 20000 * 1000;
		break;

	case BCM43602_CHIP_ID:
	case BCM43462_CHIP_ID:
		/* always 40Mhz */
		clock = 40000 * 1000;
		break;
	case BCM4312_CHIP_ID:
	case BCM4322_CHIP_ID:	case BCM43221_CHIP_ID:	case BCM43231_CHIP_ID:
	case BCM43222_CHIP_ID:	case BCM43111_CHIP_ID:	case BCM43112_CHIP_ID:
	case BCM43224_CHIP_ID:	case BCM43225_CHIP_ID:  case BCM43420_CHIP_ID:
	case BCM43421_CHIP_ID:
	case BCM43226_CHIP_ID:
	case BCM43235_CHIP_ID:	case BCM43236_CHIP_ID:	case BCM43238_CHIP_ID:
	case BCM43237_CHIP_ID:	case BCM43239_CHIP_ID:
	case BCM43234_CHIP_ID:
	case BCM4331_CHIP_ID:
	case BCM43431_CHIP_ID:
	case BCM43131_CHIP_ID:
	case BCM43217_CHIP_ID:
	case BCM43227_CHIP_ID:
	case BCM43228_CHIP_ID:
	case BCM43428_CHIP_ID:
	case BCM6362_CHIP_ID:
	case BCM4342_CHIP_ID:
	case BCM4716_CHIP_ID:
	case BCM4748_CHIP_ID:
	case BCM47162_CHIP_ID:
	case BCM4313_CHIP_ID:
	case BCM5357_CHIP_ID:
	case BCM4749_CHIP_ID:
	case BCM53572_CHIP_ID:
		/* always 20Mhz */
		clock = 20000 * 1000;
		break;
	case BCM4329_CHIP_ID:
	case BCM4315_CHIP_ID:
	case BCM4319_CHIP_ID:
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
	case BCM4330_CHIP_ID:
	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
	case BCM4335_CHIP_ID:
	case BCM4345_CHIP_ID:
	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
		clock = si_pmu1_alpclk0(sih, osh, cc);
		break;
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
	case BCM43143_CHIP_ID:
	case BCM43341_CHIP_ID:
	case BCM4334_CHIP_ID:
		clock = si_pmu2_alpclk0(sih, osh, cc);
		break;
	case BCM5356_CHIP_ID:
	case BCM4706_CHIP_ID:
		/* always 25Mhz */
		clock = 25000 * 1000;
		break;
	default:
		PMU_MSG(("No ALP clock specified "
			"for chip %s rev %d pmurev %d, using default %d Hz\n",
			bcm_chipname(sih->chip, chn, 8), sih->chiprev, sih->pmurev, clock));
		break;
	}

	/* Return to original core */
	si_setcoreidx(sih, origidx);
	return clock;
} /* si_pmu_alp_clock */

/**
 * Find the output of the "m" pll divider given pll controls that start with
 * pllreg "pll0" i.e. 12 for main 6 for phy, 0 for misc.
 */
static uint32
BCMINITFN(si_pmu5_clock)(si_t *sih, osl_t *osh, chipcregs_t *cc, uint pll0, uint m)
{
	uint32 tmp, div, ndiv, p1, p2, fc;

	if ((pll0 & 3) || (pll0 > PMU4716_MAINPLL_PLL0)) {
		PMU_ERROR(("%s: Bad pll0: %d\n", __FUNCTION__, pll0));
		return 0;
	}


	/* Strictly there is an m5 divider, but I'm not sure we use it */
	if ((m == 0) || (m > 4)) {
		PMU_ERROR(("%s: Bad m divider: %d\n", __FUNCTION__, m));
		return 0;
	}

	if ((CHIPID(sih->chip) == BCM5357_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4749_CHIP_ID)) {
		/* Detect failure in clock setting */
		if ((R_REG(osh, &cc->chipstatus) & 0x40000) != 0) {
			return (133 * 1000000);
		}
	}

	W_REG(osh, PMUREG(sih, pllcontrol_addr), pll0 + PMU5_PLL_P1P2_OFF);
	(void)R_REG(osh, PMUREG(sih, pllcontrol_addr));
	tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
	p1 = (tmp & PMU5_PLL_P1_MASK) >> PMU5_PLL_P1_SHIFT;
	p2 = (tmp & PMU5_PLL_P2_MASK) >> PMU5_PLL_P2_SHIFT;

	W_REG(osh, PMUREG(sih, pllcontrol_addr), pll0 + PMU5_PLL_M14_OFF);
	(void)R_REG(osh, PMUREG(sih, pllcontrol_addr));
	tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
	div = (tmp >> ((m - 1) * PMU5_PLL_MDIV_WIDTH)) & PMU5_PLL_MDIV_MASK;

	W_REG(osh, PMUREG(sih, pllcontrol_addr), pll0 + PMU5_PLL_NM5_OFF);
	(void)R_REG(osh, PMUREG(sih, pllcontrol_addr));
	tmp = R_REG(osh, PMUREG(sih, pllcontrol_data));
	ndiv = (tmp & PMU5_PLL_NDIV_MASK) >> PMU5_PLL_NDIV_SHIFT;

	/* Do calculation in Mhz */
	fc = si_pmu_alp_clock(sih, osh) / 1000000;
	fc = (p1 * ndiv * fc) / p2;

	PMU_NONE(("%s: p1=%d, p2=%d, ndiv=%d(0x%x), m%d=%d; fc=%d, clock=%d\n",
	          __FUNCTION__, p1, p2, ndiv, ndiv, m, div, fc, fc / div));

	/* Return clock in Hertz */
	return ((fc / div) * 1000000);
} /* si_pmu5_clock */

static uint32
BCMINITFN(si_4706_pmu_clock)(si_t *sih, osl_t *osh, chipcregs_t *cc, uint pll0, uint m)
{
	uint32 w, ndiv, p1div, p2div;
	uint32 clock;

	/* Strictly there is an m5 divider, but I'm not sure we use it */
	if ((m == 0) || (m > 4)) {
		PMU_ERROR(("%s: Bad m divider: %d\n", __FUNCTION__, m));
		return 0;
	}

	/* Get N, P1 and P2 dividers to determine CPU clock */
	W_REG(osh, PMUREG(sih, pllcontrol_addr), pll0 + PMU6_4706_PROCPLL_OFF);
	w = R_REG(NULL, PMUREG(sih, pllcontrol_data));
	ndiv = (w & PMU6_4706_PROC_NDIV_INT_MASK) >> PMU6_4706_PROC_NDIV_INT_SHIFT;
	p1div = (w & PMU6_4706_PROC_P1DIV_MASK) >> PMU6_4706_PROC_P1DIV_SHIFT;
	p2div = (w & PMU6_4706_PROC_P2DIV_MASK) >> PMU6_4706_PROC_P2DIV_SHIFT;

	if (R_REG(osh, &cc->chipstatus) & CST4706_PKG_OPTION)
		/* Low cost bonding: Fixed reference clock 25MHz and m = 4 */
		clock = (25000000 / 4) * ndiv * p2div / p1div;
	else
		/* Fixed reference clock 25MHz and m = 2 */
		clock = (25000000 / 2) * ndiv * p2div / p1div;

	if (m == PMU5_MAINPLL_MEM)
		clock = clock / 2;
	else if (m == PMU5_MAINPLL_SI)
		clock = clock / 4;

	return clock;
}

/**
 * Get backplane clock frequency, returns a value in [hz] units.
 * For designs that feed the same clock to both backplane and CPU just return the CPU clock speed.
 */
uint32
BCMINITFN(si_pmu_si_clock)(si_t *sih, osl_t *osh)
{
	chipcregs_t *cc;
	uint origidx;
	uint32 clock = HT_CLOCK;	/* in [hz] units */
#ifdef BCMDBG
	char chn[8];
#endif

	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	switch (CHIPID(sih->chip)) {
	case BCM4328_CHIP_ID:
		clock = si_pmu0_cpuclk0(sih, osh, cc);
		break;
	case BCM5354_CHIP_ID:
		clock = 120000000;
		break;
	case BCM4325_CHIP_ID:
		clock = si_pmu1_cpuclk0(sih, osh, cc);
		break;
	case BCM4322_CHIP_ID:
	case BCM43221_CHIP_ID:	case BCM43231_CHIP_ID:
	case BCM43222_CHIP_ID:	case BCM43111_CHIP_ID:	case BCM43112_CHIP_ID:
	case BCM43224_CHIP_ID:  case BCM43420_CHIP_ID:
	case BCM43225_CHIP_ID:
	case BCM43421_CHIP_ID:
	case BCM43226_CHIP_ID:
	case BCM4331_CHIP_ID:
	case BCM43431_CHIP_ID:
	case BCM6362_CHIP_ID:
	case BCM4342_CHIP_ID:
		/* 96MHz backplane clock */
		clock = 96000 * 1000;
		break;
	case BCM4716_CHIP_ID:
	case BCM4748_CHIP_ID:
	case BCM47162_CHIP_ID:
		clock = si_pmu5_clock(sih, osh, cc, PMU4716_MAINPLL_PLL0, PMU5_MAINPLL_SI);
		break;
	case BCM4329_CHIP_ID:
		if (CHIPREV(sih->chiprev) == 0)
			clock = 38400 * 1000;
		else
			clock = si_pmu1_cpuclk0(sih, osh, cc);
		break;
	case BCM4315_CHIP_ID:
	case BCM4319_CHIP_ID:
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
	case BCM4330_CHIP_ID:
	case BCM43239_CHIP_ID:
	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
	case BCM4335_CHIP_ID:
	case BCM4345_CHIP_ID:
	case BCM4360_CHIP_ID:
	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM43526_CHIP_ID:
	case BCM4352_CHIP_ID:
		clock = si_pmu1_cpuclk0(sih, osh, cc);
		break;
	case BCM43462_CHIP_ID:
	case BCM43602_CHIP_ID: {
			uint32 mdiv;
			/* Ch3 is connected to backplane_clk. Read 'bbpll_i_m3div' from pllctl[4] */
			W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU1_PLL0_PLLCTL4);
			mdiv = (R_REG(osh, PMUREG(sih, pllcontrol_data)) &
				PMU1_PLL0_PC1_M3DIV_MASK) >> PMU1_PLL0_PC1_M3DIV_SHIFT;
			clock = si_pmu1_pllfvco0(sih) / mdiv * 1000;
			break;
		}
	case BCM4313_CHIP_ID:
		/* 80MHz backplane clock */
		clock = 80000 * 1000;
		break;
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
	case BCM43143_CHIP_ID: /* HT clock and ARM clock have the same frequency */
	case BCM43341_CHIP_ID:
	case BCM4334_CHIP_ID:
		clock = si_pmu2_cpuclk0(sih, osh, cc);
		break;
	case BCM43235_CHIP_ID:	case BCM43236_CHIP_ID:	case BCM43238_CHIP_ID:
	case BCM43237_CHIP_ID:
	case BCM43234_CHIP_ID:
		clock = (cc->chipstatus & CST43236_BP_CLK) ? (120000 * 1000) : (96000 * 1000);
		break;
	case BCM5356_CHIP_ID:
		clock = si_pmu5_clock(sih, osh, cc, PMU5356_MAINPLL_PLL0, PMU5_MAINPLL_SI);
		break;
	case BCM5357_CHIP_ID:
	case BCM4749_CHIP_ID:
		clock = si_pmu5_clock(sih, osh, cc, PMU5357_MAINPLL_PLL0, PMU5_MAINPLL_SI);
		break;
	case BCM53572_CHIP_ID:
		clock = 75000000;
		break;
	case BCM4706_CHIP_ID:
		clock = si_4706_pmu_clock(sih, osh, cc, PMU4706_MAINPLL_PLL0, PMU5_MAINPLL_SI);
		break;
	default:
		PMU_MSG(("No backplane clock specified "
			"for chip %s rev %d pmurev %d, using default %d Hz\n",
			bcm_chipname(sih->chip, chn, 8), sih->chiprev, sih->pmurev, clock));
		break;
	}

	/* Return to original core */
	si_setcoreidx(sih, origidx);
	return clock;
} /* si_pmu_si_clock */

/** returns CPU clock frequency in [hz] units */
uint32
BCMINITFN(si_pmu_cpu_clock)(si_t *sih, osl_t *osh)
{
	chipcregs_t *cc;
	uint origidx;
	uint32 clock;	/* in [hz] units */

	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* 5354 chip uses a non programmable PLL of frequency 240MHz */
	if (CHIPID(sih->chip) == BCM5354_CHIP_ID)
		return 240000000;

	if (CHIPID(sih->chip) == BCM53572_CHIP_ID)
		return 300000000;

	if ((sih->pmurev >= 5) &&
		!((CHIPID(sih->chip) == BCM4329_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4319_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43234_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43235_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43236_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43238_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43237_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43239_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4336_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43362_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4314_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43142_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43143_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43341_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4334_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4324_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43242_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43243_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4330_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4352_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43526_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43460_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4345_CHIP_ID) ||
		BCM4350_CHIP(sih->chip) ||
		(CHIPID(sih->chip) == BCM4335_CHIP_ID) ||
		0)) {
		uint pll;

		switch (CHIPID(sih->chip)) {
		case BCM5356_CHIP_ID:
			pll = PMU5356_MAINPLL_PLL0;
			break;
		case BCM5357_CHIP_ID:
		case BCM4749_CHIP_ID:
			pll = PMU5357_MAINPLL_PLL0;
			break;
		default:
			pll = PMU4716_MAINPLL_PLL0;
			break;
		}

		/* Remember original core before switch to chipc */
		origidx = si_coreidx(sih);
		cc = si_setcoreidx(sih, SI_CC_IDX);
		ASSERT(cc != NULL);

		if (CHIPID(sih->chip) == BCM4706_CHIP_ID)
			clock = si_4706_pmu_clock(sih, osh, cc,
				PMU4706_MAINPLL_PLL0, PMU5_MAINPLL_CPU);
		else if (CHIPID(sih->chip) == BCM43602_CHIP_ID ||
			CHIPID(sih->chip) == BCM43462_CHIP_ID)
			clock = si_pmu1_cpuclk0(sih, osh, cc);
		else
			clock = si_pmu5_clock(sih, osh, cc, pll, PMU5_MAINPLL_CPU);

		/* Return to original core */
		si_setcoreidx(sih, origidx);
	} else
		clock = si_pmu_si_clock(sih, osh);

	return clock;
} /* si_pmu_cpu_clock */

/** get memory clock frequency, which is the same as the HT clock for newer chips. Returns [Hz]. */
uint32
BCMINITFN(si_pmu_mem_clock)(si_t *sih, osl_t *osh)
{
	chipcregs_t *cc;
	uint origidx;
	uint32 clock;

	ASSERT(sih->cccaps & CC_CAP_PMU);

	if (CHIPID(sih->chip) == BCM53572_CHIP_ID)
		return 150000000;

	if ((sih->pmurev >= 5) &&
		!((CHIPID(sih->chip) == BCM4329_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4319_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4330_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4314_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43142_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43143_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43341_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4334_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4336_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43362_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43234_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43235_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43236_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43238_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43237_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43239_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4324_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43242_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43243_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4335_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4345_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43462_CHIP_ID) ||
		BCM4350_CHIP(sih->chip) ||
		0)) {
		uint pll;

		switch (CHIPID(sih->chip)) {
		case BCM5356_CHIP_ID:
			pll = PMU5356_MAINPLL_PLL0;
			break;
		case BCM5357_CHIP_ID:
		case BCM4749_CHIP_ID:
			pll = PMU5357_MAINPLL_PLL0;
			break;
		default:
			pll = PMU4716_MAINPLL_PLL0;
			break;
		}

		/* Remember original core before switch to chipc */
		origidx = si_coreidx(sih);
		cc = si_setcoreidx(sih, SI_CC_IDX);
		ASSERT(cc != NULL);

		if (CHIPID(sih->chip) == BCM4706_CHIP_ID)
			clock = si_4706_pmu_clock(sih, osh, cc,
				PMU4706_MAINPLL_PLL0, PMU5_MAINPLL_MEM);
		else
			clock = si_pmu5_clock(sih, osh, cc, pll, PMU5_MAINPLL_MEM);

		/* Return to original core */
		si_setcoreidx(sih, origidx);
	} else {
		clock = si_pmu_si_clock(sih, osh);
	}

	return clock;
} /* si_pmu_mem_clock */

/*
 * ilpcycles per sec are now calculated during CPU init in a new way
 * for better accuracy.  We set it here for compatability.
 *
 * On platforms that do not do this we resort to the old way.
 */

#define ILP_CALC_DUR	10	/* ms, make sure 1000 can be divided by it. */

static uint32 ilpcycles_per_sec = 0;

void
BCMINITFN(si_pmu_ilp_clock_set)(uint32 cycles_per_sec)
{
	ilpcycles_per_sec = cycles_per_sec;
}

uint32
BCMINITFN(si_pmu_ilp_clock)(si_t *sih, osl_t *osh)
{
	if (ISSIM_ENAB(sih))
		return ILP_CLOCK;

	if (ilpcycles_per_sec == 0) {
		uint32 start, end, delta;
		start = R_REG(osh, PMUREG(sih, pmutimer));
		if (start != R_REG(osh, PMUREG(sih, pmutimer)))
			start = R_REG(osh, PMUREG(sih, pmutimer));
		OSL_DELAY(ILP_CALC_DUR * 1000);
		end = R_REG(osh, PMUREG(sih, pmutimer));
		if (end != R_REG(osh, PMUREG(sih, pmutimer)))
			end = R_REG(osh, PMUREG(sih, pmutimer));
		delta = end - start;
		ilpcycles_per_sec = delta * (1000 / ILP_CALC_DUR);
	}

	return ilpcycles_per_sec;
}
#endif /* !defined(BCMDONGLEHOST) */

/* SDIO Pad drive strength to select value mappings.
 * The last strength value in each table must be 0 (the tri-state value).
 */
typedef struct {
	uint8 strength;			/* Pad Drive Strength in mA */
	uint8 sel;			/* Chip-specific select value */
} sdiod_drive_str_t;

/* SDIO Drive Strength to sel value table for PMU Rev 1 */
static const sdiod_drive_str_t BCMINITDATA(sdiod_drive_strength_tab1)[] = {
	{4, 0x2},
	{2, 0x3},
	{1, 0x0},
	{0, 0x0} };

/* SDIO Drive Strength to sel value table for PMU Rev 2, 3 */
static const sdiod_drive_str_t BCMINITDATA(sdiod_drive_strength_tab2)[] = {
	{12, 0x7},
	{10, 0x6},
	{8, 0x5},
	{6, 0x4},
	{4, 0x2},
	{2, 0x1},
	{0, 0x0} };

/* SDIO Drive Strength to sel value table for PMU Rev 8 (1.8V) */
static const sdiod_drive_str_t BCMINITDATA(sdiod_drive_strength_tab3)[] = {
	{32, 0x7},
	{26, 0x6},
	{22, 0x5},
	{16, 0x4},
	{12, 0x3},
	{8, 0x2},
	{4, 0x1},
	{0, 0x0} };

/* SDIO Drive Strength to sel value table for PMU Rev 11 (1.8v) */
static const sdiod_drive_str_t BCMINITDATA(sdiod_drive_strength_tab4_1v8)[] = {
	{32, 0x6},
	{26, 0x7},
	{22, 0x4},
	{16, 0x5},
	{12, 0x2},
	{8, 0x3},
	{4, 0x0},
	{0, 0x1} };

/* SDIO Drive Strength to sel value table for PMU Rev 11 (1.2v) */

/* SDIO Drive Strength to sel value table for PMU Rev 11 (2.5v) */

/* SDIO Drive Strength to sel value table for PMU Rev 13 (1.8v) */
static const sdiod_drive_str_t BCMINITDATA(sdiod_drive_strength_tab5_1v8)[] = {
	{6, 0x7},
	{5, 0x6},
	{4, 0x5},
	{3, 0x4},
	{2, 0x2},
	{1, 0x1},
	{0, 0x0} };

/* SDIO Drive Strength to sel value table for PMU Rev 13 (3.3v) */

/** SDIO Drive Strength to sel value table for PMU Rev 17 (1.8v) */
static const sdiod_drive_str_t BCMINITDATA(sdiod_drive_strength_tab6_1v8)[] = {
	{3, 0x3},
	{2, 0x2},
	{1, 0x1},
	{0, 0x0} };


/**
 * SDIO Drive Strength to sel value table for 43143 PMU Rev 17, see Confluence 43143 Toplevel
 * architecture page, section 'PMU Chip Control 1 Register definition', click link to picture
 * BCM43143_sel_sdio_signals.jpg. Valid after PMU Chip Control 0 Register, bit31 (override) has
 * been written '1'.
 */
#if !defined(BCM_SDIO_VDDIO) || BCM_SDIO_VDDIO == 33

static const sdiod_drive_str_t BCMINITDATA(sdiod_drive_strength_tab7_3v3)[] = {
	/* note: for 14, 10, 6 and 2mA hw timing is not met according to rtl team */
	{16, 0x7},
	{12, 0x5},
	{8,  0x3},
	{4,  0x1} }; /* note: 43143 does not support tristate */

#else

static const sdiod_drive_str_t BCMINITDATA(sdiod_drive_strength_tab7_1v8)[] = {
	/* note: for 7, 5, 3 and 1mA hw timing is not met according to rtl team */
	{8, 0x7},
	{6, 0x5},
	{4,  0x3},
	{2,  0x1} }; /* note: 43143 does not support tristate */

#endif /* BCM_SDIO_VDDIO */

#define SDIOD_DRVSTR_KEY(chip, pmu)	(((chip) << 16) | (pmu))

/**
 * Balance between stable SDIO operation and power consumption is achieved using this function.
 * Note that each drive strength table is for a specific VDDIO of the SDIO pads, ideally this
 * function should read the VDDIO itself to select the correct table. For now it has been solved
 * with the 'BCM_SDIO_VDDIO' preprocessor constant.
 *
 * 'drivestrength': desired pad drive strength in mA. Drive strength of 0 requests tri-state (if
 *		    hardware supports this), if no hw support drive strength is not programmed.
 */
void
BCMINITFN(si_sdiod_drive_strength_init)(si_t *sih, osl_t *osh, uint32 drivestrength)
{
	sdiod_drive_str_t *str_tab = NULL;
	uint32 str_mask = 0;	/* only alter desired bits in PMU chipcontrol 1 register */
	uint32 str_shift = 0;
	uint32 str_ovr_pmuctl = PMU_CHIPCTL0; /* PMU chipcontrol register containing override bit */
	uint32 str_ovr_pmuval = 0;            /* position of bit within this register */
#ifdef BCMDBG
	char chn[8];
#endif

	if (!(sih->cccaps & CC_CAP_PMU)) {
		return;
	}

	switch (SDIOD_DRVSTR_KEY(sih->chip, sih->pmurev)) {
	case SDIOD_DRVSTR_KEY(BCM4325_CHIP_ID, 1):
		str_tab = (sdiod_drive_str_t *)&sdiod_drive_strength_tab1;
		str_mask = 0x30000000;
		str_shift = 28;
		break;
	case SDIOD_DRVSTR_KEY(BCM4325_CHIP_ID, 2):
	case SDIOD_DRVSTR_KEY(BCM4325_CHIP_ID, 3):
	case SDIOD_DRVSTR_KEY(BCM4315_CHIP_ID, 4):
		str_tab = (sdiod_drive_str_t *)&sdiod_drive_strength_tab2;
		str_mask = 0x00003800;
		str_shift = 11;
		break;
	case SDIOD_DRVSTR_KEY(BCM4336_CHIP_ID, 8):
	case SDIOD_DRVSTR_KEY(BCM4336_CHIP_ID, 11):
		if (sih->pmurev == 8) {
			str_tab = (sdiod_drive_str_t *)&sdiod_drive_strength_tab3;
		}
		else if (sih->pmurev == 11) {
			str_tab = (sdiod_drive_str_t *)&sdiod_drive_strength_tab4_1v8;
		}
		str_mask = 0x00003800;
		str_shift = 11;
		break;
	case SDIOD_DRVSTR_KEY(BCM4330_CHIP_ID, 12):
		str_tab = (sdiod_drive_str_t *)&sdiod_drive_strength_tab4_1v8;
		str_mask = 0x00003800;
		str_shift = 11;
		break;
	case SDIOD_DRVSTR_KEY(BCM43362_CHIP_ID, 13):
		str_tab = (sdiod_drive_str_t *)&sdiod_drive_strength_tab5_1v8;
		str_mask = 0x00003800;
		str_shift = 11;
		break;
	case SDIOD_DRVSTR_KEY(BCM4334_CHIP_ID, 17):
		str_tab = (sdiod_drive_str_t *)&sdiod_drive_strength_tab6_1v8;
		str_mask = 0x00001800;
		str_shift = 11;
		break;
	case SDIOD_DRVSTR_KEY(BCM43143_CHIP_ID, 17):
#if !defined(BCM_SDIO_VDDIO) || BCM_SDIO_VDDIO == 33
		if (drivestrength >=  ARRAYLAST(sdiod_drive_strength_tab7_3v3)->strength) {
			str_tab = (sdiod_drive_str_t *)&sdiod_drive_strength_tab7_3v3;
		}
#else
		if (drivestrength >=  ARRAYLAST(sdiod_drive_strength_tab7_1v8)->strength) {
			str_tab = (sdiod_drive_str_t *)&sdiod_drive_strength_tab7_1v8;
		}
#endif /* BCM_SDIO_VDDIO */
		str_mask = 0x00000007;
		str_ovr_pmuval = PMU43143_CC0_SDIO_DRSTR_OVR;
		break;
	default:
		PMU_MSG(("No SDIO Drive strength init done for chip %s rev %d pmurev %d\n",
		         bcm_chipname(sih->chip, chn, 8), sih->chiprev, sih->pmurev));
		break;
	}

	if (str_tab != NULL) {
		uint32 cc_data_temp;
		int i;

		/* Pick the lowest available drive strength equal or greater than the
		 * requested strength.	Drive strength of 0 requests tri-state.
		 */
		for (i = 0; drivestrength < str_tab[i].strength; i++)
			;

		if (i > 0 && drivestrength > str_tab[i].strength)
			i--;

		W_REG(osh, PMUREG(sih, chipcontrol_addr), PMU_CHIPCTL1);
		cc_data_temp = R_REG(osh, PMUREG(sih, chipcontrol_data));
		cc_data_temp &= ~str_mask;
		cc_data_temp |= str_tab[i].sel << str_shift;
		W_REG(osh, PMUREG(sih, chipcontrol_data), cc_data_temp);
		if (str_ovr_pmuval) { /* enables the selected drive strength */
			W_REG(osh,  PMUREG(sih, chipcontrol_addr), str_ovr_pmuctl);
			OR_REG(osh, PMUREG(sih, chipcontrol_data), str_ovr_pmuval);
		}
		PMU_MSG(("SDIO: %dmA drive strength requested; set to %dmA\n",
		         drivestrength, str_tab[i].strength));
	}
} /* si_sdiod_drive_strength_init */

#if !defined(BCMDONGLEHOST)
/* initialize PMU */
void
BCMATTACHFN(si_pmu_init)(si_t *sih, osl_t *osh)
{
	ASSERT(sih->cccaps & CC_CAP_PMU);

	if (sih->pmurev == 1)
		AND_REG(osh, PMUREG(sih, pmucontrol), ~PCTL_NOILP_ON_WAIT);
	else if (sih->pmurev >= 2)
		OR_REG(osh, PMUREG(sih, pmucontrol), PCTL_NOILP_ON_WAIT);

	switch (CHIPID(sih->chip)) {
	case BCM4329_CHIP_ID:
		if (CHIPREV(sih->chiprev) == 2) {
			/* Fix for 4329b0 bad LPOM state. */
			W_REG(osh, PMUREG(sih, regcontrol_addr), 2);
			OR_REG(osh, PMUREG(sih, regcontrol_data), 0x100);

			W_REG(osh, PMUREG(sih, regcontrol_addr), 3);
			OR_REG(osh, PMUREG(sih, regcontrol_data), 0x4);
		}
		break;
	case BCM4345_CHIP_ID:
#ifdef SRFAST
		OR_REG(osh, PMUREG(sih, pmucontrol_ext), 1 << PCTLEX_FTE_SHIFT);
#endif
		break;
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
#ifdef SRFAST
		/* Similar to 4350C0, rev 3 */
		OR_REG(osh, PMUREG(sih, pmucontrol_ext), 1 << PCTLEX_FTE_SHIFT);
#endif
		break;
	case BCM4350_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
#ifdef SRFAST
		/* Does not apply to Bx */
		if (CHIPREV(sih->chiprev) > 2)
			OR_REG(osh, PMUREG(sih, pmucontrol_ext), 1 << PCTLEX_FTE_SHIFT);
#endif
		break;
	default:
		break;
	}
}

static uint
BCMINITFN(si_pmu_res_uptime)(si_t *sih, osl_t *osh, chipcregs_t *cc, uint8 rsrc)
{
	uint32 deps;
	uint uptime, i, dup, dmax;
	uint32 min_mask = 0;
#ifndef SR_DEBUG
	uint32 max_mask = 0;
#endif /* SR_DEBUG */

	/* uptime of resource 'rsrc' */
	W_REG(osh, PMUREG(sih, res_table_sel), rsrc);
	if (sih->pmurev >= 13)
		uptime = (R_REG(osh, PMUREG(sih, res_updn_timer)) >> 16) & 0x3ff;
	else
		uptime = (R_REG(osh, PMUREG(sih, res_updn_timer)) >> 8) & 0xff;

	/* direct dependencies of resource 'rsrc' */
	deps = si_pmu_res_deps(sih, osh, cc, PMURES_BIT(rsrc), FALSE);
	for (i = 0; i <= PMURES_MAX_RESNUM; i ++) {
		if (!(deps & PMURES_BIT(i)))
			continue;
		deps &= ~si_pmu_res_deps(sih, osh, cc, PMURES_BIT(i), TRUE);
	}
#ifndef SR_DEBUG
	si_pmu_res_masks(sih, &min_mask, &max_mask);
#else
	/* Recalculate fast pwr up delay if min res mask/max res mask has changed */
	min_mask = R_REG(osh, &cc->min_res_mask);
#endif /* SR_DEBUG */
	deps &= ~min_mask;

	/* max uptime of direct dependencies */
	dmax = 0;
	for (i = 0; i <= PMURES_MAX_RESNUM; i ++) {
		if (!(deps & PMURES_BIT(i)))
			continue;
		dup = si_pmu_res_uptime(sih, osh, cc, (uint8)i);
		if (dmax < dup)
			dmax = dup;
	}

	PMU_MSG(("si_pmu_res_uptime: rsrc %u uptime %u(deps 0x%08x uptime %u)\n",
	         rsrc, uptime, deps, dmax));

	return uptime + dmax + PMURES_UP_TRANSITION;
}

/* Return dependencies (direct or all/indirect) for the given resources */
static uint32
si_pmu_res_deps(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 rsrcs, bool all)
{
	uint32 deps = 0;
	uint32 i;

	for (i = 0; i <= PMURES_MAX_RESNUM; i ++) {
		if (!(rsrcs & PMURES_BIT(i)))
			continue;
		W_REG(osh, PMUREG(sih, res_table_sel), i);
		deps |= R_REG(osh, PMUREG(sih, res_dep_mask));
	}

	return !all ? deps : (deps ? (deps | si_pmu_res_deps(sih, osh, cc, deps, TRUE)) : 0);
}

/**
 * OTP is powered down/up as a means of resetting it, or for saving current when OTP is unused.
 * OTP is powered up/down through PMU resources.
 * OTP will turn OFF only if its not in the dependency of any "higher" rsrc in min_res_mask
 */
void
si_pmu_otp_power(si_t *sih, osl_t *osh, bool on, uint32* min_res_mask)
{
	chipcregs_t *cc;
	uint origidx;
	uint32 rsrcs = 0;	/* rsrcs to turn on/off OTP power */
	rsc_per_chip_t *rsc;	/* chip specific resource bit positions */

	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* Don't do anything if OTP is disabled */
	if (si_is_otp_disabled(sih)) {
		PMU_MSG(("si_pmu_otp_power: OTP is disabled\n"));
		return;
	}

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	/*
	 * OTP can't be power cycled by toggling OTP_PU for always on OTP chips. For now
	 * corerev 45 is the only one that has always on OTP.
	 * Instead, the chipc register OTPCtrl1 (Offset 0xF4) bit 25 (forceOTPpwrDis) is used.
	 * Please refer to http://hwnbu-twiki.broadcom.com/bin/view/Mwgroup/ChipcommonRev45
	 */
	if (sih->ccrev == 45) {
		uint32 otpctrl1;
		otpctrl1 = R_REG(osh,  &cc->otpcontrol1);
		if (on)
			otpctrl1 &= ~OTPC_FORCE_PWR_OFF;
		else
			otpctrl1 |= OTPC_FORCE_PWR_OFF;
		W_REG(osh,  &cc->otpcontrol1, otpctrl1);
		/* Return to original core */
		si_setcoreidx(sih, origidx);
		return;
	}

	switch (CHIPID(sih->chip)) {
	case BCM4322_CHIP_ID:
	case BCM43221_CHIP_ID:
	case BCM43231_CHIP_ID:
	case BCM4342_CHIP_ID:
	case BCM4325_CHIP_ID:
	case BCM4315_CHIP_ID:
	case BCM4329_CHIP_ID:
	case BCM4319_CHIP_ID:
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
	case BCM4330_CHIP_ID:
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
	case BCM43143_CHIP_ID:
	case BCM4334_CHIP_ID:
	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
	case BCM4335_CHIP_ID:
	case BCM4345_CHIP_ID:	/* same OTP PU as 4350 */
	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
	case BCM4360_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM4352_CHIP_ID:
	case BCM43526_CHIP_ID:
		rsc = si_pmu_get_rsc_positions(sih);
		rsrcs = PMURES_BIT(rsc->otp_pu);
		break;
	default:
		break;
	}

	if (rsrcs != 0) {
		uint32 otps;
		bool on_check = FALSE; /* Stores otp_ready state */
		uint32 min_mask = 0;

		/* Turn on/off the power */
		if (on) {
			min_mask = R_REG(osh, PMUREG(sih, min_res_mask));
			*min_res_mask = min_mask;

			min_mask |= rsrcs;
			min_mask |= si_pmu_res_deps(sih, osh, cc, min_mask, TRUE);
			on_check = TRUE;
			/* Assuming max rsc mask defines OTP_PU, so not programming max */
			PMU_MSG(("Adding rsrc 0x%x to min_res_mask\n", min_mask));
			W_REG(osh, PMUREG(sih, min_res_mask), min_mask);
			si_pmu_wait_for_steady_state(sih, osh, cc);
			OSL_DELAY(1000);
			SPINWAIT(!(R_REG(osh, PMUREG(sih, res_state)) & rsrcs),
				PMU_MAX_TRANSITION_DLY);
			ASSERT(R_REG(osh, PMUREG(sih, res_state)) & rsrcs);
		}
		else {
			/*
			 * Restore back the min_res_mask,
			 * but keep OTP powered off if allowed by dependencies
			 */
			if (*min_res_mask)
				min_mask = *min_res_mask;
			else
				min_mask = R_REG(osh, PMUREG(sih, min_res_mask));

			min_mask &= ~rsrcs;
			/*
			 * OTP rsrc can be cleared only if its not
			 * in the dependency of any "higher" rsrc in min_res_mask
			 */
			min_mask |= si_pmu_res_deps(sih, osh, cc, min_mask, TRUE);
			on_check = ((min_mask & rsrcs) != 0);

			PMU_MSG(("Removing rsrc 0x%x from min_res_mask\n", min_mask));
			W_REG(osh, PMUREG(sih, min_res_mask), min_mask);
			si_pmu_wait_for_steady_state(sih, osh, cc);
		}

		SPINWAIT((((otps = R_REG(osh, &cc->otpstatus)) & OTPS_READY) !=
			(on_check ? OTPS_READY : 0)), 3000);
		ASSERT((otps & OTPS_READY) == (on_check ? OTPS_READY : 0));
		if ((otps & OTPS_READY) != (on_check ? OTPS_READY : 0))
			PMU_MSG(("OTP ready bit not %s after wait\n", (on_check ? "ON" : "OFF")));
	}

	/* Return to original core */
	si_setcoreidx(sih, origidx);
} /* si_pmu_otp_power */

/** only called for SSN and LP phy's. */
void
si_pmu_rcal(si_t *sih, osl_t *osh)
{
	chipcregs_t *cc;
	uint origidx;
	uint rcal_done, BT_out_of_reset;

	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	switch (CHIPID(sih->chip)) {
	case BCM4325_CHIP_ID: {
		uint8 rcal_code;
		uint32 val;

		/* Kick RCal */
		W_REG(osh, PMUREG(sih, chipcontrol_addr), 1);

		/* Power Down RCAL Block */
		AND_REG(osh, PMUREG(sih, chipcontrol_data), ~0x04);

		/* Check if RCAL is already done by BT */
		rcal_done = ((R_REG(osh, &cc->chipstatus)) & 0x8) >> 3;

		/* If RCAL already done, note that BT is out of reset */
		if (rcal_done == 1) {
			BT_out_of_reset = 1;
		} else {
			BT_out_of_reset = 0;
		}

		/* Power Up RCAL block */
		OR_REG(osh, PMUREG(sih, chipcontrol_data), 0x04);

		/* Wait for completion */
		SPINWAIT(!(R_REG(osh, &cc->chipstatus) & 0x08), 10 * 1000 * 1000);
		ASSERT(R_REG(osh, &cc->chipstatus) & 0x08);

		if (BT_out_of_reset) {
			rcal_code = 0x6;
		} else {
			/* Drop the LSB to convert from 5 bit code to 4 bit code */
			rcal_code =  (uint8)(R_REG(osh, &cc->chipstatus) >> 5) & 0x0f;
		}

		PMU_MSG(("RCal completed, status 0x%x, code 0x%x\n",
			R_REG(osh, &cc->chipstatus), rcal_code));

		/* Write RCal code into pmu_vreg_ctrl[32:29] */
		W_REG(osh, PMUREG(sih, regcontrol_addr), 0);
		val = R_REG(osh, PMUREG(sih, regcontrol_data)) & ~((uint32)0x07 << 29);
		val |= (uint32)(rcal_code & 0x07) << 29;
		W_REG(osh, PMUREG(sih, regcontrol_data), val);
		W_REG(osh, PMUREG(sih, regcontrol_addr), 1);
		val = R_REG(osh, PMUREG(sih, regcontrol_data)) & ~(uint32)0x01;
		val |= (uint32)((rcal_code >> 3) & 0x01);
		W_REG(osh, PMUREG(sih, regcontrol_data), val);

		/* Write RCal code into pmu_chip_ctrl[33:30] */
		W_REG(osh, PMUREG(sih, chipcontrol_addr), 0);
		val = R_REG(osh, PMUREG(sih, chipcontrol_data)) & ~((uint32)0x03 << 30);
		val |= (uint32)(rcal_code & 0x03) << 30;
		W_REG(osh, PMUREG(sih, chipcontrol_data), val);
		W_REG(osh, PMUREG(sih, chipcontrol_addr), 1);
		val = R_REG(osh, PMUREG(sih, chipcontrol_data)) & ~(uint32)0x03;
		val |= (uint32)((rcal_code >> 2) & 0x03);
		W_REG(osh, PMUREG(sih, chipcontrol_data), val);

		/* Set override in pmu_chip_ctrl[29] */
		W_REG(osh, PMUREG(sih, chipcontrol_addr), 0);
		OR_REG(osh, PMUREG(sih, chipcontrol_data), (0x01 << 29));

		/* Power off RCal block */
		W_REG(osh, PMUREG(sih, chipcontrol_addr), 1);
		AND_REG(osh, PMUREG(sih, chipcontrol_data), ~0x04);

		break;
	}
	case BCM4329_CHIP_ID: {
		uint8 rcal_code;
		uint32 val;

		/* Kick RCal */
		W_REG(osh, PMUREG(sih, chipcontrol_addr), 1);

		/* Power Down RCAL Block */
		AND_REG(osh, PMUREG(sih, chipcontrol_data), ~0x04);

		/* Power Up RCAL block */
		OR_REG(osh, PMUREG(sih, chipcontrol_data), 0x04);

		/* Wait for completion */
		SPINWAIT(!(R_REG(osh, &cc->chipstatus) & 0x08), 10 * 1000 * 1000);
		ASSERT(R_REG(osh, &cc->chipstatus) & 0x08);

		/* Drop the LSB to convert from 5 bit code to 4 bit code */
		rcal_code =  (uint8)(R_REG(osh, &cc->chipstatus) >> 5) & 0x0f;

		PMU_MSG(("RCal completed, status 0x%x, code 0x%x\n",
			R_REG(osh, &cc->chipstatus), rcal_code));

		/* Write RCal code into pmu_vreg_ctrl[32:29] */
		W_REG(osh, PMUREG(sih, regcontrol_addr), 0);
		val = R_REG(osh, PMUREG(sih, regcontrol_data)) & ~((uint32)0x07 << 29);
		val |= (uint32)(rcal_code & 0x07) << 29;
		W_REG(osh, PMUREG(sih, regcontrol_data), val);
		W_REG(osh, PMUREG(sih, regcontrol_addr), 1);
		val = R_REG(osh, PMUREG(sih, regcontrol_data)) & ~(uint32)0x01;
		val |= (uint32)((rcal_code >> 3) & 0x01);
		W_REG(osh, PMUREG(sih, regcontrol_data), val);

		/* Write RCal code into pmu_chip_ctrl[33:30] */
		W_REG(osh, PMUREG(sih, chipcontrol_addr), 0);
		val = R_REG(osh, PMUREG(sih, chipcontrol_data)) & ~((uint32)0x03 << 30);
		val |= (uint32)(rcal_code & 0x03) << 30;
		W_REG(osh, PMUREG(sih, chipcontrol_data), val);
		W_REG(osh, PMUREG(sih, chipcontrol_addr), 1);
		val = R_REG(osh, PMUREG(sih, chipcontrol_data)) & ~(uint32)0x03;
		val |= (uint32)((rcal_code >> 2) & 0x03);
		W_REG(osh, PMUREG(sih, chipcontrol_data), val);

		/* Set override in pmu_chip_ctrl[29] */
		W_REG(osh, PMUREG(sih, chipcontrol_addr), 0);
		OR_REG(osh, PMUREG(sih, chipcontrol_data), (0x01 << 29));

		/* Power off RCal block */
		W_REG(osh, PMUREG(sih, chipcontrol_addr), 1);
		AND_REG(osh, PMUREG(sih, chipcontrol_data), ~0x04);

		break;
	}
	default:
		break;
	}

	/* Return to original core */
	si_setcoreidx(sih, origidx);
} /* si_pmu_rcal */

/** only called for HT, LCN and N phy's. */
void
si_pmu_spuravoid(si_t *sih, osl_t *osh, uint8 spuravoid)
{
	chipcregs_t *cc;
	uint origidx, intr_val;
	uint32 min_res_mask = 0, max_res_mask = 0, clk_ctl_st = 0;
	bool pll_off_on = FALSE;

	ASSERT(CHIPID(sih->chip) != BCM43143_CHIP_ID); /* LCN40 PHY */

#ifdef BCMUSBDEV_ENABLED
	if ((CHIPID(sih->chip) == BCM4324_CHIP_ID) && (CHIPREV(sih->chiprev) <= 2)) {
		return;
	}
	/* spuravoid is not ready for 43242 */
	if ((CHIPID(sih->chip) == BCM43242_CHIP_ID) || (CHIPID(sih->chip) == BCM43243_CHIP_ID)) {
		return;
	}
#endif

	if ((CHIPID(sih->chip) == BCM4336_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43362_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43239_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4314_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43142_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4334_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43242_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43243_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43341_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4335_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4345_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4330_CHIP_ID))
	{
		pll_off_on = TRUE;
	}

	/* Remember original core before switch to chipc */
	cc = (chipcregs_t *)si_switch_core(sih, CC_CORE_ID, &origidx, &intr_val);
	ASSERT(cc != NULL);

	/* force the HT off  */
	if (pll_off_on)
		si_pmu_pll_off(sih, osh, cc, &min_res_mask, &max_res_mask, &clk_ctl_st);

	/* update the pll changes */
	si_pmu_spuravoid_pllupdate(sih, cc, osh, spuravoid);

	/* enable HT back on  */
	if (pll_off_on)
		si_pmu_pll_on(sih, osh, cc, min_res_mask, max_res_mask, clk_ctl_st);

	/* Return to original core */
	si_restore_core(sih, origidx, intr_val);
} /* si_pmu_spuravoid */

/* below function are only for BBPLL parallel purpose */
/** only called for HT, LCN and N phy's. */
void
si_pmu_spuravoid_isdone(si_t *sih, osl_t *osh, uint32 min_res_mask,
uint32 max_res_mask, uint32 clk_ctl_st, uint8 spuravoid)
{

	chipcregs_t *cc;
	uint origidx, intr_val;
	bool pll_off_on = FALSE;

	ASSERT(CHIPID(sih->chip) != BCM43143_CHIP_ID); /* LCN40 PHY */

#ifdef BCMUSBDEV_ENABLED
	if ((CHIPID(sih->chip) == BCM4324_CHIP_ID) && (CHIPREV(sih->chiprev) <= 2)) {
		return;
	}
	/* spuravoid is not ready for 43242 */
	if ((CHIPID(sih->chip) == BCM43242_CHIP_ID) || (CHIPID(sih->chip) == BCM43243_CHIP_ID)) {
		return;
	}
#endif

	if ((CHIPID(sih->chip) == BCM4336_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43362_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43239_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4314_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43142_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4334_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43242_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43243_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4335_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4345_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4330_CHIP_ID))
	{
		pll_off_on = TRUE;
	}
	/* Remember original core before switch to chipc */
	cc = (chipcregs_t *)si_switch_core(sih, CC_CORE_ID, &origidx, &intr_val);
	ASSERT(cc != NULL);

	if (pll_off_on)
	  si_pmu_pll_off_isdone(sih, osh, cc);
	/* update the pll changes */
	si_pmu_spuravoid_pllupdate(sih, cc, osh, spuravoid);

	/* enable HT back on  */
	if (pll_off_on)
		si_pmu_pll_on(sih, osh, cc, min_res_mask, max_res_mask, clk_ctl_st);

	/* Return to original core */
	si_restore_core(sih, origidx, intr_val);
} /* si_pmu_spuravoid_isdone */

/* below function are only for BBPLL parallel purpose */

/* For having the pllcontrol data values for spuravoid */
typedef struct {
	uint8	spuravoid_mode;
	uint8	pllctrl_reg;
	uint32	pllctrl_regval;
} pllctrl_spuravoid_t;

/* LCNXN */
/* PLL Settings for spur avoidance on/off mode */
static const pllctrl_spuravoid_t spuravoid_4324[] = {
	{1, PMU1_PLL0_PLLCTL0, 0xA7400040},
	{1, PMU1_PLL0_PLLCTL1, 0x10080706},
	{1, PMU1_PLL0_PLLCTL2, 0x0D311408},
	{1, PMU1_PLL0_PLLCTL3, 0x804F66AC},
	{1, PMU1_PLL0_PLLCTL4, 0x02600004},
	{1, PMU1_PLL0_PLLCTL5, 0x00019AB1},

	{2, PMU1_PLL0_PLLCTL0, 0xA7400040},
	{2, PMU1_PLL0_PLLCTL1, 0x10080706},
	{2, PMU1_PLL0_PLLCTL2, 0x0D311408},
	{2, PMU1_PLL0_PLLCTL3, 0x80F3ADDC},
	{2, PMU1_PLL0_PLLCTL4, 0x02600004},
	{2, PMU1_PLL0_PLLCTL5, 0x00019AB1},

	{0, PMU1_PLL0_PLLCTL0, 0xA7400040},
	{0, PMU1_PLL0_PLLCTL1, 0x10080706},
	{0, PMU1_PLL0_PLLCTL2, 0x0CB11408},
	{0, PMU1_PLL0_PLLCTL3, 0x80AB1F7C},
	{0, PMU1_PLL0_PLLCTL4, 0x02600004},
	{0, PMU1_PLL0_PLLCTL5, 0x00019AB1}
};

static void
si_pmu_pllctrl_spurupdate(si_t *sih, osl_t *osh, chipcregs_t *cc, uint8 spuravoid,
	const pllctrl_spuravoid_t *pllctrl_spur, uint32 array_size)
{
	uint8 indx;
	for (indx = 0; indx < array_size; indx++) {
		if (pllctrl_spur[indx].spuravoid_mode == spuravoid) {
			W_REG(osh, PMUREG(sih, pllcontrol_addr), pllctrl_spur[indx].pllctrl_reg);
			W_REG(osh, PMUREG(sih, pllcontrol_data), pllctrl_spur[indx].pllctrl_regval);
		}
	}
}

static void
si_pmu_spuravoid_pllupdate(si_t *sih, chipcregs_t *cc, osl_t *osh, uint8 spuravoid)
{
	uint32 tmp = 0;
#ifdef BCMDBG_ERR
	char chn[8];
#endif
	uint32 xtal_freq, reg_val, mxdiv, ndiv_int, ndiv_frac_int, part_mul;
	uint8 p1_div, p2_div, FCLkx;
	const pmu1_xtaltab0_t *params_tbl;
	uint32 *pllctrl_addr = PMUREG(sih, pllcontrol_addr);
	uint32 *pllctrl_data = PMUREG(sih, pllcontrol_data);

	ASSERT(CHIPID(sih->chip) != BCM43143_CHIP_ID); /* LCN40 PHY */

	switch (CHIPID(sih->chip)) {
	case BCM4324_CHIP_ID:
		/* If we get an invalid spurmode, then set the spur0 settings. */
		if (spuravoid > 2)
			spuravoid = 0;

		si_pmu_pllctrl_spurupdate(sih, osh, cc, spuravoid, spuravoid_4324,
			ARRAYSIZE(spuravoid_4324));

		tmp = PCTL_PLL_PLLCTL_UPD;
		break;

	case BCM5357_CHIP_ID:   case BCM4749_CHIP_ID:
	case BCM43235_CHIP_ID:	case BCM43236_CHIP_ID:	case BCM43238_CHIP_ID:
	case BCM43237_CHIP_ID:
	case BCM43234_CHIP_ID:
	case BCM6362_CHIP_ID:
	case BCM53572_CHIP_ID:

		if  ((CHIPID(sih->chip) == BCM6362_CHIP_ID) && (sih->chiprev == 0)) {
			/* 6362a0 (same clks as 4322[4-6]) */
			if (spuravoid == 1) {
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL0);
				W_REG(osh, pllctrl_data, 0x11500010);
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL1);
				W_REG(osh, pllctrl_data, 0x000C0C06);
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2);
				W_REG(osh, pllctrl_data, 0x0F600a08);
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL3);
				W_REG(osh, pllctrl_data, 0x00000000);
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL4);
				W_REG(osh, pllctrl_data, 0x2001E920);
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL5);
				W_REG(osh, pllctrl_data, 0x88888815);
			} else {
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL0);
				W_REG(osh, pllctrl_data, 0x11100010);
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL1);
				W_REG(osh, pllctrl_data, 0x000c0c06);
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2);
				W_REG(osh, pllctrl_data, 0x03000a08);
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL3);
				W_REG(osh, pllctrl_data, 0x00000000);
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL4);
				W_REG(osh, pllctrl_data, 0x200005c0);
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL5);
				W_REG(osh, pllctrl_data, 0x88888815);
			}

		} else {
			/* BCM5357 needs to touch PLL1_PLLCTL[02], so offset PLL0_PLLCTL[02] by 6 */
			const uint8 phypll_offset = ((CHIPID(sih->chip) == BCM5357_CHIP_ID) ||
			                             (CHIPID(sih->chip) == BCM4749_CHIP_ID) ||
			                             (CHIPID(sih->chip) == BCM53572_CHIP_ID))
			                               ? 6 : 0;
			const uint8 bcm5357_bcm43236_p1div[] = {0x1, 0x5, 0x5};
			const uint8 bcm5357_bcm43236_ndiv[] = {0x30, 0xf6, 0xfc};

			/* 5357[ab]0, 43236[ab]0, and 6362b0 */
			if (spuravoid > 2)
				spuravoid = 0;

			/* RMW only the P1 divider */
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL0 + phypll_offset);
			tmp = R_REG(osh, pllctrl_data);
			tmp &= (~(PMU1_PLL0_PC0_P1DIV_MASK));
			tmp |= (bcm5357_bcm43236_p1div[spuravoid] << PMU1_PLL0_PC0_P1DIV_SHIFT);
			W_REG(osh, pllctrl_data, tmp);

			/* RMW only the int feedback divider */
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2 + phypll_offset);
			tmp = R_REG(osh, pllctrl_data);
			tmp &= ~(PMU1_PLL0_PC2_NDIV_INT_MASK);
			tmp |= (bcm5357_bcm43236_ndiv[spuravoid]) << PMU1_PLL0_PC2_NDIV_INT_SHIFT;
			W_REG(osh, pllctrl_data, tmp);
		}

		tmp = 1 << 10;
		break;

	case BCM4331_CHIP_ID:
	case BCM43431_CHIP_ID:
		if (ISSIM_ENAB(sih)) {
			if (spuravoid == 2) {
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL3);
				W_REG(osh, pllctrl_data, 0x00000002);
			} else if (spuravoid == 1) {
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL3);
				W_REG(osh, pllctrl_data, 0x00000001);
			} else {
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL3);
				W_REG(osh, pllctrl_data, 0x00000000);
			}
		} else {
			if (spuravoid == 2) {
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL0);
				W_REG(osh, pllctrl_data, 0x11500014);
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2);
				W_REG(osh, pllctrl_data, 0x0FC00a08);
			} else if (spuravoid == 1) {
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL0);
				W_REG(osh, pllctrl_data, 0x11500014);
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2);
				W_REG(osh, pllctrl_data, 0x0F600a08);
			} else {
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL0);
				W_REG(osh, pllctrl_data, 0x11100014);
				W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2);
				W_REG(osh, pllctrl_data, 0x03000a08);
			}
		}
		tmp = 1 << 10;
		break;

	case BCM43224_CHIP_ID:	case BCM43225_CHIP_ID:	case BCM43421_CHIP_ID:
	case BCM43226_CHIP_ID:
		if (spuravoid == 1) {
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL0);
			W_REG(osh, pllctrl_data, 0x11500010);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL1);
			W_REG(osh, pllctrl_data, 0x000C0C06);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, pllctrl_data, 0x0F600a08);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, pllctrl_data, 0x00000000);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL4);
			W_REG(osh, pllctrl_data, 0x2001E920);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL5);
			W_REG(osh, pllctrl_data, 0x88888815);
		} else {
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL0);
			W_REG(osh, pllctrl_data, 0x11100010);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL1);
			W_REG(osh, pllctrl_data, 0x000c0c06);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, pllctrl_data, 0x03000a08);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, pllctrl_data, 0x00000000);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL4);
			W_REG(osh, pllctrl_data, 0x200005c0);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL5);
			W_REG(osh, pllctrl_data, 0x88888815);
		}
		tmp = 1 << 10;
		break;

	case BCM43222_CHIP_ID:	case BCM43111_CHIP_ID:	case BCM43112_CHIP_ID:
	case BCM43420_CHIP_ID:
		if (spuravoid == 1) {
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL0);
			W_REG(osh, pllctrl_data, 0x11500008);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL1);
			W_REG(osh, pllctrl_data, 0x0C000C06);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, pllctrl_data, 0x0F600a08);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, pllctrl_data, 0x00000000);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL4);
			W_REG(osh, pllctrl_data, 0x2001E920);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL5);
			W_REG(osh, pllctrl_data, 0x88888815);
		} else {
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL0);
			W_REG(osh, pllctrl_data, 0x11100008);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL1);
			W_REG(osh, pllctrl_data, 0x0c000c06);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, pllctrl_data, 0x03000a08);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, pllctrl_data, 0x00000000);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL4);
			W_REG(osh, pllctrl_data, 0x200005c0);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL5);
			W_REG(osh, pllctrl_data, 0x88888855);
		}

		tmp = 1 << 10;
		break;

	case BCM4716_CHIP_ID:
	case BCM4748_CHIP_ID:
	case BCM47162_CHIP_ID:
		if (spuravoid == 1) {
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL0);
			W_REG(osh, pllctrl_data, 0x11500060);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL1);
			W_REG(osh, pllctrl_data, 0x080C0C06);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, pllctrl_data, 0x0F600000);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, pllctrl_data, 0x00000000);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL4);
			W_REG(osh, pllctrl_data, 0x2001E924);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL5);
			W_REG(osh, pllctrl_data, 0x88888815);
		} else {
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL0);
			W_REG(osh, pllctrl_data, 0x11100060);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL1);
			W_REG(osh, pllctrl_data, 0x080c0c06);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, pllctrl_data, 0x03000000);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, pllctrl_data, 0x00000000);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL4);
			W_REG(osh, pllctrl_data, 0x200005c0);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL5);
			W_REG(osh, pllctrl_data, 0x88888815);
		}

		tmp = 3 << 9;
		break;

	case BCM4322_CHIP_ID:
	case BCM43221_CHIP_ID:
	case BCM43231_CHIP_ID:
	case BCM4342_CHIP_ID:
		W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL0);
		W_REG(osh, pllctrl_data, 0x11100070);
		W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL1);
		W_REG(osh, pllctrl_data, 0x1014140a);
		W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL5);
		W_REG(osh, pllctrl_data, 0x88888854);

		if (spuravoid == 1) { /* spur_avoid ON, enable 41/82/164Mhz clock mode */
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, pllctrl_data, 0x05201828);
		} else { /* enable 40/80/160Mhz clock mode */
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, pllctrl_data, 0x05001828);
		}

		tmp = 1 << 10;
		break;
	case BCM4319_CHIP_ID:
		W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL0);
		W_REG(osh, pllctrl_data, 0x11100070);
		W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL1);
		W_REG(osh, pllctrl_data, 0x1014140a);
		W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL5);
		W_REG(osh, pllctrl_data, 0x88888854);

		if (spuravoid == 1) { /* spur_avoid ON, enable 41/82/164Mhz clock mode */
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, pllctrl_data, 0x05201828);
		} else { /* enable 40/80/160Mhz clock mode */
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, pllctrl_data, 0x05001828);
		}
		break;
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
	case BCM4330_CHIP_ID:
		xtal_freq = si_alp_clock(sih)/1000;
		/* Find the frequency in the table */
		for (params_tbl = si_pmu1_xtaltab0(sih);
			params_tbl != NULL && params_tbl->xf != 0; params_tbl++)
			if ((params_tbl->fref) == xtal_freq)
				break;
		/* Could not find it so assign a default value */
		if (params_tbl == NULL || params_tbl->xf == 0)
			params_tbl = si_pmu1_xtaldef0(sih);
		ASSERT(params_tbl != NULL && params_tbl->xf != 0);

		/* FClkx = (P2/P1) * ((NDIV_INT + NDIV_FRAC/2^24)/MXDIV) * Fref
		    Fref  = XtalFreq
		    FCLkx = 82MHz for spur avoidance mode
				   80MHz for non-spur avoidance mode.
		*/
		xtal_freq = (uint32) params_tbl->fref/100;
		p1_div = params_tbl->p1div;
		p2_div = params_tbl->p2div;
		W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL1);
		reg_val =  R_REG(osh, pllctrl_data);
		mxdiv = (reg_val >> PMU1_PLL0_PC2_M6DIV_SHIFT) & PMU1_PLL0_PC2_M5DIV_MASK;

		if (spuravoid == 1)
			FCLkx = 82;
		else
			FCLkx = 80;

		part_mul = (p1_div / p2_div) * mxdiv;
		ndiv_int = (FCLkx * part_mul * 10)/ (xtal_freq);
		ndiv_frac_int = ((FCLkx * part_mul * 10) % (xtal_freq));
		ndiv_frac_int = ((ndiv_frac_int * 16777216) + (xtal_freq/2)) / (xtal_freq);

		W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2);
		reg_val =  R_REG(osh, pllctrl_data);
		W_REG(osh, pllctrl_data, ((reg_val & ~PMU1_PLL0_PC2_NDIV_INT_MASK)
					| (ndiv_int << PMU1_PLL0_PC2_NDIV_INT_SHIFT)));
		W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL3);
		W_REG(osh, pllctrl_data, ndiv_frac_int);

		tmp = PCTL_PLL_PLLCTL_UPD;
		break;

	case BCM43131_CHIP_ID:
	case BCM43217_CHIP_ID:
	case BCM43227_CHIP_ID:
	case BCM43228_CHIP_ID:
	case BCM43428_CHIP_ID:
		/* LCNXN */
		/* PLL Settings for spur avoidance on/off mode, no on2 support for 43228A0 */
		if (spuravoid == 1) {
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL0);
			W_REG(osh, pllctrl_data, 0x01100014);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL1);
			W_REG(osh, pllctrl_data, 0x040C0C06);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, pllctrl_data, 0x03140A08);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, pllctrl_data, 0x00333333);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL4);
			W_REG(osh, pllctrl_data, 0x202C2820);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL5);
			W_REG(osh, pllctrl_data, 0x88888815);
		} else {
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL0);
			W_REG(osh, pllctrl_data, 0x11100014);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL1);
			W_REG(osh, pllctrl_data, 0x040c0c06);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, pllctrl_data, 0x03000a08);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, pllctrl_data, 0x00000000);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL4);
			W_REG(osh, pllctrl_data, 0x200005c0);
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL5);
			W_REG(osh, pllctrl_data, 0x88888815);
		}
		tmp = 1 << 10;
		break;
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
	case BCM43341_CHIP_ID:
	case BCM4334_CHIP_ID:
		{
			const pmu2_xtaltab0_t *xt;
			uint32 pll0;

			xtal_freq = si_pmu_alp_clock(sih, osh)/1000;
			xt = (spuravoid == 1) ? pmu2_xtaltab0_adfll_492 : pmu2_xtaltab0_adfll_485;

			for (; xt != NULL && xt->fref != 0; xt++) {
				if (xt->fref == xtal_freq) {
					W_REG(osh, pllctrl_addr, PMU15_PLL_PLLCTL0);
					pll0 = R_REG(osh, pllctrl_data);
					pll0 &= ~PMU15_PLL_PC0_FREQTGT_MASK;
					pll0 |= (xt->freq_tgt << PMU15_PLL_PC0_FREQTGT_SHIFT);
					W_REG(osh, pllctrl_data, pll0);

					tmp = PCTL_PLL_PLLCTL_UPD;
					break;
				}
			}
		}
		break;
	case BCM4335_CHIP_ID:
		/* 4335 PLL ctrl Registers */
		/* PLL Settings for spur avoidance on/off mode,  support for 4335A0 */
		/* # spur_mode 0 VCO=963MHz
		# spur_mode 1 VCO=960MHz
		# spur_mode 2 VCO=961MHz
		# spur_mode 3 VCO=964MHz
		# spur_mode 4 VCO=962MHz
		# spur_mode 5 VCO=965MHz
		# spur_mode 6 VCO=966MHz
		# spur_mode 7 VCO=967MHz
		# spur_mode 8 VCO=968MHz
		# spur_mode 9 VCO=969MHz
		*/
		{
			uint32 plldata43162[10] = {
				/* 40MHz xtal */
				0x80133333, 0x81000000, 0x80066666, 0x8019999A, 0x800CCCCD,
				0x80200000, 0x80266666, 0x802CCCCD, 0x80333333, 0x80E8BA2F
			};
			uint32 plldata[10] = {
				0x80BFA863, 0x80AB1F7D, 0x80b1f7c9, 0x80c680af, 0x80B8D016,
				0x80CD58FC, 0x80D43149, 0x80DB0995, 0x80E1E1E2, 0x80E8BA2F
			};
			uint32 *datap = NULL;
			W_REG(osh, pllctrl_addr, PMU1_PLL0_PLLCTL3);
			/* WAR for 43162 FCBGA PKG */
			if (sih->chippkg == BCM4335_FCBGA_PKG_ID)
				datap = plldata43162;
			else
				datap = plldata;
			if (spuravoid < 10)
				W_REG(osh, pllctrl_data, datap[spuravoid]);
			else
				PMU_ERROR(("Wrong spuravoidance settings %d\n", spuravoid));
		}
		tmp = PCTL_PLL_PLLCTL_UPD;
		break;
	case BCM4345_CHIP_ID:
		/* 4345 PLL ctrl Registers */
		xtal_freq = si_pmu_alp_clock(sih, osh) / 1000;

		/* TODO - set PLLCTL registers (via set_PLL_control_regs), based on spuravoid */

		break;
	default:
		PMU_ERROR(("%s: unknown spuravoidance settings for chip %s, not changing PLL\n",
		           __FUNCTION__, bcm_chipname(sih->chip, chn, 8)));
		break;
	}

	tmp |= R_REG(osh, PMUREG(sih, pmucontrol));
	W_REG(osh, PMUREG(sih, pmucontrol), tmp);
} /* si_pmu_spuravoid_pllupdate */

extern uint32
si_pmu_cal_fvco(si_t *sih, osl_t *osh)
{
	uint32 xf, ndiv_int, ndiv_frac, fvco, pll_reg, p1_div_scale;
	uint32 r_high, r_low, int_part, frac_part, rounding_const;
	uint8 p1_div;
	chipcregs_t *cc;
	uint origidx, intr_val;

	/* Remember original core before switch to chipc */
	cc = (chipcregs_t *)si_switch_core(sih, CC_CORE_ID, &origidx, &intr_val);
	ASSERT(cc != NULL);
	BCM_REFERENCE(cc);

	xf = si_pmu_alp_clock(sih, osh)/1000;

	pll_reg = si_pmu_pllcontrol(sih, PMU1_PLL0_PLLCTL2, 0, 0);

	p1_div = (pll_reg & PMU4335_PLL0_PC2_P1DIV_MASK) >>
			PMU4335_PLL0_PC2_P1DIV_SHIFT;

	ndiv_int = (pll_reg & PMU4335_PLL0_PC2_NDIV_INT_MASK) >>
			PMU4335_PLL0_PC2_NDIV_INT_SHIFT;

	pll_reg = si_pmu_pllcontrol(sih, PMU1_PLL0_PLLCTL3, 0, 0);

	ndiv_frac = (pll_reg & PMU1_PLL0_PC3_NDIV_FRAC_MASK) >>
			PMU1_PLL0_PC3_NDIV_FRAC_SHIFT;

	/* Actual expression is as below */
	/* fvco1 = (100 * (xf * 1/p1_div) * (ndiv_int + (ndiv_frac * 1/(1 << 24)))) */
	/* * 1/(1000 * 100); */

	/* Representing 1/p1_div as a 12 bit number */
	/* Reason for the choice of 12: */
	/* ndiv_int is represented by 9 bits */
	/* so (ndiv_int << 24) needs 33 bits */
	/* xf needs 16 bits for the worst case of 52MHz clock */
	/* So (xf * (ndiv << 24)) needs atleast 49 bits */
	/* So remaining bits for uint64 : 64 - 49 = 15 bits */
	/* So, choosing 12 bits, with 3 bits of headroom */
	int_part = xf * ndiv_int;

	rounding_const = 1 << (BBPLL_NDIV_FRAC_BITS - 1);
	bcm_uint64_multiple_add(&r_high, &r_low, ndiv_frac, xf, rounding_const);
	bcm_uint64_right_shift(&frac_part, r_high, r_low, BBPLL_NDIV_FRAC_BITS);

	p1_div_scale = (1 << P1_DIV_SCALE_BITS) / p1_div;
	rounding_const = 1 << (P1_DIV_SCALE_BITS - 1);

	bcm_uint64_multiple_add(&r_high, &r_low, (int_part + frac_part),
		p1_div_scale, rounding_const);
	bcm_uint64_right_shift(&fvco, r_high, r_low, P1_DIV_SCALE_BITS);

	/* Return to original core */
	si_restore_core(sih, origidx, intr_val);
	return fvco;
} /* si_pmu_cal_fvco */

/** Only called by N (MIMO) PHY */
void
si_pmu_gband_spurwar(si_t *sih, osl_t *osh)
{
	chipcregs_t *cc;
	uint origidx, intr_val;
	uint32 cc_clk_ctl_st;
	uint32 minmask, maxmask;

	if ((CHIPID(sih->chip) == BCM43222_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43420_CHIP_ID)) {
		/* Remember original core before switch to chipc */
		cc = (chipcregs_t *)si_switch_core(sih, CC_CORE_ID, &origidx, &intr_val);
		ASSERT(cc != NULL);

		/* Remove force HT and HT Avail Request from chipc core */
		cc_clk_ctl_st = R_REG(osh, &cc->clk_ctl_st);
		AND_REG(osh, &cc->clk_ctl_st, ~(CCS_FORCEHT | CCS_HTAREQ));

		minmask = R_REG(osh, PMUREG(sih, min_res_mask));
		maxmask = R_REG(osh, PMUREG(sih, max_res_mask));

		/* Make sure the PLL is off: clear bit 4 & 5 of min/max_res_mask */
		/* Have to remove HT Avail request before powering off PLL */
		AND_REG(osh, PMUREG(sih, min_res_mask),	~(PMURES_BIT(RES4322_HT_SI_AVAIL)));
		AND_REG(osh, PMUREG(sih, max_res_mask),	~(PMURES_BIT(RES4322_HT_SI_AVAIL)));
		SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL, PMU_MAX_TRANSITION_DLY);
		AND_REG(osh, PMUREG(sih, min_res_mask),	~(PMURES_BIT(RES4322_SI_PLL_ON)));
		AND_REG(osh, PMUREG(sih, max_res_mask),	~(PMURES_BIT(RES4322_SI_PLL_ON)));
		OSL_DELAY(150);
		ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));

		/* Change backplane clock speed from 96 MHz to 80 MHz */
		W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU2_PLL_PLLCTL2);
		W_REG(osh, PMUREG(sih, pllcontrol_data), (R_REG(osh, PMUREG(sih, pllcontrol_data)) &
		                                  ~(PMU2_PLL_PC2_M6DIV_MASK)) |
		      (0xc << PMU2_PLL_PC2_M6DIV_SHIFT));

		/* Reduce the driver strengths of the phyclk160, adcclk80, and phyck80
		 * clocks from 0x8 to 0x1
		 */
		W_REG(osh, PMUREG(sih, pllcontrol_addr), PMU2_PLL_PLLCTL5);
		W_REG(osh, PMUREG(sih, pllcontrol_data), (R_REG(osh, PMUREG(sih, pllcontrol_data)) &
		                                  ~(PMU2_PLL_PC5_CLKDRIVE_CH1_MASK |
		                                    PMU2_PLL_PC5_CLKDRIVE_CH2_MASK |
		                                    PMU2_PLL_PC5_CLKDRIVE_CH3_MASK |
		                                    PMU2_PLL_PC5_CLKDRIVE_CH4_MASK)) |
		      ((1 << PMU2_PLL_PC5_CLKDRIVE_CH1_SHIFT) |
		       (1 << PMU2_PLL_PC5_CLKDRIVE_CH2_SHIFT) |
		       (1 << PMU2_PLL_PC5_CLKDRIVE_CH3_SHIFT) |
		       (1 << PMU2_PLL_PC5_CLKDRIVE_CH4_SHIFT)));

		W_REG(osh, PMUREG(sih, pmucontrol), R_REG(osh, PMUREG(sih, pmucontrol)) |
			PCTL_PLL_PLLCTL_UPD);

		/* Restore min_res_mask and max_res_mask */
		OSL_DELAY(100);
		W_REG(osh, PMUREG(sih, max_res_mask), maxmask);
		OSL_DELAY(100);
		W_REG(osh, PMUREG(sih, min_res_mask), minmask);
		OSL_DELAY(100);
		/* Make sure the PLL is on. Spinwait until the HTAvail is True */
		SPINWAIT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL), PMU_MAX_TRANSITION_DLY);
		ASSERT((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));

		/* Restore force HT and HT Avail Request on the chipc core */
		W_REG(osh, &cc->clk_ctl_st, cc_clk_ctl_st);

		/* Return to original core */
		si_restore_core(sih, origidx, intr_val);
	}
} /* si_pmu_gband_spurwar */

bool
si_pmu_is_otp_powered(si_t *sih, osl_t *osh)
{
	uint idx;
	chipcregs_t *cc;
	bool st;
	rsc_per_chip_t *rsc;		/* chip specific resource bit positions */

	/* Remember original core before switch to chipc */
	idx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	si_pmu_wait_for_steady_state(sih, osh, cc);

	switch (CHIPID(sih->chip)) {
	case BCM4322_CHIP_ID:
	case BCM43221_CHIP_ID:
	case BCM43231_CHIP_ID:
	case BCM4342_CHIP_ID:
	case BCM4325_CHIP_ID:
	case BCM4329_CHIP_ID:
	case BCM4315_CHIP_ID:
	case BCM4319_CHIP_ID:
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
	case BCM43239_CHIP_ID:
	case BCM4330_CHIP_ID:
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
	case BCM43143_CHIP_ID:
	case BCM43341_CHIP_ID:
	case BCM4334_CHIP_ID:
	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
	case BCM4335_CHIP_ID:
	case BCM4345_CHIP_ID:	/* same OTP PU as 4350 */
	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
	case BCM4360_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM43526_CHIP_ID:
	case BCM4352_CHIP_ID:
		rsc = si_pmu_get_rsc_positions(sih);
		st = (R_REG(osh, PMUREG(sih, res_state)) & PMURES_BIT(rsc->otp_pu)) != 0;
		break;

	/* These chip doesn't use PMU bit to power up/down OTP. OTP always on.
	 * Use OTP_INIT command to reset/refresh state.
	 */
	case BCM43222_CHIP_ID:	case BCM43111_CHIP_ID:	case BCM43112_CHIP_ID:
	case BCM43224_CHIP_ID:	case BCM43225_CHIP_ID:	case BCM43421_CHIP_ID:
	case BCM43236_CHIP_ID:	case BCM43235_CHIP_ID:	case BCM43238_CHIP_ID:
	case BCM43237_CHIP_ID:  case BCM43420_CHIP_ID:
	case BCM43234_CHIP_ID:
	case BCM4331_CHIP_ID:   case BCM43431_CHIP_ID:
	case BCM43602_CHIP_ID:
	case BCM43462_CHIP_ID:
		st = TRUE;
		break;
	default:
		st = TRUE;
		break;
	}

	/* Return to original core */
	si_setcoreidx(sih, idx);
	return st;
} /* si_pmu_is_otp_powered */

void
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_ERR)
si_pmu_sprom_enable(si_t *sih, osl_t *osh, bool enable)
#else
BCMATTACHFN(si_pmu_sprom_enable)(si_t *sih, osl_t *osh, bool enable)
#endif
{
	switch (CHIPID(sih->chip)) {
	case BCM4315_CHIP_ID:
		if (CHIPREV(sih->chiprev) < 1)
			break;
		if (sih->chipst & CST4315_SPROM_SEL) {
			uint32 val;
			W_REG(osh, PMUREG(sih, chipcontrol_addr), 0);
			val = R_REG(osh, PMUREG(sih, chipcontrol_data));
			if (enable)
				val &= ~0x80000000;
			else
				val |= 0x80000000;
			W_REG(osh, PMUREG(sih, chipcontrol_data), val);
		}
		break;
	default:
		break;
	}
}

bool
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_ERR)
si_pmu_is_sprom_enabled(si_t *sih, osl_t *osh)
#else
BCMATTACHFN(si_pmu_is_sprom_enabled)(si_t *sih, osl_t *osh)
#endif
{
	bool enable = TRUE;

	switch (CHIPID(sih->chip)) {
	case BCM4315_CHIP_ID:
		if (CHIPREV(sih->chiprev) < 1)
			break;
		if (!(sih->chipst & CST4315_SPROM_SEL))
			break;
		W_REG(osh, PMUREG(sih, chipcontrol_addr), 0);
		if (R_REG(osh, PMUREG(sih, chipcontrol_data)) & 0x80000000)
			enable = FALSE;
		break;
	default:
		break;
	}
	return enable;
}

/**
 * Some chip/boards can be optionally fitted with an external 32Khz clock source for increased power
 * savings (due to more accurate sleep intervals).
 */
static void
BCMATTACHFN(si_pmu_set_lpoclk)(si_t *sih, osl_t *osh)
{
	uint32 ext_lpo_sel, int_lpo_sel, timeout = 0,
		ext_lpo_avail = 0, lpo_sel = 0;
	uint32 ext_lpo_isclock; /* On e.g. 43602a0, either x-tal or clock can be on LPO pins */

	if (!(getintvar(NULL, "boardflags3")))
		return;

	ext_lpo_sel = getintvar(NULL, "boardflags3") & BFL3_FORCE_EXT_LPO_SEL;
	int_lpo_sel = getintvar(NULL, "boardflags3") & BFL3_FORCE_INT_LPO_SEL;
	ext_lpo_isclock = getintvar(NULL, "boardflags3") & BFL3_EXT_LPO_ISCLOCK;

	BCM_REFERENCE(ext_lpo_isclock);

	if (ext_lpo_sel != 0) {
		switch (CHIPID(sih->chip)) {
		case BCM43602_CHIP_ID:
		case BCM43462_CHIP_ID:
			/* External LPO is POR default enabled */
			si_pmu_regcontrol(sih, CHIPCTRLREG2, PMU43602_CC2_XTAL32_SEL,
				ext_lpo_isclock ? 0 : PMU43602_CC2_XTAL32_SEL);
			break;
		default:
			/* Force External LPO Power Up */
			si_pmu_chipcontrol(sih, CHIPCTRLREG0, CC_EXT_LPO_PU, CC_EXT_LPO_PU);
			si_gci_chipcontrol(sih, CHIPCTRLREG6, GC_EXT_LPO_PU, GC_EXT_LPO_PU);
			break;
		}

		ext_lpo_avail = R_REG(osh, PMUREG(sih, pmustatus)) & EXT_LPO_AVAIL;
		while (ext_lpo_avail == 0 && timeout < LPO_SEL_TIMEOUT) {
			OSL_DELAY(1000);
			ext_lpo_avail = R_REG(osh, PMUREG(sih, pmustatus)) & EXT_LPO_AVAIL;
			timeout++;
		}

		if (timeout >= LPO_SEL_TIMEOUT) {
			PMU_ERROR(("External LPO is not available\n"));
		} else {
			/* External LPO is available, lets use (=select) it */
			OSL_DELAY(1000);
			timeout = 0;

			switch (CHIPID(sih->chip)) {
			case BCM43602_CHIP_ID:
			case BCM43462_CHIP_ID:
				si_pmu_regcontrol(sih, CHIPCTRLREG2, PMU43602_CC2_FORCE_EXT_LPO,
					PMU43602_CC2_FORCE_EXT_LPO); /* switches to external LPO */
				break;
			default:
				/* Force External LPO Sel up */
				si_gci_chipcontrol(sih, CHIPCTRLREG6, EXT_LPO_SEL, EXT_LPO_SEL);
				/* Clear Force Internal LPO Sel */
				si_gci_chipcontrol(sih, CHIPCTRLREG6, INT_LPO_SEL, 0x0);
				OSL_DELAY(1000);

				lpo_sel = R_REG(osh, PMUREG(sih, pmucontrol)) & LPO_SEL;
				while (lpo_sel != 0 && timeout < LPO_SEL_TIMEOUT) {
					OSL_DELAY(1000);
					lpo_sel = R_REG(osh, PMUREG(sih, pmucontrol)) & LPO_SEL;
					timeout++;
				}
			}

			if (timeout >= LPO_SEL_TIMEOUT) {
				PMU_ERROR(("External LPO is not set\n"));
				/* Clear Force External LPO Sel */
				switch (CHIPID(sih->chip)) {
				case BCM43602_CHIP_ID:
				case BCM43462_CHIP_ID:
					si_pmu_regcontrol(sih, CHIPCTRLREG2,
						PMU43602_CC2_FORCE_EXT_LPO, 0);
					break;
				default:
					si_gci_chipcontrol(sih, CHIPCTRLREG6, EXT_LPO_SEL, 0x0);
					break;
				}
			} else {
				/* Clear Force Internal LPO Power Up */
				switch (CHIPID(sih->chip)) {
				case BCM43602_CHIP_ID:
				case BCM43462_CHIP_ID:
					break;
				default:
					si_pmu_chipcontrol(sih, CHIPCTRLREG0, CC_INT_LPO_PU, 0x0);
					si_gci_chipcontrol(sih, CHIPCTRLREG6, GC_INT_LPO_PU, 0x0);
					break;
				}
			} /* if (timeout) */
		} /* if (timeout) */
	} else if (int_lpo_sel != 0) {
		switch (CHIPID(sih->chip)) {
		case BCM43602_CHIP_ID:
		case BCM43462_CHIP_ID:
			break; /* do nothing, internal LPO is POR default powered and selected */
		default:
			/* Force Internal LPO Power Up */
			si_pmu_chipcontrol(sih, CHIPCTRLREG0, CC_INT_LPO_PU, CC_INT_LPO_PU);
			si_gci_chipcontrol(sih, CHIPCTRLREG6, GC_INT_LPO_PU, GC_INT_LPO_PU);

			OSL_DELAY(1000);

			/* Force Internal LPO Sel up */
			si_gci_chipcontrol(sih, CHIPCTRLREG6, INT_LPO_SEL, INT_LPO_SEL);
			/* Clear Force External LPO Sel */
			si_gci_chipcontrol(sih, CHIPCTRLREG6, EXT_LPO_SEL, 0x0);

			OSL_DELAY(1000);

			lpo_sel = R_REG(osh, PMUREG(sih, pmucontrol)) & LPO_SEL;
			timeout = 0;
			while (lpo_sel == 0 && timeout < LPO_SEL_TIMEOUT) {
				OSL_DELAY(1000);
				lpo_sel = R_REG(osh, PMUREG(sih, pmucontrol)) & LPO_SEL;
				timeout++;
			}
			if (timeout >= LPO_SEL_TIMEOUT) {
				PMU_ERROR(("Internal LPO is not set\n"));
				/* Clear Force Internal LPO Sel */
				si_gci_chipcontrol(sih, CHIPCTRLREG6, INT_LPO_SEL, 0x0);
			} else {
				/* Clear Force External LPO Power Up */
				si_pmu_chipcontrol(sih, CHIPCTRLREG0, CC_EXT_LPO_PU, 0x0);
				si_gci_chipcontrol(sih, CHIPCTRLREG6, GC_EXT_LPO_PU, 0x0);
			}
			break;
		}
	}
} /* si_pmu_set_lpoclk */

/** initialize PMU chip controls and other chip level stuff */
void
BCMATTACHFN(si_pmu_chip_init)(si_t *sih, osl_t *osh)
{
	ASSERT(sih->cccaps & CC_CAP_PMU);

	si_pmu_otp_chipcontrol(sih, osh);

#ifdef CHIPC_UART_ALWAYS_ON
	si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, clk_ctl_st), CCS_FORCEALP, CCS_FORCEALP);
#endif /* CHIPC_UART_ALWAYS_ON */

#ifndef CONFIG_XIP
	/* Gate off SPROM clock and chip select signals */
	si_pmu_sprom_enable(sih, osh, FALSE);
#endif

	/* Misc. chip control, has nothing to do with PMU */
	switch (CHIPID(sih->chip)) {
	case BCM4315_CHIP_ID:
#ifdef BCMUSBDEV
		si_setcore(sih, PCMCIA_CORE_ID, 0);
		si_core_disable(sih, 0);
#endif
		break;

	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
	{
#if defined(BCM4324A0) || defined(BCM4324A1)
		si_pmu_chipcontrol(sih, PMU_CHIPCTL1,
			(PMU4324_CC1_GPIO_CONF(-1) | PMU4324_CC1_ENABLE_UART),
			(PMU4324_CC1_GPIO_CONF(0) | PMU4324_CC1_ENABLE_UART));
		si_pmu_chipcontrol(sih, PMU_CHIPCTL2, PMU4324_CC2_SDIO_AOS_EN,
			PMU4324_CC2_SDIO_AOS_EN);
#endif

#ifdef BCM4324B0
		si_pmu_chipcontrol(sih, PMU_CHIPCTL1,
		PMU4324_CC1_ENABLE_UART,
		0);
		si_pmu_chipcontrol(sih, PMU_CHIPCTL2,
		PMU4324_CC2_SDIO_AOS_EN,
		PMU4324_CC2_SDIO_AOS_EN);
#endif

#if defined(BCM4324A1) || defined(BCM4324B0)

		/* 4324 iPA Board hence muxing out PALDO_PU signal to O/p pin &
		 * muxing out actual RF_SW_CTRL7-6 signals to O/p pin
		 */

		si_pmu_chipcontrol(sih, 4, 0x1f0000, 0x140000);
#endif
		/*
		 * Setting the pin mux to enable the GPIOs required for HSIC-OOB signals.
		 */
#if defined(BCM4324B1)
		si_pmu_chipcontrol(sih, PMU_CHIPCTL1, PMU4324_CC1_GPIO_CONF_MASK, 0x2);
#endif
		break;
	}

	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
	{
		uint32 mask, val = 0;
		uint16 clkreq_conf;
		mask = (1 << PMU_CC1_CLKREQ_TYPE_SHIFT);

		clkreq_conf = (uint16)getintvar(NULL, rstr_clkreq_conf);

		if (clkreq_conf & CLKREQ_TYPE_CONFIG_PUSHPULL)
			val =  (1 << PMU_CC1_CLKREQ_TYPE_SHIFT);

		si_pmu_chipcontrol(sih, PMU_CHIPCTL0, mask, val);
		break;
	}

#ifdef BCMQT
	case BCM4314_CHIP_ID:
	{
		uint32 tmp;


		W_REG(osh, PMUREG(sih, chipcontrol_addr), PMU_CHIPCTL3);
		tmp = R_REG(osh, PMUREG(sih, chipcontrol_data));
		tmp &= ~(1 << PMU_CC3_ENABLE_RF_SHIFT);
		tmp |= (1 << PMU_CC3_RF_DISABLE_IVALUE_SHIFT);
		W_REG(osh, PMUREG(sih, chipcontrol_data), tmp);
		break;
	}
#endif /* BCMQT */

	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
	{
		uint32 val;

		if (CST4350_IFC_MODE(sih->chipst) == CST4350_IFC_MODE_PCIE) {
			/* JIRA: SWWLAN-27305 initialize 4350 pmu control registers */
			si_pmu_chipcontrol(sih, PMU_CHIPCTL1,
				PMU_CC1_ENABLE_BBPLL_PWR_DOWN, PMU_CC1_ENABLE_BBPLL_PWR_DOWN);
			si_pmu_regcontrol(sih, 0, ~0, 1);

			/* JIRA:SWWLAN-36186; HW4345-889 */
			si_pmu_chipcontrol(sih, PMU_CHIPCTL5, CC5_4350_PMU_EN_ASSERT_MASK,
				CC5_4350_PMU_EN_ASSERT_MASK);

			/* JIRA: SWWLAN-27486 optimize power consumption when wireless is down */
			if ((CHIPID(sih->chip) == BCM4350_CHIP_ID) &&
				(sih->chiprev == 0)) { /* 4350A0 */
				val = PMU_CC2_FORCE_SUBCORE_PWR_SWITCH_ON |
				      PMU_CC2_FORCE_PHY_PWR_SWITCH_ON |
				      PMU_CC2_FORCE_VDDM_PWR_SWITCH_ON |
				      PMU_CC2_FORCE_MEMLPLDO_PWR_SWITCH_ON;
				si_pmu_chipcontrol(sih, PMU_CHIPCTL2, val, val);

				val = PMU_CC6_ENABLE_CLKREQ_WAKEUP |
				      PMU_CC6_ENABLE_PMU_WAKEUP_ALP;
				si_pmu_chipcontrol(sih, PMU_CHIPCTL6, val, val);
			}
		}
		/* Set internal/external LPO */
		si_pmu_set_lpoclk(sih, osh);
		break;
	}
	case BCM4335_CHIP_ID:
	case BCM43602_CHIP_ID: /* fall through */
	case BCM43462_CHIP_ID: /* fall through */
		/* Set internal/external LPO */
		si_pmu_set_lpoclk(sih, osh);
		break;
	default:
		break;
	}
} /* si_pmu_chip_init */

void
si_pmu_slow_clk_reinit(si_t *sih, osl_t *osh)
{
#if !defined(BCMDONGLEHOST)
	chipcregs_t *cc;
	uint origidx;
	uint32 xtalfreq, mode;

	/* PMU specific initializations */
	if (!PMUCTL_ENAB(sih))
		return;
	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);
	if (cc == NULL)
		return;

	xtalfreq = getintvar(NULL, rstr_xtalfreq);
	switch (CHIPID(sih->chip)) {
		case BCM43242_CHIP_ID:
		case BCM43243_CHIP_ID:
			xtalfreq = 37400;
			break;
		case BCM43143_CHIP_ID:
			xtalfreq = 20000;
			break;
		case BCM43602_CHIP_ID:
		case BCM43462_CHIP_ID:
			xtalfreq = 40000;
			break;
		case BCM4350_CHIP_ID:
		case BCM4354_CHIP_ID:
		case BCM4356_CHIP_ID:
		case BCM43556_CHIP_ID:
		case BCM43558_CHIP_ID:
		case BCM43566_CHIP_ID:
		case BCM43568_CHIP_ID:
		case BCM43569_CHIP_ID:
		case BCM43570_CHIP_ID:
			if (xtalfreq == 0) {
				mode = CST4350_IFC_MODE(sih->chipst);
				if ((mode == CST4350_IFC_MODE_USB20D) ||
				    (mode == CST4350_IFC_MODE_USB30D) ||
				    (mode == CST4350_IFC_MODE_USB30D_WL))
					xtalfreq = 40000;
				else {
					xtalfreq = 37400;
					if (mode == CST4350_IFC_MODE_HSIC20D ||
					    mode == CST4350_IFC_MODE_HSIC30D) {
						/* HSIC sprom_present_strap=1:40 mHz xtal */
						if (((CHIPREV(sih->chiprev) >= 3) ||
						     (CHIPID(sih->chip) == BCM4354_CHIP_ID) ||
						     (CHIPID(sih->chip) == BCM4356_CHIP_ID) ||
						     (CHIPID(sih->chip) == BCM43569_CHIP_ID) ||
						     (CHIPID(sih->chip) == BCM43570_CHIP_ID)) &&
						    CST4350_PKG_USB_40M(sih->chipst) &&
						    CST4350_PKG_USB(sih->chipst)) {
							xtalfreq = 40000;
						}
					}
				}
			}
			break;
		default:
			break;
	}
	/* If xtalfreq var not available, try to measure it */
	if (xtalfreq == 0)
		xtalfreq = si_pmu_measure_alpclk(sih, osh);
	si_pmu_enb_slow_clk(sih, osh, xtalfreq);
	/* Return to original core */
	si_setcoreidx(sih, origidx);
#endif /* !BCMDONGLEHOST */
}

/* 4345 Active Voltage supply settings */
#define OTP4345_AVS_STATUS_OFFSET 0x14 /* offset in OTP for AVS status register */
#define OTP4345_AVS_RO_OFFSET 0x18 /* offset in OTP for AVS Ring Oscillator value */

#define AVS_STATUS_ENABLED_FLAG  0x1
#define AVS_STATUS_ABORT_FLAG    0x2
#define AVS_RO_OTP_MASK  0x1fff /* Only 13 bits of Ring Oscillator value are stored in OTP */
#define AVS_RO_MIN_COUNT  4800
#define AVS_RO_MAX_COUNT  8000
#define AVS_RO_IDEAL_COUNT   4800 /* Corresponds to 1.2V */


/** initialize PMU registers in case default values proved to be suboptimal */
void
BCMATTACHFN(si_pmu_swreg_init)(si_t *sih, osl_t *osh)
{
	uint16 cbuck_mv;
	int8 vreg_val;

	ASSERT(sih->cccaps & CC_CAP_PMU);

	switch (CHIPID(sih->chip)) {
	case BCM4325_CHIP_ID:
		if (CHIPREV(sih->chiprev) < 3)
			break;
		if (((sih->chipst & CST4325_PMUTOP_2B_MASK) >> CST4325_PMUTOP_2B_SHIFT) == 1) {
			/* Bump CLDO PWM output voltage to 1.25V */
			si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_CLDO_PWM, 0xf);
			/* Bump CLDO BURST output voltage to 1.25V */
			si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_CLDO_BURST, 0xf);
		}
		/* Bump CBUCK PWM output voltage to 1.5V */
		si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_CBUCK_PWM, 0xb);
		/* Bump CBUCK BURST output voltage to 1.5V */
		si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_CBUCK_BURST, 0xb);
		/* Bump LNLDO1 output voltage to 1.25V */
		si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_LNLDO1, 0x1);
		/* Select LNLDO2 output voltage to 2.5V */
		if (sih->boardflags & BFL_LNLDO2_2P5)
			si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_LNLDO2_SEL, 0x1);
		break;
	case BCM4315_CHIP_ID: {
		uint32 val;
		if (CHIPREV(sih->chiprev) != 2)
			break;

		W_REG(osh, PMUREG(sih, regcontrol_addr), 4);
		val = R_REG(osh, PMUREG(sih, regcontrol_data));
		val |= (uint32)(1 << 16);
		W_REG(osh, PMUREG(sih, regcontrol_data), val);
		break;
	}
	case BCM4336_CHIP_ID:
		if (CHIPREV(sih->chiprev) < 2) {
			/* Reduce CLDO PWM output voltage to 1.2V */
			si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_CLDO_PWM, 0xe);
			/* Reduce CLDO BURST output voltage to 1.2V */
			si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_CLDO_BURST, 0xe);
			/* Reduce LNLDO1 output voltage to 1.2V */
			si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_LNLDO1, 0xe);
		}
		if (CHIPREV(sih->chiprev) == 2) {
			/* Reduce CBUCK PWM output voltage */
			si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_CBUCK_PWM, 0x16);
			/* Reduce CBUCK BURST output voltage */
			si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_CBUCK_BURST, 0x16);
			si_pmu_set_ldo_voltage(sih, osh, SET_LNLDO_PWERUP_LATCH_CTRL, 0x3);
		}
		if (CHIPREV(sih->chiprev) == 0)
			si_pmu_regcontrol(sih, 2, 0x400000, 0x400000);

	case BCM43362_CHIP_ID:
	case BCM4330_CHIP_ID:
		cbuck_mv = (uint16)getintvar(NULL, rstr_cbuckout);

		/* set default cbuck output to be 1.5V */
		if (!cbuck_mv)
			cbuck_mv = 1500;
		vreg_val = si_pmu_cbuckout_to_vreg_ctrl(sih, cbuck_mv);
		/* set vreg ctrl only if there is a mapping defined for vout to bit map */
		if (vreg_val >= 0) {
			si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_CBUCK_PWM, vreg_val);
			si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_CBUCK_BURST, vreg_val);
		}
		break;
	case BCM4314_CHIP_ID:
		if (CHIPREV(sih->chiprev) == 0) {
			/* Reduce LPLDO2 output voltage to 1.0V */
			si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_LDO2, 0);
		}
		break;
	case BCM43341_CHIP_ID:
	case BCM4334_CHIP_ID:
	{
		const char *cldo_val;
		uint32 cldo;

		/* Clear BT/WL REG_ON pulldown resistor disable to reduce leakage */
		si_pmu_regcontrol(sih, PMU_VREG0_ADDR, (1 << PMU_VREG0_DISABLE_PULLD_BT_SHIFT) |
			(1 << PMU_VREG0_DISABLE_PULLD_WL_SHIFT), 0);

		if (!CST4334_CHIPMODE_HSIC(sih->chipst))
			si_pmu_chipcontrol(sih, 2, CCTRL4334_HSIC_LDO_PU, CCTRL4334_HSIC_LDO_PU);

		if ((cldo_val = getvar(NULL, rstr_cldo_ldo2)) != NULL) {
			/* Adjust LPLDO2 output voltage */
			cldo = (uint32)bcm_strtoul(cldo_val, NULL, 0);
			si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_LDO2, (uint8) cldo);
		}
#ifdef SRFAST
		else
			/* Reduce LPLDO2 output voltage to 1.0V by default for SRFAST */
			si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_LDO2, 0);
#endif

		if ((cldo_val = getvar(NULL, rstr_cldo_pwm)) != NULL) {
			cldo = (uint32)bcm_strtoul(cldo_val, NULL, 0);
			si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_CLDO_PWM, (uint8) cldo);
		}

		if ((cldo_val = getvar(NULL, rstr_force_pwm_cbuck)) != NULL) {
			cldo = (uint32)bcm_strtoul(cldo_val, NULL, 0);

			pmu_corereg(sih, SI_CC_IDX, regcontrol_addr,
				~0, 0);
			pmu_corereg(sih, SI_CC_IDX, regcontrol_data,
				0x1 << 1, (cldo & 0x1) << 1);
		}
	}
		break;
	case BCM43143_CHIP_ID:
#ifndef BCM_BOOTLOADER
		/* Force CBUCK to PWM mode */
		si_pmu_regcontrol(sih, 0, 0x2, 0x2);
		/* Increase CBUCK output voltage to 1.4V */
		si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_CBUCK_PWM, 0x2);
		si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_CBUCK_BURST, 0x2);
		/* Decrease LNLDO output voltage to just under 1.2V */
		si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_LNLDO1, 0x7);

		si_pmu_chipcontrol(sih, PMU_CHIPCTL0, PMU43143_XTAL_CORE_SIZE_MASK, 0x10);
#endif
		break;
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
		si_gci_chipcontrol(sih, 5, 0x3 << 29, 0x3 << 29);
		break;
	case BCM4350_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
		/*
		 * HW4350-275: Chip wedge due to xtal startup issue when WL is in SR
		 *     and BT is out of reset
		 * WAR: Set both GCI bits 189 and 190
		 *   bit 29 (189): When set this will allow wl2clb_xtal_pu as the
		 *       mux select for xtal_coresize_pmos, nmos from WL side
		 *   bit 30 (190): When set bt_xtal_pu will be able to change the wl
		 *       which selects xtal core size between
		 *       normal and start up while wl is in reset.
		 */
		if (CHIPREV(sih->chiprev) == 3)
			si_gci_chipcontrol(sih, 5, 0x3 << 29, 0x3 << 29);
		break;
	case BCM43602_CHIP_ID:
	case BCM43462_CHIP_ID:
		/* adjust PA Vref to 2.80V */
		si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_PAREF, 0x0c);
		break;
	case BCM4345_CHIP_ID:
#if !defined(BCMDONGLEHOST) && defined(DONGLEBUILD)
	{
		uint16 avs_ro_cnt = 0;
		uint16 avs_status = 0;
		uint32 gci_val = 0;

		if (getintvar(NULL, rstr_avs_enab)) {

			if (otp_read_word(sih, OTP4345_AVS_RO_OFFSET, &avs_status) != BCME_OK)
				break;

			if ((avs_status & AVS_STATUS_ENABLED_FLAG) &&
			    !(avs_status & AVS_STATUS_ABORT_FLAG)) {
				if (otp_read_word(sih, OTP4345_AVS_RO_OFFSET,
				                  &avs_ro_cnt) != BCME_OK)
					break;
				avs_ro_cnt &= AVS_RO_OTP_MASK;
				avs_ro_cnt = LIMIT_TO_RANGE(avs_ro_cnt, AVS_RO_MIN_COUNT,
				                            AVS_RO_MAX_COUNT);
				gci_val = ((((AVS_RO_IDEAL_COUNT - avs_ro_cnt) * 71) / 10000)
				           & 0x1F) | CC4345_GCI_AVS_CTRL_ENAB;
				si_gci_chipcontrol(sih, CHIPCTRLREG3, CC4345_GCI_AVS_CTRL_MASK,
				                   gci_val << CC4345_GCI_AVS_CTRL_SHIFT);
			}
		}
	}
#endif /* !BCMDONGLEHOST && DONGLEBUILD */
	break;
	default:
		break;
	}
	si_pmu_otp_regcontrol(sih, osh);
} /* si_pmu_swreg_init */

void
si_pmu_radio_enable(si_t *sih, bool enable)
{
	ASSERT(sih->cccaps & CC_CAP_PMU);

	switch (CHIPID(sih->chip)) {
	case BCM4325_CHIP_ID:
		if (sih->boardflags & BFL_FASTPWR)
			break;

		if ((sih->boardflags & BFL_BUCKBOOST)) {
			pmu_corereg(sih, SI_CC_IDX, min_res_mask,
			           PMURES_BIT(RES4325_BUCK_BOOST_BURST),
			           enable ? PMURES_BIT(RES4325_BUCK_BOOST_BURST) : 0);
		}

		if (enable) {
			OSL_DELAY(100 * 1000);
		}
		break;
	case BCM4319_CHIP_ID:
	case BCM4330_CHIP_ID:
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
	{
		uint32 wrap_reg, val;
		wrap_reg = si_wrapperreg(sih, AI_OOBSELOUTB74, 0, 0);

		val = ((1 << OOB_SEL_OUTEN_B_5) | (1 << OOB_SEL_OUTEN_B_6));
		if (enable)
			wrap_reg |= val;
		else
			wrap_reg &= ~val;
		si_wrapperreg(sih, AI_OOBSELOUTB74, ~0, wrap_reg);

		break;
	}
	default:
		break;
	}
} /* si_pmu_radio_enable */

/** Wait for a particular clock level to be on the backplane */
uint32
si_pmu_waitforclk_on_backplane(si_t *sih, osl_t *osh, uint32 clk, uint32 delay_val)
{
	ASSERT(sih->cccaps & CC_CAP_PMU);

	if (delay_val)
		SPINWAIT(((R_REG(osh, PMUREG(sih, pmustatus)) & clk) != clk), delay_val);
	return (R_REG(osh, PMUREG(sih, pmustatus)) & clk);
}

/**
 * Measures the ALP clock frequency in KHz.  Returns 0 if not possible.
 * Possible only if PMU rev >= 10 and there is an external LPO 32768Hz crystal.
 */

#define EXT_ILP_HZ 32768

uint32
BCMATTACHFN(si_pmu_measure_alpclk)(si_t *sih, osl_t *osh)
{
	uint32 alp_khz;
	uint32 pmustat_lpo = 0;

	if (sih->pmurev < 10)
		return 0;

	ASSERT(sih->cccaps & CC_CAP_PMU);
	if ((CHIPID(sih->chip) == BCM4335_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4345_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43602_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43462_CHIP_ID) ||
		BCM4350_CHIP(sih->chip) ||
		0)
		pmustat_lpo = !(R_REG(osh, PMUREG(sih, pmucontrol)) & PCTL_LPO_SEL);
	else
		pmustat_lpo = R_REG(osh, PMUREG(sih, pmustatus)) & PST_EXTLPOAVAIL;

	if (pmustat_lpo) {
		uint32 ilp_ctr, alp_hz;

		/* Enable the reg to measure the freq, in case disabled before */
		W_REG(osh, PMUREG(sih, pmu_xtalfreq), 1U << PMU_XTALFREQ_REG_MEASURE_SHIFT);

		/* Delay for well over 4 ILP clocks */
		OSL_DELAY(1000);

		/* Read the latched number of ALP ticks per 4 ILP ticks */
		ilp_ctr = R_REG(osh, PMUREG(sih, pmu_xtalfreq)) & PMU_XTALFREQ_REG_ILPCTR_MASK;

		/* Turn off the PMU_XTALFREQ_REG_MEASURE_SHIFT bit to save power */
		W_REG(osh, PMUREG(sih, pmu_xtalfreq), 0);

		/* Calculate ALP frequency */
		alp_hz = (ilp_ctr * EXT_ILP_HZ) / 4;

		/* Round to nearest 100KHz, and at the same time convert to KHz */
		alp_khz = (alp_hz + 50000) / 100000 * 100;
	} else
		alp_khz = 0;
	return alp_khz;
} /* si_pmu_measure_alpclk */

void
si_pmu_set_4330_plldivs(si_t *sih, uint8 dacrate)
{
	uint32 FVCO = si_pmu1_pllfvco0(sih)/1000;	/* in [Mhz] */
	uint32 m1div, m2div, m3div, m4div, m5div, m6div;
	uint32 pllc1, pllc2;

	m2div = m3div = m4div = m6div = FVCO/80;

	m5div = FVCO/dacrate;

	if (CST4330_CHIPMODE_SDIOD(sih->chipst))
		m1div = FVCO/80;
	else
		m1div = FVCO/90;
	pllc1 = (m1div << PMU1_PLL0_PC1_M1DIV_SHIFT) | (m2div << PMU1_PLL0_PC1_M2DIV_SHIFT) |
		(m3div << PMU1_PLL0_PC1_M3DIV_SHIFT) | (m4div << PMU1_PLL0_PC1_M4DIV_SHIFT);
	si_pmu_pllcontrol(sih, PMU1_PLL0_PLLCTL1, ~0, pllc1);

	pllc2 = si_pmu_pllcontrol(sih, PMU1_PLL0_PLLCTL2, 0, 0);
	pllc2 &= ~(PMU1_PLL0_PC2_M5DIV_MASK | PMU1_PLL0_PC2_M6DIV_MASK);
	pllc2 |= ((m5div << PMU1_PLL0_PC2_M5DIV_SHIFT) | (m6div << PMU1_PLL0_PC2_M6DIV_SHIFT));
	si_pmu_pllcontrol(sih, PMU1_PLL0_PLLCTL2, ~0, pllc2);
} /* si_pmu_set_4330_plldivs */

typedef struct cubkout2vreg {
	uint16 cbuck_mv;
	int8  vreg_val;
} cubkout2vreg_t;

cubkout2vreg_t BCMATTACHDATA(cbuck2vreg_tbl)[] = {
	{1500,	0},
	{1800,	0x17}
};

static int8
BCMATTACHFN(si_pmu_cbuckout_to_vreg_ctrl)(si_t *sih, uint16 cbuck_mv)
{
	uint32 i;

	for (i = 0; i < ARRAYSIZE(cbuck2vreg_tbl); i++) {
		if (cbuck_mv == cbuck2vreg_tbl[i].cbuck_mv)
			return cbuck2vreg_tbl[i].vreg_val;
	}
	return -1;
}

/** Update min/max resources after SR-ASM download to d11 txfifo */
void
si_pmu_res_minmax_update(si_t *sih, osl_t *osh)
{
	si_info_t *sii = SI_INFO(sih);
	uint32 min_mask = 0, max_mask = 0;
	chipcregs_t *cc;
	uint origidx, intr_val = 0;

	/* Block ints and save current core */
	INTR_OFF(sii, intr_val);
	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	switch (CHIPID(sih->chip)) {
	case BCM4345_CHIP_ID:
	case BCM43602_CHIP_ID:
	case BCM43462_CHIP_ID:
	case BCM4350_CHIP_ID:
	case BCM4354_CHIP_ID:
	case BCM4356_CHIP_ID:
	case BCM43556_CHIP_ID:
	case BCM43558_CHIP_ID:
	case BCM43566_CHIP_ID:
	case BCM43568_CHIP_ID:
	case BCM43569_CHIP_ID:
	case BCM43570_CHIP_ID:
		si_pmu_res_masks(sih, &min_mask, &max_mask);
		max_mask = 0; /* Only care about min_mask for now */
		break;
	case BCM4335_CHIP_ID:
#if defined(SAVERESTORE)
		min_mask = PMURES_BIT(RES4335_LPLDO_PO);
#else
		min_mask = PMURES_BIT(RES4335_WL_CORE_RDY) | PMURES_BIT(RES4335_OTP_PU);
#endif
		break;
	default:
		break;
	}

	if (min_mask) {
		/* Add min mask dependencies */
		min_mask |= si_pmu_res_deps(sih, osh, cc, min_mask, FALSE);
		W_REG(osh, PMUREG(sih, min_res_mask), min_mask);
	}

	if (max_mask) {
		/* Add max mask dependencies */
		max_mask |= si_pmu_res_deps(sih, osh, cc, max_mask, FALSE);
		W_REG(osh, PMUREG(sih, max_res_mask), max_mask);
	}

	si_pmu_wait_for_steady_state(sih, osh, cc);

	/* Return to original core */
	si_setcoreidx(sih, origidx);
	INTR_RESTORE(sii, intr_val);
} /* si_pmu_res_minmax_update */

#endif /* !defined(BCMDONGLEHOST) */
