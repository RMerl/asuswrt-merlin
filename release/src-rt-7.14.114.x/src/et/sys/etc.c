/*
 * Common [OS-independent] portion of
 * Broadcom Home Networking Division 10/100 Mbit/s Ethernet
 * Device Driver.
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
 * $Id: etc.c 550727 2015-04-21 10:54:31Z $
 */

#include <et_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <proto/ethernet.h>
#include <proto/vlan.h>
#include <proto/bcmip.h>
#include <proto/802.1d.h>
#include <bcmenetmib.h>
#include <bcmenetrxh.h>
#include <bcmenetphy.h>
#include <et_dbg.h>
#include <etc.h>
#include <et_export.h>
#include <bcmutils.h>
#include <hndsoc.h>
#include <hndfwd.h>
#ifdef ETFA
#include <etc_fa.h>
#endif

#define ETC_TXERR_COUNTDOWN 3

#ifdef ETROBO
#ifndef	_siutils_h_
typedef const struct si_pub  si_t;
#endif
#include <bcmrobo.h>
#endif /* ETROBO */

uint32 et_msg_level =
#ifdef BCMDBG
	1;
#else
	0;
#endif /* BCMDBG */

/* local prototypes */
static void etc_loopback(etc_info_t *etc, int on);
static void etc_dumpetc(etc_info_t *etc, struct bcmstrbuf *b);
static void et_dump_arl_tbl(etc_info_t *etc, uint porv, uint num, struct bcmstrbuf *b);
static void et_dump_port_status(etc_info_t *etc, uint portnum, struct bcmstrbuf *b);
int et_get_portinfo(etc_info_t *etc, int port_id, et_sw_port_info_t *portstatus);

/* 802.1d priority to traffic class mapping. queues correspond one-to-one
 * with traffic classes.
 */
uint32 up2tc[NUMPRIO] = {
	TC_BE,  	/* 0    BE    TC_BE    Best Effort */
	TC_BK,  	/* 1    BK    TC_BK    Background */
	TC_BK,  	/* 2    --    TC_BK    Background */
	TC_BE,  	/* 3    EE    TC_BE    Best Effort */
	TC_CL,  	/* 4    CL    TC_CL    Controlled Load */
	TC_CL,  	/* 5    VI    TC_CL    Controlled Load */
	TC_VO,  	/* 6    VO    TC_VO    Voice */
	TC_VO   	/* 7    NC    TC_VO    Voice */
};

uint32 priq_selector[] = {
	[0x0] = TC_NONE, [0x1] = TC_BK, [0x2] = TC_BE, [0x3] = TC_BE,
	[0x4] = TC_CL,   [0x5] = TC_CL, [0x6] = TC_CL, [0x7] = TC_CL,
	[0x8] = TC_VO,   [0x9] = TC_VO, [0xa] = TC_VO, [0xb] = TC_VO,
	[0xc] = TC_VO,   [0xd] = TC_VO, [0xe] = TC_VO, [0xf] = TC_VO
};

/* find the chip opsvec for this chip */
struct chops*
etc_chipmatch(uint vendor, uint device)
{
#if !defined(_CFE_) || defined(CFG_ETC47XX)
	{
		extern struct chops bcm47xx_et_chops;

		if (bcm47xx_et_chops.id(vendor, device))
			return (&bcm47xx_et_chops);
	}
#endif

#ifdef CFG_GMAC
	{
		extern struct chops bcmgmac_et_chops;

		if (bcmgmac_et_chops.id(vendor, device))
			return (&bcmgmac_et_chops);
	}
#endif /* CFG_GMAC */
	return (NULL);
}

void*
etc_attach(void *et, uint vendor, uint device, uint coreunit, void *osh, void *regsva)
{
	etc_info_t *etc;
	uint unit = coreunit;

	/* get unit from coreunit */
	etc_unitmap(vendor, device, coreunit, &unit);

	ET_TRACE(("et%d: etc_attach: vendor 0x%x device 0x%x\n", unit, vendor, device));

	/* some code depends on packed structures */
	ASSERT(sizeof(struct ether_addr) == ETHER_ADDR_LEN);
	ASSERT(sizeof(struct ether_header) == ETHER_HDR_LEN);

	/* allocate etc_info_t state structure */
	if ((etc = (etc_info_t*) MALLOC(osh, sizeof(etc_info_t))) == NULL) {
		ET_ERROR(("et%d: etc_attach: out of memory, malloced %d bytes\n", unit,
		          MALLOCED(osh)));
		return (NULL);
	}
	bzero((char*)etc, sizeof(etc_info_t));

	etc->et = et;
	etc->unit = unit;
	etc->coreunit = coreunit;
	etc->osh = osh;
	etc->vendorid = (uint16) vendor;
	etc->deviceid = (uint16) device;
	etc->forcespeed = ET_AUTO;
	etc->linkstate = FALSE;

#ifdef ET_INGRESS_QOS
	etc->dma_rx_thresh = DMA_RX_THRESH_DEFAULT;
	etc->dma_rx_policy = DMA_RX_POLICY_NONE;
#endif /* ET_INGRESS_QOS */

#ifdef PKTC
	/* initialize default pktc values */
	etc->pktcbnd = MAX(PKTCBND, RXBND);
#endif
	etc_quota(etc);

	/* set chip opsvec */
	etc->chops = etc_chipmatch(vendor, device);
	ASSERT(etc->chops);

	/* chip attach */
	if ((etc->ch = (*etc->chops->attach)(etc, osh, regsva)) == NULL) {
		ET_ERROR(("et%d: chipattach error\n", unit));
		goto fail;
	}

	return ((void*)etc);

fail:
	etc_detach(etc);
	return (NULL);
}

void
etc_detach(etc_info_t *etc)
{
	if (etc == NULL)
		return;

	/* free chip private state */
	if (etc->ch) {
		(*etc->chops->detach)(etc->ch);
		etc->chops = etc->ch = NULL;
	}

	MFREE(etc->osh, etc, sizeof(etc_info_t));
}

void
etc_reset(etc_info_t *etc)
{
	ET_TRACE(("et%d: etc_reset\n", etc->unit));

	etc->reset++;

	/* reset the chip */
	(*etc->chops->reset)(etc->ch);

	/* free any posted tx packets */
	(*etc->chops->txreclaim)(etc->ch, TRUE);

#ifdef DMA
	/* free any posted rx packets */
	(*etc->chops->rxreclaim)(etc->ch);
#endif /* DMA */
}

void
etc_init(etc_info_t *etc, uint options)
{
	ET_TRACE(("et%d: etc_init\n", etc->unit));

	ASSERT(etc->pioactive == NULL);
#if defined(BCM_GMAC3)
	if (DEV_NTKIF(etc))
		ASSERT(!ETHER_ISNULLADDR(&etc->cur_etheraddr));
#else  /* ! BCM_GMAC3 */
	ASSERT(!ETHER_ISNULLADDR(&etc->cur_etheraddr));
#endif /* ! BCM_GMAC3 */
	ASSERT(!ETHER_ISMULTI(&etc->cur_etheraddr));

	/* init the chip */
	(*etc->chops->init)(etc->ch, options);
}

/* mark interface up */
void
etc_up(etc_info_t *etc)
{
	etc->up = TRUE;

	et_init(etc->et, ET_INIT_FULL | ET_INIT_INTRON);
}

/* mark interface down */
uint
etc_down(etc_info_t *etc, int reset)
{
	uint callback;

	callback = 0;

	ET_FLAG_DOWN(etc);

	if (reset)
		et_reset(etc->et);

	/* suppress link state changes during power management mode changes */
	if (etc->linkstate) {
		etc->linkstate = FALSE;
		if (!etc->pm_modechange)
			et_link_down(etc->et);
	}

	return (callback);
}

#ifdef CONFIG_DUMP_PREV_OOPS_MSG
extern void dump_previous_oops(void);
#endif

/* common iovar handler. return 0=ok, -1=error */
int
etc_iovar(etc_info_t *etc, uint cmd, uint set, void *arg, int len)
{
	int error;
	uint *vecarg;
#if defined(ETROBO) && !defined(_CFE_)
	int i;
	robo_info_t *robo = etc->robo;
#endif /* ETROBO && _CFE_ */

	error = 0;
	vecarg = (uint *)arg;
	ET_TRACE(("et%d: etc_iovar: cmd 0x%x\n", etc->unit, cmd));

	if ((cmd != IOV_ET_CLEAR_DUMP) && (arg == NULL)) {
		/* arg can be NULL only for IOV_ET_CLEAR_DUMP. In all
		* other cases buffer should be passed correctly
		*/
		return -1;
	}
	switch (cmd) {
#if defined(ETROBO) && !defined(_CFE_)
		case IOV_ET_POWER_SAVE_MODE:
			if (set)
				error = robo_power_save_mode_set(robo, vecarg[1], vecarg[0]);
			else {
				/* get power save mode of all the phys */
				if (vecarg[0] == MAX_NO_PHYS) {
					for (i = 0; i < MAX_NO_PHYS; i++)
						vecarg[i] = robo_power_save_mode_get(robo, i);
					break;
				}

				/* get power save mode of the phy */
				error = robo_power_save_mode_get(robo, vecarg[0]);
				if (error != -1) {
					vecarg[1] = error;
					error = 0;
				}
			}
			break;

		case IOV_ET_ROBO_DEVID:
			error = -1;

			if (robo != NULL) {
				*vecarg = robo->devid;
				error = 0;
			}
			break;
#endif /* ETROBO && !_CFE_ */
#ifdef BCMDBG
		case IOV_ET_CLEAR_DUMP:
			if (set) {
				uint size = ((char *)(&etc->rxbadlen) - (char *)(&etc->txframe));

				bzero((char *)&etc->txframe, size + sizeof(etc->rxbadlen));
				(*etc->chops->dumpmib)(etc->ch, NULL, TRUE);
				error = 0;
			}
			break;
#endif /* BCMDBG */

		case IOV_PKTC:
			if (set)
				etc->pktc = *vecarg;
			else
				*vecarg = (uint)etc->pktc;
			etc_quota(etc);
			break;

		case IOV_PKTCBND:
			if (set)
				etc->pktcbnd = MAX(*vecarg, 32);
			else
				*vecarg = etc->pktcbnd;
			etc_quota(etc);
			break;
#ifdef ET_INGRESS_QOS
		case IOV_DMA_RX_THRESH:
			if (set)
				/* Don't let the thresh to claim all descriptors */
				etc->dma_rx_thresh = MIN(*vecarg, NRXD-12);
			else
				*vecarg = etc->dma_rx_thresh;
			break;

		case IOV_DMA_RX_POLICY:
			if (set) {
				if (*vecarg > DMA_RX_POLICY_LAST) {
					error = -1;
					break;
				}
#ifdef ETROBO
				ASSERT(robo);

				if (*vecarg) {
					/* Disable flow control for IMP port */
					error = bcm_robo_flow_control(robo, FALSE);
				}
				else {
					/* Enable flow control for IMP port */
					error = bcm_robo_flow_control(robo, TRUE);
				}

				if (error != 0)
					break;
#else
				/* TBD: flow control on non-ROBO switches */
				error = -1;
				break;
#endif /* ETROBO */
				etc->dma_rx_policy = *vecarg;
			}
			else
				*vecarg = etc->dma_rx_policy;
			break;
#endif /* ET_INGRESS_QOS */

		case IOV_RXQUOTA:
			if (set)
				etc->quota = *vecarg;
			else
				*vecarg = etc->quota;
			etc_quota(etc);
			break;

		case IOV_RXLAZYTO: /* rxlazy timeout */
			if (set)
				etc->rxlazy_timeout = *vecarg;
			else
				*vecarg = etc->rxlazy_timeout;
			break;

		case IOV_RXLAZYFC: /* rxlazy framecnt */
			if (set)
				etc->rxlazy_framecnt = *vecarg;
			else
				*vecarg = etc->rxlazy_framecnt;
			break;

		case IOV_COUNTERS:
			{
				struct bcmstrbuf b;
				bcm_binit(&b, (char*)arg, len);
				etc_dumpetc(etc, &b);
			}
			break;

#ifdef CONFIG_DUMP_PREV_OOPS_MSG
		case IOV_DUMP_OOPS:
			dump_previous_oops();
			break;
#endif

#ifdef HNDCTF
		case IOV_DUMP_CTF:
			{
				struct bcmstrbuf b;
				bcm_binit(&b, (char*)arg, len);
				et_dump_ctf(etc->et, &b);
			}
			break;
#endif /* HNDCTF */

#ifdef BCMDBG_CTRACE
		case IOV_DUMP_CTRACE:
			{
				struct bcmstrbuf b;
				bcm_binit(&b, (char*)arg, len);
				et_dump_ctrace(etc->et, &b);
			}
			break;
#endif /* BCMDBG_CTRACE */

#ifdef BCMDBG
		case IOV_MACRD:
			{
				uint val;
				val = (*etc->chops->macrd)(etc->ch, vecarg[0]);
				ET_TRACE(("etc_ioctl: ETCMACRD reg_off 0x%x => 0x%x\n",
				          vecarg[0], val));
				vecarg[0] = val;
			}
			break;

		case IOV_MACWR:
			{
				ET_TRACE(("etc_ioctl: ETCMACWR reg_off 0x%x <= 0x%x\n",
				          vecarg[0], vecarg[1]));
				(*etc->chops->macwr)(etc->ch, vecarg[0], vecarg[1]);
			}
			break;
#endif /* BCMDBG */

		case IOV_DUMP:
		if (et_msg_level & 0x10000)
			bcmdumplog((char *)arg, len);
#ifdef BCMDBG
		else
		{
			struct bcmstrbuf b;
			bcm_binit(&b, (char*)arg, len);
			et_dump(etc->et, &b);
		}
#endif /* BCMDBG */
		break;

#ifdef ETFA
		case IOV_FA_DUMP:
			{
				struct bcmstrbuf b;
				bcm_binit(&b, (char*)arg, len);
				fa_dump(etc->fa, &b, FALSE);
			}
			break;
#endif /* ETFA */

#ifdef BCM_GMAC3
		case IOV_DUMP_FWDER:
			{
				struct bcmstrbuf b;
				bcm_binit(&b, (char*)arg, len);
				et_dump_fwder(etc->et, &b);
			}
			break;
#endif /* BCM_GMAC3 */

		case IOV_PORTSTATS:
			{
				struct bcmstrbuf b;
				bcm_binit(&b, (char*)arg, IOCBUFSZ);
				et_dump_port_status(etc, *vecarg, &b);
			}
			break;

		case IOV_SW_MCTBL:
			{
				struct bcmstrbuf b;
				uint num, pov;
				bcm_binit(&b, (char*)arg, IOCBUFSZ);
				pov = (*vecarg & 0x1ffff) >> 16;
				num = *vecarg & 0xffff;
				et_dump_arl_tbl(etc, pov, num, &b);
			}
			break;

		case IOV_CAP:
			{
				struct bcmstrbuf b;
				bcm_binit(&b, (char*)arg, len);
#ifdef ETFA
				/* Do we need to consider the HW cap and nvram ctf_fa_mode? */
				bcm_bprintf(&b, "fa ");
#endif
#ifdef BCM_GMAC3
				if (!getvar(NULL, "gmac3_off"))
					bcm_bprintf(&b, "gmac3 ");
#endif
			}
			break;

		default:
			error = -1;
	}

	return (error);
}

/* common ioctl handler.  return: 0=ok, -1=error */
int
etc_ioctl(etc_info_t *etc, int cmd, void *arg)
{
	int error;
	int val;
	int *vec = (int*)arg;

	error = 0;

	val = arg ? *(int*)arg : 0;

	ET_TRACE(("et%d: etc_ioctl: cmd 0x%x\n", etc->unit, cmd));

	switch (cmd) {
	case ETCUP:
		et_up(etc->et);
		break;

	case ETCDOWN:
		et_down(etc->et, TRUE);
		break;

	case ETCLOOP:
		etc_loopback(etc, val);
		break;

	case ETCDUMP:
		if (et_msg_level & 0x10000)
			bcmdumplog((char *)arg, IOCBUFSZ);
#ifdef BCMDBG
		else
		{
			struct bcmstrbuf b;
			bcm_binit(&b, (char*)arg, IOCBUFSZ);
			et_dump(etc->et, &b);
		}
#endif /* BCMDBG */
		break;

	case ETCSETMSGLEVEL:
		et_msg_level = val;
		break;

	case ETCPROMISC:
		etc_promisc(etc, val);
		break;

	case ETCQOS:
		etc_qos(etc, val);
		break;

	case ETCSPEED:
		if (etc->phyaddr == EPHY_NOREG) {
			/* Don't configure the MII interface speed through ioctl */
			goto err;
		}
		if (val == ET_1000FULL) {
			etc->speed = 1000;
			etc->duplex = 1;
		} else if (val == ET_1000HALF) {
			etc->speed = 1000;
			etc->duplex = 0;
		} else if (val == ET_100FULL) {
			etc->speed = 100;
			etc->duplex = 1;
		} else if (val == ET_100HALF) {
			etc->speed = 100;
			etc->duplex = 0;
		} else if (val == ET_10FULL) {
			etc->speed = 10;
			etc->duplex = 1;
		} else if (val == ET_10HALF) {
			etc->speed = 10;
			etc->duplex = 0;
		} else if (val == ET_AUTO)
			;
		else
			goto err;

		etc->forcespeed = val;

		/* explicitly reset the phy */
		(*etc->chops->phyreset)(etc->ch, etc->phyaddr);

		/* request restart autonegotiation if we're reverting to adv mode */
		if (etc->forcespeed == ET_AUTO) {
			etc->advertise = (ADV_100FULL | ADV_100HALF |
			                  ADV_10FULL | ADV_10HALF);
			etc->advertise2 = ADV_1000FULL;
			etc->needautoneg = TRUE;
		} else {
			etc->advertise = etc->advertise2 = 0;
			etc->needautoneg = FALSE;
		}

		et_init(etc->et, ET_INIT_INTRON);
		break;

	case ETCPHYRD:
		if (vec) {
			vec[1] = (*etc->chops->phyrd)(etc->ch, etc->phyaddr, vec[0]);
			ET_TRACE(("etc_ioctl: ETCPHYRD of reg 0x%x => 0x%x\n", vec[0], vec[1]));
		}
		break;

	case ETCPHYRD2:
		if (vec) {
			uint phyaddr, reg;
			phyaddr = vec[0] >> 16;
			if (phyaddr < MAXEPHY) {
				reg = vec[0] & 0xffff;
				vec[1] = (*etc->chops->phyrd)(etc->ch, phyaddr, reg);
				ET_TRACE(("etc_ioctl: ETCPHYRD2 of phy 0x%x, reg 0x%x => 0x%x\n",
				          phyaddr, reg, vec[1]));
			}
		}
		break;

	case ETCPHYWR:
		if (vec) {
			ET_TRACE(("etc_ioctl: ETCPHYWR to reg 0x%x <= 0x%x\n", vec[0], vec[1]));
			(*etc->chops->phywr)(etc->ch, etc->phyaddr, vec[0], (uint16)vec[1]);
		}
		break;

	case ETCPHYWR2:
		if (vec) {
			uint phyaddr, reg;
			phyaddr = vec[0] >> 16;
			if (phyaddr < MAXEPHY) {
				reg = vec[0] & 0xffff;
				(*etc->chops->phywr)(etc->ch, phyaddr, reg, (uint16)vec[1]);
				ET_TRACE(("etc_ioctl: ETCPHYWR2 to phy 0x%x, reg 0x%x <= 0x%x\n",
				          phyaddr, reg, vec[1]));
			}
		}
		break;

#ifdef ETROBO
	case ETCROBORD:
		if (etc->robo && vec) {
			uint page, reg;
			uint64 val;
			int len = 2;
			robo_info_t *robo = (robo_info_t *)etc->robo;

			page = vec[0] >> 16;
			reg = vec[0] & 0xffff;
			if ((vec[1] >= 1) && (vec[1] <= 8))
				len = vec[1];
			/* For SPI mode, the length can only be 1, 2, and 4 bytes */
			if ((len > 4) && (!strcmp(robo->ops->desc, "SPI (GPIO)"))) {
				vec[1] = -1;
				break;
			}
			val = 0;
			robo->ops->read_reg(etc->robo, page, reg, &val, len);
			*((unsigned long long *)&vec[2]) = val;
			ET_TRACE(("etc_ioctl: ETCROBORD of page 0x%x, reg 0x%x  => 0x%016llX\n",
			          page, reg, val));
		}
		break;

	case ETCROBOWR:
		if (etc->robo && vec) {
			uint page, reg;
			uint64 val;
			robo_info_t *robo = (robo_info_t *)etc->robo;
			int len = 2;
			page = vec[0] >> 16;
			reg = vec[0] & 0xffff;
			if ((vec[1] >= 1) && (vec[1] <= 8))
				len = vec[1];
			/* For SPI mode, the length can only be 1, 2, and 4 bytes */
			if ((len > 4) && (!strcmp(robo->ops->desc, "SPI (GPIO)"))) {
				vec[1] = -1;
				break;
			}
			val = *((unsigned long long *)&vec[2]);
			robo->ops->write_reg(etc->robo, page, vec[0], &val, len);
			ET_TRACE(("etc_ioctl: ETCROBOWR to page 0x%x, reg 0x%x <= 0x%016llX\n",
			          page, reg, val));
		}
		break;
#endif /* ETROBO */

	default:
	err:
		error = -1;
	}

	return (error);
}

/* called once per second */
void
etc_watchdog(etc_info_t *etc)
{
	uint16 status;
	uint16 lpa;
#if defined(ETROBO)
	robo_info_t *robo = (robo_info_t *)etc->robo;
#endif
#if defined(ETROBO) && !defined(_CFE_)
	static uint32 sleep_timer = PWRSAVE_SLEEP_TIME, wake_timer = 0;
#endif /* ETROBO && !_CFE_ */

	etc->now++;

#if defined(ETROBO)
	/* BCM53125 EEE IOP WAR for some other vendor's wrong EEE implementation. */
	if (robo)
		robo_watchdog(robo);
#endif

#if defined(ETROBO) && !defined(_CFE_)
	/* Every PWRSAVE_WAKE_TIME sec the phys that are in manual mode
	 * is taken out of that mode and link status is checked after
	 * PWRSAVE_SLEEP_TIME sec to see if any of the links is up
	 * to take that port is taken out of the manual power save mode
	 */
	if (robo) {
		if (ROBO_IS_PWRSAVE_MANUAL(robo)) {
			if (etc->now == sleep_timer) {
				robo_power_save_toggle(robo, FALSE);
				wake_timer = sleep_timer + robo->pwrsave_wake_time;
			} else if (etc->now == wake_timer) {
				robo_power_save_toggle(robo, TRUE);
				sleep_timer = wake_timer + robo->pwrsave_sleep_time;
			}
			else if (etc->now > sleep_timer && etc->now > wake_timer) {
				/* Power save may be actived after startup */
				etc->now = sleep_timer - 1;
			}
		}

		/* Apply the auto configuration from the nvram variable in the beginning */
		if ((etc->now == PWRSAVE_WAKE_TIME) && ROBO_IS_PWRSAVE_AUTO(robo)) {
			robo_power_save_mode_update(robo);
		}
	}
#endif /* ETROBO && !_CFE_ */

	/* no local phy registers */
	if (etc->phyaddr == EPHY_NOREG) {
		etc->linkstate = TRUE;
		etc->duplex = 1;
		/* keep emac txcontrol duplex bit consistent with current phy duplex */
		(*etc->chops->duplexupd)(etc->ch);
		return;
	}

	status = (*etc->chops->phyrd)(etc->ch, etc->phyaddr, 1);
	/* check for bad mdio read */
	if (status == 0xffff) {
		ET_ERROR(("et%d: etc_watchdog: bad mdio read: phyaddr %d mdcport %d\n",
			etc->unit, etc->phyaddr, etc->mdcport));
		return;
	}

	if (etc->forcespeed == ET_AUTO) {
		uint16 adv, adv2 = 0, status2 = 0, estatus;

		adv = (*etc->chops->phyrd)(etc->ch, etc->phyaddr, 4);
		lpa = (*etc->chops->phyrd)(etc->ch, etc->phyaddr, 5);

		/* read extended status register. if we are 1000BASE-T
		 * capable then get our advertised capabilities and the
		 * link partner capabilities from 1000BASE-T control and
		 * status registers.
		 */
		estatus = (*etc->chops->phyrd)(etc->ch, etc->phyaddr, 15);
		if ((estatus != 0xffff) && (estatus & EST_1000TFULL)) {
			/* read 1000BASE-T control and status registers */
			adv2 = (*etc->chops->phyrd)(etc->ch, etc->phyaddr, 9);
			status2 = (*etc->chops->phyrd)(etc->ch, etc->phyaddr, 10);
		}

		/* update current speed and duplex */
		if ((adv2 & ADV_1000FULL) && (status2 & LPA_1000FULL)) {
			etc->speed = 1000;
			etc->duplex = 1;
		} else if ((adv2 & ADV_1000HALF) && (status2 & LPA_1000HALF)) {
			etc->speed = 1000;
			etc->duplex = 0;
		} else if ((adv & ADV_100FULL) && (lpa & LPA_100FULL)) {
			etc->speed = 100;
			etc->duplex = 1;
		} else if ((adv & ADV_100HALF) && (lpa & LPA_100HALF)) {
			etc->speed = 100;
			etc->duplex = 0;
		} else if ((adv & ADV_10FULL) && (lpa & LPA_10FULL)) {
			etc->speed = 10;
			etc->duplex = 1;
		} else {
			etc->speed = 10;
			etc->duplex = 0;
		}
	}

	/* monitor link state */
	if (!etc->linkstate && (status & STAT_LINK)) {
		etc->linkstate = TRUE;
		if (etc->pm_modechange)
			etc->pm_modechange = FALSE;
		else
			et_link_up(etc->et);
	} else if (etc->linkstate && !(status & STAT_LINK)) {
		etc->linkstate = FALSE;
		if (!etc->pm_modechange)
			et_link_down(etc->et);
	}

	/* keep emac txcontrol duplex bit consistent with current phy duplex */
	(*etc->chops->duplexupd)(etc->ch);

	/* check for remote fault error */
	if (status & STAT_REMFAULT) {
		ET_ERROR(("et%d: remote fault\n", etc->unit));
	}

	/* check for jabber error */
	if (status & STAT_JAB) {
		ET_ERROR(("et%d: jabber\n", etc->unit));
	}

	/*
	 * Read chip mib counters occationally before the 16bit ones can wrap.
	 * We don't use the high-rate mib counters.
	 */
	if ((etc->now % 30) == 0)
		(*etc->chops->statsupd)(etc->ch);
}

static void
etc_loopback(etc_info_t *etc, int on)
{
	ET_TRACE(("et%d: etc_loopback: %d\n", etc->unit, on));

	etc->loopbk = (bool) on;
	et_init(etc->et, ET_INIT_INTRON);
}

void
etc_promisc(etc_info_t *etc, uint on)
{
	ET_TRACE(("et%d: etc_promisc: %d\n", etc->unit, on));

	etc->promisc = (bool) on;
	et_init(etc->et, ET_INIT_INTRON);
}

void
etc_qos(etc_info_t *etc, uint on)
{
	ET_TRACE(("et%d: etc_qos: %d\n", etc->unit, on));

	etc->qos = (bool) on;
	et_init(etc->et, ET_INIT_INTRON);
}

void
etc_quota(etc_info_t *etc)
{
	int quota = ETCQUOTA_MAX;

#ifdef PKTC
	quota = (etc->pktc) ? etc->pktcbnd : RXBND;
#endif

	/* Cap to ETCQUOTA_MAX */
	etc->quota = (quota > ETCQUOTA_MAX) ? ETCQUOTA_MAX : quota;
}

void
etc_rxlazy(etc_info_t *etc, uint microsecs, uint framecnt)
{
	ASSERT(framecnt >= 1U);
	ASSERT(etc->bp_ticks_usec != 0U);

	etc->rxlazy_timeout = microsecs * etc->bp_ticks_usec;
	etc->rxlazy_framecnt = framecnt;
}

/* WAR: BCM53115 switch is not retaining the tag while forwarding
 * the vlan/priority tagged frames even when tag status preserve
 * is enabled. This problem can be only worked around by doing
 * double tagging for priority tagged frames. This will trick the
 * switch in to just removing the outer tag on the egress. Inner
 * tag remains which contains the prio.
 */
#ifdef ETROBO
void *
etc_bcm53115_war(etc_info_t *etc, void *p)
{
	struct ethervlan_header *evh;
	uint16 vlan_tag;
	int vlan_prio;
	uint8 *data = PKTDATA(etc->osh, p);
	uint8 *ip_body = data + sizeof(struct ethervlan_header);

	evh = (struct ethervlan_header *)data;
	/* No additional TAG added if IPTOS has priority != 0 */
	if ((evh->vlan_type != hton16(ETHER_TYPE_8021Q)) ||
	    (IP_TOS46(ip_body) & IPV4_TOS_PREC_MASK))
		return (p);

	vlan_tag = evh->vlan_tag;
	vlan_prio = vlan_tag & hton16(VLAN_PRI_MASK << VLAN_PRI_SHIFT);

	/* No need to do anything for priority 0 */
	if (vlan_prio == 0)
		return (p);

	/* If the packet is shared or there is not enough headroom
	 * then allocate new header buffer and link the original
	 * buffer to it.
	 */
	if ((PKTHEADROOM(etc->osh, p) < VLAN_TAG_LEN) || PKTSHARED(p)) {
		void *pkt;
		uint16 ether_type;

		if ((pkt = PKTGET(etc->osh, VLAN_TAG_LEN +
		                  ETHERVLAN_HDR_LEN, TRUE)) == NULL) {
			ET_ERROR(("et%d: PKTGET of size %d failed during expand head\n",
			          etc->unit, VLAN_TAG_LEN + ETHERVLAN_HDR_LEN));
			return (NULL);
		}

		/* Assign priority of original frame */
		PKTSETPRIO(pkt, ntoh16(vlan_prio) >> VLAN_PRI_SHIFT);

		ether_type = evh->ether_type;

		/* Copy the vlan header to the first buffer */
		memcpy(PKTDATA(etc->osh, pkt), data, ETHERVLAN_HDR_LEN);
		PKTPULL(etc->osh, p, ETHERVLAN_HDR_LEN);

		/* Align the pointer to initialize the inner vlan tag and type
		 * fields.
		 */
		evh = (struct ethervlan_header *)(PKTDATA(etc->osh, pkt) + VLAN_TAG_LEN);
		evh->vlan_tag = vlan_tag;
		evh->ether_type = ether_type;

		/* Chain the original buffer to new header buffer */
		PKTSETNEXT(etc->osh, pkt, p);

		p = pkt;
	} else {
		data = PKTPUSH(etc->osh, p, VLAN_TAG_LEN);
		ETHERVLAN_MOVE_HDR(data, data + VLAN_TAG_LEN);
		evh = (struct ethervlan_header *)(data + VLAN_TAG_LEN);
	}

	evh->vlan_type = hton16(ETHER_TYPE_8021Q);

	/* Clear the vlan id in the inner tag */
	evh->vlan_tag &= ~(hton16(VLAN_VID_MASK));

	return (p);
}
#endif /* ETROBO */

#ifdef BCMDBG
void
etc_dump(etc_info_t *etc, struct bcmstrbuf *b)
{
	etc_dumpetc(etc, b);
	(*etc->chops->dump)(etc->ch, b);
}
#endif /* BCMDBG */

static void
etc_dumpetc(etc_info_t *etc, struct bcmstrbuf *b)
{
	char perm[32], cur[32];
	uint i;

	bcm_bprintf(b, "etc 0x%x et 0x%x unit %d msglevel %d speed/duplex %d%s\n",
		(ulong)etc, (ulong)etc->et, etc->unit, et_msg_level,
		etc->speed, (etc->duplex ? "full": "half"));
	bcm_bprintf(b, "up %d promisc %d loopbk %d forcespeed %d advertise 0x%x "
	               "advertise2 0x%x needautoneg %d\n",
	               etc->up, etc->promisc, etc->loopbk, etc->forcespeed,
	               etc->advertise, etc->advertise2, etc->needautoneg);
	bcm_bprintf(b, "piomode %d pioactive 0x%x nmulticast %d allmulti %d qos %d\n",
		etc->piomode, (ulong)etc->pioactive, etc->nmulticast, etc->allmulti, etc->qos);
	bcm_bprintf(b, "vendor 0x%x device 0x%x rev %d coreunit %d phyaddr %d mdcport %d\n",
		etc->vendorid, etc->deviceid, etc->chiprev,
		etc->coreunit, etc->phyaddr, etc->mdcport);

	bcm_bprintf(b, "perm_etheraddr %s cur_etheraddr %s\n",
		bcm_ether_ntoa(&etc->perm_etheraddr, perm),
		bcm_ether_ntoa(&etc->cur_etheraddr, cur));

	if (etc->nmulticast) {
		bcm_bprintf(b, "multicast: ");
		for (i = 0; i < etc->nmulticast; i++)
			bcm_bprintf(b, "%s ", bcm_ether_ntoa(&etc->multicast[i], cur));
		bcm_bprintf(b, "\n");
	}

	bcm_bprintf(b, "linkstate %d\n", etc->linkstate);
	bcm_bprintf(b, "\n");

	/* refresh stat counters */
	(*etc->chops->statsupd)(etc->ch);

	/* summary stat counter line */
	/* use sw frame and byte counters -- hw mib counters wrap too quickly */
	bcm_bprintf(b, "txframe %d txbyte %d txerror %d rxframe %d rxbyte %d rxerror %d\n",
		etc->txframe, etc->txbyte, etc->txerror,
		etc->rxframe, etc->rxbyte, etc->rxerror);

	/* transmit & receive stat counters */
	/* hardware mib pkt and octet counters wrap too quickly to be useful */
	(*etc->chops->dumpmib)(etc->ch, b, FALSE);

	bcm_bprintf(b, "txnobuf %d reset %d dmade %d dmada %d dmape %d\n",
	               etc->txnobuf, etc->reset, etc->dmade, etc->dmada, etc->dmape);

	/* hardware mib pkt and octet counters wrap too quickly to be useful */
	bcm_bprintf(b, "rxnobuf %d rxdmauflo %d rxoflo %d rxbadlen %d "
	               "rxgiants %d rxoflodiscards %d\n",
	               etc->rxnobuf, etc->rxdmauflo, etc->rxoflo, etc->rxbadlen,
	               etc->rxgiants, etc->rxoflodiscards);

	bcm_bprintf(b, "chained %d unchained %d maxchainsz %d currchainsz %d\n",
	               etc->chained, etc->unchained, etc->maxchainsz, etc->currchainsz);

#if defined(BCMDBG)
#if defined(PKTC)
	bcm_bprintf(b, "chain sz histo:");
	for (i = 0; i < PKTCBND && etc->chained; i++) {
		bcm_bprintf(b, "  %d(%d%%)", etc->chainsz[i],
		            (etc->chainsz[i] * 100) / etc->chained);
		if (((i % 8) == 7) && (i != PKTCBND - 1))
			bcm_bprintf(b, "\n              :");
	}
#endif /* PKTC */

	bcm_bprintf(b, "\nrx processed :%d", etc->rxprocessed);

	for (i = 0; i < ETCQUOTA_MAX; i++) {
		if (etc->quota_stats[i]) {
			bcm_bprintf(b, "  %d %d\n", i, etc->quota_stats[i]);
		}
	}
	bzero(etc->quota_stats, ETCQUOTA_MAX * sizeof(int));
#endif /* BCMDBG */

	bcm_bprintf(b, "\n");
}

uint
etc_totlen(etc_info_t *etc, void *p)
{
	uint total;

	total = 0;
	for (; p; p = PKTNEXT(etc->osh, p))
		total += PKTLEN(etc->osh, p);
	return (total);
}

static void
et_dump_arl_entry(uint porv, uint num,  uint64 reg_val64, uint32 reg_val32, struct bcmstrbuf *b)
{
	uint32  prtn;
	uint64  vidn;

	vidn = (reg_val64 & VID_MASK) >> 48;
	prtn = (reg_val32 & 0x1ff);

	if ((prtn <= 5) && ((num == 0xff) || (!porv && (vidn == num)) || (porv && (prtn == num)))) {
		bcm_bprintf(b, "%012llx       ", (reg_val64 & MACADDR_MASK));
		bcm_bprintf(b, "0x%0llx", vidn);
		bcm_bprintf(b, "%12d   ", prtn);
		bcm_bprintf(b, "0x%08x\n", reg_val32);
	}
}

static void
et_dump_arl_tbl(etc_info_t *etc, uint porv, uint num, struct bcmstrbuf *b)
{
	uint8	val8 = 0x80;
	uint32  val32;
	uint64  val64;
	robo_info_t *robo = (robo_info_t *)etc->robo;

	/* write 1 to bit 7 Page 5, address 0x50 */
	robo->ops->write_reg(etc->robo, 5, 0x50, &val8, sizeof(val8));

	OSL_DELAY(150);

	bcm_bprintf(b, "MAC ADDRESS        VID        Port        Flags\n");
	bcm_bprintf(b, "-----------        ---        ----        -----\n");

	robo->ops->read_reg(etc->robo, 5, 0x50, &val8, sizeof(uint8));
	val8 &= 0x81;	/* read bit 0 & 7  -- if "1", ARL search is started */
					/* and valid entry is found */

	while (val8) {
		/* MAC-VID  1 */
		robo->ops->read_reg(etc->robo, 5, 0x60, &val64, sizeof(uint64));
		/* reading 0x68 should clears bit 0 of 0x50 & search to cont.. */
		robo->ops->read_reg(etc->robo, 5, 0x68, &val32, sizeof(uint32));

		if (val64 && (val8 & 0x01))
			et_dump_arl_entry(porv, num, val64, val32, b);

		/* MAC-VID  2 */
		robo->ops->read_reg(etc->robo, 5, 0x70, &val64, sizeof(uint64));
		robo->ops->read_reg(etc->robo, 5, 0x78, &val32, sizeof(uint32));
		if (val64 && (val8 & 0x01))
			et_dump_arl_entry(porv, num, val64, val32, b);

		robo->ops->read_reg(etc->robo, 5, 0x50, &val8, sizeof(uint8));
		if ((val8 & 0x80) == 0) {
			break;
		}

		if ((val8 & 0x01) == 0) {
			robo->ops->read_reg(etc->robo, 5, 0x50, &val8, sizeof(uint8));
		}

		val8 = (val8 & 0x81);
	}
}

static void
et_dump_port_status(etc_info_t *etc, uint portn, struct bcmstrbuf *b)
{
	uint16 i, val;
	uint pstart, pend;
	et_sw_port_info_t portinfo;
	char *phy_speed[4] = {"Unknown", "100", "1000", "10"};

	memset(&portinfo, 0, sizeof(et_sw_port_info_t));

	if (et_get_portinfo(etc, portn, &portinfo) != 0)
		return;

	if(portn == 0xff)   {   /* all ports */
		pstart = 0;
		pend = 4;
	} else if (portn < 5) {
		pstart = pend = portn;
	} else {
		/* invalid port range */
		return;
	}

	/* pretty print */
	bcm_bprintf(b, "Port    Link    Speed(Mbps)    Duplex\n");
	bcm_bprintf(b, "----    ----    -----------    ------\n");

	for (i = pstart; i <= pend; i++) {
		bcm_bprintf(b, "\n%4d    ", i);
		if (portinfo.link_state & (1 << i)) {
			bcm_bprintf(b, "  Up");

			val = ((portinfo.speed >> (i * 2))) & 0x0003;
			bcm_bprintf(b, "%15s      ", phy_speed[val]);

			bcm_bprintf(b, portinfo.duplex & (1 << i) ? "Full" : "Half");

		} else {
			bcm_bprintf(b, "Down");
		}
	}
}

int
et_get_portinfo(etc_info_t *etc, int port_id, et_sw_port_info_t *portstatus)
{
	uint page = 1, reg;
	uint16 lnk, spd, dplx;
	robo_info_t *robo = NULL;

	if ((etc == NULL) || (portstatus == NULL))
		return -1;	/* fail */

	robo = (robo_info_t *)etc->robo;
	if ((robo == NULL) || (robo->ops == NULL))
		return -1;  /* fail */

	reg = 0;
	robo->ops->read_reg(etc->robo, page, reg, &lnk, 2);

	reg = 4;
	robo->ops->read_reg(etc->robo, page, reg, &spd, 2);

	reg = 8;
	robo->ops->read_reg(etc->robo, page, reg, &dplx, 2);

	portstatus->speed = spd;
	portstatus->link_state = lnk;
	portstatus->duplex = dplx;

	return 0;
}

#ifdef BCMDBG
void
etc_prhdr(char *msg, struct ether_header *eh, uint len, int unit)
{
	char da[32], sa[32];

	if (msg && (msg[0] != '\0'))
		printf("et%d: %s: ", unit, msg);
	else
		printf("et%d: ", unit);

	printf("dst %s src %s type 0x%x len %d\n",
		bcm_ether_ntoa((struct ether_addr *)eh->ether_dhost, da),
		bcm_ether_ntoa((struct ether_addr *)eh->ether_shost, sa),
		ntoh16(eh->ether_type),
		len);
}
void
etc_prhex(char *msg, uchar *buf, uint nbytes, int unit)
{
	if (msg && (msg[0] != '\0'))
		printf("et%d: %s:\n", unit, msg);
	else
		printf("et%d:\n", unit);

	prhex(NULL, buf, nbytes);
}
#endif /* BCMDBG */

void
etc_unitmap(uint vendor, uint device, uint coreunit, uint *unit)
{
#if !defined(_CFE_) || defined(CFG_ETC47XX)
	{
		extern struct chops bcm47xx_et_chops;

		if (bcm47xx_et_chops.id(vendor, device))
			*unit = coreunit;
	}
#endif

#ifdef CFG_GMAC
	{
		extern struct chops bcmgmac_et_chops;

		if (bcmgmac_et_chops.id(vendor, device))
			bcmgmac_et_chops.unitmap(coreunit, unit);
	}
#endif /* CFG_GMAC */
}
