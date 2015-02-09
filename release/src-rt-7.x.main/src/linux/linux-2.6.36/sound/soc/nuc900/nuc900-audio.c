/*
 * Copyright (c) 2010 Nuvoton technology corporation.
 *
 * Wan ZongShun <mcuos.com@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;version 2 of the License.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include "../codecs/ac97.h"
#include "nuc900-audio.h"

static struct snd_soc_dai_link nuc900evb_ac97_dai = {
	.name		= "AC97",
	.stream_name	= "AC97 HiFi",
	.cpu_dai	= &nuc900_ac97_dai,
	.codec_dai	= &ac97_dai,
};

static struct snd_soc_card nuc900evb_audio_machine = {
	.name		= "NUC900EVB_AC97",
	.dai_link	= &nuc900evb_ac97_dai,
	.num_links	= 1,
	.platform	= &nuc900_soc_platform,
};

static struct snd_soc_device nuc900evb_ac97_devdata = {
	.card		= &nuc900evb_audio_machine,
	.codec_dev	= &soc_codec_dev_ac97,
};

static struct platform_device *nuc900evb_asoc_dev;

static int __init nuc900evb_audio_init(void)
{
	int ret;

	ret = -ENOMEM;
	nuc900evb_asoc_dev = platform_device_alloc("soc-audio", -1);
	if (!nuc900evb_asoc_dev)
		goto out;

	/* nuc900 board audio device */
	platform_set_drvdata(nuc900evb_asoc_dev, &nuc900evb_ac97_devdata);

	nuc900evb_ac97_devdata.dev = &nuc900evb_asoc_dev->dev;
	ret = platform_device_add(nuc900evb_asoc_dev);

	if (ret) {
		platform_device_put(nuc900evb_asoc_dev);
		nuc900evb_asoc_dev = NULL;
	}

out:
	return ret;
}

static void __exit nuc900evb_audio_exit(void)
{
	platform_device_unregister(nuc900evb_asoc_dev);
}

module_init(nuc900evb_audio_init);
module_exit(nuc900evb_audio_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("NUC900 Series ASoC audio support");
MODULE_AUTHOR("Wan ZongShun");
