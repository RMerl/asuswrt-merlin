/*
 * Sonics Silicon Backplane
 * Broadcom USB-core OHCI driver
 *
 * Copyright 2007 Michael Buesch <mb@bu3sch.de>
 *
 * Derived from the OHCI-PCI driver
 * Copyright 1999 Roman Weissgaerber
 * Copyright 2000-2002 David Brownell
 * Copyright 1999 Linus Torvalds
 * Copyright 1999 Gregory P. Smith
 *
 * Derived from the USBcore related parts of Broadcom-SB
 * Copyright 2005 Broadcom Corporation
 *
 * Licensed under the GNU/GPL. See COPYING for details.
 */
#include <linux/ssb/ssb.h>


#define SSB_OHCI_TMSLOW_HOSTMODE	(1 << 29)

struct ssb_ohci_device {
	struct ohci_hcd ohci; /* _must_ be at the beginning. */

	u32 enable_flags;
};

static inline
struct ssb_ohci_device *hcd_to_ssb_ohci(struct usb_hcd *hcd)
{
	return (struct ssb_ohci_device *)(hcd->hcd_priv);
}


static int ssb_ohci_reset(struct usb_hcd *hcd)
{
	struct ssb_ohci_device *ohcidev = hcd_to_ssb_ohci(hcd);
	struct ohci_hcd *ohci = &ohcidev->ohci;
	int err;

	ohci_hcd_init(ohci);
	err = ohci_init(ohci);

	return err;
}

static int ssb_ohci_start(struct usb_hcd *hcd)
{
	struct ssb_ohci_device *ohcidev = hcd_to_ssb_ohci(hcd);
	struct ohci_hcd *ohci = &ohcidev->ohci;
	int err;

	err = ohci_run(ohci);
	if (err < 0) {
		ohci_err(ohci, "can't start\n");
		ohci_stop(hcd);
	}

	return err;
}

static const struct hc_driver ssb_ohci_hc_driver = {
	.description		= "ssb-usb-ohci",
	.product_desc		= "SSB OHCI Controller",
	.hcd_priv_size		= sizeof(struct ssb_ohci_device),

	.irq			= ohci_irq,
	.flags			= HCD_MEMORY | HCD_USB11,

	.reset			= ssb_ohci_reset,
	.start			= ssb_ohci_start,
	.stop			= ohci_stop,
	.shutdown		= ohci_shutdown,

	.urb_enqueue		= ohci_urb_enqueue,
	.urb_dequeue		= ohci_urb_dequeue,
	.endpoint_disable	= ohci_endpoint_disable,

	.get_frame_number	= ohci_get_frame,

	.hub_status_data	= ohci_hub_status_data,
	.hub_control		= ohci_hub_control,
#ifdef	CONFIG_PM
	.bus_suspend		= ohci_bus_suspend,
	.bus_resume		= ohci_bus_resume,
#endif

	.start_port_reset	= ohci_start_port_reset,
};

static void ssb_ohci_detach(struct ssb_device *dev)
{
	struct usb_hcd *hcd = ssb_get_drvdata(dev);

	if (hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);
	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);
	ssb_device_disable(dev, 0);
}

static int ssb_ohci_attach(struct ssb_device *dev)
{
	struct ssb_ohci_device *ohcidev;
	struct usb_hcd *hcd;
	int err = -ENOMEM;
	u32 tmp, flags = 0;

	if (dma_set_mask(dev->dma_dev, DMA_BIT_MASK(32)) ||
	    dma_set_coherent_mask(dev->dma_dev, DMA_BIT_MASK(32)))
		return -EOPNOTSUPP;

	if (dev->id.coreid == SSB_DEV_USB11_HOSTDEV) {
		/* Put the device into host-mode. */
		flags |= SSB_OHCI_TMSLOW_HOSTMODE;
		ssb_device_enable(dev, flags);
	} else if (dev->id.coreid == SSB_DEV_USB20_HOST) {
		/*
		 * USB 2.0 special considerations:
		 *
		 * In addition to the standard SSB reset sequence, the Host
		 * Control Register must be programmed to bring the USB core
		 * and various phy components out of reset.
		 */
		ssb_device_enable(dev, 0);
		ssb_write32(dev, 0x200, 0x7ff);

		/* Change Flush control reg */
		tmp = ssb_read32(dev, 0x400);
		tmp &= ~8;
		ssb_write32(dev, 0x400, tmp);
		tmp = ssb_read32(dev, 0x400);

		/* Change Shim control reg */
		tmp = ssb_read32(dev, 0x304);
		tmp &= ~0x100;
		ssb_write32(dev, 0x304, tmp);
		tmp = ssb_read32(dev, 0x304);

		udelay(1);

		if (dev->id.revision == 2 && dev->bus->chip_id == 0x5354) {
			/* Change syn01 reg */
			tmp = 0x00fe00fe;
			ssb_write32(dev, 0x894, tmp);

			/* Change syn03 reg */
			tmp = ssb_read32(dev, 0x89c);
			tmp |= 0x1;
			ssb_write32(dev, 0x89c, tmp);
		}
	} else
		ssb_device_enable(dev, 0);

	hcd = usb_create_hcd(&ssb_ohci_hc_driver, dev->dev,
			dev_name(dev->dev));
	if (!hcd)
		goto err_dev_disable;
	ohcidev = hcd_to_ssb_ohci(hcd);
	ohcidev->enable_flags = flags;

	tmp = ssb_read32(dev, SSB_ADMATCH0);
	hcd->rsrc_start = ssb_admatch_base(tmp);
	hcd->rsrc_len = ssb_admatch_size(tmp);
	hcd->regs = ioremap_nocache(hcd->rsrc_start, hcd->rsrc_len);
	if (!hcd->regs)
		goto err_put_hcd;
	err = usb_add_hcd(hcd, dev->irq, IRQF_DISABLED | IRQF_SHARED);
	if (err)
		goto err_iounmap;

	ssb_set_drvdata(dev, hcd);

	return err;

err_iounmap:
	iounmap(hcd->regs);
err_put_hcd:
	usb_put_hcd(hcd);
err_dev_disable:
	ssb_device_disable(dev, flags);
	return err;
}

static int ssb_ohci_probe(struct ssb_device *dev,
		const struct ssb_device_id *id)
{
	int err;
	u16 chipid_top;

	/* USBcores are only connected on embedded devices. */
	chipid_top = (dev->bus->chip_id & 0xFF00);
	if (chipid_top != 0x4700 && chipid_top != 0x5300)
		return -ENODEV;

	/* TODO: Probably need checks here; is the core connected? */

	if (usb_disabled())
		return -ENODEV;

	/* We currently always attach SSB_DEV_USB11_HOSTDEV
	 * as HOST OHCI. If we want to attach it as Client device,
	 * we must branch here and call into the (yet to
	 * be written) Client mode driver. Same for remove(). */

	err = ssb_ohci_attach(dev);

	return err;
}

static void ssb_ohci_remove(struct ssb_device *dev)
{
	ssb_ohci_detach(dev);
}

#ifdef CONFIG_PM

static int ssb_ohci_suspend(struct ssb_device *dev, pm_message_t state)
{
	ssb_device_disable(dev, 0);

	return 0;
}

static int ssb_ohci_resume(struct ssb_device *dev)
{
	struct usb_hcd *hcd = ssb_get_drvdata(dev);
	struct ssb_ohci_device *ohcidev = hcd_to_ssb_ohci(hcd);

	ssb_device_enable(dev, ohcidev->enable_flags);

	ohci_finish_controller_resume(hcd);
	return 0;
}

#else /* !CONFIG_PM */
#define ssb_ohci_suspend	NULL
#define ssb_ohci_resume	NULL
#endif /* CONFIG_PM */

static const struct ssb_device_id ssb_ohci_table[] = {
	SSB_DEVICE(SSB_VENDOR_BROADCOM, SSB_DEV_USB11_HOSTDEV, SSB_ANY_REV),
	SSB_DEVICE(SSB_VENDOR_BROADCOM, SSB_DEV_USB11_HOST, SSB_ANY_REV),
	SSB_DEVICE(SSB_VENDOR_BROADCOM, SSB_DEV_USB20_HOST, SSB_ANY_REV),
	SSB_DEVTABLE_END
};
MODULE_DEVICE_TABLE(ssb, ssb_ohci_table);

static struct ssb_driver ssb_ohci_driver = {
	.name		= KBUILD_MODNAME,
	.id_table	= ssb_ohci_table,
	.probe		= ssb_ohci_probe,
	.remove		= ssb_ohci_remove,
	.suspend	= ssb_ohci_suspend,
	.resume		= ssb_ohci_resume,
};
