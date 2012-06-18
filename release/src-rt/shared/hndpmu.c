/*
 * Misc utility routines for accessing PMU corerev specific features
 * of the SiliconBackplane-based Broadcom chips.
 *
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
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
 * $Id: hndpmu.c,v 1.234.2.35 2011-02-11 21:35:40 Exp $
 */

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

/* PLL controls/clocks */
static void si_pmu0_pllinit0(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal);
static uint32 si_pmu0_alpclk0(si_t *sih, osl_t *osh, chipcregs_t *cc);
#if !defined(_CFE_) || defined(CFG_WL)
static uint32 si_pmu0_cpuclk0(si_t *sih, osl_t *osh, chipcregs_t *cc);
static void si_pmu1_pllinit0(si_t *sih, osl_t *osh, chipcregs_t *cc, uint32 xtal);
static uint32 si_pmu1_cpuclk0(si_t *sih, osl_t *osh, chipcregs_t *cc);
static uint32 si_pmu1_alpclk0(si_t *sih, osl_t *osh, chipcregs_t *cc);

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
#endif /* !_CFE_ || CFG_WL */

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
	uint32 res_mask = 0;
	chipcregs_t *cc;
	uint origidx;

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	res_mask = PMURES_BIT(RES4330_BBPLL_PWRSW_PU) | PMURES_BIT(RES4330_MACPHY_CLKAVAIL) |
		PMURES_BIT(RES4330_HT_AVAIL);

	AND_REG(si_osh(sih), &cc->min_res_mask,	~(res_mask));
	AND_REG(si_osh(sih), &cc->max_res_mask,	~(res_mask));
	OR_REG(si_osh(sih), &cc->pmucontrol, PCTL_PLL_PLLCTL_UPD);
	OR_REG(si_osh(sih), &cc->max_res_mask,	res_mask);

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
		default:
			ASSERT(FALSE);
			break;
		}
		break;
	case BCM4331_CHIP_ID:
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

#if !defined(_CFE_) || defined(CFG_WL)
/* d11 slow to fast clock transition time in slow clock cycles */
#define D11SCC_SLOW2FAST_TRANSITION	2

uint16
BCMINITFN(si_pmu_fast_pwrup_delay)(si_t *sih, osl_t *osh)
{
	uint delay_val = PMU_MAX_TRANSITION_DLY;
	chipcregs_t *cc;
	uint origidx;
#ifdef BCMDBG
	char chn[8];

	chn[0] = 0;	/* to suppress compile error */
#endif

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
	case BCM43234_CHIP_ID:	case BCM43237_CHIP_ID:
	case BCM4331_CHIP_ID:
	case BCM43431_CHIP_ID:
	case BCM43131_CHIP_ID:
	case BCM43227_CHIP_ID:
	case BCM43228_CHIP_ID:
	case BCM43428_CHIP_ID:
	case BCM6362_CHIP_ID:
	case BCM4342_CHIP_ID:
	case BCM4313_CHIP_ID:
		delay_val = ISSIM_ENAB(sih) ? 70 : 3700;
		break;
	case BCM4328_CHIP_ID:
		delay_val = 7000;
		break;
	case BCM4325_CHIP_ID:
		if (ISSIM_ENAB(sih))
			delay_val = 70;
		else {
			uint32 ilp = si_ilp_clock(sih);
			delay_val = (si_pmu_res_uptime(sih, osh, cc, RES4325_HT_AVAIL) +
			         D11SCC_SLOW2FAST_TRANSITION) * ((1000000 + ilp - 1) / ilp);
			delay_val = (11 * delay_val) / 10;
		}
		break;
	case BCM4329_CHIP_ID:
		if (ISSIM_ENAB(sih))
			delay_val = 70;
		else {
			uint32 ilp = si_ilp_clock(sih);
			delay_val = (si_pmu_res_uptime(sih, osh, cc, RES4329_HT_AVAIL) +
			         D11SCC_SLOW2FAST_TRANSITION) * ((1000000 + ilp - 1) / ilp);
			delay_val = (11 * delay_val) / 10;
		}
		break;
	case BCM4315_CHIP_ID:
		if (ISSIM_ENAB(sih))
			delay_val = 70;
		else {
			uint32 ilp = si_ilp_clock(sih);
			delay_val = (si_pmu_res_uptime(sih, osh, cc, RES4315_HT_AVAIL) +
			         D11SCC_SLOW2FAST_TRANSITION) * ((1000000 + ilp - 1) / ilp);
			delay_val = (11 * delay_val) / 10;
		}
		break;
	case BCM4319_CHIP_ID:
		if (ISSIM_ENAB(sih))
			delay_val = 70;
		else {
#ifdef BCMUSBDEV
			/* For USB HT is always available, even durring IEEE PS,
			* so need minimal delay
			*/
			delay_val = 100;
#else /* BCMUSBDEV */
			/* For SDIO, total delay in getting HT available */
			/* Adjusted for uptime XTAL=672us, HTAVail=128us */
			uint32 ilp = si_ilp_clock(sih);
			delay_val = si_pmu_res_uptime(sih, osh, cc, RES4319_HT_AVAIL);
			PMU_MSG(("si_ilp_clock (Hz): %u delay (ilp clks): %u\n", ilp, delay_val));
			delay_val = (delay_val + D11SCC_SLOW2FAST_TRANSITION) * (1000000 / ilp);
			PMU_MSG(("delay (us): %u\n", delay_val));
			delay_val = (11 * delay_val) / 10;
			PMU_MSG(("delay (us): %u\n", delay_val));
			/* VDDIO_RF por delay = 3.4ms */
			if (delay_val < 3400) delay_val = 3400;
#endif /* BCMUSBDEV */
		}
		break;
	case BCM4336_CHIP_ID:
		if (ISSIM_ENAB(sih))
			delay_val = 70;
		else {
			uint32 ilp = si_ilp_clock(sih);
			delay_val = (si_pmu_res_uptime(sih, osh, cc, RES4336_HT_AVAIL) +
			         D11SCC_SLOW2FAST_TRANSITION) * ((1000000 + ilp - 1) / ilp);
			delay_val = (11 * delay_val) / 10;
		}
		break;
	case BCM4330_CHIP_ID:
		if (ISSIM_ENAB(sih))
			delay_val = 70;
		else {
			uint32 ilp = si_ilp_clock(sih);
			delay_val = (si_pmu_res_uptime(sih, osh, cc, RES4330_HT_AVAIL) +
			         D11SCC_SLOW2FAST_TRANSITION) * ((1000000 + ilp - 1) / ilp);
			delay_val = (11 * delay_val) / 10;
		}
		break;
	default:
		break;
	}

	/* PMU_MSG(("si_pmu_fast_pwrup_delay: chip %s rev %d delay %d\n",
	 *        bcm_chipname(sih->chip, chn, 8), sih->chiprev, delay));
	 */

	/* Return to original core */
	si_setcoreidx(sih, origidx);

	return (uint16)delay_val;
}
#endif /* !_CFE_ || CFG_WL */

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

/* Setup resource up/down timers */
typedef struct {
	uint8 resnum;
	uint16 updown;
} pmu_res_updown_t;

/* Change resource dependancies masks */
typedef struct {
	uint32 res_mask;		/* resources (chip specific) */
	int8 action;			/* action */
	uint32 depend_mask;		/* changes to the dependancies mask */
	bool (*filter)(si_t *sih);	/* action is taken when filter is NULL or return TRUE */
} pmu_res_depend_t;

/* Resource dependancies mask change action */
#define RES_DEPEND_SET		0	/* Override the dependancies mask */
#define RES_DEPEND_ADD		1	/* Add to the  dependancies mask */
#define RES_DEPEND_REMOVE	-1	/* Remove from the dependancies mask */

#if !defined(_CFE_) || defined(CFG_WL)
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
	/* Adjust ALL resource dependencies - remove CBUCK dependancies if it is not used. */
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
	/* Adjust ALL resource dependencies - remove CBUCK dependancies if it is not used. */
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
	{ RES4329_XTAL_PU, 0x1501 },
	{ RES4329_PALDO_PU, 0x3501 }
};

static const pmu_res_depend_t BCMATTACHDATA(bcm4329_res_depend)[] = {
	/* Make lnldo1 independant of CBUCK_PWM and CBUCK_BURST */
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
	char *val;

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
	case BCM43234_CHIP_ID:	case BCM43237_CHIP_ID:
	case BCM4331_CHIP_ID:   case BCM43431_CHIP_ID:
	case BCM6362_CHIP_ID:
		/* use chip default */
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
#ifdef  BCM_BOOTLOADER
		/* Initialize to ResInitMode3 for bootloader */
		min_mask = PMURES_BIT(RES4319_CBUCK_LPOM) |
			PMURES_BIT(RES4319_CBUCK_BURST) |
			PMURES_BIT(RES4319_CBUCK_PWM) |
			PMURES_BIT(RES4319_CLDO_PU) |
			PMURES_BIT(RES4319_PALDO_PU) |
			PMURES_BIT(RES4319_LNLDO1_PU) |
			PMURES_BIT(RES4319_XTAL_PU) |
			PMURES_BIT(RES4319_ALP_AVAIL) |
			PMURES_BIT(RES4319_RFPLL_PWRSW_PU) |
			PMURES_BIT(RES4319_BBPLL_PWRSW_PU) |
			PMURES_BIT(RES4319_HT_AVAIL);
#else
		/* We only need a few resources to be kept on all the time */
#ifdef BCMUSBDEV
		/* For USB HT is always available, even durring IEEE PS, so RF switches are
		* made independent of HT Avail and are by default on, but can be made off
		* during IEEE PS by ucode (and then on)
		*/
		min_mask = PMURES_BIT(RES4319_CBUCK_LPOM) |
			PMURES_BIT(RES4319_CLDO_PU) |
			PMURES_BIT(RES4319_RX_PWRSW_PU) | PMURES_BIT(RES4319_TX_PWRSW_PU) |
			PMURES_BIT(RES4319_LOGEN_PWRSW_PU) | PMURES_BIT(RES4319_AFE_PWRSW_PU);
#else
		/* For SDIO RF switches are automatically made on off along with HT */
		min_mask = PMURES_BIT(RES4319_CBUCK_LPOM) |
			PMURES_BIT(RES4319_CLDO_PU);
#endif
#endif  /* BCM_BOOTLOADER */

		/* Allow everything else to be turned on upon requests */
		max_mask = ~(~0 << rsrcs);
		break;
	case BCM4336_CHIP_ID:
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
			PMURES_BIT(RES4313_BB_PLL_PWRSW_RSRC);
		max_mask = 0xffff;
		break;
	default:
		break;
	}

	/* Apply nvram override to min mask */
	if ((val = getvar(NULL, "rmin")) != NULL) {
		PMU_MSG(("Applying rmin=%s to min_mask\n", val));
		min_mask = (uint32)bcm_strtoul(val, NULL, 0);
	}
	/* Apply nvram override to max mask */
	if ((val = getvar(NULL, "rmax")) != NULL) {
		PMU_MSG(("Applying rmax=%s to max_mask\n", val));
		max_mask = (uint32)bcm_strtoul(val, NULL, 0);
	}

	*pmin = min_mask;
	*pmax = max_mask;
}
#endif /* !_CFE_ || CFG_WL */

/* initialize PMU resources */
void
BCMATTACHFN(si_pmu_res_init)(si_t *sih, osl_t *osh)
{
#if !defined(_CFE_) || defined(CFG_WL)
	chipcregs_t *cc;
	uint origidx;
	const pmu_res_updown_t *pmu_res_updown_table = NULL;
	uint pmu_res_updown_table_sz = 0;
	const pmu_res_depend_t *pmu_res_depend_table = NULL;
	uint pmu_res_depend_table_sz = 0;
	uint32 min_mask = 0, max_mask = 0;
	char name[8], *val;
	uint i, rsrcs;

	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

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
		/* Optimize resources dependancies */
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
		/* Optimize resources dependancies masks */
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
		/* Optimize resources dependancies masks */
		pmu_res_depend_table = bcm4319a0_res_depend;
		pmu_res_depend_table_sz = ARRAYSIZE(bcm4319a0_res_depend);
		break;

	case BCM4336_CHIP_ID:
		/* Optimize resources up/down timers */
		if (ISSIM_ENAB(sih)) {
			pmu_res_updown_table = bcm4336a0_res_updown_qt;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4336a0_res_updown_qt);
		}
		else {
			pmu_res_updown_table = bcm4336a0_res_updown;
			pmu_res_updown_table_sz = ARRAYSIZE(bcm4336a0_res_updown);
		}
		/* Optimize resources dependancies masks */
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
		/* Optimize resources dependancies masks */
		pmu_res_depend_table = bcm4330a0_res_depend;
		pmu_res_depend_table_sz = ARRAYSIZE(bcm4330a0_res_depend);
		break;
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
		snprintf(name, sizeof(name), "r%dt", i);
		if ((val = getvar(NULL, name)) == NULL)
			continue;
		PMU_MSG(("Applying %s=%s to rsrc %d res_updn_timer\n", name, val, i));
		W_REG(osh, &cc->res_table_sel, (uint32)i);
		W_REG(osh, &cc->res_updn_timer, (uint32)bcm_strtoul(val, NULL, 0));
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
	/* Apply nvram overrides to dependancies masks */
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

	/* It is required to program max_mask first and then min_mask */
#ifdef BCM_BOOTLOADER
	if (CHIPID(sih->chip) == BCM4319_CHIP_ID) {
		min_mask |= R_REG(osh, &cc->min_res_mask);
		max_mask |= R_REG(osh, &cc->max_res_mask);
	}
#endif /* BCM_BOOTLOADER */

	/* Program max resource mask */
#ifdef BCM_BOOTLOADER
	/* Apply nvram override to max mask */
	if ((val = getvar(NULL, "brmax")) != NULL) {
		PMU_MSG(("Applying brmax=%s to max_res_mask\n", val));
		max_mask = (uint32)bcm_strtoul(val, NULL, 0);
	}
#endif /* BCM_BOOTLOADER */

	if (max_mask) {
		PMU_MSG(("Changing max_res_mask to 0x%x\n", max_mask));
		W_REG(osh, &cc->max_res_mask, max_mask);
	}

	/* Program min resource mask */
#ifdef BCM_BOOTLOADER
	/* Apply nvram override to min mask */
	if ((val = getvar(NULL, "brmin")) != NULL) {
		PMU_MSG(("Applying brmin=%s to min_res_mask\n", val));
		min_mask = (uint32)bcm_strtoul(val, NULL, 0);
	}
#endif /* BCM_BOOTLOADER */

	if (min_mask) {
		PMU_MSG(("Changing min_res_mask to 0x%x\n", min_mask));
		W_REG(osh, &cc->min_res_mask, min_mask);
	}

	/* Add some delay; allow resources to come up and settle. */
	OSL_DELAY(2000);

	/* Return to original core */
	si_setcoreidx(sih, origidx);

#endif /* !_CFE_ || CFG_WL */
}
/* WAR for 4319 swctrl tri-state issue */
void
si_pmu_res_4319_swctrl_war(si_t *sih, osl_t *osh, bool enable)
{
	uint32 min_mask;
	chipcregs_t *cc;
	uint origidx;
	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);
	min_mask = R_REG(osh, &cc->min_res_mask);
	if (enable)
	        W_REG(osh, &cc->min_res_mask,
	                min_mask | PMURES_BIT(RES4319_PALDO_PU));
	else
	        W_REG(osh, &cc->min_res_mask,
	                min_mask & ~PMURES_BIT(RES4319_PALDO_PU));

	/* Return to original core */
	si_setcoreidx(sih, origidx);
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

#if !defined(_CFE_) || defined(CFG_WL)
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

static const pmu1_xtaltab0_t BCMINITDATA(pmu1_xtaltab0_960)[] = {
	{12000,   1,       1,      1,     0x50,   0x0     },
	{13000,   2,       1,      1,     0x49,   0xD89D89},
	{14400,   3,       1,      1,     0x42,   0xAAAAAA},
	{15360,   4,       1,      1,     0x3E,   0x800000},
	{16200,   5,       1,      1,     0x39,   0x425ED0},
	{16800,   6,       1,      1,     0x39,   0x249249},
	{19200,   7,       1,      1,     0x32,   0x0     },
	{19800,   8,       1,      1,     0x30,   0x7C1F07},
	{20000,   9,       1,      1,     0x30,   0x0     },
	{25000,   10,      1,      1,     0x26,   0x666666},
	{26000,   11,      1,      1,     0x24,   0xEC4EC4},
	{30000,   12,      1,      1,     0x20,   0x0     },
	{37400,   13,      2,      1,     0x33,   0x563EF9},
	{38400,   14,      2,      1,     0x32,   0x0	  },
	{40000,   15,      2,      1,     0x30,   0x0     },
	{48000,   16,      2,      1,     0x28,   0x0     },
	{0,	  0,	   0,	   0,	  0,	    0	  }
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
#define PMU1_XTALTAB0_960_25000K	9
#define PMU1_XTALTAB0_960_26000K	10
#define PMU1_XTALTAB0_960_30000K	11
#define PMU1_XTALTAB0_960_37400K	12
#define PMU1_XTALTAB0_960_38400K	13
#define PMU1_XTALTAB0_960_40000K	14
#define PMU1_XTALTAB0_960_48000K	15

/* select xtal table for each chip */
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

/* select default xtal frequency for each chip */
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
		/* Default to 26000Khz */
		return &pmu1_xtaltab0_960[PMU1_XTALTAB0_960_26000K];
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
#endif /* !_CFE_ || CFG_WL */

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

#if !defined(_CFE_) || defined(CFG_WL)
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
	uint32 FVCO = si_pmu1_pllfvco0(sih);
	uint8 dacrate;

	FVCO = FVCO/1000;

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
		break;

	case BCM4336_CHIP_ID:
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

	if (CHIPID(sih->chip) == BCM4319_CHIP_ID) {
		tmp &=  ~(PMU1_PLL0_PC0_BYPASS_SDMOD_MASK);
		if (!(xt->ndiv_frac))
			tmp |= (1<<(PMU1_PLL0_PC0_BYPASS_SDMOD_SHIFT));
		else
			tmp |= (0<<(PMU1_PLL0_PC0_BYPASS_SDMOD_SHIFT));
	}

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
	if ((CHIPID(sih->chip) == BCM4336_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4330_CHIP_ID))
		ndiv_mode = PMU1_PLL0_PC2_NDIV_MODE_MFB;
	else
		ndiv_mode = PMU1_PLL0_PC2_NDIV_MODE_MASH;

	if ((CHIPID(sih->chip) == BCM4319_CHIP_ID)) {
		if (!(xt->ndiv_frac))
			ndiv_mode = PMU1_PLL0_PC2_NDIV_MODE_INT;
		else
			ndiv_mode = PMU1_PLL0_PC2_NDIV_MODE_MFB;
#ifdef BCMQT
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
		tmp = 0x120F1010;
		W_REG(osh, &cc->pllcontrol_data, tmp);
#endif
	}
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

	/* Writing to pllcontrol[4]  */
	if ((CHIPID(sih->chip) == BCM4319_CHIP_ID)) {
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL4);
		if (!(xt->ndiv_frac))
		        tmp = 0x200005c0;
		else
		        tmp = 0x202C2820;

		tmp &= ~(PMU1_PLL0_PC4_KVCO_XS_MASK);

		if (FVCO < 1600)
		        tmp |= (4<<PMU1_PLL0_PC4_KVCO_XS_SHIFT);
		else
		        tmp |= (7<<PMU1_PLL0_PC4_KVCO_XS_SHIFT);

		W_REG(osh, &cc->pllcontrol_data, tmp);
	}

	/* Write clock driving strength to pllcontrol[5] */
	if (buf_strength) {
		PMU_MSG(("Adjusting PLL buffer drive strength: %x\n", buf_strength));

		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL5);
		tmp = R_REG(osh, &cc->pllcontrol_data) & ~PMU1_PLL0_PC5_CLK_DRV_MASK;
		tmp |= (buf_strength << PMU1_PLL0_PC5_CLK_DRV_SHIFT);

		if (CHIPID(sih->chip) == BCM4319_CHIP_ID) {
			tmp &= ~(PMU1_PLL0_PC5_VCO_RNG_MASK | PMU1_PLL0_PC5_PLL_CTRL_37_32_MASK);
			if (!(xt->ndiv_frac))
				tmp |= (0x25<<(PMU1_PLL0_PC5_PLL_CTRL_37_32_SHIFT));
			else
				tmp |= (0x15<<(PMU1_PLL0_PC5_PLL_CTRL_37_32_SHIFT));

			if (FVCO < 1600)
				tmp |= (0x0<<(PMU1_PLL0_PC5_VCO_RNG_SHIFT));
			else
				tmp |= (0x1<<(PMU1_PLL0_PC5_VCO_RNG_SHIFT));
		}
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
	uint32 tmp, m1div;
#ifdef BCMDBG
	uint32 ndiv_int, ndiv_frac, p2div, p1div, fvco;
	uint32 fref;
#endif
	uint32 FVCO = si_pmu1_pllfvco0(sih);

	/* Read m1div from pllcontrol[1] */
	W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
	tmp = R_REG(osh, &cc->pllcontrol_data);
	m1div = (tmp & PMU1_PLL0_PC1_M1DIV_MASK) >> PMU1_PLL0_PC1_M1DIV_SHIFT;

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

	/* Return ARM/SB clock */
	return FVCO / m1div * 1000;
}
#endif /* !_CFE_ || CFG_WL */

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
	case BCM5354_CHIP_ID:
		if (xtalfreq == 0)
			xtalfreq = 25000;
		si_pmu0_pllinit0(sih, osh, cc, xtalfreq);
		break;
#if !defined(_CFE_) || defined(CFG_WL)
	case BCM4328_CHIP_ID:
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
	case BCM4313_CHIP_ID:
	case BCM43222_CHIP_ID:	case BCM43111_CHIP_ID:	case BCM43112_CHIP_ID:
	case BCM43224_CHIP_ID:	case BCM43225_CHIP_ID:  case BCM43420_CHIP_ID:
	case BCM43421_CHIP_ID:
	case BCM43226_CHIP_ID:
	case BCM43235_CHIP_ID:	case BCM43236_CHIP_ID:	case BCM43238_CHIP_ID:
	case BCM43234_CHIP_ID:	case BCM43237_CHIP_ID:
	case BCM4331_CHIP_ID:   case BCM43431_CHIP_ID:
	case BCM43131_CHIP_ID:
	case BCM43227_CHIP_ID:
	case BCM43228_CHIP_ID:
	case BCM43428_CHIP_ID:
	case BCM6362_CHIP_ID:
		break;
	case BCM4315_CHIP_ID:
	case BCM4319_CHIP_ID:
	case BCM4336_CHIP_ID:
	case BCM4330_CHIP_ID:
		si_pmu1_pllinit0(sih, osh, cc, xtalfreq);
		break;
#endif /* !_CFE_ || CFG_WL */
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
#if !defined(_CFE_) || defined(CFG_WL)
	case BCM4325_CHIP_ID:
	case BCM4329_CHIP_ID:
	case BCM4315_CHIP_ID:
	case BCM4319_CHIP_ID:
	case BCM4336_CHIP_ID:
	case BCM4330_CHIP_ID:
		clock = si_pmu1_alpclk0(sih, osh, cc);
		break;
#endif /* !_CFE_ || CFG_WL */
	case BCM4312_CHIP_ID:
	case BCM4322_CHIP_ID:	case BCM43221_CHIP_ID:	case BCM43231_CHIP_ID:
	case BCM43222_CHIP_ID:	case BCM43111_CHIP_ID:	case BCM43112_CHIP_ID:
	case BCM43224_CHIP_ID:	case BCM43225_CHIP_ID:  case BCM43420_CHIP_ID:
	case BCM43421_CHIP_ID:
	case BCM43226_CHIP_ID:
	case BCM43235_CHIP_ID:	case BCM43236_CHIP_ID:	case BCM43238_CHIP_ID:
	case BCM43234_CHIP_ID:	case BCM43237_CHIP_ID:
	case BCM4331_CHIP_ID:   case BCM43431_CHIP_ID:
	case BCM43131_CHIP_ID:
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
#if !defined(_CFE_) || defined(CFG_WL)
	case BCM4328_CHIP_ID:
		clock = si_pmu0_cpuclk0(sih, osh, cc);
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
	case BCM4331_CHIP_ID:   case BCM43431_CHIP_ID:
	case BCM6362_CHIP_ID:
	case BCM4342_CHIP_ID:
		/* 96MHz backplane clock */
		clock = 96000 * 1000;
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
	case BCM4330_CHIP_ID:
		clock = si_pmu1_cpuclk0(sih, osh, cc);
		break;
	case BCM4313_CHIP_ID:
		/* 80MHz backplane clock */
		clock = 80000 * 1000;
		break;
	case BCM43235_CHIP_ID:	case BCM43236_CHIP_ID:	case BCM43238_CHIP_ID:
	case BCM43234_CHIP_ID:
		clock = (cc->chipstatus & CST43236_BP_CLK) ? (120000 * 1000) : (96000 * 1000);
		break;
	case BCM43237_CHIP_ID:
		clock = (cc->chipstatus & CST43237_BP_CLK) ? (96000 * 1000) : (80000 * 1000);
		break;
#endif /* !_CFE_ || CFG_WL */
	case BCM5354_CHIP_ID:
		clock = 120000000;
		break;
	case BCM4716_CHIP_ID:
	case BCM4748_CHIP_ID:
	case BCM47162_CHIP_ID:
		clock = si_pmu5_clock(sih, osh, cc, PMU4716_MAINPLL_PLL0, PMU5_MAINPLL_SI);
		break;
	case BCM5356_CHIP_ID:
		clock = si_pmu5_clock(sih, osh, cc, PMU5356_MAINPLL_PLL0, PMU5_MAINPLL_SI);
		break;
	case BCM5357_CHIP_ID:
	case BCM4749_CHIP_ID:
		clock = si_pmu5_clock(sih, osh, cc, PMU5357_MAINPLL_PLL0, PMU5_MAINPLL_SI);
		break;
	case BCM4706_CHIP_ID:
		clock = si_4706_pmu_clock(sih, osh, cc, PMU4706_MAINPLL_PLL0, PMU5_MAINPLL_SI);
		break;
	case BCM53572_CHIP_ID:
		clock = 75000000;
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
		(CHIPID(sih->chip) == BCM43237_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43238_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4336_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM4330_CHIP_ID))) {
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
		(CHIPID(sih->chip) == BCM4336_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43234_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43235_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43236_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43237_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43238_CHIP_ID) ||
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
		OSL_DELAY(ILP_CALC_DUR * 1000);
		end = R_REG(osh, &cc->pmutimer);
		delta = end - start;
		ilpcycles_per_sec = delta * (1000 / ILP_CALC_DUR);
		si_setcoreidx(sih, origidx);
	}

	return ilpcycles_per_sec;
}

/* SDIO Pad drive strength to select value mappings */
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
	case SDIOD_DRVSTR_KEY(BCM4319_CHIP_ID, 7):
		str_tab = (sdiod_drive_str_t *)&sdiod_drive_strength_tab2;
		str_mask = 0x00003800;
		str_shift = 11;
		break;
	case SDIOD_DRVSTR_KEY(BCM4336_CHIP_ID, 8):
		str_tab = (sdiod_drive_str_t *)&sdiod_drive_strength_tab3;
		str_mask = 0x00003800;
		str_shift = 11;
		break;

	default:
		PMU_MSG(("No SDIO Drive strength init done for chip %s rev %d pmurev %d\n",
		         bcm_chipname(sih->chip, chn, 8), sih->chiprev, sih->pmurev));

		break;
	}

	if (str_tab != NULL) {
		uint32 drivestrength_sel = 0;
		uint32 cc_data_temp;
		int i;

		for (i = 0; str_tab[i].strength != 0; i ++) {
			if (drivestrength >= str_tab[i].strength) {
				drivestrength_sel = str_tab[i].sel;
				break;
			}
		}

		W_REG(osh, &cc->chipcontrol_addr, 1);
		cc_data_temp = R_REG(osh, &cc->chipcontrol_data);
		cc_data_temp &= ~str_mask;
		drivestrength_sel <<= str_shift;
		cc_data_temp |= drivestrength_sel;
		W_REG(osh, &cc->chipcontrol_data, cc_data_temp);

		PMU_MSG(("SDIO: %dmA drive strength selected, set to 0x%08x\n",
		         drivestrength, cc_data_temp));
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

#if !defined(_CFE_) || defined(CFG_WL)
	if ((CHIPID(sih->chip) == BCM4329_CHIP_ID) && (sih->chiprev == 2)) {
		/* Fix for 4329b0 bad LPOM state. */
		W_REG(osh, &cc->regcontrol_addr, 2);
		OR_REG(osh, &cc->regcontrol_data, 0x100);

		W_REG(osh, &cc->regcontrol_addr, 3);
		OR_REG(osh, &cc->regcontrol_data, 0x4);
	}

	if (CHIPID(sih->chip) == BCM4319_CHIP_ID) {
		/* Limiting the PALDO spike during init time */
		si_pmu_regcontrol(sih, 2, 0x00000007, 0x00000005);
	}
#endif /* !_CFE_ || CFG_WL */

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

#if !defined(_CFE_) || defined(CFG_WL)
/* Return up time in ILP cycles for the given resource. */
static uint
BCMINITFN(si_pmu_res_uptime)(si_t *sih, osl_t *osh, chipcregs_t *cc, uint8 rsrc)
{
	uint32 deps;
	uint up, i, dup, dmax;
	uint32 min_mask = 0, max_mask = 0;

	/* uptime of resource 'rsrc' */
	W_REG(osh, &cc->res_table_sel, rsrc);
	up = (R_REG(osh, &cc->res_updn_timer) >> 8) & 0xff;

	/* direct dependancies of resource 'rsrc' */
	deps = si_pmu_res_deps(sih, osh, cc, PMURES_BIT(rsrc), FALSE);
	for (i = 0; i <= PMURES_MAX_RESNUM; i ++) {
		if (!(deps & PMURES_BIT(i)))
			continue;
		deps &= ~si_pmu_res_deps(sih, osh, cc, PMURES_BIT(i), TRUE);
	}
	si_pmu_res_masks(sih, &min_mask, &max_mask);
	deps &= ~min_mask;

	/* max uptime of direct dependancies */
	dmax = 0;
	for (i = 0; i <= PMURES_MAX_RESNUM; i ++) {
		if (!(deps & PMURES_BIT(i)))
			continue;
		dup = si_pmu_res_uptime(sih, osh, cc, (uint8)i);
		if (dmax < dup)
			dmax = dup;
	}

	PMU_MSG(("si_pmu_res_uptime: rsrc %u uptime %u(deps 0x%08x uptime %u)\n",
	         rsrc, up, deps, dmax));

	return up + dmax + PMURES_UP_TRANSITION;
}

/* Return dependancies (direct or all/indirect) for the given resources */
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
		rsrcs = PMURES_BIT(RES4336_OTP_PU);
		break;
	case BCM4330_CHIP_ID:
		rsrcs = PMURES_BIT(RES4330_OTP_PU);
		break;
	default:
		break;
	}

	if (rsrcs != 0) {
		uint32 otps;

		/* Figure out the dependancies (exclude min_res_mask) */
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
			(on ? OTPS_READY : 0)), 100);
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
		SPINWAIT(0 == (R_REG(osh, &cc->chipstatus) & 0x08), 10 * 1000 * 1000);
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
		SPINWAIT(0 == (R_REG(osh, &cc->chipstatus) & 0x08), 10 * 1000 * 1000);
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

void
si_pmu_spuravoid(si_t *sih, osl_t *osh, uint8 spuravoid)
{
	chipcregs_t *cc;
	uint origidx, intr_val;
	uint32 tmp = 0;

	/* Remember original core before switch to chipc */
	cc = (chipcregs_t *)si_switch_core(sih, CC_CORE_ID, &origidx, &intr_val);
	ASSERT(cc != NULL);

	/* force the HT off  */
	if (CHIPID(sih->chip) == BCM4336_CHIP_ID) {
		tmp = R_REG(osh, &cc->max_res_mask);
		tmp &= ~RES4336_HT_AVAIL;
		W_REG(osh, &cc->max_res_mask, tmp);
		/* wait for the ht to really go away */
		SPINWAIT(((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL) == 0), 10000);
		ASSERT((R_REG(osh, &cc->clk_ctl_st) & CCS_HTAVAIL) == 0);
	}

	/* update the pll changes */
	si_pmu_spuravoid_pllupdate(sih, cc, osh, spuravoid);

	/* enable HT back on  */
	if (CHIPID(sih->chip) == BCM4336_CHIP_ID) {
		tmp = R_REG(osh, &cc->max_res_mask);
		tmp |= RES4336_HT_AVAIL;
		W_REG(osh, &cc->max_res_mask, tmp);
	}

	/* Return to original core */
	si_restore_core(sih, origidx, intr_val);
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

	switch (CHIPID(sih->chip)) {
	case BCM5357_CHIP_ID:   case BCM4749_CHIP_ID:
	case BCM43235_CHIP_ID:	case BCM43236_CHIP_ID:	case BCM43238_CHIP_ID:
	case BCM43234_CHIP_ID:	case BCM43237_CHIP_ID:
	case BCM6362_CHIP_ID:	case BCM53572_CHIP_ID:

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

			/* BCM5357 needs to touch PLL1_PLLCTL[02],so offset PLL0_PLLCTL[02] by 6 */
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
		break;
	case BCM4336_CHIP_ID:
		/* Looks like these are only for default xtal freq 26MHz */
		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL0);
		W_REG(osh, &cc->pllcontrol_data, 0x02100020);

		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL1);
		W_REG(osh, &cc->pllcontrol_data, 0x0C0C0C0C);

		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL2);
		W_REG(osh, &cc->pllcontrol_data, 0x01240C0C);

		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL4);
		W_REG(osh, &cc->pllcontrol_data, 0x202C2820);

		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL5);
		W_REG(osh, &cc->pllcontrol_data, 0x88888825);

		W_REG(osh, &cc->pllcontrol_addr, PMU1_PLL0_PLLCTL3);
		if (spuravoid == 1) {
			W_REG(osh, &cc->pllcontrol_data, 0x00EC4EC4);
		} else {
			W_REG(osh, &cc->pllcontrol_data, 0x00762762);
		}

		tmp = PCTL_PLL_PLLCTL_UPD;
		break;
	case BCM43131_CHIP_ID:
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
		st = (R_REG(osh, &cc->res_state) & PMURES_BIT(RES4336_OTP_PU)) != 0;
		break;
	case BCM4330_CHIP_ID:
		st = (R_REG(osh, &cc->res_state) & PMURES_BIT(RES4330_OTP_PU)) != 0;
		break;

	/* These chip doesn't use PMU bit to power up/down OTP. OTP always on.
	 * Use OTP_INIT command to reset/refresh state.
	 */
	case BCM43222_CHIP_ID:	case BCM43111_CHIP_ID:	case BCM43112_CHIP_ID:
	case BCM43224_CHIP_ID:	case BCM43225_CHIP_ID:	case BCM43421_CHIP_ID:
	case BCM43236_CHIP_ID:	case BCM43235_CHIP_ID:	case BCM43238_CHIP_ID:
	case BCM43234_CHIP_ID:	case BCM43237_CHIP_ID:  case BCM43420_CHIP_ID:
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
#endif /* !_CFE_ || CFG_WL */

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

#ifdef CHIPC_UART_ALWAYS_ON
	si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, clk_ctl_st), CCS_FORCEALP, CCS_FORCEALP);
#endif /* CHIPC_UART_ALWAYS_ON */

#ifndef CONFIG_XIP
	/* Gate off SPROM clock and chip select signals */
	si_pmu_sprom_enable(sih, osh, FALSE);
#endif

	/* Remember original core */
	origidx = si_coreidx(sih);

#if !defined(_CFE_) || defined(CFG_WL)
	/* Misc. chip control, has nothing to do with PMU */
	switch (CHIPID(sih->chip)) {
	case BCM4315_CHIP_ID:
#ifdef BCMUSBDEV
		si_setcore(sih, PCMCIA_CORE_ID, 0);
		si_core_disable(sih, 0);
#endif
		break;
	case BCM4319_CHIP_ID:
		/* No support for external LPO, so power it down */
		si_pmu_chipcontrol(sih, 0, (1<<28), (0<<28));
		break;
	default:
		break;
	}
#endif /* !_CFE_ || CFG_WL */

	/* Return to original core */
	si_setcoreidx(sih, origidx);
}

/* initialize PMU switch/regulators */
void
BCMATTACHFN(si_pmu_swreg_init)(si_t *sih, osl_t *osh)
{
#if !defined(_CFE_) || defined(CFG_WL)
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
		/* Reduce CLDO PWM output voltage to 1.2V */
		si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_CLDO_PWM, 0xe);
		/* Reduce CLDO BURST output voltage to 1.2V */
		si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_CLDO_BURST, 0xe);
		/* Reduce LNLDO1 output voltage to 1.2V */
		si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_LNLDO1, 0xe);
		if (CHIPREV(sih->chiprev) == 0)
			si_pmu_regcontrol(sih, 2, 0x400000, 0x400000);
		break;

	case BCM4330_CHIP_ID:
		/* CBUCK Voltage is 1.8 by default and set that to 1.5 */
		si_pmu_set_ldo_voltage(sih, osh, SET_LDO_VOLTAGE_CBUCK_PWM, 0);
		break;
	default:
		break;
	}
#endif /* !_CFE_ || CFG_WL */
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
		if (enable)
			si_write_wrapperreg(sih, AI_OOBSELOUTB74, (uint32)0x868584);
		else
			si_write_wrapperreg(sih, AI_OOBSELOUTB74, (uint32)0x060584);
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

	if (sih->pmurev < 10)
		return 0;

	ASSERT(sih->cccaps & CC_CAP_PMU);

	/* Remember original core before switch to chipc */
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc != NULL);

	if (R_REG(osh, &cc->pmustatus) & PST_EXTLPOAVAIL) {
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

#if !defined(_CFE_) || defined(CFG_WL)
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
#endif /* !_CFE_ || CFG_WL */
