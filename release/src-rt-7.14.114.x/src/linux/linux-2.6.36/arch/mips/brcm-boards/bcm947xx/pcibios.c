/*
 * Low-Level PCI and SB support for BCM47xx (Linux support code)
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
 * $Id: pcibios.c,v 1.11 2011-01-10 23:25:05 $
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/paccess.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
#include <linux/config.h>
#endif

#include <typedefs.h>
#include <bcmutils.h>
#include <hndsoc.h>
#include <siutils.h>
#include <hndcpu.h>
#include <hndpci.h>
#include <pcicfg.h>
#include <bcmdevs.h>
#include <bcmnvram.h>

/* Global SB handle */
extern si_t *bcm947xx_sih;
extern spinlock_t bcm947xx_sih_lock;

/* Global USB capability */
int usb_hsic_cap = 0;
int usb_hsic_cap_port = 0;
EXPORT_SYMBOL(usb_hsic_cap);
EXPORT_SYMBOL(usb_hsic_cap_port);

/* Convenience */
#define sih bcm947xx_sih
#define sih_lock bcm947xx_sih_lock

static int
sbpci_read_config_reg(struct pci_bus *bus, unsigned int devfn, int where,
                      int size, u32 *value)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&sih_lock, flags);
	ret = hndpci_read_config(sih, bus->number, PCI_SLOT(devfn),
	                        PCI_FUNC(devfn), where, value, size);
	spin_unlock_irqrestore(&sih_lock, flags);
	return ret ? PCIBIOS_DEVICE_NOT_FOUND : PCIBIOS_SUCCESSFUL;
}

static int
sbpci_write_config_reg(struct pci_bus *bus, unsigned int devfn, int where,
                       int size, u32 value)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&sih_lock, flags);
	ret = hndpci_write_config(sih, bus->number, PCI_SLOT(devfn),
	                         PCI_FUNC(devfn), where, &value, size);
	spin_unlock_irqrestore(&sih_lock, flags);
	return ret ? PCIBIOS_DEVICE_NOT_FOUND : PCIBIOS_SUCCESSFUL;
}

static struct pci_ops pcibios_ops = {
	sbpci_read_config_reg,
	sbpci_write_config_reg
};

static u32 pci_iobase = 0x100;
static u32 pci_membase = SI_PCI_DMA;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
static struct resource bcm_pci_mem_resource = {
        .name   = "bcm947xx PCI memory space",
        .start  = SI_PCI_MEM  /* BCM_PCI_MEM_BASE_PA */,
        .end    = SI_PCI_MEM + SI_PCI_MEM_SZ /* BCM_PCI_MEM_END_PA */,
        .flags  = IORESOURCE_MEM
};

static struct resource bcm_pci_io_resource = {
        .name   = "bcm947xx PCI IO space",
        .start  = 0xc0000000 /* BCM_PCI_IO_BASE_PA */,
        .end    = 0xc0001000 /* BCM_PCI_IO_END_PA */,
        .flags  = IORESOURCE_IO
};

struct pci_controller bcm947xxcontroller = {
        .pci_ops        = &pcibios_ops,
        .io_resource    = &bcm_pci_io_resource,
        .mem_resource   = &bcm_pci_mem_resource,
};
#endif

void __init
pcibios_init(void)
{
	ulong flags;

	/* For 4716, use sbtopcie0 to access the device. We
	 * can't use address match 2 (1 GB window) region as MIPS
	 * can not generate 64-bit address on the backplane.
	 */
	if (sih->chip == BCM4716_CHIP_ID) {
		printk(KERN_INFO "PCI: Using membase %x\n", SI_PCI_MEM);
		pci_membase = SI_PCI_MEM;
	}

	if (!(sih = si_kattach(SI_OSH)))
		panic("si_kattach failed");
	spin_lock_init(&sih_lock);

	spin_lock_irqsave(&sih_lock, flags);
	hndpci_init(sih);
	spin_unlock_irqrestore(&sih_lock, flags);

	set_io_port_base((unsigned long) ioremap_nocache(SI_PCI_MEM, 0x04000000));

	/* Scan the SB bus */
	printk(KERN_INFO "PCI: scanning bus %x\n", 0);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
	register_pci_controller( &bcm947xxcontroller );
	pci_scan_bus(0, &pcibios_ops, &bcm947xxcontroller);
#else
	pci_scan_bus(0, &pcibios_ops, NULL);
#endif
}

char * __init
pcibios_setup(char *str)
{
	if (!strncmp(str, "ban=", 4)) {
		hndpci_ban(simple_strtoul(str + 4, NULL, 0));
		return NULL;
	}

	return (str);
}

void __devinit
pcibios_fixup_bus(struct pci_bus *b)
{
	struct list_head *ln;
	struct pci_dev *d, *dev;
	struct resource *res;
	int pos, size;
	u32 *base;
	u8 irq;

	printk("PCI: Fixing up bus %d\n", b->number);

	/* Fix up SB */
	if (b->number == 0) {
		for (ln = b->devices.next; ln != &b->devices; ln = ln->next) {
			d = pci_dev_b(ln);
			/* Fix up interrupt lines */
			pci_read_config_byte(d, PCI_INTERRUPT_LINE, &irq);
			d->irq = irq + 2;
			pci_write_config_byte(d, PCI_INTERRUPT_LINE, d->irq);
		}
	} else {
		irq = 0;
		/* Find the corresponding IRQ of the PCI/PCIe core per bus number */
		/* All devices on the bus use the same IRQ as the core */
		list_for_each_entry(dev, &((pci_find_bus(0, 0))->devices), bus_list) {
			if ((dev != NULL) &&
			    ((dev->device == PCI_CORE_ID) ||
			    (dev->device == PCIE_CORE_ID))) {
				if (dev->subordinate && dev->subordinate->number == b->number) {
					irq = dev->irq;
					break;
				}
			}
		}

		pci_membase = hndpci_get_membase(b->number);
		/* Fix up external PCI */
		for (ln = b->devices.next; ln != &b->devices; ln = ln->next) {
			bool is_hostbridge;

			d = pci_dev_b(ln);
			is_hostbridge = hndpci_is_hostbridge(b->number, PCI_SLOT(d->devfn));
			/* Fix up resource bases */
			for (pos = 0; pos < 6; pos++) {
				res = &d->resource[pos];
				base = (res->flags & IORESOURCE_IO) ? &pci_iobase : &pci_membase;
				if (res->end) {
					size = res->end - res->start + 1;
					if (*base & (size - 1))
						*base = (*base + size) & ~(size - 1);
					res->start = *base;
					res->end = res->start + size - 1;
					*base += size;
					pci_write_config_dword(d,
						PCI_BASE_ADDRESS_0 + (pos << 2), res->start);
				}
				/* Fix up PCI bridge BAR0 only */
				if (is_hostbridge)
					break;
			}
			/* Fix up interrupt lines */
			d->irq = irq;
			pci_write_config_byte(d, PCI_INTERRUPT_LINE, d->irq);
		}
		hndpci_arb_park(sih, PCI_PARK_NVRAM);
	}
}

int
pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	return (dev->irq);
}

int
pcibios_enable_resources(struct pci_dev *dev)
{
	u16 cmd, old_cmd;
	int idx;
	struct resource *r;

	/* External PCI only */
	if (dev->bus->number == 0)
		return 0;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	old_cmd = cmd;
	for (idx = 0; idx < 6; idx++) {
		r = &dev->resource[idx];
		if (r->flags & IORESOURCE_IO)
			cmd |= PCI_COMMAND_IO;
		if (r->flags & IORESOURCE_MEM)
			cmd |= PCI_COMMAND_MEMORY;
	}
	if (dev->resource[PCI_ROM_RESOURCE].start)
		cmd |= PCI_COMMAND_MEMORY;
	if (cmd != old_cmd) {
		printk("PCI: Enabling device %s (%04x -> %04x)\n", pci_name(dev), old_cmd, cmd);
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
	return 0;
}

int
pcibios_enable_device(struct pci_dev *dev, int mask)
{
	ulong flags;
	uint coreidx, coreid;
	void *regs;
	int rc = -1;

	/* External PCI device enable */
	if (dev->bus->number != 0)
		return pcibios_enable_resources(dev);

	/* These cores come out of reset enabled */
	if (dev->device == MIPS_CORE_ID ||
	    dev->device == MIPS33_CORE_ID ||
	    dev->device == CC_CORE_ID)
		return 0;

	spin_lock_irqsave(&sih_lock, flags);
	
	regs = si_setcoreidx(sih, PCI_SLOT(dev->devfn));
	coreidx = si_coreidx(sih);
	coreid = si_coreid(sih);

	if (!regs) {
		printk(KERN_ERR "WARNING! PCIBIOS_DEVICE_NOT_FOUND\n");
		goto out;
	}

	/* 
	 * The USB core requires a special bit to be set during core
	 * reset to enable host (OHCI) mode. Resetting the SB core in
	 * pcibios_enable_device() is a hack for compatibility with
	 * vanilla usb-ohci so that it does not have to know about
	 * SB. A driver that wants to use the USB core in device mode
	 * should know about SB and should reset the bit back to 0
	 * after calling pcibios_enable_device().
	 */
	if (coreid == USB_CORE_ID) {
		si_core_disable(sih, si_core_cflags(sih, 0, 0));
		si_core_reset(sih, 1 << 29, 0);
	}
	/*
	 * USB 2.0 special considerations:
	 *
	 * 1. Since the core supports both OHCI and EHCI functions, it must
	 *    only be reset once.
	 *
	 * 2. In addition to the standard SB reset sequence, the Host Control
	 *    Register must be programmed to bring the USB core and various
	 *    phy components out of reset.
	 */
	else if (coreid == USB20H_CORE_ID) {
		if (!si_iscoreup(sih)) {
			si_core_reset(sih, 0, 0);
			mdelay(10);
			if (si_corerev(sih) >= 5) {
				uint32 tmp;
				/* Enable Misc PLL */
				tmp = readl(regs + 0x1e0);
				tmp |= 0x100;
				writel(tmp, regs + 0x1e0);
				SPINWAIT((((tmp = readl(regs + 0x1e0)) & (1 << 24))
					== 0), 1000);
				/* Take out of resets */
				writel(0x4ff, regs + 0x200);
				udelay(25);
				writel(0x6ff, regs + 0x200);
				udelay(25);

				/* Make sure digital and AFE are locked in USB PHY */
				writel(0x6b, regs + 0x524);
				udelay(50);
				tmp = readl(regs + 0x524);
				udelay(50);
				writel(0xab, regs + 0x524);
				udelay(50);
				tmp = readl(regs + 0x524);
				udelay(50);
				writel(0x2b, regs + 0x524);
				udelay(50);
				tmp = readl(regs + 0x524);
				udelay(50);
				writel(0x10ab, regs + 0x524);
				udelay(50);
				tmp = readl(regs + 0x524);
				SPINWAIT((((tmp = readl(regs + 0x528)) & 0xc000) !=
					0xc000), 100000);
				if ((tmp & 0xc000) != 0xc000) {
					printk(KERN_WARNING "WARNING! USB20H mdio_rddata 0x%08x\n", tmp);
					goto out;
				}
				writel(0x80000000, regs + 0x528);
				tmp = readl(regs + 0x314);
				udelay(265);
				writel(0x7ff, regs + 0x200);
				udelay(10);

				/* Take USB and HSIC out of non-driving modes */
				writel(0, regs + 0x510);
			} else {
				writel(0x7ff, regs + 0x200);
				udelay(1);
			}
		}
		/* PRxxxx: War for 5354 failures. */
		if (si_corerev(sih) == 1) {
			uint32 tmp;

			/* Change Flush control reg */
			tmp = readl(regs + 0x400);
			tmp &= ~8;
			writel(tmp, regs + 0x400);
			tmp = readl(regs + 0x400);
			printk(KERN_DEBUG "USB20H fcr: 0x%x\n", tmp);

			/* Change Shim control reg */
			tmp = readl(regs + 0x304);
			tmp &= ~0x100;
			writel(tmp, regs + 0x304);
			tmp = readl(regs + 0x304);
			printk(KERN_DEBUG "USB20H shim cr: 0x%x\n", tmp);
		}
		if (si_corerev(sih) == 5) {
			usb_hsic_cap = 1;
			usb_hsic_cap_port = 2;
		}
	} else {
		si_core_reset(sih, 0, 0);
	}

	/* Initialize USBHC core OK */
	rc = 0;
out:
	si_setcoreidx(sih, coreidx);
	spin_unlock_irqrestore(&sih_lock, flags);

	/* Reset the device */
	if (coreid == USB20H_CORE_ID || coreid == USB_CORE_ID)	{
		int wombo_reset = GPIO_PIN_NOTDEFINED;

		if ((wombo_reset = getgpiopin(NULL, "wombo_reset", GPIO_PIN_NOTDEFINED)) !=
	    GPIO_PIN_NOTDEFINED) {
			int reset = 1 << wombo_reset;

			printk("wombo_reset set to gpio %d\n", wombo_reset);

			si_gpioout(sih, reset, 0, GPIO_DRV_PRIORITY);
			si_gpioouten(sih, reset, reset, GPIO_DRV_PRIORITY);
			mdelay(50);

			si_gpioout(sih, reset, reset, GPIO_DRV_PRIORITY);
			mdelay(20);
		}
	}

	return rc;
}

void
pcibios_update_resource(struct pci_dev *dev, struct resource *root,
	struct resource *res, int resource)
{
	unsigned long where, size;
	u32 reg;

	/* External PCI only */
	if (dev->bus->number == 0)
		return;

	where = PCI_BASE_ADDRESS_0 + (resource * 4);
	size = res->end - res->start;
	pci_read_config_dword(dev, where, &reg);
	reg = (reg & size) | (((u32)(res->start - root->start)) & ~size);
	pci_write_config_dword(dev, where, reg);
}

static void __init
quirk_sbpci_bridge(struct pci_dev *dev)
{
	if (dev->bus->number == 0 ||
		!hndpci_is_hostbridge(dev->bus->number, PCI_SLOT(dev->devfn)))
		return;

	printk("PCI: Fixing up bridge\n");

	/* Enable PCI bridge bus mastering and memory space */
	pci_set_master(dev);
	pcibios_enable_resources(dev);

	/* Enable PCI bridge BAR1 prefetch and burst */
	pci_write_config_dword(dev, PCI_BAR1_CONTROL, 3);
}

DECLARE_PCI_FIXUP_HEADER(PCI_ANY_ID, PCI_ANY_ID, quirk_sbpci_bridge);
