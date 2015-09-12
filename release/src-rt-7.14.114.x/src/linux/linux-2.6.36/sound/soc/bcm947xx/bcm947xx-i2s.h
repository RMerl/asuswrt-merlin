/*
 * ALSA I2S Interface for the Broadcom BCM947XX family of SOCs
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
 * $Id: bcm947xx-i2s.h,v 1.1 2009-10-30 20:46:42 $
 */

#ifndef _BCM947XX_I2S_H
#define _BCM947XX_I2S_H

/* bcm947xx DAI ID's */
#define BCM947XX_DAI_I2S			0

/* I2S clock */
#define BCM947XX_I2S_SYSCLK			0

/* Maximium number of DMA transmit descriptors available. */
#define BCM947XX_NR_DMA_TXDS_MAX	128

/* Maximum number of DMA receive descriptors available. */
#define BCM947XX_NR_DMA_RXDS_MAX	BCM947XX_NR_DMA_TXDS_MAX

/* Maximum number of data bytes per DMA descriptor. */
#define BCM947XX_DMA_DATA_BYTES_MAX	4096

#define BCM947XX_DMA_RXOFS_WAR_ON

#ifdef BCM947XX_DMA_RXOFS_WAR_ON
/* Receive frame status header byte length. */
#define BCM947XX_DMA_RXOFS_BYTES 4
#else
#define BCM947XX_DMA_RXOFS_BYTES 0
#endif

/* I2S RX BufCount + RcvControl.RxOffset == 4KB will cause the DMA engine
 * to repeat previous 32-bytes at the 4KB boundary.  Enable extra offset
 * bytes at the end
 * of the data buffer to WAR.
*/
#define BCM947XX_DMA_RXOFS_4KB_WAR_ON

#ifdef BCM947XX_DMA_RXOFS_4KB_WAR_ON
/* 4 bytes is sufficient for 16-bit words, 8 is for safety. */
#define BCM947XX_DMA_RXOFS_TAIL_PAD_BYTES 8
#else
#define BCM947XX_DMA_RXOFS_TAIL_PAD_BYTES 0
#endif

/* Notes on WARs
 *
 * BCM947XX_DMA_RX_CACHE_INVALIDATE_WAR_ON
 * Perform a post-DMA cache invalidate.  On some archs (CA9) the
 * cache is not fully invalid and subseqent reads show
 * bad data from the start to 32 and 64 bytes into the data buffer.
*/

/* CA9 specific settings. */
#if defined(BCM47XX_CA9)
#define BCM947XX_DMA_RX_CACHE_INVALIDATE_WAR_ON 1
#endif /* defined(BCM47XX_CA9 */

#ifndef BCM947XX_DMA_RX_CACHE_INVALIDATE_WAR_ON
#define BCM947XX_DMA_RX_CACHE_INVALIDATE_WAR_ON 0
#endif /* BCM947XX_DMA_RX_CACHE_INVALIDATE_WAR_ON */

extern struct snd_soc_dai bcm947xx_i2s_dai;

typedef struct bcm947xx_i2s_info bcm947xx_i2s_info_t;
struct bcm947xx_i2s_info {
	/* ALSA structs. */
	struct snd_card *card;
	//	struct snd_pcm *pcm[BCM947XX_PCM_LAST];

	//spinlock_t lock;

	int		irq;
	osl_t		*osh;
	void		*regsva;			/* opaque chip registers virtual address */
	i2sregs_t	*regs;			/* pointer to device registers */
	hnddma_t	*di[1];		/* hnddma handles, per fifo */
	si_t		*sih;		/* SB handle (cookie for siutils calls) */
	uint32		mclk;		/* Frequency of system MCLK */

	/* Locking provided by ASoC. */
	int			in_use;
	int			rate;
	int			channels;
	int			sample_bits;
};

static inline char *bcm947xx_direction_str(struct snd_pcm_substream *substream)
{
	return substream->stream == SNDRV_PCM_STREAM_CAPTURE ? "capture" : "playback";
}

#endif /* _BCM947XX_I2S_H */
