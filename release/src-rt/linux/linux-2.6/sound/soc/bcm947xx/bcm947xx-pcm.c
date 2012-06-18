/*
 * ALSA PCM Interface for the Broadcom BCM947XX family of SOCs
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: bcm947xx-pcm.c,v 1.2 2009/11/12 22:25:16 Exp $
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>

#include <asm/io.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

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

#include "bcm947xx-i2s.h"
#include "bcm947xx-pcm.h"


/* Be careful here... turning on prints can break everything, if you start seeing FIFO underflows
 * then it might be due to excessive printing
 */
#define BCM947XX_PCM_DEBUG 0
#if BCM947XX_PCM_DEBUG
#define DBG(x...) printk(KERN_ERR x)
#else
#define DBG(x...)
#endif


static const struct snd_pcm_hardware bcm947xx_pcm_hardware = {
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
	/*				  SNDRV_PCM_INFO_BLOCK_TRANSFER | */
				  SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_RESUME,
	.formats		= SNDRV_PCM_FMTBIT_U16_LE |
				  SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_S20_3LE |
				  SNDRV_PCM_FMTBIT_S24_LE |
				  SNDRV_PCM_FMTBIT_S24_3LE |
				  SNDRV_PCM_FMTBIT_S32_LE,
	.channels_min		= 2,
	.channels_max		= 2,
	.period_bytes_min	= 32,
	.period_bytes_max	= 4096,
	.periods_min		= 2,
	.periods_max		= 64,
	.buffer_bytes_max	= 128 * 1024,
	.fifo_size		= 128,
};

struct bcm947xx_runtime_data {
	spinlock_t lock;
	bcm947xx_i2s_info_t *snd_bcm;
	unsigned int dma_loaded;
	unsigned int dma_limit;
	unsigned int dma_period;
	dma_addr_t dma_start;
	dma_addr_t dma_pos;
	dma_addr_t dma_end;
	uint state;
	hnddma_t	*di[1];		/* hnddma handles, per fifo */
};


#if BCM947XX_PCM_DEBUG
void
prhex(const char *msg, uchar *buf, uint nbytes)
{
	char line[128], *p;
	int len = sizeof(line);
	int nchar;
	uint i;

	if (msg && (msg[0] != '\0'))
		printf("%s: @%p\n", msg, buf);

	p = line;
	for (i = 0; i < nbytes; i++) {
		if (i % 16 == 0) {
			nchar = snprintf(p, len, "  %04d: ", i);	/* line prefix */
			p += nchar;
			len -= nchar;
		}
		if (len > 0) {
			nchar = snprintf(p, len, "%02x ", buf[i]);
			p += nchar;
			len -= nchar;
		}

		if (i % 16 == 15) {
			printf("%s\n", line);		/* flush line */
			p = line;
			len = sizeof(line);
		}
	}

	/* flush last partial line */
	if (p != line)
		printf("%s\n", line);
}
#endif /* BCM947XX_PCM_DEBUG */

static void bcm947xx_pcm_enqueue(struct snd_pcm_substream *substream)
{
	struct bcm947xx_runtime_data *brtd = substream->runtime->private_data;
	dma_addr_t pos = brtd->dma_pos;
	int ret;

	while (brtd->dma_loaded < brtd->dma_limit) {
		unsigned long len = brtd->dma_period;

		if ((pos & ~0xFFF) != (((pos+len - 1) & ~0xFFF))) {
			len = ((pos+len) & ~0xFFF) - pos;
		}

		if ((pos + len) > brtd->dma_end) {
			len  = brtd->dma_end - pos;
		}

		ret = dma_txunframed(snd_bcm->di[0], (void *)pos, len, TRUE);

		if (ret == 0) {
			pos += len;
			brtd->dma_loaded++;
			if (pos >= brtd->dma_end)
				pos = brtd->dma_start;
		} else
			break;
	}

	brtd->dma_pos = pos;
}


struct snd_pcm_substream *my_stream;


irqreturn_t bcm947xx_i2s_isr(int irq, void *devid)
{
	uint32 intstatus = R_REG(snd_bcm->osh, &snd_bcm->regs->intstatus);
#if BCM947XX_PCM_DEBUG
	uint32 intmask = R_REG(snd_bcm->osh, &snd_bcm->regs->intmask);
#endif
	uint32 intstatus_new = 0;
	uint32 int_errmask = I2S_INT_DESCERR | I2S_INT_DATAERR | I2S_INT_DESC_PROTO_ERR |
	        I2S_INT_RCVFIFO_OFLOW | I2S_INT_XMTFIFO_UFLOW | I2S_INT_SPDIF_PAR_ERR;
	struct bcm947xx_runtime_data *brtd = my_stream->runtime->private_data;

	if (intstatus & I2S_INT_XMT_INT) {
		/* reclaim descriptors that have been TX'd */
		dma_getnexttxp(snd_bcm->di[0], HNDDMA_RANGE_TRANSMITTED);

		/* clear this bit by writing a "1" back, we've serviced this */
		intstatus_new |= I2S_INT_XMT_INT;
	}

	if (intstatus & int_errmask) {
		DBG("\n\n%s: Turning off all interrupts due to error\n", __FUNCTION__);
		DBG("%s: intstatus 0x%x intmask 0x%x\n", __FUNCTION__, intstatus, intmask);


		/* something bad happened, turn off all interrupts */
		W_REG(snd_bcm->osh, &snd_bcm->regs->intmask, 0);
	}

	snd_pcm_period_elapsed(my_stream);

	spin_lock(&brtd->lock);
	brtd->dma_loaded--;
	if (brtd->state & BCM_I2S_RUNNING) {
		bcm947xx_pcm_enqueue(my_stream);
	}
	spin_unlock(&brtd->lock);

	W_REG(snd_bcm->osh, &snd_bcm->regs->intstatus, intstatus_new);

	return IRQ_RETVAL(intstatus);
}


static int bcm947xx_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct bcm947xx_runtime_data *brtd;

	DBG("%s\n", __FUNCTION__);

	snd_soc_set_runtime_hwparams(substream, &bcm947xx_pcm_hardware);

	brtd = kzalloc(sizeof(struct bcm947xx_runtime_data), GFP_KERNEL);
	if (brtd == NULL) {
		return -ENOMEM;
	}
	brtd->snd_bcm = snd_bcm;

	spin_lock_init(&brtd->lock);

	runtime->private_data = brtd;

	/* probably should put this somewhere else, after setting up isr ??? */
	dma_txreset(snd_bcm->di[0]);
	dma_txinit(snd_bcm->di[0]);

#if BCM947XX_PCM_DEBUG
	DBG("%s: i2s devcontrol 0x%x devstatus 0x%x\n", __FUNCTION__,
	    R_REG(snd_bcm->osh, &snd_bcm->regs->devcontrol),
	    R_REG(snd_bcm->osh, &snd_bcm->regs->devstatus));
	DBG("%s: i2s intstatus 0x%x intmask 0x%x\n", __FUNCTION__,
	    R_REG(snd_bcm->osh, &snd_bcm->regs->intstatus),
	    R_REG(snd_bcm->osh, &snd_bcm->regs->intmask));
	DBG("%s: i2s control 0x%x\n", __FUNCTION__,
	    R_REG(snd_bcm->osh, &snd_bcm->regs->i2scontrol));
	DBG("%s: i2s clkdivider 0x%x txplayth 0x%x\n", __FUNCTION__,
	    R_REG(snd_bcm->osh, &snd_bcm->regs->clkdivider),
	    R_REG(snd_bcm->osh, &snd_bcm->regs->txplayth));
	DBG("%s: i2s stxctrl 0x%x\n", __FUNCTION__,
	    R_REG(snd_bcm->osh, &snd_bcm->regs->stxctrl));

	{
	uint32 temp;
	temp = R_REG(snd_bcm->osh, &snd_bcm->regs->fifocounter);
	DBG("%s: i2s txcnt 0x%x rxcnt 0x%x\n", __FUNCTION__,
	    (temp & I2S_FC_TX_CNT_MASK)>> I2S_FC_TX_CNT_SHIFT,
	    (temp & I2S_FC_RX_CNT_MASK)>> I2S_FC_RX_CNT_SHIFT);
	}
#endif


	return 0;
}

static int bcm947xx_pcm_close(struct snd_pcm_substream *substream)
{
	struct bcm947xx_runtime_data *brtd = substream->runtime->private_data;

	/* Turn off interrupts... */
	W_REG(snd_bcm->osh, &snd_bcm->regs->intmask,
	      R_REG(snd_bcm->osh, &snd_bcm->regs->intmask) & ~I2S_INT_XMT_INT);

#if BCM947XX_PCM_DEBUG
	{
		/* dump dma rings to console */
#if !defined(FIFOERROR_DUMP_SIZE)
#define FIFOERROR_DUMP_SIZE 8192
#endif
		char *tmp;
		struct bcmstrbuf b;
		if (snd_bcm->di[0] && (tmp = MALLOC(snd_bcm->osh, FIFOERROR_DUMP_SIZE))) {
			bcm_binit(&b, tmp, FIFOERROR_DUMP_SIZE);
			dma_dump(snd_bcm->di[0], &b, TRUE);
			printbig(tmp);
			MFREE(snd_bcm->osh, tmp, FIFOERROR_DUMP_SIZE);
		}
	}
#endif /* BCM947XX_PCM_DEBUG */

	/* reclaim all descriptors */
	dma_txreclaim(snd_bcm->di[0], HNDDMA_RANGE_ALL);

	if (brtd)
		kfree(brtd);
	else
		DBG("%s: called with brtd == NULL\n", __FUNCTION__);

	return 0;
}

static int bcm947xx_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct bcm947xx_runtime_data *brtd = runtime->private_data;
	//struct snd_soc_pcm_runtime *rtd = substream->private_data;
	//struct bcm947xx_pcm_dma_params *dma = rtd->dai->cpu_dai->dma_data;
	unsigned long totbytes = params_buffer_bytes(params);

	int ret = 0;

#if BCM947XX_PCM_DEBUG
	size_t buffer_size = params_buffer_size(params);
	size_t buffer_bytes = params_buffer_bytes(params);
	size_t period_size = params_period_size(params);
	size_t period_bytes = params_period_bytes(params);
	size_t periods = params_periods(params);
	size_t tick_time = params_tick_time(params);

	DBG("%s: hw.periods_min %d dma_addr %p dma_bytes %d\n",
	    __FUNCTION__, runtime->hw.periods_min, (void *)runtime->dma_addr, runtime->dma_bytes);
	DBG("%s: buffer_size 0x%x buffer_bytes 0x%x\n", __FUNCTION__, buffer_size, buffer_bytes);
	DBG("%s: period_size 0x%x period_bytes 0x%x\n", __FUNCTION__, period_size, period_bytes);
	DBG("%s: periods 0x%x tick_time0x%x\n", __FUNCTION__, periods, tick_time);
#endif

	my_stream = substream;

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = totbytes;

	spin_lock_irq(&brtd->lock);
	brtd->dma_limit = runtime->hw.periods_min;
	brtd->dma_period = params_period_bytes(params);
	/* Virtual address of our runtime buffer */
	brtd->dma_start = (dma_addr_t)runtime->dma_area;
	brtd->dma_loaded = 0;
	brtd->dma_pos = brtd->dma_start;
	brtd->dma_end = brtd->dma_start + totbytes;
	spin_lock(&brtd->lock);
	spin_unlock_irq(&brtd->lock);

	return ret;
}


static int bcm947xx_pcm_hw_free(struct snd_pcm_substream *substream)
{
	//DBG("%s\n", __FUNCTION__);
	snd_pcm_set_runtime_buffer(substream, NULL);

	my_stream = NULL;

	return 0;
}

static int bcm947xx_pcm_prepare(struct snd_pcm_substream *substream)
{
	uint32 intmask = R_REG(snd_bcm->osh, &snd_bcm->regs->intmask);
	int ret = 0;

	/* Turn on Tx interrupt */
	W_REG(snd_bcm->osh, &snd_bcm->regs->intmask, intmask | I2S_INT_XMT_INT);

	/* enqueue dma buffers */
	bcm947xx_pcm_enqueue(substream);

	return ret;
}

static int bcm947xx_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct bcm947xx_runtime_data *brtd = substream->runtime->private_data;
	int ret = 0;

	//DBG("%s w/cmd %d\n", __FUNCTION__, cmd);

	spin_lock(&brtd->lock);

	switch (cmd) {

	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		brtd->state |= BCM_I2S_RUNNING;
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		brtd->state &= ~BCM_I2S_RUNNING;
		break;

	default:
		ret = -EINVAL;
		break;
	}
	spin_unlock(&brtd->lock);

	return ret;
}


static int
bcm947xx_dma_getposition(dma_addr_t *src, dma_addr_t *dst)
{
	if (src) {
		*src = (dma_addr_t)dma_getpos(snd_bcm->di[0], DMA_TX);
	} else if (dst) {
		*dst = (dma_addr_t)dma_getpos(snd_bcm->di[0], DMA_RX);
	}

	return 0;
}

static snd_pcm_uframes_t
bcm947xx_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct bcm947xx_runtime_data *brtd = runtime->private_data;
	unsigned long res;
	dma_addr_t pos = 0;

	spin_lock(&brtd->lock);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		bcm947xx_dma_getposition(NULL, &pos);
	} else {
		bcm947xx_dma_getposition(&pos, NULL);
	}

	if ((void *)pos == NULL)
		res = 0; /* DMA not running? */
	else {
		res = pos - brtd->dma_start;
		DBG("%s: pos %p - dma_start %p = 0x%x\n", __FUNCTION__, pos, brtd->dma_start, res);
	}

	spin_unlock(&brtd->lock);

	return bytes_to_frames(substream->runtime, res);
}


/* Currently unused... memory mapping is automatically done in the dma code */
static int bcm947xx_pcm_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	DBG("Entered %s\n", __FUNCTION__);
	return 0;
}


struct snd_pcm_ops bcm947xx_pcm_ops = {
	.open		= bcm947xx_pcm_open,
	.close		= bcm947xx_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= bcm947xx_pcm_hw_params,
	.hw_free	= bcm947xx_pcm_hw_free,
	.prepare	= bcm947xx_pcm_prepare,
	.trigger	= bcm947xx_pcm_trigger,
	.pointer	= bcm947xx_pcm_pointer,
	.mmap		= bcm947xx_pcm_mmap,
};


static int bcm947xx_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{

	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = bcm947xx_pcm_hardware.buffer_bytes_max;

	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = kmalloc(size, GFP_ATOMIC);
	DBG("%s: size %d @ 0x%p\n", __FUNCTION__, size, buf->area);

	if (!buf->area) {
		DBG("%s: dma_alloc failed\n", __FUNCTION__);
		return -ENOMEM;
	}
	buf->bytes = size;

	return 0;
}

static void bcm947xx_pcm_free(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	DBG("%s\n", __FUNCTION__);

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		kfree(buf->area);
		buf->area = NULL;
	}

	free_irq(snd_bcm->irq, snd_bcm);

}


static u64 bcm947xx_pcm_dmamask = DMA_32BIT_MASK;

int bcm947xx_pcm_new(struct snd_card *card, struct snd_soc_codec_dai *dai,
	struct snd_pcm *pcm)
{
	int ret = 0;

	DBG("%s\n", __FUNCTION__);

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &bcm947xx_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_32BIT_MASK;

	if (dai->playback.channels_min) {
		ret = bcm947xx_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}

	if ((request_irq(snd_bcm->irq,
	                 bcm947xx_i2s_isr, IRQF_SHARED, "i2s", snd_bcm)) < 0) {
		DBG("%s: request_irq failure\n", __FUNCTION__);
	}


 out:
	return ret;
}


struct snd_soc_platform bcm947xx_soc_platform = {
	.name		= "bcm947xx-audio",
	.pcm_ops 	= &bcm947xx_pcm_ops,
	.pcm_new	= bcm947xx_pcm_new,
	.pcm_free	= bcm947xx_pcm_free,
};

EXPORT_SYMBOL_GPL(bcm947xx_soc_platform);


MODULE_LICENSE("GPL");
/* MODULE_AUTHOR(""); */
MODULE_DESCRIPTION("BCM947XX PCM module");
