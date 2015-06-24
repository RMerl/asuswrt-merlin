/*
 * Broadcom Gigabit Ethernet MAC (Unimac) core.
 *
 * This file implements the chip-specific routines for the GMAC core.
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
 * $Id: etcgmac.c 523524 2014-12-31 02:48:08Z $
 */

#include <et_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <bcmdefs.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <bcmenetphy.h>
#include <proto/ethernet.h>
#include <proto/802.1d.h>
#include <siutils.h>
#include <sbhnddma.h>
#include <sbchipc.h>
#include <hnddma.h>
#include <et_dbg.h>
#include <hndsoc.h>
#include <hndpmu.h>
#include <bcmgmacmib.h>
#include <gmac_common.h>
#include <gmac_core.h>
#include <et_export.h>		/* for et_phyxx() routines */
#include <etcgmac.h>
#include <bcmenetrxh.h>
#include <bcmgmacrxh.h>

#ifdef ETROBO
#include <bcmrobo.h>
#endif /* ETROBO */
#ifdef ETADM
#include <etc_adm.h>
#endif /* ETADM */
#ifdef ETFA
#include <etc_fa.h>
#endif /* ETFA */
#include <hndfwd.h> /* BCM_GMAC3 */


struct bcmgmac;	/* forward declaration */
#define ch_t	struct bcmgmac
#include <etc.h>

#define GMAC_NS_COREREV(rev) ((rev == 4) || (rev == 5) || (rev == 7))

/* private chip state */
struct bcmgmac {
	void 		*et;		/* pointer to et private state */
	etc_info_t	*etc;		/* pointer to etc public state */

	gmac_commonregs_t *regscomm; /* pointer to GMAC COMMON registers */
	gmacregs_t	*regs;		/* pointer to chip registers */
	osl_t 		*osh;		/* os handle */

	void 		*etphy;		/* pointer to et for shared mdc/mdio contortion */

	uint32		intstatus;	/* saved interrupt condition bits */
	uint32		intmask;	/* current software interrupt mask */
	uint32		def_intmask;	/* default interrupt mask */

	hnddma_t	*di[NUMTXQ];	/* dma engine software state */

	bool		mibgood;	/* true once mib registers have been cleared */
	gmacmib_t	mib;		/* mib statistic counters */
	si_t 		*sih;		/* si utils handle */

	char		*vars;		/* sprom name=value */
	uint		vars_size;

	void		*adm;		/* optional admtek private data */
	mcfilter_t	mf;		/* multicast filter */
};

/* local prototypes */
static bool chipid(uint vendor, uint device);
static void *chipattach(etc_info_t *etc, void *osh, void *regsva);
static void chipdetach(ch_t *ch);
static void chipreset(ch_t *ch);
static void chipinit(ch_t *ch, uint options);
static bool chiptx(ch_t *ch, void *p);
static void *chiprx(ch_t *ch);
static int  chiprxquota(ch_t *ch, int quota, void **rxpkts);
static void chiprxlazy(ch_t *ch);
static void chiprxfill(ch_t *ch);
static int chipgetintrevents(ch_t *ch, bool in_isr);
static bool chiperrors(ch_t *ch);
static bool chipdmaerrors(ch_t *ch);
static void chipintrson(ch_t *ch);
static void chipintrsoff(ch_t *ch);
static void chiptxreclaim(ch_t *ch, bool all);
static void chiprxreclaim(ch_t *ch);
static uint chipactiverxbuf(ch_t *ch);
static void chipstatsupd(ch_t *ch);
static void chipdumpmib(ch_t *ch, struct bcmstrbuf *b, bool clear);
static void chipenablepme(ch_t *ch);
static void chipdisablepme(ch_t *ch);
static void chipconfigtimer(ch_t *ch, uint microsecs);
static void chipphyreset(ch_t *ch, uint phyaddr);
static uint16 chipphyrd(ch_t *ch, uint phyaddr, uint reg);
static void chipphywr(ch_t *ch, uint phyaddr, uint reg, uint16 v);
static uint chipmacrd(ch_t *ch, uint reg);
static void chipmacwr(ch_t *ch, uint reg, uint val);
static void chipdump(ch_t *ch, struct bcmstrbuf *b);
static void chiplongname(ch_t *ch, char *buf, uint bufsize);
static void chipduplexupd(ch_t *ch);

static void chipphyinit(ch_t *ch, uint phyaddr);
static void chipphyor(ch_t *ch, uint phyaddr, uint reg, uint16 v);
static void chipphyforce(ch_t *ch, uint phyaddr);
static void chipphyadvertise(ch_t *ch, uint phyaddr);
#ifdef BCMDBG
static void chipdumpregs(ch_t *ch, gmacregs_t *regs, struct bcmstrbuf *b);
#endif /* BCMDBG */
static void chipunitmap(uint coreunit, uint *unit);
static void gmac_mf_cleanup(ch_t *ch);
static int gmac_speed(ch_t *ch, uint32 speed);
static void gmac_miiconfig(ch_t *ch);

struct chops bcmgmac_et_chops = {
	chipid,
	chipattach,
	chipdetach,
	chipreset,
	chipinit,
	chiptx,
	chiprx,
	chiprxquota,
	chiprxlazy,
	chiprxfill,
	chipgetintrevents,
	chiperrors,
	chipdmaerrors,
	chipintrson,
	chipintrsoff,
	chiptxreclaim,
	chiprxreclaim,
	chipstatsupd,
	chipdumpmib,
	chipenablepme,
	chipdisablepme,
	chipconfigtimer,
	chipphyreset,
	chipphyrd,
	chipphywr,
	chipmacrd,
	chipmacwr,
	chipdump,
	chiplongname,
	chipduplexupd,
	chipunitmap,
	chipactiverxbuf
};

static uint devices[] = {
	BCM47XX_GMAC_ID,
	BCM4716_CHIP_ID,
	BCM4748_CHIP_ID,
	0x0000
};

static bool
chipid(uint vendor, uint device)
{
	int i;

	if (vendor != VENDOR_BROADCOM)
		return (FALSE);

	for (i = 0; devices[i]; i++) {
		if (device == devices[i])
			return (TRUE);
	}

	return (FALSE);
}

static void *
chipattach(etc_info_t *etc, void *osh, void *regsva)
{
	ch_t *ch;
	gmacregs_t *regs;
	uint i;
	char name[16];
	char *var;
	uint boardflags, boardtype, reset;
	uint32 flagbits = 0;

	ET_TRACE(("et%d: chipattach: regsva 0x%lx\n", etc->unit, (ulong)regsva));

	if ((ch = (ch_t *)MALLOC(osh, sizeof(ch_t))) == NULL) {
		ET_ERROR(("et%d: chipattach: out of memory, malloced %d bytes\n", etc->unit,
		          MALLOCED(osh)));
		return (NULL);
	}
	bzero((char *)ch, sizeof(ch_t));

	ch->etc = etc;
	ch->et = etc->et;
	ch->osh = osh;

	/* store the pointer to the sw mib */
	etc->mib = (void *)&ch->mib;

	/* get si handle */
	if ((ch->sih = si_attach(etc->deviceid, ch->osh, regsva, PCI_BUS, NULL, &ch->vars,
	                         &ch->vars_size)) == NULL) {
		ET_ERROR(("et%d: chipattach: si_attach error\n", etc->unit));
		goto fail;
	}

	if (si_corerev(ch->sih) == GMAC_4706B0_CORE_REV) {
		etc->corerev = GMAC_4706B0_CORE_REV;
		if ((ch->regscomm = (gmac_commonregs_t *)si_setcore(ch->sih,
		    GMAC_COMMON_4706_CORE_ID, 0)) == NULL) {
			ET_ERROR(("et%d: chipattach: Could not setcore to GMAC common\n",
				etc->unit));
			goto fail;
		}
	}

	if ((regs = (gmacregs_t *)si_setcore(ch->sih, GMAC_CORE_ID, etc->coreunit)) == NULL) {
		ET_ERROR(("et%d: chipattach: Could not setcore to GMAC\n", etc->unit));
		goto fail;
	}
	if (etc->corerev != GMAC_4706B0_CORE_REV)
		etc->corerev = si_corerev(ch->sih);

	ch->regs = regs;
	etc->chip = ch->sih->chip;
	etc->chiprev = ch->sih->chiprev;
	etc->chippkg = ch->sih->chippkg;
	etc->coreid = si_coreid(ch->sih);
	etc->nicmode = !(ch->sih->bustype == SI_BUS);
	etc->gmac_fwd = FALSE;  /* BCM_GMAC3 */
	etc->boardflags = getintvar(ch->vars, "boardflags");

	boardflags = etc->boardflags;
	boardtype = ch->sih->boardtype;

	/* Backplane clock ticks per microsecs: used by gptimer, intrecvlazy */
	etc->bp_ticks_usec = si_clock(ch->sih) / 1000000;

#ifdef PKTC
	etc->pktc = (getintvar(ch->vars, "pktc_disable") == 0) &&
		(getintvar(ch->vars, "ctf_disable") == 0);
#endif

	/* get our local ether addr */
#ifdef ETFA
	var = fa_get_macaddr(ch->sih, ch->vars, etc->unit);
#else
	sprintf(name, "et%dmacaddr", etc->unit);
	var = getvar(ch->vars, name);
#endif /* ETFA */
	if (var == NULL) {
		ET_ERROR(("et%d: chipattach: getvar(%s) not found\n", etc->unit, name));
		goto fail;
	}
	bcm_ether_atoe(var, &etc->perm_etheraddr);

#if defined(BCM_GMAC3)
	/*
	 * Select GMAC mode of operation:
	 * If a valid MAC address is present, it operates as an Ethernet Network
	 * interface, otherwise it operates as a forwarding GMAC interface.
	 */
	if (ETHER_ISNULLADDR(&etc->perm_etheraddr)) {
		etc->gmac_fwd = TRUE;
	}
#else  /* ! BCM_GMAC3 */
	if (ETHER_ISNULLADDR(&etc->perm_etheraddr)) {
		ET_ERROR(("et%d: chipattach: invalid format: %s=%s\n", etc->unit, name, var));
		goto fail;
	}
#endif /* ! BCM_GMAC3 */

	bcopy((char *)&etc->perm_etheraddr, (char *)&etc->cur_etheraddr, ETHER_ADDR_LEN);

	/*
	 * Too much can go wrong in scanning MDC/MDIO playing "whos my phy?" .
	 * Instead, explicitly require the environment var "et<unit>phyaddr=<val>".
	 */
	if (BCM4707_CHIP(CHIPID(ch->sih->chip))) {
		etc->phyaddr = EPHY_NOREG;
	}
	else {
		sprintf(name, "et%dphyaddr", etc->unit);
		var = getvar(ch->vars, name);
		if (var == NULL) {
			ET_ERROR(("et%d: chipattach: getvar(%s) not found\n", etc->unit, name));
			goto fail;
		}
		etc->phyaddr = bcm_atoi(var) & EPHY_MASK;
	}

	/* nvram says no phy is present */
	if (etc->phyaddr == EPHY_NONE) {
		ET_ERROR(("et%d: chipattach: phy not present\n", etc->unit));
		goto fail;
	}

	/* configure pci core */
	si_pci_setup(ch->sih, (1 << si_coreidx(ch->sih)));

	/* Northstar, take all GMAC cores out of reset */
	if (BCM4707_CHIP(CHIPID(ch->sih->chip))) {
		int ns_gmac;
		for (ns_gmac = 0; ns_gmac < MAX_GMAC_CORES_4707; ns_gmac++) {
			/* As northstar requirement, we have to reset all GAMCs before
			 * accessing them. et_probe() call pci_enable_device() for etx
			 * and do si_core_reset for GAMCx only.	 Then the other three
			 * GAMCs didn't reset.  We do it here.
			 */
			si_setcore(ch->sih, GMAC_CORE_ID, ns_gmac);
			if (!si_iscoreup(ch->sih)) {
				ET_TRACE(("et%d: reset GMAC[%d] core\n", etc->unit, ns_gmac));
				si_core_reset(ch->sih, flagbits, 0);
			}
		}
		si_setcore(ch->sih, GMAC_CORE_ID, etc->coreunit);
	}
	/* reset the gmac core */
	chipreset(ch);

	/* dma attach */
	sprintf(name, "etc%d", etc->coreunit);

	/* allocate dma resources for txqs */
	/* TX: TC_BK, RX: RX_Q0 */
	ch->di[0] = dma_attach(osh, name, ch->sih,
	                       DMAREG(ch, DMA_TX, TX_Q0),
	                       DMAREG(ch, DMA_RX, RX_Q0),
	                       NTXD, NRXD, RXBUFSZ, -1, NRXBUFPOST, HWRXOFF,
	                       &et_msg_level);

	/* TX: TC_BE, RX: UNUSED */
	ch->di[1] = dma_attach(osh, name, ch->sih,
	                       DMAREG(ch, DMA_TX, TX_Q1),
	                       NULL /* rxq unused */,
	                       NTXD, 0, 0, -1, 0, 0, &et_msg_level);

	/* TX: TC_CL, RX: UNUSED */
	ch->di[2] = dma_attach(osh, name, ch->sih,
	                       DMAREG(ch, DMA_TX, TX_Q2),
	                       NULL /* rxq unused */,
	                       NTXD, 0, 0, -1, 0, 0, &et_msg_level);

	/* TX: TC_VO, RX: UNUSED */
	ch->di[3] = dma_attach(osh, name, ch->sih,
	                       DMAREG(ch, DMA_TX, TX_Q3),
	                       NULL /* rxq unused */,
	                       NTXD, 0, 0, -1, 0, 0, &et_msg_level);

	for (i = 0; i < NUMTXQ; i++)
		if (ch->di[i] == NULL) {
			ET_ERROR(("et%d: chipattach: dma_attach failed\n", etc->unit));
			goto fail;
		}

	for (i = 0; i < NUMTXQ; i++)
		if (ch->di[i] != NULL)
			etc->txavail[i] = (uint *)&ch->di[i]->txavail;

#ifndef _CFE_
	/* override dma parameters, corerev 4 dma channel 1,2 and 3 default burstlen is 0. */
	/* corerev 4,5: NS Ax; corerev 6: BCM43909 no HW prefetch; corerev 7: NS B0 */
	if (etc->corerev == 4 ||
	    etc->corerev == 5 ||
	    etc->corerev == 7) {
#define DMA_CTL_TX 0
#define DMA_CTL_RX 1

#define DMA_CTL_MR 0
#define DMA_CTL_PC 1
#define DMA_CTL_PT 2
#define DMA_CTL_BL 3
		static uint16 dmactl[2][4] = {
			/* TX */
			{ DMA_MR_2, DMA_PC_16, DMA_PT_8, DMA_BL_1024 },
			{ 0, DMA_PC_16, DMA_PT_8, DMA_BL_128 },
		};

		dmactl[DMA_CTL_TX][DMA_CTL_MR] = (TXMR == 2 ? DMA_MR_2 : DMA_MR_1);

		if (etc->corerev == 7) {
			/* NS B0 can only be configured to DMA_PT_1 and DMA_PC_4 */
			dmactl[DMA_CTL_TX][DMA_CTL_PT] = DMA_PT_1;
			dmactl[DMA_CTL_TX][DMA_CTL_PC] = DMA_PC_4;
		} else {
			dmactl[DMA_CTL_TX][DMA_CTL_PT] = (TXPREFTHRESH == 8 ? DMA_PT_8 :
				TXPREFTHRESH == 4 ? DMA_PT_4 :
				TXPREFTHRESH == 2 ? DMA_PT_2 : DMA_PT_1);
			dmactl[DMA_CTL_TX][DMA_CTL_PC] = (TXPREFCTL == 16 ? DMA_PC_16 :
				TXPREFCTL == 8 ? DMA_PC_8 :
				TXPREFCTL == 4 ? DMA_PC_4 : DMA_PC_0);
		}

		dmactl[DMA_CTL_TX][DMA_CTL_BL] = (TXBURSTLEN == 1024 ? DMA_BL_1024 :
		                                  TXBURSTLEN == 512 ? DMA_BL_512 :
		                                  TXBURSTLEN == 256 ? DMA_BL_256 :
		                                  TXBURSTLEN == 128 ? DMA_BL_128 :
		                                  TXBURSTLEN == 64 ? DMA_BL_64 :
		                                  TXBURSTLEN == 32 ? DMA_BL_32 : DMA_BL_16);

		dmactl[DMA_CTL_RX][DMA_CTL_PT] =  (RXPREFTHRESH == 8 ? DMA_PT_8 :
		                                   RXPREFTHRESH == 4 ? DMA_PT_4 :
		                                   RXPREFTHRESH == 2 ? DMA_PT_2 : DMA_PT_1);
		dmactl[DMA_CTL_RX][DMA_CTL_PC] = (RXPREFCTL == 16 ? DMA_PC_16 :
		                                  RXPREFCTL == 8 ? DMA_PC_8 :
		                                  RXPREFCTL == 4 ? DMA_PC_4 : DMA_PC_0);
		dmactl[DMA_CTL_RX][DMA_CTL_BL] = (RXBURSTLEN == 1024 ? DMA_BL_1024 :
		                                  RXBURSTLEN == 512 ? DMA_BL_512 :
		                                  RXBURSTLEN == 256 ? DMA_BL_256 :
		                                  RXBURSTLEN == 128 ? DMA_BL_128 :
		                                  RXBURSTLEN == 64 ? DMA_BL_64 :
		                                  RXBURSTLEN == 32 ? DMA_BL_32 : DMA_BL_16);

		for (i = 0; i < NUMTXQ; i++) {
				dma_param_set(ch->di[i], HNDDMA_PID_TX_MULTI_OUTSTD_RD,
				              dmactl[DMA_CTL_TX][DMA_CTL_MR]);
				dma_param_set(ch->di[i], HNDDMA_PID_TX_PREFETCH_CTL,
				              dmactl[DMA_CTL_TX][DMA_CTL_PC]);
				dma_param_set(ch->di[i], HNDDMA_PID_TX_PREFETCH_THRESH,
				              dmactl[DMA_CTL_TX][DMA_CTL_PT]);
				dma_param_set(ch->di[i], HNDDMA_PID_TX_BURSTLEN,
				              dmactl[DMA_CTL_TX][DMA_CTL_BL]);
				dma_param_set(ch->di[i], HNDDMA_PID_RX_PREFETCH_CTL,
				              dmactl[DMA_CTL_RX][DMA_CTL_PC]);
				dma_param_set(ch->di[i], HNDDMA_PID_RX_PREFETCH_THRESH,
				              dmactl[DMA_CTL_RX][DMA_CTL_PT]);
				dma_param_set(ch->di[i], HNDDMA_PID_RX_BURSTLEN,
				              dmactl[DMA_CTL_RX][DMA_CTL_BL]);
		}
	}
#endif /* ! _CFE_ */

	/* set default sofware intmask */
	sprintf(name, "et%d_no_txint", etc->unit);
	if (getintvar(ch->vars, name)) {
		/* if no_txint variable is non-zero we disable tx interrupts.
		 * we do the tx buffer reclaim once every few frames.
		 */
		ch->def_intmask = (DEF_INTMASK & ~(I_XI0 | I_XI1 | I_XI2 | I_XI3));
		etc->txrec_thresh = (((NTXD >> 2) > TXREC_THR) ? TXREC_THR - 1 : 1);
	} else
		ch->def_intmask = DEF_INTMASK;

	ch->intmask = ch->def_intmask;

	/* reset the external phy */
	if ((reset = getgpiopin(ch->vars, "ephy_reset", GPIO_PIN_NOTDEFINED)) !=
	    GPIO_PIN_NOTDEFINED) {
		reset = 1 << reset;

		/* Keep RESET low for 2 us */
		si_gpioout(ch->sih, reset, 0, GPIO_DRV_PRIORITY);
		si_gpioouten(ch->sih, reset, reset, GPIO_DRV_PRIORITY);
		OSL_DELAY(2);

		/* Keep RESET high for at least 2 us */
		si_gpioout(ch->sih, reset, reset, GPIO_DRV_PRIORITY);
		OSL_DELAY(2);

		/* if external phy is present enable auto-negotation and
		 * advertise full capabilities as default config.
		 */
		ASSERT(etc->phyaddr != EPHY_NOREG);
		etc->needautoneg = TRUE;
		etc->advertise = (ADV_100FULL | ADV_100HALF | ADV_10FULL | ADV_10HALF);
		etc->advertise2 = ADV_1000FULL;
	}

	/* reset phy: reset it once now */
	chipphyreset(ch, etc->phyaddr);

#ifdef ETROBO
	/*
	 * Broadcom Robo ethernet switch.
	 */
	if (DEV_NTKIF(etc) &&
		(boardflags & BFL_ENETROBO) && (etc->phyaddr == EPHY_NOREG)) {

		ET_TRACE(("et%d: chipattach: Calling robo attach\n", etc->unit));

		/* Attach to the switch */
		if (!(etc->robo = bcm_robo_attach(ch->sih, ch, ch->vars,
		                                  (miird_f)bcmgmac_et_chops.phyrd,
		                                  (miiwr_f)bcmgmac_et_chops.phywr))) {
			ET_ERROR(("et%d: chipattach: robo_attach failed\n", etc->unit));
			goto fail;
		}
		/* Enable the switch and set it to a known good state */
		if (bcm_robo_enable_device(etc->robo)) {
			ET_ERROR(("et%d: chipattach: robo_enable_device failed\n", etc->unit));
			goto fail;
		}
		/* Configure the switch to do VLAN */
		if ((boardflags & BFL_ENETVLAN) &&
		    bcm_robo_config_vlan(etc->robo, etc->perm_etheraddr.octet)) {
			ET_ERROR(("et%d: chipattach: robo_config_vlan failed\n", etc->unit));
			goto fail;
		}
		/* Enable switching/forwarding */
		if (bcm_robo_enable_switch(etc->robo)) {
			ET_ERROR(("et%d: chipattach: robo_enable_switch failed\n", etc->unit));
			goto fail;
		}
#ifdef PLC
		/* Configure the switch port connected to PLC chipset */
		robo_plc_hw_init(etc->robo);
#endif /* PLC */
	}
#endif /* ETROBO */

#ifdef ETADM
	/*
	 * ADMtek ethernet switch.
	 */
	if (boardflags & BFL_ENETADM) {
		/* Attach to the device */
		if (!(ch->adm = adm_attach(ch->sih, ch->vars))) {
			ET_ERROR(("et%d: chipattach: adm_attach failed\n", etc->unit));
			goto fail;
		}
		/* Enable the external switch and set it to a known good state */
		if (adm_enable_device(ch->adm)) {
			ET_ERROR(("et%d: chipattach: adm_enable_device failed\n", etc->unit));
			goto fail;
		}
		/* Configure the switch */
		if ((boardflags & BFL_ENETVLAN) && adm_config_vlan(ch->adm)) {
			ET_ERROR(("et%d: chipattach: adm_config_vlan failed\n", etc->unit));
			goto fail;
		}
	}
#endif /* ETADM */

#ifdef ETFA
	/*
	 * Broadcom FA.
	 */
	if (BCM4707_CHIP(CHIPID(ch->sih->chip))) {
		/* Attach to the fa */
		if ((etc->fa = fa_attach(ch->sih, ch->et, ch->vars, etc->coreunit, etc->robo))) {
			ET_TRACE(("et%d: chipattach: Calling fa attach\n", etc->unit));
			/* Enable the fa */
			if (fa_enable_device(etc->fa)) {
				ET_ERROR(("et%d: chipattach: fa_enable_device failed\n",
					etc->unit));
				goto fail;
			}
		}
	}
#endif /* ETFA */

	return ((void *) ch);

fail:
	chipdetach(ch);
	return (NULL);
}

static void
chipdetach(ch_t *ch)
{
	int32 i;

	ET_TRACE(("et%d: chipdetach\n", ch->etc->unit));

	if (ch == NULL)
		return;

#ifdef ETROBO
	/* free robo state */
	if (ch->etc->robo)
		bcm_robo_detach(ch->etc->robo);
#endif /* ETROBO */

#ifdef ETADM
	/* free ADMtek state */
	if (ch->adm)
		adm_detach(ch->adm);
#endif /* ETADM */

#ifdef ETFA
	/* free FA state */
	fa_detach(ch->etc->fa);
#endif /* ETFA */

	/* free dma state */
	for (i = 0; i < NUMTXQ; i++)
		if (ch->di[i] != NULL) {
			dma_detach(ch->di[i]);
			ch->di[i] = NULL;
		}

	/* put the core back into reset */
	/* For Northstar, should not disable any GMAC core */
	if (ch->sih && !BCM4707_CHIP(CHIPID(ch->sih->chip)))
		si_core_disable(ch->sih, 0);

	ch->etc->mib = NULL;

	/* free si handle */
	si_detach(ch->sih);
	ch->sih = NULL;

	/* free vars */
	if (ch->vars)
		MFREE(ch->osh, ch->vars, ch->vars_size);

	/* free chip private state */
	MFREE(ch->osh, ch, sizeof(ch_t));
}

static void
chiplongname(ch_t *ch, char *buf, uint bufsize)
{
	char *s;

	switch (ch->etc->deviceid) {
		case BCM47XX_GMAC_ID:
		case BCM4716_CHIP_ID:
		case BCM4748_CHIP_ID:
		default:
			s = "Broadcom BCM47XX 10/100/1000 Mbps Ethernet Controller";
			break;
	}

	strncpy(buf, s, bufsize);
	buf[bufsize - 1] = '\0';
}

static uint
chipmacrd(ch_t *ch, uint offset)
{
	ASSERT(offset < 4096); /* GMAC Register space is 4K size */
	return R_REG(ch->osh, (uint *)((uint)(ch->regs) + offset));
}

static void
chipmacwr(ch_t *ch, uint offset, uint val)
{
	ASSERT(offset < 4096); /* GMAC Register space is 4K size */
	W_REG(ch->osh, (uint *)((uint)(ch->regs) + offset), val);
}

static void
chipdump(ch_t *ch, struct bcmstrbuf *b)
{
#ifdef BCMDBG
	int32 i;

	bcm_bprintf(b, "regs 0x%lx etphy 0x%lx ch->intstatus 0x%x intmask 0x%x\n",
		(ulong)ch->regs, (ulong)ch->etphy, ch->intstatus, ch->intmask);
	bcm_bprintf(b, "\n");

	/* dma engine state */
	for (i = 0; i < NUMTXQ; i++) {
		dma_dump(ch->di[i], b, TRUE);
		bcm_bprintf(b, "\n");
	}

	/* registers */
	chipdumpregs(ch, ch->regs, b);
	bcm_bprintf(b, "\n");

	/* switch registers */
#ifdef ETROBO
	if (ch->etc->robo)
		robo_dump_regs(ch->etc->robo, b);
#endif /* ETROBO */
#ifdef ETADM
	if (ch->adm)
		adm_dump_regs(ch->adm, b->buf);
#endif /* ETADM */
#ifdef ETFA
	if (ch->etc->fa) {
		/* dump entries */
		fa_dump(ch->etc->fa, b, FALSE);
		/* dump regs */
		fa_regs_show(ch->etc->fa, b);
	}
#endif /* ETFA */
#endif	/* BCMDBG */
}

#ifdef BCMDBG

#define	PRREG(name)	bcm_bprintf(b, #name " 0x%x ", R_REG(ch->osh, &regs->name))
#define	PRMIBREG(name)	bcm_bprintf(b, #name " 0x%x ", R_REG(ch->osh, &regs->mib.name))

static void
chipdumpregs(ch_t *ch, gmacregs_t *regs, struct bcmstrbuf *b)
{
	uint phyaddr;

	phyaddr = ch->etc->phyaddr;

	PRREG(devcontrol); PRREG(devstatus);
	bcm_bprintf(b, "\n");
	PRREG(biststatus);
	bcm_bprintf(b, "\n");
	PRREG(intstatus); PRREG(intmask); PRREG(gptimer);
	bcm_bprintf(b, "\n");
	PRREG(intrecvlazy);
	bcm_bprintf(b, "\n");
	PRREG(flowctlthresh); PRREG(wrrthresh); PRREG(gmac_idle_cnt_thresh);
	bcm_bprintf(b, "\n");
	if (ch->etc->corerev != GMAC_4706B0_CORE_REV) {
		PRREG(phyaccess); PRREG(phycontrol);
		bcm_bprintf(b, "\n");
	}
	PRREG(txqctl); PRREG(rxqctl);
	bcm_bprintf(b, "\n");
	PRREG(gpioselect); PRREG(gpio_output_en);
	bcm_bprintf(b, "\n");
	PRREG(clk_ctl_st); PRREG(pwrctl);
	bcm_bprintf(b, "\n");

	/* unimac registers */
	if (!BCM4707_CHIP(CHIPID(ch->sih->chip))) {
		/* BCM4707 doesn't has unimacversion register */
		PRREG(unimacversion);
	}
	PRREG(hdbkpctl);
	bcm_bprintf(b, "\n");
	PRREG(cmdcfg);
	bcm_bprintf(b, "\n");
	PRREG(macaddrhigh); PRREG(macaddrlow);
	bcm_bprintf(b, "\n");
	PRREG(rxmaxlength); PRREG(pausequanta); PRREG(macmode);
	bcm_bprintf(b, "\n");
	PRREG(outertag); PRREG(innertag); PRREG(txipg); PRREG(pausectl);
	bcm_bprintf(b, "\n");
	PRREG(txflush); PRREG(rxstatus); PRREG(txstatus);
	bcm_bprintf(b, "\n");

	/* mib registers */
	PRMIBREG(tx_good_octets); PRMIBREG(tx_good_pkts); PRMIBREG(tx_octets); PRMIBREG(tx_pkts);
	bcm_bprintf(b, "\n");
	PRMIBREG(tx_broadcast_pkts); PRMIBREG(tx_multicast_pkts);
	bcm_bprintf(b, "\n");
	PRMIBREG(tx_jabber_pkts); PRMIBREG(tx_oversize_pkts); PRMIBREG(tx_fragment_pkts);
	bcm_bprintf(b, "\n");
	PRMIBREG(tx_underruns); PRMIBREG(tx_total_cols); PRMIBREG(tx_single_cols);
	bcm_bprintf(b, "\n");
	PRMIBREG(tx_multiple_cols); PRMIBREG(tx_excessive_cols); PRMIBREG(tx_late_cols);
	bcm_bprintf(b, "\n");
	if (ch->etc->corerev != GMAC_4706B0_CORE_REV) {
		PRMIBREG(tx_defered); PRMIBREG(tx_carrier_lost); PRMIBREG(tx_pause_pkts);
		bcm_bprintf(b, "\n");
	}

	PRMIBREG(rx_good_octets); PRMIBREG(rx_good_pkts); PRMIBREG(rx_octets); PRMIBREG(rx_pkts);
	bcm_bprintf(b, "\n");
	PRMIBREG(rx_broadcast_pkts); PRMIBREG(rx_multicast_pkts);
	bcm_bprintf(b, "\n");
	PRMIBREG(rx_jabber_pkts);
	if (ch->etc->corerev != GMAC_4706B0_CORE_REV) {
		PRMIBREG(rx_oversize_pkts); PRMIBREG(rx_fragment_pkts);
		bcm_bprintf(b, "\n");
		PRMIBREG(rx_missed_pkts); PRMIBREG(rx_crc_align_errs); PRMIBREG(rx_undersize);
	}
	bcm_bprintf(b, "\n");
	if (ch->etc->corerev != GMAC_4706B0_CORE_REV) {
		PRMIBREG(rx_crc_errs); PRMIBREG(rx_align_errs); PRMIBREG(rx_symbol_errs);
		bcm_bprintf(b, "\n");
		PRMIBREG(rx_pause_pkts); PRMIBREG(rx_nonpause_pkts);
		bcm_bprintf(b, "\n");
	}
	if (phyaddr != EPHY_NOREG) {
		/* print a few interesting phy registers */
		bcm_bprintf(b, "phy0 0x%x phy1 0x%x phy2 0x%x phy3 0x%x\n",
		               chipphyrd(ch, phyaddr, 0),
		               chipphyrd(ch, phyaddr, 1),
		               chipphyrd(ch, phyaddr, 2),
		               chipphyrd(ch, phyaddr, 3));
		bcm_bprintf(b, "phy4 0x%x phy5 0x%x phy24 0x%x phy25 0x%x\n",
		               chipphyrd(ch, phyaddr, 4),
		               chipphyrd(ch, phyaddr, 5),
		               chipphyrd(ch, phyaddr, 24),
		               chipphyrd(ch, phyaddr, 25));
	}

}
#endif	/* BCMDBG */

static void
gmac_clearmib(ch_t *ch)
{
	volatile uint32 *ptr;

	if (ch->etc->corerev == GMAC_4706B0_CORE_REV)
		return;

	/* enable clear on read */
	OR_REG(ch->osh, &ch->regs->devcontrol, DC_MROR);

	for (ptr = &ch->regs->mib.tx_good_octets; ptr <= &ch->regs->mib.rx_uni_pkts; ptr++) {
		(void)R_REG(ch->osh, ptr);
		if (ptr == &ch->regs->mib.tx_q3_octets_high)
			ptr++;
	}

	return;
}

static void
gmac_init_reset(ch_t *ch)
{
	OR_REG(ch->osh, &ch->regs->cmdcfg, CC_SR(ch->etc->corerev));
	OSL_DELAY(GMAC_RESET_DELAY);
}

static void
gmac_clear_reset(ch_t *ch)
{
	AND_REG(ch->osh, &ch->regs->cmdcfg, ~CC_SR(ch->etc->corerev));
	OSL_DELAY(GMAC_RESET_DELAY);
}

static void
gmac_reset(ch_t *ch)
{
	uint32 ocmdcfg, cmdcfg;

	/* put the mac in reset */
	gmac_init_reset(ch);

	/* initialize default config */
	ocmdcfg = cmdcfg = R_REG(ch->osh, &ch->regs->cmdcfg);

	cmdcfg &= ~(CC_TE | CC_RE | CC_RPI | CC_TAI | CC_HD | CC_ML |
	            CC_CFE | CC_RL | CC_RED | CC_PE | CC_TPI | CC_PAD_EN | CC_PF);
	cmdcfg |= (CC_PROM | CC_NLC | CC_CFE | CC_TPI | CC_AT);

	if (cmdcfg != ocmdcfg)
		W_REG(ch->osh, &ch->regs->cmdcfg, cmdcfg);

	/* bring mac out of reset */
	gmac_clear_reset(ch);
}

static void
gmac_promisc(ch_t *ch, bool mode)
{
	uint32 cmdcfg;

	cmdcfg = R_REG(ch->osh, &ch->regs->cmdcfg);

	/* put the mac in reset */
	gmac_init_reset(ch);

	/* enable or disable promiscuous mode */
	if (mode)
		cmdcfg |= CC_PROM;
	else
		cmdcfg &= ~CC_PROM;

	W_REG(ch->osh, &ch->regs->cmdcfg, cmdcfg);

	/* bring mac out of reset */
	gmac_clear_reset(ch);
}

static int
gmac_speed(ch_t *ch, uint32 speed)
{
	uint32 cmdcfg;
	uint32 hd_ena = 0;

	switch (speed) {
		case ET_10HALF:
			hd_ena = CC_HD;
			/* FALLTHRU */

		case ET_10FULL:
			speed = 0;
			break;

		case ET_100HALF:
			hd_ena = CC_HD;
			/* FALLTHRU */

		case ET_100FULL:
			speed = 1;
			break;

		case ET_1000FULL:
			speed = 2;
			break;

		case ET_1000HALF:
			ET_ERROR(("et%d: gmac_speed: supports 1000 mbps full duplex only\n",
			          ch->etc->unit));
			return (FAILURE);

		case ET_2500FULL:
			speed = 3;
			break;

		default:
			ET_ERROR(("et%d: gmac_speed: speed %d not supported\n",
			          ch->etc->unit, speed));
			return (FAILURE);
	}

	cmdcfg = R_REG(ch->osh, &ch->regs->cmdcfg);

	/* put mac in reset */
	gmac_init_reset(ch);

	/* set the speed */
	cmdcfg &= ~(CC_ES_MASK | CC_HD);
	cmdcfg |= ((speed << CC_ES_SHIFT) | hd_ena);
	W_REG(ch->osh, &ch->regs->cmdcfg, cmdcfg);

	/* bring mac out of reset */
	gmac_clear_reset(ch);

	return (SUCCESS);
}

static void
gmac_macloopback(ch_t *ch, bool on)
{
	uint32 ocmdcfg, cmdcfg;

	ocmdcfg = cmdcfg = R_REG(ch->osh, &ch->regs->cmdcfg);

	/* put mac in reset */
	gmac_init_reset(ch);

	/* set/clear the mac loopback mode */
	if (on)
		cmdcfg |= CC_ML;
	else
		cmdcfg &= ~CC_ML;

	if (cmdcfg != ocmdcfg)
		W_REG(ch->osh, &ch->regs->cmdcfg, cmdcfg);

	/* bring mac out of reset */
	gmac_clear_reset(ch);
}

static int
gmac_loopback(ch_t *ch, uint32 mode)
{
	switch (mode) {
		case LOOPBACK_MODE_DMA:
			/* to enable loopback for any channel set the loopback
			 * enable bit in xmt0control register.
			 */
			dma_fifoloopbackenable(ch->di[TX_Q0]);
			break;

		case LOOPBACK_MODE_MAC:
			gmac_macloopback(ch, TRUE);
			break;

		case LOOPBACK_MODE_NONE:
			gmac_macloopback(ch, FALSE);
			break;

		default:
			ET_ERROR(("et%d: gmac_loopaback: Unknown loopback mode %d\n",
			          ch->etc->unit, mode));
			return (FAILURE);
	}

	return (SUCCESS);
}

static void
gmac_enable(ch_t *ch)
{
	uint32 cmdcfg, rxqctl, bp_clk, mdp, mode;
	gmacregs_t *regs;

	regs = ch->regs;

	cmdcfg = R_REG(ch->osh, &ch->regs->cmdcfg);

	/* put mac in reset */
	gmac_init_reset(ch);

	cmdcfg |= CC_SR(ch->etc->corerev);

	/* first deassert rx_ena and tx_ena while in reset */
	cmdcfg &= ~(CC_RE | CC_TE);
	W_REG(ch->osh, &regs->cmdcfg, cmdcfg);

	/* bring mac out of reset */
	gmac_clear_reset(ch);

	/* enable the mac transmit and receive paths now */
	OSL_DELAY(2);
	cmdcfg &= ~CC_SR(ch->etc->corerev);
	cmdcfg |= (CC_RE | CC_TE);

	/* assert rx_ena and tx_ena when out of reset to enable the mac */
	W_REG(ch->osh, &regs->cmdcfg, cmdcfg);

	/* WAR to not force ht for 47162 when gmac is in rev mii mode */
	mode = ((R_REG(ch->osh, &regs->devstatus) & DS_MM_MASK) >> DS_MM_SHIFT);
	if ((CHIPID(ch->sih->chip) != BCM47162_CHIP_ID) || (mode != 0))
		/* request ht clock */
		OR_REG(ch->osh, &regs->clk_ctl_st, CS_FH);

	if (PMUCTL_ENAB(ch->sih) && (CHIPID(ch->sih->chip) == BCM47162_CHIP_ID) && (mode == 2))
		si_pmu_chipcontrol(ch->sih, PMU_CHIPCTL1, PMU_CC1_RXC_DLL_BYPASS,
		                   PMU_CC1_RXC_DLL_BYPASS);

	/* Set the GMAC flowcontrol on and off thresholds and pause retry timer
	 * the thresholds are tuned based on size of buffer internal to GMAC.
	 */
	if ((CHIPID(ch->sih->chip) == BCM5357_CHIP_ID) ||
	    (CHIPID(ch->sih->chip) == BCM4749_CHIP_ID) ||
	    (CHIPID(ch->sih->chip) == BCM53572_CHIP_ID) ||
	    (CHIPID(ch->sih->chip) == BCM4716_CHIP_ID) ||
	    (CHIPID(ch->sih->chip) == BCM47162_CHIP_ID)) {
		uint32 flctl = 0x03cb04cb;

		if ((CHIPID(ch->sih->chip) == BCM5357_CHIP_ID) ||
		    (CHIPID(ch->sih->chip) == BCM4749_CHIP_ID) ||
		    (CHIPID(ch->sih->chip) == BCM53572_CHIP_ID))
			flctl = 0x2300e1;

		W_REG(ch->osh, &regs->flowctlthresh, flctl);
		W_REG(ch->osh, &regs->pausectl, 0x27fff);
	}

	/* To prevent any risk of the BCM4707 ROM mdp issue we saw in the QT,
	 * we use the mdp register default value
	 */
	if (!BCM4707_CHIP(CHIPID(ch->sih->chip))) {
		/* init the mac data period. the value is set according to expr
		 * ((128ns / bp_clk) - 3).
		 */
		rxqctl = R_REG(ch->osh, &regs->rxqctl);
		rxqctl &= ~RC_MDP_MASK;

		bp_clk = si_clock(ch->sih) / 1000000;
		mdp = ((bp_clk * 128) / 1000) - 3;
		W_REG(ch->osh, &regs->rxqctl, rxqctl | (mdp << RC_MDP_SHIFT));
	}

	return;
}

static void
gmac_txflowcontrol(ch_t *ch, bool on)
{
	uint32 cmdcfg;

	cmdcfg = R_REG(ch->osh, &ch->regs->cmdcfg);

	/* put the mac in reset */
	gmac_init_reset(ch);

	/* to enable tx flow control clear the rx pause ignore bit */
	if (on)
		cmdcfg &= ~CC_RPI;
	else
		cmdcfg |= CC_RPI;

	W_REG(ch->osh, &ch->regs->cmdcfg, cmdcfg);

	/* bring mac out of reset */
	gmac_clear_reset(ch);
}

static void
gmac_miiconfig(ch_t *ch)
{
	/* BCM4707 GMAC DevStatus register has different definition of "Interface Mode"
	 * Bit 12:8  "interface_mode"  This field is programmed through IDM control bits [6:2]
	 *
	 * Bit 0 : SOURCE_SYNC_MODE_EN - If set, Rx line clock input will be used by Unimac for
	 *          sampling data.If this is reset, PLL reference clock (Clock 250 or Clk 125 based
	 *          on CLK_250_SEL) will be used as receive side line clock.
	 * Bit 1 : DEST_SYNC_MODE_EN - If this is reset, PLL reference clock input (Clock 250 or
	 *          Clk 125 based on CLK_250_SEL) will be used as transmit line clock.
	 *          If this is set, TX line clock input (from external switch/PHY) is used as
	 *          transmit line clock.
	 * Bit 2 : TX_CLK_OUT_INVERT_EN - If set, this will invert the TX clock out of AMAC.
	 * Bit 3 : DIRECT_GMII_MODE - If direct gmii is set to 0, then only 25 MHz clock needs to
	 *          be fed at 25MHz reference clock input, for both 10/100 Mbps speeds.
	 *          Unimac will internally divide the clock to 2.5 MHz for 10 Mbps speed
	 * Bit 4 : CLK_250_SEL - When set, this selects 250Mhz reference clock input and hence
	 *          Unimac line rate will be 2G.
	 *          If reset, this selects 125MHz reference clock input.
	 */
	if (BCM4707_CHIP(CHIPID(ch->sih->chip))) {
		if (ch->etc->phyaddr == EPHY_NOREG) {
			si_core_cflags(ch->sih, 0x44, 0x44);
			gmac_speed(ch, ET_2500FULL);
			ch->etc->speed = 2500;
			ch->etc->duplex = 1;
		}
	} else {
		uint32 devstatus, mode;
		gmacregs_t *regs;

		regs = ch->regs;

		/* Read the devstatus to figure out the configuration
		 * mode of the interface.
		 */
		devstatus = R_REG(ch->osh, &regs->devstatus);
		mode = ((devstatus & DS_MM_MASK) >> DS_MM_SHIFT);

		/* Set the speed to 100 if the switch interface is
		 * using mii/rev mii.
		 */
		if ((mode == 0) || (mode == 1)) {
			if (ch->etc->forcespeed == ET_AUTO) {
				gmac_speed(ch, ET_100FULL);
				ch->etc->speed = 100;
				ch->etc->duplex = 1;
			} else
				gmac_speed(ch, ch->etc->forcespeed);
		} else {
			if (ch->etc->phyaddr == EPHY_NOREG) {
				ch->etc->speed = 1000;
				ch->etc->duplex = 1;
			}
		}
	}
}

static void
chipreset(ch_t *ch)
{
	gmacregs_t *regs;
	uint32 i, sflags, flagbits = 0;

	ET_TRACE(("et%d: chipreset\n", ch->etc->unit));

	regs = ch->regs;

	if (!si_iscoreup(ch->sih)) {
		if (!ch->etc->nicmode)
			si_pci_setup(ch->sih, (1 << si_coreidx(ch->sih)));
		/* power on reset: reset the enet core */
		goto chipinreset;
	}

	/* update software counters before resetting the chip */
	if (ch->mibgood)
		chipstatsupd(ch);

	/* reset the tx dma engines */
	for (i = 0; i < NUMTXQ; i++) {
		if (ch->di[i]) {
			ET_TRACE(("et%d: resetting tx dma%d\n", ch->etc->unit, i));
			dma_txreset(ch->di[i]);
		}
	}

	/* set gmac into loopback mode to ensure no rx traffic */
	gmac_loopback(ch, LOOPBACK_MODE_MAC);
	OSL_DELAY(1);

	/* reset the rx dma engine */
	if (ch->di[RX_Q0]) {
		ET_TRACE(("et%d: resetting rx dma\n", ch->etc->unit));
		dma_rxreset(ch->di[RX_Q0]);
	}

	/* clear the multicast filter table */
	gmac_mf_cleanup(ch);

chipinreset:
	sflags = si_core_sflags(ch->sih, 0, 0);
	/* Do not enable internal switch for 47186/47188 */
	if ((((CHIPID(ch->sih->chip) == BCM5357_CHIP_ID) ||
	      (CHIPID(ch->sih->chip) == BCM4749_CHIP_ID)) &&
	     (ch->sih->chippkg == BCM47186_PKG_ID)) ||
	    ((CHIPID(ch->sih->chip) == BCM53572_CHIP_ID) && (ch->sih->chippkg == BCM47188_PKG_ID)))
		sflags &= ~SISF_SW_ATTACHED;

	if (sflags & SISF_SW_ATTACHED) {
		ET_TRACE(("et%d: internal switch attached\n", ch->etc->unit));
		flagbits = SICF_SWCLKE;
		if (!ch->etc->robo) {
			ET_TRACE(("et%d: reseting switch\n", ch->etc->unit));
			flagbits |= SICF_SWRST;
		}
	}

	/* 3GMAC: for BCM4707 and BCM47094, only do core reset at chipattach */
	if ((CHIPID(ch->sih->chip) != BCM4707_CHIP_ID) &&
	    (CHIPID(ch->sih->chip) != BCM47094_CHIP_ID)) {
		/* reset core */
		si_core_reset(ch->sih, flagbits, 0);
	}

	/* Request Misc PLL for corerev > 2 */
	if (ch->etc->corerev > 2 && !BCM4707_CHIP(CHIPID(ch->sih->chip))) {
		OR_REG(ch->osh, &regs->clk_ctl_st, CS_ER);
		SPINWAIT((R_REG(ch->osh, &regs->clk_ctl_st) & CS_ES) != CS_ES, 1000);
	}

	if ((CHIPID(ch->sih->chip) == BCM5357_CHIP_ID) ||
	    (CHIPID(ch->sih->chip) == BCM4749_CHIP_ID) ||
	    (CHIPID(ch->sih->chip) == BCM53572_CHIP_ID)) {
		char *var;
		uint32 sw_type = PMU_CC1_SW_TYPE_EPHY | PMU_CC1_IF_TYPE_MII;

		if ((var = getvar(ch->vars, "et_swtype")) != NULL)
			sw_type = (bcm_atoi(var) & 0x0f) << 4;
		else if ((CHIPID(ch->sih->chip) == BCM5357_CHIP_ID) &&
		         (ch->sih->chippkg == BCM5358_PKG_ID))
			sw_type = PMU_CC1_SW_TYPE_EPHYRMII;
		else if (((CHIPID(ch->sih->chip) != BCM53572_CHIP_ID) &&
		          (ch->sih->chippkg == BCM47186_PKG_ID)) ||
		         ((CHIPID(ch->sih->chip) == BCM53572_CHIP_ID) &&
		          (ch->sih->chippkg == BCM47188_PKG_ID)) ||
		         (ch->sih->chippkg == HWSIM_PKG_ID))
			sw_type = PMU_CC1_IF_TYPE_RGMII|PMU_CC1_SW_TYPE_RGMII;

		ET_TRACE(("%s: sw_type %04x\n", __FUNCTION__, sw_type));
		if (PMUCTL_ENAB(ch->sih)) {
			si_pmu_chipcontrol(ch->sih, PMU_CHIPCTL1,
				(PMU_CC1_IF_TYPE_MASK|PMU_CC1_SW_TYPE_MASK),
				sw_type);
		}
	}

	if ((sflags & SISF_SW_ATTACHED) && (!ch->etc->robo)) {
		ET_TRACE(("et%d: taking switch out of reset\n", ch->etc->unit));
		si_core_cflags(ch->sih, SICF_SWRST, 0);
	}

	/* reset gmac */
	gmac_reset(ch);

	/* clear mib */
	gmac_clearmib(ch);
	ch->mibgood = TRUE;

	if (ch->etc->corerev == GMAC_4706B0_CORE_REV)
		OR_REG(ch->osh, &ch->regscomm->phycontrol, PC_MTE);
	else
		OR_REG(ch->osh, &regs->phycontrol, PC_MTE);

	/* Read the devstatus to figure out the configuration mode of
	 * the interface. Set the speed to 100 if the switch interface
	 * is mii/rmii.
	 */
	gmac_miiconfig(ch);

#if !defined(_CFE_) && defined(BCM47XX_CA9)
	if (OSL_ACP_WAR_ENAB() || OSL_ARCH_IS_COHERENT()) {
		uint32 mask = (0xf << 16) | (0xf << 7) | (0x1f << 25) | (0x1f << 20);
		uint32 val = (0xb << 16) | (0x7 << 7) | (0x1 << 25) | (0x1 << 20);
		/* si_core_cflags to change ARCACHE[19:16] to 0xb, and AWCACHE[10:7] to 0x7,
		 * ARUSER[29:25] to 0x1, and AWUSER [24:20] to 0x1
		 */
		si_core_cflags(ch->sih, mask, val);
	}
#endif /* !_CFE_ && BCM47XX_CA9 */

	/* gmac doesn't have internal phy */
	chipphyinit(ch, ch->etc->phyaddr);

	/* clear persistent sw intstatus */
	ch->intstatus = 0;
}

/*
 * Lookup a multicast address in the filter hash table.
 */
static int
gmac_mf_lkup(ch_t *ch, struct ether_addr *mcaddr)
{
	mflist_t *ptr;

	/* find the multicast address */
	for (ptr = ch->mf.bucket[GMAC_MCADDR_HASH(mcaddr)]; ptr != NULL; ptr = ptr->next) {
		if (!ETHER_MCADDR_CMP(&ptr->mc_addr, mcaddr))
			return (SUCCESS);
	}

	return (FAILURE);
}

/*
 * Add a multicast address to the filter hash table.
 */
static int
gmac_mf_add(ch_t *ch, struct ether_addr *mcaddr)
{
	uint32 hash;
	mflist_t *entry;
#ifdef BCMDBG
	char mac[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	/* add multicast addresses only */
	if (!ETHER_ISMULTI(mcaddr)) {
		ET_ERROR(("et%d: adding invalid multicast address %s\n",
		          ch->etc->unit, bcm_ether_ntoa(mcaddr, mac)));
		return (FAILURE);
	}

	/* discard duplicate add requests */
	if (gmac_mf_lkup(ch, mcaddr) == SUCCESS) {
		ET_ERROR(("et%d: adding duplicate mcast filter entry\n", ch->etc->unit));
		return (FAILURE);
	}

	/* allocate memory for list entry */
	entry = MALLOC(ch->osh, sizeof(mflist_t));
	if (entry == NULL) {
		ET_ERROR(("et%d: out of memory allocating mcast filter entry\n", ch->etc->unit));
		return (FAILURE);
	}

	/* add the entry to the hash bucket */
	ether_copy(mcaddr, &entry->mc_addr);
	hash = GMAC_MCADDR_HASH(mcaddr);
	entry->next = ch->mf.bucket[hash];
	ch->mf.bucket[hash] = entry;

	return (SUCCESS);
}

/*
 * Cleanup the multicast filter hash table.
 */
static void
gmac_mf_cleanup(ch_t *ch)
{
	mflist_t *ptr, *tmp;
	int32 i;

	for (i = 0; i < GMAC_HASHT_SIZE; i++) {
		ptr = ch->mf.bucket[i];
		while (ptr) {
			tmp = ptr;
			ptr = ptr->next;
			MFREE(ch->osh, tmp, sizeof(mflist_t));
		}
		ch->mf.bucket[i] = NULL;
	}
}

/*
 * Initialize all the chip registers.  If dma mode, init tx and rx dma engines
 * but leave the devcontrol tx and rx (fifos) disabled.
 */
static void
chipinit(ch_t *ch, uint options)
{
	etc_info_t *etc;
	gmacregs_t *regs;
	uint idx;
	uint i;

	regs = ch->regs;
	etc = ch->etc;
	idx = 0;

	ET_TRACE(("et%d: chipinit\n", etc->unit));

	/* enable one rx interrupt per received frame */
	W_REG(ch->osh, &regs->intrecvlazy, (1 << IRL_FC_SHIFT));

	/* enable 802.3x tx flow control (honor received PAUSE frames) */
	gmac_txflowcontrol(ch, TRUE);

	/* enable/disable promiscuous mode */
	gmac_promisc(ch, etc->promisc);

	/* set our local address */
	W_REG(ch->osh, &regs->macaddrhigh,
	      hton32(*(uint32 *)&etc->cur_etheraddr.octet[0]));
	W_REG(ch->osh, &regs->macaddrlow,
	      hton16(*(uint16 *)&etc->cur_etheraddr.octet[4]));

	if (!etc->promisc) {
		/* gmac doesn't have a cam, hence do the multicast address filtering
		 * in the software
		 */
		/* allmulti or a list of discrete multicast addresses */
		if (!etc->allmulti && etc->nmulticast)
			for (i = 0; i < etc->nmulticast; i++)
				(void)gmac_mf_add(ch, &etc->multicast[i]);
	}

	/* optionally enable mac-level loopback */
	if (etc->loopbk)
		gmac_loopback(ch, LOOPBACK_MODE_MAC);
	else
		gmac_loopback(ch, LOOPBACK_MODE_NONE);

	/* set max frame lengths - account for possible vlan tag */
	W_REG(ch->osh, &regs->rxmaxlength, ETHER_MAX_LEN + 32);

	/*
	 * Optionally, disable phy autonegotiation and force our speed/duplex
	 * or constrain our advertised capabilities.
	 */
	if (etc->forcespeed != ET_AUTO) {
		gmac_speed(ch, etc->forcespeed);
		chipphyforce(ch, etc->phyaddr);
	} else if (etc->advertise && etc->needautoneg)
		chipphyadvertise(ch, etc->phyaddr);

	/* NS B0 only enables 4 entries x 4 channels */
	if (etc->corerev == 7)
		OR_REG(ch->osh, &regs->pwrctl, 0x1);

	/* enable the overflow continue feature and disable parity */
	dma_ctrlflags(ch->di[0], DMA_CTRL_ROC | DMA_CTRL_PEN | DMA_CTRL_RXSINGLE /* mask */,
	              DMA_CTRL_ROC | DMA_CTRL_RXSINGLE /* value */);

	if (options & ET_INIT_FULL) {
		/* initialize the tx and rx dma channels */
		for (i = 0; i < NUMTXQ; i++)
			dma_txinit(ch->di[i]);
		dma_rxinit(ch->di[RX_Q0]);

		/* post dma receive buffers */
		dma_rxfill(ch->di[RX_Q0]);

		/* lastly, enable interrupts */
		if (options & ET_INIT_INTRON)
			et_intrson(etc->et);
	}
	else
		dma_rxenable(ch->di[RX_Q0]);

	/* turn on the emac */
	gmac_enable(ch);
}

/* dma transmit */
static bool BCMFASTPATH
chiptx(ch_t *ch, void *p0)
{
	int error, len;
	uint32 q = TX_Q0;

	ET_TRACE(("et%d: chiptx\n", ch->etc->unit));
	ET_LOG("et%d: chiptx", ch->etc->unit, 0);

#ifdef ETROBO
	if ((ch->etc->robo != NULL) &&
	    (((robo_info_t *)ch->etc->robo)->devid == DEVID53115)) {
		void *p = p0;

		if ((p0 = etc_bcm53115_war(ch->etc, p)) == NULL) {
			PKTFREE(ch->osh, p, TRUE);
			return FALSE;
		}
	}
#endif /* ETROBO */

	len = PKTLEN(ch->osh, p0);

	/* check tx max length */
	if (len > (ETHER_MAX_LEN + 32)) {
		ET_ERROR(("et%d: chiptx: max frame length exceeded\n",
		          ch->etc->unit));
		PKTFREE(ch->osh, p0, TRUE);
		return FALSE;
	}

	if ((len < GMAC_MIN_FRAMESIZE) && (ch->etc->corerev == 0))
		PKTSETLEN(ch->osh, p0, GMAC_MIN_FRAMESIZE);

	/* queue the packet based on its priority */
	if (ch->etc->qos) {
		if (ch->etc->corerev != 4 && ch->etc->corerev != 5) {
			q = etc_up2tc(PKTPRIO(p0));
		}
		else {
			q = TC_BE;
		}
	}

	ASSERT(q < NUMTXQ);

	/* if tx completion intr is disabled then do the reclaim
	 * once every few frames transmitted.
	 */
	if ((ch->etc->txframes[q] & ch->etc->txrec_thresh) == 1)
		dma_txreclaim(ch->di[q], HNDDMA_RANGE_TRANSMITTED);

#if defined(_CFE_) || defined(__NetBSD__)
	error = dma_txfast(ch->di[q], p0, TRUE);
#else
#ifdef PKTC
#define DMA_COMMIT	((PKTCFLAGS(p0) & 1) != 0)
#else
#define DMA_COMMIT	(TRUE)
#endif
	error = dma_txfast(ch->di[q], p0, DMA_COMMIT);
#endif /* defined(_CFE_) || defined(__NetBSD__) */

	if (error) {
		ET_ERROR(("et%d: chiptx: out of txds\n", ch->etc->unit));
		ch->etc->txnobuf++;
		return FALSE;
	}

	ch->etc->txframes[q]++;

	/* set back the orig length */
	if (len < GMAC_MIN_FRAMESIZE)
		PKTSETLEN(ch->osh, p0, len);

	return TRUE;
}

/* reclaim completed transmit descriptors and packets */
static void BCMFASTPATH
chiptxreclaim(ch_t *ch, bool forceall)
{
	int32 i;

	ET_TRACE(("et%d: chiptxreclaim\n", ch->etc->unit));

	for (i = 0; i < NUMTXQ; i++) {
		if (*(uint *)(ch->etc->txavail[i]) < NTXD)
			dma_txreclaim(ch->di[i], forceall ? HNDDMA_RANGE_ALL :
			                                    HNDDMA_RANGE_TRANSMITTED);
		ch->intstatus &= ~(I_XI0 << i);
	}
}

/* dma receive: returns a pointer to the next frame received, or NULL if there are no more */
static void * BCMFASTPATH
chiprx(ch_t *ch)
{
	void *p;
	struct ether_addr *da;

	ET_TRACE(("et%d: chiprx\n", ch->etc->unit));
	ET_LOG("et%d: chiprx", ch->etc->unit, 0);

	/* gmac doesn't have a cam to do address filtering. so we implement
	 * the multicast address filtering here.
	 */
	while ((p = dma_rx(ch->di[RX_Q0])) != NULL) {
		/* check for overflow error packet */
		if (RXH_FLAGS(ch->etc, PKTDATA(ch->osh, p)) & htol16(GRXF_OVF)) {
			ET_ERROR(("et%d: chiprx, dma overflow\n", ch->etc->unit));
			PKTFREE(ch->osh, p, FALSE);
			ch->etc->rxoflodiscards++;
			continue;
		}

		if (ch->etc->allmulti) {
			return (p);
		}
		else {
			/* skip the rx header */
			PKTPULL(ch->osh, p, HWRXOFF);

			/* do filtering only for multicast packets when allmulti is false */
			da = (struct ether_addr *)PKTDATA(ch->osh, p);
			if (!ETHER_ISMULTI(da) ||
			    (gmac_mf_lkup(ch, da) == SUCCESS) || ETHER_ISBCAST(da)) {
				PKTPUSH(ch->osh, p, HWRXOFF);
				return (p);
			}
			PKTFREE(ch->osh, p, FALSE);
		}
	}

	ch->intstatus &= ~I_RI;

	/* post more rx buffers since we consumed a few */
	dma_rxfill(ch->di[RX_Q0]);

	return (NULL);
}

static int BCMFASTPATH /* dma receive quota number of pkts */
chiprxquota(ch_t *ch, int quota, void **rxpkts)
{
	int rxcnt;
	void * pkt;
	uint8 * rxh;
	hnddma_t * di_rx_q0;

	ET_TRACE(("et%d: chiprxquota %d\n", ch->etc->unit, quota));
	ET_LOG("et%d: chiprxquota", ch->etc->unit, 0);

	rxcnt = 0;
	di_rx_q0 = ch->di[RX_Q0];

	/* Fetch quota number of pkts (or ring empty) */
	while ((rxcnt < quota) && ((pkt = dma_rx(di_rx_q0)) != NULL)) {
		rxh = PKTDATA(ch->osh, pkt); /* start of pkt data */

#if !defined(_CFE_)
		bcm_prefetch_32B(rxh + 32, 1); /* skip 30B of HWRXOFF */
#endif /* _CFE_ */

#if defined(BCM_GMAC3)
		if (GMAC_RXH_FLAGS(rxh)) { /* rx error reported in rxheader */
			et_discard(ch->etc->et, pkt); /* handoff to port layer */
			continue; /* do not increment rxcnt */
		}
#endif /* BCM_GMAC3 */

		rxpkts[rxcnt] = pkt;
		rxcnt++;
	}

	/* This pass is not on the tput performance path! */
	if (!ch->etc->allmulti) { /* SW Filter all configured mf EthDA(s) */
		struct ether_addr *da;
		int nrx;
		int mfpass = 0; /* pkts that passed mf lkup */

		for (nrx = 0; nrx < rxcnt; nrx++) {
			pkt = rxpkts[nrx];
			da = (struct ether_addr *)(PKTDATA(ch->osh, pkt) + HWRXOFF);
			if (!ETHER_ISMULTI(da) || ETHER_ISBCAST(da) ||
				(gmac_mf_lkup(ch, da) == SUCCESS)) {
				rxpkts[mfpass++] = pkt; /* repack rxpkts array */
			} else {
				PKTFREE(ch->osh, pkt, FALSE);
			}
		}

		rxcnt = mfpass;
	}

	/* post more rx buffers since we consumed a few */
	dma_rxfill(di_rx_q0);

	if (rxcnt < quota) { /* ring is "possibly" empty, enable et interrupts */
		ch->intstatus &= ~I_RI;
	}

	return rxcnt; /* rxpkts[] has rxcnt number of pkts to be processed */
}

static void BCMFASTPATH
chiprxlazy(ch_t *ch)
{
	uint reg_val = ((ch->etc->rxlazy_timeout & IRL_TO_MASK) |
	                (ch->etc->rxlazy_framecnt << IRL_FC_SHIFT));
	W_REG(ch->osh, &ch->regs->intrecvlazy, reg_val);
}


/* reclaim completed dma receive descriptors and packets */
static void
chiprxreclaim(ch_t *ch)
{
	ET_TRACE(("et%d: chiprxreclaim\n", ch->etc->unit));
	dma_rxreclaim(ch->di[RX_Q0]);
	ch->intstatus &= ~I_RI;
}

/* calculate the number of free dma receive descriptors */
static uint BCMFASTPATH
chipactiverxbuf(ch_t *ch)
{
	ET_TRACE(("et%d: chipactiverxbuf\n", ch->etc->unit));
	ET_LOG("et%d: chipactiverxbuf", ch->etc->unit, 0);
	return dma_activerxbuf(ch->di[RX_Q0]);
}

/* allocate and post dma receive buffers */
static void BCMFASTPATH
chiprxfill(ch_t *ch)
{
	ET_TRACE(("et%d: chiprxfill\n", ch->etc->unit));
	ET_LOG("et%d: chiprx", ch->etc->unit, 0);
	dma_rxfill(ch->di[RX_Q0]);
}

/* get current and pending interrupt events */
static int BCMFASTPATH
chipgetintrevents(ch_t *ch, bool in_isr)
{
	uint32 intstatus;
	int events;

	events = 0;

	/* read the interrupt status register */
	intstatus = R_REG(ch->osh, &ch->regs->intstatus);

	/* defer unsolicited interrupts */
	intstatus &= (in_isr ? ch->intmask : ch->def_intmask);

	if (intstatus != 0)
		events = INTR_NEW;

	/* or new bits into persistent intstatus */
	intstatus = (ch->intstatus |= intstatus);

	/* return if no events */
	if (intstatus == 0)
		return (0);

	/* convert chip-specific intstatus bits into generic intr event bits */
#if defined(BCM_GMAC3)
	if (intstatus & I_TO) {
		events |= INTR_TO;                          /* post to et_dpc */

		W_REG(ch->osh, &ch->regs->intstatus, I_TO); /* explicitly ack I_TO */
		ch->intstatus &= ~(I_TO);                   /* handled in et_linux */
	}
#endif /* BCM_GMAC3 */

	if (intstatus & I_RI)
		events |= INTR_RX;
	if (intstatus & (I_XI0 | I_XI1 | I_XI2 | I_XI3))
		events |= INTR_TX;
#if defined(_CFE_)
	if (intstatus & ~(I_RDU | I_RFO) & I_ERRORS)
#else
	if (intstatus & I_ERRORS)
#endif
		events |= INTR_ERROR;

	return (events);
}

/* enable chip interrupts */
static void BCMFASTPATH
chipintrson(ch_t *ch)
{
	ch->intmask = ch->def_intmask;
	W_REG(ch->osh, &ch->regs->intmask, ch->intmask);
}

/* disable chip interrupts */
static void BCMFASTPATH
chipintrsoff(ch_t *ch)
{
	/* disable further interrupts from gmac */
#if defined(BCM_GMAC3)
	ch->intmask = I_TO;	/* Do not disable GPtimer Interrupt */
#else  /* ! BCM_GMAC3 */
	ch->intmask = 0;
#endif /* ! BCM_GMAC3 */

	W_REG(ch->osh, &ch->regs->intmask, ch->intmask);
	(void) R_REG(ch->osh, &ch->regs->intmask);	/* sync readback */

	/* clear the interrupt conditions */
	W_REG(ch->osh, &ch->regs->intstatus, ch->intstatus);
}

/* return true of caller should re-initialize, otherwise false */
static bool BCMFASTPATH
chiperrors(ch_t *ch)
{
	uint32 intstatus;
	etc_info_t *etc;

	etc = ch->etc;

	intstatus = ch->intstatus;
	ch->intstatus &= ~(I_ERRORS);

	ET_TRACE(("et%d: chiperrors: intstatus 0x%x\n", etc->unit, intstatus));

	if (intstatus & I_PDEE) {
		ET_ERROR(("et%d: descriptor error\n", etc->unit));
		etc->dmade++;
	}

	if (intstatus & I_PDE) {
		ET_ERROR(("et%d: data error\n", etc->unit));
		etc->dmada++;
	}

	if (intstatus & I_DE) {
		ET_ERROR(("et%d: descriptor protocol error\n", etc->unit));
		etc->dmape++;
	}

	if (intstatus & I_RDU) {
		ET_ERROR(("et%d: receive descriptor underflow\n", etc->unit));
		etc->rxdmauflo++;
	}

	if (intstatus & I_RFO) {
		ET_TRACE(("et%d: receive fifo overflow\n", etc->unit));
		etc->rxoflo++;
	}

	if (intstatus & I_XFU) {
		ET_ERROR(("et%d: transmit fifo underflow\n", etc->unit));
		etc->txuflo++;
	}

	/* if overflows or decriptors underflow, don't report it
	 * as an error and provoque a reset
	 */
	if (intstatus & ~(I_RDU | I_RFO) & I_ERRORS) {
		return (TRUE);
	}

	return (FALSE);
}

static bool
chipdmaerrors(ch_t *ch)
{
	return dma_rxtxerror(ch->di[TX_Q1], TRUE);
}

static void
chipstatsupd(ch_t *ch)
{
	etc_info_t *etc;
	gmacregs_t *regs;
	volatile uint32 *s;
	uint32 *d;

	etc = ch->etc;
	regs = ch->regs;

	/* read the mib counters and update the driver maintained software
	 * counters.
	 */
	if (etc->corerev != GMAC_4706B0_CORE_REV) {
		OR_REG(ch->osh, &regs->devcontrol, DC_MROR);
		for (s = &regs->mib.tx_good_octets, d = &ch->mib.tx_good_octets;
		     s <= &regs->mib.rx_uni_pkts; s++, d++) {
			*d += R_REG(ch->osh, s);
			if (s == &ch->regs->mib.tx_q3_octets_high) {
				s++;
				d++;
			}
		}
	}


	/*
	 * Aggregate transmit and receive errors that probably resulted
	 * in the loss of a frame are computed on the fly.
	 *
	 * We seem to get lots of tx_carrier_lost errors when flipping
	 * speed modes so don't count these as tx errors.
	 *
	 * Arbitrarily lump the non-specific dma errors as tx errors.
	 */
	etc->rxgiants = (ch->di[RX_Q0])->rxgiants;
	etc->txerror = ch->mib.tx_jabber_pkts + ch->mib.tx_oversize_pkts
		+ ch->mib.tx_underruns + ch->mib.tx_excessive_cols
		+ ch->mib.tx_late_cols + etc->txnobuf + etc->dmade
		+ etc->dmada + etc->dmape + etc->txuflo;
	etc->rxerror = ch->mib.rx_jabber_pkts + ch->mib.rx_oversize_pkts
		+ ch->mib.rx_missed_pkts + ch->mib.rx_crc_align_errs
		+ ch->mib.rx_undersize + ch->mib.rx_crc_errs
		+ ch->mib.rx_align_errs + ch->mib.rx_symbol_errs
		+ etc->rxnobuf + etc->rxdmauflo + etc->rxoflo + etc->rxbadlen + etc->rxgiants;
}

static void
chipdumpmib(ch_t *ch, struct bcmstrbuf *b, bool clear)
{
	gmacmib_t *m;

	m = &ch->mib;

	if (clear) {
		bzero((char *)m, sizeof(gmacmib_t));
		return;
	}

	bcm_bprintf(b, "tx_broadcast_pkts %d tx_multicast_pkts %d tx_jabber_pkts %d "
	               "tx_oversize_pkts %d\n",
	               m->tx_broadcast_pkts, m->tx_multicast_pkts,
	               m->tx_jabber_pkts,
	               m->tx_oversize_pkts);
	bcm_bprintf(b, "tx_fragment_pkts %d tx_underruns %d\n",
	               m->tx_fragment_pkts, m->tx_underruns);
	bcm_bprintf(b, "tx_total_cols %d tx_single_cols %d tx_multiple_cols %d "
	               "tx_excessive_cols %d\n",
	               m->tx_total_cols, m->tx_single_cols, m->tx_multiple_cols,
	               m->tx_excessive_cols);
	bcm_bprintf(b, "tx_late_cols %d tx_defered %d tx_carrier_lost %d tx_pause_pkts %d\n",
	               m->tx_late_cols, m->tx_defered, m->tx_carrier_lost,
	               m->tx_pause_pkts);

	/* receive stat counters */
	/* hardware mib pkt and octet counters wrap too quickly to be useful */
	bcm_bprintf(b, "rx_broadcast_pkts %d rx_multicast_pkts %d rx_jabber_pkts %d "
	               "rx_oversize_pkts %d\n",
	               m->rx_broadcast_pkts, m->rx_multicast_pkts,
	               m->rx_jabber_pkts, m->rx_oversize_pkts);
	bcm_bprintf(b, "rx_fragment_pkts %d rx_missed_pkts %d rx_crc_align_errs %d "
	               "rx_undersize %d\n",
	               m->rx_fragment_pkts, m->rx_missed_pkts,
	               m->rx_crc_align_errs, m->rx_undersize);
	bcm_bprintf(b, "rx_crc_errs %d rx_align_errs %d rx_symbol_errs %d\n",
	               m->rx_crc_errs, m->rx_align_errs, m->rx_symbol_errs);
	bcm_bprintf(b, "rx_pause_pkts %d rx_nonpause_pkts %d\n",
	               m->rx_pause_pkts, m->rx_nonpause_pkts);
}

static void
chipenablepme(ch_t *ch)
{
	return;
}

static void
chipdisablepme(ch_t *ch)
{
	return;
}

static void
chipduplexupd(ch_t *ch)
{
	uint32 cmdcfg;
	int32 duplex, speed;

	cmdcfg = R_REG(ch->osh, &ch->regs->cmdcfg);

	/* check if duplex mode changed */
	if (ch->etc->duplex && (cmdcfg & CC_HD))
		duplex = 0;
	else if (!ch->etc->duplex && ((cmdcfg & CC_HD) == 0))
		duplex = CC_HD;
	else
		duplex = -1;

	/* check if the speed changed */
	speed = ((cmdcfg & CC_ES_MASK) >> CC_ES_SHIFT);
	if ((ch->etc->speed == 1000) && (speed != 2))
		speed = 2;
	else if ((ch->etc->speed == 100) && (speed != 1))
		speed = 1;
	else if ((ch->etc->speed == 10) && (speed != 0))
		speed = 0;
	else
		speed = -1;

	/* no duplex or speed change required */
	if ((speed == -1) && (duplex == -1))
		return;

	/* update the speed */
	if (speed != -1) {
		cmdcfg &= ~CC_ES_MASK;
		cmdcfg |= (speed << CC_ES_SHIFT);
	}

	/* update the duplex mode */
	if (duplex != -1) {
		cmdcfg &= ~CC_HD;
		cmdcfg |= duplex;
	}

	ET_TRACE(("chipduplexupd: updating speed & duplex %x\n", cmdcfg));

	/* put mac in reset */
	gmac_init_reset(ch);

	W_REG(ch->osh, &ch->regs->cmdcfg, cmdcfg);

	/* bring mac out of reset */
	gmac_clear_reset(ch);
}

static uint16
chipphyrd(ch_t *ch, uint phyaddr, uint reg)
{
	uint32 tmp;
	gmacregs_t *regs;
	uint32 *phycontrol_addr, *phyaccess_addr;

	if (GMAC_NS_COREREV(ch->etc->corerev)) {
		ET_ERROR(("et%d: chipphyrd: not supported\n", ch->etc->unit));
		return 0;
	}

	ASSERT(phyaddr < MAXEPHY);
	ASSERT(reg < MAXPHYREG);

	regs = ch->regs;

	if (ch->etc->corerev == GMAC_4706B0_CORE_REV) {
		phycontrol_addr = (uint32 *)&ch->regscomm->phycontrol;
		phyaccess_addr = (uint32 *)&ch->regscomm->phyaccess;
	} else {
		phycontrol_addr = (uint32 *)&regs->phycontrol;
		phyaccess_addr = (uint32 *)&regs->phyaccess;
	}

	/* issue the read */
	tmp = R_REG(ch->osh, phycontrol_addr);
	tmp &= ~0x1f;
	tmp |= phyaddr;
	W_REG(ch->osh, phycontrol_addr, tmp);
	W_REG(ch->osh, phyaccess_addr,
	      (PA_START | (phyaddr << PA_ADDR_SHIFT) | (reg << PA_REG_SHIFT)));

	/* wait for it to complete */
	SPINWAIT((R_REG(ch->osh, phyaccess_addr) & PA_START), 1000);
	tmp = R_REG(ch->osh, phyaccess_addr);
	if (tmp & PA_START) {
		ET_ERROR(("et%d: chipphyrd: did not complete\n", ch->etc->unit));
		tmp = 0xffff;
	}

	return (tmp & PA_DATA_MASK);
}

static void
chipphywr(ch_t *ch, uint phyaddr, uint reg, uint16 v)
{
	uint32 tmp;
	gmacregs_t *regs;
	uint32 *phycontrol_addr, *phyaccess_addr;

	if (GMAC_NS_COREREV(ch->etc->corerev)) {
		ET_ERROR(("et%d: chipphywr: not supported\n", ch->etc->unit));
		return;
	}

	ASSERT(phyaddr < MAXEPHY);
	ASSERT(reg < MAXPHYREG);

	regs = ch->regs;

	if (ch->etc->corerev == GMAC_4706B0_CORE_REV) {
		phycontrol_addr = (uint32 *)&ch->regscomm->phycontrol;
		phyaccess_addr = (uint32 *)&ch->regscomm->phyaccess;
	} else {
		phycontrol_addr = (uint32 *)&regs->phycontrol;
		phyaccess_addr = (uint32 *)&regs->phyaccess;
	}

	/* clear mdioint bit of intstatus first  */
	tmp = R_REG(ch->osh, phycontrol_addr);
	tmp &= ~0x1f;
	tmp |= phyaddr;
	W_REG(ch->osh, phycontrol_addr, tmp);
	W_REG(ch->osh, &regs->intstatus, I_MDIO);
	ASSERT((R_REG(ch->osh, &regs->intstatus) & I_MDIO) == 0);

	/* issue the write */
	W_REG(ch->osh, phyaccess_addr,
	      (PA_START | PA_WRITE | (phyaddr << PA_ADDR_SHIFT) | (reg << PA_REG_SHIFT) | v));

	/* wait for it to complete */
	SPINWAIT((R_REG(ch->osh, phyaccess_addr) & PA_START), 1000);
	if (R_REG(ch->osh, phyaccess_addr) & PA_START) {
		ET_ERROR(("et%d: chipphywr: did not complete\n", ch->etc->unit));
	}
}

static void
chipphyor(ch_t *ch, uint phyaddr, uint reg, uint16 v)
{
	uint16 tmp;

	tmp = chipphyrd(ch, phyaddr, reg);
	tmp |= v;
	chipphywr(ch, phyaddr, reg, tmp);
}

static void
chipconfigtimer(ch_t *ch, uint microsecs)
{
	ASSERT(ch->etc->bp_ticks_usec != 0);

	/* Enable general purpose timer in periodic mode */
	W_REG(ch->osh, &ch->regs->gptimer, microsecs * ch->etc->bp_ticks_usec);
}

static void
chipphyreset(ch_t *ch, uint phyaddr)
{
	ASSERT(phyaddr < MAXEPHY);

	if (phyaddr == EPHY_NOREG)
		return;

	ET_TRACE(("et%d: chipphyreset: phyaddr %d\n", ch->etc->unit, phyaddr));

	chipphywr(ch, phyaddr, 0, CTL_RESET);
	OSL_DELAY(100);
	if (chipphyrd(ch, phyaddr, 0) & CTL_RESET) {
		ET_ERROR(("et%d: chipphyreset: reset not complete\n", ch->etc->unit));
	}

	chipphyinit(ch, phyaddr);
}

static void
chipphyinit(ch_t *ch, uint phyaddr)
{
	if (CHIPID(ch->sih->chip) == BCM5356_CHIP_ID) {
		int i;

		for (i = 0; i < 5; i++) {
			chipphywr(ch, i, 0x1f, 0x008b);
			chipphywr(ch, i, 0x15, 0x0100);
			chipphywr(ch, i, 0x1f, 0x000f);
			chipphywr(ch, i, 0x12, 0x2aaa);
			chipphywr(ch, i, 0x1f, 0x000b);
		}
	}

	if ((((CHIPID(ch->sih->chip) == BCM5357_CHIP_ID) ||
	      (CHIPID(ch->sih->chip) == BCM4749_CHIP_ID)) &&
	     (ch->sih->chippkg != BCM47186_PKG_ID)) ||
	    ((CHIPID(ch->sih->chip) == BCM53572_CHIP_ID) &&
	     (ch->sih->chippkg != BCM47188_PKG_ID))) {
		int i;

		/* Clear ephy power down bits in case it was set for coma mode */
		if (PMUCTL_ENAB(ch->sih)) {
			si_pmu_chipcontrol(ch->sih, 2, 0xc0000000, 0);
			si_pmu_chipcontrol(ch->sih, 4, 0x80000000, 0);
		}

		for (i = 0; i < 5; i++) {
			chipphywr(ch, i, 0x1f, 0x000f);
			chipphywr(ch, i, 0x16, 0x5284);
			chipphywr(ch, i, 0x1f, 0x000b);
			chipphywr(ch, i, 0x17, 0x0010);
			chipphywr(ch, i, 0x1f, 0x000f);
			chipphywr(ch, i, 0x16, 0x5296);
			chipphywr(ch, i, 0x17, 0x1073);
			chipphywr(ch, i, 0x17, 0x9073);
			chipphywr(ch, i, 0x16, 0x52b6);
			chipphywr(ch, i, 0x17, 0x9273);
			chipphywr(ch, i, 0x1f, 0x000b);
		}
	}

	if (phyaddr == EPHY_NOREG)
		return;

	ET_TRACE(("et%d: chipphyinit: phyaddr %d\n", ch->etc->unit, phyaddr));
}

static void
chipphyforce(ch_t *ch, uint phyaddr)
{
	etc_info_t *etc;
	uint16 ctl;

	ASSERT(phyaddr < MAXEPHY);

	if (phyaddr == EPHY_NOREG)
		return;

	etc = ch->etc;

	if (etc->forcespeed == ET_AUTO)
		return;

	ET_TRACE(("et%d: chipphyforce: phyaddr %d speed %d\n",
	          ch->etc->unit, phyaddr, etc->forcespeed));

	ctl = chipphyrd(ch, phyaddr, 0);
	ctl &= ~(CTL_SPEED | CTL_SPEED_MSB | CTL_ANENAB | CTL_DUPLEX);

	switch (etc->forcespeed) {
		case ET_10HALF:
			break;

		case ET_10FULL:
			ctl |= CTL_DUPLEX;
			break;

		case ET_100HALF:
			ctl |= CTL_SPEED_100;
			break;

		case ET_100FULL:
			ctl |= (CTL_SPEED_100 | CTL_DUPLEX);
			break;

		case ET_1000FULL:
			ctl |= (CTL_SPEED_1000 | CTL_DUPLEX);
			break;
	}

	chipphywr(ch, phyaddr, 0, ctl);
}

/* set selected capability bits in autonegotiation advertisement */
static void
chipphyadvertise(ch_t *ch, uint phyaddr)
{
	etc_info_t *etc;
	uint16 adv, adv2;

	ASSERT(phyaddr < MAXEPHY);

	if (phyaddr == EPHY_NOREG)
		return;

	etc = ch->etc;

	if ((etc->forcespeed != ET_AUTO) || !etc->needautoneg)
		return;

	ASSERT(etc->advertise);

	ET_TRACE(("et%d: chipphyadvertise: phyaddr %d advertise %x\n",
	          ch->etc->unit, phyaddr, etc->advertise));

	/* reset our advertised capabilitity bits */
	adv = chipphyrd(ch, phyaddr, 4);
	adv &= ~(ADV_100FULL | ADV_100HALF | ADV_10FULL | ADV_10HALF);
	adv |= etc->advertise;
	chipphywr(ch, phyaddr, 4, adv);

	adv2 = chipphyrd(ch, phyaddr, 9);
	adv2 &= ~(ADV_1000FULL | ADV_1000HALF);
	adv2 |= etc->advertise2;
	chipphywr(ch, phyaddr, 9, adv2);

	ET_TRACE(("et%d: chipphyadvertise: phyaddr %d adv %x adv2 %x phyad0 %x\n",
	          ch->etc->unit, phyaddr, adv, adv2, chipphyrd(ch, phyaddr, 0)));

	/* restart autonegotiation */
	chipphyor(ch, phyaddr, 0, CTL_RESTART);

	etc->needautoneg = FALSE;
}

void
chipunitmap(uint coreunit, uint *unit)
{
#ifdef ETFA
	*unit = fa_core2unit(si_kattach(SI_OSH), coreunit);
#endif
}
