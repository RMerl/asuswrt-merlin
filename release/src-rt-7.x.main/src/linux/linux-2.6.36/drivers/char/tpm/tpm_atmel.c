/*
 * Copyright (C) 2004 IBM Corporation
 *
 * Authors:
 * Leendert van Doorn <leendert@watson.ibm.com>
 * Dave Safford <safford@watson.ibm.com>
 * Reiner Sailer <sailer@watson.ibm.com>
 * Kylene Hall <kjhall@us.ibm.com>
 *
 * Maintained by: <tpmdd-devel@lists.sourceforge.net>
 *
 * Device driver for TCG/TCPA TPM (trusted platform module).
 * Specifications at www.trustedcomputinggroup.org	 
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 * 
 */

#include "tpm.h"
#include "tpm_atmel.h"

/* write status bits */
enum tpm_atmel_write_status {
	ATML_STATUS_ABORT = 0x01,
	ATML_STATUS_LASTBYTE = 0x04
};
/* read status bits */
enum tpm_atmel_read_status {
	ATML_STATUS_BUSY = 0x01,
	ATML_STATUS_DATA_AVAIL = 0x02,
	ATML_STATUS_REWRITE = 0x04,
	ATML_STATUS_READY = 0x08
};

static int tpm_atml_recv(struct tpm_chip *chip, u8 *buf, size_t count)
{
	u8 status, *hdr = buf;
	u32 size;
	int i;
	__be32 *native_size;

	/* start reading header */
	if (count < 6)
		return -EIO;

	for (i = 0; i < 6; i++) {
		status = ioread8(chip->vendor.iobase + 1);
		if ((status & ATML_STATUS_DATA_AVAIL) == 0) {
			dev_err(chip->dev, "error reading header\n");
			return -EIO;
		}
		*buf++ = ioread8(chip->vendor.iobase);
	}

	/* size of the data received */
	native_size = (__force __be32 *) (hdr + 2);
	size = be32_to_cpu(*native_size);

	if (count < size) {
		dev_err(chip->dev,
			"Recv size(%d) less than available space\n", size);
		for (; i < size; i++) {	/* clear the waiting data anyway */
			status = ioread8(chip->vendor.iobase + 1);
			if ((status & ATML_STATUS_DATA_AVAIL) == 0) {
				dev_err(chip->dev, "error reading data\n");
				return -EIO;
			}
		}
		return -EIO;
	}

	/* read all the data available */
	for (; i < size; i++) {
		status = ioread8(chip->vendor.iobase + 1);
		if ((status & ATML_STATUS_DATA_AVAIL) == 0) {
			dev_err(chip->dev, "error reading data\n");
			return -EIO;
		}
		*buf++ = ioread8(chip->vendor.iobase);
	}

	/* make sure data available is gone */
	status = ioread8(chip->vendor.iobase + 1);

	if (status & ATML_STATUS_DATA_AVAIL) {
		dev_err(chip->dev, "data available is stuck\n");
		return -EIO;
	}

	return size;
}

static int tpm_atml_send(struct tpm_chip *chip, u8 *buf, size_t count)
{
	int i;

	dev_dbg(chip->dev, "tpm_atml_send:\n");
	for (i = 0; i < count; i++) {
		dev_dbg(chip->dev, "%d 0x%x(%d)\n",  i, buf[i], buf[i]);
 		iowrite8(buf[i], chip->vendor.iobase);
	}

	return count;
}

static void tpm_atml_cancel(struct tpm_chip *chip)
{
	iowrite8(ATML_STATUS_ABORT, chip->vendor.iobase + 1);
}

static u8 tpm_atml_status(struct tpm_chip *chip)
{
	return ioread8(chip->vendor.iobase + 1);
}

static const struct file_operations atmel_ops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.open = tpm_open,
	.read = tpm_read,
	.write = tpm_write,
	.release = tpm_release,
};

static DEVICE_ATTR(pubek, S_IRUGO, tpm_show_pubek, NULL);
static DEVICE_ATTR(pcrs, S_IRUGO, tpm_show_pcrs, NULL);
static DEVICE_ATTR(caps, S_IRUGO, tpm_show_caps, NULL);
static DEVICE_ATTR(cancel, S_IWUSR |S_IWGRP, NULL, tpm_store_cancel);

static struct attribute* atmel_attrs[] = {
	&dev_attr_pubek.attr,
	&dev_attr_pcrs.attr,
	&dev_attr_caps.attr,
	&dev_attr_cancel.attr,
	NULL,
};

static struct attribute_group atmel_attr_grp = { .attrs = atmel_attrs };

static const struct tpm_vendor_specific tpm_atmel = {
	.recv = tpm_atml_recv,
	.send = tpm_atml_send,
	.cancel = tpm_atml_cancel,
	.status = tpm_atml_status,
	.req_complete_mask = ATML_STATUS_BUSY | ATML_STATUS_DATA_AVAIL,
	.req_complete_val = ATML_STATUS_DATA_AVAIL,
	.req_canceled = ATML_STATUS_READY,
	.attr_group = &atmel_attr_grp,
	.miscdev = { .fops = &atmel_ops, },
};

static struct platform_device *pdev;

static void atml_plat_remove(void)
{
	struct tpm_chip *chip = dev_get_drvdata(&pdev->dev);

	if (chip) {
		if (chip->vendor.have_region)
			atmel_release_region(chip->vendor.base,
					     chip->vendor.region_size);
		atmel_put_base_addr(chip->vendor.iobase);
		tpm_remove_hardware(chip->dev);
		platform_device_unregister(pdev);
	}
}

static int tpm_atml_suspend(struct platform_device *dev, pm_message_t msg)
{
	return tpm_pm_suspend(&dev->dev, msg);
}

static int tpm_atml_resume(struct platform_device *dev)
{
	return tpm_pm_resume(&dev->dev);
}
static struct platform_driver atml_drv = {
	.driver = {
		.name = "tpm_atmel",
		.owner		= THIS_MODULE,
	},
	.suspend = tpm_atml_suspend,
	.resume = tpm_atml_resume,
};

static int __init init_atmel(void)
{
	int rc = 0;
	void __iomem *iobase = NULL;
	int have_region, region_size;
	unsigned long base;
	struct  tpm_chip *chip;

	rc = platform_driver_register(&atml_drv);
	if (rc)
		return rc;

	if ((iobase = atmel_get_base_addr(&base, &region_size)) == NULL) {
		rc = -ENODEV;
		goto err_unreg_drv;
	}

	have_region =
	    (atmel_request_region
	     (tpm_atmel.base, region_size, "tpm_atmel0") == NULL) ? 0 : 1;

	pdev = platform_device_register_simple("tpm_atmel", -1, NULL, 0);
	if (IS_ERR(pdev)) {
		rc = PTR_ERR(pdev);
		goto err_rel_reg;
	}

	if (!(chip = tpm_register_hardware(&pdev->dev, &tpm_atmel))) {
		rc = -ENODEV;
		goto err_unreg_dev;
	}

	chip->vendor.iobase = iobase;
	chip->vendor.base = base;
	chip->vendor.have_region = have_region;
	chip->vendor.region_size = region_size;

	return 0;

err_unreg_dev:
	platform_device_unregister(pdev);
err_rel_reg:
	atmel_put_base_addr(iobase);
	if (have_region)
		atmel_release_region(base,
				     region_size);
err_unreg_drv:
	platform_driver_unregister(&atml_drv);
	return rc;
}

static void __exit cleanup_atmel(void)
{
	platform_driver_unregister(&atml_drv);
	atml_plat_remove();
}

module_init(init_atmel);
module_exit(cleanup_atmel);

MODULE_AUTHOR("Leendert van Doorn (leendert@watson.ibm.com)");
MODULE_DESCRIPTION("TPM Driver");
MODULE_VERSION("2.0");
MODULE_LICENSE("GPL");
