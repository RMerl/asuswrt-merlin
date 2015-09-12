/*
 * Broadcom Home Networking Division 10/100 Mbit/s Ethernet core.
 *
 * This file implements the chip-specific routines
 * for Broadcom HNBU Sonics SiliconBackplane enet cores.
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
 * $Id: etc47xx.c 573045 2015-07-21 20:17:41Z $
 */

#include <et_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <bcmdefs.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <proto/ethernet.h>
#include <siutils.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <et_dbg.h>
#include <hndsoc.h>
#include <bcmenet47xx.h>
#include <et_export.h>		/* for et_phyxx() routines */

#ifdef ETROBO
#include <bcmrobo.h>
#endif /* ETROBO */
#ifdef ETADM
#include <etc_adm.h>
#endif /* ETADM */

struct bcm4xxx;	/* forward declaration */
#define ch_t	struct bcm4xxx
#include <etc.h>

/* private chip state */
struct bcm4xxx {
	void 		*et;		/* pointer to et private state */
	etc_info_t	*etc;		/* pointer to etc public state */

	bcmenetregs_t	*regs;		/* pointer to chip registers */
	osl_t 		*osh;		/* os handle */

	void 		*etphy;		/* pointer to et for shared mdc/mdio contortion */

	uint32		intstatus;	/* saved interrupt condition bits */
	uint32		intmask;	/* current software interrupt mask */

	hnddma_t	*di;		/* dma engine software state */

	bool		mibgood;	/* true once mib registers have been cleared */
	bcmenetmib_t	mib;		/* mib counters */
	si_t 		*sih;		/* si utils handle */

	char		*vars;		/* sprom name=value */
	uint		vars_size;

	void		*adm;		/* optional admtek private data */
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
static void chipintrson(ch_t *ch);
static void chipintrsoff(ch_t *ch);
static void chiptxreclaim(ch_t *ch, bool all);
static void chiprxreclaim(ch_t *ch);
static void chipstatsupd(ch_t *ch);
static void chipdumpmib(ch_t *ch, struct bcmstrbuf *b, bool clear);
static void chipenablepme(ch_t *ch);
static void chipdisablepme(ch_t *ch);
static void chipconfigtimer(ch_t *ch, uint microsecs);
static void chipphyreset(ch_t *ch, uint phyaddr);
static void chipphyinit(ch_t *ch, uint phyaddr);
static uint16 chipphyrd(ch_t *ch, uint phyaddr, uint reg);
static void chipdump(ch_t *ch, struct bcmstrbuf *b);
static void chiplongname(ch_t *ch, char *buf, uint bufsize);
static void chipduplexupd(ch_t *ch);
static void chipwrcam(struct bcm4xxx *ch, struct ether_addr *ea, uint camindex);
static void chipphywr(struct bcm4xxx *ch, uint phyaddr, uint reg, uint16 v);
static void chipphyor(struct bcm4xxx *ch, uint phyaddr, uint reg, uint16 v);
static void chipphyand(struct bcm4xxx *ch, uint phyaddr, uint reg, uint16 v);
static void chipphyforce(struct bcm4xxx *ch, uint phyaddr);
static void chipphyadvertise(struct bcm4xxx *ch, uint phyaddr);
static uint chipmacrd(struct bcm4xxx *ch, uint reg);
static void chipmacwr(struct bcm4xxx *ch, uint reg, uint val);
#ifdef BCMDBG
static void chipdumpregs(struct bcm4xxx *ch, bcmenetregs_t *regs, struct bcmstrbuf *b);
#endif /* BCMDBG */

/* chip interrupt bit error summary */
#define	I_ERRORS	(I_PC | I_PD | I_DE | I_RU | I_RO | I_XU)
#define	DEF_INTMASK	(I_XI | I_RI | I_ERRORS)

struct chops bcm47xx_et_chops = {
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
	NULL,
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
	chipduplexupd
};

static uint devices[] = {
	BCM47XX_ENET_ID,
	0x0000 };

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
	struct bcm4xxx *ch;
	bcmenetregs_t *regs;
	char name[16];
	char *var;
	uint boardflags, boardtype;

	ET_TRACE(("et%d: chipattach: regsva 0x%lx\n", etc->unit, (ulong)regsva));

	if ((ch = (struct bcm4xxx *)MALLOC(osh, sizeof(struct bcm4xxx))) == NULL) {
		ET_ERROR(("et%d: chipattach: out of memory, malloced %d bytes\n", etc->unit,
		          MALLOCED(osh)));
		return (NULL);
	}
	bzero((char *)ch, sizeof(struct bcm4xxx));

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

	/* We used to have an assert here like:
	 *	si_coreid(ch->sih) == ENET_CORE_ID
	 * but srom-less systems and simulators don't have a way to
	 * provide a default bar0window so we were relying on nvram
	 * variables. At some point we decided that we could do away
	 * with that since the wireless driver was simply doing a
	 * setcore in attach. So we need to do the same here for
	 * the ethernet.
	 */
	if ((regs = (bcmenetregs_t *)si_setcore(ch->sih, ENET_CORE_ID, etc->unit)) == NULL) {
		ET_ERROR(("et%d: chipattach: Could not setcore to the ENET core\n", etc->unit));
		goto fail;
	}

	ch->regs = regs;
	etc->chip = ch->sih->chip;
	etc->chiprev = ch->sih->chiprev;
	etc->coreid = si_coreid(ch->sih);
	etc->corerev = si_corerev(ch->sih);
	etc->nicmode = !(ch->sih->bustype == SI_BUS);
	etc->coreunit = si_coreunit(ch->sih);
	etc->boardflags = getintvar(ch->vars, "boardflags");

	etc->hwrxoff = HWRXOFF;

	boardflags = etc->boardflags;
	boardtype = ch->sih->boardtype;

	/* Backplane clock ticks per microsecs: used by gptimer, intrecvlazy */
	etc->bp_ticks_usec = si_clock(ch->sih) / 1000000;

	/* get our local ether addr */
	sprintf(name, "et%dmacaddr", etc->coreunit);
	var = getvar(ch->vars, name);
	if (var == NULL) {
		ET_ERROR(("et%d: chipattach: NVRAM_GET(%s) not found\n", etc->unit, name));
		goto fail;
	}
	bcm_ether_atoe(var, &etc->perm_etheraddr);

	if (ETHER_ISNULLADDR(&etc->perm_etheraddr)) {
		ET_ERROR(("et%d: chipattach: invalid format: %s=%s\n", etc->unit, name, var));
		goto fail;
	}
	bcopy((char *)&etc->perm_etheraddr, (char *)&etc->cur_etheraddr, ETHER_ADDR_LEN);

	/*
	 * Too much can go wrong in scanning MDC/MDIO playing "whos my phy?" .
	 * Instead, explicitly require the environment var "et<coreunit>phyaddr=<val>".
	 */

	/* get our phyaddr value */
	sprintf(name, "et%dphyaddr", etc->coreunit);
	var = getvar(ch->vars, name);
	if (var == NULL) {
		ET_ERROR(("et%d: chipattach: NVRAM_GET(%s) not found\n", etc->unit, name));
		goto fail;
	}
	etc->phyaddr = bcm_atoi(var) & EPHY_MASK;

	/* nvram says no phy is present */
	if (etc->phyaddr == EPHY_NONE) {
		ET_ERROR(("et%d: chipattach: phy not present\n", etc->unit));
		goto fail;
	}

	/* get our mdc/mdio port number */
	sprintf(name, "et%dmdcport", etc->coreunit);
	var = getvar(ch->vars, name);
	if (var == NULL) {
		ET_ERROR(("et%d: chipattach: NVRAM_GET(%s) not found\n", etc->unit, name));
		goto fail;
	}
	etc->mdcport = bcm_atoi(var);

	/* configure pci core */
	si_pci_setup(ch->sih, (1 << si_coreidx(ch->sih)));

	/* reset the enet core */
	chipreset(ch);

	/* dma attach */
	sprintf(name, "et%d", etc->coreunit);
	if ((ch->di = dma_attach(osh, name, ch->sih,
	                         (void *)&regs->dmaregs.xmt, (void *)&regs->dmaregs.rcv,
	                         NTXD, NRXD, RXBUFSZ, -1, NRXBUFPOST, HWRXOFF,
	                         &et_msg_level)) == NULL) {
		ET_ERROR(("et%d: chipattach: dma_attach failed\n", etc->unit));
		goto fail;
	}
	etc->txavail[TX_Q0] = (uint *)&ch->di->txavail;

	/* set default sofware intmask */
	ch->intmask = DEF_INTMASK;

	/*
	 * For the 5222 dual phy shared mdio contortion, our phy is
	 * on someone elses mdio pins.  This other enet enet
	 * may not yet be attached so we must defer the et_phyfind().
	 */
	/* if local phy: reset it once now */
	if (etc->mdcport == etc->coreunit)
		chipphyreset(ch, etc->phyaddr);

#ifdef ETROBO
	/*
	 * Broadcom Robo ethernet switch.
	 */
	if ((boardflags & BFL_ENETROBO) &&
	    (etc->phyaddr == EPHY_NOREG)) {
		/* Attach to the switch */
		if (!(etc->robo = bcm_robo_attach(ch->sih, ch, ch->vars,
		                                  (miird_f)bcm47xx_et_chops.phyrd,
		                                  (miiwr_f)bcm47xx_et_chops.phywr))) {
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

	return ((void *)ch);

fail:
	chipdetach(ch);
	return (NULL);
}

static void
chipdetach(struct bcm4xxx *ch)
{
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

	/* free dma state */
	if (ch->di)
		dma_detach(ch->di);
	ch->di = NULL;

	/* put the core back into reset */
	if (ch->sih)
		si_core_disable(ch->sih, 0);

	/* free si handle */
	si_detach(ch->sih);
	ch->sih = NULL;

	/* free vars */
	if (ch->vars)
		MFREE(ch->osh, ch->vars, ch->vars_size);

	/* free chip private state */
	MFREE(ch->osh, ch, sizeof(struct bcm4xxx));
}

static void
chiplongname(struct bcm4xxx *ch, char *buf, uint bufsize)
{
	char *s;

	switch (ch->etc->deviceid) {
	case BCM47XX_ENET_ID:
	default:
		s = "Broadcom BCM47xx 10/100 Mbps Ethernet Controller";
		break;
	}

	strncpy(buf, s, bufsize);
	buf[bufsize - 1] = '\0';
}

static uint
chipmacrd(struct bcm4xxx *ch, uint offset)
{
	ASSERT(offset < 4096); /* GMAC Register space is 4K size */
	return R_REG(ch->osh, (uint *)((uint)(ch->regs) + offset));
}

static void
chipmacwr(struct bcm4xxx *ch, uint offset, uint val)
{
	ASSERT(offset < 4096); /* GMAC Register space is 4K size */
	W_REG(ch->osh, (uint *)((uint)(ch->regs) + offset), val);
}

static void
chipdump(struct bcm4xxx *ch, struct bcmstrbuf *b)
{
#ifdef BCMDBG
	bcm_bprintf(b, "regs 0x%x etphy 0x%x ch->intstatus 0x%x intmask 0x%x\n",
		(ulong)ch->regs, (ulong)ch->etphy, ch->intstatus, ch->intmask);
	bcm_bprintf(b, "\n");

	/* dma engine state */
	dma_dump(ch->di, b, FALSE);
	bcm_bprintf(b, "\n");

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
#endif	/* BCMDBG */
}

#ifdef BCMDBG

#define	PRREG(name)	bcm_bprintf(b, #name " 0x%x ", R_REG(ch->osh, &regs->name))
#define	PRMIBREG(name)	bcm_bprintf(b, #name " 0x%x ", R_REG(ch->osh, &regs->mib.name))

static void
chipdumpregs(struct bcm4xxx *ch, bcmenetregs_t *regs, struct bcmstrbuf *b)
{
	uint phyaddr;

	phyaddr = ch->etc->phyaddr;

	PRREG(devcontrol); PRREG(biststatus); PRREG(wakeuplength);
	bcm_bprintf(b, "\n");
	PRREG(intstatus); PRREG(intmask); PRREG(gptimer);
	bcm_bprintf(b, "\n");
	PRREG(emactxmaxburstlen); PRREG(emacrxmaxburstlen);
	PRREG(emaccontrol); PRREG(emacflowcontrol);
	bcm_bprintf(b, "\n");
	PRREG(intrecvlazy);
	bcm_bprintf(b, "\n");

	/* emac registers */
	PRREG(rxconfig); PRREG(rxmaxlength); PRREG(txmaxlength);
	bcm_bprintf(b, "\n");
	PRREG(mdiocontrol); PRREG(camcontrol); PRREG(enetcontrol);
	bcm_bprintf(b, "\n");
	PRREG(txcontrol); PRREG(txwatermark); PRREG(mibcontrol);
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
	PRMIBREG(tx_defered); PRMIBREG(tx_carrier_lost); PRMIBREG(tx_pause_pkts);
	bcm_bprintf(b, "\n");

	PRMIBREG(rx_good_octets); PRMIBREG(rx_good_pkts); PRMIBREG(rx_octets); PRMIBREG(rx_pkts);
	bcm_bprintf(b, "\n");
	PRMIBREG(rx_broadcast_pkts); PRMIBREG(rx_multicast_pkts);
	bcm_bprintf(b, "\n");
	PRMIBREG(rx_jabber_pkts); PRMIBREG(rx_oversize_pkts); PRMIBREG(rx_fragment_pkts);
	bcm_bprintf(b, "\n");
	PRMIBREG(rx_missed_pkts); PRMIBREG(rx_crc_align_errs); PRMIBREG(rx_undersize);
	bcm_bprintf(b, "\n");
	PRMIBREG(rx_crc_errs); PRMIBREG(rx_align_errs); PRMIBREG(rx_symbol_errs);
	bcm_bprintf(b, "\n");
	PRMIBREG(rx_pause_pkts); PRMIBREG(rx_nonpause_pkts);
	bcm_bprintf(b, "\n");

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

#define	MDC_RATIO	5000000

static void
chipreset(struct bcm4xxx *ch)
{
	bcmenetregs_t *regs;
	uint32 clk, mdc;

	ET_TRACE(("et%d: chipreset\n", ch->etc->unit));

	regs = ch->regs;

	if (!si_iscoreup(ch->sih)) {
		if (!ch->etc->nicmode)
			si_pci_setup(ch->sih, (1 << si_coreidx(ch->sih)));
		/* power on reset: reset the enet core */
		si_core_reset(ch->sih, 0, 0);
		goto chipinreset;
	}

	/* read counters before resetting the chip */
	if (ch->mibgood)
		chipstatsupd(ch);

	/* reset the tx dma engine */
	if (ch->di)
		dma_txreset(ch->di);

	/* set emac into loopback mode to ensure no rx traffic */
	W_REG(ch->osh, &regs->rxconfig, ERC_LE);
	OSL_DELAY(1);

	/* reset the rx dma engine */
	if (ch->di)
		dma_rxreset(ch->di);

	/* reset core */
	si_core_reset(ch->sih, 0, 0);

chipinreset:

	/* must clear mib registers by hand */
	W_REG(ch->osh, &regs->mibcontrol, EMC_RZ);
	(void) R_REG(ch->osh, &regs->mib.tx_broadcast_pkts);
	(void) R_REG(ch->osh, &regs->mib.tx_multicast_pkts);
	(void) R_REG(ch->osh, &regs->mib.tx_len_64);
	(void) R_REG(ch->osh, &regs->mib.tx_len_65_to_127);
	(void) R_REG(ch->osh, &regs->mib.tx_len_128_to_255);
	(void) R_REG(ch->osh, &regs->mib.tx_len_256_to_511);
	(void) R_REG(ch->osh, &regs->mib.tx_len_512_to_1023);
	(void) R_REG(ch->osh, &regs->mib.tx_len_1024_to_max);
	(void) R_REG(ch->osh, &regs->mib.tx_jabber_pkts);
	(void) R_REG(ch->osh, &regs->mib.tx_oversize_pkts);
	(void) R_REG(ch->osh, &regs->mib.tx_fragment_pkts);
	(void) R_REG(ch->osh, &regs->mib.tx_underruns);
	(void) R_REG(ch->osh, &regs->mib.tx_total_cols);
	(void) R_REG(ch->osh, &regs->mib.tx_single_cols);
	(void) R_REG(ch->osh, &regs->mib.tx_multiple_cols);
	(void) R_REG(ch->osh, &regs->mib.tx_excessive_cols);
	(void) R_REG(ch->osh, &regs->mib.tx_late_cols);
	(void) R_REG(ch->osh, &regs->mib.tx_defered);
	(void) R_REG(ch->osh, &regs->mib.tx_carrier_lost);
	(void) R_REG(ch->osh, &regs->mib.tx_pause_pkts);
	(void) R_REG(ch->osh, &regs->mib.rx_broadcast_pkts);
	(void) R_REG(ch->osh, &regs->mib.rx_multicast_pkts);
	(void) R_REG(ch->osh, &regs->mib.rx_len_64);
	(void) R_REG(ch->osh, &regs->mib.rx_len_65_to_127);
	(void) R_REG(ch->osh, &regs->mib.rx_len_128_to_255);
	(void) R_REG(ch->osh, &regs->mib.rx_len_256_to_511);
	(void) R_REG(ch->osh, &regs->mib.rx_len_512_to_1023);
	(void) R_REG(ch->osh, &regs->mib.rx_len_1024_to_max);
	(void) R_REG(ch->osh, &regs->mib.rx_jabber_pkts);
	(void) R_REG(ch->osh, &regs->mib.rx_oversize_pkts);
	(void) R_REG(ch->osh, &regs->mib.rx_fragment_pkts);
	(void) R_REG(ch->osh, &regs->mib.rx_missed_pkts);
	(void) R_REG(ch->osh, &regs->mib.rx_crc_align_errs);
	(void) R_REG(ch->osh, &regs->mib.rx_undersize);
	(void) R_REG(ch->osh, &regs->mib.rx_crc_errs);
	(void) R_REG(ch->osh, &regs->mib.rx_align_errs);
	(void) R_REG(ch->osh, &regs->mib.rx_symbol_errs);
	(void) R_REG(ch->osh, &regs->mib.rx_pause_pkts);
	(void) R_REG(ch->osh, &regs->mib.rx_nonpause_pkts);
	ch->mibgood = TRUE;

	/*
	 * We want the phy registers to be accessible even when
	 * the driver is "downed" so initialize MDC preamble, frequency,
	 * and whether internal or external phy here.
	 */
	/* default:  100Mhz SI clock and external phy */
	W_REG(ch->osh, &regs->mdiocontrol, 0x94);
	if (ch->etc->deviceid == BCM47XX_ENET_ID) {
		/* 47xx chips: find out the clock */
		if ((clk = si_clock(ch->sih)) != 0) {
			mdc = 0x80 | ((clk + (MDC_RATIO / 2)) / MDC_RATIO);
			W_REG(ch->osh, &regs->mdiocontrol, mdc);
		} else {
			ET_ERROR(("et%d: chipreset: Could not figure out backplane clock, "
			          "using 100Mhz\n",
			          ch->etc->unit));
		}
	}

	/* some chips have internal phy, some don't */
	if (!(R_REG(ch->osh, &regs->devcontrol) & DC_IP)) {
		W_REG(ch->osh, &regs->enetcontrol, EC_EP);
	} else if (R_REG(ch->osh, &regs->devcontrol) & DC_ER) {
		AND_REG(ch->osh, &regs->devcontrol, ~DC_ER);
		OSL_DELAY(100);
		chipphyinit(ch, ch->etc->phyaddr);
	}

	/* clear persistent sw intstatus */
	ch->intstatus = 0;
}

/*
 * Initialize all the chip registers.  If dma mode, init tx and rx dma engines
 * but leave the devcontrol tx and rx (fifos) disabled.
 */
static void
chipinit(struct bcm4xxx *ch, uint options)
{
	etc_info_t *etc;
	bcmenetregs_t *regs;
	uint idx;
	uint i;

	regs = ch->regs;
	etc = ch->etc;
	idx = 0;

	ET_TRACE(("et%d: chipinit\n", etc->unit));

	/* enable crc32 generation */
	OR_REG(ch->osh, &regs->emaccontrol, EMC_CG);

	/* enable one rx interrupt per received frame */
	W_REG(ch->osh, &regs->intrecvlazy, (1 << IRL_FC_SHIFT));

	/* enable 802.3x tx flow control (honor received PAUSE frames) */
	W_REG(ch->osh, &regs->rxconfig, ERC_FE | ERC_UF);

	/* initialize CAM */
	if (etc->promisc || (R_REG(ch->osh, &regs->rxconfig) & ERC_CA))
		OR_REG(ch->osh, &regs->rxconfig, ERC_PE);
	else {
		/* our local address */
		chipwrcam(ch, &etc->cur_etheraddr, idx++);

		/* allmulti or a list of discrete multicast addresses */
		if (etc->allmulti)
			OR_REG(ch->osh, &regs->rxconfig, ERC_AM);
		else if (etc->nmulticast) {
			for (i = 0; i < etc->nmulticast; i++)
				chipwrcam(ch, &etc->multicast[i], idx++);
		}

		/* enable cam */
		OR_REG(ch->osh, &regs->camcontrol, CC_CE);
	}

	/* optionally enable mac-level loopback */
	if (etc->loopbk)
		OR_REG(ch->osh, &regs->rxconfig, ERC_LE);

	/* set max frame lengths - account for possible vlan tag */
	W_REG(ch->osh, &regs->rxmaxlength, ETHER_MAX_LEN + 32);
	W_REG(ch->osh, &regs->txmaxlength, ETHER_MAX_LEN + 32);

	/* set tx watermark */
	W_REG(ch->osh, &regs->txwatermark, 56);

	/*
	 * Optionally, disable phy autonegotiation and force our speed/duplex
	 * or constrain our advertised capabilities.
	 */
	if (etc->forcespeed != ET_AUTO)
		chipphyforce(ch, etc->phyaddr);
	else if (etc->advertise && etc->needautoneg)
		chipphyadvertise(ch, etc->phyaddr);

	if (options & ET_INIT_FULL) {
		/* initialize the tx and rx dma channels */
		dma_txinit(ch->di);
		dma_rxinit(ch->di);

		/* post dma receive buffers */
		dma_rxfill(ch->di);

		/* lastly, enable interrupts */
		if (options & ET_INIT_INTRON)
			et_intrson(etc->et);
	}
	else
		dma_rxenable(ch->di);

	/* turn on the emac */
	OR_REG(ch->osh, &regs->enetcontrol, EC_EE);
}

/* dma transmit */
static bool BCMFASTPATH
chiptx(struct bcm4xxx *ch, void *p0)
{
	int error;

	ET_TRACE(("et%d: chiptx\n", ch->etc->unit));
	ET_LOG("et%d: chiptx", ch->etc->unit, 0);

	error = dma_txfast(ch->di, p0, TRUE);

	if (error) {
		ET_ERROR(("et%d: chiptx: out of txds\n", ch->etc->unit));
		ch->etc->txnobuf++;
		return FALSE;
	}
	return TRUE;
}

/* reclaim completed transmit descriptors and packets */
static void BCMFASTPATH
chiptxreclaim(struct bcm4xxx *ch, bool forceall)
{
	ET_TRACE(("et%d: chiptxreclaim\n", ch->etc->unit));
	dma_txreclaim(ch->di, forceall ? HNDDMA_RANGE_ALL : HNDDMA_RANGE_TRANSMITTED);
	ch->intstatus &= ~I_XI;
}

/* dma receive: returns a pointer to the next frame received, or NULL if there are no more */
static void * BCMFASTPATH
chiprx(struct bcm4xxx *ch)
{
	void *p;

	ET_TRACE(("et%d: chiprx\n", ch->etc->unit));
	ET_LOG("et%d: chiprx", ch->etc->unit, 0);

	if ((p = dma_rx(ch->di)) == NULL)
		ch->intstatus &= ~I_RI;

	return (p);
}

static int BCMFASTPATH
chiprxquota(ch_t *ch, int quota, void **rxpkts)
{
	int rxcnt;
	void * pkt;
	uint8 * addr;

	ET_TRACE(("et%d: chiprxquota\n", ch->etc->unit));
	ET_LOG("et%d: chiprxquota", ch->etc->unit, 0);

	rxcnt = 0;

	while ((quota > 0) && ((pkt = dma_rx(ch->di)) != NULL)) {
		addr = PKTDATA(ch->osh, pkt);
#if !defined(_CFE_)
		bcm_prefetch_32B(addr + 32, 1);
#endif /* _CFE_ */
		rxpkts[rxcnt] = pkt;
		rxcnt++;
		quota--;
	}

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
chiprxreclaim(struct bcm4xxx *ch)
{
	ET_TRACE(("et%d: chiprxreclaim\n", ch->etc->unit));
	dma_rxreclaim(ch->di);
	ch->intstatus &= ~I_RI;
}

/* allocate and post dma receive buffers */
static void BCMFASTPATH
chiprxfill(struct bcm4xxx *ch)
{
	ET_TRACE(("et%d: chiprxfill\n", ch->etc->unit));
	ET_LOG("et%d: chiprx", ch->etc->unit, 0);
	dma_rxfill(ch->di);
}

/* get current and pending interrupt events */
static int BCMFASTPATH
chipgetintrevents(struct bcm4xxx *ch, bool in_isr)
{
	bcmenetregs_t *regs;
	uint32 intstatus;
	int events;

	regs = ch->regs;
	events = 0;

	/* read the interrupt status register */
	intstatus = R_REG(ch->osh, &regs->intstatus);

	/* defer unsolicited interrupts */
	intstatus &= (in_isr ? ch->intmask : DEF_INTMASK);

	/* clear non-error interrupt conditions */
	if (intstatus != 0) {
		W_REG(ch->osh, &regs->intstatus, intstatus);
		events = INTR_NEW;
	}

	/* or new bits into persistent intstatus */
	intstatus = (ch->intstatus |= intstatus);

	/* return if no events */
	if (intstatus == 0)
		return (0);

	/* convert chip-specific intstatus bits into generic intr event bits */
	if (intstatus & I_RI)
		events |= INTR_RX;
	if (intstatus & I_XI)
		events |= INTR_TX;
	if (intstatus & I_ERRORS)
		events |= INTR_ERROR;
	if (intstatus & I_TO)
		events |= INTR_TO;

	return (events);
}

/* enable chip interrupts */
static void BCMFASTPATH
chipintrson(struct bcm4xxx *ch)
{
	ch->intmask = DEF_INTMASK;
	W_REG(ch->osh, &ch->regs->intmask, ch->intmask);
}

/* disable chip interrupts */
static void BCMFASTPATH
chipintrsoff(struct bcm4xxx *ch)
{
	W_REG(ch->osh, &ch->regs->intmask, 0);
	(void) R_REG(ch->osh, &ch->regs->intmask);	/* sync readback */
	ch->intmask = 0;
}

/* return true of caller should re-initialize, otherwise false */
static bool BCMFASTPATH
chiperrors(struct bcm4xxx *ch)
{
	uint32 intstatus;
	etc_info_t *etc;

	etc = ch->etc;

	intstatus = ch->intstatus;
	ch->intstatus &= ~(I_ERRORS);

	ET_TRACE(("et%d: chiperrors: intstatus 0x%x\n", etc->unit, intstatus));

	if (intstatus & I_PC) {
		ET_ERROR(("et%d: descriptor error\n", etc->unit));
		etc->dmade++;
	}

	if (intstatus & I_PD) {
		ET_ERROR(("et%d: data error\n", etc->unit));
		etc->dmada++;
	}

	if (intstatus & I_DE) {
		ET_ERROR(("et%d: descriptor protocol error\n", etc->unit));
		etc->dmape++;
	}
	/* NOTE : this ie NOT an error. It becomes an error only
	 * when the rx fifo overflows
	 */
	if (intstatus & I_RU) {
		ET_ERROR(("et%d: receive descriptor underflow\n", etc->unit));
		etc->rxdmauflo++;
	}

	if (intstatus & I_RO) {
		ET_ERROR(("et%d: receive fifo overflow\n", etc->unit));
		etc->rxoflo++;
	}

	if (intstatus & I_XU) {
		ET_ERROR(("et%d: transmit fifo underflow\n", etc->unit));
		etc->txuflo++;
	}
	/* if overflows or decriptors underflow, don't report it
	 * as an error and  provoque a reset
	 */
	if (intstatus & ~(I_RU) & I_ERRORS)
		return (TRUE);
	return FALSE;
}

static void
chipwrcam(struct bcm4xxx *ch, struct ether_addr *ea, uint camindex)
{
	uint32 w;

	ASSERT((R_REG(ch->osh, &ch->regs->camcontrol) & (CC_CB | CC_CE)) == 0);

	w = (ea->octet[2] << 24) | (ea->octet[3] << 16) | (ea->octet[4] << 8)
		| ea->octet[5];
	W_REG(ch->osh, &ch->regs->camdatalo, w);
	w = CD_V | (ea->octet[0] << 8) | ea->octet[1];
	W_REG(ch->osh, &ch->regs->camdatahi, w);
	W_REG(ch->osh, &ch->regs->camcontrol, ((camindex << CC_INDEX_SHIFT) | CC_WR));

	/* spin until done */
	SPINWAIT((R_REG(ch->osh, &ch->regs->camcontrol) & CC_CB), 1000);

	/*
	 * This assertion is usually caused by the phy not providing a clock
	 * to the bottom portion of the mac..
	 */
	ASSERT((R_REG(ch->osh, &ch->regs->camcontrol) & CC_CB) == 0);
}

static void
chipstatsupd(struct bcm4xxx *ch)
{
	etc_info_t *etc;
	bcmenetregs_t *regs;
	bcmenetmib_t *m;

	etc = ch->etc;
	regs = ch->regs;
	m = &ch->mib;

	/*
	 * mib counters are clear-on-read.
	 * Don't bother using the pkt and octet counters since they are only
	 * 16bits and wrap too quickly to be useful.
	 */
	m->tx_broadcast_pkts += R_REG(ch->osh, &regs->mib.tx_broadcast_pkts);
	m->tx_multicast_pkts += R_REG(ch->osh, &regs->mib.tx_multicast_pkts);
	m->tx_len_64 += R_REG(ch->osh, &regs->mib.tx_len_64);
	m->tx_len_65_to_127 += R_REG(ch->osh, &regs->mib.tx_len_65_to_127);
	m->tx_len_128_to_255 += R_REG(ch->osh, &regs->mib.tx_len_128_to_255);
	m->tx_len_256_to_511 += R_REG(ch->osh, &regs->mib.tx_len_256_to_511);
	m->tx_len_512_to_1023 += R_REG(ch->osh, &regs->mib.tx_len_512_to_1023);
	m->tx_len_1024_to_max += R_REG(ch->osh, &regs->mib.tx_len_1024_to_max);
	m->tx_jabber_pkts += R_REG(ch->osh, &regs->mib.tx_jabber_pkts);
	m->tx_oversize_pkts += R_REG(ch->osh, &regs->mib.tx_oversize_pkts);
	m->tx_fragment_pkts += R_REG(ch->osh, &regs->mib.tx_fragment_pkts);
	m->tx_underruns += R_REG(ch->osh, &regs->mib.tx_underruns);
	m->tx_total_cols += R_REG(ch->osh, &regs->mib.tx_total_cols);
	m->tx_single_cols += R_REG(ch->osh, &regs->mib.tx_single_cols);
	m->tx_multiple_cols += R_REG(ch->osh, &regs->mib.tx_multiple_cols);
	m->tx_excessive_cols += R_REG(ch->osh, &regs->mib.tx_excessive_cols);
	m->tx_late_cols += R_REG(ch->osh, &regs->mib.tx_late_cols);
	m->tx_defered += R_REG(ch->osh, &regs->mib.tx_defered);
	m->tx_carrier_lost += R_REG(ch->osh, &regs->mib.tx_carrier_lost);
	m->tx_pause_pkts += R_REG(ch->osh, &regs->mib.tx_pause_pkts);
	m->rx_broadcast_pkts += R_REG(ch->osh, &regs->mib.rx_broadcast_pkts);
	m->rx_multicast_pkts += R_REG(ch->osh, &regs->mib.rx_multicast_pkts);
	m->rx_len_64 += R_REG(ch->osh, &regs->mib.rx_len_64);
	m->rx_len_65_to_127 += R_REG(ch->osh, &regs->mib.rx_len_65_to_127);
	m->rx_len_128_to_255 += R_REG(ch->osh, &regs->mib.rx_len_128_to_255);
	m->rx_len_256_to_511 += R_REG(ch->osh, &regs->mib.rx_len_256_to_511);
	m->rx_len_512_to_1023 += R_REG(ch->osh, &regs->mib.rx_len_512_to_1023);
	m->rx_len_1024_to_max += R_REG(ch->osh, &regs->mib.rx_len_1024_to_max);
	m->rx_jabber_pkts += R_REG(ch->osh, &regs->mib.rx_jabber_pkts);
	m->rx_oversize_pkts += R_REG(ch->osh, &regs->mib.rx_oversize_pkts);
	m->rx_fragment_pkts += R_REG(ch->osh, &regs->mib.rx_fragment_pkts);
	m->rx_missed_pkts += R_REG(ch->osh, &regs->mib.rx_missed_pkts);
	m->rx_crc_align_errs += R_REG(ch->osh, &regs->mib.rx_crc_align_errs);
	m->rx_undersize += R_REG(ch->osh, &regs->mib.rx_undersize);
	m->rx_crc_errs += R_REG(ch->osh, &regs->mib.rx_crc_errs);
	m->rx_align_errs += R_REG(ch->osh, &regs->mib.rx_align_errs);
	m->rx_symbol_errs += R_REG(ch->osh, &regs->mib.rx_symbol_errs);
	m->rx_pause_pkts += R_REG(ch->osh, &regs->mib.rx_pause_pkts);
	m->rx_nonpause_pkts += R_REG(ch->osh, &regs->mib.rx_nonpause_pkts);

	/*
	 * Aggregate transmit and receive errors that probably resulted
	 * in the loss of a frame are computed on the fly.
	 *
	 * We seem to get lots of tx_carrier_lost errors when flipping
	 * speed modes so don't count these as tx errors.
	 *
	 * Arbitrarily lump the non-specific dma errors as tx errors.
	 */
	etc->rxgiants = ch->di->rxgiants;
	etc->txerror = m->tx_jabber_pkts + m->tx_oversize_pkts
		+ m->tx_underruns + m->tx_excessive_cols
		+ m->tx_late_cols + etc->txnobuf + etc->dmade
		+ etc->dmada + etc->dmape + etc->txuflo;
	etc->rxerror = m->rx_jabber_pkts + m->rx_oversize_pkts
		+ m->rx_missed_pkts + m->rx_crc_align_errs
		+ m->rx_undersize + m->rx_crc_errs
		+ m->rx_align_errs + m->rx_symbol_errs
		+ etc->rxnobuf + etc->rxdmauflo + etc->rxoflo + etc->rxbadlen + etc->rxgiants;
}

static void
chipdumpmib(ch_t *ch, struct bcmstrbuf *b, bool clear)
{
	bcmenetmib_t *m;

	m = &ch->mib;

	if (clear) {
		bzero((char *)m, sizeof(bcmenetmib_t));
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
chipenablepme(struct bcm4xxx *ch)
{
	bcmenetregs_t *regs;

	regs = ch->regs;

	/* enable chip wakeup pattern matching */
	OR_REG(ch->osh, &regs->devcontrol, DC_PM);

	/* enable sonics bus PME */
	si_core_cflags(ch->sih, SICF_PME_EN, SICF_PME_EN);
}

static void
chipdisablepme(struct bcm4xxx *ch)
{
	bcmenetregs_t *regs;

	regs = ch->regs;

	AND_REG(ch->osh, &regs->devcontrol, ~DC_PM);
	si_core_cflags(ch->sih, SICF_PME_EN, 0);
}

static void
chipduplexupd(struct bcm4xxx *ch)
{
	uint32 txcontrol;

	txcontrol = R_REG(ch->osh, &ch->regs->txcontrol);
	if (ch->etc->duplex && !(txcontrol & EXC_FD))
		OR_REG(ch->osh, &ch->regs->txcontrol, EXC_FD);
	else if (!ch->etc->duplex && (txcontrol & EXC_FD))
		AND_REG(ch->osh, &ch->regs->txcontrol, ~EXC_FD);
}

static uint16
chipphyrd(struct bcm4xxx *ch, uint phyaddr, uint reg)
{
	bcmenetregs_t *regs;

	ASSERT(phyaddr < MAXEPHY);

	/*
	 * BCM5222 dualphy shared mdio contortion.
	 * remote phy: another emac controls our phy.
	 */
	if (ch->etc->mdcport != ch->etc->coreunit) {
		if (ch->etphy == NULL) {
			ch->etphy = et_phyfind(ch->et, ch->etc->mdcport);

			/* first time reset */
			if (ch->etphy)
				chipphyreset(ch, ch->etc->phyaddr);
		}
		if (ch->etphy)
			return (et_phyrd(ch->etphy, phyaddr, reg));
		else
			return (0xffff);
	}

	/* local phy: our emac controls our phy */

	regs = ch->regs;

	/* clear mii_int */
	W_REG(ch->osh, &regs->emacintstatus, EI_MII);

	/* issue the read */
	W_REG(ch->osh, &regs->mdiodata,  (MD_SB_START | MD_OP_READ | (phyaddr << MD_PMD_SHIFT)
		| (reg << MD_RA_SHIFT) | MD_TA_VALID));

	/* wait for it to complete */
	SPINWAIT(((R_REG(ch->osh, &regs->emacintstatus) & EI_MII) == 0), 100);
	if ((R_REG(ch->osh, &regs->emacintstatus) & EI_MII) == 0) {
		ET_ERROR(("et%d: chipphyrd: did not complete\n", ch->etc->unit));
	}

	return (R_REG(ch->osh, &regs->mdiodata) & MD_DATA_MASK);
}

static void
chipphywr(struct bcm4xxx *ch, uint phyaddr, uint reg, uint16 v)
{
	bcmenetregs_t *regs;

	ASSERT(phyaddr < MAXEPHY);

	/*
	 * BCM5222 dualphy shared mdio contortion.
	 * remote phy: another emac controls our phy.
	 */
	if (ch->etc->mdcport != ch->etc->coreunit) {
		if (ch->etphy == NULL)
			ch->etphy = et_phyfind(ch->et, ch->etc->mdcport);
		if (ch->etphy)
			et_phywr(ch->etphy, phyaddr, reg, v);
		return;
	}

	/* local phy: our emac controls our phy */

	regs = ch->regs;

	/* clear mii_int */
	W_REG(ch->osh, &regs->emacintstatus, EI_MII);
	ASSERT((R_REG(ch->osh, &regs->emacintstatus) & EI_MII) == 0);

	/* issue the write */
	W_REG(ch->osh, &regs->mdiodata,  (MD_SB_START | MD_OP_WRITE | (phyaddr << MD_PMD_SHIFT)
		| (reg << MD_RA_SHIFT) | MD_TA_VALID | v));

	/* wait for it to complete */
	SPINWAIT(((R_REG(ch->osh, &regs->emacintstatus) & EI_MII) == 0), 100);
	if ((R_REG(ch->osh, &regs->emacintstatus) & EI_MII) == 0) {
		ET_ERROR(("et%d: chipphywr: did not complete\n", ch->etc->unit));
	}
}

static void
chipphyor(struct bcm4xxx *ch, uint phyaddr, uint reg, uint16 v)
{
	uint16 tmp;

	tmp = chipphyrd(ch, phyaddr, reg);
	tmp |= v;
	chipphywr(ch, phyaddr, reg, tmp);
}

static void
chipphyand(struct bcm4xxx *ch, uint phyaddr, uint reg, uint16 v)
{
	uint16 tmp;

	tmp = chipphyrd(ch, phyaddr, reg);
	tmp &= v;
	chipphywr(ch, phyaddr, reg, tmp);
}

static void
chipconfigtimer(struct bcm4xxx *ch, uint microsecs)
{
	ASSERT(ch->etc->bp_ticks_usec != 0);

	/* Enable general purpose timer in periodic mode */
	W_REG(ch->osh, &ch->regs->gptimer, microsecs * ch->etc->bp_ticks_usec);
}

static void
chipphyreset(struct bcm4xxx *ch, uint phyaddr)
{
	ASSERT(phyaddr < MAXEPHY);

	if (phyaddr == EPHY_NOREG)
		return;

	chipphywr(ch, phyaddr, 0, CTL_RESET);
	OSL_DELAY(100);
	if (chipphyrd(ch, phyaddr, 0) & CTL_RESET) {
		ET_ERROR(("et%d: chipphyreset: reset not complete\n", ch->etc->unit));
	}

	chipphyinit(ch, phyaddr);
}

static void
chipphyinit(struct bcm4xxx *ch, uint phyaddr)
{
	uint	phyid = 0;

	/* enable activity led */
	chipphyand(ch, phyaddr, 26, 0x7fff);

	/* enable traffic meter led mode */
	chipphyor(ch, phyaddr, 27, (1 << 6));

	phyid = chipphyrd(ch, phyaddr, 0x2);
	phyid |=  chipphyrd(ch, phyaddr, 0x3) << 16;
	if (phyid == 0x55210022) {
		chipphywr(ch, phyaddr, 30, (uint16) (chipphyrd(ch, phyaddr, 30) | 0x3000));
		chipphywr(ch, phyaddr, 22, (uint16) (chipphyrd(ch, phyaddr, 22) & 0xffdf));
	}
}

static void
chipphyforce(struct bcm4xxx *ch, uint phyaddr)
{
	etc_info_t *etc;
	uint16 ctl;

	ASSERT(phyaddr < MAXEPHY);

	if (phyaddr == EPHY_NOREG)
		return;

	etc = ch->etc;

	if (etc->forcespeed == ET_AUTO)
		return;

	ctl = chipphyrd(ch, phyaddr, 0);
	ctl &= ~(CTL_SPEED | CTL_ANENAB | CTL_DUPLEX);

	switch (etc->forcespeed) {
	case ET_10HALF:
		break;

	case ET_10FULL:
		ctl |= CTL_DUPLEX;
		break;

	case ET_100HALF:
		ctl |= CTL_SPEED;
		break;

	case ET_100FULL:
		ctl |= (CTL_SPEED | CTL_DUPLEX);
		break;
	}

	chipphywr(ch, phyaddr, 0, ctl);
}

/* set selected capability bits in autonegotiation advertisement */
static void
chipphyadvertise(struct bcm4xxx *ch, uint phyaddr)
{
	etc_info_t *etc;
	uint16 adv;

	ASSERT(phyaddr < MAXEPHY);

	if (phyaddr == EPHY_NOREG)
		return;

	etc = ch->etc;

	if ((etc->forcespeed != ET_AUTO) || !etc->needautoneg)
		return;

	ASSERT(etc->advertise);

	/* reset our advertised capabilitity bits */
	adv = chipphyrd(ch, phyaddr, 4);
	adv &= ~(ADV_100FULL | ADV_100HALF | ADV_10FULL | ADV_10HALF);
	adv |= etc->advertise;
	chipphywr(ch, phyaddr, 4, adv);

	/* restart autonegotiation */
	chipphyor(ch, phyaddr, 0, CTL_RESTART);

	etc->needautoneg = FALSE;
}
