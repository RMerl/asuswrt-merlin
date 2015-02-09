/*
 * This file is part of the Chelsio T4 Ethernet driver for Linux.
 *
 * Copyright (c) 2003-2010 Chelsio Communications, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/bitmap.h>
#include <linux/crc32.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/etherdevice.h>
#include <linux/firmware.h>
#include <linux/if_vlan.h>
#include <linux/init.h>
#include <linux/log2.h>
#include <linux/mdio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/aer.h>
#include <linux/rtnetlink.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/sockios.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <net/neighbour.h>
#include <net/netevent.h>
#include <asm/uaccess.h>

#include "cxgb4.h"
#include "t4_regs.h"
#include "t4_msg.h"
#include "t4fw_api.h"
#include "l2t.h"

#define DRV_VERSION "1.3.0-ko"
#define DRV_DESC "Chelsio T4 Network Driver"

/*
 * Max interrupt hold-off timer value in us.  Queues fall back to this value
 * under extreme memory pressure so it's largish to give the system time to
 * recover.
 */
#define MAX_SGE_TIMERVAL 200U

#ifdef CONFIG_PCI_IOV
/*
 * Virtual Function provisioning constants.  We need two extra Ingress Queues
 * with Interrupt capability to serve as the VF's Firmware Event Queue and
 * Forwarded Interrupt Queue (when using MSI mode) -- neither will have Free
 * Lists associated with them).  For each Ethernet/Control Egress Queue and
 * for each Free List, we need an Egress Context.
 */
enum {
	VFRES_NPORTS = 1,		/* # of "ports" per VF */
	VFRES_NQSETS = 2,		/* # of "Queue Sets" per VF */

	VFRES_NVI = VFRES_NPORTS,	/* # of Virtual Interfaces */
	VFRES_NETHCTRL = VFRES_NQSETS,	/* # of EQs used for ETH or CTRL Qs */
	VFRES_NIQFLINT = VFRES_NQSETS+2,/* # of ingress Qs/w Free List(s)/intr */
	VFRES_NIQ = 0,			/* # of non-fl/int ingress queues */
	VFRES_NEQ = VFRES_NQSETS*2,	/* # of egress queues */
	VFRES_TC = 0,			/* PCI-E traffic class */
	VFRES_NEXACTF = 16,		/* # of exact MPS filters */

	VFRES_R_CAPS = FW_CMD_CAP_DMAQ|FW_CMD_CAP_VF|FW_CMD_CAP_PORT,
	VFRES_WX_CAPS = FW_CMD_CAP_DMAQ|FW_CMD_CAP_VF,
};

/*
 * Provide a Port Access Rights Mask for the specified PF/VF.  This is very
 * static and likely not to be useful in the long run.  We really need to
 * implement some form of persistent configuration which the firmware
 * controls.
 */
static unsigned int pfvfres_pmask(struct adapter *adapter,
				  unsigned int pf, unsigned int vf)
{
	unsigned int portn, portvec;

	/*
	 * Give PF's access to all of the ports.
	 */
	if (vf == 0)
		return FW_PFVF_CMD_PMASK_MASK;

	/*
	 * For VFs, we'll assign them access to the ports based purely on the
	 * PF.  We assign active ports in order, wrapping around if there are
	 * fewer active ports than PFs: e.g. active port[pf % nports].
	 * Unfortunately the adapter's port_info structs haven't been
	 * initialized yet so we have to compute this.
	 */
	if (adapter->params.nports == 0)
		return 0;

	portn = pf % adapter->params.nports;
	portvec = adapter->params.portvec;
	for (;;) {
		/*
		 * Isolate the lowest set bit in the port vector.  If we're at
		 * the port number that we want, return that as the pmask.
		 * otherwise mask that bit out of the port vector and
		 * decrement our port number ...
		 */
		unsigned int pmask = portvec ^ (portvec & (portvec-1));
		if (portn == 0)
			return pmask;
		portn--;
		portvec &= ~pmask;
	}
	/*NOTREACHED*/
}
#endif

enum {
	MEMWIN0_APERTURE = 65536,
	MEMWIN0_BASE     = 0x30000,
	MEMWIN1_APERTURE = 32768,
	MEMWIN1_BASE     = 0x28000,
	MEMWIN2_APERTURE = 2048,
	MEMWIN2_BASE     = 0x1b800,
};

enum {
	MAX_TXQ_ENTRIES      = 16384,
	MAX_CTRL_TXQ_ENTRIES = 1024,
	MAX_RSPQ_ENTRIES     = 16384,
	MAX_RX_BUFFERS       = 16384,
	MIN_TXQ_ENTRIES      = 32,
	MIN_CTRL_TXQ_ENTRIES = 32,
	MIN_RSPQ_ENTRIES     = 128,
	MIN_FL_ENTRIES       = 16
};

#define DFLT_MSG_ENABLE (NETIF_MSG_DRV | NETIF_MSG_PROBE | NETIF_MSG_LINK | \
			 NETIF_MSG_TIMER | NETIF_MSG_IFDOWN | NETIF_MSG_IFUP |\
			 NETIF_MSG_RX_ERR | NETIF_MSG_TX_ERR)

#define CH_DEVICE(devid, data) { PCI_VDEVICE(CHELSIO, devid), (data) }

static DEFINE_PCI_DEVICE_TABLE(cxgb4_pci_tbl) = {
	CH_DEVICE(0xa000, 0),  /* PE10K */
	CH_DEVICE(0x4001, 0),
	CH_DEVICE(0x4002, 0),
	CH_DEVICE(0x4003, 0),
	CH_DEVICE(0x4004, 0),
	CH_DEVICE(0x4005, 0),
	CH_DEVICE(0x4006, 0),
	CH_DEVICE(0x4007, 0),
	CH_DEVICE(0x4008, 0),
	CH_DEVICE(0x4009, 0),
	CH_DEVICE(0x400a, 0),
	{ 0, }
};

#define FW_FNAME "cxgb4/t4fw.bin"

MODULE_DESCRIPTION(DRV_DESC);
MODULE_AUTHOR("Chelsio Communications");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRV_VERSION);
MODULE_DEVICE_TABLE(pci, cxgb4_pci_tbl);
MODULE_FIRMWARE(FW_FNAME);

static int dflt_msg_enable = DFLT_MSG_ENABLE;

module_param(dflt_msg_enable, int, 0644);
MODULE_PARM_DESC(dflt_msg_enable, "Chelsio T4 default message enable bitmap");

/*
 * The driver uses the best interrupt scheme available on a platform in the
 * order MSI-X, MSI, legacy INTx interrupts.  This parameter determines which
 * of these schemes the driver may consider as follows:
 *
 * msi = 2: choose from among all three options
 * msi = 1: only consider MSI and INTx interrupts
 * msi = 0: force INTx interrupts
 */
static int msi = 2;

module_param(msi, int, 0644);
MODULE_PARM_DESC(msi, "whether to use INTx (0), MSI (1) or MSI-X (2)");

/*
 * Queue interrupt hold-off timer values.  Queues default to the first of these
 * upon creation.
 */
static unsigned int intr_holdoff[SGE_NTIMERS - 1] = { 5, 10, 20, 50, 100 };

module_param_array(intr_holdoff, uint, NULL, 0644);
MODULE_PARM_DESC(intr_holdoff, "values for queue interrupt hold-off timers "
		 "0..4 in microseconds");

static unsigned int intr_cnt[SGE_NCOUNTERS - 1] = { 4, 8, 16 };

module_param_array(intr_cnt, uint, NULL, 0644);
MODULE_PARM_DESC(intr_cnt,
		 "thresholds 1..3 for queue interrupt packet counters");

static int vf_acls;

#ifdef CONFIG_PCI_IOV
module_param(vf_acls, bool, 0644);
MODULE_PARM_DESC(vf_acls, "if set enable virtualization L2 ACL enforcement");

static unsigned int num_vf[4];

module_param_array(num_vf, uint, NULL, 0644);
MODULE_PARM_DESC(num_vf, "number of VFs for each of PFs 0-3");
#endif

static struct dentry *cxgb4_debugfs_root;

static LIST_HEAD(adapter_list);
static DEFINE_MUTEX(uld_mutex);
static struct cxgb4_uld_info ulds[CXGB4_ULD_MAX];
static const char *uld_str[] = { "RDMA", "iSCSI" };

static void link_report(struct net_device *dev)
{
	if (!netif_carrier_ok(dev))
		netdev_info(dev, "link down\n");
	else {
		static const char *fc[] = { "no", "Rx", "Tx", "Tx/Rx" };

		const char *s = "10Mbps";
		const struct port_info *p = netdev_priv(dev);

		switch (p->link_cfg.speed) {
		case SPEED_10000:
			s = "10Gbps";
			break;
		case SPEED_1000:
			s = "1000Mbps";
			break;
		case SPEED_100:
			s = "100Mbps";
			break;
		}

		netdev_info(dev, "link up, %s, full-duplex, %s PAUSE\n", s,
			    fc[p->link_cfg.fc]);
	}
}

void t4_os_link_changed(struct adapter *adapter, int port_id, int link_stat)
{
	struct net_device *dev = adapter->port[port_id];

	/* Skip changes from disabled ports. */
	if (netif_running(dev) && link_stat != netif_carrier_ok(dev)) {
		if (link_stat)
			netif_carrier_on(dev);
		else
			netif_carrier_off(dev);

		link_report(dev);
	}
}

void t4_os_portmod_changed(const struct adapter *adap, int port_id)
{
	static const char *mod_str[] = {
		NULL, "LR", "SR", "ER", "passive DA", "active DA", "LRM"
	};

	const struct net_device *dev = adap->port[port_id];
	const struct port_info *pi = netdev_priv(dev);

	if (pi->mod_type == FW_PORT_MOD_TYPE_NONE)
		netdev_info(dev, "port module unplugged\n");
	else if (pi->mod_type < ARRAY_SIZE(mod_str))
		netdev_info(dev, "%s module inserted\n", mod_str[pi->mod_type]);
}

/*
 * Configure the exact and hash address filters to handle a port's multicast
 * and secondary unicast MAC addresses.
 */
static int set_addr_filters(const struct net_device *dev, bool sleep)
{
	u64 mhash = 0;
	u64 uhash = 0;
	bool free = true;
	u16 filt_idx[7];
	const u8 *addr[7];
	int ret, naddr = 0;
	const struct netdev_hw_addr *ha;
	int uc_cnt = netdev_uc_count(dev);
	int mc_cnt = netdev_mc_count(dev);
	const struct port_info *pi = netdev_priv(dev);
	unsigned int mb = pi->adapter->fn;

	/* first do the secondary unicast addresses */
	netdev_for_each_uc_addr(ha, dev) {
		addr[naddr++] = ha->addr;
		if (--uc_cnt == 0 || naddr >= ARRAY_SIZE(addr)) {
			ret = t4_alloc_mac_filt(pi->adapter, mb, pi->viid, free,
					naddr, addr, filt_idx, &uhash, sleep);
			if (ret < 0)
				return ret;

			free = false;
			naddr = 0;
		}
	}

	/* next set up the multicast addresses */
	netdev_for_each_mc_addr(ha, dev) {
		addr[naddr++] = ha->addr;
		if (--mc_cnt == 0 || naddr >= ARRAY_SIZE(addr)) {
			ret = t4_alloc_mac_filt(pi->adapter, mb, pi->viid, free,
					naddr, addr, filt_idx, &mhash, sleep);
			if (ret < 0)
				return ret;

			free = false;
			naddr = 0;
		}
	}

	return t4_set_addr_hash(pi->adapter, mb, pi->viid, uhash != 0,
				uhash | mhash, sleep);
}

/*
 * Set Rx properties of a port, such as promiscruity, address filters, and MTU.
 * If @mtu is -1 it is left unchanged.
 */
static int set_rxmode(struct net_device *dev, int mtu, bool sleep_ok)
{
	int ret;
	struct port_info *pi = netdev_priv(dev);

	ret = set_addr_filters(dev, sleep_ok);
	if (ret == 0)
		ret = t4_set_rxmode(pi->adapter, pi->adapter->fn, pi->viid, mtu,
				    (dev->flags & IFF_PROMISC) ? 1 : 0,
				    (dev->flags & IFF_ALLMULTI) ? 1 : 0, 1, -1,
				    sleep_ok);
	return ret;
}

/**
 *	link_start - enable a port
 *	@dev: the port to enable
 *
 *	Performs the MAC and PHY actions needed to enable a port.
 */
static int link_start(struct net_device *dev)
{
	int ret;
	struct port_info *pi = netdev_priv(dev);
	unsigned int mb = pi->adapter->fn;

	/*
	 * We do not set address filters and promiscuity here, the stack does
	 * that step explicitly.
	 */
	ret = t4_set_rxmode(pi->adapter, mb, pi->viid, dev->mtu, -1, -1, -1,
			    pi->vlan_grp != NULL, true);
	if (ret == 0) {
		ret = t4_change_mac(pi->adapter, mb, pi->viid,
				    pi->xact_addr_filt, dev->dev_addr, true,
				    true);
		if (ret >= 0) {
			pi->xact_addr_filt = ret;
			ret = 0;
		}
	}
	if (ret == 0)
		ret = t4_link_start(pi->adapter, mb, pi->tx_chan,
				    &pi->link_cfg);
	if (ret == 0)
		ret = t4_enable_vi(pi->adapter, mb, pi->viid, true, true);
	return ret;
}

/*
 * Response queue handler for the FW event queue.
 */
static int fwevtq_handler(struct sge_rspq *q, const __be64 *rsp,
			  const struct pkt_gl *gl)
{
	u8 opcode = ((const struct rss_header *)rsp)->opcode;

	rsp++;                                          /* skip RSS header */
	if (likely(opcode == CPL_SGE_EGR_UPDATE)) {
		const struct cpl_sge_egr_update *p = (void *)rsp;
		unsigned int qid = EGR_QID(ntohl(p->opcode_qid));
		struct sge_txq *txq = q->adap->sge.egr_map[qid];

		txq->restarts++;
		if ((u8 *)txq < (u8 *)q->adap->sge.ethrxq) {
			struct sge_eth_txq *eq;

			eq = container_of(txq, struct sge_eth_txq, q);
			netif_tx_wake_queue(eq->txq);
		} else {
			struct sge_ofld_txq *oq;

			oq = container_of(txq, struct sge_ofld_txq, q);
			tasklet_schedule(&oq->qresume_tsk);
		}
	} else if (opcode == CPL_FW6_MSG || opcode == CPL_FW4_MSG) {
		const struct cpl_fw6_msg *p = (void *)rsp;

		if (p->type == 0)
			t4_handle_fw_rpl(q->adap, p->data);
	} else if (opcode == CPL_L2T_WRITE_RPL) {
		const struct cpl_l2t_write_rpl *p = (void *)rsp;

		do_l2t_write_rpl(q->adap, p);
	} else
		dev_err(q->adap->pdev_dev,
			"unexpected CPL %#x on FW event queue\n", opcode);
	return 0;
}

/**
 *	uldrx_handler - response queue handler for ULD queues
 *	@q: the response queue that received the packet
 *	@rsp: the response queue descriptor holding the offload message
 *	@gl: the gather list of packet fragments
 *
 *	Deliver an ingress offload packet to a ULD.  All processing is done by
 *	the ULD, we just maintain statistics.
 */
static int uldrx_handler(struct sge_rspq *q, const __be64 *rsp,
			 const struct pkt_gl *gl)
{
	struct sge_ofld_rxq *rxq = container_of(q, struct sge_ofld_rxq, rspq);

	if (ulds[q->uld].rx_handler(q->adap->uld_handle[q->uld], rsp, gl)) {
		rxq->stats.nomem++;
		return -1;
	}
	if (gl == NULL)
		rxq->stats.imm++;
	else if (gl == CXGB4_MSG_AN)
		rxq->stats.an++;
	else
		rxq->stats.pkts++;
	return 0;
}

static void disable_msi(struct adapter *adapter)
{
	if (adapter->flags & USING_MSIX) {
		pci_disable_msix(adapter->pdev);
		adapter->flags &= ~USING_MSIX;
	} else if (adapter->flags & USING_MSI) {
		pci_disable_msi(adapter->pdev);
		adapter->flags &= ~USING_MSI;
	}
}

/*
 * Interrupt handler for non-data events used with MSI-X.
 */
static irqreturn_t t4_nondata_intr(int irq, void *cookie)
{
	struct adapter *adap = cookie;

	u32 v = t4_read_reg(adap, MYPF_REG(PL_PF_INT_CAUSE));
	if (v & PFSW) {
		adap->swintr = 1;
		t4_write_reg(adap, MYPF_REG(PL_PF_INT_CAUSE), v);
	}
	t4_slow_intr_handler(adap);
	return IRQ_HANDLED;
}

/*
 * Name the MSI-X interrupts.
 */
static void name_msix_vecs(struct adapter *adap)
{
	int i, j, msi_idx = 2, n = sizeof(adap->msix_info[0].desc) - 1;

	/* non-data interrupts */
	snprintf(adap->msix_info[0].desc, n, "%s", adap->name);
	adap->msix_info[0].desc[n] = 0;

	/* FW events */
	snprintf(adap->msix_info[1].desc, n, "%s-FWeventq", adap->name);
	adap->msix_info[1].desc[n] = 0;

	/* Ethernet queues */
	for_each_port(adap, j) {
		struct net_device *d = adap->port[j];
		const struct port_info *pi = netdev_priv(d);

		for (i = 0; i < pi->nqsets; i++, msi_idx++) {
			snprintf(adap->msix_info[msi_idx].desc, n, "%s-Rx%d",
				 d->name, i);
			adap->msix_info[msi_idx].desc[n] = 0;
		}
	}

	/* offload queues */
	for_each_ofldrxq(&adap->sge, i) {
		snprintf(adap->msix_info[msi_idx].desc, n, "%s-ofld%d",
			 adap->name, i);
		adap->msix_info[msi_idx++].desc[n] = 0;
	}
	for_each_rdmarxq(&adap->sge, i) {
		snprintf(adap->msix_info[msi_idx].desc, n, "%s-rdma%d",
			 adap->name, i);
		adap->msix_info[msi_idx++].desc[n] = 0;
	}
}

static int request_msix_queue_irqs(struct adapter *adap)
{
	struct sge *s = &adap->sge;
	int err, ethqidx, ofldqidx = 0, rdmaqidx = 0, msi = 2;

	err = request_irq(adap->msix_info[1].vec, t4_sge_intr_msix, 0,
			  adap->msix_info[1].desc, &s->fw_evtq);
	if (err)
		return err;

	for_each_ethrxq(s, ethqidx) {
		err = request_irq(adap->msix_info[msi].vec, t4_sge_intr_msix, 0,
				  adap->msix_info[msi].desc,
				  &s->ethrxq[ethqidx].rspq);
		if (err)
			goto unwind;
		msi++;
	}
	for_each_ofldrxq(s, ofldqidx) {
		err = request_irq(adap->msix_info[msi].vec, t4_sge_intr_msix, 0,
				  adap->msix_info[msi].desc,
				  &s->ofldrxq[ofldqidx].rspq);
		if (err)
			goto unwind;
		msi++;
	}
	for_each_rdmarxq(s, rdmaqidx) {
		err = request_irq(adap->msix_info[msi].vec, t4_sge_intr_msix, 0,
				  adap->msix_info[msi].desc,
				  &s->rdmarxq[rdmaqidx].rspq);
		if (err)
			goto unwind;
		msi++;
	}
	return 0;

unwind:
	while (--rdmaqidx >= 0)
		free_irq(adap->msix_info[--msi].vec,
			 &s->rdmarxq[rdmaqidx].rspq);
	while (--ofldqidx >= 0)
		free_irq(adap->msix_info[--msi].vec,
			 &s->ofldrxq[ofldqidx].rspq);
	while (--ethqidx >= 0)
		free_irq(adap->msix_info[--msi].vec, &s->ethrxq[ethqidx].rspq);
	free_irq(adap->msix_info[1].vec, &s->fw_evtq);
	return err;
}

static void free_msix_queue_irqs(struct adapter *adap)
{
	int i, msi = 2;
	struct sge *s = &adap->sge;

	free_irq(adap->msix_info[1].vec, &s->fw_evtq);
	for_each_ethrxq(s, i)
		free_irq(adap->msix_info[msi++].vec, &s->ethrxq[i].rspq);
	for_each_ofldrxq(s, i)
		free_irq(adap->msix_info[msi++].vec, &s->ofldrxq[i].rspq);
	for_each_rdmarxq(s, i)
		free_irq(adap->msix_info[msi++].vec, &s->rdmarxq[i].rspq);
}

/**
 *	write_rss - write the RSS table for a given port
 *	@pi: the port
 *	@queues: array of queue indices for RSS
 *
 *	Sets up the portion of the HW RSS table for the port's VI to distribute
 *	packets to the Rx queues in @queues.
 */
static int write_rss(const struct port_info *pi, const u16 *queues)
{
	u16 *rss;
	int i, err;
	const struct sge_eth_rxq *q = &pi->adapter->sge.ethrxq[pi->first_qset];

	rss = kmalloc(pi->rss_size * sizeof(u16), GFP_KERNEL);
	if (!rss)
		return -ENOMEM;

	/* map the queue indices to queue ids */
	for (i = 0; i < pi->rss_size; i++, queues++)
		rss[i] = q[*queues].rspq.abs_id;

	err = t4_config_rss_range(pi->adapter, pi->adapter->fn, pi->viid, 0,
				  pi->rss_size, rss, pi->rss_size);
	kfree(rss);
	return err;
}

/**
 *	setup_rss - configure RSS
 *	@adap: the adapter
 *
 *	Sets up RSS for each port.
 */
static int setup_rss(struct adapter *adap)
{
	int i, err;

	for_each_port(adap, i) {
		const struct port_info *pi = adap2pinfo(adap, i);

		err = write_rss(pi, pi->rss);
		if (err)
			return err;
	}
	return 0;
}

/*
 * Wait until all NAPI handlers are descheduled.
 */
static void quiesce_rx(struct adapter *adap)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(adap->sge.ingr_map); i++) {
		struct sge_rspq *q = adap->sge.ingr_map[i];

		if (q && q->handler)
			napi_disable(&q->napi);
	}
}

/*
 * Enable NAPI scheduling and interrupt generation for all Rx queues.
 */
static void enable_rx(struct adapter *adap)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(adap->sge.ingr_map); i++) {
		struct sge_rspq *q = adap->sge.ingr_map[i];

		if (!q)
			continue;
		if (q->handler)
			napi_enable(&q->napi);
		/* 0-increment GTS to start the timer and enable interrupts */
		t4_write_reg(adap, MYPF_REG(SGE_PF_GTS),
			     SEINTARM(q->intr_params) |
			     INGRESSQID(q->cntxt_id));
	}
}

/**
 *	setup_sge_queues - configure SGE Tx/Rx/response queues
 *	@adap: the adapter
 *
 *	Determines how many sets of SGE queues to use and initializes them.
 *	We support multiple queue sets per port if we have MSI-X, otherwise
 *	just one queue set per port.
 */
static int setup_sge_queues(struct adapter *adap)
{
	int err, msi_idx, i, j;
	struct sge *s = &adap->sge;

	bitmap_zero(s->starving_fl, MAX_EGRQ);
	bitmap_zero(s->txq_maperr, MAX_EGRQ);

	if (adap->flags & USING_MSIX)
		msi_idx = 1;         /* vector 0 is for non-queue interrupts */
	else {
		err = t4_sge_alloc_rxq(adap, &s->intrq, false, adap->port[0], 0,
				       NULL, NULL);
		if (err)
			return err;
		msi_idx = -((int)s->intrq.abs_id + 1);
	}

	err = t4_sge_alloc_rxq(adap, &s->fw_evtq, true, adap->port[0],
			       msi_idx, NULL, fwevtq_handler);
	if (err) {
freeout:	t4_free_sge_resources(adap);
		return err;
	}

	for_each_port(adap, i) {
		struct net_device *dev = adap->port[i];
		struct port_info *pi = netdev_priv(dev);
		struct sge_eth_rxq *q = &s->ethrxq[pi->first_qset];
		struct sge_eth_txq *t = &s->ethtxq[pi->first_qset];

		for (j = 0; j < pi->nqsets; j++, q++) {
			if (msi_idx > 0)
				msi_idx++;
			err = t4_sge_alloc_rxq(adap, &q->rspq, false, dev,
					       msi_idx, &q->fl,
					       t4_ethrx_handler);
			if (err)
				goto freeout;
			q->rspq.idx = j;
			memset(&q->stats, 0, sizeof(q->stats));
		}
		for (j = 0; j < pi->nqsets; j++, t++) {
			err = t4_sge_alloc_eth_txq(adap, t, dev,
					netdev_get_tx_queue(dev, j),
					s->fw_evtq.cntxt_id);
			if (err)
				goto freeout;
		}
	}

	j = s->ofldqsets / adap->params.nports; /* ofld queues per channel */
	for_each_ofldrxq(s, i) {
		struct sge_ofld_rxq *q = &s->ofldrxq[i];
		struct net_device *dev = adap->port[i / j];

		if (msi_idx > 0)
			msi_idx++;
		err = t4_sge_alloc_rxq(adap, &q->rspq, false, dev, msi_idx,
				       &q->fl, uldrx_handler);
		if (err)
			goto freeout;
		memset(&q->stats, 0, sizeof(q->stats));
		s->ofld_rxq[i] = q->rspq.abs_id;
		err = t4_sge_alloc_ofld_txq(adap, &s->ofldtxq[i], dev,
					    s->fw_evtq.cntxt_id);
		if (err)
			goto freeout;
	}

	for_each_rdmarxq(s, i) {
		struct sge_ofld_rxq *q = &s->rdmarxq[i];

		if (msi_idx > 0)
			msi_idx++;
		err = t4_sge_alloc_rxq(adap, &q->rspq, false, adap->port[i],
				       msi_idx, &q->fl, uldrx_handler);
		if (err)
			goto freeout;
		memset(&q->stats, 0, sizeof(q->stats));
		s->rdma_rxq[i] = q->rspq.abs_id;
	}

	for_each_port(adap, i) {
		/*
		 * Note that ->rdmarxq[i].rspq.cntxt_id below is 0 if we don't
		 * have RDMA queues, and that's the right value.
		 */
		err = t4_sge_alloc_ctrl_txq(adap, &s->ctrlq[i], adap->port[i],
					    s->fw_evtq.cntxt_id,
					    s->rdmarxq[i].rspq.cntxt_id);
		if (err)
			goto freeout;
	}

	t4_write_reg(adap, MPS_TRC_RSS_CONTROL,
		     RSSCONTROL(netdev2pinfo(adap->port[0])->tx_chan) |
		     QUEUENUMBER(s->ethrxq[0].rspq.abs_id));
	return 0;
}

/*
 * Returns 0 if new FW was successfully loaded, a positive errno if a load was
 * started but failed, and a negative errno if flash load couldn't start.
 */
static int upgrade_fw(struct adapter *adap)
{
	int ret;
	u32 vers;
	const struct fw_hdr *hdr;
	const struct firmware *fw;
	struct device *dev = adap->pdev_dev;

	ret = request_firmware(&fw, FW_FNAME, dev);
	if (ret < 0) {
		dev_err(dev, "unable to load firmware image " FW_FNAME
			", error %d\n", ret);
		return ret;
	}

	hdr = (const struct fw_hdr *)fw->data;
	vers = ntohl(hdr->fw_ver);
	if (FW_HDR_FW_VER_MAJOR_GET(vers) != FW_VERSION_MAJOR) {
		ret = -EINVAL;              /* wrong major version, won't do */
		goto out;
	}

	/*
	 * If the flash FW is unusable or we found something newer, load it.
	 */
	if (FW_HDR_FW_VER_MAJOR_GET(adap->params.fw_vers) != FW_VERSION_MAJOR ||
	    vers > adap->params.fw_vers) {
		ret = -t4_load_fw(adap, fw->data, fw->size);
		if (!ret)
			dev_info(dev, "firmware upgraded to version %pI4 from "
				 FW_FNAME "\n", &hdr->fw_ver);
	}
out:	release_firmware(fw);
	return ret;
}

/*
 * Allocate a chunk of memory using kmalloc or, if that fails, vmalloc.
 * The allocated memory is cleared.
 */
void *t4_alloc_mem(size_t size)
{
	void *p = kmalloc(size, GFP_KERNEL);

	if (!p)
		p = vmalloc(size);
	if (p)
		memset(p, 0, size);
	return p;
}

/*
 * Free memory allocated through alloc_mem().
 */
void t4_free_mem(void *addr)
{
	if (is_vmalloc_addr(addr))
		vfree(addr);
	else
		kfree(addr);
}

static inline int is_offload(const struct adapter *adap)
{
	return adap->params.offload;
}

/*
 * Implementation of ethtool operations.
 */

static u32 get_msglevel(struct net_device *dev)
{
	return netdev2adap(dev)->msg_enable;
}

static void set_msglevel(struct net_device *dev, u32 val)
{
	netdev2adap(dev)->msg_enable = val;
}

static char stats_strings[][ETH_GSTRING_LEN] = {
	"TxOctetsOK         ",
	"TxFramesOK         ",
	"TxBroadcastFrames  ",
	"TxMulticastFrames  ",
	"TxUnicastFrames    ",
	"TxErrorFrames      ",

	"TxFrames64         ",
	"TxFrames65To127    ",
	"TxFrames128To255   ",
	"TxFrames256To511   ",
	"TxFrames512To1023  ",
	"TxFrames1024To1518 ",
	"TxFrames1519ToMax  ",

	"TxFramesDropped    ",
	"TxPauseFrames      ",
	"TxPPP0Frames       ",
	"TxPPP1Frames       ",
	"TxPPP2Frames       ",
	"TxPPP3Frames       ",
	"TxPPP4Frames       ",
	"TxPPP5Frames       ",
	"TxPPP6Frames       ",
	"TxPPP7Frames       ",

	"RxOctetsOK         ",
	"RxFramesOK         ",
	"RxBroadcastFrames  ",
	"RxMulticastFrames  ",
	"RxUnicastFrames    ",

	"RxFramesTooLong    ",
	"RxJabberErrors     ",
	"RxFCSErrors        ",
	"RxLengthErrors     ",
	"RxSymbolErrors     ",
	"RxRuntFrames       ",

	"RxFrames64         ",
	"RxFrames65To127    ",
	"RxFrames128To255   ",
	"RxFrames256To511   ",
	"RxFrames512To1023  ",
	"RxFrames1024To1518 ",
	"RxFrames1519ToMax  ",

	"RxPauseFrames      ",
	"RxPPP0Frames       ",
	"RxPPP1Frames       ",
	"RxPPP2Frames       ",
	"RxPPP3Frames       ",
	"RxPPP4Frames       ",
	"RxPPP5Frames       ",
	"RxPPP6Frames       ",
	"RxPPP7Frames       ",

	"RxBG0FramesDropped ",
	"RxBG1FramesDropped ",
	"RxBG2FramesDropped ",
	"RxBG3FramesDropped ",
	"RxBG0FramesTrunc   ",
	"RxBG1FramesTrunc   ",
	"RxBG2FramesTrunc   ",
	"RxBG3FramesTrunc   ",

	"TSO                ",
	"TxCsumOffload      ",
	"RxCsumGood         ",
	"VLANextractions    ",
	"VLANinsertions     ",
	"GROpackets         ",
	"GROmerged          ",
};

static int get_sset_count(struct net_device *dev, int sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return ARRAY_SIZE(stats_strings);
	default:
		return -EOPNOTSUPP;
	}
}

#define T4_REGMAP_SIZE (160 * 1024)

static int get_regs_len(struct net_device *dev)
{
	return T4_REGMAP_SIZE;
}

static int get_eeprom_len(struct net_device *dev)
{
	return EEPROMSIZE;
}

static void get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	struct adapter *adapter = netdev2adap(dev);

	strcpy(info->driver, KBUILD_MODNAME);
	strcpy(info->version, DRV_VERSION);
	strcpy(info->bus_info, pci_name(adapter->pdev));

	if (!adapter->params.fw_vers)
		strcpy(info->fw_version, "N/A");
	else
		snprintf(info->fw_version, sizeof(info->fw_version),
			"%u.%u.%u.%u, TP %u.%u.%u.%u",
			FW_HDR_FW_VER_MAJOR_GET(adapter->params.fw_vers),
			FW_HDR_FW_VER_MINOR_GET(adapter->params.fw_vers),
			FW_HDR_FW_VER_MICRO_GET(adapter->params.fw_vers),
			FW_HDR_FW_VER_BUILD_GET(adapter->params.fw_vers),
			FW_HDR_FW_VER_MAJOR_GET(adapter->params.tp_vers),
			FW_HDR_FW_VER_MINOR_GET(adapter->params.tp_vers),
			FW_HDR_FW_VER_MICRO_GET(adapter->params.tp_vers),
			FW_HDR_FW_VER_BUILD_GET(adapter->params.tp_vers));
}

static void get_strings(struct net_device *dev, u32 stringset, u8 *data)
{
	if (stringset == ETH_SS_STATS)
		memcpy(data, stats_strings, sizeof(stats_strings));
}

/*
 * port stats maintained per queue of the port.  They should be in the same
 * order as in stats_strings above.
 */
struct queue_port_stats {
	u64 tso;
	u64 tx_csum;
	u64 rx_csum;
	u64 vlan_ex;
	u64 vlan_ins;
	u64 gro_pkts;
	u64 gro_merged;
};

static void collect_sge_port_stats(const struct adapter *adap,
		const struct port_info *p, struct queue_port_stats *s)
{
	int i;
	const struct sge_eth_txq *tx = &adap->sge.ethtxq[p->first_qset];
	const struct sge_eth_rxq *rx = &adap->sge.ethrxq[p->first_qset];

	memset(s, 0, sizeof(*s));
	for (i = 0; i < p->nqsets; i++, rx++, tx++) {
		s->tso += tx->tso;
		s->tx_csum += tx->tx_cso;
		s->rx_csum += rx->stats.rx_cso;
		s->vlan_ex += rx->stats.vlan_ex;
		s->vlan_ins += tx->vlan_ins;
		s->gro_pkts += rx->stats.lro_pkts;
		s->gro_merged += rx->stats.lro_merged;
	}
}

static void get_stats(struct net_device *dev, struct ethtool_stats *stats,
		      u64 *data)
{
	struct port_info *pi = netdev_priv(dev);
	struct adapter *adapter = pi->adapter;

	t4_get_port_stats(adapter, pi->tx_chan, (struct port_stats *)data);

	data += sizeof(struct port_stats) / sizeof(u64);
	collect_sge_port_stats(adapter, pi, (struct queue_port_stats *)data);
}

/*
 * Return a version number to identify the type of adapter.  The scheme is:
 * - bits 0..9: chip version
 * - bits 10..15: chip revision
 * - bits 16..23: register dump version
 */
static inline unsigned int mk_adap_vers(const struct adapter *ap)
{
	return 4 | (ap->params.rev << 10) | (1 << 16);
}

static void reg_block_dump(struct adapter *ap, void *buf, unsigned int start,
			   unsigned int end)
{
	u32 *p = buf + start;

	for ( ; start <= end; start += sizeof(u32))
		*p++ = t4_read_reg(ap, start);
}

static void get_regs(struct net_device *dev, struct ethtool_regs *regs,
		     void *buf)
{
	static const unsigned int reg_ranges[] = {
		0x1008, 0x1108,
		0x1180, 0x11b4,
		0x11fc, 0x123c,
		0x1300, 0x173c,
		0x1800, 0x18fc,
		0x3000, 0x30d8,
		0x30e0, 0x5924,
		0x5960, 0x59d4,
		0x5a00, 0x5af8,
		0x6000, 0x6098,
		0x6100, 0x6150,
		0x6200, 0x6208,
		0x6240, 0x6248,
		0x6280, 0x6338,
		0x6370, 0x638c,
		0x6400, 0x643c,
		0x6500, 0x6524,
		0x6a00, 0x6a38,
		0x6a60, 0x6a78,
		0x6b00, 0x6b84,
		0x6bf0, 0x6c84,
		0x6cf0, 0x6d84,
		0x6df0, 0x6e84,
		0x6ef0, 0x6f84,
		0x6ff0, 0x7084,
		0x70f0, 0x7184,
		0x71f0, 0x7284,
		0x72f0, 0x7384,
		0x73f0, 0x7450,
		0x7500, 0x7530,
		0x7600, 0x761c,
		0x7680, 0x76cc,
		0x7700, 0x7798,
		0x77c0, 0x77fc,
		0x7900, 0x79fc,
		0x7b00, 0x7c38,
		0x7d00, 0x7efc,
		0x8dc0, 0x8e1c,
		0x8e30, 0x8e78,
		0x8ea0, 0x8f6c,
		0x8fc0, 0x9074,
		0x90fc, 0x90fc,
		0x9400, 0x9458,
		0x9600, 0x96bc,
		0x9800, 0x9808,
		0x9820, 0x983c,
		0x9850, 0x9864,
		0x9c00, 0x9c6c,
		0x9c80, 0x9cec,
		0x9d00, 0x9d6c,
		0x9d80, 0x9dec,
		0x9e00, 0x9e6c,
		0x9e80, 0x9eec,
		0x9f00, 0x9f6c,
		0x9f80, 0x9fec,
		0xd004, 0xd03c,
		0xdfc0, 0xdfe0,
		0xe000, 0xea7c,
		0xf000, 0x11190,
		0x19040, 0x1906c,
		0x19078, 0x19080,
		0x1908c, 0x19124,
		0x19150, 0x191b0,
		0x191d0, 0x191e8,
		0x19238, 0x1924c,
		0x193f8, 0x19474,
		0x19490, 0x194f8,
		0x19800, 0x19f30,
		0x1a000, 0x1a06c,
		0x1a0b0, 0x1a120,
		0x1a128, 0x1a138,
		0x1a190, 0x1a1c4,
		0x1a1fc, 0x1a1fc,
		0x1e040, 0x1e04c,
		0x1e284, 0x1e28c,
		0x1e2c0, 0x1e2c0,
		0x1e2e0, 0x1e2e0,
		0x1e300, 0x1e384,
		0x1e3c0, 0x1e3c8,
		0x1e440, 0x1e44c,
		0x1e684, 0x1e68c,
		0x1e6c0, 0x1e6c0,
		0x1e6e0, 0x1e6e0,
		0x1e700, 0x1e784,
		0x1e7c0, 0x1e7c8,
		0x1e840, 0x1e84c,
		0x1ea84, 0x1ea8c,
		0x1eac0, 0x1eac0,
		0x1eae0, 0x1eae0,
		0x1eb00, 0x1eb84,
		0x1ebc0, 0x1ebc8,
		0x1ec40, 0x1ec4c,
		0x1ee84, 0x1ee8c,
		0x1eec0, 0x1eec0,
		0x1eee0, 0x1eee0,
		0x1ef00, 0x1ef84,
		0x1efc0, 0x1efc8,
		0x1f040, 0x1f04c,
		0x1f284, 0x1f28c,
		0x1f2c0, 0x1f2c0,
		0x1f2e0, 0x1f2e0,
		0x1f300, 0x1f384,
		0x1f3c0, 0x1f3c8,
		0x1f440, 0x1f44c,
		0x1f684, 0x1f68c,
		0x1f6c0, 0x1f6c0,
		0x1f6e0, 0x1f6e0,
		0x1f700, 0x1f784,
		0x1f7c0, 0x1f7c8,
		0x1f840, 0x1f84c,
		0x1fa84, 0x1fa8c,
		0x1fac0, 0x1fac0,
		0x1fae0, 0x1fae0,
		0x1fb00, 0x1fb84,
		0x1fbc0, 0x1fbc8,
		0x1fc40, 0x1fc4c,
		0x1fe84, 0x1fe8c,
		0x1fec0, 0x1fec0,
		0x1fee0, 0x1fee0,
		0x1ff00, 0x1ff84,
		0x1ffc0, 0x1ffc8,
		0x20000, 0x2002c,
		0x20100, 0x2013c,
		0x20190, 0x201c8,
		0x20200, 0x20318,
		0x20400, 0x20528,
		0x20540, 0x20614,
		0x21000, 0x21040,
		0x2104c, 0x21060,
		0x210c0, 0x210ec,
		0x21200, 0x21268,
		0x21270, 0x21284,
		0x212fc, 0x21388,
		0x21400, 0x21404,
		0x21500, 0x21518,
		0x2152c, 0x2153c,
		0x21550, 0x21554,
		0x21600, 0x21600,
		0x21608, 0x21628,
		0x21630, 0x2163c,
		0x21700, 0x2171c,
		0x21780, 0x2178c,
		0x21800, 0x21c38,
		0x21c80, 0x21d7c,
		0x21e00, 0x21e04,
		0x22000, 0x2202c,
		0x22100, 0x2213c,
		0x22190, 0x221c8,
		0x22200, 0x22318,
		0x22400, 0x22528,
		0x22540, 0x22614,
		0x23000, 0x23040,
		0x2304c, 0x23060,
		0x230c0, 0x230ec,
		0x23200, 0x23268,
		0x23270, 0x23284,
		0x232fc, 0x23388,
		0x23400, 0x23404,
		0x23500, 0x23518,
		0x2352c, 0x2353c,
		0x23550, 0x23554,
		0x23600, 0x23600,
		0x23608, 0x23628,
		0x23630, 0x2363c,
		0x23700, 0x2371c,
		0x23780, 0x2378c,
		0x23800, 0x23c38,
		0x23c80, 0x23d7c,
		0x23e00, 0x23e04,
		0x24000, 0x2402c,
		0x24100, 0x2413c,
		0x24190, 0x241c8,
		0x24200, 0x24318,
		0x24400, 0x24528,
		0x24540, 0x24614,
		0x25000, 0x25040,
		0x2504c, 0x25060,
		0x250c0, 0x250ec,
		0x25200, 0x25268,
		0x25270, 0x25284,
		0x252fc, 0x25388,
		0x25400, 0x25404,
		0x25500, 0x25518,
		0x2552c, 0x2553c,
		0x25550, 0x25554,
		0x25600, 0x25600,
		0x25608, 0x25628,
		0x25630, 0x2563c,
		0x25700, 0x2571c,
		0x25780, 0x2578c,
		0x25800, 0x25c38,
		0x25c80, 0x25d7c,
		0x25e00, 0x25e04,
		0x26000, 0x2602c,
		0x26100, 0x2613c,
		0x26190, 0x261c8,
		0x26200, 0x26318,
		0x26400, 0x26528,
		0x26540, 0x26614,
		0x27000, 0x27040,
		0x2704c, 0x27060,
		0x270c0, 0x270ec,
		0x27200, 0x27268,
		0x27270, 0x27284,
		0x272fc, 0x27388,
		0x27400, 0x27404,
		0x27500, 0x27518,
		0x2752c, 0x2753c,
		0x27550, 0x27554,
		0x27600, 0x27600,
		0x27608, 0x27628,
		0x27630, 0x2763c,
		0x27700, 0x2771c,
		0x27780, 0x2778c,
		0x27800, 0x27c38,
		0x27c80, 0x27d7c,
		0x27e00, 0x27e04
	};

	int i;
	struct adapter *ap = netdev2adap(dev);

	regs->version = mk_adap_vers(ap);

	memset(buf, 0, T4_REGMAP_SIZE);
	for (i = 0; i < ARRAY_SIZE(reg_ranges); i += 2)
		reg_block_dump(ap, buf, reg_ranges[i], reg_ranges[i + 1]);
}

static int restart_autoneg(struct net_device *dev)
{
	struct port_info *p = netdev_priv(dev);

	if (!netif_running(dev))
		return -EAGAIN;
	if (p->link_cfg.autoneg != AUTONEG_ENABLE)
		return -EINVAL;
	t4_restart_aneg(p->adapter, p->adapter->fn, p->tx_chan);
	return 0;
}

static int identify_port(struct net_device *dev, u32 data)
{
	struct adapter *adap = netdev2adap(dev);

	if (data == 0)
		data = 2;     /* default to 2 seconds */

	return t4_identify_port(adap, adap->fn, netdev2pinfo(dev)->viid,
				data * 5);
}

static unsigned int from_fw_linkcaps(unsigned int type, unsigned int caps)
{
	unsigned int v = 0;

	if (type == FW_PORT_TYPE_BT_SGMII || type == FW_PORT_TYPE_BT_XFI ||
	    type == FW_PORT_TYPE_BT_XAUI) {
		v |= SUPPORTED_TP;
		if (caps & FW_PORT_CAP_SPEED_100M)
			v |= SUPPORTED_100baseT_Full;
		if (caps & FW_PORT_CAP_SPEED_1G)
			v |= SUPPORTED_1000baseT_Full;
		if (caps & FW_PORT_CAP_SPEED_10G)
			v |= SUPPORTED_10000baseT_Full;
	} else if (type == FW_PORT_TYPE_KX4 || type == FW_PORT_TYPE_KX) {
		v |= SUPPORTED_Backplane;
		if (caps & FW_PORT_CAP_SPEED_1G)
			v |= SUPPORTED_1000baseKX_Full;
		if (caps & FW_PORT_CAP_SPEED_10G)
			v |= SUPPORTED_10000baseKX4_Full;
	} else if (type == FW_PORT_TYPE_KR)
		v |= SUPPORTED_Backplane | SUPPORTED_10000baseKR_Full;
	else if (type == FW_PORT_TYPE_BP_AP)
		v |= SUPPORTED_Backplane | SUPPORTED_10000baseR_FEC;
	else if (type == FW_PORT_TYPE_FIBER_XFI ||
		 type == FW_PORT_TYPE_FIBER_XAUI || type == FW_PORT_TYPE_SFP)
		v |= SUPPORTED_FIBRE;

	if (caps & FW_PORT_CAP_ANEG)
		v |= SUPPORTED_Autoneg;
	return v;
}

static unsigned int to_fw_linkcaps(unsigned int caps)
{
	unsigned int v = 0;

	if (caps & ADVERTISED_100baseT_Full)
		v |= FW_PORT_CAP_SPEED_100M;
	if (caps & ADVERTISED_1000baseT_Full)
		v |= FW_PORT_CAP_SPEED_1G;
	if (caps & ADVERTISED_10000baseT_Full)
		v |= FW_PORT_CAP_SPEED_10G;
	return v;
}

static int get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	const struct port_info *p = netdev_priv(dev);

	if (p->port_type == FW_PORT_TYPE_BT_SGMII ||
	    p->port_type == FW_PORT_TYPE_BT_XFI ||
	    p->port_type == FW_PORT_TYPE_BT_XAUI)
		cmd->port = PORT_TP;
	else if (p->port_type == FW_PORT_TYPE_FIBER_XFI ||
		 p->port_type == FW_PORT_TYPE_FIBER_XAUI)
		cmd->port = PORT_FIBRE;
	else if (p->port_type == FW_PORT_TYPE_SFP) {
		if (p->mod_type == FW_PORT_MOD_TYPE_TWINAX_PASSIVE ||
		    p->mod_type == FW_PORT_MOD_TYPE_TWINAX_ACTIVE)
			cmd->port = PORT_DA;
		else
			cmd->port = PORT_FIBRE;
	} else
		cmd->port = PORT_OTHER;

	if (p->mdio_addr >= 0) {
		cmd->phy_address = p->mdio_addr;
		cmd->transceiver = XCVR_EXTERNAL;
		cmd->mdio_support = p->port_type == FW_PORT_TYPE_BT_SGMII ?
			MDIO_SUPPORTS_C22 : MDIO_SUPPORTS_C45;
	} else {
		cmd->phy_address = 0;  /* not really, but no better option */
		cmd->transceiver = XCVR_INTERNAL;
		cmd->mdio_support = 0;
	}

	cmd->supported = from_fw_linkcaps(p->port_type, p->link_cfg.supported);
	cmd->advertising = from_fw_linkcaps(p->port_type,
					    p->link_cfg.advertising);
	cmd->speed = netif_carrier_ok(dev) ? p->link_cfg.speed : 0;
	cmd->duplex = DUPLEX_FULL;
	cmd->autoneg = p->link_cfg.autoneg;
	cmd->maxtxpkt = 0;
	cmd->maxrxpkt = 0;
	return 0;
}

static unsigned int speed_to_caps(int speed)
{
	if (speed == SPEED_100)
		return FW_PORT_CAP_SPEED_100M;
	if (speed == SPEED_1000)
		return FW_PORT_CAP_SPEED_1G;
	if (speed == SPEED_10000)
		return FW_PORT_CAP_SPEED_10G;
	return 0;
}

static int set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	unsigned int cap;
	struct port_info *p = netdev_priv(dev);
	struct link_config *lc = &p->link_cfg;

	if (cmd->duplex != DUPLEX_FULL)     /* only full-duplex supported */
		return -EINVAL;

	if (!(lc->supported & FW_PORT_CAP_ANEG)) {
		/*
		 * PHY offers a single speed.  See if that's what's
		 * being requested.
		 */
		if (cmd->autoneg == AUTONEG_DISABLE &&
		    (lc->supported & speed_to_caps(cmd->speed)))
				return 0;
		return -EINVAL;
	}

	if (cmd->autoneg == AUTONEG_DISABLE) {
		cap = speed_to_caps(cmd->speed);

		if (!(lc->supported & cap) || cmd->speed == SPEED_1000 ||
		    cmd->speed == SPEED_10000)
			return -EINVAL;
		lc->requested_speed = cap;
		lc->advertising = 0;
	} else {
		cap = to_fw_linkcaps(cmd->advertising);
		if (!(lc->supported & cap))
			return -EINVAL;
		lc->requested_speed = 0;
		lc->advertising = cap | FW_PORT_CAP_ANEG;
	}
	lc->autoneg = cmd->autoneg;

	if (netif_running(dev))
		return t4_link_start(p->adapter, p->adapter->fn, p->tx_chan,
				     lc);
	return 0;
}

static void get_pauseparam(struct net_device *dev,
			   struct ethtool_pauseparam *epause)
{
	struct port_info *p = netdev_priv(dev);

	epause->autoneg = (p->link_cfg.requested_fc & PAUSE_AUTONEG) != 0;
	epause->rx_pause = (p->link_cfg.fc & PAUSE_RX) != 0;
	epause->tx_pause = (p->link_cfg.fc & PAUSE_TX) != 0;
}

static int set_pauseparam(struct net_device *dev,
			  struct ethtool_pauseparam *epause)
{
	struct port_info *p = netdev_priv(dev);
	struct link_config *lc = &p->link_cfg;

	if (epause->autoneg == AUTONEG_DISABLE)
		lc->requested_fc = 0;
	else if (lc->supported & FW_PORT_CAP_ANEG)
		lc->requested_fc = PAUSE_AUTONEG;
	else
		return -EINVAL;

	if (epause->rx_pause)
		lc->requested_fc |= PAUSE_RX;
	if (epause->tx_pause)
		lc->requested_fc |= PAUSE_TX;
	if (netif_running(dev))
		return t4_link_start(p->adapter, p->adapter->fn, p->tx_chan,
				     lc);
	return 0;
}

static u32 get_rx_csum(struct net_device *dev)
{
	struct port_info *p = netdev_priv(dev);

	return p->rx_offload & RX_CSO;
}

static int set_rx_csum(struct net_device *dev, u32 data)
{
	struct port_info *p = netdev_priv(dev);

	if (data)
		p->rx_offload |= RX_CSO;
	else
		p->rx_offload &= ~RX_CSO;
	return 0;
}

static void get_sge_param(struct net_device *dev, struct ethtool_ringparam *e)
{
	const struct port_info *pi = netdev_priv(dev);
	const struct sge *s = &pi->adapter->sge;

	e->rx_max_pending = MAX_RX_BUFFERS;
	e->rx_mini_max_pending = MAX_RSPQ_ENTRIES;
	e->rx_jumbo_max_pending = 0;
	e->tx_max_pending = MAX_TXQ_ENTRIES;

	e->rx_pending = s->ethrxq[pi->first_qset].fl.size - 8;
	e->rx_mini_pending = s->ethrxq[pi->first_qset].rspq.size;
	e->rx_jumbo_pending = 0;
	e->tx_pending = s->ethtxq[pi->first_qset].q.size;
}

static int set_sge_param(struct net_device *dev, struct ethtool_ringparam *e)
{
	int i;
	const struct port_info *pi = netdev_priv(dev);
	struct adapter *adapter = pi->adapter;
	struct sge *s = &adapter->sge;

	if (e->rx_pending > MAX_RX_BUFFERS || e->rx_jumbo_pending ||
	    e->tx_pending > MAX_TXQ_ENTRIES ||
	    e->rx_mini_pending > MAX_RSPQ_ENTRIES ||
	    e->rx_mini_pending < MIN_RSPQ_ENTRIES ||
	    e->rx_pending < MIN_FL_ENTRIES || e->tx_pending < MIN_TXQ_ENTRIES)
		return -EINVAL;

	if (adapter->flags & FULL_INIT_DONE)
		return -EBUSY;

	for (i = 0; i < pi->nqsets; ++i) {
		s->ethtxq[pi->first_qset + i].q.size = e->tx_pending;
		s->ethrxq[pi->first_qset + i].fl.size = e->rx_pending + 8;
		s->ethrxq[pi->first_qset + i].rspq.size = e->rx_mini_pending;
	}
	return 0;
}

static int closest_timer(const struct sge *s, int time)
{
	int i, delta, match = 0, min_delta = INT_MAX;

	for (i = 0; i < ARRAY_SIZE(s->timer_val); i++) {
		delta = time - s->timer_val[i];
		if (delta < 0)
			delta = -delta;
		if (delta < min_delta) {
			min_delta = delta;
			match = i;
		}
	}
	return match;
}

static int closest_thres(const struct sge *s, int thres)
{
	int i, delta, match = 0, min_delta = INT_MAX;

	for (i = 0; i < ARRAY_SIZE(s->counter_val); i++) {
		delta = thres - s->counter_val[i];
		if (delta < 0)
			delta = -delta;
		if (delta < min_delta) {
			min_delta = delta;
			match = i;
		}
	}
	return match;
}

/*
 * Return a queue's interrupt hold-off time in us.  0 means no timer.
 */
static unsigned int qtimer_val(const struct adapter *adap,
			       const struct sge_rspq *q)
{
	unsigned int idx = q->intr_params >> 1;

	return idx < SGE_NTIMERS ? adap->sge.timer_val[idx] : 0;
}

/**
 *	set_rxq_intr_params - set a queue's interrupt holdoff parameters
 *	@adap: the adapter
 *	@q: the Rx queue
 *	@us: the hold-off time in us, or 0 to disable timer
 *	@cnt: the hold-off packet count, or 0 to disable counter
 *
 *	Sets an Rx queue's interrupt hold-off time and packet count.  At least
 *	one of the two needs to be enabled for the queue to generate interrupts.
 */
static int set_rxq_intr_params(struct adapter *adap, struct sge_rspq *q,
			       unsigned int us, unsigned int cnt)
{
	if ((us | cnt) == 0)
		cnt = 1;

	if (cnt) {
		int err;
		u32 v, new_idx;

		new_idx = closest_thres(&adap->sge, cnt);
		if (q->desc && q->pktcnt_idx != new_idx) {
			/* the queue has already been created, update it */
			v = FW_PARAMS_MNEM(FW_PARAMS_MNEM_DMAQ) |
			    FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DMAQ_IQ_INTCNTTHRESH) |
			    FW_PARAMS_PARAM_YZ(q->cntxt_id);
			err = t4_set_params(adap, adap->fn, adap->fn, 0, 1, &v,
					    &new_idx);
			if (err)
				return err;
		}
		q->pktcnt_idx = new_idx;
	}

	us = us == 0 ? 6 : closest_timer(&adap->sge, us);
	q->intr_params = QINTR_TIMER_IDX(us) | (cnt > 0 ? QINTR_CNT_EN : 0);
	return 0;
}

static int set_coalesce(struct net_device *dev, struct ethtool_coalesce *c)
{
	const struct port_info *pi = netdev_priv(dev);
	struct adapter *adap = pi->adapter;

	return set_rxq_intr_params(adap, &adap->sge.ethrxq[pi->first_qset].rspq,
			c->rx_coalesce_usecs, c->rx_max_coalesced_frames);
}

static int get_coalesce(struct net_device *dev, struct ethtool_coalesce *c)
{
	const struct port_info *pi = netdev_priv(dev);
	const struct adapter *adap = pi->adapter;
	const struct sge_rspq *rq = &adap->sge.ethrxq[pi->first_qset].rspq;

	c->rx_coalesce_usecs = qtimer_val(adap, rq);
	c->rx_max_coalesced_frames = (rq->intr_params & QINTR_CNT_EN) ?
		adap->sge.counter_val[rq->pktcnt_idx] : 0;
	return 0;
}

/*
 * Translate a physical EEPROM address to virtual.  The first 1K is accessed
 * through virtual addresses starting at 31K, the rest is accessed through
 * virtual addresses starting at 0.  This mapping is correct only for PF0.
 */
static int eeprom_ptov(unsigned int phys_addr)
{
	if (phys_addr < 1024)
		return phys_addr + (31 << 10);
	if (phys_addr < EEPROMSIZE)
		return phys_addr - 1024;
	return -EINVAL;
}

/*
 * The next two routines implement eeprom read/write from physical addresses.
 * The physical->virtual translation is correct only for PF0.
 */
static int eeprom_rd_phys(struct adapter *adap, unsigned int phys_addr, u32 *v)
{
	int vaddr = eeprom_ptov(phys_addr);

	if (vaddr >= 0)
		vaddr = pci_read_vpd(adap->pdev, vaddr, sizeof(u32), v);
	return vaddr < 0 ? vaddr : 0;
}

static int eeprom_wr_phys(struct adapter *adap, unsigned int phys_addr, u32 v)
{
	int vaddr = eeprom_ptov(phys_addr);

	if (vaddr >= 0)
		vaddr = pci_write_vpd(adap->pdev, vaddr, sizeof(u32), &v);
	return vaddr < 0 ? vaddr : 0;
}

#define EEPROM_MAGIC 0x38E2F10C

static int get_eeprom(struct net_device *dev, struct ethtool_eeprom *e,
		      u8 *data)
{
	int i, err = 0;
	struct adapter *adapter = netdev2adap(dev);

	u8 *buf = kmalloc(EEPROMSIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	e->magic = EEPROM_MAGIC;
	for (i = e->offset & ~3; !err && i < e->offset + e->len; i += 4)
		err = eeprom_rd_phys(adapter, i, (u32 *)&buf[i]);

	if (!err)
		memcpy(data, buf + e->offset, e->len);
	kfree(buf);
	return err;
}

static int set_eeprom(struct net_device *dev, struct ethtool_eeprom *eeprom,
		      u8 *data)
{
	u8 *buf;
	int err = 0;
	u32 aligned_offset, aligned_len, *p;
	struct adapter *adapter = netdev2adap(dev);

	if (eeprom->magic != EEPROM_MAGIC)
		return -EINVAL;

	aligned_offset = eeprom->offset & ~3;
	aligned_len = (eeprom->len + (eeprom->offset & 3) + 3) & ~3;

	if (aligned_offset != eeprom->offset || aligned_len != eeprom->len) {
		/*
		 * RMW possibly needed for first or last words.
		 */
		buf = kmalloc(aligned_len, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;
		err = eeprom_rd_phys(adapter, aligned_offset, (u32 *)buf);
		if (!err && aligned_len > 4)
			err = eeprom_rd_phys(adapter,
					     aligned_offset + aligned_len - 4,
					     (u32 *)&buf[aligned_len - 4]);
		if (err)
			goto out;
		memcpy(buf + (eeprom->offset & 3), data, eeprom->len);
	} else
		buf = data;

	err = t4_seeprom_wp(adapter, false);
	if (err)
		goto out;

	for (p = (u32 *)buf; !err && aligned_len; aligned_len -= 4, p++) {
		err = eeprom_wr_phys(adapter, aligned_offset, *p);
		aligned_offset += 4;
	}

	if (!err)
		err = t4_seeprom_wp(adapter, true);
out:
	if (buf != data)
		kfree(buf);
	return err;
}

static int set_flash(struct net_device *netdev, struct ethtool_flash *ef)
{
	int ret;
	const struct firmware *fw;
	struct adapter *adap = netdev2adap(netdev);

	ef->data[sizeof(ef->data) - 1] = '\0';
	ret = request_firmware(&fw, ef->data, adap->pdev_dev);
	if (ret < 0)
		return ret;

	ret = t4_load_fw(adap, fw->data, fw->size);
	release_firmware(fw);
	if (!ret)
		dev_info(adap->pdev_dev, "loaded firmware %s\n", ef->data);
	return ret;
}

#define WOL_SUPPORTED (WAKE_BCAST | WAKE_MAGIC)
#define BCAST_CRC 0xa0ccc1a6

static void get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	wol->supported = WAKE_BCAST | WAKE_MAGIC;
	wol->wolopts = netdev2adap(dev)->wol;
	memset(&wol->sopass, 0, sizeof(wol->sopass));
}

static int set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	int err = 0;
	struct port_info *pi = netdev_priv(dev);

	if (wol->wolopts & ~WOL_SUPPORTED)
		return -EINVAL;
	t4_wol_magic_enable(pi->adapter, pi->tx_chan,
			    (wol->wolopts & WAKE_MAGIC) ? dev->dev_addr : NULL);
	if (wol->wolopts & WAKE_BCAST) {
		err = t4_wol_pat_enable(pi->adapter, pi->tx_chan, 0xfe, ~0ULL,
					~0ULL, 0, false);
		if (!err)
			err = t4_wol_pat_enable(pi->adapter, pi->tx_chan, 1,
						~6ULL, ~0ULL, BCAST_CRC, true);
	} else
		t4_wol_pat_enable(pi->adapter, pi->tx_chan, 0, 0, 0, 0, false);
	return err;
}

#define TSO_FLAGS (NETIF_F_TSO | NETIF_F_TSO6 | NETIF_F_TSO_ECN)

static int set_tso(struct net_device *dev, u32 value)
{
	if (value)
		dev->features |= TSO_FLAGS;
	else
		dev->features &= ~TSO_FLAGS;
	return 0;
}

static int set_flags(struct net_device *dev, u32 flags)
{
	return ethtool_op_set_flags(dev, flags, ETH_FLAG_RXHASH);
}

static int get_rss_table(struct net_device *dev, struct ethtool_rxfh_indir *p)
{
	const struct port_info *pi = netdev_priv(dev);
	unsigned int n = min_t(unsigned int, p->size, pi->rss_size);

	p->size = pi->rss_size;
	while (n--)
		p->ring_index[n] = pi->rss[n];
	return 0;
}

static int set_rss_table(struct net_device *dev,
			 const struct ethtool_rxfh_indir *p)
{
	unsigned int i;
	struct port_info *pi = netdev_priv(dev);

	if (p->size != pi->rss_size)
		return -EINVAL;
	for (i = 0; i < p->size; i++)
		if (p->ring_index[i] >= pi->nqsets)
			return -EINVAL;
	for (i = 0; i < p->size; i++)
		pi->rss[i] = p->ring_index[i];
	if (pi->adapter->flags & FULL_INIT_DONE)
		return write_rss(pi, pi->rss);
	return 0;
}

static int get_rxnfc(struct net_device *dev, struct ethtool_rxnfc *info,
		     void *rules)
{
	const struct port_info *pi = netdev_priv(dev);

	switch (info->cmd) {
	case ETHTOOL_GRXFH: {
		unsigned int v = pi->rss_mode;

		info->data = 0;
		switch (info->flow_type) {
		case TCP_V4_FLOW:
			if (v & FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN)
				info->data = RXH_IP_SRC | RXH_IP_DST |
					     RXH_L4_B_0_1 | RXH_L4_B_2_3;
			else if (v & FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN)
				info->data = RXH_IP_SRC | RXH_IP_DST;
			break;
		case UDP_V4_FLOW:
			if ((v & FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN) &&
			    (v & FW_RSS_VI_CONFIG_CMD_UDPEN))
				info->data = RXH_IP_SRC | RXH_IP_DST |
					     RXH_L4_B_0_1 | RXH_L4_B_2_3;
			else if (v & FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN)
				info->data = RXH_IP_SRC | RXH_IP_DST;
			break;
		case SCTP_V4_FLOW:
		case AH_ESP_V4_FLOW:
		case IPV4_FLOW:
			if (v & FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN)
				info->data = RXH_IP_SRC | RXH_IP_DST;
			break;
		case TCP_V6_FLOW:
			if (v & FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN)
				info->data = RXH_IP_SRC | RXH_IP_DST |
					     RXH_L4_B_0_1 | RXH_L4_B_2_3;
			else if (v & FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN)
				info->data = RXH_IP_SRC | RXH_IP_DST;
			break;
		case UDP_V6_FLOW:
			if ((v & FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN) &&
			    (v & FW_RSS_VI_CONFIG_CMD_UDPEN))
				info->data = RXH_IP_SRC | RXH_IP_DST |
					     RXH_L4_B_0_1 | RXH_L4_B_2_3;
			else if (v & FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN)
				info->data = RXH_IP_SRC | RXH_IP_DST;
			break;
		case SCTP_V6_FLOW:
		case AH_ESP_V6_FLOW:
		case IPV6_FLOW:
			if (v & FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN)
				info->data = RXH_IP_SRC | RXH_IP_DST;
			break;
		}
		return 0;
	}
	case ETHTOOL_GRXRINGS:
		info->data = pi->nqsets;
		return 0;
	}
	return -EOPNOTSUPP;
}

static struct ethtool_ops cxgb_ethtool_ops = {
	.get_settings      = get_settings,
	.set_settings      = set_settings,
	.get_drvinfo       = get_drvinfo,
	.get_msglevel      = get_msglevel,
	.set_msglevel      = set_msglevel,
	.get_ringparam     = get_sge_param,
	.set_ringparam     = set_sge_param,
	.get_coalesce      = get_coalesce,
	.set_coalesce      = set_coalesce,
	.get_eeprom_len    = get_eeprom_len,
	.get_eeprom        = get_eeprom,
	.set_eeprom        = set_eeprom,
	.get_pauseparam    = get_pauseparam,
	.set_pauseparam    = set_pauseparam,
	.get_rx_csum       = get_rx_csum,
	.set_rx_csum       = set_rx_csum,
	.set_tx_csum       = ethtool_op_set_tx_ipv6_csum,
	.set_sg            = ethtool_op_set_sg,
	.get_link          = ethtool_op_get_link,
	.get_strings       = get_strings,
	.phys_id           = identify_port,
	.nway_reset        = restart_autoneg,
	.get_sset_count    = get_sset_count,
	.get_ethtool_stats = get_stats,
	.get_regs_len      = get_regs_len,
	.get_regs          = get_regs,
	.get_wol           = get_wol,
	.set_wol           = set_wol,
	.set_tso           = set_tso,
	.set_flags         = set_flags,
	.get_rxnfc         = get_rxnfc,
	.get_rxfh_indir    = get_rss_table,
	.set_rxfh_indir    = set_rss_table,
	.flash_device      = set_flash,
};

/*
 * debugfs support
 */

static int mem_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t mem_read(struct file *file, char __user *buf, size_t count,
			loff_t *ppos)
{
	loff_t pos = *ppos;
	loff_t avail = file->f_path.dentry->d_inode->i_size;
	unsigned int mem = (uintptr_t)file->private_data & 3;
	struct adapter *adap = file->private_data - mem;

	if (pos < 0)
		return -EINVAL;
	if (pos >= avail)
		return 0;
	if (count > avail - pos)
		count = avail - pos;

	while (count) {
		size_t len;
		int ret, ofst;
		__be32 data[16];

		if (mem == MEM_MC)
			ret = t4_mc_read(adap, pos, data, NULL);
		else
			ret = t4_edc_read(adap, mem, pos, data, NULL);
		if (ret)
			return ret;

		ofst = pos % sizeof(data);
		len = min(count, sizeof(data) - ofst);
		if (copy_to_user(buf, (u8 *)data + ofst, len))
			return -EFAULT;

		buf += len;
		pos += len;
		count -= len;
	}
	count = pos - *ppos;
	*ppos = pos;
	return count;
}

static const struct file_operations mem_debugfs_fops = {
	.owner   = THIS_MODULE,
	.open    = mem_open,
	.read    = mem_read,
};

static void __devinit add_debugfs_mem(struct adapter *adap, const char *name,
				      unsigned int idx, unsigned int size_mb)
{
	struct dentry *de;

	de = debugfs_create_file(name, S_IRUSR, adap->debugfs_root,
				 (void *)adap + idx, &mem_debugfs_fops);
	if (de && de->d_inode)
		de->d_inode->i_size = size_mb << 20;
}

static int __devinit setup_debugfs(struct adapter *adap)
{
	int i;

	if (IS_ERR_OR_NULL(adap->debugfs_root))
		return -1;

	i = t4_read_reg(adap, MA_TARGET_MEM_ENABLE);
	if (i & EDRAM0_ENABLE)
		add_debugfs_mem(adap, "edc0", MEM_EDC0, 5);
	if (i & EDRAM1_ENABLE)
		add_debugfs_mem(adap, "edc1", MEM_EDC1, 5);
	if (i & EXT_MEM_ENABLE)
		add_debugfs_mem(adap, "mc", MEM_MC,
			EXT_MEM_SIZE_GET(t4_read_reg(adap, MA_EXT_MEMORY_BAR)));
	if (adap->l2t)
		debugfs_create_file("l2t", S_IRUSR, adap->debugfs_root, adap,
				    &t4_l2t_fops);
	return 0;
}

/*
 * upper-layer driver support
 */

/*
 * Allocate an active-open TID and set it to the supplied value.
 */
int cxgb4_alloc_atid(struct tid_info *t, void *data)
{
	int atid = -1;

	spin_lock_bh(&t->atid_lock);
	if (t->afree) {
		union aopen_entry *p = t->afree;

		atid = p - t->atid_tab;
		t->afree = p->next;
		p->data = data;
		t->atids_in_use++;
	}
	spin_unlock_bh(&t->atid_lock);
	return atid;
}
EXPORT_SYMBOL(cxgb4_alloc_atid);

/*
 * Release an active-open TID.
 */
void cxgb4_free_atid(struct tid_info *t, unsigned int atid)
{
	union aopen_entry *p = &t->atid_tab[atid];

	spin_lock_bh(&t->atid_lock);
	p->next = t->afree;
	t->afree = p;
	t->atids_in_use--;
	spin_unlock_bh(&t->atid_lock);
}
EXPORT_SYMBOL(cxgb4_free_atid);

/*
 * Allocate a server TID and set it to the supplied value.
 */
int cxgb4_alloc_stid(struct tid_info *t, int family, void *data)
{
	int stid;

	spin_lock_bh(&t->stid_lock);
	if (family == PF_INET) {
		stid = find_first_zero_bit(t->stid_bmap, t->nstids);
		if (stid < t->nstids)
			__set_bit(stid, t->stid_bmap);
		else
			stid = -1;
	} else {
		stid = bitmap_find_free_region(t->stid_bmap, t->nstids, 2);
		if (stid < 0)
			stid = -1;
	}
	if (stid >= 0) {
		t->stid_tab[stid].data = data;
		stid += t->stid_base;
		t->stids_in_use++;
	}
	spin_unlock_bh(&t->stid_lock);
	return stid;
}
EXPORT_SYMBOL(cxgb4_alloc_stid);

/*
 * Release a server TID.
 */
void cxgb4_free_stid(struct tid_info *t, unsigned int stid, int family)
{
	stid -= t->stid_base;
	spin_lock_bh(&t->stid_lock);
	if (family == PF_INET)
		__clear_bit(stid, t->stid_bmap);
	else
		bitmap_release_region(t->stid_bmap, stid, 2);
	t->stid_tab[stid].data = NULL;
	t->stids_in_use--;
	spin_unlock_bh(&t->stid_lock);
}
EXPORT_SYMBOL(cxgb4_free_stid);

/*
 * Populate a TID_RELEASE WR.  Caller must properly size the skb.
 */
static void mk_tid_release(struct sk_buff *skb, unsigned int chan,
			   unsigned int tid)
{
	struct cpl_tid_release *req;

	set_wr_txq(skb, CPL_PRIORITY_SETUP, chan);
	req = (struct cpl_tid_release *)__skb_put(skb, sizeof(*req));
	INIT_TP_WR(req, tid);
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_TID_RELEASE, tid));
}

/*
 * Queue a TID release request and if necessary schedule a work queue to
 * process it.
 */
void cxgb4_queue_tid_release(struct tid_info *t, unsigned int chan,
			     unsigned int tid)
{
	void **p = &t->tid_tab[tid];
	struct adapter *adap = container_of(t, struct adapter, tids);

	spin_lock_bh(&adap->tid_release_lock);
	*p = adap->tid_release_head;
	/* Low 2 bits encode the Tx channel number */
	adap->tid_release_head = (void **)((uintptr_t)p | chan);
	if (!adap->tid_release_task_busy) {
		adap->tid_release_task_busy = true;
		schedule_work(&adap->tid_release_task);
	}
	spin_unlock_bh(&adap->tid_release_lock);
}
EXPORT_SYMBOL(cxgb4_queue_tid_release);

/*
 * Process the list of pending TID release requests.
 */
static void process_tid_release_list(struct work_struct *work)
{
	struct sk_buff *skb;
	struct adapter *adap;

	adap = container_of(work, struct adapter, tid_release_task);

	spin_lock_bh(&adap->tid_release_lock);
	while (adap->tid_release_head) {
		void **p = adap->tid_release_head;
		unsigned int chan = (uintptr_t)p & 3;
		p = (void *)p - chan;

		adap->tid_release_head = *p;
		*p = NULL;
		spin_unlock_bh(&adap->tid_release_lock);

		while (!(skb = alloc_skb(sizeof(struct cpl_tid_release),
					 GFP_KERNEL)))
			schedule_timeout_uninterruptible(1);

		mk_tid_release(skb, chan, p - adap->tids.tid_tab);
		t4_ofld_send(adap, skb);
		spin_lock_bh(&adap->tid_release_lock);
	}
	adap->tid_release_task_busy = false;
	spin_unlock_bh(&adap->tid_release_lock);
}

/*
 * Release a TID and inform HW.  If we are unable to allocate the release
 * message we defer to a work queue.
 */
void cxgb4_remove_tid(struct tid_info *t, unsigned int chan, unsigned int tid)
{
	void *old;
	struct sk_buff *skb;
	struct adapter *adap = container_of(t, struct adapter, tids);

	old = t->tid_tab[tid];
	skb = alloc_skb(sizeof(struct cpl_tid_release), GFP_ATOMIC);
	if (likely(skb)) {
		t->tid_tab[tid] = NULL;
		mk_tid_release(skb, chan, tid);
		t4_ofld_send(adap, skb);
	} else
		cxgb4_queue_tid_release(t, chan, tid);
	if (old)
		atomic_dec(&t->tids_in_use);
}
EXPORT_SYMBOL(cxgb4_remove_tid);

/*
 * Allocate and initialize the TID tables.  Returns 0 on success.
 */
static int tid_init(struct tid_info *t)
{
	size_t size;
	unsigned int natids = t->natids;

	size = t->ntids * sizeof(*t->tid_tab) + natids * sizeof(*t->atid_tab) +
	       t->nstids * sizeof(*t->stid_tab) +
	       BITS_TO_LONGS(t->nstids) * sizeof(long);
	t->tid_tab = t4_alloc_mem(size);
	if (!t->tid_tab)
		return -ENOMEM;

	t->atid_tab = (union aopen_entry *)&t->tid_tab[t->ntids];
	t->stid_tab = (struct serv_entry *)&t->atid_tab[natids];
	t->stid_bmap = (unsigned long *)&t->stid_tab[t->nstids];
	spin_lock_init(&t->stid_lock);
	spin_lock_init(&t->atid_lock);

	t->stids_in_use = 0;
	t->afree = NULL;
	t->atids_in_use = 0;
	atomic_set(&t->tids_in_use, 0);

	/* Setup the free list for atid_tab and clear the stid bitmap. */
	if (natids) {
		while (--natids)
			t->atid_tab[natids - 1].next = &t->atid_tab[natids];
		t->afree = t->atid_tab;
	}
	bitmap_zero(t->stid_bmap, t->nstids);
	return 0;
}

/**
 *	cxgb4_create_server - create an IP server
 *	@dev: the device
 *	@stid: the server TID
 *	@sip: local IP address to bind server to
 *	@sport: the server's TCP port
 *	@queue: queue to direct messages from this server to
 *
 *	Create an IP server for the given port and address.
 *	Returns <0 on error and one of the %NET_XMIT_* values on success.
 */
int cxgb4_create_server(const struct net_device *dev, unsigned int stid,
			__be32 sip, __be16 sport, unsigned int queue)
{
	unsigned int chan;
	struct sk_buff *skb;
	struct adapter *adap;
	struct cpl_pass_open_req *req;

	skb = alloc_skb(sizeof(*req), GFP_KERNEL);
	if (!skb)
		return -ENOMEM;

	adap = netdev2adap(dev);
	req = (struct cpl_pass_open_req *)__skb_put(skb, sizeof(*req));
	INIT_TP_WR(req, 0);
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_PASS_OPEN_REQ, stid));
	req->local_port = sport;
	req->peer_port = htons(0);
	req->local_ip = sip;
	req->peer_ip = htonl(0);
	chan = netdev2pinfo(adap->sge.ingr_map[queue]->netdev)->tx_chan;
	req->opt0 = cpu_to_be64(TX_CHAN(chan));
	req->opt1 = cpu_to_be64(CONN_POLICY_ASK |
				SYN_RSS_ENABLE | SYN_RSS_QUEUE(queue));
	return t4_mgmt_tx(adap, skb);
}
EXPORT_SYMBOL(cxgb4_create_server);

/**
 *	cxgb4_create_server6 - create an IPv6 server
 *	@dev: the device
 *	@stid: the server TID
 *	@sip: local IPv6 address to bind server to
 *	@sport: the server's TCP port
 *	@queue: queue to direct messages from this server to
 *
 *	Create an IPv6 server for the given port and address.
 *	Returns <0 on error and one of the %NET_XMIT_* values on success.
 */
int cxgb4_create_server6(const struct net_device *dev, unsigned int stid,
			 const struct in6_addr *sip, __be16 sport,
			 unsigned int queue)
{
	unsigned int chan;
	struct sk_buff *skb;
	struct adapter *adap;
	struct cpl_pass_open_req6 *req;

	skb = alloc_skb(sizeof(*req), GFP_KERNEL);
	if (!skb)
		return -ENOMEM;

	adap = netdev2adap(dev);
	req = (struct cpl_pass_open_req6 *)__skb_put(skb, sizeof(*req));
	INIT_TP_WR(req, 0);
	OPCODE_TID(req) = htonl(MK_OPCODE_TID(CPL_PASS_OPEN_REQ6, stid));
	req->local_port = sport;
	req->peer_port = htons(0);
	req->local_ip_hi = *(__be64 *)(sip->s6_addr);
	req->local_ip_lo = *(__be64 *)(sip->s6_addr + 8);
	req->peer_ip_hi = cpu_to_be64(0);
	req->peer_ip_lo = cpu_to_be64(0);
	chan = netdev2pinfo(adap->sge.ingr_map[queue]->netdev)->tx_chan;
	req->opt0 = cpu_to_be64(TX_CHAN(chan));
	req->opt1 = cpu_to_be64(CONN_POLICY_ASK |
				SYN_RSS_ENABLE | SYN_RSS_QUEUE(queue));
	return t4_mgmt_tx(adap, skb);
}
EXPORT_SYMBOL(cxgb4_create_server6);

/**
 *	cxgb4_best_mtu - find the entry in the MTU table closest to an MTU
 *	@mtus: the HW MTU table
 *	@mtu: the target MTU
 *	@idx: index of selected entry in the MTU table
 *
 *	Returns the index and the value in the HW MTU table that is closest to
 *	but does not exceed @mtu, unless @mtu is smaller than any value in the
 *	table, in which case that smallest available value is selected.
 */
unsigned int cxgb4_best_mtu(const unsigned short *mtus, unsigned short mtu,
			    unsigned int *idx)
{
	unsigned int i = 0;

	while (i < NMTUS - 1 && mtus[i + 1] <= mtu)
		++i;
	if (idx)
		*idx = i;
	return mtus[i];
}
EXPORT_SYMBOL(cxgb4_best_mtu);

/**
 *	cxgb4_port_chan - get the HW channel of a port
 *	@dev: the net device for the port
 *
 *	Return the HW Tx channel of the given port.
 */
unsigned int cxgb4_port_chan(const struct net_device *dev)
{
	return netdev2pinfo(dev)->tx_chan;
}
EXPORT_SYMBOL(cxgb4_port_chan);

/**
 *	cxgb4_port_viid - get the VI id of a port
 *	@dev: the net device for the port
 *
 *	Return the VI id of the given port.
 */
unsigned int cxgb4_port_viid(const struct net_device *dev)
{
	return netdev2pinfo(dev)->viid;
}
EXPORT_SYMBOL(cxgb4_port_viid);

/**
 *	cxgb4_port_idx - get the index of a port
 *	@dev: the net device for the port
 *
 *	Return the index of the given port.
 */
unsigned int cxgb4_port_idx(const struct net_device *dev)
{
	return netdev2pinfo(dev)->port_id;
}
EXPORT_SYMBOL(cxgb4_port_idx);

/**
 *	cxgb4_netdev_by_hwid - return the net device of a HW port
 *	@pdev: identifies the adapter
 *	@id: the HW port id
 *
 *	Return the net device associated with the interface with the given HW
 *	id.
 */
struct net_device *cxgb4_netdev_by_hwid(struct pci_dev *pdev, unsigned int id)
{
	const struct adapter *adap = pci_get_drvdata(pdev);

	if (!adap || id >= NCHAN)
		return NULL;
	id = adap->chan_map[id];
	return id < MAX_NPORTS ? adap->port[id] : NULL;
}
EXPORT_SYMBOL(cxgb4_netdev_by_hwid);

void cxgb4_get_tcp_stats(struct pci_dev *pdev, struct tp_tcp_stats *v4,
			 struct tp_tcp_stats *v6)
{
	struct adapter *adap = pci_get_drvdata(pdev);

	spin_lock(&adap->stats_lock);
	t4_tp_get_tcp_stats(adap, v4, v6);
	spin_unlock(&adap->stats_lock);
}
EXPORT_SYMBOL(cxgb4_get_tcp_stats);

void cxgb4_iscsi_init(struct net_device *dev, unsigned int tag_mask,
		      const unsigned int *pgsz_order)
{
	struct adapter *adap = netdev2adap(dev);

	t4_write_reg(adap, ULP_RX_ISCSI_TAGMASK, tag_mask);
	t4_write_reg(adap, ULP_RX_ISCSI_PSZ, HPZ0(pgsz_order[0]) |
		     HPZ1(pgsz_order[1]) | HPZ2(pgsz_order[2]) |
		     HPZ3(pgsz_order[3]));
}
EXPORT_SYMBOL(cxgb4_iscsi_init);

static struct pci_driver cxgb4_driver;

static void check_neigh_update(struct neighbour *neigh)
{
	const struct device *parent;
	const struct net_device *netdev = neigh->dev;

	if (netdev->priv_flags & IFF_802_1Q_VLAN)
		netdev = vlan_dev_real_dev(netdev);
	parent = netdev->dev.parent;
	if (parent && parent->driver == &cxgb4_driver.driver)
		t4_l2t_update(dev_get_drvdata(parent), neigh);
}

static int netevent_cb(struct notifier_block *nb, unsigned long event,
		       void *data)
{
	switch (event) {
	case NETEVENT_NEIGH_UPDATE:
		check_neigh_update(data);
		break;
	case NETEVENT_PMTU_UPDATE:
	case NETEVENT_REDIRECT:
	default:
		break;
	}
	return 0;
}

static bool netevent_registered;
static struct notifier_block cxgb4_netevent_nb = {
	.notifier_call = netevent_cb
};

static void uld_attach(struct adapter *adap, unsigned int uld)
{
	void *handle;
	struct cxgb4_lld_info lli;

	lli.pdev = adap->pdev;
	lli.l2t = adap->l2t;
	lli.tids = &adap->tids;
	lli.ports = adap->port;
	lli.vr = &adap->vres;
	lli.mtus = adap->params.mtus;
	if (uld == CXGB4_ULD_RDMA) {
		lli.rxq_ids = adap->sge.rdma_rxq;
		lli.nrxq = adap->sge.rdmaqs;
	} else if (uld == CXGB4_ULD_ISCSI) {
		lli.rxq_ids = adap->sge.ofld_rxq;
		lli.nrxq = adap->sge.ofldqsets;
	}
	lli.ntxq = adap->sge.ofldqsets;
	lli.nchan = adap->params.nports;
	lli.nports = adap->params.nports;
	lli.wr_cred = adap->params.ofldq_wr_cred;
	lli.adapter_type = adap->params.rev;
	lli.iscsi_iolen = MAXRXDATA_GET(t4_read_reg(adap, TP_PARA_REG2));
	lli.udb_density = 1 << QUEUESPERPAGEPF0_GET(
			t4_read_reg(adap, SGE_EGRESS_QUEUES_PER_PAGE_PF) >>
			(adap->fn * 4));
	lli.ucq_density = 1 << QUEUESPERPAGEPF0_GET(
			t4_read_reg(adap, SGE_INGRESS_QUEUES_PER_PAGE_PF) >>
			(adap->fn * 4));
	lli.gts_reg = adap->regs + MYPF_REG(SGE_PF_GTS);
	lli.db_reg = adap->regs + MYPF_REG(SGE_PF_KDOORBELL);
	lli.fw_vers = adap->params.fw_vers;

	handle = ulds[uld].add(&lli);
	if (IS_ERR(handle)) {
		dev_warn(adap->pdev_dev,
			 "could not attach to the %s driver, error %ld\n",
			 uld_str[uld], PTR_ERR(handle));
		return;
	}

	adap->uld_handle[uld] = handle;

	if (!netevent_registered) {
		register_netevent_notifier(&cxgb4_netevent_nb);
		netevent_registered = true;
	}

	if (adap->flags & FULL_INIT_DONE)
		ulds[uld].state_change(handle, CXGB4_STATE_UP);
}

static void attach_ulds(struct adapter *adap)
{
	unsigned int i;

	mutex_lock(&uld_mutex);
	list_add_tail(&adap->list_node, &adapter_list);
	for (i = 0; i < CXGB4_ULD_MAX; i++)
		if (ulds[i].add)
			uld_attach(adap, i);
	mutex_unlock(&uld_mutex);
}

static void detach_ulds(struct adapter *adap)
{
	unsigned int i;

	mutex_lock(&uld_mutex);
	list_del(&adap->list_node);
	for (i = 0; i < CXGB4_ULD_MAX; i++)
		if (adap->uld_handle[i]) {
			ulds[i].state_change(adap->uld_handle[i],
					     CXGB4_STATE_DETACH);
			adap->uld_handle[i] = NULL;
		}
	if (netevent_registered && list_empty(&adapter_list)) {
		unregister_netevent_notifier(&cxgb4_netevent_nb);
		netevent_registered = false;
	}
	mutex_unlock(&uld_mutex);
}

static void notify_ulds(struct adapter *adap, enum cxgb4_state new_state)
{
	unsigned int i;

	mutex_lock(&uld_mutex);
	for (i = 0; i < CXGB4_ULD_MAX; i++)
		if (adap->uld_handle[i])
			ulds[i].state_change(adap->uld_handle[i], new_state);
	mutex_unlock(&uld_mutex);
}

/**
 *	cxgb4_register_uld - register an upper-layer driver
 *	@type: the ULD type
 *	@p: the ULD methods
 *
 *	Registers an upper-layer driver with this driver and notifies the ULD
 *	about any presently available devices that support its type.  Returns
 *	%-EBUSY if a ULD of the same type is already registered.
 */
int cxgb4_register_uld(enum cxgb4_uld type, const struct cxgb4_uld_info *p)
{
	int ret = 0;
	struct adapter *adap;

	if (type >= CXGB4_ULD_MAX)
		return -EINVAL;
	mutex_lock(&uld_mutex);
	if (ulds[type].add) {
		ret = -EBUSY;
		goto out;
	}
	ulds[type] = *p;
	list_for_each_entry(adap, &adapter_list, list_node)
		uld_attach(adap, type);
out:	mutex_unlock(&uld_mutex);
	return ret;
}
EXPORT_SYMBOL(cxgb4_register_uld);

/**
 *	cxgb4_unregister_uld - unregister an upper-layer driver
 *	@type: the ULD type
 *
 *	Unregisters an existing upper-layer driver.
 */
int cxgb4_unregister_uld(enum cxgb4_uld type)
{
	struct adapter *adap;

	if (type >= CXGB4_ULD_MAX)
		return -EINVAL;
	mutex_lock(&uld_mutex);
	list_for_each_entry(adap, &adapter_list, list_node)
		adap->uld_handle[type] = NULL;
	ulds[type].add = NULL;
	mutex_unlock(&uld_mutex);
	return 0;
}
EXPORT_SYMBOL(cxgb4_unregister_uld);

/**
 *	cxgb_up - enable the adapter
 *	@adap: adapter being enabled
 *
 *	Called when the first port is enabled, this function performs the
 *	actions necessary to make an adapter operational, such as completing
 *	the initialization of HW modules, and enabling interrupts.
 *
 *	Must be called with the rtnl lock held.
 */
static int cxgb_up(struct adapter *adap)
{
	int err;

	err = setup_sge_queues(adap);
	if (err)
		goto out;
	err = setup_rss(adap);
	if (err)
		goto freeq;

	if (adap->flags & USING_MSIX) {
		name_msix_vecs(adap);
		err = request_irq(adap->msix_info[0].vec, t4_nondata_intr, 0,
				  adap->msix_info[0].desc, adap);
		if (err)
			goto irq_err;

		err = request_msix_queue_irqs(adap);
		if (err) {
			free_irq(adap->msix_info[0].vec, adap);
			goto irq_err;
		}
	} else {
		err = request_irq(adap->pdev->irq, t4_intr_handler(adap),
				  (adap->flags & USING_MSI) ? 0 : IRQF_SHARED,
				  adap->name, adap);
		if (err)
			goto irq_err;
	}
	enable_rx(adap);
	t4_sge_start(adap);
	t4_intr_enable(adap);
	adap->flags |= FULL_INIT_DONE;
	notify_ulds(adap, CXGB4_STATE_UP);
 out:
	return err;
 irq_err:
	dev_err(adap->pdev_dev, "request_irq failed, err %d\n", err);
 freeq:
	t4_free_sge_resources(adap);
	goto out;
}

static void cxgb_down(struct adapter *adapter)
{
	t4_intr_disable(adapter);
	cancel_work_sync(&adapter->tid_release_task);
	adapter->tid_release_task_busy = false;
	adapter->tid_release_head = NULL;

	if (adapter->flags & USING_MSIX) {
		free_msix_queue_irqs(adapter);
		free_irq(adapter->msix_info[0].vec, adapter);
	} else
		free_irq(adapter->pdev->irq, adapter);
	quiesce_rx(adapter);
	t4_sge_stop(adapter);
	t4_free_sge_resources(adapter);
	adapter->flags &= ~FULL_INIT_DONE;
}

/*
 * net_device operations
 */
static int cxgb_open(struct net_device *dev)
{
	int err;
	struct port_info *pi = netdev_priv(dev);
	struct adapter *adapter = pi->adapter;

	if (!(adapter->flags & FULL_INIT_DONE)) {
		err = cxgb_up(adapter);
		if (err < 0)
			return err;
	}

	dev->real_num_tx_queues = pi->nqsets;
	err = link_start(dev);
	if (!err)
		netif_tx_start_all_queues(dev);
	return err;
}

static int cxgb_close(struct net_device *dev)
{
	struct port_info *pi = netdev_priv(dev);
	struct adapter *adapter = pi->adapter;

	netif_tx_stop_all_queues(dev);
	netif_carrier_off(dev);
	return t4_enable_vi(adapter, adapter->fn, pi->viid, false, false);
}

static struct rtnl_link_stats64 *cxgb_get_stats(struct net_device *dev,
						struct rtnl_link_stats64 *ns)
{
	struct port_stats stats;
	struct port_info *p = netdev_priv(dev);
	struct adapter *adapter = p->adapter;

	spin_lock(&adapter->stats_lock);
	t4_get_port_stats(adapter, p->tx_chan, &stats);
	spin_unlock(&adapter->stats_lock);

	ns->tx_bytes   = stats.tx_octets;
	ns->tx_packets = stats.tx_frames;
	ns->rx_bytes   = stats.rx_octets;
	ns->rx_packets = stats.rx_frames;
	ns->multicast  = stats.rx_mcast_frames;

	/* detailed rx_errors */
	ns->rx_length_errors = stats.rx_jabber + stats.rx_too_long +
			       stats.rx_runt;
	ns->rx_over_errors   = 0;
	ns->rx_crc_errors    = stats.rx_fcs_err;
	ns->rx_frame_errors  = stats.rx_symbol_err;
	ns->rx_fifo_errors   = stats.rx_ovflow0 + stats.rx_ovflow1 +
			       stats.rx_ovflow2 + stats.rx_ovflow3 +
			       stats.rx_trunc0 + stats.rx_trunc1 +
			       stats.rx_trunc2 + stats.rx_trunc3;
	ns->rx_missed_errors = 0;

	/* detailed tx_errors */
	ns->tx_aborted_errors   = 0;
	ns->tx_carrier_errors   = 0;
	ns->tx_fifo_errors      = 0;
	ns->tx_heartbeat_errors = 0;
	ns->tx_window_errors    = 0;

	ns->tx_errors = stats.tx_error_frames;
	ns->rx_errors = stats.rx_symbol_err + stats.rx_fcs_err +
		ns->rx_length_errors + stats.rx_len_err + ns->rx_fifo_errors;
	return ns;
}

static int cxgb_ioctl(struct net_device *dev, struct ifreq *req, int cmd)
{
	unsigned int mbox;
	int ret = 0, prtad, devad;
	struct port_info *pi = netdev_priv(dev);
	struct mii_ioctl_data *data = (struct mii_ioctl_data *)&req->ifr_data;

	switch (cmd) {
	case SIOCGMIIPHY:
		if (pi->mdio_addr < 0)
			return -EOPNOTSUPP;
		data->phy_id = pi->mdio_addr;
		break;
	case SIOCGMIIREG:
	case SIOCSMIIREG:
		if (mdio_phy_id_is_c45(data->phy_id)) {
			prtad = mdio_phy_id_prtad(data->phy_id);
			devad = mdio_phy_id_devad(data->phy_id);
		} else if (data->phy_id < 32) {
			prtad = data->phy_id;
			devad = 0;
			data->reg_num &= 0x1f;
		} else
			return -EINVAL;

		mbox = pi->adapter->fn;
		if (cmd == SIOCGMIIREG)
			ret = t4_mdio_rd(pi->adapter, mbox, prtad, devad,
					 data->reg_num, &data->val_out);
		else
			ret = t4_mdio_wr(pi->adapter, mbox, prtad, devad,
					 data->reg_num, data->val_in);
		break;
	default:
		return -EOPNOTSUPP;
	}
	return ret;
}

static void cxgb_set_rxmode(struct net_device *dev)
{
	/* unfortunately we can't return errors to the stack */
	set_rxmode(dev, -1, false);
}

static int cxgb_change_mtu(struct net_device *dev, int new_mtu)
{
	int ret;
	struct port_info *pi = netdev_priv(dev);

	if (new_mtu < 81 || new_mtu > MAX_MTU)         /* accommodate SACK */
		return -EINVAL;
	ret = t4_set_rxmode(pi->adapter, pi->adapter->fn, pi->viid, new_mtu, -1,
			    -1, -1, -1, true);
	if (!ret)
		dev->mtu = new_mtu;
	return ret;
}

static int cxgb_set_mac_addr(struct net_device *dev, void *p)
{
	int ret;
	struct sockaddr *addr = p;
	struct port_info *pi = netdev_priv(dev);

	if (!is_valid_ether_addr(addr->sa_data))
		return -EINVAL;

	ret = t4_change_mac(pi->adapter, pi->adapter->fn, pi->viid,
			    pi->xact_addr_filt, addr->sa_data, true, true);
	if (ret < 0)
		return ret;

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);
	pi->xact_addr_filt = ret;
	return 0;
}

static void vlan_rx_register(struct net_device *dev, struct vlan_group *grp)
{
	struct port_info *pi = netdev_priv(dev);

	pi->vlan_grp = grp;
	t4_set_rxmode(pi->adapter, pi->adapter->fn, pi->viid, -1, -1, -1, -1,
		      grp != NULL, true);
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void cxgb_netpoll(struct net_device *dev)
{
	struct port_info *pi = netdev_priv(dev);
	struct adapter *adap = pi->adapter;

	if (adap->flags & USING_MSIX) {
		int i;
		struct sge_eth_rxq *rx = &adap->sge.ethrxq[pi->first_qset];

		for (i = pi->nqsets; i; i--, rx++)
			t4_sge_intr_msix(0, &rx->rspq);
	} else
		t4_intr_handler(adap)(0, adap);
}
#endif

static const struct net_device_ops cxgb4_netdev_ops = {
	.ndo_open             = cxgb_open,
	.ndo_stop             = cxgb_close,
	.ndo_start_xmit       = t4_eth_xmit,
	.ndo_get_stats64      = cxgb_get_stats,
	.ndo_set_rx_mode      = cxgb_set_rxmode,
	.ndo_set_mac_address  = cxgb_set_mac_addr,
	.ndo_validate_addr    = eth_validate_addr,
	.ndo_do_ioctl         = cxgb_ioctl,
	.ndo_change_mtu       = cxgb_change_mtu,
	.ndo_vlan_rx_register = vlan_rx_register,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller  = cxgb_netpoll,
#endif
};

void t4_fatal_err(struct adapter *adap)
{
	t4_set_reg_field(adap, SGE_CONTROL, GLOBALENABLE, 0);
	t4_intr_disable(adap);
	dev_alert(adap->pdev_dev, "encountered fatal error, adapter stopped\n");
}

static void setup_memwin(struct adapter *adap)
{
	u32 bar0;

	bar0 = pci_resource_start(adap->pdev, 0);  /* truncation intentional */
	t4_write_reg(adap, PCIE_MEM_ACCESS_REG(PCIE_MEM_ACCESS_BASE_WIN, 0),
		     (bar0 + MEMWIN0_BASE) | BIR(0) |
		     WINDOW(ilog2(MEMWIN0_APERTURE) - 10));
	t4_write_reg(adap, PCIE_MEM_ACCESS_REG(PCIE_MEM_ACCESS_BASE_WIN, 1),
		     (bar0 + MEMWIN1_BASE) | BIR(0) |
		     WINDOW(ilog2(MEMWIN1_APERTURE) - 10));
	t4_write_reg(adap, PCIE_MEM_ACCESS_REG(PCIE_MEM_ACCESS_BASE_WIN, 2),
		     (bar0 + MEMWIN2_BASE) | BIR(0) |
		     WINDOW(ilog2(MEMWIN2_APERTURE) - 10));
	if (adap->vres.ocq.size) {
		unsigned int start, sz_kb;

		start = pci_resource_start(adap->pdev, 2) +
			OCQ_WIN_OFFSET(adap->pdev, &adap->vres);
		sz_kb = roundup_pow_of_two(adap->vres.ocq.size) >> 10;
		t4_write_reg(adap,
			     PCIE_MEM_ACCESS_REG(PCIE_MEM_ACCESS_BASE_WIN, 3),
			     start | BIR(1) | WINDOW(ilog2(sz_kb)));
		t4_write_reg(adap,
			     PCIE_MEM_ACCESS_REG(PCIE_MEM_ACCESS_OFFSET, 3),
			     adap->vres.ocq.start);
		t4_read_reg(adap,
			    PCIE_MEM_ACCESS_REG(PCIE_MEM_ACCESS_OFFSET, 3));
	}
}

static int adap_init1(struct adapter *adap, struct fw_caps_config_cmd *c)
{
	u32 v;
	int ret;

	/* get device capabilities */
	memset(c, 0, sizeof(*c));
	c->op_to_write = htonl(FW_CMD_OP(FW_CAPS_CONFIG_CMD) |
			       FW_CMD_REQUEST | FW_CMD_READ);
	c->retval_len16 = htonl(FW_LEN16(*c));
	ret = t4_wr_mbox(adap, adap->fn, c, sizeof(*c), c);
	if (ret < 0)
		return ret;

	/* select capabilities we'll be using */
	if (c->niccaps & htons(FW_CAPS_CONFIG_NIC_VM)) {
		if (!vf_acls)
			c->niccaps ^= htons(FW_CAPS_CONFIG_NIC_VM);
		else
			c->niccaps = htons(FW_CAPS_CONFIG_NIC_VM);
	} else if (vf_acls) {
		dev_err(adap->pdev_dev, "virtualization ACLs not supported");
		return ret;
	}
	c->op_to_write = htonl(FW_CMD_OP(FW_CAPS_CONFIG_CMD) |
			       FW_CMD_REQUEST | FW_CMD_WRITE);
	ret = t4_wr_mbox(adap, adap->fn, c, sizeof(*c), NULL);
	if (ret < 0)
		return ret;

	ret = t4_config_glbl_rss(adap, adap->fn,
				 FW_RSS_GLB_CONFIG_CMD_MODE_BASICVIRTUAL,
				 FW_RSS_GLB_CONFIG_CMD_TNLMAPEN |
				 FW_RSS_GLB_CONFIG_CMD_TNLALLLKP);
	if (ret < 0)
		return ret;

	ret = t4_cfg_pfvf(adap, adap->fn, adap->fn, 0, MAX_EGRQ, 64, MAX_INGQ,
			  0, 0, 4, 0xf, 0xf, 16, FW_CMD_CAP_PF, FW_CMD_CAP_PF);
	if (ret < 0)
		return ret;

	t4_sge_init(adap);

	/* tweak some settings */
	t4_write_reg(adap, TP_SHIFT_CNT, 0x64f8849);
	t4_write_reg(adap, ULP_RX_TDDP_PSZ, HPZ0(PAGE_SHIFT - 12));
	t4_write_reg(adap, TP_PIO_ADDR, TP_INGRESS_CONFIG);
	v = t4_read_reg(adap, TP_PIO_DATA);
	t4_write_reg(adap, TP_PIO_DATA, v & ~CSUM_HAS_PSEUDO_HDR);

	/* get basic stuff going */
	return t4_early_init(adap, adap->fn);
}

/*
 * Max # of ATIDs.  The absolute HW max is 16K but we keep it lower.
 */
#define MAX_ATIDS 8192U

/*
 * Phase 0 of initialization: contact FW, obtain config, perform basic init.
 */
static int adap_init0(struct adapter *adap)
{
	int ret;
	u32 v, port_vec;
	enum dev_state state;
	u32 params[7], val[7];
	struct fw_caps_config_cmd c;

	ret = t4_check_fw_version(adap);
	if (ret == -EINVAL || ret > 0) {
		if (upgrade_fw(adap) >= 0)             /* recache FW version */
			ret = t4_check_fw_version(adap);
	}
	if (ret < 0)
		return ret;

	/* contact FW, request master */
	ret = t4_fw_hello(adap, adap->fn, adap->fn, MASTER_MUST, &state);
	if (ret < 0) {
		dev_err(adap->pdev_dev, "could not connect to FW, error %d\n",
			ret);
		return ret;
	}

	/* reset device */
	ret = t4_fw_reset(adap, adap->fn, PIORSTMODE | PIORST);
	if (ret < 0)
		goto bye;

	for (v = 0; v < SGE_NTIMERS - 1; v++)
		adap->sge.timer_val[v] = min(intr_holdoff[v], MAX_SGE_TIMERVAL);
	adap->sge.timer_val[SGE_NTIMERS - 1] = MAX_SGE_TIMERVAL;
	adap->sge.counter_val[0] = 1;
	for (v = 1; v < SGE_NCOUNTERS; v++)
		adap->sge.counter_val[v] = min(intr_cnt[v - 1],
					       THRESHOLD_3_MASK);
#define FW_PARAM_DEV(param) \
	(FW_PARAMS_MNEM(FW_PARAMS_MNEM_DEV) | \
	 FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_DEV_##param))

	params[0] = FW_PARAM_DEV(CCLK);
	ret = t4_query_params(adap, adap->fn, adap->fn, 0, 1, params, val);
	if (ret < 0)
		goto bye;
	adap->params.vpd.cclk = val[0];

	ret = adap_init1(adap, &c);
	if (ret < 0)
		goto bye;

#define FW_PARAM_PFVF(param) \
	(FW_PARAMS_MNEM(FW_PARAMS_MNEM_PFVF) | \
	 FW_PARAMS_PARAM_X(FW_PARAMS_PARAM_PFVF_##param) | \
	 FW_PARAMS_PARAM_Y(adap->fn))

	params[0] = FW_PARAM_DEV(PORTVEC);
	params[1] = FW_PARAM_PFVF(L2T_START);
	params[2] = FW_PARAM_PFVF(L2T_END);
	params[3] = FW_PARAM_PFVF(FILTER_START);
	params[4] = FW_PARAM_PFVF(FILTER_END);
	ret = t4_query_params(adap, adap->fn, adap->fn, 0, 5, params, val);
	if (ret < 0)
		goto bye;
	port_vec = val[0];
	adap->tids.ftid_base = val[3];
	adap->tids.nftids = val[4] - val[3] + 1;

	if (c.ofldcaps) {
		/* query offload-related parameters */
		params[0] = FW_PARAM_DEV(NTID);
		params[1] = FW_PARAM_PFVF(SERVER_START);
		params[2] = FW_PARAM_PFVF(SERVER_END);
		params[3] = FW_PARAM_PFVF(TDDP_START);
		params[4] = FW_PARAM_PFVF(TDDP_END);
		params[5] = FW_PARAM_DEV(FLOWC_BUFFIFO_SZ);
		ret = t4_query_params(adap, adap->fn, adap->fn, 0, 6, params,
				      val);
		if (ret < 0)
			goto bye;
		adap->tids.ntids = val[0];
		adap->tids.natids = min(adap->tids.ntids / 2, MAX_ATIDS);
		adap->tids.stid_base = val[1];
		adap->tids.nstids = val[2] - val[1] + 1;
		adap->vres.ddp.start = val[3];
		adap->vres.ddp.size = val[4] - val[3] + 1;
		adap->params.ofldq_wr_cred = val[5];
		adap->params.offload = 1;
	}
	if (c.rdmacaps) {
		params[0] = FW_PARAM_PFVF(STAG_START);
		params[1] = FW_PARAM_PFVF(STAG_END);
		params[2] = FW_PARAM_PFVF(RQ_START);
		params[3] = FW_PARAM_PFVF(RQ_END);
		params[4] = FW_PARAM_PFVF(PBL_START);
		params[5] = FW_PARAM_PFVF(PBL_END);
		ret = t4_query_params(adap, adap->fn, adap->fn, 0, 6, params,
				      val);
		if (ret < 0)
			goto bye;
		adap->vres.stag.start = val[0];
		adap->vres.stag.size = val[1] - val[0] + 1;
		adap->vres.rq.start = val[2];
		adap->vres.rq.size = val[3] - val[2] + 1;
		adap->vres.pbl.start = val[4];
		adap->vres.pbl.size = val[5] - val[4] + 1;

		params[0] = FW_PARAM_PFVF(SQRQ_START);
		params[1] = FW_PARAM_PFVF(SQRQ_END);
		params[2] = FW_PARAM_PFVF(CQ_START);
		params[3] = FW_PARAM_PFVF(CQ_END);
		params[4] = FW_PARAM_PFVF(OCQ_START);
		params[5] = FW_PARAM_PFVF(OCQ_END);
		ret = t4_query_params(adap, adap->fn, adap->fn, 0, 6, params,
				      val);
		if (ret < 0)
			goto bye;
		adap->vres.qp.start = val[0];
		adap->vres.qp.size = val[1] - val[0] + 1;
		adap->vres.cq.start = val[2];
		adap->vres.cq.size = val[3] - val[2] + 1;
		adap->vres.ocq.start = val[4];
		adap->vres.ocq.size = val[5] - val[4] + 1;
	}
	if (c.iscsicaps) {
		params[0] = FW_PARAM_PFVF(ISCSI_START);
		params[1] = FW_PARAM_PFVF(ISCSI_END);
		ret = t4_query_params(adap, adap->fn, adap->fn, 0, 2, params,
				      val);
		if (ret < 0)
			goto bye;
		adap->vres.iscsi.start = val[0];
		adap->vres.iscsi.size = val[1] - val[0] + 1;
	}
#undef FW_PARAM_PFVF
#undef FW_PARAM_DEV

	adap->params.nports = hweight32(port_vec);
	adap->params.portvec = port_vec;
	adap->flags |= FW_OK;

	/* These are finalized by FW initialization, load their values now */
	v = t4_read_reg(adap, TP_TIMER_RESOLUTION);
	adap->params.tp.tre = TIMERRESOLUTION_GET(v);
	t4_read_mtu_tbl(adap, adap->params.mtus, NULL);
	t4_load_mtus(adap, adap->params.mtus, adap->params.a_wnd,
		     adap->params.b_wnd);

#ifdef CONFIG_PCI_IOV
	/*
	 * Provision resource limits for Virtual Functions.  We currently
	 * grant them all the same static resource limits except for the Port
	 * Access Rights Mask which we're assigning based on the PF.  All of
	 * the static provisioning stuff for both the PF and VF really needs
	 * to be managed in a persistent manner for each device which the
	 * firmware controls.
	 */
	{
		int pf, vf;

		for (pf = 0; pf < ARRAY_SIZE(num_vf); pf++) {
			if (num_vf[pf] <= 0)
				continue;

			/* VF numbering starts at 1! */
			for (vf = 1; vf <= num_vf[pf]; vf++) {
				ret = t4_cfg_pfvf(adap, adap->fn, pf, vf,
						  VFRES_NEQ, VFRES_NETHCTRL,
						  VFRES_NIQFLINT, VFRES_NIQ,
						  VFRES_TC, VFRES_NVI,
						  FW_PFVF_CMD_CMASK_MASK,
						  pfvfres_pmask(adap, pf, vf),
						  VFRES_NEXACTF,
						  VFRES_R_CAPS, VFRES_WX_CAPS);
				if (ret < 0)
					dev_warn(adap->pdev_dev, "failed to "
						 "provision pf/vf=%d/%d; "
						 "err=%d\n", pf, vf, ret);
			}
		}
	}
#endif

	setup_memwin(adap);
	return 0;

	/*
	 * If a command timed out or failed with EIO FW does not operate within
	 * its spec or something catastrophic happened to HW/FW, stop issuing
	 * commands.
	 */
bye:	if (ret != -ETIMEDOUT && ret != -EIO)
		t4_fw_bye(adap, adap->fn);
	return ret;
}

/* EEH callbacks */

static pci_ers_result_t eeh_err_detected(struct pci_dev *pdev,
					 pci_channel_state_t state)
{
	int i;
	struct adapter *adap = pci_get_drvdata(pdev);

	if (!adap)
		goto out;

	rtnl_lock();
	adap->flags &= ~FW_OK;
	notify_ulds(adap, CXGB4_STATE_START_RECOVERY);
	for_each_port(adap, i) {
		struct net_device *dev = adap->port[i];

		netif_device_detach(dev);
		netif_carrier_off(dev);
	}
	if (adap->flags & FULL_INIT_DONE)
		cxgb_down(adap);
	rtnl_unlock();
	pci_disable_device(pdev);
out:	return state == pci_channel_io_perm_failure ?
		PCI_ERS_RESULT_DISCONNECT : PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t eeh_slot_reset(struct pci_dev *pdev)
{
	int i, ret;
	struct fw_caps_config_cmd c;
	struct adapter *adap = pci_get_drvdata(pdev);

	if (!adap) {
		pci_restore_state(pdev);
		pci_save_state(pdev);
		return PCI_ERS_RESULT_RECOVERED;
	}

	if (pci_enable_device(pdev)) {
		dev_err(&pdev->dev, "cannot reenable PCI device after reset\n");
		return PCI_ERS_RESULT_DISCONNECT;
	}

	pci_set_master(pdev);
	pci_restore_state(pdev);
	pci_save_state(pdev);
	pci_cleanup_aer_uncorrect_error_status(pdev);

	if (t4_wait_dev_ready(adap) < 0)
		return PCI_ERS_RESULT_DISCONNECT;
	if (t4_fw_hello(adap, adap->fn, adap->fn, MASTER_MUST, NULL))
		return PCI_ERS_RESULT_DISCONNECT;
	adap->flags |= FW_OK;
	if (adap_init1(adap, &c))
		return PCI_ERS_RESULT_DISCONNECT;

	for_each_port(adap, i) {
		struct port_info *p = adap2pinfo(adap, i);

		ret = t4_alloc_vi(adap, adap->fn, p->tx_chan, adap->fn, 0, 1,
				  NULL, NULL);
		if (ret < 0)
			return PCI_ERS_RESULT_DISCONNECT;
		p->viid = ret;
		p->xact_addr_filt = -1;
	}

	t4_load_mtus(adap, adap->params.mtus, adap->params.a_wnd,
		     adap->params.b_wnd);
	setup_memwin(adap);
	if (cxgb_up(adap))
		return PCI_ERS_RESULT_DISCONNECT;
	return PCI_ERS_RESULT_RECOVERED;
}

static void eeh_resume(struct pci_dev *pdev)
{
	int i;
	struct adapter *adap = pci_get_drvdata(pdev);

	if (!adap)
		return;

	rtnl_lock();
	for_each_port(adap, i) {
		struct net_device *dev = adap->port[i];

		if (netif_running(dev)) {
			link_start(dev);
			cxgb_set_rxmode(dev);
		}
		netif_device_attach(dev);
	}
	rtnl_unlock();
}

static struct pci_error_handlers cxgb4_eeh = {
	.error_detected = eeh_err_detected,
	.slot_reset     = eeh_slot_reset,
	.resume         = eeh_resume,
};

static inline bool is_10g_port(const struct link_config *lc)
{
	return (lc->supported & FW_PORT_CAP_SPEED_10G) != 0;
}

static inline void init_rspq(struct sge_rspq *q, u8 timer_idx, u8 pkt_cnt_idx,
			     unsigned int size, unsigned int iqe_size)
{
	q->intr_params = QINTR_TIMER_IDX(timer_idx) |
			 (pkt_cnt_idx < SGE_NCOUNTERS ? QINTR_CNT_EN : 0);
	q->pktcnt_idx = pkt_cnt_idx < SGE_NCOUNTERS ? pkt_cnt_idx : 0;
	q->iqe_len = iqe_size;
	q->size = size;
}

/*
 * Perform default configuration of DMA queues depending on the number and type
 * of ports we found and the number of available CPUs.  Most settings can be
 * modified by the admin prior to actual use.
 */
static void __devinit cfg_queues(struct adapter *adap)
{
	struct sge *s = &adap->sge;
	int i, q10g = 0, n10g = 0, qidx = 0;

	for_each_port(adap, i)
		n10g += is_10g_port(&adap2pinfo(adap, i)->link_cfg);

	/*
	 * We default to 1 queue per non-10G port and up to # of cores queues
	 * per 10G port.
	 */
	if (n10g)
		q10g = (MAX_ETH_QSETS - (adap->params.nports - n10g)) / n10g;
	if (q10g > num_online_cpus())
		q10g = num_online_cpus();

	for_each_port(adap, i) {
		struct port_info *pi = adap2pinfo(adap, i);

		pi->first_qset = qidx;
		pi->nqsets = is_10g_port(&pi->link_cfg) ? q10g : 1;
		qidx += pi->nqsets;
	}

	s->ethqsets = qidx;
	s->max_ethqsets = qidx;   /* MSI-X may lower it later */

	if (is_offload(adap)) {
		/*
		 * For offload we use 1 queue/channel if all ports are up to 1G,
		 * otherwise we divide all available queues amongst the channels
		 * capped by the number of available cores.
		 */
		if (n10g) {
			i = min_t(int, ARRAY_SIZE(s->ofldrxq),
				  num_online_cpus());
			s->ofldqsets = roundup(i, adap->params.nports);
		} else
			s->ofldqsets = adap->params.nports;
		/* For RDMA one Rx queue per channel suffices */
		s->rdmaqs = adap->params.nports;
	}

	for (i = 0; i < ARRAY_SIZE(s->ethrxq); i++) {
		struct sge_eth_rxq *r = &s->ethrxq[i];

		init_rspq(&r->rspq, 0, 0, 1024, 64);
		r->fl.size = 72;
	}

	for (i = 0; i < ARRAY_SIZE(s->ethtxq); i++)
		s->ethtxq[i].q.size = 1024;

	for (i = 0; i < ARRAY_SIZE(s->ctrlq); i++)
		s->ctrlq[i].q.size = 512;

	for (i = 0; i < ARRAY_SIZE(s->ofldtxq); i++)
		s->ofldtxq[i].q.size = 1024;

	for (i = 0; i < ARRAY_SIZE(s->ofldrxq); i++) {
		struct sge_ofld_rxq *r = &s->ofldrxq[i];

		init_rspq(&r->rspq, 0, 0, 1024, 64);
		r->rspq.uld = CXGB4_ULD_ISCSI;
		r->fl.size = 72;
	}

	for (i = 0; i < ARRAY_SIZE(s->rdmarxq); i++) {
		struct sge_ofld_rxq *r = &s->rdmarxq[i];

		init_rspq(&r->rspq, 0, 0, 511, 64);
		r->rspq.uld = CXGB4_ULD_RDMA;
		r->fl.size = 72;
	}

	init_rspq(&s->fw_evtq, 6, 0, 512, 64);
	init_rspq(&s->intrq, 6, 0, 2 * MAX_INGQ, 64);
}

/*
 * Reduce the number of Ethernet queues across all ports to at most n.
 * n provides at least one queue per port.
 */
static void __devinit reduce_ethqs(struct adapter *adap, int n)
{
	int i;
	struct port_info *pi;

	while (n < adap->sge.ethqsets)
		for_each_port(adap, i) {
			pi = adap2pinfo(adap, i);
			if (pi->nqsets > 1) {
				pi->nqsets--;
				adap->sge.ethqsets--;
				if (adap->sge.ethqsets <= n)
					break;
			}
		}

	n = 0;
	for_each_port(adap, i) {
		pi = adap2pinfo(adap, i);
		pi->first_qset = n;
		n += pi->nqsets;
	}
}

/* 2 MSI-X vectors needed for the FW queue and non-data interrupts */
#define EXTRA_VECS 2

static int __devinit enable_msix(struct adapter *adap)
{
	int ofld_need = 0;
	int i, err, want, need;
	struct sge *s = &adap->sge;
	unsigned int nchan = adap->params.nports;
	struct msix_entry entries[MAX_INGQ + 1];

	for (i = 0; i < ARRAY_SIZE(entries); ++i)
		entries[i].entry = i;

	want = s->max_ethqsets + EXTRA_VECS;
	if (is_offload(adap)) {
		want += s->rdmaqs + s->ofldqsets;
		/* need nchan for each possible ULD */
		ofld_need = 2 * nchan;
	}
	need = adap->params.nports + EXTRA_VECS + ofld_need;

	while ((err = pci_enable_msix(adap->pdev, entries, want)) >= need)
		want = err;

	if (!err) {
		/*
		 * Distribute available vectors to the various queue groups.
		 * Every group gets its minimum requirement and NIC gets top
		 * priority for leftovers.
		 */
		i = want - EXTRA_VECS - ofld_need;
		if (i < s->max_ethqsets) {
			s->max_ethqsets = i;
			if (i < s->ethqsets)
				reduce_ethqs(adap, i);
		}
		if (is_offload(adap)) {
			i = want - EXTRA_VECS - s->max_ethqsets;
			i -= ofld_need - nchan;
			s->ofldqsets = (i / nchan) * nchan;  /* round down */
		}
		for (i = 0; i < want; ++i)
			adap->msix_info[i].vec = entries[i].vector;
	} else if (err > 0)
		dev_info(adap->pdev_dev,
			 "only %d MSI-X vectors left, not using MSI-X\n", err);
	return err;
}

#undef EXTRA_VECS

static int __devinit init_rss(struct adapter *adap)
{
	unsigned int i, j;

	for_each_port(adap, i) {
		struct port_info *pi = adap2pinfo(adap, i);

		pi->rss = kcalloc(pi->rss_size, sizeof(u16), GFP_KERNEL);
		if (!pi->rss)
			return -ENOMEM;
		for (j = 0; j < pi->rss_size; j++)
			pi->rss[j] = j % pi->nqsets;
	}
	return 0;
}

static void __devinit print_port_info(struct adapter *adap)
{
	static const char *base[] = {
		"R XFI", "R XAUI", "T SGMII", "T XFI", "T XAUI", "KX4", "CX4",
		"KX", "KR", "KR SFP+", "KR FEC"
	};

	int i;
	char buf[80];
	const char *spd = "";

	if (adap->params.pci.speed == PCI_EXP_LNKSTA_CLS_2_5GB)
		spd = " 2.5 GT/s";
	else if (adap->params.pci.speed == PCI_EXP_LNKSTA_CLS_5_0GB)
		spd = " 5 GT/s";

	for_each_port(adap, i) {
		struct net_device *dev = adap->port[i];
		const struct port_info *pi = netdev_priv(dev);
		char *bufp = buf;

		if (!test_bit(i, &adap->registered_device_map))
			continue;

		if (pi->link_cfg.supported & FW_PORT_CAP_SPEED_100M)
			bufp += sprintf(bufp, "100/");
		if (pi->link_cfg.supported & FW_PORT_CAP_SPEED_1G)
			bufp += sprintf(bufp, "1000/");
		if (pi->link_cfg.supported & FW_PORT_CAP_SPEED_10G)
			bufp += sprintf(bufp, "10G/");
		if (bufp != buf)
			--bufp;
		sprintf(bufp, "BASE-%s", base[pi->port_type]);

		netdev_info(dev, "Chelsio %s rev %d %s %sNIC PCIe x%d%s%s\n",
			    adap->params.vpd.id, adap->params.rev,
			    buf, is_offload(adap) ? "R" : "",
			    adap->params.pci.width, spd,
			    (adap->flags & USING_MSIX) ? " MSI-X" :
			    (adap->flags & USING_MSI) ? " MSI" : "");
		if (adap->name == dev->name)
			netdev_info(dev, "S/N: %s, E/C: %s\n",
				    adap->params.vpd.sn, adap->params.vpd.ec);
	}
}

/*
 * Free the following resources:
 * - memory used for tables
 * - MSI/MSI-X
 * - net devices
 * - resources FW is holding for us
 */
static void free_some_resources(struct adapter *adapter)
{
	unsigned int i;

	t4_free_mem(adapter->l2t);
	t4_free_mem(adapter->tids.tid_tab);
	disable_msi(adapter);

	for_each_port(adapter, i)
		if (adapter->port[i]) {
			kfree(adap2pinfo(adapter, i)->rss);
			free_netdev(adapter->port[i]);
		}
	if (adapter->flags & FW_OK)
		t4_fw_bye(adapter, adapter->fn);
}

#define VLAN_FEAT (NETIF_F_SG | NETIF_F_IP_CSUM | TSO_FLAGS | \
		   NETIF_F_IPV6_CSUM | NETIF_F_HIGHDMA)

static int __devinit init_one(struct pci_dev *pdev,
			      const struct pci_device_id *ent)
{
	int func, i, err;
	struct port_info *pi;
	unsigned int highdma = 0;
	struct adapter *adapter = NULL;

	printk_once(KERN_INFO "%s - version %s\n", DRV_DESC, DRV_VERSION);

	err = pci_request_regions(pdev, KBUILD_MODNAME);
	if (err) {
		/* Just info, some other driver may have claimed the device. */
		dev_info(&pdev->dev, "cannot obtain PCI resources\n");
		return err;
	}

	/* We control everything through one PF */
	func = PCI_FUNC(pdev->devfn);
	if (func != ent->driver_data) {
		pci_save_state(pdev);        /* to restore SR-IOV later */
		goto sriov;
	}

	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "cannot enable PCI device\n");
		goto out_release_regions;
	}

	if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(64))) {
		highdma = NETIF_F_HIGHDMA;
		err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
		if (err) {
			dev_err(&pdev->dev, "unable to obtain 64-bit DMA for "
				"coherent allocations\n");
			goto out_disable_device;
		}
	} else {
		err = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (err) {
			dev_err(&pdev->dev, "no usable DMA configuration\n");
			goto out_disable_device;
		}
	}

	pci_enable_pcie_error_reporting(pdev);
	pci_set_master(pdev);
	pci_save_state(pdev);

	adapter = kzalloc(sizeof(*adapter), GFP_KERNEL);
	if (!adapter) {
		err = -ENOMEM;
		goto out_disable_device;
	}

	adapter->regs = pci_ioremap_bar(pdev, 0);
	if (!adapter->regs) {
		dev_err(&pdev->dev, "cannot map device registers\n");
		err = -ENOMEM;
		goto out_free_adapter;
	}

	adapter->pdev = pdev;
	adapter->pdev_dev = &pdev->dev;
	adapter->fn = func;
	adapter->name = pci_name(pdev);
	adapter->msg_enable = dflt_msg_enable;
	memset(adapter->chan_map, 0xff, sizeof(adapter->chan_map));

	spin_lock_init(&adapter->stats_lock);
	spin_lock_init(&adapter->tid_release_lock);

	INIT_WORK(&adapter->tid_release_task, process_tid_release_list);

	err = t4_prep_adapter(adapter);
	if (err)
		goto out_unmap_bar;
	err = adap_init0(adapter);
	if (err)
		goto out_unmap_bar;

	for_each_port(adapter, i) {
		struct net_device *netdev;

		netdev = alloc_etherdev_mq(sizeof(struct port_info),
					   MAX_ETH_QSETS);
		if (!netdev) {
			err = -ENOMEM;
			goto out_free_dev;
		}

		SET_NETDEV_DEV(netdev, &pdev->dev);

		adapter->port[i] = netdev;
		pi = netdev_priv(netdev);
		pi->adapter = adapter;
		pi->xact_addr_filt = -1;
		pi->rx_offload = RX_CSO;
		pi->port_id = i;
		netif_carrier_off(netdev);
		netif_tx_stop_all_queues(netdev);
		netdev->irq = pdev->irq;

		netdev->features |= NETIF_F_SG | TSO_FLAGS;
		netdev->features |= NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM;
		netdev->features |= NETIF_F_GRO | NETIF_F_RXHASH | highdma;
		netdev->features |= NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX;
		netdev->vlan_features = netdev->features & VLAN_FEAT;

		netdev->netdev_ops = &cxgb4_netdev_ops;
		SET_ETHTOOL_OPS(netdev, &cxgb_ethtool_ops);
	}

	pci_set_drvdata(pdev, adapter);

	if (adapter->flags & FW_OK) {
		err = t4_port_init(adapter, func, func, 0);
		if (err)
			goto out_free_dev;
	}

	/*
	 * Configure queues and allocate tables now, they can be needed as
	 * soon as the first register_netdev completes.
	 */
	cfg_queues(adapter);

	adapter->l2t = t4_init_l2t();
	if (!adapter->l2t) {
		/* We tolerate a lack of L2T, giving up some functionality */
		dev_warn(&pdev->dev, "could not allocate L2T, continuing\n");
		adapter->params.offload = 0;
	}

	if (is_offload(adapter) && tid_init(&adapter->tids) < 0) {
		dev_warn(&pdev->dev, "could not allocate TID table, "
			 "continuing\n");
		adapter->params.offload = 0;
	}

	/* See what interrupts we'll be using */
	if (msi > 1 && enable_msix(adapter) == 0)
		adapter->flags |= USING_MSIX;
	else if (msi > 0 && pci_enable_msi(pdev) == 0)
		adapter->flags |= USING_MSI;

	err = init_rss(adapter);
	if (err)
		goto out_free_dev;

	/*
	 * The card is now ready to go.  If any errors occur during device
	 * registration we do not fail the whole card but rather proceed only
	 * with the ports we manage to register successfully.  However we must
	 * register at least one net device.
	 */
	for_each_port(adapter, i) {
		err = register_netdev(adapter->port[i]);
		if (err)
			dev_warn(&pdev->dev,
				 "cannot register net device %s, skipping\n",
				 adapter->port[i]->name);
		else {
			/*
			 * Change the name we use for messages to the name of
			 * the first successfully registered interface.
			 */
			if (!adapter->registered_device_map)
				adapter->name = adapter->port[i]->name;

			__set_bit(i, &adapter->registered_device_map);
			adapter->chan_map[adap2pinfo(adapter, i)->tx_chan] = i;
		}
	}
	if (!adapter->registered_device_map) {
		dev_err(&pdev->dev, "could not register any net devices\n");
		goto out_free_dev;
	}

	if (cxgb4_debugfs_root) {
		adapter->debugfs_root = debugfs_create_dir(pci_name(pdev),
							   cxgb4_debugfs_root);
		setup_debugfs(adapter);
	}

	if (is_offload(adapter))
		attach_ulds(adapter);

	print_port_info(adapter);

sriov:
#ifdef CONFIG_PCI_IOV
	if (func < ARRAY_SIZE(num_vf) && num_vf[func] > 0)
		if (pci_enable_sriov(pdev, num_vf[func]) == 0)
			dev_info(&pdev->dev,
				 "instantiated %u virtual functions\n",
				 num_vf[func]);
#endif
	return 0;

 out_free_dev:
	free_some_resources(adapter);
 out_unmap_bar:
	iounmap(adapter->regs);
 out_free_adapter:
	kfree(adapter);
 out_disable_device:
	pci_disable_pcie_error_reporting(pdev);
	pci_disable_device(pdev);
 out_release_regions:
	pci_release_regions(pdev);
	pci_set_drvdata(pdev, NULL);
	return err;
}

static void __devexit remove_one(struct pci_dev *pdev)
{
	struct adapter *adapter = pci_get_drvdata(pdev);

	pci_disable_sriov(pdev);

	if (adapter) {
		int i;

		if (is_offload(adapter))
			detach_ulds(adapter);

		for_each_port(adapter, i)
			if (test_bit(i, &adapter->registered_device_map))
				unregister_netdev(adapter->port[i]);

		if (adapter->debugfs_root)
			debugfs_remove_recursive(adapter->debugfs_root);

		if (adapter->flags & FULL_INIT_DONE)
			cxgb_down(adapter);

		free_some_resources(adapter);
		iounmap(adapter->regs);
		kfree(adapter);
		pci_disable_pcie_error_reporting(pdev);
		pci_disable_device(pdev);
		pci_release_regions(pdev);
		pci_set_drvdata(pdev, NULL);
	} else if (PCI_FUNC(pdev->devfn) > 0)
		pci_release_regions(pdev);
}

static struct pci_driver cxgb4_driver = {
	.name     = KBUILD_MODNAME,
	.id_table = cxgb4_pci_tbl,
	.probe    = init_one,
	.remove   = __devexit_p(remove_one),
	.err_handler = &cxgb4_eeh,
};

static int __init cxgb4_init_module(void)
{
	int ret;

	/* Debugfs support is optional, just warn if this fails */
	cxgb4_debugfs_root = debugfs_create_dir(KBUILD_MODNAME, NULL);
	if (!cxgb4_debugfs_root)
		pr_warning("could not create debugfs entry, continuing\n");

	ret = pci_register_driver(&cxgb4_driver);
	if (ret < 0)
		debugfs_remove(cxgb4_debugfs_root);
	return ret;
}

static void __exit cxgb4_cleanup_module(void)
{
	pci_unregister_driver(&cxgb4_driver);
	debugfs_remove(cxgb4_debugfs_root);  /* NULL ok */
}

module_init(cxgb4_init_module);
module_exit(cxgb4_cleanup_module);
