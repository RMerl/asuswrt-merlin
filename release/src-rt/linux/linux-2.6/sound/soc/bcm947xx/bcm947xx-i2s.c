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
 * $Id: bcm947xx-i2s.c,v 1.2 2009/11/12 22:26:07 Exp $
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>

#include <sound/driver.h>
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

bcm947xx_i2s_info_t *snd_bcm = NULL;
EXPORT_SYMBOL_GPL(snd_bcm);



static int bcm947xx_i2s_startup(struct snd_pcm_substream *substream)
{
	//DBG("%s\n", __FUNCTION__);
	return 0;
}

static void bcm947xx_i2s_shutdown(struct snd_pcm_substream *substream)
{
	//DBG("%s\n", __FUNCTION__);
	return;
}

static int bcm947xx_i2s_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (snd_bcm && snd_bcm->sih)
		if (si_findcoreidx(snd_bcm->sih, I2S_CORE_ID, 0) == BADIDX)
			ret = -EINVAL;

	return ret;
}


static int bcm947xx_i2s_suspend(struct platform_device *dev,
	struct snd_soc_cpu_dai *dai)
{
	DBG("%s - TBD\n", __FUNCTION__);
	return 0;
}

static int bcm947xx_i2s_resume(struct platform_device *dev,
	struct snd_soc_cpu_dai *dai)
{
	DBG("%s - TBD\n", __FUNCTION__);
	return 0;
}

static int bcm947xx_i2s_trigger(struct snd_pcm_substream *substream, int cmd)
{
	uint32 i2scontrol = R_REG(snd_bcm.osh, &snd_bcm->regs->i2scontrol);
	int ret = 0;

	DBG("%s w/cmd %d\n", __FUNCTION__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		i2scontrol |= I2S_CTRL_PLAYEN;
		W_REG(snd_bcm.osh, &snd_bcm->regs->i2scontrol, i2scontrol);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		i2scontrol &= ~I2S_CTRL_PLAYEN;
		W_REG(snd_bcm.osh, &snd_bcm->regs->i2scontrol, i2scontrol);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

/* Set I2S DAI format */
static int bcm947xx_i2s_set_fmt(struct snd_soc_cpu_dai *cpu_dai,
		unsigned int fmt)
{
	u32 devctrl = R_REG(snd_bcm.osh, &snd_bcm->regs->devcontrol);

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
	W_REG(snd_bcm.osh, &snd_bcm->regs->devcontrol, devctrl);

	return 0;
}


/*
 * Set Clock source
 */
static int bcm947xx_i2s_set_sysclk(struct snd_soc_cpu_dai *cpu_dai,
		int clk_id, unsigned int freq, int dir)
{
	/* Stash the MCLK rate that we're using, we can use it to help us to pick
	 * the right clkdiv settings later.
	 */
	snd_bcm->mclk = freq;

	return 0;
}

static int bcm947xx_i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	u32 devctrl = R_REG(snd_bcm.osh, &snd_bcm->regs->devcontrol);
	u32 clkdiv = R_REG(snd_bcm.osh, &snd_bcm->regs->clkdivider);
	u32 stxctrl = R_REG(snd_bcm.osh, &snd_bcm->regs->stxctrl);
	uint32 srate = 0;
	uint32 rate = params_rate(params);
	int channels = params_channels(params);
	int ii = 0;
	bool found = FALSE;

	/* Set up our ClockDivider register with audio sample rate */
	for (ii = 0; ii < ARRAY_SIZE(i2s_clkdiv_coeffs); ii++) {
		if ((i2s_clkdiv_coeffs[ii].rate == rate) &&
		    (i2s_clkdiv_coeffs[ii].mclk == snd_bcm->mclk)) {
			found = TRUE;
			break;
		}
	}

	if (found != TRUE) {
		printk(KERN_ERR "%s: unsupported audio sample rate %d Hz and mclk %d Hz "
		       "combination\n", __FUNCTION__, rate, snd_bcm->mclk);
		return -EINVAL;
	} else {
		/* Write the new SRATE into the clock divider register */
		srate = (i2s_clkdiv_coeffs[ii].srate << I2S_CLKDIV_SRATE_SHIFT);
		clkdiv &= ~I2S_CLKDIV_SRATE_MASK;
		W_REG(snd_bcm.osh, &snd_bcm->regs->clkdivider, clkdiv | srate);

		DBG("%s: i2s clkdivider 0x%x txplayth 0x%x\n", __FUNCTION__,
		    R_REG(snd_bcm.osh, &snd_bcm->regs->clkdivider),
		    R_REG(snd_bcm.osh, &snd_bcm->regs->txplayth));
		DBG("%s: audio sample rate %d Hz and mclk %d Hz\n",
		    __FUNCTION__, rate, snd_bcm.mclk);
	}

	DBG("%s: %d channels in this stream\n", __FUNCTION__, channels);

	/* Set up for the # of channels in this stream */
	/* For I2S/SPDIF we support 2 channel -OR- 6 (5.1) channels */
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

	DBG("%s: access 0x%x\n", __FUNCTION__, params_access(params));
	DBG("%s: format 0x%x\n", __FUNCTION__, params_format(params));
	DBG("%s: subformat 0x%x\n", __FUNCTION__, params_subformat(params));

	/* clear TX word length bits then Set the # of bits per sample in this stream */
	devctrl &= ~I2S_DC_WL_TX_MASK;
	stxctrl &= ~I2S_STXC_WL_MASK;
	switch (params_format(params)) {
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
		DBG("unsupported format\n");
		break;
	}

	/* For now, we're only interested in Tx so we'll set up half-duplex Tx-only */
	devctrl &= ~I2S_DC_DPX_MASK;

	/* Write I2S devcontrol reg */
	W_REG(snd_bcm.osh, &snd_bcm->regs->devcontrol, devctrl);
	W_REG(snd_bcm.osh, &snd_bcm->regs->stxctrl, stxctrl);

	return 0;
}

#define BCM947XX_I2S_RATES \
        (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 | \
        SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_32000 | \
        SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000)

#define BCM947XX_I2S_FORMATS \
        (SNDRV_PCM_FMTBIT_S8  | SNDRV_PCM_FMTBIT_U8 | \
         SNDRV_PCM_FMTBIT_S16 | SNDRV_PCM_FMTBIT_U16 | \
         SNDRV_PCM_FMTBIT_S24 | SNDRV_PCM_FMTBIT_U24 | \
         SNDRV_PCM_FMTBIT_S32 | SNDRV_PCM_FMTBIT_U32)

struct snd_soc_cpu_dai bcm947xx_i2s_dai = {
	.name = "bcm947xx-i2s",
	.id = 0,
	.type = SND_SOC_DAI_I2S,
	.probe = bcm947xx_i2s_probe,
	.suspend = bcm947xx_i2s_suspend,
	.resume = bcm947xx_i2s_resume,
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = BCM947XX_I2S_RATES,
		.formats = BCM947XX_I2S_FORMATS,},
	.ops = {
		.startup = bcm947xx_i2s_startup,
		.shutdown = bcm947xx_i2s_shutdown,
		.trigger = bcm947xx_i2s_trigger,
		.hw_params = bcm947xx_i2s_hw_params,},
	.dai_ops = {
		.set_fmt = bcm947xx_i2s_set_fmt,
		.set_sysclk = bcm947xx_i2s_set_sysclk,
	},
};

EXPORT_SYMBOL_GPL(bcm947xx_i2s_dai);


MODULE_LICENSE("GPL");
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

static bcm947xx_i2s_info_t *
bcm947xx_i2s_pci_attach(uint16 vendor, uint16 device, ulong regs, uint bustype, void *btparam,
                        uint irq)
{
	osl_t *osh = NULL;
	bcm947xx_i2s_info_t *snd = NULL;
	int ret;

	uint addrwidth;
	int dma_attach_err = 0;


	DBG("%s: vendor 0x%x device 0x%x regs 0x%x bustype 0x%x btparam %p irq 0x%x\n",
	    __FUNCTION__, vendor, device, regs, bustype, btparam, irq);


	osh = osl_attach(btparam, bustype, FALSE);
	ASSERT(osh);

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
	snd->irq = irq;

	/*
	 * Do the hardware portion of the attach.
	 * Also initialize software state that depends on the particular hardware
	 * we are running.
	 */
	snd->sih = si_attach((uint)device, snd->osh, snd->regsva, bustype, btparam,
	                        NULL, NULL);

	snd->regs = (i2sregs_t *)si_setcore(snd->sih, I2S_CORE_ID, 0);

	addrwidth = dma_addrwidth(snd->sih, DMAREG(snd, DMA_TX, 0));

	snd->di[0] = dma_attach(snd->osh, "i2s_dma", snd->sih,
	                            DMAREG(snd, DMA_TX, 0),
	                            NULL, 64, 0,
	                            0, -1, 0, 0, NULL);

	dma_attach_err |= (NULL == snd->di[0]);

	/* Tell DMA that we're not using framed/packet data */
	dma_ctrlflags(snd->di[0], DMA_CTRL_UNFRAMED /* mask */, DMA_CTRL_UNFRAMED /* value */);

	/* for 471X chips, Turn on I2S pins. They're MUX'd with PFLASH pins, and PFLASH is ON
	 * by default
	 */
	if (CHIPID(snd->sih->chip) == BCM4716_CHIP_ID) {
		ret = si_corereg(snd->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol),
	                 CCTRL471X_I2S_PINS_ENABLE, CCTRL471X_I2S_PINS_ENABLE);
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

	pci_set_drvdata(pdev, snd_bcm);
	DBG("%s: snd_bcm @ %p snd_bcm.regs @ %p\n", __FUNCTION__, snd_bcm, snd_bcm.regs);

	return err;
}

static void __devexit bcm947xx_i2s_pci_remove(struct pci_dev *pdev)
{
	bcm947xx_i2s_info_t *sndbcm = (bcm947xx_i2s_info_t *) pci_get_drvdata(pdev);

	bcm947xx_i2s_free(sndbcm);
	snd_bcm = (bcm947xx_i2s_info_t *)NULL;
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
	return pci_register_driver(&bcm947xx_i2s_pci_driver);
}

static void __exit bcm947xx_i2s_pci_exit(void)
{
	pci_unregister_driver(&bcm947xx_i2s_pci_driver);
}

module_init(bcm947xx_i2s_pci_init)
module_exit(bcm947xx_i2s_pci_exit)
