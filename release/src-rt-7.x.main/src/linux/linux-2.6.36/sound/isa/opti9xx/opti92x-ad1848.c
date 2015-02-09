/*
    card-opti92x-ad1848.c - driver for OPTi 82c92x based soundcards.
    Copyright (C) 1998-2000 by Massimo Piccioni <dafastidio@libero.it>

    Part of this code was developed at the Italian Ministry of Air Defence,
    Sixth Division (oh, che pace ...), Rome.

    Thanks to Maria Grazia Pollarini, Salvatore Vassallo.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/


#include <linux/init.h>
#include <linux/err.h>
#include <linux/isa.h>
#include <linux/delay.h>
#include <linux/pnp.h>
#include <linux/moduleparam.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <sound/core.h>
#include <sound/tlv.h>
#include <sound/wss.h>
#include <sound/mpu401.h>
#include <sound/opl3.h>
#ifndef OPTi93X
#include <sound/opl4.h>
#endif
#define SNDRV_LEGACY_FIND_FREE_IRQ
#define SNDRV_LEGACY_FIND_FREE_DMA
#include <sound/initval.h>

MODULE_AUTHOR("Massimo Piccioni <dafastidio@libero.it>");
MODULE_LICENSE("GPL");
#ifdef OPTi93X
MODULE_DESCRIPTION("OPTi93X");
MODULE_SUPPORTED_DEVICE("{{OPTi,82C931/3}}");
#else	/* OPTi93X */
#ifdef CS4231
MODULE_DESCRIPTION("OPTi92X - CS4231");
MODULE_SUPPORTED_DEVICE("{{OPTi,82C924 (CS4231)},"
		"{OPTi,82C925 (CS4231)}}");
#else	/* CS4231 */
MODULE_DESCRIPTION("OPTi92X - AD1848");
MODULE_SUPPORTED_DEVICE("{{OPTi,82C924 (AD1848)},"
		"{OPTi,82C925 (AD1848)},"
	        "{OAK,Mozart}}");
#endif	/* CS4231 */
#endif	/* OPTi93X */

static int index = SNDRV_DEFAULT_IDX1;	/* Index 0-MAX */
static char *id = SNDRV_DEFAULT_STR1;		/* ID for this card */
//static int enable = SNDRV_DEFAULT_ENABLE1;	/* Enable this card */
#ifdef CONFIG_PNP
static int isapnp = 1;			/* Enable ISA PnP detection */
#endif
static long port = SNDRV_DEFAULT_PORT1; 	/* 0x530,0xe80,0xf40,0x604 */
static long mpu_port = SNDRV_DEFAULT_PORT1;	/* 0x300,0x310,0x320,0x330 */
static long fm_port = SNDRV_DEFAULT_PORT1;	/* 0x388 */
static int irq = SNDRV_DEFAULT_IRQ1;		/* 5,7,9,10,11 */
static int mpu_irq = SNDRV_DEFAULT_IRQ1;	/* 5,7,9,10 */
static int dma1 = SNDRV_DEFAULT_DMA1;		/* 0,1,3 */
#if defined(CS4231) || defined(OPTi93X)
static int dma2 = SNDRV_DEFAULT_DMA1;		/* 0,1,3 */
#endif	/* CS4231 || OPTi93X */

module_param(index, int, 0444);
MODULE_PARM_DESC(index, "Index value for opti9xx based soundcard.");
module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for opti9xx based soundcard.");
//module_param(enable, bool, 0444);
//MODULE_PARM_DESC(enable, "Enable opti9xx soundcard.");
#ifdef CONFIG_PNP
module_param(isapnp, bool, 0444);
MODULE_PARM_DESC(isapnp, "Enable ISA PnP detection for specified soundcard.");
#endif
module_param(port, long, 0444);
MODULE_PARM_DESC(port, "WSS port # for opti9xx driver.");
module_param(mpu_port, long, 0444);
MODULE_PARM_DESC(mpu_port, "MPU-401 port # for opti9xx driver.");
module_param(fm_port, long, 0444);
MODULE_PARM_DESC(fm_port, "FM port # for opti9xx driver.");
module_param(irq, int, 0444);
MODULE_PARM_DESC(irq, "WSS irq # for opti9xx driver.");
module_param(mpu_irq, int, 0444);
MODULE_PARM_DESC(mpu_irq, "MPU-401 irq # for opti9xx driver.");
module_param(dma1, int, 0444);
MODULE_PARM_DESC(dma1, "1st dma # for opti9xx driver.");
#if defined(CS4231) || defined(OPTi93X)
module_param(dma2, int, 0444);
MODULE_PARM_DESC(dma2, "2nd dma # for opti9xx driver.");
#endif	/* CS4231 || OPTi93X */

#define OPTi9XX_HW_82C928	1
#define OPTi9XX_HW_82C929	2
#define OPTi9XX_HW_82C924	3
#define OPTi9XX_HW_82C925	4
#define OPTi9XX_HW_82C930	5
#define OPTi9XX_HW_82C931	6
#define OPTi9XX_HW_82C933	7
#define OPTi9XX_HW_LAST		OPTi9XX_HW_82C933

#define OPTi9XX_MC_REG(n)	n

#ifdef OPTi93X

#define OPTi93X_STATUS			0x02
#define OPTi93X_PORT(chip, r)		((chip)->port + OPTi93X_##r)

#define OPTi93X_IRQ_PLAYBACK		0x04
#define OPTi93X_IRQ_CAPTURE		0x08

#endif /* OPTi93X */

struct snd_opti9xx {
	unsigned short hardware;
	unsigned char password;
	char name[7];

	unsigned long mc_base;
	struct resource *res_mc_base;
	unsigned long mc_base_size;
#ifdef OPTi93X
	unsigned long mc_indir_index;
	unsigned long mc_indir_size;
	struct resource *res_mc_indir;
	struct snd_wss *codec;
#endif	/* OPTi93X */
	unsigned long pwd_reg;

	spinlock_t lock;

	long wss_base;
	int irq;
};

static int snd_opti9xx_pnp_is_probed;

#ifdef CONFIG_PNP

static struct pnp_card_device_id snd_opti9xx_pnpids[] = {
#ifndef OPTi93X
	/* OPTi 82C924 */
	{ .id = "OPT0924",
	  .devs = { { "OPT0000" }, { "OPT0002" }, { "OPT0005" } },
	  .driver_data = 0x0924 },
	/* OPTi 82C925 */
	{ .id = "OPT0925",
	  .devs = { { "OPT9250" }, { "OPT0002" }, { "OPT0005" } },
	  .driver_data = 0x0925 },
#else
	/* OPTi 82C931/3 */
	{ .id = "OPT0931", .devs = { { "OPT9310" }, { "OPT0002" } },
	  .driver_data = 0x0931 },
#endif	/* OPTi93X */
	{ .id = "" }
};

MODULE_DEVICE_TABLE(pnp_card, snd_opti9xx_pnpids);

#endif	/* CONFIG_PNP */

#ifdef OPTi93X
#define DEV_NAME "opti93x"
#else
#define DEV_NAME "opti92x"
#endif

static char * snd_opti9xx_names[] = {
	"unknown",
	"82C928",	"82C929",
	"82C924",	"82C925",
	"82C930",	"82C931",	"82C933"
};


static long __devinit snd_legacy_find_free_ioport(long *port_table, long size)
{
	while (*port_table != -1) {
		if (request_region(*port_table, size, "ALSA test")) {
			release_region(*port_table, size);
			return *port_table;
		}
		port_table++;
	}
	return -1;
}

static int __devinit snd_opti9xx_init(struct snd_opti9xx *chip,
				      unsigned short hardware)
{
	static int opti9xx_mc_size[] = {7, 7, 10, 10, 2, 2, 2};

	chip->hardware = hardware;
	strcpy(chip->name, snd_opti9xx_names[hardware]);

	spin_lock_init(&chip->lock);

	chip->irq = -1;

#ifndef OPTi93X
#ifdef CONFIG_PNP
	if (isapnp && chip->mc_base)
		/* PnP resource gives the least 10 bits */
		chip->mc_base |= 0xc00;
	else
#endif	/* CONFIG_PNP */
	{
		chip->mc_base = 0xf8c;
		chip->mc_base_size = opti9xx_mc_size[hardware];
	}
#else
		chip->mc_base_size = opti9xx_mc_size[hardware];
#endif

	switch (hardware) {
#ifndef OPTi93X
	case OPTi9XX_HW_82C928:
	case OPTi9XX_HW_82C929:
		chip->password = (hardware == OPTi9XX_HW_82C928) ? 0xe2 : 0xe3;
		chip->pwd_reg = 3;
		break;

	case OPTi9XX_HW_82C924:
	case OPTi9XX_HW_82C925:
		chip->password = 0xe5;
		chip->pwd_reg = 3;
		break;
#else	/* OPTi93X */

	case OPTi9XX_HW_82C930:
	case OPTi9XX_HW_82C931:
	case OPTi9XX_HW_82C933:
		chip->mc_base = (hardware == OPTi9XX_HW_82C930) ? 0xf8f : 0xf8d;
		if (!chip->mc_indir_index) {
			chip->mc_indir_index = 0xe0e;
			chip->mc_indir_size = 2;
		}
		chip->password = 0xe4;
		chip->pwd_reg = 0;
		break;
#endif	/* OPTi93X */

	default:
		snd_printk(KERN_ERR "chip %d not supported\n", hardware);
		return -ENODEV;
	}
	return 0;
}

static unsigned char snd_opti9xx_read(struct snd_opti9xx *chip,
				      unsigned char reg)
{
	unsigned long flags;
	unsigned char retval = 0xff;

	spin_lock_irqsave(&chip->lock, flags);
	outb(chip->password, chip->mc_base + chip->pwd_reg);

	switch (chip->hardware) {
#ifndef OPTi93X
	case OPTi9XX_HW_82C924:
	case OPTi9XX_HW_82C925:
		if (reg > 7) {
			outb(reg, chip->mc_base + 8);
			outb(chip->password, chip->mc_base + chip->pwd_reg);
			retval = inb(chip->mc_base + 9);
			break;
		}

	case OPTi9XX_HW_82C928:
	case OPTi9XX_HW_82C929:
		retval = inb(chip->mc_base + reg);
		break;
#else	/* OPTi93X */

	case OPTi9XX_HW_82C930:
	case OPTi9XX_HW_82C931:
	case OPTi9XX_HW_82C933:
		outb(reg, chip->mc_indir_index);
		outb(chip->password, chip->mc_base + chip->pwd_reg);
		retval = inb(chip->mc_indir_index + 1);
		break;
#endif	/* OPTi93X */

	default:
		snd_printk(KERN_ERR "chip %d not supported\n", chip->hardware);
	}

	spin_unlock_irqrestore(&chip->lock, flags);
	return retval;
}

static void snd_opti9xx_write(struct snd_opti9xx *chip, unsigned char reg,
			      unsigned char value)
{
	unsigned long flags;

	spin_lock_irqsave(&chip->lock, flags);
	outb(chip->password, chip->mc_base + chip->pwd_reg);

	switch (chip->hardware) {
#ifndef OPTi93X
	case OPTi9XX_HW_82C924:
	case OPTi9XX_HW_82C925:
		if (reg > 7) {
			outb(reg, chip->mc_base + 8);
			outb(chip->password, chip->mc_base + chip->pwd_reg);
			outb(value, chip->mc_base + 9);
			break;
		}

	case OPTi9XX_HW_82C928:
	case OPTi9XX_HW_82C929:
		outb(value, chip->mc_base + reg);
		break;
#else	/* OPTi93X */

	case OPTi9XX_HW_82C930:
	case OPTi9XX_HW_82C931:
	case OPTi9XX_HW_82C933:
		outb(reg, chip->mc_indir_index);
		outb(chip->password, chip->mc_base + chip->pwd_reg);
		outb(value, chip->mc_indir_index + 1);
		break;
#endif	/* OPTi93X */

	default:
		snd_printk(KERN_ERR "chip %d not supported\n", chip->hardware);
	}

	spin_unlock_irqrestore(&chip->lock, flags);
}


#define snd_opti9xx_write_mask(chip, reg, value, mask)	\
	snd_opti9xx_write(chip, reg,			\
		(snd_opti9xx_read(chip, reg) & ~(mask)) | ((value) & (mask)))


static int __devinit snd_opti9xx_configure(struct snd_opti9xx *chip,
					   long port,
					   int irq, int dma1, int dma2,
					   long mpu_port, int mpu_irq)
{
	unsigned char wss_base_bits;
	unsigned char irq_bits;
	unsigned char dma_bits;
	unsigned char mpu_port_bits = 0;
	unsigned char mpu_irq_bits;

	switch (chip->hardware) {
#ifndef OPTi93X
	case OPTi9XX_HW_82C924:
		/* opti 929 mode (?), OPL3 clock output, audio enable */
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(4), 0xf0, 0xfc);
		/* enable wave audio */
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(6), 0x02, 0x02);

	case OPTi9XX_HW_82C925:
		/* enable WSS mode */
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(1), 0x80, 0x80);
		/* OPL3 FM synthesis */
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(2), 0x00, 0x20);
		/* disable Sound Blaster IRQ and DMA */
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(3), 0xf0, 0xff);
#ifdef CS4231
		/* cs4231/4248 fix enabled */
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(5), 0x02, 0x02);
#else
		/* cs4231/4248 fix disabled */
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(5), 0x00, 0x02);
#endif	/* CS4231 */
		break;

	case OPTi9XX_HW_82C928:
	case OPTi9XX_HW_82C929:
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(1), 0x80, 0x80);
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(2), 0x00, 0x20);
		/*
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(3), 0xa2, 0xae);
		*/
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(4), 0x00, 0x0c);
#ifdef CS4231
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(5), 0x02, 0x02);
#else
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(5), 0x00, 0x02);
#endif	/* CS4231 */
		break;

#else	/* OPTi93X */
	case OPTi9XX_HW_82C931:
	case OPTi9XX_HW_82C933:
		/*
		 * The BTC 1817DW has QS1000 wavetable which is connected
		 * to the serial digital input of the OPTI931.
		 */
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(21), 0x82, 0xff);
		/* 
		 * This bit sets OPTI931 to automaticaly select FM
		 * or digital input signal.
		 */
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(26), 0x01, 0x01);
	case OPTi9XX_HW_82C930: /* FALL THROUGH */
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(6), 0x02, 0x03);
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(3), 0x00, 0xff);
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(4), 0x10 |
			(chip->hardware == OPTi9XX_HW_82C930 ? 0x00 : 0x04),
			0x34);
		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(5), 0x20, 0xbf);
		break;
#endif	/* OPTi93X */

	default:
		snd_printk(KERN_ERR "chip %d not supported\n", chip->hardware);
		return -EINVAL;
	}

	/* PnP resource says it decodes only 10 bits of address */
	switch (port & 0x3ff) {
	case 0x130:
		chip->wss_base = 0x530;
		wss_base_bits = 0x00;
		break;
	case 0x204:
		chip->wss_base = 0x604;
		wss_base_bits = 0x03;
		break;
	case 0x280:
		chip->wss_base = 0xe80;
		wss_base_bits = 0x01;
		break;
	case 0x340:
		chip->wss_base = 0xf40;
		wss_base_bits = 0x02;
		break;
	default:
		snd_printk(KERN_WARNING "WSS port 0x%lx not valid\n", port);
		goto __skip_base;
	}
	snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(1), wss_base_bits << 4, 0x30);

__skip_base:
	switch (irq) {
//#ifdef OPTi93X
	case 5:
		irq_bits = 0x05;
		break;
//#endif	/* OPTi93X */
	case 7:
		irq_bits = 0x01;
		break;
	case 9:
		irq_bits = 0x02;
		break;
	case 10:
		irq_bits = 0x03;
		break;
	case 11:
		irq_bits = 0x04;
		break;
	default:
		snd_printk(KERN_WARNING "WSS irq # %d not valid\n", irq);
		goto __skip_resources;
	}

	switch (dma1) {
	case 0:
		dma_bits = 0x01;
		break;
	case 1:
		dma_bits = 0x02;
		break;
	case 3:
		dma_bits = 0x03;
		break;
	default:
		snd_printk(KERN_WARNING "WSS dma1 # %d not valid\n", dma1);
		goto __skip_resources;
	}

#if defined(CS4231) || defined(OPTi93X)
	if (dma1 == dma2) {
		snd_printk(KERN_ERR "don't want to share dmas\n");
		return -EBUSY;
	}

	switch (dma2) {
	case 0:
	case 1:
		break;
	default:
		snd_printk(KERN_WARNING "WSS dma2 # %d not valid\n", dma2);
		goto __skip_resources;
	}
	dma_bits |= 0x04;
#endif	/* CS4231 || OPTi93X */

#ifndef OPTi93X
	 outb(irq_bits << 3 | dma_bits, chip->wss_base);
#else /* OPTi93X */
	snd_opti9xx_write(chip, OPTi9XX_MC_REG(3), (irq_bits << 3 | dma_bits));
#endif /* OPTi93X */

__skip_resources:
	if (chip->hardware > OPTi9XX_HW_82C928) {
		switch (mpu_port) {
		case 0:
		case -1:
			break;
		case 0x300:
			mpu_port_bits = 0x03;
			break;
		case 0x310:
			mpu_port_bits = 0x02;
			break;
		case 0x320:
			mpu_port_bits = 0x01;
			break;
		case 0x330:
			mpu_port_bits = 0x00;
			break;
		default:
			snd_printk(KERN_WARNING
				   "MPU-401 port 0x%lx not valid\n", mpu_port);
			goto __skip_mpu;
		}

		switch (mpu_irq) {
		case 5:
			mpu_irq_bits = 0x02;
			break;
		case 7:
			mpu_irq_bits = 0x03;
			break;
		case 9:
			mpu_irq_bits = 0x00;
			break;
		case 10:
			mpu_irq_bits = 0x01;
			break;
		default:
			snd_printk(KERN_WARNING "MPU-401 irq # %d not valid\n",
				mpu_irq);
			goto __skip_mpu;
		}

		snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(6),
			(mpu_port <= 0) ? 0x00 :
				0x80 | mpu_port_bits << 5 | mpu_irq_bits << 3,
			0xf8);
	}
__skip_mpu:

	return 0;
}

#ifdef OPTi93X

static const DECLARE_TLV_DB_SCALE(db_scale_5bit_3db_step, -9300, 300, 0);
static const DECLARE_TLV_DB_SCALE(db_scale_5bit, -4650, 150, 0);
static const DECLARE_TLV_DB_SCALE(db_scale_4bit_12db_max, -3300, 300, 0);

static struct snd_kcontrol_new snd_opti93x_controls[] = {
WSS_DOUBLE("Master Playback Switch", 0,
		OPTi93X_OUT_LEFT, OPTi93X_OUT_RIGHT, 7, 7, 1, 1),
WSS_DOUBLE_TLV("Master Playback Volume", 0,
		OPTi93X_OUT_LEFT, OPTi93X_OUT_RIGHT, 1, 1, 31, 1,
		db_scale_5bit_3db_step),
WSS_DOUBLE_TLV("PCM Playback Volume", 0,
		CS4231_LEFT_OUTPUT, CS4231_RIGHT_OUTPUT, 0, 0, 31, 1,
		db_scale_5bit),
WSS_DOUBLE_TLV("FM Playback Volume", 0,
		CS4231_AUX2_LEFT_INPUT, CS4231_AUX2_RIGHT_INPUT, 1, 1, 15, 1,
		db_scale_4bit_12db_max),
WSS_DOUBLE("Line Playback Switch", 0,
		CS4231_LEFT_LINE_IN, CS4231_RIGHT_LINE_IN, 7, 7, 1, 1),
WSS_DOUBLE_TLV("Line Playback Volume", 0,
		CS4231_LEFT_LINE_IN, CS4231_RIGHT_LINE_IN, 0, 0, 15, 1,
		db_scale_4bit_12db_max),
WSS_DOUBLE("Mic Playback Switch", 0,
		OPTi93X_MIC_LEFT_INPUT, OPTi93X_MIC_RIGHT_INPUT, 7, 7, 1, 1),
WSS_DOUBLE_TLV("Mic Playback Volume", 0,
		OPTi93X_MIC_LEFT_INPUT, OPTi93X_MIC_RIGHT_INPUT, 1, 1, 15, 1,
		db_scale_4bit_12db_max),
WSS_DOUBLE_TLV("CD Playback Volume", 0,
		CS4231_AUX1_LEFT_INPUT, CS4231_AUX1_RIGHT_INPUT, 1, 1, 15, 1,
		db_scale_4bit_12db_max),
WSS_DOUBLE("Aux Playback Switch", 0,
		OPTi931_AUX_LEFT_INPUT, OPTi931_AUX_RIGHT_INPUT, 7, 7, 1, 1),
WSS_DOUBLE_TLV("Aux Playback Volume", 0,
		OPTi931_AUX_LEFT_INPUT, OPTi931_AUX_RIGHT_INPUT, 1, 1, 15, 1,
		db_scale_4bit_12db_max),
};

static int __devinit snd_opti93x_mixer(struct snd_wss *chip)
{
	struct snd_card *card;
	unsigned int idx;
	struct snd_ctl_elem_id id1, id2;
	int err;

	if (snd_BUG_ON(!chip || !chip->pcm))
		return -EINVAL;

	card = chip->card;

	strcpy(card->mixername, chip->pcm->name);

	memset(&id1, 0, sizeof(id1));
	memset(&id2, 0, sizeof(id2));
	id1.iface = id2.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	/* reassign AUX0 switch to CD */
	strcpy(id1.name, "Aux Playback Switch");
	strcpy(id2.name, "CD Playback Switch");
	err = snd_ctl_rename_id(card, &id1, &id2);
	if (err < 0) {
		snd_printk(KERN_ERR "Cannot rename opti93x control\n");
		return err;
	}
	/* reassign AUX1 switch to FM */
	strcpy(id1.name, "Aux Playback Switch"); id1.index = 1;
	strcpy(id2.name, "FM Playback Switch");
	err = snd_ctl_rename_id(card, &id1, &id2);
	if (err < 0) {
		snd_printk(KERN_ERR "Cannot rename opti93x control\n");
		return err;
	}
	/* remove AUX1 volume */
	strcpy(id1.name, "Aux Playback Volume"); id1.index = 1;
	snd_ctl_remove_id(card, &id1);

	/* Replace WSS volume controls with OPTi93x volume controls */
	id1.index = 0;
	for (idx = 0; idx < ARRAY_SIZE(snd_opti93x_controls); idx++) {
		strcpy(id1.name, snd_opti93x_controls[idx].name);
		snd_ctl_remove_id(card, &id1);

		err = snd_ctl_add(card,
				snd_ctl_new1(&snd_opti93x_controls[idx], chip));
		if (err < 0)
			return err;
	}
	return 0;
}

static irqreturn_t snd_opti93x_interrupt(int irq, void *dev_id)
{
	struct snd_opti9xx *chip = dev_id;
	struct snd_wss *codec = chip->codec;
	unsigned char status;

	if (!codec)
		return IRQ_HANDLED;

	status = snd_opti9xx_read(chip, OPTi9XX_MC_REG(11));
	if ((status & OPTi93X_IRQ_PLAYBACK) && codec->playback_substream)
		snd_pcm_period_elapsed(codec->playback_substream);
	if ((status & OPTi93X_IRQ_CAPTURE) && codec->capture_substream) {
		snd_wss_overrange(codec);
		snd_pcm_period_elapsed(codec->capture_substream);
	}
	outb(0x00, OPTi93X_PORT(codec, STATUS));
	return IRQ_HANDLED;
}

#endif /* OPTi93X */

static int __devinit snd_opti9xx_read_check(struct snd_opti9xx *chip)
{
	unsigned char value;
#ifdef OPTi93X
	unsigned long flags;
#endif

	chip->res_mc_base = request_region(chip->mc_base, chip->mc_base_size,
					   "OPTi9xx MC");
	if (chip->res_mc_base == NULL)
		return -EBUSY;
#ifndef OPTi93X
	value = snd_opti9xx_read(chip, OPTi9XX_MC_REG(1));
	if (value != 0xff && value != inb(chip->mc_base + OPTi9XX_MC_REG(1)))
		if (value == snd_opti9xx_read(chip, OPTi9XX_MC_REG(1)))
			return 0;
#else	/* OPTi93X */
	chip->res_mc_indir = request_region(chip->mc_indir_index,
					    chip->mc_indir_size,
					    "OPTi93x MC");
	if (chip->res_mc_indir == NULL)
		return -EBUSY;

	spin_lock_irqsave(&chip->lock, flags);
	outb(chip->password, chip->mc_base + chip->pwd_reg);
	outb(((chip->mc_indir_index & 0x1f0) >> 4), chip->mc_base);
	spin_unlock_irqrestore(&chip->lock, flags);

	value = snd_opti9xx_read(chip, OPTi9XX_MC_REG(7));
	snd_opti9xx_write(chip, OPTi9XX_MC_REG(7), 0xff - value);
	if (snd_opti9xx_read(chip, OPTi9XX_MC_REG(7)) == 0xff - value)
		return 0;

	release_and_free_resource(chip->res_mc_indir);
	chip->res_mc_indir = NULL;
#endif	/* OPTi93X */
	release_and_free_resource(chip->res_mc_base);
	chip->res_mc_base = NULL;

	return -ENODEV;
}

static int __devinit snd_card_opti9xx_detect(struct snd_card *card,
					     struct snd_opti9xx *chip)
{
	int i, err;

#ifndef OPTi93X
	for (i = OPTi9XX_HW_82C928; i < OPTi9XX_HW_82C930; i++) {
#else
	for (i = OPTi9XX_HW_82C931; i >= OPTi9XX_HW_82C930; i--) {
#endif
		err = snd_opti9xx_init(chip, i);
		if (err < 0)
			return err;

		err = snd_opti9xx_read_check(chip);
		if (err == 0)
			return 1;
#ifdef OPTi93X
		chip->mc_indir_index = 0;
#endif
	}
	return -ENODEV;
}

#ifdef CONFIG_PNP
static int __devinit snd_card_opti9xx_pnp(struct snd_opti9xx *chip,
					  struct pnp_card_link *card,
					  const struct pnp_card_device_id *pid)
{
	struct pnp_dev *pdev;
	int err;
	struct pnp_dev *devmpu;
#ifndef OPTi93X
	struct pnp_dev *devmc;
#endif

	pdev = pnp_request_card_device(card, pid->devs[0].id, NULL);
	if (pdev == NULL)
		return -EBUSY;

	err = pnp_activate_dev(pdev);
	if (err < 0) {
		snd_printk(KERN_ERR "AUDIO pnp configure failure: %d\n", err);
		return err;
	}

#ifdef OPTi93X
	port = pnp_port_start(pdev, 0) - 4;
	fm_port = pnp_port_start(pdev, 1) + 8;
	chip->mc_indir_index = pnp_port_start(pdev, 3) + 2;
	chip->mc_indir_size = pnp_port_len(pdev, 3) - 2;
#else
	devmc = pnp_request_card_device(card, pid->devs[2].id, NULL);
	if (devmc == NULL)
		return -EBUSY;

	err = pnp_activate_dev(devmc);
	if (err < 0) {
		snd_printk(KERN_ERR "MC pnp configure failure: %d\n", err);
		return err;
	}

	port = pnp_port_start(pdev, 1);
	fm_port = pnp_port_start(pdev, 2) + 8;
	/*
	 * The MC(0) is never accessed and card does not
	 * include it in the PnP resource range. OPTI93x include it.
	 */
	chip->mc_base = pnp_port_start(devmc, 0) - 1;
	chip->mc_base_size = pnp_port_len(devmc, 0) + 1;
#endif	/* OPTi93X */
	irq = pnp_irq(pdev, 0);
	dma1 = pnp_dma(pdev, 0);
#if defined(CS4231) || defined(OPTi93X)
	dma2 = pnp_dma(pdev, 1);
#endif	/* CS4231 || OPTi93X */

	devmpu = pnp_request_card_device(card, pid->devs[1].id, NULL);

	if (devmpu && mpu_port > 0) {
		err = pnp_activate_dev(devmpu);
		if (err < 0) {
			snd_printk(KERN_ERR "MPU401 pnp configure failure\n");
			mpu_port = -1;
		} else {
			mpu_port = pnp_port_start(devmpu, 0);
			mpu_irq = pnp_irq(devmpu, 0);
		}
	}
	return pid->driver_data;
}
#endif	/* CONFIG_PNP */

static void snd_card_opti9xx_free(struct snd_card *card)
{
	struct snd_opti9xx *chip = card->private_data;

	if (chip) {
#ifdef OPTi93X
		if (chip->irq > 0) {
			disable_irq(chip->irq);
			free_irq(chip->irq, chip);
		}
		release_and_free_resource(chip->res_mc_indir);
#endif
		release_and_free_resource(chip->res_mc_base);
	}
}

static int __devinit snd_opti9xx_probe(struct snd_card *card)
{
	static long possible_ports[] = {0x530, 0xe80, 0xf40, 0x604, -1};
	int error;
	int xdma2;
	struct snd_opti9xx *chip = card->private_data;
	struct snd_wss *codec;
#ifdef CS4231
	struct snd_timer *timer;
#endif
	struct snd_pcm *pcm;
	struct snd_rawmidi *rmidi;
	struct snd_hwdep *synth;

#if defined(CS4231) || defined(OPTi93X)
	xdma2 = dma2;
#else
	xdma2 = -1;
#endif

	if (port == SNDRV_AUTO_PORT) {
		port = snd_legacy_find_free_ioport(possible_ports, 4);
		if (port < 0) {
			snd_printk(KERN_ERR "unable to find a free WSS port\n");
			return -EBUSY;
		}
	}
	error = snd_opti9xx_configure(chip, port, irq, dma1, xdma2,
				      mpu_port, mpu_irq);
	if (error)
		return error;

	error = snd_wss_create(card, chip->wss_base + 4, -1, irq, dma1, xdma2,
#ifdef OPTi93X
			       WSS_HW_OPTI93X, WSS_HWSHARE_IRQ,
#else
			       WSS_HW_DETECT, 0,
#endif
			       &codec);
	if (error < 0)
		return error;
#ifdef OPTi93X
	chip->codec = codec;
#endif
	error = snd_wss_pcm(codec, 0, &pcm);
	if (error < 0)
		return error;
	error = snd_wss_mixer(codec);
	if (error < 0)
		return error;
#ifdef OPTi93X
	error = snd_opti93x_mixer(codec);
	if (error < 0)
		return error;
#endif
#ifdef CS4231
	error = snd_wss_timer(codec, 0, &timer);
	if (error < 0)
		return error;
#endif
#ifdef OPTi93X
	error = request_irq(irq, snd_opti93x_interrupt,
			    IRQF_DISABLED, DEV_NAME" - WSS", chip);
	if (error < 0) {
		snd_printk(KERN_ERR "opti9xx: can't grab IRQ %d\n", irq);
		return error;
	}
#endif
	chip->irq = irq;
	strcpy(card->driver, chip->name);
	sprintf(card->shortname, "OPTi %s", card->driver);
#if defined(CS4231) || defined(OPTi93X)
	sprintf(card->longname, "%s, %s at 0x%lx, irq %d, dma %d&%d",
		card->shortname, pcm->name,
		chip->wss_base + 4, irq, dma1, xdma2);
#else
	sprintf(card->longname, "%s, %s at 0x%lx, irq %d, dma %d",
		card->shortname, pcm->name, chip->wss_base + 4, irq, dma1);
#endif	/* CS4231 || OPTi93X */

	if (mpu_port <= 0 || mpu_port == SNDRV_AUTO_PORT)
		rmidi = NULL;
	else {
		error = snd_mpu401_uart_new(card, 0, MPU401_HW_MPU401,
				mpu_port, 0, mpu_irq, IRQF_DISABLED, &rmidi);
		if (error)
			snd_printk(KERN_WARNING "no MPU-401 device at 0x%lx?\n",
				   mpu_port);
	}

	if (fm_port > 0 && fm_port != SNDRV_AUTO_PORT) {
		struct snd_opl3 *opl3 = NULL;
#ifndef OPTi93X
		if (chip->hardware == OPTi9XX_HW_82C928 ||
		    chip->hardware == OPTi9XX_HW_82C929 ||
		    chip->hardware == OPTi9XX_HW_82C924) {
			struct snd_opl4 *opl4;
			/* assume we have an OPL4 */
			snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(2),
					       0x20, 0x20);
			if (snd_opl4_create(card, fm_port, fm_port - 8,
					    2, &opl3, &opl4) < 0) {
				/* no luck, use OPL3 instead */
				snd_opti9xx_write_mask(chip, OPTi9XX_MC_REG(2),
						       0x00, 0x20);
			}
		}
#endif	/* !OPTi93X */
		if (!opl3 && snd_opl3_create(card, fm_port, fm_port + 2,
					     OPL3_HW_AUTO, 0, &opl3) < 0) {
			snd_printk(KERN_WARNING "no OPL device at 0x%lx-0x%lx\n",
				   fm_port, fm_port + 4 - 1);
		}
		if (opl3) {
			error = snd_opl3_hwdep_new(opl3, 0, 1, &synth);
			if (error < 0)
				return error;
		}
	}

	return snd_card_register(card);
}

static int snd_opti9xx_card_new(struct snd_card **cardp)
{
	struct snd_card *card;
	int err;

	err = snd_card_create(index, id, THIS_MODULE,
			      sizeof(struct snd_opti9xx), &card);
	if (err < 0)
		return err;
	card->private_free = snd_card_opti9xx_free;
	*cardp = card;
	return 0;
}

static int __devinit snd_opti9xx_isa_match(struct device *devptr,
					   unsigned int dev)
{
#ifdef CONFIG_PNP
	if (snd_opti9xx_pnp_is_probed)
		return 0;
	if (isapnp)
		return 0;
#endif
	return 1;
}

static int __devinit snd_opti9xx_isa_probe(struct device *devptr,
					   unsigned int dev)
{
	struct snd_card *card;
	int error;
	static long possible_mpu_ports[] = {0x300, 0x310, 0x320, 0x330, -1};
#ifdef OPTi93X
	static int possible_irqs[] = {5, 9, 10, 11, 7, -1};
#else
	static int possible_irqs[] = {9, 10, 11, 7, -1};
#endif	/* OPTi93X */
	static int possible_mpu_irqs[] = {5, 9, 10, 7, -1};
	static int possible_dma1s[] = {3, 1, 0, -1};
#if defined(CS4231) || defined(OPTi93X)
	static int possible_dma2s[][2] = {{1,-1}, {0,-1}, {-1,-1}, {0,-1}};
#endif	/* CS4231 || OPTi93X */

	if (mpu_port == SNDRV_AUTO_PORT) {
		if ((mpu_port = snd_legacy_find_free_ioport(possible_mpu_ports, 2)) < 0) {
			snd_printk(KERN_ERR "unable to find a free MPU401 port\n");
			return -EBUSY;
		}
	}
	if (irq == SNDRV_AUTO_IRQ) {
		if ((irq = snd_legacy_find_free_irq(possible_irqs)) < 0) {
			snd_printk(KERN_ERR "unable to find a free IRQ\n");
			return -EBUSY;
		}
	}
	if (mpu_irq == SNDRV_AUTO_IRQ) {
		if ((mpu_irq = snd_legacy_find_free_irq(possible_mpu_irqs)) < 0) {
			snd_printk(KERN_ERR "unable to find a free MPU401 IRQ\n");
			return -EBUSY;
		}
	}
	if (dma1 == SNDRV_AUTO_DMA) {
		if ((dma1 = snd_legacy_find_free_dma(possible_dma1s)) < 0) {
			snd_printk(KERN_ERR "unable to find a free DMA1\n");
			return -EBUSY;
		}
	}
#if defined(CS4231) || defined(OPTi93X)
	if (dma2 == SNDRV_AUTO_DMA) {
		if ((dma2 = snd_legacy_find_free_dma(possible_dma2s[dma1 % 4])) < 0) {
			snd_printk(KERN_ERR "unable to find a free DMA2\n");
			return -EBUSY;
		}
	}
#endif

	error = snd_opti9xx_card_new(&card);
	if (error < 0)
		return error;

	if ((error = snd_card_opti9xx_detect(card, card->private_data)) < 0) {
		snd_card_free(card);
		return error;
	}
	snd_card_set_dev(card, devptr);
	if ((error = snd_opti9xx_probe(card)) < 0) {
		snd_card_free(card);
		return error;
	}
	dev_set_drvdata(devptr, card);
	return 0;
}

static int __devexit snd_opti9xx_isa_remove(struct device *devptr,
					    unsigned int dev)
{
	snd_card_free(dev_get_drvdata(devptr));
	dev_set_drvdata(devptr, NULL);
	return 0;
}

static struct isa_driver snd_opti9xx_driver = {
	.match		= snd_opti9xx_isa_match,
	.probe		= snd_opti9xx_isa_probe,
	.remove		= __devexit_p(snd_opti9xx_isa_remove),
	.driver		= {
		.name	= DEV_NAME
	},
};

#ifdef CONFIG_PNP
static int __devinit snd_opti9xx_pnp_probe(struct pnp_card_link *pcard,
					   const struct pnp_card_device_id *pid)
{
	struct snd_card *card;
	int error, hw;
	struct snd_opti9xx *chip;

	if (snd_opti9xx_pnp_is_probed)
		return -EBUSY;
	if (! isapnp)
		return -ENODEV;
	error = snd_opti9xx_card_new(&card);
	if (error < 0)
		return error;
	chip = card->private_data;

	hw = snd_card_opti9xx_pnp(chip, pcard, pid);
	switch (hw) {
	case 0x0924:
		hw = OPTi9XX_HW_82C924;
		break;
	case 0x0925:
		hw = OPTi9XX_HW_82C925;
		break;
	case 0x0931:
		hw = OPTi9XX_HW_82C931;
		break;
	default:
		snd_card_free(card);
		return -ENODEV;
	}

	if ((error = snd_opti9xx_init(chip, hw))) {
		snd_card_free(card);
		return error;
	}
	error = snd_opti9xx_read_check(chip);
	if (error) {
		snd_printk(KERN_ERR "OPTI chip not found\n");
		snd_card_free(card);
		return error;
	}
	snd_card_set_dev(card, &pcard->card->dev);
	if ((error = snd_opti9xx_probe(card)) < 0) {
		snd_card_free(card);
		return error;
	}
	pnp_set_card_drvdata(pcard, card);
	snd_opti9xx_pnp_is_probed = 1;
	return 0;
}

static void __devexit snd_opti9xx_pnp_remove(struct pnp_card_link * pcard)
{
	snd_card_free(pnp_get_card_drvdata(pcard));
	pnp_set_card_drvdata(pcard, NULL);
	snd_opti9xx_pnp_is_probed = 0;
}

static struct pnp_card_driver opti9xx_pnpc_driver = {
	.flags		= PNP_DRIVER_RES_DISABLE,
	.name		= "opti9xx",
	.id_table	= snd_opti9xx_pnpids,
	.probe		= snd_opti9xx_pnp_probe,
	.remove		= __devexit_p(snd_opti9xx_pnp_remove),
};
#endif

#ifdef OPTi93X
#define CHIP_NAME	"82C93x"
#else
#define CHIP_NAME	"82C92x"
#endif

static int __init alsa_card_opti9xx_init(void)
{
#ifdef CONFIG_PNP
	pnp_register_card_driver(&opti9xx_pnpc_driver);
	if (snd_opti9xx_pnp_is_probed)
		return 0;
	pnp_unregister_card_driver(&opti9xx_pnpc_driver);
#endif
	return isa_register_driver(&snd_opti9xx_driver, 1);
}

static void __exit alsa_card_opti9xx_exit(void)
{
	if (!snd_opti9xx_pnp_is_probed) {
		isa_unregister_driver(&snd_opti9xx_driver);
		return;
	}
#ifdef CONFIG_PNP
	pnp_unregister_card_driver(&opti9xx_pnpc_driver);
#endif
}

module_init(alsa_card_opti9xx_init)
module_exit(alsa_card_opti9xx_exit)
