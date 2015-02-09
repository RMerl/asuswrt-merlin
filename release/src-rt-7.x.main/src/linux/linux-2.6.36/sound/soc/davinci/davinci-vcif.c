/*
 * ALSA SoC Voice Codec Interface for TI DAVINCI processor
 *
 * Copyright (C) 2010 Texas Instruments.
 *
 * Author: Miguel Aguilar <miguel.aguilar@ridgerun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/mfd/davinci_voicecodec.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include "davinci-pcm.h"
#include "davinci-i2s.h"
#include "davinci-vcif.h"

#define MOD_REG_BIT(val, mask, set) do { \
	if (set) { \
		val |= mask; \
	} else { \
		val &= ~mask; \
	} \
} while (0)

struct davinci_vcif_dev {
	struct davinci_vc *davinci_vc;
	struct davinci_pcm_dma_params	dma_params[2];
};

static void davinci_vcif_start(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct davinci_vcif_dev *davinci_vcif_dev =
					rtd->dai->cpu_dai->private_data;
	struct davinci_vc *davinci_vc = davinci_vcif_dev->davinci_vc;
	u32 w;

	/* Start the sample generator and enable transmitter/receiver */
	w = readl(davinci_vc->base + DAVINCI_VC_CTRL);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		MOD_REG_BIT(w, DAVINCI_VC_CTRL_RSTDAC, 1);
	else
		MOD_REG_BIT(w, DAVINCI_VC_CTRL_RSTADC, 1);

	writel(w, davinci_vc->base + DAVINCI_VC_CTRL);
}

static void davinci_vcif_stop(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct davinci_vcif_dev *davinci_vcif_dev =
					rtd->dai->cpu_dai->private_data;
	struct davinci_vc *davinci_vc = davinci_vcif_dev->davinci_vc;
	u32 w;

	/* Reset transmitter/receiver and sample rate/frame sync generators */
	w = readl(davinci_vc->base + DAVINCI_VC_CTRL);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		MOD_REG_BIT(w, DAVINCI_VC_CTRL_RSTDAC, 0);
	else
		MOD_REG_BIT(w, DAVINCI_VC_CTRL_RSTADC, 0);

	writel(w, davinci_vc->base + DAVINCI_VC_CTRL);
}

static int davinci_vcif_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct davinci_vcif_dev *davinci_vcif_dev = dai->private_data;
	struct davinci_vc *davinci_vc = davinci_vcif_dev->davinci_vc;
	struct davinci_pcm_dma_params *dma_params =
			&davinci_vcif_dev->dma_params[substream->stream];
	u32 w;

	/* Restart the codec before setup */
	davinci_vcif_stop(substream);
	davinci_vcif_start(substream);

	/* General line settings */
	writel(DAVINCI_VC_CTRL_MASK, davinci_vc->base + DAVINCI_VC_CTRL);

	writel(DAVINCI_VC_INT_MASK, davinci_vc->base + DAVINCI_VC_INTCLR);

	writel(DAVINCI_VC_INT_MASK, davinci_vc->base + DAVINCI_VC_INTEN);

	w = readl(davinci_vc->base + DAVINCI_VC_CTRL);

	/* Determine xfer data type */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U8:
		dma_params->data_type = 0;

		MOD_REG_BIT(w, DAVINCI_VC_CTRL_RD_BITS_8 |
			    DAVINCI_VC_CTRL_RD_UNSIGNED |
			    DAVINCI_VC_CTRL_WD_BITS_8 |
			    DAVINCI_VC_CTRL_WD_UNSIGNED, 1);
		break;
	case SNDRV_PCM_FORMAT_S8:
		dma_params->data_type = 1;

		MOD_REG_BIT(w, DAVINCI_VC_CTRL_RD_BITS_8 |
			    DAVINCI_VC_CTRL_WD_BITS_8, 1);

		MOD_REG_BIT(w, DAVINCI_VC_CTRL_RD_UNSIGNED |
			    DAVINCI_VC_CTRL_WD_UNSIGNED, 0);
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		dma_params->data_type = 2;

		MOD_REG_BIT(w, DAVINCI_VC_CTRL_RD_BITS_8 |
			    DAVINCI_VC_CTRL_RD_UNSIGNED |
			    DAVINCI_VC_CTRL_WD_BITS_8 |
			    DAVINCI_VC_CTRL_WD_UNSIGNED, 0);
		break;
	default:
		printk(KERN_WARNING "davinci-vcif: unsupported PCM format");
		return -EINVAL;
	}

	dma_params->acnt  = dma_params->data_type;

	writel(w, davinci_vc->base + DAVINCI_VC_CTRL);

	return 0;
}

static int davinci_vcif_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		davinci_vcif_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		davinci_vcif_stop(substream);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

#define DAVINCI_VCIF_RATES	SNDRV_PCM_RATE_8000_48000

static struct snd_soc_dai_ops davinci_vcif_dai_ops = {
	.trigger	= davinci_vcif_trigger,
	.hw_params	= davinci_vcif_hw_params,
};

struct snd_soc_dai davinci_vcif_dai = {
	.name = "davinci-vcif",
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = DAVINCI_VCIF_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = DAVINCI_VCIF_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.ops = &davinci_vcif_dai_ops,

};
EXPORT_SYMBOL_GPL(davinci_vcif_dai);

static int davinci_vcif_probe(struct platform_device *pdev)
{
	struct davinci_vc *davinci_vc = platform_get_drvdata(pdev);
	struct davinci_vcif_dev *davinci_vcif_dev;
	int ret;

	davinci_vcif_dev = kzalloc(sizeof(struct davinci_vcif_dev), GFP_KERNEL);
	if (!davinci_vcif_dev) {
		dev_dbg(&pdev->dev,
			"could not allocate memory for private data\n");
		return -ENOMEM;
	}

	/* DMA tx params */
	davinci_vcif_dev->davinci_vc = davinci_vc;
	davinci_vcif_dev->dma_params[SNDRV_PCM_STREAM_PLAYBACK].channel =
					davinci_vc->davinci_vcif.dma_tx_channel;
	davinci_vcif_dev->dma_params[SNDRV_PCM_STREAM_PLAYBACK].dma_addr =
					davinci_vc->davinci_vcif.dma_tx_addr;

	/* DMA rx params */
	davinci_vcif_dev->dma_params[SNDRV_PCM_STREAM_CAPTURE].channel =
					davinci_vc->davinci_vcif.dma_rx_channel;
	davinci_vcif_dev->dma_params[SNDRV_PCM_STREAM_CAPTURE].dma_addr =
					davinci_vc->davinci_vcif.dma_rx_addr;

	davinci_vcif_dai.dev = &pdev->dev;
	davinci_vcif_dai.capture.dma_data = davinci_vcif_dev->dma_params;
	davinci_vcif_dai.playback.dma_data = davinci_vcif_dev->dma_params;
	davinci_vcif_dai.private_data = davinci_vcif_dev;

	ret = snd_soc_register_dai(&davinci_vcif_dai);
	if (ret != 0) {
		dev_err(&pdev->dev, "could not register dai\n");
		goto fail;
	}

	return 0;

fail:
	kfree(davinci_vcif_dev);

	return ret;
}

static int davinci_vcif_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&davinci_vcif_dai);

	return 0;
}

static struct platform_driver davinci_vcif_driver = {
	.probe		= davinci_vcif_probe,
	.remove		= davinci_vcif_remove,
	.driver		= {
		.name	= "davinci_vcif",
		.owner	= THIS_MODULE,
	},
};

static int __init davinci_vcif_init(void)
{
	return platform_driver_probe(&davinci_vcif_driver, davinci_vcif_probe);
}
module_init(davinci_vcif_init);

static void __exit davinci_vcif_exit(void)
{
	platform_driver_unregister(&davinci_vcif_driver);
}
module_exit(davinci_vcif_exit);

MODULE_AUTHOR("Miguel Aguilar");
MODULE_DESCRIPTION("Texas Instruments DaVinci ASoC Voice Codec Interface");
MODULE_LICENSE("GPL");
