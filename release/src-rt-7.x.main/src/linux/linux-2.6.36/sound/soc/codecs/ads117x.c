/*
 * ads117x.c  --  Driver for ads1174/8 ADC chips
 *
 * Copyright 2009 ShotSpotter Inc.
 * Author: Graeme Gregory <gg@slimlogic.co.uk>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include "ads117x.h"

#define ADS117X_RATES (SNDRV_PCM_RATE_8000_48000)

#define ADS117X_FORMATS (SNDRV_PCM_FMTBIT_S16_LE)

struct snd_soc_dai ads117x_dai = {
/* ADC */
	.name = "ADS117X ADC",
	.id = 1,
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 32,
		.rates = ADS117X_RATES,
		.formats = ADS117X_FORMATS,},
};
EXPORT_SYMBOL_GPL(ads117x_dai);

static int ads117x_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	int ret;

	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (codec == NULL)
		return -ENOMEM;

	socdev->card->codec = codec;
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);
	codec->name = "ADS117X";
	codec->owner = THIS_MODULE;
	codec->dai = &ads117x_dai;
	codec->num_dai = 1;

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		printk(KERN_ERR "ads117x: failed to create pcms\n");
		kfree(codec);
		return ret;
	}

	return 0;
}

static int ads117x_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	snd_soc_free_pcms(socdev);
	kfree(codec);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_ads117x = {
	.probe =	ads117x_probe,
	.remove =	ads117x_remove,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_ads117x);

static __devinit int ads117x_platform_probe(struct platform_device *pdev)
{
	ads117x_dai.dev = &pdev->dev;
	return snd_soc_register_dai(&ads117x_dai);
}

static int __devexit ads117x_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&ads117x_dai);
	return 0;
}

static struct platform_driver ads117x_codec_driver = {
	.driver = {
			.name = "ads117x",
			.owner = THIS_MODULE,
	},

	.probe = ads117x_platform_probe,
	.remove = __devexit_p(ads117x_platform_remove),
};

static int __init ads117x_init(void)
{
	return platform_driver_register(&ads117x_codec_driver);
}
module_init(ads117x_init);

static void __exit ads117x_exit(void)
{
	platform_driver_unregister(&ads117x_codec_driver);
}
module_exit(ads117x_exit);

MODULE_DESCRIPTION("ASoC ads117x driver");
MODULE_AUTHOR("Graeme Gregory");
MODULE_LICENSE("GPL");
