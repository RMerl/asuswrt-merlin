/*
 * ALSA I2S Interface for the Broadcom BCM947XX family of SOCs
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
};

static inline char *bcm947xx_direction_str(struct snd_pcm_substream *substream)
{
	return substream->stream == SNDRV_PCM_STREAM_CAPTURE ? "capture" : "playback";
}

#endif /* _BCM947XX_I2S_H */
