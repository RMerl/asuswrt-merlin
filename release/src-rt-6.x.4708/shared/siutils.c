/*
 * Misc utility routines for accessing chip-specific features
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
 * $Id: siutils.c 398971 2013-04-26 22:39:49Z $
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
#include <pci_core.h>
#include <pcie_core.h>
#include <nicpci.h>
#include <bcmnvram.h>
#include <bcmsrom.h>
#include <hndtcam.h>
#include <pcicfg.h>
#include <sbpcmcia.h>
#include <sbsocram.h>
#include <bcmotp.h>
#include <hndpmu.h>
#ifdef BCMSPI
#include <spid.h>
#endif /* BCMSPI */
#if !defined(BCM_BOOTLOADER) && defined(SAVERESTORE)
#include <saverestore.h>
#endif

#ifdef BCM_SDRBL
#include <hndcpu.h>
#endif /* BCM_SDRBL */

#include "siutils_priv.h"

/* local prototypes */
static si_info_t *si_doattach(si_info_t *sii, uint devid, osl_t *osh, void *regs,
                              uint bustype, void *sdh, char **vars, uint *varsz);
static bool si_buscore_prep(si_info_t *sii, uint bustype, uint devid, void *sdh);
static bool si_buscore_setup(si_info_t *sii, chipcregs_t *cc, uint bustype, uint32 savewin,
	uint *origidx, void *regs);

static void si_nvram_process(si_info_t *sii, char *pvars);
#if (!defined(_CFE_) && !defined(_CFEZ_)) || defined(CFG_WL)
static void si_sromvars_fixup_4331(si_t *sih, char *pvars);
#endif /* (!_CFE_ && !_CFEZ_) || CFG_WL */

/* dev path concatenation util */
static char *si_devpathvar(si_t *sih, char *var, int len, const char *name);
static bool _si_clkctl_cc(si_info_t *sii, uint mode);
static bool si_ispcie(si_info_t *sii);
static uint BCMINITFN(socram_banksize)(si_info_t *sii, sbsocramregs_t *r, uint8 idx, uint8 mtype);

void si_gci_chipctrl_overrides(osl_t *osh, si_t *sih, char *pvars);


/* global variable to indicate reservation/release of gpio's */
static uint32 si_gpioreservation = 0;

/* global flag to prevent shared resources from being initialized multiple times in si_attach() */

int do_4360_pcie2_war = 0;

/*
 * Allocate a si handle.
 * devid - pci device id (used to determine chip#)
 * osh - opaque OS handle
 * regs - virtual address of initial core registers
 * bustype - pci/pcmcia/sb/sdio/etc
 * vars - pointer to a pointer area for "environment" variables
 * varsz - pointer to int to return the size of the vars
 */
si_t *
BCMATTACHFN(si_attach)(uint devid, osl_t *osh, void *regs,
                       uint bustype, void *sdh, char **vars, uint *varsz)
{
	si_info_t *sii;

	/* alloc si_info_t */
	if ((sii = MALLOC(osh, sizeof (si_info_t))) == NULL) {
		SI_ERROR(("si_attach: malloc failed! malloced %d bytes\n", MALLOCED(osh)));
		return (NULL);
	}

	if (si_doattach(sii, devid, osh, regs, bustype, sdh, vars, varsz) == NULL) {
		MFREE(osh, sii, sizeof(si_info_t));
		return (NULL);
	}
	sii->vars = vars ? *vars : NULL;
	sii->varsz = varsz ? *varsz : 0;

	return (si_t *)sii;
}

/* global kernel resource */
static si_info_t ksii;

static uint32	wd_msticks;		/* watchdog timer ticks normalized to ms */

/* generic kernel variant of si_attach() */
si_t *
BCMATTACHFN(si_kattach)(osl_t *osh)
{
	static bool ksii_attached = FALSE;

	if (!ksii_attached) {
		void *regs;
#ifndef SI_ENUM_BASE_VARIABLE
		regs = REG_MAP(SI_ENUM_BASE, SI_CORE_SIZE);
#endif

		if (si_doattach(&ksii, BCM4710_DEVICE_ID, osh, regs,
		                SI_BUS, NULL,
		                osh != SI_OSH ? &ksii.vars : NULL,
		                osh != SI_OSH ? &ksii.varsz : NULL) == NULL) {
			SI_ERROR(("si_kattach: si_doattach failed\n"));
			REG_UNMAP(regs);
			return NULL;
		}
		REG_UNMAP(regs);

		/* save ticks normalized to ms for si_watchdog_ms() */
		if (PMUCTL_ENAB(&ksii.pub)) {
			if (CHIPID(ksii.pub.chip) == BCM4706_CHIP_ID) {
				/* 4706 CC and PMU watchdogs are clocked at 1/4 of ALP clock */
				wd_msticks = (si_alp_clock(&ksii.pub) / 4) / 1000;
			}
			else
				/* based on 32KHz ILP clock */
				wd_msticks = 32;
		} else {
			if (ksii.pub.ccrev < 18)
				wd_msticks = si_clock(&ksii.pub) / 1000;
			else
				wd_msticks = si_alp_clock(&ksii.pub) / 1000;
		}

		ksii_attached = TRUE;
		SI_MSG(("si_kattach done. ccrev = %d, wd_msticks = %d\n",
		        ksii.pub.ccrev, wd_msticks));
	}

	return &ksii.pub;
}

bool
si_ldo_war(si_t *sih, uint devid)
{
	si_info_t *sii = SI_INFO(sih);
	uint32 w;
	chipcregs_t *cc;
	void *regs = sii->curmap;
	uint32 rev_id, ccst;

	rev_id = OSL_PCI_READ_CONFIG(sii->osh, PCI_CFG_REV, sizeof(uint32));
	rev_id &= 0xff;
	if (!(((CHIPID(devid) == BCM4322_CHIP_ID) ||
	       (CHIPID(devid) == BCM4342_CHIP_ID) ||
	       (CHIPID(devid) == BCM4322_D11N_ID) ||
	       (CHIPID(devid) == BCM4322_D11N2G_ID) ||
	       (CHIPID(devid) == BCM4322_D11N5G_ID)) &&
	      (rev_id == 0)))
		return TRUE;

	SI_MSG(("si_ldo_war: PCI devid 0x%x rev %d, HACK to fix 4322a0 LDO/PMU\n", devid, rev_id));

	/* switch to chipcommon */
	w = OSL_PCI_READ_CONFIG(sii->osh, PCI_BAR0_WIN, sizeof(uint32));
	OSL_PCI_WRITE_CONFIG(sii->osh, PCI_BAR0_WIN, sizeof(uint32), SI_ENUM_BASE);
	cc = (chipcregs_t *)regs;

	/* clear bit 7 to fix LDO
	 * write to register *blindly* WITHOUT read since read may timeout
	 *  because the default clock is 32k ILP
	 */
	W_REG(sii->osh, &cc->regcontrol_addr, 0);
	/* AND_REG(sii->osh, &cc->regcontrol_data, ~0x80); */
	W_REG(sii->osh, &cc->regcontrol_data, 0x3001);

	OSL_DELAY(5000);

	/* request ALP_AVAIL through PMU to move sb out of ILP */
	W_REG(sii->osh, &cc->min_res_mask, 0x0d);

	SPINWAIT(((ccst = OSL_PCI_READ_CONFIG(sii->osh, PCI_CLK_CTL_ST, 4)) & CCS_ALPAVAIL)
		 == 0, PMU_MAX_TRANSITION_DLY);

	if ((ccst & CCS_ALPAVAIL) == 0) {
		SI_ERROR(("ALP never came up clk_ctl_st: 0x%x\n", ccst));
		return FALSE;
	}
	SI_MSG(("si_ldo_war: 4322a0 HACK done\n"));

	OSL_PCI_WRITE_CONFIG(sii->osh, PCI_BAR0_WIN, sizeof(uint32), w);

	return TRUE;
}

static bool
BCMATTACHFN(si_buscore_prep)(si_info_t *sii, uint bustype, uint devid, void *sdh)
{
	/* need to set memseg flag for CF card first before any sb registers access */
	if (BUSTYPE(bustype) == PCMCIA_BUS)
		sii->memseg = TRUE;

	if (BUSTYPE(bustype) == PCI_BUS) {
		if (!si_ldo_war((si_t *)sii, devid))
			return FALSE;
	}

	/* kludge to enable the clock on the 4306 which lacks a slowclock */
	if (BUSTYPE(bustype) == PCI_BUS && !si_ispcie(sii))
		si_clkctl_xtal(&sii->pub, XTAL|PLL, ON);


	return TRUE;
}

static bool
BCMATTACHFN(si_buscore_setup)(si_info_t *sii, chipcregs_t *cc, uint bustype, uint32 savewin,
	uint *origidx, void *regs)
{
	bool pci, pcie, pcie_gen2 = FALSE;
	uint i;
	uint pciidx, pcieidx, pcirev, pcierev;

	cc = si_setcoreidx(&sii->pub, SI_CC_IDX);
	ASSERT((uintptr)cc);

	/* get chipcommon rev */
	sii->pub.ccrev = (int)si_corerev(&sii->pub);

	/* get chipcommon chipstatus */
	if (sii->pub.ccrev >= 11)
		sii->pub.chipst = R_REG(sii->osh, &cc->chipstatus);

	/* get chipcommon capabilites */
	sii->pub.cccaps = R_REG(sii->osh, &cc->capabilities);
	/* get chipcommon extended capabilities */

	if (sii->pub.ccrev >= 35)
		sii->pub.cccaps_ext = R_REG(sii->osh, &cc->capabilities_ext);

	/* get pmu rev and caps */
	if (sii->pub.cccaps & CC_CAP_PMU) {
		sii->pub.pmucaps = R_REG(sii->osh, &cc->pmucapabilities);
		sii->pub.pmurev = sii->pub.pmucaps & PCAP_REV_MASK;
	}

	SI_MSG(("Chipc: rev %d, caps 0x%x, chipst 0x%x pmurev %d, pmucaps 0x%x\n",
		sii->pub.ccrev, sii->pub.cccaps, sii->pub.chipst, sii->pub.pmurev,
		sii->pub.pmucaps));

	/* figure out bus/orignal core idx */
	sii->pub.buscoretype = NODEV_CORE_ID;
	sii->pub.buscorerev = (uint)NOREV;
	sii->pub.buscoreidx = BADIDX;

	pci = pcie = FALSE;
	pcirev = pcierev = (uint)NOREV;
	pciidx = pcieidx = BADIDX;

	for (i = 0; i < sii->numcores; i++) {
		uint cid, crev;

		si_setcoreidx(&sii->pub, i);
		cid = si_coreid(&sii->pub);
		crev = si_corerev(&sii->pub);

		/* Display cores found */
		SI_VMSG(("CORE[%d]: id 0x%x rev %d base 0x%x regs 0x%p\n",
		        i, cid, crev, sii->coresba[i], sii->regs[i]));

		if (BUSTYPE(bustype) == PCI_BUS) {
			if (cid == PCI_CORE_ID) {
				pciidx = i;
				pcirev = crev;
				pci = TRUE;
			} else if ((cid == PCIE_CORE_ID) || (cid == PCIE2_CORE_ID)) {
				pcieidx = i;
				pcierev = crev;
				pcie = TRUE;
				if (cid == PCIE2_CORE_ID)
					pcie_gen2 = TRUE;
			}
		} else if ((BUSTYPE(bustype) == PCMCIA_BUS) &&
		           (cid == PCMCIA_CORE_ID)) {
			sii->pub.buscorerev = crev;
			sii->pub.buscoretype = cid;
			sii->pub.buscoreidx = i;
		}

		/* find the core idx before entering this func. */
		if ((savewin && (savewin == sii->coresba[i])) ||
		    (regs == sii->regs[i]))
			*origidx = i;
	}

	if (pci && pcie) {
		if (si_ispcie(sii))
			pci = FALSE;
		else
			pcie = FALSE;
	}
	if (pci) {
		sii->pub.buscoretype = PCI_CORE_ID;
		sii->pub.buscorerev = pcirev;
		sii->pub.buscoreidx = pciidx;
	} else if (pcie) {
		if (pcie_gen2)
			sii->pub.buscoretype = PCIE2_CORE_ID;
		else
			sii->pub.buscoretype = PCIE_CORE_ID;
		sii->pub.buscorerev = pcierev;
		sii->pub.buscoreidx = pcieidx;
	}

	SI_VMSG(("Buscore id/type/rev %d/0x%x/%d\n", sii->pub.buscoreidx, sii->pub.buscoretype,
	         sii->pub.buscorerev));

	if (BUSTYPE(sii->pub.bustype) == SI_BUS && (CHIPID(sii->pub.chip) == BCM4712_CHIP_ID) &&
	    (sii->pub.chippkg != BCM4712LARGE_PKG_ID) && (CHIPREV(sii->pub.chiprev) <= 3))
		OR_REG(sii->osh, &cc->slow_clk_ctl, SCC_SS_XTAL);

	/* fixup necessary chip/core configurations */
	if (BUSTYPE(sii->pub.bustype) == PCI_BUS) {
		if (SI_FAST(sii)) {
			if (!sii->pch &&
			    ((sii->pch = (void *)(uintptr)pcicore_init(&sii->pub, sii->osh,
				(void *)PCIEREGS(sii))) == NULL))
				return FALSE;
		}
		if (si_pci_fixcfg(&sii->pub)) {
			SI_ERROR(("si_doattach: si_pci_fixcfg failed\n"));
			return FALSE;
		}
	}


	/* return to the original core */
	si_setcoreidx(&sii->pub, *origidx);

	return TRUE;
}

static uint32
si_fixup_vid(si_info_t *sii, char *pvars, uint32 conf_vid)
{
	struct si_pub *sih = &sii->pub;
	uint32 srom_vid;

	if (BUSTYPE(sih->bustype) != PCI_BUS)
		return conf_vid;

	if ((CHIPID(sih->chip) != BCM4331_CHIP_ID) && (CHIPID(sih->chip) != BCM43431_CHIP_ID))
		return conf_vid;

	/* Ext PA Controls for 4331 12x9 Package */
	if (sih->chippkg != 9)
		return conf_vid;

	srom_vid = (getintvar(pvars, "boardtype") << 16) | getintvar(pvars, "subvid");
	if (srom_vid != conf_vid) {
		SI_ERROR(("%s: Override mismatch conf_vid(0x%04x) with srom_vid(0x%04x)\n",
			__FUNCTION__, conf_vid, srom_vid));
		conf_vid = srom_vid;
	}

	return conf_vid;
}

static void
BCMATTACHFN(si_nvram_process)(si_info_t *sii, char *pvars)
{
	uint w = 0;
	if (BUSTYPE(sii->pub.bustype) == PCMCIA_BUS) {
		w = getintvar(pvars, "regwindowsz");
		sii->memseg = (w <= CFTABLE_REGWIN_2K) ? TRUE : FALSE;
	}

	/* get boardtype and boardrev */
	switch (BUSTYPE(sii->pub.bustype)) {
	case PCI_BUS:
		/* do a pci config read to get subsystem id and subvendor id */
		w = OSL_PCI_READ_CONFIG(sii->osh, PCI_CFG_SVID, sizeof(uint32));
		w = si_fixup_vid(sii, pvars, w);

		/* Let nvram variables override subsystem Vend/ID */
		if ((sii->pub.boardvendor = (uint16)si_getdevpathintvar(&sii->pub, "boardvendor"))
			== 0) {
#ifdef BCMHOSTVARS
			if ((w & 0xffff) == 0)
				sii->pub.boardvendor = VENDOR_BROADCOM;
			else
#endif /* !BCMHOSTVARS */
				sii->pub.boardvendor = w & 0xffff;
		}
		else
			SI_ERROR(("Overriding boardvendor: 0x%x instead of 0x%x\n",
				sii->pub.boardvendor, w & 0xffff));
		if ((sii->pub.boardtype = (uint16)si_getdevpathintvar(&sii->pub, "boardtype"))
			== 0) {
			if ((sii->pub.boardtype = getintvar(pvars, "boardtype")) == 0)
				sii->pub.boardtype = (w >> 16) & 0xffff;
		}
		else
			SI_ERROR(("Overriding boardtype: 0x%x instead of 0x%x\n",
				sii->pub.boardtype, (w >> 16) & 0xffff));
		break;

	case PCMCIA_BUS:
		sii->pub.boardvendor = getintvar(pvars, "manfid");
		sii->pub.boardtype = getintvar(pvars, "prodid");
		break;


	case SI_BUS:
	case JTAG_BUS:
		sii->pub.boardvendor = VENDOR_BROADCOM;
		if (pvars == NULL || ((sii->pub.boardtype = getintvar(pvars, "prodid")) == 0))
			if ((sii->pub.boardtype = getintvar(NULL, "boardtype")) == 0)
				sii->pub.boardtype = 0xffff;

		if (CHIPTYPE(sii->pub.socitype) == SOCI_UBUS) {
			/* do a pci config read to get subsystem id and subvendor id */
			w = OSL_PCI_READ_CONFIG(sii->osh, PCI_CFG_SVID, sizeof(uint32));
			sii->pub.boardvendor = w & 0xffff;
			sii->pub.boardtype = (w >> 16) & 0xffff;
		}
		break;
	}

	if (sii->pub.boardtype == 0) {
		SI_ERROR(("si_doattach: unknown board type\n"));
		ASSERT(sii->pub.boardtype);
	}

	sii->pub.boardrev = getintvar(pvars, "boardrev");
	sii->pub.boardflags = getintvar(pvars, "boardflags");
#ifdef BCM_SDRBL
	sii->pub.boardflags2 |= ((!CHIP_HOSTIF_USB(&(sii->pub))) ? ((si_arm_sflags(&(sii->pub))
				 & SISF_SDRENABLE) ?  BFL2_SDR_EN:0):
				 (((uint)getintvar(pvars, "boardflags2")) & BFL2_SDR_EN));
#endif /* BCM_SDRBL */
}

#if (!defined(_CFE_) && !defined(_CFEZ_)) || defined(CFG_WL)
static void
BCMATTACHFN(si_sromvars_fixup_4331)(si_t *sih, char *pvars)
{

	const char *sromvars[] =
	        {"extpagain2g", "extpagain5g"};
	int sromvars_size = sizeof(sromvars)/sizeof(char *);
	int ii;
	uint boardtype = sih->boardtype;
	uint boardrev = sih->boardrev;
	bool update = ((boardtype == BCM94331BU_SSID) ||
	               (boardtype == BCM94331S9BU_SSID) ||
	               (boardtype == BCM94331MCI_SSID) ||
	               (boardtype == BCM94331MC_SSID) ||
	               (boardtype == BCM94331PCIEBT4_SSID) ||
	               (boardtype == BCM94331X19 && boardrev == 0x1100) ||
	               (boardtype == BCM94331HM_SSID && boardrev < 0x1152));

	if (pvars == NULL || !update) {
		return;
	}

	for (ii = 0; ii < sromvars_size; ii++) {
		char* val = getvar(pvars, sromvars[ii]);

		while (val && *val) {
			*val = '0';
			val++;
		}
	}
}
#endif /* (!_CFE_ && !_CFEZ_) || CFG_WL */

#if defined(CONFIG_XIP) && defined(BCMTCAM)
extern uint8 patch_pair;
#endif /* CONFIG_XIP && BCMTCAM */

typedef struct {
	uint8 uart_tx;
	uint32 uart_rx;
} si_mux_uartopt_t;

/* note: each index corr to MUXENAB4335_UART mask >> shift - 1 */
static const si_mux_uartopt_t BCMATTACHDATA(mux4335_uartopt)[] = {
		{CC4335_PIN_GPIO_06, CC4335_PIN_GPIO_02},
		{CC4335_PIN_GPIO_12, CC4335_PIN_GPIO_13},
		{CC4335_PIN_SDIO_DATA0, CC4335_PIN_SDIO_CMD},
		{CC4335_PIN_RF_SW_CTRL_9, CC4335_PIN_RF_SW_CTRL_8}
};

/* note: each index corr to MUXENAB4335_HOSTWAKE mask > shift - 1 */
static const uint8 BCMATTACHDATA(mux4335_hostwakeopt)[] = {
		CC4335_PIN_GPIO_00,
		CC4335_PIN_GPIO_05,
		CC4335_PIN_GPIO_09
};


/* want to have this available all the time to switch mux for debugging */
void
BCMATTACHFN(si_muxenab)(si_t *sih, uint32 w)
{
	uint32 chipcontrol, pmu_chipcontrol;

	pmu_chipcontrol = si_pmu_chipcontrol(sih, 1, 0, 0);
	chipcontrol = si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol),
	                         0, 0);

	switch (CHIPID(sih->chip)) {
	case BCM4330_CHIP_ID:
		/* clear the bits */
		chipcontrol &= ~(CCTRL_4330_JTAG_DISABLE | CCTRL_4330_ERCX_SEL |
		                 CCTRL_4330_GPIO_SEL | CCTRL_4330_SDIO_HOST_WAKE);
		pmu_chipcontrol &= ~PCTL_4330_SERIAL_ENAB;

		/* 4330 default is to have jtag enabled */
		if (!(w & MUXENAB_JTAG))
			chipcontrol |= CCTRL_4330_JTAG_DISABLE;
		if (w & MUXENAB_UART)
			pmu_chipcontrol |= PCTL_4330_SERIAL_ENAB;
		if (w & MUXENAB_GPIO)
			chipcontrol |= CCTRL_4330_GPIO_SEL;
		if (w & MUXENAB_ERCX)
			chipcontrol |= CCTRL_4330_ERCX_SEL;
		if (w & MUXENAB_HOST_WAKE)
			chipcontrol |= CCTRL_4330_SDIO_HOST_WAKE;
		break;
	case BCM4336_CHIP_ID:
		if (w & MUXENAB_UART)
			pmu_chipcontrol |= PCTL_4336_SERIAL_ENAB;
		else
			pmu_chipcontrol &= ~PCTL_4336_SERIAL_ENAB;
		break;
	case BCM4360_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM4352_CHIP_ID:
	case BCM43526_CHIP_ID:
		if (w & MUXENAB_UART)
			chipcontrol |= CCTL_4360_UART_SEL;
		break;
	case BCM43143_CHIP_ID:
		chipcontrol = 0;
		/* 43143 does not support ERCX */
		if (!(w & MUXENAB_UART))
			chipcontrol |= CCTRL_43143_RF_XSWCTRL;
		/* JTAG is enabled when SECI is disabled */
		if (w & MUXENAB_SECI)
			chipcontrol |= CCTRL_43143_SECI;
		if (w & MUXENAB_BT_LEGACY)
			chipcontrol |= CCTRL_43143_BT_LEGACY;
		if (w & MUXENAB_I2S_EN)
			chipcontrol |= CCTRL_43143_I2S_MODE;
		if (w & MUXENAB_I2S_MASTER)
			chipcontrol |= CCTRL_43143_I2S_MASTER;
		if (w & MUXENAB_I2S_FULL)
			chipcontrol |= CCTRL_43143_I2S_FULL;
		if (!(w & MUXENAB_SFLASH))
			chipcontrol |= CCTRL_43143_GSIO;
		if (w & MUXENAB_RFSWCTRL0)
			chipcontrol |= CCTRL_43143_RF_SWCTRL_0;
		if (w & MUXENAB_RFSWCTRL1)
			chipcontrol |= CCTRL_43143_RF_SWCTRL_1;
		if (w & MUXENAB_RFSWCTRL2)
			chipcontrol |= CCTRL_43143_RF_SWCTRL_2;
		if (w & MUXENAB_HOST_WAKE)
			chipcontrol |= CCTRL_43143_HOST_WAKE0;
		if (w & MUXENAB_HOST_WAKE1)
			chipcontrol |= CCTRL_43143_HOST_WAKE1;
		break;
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
		/* clear the bits */
		pmu_chipcontrol &= ~CCTRL1_4324_GPIO_SEL;
		if (w & MUXENAB_GPIO)
			pmu_chipcontrol |= CCTRL1_4324_GPIO_SEL;
		break;

	case BCM4335_CHIP_ID:
		/* drive default pins for UART. Note: 15 values possible;
		* 0 means disabled; 1 means index to 0 in mux4335_uartopt
		* array, etc.
		*/
		if (w & MUXENAB4335_UART_MASK) {
			uint32 uart_rx = 0, uart_tx = 0;
			uint8 uartopt_ix = MUXENAB4335_GETIX(w, UART);

			uart_rx = mux4335_uartopt[uartopt_ix].uart_rx;
			uart_tx = mux4335_uartopt[uartopt_ix].uart_tx;

			if (uartopt_ix >
				sizeof(mux4335_uartopt)/sizeof(mux4335_uartopt[0]) - 1) {
				SI_ERROR(("%s: wrong index %d for uart\n",
					__FUNCTION__, uartopt_ix));
				break;
			}

			si_gci_set_functionsel(sih, uart_rx, CC4335_FNSEL_UART);
			si_gci_set_functionsel(sih, uart_tx, CC4335_FNSEL_UART);

			if ((uart_rx == CC4335_PIN_GPIO_02) && (uart_tx == CC4335_PIN_GPIO_06))
				si_gci_chipcontrol(sih, CC_GCI_CHIPCTRL_06,
					CC_GCI_06_JTAG_SEL_MASK,
					(1 << CC_GCI_06_JTAG_SEL_SHIFT));
		}
		/*
		* 0x10 : use GPIO0 as host wake up pin
		* 0x20 : use GPIO5 as host wake up pin
		* 0x30 : use GPIO9 as host wake up pin
		* 0x40 ~ 0xf0: Reserved
		*/
		if (w & MUXENAB4335_HOSTWAKE_MASK) {
			uint8 hostwake = 0;
			uint8 hostwake_ix = MUXENAB4335_GETIX(w, HOSTWAKE);

			if (hostwake_ix >
				sizeof(mux4335_hostwakeopt)/sizeof(mux4335_hostwakeopt[0]) - 1) {
				SI_ERROR(("%s: wrong index %d for hostwake\n",
					__FUNCTION__, hostwake_ix));
				break;
			}

			hostwake = mux4335_hostwakeopt[hostwake_ix];
			si_gci_set_functionsel(sih, hostwake, CC4335_FNSEL_MISC1);
		}
		break;

	default:
		/* muxenab specified for an unsupported chip */
		ASSERT(0);
		break;
	}

	/* write both updated values to hw */
	si_pmu_chipcontrol(sih, 1, ~0, pmu_chipcontrol);
	si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol),
	           ~0, chipcontrol);
}

/* ltecx GCI reg access */
uint32
si_gci_direct(si_t *sih, uint offset, uint32 mask, uint32 val)
{
	/* gci direct reg access */
	return si_corereg(sih, SI_CC_IDX, offset, mask, val);
}
uint32
si_gci_indirect(si_t *sih, uint regidx, uint offset, uint32 mask, uint32 val)
{
	/* gci indirect reg access */
	si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gci_indirect_addr), ~0, regidx);
	return si_corereg(sih, SI_CC_IDX, offset, mask, val);
}
uint32
si_gci_input(si_t *sih, uint reg)
{
	/* gci_input[] */
	return si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gci_input[reg]), 0, 0);
}
uint32
si_gci_output(si_t *sih, uint reg, uint32 mask, uint32 val)
{
	/* gci_output[] */
	return si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gci_output[reg]), mask, val);
}
uint32
si_gci_int_enable(si_t *sih, bool enable)
{
	uint offs;

	/* enable GCI interrupt */
	offs = OFFSETOF(chipcregs_t, intmask);
	return (si_corereg(sih, SI_CC_IDX, offs, CI_ECI, (enable ? CI_ECI : 0)));
}
void
si_gci_reset(si_t *sih)
{
	int i;

	/* reset SECI block */
	si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_corectrl), 0xFFFFFFFF, 0x01);
	for (i = 0; i < 100; i++);
	si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_corectrl), 0xFFFFFFFF, 0x00);

	/* clear events */
	for (i = 0; i < 32; i++)
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_event[i]), 0xFFFFFFFF, 0x00);
}
void
si_ercx_init(si_t *sih)
{
	if (CHIPID(sih->chip) == BCM4334_CHIP_ID) {
		/* enable ERCX: jtagSel bit=0, gpio as ercx bit=1 */
		si_pmu_chipcontrol(sih, PMU1_PLL0_CHIPCTL1, 0x0000000F, 0x0000000A);
	}
	else if (CHIPID(sih->chip) == BCM4335_CHIP_ID ||
		0) {
		/* reset GCI block */
		si_gci_reset(sih);

		/* enable ERCX (pure gpio) mode */
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_corectrl), 0xFFFFFFFF, 0x30);
		/* config GPIO 8-9-12-13 as pure GPIO for ERCX */
		si_gci_set_functionsel(sih, CC4335_PIN_GPIO_08, CC4335_FNSEL_GCI0);
		si_gci_set_functionsel(sih, CC4335_PIN_GPIO_09, CC4335_FNSEL_GCI0);
		si_gci_set_functionsel(sih, CC4335_PIN_GPIO_12, CC4335_FNSEL_GCI0);
		si_gci_set_functionsel(sih, CC4335_PIN_GPIO_13, CC4335_FNSEL_GCI0);
		/* gpio-0 as output & gpio1/2/3 as input */
		si_gci_indirect(sih, 0,
			OFFSETOF(chipcregs_t, gci_gpioctl), 0x01010102, 0x01010102);
		si_gci_indirect(sih, 1,
			OFFSETOF(chipcregs_t, gci_gpioctl), 0x00000000, 0x00000000);
		/* gpio mapping: wlan_prio(gpio0),frmsync(gpio1),mws_rx(gpio2),mws_tx(gpio3) */
		si_gci_indirect(sih, 0x00000,
			OFFSETOF(chipcregs_t, gci_gpiomask), 0x00000010, 0x00000010);
		si_gci_indirect(sih, 0x10010,
			OFFSETOF(chipcregs_t, gci_gpiomask), 0x00000001, 0x00000001);
		si_gci_indirect(sih, 0x20010,
			OFFSETOF(chipcregs_t, gci_gpiomask), 0x00000002, 0x00000002);
		si_gci_indirect(sih, 0x30010,
			OFFSETOF(chipcregs_t, gci_gpiomask), 0x00000004, 0x00000004);
	}
}
void
si_wci2_init(si_t *sih)
{
	if (CHIPID(sih->chip) == BCM4335_CHIP_ID ||
		0) {
		/* reset GCI block */
		si_gci_reset(sih);

		/* enable BT-SIG mode */
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_corectrl), 0xFFFFFFFF, 0x24);
		/* config GPIO pins 8/9 as SECI_IN/SECI_OUT */
		si_gci_set_functionsel(sih, CC4335_PIN_GPIO_08, CC4335_FNSEL_GCI0);
		si_gci_set_functionsel(sih, CC4335_PIN_GPIO_09, CC4335_FNSEL_GCI0);
		/* baudrate:3mbps, escseq:0xdb, high baudrate, enable seci_tx/rx */
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_miscctl), 0x000F, 0x0000);
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_secibauddiv), 0xFFFFFFFF, 0xF4);
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_secifcr), 0xFFFFFFFF, 0x00);
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_secimcr), 0xFFFFFFFF, 0x89);
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_secilcr), 0xFFFFFFFF, 0x28);
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_uartescval), 0xFFFFFFFF, 0xDB);
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_baudadj), 0xFFFFFFFF, 0x22);

		/* GPIO 3-7 as BT_SIG complaint */
		/* config GPIO pins 3-7 as input */
		si_gci_indirect(sih, 0,
			OFFSETOF(chipcregs_t, gci_gpioctl), 0x20000000, 0x20000000);
		si_gci_indirect(sih, 1,
			OFFSETOF(chipcregs_t, gci_gpioctl), 0x20202020, 0x20202020);
		/* gpio mapping: frmsync-gpio7, mws_rx-gpio6, mws_tx-gpio5,
		 * pat[0]-gpio4, pat[1]-gpio3
		 */
		si_gci_indirect(sih, 0x70010,
			OFFSETOF(chipcregs_t, gci_gpiomask), 0x00000001, 0x00000001);
		si_gci_indirect(sih, 0x60010,
			OFFSETOF(chipcregs_t, gci_gpiomask), 0x00000002, 0x00000002);
		si_gci_indirect(sih, 0x50010,
			OFFSETOF(chipcregs_t, gci_gpiomask), 0x00000004, 0x00000004);
		si_gci_indirect(sih, 0x40010,
			OFFSETOF(chipcregs_t, gci_gpiomask), 0x06000000, 0x06000000);
		si_gci_indirect(sih, 0x30010,
			OFFSETOF(chipcregs_t, gci_gpiomask), 0x08000000, 0x08000000);
		/* gpio mapping: wlan_rx_prio-gpio5, wlan_tx_on-gpio4 */
		si_gci_indirect(sih, 0x50000,
			OFFSETOF(chipcregs_t, gci_gpiomask), 0x00000010, 0x00000010);
		si_gci_indirect(sih, 0x40000,
			OFFSETOF(chipcregs_t, gci_gpiomask), 0x00000020, 0x00000020);
		/* enable gpio out on gpio4(wlanrxprio), gpio5(wlantxon) */
		si_gci_direct(sih,
			OFFSETOF(chipcregs_t, gci_control_0), 0x00000030, 0x00000030);
	}
}

uint16
si_cc_get_reg16(uint32 reg_offs)
{
	return (*((volatile uint16 *)((char *)SI_ENUM_BASE + reg_offs)));
}

uint32
si_cc_get_reg32(uint32 reg_offs)
{
	return (*((volatile uint32 *)((char *)SI_ENUM_BASE + reg_offs)));
}

uint32
si_cc_set_reg32(uint32 reg_offs, uint32 val)
{
	*((volatile uint32 *)((char *)SI_ENUM_BASE + reg_offs)) = val;
	return si_cc_get_reg32(reg_offs);
}

uint32
si_gci_preinit_upd_indirect(uint32 regidx, uint32 setval, uint32 mask)
{
	uint32 val = 0;

	si_cc_set_reg32(CC_GCI_INDIRECT_ADDR_REG, regidx);
	val = si_cc_get_reg32(CC_GCI_CHIP_CTRL_REG);

	val &= ~mask;
	val |= setval;

	return si_cc_set_reg32(CC_GCI_CHIP_CTRL_REG, val);
}

void
si_gci_seci_init(si_t *sih)
{
	if (CHIPID(sih->chip) == BCM4335_CHIP_ID ||
		0) {
		/* reset GCI block */
		si_gci_reset(sih);

		/* enable SECI mode */
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_corectrl), 0xFFFFFFFF, 0x14);
		/* config GPIO pins 8/9 as SECI_IN/SECI_OUT */
		si_gci_set_functionsel(sih, CC4335_PIN_GPIO_08, CC4335_FNSEL_GCI0);
		si_gci_set_functionsel(sih, CC4335_PIN_GPIO_09, CC4335_FNSEL_GCI0);
		/* baudrate:3mbps, escseq:0xdb, high baudrate, enable seci_tx/rx */
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_miscctl), 0x0000000F, 0x0000);
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_secibauddiv), 0xFFFFFFFF, 0xF4);
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_secifcr), 0xFFFFFFFF, 0x00);
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_secimcr), 0xFFFFFFFF, 0x89);
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_secilcr), 0xFFFFFFFF, 0x28);
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_uartescval), 0xFFFFFFFF, 0xDB);
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_baudadj), 0xFFFFFFFF, 0x22);

		/* map nibble from IP=4 (LTE) with addr 0-11 to LTE space
		 * (lower nibble addr; upper nibble IP)
		 */
		si_gci_indirect(sih, 0,
			OFFSETOF(chipcregs_t, gci_secif0rx_offset), 0xFFFFFFFF, 0x43424140);
		si_gci_indirect(sih, 1,
			OFFSETOF(chipcregs_t, gci_secif0rx_offset), 0xFFFFFFFF, 0x47464544);
		si_gci_indirect(sih, 2,
			OFFSETOF(chipcregs_t, gci_secif0rx_offset), 0xFFFFFFFF, 0x4b4a4948);

		/* select nibbles to be communicated using format-I: wlan nibble 1/4, bt nibble 1 */
		/* note: we can only select 1st 12 nibbles of each IP for format_0 */
		si_gci_indirect(sih, 0,
			OFFSETOF(chipcregs_t, gci_seciusef0tx_reg), 0xFFFFFFFF, 0x00000012);
		si_gci_indirect(sih, 1,
			OFFSETOF(chipcregs_t, gci_seciusef0tx_reg), 0xFFFFFFFF, 0x00000002);

		/* assigns address to To LTE nibbles from BT-WLAN IP space (addr 0 to 11) */
		/* wlan nibble1: addr0, wlan nibble4: addr1, wlan nibble12: can't be communicated */
		/* bt nibble1: addr2, bt nibble12/13: can't be communicated */
		si_gci_indirect(sih, 0,
			OFFSETOF(chipcregs_t, gci_secif0tx_offset), 0x000F00F0, 0x00010000);
		si_gci_indirect(sih, 4,
			OFFSETOF(chipcregs_t, gci_secif0tx_offset), 0x000000F0, 0x00000020);

		/* enable wlan nibble 1 and 4 control bits */
		/* NOTE: BT should enable bits for nibble 1 */
		si_gci_direct(sih,
			OFFSETOF(chipcregs_t, gci_control_0), 0xFFFFFFFF, 0x000F00F0);

		/* mailbox 1 to 1 mapping:
		 * mailbox data generated by an IP goes to its own mailbox space in peer GCI chip
		 */
		si_gci_direct(sih,
			OFFSETOF(chipcregs_t, gci_secif1tx_offset), 0xFFFFFFFF, 0x00043210);
	}
}


/* write 'val' to the gci chip control register indexed by 'reg' */
uint32
si_gci_chipcontrol(si_t *sih, uint reg, uint32 mask, uint32 val)
{
	/* because NFLASH and GCI clashes in 0xC00 */
#ifndef NFLASH_SUPPORT
	si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gci_indirect_addr), ~0, reg);
	return si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gci_chipctrl), mask, val);
#else /* NFLASH_SUPPORT */
	ASSERT(0);
	return 0xFFFFFFFF;
#endif
}

/* input: pin number
* output: chipcontrol reg and bits to shift for pin fn-sel's first regbit.
* eg: gpio9 will give regidx: 1 and pos 4
*/
uint8
si_gci_get_chipctrlreg_idx(uint32 pin, uint32 *regidx, uint32 *pos)
{
	*regidx = (pin / 8);
	*pos = (pin % 8)*4;

	SI_MSG(("si_gci_get_chipctrlreg_idx:%d:%d:%d\n", pin, *regidx, *pos));

	return 0;
}

/* setup a given pin for fnsel function */
void
si_gci_set_functionsel(si_t *sih, uint32 pin, uint8 fnsel)
{
	uint32 reg = 0, pos = 0;

	SI_MSG(("si_gci_set_functionsel:%d\n", pin));

	si_gci_get_chipctrlreg_idx(pin, &reg, &pos);
	si_gci_chipcontrol(sih, reg, GCIMASK(pos), GCIPOSVAL(fnsel, pos));
}

void
si_gci_chipctrl_overrides(osl_t *osh, si_t *sih, char *pvars)
{
	uint8 num_cc = 0;
	char gciccstr[16];
	const char *otp_val;
	uint32 gciccval = 0, cap1 = 0;
	int i = 0;

/* because NFLASH and GCI clashes in 0xC00 */
#ifndef NFLASH_SUPPORT
	cap1 = si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gci_corecaps1), 0, 0);
#else /* NFLASH_SUPPORT */
	ASSERT(0);
#endif
	num_cc = CC_GCI_NUMCHIPCTRLREGS(cap1);

	for (i = 0; i < num_cc; i++) {
		snprintf(gciccstr, sizeof(gciccstr), "gcr%d", i);

		if ((otp_val = getvar(NULL, gciccstr)) == NULL)
			continue;

		gciccval = (uint32) getintvar(pvars, gciccstr);
		si_gci_chipcontrol(sih, i, ~0, gciccval);
	}
}

static si_info_t *
BCMATTACHFN(si_doattach)(si_info_t *sii, uint devid, osl_t *osh, void *regs,
                       uint bustype, void *sdh, char **vars, uint *varsz)
{
	struct si_pub *sih = &sii->pub;
	uint32 w, savewin;
	chipcregs_t *cc;
	char *pvars = NULL;
	uint origidx;
#if (!defined(_CFE_) && !defined(_CFEZ_)) || defined(CFG_WL)
	bool fixup_boardtype = FALSE;
#endif /* (!_CFE_ && !_CFEZ_) || CFG_WL */
	ASSERT(GOODREGS(regs));

	bzero((uchar*)sii, sizeof(si_info_t));

	savewin = 0;

	sih->buscoreidx = BADIDX;

	sii->curmap = regs;
	sii->sdh = sdh;
	sii->osh = osh;

#ifdef SI_ENUM_BASE_VARIABLE
	si_enum_base_init(sih, bustype);
#endif /* SI_ENUM_BASE_VARIABLE */

	/* check to see if we are a si core mimic'ing a pci core */
	if ((bustype == PCI_BUS) &&
	    (OSL_PCI_READ_CONFIG(sii->osh, PCI_SPROM_CONTROL, sizeof(uint32)) == 0xffffffff)) {
		SI_ERROR(("%s: incoming bus is PCI but it's a lie, switching to SI "
		          "devid:0x%x\n", __FUNCTION__, devid));
		bustype = SI_BUS;
	}

	/* find Chipcommon address */
	if (bustype == PCI_BUS) {
		savewin = OSL_PCI_READ_CONFIG(sii->osh, PCI_BAR0_WIN, sizeof(uint32));
		if (!GOODCOREADDR(savewin, SI_ENUM_BASE))
			savewin = SI_ENUM_BASE;
		OSL_PCI_WRITE_CONFIG(sii->osh, PCI_BAR0_WIN, 4, SI_ENUM_BASE);
		if (!regs)
			return NULL;
		cc = (chipcregs_t *)regs;
	} else {
		cc = (chipcregs_t *)REG_MAP(SI_ENUM_BASE, SI_CORE_SIZE);
	}

	sih->bustype = bustype;
	if (bustype != BUSTYPE(bustype)) {
		SI_ERROR(("si_doattach: bus type %d does not match configured bus type %d\n",
			bustype, BUSTYPE(bustype)));
		return NULL;
	}

	/* bus/core/clk setup for register access */
	if (!si_buscore_prep(sii, bustype, devid, sdh)) {
		SI_ERROR(("si_doattach: si_core_clk_prep failed %d\n", bustype));
		return NULL;
	}

	/* ChipID recognition.
	 *   We assume we can read chipid at offset 0 from the regs arg.
	 *   If we add other chiptypes (or if we need to support old sdio hosts w/o chipcommon),
	 *   some way of recognizing them needs to be added here.
	 */
	if (!cc) {
		SI_ERROR(("%s: chipcommon register space is null \n", __FUNCTION__));
		return NULL;
	}
	w = R_REG(osh, &cc->chipid);
	sih->socitype = (w & CID_TYPE_MASK) >> CID_TYPE_SHIFT;
	/* Might as wll fill in chip id rev & pkg */
	sih->chip = w & CID_ID_MASK;
	sih->chiprev = (w & CID_REV_MASK) >> CID_REV_SHIFT;
	sih->chippkg = (w & CID_PKG_MASK) >> CID_PKG_SHIFT;

	if ((CHIPID(sih->chip) == BCM4329_CHIP_ID) && (sih->chiprev == 0) &&
		(sih->chippkg != BCM4329_289PIN_PKG_ID)) {
		sih->chippkg = BCM4329_182PIN_PKG_ID;
	}
	sih->issim = IS_SIM(sih->chippkg);

	/* scan for cores */
	if (CHIPTYPE(sii->pub.socitype) == SOCI_SB) {
		SI_MSG(("Found chip type SB (0x%08x)\n", w));
		sb_scan(&sii->pub, regs, devid);
	} else if ((CHIPTYPE(sii->pub.socitype) == SOCI_AI) ||
		(CHIPTYPE(sii->pub.socitype) == SOCI_NAI)) {
		if (CHIPTYPE(sii->pub.socitype) == SOCI_AI)
			SI_MSG(("Found chip type AI (0x%08x)\n", w));
		else
			SI_MSG(("Found chip type NAI (0x%08x)\n", w));
		/* pass chipc address instead of original core base */
		ai_scan(&sii->pub, (void *)(uintptr)cc, devid);
	} else if (CHIPTYPE(sii->pub.socitype) == SOCI_UBUS) {
		SI_MSG(("Found chip type UBUS (0x%08x), chip id = 0x%4x\n", w, sih->chip));
		/* pass chipc address instead of original core base */
		ub_scan(&sii->pub, (void *)(uintptr)cc, devid);
	} else {
		SI_ERROR(("Found chip of unknown type (0x%08x)\n", w));
		return NULL;
	}
	/* no cores found, bail out */
	if (sii->numcores == 0) {
		SI_ERROR(("si_doattach: could not find any cores\n"));
		return NULL;
	}
	/* bus/core/clk setup */
	origidx = SI_CC_IDX;
	if (!si_buscore_setup(sii, cc, bustype, savewin, &origidx, regs)) {
		SI_ERROR(("si_doattach: si_buscore_setup failed\n"));
		goto exit;
	}

#if (!defined(_CFE_) && !defined(_CFEZ_)) || defined(CFG_WL)
	if (CHIPID(sih->chip) == BCM4322_CHIP_ID && (((sih->chipst & CST4322_SPROM_OTP_SEL_MASK)
		>> CST4322_SPROM_OTP_SEL_SHIFT) == (CST4322_OTP_PRESENT |
		CST4322_SPROM_PRESENT))) {
		SI_ERROR(("%s: Invalid setting: both SPROM and OTP strapped.\n", __FUNCTION__));
		return NULL;
	}

	/* assume current core is CC */
	if ((sii->pub.ccrev == 0x25) && ((CHIPID(sih->chip) == BCM43236_CHIP_ID ||
	                                  CHIPID(sih->chip) == BCM43235_CHIP_ID ||
	                                  CHIPID(sih->chip) == BCM43234_CHIP_ID ||
	                                  CHIPID(sih->chip) == BCM43238_CHIP_ID) &&
	                                 (CHIPREV(sii->pub.chiprev) <= 2))) {

		if ((cc->chipstatus & CST43236_BP_CLK) != 0) {
			uint clkdiv;
			clkdiv = R_REG(osh, &cc->clkdiv);
			/* otp_clk_div is even number, 120/14 < 9mhz */
			clkdiv = (clkdiv & ~CLKD_OTP) | (14 << CLKD_OTP_SHIFT);
			W_REG(osh, &cc->clkdiv, clkdiv);
			SI_ERROR(("%s: set clkdiv to %x\n", __FUNCTION__, clkdiv));
		}
		OSL_DELAY(10);
	}

	if (bustype == PCI_BUS) {
		if ((CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43431_CHIP_ID)) {
			/* Check Ext PA Controls for 4331 12x9 Package before the fixup */
			if (sih->chippkg == 9) {
				uint32 val = si_chipcontrl_read(sih);
				fixup_boardtype = ((val & CCTRL4331_EXTPA_ON_GPIO2_5) ==
					CCTRL4331_EXTPA_ON_GPIO2_5);
			}
			/* set default mux pin to SROM */
			si_chipcontrl_epa4331(sih, FALSE);
			si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, watchdog), ~0, 100);
			OSL_DELAY(20000);	/* Srom read takes ~12mS */
		}

		if (((CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
		     (CHIPID(sih->chip) == BCM43460_CHIP_ID) ||
		     (CHIPID(sih->chip) == BCM4352_CHIP_ID)) &&
		    (CHIPREV(sih->chiprev) <= 2)) {
			pcie_disable_TL_clk_gating(sii->pch);
			pcie_set_L1_entry_time(sii->pch, 0x40);
		}

#ifdef BCMQT
		/* Set OTPClkDiv to smaller value otherwise OTP always reads 0xFFFF.
		 * For real-chip we shouldn't set OTPClkDiv to 2 because 20/2 = 10 > 9Mhz
		 * but for 4314 QT if we set it to 4. OTP reads 0xFFFF every two words.
		 */
		{
			uint otpclkdiv = 0;

			if ((CHIPID(sih->chip) == BCM4314_CHIP_ID) ||
				(CHIPID(sih->chip) == BCM43142_CHIP_ID)) {
				otpclkdiv = 2;
			} else if ((CHIPID(sih->chip) == BCM43131_CHIP_ID) ||
				(CHIPID(sih->chip) == BCM43217_CHIP_ID) ||
				(CHIPID(sih->chip) == BCM43227_CHIP_ID) ||
				(CHIPID(sih->chip) == BCM43228_CHIP_ID)) {
				otpclkdiv = 4;
			}

			if (otpclkdiv != 0) {
				uint clkdiv, savecore;
				savecore = si_coreidx(sih);
				si_setcore(sih, CC_CORE_ID, 0);

				clkdiv = R_REG(osh, &cc->clkdiv);
				clkdiv = (clkdiv & ~CLKD_OTP) | (otpclkdiv << CLKD_OTP_SHIFT);
				W_REG(osh, &cc->clkdiv, clkdiv);

				SI_ERROR(("%s: set clkdiv to 0x%x for QT\n", __FUNCTION__, clkdiv));
				si_setcoreidx(sih, savecore);
			}
		}
#endif /* BCMQT */
	}
#endif /* (!_CFE_ && !_CFEZ_) || CFG_WL */
#ifdef BCM_SDRBL
	/* 4360 rom bootloader in PCIE case, if the SDR is enabled, But preotection is
	 * not turned on, then we want to hold arm in reset.
	 * Bottomline: In sdrenable case, we allow arm to boot only when protection is
	 * turned on.
	 */
	if (CHIP_HOSTIF_PCIE(&(sii->pub))) {
		uint32 sflags = si_arm_sflags(&(sii->pub));

		/* If SDR is enabled but protection is not turned on
		* then we want to force arm to WFI.
		*/
		if ((sflags & (SISF_SDRENABLE | SISF_TCMPROT)) == SISF_SDRENABLE) {
			disable_arm_ints(PS_I);
			while (1) {
				hnd_cpu_wait(sih);
			}
		}
	}
#endif /* BCM_SDRBL */
#ifdef SI_SPROM_PROBE
	si_sprom_init(sih);
#endif /* SI_SPROM_PROBE */

#if !defined(BCMHIGHSDIO)
	/* Init nvram from flash if it exists */
	nvram_init((void *)&(sii->pub));

#if defined(_CFE_) && defined(BCM_DEVINFO)
	devinfo_nvram_init((void *)&(sii->pub));
#endif

	/* Init nvram from sprom/otp if they exist */
	if (srom_var_init(&sii->pub, BUSTYPE(bustype), regs, sii->osh, vars, varsz)) {
		SI_ERROR(("si_doattach: srom_var_init failed: bad srom\n"));
		goto exit;
	}
	pvars = vars ? *vars : NULL;

	si_nvram_process(sii, pvars);

#if (!defined(_CFE_) && !defined(_CFEZ_)) || defined(CFG_WL)
	if (bustype == PCI_BUS) {
		if ((CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43431_CHIP_ID)) {
			si_sromvars_fixup_4331(sih, pvars);
			if (fixup_boardtype)
				sii->pub.boardtype = getintvar(pvars, "boardtype");
		}
	}
#endif /* (!_CFE_ && !_CFEZ_) || CFG_WL */

	/* === NVRAM, clock is ready === */
#else
	pvars = NULL;
	BCM_REFERENCE(pvars);
#endif 


#if defined(CONFIG_XIP) && defined(BCMTCAM)
		/* patch the ROM if there are any patch pairs from OTP/SPROM */
		if (patch_pair) {

#if defined(__ARM_ARCH_7R__)
			hnd_tcam_bootloader_load(si_setcore(sih, ARMCR4_CORE_ID, 0), pvars);
#else
			hnd_tcam_bootloader_load(si_setcore(sih, SOCRAM_CORE_ID, 0), pvars);
#endif
			si_setcoreidx(sih, origidx);
		}
#endif /* CONFIG_XIP && BCMTCAM */

		/* bootloader should retain default pulls */
#ifndef BCM_BOOTLOADER
		if (sii->pub.ccrev >= 20) {
			uint32 gpiopullup = 0, gpiopulldown = 0;
			cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
			ASSERT(cc != NULL);

			/* 4314/43142 has pin muxing, don't clear gpio bits */
			if ((CHIPID(sih->chip) == BCM4314_CHIP_ID) ||
				(CHIPID(sih->chip) == BCM43142_CHIP_ID)) {
				gpiopullup |= 0x402e0;
				gpiopulldown |= 0x20500;
			}

			W_REG(osh, &cc->gpiopullup, gpiopullup);
			W_REG(osh, &cc->gpiopulldown, gpiopulldown);
			si_setcoreidx(sih, origidx);
		}
#endif /* !BCM_BOOTLOADER */

		/* PMU specific initializations */
		if (PMUCTL_ENAB(sih)) {
			uint32 xtalfreq, mode;
			si_pmu_init(sih, sii->osh);
			si_pmu_chip_init(sih, sii->osh);
			xtalfreq = getintvar(pvars, "xtalfreq");
			switch (CHIPID(sih->chip)) {
				case BCM43242_CHIP_ID:
				case BCM43243_CHIP_ID:
					xtalfreq = 37400;
					break;
				case BCM43143_CHIP_ID:
					xtalfreq = 20000;
					break;

				case BCM4350_CHIP_ID:
					if (xtalfreq == 0) {
						mode = CST4350_IFC_MODE(sih->chipst);
						if ((mode == CST4350_IFC_MODE_USB20D) ||
							(mode == CST4350_IFC_MODE_USB30D) ||
							(mode == CST4350_IFC_MODE_USB30D_WL))
							xtalfreq = 40000;
						else
							xtalfreq = 37400;
					}
					break;
				default:
					break;
			}
			/* If xtalfreq var not available, try to measure it */
			if (xtalfreq == 0)
				xtalfreq = si_pmu_measure_alpclk(sih, sii->osh);
#if !defined(BCMHIGHSDIO)
			si_pmu_pll_init(sih, sii->osh, xtalfreq);
			si_pmu_res_init(sih, sii->osh);
#endif
			si_pmu_swreg_init(sih, sii->osh);
		}

	/* setup the GPIO based LED powersave register */
	if (sii->pub.ccrev >= 16) {
		if ((w = getintvar(pvars, "leddc")) == 0)
			w = DEFAULT_GPIOTIMERVAL;
		si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gpiotimerval), ~0, w);
	}

	if (PCI_FORCEHT(sii)) {
		SI_MSG(("si_doattach: force HT\n"));
		sih->pci_pr32414 = TRUE;
		si_clkctl_init(sih);
		_si_clkctl_cc(sii, CLK_FAST);
	}

#if (!defined(_CFE_) && !defined(_CFEZ_)) || defined(CFG_WL)
	if (PCIE(sii)) {
		ASSERT(sii->pch != NULL);

		pcicore_attach(sii->pch, pvars, SI_DOATTACH);

		if (((CHIPID(sih->chip) == BCM4311_CHIP_ID) && (CHIPREV(sih->chiprev) == 2)) ||
		    (CHIPID(sih->chip) == BCM4312_CHIP_ID)) {
			SI_MSG(("si_doattach: clear initiator timeout\n"));
			sb_set_initiator_to(sih, 0x3, si_findcoreidx(sih, D11_CORE_ID, 0));
		}
	}

	if ((CHIPID(sih->chip) == BCM43224_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43421_CHIP_ID)) {
		/* enable 12 mA drive strenth for 43224 and set chipControl register bit 15 */
		if (CHIPREV(sih->chiprev) == 0) {
			SI_MSG(("Applying 43224A0 WARs\n"));
			si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol),
			           CCTRL43224_GPIO_TOGGLE, CCTRL43224_GPIO_TOGGLE);
			si_pmu_chipcontrol(sih, 0, CCTRL_43224A0_12MA_LED_DRIVE,
			                   CCTRL_43224A0_12MA_LED_DRIVE);
		}
		if (CHIPREV(sih->chiprev) >= 1) {
			SI_MSG(("Applying 43224B0+ WARs\n"));
			si_pmu_chipcontrol(sih, 0, CCTRL_43224B0_12MA_LED_DRIVE,
			                   CCTRL_43224B0_12MA_LED_DRIVE);
		}
	}

	/* configure default pinmux enables for the chip */
	if (getvar(pvars, "muxenab") != NULL) {
		w = getintvar(pvars, "muxenab");
		si_muxenab((si_t *)sii, w);
	}

	/* enable GPIO interrupts when clocks are off */
	if (sii->pub.ccrev >= 21) {
		uint32 corecontrol;
		corecontrol = si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, corecontrol),
		                         0, 0);
		corecontrol |= CC_ASYNCGPIO;
		si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, corecontrol),
		           corecontrol, corecontrol);
	}

	if (CHIPID(sih->chip) == BCM4313_CHIP_ID) {
		/* enable 12 mA drive strenth for 4313 and set chipControl register bit 1 */
		SI_MSG(("Applying 4313 WARs\n"));
		si_pmu_chipcontrol(sih, 0, CCTRL_4313_12MA_LED_DRIVE,	CCTRL_4313_12MA_LED_DRIVE);
	}
#endif /* (!_CFE_ && !_CFEZ_) || CFG_WL */

#if defined(SAVERESTORE)
	sr_save_restore_init(sih);
#endif
	/* clear any previous epidiag-induced target abort */
	ASSERT(!si_taclear(sih, FALSE));

	if ((CHIPID(sih->chip) == BCM4335_CHIP_ID) ||
	0) {
		si_gci_chipctrl_overrides(osh, sih, pvars);
	}

	return (sii);

exit:
	if (BUSTYPE(sih->bustype) == PCI_BUS) {
		if (sii->pch)
			pcicore_deinit(sii->pch);
		sii->pch = NULL;
	}

	return NULL;
}

/* may be called with core in reset */
void
BCMATTACHFN(si_detach)(si_t *sih)
{
	si_info_t *sii;
	uint idx;

#if defined(STA)
	struct si_pub *si_local = NULL;
	bcopy(&sih, &si_local, sizeof(si_t*));
#endif 

	sii = SI_INFO(sih);

	if (sii == NULL)
		return;

	if (BUSTYPE(sih->bustype) == SI_BUS)
		for (idx = 0; idx < SI_MAXCORES; idx++)
			if (sii->regs[idx]) {
				REG_UNMAP(sii->regs[idx]);
				sii->regs[idx] = NULL;
			}

#if defined(STA)
#if !defined(BCMHIGHSDIO)
	srom_var_deinit((void *)si_local);
#endif
	nvram_exit((void *)si_local); /* free up nvram buffers */
#endif 

	if (BUSTYPE(sih->bustype) == PCI_BUS) {
		if (sii->pch)
			pcicore_deinit(sii->pch);
		sii->pch = NULL;
	}

#if !defined(BCMBUSTYPE) || (BCMBUSTYPE == SI_BUS)
	if (sii != &ksii)
#endif	/* !BCMBUSTYPE || (BCMBUSTYPE == SI_BUS) */
		MFREE(sii->osh, sii, sizeof(si_info_t));
}

void *
si_osh(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);
	return sii->osh;
}

void
si_setosh(si_t *sih, osl_t *osh)
{
	si_info_t *sii;

	sii = SI_INFO(sih);
	if (sii->osh != NULL) {
		SI_ERROR(("osh is already set....\n"));
		ASSERT(!sii->osh);
	}
	sii->osh = osh;
}

/* register driver interrupt disabling and restoring callback functions */
void
si_register_intr_callback(si_t *sih, void *intrsoff_fn, void *intrsrestore_fn,
                          void *intrsenabled_fn, void *intr_arg)
{
	si_info_t *sii;

	sii = SI_INFO(sih);
	sii->intr_arg = intr_arg;
	sii->intrsoff_fn = (si_intrsoff_t)intrsoff_fn;
	sii->intrsrestore_fn = (si_intrsrestore_t)intrsrestore_fn;
	sii->intrsenabled_fn = (si_intrsenabled_t)intrsenabled_fn;
	/* save current core id.  when this function called, the current core
	 * must be the core which provides driver functions(il, et, wl, etc.)
	 */
	sii->dev_coreid = sii->coreid[sii->curidx];
}

void
si_deregister_intr_callback(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);
	sii->intrsoff_fn = NULL;
}

uint
si_intflag(si_t *sih)
{
	si_info_t *sii = SI_INFO(sih);

	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_intflag(sih);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		return R_REG(sii->osh, ((uint32 *)(uintptr)
			    (sii->oob_router + OOB_STATUSA)));
	else {
		ASSERT(0);
		return 0;
	}
}

uint
si_flag(si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_flag(sih);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_flag(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_flag(sih);
	else {
		ASSERT(0);
		return 0;
	}
}

void
si_setint(si_t *sih, int siflag)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		sb_setint(sih, siflag);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		ai_setint(sih, siflag);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		ub_setint(sih, siflag);
	else
		ASSERT(0);
}

uint
si_coreid(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);
	return sii->coreid[sii->curidx];
}

uint
si_coreidx(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);
	return sii->curidx;
}

/* return the core-type instantiation # of the current core */
uint
si_coreunit(si_t *sih)
{
	si_info_t *sii;
	uint idx;
	uint coreid;
	uint coreunit;
	uint i;

	sii = SI_INFO(sih);
	coreunit = 0;

	idx = sii->curidx;

	ASSERT(GOODREGS(sii->curmap));
	coreid = si_coreid(sih);

	/* count the cores of our type */
	for (i = 0; i < idx; i++)
		if (sii->coreid[i] == coreid)
			coreunit++;

	return (coreunit);
}

uint
si_corevendor(si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_corevendor(sih);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_corevendor(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_corevendor(sih);
	else {
		ASSERT(0);
		return 0;
	}
}

bool
si_backplane64(si_t *sih)
{
	return ((sih->cccaps & CC_CAP_BKPLN64) != 0);
}

uint
si_corerev(si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_corerev(sih);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_corerev(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_corerev(sih);
	else {
		ASSERT(0);
		return 0;
	}
}

/* return index of coreid or BADIDX if not found */
uint
si_findcoreidx(si_t *sih, uint coreid, uint coreunit)
{
	si_info_t *sii;
	uint found;
	uint i;

	sii = SI_INFO(sih);

	found = 0;

	for (i = 0; i < sii->numcores; i++)
		if (sii->coreid[i] == coreid) {
			if (found == coreunit)
				return (i);
			found++;
		}

	return (BADIDX);
}

/* return list of found cores */
uint
si_corelist(si_t *sih, uint coreid[])
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	bcopy((uchar*)sii->coreid, (uchar*)coreid, (sii->numcores * sizeof(uint)));
	return (sii->numcores);
}

/* return current wrapper mapping */
void *
si_wrapperregs(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);
	ASSERT(GOODREGS(sii->curwrap));

	return (sii->curwrap);
}

/* return current register mapping */
void *
si_coreregs(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);
	ASSERT(GOODREGS(sii->curmap));

	return (sii->curmap);
}

/*
 * This function changes logical "focus" to the indicated core;
 * must be called with interrupts off.
 * Moreover, callers should keep interrupts off during switching out of and back to d11 core
 */
void *
si_setcore(si_t *sih, uint coreid, uint coreunit)
{
	uint idx;

	idx = si_findcoreidx(sih, coreid, coreunit);
	if (!GOODIDX(idx))
		return (NULL);

	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_setcoreidx(sih, idx);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_setcoreidx(sih, idx);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_setcoreidx(sih, idx);
	else {
		ASSERT(0);
		return NULL;
	}
}

void *
si_setcoreidx(si_t *sih, uint coreidx)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_setcoreidx(sih, coreidx);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_setcoreidx(sih, coreidx);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_setcoreidx(sih, coreidx);
	else {
		ASSERT(0);
		return NULL;
	}
}

/* Turn off interrupt as required by sb_setcore, before switch core */
void *
si_switch_core(si_t *sih, uint coreid, uint *origidx, uint *intr_val)
{
	void *cc;
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (SI_FAST(sii)) {
		/* Overloading the origidx variable to remember the coreid,
		 * this works because the core ids cannot be confused with
		 * core indices.
		 */
		*origidx = coreid;
		if (coreid == CC_CORE_ID)
			return (void *)CCREGS_FAST(sii);
		else if (coreid == sih->buscoretype)
			return (void *)PCIEREGS(sii);
	}
	INTR_OFF(sii, *intr_val);
	*origidx = sii->curidx;
	cc = si_setcore(sih, coreid, 0);
	ASSERT(cc != NULL);

	return cc;
}

/* restore coreidx and restore interrupt */
void
si_restore_core(si_t *sih, uint coreid, uint intr_val)
{
	si_info_t *sii;

	sii = SI_INFO(sih);
	if (SI_FAST(sii) && ((coreid == CC_CORE_ID) || (coreid == sih->buscoretype)))
		return;

	si_setcoreidx(sih, coreid);
	INTR_RESTORE(sii, intr_val);
}

int
si_numaddrspaces(si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_numaddrspaces(sih);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_numaddrspaces(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_numaddrspaces(sih);
	else {
		ASSERT(0);
		return 0;
	}
}

uint32
si_addrspace(si_t *sih, uint asidx)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_addrspace(sih, asidx);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_addrspace(sih, asidx);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_addrspace(sih, asidx);
	else {
		ASSERT(0);
		return 0;
	}
}

uint32
si_addrspacesize(si_t *sih, uint asidx)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_addrspacesize(sih, asidx);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_addrspacesize(sih, asidx);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_addrspacesize(sih, asidx);
	else {
		ASSERT(0);
		return 0;
	}
}

void
si_coreaddrspaceX(si_t *sih, uint asidx, uint32 *addr, uint32 *size)
{
	/* Only supported for SOCI_AI */
	if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		ai_coreaddrspaceX(sih, asidx, addr, size);
	else
		*size = 0;
}

uint32
si_core_cflags(si_t *sih, uint32 mask, uint32 val)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_core_cflags(sih, mask, val);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_core_cflags(sih, mask, val);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_core_cflags(sih, mask, val);
	else {
		ASSERT(0);
		return 0;
	}
}

void
si_core_cflags_wo(si_t *sih, uint32 mask, uint32 val)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		sb_core_cflags_wo(sih, mask, val);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		ai_core_cflags_wo(sih, mask, val);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		ub_core_cflags_wo(sih, mask, val);
	else
		ASSERT(0);
}

uint32
si_core_sflags(si_t *sih, uint32 mask, uint32 val)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_core_sflags(sih, mask, val);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_core_sflags(sih, mask, val);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_core_sflags(sih, mask, val);
	else {
		ASSERT(0);
		return 0;
	}
}

bool
si_iscoreup(si_t *sih)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_iscoreup(sih);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_iscoreup(sih);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_iscoreup(sih);
	else {
		ASSERT(0);
		return FALSE;
	}
}

uint
si_wrapperreg(si_t *sih, uint32 offset, uint32 mask, uint32 val)
{
	/* only for AI back plane chips */
	if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		return (ai_wrap_reg(sih, offset, mask, val));
	return 0;
}

uint
si_corereg(si_t *sih, uint coreidx, uint regoff, uint mask, uint val)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_corereg(sih, coreidx, regoff, mask, val);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		return ai_corereg(sih, coreidx, regoff, mask, val);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return ub_corereg(sih, coreidx, regoff, mask, val);
	else {
		ASSERT(0);
		return 0;
	}
}

void
si_core_disable(si_t *sih, uint32 bits)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		sb_core_disable(sih, bits);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		ai_core_disable(sih, bits);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		ub_core_disable(sih, bits);
}

void
si_core_reset(si_t *sih, uint32 bits, uint32 resetbits)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		sb_core_reset(sih, bits, resetbits);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		ai_core_reset(sih, bits, resetbits);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		ub_core_reset(sih, bits, resetbits);
}

/* Run bist on current core. Caller needs to take care of core-specific bist hazards */
int
si_corebist(si_t *sih)
{
	uint32 cflags;
	int result = 0;

	/* Read core control flags */
	cflags = si_core_cflags(sih, 0, 0);

	/* Set bist & fgc */
	si_core_cflags(sih, ~0, (SICF_BIST_EN | SICF_FGC));

	/* Wait for bist done */
	SPINWAIT(((si_core_sflags(sih, 0, 0) & SISF_BIST_DONE) == 0), 100000);

	if (si_core_sflags(sih, 0, 0) & SISF_BIST_ERROR)
		result = BCME_ERROR;

	/* Reset core control flags */
	si_core_cflags(sih, 0xffff, cflags);

	return result;
}

static uint32
BCMINITFN(factor6)(uint32 x)
{
	switch (x) {
	case CC_F6_2:	return 2;
	case CC_F6_3:	return 3;
	case CC_F6_4:	return 4;
	case CC_F6_5:	return 5;
	case CC_F6_6:	return 6;
	case CC_F6_7:	return 7;
	default:	return 0;
	}
}

/* calculate the speed the SI would run at given a set of clockcontrol values */
uint32
BCMINITFN(si_clock_rate)(uint32 pll_type, uint32 n, uint32 m)
{
	uint32 n1, n2, clock, m1, m2, m3, mc;

	n1 = n & CN_N1_MASK;
	n2 = (n & CN_N2_MASK) >> CN_N2_SHIFT;

	if (pll_type == PLL_TYPE6) {
		if (m & CC_T6_MMASK)
			return CC_T6_M1;
		else
			return CC_T6_M0;
	} else if ((pll_type == PLL_TYPE1) ||
	           (pll_type == PLL_TYPE3) ||
	           (pll_type == PLL_TYPE4) ||
	           (pll_type == PLL_TYPE7)) {
		n1 = factor6(n1);
		n2 += CC_F5_BIAS;
	} else if (pll_type == PLL_TYPE2) {
		n1 += CC_T2_BIAS;
		n2 += CC_T2_BIAS;
		ASSERT((n1 >= 2) && (n1 <= 7));
		ASSERT((n2 >= 5) && (n2 <= 23));
	} else if (pll_type == PLL_TYPE5) {
		return (100000000);
	} else
		ASSERT(0);
	/* PLL types 3 and 7 use BASE2 (25Mhz) */
	if ((pll_type == PLL_TYPE3) ||
	    (pll_type == PLL_TYPE7)) {
		clock = CC_CLOCK_BASE2 * n1 * n2;
	} else
		clock = CC_CLOCK_BASE1 * n1 * n2;

	if (clock == 0)
		return 0;

	m1 = m & CC_M1_MASK;
	m2 = (m & CC_M2_MASK) >> CC_M2_SHIFT;
	m3 = (m & CC_M3_MASK) >> CC_M3_SHIFT;
	mc = (m & CC_MC_MASK) >> CC_MC_SHIFT;

	if ((pll_type == PLL_TYPE1) ||
	    (pll_type == PLL_TYPE3) ||
	    (pll_type == PLL_TYPE4) ||
	    (pll_type == PLL_TYPE7)) {
		m1 = factor6(m1);
		if ((pll_type == PLL_TYPE1) || (pll_type == PLL_TYPE3))
			m2 += CC_F5_BIAS;
		else
			m2 = factor6(m2);
		m3 = factor6(m3);

		switch (mc) {
		case CC_MC_BYPASS:	return (clock);
		case CC_MC_M1:		return (clock / m1);
		case CC_MC_M1M2:	return (clock / (m1 * m2));
		case CC_MC_M1M2M3:	return (clock / (m1 * m2 * m3));
		case CC_MC_M1M3:	return (clock / (m1 * m3));
		default:		return (0);
		}
	} else {
		ASSERT(pll_type == PLL_TYPE2);

		m1 += CC_T2_BIAS;
		m2 += CC_T2M2_BIAS;
		m3 += CC_T2_BIAS;
		ASSERT((m1 >= 2) && (m1 <= 7));
		ASSERT((m2 >= 3) && (m2 <= 10));
		ASSERT((m3 >= 2) && (m3 <= 7));

		if ((mc & CC_T2MC_M1BYP) == 0)
			clock /= m1;
		if ((mc & CC_T2MC_M2BYP) == 0)
			clock /= m2;
		if ((mc & CC_T2MC_M3BYP) == 0)
			clock /= m3;

		return (clock);
	}
}

/* Some chips could have multiple host interfaces, however only one will be active.
 * For a given chip. Depending pkgopt and cc_chipst return the active host interface.
 */
uint
si_chip_hostif(si_t *sih)
{
	uint hosti;

	switch (CHIPID(sih->chip)) {
	case BCM4360_CHIP_ID:
	case BCM43526_CHIP_ID:
		/* chippkg bit-0 == 0 is PCIE only pkgs
		 * chippkg bit-0 == 1 has both PCIE and USB cores enabled
		 */
		if ((sih->chippkg & 0x1) && (sih->chipst & CST4360_MODE_USB))
			hosti = CHIP_HOSTIF_USBMODE;
		else
			hosti = CHIP_HOSTIF_PCIEMODE;

		break;

	case BCM4335_CHIP_ID:
		/* TBD: like in 4360, do we need to check pkg? */
		if (CST4335_CHIPMODE_USB20D(sih->chipst))
			hosti = CHIP_HOSTIF_USBMODE;
		else if (CST4335_CHIPMODE_SDIOD(sih->chipst))
			hosti = CHIP_HOSTIF_SDIOMODE;
		else
			hosti = CHIP_HOSTIF_PCIEMODE;
		break;

	default:
		hosti = 0;
		break;
	}

	return hosti;
}

bool si_read_pmu_autopll(si_t *sih)
{
	si_info_t *sii;
	sii = SI_INFO(sih);
	return (si_pmu_is_autoresetphyclk_disabled(sih, sii->osh));
}

uint32
BCMINITFN(si_clock)(si_t *sih)
{
	si_info_t *sii;
	chipcregs_t *cc;
	uint32 n, m;
	uint idx;
	uint32 pll_type, rate;
	uint intr_val = 0;

	if (BCM4707_CHIP(CHIPID(sih->chip))) {
		if (sih->chippkg == BCM4709_PKG_ID) {
			return NS_SI_CLOCK;
		} else
			return NS_SLOW_SI_CLOCK;
	}

	sii = SI_INFO(sih);
	INTR_OFF(sii, intr_val);
	if (PMUCTL_ENAB(sih)) {
		rate = si_pmu_si_clock(sih, sii->osh);
		goto exit;
	}

	idx = sii->curidx;
	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
	ASSERT(cc != NULL);

	n = R_REG(sii->osh, &cc->clockcontrol_n);
	pll_type = sih->cccaps & CC_CAP_PLL_MASK;
	if (pll_type == PLL_TYPE6)
		m = R_REG(sii->osh, &cc->clockcontrol_m3);
	else if (pll_type == PLL_TYPE3)
		m = R_REG(sii->osh, &cc->clockcontrol_m2);
	else
		m = R_REG(sii->osh, &cc->clockcontrol_sb);

	/* calculate rate */
	rate = si_clock_rate(pll_type, n, m);

	if (pll_type == PLL_TYPE3)
		rate = rate / 2;

	/* switch back to previous core */
	si_setcoreidx(sih, idx);
exit:
	INTR_RESTORE(sii, intr_val);

	return rate;
}

uint32
BCMINITFN(si_alp_clock)(si_t *sih)
{
	if (PMUCTL_ENAB(sih))
		return si_pmu_alp_clock(sih, si_osh(sih));
	else if (BCM4707_CHIP(CHIPID(sih->chip))) {
		if (sih->chippkg == BCM4709_PKG_ID)
			return NS_ALP_CLOCK;
		else
			return NS_SLOW_ALP_CLOCK;
	}

	return ALP_CLOCK;
}

uint32
BCMINITFN(si_ilp_clock)(si_t *sih)
{
	if (PMUCTL_ENAB(sih))
		return si_pmu_ilp_clock(sih, si_osh(sih));

	return ILP_CLOCK;
}

/* set chip watchdog reset timer to fire in 'ticks' */
void
si_watchdog(si_t *sih, uint ticks)
{
	uint nb, maxt;

	if (PMUCTL_ENAB(sih)) {

#if (!defined(_CFE_) && !defined(_CFEZ_)) || defined(CFG_WL)
		if ((CHIPID(sih->chip) == BCM4319_CHIP_ID) &&
		    (CHIPREV(sih->chiprev) == 0) && (ticks != 0)) {
			si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, clk_ctl_st), ~0, 0x2);
			si_setcore(sih, USB20D_CORE_ID, 0);
			si_core_disable(sih, 1);
			si_setcore(sih, CC_CORE_ID, 0);
		}
#endif /* (!_CFE_ && !_CFEZ_) || CFG_WL */

		if (CHIPID(sih->chip) == BCM4706_CHIP_ID)
			nb = 32;
		else
			nb = (sih->ccrev < 26) ? 16 : ((sih->ccrev >= 37) ? 32 : 24);
		/* The mips compiler uses the sllv instruction,
		 * so we specially handle the 32-bit case.
		 */
		if (nb == 32)
			maxt = 0xffffffff;
		else
			maxt = ((1 << nb) - 1);

		if (ticks == 1)
			ticks = 2;
		else if (ticks > maxt)
			ticks = maxt;

		si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, pmuwatchdog), ~0, ticks);
	} else {
		if (!BCM4707_CHIP(CHIPID(sih->chip))) {
			/* make sure we come up in fast clock mode; or if clearing, clear clock */
			si_clkctl_cc(sih, ticks ? CLK_FAST : CLK_DYNAMIC);
		}
		maxt = (1 << 28) - 1;
		if (ticks > maxt)
			ticks = maxt;

		si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, watchdog), ~0, ticks);
	}
}

/* trigger watchdog reset after ms milliseconds */
void
si_watchdog_ms(si_t *sih, uint32 ms)
{
	si_watchdog(sih, wd_msticks * ms);
}

uint32
si_watchdog_msticks(void)
{
	return wd_msticks;
}

bool
si_taclear(si_t *sih, bool details)
{
#if defined(BCMDBG_ERR) || defined(BCMASSERT_SUPPORT)
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		return sb_taclear(sih, details);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		return FALSE;
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		return FALSE;
	else {
		ASSERT(0);
		return FALSE;
	}
#else
	return FALSE;
#endif 
}

uint16
BCMATTACHFN(si_d11_devid)(si_t *sih)
{
	si_info_t *sii = SI_INFO(sih);
	uint16 device;

	/* Fix device id for dual band BCM4328 */
	if (CHIPID(sih->chip) == BCM4328_CHIP_ID &&
	    (sih->chippkg == BCM4328USBDUAL_PKG_ID || sih->chippkg == BCM4328SDIODUAL_PKG_ID))
		device = BCM4328_D11DUAL_ID;
	else {
		/* normal case: nvram variable with devpath->devid->wl0id */
		if ((device = (uint16)si_getdevpathintvar(sih, "devid")) != 0)
			;
		/* Get devid from OTP/SPROM depending on where the SROM is read */
		else if ((device = (uint16)getintvar(sii->vars, "devid")) != 0)
			;
		/* no longer support wl0id, but keep the code here for backward compatibility. */
		else if ((device = (uint16)getintvar(sii->vars, "wl0id")) != 0)
			;
		else if (CHIPID(sih->chip) == BCM4712_CHIP_ID) {
			/* Chip specific conversion */
			if (sih->chippkg == BCM4712SMALL_PKG_ID)
				device = BCM4306_D11G_ID;
			else
				device = BCM4306_D11DUAL_ID;
		} else {
			/* ignore it */
			device = 0xffff;
		}
	}
	return device;
}

int
BCMATTACHFN(si_corepciid)(si_t *sih, uint func, uint16 *pcivendor, uint16 *pcidevice,
                          uint8 *pciclass, uint8 *pcisubclass, uint8 *pciprogif,
                          uint8 *pciheader)
{
	uint16 vendor = 0xffff, device = 0xffff;
	uint8 class, subclass, progif = 0;
	uint8 header = PCI_HEADER_NORMAL;
	uint32 core = si_coreid(sih);

	/* Verify whether the function exists for the core */
	if (func >= (uint)((core == USB20H_CORE_ID) || (core == NS_USB20_CORE_ID) ? 2 : 1))
		return BCME_ERROR;

	/* Known vendor translations */
	switch (si_corevendor(sih)) {
	case SB_VEND_BCM:
	case MFGID_BRCM:
		vendor = VENDOR_BROADCOM;
		break;
	default:
		return BCME_ERROR;
	}

	/* Determine class based on known core codes */
	switch (core) {
	case ENET_CORE_ID:
		class = PCI_CLASS_NET;
		subclass = PCI_NET_ETHER;
		device = BCM47XX_ENET_ID;
		break;
	case GIGETH_CORE_ID:
		class = PCI_CLASS_NET;
		subclass = PCI_NET_ETHER;
		device = BCM47XX_GIGETH_ID;
		break;
	case GMAC_CORE_ID:
		class = PCI_CLASS_NET;
		subclass = PCI_NET_ETHER;
		device = BCM47XX_GMAC_ID;
		break;
	case SDRAM_CORE_ID:
	case MEMC_CORE_ID:
	case DMEMC_CORE_ID:
	case SOCRAM_CORE_ID:
		class = PCI_CLASS_MEMORY;
		subclass = PCI_MEMORY_RAM;
		device = (uint16)core;
		break;
	case PCI_CORE_ID:
	case PCIE_CORE_ID:
	case PCIE2_CORE_ID:
		class = PCI_CLASS_BRIDGE;
		subclass = PCI_BRIDGE_PCI;
		device = (uint16)core;
		header = PCI_HEADER_BRIDGE;
		break;
	case MIPS33_CORE_ID:
	case MIPS74K_CORE_ID:
		class = PCI_CLASS_CPU;
		subclass = PCI_CPU_MIPS;
		device = (uint16)core;
		break;
	case CODEC_CORE_ID:
		class = PCI_CLASS_COMM;
		subclass = PCI_COMM_MODEM;
		device = BCM47XX_V90_ID;
		break;
	case I2S_CORE_ID:
		class = PCI_CLASS_MMEDIA;
		subclass = PCI_MMEDIA_AUDIO;
		device = BCM47XX_AUDIO_ID;
		break;
	case USB_CORE_ID:
	case USB11H_CORE_ID:
		class = PCI_CLASS_SERIAL;
		subclass = PCI_SERIAL_USB;
		progif = 0x10; /* OHCI */
		device = BCM47XX_USBH_ID;
		break;
	case USB20H_CORE_ID:
	case NS_USB20_CORE_ID:
		class = PCI_CLASS_SERIAL;
		subclass = PCI_SERIAL_USB;
		progif = func == 0 ? 0x10 : 0x20; /* OHCI/EHCI value defined in spec */
		device = BCM47XX_USB20H_ID;
		header = PCI_HEADER_MULTI; /* multifunction */
		break;
	case IPSEC_CORE_ID:
		class = PCI_CLASS_CRYPT;
		subclass = PCI_CRYPT_NETWORK;
		device = BCM47XX_IPSEC_ID;
		break;
	case NS_USB30_CORE_ID:
		class = PCI_CLASS_SERIAL;
		subclass = PCI_SERIAL_USB;
		progif = 0x30; /* XHCI */
		device = BCM47XX_USB30H_ID;
		break;
	case ROBO_CORE_ID:
		/* Don't use class NETWORK, so wl/et won't attempt to recognize it */
		class = PCI_CLASS_COMM;
		subclass = PCI_COMM_OTHER;
		device = BCM47XX_ROBO_ID;
		break;
	case CC_CORE_ID:
		class = PCI_CLASS_MEMORY;
		subclass = PCI_MEMORY_FLASH;
		device = (uint16)core;
		break;
	case SATAXOR_CORE_ID:
		class = PCI_CLASS_XOR;
		subclass = PCI_XOR_QDMA;
		device = BCM47XX_SATAXOR_ID;
		break;
	case ATA100_CORE_ID:
		class = PCI_CLASS_DASDI;
		subclass = PCI_DASDI_IDE;
		device = BCM47XX_ATA100_ID;
		break;
	case USB11D_CORE_ID:
		class = PCI_CLASS_SERIAL;
		subclass = PCI_SERIAL_USB;
		device = BCM47XX_USBD_ID;
		break;
	case USB20D_CORE_ID:
		class = PCI_CLASS_SERIAL;
		subclass = PCI_SERIAL_USB;
		device = BCM47XX_USB20D_ID;
		break;
	case D11_CORE_ID:
		class = PCI_CLASS_NET;
		subclass = PCI_NET_OTHER;
		device = si_d11_devid(sih);
		break;

	default:
		class = subclass = progif = 0xff;
		device = (uint16)core;
		break;
	}

	*pcivendor = vendor;
	*pcidevice = device;
	*pciclass = class;
	*pcisubclass = subclass;
	*pciprogif = progif;
	*pciheader = header;

	return 0;
}

#if defined(BCMDBG)
/* print interesting sbconfig registers */
void
si_dumpregs(si_t *sih, struct bcmstrbuf *b)
{
	si_info_t *sii;
	uint origidx, intr_val = 0;

	sii = SI_INFO(sih);
	origidx = sii->curidx;

	INTR_OFF(sii, intr_val);
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		sb_dumpregs(sih, b);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		ai_dumpregs(sih, b);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		ub_dumpregs(sih, b);
	else
		ASSERT(0);

	si_setcoreidx(sih, origidx);
	INTR_RESTORE(sii, intr_val);
}
#endif	

#ifdef BCMDBG
void
si_view(si_t *sih, bool verbose)
{
	if (CHIPTYPE(sih->socitype) == SOCI_SB)
		sb_view(sih, verbose);
	else if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		ai_view(sih, verbose);
	else if (CHIPTYPE(sih->socitype) == SOCI_UBUS)
		ub_view(sih, verbose);
	else
		ASSERT(0);
}

void
si_viewall(si_t *sih, bool verbose)
{
	si_info_t *sii;
	uint curidx, i;
	uint intr_val = 0;

	sii = SI_INFO(sih);
	curidx = sii->curidx;

	INTR_OFF(sii, intr_val);
	if ((CHIPTYPE(sih->socitype) == SOCI_AI) || (CHIPTYPE(sih->socitype) == SOCI_NAI))
		ai_viewall(sih, verbose);
	else {
		SI_ERROR(("si_viewall: num_cores %d\n", sii->numcores));
		for (i = 0; i < sii->numcores; i++) {
			si_setcoreidx(sih, i);
			si_view(sih, verbose);
		}
	}
	si_setcoreidx(sih, curidx);
	INTR_RESTORE(sii, intr_val);
}
#endif	/* BCMDBG */

/* return the slow clock source - LPO, XTAL, or PCI */
static uint
si_slowclk_src(si_info_t *sii)
{
	chipcregs_t *cc;

	ASSERT(SI_FAST(sii) || si_coreid(&sii->pub) == CC_CORE_ID);

	if (sii->pub.ccrev < 6) {
		if ((BUSTYPE(sii->pub.bustype) == PCI_BUS) &&
		    (OSL_PCI_READ_CONFIG(sii->osh, PCI_GPIO_OUT, sizeof(uint32)) &
		     PCI_CFG_GPIO_SCS))
			return (SCC_SS_PCI);
		else
			return (SCC_SS_XTAL);
	} else if (sii->pub.ccrev < 10) {
		cc = (chipcregs_t *)si_setcoreidx(&sii->pub, sii->curidx);
		return (R_REG(sii->osh, &cc->slow_clk_ctl) & SCC_SS_MASK);
	} else	/* Insta-clock */
		return (SCC_SS_XTAL);
}

/* return the ILP (slowclock) min or max frequency */
static uint
si_slowclk_freq(si_info_t *sii, bool max_freq, chipcregs_t *cc)
{
	uint32 slowclk;
	uint div;

	ASSERT(SI_FAST(sii) || si_coreid(&sii->pub) == CC_CORE_ID);

	/* shouldn't be here unless we've established the chip has dynamic clk control */
	ASSERT(R_REG(sii->osh, &cc->capabilities) & CC_CAP_PWR_CTL);

	slowclk = si_slowclk_src(sii);
	if (sii->pub.ccrev < 6) {
		if (slowclk == SCC_SS_PCI)
			return (max_freq ? (PCIMAXFREQ / 64) : (PCIMINFREQ / 64));
		else
			return (max_freq ? (XTALMAXFREQ / 32) : (XTALMINFREQ / 32));
	} else if (sii->pub.ccrev < 10) {
		div = 4 *
		        (((R_REG(sii->osh, &cc->slow_clk_ctl) & SCC_CD_MASK) >> SCC_CD_SHIFT) + 1);
		if (slowclk == SCC_SS_LPO)
			return (max_freq ? LPOMAXFREQ : LPOMINFREQ);
		else if (slowclk == SCC_SS_XTAL)
			return (max_freq ? (XTALMAXFREQ / div) : (XTALMINFREQ / div));
		else if (slowclk == SCC_SS_PCI)
			return (max_freq ? (PCIMAXFREQ / div) : (PCIMINFREQ / div));
		else
			ASSERT(0);
	} else {
		/* Chipc rev 10 is InstaClock */
		div = R_REG(sii->osh, &cc->system_clk_ctl) >> SYCC_CD_SHIFT;
		div = 4 * (div + 1);
		return (max_freq ? XTALMAXFREQ : (XTALMINFREQ / div));
	}
	return (0);
}

static void
BCMINITFN(si_clkctl_setdelay)(si_info_t *sii, void *chipcregs)
{
	chipcregs_t *cc = (chipcregs_t *)chipcregs;
	uint slowmaxfreq, pll_delay, slowclk;
	uint pll_on_delay, fref_sel_delay;

	pll_delay = PLL_DELAY;

	/* If the slow clock is not sourced by the xtal then add the xtal_on_delay
	 * since the xtal will also be powered down by dynamic clk control logic.
	 */

	slowclk = si_slowclk_src(sii);
	if (slowclk != SCC_SS_XTAL)
		pll_delay += XTAL_ON_DELAY;

	/* Starting with 4318 it is ILP that is used for the delays */
	slowmaxfreq = si_slowclk_freq(sii, (sii->pub.ccrev >= 10) ? FALSE : TRUE, cc);

	pll_on_delay = ((slowmaxfreq * pll_delay) + 999999) / 1000000;
	fref_sel_delay = ((slowmaxfreq * FREF_DELAY) + 999999) / 1000000;

	W_REG(sii->osh, &cc->pll_on_delay, pll_on_delay);
	W_REG(sii->osh, &cc->fref_sel_delay, fref_sel_delay);
}

/* initialize power control delay registers */
void
BCMINITFN(si_clkctl_init)(si_t *sih)
{
	si_info_t *sii;
	uint origidx = 0;
	chipcregs_t *cc;
	bool fast;

	if (!CCCTL_ENAB(sih))
		return;

	sii = SI_INFO(sih);
	fast = SI_FAST(sii);
	if (!fast) {
		origidx = sii->curidx;
		if ((cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0)) == NULL)
			return;
	} else if ((cc = (chipcregs_t *)CCREGS_FAST(sii)) == NULL)
		return;
	ASSERT(cc != NULL);

	/* set all Instaclk chip ILP to 1 MHz */
	if (sih->ccrev >= 10)
		SET_REG(sii->osh, &cc->system_clk_ctl, SYCC_CD_MASK,
		        (ILP_DIV_1MHZ << SYCC_CD_SHIFT));

	si_clkctl_setdelay(sii, (void *)(uintptr)cc);

	OSL_DELAY(20000);

	if (!fast)
		si_setcoreidx(sih, origidx);
}

/* return the value suitable for writing to the dot11 core FAST_PWRUP_DELAY register */
uint16
BCMINITFN(si_clkctl_fast_pwrup_delay)(si_t *sih)
{
	si_info_t *sii;
	uint origidx = 0;
	chipcregs_t *cc;
	uint slowminfreq;
	uint16 fpdelay;
	uint intr_val = 0;
	bool fast;

	sii = SI_INFO(sih);
	if (PMUCTL_ENAB(sih)) {
		INTR_OFF(sii, intr_val);
		fpdelay = si_pmu_fast_pwrup_delay(sih, sii->osh);
		INTR_RESTORE(sii, intr_val);
		return fpdelay;
	}

	if (!CCCTL_ENAB(sih))
		return 0;

	fast = SI_FAST(sii);
	fpdelay = 0;
	if (!fast) {
		origidx = sii->curidx;
		INTR_OFF(sii, intr_val);
		if ((cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0)) == NULL)
			goto done;
	}
	else if ((cc = (chipcregs_t *)CCREGS_FAST(sii)) == NULL)
		goto done;
	ASSERT(cc != NULL);

	slowminfreq = si_slowclk_freq(sii, FALSE, cc);
	fpdelay = (((R_REG(sii->osh, &cc->pll_on_delay) + 2) * 1000000) +
	           (slowminfreq - 1)) / slowminfreq;

done:
	if (!fast) {
		si_setcoreidx(sih, origidx);
		INTR_RESTORE(sii, intr_val);
	}
	return fpdelay;
}

/* turn primary xtal and/or pll off/on */
int
si_clkctl_xtal(si_t *sih, uint what, bool on)
{
	si_info_t *sii;
	uint32 in, out, outen;

	sii = SI_INFO(sih);

	switch (BUSTYPE(sih->bustype)) {


	case PCMCIA_BUS:
		return (0);


	case PCI_BUS:
		/* pcie core doesn't have any mapping to control the xtal pu */
		if (PCIE(sii))
			return -1;

		in = OSL_PCI_READ_CONFIG(sii->osh, PCI_GPIO_IN, sizeof(uint32));
		out = OSL_PCI_READ_CONFIG(sii->osh, PCI_GPIO_OUT, sizeof(uint32));
		outen = OSL_PCI_READ_CONFIG(sii->osh, PCI_GPIO_OUTEN, sizeof(uint32));

		/*
		 * Avoid glitching the clock if GPRS is already using it.
		 * We can't actually read the state of the PLLPD so we infer it
		 * by the value of XTAL_PU which *is* readable via gpioin.
		 */
		if (on && (in & PCI_CFG_GPIO_XTAL))
			return (0);

		if (what & XTAL)
			outen |= PCI_CFG_GPIO_XTAL;
		if (what & PLL)
			outen |= PCI_CFG_GPIO_PLL;

		if (on) {
			/* turn primary xtal on */
			if (what & XTAL) {
				out |= PCI_CFG_GPIO_XTAL;
				if (what & PLL)
					out |= PCI_CFG_GPIO_PLL;
				OSL_PCI_WRITE_CONFIG(sii->osh, PCI_GPIO_OUT,
				                     sizeof(uint32), out);
				OSL_PCI_WRITE_CONFIG(sii->osh, PCI_GPIO_OUTEN,
				                     sizeof(uint32), outen);
				OSL_DELAY(XTAL_ON_DELAY);
			}

			/* turn pll on */
			if (what & PLL) {
				out &= ~PCI_CFG_GPIO_PLL;
				OSL_PCI_WRITE_CONFIG(sii->osh, PCI_GPIO_OUT,
				                     sizeof(uint32), out);
				OSL_DELAY(2000);
			}
		} else {
			if (what & XTAL)
				out &= ~PCI_CFG_GPIO_XTAL;
			if (what & PLL)
				out |= PCI_CFG_GPIO_PLL;
			OSL_PCI_WRITE_CONFIG(sii->osh, PCI_GPIO_OUT, sizeof(uint32), out);
			OSL_PCI_WRITE_CONFIG(sii->osh, PCI_GPIO_OUTEN, sizeof(uint32),
			                     outen);
		}
		return 0;

	default:
		return (-1);
	}

	return (0);
}

/*
 *  clock control policy function throught chipcommon
 *
 *    set dynamic clk control mode (forceslow, forcefast, dynamic)
 *    returns true if we are forcing fast clock
 *    this is a wrapper over the next internal function
 *      to allow flexible policy settings for outside caller
 */
bool
si_clkctl_cc(si_t *sih, uint mode)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	/* chipcommon cores prior to rev6 don't support dynamic clock control */
	if (sih->ccrev < 6)
		return FALSE;

	if (PCI_FORCEHT(sii))
		return (mode == CLK_FAST);

	return _si_clkctl_cc(sii, mode);
}

/* clk control mechanism through chipcommon, no policy checking */
static bool
_si_clkctl_cc(si_info_t *sii, uint mode)
{
	uint origidx = 0;
	chipcregs_t *cc;
	uint32 scc;
	uint intr_val = 0;
	bool fast = SI_FAST(sii);

	/* chipcommon cores prior to rev6 don't support dynamic clock control */
	if (sii->pub.ccrev < 6)
		return (FALSE);

	/* Chips with ccrev 10 are EOL and they don't have SYCC_HR which we use below */
	ASSERT(sii->pub.ccrev != 10);

	if (!fast) {
		INTR_OFF(sii, intr_val);
		origidx = sii->curidx;

		if ((BUSTYPE(sii->pub.bustype) == SI_BUS) &&
		    si_setcore(&sii->pub, MIPS33_CORE_ID, 0) &&
		    (si_corerev(&sii->pub) <= 7) && (sii->pub.ccrev >= 10))
			goto done;

		cc = (chipcregs_t *) si_setcore(&sii->pub, CC_CORE_ID, 0);
	} else if ((cc = (chipcregs_t *) CCREGS_FAST(sii)) == NULL)
		goto done;
	ASSERT(cc != NULL);

	if (!CCCTL_ENAB(&sii->pub) && (sii->pub.ccrev < 20))
		goto done;

	switch (mode) {
	case CLK_FAST:	/* FORCEHT, fast (pll) clock */
		if (sii->pub.ccrev < 10) {
			/* don't forget to force xtal back on before we clear SCC_DYN_XTAL.. */
			si_clkctl_xtal(&sii->pub, XTAL, ON);
			SET_REG(sii->osh, &cc->slow_clk_ctl, (SCC_XC | SCC_FS | SCC_IP), SCC_IP);
		} else if (sii->pub.ccrev < 20) {
			OR_REG(sii->osh, &cc->system_clk_ctl, SYCC_HR);
		} else {
			OR_REG(sii->osh, &cc->clk_ctl_st, CCS_FORCEHT);
		}

		/* wait for the PLL */
		if (PMUCTL_ENAB(&sii->pub)) {
			uint32 htavail = CCS_HTAVAIL;
			if (CHIPID(sii->pub.chip) == BCM4328_CHIP_ID)
				htavail = CCS0_HTAVAIL;
			SPINWAIT(((R_REG(sii->osh, &cc->clk_ctl_st) & htavail) == 0),
			         PMU_MAX_TRANSITION_DLY);
			ASSERT(R_REG(sii->osh, &cc->clk_ctl_st) & htavail);
		} else {
			OSL_DELAY(PLL_DELAY);
		}
		break;

	case CLK_DYNAMIC:	/* enable dynamic clock control */
		if (sii->pub.ccrev < 10) {
			scc = R_REG(sii->osh, &cc->slow_clk_ctl);
			scc &= ~(SCC_FS | SCC_IP | SCC_XC);
			if ((scc & SCC_SS_MASK) != SCC_SS_XTAL)
				scc |= SCC_XC;
			W_REG(sii->osh, &cc->slow_clk_ctl, scc);

			/* for dynamic control, we have to release our xtal_pu "force on" */
			if (scc & SCC_XC)
				si_clkctl_xtal(&sii->pub, XTAL, OFF);
		} else if (sii->pub.ccrev < 20) {
			/* Instaclock */
			AND_REG(sii->osh, &cc->system_clk_ctl, ~SYCC_HR);
		} else {
			AND_REG(sii->osh, &cc->clk_ctl_st, ~CCS_FORCEHT);
		}

		/* wait for the PLL */
		if (PMUCTL_ENAB(&sii->pub)) {
			uint32 htavail = CCS_HTAVAIL;
			if (CHIPID(sii->pub.chip) == BCM4328_CHIP_ID)
				htavail = CCS0_HTAVAIL;
			SPINWAIT(((R_REG(sii->osh, &cc->clk_ctl_st) & htavail) != 0),
			         PMU_MAX_TRANSITION_DLY);
			ASSERT(!(R_REG(sii->osh, &cc->clk_ctl_st) & htavail));
		} else {
			OSL_DELAY(PLL_DELAY);
		}

		break;

	default:
		ASSERT(0);
	}

done:
	if (!fast) {
		si_setcoreidx(&sii->pub, origidx);
		INTR_RESTORE(sii, intr_val);
	}
	return (mode == CLK_FAST);
}

/* Build device path. Support SI, PCI, and JTAG for now. */
int
BCMNMIATTACHFN(si_devpath)(si_t *sih, char *path, int size)
{
	int slen;

	ASSERT(path != NULL);
	ASSERT(size >= SI_DEVPATH_BUFSZ);

	if (!path || size <= 0)
		return -1;

	switch (BUSTYPE(sih->bustype)) {
	case SI_BUS:
	case JTAG_BUS:
		slen = snprintf(path, (size_t)size, "sb/%u/", si_coreidx(sih));
		break;
	case PCI_BUS:
		ASSERT((SI_INFO(sih))->osh != NULL);
		slen = snprintf(path, (size_t)size, "pci/%u/%u/",
		                OSL_PCI_BUS((SI_INFO(sih))->osh),
		                OSL_PCI_SLOT((SI_INFO(sih))->osh));
		break;
	case PCMCIA_BUS:
		SI_ERROR(("si_devpath: OSL_PCMCIA_BUS() not implemented, bus 1 assumed\n"));
		SI_ERROR(("si_devpath: OSL_PCMCIA_SLOT() not implemented, slot 1 assumed\n"));
		slen = snprintf(path, (size_t)size, "pc/1/1/");
		break;
	default:
		slen = -1;
		ASSERT(0);
		break;
	}

	if (slen < 0 || slen >= size) {
		path[0] = '\0';
		return -1;
	}

	return 0;
}

char *
BCMATTACHFN(si_coded_devpathvar)(si_t *sih, char *varname, int var_len, const char *name)
{
	char pathname[SI_DEVPATH_BUFSZ + 32];
	char devpath[SI_DEVPATH_BUFSZ + 32];
	char *p;
	int idx;
	int len1;
	int len2;

	/* try to get compact devpath if it exist */
	if (si_devpath(sih, devpath, SI_DEVPATH_BUFSZ) == 0) {
		/* eliminate ending '/' (if present) */
		len1 = strlen(devpath);
		if (devpath[len1 - 1] == '/')
			len1--;

		for (idx = 0; idx < SI_MAXCORES; idx++) {
			snprintf(pathname, SI_DEVPATH_BUFSZ, "devpath%d", idx);
			if ((p = getvar(NULL, pathname)) == NULL)
				continue;

			/* eliminate ending '/' (if present) */
			len2 = strlen(p);
			if (p[len2 - 1] == '/')
				len2--;

			/* check that both lengths match and if so compare */
			/* the strings (minus trailing '/'s if present */
			if ((len1 == len2) && (memcmp(p, devpath, len1) == 0)) {
				snprintf(varname, var_len, "%d:%s", idx, name);
				return varname;
			}
		}
	}

	return NULL;
}

/* Get a variable, but only if it has a devpath prefix */
char *
BCMATTACHFN(si_getdevpathvar)(si_t *sih, const char *name)
{
	char varname[SI_DEVPATH_BUFSZ + 32];
	char *val;

	si_devpathvar(sih, varname, sizeof(varname), name);

	if ((val = getvar(NULL, varname)) != NULL)
		return val;

	/* try to get compact devpath if it exist */
	if (si_coded_devpathvar(sih, varname, sizeof(varname), name) == NULL)
		return NULL;

	return (getvar(NULL, varname));
}

/* Get a variable, but only if it has a devpath prefix */
int
BCMATTACHFN(si_getdevpathintvar)(si_t *sih, const char *name)
{
#if defined(BCMBUSTYPE) && (BCMBUSTYPE == SI_BUS)
	return (getintvar(NULL, name));
#else
	char varname[SI_DEVPATH_BUFSZ + 32];
	int val;

	si_devpathvar(sih, varname, sizeof(varname), name);

	if ((val = getintvar(NULL, varname)) != 0)
		return val;

	/* try to get compact devpath if it exist */
	if (si_coded_devpathvar(sih, varname, sizeof(varname), name) == NULL)
		return 0;

	return (getintvar(NULL, varname));
#endif /* BCMBUSTYPE && BCMBUSTYPE == SI_BUS */
}

#ifndef DONGLEBUILD
char *
si_getnvramflvar(si_t *sih, const char *name)
{
	return (getvar(NULL, name));
}
#endif /* DONGLEBUILD */

/* Concatenate the dev path with a varname into the given 'var' buffer
 * and return the 'var' pointer.
 * Nothing is done to the arguments if len == 0 or var is NULL, var is still returned.
 * On overflow, the first char will be set to '\0'.
 */
static char *
BCMATTACHFN(si_devpathvar)(si_t *sih, char *var, int len, const char *name)
{
	uint path_len;

	if (!var || len <= 0)
		return var;

	if (si_devpath(sih, var, len) == 0) {
		path_len = strlen(var);

		if (strlen(name) + 1 > (uint)(len - path_len))
			var[0] = '\0';
		else
			strncpy(var + path_len, name, len - path_len - 1);
	}

	return var;
}

/* Function to write a chipcommon register */
uint32
write_ccreg(si_t *sih, uint32 offset, uint32 mask, uint32 val)
{
	si_info_t *sii;
	uint32 reg_val = 0;

	sii = SI_INFO(sih);

	/* abort for invalid offset */
	if (offset > sizeof(chipcregs_t))
		return 0;

	reg_val = si_corereg(&sii->pub, SI_CC_IDX, offset, mask, val);

	return reg_val;
}
uint32
si_ccreg(si_t *sih, uint32 offset, uint32 mask, uint32 val)
{
	si_info_t *sii;
	uint32 reg_val = 0;

	sii = SI_INFO(sih);

	/* abort for invalid offset */
	if (offset > sizeof(chipcregs_t))
		return 0;

	reg_val = si_corereg(&sii->pub, SI_CC_IDX, offset, mask, val);

	return reg_val;
}


uint32
si_pciereg(si_t *sih, uint32 offset, uint32 mask, uint32 val, uint type)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (!PCIE(sii)) {
		SI_ERROR(("%s: Not a PCIE device\n", __FUNCTION__));
		return 0;
	}

	return pcicore_pciereg(sii->pch, offset, mask, val, type);
}

uint32
si_pcieserdesreg(si_t *sih, uint32 mdioslave, uint32 offset, uint32 mask, uint32 val)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (!PCIE(sii)) {
		SI_ERROR(("%s: Not a PCIE device\n", __FUNCTION__));
		return 0;
	}

	return pcicore_pcieserdesreg(sii->pch, mdioslave, offset, mask, val);

}

/* return TRUE if PCIE capability exists in the pci config space */
static bool
si_ispcie(si_info_t *sii)
{
	uint8 cap_ptr;

	if (BUSTYPE(sii->pub.bustype) != PCI_BUS)
		return FALSE;

	cap_ptr = pcicore_find_pci_capability(sii->osh, PCI_CAP_PCIECAP_ID, NULL, NULL);
	if (!cap_ptr)
		return FALSE;

	return TRUE;
}

/* Wake-on-wireless-LAN (WOWL) support functions */
/* Enable PME generation and disable clkreq */
void
si_pci_pmeen(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	pcicore_pmeen(sii->pch);
}

/* Return TRUE if PME status is set */
bool
si_pci_pmestat(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	return pcicore_pmestat(sii->pch);
}

/* Disable PME generation, clear the PME status bit if set */
void
si_pci_pmeclr(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	pcicore_pmeclr(sii->pch);
}

void
si_pci_pmestatclr(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	pcicore_pmestatclr(sii->pch);
}

/* initialize the pcmcia core */
void
si_pcmcia_init(si_t *sih)
{
	si_info_t *sii;
	uint8 cor = 0;

	sii = SI_INFO(sih);

	/* enable d11 mac interrupts */
	OSL_PCMCIA_READ_ATTR(sii->osh, PCMCIA_FCR0 + PCMCIA_COR, &cor, 1);
	cor |= COR_IRQEN | COR_FUNEN;
	OSL_PCMCIA_WRITE_ATTR(sii->osh, PCMCIA_FCR0 + PCMCIA_COR, &cor, 1);

}


bool
BCMATTACHFN(si_pci_war16165)(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	return (PCI(sii) && (sih->buscorerev <= 10));
}

/* Disable pcie_war_ovr for some platforms (sigh!)
 * This is for boards that have BFL2_PCIEWAR_OVR set
 * but are in systems that still want the benefits of ASPM
 * Note that this should be done AFTER si_doattach
 */
void
si_pcie_war_ovr_update(si_t *sih, uint8 aspm)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (!PCIE_GEN1(sii))
		return;

	pcie_war_ovr_aspm_update(sii->pch, aspm);
}

void
si_pcie_power_save_enable(si_t *sih, bool enable)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (!PCIE_GEN1(sii))
		return;

	pcie_power_save_enable(sii->pch, enable);
}

void
si_pcie_set_maxpayload_size(si_t *sih, uint16 size)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (!PCIE(sii))
		return;

	pcie_set_maxpayload_size(sii->pch, size);
}

uint16
si_pcie_get_maxpayload_size(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (!PCIE(sii))
		return (0);

	return pcie_get_maxpayload_size(sii->pch);
}

void
si_pcie_set_request_size(si_t *sih, uint16 size)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (!PCIE(sii))
		return;

	pcie_set_request_size(sii->pch, size);
}

uint16
si_pcie_get_request_size(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (!PCIE_GEN1(sii))
		return (0);

	return pcie_get_request_size(sii->pch);
}


uint16
si_pcie_get_ssid(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (!PCIE_GEN1(sii))
		return (0);

	return pcie_get_ssid(sii->pch);
}

uint32
si_pcie_get_bar0(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (!PCIE(sii))
		return (0);

	return pcie_get_bar0(sii->pch);
}

int
si_pcie_configspace_cache(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (!PCIE(sii))
		return -1;

	return pcie_configspace_cache(sii->pch);
}

int
si_pcie_configspace_restore(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (!PCIE(sii))
		return -1;

	return pcie_configspace_restore(sii->pch);
}

int
si_pcie_configspace_get(si_t *sih, uint8 *buf, uint size)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (!PCIE(sii) || size > PCI_CONFIG_SPACE_SIZE)
		return -1;

	return pcie_configspace_get(sii->pch, buf, size);
}

/* back door for other module to override chippkg */
void
si_chippkg_set(si_t *sih, uint val)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	sii->pub.chippkg = val;
}

void
BCMINITFN(si_pci_up)(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	/* if not pci bus, we're done */
	if (BUSTYPE(sih->bustype) != PCI_BUS)
		return;

	if (PCI_FORCEHT(sii))
		_si_clkctl_cc(sii, CLK_FAST);

	if (PCIE(sii)) {
		pcicore_up(sii->pch, SI_PCIUP);
		if (((CHIPID(sih->chip) == BCM4311_CHIP_ID) && (CHIPREV(sih->chiprev) == 2)) ||
		    (CHIPID(sih->chip) == BCM4312_CHIP_ID))
			sb_set_initiator_to((void *)sii, 0x3,
			                    si_findcoreidx((void *)sii, D11_CORE_ID, 0));
	}
}

/* Unconfigure and/or apply various WARs when system is going to sleep mode */
void
BCMUNINITFN(si_pci_sleep)(si_t *sih)
{
	si_info_t *sii;

	do_4360_pcie2_war = 0;

	sii = SI_INFO(sih);

	pcicore_sleep(sii->pch);
}

/* Unconfigure and/or apply various WARs when going down */
void
BCMINITFN(si_pci_down)(si_t *sih)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	/* if not pci bus, we're done */
	if (BUSTYPE(sih->bustype) != PCI_BUS)
		return;

	/* release FORCEHT since chip is going to "down" state */
	if (PCI_FORCEHT(sii))
		_si_clkctl_cc(sii, CLK_DYNAMIC);

	pcicore_down(sii->pch, SI_PCIDOWN);
}

/*
 * Configure the pci core for pci client (NIC) action
 * coremask is the bitvec of cores by index to be enabled.
 */
void
BCMATTACHFN(si_pci_setup)(si_t *sih, uint coremask)
{
	si_info_t *sii;
	sbpciregs_t *pciregs = NULL;
	uint32 siflag = 0, w;
	uint idx = 0;

	sii = SI_INFO(sih);

	if (BUSTYPE(sii->pub.bustype) != PCI_BUS)
		return;

	ASSERT(PCI(sii) || PCIE(sii));
	ASSERT(sii->pub.buscoreidx != BADIDX);

	if (PCI(sii)) {
		/* get current core index */
		idx = sii->curidx;

		/* we interrupt on this backplane flag number */
		siflag = si_flag(sih);

		/* switch over to pci core */
		pciregs = (sbpciregs_t *)si_setcoreidx(sih, sii->pub.buscoreidx);
	}

	/*
	 * Enable sb->pci interrupts.  Assume
	 * PCI rev 2.3 support was added in pci core rev 6 and things changed..
	 */
	if (PCIE(sii) || (PCI(sii) && ((sii->pub.buscorerev) >= 6))) {
		/* pci config write to set this core bit in PCIIntMask */
		w = OSL_PCI_READ_CONFIG(sii->osh, PCI_INT_MASK, sizeof(uint32));
		w |= (coremask << PCI_SBIM_SHIFT);
#ifdef USER_MODE
		/* User mode operate with interrupt disabled */
		w &= !(coremask << PCI_SBIM_SHIFT);
#endif
		OSL_PCI_WRITE_CONFIG(sii->osh, PCI_INT_MASK, sizeof(uint32), w);
	} else {
		/* set sbintvec bit for our flag number */
		si_setint(sih, siflag);
	}

	if (PCI(sii)) {
		OR_REG(sii->osh, &pciregs->sbtopci2, (SBTOPCI_PREF | SBTOPCI_BURST));
		if (sii->pub.buscorerev >= 11) {
			OR_REG(sii->osh, &pciregs->sbtopci2, SBTOPCI_RC_READMULTI);
			w = R_REG(sii->osh, &pciregs->clkrun);
			W_REG(sii->osh, &pciregs->clkrun, (w | PCI_CLKRUN_DSBL));
			w = R_REG(sii->osh, &pciregs->clkrun);
		}

		/* switch back to previous core */
		si_setcoreidx(sih, idx);
	}
}

uint8
si_pcieclkreq(si_t *sih, uint32 mask, uint32 val)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (!(PCIE(sii)))
		return 0;

	return pcie_clkreq(sii->pch, mask, val);
}

uint32
si_pcielcreg(si_t *sih, uint32 mask, uint32 val)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (!PCIE(sii))
		return 0;

	return pcie_lcreg(sii->pch, mask, val);
}

uint8
si_pcieltrenable(si_t *sih, uint32 mask, uint32 val)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (!(PCIE(sii)))
		return 0;

	return pcie_ltrenable(sii->pch, mask, val);
}

void
si_pcie_set_error_injection(si_t *sih, uint32 mode)
{
	si_info_t *sii;

	sii = SI_INFO(sih);

	if (!PCIE(sii))
		return;

	pcie_set_error_injection(sii->pch, mode);
}

/* indirect way to read pcie config regs */
uint
si_pcie_readreg(void *sih, uint addrtype, uint offset)
{
	return pcie_readreg(sih, (sbpcieregs_t *)PCIEREGS(((si_info_t *)sih)),
	                    addrtype, offset);
}

/*
 * Fixup SROMless PCI device's configuration.
 * The current core may be changed upon return.
 */
int
si_pci_fixcfg(si_t *sih)
{
	uint origidx, pciidx;
	sbpciregs_t *pciregs = NULL;
	sbpcieregs_t *pcieregs = NULL;
	uint16 val16, *reg16 = NULL;
	uint32 w;

	si_info_t *sii = SI_INFO(sih);

	ASSERT(BUSTYPE(sii->pub.bustype) == PCI_BUS);

	if ((CHIPID(sii->pub.chip) == BCM4321_CHIP_ID) && (CHIPREV(sii->pub.chiprev) < 2)) {
		w = (CHIPREV(sii->pub.chiprev) == 0) ?
		        CHIPCTRL_4321A0_DEFAULT : CHIPCTRL_4321A1_DEFAULT;
		si_corereg(&sii->pub, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol), ~0, w);
	}

	/* Fixup PI in SROM shadow area to enable the correct PCI core access */
	/* save the current index */
	origidx = si_coreidx(&sii->pub);

	/* check 'pi' is correct and fix it if not */
	if (sii->pub.buscoretype == PCIE2_CORE_ID) {
		pcieregs = (sbpcieregs_t *)si_setcore(&sii->pub, PCIE2_CORE_ID, 0);
		ASSERT(pcieregs != NULL);
		reg16 = &pcieregs->sprom[SRSH_PI_OFFSET];
	} else if (sii->pub.buscoretype == PCIE_CORE_ID) {
		pcieregs = (sbpcieregs_t *)si_setcore(&sii->pub, PCIE_CORE_ID, 0);
		ASSERT(pcieregs != NULL);
		reg16 = &pcieregs->sprom[SRSH_PI_OFFSET];
	} else if (sii->pub.buscoretype == PCI_CORE_ID) {
		pciregs = (sbpciregs_t *)si_setcore(&sii->pub, PCI_CORE_ID, 0);
		ASSERT(pciregs != NULL);
		reg16 = &pciregs->sprom[SRSH_PI_OFFSET];
	}
	pciidx = si_coreidx(&sii->pub);

	if (!reg16) return -1;

	val16 = R_REG(sii->osh, reg16);
	if (((val16 & SRSH_PI_MASK) >> SRSH_PI_SHIFT) != (uint16)pciidx) {
		val16 = (uint16)(pciidx << SRSH_PI_SHIFT) | (val16 & ~SRSH_PI_MASK);
		W_REG(sii->osh, reg16, val16);
	}

	/* restore the original index */
	si_setcoreidx(&sii->pub, origidx);

	pcicore_hwup(sii->pch);
	return 0;
}

#if defined(BCMDBG)
#endif 

/* change logical "focus" to the gpio core for optimized access */
void *
si_gpiosetcore(si_t *sih)
{
	return (si_setcoreidx(sih, SI_CC_IDX));
}

/*
 * mask & set gpiocontrol bits.
 * If a gpiocontrol bit is set to 0, chipcommon controls the corresponding GPIO pin.
 * If a gpiocontrol bit is set to 1, the GPIO pin is no longer a GPIO and becomes dedicated
 *   to some chip-specific purpose.
 */
uint32
si_gpiocontrol(si_t *sih, uint32 mask, uint32 val, uint8 priority)
{
	uint regoff;

	regoff = 0;

	/* gpios could be shared on router platforms
	 * ignore reservation if it's high priority (e.g., test apps)
	 */
	if ((priority != GPIO_HI_PRIORITY) &&
	    (BUSTYPE(sih->bustype) == SI_BUS) && (val || mask)) {
		mask = priority ? (si_gpioreservation & mask) :
			((si_gpioreservation | mask) & ~(si_gpioreservation));
		val &= mask;
	}

	regoff = OFFSETOF(chipcregs_t, gpiocontrol);
	return (si_corereg(sih, SI_CC_IDX, regoff, mask, val));
}

/* mask&set gpio output enable bits */
uint32
si_gpioouten(si_t *sih, uint32 mask, uint32 val, uint8 priority)
{
	uint regoff;

	regoff = 0;

	/* gpios could be shared on router platforms
	 * ignore reservation if it's high priority (e.g., test apps)
	 */
	if ((priority != GPIO_HI_PRIORITY) &&
	    (BUSTYPE(sih->bustype) == SI_BUS) && (val || mask)) {
		mask = priority ? (si_gpioreservation & mask) :
			((si_gpioreservation | mask) & ~(si_gpioreservation));
		val &= mask;
	}

	regoff = OFFSETOF(chipcregs_t, gpioouten);
	return (si_corereg(sih, SI_CC_IDX, regoff, mask, val));
}

/* mask&set gpio output bits */
uint32
si_gpioout(si_t *sih, uint32 mask, uint32 val, uint8 priority)
{
	uint regoff;

	regoff = 0;

	/* gpios could be shared on router platforms
	 * ignore reservation if it's high priority (e.g., test apps)
	 */
	if ((priority != GPIO_HI_PRIORITY) &&
	    (BUSTYPE(sih->bustype) == SI_BUS) && (val || mask)) {
		mask = priority ? (si_gpioreservation & mask) :
			((si_gpioreservation | mask) & ~(si_gpioreservation));
		val &= mask;
	}

	regoff = OFFSETOF(chipcregs_t, gpioout);
	return (si_corereg(sih, SI_CC_IDX, regoff, mask, val));
}

/* reserve one gpio */
uint32
si_gpioreserve(si_t *sih, uint32 gpio_bitmask, uint8 priority)
{
	/* only cores on SI_BUS share GPIO's and only applcation users need to
	 * reserve/release GPIO
	 */
	if ((BUSTYPE(sih->bustype) != SI_BUS) || (!priority)) {
		ASSERT((BUSTYPE(sih->bustype) == SI_BUS) && (priority));
		return 0xffffffff;
	}
	/* make sure only one bit is set */
	if ((!gpio_bitmask) || ((gpio_bitmask) & (gpio_bitmask - 1))) {
		ASSERT((gpio_bitmask) && !((gpio_bitmask) & (gpio_bitmask - 1)));
		return 0xffffffff;
	}

	/* already reserved */
	if (si_gpioreservation & gpio_bitmask)
		return 0xffffffff;
	/* set reservation */
	si_gpioreservation |= gpio_bitmask;

	return si_gpioreservation;
}

/* release one gpio */
/*
 * releasing the gpio doesn't change the current value on the GPIO last write value
 * persists till some one overwrites it
 */

uint32
si_gpiorelease(si_t *sih, uint32 gpio_bitmask, uint8 priority)
{
	/* only cores on SI_BUS share GPIO's and only applcation users need to
	 * reserve/release GPIO
	 */
	if ((BUSTYPE(sih->bustype) != SI_BUS) || (!priority)) {
		ASSERT((BUSTYPE(sih->bustype) == SI_BUS) && (priority));
		return 0xffffffff;
	}
	/* make sure only one bit is set */
	if ((!gpio_bitmask) || ((gpio_bitmask) & (gpio_bitmask - 1))) {
		ASSERT((gpio_bitmask) && !((gpio_bitmask) & (gpio_bitmask - 1)));
		return 0xffffffff;
	}

	/* already released */
	if (!(si_gpioreservation & gpio_bitmask))
		return 0xffffffff;

	/* clear reservation */
	si_gpioreservation &= ~gpio_bitmask;

	return si_gpioreservation;
}

/* return the current gpioin register value */
uint32
si_gpioin(si_t *sih)
{
	uint regoff;

	regoff = OFFSETOF(chipcregs_t, gpioin);
	return (si_corereg(sih, SI_CC_IDX, regoff, 0, 0));
}

/* mask&set gpio interrupt polarity bits */
uint32
si_gpiointpolarity(si_t *sih, uint32 mask, uint32 val, uint8 priority)
{
	uint regoff;

	/* gpios could be shared on router platforms */
	if ((BUSTYPE(sih->bustype) == SI_BUS) && (val || mask)) {
		mask = priority ? (si_gpioreservation & mask) :
			((si_gpioreservation | mask) & ~(si_gpioreservation));
		val &= mask;
	}

	regoff = OFFSETOF(chipcregs_t, gpiointpolarity);
	return (si_corereg(sih, SI_CC_IDX, regoff, mask, val));
}

/* mask&set gpio interrupt mask bits */
uint32
si_gpiointmask(si_t *sih, uint32 mask, uint32 val, uint8 priority)
{
	uint regoff;

	/* gpios could be shared on router platforms */
	if ((BUSTYPE(sih->bustype) == SI_BUS) && (val || mask)) {
		mask = priority ? (si_gpioreservation & mask) :
			((si_gpioreservation | mask) & ~(si_gpioreservation));
		val &= mask;
	}

	regoff = OFFSETOF(chipcregs_t, gpiointmask);
	return (si_corereg(sih, SI_CC_IDX, regoff, mask, val));
}

/* assign the gpio to an led */
uint32
si_gpioled(si_t *sih, uint32 mask, uint32 val)
{
	if (sih->ccrev < 16)
		return 0xffffffff;

	/* gpio led powersave reg */
	return (si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gpiotimeroutmask), mask, val));
}

/* mask&set gpio timer val */
uint32
si_gpiotimerval(si_t *sih, uint32 mask, uint32 gpiotimerval)
{
	if (sih->ccrev < 16)
		return 0xffffffff;

	return (si_corereg(sih, SI_CC_IDX,
		OFFSETOF(chipcregs_t, gpiotimerval), mask, gpiotimerval));
}

uint32
si_gpiopull(si_t *sih, bool updown, uint32 mask, uint32 val)
{
	uint offs;

	if (sih->ccrev < 20)
		return 0xffffffff;

	offs = (updown ? OFFSETOF(chipcregs_t, gpiopulldown) : OFFSETOF(chipcregs_t, gpiopullup));
	return (si_corereg(sih, SI_CC_IDX, offs, mask, val));
}

uint32
si_gpioevent(si_t *sih, uint regtype, uint32 mask, uint32 val)
{
	uint offs;

	if (sih->ccrev < 11)
		return 0xffffffff;

	if (regtype == GPIO_REGEVT)
		offs = OFFSETOF(chipcregs_t, gpioevent);
	else if (regtype == GPIO_REGEVT_INTMSK)
		offs = OFFSETOF(chipcregs_t, gpioeventintmask);
	else if (regtype == GPIO_REGEVT_INTPOL)
		offs = OFFSETOF(chipcregs_t, gpioeventintpolarity);
	else
		return 0xffffffff;

	return (si_corereg(sih, SI_CC_IDX, offs, mask, val));
}

void *
BCMATTACHFN(si_gpio_handler_register)(si_t *sih, uint32 event,
	bool level, gpio_handler_t cb, void *arg)
{
	si_info_t *sii;
	gpioh_item_t *gi;

	ASSERT(event);
	ASSERT(cb != NULL);

	sii = SI_INFO(sih);
	if (sih->ccrev < 11)
		return NULL;

	if ((gi = MALLOC(sii->osh, sizeof(gpioh_item_t))) == NULL)
		return NULL;

	bzero(gi, sizeof(gpioh_item_t));
	gi->event = event;
	gi->handler = cb;
	gi->arg = arg;
	gi->level = level;

	gi->next = sii->gpioh_head;
	sii->gpioh_head = gi;

#ifdef BCMDBG_ERR
	{
		gpioh_item_t *h = sii->gpioh_head;
		int cnt = 0;

		for (; h; h = h->next) {
			cnt++;
			SI_ERROR(("gpiohdler=%p cb=%p event=0x%x\n",
				h, h->handler, h->event));
		}
		SI_ERROR(("gpiohdler total=%d\n", cnt));
	}
#endif
	return (void *)(gi);
}

void
BCMATTACHFN(si_gpio_handler_unregister)(si_t *sih, void *gpioh)
{
	si_info_t *sii;
	gpioh_item_t *p, *n;

	sii = SI_INFO(sih);
	if (sih->ccrev < 11)
		return;

	ASSERT(sii->gpioh_head != NULL);
	if ((void*)sii->gpioh_head == gpioh) {
		sii->gpioh_head = sii->gpioh_head->next;
		MFREE(sii->osh, gpioh, sizeof(gpioh_item_t));
		return;
	} else {
		p = sii->gpioh_head;
		n = p->next;
		while (n) {
			if ((void*)n == gpioh) {
				p->next = n->next;
				MFREE(sii->osh, gpioh, sizeof(gpioh_item_t));
				return;
			}
			p = n;
			n = n->next;
		}
	}

#ifdef BCMDBG_ERR
	{
		gpioh_item_t *h = sii->gpioh_head;
		int cnt = 0;

		for (; h; h = h->next) {
			cnt++;
			SI_ERROR(("gpiohdler=%p cb=%p event=0x%x\n",
				h, h->handler, h->event));
		}
		SI_ERROR(("gpiohdler total=%d\n", cnt));
	}
#endif
	ASSERT(0); /* Not found in list */
}

void
si_gpio_handler_process(si_t *sih)
{
	si_info_t *sii;
	gpioh_item_t *h;
	uint32 level = si_gpioin(sih);
	uint32 levelp = si_gpiointpolarity(sih, 0, 0, 0);
	uint32 edge = si_gpioevent(sih, GPIO_REGEVT, 0, 0);
	uint32 edgep = si_gpioevent(sih, GPIO_REGEVT_INTPOL, 0, 0);

	sii = SI_INFO(sih);
	for (h = sii->gpioh_head; h != NULL; h = h->next) {
		if (h->handler) {
			uint32 status = (h->level ? level : edge) & h->event;
			uint32 polarity = (h->level ? levelp : edgep) & h->event;

			/* polarity bitval is opposite of status bitval */
			if (status ^ polarity)
				h->handler(status, h->arg);
		}
	}

	si_gpioevent(sih, GPIO_REGEVT, edge, edge); /* clear edge-trigger status */
}

uint32
si_gpio_int_enable(si_t *sih, bool enable)
{
	uint offs;

	if (sih->ccrev < 11)
		return 0xffffffff;

	offs = OFFSETOF(chipcregs_t, intmask);
	return (si_corereg(sih, SI_CC_IDX, offs, CI_GPIO, (enable ? CI_GPIO : 0)));
}


/* Return the size of the specified SOCRAM bank */
static uint
socram_banksize(si_info_t *sii, sbsocramregs_t *regs, uint8 idx, uint8 mem_type)
{
	uint banksize, bankinfo;
	uint bankidx = idx | (mem_type << SOCRAM_BANKIDX_MEMTYPE_SHIFT);

	ASSERT(mem_type <= SOCRAM_MEMTYPE_DEVRAM);

	W_REG(sii->osh, &regs->bankidx, bankidx);
	bankinfo = R_REG(sii->osh, &regs->bankinfo);
	banksize = SOCRAM_BANKINFO_SZBASE * ((bankinfo & SOCRAM_BANKINFO_SZMASK) + 1);
	return banksize;
}

void
si_socdevram(si_t *sih, bool set, uint8 *enable, uint8 *protect, uint8 *remap)
{
	si_info_t *sii;
	uint origidx;
	uint intr_val = 0;
	sbsocramregs_t *regs;
	bool wasup;
	uint corerev;

	sii = SI_INFO(sih);

	/* Block ints and save current core */
	INTR_OFF(sii, intr_val);
	origidx = si_coreidx(sih);

	if (!set)
		*enable = *protect = *remap = 0;

	/* Switch to SOCRAM core */
	if (!(regs = si_setcore(sih, SOCRAM_CORE_ID, 0)))
		goto done;

	/* Get info for determining size */
	if (!(wasup = si_iscoreup(sih)))
		si_core_reset(sih, 0, 0);

	corerev = si_corerev(sih);
	if (corerev >= 10) {
		uint32 extcinfo;
		uint8 nb;
		uint8 i;
		uint32 bankidx, bankinfo;

		extcinfo = R_REG(sii->osh, &regs->extracoreinfo);
		nb = ((extcinfo & SOCRAM_DEVRAMBANK_MASK) >> SOCRAM_DEVRAMBANK_SHIFT);
		for (i = 0; i < nb; i++) {
			bankidx = i | (SOCRAM_MEMTYPE_DEVRAM << SOCRAM_BANKIDX_MEMTYPE_SHIFT);
			W_REG(sii->osh, &regs->bankidx, bankidx);
			bankinfo = R_REG(sii->osh, &regs->bankinfo);
			if (set) {
				bankinfo &= ~SOCRAM_BANKINFO_DEVRAMSEL_MASK;
				bankinfo &= ~SOCRAM_BANKINFO_DEVRAMPRO_MASK;
				bankinfo &= ~SOCRAM_BANKINFO_DEVRAMREMAP_MASK;
				if (*enable) {
					bankinfo |= (1 << SOCRAM_BANKINFO_DEVRAMSEL_SHIFT);
					if (*protect)
						bankinfo |= (1 << SOCRAM_BANKINFO_DEVRAMPRO_SHIFT);
					if ((corerev >= 16) && *remap)
						bankinfo |=
							(1 << SOCRAM_BANKINFO_DEVRAMREMAP_SHIFT);
				}
				W_REG(sii->osh, &regs->bankinfo, bankinfo);
			}
			else if (i == 0) {
				if (bankinfo & SOCRAM_BANKINFO_DEVRAMSEL_MASK) {
					*enable = 1;
					if (bankinfo & SOCRAM_BANKINFO_DEVRAMPRO_MASK)
						*protect = 1;
					if (bankinfo & SOCRAM_BANKINFO_DEVRAMREMAP_MASK)
						*remap = 1;
				}
			}
		}
	}

	/* Return to previous state and core */
	if (!wasup)
		si_core_disable(sih, 0);
	si_setcoreidx(sih, origidx);

done:
	INTR_RESTORE(sii, intr_val);
}

bool
si_socdevram_remap_isenb(si_t *sih)
{
	si_info_t *sii;
	uint origidx;
	uint intr_val = 0;
	sbsocramregs_t *regs;
	bool wasup, remap = FALSE;
	uint corerev;
	uint32 extcinfo;
	uint8 nb;
	uint8 i;
	uint32 bankidx, bankinfo;

	sii = SI_INFO(sih);

	/* Block ints and save current core */
	INTR_OFF(sii, intr_val);
	origidx = si_coreidx(sih);

	/* Switch to SOCRAM core */
	if (!(regs = si_setcore(sih, SOCRAM_CORE_ID, 0)))
		goto done;

	/* Get info for determining size */
	if (!(wasup = si_iscoreup(sih)))
		si_core_reset(sih, 0, 0);

	corerev = si_corerev(sih);
	if (corerev >= 16) {
		extcinfo = R_REG(sii->osh, &regs->extracoreinfo);
		nb = ((extcinfo & SOCRAM_DEVRAMBANK_MASK) >> SOCRAM_DEVRAMBANK_SHIFT);
		for (i = 0; i < nb; i++) {
			bankidx = i | (SOCRAM_MEMTYPE_DEVRAM << SOCRAM_BANKIDX_MEMTYPE_SHIFT);
			W_REG(sii->osh, &regs->bankidx, bankidx);
			bankinfo = R_REG(sii->osh, &regs->bankinfo);
			if (bankinfo & SOCRAM_BANKINFO_DEVRAMREMAP_MASK) {
				remap = TRUE;
				break;
			}
		}
	}

	/* Return to previous state and core */
	if (!wasup)
		si_core_disable(sih, 0);
	si_setcoreidx(sih, origidx);

done:
	INTR_RESTORE(sii, intr_val);
	return remap;
}

bool
si_socdevram_pkg(si_t *sih)
{
	if (si_socdevram_size(sih) > 0)
		return TRUE;
	else
		return FALSE;
}

uint32
si_socdevram_size(si_t *sih)
{
	si_info_t *sii;
	uint origidx;
	uint intr_val = 0;
	uint32 memsize = 0;
	sbsocramregs_t *regs;
	bool wasup;
	uint corerev;

	sii = SI_INFO(sih);

	/* Block ints and save current core */
	INTR_OFF(sii, intr_val);
	origidx = si_coreidx(sih);

	/* Switch to SOCRAM core */
	if (!(regs = si_setcore(sih, SOCRAM_CORE_ID, 0)))
		goto done;

	/* Get info for determining size */
	if (!(wasup = si_iscoreup(sih)))
		si_core_reset(sih, 0, 0);

	corerev = si_corerev(sih);
	if (corerev >= 10) {
		uint32 extcinfo;
		uint8 nb;
		uint8 i;

		extcinfo = R_REG(sii->osh, &regs->extracoreinfo);
		nb = (((extcinfo & SOCRAM_DEVRAMBANK_MASK) >> SOCRAM_DEVRAMBANK_SHIFT));
		for (i = 0; i < nb; i++)
			memsize += socram_banksize(sii, regs, i, SOCRAM_MEMTYPE_DEVRAM);
	}

	/* Return to previous state and core */
	if (!wasup)
		si_core_disable(sih, 0);
	si_setcoreidx(sih, origidx);

done:
	INTR_RESTORE(sii, intr_val);

	return memsize;
}

uint32
si_socdevram_remap_size(si_t *sih)
{
	si_info_t *sii;
	uint origidx;
	uint intr_val = 0;
	uint32 memsize = 0, banksz;
	sbsocramregs_t *regs;
	bool wasup;
	uint corerev;
	uint32 extcinfo;
	uint8 nb;
	uint8 i;
	uint32 bankidx, bankinfo;

	sii = SI_INFO(sih);

	/* Block ints and save current core */
	INTR_OFF(sii, intr_val);
	origidx = si_coreidx(sih);

	/* Switch to SOCRAM core */
	if (!(regs = si_setcore(sih, SOCRAM_CORE_ID, 0)))
		goto done;

	/* Get info for determining size */
	if (!(wasup = si_iscoreup(sih)))
		si_core_reset(sih, 0, 0);

	corerev = si_corerev(sih);
	if (corerev >= 16) {
		extcinfo = R_REG(sii->osh, &regs->extracoreinfo);
		nb = (((extcinfo & SOCRAM_DEVRAMBANK_MASK) >> SOCRAM_DEVRAMBANK_SHIFT));

		/*
		 * FIX: A0 Issue: Max addressable is 512KB, instead 640KB
		 * Only four banks are accessible to ARM
		 */
		if ((corerev == 16) && (nb == 5))
			nb = 4;

		for (i = 0; i < nb; i++) {
			bankidx = i | (SOCRAM_MEMTYPE_DEVRAM << SOCRAM_BANKIDX_MEMTYPE_SHIFT);
			W_REG(sii->osh, &regs->bankidx, bankidx);
			bankinfo = R_REG(sii->osh, &regs->bankinfo);
			if (bankinfo & SOCRAM_BANKINFO_DEVRAMREMAP_MASK) {
				banksz = socram_banksize(sii, regs, i, SOCRAM_MEMTYPE_DEVRAM);
				memsize += banksz;
			} else {
				/* Account only consecutive banks for now */
				break;
			}
		}
	}

	/* Return to previous state and core */
	if (!wasup)
		si_core_disable(sih, 0);
	si_setcoreidx(sih, origidx);

done:
	INTR_RESTORE(sii, intr_val);

	return memsize;
}

/* Return the RAM size of the SOCRAM core */
uint32
si_socram_size(si_t *sih)
{
	si_info_t *sii;
	uint origidx;
	uint intr_val = 0;

	sbsocramregs_t *regs;
	bool wasup;
	uint corerev;
	uint32 coreinfo;
	uint memsize = 0;

	sii = SI_INFO(sih);

	/* Block ints and save current core */
	INTR_OFF(sii, intr_val);
	origidx = si_coreidx(sih);

	/* Switch to SOCRAM core */
	if (!(regs = si_setcore(sih, SOCRAM_CORE_ID, 0)))
		goto done;

	/* Get info for determining size */
	if (!(wasup = si_iscoreup(sih)))
		si_core_reset(sih, 0, 0);
	corerev = si_corerev(sih);
	coreinfo = R_REG(sii->osh, &regs->coreinfo);

	/* Calculate size from coreinfo based on rev */
	if (corerev == 0)
		memsize = 1 << (16 + (coreinfo & SRCI_MS0_MASK));
	else if (corerev < 3) {
		memsize = 1 << (SR_BSZ_BASE + (coreinfo & SRCI_SRBSZ_MASK));
		memsize *= (coreinfo & SRCI_SRNB_MASK) >> SRCI_SRNB_SHIFT;
	} else if ((corerev <= 7) || (corerev == 12)) {
		uint nb = (coreinfo & SRCI_SRNB_MASK) >> SRCI_SRNB_SHIFT;
		uint bsz = (coreinfo & SRCI_SRBSZ_MASK);
		uint lss = (coreinfo & SRCI_LSS_MASK) >> SRCI_LSS_SHIFT;
		if (lss != 0)
			nb --;
		memsize = nb * (1 << (bsz + SR_BSZ_BASE));
		if (lss != 0)
			memsize += (1 << ((lss - 1) + SR_BSZ_BASE));
	} else {
		uint8 i;
		uint nb = (coreinfo & SRCI_SRNB_MASK) >> SRCI_SRNB_SHIFT;
		for (i = 0; i < nb; i++)
			memsize += socram_banksize(sii, regs, i, SOCRAM_MEMTYPE_RAM);
	}

	/* Return to previous state and core */
	if (!wasup)
		si_core_disable(sih, 0);
	si_setcoreidx(sih, origidx);

done:
	INTR_RESTORE(sii, intr_val);

	return memsize;
}

#if defined(WLOFFLD)

/* Return the TCM-RAM size of the ARMCR4 core. */
uint32
si_tcm_size(si_t *sih)
{
	si_info_t *sii;
	uint origidx;
	uint intr_val = 0;
	uint8 *regs;
	bool wasup;
	uint32 corecap;
	uint memsize = 0;
	uint32 nab = 0;
	uint32 nbb = 0;
	uint32 totb = 0;
	uint32 bxinfo = 0;
	uint32 idx = 0;
	uint32 *arm_cap_reg;
	uint32 *arm_bidx;
	uint32 *arm_binfo;

	sii = SI_INFO(sih);

	/* Block ints and save current core */
	INTR_OFF(sii, intr_val);
	origidx = si_coreidx(sih);

	/* Switch to CR4 core */
	if (!(regs = si_setcore(sih, ARMCR4_CORE_ID, 0)))
		goto done;

	/* Get info for determining size. If in reset, come out of reset,
	 * but remain in halt
	 */
	if (!(wasup = si_iscoreup(sih)))
		si_core_reset(sih, SICF_CPUHALT, SICF_CPUHALT);

	arm_cap_reg = (uint32 *)(regs + SI_CR4_CAP);
	corecap = R_REG(sii->osh, arm_cap_reg);

	nab = (corecap & ARMCR4_TCBANB_MASK) >> ARMCR4_TCBANB_SHIFT;
	nbb = (corecap & ARMCR4_TCBBNB_MASK) >> ARMCR4_TCBBNB_SHIFT;
	totb = nab + nbb;

	arm_bidx = (uint32 *)(regs + SI_CR4_BANKIDX);
	arm_binfo = (uint32 *)(regs + SI_CR4_BANKINFO);
	for (idx = 0; idx < totb; idx++) {
		W_REG(sii->osh, arm_bidx, idx);

		bxinfo = R_REG(sii->osh, arm_binfo);
		memsize += ((bxinfo & ARMCR4_BSZ_MASK) + 1) * ARMCR4_BSZ_MULT;
	}

	/* Return to previous state and core */
	if (!wasup)
		si_core_disable(sih, 0);
	si_setcoreidx(sih, origidx);

done:
	INTR_RESTORE(sii, intr_val);

	return memsize;
}
#endif 

#ifdef BCMECICOEX
#define NOTIFY_BT_FM_DISABLE(sih, val) \
	si_eci_notify_bt((sih), ECI_OUT_FM_DISABLE_MASK(sih->ccrev), \
			 ((val) << ECI_OUT_FM_DISABLE_SHIFT(sih->ccrev)), FALSE)

/* Query OTP to see if FM is disabled */
static int
BCMINITFN(si_query_FMDisabled_from_OTP)(si_t *sih, uint16 *FMDisabled)
{
	int error = BCME_OK;
	uint bitoff = 0;
	bool wasup;
	void *oh;

	/* Determine the bit for the chip */
	switch (CHIPID(sih->chip)) {
	case BCM4325_CHIP_ID:
		if (CHIPREV(sih->chiprev) >= 6)
			bitoff = OTP4325_FM_DISABLED_OFFSET;
		break;
	default:
		break;
	}

	/* If there is a bit for this chip, check it */
	if (bitoff) {
		if (!(wasup = si_is_otp_powered(sih))) {
			si_otp_power(sih, TRUE);
		}

		if ((oh = otp_init(sih)) != NULL)
			*FMDisabled = !otp_read_bit(oh, OTP4325_FM_DISABLED_OFFSET);
		else
			error = BCME_NOTFOUND;

		if (!wasup) {
			si_otp_power(sih, FALSE);
		}
	}

	return error;
}

bool
si_eci(si_t *sih)
{
	return (!!(sih->cccaps & CC_CAP_ECI));
}

bool
si_seci(si_t *sih)
{
	return (sih->cccaps_ext & CC_CAP_EXT_SECI_PRESENT);
}

bool
si_gci(si_t *sih)
{
	return (sih->cccaps_ext & CC_CAP_EXT_GCI_PRESENT);
}

/* ECI Init routine */
int
BCMINITFN(si_eci_init)(si_t *sih)
{
	uint32 origidx = 0;
	si_info_t *sii;
	chipcregs_t *cc;
	bool fast;
	uint16 FMDisabled = FALSE;

	/* check for ECI capability */
	if (!(sih->cccaps & CC_CAP_ECI))
		return BCME_ERROR;

	sii = SI_INFO(sih);
	fast = SI_FAST(sii);
	if (!fast) {
		origidx = sii->curidx;
		if ((cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0)) == NULL)
			return BCME_ERROR;
	} else if ((cc = (chipcregs_t *)CCREGS_FAST(sii)) == NULL)
		return BCME_ERROR;
	ASSERT(cc);

	/* disable level based interrupts */
	if (sih->ccrev < 35) {
		W_REG(sii->osh, &cc->eci.lt35.eci_intmaskhi, 0x0);
		W_REG(sii->osh, &cc->eci.lt35.eci_intmaskmi, 0x0);
		W_REG(sii->osh, &cc->eci.lt35.eci_intmasklo, 0x0);
	}
	else {
		W_REG(sii->osh, &cc->eci.ge35.eci_intmaskhi, 0x0);
		W_REG(sii->osh, &cc->eci.ge35.eci_intmasklo, 0x0);
	}

	/* Assign eci_output bits between 'wl' and dot11mac */
	if (sih->ccrev < 35) {
		W_REG(sii->osh, &cc->eci.lt35.eci_control, ECI_MACCTRL_BITS);
	}
	else {
		W_REG(sii->osh, &cc->eci.ge35.eci_controllo, ECI_MACCTRLLO_BITS);
		W_REG(sii->osh, &cc->eci.ge35.eci_controlhi, ECI_MACCTRLHI_BITS);
	}

	/* enable only edge based interrupts
	 * only toggle on bit 62 triggers an interrupt
	 */
	if (sih->ccrev < 35) {
		W_REG(sii->osh, &cc->eci.lt35.eci_eventmaskhi, 0x0);
		W_REG(sii->osh, &cc->eci.lt35.eci_eventmaskmi, 0x0);
		W_REG(sii->osh, &cc->eci.lt35.eci_eventmasklo, 0x0);
	}
	else {
		W_REG(sii->osh, &cc->eci.ge35.eci_eventmaskhi, 0x0);
		W_REG(sii->osh, &cc->eci.ge35.eci_eventmasklo, 0x0);
	}

	/* restore previous core */
	if (!fast)
		si_setcoreidx(sih, origidx);

	/* if FM disabled in OTP, let BT know */
	if (!si_query_FMDisabled_from_OTP(sih, &FMDisabled)) {
		if (FMDisabled) {
			NOTIFY_BT_FM_DISABLE(sih, 1);
		}
	}

	return 0;
}

/*
 * Write values to BT on eci_output.
 */
void
si_eci_notify_bt(si_t *sih, uint32 mask, uint32 val, bool interrupt)
{
	uint32 offset;

	if ((sih->cccaps & CC_CAP_ECI) ||
		(si_seci(sih)))
	{
		/* ECI or SECI mode */
		/* Clear interrupt bit by default */
		if (interrupt)
			si_corereg(sih, SI_CC_IDX,
			   (sih->ccrev < 35 ?
			    OFFSETOF(chipcregs_t, eci.lt35.eci_output) :
			    OFFSETOF(chipcregs_t, eci.ge35.eci_outputlo)),
			   (1 << 30), 0);

		if (sih->ccrev >= 35) {
			if ((mask & 0xFFFF0000) == ECI48_OUT_MASKMAGIC_HIWORD) {
				offset = OFFSETOF(chipcregs_t, eci.ge35.eci_outputhi);
				mask = mask & ~0xFFFF0000;
			}
			else {
				offset = OFFSETOF(chipcregs_t, eci.ge35.eci_outputlo);
		mask = mask | (1<<30);
				val = val & ~(1 << 30);
			}
		}
		else {
			offset = OFFSETOF(chipcregs_t, eci.lt35.eci_output);
			val = val & ~(1 << 30);
		}

		si_corereg(sih, SI_CC_IDX, offset, mask, val);

		/* Set interrupt bit if needed */
		if (interrupt)
			si_corereg(sih, SI_CC_IDX,
			   (sih->ccrev < 35 ?
			    OFFSETOF(chipcregs_t, eci.lt35.eci_output) :
			    OFFSETOF(chipcregs_t, eci.ge35.eci_outputlo)),
			   (1 << 30), (1 << 30));
	}
	else if (sih->cccaps_ext & CC_CAP_EXT_GCI_PRESENT)
	{
		/* GCI Mode */
		if ((mask & 0xFFFF0000) == ECI48_OUT_MASKMAGIC_HIWORD) {
			mask = mask & ~0xFFFF0000;
			si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_output[1]), mask, val);
		}
	}
}

/* seci clock enable/disable */
static void
si_seci_clkreq(si_t *sih, bool enable)
{
	uint32 clk_ctl_st;
	uint32 offset;
	uint32 val;

	if (!si_seci(sih))
		return;

	if (enable)
		val = CLKCTL_STS_SECI_CLK_REQ;
	else
		val = 0;

	offset = OFFSETOF(chipcregs_t, clk_ctl_st);

	si_corereg(sih, SI_CC_IDX, offset, CLKCTL_STS_SECI_CLK_REQ, val);

	if (!enable)
		return;

	SPINWAIT(!(si_corereg(sih, 0, offset, 0, 0) & CLKCTL_STS_SECI_CLK_AVAIL),
	    PMU_MAX_TRANSITION_DLY);

	clk_ctl_st = si_corereg(sih, 0, offset, 0, 0);
	if (enable) {
		if (!(clk_ctl_st & CLKCTL_STS_SECI_CLK_AVAIL)) {
			SI_ERROR(("SECI clock is still not available\n"));
			return;
		}
	}
}

void
BCMINITFN(si_seci_down)(si_t *sih)
{
	uint32 origidx = 0;
	si_info_t *sii;
	chipcregs_t *cc;
	bool fast;
	uint32 seci_conf;

	if (!si_seci(sih))
		return;

	sii = SI_INFO(sih);
	fast = SI_FAST(sii);

	if (!fast) {
		origidx = sii->curidx;
		if ((cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0)) == NULL)
			return;
	} else if ((cc = (chipcregs_t *)CCREGS_FAST(sii)) == NULL)
		return;
	ASSERT(cc);
	/* 4331 X28 sign off seci */
	if (CHIPID(sih->chip) == BCM4331_CHIP_ID) {
		/* flush seci */
		seci_conf = R_REG(sii->osh, &cc->SECI_config);
		seci_conf |= SECI_UPD_SECI;
		W_REG(sii->osh, &cc->SECI_config, seci_conf);
		SPINWAIT((R_REG(sii->osh, &cc->SECI_config) & SECI_UPD_SECI), 1000);

		/* SECI sign off */
		W_REG(sii->osh, &cc->seci_uart_data, SECI_SIGNOFF_0);
		W_REG(sii->osh, &cc->seci_uart_data, SECI_SIGNOFF_1);
		SPINWAIT((R_REG(sii->osh, &cc->seci_uart_lsr) & (1 << 2)), 1000);
		/* put seci in reset */
		seci_conf = R_REG(sii->osh, &cc->SECI_config);
		seci_conf &= ~SECI_ENAB_SECI_ECI;
		W_REG(sii->osh, &cc->SECI_config, seci_conf);
		seci_conf |= SECI_RESET;
		W_REG(sii->osh, &cc->SECI_config, seci_conf);
	}

	/* bring down the clock if up */
	si_seci_clkreq(sih, FALSE);

	/* restore previous core */
	if (!fast)
		si_setcoreidx(sih, origidx);
}

void
si_seci_upd(si_t *sih, bool enable)
{
	uint32 origidx = 0;
	si_info_t *sii;
	chipcregs_t *cc;
	bool fast;
	uint32 regval;
	uint intr_val = 0;

	if (!si_seci(sih))
		return;

	sii = SI_INFO(sih);
	fast = SI_FAST(sii);
	INTR_OFF(sii, intr_val);
	if (!fast) {
		origidx = sii->curidx;
		if ((cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0)) == NULL)
			goto exit;
	} else if ((cc = (chipcregs_t *)CCREGS_FAST(sii)) == NULL)
		goto exit;

	ASSERT(cc);

	/* 4331 Select SECI based on enable input */
	if (CHIPID(sih->chip) == BCM4331_CHIP_ID) {
		regval = R_REG(sii->osh, &cc->chipcontrol);
		if (enable)
			regval |= CCTRL4331_SECI;
		else
			regval &= ~CCTRL4331_SECI;
		W_REG(sii->osh, &cc->chipcontrol, regval);

		if (enable) {
			/* Send ECI update to BT */
			regval = R_REG(sii->osh, &cc->SECI_config);
			regval |= SECI_UPD_SECI;
			W_REG(sii->osh, &cc->SECI_config, regval);
			SPINWAIT((R_REG(sii->osh, &cc->SECI_config) & SECI_UPD_SECI), 1000);
			/* Request ECI update from BT */
			W_REG(sii->osh, &cc->seci_uart_data, SECI_SLIP_ESC_CHAR);
			W_REG(sii->osh, &cc->seci_uart_data, SECI_REFRESH_REQ);
		}
	}

exit:
	/* restore previous core */
	if (!fast)
		si_setcoreidx(sih, origidx);

	INTR_RESTORE(sii, intr_val);
}

/* SECI Init routine, pass in seci_mode */
void *
BCMINITFN(si_seci_init)(si_t *sih, uint8  seci_mode)
{
	uint32 origidx = 0;
	uint32 offset;
	si_info_t *sii;
	void *ptr;
	chipcregs_t *cc;
	bool fast;
	uint32 seci_conf;
	uint32 regval;

	if (sih->ccrev < 35)
		return NULL;

	if (!si_seci(sih))
		return NULL;

	if (seci_mode > SECI_MODE_MASK)
		return NULL;

	sii = SI_INFO(sih);
	fast = SI_FAST(sii);
	if (!fast) {
		origidx = sii->curidx;
		if ((ptr = si_setcore(sih, CC_CORE_ID, 0)) == NULL)
			return NULL;
	} else if ((ptr = CCREGS_FAST(sii)) == NULL)
		return NULL;
	cc = (chipcregs_t *)ptr;
	ASSERT(cc);


	/* 43236 (ccrev 36) muxes SECI on JTAG pins. Select SECI. */
	if (CHIPID(sih->chip) == BCM43236_CHIP_ID ||
	    CHIPID(sih->chip) == BCM4331_CHIP_ID) {
		regval = R_REG(sii->osh, &cc->chipcontrol);
		regval |= CCTRL4331_SECI;
		W_REG(sii->osh, &cc->chipcontrol, regval);
	}

	/* 43143 (ccrev 43) mux SECI on JTAG pins. Select SECI. */
	if (CHIPID(sih->chip) == BCM43143_CHIP_ID) {
		regval = R_REG(sii->osh, &cc->chipcontrol);
		regval &= ~(CCTRL_43143_SECI | CCTRL_43143_BT_LEGACY);
		switch (seci_mode) {
			case SECI_MODE_LEGACY_3WIRE_WLAN:
				regval |= CCTRL_43143_BT_LEGACY;
				break;
			case SECI_MODE_SECI:
				regval |= CCTRL_43143_SECI;
				break;
			default:
				ASSERT(0);
		}
		W_REG(sii->osh, &cc->chipcontrol, regval);
	}

	if ((CHIPID(sih->chip) == BCM43236_CHIP_ID) ||
	    (CHIPID(sih->chip) == BCM43143_CHIP_ID)) {
		regval = R_REG(sii->osh, &cc->jtagctrl);
		regval |= 0x1;
		W_REG(sii->osh, &cc->jtagctrl, regval);
	}

	/* enable SECI clock */
	si_seci_clkreq(sih, TRUE);

	/* put the SECI in reset */
	seci_conf = R_REG(sii->osh, &cc->SECI_config);
	seci_conf &= ~SECI_ENAB_SECI_ECI;
	W_REG(sii->osh, &cc->SECI_config, seci_conf);
	seci_conf = SECI_RESET;
	W_REG(sii->osh, &cc->SECI_config, seci_conf);

	/* set force-low, and set EN_SECI for all non-legacy modes */
	seci_conf |= SECI_ENAB_SECIOUT_DIS;
	if ((seci_mode == SECI_MODE_UART) || (seci_mode == SECI_MODE_SECI) ||
	    (seci_mode == SECI_MODE_HALF_SECI))
	{
		seci_conf |= SECI_ENAB_SECI_ECI;
	}
	W_REG(sii->osh, &cc->SECI_config, seci_conf);

	/* take seci out of reset */
	seci_conf = R_REG(sii->osh, &cc->SECI_config);
	seci_conf &= ~(SECI_RESET);
	W_REG(sii->osh, &cc->SECI_config, seci_conf);

	/* set UART/SECI baud rate */
	/* hard-coded at 4MBaud for now */
	if ((seci_mode == SECI_MODE_UART) || (seci_mode == SECI_MODE_SECI) ||
	    (seci_mode == SECI_MODE_HALF_SECI)) {
		if ((CHIPID(sih->chip) == BCM43236_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
		    (CHIPID(sih->chip) == BCM43143_CHIP_ID)) {
			/* 43236 ccrev = 36 and MAC clk = 96MHz */
			/* 4331,43143 MAC clk = 96MHz */
			offset = OFFSETOF(chipcregs_t, seci_uart_bauddiv);
			si_corereg(sih, SI_CC_IDX, offset, 0xFF, 0xFF);
			offset = OFFSETOF(chipcregs_t, seci_uart_baudadj);
			si_corereg(sih, SI_CC_IDX, offset, 0xFF, 0x44);
		}
		else if ((CHIPID(sih->chip) == BCM4360_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43460_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM43526_CHIP_ID) ||
			(CHIPID(sih->chip) == BCM4352_CHIP_ID)) {
			/* MAC clk is 160MHz */
			offset = OFFSETOF(chipcregs_t, seci_uart_bauddiv);
			si_corereg(sih, SI_CC_IDX, offset, 0xFF, 0xFE);
			offset = OFFSETOF(chipcregs_t, seci_uart_baudadj);
			si_corereg(sih, SI_CC_IDX, offset, 0xFF, 0x44);
		}
		else {
			/* 4336 MAC clk is 80MHz */
			offset = OFFSETOF(chipcregs_t, seci_uart_bauddiv);
			si_corereg(sih, SI_CC_IDX, offset, 0xFF, 0xFF);
			offset = OFFSETOF(chipcregs_t, seci_uart_baudadj);
			si_corereg(sih, SI_CC_IDX, offset, 0xFF, 0x22);
		}

		/* LCR/MCR settings */
		offset = OFFSETOF(chipcregs_t, seci_uart_lcr);
		si_corereg(sih, SI_CC_IDX, offset, 0xFF,
			(SECI_UART_LCR_RX_EN | SECI_UART_LCR_TXO_EN)); /* 0x28 */
		offset = OFFSETOF(chipcregs_t, seci_uart_mcr);
		si_corereg(sih, SI_CC_IDX, offset,
			0xFF, (SECI_UART_MCR_TX_EN | SECI_UART_MCR_BAUD_ADJ_EN)); /* 0x81 */

		/* Give control of ECI output regs to MAC core */
		offset = OFFSETOF(chipcregs_t, eci.ge35.eci_controllo);
		si_corereg(sih, SI_CC_IDX, offset, 0xFFFFFFFF, ECI_MACCTRLLO_BITS);
		offset = OFFSETOF(chipcregs_t, eci.ge35.eci_controlhi);
		si_corereg(sih, SI_CC_IDX, offset, 0xFFFF, ECI_MACCTRLHI_BITS);
	}

	/* set the seci mode in seci conf register */
	seci_conf = R_REG(sii->osh, &cc->SECI_config);
	seci_conf &= ~(SECI_MODE_MASK << SECI_MODE_SHIFT);
	seci_conf |= (seci_mode << SECI_MODE_SHIFT);
	W_REG(sii->osh, &cc->SECI_config, seci_conf);

	/* Clear force-low bit */
	seci_conf = R_REG(sii->osh, &cc->SECI_config);
	seci_conf &= ~SECI_ENAB_SECIOUT_DIS;
	W_REG(sii->osh, &cc->SECI_config, seci_conf);

	/* restore previous core */
	if (!fast)
		si_setcoreidx(sih, origidx);

	return ptr;
}

void *
BCMINITFN(si_gci_init)(si_t *sih)
{
	if (sih->cccaps_ext & CC_CAP_EXT_GCI_PRESENT)
	{
		si_gci_reset(sih);
		/* Set GCI Control bits 40 - 47 to be SW Controlled. These bits
		contain WL channel info and are sent to BT.
		*/
		si_gci_direct(sih, OFFSETOF(chipcregs_t, gci_control_1),
			GCI_WL_CHN_INFO_MASK, GCI_WL_CHN_INFO_MASK);
	}
	return (NULL);
}
#endif /* BCMECICOEX */

#if (!defined(_CFE_) && !defined(_CFEZ_)) || defined(CFG_WL)
void
si_btcgpiowar(si_t *sih)
{
	si_info_t *sii;
	uint origidx;
	uint intr_val = 0;
	chipcregs_t *cc;

	sii = SI_INFO(sih);

	/* Make sure that there is ChipCommon core present &&
	 * UART_TX is strapped to 1
	 */
	if (!(sih->cccaps & CC_CAP_UARTGPIO))
		return;

	/* si_corereg cannot be used as we have to guarantee 8-bit read/writes */
	INTR_OFF(sii, intr_val);

	origidx = si_coreidx(sih);

	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
	ASSERT(cc != NULL);

	W_REG(sii->osh, &cc->uart0mcr, R_REG(sii->osh, &cc->uart0mcr) | 0x04);

	/* restore the original index */
	si_setcoreidx(sih, origidx);

	INTR_RESTORE(sii, intr_val);
}

void
si_chipcontrl_btshd0_4331(si_t *sih, bool on)
{
	si_info_t *sii;
	chipcregs_t *cc;
	uint origidx;
	uint32 val;
	uint intr_val = 0;

	sii = SI_INFO(sih);

	INTR_OFF(sii, intr_val);

	origidx = si_coreidx(sih);

	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);

	val = R_REG(sii->osh, &cc->chipcontrol);

	/* bt_shd0 controls are same for 4331 chiprevs 0 and 1, packages 12x9 and 12x12 */
	if (on) {
		/* Enable bt_shd0 on gpio4: */
		val |= (CCTRL4331_BT_SHD0_ON_GPIO4);
		W_REG(sii->osh, &cc->chipcontrol, val);
	} else {
		val &= ~(CCTRL4331_BT_SHD0_ON_GPIO4);
		W_REG(sii->osh, &cc->chipcontrol, val);
	}

	/* restore the original index */
	si_setcoreidx(sih, origidx);

	INTR_RESTORE(sii, intr_val);
}

void
si_chipcontrl_restore(si_t *sih, uint32 val)
{
	si_info_t *sii;
	chipcregs_t *cc;
	uint origidx;

	sii = SI_INFO(sih);
	origidx = si_coreidx(sih);
	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
	W_REG(sii->osh, &cc->chipcontrol, val);
	si_setcoreidx(sih, origidx);
}

uint32
si_chipcontrl_read(si_t *sih)
{
	si_info_t *sii;
	chipcregs_t *cc;
	uint origidx;
	uint32 val;

	sii = SI_INFO(sih);
	origidx = si_coreidx(sih);
	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
	val = R_REG(sii->osh, &cc->chipcontrol);
	si_setcoreidx(sih, origidx);
	return val;
}

void
si_chipcontrl_epa4331(si_t *sih, bool on)
{
	si_info_t *sii;
	chipcregs_t *cc;
	uint origidx;
	uint32 val;

	sii = SI_INFO(sih);
	origidx = si_coreidx(sih);

	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);

	val = R_REG(sii->osh, &cc->chipcontrol);

	if (on) {
		if (sih->chippkg == 9 || sih->chippkg == 0xb) {
			val |= (CCTRL4331_EXTPA_EN | CCTRL4331_EXTPA_ON_GPIO2_5);
			/* Ext PA Controls for 4331 12x9 Package */
			W_REG(sii->osh, &cc->chipcontrol, val);
		} else {
			/* Ext PA Controls for 4331 12x12 Package */
			if (sih->chiprev > 0) {
				W_REG(sii->osh, &cc->chipcontrol, val |
				      (CCTRL4331_EXTPA_EN) | (CCTRL4331_EXTPA_EN2));
			} else {
				W_REG(sii->osh, &cc->chipcontrol, val | (CCTRL4331_EXTPA_EN));
			}
		}
	} else {
		val &= ~(CCTRL4331_EXTPA_EN | CCTRL4331_EXTPA_EN2 | CCTRL4331_EXTPA_ON_GPIO2_5);
		W_REG(sii->osh, &cc->chipcontrol, val);
	}

	si_setcoreidx(sih, origidx);
}

/* switch muxed pins, on: SROM, off: FEMCTRL */
void
si_chipcontrl_srom4360(si_t *sih, bool on)
{
	si_info_t *sii;
	chipcregs_t *cc;
	uint origidx;
	uint32 val;

	sii = SI_INFO(sih);
	origidx = si_coreidx(sih);

	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);

	val = R_REG(sii->osh, &cc->chipcontrol);

	if (on) {
		val &= ~(CCTRL4360_SECI_MODE |
			CCTRL4360_BTSWCTRL_MODE |
			CCTRL4360_EXTRA_FEMCTRL_MODE |
			CCTRL4360_BT_LGCY_MODE |
			CCTRL4360_CORE2FEMCTRL4_ON);

		W_REG(sii->osh, &cc->chipcontrol, val);
	} else {
	}

	si_setcoreidx(sih, origidx);
}

void
si_chipcontrl_epa4331_wowl(si_t *sih, bool enter_wowl)
{
	si_info_t *sii;
	chipcregs_t *cc;
	uint origidx;
	uint32 val;
	bool sel_chip;

	sel_chip = (CHIPID(sih->chip) == BCM4331_CHIP_ID) ||
		(CHIPID(sih->chip) == BCM43431_CHIP_ID);
	sel_chip &= ((sih->chippkg == 9 || sih->chippkg == 0xb));

	if (!sel_chip)
		return;

	sii = SI_INFO(sih);
	origidx = si_coreidx(sih);

	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);

	val = R_REG(sii->osh, &cc->chipcontrol);

	if (enter_wowl) {
		val |= CCTRL4331_EXTPA_EN;
		W_REG(sii->osh, &cc->chipcontrol, val);
	} else {
		val |= (CCTRL4331_EXTPA_EN | CCTRL4331_EXTPA_ON_GPIO2_5);
		W_REG(sii->osh, &cc->chipcontrol, val);
	}
	si_setcoreidx(sih, origidx);
}
#endif /* (!_CFE_ && !_CFEZ_) || CFG_WL */

uint
si_pll_reset(si_t *sih)
{
	uint err = 0;

	uint intr_val = 0;
	si_info_t *sii;
	sii = SI_INFO(sih);
	INTR_OFF(sii, intr_val);
	err = si_pll_minresmask_reset(sih, sii->osh);
	INTR_RESTORE(sii, intr_val);
	return (err);
}

/* Enable BT-COEX & Ex-PA for 4313 */
void
si_epa_4313war(si_t *sih)
{
	si_info_t *sii;
	chipcregs_t *cc;
	uint origidx;

	sii = SI_INFO(sih);
	origidx = si_coreidx(sih);

	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);

	/* EPA Fix */
	W_REG(sii->osh, &cc->gpiocontrol,
		R_REG(sii->osh, &cc->gpiocontrol) | GPIO_CTRL_EPA_EN_MASK);

	si_setcoreidx(sih, origidx);
}

void
si_clk_pmu_htavail_set(si_t *sih, bool set_clear)
{
	si_info_t *sii;
	sii = SI_INFO(sih);

	si_pmu_minresmask_htavail_set(sih, sii->osh, set_clear);
}

/* Re-enable synth_pwrsw resource in min_res_mask for 4313 */
void
si_pmu_synth_pwrsw_4313_war(si_t *sih)
{
	si_info_t *sii;
	chipcregs_t *cc;
	uint origidx;

	sii = SI_INFO(sih);
	origidx = si_coreidx(sih);

	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
	if (!(cc->min_res_mask & PMURES_BIT(RES4313_SYNTH_PWRSW_RSRC)))
		OR_REG(sii->osh, &cc->min_res_mask, PMURES_BIT(RES4313_SYNTH_PWRSW_RSRC));

	si_setcoreidx(sih, origidx);
}

/* WL/BT control for 4313 btcombo boards >= P250 */
void
si_btcombo_p250_4313_war(si_t *sih)
{
	si_info_t *sii;
	chipcregs_t *cc;
	uint origidx;

	sii = SI_INFO(sih);
	origidx = si_coreidx(sih);

	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);
	W_REG(sii->osh, &cc->gpiocontrol,
		R_REG(sii->osh, &cc->gpiocontrol) | GPIO_CTRL_5_6_EN_MASK);

	W_REG(sii->osh, &cc->gpioouten,
		R_REG(sii->osh, &cc->gpioouten) | GPIO_CTRL_5_6_EN_MASK);

	si_setcoreidx(sih, origidx);
}
void
si_btc_enable_chipcontrol(si_t *sih)
{
	si_info_t *sii;
	chipcregs_t *cc;
	uint origidx;

	sii = SI_INFO(sih);
	origidx = si_coreidx(sih);

	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);

	/* BT fix */
	W_REG(sii->osh, &cc->chipcontrol,
		R_REG(sii->osh, &cc->chipcontrol) | CC_BTCOEX_EN_MASK);

	si_setcoreidx(sih, origidx);
}
void
si_btcombo_43228_war(si_t *sih)
{
	si_info_t *sii;
	chipcregs_t *cc;
	uint origidx;

	sii = SI_INFO(sih);
	origidx = si_coreidx(sih);

	cc = (chipcregs_t *)si_setcore(sih, CC_CORE_ID, 0);

	W_REG(sii->osh, &cc->gpioouten, GPIO_CTRL_7_6_EN_MASK);
	W_REG(sii->osh, &cc->gpioout, GPIO_OUT_7_EN_MASK);

	si_setcoreidx(sih, origidx);
}

/* check if the device is removed */
bool
si_deviceremoved(si_t *sih)
{
	uint32 w;
	si_info_t *sii;

	sii = SI_INFO(sih);

	switch (BUSTYPE(sih->bustype)) {
	case PCI_BUS:
		ASSERT(sii->osh != NULL);
		w = OSL_PCI_READ_CONFIG(sii->osh, PCI_CFG_VID, sizeof(uint32));
		if ((w & 0xFFFF) != VENDOR_BROADCOM)
			return TRUE;
		break;
	}
	return FALSE;
}

bool
si_is_sprom_available(si_t *sih)
{
	if (sih->ccrev >= 31) {
		si_info_t *sii;
		uint origidx;
		chipcregs_t *cc;
		uint32 sromctrl;

		if ((sih->cccaps & CC_CAP_SROM) == 0)
			return FALSE;

		sii = SI_INFO(sih);
		origidx = sii->curidx;
		cc = si_setcoreidx(sih, SI_CC_IDX);
		sromctrl = R_REG(sii->osh, &cc->sromcontrol);
		si_setcoreidx(sih, origidx);
		return (sromctrl & SRC_PRESENT);
	}

	switch (CHIPID(sih->chip)) {
	case BCM4312_CHIP_ID:
		return ((sih->chipst & CST4312_SPROM_OTP_SEL_MASK) != CST4312_OTP_SEL);
	case BCM4325_CHIP_ID:
		return (sih->chipst & CST4325_SPROM_SEL) != 0;
	case BCM4322_CHIP_ID:	case BCM43221_CHIP_ID:	case BCM43231_CHIP_ID:
	case BCM43222_CHIP_ID:	case BCM43111_CHIP_ID:	case BCM43112_CHIP_ID:
	case BCM4342_CHIP_ID: {
		uint32 spromotp;
		spromotp = (sih->chipst & CST4322_SPROM_OTP_SEL_MASK) >>
		        CST4322_SPROM_OTP_SEL_SHIFT;
		return (spromotp & CST4322_SPROM_PRESENT) != 0;
	}
	case BCM4329_CHIP_ID:
		return (sih->chipst & CST4329_SPROM_SEL) != 0;
	case BCM4315_CHIP_ID:
		return (sih->chipst & CST4315_SPROM_SEL) != 0;
	case BCM4319_CHIP_ID:
		return (sih->chipst & CST4319_SPROM_SEL) != 0;
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
		return (sih->chipst & CST4336_SPROM_PRESENT) != 0;
	case BCM4330_CHIP_ID:
		return (sih->chipst & CST4330_SPROM_PRESENT) != 0;
	case BCM4313_CHIP_ID:
		return (sih->chipst & CST4313_SPROM_PRESENT) != 0;
	case BCM4331_CHIP_ID:
	case BCM43431_CHIP_ID:
		return (sih->chipst & CST4331_SPROM_PRESENT) != 0;
	case BCM43239_CHIP_ID:
		return ((sih->chipst & CST43239_SPROM_MASK) &&
			!(sih->chipst & CST43239_SFLASH_MASK));
	case BCM4324_CHIP_ID:
	case BCM43242_CHIP_ID:
		return ((sih->chipst & CST4324_SPROM_MASK) &&
			!(sih->chipst & CST4324_SFLASH_MASK));
	case BCM4335_CHIP_ID:
		return ((sih->chipst & CST4335_SPROM_MASK) &&
			!(sih->chipst & CST4335_SFLASH_MASK));

	case BCM4350_CHIP_ID:
		return (sih->chipst & CST4350_SPROM_PRESENT) != 0;

	case BCM43131_CHIP_ID:
	case BCM43217_CHIP_ID:
	case BCM43227_CHIP_ID:
	case BCM43228_CHIP_ID:
	case BCM43428_CHIP_ID:
		return (sih->chipst & CST43228_OTP_PRESENT) != CST43228_OTP_PRESENT;
	default:
		return TRUE;
	}
}

bool
si_is_otp_disabled(si_t *sih)
{
	switch (CHIPID(sih->chip)) {
	case BCM4325_CHIP_ID:
		return (sih->chipst & CST4325_SPROM_OTP_SEL_MASK) == CST4325_OTP_PWRDN;
	case BCM4322_CHIP_ID:
	case BCM43221_CHIP_ID:
	case BCM43231_CHIP_ID:
	case BCM4342_CHIP_ID:
		return (((sih->chipst & CST4322_SPROM_OTP_SEL_MASK) >>
			CST4322_SPROM_OTP_SEL_SHIFT) & CST4322_OTP_PRESENT) !=
			CST4322_OTP_PRESENT;
	case BCM4329_CHIP_ID:
		return (sih->chipst & CST4329_SPROM_OTP_SEL_MASK) == CST4329_OTP_PWRDN;
	case BCM4315_CHIP_ID:
		return (sih->chipst & CST4315_SPROM_OTP_SEL_MASK) == CST4315_OTP_PWRDN;
	case BCM4319_CHIP_ID:
		return (sih->chipst & CST4319_SPROM_OTP_SEL_MASK) == CST4319_OTP_PWRDN;
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID:
		return ((sih->chipst & CST4336_OTP_PRESENT) == 0);
	case BCM4330_CHIP_ID:
		return ((sih->chipst & CST4330_OTP_PRESENT) == 0);
	case BCM43237_CHIP_ID:
		return FALSE;
	case BCM4313_CHIP_ID:
		return (sih->chipst & CST4313_OTP_PRESENT) == 0;
	case BCM4331_CHIP_ID:
		return (sih->chipst & CST4331_OTP_PRESENT) != CST4331_OTP_PRESENT;
	case BCM4360_CHIP_ID:
	case BCM43526_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM4352_CHIP_ID:
		/* 4360 OTP is always powered and enabled */
		return FALSE;
	/* These chips always have their OTP on */
	case BCM43111_CHIP_ID:	case BCM43112_CHIP_ID:	case BCM43222_CHIP_ID:
	case BCM43224_CHIP_ID:	case BCM43225_CHIP_ID:
	case BCM43421_CHIP_ID:
	case BCM43226_CHIP_ID:
	case BCM43235_CHIP_ID:	case BCM43236_CHIP_ID:	case BCM43238_CHIP_ID:
	case BCM43234_CHIP_ID:	case BCM43239_CHIP_ID:	case BCM4324_CHIP_ID:
	case BCM43431_CHIP_ID:
	case BCM43131_CHIP_ID:
	case BCM43217_CHIP_ID:
	case BCM43227_CHIP_ID:
	case BCM43228_CHIP_ID:
	case BCM43428_CHIP_ID: case BCM4335_CHIP_ID:
	case BCM43143_CHIP_ID:
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
	default:
		return FALSE;
	}
}

bool
si_is_otp_powered(si_t *sih)
{
	if (PMUCTL_ENAB(sih))
		return si_pmu_is_otp_powered(sih, si_osh(sih));
	return TRUE;
}

void
si_otp_power(si_t *sih, bool on)
{
	if (PMUCTL_ENAB(sih))
		si_pmu_otp_power(sih, si_osh(sih), on);
	OSL_DELAY(1000);
}

bool
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_ERR)
si_is_sprom_enabled(si_t *sih)
#else
BCMATTACHFN(si_is_sprom_enabled)(si_t *sih)
#endif
{
	if (PMUCTL_ENAB(sih))
		return si_pmu_is_sprom_enabled(sih, si_osh(sih));
	return TRUE;
}

void
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_ERR)
si_sprom_enable(si_t *sih, bool enable)
#else
BCMATTACHFN(si_sprom_enable)(si_t *sih, bool enable)
#endif
{
	if (PMUCTL_ENAB(sih))
		si_pmu_sprom_enable(sih, si_osh(sih), enable);
}

/* Return BCME_NOTFOUND if the card doesn't have CIS format nvram */
int
si_cis_source(si_t *sih)
{
	/* Many chips have the same mapping of their chipstatus field */
	static const uint cis_sel[] = { CIS_DEFAULT, CIS_SROM, CIS_OTP, CIS_SROM };
	static const uint cis_43236_sel[] = { CIS_DEFAULT, CIS_OTP };

	/* PCI chips use SROM format instead of CIS */
	if (BUSTYPE(sih->bustype) == PCI_BUS)
		return BCME_NOTFOUND;

	switch (CHIPID(sih->chip)) {
	case BCM4325_CHIP_ID:
		return ((sih->chipst & CST4325_SPROM_OTP_SEL_MASK) >= ARRAYSIZE(cis_sel)) ?
		        CIS_DEFAULT : cis_sel[(sih->chipst & CST4325_SPROM_OTP_SEL_MASK)];
	case BCM4322_CHIP_ID:	case BCM43221_CHIP_ID:	case BCM43231_CHIP_ID:
	case BCM4342_CHIP_ID: {
		uint8 strap = (sih->chipst & CST4322_SPROM_OTP_SEL_MASK) >>
			CST4322_SPROM_OTP_SEL_SHIFT;

		return ((strap >= ARRAYSIZE(cis_sel)) ?  CIS_DEFAULT : cis_sel[strap]);

	}

	case BCM43235_CHIP_ID:	case BCM43236_CHIP_ID:	case BCM43238_CHIP_ID:
	case BCM43234_CHIP_ID: {
		uint8 strap = (sih->chipst & CST43236_OTP_SEL_MASK) >>
			CST43236_OTP_SEL_SHIFT;
		return ((strap >= ARRAYSIZE(cis_43236_sel)) ?  CIS_DEFAULT : cis_43236_sel[strap]);
	}

	case BCM4329_CHIP_ID:
		return ((sih->chipst & CST4329_SPROM_OTP_SEL_MASK) >= ARRAYSIZE(cis_sel)) ?
		        CIS_DEFAULT : cis_sel[(sih->chipst & CST4329_SPROM_OTP_SEL_MASK)];

	case BCM4315_CHIP_ID:

		return ((sih->chipst & CST4315_SPROM_OTP_SEL_MASK) >= ARRAYSIZE(cis_sel)) ?
		        CIS_DEFAULT : cis_sel[(sih->chipst & CST4315_SPROM_OTP_SEL_MASK)];

	case BCM4319_CHIP_ID: {
		uint cis_sel4319 = ((sih->chipst & CST4319_SPROM_OTP_SEL_MASK) >>
		                    CST4319_SPROM_OTP_SEL_SHIFT);
		return (cis_sel4319 >= ARRAYSIZE(cis_sel)) ? CIS_DEFAULT : cis_sel[cis_sel4319];
	}
	case BCM4336_CHIP_ID:
	case BCM43362_CHIP_ID: {
		if (sih->chipst & CST4336_SPROM_PRESENT)
			return CIS_SROM;
		if (sih->chipst & CST4336_OTP_PRESENT)
			return CIS_OTP;
		return CIS_DEFAULT;
	}
	case BCM4330_CHIP_ID: {
		if (sih->chipst & CST4330_SPROM_PRESENT)
			return CIS_SROM;
		if (sih->chipst & CST4330_OTP_PRESENT)
			return CIS_OTP;
		return CIS_DEFAULT;
	}
	case BCM43239_CHIP_ID: {
		if ((sih->chipst & CST43239_SPROM_MASK) && !(sih->chipst & CST43239_SFLASH_MASK))
			return CIS_SROM;
		return CIS_OTP;
	}
	case BCM4324_CHIP_ID:
	{
		if ((sih->chipst & CST4324_SPROM_MASK) && !(sih->chipst & CST4324_SFLASH_MASK))
			return CIS_SROM;
		return CIS_OTP;
	}
	case BCM4335_CHIP_ID:
	{
		if ((sih->chipst & CST4335_SPROM_MASK) && !(sih->chipst & CST4335_SFLASH_MASK))
			return CIS_SROM;
		return CIS_OTP;
	}
	case BCM43237_CHIP_ID: {
		uint8 strap = (sih->chipst & CST43237_OTP_SEL_MASK) >>
			CST43237_OTP_SEL_SHIFT;
		return ((strap >= ARRAYSIZE(cis_43236_sel)) ?  CIS_DEFAULT : cis_43236_sel[strap]);
	}
	case BCM43143_CHIP_ID: {
		return CIS_OTP; /* BCM43143 does not support SROM nor OTP strappings */
	}
	case BCM43242_CHIP_ID:
	case BCM43243_CHIP_ID:
	{
		return CIS_OTP; /* BCM43242 does not support SPROM */
	}
	case BCM4360_CHIP_ID:
	case BCM43460_CHIP_ID:
	case BCM4352_CHIP_ID:
	case BCM43526_CHIP_ID: {
		if ((sih->chipst & CST4360_OTP_ENABLED))
			return CIS_OTP;
		return CIS_DEFAULT;
	}
	default:
		return CIS_DEFAULT;
	}
}

/* Read/write to OTP to find the FAB manf */
int
BCMINITFN(si_otp_fabid)(si_t *sih, uint16 *fabid, bool rw)
{
	int error = BCME_OK;
	uint offset = 0;
	uint16 data, mask = 0, shift = 0;

	switch (CHIPID(sih->chip)) {
	case BCM4329_CHIP_ID:
		/* Bit locations 133-130 */
		if (sih->chiprev >= 3) {
			offset = 8;
			mask = 0x3c;
			shift = 2;
		}
		break;
	case BCM43362_CHIP_ID:
		/* Bit locations 134-130 */
		offset = 8;
		mask = 0x7c;
		shift = 2;
		break;
	case BCM5356_CHIP_ID:
		/* Bit locations 133-130 */
		offset = 8;
		mask = 0x3c;
		shift = 2;
		break;
	default:
		error = BCME_EPERM;
		return error;
	}

	if (rw == TRUE) {
		/* TRUE --> read */
		error = otp_read_word(sih, offset, &data);
		if (!error)
			*fabid = (data & mask) >> shift;
	} else {
		data = *fabid;
		data = (data << shift) & mask;
#ifdef BCMNVRAMW
		error = otp_write_word(sih, offset, data);
#endif /* BCMNVRAMW */
	}

	return error;
}
uint16 BCMINITFN(si_fabid)(si_t *sih)
{
	uint32 data;
	uint16 fabid = 0;

	switch (CHIPID(sih->chip)) {
		case BCM4329_CHIP_ID:
		case BCM43362_CHIP_ID:
		case BCM5356_CHIP_ID:
			if (si_otp_fabid(sih, &fabid, TRUE) != BCME_OK)
			{
				SI_ERROR(("si_fabid: reading fabid from otp failed.\n"));
			}
			break;

		case BCM4330_CHIP_ID:
			data = si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol),
				0, 0);
			fabid = ((data & 0xc0000000) >> 30)+((data & 0x20000000) >> 27);
			break;

		case BCM4334_CHIP_ID:
			data = si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, fabid),	0, 0);
			fabid = data & 0xf;
			break;

		case BCM4324_CHIP_ID:
		case BCM4335_CHIP_ID:
		case BCM43143_CHIP_ID:
		case BCM43242_CHIP_ID:
		case BCM43243_CHIP_ID:
			/* intentional fallthrough */
			data = si_corereg(sih, SI_CC_IDX, OFFSETOF(chipcregs_t, fabid),	0, 0);
			fabid = data & 0xf;
			break;

		default:
			break;
	}

	return fabid;
}

uint32 si_get_sromctl(si_t *sih)
{
	chipcregs_t *cc;
	uint origidx;
	uint32 sromctl;
	osl_t *osh;

	osh = si_osh(sih);
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT((uintptr)cc);

	sromctl = R_REG(osh, &cc->sromcontrol);

	/* return to the original core */
	si_setcoreidx(sih, origidx);
	return sromctl;
}

int si_set_sromctl(si_t *sih, uint32 value)
{
	chipcregs_t *cc;
	uint origidx;
	osl_t *osh;

	osh = si_osh(sih);
	origidx = si_coreidx(sih);
	cc = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT((uintptr)cc);

	/* get chipcommon rev */
	if (si_corerev(sih) < 32)
		return BCME_UNSUPPORTED;

	W_REG(osh, &cc->sromcontrol, value);

	/* return to the original core */
	si_setcoreidx(sih, origidx);
	return BCME_OK;

}

uint
si_core_wrapperreg(si_t *sih, uint32 coreidx, uint32 offset, uint32 mask, uint32 val)
{
	uint origidx;
	uint ret_val;

	origidx = si_coreidx(sih);

	si_setcoreidx(sih, coreidx);

	ret_val = si_wrapperreg(sih, offset, mask, val);

	/* return to the original core */
	si_setcoreidx(sih, origidx);
	return ret_val;
}

#ifdef WLC_LOW
/* To make sure that, res mask is minimal to save power and also, to indicate
* specifically for 4335 host about the SR logic.
*/
void
si_update_masks(si_t *sih)
{
	/* set min res mask */
	si_ccreg(sih, MINRESMASKREG, ~0, 0x1);

	/* Moved from ucode to driver for 4335 */
	si_ccreg(sih, PMUREG_RESREQ_MASK, ~0, 0x7ffbfff);

	/* set_sdio_aos_wakeup_mask */
	si_ccreg(sih, CHIPCTRLADDR, ~0, CHIPCTRLREG2);
	si_ccreg(sih, CHIPCTRLDATA, 0x01000000, 0x01000000);
}

void
si_force_islanding(si_t *sih, bool enable)
{
	si_ccreg(sih, CHIPCTRLADDR, ~0, CHIPCTRLREG2);

	if (enable) {
		/* Turn on the islands */
		si_ccreg(sih, CHIPCTRLDATA, 0x1c0000, 0x0);
	} else {
		/* Turn off the islands */
		si_ccreg(sih, CHIPCTRLDATA, 0x03c0000, 0x03c0000);
	}
}
#endif /* WLC_LOW */
