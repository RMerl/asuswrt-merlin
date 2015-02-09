/*
 * TXx9 ACLC AC97 driver
 *
 * Copyright (C) 2009 Atsushi Nemoto
 *
 * Based on RBTX49xx patch from CELF patch archive.
 * (C) Copyright TOSHIBA CORPORATION 2004-2006
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gfp.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include "txx9aclc.h"

#define AC97_DIR	\
	(SND_SOC_DAIDIR_PLAYBACK | SND_SOC_DAIDIR_CAPTURE)

#define AC97_RATES	\
	SNDRV_PCM_RATE_8000_48000

#ifdef __BIG_ENDIAN
#define AC97_FMTS	SNDRV_PCM_FMTBIT_S16_BE
#else
#define AC97_FMTS	SNDRV_PCM_FMTBIT_S16_LE
#endif

static DECLARE_WAIT_QUEUE_HEAD(ac97_waitq);

/* REVISIT: How to find txx9aclc_soc_device from snd_ac97? */
static struct txx9aclc_soc_device *txx9aclc_soc_dev;

static int txx9aclc_regready(struct txx9aclc_soc_device *dev)
{
	struct txx9aclc_plat_drvdata *drvdata = txx9aclc_get_plat_drvdata(dev);

	return __raw_readl(drvdata->base + ACINTSTS) & ACINT_REGACCRDY;
}

/* AC97 controller reads codec register */
static unsigned short txx9aclc_ac97_read(struct snd_ac97 *ac97,
					 unsigned short reg)
{
	struct txx9aclc_soc_device *dev = txx9aclc_soc_dev;
	struct txx9aclc_plat_drvdata *drvdata = txx9aclc_get_plat_drvdata(dev);
	void __iomem *base = drvdata->base;
	u32 dat;

	if (!(__raw_readl(base + ACINTSTS) & ACINT_CODECRDY(ac97->num)))
		return 0xffff;
	reg |= ac97->num << 7;
	dat = (reg << ACREGACC_REG_SHIFT) | ACREGACC_READ;
	__raw_writel(dat, base + ACREGACC);
	__raw_writel(ACINT_REGACCRDY, base + ACINTEN);
	if (!wait_event_timeout(ac97_waitq, txx9aclc_regready(dev), HZ)) {
		__raw_writel(ACINT_REGACCRDY, base + ACINTDIS);
		dev_err(dev->soc_dev.dev, "ac97 read timeout (reg %#x)\n", reg);
		dat = 0xffff;
		goto done;
	}
	dat = __raw_readl(base + ACREGACC);
	if (((dat >> ACREGACC_REG_SHIFT) & 0xff) != reg) {
		dev_err(dev->soc_dev.dev, "reg mismatch %x with %x\n",
			dat, reg);
		dat = 0xffff;
		goto done;
	}
	dat = (dat >> ACREGACC_DAT_SHIFT) & 0xffff;
done:
	__raw_writel(ACINT_REGACCRDY, base + ACINTDIS);
	return dat;
}

/* AC97 controller writes to codec register */
static void txx9aclc_ac97_write(struct snd_ac97 *ac97, unsigned short reg,
				unsigned short val)
{
	struct txx9aclc_soc_device *dev = txx9aclc_soc_dev;
	struct txx9aclc_plat_drvdata *drvdata = txx9aclc_get_plat_drvdata(dev);
	void __iomem *base = drvdata->base;

	__raw_writel(((reg | (ac97->num << 7)) << ACREGACC_REG_SHIFT) |
		     (val << ACREGACC_DAT_SHIFT),
		     base + ACREGACC);
	__raw_writel(ACINT_REGACCRDY, base + ACINTEN);
	if (!wait_event_timeout(ac97_waitq, txx9aclc_regready(dev), HZ)) {
		dev_err(dev->soc_dev.dev,
			"ac97 write timeout (reg %#x)\n", reg);
	}
	__raw_writel(ACINT_REGACCRDY, base + ACINTDIS);
}

static void txx9aclc_ac97_cold_reset(struct snd_ac97 *ac97)
{
	struct txx9aclc_soc_device *dev = txx9aclc_soc_dev;
	struct txx9aclc_plat_drvdata *drvdata = txx9aclc_get_plat_drvdata(dev);
	void __iomem *base = drvdata->base;
	u32 ready = ACINT_CODECRDY(ac97->num) | ACINT_REGACCRDY;

	__raw_writel(ACCTL_ENLINK, base + ACCTLDIS);
	mmiowb();
	udelay(1);
	__raw_writel(ACCTL_ENLINK, base + ACCTLEN);
	/* wait for primary codec ready status */
	__raw_writel(ready, base + ACINTEN);
	if (!wait_event_timeout(ac97_waitq,
				(__raw_readl(base + ACINTSTS) & ready) == ready,
				HZ)) {
		dev_err(&ac97->dev, "primary codec is not ready "
			"(status %#x)\n",
			__raw_readl(base + ACINTSTS));
	}
	__raw_writel(ACINT_REGACCRDY, base + ACINTSTS);
	__raw_writel(ready, base + ACINTDIS);
}

/* AC97 controller operations */
struct snd_ac97_bus_ops soc_ac97_ops = {
	.read		= txx9aclc_ac97_read,
	.write		= txx9aclc_ac97_write,
	.reset		= txx9aclc_ac97_cold_reset,
};
EXPORT_SYMBOL_GPL(soc_ac97_ops);

static irqreturn_t txx9aclc_ac97_irq(int irq, void *dev_id)
{
	struct txx9aclc_plat_drvdata *drvdata = dev_id;
	void __iomem *base = drvdata->base;

	__raw_writel(__raw_readl(base + ACINTMSTS), base + ACINTDIS);
	wake_up(&ac97_waitq);
	return IRQ_HANDLED;
}

static int txx9aclc_ac97_probe(struct platform_device *pdev,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct txx9aclc_soc_device *dev =
		container_of(socdev, struct txx9aclc_soc_device, soc_dev);

	dev->aclc_pdev = to_platform_device(dai->dev);
	txx9aclc_soc_dev = dev;
	return 0;
}

static void txx9aclc_ac97_remove(struct platform_device *pdev,
				 struct snd_soc_dai *dai)
{
	struct platform_device *aclc_pdev = to_platform_device(dai->dev);
	struct txx9aclc_plat_drvdata *drvdata = platform_get_drvdata(aclc_pdev);

	/* disable AC-link */
	__raw_writel(ACCTL_ENLINK, drvdata->base + ACCTLDIS);
	txx9aclc_soc_dev = NULL;
}

struct snd_soc_dai txx9aclc_ac97_dai = {
	.name			= "txx9aclc_ac97",
	.ac97_control		= 1,
	.probe			= txx9aclc_ac97_probe,
	.remove			= txx9aclc_ac97_remove,
	.playback = {
		.rates		= AC97_RATES,
		.formats	= AC97_FMTS,
		.channels_min	= 2,
		.channels_max	= 2,
	},
	.capture = {
		.rates		= AC97_RATES,
		.formats	= AC97_FMTS,
		.channels_min	= 2,
		.channels_max	= 2,
	},
};
EXPORT_SYMBOL_GPL(txx9aclc_ac97_dai);

static int __devinit txx9aclc_ac97_dev_probe(struct platform_device *pdev)
{
	struct txx9aclc_plat_drvdata *drvdata;
	struct resource *r;
	int err;
	int irq;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;
	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r)
		return -EBUSY;

	if (!devm_request_mem_region(&pdev->dev, r->start, resource_size(r),
				     dev_name(&pdev->dev)))
		return -EBUSY;

	drvdata = devm_kzalloc(&pdev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;
	platform_set_drvdata(pdev, drvdata);
	drvdata->physbase = r->start;
	if (sizeof(drvdata->physbase) > sizeof(r->start) &&
	    r->start >= TXX9_DIRECTMAP_BASE &&
	    r->start < TXX9_DIRECTMAP_BASE + 0x400000)
		drvdata->physbase |= 0xf00000000ull;
	drvdata->base = devm_ioremap(&pdev->dev, r->start, resource_size(r));
	if (!drvdata->base)
		return -EBUSY;
	err = devm_request_irq(&pdev->dev, irq, txx9aclc_ac97_irq,
			       IRQF_DISABLED, dev_name(&pdev->dev), drvdata);
	if (err < 0)
		return err;

	txx9aclc_ac97_dai.dev = &pdev->dev;
	return snd_soc_register_dai(&txx9aclc_ac97_dai);
}

static int __devexit txx9aclc_ac97_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&txx9aclc_ac97_dai);
	return 0;
}

static struct platform_driver txx9aclc_ac97_driver = {
	.probe		= txx9aclc_ac97_dev_probe,
	.remove		= __devexit_p(txx9aclc_ac97_dev_remove),
	.driver		= {
		.name	= "txx9aclc-ac97",
		.owner	= THIS_MODULE,
	},
};

static int __init txx9aclc_ac97_init(void)
{
	return platform_driver_register(&txx9aclc_ac97_driver);
}

static void __exit txx9aclc_ac97_exit(void)
{
	platform_driver_unregister(&txx9aclc_ac97_driver);
}

module_init(txx9aclc_ac97_init);
module_exit(txx9aclc_ac97_exit);

MODULE_AUTHOR("Atsushi Nemoto <anemo@mba.ocn.ne.jp>");
MODULE_DESCRIPTION("TXx9 ACLC AC97 driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:txx9aclc-ac97");
