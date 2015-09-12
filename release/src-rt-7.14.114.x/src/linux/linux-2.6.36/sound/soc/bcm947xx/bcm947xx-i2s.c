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
 * $Id: bcm947xx-i2s.c,v 1.3 2010-05-14 00:36:48 $
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
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
#include <sbchipc.h>
#include <i2s_core.h>
#include <hndpmu.h>
#include <chipcommonb.h>

#include "bcm947xx-i2s.h"

/* Be careful here... turning on prints can break everything, if you start seeing FIFO underflows
 * then it might be due to excessive printing
 */
#define BCM947XX_I2S_DEBUG 0
#if BCM947XX_I2S_DEBUG
#define DBG(x...) printk(KERN_ERR x)
#else
#define DBG(x...)
#endif


#define BCM947XX_SND "bcm947xx i2s sound"


static int bcm947xx_i2s_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	//DBG("%s\n", __FUNCTION__);
	return 0;
}

static void bcm947xx_i2s_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	//DBG("%s\n", __FUNCTION__);
	return;
}

static int bcm947xx_i2s_probe(struct platform_device *pdev,
	struct snd_soc_dai *dai)
{
	int ret = 0;
	bcm947xx_i2s_info_t *snd_bcm = dai->private_data;

	if (snd_bcm && snd_bcm->sih)
		if (si_findcoreidx(snd_bcm->sih, I2S_CORE_ID, 0) == BADIDX)
			ret = -EINVAL;

	return ret;
}


static int bcm947xx_i2s_suspend(struct snd_soc_dai *dai)
{
	DBG("%s - TBD\n", __FUNCTION__);
	return 0;
}

static int bcm947xx_i2s_resume(struct snd_soc_dai *dai)
{
	DBG("%s - TBD\n", __FUNCTION__);
	return 0;
}

#define I2S_INTRECE_LAZY_FC_MASK	0xFF000000
#define I2S_INTRECE_LAZY_FC_SHIFT	24

static int bcm947xx_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
	struct snd_soc_dai *dai)
{
	bcm947xx_i2s_info_t *snd_bcm = dai->private_data;
	uint32 i2scontrol = R_REG(snd_bcm->osh, &snd_bcm->regs->i2scontrol);
	uint32 i2sintrecelazy = R_REG(snd_bcm->osh, &snd_bcm->regs->intrecvlazy);
	int ret = 0;

	DBG("%s w/cmd %d\n", __FUNCTION__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			/* Enable I2S lazy interrupt.  The lazy interrupt is required
			 * to generate reception interrupts in contrast to using the
			 * IOC mechansism for TX.  Interrupt when 1 frame is
			 * transferred, corresponding to one descriptor.
			*/
			i2sintrecelazy &= ~I2S_INTRECE_LAZY_FC_MASK;
			i2sintrecelazy |= 1 << I2S_INTRECE_LAZY_FC_SHIFT;
			i2scontrol |= I2S_CTRL_RECEN;
		} else {
			i2scontrol |= I2S_CTRL_PLAYEN;
		}
		W_REG(snd_bcm->osh, &snd_bcm->regs->i2scontrol, i2scontrol);
		W_REG(snd_bcm->osh, &snd_bcm->regs->intrecvlazy, i2sintrecelazy);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			i2scontrol &= ~I2S_CTRL_RECEN;
		} else {
			i2scontrol &= ~I2S_CTRL_PLAYEN;
		}
		W_REG(snd_bcm->osh, &snd_bcm->regs->i2scontrol, i2scontrol);
		break;
	default:
	ret = -EINVAL;
	}

	DBG("%s: i2s intstatus 0x%x intmask 0x%x\n", __FUNCTION__,
	    R_REG(snd_bcm->osh, &snd_bcm->regs->intstatus),
	    R_REG(snd_bcm->osh, &snd_bcm->regs->intmask));

	return ret;
}

/* Set I2S DAI format */
static int bcm947xx_i2s_set_fmt(struct snd_soc_dai *cpu_dai,
		unsigned int fmt)
{
	bcm947xx_i2s_info_t *snd_bcm = cpu_dai->private_data;
	u32 devctrl = R_REG(snd_bcm->osh, &snd_bcm->regs->devcontrol);

	DBG("%s: format 0x%x\n", __FUNCTION__, fmt);

	/* We always want this core to be in I2S mode */
	devctrl &= ~I2S_DC_MODE_TDM;

	/* See include/sound/soc.h for DAIFMT */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		/* Codec clk master and frame master */
		devctrl |= I2S_DC_BCLKD_IN;
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		/* Codec clk slave and frame master */
		/* BCM SOC is the master */
		devctrl &= ~I2S_DC_BCLKD_IN;
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		/* Codec clk master and frame slave */
		devctrl |= I2S_DC_BCLKD_IN;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		/* Codec clk slave and frame slave */
		/* BCM SOC is the master */
		devctrl &= ~I2S_DC_BCLKD_IN;
		break;
	default:
		DBG("%s: unsupported MASTER: 0x%x \n", __FUNCTION__,
		    fmt & SND_SOC_DAIFMT_MASTER_MASK );
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		/* we only support I2S Format */
		break;
	default:
		DBG("%s: unsupported FORMAT: 0x%x \n", __FUNCTION__,
		    fmt & SND_SOC_DAIFMT_FORMAT_MASK );
		return -EINVAL;
	}

	//DBG("%s: I2S setting devctrl to 0x%x\n", __FUNCTION__, devctrl);
	/* Write I2S devcontrol reg */
	W_REG(snd_bcm->osh, &snd_bcm->regs->devcontrol, devctrl);

	return 0;
}


/*
 * Set Clock source
 */
static int bcm947xx_i2s_set_sysclk(struct snd_soc_dai *cpu_dai,
		int clk_id, unsigned int freq, int dir)
{
	bcm947xx_i2s_info_t *snd_bcm = cpu_dai->private_data;
	/* Stash the MCLK rate that we're using, we can use it to help us to pick
	 * the right clkdiv settings later.
	 */
	snd_bcm->mclk = freq;
	DBG("%s: mclk %d Hz\n", __FUNCTION__, snd_bcm->mclk);

	return 0;
}

static int bcm947xx_i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	bcm947xx_i2s_info_t *snd_bcm = dai->private_data;
	u32 devctrl = R_REG(snd_bcm->osh, &snd_bcm->regs->devcontrol);
	u32 clkdiv = R_REG(snd_bcm->osh, &snd_bcm->regs->clkdivider);
	u32 stxctrl = R_REG(snd_bcm->osh, &snd_bcm->regs->stxctrl);
	uint32 srate = 0;
	uint32 rate = params_rate(params);
	int channels = params_channels(params);
	int ii = 0;
	bool found = FALSE;
	bool in_use;
	
	/* Joint settings in-use?
	 * Limit the scope of what can be configured.  For example,
	 * it doesn't make sense to change the sample rate since clocks
	 * are shared.
	*/
	in_use = (snd_bcm->in_use & ~(1 << substream->stream));

	DBG("%s: %s in_use=%d\n", __FUNCTION__, bcm947xx_direction_str(substream), in_use);

	/* Touching DevControl when in_use will cause the I2S FIFO to be
	 * reset thus introducing an audible "pop" on the in-use stream.
	 * Just bail for now instead of trying to support additional
	 * configurations.
	*/
	if (in_use)
		goto DONE;

	/* ClockDivider.SRate */
	{
		/* Set up our ClockDivider register with audio sample rate */
		for (ii = 0; ii < ARRAY_SIZE(i2s_clkdiv_coeffs); ii++) {
			if ((i2s_clkdiv_coeffs[ii].rate == rate) &&
				(i2s_clkdiv_coeffs[ii].mclk == snd_bcm->mclk)) {
				found = TRUE;
				break;
			}
		}

		if(found != TRUE) {
			printk(KERN_ERR "%s: unsupported audio sample rate %d Hz and mclk %d Hz "
				   "combination\n", __FUNCTION__, rate, snd_bcm->mclk);
			return -EINVAL;
		} else {
			/* Write the new SRATE into the clock divider register */
			srate = (i2s_clkdiv_coeffs[ii].srate << I2S_CLKDIV_SRATE_SHIFT);
			clkdiv &= ~I2S_CLKDIV_SRATE_MASK;
			W_REG(snd_bcm->osh, &snd_bcm->regs->clkdivider, clkdiv | srate);

			DBG("%s: i2s clkdivider 0x%x txplayth 0x%x\n", __FUNCTION__,
				R_REG(snd_bcm->osh, &snd_bcm->regs->clkdivider),
				R_REG(snd_bcm->osh, &snd_bcm->regs->txplayth));
			DBG("%s: audio sample rate %d Hz and mclk %d Hz\n",
				__FUNCTION__, rate, snd_bcm->mclk);
		}
	}

	/* DevControl.OPCHSEL */
	{
		/* Set up for the # of channels in this stream */
		/* For I2S/SPDIF we support 2 channel -OR- 6 (5.1) channels */
		DBG("%s: %d channels in this stream\n", __FUNCTION__, channels);

		switch (channels) {
		case 2:
			devctrl &= ~I2S_DC_OPCHSEL_6;
			break;
		case 6:
			devctrl |= I2S_DC_OPCHSEL_6;
			break;
		default:
			printk(KERN_ERR "%s: unsupported number of channels in stream - %d\n"
				   "combination\n", __FUNCTION__, channels);
			return -EINVAL;
		}
	}

	DBG("%s: rate %d access 0x%x format 0x%x subformat 0x%x\n", __FUNCTION__,
		rate, params_access(params), params_format(params), params_subformat(params));

	/* Clear TX/RX word length bits then set the # of bits per sample in this stream.
	 * This could be configurated separately from the in-use substream (see in_use
	 * comment above).
	*/
	{
		/* DevControl.WL_RX */
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			devctrl &= ~(I2S_DC_WL_RX_MASK);
			switch (params_format(params)) {
			/* Available in I2S core rev > 2. */
			case SNDRV_PCM_FORMAT_U8:
				devctrl |= 0x8000;
				break;
			case SNDRV_PCM_FORMAT_S16_LE:
				devctrl |= 0x0;
				break;
			case SNDRV_PCM_FORMAT_S20_3LE:
				devctrl |= 0x1000;
				break;
			case SNDRV_PCM_FORMAT_S24_LE:
			case SNDRV_PCM_FORMAT_S24_3LE:
				devctrl |= 0x2000;
				break;
			case SNDRV_PCM_FORMAT_S32_LE:
				devctrl |= 0x3000;
				break;
			default:
				DBG("%s: unsupported format\n", __FUNCTION__);
				return -EINVAL;
			}

		/* DevControl.WL_TX */
		} else {
			devctrl &= ~(I2S_DC_WL_TX_MASK);
			stxctrl &= ~(I2S_STXC_WL_MASK);
			switch (params_format(params)) {
			/* Available in I2S core rev > 2. */
			case SNDRV_PCM_FORMAT_U8:
				devctrl |= 0x4000;
				stxctrl |= 0x1000;
				break;
			case SNDRV_PCM_FORMAT_S16_LE:
				devctrl |= 0x0;
				stxctrl |= 0x0;
				break;
			case SNDRV_PCM_FORMAT_S20_3LE:
				devctrl |= 0x400;
				stxctrl |= 0x01;
				break;
			case SNDRV_PCM_FORMAT_S24_LE:
			case SNDRV_PCM_FORMAT_S24_3LE:
				devctrl |= 0x800;
				stxctrl |= 0x02;
				break;
			case SNDRV_PCM_FORMAT_S32_LE:
				devctrl |= 0xC00;
				/* SPDIF doesn't support 32 bit samples */
				/* Should we just disable SPDIF rather than putting out garbage? */
				stxctrl |= 0x03;
				break;
			default:
				DBG("%s: unsupported format\n", __FUNCTION__);
				return -EINVAL;
			}
		}
	}

	/* DevControl.DPX */
	{
		/* Full duplex. */
		devctrl |= I2S_DC_DPX_MASK;
	}

	/* Write I2S devcontrol reg */
	W_REG(snd_bcm->osh, &snd_bcm->regs->devcontrol, devctrl);
	
	/* DevControl.I2SCFG */
	{
		/* Required when changing duplex. */
		devctrl |= I2S_DC_I2SCFG;
		W_REG(snd_bcm->osh, &snd_bcm->regs->devcontrol, devctrl);
	}
	
//	W_REG(snd_bcm->osh, &snd_bcm->regs->stxctrl, stxctrl);
//	DBG("%s: set devctrl 0x%x && stxctrl 0x%x\n", __FUNCTION__, devctrl, stxctrl);

DONE:
	DBG("%s: read devctrl 0x%x stxctrl 0x%x\n", __FUNCTION__,
	    R_REG(snd_bcm->osh, &snd_bcm->regs->devcontrol),
	    R_REG(snd_bcm->osh, &snd_bcm->regs->stxctrl));
	return 0;
}

#define BCM947XX_I2S_RATES \
        (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 | \
        SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_32000 | \
		SNDRV_PCM_RATE_44100 | \
        SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000)

#define BCM947XX_I2S_FORMATS \
        (SNDRV_PCM_FMTBIT_S8  | SNDRV_PCM_FMTBIT_U8 | \
         SNDRV_PCM_FMTBIT_S16 | SNDRV_PCM_FMTBIT_U16 | \
         SNDRV_PCM_FMTBIT_S24 | SNDRV_PCM_FMTBIT_U24 | \
         SNDRV_PCM_FMTBIT_S32 | SNDRV_PCM_FMTBIT_U32)

struct snd_soc_dai_ops bcm947xx_i2s_dai_ops = {
	.startup = bcm947xx_i2s_startup,
	.shutdown = bcm947xx_i2s_shutdown,
	.trigger = bcm947xx_i2s_trigger,
	.hw_params = bcm947xx_i2s_hw_params,
	.set_fmt = bcm947xx_i2s_set_fmt,
	.set_sysclk = bcm947xx_i2s_set_sysclk,
};

struct snd_soc_dai bcm947xx_i2s_dai = {
	.name = "bcm947xx-i2s",
	.id = 0,
/*	.type = SND_SOC_DAI_I2S, */
	.probe = bcm947xx_i2s_probe,
	.suspend = bcm947xx_i2s_suspend,
	.resume = bcm947xx_i2s_resume,
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = BCM947XX_I2S_RATES,
		.formats = BCM947XX_I2S_FORMATS,},
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = BCM947XX_I2S_RATES,
		.formats = BCM947XX_I2S_FORMATS,},
	.ops = &bcm947xx_i2s_dai_ops,
};

EXPORT_SYMBOL_GPL(bcm947xx_i2s_dai);

MODULE_LICENSE("GPL and additional rights");
/* MODULE_AUTHOR(""); */
MODULE_DESCRIPTION("BCM947XX I2S module");


/************************************************************************************************/

#define DMAREG(a, direction, fifonum)	( \
	(direction == DMA_TX) ? \
                (void *)(uintptr)&(a->regs->dmaregs[fifonum].dmaxmt) : \
                (void *)(uintptr)&(a->regs->dmaregs[fifonum].dmarcv))

static struct pci_device_id bcm947xx_i2s_pci_id_table[] = {
	{ PCI_VENDOR_ID_BROADCOM, BCM47XX_AUDIO_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{0,}
};

MODULE_DEVICE_TABLE(pci, bcm947xx_i2s_pci_id_table);

static uint msg_level=1;

static bcm947xx_i2s_info_t *
bcm947xx_i2s_pci_attach(uint16 vendor, uint16 device, ulong regs, uint bustype, void *btparam,
                        uint irq)
{
	osl_t *osh = NULL;
	bcm947xx_i2s_info_t *snd = NULL;
	int ret;

	int dma_attach_err = 0;


	DBG("%s: vendor 0x%x device 0x%x regs 0x%lx bustype 0x%x btparam %p irq 0x%x\n",
	    __FUNCTION__, vendor, device, regs, bustype, btparam, irq);


	osh = osl_attach(btparam, bustype, FALSE);
	ASSERT(osh);

	/* Set ACP coherence flag */
	if (OSL_ARCH_IS_COHERENT())
		osl_flag_set(osh, OSL_ACP_COHERENCE);

	/* allocate private info */
	if ((snd = (bcm947xx_i2s_info_t *) MALLOC(osh, sizeof(bcm947xx_i2s_info_t))) == NULL) {
		osl_detach(osh);
		return NULL;
	}

	bzero(snd, sizeof(bcm947xx_i2s_info_t));
	snd->osh = osh;

	if ((snd->regsva = ioremap_nocache(regs, PCI_BAR0_WINSZ)) == NULL) {
		DBG("ioremap_nocache() failed\n");
                osl_detach(snd->osh);
		return NULL;
	}

	/*
	 * Do the hardware portion of the attach.
	 * Also initialize software state that depends on the particular hardware
	 * we are running.
	 */
	snd->sih = si_attach((uint)device, snd->osh, snd->regsva, bustype, btparam,
	                        NULL, NULL);

	snd->regs = (i2sregs_t *)si_setcore(snd->sih, I2S_CORE_ID, 0);
	si_core_reset(snd->sih, 0, 0);

	snd->di[0] = dma_attach(snd->osh, "i2s_dma", snd->sih,
					DMAREG(snd, DMA_TX, 0),
					DMAREG(snd, DMA_RX, 0),
					BCM947XX_NR_DMA_TXDS_MAX,
					BCM947XX_NR_DMA_RXDS_MAX,
					BCM947XX_DMA_DATA_BYTES_MAX,
					0, /* rxextraheadroom */
					0, /* nrxpost */
					BCM947XX_DMA_RXOFS_BYTES,
					&msg_level);
	if (snd->di[0] == NULL)
		DBG("%s: DMA attach unsuccessful!\n", __FUNCTION__);

	dma_attach_err |= (NULL == snd->di[0]);

	/* Tell DMA that we're not using framed/packet data */
	dma_ctrlflags(snd->di[0], DMA_CTRL_UNFRAMED /* mask */, DMA_CTRL_UNFRAMED /* value */);

	if (CHIPID(snd->sih->chip) == BCM4716_CHIP_ID) {
		/* for 471X chips, Turn on I2S pins. They're MUX'd with PFLASH pins, and PFLASH
		 * is ON by default
		 */
		ret = si_corereg(snd->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol),
	                 CCTRL_471X_I2S_PINS_ENABLE, CCTRL_471X_I2S_PINS_ENABLE);
	} else if (CHIPID(snd->sih->chip) == BCM5357_CHIP_ID) {
		/* Write to the 2nd chipcontrol reg. to turn on I2S pins */
		ret = si_pmu_chipcontrol(snd->sih, PMU1_PLL0_CHIPCTL1, CCTRL_5357_I2S_PINS_ENABLE,
		                         CCTRL_5357_I2S_PINS_ENABLE);
		/* Write to the 2nd chipcontrol reg. to turn on I2C-via-gpio pins */
		ret = si_pmu_chipcontrol(snd->sih, PMU1_PLL0_CHIPCTL1,
		                         CCTRL_5357_I2CSPI_PINS_ENABLE, 0);
	} else if ((CHIPID(snd->sih->chip) == BCM4707_CHIP_ID &&
			snd->sih->chippkg == BCM4709_PKG_ID) ||
			(CHIPID(snd->sih->chip) == BCM47094_CHIP_ID &&
			snd->sih->chippkg == BCM4709_PKG_ID)) {
		snd->irq = 120;
		/* Enable I2S clock. */
		{
#define I2S_M0_IDM_IO_CONTROL_DIRECT 0x18117408
		volatile uint32 *i2s_m0_idm_io_control_direct =
			(uint32 *)REG_MAP(I2S_M0_IDM_IO_CONTROL_DIRECT, sizeof(uint32));
		/*
		 * Gotta be a better way to set this!
		 */
		uint32 val = R_REG(snd->osh, i2s_m0_idm_io_control_direct);
		uint32 arcache = 0xb, awcache = 0x7;

		/* ARCACHE=0xb - Cacheable write-back, allocate on write
		 * AWCACHE=0x7 - Cacheable write-back, allocate on read
		 */
		if (arch_is_coherent()) {
			val &= ~((0xf << 16) | (0xf << 20));
			val |= (arcache << 16) | (awcache << 20);
		}
		W_REG(snd->osh, i2s_m0_idm_io_control_direct, val | 0x1);
		val = R_REG(snd->osh, i2s_m0_idm_io_control_direct);
		DBG("%s: idm_io_control_direct 0x%x\n", __FUNCTION__, val);
		REG_UNMAP(i2s_m0_idm_io_control_direct);
		}
	}

	return snd;
}

static void
bcm947xx_i2s_free(bcm947xx_i2s_info_t *sndbcm)
{
	osl_t *osh = sndbcm->osh;

	dma_detach(sndbcm->di[0]);

	si_detach(sndbcm->sih);

	MFREE(osh, sndbcm, sizeof(bcm947xx_i2s_info_t));

	osl_detach(osh);
}


static int __devinit
bcm947xx_i2s_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	bcm947xx_i2s_info_t *snd_bcm;
	int err = 0;

	DBG("%s: for pdev 0x%x w/irq %d.\n", __FUNCTION__, pdev->device, pdev->irq);

	if ((pdev->vendor != PCI_VENDOR_ID_BROADCOM) || (pdev->device != BCM47XX_AUDIO_ID)) {
		DBG("%s: early bailout pcideviceid mismatch -  0x%x.\n",
		       __FUNCTION__, pdev->device);
		return (-ENODEV);
	}

	err = pci_enable_device(pdev);
	if (err) {
		DBG("%s: Cannot enable device %d-%d_%d\n", __FUNCTION__,
		          pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
		return (-ENODEV);
	}
	pci_set_master(pdev);

	snd_bcm = bcm947xx_i2s_pci_attach(pdev->vendor, pdev->device, pci_resource_start(pdev, 0),
	                                  PCI_BUS, pdev, pdev->irq);
	if (!snd_bcm)
		return -ENODEV;

	bcm947xx_i2s_dai.dev = &pdev->dev;
	bcm947xx_i2s_dai.private_data = snd_bcm;
	err = snd_soc_register_dai(&bcm947xx_i2s_dai);
	if (err) {
		DBG("%s: Cannot register SOC DAI 0x%x\n", __FUNCTION__, err);
		bcm947xx_i2s_free(snd_bcm);
		return -ENODEV;
	}

	pci_set_drvdata(pdev, snd_bcm);

	return err;
}

static void __devexit bcm947xx_i2s_pci_remove(struct pci_dev *pdev, struct snd_soc_dai *dai)
{
	bcm947xx_i2s_info_t *sndbcm = (bcm947xx_i2s_info_t *) pci_get_drvdata(pdev);

	snd_soc_unregister_dai(&bcm947xx_i2s_dai);
	bcm947xx_i2s_free(sndbcm);
	pci_set_drvdata(pdev, NULL);
}


static struct pci_driver bcm947xx_i2s_pci_driver = {
	.name = BCM947XX_SND,
	.id_table = bcm947xx_i2s_pci_id_table,
	.probe = bcm947xx_i2s_pci_probe,
	.remove = __devexit_p(bcm947xx_i2s_pci_remove),
};

static int __init bcm947xx_i2s_pci_init(void)
{
	int err = 0;

	err = pci_register_driver(&bcm947xx_i2s_pci_driver);
	if (err) {
		printk(KERN_ERR "%s: Couldn't register bcm947xx_i2s_pci_driver 0x%x\n",
		       __FUNCTION__, err);
	}

	return err;
}

static void __exit bcm947xx_i2s_pci_exit(void)
{
	pci_unregister_driver(&bcm947xx_i2s_pci_driver);
}

module_init(bcm947xx_i2s_pci_init)
module_exit(bcm947xx_i2s_pci_exit)
