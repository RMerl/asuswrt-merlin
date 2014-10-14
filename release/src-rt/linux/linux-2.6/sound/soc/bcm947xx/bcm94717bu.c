/*
 * SoC audio for BCM94717BU Board
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: bcm94717bu.c,v 1.1 2009/10/30 20:41:44 Exp $
 */


#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/i2c-gpio.h>

#include <typedefs.h>
#include <bcmdevs.h>
#include <pcicfg.h>
#include <hndsoc.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <i2s_core.h>


#include "../codecs/wm8750.h"
#include "bcm947xx-pcm.h"
#include "bcm947xx-i2s.h"

#define BCM947XX_BU_DEBUG 0
#if BCM947XX_BU_DEBUG
#define DBG(x...) printk(KERN_ERR x)
#else
#define DBG(x...)
#endif


 /* MCLK in Hz - to bcm94717 & Wolfson 8750 */
#define BCM94717BU_MCLK_FREQ 12288000


static int bcm94717bu_startup(struct snd_pcm_substream *substream)
{
	//struct snd_soc_pcm_runtime *rtd = substream->private_data;
	//struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	//struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	int ret = 0;

	//DBG("%s:\n", __FUNCTION__);

	return ret;
}

static void bcm94717bu_shutdown(struct snd_pcm_substream *substream)
{
	//DBG("%s\n", __FUNCTION__);
	return;
}

static int bcm94717bu_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	unsigned int fmt;
	
	int ret = 0;

	fmt = SND_SOC_DAIFMT_I2S |		/* I2S mode audio */
	        SND_SOC_DAIFMT_NB_NF |		/* BCLK not inverted and normal LRCLK polarity */
	        SND_SOC_DAIFMT_CBS_CFS;		/* BCM947xx is I2S Master / codec is slave */

	/* set codec DAI configuration */
	ret = codec_dai->dai_ops.set_fmt(codec_dai, fmt);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = cpu_dai->dai_ops.set_fmt(cpu_dai, fmt);
	if (ret < 0)
		return ret;

	/* set the codec system clock for DAC and ADC */
	ret = codec_dai->dai_ops.set_sysclk(codec_dai, WM8750_SYSCLK, BCM94717BU_MCLK_FREQ,
		SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	/* set the I2S system clock as input (unused) */
	ret = cpu_dai->dai_ops.set_sysclk(cpu_dai, BCM947XX_I2S_SYSCLK, BCM94717BU_MCLK_FREQ,
		SND_SOC_CLOCK_IN);

	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops bcm94717bu_ops = {
	.startup = bcm94717bu_startup,
	.hw_params = bcm94717bu_hw_params,
	.shutdown = bcm94717bu_shutdown,
};

/*
 * Logic for a wm8750
 */
static int bcm94717bu_wm8750_init(struct snd_soc_codec *codec)
{
	DBG("%s\n", __FUNCTION__);

	snd_soc_dapm_sync_endpoints(codec);

	return 0;
}

/* bcm94717bu digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link bcm94717bu_dai = {
	.name = "WM8750",
	.stream_name = "WM8750",
	.cpu_dai = &bcm947xx_i2s_dai,
	.codec_dai = &wm8750_dai,
	.init = bcm94717bu_wm8750_init,
	.ops = &bcm94717bu_ops,
};

/* bcm94717bu audio machine driver */
static struct snd_soc_machine snd_soc_machine_bcm94717bu = {
	.name = "Bcm94717bu",
	.dai_link = &bcm94717bu_dai,
	.num_links = 1,
};

/* bcm94717bu audio private data */
static struct wm8750_setup_data bcm94717bu_wm8750_setup = {
	.i2c_address = 0x1a, /* 2wire / I2C interface */
};

/* bcm94717bu audio subsystem */
static struct snd_soc_device bcm94717bu_snd_devdata = {
	.machine = &snd_soc_machine_bcm94717bu,
	.platform = &bcm947xx_soc_platform,
	.codec_dev = &soc_codec_dev_wm8750,
	.codec_data = &bcm94717bu_wm8750_setup,
};

static struct platform_device *bcm94717bu_snd_device;

static int machine_is_bcm94717bu(void)
{
	DBG("%s\n", __FUNCTION__);
	return 1;
}



static struct i2c_gpio_platform_data i2c_gpio_data = {
	.sda_pin	= 1,
	.scl_pin	= 4,
};

static struct platform_device i2c_gpio_device = {
	.name		= "i2c-gpio",
	.id		= 0,
	.dev		= {
		.platform_data	= &i2c_gpio_data,
	},
};


static int __init bcm94717bu_init(void)
{
	int ret;

	DBG("%s\n", __FUNCTION__);

	if (!machine_is_bcm94717bu())
		return -ENODEV;

	ret = platform_device_register(&i2c_gpio_device);
	if (ret) {
		platform_device_put(&i2c_gpio_device);
		return ret;
	}

	bcm94717bu_snd_device = platform_device_alloc("soc-audio", -1);
	if (!bcm94717bu_snd_device)
		return -ENOMEM;

	platform_set_drvdata(bcm94717bu_snd_device, &bcm94717bu_snd_devdata);
	bcm94717bu_snd_devdata.dev = &bcm94717bu_snd_device->dev;
	ret = platform_device_add(bcm94717bu_snd_device);

	if (ret) {
		platform_device_put(bcm94717bu_snd_device);
	}

	return ret;
}

static void __exit bcm94717bu_exit(void)
{
	platform_device_unregister(bcm94717bu_snd_device);
}

module_init(bcm94717bu_init);
module_exit(bcm94717bu_exit);

/* Module information */
MODULE_DESCRIPTION("ALSA SoC BCM94717BU");
MODULE_LICENSE("GPL");
