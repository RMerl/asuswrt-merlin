/*
 * eukrea-tlv320.c  --  SoC audio for eukrea_cpuimxXX in I2S mode
 *
 * Copyright 2010 Eric Bénard, Eukréa Electromatique <eric@eukrea.com>
 *
 * based on sound/soc/s3c24xx/s3c24xx_simtec_tlv320aic23.c
 * which is Copyright 2009 Simtec Electronics
 * and on sound/soc/imx/phycore-ac97.c which is
 * Copyright 2009 Sascha Hauer, Pengutronix <s.hauer@pengutronix.de>
 * 
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <asm/mach-types.h>

#include "../codecs/tlv320aic23.h"
#include "imx-ssi.h"

#define CODEC_CLOCK 12000000

static int eukrea_tlv320_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	int ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
				  SND_SOC_DAIFMT_NB_NF |
				  SND_SOC_DAIFMT_CBM_CFM);
	if (ret) {
		pr_err("%s: failed set cpu dai format\n", __func__);
		return ret;
	}

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
				  SND_SOC_DAIFMT_NB_NF |
				  SND_SOC_DAIFMT_CBM_CFM);
	if (ret) {
		pr_err("%s: failed set codec dai format\n", __func__);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(codec_dai, 0,
				     CODEC_CLOCK, SND_SOC_CLOCK_OUT);
	if (ret) {
		pr_err("%s: failed setting codec sysclk\n", __func__);
		return ret;
	}
	snd_soc_dai_set_tdm_slot(cpu_dai, 0xffffffc, 0xffffffc, 2, 0);

	ret = snd_soc_dai_set_sysclk(cpu_dai, IMX_SSP_SYS_CLK, 0,
				SND_SOC_CLOCK_IN);
	if (ret) {
		pr_err("can't set CPU system clock IMX_SSP_SYS_CLK\n");
		return ret;
	}

	return 0;
}

static struct snd_soc_ops eukrea_tlv320_snd_ops = {
	.hw_params	= eukrea_tlv320_hw_params,
};

static struct snd_soc_dai_link eukrea_tlv320_dai = {
	.name		= "tlv320aic23",
	.stream_name	= "TLV320AIC23",
	.codec_dai	= &tlv320aic23_dai,
	.ops		= &eukrea_tlv320_snd_ops,
};

static struct snd_soc_card eukrea_tlv320 = {
	.name		= "cpuimx-audio",
	.platform	= &imx_soc_platform,
	.dai_link	= &eukrea_tlv320_dai,
	.num_links	= 1,
};

static struct snd_soc_device eukrea_tlv320_snd_devdata = {
	.card		= &eukrea_tlv320,
	.codec_dev	= &soc_codec_dev_tlv320aic23,
};

static struct platform_device *eukrea_tlv320_snd_device;

static int __init eukrea_tlv320_init(void)
{
	int ret;

	if (!machine_is_eukrea_cpuimx27() && !machine_is_eukrea_cpuimx25sd()
		&& !machine_is_eukrea_cpuimx35sd())
		/* return happy. We might run on a totally different machine */
		return 0;

	eukrea_tlv320_snd_device = platform_device_alloc("soc-audio", -1);
	if (!eukrea_tlv320_snd_device)
		return -ENOMEM;

	eukrea_tlv320_dai.cpu_dai = &imx_ssi_pcm_dai[0];

	platform_set_drvdata(eukrea_tlv320_snd_device, &eukrea_tlv320_snd_devdata);
	eukrea_tlv320_snd_devdata.dev = &eukrea_tlv320_snd_device->dev;
	ret = platform_device_add(eukrea_tlv320_snd_device);

	if (ret) {
		printk(KERN_ERR "ASoC: Platform device allocation failed\n");
		platform_device_put(eukrea_tlv320_snd_device);
	}

	return ret;
}

static void __exit eukrea_tlv320_exit(void)
{
	platform_device_unregister(eukrea_tlv320_snd_device);
}

module_init(eukrea_tlv320_init);
module_exit(eukrea_tlv320_exit);

MODULE_AUTHOR("Eric Bénard <eric@eukrea.com>");
MODULE_DESCRIPTION("CPUIMX ALSA SoC driver");
MODULE_LICENSE("GPL");
