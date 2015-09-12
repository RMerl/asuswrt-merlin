/*
 * ALSA PCM Interface for the Broadcom BCM947XX family of SOCs
 *
 * Copyright (C) 1999-2015, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: bcm947xx-pcm.c,v 1.2 2009-11-12 22:25:16 $
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>

#include <asm/io.h>

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
#define BCM947XX_PCM_DEBUG_ON 1
#define DBG(x...) printk(KERN_ERR x)
#else
#define BCM947XX_PCM_DEBUG_ON 0
#define DBG(x...) if(0) printk(KERN_ERR x)
#endif

/* Tracing/debugging. */
#define BCM947XX_DUMP_RING_BUFFER_ON_PCM_CLOSE_ON 	0
#define BCM947XX_TRACE_CAPTURE_COPY_ON 				0
#define BCM947XX_TRACE_PLAYBACK_COPY_ON 			0
#define BCM947XX_TRACE_PCM_ENQUEUE_STATE_ON 		0

/* Enable internal I2S SDOUT to SDIN connection (XmtControl.LoopbackEn). */
#define BCM947XX_DMA_LOOPBACK_ENABLED 0

/* HND DMA has an empty slot to count.  ALSA will select the highest possible period
 * count that is a power of 2.  For example, if hnddma is configured for 128 descriptors,
 * only 127 will be available.  If 127 periods is reported to ALSA, then ALSA will choose
 * the nearest power of 2, which is 64 periods.
*/
#define BCM947XX_PLAYBACK_NR_PERIODS_MAX (BCM947XX_NR_DMA_TXDS_MAX/2)
#define BCM947XX_CAPTURE_NR_PERIODS_MAX	(BCM947XX_NR_DMA_RXDS_MAX/2)

#define BCM947XX_PLAYBACK_PERIOD_BYTES_MAX (BCM947XX_DMA_DATA_BYTES_MAX)
#define BCM947XX_CAPTURE_PERIOD_BYTES_MAX (BCM947XX_DMA_DATA_BYTES_MAX - BCM947XX_DMA_RXOFS_BYTES - \
										   BCM947XX_DMA_RXOFS_TAIL_PAD_BYTES)

/* This value is reported to ALSA as the maximum buffer size.  It is the linear
 * buffer range.
*/
#define BCM947XX_PLAYBACK_BUFFER_BYTES_MAX (BCM947XX_PLAYBACK_PERIOD_BYTES_MAX * BCM947XX_PLAYBACK_NR_PERIODS_MAX)
#define BCM947XX_CAPTURE_BUFFER_BYTES_MAX (BCM947XX_CAPTURE_PERIOD_BYTES_MAX * BCM947XX_CAPTURE_NR_PERIODS_MAX)

/* This value is the actual size of the audio buffer which includes any extra headers. The
 * headers fragment the buffer.
*/
#define BCM947XX_PLAYBACK_DMA_BUFFER_BYTES_MAX (BCM947XX_DMA_DATA_BYTES_MAX * BCM947XX_PLAYBACK_NR_PERIODS_MAX)
#define BCM947XX_CAPTURE_DMA_BUFFER_BYTES_MAX (BCM947XX_DMA_DATA_BYTES_MAX * BCM947XX_CAPTURE_NR_PERIODS_MAX)

/* Other h/w related settings. */

#define BCM947XX_PCM_HARDWARE_INFO \
		(SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_PAUSE | \
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_JOINT_DUPLEX)
				  
#define BCM947XX_PCM_HARDWARE_FORMATS \
	    (SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S16_LE | \
	     SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE | \
		 SNDRV_PCM_FMTBIT_S24_3LE |	SNDRV_PCM_FMTBIT_S32_LE)

/* ALSA h/w definitions. */

static const struct snd_pcm_hardware bcm947xx_pcm_hardware_playback = {
	.info				= BCM947XX_PCM_HARDWARE_INFO,
	.formats			= BCM947XX_PCM_HARDWARE_FORMATS,
	.channels_min		= 2,
	.channels_max		= 2,
	.period_bytes_min	= 32,
	.period_bytes_max	= BCM947XX_PLAYBACK_PERIOD_BYTES_MAX,
	.periods_min		= 2,
	.periods_max		= BCM947XX_PLAYBACK_NR_PERIODS_MAX,
	.buffer_bytes_max	= BCM947XX_PLAYBACK_BUFFER_BYTES_MAX,
};

static const struct snd_pcm_hardware bcm947xx_pcm_hardware_capture = {
	.info				= BCM947XX_PCM_HARDWARE_INFO,
	.formats			= BCM947XX_PCM_HARDWARE_FORMATS,
	.channels_min		= 2,
	.channels_max		= 2,
	.period_bytes_min	= 32,
	.period_bytes_max	= BCM947XX_CAPTURE_PERIOD_BYTES_MAX,
	.periods_min		= 2,
	.periods_max		= BCM947XX_CAPTURE_NR_PERIODS_MAX,
	.buffer_bytes_max	= BCM947XX_CAPTURE_BUFFER_BYTES_MAX,
};

struct bcm947xx_runtime_data {
	spinlock_t lock;
	unsigned int dma_loaded;
	unsigned int dma_limit;
	/* Byte length for data payload in one period */
	unsigned int dma_period;
	/* Byte offset to the period data. */
	unsigned int dma_ofs;
	dma_addr_t dma_start;
	dma_addr_t dma_pos;
	dma_addr_t dma_end;
	unsigned long bytes_pending;
	hnddma_t	*di[1];		/* hnddma handles, per fifo */
};

const uint32 intmask_playback 	= I2S_INT_XMT_INT | I2S_INT_XMTFIFO_UFLOW;
const uint32 intmask_capture	= I2S_INT_RCV_INT | I2S_INT_RCVFIFO_OFLOW;

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


/* Acquire lock before calling me. */
static void bcm947xx_pcm_enqueue(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	bcm947xx_i2s_info_t *snd_bcm = rtd->dai->cpu_dai->private_data;
	struct bcm947xx_runtime_data *brtd = substream->runtime->private_data;
	dma_addr_t pos = brtd->dma_pos;
	int ret;

	if (BCM947XX_TRACE_PCM_ENQUEUE_STATE_ON) {
		DBG("%s dma_loaded: %d dma_limit: %d dma_period: %d\n", __FUNCTION__,
			brtd->dma_loaded, brtd->dma_limit, brtd->dma_period);
		DBG("%s: pos %p - dma_start %p - dma_end %p\n", __FUNCTION__,
			(void *)pos, (void *)brtd->dma_start, (void *)brtd->dma_end);
	}
	
	snd_BUG_ON(0 == brtd->bytes_pending);
	
	while (brtd->dma_loaded < brtd->dma_limit) {
		unsigned long len = brtd->dma_period + brtd->dma_ofs;

		if (((pos & ~0xFFF) != (((pos+len - 1) & ~0xFFF)))) {
			/* This shouldn't happen because periods can't exceed 4KB and they
			 * are 4KB aligned.
			*/
			DBG("%s 4KB boundary violation pos=%p len=%lu\n", __FUNCTION__, (void *)pos, len);
			len = ((pos+len) & ~0xFFF) - pos;	   
		}

		/* Clip to end of ring-buffer. */
		if ((pos + len) >= brtd->dma_end) {
			len  = brtd->dma_end - pos;
			snd_BUG_ON(len <= brtd->dma_ofs);
		}

		/* Not enough data to fill a period or DMA page, therefore bail until
		 * more data is written to the audio buffer.
		*/
		if (brtd->bytes_pending + brtd->dma_ofs < len)
			break;

		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			ret = dma_rxfill_unframed(snd_bcm->di[0], (void *)pos, len, TRUE);
		} else {
			ret = dma_txunframed(snd_bcm->di[0], (void *)pos, len, TRUE);
		}

		if (ret == 0) {
			/* Move to next DMA chunk. */
			pos += BCM947XX_DMA_DATA_BYTES_MAX;
			brtd->dma_loaded++;
			brtd->bytes_pending -= len - brtd->dma_ofs;
			if (pos >= brtd->dma_end)
				pos = brtd->dma_start;
		} else {
			DBG("%s dma_%sunframed returned an error\n", __FUNCTION__,
				substream->stream == SNDRV_PCM_STREAM_CAPTURE ? "rxfill_" : "tx");
			break;
		}
	}

	brtd->dma_pos = pos;
//	DBG("%s loaded: %p %lu\n", __FUNCTION__, brtd->dma_pos, brtd->dma_loaded);
}

static void bcm947xx_dma_abort(struct snd_pcm_substream *substream)
{
	if (snd_pcm_running(substream)) {
		unsigned long flags;
		DBG("%s XRUN\n", __FUNCTION__);
		snd_pcm_stream_lock_irqsave(substream, flags);
		snd_pcm_stop(substream, SNDRV_PCM_STATE_XRUN);
		snd_pcm_stream_unlock_irqrestore(substream, flags);
	}
}

static inline struct bcm947xx_runtime_data *bcm947xx_pcm_brtd_from_running_substream(
	struct snd_pcm_substream *substream)
{
	return
		substream &&			/* valid after pcm_new */
		substream->runtime &&	/* valid after pcm_open */
		snd_pcm_running(substream) ? substream->runtime->private_data : NULL;
}

irqreturn_t bcm947xx_i2s_isr(int irq, void *devid)
{
	uint32 intstatus, intmask;
	uint32 intstatus_new = 0;
	uint32 int_errmask = I2S_INT_DESCERR | I2S_INT_DATAERR | I2S_INT_DESC_PROTO_ERR |
	        I2S_INT_SPDIF_PAR_ERR;
	struct snd_pcm *pcm = devid;
	struct snd_soc_pcm_runtime *rtd = pcm->private_data;
	bcm947xx_i2s_info_t *snd_bcm = rtd->dai->cpu_dai->private_data;
	struct snd_pcm_substream *substream;
	struct bcm947xx_runtime_data *brtd;

//	DBG("%s enter\n", __FUNCTION__);

	intstatus = R_REG(snd_bcm->osh, &snd_bcm->regs->intstatus);

	if (BCM947XX_PCM_DEBUG_ON) {
		intmask = R_REG(snd_bcm->osh, &snd_bcm->regs->intmask);
	} else {
		(void)intmask;
	}

//	DBG("%s: intstatus 0x%x intmask 0x%x\n", __FUNCTION__, intstatus, intmask);

	/* Playback. */
	substream = pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	if ((brtd = bcm947xx_pcm_brtd_from_running_substream(substream))) {

		if (intstatus & I2S_INT_XMT_INT) {
			/* reclaim descriptors that have been TX'd */
			spin_lock(&brtd->lock);
			dma_getnexttxp(snd_bcm->di[0], HNDDMA_RANGE_TRANSMITTED);
			spin_unlock(&brtd->lock);

			/* clear this bit by writing a "1" back, we've serviced this */
			intstatus_new |= I2S_INT_XMT_INT;

			snd_pcm_period_elapsed(substream);
		
			spin_lock(&brtd->lock);
			snd_BUG_ON(0 == brtd->dma_loaded);
			brtd->dma_loaded--;
			spin_unlock(&brtd->lock);
		}

		if (intstatus & I2S_INT_XMTFIFO_UFLOW) {
			intstatus_new |= I2S_INT_XMTFIFO_UFLOW;
			bcm947xx_dma_abort(substream);
		}
	}

	/* Capture. */
	substream = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
	if ((brtd = bcm947xx_pcm_brtd_from_running_substream(substream))) {

		if (intstatus & I2S_INT_RCV_INT) {

			spin_lock(&brtd->lock);
			dma_getnextrxp(snd_bcm->di[0], false);
			spin_unlock(&brtd->lock);

			/* clear this bit by writing a "1" back, we've serviced this */
			intstatus_new |= I2S_INT_RCV_INT;

			snd_pcm_period_elapsed(substream);

			spin_lock(&brtd->lock);
			snd_BUG_ON(0 == brtd->dma_loaded);
			brtd->dma_loaded--;
			spin_unlock(&brtd->lock);
		}

		if (intstatus & I2S_INT_RCVFIFO_OFLOW) {
			intstatus_new |= I2S_INT_RCVFIFO_OFLOW;
			bcm947xx_dma_abort(substream);
		}
	}
	
	/* Common.*/
	if (intstatus & int_errmask) {
		DBG("\n\n%s: Turning off all interrupts due to error\n", __FUNCTION__);
		DBG("%s: intstatus 0x%x intmask 0x%x\n", __FUNCTION__, intstatus, intmask);


		/* something bad happened, turn off all interrupts */
		W_REG(snd_bcm->osh, &snd_bcm->regs->intmask, 0);
	}

	/* Acknowledge interrupts. */
	W_REG(snd_bcm->osh, &snd_bcm->regs->intstatus, intstatus_new);

//	DBG("%s exit\n", __FUNCTION__);

	return IRQ_RETVAL(intstatus);
}


static bool bcm947xx_pcm_joint_duplex_settings_equal(bcm947xx_i2s_info_t *snd_bcm,
	struct snd_pcm_hw_params *params)
{
	return snd_bcm->rate == params_rate(params) &&
		   snd_bcm->channels == params_channels(params) &&
		   snd_bcm->sample_bits == hw_param_interval(params, SNDRV_PCM_HW_PARAM_SAMPLE_BITS)->min;
}


static void bcm947xx_pcm_set_joint_duplex_settings(bcm947xx_i2s_info_t *snd_bcm,
	struct snd_pcm_hw_params *params)
{
	snd_bcm->rate = params_rate(params);
	snd_bcm->channels = params_channels(params);
	snd_bcm->sample_bits = hw_param_interval(params, SNDRV_PCM_HW_PARAM_SAMPLE_BITS)->min;
}


static void bcm947xx_pcm_clear_joint_duplex_settings(bcm947xx_i2s_info_t *snd_bcm)
{
	snd_bcm->rate = -1;
	snd_bcm->channels = -1;
	snd_bcm->sample_bits = -1;
}

static int bcm947xx_pcm_set_constraints(struct snd_pcm_runtime *runtime,
	bcm947xx_i2s_info_t *snd_bcm)
{
	int ret;

	/* rate */
	if ((ret = snd_pcm_hw_constraint_minmax(runtime,
					SNDRV_PCM_HW_PARAM_RATE, snd_bcm->rate, snd_bcm->rate)) < 0)
	{
		DBG("%s SNDRV_PCM_HW_PARAM_RATE failed rate=%d\n", __FUNCTION__, snd_bcm->rate);
		goto err;
	}
	/* channels */
	if ((ret = snd_pcm_hw_constraint_minmax(runtime,
					SNDRV_PCM_HW_PARAM_CHANNELS, snd_bcm->channels, snd_bcm->channels)) < 0)
	{
		DBG("%s SNDRV_PCM_HW_PARAM_CHANNELS failed channels=%d\n", __FUNCTION__, snd_bcm->channels);
		goto err;
	}
	/* width */
	if ((ret = snd_pcm_hw_constraint_minmax(runtime,
					SNDRV_PCM_HW_PARAM_SAMPLE_BITS, snd_bcm->sample_bits, snd_bcm->sample_bits)) < 0)
	{
		DBG("%s SNDRV_PCM_HW_PARAM_SAMPLE_BITS failed bits=%d\n", __FUNCTION__, snd_bcm->sample_bits);
		goto err;
	}

err:
	if (ret < 0)
		DBG("%s return with error %d\n", __FUNCTION__, ret);

	return ret;
}

static int bcm947xx_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	bcm947xx_i2s_info_t *snd_bcm = rtd->dai->cpu_dai->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct bcm947xx_runtime_data *brtd;
	int ret;

	DBG("%s %s\n", __FUNCTION__, bcm947xx_direction_str(substream));

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		snd_soc_set_runtime_hwparams(substream, &bcm947xx_pcm_hardware_capture);
	} else {
		snd_soc_set_runtime_hwparams(substream, &bcm947xx_pcm_hardware_playback);
	}

	if (snd_bcm->in_use & ~(1 << substream->stream)) {
		/* Another stream is in-use; set our constraints to match. */
		if ((ret = bcm947xx_pcm_set_constraints(runtime, snd_bcm)) < 0)
			return ret;
	}
	
	brtd = kzalloc(sizeof(struct bcm947xx_runtime_data), GFP_KERNEL);
	if (brtd == NULL) {
		return -ENOMEM;
	}

	spin_lock_init(&brtd->lock);

	runtime->private_data = brtd;

	/* probably should put this somewhere else, after setting up isr ??? */
	/* Enable appropriate channel. */
	/* Channel dma_XXXinit needs to be called before descriptors can be
	 * posted to the DMA.  Posting currently occurs in the copy operator,
	 * which is called after prepare but before trigger(start).
	*/
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		dma_rxreset(snd_bcm->di[0]);
		dma_rxinit(snd_bcm->di[0]);
	} else {
		dma_txreset(snd_bcm->di[0]);
		dma_txinit(snd_bcm->di[0]);
		if (BCM947XX_DMA_LOOPBACK_ENABLED)
			dma_fifoloopbackenable(snd_bcm->di[0]);
//		dma_txsuspend(snd_bcm->di[0]);
	}

	if (BCM947XX_PCM_DEBUG_ON) {
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
	}

	return 0;
}

static int bcm947xx_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	bcm947xx_i2s_info_t *snd_bcm = rtd->dai->cpu_dai->private_data;
	struct bcm947xx_runtime_data *brtd = substream->runtime->private_data;

	DBG("%s %s\n", __FUNCTION__, bcm947xx_direction_str(substream));
	
	DBG("%s: i2s intstatus 0x%x intmask 0x%x\n", __FUNCTION__,
	    R_REG(snd_bcm->osh, &snd_bcm->regs->intstatus),
	    R_REG(snd_bcm->osh, &snd_bcm->regs->intmask));

/* #if required because dma_dump is unavailable in non-debug builds. */
#if BCM947XX_DUMP_RING_BUFFER_ON_PCM_CLOSE_ON
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
#endif /* BCM947XX_DUMP_RING_BUFFER_ON_PCM_CLOSE_ON */

	/* reclaim all descriptors */
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		dma_rxreset(snd_bcm->di[0]);
		dma_rxreclaim(snd_bcm->di[0]);
	} else {
		dma_txreset(snd_bcm->di[0]);
		dma_txreclaim(snd_bcm->di[0], HNDDMA_RANGE_ALL);
	}

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
	//struct bcm947xx_pcm_dma_params *dma = rtd->dai->cpu_dai->dma_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	bcm947xx_i2s_info_t *snd_bcm = rtd->dai->cpu_dai->private_data;
	unsigned long totbytes;
	unsigned int dma_ofs;
	unsigned long flags;

	DBG("%s %s\n", __FUNCTION__, bcm947xx_direction_str(substream));

	/* RX DMA requires a data offset due to the RX status header.
	 * Although there is a register setting to make the status header offset
	 * zero, it doesn't seem to work with 4709.
	*/
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		dma_ofs = BCM947XX_DMA_RXOFS_BYTES;
	} else {
		dma_ofs = 0;
	}

	/* Total bytes in the DMA buffer (excluding period fragement), including unused and
	 * header bytes.
	*/
	totbytes  = params_buffer_bytes(params);
	totbytes += params_periods(params) * (BCM947XX_DMA_DATA_BYTES_MAX - params_period_bytes(params));

	/* Account for period fragment. */
	if (params_buffer_bytes(params) > params_periods(params) * params_period_bytes(params)) {
		totbytes += dma_ofs;
	}

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	runtime->dma_bytes = params_buffer_bytes(params);

	spin_lock_irqsave(&brtd->lock, flags);

	brtd->dma_limit = params_periods(params); //runtime->hw.periods_min;
	brtd->dma_period = params_period_bytes(params);

	/* Virtual address of our runtime buffer */
	brtd->dma_start = (dma_addr_t)runtime->dma_area;
	brtd->dma_end = brtd->dma_start + totbytes;

	brtd->dma_ofs = dma_ofs;

	if (!(snd_bcm->in_use & ~(1 << substream->stream))) {
		/* Other stream not in-use (we own the settings). */
		/* It's safe to set the joint settings and mark as in-use. */
		bcm947xx_pcm_set_joint_duplex_settings(snd_bcm, params);
		snd_bcm->in_use |= (1 << substream->stream);

	} else if (!bcm947xx_pcm_joint_duplex_settings_equal(snd_bcm, params)) {
		/* Joint settings don't match; therefore, we're not in-use; bail. */
		DBG("%s joint duplex settings not equal\n", __FUNCTION__);
		snd_pcm_set_runtime_buffer(substream, NULL);
		snd_bcm->in_use &= ~(1 << substream->stream);
		return -EBUSY;

	} else {
		/* Joint settings matched, and perhaps our first time; mark as in-use! */
		snd_bcm->in_use |= (1 << substream->stream);
	}

	spin_unlock_irqrestore(&brtd->lock, flags);

	if (BCM947XX_PCM_DEBUG_ON)
	{
		size_t buffer_size = params_buffer_size(params);
		size_t buffer_bytes = params_buffer_bytes(params);
		size_t period_size = params_period_size(params);
		size_t period_bytes = params_period_bytes(params);
		size_t periods = params_periods(params);

		DBG("%s: dma_limit %d dma_ofs %d dma_addr %p dma_bytes %d dma_start %p dma_end %p\n",
			__FUNCTION__, brtd->dma_limit, brtd->dma_ofs, (void *)runtime->dma_addr, runtime->dma_bytes,
			(void *)brtd->dma_start, (void *)brtd->dma_end);
		DBG("%s: buffer_size %d buffer_bytes %d\n", __FUNCTION__, buffer_size, buffer_bytes);
		DBG("%s: period_size %d period_bytes %d periods %d\n", __FUNCTION__, period_size, period_bytes, periods);
	}

	return 0;
}

static int bcm947xx_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	bcm947xx_i2s_info_t *snd_bcm = rtd->dai->cpu_dai->private_data;

	DBG("%s %s\n", __FUNCTION__, bcm947xx_direction_str(substream));

	snd_pcm_set_runtime_buffer(substream, NULL);

	/* This stream is no longer consider in-use. */
	snd_bcm->in_use &= ~(1 << substream->stream);

	if (!snd_bcm->in_use)
	{	/* All streams are not used. */
		bcm947xx_pcm_clear_joint_duplex_settings(snd_bcm);
	}

	return 0;
}

/* Called after hw_params and before trigger(start). */
static int bcm947xx_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	bcm947xx_i2s_info_t *snd_bcm = rtd->dai->cpu_dai->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct bcm947xx_runtime_data *brtd = runtime->private_data;
	uint32 intmask = R_REG(snd_bcm->osh, &snd_bcm->regs->intmask);
	uint32 intstatus = 0;
	unsigned long flags;
	int ret = 0;

	DBG("%s %s\n", __FUNCTION__, bcm947xx_direction_str(substream));

	spin_lock_irqsave(&brtd->lock, flags);

	/* Reset s/w DMA accounting. */
	brtd->dma_pos = brtd->dma_start;
	brtd->dma_loaded = brtd->bytes_pending = 0;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		intmask |= intmask_capture;
		intstatus |= intmask_capture;
	} else {
		intmask |= intmask_playback;
		intstatus |= intmask_playback;
	}
	/* Clear any pending interrupts. */
	W_REG(snd_bcm->osh, &snd_bcm->regs->intstatus, intstatus);
	/* Enable interrupts. */
	W_REG(snd_bcm->osh, &snd_bcm->regs->intmask, intmask);

	spin_unlock_irqrestore(&brtd->lock, flags);

	DBG("%s: i2s intstatus 0x%x intmask 0x%x\n", __FUNCTION__,
	    R_REG(snd_bcm->osh, &snd_bcm->regs->intstatus),
	    R_REG(snd_bcm->osh, &snd_bcm->regs->intmask));

	return ret;
}

/* Lock me. */
/* pos must be in the data area. */
static snd_pcm_uframes_t bcm947xx_to_linear(struct snd_pcm_substream *substream, dma_addr_t pos)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct bcm947xx_runtime_data *brtd = runtime->private_data;
	unsigned int slot;
	unsigned int extrabytes;
	unsigned long byteofs;

	/* Byte offset of position in DMA buffer. */
	snd_BUG_ON(pos < brtd->dma_start + brtd->dma_ofs);
	byteofs = pos >= brtd->dma_start ? (unsigned long)(pos - brtd->dma_start) : 0;
	
	/* Period slot of pos (0-index). */
	slot = byteofs/BCM947XX_DMA_DATA_BYTES_MAX;

	/* Extra bytes in descriptor (header + unused + pad). */
	extrabytes = BCM947XX_DMA_DATA_BYTES_MAX - snd_pcm_lib_period_bytes(substream);
	
	/* Adjust byte offset to be the linear byte offset by subtracting
	 * the unused and header offset bytes.
	*/
	byteofs -= slot * extrabytes +	/* all per slot extra up before current slot */
		brtd->dma_ofs;				/* header offset in current slot */

//	DBG("%s %p slot=%u extrabytes=%u byteofs=%lu(%ld)\n",
//		__FUNCTION__, (void *)pos, slot, extrabytes,
//		byteofs, bytes_to_frames(runtime, byteofs));

	return bytes_to_frames(runtime, byteofs);
}

/* Lock me. */
static inline char *bcm947xx_from_linear(struct snd_pcm_substream *substream, snd_pcm_uframes_t pos)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct bcm947xx_runtime_data *brtd = runtime->private_data;
	unsigned int slot;
	unsigned int extrabytes;
	char *hwbuf;

	/* Current slot (0-index). */
	slot = pos / runtime->period_size;

	/* Extra bytes in descriptor (header + unused). */
	extrabytes = BCM947XX_DMA_DATA_BYTES_MAX - snd_pcm_lib_period_bytes(substream);

	/* Calculate DMA buffer position by adding the unused
	 * and header offset bytes. */
	hwbuf = runtime->dma_area +				/* base offset */
		slot * extrabytes +					/* all per slot extra up before current slot */
		frames_to_bytes(runtime, pos) +		/* position in slot */
		brtd->dma_ofs;						/* header offset in current slot */

	return hwbuf;
}

static int
bcm947xx_dma_getposition(bcm947xx_i2s_info_t *snd_bcm, dma_addr_t *src, dma_addr_t *dst)
{
	if (src) {
		/* DMA_TX */
		*src = (dma_addr_t)dma_getpos(snd_bcm->di[0], TRUE);
	} else if (dst) {
		/* DMA_RX */
		*dst = (dma_addr_t)dma_getpos(snd_bcm->di[0], FALSE);
	}

	return 0;
}

/* Called with interrupts disabled. */
static snd_pcm_uframes_t
bcm947xx_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct bcm947xx_runtime_data *brtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	bcm947xx_i2s_info_t *snd_bcm = rtd->dai->cpu_dai->private_data;
	snd_pcm_uframes_t res;
	dma_addr_t pos = 0;

//	DBG("%s %s\n", __FUNCTION__, bcm947xx_direction_str(substream));
	
	spin_lock(&brtd->lock);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		bcm947xx_dma_getposition(snd_bcm, NULL, &pos);
	} else {
		bcm947xx_dma_getposition(snd_bcm, &pos, NULL);
	}

	if ((void *)pos == NULL) {
		res = 0; /* DMA not running? */
	} else {
		res = bcm947xx_to_linear(substream, pos + brtd->dma_ofs);
	}

	spin_unlock(&brtd->lock);

	if (snd_BUG_ON(res % runtime->period_size != 0))
		DBG("%s pcm_pointer %lu\n", __FUNCTION__, res);

	return res;
}

/* count is contiguous within the period. */
static int bcm947xx_pcm_copy(struct snd_pcm_substream *substream, int channel,
        snd_pcm_uframes_t pos, void *src, snd_pcm_uframes_t count)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct bcm947xx_runtime_data *brtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	bcm947xx_i2s_info_t *snd_bcm = rtd->dai->cpu_dai->private_data;
	ssize_t framebytes = frames_to_bytes(runtime, count);
	char *hwbuf;
	unsigned long flags;
	
	/* Linear position to DMA pointer. */
	spin_lock_irqsave(&brtd->lock, flags);
	hwbuf = bcm947xx_from_linear(substream, pos);
	spin_unlock_irqrestore(&brtd->lock, flags);

	/* Copy (capture). */
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (BCM947XX_DMA_RX_CACHE_INVALIDATE_WAR_ON &&
			pos % runtime->period_size == 0)
		{
			DMA_MAP(snd_bcm->osh, hwbuf-brtd->dma_ofs, BCM947XX_DMA_DATA_BYTES_MAX, DMA_RX, NULL, NULL);
		}
		if (copy_to_user(src, hwbuf, framebytes))
			return -EFAULT;
		if (BCM947XX_TRACE_CAPTURE_COPY_ON) {
			DBG("%s hwbuf=%p pos=%lu count=%lu (%zd bytes)\n", __FUNCTION__,
				hwbuf, pos, count, framebytes);
		}

	/* Copy (playback). */
	} else {
		if (copy_from_user(hwbuf, src, framebytes))
			return -EFAULT;
		if (BCM947XX_TRACE_PLAYBACK_COPY_ON) {
			DBG("%s hwbuf=%p pos=%lu count=%lu (%zd bytes)\n", __FUNCTION__,
				hwbuf, pos, count, framebytes);
		}
	}

	spin_lock_irqsave(&brtd->lock, flags);
	brtd->bytes_pending += frames_to_bytes(runtime, count);
	bcm947xx_pcm_enqueue(substream);
	spin_unlock_irqrestore(&brtd->lock, flags);

	return 0;
}

/* Called with interrupts disabled. */
/* Maybe called from snd_period_elapsed. */
static int bcm947xx_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	bcm947xx_i2s_info_t *snd_bcm = rtd->dai->cpu_dai->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct bcm947xx_runtime_data *brtd = runtime->private_data;
	uint32 intmask = R_REG(snd_bcm->osh, &snd_bcm->regs->intmask);
	int ret = 0;
	
	DBG("%s %s w/cmd=%d\n", __FUNCTION__, bcm947xx_direction_str(substream), cmd);

	spin_lock(&brtd->lock);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			brtd->bytes_pending = snd_pcm_lib_buffer_bytes(substream);
			bcm947xx_pcm_enqueue(substream);
		} else {
//			dma_txresume(snd_bcm->di[0]);
		}
		break;

	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
//			dma_txsuspend(snd_bcm->di[0]);
			break;
		}
		/* fall-thru */

	case SNDRV_PCM_TRIGGER_STOP:
		/* Reset the disable interrupts, DMA RX/TX channel.
		   Might get here as a result of calling snd_pcm_period_elapsed.
		*/
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			intmask &= ~intmask_capture;
			W_REG(snd_bcm->osh, &snd_bcm->regs->intmask, intmask);
			dma_rxreset(snd_bcm->di[0]);
			dma_rxreclaim(snd_bcm->di[0]);
			dma_rxinit(snd_bcm->di[0]);
		} else {
			/* Disable transmit interrupts. */
			intmask &= ~intmask_playback;
			W_REG(snd_bcm->osh, &snd_bcm->regs->intmask, intmask);
			dma_txreset(snd_bcm->di[0]);
			dma_txreclaim(snd_bcm->di[0], HNDDMA_RANGE_ALL);
			dma_txinit(snd_bcm->di[0]);
			if (BCM947XX_DMA_LOOPBACK_ENABLED)
				dma_fifoloopbackenable(snd_bcm->di[0]);
//			dma_txsuspend(snd_bcm->di[0]);
		}
		break;
	default:	
		ret = -EINVAL;
	}

	spin_unlock(&brtd->lock);

	DBG("%s: i2s intstatus 0x%x intmask 0x%x\n", __FUNCTION__,
	    R_REG(snd_bcm->osh, &snd_bcm->regs->intstatus),
	    R_REG(snd_bcm->osh, &snd_bcm->regs->intmask));

	return ret;
}

struct snd_pcm_ops bcm947xx_pcm_ops = {
	.open		= bcm947xx_pcm_open,
	.close		= bcm947xx_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= bcm947xx_pcm_hw_params,
	.hw_free	= bcm947xx_pcm_hw_free,
	.prepare	= bcm947xx_pcm_prepare,
	.pointer	= bcm947xx_pcm_pointer,
	.copy		= bcm947xx_pcm_copy,
	.trigger	= bcm947xx_pcm_trigger,
};


static int bcm947xx_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{

	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		size = BCM947XX_CAPTURE_DMA_BUFFER_BYTES_MAX;
	} else {
		size = BCM947XX_PLAYBACK_DMA_BUFFER_BYTES_MAX;
	}

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

static void bcm947xx_pcm_preallocate_free(struct snd_pcm *pcm)
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
}

static void bcm947xx_pcm_free(struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *rtd = pcm->private_data;
	bcm947xx_i2s_info_t *snd_bcm = rtd->dai->cpu_dai->private_data;
	
	DBG("%s\n", __FUNCTION__);

	bcm947xx_pcm_preallocate_free(pcm);
	free_irq(snd_bcm->irq, pcm);
}


static u64 bcm947xx_pcm_dmamask = DMA_BIT_MASK(32);

int bcm947xx_pcm_new(struct snd_card *card, struct snd_soc_dai *dai,
	struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *rtd = pcm->private_data;
	bcm947xx_i2s_info_t *snd_bcm = rtd->dai->cpu_dai->private_data;
	int ret = 0;

	DBG("%s\n", __FUNCTION__);

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &bcm947xx_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	if (dai->playback.channels_min) {
		ret = bcm947xx_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_PLAYBACK);
		if (ret)
			goto out;
	}
	if (dai->capture.channels_min) {
		ret = bcm947xx_pcm_preallocate_dma_buffer(pcm,
			SNDRV_PCM_STREAM_CAPTURE);
		if (ret)
			goto out;
	}


	if ((ret = request_irq(snd_bcm->irq,
	                 bcm947xx_i2s_isr, IRQF_SHARED, "i2s", pcm)) < 0) {
		DBG("%s: request_irq failure\n", __FUNCTION__);
	}

	/* Initialize joint playback/capture settings.
	 * These must be the same for both playback and capture.
	*/
	snd_bcm->in_use = 0;
	bcm947xx_pcm_clear_joint_duplex_settings(snd_bcm);
	
out:
	if (ret < 0) {
		bcm947xx_pcm_preallocate_free(pcm);
	}

	return ret;
}


struct snd_soc_platform bcm947xx_soc_platform = {
	.name		= "bcm947xx-audio",
	.pcm_ops 	= &bcm947xx_pcm_ops,
	.pcm_new	= bcm947xx_pcm_new,
	.pcm_free	= bcm947xx_pcm_free,
};

EXPORT_SYMBOL_GPL(bcm947xx_soc_platform);

static int __init bcm947xx_soc_platform_init(void)
{
	return snd_soc_register_platform(&bcm947xx_soc_platform);
}
module_init(bcm947xx_soc_platform_init);

static void __exit bcm947xx_soc_platform_exit(void)
{
	snd_soc_unregister_platform(&bcm947xx_soc_platform);
}
module_exit(bcm947xx_soc_platform_exit);

MODULE_LICENSE("GPL and additional rights");
/* MODULE_AUTHOR(""); */
MODULE_DESCRIPTION("BCM947XX PCM module");
