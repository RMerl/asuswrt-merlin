/*
 * SoC audio for BCM94717AP Board
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: bcm94717ap.c,v 1.1 2009/10/30 20:40:14 Exp $
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


#include "../codecs/wm8955.h"
#include "bcm947xx-pcm.h"
#include "bcm947xx-i2s.h"

#define BCM947XX_AP_DEBUG 0
#if BCM947XX_AP_DEBUG
#define DBG(x...) printk(KERN_ERR x)
#else
#define DBG(x...)
#endif


/* MCLK in Hz - to bcm94717ap & Wolfson 8955 */
#define BCM94717AP_MCLK_FREQ 20000000 /* 20 MHz */


static int bcm94717ap_startup(struct snd_pcm_substream *substream)
{
	//struct snd_soc_pcm_runtime *rtd = substream->private_data;
	//struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	//struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	int ret = 0;

	DBG("%s:\n", __FUNCTION__);

	return ret;
}

/* we need to unmute the HP at shutdown as the mute burns power on bcm94717ap */
static void bcm94717ap_shutdown(struct snd_pcm_substream *substream)
{
	DBG("%s\n", __FUNCTION__);
	return;
}

static int bcm94717ap_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	unsigned int fmt;
	int freq = 12288000;
	//int freq = 11289600;
	int ret = 0;

	fmt = SND_SOC_DAIFMT_I2S |		/* I2S mode audio */
	      SND_SOC_DAIFMT_NB_NF |		/* BCLK not inverted and normal LRCLK polarity */
	      SND_SOC_DAIFMT_CBM_CFM;		/* BCM947xx is I2S Slave */

	/* set codec DAI configuration */
	DBG("%s: calling set_fmt with fmt 0x%x\n", __FUNCTION__, fmt);
	ret = codec_dai->dai_ops.set_fmt(codec_dai, fmt);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = cpu_dai->dai_ops.set_fmt(cpu_dai, fmt);
	if (ret < 0)
		return ret;


	/* set up the PLL in codec */
	ret = codec_dai->dai_ops.set_pll(codec_dai, 0, BCM94717AP_MCLK_FREQ, freq);
	if (ret < 0) {
		DBG("%s: Error CODEC DAI set_pll returned %d\n", __FUNCTION__, ret);
		return ret;
	}
	/* set the codec system clock for DAC and ADC */
	ret = codec_dai->dai_ops.set_sysclk(codec_dai, WM8955_SYSCLK, freq,
		SND_SOC_CLOCK_IN);
	DBG("%s: codec set_sysclk returned %d\n", __FUNCTION__, ret);
	if (ret < 0)
		return ret;

	/* set the I2S system clock as input (unused) */
	ret = cpu_dai->dai_ops.set_sysclk(cpu_dai, BCM947XX_I2S_SYSCLK, freq,
		SND_SOC_CLOCK_IN);

	DBG("%s: cpu set_sysclk returned %d\n", __FUNCTION__, ret);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops bcm94717ap_ops = {
	.startup = bcm94717ap_startup,
	.hw_params = bcm94717ap_hw_params,
	.shutdown = bcm94717ap_shutdown,
};

/*
 * Logic for a wm8955
 */
static int bcm94717ap_wm8955_init(struct snd_soc_codec *codec)
{
	DBG("%s\n", __FUNCTION__);

	snd_soc_dapm_sync_endpoints(codec);

	return 0;
}

/* bcm94717ap digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link bcm94717ap_dai = {
	.name = "WM8955",
	.stream_name = "WM8955",
	.cpu_dai = &bcm947xx_i2s_dai,
	.codec_dai = &wm8955_dai,
	.init = bcm94717ap_wm8955_init,
	.ops = &bcm94717ap_ops,
};

/* bcm94717ap audio machine driver */
static struct snd_soc_machine snd_soc_machine_bcm94717ap = {
	.name = "Bcm94717ap",
	.dai_link = &bcm94717ap_dai,
	.num_links = 1,
};

/* bcm94717ap audio private data */
static struct wm8955_setup_data bcm94717ap_wm8955_setup = {
	.i2c_address = 0x1a, /* 2wire / I2C interface */
};

/* bcm94717ap audio subsystem */
static struct snd_soc_device bcm94717ap_snd_devdata = {
	.machine = &snd_soc_machine_bcm94717ap,
	.platform = &bcm947xx_soc_platform,
	.codec_dev = &soc_codec_dev_wm8955,
	.codec_data = &bcm94717ap_wm8955_setup,
};

static struct platform_device *bcm94717ap_snd_device;

static int machine_is_bcm94717ap(void)
{
	DBG("%s\n", __FUNCTION__);
	return 1;
}


static struct i2c_gpio_platform_data i2c_gpio_data = {
	.sda_pin	= 4,
	.scl_pin	= 5,
};

static struct platform_device i2c_gpio_device = {
	.name		= "i2c-gpio",
	.id		= 0,
	.dev		= {
		.platform_data	= &i2c_gpio_data,
	},
};


static int __init bcm94717ap_init(void)
{
	int ret;

	DBG("%s\n", __FUNCTION__);

	if (!machine_is_bcm94717ap())
		return -ENODEV;

	ret = platform_device_register(&i2c_gpio_device);
	if (ret) {
		platform_device_put(&i2c_gpio_device);
		return ret;
	}

	bcm94717ap_snd_device = platform_device_alloc("soc-audio", -1);
	if (!bcm94717ap_snd_device)
		return -ENOMEM;

	platform_set_drvdata(bcm94717ap_snd_device, &bcm94717ap_snd_devdata);
	bcm94717ap_snd_devdata.dev = &bcm94717ap_snd_device->dev;
	ret = platform_device_add(bcm94717ap_snd_device);

	if (ret) {
		platform_device_put(bcm94717ap_snd_device);
	}

	return ret;
}

static void __exit bcm94717ap_exit(void)
{
	DBG("%s\n", __FUNCTION__);
	platform_device_unregister(bcm94717ap_snd_device);
}

module_init(bcm94717ap_init);
module_exit(bcm94717ap_exit);

/* Module information */
MODULE_DESCRIPTION("ALSA SoC BCM94717AP");
MODULE_LICENSE("GPL");
