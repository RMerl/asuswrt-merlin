/*
 * Copyright (C) 2007-2010 Freescale Semiconductor, Inc.
 *
 * Author: Tony Li <tony.li@freescale.com>
 *	   Jason Jin <Jason.jin@freescale.com>
 *
 * The hwirq alloc and free code reuse from sysdev/mpic_msi.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of the
 * License.
 *
 */
#include <linux/irq.h>
#include <linux/bootmem.h>
#include <linux/msi.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/of_platform.h>
#include <sysdev/fsl_soc.h>
#include <asm/prom.h>
#include <asm/hw_irq.h>
#include <asm/ppc-pci.h>
#include <asm/mpic.h>
#include "fsl_msi.h"

LIST_HEAD(msi_head);

struct fsl_msi_feature {
	u32 fsl_pic_ip;
	u32 msiir_offset;
};

struct fsl_msi_cascade_data {
	struct fsl_msi *msi_data;
	int index;
};

static inline u32 fsl_msi_read(u32 __iomem *base, unsigned int reg)
{
	return in_be32(base + (reg >> 2));
}

/*
 * We do not need this actually. The MSIR register has been read once
 * in the cascade interrupt. So, this MSI interrupt has been acked
*/
static void fsl_msi_end_irq(unsigned int virq)
{
}

static struct irq_chip fsl_msi_chip = {
	.mask		= mask_msi_irq,
	.unmask		= unmask_msi_irq,
	.ack		= fsl_msi_end_irq,
	.name		= "FSL-MSI",
};

static int fsl_msi_host_map(struct irq_host *h, unsigned int virq,
				irq_hw_number_t hw)
{
	struct fsl_msi *msi_data = h->host_data;
	struct irq_chip *chip = &fsl_msi_chip;

	irq_to_desc(virq)->status |= IRQ_TYPE_EDGE_FALLING;

	set_irq_chip_data(virq, msi_data);
	set_irq_chip_and_handler(virq, chip, handle_edge_irq);

	return 0;
}

static struct irq_host_ops fsl_msi_host_ops = {
	.map = fsl_msi_host_map,
};

static int fsl_msi_init_allocator(struct fsl_msi *msi_data)
{
	int rc;

	rc = msi_bitmap_alloc(&msi_data->bitmap, NR_MSI_IRQS,
			      msi_data->irqhost->of_node);
	if (rc)
		return rc;

	rc = msi_bitmap_reserve_dt_hwirqs(&msi_data->bitmap);
	if (rc < 0) {
		msi_bitmap_free(&msi_data->bitmap);
		return rc;
	}

	return 0;
}

static int fsl_msi_check_device(struct pci_dev *pdev, int nvec, int type)
{
	if (type == PCI_CAP_ID_MSIX)
		pr_debug("fslmsi: MSI-X untested, trying anyway.\n");

	return 0;
}

static void fsl_teardown_msi_irqs(struct pci_dev *pdev)
{
	struct msi_desc *entry;
	struct fsl_msi *msi_data;

	list_for_each_entry(entry, &pdev->msi_list, list) {
		if (entry->irq == NO_IRQ)
			continue;
		msi_data = get_irq_data(entry->irq);
		set_irq_msi(entry->irq, NULL);
		msi_bitmap_free_hwirqs(&msi_data->bitmap,
				       virq_to_hw(entry->irq), 1);
		irq_dispose_mapping(entry->irq);
	}

	return;
}

static void fsl_compose_msi_msg(struct pci_dev *pdev, int hwirq,
				struct msi_msg *msg,
				struct fsl_msi *fsl_msi_data)
{
	struct fsl_msi *msi_data = fsl_msi_data;
	struct pci_controller *hose = pci_bus_to_host(pdev->bus);
	u32 base = 0;

	pci_bus_read_config_dword(hose->bus,
		PCI_DEVFN(0, 0), PCI_BASE_ADDRESS_0, &base);

	msg->address_lo = msi_data->msi_addr_lo + base;
	msg->address_hi = msi_data->msi_addr_hi;
	msg->data = hwirq;

	pr_debug("%s: allocated srs: %d, ibs: %d\n",
		__func__, hwirq / IRQS_PER_MSI_REG, hwirq % IRQS_PER_MSI_REG);
}

static int fsl_setup_msi_irqs(struct pci_dev *pdev, int nvec, int type)
{
	int rc, hwirq = -ENOMEM;
	unsigned int virq;
	struct msi_desc *entry;
	struct msi_msg msg;
	struct fsl_msi *msi_data;

	list_for_each_entry(entry, &pdev->msi_list, list) {
		list_for_each_entry(msi_data, &msi_head, list) {
			hwirq = msi_bitmap_alloc_hwirqs(&msi_data->bitmap, 1);
			if (hwirq >= 0)
				break;
		}

		if (hwirq < 0) {
			rc = hwirq;
			pr_debug("%s: fail allocating msi interrupt\n",
					__func__);
			goto out_free;
		}

		virq = irq_create_mapping(msi_data->irqhost, hwirq);

		if (virq == NO_IRQ) {
			pr_debug("%s: fail mapping hwirq 0x%x\n",
					__func__, hwirq);
			msi_bitmap_free_hwirqs(&msi_data->bitmap, hwirq, 1);
			rc = -ENOSPC;
			goto out_free;
		}
		set_irq_data(virq, msi_data);
		set_irq_msi(virq, entry);

		fsl_compose_msi_msg(pdev, hwirq, &msg, msi_data);
		write_msi_msg(virq, &msg);
	}
	return 0;

out_free:
	/* free by the caller of this function */
	return rc;
}

static void fsl_msi_cascade(unsigned int irq, struct irq_desc *desc)
{
	unsigned int cascade_irq;
	struct fsl_msi *msi_data;
	int msir_index = -1;
	u32 msir_value = 0;
	u32 intr_index;
	u32 have_shift = 0;
	struct fsl_msi_cascade_data *cascade_data;

	cascade_data = (struct fsl_msi_cascade_data *)get_irq_data(irq);
	msi_data = cascade_data->msi_data;

	raw_spin_lock(&desc->lock);
	if ((msi_data->feature &  FSL_PIC_IP_MASK) == FSL_PIC_IP_IPIC) {
		if (desc->chip->mask_ack)
			desc->chip->mask_ack(irq);
		else {
			desc->chip->mask(irq);
			desc->chip->ack(irq);
		}
	}

	if (unlikely(desc->status & IRQ_INPROGRESS))
		goto unlock;

	msir_index = cascade_data->index;

	if (msir_index >= NR_MSI_REG)
		cascade_irq = NO_IRQ;

	desc->status |= IRQ_INPROGRESS;
	switch (msi_data->feature & FSL_PIC_IP_MASK) {
	case FSL_PIC_IP_MPIC:
		msir_value = fsl_msi_read(msi_data->msi_regs,
			msir_index * 0x10);
		break;
	case FSL_PIC_IP_IPIC:
		msir_value = fsl_msi_read(msi_data->msi_regs, msir_index * 0x4);
		break;
	}

	while (msir_value) {
		intr_index = ffs(msir_value) - 1;

		cascade_irq = irq_linear_revmap(msi_data->irqhost,
				msir_index * IRQS_PER_MSI_REG +
					intr_index + have_shift);
		if (cascade_irq != NO_IRQ)
			generic_handle_irq(cascade_irq);
		have_shift += intr_index + 1;
		msir_value = msir_value >> (intr_index + 1);
	}
	desc->status &= ~IRQ_INPROGRESS;

	switch (msi_data->feature & FSL_PIC_IP_MASK) {
	case FSL_PIC_IP_MPIC:
		desc->chip->eoi(irq);
		break;
	case FSL_PIC_IP_IPIC:
		if (!(desc->status & IRQ_DISABLED) && desc->chip->unmask)
			desc->chip->unmask(irq);
		break;
	}
unlock:
	raw_spin_unlock(&desc->lock);
}

static int fsl_of_msi_remove(struct platform_device *ofdev)
{
	struct fsl_msi *msi = ofdev->dev.platform_data;
	int virq, i;
	struct fsl_msi_cascade_data *cascade_data;

	if (msi->list.prev != NULL)
		list_del(&msi->list);
	for (i = 0; i < NR_MSI_REG; i++) {
		virq = msi->msi_virqs[i];
		if (virq != NO_IRQ) {
			cascade_data = get_irq_data(virq);
			kfree(cascade_data);
			irq_dispose_mapping(virq);
		}
	}
	if (msi->bitmap.bitmap)
		msi_bitmap_free(&msi->bitmap);
	iounmap(msi->msi_regs);
	kfree(msi);

	return 0;
}

static int __devinit fsl_of_msi_probe(struct platform_device *dev,
				const struct of_device_id *match)
{
	struct fsl_msi *msi;
	struct resource res;
	int err, i, count;
	int rc;
	int virt_msir;
	const u32 *p;
	struct fsl_msi_feature *features = match->data;
	struct fsl_msi_cascade_data *cascade_data = NULL;
	int len;
	u32 offset;

	printk(KERN_DEBUG "Setting up Freescale MSI support\n");

	msi = kzalloc(sizeof(struct fsl_msi), GFP_KERNEL);
	if (!msi) {
		dev_err(&dev->dev, "No memory for MSI structure\n");
		return -ENOMEM;
	}
	dev->dev.platform_data = msi;

	msi->irqhost = irq_alloc_host(dev->dev.of_node, IRQ_HOST_MAP_LINEAR,
				      NR_MSI_IRQS, &fsl_msi_host_ops, 0);

	if (msi->irqhost == NULL) {
		dev_err(&dev->dev, "No memory for MSI irqhost\n");
		err = -ENOMEM;
		goto error_out;
	}

	/* Get the MSI reg base */
	err = of_address_to_resource(dev->dev.of_node, 0, &res);
	if (err) {
		dev_err(&dev->dev, "%s resource error!\n",
				dev->dev.of_node->full_name);
		goto error_out;
	}

	msi->msi_regs = ioremap(res.start, res.end - res.start + 1);
	if (!msi->msi_regs) {
		dev_err(&dev->dev, "ioremap problem failed\n");
		goto error_out;
	}

	msi->feature = features->fsl_pic_ip;

	msi->irqhost->host_data = msi;

	msi->msi_addr_hi = 0x0;
	msi->msi_addr_lo = features->msiir_offset + (res.start & 0xfffff);

	rc = fsl_msi_init_allocator(msi);
	if (rc) {
		dev_err(&dev->dev, "Error allocating MSI bitmap\n");
		goto error_out;
	}

	p = of_get_property(dev->dev.of_node, "interrupts", &count);
	if (!p) {
		dev_err(&dev->dev, "no interrupts property found on %s\n",
				dev->dev.of_node->full_name);
		err = -ENODEV;
		goto error_out;
	}
	if (count % 8 != 0) {
		dev_err(&dev->dev, "Malformed interrupts property on %s\n",
				dev->dev.of_node->full_name);
		err = -EINVAL;
		goto error_out;
	}
	offset = 0;
	p = of_get_property(dev->dev.of_node, "msi-available-ranges", &len);
	if (p)
		offset = *p / IRQS_PER_MSI_REG;

	count /= sizeof(u32);
	for (i = 0; i < min(count / 2, NR_MSI_REG); i++) {
		virt_msir = irq_of_parse_and_map(dev->dev.of_node, i);
		if (virt_msir != NO_IRQ) {
			cascade_data = kzalloc(
					sizeof(struct fsl_msi_cascade_data),
					GFP_KERNEL);
			if (!cascade_data) {
				dev_err(&dev->dev,
					"No memory for MSI cascade data\n");
				err = -ENOMEM;
				goto error_out;
			}
			msi->msi_virqs[i] = virt_msir;
			cascade_data->index = i + offset;
			cascade_data->msi_data = msi;
			set_irq_data(virt_msir, (void *)cascade_data);
			set_irq_chained_handler(virt_msir, fsl_msi_cascade);
		}
	}

	list_add_tail(&msi->list, &msi_head);

	/* The multiple setting ppc_md.setup_msi_irqs will not harm things */
	if (!ppc_md.setup_msi_irqs) {
		ppc_md.setup_msi_irqs = fsl_setup_msi_irqs;
		ppc_md.teardown_msi_irqs = fsl_teardown_msi_irqs;
		ppc_md.msi_check_device = fsl_msi_check_device;
	} else if (ppc_md.setup_msi_irqs != fsl_setup_msi_irqs) {
		dev_err(&dev->dev, "Different MSI driver already installed!\n");
		err = -ENODEV;
		goto error_out;
	}
	return 0;
error_out:
	fsl_of_msi_remove(dev);
	return err;
}

static const struct fsl_msi_feature mpic_msi_feature = {
	.fsl_pic_ip = FSL_PIC_IP_MPIC,
	.msiir_offset = 0x140,
};

static const struct fsl_msi_feature ipic_msi_feature = {
	.fsl_pic_ip = FSL_PIC_IP_IPIC,
	.msiir_offset = 0x38,
};

static const struct of_device_id fsl_of_msi_ids[] = {
	{
		.compatible = "fsl,mpic-msi",
		.data = (void *)&mpic_msi_feature,
	},
	{
		.compatible = "fsl,ipic-msi",
		.data = (void *)&ipic_msi_feature,
	},
	{}
};

static struct of_platform_driver fsl_of_msi_driver = {
	.driver = {
		.name = "fsl-msi",
		.owner = THIS_MODULE,
		.of_match_table = fsl_of_msi_ids,
	},
	.probe = fsl_of_msi_probe,
	.remove = fsl_of_msi_remove,
};

static __init int fsl_of_msi_init(void)
{
	return of_register_platform_driver(&fsl_of_msi_driver);
}

subsys_initcall(fsl_of_msi_init);
