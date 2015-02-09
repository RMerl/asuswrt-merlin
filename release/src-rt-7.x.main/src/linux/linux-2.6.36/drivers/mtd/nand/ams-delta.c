/*
 *  drivers/mtd/nand/ams-delta.c
 *
 *  Copyright (C) 2006 Jonathan McDowell <noodles@earth.li>
 *
 *  Derived from drivers/mtd/toto.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 *   This is a device driver for the NAND flash device found on the
 *   Amstrad E3 (Delta).
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <asm/io.h>
#include <mach/hardware.h>
#include <asm/sizes.h>
#include <mach/gpio.h>
#include <plat/board-ams-delta.h>

/*
 * MTD structure for E3 (Delta)
 */
static struct mtd_info *ams_delta_mtd = NULL;

#define NAND_MASK (AMS_DELTA_LATCH2_NAND_NRE | AMS_DELTA_LATCH2_NAND_NWE | AMS_DELTA_LATCH2_NAND_CLE | AMS_DELTA_LATCH2_NAND_ALE | AMS_DELTA_LATCH2_NAND_NCE | AMS_DELTA_LATCH2_NAND_NWP)

/*
 * Define partitions for flash devices
 */

static struct mtd_partition partition_info[] = {
	{ .name		= "Kernel",
	  .offset	= 0,
	  .size		= 3 * SZ_1M + SZ_512K },
	{ .name		= "u-boot",
	  .offset	= 3 * SZ_1M + SZ_512K,
	  .size		= SZ_256K },
	{ .name		= "u-boot params",
	  .offset	= 3 * SZ_1M + SZ_512K + SZ_256K,
	  .size		= SZ_256K },
	{ .name		= "Amstrad LDR",
	  .offset	= 4 * SZ_1M,
	  .size		= SZ_256K },
	{ .name		= "File system",
	  .offset	= 4 * SZ_1M + 1 * SZ_256K,
	  .size		= 27 * SZ_1M },
	{ .name		= "PBL reserved",
	  .offset	= 32 * SZ_1M - 3 * SZ_256K,
	  .size		=  3 * SZ_256K },
};

static void ams_delta_write_byte(struct mtd_info *mtd, u_char byte)
{
	struct nand_chip *this = mtd->priv;

	omap_writew(0, (OMAP1_MPUIO_BASE + OMAP_MPUIO_IO_CNTL));
	omap_writew(byte, this->IO_ADDR_W);
	ams_delta_latch2_write(AMS_DELTA_LATCH2_NAND_NWE, 0);
	ndelay(40);
	ams_delta_latch2_write(AMS_DELTA_LATCH2_NAND_NWE,
			       AMS_DELTA_LATCH2_NAND_NWE);
}

static u_char ams_delta_read_byte(struct mtd_info *mtd)
{
	u_char res;
	struct nand_chip *this = mtd->priv;

	ams_delta_latch2_write(AMS_DELTA_LATCH2_NAND_NRE, 0);
	ndelay(40);
	omap_writew(~0, (OMAP1_MPUIO_BASE + OMAP_MPUIO_IO_CNTL));
	res = omap_readw(this->IO_ADDR_R);
	ams_delta_latch2_write(AMS_DELTA_LATCH2_NAND_NRE,
			       AMS_DELTA_LATCH2_NAND_NRE);

	return res;
}

static void ams_delta_write_buf(struct mtd_info *mtd, const u_char *buf,
				int len)
{
	int i;

	for (i=0; i<len; i++)
		ams_delta_write_byte(mtd, buf[i]);
}

static void ams_delta_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	int i;

	for (i=0; i<len; i++)
		buf[i] = ams_delta_read_byte(mtd);
}

static int ams_delta_verify_buf(struct mtd_info *mtd, const u_char *buf,
				int len)
{
	int i;

	for (i=0; i<len; i++)
		if (buf[i] != ams_delta_read_byte(mtd))
			return -EFAULT;

	return 0;
}

/*
 * Command control function
 *
 * ctrl:
 * NAND_NCE: bit 0 -> bit 2
 * NAND_CLE: bit 1 -> bit 7
 * NAND_ALE: bit 2 -> bit 6
 */
static void ams_delta_hwcontrol(struct mtd_info *mtd, int cmd,
				unsigned int ctrl)
{

	if (ctrl & NAND_CTRL_CHANGE) {
		unsigned long bits;

		bits = (~ctrl & NAND_NCE) ? AMS_DELTA_LATCH2_NAND_NCE : 0;
		bits |= (ctrl & NAND_CLE) ? AMS_DELTA_LATCH2_NAND_CLE : 0;
		bits |= (ctrl & NAND_ALE) ? AMS_DELTA_LATCH2_NAND_ALE : 0;

		ams_delta_latch2_write(AMS_DELTA_LATCH2_NAND_CLE |
				AMS_DELTA_LATCH2_NAND_ALE |
				AMS_DELTA_LATCH2_NAND_NCE, bits);
	}

	if (cmd != NAND_CMD_NONE)
		ams_delta_write_byte(mtd, cmd);
}

static int ams_delta_nand_ready(struct mtd_info *mtd)
{
	return gpio_get_value(AMS_DELTA_GPIO_PIN_NAND_RB);
}

/*
 * Main initialization routine
 */
static int __init ams_delta_init(void)
{
	struct nand_chip *this;
	int err = 0;

	/* Allocate memory for MTD device structure and private data */
	ams_delta_mtd = kmalloc(sizeof(struct mtd_info) +
				sizeof(struct nand_chip), GFP_KERNEL);
	if (!ams_delta_mtd) {
		printk (KERN_WARNING "Unable to allocate E3 NAND MTD device structure.\n");
		err = -ENOMEM;
		goto out;
	}

	ams_delta_mtd->owner = THIS_MODULE;

	/* Get pointer to private data */
	this = (struct nand_chip *) (&ams_delta_mtd[1]);

	/* Initialize structures */
	memset(ams_delta_mtd, 0, sizeof(struct mtd_info));
	memset(this, 0, sizeof(struct nand_chip));

	/* Link the private data with the MTD structure */
	ams_delta_mtd->priv = this;

	/* Set address of NAND IO lines */
	this->IO_ADDR_R = (OMAP1_MPUIO_BASE + OMAP_MPUIO_INPUT_LATCH);
	this->IO_ADDR_W = (OMAP1_MPUIO_BASE + OMAP_MPUIO_OUTPUT);
	this->read_byte = ams_delta_read_byte;
	this->write_buf = ams_delta_write_buf;
	this->read_buf = ams_delta_read_buf;
	this->verify_buf = ams_delta_verify_buf;
	this->cmd_ctrl = ams_delta_hwcontrol;
	if (gpio_request(AMS_DELTA_GPIO_PIN_NAND_RB, "nand_rdy") == 0) {
		this->dev_ready = ams_delta_nand_ready;
	} else {
		this->dev_ready = NULL;
		printk(KERN_NOTICE "Couldn't request gpio for Delta NAND ready.\n");
	}
	/* 25 us command delay time */
	this->chip_delay = 30;
	this->ecc.mode = NAND_ECC_SOFT;

	/* Set chip enabled, but  */
	ams_delta_latch2_write(NAND_MASK, AMS_DELTA_LATCH2_NAND_NRE |
					  AMS_DELTA_LATCH2_NAND_NWE |
					  AMS_DELTA_LATCH2_NAND_NCE |
					  AMS_DELTA_LATCH2_NAND_NWP);

	/* Scan to find existance of the device */
	if (nand_scan(ams_delta_mtd, 1)) {
		err = -ENXIO;
		goto out_mtd;
	}

	/* Register the partitions */
	add_mtd_partitions(ams_delta_mtd, partition_info,
			   ARRAY_SIZE(partition_info));

	goto out;

 out_mtd:
	kfree(ams_delta_mtd);
 out:
	return err;
}

module_init(ams_delta_init);

/*
 * Clean up routine
 */
static void __exit ams_delta_cleanup(void)
{
	/* Release resources, unregister device */
	nand_release(ams_delta_mtd);

	/* Free the MTD device structure */
	kfree(ams_delta_mtd);
}
module_exit(ams_delta_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jonathan McDowell <noodles@earth.li>");
MODULE_DESCRIPTION("Glue layer for NAND flash on Amstrad E3 (Delta)");
