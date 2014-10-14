/*
 * driver/media/radio/radio-tea5764.c
 *
 * Driver for TEA5764 radio chip for linux 2.6.
 * This driver is for TEA5764 chip from NXP, used in EZX phones from Motorola.
 * The I2C protocol is used for communicate with chip.
 *
 * Based in radio-tea5761.c Copyright (C) 2005 Nokia Corporation
 *
 *  Copyright (c) 2008 Fabio Belavenuto <belavenuto@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * History:
 * 2008-12-06   Fabio Belavenuto <belavenuto@gmail.com>
 *              initial code
 *
 * TODO:
 *  add platform_data support for IRQs platform dependencies
 *  add RDS support
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>			/* Initdata			*/
#include <linux/videodev2.h>		/* kernel radio structs		*/
#include <linux/i2c.h>			/* I2C				*/
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <linux/version.h>      	/* for KERNEL_VERSION MACRO     */

#define DRIVER_VERSION	"v0.01"
#define RADIO_VERSION	KERNEL_VERSION(0, 0, 1)

#define DRIVER_AUTHOR	"Fabio Belavenuto <belavenuto@gmail.com>"
#define DRIVER_DESC	"A driver for the TEA5764 radio chip for EZX Phones."

#define PINFO(format, ...)\
	printk(KERN_INFO KBUILD_MODNAME ": "\
		DRIVER_VERSION ": " format "\n", ## __VA_ARGS__)
#define PWARN(format, ...)\
	printk(KERN_WARNING KBUILD_MODNAME ": "\
		DRIVER_VERSION ": " format "\n", ## __VA_ARGS__)
# define PDEBUG(format, ...)\
	printk(KERN_DEBUG KBUILD_MODNAME ": "\
		DRIVER_VERSION ": " format "\n", ## __VA_ARGS__)

/* Frequency limits in MHz -- these are European values.  For Japanese
devices, that would be 76000 and 91000.  */
#define FREQ_MIN  87500
#define FREQ_MAX 108000
#define FREQ_MUL 16

/* TEA5764 registers */
#define TEA5764_MANID		0x002b
#define TEA5764_CHIPID		0x5764

#define TEA5764_INTREG_BLMSK	0x0001
#define TEA5764_INTREG_FRRMSK	0x0002
#define TEA5764_INTREG_LEVMSK	0x0008
#define TEA5764_INTREG_IFMSK	0x0010
#define TEA5764_INTREG_BLMFLAG	0x0100
#define TEA5764_INTREG_FRRFLAG	0x0200
#define TEA5764_INTREG_LEVFLAG	0x0800
#define TEA5764_INTREG_IFFLAG	0x1000

#define TEA5764_FRQSET_SUD	0x8000
#define TEA5764_FRQSET_SM	0x4000

#define TEA5764_TNCTRL_PUPD1	0x8000
#define TEA5764_TNCTRL_PUPD0	0x4000
#define TEA5764_TNCTRL_BLIM	0x2000
#define TEA5764_TNCTRL_SWPM	0x1000
#define TEA5764_TNCTRL_IFCTC	0x0800
#define TEA5764_TNCTRL_AFM	0x0400
#define TEA5764_TNCTRL_SMUTE	0x0200
#define TEA5764_TNCTRL_SNC	0x0100
#define TEA5764_TNCTRL_MU	0x0080
#define TEA5764_TNCTRL_SSL1	0x0040
#define TEA5764_TNCTRL_SSL0	0x0020
#define TEA5764_TNCTRL_HLSI	0x0010
#define TEA5764_TNCTRL_MST	0x0008
#define TEA5764_TNCTRL_SWP	0x0004
#define TEA5764_TNCTRL_DTC	0x0002
#define TEA5764_TNCTRL_AHLSI	0x0001

#define TEA5764_TUNCHK_LEVEL(x)	(((x) & 0x00F0) >> 4)
#define TEA5764_TUNCHK_IFCNT(x) (((x) & 0xFE00) >> 9)
#define TEA5764_TUNCHK_TUNTO	0x0100
#define TEA5764_TUNCHK_LD	0x0008
#define TEA5764_TUNCHK_STEREO	0x0004

#define TEA5764_TESTREG_TRIGFR	0x0800

struct tea5764_regs {
	u16 intreg;				/* INTFLAG & INTMSK */
	u16 frqset;				/* FRQSETMSB & FRQSETLSB */
	u16 tnctrl;				/* TNCTRL1 & TNCTRL2 */
	u16 frqchk;				/* FRQCHKMSB & FRQCHKLSB */
	u16 tunchk;				/* IFCHK & LEVCHK */
	u16 testreg;				/* TESTBITS & TESTMODE */
	u16 rdsstat;				/* RDSSTAT1 & RDSSTAT2 */
	u16 rdslb;				/* RDSLBMSB & RDSLBLSB */
	u16 rdspb;				/* RDSPBMSB & RDSPBLSB */
	u16 rdsbc;				/* RDSBBC & RDSGBC */
	u16 rdsctrl;				/* RDSCTRL1 & RDSCTRL2 */
	u16 rdsbbl;				/* PAUSEDET & RDSBBL */
	u16 manid;				/* MANID1 & MANID2 */
	u16 chipid;				/* CHIPID1 & CHIPID2 */
} __attribute__ ((packed));

struct tea5764_write_regs {
	u8 intreg;				/* INTMSK */
	u16 frqset;				/* FRQSETMSB & FRQSETLSB */
	u16 tnctrl;				/* TNCTRL1 & TNCTRL2 */
	u16 testreg;				/* TESTBITS & TESTMODE */
	u16 rdsctrl;				/* RDSCTRL1 & RDSCTRL2 */
	u16 rdsbbl;				/* PAUSEDET & RDSBBL */
} __attribute__ ((packed));

#ifndef RADIO_TEA5764_XTAL
#define RADIO_TEA5764_XTAL 1
#endif

static int radio_nr = -1;
static int use_xtal = RADIO_TEA5764_XTAL;

struct tea5764_device {
	struct i2c_client		*i2c_client;
	struct video_device		*videodev;
	struct tea5764_regs		regs;
	struct mutex			mutex;
};

/* I2C code related */
int tea5764_i2c_read(struct tea5764_device *radio)
{
	int i;
	u16 *p = (u16 *) &radio->regs;

	struct i2c_msg msgs[1] = {
		{ radio->i2c_client->addr, I2C_M_RD, sizeof(radio->regs),
			(void *)&radio->regs },
	};
	if (i2c_transfer(radio->i2c_client->adapter, msgs, 1) != 1)
		return -EIO;
	for (i = 0; i < sizeof(struct tea5764_regs) / sizeof(u16); i++)
		p[i] = __be16_to_cpu(p[i]);

	return 0;
}

int tea5764_i2c_write(struct tea5764_device *radio)
{
	struct tea5764_write_regs wr;
	struct tea5764_regs *r = &radio->regs;
	struct i2c_msg msgs[1] = {
		{ radio->i2c_client->addr, 0, sizeof(wr), (void *) &wr },
	};
	wr.intreg  = r->intreg & 0xff;
	wr.frqset  = __cpu_to_be16(r->frqset);
	wr.tnctrl  = __cpu_to_be16(r->tnctrl);
	wr.testreg = __cpu_to_be16(r->testreg);
	wr.rdsctrl = __cpu_to_be16(r->rdsctrl);
	wr.rdsbbl  = __cpu_to_be16(r->rdsbbl);
	if (i2c_transfer(radio->i2c_client->adapter, msgs, 1) != 1)
		return -EIO;
	return 0;
}

/* V4L2 code related */
static struct v4l2_queryctrl radio_qctrl[] = {
	{
		.id            = V4L2_CID_AUDIO_MUTE,
		.name          = "Mute",
		.minimum       = 0,
		.maximum       = 1,
		.default_value = 1,
		.type          = V4L2_CTRL_TYPE_BOOLEAN,
	}
};

static void tea5764_power_up(struct tea5764_device *radio)
{
	struct tea5764_regs *r = &radio->regs;

	if (!(r->tnctrl & TEA5764_TNCTRL_PUPD0)) {
		r->tnctrl &= ~(TEA5764_TNCTRL_AFM | TEA5764_TNCTRL_MU |
			       TEA5764_TNCTRL_HLSI);
		if (!use_xtal)
			r->testreg |= TEA5764_TESTREG_TRIGFR;
		else
			r->testreg &= ~TEA5764_TESTREG_TRIGFR;

		r->tnctrl |= TEA5764_TNCTRL_PUPD0;
		tea5764_i2c_write(radio);
	}
}

static void tea5764_power_down(struct tea5764_device *radio)
{
	struct tea5764_regs *r = &radio->regs;

	if (r->tnctrl & TEA5764_TNCTRL_PUPD0) {
		r->tnctrl &= ~TEA5764_TNCTRL_PUPD0;
		tea5764_i2c_write(radio);
	}
}

static void tea5764_set_freq(struct tea5764_device *radio, int freq)
{
	struct tea5764_regs *r = &radio->regs;

	/* formula: (freq [+ or -] 225000) / 8192 */
	if (r->tnctrl & TEA5764_TNCTRL_HLSI)
		r->frqset = (freq + 225000) / 8192;
	else
		r->frqset = (freq - 225000) / 8192;
}

static int tea5764_get_freq(struct tea5764_device *radio)
{
	struct tea5764_regs *r = &radio->regs;

	if (r->tnctrl & TEA5764_TNCTRL_HLSI)
		return (r->frqchk * 8192) - 225000;
	else
		return (r->frqchk * 8192) + 225000;
}

/* tune an frequency, freq is defined by v4l's TUNER_LOW, i.e. 1/16th kHz */
static void tea5764_tune(struct tea5764_device *radio, int freq)
{
	tea5764_set_freq(radio, freq);
	if (tea5764_i2c_write(radio))
		PWARN("Could not set frequency!");
}

static void tea5764_set_audout_mode(struct tea5764_device *radio, int audmode)
{
	struct tea5764_regs *r = &radio->regs;
	int tnctrl = r->tnctrl;

	if (audmode == V4L2_TUNER_MODE_MONO)
		r->tnctrl |= TEA5764_TNCTRL_MST;
	else
		r->tnctrl &= ~TEA5764_TNCTRL_MST;
	if (tnctrl != r->tnctrl)
		tea5764_i2c_write(radio);
}

static int tea5764_get_audout_mode(struct tea5764_device *radio)
{
	struct tea5764_regs *r = &radio->regs;

	if (r->tnctrl & TEA5764_TNCTRL_MST)
		return V4L2_TUNER_MODE_MONO;
	else
		return V4L2_TUNER_MODE_STEREO;
}

static void tea5764_mute(struct tea5764_device *radio, int on)
{
	struct tea5764_regs *r = &radio->regs;
	int tnctrl = r->tnctrl;

	if (on)
		r->tnctrl |= TEA5764_TNCTRL_MU;
	else
		r->tnctrl &= ~TEA5764_TNCTRL_MU;
	if (tnctrl != r->tnctrl)
		tea5764_i2c_write(radio);
}

static int tea5764_is_muted(struct tea5764_device *radio)
{
	return radio->regs.tnctrl & TEA5764_TNCTRL_MU;
}

/* V4L2 vidioc */
static int vidioc_querycap(struct file *file, void  *priv,
					struct v4l2_capability *v)
{
	struct tea5764_device *radio = video_drvdata(file);
	struct video_device *dev = radio->videodev;

	strlcpy(v->driver, dev->dev.driver->name, sizeof(v->driver));
	strlcpy(v->card, dev->name, sizeof(v->card));
	snprintf(v->bus_info, sizeof(v->bus_info),
		 "I2C:%s", dev_name(&dev->dev));
	v->version = RADIO_VERSION;
	v->capabilities = V4L2_CAP_TUNER | V4L2_CAP_RADIO;
	return 0;
}

static int vidioc_g_tuner(struct file *file, void *priv,
				struct v4l2_tuner *v)
{
	struct tea5764_device *radio = video_drvdata(file);
	struct tea5764_regs *r = &radio->regs;

	if (v->index > 0)
		return -EINVAL;

	memset(v, 0, sizeof(*v));
	strcpy(v->name, "FM");
	v->type = V4L2_TUNER_RADIO;
	tea5764_i2c_read(radio);
	v->rangelow   = FREQ_MIN * FREQ_MUL;
	v->rangehigh  = FREQ_MAX * FREQ_MUL;
	v->capability = V4L2_TUNER_CAP_LOW | V4L2_TUNER_CAP_STEREO;
	if (r->tunchk & TEA5764_TUNCHK_STEREO)
		v->rxsubchans = V4L2_TUNER_SUB_STEREO;
	else
		v->rxsubchans = V4L2_TUNER_SUB_MONO;
	v->audmode = tea5764_get_audout_mode(radio);
	v->signal = TEA5764_TUNCHK_LEVEL(r->tunchk) * 0xffff / 0xf;
	v->afc = TEA5764_TUNCHK_IFCNT(r->tunchk);

	return 0;
}

static int vidioc_s_tuner(struct file *file, void *priv,
				struct v4l2_tuner *v)
{
	struct tea5764_device *radio = video_drvdata(file);

	if (v->index > 0)
		return -EINVAL;

	tea5764_set_audout_mode(radio, v->audmode);
	return 0;
}

static int vidioc_s_frequency(struct file *file, void *priv,
				struct v4l2_frequency *f)
{
	struct tea5764_device *radio = video_drvdata(file);

	if (f->tuner != 0 || f->type != V4L2_TUNER_RADIO)
		return -EINVAL;
	if (f->frequency == 0) {
		/* We special case this as a power down control. */
		tea5764_power_down(radio);
	}
	if (f->frequency < (FREQ_MIN * FREQ_MUL))
		return -EINVAL;
	if (f->frequency > (FREQ_MAX * FREQ_MUL))
		return -EINVAL;
	tea5764_power_up(radio);
	tea5764_tune(radio, (f->frequency * 125) / 2);
	return 0;
}

static int vidioc_g_frequency(struct file *file, void *priv,
				struct v4l2_frequency *f)
{
	struct tea5764_device *radio = video_drvdata(file);
	struct tea5764_regs *r = &radio->regs;

	if (f->tuner != 0)
		return -EINVAL;
	tea5764_i2c_read(radio);
	memset(f, 0, sizeof(*f));
	f->type = V4L2_TUNER_RADIO;
	if (r->tnctrl & TEA5764_TNCTRL_PUPD0)
		f->frequency = (tea5764_get_freq(radio) * 2) / 125;
	else
		f->frequency = 0;

	return 0;
}

static int vidioc_queryctrl(struct file *file, void *priv,
			    struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(radio_qctrl); i++) {
		if (qc->id && qc->id == radio_qctrl[i].id) {
			memcpy(qc, &(radio_qctrl[i]), sizeof(*qc));
			return 0;
		}
	}
	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
			    struct v4l2_control *ctrl)
{
	struct tea5764_device *radio = video_drvdata(file);

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_MUTE:
		tea5764_i2c_read(radio);
		ctrl->value = tea5764_is_muted(radio) ? 1 : 0;
		return 0;
	}
	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
			    struct v4l2_control *ctrl)
{
	struct tea5764_device *radio = video_drvdata(file);

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_MUTE:
		tea5764_mute(radio, ctrl->value);
		return 0;
	}
	return -EINVAL;
}

static int vidioc_g_input(struct file *filp, void *priv, unsigned int *i)
{
	*i = 0;
	return 0;
}

static int vidioc_s_input(struct file *filp, void *priv, unsigned int i)
{
	if (i != 0)
		return -EINVAL;
	return 0;
}

static int vidioc_g_audio(struct file *file, void *priv,
			   struct v4l2_audio *a)
{
	if (a->index > 1)
		return -EINVAL;

	strcpy(a->name, "Radio");
	a->capability = V4L2_AUDCAP_STEREO;
	return 0;
}

static int vidioc_s_audio(struct file *file, void *priv,
			   struct v4l2_audio *a)
{
	if (a->index != 0)
		return -EINVAL;

	return 0;
}

/* File system interface */
static const struct v4l2_file_operations tea5764_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= video_ioctl2,
};

static const struct v4l2_ioctl_ops tea5764_ioctl_ops = {
	.vidioc_querycap    = vidioc_querycap,
	.vidioc_g_tuner     = vidioc_g_tuner,
	.vidioc_s_tuner     = vidioc_s_tuner,
	.vidioc_g_audio     = vidioc_g_audio,
	.vidioc_s_audio     = vidioc_s_audio,
	.vidioc_g_input     = vidioc_g_input,
	.vidioc_s_input     = vidioc_s_input,
	.vidioc_g_frequency = vidioc_g_frequency,
	.vidioc_s_frequency = vidioc_s_frequency,
	.vidioc_queryctrl   = vidioc_queryctrl,
	.vidioc_g_ctrl      = vidioc_g_ctrl,
	.vidioc_s_ctrl      = vidioc_s_ctrl,
};

/* V4L2 interface */
static struct video_device tea5764_radio_template = {
	.name		= "TEA5764 FM-Radio",
	.fops           = &tea5764_fops,
	.ioctl_ops 	= &tea5764_ioctl_ops,
	.release	= video_device_release,
};

/* I2C probe: check if the device exists and register with v4l if it is */
static int __devinit tea5764_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct tea5764_device *radio;
	struct tea5764_regs *r;
	int ret;

	PDEBUG("probe");
	radio = kzalloc(sizeof(struct tea5764_device), GFP_KERNEL);
	if (!radio)
		return -ENOMEM;

	mutex_init(&radio->mutex);
	radio->i2c_client = client;
	ret = tea5764_i2c_read(radio);
	if (ret)
		goto errfr;
	r = &radio->regs;
	PDEBUG("chipid = %04X, manid = %04X", r->chipid, r->manid);
	if (r->chipid != TEA5764_CHIPID ||
		(r->manid & 0x0fff) != TEA5764_MANID) {
		PWARN("This chip is not a TEA5764!");
		ret = -EINVAL;
		goto errfr;
	}

	radio->videodev = video_device_alloc();
	if (!(radio->videodev)) {
		ret = -ENOMEM;
		goto errfr;
	}
	memcpy(radio->videodev, &tea5764_radio_template,
		sizeof(tea5764_radio_template));

	i2c_set_clientdata(client, radio);
	video_set_drvdata(radio->videodev, radio);
	radio->videodev->lock = &radio->mutex;

	/* initialize and power off the chip */
	tea5764_i2c_read(radio);
	tea5764_set_audout_mode(radio, V4L2_TUNER_MODE_STEREO);
	tea5764_mute(radio, 1);
	tea5764_power_down(radio);

	ret = video_register_device(radio->videodev, VFL_TYPE_RADIO, radio_nr);
	if (ret < 0) {
		PWARN("Could not register video device!");
		goto errrel;
	}

	PINFO("registered.");
	return 0;
errrel:
	video_device_release(radio->videodev);
errfr:
	kfree(radio);
	return ret;
}

static int __devexit tea5764_i2c_remove(struct i2c_client *client)
{
	struct tea5764_device *radio = i2c_get_clientdata(client);

	PDEBUG("remove");
	if (radio) {
		tea5764_power_down(radio);
		video_unregister_device(radio->videodev);
		kfree(radio);
	}
	return 0;
}

/* I2C subsystem interface */
static const struct i2c_device_id tea5764_id[] = {
	{ "radio-tea5764", 0 },
	{ }					/* Terminating entry */
};
MODULE_DEVICE_TABLE(i2c, tea5764_id);

static struct i2c_driver tea5764_i2c_driver = {
	.driver = {
		.name = "radio-tea5764",
		.owner = THIS_MODULE,
	},
	.probe = tea5764_i2c_probe,
	.remove = __devexit_p(tea5764_i2c_remove),
	.id_table = tea5764_id,
};

/* init the driver */
static int __init tea5764_init(void)
{
	int ret = i2c_add_driver(&tea5764_i2c_driver);

	printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_VERSION ": "
		DRIVER_DESC "\n");
	return ret;
}

/* cleanup the driver */
static void __exit tea5764_exit(void)
{
	i2c_del_driver(&tea5764_i2c_driver);
}

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

module_param(use_xtal, int, 1);
MODULE_PARM_DESC(use_xtal, "Chip have a xtal connected in board");
module_param(radio_nr, int, 0);
MODULE_PARM_DESC(radio_nr, "video4linux device number to use");

module_init(tea5764_init);
module_exit(tea5764_exit);
