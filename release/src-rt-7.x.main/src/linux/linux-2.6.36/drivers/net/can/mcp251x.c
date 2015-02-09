/*
 * CAN bus driver for Microchip 251x CAN Controller with SPI Interface
 *
 * MCP2510 support and bug fixes by Christian Pellegrin
 * <chripell@evolware.org>
 *
 * Copyright 2009 Christian Pellegrin EVOL S.r.l.
 *
 * Copyright 2007 Raymarine UK, Ltd. All Rights Reserved.
 * Written under contract by:
 *   Chris Elston, Katalix Systems, Ltd.
 *
 * Based on Microchip MCP251x CAN controller driver written by
 * David Vrabel, Copyright 2006 Arcom Control Systems Ltd.
 *
 * Based on CAN bus driver for the CCAN controller written by
 * - Sascha Hauer, Marc Kleine-Budde, Pengutronix
 * - Simon Kallweit, intefo AG
 * Copyright 2007
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the version 2 of the GNU General Public License
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *
 * Your platform definition file should specify something like:
 *
 * static struct mcp251x_platform_data mcp251x_info = {
 *         .oscillator_frequency = 8000000,
 *         .board_specific_setup = &mcp251x_setup,
 *         .model = CAN_MCP251X_MCP2510,
 *         .power_enable = mcp251x_power_enable,
 *         .transceiver_enable = NULL,
 * };
 *
 * static struct spi_board_info spi_board_info[] = {
 *         {
 *                 .modalias = "mcp251x",
 *                 .platform_data = &mcp251x_info,
 *                 .irq = IRQ_EINT13,
 *                 .max_speed_hz = 2*1000*1000,
 *                 .chip_select = 2,
 *         },
 * };
 *
 * Please see mcp251x.h for a description of the fields in
 * struct mcp251x_platform_data.
 *
 */

#include <linux/can/core.h>
#include <linux/can/dev.h>
#include <linux/can/platform/mcp251x.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/freezer.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>

/* SPI interface instruction set */
#define INSTRUCTION_WRITE	0x02
#define INSTRUCTION_READ	0x03
#define INSTRUCTION_BIT_MODIFY	0x05
#define INSTRUCTION_LOAD_TXB(n)	(0x40 + 2 * (n))
#define INSTRUCTION_READ_RXB(n)	(((n) == 0) ? 0x90 : 0x94)
#define INSTRUCTION_RESET	0xC0

/* MPC251x registers */
#define CANSTAT	      0x0e
#define CANCTRL	      0x0f
#  define CANCTRL_REQOP_MASK	    0xe0
#  define CANCTRL_REQOP_CONF	    0x80
#  define CANCTRL_REQOP_LISTEN_ONLY 0x60
#  define CANCTRL_REQOP_LOOPBACK    0x40
#  define CANCTRL_REQOP_SLEEP	    0x20
#  define CANCTRL_REQOP_NORMAL	    0x00
#  define CANCTRL_OSM		    0x08
#  define CANCTRL_ABAT		    0x10
#define TEC	      0x1c
#define REC	      0x1d
#define CNF1	      0x2a
#  define CNF1_SJW_SHIFT   6
#define CNF2	      0x29
#  define CNF2_BTLMODE	   0x80
#  define CNF2_SAM         0x40
#  define CNF2_PS1_SHIFT   3
#define CNF3	      0x28
#  define CNF3_SOF	   0x08
#  define CNF3_WAKFIL	   0x04
#  define CNF3_PHSEG2_MASK 0x07
#define CANINTE	      0x2b
#  define CANINTE_MERRE 0x80
#  define CANINTE_WAKIE 0x40
#  define CANINTE_ERRIE 0x20
#  define CANINTE_TX2IE 0x10
#  define CANINTE_TX1IE 0x08
#  define CANINTE_TX0IE 0x04
#  define CANINTE_RX1IE 0x02
#  define CANINTE_RX0IE 0x01
#define CANINTF	      0x2c
#  define CANINTF_MERRF 0x80
#  define CANINTF_WAKIF 0x40
#  define CANINTF_ERRIF 0x20
#  define CANINTF_TX2IF 0x10
#  define CANINTF_TX1IF 0x08
#  define CANINTF_TX0IF 0x04
#  define CANINTF_RX1IF 0x02
#  define CANINTF_RX0IF 0x01
#define EFLG	      0x2d
#  define EFLG_EWARN	0x01
#  define EFLG_RXWAR	0x02
#  define EFLG_TXWAR	0x04
#  define EFLG_RXEP	0x08
#  define EFLG_TXEP	0x10
#  define EFLG_TXBO	0x20
#  define EFLG_RX0OVR	0x40
#  define EFLG_RX1OVR	0x80
#define TXBCTRL(n)  (((n) * 0x10) + 0x30 + TXBCTRL_OFF)
#  define TXBCTRL_ABTF	0x40
#  define TXBCTRL_MLOA	0x20
#  define TXBCTRL_TXERR 0x10
#  define TXBCTRL_TXREQ 0x08
#define TXBSIDH(n)  (((n) * 0x10) + 0x30 + TXBSIDH_OFF)
#  define SIDH_SHIFT    3
#define TXBSIDL(n)  (((n) * 0x10) + 0x30 + TXBSIDL_OFF)
#  define SIDL_SID_MASK    7
#  define SIDL_SID_SHIFT   5
#  define SIDL_EXIDE_SHIFT 3
#  define SIDL_EID_SHIFT   16
#  define SIDL_EID_MASK    3
#define TXBEID8(n)  (((n) * 0x10) + 0x30 + TXBEID8_OFF)
#define TXBEID0(n)  (((n) * 0x10) + 0x30 + TXBEID0_OFF)
#define TXBDLC(n)   (((n) * 0x10) + 0x30 + TXBDLC_OFF)
#  define DLC_RTR_SHIFT    6
#define TXBCTRL_OFF 0
#define TXBSIDH_OFF 1
#define TXBSIDL_OFF 2
#define TXBEID8_OFF 3
#define TXBEID0_OFF 4
#define TXBDLC_OFF  5
#define TXBDAT_OFF  6
#define RXBCTRL(n)  (((n) * 0x10) + 0x60 + RXBCTRL_OFF)
#  define RXBCTRL_BUKT	0x04
#  define RXBCTRL_RXM0	0x20
#  define RXBCTRL_RXM1	0x40
#define RXBSIDH(n)  (((n) * 0x10) + 0x60 + RXBSIDH_OFF)
#  define RXBSIDH_SHIFT 3
#define RXBSIDL(n)  (((n) * 0x10) + 0x60 + RXBSIDL_OFF)
#  define RXBSIDL_IDE   0x08
#  define RXBSIDL_EID   3
#  define RXBSIDL_SHIFT 5
#define RXBEID8(n)  (((n) * 0x10) + 0x60 + RXBEID8_OFF)
#define RXBEID0(n)  (((n) * 0x10) + 0x60 + RXBEID0_OFF)
#define RXBDLC(n)   (((n) * 0x10) + 0x60 + RXBDLC_OFF)
#  define RXBDLC_LEN_MASK  0x0f
#  define RXBDLC_RTR       0x40
#define RXBCTRL_OFF 0
#define RXBSIDH_OFF 1
#define RXBSIDL_OFF 2
#define RXBEID8_OFF 3
#define RXBEID0_OFF 4
#define RXBDLC_OFF  5
#define RXBDAT_OFF  6
#define RXFSIDH(n) ((n) * 4)
#define RXFSIDL(n) ((n) * 4 + 1)
#define RXFEID8(n) ((n) * 4 + 2)
#define RXFEID0(n) ((n) * 4 + 3)
#define RXMSIDH(n) ((n) * 4 + 0x20)
#define RXMSIDL(n) ((n) * 4 + 0x21)
#define RXMEID8(n) ((n) * 4 + 0x22)
#define RXMEID0(n) ((n) * 4 + 0x23)

#define GET_BYTE(val, byte)			\
	(((val) >> ((byte) * 8)) & 0xff)
#define SET_BYTE(val, byte)			\
	(((val) & 0xff) << ((byte) * 8))

/*
 * Buffer size required for the largest SPI transfer (i.e., reading a
 * frame)
 */
#define CAN_FRAME_MAX_DATA_LEN	8
#define SPI_TRANSFER_BUF_LEN	(6 + CAN_FRAME_MAX_DATA_LEN)
#define CAN_FRAME_MAX_BITS	128

#define TX_ECHO_SKB_MAX	1

#define DEVICE_NAME "mcp251x"

static int mcp251x_enable_dma; /* Enable SPI DMA. Default: 0 (Off) */
module_param(mcp251x_enable_dma, int, S_IRUGO);
MODULE_PARM_DESC(mcp251x_enable_dma, "Enable SPI DMA. Default: 0 (Off)");

static struct can_bittiming_const mcp251x_bittiming_const = {
	.name = DEVICE_NAME,
	.tseg1_min = 3,
	.tseg1_max = 16,
	.tseg2_min = 2,
	.tseg2_max = 8,
	.sjw_max = 4,
	.brp_min = 1,
	.brp_max = 64,
	.brp_inc = 1,
};

struct mcp251x_priv {
	struct can_priv	   can;
	struct net_device *net;
	struct spi_device *spi;

	struct mutex mcp_lock; /* SPI device lock */

	u8 *spi_tx_buf;
	u8 *spi_rx_buf;
	dma_addr_t spi_tx_dma;
	dma_addr_t spi_rx_dma;

	struct sk_buff *tx_skb;
	int tx_len;

	struct workqueue_struct *wq;
	struct work_struct tx_work;
	struct work_struct restart_work;

	int force_quit;
	int after_suspend;
#define AFTER_SUSPEND_UP 1
#define AFTER_SUSPEND_DOWN 2
#define AFTER_SUSPEND_POWER 4
#define AFTER_SUSPEND_RESTART 8
	int restart_tx;
};

static void mcp251x_clean(struct net_device *net)
{
	struct mcp251x_priv *priv = netdev_priv(net);

	if (priv->tx_skb || priv->tx_len)
		net->stats.tx_errors++;
	if (priv->tx_skb)
		dev_kfree_skb(priv->tx_skb);
	if (priv->tx_len)
		can_free_echo_skb(priv->net, 0);
	priv->tx_skb = NULL;
	priv->tx_len = 0;
}

/*
 * Note about handling of error return of mcp251x_spi_trans: accessing
 * registers via SPI is not really different conceptually than using
 * normal I/O assembler instructions, although it's much more
 * complicated from a practical POV. So it's not advisable to always
 * check the return value of this function. Imagine that every
 * read{b,l}, write{b,l} and friends would be bracketed in "if ( < 0)
 * error();", it would be a great mess (well there are some situation
 * when exception handling C++ like could be useful after all). So we
 * just check that transfers are OK at the beginning of our
 * conversation with the chip and to avoid doing really nasty things
 * (like injecting bogus packets in the network stack).
 */
static int mcp251x_spi_trans(struct spi_device *spi, int len)
{
	struct mcp251x_priv *priv = dev_get_drvdata(&spi->dev);
	struct spi_transfer t = {
		.tx_buf = priv->spi_tx_buf,
		.rx_buf = priv->spi_rx_buf,
		.len = len,
		.cs_change = 0,
	};
	struct spi_message m;
	int ret;

	spi_message_init(&m);

	if (mcp251x_enable_dma) {
		t.tx_dma = priv->spi_tx_dma;
		t.rx_dma = priv->spi_rx_dma;
		m.is_dma_mapped = 1;
	}

	spi_message_add_tail(&t, &m);

	ret = spi_sync(spi, &m);
	if (ret)
		dev_err(&spi->dev, "spi transfer failed: ret = %d\n", ret);
	return ret;
}

static u8 mcp251x_read_reg(struct spi_device *spi, uint8_t reg)
{
	struct mcp251x_priv *priv = dev_get_drvdata(&spi->dev);
	u8 val = 0;

	priv->spi_tx_buf[0] = INSTRUCTION_READ;
	priv->spi_tx_buf[1] = reg;

	mcp251x_spi_trans(spi, 3);
	val = priv->spi_rx_buf[2];

	return val;
}

static void mcp251x_write_reg(struct spi_device *spi, u8 reg, uint8_t val)
{
	struct mcp251x_priv *priv = dev_get_drvdata(&spi->dev);

	priv->spi_tx_buf[0] = INSTRUCTION_WRITE;
	priv->spi_tx_buf[1] = reg;
	priv->spi_tx_buf[2] = val;

	mcp251x_spi_trans(spi, 3);
}

static void mcp251x_write_bits(struct spi_device *spi, u8 reg,
			       u8 mask, uint8_t val)
{
	struct mcp251x_priv *priv = dev_get_drvdata(&spi->dev);

	priv->spi_tx_buf[0] = INSTRUCTION_BIT_MODIFY;
	priv->spi_tx_buf[1] = reg;
	priv->spi_tx_buf[2] = mask;
	priv->spi_tx_buf[3] = val;

	mcp251x_spi_trans(spi, 4);
}

static void mcp251x_hw_tx_frame(struct spi_device *spi, u8 *buf,
				int len, int tx_buf_idx)
{
	struct mcp251x_platform_data *pdata = spi->dev.platform_data;
	struct mcp251x_priv *priv = dev_get_drvdata(&spi->dev);

	if (pdata->model == CAN_MCP251X_MCP2510) {
		int i;

		for (i = 1; i < TXBDAT_OFF + len; i++)
			mcp251x_write_reg(spi, TXBCTRL(tx_buf_idx) + i,
					  buf[i]);
	} else {
		memcpy(priv->spi_tx_buf, buf, TXBDAT_OFF + len);
		mcp251x_spi_trans(spi, TXBDAT_OFF + len);
	}
}

static void mcp251x_hw_tx(struct spi_device *spi, struct can_frame *frame,
			  int tx_buf_idx)
{
	u32 sid, eid, exide, rtr;
	u8 buf[SPI_TRANSFER_BUF_LEN];

	exide = (frame->can_id & CAN_EFF_FLAG) ? 1 : 0; /* Extended ID Enable */
	if (exide)
		sid = (frame->can_id & CAN_EFF_MASK) >> 18;
	else
		sid = frame->can_id & CAN_SFF_MASK; /* Standard ID */
	eid = frame->can_id & CAN_EFF_MASK; /* Extended ID */
	rtr = (frame->can_id & CAN_RTR_FLAG) ? 1 : 0; /* Remote transmission */

	buf[TXBCTRL_OFF] = INSTRUCTION_LOAD_TXB(tx_buf_idx);
	buf[TXBSIDH_OFF] = sid >> SIDH_SHIFT;
	buf[TXBSIDL_OFF] = ((sid & SIDL_SID_MASK) << SIDL_SID_SHIFT) |
		(exide << SIDL_EXIDE_SHIFT) |
		((eid >> SIDL_EID_SHIFT) & SIDL_EID_MASK);
	buf[TXBEID8_OFF] = GET_BYTE(eid, 1);
	buf[TXBEID0_OFF] = GET_BYTE(eid, 0);
	buf[TXBDLC_OFF] = (rtr << DLC_RTR_SHIFT) | frame->can_dlc;
	memcpy(buf + TXBDAT_OFF, frame->data, frame->can_dlc);
	mcp251x_hw_tx_frame(spi, buf, frame->can_dlc, tx_buf_idx);
	mcp251x_write_reg(spi, TXBCTRL(tx_buf_idx), TXBCTRL_TXREQ);
}

static void mcp251x_hw_rx_frame(struct spi_device *spi, u8 *buf,
				int buf_idx)
{
	struct mcp251x_priv *priv = dev_get_drvdata(&spi->dev);
	struct mcp251x_platform_data *pdata = spi->dev.platform_data;

	if (pdata->model == CAN_MCP251X_MCP2510) {
		int i, len;

		for (i = 1; i < RXBDAT_OFF; i++)
			buf[i] = mcp251x_read_reg(spi, RXBCTRL(buf_idx) + i);

		len = get_can_dlc(buf[RXBDLC_OFF] & RXBDLC_LEN_MASK);
		for (; i < (RXBDAT_OFF + len); i++)
			buf[i] = mcp251x_read_reg(spi, RXBCTRL(buf_idx) + i);
	} else {
		priv->spi_tx_buf[RXBCTRL_OFF] = INSTRUCTION_READ_RXB(buf_idx);
		mcp251x_spi_trans(spi, SPI_TRANSFER_BUF_LEN);
		memcpy(buf, priv->spi_rx_buf, SPI_TRANSFER_BUF_LEN);
	}
}

static void mcp251x_hw_rx(struct spi_device *spi, int buf_idx)
{
	struct mcp251x_priv *priv = dev_get_drvdata(&spi->dev);
	struct sk_buff *skb;
	struct can_frame *frame;
	u8 buf[SPI_TRANSFER_BUF_LEN];

	skb = alloc_can_skb(priv->net, &frame);
	if (!skb) {
		dev_err(&spi->dev, "cannot allocate RX skb\n");
		priv->net->stats.rx_dropped++;
		return;
	}

	mcp251x_hw_rx_frame(spi, buf, buf_idx);
	if (buf[RXBSIDL_OFF] & RXBSIDL_IDE) {
		/* Extended ID format */
		frame->can_id = CAN_EFF_FLAG;
		frame->can_id |=
			/* Extended ID part */
			SET_BYTE(buf[RXBSIDL_OFF] & RXBSIDL_EID, 2) |
			SET_BYTE(buf[RXBEID8_OFF], 1) |
			SET_BYTE(buf[RXBEID0_OFF], 0) |
			/* Standard ID part */
			(((buf[RXBSIDH_OFF] << RXBSIDH_SHIFT) |
			  (buf[RXBSIDL_OFF] >> RXBSIDL_SHIFT)) << 18);
		/* Remote transmission request */
		if (buf[RXBDLC_OFF] & RXBDLC_RTR)
			frame->can_id |= CAN_RTR_FLAG;
	} else {
		/* Standard ID format */
		frame->can_id =
			(buf[RXBSIDH_OFF] << RXBSIDH_SHIFT) |
			(buf[RXBSIDL_OFF] >> RXBSIDL_SHIFT);
	}
	/* Data length */
	frame->can_dlc = get_can_dlc(buf[RXBDLC_OFF] & RXBDLC_LEN_MASK);
	memcpy(frame->data, buf + RXBDAT_OFF, frame->can_dlc);

	priv->net->stats.rx_packets++;
	priv->net->stats.rx_bytes += frame->can_dlc;
	netif_rx(skb);
}

static void mcp251x_hw_sleep(struct spi_device *spi)
{
	mcp251x_write_reg(spi, CANCTRL, CANCTRL_REQOP_SLEEP);
}

static netdev_tx_t mcp251x_hard_start_xmit(struct sk_buff *skb,
					   struct net_device *net)
{
	struct mcp251x_priv *priv = netdev_priv(net);
	struct spi_device *spi = priv->spi;

	if (priv->tx_skb || priv->tx_len) {
		dev_warn(&spi->dev, "hard_xmit called while tx busy\n");
		return NETDEV_TX_BUSY;
	}

	if (can_dropped_invalid_skb(net, skb))
		return NETDEV_TX_OK;

	netif_stop_queue(net);
	priv->tx_skb = skb;
	queue_work(priv->wq, &priv->tx_work);

	return NETDEV_TX_OK;
}

static int mcp251x_do_set_mode(struct net_device *net, enum can_mode mode)
{
	struct mcp251x_priv *priv = netdev_priv(net);

	switch (mode) {
	case CAN_MODE_START:
		mcp251x_clean(net);
		/* We have to delay work since SPI I/O may sleep */
		priv->can.state = CAN_STATE_ERROR_ACTIVE;
		priv->restart_tx = 1;
		if (priv->can.restart_ms == 0)
			priv->after_suspend = AFTER_SUSPEND_RESTART;
		queue_work(priv->wq, &priv->restart_work);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int mcp251x_set_normal_mode(struct spi_device *spi)
{
	struct mcp251x_priv *priv = dev_get_drvdata(&spi->dev);
	unsigned long timeout;

	/* Enable interrupts */
	mcp251x_write_reg(spi, CANINTE,
			  CANINTE_ERRIE | CANINTE_TX2IE | CANINTE_TX1IE |
			  CANINTE_TX0IE | CANINTE_RX1IE | CANINTE_RX0IE);

	if (priv->can.ctrlmode & CAN_CTRLMODE_LOOPBACK) {
		/* Put device into loopback mode */
		mcp251x_write_reg(spi, CANCTRL, CANCTRL_REQOP_LOOPBACK);
	} else if (priv->can.ctrlmode & CAN_CTRLMODE_LISTENONLY) {
		/* Put device into listen-only mode */
		mcp251x_write_reg(spi, CANCTRL, CANCTRL_REQOP_LISTEN_ONLY);
	} else {
		/* Put device into normal mode */
		mcp251x_write_reg(spi, CANCTRL, CANCTRL_REQOP_NORMAL);

		/* Wait for the device to enter normal mode */
		timeout = jiffies + HZ;
		while (mcp251x_read_reg(spi, CANSTAT) & CANCTRL_REQOP_MASK) {
			schedule();
			if (time_after(jiffies, timeout)) {
				dev_err(&spi->dev, "MCP251x didn't"
					" enter in normal mode\n");
				return -EBUSY;
			}
		}
	}
	priv->can.state = CAN_STATE_ERROR_ACTIVE;
	return 0;
}

static int mcp251x_do_set_bittiming(struct net_device *net)
{
	struct mcp251x_priv *priv = netdev_priv(net);
	struct can_bittiming *bt = &priv->can.bittiming;
	struct spi_device *spi = priv->spi;

	mcp251x_write_reg(spi, CNF1, ((bt->sjw - 1) << CNF1_SJW_SHIFT) |
			  (bt->brp - 1));
	mcp251x_write_reg(spi, CNF2, CNF2_BTLMODE |
			  (priv->can.ctrlmode & CAN_CTRLMODE_3_SAMPLES ?
			   CNF2_SAM : 0) |
			  ((bt->phase_seg1 - 1) << CNF2_PS1_SHIFT) |
			  (bt->prop_seg - 1));
	mcp251x_write_bits(spi, CNF3, CNF3_PHSEG2_MASK,
			   (bt->phase_seg2 - 1));
	dev_info(&spi->dev, "CNF: 0x%02x 0x%02x 0x%02x\n",
		 mcp251x_read_reg(spi, CNF1),
		 mcp251x_read_reg(spi, CNF2),
		 mcp251x_read_reg(spi, CNF3));

	return 0;
}

static int mcp251x_setup(struct net_device *net, struct mcp251x_priv *priv,
			 struct spi_device *spi)
{
	mcp251x_do_set_bittiming(net);

	mcp251x_write_reg(spi, RXBCTRL(0),
			  RXBCTRL_BUKT | RXBCTRL_RXM0 | RXBCTRL_RXM1);
	mcp251x_write_reg(spi, RXBCTRL(1),
			  RXBCTRL_RXM0 | RXBCTRL_RXM1);
	return 0;
}

static int mcp251x_hw_reset(struct spi_device *spi)
{
	struct mcp251x_priv *priv = dev_get_drvdata(&spi->dev);
	int ret;
	unsigned long timeout;

	priv->spi_tx_buf[0] = INSTRUCTION_RESET;
	ret = spi_write(spi, priv->spi_tx_buf, 1);
	if (ret) {
		dev_err(&spi->dev, "reset failed: ret = %d\n", ret);
		return -EIO;
	}

	/* Wait for reset to finish */
	timeout = jiffies + HZ;
	mdelay(10);
	while ((mcp251x_read_reg(spi, CANSTAT) & CANCTRL_REQOP_MASK)
	       != CANCTRL_REQOP_CONF) {
		schedule();
		if (time_after(jiffies, timeout)) {
			dev_err(&spi->dev, "MCP251x didn't"
				" enter in conf mode after reset\n");
			return -EBUSY;
		}
	}
	return 0;
}

static int mcp251x_hw_probe(struct spi_device *spi)
{
	int st1, st2;

	mcp251x_hw_reset(spi);

	/*
	 * Please note that these are "magic values" based on after
	 * reset defaults taken from data sheet which allows us to see
	 * if we really have a chip on the bus (we avoid common all
	 * zeroes or all ones situations)
	 */
	st1 = mcp251x_read_reg(spi, CANSTAT) & 0xEE;
	st2 = mcp251x_read_reg(spi, CANCTRL) & 0x17;

	dev_dbg(&spi->dev, "CANSTAT 0x%02x CANCTRL 0x%02x\n", st1, st2);

	/* Check for power up default values */
	return (st1 == 0x80 && st2 == 0x07) ? 1 : 0;
}

static void mcp251x_open_clean(struct net_device *net)
{
	struct mcp251x_priv *priv = netdev_priv(net);
	struct spi_device *spi = priv->spi;
	struct mcp251x_platform_data *pdata = spi->dev.platform_data;

	free_irq(spi->irq, priv);
	mcp251x_hw_sleep(spi);
	if (pdata->transceiver_enable)
		pdata->transceiver_enable(0);
	close_candev(net);
}

static int mcp251x_stop(struct net_device *net)
{
	struct mcp251x_priv *priv = netdev_priv(net);
	struct spi_device *spi = priv->spi;
	struct mcp251x_platform_data *pdata = spi->dev.platform_data;

	close_candev(net);

	priv->force_quit = 1;
	free_irq(spi->irq, priv);
	destroy_workqueue(priv->wq);
	priv->wq = NULL;

	mutex_lock(&priv->mcp_lock);

	/* Disable and clear pending interrupts */
	mcp251x_write_reg(spi, CANINTE, 0x00);
	mcp251x_write_reg(spi, CANINTF, 0x00);

	mcp251x_write_reg(spi, TXBCTRL(0), 0);
	mcp251x_clean(net);

	mcp251x_hw_sleep(spi);

	if (pdata->transceiver_enable)
		pdata->transceiver_enable(0);

	priv->can.state = CAN_STATE_STOPPED;

	mutex_unlock(&priv->mcp_lock);

	return 0;
}

static void mcp251x_error_skb(struct net_device *net, int can_id, int data1)
{
	struct sk_buff *skb;
	struct can_frame *frame;

	skb = alloc_can_err_skb(net, &frame);
	if (skb) {
		frame->can_id = can_id;
		frame->data[1] = data1;
		netif_rx(skb);
	} else {
		dev_err(&net->dev,
			"cannot allocate error skb\n");
	}
}

static void mcp251x_tx_work_handler(struct work_struct *ws)
{
	struct mcp251x_priv *priv = container_of(ws, struct mcp251x_priv,
						 tx_work);
	struct spi_device *spi = priv->spi;
	struct net_device *net = priv->net;
	struct can_frame *frame;

	mutex_lock(&priv->mcp_lock);
	if (priv->tx_skb) {
		if (priv->can.state == CAN_STATE_BUS_OFF) {
			mcp251x_clean(net);
		} else {
			frame = (struct can_frame *)priv->tx_skb->data;

			if (frame->can_dlc > CAN_FRAME_MAX_DATA_LEN)
				frame->can_dlc = CAN_FRAME_MAX_DATA_LEN;
			mcp251x_hw_tx(spi, frame, 0);
			priv->tx_len = 1 + frame->can_dlc;
			can_put_echo_skb(priv->tx_skb, net, 0);
			priv->tx_skb = NULL;
		}
	}
	mutex_unlock(&priv->mcp_lock);
}

static void mcp251x_restart_work_handler(struct work_struct *ws)
{
	struct mcp251x_priv *priv = container_of(ws, struct mcp251x_priv,
						 restart_work);
	struct spi_device *spi = priv->spi;
	struct net_device *net = priv->net;

	mutex_lock(&priv->mcp_lock);
	if (priv->after_suspend) {
		mdelay(10);
		mcp251x_hw_reset(spi);
		mcp251x_setup(net, priv, spi);
		if (priv->after_suspend & AFTER_SUSPEND_RESTART) {
			mcp251x_set_normal_mode(spi);
		} else if (priv->after_suspend & AFTER_SUSPEND_UP) {
			netif_device_attach(net);
			mcp251x_clean(net);
			mcp251x_set_normal_mode(spi);
			netif_wake_queue(net);
		} else {
			mcp251x_hw_sleep(spi);
		}
		priv->after_suspend = 0;
		priv->force_quit = 0;
	}

	if (priv->restart_tx) {
		priv->restart_tx = 0;
		mcp251x_write_reg(spi, TXBCTRL(0), 0);
		mcp251x_clean(net);
		netif_wake_queue(net);
		mcp251x_error_skb(net, CAN_ERR_RESTARTED, 0);
	}
	mutex_unlock(&priv->mcp_lock);
}

static irqreturn_t mcp251x_can_ist(int irq, void *dev_id)
{
	struct mcp251x_priv *priv = dev_id;
	struct spi_device *spi = priv->spi;
	struct net_device *net = priv->net;

	mutex_lock(&priv->mcp_lock);
	while (!priv->force_quit) {
		enum can_state new_state;
		u8 intf = mcp251x_read_reg(spi, CANINTF);
		u8 eflag;
		int can_id = 0, data1 = 0;

		if (intf & CANINTF_RX0IF) {
			mcp251x_hw_rx(spi, 0);
			/* Free one buffer ASAP */
			mcp251x_write_bits(spi, CANINTF, intf & CANINTF_RX0IF,
					   0x00);
		}

		if (intf & CANINTF_RX1IF)
			mcp251x_hw_rx(spi, 1);

		mcp251x_write_bits(spi, CANINTF, intf, 0x00);

		eflag = mcp251x_read_reg(spi, EFLG);
		mcp251x_write_reg(spi, EFLG, 0x00);

		/* Update can state */
		if (eflag & EFLG_TXBO) {
			new_state = CAN_STATE_BUS_OFF;
			can_id |= CAN_ERR_BUSOFF;
		} else if (eflag & EFLG_TXEP) {
			new_state = CAN_STATE_ERROR_PASSIVE;
			can_id |= CAN_ERR_CRTL;
			data1 |= CAN_ERR_CRTL_TX_PASSIVE;
		} else if (eflag & EFLG_RXEP) {
			new_state = CAN_STATE_ERROR_PASSIVE;
			can_id |= CAN_ERR_CRTL;
			data1 |= CAN_ERR_CRTL_RX_PASSIVE;
		} else if (eflag & EFLG_TXWAR) {
			new_state = CAN_STATE_ERROR_WARNING;
			can_id |= CAN_ERR_CRTL;
			data1 |= CAN_ERR_CRTL_TX_WARNING;
		} else if (eflag & EFLG_RXWAR) {
			new_state = CAN_STATE_ERROR_WARNING;
			can_id |= CAN_ERR_CRTL;
			data1 |= CAN_ERR_CRTL_RX_WARNING;
		} else {
			new_state = CAN_STATE_ERROR_ACTIVE;
		}

		/* Update can state statistics */
		switch (priv->can.state) {
		case CAN_STATE_ERROR_ACTIVE:
			if (new_state >= CAN_STATE_ERROR_WARNING &&
			    new_state <= CAN_STATE_BUS_OFF)
				priv->can.can_stats.error_warning++;
		case CAN_STATE_ERROR_WARNING:	/* fallthrough */
			if (new_state >= CAN_STATE_ERROR_PASSIVE &&
			    new_state <= CAN_STATE_BUS_OFF)
				priv->can.can_stats.error_passive++;
			break;
		default:
			break;
		}
		priv->can.state = new_state;

		if (intf & CANINTF_ERRIF) {
			/* Handle overflow counters */
			if (eflag & (EFLG_RX0OVR | EFLG_RX1OVR)) {
				if (eflag & EFLG_RX0OVR)
					net->stats.rx_over_errors++;
				if (eflag & EFLG_RX1OVR)
					net->stats.rx_over_errors++;
				can_id |= CAN_ERR_CRTL;
				data1 |= CAN_ERR_CRTL_RX_OVERFLOW;
			}
			mcp251x_error_skb(net, can_id, data1);
		}

		if (priv->can.state == CAN_STATE_BUS_OFF) {
			if (priv->can.restart_ms == 0) {
				priv->force_quit = 1;
				can_bus_off(net);
				mcp251x_hw_sleep(spi);
				break;
			}
		}

		if (intf == 0)
			break;

		if (intf & (CANINTF_TX2IF | CANINTF_TX1IF | CANINTF_TX0IF)) {
			net->stats.tx_packets++;
			net->stats.tx_bytes += priv->tx_len - 1;
			if (priv->tx_len) {
				can_get_echo_skb(net, 0);
				priv->tx_len = 0;
			}
			netif_wake_queue(net);
		}

	}
	mutex_unlock(&priv->mcp_lock);
	return IRQ_HANDLED;
}

static int mcp251x_open(struct net_device *net)
{
	struct mcp251x_priv *priv = netdev_priv(net);
	struct spi_device *spi = priv->spi;
	struct mcp251x_platform_data *pdata = spi->dev.platform_data;
	int ret;

	ret = open_candev(net);
	if (ret) {
		dev_err(&spi->dev, "unable to set initial baudrate!\n");
		return ret;
	}

	mutex_lock(&priv->mcp_lock);
	if (pdata->transceiver_enable)
		pdata->transceiver_enable(1);

	priv->force_quit = 0;
	priv->tx_skb = NULL;
	priv->tx_len = 0;

	ret = request_threaded_irq(spi->irq, NULL, mcp251x_can_ist,
			  IRQF_TRIGGER_FALLING, DEVICE_NAME, priv);
	if (ret) {
		dev_err(&spi->dev, "failed to acquire irq %d\n", spi->irq);
		if (pdata->transceiver_enable)
			pdata->transceiver_enable(0);
		close_candev(net);
		goto open_unlock;
	}

	priv->wq = create_freezeable_workqueue("mcp251x_wq");
	INIT_WORK(&priv->tx_work, mcp251x_tx_work_handler);
	INIT_WORK(&priv->restart_work, mcp251x_restart_work_handler);

	ret = mcp251x_hw_reset(spi);
	if (ret) {
		mcp251x_open_clean(net);
		goto open_unlock;
	}
	ret = mcp251x_setup(net, priv, spi);
	if (ret) {
		mcp251x_open_clean(net);
		goto open_unlock;
	}
	ret = mcp251x_set_normal_mode(spi);
	if (ret) {
		mcp251x_open_clean(net);
		goto open_unlock;
	}
	netif_wake_queue(net);

open_unlock:
	mutex_unlock(&priv->mcp_lock);
	return ret;
}

static const struct net_device_ops mcp251x_netdev_ops = {
	.ndo_open = mcp251x_open,
	.ndo_stop = mcp251x_stop,
	.ndo_start_xmit = mcp251x_hard_start_xmit,
};

static int __devinit mcp251x_can_probe(struct spi_device *spi)
{
	struct net_device *net;
	struct mcp251x_priv *priv;
	struct mcp251x_platform_data *pdata = spi->dev.platform_data;
	int model = spi_get_device_id(spi)->driver_data;
	int ret = -ENODEV;

	if (!pdata)
		/* Platform data is required for osc freq */
		goto error_out;

	if (model)
		pdata->model = model;

	/* Allocate can/net device */
	net = alloc_candev(sizeof(struct mcp251x_priv), TX_ECHO_SKB_MAX);
	if (!net) {
		ret = -ENOMEM;
		goto error_alloc;
	}

	net->netdev_ops = &mcp251x_netdev_ops;
	net->flags |= IFF_ECHO;

	priv = netdev_priv(net);
	priv->can.bittiming_const = &mcp251x_bittiming_const;
	priv->can.do_set_mode = mcp251x_do_set_mode;
	priv->can.clock.freq = pdata->oscillator_frequency / 2;
	priv->can.ctrlmode_supported = CAN_CTRLMODE_3_SAMPLES |
		CAN_CTRLMODE_LOOPBACK | CAN_CTRLMODE_LISTENONLY;
	priv->net = net;
	dev_set_drvdata(&spi->dev, priv);

	priv->spi = spi;
	mutex_init(&priv->mcp_lock);

	/* If requested, allocate DMA buffers */
	if (mcp251x_enable_dma) {
		spi->dev.coherent_dma_mask = ~0;

		/*
		 * Minimum coherent DMA allocation is PAGE_SIZE, so allocate
		 * that much and share it between Tx and Rx DMA buffers.
		 */
		priv->spi_tx_buf = dma_alloc_coherent(&spi->dev,
						      PAGE_SIZE,
						      &priv->spi_tx_dma,
						      GFP_DMA);

		if (priv->spi_tx_buf) {
			priv->spi_rx_buf = (u8 *)(priv->spi_tx_buf +
						  (PAGE_SIZE / 2));
			priv->spi_rx_dma = (dma_addr_t)(priv->spi_tx_dma +
							(PAGE_SIZE / 2));
		} else {
			/* Fall back to non-DMA */
			mcp251x_enable_dma = 0;
		}
	}

	/* Allocate non-DMA buffers */
	if (!mcp251x_enable_dma) {
		priv->spi_tx_buf = kmalloc(SPI_TRANSFER_BUF_LEN, GFP_KERNEL);
		if (!priv->spi_tx_buf) {
			ret = -ENOMEM;
			goto error_tx_buf;
		}
		priv->spi_rx_buf = kmalloc(SPI_TRANSFER_BUF_LEN, GFP_KERNEL);
		if (!priv->spi_rx_buf) {
			ret = -ENOMEM;
			goto error_rx_buf;
		}
	}

	if (pdata->power_enable)
		pdata->power_enable(1);

	/* Call out to platform specific setup */
	if (pdata->board_specific_setup)
		pdata->board_specific_setup(spi);

	SET_NETDEV_DEV(net, &spi->dev);

	/* Configure the SPI bus */
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	spi_setup(spi);

	/* Here is OK to not lock the MCP, no one knows about it yet */
	if (!mcp251x_hw_probe(spi)) {
		dev_info(&spi->dev, "Probe failed\n");
		goto error_probe;
	}
	mcp251x_hw_sleep(spi);

	if (pdata->transceiver_enable)
		pdata->transceiver_enable(0);

	ret = register_candev(net);
	if (!ret) {
		dev_info(&spi->dev, "probed\n");
		return ret;
	}
error_probe:
	if (!mcp251x_enable_dma)
		kfree(priv->spi_rx_buf);
error_rx_buf:
	if (!mcp251x_enable_dma)
		kfree(priv->spi_tx_buf);
error_tx_buf:
	free_candev(net);
	if (mcp251x_enable_dma)
		dma_free_coherent(&spi->dev, PAGE_SIZE,
				  priv->spi_tx_buf, priv->spi_tx_dma);
error_alloc:
	if (pdata->power_enable)
		pdata->power_enable(0);
	dev_err(&spi->dev, "probe failed\n");
error_out:
	return ret;
}

static int __devexit mcp251x_can_remove(struct spi_device *spi)
{
	struct mcp251x_platform_data *pdata = spi->dev.platform_data;
	struct mcp251x_priv *priv = dev_get_drvdata(&spi->dev);
	struct net_device *net = priv->net;

	unregister_candev(net);
	free_candev(net);

	if (mcp251x_enable_dma) {
		dma_free_coherent(&spi->dev, PAGE_SIZE,
				  priv->spi_tx_buf, priv->spi_tx_dma);
	} else {
		kfree(priv->spi_tx_buf);
		kfree(priv->spi_rx_buf);
	}

	if (pdata->power_enable)
		pdata->power_enable(0);

	return 0;
}

#ifdef CONFIG_PM
static int mcp251x_can_suspend(struct spi_device *spi, pm_message_t state)
{
	struct mcp251x_platform_data *pdata = spi->dev.platform_data;
	struct mcp251x_priv *priv = dev_get_drvdata(&spi->dev);
	struct net_device *net = priv->net;

	priv->force_quit = 1;
	disable_irq(spi->irq);
	/*
	 * Note: at this point neither IST nor workqueues are running.
	 * open/stop cannot be called anyway so locking is not needed
	 */
	if (netif_running(net)) {
		netif_device_detach(net);

		mcp251x_hw_sleep(spi);
		if (pdata->transceiver_enable)
			pdata->transceiver_enable(0);
		priv->after_suspend = AFTER_SUSPEND_UP;
	} else {
		priv->after_suspend = AFTER_SUSPEND_DOWN;
	}

	if (pdata->power_enable) {
		pdata->power_enable(0);
		priv->after_suspend |= AFTER_SUSPEND_POWER;
	}

	return 0;
}

static int mcp251x_can_resume(struct spi_device *spi)
{
	struct mcp251x_platform_data *pdata = spi->dev.platform_data;
	struct mcp251x_priv *priv = dev_get_drvdata(&spi->dev);

	if (priv->after_suspend & AFTER_SUSPEND_POWER) {
		pdata->power_enable(1);
		queue_work(priv->wq, &priv->restart_work);
	} else {
		if (priv->after_suspend & AFTER_SUSPEND_UP) {
			if (pdata->transceiver_enable)
				pdata->transceiver_enable(1);
			queue_work(priv->wq, &priv->restart_work);
		} else {
			priv->after_suspend = 0;
		}
	}
	priv->force_quit = 0;
	enable_irq(spi->irq);
	return 0;
}
#else
#define mcp251x_can_suspend NULL
#define mcp251x_can_resume NULL
#endif

static struct spi_device_id mcp251x_id_table[] = {
	{ "mcp251x", 	0 /* Use pdata.model */ },
	{ "mcp2510",	CAN_MCP251X_MCP2510 },
	{ "mcp2515",	CAN_MCP251X_MCP2515 },
	{ },
};

MODULE_DEVICE_TABLE(spi, mcp251x_id_table);

static struct spi_driver mcp251x_can_driver = {
	.driver = {
		.name = DEVICE_NAME,
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
	},

	.id_table = mcp251x_id_table,
	.probe = mcp251x_can_probe,
	.remove = __devexit_p(mcp251x_can_remove),
	.suspend = mcp251x_can_suspend,
	.resume = mcp251x_can_resume,
};

static int __init mcp251x_can_init(void)
{
	return spi_register_driver(&mcp251x_can_driver);
}

static void __exit mcp251x_can_exit(void)
{
	spi_unregister_driver(&mcp251x_can_driver);
}

module_init(mcp251x_can_init);
module_exit(mcp251x_can_exit);

MODULE_AUTHOR("Chris Elston <celston@katalix.com>, "
	      "Christian Pellegrin <chripell@evolware.org>");
MODULE_DESCRIPTION("Microchip 251x CAN driver");
MODULE_LICENSE("GPL v2");
