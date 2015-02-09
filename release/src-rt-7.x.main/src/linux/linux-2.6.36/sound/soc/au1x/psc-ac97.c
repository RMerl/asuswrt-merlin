/*
 * Au12x0/Au1550 PSC ALSA ASoC audio support.
 *
 * (c) 2007-2009 MSC Vertriebsges.m.b.H.,
 *	Manuel Lauss <manuel.lauss@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Au1xxx-PSC AC97 glue.
 *
 * NOTE: all of these drivers can only work with a SINGLE instance
 *	 of a PSC. Multiple independent audio devices are impossible
 *	 with ASoC v1.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/suspend.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <asm/mach-au1x00/au1000.h>
#include <asm/mach-au1x00/au1xxx_psc.h>

#include "psc.h"

/* how often to retry failed codec register reads/writes */
#define AC97_RW_RETRIES	5

#define AC97_DIR	\
	(SND_SOC_DAIDIR_PLAYBACK | SND_SOC_DAIDIR_CAPTURE)

#define AC97_RATES	\
	SNDRV_PCM_RATE_8000_48000

#define AC97_FMTS	\
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3BE)

#define AC97PCR_START(stype)	\
	((stype) == PCM_TX ? PSC_AC97PCR_TS : PSC_AC97PCR_RS)
#define AC97PCR_STOP(stype)	\
	((stype) == PCM_TX ? PSC_AC97PCR_TP : PSC_AC97PCR_RP)
#define AC97PCR_CLRFIFO(stype)	\
	((stype) == PCM_TX ? PSC_AC97PCR_TC : PSC_AC97PCR_RC)

#define AC97STAT_BUSY(stype)	\
	((stype) == PCM_TX ? PSC_AC97STAT_TB : PSC_AC97STAT_RB)

/* instance data. There can be only one, MacLeod!!!! */
static struct au1xpsc_audio_data *au1xpsc_ac97_workdata;

/* AC97 controller reads codec register */
static unsigned short au1xpsc_ac97_read(struct snd_ac97 *ac97,
					unsigned short reg)
{
	struct au1xpsc_audio_data *pscdata = au1xpsc_ac97_workdata;
	unsigned short retry, tmo;
	unsigned long data;

	au_writel(PSC_AC97EVNT_CD, AC97_EVNT(pscdata));
	au_sync();

	retry = AC97_RW_RETRIES;
	do {
		mutex_lock(&pscdata->lock);

		au_writel(PSC_AC97CDC_RD | PSC_AC97CDC_INDX(reg),
			  AC97_CDC(pscdata));
		au_sync();

		tmo = 20;
		do {
			udelay(21);
			if (au_readl(AC97_EVNT(pscdata)) & PSC_AC97EVNT_CD)
				break;
		} while (--tmo);

		data = au_readl(AC97_CDC(pscdata));

		au_writel(PSC_AC97EVNT_CD, AC97_EVNT(pscdata));
		au_sync();

		mutex_unlock(&pscdata->lock);

		if (reg != ((data >> 16) & 0x7f))
			tmo = 1;	/* wrong register, try again */

	} while (--retry && !tmo);

	return retry ? data & 0xffff : 0xffff;
}

/* AC97 controller writes to codec register */
static void au1xpsc_ac97_write(struct snd_ac97 *ac97, unsigned short reg,
				unsigned short val)
{
	struct au1xpsc_audio_data *pscdata = au1xpsc_ac97_workdata;
	unsigned int tmo, retry;

	au_writel(PSC_AC97EVNT_CD, AC97_EVNT(pscdata));
	au_sync();

	retry = AC97_RW_RETRIES;
	do {
		mutex_lock(&pscdata->lock);

		au_writel(PSC_AC97CDC_INDX(reg) | (val & 0xffff),
			  AC97_CDC(pscdata));
		au_sync();

		tmo = 20;
		do {
			udelay(21);
			if (au_readl(AC97_EVNT(pscdata)) & PSC_AC97EVNT_CD)
				break;
		} while (--tmo);

		au_writel(PSC_AC97EVNT_CD, AC97_EVNT(pscdata));
		au_sync();

		mutex_unlock(&pscdata->lock);
	} while (--retry && !tmo);
}

/* AC97 controller asserts a warm reset */
static void au1xpsc_ac97_warm_reset(struct snd_ac97 *ac97)
{
	struct au1xpsc_audio_data *pscdata = au1xpsc_ac97_workdata;

	au_writel(PSC_AC97RST_SNC, AC97_RST(pscdata));
	au_sync();
	msleep(10);
	au_writel(0, AC97_RST(pscdata));
	au_sync();
}

static void au1xpsc_ac97_cold_reset(struct snd_ac97 *ac97)
{
	struct au1xpsc_audio_data *pscdata = au1xpsc_ac97_workdata;
	int i;

	/* disable PSC during cold reset */
	au_writel(0, AC97_CFG(au1xpsc_ac97_workdata));
	au_sync();
	au_writel(PSC_CTRL_DISABLE, PSC_CTRL(pscdata));
	au_sync();

	/* issue cold reset */
	au_writel(PSC_AC97RST_RST, AC97_RST(pscdata));
	au_sync();
	msleep(500);
	au_writel(0, AC97_RST(pscdata));
	au_sync();

	/* enable PSC */
	au_writel(PSC_CTRL_ENABLE, PSC_CTRL(pscdata));
	au_sync();

	/* wait for PSC to indicate it's ready */
	i = 1000;
	while (!((au_readl(AC97_STAT(pscdata)) & PSC_AC97STAT_SR)) && (--i))
		msleep(1);

	if (i == 0) {
		printk(KERN_ERR "au1xpsc-ac97: PSC not ready!\n");
		return;
	}

	/* enable the ac97 function */
	au_writel(pscdata->cfg | PSC_AC97CFG_DE_ENABLE, AC97_CFG(pscdata));
	au_sync();

	/* wait for AC97 core to become ready */
	i = 1000;
	while (!((au_readl(AC97_STAT(pscdata)) & PSC_AC97STAT_DR)) && (--i))
		msleep(1);
	if (i == 0)
		printk(KERN_ERR "au1xpsc-ac97: AC97 ctrl not ready\n");
}

/* AC97 controller operations */
struct snd_ac97_bus_ops soc_ac97_ops = {
	.read		= au1xpsc_ac97_read,
	.write		= au1xpsc_ac97_write,
	.reset		= au1xpsc_ac97_cold_reset,
	.warm_reset	= au1xpsc_ac97_warm_reset,
};
EXPORT_SYMBOL_GPL(soc_ac97_ops);

static int au1xpsc_ac97_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct au1xpsc_audio_data *pscdata = au1xpsc_ac97_workdata;
	unsigned long r, ro, stat;
	int chans, t, stype = SUBSTREAM_TYPE(substream);

	chans = params_channels(params);

	r = ro = au_readl(AC97_CFG(pscdata));
	stat = au_readl(AC97_STAT(pscdata));

	/* already active? */
	if (stat & (PSC_AC97STAT_TB | PSC_AC97STAT_RB)) {
		/* reject parameters not currently set up */
		if ((PSC_AC97CFG_GET_LEN(r) != params->msbits) ||
		    (pscdata->rate != params_rate(params)))
			return -EINVAL;
	} else {

		/* set sample bitdepth: REG[24:21]=(BITS-2)/2 */
		r &= ~PSC_AC97CFG_LEN_MASK;
		r |= PSC_AC97CFG_SET_LEN(params->msbits);

		/* channels: enable slots for front L/R channel */
		if (stype == PCM_TX) {
			r &= ~PSC_AC97CFG_TXSLOT_MASK;
			r |= PSC_AC97CFG_TXSLOT_ENA(3);
			r |= PSC_AC97CFG_TXSLOT_ENA(4);
		} else {
			r &= ~PSC_AC97CFG_RXSLOT_MASK;
			r |= PSC_AC97CFG_RXSLOT_ENA(3);
			r |= PSC_AC97CFG_RXSLOT_ENA(4);
		}

		/* do we need to poke the hardware? */
		if (!(r ^ ro))
			goto out;

		/* ac97 engine is about to be disabled */
		mutex_lock(&pscdata->lock);

		/* disable AC97 device controller first... */
		au_writel(r & ~PSC_AC97CFG_DE_ENABLE, AC97_CFG(pscdata));
		au_sync();

		/* ...wait for it... */
		t = 100;
		while ((au_readl(AC97_STAT(pscdata)) & PSC_AC97STAT_DR) && --t)
			msleep(1);

		if (!t)
			printk(KERN_ERR "PSC-AC97: can't disable!\n");

		/* ...write config... */
		au_writel(r, AC97_CFG(pscdata));
		au_sync();

		/* ...enable the AC97 controller again... */
		au_writel(r | PSC_AC97CFG_DE_ENABLE, AC97_CFG(pscdata));
		au_sync();

		/* ...and wait for ready bit */
		t = 100;
		while ((!(au_readl(AC97_STAT(pscdata)) & PSC_AC97STAT_DR)) && --t)
			msleep(1);

		if (!t)
			printk(KERN_ERR "PSC-AC97: can't enable!\n");

		mutex_unlock(&pscdata->lock);

		pscdata->cfg = r;
		pscdata->rate = params_rate(params);
	}

out:
	return 0;
}

static int au1xpsc_ac97_trigger(struct snd_pcm_substream *substream,
				int cmd, struct snd_soc_dai *dai)
{
	struct au1xpsc_audio_data *pscdata = au1xpsc_ac97_workdata;
	int ret, stype = SUBSTREAM_TYPE(substream);

	ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		au_writel(AC97PCR_CLRFIFO(stype), AC97_PCR(pscdata));
		au_sync();
		au_writel(AC97PCR_START(stype), AC97_PCR(pscdata));
		au_sync();
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		au_writel(AC97PCR_STOP(stype), AC97_PCR(pscdata));
		au_sync();

		while (au_readl(AC97_STAT(pscdata)) & AC97STAT_BUSY(stype))
			asm volatile ("nop");

		au_writel(AC97PCR_CLRFIFO(stype), AC97_PCR(pscdata));
		au_sync();

		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int au1xpsc_ac97_probe(struct platform_device *pdev,
			      struct snd_soc_dai *dai)
{
	return au1xpsc_ac97_workdata ? 0 : -ENODEV;
}

static void au1xpsc_ac97_remove(struct platform_device *pdev,
				struct snd_soc_dai *dai)
{
}

static struct snd_soc_dai_ops au1xpsc_ac97_dai_ops = {
	.trigger	= au1xpsc_ac97_trigger,
	.hw_params	= au1xpsc_ac97_hw_params,
};

struct snd_soc_dai au1xpsc_ac97_dai = {
	.name			= "au1xpsc_ac97",
	.ac97_control		= 1,
	.probe			= au1xpsc_ac97_probe,
	.remove			= au1xpsc_ac97_remove,
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
	.ops = &au1xpsc_ac97_dai_ops,
};
EXPORT_SYMBOL_GPL(au1xpsc_ac97_dai);

static int __devinit au1xpsc_ac97_drvprobe(struct platform_device *pdev)
{
	int ret;
	struct resource *r;
	unsigned long sel;
	struct au1xpsc_audio_data *wd;

	if (au1xpsc_ac97_workdata)
		return -EBUSY;

	wd = kzalloc(sizeof(struct au1xpsc_audio_data), GFP_KERNEL);
	if (!wd)
		return -ENOMEM;

	mutex_init(&wd->lock);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r) {
		ret = -ENODEV;
		goto out0;
	}

	ret = -EBUSY;
	if (!request_mem_region(r->start, resource_size(r), pdev->name))
		goto out0;

	wd->mmio = ioremap(r->start, resource_size(r));
	if (!wd->mmio)
		goto out1;

	/* configuration: max dma trigger threshold, enable ac97 */
	wd->cfg = PSC_AC97CFG_RT_FIFO8 | PSC_AC97CFG_TT_FIFO8 |
		  PSC_AC97CFG_DE_ENABLE;

	/* preserve PSC clock source set up by platform	 */
	sel = au_readl(PSC_SEL(wd)) & PSC_SEL_CLK_MASK;
	au_writel(PSC_CTRL_DISABLE, PSC_CTRL(wd));
	au_sync();
	au_writel(0, PSC_SEL(wd));
	au_sync();
	au_writel(PSC_SEL_PS_AC97MODE | sel, PSC_SEL(wd));
	au_sync();

	ret = snd_soc_register_dai(&au1xpsc_ac97_dai);
	if (ret)
		goto out1;

	wd->dmapd = au1xpsc_pcm_add(pdev);
	if (wd->dmapd) {
		platform_set_drvdata(pdev, wd);
		au1xpsc_ac97_workdata = wd;	/* MDEV */
		return 0;
	}

	snd_soc_unregister_dai(&au1xpsc_ac97_dai);
out1:
	release_mem_region(r->start, resource_size(r));
out0:
	kfree(wd);
	return ret;
}

static int __devexit au1xpsc_ac97_drvremove(struct platform_device *pdev)
{
	struct au1xpsc_audio_data *wd = platform_get_drvdata(pdev);
	struct resource *r = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (wd->dmapd)
		au1xpsc_pcm_destroy(wd->dmapd);

	snd_soc_unregister_dai(&au1xpsc_ac97_dai);

	/* disable PSC completely */
	au_writel(0, AC97_CFG(wd));
	au_sync();
	au_writel(PSC_CTRL_DISABLE, PSC_CTRL(wd));
	au_sync();

	iounmap(wd->mmio);
	release_mem_region(r->start, resource_size(r));
	kfree(wd);

	au1xpsc_ac97_workdata = NULL;	/* MDEV */

	return 0;
}

#ifdef CONFIG_PM
static int au1xpsc_ac97_drvsuspend(struct device *dev)
{
	struct au1xpsc_audio_data *wd = dev_get_drvdata(dev);

	/* save interesting registers and disable PSC */
	wd->pm[0] = au_readl(PSC_SEL(wd));

	au_writel(0, AC97_CFG(wd));
	au_sync();
	au_writel(PSC_CTRL_DISABLE, PSC_CTRL(wd));
	au_sync();

	return 0;
}

static int au1xpsc_ac97_drvresume(struct device *dev)
{
	struct au1xpsc_audio_data *wd = dev_get_drvdata(dev);

	/* restore PSC clock config */
	au_writel(wd->pm[0] | PSC_SEL_PS_AC97MODE, PSC_SEL(wd));
	au_sync();

	/* after this point the ac97 core will cold-reset the codec.
	 * During cold-reset the PSC is reinitialized and the last
	 * configuration set up in hw_params() is restored.
	 */
	return 0;
}

static struct dev_pm_ops au1xpscac97_pmops = {
	.suspend	= au1xpsc_ac97_drvsuspend,
	.resume		= au1xpsc_ac97_drvresume,
};

#define AU1XPSCAC97_PMOPS &au1xpscac97_pmops

#else

#define AU1XPSCAC97_PMOPS NULL

#endif

static struct platform_driver au1xpsc_ac97_driver = {
	.driver	= {
		.name	= "au1xpsc_ac97",
		.owner	= THIS_MODULE,
		.pm	= AU1XPSCAC97_PMOPS,
	},
	.probe		= au1xpsc_ac97_drvprobe,
	.remove		= __devexit_p(au1xpsc_ac97_drvremove),
};

static int __init au1xpsc_ac97_load(void)
{
	au1xpsc_ac97_workdata = NULL;
	return platform_driver_register(&au1xpsc_ac97_driver);
}

static void __exit au1xpsc_ac97_unload(void)
{
	platform_driver_unregister(&au1xpsc_ac97_driver);
}

module_init(au1xpsc_ac97_load);
module_exit(au1xpsc_ac97_unload);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Au12x0/Au1550 PSC AC97 ALSA ASoC audio driver");
MODULE_AUTHOR("Manuel Lauss");
