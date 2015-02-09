/*
 * MUSB OTG controller driver for Blackfin Processors
 *
 * Copyright 2006-2008 Analog Devices Inc.
 *
 * Enter bugs at http://blackfin.uclinux.org/
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/gpio.h>
#include <linux/io.h>

#include <asm/cacheflush.h>

#include "musb_core.h"
#include "blackfin.h"

/*
 * Load an endpoint's FIFO
 */
void musb_write_fifo(struct musb_hw_ep *hw_ep, u16 len, const u8 *src)
{
	void __iomem *fifo = hw_ep->fifo;
	void __iomem *epio = hw_ep->regs;
	u8 epnum = hw_ep->epnum;

	prefetch((u8 *)src);

	musb_writew(epio, MUSB_TXCOUNT, len);

	DBG(4, "TX ep%d fifo %p count %d buf %p, epio %p\n",
			hw_ep->epnum, fifo, len, src, epio);

	dump_fifo_data(src, len);

	if (!ANOMALY_05000380 && epnum != 0) {
		u16 dma_reg;

		flush_dcache_range((unsigned long)src,
			(unsigned long)(src + len));

		/* Setup DMA address register */
		dma_reg = (u32)src;
		bfin_write16(USB_DMA_REG(epnum, USB_DMAx_ADDR_LOW), dma_reg);
		SSYNC();

		dma_reg = (u32)src >> 16;
		bfin_write16(USB_DMA_REG(epnum, USB_DMAx_ADDR_HIGH), dma_reg);
		SSYNC();

		/* Setup DMA count register */
		bfin_write16(USB_DMA_REG(epnum, USB_DMAx_COUNT_LOW), len);
		bfin_write16(USB_DMA_REG(epnum, USB_DMAx_COUNT_HIGH), 0);
		SSYNC();

		/* Enable the DMA */
		dma_reg = (epnum << 4) | DMA_ENA | INT_ENA | DIRECTION;
		bfin_write16(USB_DMA_REG(epnum, USB_DMAx_CTRL), dma_reg);
		SSYNC();

		/* Wait for compelete */
		while (!(bfin_read_USB_DMA_INTERRUPT() & (1 << epnum)))
			cpu_relax();

		/* acknowledge dma interrupt */
		bfin_write_USB_DMA_INTERRUPT(1 << epnum);
		SSYNC();

		/* Reset DMA */
		bfin_write16(USB_DMA_REG(epnum, USB_DMAx_CTRL), 0);
		SSYNC();
	} else {
		SSYNC();

		if (unlikely((unsigned long)src & 0x01))
			outsw_8((unsigned long)fifo, src, (len + 1) >> 1);
		else
			outsw((unsigned long)fifo, src, (len + 1) >> 1);
	}
}
/*
 * Unload an endpoint's FIFO
 */
void musb_read_fifo(struct musb_hw_ep *hw_ep, u16 len, u8 *dst)
{
	void __iomem *fifo = hw_ep->fifo;
	u8 epnum = hw_ep->epnum;

	if (ANOMALY_05000467 && epnum != 0) {
		u16 dma_reg;

		invalidate_dcache_range((unsigned long)dst,
			(unsigned long)(dst + len));

		/* Setup DMA address register */
		dma_reg = (u32)dst;
		bfin_write16(USB_DMA_REG(epnum, USB_DMAx_ADDR_LOW), dma_reg);
		SSYNC();

		dma_reg = (u32)dst >> 16;
		bfin_write16(USB_DMA_REG(epnum, USB_DMAx_ADDR_HIGH), dma_reg);
		SSYNC();

		/* Setup DMA count register */
		bfin_write16(USB_DMA_REG(epnum, USB_DMAx_COUNT_LOW), len);
		bfin_write16(USB_DMA_REG(epnum, USB_DMAx_COUNT_HIGH), 0);
		SSYNC();

		/* Enable the DMA */
		dma_reg = (epnum << 4) | DMA_ENA | INT_ENA;
		bfin_write16(USB_DMA_REG(epnum, USB_DMAx_CTRL), dma_reg);
		SSYNC();

		/* Wait for compelete */
		while (!(bfin_read_USB_DMA_INTERRUPT() & (1 << epnum)))
			cpu_relax();

		/* acknowledge dma interrupt */
		bfin_write_USB_DMA_INTERRUPT(1 << epnum);
		SSYNC();

		/* Reset DMA */
		bfin_write16(USB_DMA_REG(epnum, USB_DMAx_CTRL), 0);
		SSYNC();
	} else {
		SSYNC();
		/* Read the last byte of packet with odd size from address fifo + 4
		 * to trigger 1 byte access to EP0 FIFO.
		 */
		if (len == 1)
			*dst = (u8)inw((unsigned long)fifo + 4);
		else {
			if (unlikely((unsigned long)dst & 0x01))
				insw_8((unsigned long)fifo, dst, len >> 1);
			else
				insw((unsigned long)fifo, dst, len >> 1);

			if (len & 0x01)
				*(dst + len - 1) = (u8)inw((unsigned long)fifo + 4);
		}
	}
	DBG(4, "%cX ep%d fifo %p count %d buf %p\n",
			'R', hw_ep->epnum, fifo, len, dst);

	dump_fifo_data(dst, len);
}

static irqreturn_t blackfin_interrupt(int irq, void *__hci)
{
	unsigned long	flags;
	irqreturn_t	retval = IRQ_NONE;
	struct musb	*musb = __hci;

	spin_lock_irqsave(&musb->lock, flags);

	musb->int_usb = musb_readb(musb->mregs, MUSB_INTRUSB);
	musb->int_tx = musb_readw(musb->mregs, MUSB_INTRTX);
	musb->int_rx = musb_readw(musb->mregs, MUSB_INTRRX);

	if (musb->int_usb || musb->int_tx || musb->int_rx) {
		musb_writeb(musb->mregs, MUSB_INTRUSB, musb->int_usb);
		musb_writew(musb->mregs, MUSB_INTRTX, musb->int_tx);
		musb_writew(musb->mregs, MUSB_INTRRX, musb->int_rx);
		retval = musb_interrupt(musb);
	}

	/* Start sampling ID pin, when plug is removed from MUSB */
	if (is_otg_enabled(musb) && (musb->xceiv->state == OTG_STATE_B_IDLE
		|| musb->xceiv->state == OTG_STATE_A_WAIT_BCON)) {
		mod_timer(&musb_conn_timer, jiffies + TIMER_DELAY);
		musb->a_wait_bcon = TIMER_DELAY;
	}

	spin_unlock_irqrestore(&musb->lock, flags);

	return retval;
}

static void musb_conn_timer_handler(unsigned long _musb)
{
	struct musb *musb = (void *)_musb;
	unsigned long flags;
	u16 val;
	static u8 toggle;

	spin_lock_irqsave(&musb->lock, flags);
	switch (musb->xceiv->state) {
	case OTG_STATE_A_IDLE:
	case OTG_STATE_A_WAIT_BCON:
		/* Start a new session */
		val = musb_readw(musb->mregs, MUSB_DEVCTL);
		val &= ~MUSB_DEVCTL_SESSION;
		musb_writew(musb->mregs, MUSB_DEVCTL, val);
		val |= MUSB_DEVCTL_SESSION;
		musb_writew(musb->mregs, MUSB_DEVCTL, val);
		/* Check if musb is host or peripheral. */
		val = musb_readw(musb->mregs, MUSB_DEVCTL);

		if (!(val & MUSB_DEVCTL_BDEVICE)) {
			gpio_set_value(musb->config->gpio_vrsel, 1);
			musb->xceiv->state = OTG_STATE_A_WAIT_BCON;
		} else {
			gpio_set_value(musb->config->gpio_vrsel, 0);
			/* Ignore VBUSERROR and SUSPEND IRQ */
			val = musb_readb(musb->mregs, MUSB_INTRUSBE);
			val &= ~MUSB_INTR_VBUSERROR;
			musb_writeb(musb->mregs, MUSB_INTRUSBE, val);

			val = MUSB_INTR_SUSPEND | MUSB_INTR_VBUSERROR;
			musb_writeb(musb->mregs, MUSB_INTRUSB, val);
			if (is_otg_enabled(musb))
				musb->xceiv->state = OTG_STATE_B_IDLE;
			else
				musb_writeb(musb->mregs, MUSB_POWER, MUSB_POWER_HSENAB);
		}
		mod_timer(&musb_conn_timer, jiffies + TIMER_DELAY);
		break;
	case OTG_STATE_B_IDLE:

		if (!is_peripheral_enabled(musb))
			break;
		/* Start a new session.  It seems that MUSB needs taking
		 * some time to recognize the type of the plug inserted?
		 */
		val = musb_readw(musb->mregs, MUSB_DEVCTL);
		val |= MUSB_DEVCTL_SESSION;
		musb_writew(musb->mregs, MUSB_DEVCTL, val);
		val = musb_readw(musb->mregs, MUSB_DEVCTL);

		if (!(val & MUSB_DEVCTL_BDEVICE)) {
			gpio_set_value(musb->config->gpio_vrsel, 1);
			musb->xceiv->state = OTG_STATE_A_WAIT_BCON;
		} else {
			gpio_set_value(musb->config->gpio_vrsel, 0);

			/* Ignore VBUSERROR and SUSPEND IRQ */
			val = musb_readb(musb->mregs, MUSB_INTRUSBE);
			val &= ~MUSB_INTR_VBUSERROR;
			musb_writeb(musb->mregs, MUSB_INTRUSBE, val);

			val = MUSB_INTR_SUSPEND | MUSB_INTR_VBUSERROR;
			musb_writeb(musb->mregs, MUSB_INTRUSB, val);

			/* Toggle the Soft Conn bit, so that we can response to
			 * the inserting of either A-plug or B-plug.
			 */
			if (toggle) {
				val = musb_readb(musb->mregs, MUSB_POWER);
				val &= ~MUSB_POWER_SOFTCONN;
				musb_writeb(musb->mregs, MUSB_POWER, val);
				toggle = 0;
			} else {
				val = musb_readb(musb->mregs, MUSB_POWER);
				val |= MUSB_POWER_SOFTCONN;
				musb_writeb(musb->mregs, MUSB_POWER, val);
				toggle = 1;
			}
			/* The delay time is set to 1/4 second by default,
			 * shortening it, if accelerating A-plug detection
			 * is needed in OTG mode.
			 */
			mod_timer(&musb_conn_timer, jiffies + TIMER_DELAY / 4);
		}
		break;
	default:
		DBG(1, "%s state not handled\n", otg_state_string(musb));
		break;
	}
	spin_unlock_irqrestore(&musb->lock, flags);

	DBG(4, "state is %s\n", otg_state_string(musb));
}

void musb_platform_enable(struct musb *musb)
{
	if (!is_otg_enabled(musb) && is_host_enabled(musb)) {
		mod_timer(&musb_conn_timer, jiffies + TIMER_DELAY);
		musb->a_wait_bcon = TIMER_DELAY;
	}
}

void musb_platform_disable(struct musb *musb)
{
}

static void bfin_set_vbus(struct musb *musb, int is_on)
{
	int value = musb->config->gpio_vrsel_active;
	if (!is_on)
		value = !value;
	gpio_set_value(musb->config->gpio_vrsel, value);

	DBG(1, "VBUS %s, devctl %02x "
		/* otg %3x conf %08x prcm %08x */ "\n",
		otg_state_string(musb),
		musb_readb(musb->mregs, MUSB_DEVCTL));
}

static int bfin_set_power(struct otg_transceiver *x, unsigned mA)
{
	return 0;
}

void musb_platform_try_idle(struct musb *musb, unsigned long timeout)
{
	if (!is_otg_enabled(musb) && is_host_enabled(musb))
		mod_timer(&musb_conn_timer, jiffies + TIMER_DELAY);
}

int musb_platform_get_vbus_status(struct musb *musb)
{
	return 0;
}

int musb_platform_set_mode(struct musb *musb, u8 musb_mode)
{
	return -EIO;
}

int __init musb_platform_init(struct musb *musb, void *board_data)
{

	/*
	 * Rev 1.0 BF549 EZ-KITs require PE7 to be high for both DEVICE
	 * and OTG HOST modes, while rev 1.1 and greater require PE7 to
	 * be low for DEVICE mode and high for HOST mode. We set it high
	 * here because we are in host mode
	 */

	if (gpio_request(musb->config->gpio_vrsel, "USB_VRSEL")) {
		printk(KERN_ERR "Failed ro request USB_VRSEL GPIO_%d \n",
			musb->config->gpio_vrsel);
		return -ENODEV;
	}
	gpio_direction_output(musb->config->gpio_vrsel, 0);

	usb_nop_xceiv_register();
	musb->xceiv = otg_get_transceiver();
	if (!musb->xceiv) {
		gpio_free(musb->config->gpio_vrsel);
		return -ENODEV;
	}

	if (ANOMALY_05000346) {
		bfin_write_USB_APHY_CALIB(ANOMALY_05000346_value);
		SSYNC();
	}

	if (ANOMALY_05000347) {
		bfin_write_USB_APHY_CNTRL(0x0);
		SSYNC();
	}

	/* Configure PLL oscillator register */
	bfin_write_USB_PLLOSC_CTRL(0x30a8);
	SSYNC();

	bfin_write_USB_SRP_CLKDIV((get_sclk()/1000) / 32 - 1);
	SSYNC();

	bfin_write_USB_EP_NI0_RXMAXP(64);
	SSYNC();

	bfin_write_USB_EP_NI0_TXMAXP(64);
	SSYNC();

	/* Route INTRUSB/INTR_RX/INTR_TX to USB_INT0*/
	bfin_write_USB_GLOBINTR(0x7);
	SSYNC();

	bfin_write_USB_GLOBAL_CTL(GLOBAL_ENA | EP1_TX_ENA | EP2_TX_ENA |
				EP3_TX_ENA | EP4_TX_ENA | EP5_TX_ENA |
				EP6_TX_ENA | EP7_TX_ENA | EP1_RX_ENA |
				EP2_RX_ENA | EP3_RX_ENA | EP4_RX_ENA |
				EP5_RX_ENA | EP6_RX_ENA | EP7_RX_ENA);
	SSYNC();

	if (is_host_enabled(musb)) {
		musb->board_set_vbus = bfin_set_vbus;
		setup_timer(&musb_conn_timer,
			musb_conn_timer_handler, (unsigned long) musb);
	}
	if (is_peripheral_enabled(musb))
		musb->xceiv->set_power = bfin_set_power;

	musb->isr = blackfin_interrupt;

	return 0;
}

int musb_platform_exit(struct musb *musb)
{
	gpio_free(musb->config->gpio_vrsel);

	otg_put_transceiver(musb->xceiv);
	usb_nop_xceiv_unregister();
	return 0;
}
