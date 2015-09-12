/*
 * Linux device driver for
 * Broadcom BCM47XX 10/100/1000 Mbps Ethernet Controller
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
 * $Id: et_linux.c 575708 2015-07-30 20:27:43Z $
 */

#include <et_cfg.h>
#define __UNDEF_NO_VERSION__

#include <typedefs.h>

#include <linux/module.h>
#include <linuxver.h>
#include <bcmdefs.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/sockios.h>
#ifdef SIOCETHTOOL
#include <linux/ethtool.h>
#endif /* SIOCETHTOOL */
#include <linux/ip.h>
#include <linux/if_vlan.h>
#include <net/tcp.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>

#include <osl.h>
#include <epivers.h>
#include <bcmendian.h>
#include <bcmdefs.h>
#include <proto/ethernet.h>
#include <proto/vlan.h>
#include <proto/bcmip.h>
#include <bcmdevs.h>
#include <bcmenetmib.h>
#include <bcmgmacmib.h>
#include <bcmenetrxh.h>
#include <bcmenetphy.h>
#include <etioctl.h>
#include <bcmutils.h>
#include <pcicfg.h>
#include <et_dbg.h>
#include <hndsoc.h>
#include <bcmgmacrxh.h>
#include <etc.h>
#ifdef HNDCTF
#include <ctf/hndctf.h>
#endif /* HNDCTF */
#include <hndfwd.h>
#if defined(PLC) || defined(ETFA)
#include <siutils.h>
#endif /* PLC || ETFA */
#ifdef ETFA
#include <etc_fa.h>
#endif /* ETFA */
#ifdef ETAGG
#include <etc_agg.h>

#ifdef ETFA
#define FA_RX_BCM_HDR_ENAB(et)	FA_RX_BCM_HDR((fa_t *)et->etc->fa)
#define FA_TX_BCM_HDR_ENAB(et)	FA_TX_BCM_HDR((fa_t *)et->etc->fa)
#else /* ! ETFA */
#define FA_RX_BCM_HDR_ENAB(et)	0
#define FA_TX_BCM_HDR_ENAB(et)	0
#endif /* ! ETFA */

#ifdef ETAGG
#define AGG_RX_BCM_HDR_ENAB(et)	AGG_BCM_HDR_ENAB(et->etc->agg)
#define AGG_TX_BCM_HDR_ENAB(et)	AGG_BCM_HDR_ENAB(et->etc->agg)
#else /* ! ETAGG */
#define AGG_RX_BCM_HDR_ENAB(et)	0
#define AGG_TX_BCM_HDR_ENAB(et)	0
#endif /* ! ETAGG */

#define RX_BCM_HDR_ENAB(et)	(FA_RX_BCM_HDR_ENAB(et) | AGG_RX_BCM_HDR_ENAB(et))
#define TX_BCM_HDR_ENAB(et)	(FA_TX_BCM_HDR_ENAB(et) | AGG_TX_BCM_HDR_ENAB(et))
#endif /* ETAGG */

#ifdef ETFA
#define CTF_CAPABLE_DEV(et)	FA_CTF_CAPABLE_DEV((fa_t *)(et)->etc->fa)
#else
#define CTF_CAPABLE_DEV(et)	(TRUE)
#endif /* !ETFA */

#ifdef ET_ALL_PASSIVE_ON
/* When ET_ALL_PASSIVE_ON, ET_ALL_PASSIVE must be true */
#if defined(BCM_GMAC3)
#define ET_ALL_PASSIVE_ENAB(et)	0
#else  /* ! BCM_GMAC3 */
#define ET_ALL_PASSIVE_ENAB(et)	1
#endif /* ! BCM_GMAC3 */
#else
#ifdef ET_ALL_PASSIVE
#define ET_ALL_PASSIVE_ENAB(et)	(!(et)->all_dispatch_mode)
#else /* ET_ALL_PASSIVE */
#define ET_ALL_PASSIVE_ENAB(et)	0
#endif /* ET_ALL_PASSIVE */
#endif	/* ET_ALL_PASSIVE_ON */

#ifdef USBAP
#define MAX_IP_HDR_LEN			60
#define MAX_TCP_HDR_LEN			60
#define MAX_TCP_CTRL_PKT_LEN	(ETHERVLAN_HDR_LEN+MAX_IP_HDR_LEN+MAX_TCP_HDR_LEN+ETHER_CRC_LEN)
#define PKT_IS_TCP_CTRL(osh, p)	(PKTLEN((osh), (p)) <= (MAX_TCP_CTRL_PKT_LEN + HWRXOFF))
#define SKIP_TCP_CTRL_CHECK(et)	((et)->etc->speed >= 1000)
#endif

#if defined(BCM_GMAC3)

/* Ensure linux_osl.h:FWDER_HWRXOFF matches etc.h:GMAC_FWER_HWRXOFF */
#if (FWDER_HWRXOFF != GMAC_FWDER_HWRXOFF)
#error "FWDER_HWRXOFF mismatch with GMAC_FWDER_HWRXOFF"
#endif

#if defined(ETFA)
#error "GMAC3 based forwarding not compatible with ETFA - AuX port"
#endif /* ETFA */
#if !defined(DMA)
#error "GMAC3 not supported without DMA"
#endif /* ! DMA */
#endif /* BCM_GMAC3 */

static int et_get_settings(struct net_device *dev, struct ethtool_cmd *ecmd);
static int et_set_settings(struct net_device *dev, struct ethtool_cmd *ecmd);
static void et_get_driver_info(struct net_device *dev, struct ethtool_drvinfo *info);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static const struct ethtool_ops et_ethtool_ops =
{
	.get_settings = et_get_settings,
	.set_settings = et_set_settings,
	.get_drvinfo = et_get_driver_info
};
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36) */


#ifdef ET_INGRESS_QOS
#define TOSS_CAP	4
#define PROC_CAP	4
#endif /* ET_INGRESS_QOS */

MODULE_LICENSE("Proprietary");

#ifdef PLC
typedef struct et_plc {
	bool	hw;			/* plc hardware present */
	int32	txvid;			/* vlan used to send to plc */
	int32	rxvid1;			/* frames rx'd on this will be sent to br */
	int32	rxvid2;			/* frames rx'd on this will be sent to wds */
	int32	rxvid3;			/* frames rx'd on this will be sent to plc */
	struct net_device *txdev;	/* plc device (vid 3) for tx */
	struct net_device *rxdev1;	/* plc device (vid 4) for rx */
	struct net_device *rxdev2;	/* plc device (vid 5) for tx & rx */
	struct net_device *rxdev3;	/* plc device (vid 6) for tx & rx */
} et_plc_t;
#endif /* PLC */

/* In 2.6.20 kernels work functions get passed a pointer to the
 * struct work, so things will continue to work as long as the work
 * structure is the first component of the task structure.
 */
typedef struct et_task {
	struct work_struct work;
	void *context;
} et_task_t;

typedef struct et_info {
	etc_info_t	*etc;		/* pointer to common os-independent data */
	struct net_device *dev;		/* backpoint to device */
	struct pci_dev *pdev;		/* backpoint to pci_dev */
	void		*osh;		/* pointer to os handle */
#if defined(BCM_GMAC3)
	bool fwder_up;			/* BCM_GMAC3: runtime forwarder state */
	struct fwder *fwdh;		/* BCM_GMAC3: my upstream forwarder handle */
#endif /* BCM_GMAC3 */
#ifdef HNDCTF
	ctf_t		*cih;		/* ctf instance handle */
	ctf_brc_hot_t	*brc_hot;	/* hot bridge cache entry */
#endif
#ifdef ETFA
	uint8       fa_bhdr_sz;
	bool        fa_aux_dev;
#endif /* ET_FA */
	struct semaphore sem;		/* use semaphore to allow sleep */
	spinlock_t	lock;		/* per-device perimeter lock */
	spinlock_t	txq_lock;	/* lock for txq protection */
	spinlock_t	isr_lock;	/* lock for irq reentrancy protection */
	struct sk_buff_head txq[NUMTXQ];	/* send queue */
	int   txq_pktcnt[NUMTXQ];	/* packets in each tx queue */
	void *regsva;			/* opaque chip registers virtual address */
	struct timer_list timer;	/* one second watchdog timer */
	bool set;			/* indicate the timer is set or not */
	struct net_device_stats stats;	/* stat counter reporting structure */
	int events;			/* bit channel between isr and dpc */
	struct et_info *next;		/* pointer to next et_info_t in chain */
#ifdef	NAPI2_POLL
	struct napi_struct napi_poll;
#endif	/* NAPI2_POLL */
#ifndef NAPI_POLL
	struct tasklet_struct tasklet;	/* dpc tasklet */
	struct tasklet_struct tx_tasklet;	/* tx tasklet */
#endif /* NAPI_POLL */
#ifdef ET_ALL_PASSIVE
	et_task_t	dpc_task;	/* work queue for rx dpc */
	et_task_t	txq_task;	/* work queue for tx frames */
	et_task_t	wd_task;	/* work queue for watchdog */
	bool		all_dispatch_mode;	/* dispatch mode: tasklets or passive */
#endif /* ET_ALL_PASSIVE */
	bool resched;			/* dpc was rescheduled */
#ifdef PLC
	et_plc_t	plc;		/* plc interface info */
#endif /* PLC */
#ifdef ETFA
	spinlock_t	fa_lock;	/* lock for fa cache protection */
#endif
#if defined(ETFA) || defined(ETAGG)
	uint8		bhdr_sz;	/* size of brcm header */
	int8		bhdr_roff;	/* brcm header's position relative to dest mac */
#endif /* ETFA || ETAGG */
#ifdef ETAGG
	void		*aggdev;	/* dummy dev for control agg */
#endif /* ETAGG */
} et_info_t;

static int et_found = 0;
static et_info_t *et_list = NULL;

#define	ET_INFO(dev)	(et_info_t *)(DEV_PRIV(dev))


#define ET_LOCK(et) \
do { \
	if (ET_ALL_PASSIVE_ENAB(et)) \
		down(&(et)->sem); \
	else \
		spin_lock_bh(&(et)->lock); \
} while (0)

#define ET_UNLOCK(et) \
do { \
	if (ET_ALL_PASSIVE_ENAB(et)) \
		up(&(et)->sem); \
	else \
		spin_unlock_bh(&(et)->lock); \
} while (0)

#define ET_TXQ_LOCK(et)		spin_lock_bh(&(et)->txq_lock)
#define ET_TXQ_UNLOCK(et)	spin_unlock_bh(&(et)->txq_lock)

#define INT_LOCK(et, flags)	spin_lock_irqsave(&(et)->isr_lock, flags)
#define INT_UNLOCK(et, flags)	spin_unlock_irqrestore(&(et)->isr_lock, flags)

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 4, 5)
#error Linux version must be newer than 2.4.5
#endif	/* LINUX_VERSION_CODE <= KERNEL_VERSION(2, 4, 5) */

/* linux 2.4 doesn't have in_atomic */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
#define in_atomic() 0
#endif

/* prototypes called by etc.c */
void et_init(et_info_t *et, uint options);
void et_reset(et_info_t *et);
void et_link_up(et_info_t *et);
void et_link_down(et_info_t *et);
void et_up(et_info_t *et);
void et_down(et_info_t *et, int reset);
void et_discard(et_info_t *et, void *pkt);
void et_dump(et_info_t *et, struct bcmstrbuf *b);
#ifdef HNDCTF
void et_dump_ctf(et_info_t *et, struct bcmstrbuf *b);
#endif
#ifdef BCMDBG_CTRACE
void et_dump_ctrace(et_info_t *et, struct bcmstrbuf *b);
#endif
#if defined(BCM_GMAC3)
void et_dump_fwder(et_info_t *et, struct bcmstrbuf *b);
#endif /* BCM_GMAC3 */

/* local prototypes */
static void et_free(et_info_t *et);
static int et_open(struct net_device *dev);
static int et_close(struct net_device *dev);
#if defined(BCM_GMAC3)
static int et_forward(struct fwder * fwder, struct sk_buff *skbs, int skb_cnt,
                      struct net_device *rx_dev);
static int et_dummy_start(struct sk_buff *skb, struct net_device *dev);
#endif /* BCM_GMAC3 */
static int et_start(struct sk_buff *skb, struct net_device *dev);
static void et_sendnext(et_info_t *et);
static struct net_device_stats *et_get_stats(struct net_device *dev);
static int et_set_mac_address(struct net_device *dev, void *addr);
static void et_set_multicast_list(struct net_device *dev);
static void _et_watchdog(struct net_device *data);
static void et_watchdog(ulong data);
static int et_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);
#ifdef ETFA
static int et_fa_default_cb(void *dev, ctf_ipc_t *ipc, bool v6, int cmd);
static int et_fa_normal_cb(void *dev, ctf_ipc_t *ipc, bool v6, int cmd);
void et_fa_up(et_info_t *et);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
static irqreturn_t et_isr(int irq, void *dev_id);
#else
static irqreturn_t et_isr(int irq, void *dev_id, struct pt_regs *ptregs);
#endif
#ifdef	NAPI2_POLL
static int et_poll(struct napi_struct *napi, int budget);
#elif defined(NAPI_POLL)
static int et_poll(struct net_device *dev, int *budget);
#else /* ! NAPI_POLL */
static void et_dpc(ulong data);
#endif /* NAPI_POLL */
static void et_tx_tasklet(ulong data);
#ifdef ET_ALL_PASSIVE
static void et_dpc_work(struct et_task *task);
static void et_watchdog_task(et_task_t *task);
#endif /* ET_ALL_PASSIVE */
static void et_txq_work(struct et_task *task);
static inline int32 et_ctf_forward(et_info_t *et, struct sk_buff *skb);
static void et_sendup(et_info_t *et, struct sk_buff *skb, int dataoff);
#ifdef BCMDBG
static void et_dumpet(et_info_t *et, struct bcmstrbuf *b);
#endif /* BCMDBG */
static void et_error(et_info_t *et, struct sk_buff *skb, void *rxh);

#ifdef HAVE_NET_DEVICE_OPS
static const struct net_device_ops et_netdev_ops = {
	.ndo_open = et_open,
	.ndo_stop = et_close,
	.ndo_start_xmit = et_start,
	.ndo_get_stats = et_get_stats,
	.ndo_set_mac_address = et_set_mac_address,
	.ndo_set_multicast_list = et_set_multicast_list,
	.ndo_do_ioctl = et_ioctl,
};

#if defined(BCM_GMAC3)
static const struct net_device_ops et_gmac3_netdev_ops = {
	.ndo_open = et_open,
	.ndo_stop = et_close,
	.ndo_start_xmit = et_dummy_start,
	.ndo_get_stats = et_get_stats,
	.ndo_set_mac_address = et_set_mac_address,
	.ndo_set_multicast_list = et_set_multicast_list,
	.ndo_do_ioctl = et_ioctl,
};
#endif /* BCM_GMAC3 */

#endif /* HAVE_NET_DEVICE_OPS */

/* recognized PCI IDs */
static struct pci_device_id et_id_table[] __devinitdata = {
	{ vendor: PCI_ANY_ID,
	device: PCI_ANY_ID,
	subvendor: PCI_ANY_ID,
	subdevice: PCI_ANY_ID,
	class: PCI_CLASS_NETWORK_OTHER << 8,
	class_mask: 0xffff00,
	driver_data: 0,
	},
	{ vendor: PCI_ANY_ID,
	device: PCI_ANY_ID,
	subvendor: PCI_ANY_ID,
	subdevice: PCI_ANY_ID,
	class: PCI_CLASS_NETWORK_ETHERNET << 8,
	class_mask: 0xffff00,
	driver_data: 0,
	},
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, et_id_table);

static unsigned int online_cpus = 1;
#if defined(BCMDBG)
static uint32 msglevel = 0xdeadbeef;
module_param(msglevel, uint, 0644);
#endif /* defined(BCMDBG) */

#ifdef ET_ALL_PASSIVE
/* passive mode: 1: enable, 0: disable */
static int passivemode = 0;
module_param(passivemode, int, 0);
#endif /* ET_ALL_PASSIVE */
static int txworkq = 0;
module_param(txworkq, int, 0);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
#define ET_TXQ_THRESH_DEFAULT   1536
#else
#define ET_TXQ_THRESH_DEFAULT   3300
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36) */

static int et_txq_thresh = ET_TXQ_THRESH_DEFAULT;
module_param(et_txq_thresh, int, 0);

static uint et_rxlazy_timeout = ET_RXLAZY_TIMEOUT;
module_param(et_rxlazy_timeout, uint, 0);

static uint et_rxlazy_framecnt = ET_RXLAZY_FRAMECNT;
module_param(et_rxlazy_framecnt, uint, 0);

static uint et_rxlazy_dyn_thresh = 0;
module_param(et_rxlazy_dyn_thresh, uint, 0);

#ifdef PKTC
#ifndef HNDCTF
#error "Packet chaining feature can't work w/o CTF"
#endif
#define PKTC_ENAB(et)	((et)->etc->pktc)
#define PKTCMC	32
struct pktc_data {
	void	*chead;		/* chain head */
	void	*ctail;		/* chain tail */
	uint8	*h_da;		/* pointer to da of chain head */
	uint8	*h_sa;		/* pointer to sa of chain head */
	uint8	h_prio;		/* prio of chain head */
};
typedef struct pktc_data pktc_data_t;

/**
 * Given packet fields and bridge cache, determine whether a packet is
 * eligible to be added to current chain data entry.
 */

#if defined(BCM_GMAC3)

static inline bool
is_pkt_chainable(et_info_t *et, void * pkt, void *pkthdr, uint8 prio,
                 pktc_data_t *cd, const bool dev_ntkif)
{
	struct ether_header *eh = (struct ether_header *)pkthdr;

	/* RXHFLAGS is tested in the etcgmac during chip rx quota, already */
	if ((!ETHER_ISNULLDEST(eh->ether_dhost)) &&
	    !eacmp(eh->ether_dhost, cd->h_da) &&
	    !eacmp(eh->ether_shost, cd->h_sa) &&
	    (prio == cd->h_prio)) {

		if (dev_ntkif) { /* NTKIF: Lookup hot bridge cache */

			/* Packets are received with VLAN Tag on ntkif port#8 | GMAC#2 */
			struct ethervlan_header *evh = (struct ethervlan_header *)pkthdr;

			if (et->brc_hot &&
			    CTF_HOTBRC_CMP(et->brc_hot, evh->ether_dhost, (void*)et->dev) &&
			    (evh->vlan_type == HTON16(ETHER_TYPE_8021Q)) &&
			    ((evh->ether_type == HTON16(ETHER_TYPE_IP)) ||
			     (evh->ether_type == HTON16(ETHER_TYPE_IPV6)))) {

				return TRUE;
			}

		} else {

			/* Packets are received without VLAN Tag on GMAC forwarders */

			if ((eh->ether_type == HTON16(ETHER_TYPE_IP)) ||
			    (eh->ether_type == HTON16(ETHER_TYPE_IPV6))) {

				return TRUE;
			}
		}
	}

	return FALSE; /* not eligible to be chained with h_xx entry */
}

#else /* ! BCM_GMAC3 */

static inline bool
is_pkt_chainable(et_info_t *et, void * pkt, void * pkthdr, uint8 prio,
                 pktc_data_t *cd, const bool dev_ntkif)
{
	struct ethervlan_header *evh = (struct ethervlan_header *)pkthdr;

	if ((!ETHER_ISNULLDEST(evh->ether_dhost)) &&
	    !eacmp(evh->ether_dhost, cd->h_da) &&
	    !eacmp(evh->ether_shost, cd->h_sa) &&
	    et->brc_hot &&
	    CTF_HOTBRC_CMP(et->brc_hot, evh, (void *)et->dev) &&
	    (prio == cd->h_prio) &&
	    (evh->vlan_type == HTON16(ETHER_TYPE_8021Q)) &&
	    ((evh->ether_type == HTON16(ETHER_TYPE_IP)) ||
	     (evh->ether_type == HTON16(ETHER_TYPE_IPV6))) &&
	    (!RXH_FLAGS(et->etc, PKTDATA(et->osh, pkt)))) {

		return TRUE;
	}

	return FALSE; /* not eligible to be chained with h_xx entry */
}

#endif /* BCM_GMAC3 */

#else /* PKTC */
#define PKTC_ENAB(et)	0
#endif /* PKTC */

#ifdef ETAGG
static int
et_agg_dummy_start(struct sk_buff *skb, struct net_device *dev)
{
	if (skb)
		kfree_skb(skb);

	return 0;
}

#ifdef HAVE_NET_DEVICE_OPS
static const struct net_device_ops et_agg_netdev_ops = {
	.ndo_start_xmit = et_agg_dummy_start,
};
#endif /* HAVE_NET_DEVICE_OPS */

int32
et_agg_control(void *dev, int32 cmd, void *in, void *out)
{
	int32 ret = BCME_ERROR;
	et_info_t *et;
	uint32 *invars = NULL, *outvars = NULL;
	uint32 in_num = 0, out_num = 0;

	if (dev == NULL)
		goto err;

	et = ET_INFO((struct net_device *)dev);

	if (in) {
		invars = (uint32 *)in;
		in_num = invars[0];
	}

	if (out) {
		outvars = (uint32 *)out;
		out_num = outvars[0];
	}

	switch (cmd) {
	case AGG_GET_LINKSTS:
		if (out_num < 1)
			goto err;

		ret = agg_get_linksts(et->etc->agg, &outvars[1]);
		break;

	case AGG_GET_PORTSTS:
		if (in_num < 1 || out_num < 3)
			goto err;

		ret = agg_get_portsts(et->etc->agg, invars[1], &outvars[1],
			&outvars[2], &outvars[3]);
		break;

	case AGG_SET_GRP:
		if (in_num < 2)
			goto err;

		ret = agg_group_update(et->etc->agg, invars[1], invars[2]);
		break;

	case AGG_SET_BHDR:
		if (in_num < 1)
			goto err;

		ret = agg_bhdr_switch(et->etc->agg, invars[1]);
		break;

	default:
		ret = BCME_ERROR;
		ET_ERROR(("cmd:%d not supported\n", cmd));
		break;
	}

err:
	if (ret == BCME_ERROR) {
		ET_ERROR(("et_agg_control: error! dev %p cmd %d in_num %d out_num %d\n",
			dev, cmd, in_num, out_num));
	}

	return ret;
}

static void *
et_aggdev_register(osl_t *osh, void *dev, int unit)
{
	struct net_device *aggdev = NULL;
	et_agg_ctl_t *agg_ctl;
	uint8 name[IFNAMSIZ] = "agg";

	if (!osh || !dev) {
		ET_ERROR(("et%d: et_aggdev_register: NULL input pointer\n", unit));
		return NULL;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	if ((aggdev = alloc_etherdev(sizeof(et_agg_ctl_t))) == NULL) {
		ET_ERROR(("et%d: et_aggdev_register: alloc_etherdev() failed\n", unit));
		return NULL;
	}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
	if ((aggdev = alloc_etherdev(0)) == NULL) {
		ET_ERROR(("et%d: et_aggdev_register: alloc_etherdev() failed\n", unit));
		return NULL;
	}
	/* allocate private info */
	if ((agg_ctl = (et_agg_ctl_t *)MALLOC(osh, sizeof(et_agg_ctl_t))) == NULL) {
		ET_ERROR(("et%d: et_aggdev_register: out of memory, malloced %d bytes\n", unit,
		          MALLOCED(osh)));
		MFREE(osh, aggdev, sizeof(et_agg_ctl_t));
		return NULL;
	}
	aggdev->priv = (void *)agg_ctl;
#else /* < 2.6.0 */
	if (!(aggdev = (struct net_device *) MALLOC(osh, sizeof(struct net_device)))) {
		ET_ERROR(("et%d: et_aggdev_register: out of memory, malloced %d bytes\n", unit,
		          MALLOCED(osh)));
		return NULL;
	}
	bzero(aggdev, sizeof(struct net_device));

	if (!init_etherdev(aggdev, 0)) {
		ET_ERROR(("et%d: et_aggdev_register: init_etherdev() failed\n", unit));
		MFREE(osh, aggdev, sizeof(struct net_device));
		return NULL;
	}
	/* allocate private info */
	if ((agg_ctl = (et_agg_ctl_t *)MALLOC(osh, sizeof(et_agg_ctl_t))) == NULL) {
		ET_ERROR(("et%d: et_aggdev_register: out of memory, malloced %d bytes\n", unit,
		          MALLOCED(osh)));
		MFREE(osh, aggdev, sizeof(et_agg_ctl_t));
		return NULL;
	}
	aggdev->priv = (void *)agg_ctl;
#endif /* < 2.6.0 */
	strncpy(aggdev->name, name, IFNAMSIZ);
	agg_ctl = (et_agg_ctl_t *)DEV_PRIV(aggdev);
	bzero(agg_ctl, sizeof(et_agg_ctl_t));
	agg_ctl->fn = et_agg_control;
	agg_ctl->dev = dev;

#ifndef HAVE_NET_DEVICE_OPS
	aggdev->hard_start_xmit = et_agg_dummy_start;
#else /* ! HAVE_NET_DEVICE_OPS */
	aggdev->netdev_ops = &et_agg_netdev_ops;
#endif /* HAVE_NET_DEVICE_OPS */

	if (register_netdev(aggdev)) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
		free_netdev(aggdev);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
		free_netdev(aggdev);
		MFREE(osh, agg_ctl, sizeof(et_agg_ctl_t));
#else /* < 2.6.0 */
		MFREE(osh, aggdev, sizeof(struct net_device));
		MFREE(osh, agg_ctl, sizeof(et_agg_ctl_t));
#endif /* < 2.6.0 */
	}

	return (void *)aggdev;
}

void
et_aggdev_unregister(osl_t *osh, struct net_device *aggdev)
{
	et_agg_ctl_t *agg_ctl;

	if (!osh || !aggdev)
		return;

	agg_ctl = (et_agg_ctl_t *)DEV_PRIV(aggdev);
	unregister_netdev(aggdev);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	free_netdev(aggdev);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
	free_netdev(aggdev);
	MFREE(osh, agg_ctl, sizeof(et_agg_ctl_t));
#else /* < 2.6.0 */
	MFREE(osh, aggdev, sizeof(struct net_device));
	MFREE(osh, agg_ctl, sizeof(et_agg_ctl_t));
#endif /* < 2.6.0 */
}
#endif /* ETAGG */

#ifdef HNDCTF
static void
et_ctf_detach(ctf_t *ci, void *arg)
{
	et_info_t *et = (et_info_t *)arg;

	et->cih = NULL;

#ifdef CTFPOOL
	/* free the buffers in fast pool */
	osl_ctfpool_cleanup(et->osh);
#endif /* CTFPOOL */

	return;
}
#endif /* HNDCTF */

#ifdef PLC
static void
et_plc_reset(void)
{
	si_t *sih = NULL;
	uint reset_plc;         /* plc reset signal */
	uint refvdd_off_plc;    /* plc 3.3V control */
	uint en_off_plc;        /* plc 0.9V control */
	int refvdd_off_plc_mask;
	int en_off_plc_mask;
	int reset_plc_mask;

	sih = si_kattach(SI_OSH);

	/* Reset plc and turn on plc 3.3V and 0.95V */
	refvdd_off_plc = getgpiopin(NULL, "refvdd_off_plc", GPIO_PIN_NOTDEFINED);
	en_off_plc = getgpiopin(NULL, "en_off_plc", GPIO_PIN_NOTDEFINED);
	reset_plc = getgpiopin(NULL, "reset_plc", GPIO_PIN_NOTDEFINED);

	if (reset_plc != GPIO_PIN_NOTDEFINED) {
		reset_plc_mask = 1 << reset_plc;

		si_gpioout(sih, reset_plc_mask, reset_plc_mask, GPIO_HI_PRIORITY);
		si_gpioouten(sih, reset_plc_mask, reset_plc_mask, GPIO_HI_PRIORITY);

		if ((refvdd_off_plc != GPIO_PIN_NOTDEFINED) &&
		    (en_off_plc != GPIO_PIN_NOTDEFINED)) {
			refvdd_off_plc_mask = 1 << refvdd_off_plc;
			en_off_plc_mask = 1 << en_off_plc;

			si_gpioout(sih, refvdd_off_plc_mask, 0, GPIO_HI_PRIORITY);
			si_gpioouten(sih, refvdd_off_plc_mask, refvdd_off_plc_mask,
				GPIO_HI_PRIORITY);

			si_gpioout(sih, en_off_plc_mask, en_off_plc_mask, GPIO_HI_PRIORITY);
			si_gpioouten(sih, en_off_plc_mask, en_off_plc_mask, GPIO_HI_PRIORITY);
		}

		/* Reset signal, 150ms at least */
		bcm_mdelay(300);
		si_gpioout(sih, reset_plc_mask, 0, GPIO_HI_PRIORITY);
	}
}

#define PLC_VIDTODEV(et, vid)	\
	(((vid) == (et)->plc.rxvid2) ? (et)->plc.rxdev2 : \
	 ((vid) == (et)->plc.rxvid1) ? (et)->plc.rxdev1 : \
	 ((vid) == (et)->plc.rxvid3) ? (et)->plc.rxdev3 : \
	 ((vid) == (et)->plc.txvid)  ? (et)->plc.txdev : NULL)

static int
et_plc_netdev_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct net_device *real_dev, *vl_dev;
	et_info_t *et;
	uint16 vid;

	/* we are only interested in plc vifs */
	vl_dev = (struct net_device *)ptr;
	if ((vl_dev->priv_flags & IFF_802_1Q_VLAN) == 0)
		return NOTIFY_DONE;

	/* get the pointer to real device */
	real_dev = VLAN_DEV_INFO(vl_dev)->real_dev;
	vid = VLAN_DEV_INFO(vl_dev)->vlan_id;

	et = ET_INFO(real_dev);
	if (et == NULL) {
		ET_ERROR(("%s: not an ethernet vlan\n", __FUNCTION__));
		return NOTIFY_DONE;
	}

	ET_TRACE(("et%d: %s: event %ld for %s\n", et->etc->unit, __FUNCTION__,
	          event, vl_dev->name));

	switch (event) {
	case NETDEV_REGISTER:
	case NETDEV_UP:
	case NETDEV_CHANGE:
		/* save the netdev pointers of plc vifs when corresponding
		 * interface register event is received.
		 */
		if (vid == et->plc.txvid)
			et->plc.txdev = vl_dev;
		else if (vid == et->plc.rxvid1)
			et->plc.rxdev1 = vl_dev;
		else if (vid == et->plc.rxvid2)
			et->plc.rxdev2 = vl_dev;
		else if (vid == et->plc.rxvid3)
			et->plc.rxdev3 = vl_dev;
		else
			;
		break;

	case NETDEV_UNREGISTER:
	case NETDEV_DOWN:
		/* clear the netdev pointers of plc vifs when corresponding
		 * interface unregister event is received.
		 */
		if (vid == et->plc.txvid)
			et->plc.txdev = NULL;
		else if (vid == et->plc.rxvid1)
			et->plc.rxdev1 = NULL;
		else if (vid == et->plc.rxvid2)
			et->plc.rxdev2 = NULL;
		else if (vid == et->plc.rxvid3)
			et->plc.rxdev3 = NULL;
		else
			;
		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block et_plc_netdev_notifier = {
	.notifier_call = et_plc_netdev_event
};

static inline int32
et_plc_recv(et_info_t *et, struct sk_buff *skb)
{
	struct net_device *plc_dev;
	struct ethervlan_header *evh;
	uint8 *pkt_data;
	static uint8 snap_const[] = {0xaa, 0xaa, 0x03, 0x00, 0x1f, 0x84, 0x89, 0x12};

	pkt_data = (uint8 *)PKTDATA(et->osh, skb);
	evh = (struct ethervlan_header *)pkt_data;

	/* all incoming frames from plc are vlan tagged */
	if (evh->vlan_type != HTON16(ETHER_TYPE_8021Q))
		return -1;

	ASSERT((NTOH16(evh->vlan_tag) & VLAN_VID_MASK) != 3);

	plc_dev = PLC_VIDTODEV(et, NTOH16(evh->vlan_tag) & VLAN_VID_MASK);

#ifdef ET_PLC_NO_VLAN_TAG_RX
	/* The untagged packet from plc 60321 will be tagged with VID=7 by Robo Switch.
	 * Check ethernet type to tell it from the packet tagged with VID=7 from 60321.
	 */
	if (plc_dev == NULL) {
		int vid = NTOH16(evh->vlan_tag) & VLAN_VID_MASK;

		/* Skip when is a PLC control packet or a gigle daemon packet */
		if ((vid != 7) ||
		    (evh->ether_type == HTON16(ETHER_TYPE_GIGLED)) ||
			(evh->ether_type == HTON16(ETHER_TYPE_8912)))
			return -1;

		/* Skip when it is a PLC BOOT packet */
		if (NTOH16(evh->ether_type) < ETHER_TYPE_MIN) {
			/* Check snap field */
			pkt_data += sizeof(struct ethervlan_header);
			if (!memcmp(pkt_data, snap_const, sizeof(snap_const)))
				return -1;
		}

		/* Skip in case of plc rxvid1 is NULL */
		if (et->plc.rxdev1 == NULL)
			return -1;

		/* Change VID=7 to be plc rxvid1 to emulate it's received by plv rxdev1 */
		plc_dev = et->plc.rxdev1;
		evh->vlan_tag &= ~HTON16(VLAN_VID_MASK);
		evh->vlan_tag |= HTON16(et->plc.rxvid1);
	}
#else
	if (plc_dev == NULL)
		return -1;
#endif /* ET_PLC_NO_VLAN_TAG_RX */

	/* call the master hook function to route frames to appropriate
	 * transmit interface.
	 */
	if (plc_dev->master_hook != NULL) {
		PKTSETPRIO(skb, (NTOH16(evh->vlan_tag) >> VLAN_PRI_SHIFT) & VLAN_PRI_MASK);
		if (plc_dev->master_hook(skb, plc_dev, plc_dev->master_hook_arg) == 0) {
			struct net_device_stats *stats;
			stats = vlan_dev_get_stats(plc_dev);
			stats->rx_packets++;
			stats->rx_bytes += skb->len;
			return 0;
		}
		skb->dev = plc_dev->master;
	}

	return -1;
}
#endif /* PLC */

static int __devinit
et_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct net_device *dev = NULL;
	et_info_t *et;
	osl_t *osh;
	char name[128];
	int i, coreunit = et_found, err;
#ifdef PLC
	int8 *var;
#endif /* PLC */
	/* Scan based on core unit.
	  * Map core unit to nvram unit for FA, otherwise, core unit is equal to nvram unit
	  */
	int unit = coreunit;
#ifdef ETAGG
	static bool aggdev_registered = FALSE;
#endif /* ETAGG */

	ET_TRACE(("et core%d: et_probe: bus %d slot %d func %d irq %d\n", coreunit,
	          pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn), pdev->irq));

	if (!etc_chipmatch(pdev->vendor, pdev->device))
		return -ENODEV;

#ifdef ETFA
	sprintf(name, "et%dmacaddr", unit);
	if (getvar(NULL, name))
		fa_set_aux_unit(si_kattach(SI_OSH), unit);
#endif /* ETFA */

	/* Map core unit to nvram unit for FA, otherwise, core unit is equal to nvram unit */
	etc_unitmap(pdev->vendor, pdev->device, coreunit, &unit);

	/* Advanced for next core unit */
	et_found++;

#ifdef ETFA
	/* Get mac address from nvram */
	if (fa_get_macaddr(si_kattach(SI_OSH), NULL, unit) == NULL) {
		ET_ERROR(("et core%d: can not bind to et%d\n", coreunit, unit));
		return -ENODEV;
	}
#else
	/* pre-qualify et unit, that can save the effort to do et_detach */
	sprintf(name, "et%dmacaddr", unit);
	if (getvar(NULL, name) == NULL) {
		ET_ERROR(("et%d: et%dmacaddr not found, ignore it\n", unit, unit));
		ET_ERROR(("et core%d: can not bind to et%d\n", coreunit, unit));
		return -ENODEV;
	}
#endif /* ETFA */

	/* Use ET_ERROR to print core unit to nvram unit mapping always */
	ET_ERROR(("et core%d: bind to et%d\n", coreunit, unit));

	osh = osl_attach(pdev, PCI_BUS, FALSE);
	ASSERT(osh);

	/* Set ACP coherence flag */
	if (OSL_ACP_WAR_ENAB() || OSL_ARCH_IS_COHERENT())
		osl_flag_set(osh, OSL_ACP_COHERENCE);

	pci_set_master(pdev);
	if ((err = pci_enable_device(pdev)) != 0) {
		ET_ERROR(("et%d: et_probe: pci_enable_device() failed\n", unit));
		osl_detach(osh);
		return err;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	if ((dev = alloc_etherdev(sizeof(et_info_t))) == NULL) {
		ET_ERROR(("et%d: et_probe: alloc_etherdev() failed\n", unit));
		osl_detach(osh);
		return -ENOMEM;
	}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
	if ((dev = alloc_etherdev(0)) == NULL) {
		ET_ERROR(("et%d: et_probe: alloc_etherdev() failed\n", unit));
		osl_detach(osh);
		return -ENOMEM;
	}
	/* allocate private info */
	if ((et = (et_info_t *)MALLOC(osh, sizeof(et_info_t))) == NULL) {
		ET_ERROR(("et%d: et_probe: out of memory, malloced %d bytes\n", unit,
		          MALLOCED(osh)));
		MFREE(osh, dev, sizeof(et_info_t));
		osl_detach(osh);
		return -ENOMEM;
	}
	dev->priv = (void *)et;
#else
	if (!(dev = (struct net_device *) MALLOC(osh, sizeof(struct net_device)))) {
		ET_ERROR(("et%d: et_probe: out of memory, malloced %d bytes\n", unit,
		          MALLOCED(osh)));
		osl_detach(osh);
		return -ENOMEM;
	}
	bzero(dev, sizeof(struct net_device));

	if (!init_etherdev(dev, 0)) {
		ET_ERROR(("et%d: et_probe: init_etherdev() failed\n", unit));
		MFREE(osh, dev, sizeof(struct net_device));
		osl_detach(osh);
		return -ENOMEM;
	}
	/* allocate private info */
	if ((et = (et_info_t *)MALLOC(osh, sizeof(et_info_t))) == NULL) {
		ET_ERROR(("et%d: et_probe: out of memory, malloced %d bytes\n", unit,
		          MALLOCED(osh)));
		MFREE(osh, dev, sizeof(et_info_t));
		osl_detach(osh);
		return -ENOMEM;
	}
	dev->priv = (void *)et;
#endif /* < 2.6.0 */

	et = ET_INFO(dev);
	bzero(et, sizeof(et_info_t));	/* Is this needed in 2.6.36 ? -LR */
	et->dev = dev;
	et->pdev = pdev;
	et->osh = osh;
	pci_set_drvdata(pdev, et);

	/* map chip registers (47xx: and sprom) */
	dev->base_addr = pci_resource_start(pdev, 0);
	if ((et->regsva = ioremap_nocache(dev->base_addr, PCI_BAR0_WINSZ)) == NULL) {
		ET_ERROR(("et%d: ioremap() failed\n", unit));
		goto fail;
	}

	init_MUTEX(&et->sem);
	spin_lock_init(&et->lock);
	spin_lock_init(&et->txq_lock);
	spin_lock_init(&et->isr_lock);

	for (i = 0; i < NUMTXQ; i++)
		skb_queue_head_init(&et->txq[i]);

	/* common load-time initialization */
	et->etc = etc_attach((void *)et, pdev->vendor, pdev->device, coreunit, osh, et->regsva);
	if (et->etc == NULL) {
		ET_ERROR(("et%d: etc_attach() failed\n", unit));
		goto fail;
	}

#ifdef ETFA
	if (et->etc->fa) {
		fa_set_name(et->etc->fa, dev->name);
	}
	et->fa_bhdr_sz = FA_RX_BCM_HDR((fa_t *)et->etc->fa)
	               ? sizeof(bcm_hdr_t) : 0;
	et->fa_aux_dev = FA_IS_AUX_DEV((fa_t *)et->etc->fa)
		           ? TRUE : FALSE;

#ifdef ETAGG
	if (et->fa_bhdr_sz) {
		et->bhdr_sz = et->fa_bhdr_sz;
		/* bhdr would be inserted before dest mac address of packets */
		et->bhdr_roff = 0 - et->bhdr_sz;
	}
#endif	/* ETAGG */
#endif /* ETFA */

#ifdef ETAGG
	if (et->etc->agg) {
		if (!aggdev_registered) {
			et->aggdev = et_aggdev_register(osh, dev, unit);
			if (et->aggdev)
				aggdev_registered = TRUE;
		}

		if ((et->aggdev != NULL) && (et->bhdr_sz == 0)) {
			et->bhdr_sz = sizeof(bcm53125_hdr_t);
			/* bhdr would be inserted after src mac address of packets */
			et->bhdr_roff = ETHER_ADDR_LEN * 2;
		}

		ASSERT(et->bhdr_sz == sizeof(bcm53125_hdr_t));
	}
#endif /* ETAGG */

#if defined(BCM_GMAC3)
	fwder_init(); /* initialize the fwder */

	et->fwder_up = FALSE; /* et_forward() enabled/disabled on et_up/et_down */
	et->fwdh = (struct fwder *)NULL;

	if (DEV_FWDER(et->etc)) { /* Attach driver to forwarder */
		/* Ethernet network interface uses "ethXX". Use "fwdXX" instead */
		strncpy(dev->name, DEV_FWDER_NAME, 3);

		/* Store the rx lazy configuration */
		etc_rxlazy(et->etc, et_rxlazy_timeout, et_rxlazy_framecnt);

		/*
		 * Attach my transmit handler to UPSTREAM fwder instance on core=unit
		 *      wl# MAC -> wl_sendup -> et_forward -> et::GMAC#
		 * and get the DNSTREAM direction transmit handler for use in sendup.
		 *      et_sendup/chain -> et->fwdh->bypass_fn=wl_forward -> wl_start
		 */
		et->fwdh = fwder_attach(FWDER_UPSTREAM, et->etc->coreunit,
		                        FWDER_NIC_MODE, et_forward, et->dev, et->osh);
		if (et->fwdh) {
			et->fwdh->dataoff = GMAC_FWDER_HWRXOFF;
#ifdef ETFA
			et->fwdh->dataoff += et->fa_bhdr_sz;
#endif
		}
		ET_TRACE(("et%d: unit<%d> fwder<%p>\n", unit, et->etc->coreunit, et->fwdh));
	}
#endif /* BCM_GMAC3 */

#ifdef HNDCTF
	/* Normally we do ctf_attach for each ethernet devices but here we have to ignore
	 * the aux device which invoked by the FA.  Because the aux device is a FA auxiliary
	 * device, the skb->dev for all the packets from aux will be change to FA dev later.
	 */
	if (CTF_CAPABLE_DEV(et)) {
		et->cih = ctf_attach(osh, dev->name, &et_msg_level, et_ctf_detach, et);

		if (DEV_NTKIF(et->etc)) {   /* Bind CTF to ethernet network interface */
			if (ctf_dev_register(et->cih, dev, FALSE) != BCME_OK) {
				ET_ERROR(("et%d: ctf_dev_register() failed\n", unit));
				goto fail;
			}
		}

#ifdef ETFA
		if (FA_IS_FA_DEV((fa_t *)et->etc->fa)) {
			if (getintvar(NULL, "ctf_fa_mode") == CTF_FA_NORMAL) {
				ctf_fa_register(et->cih, (ctf_fa_cb_t)et_fa_normal_cb, dev);
			}
			else {
				ctf_fa_register(et->cih, (ctf_fa_cb_t)et_fa_default_cb, dev);
			}
		}
#endif

#ifdef CTFPOOL
		/* create ctf packet pool with specified number of buffers */
		if (CTF_ENAB(et->cih)) {
			uint32 poolsz;
			/* use large ctf poolsz for platforms with more memory */
			poolsz = ((num_physpages >= 32767) ? CTFPOOLSZ * 2 :
			          ((num_physpages >= 8192) ? CTFPOOLSZ : 0));
#if defined(BCM_GMAC3)
                        /* Atlas-II w/ 256M: 4K sized CTFPOOL
                         * Need minimum of 3K packets:
                         *   512 rx ring + 2048 flowring backup queue  + 512 flowring
                         */
                        if (DEV_FWDER(et->etc) && (num_physpages >= 65535)) {
                                poolsz = CTFPOOLSZ * 4;
                        }
#endif /* BCM_GMAC3 */
			if ((poolsz > 0) &&
			    (osl_ctfpool_init(osh, poolsz, RXBUFSZ+BCMEXTRAHDROOM) < 0)) {
				ET_ERROR(("et%d: chipattach: ctfpool alloc/init failed\n", unit));
				goto fail;
			}
		}
#endif /* CTFPOOL */
	}
#endif /* HNDCTF */

	bcopy(&et->etc->cur_etheraddr, dev->dev_addr, ETHER_ADDR_LEN);

	/* init 1 second watchdog timer */
	init_timer(&et->timer);
	et->timer.data = (ulong)dev;
	et->timer.function = et_watchdog;

#ifdef CONFIG_SMP
	/* initialize number of online cpus */
	online_cpus = num_online_cpus();
#if defined(__ARM_ARCH_7A__)
	if (online_cpus > 1) {
		if (et_txq_thresh == 0) {
			et_txq_thresh = ET_TXQ_THRESH_DEFAULT;
		}
	}
#endif /* __ARM_ARCH_7A__ */
#else
	online_cpus = 1;
#endif /* CONFIG_SMP */
	ET_ERROR(("et%d: online cpus %d\n", unit, online_cpus));

#ifdef	NAPI2_POLL
	netif_napi_add(dev, & et->napi_poll, et_poll, 64);
	napi_enable(&et->napi_poll);
#endif	/* NAPI2_POLL */

#if !defined(NAPI_POLL) && !defined(NAPI2_POLL)
	/* setup the bottom half handler */
	tasklet_init(&et->tasklet, et_dpc, (ulong)et);
#endif /*! NAPIx_POLL */

	tasklet_init(&et->tx_tasklet, et_tx_tasklet, (ulong)et);
	tasklet_disable(&et->tx_tasklet);

#ifdef ET_ALL_PASSIVE
	if (ET_ALL_PASSIVE_ENAB(et)) {
		MY_INIT_WORK(&et->dpc_task.work, (work_func_t)et_dpc_work);
		et->dpc_task.context = et;
		MY_INIT_WORK(&et->txq_task.work, (work_func_t)et_txq_work);
		et->txq_task.context = et;
		MY_INIT_WORK(&et->wd_task.work, (work_func_t)et_watchdog_task);
		et->wd_task.context = et;
	} else if (txworkq) {
		MY_INIT_WORK(&et->txq_task.work, (work_func_t)et_txq_work);
		et->txq_task.context = et;
	}
	et->all_dispatch_mode = (passivemode == 0) ? TRUE : FALSE;
#endif  /* ET_ALL_PASSIVE */

	/* register our interrupt handler */
	if (request_irq(pdev->irq, et_isr, IRQF_SHARED, dev->name, et)) {
		ET_ERROR(("et%d: request_irq() failed\n", unit));
		goto fail;
	}
	dev->irq = pdev->irq;

#if defined(BCM_GMAC3)
	if (DEV_FWDER(et->etc)) {
		/* Setup GMAC forwarder irq affinity by fwder unit. */
		fwder_affinity(FWDER_DNSTREAM, et->etc->coreunit, pdev->irq);
	}
#endif /* BCM_GMAC3 */

	/* add us to the global linked list */
	et->next = et_list;
	et_list = et;

#ifndef HAVE_NET_DEVICE_OPS
	/* lastly, enable our entry points */
	dev->open = et_open;
	dev->stop = et_close;
#if defined(BCM_GMAC3)
	if (DEV_FWDER(et->etc))
		dev->hard_start_xmit = et_dummy_start;
	else
#endif /* BCM_GMAC3 */
		dev->hard_start_xmit = et_start;
	dev->get_stats = et_get_stats;
	dev->set_mac_address = et_set_mac_address;
	dev->set_multicast_list = et_set_multicast_list;
	dev->do_ioctl = et_ioctl;
#ifdef NAPI_POLL
	dev->poll = et_poll;
	dev->weight = (ET_GMAC(et->etc) ? 64 : 32);
#endif /* NAPI_POLL */
#else /* ! HAVE_NET_DEVICE_OPS */
	/* Linux 2.6.36 and up. - LR */
#if defined(BCM_GMAC3)
	if (DEV_FWDER(et->etc))
		dev->netdev_ops = &et_gmac3_netdev_ops;
	else
#endif /* BCM_GMAC3 */
		dev->netdev_ops = &et_netdev_ops;
#ifdef NAPI_POLL
	dev->poll = et_poll;
	dev->weight = (ET_GMAC(et->etc) ? 64 : 32);
#endif /* NAPI_POLL */

#endif /* HAVE_NET_DEVICE_OPS */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	dev->ethtool_ops = &et_ethtool_ops;
#endif

	if (register_netdev(dev)) {
		ET_ERROR(("et%d: register_netdev() failed\n", unit));
		goto fail;
	}

#ifdef __ARM_ARCH_7A__
	dev->features = (NETIF_F_SG | NETIF_F_FRAGLIST | NETIF_F_ALL_CSUM);
#endif

	/* print hello string */
	(*et->etc->chops->longname)(et->etc->ch, name, sizeof(name));
	printf("%s: %s %s\n", dev->name, name, EPI_VERSION_STR);

#ifdef HNDCTF
	if (DEV_NTKIF(et->etc)) {   /* Enable CTF on ethernet network interface */
		if (et->cih &&
			ctf_enable(et->cih, dev, TRUE, &et->brc_hot) != BCME_OK) {
			ET_ERROR(("et%d: ctf_enable() failed\n", unit));
			goto fail;
		}
	}
#endif /* HNDCTF */

	ET_TRACE(("et%d: registered dev<%p,%s> ET<%u:%s> irq<%u>\n", unit,
	         dev, dev->name, et->etc->coreunit,
	         DEV_NTKIF(et->etc) ? "NETWORKIF" : "FORWARDER", dev->irq));

#ifdef PLC
	/* read plc_vifs to initialize the vids to use for receiving
	 * and forwarding the frames.
	 */
	var = getvar(NULL, "plc_vifs");

	if (var == NULL) {
		ET_ERROR(("et%d: %s: PLC vifs not configured\n", unit, __FUNCTION__));
		return (0);
	}

	et_plc_reset();

	et->plc.hw = TRUE;

	/* initialize the vids to use for plc rx and tx */
	sscanf(var, "vlan%d vlan%d vlan%d vlan%d",
	       &et->plc.txvid, &et->plc.rxvid1, &et->plc.rxvid2, &et->plc.rxvid3);

	ET_ERROR(("et%d: %s: PLC vifs %s\n", unit, __FUNCTION__, var));

	/* register a callback to be called on plc dev create event */
	register_netdevice_notifier(&et_plc_netdev_notifier);
#endif /* PLC */

	return (0);

fail:
	et_free(et);
	return (-ENODEV);
}

static int
et_suspend(struct pci_dev *pdev, DRV_SUSPEND_STATE_TYPE state)
{
	et_info_t *et;

	if ((et = (et_info_t *) pci_get_drvdata(pdev))) {
		netif_device_detach(et->dev);
		ET_LOCK(et);
		et_down(et, 1);
		ET_UNLOCK(et);
	}

	return 0;
}

static int
et_resume(struct pci_dev *pdev)
{
	et_info_t *et;

	if ((et = (et_info_t *) pci_get_drvdata(pdev))) {
		ET_LOCK(et);
		et_up(et);
		ET_UNLOCK(et);
		netif_device_attach(et->dev);
	}

	return 0;
}

/* Compatibility routines */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 6)
static void
_et_suspend(struct pci_dev *pdev)
{
	et_suspend(pdev, 0);
}

static void
_et_resume(struct pci_dev *pdev)
{
	et_resume(pdev);
}
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 6) */

static void __devexit
et_remove(struct pci_dev *pdev)
{
	et_info_t *et;

	if (!etc_chipmatch(pdev->vendor, pdev->device))
		return;

#if defined(BCM_GMAC3)
	fwder_exit();
#endif /* BCM_GMAC3 */

#ifdef PLC
	/* Un-register us from receiving netdevice events */
	unregister_netdevice_notifier(&et_plc_netdev_notifier);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 12)
	et_suspend(pdev, 0);
#else
	et_suspend(pdev, PMSG_SUSPEND);
#endif

	if ((et = (et_info_t *) pci_get_drvdata(pdev))) {
		et_free(et);
		pci_set_drvdata(pdev, NULL);
	}
}

static struct pci_driver et_pci_driver = {
	name:		"et",
	probe:		et_probe,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 6)
	suspend:	_et_suspend,
	resume:		_et_resume,
#else
	suspend:	et_suspend,
	resume:		et_resume,
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 6) */
	remove:		__devexit_p(et_remove),
	id_table:	et_id_table,
	};

static int __init
et_module_init(void)
{
	char * var;

#if defined(BCMDBG)
	if (msglevel != 0xdeadbeef)
		et_msg_level = msglevel;
	else {
		var = getvar(NULL, "et_msglevel");
		if (var)
			et_msg_level = bcm_strtoul(var, NULL, 0);
	}

	printf("%s: msglevel set to 0x%x\n", __FUNCTION__, et_msg_level);
#endif /* defined(BCMDBG) */

#ifdef ET_ALL_PASSIVE
	{
		var = getvar(NULL, "et_dispatch_mode");
		if (var)
			passivemode = bcm_strtoul(var, NULL, 0);
		printf("%s: passivemode set to 0x%x\n", __FUNCTION__, passivemode);
		var = getvar(NULL, "txworkq");
		if (var)
			txworkq = bcm_strtoul(var, NULL, 0);
		printf("%s: txworkq set to 0x%x\n", __FUNCTION__, txworkq);
	}
#endif /* ET_ALL_PASSIVE */
	var = getvar(NULL, "et_txq_thresh");
	if (var)
		et_txq_thresh = bcm_strtoul(var, NULL, 0);
	printf("%s: et_txq_thresh set to 0x%x\n", __FUNCTION__, et_txq_thresh);

	var = getvar(NULL, "et_rxlazy_timeout");
	if (var)
		et_rxlazy_timeout = bcm_strtoul(var, NULL, 0);
	printf("%s: et_rxlazy_timeout set to 0x%x\n", __FUNCTION__, et_rxlazy_timeout);

	var = getvar(NULL, "et_rxlazy_framecnt");
	if (var)
		et_rxlazy_framecnt = bcm_strtoul(var, NULL, 0);
	printf("%s: et_rxlazy_framecnt set to 0x%x\n", __FUNCTION__, et_rxlazy_framecnt);

        var = getvar(NULL, "et_rxlazy_dyn_thresh");
        if (var)
                et_rxlazy_dyn_thresh = bcm_strtoul(var, NULL, 0);
        printf("%s: et_rxlazy_dyn_thresh set to %d\n", __FUNCTION__, et_rxlazy_dyn_thresh);

	return pci_module_init(&et_pci_driver);
}

static void __exit
et_module_exit(void)
{
	pci_unregister_driver(&et_pci_driver);
}

module_init(et_module_init);
module_exit(et_module_exit);

static void
et_free(et_info_t *et)
{
	et_info_t **prev;
	osl_t *osh;

	if (et == NULL)
		return;

	ET_TRACE(("et: et_free\n"));

#if defined(BCM_GMAC3)
	if (DEV_FWDER(et->etc)) {
		et->fwder_up = FALSE; /* Disable et_forward() */
		ET_TRACE(("et%d: fwder<%d> dettached\n", et->etc->unit, et->etc->coreunit));
		/* Dettach driver from fwder */
		et->fwdh = fwder_dettach(et->fwdh, FWDER_UPSTREAM, et->etc->coreunit);
	}
#endif /* BCM_GMAC3 */

	if (et->dev && et->dev->irq)
		free_irq(et->dev->irq, et);

#ifdef	NAPI2_POLL
	napi_disable(&et->napi_poll);
	netif_napi_del(&et->napi_poll);
#endif	/* NAPI2_POLL */

#ifdef HNDCTF
	if (et->cih)
		ctf_dev_unregister(et->cih, et->dev);
#endif /* HNDCTF */

#ifdef ETAGG
	if (et->aggdev) {
		et_aggdev_unregister(et->osh, (struct net_device *)et->aggdev);
		et->aggdev = NULL;
	}
#endif /* ETAGG */

	if (et->dev) {
		unregister_netdev(et->dev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
		free_netdev(et->dev);
#else
		MFREE(et->osh, et->dev, sizeof(struct net_device));
#endif
		et->dev = NULL;
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36) */
	}

#ifdef CTFPOOL
	/* free the buffers in fast pool */
	osl_ctfpool_cleanup(et->osh);
#endif /* CTFPOOL */

#ifdef HNDCTF
	/* free ctf resources */
	if (et->cih)
		ctf_detach(et->cih);
#endif /* HNDCTF */

	/* free common resources */
	if (et->etc) {
		etc_detach(et->etc);
		et->etc = NULL;
	}

	/*
	 * unregister_netdev() calls get_stats() which may read chip registers
	 * so we cannot unmap the chip registers until after calling unregister_netdev() .
	 */
	if (et->regsva) {
		iounmap((void *)et->regsva);
		et->regsva = NULL;
	}

	/* remove us from the global linked list */
	for (prev = &et_list; *prev; prev = &(*prev)->next)
		if (*prev == et) {
			*prev = et->next;
			break;
		}

	osh = et->osh;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	if (et->dev) {
		free_netdev(et->dev);
		et->dev = NULL;
	}
#else
	MFREE(et->osh, et, sizeof(et_info_t));
#endif

#ifdef BCMDBG_CTRACE
	PKT_CTRACE_DUMP(osh, NULL);
#endif

	if (MALLOCED(osh))
		printf("Memory leak of bytes %d\n", MALLOCED(osh));
	ASSERT(MALLOCED(osh) == 0);

	osl_detach(osh);
}

static int
et_open(struct net_device *dev)
{
	et_info_t *et;

	et = ET_INFO(dev);

	ET_TRACE(("et%d: et_open\n", et->etc->unit));

	et->etc->promisc = (dev->flags & IFF_PROMISC)? TRUE: FALSE;
	et->etc->allmulti = (dev->flags & IFF_ALLMULTI)? TRUE: et->etc->promisc;

	ET_LOCK(et);
	et_up(et);
	ET_UNLOCK(et);

	OLD_MOD_INC_USE_COUNT;

	return (0);
}

static int
et_close(struct net_device *dev)
{
	et_info_t *et;

	et = ET_INFO(dev);

	ET_TRACE(("et%d: et_close\n", et->etc->unit));

	et->etc->promisc = FALSE;
	et->etc->allmulti = FALSE;

	ET_LOCK(et);
	et_down(et, 1);
	ET_UNLOCK(et);

	OLD_MOD_DEC_USE_COUNT;

	return (0);
}


static void BCMFASTPATH
et_txq_work(struct et_task *task)
{
	et_info_t *et = (et_info_t *)task->context;

	ET_LOCK(et);
	et_sendnext(et);
	ET_UNLOCK(et);

	return;
}

/*
 * Driver level checksum offload. This is being done so that we can advertise
 * checksum offload support to Linux.
 */
static void BCMFASTPATH_HOST
et_cso(et_info_t *et, struct sk_buff *skb)
{
	struct ethervlan_header *evh;
	uint8 *th = skb_transport_header(skb);
	uint16 thoff, eth_type, *check;
	uint8 prot;

	ASSERT(!PKTISCTF(et->osh, skb));

	evh = (struct ethervlan_header *)PKTDATA(et->osh, skb);
	eth_type = ((evh->vlan_type == HTON16(ETHER_TYPE_8021Q)) ?
	            evh->ether_type : evh->vlan_type);

	/* tcp/udp checksum calculation */
	thoff = (th - skb->data);
	if (eth_type == HTON16(ETHER_TYPE_IP)) {
		struct iphdr *ih = ip_hdr(skb);
		prot = ih->protocol;
		ASSERT((prot == IP_PROT_TCP) || (prot == IP_PROT_UDP));
		check = (uint16 *)(th + ((prot == IP_PROT_UDP) ?
			offsetof(struct udphdr, check) : offsetof(struct tcphdr, check)));
		*check = 0;
		skb->csum = skb_checksum(skb, thoff, skb->len - thoff, 0);
		*check = csum_tcpudp_magic(ih->saddr, ih->daddr,
		                           skb->len - thoff, prot, skb->csum);
	} else if (eth_type == HTON16(ETHER_TYPE_IPV6)) {
		struct ipv6hdr *ih = ipv6_hdr(skb);
		prot = IPV6_PROT(ih);
		ASSERT((prot == IP_PROT_TCP) || (prot == IP_PROT_UDP));
		check = (uint16 *)(th + ((prot == IP_PROT_UDP) ?
			offsetof(struct udphdr, check) : offsetof(struct tcphdr, check)));
		*check = 0;
		skb->csum = skb_checksum(skb, thoff, skb->len - thoff, 0);
		*check = csum_ipv6_magic(&ih->saddr, &ih->daddr,
		                         skb->len - thoff, prot, skb->csum);
	} else
		return;

	if ((*check == 0) && (prot == IP_PROT_UDP))
		*check = CSUM_MANGLED_0;
}
/*
 * Called in non-passive mode when we need to send frames received on other CPU.
 */
static void BCMFASTPATH
et_sched_tx_tasklet(void *t)
{
	et_info_t *et = (et_info_t *)t;
	tasklet_schedule(&et->tx_tasklet);
}

#if defined(BCM_GMAC3)
/**
 * BCM_GMAC3: Forwarder transmit handler.
 * WLAN driver may forward a packet chain, and not an array of packet pointers.
 */
static int BCMFASTPATH
et_forward(struct fwder *fwder, struct sk_buff *skbs, int skb_cnt,
           struct net_device *rx_dev)
{
	et_info_t *et;

	/* Use the attached default net_device. */
	ASSERT(fwder != NULL);
	ASSERT(fwder->dev_def != NULL);

	et = ET_INFO(fwder->dev_def);

	if (!et->fwder_up)
		return -ENETDOWN;

	et_start((struct sk_buff *)skbs, fwder->dev_def);

	return FWDER_SUCCESS;
}

static int
et_dummy_start(struct sk_buff *skb, struct net_device *dev)
{
	et_info_t *et = ET_INFO(dev);

	PKTFRMNATIVE(et->osh, skb);
	PKTFREE(et->osh, skb, TRUE);
	return 0;
}
#endif /* BCM_GMAC3 */

/*
 * Below wrapper functions for skb queue/dequeue maintain packet counters.
 * In case of packet chaining, number of entries is not equal to no. of packets.
 * Queuing threshold is to be compared against total packet count in a queue and
 * not against the queue length.
 */

static void BCMFASTPATH
et_skb_queue_tail(et_info_t *et, int qid,  struct sk_buff *skb)
{
	__skb_queue_tail(&et->txq[qid], skb);
	et->txq_pktcnt[qid] += (PKTISCHAINED(skb) ? PKTCCNT(skb): 1);
}

static struct sk_buff * BCMFASTPATH
et_skb_dequeue(et_info_t *et, int qid)
{
	struct sk_buff *skb;

	skb = __skb_dequeue(&et->txq[qid]);
	if (!skb) return skb;

	ASSERT(et->txq_pktcnt[qid] > 0);
	et->txq_pktcnt[qid] -= (PKTISCHAINED(skb) ? PKTCCNT(skb): 1);
	ASSERT(et->txq_pktcnt[qid] >= 0);

	return skb;
}

/*
 * Yeah, queueing the packets on a tx queue instead of throwing them
 * directly into the descriptor ring in the case of dma is kinda lame,
 * but this results in a unified transmit path for both dma and pio
 * and localizes/simplifies the netif_*_queue semantics, too.
 */
#ifdef CONFIG_SMP
#define ET_CONFIG_SMP()	TRUE
#else
#define ET_CONFIG_SMP()	FALSE
#endif /* CONFIG_SMP */
#if defined(CONFIG_ET) || defined(CONFIG_ET_MODULE)
#define ET_RTR()	TRUE
#else
#define ET_RTR()	FALSE
#endif /* CONFIG_ET */
static int BCMFASTPATH
et_start(struct sk_buff *skb, struct net_device *dev)
{
	et_info_t *et;
	uint32 q = 0;

	et = ET_INFO(dev);

	if (PKTISFAAUX(skb)) {
		PKTCLRFAAUX(skb);
		PKTFRMNATIVE(et->osh, skb);
		PKTCFREE(et->osh, skb, TRUE);
		return 0;
	}

	if (ET_GMAC(et->etc) && (et->etc->qos))
		q = etc_up2tc(PKTPRIO(skb));

	ET_TRACE(("et%d: et_start: len %d\n", et->etc->unit, skb->len));
	ET_LOG("et%d: et_start: len %d", et->etc->unit, skb->len);

	ET_TXQ_LOCK(et);

	if (et_txq_thresh && (et->txq_pktcnt[q] >= et_txq_thresh)) {
		ET_TXQ_UNLOCK(et);
		PKTFRMNATIVE(et->osh, skb);
		PKTCFREE(et->osh, skb, TRUE);
		return 0;
	}

	/* put it on the tx queue and call sendnext */
	et_skb_queue_tail(et, q, skb);
	et->etc->txq_state |= (1 << q);
	ET_TXQ_UNLOCK(et);

	/* Call in the same context when we are UP and non-passive is enabled */
	if (ET_ALL_PASSIVE_ENAB(et) || (ET_RTR() && ET_CONFIG_SMP())) {
		/* In smp non passive mode, schedule tasklet for tx */
		if (!ET_ALL_PASSIVE_ENAB(et) && txworkq == 0)
			et_sched_tx_tasklet(et);
#ifdef ET_ALL_PASSIVE
		else {
#ifdef CONFIG_SMP
			if (online_cpus > 1)
			schedule_work_on(1 - raw_smp_processor_id(), &et->txq_task.work);
			else
#endif /* CONFIG_SMP */
			schedule_work(&et->txq_task.work);
		}
#endif /* ET_ALL_PASSIVE */
	} else {
			ET_LOCK(et);
		et_sendnext(et);
			ET_UNLOCK(et);
	}

	ET_LOG("et%d: et_start ret\n", et->etc->unit, 0);

	return (0);
}


static void BCMFASTPATH
et_tx_tasklet(ulong data)
{
	et_task_t task;
	task.context = (void *)data;
	et_txq_work(&task);
}

static void BCMFASTPATH
et_sendnext(et_info_t *et)
{
	etc_info_t *etc;
	struct sk_buff *skb;
	void *p, *n;
	uint32 priq = TX_Q0;
	uint16 vlan_type;
#ifdef DMA
	uint32 txavail;
	uint32 pktcnt;
#endif
#ifdef ETAGG
	void *bhdrp = NULL;
	uint16 dest_pid = AGG_INVALID_PID;
#endif /* ETAGG */

	etc = et->etc;

	ET_TRACE(("et%d: et_sendnext\n", etc->unit));
	ET_LOG("et%d: et_sendnext", etc->unit, 0);

	/* dequeue packets from highest priority queue and send */
	while (1) {
		ET_TXQ_LOCK(et);

		if (etc->txq_state == 0)
			break;

		priq = etc_priq(etc->txq_state);

		ET_TRACE(("et%d: txq_state %x priq %d txavail %d\n",
		          etc->unit, etc->txq_state, priq,
		          *(uint *)etc->txavail[priq]));

		if ((skb = skb_peek(&et->txq[priq])) == NULL) {
			etc->txq_state &= ~(1 << priq);
			ET_TXQ_UNLOCK(et);
			continue;
		}

#ifdef DMA
		if (!PKTISCHAINED(skb)) {
			pktcnt = skb_shinfo(skb)->nr_frags + 1;
		} else {
			pktcnt = PKTCCNT(skb);
		}

		/* current highest priority dma queue is full */
		txavail = *(uint *)(etc->txavail[priq]);
		if (txavail < pktcnt)
#else /* DMA */
		if (etc->pioactive != NULL)
#endif /* DMA */
			break;

		skb = et_skb_dequeue(et, priq);

		ET_TXQ_UNLOCK(et);

#if defined(ETFA) || defined(ETAGG)
		/*
		 * When BHDR is enabled for TX, BHDR will be inserted in the packet header.
		 * So we need to reallocate private buffers for shared packets and
		 * convert non-linear small packets to linear for trimming later.
		 */
		if (TX_BCM_HDR_ENAB(et) && !PKTISCHAINED(skb) &&
			(PKTSHARED(skb) ||
			((skb_shinfo(skb)->nr_frags > 1) &&
			(PKTLEN(et->osh, skb) < ETHER_MIN_LEN)))) {
			struct sk_buff *nskb = NULL;

			nskb = skb_copy_expand(skb, PKTHEADROOM(et->osh, skb) + et->bhdr_sz,
				PKTTAILROOM(et->osh, skb), GFP_ATOMIC);
			PKTFRMNATIVE(et->osh, skb);
			PKTFREE(et->osh, skb, TRUE);
			if (!nskb) {
				etc->txnobuf++;
				continue;
			}
			skb = nskb;
		}
#endif /* ETFA || ETAGG */

		ET_PRHDR("tx", (struct ether_header *)skb->data, skb->len, etc->unit);
		ET_PRPKT("txpkt", skb->data, skb->len, etc->unit);

		/* convert the packet. */
		p = PKTFRMNATIVE(etc->osh, skb);
		ASSERT(p != NULL);

		ET_TRACE(("%s: sdu %p chained %d chain sz %d next %p\n",
		          __FUNCTION__, p, PKTISCHAINED(p), PKTCCNT(p), PKTCLINK(p)));

		vlan_type = ((struct ethervlan_header *)PKTDATA(et->osh, p))->vlan_type;
		FOREACH_CHAINED_PKT(p, n) {
			PKTCLRCHAINED(et->osh, p);
			/* replicate vlan header contents from curr frame */
			if ((n != NULL) && (vlan_type == HTON16(ETHER_TYPE_8021Q))) {
				uint8 *n_evh;
				n_evh = PKTPUSH(et->osh, n, VLAN_TAG_LEN);
				*(struct ethervlan_header *)n_evh =
				*(struct ethervlan_header *)PKTDATA(et->osh, p);
			} else if (n == NULL)
				PKTCSETFLAG(p, 1);

			if (PKTSUMNEEDED((struct sk_buff *)p))
				et_cso(et, (struct sk_buff *)p);
#ifdef ETAGG
			/* parse lacp packet and retrive the dest pid before adding brcm header */
			if (AGG_BCM_HDR_ENAB(et->etc->agg)) {
				void *lacph;

				dest_pid = AGG_INVALID_PID;
				lacph = agg_get_lacp_header(PKTDATA(et->osh, p));
				if (lacph)
					dest_pid = agg_get_lacp_dest_pid(lacph);
			}
#endif /* ETAGG */

#ifdef ETFA
			/* If this device is connected to FA, and FA is configures
			 * to receive bcmhdr, add one.
			 */
			if (FA_TX_BCM_HDR((fa_t *)et->etc->fa)) {
				p = fa_process_tx(et->etc->fa, p);

				/* Bypass transmitting since the packet pointer is NULL */
				if (p == NULL)
					continue;
			}
#endif /* ETFA */

#ifdef ETAGG
			if (AGG_BCM_HDR_ENAB(et->etc->agg)) {
				void *orig_p = p;
				unsigned int orig_flags = PKTCGETFLAGS(p);

				bhdrp = NULL;
#ifdef ETFA
				if (FA_TX_BCM_HDR((fa_t *)et->etc->fa))
					bhdrp = PKTDATA(et->osh, p);
#endif /* ETFA */
				/* add brcm header and set destination port via dest_pid */
				p = agg_process_tx(et->etc->agg, p, bhdrp, et->bhdr_roff, dest_pid);
				if (p == NULL)
					continue;

				if (p != orig_p) {
					PKTCSETFLAGS(p, orig_flags);
					PKTSETCLINK((p), NULL);
				}
			}
#endif /* ETAGG */

			(*etc->chops->tx)(etc->ch, p);
			etc->txframe++;
			etc->txbyte += PKTLEN(et->osh, p);
		}
	}

	ET_TXQ_UNLOCK(et);
}

void
et_init(et_info_t *et, uint options)
{
	ET_TRACE(("et%d: et_init\n", et->etc->unit));
	ET_LOG("et%d: et_init", et->etc->unit, 0);


	etc_init(et->etc, options);
}


void
et_reset(et_info_t *et)
{
	ET_TRACE(("et%d: et_reset\n", et->etc->unit));

	etc_reset(et->etc);

	/* zap any pending dpc interrupt bits */
	et->events = 0;

	/* dpc will not be rescheduled */
	et->resched = 0;
}

void
et_up(et_info_t *et)
{
	etc_info_t *etc;

	etc = et->etc;

	if (etc->up)
		return;

	ET_TRACE(("et%d: et_up\n", etc->unit));

	etc_up(etc);

	/* schedule one second watchdog timer */
	et->timer.expires = jiffies + HZ;
	add_timer(&et->timer);
	et->set = TRUE;

	netif_start_queue(et->dev);

	ET_UNLOCK(et);
	tasklet_enable(&et->tx_tasklet);
	ET_LOCK(et);

#if defined(BCM_GMAC3)
	if (DEV_FWDER(et->etc)) {
		ET_TRACE(("et%d: fwder<%d> up", etc->unit, etc->coreunit));
		et->fwder_up = TRUE; /* enable et_forward() */
	}
#endif /* BCM_GMAC3 */

#ifdef ETFA
	if (et->etc->fa)
		fa_et_up(et->etc->fa);
#endif
}

void
et_down(et_info_t *et, int reset)
{
	etc_info_t *etc;
	struct sk_buff *skb;
	int32 i;

	etc = et->etc;

	ET_TRACE(("et%d: et_down\n", etc->unit));

#if defined(BCM_GMAC3)
	if (DEV_FWDER(et->etc)) {
		ET_TRACE(("et%d: fwder<%d> down", etc->unit, etc->coreunit));
		et->fwder_up = FALSE; /* disable et_forward() */
	}
#endif /* BCM_GMAC3 */

	netif_down(et->dev);
	netif_stop_queue(et->dev);

	/* stop watchdog timer */
	del_timer(&et->timer);
	et->set = FALSE;

	/* LR:
	 * Mark netif flag down,
	 * to prevent tasklet from rescheduling itself,
	 * and to prevent the ISR to scheduling any work items
	 */
	ET_FLAG_DOWN(etc);

#if !defined(NAPI_POLL) && !defined(NAPI2_POLL)
	/* kill dpc before cleaning out the queued buffers */
	ET_UNLOCK(et);
	tasklet_kill(&et->tasklet);
	ET_LOCK(et);
#endif /* NAPI_POLL */

	/* kill tx tasklet */
	ET_UNLOCK(et);
	tasklet_disable(&et->tx_tasklet);
	tasklet_kill(&et->tx_tasklet);
	ET_LOCK(et);

#ifdef ET_ALL_PASSIVE
	/* Kill workqueue items too, or at least wait fo rthem to finisg */
#ifdef	__USE_GPL_SYMBOLS_
	/* LR:
	 * This is the right way to do this, but we can't unless we release the
	 * driver to GPL, which we're not allowed to.
	 */
	cancel_work_sync(&et->dpc_task.work);
	cancel_work_sync(&et->txq_task.work);
	cancel_work_sync(&et->wd_task.work);
#else
	flush_scheduled_work();
#endif
#endif /* ET_ALL_PASSIVE */

	/*
	 * LR: Now that all background activity ceased,
	 * it should be safe to proceed with shutdown
	 */

	/* flush the txq(s) */
	for (i = 0; i < NUMTXQ; i++)
		while ((skb = et_skb_dequeue(et, i)))
			PKTFREE(etc->osh, skb, TRUE);

	/* Shut down the hardware, reclaim Rx buffers */
	etc_down(etc, reset);

#ifdef ETFA
	if (et->etc->fa)
		fa_et_down(et->etc->fa);
#endif
}

/*
 * These are interrupt on/off entry points. Disable interrupts
 * during interrupt state transition.
 */
void
et_intrson(et_info_t *et)
{
	unsigned long flags;
	INT_LOCK(et, flags);
	(*et->etc->chops->intrson)(et->etc->ch);
	INT_UNLOCK(et, flags);
}

void
et_discard(et_info_t *et, void *pkt)
{
	void * rxh = PKTDATA(et->osh, pkt);
	int dataoff = et->etc->hwrxoff;

#if defined(ETFA)
	dataoff += et->fa_bhdr_sz;
#endif
	PKTPULL(et->osh, pkt, dataoff);

	/* skb->data points to ethernet header for error reporting. */
	et_error(et, (struct sk_buff *)pkt, rxh);

	PKTFREE(et->osh, pkt, FALSE);
}

static void
_et_watchdog(struct net_device *dev)
{
	et_info_t *et;

	et = ET_INFO(dev);

	ET_LOCK(et);

	etc_watchdog(et->etc);

#if defined(BCM_GMAC3)
        if (DEV_FWDER(et->etc)) {
                if (et_rxlazy_dyn_thresh > 0) {
                        if ((et->etc->rxframe - et->etc->rxrecord) > et_rxlazy_dyn_thresh) {
                                etc_rxlazy(et->etc, ET_RXLAZY_TIMEOUT, ET_RXLAZY_FRAMECNT);
                        } else {
                                etc_rxlazy(et->etc, et_rxlazy_timeout, et_rxlazy_framecnt);
                        }
                }
                et->etc->rxrecord = et->etc->rxframe;
        }
#endif

	if (et->set) {
		/* reschedule one second watchdog timer */
		et->timer.expires = jiffies + HZ;
		add_timer(&et->timer);
	}

#ifdef CTFPOOL
	/* allocate and add a new skb to the pkt pool */
	if (CTF_ENAB(et->cih))
		osl_ctfpool_replenish(et->osh, CTFPOOL_REFILL_THRESH);
#endif /* CTFPOOL */
	ET_UNLOCK(et);
}

#ifdef ET_ALL_PASSIVE
static void
et_watchdog_task(et_task_t *task)
{
	et_info_t *et = (et_info_t *)task->context;

	_et_watchdog((struct net_device *)et->dev);
}
#endif /* ET_ALL_PASSIVE */

static void
et_watchdog(ulong data)
{
	struct net_device *dev = (struct net_device *)data;
#ifdef ET_ALL_PASSIVE
	et_info_t *et = ET_INFO(dev);
#endif /* ET_ALL_PASSIVE */

	if (!ET_ALL_PASSIVE_ENAB(et))
		_et_watchdog(dev);
#ifdef ET_ALL_PASSIVE
	else
		schedule_work(&et->wd_task.work);
#endif /* ET_ALL_PASSIVE */
}


static int
et_get_settings(struct net_device *dev, struct ethtool_cmd *ecmd)
{
	et_info_t *et = ET_INFO(dev);

	ecmd->supported = (SUPPORTED_10baseT_Half | SUPPORTED_10baseT_Full |
	                   SUPPORTED_100baseT_Half | SUPPORTED_100baseT_Full |
	                   SUPPORTED_Autoneg);
	ecmd->advertising = ADVERTISED_TP;
	ecmd->advertising |= (et->etc->advertise & ADV_10HALF) ?
	        ADVERTISED_10baseT_Half : 0;
	ecmd->advertising |= (et->etc->advertise & ADV_10FULL) ?
	        ADVERTISED_10baseT_Full : 0;
	ecmd->advertising |= (et->etc->advertise & ADV_100HALF) ?
	        ADVERTISED_100baseT_Half : 0;
	ecmd->advertising |= (et->etc->advertise & ADV_100FULL) ?
	        ADVERTISED_100baseT_Full : 0;
	ecmd->advertising |= (et->etc->advertise2 & ADV_1000FULL) ?
	        ADVERTISED_1000baseT_Full : 0;
	ecmd->advertising |= (et->etc->advertise2 & ADV_1000HALF) ?
	        ADVERTISED_1000baseT_Half : 0;
	ecmd->advertising |= (et->etc->forcespeed == ET_AUTO) ?
	        ADVERTISED_Autoneg : 0;
	if (et->etc->linkstate) {
		ecmd->speed = (et->etc->speed == 1000) ? SPEED_1000 :
		               ((et->etc->speed == 100) ? SPEED_100 : SPEED_10);
		ecmd->duplex = (et->etc->duplex == 1) ? DUPLEX_FULL : DUPLEX_HALF;
	} else {
		ecmd->speed = 0;
		ecmd->duplex = 0;
	}
	ecmd->port = PORT_TP;
	ecmd->phy_address = 0;
	ecmd->transceiver = XCVR_INTERNAL;
	ecmd->autoneg = (et->etc->forcespeed == ET_AUTO) ? AUTONEG_ENABLE : AUTONEG_DISABLE;
	ecmd->maxtxpkt = 0;
	ecmd->maxrxpkt = 0;

	return 0;
}

static int
et_set_settings(struct net_device *dev, struct ethtool_cmd *ecmd)
{
	int speed;
	et_info_t *et = ET_INFO(dev);

	if (!capable(CAP_NET_ADMIN))
		return (-EPERM);

	else if (ecmd->speed == SPEED_10 && ecmd->duplex == DUPLEX_HALF)
		speed = ET_10HALF;
	else if (ecmd->speed == SPEED_10 && ecmd->duplex == DUPLEX_FULL)
		speed = ET_10FULL;
	else if (ecmd->speed == SPEED_100 && ecmd->duplex == DUPLEX_HALF)
		speed = ET_100HALF;
	else if (ecmd->speed == SPEED_100 && ecmd->duplex == DUPLEX_FULL)
		speed = ET_100FULL;
	else if (ecmd->speed == SPEED_1000 && ecmd->duplex == DUPLEX_FULL)
		speed = ET_1000FULL;
	else if (ecmd->autoneg == AUTONEG_ENABLE)
		speed = ET_AUTO;
	else
		return (-EINVAL);

	return etc_ioctl(et->etc, ETCSPEED, &speed);
}

static void
et_get_driver_info(struct net_device *dev, struct ethtool_drvinfo *info)
{
	et_info_t *et = ET_INFO(dev);

	bzero(info, sizeof(struct ethtool_drvinfo));
	info->cmd = ETHTOOL_GDRVINFO;
	sprintf(info->driver, "et%d", et->etc->unit);
	strncpy(info->version, EPI_VERSION_STR, sizeof(info->version));
	info->version[(sizeof(info->version))-1] = '\0';
}

#ifdef SIOCETHTOOL
static int
et_ethtool(et_info_t *et, struct ethtool_cmd *ecmd)
{
	int ret = 0;

	ET_LOCK(et);

	switch (ecmd->cmd) {
	case ETHTOOL_GSET:
		ret = et_get_settings(et->dev, ecmd);
		break;
	case ETHTOOL_SSET:
		ret = et_set_settings(et->dev, ecmd);
		break;
	case ETHTOOL_GDRVINFO:
		et_get_driver_info(et->dev, (struct ethtool_drvinfo *)ecmd);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	ET_UNLOCK(et);

	return (ret);
}
#endif /* SIOCETHTOOL */

static int
et_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	et_info_t *et;
	int error;
	char *buf;
	int size, ethtoolcmd;
	bool get = 0, set;
	et_var_t *var = NULL;
	void *buffer = NULL;

	et = ET_INFO(dev);

	ET_TRACE(("et%d: et_ioctl: cmd 0x%x\n", et->etc->unit, cmd));

	switch (cmd) {
#ifdef SIOCETHTOOL
	case SIOCETHTOOL:
		if (copy_from_user(&ethtoolcmd, ifr->ifr_data, sizeof(uint32)))
			return (-EFAULT);

		if (ethtoolcmd == ETHTOOL_GDRVINFO)
			size = sizeof(struct ethtool_drvinfo);
		else
			size = sizeof(struct ethtool_cmd);
		get = TRUE; set = TRUE;
		break;
#endif /* SIOCETHTOOL */
	case SIOCGETCDUMP:
		size = IOCBUFSZ;
		get = TRUE; set = FALSE;
		break;
	case SIOCGETCPHYRD:
	case SIOCGETCPHYRD2:
	case SIOCGETCROBORD:
		size = sizeof(int) * 4;
		get = TRUE; set = TRUE;
		break;
	case SIOCSETCPHYWR:
	case SIOCSETCPHYWR2:
	case SIOCSETCROBOWR:
		size = sizeof(int) * 4;
		get = FALSE; set = TRUE;
		break;
	case SIOCSETGETVAR:
		size = sizeof(et_var_t);
		set = TRUE;
		break;
	default:
		size = sizeof(int);
		get = FALSE; set = TRUE;
		break;
	}

	if ((buf = MALLOC(et->osh, size)) == NULL) {
		ET_ERROR(("et: et_ioctl: out of memory, malloced %d bytes\n", MALLOCED(et->osh)));
		return (-ENOMEM);
	}

	if (set && copy_from_user(buf, ifr->ifr_data, size)) {
		MFREE(et->osh, buf, size);
		return (-EFAULT);
	}

	if (cmd == SIOCSETGETVAR) {
		var = (et_var_t *)buf;
		if (var->buf) {
			if (!var->set)
				get = TRUE;

			if ((var->len == 0) || (var->len > ET_IOCTL_MAXLEN)) {
				ET_ERROR(("et: et_ioctl: Invalid var len %d\n", var->len));
				MFREE(et->osh, buf, size);
				return (-EFAULT);
			}

			if (!(buffer = (void *) MALLOC(et->osh, var->len))) {
				ET_ERROR(("et: et_ioctl: out of memory, malloced %d bytes\n",
					MALLOCED(et->osh)));
				MFREE(et->osh, buf, size);
				return (-ENOMEM);
			}

			if (copy_from_user(buffer, var->buf, var->len)) {
				MFREE(et->osh, buffer, var->len);
				MFREE(et->osh, buf, size);
				return (-EFAULT);
			}
		} else if (!var->set) {
			/* Get commands must have a return buffer */
			MFREE(et->osh, buf, size);
			return (-EFAULT);
		}
	}

	switch (cmd) {
#ifdef SIOCETHTOOL
	case SIOCETHTOOL:
		error = et_ethtool(et, (struct ethtool_cmd *)buf);
		break;
#endif /* SIOCETHTOOL */
	case SIOCSETGETVAR:
		ET_LOCK(et);
		error = etc_iovar(et->etc, var->cmd, var->set, buffer, var->len);
		ET_UNLOCK(et);
		if (!error && get && var->buf && buffer)
			error = copy_to_user(var->buf, buffer, var->len);

		if (buffer)
			MFREE(et->osh, buffer, var->len);
		break;
	default:
		ET_LOCK(et);
		error = etc_ioctl(et->etc, cmd - SIOCSETCUP, buf) ? -EINVAL : 0;
		ET_UNLOCK(et);
		break;
	}

	if (!error && get)
		error = copy_to_user(ifr->ifr_data, buf, size);

	MFREE(et->osh, buf, size);

	return (error);
}

static struct net_device_stats *
et_get_stats(struct net_device *dev)
{
	et_info_t *et;
	etc_info_t *etc;
	struct net_device_stats *stats;
	int locked = 0;

	et = ET_INFO(dev);

	ET_TRACE(("et%d: et_get_stats\n", et->etc->unit));

	if (!in_atomic()) {
		locked = 1;
		ET_LOCK(et);
	}

	etc = et->etc;
	stats = &et->stats;
	bzero(stats, sizeof(struct net_device_stats));

	/* refresh stats */
	if (et->etc->up)
		(*etc->chops->statsupd)(etc->ch);

	/* SWAG */
	stats->rx_packets = etc->rxframe;
	stats->tx_packets = etc->txframe;
	stats->rx_bytes = etc->rxbyte;
	stats->tx_bytes = etc->txbyte;
	stats->rx_errors = etc->rxerror;
	stats->tx_errors = etc->txerror;

	if (ET_GMAC(etc)) {
		gmacmib_t *mib;

		mib = etc->mib;
		stats->collisions = mib->tx_total_cols;
		stats->rx_length_errors = (mib->rx_oversize_pkts + mib->rx_undersize);
		stats->rx_crc_errors = mib->rx_crc_errs;
		stats->rx_frame_errors = mib->rx_align_errs;
		stats->rx_missed_errors = mib->rx_missed_pkts;
	} else {
		bcmenetmib_t *mib;

		mib = etc->mib;
		stats->collisions = mib->tx_total_cols;
		stats->rx_length_errors = (mib->rx_oversize_pkts + mib->rx_undersize);
		stats->rx_crc_errors = mib->rx_crc_errs;
		stats->rx_frame_errors = mib->rx_align_errs;
		stats->rx_missed_errors = mib->rx_missed_pkts;

	}

	stats->rx_fifo_errors = etc->rxoflo;
	stats->rx_over_errors = etc->rxgiants;
	stats->tx_fifo_errors = etc->txuflo;

	if (locked)
		ET_UNLOCK(et);

	return (stats);
}

static int
et_set_mac_address(struct net_device *dev, void *addr)
{
	et_info_t *et;
	struct sockaddr *sa = (struct sockaddr *) addr;

	et = ET_INFO(dev);
	ET_TRACE(("et%d: et_set_mac_address\n", et->etc->unit));

	if (et->etc->up)
		return -EBUSY;

	bcopy(sa->sa_data, dev->dev_addr, ETHER_ADDR_LEN);
	bcopy(dev->dev_addr, &et->etc->cur_etheraddr, ETHER_ADDR_LEN);

	return 0;
}

static void
et_set_multicast_list(struct net_device *dev)
{
	et_info_t *et;
	etc_info_t *etc;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
	struct dev_mc_list *mclist;
#else
	struct netdev_hw_addr *ha;
#endif
	int i;
	int locked = 0;

	et = ET_INFO(dev);
	etc = et->etc;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
	mclist = NULL ;		/* fend off warnings */
#else
	ha = NULL;
#endif

	ET_TRACE(("et%d: et_set_multicast_list\n", etc->unit));

	if (!in_atomic()) {
		locked = 1;
		ET_LOCK(et);
	}

	if (etc->up) {
		etc->promisc = (dev->flags & IFF_PROMISC)? TRUE: FALSE;
		etc->allmulti = (dev->flags & IFF_ALLMULTI)? TRUE: etc->promisc;

		/* copy the list of multicasts into our private table */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
		for (i = 0, mclist = dev->mc_list; mclist && (i < dev->mc_count);
			i++, mclist = mclist->next) {
			if (i >= MAXMULTILIST) {
				etc->allmulti = TRUE;
				i = 0;
				break;
			}
			etc->multicast[i] = *((struct ether_addr *)mclist->dmi_addr);
		}
#else	/* >= 2.6.36 */
		i = 0;
		netdev_for_each_mc_addr(ha, dev) {
			if (i >= MAXMULTILIST) {
				etc->allmulti = TRUE;
				i = 0;
				break;
			}
			etc->multicast[i] = *((struct ether_addr *)ha->addr);
			i++;
		} /* for each ha */
#endif /* LINUX_VERSION_CODE */
		etc->nmulticast = i;

		/* LR: partial re-init, DMA is already initialized */
		et_init(et, ET_INIT_INTRON);
	}

	if (locked)
		ET_UNLOCK(et);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
static irqreturn_t BCMFASTPATH
et_isr(int irq, void *dev_id)
#else
static irqreturn_t BCMFASTPATH
et_isr(int irq, void *dev_id, struct pt_regs *ptregs)
#endif
{
	et_info_t *et;
	struct chops *chops;
	void *ch;
	uint events = 0;

	et = (et_info_t *)dev_id;
	chops = et->etc->chops;
	ch = et->etc->ch;

	/* guard against shared interrupts */
	if (!et->etc->up)
		goto done;

	/* get interrupt condition bits */
	events = (*chops->getintrevents)(ch, TRUE);

	/* not for us */
	if (!(events & INTR_NEW))
		goto done;

	ET_TRACE(("et%d: et_isr: events 0x%x\n", et->etc->unit, events));
	ET_LOG("et%d: et_isr: events 0x%x", et->etc->unit, events);

	/* disable interrupts */
	(*chops->intrsoff)(ch);

	/* save intstatus bits */
	ASSERT(et->events == 0);
	et->events = events;

	ASSERT(et->resched == FALSE);

#ifdef NAPI2_POLL

	napi_schedule(&et->napi_poll);

#elif defined(NAPI_POLL)
	/* allow the device to be added to the cpu polling list if we are up */
	if (netif_rx_schedule_prep(et->dev)) {
		/* tell the network core that we have packets to send up */
		__netif_rx_schedule(et->dev);
	} else {
		ET_ERROR(("et%d: et_isr: intr while in poll!\n",
		          et->etc->unit));
		(*chops->intrson)(ch);
	}
#else /* ! NAPI_POLL && ! NAPI2_POLL */
	/* schedule dpc */
#ifdef ET_ALL_PASSIVE
	if (ET_ALL_PASSIVE_ENAB(et)) {
		schedule_work(&et->dpc_task.work);
	} else
#endif /* ET_ALL_PASSIVE */
	tasklet_schedule(&et->tasklet);
#endif /* NAPI_POLL */

done:
	ET_LOG("et%d: et_isr ret", et->etc->unit, 0);

	return IRQ_RETVAL(events & INTR_NEW);
}

#ifdef PKTC
static void
et_sendup_chain_error_handler(et_info_t *et, struct sk_buff *skb, uint sz, int32 err)
{
	struct sk_buff *nskb;
	uint16 vlan_tag;
	struct ethervlan_header *n_evh;

	ASSERT(err != BCME_OK);

	et->etc->chained -= sz;
	et->etc->unchained += sz;

	/* get original vlan tag from the first packet */
	vlan_tag = ((struct ethervlan_header *)PKTDATA(et->osh, skb))->vlan_tag;

	FOREACH_CHAINED_PKT(skb, nskb) {
		PKTCLRCHAINED(et->osh, skb);
		PKTCCLRFLAGS(skb);
		if (nskb != NULL) {
			/* update vlan header changed by CTF_HOTBRC_L2HDR_PREP */
			n_evh = (struct ethervlan_header *)PKTDATA(et->osh, nskb);

			if (n_evh->vlan_type == HTON16(ETHER_TYPE_8021Q)) {
				/* change back original vlan tag */
				n_evh = (struct ethervlan_header *)PKTDATA(et->osh, nskb);
				n_evh->vlan_tag = vlan_tag;
			}
			else {
				/* add back original vlan header */
				n_evh = (struct ethervlan_header *)PKTPUSH(et->osh, nskb,
					VLAN_TAG_LEN);
				*n_evh = *(struct ethervlan_header *)PKTDATA(et->osh, skb);
			}
			nskb->dev = skb->dev;
		}
		PKTSETPRIO(skb, 0);
		/* Update the PKTCCNT and PKTCLEN for unchained sendup cases */
		PKTCSETCNT(skb, 1);
		PKTCSETLEN(skb, PKTLEN(et->etc->osh, skb));

		if (et_ctf_forward(et, skb) == BCME_OK) {
			ET_ERROR(("et%d: shall not happen\n", et->etc->unit));
		}

		if (et->etc->qos)
			pktsetprio(skb, TRUE);

		skb->protocol = eth_type_trans(skb, skb->dev);

			/* send it up */
#if defined(NAPI_POLL) || defined(NAPI2_POLL)
		netif_receive_skb(skb);
#else /* NAPI_POLL */
		netif_rx(skb);
#endif /* NAPI_POLL */
	}
}

static void BCMFASTPATH
et_sendup_chain(et_info_t *et, void *h)
{
	struct sk_buff *skb;
	uint sz = PKTCCNT(h);
	osl_t * osh = et->etc->osh;
	int32 err;

	ASSERT(h != NULL);

	ASSERT((sz > 0) && (sz <= PKTCBND));
	ET_TRACE(("et%d: %s: sending up packet chain of sz %d\n",
	          et->etc->unit, __FUNCTION__, sz));
	et->etc->chained += sz;
#ifdef BCMDBG
	et->etc->chainsz[sz - 1] += sz;
#endif
	et->etc->currchainsz = sz;
	et->etc->maxchainsz = MAX(et->etc->maxchainsz, sz);

	skb = PKTTONATIVE(osh, h);
	skb->dev = et->dev;

#if defined(BCM_GMAC3)
	/* Forward chain directly to wl transmit */
	if (DEV_FWDER(et->etc)) { /* BCM_GMAC3 */
		if (fwder_transmit(et->fwdh, skb, sz, skb->dev) != FWDER_SUCCESS) {
			PKTFRMNATIVE(osh, skb);
			PKTCFREE(osh, skb, FALSE);
		}
	} else
#endif /* BCM_GMAC3 */
	{
		/* send up the packet chain */
		if ((err = ctf_forward(et->cih, h, skb->dev)) == BCME_OK)
			return;
		et_sendup_chain_error_handler(et, skb, sz, err);
	}
}
#endif /* PKTC */

#ifdef ET_INGRESS_QOS
static inline bool
et_discard_rx(et_info_t *et, struct chops *chops,
	void *ch, uint8 *evh, uint8 prio, uint16 toss, int quota)
{
	uint16 left;

	/* Regardless of DMA RX discard policy ICMP and IGMP packets are passed */
	if (IP_PROT46(evh + ETHERVLAN_HDR_LEN) != IP_PROT_IGMP &&
		IP_PROT46(evh + ETHERVLAN_HDR_LEN) != IP_PROT_ICMP) {
		ASSERT(chops->activerxbuf);
		left = (*chops->activerxbuf)(ch);
		if (left < et->etc->dma_rx_thresh && toss < (quota << TOSS_CAP)) {
			if ((et->etc->dma_rx_policy == DMA_RX_POLICY_TOS &&
				prio != IPV4_TOS_CRITICAL) ||
				(et->etc->dma_rx_policy == DMA_RX_POLICY_UDP &&
				IP_PROT46(evh + ETHERVLAN_HDR_LEN) != IP_PROT_UDP)) {
				/* discard the packet */
				return TRUE;
			}
		}
	}
	return FALSE;
}
#endif /* ET_INGRESS_QOS */

#if defined(PKTC)
/*
 * +----------------------------------------------------------------------------
 *
 * Packet Chaining PASS #2. PKTC handles all bridged packets by explicitly
 * chaining them before fwding via the CTF layer. A chain database is first
 * initialized. The chain database is populated with first seen Mac addresses.
 * The chain database is also used to compose a chain for all packets that match
 * the CD entry as well as meet several criteria, e.g. match Eth Da, Sa, prio,
 * EtherType, etc with an hashed looked up entry in the hot brc cache.
 * Chained packets are forwarded in one shot via CTF.
 *
 * Packets that miss the chain database, are returned for single packet
 * processing via an attempt through CTF.
 *
 * +----------------------------------------------------------------------------
 */
static inline int
et_chain_rx(et_info_t *et, int rxcnt, void **rxpkts, const int dataoff,
            struct chops *chops, void *ch)
{
	void *pkt;
	osl_t *osh = et->osh;
	int nrx, nochain = 0;
	bool stop_chain = FALSE;
#ifdef ET_INGRESS_QOS
	uint16 left = NRXBUFPOST, tossed = 0, quota = rxcnt;
#endif /* ET_INGRESS_QOS */
	pktc_data_t cd[PKTCMC] = {{0}};
	uint8 *evh, prio;
	int32 i = 0, cidx = 0;
#if defined(USBAP)
	bool check_tcp_ctrl = SKIP_TCP_CTRL_CHECK(et);
#endif /* USBAP */
#ifdef ETAGG
	const int dataoff_ext = (RX_BCM_HDR_ENAB(et) && (et->bhdr_roff > 0)) ? et->bhdr_sz : 0;
#else /* ! ETAGG */
	const int dataoff_ext = 0;
#endif /* ! ETAGG */

	for (nrx = 0; nrx < rxcnt; nrx++) {

		pkt = rxpkts[nrx]; /* fetch the next pkt */

		ASSERT(pkt != NULL);
		ASSERT(PKTCLINK(pkt) == NULL);
#ifdef ETAGG
		if (AGG_BCM_HDR_ENAB(et->etc->agg))
			agg_process_rx(et->etc->agg, pkt, dataoff, et->bhdr_roff);
#endif /* ETAGG */

#ifdef ETAGG
		evh = PKTDATA(osh, pkt) + dataoff + dataoff_ext; /* fetch eth_vlan hdr start */
#else
		evh = PKTDATA(osh, pkt) + dataoff; /* fetch eth_vlan hdr start */
#endif

#if defined(BCM_GMAC3)
		if (DEV_FWDER(et->etc)) {
			prio = IP_TOS46(evh + ETHER_HDR_LEN) >> IPV4_TOS_PREC_SHIFT;
		} else
#endif /* BCM_GMAC3 */
		{
			prio = IP_TOS46(evh + ETHERVLAN_HDR_LEN) >> IPV4_TOS_PREC_SHIFT;

#ifdef ET_INGRESS_QOS
			if (et->etc->dma_rx_policy) {
				if (et_discard_rx(et, chops, ch, evh, prio, tossed, quota)) {
					PKTFREE(et->osh, pkt, FALSE); /* toss the packet */
					tossed++;
					continue;
				}
			}
#endif /* ET_INGRESS_QOS */
		}

		/* In case of WAN originating pkt, prio is incorrect */

#if defined(USBAP)
		if (check_tcp_ctrl && PKT_IS_TCP_CTRL(osh, pkt)) {
			rxpkts[nochain] = pkt;
			nochain++;
		} else
#endif /* USBAP */
		{
			if (cd[0].h_da == NULL) {
				cd[0].h_da = evh; cd[0].h_sa = evh + ETHER_ADDR_LEN;
				cd[0].h_prio = prio;
			}

			for (i = 0; i <= cidx; i++) {
				if (is_pkt_chainable(et, pkt, evh, prio, &cd[i],
				                     DEV_NTKIF(et->etc))) {
					break;
				}
				else if ((i + 1 < PKTCMC) && (cd[i + 1].h_da == NULL)) {
					cidx++;
					cd[cidx].h_da = evh;
					cd[cidx].h_sa = evh + ETHER_ADDR_LEN;
					cd[cidx].h_prio = prio;
				}
			}

			/* entry matched or new entry created in cd[] */
			if ((i < PKTCMC) && !stop_chain) {

				int pktlen = PKTLEN(osh, pkt);

				/* enqueue chainable packet into cd's list */
				PKTCENQTAIL(cd[i].chead, cd[i].ctail, pkt);

				et->etc->rxbyte += pktlen;
				PKTSETLEN(osh, pkt, pktlen - ETHER_CRC_LEN); /* strip crc32 */

#if defined(ETFA)
				{
					int nid;
					if (et->fa_bhdr_sz) {
						nid = fa_get_nid_rx(osh, pkt, HWRXOFF);
					} else {
						nid = BCM_FA_INVALID_IDX_VAL;
					}
					PKTSETFAHIDX(pkt, BCM_FA_INVALID_IDX_VAL);
				}
#endif /* ETFA */

				/* Pull in one shot, both HW Rx Hdr (and BRCMHdr, if present) */
#ifdef ETAGG
				PKTPULL(osh, pkt, dataoff + dataoff_ext);
#else
				PKTPULL(osh, pkt, dataoff);
#endif

				/* update header for non-first frames */
				if (DEV_NTKIF(et->etc) && cd[i].chead != pkt) {
					/* Possibly another PKTPULL to pop VLAN hdr */
					CTF_HOTBRC_L2HDR_PREP(osh, et->brc_hot, prio,
					                      PKTDATA(osh, pkt), pkt);
				}

				PKTCINCRCNT(cd[i].chead);
				PKTSETCHAINED(osh, pkt);
				PKTCADDLEN(cd[i].chead, PKTLEN(osh, pkt));

				if (PKTCCNT(cd[i].chead) >= PKTCBND)
					stop_chain = TRUE;

			} else { /* not chainable, so return pkt via rxpkts array */

				rxpkts[nochain] = pkt; /* for, single sendup processing */
				nochain++;
			}
		}
	}   /* for nrx < rxcnt */

	et->etc->rxframe += rxcnt - nochain;
#ifdef ET_INGRESS_QOS
	et->etc->rxframe -= tossed;
#endif

	/* send up the chain(s) at one fell swoop */
	ASSERT(cidx < PKTCMC);

	for (i = 0; i <= cidx; i++) {
		if (cd[i].chead != NULL) {
			PKTSETPRIO(cd[i].chead, cd[i].h_prio);
			et_sendup_chain(et, cd[i].chead);
		}
	}

	return nochain; /* number of single pkt processing pass */
}
#endif /* PKTC */

/*
 * +----------------------------------------------------------------------------
 * CTF forwarded PASS #3. All packets that miss WOFA and PKTC, are now processed
 * one at a time.
 * +----------------------------------------------------------------------------
 */
static inline void
et_single_rx(et_info_t *et, int rxcnt, void **rxpkts, int dataoff)
{
	int nrx;
	struct sk_buff *skb;
#ifdef ETAGG
	const int dataoff_ext = (RX_BCM_HDR_ENAB(et) && (et->bhdr_roff > 0)) ? et->bhdr_sz : 0;
#else /* ! ETAGG */
	const int dataoff_ext = 0;
#endif /* ! ETAGG */

	et->etc->unchained += rxcnt;

	for (nrx = 0; nrx < rxcnt; nrx++) {
		ASSERT(rxpkts[nrx] != NULL);
#ifdef ETAGG
		do {
#if defined(PKTC)
			/* bhdr is handled in et_chain_rx */
			if (PKTC_ENAB(et))
				break;
#endif /* PKTC */
			if (AGG_BCM_HDR_ENAB(et->etc->agg)) {
				agg_process_rx(et->etc->agg, rxpkts[nrx],
					dataoff, et->bhdr_roff);
			}
		} while (0);
#endif /* ETAGG */

		skb = PKTTONATIVE(et->osh, rxpkts[nrx]);

#if defined(BCM_GMAC3)
		if (DEV_FWDER(et->etc)) {
			struct ether_header *eh;
#ifdef ETAGG
			eh = (struct ether_header *)(skb->data + dataoff + dataoff_ext);
#else
			eh = (struct ether_header *)(skb->data + dataoff);
#endif
			skb->priority = IP_TOS46(eh + ETHER_HDR_LEN) >> IPV4_TOS_PREC_SHIFT;
		} else
#endif /* BCM_GMAC3 */
		{
			PKTSETPRIO(skb, 0);
		}

#ifdef ETAGG
		et_sendup(et, skb, dataoff + dataoff_ext);
#else
		et_sendup(et, skb, dataoff);
#endif
	}
}

static /* inline */ int BCMFASTPATH
et_rxevent(void * ch, int quota, struct chops *chops, et_info_t *et)
{
	int rxcnt;
	void *rxpkts[ETCQUOTA_MAX];
#if defined(ETFA)
	const int dataoff = et->etc->hwrxoff + et->fa_bhdr_sz;
#else  /* ! ETFA */
	const int dataoff = et->etc->hwrxoff;
#endif /* ! ETFA */

	/* fetch max quota number of pkts from rx ring, and save in rxpkts */
	/* chiprxquota does not check RXH_FLAGS,etc. and only prefetches 8x64bits */
	rxcnt = (*chops->rxquota)(ch, quota, rxpkts); /* refills at the end */

	/* reschedule et_dpc()/et_poll(): if pkts may be pending in rx ring */
	et->resched = (rxcnt >= quota); /* Need fixup for ET_INGRESS_QOS */
	quota = rxcnt;  /* quota: total number of pkts that will be processed */

#if defined(ETFA)
	if (et->fa_aux_dev) /* No bulk bypass processing for FA aux port */
		goto fa_aux_skip_bulk_bypass;
#endif /* ETFA */

#if defined(BCM_GMAC3)
	/* If Dongle mode forwarder, transmit array of packets to dongle host driver
	 * which will consume all packets (or free them on error).
	 */
	if (DEV_FWDER(et->etc) && (fwder_is_mode(et->fwdh, FWDER_DNG_MODE))) {

		/* Bulk decrement pktalloced count. If fwder fails to transfer the pkts,
		 * then increment pktalloced count before freeing to ctfpool.
		 */

		et->etc->rxframe += rxcnt;

		PKTTOFWDER(et->osh, rxpkts, rxcnt);

		if (fwder_transmit(et->fwdh, (void *)rxpkts, rxcnt, et->dev) != FWDER_SUCCESS) {
			int cnt;
			PKTFRMFWDER(et->osh, rxpkts, rxcnt);
			for (cnt = 0; cnt < rxcnt; cnt++) {
				PKTFREE(et->osh, rxpkts[cnt], FALSE);
			}
		}
		goto fwder_dng_bypass;
	}
#endif /* BCM_GMAC3 */

	/* DEV FWDER::FWDER_NIC_MODE or DEV_NTK.
	 * DEV_FWDER: NIC Mode fwder will bridge pkts to WLAN using pkt chaining.
	 * DEV_NTK: Network device will bridge/route packets with chaining/CTF.
	 */
#if defined(PKTC)
	if (PKTC_ENAB(et)) {
		rxcnt = et_chain_rx(et, rxcnt, rxpkts, dataoff, chops, ch);
	}
#endif /* PKTC */

#if defined(ETFA)
fa_aux_skip_bulk_bypass:
#endif /* ET_FA */

	/* Process leftover rxcnt number of unchained pkts, sendup one at a time. */
	(void)et_single_rx(et, rxcnt, rxpkts, dataoff);

#if defined(BCM_GMAC3)
fwder_dng_bypass:
#endif

#if defined(BCMDBG)
	et->etc->rxprocessed += quota;
#endif /* BCMDBG */

	return quota; /* total number of processed pkts */
}

#if defined(NAPI2_POLL)
static int BCMFASTPATH
et_poll(struct napi_struct *napi, int budget)
{
	int quota = budget;
	struct net_device *dev = napi->dev;
	et_info_t *et = ET_INFO(dev);

#elif defined(NAPI_POLL)
static int BCMFASTPATH
et_poll(struct net_device *dev, int *budget)
{
	int quota = min(RXBND, *budget);
	et_info_t *et = ET_INFO(dev);
#else /* NAPI_POLL */
static void BCMFASTPATH
et_dpc(ulong data)
{
	et_info_t *et = (et_info_t *)data;
	int quota = et->etc->quota;
#endif /* NAPI_POLL */
	struct chops *chops;
	void *ch;
	uint nrx = 0;

	chops = et->etc->chops;
	ch = et->etc->ch;

	ET_TRACE(("et%d: et_dpc: events 0x%x\n", et->etc->unit, et->events));
	ET_LOG("et%d: et_dpc: events 0x%x", et->etc->unit, et->events);

#if !defined(NAPI_POLL) && !defined(NAPI2_POLL)
	ET_LOCK(et);
#endif /* ! NAPIx_POLL */

	if (!et->etc->up)
		goto done;

	/* get interrupt condition bits again when dpc was rescheduled */
	if (et->resched) {
		et->events = (*chops->getintrevents)(ch, FALSE);
		et->resched = FALSE;
	}

	if (et->events & INTR_RX) {
		nrx = et_rxevent(ch, quota, chops, et);
	}

	if (et->events & INTR_TX)
		(*chops->txreclaim)(ch, FALSE);

	(*chops->rxfill)(ch);

	/* handle error conditions, if reset required leave interrupts off! */
	if (et->events & INTR_ERROR) {
		if ((*chops->errors)(ch))
			et_init(et, ET_INIT_INTROFF);
		else
			if (nrx < quota) {
				nrx += et_rxevent(ch, quota, chops, et);
			}
	}

	/* run the tx queue */
	if (et->etc->txq_state != 0) {
		if (!ET_ALL_PASSIVE_ENAB(et) && txworkq == 0)
			et_sendnext(et);
#ifdef ET_ALL_PASSIVE
#ifdef CONFIG_SMP
		else if (txworkq && online_cpus > 1) {
			schedule_work_on(1 - raw_smp_processor_id(), &et->txq_task.work);
		}
#endif
		else {
			schedule_work(&et->txq_task.work);
		}
#endif /* ET_ALL_PASSIVE */
	}

	/* clear this before re-enabling interrupts */
	et->events = 0;

	/* something may bring the driver down */
	if (!et->etc->up) {
		et->resched = FALSE;
		goto done;
	}

#if !defined(NAPI_POLL) && !defined(NAPI2_POLL)
#ifdef ET_ALL_PASSIVE
	if (et->resched) {
		if (!ET_ALL_PASSIVE_ENAB(et)) {
			tasklet_schedule(&et->tasklet);
		} else {
			schedule_work(&et->dpc_task.work);
		}
	}
	else {
#if defined(BCM_GMAC3)
		if (DEV_FWDER(et->etc)) {
			(*chops->rxlazy)(ch);
		}
#endif /* BCM_GMAC3 */
		(*chops->intrson)(ch);
	}
#else /* ET_ALL_PASSIVE */
	/* there may be frames left, reschedule et_dpc() */
	if (et->resched) {
		tasklet_schedule(&et->tasklet);
	/* re-enable interrupts */
	} else {
#if defined(BCM_GMAC3)
		if (DEV_FWDER(et->etc)) {
			(*chops->rxlazy)(ch);
		}
#endif /* BCM_GMAC3 */
		(*chops->intrson)(ch);
	}
#endif /* ET_ALL_PASSIVE */
#endif /* ! NAPIx_POLL */

done:
	ET_LOG("et%d: et_dpc ret", et->etc->unit, 0);

#if defined(NAPI_POLL) || defined(NAPI2_POLL)
#ifdef	NAPI_POLL
	/* update number of frames processed */
	*budget -= nrx;
	dev->quota -= nrx;

	ET_TRACE(("et%d: et_poll: quota %d budget %d\n",
	          et->etc->unit, dev->quota, *budget));
#else
	ET_TRACE(("et%d: et_poll: budget %d\n",
	          et->etc->unit, budget));
#endif

	/* we got packets but no quota */
	if (et->resched)
		/* indicate that we are not done, don't enable
		 * interrupts yet. linux network core will call
		 * us again.
		 */
		return (1);

#ifdef	NAPI2_POLL
	napi_complete(napi);
#else	/* NAPI_POLL */
	netif_rx_complete(dev);
#endif

	/* enable interrupts now */
	(*chops->intrson)(ch);

	/* indicate that we are done */
	return (0);
#else /* NAPI_POLL */
	ET_UNLOCK(et);
	return;
#endif /* NAPI_POLL */
}

#ifdef ET_ALL_PASSIVE
static void BCMFASTPATH
et_dpc_work(struct et_task *task)
{
#if !defined(NAPI_POLL) && !defined(NAPI2_POLL)
	et_info_t *et = (et_info_t *)task->context;
	et_dpc((unsigned long)et);
#else
	BUG_ON(1);
#endif
	return;
}
#endif /* ET_ALL_PASSIVE */

static void
et_error(et_info_t *et, struct sk_buff *skb, void *rxh)
{
	uchar eabuf[32];
	struct ether_header *eh;

	eh = (struct ether_header *)skb->data;
	bcm_ether_ntoa((struct ether_addr *)eh->ether_shost, eabuf);

	if (RXH_FLAGS(et->etc, rxh) & htol16(GRXF_OVF)) {
		ET_ERROR(("et%d: chiprx, dma overflow\n", et->etc->unit));
		et->etc->rxoflodiscards++;
	}

	if (RXH_OVERSIZE(et->etc, rxh)) {
		ET_ERROR(("et%d: rx: over size packet from %s\n", et->etc->unit, eabuf));
	}
	if (RXH_CRC(et->etc, rxh)) {
		ET_ERROR(("et%d: rx: crc error from %s\n", et->etc->unit, eabuf));
	}
	if (RXH_OVF(et->etc, rxh)) {
		ET_ERROR(("et%d: rx: fifo overflow\n", et->etc->unit));
	}
	if (RXH_NO(et->etc, rxh)) {
		ET_ERROR(("et%d: rx: crc error (odd nibbles) from %s\n",
		          et->etc->unit, eabuf));
	}
	if (RXH_RXER(et->etc, rxh)) {
		ET_ERROR(("et%d: rx: symbol error from %s\n", et->etc->unit, eabuf));
	}
}

#ifdef CONFIG_IP_NF_DNSMQ
typedef int (*dnsmqHitHook)(struct sk_buff *skb);
extern dnsmqHitHook dnsmq_hit_hook;
#endif

static inline int32
et_ctf_forward(et_info_t *et, struct sk_buff *skb)
{
#ifdef HNDCTF
	int ret;
#endif

#ifdef CONFIG_IP_NF_DNSMQ
       if(dnsmq_hit_hook&&dnsmq_hit_hook(skb))
               return (BCME_ERROR);
#endif

#ifdef HNDCTF
	/* try cut thru first */
	if (CTF_ENAB(et->cih) && (ret = ctf_forward(et->cih, skb, skb->dev)) != BCME_ERROR) {
		if (ret == BCME_EPERM) {
			PKTFRMNATIVE(et->osh, skb);
			PKTFREE(et->osh, skb, FALSE);
		}
		return (BCME_OK);
	}

	/* clear skipct flag before sending up */
	PKTCLRSKIPCT(et->osh, skb);
#endif /* HNDCTF */

#ifdef CTFPOOL
	/* allocate and add a new skb to the pkt pool */
	if (PKTISFAST(et->osh, skb))
		osl_ctfpool_add(et->osh);

	/* clear fast buf flag before sending up */
	PKTCLRFAST(et->osh, skb);

	/* re-init the hijacked field */
	CTFPOOLPTR(et->osh, skb) = NULL;
#endif /* CTFPOOL */

#ifdef	CTFMAP
	/* map the unmapped buffer memory before sending up */
	if (PKTLEN(et->osh, skb) > (CTFMAPSZ - 4))
		PKTCTFMAP(et->osh, skb);
	else {
		PKTCLRCTF(et->osh, skb);
		CTFMAPPTR(et->osh, skb) = NULL;
	}
#endif	/* CTFMAP */

	return (BCME_ERROR);
}

void BCMFASTPATH
et_sendup(et_info_t *et, struct sk_buff *skb, int dataoff)
{
	etc_info_t *etc;
	void *rxh;

	etc = et->etc;

	/* packet buffer starts with rxhdr */
	rxh = skb->data;

	/* strip off rxhdr */
	__skb_pull(skb, dataoff);

	ET_TRACE(("et%d: et_sendup: %d bytes\n", et->etc->unit, skb->len));
	ET_LOG("et%d: et_sendup: len %d", et->etc->unit, skb->len);

	etc->rxframe++;
	etc->rxbyte += skb->len;

	/* eh should now be aligned 2-mod-4 */
	ASSERT(ISALIGNED(skb->data, 2)); /* ((ulong)skb->data & 3) == 2 */

	/* strip off crc32 */
	skb_trim(skb, skb->len - ETHER_CRC_LEN);

	/* Update the PKTCCNT and PKTCLEN for unchained sendup cases */

	PKTCSETCNT(skb, 1);
	PKTCSETLEN(skb, PKTLEN(etc->osh, skb));
	ET_PRHDR("rx", (struct ether_header *)skb->data, skb->len, etc->unit);
	ET_PRPKT("rxpkt", skb->data, skb->len, etc->unit);

#if !defined(BCM_GMAC3)
	{
		/* get the error flags */
		uint16 flags;
		flags = RXH_FLAGS(etc, rxh);

		/* check for reported frame errors */
		if (flags)
			goto err;
	}
#endif /* ! BCM_GMAC3 */

	skb->dev = et->dev;

#ifdef ETFA
	if (FA_RX_BCM_HDR((fa_t *)et->etc->fa) || FA_IS_AUX_DEV((fa_t *)et->etc->fa))
		fa_process_rx(et->etc->fa, skb);
	else
		PKTSETFAHIDX(skb, BCM_FA_INVALID_IDX_VAL);
#endif /* ETFA */

#ifdef PLC
	if (et->plc.hw && (et_plc_recv(et, skb) == 0))
		return;
#endif /* PLC */

#if defined(BCM_GMAC3)
	/* Forward single pkt directly to wl transmit */
	if (DEV_FWDER(et->etc)) { /* BCM_GMAC3 */
		if (fwder_transmit(et->fwdh, skb, 1, skb->dev) != FWDER_SUCCESS) {
			PKTFRMNATIVE(etc->osh, skb);
			PKTFREE(etc->osh, skb, FALSE);
		}
		return;
	}
#endif /* BCM_GMAC3 */

#ifdef HNDCTF
	/* try cut thru' before sending up */
	if (et_ctf_forward(et, skb) != BCME_ERROR)
		return;

	PKTCLRTOBR(etc->osh, skb);
	if (PKTISFAFREED(skb)) {
		/* HW FA ate it */
		PKTCLRFAAUX(skb);
		PKTCLRFAFREED(skb);
		PKTFRMNATIVE(etc->osh, skb);
		PKTFREE(etc->osh, skb, FALSE);
		return;
	}
#endif /* HNDCTF */

	ASSERT(!PKTISCHAINED(skb));
	ASSERT(PKTCGETFLAGS(skb) == 0);

	/* extract priority from payload and store it out-of-band
	 * in skb->priority
	 */
	if (et->etc->qos)
		pktsetprio(skb, TRUE);

	skb->protocol = eth_type_trans(skb, skb->dev);

	/* send it up */
#if defined(NAPI_POLL) || defined(NAPI2_POLL)
	netif_receive_skb(skb);
#else /* NAPI_POLL */
	netif_rx(skb);
#endif /* NAPI_POLL */

	ET_LOG("et%d: et_sendup ret", et->etc->unit, 0);

	return;

#if !defined(BCM_GMAC3)
err:
	et_error(et, skb, rxh);
	PKTFRMNATIVE(etc->osh, skb);
	PKTFREE(etc->osh, skb, FALSE);

	return;
#endif /* ! BCM_GMAC3 */
}

#ifdef HNDCTF
void
et_dump_ctf(et_info_t *et, struct bcmstrbuf *b)
{
	ctf_dump(et->cih, b);
}
#endif

#ifdef BCMDBG_CTRACE
void et_dump_ctrace(et_info_t *et, struct bcmstrbuf *b)
{
	PKT_CTRACE_DUMP(et->etc->osh, b);
}
#endif

#if defined(BCM_GMAC3)
void et_dump_fwder(et_info_t *et, struct bcmstrbuf *b)
{
	fwder_dump(b);
}
#endif /* BCM_GMAC3 */

void
et_dump(et_info_t *et, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "et%d: %s %s version %s\n", et->etc->unit,
		__DATE__, __TIME__, EPI_VERSION_STR);

#ifdef HNDCTF
#if defined(BCMDBG)
	ctf_dump(et->cih, b);
#endif 
#endif /* HNDCTF */

#if defined(BCM_GMAC3)
	et_dump_fwder(et, b);
#endif /* BCM_GMAC3 */

#ifdef BCMDBG
	et_dumpet(et, b);
	etc_dump(et->etc, b);
#endif /* BCMDBG */

#ifdef BCMDBG_CTRACE
	PKT_CTRACE_DUMP(et->etc->osh, b);
#endif
}

#ifdef BCMDBG
static void
et_dumpet(et_info_t *et, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "et %p dev %p name %s tbusy %d txq[0].qlen %d malloced %d\n",
		et, et->dev, et->dev->name, (uint)netif_queue_stopped(et->dev), et->txq[0].qlen,
		MALLOCED(et->osh));
}
#endif	/* BCMDBG */

void
et_link_up(et_info_t *et)
{
	ET_ERROR(("et%d: link up (%d%s)\n",
		et->etc->unit, et->etc->speed, (et->etc->duplex? "FD" : "HD")));
}

void
et_link_down(et_info_t *et)
{
	ET_ERROR(("et%d: link down\n", et->etc->unit));
}

/*
 * 47XX-specific shared mdc/mdio contortion:
 * Find the et associated with the same chip as <et>
 * and coreunit matching <coreunit>.
 */
void *
et_phyfind(et_info_t *et, uint coreunit)
{
	et_info_t *tmp;
	uint bus, slot;

	bus = et->pdev->bus->number;
	slot = PCI_SLOT(et->pdev->devfn);

	/* walk the list et's */
	for (tmp = et_list; tmp; tmp = tmp->next) {
		if (et->etc == NULL)
			continue;
		if (tmp->pdev == NULL)
			continue;
		if (tmp->pdev->bus->number != bus)
			continue;
		if (tmp->etc->nicmode)
			if (PCI_SLOT(tmp->pdev->devfn) != slot)
				continue;
		if (tmp->etc->coreunit != coreunit)
			continue;
		break;
	}
	return (tmp);
}

/* shared phy read entry point */
uint16
et_phyrd(et_info_t *et, uint phyaddr, uint reg)
{
	uint16 val;

	ET_LOCK(et);
	val = et->etc->chops->phyrd(et->etc->ch, phyaddr, reg);
	ET_UNLOCK(et);

	return (val);
}

/* shared phy write entry point */
void
et_phywr(et_info_t *et, uint phyaddr, uint reg, uint16 val)
{
	ET_LOCK(et);
	et->etc->chops->phywr(et->etc->ch, phyaddr, reg, val);
	ET_UNLOCK(et);
}

#ifdef ETFA
void
et_fa_lock_init(et_info_t *et)
{
	spin_lock_init(&et->fa_lock);
}

void
et_fa_lock(et_info_t *et)
{
	spin_lock_bh(&et->fa_lock);
}

void
et_fa_unlock(et_info_t *et)
{
	spin_unlock_bh(&et->fa_lock);
}

void *
et_fa_get_fa_dev(et_info_t *et)
{
	return et->dev;
}

bool
et_fa_dev_on(void *dev)
{
	if (dev == NULL)
		return FALSE;

	return ((struct net_device *)dev)->fa_on;
}

void
et_fa_set_dev_on(et_info_t *et)
{
	et->dev->fa_on = TRUE;
}

/* Do nothing */
static int
et_fa_default_cb(void *dev, ctf_ipc_t *ipc, bool v6, int cmd)
{
	return BCME_OK;
}

static int
et_fa_normal_cb(void *dev, ctf_ipc_t *ipc, bool v6, int cmd)
{
	int error = BCME_OK;
	et_info_t *et;

	/* Validate the input params */
	if (dev == NULL || ipc == NULL)
		return BCME_ERROR;

	et = ET_INFO((struct net_device *)dev);
	switch (cmd) {
		case FA_CB_ADD_NAPT:
			error = fa_napt_add(et->etc->fa, ipc, v6);
			break;
		case FA_CB_DEL_NAPT:
			error = fa_napt_del(et->etc->fa, ipc, v6);
			break;
		case FA_CB_GET_LIVE:
			fa_napt_live(et->etc->fa, ipc, v6);
			break;
		case FA_CB_CONNTRACK:
			fa_conntrack(et->etc->fa, ipc, v6);
			break;
	}

	return error;
}

void *
et_fa_fs_create(void)
{
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *proc_fa = NULL;

	if ((proc_fa = create_proc_entry("fa", 0, NULL)))
		proc_fa->read_proc = fa_read_proc;

	return proc_fa;
#else
	return NULL;
#endif /* CONFIG_PROC_FS */
}

void
et_fa_fs_clean(void)
{
#ifdef CONFIG_PROC_FS
	remove_proc_entry("fa", NULL);
#endif /* CONFIG_PROC_FS */
}

void
et_fa_up(et_info_t *et)
{
	ET_LOCK(et);
	et_up(et);
	ET_UNLOCK(et);
}
#endif /* ETFA */
