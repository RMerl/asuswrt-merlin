/*
 * Generic TXx9 ACLC platform driver
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include "txx9aclc.h"

static const struct snd_pcm_hardware txx9aclc_pcm_hardware = {
	/*
	 * REVISIT: SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_MMAP_VALID
	 * needs more works for noncoherent MIPS.
	 */
	.info		  = SNDRV_PCM_INFO_INTERLEAVED |
			    SNDRV_PCM_INFO_BATCH |
			    SNDRV_PCM_INFO_PAUSE,
#ifdef __BIG_ENDIAN
	.formats	  = SNDRV_PCM_FMTBIT_S16_BE,
#else
	.formats	  = SNDRV_PCM_FMTBIT_S16_LE,
#endif
	.period_bytes_min = 1024,
	.period_bytes_max = 8 * 1024,
	.periods_min	  = 2,
	.periods_max	  = 4096,
	.buffer_bytes_max = 32 * 1024,
};

static int txx9aclc_pcm_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct txx9aclc_dmadata *dmadata = runtime->private_data;
	int ret;

	ret = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(params));
	if (ret < 0)
		return ret;

	dev_dbg(socdev->dev,
		"runtime->dma_area = %#lx dma_addr = %#lx dma_bytes = %zd "
		"runtime->min_align %ld\n",
		(unsigned long)runtime->dma_area,
		(unsigned long)runtime->dma_addr, runtime->dma_bytes,
		runtime->min_align);
	dev_dbg(socdev->dev,
		"periods %d period_bytes %d stream %d\n",
		params_periods(params), params_period_bytes(params),
		substream->stream);

	dmadata->substream = substream;
	dmadata->pos = 0;
	return 0;
}

static int txx9aclc_pcm_hw_free(struct snd_pcm_substream *substream)
{
	return snd_pcm_lib_free_pages(substream);
}

static int txx9aclc_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct txx9aclc_dmadata *dmadata = runtime->private_data;

	dmadata->dma_addr = runtime->dma_addr;
	dmadata->buffer_bytes = snd_pcm_lib_buffer_bytes(substream);
	dmadata->period_bytes = snd_pcm_lib_period_bytes(substream);

	if (dmadata->buffer_bytes == dmadata->period_bytes) {
		dmadata->frag_bytes = dmadata->period_bytes >> 1;
		dmadata->frags = 2;
	} else {
		dmadata->frag_bytes = dmadata->period_bytes;
		dmadata->frags = dmadata->buffer_bytes / dmadata->period_bytes;
	}
	dmadata->frag_count = 0;
	dmadata->pos = 0;
	return 0;
}

static void txx9aclc_dma_complete(void *arg)
{
	struct txx9aclc_dmadata *dmadata = arg;
	unsigned long flags;

	/* dma completion handler cannot submit new operations */
	spin_lock_irqsave(&dmadata->dma_lock, flags);
	if (dmadata->frag_count >= 0) {
		dmadata->dmacount--;
		BUG_ON(dmadata->dmacount < 0);
		tasklet_schedule(&dmadata->tasklet);
	}
	spin_unlock_irqrestore(&dmadata->dma_lock, flags);
}

static struct dma_async_tx_descriptor *
txx9aclc_dma_submit(struct txx9aclc_dmadata *dmadata, dma_addr_t buf_dma_addr)
{
	struct dma_chan *chan = dmadata->dma_chan;
	struct dma_async_tx_descriptor *desc;
	struct scatterlist sg;

	sg_init_table(&sg, 1);
	sg_set_page(&sg, pfn_to_page(PFN_DOWN(buf_dma_addr)),
		    dmadata->frag_bytes, buf_dma_addr & (PAGE_SIZE - 1));
	sg_dma_address(&sg) = buf_dma_addr;
	desc = chan->device->device_prep_slave_sg(chan, &sg, 1,
		dmadata->substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		DMA_TO_DEVICE : DMA_FROM_DEVICE,
		DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc) {
		dev_err(&chan->dev->device, "cannot prepare slave dma\n");
		return NULL;
	}
	desc->callback = txx9aclc_dma_complete;
	desc->callback_param = dmadata;
	desc->tx_submit(desc);
	return desc;
}

#define NR_DMA_CHAIN		2

static void txx9aclc_dma_tasklet(unsigned long data)
{
	struct txx9aclc_dmadata *dmadata = (struct txx9aclc_dmadata *)data;
	struct dma_chan *chan = dmadata->dma_chan;
	struct dma_async_tx_descriptor *desc;
	struct snd_pcm_substream *substream = dmadata->substream;
	u32 ctlbit = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		ACCTL_AUDODMA : ACCTL_AUDIDMA;
	int i;
	unsigned long flags;

	spin_lock_irqsave(&dmadata->dma_lock, flags);
	if (dmadata->frag_count < 0) {
		struct txx9aclc_soc_device *dev =
			container_of(dmadata, struct txx9aclc_soc_device,
				     dmadata[substream->stream]);
		struct txx9aclc_plat_drvdata *drvdata =
			txx9aclc_get_plat_drvdata(dev);
		void __iomem *base = drvdata->base;

		spin_unlock_irqrestore(&dmadata->dma_lock, flags);
		chan->device->device_control(chan, DMA_TERMINATE_ALL, 0);
		/* first time */
		for (i = 0; i < NR_DMA_CHAIN; i++) {
			desc = txx9aclc_dma_submit(dmadata,
				dmadata->dma_addr + i * dmadata->frag_bytes);
			if (!desc)
				return;
		}
		dmadata->dmacount = NR_DMA_CHAIN;
		chan->device->device_issue_pending(chan);
		spin_lock_irqsave(&dmadata->dma_lock, flags);
		__raw_writel(ctlbit, base + ACCTLEN);
		dmadata->frag_count = NR_DMA_CHAIN % dmadata->frags;
		spin_unlock_irqrestore(&dmadata->dma_lock, flags);
		return;
	}
	BUG_ON(dmadata->dmacount >= NR_DMA_CHAIN);
	while (dmadata->dmacount < NR_DMA_CHAIN) {
		dmadata->dmacount++;
		spin_unlock_irqrestore(&dmadata->dma_lock, flags);
		desc = txx9aclc_dma_submit(dmadata,
			dmadata->dma_addr +
			dmadata->frag_count * dmadata->frag_bytes);
		if (!desc)
			return;
		chan->device->device_issue_pending(chan);

		spin_lock_irqsave(&dmadata->dma_lock, flags);
		dmadata->frag_count++;
		dmadata->frag_count %= dmadata->frags;
		dmadata->pos += dmadata->frag_bytes;
		dmadata->pos %= dmadata->buffer_bytes;
		if ((dmadata->frag_count * dmadata->frag_bytes) %
		    dmadata->period_bytes == 0)
			snd_pcm_period_elapsed(substream);
	}
	spin_unlock_irqrestore(&dmadata->dma_lock, flags);
}

static int txx9aclc_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct txx9aclc_dmadata *dmadata = substream->runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct txx9aclc_soc_device *dev =
		container_of(rtd->socdev, struct txx9aclc_soc_device, soc_dev);
	struct txx9aclc_plat_drvdata *drvdata = txx9aclc_get_plat_drvdata(dev);
	void __iomem *base = drvdata->base;
	unsigned long flags;
	int ret = 0;
	u32 ctlbit = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		ACCTL_AUDODMA : ACCTL_AUDIDMA;

	spin_lock_irqsave(&dmadata->dma_lock, flags);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		dmadata->frag_count = -1;
		tasklet_schedule(&dmadata->tasklet);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		__raw_writel(ctlbit, base + ACCTLDIS);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		__raw_writel(ctlbit, base + ACCTLEN);
		break;
	default:
		ret = -EINVAL;
	}
	spin_unlock_irqrestore(&dmadata->dma_lock, flags);
	return ret;
}

static snd_pcm_uframes_t
txx9aclc_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct txx9aclc_dmadata *dmadata = substream->runtime->private_data;

	return bytes_to_frames(substream->runtime, dmadata->pos);
}

static int txx9aclc_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct txx9aclc_soc_device *dev =
		container_of(rtd->socdev, struct txx9aclc_soc_device, soc_dev);
	struct txx9aclc_dmadata *dmadata = &dev->dmadata[substream->stream];
	int ret;

	ret = snd_soc_set_runtime_hwparams(substream, &txx9aclc_pcm_hardware);
	if (ret)
		return ret;
	/* ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(substream->runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		return ret;
	substream->runtime->private_data = dmadata;
	return 0;
}

static int txx9aclc_pcm_close(struct snd_pcm_substream *substream)
{
	struct txx9aclc_dmadata *dmadata = substream->runtime->private_data;
	struct dma_chan *chan = dmadata->dma_chan;

	dmadata->frag_count = -1;
	chan->device->device_control(chan, DMA_TERMINATE_ALL, 0);
	return 0;
}

static struct snd_pcm_ops txx9aclc_pcm_ops = {
	.open		= txx9aclc_pcm_open,
	.close		= txx9aclc_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= txx9aclc_pcm_hw_params,
	.hw_free	= txx9aclc_pcm_hw_free,
	.prepare	= txx9aclc_pcm_prepare,
	.trigger	= txx9aclc_pcm_trigger,
	.pointer	= txx9aclc_pcm_pointer,
};

static void txx9aclc_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static int txx9aclc_pcm_new(struct snd_card *card, struct snd_soc_dai *dai,
			    struct snd_pcm *pcm)
{
	return snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
		card->dev, 64 * 1024, 4 * 1024 * 1024);
}

static bool filter(struct dma_chan *chan, void *param)
{
	struct txx9aclc_dmadata *dmadata = param;
	char *devname;
	bool found = false;

	devname = kasprintf(GFP_KERNEL, "%s.%d", dmadata->dma_res->name,
		(int)dmadata->dma_res->start);
	if (strcmp(dev_name(chan->device->dev), devname) == 0) {
		chan->private = &dmadata->dma_slave;
		found = true;
	}
	kfree(devname);
	return found;
}

static int txx9aclc_dma_init(struct txx9aclc_soc_device *dev,
			     struct txx9aclc_dmadata *dmadata)
{
	struct txx9aclc_plat_drvdata *drvdata = txx9aclc_get_plat_drvdata(dev);
	struct txx9dmac_slave *ds = &dmadata->dma_slave;
	dma_cap_mask_t mask;

	spin_lock_init(&dmadata->dma_lock);

	ds->reg_width = sizeof(u32);
	if (dmadata->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ds->tx_reg = drvdata->physbase + ACAUDODAT;
		ds->rx_reg = 0;
	} else {
		ds->tx_reg = 0;
		ds->rx_reg = drvdata->physbase + ACAUDIDAT;
	}

	/* Try to grab a DMA channel */
	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	dmadata->dma_chan = dma_request_channel(mask, filter, dmadata);
	if (!dmadata->dma_chan) {
		dev_err(dev->soc_dev.dev,
			"DMA channel for %s is not available\n",
			dmadata->stream == SNDRV_PCM_STREAM_PLAYBACK ?
			"playback" : "capture");
		return -EBUSY;
	}
	tasklet_init(&dmadata->tasklet, txx9aclc_dma_tasklet,
		     (unsigned long)dmadata);
	return 0;
}

static int txx9aclc_pcm_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct txx9aclc_soc_device *dev =
		container_of(socdev, struct txx9aclc_soc_device, soc_dev);
	struct resource *r;
	int i;
	int ret;

	dev->dmadata[0].stream = SNDRV_PCM_STREAM_PLAYBACK;
	dev->dmadata[1].stream = SNDRV_PCM_STREAM_CAPTURE;
	for (i = 0; i < 2; i++) {
		r = platform_get_resource(dev->aclc_pdev, IORESOURCE_DMA, i);
		if (!r) {
			ret = -EBUSY;
			goto exit;
		}
		dev->dmadata[i].dma_res = r;
		ret = txx9aclc_dma_init(dev, &dev->dmadata[i]);
		if (ret)
			goto exit;
	}
	return 0;

exit:
	for (i = 0; i < 2; i++) {
		if (dev->dmadata[i].dma_chan)
			dma_release_channel(dev->dmadata[i].dma_chan);
		dev->dmadata[i].dma_chan = NULL;
	}
	return ret;
}

static int txx9aclc_pcm_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct txx9aclc_soc_device *dev =
		container_of(socdev, struct txx9aclc_soc_device, soc_dev);
	struct txx9aclc_plat_drvdata *drvdata = txx9aclc_get_plat_drvdata(dev);
	void __iomem *base = drvdata->base;
	int i;

	/* disable all FIFO DMAs */
	__raw_writel(ACCTL_AUDODMA | ACCTL_AUDIDMA, base + ACCTLDIS);
	/* dummy R/W to clear pending DMAREQ if any */
	__raw_writel(__raw_readl(base + ACAUDIDAT), base + ACAUDODAT);

	for (i = 0; i < 2; i++) {
		struct txx9aclc_dmadata *dmadata = &dev->dmadata[i];
		struct dma_chan *chan = dmadata->dma_chan;
		if (chan) {
			dmadata->frag_count = -1;
			chan->device->device_control(chan,
						     DMA_TERMINATE_ALL, 0);
			dma_release_channel(chan);
		}
		dev->dmadata[i].dma_chan = NULL;
	}
	return 0;
}

struct snd_soc_platform txx9aclc_soc_platform = {
	.name		= "txx9aclc-audio",
	.probe		= txx9aclc_pcm_probe,
	.remove		= txx9aclc_pcm_remove,
	.pcm_ops 	= &txx9aclc_pcm_ops,
	.pcm_new	= txx9aclc_pcm_new,
	.pcm_free	= txx9aclc_pcm_free_dma_buffers,
};
EXPORT_SYMBOL_GPL(txx9aclc_soc_platform);

static int __init txx9aclc_soc_platform_init(void)
{
	return snd_soc_register_platform(&txx9aclc_soc_platform);
}

static void __exit txx9aclc_soc_platform_exit(void)
{
	snd_soc_unregister_platform(&txx9aclc_soc_platform);
}

module_init(txx9aclc_soc_platform_init);
module_exit(txx9aclc_soc_platform_exit);

MODULE_AUTHOR("Atsushi Nemoto <anemo@mba.ocn.ne.jp>");
MODULE_DESCRIPTION("TXx9 ACLC Audio DMA driver");
MODULE_LICENSE("GPL");
