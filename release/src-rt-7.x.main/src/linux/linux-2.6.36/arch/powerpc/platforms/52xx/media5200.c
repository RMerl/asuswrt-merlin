/*
 * Support for 'media5200-platform' compatible boards.
 *
 * Copyright (C) 2008 Secret Lab Technologies Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Description:
 * This code implements support for the Freescape Media5200 platform
 * (built around the MPC5200 SoC).
 *
 * Notable characteristic of the Media5200 is the presence of an FPGA
 * that has all external IRQ lines routed through it.  This file implements
 * a cascaded interrupt controller driver which attaches itself to the
 * Virtual IRQ subsystem after the primary mpc5200 interrupt controller
 * is initialized.
 *
 */

#undef DEBUG

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <asm/time.h>
#include <asm/prom.h>
#include <asm/machdep.h>
#include <asm/mpc52xx.h>

static struct of_device_id mpc5200_gpio_ids[] __initdata = {
	{ .compatible = "fsl,mpc5200-gpio", },
	{ .compatible = "mpc5200-gpio", },
	{}
};

/* FPGA register set */
#define MEDIA5200_IRQ_ENABLE (0x40c)
#define MEDIA5200_IRQ_STATUS (0x410)
#define MEDIA5200_NUM_IRQS   (6)
#define MEDIA5200_IRQ_SHIFT  (32 - MEDIA5200_NUM_IRQS)

struct media5200_irq {
	void __iomem *regs;
	spinlock_t lock;
	struct irq_host *irqhost;
};
struct media5200_irq media5200_irq;

static void media5200_irq_unmask(unsigned int virq)
{
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&media5200_irq.lock, flags);
	val = in_be32(media5200_irq.regs + MEDIA5200_IRQ_ENABLE);
	val |= 1 << (MEDIA5200_IRQ_SHIFT + irq_map[virq].hwirq);
	out_be32(media5200_irq.regs + MEDIA5200_IRQ_ENABLE, val);
	spin_unlock_irqrestore(&media5200_irq.lock, flags);
}

static void media5200_irq_mask(unsigned int virq)
{
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&media5200_irq.lock, flags);
	val = in_be32(media5200_irq.regs + MEDIA5200_IRQ_ENABLE);
	val &= ~(1 << (MEDIA5200_IRQ_SHIFT + irq_map[virq].hwirq));
	out_be32(media5200_irq.regs + MEDIA5200_IRQ_ENABLE, val);
	spin_unlock_irqrestore(&media5200_irq.lock, flags);
}

static struct irq_chip media5200_irq_chip = {
	.name = "Media5200 FPGA",
	.unmask = media5200_irq_unmask,
	.mask = media5200_irq_mask,
	.mask_ack = media5200_irq_mask,
};

void media5200_irq_cascade(unsigned int virq, struct irq_desc *desc)
{
	int sub_virq, val;
	u32 status, enable;

	/* Mask off the cascaded IRQ */
	raw_spin_lock(&desc->lock);
	desc->chip->mask(virq);
	raw_spin_unlock(&desc->lock);

	/* Ask the FPGA for IRQ status.  If 'val' is 0, then no irqs
	 * are pending.  'ffs()' is 1 based */
	status = in_be32(media5200_irq.regs + MEDIA5200_IRQ_ENABLE);
	enable = in_be32(media5200_irq.regs + MEDIA5200_IRQ_STATUS);
	val = ffs((status & enable) >> MEDIA5200_IRQ_SHIFT);
	if (val) {
		sub_virq = irq_linear_revmap(media5200_irq.irqhost, val - 1);
		/* pr_debug("%s: virq=%i s=%.8x e=%.8x hwirq=%i subvirq=%i\n",
		 *          __func__, virq, status, enable, val - 1, sub_virq);
		 */
		generic_handle_irq(sub_virq);
	}

	/* Processing done; can reenable the cascade now */
	raw_spin_lock(&desc->lock);
	desc->chip->ack(virq);
	if (!(desc->status & IRQ_DISABLED))
		desc->chip->unmask(virq);
	raw_spin_unlock(&desc->lock);
}

static int media5200_irq_map(struct irq_host *h, unsigned int virq,
			     irq_hw_number_t hw)
{
	struct irq_desc *desc = irq_to_desc(virq);

	pr_debug("%s: h=%p, virq=%i, hwirq=%i\n", __func__, h, virq, (int)hw);
	set_irq_chip_data(virq, &media5200_irq);
	set_irq_chip_and_handler(virq, &media5200_irq_chip, handle_level_irq);
	set_irq_type(virq, IRQ_TYPE_LEVEL_LOW);
	desc->status &= ~(IRQ_TYPE_SENSE_MASK | IRQ_LEVEL);
	desc->status |= IRQ_TYPE_LEVEL_LOW | IRQ_LEVEL;

	return 0;
}

static int media5200_irq_xlate(struct irq_host *h, struct device_node *ct,
				 const u32 *intspec, unsigned int intsize,
				 irq_hw_number_t *out_hwirq,
				 unsigned int *out_flags)
{
	if (intsize != 2)
		return -1;

	pr_debug("%s: bank=%i, number=%i\n", __func__, intspec[0], intspec[1]);
	*out_hwirq = intspec[1];
	*out_flags = IRQ_TYPE_NONE;
	return 0;
}

static struct irq_host_ops media5200_irq_ops = {
	.map = media5200_irq_map,
	.xlate = media5200_irq_xlate,
};

/*
 * Setup Media5200 IRQ mapping
 */
static void __init media5200_init_irq(void)
{
	struct device_node *fpga_np;
	int cascade_virq;

	/* First setup the regular MPC5200 interrupt controller */
	mpc52xx_init_irq();

	/* Now find the FPGA IRQ */
	fpga_np = of_find_compatible_node(NULL, NULL, "fsl,media5200-fpga");
	if (!fpga_np)
		goto out;
	pr_debug("%s: found fpga node: %s\n", __func__, fpga_np->full_name);

	media5200_irq.regs = of_iomap(fpga_np, 0);
	if (!media5200_irq.regs)
		goto out;
	pr_debug("%s: mapped to %p\n", __func__, media5200_irq.regs);

	cascade_virq = irq_of_parse_and_map(fpga_np, 0);
	if (!cascade_virq)
		goto out;
	pr_debug("%s: cascaded on virq=%i\n", __func__, cascade_virq);

	/* Disable all FPGA IRQs */
	out_be32(media5200_irq.regs + MEDIA5200_IRQ_ENABLE, 0);

	spin_lock_init(&media5200_irq.lock);

	media5200_irq.irqhost = irq_alloc_host(fpga_np, IRQ_HOST_MAP_LINEAR,
					       MEDIA5200_NUM_IRQS,
					       &media5200_irq_ops, -1);
	if (!media5200_irq.irqhost)
		goto out;
	pr_debug("%s: allocated irqhost\n", __func__);

	media5200_irq.irqhost->host_data = &media5200_irq;

	set_irq_data(cascade_virq, &media5200_irq);
	set_irq_chained_handler(cascade_virq, media5200_irq_cascade);

	return;

 out:
	pr_err("Could not find Media5200 FPGA; PCI interrupts will not work\n");
}

/*
 * Setup the architecture
 */
static void __init media5200_setup_arch(void)
{

	struct device_node *np;
	struct mpc52xx_gpio __iomem *gpio;
	u32 port_config;

	if (ppc_md.progress)
		ppc_md.progress("media5200_setup_arch()", 0);

	/* Map important registers from the internal memory map */
	mpc52xx_map_common_devices();

	/* Some mpc5200 & mpc5200b related configuration */
	mpc5200_setup_xlb_arbiter();

	mpc52xx_setup_pci();

	np = of_find_matching_node(NULL, mpc5200_gpio_ids);
	gpio = of_iomap(np, 0);
	of_node_put(np);
	if (!gpio) {
		printk(KERN_ERR "%s() failed. expect abnormal behavior\n",
		       __func__);
		return;
	}

	/* Set port config */
	port_config = in_be32(&gpio->port_config);

	port_config &= ~0x03000000;	/* ATA CS is on csb_4/5		*/
	port_config |=  0x01000000;

	out_be32(&gpio->port_config, port_config);

	/* Unmap zone */
	iounmap(gpio);

}

/* list of the supported boards */
static char *board[] __initdata = {
	"fsl,media5200",
	NULL
};

/*
 * Called very early, MMU is off, device-tree isn't unflattened
 */
static int __init media5200_probe(void)
{
	unsigned long node = of_get_flat_dt_root();
	int i = 0;

	while (board[i]) {
		if (of_flat_dt_is_compatible(node, board[i]))
			break;
		i++;
	}

	return (board[i] != NULL);
}

define_machine(media5200_platform) {
	.name		= "media5200-platform",
	.probe		= media5200_probe,
	.setup_arch	= media5200_setup_arch,
	.init		= mpc52xx_declare_of_platform_devices,
	.init_IRQ	= media5200_init_irq,
	.get_irq	= mpc52xx_get_irq,
	.restart	= mpc52xx_restart,
	.calibrate_decr	= generic_calibrate_decr,
};
