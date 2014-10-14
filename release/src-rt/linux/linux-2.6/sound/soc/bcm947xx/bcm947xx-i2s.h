/*
 * ALSA I2S Interface for the Broadcom BCM947XX family of SOCs
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: bcm947xx-i2s.h,v 1.1 2009/10/30 20:46:42 Exp $
 */

#ifndef _BCM947XX_I2S_H
#define _BCM947XX_I2S_H

/* bcm947xx DAI ID's */
#define BCM947XX_DAI_I2S			0

/* I2S clock */
#define BCM947XX_I2S_SYSCLK			0

extern struct snd_soc_cpu_dai bcm947xx_i2s_dai;

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
};

extern bcm947xx_i2s_info_t *snd_bcm;

#endif /* _BCM947XX_I2S_H */
