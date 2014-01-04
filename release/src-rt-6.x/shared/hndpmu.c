/*
 * Misc utility routines for accessing PMU corerev specific features
 * of the SiliconBackplane-based Broadcom chips.
 *
 * Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
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
 * $Id: hndpmu.c 371895 2012-11-29 20:15:08Z $
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

/*
 * Following globals are used to store NVRAM data after reclaim. It would have
 * been better to use additional fields in si_t instead, but unfortunately
 * si_t (=si_pub) is the first field in si_info_t (not as a pointer, but the
 * entire structure is included). Therefore any change to si_t would change the
 * offsets of fields in si_info_t and as such break ROMmed code that uses it.
 */
static uint32 nvram_min_mask;
static bool min_mask_valid = FALSE;
static uint32 nvram_max_mask;
static bool max_mask_valid = FALSE;

/* PLL controls/clocks */
static void si_pmu0_pllinit0(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal);
static void si_pmu1_pllinit0(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal);
static void si_pmu1_pllinit1(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal);
static void si_pmu2_pllinit0(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal);
static void si_pmu_pll_off(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 *min_mask,
	uint32 *max_mask, uint32 *clk_ctl_st);
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

void *g_si_pmutmr_lock_arg = NULL;
si_pmu_callback_t g_si_pmutmr_lock_cb = NULL, g_si_pmutmr_unlock_cb = NULL;

/* FVCO frequency */
#define FVCO_880	880000	/* 880MHz */
#define FVCO_1760	1760000	/* 1760MHz */
#define FVCO_1440	1440000	/* 1440MHz */
#define FVCO_960	960000	/* 960MHz */

/* Read/write a chipcontrol reg */
uint32
si_pmu_chipcontrol(si_t *sih, uint reg, uint32 mask, uint32 val)
{
	si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol_addr), ~0, reg);
	return si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol_data), mask, val);
}

/* Read/write a regcontrol reg */
uint32
si_pmu_regcontrol(si_t *sih, uint reg, uint32 mask, uint32 val)
{
	si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, regcontrol_addr), ~0, reg);
	return si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, regcontrol_data), mask, val);
}

/* Read/write a pllcontrol reg */
uint32
si_pmu_pllcontrol(si_t *sih, uint reg, uint32 mask, uint32 val)
{
	si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, pllcontrol_addr), ~0, reg);
	return si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, pllcontrol_data), mask, val);
}

/* PMU PLL update */
void
si_pmu_pllupd(si_t *sih)
{
	si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, pmucontrol),
	           PCTL_PLL_PLLCTL_UPD, PCTL_PLL_PLLCTL_UPD);
}

/* PMU PLL reset */
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

	OR_REG(osh, &cc->pmucontrol, PCTL_PLL_PLLCTL_UPD);
	si_pmu_pll_on(sih, osh, cc, min_res_mask, max_res_mask, clk_ctl_st);

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

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

	chipcregs_t *cc;
	uint origidx;

	if (sih->pmurev >= 5) {
		pll_ctrlcnt = (sih->pmucaps & PCAP5_PC_MASK) >> PCAP5_PC_SHIFT;
	}
	else {
		pll_ctrlcnt = (sih->pmucaps & PCAP_PC_MASK) >> PCAP_PC_SHIFT;
	}

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	for (i = 0; i < pll_ctrlcnt; i++) {
		snprintf(name, sizeof(name), "pll%d", i);
		if ((otp_val = getvar(NULL, name)) == NULL)
			continue;

		val = (uint32)bcm_strtoul(otp_val, NULL, 0);
		W_REG(osh, &cc->pllcontrol_addr, i);
		W_REG(osh, &cc->pllcontrol_data, val);
	}
	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

/* The check for OTP parameters for the Voltage Regulator registers is done and if found the
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

	chipcregs_t *cc;
	uint origidx;

	if (sih->pmurev >= 5) {
		vreg_ctrlcnt = (sih->pmucaps & PCAP5_VC_MASK) >> PCAP5_VC_SHIFT;
	}
	else {
		vreg_ctrlcnt = (sih->pmucaps & PCAP_VC_MASK) >> PCAP_VC_SHIFT;
	}

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	for (i = 0; i < vreg_ctrlcnt; i++) {
		snprintf(name, sizeof(name), "reg%d", i);
		if ((otp_val = getvar(NULL, name)) == NULL)
			continue;

		val = (uint32)bcm_strtoul(otp_val, NULL, 0);
		W_REG(osh, &cc->regcontrol_addr, i);
		W_REG(osh, &cc->regcontrol_data, val);
	}
	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

/* The check for OTP parameters for the chip control registers is done and if found the
 * registers are updated accordingly.
 */
void
BCMATTACHFN(si_pmu_otp_chipcontrol)(si_t *sih, osl_t *osh)
{
	uint origidx;
	uint32 val, cc_ctrlcnt, i;
	char name[16];
	const char *otp_val;
	chipcregs_t *cc;

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	if (sih->pmurev >= 5) {
		cc_ctrlcnt = (sih->pmucaps & PCAP5_CC_MASK) >> PCAP5_CC_SHIFT;
	}
	else {
		cc_ctrlcnt = (sih->pmucaps & PCAP_CC_MASK) >> PCAP_CC_SHIFT;
	}

	for (i = 0; i < cc_ctrlcnt; i++) {
		snprintf(name, sizeof(name), "chipc%d", i);
		if ((otp_val = getvar(NULL, name)) == NULL)
			continue;

		val = (uint32)bcm_strtoul(otp_val, NULL, 0);
		W_REG(osh, &cc->chipcontrol_addr, i);
		W_REG(osh, &cc->chipcontrol_data, val);
	}

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

/* Setup switcher voltage */
void
BCMATTACHFN(si_pmu_set_switcher_voltage)(si_t *sih, osl_t *osh,
                                         uint8 bb_voltage, uint8 rf_voltage)
{
	chipcregs_t *cc;
	uint origidx;

	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	W_REG(osh, &cc->regcontrol_addr, 0x01);
	W_REG(osh, &cc->regcontrol_data, (uint32)(bb_voltage & 0x1f) << 22);

	W_REG(osh, &cc->regcontrol_addr, 0x00);
	W_REG(osh, &cc->regcontrol_data, (uint32)(rf_voltage & 0x1f) << 14);

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

void
BCMATTACHFN(si_pmu_set_ldo_voltage)(si_t *sih, osl_t *osh, uint8 ldo, uint8 voltage)
{
	uint8 sr_cntl_shift = 0, rc_shift = 0, shift = 0, mask = 0;
	uint8 addr = 0;

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
	case BCM43431_CHIP_ID:
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
	case BCM43143_CHIP_ID: /* No LDO specific initialization required */
	default:
		ASSERT(FALSE);
		return;
	}

	shift = sr_cntl_shift + rc_shift;

	si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, regcontrol_addr),
		~0, addr);
	si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, regcontrol_data),
		mask << shift, (voltage & mask) << shift);
}

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

	si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, min_res_mask),
	           PMURES_BIT(ldo), enable ? PMURES_BIT(ldo) : 0);
}

/* d11 slow to fast clock transition time in slow clock cycles */
#define D11SCC_SLOW2FAST_TRANSITION	2

/* returns [us] units */
uint16
BCMINITFN(si_pmu_fast_pwrup_delay)(si_t *sih, osl_t *osh)
{
	uint pmudelay = PMU_MAX_TRANSITION_DLY;
	chipcregs_t *cc;
	uint origidx;

	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

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
	case BCM4360_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM4352_CHIP_ID:
	case BCM43526_CHIP_ID:
		pmudelay = ISSIM_ENAB(sih) ? 70 : 3700;
		break;
	case BCM4328_CHIP_ID:
		pmudelay = 7000;
		break;
	case BCM4325_CHIP_ID:
		if (ISSIM_ENAB(sih))
			pmudelay = 70;
		else {
			uint32 ilp = si_ilp_clock(sih);
			pmudelay = (si_pmu_res_uptime(sih, osh, cc, RES4325_HT_AVAIL) +
			         D11SCC_SLOW2FAST_TRANSITION) * ((1000000 + ilp - 1) / ilp);
			pmudelay = (11 * pmudelay) / 10;
		}
		break;
	case BCM4329_CHIP_ID:
		if (ISSIM_ENAB(sih))
			pmudelay = 70;
		else {
			uint32 ilp = si_ilp_clock(sih);
			pmudelay = (si_pmu_res_uptime(sih, osh, cc, RES4329_HT_AVAIL) +
			         D11SCC_SLOW2FAST_TRANSITION) * ((1000000 + ilp - 1) / ilp);
			pmudelay = (11 * pmudelay) / 10;
		}
		break;
	case BCM4315_CHIP_ID:
		if (ISSIM_ENAB(sih))
			pmudelay = 70;
		else {
			uint32 ilp = si_ilp_clock(sih);
			pmudelay = (si_pmu_res_uptime(sih, osh, cc, RES4315_HT_AVAIL) +
			         D11SCC_SLOW2FAST_TRANSITION) * ((1000000 + ilp - 1) / ilp);
			pmudelay = (11 * pmudelay) / 10;
		}
		break;
	case BCM4319_CHIP_ID:
		pmudelay = ISSIM_ENAB(sih) ? 70 : 3700;
		break;
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
		if (ISSIM_ENAB(sih))
			pmudelay = 70;
		else {
			uint32 ilp = si_ilp_clock(sih);
			pmudelay = (si_pmu_res_uptime(sih, osh, cc, RES4336_HT_AVAIL) +
			         D11SCC_SLOW2FAST_TRANSITION) * ((1000000 + ilp - 1) / ilp);
			pmudelay = (11 * pmudelay) / 10;
		}
		break;
	case BCM4330_CHIP_ID:
		if (ISSIM_ENAB(sih))
			pmudelay = 70;
		else {
			uint32 ilp = si_ilp_clock(sih);
			pmudelay = (si_pmu_res_uptime(sih, osh, cc, RES4330_HT_AVAIL) +
			         D11SCC_SLOW2FAST_TRANSITION) * ((1000000 + ilp - 1) / ilp);
			pmudelay = (11 * pmudelay) / 10;
		}
		break;
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
		/* FIX: TODO */
		if (ISSIM_ENAB(sih))
			pmudelay = 70;
		else {
			uint32 ilp = si_ilp_clock(sih);
			pmudelay = (si_pmu_res_uptime(sih, osh, cc, RES4314_HT_AVAIL) +
			         D11SCC_SLOW2FAST_TRANSITION) * ((1000000 + ilp - 1) / ilp);
			pmudelay = (11 * pmudelay) / 10;
		}
		break;
	case BCM43143_CHIP_ID:
		if (ISSIM_ENAB(sih))
			pmudelay = 70;
		else {
			/* Retrieve time by reading it out of the hardware */
			uint32 ilp = si_ilp_clock(sih);
			pmudelay = (si_pmu_res_uptime(sih, osh, cc, RES43143_HT_AVAIL) +
			         D11SCC_SLOW2FAST_TRANSITION) * ((1000000 + ilp - 1) / ilp);
			pmudelay = (11 * pmudelay) / 10;
		}
		break;
	case BCM4334_CHIP_ID:
		/* FIX: TODO */
		if (ISSIM_ENAB(sih))
			pmudelay = 70;
		else {
			uint32 ilp = si_ilp_clock(sih);
			pmudelay = (si_pmu_res_uptime(sih, osh, cc, RES4334_HT_AVAIL) +
			         D11SCC_SLOW2FAST_TRANSITION) * ((1000000 + ilp - 1) / ilp);
			pmudelay = (11 * pmudelay) / 10;
		}
		break;
	case BCM4335_CHIP_ID:
		if (ISSIM_ENAB(sih))
			pmudelay = 70;
		else {
			uint32 ilp = si_ilp_clock(sih);
			pmudelay = (si_pmu_res_uptime(sih, osh, cc, RES4335_HT_AVAIL) +
			         D11SCC_SLOW2FAST_TRANSITION) * ((1000000 + ilp - 1) / ilp);
			pmudelay = (11 * pmudelay) / 10;
		}
		break;
	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
		if (ISSIM_ENAB(sih))
			pmudelay = 70;
		else {
			uint32 ilp = si_ilp_clock(sih);
			pmudelay = (si_pmu_res_uptime(sih, osh, cc, RES4324_HT_AVAIL) +
			         D11SCC_SLOW2FAST_TRANSITION) * ((1000000 + ilp - 1) / ilp);
			pmudelay = (11 * pmudelay) / 10;
		}
		break;
	default:
		break;
	}

	/* Return to original core */
	si_setcoreidx(sih, origidx);

	return (uint16)pmudelay;
}

uint32
BCMATTACHFN(si_pmu_force_ilp)(si_t *sih, osl_t *osh, bool force)
{
	chipcregs_t *cc;
	uint origidx;
	uint32 oldpmucontrol;

	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	oldpmucontrol = R_REG(osh, &cc->pmucontrol);
	if (force)
		W_REG(osh, &cc->pmucontrol, oldpmucontrol &
			~(PCTL_HT_REQ_EN | PCTL_ALP_REQ_EN));
	else
		W_REG(osh, &cc->pmucontrol, oldpmucontrol |
			(PCTL_HT_REQ_EN | PCTL_ALP_REQ_EN));

	/* Return to original core */
	si_setcoreidx(sih, origidx);

	return oldpmucontrol;
}

uint32
BCMATTACHFN(si_pmu_enb_ht_req)(si_t *sih, osl_t *osh, bool enb)
{
	chipcregs_t *cc;
	uint origidx;
	uint32 oldpmucontrol;

	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	oldpmucontrol = R_REG(osh, &cc->pmucontrol);
	if (enb)
		W_REG(osh, &cc->pmucontrol, oldpmucontrol | PCTL_HT_REQ_EN);
	else
		W_REG(osh, &cc->pmucontrol, oldpmucontrol & ~PCTL_HT_REQ_EN);

	/* Return to original core */
	si_setcoreidx(sih, origidx);

	return oldpmucontrol;
}

/* Setup resource up/down timers */
typedef struct {
	uint8 resnum;
	uint32 updown;
} pmu_res_updown_t;

/* Change resource dependencies masks */
typedef struct {
	uint32 res_mask;		/* resources (chip specific) */
	int8 action;			/* action */
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
	{RES4335_XTAL_PU, 0x00170002}
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

/* Determine min/max rsrc masks. Value 0 leaves hardware at default. */
static void
si_pmu_res_masks(si_t *sih, uint32 *pmin, uint32 *pmax)
{
	uint32 min_mask = 0, max_mask = 0;
	uint rsrcs;
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
		/* use chip default */
		break;

	case BCM4360_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM4352_CHIP_ID:
	case BCM43526_CHIP_ID:
		if (CHIPREV(sih->chiprev) >= 0x3) {
			int cst_ht = ((sih->chipst & CST4360_RSRC_INIT_MODE_MASK)
					>> CST4360_RSRC_INIT_MODE_SHIFT) & 0x1;
			if (cst_ht == 0)
				max_mask = 0x1ff;
		}
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
		min_mask = si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, min_res_mask), 0, 0);
		/* Set the bits for all resources in the max mask except for the SR Engine */
		max_mask = 0x7FFFFFFF;
		break;
	case BCM4335_CHIP_ID:
		/* Read the min_res_mask register. Set the mask to 0 as we need to only read. */
		min_mask = si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, min_res_mask), 0, 0);
		/* For 4335, min res mask is set to 1 after disabling power islanding */
		/* Write a non-zero value in chipcontrol reg 3 */
		si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol_addr),
		           PMU_CHIPCTL3, PMU_CHIPCTL3);
		si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol_data),
		           0x1, 0x1);
		/* Set the bits for all resources in the max mask except for the SR Engine */
		max_mask = 0x7FFFFFFF;
		break;
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
		/* Read the min_res_mask register. Set the mask to 0 as we need to only read. */
		min_mask = si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, min_res_mask), 0, 0);

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

	default:
		PMU_ERROR(("MIN and MAX mask is not programmed\n"));
		break;
	}

	/* Apply nvram override to min mask */
	if (min_mask_valid == TRUE) {
		PMU_MSG(("Applying rmin=%d to min_mask\n", nvram_min_mask));
		min_mask = nvram_min_mask;
	}
	/* Apply nvram override to max mask */
	if (max_mask_valid == TRUE) {
		PMU_MSG(("Applying rmax=%d to max_mask\n", nvram_max_mask));
		max_mask = nvram_max_mask;
	}

	*pmin = min_mask;
	*pmax = max_mask;
}

/* initialize PMU resources */
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
	uint32 min_mask = 0, max_mask = 0;
	char name[8];
	const char *val;
	uint i, rsrcs;

	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	/* Cache nvram override to min mask */
	if ((val = getvar(NULL, "rmin")) != NULL) {
		min_mask_valid = TRUE;
		nvram_min_mask = (uint32)bcm_strtoul(val, NULL, 0);
	}
	/* Cache nvram override to max mask */
	if ((val = getvar(NULL, "rmax")) != NULL) {
		max_mask_valid = TRUE;
		nvram_max_mask = (uint32)bcm_strtoul(val, NULL, 0);
	}

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
			W_REG(osh, &cc->chipcontrol_addr, PMU_CHIPCTL3);
			tmp = R_REG(osh, &cc->chipcontrol_data);
			tmp |= (1 << PMU_CC3_ENABLE_SDIO_WAKEUP_SHIFT);
			W_REG(osh, &cc->chipcontrol_data, tmp);
		}
	case BCM43142_CHIP_ID:
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
		W_REG(osh, &cc->res_table_sel,
		      pmu_res_updown_table[pmu_res_updown_table_sz].resnum);
		W_REG(osh, &cc->res_updn_timer,
		      pmu_res_updown_table[pmu_res_updown_table_sz].updown);
	}
	/* Apply nvram overrides to up/down timers */
	for (i = 0; i < rsrcs; i ++) {
		uint32 r_val;
		snprintf(name, sizeof(name), "r%dt", i);
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
		W_REG(osh, &cc->res_table_sel, (uint32)i);
		W_REG(osh, &cc->res_updn_timer, r_val);
	}

	/* Program resource dependencies table */
	while (pmu_res_depend_table_sz--) {
		ASSERT(pmu_res_depend_table != NULL);
		if (pmu_res_depend_table[pmu_res_depend_table_sz].filter != NULL &&
		    !(pmu_res_depend_table[pmu_res_depend_table_sz].filter)(sih))
			continue;
		for (i = 0; i < rsrcs; i ++) {
			if ((pmu_res_depend_table[pmu_res_depend_table_sz].res_mask &
			     PMURES_BIT(i)) == 0)
				continue;
			W_REG(osh, &cc->res_table_sel, i);
			switch (pmu_res_depend_table[pmu_res_depend_table_sz].action) {
			case RES_DEPEND_SET:
				PMU_MSG(("Changing rsrc %d res_dep_mask to 0x%x\n", i,
				    pmu_res_depend_table[pmu_res_depend_table_sz].depend_mask));
				W_REG(osh, &cc->res_dep_mask,
				      pmu_res_depend_table[pmu_res_depend_table_sz].depend_mask);
				break;
			case RES_DEPEND_ADD:
				PMU_MSG(("Adding 0x%x to rsrc %d res_dep_mask\n",
				    pmu_res_depend_table[pmu_res_depend_table_sz].depend_mask, i));
				OR_REG(osh, &cc->res_dep_mask,
				       pmu_res_depend_table[pmu_res_depend_table_sz].depend_mask);
				break;
			case RES_DEPEND_REMOVE:
				PMU_MSG(("Removing 0x%x from rsrc %d res_dep_mask\n",
				    pmu_res_depend_table[pmu_res_depend_table_sz].depend_mask, i));
				AND_REG(osh, &cc->res_dep_mask,
				        ~pmu_res_depend_table[pmu_res_depend_table_sz].depend_mask);
				break;
			default:
				ASSERT(0);
				break;
			}
		}
	}
	/* Apply nvram overrides to dependencies masks */
	for (i = 0; i < rsrcs; i ++) {
		snprintf(name, sizeof(name), "r%dd", i);
		if ((val = getvar(NULL, name)) == NULL)
			continue;
		PMU_MSG(("Applying %s=%s to rsrc %d res_dep_mask\n", name, val, i));
		W_REG(osh, &cc->res_table_sel, (uint32)i);
		W_REG(osh, &cc->res_dep_mask, (uint32)bcm_strtoul(val, NULL, 0));
	}

	/* Determine min/max rsrc masks */
	si_pmu_res_masks(sih, &min_mask, &max_mask);

	/* Add min mask dependencies */
	min_mask |= si_pmu_res_deps(sih, osh, cc, min_mask, FALSE);

	/* It is required to program max_mask first and then min_mask */
#ifdef BCM_BOOTLOADER
	if (CHIPID(sih->chip) == BCM4319_CHIP_ID) {
		min_mask |= R_REG(osh, &cc->min_res_mask);
		max_mask |= R_REG(osh, &cc->max_res_mask);
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

	if (max_mask) {
		/* Ensure there is no bit set in min_mask which is not set in max_mask */
		max_mask |= min_mask;

		/* First set the bits which change from 0 to 1 in max, then update the
		 * min_mask register and then reset the bits which change from 1 to 0
		 * in max. This is required as the bit in MAX should never go to 0 when
		 * the corresponding bit in min is still 1. Similarly the bit in min cannot
		 * be 1 when the corresponding bit in max is still 0.
		 */
		OR_REG(osh, &cc->max_res_mask, max_mask);
	} else {
		/* First set the bits which change from 0 to 1 in max, then update the
		 * min_mask register and then reset the bits which change from 1 to 0
		 * in max. This is required as the bit in MAX should never go to 0 when
		 * the corresponding bit in min is still 1. Similarly the bit in min cannot
		 * be 1 when the corresponding bit in max is still 0.
		 */
		if (min_mask)
			OR_REG(osh, &cc->max_res_mask, min_mask);
	}

	/* Program min resource mask */
	if (min_mask) {
		PMU_MSG(("Changing min_res_mask to 0x%x\n", min_mask));
		W_REG(osh, &cc->min_res_mask, min_mask);
	}

	/* Program max resource mask */
	if (max_mask) {
		PMU_MSG(("Changing max_res_mask to 0x%x\n", max_mask));
		W_REG(osh, &cc->max_res_mask, max_mask);
	}

	/* Add some delay; allow resources to come up and settle. */
	OSL_DELAY(2000);

	/* Return to original core */
	si_setcoreidx(sih, origidx);
#endif /* !_CFE_ && !_CFEZ_ */
}

/* setup pll and query clock speed */
typedef struct {
	uint16	freq;
	uint8	xf;
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
/*
 * Set new backplane PLL clock frequency
 */
static void
BCMATTACHFN(si_pmu0_sbclk4328)(si_t *sih, int freq)
{
	uint32 tmp, oldmax, oldmin, origidx;
	chipcregs_t *cc;

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc);

	/* Set new backplane PLL clock */
	W_REG(osh, &cc->pllcontrol_addr, PMU0_PLL0_PLLCTL0);
	tmp = R_REG(osh, &cc->pllcontrol_data);
	tmp &= ~(PMU0_PLL0_PC0_DIV_ARM_MASK);
	tmp |= freq << PMU0_PLL0_PC0_DIV_ARM_SHIFT;
	W_REG(osh, &cc->pllcontrol_data, tmp);

	/* Power cycle BB_PLL_PU by disabling/enabling it to take on new freq */
	/* Disable PLL */
	oldmin = R_REG(osh, &cc->min_res_mask);
	oldmax = R_REG(osh, &cc->max_res_mask);
	W_REG(osh, &cc->min_res_mask, oldmin & ~PMURES_BIT(RES4328_BB_PLL_PU));
	W_REG(osh, &cc->max_res_mask, oldmax & ~PMURES_BIT(RES4328_BB_PLL_PU));

	/* It takes over several hundred usec to re-enable the PLL since the
	 * sequencer state machines run on ILP clock. Set delay at 450us to be safe.
	 *
	 * Be sure PLL is powered down first before re-enabling it.
	 */

	OSL_DELAY(PLL_DELAY);
	SPINWAIT((R_REG(osh, &cc->res_state) & PMURES_BIT(RES4328_BB_PLL_PU)), PLL_DELAY*3);
	if (R_REG(osh, &cc->res_state) & PMURES_BIT(RES4328_BB_PLL_PU)) {
		/* If BB_PLL not powered down yet, new backplane PLL clock
		 *  may not take effect.
		 *
		 * Still early during bootup so no serial output here.
		 */
		PMU_ERROR(("Fatal: BB_PLL not power down yet!\n"));
		ASSERT(!(R_REG(osh, &cc->res_state) & PMURES_BIT(RES4328_BB_PLL_PU)));
	}

	/* Enable PLL */
	W_REG(osh, &cc->max_res_mask, oldmax);

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}
#endif /* BCMUSBDEV */

/* Set up PLL registers in the PMU as per the crystal speed.
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
	tmp = (R_REG(osh, &cc->pmucontrol) & PCTL_XTALFREQ_MASK) >>
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
		AND_REG(osh, &cc->min_res_mask, ~PMURES_BIT(RES4328_BB_PLL_PU));
		AND_REG(osh, &cc->max_res_mask, ~PMURES_BIT(RES4328_BB_PLL_PU));
		break;
	case BCM5354_CHIP_ID:
		AND_REG(osh, &cc->min_res_mask, ~PMURES_BIT(RES5354_BB_PLL_PU));
		AND_REG(osh, &cc->max_res_mask, ~PMURES_BIT(RES5354_BB_PLL_PU));
		break;
	default:
		ASSERT(0);
	}
	SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS0_HTAVAIL, PMU_MAX_TRANSITION_DLY);
	ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS0_HTAVAIL));

	PMU_MSG(("Done masking\n"));

	/* Write PDIV in pllcontrol[0] */
	W_REG(osh, &cc->pllcontrol_addr, PMU0_PLL0_PLLCTL0);
	tmp = R_REG(osh, &cc->pllcontrol_data);
	if (xt->freq >= PMU0_PLL0_PC0_PDIV_FREQ)
		tmp |= PMU0_PLL0_PC0_PDIV_MASK;
	else
		tmp &= ~PMU0_PLL0_PC0_PDIV_MASK;
	W_REG(osh, &cc->pllcontrol_data, tmp);

	/* Write WILD in pllcontrol[1] */
	W_REG(osh, &cc->pllcontrol_addr, PMU0_PLL0_PLLCTL1);
	tmp = R_REG(osh, &cc->pllcontrol_data);
	tmp = ((tmp & ~(PMU0_PLL0_PC1_WILD_INT_MASK | PMU0_PLL0_PC1_WILD_FRAC_MASK)) |
	       (((xt->wbint << PMU0_PLL0_PC1_WILD_INT_SHIFT) &
	         PMU0_PLL0_PC1_WILD_INT_MASK) |
	        ((xt->wbfrac << PMU0_PLL0_PC1_WILD_FRAC_SHIFT) &
	         PMU0_PLL0_PC1_WILD_FRAC_MASK)));
	if (xt->wbfrac == 0)
		tmp |= PMU0_PLL0_PC1_STOP_MOD;
	else
		tmp &= ~PMU0_PLL0_PC1_STOP_MOD;
	W_REG(osh, &cc->pllcontrol_data, tmp);

	/* Write WILD in pllcontrol[2] */
	W_REG(osh, &cc->pllcontrol_addr, PMU0_PLL0_PLLCTL2);
	tmp = R_REG(osh, &cc->pllcontrol_data);
	tmp = ((tmp & ~PMU0_PLL0_PC2_WILD_INT_MASK) |
	       ((xt->wbint >> PMU0_PLL0_PC2_WILD_INT_SHIFT) &
	        PMU0_PLL0_PC2_WILD_INT_MASK));
	W_REG(osh, &cc->pllcontrol_data, tmp);

	PMU_MSG(("Done pll\n"));

	/* Write XtalFreq. Set the divisor also. */
	tmp = R_REG(osh, &cc->pmucontrol);
	tmp = ((tmp & ~PCTL_ILP_DIV_MASK) |
	       (((((xt->freq + 127) / 128) - 1) << PCTL_ILP_DIV_SHIFT) & PCTL_ILP_DIV_MASK));
	tmp = ((tmp & ~PCTL_XTALFREQ_MASK) |
	       ((xt->xf << PCTL_XTALFREQ_SHIFT) & PCTL_XTALFREQ_MASK));
	W_REG(osh, &cc->pmucontrol, tmp);
}

/* query alp/xtal clock frequency */
static uint32
BCMINITFN(si_pmu0_alpclk0)(si_t *sih, osl_t *osh, chipcregs_t *cc)
{
	const pmu0_xtaltab0_t *xt;
	uint32 xf;

	/* Find the frequency in the table */
	xf = (R_REG(osh, &cc->pmucontrol) & PCTL_XTALFREQ_MASK) >>
	        PCTL_XTALFREQ_SHIFT;
	for (xt = pmu0_xtaltab0; xt->freq; xt++)
		if (xt->xf == xf)
			break;
	/* PLL must be configured before */
	ASSERT(xt->freq);

	return xt->freq * 1000;
}

/* query CPU clock frequency */
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
	W_REG(osh, &cc->pllcontrol_addr, PMU0_PLL0_PLLCTL0);
	tmp = R_REG(osh, &cc->pllcontrol_data);
	divarm = (tmp & PMU0_PLL0_PC0_DIV_ARM_MASK) >> PMU0_PLL0_PC0_DIV_ARM_SHIFT;

#ifdef BCMDBG
	/* Calculate fvco based on xtal freq, pdiv, and wild */
	pdiv = tmp & PMU0_PLL0_PC0_PDIV_MASK;

	W_REG(osh, &cc->pllcontrol_addr, PMU0_PLL0_PLLCTL1);
	tmp = R_REG(osh, &cc->pllcontrol_data);
	wbfrac = (tmp & PMU0_PLL0_PC1_WILD_FRAC_MASK) >> PMU0_PLL0_PC1_WILD_FRAC_SHIFT;
	wbint = (tmp & PMU0_PLL0_PC1_WILD_INT_MASK) >> PMU0_PLL0_PC1_WILD_INT_SHIFT;

	W_REG(osh, &cc->pllcontrol_addr, PMU0_PLL0_PLLCTL2);
	tmp = R_REG(osh, &cc->pllcontrol_data);
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
}

/* setup pll and query clock speed */
typedef struct {
	uint16	fref;
	uint8	xf;
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

/* table index */
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

/* table index */
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

static const pmu1_xtaltab0_t BCMINITDATA(pmu1_xtaltab0_960)[] = {
	{12000,   XTALTAB0_960_12000K,      1,      1,     0x50,   0x0     },
	{13000,   XTALTAB0_960_13000K,      1,      1,     0x49,   0xD89D89},
	{14400,   XTALTAB0_960_14400K,      1,      1,     0x42,   0xAAAAAA},
	{15360,   XTALTAB0_960_15360K,      1,      1,     0x3E,   0x800000},
	{16200,   XTALTAB0_960_16200K,      1,      1,     0x3B,   0x425ED0},
	{16800,   XTALTAB0_960_16800K,      1,      1,     0x39,   0x249249},
	{19200,   XTALTAB0_960_19200K,      1,      1,     0x32,   0x0     },
	{19800,   XTALTAB0_960_19800K,      1,      1,     0x30,   0x7C1F07},
	{20000,   XTALTAB0_960_20000K,      1,      1,     0x30,   0x0     },
	{24000,   XTALTAB0_960_24000K,      1,      1,     0x28,   0x0     },
	{25000,   XTALTAB0_960_25000K,      1,      1,     0x26,   0x666666},
	{26000,   XTALTAB0_960_26000K,      1,      1,     0x24,   0xEC4EC4},
	{30000,   XTALTAB0_960_30000K,      1,      1,     0x20,   0x0     },
	{33600,   XTALTAB0_960_33600K,      1,      1,     0x1C,   0x924924},
	{37400,   XTALTAB0_960_37400K,      2,      1,     0x33,   0x563EF9},
	{38400,   XTALTAB0_960_38400K,      2,      1,     0x32,   0x0	  },
	{40000,   XTALTAB0_960_40000K,      2,      1,     0x30,   0x0     },
	{48000,   XTALTAB0_960_48000K,      2,      1,     0x28,   0x0     },
	{52000,   XTALTAB0_960_52000K,      2,      1,     0x24,   0xEC4EC4},
	{0,	      0,       0,      0,     0,      0	      }
};

/* table index */
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
	uint16 	clock;
	uint8	mode;
	uint8	xf;
} pllctrl_data_t;

/*  *****************************  tables for 4335a0 *********************** */
/* PLL control register table giving info about the xtal supported for 4324.
* There should be a one to one mapping for "pllctrl_data_t" and the corresponding table
* pmu1_pllctrl_tab_4335.
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


static const uint32	pmu1_pllctrl_tab_4335_963mhz[] = {
	0x50800000, 0x0C080803, 0x28310806, 0x61400000, 0x02600005, 0x0004FFFD,
	0x50800000, 0x0C080803, 0x25310806, 0x4013B13B, 0x02600005, 0x00049D87,
	0x50800000, 0x0C080803, 0x21310806, 0x40E00000, 0x02600005, 0x00042AA8,
	0x50800000, 0x0C080803, 0x1F310806, 0x40B20000, 0x02600005, 0x0003E7FE,
	0x50800000, 0x0C080803, 0x1DB10806, 0x2071C71C, 0x02600005, 0x0003B424,
	0x50800000, 0x0C080803, 0x1CB10806, 0x20524924, 0x02600005, 0x00039247,
	0x50800000, 0x0C080803, 0x19310806, 0x01280000, 0x02600005, 0x00031FFE,
	0x50800000, 0x0C080803, 0x18310806, 0x00A2E8BA, 0x02600005, 0x000307C0,
	0x50800000, 0x0C080803, 0x18310806, 0x01266666, 0x02600005, 0x0002FFFE,
	0x50800000, 0x0C080803, 0x14310806, 0xC1200000, 0x02600004, 0x00027FFE,
	0x50800000, 0x0C080803, 0x13310806, 0xC0851EB8, 0x02600004, 0x00026665,
	0x50800000, 0x0C080803, 0x12B10806, 0xC009D89D, 0x02600004, 0x00024EC4,
	0x50800000, 0x0C080803, 0x10310806, 0xA1199999, 0x02600004, 0x0001FFFF,
	0x50800000, 0x0C080803, 0x0E310806, 0xA0A92492, 0x02600004, 0x0001C923,
	0x50800000, 0x0C060803, 0x0CB10806, 0x80BFA862, 0x02600004, 0x00019AB1,
	0x50800000, 0x0C080803, 0x0CB10806, 0x81140000, 0x02600004, 0x00018FFF,
	0x50800000, 0x0C080803, 0x0C310806, 0x81133333, 0x02600004, 0x00017FFF,
	0x50800000, 0x0C080803, 0x0A310806, 0x61100000, 0x02600004, 0x00013FFF,
	0x50800000, 0x0C080803, 0x09310806, 0x6084EC4E, 0x02600004, 0x00012762,
};

/*  ************************  tables for 4335a0 END *********************** */

/*  *****************************  tables for 43242a0 *********************** */
/* PLL control register table giving info about the xtal supported for 4324.
* There should be a one to one mapping for "pllctrl_data_t" and the corresponding table
* pmu1_pllctrl_tab_4324.
*/
static const pllctrl_data_t BCMATTACHDATA(pmu1_xtaltab0_43242)[] = {
	{37400, 0, XTALTAB0_960_37400K},
};

/*  ************************  tables for 4324a02 END *********************** */

/*  *****************************  tables for 4350a0 *********************** */

#define XTAL_DEFAULT_4350	37400
static const pllctrl_data_t pmu1_xtaltab0_4350[] = {
	{37400, 0, XTALTAB0_960_37400K},
	{40000, 0, XTALTAB0_960_40000K},
};

static const uint32	pmu1_pllctrl_tab_4350_963mhz[] = {
	0x50800000, 0x0C060603, 0x0CB10814, 0x80BFAA00, 0x02600004, 0x00019AB1, 0x04a6c181,
	0x50800000, 0x0C060603, 0xC310814, 0x133333, 0x02600004, 0x00019AB1, 0x04a6c181
};
/*  ************************  tables for 4350a0 END *********************** */

/* PLL control register values(all registers) for the xtal supported for 43242.
 * There should be a one to one mapping for "pllctrl_data_t" and the table below.
 * only support 37.4M
 */
static const uint32	BCMATTACHDATA(pmu1_pllctrl_tab_43242A0)[] = {
	0xA7400040, 0x10080A06, 0x0CB11408, 0x80AB1F7C, 0x02600004, 0x00A6C4D3, /* 37400 */
};

static const uint32	BCMATTACHDATA(pmu1_pllctrl_tab_43242A1)[] = {
	0xA7400040, 0x10080A06, 0x0CB11408, 0x80AB1F7C, 0x02600004, 0x00A6C191, /* 37400 */
};

/* 4334/4314 ADFLL freq target params */
typedef struct {
	uint16  fref;
	uint8   xf;
	uint32  freq_tgt;
} pmu2_xtaltab0_t;

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

/* returns xtal table for each chip */
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
	case BCM43526_CHIP_ID:
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
}

/* returns default xtal frequency for each chip */
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
	case BCM43460_CHIP_ID:
	case BCM43526_CHIP_ID:
		/* Default to 37400Khz */
		return &pmu1_xtaltab0_960[PMU1_XTALTAB0_960_37400K];
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
}

/* select default pll fvco for each chip */
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
	case BCM4335_CHIP_ID:
	case BCM4360_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM43526_CHIP_ID:
		return FVCO_960;
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
}

/* query alp/xtal clock frequency */
static uint32
BCMINITFN(si_pmu1_alpclk0)(si_t *sih, osl_t *osh, chipcregs_t *cc)
{
	const pmu1_xtaltab0_t *xt;
	uint32 xf;

	/* Find the frequency in the table */
	xf = (R_REG(osh, &cc->pmucontrol) & PCTL_XTALFREQ_MASK) >>
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

static uint32
si_pmu_htclk_mask(si_t *sih)
{
	uint32 ht_req = 0;

	switch (CHIPID(sih->chip))
	{
		case BCM43239_CHIP_ID:
			ht_req = PMURES_BIT(RES43239_HT_AVAIL) |
				PMURES_BIT(RES43239_MACPHY_CLKAVAIL);
			break;
		case BCM4324_CHIP_ID:
	    case BCM43242_CHIP_ID:
	    case BCM43243_CHIP_ID:
			ht_req = PMURES_BIT(RES4324_HT_AVAIL) |
				PMURES_BIT(RES4324_MACPHY_CLKAVAIL);
			break;
		case BCM4330_CHIP_ID:
			ht_req = PMURES_BIT(RES4330_BBPLL_PWRSW_PU) |
				PMURES_BIT(RES4330_MACPHY_CLKAVAIL) | PMURES_BIT(RES4330_HT_AVAIL);
			break;
		case BCM43362_CHIP_ID:
		case BCM4336_CHIP_ID:
			ht_req = PMURES_BIT(RES4336_BBPLL_PWRSW_PU) |
				PMURES_BIT(RES4336_MACPHY_CLKAVAIL) | PMURES_BIT(RES4336_HT_AVAIL);
			break;
		case BCM4334_CHIP_ID:
			ht_req = PMURES_BIT(RES4334_HT_AVAIL) |
				PMURES_BIT(RES4334_MACPHY_CLK_AVAIL);
			break;
		case BCM4335_CHIP_ID:
			ht_req = PMURES_BIT(RES4335_HT_AVAIL) |
				PMURES_BIT(RES4335_MACPHY_CLKAVAIL) |
				PMURES_BIT(RES4335_HT_START);
			break;
		case BCM43143_CHIP_ID:
			ht_req = PMURES_BIT(RES43143_HT_AVAIL) |
				PMURES_BIT(RES43143_MACPHY_CLK_AVAIL);
			break;
		default:
			ASSERT(0);
			break;
	}
	return ht_req;
}

void
si_pmu_minresmask_htavail_set(si_t *sih, osl_t *osh, bool set_clear)
{
	chipcregs_t *cc;
	uint origidx;
	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	if (!set_clear) {
		switch (CHIPID(sih->chip)) {
		case BCM4313_CHIP_ID:
			if ((cc->min_res_mask) & (PMURES_BIT(RES4313_HT_AVAIL_RSRC)))
				AND_REG(osh, &cc->min_res_mask,
					~(PMURES_BIT(RES4313_HT_AVAIL_RSRC)));
			break;
		default:
			break;
		}
	}

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

uint
si_pll_minresmask_reset(si_t *sih, osl_t *osh)
{
	chipcregs_t *cc;
	uint origidx;
	uint err = BCME_OK;

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	switch (CHIPID(sih->chip)) {
		case BCM4313_CHIP_ID:
			/* write to min_res_mask 0x200d : clear min_rsrc_mask */
			AND_REG(osh, &cc->min_res_mask, ~(PMURES_BIT(RES4313_HT_AVAIL_RSRC)));
			OSL_DELAY(100);
			/* write to max_res_mask 0xBFFF: clear max_rsrc_mask */
			AND_REG(osh, &cc->max_res_mask, ~(PMURES_BIT(RES4313_HT_AVAIL_RSRC)));
			OSL_DELAY(100);
			/* write to max_res_mask 0xFFFF :set max_rsrc_mask */
			OR_REG(osh, &cc->max_res_mask, (PMURES_BIT(RES4313_HT_AVAIL_RSRC)));
			break;
		default:
			PMU_ERROR(("%s: PLL reset not supported\n", __FUNCTION__));
			err = BCME_UNSUPPORTED;
			break;
	}
	/* Return to original core */
	si_setcoreidx(sih, origidx);
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
		clock = 37400*1000;
		break;
	}
	return clock;
}

/* Note: if cc is NULL, this function returns xf, without programming PLL registers. */
static uint8
si_pmu_pllctrlreg_update(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal, uint8 spur_mode,
	const pllctrl_data_t *pllctrlreg_update, uint32 array_size, const uint32 *pllctrlreg_val)
{
	uint8 indx, reg_offset, xf = 0;
	uint8 pll_ctrlcnt = 0;

	ASSERT(pllctrlreg_update);

	if (sih->pmurev >= 5) {
		pll_ctrlcnt = (sih->pmucaps & PCAP5_PC_MASK) >> PCAP5_PC_SHIFT;
	} else {
		pll_ctrlcnt = (sih->pmucaps & PCAP_PC_MASK) >> PCAP_PC_SHIFT;
	}

	/* Program the PLL control register if the xtal value matches with the table value */
	for (indx = 0; indx < array_size; indx++) {
		/* If the entry does not match the xtal and spur_mode jsut contiue the loop */
		if (!((pllctrlreg_update[indx].clock == (uint16)xtal) &&
			(pllctrlreg_update[indx].mode == spur_mode)))
			continue;
		/* Dont program the PLL registers if register base is NULL.
		 * If null just return the xref.
		 */
		if (cc) {
			for (reg_offset = 0; reg_offset < pll_ctrlcnt; reg_offset++) {
				W_REG(osh, &cc->pllcontrol_addr, reg_offset);
				W_REG(osh, &cc->pllcontrol_data,
					pllctrlreg_val[indx*pll_ctrlcnt + reg_offset]);
			}
		}
		xf = pllctrlreg_update[indx].xf;
		break;
	}
	return xf;
}

/* Chip-specific overrides to PLLCONTROL registers during init */
/* This takes less precedence over OTP PLLCONTROL overrides */
/* If update_required=FALSE, it returns TRUE if a update is about to occur.
 * No write happens
 */
bool
BCMATTACHFN(si_pmu_update_pllcontrol)(si_t *sih, osl_t *osh, uint32 xtal, bool update_required)
{
	chipcregs_t *cc;
	uint origidx;
	bool write_en = FALSE;
	uint8 xf = 0;
	uint32 tmp;
	uint32 xtalfreq = 0;
	const pllctrl_data_t *pllctrlreg_update = NULL;
	uint32 array_size = 0;
	const uint32 *pllctrlreg_val = NULL;

	/* If there is OTP or NVRAM entry for xtalfreq, program the
	 * PLL control register even if it is default xtal.
	 */
	xtalfreq = getintvar(NULL, "xtalfreq");
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
		pllctrlreg_update = pmu1_xtaltab0_4335;
		array_size = ARRAYSIZE(pmu1_xtaltab0_4335);
		pllctrlreg_val = pmu1_pllctrl_tab_4335_963mhz;

#ifndef BCM_BOOTLOADER
		/* If PMU1_PLL0_PC2_MxxDIV_MASKxx have to change,
		 * then set write_en to true.
		 */
		write_en = TRUE;
#endif
		break;

	case BCM4350_CHIP_ID:
		pllctrlreg_update = pmu1_xtaltab0_4350;
		array_size = ARRAYSIZE(pmu1_xtaltab0_4350);
		pllctrlreg_val = pmu1_pllctrl_tab_4350_963mhz;

		/* If PMU1_PLL0_PC2_MxxDIV_MASKxx have to change,
		 * then set write_en to true.
		 */
		write_en = TRUE;
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
#ifndef BCM_BOOTLOADER
		write_en = TRUE;
#endif
		break;

	/* FOR ANY FURTHER CHIPS, add a case here, define tables similar to
	 * pmu1_xtaltab0_4324 & pmu1_pllctrl_tab_4324 based on resources
	 * and assign. For loop below will take care of programming.
	 */
	default:
		/* write_en is FALSE in this case. So returns from the function */
		write_en = FALSE;
		break;
	}

	/* ** PROGRAM THE REGISTERS BASED ON ABOVE CONDITIONS */
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
		 * the xf for the xtal being programmed but dont program the registers now
		 * as the PLL is not yet turned OFF.
		 */
		xf = si_pmu_pllctrlreg_update(sih, osh, NULL, xtal, 0, pllctrlreg_update,
			array_size, pllctrlreg_val);

		/* Program the PLL based on the xtal value. */
		if (xf != 0) {
			/* Write XtalFreq. Set the divisor also. */
			tmp = R_REG(osh, &cc->pmucontrol) &
				~(PCTL_ILP_DIV_MASK | PCTL_XTALFREQ_MASK);
			tmp |= (((((xtal + 127) / 128) - 1) << PCTL_ILP_DIV_SHIFT) &
				PCTL_ILP_DIV_MASK) |
				((xf << PCTL_XTALFREQ_SHIFT) & PCTL_XTALFREQ_MASK);
			W_REG(osh, &cc->pmucontrol, tmp);
		} else {
			write_en = FALSE;
			printf("Invalid/Unsupported xtal value %d", xtal);
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
	default:
		break;
	}

exit:
	/* Return to original core */
	si_setcoreidx(sih, origidx);

	return write_en;
}

int si_pmu_wait_for_steady_state(osl_t *osh, chipcregs_t *cc);

uint32
si_pmu_get_pmutime_diff(osl_t *osh, chipcregs_t *cc, uint32 *prev);

bool
si_pmu_wait_for_res_pending(osl_t *osh, chipcregs_t *cc, uint usec,
	bool cond, uint32 *elapsed_time);

uint32
si_pmu_get_pmutimer(osl_t *osh, chipcregs_t *cc);

/* PMU timer ticks once in 32uS */
#define PMU_US_STEPS (32)

uint32
si_pmu_get_pmutimer(osl_t *osh, chipcregs_t *cc)
{
	uint32 start;
	start = R_REG(osh, &cc->pmutimer);
	if (start != R_REG(osh, &cc->pmutimer))
		start = R_REG(osh, &cc->pmutimer);
	return (start);
}

/* returns
 * a) diff between a 'prev' value of pmu timer and current value
 * b) the current pmutime value in 'prev'
 * 	So, 'prev' is an IO parameter.
 */
uint32
si_pmu_get_pmutime_diff(osl_t *osh, chipcregs_t *cc, uint32 *prev)
{
	uint32 pmutime_diff = 0, pmutime_val = 0;
	uint32 prev_val = *prev;

	/* read current value */
	pmutime_val = si_pmu_get_pmutimer(osh, cc);
	/* diff btween prev and current value, take on wraparound case as well. */
	pmutime_diff = (pmutime_val >= prev_val) ?
		(pmutime_val - prev_val) :
		(~prev_val + pmutime_val + 1);
	*prev = pmutime_val;
	return pmutime_diff;
}

/* wait for usec for the res_pending register to change.
	NOTE: usec SHOULD be > 32uS
	if cond = TRUE, res_pending will be read until it becomes == 0;
	If cond = FALSE, res_pending will be read until it becomes != 0;
	returns TRUE if timedout.
	returns elapsed time in this loop in elapsed_time
*/
bool
si_pmu_wait_for_res_pending(osl_t *osh, chipcregs_t *cc, uint usec,
	bool cond, uint32 *elapsed_time)
{
	/* add 32uSec more */
	uint countdown = usec;
	uint32 pmutime_prev = 0, pmutime_elapsed = 0, res_pend;
	bool pending = FALSE;

	/* store current time */
	pmutime_prev = si_pmu_get_pmutimer(osh, cc);
	while (1) {
		res_pend = R_REG(osh, &cc->res_pending);

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
		pmutime_elapsed += si_pmu_get_pmutime_diff(osh, cc, &pmutime_prev);
	}

	*elapsed_time = pmutime_elapsed * PMU_US_STEPS;
	return pending;
}

/*
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
int si_pmu_wait_for_steady_state(osl_t *osh, chipcregs_t *cc)
{
	int stat = 0;
	bool timedout = FALSE;
	uint32 elapsed = 0, pmutime_total_elapsed = 0;

	while (1) {
		/* wait until all resources are settled down [till res_pending becomes 0] */
		timedout = si_pmu_wait_for_res_pending(osh, cc,
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
		timedout = si_pmu_wait_for_res_pending(osh, cc,
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
}


/* Turn Off the PLL - Required before setting the PLL registers */
static void
si_pmu_pll_off(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 *min_mask,
	uint32 *max_mask, uint32 *clk_ctl_st)
{
	uint32 ht_req;

	/* Save the original register values */
	*min_mask = R_REG(osh, &cc->min_res_mask);
	*max_mask = R_REG(osh, &cc->max_res_mask);
	*clk_ctl_st = R_REG(osh, &cc->clk_ctl_st);

	ht_req = si_pmu_htclk_mask(sih);
	if (ht_req == 0)
		return;

	if ((CHIPID(sih->chip) == BCM4335_CHIP_ID) ||
	0) {
		/* slightly different way for 4335, but this could be applied for other chips also.
		* If HT_AVAIL is not set, wait to see if any resources are availing HT.
		*/
		if (((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL) != CCS_HTAVAIL))
			si_pmu_wait_for_steady_state(osh, cc);
	} else {
		OR_REG(osh,  &cc->max_res_mask, ht_req);
		/* wait for HT to be ready before taking the HT away...HT could be coming up... */
		SPINWAIT(((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL) != CCS_HTAVAIL),
			PMU_MAX_TRANSITION_DLY);
		ASSERT((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
	}

	AND_REG(osh, &cc->min_res_mask, ~ht_req);
	AND_REG(osh, &cc->max_res_mask, ~ht_req);

	SPINWAIT(((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL) == CCS_HTAVAIL),
		PMU_MAX_TRANSITION_DLY);
	ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
	OSL_DELAY(100);
}

/* Turn ON/restore the PLL based on the mask received */
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
		OR_REG(osh, &cc->max_res_mask, max_mask_mask);

	if (min_mask_mask != 0)
		OR_REG(osh, &cc->min_res_mask, min_mask_mask);

	if (clk_ctl_st_mask & CCS_HTAVAIL) {
		/* Wait for HT_AVAIL to come back */
		SPINWAIT(((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL) != CCS_HTAVAIL),
			PMU_MAX_TRANSITION_DLY);
		ASSERT((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
	}
}

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
		snprintf(name, sizeof(name), "pll%d", i);
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
		OR_REG(osh, &cc->pmucontrol, PCTL_PLL_PLLCTL_UPD);

	/* Restore back the register values. This ensures PLL remains on if it
	 * was originally on and remains off if it was originally off.
	 */
	si_pmu_pll_on(sih, osh, cc, min_mask, max_mask, clk_ctl_st);
}

static void
BCMATTACHFN(si_pmu2_pllinit0)(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal)
{
	const pmu2_xtaltab0_t *xt;
	int xt_idx;
	uint32 freq_tgt, pll0;

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

	W_REG(osh, &cc->pllcontrol_addr, PMU15_PLL_PLLCTL0);
	pll0 = R_REG(osh, &cc->pllcontrol_data);

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
	case BCM4334_CHIP_ID:
		AND_REG(osh, &cc->min_res_mask,
			~(PMURES_BIT(RES4334_HT_AVAIL) | PMURES_BIT(RES4334_MACPHY_CLK_AVAIL)));
		AND_REG(osh, &cc->max_res_mask,
			~(PMURES_BIT(RES4334_HT_AVAIL) | PMURES_BIT(RES4334_MACPHY_CLK_AVAIL)));
		SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL, PMU_MAX_TRANSITION_DLY);
		ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
		break;
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
		AND_REG(osh, &cc->min_res_mask,
			~(PMURES_BIT(RES4314_HT_AVAIL) | PMURES_BIT(RES4314_MACPHY_CLK_AVAIL)));
		AND_REG(osh, &cc->max_res_mask,
			~(PMURES_BIT(RES4314_HT_AVAIL) | PMURES_BIT(RES4314_MACPHY_CLK_AVAIL)));
		SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL, PMU_MAX_TRANSITION_DLY);
		ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
		break;
	case BCM43143_CHIP_ID:
		AND_REG(osh, &cc->min_res_mask,
			~(PMURES_BIT(RES43143_HT_AVAIL) | PMURES_BIT(RES43143_MACPHY_CLK_AVAIL)));
		AND_REG(osh, &cc->max_res_mask,
			~(PMURES_BIT(RES43143_HT_AVAIL) | PMURES_BIT(RES43143_MACPHY_CLK_AVAIL)));
		SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL, PMU_MAX_TRANSITION_DLY);
		ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
		break;
	default:
		PMU_ERROR(("%s: Turn HT off for 0x%x????\n", __FUNCTION__, CHIPID(sih->chip)));
		break;
	}

	pll0 = (pll0 & ~PMU15_PLL_PC0_FREQTGT_MASK) | (xt->freq_tgt << PMU15_PLL_PC0_FREQTGT_SHIFT);
	W_REG(osh, &cc->pllcontrol_data, pll0);

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
		OR_REG(osh, &cc->pmucontrol, PCTL_PLL_PLLCTL_UPD);

exit:
	/* Vars over-rides */
	si_pmu2_pll_vars_init(sih, osh, cc);
}

static uint32
BCMINITFN(si_pmu2_cpuclk0)(si_t *sih, osl_t *osh, chipcregs_t *cc)
{
	const pmu2_xtaltab0_t *xt;
	uint32 freq_tgt = 0, pll0 = 0;

	switch (CHIPID(sih->chip)) {
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
	case BCM43143_CHIP_ID:
	case BCM4334_CHIP_ID:
		W_REG(osh, &cc->pllcontrol_addr, PMU15_PLL_PLLCTL0);
		pll0 = R_REG(osh, &cc->pllcontrol_data);
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

/*
 * Query ALP/xtal clock frequency
 */
static uint32
BCMINITFN(si_pmu2_alpclk0)(si_t *sih, osl_t *osh, chipcregs_t *cc)
{
	const pmu2_xtaltab0_t *xt;
	uint32 freq_tgt, pll0;

	W_REG(osh, &cc->pllcontrol_addr, PMU15_PLL_PLLCTL0);
	pll0 = R_REG(osh, &cc->pllcontrol_data);

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

/* Set up PLL registers in the PMU as per the OTP values. */
static void
BCMATTACHFN(si_pmu1_pllinit1)(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal)
{
	char name[16];
	const char *otp_val;
	uint8 i, otp_entry_found = FALSE, pll_reg_update_required;
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
		snprintf(name, sizeof(name), "pll%d", i);
		if ((otp_val = getvar(NULL, name)) == NULL)
			continue;

		/* If OTP entry is found for PLL register, then turn off the PLL
		 * and set the status of the OTP entry accordingly.
		 */
		otp_entry_found = TRUE;
		break;
	}

	pll_reg_update_required = si_pmu_update_pllcontrol(sih, osh, xtal, FALSE);

	/* If no OTP parameter is found and no chip-specific updates are needed, return. */
	if ((otp_entry_found == FALSE) && (pll_reg_update_required == FALSE))
		return;

	/* Make sure PLL is off */
	si_pmu_pll_off(sih, osh, cc, &min_mask, &max_mask, &clk_ctl_st);

	if (pll_reg_update_required) {
	/* Update any chip-specific PLL registers . Do actual write */
	si_pmu_update_pllcontrol(sih, osh, xtal, TRUE);
	}

	/* Update the PLL register if there is a OTP entry for PLL registers */
	si_pmu_otp_pllcontrol(sih, osh);

	/* Flush deferred pll control registers writes */
	if (sih->pmurev >= 2)
		OR_REG(osh, &cc->pmucontrol, PCTL_PLL_PLLCTL_UPD);

	/* Restore back the register values. This ensures PLL remains on if it
	 * was originally on and remains off if it was originally off.
	 */
	si_pmu_pll_on(sih, osh, cc, min_mask, max_mask, clk_ctl_st);
}

/* Set up PLL registers in the PMU as per the crystal speed.
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
	if ((((R_REG(osh, &cc->pmucontrol) & PCTL_XTALFREQ_MASK) >>
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
		AND_REG(osh, &cc->min_res_mask,
		        ~(PMURES_BIT(RES4325_BBPLL_PWRSW_PU) | PMURES_BIT(RES4325_HT_AVAIL)));
		AND_REG(osh, &cc->max_res_mask,
		        ~(PMURES_BIT(RES4325_BBPLL_PWRSW_PU) | PMURES_BIT(RES4325_HT_AVAIL)));
		SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL, PMU_MAX_TRANSITION_DLY);
		ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
		break;
	case BCM4329_CHIP_ID:
		/* Change the BBPLL drive strength to 8 for all channels */
		buf_strength = 0x888888;
		AND_REG(osh, &cc->min_res_mask,
		        ~(PMURES_BIT(RES4329_BBPLL_PWRSW_PU) | PMURES_BIT(RES4329_HT_AVAIL)));
		AND_REG(osh, &cc->max_res_mask,
		        ~(PMURES_BIT(RES4329_BBPLL_PWRSW_PU) | PMURES_BIT(RES4329_HT_AVAIL)));
		SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL, PMU_MAX_TRANSITION_DLY);
		ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL4);
		if (xt->fref == 38400)
			tmp = 0x200024C0;
		else if (xt->fref == 37400)
			tmp = 0x20004500;
		else if (xt->fref == 26000)
			tmp = 0x200024C0;
		else
			tmp = 0x200005C0; /* Chip Dflt Settings */
		W_REG(osh, &cc->pllcontrol_data, tmp);
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL5);
		tmp = R_REG(osh, &cc->pllcontrol_data) & PMU1_PLL0_PC5_CLK_DRV_MASK;
		if ((xt->fref == 38400) || (xt->fref == 37400) || (xt->fref == 26000))
			tmp |= 0x15;
		else
			tmp |= 0x25; /* Chip Dflt Settings */
		W_REG(osh, &cc->pllcontrol_data, tmp);
		break;
	case BCM4315_CHIP_ID:
		/* Change the BBPLL drive strength to 2 for all channels */
		buf_strength = 0x222222;
		/* Make sure the PLL is off */
		AND_REG(osh, &cc->min_res_mask, ~(PMURES_BIT(RES4315_HT_AVAIL)));
		AND_REG(osh, &cc->max_res_mask, ~(PMURES_BIT(RES4315_HT_AVAIL)));
		OSL_DELAY(100);

		AND_REG(osh, &cc->min_res_mask, ~(PMURES_BIT(RES4315_BBPLL_PWRSW_PU)));
		AND_REG(osh, &cc->max_res_mask, ~(PMURES_BIT(RES4315_BBPLL_PWRSW_PU)));
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
		AND_REG(osh, &cc->min_res_mask, ~(PMURES_BIT(RES4319_HT_AVAIL)));
		AND_REG(osh, &cc->max_res_mask, ~(PMURES_BIT(RES4319_HT_AVAIL)));

		OSL_DELAY(100);
		AND_REG(osh, &cc->min_res_mask, ~(PMURES_BIT(RES4319_BBPLL_PWRSW_PU)));
		AND_REG(osh, &cc->max_res_mask, ~(PMURES_BIT(RES4319_BBPLL_PWRSW_PU)));

		OSL_DELAY(100);
		SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL, PMU_MAX_TRANSITION_DLY);
		ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL4);
		tmp = 0x200005c0;
		W_REG(osh, &cc->pllcontrol_data, tmp);
		break;

	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
		AND_REG(osh, &cc->min_res_mask,
			~(PMURES_BIT(RES4336_HT_AVAIL) | PMURES_BIT(RES4336_MACPHY_CLKAVAIL)));
		AND_REG(osh, &cc->max_res_mask,
			~(PMURES_BIT(RES4336_HT_AVAIL) | PMURES_BIT(RES4336_MACPHY_CLKAVAIL)));
		OSL_DELAY(100);
		SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL, PMU_MAX_TRANSITION_DLY);
		ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));
		break;

	case BCM4330_CHIP_ID:
		AND_REG(osh, &cc->min_res_mask,
			~(PMURES_BIT(RES4330_HT_AVAIL) | PMURES_BIT(RES4330_MACPHY_CLKAVAIL)));
		AND_REG(osh, &cc->max_res_mask,
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
	W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0);
	tmp = R_REG(osh, &cc->pllcontrol_data) &
	        ~(PMU1_PLL0_PC0_P1DIV_MASK | PMU1_PLL0_PC0_P2DIV_MASK);
	tmp |= ((xt->p1div << PMU1_PLL0_PC0_P1DIV_SHIFT) & PMU1_PLL0_PC0_P1DIV_MASK) |
	        ((xt->p2div << PMU1_PLL0_PC0_P2DIV_SHIFT) & PMU1_PLL0_PC0_P2DIV_MASK);
	W_REG(osh, &cc->pllcontrol_data, tmp);

	if ((CHIPID(sih->chip) == BCM4330_CHIP_ID)) {
		if (CHIPREV(sih->chiprev) < 2)
			dacrate = 160;
		else {
			if (!(dacrate = (uint8)getintvar(NULL, "dacrate2g")))
				dacrate = 80;
		}
		si_pmu_set_4330_plldivs(sih, dacrate);
	}

	if ((CHIPID(sih->chip) == BCM4329_CHIP_ID) && (CHIPREV(sih->chiprev) == 0)) {

		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
		tmp = R_REG(osh, &cc->pllcontrol_data);
		tmp = tmp & (~DOT11MAC_880MHZ_CLK_DIVISOR_MASK);
		tmp = tmp | DOT11MAC_880MHZ_CLK_DIVISOR_VAL;
		W_REG(osh, &cc->pllcontrol_data, tmp);
	}
	if ((CHIPID(sih->chip) == BCM4319_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4336_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43362_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4330_CHIP_ID))
		ndiv_mode = PMU1_PLL0_PC2_NDIV_MODE_MFB;
	else
		ndiv_mode = PMU1_PLL0_PC2_NDIV_MODE_MASH;

	/* Write ndiv_int and ndiv_mode to pllcontrol[2] */
	W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
	tmp = R_REG(osh, &cc->pllcontrol_data) &
	        ~(PMU1_PLL0_PC2_NDIV_INT_MASK | PMU1_PLL0_PC2_NDIV_MODE_MASK);
	tmp |= ((xt->ndiv_int << PMU1_PLL0_PC2_NDIV_INT_SHIFT) & PMU1_PLL0_PC2_NDIV_INT_MASK) |
	        ((ndiv_mode << PMU1_PLL0_PC2_NDIV_MODE_SHIFT) & PMU1_PLL0_PC2_NDIV_MODE_MASK);
	W_REG(osh, &cc->pllcontrol_data, tmp);

	/* Write ndiv_frac to pllcontrol[3] */
	W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
	tmp = R_REG(osh, &cc->pllcontrol_data) & ~PMU1_PLL0_PC3_NDIV_FRAC_MASK;
	tmp |= ((xt->ndiv_frac << PMU1_PLL0_PC3_NDIV_FRAC_SHIFT) &
	        PMU1_PLL0_PC3_NDIV_FRAC_MASK);
	W_REG(osh, &cc->pllcontrol_data, tmp);

	/* Write clock driving strength to pllcontrol[5] */
	if (buf_strength) {
		PMU_MSG(("Adjusting PLL buffer drive strength: %x\n", buf_strength));

		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL5);
		tmp = R_REG(osh, &cc->pllcontrol_data) & ~PMU1_PLL0_PC5_CLK_DRV_MASK;
		tmp |= (buf_strength << PMU1_PLL0_PC5_CLK_DRV_SHIFT);
		W_REG(osh, &cc->pllcontrol_data, tmp);
	}

	PMU_MSG(("Done pll\n"));

	/* to operate the 4319 usb in 24MHz/48MHz; chipcontrol[2][84:83] needs
	 * to be updated.
	 */
	if ((CHIPID(sih->chip) == BCM4319_CHIP_ID) && (xt->fref != XTAL_FREQ_30000MHZ)) {
		W_REG(osh, &cc->chipcontrol_addr, PMU1_PLL0_CHIPCTL2);
		tmp = R_REG(osh, &cc->chipcontrol_data) & ~CCTL_4319USB_XTAL_SEL_MASK;
		if (xt->fref == XTAL_FREQ_24000MHZ) {
			tmp |= (CCTL_4319USB_24MHZ_PLL_SEL << CCTL_4319USB_XTAL_SEL_SHIFT);
		} else if (xt->fref == XTAL_FREQ_48000MHZ) {
			tmp |= (CCTL_4319USB_48MHZ_PLL_SEL << CCTL_4319USB_XTAL_SEL_SHIFT);
		}
		W_REG(osh, &cc->chipcontrol_data, tmp);
	}

	/* Flush deferred pll control registers writes */
	if (sih->pmurev >= 2)
		OR_REG(osh, &cc->pmucontrol, PCTL_PLL_PLLCTL_UPD);

	/* Write XtalFreq. Set the divisor also. */
	tmp = R_REG(osh, &cc->pmucontrol) &
	        ~(PCTL_ILP_DIV_MASK | PCTL_XTALFREQ_MASK);
	tmp |= (((((xt->fref + 127) / 128) - 1) << PCTL_ILP_DIV_SHIFT) &
	        PCTL_ILP_DIV_MASK) |
	       ((xt->xf << PCTL_XTALFREQ_SHIFT) & PCTL_XTALFREQ_MASK);

	if ((CHIPID(sih->chip) == BCM4329_CHIP_ID) && CHIPREV(sih->chiprev) == 0) {
		/* clear the htstretch before clearing HTReqEn */
		AND_REG(osh, &cc->clkstretch, ~CSTRETCH_HT);
		tmp &= ~PCTL_HT_REQ_EN;
	}

	W_REG(osh, &cc->pmucontrol, tmp);
}

/* query the CPU clock frequency */
static uint32
BCMINITFN(si_pmu1_cpuclk0)(si_t *sih, osl_t *osh, chipcregs_t *cc)
{
	uint32 tmp, mdiv = 1;
#ifndef WLRXOE
#ifdef BCMDBG
	uint32 ndiv_int, ndiv_frac, p2div, p1div, fvco;
	uint32 fref;
#endif
#endif /* WLRXOE */
#ifdef BCMDBG
	char chn[8];
#endif
	uint32 FVCO = si_pmu1_pllfvco0(sih);

	switch (CHIPID(sih->chip)) {
	case BCM4325_CHIP_ID:
	case BCM4329_CHIP_ID:
	case BCM4315_CHIP_ID:
	case BCM4319_CHIP_ID:
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
	case BCM4330_CHIP_ID:
		/* Read m1div from pllcontrol[1] */
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
		tmp = R_REG(osh, &cc->pllcontrol_data);
		mdiv = (tmp & PMU1_PLL0_PC1_M1DIV_MASK) >> PMU1_PLL0_PC1_M1DIV_SHIFT;
		break;
	case BCM43239_CHIP_ID:
		/* Read m6div from pllcontrol[2] */
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
		tmp = R_REG(osh, &cc->pllcontrol_data);
		mdiv = (tmp & PMU1_PLL0_PC2_M6DIV_MASK) >> PMU1_PLL0_PC2_M6DIV_SHIFT;
		break;
	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
		/* Read m2div from pllcontrol[1] */
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
		tmp = R_REG(osh, &cc->pllcontrol_data);
		mdiv = (tmp & PMU1_PLL0_PC1_M2DIV_MASK) >> PMU1_PLL0_PC1_M2DIV_SHIFT;
		break;
	case BCM4335_CHIP_ID:
		/* Read m3div from pllcontrol[1] */
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
		tmp = R_REG(osh, &cc->pllcontrol_data);
		mdiv = (tmp & PMU1_PLL0_PC1_M3DIV_MASK) >> PMU1_PLL0_PC1_M3DIV_SHIFT;
		break;
	case BCM4360_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM43526_CHIP_ID:
		/* Read m3div from pllcontrol[1] */
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL5);
		tmp = R_REG(osh, &cc->pllcontrol_data);
		mdiv = (tmp & PMU1_PLL0_PC2_M6DIV_MASK) >> PMU1_PLL0_PC2_M6DIV_SHIFT;
		break;
	default:
		PMU_MSG(("si_pmu1_cpuclk0: Unknown chipid %s\n", bcm_chipname(sih->chip, chn, 8)));
		ASSERT(0);
		break;
	}
#ifndef WLRXOE
#ifdef BCMDBG
	/* Read p2div/p1div from pllcontrol[0] */
	W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0);
	tmp = R_REG(osh, &cc->pllcontrol_data);
	p2div = (tmp & PMU1_PLL0_PC0_P2DIV_MASK) >> PMU1_PLL0_PC0_P2DIV_SHIFT;
	p1div = (tmp & PMU1_PLL0_PC0_P1DIV_MASK) >> PMU1_PLL0_PC0_P1DIV_SHIFT;

	/* Calculate fvco based on xtal freq and ndiv and pdiv */
	W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
	tmp = R_REG(osh, &cc->pllcontrol_data);
	ndiv_int = (tmp & PMU1_PLL0_PC2_NDIV_INT_MASK) >> PMU1_PLL0_PC2_NDIV_INT_SHIFT;

	W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
	tmp = R_REG(osh, &cc->pllcontrol_data);
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
#endif /* WLRXOE */
	/* Return ARM/SB clock */
	return FVCO / mdiv * 1000;
}

bool
si_pmu_is_autoresetphyclk_disabled(si_t *sih, osl_t *osh)
{
	chipcregs_t *cc;
	uint origidx;
	bool disable = FALSE;

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	switch (CHIPID(sih->chip)) {
	case BCM43239_CHIP_ID:
		W_REG(osh, &cc->chipcontrol_addr, 0);
		if (R_REG(osh, &cc->chipcontrol_data) & 0x00000002)
			disable = TRUE;
		break;
	default:
		break;
	}

	/* Return to original core */
	si_setcoreidx(sih, origidx);
	return disable;
}

static void
BCMATTACHFN(si_set_bb_vcofreq_frac)(si_t *sih, osl_t *osh, int vcofreq, int frac, int xtalfreq)
{
	uint vcofreq_withfrac, p1div, ndiv_int, is_frac, ndiv_mode, val;
	uint frac1, frac2, fraca;
	chipcregs_t *cc;

	cc = si_setcoreidx(sih, SI_CC_IDX);
	if (R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL)
	{
		PMU_MSG(("HTAVAIL is set, so not updating BBPLL Frequency \n"));
		return;
	}

	vcofreq_withfrac = vcofreq * 10000 + frac;
	p1div = 0x1;
	ndiv_int = vcofreq / xtalfreq;
	is_frac = (vcofreq_withfrac % (xtalfreq * 10000)) ? 1 : 0;
	ndiv_mode = is_frac ? 3 : 0;
	PMU_ERROR(("ChangeVCO => vco:%d, xtalF:%d, frac: %d, ndivMode: %d, ndivint: %d\n",
		vcofreq, xtalfreq, frac, ndiv_mode, ndiv_int));

	val = (ndiv_int << 7) | (ndiv_mode << 4) | p1div;
	PMU_ERROR(("Data written into the PLL_CNTRL_ADDR2: %08x\n", val));
	si_pmu_pllcontrol(sih, 2, ~0, val);

	if (is_frac) {
		frac1 = (vcofreq % xtalfreq) * (1 << 24) / xtalfreq;
		frac2 = (frac % (xtalfreq * 10000)) * (1 << 24) / (xtalfreq * 10000);
		fraca = frac1 + frac2;
		si_pmu_pllcontrol(sih, 3, ~0, fraca);
		PMU_MSG(("Data written into the PLL_CNTRL_ADDR3 (Fractional): %08x\n", fraca));
	}
	si_pmu_pllupd(sih);
}

/* return vcofreq with fraction in 100Hz, now only work on 4360 */
uint32
si_pmu_get_bb_vcofreq(si_t *sih, osl_t *osh, int xtalfreq)
{
	uint32 ndiv_int, is_frac, frac = 0, vcofreq, data;

	if ((CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43460_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43526_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM4352_CHIP_ID)) {
		data = si_pmu_pllcontrol(sih, 2, 0, 0);
		ndiv_int = data >> 7;
		is_frac = (data >> 4) & 7;

		if (is_frac) {
			/* vcofreq fraction = (10000 * data * xtalfreq + (1 << 23)) / (1 << 24);
			 * handle overflow
			 */
			uint32 r1, r0;

			data = si_pmu_pllcontrol(sih, 3, 0, 0);

			bcm_uint64_multiple_add(&r1, &r0, 10000 * xtalfreq, data, 1 << 23);
			frac = (r1 << 8) | (r0 >> 24);
		}

		if (xtalfreq > (int)((0xffffffff - frac) / (10000 * ndiv_int))) {
			PMU_ERROR(("%s: xtalfreq is too big, %d\n", __FUNCTION__, xtalfreq));
			return 0;
		}

		vcofreq = xtalfreq * ndiv_int * 10000 + frac;
		return vcofreq;
	} else {
		/* put more chips here */
		PMU_ERROR(("%s: only work on 4360\n", __FUNCTION__));
		return 0;
	}
}

/* initialize PLL */
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

			minmask = R_REG(osh, &cc->min_res_mask);
			maxmask = R_REG(osh, &cc->max_res_mask);

			/* Make sure the PLL is off: clear bit 4 & 5 of min/max_res_mask */
			/* Have to remove HT Avail request before powering off PLL */
			AND_REG(osh, &cc->min_res_mask,	~(PMURES_BIT(RES4322_HT_SI_AVAIL)));
			AND_REG(osh, &cc->max_res_mask,	~(PMURES_BIT(RES4322_HT_SI_AVAIL)));
			SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL, PMU_MAX_TRANSITION_DLY);
			AND_REG(osh, &cc->min_res_mask,	~(PMURES_BIT(RES4322_SI_PLL_ON)));
			AND_REG(osh, &cc->max_res_mask,	~(PMURES_BIT(RES4322_SI_PLL_ON)));
			OSL_DELAY(1000);
			ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));


			W_REG(osh, &cc->pllcontrol_addr, PMU2_SI_PLL_PLLCTL);
			W_REG(osh, &cc->pllcontrol_data, 0x380005c0);


			OSL_DELAY(100);
			W_REG(osh, &cc->max_res_mask, maxmask);
			OSL_DELAY(100);
			W_REG(osh, &cc->min_res_mask, minmask);
			OSL_DELAY(100);
		}

		break;
	}

	case BCM4360_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM4352_CHIP_ID: {
		if (CHIPREV(sih->chiprev) > 2)
			si_set_bb_vcofreq_frac(sih, osh, 960, 98, 40);
		break;
	}

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
		si_pmu1_pllinit1(sih, osh, cc, xtalfreq);
		break;
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
	case BCM43143_CHIP_ID:
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

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

/* query alp/xtal clock frequency */
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
		clock = si_pmu1_alpclk0(sih, osh, cc);
		break;
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
	case BCM43143_CHIP_ID:
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
}

/* Find the output of the "m" pll divider given pll controls that start with
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

	W_REG(osh, &cc->pllcontrol_addr, pll0 + PMU5_PLL_P1P2_OFF);
	(void)R_REG(osh, &cc->pllcontrol_addr);
	tmp = R_REG(osh, &cc->pllcontrol_data);
	p1 = (tmp & PMU5_PLL_P1_MASK) >> PMU5_PLL_P1_SHIFT;
	p2 = (tmp & PMU5_PLL_P2_MASK) >> PMU5_PLL_P2_SHIFT;

	W_REG(osh, &cc->pllcontrol_addr, pll0 + PMU5_PLL_M14_OFF);
	(void)R_REG(osh, &cc->pllcontrol_addr);
	tmp = R_REG(osh, &cc->pllcontrol_data);
	div = (tmp >> ((m - 1) * PMU5_PLL_MDIV_WIDTH)) & PMU5_PLL_MDIV_MASK;

	W_REG(osh, &cc->pllcontrol_addr, pll0 + PMU5_PLL_NM5_OFF);
	(void)R_REG(osh, &cc->pllcontrol_addr);
	tmp = R_REG(osh, &cc->pllcontrol_data);
	ndiv = (tmp & PMU5_PLL_NDIV_MASK) >> PMU5_PLL_NDIV_SHIFT;

	/* Do calculation in Mhz */
	fc = si_pmu_alp_clock(sih, osh) / 1000000;
	fc = (p1 * ndiv * fc) / p2;

	PMU_NONE(("%s: p1=%d, p2=%d, ndiv=%d(0x%x), m%d=%d; fc=%d, clock=%d\n",
	          __FUNCTION__, p1, p2, ndiv, ndiv, m, div, fc, fc / div));

	/* Return clock in Hertz */
	return ((fc / div) * 1000000);
}

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
	W_REG(osh, &cc->pllcontrol_addr, pll0 + PMU6_4706_PROCPLL_OFF);
	w = R_REG(NULL, &cc->pllcontrol_data);
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

/* query backplane clock frequency */
/* For designs that feed the same clock to both backplane
 * and CPU just return the CPU clock speed.
 */
uint32
BCMINITFN(si_pmu_si_clock)(si_t *sih, osl_t *osh)
{
	chipcregs_t *cc;
	uint origidx;
	uint32 clock = HT_CLOCK;
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
	case BCM4360_CHIP_ID:
	case BCM4350_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM43526_CHIP_ID:
		clock = si_pmu1_cpuclk0(sih, osh, cc);
		break;

	case BCM4313_CHIP_ID:
		/* 80MHz backplane clock */
		clock = 80000 * 1000;
		break;
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
	case BCM43143_CHIP_ID: /* HT clock and ARM clock have the same frequency */
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
}

/* query CPU clock frequency */
uint32
BCMINITFN(si_pmu_cpu_clock)(si_t *sih, osl_t *osh)
{
	chipcregs_t *cc;
	uint origidx;
	uint32 clock;

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
		(CHIPID(sih->chip) == BCM4334_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4324_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43242_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43243_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4330_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43526_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43460_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4352_CHIP_ID) ||
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
		else
			clock = si_pmu5_clock(sih, osh, cc, pll, PMU5_MAINPLL_CPU);

		/* Return to original core */
		si_setcoreidx(sih, origidx);
	} else
		clock = si_pmu_si_clock(sih, osh);

	return clock;
}

/* query memory clock frequency */
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
}

/* Measure ILP clock frequency */
#define ILP_CALC_DUR	10	/* ms, make sure 1000 can be divided by it. */

static uint32 ilpcycles_per_sec = 0;

uint32
BCMINITFN(si_pmu_ilp_clock)(si_t *sih, osl_t *osh)
{
	if (ISSIM_ENAB(sih))
		return ILP_CLOCK;

	if (ilpcycles_per_sec == 0) {
		uint32 start, end, delta;
		uint32 origidx = si_coreidx(sih);
		chipcregs_t *cc = si_setcoreidx(sih, SI_CC_IDX);
		ASSERT(cc != NULL);
		start = R_REG(osh, &cc->pmutimer);
		if (start != R_REG(osh, &cc->pmutimer))
			start = R_REG(osh, &cc->pmutimer);
		OSL_DELAY(ILP_CALC_DUR * 1000);
		end = R_REG(osh, &cc->pmutimer);
		if (end != R_REG(osh, &cc->pmutimer))
			end = R_REG(osh, &cc->pmutimer);
		delta = end - start;
		ilpcycles_per_sec = delta * (1000 / ILP_CALC_DUR);
		si_setcoreidx(sih, origidx);
	}

	return ilpcycles_per_sec;
}

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


#define SDIOD_DRVSTR_KEY(chip, pmu)	(((chip) << 16) | (pmu))

void
BCMINITFN(si_sdiod_drive_strength_init)(si_t *sih, osl_t *osh, uint32 drivestrength)
{
	chipcregs_t *cc;
	uint origidx, intr_val = 0;
	sdiod_drive_str_t *str_tab = NULL;
	uint32 str_mask = 0;
	uint32 str_shift = 0;
#ifdef BCMDBG
	char chn[8];
#endif

	if (!(sih->cccaps & CC_CAP_PMU)) {
		return;
	}

	/* Remember original core before switch to chipc */
	cc = (chipcregs_t *) si_switch_core(sih, CC_CORE_ID, &origidx, &intr_val);

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
	default:
		PMU_MSG(("No SDIO Drive strength init done for chip %s rev %d pmurev %d\n",
		         bcm_chipname(sih->chip, chn, 8), sih->chiprev, sih->pmurev));

		break;
	}

	if (str_tab != NULL && cc != NULL) {
		uint32 cc_data_temp;
		int i;

		/* Pick the lowest available drive strength equal or greater than the
		 * requested strength.	Drive strength of 0 requests tri-state.
		 */
		for (i = 0; drivestrength < str_tab[i].strength; i++)
			;

		if (i > 0 && drivestrength > str_tab[i].strength)
			i--;

		W_REG(osh, &cc->chipcontrol_addr, 1);
		cc_data_temp = R_REG(osh, &cc->chipcontrol_data);
		cc_data_temp &= ~str_mask;
		cc_data_temp |= str_tab[i].sel << str_shift;
		W_REG(osh, &cc->chipcontrol_data, cc_data_temp);

		PMU_MSG(("SDIO: %dmA drive strength requested; set to %dmA\n",
		         drivestrength, str_tab[i].strength));
	}

	/* Return to original core */
	si_restore_core(sih, origidx, intr_val);
}

/* initialize PMU */
void
BCMATTACHFN(si_pmu_init)(si_t *sih, osl_t *osh)
{
	chipcregs_t *cc;
	uint origidx;

	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	if (sih->pmurev == 1)
		AND_REG(osh, &cc->pmucontrol, ~PCTL_NOILP_ON_WAIT);
	else if (sih->pmurev >= 2)
		OR_REG(osh, &cc->pmucontrol, PCTL_NOILP_ON_WAIT);

	if ((CHIPID(sih->chip) == BCM4329_CHIP_ID) && (sih->chiprev == 2)) {
		/* Fix for 4329b0 bad LPOM state. */
		W_REG(osh, &cc->regcontrol_addr, 2);
		OR_REG(osh, &cc->regcontrol_data, 0x100);

		W_REG(osh, &cc->regcontrol_addr, 3);
		OR_REG(osh, &cc->regcontrol_data, 0x4);
	}

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

/* Return worst case up time in [ILP cycles] for the given resource. */
static uint
BCMINITFN(si_pmu_res_uptime)(si_t *sih, osl_t *osh, chipcregs_t *cc, uint8 rsrc)
{
	uint32 deps;
	uint uptime, i, dup, dmax;
	uint32 min_mask = 0, max_mask = 0;

	/* uptime of resource 'rsrc' */
	W_REG(osh, &cc->res_table_sel, rsrc);
	if (sih->pmurev >= 13)
		uptime = (R_REG(osh, &cc->res_updn_timer) >> 16) & 0x3ff;
	else
		uptime = (R_REG(osh, &cc->res_updn_timer) >> 8) & 0xff;

	/* direct dependencies of resource 'rsrc' */
	deps = si_pmu_res_deps(sih, osh, cc, PMURES_BIT(rsrc), FALSE);
	for (i = 0; i <= PMURES_MAX_RESNUM; i ++) {
		if (!(deps & PMURES_BIT(i)))
			continue;
		deps &= ~si_pmu_res_deps(sih, osh, cc, PMURES_BIT(i), TRUE);
	}
	si_pmu_res_masks(sih, &min_mask, &max_mask);
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
		W_REG(osh, &cc->res_table_sel, i);
		deps |= R_REG(osh, &cc->res_dep_mask);
	}

	return !all ? deps : (deps ? (deps | si_pmu_res_deps(sih, osh, cc, deps, TRUE)) : 0);
}

/* power up/down OTP through PMU resources */
void
si_pmu_otp_power(si_t *sih, osl_t *osh, bool on)
{
	chipcregs_t *cc;
	uint origidx;
	uint32 rsrcs = 0;	/* rsrcs to turn on/off OTP power */

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

	switch (CHIPID(sih->chip)) {
	case BCM4322_CHIP_ID:
	case BCM43221_CHIP_ID:
	case BCM43231_CHIP_ID:
	case BCM4342_CHIP_ID:
		rsrcs = PMURES_BIT(RES4322_OTP_PU);
		break;
	case BCM4325_CHIP_ID:
		rsrcs = PMURES_BIT(RES4325_OTP_PU);
		break;
	case BCM4315_CHIP_ID:
		rsrcs = PMURES_BIT(RES4315_OTP_PU);
		break;
	case BCM4329_CHIP_ID:
		rsrcs = PMURES_BIT(RES4329_OTP_PU);
		break;
	case BCM4319_CHIP_ID:
		rsrcs = PMURES_BIT(RES4319_OTP_PU);
		break;
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
		rsrcs = PMURES_BIT(RES4336_OTP_PU);
		break;
	case BCM4330_CHIP_ID:
		rsrcs = PMURES_BIT(RES4330_OTP_PU);
		break;
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
		rsrcs = PMURES_BIT(RES4314_OTP_PU);
		break;
	case BCM43143_CHIP_ID:
		rsrcs = PMURES_BIT(RES43143_OTP_PU);
		break;
	case BCM4334_CHIP_ID:
		rsrcs = PMURES_BIT(RES4334_OTP_PU);
		break;
	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
		rsrcs = PMURES_BIT(RES4324_OTP_PU);
		break;
	case BCM4335_CHIP_ID:
		rsrcs = PMURES_BIT(RES4335_OTP_PU);
		break;
	case BCM4360_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM4352_CHIP_ID:
	case BCM43526_CHIP_ID:
		rsrcs = PMURES_BIT(RES4360_OTP_PU);
		break;
	default:
		break;
	}

	if (rsrcs != 0) {
		uint32 otps;

		/* Figure out the dependencies (exclude min_res_mask) */
		uint32 deps = si_pmu_res_deps(sih, osh, cc, rsrcs, TRUE);
		uint32 min_mask = 0, max_mask = 0;
		si_pmu_res_masks(sih, &min_mask, &max_mask);
		deps &= ~min_mask;
		/* Turn on/off the power */
		if (on) {
			PMU_MSG(("Adding rsrc 0x%x to min_res_mask\n", rsrcs | deps));
			OR_REG(osh, &cc->min_res_mask, (rsrcs | deps));
			OSL_DELAY(1000);
			SPINWAIT(!(R_REG(osh, &cc->res_state) & rsrcs), PMU_MAX_TRANSITION_DLY);
			ASSERT(R_REG(osh, &cc->res_state) & rsrcs);
		}
		else {
			PMU_MSG(("Removing rsrc 0x%x from min_res_mask\n", rsrcs | deps));
			AND_REG(osh, &cc->min_res_mask, ~(rsrcs | deps));
		}

		SPINWAIT((((otps = R_REG(osh, &cc->otpstatus)) & OTPS_READY) !=
			(on ? OTPS_READY : 0)), 200);
		ASSERT((otps & OTPS_READY) == (on ? OTPS_READY : 0));
		if ((otps & OTPS_READY) != (on ? OTPS_READY : 0))
			PMU_MSG(("OTP ready bit not %s after wait\n", (on ? "ON" : "OFF")));
	}

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

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
		W_REG(osh, &cc->chipcontrol_addr, 1);

		/* Power Down RCAL Block */
		AND_REG(osh, &cc->chipcontrol_data, ~0x04);

		/* Check if RCAL is already done by BT */
		rcal_done = ((R_REG(osh, &cc->chipstatus)) & 0x8) >> 3;

		/* If RCAL already done, note that BT is out of reset */
		if (rcal_done == 1) {
			BT_out_of_reset = 1;
		} else {
			BT_out_of_reset = 0;
		}

		/* Power Up RCAL block */
		OR_REG(osh, &cc->chipcontrol_data, 0x04);

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
		W_REG(osh, &cc->regcontrol_addr, 0);
		val = R_REG(osh, &cc->regcontrol_data) & ~((uint32)0x07 << 29);
		val |= (uint32)(rcal_code & 0x07) << 29;
		W_REG(osh, &cc->regcontrol_data, val);
		W_REG(osh, &cc->regcontrol_addr, 1);
		val = R_REG(osh, &cc->regcontrol_data) & ~(uint32)0x01;
		val |= (uint32)((rcal_code >> 3) & 0x01);
		W_REG(osh, &cc->regcontrol_data, val);

		/* Write RCal code into pmu_chip_ctrl[33:30] */
		W_REG(osh, &cc->chipcontrol_addr, 0);
		val = R_REG(osh, &cc->chipcontrol_data) & ~((uint32)0x03 << 30);
		val |= (uint32)(rcal_code & 0x03) << 30;
		W_REG(osh, &cc->chipcontrol_data, val);
		W_REG(osh, &cc->chipcontrol_addr, 1);
		val = R_REG(osh, &cc->chipcontrol_data) & ~(uint32)0x03;
		val |= (uint32)((rcal_code >> 2) & 0x03);
		W_REG(osh, &cc->chipcontrol_data, val);

		/* Set override in pmu_chip_ctrl[29] */
		W_REG(osh, &cc->chipcontrol_addr, 0);
		OR_REG(osh, &cc->chipcontrol_data, (0x01 << 29));

		/* Power off RCal block */
		W_REG(osh, &cc->chipcontrol_addr, 1);
		AND_REG(osh, &cc->chipcontrol_data, ~0x04);

		break;
	}
	case BCM4329_CHIP_ID: {
		uint8 rcal_code;
		uint32 val;

		/* Kick RCal */
		W_REG(osh, &cc->chipcontrol_addr, 1);

		/* Power Down RCAL Block */
		AND_REG(osh, &cc->chipcontrol_data, ~0x04);

		/* Power Up RCAL block */
		OR_REG(osh, &cc->chipcontrol_data, 0x04);

		/* Wait for completion */
		SPINWAIT(!(R_REG(osh, &cc->chipstatus) & 0x08), 10 * 1000 * 1000);
		ASSERT(R_REG(osh, &cc->chipstatus) & 0x08);

		/* Drop the LSB to convert from 5 bit code to 4 bit code */
		rcal_code =  (uint8)(R_REG(osh, &cc->chipstatus) >> 5) & 0x0f;

		PMU_MSG(("RCal completed, status 0x%x, code 0x%x\n",
			R_REG(osh, &cc->chipstatus), rcal_code));

		/* Write RCal code into pmu_vreg_ctrl[32:29] */
		W_REG(osh, &cc->regcontrol_addr, 0);
		val = R_REG(osh, &cc->regcontrol_data) & ~((uint32)0x07 << 29);
		val |= (uint32)(rcal_code & 0x07) << 29;
		W_REG(osh, &cc->regcontrol_data, val);
		W_REG(osh, &cc->regcontrol_addr, 1);
		val = R_REG(osh, &cc->regcontrol_data) & ~(uint32)0x01;
		val |= (uint32)((rcal_code >> 3) & 0x01);
		W_REG(osh, &cc->regcontrol_data, val);

		/* Write RCal code into pmu_chip_ctrl[33:30] */
		W_REG(osh, &cc->chipcontrol_addr, 0);
		val = R_REG(osh, &cc->chipcontrol_data) & ~((uint32)0x03 << 30);
		val |= (uint32)(rcal_code & 0x03) << 30;
		W_REG(osh, &cc->chipcontrol_data, val);
		W_REG(osh, &cc->chipcontrol_addr, 1);
		val = R_REG(osh, &cc->chipcontrol_data) & ~(uint32)0x03;
		val |= (uint32)((rcal_code >> 2) & 0x03);
		W_REG(osh, &cc->chipcontrol_data, val);

		/* Set override in pmu_chip_ctrl[29] */
		W_REG(osh, &cc->chipcontrol_addr, 0);
		OR_REG(osh, &cc->chipcontrol_data, (0x01 << 29));

		/* Power off RCal block */
		W_REG(osh, &cc->chipcontrol_addr, 1);
		AND_REG(osh, &cc->chipcontrol_data, ~0x04);

		break;
	}
	default:
		break;
	}

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

/* only called for HT, LCN and N phy's. */
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
		(CHIPID(sih->chip) == BCM4335_CHIP_ID) ||
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
}

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
si_pmu_pllctrl_spurupdate(osl_t *osh, chipcregs_t *cc, uint8 spuravoid,
	const pllctrl_spuravoid_t *pllctrl_spur, uint32 array_size)
{
	uint8 indx;
	for (indx = 0; indx < array_size; indx++) {
		if (pllctrl_spur[indx].spuravoid_mode == spuravoid) {
			W_REG(osh, &cc->pllcontrol_addr, pllctrl_spur[indx].pllctrl_reg);
			W_REG(osh, &cc->pllcontrol_data, pllctrl_spur[indx].pllctrl_regval);
		}
	}
}

static void
si_pmu_spuravoid_pllupdate(si_t *sih, chipcregs_t *cc, osl_t *osh, uint8 spuravoid)
{
	uint32 tmp = 0;
	uint8 phypll_offset = 0;
	uint8 bcm5357_bcm43236_p1div[] = {0x1, 0x5, 0x5};
	uint8 bcm5357_bcm43236_ndiv[] = {0x30, 0xf6, 0xfc};
#ifdef BCMDBG_ERR
	char chn[8];
#endif
	uint32 xtal_freq, reg_val, mxdiv, ndiv_int, ndiv_frac_int, part_mul;
	uint8 p1_div, p2_div, FCLkx;
	const pmu1_xtaltab0_t *params_tbl;

	ASSERT(CHIPID(sih->chip) != BCM43143_CHIP_ID); /* LCN40 PHY */

	switch (CHIPID(sih->chip)) {
	case BCM4324_CHIP_ID:
		/* If we get an invalid spurmode, then set the spur0 settings. */
		if (spuravoid > 2)
			spuravoid = 0;

		si_pmu_pllctrl_spurupdate(osh, cc, spuravoid, spuravoid_4324,
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
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0);
				W_REG(osh, &cc->pllcontrol_data, 0x11500010);
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
				W_REG(osh, &cc->pllcontrol_data, 0x000C0C06);
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
				W_REG(osh, &cc->pllcontrol_data, 0x0F600a08);
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
				W_REG(osh, &cc->pllcontrol_data, 0x00000000);
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL4);
				W_REG(osh, &cc->pllcontrol_data, 0x2001E920);
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL5);
				W_REG(osh, &cc->pllcontrol_data, 0x88888815);
			} else {
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0);
				W_REG(osh, &cc->pllcontrol_data, 0x11100010);
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
				W_REG(osh, &cc->pllcontrol_data, 0x000c0c06);
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
				W_REG(osh, &cc->pllcontrol_data, 0x03000a08);
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
				W_REG(osh, &cc->pllcontrol_data, 0x00000000);
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL4);
				W_REG(osh, &cc->pllcontrol_data, 0x200005c0);
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL5);
				W_REG(osh, &cc->pllcontrol_data, 0x88888815);
			}

		} else {
			/* 5357[ab]0, 43236[ab]0, and 6362b0 */

			/* BCM5357 needs to touch PLL1_PLLCTL[02], so offset PLL0_PLLCTL[02] by 6 */
			phypll_offset = ((CHIPID(sih->chip) == BCM5357_CHIP_ID) ||
			       (CHIPID(sih->chip) == BCM4749_CHIP_ID) ||
			       (CHIPID(sih->chip) == BCM53572_CHIP_ID)) ? 6 : 0;

			/* RMW only the P1 divider */
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0 + phypll_offset);
			tmp = R_REG(osh, &cc->pllcontrol_data);
			tmp &= (~(PMU1_PLL0_PC0_P1DIV_MASK));
			tmp |= (bcm5357_bcm43236_p1div[spuravoid] << PMU1_PLL0_PC0_P1DIV_SHIFT);
			W_REG(osh, &cc->pllcontrol_data, tmp);

			/* RMW only the int feedback divider */
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2 + phypll_offset);
			tmp = R_REG(osh, &cc->pllcontrol_data);
			tmp &= ~(PMU1_PLL0_PC2_NDIV_INT_MASK);
			tmp |= (bcm5357_bcm43236_ndiv[spuravoid]) << PMU1_PLL0_PC2_NDIV_INT_SHIFT;
			W_REG(osh, &cc->pllcontrol_data, tmp);
		}

		tmp = 1 << 10;
		break;

	case BCM4331_CHIP_ID:
	case BCM43431_CHIP_ID:
		if (ISSIM_ENAB(sih)) {
			if (spuravoid == 2) {
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
				W_REG(osh, &cc->pllcontrol_data, 0x00000002);
			} else if (spuravoid == 1) {
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
				W_REG(osh, &cc->pllcontrol_data, 0x00000001);
			} else {
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
				W_REG(osh, &cc->pllcontrol_data, 0x00000000);
			}
		} else {
			if (spuravoid == 2) {
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0);
				W_REG(osh, &cc->pllcontrol_data, 0x11500014);
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
				W_REG(osh, &cc->pllcontrol_data, 0x0FC00a08);
			} else if (spuravoid == 1) {
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0);
				W_REG(osh, &cc->pllcontrol_data, 0x11500014);
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
				W_REG(osh, &cc->pllcontrol_data, 0x0F600a08);
			} else {
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0);
				W_REG(osh, &cc->pllcontrol_data, 0x11100014);
				W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
				W_REG(osh, &cc->pllcontrol_data, 0x03000a08);
			}
		}
		tmp = 1 << 10;
		break;

	case BCM43224_CHIP_ID:	case BCM43225_CHIP_ID:	case BCM43421_CHIP_ID:
	case BCM43226_CHIP_ID:
		if (spuravoid == 1) {
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0);
			W_REG(osh, &cc->pllcontrol_data, 0x11500010);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
			W_REG(osh, &cc->pllcontrol_data, 0x000C0C06);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, &cc->pllcontrol_data, 0x0F600a08);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, &cc->pllcontrol_data, 0x00000000);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL4);
			W_REG(osh, &cc->pllcontrol_data, 0x2001E920);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL5);
			W_REG(osh, &cc->pllcontrol_data, 0x88888815);
		} else {
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0);
			W_REG(osh, &cc->pllcontrol_data, 0x11100010);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
			W_REG(osh, &cc->pllcontrol_data, 0x000c0c06);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, &cc->pllcontrol_data, 0x03000a08);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, &cc->pllcontrol_data, 0x00000000);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL4);
			W_REG(osh, &cc->pllcontrol_data, 0x200005c0);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL5);
			W_REG(osh, &cc->pllcontrol_data, 0x88888815);
		}
		tmp = 1 << 10;
		break;

	case BCM43222_CHIP_ID:	case BCM43111_CHIP_ID:	case BCM43112_CHIP_ID:
	case BCM43420_CHIP_ID:
		if (spuravoid == 1) {
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0);
			W_REG(osh, &cc->pllcontrol_data, 0x11500008);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
			W_REG(osh, &cc->pllcontrol_data, 0x0C000C06);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, &cc->pllcontrol_data, 0x0F600a08);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, &cc->pllcontrol_data, 0x00000000);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL4);
			W_REG(osh, &cc->pllcontrol_data, 0x2001E920);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL5);
			W_REG(osh, &cc->pllcontrol_data, 0x88888815);
		} else {
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0);
			W_REG(osh, &cc->pllcontrol_data, 0x11100008);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
			W_REG(osh, &cc->pllcontrol_data, 0x0c000c06);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, &cc->pllcontrol_data, 0x03000a08);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, &cc->pllcontrol_data, 0x00000000);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL4);
			W_REG(osh, &cc->pllcontrol_data, 0x200005c0);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL5);
			W_REG(osh, &cc->pllcontrol_data, 0x88888855);
		}

		tmp = 1 << 10;
		break;

	case BCM4716_CHIP_ID:
	case BCM4748_CHIP_ID:
	case BCM47162_CHIP_ID:
		if (spuravoid == 1) {
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0);
			W_REG(osh, &cc->pllcontrol_data, 0x11500060);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
			W_REG(osh, &cc->pllcontrol_data, 0x080C0C06);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, &cc->pllcontrol_data, 0x0F600000);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, &cc->pllcontrol_data, 0x00000000);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL4);
			W_REG(osh, &cc->pllcontrol_data, 0x2001E924);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL5);
			W_REG(osh, &cc->pllcontrol_data, 0x88888815);
		} else {
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0);
			W_REG(osh, &cc->pllcontrol_data, 0x11100060);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
			W_REG(osh, &cc->pllcontrol_data, 0x080c0c06);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, &cc->pllcontrol_data, 0x03000000);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, &cc->pllcontrol_data, 0x00000000);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL4);
			W_REG(osh, &cc->pllcontrol_data, 0x200005c0);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL5);
			W_REG(osh, &cc->pllcontrol_data, 0x88888815);
		}

		tmp = 3 << 9;
		break;

	case BCM4322_CHIP_ID:
	case BCM43221_CHIP_ID:
	case BCM43231_CHIP_ID:
	case BCM4342_CHIP_ID:
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0);
		W_REG(osh, &cc->pllcontrol_data, 0x11100070);
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
		W_REG(osh, &cc->pllcontrol_data, 0x1014140a);
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL5);
		W_REG(osh, &cc->pllcontrol_data, 0x88888854);

		if (spuravoid == 1) { /* spur_avoid ON, enable 41/82/164Mhz clock mode */
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, &cc->pllcontrol_data, 0x05201828);
		} else { /* enable 40/80/160Mhz clock mode */
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, &cc->pllcontrol_data, 0x05001828);
		}

		tmp = 1 << 10;
		break;
	case BCM4319_CHIP_ID:
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0);
		W_REG(osh, &cc->pllcontrol_data, 0x11100070);
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
		W_REG(osh, &cc->pllcontrol_data, 0x1014140a);
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL5);
		W_REG(osh, &cc->pllcontrol_data, 0x88888854);

		if (spuravoid == 1) { /* spur_avoid ON, enable 41/82/164Mhz clock mode */
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, &cc->pllcontrol_data, 0x05201828);
		} else { /* enable 40/80/160Mhz clock mode */
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, &cc->pllcontrol_data, 0x05001828);
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
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
		reg_val =  R_REG(osh, &cc->pllcontrol_data);
		mxdiv = (reg_val >> PMU1_PLL0_PC2_M6DIV_SHIFT) & PMU1_PLL0_PC2_M5DIV_MASK;

		if (spuravoid == 1)
			FCLkx = 82;
		else
			FCLkx = 80;

		part_mul = (p1_div / p2_div) * mxdiv;
		ndiv_int = (FCLkx * part_mul * 10)/ (xtal_freq);
		ndiv_frac_int = ((FCLkx * part_mul * 10) % (xtal_freq));
		ndiv_frac_int = ((ndiv_frac_int * 16777216) + (xtal_freq/2)) / (xtal_freq);

		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
		reg_val =  R_REG(osh, &cc->pllcontrol_data);
		W_REG(osh, &cc->pllcontrol_data, ((reg_val & ~PMU1_PLL0_PC2_NDIV_INT_MASK)
					| (ndiv_int << PMU1_PLL0_PC2_NDIV_INT_SHIFT)));
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
		W_REG(osh, &cc->pllcontrol_data, ndiv_frac_int);

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
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0);
			W_REG(osh, &cc->pllcontrol_data, 0x01100014);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
			W_REG(osh, &cc->pllcontrol_data, 0x040C0C06);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, &cc->pllcontrol_data, 0x03140A08);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, &cc->pllcontrol_data, 0x00333333);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL4);
			W_REG(osh, &cc->pllcontrol_data, 0x202C2820);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL5);
			W_REG(osh, &cc->pllcontrol_data, 0x88888815);
		} else {
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0);
			W_REG(osh, &cc->pllcontrol_data, 0x11100014);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
			W_REG(osh, &cc->pllcontrol_data, 0x040c0c06);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
			W_REG(osh, &cc->pllcontrol_data, 0x03000a08);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, &cc->pllcontrol_data, 0x00000000);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL4);
			W_REG(osh, &cc->pllcontrol_data, 0x200005c0);
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL5);
			W_REG(osh, &cc->pllcontrol_data, 0x88888815);
		}
		tmp = 1 << 10;
		break;
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
	case BCM4334_CHIP_ID:
		{
			const pmu2_xtaltab0_t *xt;
			uint32 pll0;

			xtal_freq = si_pmu_alp_clock(sih, osh)/1000;
			xt = (spuravoid == 1) ? pmu2_xtaltab0_adfll_492 : pmu2_xtaltab0_adfll_485;

			for (; xt != NULL && xt->fref != 0; xt++) {
				if (xt->fref == xtal_freq) {
					W_REG(osh, &cc->pllcontrol_addr, PMU15_PLL_PLLCTL0);
					pll0 = R_REG(osh, &cc->pllcontrol_data);
					pll0 &= ~PMU15_PLL_PC0_FREQTGT_MASK;
					pll0 |= (xt->freq_tgt << PMU15_PLL_PC0_FREQTGT_SHIFT);
					W_REG(osh, &cc->pllcontrol_data, pll0);

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
		switch (spuravoid) {
		case 2:
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, &cc->pllcontrol_data, 0x80b1f7c9);
			break;
		case 3:
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, &cc->pllcontrol_data, 0x80c680af);
			break;
		case 4:
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, &cc->pllcontrol_data, 0x80B8D016);
			break;
		case 5:
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, &cc->pllcontrol_data, 0x80CD58FC);
			break;
		case 6:
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, &cc->pllcontrol_data, 0x80D43149);
			break;
		case 7:
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, &cc->pllcontrol_data, 0x80DB0995);
			break;
		case 8:
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, &cc->pllcontrol_data, 0x80E1E1E2);
			break;
		case 9:
			W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
			W_REG(osh, &cc->pllcontrol_data, 0x80E8BA2F);
			break;
		default:
			break;
		}
		tmp = PCTL_PLL_PLLCTL_UPD;
		break;
	default:
		PMU_ERROR(("%s: unknown spuravoidance settings for chip %s, not changing PLL\n",
		           __FUNCTION__, bcm_chipname(sih->chip, chn, 8)));
		break;
	}

	tmp |= R_REG(osh, &cc->pmucontrol);
	W_REG(osh, &cc->pmucontrol, tmp);
}

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

		minmask = R_REG(osh, &cc->min_res_mask);
		maxmask = R_REG(osh, &cc->max_res_mask);

		/* Make sure the PLL is off: clear bit 4 & 5 of min/max_res_mask */
		/* Have to remove HT Avail request before powering off PLL */
		AND_REG(osh, &cc->min_res_mask,	~(PMURES_BIT(RES4322_HT_SI_AVAIL)));
		AND_REG(osh, &cc->max_res_mask,	~(PMURES_BIT(RES4322_HT_SI_AVAIL)));
		SPINWAIT(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL, PMU_MAX_TRANSITION_DLY);
		AND_REG(osh, &cc->min_res_mask,	~(PMURES_BIT(RES4322_SI_PLL_ON)));
		AND_REG(osh, &cc->max_res_mask,	~(PMURES_BIT(RES4322_SI_PLL_ON)));
		OSL_DELAY(150);
		ASSERT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));

		/* Change backplane clock speed from 96 MHz to 80 MHz */
		W_REG(osh, &cc->pllcontrol_addr, PMU2_PLL_PLLCTL2);
		W_REG(osh, &cc->pllcontrol_data, (R_REG(osh, &cc->pllcontrol_data) &
		                                  ~(PMU2_PLL_PC2_M6DIV_MASK)) |
		      (0xc << PMU2_PLL_PC2_M6DIV_SHIFT));

		/* Reduce the driver strengths of the phyclk160, adcclk80, and phyck80
		 * clocks from 0x8 to 0x1
		 */
		W_REG(osh, &cc->pllcontrol_addr, PMU2_PLL_PLLCTL5);
		W_REG(osh, &cc->pllcontrol_data, (R_REG(osh, &cc->pllcontrol_data) &
		                                  ~(PMU2_PLL_PC5_CLKDRIVE_CH1_MASK |
		                                    PMU2_PLL_PC5_CLKDRIVE_CH2_MASK |
		                                    PMU2_PLL_PC5_CLKDRIVE_CH3_MASK |
		                                    PMU2_PLL_PC5_CLKDRIVE_CH4_MASK)) |
		      ((1 << PMU2_PLL_PC5_CLKDRIVE_CH1_SHIFT) |
		       (1 << PMU2_PLL_PC5_CLKDRIVE_CH2_SHIFT) |
		       (1 << PMU2_PLL_PC5_CLKDRIVE_CH3_SHIFT) |
		       (1 << PMU2_PLL_PC5_CLKDRIVE_CH4_SHIFT)));

		W_REG(osh, &cc->pmucontrol, R_REG(osh, &cc->pmucontrol) | PCTL_PLL_PLLCTL_UPD);

		/* Restore min_res_mask and max_res_mask */
		OSL_DELAY(100);
		W_REG(osh, &cc->max_res_mask, maxmask);
		OSL_DELAY(100);
		W_REG(osh, &cc->min_res_mask, minmask);
		OSL_DELAY(100);
		/* Make sure the PLL is on. Spinwait until the HTAvail is True */
		SPINWAIT(!(R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL), PMU_MAX_TRANSITION_DLY);
		ASSERT((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL));

		/* Restore force HT and HT Avail Request on the chipc core */
		W_REG(osh, &cc->clk_ctl_st, cc_clk_ctl_st);

		/* Return to original core */
		si_restore_core(sih, origidx, intr_val);
	}
}

bool
si_pmu_is_otp_powered(si_t *sih, osl_t *osh)
{
	uint idx;
	chipcregs_t *cc;
	bool st;

	/* Remember original core before switch to chipc */
	idx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	switch (CHIPID(sih->chip)) {
	case BCM4322_CHIP_ID:
	case BCM43221_CHIP_ID:
	case BCM43231_CHIP_ID:
	case BCM4342_CHIP_ID:
		st = (R_REG(osh, &cc->res_state) & PMURES_BIT(RES4322_OTP_PU)) != 0;
		break;
	case BCM4325_CHIP_ID:
		st = (R_REG(osh, &cc->res_state) & PMURES_BIT(RES4325_OTP_PU)) != 0;
		break;
	case BCM4329_CHIP_ID:
		st = (R_REG(osh, &cc->res_state) & PMURES_BIT(RES4329_OTP_PU)) != 0;
		break;
	case BCM4315_CHIP_ID:
		st = (R_REG(osh, &cc->res_state) & PMURES_BIT(RES4315_OTP_PU)) != 0;
		break;
	case BCM4319_CHIP_ID:
		st = (R_REG(osh, &cc->res_state) & PMURES_BIT(RES4319_OTP_PU)) != 0;
		break;
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
		st = (R_REG(osh, &cc->res_state) & PMURES_BIT(RES4336_OTP_PU)) != 0;
		break;
	case BCM43239_CHIP_ID:
		st = (R_REG(osh, &cc->res_state) & PMURES_BIT(RES43239_OTP_PU)) != 0;
		break;
	case BCM4330_CHIP_ID:
		st = (R_REG(osh, &cc->res_state) & PMURES_BIT(RES4330_OTP_PU)) != 0;
		break;
	case BCM4314_CHIP_ID:
	case BCM43142_CHIP_ID:
		st = (R_REG(osh, &cc->res_state) & PMURES_BIT(RES4314_OTP_PU)) != 0;
		break;
	case BCM43143_CHIP_ID:
		st = (R_REG(osh, &cc->res_state) & PMURES_BIT(RES43143_OTP_PU)) != 0;
		break;
	case BCM4334_CHIP_ID:
		st = (R_REG(osh, &cc->res_state) & PMURES_BIT(RES4334_OTP_PU)) != 0;
		break;
	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
		st = (R_REG(osh, &cc->res_state) & PMURES_BIT(RES4324_OTP_PU)) != 0;
		break;

	case BCM4335_CHIP_ID:
		st = (R_REG(osh, &cc->res_state) & PMURES_BIT(RES4335_OTP_PU)) != 0;
		break;
	case BCM4360_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM43526_CHIP_ID:
	case BCM4352_CHIP_ID:
		st = (R_REG(osh, &cc->res_state) & PMURES_BIT(RES4360_OTP_PU)) != 0;
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
		st = TRUE;
		break;
	default:
		st = TRUE;
		break;
	}

	/* Return to original core */
	si_setcoreidx(sih, idx);
	return st;
}

void
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_ERR)
si_pmu_sprom_enable(si_t *sih, osl_t *osh, bool enable)
#else
BCMATTACHFN(si_pmu_sprom_enable)(si_t *sih, osl_t *osh, bool enable)
#endif
{
	chipcregs_t *cc;
	uint origidx;

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	switch (CHIPID(sih->chip)) {
	case BCM4315_CHIP_ID:
		if (CHIPREV(sih->chiprev) < 1)
			break;
		if (sih->chipst & CST4315_SPROM_SEL) {
			uint32 val;
			W_REG(osh, &cc->chipcontrol_addr, 0);
			val = R_REG(osh, &cc->chipcontrol_data);
			if (enable)
				val &= ~0x80000000;
			else
				val |= 0x80000000;
			W_REG(osh, &cc->chipcontrol_data, val);
		}
		break;
	default:
		break;
	}

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

bool
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_ERR)
si_pmu_is_sprom_enabled(si_t *sih, osl_t *osh)
#else
BCMATTACHFN(si_pmu_is_sprom_enabled)(si_t *sih, osl_t *osh)
#endif
{
	chipcregs_t *cc;
	uint origidx;
	bool enable = TRUE;

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	switch (CHIPID(sih->chip)) {
	case BCM4315_CHIP_ID:
		if (CHIPREV(sih->chiprev) < 1)
			break;
		if (!(sih->chipst & CST4315_SPROM_SEL))
			break;
		W_REG(osh, &cc->chipcontrol_addr, 0);
		if (R_REG(osh, &cc->chipcontrol_data) & 0x80000000)
			enable = FALSE;
		break;
	default:
		break;
	}

	/* Return to original core */
	si_setcoreidx(sih, origidx);
	return enable;
}

/* initialize PMU chip controls and other chip level stuff */
void
BCMATTACHFN(si_pmu_chip_init)(si_t *sih, osl_t *osh)
{
	uint origidx;

	ASSERT(sih->cccaps & CC_CAP_PMU);

	si_pmu_otp_chipcontrol(sih, osh);

#ifdef CHIPC_UART_ALWAYS_ON
	si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, clk_ctl_st), CCS_FORCEALP, CCS_FORCEALP);
#endif /* CHIPC_UART_ALWAYS_ON */

#ifndef CONFIG_XIP
	/* Gate off SPROM clock and chip select signals */
	si_pmu_sprom_enable(sih, osh, FALSE);
#endif

	/* Remember original core */
	origidx = si_coreidx(sih);

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

		clkreq_conf = (uint16)getintvar(NULL, "clkreq_conf");

		if (clkreq_conf & CLKREQ_TYPE_CONFIG_PUSHPULL)
			val =  (1 << PMU_CC1_CLKREQ_TYPE_SHIFT);

		si_pmu_chipcontrol(sih, PMU_CHIPCTL0, mask, val);
		break;
	}

#ifdef BCMQT
	case BCM4314_CHIP_ID:
	{
		chipcregs_t *cc;
		uint32 tmp;

		cc = si_setcoreidx(sih, SI_CC_IDX);

		W_REG(osh, &cc->chipcontrol_addr, PMU_CHIPCTL3);
		tmp = R_REG(osh, &cc->chipcontrol_data);
		tmp &= ~(1 << PMU_CC3_ENABLE_RF_SHIFT);
		tmp |= (1 << PMU_CC3_RF_DISABLE_IVALUE_SHIFT);
		W_REG(osh, &cc->chipcontrol_data, tmp);
		break;
	}
#endif /* BCMQT */
	default:
		break;
	}

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

/* initialize PMU registers in case default values proved to be suboptimal */
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
		chipcregs_t *cc;
		uint origidx;

		if (CHIPREV(sih->chiprev) != 2)
			break;

		/* Remember original core before switch to chipc */
		origidx = si_coreidx(sih);
		cc = si_setcoreidx(sih, SI_CC_IDX);
		ASSERT(cc != NULL);

		W_REG(osh, &cc->regcontrol_addr, 4);
		val = R_REG(osh, &cc->regcontrol_data);
		val |= (uint32)(1 << 16);
		W_REG(osh, &cc->regcontrol_data, val);

		/* Return to original core */
		si_setcoreidx(sih, origidx);
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
		cbuck_mv = (uint16)getintvar(NULL, "cbuckout");

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
	case BCM4334_CHIP_ID:
		if (!CST4334_CHIPMODE_HSIC(sih->chipst))
			si_pmu_chipcontrol(sih, 2, CCTRL4334_HSIC_LDO_PU, CCTRL4334_HSIC_LDO_PU);
		break;
	case BCM43143_CHIP_ID:
#ifndef BCM_BOOTLOADER
		si_pmu_chipcontrol(sih, PMU_CHIPCTL0, PMU43143_XTAL_CORE_SIZE_MASK, 0x20);
#endif
	default:
		break;
	}
	si_pmu_otp_regcontrol(sih, osh);
}

void
si_pmu_radio_enable(si_t *sih, bool enable)
{
	ASSERT(sih->cccaps & CC_CAP_PMU);

	switch (CHIPID(sih->chip)) {
	case BCM4325_CHIP_ID:
		if (sih->boardflags & BFL_FASTPWR)
			break;

		if ((sih->boardflags & BFL_BUCKBOOST)) {
			si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, min_res_mask),
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
}

/* Wait for a particular clock level to be on the backplane */
uint32
si_pmu_waitforclk_on_backplane(si_t *sih, osl_t *osh, uint32 clk, uint32 delay_val)
{
	chipcregs_t *cc;
	uint origidx;

	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	if (delay_val)
		SPINWAIT(((R_REG(osh, &cc->pmustatus) & clk) != clk), delay_val);

	/* Return to original core */
	si_setcoreidx(sih, origidx);

	return (R_REG(osh, &cc->pmustatus) & clk);
}

/*
 * Measures the ALP clock frequency in KHz.  Returns 0 if not possible.
 * Possible only if PMU rev >= 10 and there is an external LPO 32768Hz crystal.
 */

#define EXT_ILP_HZ 32768

uint32
BCMATTACHFN(si_pmu_measure_alpclk)(si_t *sih, osl_t *osh)
{
	chipcregs_t *cc;
	uint origidx;
	uint32 alp_khz;
	uint32 pmustat_lpo = 0;

	if (sih->pmurev < 10)
		return 0;

	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	if ((CHIPID(sih->chip) == BCM4335_CHIP_ID) ||
	0)
		pmustat_lpo = !(R_REG(osh, &cc->pmucontrol) & PCTL_LPO_SEL);
	else
		pmustat_lpo = R_REG(osh, &cc->pmustatus) & PST_EXTLPOAVAIL;

	if (pmustat_lpo) {
		uint32 ilp_ctr, alp_hz;

		/* Enable the reg to measure the freq, in case disabled before */
		W_REG(osh, &cc->pmu_xtalfreq, 1U << PMU_XTALFREQ_REG_MEASURE_SHIFT);

		/* Delay for well over 4 ILP clocks */
		OSL_DELAY(1000);

		/* Read the latched number of ALP ticks per 4 ILP ticks */
		ilp_ctr = R_REG(osh, &cc->pmu_xtalfreq) & PMU_XTALFREQ_REG_ILPCTR_MASK;

		/* Turn off the PMU_XTALFREQ_REG_MEASURE_SHIFT bit to save power */
		W_REG(osh, &cc->pmu_xtalfreq, 0);

		/* Calculate ALP frequency */
		alp_hz = (ilp_ctr * EXT_ILP_HZ) / 4;

		/* Round to nearest 100KHz, and at the same time convert to KHz */
		alp_khz = (alp_hz + 50000) / 100000 * 100;
	} else
		alp_khz = 0;

	/* Return to original core */
	si_setcoreidx(sih, origidx);

	return alp_khz;
}

void
si_pmu_set_4330_plldivs(si_t *sih, uint8 dacrate)
{
	uint32 FVCO = si_pmu1_pllfvco0(sih)/1000;
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
}

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
