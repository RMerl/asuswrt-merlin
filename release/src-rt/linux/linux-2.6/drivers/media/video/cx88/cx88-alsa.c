/*
 *
 *  Support for audio capture
 *  PCI function #1 of the cx2388x.
 *
 *    (c) 2007 Trent Piepho <xyzzy@speakeasy.org>
 *    (c) 2005,2006 Ricardo Cerqueira <v4l@cerqueira.org>
 *    (c) 2005 Mauro Carvalho Chehab <mchehab@infradead.org>
 *    Based on a dummy cx88 module by Gerd Knorr <kraxel@bytesex.org>
 *    Based on dummy.c by Jaroslav Kysela <perex@perex.cz>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/pci.h>
#include <linux/slab.h>

#include <asm/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/control.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <media/wm8775.h>

#include "cx88.h"
#include "cx88-reg.h"

#define dprintk(level,fmt, arg...)	if (debug >= level) \
	printk(KERN_INFO "%s/1: " fmt, chip->core->name , ## arg)

#define dprintk_core(level,fmt, arg...)	if (debug >= level) \
	printk(KERN_DEBUG "%s/1: " fmt, chip->core->name , ## arg)

/****************************************************************************
	Data type declarations - Can be moded to a header file later
 ****************************************************************************/

struct cx88_audio_buffer {
	unsigned int               bpl;
	struct btcx_riscmem        risc;
	struct videobuf_dmabuf     dma;
};

struct cx88_audio_dev {
	struct cx88_core           *core;
	struct cx88_dmaqueue       q;

	/* pci i/o */
	struct pci_dev             *pci;

	/* audio controls */
	int                        irq;

	struct snd_card            *card;

	spinlock_t                 reg_lock;
	atomic_t		   count;

	unsigned int               dma_size;
	unsigned int               period_size;
	unsigned int               num_periods;

	struct videobuf_dmabuf     *dma_risc;

	struct cx88_audio_buffer   *buf;

	struct snd_pcm_substream   *substream;
};
typedef struct cx88_audio_dev snd_cx88_card_t;



/****************************************************************************
			Module global static vars
 ****************************************************************************/

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;	/* Index 0-MAX */
static const char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;	/* ID for this card */
static int enable[SNDRV_CARDS] = {1, [1 ... (SNDRV_CARDS - 1)] = 1};

module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable cx88x soundcard. default enabled.");

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for cx88x capture interface(s).");


/****************************************************************************
				Module macros
 ****************************************************************************/

MODULE_DESCRIPTION("ALSA driver module for cx2388x based TV cards");
MODULE_AUTHOR("Ricardo Cerqueira");
MODULE_AUTHOR("Mauro Carvalho Chehab <mchehab@infradead.org>");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("{{Conexant,23881},"
			"{{Conexant,23882},"
			"{{Conexant,23883}");
static unsigned int debug;
module_param(debug,int,0644);
MODULE_PARM_DESC(debug,"enable debug messages");

/****************************************************************************
			Module specific funtions
 ****************************************************************************/

/*
 * BOARD Specific: Sets audio DMA
 */

static int _cx88_start_audio_dma(snd_cx88_card_t *chip)
{
	struct cx88_audio_buffer *buf = chip->buf;
	struct cx88_core *core=chip->core;
	const struct sram_channel *audio_ch = &cx88_sram_channels[SRAM_CH25];

	/* Make sure RISC/FIFO are off before changing FIFO/RISC settings */
	cx_clear(MO_AUD_DMACNTRL, 0x11);

	/* setup fifo + format - out channel */
	cx88_sram_channel_setup(chip->core, audio_ch, buf->bpl, buf->risc.dma);

	/* sets bpl size */
	cx_write(MO_AUDD_LNGTH, buf->bpl);

	/* reset counter */
	cx_write(MO_AUDD_GPCNTRL, GP_COUNT_CONTROL_RESET);
	atomic_set(&chip->count, 0);

	dprintk(1, "Start audio DMA, %d B/line, %d lines/FIFO, %d periods, %d "
		"byte buffer\n", buf->bpl, cx_read(audio_ch->cmds_start + 8)>>1,
		chip->num_periods, buf->bpl * chip->num_periods);

	/* Enables corresponding bits at AUD_INT_STAT */
	cx_write(MO_AUD_INTMSK, AUD_INT_OPC_ERR | AUD_INT_DN_SYNC |
				AUD_INT_DN_RISCI2 | AUD_INT_DN_RISCI1);

	/* Clean any pending interrupt bits already set */
	cx_write(MO_AUD_INTSTAT, ~0);

	/* enable audio irqs */
	cx_set(MO_PCI_INTMSK, chip->core->pci_irqmask | PCI_INT_AUDINT);

	/* start dma */
	cx_set(MO_DEV_CNTRL2, (1<<5)); /* Enables Risc Processor */
	cx_set(MO_AUD_DMACNTRL, 0x11); /* audio downstream FIFO and RISC enable */

	if (debug)
		cx88_sram_channel_dump(chip->core, audio_ch);

	return 0;
}

/*
 * BOARD Specific: Resets audio DMA
 */
static int _cx88_stop_audio_dma(snd_cx88_card_t *chip)
{
	struct cx88_core *core=chip->core;
	dprintk(1, "Stopping audio DMA\n");

	/* stop dma */
	cx_clear(MO_AUD_DMACNTRL, 0x11);

	/* disable irqs */
	cx_clear(MO_PCI_INTMSK, PCI_INT_AUDINT);
	cx_clear(MO_AUD_INTMSK, AUD_INT_OPC_ERR | AUD_INT_DN_SYNC |
				AUD_INT_DN_RISCI2 | AUD_INT_DN_RISCI1);

	if (debug)
		cx88_sram_channel_dump(chip->core, &cx88_sram_channels[SRAM_CH25]);

	return 0;
}

#define MAX_IRQ_LOOP 50

/*
 * BOARD Specific: IRQ dma bits
 */
static const char *cx88_aud_irqs[32] = {
	"dn_risci1", "up_risci1", "rds_dn_risc1", /* 0-2 */
	NULL,					  /* reserved */
	"dn_risci2", "up_risci2", "rds_dn_risc2", /* 4-6 */
	NULL,					  /* reserved */
	"dnf_of", "upf_uf", "rds_dnf_uf",	  /* 8-10 */
	NULL,					  /* reserved */
	"dn_sync", "up_sync", "rds_dn_sync",	  /* 12-14 */
	NULL,					  /* reserved */
	"opc_err", "par_err", "rip_err",	  /* 16-18 */
	"pci_abort", "ber_irq", "mchg_irq"	  /* 19-21 */
};

/*
 * BOARD Specific: Threats IRQ audio specific calls
 */
static void cx8801_aud_irq(snd_cx88_card_t *chip)
{
	struct cx88_core *core = chip->core;
	u32 status, mask;

	status = cx_read(MO_AUD_INTSTAT);
	mask   = cx_read(MO_AUD_INTMSK);
	if (0 == (status & mask))
		return;
	cx_write(MO_AUD_INTSTAT, status);
	if (debug > 1  ||  (status & mask & ~0xff))
		cx88_print_irqbits(core->name, "irq aud",
				   cx88_aud_irqs, ARRAY_SIZE(cx88_aud_irqs),
				   status, mask);
	/* risc op code error */
	if (status & AUD_INT_OPC_ERR) {
		printk(KERN_WARNING "%s/1: Audio risc op code error\n",core->name);
		cx_clear(MO_AUD_DMACNTRL, 0x11);
		cx88_sram_channel_dump(core, &cx88_sram_channels[SRAM_CH25]);
	}
	if (status & AUD_INT_DN_SYNC) {
		dprintk(1, "Downstream sync error\n");
		cx_write(MO_AUDD_GPCNTRL, GP_COUNT_CONTROL_RESET);
		return;
	}
	/* risc1 downstream */
	if (status & AUD_INT_DN_RISCI1) {
		atomic_set(&chip->count, cx_read(MO_AUDD_GPCNT));
		snd_pcm_period_elapsed(chip->substream);
	}
	/* FIXME: Any other status should deserve a special handling? */
}

/*
 * BOARD Specific: Handles IRQ calls
 */
static irqreturn_t cx8801_irq(int irq, void *dev_id)
{
	snd_cx88_card_t *chip = dev_id;
	struct cx88_core *core = chip->core;
	u32 status;
	int loop, handled = 0;

	for (loop = 0; loop < MAX_IRQ_LOOP; loop++) {
		status = cx_read(MO_PCI_INTSTAT) &
			(core->pci_irqmask | PCI_INT_AUDINT);
		if (0 == status)
			goto out;
		dprintk(3, "cx8801_irq loop %d/%d, status %x\n",
			loop, MAX_IRQ_LOOP, status);
		handled = 1;
		cx_write(MO_PCI_INTSTAT, status);

		if (status & core->pci_irqmask)
			cx88_core_irq(core, status);
		if (status & PCI_INT_AUDINT)
			cx8801_aud_irq(chip);
	}

	if (MAX_IRQ_LOOP == loop) {
		printk(KERN_ERR
		       "%s/1: IRQ loop detected, disabling interrupts\n",
		       core->name);
		cx_clear(MO_PCI_INTMSK, PCI_INT_AUDINT);
	}

 out:
	return IRQ_RETVAL(handled);
}


static int dsp_buffer_free(snd_cx88_card_t *chip)
{
	BUG_ON(!chip->dma_size);

	dprintk(2,"Freeing buffer\n");
	videobuf_dma_unmap(&chip->pci->dev, chip->dma_risc);
	videobuf_dma_free(chip->dma_risc);
	btcx_riscmem_free(chip->pci,&chip->buf->risc);
	kfree(chip->buf);

	chip->dma_risc = NULL;
	chip->dma_size = 0;

	return 0;
}

/****************************************************************************
				ALSA PCM Interface
 ****************************************************************************/

/*
 * Digital hardware definition
 */
#define DEFAULT_FIFO_SIZE	4096
static const struct snd_pcm_hardware snd_cx88_digital_hw = {
	.info = SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_MMAP_VALID,
	.formats = SNDRV_PCM_FMTBIT_S16_LE,

	.rates =		SNDRV_PCM_RATE_48000,
	.rate_min =		48000,
	.rate_max =		48000,
	.channels_min = 2,
	.channels_max = 2,
	/* Analog audio output will be full of clicks and pops if there
	   are not exactly four lines in the SRAM FIFO buffer.  */
	.period_bytes_min = DEFAULT_FIFO_SIZE/4,
	.period_bytes_max = DEFAULT_FIFO_SIZE/4,
	.periods_min = 1,
	.periods_max = 1024,
	.buffer_bytes_max = (1024*1024),
};

/*
 * audio pcm capture open callback
 */
static int snd_cx88_pcm_open(struct snd_pcm_substream *substream)
{
	snd_cx88_card_t *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	int err;

	if (!chip) {
		printk(KERN_ERR "BUG: cx88 can't find device struct."
				" Can't proceed with open\n");
		return -ENODEV;
	}

	err = snd_pcm_hw_constraint_pow2(runtime, 0, SNDRV_PCM_HW_PARAM_PERIODS);
	if (err < 0)
		goto _error;

	chip->substream = substream;

	runtime->hw = snd_cx88_digital_hw;

	if (cx88_sram_channels[SRAM_CH25].fifo_size != DEFAULT_FIFO_SIZE) {
		unsigned int bpl = cx88_sram_channels[SRAM_CH25].fifo_size / 4;
		bpl &= ~7; /* must be multiple of 8 */
		runtime->hw.period_bytes_min = bpl;
		runtime->hw.period_bytes_max = bpl;
	}

	return 0;
_error:
	dprintk(1,"Error opening PCM!\n");
	return err;
}

/*
 * audio close callback
 */
static int snd_cx88_close(struct snd_pcm_substream *substream)
{
	return 0;
}

/*
 * hw_params callback
 */
static int snd_cx88_hw_params(struct snd_pcm_substream * substream,
			      struct snd_pcm_hw_params * hw_params)
{
	snd_cx88_card_t *chip = snd_pcm_substream_chip(substream);
	struct videobuf_dmabuf *dma;

	struct cx88_audio_buffer *buf;
	int ret;

	if (substream->runtime->dma_area) {
		dsp_buffer_free(chip);
		substream->runtime->dma_area = NULL;
	}

	chip->period_size = params_period_bytes(hw_params);
	chip->num_periods = params_periods(hw_params);
	chip->dma_size = chip->period_size * params_periods(hw_params);

	BUG_ON(!chip->dma_size);
	BUG_ON(chip->num_periods & (chip->num_periods-1));

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (NULL == buf)
		return -ENOMEM;

	buf->bpl = chip->period_size;

	dma = &buf->dma;
	videobuf_dma_init(dma);
	ret = videobuf_dma_init_kernel(dma, PCI_DMA_FROMDEVICE,
			(PAGE_ALIGN(chip->dma_size) >> PAGE_SHIFT));
	if (ret < 0)
		goto error;

	ret = videobuf_dma_map(&chip->pci->dev, dma);
	if (ret < 0)
		goto error;

	ret = cx88_risc_databuffer(chip->pci, &buf->risc, dma->sglist,
				   chip->period_size, chip->num_periods, 1);
	if (ret < 0)
		goto error;

	/* Loop back to start of program */
	buf->risc.jmp[0] = cpu_to_le32(RISC_JUMP|RISC_IRQ1|RISC_CNT_INC);
	buf->risc.jmp[1] = cpu_to_le32(buf->risc.dma);

	chip->buf = buf;
	chip->dma_risc = dma;

	substream->runtime->dma_area = chip->dma_risc->vaddr;
	substream->runtime->dma_bytes = chip->dma_size;
	substream->runtime->dma_addr = 0;
	return 0;

error:
	kfree(buf);
	return ret;
}

/*
 * hw free callback
 */
static int snd_cx88_hw_free(struct snd_pcm_substream * substream)
{

	snd_cx88_card_t *chip = snd_pcm_substream_chip(substream);

	if (substream->runtime->dma_area) {
		dsp_buffer_free(chip);
		substream->runtime->dma_area = NULL;
	}

	return 0;
}

/*
 * prepare callback
 */
static int snd_cx88_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

/*
 * trigger callback
 */
static int snd_cx88_card_trigger(struct snd_pcm_substream *substream, int cmd)
{
	snd_cx88_card_t *chip = snd_pcm_substream_chip(substream);
	int err;

	/* Local interrupts are already disabled by ALSA */
	spin_lock(&chip->reg_lock);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		err=_cx88_start_audio_dma(chip);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		err=_cx88_stop_audio_dma(chip);
		break;
	default:
		err=-EINVAL;
		break;
	}

	spin_unlock(&chip->reg_lock);

	return err;
}

/*
 * pointer callback
 */
static snd_pcm_uframes_t snd_cx88_pointer(struct snd_pcm_substream *substream)
{
	snd_cx88_card_t *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	u16 count;

	count = atomic_read(&chip->count);

//	dprintk(2, "%s - count %d (+%u), period %d, frame %lu\n", __func__,
//		count, new, count & (runtime->periods-1),
//		runtime->period_size * (count & (runtime->periods-1)));
	return runtime->period_size * (count & (runtime->periods-1));
}

/*
 * page callback (needed for mmap)
 */
static struct page *snd_cx88_page(struct snd_pcm_substream *substream,
				unsigned long offset)
{
	void *pageptr = substream->runtime->dma_area + offset;
	return vmalloc_to_page(pageptr);
}

/*
 * operators
 */
static struct snd_pcm_ops snd_cx88_pcm_ops = {
	.open = snd_cx88_pcm_open,
	.close = snd_cx88_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = snd_cx88_hw_params,
	.hw_free = snd_cx88_hw_free,
	.prepare = snd_cx88_prepare,
	.trigger = snd_cx88_card_trigger,
	.pointer = snd_cx88_pointer,
	.page = snd_cx88_page,
};

/*
 * create a PCM device
 */
static int __devinit snd_cx88_pcm(snd_cx88_card_t *chip, int device, const char *name)
{
	int err;
	struct snd_pcm *pcm;

	err = snd_pcm_new(chip->card, name, device, 0, 1, &pcm);
	if (err < 0)
		return err;
	pcm->private_data = chip;
	strcpy(pcm->name, name);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &snd_cx88_pcm_ops);

	return 0;
}

/****************************************************************************
				CONTROL INTERFACE
 ****************************************************************************/
static int snd_cx88_volume_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *info)
{
	info->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	info->count = 2;
	info->value.integer.min = 0;
	info->value.integer.max = 0x3f;

	return 0;
}

static int snd_cx88_volume_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *value)
{
	snd_cx88_card_t *chip = snd_kcontrol_chip(kcontrol);
	struct cx88_core *core=chip->core;
	int vol = 0x3f - (cx_read(AUD_VOL_CTL) & 0x3f),
	    bal = cx_read(AUD_BAL_CTL);

	value->value.integer.value[(bal & 0x40) ? 0 : 1] = vol;
	vol -= (bal & 0x3f);
	value->value.integer.value[(bal & 0x40) ? 1 : 0] = vol < 0 ? 0 : vol;

	return 0;
}

static void snd_cx88_wm8775_volume_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *value)
{
	snd_cx88_card_t *chip = snd_kcontrol_chip(kcontrol);
	struct cx88_core *core = chip->core;
	struct v4l2_control client_ctl;
	int left = value->value.integer.value[0];
	int right = value->value.integer.value[1];
	int v, b;

	memset(&client_ctl, 0, sizeof(client_ctl));

	/* Pass volume & balance onto any WM8775 */
	if (left >= right) {
		v = left << 10;
		b = left ? (0x8000 * right) / left : 0x8000;
	} else {
		v = right << 10;
		b = right ? 0xffff - (0x8000 * left) / right : 0x8000;
	}
	client_ctl.value = v;
	client_ctl.id = V4L2_CID_AUDIO_VOLUME;
	call_hw(core, WM8775_GID, core, s_ctrl, &client_ctl);

	client_ctl.value = b;
	client_ctl.id = V4L2_CID_AUDIO_BALANCE;
	call_hw(core, WM8775_GID, core, s_ctrl, &client_ctl);
}

/* OK - TODO: test it */
static int snd_cx88_volume_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *value)
{
	snd_cx88_card_t *chip = snd_kcontrol_chip(kcontrol);
	struct cx88_core *core=chip->core;
	int left, right, v, b;
	int changed = 0;
	u32 old;

	if (core->board.audio_chip == V4L2_IDENT_WM8775)
		snd_cx88_wm8775_volume_put(kcontrol, value);

	left = value->value.integer.value[0] & 0x3f;
	right = value->value.integer.value[1] & 0x3f;
	b = right - left;
	if (b < 0) {
		v = 0x3f - left;
		b = (-b) | 0x40;
	} else {
		v = 0x3f - right;
	}
	/* Do we really know this will always be called with IRQs on? */
	spin_lock_irq(&chip->reg_lock);
	old = cx_read(AUD_VOL_CTL);
	if (v != (old & 0x3f)) {
		cx_swrite(SHADOW_AUD_VOL_CTL, AUD_VOL_CTL, (old & ~0x3f) | v);
		changed = 1;
	}
	if ((cx_read(AUD_BAL_CTL) & 0x7f) != b) {
		cx_write(AUD_BAL_CTL, b);
		changed = 1;
	}
	spin_unlock_irq(&chip->reg_lock);

	return changed;
}

static const DECLARE_TLV_DB_SCALE(snd_cx88_db_scale, -6300, 100, 0);

static const struct snd_kcontrol_new snd_cx88_volume = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE |
		  SNDRV_CTL_ELEM_ACCESS_TLV_READ,
	.name = "Analog-TV Volume",
	.info = snd_cx88_volume_info,
	.get = snd_cx88_volume_get,
	.put = snd_cx88_volume_put,
	.tlv.p = snd_cx88_db_scale,
};

static int snd_cx88_switch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *value)
{
	snd_cx88_card_t *chip = snd_kcontrol_chip(kcontrol);
	struct cx88_core *core = chip->core;
	u32 bit = kcontrol->private_value;

	value->value.integer.value[0] = !(cx_read(AUD_VOL_CTL) & bit);
	return 0;
}

static int snd_cx88_switch_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *value)
{
	snd_cx88_card_t *chip = snd_kcontrol_chip(kcontrol);
	struct cx88_core *core = chip->core;
	u32 bit = kcontrol->private_value;
	int ret = 0;
	u32 vol;

	spin_lock_irq(&chip->reg_lock);
	vol = cx_read(AUD_VOL_CTL);
	if (value->value.integer.value[0] != !(vol & bit)) {
		vol ^= bit;
		cx_swrite(SHADOW_AUD_VOL_CTL, AUD_VOL_CTL, vol);
		/* Pass mute onto any WM8775 */
		if ((core->board.audio_chip == V4L2_IDENT_WM8775) &&
		    ((1<<6) == bit)) {
			struct v4l2_control client_ctl;

			memset(&client_ctl, 0, sizeof(client_ctl));
			client_ctl.value = 0 != (vol & bit);
			client_ctl.id = V4L2_CID_AUDIO_MUTE;
			call_hw(core, WM8775_GID, core, s_ctrl, &client_ctl);
		}
		ret = 1;
	}
	spin_unlock_irq(&chip->reg_lock);
	return ret;
}

static const struct snd_kcontrol_new snd_cx88_dac_switch = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Audio-Out Switch",
	.info = snd_ctl_boolean_mono_info,
	.get = snd_cx88_switch_get,
	.put = snd_cx88_switch_put,
	.private_value = (1<<8),
};

static const struct snd_kcontrol_new snd_cx88_source_switch = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Analog-TV Switch",
	.info = snd_ctl_boolean_mono_info,
	.get = snd_cx88_switch_get,
	.put = snd_cx88_switch_put,
	.private_value = (1<<6),
};

static int snd_cx88_alc_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *value)
{
	snd_cx88_card_t *chip = snd_kcontrol_chip(kcontrol);
	struct cx88_core *core = chip->core;
	struct v4l2_control client_ctl;

	memset(&client_ctl, 0, sizeof(client_ctl));
	client_ctl.id = V4L2_CID_AUDIO_LOUDNESS;
	call_hw(core, WM8775_GID, core, g_ctrl, &client_ctl);
	value->value.integer.value[0] = client_ctl.value ? 1 : 0;

	return 0;
}

static int snd_cx88_alc_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *value)
{
	snd_cx88_card_t *chip = snd_kcontrol_chip(kcontrol);
	struct cx88_core *core = chip->core;
	struct v4l2_control client_ctl;

	memset(&client_ctl, 0, sizeof(client_ctl));
	client_ctl.value = 0 != value->value.integer.value[0];
	client_ctl.id = V4L2_CID_AUDIO_LOUDNESS;
	call_hw(core, WM8775_GID, core, s_ctrl, &client_ctl);

	return 0;
}

static struct snd_kcontrol_new snd_cx88_alc_switch = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Line-In ALC Switch",
	.info = snd_ctl_boolean_mono_info,
	.get = snd_cx88_alc_get,
	.put = snd_cx88_alc_put,
};

/****************************************************************************
			Basic Flow for Sound Devices
 ****************************************************************************/

/*
 * PCI ID Table - 14f1:8801 and 14f1:8811 means function 1: Audio
 * Only boards with eeprom and byte 1 at eeprom=1 have it
 */

static const struct pci_device_id const cx88_audio_pci_tbl[] __devinitdata = {
	{0x14f1,0x8801,PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{0x14f1,0x8811,PCI_ANY_ID,PCI_ANY_ID,0,0,0},
	{0, }
};
MODULE_DEVICE_TABLE(pci, cx88_audio_pci_tbl);

/*
 * Chip-specific destructor
 */

static int snd_cx88_free(snd_cx88_card_t *chip)
{

	if (chip->irq >= 0)
		free_irq(chip->irq, chip);

	cx88_core_put(chip->core,chip->pci);

	pci_disable_device(chip->pci);
	return 0;
}

/*
 * Component Destructor
 */
static void snd_cx88_dev_free(struct snd_card * card)
{
	snd_cx88_card_t *chip = card->private_data;

	snd_cx88_free(chip);
}


/*
 * Alsa Constructor - Component probe
 */

static int devno;
static int __devinit snd_cx88_create(struct snd_card *card,
				     struct pci_dev *pci,
				     snd_cx88_card_t **rchip,
				     struct cx88_core **core_ptr)
{
	snd_cx88_card_t   *chip;
	struct cx88_core  *core;
	int               err;
	unsigned char     pci_lat;

	*rchip = NULL;

	err = pci_enable_device(pci);
	if (err < 0)
		return err;

	pci_set_master(pci);

	chip = card->private_data;

	core = cx88_core_get(pci);
	if (NULL == core) {
		err = -EINVAL;
		return err;
	}

	if (!pci_dma_supported(pci,DMA_BIT_MASK(32))) {
		dprintk(0, "%s/1: Oops: no 32bit PCI DMA ???\n",core->name);
		err = -EIO;
		cx88_core_put(core, pci);
		return err;
	}


	/* pci init */
	chip->card = card;
	chip->pci = pci;
	chip->irq = -1;
	spin_lock_init(&chip->reg_lock);

	chip->core = core;

	/* get irq */
	err = request_irq(chip->pci->irq, cx8801_irq,
			  IRQF_SHARED | IRQF_DISABLED, chip->core->name, chip);
	if (err < 0) {
		dprintk(0, "%s: can't get IRQ %d\n",
		       chip->core->name, chip->pci->irq);
		return err;
	}

	/* print pci info */
	pci_read_config_byte(pci, PCI_LATENCY_TIMER, &pci_lat);

	dprintk(1,"ALSA %s/%i: found at %s, rev: %d, irq: %d, "
	       "latency: %d, mmio: 0x%llx\n", core->name, devno,
	       pci_name(pci), pci->revision, pci->irq,
	       pci_lat, (unsigned long long)pci_resource_start(pci,0));

	chip->irq = pci->irq;
	synchronize_irq(chip->irq);

	snd_card_set_dev(card, &pci->dev);

	*rchip = chip;
	*core_ptr = core;

	return 0;
}

static int __devinit cx88_audio_initdev(struct pci_dev *pci,
				    const struct pci_device_id *pci_id)
{
	struct snd_card  *card;
	snd_cx88_card_t  *chip;
	struct cx88_core *core = NULL;
	int              err;

	if (devno >= SNDRV_CARDS)
		return (-ENODEV);

	if (!enable[devno]) {
		++devno;
		return (-ENOENT);
	}

	err = snd_card_create(index[devno], id[devno], THIS_MODULE,
			      sizeof(snd_cx88_card_t), &card);
	if (err < 0)
		return err;

	card->private_free = snd_cx88_dev_free;

	err = snd_cx88_create(card, pci, &chip, &core);
	if (err < 0)
		goto error;

	err = snd_cx88_pcm(chip, 0, "CX88 Digital");
	if (err < 0)
		goto error;

	err = snd_ctl_add(card, snd_ctl_new1(&snd_cx88_volume, chip));
	if (err < 0)
		goto error;
	err = snd_ctl_add(card, snd_ctl_new1(&snd_cx88_dac_switch, chip));
	if (err < 0)
		goto error;
	err = snd_ctl_add(card, snd_ctl_new1(&snd_cx88_source_switch, chip));
	if (err < 0)
		goto error;

	/* If there's a wm8775 then add a Line-In ALC switch */
	if (core->board.audio_chip == V4L2_IDENT_WM8775)
		snd_ctl_add(card, snd_ctl_new1(&snd_cx88_alc_switch, chip));

	strcpy (card->driver, "CX88x");
	sprintf(card->shortname, "Conexant CX%x", pci->device);
	sprintf(card->longname, "%s at %#llx",
		card->shortname,(unsigned long long)pci_resource_start(pci, 0));
	strcpy (card->mixername, "CX88");

	dprintk (0, "%s/%i: ALSA support for cx2388x boards\n",
	       card->driver,devno);

	err = snd_card_register(card);
	if (err < 0)
		goto error;
	pci_set_drvdata(pci,card);

	devno++;
	return 0;

error:
	snd_card_free(card);
	return err;
}
/*
 * ALSA destructor
 */
static void __devexit cx88_audio_finidev(struct pci_dev *pci)
{
	struct cx88_audio_dev *card = pci_get_drvdata(pci);

	snd_card_free((void *)card);

	pci_set_drvdata(pci, NULL);

	devno--;
}

/*
 * PCI driver definition
 */

static struct pci_driver cx88_audio_pci_driver = {
	.name     = "cx88_audio",
	.id_table = cx88_audio_pci_tbl,
	.probe    = cx88_audio_initdev,
	.remove   = __devexit_p(cx88_audio_finidev),
};

/****************************************************************************
				LINUX MODULE INIT
 ****************************************************************************/

/*
 * module init
 */
static int __init cx88_audio_init(void)
{
	printk(KERN_INFO "cx2388x alsa driver version %d.%d.%d loaded\n",
	       (CX88_VERSION_CODE >> 16) & 0xff,
	       (CX88_VERSION_CODE >>  8) & 0xff,
	       CX88_VERSION_CODE & 0xff);
#ifdef SNAPSHOT
	printk(KERN_INFO "cx2388x: snapshot date %04d-%02d-%02d\n",
	       SNAPSHOT/10000, (SNAPSHOT/100)%100, SNAPSHOT%100);
#endif
	return pci_register_driver(&cx88_audio_pci_driver);
}

/*
 * module remove
 */
static void __exit cx88_audio_fini(void)
{
	pci_unregister_driver(&cx88_audio_pci_driver);
}

module_init(cx88_audio_init);
module_exit(cx88_audio_fini);

/* ----------------------------------------------------------- */
/*
 * Local variables:
 * c-basic-offset: 8
 * End:
 */
