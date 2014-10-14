/*
 * ci13xxx_udc.c - MIPS USB IP core family device controller
 *
 * Copyright (C) 2008 Chipidea - MIPS Technologies, Inc. All rights reserved.
 *
 * Author: David Lopo
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * Description: MIPS USB IP core family device controller
 *              Currently it only supports IP part number CI13412
 *
 * This driver is composed of several blocks:
 * - HW:     hardware interface
 * - DBG:    debug facilities (optional)
 * - UTIL:   utilities
 * - ISR:    interrupts handling
 * - ENDPT:  endpoint operations (Gadget API)
 * - GADGET: gadget operations (Gadget API)
 * - BUS:    bus glue code, bus abstraction layer
 *
 * Compile Options
 * - CONFIG_USB_GADGET_DEBUG_FILES: enable debug facilities
 * - STALL_IN:  non-empty bulk-in pipes cannot be halted
 *              if defined mass storage compliance succeeds but with warnings
 *              => case 4: Hi >  Dn
 *              => case 5: Hi >  Di
 *              => case 8: Hi <> Do
 *              if undefined usbtest 13 fails
 * - TRACE:     enable function tracing (depends on DEBUG)
 *
 * Main Features
 * - Chapter 9 & Mass Storage Compliance with Gadget File Storage
 * - Chapter 9 Compliance with Gadget Zero (STALL_IN undefined)
 * - Normal & LPM support
 *
 * USBTEST Report
 * - OK: 0-12, 13 (STALL_IN defined) & 14
 * - Not Supported: 15 & 16 (ISO)
 *
 * TODO List
 * - OTG
 * - Isochronous & Interrupt Traffic
 * - Handle requests which spawns into several TDs
 * - GET_STATUS(device) - always reports 0
 * - Gadget API (majority of optional features)
 * - Suspend & Remote Wakeup
 */
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/otg.h>

#include "ci13xxx_udc.h"


/******************************************************************************
 * DEFINE
 *****************************************************************************/
/* ctrl register bank access */
static DEFINE_SPINLOCK(udc_lock);

/* control endpoint description */
static const struct usb_endpoint_descriptor
ctrl_endpt_out_desc = {
	.bLength         = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes    = USB_ENDPOINT_XFER_CONTROL,
	.wMaxPacketSize  = cpu_to_le16(CTRL_PAYLOAD_MAX),
};

static const struct usb_endpoint_descriptor
ctrl_endpt_in_desc = {
	.bLength         = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes    = USB_ENDPOINT_XFER_CONTROL,
	.wMaxPacketSize  = cpu_to_le16(CTRL_PAYLOAD_MAX),
};

/* UDC descriptor */
static struct ci13xxx *_udc;

/* Interrupt statistics */
#define ISR_MASK   0x1F
static struct {
	u32 test;
	u32 ui;
	u32 uei;
	u32 pci;
	u32 uri;
	u32 sli;
	u32 none;
	struct {
		u32 cnt;
		u32 buf[ISR_MASK+1];
		u32 idx;
	} hndl;
} isr_statistics;

/**
 * ffs_nr: find first (least significant) bit set
 * @x: the word to search
 *
 * This function returns bit number (instead of position)
 */
static int ffs_nr(u32 x)
{
	int n = ffs(x);

	return n ? n-1 : 32;
}

/******************************************************************************
 * HW block
 *****************************************************************************/
/* register bank descriptor */
static struct {
	unsigned      lpm;    /* is LPM? */
	void __iomem *abs;    /* bus map offset */
	void __iomem *cap;    /* bus map offset + CAP offset + CAP data */
	size_t        size;   /* bank size */
} hw_bank;

/* MSM specific */
#define ABS_AHBBURST        (0x0090UL)
#define ABS_AHBMODE         (0x0098UL)
/* UDC register map */
#define ABS_CAPLENGTH       (0x100UL)
#define ABS_HCCPARAMS       (0x108UL)
#define ABS_DCCPARAMS       (0x124UL)
#define ABS_TESTMODE        (hw_bank.lpm ? 0x0FCUL : 0x138UL)
/* offset to CAPLENTGH (addr + data) */
#define CAP_USBCMD          (0x000UL)
#define CAP_USBSTS          (0x004UL)
#define CAP_USBINTR         (0x008UL)
#define CAP_DEVICEADDR      (0x014UL)
#define CAP_ENDPTLISTADDR   (0x018UL)
#define CAP_PORTSC          (0x044UL)
#define CAP_DEVLC           (0x084UL)
#define CAP_USBMODE         (hw_bank.lpm ? 0x0C8UL : 0x068UL)
#define CAP_ENDPTSETUPSTAT  (hw_bank.lpm ? 0x0D8UL : 0x06CUL)
#define CAP_ENDPTPRIME      (hw_bank.lpm ? 0x0DCUL : 0x070UL)
#define CAP_ENDPTFLUSH      (hw_bank.lpm ? 0x0E0UL : 0x074UL)
#define CAP_ENDPTSTAT       (hw_bank.lpm ? 0x0E4UL : 0x078UL)
#define CAP_ENDPTCOMPLETE   (hw_bank.lpm ? 0x0E8UL : 0x07CUL)
#define CAP_ENDPTCTRL       (hw_bank.lpm ? 0x0ECUL : 0x080UL)
#define CAP_LAST            (hw_bank.lpm ? 0x12CUL : 0x0C0UL)

/* maximum number of enpoints: valid only after hw_device_reset() */
static unsigned hw_ep_max;

/**
 * hw_ep_bit: calculates the bit number
 * @num: endpoint number
 * @dir: endpoint direction
 *
 * This function returns bit number
 */
static inline int hw_ep_bit(int num, int dir)
{
	return num + (dir ? 16 : 0);
}

/**
 * hw_aread: reads from register bitfield
 * @addr: address relative to bus map
 * @mask: bitfield mask
 *
 * This function returns register bitfield data
 */
static u32 hw_aread(u32 addr, u32 mask)
{
	return ioread32(addr + hw_bank.abs) & mask;
}

/**
 * hw_awrite: writes to register bitfield
 * @addr: address relative to bus map
 * @mask: bitfield mask
 * @data: new data
 */
static void hw_awrite(u32 addr, u32 mask, u32 data)
{
	iowrite32(hw_aread(addr, ~mask) | (data & mask),
		  addr + hw_bank.abs);
}

/**
 * hw_cread: reads from register bitfield
 * @addr: address relative to CAP offset plus content
 * @mask: bitfield mask
 *
 * This function returns register bitfield data
 */
static u32 hw_cread(u32 addr, u32 mask)
{
	return ioread32(addr + hw_bank.cap) & mask;
}

/**
 * hw_cwrite: writes to register bitfield
 * @addr: address relative to CAP offset plus content
 * @mask: bitfield mask
 * @data: new data
 */
static void hw_cwrite(u32 addr, u32 mask, u32 data)
{
	iowrite32(hw_cread(addr, ~mask) | (data & mask),
		  addr + hw_bank.cap);
}

/**
 * hw_ctest_and_clear: tests & clears register bitfield
 * @addr: address relative to CAP offset plus content
 * @mask: bitfield mask
 *
 * This function returns register bitfield data
 */
static u32 hw_ctest_and_clear(u32 addr, u32 mask)
{
	u32 reg = hw_cread(addr, mask);

	iowrite32(reg, addr + hw_bank.cap);
	return reg;
}

/**
 * hw_ctest_and_write: tests & writes register bitfield
 * @addr: address relative to CAP offset plus content
 * @mask: bitfield mask
 * @data: new data
 *
 * This function returns register bitfield data
 */
static u32 hw_ctest_and_write(u32 addr, u32 mask, u32 data)
{
	u32 reg = hw_cread(addr, ~0);

	iowrite32((reg & ~mask) | (data & mask), addr + hw_bank.cap);
	return (reg & mask) >> ffs_nr(mask);
}

static int hw_device_init(void __iomem *base)
{
	u32 reg;

	/* bank is a module variable */
	hw_bank.abs = base;

	hw_bank.cap = hw_bank.abs;
	hw_bank.cap += ABS_CAPLENGTH;
	hw_bank.cap += ioread8(hw_bank.cap);

	reg = hw_aread(ABS_HCCPARAMS, HCCPARAMS_LEN) >> ffs_nr(HCCPARAMS_LEN);
	hw_bank.lpm  = reg;
	hw_bank.size = hw_bank.cap - hw_bank.abs;
	hw_bank.size += CAP_LAST;
	hw_bank.size /= sizeof(u32);

	reg = hw_aread(ABS_DCCPARAMS, DCCPARAMS_DEN) >> ffs_nr(DCCPARAMS_DEN);
	hw_ep_max = reg * 2;   /* cache hw ENDPT_MAX */

	if (hw_ep_max == 0 || hw_ep_max > ENDPT_MAX)
		return -ENODEV;

	/* setup lock mode ? */

	/* ENDPTSETUPSTAT is '0' by default */

	/* HCSPARAMS.bf.ppc SHOULD BE zero for device */

	return 0;
}
/**
 * hw_device_reset: resets chip (execute without interruption)
 * @base: register base address
 *
 * This function returns an error code
 */
static int hw_device_reset(struct ci13xxx *udc)
{
	/* should flush & stop before reset */
	hw_cwrite(CAP_ENDPTFLUSH, ~0, ~0);
	hw_cwrite(CAP_USBCMD, USBCMD_RS, 0);

	hw_cwrite(CAP_USBCMD, USBCMD_RST, USBCMD_RST);
	while (hw_cread(CAP_USBCMD, USBCMD_RST))
		udelay(10);             /* not RTOS friendly */


	if (udc->udc_driver->notify_event)
		udc->udc_driver->notify_event(udc,
			CI13XXX_CONTROLLER_RESET_EVENT);

	if (udc->udc_driver->flags && CI13XXX_DISABLE_STREAMING)
		hw_cwrite(CAP_USBMODE, USBMODE_SDIS, USBMODE_SDIS);

	/* USBMODE should be configured step by step */
	hw_cwrite(CAP_USBMODE, USBMODE_CM, USBMODE_CM_IDLE);
	hw_cwrite(CAP_USBMODE, USBMODE_CM, USBMODE_CM_DEVICE);
	hw_cwrite(CAP_USBMODE, USBMODE_SLOM, USBMODE_SLOM);  /* HW >= 2.3 */

	if (hw_cread(CAP_USBMODE, USBMODE_CM) != USBMODE_CM_DEVICE) {
		pr_err("cannot enter in device mode");
		pr_err("lpm = %i", hw_bank.lpm);
		return -ENODEV;
	}

	return 0;
}

/**
 * hw_device_state: enables/disables interrupts & starts/stops device (execute
 *                  without interruption)
 * @dma: 0 => disable, !0 => enable and set dma engine
 *
 * This function returns an error code
 */
static int hw_device_state(u32 dma)
{
	if (dma) {
		hw_cwrite(CAP_ENDPTLISTADDR, ~0, dma);
		/* interrupt, error, port change, reset, sleep/suspend */
		hw_cwrite(CAP_USBINTR, ~0,
			     USBi_UI|USBi_UEI|USBi_PCI|USBi_URI|USBi_SLI);
		hw_cwrite(CAP_USBCMD, USBCMD_RS, USBCMD_RS);
	} else {
		hw_cwrite(CAP_USBCMD, USBCMD_RS, 0);
		hw_cwrite(CAP_USBINTR, ~0, 0);
	}
	return 0;
}

/**
 * hw_ep_flush: flush endpoint fifo (execute without interruption)
 * @num: endpoint number
 * @dir: endpoint direction
 *
 * This function returns an error code
 */
static int hw_ep_flush(int num, int dir)
{
	int n = hw_ep_bit(num, dir);

	do {
		/* flush any pending transfer */
		hw_cwrite(CAP_ENDPTFLUSH, BIT(n), BIT(n));
		while (hw_cread(CAP_ENDPTFLUSH, BIT(n)))
			cpu_relax();
	} while (hw_cread(CAP_ENDPTSTAT, BIT(n)));

	return 0;
}

/**
 * hw_ep_disable: disables endpoint (execute without interruption)
 * @num: endpoint number
 * @dir: endpoint direction
 *
 * This function returns an error code
 */
static int hw_ep_disable(int num, int dir)
{
	hw_ep_flush(num, dir);
	hw_cwrite(CAP_ENDPTCTRL + num * sizeof(u32),
		  dir ? ENDPTCTRL_TXE : ENDPTCTRL_RXE, 0);
	return 0;
}

/**
 * hw_ep_enable: enables endpoint (execute without interruption)
 * @num:  endpoint number
 * @dir:  endpoint direction
 * @type: endpoint type
 *
 * This function returns an error code
 */
static int hw_ep_enable(int num, int dir, int type)
{
	u32 mask, data;

	if (dir) {
		mask  = ENDPTCTRL_TXT;  /* type    */
		data  = type << ffs_nr(mask);

		mask |= ENDPTCTRL_TXS;  /* unstall */
		mask |= ENDPTCTRL_TXR;  /* reset data toggle */
		data |= ENDPTCTRL_TXR;
		mask |= ENDPTCTRL_TXE;  /* enable  */
		data |= ENDPTCTRL_TXE;
	} else {
		mask  = ENDPTCTRL_RXT;  /* type    */
		data  = type << ffs_nr(mask);

		mask |= ENDPTCTRL_RXS;  /* unstall */
		mask |= ENDPTCTRL_RXR;  /* reset data toggle */
		data |= ENDPTCTRL_RXR;
		mask |= ENDPTCTRL_RXE;  /* enable  */
		data |= ENDPTCTRL_RXE;
	}
	hw_cwrite(CAP_ENDPTCTRL + num * sizeof(u32), mask, data);
	return 0;
}

/**
 * hw_ep_get_halt: return endpoint halt status
 * @num: endpoint number
 * @dir: endpoint direction
 *
 * This function returns 1 if endpoint halted
 */
static int hw_ep_get_halt(int num, int dir)
{
	u32 mask = dir ? ENDPTCTRL_TXS : ENDPTCTRL_RXS;

	return hw_cread(CAP_ENDPTCTRL + num * sizeof(u32), mask) ? 1 : 0;
}

/**
 * hw_test_and_clear_setup_status: test & clear setup status (execute without
 *                                 interruption)
 * @n: bit number (endpoint)
 *
 * This function returns setup status
 */
static int hw_test_and_clear_setup_status(int n)
{
	return hw_ctest_and_clear(CAP_ENDPTSETUPSTAT, BIT(n));
}

/**
 * hw_ep_prime: primes endpoint (execute without interruption)
 * @num:     endpoint number
 * @dir:     endpoint direction
 * @is_ctrl: true if control endpoint
 *
 * This function returns an error code
 */
static int hw_ep_prime(int num, int dir, int is_ctrl)
{
	int n = hw_ep_bit(num, dir);

	if (is_ctrl && dir == RX && hw_cread(CAP_ENDPTSETUPSTAT, BIT(num)))
		return -EAGAIN;

	hw_cwrite(CAP_ENDPTPRIME, BIT(n), BIT(n));

	while (hw_cread(CAP_ENDPTPRIME, BIT(n)))
		cpu_relax();
	if (is_ctrl && dir == RX  && hw_cread(CAP_ENDPTSETUPSTAT, BIT(num)))
		return -EAGAIN;

	/* status shoult be tested according with manual but it doesn't work */
	return 0;
}

/**
 * hw_ep_set_halt: configures ep halt & resets data toggle after clear (execute
 *                 without interruption)
 * @num:   endpoint number
 * @dir:   endpoint direction
 * @value: true => stall, false => unstall
 *
 * This function returns an error code
 */
static int hw_ep_set_halt(int num, int dir, int value)
{
	if (value != 0 && value != 1)
		return -EINVAL;

	do {
		u32 addr = CAP_ENDPTCTRL + num * sizeof(u32);
		u32 mask_xs = dir ? ENDPTCTRL_TXS : ENDPTCTRL_RXS;
		u32 mask_xr = dir ? ENDPTCTRL_TXR : ENDPTCTRL_RXR;

		/* data toggle - reserved for EP0 but it's in ESS */
		hw_cwrite(addr, mask_xs|mask_xr, value ? mask_xs : mask_xr);

	} while (value != hw_ep_get_halt(num, dir));

	return 0;
}

/**
 * hw_intr_clear: disables interrupt & clears interrupt status (execute without
 *                interruption)
 * @n: interrupt bit
 *
 * This function returns an error code
 */
static int hw_intr_clear(int n)
{
	if (n >= REG_BITS)
		return -EINVAL;

	hw_cwrite(CAP_USBINTR, BIT(n), 0);
	hw_cwrite(CAP_USBSTS,  BIT(n), BIT(n));
	return 0;
}

/**
 * hw_intr_force: enables interrupt & forces interrupt status (execute without
 *                interruption)
 * @n: interrupt bit
 *
 * This function returns an error code
 */
static int hw_intr_force(int n)
{
	if (n >= REG_BITS)
		return -EINVAL;

	hw_awrite(ABS_TESTMODE, TESTMODE_FORCE, TESTMODE_FORCE);
	hw_cwrite(CAP_USBINTR,  BIT(n), BIT(n));
	hw_cwrite(CAP_USBSTS,   BIT(n), BIT(n));
	hw_awrite(ABS_TESTMODE, TESTMODE_FORCE, 0);
	return 0;
}

/**
 * hw_is_port_high_speed: test if port is high speed
 *
 * This function returns true if high speed port
 */
static int hw_port_is_high_speed(void)
{
	return hw_bank.lpm ? hw_cread(CAP_DEVLC, DEVLC_PSPD) :
		hw_cread(CAP_PORTSC, PORTSC_HSP);
}

/**
 * hw_port_test_get: reads port test mode value
 *
 * This function returns port test mode value
 */
static u8 hw_port_test_get(void)
{
	return hw_cread(CAP_PORTSC, PORTSC_PTC) >> ffs_nr(PORTSC_PTC);
}

/**
 * hw_port_test_set: writes port test mode (execute without interruption)
 * @mode: new value
 *
 * This function returns an error code
 */
static int hw_port_test_set(u8 mode)
{
	const u8 TEST_MODE_MAX = 7;

	if (mode > TEST_MODE_MAX)
		return -EINVAL;

	hw_cwrite(CAP_PORTSC, PORTSC_PTC, mode << ffs_nr(PORTSC_PTC));
	return 0;
}

/**
 * hw_read_intr_enable: returns interrupt enable register
 *
 * This function returns register data
 */
static u32 hw_read_intr_enable(void)
{
	return hw_cread(CAP_USBINTR, ~0);
}

/**
 * hw_read_intr_status: returns interrupt status register
 *
 * This function returns register data
 */
static u32 hw_read_intr_status(void)
{
	return hw_cread(CAP_USBSTS, ~0);
}

/**
 * hw_register_read: reads all device registers (execute without interruption)
 * @buf:  destination buffer
 * @size: buffer size
 *
 * This function returns number of registers read
 */
static size_t hw_register_read(u32 *buf, size_t size)
{
	unsigned i;

	if (size > hw_bank.size)
		size = hw_bank.size;

	for (i = 0; i < size; i++)
		buf[i] = hw_aread(i * sizeof(u32), ~0);

	return size;
}

/**
 * hw_register_write: writes to register
 * @addr: register address
 * @data: register value
 *
 * This function returns an error code
 */
static int hw_register_write(u16 addr, u32 data)
{
	/* align */
	addr /= sizeof(u32);

	if (addr >= hw_bank.size)
		return -EINVAL;

	/* align */
	addr *= sizeof(u32);

	hw_awrite(addr, ~0, data);
	return 0;
}

/**
 * hw_test_and_clear_complete: test & clear complete status (execute without
 *                             interruption)
 * @n: bit number (endpoint)
 *
 * This function returns complete status
 */
static int hw_test_and_clear_complete(int n)
{
	return hw_ctest_and_clear(CAP_ENDPTCOMPLETE, BIT(n));
}

/**
 * hw_test_and_clear_intr_active: test & clear active interrupts (execute
 *                                without interruption)
 *
 * This function returns active interrutps
 */
static u32 hw_test_and_clear_intr_active(void)
{
	u32 reg = hw_read_intr_status() & hw_read_intr_enable();

	hw_cwrite(CAP_USBSTS, ~0, reg);
	return reg;
}

/**
 * hw_test_and_clear_setup_guard: test & clear setup guard (execute without
 *                                interruption)
 *
 * This function returns guard value
 */
static int hw_test_and_clear_setup_guard(void)
{
	return hw_ctest_and_write(CAP_USBCMD, USBCMD_SUTW, 0);
}

/**
 * hw_test_and_set_setup_guard: test & set setup guard (execute without
 *                              interruption)
 *
 * This function returns guard value
 */
static int hw_test_and_set_setup_guard(void)
{
	return hw_ctest_and_write(CAP_USBCMD, USBCMD_SUTW, USBCMD_SUTW);
}

/**
 * hw_usb_set_address: configures USB address (execute without interruption)
 * @value: new USB address
 *
 * This function returns an error code
 */
static int hw_usb_set_address(u8 value)
{
	/* advance */
	hw_cwrite(CAP_DEVICEADDR, DEVICEADDR_USBADR | DEVICEADDR_USBADRA,
		  value << ffs_nr(DEVICEADDR_USBADR) | DEVICEADDR_USBADRA);
	return 0;
}

/**
 * hw_usb_reset: restart device after a bus reset (execute without
 *               interruption)
 *
 * This function returns an error code
 */
static int hw_usb_reset(void)
{
	hw_usb_set_address(0);

	/* ESS flushes only at end?!? */
	hw_cwrite(CAP_ENDPTFLUSH,    ~0, ~0);   /* flush all EPs */

	/* clear setup token semaphores */
	hw_cwrite(CAP_ENDPTSETUPSTAT, 0,  0);   /* writes its content */

	/* clear complete status */
	hw_cwrite(CAP_ENDPTCOMPLETE,  0,  0);   /* writes its content */

	/* wait until all bits cleared */
	while (hw_cread(CAP_ENDPTPRIME, ~0))
		udelay(10);             /* not RTOS friendly */

	/* reset all endpoints ? */

	/* reset internal status and wait for further instructions
	   no need to verify the port reset status (ESS does it) */

	return 0;
}

/******************************************************************************
 * DBG block
 *****************************************************************************/
/**
 * show_device: prints information about device capabilities and status
 *
 * Check "device.h" for details
 */
static ssize_t show_device(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct ci13xxx *udc = container_of(dev, struct ci13xxx, gadget.dev);
	struct usb_gadget *gadget = &udc->gadget;
	int n = 0;

	dbg_trace("[%s] %p\n", __func__, buf);
	if (attr == NULL || buf == NULL) {
		dev_err(dev, "[%s] EINVAL\n", __func__);
		return 0;
	}

	n += scnprintf(buf + n, PAGE_SIZE - n, "speed             = %d\n",
		       gadget->speed);
	n += scnprintf(buf + n, PAGE_SIZE - n, "is_dualspeed      = %d\n",
		       gadget->is_dualspeed);
	n += scnprintf(buf + n, PAGE_SIZE - n, "is_otg            = %d\n",
		       gadget->is_otg);
	n += scnprintf(buf + n, PAGE_SIZE - n, "is_a_peripheral   = %d\n",
		       gadget->is_a_peripheral);
	n += scnprintf(buf + n, PAGE_SIZE - n, "b_hnp_enable      = %d\n",
		       gadget->b_hnp_enable);
	n += scnprintf(buf + n, PAGE_SIZE - n, "a_hnp_support     = %d\n",
		       gadget->a_hnp_support);
	n += scnprintf(buf + n, PAGE_SIZE - n, "a_alt_hnp_support = %d\n",
		       gadget->a_alt_hnp_support);
	n += scnprintf(buf + n, PAGE_SIZE - n, "name              = %s\n",
		       (gadget->name ? gadget->name : ""));

	return n;
}
static DEVICE_ATTR(device, S_IRUSR, show_device, NULL);

/**
 * show_driver: prints information about attached gadget (if any)
 *
 * Check "device.h" for details
 */
static ssize_t show_driver(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct ci13xxx *udc = container_of(dev, struct ci13xxx, gadget.dev);
	struct usb_gadget_driver *driver = udc->driver;
	int n = 0;

	dbg_trace("[%s] %p\n", __func__, buf);
	if (attr == NULL || buf == NULL) {
		dev_err(dev, "[%s] EINVAL\n", __func__);
		return 0;
	}

	if (driver == NULL)
		return scnprintf(buf, PAGE_SIZE,
				 "There is no gadget attached!\n");

	n += scnprintf(buf + n, PAGE_SIZE - n, "function  = %s\n",
		       (driver->function ? driver->function : ""));
	n += scnprintf(buf + n, PAGE_SIZE - n, "max speed = %d\n",
		       driver->speed);

	return n;
}
static DEVICE_ATTR(driver, S_IRUSR, show_driver, NULL);

/* Maximum event message length */
#define DBG_DATA_MSG   64UL

/* Maximum event messages */
#define DBG_DATA_MAX   128UL

/* Event buffer descriptor */
static struct {
	char     (buf[DBG_DATA_MAX])[DBG_DATA_MSG];   /* buffer */
	unsigned idx;   /* index */
	unsigned tty;   /* print to console? */
	rwlock_t lck;   /* lock */
} dbg_data = {
	.idx = 0,
	.tty = 0,
	.lck = __RW_LOCK_UNLOCKED(lck)
};

/**
 * dbg_dec: decrements debug event index
 * @idx: buffer index
 */
static void dbg_dec(unsigned *idx)
{
	*idx = (*idx - 1) & (DBG_DATA_MAX-1);
}

/**
 * dbg_inc: increments debug event index
 * @idx: buffer index
 */
static void dbg_inc(unsigned *idx)
{
	*idx = (*idx + 1) & (DBG_DATA_MAX-1);
}

/**
 * dbg_print:  prints the common part of the event
 * @addr:   endpoint address
 * @name:   event name
 * @status: status
 * @extra:  extra information
 */
static void dbg_print(u8 addr, const char *name, int status, const char *extra)
{
	struct timeval tval;
	unsigned int stamp;
	unsigned long flags;

	write_lock_irqsave(&dbg_data.lck, flags);

	do_gettimeofday(&tval);
	stamp = tval.tv_sec & 0xFFFF;	/* 2^32 = 4294967296. Limit to 4096s */
	stamp = stamp * 1000000 + tval.tv_usec;

	scnprintf(dbg_data.buf[dbg_data.idx], DBG_DATA_MSG,
		  "%04X\t� %02X %-7.7s %4i �\t%s\n",
		  stamp, addr, name, status, extra);

	dbg_inc(&dbg_data.idx);

	write_unlock_irqrestore(&dbg_data.lck, flags);

	if (dbg_data.tty != 0)
		pr_notice("%04X\t� %02X %-7.7s %4i �\t%s\n",
			  stamp, addr, name, status, extra);
}

/**
 * dbg_done: prints a DONE event
 * @addr:   endpoint address
 * @td:     transfer descriptor
 * @status: status
 */
static void dbg_done(u8 addr, const u32 token, int status)
{
	char msg[DBG_DATA_MSG];

	scnprintf(msg, sizeof(msg), "%d %02X",
		  (int)(token & TD_TOTAL_BYTES) >> ffs_nr(TD_TOTAL_BYTES),
		  (int)(token & TD_STATUS)      >> ffs_nr(TD_STATUS));
	dbg_print(addr, "DONE", status, msg);
}

/**
 * dbg_event: prints a generic event
 * @addr:   endpoint address
 * @name:   event name
 * @status: status
 */
static void dbg_event(u8 addr, const char *name, int status)
{
	if (name != NULL)
		dbg_print(addr, name, status, "");
}

/*
 * dbg_queue: prints a QUEUE event
 * @addr:   endpoint address
 * @req:    USB request
 * @status: status
 */
static void dbg_queue(u8 addr, const struct usb_request *req, int status)
{
	char msg[DBG_DATA_MSG];

	if (req != NULL) {
		scnprintf(msg, sizeof(msg),
			  "%d %d", !req->no_interrupt, req->length);
		dbg_print(addr, "QUEUE", status, msg);
	}
}

/**
 * dbg_setup: prints a SETUP event
 * @addr: endpoint address
 * @req:  setup request
 */
static void dbg_setup(u8 addr, const struct usb_ctrlrequest *req)
{
	char msg[DBG_DATA_MSG];

	if (req != NULL) {
		scnprintf(msg, sizeof(msg),
			  "%02X %02X %04X %04X %d", req->bRequestType,
			  req->bRequest, le16_to_cpu(req->wValue),
			  le16_to_cpu(req->wIndex), le16_to_cpu(req->wLength));
		dbg_print(addr, "SETUP", 0, msg);
	}
}

/**
 * show_events: displays the event buffer
 *
 * Check "device.h" for details
 */
static ssize_t show_events(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	unsigned long flags;
	unsigned i, j, n = 0;

	dbg_trace("[%s] %p\n", __func__, buf);
	if (attr == NULL || buf == NULL) {
		dev_err(dev, "[%s] EINVAL\n", __func__);
		return 0;
	}

	read_lock_irqsave(&dbg_data.lck, flags);

	i = dbg_data.idx;
	for (dbg_dec(&i); i != dbg_data.idx; dbg_dec(&i)) {
		n += strlen(dbg_data.buf[i]);
		if (n >= PAGE_SIZE) {
			n -= strlen(dbg_data.buf[i]);
			break;
		}
	}
	for (j = 0, dbg_inc(&i); j < n; dbg_inc(&i))
		j += scnprintf(buf + j, PAGE_SIZE - j,
			       "%s", dbg_data.buf[i]);

	read_unlock_irqrestore(&dbg_data.lck, flags);

	return n;
}

/**
 * store_events: configure if events are going to be also printed to console
 *
 * Check "device.h" for details
 */
static ssize_t store_events(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned tty;

	dbg_trace("[%s] %p, %d\n", __func__, buf, count);
	if (attr == NULL || buf == NULL) {
		dev_err(dev, "[%s] EINVAL\n", __func__);
		goto done;
	}

	if (sscanf(buf, "%u", &tty) != 1 || tty > 1) {
		dev_err(dev, "<1|0>: enable|disable console log\n");
		goto done;
	}

	dbg_data.tty = tty;
	dev_info(dev, "tty = %u", dbg_data.tty);

 done:
	return count;
}
static DEVICE_ATTR(events, S_IRUSR | S_IWUSR, show_events, store_events);

/**
 * show_inters: interrupt status, enable status and historic
 *
 * Check "device.h" for details
 */
static ssize_t show_inters(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct ci13xxx *udc = container_of(dev, struct ci13xxx, gadget.dev);
	unsigned long flags;
	u32 intr;
	unsigned i, j, n = 0;

	dbg_trace("[%s] %p\n", __func__, buf);
	if (attr == NULL || buf == NULL) {
		dev_err(dev, "[%s] EINVAL\n", __func__);
		return 0;
	}

	spin_lock_irqsave(udc->lock, flags);

	n += scnprintf(buf + n, PAGE_SIZE - n,
		       "status = %08x\n", hw_read_intr_status());
	n += scnprintf(buf + n, PAGE_SIZE - n,
		       "enable = %08x\n", hw_read_intr_enable());

	n += scnprintf(buf + n, PAGE_SIZE - n, "*test = %d\n",
		       isr_statistics.test);
	n += scnprintf(buf + n, PAGE_SIZE - n, "� ui  = %d\n",
		       isr_statistics.ui);
	n += scnprintf(buf + n, PAGE_SIZE - n, "� uei = %d\n",
		       isr_statistics.uei);
	n += scnprintf(buf + n, PAGE_SIZE - n, "� pci = %d\n",
		       isr_statistics.pci);
	n += scnprintf(buf + n, PAGE_SIZE - n, "� uri = %d\n",
		       isr_statistics.uri);
	n += scnprintf(buf + n, PAGE_SIZE - n, "� sli = %d\n",
		       isr_statistics.sli);
	n += scnprintf(buf + n, PAGE_SIZE - n, "*none = %d\n",
		       isr_statistics.none);
	n += scnprintf(buf + n, PAGE_SIZE - n, "*hndl = %d\n",
		       isr_statistics.hndl.cnt);

	for (i = isr_statistics.hndl.idx, j = 0; j <= ISR_MASK; j++, i++) {
		i   &= ISR_MASK;
		intr = isr_statistics.hndl.buf[i];

		if (USBi_UI  & intr)
			n += scnprintf(buf + n, PAGE_SIZE - n, "ui  ");
		intr &= ~USBi_UI;
		if (USBi_UEI & intr)
			n += scnprintf(buf + n, PAGE_SIZE - n, "uei ");
		intr &= ~USBi_UEI;
		if (USBi_PCI & intr)
			n += scnprintf(buf + n, PAGE_SIZE - n, "pci ");
		intr &= ~USBi_PCI;
		if (USBi_URI & intr)
			n += scnprintf(buf + n, PAGE_SIZE - n, "uri ");
		intr &= ~USBi_URI;
		if (USBi_SLI & intr)
			n += scnprintf(buf + n, PAGE_SIZE - n, "sli ");
		intr &= ~USBi_SLI;
		if (intr)
			n += scnprintf(buf + n, PAGE_SIZE - n, "??? ");
		if (isr_statistics.hndl.buf[i])
			n += scnprintf(buf + n, PAGE_SIZE - n, "\n");
	}

	spin_unlock_irqrestore(udc->lock, flags);

	return n;
}

/**
 * store_inters: enable & force or disable an individual interrutps
 *                   (to be used for test purposes only)
 *
 * Check "device.h" for details
 */
static ssize_t store_inters(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct ci13xxx *udc = container_of(dev, struct ci13xxx, gadget.dev);
	unsigned long flags;
	unsigned en, bit;

	dbg_trace("[%s] %p, %d\n", __func__, buf, count);
	if (attr == NULL || buf == NULL) {
		dev_err(dev, "[%s] EINVAL\n", __func__);
		goto done;
	}

	if (sscanf(buf, "%u %u", &en, &bit) != 2 || en > 1) {
		dev_err(dev, "<1|0> <bit>: enable|disable interrupt");
		goto done;
	}

	spin_lock_irqsave(udc->lock, flags);
	if (en) {
		if (hw_intr_force(bit))
			dev_err(dev, "invalid bit number\n");
		else
			isr_statistics.test++;
	} else {
		if (hw_intr_clear(bit))
			dev_err(dev, "invalid bit number\n");
	}
	spin_unlock_irqrestore(udc->lock, flags);

 done:
	return count;
}
static DEVICE_ATTR(inters, S_IRUSR | S_IWUSR, show_inters, store_inters);

/**
 * show_port_test: reads port test mode
 *
 * Check "device.h" for details
 */
static ssize_t show_port_test(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct ci13xxx *udc = container_of(dev, struct ci13xxx, gadget.dev);
	unsigned long flags;
	unsigned mode;

	dbg_trace("[%s] %p\n", __func__, buf);
	if (attr == NULL || buf == NULL) {
		dev_err(dev, "[%s] EINVAL\n", __func__);
		return 0;
	}

	spin_lock_irqsave(udc->lock, flags);
	mode = hw_port_test_get();
	spin_unlock_irqrestore(udc->lock, flags);

	return scnprintf(buf, PAGE_SIZE, "mode = %u\n", mode);
}

/**
 * store_port_test: writes port test mode
 *
 * Check "device.h" for details
 */
static ssize_t store_port_test(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct ci13xxx *udc = container_of(dev, struct ci13xxx, gadget.dev);
	unsigned long flags;
	unsigned mode;

	dbg_trace("[%s] %p, %d\n", __func__, buf, count);
	if (attr == NULL || buf == NULL) {
		dev_err(dev, "[%s] EINVAL\n", __func__);
		goto done;
	}

	if (sscanf(buf, "%u", &mode) != 1) {
		dev_err(dev, "<mode>: set port test mode");
		goto done;
	}

	spin_lock_irqsave(udc->lock, flags);
	if (hw_port_test_set(mode))
		dev_err(dev, "invalid mode\n");
	spin_unlock_irqrestore(udc->lock, flags);

 done:
	return count;
}
static DEVICE_ATTR(port_test, S_IRUSR | S_IWUSR,
		   show_port_test, store_port_test);

/**
 * show_qheads: DMA contents of all queue heads
 *
 * Check "device.h" for details
 */
static ssize_t show_qheads(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct ci13xxx *udc = container_of(dev, struct ci13xxx, gadget.dev);
	unsigned long flags;
	unsigned i, j, n = 0;

	dbg_trace("[%s] %p\n", __func__, buf);
	if (attr == NULL || buf == NULL) {
		dev_err(dev, "[%s] EINVAL\n", __func__);
		return 0;
	}

	spin_lock_irqsave(udc->lock, flags);
	for (i = 0; i < hw_ep_max/2; i++) {
		struct ci13xxx_ep *mEpRx = &udc->ci13xxx_ep[i];
		struct ci13xxx_ep *mEpTx = &udc->ci13xxx_ep[i + hw_ep_max/2];
		n += scnprintf(buf + n, PAGE_SIZE - n,
			       "EP=%02i: RX=%08X TX=%08X\n",
			       i, (u32)mEpRx->qh.dma, (u32)mEpTx->qh.dma);
		for (j = 0; j < (sizeof(struct ci13xxx_qh)/sizeof(u32)); j++) {
			n += scnprintf(buf + n, PAGE_SIZE - n,
				       " %04X:    %08X    %08X\n", j,
				       *((u32 *)mEpRx->qh.ptr + j),
				       *((u32 *)mEpTx->qh.ptr + j));
		}
	}
	spin_unlock_irqrestore(udc->lock, flags);

	return n;
}
static DEVICE_ATTR(qheads, S_IRUSR, show_qheads, NULL);

/**
 * show_registers: dumps all registers
 *
 * Check "device.h" for details
 */
static ssize_t show_registers(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct ci13xxx *udc = container_of(dev, struct ci13xxx, gadget.dev);
	unsigned long flags;
	u32 dump[512];
	unsigned i, k, n = 0;

	dbg_trace("[%s] %p\n", __func__, buf);
	if (attr == NULL || buf == NULL) {
		dev_err(dev, "[%s] EINVAL\n", __func__);
		return 0;
	}

	spin_lock_irqsave(udc->lock, flags);
	k = hw_register_read(dump, sizeof(dump)/sizeof(u32));
	spin_unlock_irqrestore(udc->lock, flags);

	for (i = 0; i < k; i++) {
		n += scnprintf(buf + n, PAGE_SIZE - n,
			       "reg[0x%04X] = 0x%08X\n",
			       i * (unsigned)sizeof(u32), dump[i]);
	}

	return n;
}

/**
 * store_registers: writes value to register address
 *
 * Check "device.h" for details
 */
static ssize_t store_registers(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct ci13xxx *udc = container_of(dev, struct ci13xxx, gadget.dev);
	unsigned long addr, data, flags;

	dbg_trace("[%s] %p, %d\n", __func__, buf, count);
	if (attr == NULL || buf == NULL) {
		dev_err(dev, "[%s] EINVAL\n", __func__);
		goto done;
	}

	if (sscanf(buf, "%li %li", &addr, &data) != 2) {
		dev_err(dev, "<addr> <data>: write data to register address");
		goto done;
	}

	spin_lock_irqsave(udc->lock, flags);
	if (hw_register_write(addr, data))
		dev_err(dev, "invalid address range\n");
	spin_unlock_irqrestore(udc->lock, flags);

 done:
	return count;
}
static DEVICE_ATTR(registers, S_IRUSR | S_IWUSR,
		   show_registers, store_registers);

/**
 * show_requests: DMA contents of all requests currently queued (all endpts)
 *
 * Check "device.h" for details
 */
static ssize_t show_requests(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct ci13xxx *udc = container_of(dev, struct ci13xxx, gadget.dev);
	unsigned long flags;
	struct list_head   *ptr = NULL;
	struct ci13xxx_req *req = NULL;
	unsigned i, j, n = 0, qSize = sizeof(struct ci13xxx_td)/sizeof(u32);

	dbg_trace("[%s] %p\n", __func__, buf);
	if (attr == NULL || buf == NULL) {
		dev_err(dev, "[%s] EINVAL\n", __func__);
		return 0;
	}

	spin_lock_irqsave(udc->lock, flags);
	for (i = 0; i < hw_ep_max; i++)
		list_for_each(ptr, &udc->ci13xxx_ep[i].qh.queue)
		{
			req = list_entry(ptr, struct ci13xxx_req, queue);

			n += scnprintf(buf + n, PAGE_SIZE - n,
					"EP=%02i: TD=%08X %s\n",
					i % hw_ep_max/2, (u32)req->dma,
					((i < hw_ep_max/2) ? "RX" : "TX"));

			for (j = 0; j < qSize; j++)
				n += scnprintf(buf + n, PAGE_SIZE - n,
						" %04X:    %08X\n", j,
						*((u32 *)req->ptr + j));
		}
	spin_unlock_irqrestore(udc->lock, flags);

	return n;
}
static DEVICE_ATTR(requests, S_IRUSR, show_requests, NULL);

/**
 * dbg_create_files: initializes the attribute interface
 * @dev: device
 *
 * This function returns an error code
 */
__maybe_unused static int dbg_create_files(struct device *dev)
{
	int retval = 0;

	if (dev == NULL)
		return -EINVAL;
	retval = device_create_file(dev, &dev_attr_device);
	if (retval)
		goto done;
	retval = device_create_file(dev, &dev_attr_driver);
	if (retval)
		goto rm_device;
	retval = device_create_file(dev, &dev_attr_events);
	if (retval)
		goto rm_driver;
	retval = device_create_file(dev, &dev_attr_inters);
	if (retval)
		goto rm_events;
	retval = device_create_file(dev, &dev_attr_port_test);
	if (retval)
		goto rm_inters;
	retval = device_create_file(dev, &dev_attr_qheads);
	if (retval)
		goto rm_port_test;
	retval = device_create_file(dev, &dev_attr_registers);
	if (retval)
		goto rm_qheads;
	retval = device_create_file(dev, &dev_attr_requests);
	if (retval)
		goto rm_registers;
	return 0;

 rm_registers:
	device_remove_file(dev, &dev_attr_registers);
 rm_qheads:
	device_remove_file(dev, &dev_attr_qheads);
 rm_port_test:
	device_remove_file(dev, &dev_attr_port_test);
 rm_inters:
	device_remove_file(dev, &dev_attr_inters);
 rm_events:
	device_remove_file(dev, &dev_attr_events);
 rm_driver:
	device_remove_file(dev, &dev_attr_driver);
 rm_device:
	device_remove_file(dev, &dev_attr_device);
 done:
	return retval;
}

/**
 * dbg_remove_files: destroys the attribute interface
 * @dev: device
 *
 * This function returns an error code
 */
__maybe_unused static int dbg_remove_files(struct device *dev)
{
	if (dev == NULL)
		return -EINVAL;
	device_remove_file(dev, &dev_attr_requests);
	device_remove_file(dev, &dev_attr_registers);
	device_remove_file(dev, &dev_attr_qheads);
	device_remove_file(dev, &dev_attr_port_test);
	device_remove_file(dev, &dev_attr_inters);
	device_remove_file(dev, &dev_attr_events);
	device_remove_file(dev, &dev_attr_driver);
	device_remove_file(dev, &dev_attr_device);
	return 0;
}

/******************************************************************************
 * UTIL block
 *****************************************************************************/
/**
 * _usb_addr: calculates endpoint address from direction & number
 * @ep:  endpoint
 */
static inline u8 _usb_addr(struct ci13xxx_ep *ep)
{
	return ((ep->dir == TX) ? USB_ENDPOINT_DIR_MASK : 0) | ep->num;
}

/**
 * _hardware_queue: configures a request at hardware level
 * @gadget: gadget
 * @mEp:    endpoint
 *
 * This function returns an error code
 */
static int _hardware_enqueue(struct ci13xxx_ep *mEp, struct ci13xxx_req *mReq)
{
	unsigned i;
	int ret = 0;
	unsigned length = mReq->req.length;

	trace("%p, %p", mEp, mReq);

	/* don't queue twice */
	if (mReq->req.status == -EALREADY)
		return -EALREADY;

	mReq->req.status = -EALREADY;
	if (length && !mReq->req.dma) {
		mReq->req.dma = \
			dma_map_single(mEp->device, mReq->req.buf,
				       length, mEp->dir ? DMA_TO_DEVICE :
				       DMA_FROM_DEVICE);
		if (mReq->req.dma == 0)
			return -ENOMEM;

		mReq->map = 1;
	}

	if (mReq->req.zero && length && (length % mEp->ep.maxpacket == 0)) {
		mReq->zptr = dma_pool_alloc(mEp->td_pool, GFP_ATOMIC,
					   &mReq->zdma);
		if (mReq->zptr == NULL) {
			if (mReq->map) {
				dma_unmap_single(mEp->device, mReq->req.dma,
					length, mEp->dir ? DMA_TO_DEVICE :
					DMA_FROM_DEVICE);
				mReq->req.dma = 0;
				mReq->map     = 0;
			}
			return -ENOMEM;
		}
		memset(mReq->zptr, 0, sizeof(*mReq->zptr));
		mReq->zptr->next    = TD_TERMINATE;
		mReq->zptr->token   = TD_STATUS_ACTIVE;
		if (!mReq->req.no_interrupt)
			mReq->zptr->token   |= TD_IOC;
	}
	/*
	 * TD configuration
	 * TODO - handle requests which spawns into several TDs
	 */
	memset(mReq->ptr, 0, sizeof(*mReq->ptr));
	mReq->ptr->token    = length << ffs_nr(TD_TOTAL_BYTES);
	mReq->ptr->token   &= TD_TOTAL_BYTES;
	mReq->ptr->token   |= TD_STATUS_ACTIVE;
	if (mReq->zptr) {
		mReq->ptr->next    = mReq->zdma;
	} else {
		mReq->ptr->next    = TD_TERMINATE;
		if (!mReq->req.no_interrupt)
			mReq->ptr->token  |= TD_IOC;
	}
	mReq->ptr->page[0]  = mReq->req.dma;
	for (i = 1; i < 5; i++)
		mReq->ptr->page[i] =
			(mReq->req.dma + i * CI13XXX_PAGE_SIZE) & ~TD_RESERVED_MASK;

	if (!list_empty(&mEp->qh.queue)) {
		struct ci13xxx_req *mReqPrev;
		int n = hw_ep_bit(mEp->num, mEp->dir);
		int tmp_stat;

		mReqPrev = list_entry(mEp->qh.queue.prev,
				struct ci13xxx_req, queue);
		if (mReqPrev->zptr)
			mReqPrev->zptr->next = mReq->dma & TD_ADDR_MASK;
		else
			mReqPrev->ptr->next = mReq->dma & TD_ADDR_MASK;
		wmb();
		if (hw_cread(CAP_ENDPTPRIME, BIT(n)))
			goto done;
		do {
			hw_cwrite(CAP_USBCMD, USBCMD_ATDTW, USBCMD_ATDTW);
			tmp_stat = hw_cread(CAP_ENDPTSTAT, BIT(n));
		} while (!hw_cread(CAP_USBCMD, USBCMD_ATDTW));
		hw_cwrite(CAP_USBCMD, USBCMD_ATDTW, 0);
		if (tmp_stat)
			goto done;
	}

	/*  QH configuration */
	mEp->qh.ptr->td.next   = mReq->dma;    /* TERMINATE = 0 */
	mEp->qh.ptr->td.token &= ~TD_STATUS;   /* clear status */
	mEp->qh.ptr->cap |=  QH_ZLT;

	wmb();   /* synchronize before ep prime */

	ret = hw_ep_prime(mEp->num, mEp->dir,
			   mEp->type == USB_ENDPOINT_XFER_CONTROL);
done:
	return ret;
}

/**
 * _hardware_dequeue: handles a request at hardware level
 * @gadget: gadget
 * @mEp:    endpoint
 *
 * This function returns an error code
 */
static int _hardware_dequeue(struct ci13xxx_ep *mEp, struct ci13xxx_req *mReq)
{
	trace("%p, %p", mEp, mReq);

	if (mReq->req.status != -EALREADY)
		return -EINVAL;

	if ((TD_STATUS_ACTIVE & mReq->ptr->token) != 0)
		return -EBUSY;

	if (mReq->zptr) {
		if ((TD_STATUS_ACTIVE & mReq->zptr->token) != 0)
			return -EBUSY;
		dma_pool_free(mEp->td_pool, mReq->zptr, mReq->zdma);
		mReq->zptr = NULL;
	}

	mReq->req.status = 0;

	if (mReq->map) {
		dma_unmap_single(mEp->device, mReq->req.dma, mReq->req.length,
				 mEp->dir ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
		mReq->req.dma = 0;
		mReq->map     = 0;
	}

	mReq->req.status = mReq->ptr->token & TD_STATUS;
	if ((TD_STATUS_HALTED & mReq->req.status) != 0)
		mReq->req.status = -1;
	else if ((TD_STATUS_DT_ERR & mReq->req.status) != 0)
		mReq->req.status = -1;
	else if ((TD_STATUS_TR_ERR & mReq->req.status) != 0)
		mReq->req.status = -1;

	mReq->req.actual   = mReq->ptr->token & TD_TOTAL_BYTES;
	mReq->req.actual >>= ffs_nr(TD_TOTAL_BYTES);
	mReq->req.actual   = mReq->req.length - mReq->req.actual;
	mReq->req.actual   = mReq->req.status ? 0 : mReq->req.actual;

	return mReq->req.actual;
}

/**
 * _ep_nuke: dequeues all endpoint requests
 * @mEp: endpoint
 *
 * This function returns an error code
 * Caller must hold lock
 */
static int _ep_nuke(struct ci13xxx_ep *mEp)
__releases(mEp->lock)
__acquires(mEp->lock)
{
	trace("%p", mEp);

	if (mEp == NULL)
		return -EINVAL;

	hw_ep_flush(mEp->num, mEp->dir);

	while (!list_empty(&mEp->qh.queue)) {

		/* pop oldest request */
		struct ci13xxx_req *mReq = \
			list_entry(mEp->qh.queue.next,
				   struct ci13xxx_req, queue);
		list_del_init(&mReq->queue);
		mReq->req.status = -ESHUTDOWN;

		if (mReq->req.complete != NULL) {
			spin_unlock(mEp->lock);
			mReq->req.complete(&mEp->ep, &mReq->req);
			spin_lock(mEp->lock);
		}
	}
	return 0;
}

/**
 * _gadget_stop_activity: stops all USB activity, flushes & disables all endpts
 * @gadget: gadget
 *
 * This function returns an error code
 * Caller must hold lock
 */
static int _gadget_stop_activity(struct usb_gadget *gadget)
{
	struct usb_ep *ep;
	struct ci13xxx    *udc = container_of(gadget, struct ci13xxx, gadget);
	unsigned long flags;

	trace("%p", gadget);

	if (gadget == NULL)
		return -EINVAL;

	spin_lock_irqsave(udc->lock, flags);
	udc->gadget.speed = USB_SPEED_UNKNOWN;
	udc->remote_wakeup = 0;
	udc->suspended = 0;
	spin_unlock_irqrestore(udc->lock, flags);

	/* flush all endpoints */
	gadget_for_each_ep(ep, gadget) {
		usb_ep_fifo_flush(ep);
	}
	usb_ep_fifo_flush(&udc->ep0out.ep);
	usb_ep_fifo_flush(&udc->ep0in.ep);

	udc->driver->disconnect(gadget);

	/* make sure to disable all endpoints */
	gadget_for_each_ep(ep, gadget) {
		usb_ep_disable(ep);
	}
	usb_ep_disable(&udc->ep0out.ep);
	usb_ep_disable(&udc->ep0in.ep);

	if (udc->status != NULL) {
		usb_ep_free_request(&udc->ep0in.ep, udc->status);
		udc->status = NULL;
	}

	return 0;
}

/******************************************************************************
 * ISR block
 *****************************************************************************/
/**
 * isr_reset_handler: USB reset interrupt handler
 * @udc: UDC device
 *
 * This function resets USB engine after a bus reset occurred
 */
static void isr_reset_handler(struct ci13xxx *udc)
__releases(udc->lock)
__acquires(udc->lock)
{
	int retval;

	trace("%p", udc);

	if (udc == NULL) {
		err("EINVAL");
		return;
	}

	dbg_event(0xFF, "BUS RST", 0);

	spin_unlock(udc->lock);
	retval = _gadget_stop_activity(&udc->gadget);
	if (retval)
		goto done;

	retval = hw_usb_reset();
	if (retval)
		goto done;

	retval = usb_ep_enable(&udc->ep0out.ep, &ctrl_endpt_out_desc);
	if (retval)
		goto done;

	retval = usb_ep_enable(&udc->ep0in.ep, &ctrl_endpt_in_desc);
	if (!retval) {
		udc->status = usb_ep_alloc_request(&udc->ep0in.ep, GFP_ATOMIC);
		if (udc->status == NULL) {
			usb_ep_disable(&udc->ep0out.ep);
			retval = -ENOMEM;
		}
	}
	spin_lock(udc->lock);

 done:
	if (retval)
		err("error: %i", retval);
}

/**
 * isr_get_status_complete: get_status request complete function
 * @ep:  endpoint
 * @req: request handled
 *
 * Caller must release lock
 */
static void isr_get_status_complete(struct usb_ep *ep, struct usb_request *req)
{
	trace("%p, %p", ep, req);

	if (ep == NULL || req == NULL) {
		err("EINVAL");
		return;
	}

	kfree(req->buf);
	usb_ep_free_request(ep, req);
}

/**
 * isr_get_status_response: get_status request response
 * @udc: udc struct
 * @setup: setup request packet
 *
 * This function returns an error code
 */
static int isr_get_status_response(struct ci13xxx *udc,
				   struct usb_ctrlrequest *setup)
__releases(mEp->lock)
__acquires(mEp->lock)
{
	struct ci13xxx_ep *mEp = &udc->ep0in;
	struct usb_request *req = NULL;
	gfp_t gfp_flags = GFP_ATOMIC;
	int dir, num, retval;

	trace("%p, %p", mEp, setup);

	if (mEp == NULL || setup == NULL)
		return -EINVAL;

	spin_unlock(mEp->lock);
	req = usb_ep_alloc_request(&mEp->ep, gfp_flags);
	spin_lock(mEp->lock);
	if (req == NULL)
		return -ENOMEM;

	req->complete = isr_get_status_complete;
	req->length   = 2;
	req->buf      = kzalloc(req->length, gfp_flags);
	if (req->buf == NULL) {
		retval = -ENOMEM;
		goto err_free_req;
	}

	if ((setup->bRequestType & USB_RECIP_MASK) == USB_RECIP_DEVICE) {
		/* Assume that device is bus powered for now. */
		*((u16 *)req->buf) = _udc->remote_wakeup << 1;
		retval = 0;
	} else if ((setup->bRequestType & USB_RECIP_MASK) \
		   == USB_RECIP_ENDPOINT) {
		dir = (le16_to_cpu(setup->wIndex) & USB_ENDPOINT_DIR_MASK) ?
			TX : RX;
		num =  le16_to_cpu(setup->wIndex) & USB_ENDPOINT_NUMBER_MASK;
		*((u16 *)req->buf) = hw_ep_get_halt(num, dir);
	}
	/* else do nothing; reserved for future use */

	spin_unlock(mEp->lock);
	retval = usb_ep_queue(&mEp->ep, req, gfp_flags);
	spin_lock(mEp->lock);
	if (retval)
		goto err_free_buf;

	return 0;

 err_free_buf:
	kfree(req->buf);
 err_free_req:
	spin_unlock(mEp->lock);
	usb_ep_free_request(&mEp->ep, req);
	spin_lock(mEp->lock);
	return retval;
}

/**
 * isr_setup_status_complete: setup_status request complete function
 * @ep:  endpoint
 * @req: request handled
 *
 * Caller must release lock. Put the port in test mode if test mode
 * feature is selected.
 */
static void
isr_setup_status_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct ci13xxx *udc = req->context;
	unsigned long flags;

	trace("%p, %p", ep, req);

	spin_lock_irqsave(udc->lock, flags);
	if (udc->test_mode)
		hw_port_test_set(udc->test_mode);
	spin_unlock_irqrestore(udc->lock, flags);
}

/**
 * isr_setup_status_phase: queues the status phase of a setup transation
 * @udc: udc struct
 *
 * This function returns an error code
 */
static int isr_setup_status_phase(struct ci13xxx *udc)
__releases(mEp->lock)
__acquires(mEp->lock)
{
	int retval;
	struct ci13xxx_ep *mEp;

	trace("%p", udc);

	mEp = (udc->ep0_dir == TX) ? &udc->ep0out : &udc->ep0in;
	udc->status->context = udc;
	udc->status->complete = isr_setup_status_complete;

	spin_unlock(mEp->lock);
	retval = usb_ep_queue(&mEp->ep, udc->status, GFP_ATOMIC);
	spin_lock(mEp->lock);

	return retval;
}

/**
 * isr_tr_complete_low: transaction complete low level handler
 * @mEp: endpoint
 *
 * This function returns an error code
 * Caller must hold lock
 */
static int isr_tr_complete_low(struct ci13xxx_ep *mEp)
__releases(mEp->lock)
__acquires(mEp->lock)
{
	struct ci13xxx_req *mReq, *mReqTemp;
	int retval;

	trace("%p", mEp);

	if (list_empty(&mEp->qh.queue))
		return -EINVAL;

	list_for_each_entry_safe(mReq, mReqTemp, &mEp->qh.queue,
			queue) {
		retval = _hardware_dequeue(mEp, mReq);
		if (retval < 0)
			break;
		list_del_init(&mReq->queue);
		dbg_done(_usb_addr(mEp), mReq->ptr->token, retval);
		if (mReq->req.complete != NULL) {
			spin_unlock(mEp->lock);
			mReq->req.complete(&mEp->ep, &mReq->req);
			spin_lock(mEp->lock);
		}
	}

	if (retval == EBUSY)
		retval = 0;
	if (retval < 0)
		dbg_event(_usb_addr(mEp), "DONE", retval);

	return retval;
}

/**
 * isr_tr_complete_handler: transaction complete interrupt handler
 * @udc: UDC descriptor
 *
 * This function handles traffic events
 */
static void isr_tr_complete_handler(struct ci13xxx *udc)
__releases(udc->lock)
__acquires(udc->lock)
{
	unsigned i;
	u8 tmode = 0;

	trace("%p", udc);

	if (udc == NULL) {
		err("EINVAL");
		return;
	}

	for (i = 0; i < hw_ep_max; i++) {
		struct ci13xxx_ep *mEp  = &udc->ci13xxx_ep[i];
		int type, num, err = -EINVAL;
		struct usb_ctrlrequest req;

		if (mEp->desc == NULL)
			continue;   /* not configured */

		if (hw_test_and_clear_complete(i)) {
			err = isr_tr_complete_low(mEp);
			if (mEp->type == USB_ENDPOINT_XFER_CONTROL) {
				if (err > 0)   /* needs status phase */
					err = isr_setup_status_phase(udc);
				if (err < 0) {
					dbg_event(_usb_addr(mEp),
						  "ERROR", err);
					spin_unlock(udc->lock);
					if (usb_ep_set_halt(&mEp->ep))
						err("error: ep_set_halt");
					spin_lock(udc->lock);
				}
			}
		}

		if (mEp->type != USB_ENDPOINT_XFER_CONTROL ||
		    !hw_test_and_clear_setup_status(i))
			continue;

		if (i != 0) {
			warn("ctrl traffic received at endpoint");
			continue;
		}

		/*
		 * Flush data and handshake transactions of previous
		 * setup packet.
		 */
		_ep_nuke(&udc->ep0out);
		_ep_nuke(&udc->ep0in);

		/* read_setup_packet */
		do {
			hw_test_and_set_setup_guard();
			memcpy(&req, &mEp->qh.ptr->setup, sizeof(req));
		} while (!hw_test_and_clear_setup_guard());

		type = req.bRequestType;

		udc->ep0_dir = (type & USB_DIR_IN) ? TX : RX;

		dbg_setup(_usb_addr(mEp), &req);

		switch (req.bRequest) {
		case USB_REQ_CLEAR_FEATURE:
			if (type == (USB_DIR_OUT|USB_RECIP_ENDPOINT) &&
					le16_to_cpu(req.wValue) ==
					USB_ENDPOINT_HALT) {
				if (req.wLength != 0)
					break;
				num  = le16_to_cpu(req.wIndex);
				num &= USB_ENDPOINT_NUMBER_MASK;
				if (!udc->ci13xxx_ep[num].wedge) {
					spin_unlock(udc->lock);
					err = usb_ep_clear_halt(
						&udc->ci13xxx_ep[num].ep);
					spin_lock(udc->lock);
					if (err)
						break;
				}
				err = isr_setup_status_phase(udc);
			} else if (type == (USB_DIR_OUT|USB_RECIP_DEVICE) &&
					le16_to_cpu(req.wValue) ==
					USB_DEVICE_REMOTE_WAKEUP) {
				if (req.wLength != 0)
					break;
				udc->remote_wakeup = 0;
				err = isr_setup_status_phase(udc);
			} else {
				goto delegate;
			}
			break;
		case USB_REQ_GET_STATUS:
			if (type != (USB_DIR_IN|USB_RECIP_DEVICE)   &&
			    type != (USB_DIR_IN|USB_RECIP_ENDPOINT) &&
			    type != (USB_DIR_IN|USB_RECIP_INTERFACE))
				goto delegate;
			if (le16_to_cpu(req.wLength) != 2 ||
			    le16_to_cpu(req.wValue)  != 0)
				break;
			err = isr_get_status_response(udc, &req);
			break;
		case USB_REQ_SET_ADDRESS:
			if (type != (USB_DIR_OUT|USB_RECIP_DEVICE))
				goto delegate;
			if (le16_to_cpu(req.wLength) != 0 ||
			    le16_to_cpu(req.wIndex)  != 0)
				break;
			err = hw_usb_set_address((u8)le16_to_cpu(req.wValue));
			if (err)
				break;
			err = isr_setup_status_phase(udc);
			break;
		case USB_REQ_SET_FEATURE:
			if (type == (USB_DIR_OUT|USB_RECIP_ENDPOINT) &&
					le16_to_cpu(req.wValue) ==
					USB_ENDPOINT_HALT) {
				if (req.wLength != 0)
					break;
				num  = le16_to_cpu(req.wIndex);
				num &= USB_ENDPOINT_NUMBER_MASK;

				spin_unlock(udc->lock);
				err = usb_ep_set_halt(&udc->ci13xxx_ep[num].ep);
				spin_lock(udc->lock);
				if (!err)
					isr_setup_status_phase(udc);
			} else if (type == (USB_DIR_OUT|USB_RECIP_DEVICE)) {
				if (req.wLength != 0)
					break;
				switch (le16_to_cpu(req.wValue)) {
				case USB_DEVICE_REMOTE_WAKEUP:
					udc->remote_wakeup = 1;
					err = isr_setup_status_phase(udc);
					break;
				case USB_DEVICE_TEST_MODE:
					tmode = le16_to_cpu(req.wIndex) >> 8;
					switch (tmode) {
					case TEST_J:
					case TEST_K:
					case TEST_SE0_NAK:
					case TEST_PACKET:
					case TEST_FORCE_EN:
						udc->test_mode = tmode;
						err = isr_setup_status_phase(
								udc);
						break;
					default:
						break;
					}
				default:
					goto delegate;
				}
			} else {
				goto delegate;
			}
			break;
		default:
delegate:
			if (req.wLength == 0)   /* no data phase */
				udc->ep0_dir = TX;

			spin_unlock(udc->lock);
			err = udc->driver->setup(&udc->gadget, &req);
			spin_lock(udc->lock);
			break;
		}

		if (err < 0) {
			dbg_event(_usb_addr(mEp), "ERROR", err);

			spin_unlock(udc->lock);
			if (usb_ep_set_halt(&mEp->ep))
				err("error: ep_set_halt");
			spin_lock(udc->lock);
		}
	}
}

/******************************************************************************
 * ENDPT block
 *****************************************************************************/
/**
 * ep_enable: configure endpoint, making it usable
 *
 * Check usb_ep_enable() at "usb_gadget.h" for details
 */
static int ep_enable(struct usb_ep *ep,
		     const struct usb_endpoint_descriptor *desc)
{
	struct ci13xxx_ep *mEp = container_of(ep, struct ci13xxx_ep, ep);
	int retval = 0;
	unsigned long flags;

	trace("%p, %p", ep, desc);

	if (ep == NULL || desc == NULL)
		return -EINVAL;

	spin_lock_irqsave(mEp->lock, flags);

	/* only internal SW should enable ctrl endpts */

	mEp->desc = desc;

	if (!list_empty(&mEp->qh.queue))
		warn("enabling a non-empty endpoint!");

	mEp->dir  = usb_endpoint_dir_in(desc) ? TX : RX;
	mEp->num  = usb_endpoint_num(desc);
	mEp->type = usb_endpoint_type(desc);

	mEp->ep.maxpacket = __constant_le16_to_cpu(desc->wMaxPacketSize);

	dbg_event(_usb_addr(mEp), "ENABLE", 0);

	mEp->qh.ptr->cap = 0;

	if (mEp->type == USB_ENDPOINT_XFER_CONTROL)
		mEp->qh.ptr->cap |=  QH_IOS;
	else if (mEp->type == USB_ENDPOINT_XFER_ISOC)
		mEp->qh.ptr->cap &= ~QH_MULT;
	else
		mEp->qh.ptr->cap &= ~QH_ZLT;

	mEp->qh.ptr->cap |=
		(mEp->ep.maxpacket << ffs_nr(QH_MAX_PKT)) & QH_MAX_PKT;
	mEp->qh.ptr->td.next |= TD_TERMINATE;   /* needed? */

	retval |= hw_ep_enable(mEp->num, mEp->dir, mEp->type);

	spin_unlock_irqrestore(mEp->lock, flags);
	return retval;
}

/**
 * ep_disable: endpoint is no longer usable
 *
 * Check usb_ep_disable() at "usb_gadget.h" for details
 */
static int ep_disable(struct usb_ep *ep)
{
	struct ci13xxx_ep *mEp = container_of(ep, struct ci13xxx_ep, ep);
	int direction, retval = 0;
	unsigned long flags;

	trace("%p", ep);

	if (ep == NULL)
		return -EINVAL;
	else if (mEp->desc == NULL)
		return -EBUSY;

	spin_lock_irqsave(mEp->lock, flags);

	/* only internal SW should disable ctrl endpts */

	direction = mEp->dir;
	do {
		dbg_event(_usb_addr(mEp), "DISABLE", 0);

		retval |= _ep_nuke(mEp);
		retval |= hw_ep_disable(mEp->num, mEp->dir);

		if (mEp->type == USB_ENDPOINT_XFER_CONTROL)
			mEp->dir = (mEp->dir == TX) ? RX : TX;

	} while (mEp->dir != direction);

	mEp->desc = NULL;

	spin_unlock_irqrestore(mEp->lock, flags);
	return retval;
}

/**
 * ep_alloc_request: allocate a request object to use with this endpoint
 *
 * Check usb_ep_alloc_request() at "usb_gadget.h" for details
 */
static struct usb_request *ep_alloc_request(struct usb_ep *ep, gfp_t gfp_flags)
{
	struct ci13xxx_ep  *mEp  = container_of(ep, struct ci13xxx_ep, ep);
	struct ci13xxx_req *mReq = NULL;

	trace("%p, %i", ep, gfp_flags);

	if (ep == NULL) {
		err("EINVAL");
		return NULL;
	}

	mReq = kzalloc(sizeof(struct ci13xxx_req), gfp_flags);
	if (mReq != NULL) {
		INIT_LIST_HEAD(&mReq->queue);

		mReq->ptr = dma_pool_alloc(mEp->td_pool, gfp_flags,
					   &mReq->dma);
		if (mReq->ptr == NULL) {
			kfree(mReq);
			mReq = NULL;
		}
	}

	dbg_event(_usb_addr(mEp), "ALLOC", mReq == NULL);

	return (mReq == NULL) ? NULL : &mReq->req;
}

/**
 * ep_free_request: frees a request object
 *
 * Check usb_ep_free_request() at "usb_gadget.h" for details
 */
static void ep_free_request(struct usb_ep *ep, struct usb_request *req)
{
	struct ci13xxx_ep  *mEp  = container_of(ep,  struct ci13xxx_ep, ep);
	struct ci13xxx_req *mReq = container_of(req, struct ci13xxx_req, req);
	unsigned long flags;

	trace("%p, %p", ep, req);

	if (ep == NULL || req == NULL) {
		err("EINVAL");
		return;
	} else if (!list_empty(&mReq->queue)) {
		err("EBUSY");
		return;
	}

	spin_lock_irqsave(mEp->lock, flags);

	if (mReq->ptr)
		dma_pool_free(mEp->td_pool, mReq->ptr, mReq->dma);
	kfree(mReq);

	dbg_event(_usb_addr(mEp), "FREE", 0);

	spin_unlock_irqrestore(mEp->lock, flags);
}

/**
 * ep_queue: queues (submits) an I/O request to an endpoint
 *
 * Check usb_ep_queue()* at usb_gadget.h" for details
 */
static int ep_queue(struct usb_ep *ep, struct usb_request *req,
		    gfp_t __maybe_unused gfp_flags)
{
	struct ci13xxx_ep  *mEp  = container_of(ep,  struct ci13xxx_ep, ep);
	struct ci13xxx_req *mReq = container_of(req, struct ci13xxx_req, req);
	int retval = 0;
	unsigned long flags;

	trace("%p, %p, %X", ep, req, gfp_flags);

	if (ep == NULL || req == NULL || mEp->desc == NULL)
		return -EINVAL;

	spin_lock_irqsave(mEp->lock, flags);

	if (mEp->type == USB_ENDPOINT_XFER_CONTROL &&
	    !list_empty(&mEp->qh.queue)) {
		_ep_nuke(mEp);
		retval = -EOVERFLOW;
		warn("endpoint ctrl %X nuked", _usb_addr(mEp));
	}

	/* first nuke then test link, e.g. previous status has not sent */
	if (!list_empty(&mReq->queue)) {
		retval = -EBUSY;
		err("request already in queue");
		goto done;
	}

	if (req->length > (4 * CI13XXX_PAGE_SIZE)) {
		req->length = (4 * CI13XXX_PAGE_SIZE);
		retval = -EMSGSIZE;
		warn("request length truncated");
	}

	dbg_queue(_usb_addr(mEp), req, retval);

	/* push request */
	mReq->req.status = -EINPROGRESS;
	mReq->req.actual = 0;

	retval = _hardware_enqueue(mEp, mReq);

	if (retval == -EALREADY) {
		dbg_event(_usb_addr(mEp), "QUEUE", retval);
		retval = 0;
	}
	if (!retval)
		list_add_tail(&mReq->queue, &mEp->qh.queue);

 done:
	spin_unlock_irqrestore(mEp->lock, flags);
	return retval;
}

/**
 * ep_dequeue: dequeues (cancels, unlinks) an I/O request from an endpoint
 *
 * Check usb_ep_dequeue() at "usb_gadget.h" for details
 */
static int ep_dequeue(struct usb_ep *ep, struct usb_request *req)
{
	struct ci13xxx_ep  *mEp  = container_of(ep,  struct ci13xxx_ep, ep);
	struct ci13xxx_req *mReq = container_of(req, struct ci13xxx_req, req);
	unsigned long flags;

	trace("%p, %p", ep, req);

	if (ep == NULL || req == NULL || mReq->req.status != -EALREADY ||
		mEp->desc == NULL || list_empty(&mReq->queue) ||
		list_empty(&mEp->qh.queue))
		return -EINVAL;

	spin_lock_irqsave(mEp->lock, flags);

	dbg_event(_usb_addr(mEp), "DEQUEUE", 0);

	hw_ep_flush(mEp->num, mEp->dir);

	/* pop request */
	list_del_init(&mReq->queue);
	if (mReq->map) {
		dma_unmap_single(mEp->device, mReq->req.dma, mReq->req.length,
				 mEp->dir ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
		mReq->req.dma = 0;
		mReq->map     = 0;
	}
	req->status = -ECONNRESET;

	if (mReq->req.complete != NULL) {
		spin_unlock(mEp->lock);
		mReq->req.complete(&mEp->ep, &mReq->req);
		spin_lock(mEp->lock);
	}

	spin_unlock_irqrestore(mEp->lock, flags);
	return 0;
}

/**
 * ep_set_halt: sets the endpoint halt feature
 *
 * Check usb_ep_set_halt() at "usb_gadget.h" for details
 */
static int ep_set_halt(struct usb_ep *ep, int value)
{
	struct ci13xxx_ep *mEp = container_of(ep, struct ci13xxx_ep, ep);
	int direction, retval = 0;
	unsigned long flags;

	trace("%p, %i", ep, value);

	if (ep == NULL || mEp->desc == NULL)
		return -EINVAL;

	spin_lock_irqsave(mEp->lock, flags);

#ifndef STALL_IN
	/* g_file_storage MS compliant but g_zero fails chapter 9 compliance */
	if (value && mEp->type == USB_ENDPOINT_XFER_BULK && mEp->dir == TX &&
	    !list_empty(&mEp->qh.queue)) {
		spin_unlock_irqrestore(mEp->lock, flags);
		return -EAGAIN;
	}
#endif

	direction = mEp->dir;
	do {
		dbg_event(_usb_addr(mEp), "HALT", value);
		retval |= hw_ep_set_halt(mEp->num, mEp->dir, value);

		if (!value)
			mEp->wedge = 0;

		if (mEp->type == USB_ENDPOINT_XFER_CONTROL)
			mEp->dir = (mEp->dir == TX) ? RX : TX;

	} while (mEp->dir != direction);

	spin_unlock_irqrestore(mEp->lock, flags);
	return retval;
}

/**
 * ep_set_wedge: sets the halt feature and ignores clear requests
 *
 * Check usb_ep_set_wedge() at "usb_gadget.h" for details
 */
static int ep_set_wedge(struct usb_ep *ep)
{
	struct ci13xxx_ep *mEp = container_of(ep, struct ci13xxx_ep, ep);
	unsigned long flags;

	trace("%p", ep);

	if (ep == NULL || mEp->desc == NULL)
		return -EINVAL;

	spin_lock_irqsave(mEp->lock, flags);

	dbg_event(_usb_addr(mEp), "WEDGE", 0);
	mEp->wedge = 1;

	spin_unlock_irqrestore(mEp->lock, flags);

	return usb_ep_set_halt(ep);
}

/**
 * ep_fifo_flush: flushes contents of a fifo
 *
 * Check usb_ep_fifo_flush() at "usb_gadget.h" for details
 */
static void ep_fifo_flush(struct usb_ep *ep)
{
	struct ci13xxx_ep *mEp = container_of(ep, struct ci13xxx_ep, ep);
	unsigned long flags;

	trace("%p", ep);

	if (ep == NULL) {
		err("%02X: -EINVAL", _usb_addr(mEp));
		return;
	}

	spin_lock_irqsave(mEp->lock, flags);

	dbg_event(_usb_addr(mEp), "FFLUSH", 0);
	hw_ep_flush(mEp->num, mEp->dir);

	spin_unlock_irqrestore(mEp->lock, flags);
}

/**
 * Endpoint-specific part of the API to the USB controller hardware
 * Check "usb_gadget.h" for details
 */
static const struct usb_ep_ops usb_ep_ops = {
	.enable	       = ep_enable,
	.disable       = ep_disable,
	.alloc_request = ep_alloc_request,
	.free_request  = ep_free_request,
	.queue	       = ep_queue,
	.dequeue       = ep_dequeue,
	.set_halt      = ep_set_halt,
	.set_wedge     = ep_set_wedge,
	.fifo_flush    = ep_fifo_flush,
};

/******************************************************************************
 * GADGET block
 *****************************************************************************/
static int ci13xxx_vbus_session(struct usb_gadget *_gadget, int is_active)
{
	struct ci13xxx *udc = container_of(_gadget, struct ci13xxx, gadget);
	unsigned long flags;
	int gadget_ready = 0;

	if (!(udc->udc_driver->flags & CI13XXX_PULLUP_ON_VBUS))
		return -EOPNOTSUPP;

	spin_lock_irqsave(udc->lock, flags);
	udc->vbus_active = is_active;
	if (udc->driver)
		gadget_ready = 1;
	spin_unlock_irqrestore(udc->lock, flags);

	if (gadget_ready) {
		if (is_active) {
			pm_runtime_get_sync(&_gadget->dev);
			hw_device_reset(udc);
			hw_device_state(udc->ep0out.qh.dma);
		} else {
			hw_device_state(0);
			if (udc->udc_driver->notify_event)
				udc->udc_driver->notify_event(udc,
				CI13XXX_CONTROLLER_STOPPED_EVENT);
			_gadget_stop_activity(&udc->gadget);
			pm_runtime_put_sync(&_gadget->dev);
		}
	}

	return 0;
}

static int ci13xxx_wakeup(struct usb_gadget *_gadget)
{
	struct ci13xxx *udc = container_of(_gadget, struct ci13xxx, gadget);
	unsigned long flags;
	int ret = 0;

	trace();

	spin_lock_irqsave(udc->lock, flags);
	if (!udc->remote_wakeup) {
		ret = -EOPNOTSUPP;
		dbg_trace("remote wakeup feature is not enabled\n");
		goto out;
	}
	if (!hw_cread(CAP_PORTSC, PORTSC_SUSP)) {
		ret = -EINVAL;
		dbg_trace("port is not suspended\n");
		goto out;
	}
	hw_cwrite(CAP_PORTSC, PORTSC_FPR, PORTSC_FPR);
out:
	spin_unlock_irqrestore(udc->lock, flags);
	return ret;
}

/**
 * Device operations part of the API to the USB controller hardware,
 * which don't involve endpoints (or i/o)
 * Check  "usb_gadget.h" for details
 */
static const struct usb_gadget_ops usb_gadget_ops = {
	.vbus_session	= ci13xxx_vbus_session,
	.wakeup		= ci13xxx_wakeup,
};

/**
 * usb_gadget_probe_driver: register a gadget driver
 * @driver: the driver being registered
 * @bind: the driver's bind callback
 *
 * Check usb_gadget_probe_driver() at <linux/usb/gadget.h> for details.
 * Interrupts are enabled here.
 */
int usb_gadget_probe_driver(struct usb_gadget_driver *driver,
		int (*bind)(struct usb_gadget *))
{
	struct ci13xxx *udc = _udc;
	unsigned long flags;
	int i, j;
	int retval = -ENOMEM;

	trace("%p", driver);

	if (driver             == NULL ||
	    bind               == NULL ||
	    driver->setup      == NULL ||
	    driver->disconnect == NULL ||
	    driver->suspend    == NULL ||
	    driver->resume     == NULL)
		return -EINVAL;
	else if (udc         == NULL)
		return -ENODEV;
	else if (udc->driver != NULL)
		return -EBUSY;

	/* alloc resources */
	udc->qh_pool = dma_pool_create("ci13xxx_qh", &udc->gadget.dev,
				       sizeof(struct ci13xxx_qh),
				       64, CI13XXX_PAGE_SIZE);
	if (udc->qh_pool == NULL)
		return -ENOMEM;

	udc->td_pool = dma_pool_create("ci13xxx_td", &udc->gadget.dev,
				       sizeof(struct ci13xxx_td),
				       64, CI13XXX_PAGE_SIZE);
	if (udc->td_pool == NULL) {
		dma_pool_destroy(udc->qh_pool);
		udc->qh_pool = NULL;
		return -ENOMEM;
	}

	spin_lock_irqsave(udc->lock, flags);

	info("hw_ep_max = %d", hw_ep_max);

	udc->gadget.dev.driver = NULL;

	retval = 0;
	for (i = 0; i < hw_ep_max/2; i++) {
		for (j = RX; j <= TX; j++) {
			int k = i + j * hw_ep_max/2;
			struct ci13xxx_ep *mEp = &udc->ci13xxx_ep[k];

			scnprintf(mEp->name, sizeof(mEp->name), "ep%i%s", i,
					(j == TX)  ? "in" : "out");

			mEp->lock         = udc->lock;
			mEp->device       = &udc->gadget.dev;
			mEp->td_pool      = udc->td_pool;

			mEp->ep.name      = mEp->name;
			mEp->ep.ops       = &usb_ep_ops;
			mEp->ep.maxpacket = CTRL_PAYLOAD_MAX;

			INIT_LIST_HEAD(&mEp->qh.queue);
			spin_unlock_irqrestore(udc->lock, flags);
			mEp->qh.ptr = dma_pool_alloc(udc->qh_pool, GFP_KERNEL,
					&mEp->qh.dma);
			spin_lock_irqsave(udc->lock, flags);
			if (mEp->qh.ptr == NULL)
				retval = -ENOMEM;
			else
				memset(mEp->qh.ptr, 0, sizeof(*mEp->qh.ptr));

			/* skip ep0 out and in endpoints */
			if (i == 0)
				continue;

			list_add_tail(&mEp->ep.ep_list, &udc->gadget.ep_list);
		}
	}
	if (retval)
		goto done;

	udc->gadget.ep0 = &udc->ep0in.ep;
	/* bind gadget */
	driver->driver.bus     = NULL;
	udc->gadget.dev.driver = &driver->driver;

	spin_unlock_irqrestore(udc->lock, flags);
	retval = bind(&udc->gadget);                /* MAY SLEEP */
	spin_lock_irqsave(udc->lock, flags);

	if (retval) {
		udc->gadget.dev.driver = NULL;
		goto done;
	}

	udc->driver = driver;
	pm_runtime_get_sync(&udc->gadget.dev);
	if (udc->udc_driver->flags & CI13XXX_PULLUP_ON_VBUS) {
		if (udc->vbus_active) {
			if (udc->udc_driver->flags & CI13XXX_REGS_SHARED)
				hw_device_reset(udc);
		} else {
			pm_runtime_put_sync(&udc->gadget.dev);
			goto done;
		}
	}

	retval = hw_device_state(udc->ep0out.qh.dma);
	if (retval)
		pm_runtime_put_sync(&udc->gadget.dev);

 done:
	spin_unlock_irqrestore(udc->lock, flags);
	return retval;
}
EXPORT_SYMBOL(usb_gadget_probe_driver);

/**
 * usb_gadget_unregister_driver: unregister a gadget driver
 *
 * Check usb_gadget_unregister_driver() at "usb_gadget.h" for details
 */
int usb_gadget_unregister_driver(struct usb_gadget_driver *driver)
{
	struct ci13xxx *udc = _udc;
	unsigned long i, flags;

	trace("%p", driver);

	if (driver             == NULL ||
	    driver->unbind     == NULL ||
	    driver->setup      == NULL ||
	    driver->disconnect == NULL ||
	    driver->suspend    == NULL ||
	    driver->resume     == NULL ||
	    driver             != udc->driver)
		return -EINVAL;

	spin_lock_irqsave(udc->lock, flags);

	if (!(udc->udc_driver->flags & CI13XXX_PULLUP_ON_VBUS) ||
			udc->vbus_active) {
		hw_device_state(0);
		if (udc->udc_driver->notify_event)
			udc->udc_driver->notify_event(udc,
			CI13XXX_CONTROLLER_STOPPED_EVENT);
		_gadget_stop_activity(&udc->gadget);
		pm_runtime_put(&udc->gadget.dev);
	}

	/* unbind gadget */
	spin_unlock_irqrestore(udc->lock, flags);
	driver->unbind(&udc->gadget);               /* MAY SLEEP */
	spin_lock_irqsave(udc->lock, flags);

	udc->gadget.dev.driver = NULL;

	/* free resources */
	for (i = 0; i < hw_ep_max; i++) {
		struct ci13xxx_ep *mEp = &udc->ci13xxx_ep[i];

		if (!list_empty(&mEp->ep.ep_list))
			list_del_init(&mEp->ep.ep_list);

		if (mEp->qh.ptr != NULL)
			dma_pool_free(udc->qh_pool, mEp->qh.ptr, mEp->qh.dma);
	}

	udc->gadget.ep0 = NULL;
	udc->driver = NULL;

	spin_unlock_irqrestore(udc->lock, flags);

	if (udc->td_pool != NULL) {
		dma_pool_destroy(udc->td_pool);
		udc->td_pool = NULL;
	}
	if (udc->qh_pool != NULL) {
		dma_pool_destroy(udc->qh_pool);
		udc->qh_pool = NULL;
	}

	return 0;
}
EXPORT_SYMBOL(usb_gadget_unregister_driver);

/******************************************************************************
 * BUS block
 *****************************************************************************/
/**
 * udc_irq: global interrupt handler
 *
 * This function returns IRQ_HANDLED if the IRQ has been handled
 * It locks access to registers
 */
static irqreturn_t udc_irq(void)
{
	struct ci13xxx *udc = _udc;
	irqreturn_t retval;
	u32 intr;

	trace();

	if (udc == NULL) {
		err("ENODEV");
		return IRQ_HANDLED;
	}

	spin_lock(udc->lock);

	if (udc->udc_driver->flags & CI13XXX_REGS_SHARED) {
		if (hw_cread(CAP_USBMODE, USBMODE_CM) !=
				USBMODE_CM_DEVICE) {
			spin_unlock(udc->lock);
			return IRQ_NONE;
		}
	}
	intr = hw_test_and_clear_intr_active();
	if (intr) {
		isr_statistics.hndl.buf[isr_statistics.hndl.idx++] = intr;
		isr_statistics.hndl.idx &= ISR_MASK;
		isr_statistics.hndl.cnt++;

		/* order defines priority - do NOT change it */
		if (USBi_URI & intr) {
			isr_statistics.uri++;
			isr_reset_handler(udc);
		}
		if (USBi_PCI & intr) {
			isr_statistics.pci++;
			udc->gadget.speed = hw_port_is_high_speed() ?
				USB_SPEED_HIGH : USB_SPEED_FULL;
			if (udc->suspended) {
				spin_unlock(udc->lock);
				udc->driver->resume(&udc->gadget);
				spin_lock(udc->lock);
				udc->suspended = 0;
			}
		}
		if (USBi_UEI & intr)
			isr_statistics.uei++;
		if (USBi_UI  & intr) {
			isr_statistics.ui++;
			isr_tr_complete_handler(udc);
		}
		if (USBi_SLI & intr) {
			if (udc->gadget.speed != USB_SPEED_UNKNOWN) {
				udc->suspended = 1;
				spin_unlock(udc->lock);
				udc->driver->suspend(&udc->gadget);
				spin_lock(udc->lock);
			}
			isr_statistics.sli++;
		}
		retval = IRQ_HANDLED;
	} else {
		isr_statistics.none++;
		retval = IRQ_NONE;
	}
	spin_unlock(udc->lock);

	return retval;
}

/**
 * udc_release: driver release function
 * @dev: device
 *
 * Currently does nothing
 */
static void udc_release(struct device *dev)
{
	trace("%p", dev);

	if (dev == NULL)
		err("EINVAL");
}

/**
 * udc_probe: parent probe must call this to initialize UDC
 * @dev:  parent device
 * @regs: registers base address
 * @name: driver name
 *
 * This function returns an error code
 * No interrupts active, the IRQ has not been requested yet
 * Kernel assumes 32-bit DMA operations by default, no need to dma_set_mask
 */
static int udc_probe(struct ci13xxx_udc_driver *driver, struct device *dev,
		void __iomem *regs)
{
	struct ci13xxx *udc;
	int retval = 0;

	trace("%p, %p, %p", dev, regs, name);

	if (dev == NULL || regs == NULL || driver == NULL ||
			driver->name == NULL)
		return -EINVAL;

	udc = kzalloc(sizeof(struct ci13xxx), GFP_KERNEL);
	if (udc == NULL)
		return -ENOMEM;

	udc->lock = &udc_lock;
	udc->regs = regs;
	udc->udc_driver = driver;

	udc->gadget.ops          = &usb_gadget_ops;
	udc->gadget.speed        = USB_SPEED_UNKNOWN;
	udc->gadget.is_dualspeed = 1;
	udc->gadget.is_otg       = 0;
	udc->gadget.name         = driver->name;

	INIT_LIST_HEAD(&udc->gadget.ep_list);
	udc->gadget.ep0 = NULL;

	dev_set_name(&udc->gadget.dev, "gadget");
	udc->gadget.dev.dma_mask = dev->dma_mask;
	udc->gadget.dev.coherent_dma_mask = dev->coherent_dma_mask;
	udc->gadget.dev.parent   = dev;
	udc->gadget.dev.release  = udc_release;

	retval = hw_device_init(regs);
	if (retval < 0)
		goto free_udc;

	udc->transceiver = otg_get_transceiver();

	if (udc->udc_driver->flags & CI13XXX_REQUIRE_TRANSCEIVER) {
		if (udc->transceiver == NULL) {
			retval = -ENODEV;
			goto free_udc;
		}
	}

	if (!(udc->udc_driver->flags & CI13XXX_REGS_SHARED)) {
		retval = hw_device_reset(udc);
		if (retval)
			goto put_transceiver;
	}

	retval = device_register(&udc->gadget.dev);
	if (retval) {
		put_device(&udc->gadget.dev);
		goto put_transceiver;
	}

#ifdef CONFIG_USB_GADGET_DEBUG_FILES
	retval = dbg_create_files(&udc->gadget.dev);
#endif
	if (retval)
		goto unreg_device;

	if (udc->transceiver) {
		retval = otg_set_peripheral(udc->transceiver, &udc->gadget);
		if (retval)
			goto remove_dbg;
	}
	pm_runtime_no_callbacks(&udc->gadget.dev);
	pm_runtime_enable(&udc->gadget.dev);

	_udc = udc;
	return retval;

	err("error = %i", retval);
remove_dbg:
#ifdef CONFIG_USB_GADGET_DEBUG_FILES
	dbg_remove_files(&udc->gadget.dev);
#endif
unreg_device:
	device_unregister(&udc->gadget.dev);
put_transceiver:
	if (udc->transceiver)
		otg_put_transceiver(udc->transceiver);
free_udc:
	kfree(udc);
	_udc = NULL;
	return retval;
}

/**
 * udc_remove: parent remove must call this to remove UDC
 *
 * No interrupts active, the IRQ has been released
 */
static void udc_remove(void)
{
	struct ci13xxx *udc = _udc;

	if (udc == NULL) {
		err("EINVAL");
		return;
	}

	if (udc->transceiver) {
		otg_set_peripheral(udc->transceiver, &udc->gadget);
		otg_put_transceiver(udc->transceiver);
	}
#ifdef CONFIG_USB_GADGET_DEBUG_FILES
	dbg_remove_files(&udc->gadget.dev);
#endif
	device_unregister(&udc->gadget.dev);

	kfree(udc);
	_udc = NULL;
}
