/*
 * Common [OS-independent] header file for
 * Broadcom BCM47XX 10/100Mbps Ethernet Device Driver
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
 * $Id: etc.h 573045 2015-07-21 20:17:41Z $
 */

#ifndef _etc_h_
#define _etc_h_

#include <etioctl.h>

#define	MAXMULTILIST	32

#ifndef ch_t
#define	ch_t void
#endif

#define NUMTXQ		4

#define TXREC_THR       8
#define ETCQUOTA_MAX	64 /* Max et quota in dpc mode: bounds IOV_PKTCBND */

#if defined(__ECOS)
#define IOCBUFSZ	4096
#elif defined(__linux__)
#define IOCBUFSZ	16384
#else
#define IOCBUFSZ	4096
#endif
struct etc_info;	/* forward declaration */
struct bcmstrbuf;	/* forward declaration */

#ifdef ET_INGRESS_QOS
#define DMA_RX_THRESH_DEFAULT	250
#define DMA_RX_POLICY_NONE	0
#define DMA_RX_POLICY_UDP	1
#define DMA_RX_POLICY_TOS	2
#define DMA_RX_POLICY_LAST	2
#endif /* ET_INGRESS_QOS */

/* each chip type supports a set of chip-type-specific ops */
struct chops {
	bool (*id)(uint vendor, uint device);		/* return true if match */
	void *(*attach)(struct etc_info *etc, void *dev, void *regs);
	void (*detach)(ch_t *ch);			/* free chip private state */
	void (*reset)(ch_t *ch);			/* chip reset */
	void (*init)(ch_t *ch, uint options);		/* chip init */
	bool (*tx)(ch_t *ch, void *p);			/* transmit frame */
	void *(*rx)(ch_t *ch);				/* receive frame */
	int  (*rxquota)(ch_t *ch, int quota, void **rxpkts); /* receive quota pkt */
	void (*rxlazy)(ch_t *ch);
	void (*rxfill)(ch_t *ch);			/* post dma rx buffers */
	int (*getintrevents)(ch_t *ch, bool in_isr);	/* return intr events */
	bool (*errors)(ch_t *ch);			/* handle chip errors */
	bool (*dmaerrors)(ch_t *ch);			/* check chip dma errors */
	void (*intrson)(ch_t *ch);			/* enable chip interrupts */
	void (*intrsoff)(ch_t *ch);			/* disable chip interrupts */
	void (*txreclaim)(ch_t *ch, bool all);		/* reclaim transmit resources */
	void (*rxreclaim)(ch_t *ch);			/* reclaim receive resources */
	void (*statsupd)(ch_t *ch);			/* update sw stat counters */
	void (*dumpmib)(ch_t *ch, struct bcmstrbuf *, bool clear);	/* get sw mib counters */
	void (*enablepme)(ch_t *ch);			/* enable PME */
	void (*disablepme)(ch_t *ch);			/* disable PME */
	void (*configtimer)(ch_t *ch, uint microsecs); /* enable|disable gptimer */
	void (*phyreset)(ch_t *ch, uint phyaddr);	/* reset phy */
	uint16 (*phyrd)(ch_t *ch, uint phyaddr, uint reg);	/* read phy register */
	void (*phywr)(ch_t *ch, uint phyaddr, uint reg, uint16 val);	/* write phy register */
	uint (*macrd)(ch_t *ch, uint reg);	/* read gmac register */
	void (*macwr)(ch_t *ch, uint reg, uint val);	/* write gmac register */
	void (*dump)(ch_t *ch, struct bcmstrbuf *b);		/* debugging output */
	void (*longname)(ch_t *ch, char *buf, uint bufsize);	/* return descriptive name */
	void (*duplexupd)(ch_t *ch);			/* keep mac duplex consistent */
	void (*unitmap)(uint coreunit, uint *unit);	/* core unit to enet unit mapping */
	uint (*activerxbuf)(ch_t *ch); /* calculate the number of available free rx descriptors */
};

/*
 * "Common" os-independent software state structure.
 */
typedef struct etc_info {
	void		*et;		/* pointer to os-specific private state */
	uint		unit;		/* device instance number */
	void 		*osh; 		/* pointer to os handler */
	bool		gmac_fwd;	/* wl rx/tx forwarding over gmac: -DBCM_GMAC3 */
	bool		pktc;		/* packet chaining enabled or not */

	uint		bp_ticks_usec; /* backplane clock ticks per microsec */
	uint		rxlazy_timeout; /* rxlazy timeout configuration */
	uint		rxlazy_framecnt; /* rxlazy framecount configuration */

	int         hwrxoff;	/* HW RXOFFSET */
	int         pktcbnd;	/* max # of packets to chain */
	int         quota;		/* max # of packets to recv */

	void		*mib;		/* pointer to s/w maintained mib counters */
	bool		up;		/* interface up and running */
	bool		promisc;	/* promiscuous destination address */
	bool		qos;		/* QoS priority determination on rx */
	bool		loopbk;		/* loopback override mode */

	int		forcespeed;	/* disable autonegotiation and force speed/duplex */
	uint		advertise;	/* control speed/duplex advertised caps */
	uint		advertise2;	/* control gige speed/duplex advertised caps */
	bool		needautoneg;	/* request restart autonegotiation */
	int		speed;		/* current speed: 10, 100 */
	int		duplex;		/* current duplex: 0=half, 1=full */

	bool		piomode;	/* enable programmed io (!dma) */
	void		*pioactive;	/* points to pio packet being transmitted */
	volatile uint	*txavail[NUMTXQ];	/* dma: # tx descriptors available */

	uint16		vendorid;	/* pci function vendor id */
	uint16		deviceid;	/* pci function device id */
	uint		chip;		/* chip number */
	uint		chiprev;	/* chip revision */
	uint		chippkg;	/* chip package option */
	uint		coreid;		/* core id */
	uint		corerev;	/* core revision */

	bool		nicmode;	/* is this core using its own pci i/f */

	struct chops	*chops;		/* pointer to chip-specific opsvec */
	void		*ch;		/* pointer to chip-specific state */
	void		*robo;		/* optional robo private data */

	uint		txq_state;	/* tx queues state bits */
	uint		coreunit;	/* sb chips: chip enet instance # */
	uint		phyaddr;	/* sb chips: mdio 5-bit phy address */
	uint		mdcport;	/* sb chips: which mii to use (enet core #) to access phy */

	struct ether_addr cur_etheraddr; /* our local ethernet address */
	struct ether_addr perm_etheraddr; /* original sprom local ethernet address */

	struct ether_addr multicast[MAXMULTILIST];
	uint		nmulticast;
	bool		allmulti;	/* enable all multicasts */

	bool		linkstate;	/* link integrity state */
	bool		pm_modechange;	/* true if mode change is to due pm */

	uint32		now;		/* elapsed seconds */

	uint32		boardflags;	/* board flags */
	uint32		txrec_thresh;	/* # of tx frames after which reclaim is done */

#ifdef ET_INGRESS_QOS
	uint16		dma_rx_thresh;	/* DMA red zone RX descriptor threshold */
	uint16		dma_rx_policy;	/* DMA RX discard policy in red zone */
#endif /* ET_INGRESS_QOS */

	/* sw-maintained stat counters */
	uint32		txframes[NUMTXQ];	/* transmitted frames on each tx fifo */
	uint32		txframe;	/* transmitted frames */
	uint32		txbyte;		/* transmitted bytes */
	uint32		rxframe;	/* received frames */
	uint32		rxbyte;		/* received bytes */
	uint32		txerror;	/* total tx errors */
	uint32		txnobuf;	/* tx out-of-buffer errors */
	uint32		rxerror;	/* total rx errors */
	uint32		rxgiants;	/* total rx giant frames */
	uint32		rxnobuf;	/* rx out-of-buffer errors */
	uint32		reset;		/* reset count */
	uint32		dmade;		/* pci descriptor errors */
	uint32		dmada;		/* pci data errors */
	uint32		dmape;		/* descriptor protocol error */
	uint32		rxdmauflo;	/* receive descriptor underflow */
	uint32		rxoflo;		/* receive fifo overflow */
	uint32		txuflo;		/* transmit fifo underflow */
	uint32		rxoflodiscards;	/* frames discarded during rx fifo overflow */
	uint32		rxbadlen;	/* 802.3 len field != read length */
	uint32		chained;	/* number of frames chained */
	uint32		unchained;	/* number of frames not chained */
	uint32		maxchainsz;	/* max chain size so far */
	uint32		currchainsz;	/* current chain size */
#if defined(BCMDBG)
#if defined(PKTC)
	uint32		chainsz[PKTCBND];	/* chain size histo */
#endif /* PKTC */
	uint32		quota_stats[ETCQUOTA_MAX];
#endif /* BCMDBG */
	uint32		rxprocessed;
	uint32		reset_countdown;
#ifdef ETFA
	void		*fa;		/* optional fa private data */
#endif
#ifdef ETAGG
        void		*agg;		/* agg private data */
#endif /* ETAGG */
	uint32		rxrecord;
} etc_info_t;

typedef struct et_sw_port_info {
	uint16 port_id;
	uint16 link_state;	/* 0:down  1:up */
	uint16 speed;		/* 0:unknown  1:10M  2:100M  3:1000M  4:200M */
	uint16 duplex;		/* 0:unknown  1:half  2:full */
} et_sw_port_info_t;

#define MACADDR_MASK	0x0000FFFFFFFFFFFFLL
#define VID_MASK		0x0FFF000000000000LL

/* interrupt event bitvec */
#define	INTR_TX		0x1
#define	INTR_RX		0x2
#define	INTR_ERROR	0x4
#define	INTR_TO		0x8
#define	INTR_NEW	0x10

/* forcespeed values */
#define	ET_AUTO		-1
#define	ET_10HALF	0
#define	ET_10FULL	1
#define	ET_100HALF	2
#define	ET_100FULL	3
#define	ET_1000HALF	4
#define	ET_1000FULL	5
#define	ET_2500FULL	6

/* init options */
#define ET_INIT_FULL     0x1
#define ET_INIT_INTRON   0x2

/* Specific init options for et_init */
#define ET_INIT_DEF_OPTIONS   (ET_INIT_FULL | ET_INIT_INTRON)
#define ET_INIT_INTROFF       (ET_INIT_FULL)
#define ET_INIT_PARTIAL      (0)

/* macro to safely clear the UP flag */
#define ET_FLAG_DOWN(x)   (*(x)->chops->intrsoff)((x)->ch);  \
			  (x)->up = FALSE;

/*
 * Least-common denominator rxbuf start-of-data offset:
 * Must be >= size of largest rxhdr
 * Must be 2-mod-4 aligned so IP is 0-mod-4
 */
#define	HWRXOFF				30

/* In GMAC3 mode, the fwder GMAC uses a smaller HWRXOFF so as to ensure that
 * the DMA(s)LAN and WLAN are aligned to 32B boundaries
 */
#define GMAC_FWDER_HWRXOFF	18

#define TC_BK		0	/* background traffic class */
#define TC_BE		1	/* best effort traffic class */
#define TC_CL		2	/* controlled load traffic class */
#define TC_VO		3	/* voice traffic class */
#define TC_NONE		-1	/* traffic class none */

#define RX_Q0		0	/* receive DMA queue */
#define NUMRXQ		1	/* gmac has one rx queue */

#define TX_Q0		TC_BK	/* DMA txq 0 */
#define TX_Q1		TC_BE	/* DMA txq 1 */
#define TX_Q2		TC_CL	/* DMA txq 2 */
#define TX_Q3		TC_VO	/* DMA txq 3 */


static inline uint32
etc_up2tc(uint32 up)
{
	extern uint32 up2tc[];

	return (up2tc[up]);
}

static inline uint32
etc_priq(uint32 txq_state)
{
	extern uint32 priq_selector[];

	return (priq_selector[txq_state]);
}

/* RXH_FLAGS: GMAC and ENET47XX versions. */
#define IS_GMAC(etc)            ((etc)->coreid == GMAC_CORE_ID)

/* Test whether any rx errors occurred. */
#define GMAC_RXH_FLAGS(rxh) \
	((((bcmgmacrxh_t *)(rxh))->flags) & htol16(GRXF_CRC | GRXF_OVF | GRXF_OVERSIZE))

#define ENET47XX_RXH_FLAGS(rxh) \
	((((bcmenetrxh_t *)(rxh))->flags) & htol16(RXF_NO | RXF_RXER | RXF_CRC | RXF_OV))

#define RXH_FLAGS(etc, rxh) \
	(IS_GMAC(etc) ? GMAC_RXH_FLAGS(rxh) : ENET47XX_RXH_FLAGS(rxh))

/* Host order rx header error flag accessors. */
#define GMAC_RXH_FLAG_NONE      (FALSE)
#define ENET47XX_RXH_FLAG_NONE  (FALSE)

#define GMAC_RXERROR(rxh, mask) \
	((ltoh16(((bcmgmacrxh_t *)(rxh))->flags)) & (mask))
#define ENET47XX_RXERROR(rxh, mask) \
	((ltoh16(((bcmenetrxh_t *)(rxh))->flags)) & (mask))

/* rx: over size packet error */
#define RXH_OVERSIZE(etc, rxh) \
	(IS_GMAC(etc) ? GMAC_RXERROR(rxh, GRXF_OVERSIZE) : ENET47XX_RXH_FLAG_NONE)

/* rx: crc error. */
#define RXH_CRC(etc, rxh) \
	(IS_GMAC(etc) ? GMAC_RXERROR(rxh, GRXF_CRC) : ENET47XX_RXERROR(rxh, RXF_CRC))

/* rx: fifo overflow error. */
#define RXH_OVF(etc, rxh) \
	(IS_GMAC(etc) ? GMAC_RXERROR(rxh, GRXF_OVF) : ENET47XX_RXERROR(rxh, RXF_OV))

/* rx: symbol error */
#define RXH_RXER(etc, rxh) \
	(IS_GMAC(etc) ? GMAC_RXH_FLAG_NONE : ENET47XX_RXERROR(rxh, RXF_RXER))

/* rx: crc error (odd nibbles) */
#define RXH_NO(etc, rxh) \
	(IS_GMAC(etc) ? GMAC_RXH_FLAG_NONE : ENET47XX_RXERROR(rxh, RXF_NO))


#ifdef	CFG_GMAC
#define ET_GMAC(etc)	((etc)->coreid == GMAC_CORE_ID)
#else
#define ET_GMAC(etc)	(0)
#endif	/* CFG_GMAC */

#if defined(BCM_GMAC3)
/** Types of ethernet device driver.
 * In the 3 GMAC Model, the ethernet driver operates as either a
 * - FWD: HW Switch Forwarder to WLAN MAC Interface such as wl0, wl1
 * - NTK: Ethernet Network Interface binding to network stack (via CTF)
 */
#define DEV_FWDER_NAME          "fwd"
#define DEV_NTKIF(etc)          ((etc)->gmac_fwd == FALSE)
#define DEV_FWDER(etc)          ((etc)->gmac_fwd == TRUE)
#else  /* ! BCM_GMAC3 */
#define DEV_FWDER_NAME          "eth"
#define DEV_NTKIF(etc)          (TRUE)
#define DEV_FWDER(etc)          (FALSE)
#endif /* ! BCM_GMAC3 */

/* exported prototypes */
extern struct chops *etc_chipmatch(uint vendor, uint device);
extern void *etc_attach(void *et, uint vendor, uint device, uint coreunit, void *dev, void *regsva);
extern void etc_detach(etc_info_t *etc);
extern void etc_reset(etc_info_t *etc);
extern void etc_init(etc_info_t *etc, uint options);
extern void etc_up(etc_info_t *etc);
extern uint etc_down(etc_info_t *etc, int reset);
extern int etc_ioctl(etc_info_t *etc, int cmd, void *arg);
extern int etc_iovar(etc_info_t *etc, uint cmd, uint set, void *arg, int len);
extern void etc_promisc(etc_info_t *etc, uint on);
extern void etc_qos(etc_info_t *etc, uint on);
extern void etc_quota(etc_info_t *etc);
extern void etc_rxlazy(etc_info_t *etc, uint microsecs, uint framecnt);
extern void etc_dump(etc_info_t *etc, struct bcmstrbuf *b);
extern void etc_watchdog(etc_info_t *etc);
extern uint etc_totlen(etc_info_t *etc, void *p);
#ifdef ETROBO
extern void *etc_bcm53115_war(etc_info_t *etc, void *p);
#endif /* ETROBO */
extern void etc_unitmap(uint vendor, uint device, uint coreunit, uint *unit);

#endif	/* _etc_h_ */
