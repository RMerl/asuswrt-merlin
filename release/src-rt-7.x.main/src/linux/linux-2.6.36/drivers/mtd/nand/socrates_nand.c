/*
 * drivers/mtd/nand/socrates_nand.c
 *
 *  Copyright © 2008 Ilya Yanok, Emcraft Systems
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/of_platform.h>
#include <linux/io.h>

#define FPGA_NAND_CMD_MASK		(0x7 << 28)
#define FPGA_NAND_CMD_COMMAND		(0x0 << 28)
#define FPGA_NAND_CMD_ADDR		(0x1 << 28)
#define FPGA_NAND_CMD_READ		(0x2 << 28)
#define FPGA_NAND_CMD_WRITE		(0x3 << 28)
#define FPGA_NAND_BUSY			(0x1 << 15)
#define FPGA_NAND_ENABLE		(0x1 << 31)
#define FPGA_NAND_DATA_SHIFT		16

struct socrates_nand_host {
	struct nand_chip	nand_chip;
	struct mtd_info		mtd;
	void __iomem		*io_base;
	struct device		*dev;
};

/**
 * socrates_nand_write_buf -  write buffer to chip
 * @mtd:	MTD device structure
 * @buf:	data buffer
 * @len:	number of bytes to write
 */
static void socrates_nand_write_buf(struct mtd_info *mtd,
		const uint8_t *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;
	struct socrates_nand_host *host = this->priv;

	for (i = 0; i < len; i++) {
		out_be32(host->io_base, FPGA_NAND_ENABLE |
				FPGA_NAND_CMD_WRITE |
				(buf[i] << FPGA_NAND_DATA_SHIFT));
	}
}

/**
 * socrates_nand_read_buf -  read chip data into buffer
 * @mtd:	MTD device structure
 * @buf:	buffer to store date
 * @len:	number of bytes to read
 */
static void socrates_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	int i;
	struct nand_chip *this = mtd->priv;
	struct socrates_nand_host *host = this->priv;
	uint32_t val;

	val = FPGA_NAND_ENABLE | FPGA_NAND_CMD_READ;

	out_be32(host->io_base, val);
	for (i = 0; i < len; i++) {
		buf[i] = (in_be32(host->io_base) >>
				FPGA_NAND_DATA_SHIFT) & 0xff;
	}
}

/**
 * socrates_nand_read_byte -  read one byte from the chip
 * @mtd:	MTD device structure
 */
static uint8_t socrates_nand_read_byte(struct mtd_info *mtd)
{
	uint8_t byte;
	socrates_nand_read_buf(mtd, &byte, sizeof(byte));
	return byte;
}

/**
 * socrates_nand_read_word -  read one word from the chip
 * @mtd:	MTD device structure
 */
static uint16_t socrates_nand_read_word(struct mtd_info *mtd)
{
	uint16_t word;
	socrates_nand_read_buf(mtd, (uint8_t *)&word, sizeof(word));
	return word;
}

/**
 * socrates_nand_verify_buf -  Verify chip data against buffer
 * @mtd:	MTD device structure
 * @buf:	buffer containing the data to compare
 * @len:	number of bytes to compare
 */
static int socrates_nand_verify_buf(struct mtd_info *mtd, const u8 *buf,
		int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (buf[i] != socrates_nand_read_byte(mtd))
			return -EFAULT;
	}
	return 0;
}

/*
 * Hardware specific access to control-lines
 */
static void socrates_nand_cmd_ctrl(struct mtd_info *mtd, int cmd,
		unsigned int ctrl)
{
	struct nand_chip *nand_chip = mtd->priv;
	struct socrates_nand_host *host = nand_chip->priv;
	uint32_t val;

	if (cmd == NAND_CMD_NONE)
		return;

	if (ctrl & NAND_CLE)
		val = FPGA_NAND_CMD_COMMAND;
	else
		val = FPGA_NAND_CMD_ADDR;

	if (ctrl & NAND_NCE)
		val |= FPGA_NAND_ENABLE;

	val |= (cmd & 0xff) << FPGA_NAND_DATA_SHIFT;

	out_be32(host->io_base, val);
}

/*
 * Read the Device Ready pin.
 */
static int socrates_nand_device_ready(struct mtd_info *mtd)
{
	struct nand_chip *nand_chip = mtd->priv;
	struct socrates_nand_host *host = nand_chip->priv;

	if (in_be32(host->io_base) & FPGA_NAND_BUSY)
		return 0; /* busy */
	return 1;
}

#ifdef CONFIG_MTD_PARTITIONS
static const char *part_probes[] = { "cmdlinepart", NULL };
#endif

/*
 * Probe for the NAND device.
 */
static int __devinit socrates_nand_probe(struct platform_device *ofdev,
					 const struct of_device_id *ofid)
{
	struct socrates_nand_host *host;
	struct mtd_info *mtd;
	struct nand_chip *nand_chip;
	int res;

#ifdef CONFIG_MTD_PARTITIONS
	struct mtd_partition *partitions = NULL;
	int num_partitions = 0;
#endif

	/* Allocate memory for the device structure (and zero it) */
	host = kzalloc(sizeof(struct socrates_nand_host), GFP_KERNEL);
	if (!host) {
		printk(KERN_ERR
		       "socrates_nand: failed to allocate device structure.\n");
		return -ENOMEM;
	}

	host->io_base = of_iomap(ofdev->dev.of_node, 0);
	if (host->io_base == NULL) {
		printk(KERN_ERR "socrates_nand: ioremap failed\n");
		kfree(host);
		return -EIO;
	}

	mtd = &host->mtd;
	nand_chip = &host->nand_chip;
	host->dev = &ofdev->dev;

	nand_chip->priv = host;		/* link the private data structures */
	mtd->priv = nand_chip;
	mtd->name = "socrates_nand";
	mtd->owner = THIS_MODULE;
	mtd->dev.parent = &ofdev->dev;

	/*should never be accessed directly */
	nand_chip->IO_ADDR_R = (void *)0xdeadbeef;
	nand_chip->IO_ADDR_W = (void *)0xdeadbeef;

	nand_chip->cmd_ctrl = socrates_nand_cmd_ctrl;
	nand_chip->read_byte = socrates_nand_read_byte;
	nand_chip->read_word = socrates_nand_read_word;
	nand_chip->write_buf = socrates_nand_write_buf;
	nand_chip->read_buf = socrates_nand_read_buf;
	nand_chip->verify_buf = socrates_nand_verify_buf;
	nand_chip->dev_ready = socrates_nand_device_ready;

	nand_chip->ecc.mode = NAND_ECC_SOFT;	/* enable ECC */

	/* TODO: I have no idea what real delay is. */
	nand_chip->chip_delay = 20;		/* 20us command delay time */

	dev_set_drvdata(&ofdev->dev, host);

	/* first scan to find the device and get the page size */
	if (nand_scan_ident(mtd, 1, NULL)) {
		res = -ENXIO;
		goto out;
	}

	/* second phase scan */
	if (nand_scan_tail(mtd)) {
		res = -ENXIO;
		goto out;
	}

#ifdef CONFIG_MTD_PARTITIONS
#ifdef CONFIG_MTD_CMDLINE_PARTS
	num_partitions = parse_mtd_partitions(mtd, part_probes,
					      &partitions, 0);
	if (num_partitions < 0) {
		res = num_partitions;
		goto release;
	}
#endif

#ifdef CONFIG_MTD_OF_PARTS
	if (num_partitions == 0) {
		num_partitions = of_mtd_parse_partitions(&ofdev->dev,
							 ofdev->dev.of_node,
							 &partitions);
		if (num_partitions < 0) {
			res = num_partitions;
			goto release;
		}
	}
#endif
	if (partitions && (num_partitions > 0))
		res = add_mtd_partitions(mtd, partitions, num_partitions);
	else
#endif
		res = add_mtd_device(mtd);

	if (!res)
		return res;

#ifdef CONFIG_MTD_PARTITIONS
release:
#endif
	nand_release(mtd);

out:
	dev_set_drvdata(&ofdev->dev, NULL);
	iounmap(host->io_base);
	kfree(host);
	return res;
}

/*
 * Remove a NAND device.
 */
static int __devexit socrates_nand_remove(struct platform_device *ofdev)
{
	struct socrates_nand_host *host = dev_get_drvdata(&ofdev->dev);
	struct mtd_info *mtd = &host->mtd;

	nand_release(mtd);

	dev_set_drvdata(&ofdev->dev, NULL);
	iounmap(host->io_base);
	kfree(host);

	return 0;
}

static const struct of_device_id socrates_nand_match[] =
{
	{
		.compatible   = "abb,socrates-nand",
	},
	{},
};

MODULE_DEVICE_TABLE(of, socrates_nand_match);

static struct of_platform_driver socrates_nand_driver = {
	.driver = {
		.name = "socrates_nand",
		.owner = THIS_MODULE,
		.of_match_table = socrates_nand_match,
	},
	.probe		= socrates_nand_probe,
	.remove		= __devexit_p(socrates_nand_remove),
};

static int __init socrates_nand_init(void)
{
	return of_register_platform_driver(&socrates_nand_driver);
}

static void __exit socrates_nand_exit(void)
{
	of_unregister_platform_driver(&socrates_nand_driver);
}

module_init(socrates_nand_init);
module_exit(socrates_nand_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ilya Yanok");
MODULE_DESCRIPTION("NAND driver for Socrates board");
