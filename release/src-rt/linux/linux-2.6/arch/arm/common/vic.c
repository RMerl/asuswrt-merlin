/*
 *  linux/arch/arm/common/vic.c
 *
 *  Copyright (C) 1999 - 2003 ARM Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/init.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/sysdev.h>
#include <linux/device.h>
#include <linux/amba/bus.h>

#include <asm/mach/irq.h>
#include <asm/hardware/vic.h>

#if defined(CONFIG_PM)
/**
 * struct vic_device - VIC PM device
 * @sysdev: The system device which is registered.
 * @irq: The IRQ number for the base of the VIC.
 * @base: The register base for the VIC.
 * @resume_sources: A bitmask of interrupts for resume.
 * @resume_irqs: The IRQs enabled for resume.
 * @int_select: Save for VIC_INT_SELECT.
 * @int_enable: Save for VIC_INT_ENABLE.
 * @soft_int: Save for VIC_INT_SOFT.
 * @protect: Save for VIC_PROTECT.
 */
struct vic_device {
	struct sys_device sysdev;

	void __iomem	*base;
	int		irq;
	u32		resume_sources;
	u32		resume_irqs;
	u32		int_select;
	u32		int_enable;
	u32		soft_int;
	u32		protect;
};

/* we cannot allocate memory when VICs are initially registered */
static struct vic_device vic_devices[CONFIG_ARM_VIC_NR];

static int vic_id;

static inline struct vic_device *to_vic(struct sys_device *sys)
{
	return container_of(sys, struct vic_device, sysdev);
}
#endif /* CONFIG_PM */

/**
 * vic_init2 - common initialisation code
 * @base: Base of the VIC.
 *
 * Common initialisation code for registration
 * and resume.
*/
static void vic_init2(void __iomem *base)
{
	int i;

	for (i = 0; i < 16; i++) {
		void __iomem *reg = base + VIC_VECT_CNTL0 + (i * 4);
		writel(VIC_VECT_CNTL_ENABLE | i, reg);
	}

	writel(32, base + VIC_PL190_DEF_VECT_ADDR);
}

#if defined(CONFIG_PM)
static int vic_class_resume(struct sys_device *dev)
{
	struct vic_device *vic = to_vic(dev);
	void __iomem *base = vic->base;

	printk(KERN_DEBUG "%s: resuming vic at %p\n", __func__, base);

	/* re-initialise static settings */
	vic_init2(base);

	writel(vic->int_select, base + VIC_INT_SELECT);
	writel(vic->protect, base + VIC_PROTECT);

	/* set the enabled ints and then clear the non-enabled */
	writel(vic->int_enable, base + VIC_INT_ENABLE);
	writel(~vic->int_enable, base + VIC_INT_ENABLE_CLEAR);

	/* and the same for the soft-int register */

	writel(vic->soft_int, base + VIC_INT_SOFT);
	writel(~vic->soft_int, base + VIC_INT_SOFT_CLEAR);

	return 0;
}

static int vic_class_suspend(struct sys_device *dev, pm_message_t state)
{
	struct vic_device *vic = to_vic(dev);
	void __iomem *base = vic->base;

	printk(KERN_DEBUG "%s: suspending vic at %p\n", __func__, base);

	vic->int_select = readl(base + VIC_INT_SELECT);
	vic->int_enable = readl(base + VIC_INT_ENABLE);
	vic->soft_int = readl(base + VIC_INT_SOFT);
	vic->protect = readl(base + VIC_PROTECT);

	/* set the interrupts (if any) that are used for
	 * resuming the system */

	writel(vic->resume_irqs, base + VIC_INT_ENABLE);
	writel(~vic->resume_irqs, base + VIC_INT_ENABLE_CLEAR);

	return 0;
}

struct sysdev_class vic_class = {
	.name		= "vic",
	.suspend	= vic_class_suspend,
	.resume		= vic_class_resume,
};

/**
 * vic_pm_init - initicall to register VIC pm
 *
 * This is called via late_initcall() to register
 * the resources for the VICs due to the early
 * nature of the VIC's registration.
*/
static int __init vic_pm_init(void)
{
	struct vic_device *dev = vic_devices;
	int err;
	int id;

	if (vic_id == 0)
		return 0;

	err = sysdev_class_register(&vic_class);
	if (err) {
		printk(KERN_ERR "%s: cannot register class\n", __func__);
		return err;
	}

	for (id = 0; id < vic_id; id++, dev++) {
		dev->sysdev.id = id;
		dev->sysdev.cls = &vic_class;

		err = sysdev_register(&dev->sysdev);
		if (err) {
			printk(KERN_ERR "%s: failed to register device\n",
			       __func__);
			return err;
		}
	}

	return 0;
}
late_initcall(vic_pm_init);

/**
 * vic_pm_register - Register a VIC for later power management control
 * @base: The base address of the VIC.
 * @irq: The base IRQ for the VIC.
 * @resume_sources: bitmask of interrupts allowed for resume sources.
 *
 * Register the VIC with the system device tree so that it can be notified
 * of suspend and resume requests and ensure that the correct actions are
 * taken to re-instate the settings on resume.
 */
static void __init vic_pm_register(void __iomem *base, unsigned int irq, u32 resume_sources)
{
	struct vic_device *v;

	if (vic_id >= ARRAY_SIZE(vic_devices))
		printk(KERN_ERR "%s: too few VICs, increase CONFIG_ARM_VIC_NR\n", __func__);
	else {
		v = &vic_devices[vic_id];
		v->base = base;
		v->resume_sources = resume_sources;
		v->irq = irq;
		vic_id++;
	}
}
#else
static inline void vic_pm_register(void __iomem *base, unsigned int irq, u32 arg1) { }
#endif /* CONFIG_PM */

static void vic_ack_irq(struct irq_data *d)
{
	void __iomem *base = irq_data_get_irq_chip_data(d);
	unsigned int irq = d->irq & 31;
	writel(1 << irq, base + VIC_INT_ENABLE_CLEAR);
	/* moreover, clear the soft-triggered, in case it was the reason */
	writel(1 << irq, base + VIC_INT_SOFT_CLEAR);
}

static void vic_mask_irq(struct irq_data *d)
{
	void __iomem *base = irq_data_get_irq_chip_data(d);
	unsigned int irq = d->irq & 31;
	writel(1 << irq, base + VIC_INT_ENABLE_CLEAR);
}

static void vic_unmask_irq(struct irq_data *d)
{
	void __iomem *base = irq_data_get_irq_chip_data(d);
	unsigned int irq = d->irq & 31;
	writel(1 << irq, base + VIC_INT_ENABLE);
}

#if defined(CONFIG_PM)
static struct vic_device *vic_from_irq(unsigned int irq)
{
        struct vic_device *v = vic_devices;
	unsigned int base_irq = irq & ~31;
	int id;

	for (id = 0; id < vic_id; id++, v++) {
		if (v->irq == base_irq)
			return v;
	}

	return NULL;
}

static int vic_set_wake(struct irq_data *d, unsigned int on)
{
	struct vic_device *v = vic_from_irq(d->irq);
	unsigned int off = d->irq & 31;
	u32 bit = 1 << off;

	if (!v)
		return -EINVAL;

	if (!(bit & v->resume_sources))
		return -EINVAL;

	if (on)
		v->resume_irqs |= bit;
	else
		v->resume_irqs &= ~bit;

	return 0;
}
#else
#define vic_set_wake NULL
#endif /* CONFIG_PM */

static struct irq_chip vic_chip = {
	.name		= "VIC",
	.irq_ack	= vic_ack_irq,
	.irq_mask	= vic_mask_irq,
	.irq_unmask	= vic_unmask_irq,
	.irq_set_wake	= vic_set_wake,
};

static void __init vic_disable(void __iomem *base)
{
	writel(0, base + VIC_INT_SELECT);
	writel(0, base + VIC_INT_ENABLE);
	writel(~0, base + VIC_INT_ENABLE_CLEAR);
	writel(0, base + VIC_IRQ_STATUS);
	writel(0, base + VIC_ITCR);
	writel(~0, base + VIC_INT_SOFT_CLEAR);
}

static void __init vic_clear_interrupts(void __iomem *base)
{
	unsigned int i;

	writel(0, base + VIC_PL190_VECT_ADDR);
	for (i = 0; i < 19; i++) {
		unsigned int value;

		value = readl(base + VIC_PL190_VECT_ADDR);
		writel(value, base + VIC_PL190_VECT_ADDR);
	}
}

static void __init vic_set_irq_sources(void __iomem *base,
				unsigned int irq_start, u32 vic_sources)
{
	unsigned int i;

	for (i = 0; i < 32; i++) {
		if (vic_sources & (1 << i)) {
			unsigned int irq = irq_start + i;

			irq_set_chip_and_handler(irq, &vic_chip,
						 handle_level_irq);
			irq_set_chip_data(irq, base);
			set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
		}
	}
}

/*
 * The PL190 cell from ARM has been modified by ST to handle 64 interrupts.
 * The original cell has 32 interrupts, while the modified one has 64,
 * replocating two blocks 0x00..0x1f in 0x20..0x3f. In that case
 * the probe function is called twice, with base set to offset 000
 *  and 020 within the page. We call this "second block".
 */
static void __init vic_init_st(void __iomem *base, unsigned int irq_start,
				u32 vic_sources)
{
	unsigned int i;
	int vic_2nd_block = ((unsigned long)base & ~PAGE_MASK) != 0;

	/* Disable all interrupts initially. */
	vic_disable(base);

	/*
	 * Make sure we clear all existing interrupts. The vector registers
	 * in this cell are after the second block of general registers,
	 * so we can address them using standard offsets, but only from
	 * the second base address, which is 0x20 in the page
	 */
	if (vic_2nd_block) {
		vic_clear_interrupts(base);

		/* ST has 16 vectors as well, but we don't enable them by now */
		for (i = 0; i < 16; i++) {
			void __iomem *reg = base + VIC_VECT_CNTL0 + (i * 4);
			writel(0, reg);
		}

		writel(32, base + VIC_PL190_DEF_VECT_ADDR);
	}

	vic_set_irq_sources(base, irq_start, vic_sources);
}

/**
 * vic_init - initialise a vectored interrupt controller
 * @base: iomem base address
 * @irq_start: starting interrupt number, must be muliple of 32
 * @vic_sources: bitmask of interrupt sources to allow
 * @resume_sources: bitmask of interrupt sources to allow for resume
 */
void __init vic_init(void __iomem *base, unsigned int irq_start,
		     u32 vic_sources, u32 resume_sources)
{
	unsigned int i;
	u32 cellid = 0;
	enum amba_vendor vendor;

	/* Identify which VIC cell this one is, by reading the ID */
	for (i = 0; i < 4; i++) {
		u32 addr = ((u32)base & PAGE_MASK) + 0xfe0 + (i * 4);
		cellid |= (readl(addr) & 0xff) << (8 * i);
	}
	vendor = (cellid >> 12) & 0xff;
	printk(KERN_INFO "VIC @%p: id 0x%08x, vendor 0x%02x\n",
	       base, cellid, vendor);

	switch(vendor) {
	case AMBA_VENDOR_ST:
		vic_init_st(base, irq_start, vic_sources);
		return;
	default:
		printk(KERN_WARNING "VIC: unknown vendor, continuing anyways\n");
		/* fall through */
	case AMBA_VENDOR_ARM:
		break;
	}

	/* Disable all interrupts initially. */
	vic_disable(base);

	/* Make sure we clear all existing interrupts */
	vic_clear_interrupts(base);

	vic_init2(base);

	vic_set_irq_sources(base, irq_start, vic_sources);

	vic_pm_register(base, irq_start, resume_sources);
}
