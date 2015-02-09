/*
 * radio-timb.c Timberdale FPGA Radio driver
 * Copyright (c) 2009 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/version.h>
#include <linux/io.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <media/timb_radio.h>

#define DRIVER_NAME "timb-radio"

struct timbradio {
	struct timb_radio_platform_data	pdata;
	struct v4l2_subdev	*sd_tuner;
	struct v4l2_subdev	*sd_dsp;
	struct video_device	video_dev;
	struct v4l2_device	v4l2_dev;
};


static int timbradio_vidioc_querycap(struct file *file, void  *priv,
	struct v4l2_capability *v)
{
	strlcpy(v->driver, DRIVER_NAME, sizeof(v->driver));
	strlcpy(v->card, "Timberdale Radio", sizeof(v->card));
	snprintf(v->bus_info, sizeof(v->bus_info), "platform:"DRIVER_NAME);
	v->version = KERNEL_VERSION(0, 0, 1);
	v->capabilities = V4L2_CAP_TUNER | V4L2_CAP_RADIO;
	return 0;
}

static int timbradio_vidioc_g_tuner(struct file *file, void *priv,
	struct v4l2_tuner *v)
{
	struct timbradio *tr = video_drvdata(file);
	return v4l2_subdev_call(tr->sd_tuner, tuner, g_tuner, v);
}

static int timbradio_vidioc_s_tuner(struct file *file, void *priv,
	struct v4l2_tuner *v)
{
	struct timbradio *tr = video_drvdata(file);
	return v4l2_subdev_call(tr->sd_tuner, tuner, s_tuner, v);
}

static int timbradio_vidioc_g_input(struct file *filp, void *priv,
	unsigned int *i)
{
	*i = 0;
	return 0;
}

static int timbradio_vidioc_s_input(struct file *filp, void *priv,
	unsigned int i)
{
	return i ? -EINVAL : 0;
}

static int timbradio_vidioc_g_audio(struct file *file, void *priv,
	struct v4l2_audio *a)
{
	a->index = 0;
	strlcpy(a->name, "Radio", sizeof(a->name));
	a->capability = V4L2_AUDCAP_STEREO;
	return 0;
}

static int timbradio_vidioc_s_audio(struct file *file, void *priv,
	struct v4l2_audio *a)
{
	return a->index ? -EINVAL : 0;
}

static int timbradio_vidioc_s_frequency(struct file *file, void *priv,
	struct v4l2_frequency *f)
{
	struct timbradio *tr = video_drvdata(file);
	return v4l2_subdev_call(tr->sd_tuner, tuner, s_frequency, f);
}

static int timbradio_vidioc_g_frequency(struct file *file, void *priv,
	struct v4l2_frequency *f)
{
	struct timbradio *tr = video_drvdata(file);
	return v4l2_subdev_call(tr->sd_tuner, tuner, g_frequency, f);
}

static int timbradio_vidioc_queryctrl(struct file *file, void *priv,
	struct v4l2_queryctrl *qc)
{
	struct timbradio *tr = video_drvdata(file);
	return v4l2_subdev_call(tr->sd_dsp, core, queryctrl, qc);
}

static int timbradio_vidioc_g_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	struct timbradio *tr = video_drvdata(file);
	return v4l2_subdev_call(tr->sd_dsp, core, g_ctrl, ctrl);
}

static int timbradio_vidioc_s_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	struct timbradio *tr = video_drvdata(file);
	return v4l2_subdev_call(tr->sd_dsp, core, s_ctrl, ctrl);
}

static const struct v4l2_ioctl_ops timbradio_ioctl_ops = {
	.vidioc_querycap	= timbradio_vidioc_querycap,
	.vidioc_g_tuner		= timbradio_vidioc_g_tuner,
	.vidioc_s_tuner		= timbradio_vidioc_s_tuner,
	.vidioc_g_frequency	= timbradio_vidioc_g_frequency,
	.vidioc_s_frequency	= timbradio_vidioc_s_frequency,
	.vidioc_g_input		= timbradio_vidioc_g_input,
	.vidioc_s_input		= timbradio_vidioc_s_input,
	.vidioc_g_audio		= timbradio_vidioc_g_audio,
	.vidioc_s_audio		= timbradio_vidioc_s_audio,
	.vidioc_queryctrl	= timbradio_vidioc_queryctrl,
	.vidioc_g_ctrl		= timbradio_vidioc_g_ctrl,
	.vidioc_s_ctrl		= timbradio_vidioc_s_ctrl
};

static const struct v4l2_file_operations timbradio_fops = {
	.owner		= THIS_MODULE,
	.ioctl		= video_ioctl2,
};

static int __devinit timbradio_probe(struct platform_device *pdev)
{
	struct timb_radio_platform_data *pdata = pdev->dev.platform_data;
	struct timbradio *tr;
	int err;

	if (!pdata) {
		dev_err(&pdev->dev, "Platform data missing\n");
		err = -EINVAL;
		goto err;
	}

	tr = kzalloc(sizeof(*tr), GFP_KERNEL);
	if (!tr) {
		err = -ENOMEM;
		goto err;
	}

	tr->pdata = *pdata;

	strlcpy(tr->video_dev.name, "Timberdale Radio",
		sizeof(tr->video_dev.name));
	tr->video_dev.fops = &timbradio_fops;
	tr->video_dev.ioctl_ops = &timbradio_ioctl_ops;
	tr->video_dev.release = video_device_release_empty;
	tr->video_dev.minor = -1;

	strlcpy(tr->v4l2_dev.name, DRIVER_NAME, sizeof(tr->v4l2_dev.name));
	err = v4l2_device_register(NULL, &tr->v4l2_dev);
	if (err)
		goto err_v4l2_dev;

	tr->video_dev.v4l2_dev = &tr->v4l2_dev;

	err = video_register_device(&tr->video_dev, VFL_TYPE_RADIO, -1);
	if (err) {
		dev_err(&pdev->dev, "Error reg video\n");
		goto err_video_req;
	}

	video_set_drvdata(&tr->video_dev, tr);

	platform_set_drvdata(pdev, tr);
	return 0;

err_video_req:
	video_device_release_empty(&tr->video_dev);
	v4l2_device_unregister(&tr->v4l2_dev);
err_v4l2_dev:
	kfree(tr);
err:
	dev_err(&pdev->dev, "Failed to register: %d\n", err);

	return err;
}

static int __devexit timbradio_remove(struct platform_device *pdev)
{
	struct timbradio *tr = platform_get_drvdata(pdev);

	video_unregister_device(&tr->video_dev);
	video_device_release_empty(&tr->video_dev);

	v4l2_device_unregister(&tr->v4l2_dev);

	kfree(tr);

	return 0;
}

static struct platform_driver timbradio_platform_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= timbradio_probe,
	.remove		= timbradio_remove,
};

/*--------------------------------------------------------------------------*/

static int __init timbradio_init(void)
{
	return platform_driver_register(&timbradio_platform_driver);
}

static void __exit timbradio_exit(void)
{
	platform_driver_unregister(&timbradio_platform_driver);
}

module_init(timbradio_init);
module_exit(timbradio_exit);

MODULE_DESCRIPTION("Timberdale Radio driver");
MODULE_AUTHOR("Mocean Laboratories <info@mocean-labs.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:"DRIVER_NAME);
