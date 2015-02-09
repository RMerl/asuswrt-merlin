/***************************************************************************
 * Plug-in for TAS5110C1B image sensor connected to the SN9C1xx PC Camera  *
 * Controllers                                                             *
 *                                                                         *
 * Copyright (C) 2004-2007 by Luca Risolia <luca.risolia@studio.unibo.it>  *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 2 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program; if not, write to the Free Software             *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.               *
 ***************************************************************************/

#include "sn9c102_sensor.h"
#include "sn9c102_devtable.h"


static int tas5110c1b_init(struct sn9c102_device* cam)
{
	int err = 0;

	err = sn9c102_write_const_regs(cam, {0x01, 0x01}, {0x44, 0x01},
				       {0x00, 0x10}, {0x00, 0x11},
				       {0x0a, 0x14}, {0x60, 0x17},
				       {0x06, 0x18}, {0xfb, 0x19});

	err += sn9c102_i2c_write(cam, 0xc0, 0x80);

	return err;
}


static int tas5110c1b_set_ctrl(struct sn9c102_device* cam,
			       const struct v4l2_control* ctrl)
{
	int err = 0;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		err += sn9c102_i2c_write(cam, 0x20, 0xf6 - ctrl->value);
		break;
	default:
		return -EINVAL;
	}

	return err ? -EIO : 0;
}


static int tas5110c1b_set_crop(struct sn9c102_device* cam,
			       const struct v4l2_rect* rect)
{
	struct sn9c102_sensor* s = sn9c102_get_sensor(cam);
	int err = 0;
	u8 h_start = (u8)(rect->left - s->cropcap.bounds.left) + 69,
	   v_start = (u8)(rect->top - s->cropcap.bounds.top) + 9;

	err += sn9c102_write_reg(cam, h_start, 0x12);
	err += sn9c102_write_reg(cam, v_start, 0x13);

	/* Don't change ! */
	err += sn9c102_write_reg(cam, 0x14, 0x1a);
	err += sn9c102_write_reg(cam, 0x0a, 0x1b);
	err += sn9c102_write_reg(cam, sn9c102_pread_reg(cam, 0x19), 0x19);

	return err;
}


static int tas5110c1b_set_pix_format(struct sn9c102_device* cam,
				     const struct v4l2_pix_format* pix)
{
	int err = 0;

	if (pix->pixelformat == V4L2_PIX_FMT_SN9C10X)
		err += sn9c102_write_reg(cam, 0x2b, 0x19);
	else
		err += sn9c102_write_reg(cam, 0xfb, 0x19);

	return err;
}


static const struct sn9c102_sensor tas5110c1b = {
	.name = "TAS5110C1B",
	.maintainer = "Luca Risolia <luca.risolia@studio.unibo.it>",
	.supported_bridge = BRIDGE_SN9C101 | BRIDGE_SN9C102,
	.sysfs_ops = SN9C102_I2C_WRITE,
	.frequency = SN9C102_I2C_100KHZ,
	.interface = SN9C102_I2C_3WIRES,
	.init = &tas5110c1b_init,
	.qctrl = {
		{
			.id = V4L2_CID_GAIN,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "global gain",
			.minimum = 0x00,
			.maximum = 0xf6,
			.step = 0x01,
			.default_value = 0x40,
			.flags = 0,
		},
	},
	.set_ctrl = &tas5110c1b_set_ctrl,
	.cropcap = {
		.bounds = {
			.left = 0,
			.top = 0,
			.width = 352,
			.height = 288,
		},
		.defrect = {
			.left = 0,
			.top = 0,
			.width = 352,
			.height = 288,
		},
	},
	.set_crop = &tas5110c1b_set_crop,
	.pix_format = {
		.width = 352,
		.height = 288,
		.pixelformat = V4L2_PIX_FMT_SBGGR8,
		.priv = 8,
	},
	.set_pix_format = &tas5110c1b_set_pix_format
};


int sn9c102_probe_tas5110c1b(struct sn9c102_device* cam)
{
	const struct usb_device_id tas5110c1b_id_table[] = {
		{ USB_DEVICE(0x0c45, 0x6001), },
		{ USB_DEVICE(0x0c45, 0x6005), },
		{ USB_DEVICE(0x0c45, 0x60ab), },
		{ }
	};

	/* Sensor detection is based on USB pid/vid */
	if (!sn9c102_match_id(cam, tas5110c1b_id_table))
		return -ENODEV;

	sn9c102_attach_sensor(cam, &tas5110c1b);

	return 0;
}
