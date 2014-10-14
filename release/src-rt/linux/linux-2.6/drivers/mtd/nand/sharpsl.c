/*
 * drivers/mtd/nand/sharpsl.c
 *
 *  Copyright (C) 2004 Richard Purdie
 *  Copyright (C) 2008 Dmitry Baryshkov
 *
 *  Based on Sharp's NAND driver sharp_sl.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/genhd.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/sharpsl.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <asm/io.h>
#include <mach/hardware.h>
#include <asm/mach-types.h>

struct sharpsl_nand {
	struct mtd_info		mtd;
	struct nand_chip	chip;

	void __iomem		*io;
};

#define mtd_to_sharpsl(_mtd)	container_of(_mtd, struct sharpsl_nand, mtd)

/* register offset */
#define ECCLPLB		0x00	/* line parity 7 - 0 bit */
#define ECCLPUB		0x04	/* line parity 15 - 8 bit */
#define ECCCP		0x08	/* column parity 5 - 0 bit */
#define ECCCNTR		0x0C	/* ECC byte counter */
#define ECCCLRR		0x10	/* cleare ECC */
#define FLASHIO		0x14	/* Flash I/O */
#define FLASHCTL	0x18	/* Flash Control */

/* Flash control bit */
#define FLRYBY		(1 << 5)
#define FLCE1		(1 << 4)
#define FLWP		(1 << 3)
#define FLALE		(1 << 2)
#define FLCLE		(1 << 1)
#define FLCE0		(1 << 0)

/*
 *	hardware specific access to control-lines
 *	ctrl:
 *	NAND_CNE: bit 0 -> ! bit 0 & 4
 *	NAND_CLE: bit 1 -> bit 1
 *	NAND_ALE: bit 2 -> bit 2
 *
 */
static void sharpsl_nand_hwcontrol(struct mtd_info *mtd, int cmd,
				   unsigned int ctrl)
{
	struct sharpsl_nand *sharpsl = mtd_to_sharpsl(mtd);
	struct nand_chip *chip = mtd->priv;

	if (ctrl & NAND_CTRL_CHANGE) {
		unsigned char bits = ctrl & 0x07;

		bits |= (ctrl & 0x01) << 4;

		bits ^= 0x11;

		writeb((readb(sharpsl->io + FLASHCTL) & ~0x17) | bits, sharpsl->io + FLASHCTL);
	}

	if (cmd != NAND_CMD_NONE)
		writeb(cmd, chip->IO_ADDR_W);
}

static int sharpsl_nand_dev_ready(struct mtd_info *mtd)
{
	struct sharpsl_nand *sharpsl = mtd_to_sharpsl(mtd);
	return !((readb(sharpsl->io + FLASHCTL) & FLRYBY) == 0);
}

static void sharpsl_nand_enable_hwecc(struct mtd_info *mtd, int mode)
{
	struct sharpsl_nand *sharpsl = mtd_to_sharpsl(mtd);
	writeb(0, sharpsl->io + ECCCLRR);
}

static int sharpsl_nand_calculate_ecc(struct mtd_info *mtd, const u_char * dat, u_char * ecc_code)
{
	struct sharpsl_nand *sharpsl = mtd_to_sharpsl(mtd);
	ecc_code[0] = ~readb(sharpsl->io + ECCLPUB);
	ecc_code[1] = ~readb(sharpsl->io + ECCLPLB);
	ecc_code[2] = (~readb(sharpsl->io + ECCCP) << 2) | 0x03;
	return readb(sharpsl->io + ECCCNTR) != 0;
}

#ifdef CONFIG_MTD_PARTITIONS
static const char *part_probes[] = { "cmdlinepart", NULL };
#endif

/*
 * Main initialization routine
 */
static int __devinit sharpsl_nand_probe(struct platform_device *pdev)
{
	struct nand_chip *this;
#ifdef CONFIG_MTD_PARTITIONS
	struct mtd_partition *sharpsl_partition_info;
	int nr_partitions;
#endif
	struct resource *r;
	int err = 0;
	struct sharpsl_nand *sharpsl;
	struct sharpsl_nand_platform_data *data = pdev->dev.platform_data;

	if (!data) {
		dev_err(&pdev->dev, "no platform data!\n");
		return -EINVAL;
	}

	/* Allocate memory for MTD device structure and private data */
	sharpsl = kzalloc(sizeof(struct sharpsl_nand), GFP_KERNEL);
	if (!sharpsl) {
		printk("Unable to allocate SharpSL NAND MTD device structure.\n");
		return -ENOMEM;
	}

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		dev_err(&pdev->dev, "no io memory resource defined!\n");
		err = -ENODEV;
		goto err_get_res;
	}

	/* map physical address */
	sharpsl->io = ioremap(r->start, resource_size(r));
	if (!sharpsl->io) {
		printk("ioremap to access Sharp SL NAND chip failed\n");
		err = -EIO;
		goto err_ioremap;
	}

	/* Get pointer to private data */
	this = (struct nand_chip *)(&sharpsl->chip);

	/* Link the private data with the MTD structure */
	sharpsl->mtd.priv = this;
	sharpsl->mtd.owner = THIS_MODULE;

	platform_set_drvdata(pdev, sharpsl);

	/*
	 * PXA initialize
	 */
	writeb(readb(sharpsl->io + FLASHCTL) | FLWP, sharpsl->io + FLASHCTL);

	/* Set address of NAND IO lines */
	this->IO_ADDR_R = sharpsl->io + FLASHIO;
	this->IO_ADDR_W = sharpsl->io + FLASHIO;
	/* Set address of hardware control function */
	this->cmd_ctrl = sharpsl_nand_hwcontrol;
	this->dev_ready = sharpsl_nand_dev_ready;
	/* 15 us command delay time */
	this->chip_delay = 15;
	/* set eccmode using hardware ECC */
	this->ecc.mode = NAND_ECC_HW;
	this->ecc.size = 256;
	this->ecc.bytes = 3;
	this->badblock_pattern = data->badblock_pattern;
	this->ecc.layout = data->ecc_layout;
	this->ecc.hwctl = sharpsl_nand_enable_hwecc;
	this->ecc.calculate = sharpsl_nand_calculate_ecc;
	this->ecc.correct = nand_correct_data;

	/* Scan to find existence of the device */
	err = nand_scan(&sharpsl->mtd, 1);
	if (err)
		goto err_scan;

	/* Register the partitions */
	sharpsl->mtd.name = "sharpsl-nand";
#ifdef CONFIG_MTD_PARTITIONS
	nr_partitions = parse_mtd_partitions(&sharpsl->mtd, part_probes, &sharpsl_partition_info, 0);
	if (nr_partitions <= 0) {
		nr_partitions = data->nr_partitions;
		sharpsl_partition_info = data->partitions;
	}

	if (nr_partitions > 0)
		err = add_mtd_partitions(&sharpsl->mtd, sharpsl_partition_info, nr_partitions);
	else
#endif
	err = add_mtd_device(&sharpsl->mtd);
	if (err)
		goto err_add;

	/* Return happy */
	return 0;

err_add:
	nand_release(&sharpsl->mtd);

err_scan:
	platform_set_drvdata(pdev, NULL);
	iounmap(sharpsl->io);
err_ioremap:
err_get_res:
	kfree(sharpsl);
	return err;
}

/*
 * Clean up routine
 */
static int __devexit sharpsl_nand_remove(struct platform_device *pdev)
{
	struct sharpsl_nand *sharpsl = platform_get_drvdata(pdev);

	/* Release resources, unregister device */
	nand_release(&sharpsl->mtd);

	platform_set_drvdata(pdev, NULL);

	iounmap(sharpsl->io);

	/* Free the MTD device structure */
	kfree(sharpsl);

	return 0;
}

static struct platform_driver sharpsl_nand_driver = {
	.driver = {
		.name	= "sharpsl-nand",
		.owner	= THIS_MODULE,
	},
	.probe		= sharpsl_nand_probe,
	.remove		= __devexit_p(sharpsl_nand_remove),
};

static int __init sharpsl_nand_init(void)
{
	return platform_driver_register(&sharpsl_nand_driver);
}
module_init(sharpsl_nand_init);

static void __exit sharpsl_nand_exit(void)
{
	platform_driver_unregister(&sharpsl_nand_driver);
}
module_exit(sharpsl_nand_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Richard Purdie <rpurdie@rpsys.net>");
MODULE_DESCRIPTION("Device specific logic for NAND flash on Sharp SL-C7xx Series");
