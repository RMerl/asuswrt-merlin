/*
 *  Routines for control of the AK4113 via I2C/4-wire serial interface
 *  IEC958 (S/PDIF) receiver by Asahi Kasei
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *  Copyright (c) by Pavel Hofman <pavel.hofman@ivitera.com>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/slab.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/ak4113.h>
#include <sound/asoundef.h>
#include <sound/info.h>

MODULE_AUTHOR("Pavel Hofman <pavel.hofman@ivitera.com>");
MODULE_DESCRIPTION("AK4113 IEC958 (S/PDIF) receiver by Asahi Kasei");
MODULE_LICENSE("GPL");

#define AK4113_ADDR			0x00 /* fixed address */

static void ak4113_stats(struct work_struct *work);
static void ak4113_init_regs(struct ak4113 *chip);


static void reg_write(struct ak4113 *ak4113, unsigned char reg,
		unsigned char val)
{
	ak4113->write(ak4113->private_data, reg, val);
	if (reg < sizeof(ak4113->regmap))
		ak4113->regmap[reg] = val;
}

static inline unsigned char reg_read(struct ak4113 *ak4113, unsigned char reg)
{
	return ak4113->read(ak4113->private_data, reg);
}

static void snd_ak4113_free(struct ak4113 *chip)
{
	chip->init = 1;	/* don't schedule new work */
	mb();
	cancel_delayed_work(&chip->work);
	flush_scheduled_work();
	kfree(chip);
}

static int snd_ak4113_dev_free(struct snd_device *device)
{
	struct ak4113 *chip = device->device_data;
	snd_ak4113_free(chip);
	return 0;
}

int snd_ak4113_create(struct snd_card *card, ak4113_read_t *read,
		ak4113_write_t *write, const unsigned char *pgm,
		void *private_data, struct ak4113 **r_ak4113)
{
	struct ak4113 *chip;
	int err = 0;
	unsigned char reg;
	static struct snd_device_ops ops = {
		.dev_free =     snd_ak4113_dev_free,
	};

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (chip == NULL)
		return -ENOMEM;
	spin_lock_init(&chip->lock);
	chip->card = card;
	chip->read = read;
	chip->write = write;
	chip->private_data = private_data;
	INIT_DELAYED_WORK(&chip->work, ak4113_stats);

	for (reg = 0; reg < AK4113_WRITABLE_REGS ; reg++)
		chip->regmap[reg] = pgm[reg];
	ak4113_init_regs(chip);

	chip->rcs0 = reg_read(chip, AK4113_REG_RCS0) & ~(AK4113_QINT |
			AK4113_CINT | AK4113_STC);
	chip->rcs1 = reg_read(chip, AK4113_REG_RCS1);
	chip->rcs2 = reg_read(chip, AK4113_REG_RCS2);
	err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, chip, &ops);
	if (err < 0)
		goto __fail;

	if (r_ak4113)
		*r_ak4113 = chip;
	return 0;

__fail:
	snd_ak4113_free(chip);
	return err < 0 ? err : -EIO;
}
EXPORT_SYMBOL_GPL(snd_ak4113_create);

void snd_ak4113_reg_write(struct ak4113 *chip, unsigned char reg,
		unsigned char mask, unsigned char val)
{
	if (reg >= AK4113_WRITABLE_REGS)
		return;
	reg_write(chip, reg, (chip->regmap[reg] & ~mask) | val);
}
EXPORT_SYMBOL_GPL(snd_ak4113_reg_write);

static void ak4113_init_regs(struct ak4113 *chip)
{
	unsigned char old = chip->regmap[AK4113_REG_PWRDN], reg;

	/* bring the chip to reset state and powerdown state */
	reg_write(chip, AK4113_REG_PWRDN, old & ~(AK4113_RST|AK4113_PWN));
	udelay(200);
	/* release reset, but leave powerdown */
	reg_write(chip, AK4113_REG_PWRDN, (old | AK4113_RST) & ~AK4113_PWN);
	udelay(200);
	for (reg = 1; reg < AK4113_WRITABLE_REGS; reg++)
		reg_write(chip, reg, chip->regmap[reg]);
	/* release powerdown, everything is initialized now */
	reg_write(chip, AK4113_REG_PWRDN, old | AK4113_RST | AK4113_PWN);
}

void snd_ak4113_reinit(struct ak4113 *chip)
{
	chip->init = 1;
	mb();
	flush_scheduled_work();
	ak4113_init_regs(chip);
	/* bring up statistics / event queing */
	chip->init = 0;
	if (chip->kctls[0])
		schedule_delayed_work(&chip->work, HZ / 10);
}
EXPORT_SYMBOL_GPL(snd_ak4113_reinit);

static unsigned int external_rate(unsigned char rcs1)
{
	switch (rcs1 & (AK4113_FS0|AK4113_FS1|AK4113_FS2|AK4113_FS3)) {
	case AK4113_FS_8000HZ:
		return 8000;
	case AK4113_FS_11025HZ:
		return 11025;
	case AK4113_FS_16000HZ:
		return 16000;
	case AK4113_FS_22050HZ:
		return 22050;
	case AK4113_FS_24000HZ:
		return 24000;
	case AK4113_FS_32000HZ:
		return 32000;
	case AK4113_FS_44100HZ:
		return 44100;
	case AK4113_FS_48000HZ:
		return 48000;
	case AK4113_FS_64000HZ:
		return 64000;
	case AK4113_FS_88200HZ:
		return 88200;
	case AK4113_FS_96000HZ:
		return 96000;
	case AK4113_FS_176400HZ:
		return 176400;
	case AK4113_FS_192000HZ:
		return 192000;
	default:
		return 0;
	}
}

static int snd_ak4113_in_error_info(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = LONG_MAX;
	return 0;
}

static int snd_ak4113_in_error_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct ak4113 *chip = snd_kcontrol_chip(kcontrol);
	long *ptr;

	spin_lock_irq(&chip->lock);
	ptr = (long *)(((char *)chip) + kcontrol->private_value);
	ucontrol->value.integer.value[0] = *ptr;
	*ptr = 0;
	spin_unlock_irq(&chip->lock);
	return 0;
}

#define snd_ak4113_in_bit_info		snd_ctl_boolean_mono_info

static int snd_ak4113_in_bit_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct ak4113 *chip = snd_kcontrol_chip(kcontrol);
	unsigned char reg = kcontrol->private_value & 0xff;
	unsigned char bit = (kcontrol->private_value >> 8) & 0xff;
	unsigned char inv = (kcontrol->private_value >> 31) & 1;

	ucontrol->value.integer.value[0] =
		((reg_read(chip, reg) & (1 << bit)) ? 1 : 0) ^ inv;
	return 0;
}

static int snd_ak4113_rx_info(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 5;
	return 0;
}

static int snd_ak4113_rx_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct ak4113 *chip = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] =
		(AK4113_IPS(chip->regmap[AK4113_REG_IO1]));
	return 0;
}

static int snd_ak4113_rx_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct ak4113 *chip = snd_kcontrol_chip(kcontrol);
	int change;
	u8 old_val;

	spin_lock_irq(&chip->lock);
	old_val = chip->regmap[AK4113_REG_IO1];
	change = ucontrol->value.integer.value[0] != AK4113_IPS(old_val);
	if (change)
		reg_write(chip, AK4113_REG_IO1,
				(old_val & (~AK4113_IPS(0xff))) |
				(AK4113_IPS(ucontrol->value.integer.value[0])));
	spin_unlock_irq(&chip->lock);
	return change;
}

static int snd_ak4113_rate_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 192000;
	return 0;
}

static int snd_ak4113_rate_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct ak4113 *chip = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = external_rate(reg_read(chip,
				AK4113_REG_RCS1));
	return 0;
}

static int snd_ak4113_spdif_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}

static int snd_ak4113_spdif_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct ak4113 *chip = snd_kcontrol_chip(kcontrol);
	unsigned i;

	for (i = 0; i < AK4113_REG_RXCSB_SIZE; i++)
		ucontrol->value.iec958.status[i] = reg_read(chip,
				AK4113_REG_RXCSB0 + i);
	return 0;
}

static int snd_ak4113_spdif_mask_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;
	return 0;
}

static int snd_ak4113_spdif_mask_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	memset(ucontrol->value.iec958.status, 0xff, AK4113_REG_RXCSB_SIZE);
	return 0;
}

static int snd_ak4113_spdif_pinfo(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xffff;
	uinfo->count = 4;
	return 0;
}

static int snd_ak4113_spdif_pget(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct ak4113 *chip = snd_kcontrol_chip(kcontrol);
	unsigned short tmp;

	ucontrol->value.integer.value[0] = 0xf8f2;
	ucontrol->value.integer.value[1] = 0x4e1f;
	tmp = reg_read(chip, AK4113_REG_Pc0) |
		(reg_read(chip, AK4113_REG_Pc1) << 8);
	ucontrol->value.integer.value[2] = tmp;
	tmp = reg_read(chip, AK4113_REG_Pd0) |
		(reg_read(chip, AK4113_REG_Pd1) << 8);
	ucontrol->value.integer.value[3] = tmp;
	return 0;
}

static int snd_ak4113_spdif_qinfo(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = AK4113_REG_QSUB_SIZE;
	return 0;
}

static int snd_ak4113_spdif_qget(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct ak4113 *chip = snd_kcontrol_chip(kcontrol);
	unsigned i;

	for (i = 0; i < AK4113_REG_QSUB_SIZE; i++)
		ucontrol->value.bytes.data[i] = reg_read(chip,
				AK4113_REG_QSUB_ADDR + i);
	return 0;
}

/* Don't forget to change AK4113_CONTROLS define!!! */
static struct snd_kcontrol_new snd_ak4113_iec958_controls[] = {
{
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		"IEC958 Parity Errors",
	.access =	SNDRV_CTL_ELEM_ACCESS_READ |
		SNDRV_CTL_ELEM_ACCESS_VOLATILE,
	.info =		snd_ak4113_in_error_info,
	.get =		snd_ak4113_in_error_get,
	.private_value = offsetof(struct ak4113, parity_errors),
},
{
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		"IEC958 V-Bit Errors",
	.access =	SNDRV_CTL_ELEM_ACCESS_READ |
		SNDRV_CTL_ELEM_ACCESS_VOLATILE,
	.info =		snd_ak4113_in_error_info,
	.get =		snd_ak4113_in_error_get,
	.private_value = offsetof(struct ak4113, v_bit_errors),
},
{
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		"IEC958 C-CRC Errors",
	.access =	SNDRV_CTL_ELEM_ACCESS_READ |
		SNDRV_CTL_ELEM_ACCESS_VOLATILE,
	.info =		snd_ak4113_in_error_info,
	.get =		snd_ak4113_in_error_get,
	.private_value = offsetof(struct ak4113, ccrc_errors),
},
{
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		"IEC958 Q-CRC Errors",
	.access =	SNDRV_CTL_ELEM_ACCESS_READ |
		SNDRV_CTL_ELEM_ACCESS_VOLATILE,
	.info =		snd_ak4113_in_error_info,
	.get =		snd_ak4113_in_error_get,
	.private_value = offsetof(struct ak4113, qcrc_errors),
},
{
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		"IEC958 External Rate",
	.access =	SNDRV_CTL_ELEM_ACCESS_READ |
		SNDRV_CTL_ELEM_ACCESS_VOLATILE,
	.info =		snd_ak4113_rate_info,
	.get =		snd_ak4113_rate_get,
},
{
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		SNDRV_CTL_NAME_IEC958("", CAPTURE, MASK),
	.access =	SNDRV_CTL_ELEM_ACCESS_READ,
	.info =		snd_ak4113_spdif_mask_info,
	.get =		snd_ak4113_spdif_mask_get,
},
{
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		SNDRV_CTL_NAME_IEC958("", CAPTURE, DEFAULT),
	.access =	SNDRV_CTL_ELEM_ACCESS_READ |
		SNDRV_CTL_ELEM_ACCESS_VOLATILE,
	.info =		snd_ak4113_spdif_info,
	.get =		snd_ak4113_spdif_get,
},
{
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		"IEC958 Preample Capture Default",
	.access =	SNDRV_CTL_ELEM_ACCESS_READ |
		SNDRV_CTL_ELEM_ACCESS_VOLATILE,
	.info =		snd_ak4113_spdif_pinfo,
	.get =		snd_ak4113_spdif_pget,
},
{
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		"IEC958 Q-subcode Capture Default",
	.access =	SNDRV_CTL_ELEM_ACCESS_READ |
		SNDRV_CTL_ELEM_ACCESS_VOLATILE,
	.info =		snd_ak4113_spdif_qinfo,
	.get =		snd_ak4113_spdif_qget,
},
{
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		"IEC958 Audio",
	.access =	SNDRV_CTL_ELEM_ACCESS_READ |
		SNDRV_CTL_ELEM_ACCESS_VOLATILE,
	.info =		snd_ak4113_in_bit_info,
	.get =		snd_ak4113_in_bit_get,
	.private_value = (1<<31) | (1<<8) | AK4113_REG_RCS0,
},
{
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		"IEC958 Non-PCM Bitstream",
	.access =	SNDRV_CTL_ELEM_ACCESS_READ |
		SNDRV_CTL_ELEM_ACCESS_VOLATILE,
	.info =		snd_ak4113_in_bit_info,
	.get =		snd_ak4113_in_bit_get,
	.private_value = (0<<8) | AK4113_REG_RCS1,
},
{
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		"IEC958 DTS Bitstream",
	.access =	SNDRV_CTL_ELEM_ACCESS_READ |
		SNDRV_CTL_ELEM_ACCESS_VOLATILE,
	.info =		snd_ak4113_in_bit_info,
	.get =		snd_ak4113_in_bit_get,
	.private_value = (1<<8) | AK4113_REG_RCS1,
},
{
	.iface =	SNDRV_CTL_ELEM_IFACE_PCM,
	.name =		"AK4113 Input Select",
	.access =	SNDRV_CTL_ELEM_ACCESS_READ |
		SNDRV_CTL_ELEM_ACCESS_WRITE,
	.info =		snd_ak4113_rx_info,
	.get =		snd_ak4113_rx_get,
	.put =		snd_ak4113_rx_put,
}
};

static void snd_ak4113_proc_regs_read(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct ak4113 *ak4113 = entry->private_data;
	int reg, val;
	/* all ak4113 registers 0x00 - 0x1c */
	for (reg = 0; reg < 0x1d; reg++) {
		val = reg_read(ak4113, reg);
		snd_iprintf(buffer, "0x%02x = 0x%02x\n", reg, val);
	}
}

static void snd_ak4113_proc_init(struct ak4113 *ak4113)
{
	struct snd_info_entry *entry;
	if (!snd_card_proc_new(ak4113->card, "ak4113", &entry))
		snd_info_set_text_ops(entry, ak4113, snd_ak4113_proc_regs_read);
}

int snd_ak4113_build(struct ak4113 *ak4113,
		struct snd_pcm_substream *cap_substream)
{
	struct snd_kcontrol *kctl;
	unsigned int idx;
	int err;

	if (snd_BUG_ON(!cap_substream))
		return -EINVAL;
	ak4113->substream = cap_substream;
	for (idx = 0; idx < AK4113_CONTROLS; idx++) {
		kctl = snd_ctl_new1(&snd_ak4113_iec958_controls[idx], ak4113);
		if (kctl == NULL)
			return -ENOMEM;
		kctl->id.device = cap_substream->pcm->device;
		kctl->id.subdevice = cap_substream->number;
		err = snd_ctl_add(ak4113->card, kctl);
		if (err < 0)
			return err;
		ak4113->kctls[idx] = kctl;
	}
	snd_ak4113_proc_init(ak4113);
	/* trigger workq */
	schedule_delayed_work(&ak4113->work, HZ / 10);
	return 0;
}
EXPORT_SYMBOL_GPL(snd_ak4113_build);

int snd_ak4113_external_rate(struct ak4113 *ak4113)
{
	unsigned char rcs1;

	rcs1 = reg_read(ak4113, AK4113_REG_RCS1);
	return external_rate(rcs1);
}
EXPORT_SYMBOL_GPL(snd_ak4113_external_rate);

int snd_ak4113_check_rate_and_errors(struct ak4113 *ak4113, unsigned int flags)
{
	struct snd_pcm_runtime *runtime =
		ak4113->substream ? ak4113->substream->runtime : NULL;
	unsigned long _flags;
	int res = 0;
	unsigned char rcs0, rcs1, rcs2;
	unsigned char c0, c1;

	rcs1 = reg_read(ak4113, AK4113_REG_RCS1);
	if (flags & AK4113_CHECK_NO_STAT)
		goto __rate;
	rcs0 = reg_read(ak4113, AK4113_REG_RCS0);
	rcs2 = reg_read(ak4113, AK4113_REG_RCS2);
	spin_lock_irqsave(&ak4113->lock, _flags);
	if (rcs0 & AK4113_PAR)
		ak4113->parity_errors++;
	if (rcs0 & AK4113_V)
		ak4113->v_bit_errors++;
	if (rcs2 & AK4113_CCRC)
		ak4113->ccrc_errors++;
	if (rcs2 & AK4113_QCRC)
		ak4113->qcrc_errors++;
	c0 = (ak4113->rcs0 & (AK4113_QINT | AK4113_CINT | AK4113_STC |
				AK4113_AUDION | AK4113_AUTO | AK4113_UNLCK)) ^
		(rcs0 & (AK4113_QINT | AK4113_CINT | AK4113_STC |
			 AK4113_AUDION | AK4113_AUTO | AK4113_UNLCK));
	c1 = (ak4113->rcs1 & (AK4113_DTSCD | AK4113_NPCM | AK4113_PEM |
				AK4113_DAT | 0xf0)) ^
		(rcs1 & (AK4113_DTSCD | AK4113_NPCM | AK4113_PEM |
			 AK4113_DAT | 0xf0));
	ak4113->rcs0 = rcs0 & ~(AK4113_QINT | AK4113_CINT | AK4113_STC);
	ak4113->rcs1 = rcs1;
	ak4113->rcs2 = rcs2;
	spin_unlock_irqrestore(&ak4113->lock, _flags);

	if (rcs0 & AK4113_PAR)
		snd_ctl_notify(ak4113->card, SNDRV_CTL_EVENT_MASK_VALUE,
				&ak4113->kctls[0]->id);
	if (rcs0 & AK4113_V)
		snd_ctl_notify(ak4113->card, SNDRV_CTL_EVENT_MASK_VALUE,
				&ak4113->kctls[1]->id);
	if (rcs2 & AK4113_CCRC)
		snd_ctl_notify(ak4113->card, SNDRV_CTL_EVENT_MASK_VALUE,
				&ak4113->kctls[2]->id);
	if (rcs2 & AK4113_QCRC)
		snd_ctl_notify(ak4113->card, SNDRV_CTL_EVENT_MASK_VALUE,
				&ak4113->kctls[3]->id);

	/* rate change */
	if (c1 & 0xf0)
		snd_ctl_notify(ak4113->card, SNDRV_CTL_EVENT_MASK_VALUE,
				&ak4113->kctls[4]->id);

	if ((c1 & AK4113_PEM) | (c0 & AK4113_CINT))
		snd_ctl_notify(ak4113->card, SNDRV_CTL_EVENT_MASK_VALUE,
				&ak4113->kctls[6]->id);
	if (c0 & AK4113_QINT)
		snd_ctl_notify(ak4113->card, SNDRV_CTL_EVENT_MASK_VALUE,
				&ak4113->kctls[8]->id);

	if (c0 & AK4113_AUDION)
		snd_ctl_notify(ak4113->card, SNDRV_CTL_EVENT_MASK_VALUE,
				&ak4113->kctls[9]->id);
	if (c1 & AK4113_NPCM)
		snd_ctl_notify(ak4113->card, SNDRV_CTL_EVENT_MASK_VALUE,
				&ak4113->kctls[10]->id);
	if (c1 & AK4113_DTSCD)
		snd_ctl_notify(ak4113->card, SNDRV_CTL_EVENT_MASK_VALUE,
				&ak4113->kctls[11]->id);

	if (ak4113->change_callback && (c0 | c1) != 0)
		ak4113->change_callback(ak4113, c0, c1);

__rate:
	/* compare rate */
	res = external_rate(rcs1);
	if (!(flags & AK4113_CHECK_NO_RATE) && runtime &&
			(runtime->rate != res)) {
		snd_pcm_stream_lock_irqsave(ak4113->substream, _flags);
		if (snd_pcm_running(ak4113->substream)) {
			/*printk(KERN_DEBUG "rate changed (%i <- %i)\n",
			 * runtime->rate, res); */
			snd_pcm_stop(ak4113->substream,
					SNDRV_PCM_STATE_DRAINING);
			wake_up(&runtime->sleep);
			res = 1;
		}
		snd_pcm_stream_unlock_irqrestore(ak4113->substream, _flags);
	}
	return res;
}
EXPORT_SYMBOL_GPL(snd_ak4113_check_rate_and_errors);

static void ak4113_stats(struct work_struct *work)
{
	struct ak4113 *chip = container_of(work, struct ak4113, work.work);

	if (!chip->init)
		snd_ak4113_check_rate_and_errors(chip, chip->check_flags);

	schedule_delayed_work(&chip->work, HZ / 10);
}
