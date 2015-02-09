/*
 * flexcan.c - FLEXCAN CAN controller driver
 *
 * Copyright (c) 2005-2006 Varma Electronics Oy
 * Copyright (c) 2009 Sascha Hauer, Pengutronix
 * Copyright (c) 2010 Marc Kleine-Budde, Pengutronix
 *
 * Based on code originally by Andrey Volkov <avolkov@varma-el.com>
 *
 * LICENCE:
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/netdevice.h>
#include <linux/can.h>
#include <linux/can/dev.h>
#include <linux/can/error.h>
#include <linux/can/platform/flexcan.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <mach/clock.h>

#define DRV_NAME			"flexcan"

/* 8 for RX fifo and 2 error handling */
#define FLEXCAN_NAPI_WEIGHT		(8 + 2)

/* FLEXCAN module configuration register (CANMCR) bits */
#define FLEXCAN_MCR_MDIS		BIT(31)
#define FLEXCAN_MCR_FRZ			BIT(30)
#define FLEXCAN_MCR_FEN			BIT(29)
#define FLEXCAN_MCR_HALT		BIT(28)
#define FLEXCAN_MCR_NOT_RDY		BIT(27)
#define FLEXCAN_MCR_WAK_MSK		BIT(26)
#define FLEXCAN_MCR_SOFTRST		BIT(25)
#define FLEXCAN_MCR_FRZ_ACK		BIT(24)
#define FLEXCAN_MCR_SUPV		BIT(23)
#define FLEXCAN_MCR_SLF_WAK		BIT(22)
#define FLEXCAN_MCR_WRN_EN		BIT(21)
#define FLEXCAN_MCR_LPM_ACK		BIT(20)
#define FLEXCAN_MCR_WAK_SRC		BIT(19)
#define FLEXCAN_MCR_DOZE		BIT(18)
#define FLEXCAN_MCR_SRX_DIS		BIT(17)
#define FLEXCAN_MCR_BCC			BIT(16)
#define FLEXCAN_MCR_LPRIO_EN		BIT(13)
#define FLEXCAN_MCR_AEN			BIT(12)
#define FLEXCAN_MCR_MAXMB(x)		((x) & 0xf)
#define FLEXCAN_MCR_IDAM_A		(0 << 8)
#define FLEXCAN_MCR_IDAM_B		(1 << 8)
#define FLEXCAN_MCR_IDAM_C		(2 << 8)
#define FLEXCAN_MCR_IDAM_D		(3 << 8)

/* FLEXCAN control register (CANCTRL) bits */
#define FLEXCAN_CTRL_PRESDIV(x)		(((x) & 0xff) << 24)
#define FLEXCAN_CTRL_RJW(x)		(((x) & 0x03) << 22)
#define FLEXCAN_CTRL_PSEG1(x)		(((x) & 0x07) << 19)
#define FLEXCAN_CTRL_PSEG2(x)		(((x) & 0x07) << 16)
#define FLEXCAN_CTRL_BOFF_MSK		BIT(15)
#define FLEXCAN_CTRL_ERR_MSK		BIT(14)
#define FLEXCAN_CTRL_CLK_SRC		BIT(13)
#define FLEXCAN_CTRL_LPB		BIT(12)
#define FLEXCAN_CTRL_TWRN_MSK		BIT(11)
#define FLEXCAN_CTRL_RWRN_MSK		BIT(10)
#define FLEXCAN_CTRL_SMP		BIT(7)
#define FLEXCAN_CTRL_BOFF_REC		BIT(6)
#define FLEXCAN_CTRL_TSYN		BIT(5)
#define FLEXCAN_CTRL_LBUF		BIT(4)
#define FLEXCAN_CTRL_LOM		BIT(3)
#define FLEXCAN_CTRL_PROPSEG(x)		((x) & 0x07)
#define FLEXCAN_CTRL_ERR_BUS		(FLEXCAN_CTRL_ERR_MSK)
#define FLEXCAN_CTRL_ERR_STATE \
	(FLEXCAN_CTRL_TWRN_MSK | FLEXCAN_CTRL_RWRN_MSK | \
	 FLEXCAN_CTRL_BOFF_MSK)
#define FLEXCAN_CTRL_ERR_ALL \
	(FLEXCAN_CTRL_ERR_BUS | FLEXCAN_CTRL_ERR_STATE)

/* FLEXCAN error and status register (ESR) bits */
#define FLEXCAN_ESR_TWRN_INT		BIT(17)
#define FLEXCAN_ESR_RWRN_INT		BIT(16)
#define FLEXCAN_ESR_BIT1_ERR		BIT(15)
#define FLEXCAN_ESR_BIT0_ERR		BIT(14)
#define FLEXCAN_ESR_ACK_ERR		BIT(13)
#define FLEXCAN_ESR_CRC_ERR		BIT(12)
#define FLEXCAN_ESR_FRM_ERR		BIT(11)
#define FLEXCAN_ESR_STF_ERR		BIT(10)
#define FLEXCAN_ESR_TX_WRN		BIT(9)
#define FLEXCAN_ESR_RX_WRN		BIT(8)
#define FLEXCAN_ESR_IDLE		BIT(7)
#define FLEXCAN_ESR_TXRX		BIT(6)
#define FLEXCAN_EST_FLT_CONF_SHIFT	(4)
#define FLEXCAN_ESR_FLT_CONF_MASK	(0x3 << FLEXCAN_EST_FLT_CONF_SHIFT)
#define FLEXCAN_ESR_FLT_CONF_ACTIVE	(0x0 << FLEXCAN_EST_FLT_CONF_SHIFT)
#define FLEXCAN_ESR_FLT_CONF_PASSIVE	(0x1 << FLEXCAN_EST_FLT_CONF_SHIFT)
#define FLEXCAN_ESR_BOFF_INT		BIT(2)
#define FLEXCAN_ESR_ERR_INT		BIT(1)
#define FLEXCAN_ESR_WAK_INT		BIT(0)
#define FLEXCAN_ESR_ERR_BUS \
	(FLEXCAN_ESR_BIT1_ERR | FLEXCAN_ESR_BIT0_ERR | \
	 FLEXCAN_ESR_ACK_ERR | FLEXCAN_ESR_CRC_ERR | \
	 FLEXCAN_ESR_FRM_ERR | FLEXCAN_ESR_STF_ERR)
#define FLEXCAN_ESR_ERR_STATE \
	(FLEXCAN_ESR_TWRN_INT | FLEXCAN_ESR_RWRN_INT | FLEXCAN_ESR_BOFF_INT)
#define FLEXCAN_ESR_ERR_ALL \
	(FLEXCAN_ESR_ERR_BUS | FLEXCAN_ESR_ERR_STATE)

/* FLEXCAN interrupt flag register (IFLAG) bits */
#define FLEXCAN_TX_BUF_ID		8
#define FLEXCAN_IFLAG_BUF(x)		BIT(x)
#define FLEXCAN_IFLAG_RX_FIFO_OVERFLOW	BIT(7)
#define FLEXCAN_IFLAG_RX_FIFO_WARN	BIT(6)
#define FLEXCAN_IFLAG_RX_FIFO_AVAILABLE	BIT(5)
#define FLEXCAN_IFLAG_DEFAULT \
	(FLEXCAN_IFLAG_RX_FIFO_OVERFLOW | FLEXCAN_IFLAG_RX_FIFO_AVAILABLE | \
	 FLEXCAN_IFLAG_BUF(FLEXCAN_TX_BUF_ID))

/* FLEXCAN message buffers */
#define FLEXCAN_MB_CNT_CODE(x)		(((x) & 0xf) << 24)
#define FLEXCAN_MB_CNT_SRR		BIT(22)
#define FLEXCAN_MB_CNT_IDE		BIT(21)
#define FLEXCAN_MB_CNT_RTR		BIT(20)
#define FLEXCAN_MB_CNT_LENGTH(x)	(((x) & 0xf) << 16)
#define FLEXCAN_MB_CNT_TIMESTAMP(x)	((x) & 0xffff)

#define FLEXCAN_MB_CODE_MASK		(0xf0ffffff)

/* Structure of the message buffer */
struct flexcan_mb {
	u32 can_ctrl;
	u32 can_id;
	u32 data[2];
};

/* Structure of the hardware registers */
struct flexcan_regs {
	u32 mcr;		/* 0x00 */
	u32 ctrl;		/* 0x04 */
	u32 timer;		/* 0x08 */
	u32 _reserved1;		/* 0x0c */
	u32 rxgmask;		/* 0x10 */
	u32 rx14mask;		/* 0x14 */
	u32 rx15mask;		/* 0x18 */
	u32 ecr;		/* 0x1c */
	u32 esr;		/* 0x20 */
	u32 imask2;		/* 0x24 */
	u32 imask1;		/* 0x28 */
	u32 iflag2;		/* 0x2c */
	u32 iflag1;		/* 0x30 */
	u32 _reserved2[19];
	struct flexcan_mb cantxfg[64];
};

struct flexcan_priv {
	struct can_priv can;
	struct net_device *dev;
	struct napi_struct napi;

	void __iomem *base;
	u32 reg_esr;
	u32 reg_ctrl_default;

	struct clk *clk;
	struct flexcan_platform_data *pdata;
};

static struct can_bittiming_const flexcan_bittiming_const = {
	.name = DRV_NAME,
	.tseg1_min = 4,
	.tseg1_max = 16,
	.tseg2_min = 2,
	.tseg2_max = 8,
	.sjw_max = 4,
	.brp_min = 1,
	.brp_max = 256,
	.brp_inc = 1,
};

/*
 * Swtich transceiver on or off
 */
static void flexcan_transceiver_switch(const struct flexcan_priv *priv, int on)
{
	if (priv->pdata && priv->pdata->transceiver_switch)
		priv->pdata->transceiver_switch(on);
}

static inline int flexcan_has_and_handle_berr(const struct flexcan_priv *priv,
					      u32 reg_esr)
{
	return (priv->can.ctrlmode & CAN_CTRLMODE_BERR_REPORTING) &&
		(reg_esr & FLEXCAN_ESR_ERR_BUS);
}

static inline void flexcan_chip_enable(struct flexcan_priv *priv)
{
	struct flexcan_regs __iomem *regs = priv->base;
	u32 reg;

	reg = readl(&regs->mcr);
	reg &= ~FLEXCAN_MCR_MDIS;
	writel(reg, &regs->mcr);

	udelay(10);
}

static inline void flexcan_chip_disable(struct flexcan_priv *priv)
{
	struct flexcan_regs __iomem *regs = priv->base;
	u32 reg;

	reg = readl(&regs->mcr);
	reg |= FLEXCAN_MCR_MDIS;
	writel(reg, &regs->mcr);
}

static int flexcan_get_berr_counter(const struct net_device *dev,
				    struct can_berr_counter *bec)
{
	const struct flexcan_priv *priv = netdev_priv(dev);
	struct flexcan_regs __iomem *regs = priv->base;
	u32 reg = readl(&regs->ecr);

	bec->txerr = (reg >> 0) & 0xff;
	bec->rxerr = (reg >> 8) & 0xff;

	return 0;
}

static int flexcan_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	const struct flexcan_priv *priv = netdev_priv(dev);
	struct net_device_stats *stats = &dev->stats;
	struct flexcan_regs __iomem *regs = priv->base;
	struct can_frame *cf = (struct can_frame *)skb->data;
	u32 can_id;
	u32 ctrl = FLEXCAN_MB_CNT_CODE(0xc) | (cf->can_dlc << 16);

	if (can_dropped_invalid_skb(dev, skb))
		return NETDEV_TX_OK;

	netif_stop_queue(dev);

	if (cf->can_id & CAN_EFF_FLAG) {
		can_id = cf->can_id & CAN_EFF_MASK;
		ctrl |= FLEXCAN_MB_CNT_IDE | FLEXCAN_MB_CNT_SRR;
	} else {
		can_id = (cf->can_id & CAN_SFF_MASK) << 18;
	}

	if (cf->can_id & CAN_RTR_FLAG)
		ctrl |= FLEXCAN_MB_CNT_RTR;

	if (cf->can_dlc > 0) {
		u32 data = be32_to_cpup((__be32 *)&cf->data[0]);
		writel(data, &regs->cantxfg[FLEXCAN_TX_BUF_ID].data[0]);
	}
	if (cf->can_dlc > 3) {
		u32 data = be32_to_cpup((__be32 *)&cf->data[4]);
		writel(data, &regs->cantxfg[FLEXCAN_TX_BUF_ID].data[1]);
	}

	writel(can_id, &regs->cantxfg[FLEXCAN_TX_BUF_ID].can_id);
	writel(ctrl, &regs->cantxfg[FLEXCAN_TX_BUF_ID].can_ctrl);

	kfree_skb(skb);

	/* tx_packets is incremented in flexcan_irq */
	stats->tx_bytes += cf->can_dlc;

	return NETDEV_TX_OK;
}

static void do_bus_err(struct net_device *dev,
		       struct can_frame *cf, u32 reg_esr)
{
	struct flexcan_priv *priv = netdev_priv(dev);
	int rx_errors = 0, tx_errors = 0;

	cf->can_id |= CAN_ERR_PROT | CAN_ERR_BUSERROR;

	if (reg_esr & FLEXCAN_ESR_BIT1_ERR) {
		dev_dbg(dev->dev.parent, "BIT1_ERR irq\n");
		cf->data[2] |= CAN_ERR_PROT_BIT1;
		tx_errors = 1;
	}
	if (reg_esr & FLEXCAN_ESR_BIT0_ERR) {
		dev_dbg(dev->dev.parent, "BIT0_ERR irq\n");
		cf->data[2] |= CAN_ERR_PROT_BIT0;
		tx_errors = 1;
	}
	if (reg_esr & FLEXCAN_ESR_ACK_ERR) {
		dev_dbg(dev->dev.parent, "ACK_ERR irq\n");
		cf->can_id |= CAN_ERR_ACK;
		cf->data[3] |= CAN_ERR_PROT_LOC_ACK;
		tx_errors = 1;
	}
	if (reg_esr & FLEXCAN_ESR_CRC_ERR) {
		dev_dbg(dev->dev.parent, "CRC_ERR irq\n");
		cf->data[2] |= CAN_ERR_PROT_BIT;
		cf->data[3] |= CAN_ERR_PROT_LOC_CRC_SEQ;
		rx_errors = 1;
	}
	if (reg_esr & FLEXCAN_ESR_FRM_ERR) {
		dev_dbg(dev->dev.parent, "FRM_ERR irq\n");
		cf->data[2] |= CAN_ERR_PROT_FORM;
		rx_errors = 1;
	}
	if (reg_esr & FLEXCAN_ESR_STF_ERR) {
		dev_dbg(dev->dev.parent, "STF_ERR irq\n");
		cf->data[2] |= CAN_ERR_PROT_STUFF;
		rx_errors = 1;
	}

	priv->can.can_stats.bus_error++;
	if (rx_errors)
		dev->stats.rx_errors++;
	if (tx_errors)
		dev->stats.tx_errors++;
}

static int flexcan_poll_bus_err(struct net_device *dev, u32 reg_esr)
{
	struct sk_buff *skb;
	struct can_frame *cf;

	skb = alloc_can_err_skb(dev, &cf);
	if (unlikely(!skb))
		return 0;

	do_bus_err(dev, cf, reg_esr);
	netif_receive_skb(skb);

	dev->stats.rx_packets++;
	dev->stats.rx_bytes += cf->can_dlc;

	return 1;
}

static void do_state(struct net_device *dev,
		     struct can_frame *cf, enum can_state new_state)
{
	struct flexcan_priv *priv = netdev_priv(dev);
	struct can_berr_counter bec;

	flexcan_get_berr_counter(dev, &bec);

	switch (priv->can.state) {
	case CAN_STATE_ERROR_ACTIVE:
		/*
		 * from: ERROR_ACTIVE
		 * to  : ERROR_WARNING, ERROR_PASSIVE, BUS_OFF
		 * =>  : there was a warning int
		 */
		if (new_state >= CAN_STATE_ERROR_WARNING &&
		    new_state <= CAN_STATE_BUS_OFF) {
			dev_dbg(dev->dev.parent, "Error Warning IRQ\n");
			priv->can.can_stats.error_warning++;

			cf->can_id |= CAN_ERR_CRTL;
			cf->data[1] = (bec.txerr > bec.rxerr) ?
				CAN_ERR_CRTL_TX_WARNING :
				CAN_ERR_CRTL_RX_WARNING;
		}
	case CAN_STATE_ERROR_WARNING:	/* fallthrough */
		/*
		 * from: ERROR_ACTIVE, ERROR_WARNING
		 * to  : ERROR_PASSIVE, BUS_OFF
		 * =>  : error passive int
		 */
		if (new_state >= CAN_STATE_ERROR_PASSIVE &&
		    new_state <= CAN_STATE_BUS_OFF) {
			dev_dbg(dev->dev.parent, "Error Passive IRQ\n");
			priv->can.can_stats.error_passive++;

			cf->can_id |= CAN_ERR_CRTL;
			cf->data[1] = (bec.txerr > bec.rxerr) ?
				CAN_ERR_CRTL_TX_PASSIVE :
				CAN_ERR_CRTL_RX_PASSIVE;
		}
		break;
	case CAN_STATE_BUS_OFF:
		dev_err(dev->dev.parent,
			"BUG! hardware recovered automatically from BUS_OFF\n");
		break;
	default:
		break;
	}

	/* process state changes depending on the new state */
	switch (new_state) {
	case CAN_STATE_ERROR_ACTIVE:
		dev_dbg(dev->dev.parent, "Error Active\n");
		cf->can_id |= CAN_ERR_PROT;
		cf->data[2] = CAN_ERR_PROT_ACTIVE;
		break;
	case CAN_STATE_BUS_OFF:
		cf->can_id |= CAN_ERR_BUSOFF;
		can_bus_off(dev);
		break;
	default:
		break;
	}
}

static int flexcan_poll_state(struct net_device *dev, u32 reg_esr)
{
	struct flexcan_priv *priv = netdev_priv(dev);
	struct sk_buff *skb;
	struct can_frame *cf;
	enum can_state new_state;
	int flt;

	flt = reg_esr & FLEXCAN_ESR_FLT_CONF_MASK;
	if (likely(flt == FLEXCAN_ESR_FLT_CONF_ACTIVE)) {
		if (likely(!(reg_esr & (FLEXCAN_ESR_TX_WRN |
					FLEXCAN_ESR_RX_WRN))))
			new_state = CAN_STATE_ERROR_ACTIVE;
		else
			new_state = CAN_STATE_ERROR_WARNING;
	} else if (unlikely(flt == FLEXCAN_ESR_FLT_CONF_PASSIVE))
		new_state = CAN_STATE_ERROR_PASSIVE;
	else
		new_state = CAN_STATE_BUS_OFF;

	/* state hasn't changed */
	if (likely(new_state == priv->can.state))
		return 0;

	skb = alloc_can_err_skb(dev, &cf);
	if (unlikely(!skb))
		return 0;

	do_state(dev, cf, new_state);
	priv->can.state = new_state;
	netif_receive_skb(skb);

	dev->stats.rx_packets++;
	dev->stats.rx_bytes += cf->can_dlc;

	return 1;
}

static void flexcan_read_fifo(const struct net_device *dev,
			      struct can_frame *cf)
{
	const struct flexcan_priv *priv = netdev_priv(dev);
	struct flexcan_regs __iomem *regs = priv->base;
	struct flexcan_mb __iomem *mb = &regs->cantxfg[0];
	u32 reg_ctrl, reg_id;

	reg_ctrl = readl(&mb->can_ctrl);
	reg_id = readl(&mb->can_id);
	if (reg_ctrl & FLEXCAN_MB_CNT_IDE)
		cf->can_id = ((reg_id >> 0) & CAN_EFF_MASK) | CAN_EFF_FLAG;
	else
		cf->can_id = (reg_id >> 18) & CAN_SFF_MASK;

	if (reg_ctrl & FLEXCAN_MB_CNT_RTR)
		cf->can_id |= CAN_RTR_FLAG;
	cf->can_dlc = get_can_dlc((reg_ctrl >> 16) & 0xf);

	*(__be32 *)(cf->data + 0) = cpu_to_be32(readl(&mb->data[0]));
	*(__be32 *)(cf->data + 4) = cpu_to_be32(readl(&mb->data[1]));

	/* mark as read */
	writel(FLEXCAN_IFLAG_RX_FIFO_AVAILABLE, &regs->iflag1);
	readl(&regs->timer);
}

static int flexcan_read_frame(struct net_device *dev)
{
	struct net_device_stats *stats = &dev->stats;
	struct can_frame *cf;
	struct sk_buff *skb;

	skb = alloc_can_skb(dev, &cf);
	if (unlikely(!skb)) {
		stats->rx_dropped++;
		return 0;
	}

	flexcan_read_fifo(dev, cf);
	netif_receive_skb(skb);

	stats->rx_packets++;
	stats->rx_bytes += cf->can_dlc;

	return 1;
}

static int flexcan_poll(struct napi_struct *napi, int quota)
{
	struct net_device *dev = napi->dev;
	const struct flexcan_priv *priv = netdev_priv(dev);
	struct flexcan_regs __iomem *regs = priv->base;
	u32 reg_iflag1, reg_esr;
	int work_done = 0;

	/*
	 * The error bits are cleared on read,
	 * use saved value from irq handler.
	 */
	reg_esr = readl(&regs->esr) | priv->reg_esr;

	/* handle state changes */
	work_done += flexcan_poll_state(dev, reg_esr);

	/* handle RX-FIFO */
	reg_iflag1 = readl(&regs->iflag1);
	while (reg_iflag1 & FLEXCAN_IFLAG_RX_FIFO_AVAILABLE &&
	       work_done < quota) {
		work_done += flexcan_read_frame(dev);
		reg_iflag1 = readl(&regs->iflag1);
	}

	/* report bus errors */
	if (flexcan_has_and_handle_berr(priv, reg_esr) && work_done < quota)
		work_done += flexcan_poll_bus_err(dev, reg_esr);

	if (work_done < quota) {
		napi_complete(napi);
		/* enable IRQs */
		writel(FLEXCAN_IFLAG_DEFAULT, &regs->imask1);
		writel(priv->reg_ctrl_default, &regs->ctrl);
	}

	return work_done;
}

static irqreturn_t flexcan_irq(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct net_device_stats *stats = &dev->stats;
	struct flexcan_priv *priv = netdev_priv(dev);
	struct flexcan_regs __iomem *regs = priv->base;
	u32 reg_iflag1, reg_esr;

	reg_iflag1 = readl(&regs->iflag1);
	reg_esr = readl(&regs->esr);
	writel(FLEXCAN_ESR_ERR_INT, &regs->esr);	/* ACK err IRQ */

	/*
	 * schedule NAPI in case of:
	 * - rx IRQ
	 * - state change IRQ
	 * - bus error IRQ and bus error reporting is activated
	 */
	if ((reg_iflag1 & FLEXCAN_IFLAG_RX_FIFO_AVAILABLE) ||
	    (reg_esr & FLEXCAN_ESR_ERR_STATE) ||
	    flexcan_has_and_handle_berr(priv, reg_esr)) {
		/*
		 * The error bits are cleared on read,
		 * save them for later use.
		 */
		priv->reg_esr = reg_esr & FLEXCAN_ESR_ERR_BUS;
		writel(FLEXCAN_IFLAG_DEFAULT & ~FLEXCAN_IFLAG_RX_FIFO_AVAILABLE,
		       &regs->imask1);
		writel(priv->reg_ctrl_default & ~FLEXCAN_CTRL_ERR_ALL,
		       &regs->ctrl);
		napi_schedule(&priv->napi);
	}

	/* FIFO overflow */
	if (reg_iflag1 & FLEXCAN_IFLAG_RX_FIFO_OVERFLOW) {
		writel(FLEXCAN_IFLAG_RX_FIFO_OVERFLOW, &regs->iflag1);
		dev->stats.rx_over_errors++;
		dev->stats.rx_errors++;
	}

	/* transmission complete interrupt */
	if (reg_iflag1 & (1 << FLEXCAN_TX_BUF_ID)) {
		/* tx_bytes is incremented in flexcan_start_xmit */
		stats->tx_packets++;
		writel((1 << FLEXCAN_TX_BUF_ID), &regs->iflag1);
		netif_wake_queue(dev);
	}

	return IRQ_HANDLED;
}

static void flexcan_set_bittiming(struct net_device *dev)
{
	const struct flexcan_priv *priv = netdev_priv(dev);
	const struct can_bittiming *bt = &priv->can.bittiming;
	struct flexcan_regs __iomem *regs = priv->base;
	u32 reg;

	reg = readl(&regs->ctrl);
	reg &= ~(FLEXCAN_CTRL_PRESDIV(0xff) |
		 FLEXCAN_CTRL_RJW(0x3) |
		 FLEXCAN_CTRL_PSEG1(0x7) |
		 FLEXCAN_CTRL_PSEG2(0x7) |
		 FLEXCAN_CTRL_PROPSEG(0x7) |
		 FLEXCAN_CTRL_LPB |
		 FLEXCAN_CTRL_SMP |
		 FLEXCAN_CTRL_LOM);

	reg |= FLEXCAN_CTRL_PRESDIV(bt->brp - 1) |
		FLEXCAN_CTRL_PSEG1(bt->phase_seg1 - 1) |
		FLEXCAN_CTRL_PSEG2(bt->phase_seg2 - 1) |
		FLEXCAN_CTRL_RJW(bt->sjw - 1) |
		FLEXCAN_CTRL_PROPSEG(bt->prop_seg - 1);

	if (priv->can.ctrlmode & CAN_CTRLMODE_LOOPBACK)
		reg |= FLEXCAN_CTRL_LPB;
	if (priv->can.ctrlmode & CAN_CTRLMODE_LISTENONLY)
		reg |= FLEXCAN_CTRL_LOM;
	if (priv->can.ctrlmode & CAN_CTRLMODE_3_SAMPLES)
		reg |= FLEXCAN_CTRL_SMP;

	dev_info(dev->dev.parent, "writing ctrl=0x%08x\n", reg);
	writel(reg, &regs->ctrl);

	/* print chip status */
	dev_dbg(dev->dev.parent, "%s: mcr=0x%08x ctrl=0x%08x\n", __func__,
		readl(&regs->mcr), readl(&regs->ctrl));
}

/*
 * flexcan_chip_start
 *
 * this functions is entered with clocks enabled
 *
 */
static int flexcan_chip_start(struct net_device *dev)
{
	struct flexcan_priv *priv = netdev_priv(dev);
	struct flexcan_regs __iomem *regs = priv->base;
	unsigned int i;
	int err;
	u32 reg_mcr, reg_ctrl;

	/* enable module */
	flexcan_chip_enable(priv);

	/* soft reset */
	writel(FLEXCAN_MCR_SOFTRST, &regs->mcr);
	udelay(10);

	reg_mcr = readl(&regs->mcr);
	if (reg_mcr & FLEXCAN_MCR_SOFTRST) {
		dev_err(dev->dev.parent,
			"Failed to softreset can module (mcr=0x%08x)\n",
			reg_mcr);
		err = -ENODEV;
		goto out;
	}

	flexcan_set_bittiming(dev);

	/*
	 * MCR
	 *
	 * enable freeze
	 * enable fifo
	 * halt now
	 * only supervisor access
	 * enable warning int
	 * choose format C
	 *
	 */
	reg_mcr = readl(&regs->mcr);
	reg_mcr |= FLEXCAN_MCR_FRZ | FLEXCAN_MCR_FEN | FLEXCAN_MCR_HALT |
		FLEXCAN_MCR_SUPV | FLEXCAN_MCR_WRN_EN |
		FLEXCAN_MCR_IDAM_C;
	dev_dbg(dev->dev.parent, "%s: writing mcr=0x%08x", __func__, reg_mcr);
	writel(reg_mcr, &regs->mcr);

	/*
	 * CTRL
	 *
	 * disable timer sync feature
	 *
	 * disable auto busoff recovery
	 * transmit lowest buffer first
	 *
	 * enable tx and rx warning interrupt
	 * enable bus off interrupt
	 * (== FLEXCAN_CTRL_ERR_STATE)
	 *
	 * _note_: we enable the "error interrupt"
	 * (FLEXCAN_CTRL_ERR_MSK), too. Otherwise we don't get any
	 * warning or bus passive interrupts.
	 */
	reg_ctrl = readl(&regs->ctrl);
	reg_ctrl &= ~FLEXCAN_CTRL_TSYN;
	reg_ctrl |= FLEXCAN_CTRL_BOFF_REC | FLEXCAN_CTRL_LBUF |
		FLEXCAN_CTRL_ERR_STATE | FLEXCAN_CTRL_ERR_MSK;

	/* save for later use */
	priv->reg_ctrl_default = reg_ctrl;
	dev_dbg(dev->dev.parent, "%s: writing ctrl=0x%08x", __func__, reg_ctrl);
	writel(reg_ctrl, &regs->ctrl);

	for (i = 0; i < ARRAY_SIZE(regs->cantxfg); i++) {
		writel(0, &regs->cantxfg[i].can_ctrl);
		writel(0, &regs->cantxfg[i].can_id);
		writel(0, &regs->cantxfg[i].data[0]);
		writel(0, &regs->cantxfg[i].data[1]);

		/* put MB into rx queue */
		writel(FLEXCAN_MB_CNT_CODE(0x4), &regs->cantxfg[i].can_ctrl);
	}

	/* acceptance mask/acceptance code (accept everything) */
	writel(0x0, &regs->rxgmask);
	writel(0x0, &regs->rx14mask);
	writel(0x0, &regs->rx15mask);

	flexcan_transceiver_switch(priv, 1);

	/* synchronize with the can bus */
	reg_mcr = readl(&regs->mcr);
	reg_mcr &= ~FLEXCAN_MCR_HALT;
	writel(reg_mcr, &regs->mcr);

	priv->can.state = CAN_STATE_ERROR_ACTIVE;

	/* enable FIFO interrupts */
	writel(FLEXCAN_IFLAG_DEFAULT, &regs->imask1);

	/* print chip status */
	dev_dbg(dev->dev.parent, "%s: reading mcr=0x%08x ctrl=0x%08x\n",
		__func__, readl(&regs->mcr), readl(&regs->ctrl));

	return 0;

 out:
	flexcan_chip_disable(priv);
	return err;
}

/*
 * flexcan_chip_stop
 *
 * this functions is entered with clocks enabled
 *
 */
static void flexcan_chip_stop(struct net_device *dev)
{
	struct flexcan_priv *priv = netdev_priv(dev);
	struct flexcan_regs __iomem *regs = priv->base;
	u32 reg;

	/* Disable all interrupts */
	writel(0, &regs->imask1);

	/* Disable + halt module */
	reg = readl(&regs->mcr);
	reg |= FLEXCAN_MCR_MDIS | FLEXCAN_MCR_HALT;
	writel(reg, &regs->mcr);

	flexcan_transceiver_switch(priv, 0);
	priv->can.state = CAN_STATE_STOPPED;

	return;
}

static int flexcan_open(struct net_device *dev)
{
	struct flexcan_priv *priv = netdev_priv(dev);
	int err;

	clk_enable(priv->clk);

	err = open_candev(dev);
	if (err)
		goto out;

	err = request_irq(dev->irq, flexcan_irq, IRQF_SHARED, dev->name, dev);
	if (err)
		goto out_close;

	/* start chip and queuing */
	err = flexcan_chip_start(dev);
	if (err)
		goto out_close;
	napi_enable(&priv->napi);
	netif_start_queue(dev);

	return 0;

 out_close:
	close_candev(dev);
 out:
	clk_disable(priv->clk);

	return err;
}

static int flexcan_close(struct net_device *dev)
{
	struct flexcan_priv *priv = netdev_priv(dev);

	netif_stop_queue(dev);
	napi_disable(&priv->napi);
	flexcan_chip_stop(dev);

	free_irq(dev->irq, dev);
	clk_disable(priv->clk);

	close_candev(dev);

	return 0;
}

static int flexcan_set_mode(struct net_device *dev, enum can_mode mode)
{
	int err;

	switch (mode) {
	case CAN_MODE_START:
		err = flexcan_chip_start(dev);
		if (err)
			return err;

		netif_wake_queue(dev);
		break;

	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static const struct net_device_ops flexcan_netdev_ops = {
	.ndo_open	= flexcan_open,
	.ndo_stop	= flexcan_close,
	.ndo_start_xmit	= flexcan_start_xmit,
};

static int __devinit register_flexcandev(struct net_device *dev)
{
	struct flexcan_priv *priv = netdev_priv(dev);
	struct flexcan_regs __iomem *regs = priv->base;
	u32 reg, err;

	clk_enable(priv->clk);

	/* select "bus clock", chip must be disabled */
	flexcan_chip_disable(priv);
	reg = readl(&regs->ctrl);
	reg |= FLEXCAN_CTRL_CLK_SRC;
	writel(reg, &regs->ctrl);

	flexcan_chip_enable(priv);

	/* set freeze, halt and activate FIFO, restrict register access */
	reg = readl(&regs->mcr);
	reg |= FLEXCAN_MCR_FRZ | FLEXCAN_MCR_HALT |
		FLEXCAN_MCR_FEN | FLEXCAN_MCR_SUPV;
	writel(reg, &regs->mcr);

	/*
	 * Currently we only support newer versions of this core
	 * featuring a RX FIFO. Older cores found on some Coldfire
	 * derivates are not yet supported.
	 */
	reg = readl(&regs->mcr);
	if (!(reg & FLEXCAN_MCR_FEN)) {
		dev_err(dev->dev.parent,
			"Could not enable RX FIFO, unsupported core\n");
		err = -ENODEV;
		goto out;
	}

	err = register_candev(dev);

 out:
	/* disable core and turn off clocks */
	flexcan_chip_disable(priv);
	clk_disable(priv->clk);

	return err;
}

static void __devexit unregister_flexcandev(struct net_device *dev)
{
	unregister_candev(dev);
}

static int __devinit flexcan_probe(struct platform_device *pdev)
{
	struct net_device *dev;
	struct flexcan_priv *priv;
	struct resource *mem;
	struct clk *clk;
	void __iomem *base;
	resource_size_t mem_size;
	int err, irq;

	clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "no clock defined\n");
		err = PTR_ERR(clk);
		goto failed_clock;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (!mem || irq <= 0) {
		err = -ENODEV;
		goto failed_get;
	}

	mem_size = resource_size(mem);
	if (!request_mem_region(mem->start, mem_size, pdev->name)) {
		err = -EBUSY;
		goto failed_req;
	}

	base = ioremap(mem->start, mem_size);
	if (!base) {
		err = -ENOMEM;
		goto failed_map;
	}

	dev = alloc_candev(sizeof(struct flexcan_priv), 0);
	if (!dev) {
		err = -ENOMEM;
		goto failed_alloc;
	}

	dev->netdev_ops = &flexcan_netdev_ops;
	dev->irq = irq;
	dev->flags |= IFF_ECHO; /* we support local echo in hardware */

	priv = netdev_priv(dev);
	priv->can.clock.freq = clk_get_rate(clk);
	priv->can.bittiming_const = &flexcan_bittiming_const;
	priv->can.do_set_mode = flexcan_set_mode;
	priv->can.do_get_berr_counter = flexcan_get_berr_counter;
	priv->can.ctrlmode_supported = CAN_CTRLMODE_LOOPBACK |
		CAN_CTRLMODE_LISTENONLY	| CAN_CTRLMODE_3_SAMPLES |
		CAN_CTRLMODE_BERR_REPORTING;
	priv->base = base;
	priv->dev = dev;
	priv->clk = clk;
	priv->pdata = pdev->dev.platform_data;

	netif_napi_add(dev, &priv->napi, flexcan_poll, FLEXCAN_NAPI_WEIGHT);

	dev_set_drvdata(&pdev->dev, dev);
	SET_NETDEV_DEV(dev, &pdev->dev);

	err = register_flexcandev(dev);
	if (err) {
		dev_err(&pdev->dev, "registering netdev failed\n");
		goto failed_register;
	}

	dev_info(&pdev->dev, "device registered (reg_base=%p, irq=%d)\n",
		 priv->base, dev->irq);

	return 0;

 failed_register:
	free_candev(dev);
 failed_alloc:
	iounmap(base);
 failed_map:
	release_mem_region(mem->start, mem_size);
 failed_req:
	clk_put(clk);
 failed_get:
 failed_clock:
	return err;
}

static int __devexit flexcan_remove(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct flexcan_priv *priv = netdev_priv(dev);
	struct resource *mem;

	unregister_flexcandev(dev);
	platform_set_drvdata(pdev, NULL);
	free_candev(dev);
	iounmap(priv->base);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(mem->start, resource_size(mem));

	clk_put(priv->clk);

	return 0;
}

static struct platform_driver flexcan_driver = {
	.driver.name = DRV_NAME,
	.probe = flexcan_probe,
	.remove = __devexit_p(flexcan_remove),
};

static int __init flexcan_init(void)
{
	pr_info("%s netdevice driver\n", DRV_NAME);
	return platform_driver_register(&flexcan_driver);
}

static void __exit flexcan_exit(void)
{
	platform_driver_unregister(&flexcan_driver);
	pr_info("%s: driver removed\n", DRV_NAME);
}

module_init(flexcan_init);
module_exit(flexcan_exit);

MODULE_AUTHOR("Sascha Hauer <kernel@pengutronix.de>, "
	      "Marc Kleine-Budde <kernel@pengutronix.de>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CAN port driver for flexcan based chip");
