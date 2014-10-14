/*
 * TXx9 NAND flash memory controller driver
 * Based on RBTX49xx patch from CELF patch archive.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * (C) Copyright TOSHIBA CORPORATION 2004-2007
 * All Rights Reserved.
 */
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>
#include <linux/io.h>
#include <asm/txx9/ndfmc.h>

/* TXX9 NDFMC Registers */
#define TXX9_NDFDTR	0x00
#define TXX9_NDFMCR	0x04
#define TXX9_NDFSR	0x08
#define TXX9_NDFISR	0x0c
#define TXX9_NDFIMR	0x10
#define TXX9_NDFSPR	0x14
#define TXX9_NDFRSTR	0x18	/* not TX4939 */

/* NDFMCR : NDFMC Mode Control */
#define TXX9_NDFMCR_WE	0x80
#define TXX9_NDFMCR_ECC_ALL	0x60
#define TXX9_NDFMCR_ECC_RESET	0x60
#define TXX9_NDFMCR_ECC_READ	0x40
#define TXX9_NDFMCR_ECC_ON	0x20
#define TXX9_NDFMCR_ECC_OFF	0x00
#define TXX9_NDFMCR_CE	0x10
#define TXX9_NDFMCR_BSPRT	0x04	/* TX4925/TX4926 only */
#define TXX9_NDFMCR_ALE	0x02
#define TXX9_NDFMCR_CLE	0x01
/* TX4939 only */
#define TXX9_NDFMCR_X16	0x0400
#define TXX9_NDFMCR_DMAREQ_MASK	0x0300
#define TXX9_NDFMCR_DMAREQ_NODMA	0x0000
#define TXX9_NDFMCR_DMAREQ_128	0x0100
#define TXX9_NDFMCR_DMAREQ_256	0x0200
#define TXX9_NDFMCR_DMAREQ_512	0x0300
#define TXX9_NDFMCR_CS_MASK	0x0c
#define TXX9_NDFMCR_CS(ch)	((ch) << 2)

/* NDFMCR : NDFMC Status */
#define TXX9_NDFSR_BUSY	0x80
/* TX4939 only */
#define TXX9_NDFSR_DMARUN	0x40

/* NDFMCR : NDFMC Reset */
#define TXX9_NDFRSTR_RST	0x01

struct txx9ndfmc_priv {
	struct platform_device *dev;
	struct nand_chip chip;
	struct mtd_info mtd;
	int cs;
	const char *mtdname;
};

#define MAX_TXX9NDFMC_DEV	4
struct txx9ndfmc_drvdata {
	struct mtd_info *mtds[MAX_TXX9NDFMC_DEV];
	void __iomem *base;
	unsigned char hold;	/* in gbusclock */
	unsigned char spw;	/* in gbusclock */
	struct nand_hw_control hw_control;
#ifdef CONFIG_MTD_PARTITIONS
	struct mtd_partition *parts[MAX_TXX9NDFMC_DEV];
#endif
};

static struct platform_device *mtd_to_platdev(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct txx9ndfmc_priv *txx9_priv = chip->priv;
	return txx9_priv->dev;
}

static void __iomem *ndregaddr(struct platform_device *dev, unsigned int reg)
{
	struct txx9ndfmc_drvdata *drvdata = platform_get_drvdata(dev);
	struct txx9ndfmc_platform_data *plat = dev->dev.platform_data;

	return drvdata->base + (reg << plat->shift);
}

static u32 txx9ndfmc_read(struct platform_device *dev, unsigned int reg)
{
	return __raw_readl(ndregaddr(dev, reg));
}

static void txx9ndfmc_write(struct platform_device *dev,
			    u32 val, unsigned int reg)
{
	__raw_writel(val, ndregaddr(dev, reg));
}

static uint8_t txx9ndfmc_read_byte(struct mtd_info *mtd)
{
	struct platform_device *dev = mtd_to_platdev(mtd);

	return txx9ndfmc_read(dev, TXX9_NDFDTR);
}

static void txx9ndfmc_write_buf(struct mtd_info *mtd, const uint8_t *buf,
				int len)
{
	struct platform_device *dev = mtd_to_platdev(mtd);
	void __iomem *ndfdtr = ndregaddr(dev, TXX9_NDFDTR);
	u32 mcr = txx9ndfmc_read(dev, TXX9_NDFMCR);

	txx9ndfmc_write(dev, mcr | TXX9_NDFMCR_WE, TXX9_NDFMCR);
	while (len--)
		__raw_writel(*buf++, ndfdtr);
	txx9ndfmc_write(dev, mcr, TXX9_NDFMCR);
}

static void txx9ndfmc_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	struct platform_device *dev = mtd_to_platdev(mtd);
	void __iomem *ndfdtr = ndregaddr(dev, TXX9_NDFDTR);

	while (len--)
		*buf++ = __raw_readl(ndfdtr);
}

static int txx9ndfmc_verify_buf(struct mtd_info *mtd, const uint8_t *buf,
				int len)
{
	struct platform_device *dev = mtd_to_platdev(mtd);
	void __iomem *ndfdtr = ndregaddr(dev, TXX9_NDFDTR);

	while (len--)
		if (*buf++ != (uint8_t)__raw_readl(ndfdtr))
			return -EFAULT;
	return 0;
}

static void txx9ndfmc_cmd_ctrl(struct mtd_info *mtd, int cmd,
			       unsigned int ctrl)
{
	struct nand_chip *chip = mtd->priv;
	struct txx9ndfmc_priv *txx9_priv = chip->priv;
	struct platform_device *dev = txx9_priv->dev;
	struct txx9ndfmc_platform_data *plat = dev->dev.platform_data;

	if (ctrl & NAND_CTRL_CHANGE) {
		u32 mcr = txx9ndfmc_read(dev, TXX9_NDFMCR);

		mcr &= ~(TXX9_NDFMCR_CLE | TXX9_NDFMCR_ALE | TXX9_NDFMCR_CE);
		mcr |= ctrl & NAND_CLE ? TXX9_NDFMCR_CLE : 0;
		mcr |= ctrl & NAND_ALE ? TXX9_NDFMCR_ALE : 0;
		/* TXX9_NDFMCR_CE bit is 0:high 1:low */
		mcr |= ctrl & NAND_NCE ? TXX9_NDFMCR_CE : 0;
		if (txx9_priv->cs >= 0 && (ctrl & NAND_NCE)) {
			mcr &= ~TXX9_NDFMCR_CS_MASK;
			mcr |= TXX9_NDFMCR_CS(txx9_priv->cs);
		}
		txx9ndfmc_write(dev, mcr, TXX9_NDFMCR);
	}
	if (cmd != NAND_CMD_NONE)
		txx9ndfmc_write(dev, cmd & 0xff, TXX9_NDFDTR);
	if (plat->flags & NDFMC_PLAT_FLAG_DUMMYWRITE) {
		/* dummy write to update external latch */
		if ((ctrl & NAND_CTRL_CHANGE) && cmd == NAND_CMD_NONE)
			txx9ndfmc_write(dev, 0, TXX9_NDFDTR);
	}
	mmiowb();
}

static int txx9ndfmc_dev_ready(struct mtd_info *mtd)
{
	struct platform_device *dev = mtd_to_platdev(mtd);

	return !(txx9ndfmc_read(dev, TXX9_NDFSR) & TXX9_NDFSR_BUSY);
}

static int txx9ndfmc_calculate_ecc(struct mtd_info *mtd, const uint8_t *dat,
				   uint8_t *ecc_code)
{
	struct platform_device *dev = mtd_to_platdev(mtd);
	struct nand_chip *chip = mtd->priv;
	int eccbytes;
	u32 mcr = txx9ndfmc_read(dev, TXX9_NDFMCR);

	mcr &= ~TXX9_NDFMCR_ECC_ALL;
	txx9ndfmc_write(dev, mcr | TXX9_NDFMCR_ECC_OFF, TXX9_NDFMCR);
	txx9ndfmc_write(dev, mcr | TXX9_NDFMCR_ECC_READ, TXX9_NDFMCR);
	for (eccbytes = chip->ecc.bytes; eccbytes > 0; eccbytes -= 3) {
		ecc_code[1] = txx9ndfmc_read(dev, TXX9_NDFDTR);
		ecc_code[0] = txx9ndfmc_read(dev, TXX9_NDFDTR);
		ecc_code[2] = txx9ndfmc_read(dev, TXX9_NDFDTR);
		ecc_code += 3;
	}
	txx9ndfmc_write(dev, mcr | TXX9_NDFMCR_ECC_OFF, TXX9_NDFMCR);
	return 0;
}

static int txx9ndfmc_correct_data(struct mtd_info *mtd, unsigned char *buf,
		unsigned char *read_ecc, unsigned char *calc_ecc)
{
	struct nand_chip *chip = mtd->priv;
	int eccsize;
	int corrected = 0;
	int stat;

	for (eccsize = chip->ecc.size; eccsize > 0; eccsize -= 256) {
		stat = __nand_correct_data(buf, read_ecc, calc_ecc, 256);
		if (stat < 0)
			return stat;
		corrected += stat;
		buf += 256;
		read_ecc += 3;
		calc_ecc += 3;
	}
	return corrected;
}

static void txx9ndfmc_enable_hwecc(struct mtd_info *mtd, int mode)
{
	struct platform_device *dev = mtd_to_platdev(mtd);
	u32 mcr = txx9ndfmc_read(dev, TXX9_NDFMCR);

	mcr &= ~TXX9_NDFMCR_ECC_ALL;
	txx9ndfmc_write(dev, mcr | TXX9_NDFMCR_ECC_RESET, TXX9_NDFMCR);
	txx9ndfmc_write(dev, mcr | TXX9_NDFMCR_ECC_OFF, TXX9_NDFMCR);
	txx9ndfmc_write(dev, mcr | TXX9_NDFMCR_ECC_ON, TXX9_NDFMCR);
}

static void txx9ndfmc_initialize(struct platform_device *dev)
{
	struct txx9ndfmc_platform_data *plat = dev->dev.platform_data;
	struct txx9ndfmc_drvdata *drvdata = platform_get_drvdata(dev);
	int tmout = 100;

	if (plat->flags & NDFMC_PLAT_FLAG_NO_RSTR)
		; /* no NDFRSTR.  Write to NDFSPR resets the NDFMC. */
	else {
		/* reset NDFMC */
		txx9ndfmc_write(dev,
				txx9ndfmc_read(dev, TXX9_NDFRSTR) |
				TXX9_NDFRSTR_RST,
				TXX9_NDFRSTR);
		while (txx9ndfmc_read(dev, TXX9_NDFRSTR) & TXX9_NDFRSTR_RST) {
			if (--tmout == 0) {
				dev_err(&dev->dev, "reset failed.\n");
				break;
			}
			udelay(1);
		}
	}
	/* setup Hold Time, Strobe Pulse Width */
	txx9ndfmc_write(dev, (drvdata->hold << 4) | drvdata->spw, TXX9_NDFSPR);
	txx9ndfmc_write(dev,
			(plat->flags & NDFMC_PLAT_FLAG_USE_BSPRT) ?
			TXX9_NDFMCR_BSPRT : 0, TXX9_NDFMCR);
}

#define TXX9NDFMC_NS_TO_CYC(gbusclk, ns) \
	DIV_ROUND_UP((ns) * DIV_ROUND_UP(gbusclk, 1000), 1000000)

static int txx9ndfmc_nand_scan(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	int ret;

	ret = nand_scan_ident(mtd, 1, NULL);
	if (!ret) {
		if (mtd->writesize >= 512) {
			/* Hardware ECC 6 byte ECC per 512 Byte data */
			chip->ecc.size = 512;
			chip->ecc.bytes = 6;
		}
		ret = nand_scan_tail(mtd);
	}
	return ret;
}

static int __init txx9ndfmc_probe(struct platform_device *dev)
{
	struct txx9ndfmc_platform_data *plat = dev->dev.platform_data;
#ifdef CONFIG_MTD_PARTITIONS
	static const char *probes[] = { "cmdlinepart", NULL };
#endif
	int hold, spw;
	int i;
	struct txx9ndfmc_drvdata *drvdata;
	unsigned long gbusclk = plat->gbus_clock;
	struct resource *res;

	res = platform_get_resource(dev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;
	drvdata = devm_kzalloc(&dev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;
	if (!devm_request_mem_region(&dev->dev, res->start,
				     resource_size(res), dev_name(&dev->dev)))
		return -EBUSY;
	drvdata->base = devm_ioremap(&dev->dev, res->start,
				     resource_size(res));
	if (!drvdata->base)
		return -EBUSY;

	hold = plat->hold ?: 20; /* tDH */
	spw = plat->spw ?: 90; /* max(tREADID, tWP, tRP) */

	hold = TXX9NDFMC_NS_TO_CYC(gbusclk, hold);
	spw = TXX9NDFMC_NS_TO_CYC(gbusclk, spw);
	if (plat->flags & NDFMC_PLAT_FLAG_HOLDADD)
		hold -= 2;	/* actual hold time : (HOLD + 2) BUSCLK */
	spw -= 1;	/* actual wait time : (SPW + 1) BUSCLK */
	hold = clamp(hold, 1, 15);
	drvdata->hold = hold;
	spw = clamp(spw, 1, 15);
	drvdata->spw = spw;
	dev_info(&dev->dev, "CLK:%ldMHz HOLD:%d SPW:%d\n",
		 (gbusclk + 500000) / 1000000, hold, spw);

	spin_lock_init(&drvdata->hw_control.lock);
	init_waitqueue_head(&drvdata->hw_control.wq);

	platform_set_drvdata(dev, drvdata);
	txx9ndfmc_initialize(dev);

	for (i = 0; i < MAX_TXX9NDFMC_DEV; i++) {
		struct txx9ndfmc_priv *txx9_priv;
		struct nand_chip *chip;
		struct mtd_info *mtd;
#ifdef CONFIG_MTD_PARTITIONS
		int nr_parts;
#endif

		if (!(plat->ch_mask & (1 << i)))
			continue;
		txx9_priv = kzalloc(sizeof(struct txx9ndfmc_priv),
				    GFP_KERNEL);
		if (!txx9_priv) {
			dev_err(&dev->dev, "Unable to allocate "
				"TXx9 NDFMC MTD device structure.\n");
			continue;
		}
		chip = &txx9_priv->chip;
		mtd = &txx9_priv->mtd;
		mtd->owner = THIS_MODULE;

		mtd->priv = chip;

		chip->read_byte = txx9ndfmc_read_byte;
		chip->read_buf = txx9ndfmc_read_buf;
		chip->write_buf = txx9ndfmc_write_buf;
		chip->verify_buf = txx9ndfmc_verify_buf;
		chip->cmd_ctrl = txx9ndfmc_cmd_ctrl;
		chip->dev_ready = txx9ndfmc_dev_ready;
		chip->ecc.calculate = txx9ndfmc_calculate_ecc;
		chip->ecc.correct = txx9ndfmc_correct_data;
		chip->ecc.hwctl = txx9ndfmc_enable_hwecc;
		chip->ecc.mode = NAND_ECC_HW;
		/* txx9ndfmc_nand_scan will overwrite ecc.size and ecc.bytes */
		chip->ecc.size = 256;
		chip->ecc.bytes = 3;
		chip->chip_delay = 100;
		chip->controller = &drvdata->hw_control;

		chip->priv = txx9_priv;
		txx9_priv->dev = dev;

		if (plat->ch_mask != 1) {
			txx9_priv->cs = i;
			txx9_priv->mtdname = kasprintf(GFP_KERNEL, "%s.%u",
						       dev_name(&dev->dev), i);
		} else {
			txx9_priv->cs = -1;
			txx9_priv->mtdname = kstrdup(dev_name(&dev->dev),
						     GFP_KERNEL);
		}
		if (!txx9_priv->mtdname) {
			kfree(txx9_priv);
			dev_err(&dev->dev, "Unable to allocate MTD name.\n");
			continue;
		}
		if (plat->wide_mask & (1 << i))
			chip->options |= NAND_BUSWIDTH_16;

		if (txx9ndfmc_nand_scan(mtd)) {
			kfree(txx9_priv->mtdname);
			kfree(txx9_priv);
			continue;
		}
		mtd->name = txx9_priv->mtdname;

#ifdef CONFIG_MTD_PARTITIONS
		nr_parts = parse_mtd_partitions(mtd, probes,
						&drvdata->parts[i], 0);
		if (nr_parts > 0)
			add_mtd_partitions(mtd, drvdata->parts[i], nr_parts);
#endif
		add_mtd_device(mtd);
		drvdata->mtds[i] = mtd;
	}

	return 0;
}

static int __exit txx9ndfmc_remove(struct platform_device *dev)
{
	struct txx9ndfmc_drvdata *drvdata = platform_get_drvdata(dev);
	int i;

	platform_set_drvdata(dev, NULL);
	if (!drvdata)
		return 0;
	for (i = 0; i < MAX_TXX9NDFMC_DEV; i++) {
		struct mtd_info *mtd = drvdata->mtds[i];
		struct nand_chip *chip;
		struct txx9ndfmc_priv *txx9_priv;

		if (!mtd)
			continue;
		chip = mtd->priv;
		txx9_priv = chip->priv;

		nand_release(mtd);
#ifdef CONFIG_MTD_PARTITIONS
		kfree(drvdata->parts[i]);
#endif
		kfree(txx9_priv->mtdname);
		kfree(txx9_priv);
	}
	return 0;
}

#ifdef CONFIG_PM
static int txx9ndfmc_resume(struct platform_device *dev)
{
	if (platform_get_drvdata(dev))
		txx9ndfmc_initialize(dev);
	return 0;
}
#else
#define txx9ndfmc_resume NULL
#endif

static struct platform_driver txx9ndfmc_driver = {
	.remove		= __exit_p(txx9ndfmc_remove),
	.resume		= txx9ndfmc_resume,
	.driver		= {
		.name	= "txx9ndfmc",
		.owner	= THIS_MODULE,
	},
};

static int __init txx9ndfmc_init(void)
{
	return platform_driver_probe(&txx9ndfmc_driver, txx9ndfmc_probe);
}

static void __exit txx9ndfmc_exit(void)
{
	platform_driver_unregister(&txx9ndfmc_driver);
}

module_init(txx9ndfmc_init);
module_exit(txx9ndfmc_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TXx9 SoC NAND flash controller driver");
MODULE_ALIAS("platform:txx9ndfmc");
