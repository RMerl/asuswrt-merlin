/*
 * ov534-ov7xxx gspca driver
 *
 * Copyright (C) 2008 Antonio Ospite <ospite@studenti.unina.it>
 * Copyright (C) 2008 Jim Paris <jim@jtan.com>
 * Copyright (C) 2009 Jean-Francois Moine http://moinejf.free.fr
 *
 * Based on a prototype written by Mark Ferrell <majortrips@gmail.com>
 * USB protocol reverse engineered by Jim Paris <jim@jtan.com>
 * https://jim.sh/svn/jim/devl/playstation/ps3/eye/test/
 *
 * PS3 Eye camera enhanced by Richard Kaswy http://kaswy.free.fr
 * PS3 Eye camera - brightness, contrast, awb, agc, aec controls
 *                  added by Max Thrun <bear24rw@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define MODULE_NAME "ov534"

#include "gspca.h"

#define OV534_REG_ADDRESS	0xf1	/* sensor address */
#define OV534_REG_SUBADDR	0xf2
#define OV534_REG_WRITE		0xf3
#define OV534_REG_READ		0xf4
#define OV534_REG_OPERATION	0xf5
#define OV534_REG_STATUS	0xf6

#define OV534_OP_WRITE_3	0x37
#define OV534_OP_WRITE_2	0x33
#define OV534_OP_READ_2		0xf9

#define CTRL_TIMEOUT 500

MODULE_AUTHOR("Antonio Ospite <ospite@studenti.unina.it>");
MODULE_DESCRIPTION("GSPCA/OV534 USB Camera Driver");
MODULE_LICENSE("GPL");

/* controls */
enum e_ctrl {
	BRIGHTNESS,
	CONTRAST,
	GAIN,
	EXPOSURE,
	AGC,
	AWB,
	AEC,
	SHARPNESS,
	HFLIP,
	VFLIP,
	COLORS,
	LIGHTFREQ,
	NCTRLS		/* number of controls */
};

/* specific webcam descriptor */
struct sd {
	struct gspca_dev gspca_dev;	/* !! must be the first item */

	struct gspca_ctrl ctrls[NCTRLS];

	__u32 last_pts;
	u16 last_fid;
	u8 frame_rate;

	u8 sensor;
};
enum sensors {
	SENSOR_OV767x,
	SENSOR_OV772x,
	NSENSORS
};

/* V4L2 controls supported by the driver */
static void setbrightness(struct gspca_dev *gspca_dev);
static void setcontrast(struct gspca_dev *gspca_dev);
static void setgain(struct gspca_dev *gspca_dev);
static void setexposure(struct gspca_dev *gspca_dev);
static int sd_setagc(struct gspca_dev *gspca_dev, __s32 val);
static void setawb(struct gspca_dev *gspca_dev);
static void setaec(struct gspca_dev *gspca_dev);
static void setsharpness(struct gspca_dev *gspca_dev);
static void sethvflip(struct gspca_dev *gspca_dev);
static void setcolors(struct gspca_dev *gspca_dev);
static void setlightfreq(struct gspca_dev *gspca_dev);

static int sd_start(struct gspca_dev *gspca_dev);
static void sd_stopN(struct gspca_dev *gspca_dev);

static const struct ctrl sd_ctrls[] = {
[BRIGHTNESS] = {
		{
			.id      = V4L2_CID_BRIGHTNESS,
			.type    = V4L2_CTRL_TYPE_INTEGER,
			.name    = "Brightness",
			.minimum = 0,
			.maximum = 255,
			.step    = 1,
			.default_value = 0,
		},
		.set_control = setbrightness
	},
[CONTRAST] = {
		{
			.id      = V4L2_CID_CONTRAST,
			.type    = V4L2_CTRL_TYPE_INTEGER,
			.name    = "Contrast",
			.minimum = 0,
			.maximum = 255,
			.step    = 1,
			.default_value = 32,
		},
		.set_control = setcontrast
	},
[GAIN] = {
		{
			.id      = V4L2_CID_GAIN,
			.type    = V4L2_CTRL_TYPE_INTEGER,
			.name    = "Main Gain",
			.minimum = 0,
			.maximum = 63,
			.step    = 1,
			.default_value = 20,
		},
		.set_control = setgain
	},
[EXPOSURE] = {
		{
			.id      = V4L2_CID_EXPOSURE,
			.type    = V4L2_CTRL_TYPE_INTEGER,
			.name    = "Exposure",
			.minimum = 0,
			.maximum = 255,
			.step    = 1,
			.default_value = 120,
		},
		.set_control = setexposure
	},
[AGC] = {
		{
			.id      = V4L2_CID_AUTOGAIN,
			.type    = V4L2_CTRL_TYPE_BOOLEAN,
			.name    = "Auto Gain",
			.minimum = 0,
			.maximum = 1,
			.step    = 1,
			.default_value = 1,
		},
		.set = sd_setagc
	},
[AWB] = {
		{
			.id      = V4L2_CID_AUTO_WHITE_BALANCE,
			.type    = V4L2_CTRL_TYPE_BOOLEAN,
			.name    = "Auto White Balance",
			.minimum = 0,
			.maximum = 1,
			.step    = 1,
			.default_value = 1,
		},
		.set_control = setawb
	},
[AEC] = {
		{
			.id      = V4L2_CID_EXPOSURE_AUTO,
			.type    = V4L2_CTRL_TYPE_BOOLEAN,
			.name    = "Auto Exposure",
			.minimum = 0,
			.maximum = 1,
			.step    = 1,
			.default_value = 1,
		},
		.set_control = setaec
	},
[SHARPNESS] = {
		{
			.id      = V4L2_CID_SHARPNESS,
			.type    = V4L2_CTRL_TYPE_INTEGER,
			.name    = "Sharpness",
			.minimum = 0,
			.maximum = 63,
			.step    = 1,
			.default_value = 0,
		},
		.set_control = setsharpness
	},
[HFLIP] = {
		{
			.id      = V4L2_CID_HFLIP,
			.type    = V4L2_CTRL_TYPE_BOOLEAN,
			.name    = "HFlip",
			.minimum = 0,
			.maximum = 1,
			.step    = 1,
			.default_value = 0,
		},
		.set_control = sethvflip
	},
[VFLIP] = {
		{
			.id      = V4L2_CID_VFLIP,
			.type    = V4L2_CTRL_TYPE_BOOLEAN,
			.name    = "VFlip",
			.minimum = 0,
			.maximum = 1,
			.step    = 1,
			.default_value = 0,
		},
		.set_control = sethvflip
	},
[COLORS] = {
		{
			.id      = V4L2_CID_SATURATION,
			.type    = V4L2_CTRL_TYPE_INTEGER,
			.name    = "Saturation",
			.minimum = 0,
			.maximum = 6,
			.step    = 1,
			.default_value = 3,
		},
		.set_control = setcolors
	},
[LIGHTFREQ] = {
		{
			.id      = V4L2_CID_POWER_LINE_FREQUENCY,
			.type    = V4L2_CTRL_TYPE_MENU,
			.name    = "Light Frequency Filter",
			.minimum = 0,
			.maximum = 1,
			.step    = 1,
			.default_value = 0,
		},
		.set_control = setlightfreq
	},
};

static const struct v4l2_pix_format ov772x_mode[] = {
	{320, 240, V4L2_PIX_FMT_YUYV, V4L2_FIELD_NONE,
	 .bytesperline = 320 * 2,
	 .sizeimage = 320 * 240 * 2,
	 .colorspace = V4L2_COLORSPACE_SRGB,
	 .priv = 1},
	{640, 480, V4L2_PIX_FMT_YUYV, V4L2_FIELD_NONE,
	 .bytesperline = 640 * 2,
	 .sizeimage = 640 * 480 * 2,
	 .colorspace = V4L2_COLORSPACE_SRGB,
	 .priv = 0},
};
static const struct v4l2_pix_format ov767x_mode[] = {
	{320, 240, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG},
	{640, 480, V4L2_PIX_FMT_JPEG, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 * 480 * 3 / 8 + 590,
		.colorspace = V4L2_COLORSPACE_JPEG},
};

static const u8 qvga_rates[] = {125, 100, 75, 60, 50, 40, 30};
static const u8 vga_rates[] = {60, 50, 40, 30, 15};

static const struct framerates ov772x_framerates[] = {
	{ /* 320x240 */
		.rates = qvga_rates,
		.nrates = ARRAY_SIZE(qvga_rates),
	},
	{ /* 640x480 */
		.rates = vga_rates,
		.nrates = ARRAY_SIZE(vga_rates),
	},
};

struct reg_array {
	const u8 (*val)[2];
	int len;
};

static const u8 bridge_init_767x[][2] = {
/* comments from the ms-win file apollo7670.set */
/* str1 */
	{0xf1, 0x42},
	{0x88, 0xf8},
	{0x89, 0xff},
	{0x76, 0x03},
	{0x92, 0x03},
	{0x95, 0x10},
	{0xe2, 0x00},
	{0xe7, 0x3e},
	{0x8d, 0x1c},
	{0x8e, 0x00},
	{0x8f, 0x00},
	{0x1f, 0x00},
	{0xc3, 0xf9},
	{0x89, 0xff},
	{0x88, 0xf8},
	{0x76, 0x03},
	{0x92, 0x01},
	{0x93, 0x18},
	{0x1c, 0x00},
	{0x1d, 0x48},
	{0x1d, 0x00},
	{0x1d, 0xff},
	{0x1d, 0x02},
	{0x1d, 0x58},
	{0x1d, 0x00},
	{0x1c, 0x0a},
	{0x1d, 0x0a},
	{0x1d, 0x0e},
	{0xc0, 0x50},	/* HSize 640 */
	{0xc1, 0x3c},	/* VSize 480 */
	{0x34, 0x05},	/* enable Audio Suspend mode */
	{0xc2, 0x0c},	/* Input YUV */
	{0xc3, 0xf9},	/* enable PRE */
	{0x34, 0x05},	/* enable Audio Suspend mode */
	{0xe7, 0x2e},	/* this solves failure of "SuspendResumeTest" */
	{0x31, 0xf9},	/* enable 1.8V Suspend */
	{0x35, 0x02},	/* turn on JPEG */
	{0xd9, 0x10},
	{0x25, 0x42},	/* GPIO[8]:Input */
	{0x94, 0x11},	/* If the default setting is loaded when
			 * system boots up, this flag is closed here */
};
static const u8 sensor_init_767x[][2] = {
	{0x12, 0x80},
	{0x11, 0x03},
	{0x3a, 0x04},
	{0x12, 0x00},
	{0x17, 0x13},
	{0x18, 0x01},
	{0x32, 0xb6},
	{0x19, 0x02},
	{0x1a, 0x7a},
	{0x03, 0x0a},
	{0x0c, 0x00},
	{0x3e, 0x00},
	{0x70, 0x3a},
	{0x71, 0x35},
	{0x72, 0x11},
	{0x73, 0xf0},
	{0xa2, 0x02},
	{0x7a, 0x2a},	/* set Gamma=1.6 below */
	{0x7b, 0x12},
	{0x7c, 0x1d},
	{0x7d, 0x2d},
	{0x7e, 0x45},
	{0x7f, 0x50},
	{0x80, 0x59},
	{0x81, 0x62},
	{0x82, 0x6b},
	{0x83, 0x73},
	{0x84, 0x7b},
	{0x85, 0x8a},
	{0x86, 0x98},
	{0x87, 0xb2},
	{0x88, 0xca},
	{0x89, 0xe0},
	{0x13, 0xe0},
	{0x00, 0x00},
	{0x10, 0x00},
	{0x0d, 0x40},
	{0x14, 0x38},	/* gain max 16x */
	{0xa5, 0x05},
	{0xab, 0x07},
	{0x24, 0x95},
	{0x25, 0x33},
	{0x26, 0xe3},
	{0x9f, 0x78},
	{0xa0, 0x68},
	{0xa1, 0x03},
	{0xa6, 0xd8},
	{0xa7, 0xd8},
	{0xa8, 0xf0},
	{0xa9, 0x90},
	{0xaa, 0x94},
	{0x13, 0xe5},
	{0x0e, 0x61},
	{0x0f, 0x4b},
	{0x16, 0x02},
	{0x21, 0x02},
	{0x22, 0x91},
	{0x29, 0x07},
	{0x33, 0x0b},
	{0x35, 0x0b},
	{0x37, 0x1d},
	{0x38, 0x71},
	{0x39, 0x2a},
	{0x3c, 0x78},
	{0x4d, 0x40},
	{0x4e, 0x20},
	{0x69, 0x00},
	{0x6b, 0x4a},
	{0x74, 0x10},
	{0x8d, 0x4f},
	{0x8e, 0x00},
	{0x8f, 0x00},
	{0x90, 0x00},
	{0x91, 0x00},
	{0x96, 0x00},
	{0x9a, 0x80},
	{0xb0, 0x84},
	{0xb1, 0x0c},
	{0xb2, 0x0e},
	{0xb3, 0x82},
	{0xb8, 0x0a},
	{0x43, 0x0a},
	{0x44, 0xf0},
	{0x45, 0x34},
	{0x46, 0x58},
	{0x47, 0x28},
	{0x48, 0x3a},
	{0x59, 0x88},
	{0x5a, 0x88},
	{0x5b, 0x44},
	{0x5c, 0x67},
	{0x5d, 0x49},
	{0x5e, 0x0e},
	{0x6c, 0x0a},
	{0x6d, 0x55},
	{0x6e, 0x11},
	{0x6f, 0x9f},
	{0x6a, 0x40},
	{0x01, 0x40},
	{0x02, 0x40},
	{0x13, 0xe7},
	{0x4f, 0x80},
	{0x50, 0x80},
	{0x51, 0x00},
	{0x52, 0x22},
	{0x53, 0x5e},
	{0x54, 0x80},
	{0x58, 0x9e},
	{0x41, 0x08},
	{0x3f, 0x00},
	{0x75, 0x04},
	{0x76, 0xe1},
	{0x4c, 0x00},
	{0x77, 0x01},
	{0x3d, 0xc2},
	{0x4b, 0x09},
	{0xc9, 0x60},
	{0x41, 0x38},	/* jfm: auto sharpness + auto de-noise  */
	{0x56, 0x40},
	{0x34, 0x11},
	{0x3b, 0xc2},
	{0xa4, 0x8a},	/* Night mode trigger point */
	{0x96, 0x00},
	{0x97, 0x30},
	{0x98, 0x20},
	{0x99, 0x20},
	{0x9a, 0x84},
	{0x9b, 0x29},
	{0x9c, 0x03},
	{0x9d, 0x4c},
	{0x9e, 0x3f},
	{0x78, 0x04},
	{0x79, 0x01},
	{0xc8, 0xf0},
	{0x79, 0x0f},
	{0xc8, 0x00},
	{0x79, 0x10},
	{0xc8, 0x7e},
	{0x79, 0x0a},
	{0xc8, 0x80},
	{0x79, 0x0b},
	{0xc8, 0x01},
	{0x79, 0x0c},
	{0xc8, 0x0f},
	{0x79, 0x0d},
	{0xc8, 0x20},
	{0x79, 0x09},
	{0xc8, 0x80},
	{0x79, 0x02},
	{0xc8, 0xc0},
	{0x79, 0x03},
	{0xc8, 0x20},
	{0x79, 0x26},
};
static const u8 bridge_start_vga_767x[][2] = {
/* str59 JPG */
	{0x94, 0xaa},
	{0xf1, 0x42},
	{0xe5, 0x04},
	{0xc0, 0x50},
	{0xc1, 0x3c},
	{0xc2, 0x0c},
	{0x35, 0x02},	/* turn on JPEG */
	{0xd9, 0x10},
	{0xda, 0x00},	/* for higher clock rate(30fps) */
	{0x34, 0x05},	/* enable Audio Suspend mode */
	{0xc3, 0xf9},	/* enable PRE */
	{0x8c, 0x00},	/* CIF VSize LSB[2:0] */
	{0x8d, 0x1c},	/* output YUV */
/*	{0x34, 0x05},	 * enable Audio Suspend mode (?) */
	{0x50, 0x00},	/* H/V divider=0 */
	{0x51, 0xa0},	/* input H=640/4 */
	{0x52, 0x3c},	/* input V=480/4 */
	{0x53, 0x00},	/* offset X=0 */
	{0x54, 0x00},	/* offset Y=0 */
	{0x55, 0x00},	/* H/V size[8]=0 */
	{0x57, 0x00},	/* H-size[9]=0 */
	{0x5c, 0x00},	/* output size[9:8]=0 */
	{0x5a, 0xa0},	/* output H=640/4 */
	{0x5b, 0x78},	/* output V=480/4 */
	{0x1c, 0x0a},
	{0x1d, 0x0a},
	{0x94, 0x11},
};
static const u8 sensor_start_vga_767x[][2] = {
	{0x11, 0x01},
	{0x1e, 0x04},
	{0x19, 0x02},
	{0x1a, 0x7a},
};
static const u8 bridge_start_qvga_767x[][2] = {
/* str86 JPG */
	{0x94, 0xaa},
	{0xf1, 0x42},
	{0xe5, 0x04},
	{0xc0, 0x80},
	{0xc1, 0x60},
	{0xc2, 0x0c},
	{0x35, 0x02},	/* turn on JPEG */
	{0xd9, 0x10},
	{0xc0, 0x50},	/* CIF HSize 640 */
	{0xc1, 0x3c},	/* CIF VSize 480 */
	{0x8c, 0x00},	/* CIF VSize LSB[2:0] */
	{0x8d, 0x1c},	/* output YUV */
	{0x34, 0x05},	/* enable Audio Suspend mode */
	{0xc2, 0x4c},	/* output YUV and Enable DCW */
	{0xc3, 0xf9},	/* enable PRE */
	{0x1c, 0x00},	/* indirect addressing */
	{0x1d, 0x48},	/* output YUV422 */
	{0x50, 0x89},	/* H/V divider=/2; plus DCW AVG */
	{0x51, 0xa0},	/* DCW input H=640/4 */
	{0x52, 0x78},	/* DCW input V=480/4 */
	{0x53, 0x00},	/* offset X=0 */
	{0x54, 0x00},	/* offset Y=0 */
	{0x55, 0x00},	/* H/V size[8]=0 */
	{0x57, 0x00},	/* H-size[9]=0 */
	{0x5c, 0x00},	/* DCW output size[9:8]=0 */
	{0x5a, 0x50},	/* DCW output H=320/4 */
	{0x5b, 0x3c},	/* DCW output V=240/4 */
	{0x1c, 0x0a},
	{0x1d, 0x0a},
	{0x94, 0x11},
};
static const u8 sensor_start_qvga_767x[][2] = {
	{0x11, 0x01},
	{0x1e, 0x04},
	{0x19, 0x02},
	{0x1a, 0x7a},
};

static const u8 bridge_init_772x[][2] = {
	{ 0xc2, 0x0c },
	{ 0x88, 0xf8 },
	{ 0xc3, 0x69 },
	{ 0x89, 0xff },
	{ 0x76, 0x03 },
	{ 0x92, 0x01 },
	{ 0x93, 0x18 },
	{ 0x94, 0x10 },
	{ 0x95, 0x10 },
	{ 0xe2, 0x00 },
	{ 0xe7, 0x3e },

	{ 0x96, 0x00 },

	{ 0x97, 0x20 },
	{ 0x97, 0x20 },
	{ 0x97, 0x20 },
	{ 0x97, 0x0a },
	{ 0x97, 0x3f },
	{ 0x97, 0x4a },
	{ 0x97, 0x20 },
	{ 0x97, 0x15 },
	{ 0x97, 0x0b },

	{ 0x8e, 0x40 },
	{ 0x1f, 0x81 },
	{ 0x34, 0x05 },
	{ 0xe3, 0x04 },
	{ 0x88, 0x00 },
	{ 0x89, 0x00 },
	{ 0x76, 0x00 },
	{ 0xe7, 0x2e },
	{ 0x31, 0xf9 },
	{ 0x25, 0x42 },
	{ 0x21, 0xf0 },

	{ 0x1c, 0x00 },
	{ 0x1d, 0x40 },
	{ 0x1d, 0x02 }, /* payload size 0x0200 * 4 = 2048 bytes */
	{ 0x1d, 0x00 }, /* payload size */

	{ 0x1d, 0x02 }, /* frame size 0x025800 * 4 = 614400 */
	{ 0x1d, 0x58 }, /* frame size */
	{ 0x1d, 0x00 }, /* frame size */

	{ 0x1c, 0x0a },
	{ 0x1d, 0x08 }, /* turn on UVC header */
	{ 0x1d, 0x0e }, /* .. */

	{ 0x8d, 0x1c },
	{ 0x8e, 0x80 },
	{ 0xe5, 0x04 },

	{ 0xc0, 0x50 },
	{ 0xc1, 0x3c },
	{ 0xc2, 0x0c },
};
static const u8 sensor_init_772x[][2] = {
	{ 0x12, 0x80 },
	{ 0x11, 0x01 },
/*fixme: better have a delay?*/
	{ 0x11, 0x01 },
	{ 0x11, 0x01 },
	{ 0x11, 0x01 },
	{ 0x11, 0x01 },
	{ 0x11, 0x01 },
	{ 0x11, 0x01 },
	{ 0x11, 0x01 },
	{ 0x11, 0x01 },
	{ 0x11, 0x01 },
	{ 0x11, 0x01 },

	{ 0x3d, 0x03 },
	{ 0x17, 0x26 },
	{ 0x18, 0xa0 },
	{ 0x19, 0x07 },
	{ 0x1a, 0xf0 },
	{ 0x32, 0x00 },
	{ 0x29, 0xa0 },
	{ 0x2c, 0xf0 },
	{ 0x65, 0x20 },
	{ 0x11, 0x01 },
	{ 0x42, 0x7f },
	{ 0x63, 0xaa },		/* AWB - was e0 */
	{ 0x64, 0xff },
	{ 0x66, 0x00 },
	{ 0x13, 0xf0 },		/* com8 */
	{ 0x0d, 0x41 },
	{ 0x0f, 0xc5 },
	{ 0x14, 0x11 },

	{ 0x22, 0x7f },
	{ 0x23, 0x03 },
	{ 0x24, 0x40 },
	{ 0x25, 0x30 },
	{ 0x26, 0xa1 },
	{ 0x2a, 0x00 },
	{ 0x2b, 0x00 },
	{ 0x6b, 0xaa },
	{ 0x13, 0xff },		/* AWB */

	{ 0x90, 0x05 },
	{ 0x91, 0x01 },
	{ 0x92, 0x03 },
	{ 0x93, 0x00 },
	{ 0x94, 0x60 },
	{ 0x95, 0x3c },
	{ 0x96, 0x24 },
	{ 0x97, 0x1e },
	{ 0x98, 0x62 },
	{ 0x99, 0x80 },
	{ 0x9a, 0x1e },
	{ 0x9b, 0x08 },
	{ 0x9c, 0x20 },
	{ 0x9e, 0x81 },

	{ 0xa6, 0x04 },
	{ 0x7e, 0x0c },
	{ 0x7f, 0x16 },
	{ 0x80, 0x2a },
	{ 0x81, 0x4e },
	{ 0x82, 0x61 },
	{ 0x83, 0x6f },
	{ 0x84, 0x7b },
	{ 0x85, 0x86 },
	{ 0x86, 0x8e },
	{ 0x87, 0x97 },
	{ 0x88, 0xa4 },
	{ 0x89, 0xaf },
	{ 0x8a, 0xc5 },
	{ 0x8b, 0xd7 },
	{ 0x8c, 0xe8 },
	{ 0x8d, 0x20 },

	{ 0x0c, 0x90 },

	{ 0x2b, 0x00 },
	{ 0x22, 0x7f },
	{ 0x23, 0x03 },
	{ 0x11, 0x01 },
	{ 0x0c, 0xd0 },
	{ 0x64, 0xff },
	{ 0x0d, 0x41 },

	{ 0x14, 0x41 },
	{ 0x0e, 0xcd },
	{ 0xac, 0xbf },
	{ 0x8e, 0x00 },		/* De-noise threshold */
	{ 0x0c, 0xd0 }
};
static const u8 bridge_start_vga_772x[][2] = {
	{0x1c, 0x00},
	{0x1d, 0x40},
	{0x1d, 0x02},
	{0x1d, 0x00},
	{0x1d, 0x02},
	{0x1d, 0x58},
	{0x1d, 0x00},
	{0xc0, 0x50},
	{0xc1, 0x3c},
};
static const u8 sensor_start_vga_772x[][2] = {
	{0x12, 0x00},
	{0x17, 0x26},
	{0x18, 0xa0},
	{0x19, 0x07},
	{0x1a, 0xf0},
	{0x29, 0xa0},
	{0x2c, 0xf0},
	{0x65, 0x20},
};
static const u8 bridge_start_qvga_772x[][2] = {
	{0x1c, 0x00},
	{0x1d, 0x40},
	{0x1d, 0x02},
	{0x1d, 0x00},
	{0x1d, 0x01},
	{0x1d, 0x4b},
	{0x1d, 0x00},
	{0xc0, 0x28},
	{0xc1, 0x1e},
};
static const u8 sensor_start_qvga_772x[][2] = {
	{0x12, 0x40},
	{0x17, 0x3f},
	{0x18, 0x50},
	{0x19, 0x03},
	{0x1a, 0x78},
	{0x29, 0x50},
	{0x2c, 0x78},
	{0x65, 0x2f},
};

static void ov534_reg_write(struct gspca_dev *gspca_dev, u16 reg, u8 val)
{
	struct usb_device *udev = gspca_dev->dev;
	int ret;

	if (gspca_dev->usb_err < 0)
		return;

	PDEBUG(D_USBO, "SET 01 0000 %04x %02x", reg, val);
	gspca_dev->usb_buf[0] = val;
	ret = usb_control_msg(udev,
			      usb_sndctrlpipe(udev, 0),
			      0x01,
			      USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			      0x00, reg, gspca_dev->usb_buf, 1, CTRL_TIMEOUT);
	if (ret < 0) {
		err("write failed %d", ret);
		gspca_dev->usb_err = ret;
	}
}

static u8 ov534_reg_read(struct gspca_dev *gspca_dev, u16 reg)
{
	struct usb_device *udev = gspca_dev->dev;
	int ret;

	if (gspca_dev->usb_err < 0)
		return 0;
	ret = usb_control_msg(udev,
			      usb_rcvctrlpipe(udev, 0),
			      0x01,
			      USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			      0x00, reg, gspca_dev->usb_buf, 1, CTRL_TIMEOUT);
	PDEBUG(D_USBI, "GET 01 0000 %04x %02x", reg, gspca_dev->usb_buf[0]);
	if (ret < 0) {
		err("read failed %d", ret);
		gspca_dev->usb_err = ret;
	}
	return gspca_dev->usb_buf[0];
}

/* Two bits control LED: 0x21 bit 7 and 0x23 bit 7.
 * (direction and output)? */
static void ov534_set_led(struct gspca_dev *gspca_dev, int status)
{
	u8 data;

	PDEBUG(D_CONF, "led status: %d", status);

	data = ov534_reg_read(gspca_dev, 0x21);
	data |= 0x80;
	ov534_reg_write(gspca_dev, 0x21, data);

	data = ov534_reg_read(gspca_dev, 0x23);
	if (status)
		data |= 0x80;
	else
		data &= ~0x80;

	ov534_reg_write(gspca_dev, 0x23, data);

	if (!status) {
		data = ov534_reg_read(gspca_dev, 0x21);
		data &= ~0x80;
		ov534_reg_write(gspca_dev, 0x21, data);
	}
}

static int sccb_check_status(struct gspca_dev *gspca_dev)
{
	u8 data;
	int i;

	for (i = 0; i < 5; i++) {
		data = ov534_reg_read(gspca_dev, OV534_REG_STATUS);

		switch (data) {
		case 0x00:
			return 1;
		case 0x04:
			return 0;
		case 0x03:
			break;
		default:
			PDEBUG(D_ERR, "sccb status 0x%02x, attempt %d/5",
			       data, i + 1);
		}
	}
	return 0;
}

static void sccb_reg_write(struct gspca_dev *gspca_dev, u8 reg, u8 val)
{
	PDEBUG(D_USBO, "sccb write: %02x %02x", reg, val);
	ov534_reg_write(gspca_dev, OV534_REG_SUBADDR, reg);
	ov534_reg_write(gspca_dev, OV534_REG_WRITE, val);
	ov534_reg_write(gspca_dev, OV534_REG_OPERATION, OV534_OP_WRITE_3);

	if (!sccb_check_status(gspca_dev)) {
		err("sccb_reg_write failed");
		gspca_dev->usb_err = -EIO;
	}
}

static u8 sccb_reg_read(struct gspca_dev *gspca_dev, u16 reg)
{
	ov534_reg_write(gspca_dev, OV534_REG_SUBADDR, reg);
	ov534_reg_write(gspca_dev, OV534_REG_OPERATION, OV534_OP_WRITE_2);
	if (!sccb_check_status(gspca_dev))
		err("sccb_reg_read failed 1");

	ov534_reg_write(gspca_dev, OV534_REG_OPERATION, OV534_OP_READ_2);
	if (!sccb_check_status(gspca_dev))
		err("sccb_reg_read failed 2");

	return ov534_reg_read(gspca_dev, OV534_REG_READ);
}

/* output a bridge sequence (reg - val) */
static void reg_w_array(struct gspca_dev *gspca_dev,
			const u8 (*data)[2], int len)
{
	while (--len >= 0) {
		ov534_reg_write(gspca_dev, (*data)[0], (*data)[1]);
		data++;
	}
}

/* output a sensor sequence (reg - val) */
static void sccb_w_array(struct gspca_dev *gspca_dev,
			const u8 (*data)[2], int len)
{
	while (--len >= 0) {
		if ((*data)[0] != 0xff) {
			sccb_reg_write(gspca_dev, (*data)[0], (*data)[1]);
		} else {
			sccb_reg_read(gspca_dev, (*data)[1]);
			sccb_reg_write(gspca_dev, 0xff, 0x00);
		}
		data++;
	}
}

/* ov772x specific controls */
static void set_frame_rate(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int i;
	struct rate_s {
		u8 fps;
		u8 r11;
		u8 r0d;
		u8 re5;
	};
	const struct rate_s *r;
	static const struct rate_s rate_0[] = {	/* 640x480 */
		{60, 0x01, 0xc1, 0x04},
		{50, 0x01, 0x41, 0x02},
		{40, 0x02, 0xc1, 0x04},
		{30, 0x04, 0x81, 0x02},
		{15, 0x03, 0x41, 0x04},
	};
	static const struct rate_s rate_1[] = {	/* 320x240 */
		{125, 0x02, 0x81, 0x02},
		{100, 0x02, 0xc1, 0x04},
		{75, 0x03, 0xc1, 0x04},
		{60, 0x04, 0xc1, 0x04},
		{50, 0x02, 0x41, 0x04},
		{40, 0x03, 0x41, 0x04},
		{30, 0x04, 0x41, 0x04},
	};

	if (sd->sensor != SENSOR_OV772x)
		return;
	if (gspca_dev->cam.cam_mode[gspca_dev->curr_mode].priv == 0) {
		r = rate_0;
		i = ARRAY_SIZE(rate_0);
	} else {
		r = rate_1;
		i = ARRAY_SIZE(rate_1);
	}
	while (--i > 0) {
		if (sd->frame_rate >= r->fps)
			break;
		r++;
	}

	sccb_reg_write(gspca_dev, 0x11, r->r11);
	sccb_reg_write(gspca_dev, 0x0d, r->r0d);
	ov534_reg_write(gspca_dev, 0xe5, r->re5);

	PDEBUG(D_PROBE, "frame_rate: %d", r->fps);
}

static void setbrightness(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int val;

	val = sd->ctrls[BRIGHTNESS].val;
	if (sd->sensor == SENSOR_OV767x) {
		if (val < 0)
			val = 0x80 - val;
		sccb_reg_write(gspca_dev, 0x55, val);	/* bright */
	} else {
		sccb_reg_write(gspca_dev, 0x9b, val);
	}
}

static void setcontrast(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	u8 val;

	val = sd->ctrls[CONTRAST].val;
	if (sd->sensor == SENSOR_OV767x)
		sccb_reg_write(gspca_dev, 0x56, val);	/* contras */
	else
		sccb_reg_write(gspca_dev, 0x9c, val);
}

static void setgain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	u8 val;

	if (sd->ctrls[AGC].val)
		return;

	val = sd->ctrls[GAIN].val;
	switch (val & 0x30) {
	case 0x00:
		val &= 0x0f;
		break;
	case 0x10:
		val &= 0x0f;
		val |= 0x30;
		break;
	case 0x20:
		val &= 0x0f;
		val |= 0x70;
		break;
	default:
/*	case 0x30: */
		val &= 0x0f;
		val |= 0xf0;
		break;
	}
	sccb_reg_write(gspca_dev, 0x00, val);
}

static void setexposure(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	u8 val;

	if (sd->ctrls[AEC].val)
		return;

	val = sd->ctrls[EXPOSURE].val;
	if (sd->sensor == SENSOR_OV767x) {

		/* set only aec[9:2] */
		sccb_reg_write(gspca_dev, 0x10, val);	/* aech */
	} else {

		/* 'val' is one byte and represents half of the exposure value
		 * we are going to set into registers, a two bytes value:
		 *
		 *    MSB: ((u16) val << 1) >> 8   == val >> 7
		 *    LSB: ((u16) val << 1) & 0xff == val << 1
		 */
		sccb_reg_write(gspca_dev, 0x08, val >> 7);
		sccb_reg_write(gspca_dev, 0x10, val << 1);
	}
}

static void setagc(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (sd->ctrls[AGC].val) {
		sccb_reg_write(gspca_dev, 0x13,
				sccb_reg_read(gspca_dev, 0x13) | 0x04);
		sccb_reg_write(gspca_dev, 0x64,
				sccb_reg_read(gspca_dev, 0x64) | 0x03);
	} else {
		sccb_reg_write(gspca_dev, 0x13,
				sccb_reg_read(gspca_dev, 0x13) & ~0x04);
		sccb_reg_write(gspca_dev, 0x64,
				sccb_reg_read(gspca_dev, 0x64) & ~0x03);

		setgain(gspca_dev);
	}
}

static void setawb(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	if (sd->ctrls[AWB].val) {
		sccb_reg_write(gspca_dev, 0x13,
				sccb_reg_read(gspca_dev, 0x13) | 0x02);
		if (sd->sensor == SENSOR_OV772x)
			sccb_reg_write(gspca_dev, 0x63,
				sccb_reg_read(gspca_dev, 0x63) | 0xc0);
	} else {
		sccb_reg_write(gspca_dev, 0x13,
				sccb_reg_read(gspca_dev, 0x13) & ~0x02);
		if (sd->sensor == SENSOR_OV772x)
			sccb_reg_write(gspca_dev, 0x63,
				sccb_reg_read(gspca_dev, 0x63) & ~0xc0);
	}
}

static void setaec(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	u8 data;

	data = sd->sensor == SENSOR_OV767x ?
			0x05 :		/* agc + aec */
			0x01;		/* agc */
	if (sd->ctrls[AEC].val)
		sccb_reg_write(gspca_dev, 0x13,
				sccb_reg_read(gspca_dev, 0x13) | data);
	else {
		sccb_reg_write(gspca_dev, 0x13,
				sccb_reg_read(gspca_dev, 0x13) & ~data);
		if (sd->sensor == SENSOR_OV767x)
			sd->ctrls[EXPOSURE].val =
				sccb_reg_read(gspca_dev, 10);	/* aech */
		else
			setexposure(gspca_dev);
	}
}

static void setsharpness(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	u8 val;

	val = sd->ctrls[SHARPNESS].val;
	sccb_reg_write(gspca_dev, 0x91, val);	/* Auto de-noise threshold */
	sccb_reg_write(gspca_dev, 0x8e, val);	/* De-noise threshold */
}

static void sethvflip(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	u8 val;

	if (sd->sensor == SENSOR_OV767x) {
		val = sccb_reg_read(gspca_dev, 0x1e);	/* mvfp */
		val &= ~0x30;
		if (sd->ctrls[HFLIP].val)
			val |= 0x20;
		if (sd->ctrls[VFLIP].val)
			val |= 0x10;
		sccb_reg_write(gspca_dev, 0x1e, val);
	} else {
		val = sccb_reg_read(gspca_dev, 0x0c);
		val &= ~0xc0;
		if (sd->ctrls[HFLIP].val == 0)
			val |= 0x40;
		if (sd->ctrls[VFLIP].val == 0)
			val |= 0x80;
		sccb_reg_write(gspca_dev, 0x0c, val);
	}
}

static void setcolors(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	u8 val;
	int i;
	static u8 color_tb[][6] = {
		{0x42, 0x42, 0x00, 0x11, 0x30, 0x41},
		{0x52, 0x52, 0x00, 0x16, 0x3c, 0x52},
		{0x66, 0x66, 0x00, 0x1b, 0x4b, 0x66},
		{0x80, 0x80, 0x00, 0x22, 0x5e, 0x80},
		{0x9a, 0x9a, 0x00, 0x29, 0x71, 0x9a},
		{0xb8, 0xb8, 0x00, 0x31, 0x87, 0xb8},
		{0xdd, 0xdd, 0x00, 0x3b, 0xa2, 0xdd},
	};

	val = sd->ctrls[COLORS].val;
	for (i = 0; i < ARRAY_SIZE(color_tb[0]); i++)
		sccb_reg_write(gspca_dev, 0x4f + i, color_tb[val][i]);
}

static void setlightfreq(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	u8 val;

	val = sd->ctrls[LIGHTFREQ].val ? 0x9e : 0x00;
	if (sd->sensor == SENSOR_OV767x) {
		sccb_reg_write(gspca_dev, 0x2a, 0x00);
		if (val)
			val = 0x9d;	/* insert dummy to 25fps for 50Hz */
	}
	sccb_reg_write(gspca_dev, 0x2b, val);
}


/* this function is called at probe time */
static int sd_config(struct gspca_dev *gspca_dev,
		     const struct usb_device_id *id)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct cam *cam;

	cam = &gspca_dev->cam;

	cam->ctrls = sd->ctrls;

	/* the auto white balance control works only when auto gain is set */
	if (sd_ctrls[AGC].qctrl.default_value == 0)
		gspca_dev->ctrl_inac |= (1 << AWB);

	cam->cam_mode = ov772x_mode;
	cam->nmodes = ARRAY_SIZE(ov772x_mode);

	sd->frame_rate = 30;

	return 0;
}

/* this function is called at probe and resume time */
static int sd_init(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	u16 sensor_id;
	static const struct reg_array bridge_init[NSENSORS] = {
	[SENSOR_OV767x] = {bridge_init_767x, ARRAY_SIZE(bridge_init_767x)},
	[SENSOR_OV772x] = {bridge_init_772x, ARRAY_SIZE(bridge_init_772x)},
	};
	static const struct reg_array sensor_init[NSENSORS] = {
	[SENSOR_OV767x] = {sensor_init_767x, ARRAY_SIZE(sensor_init_767x)},
	[SENSOR_OV772x] = {sensor_init_772x, ARRAY_SIZE(sensor_init_772x)},
	};

	/* reset bridge */
	ov534_reg_write(gspca_dev, 0xe7, 0x3a);
	ov534_reg_write(gspca_dev, 0xe0, 0x08);
	msleep(100);

	/* initialize the sensor address */
	ov534_reg_write(gspca_dev, OV534_REG_ADDRESS, 0x42);

	/* reset sensor */
	sccb_reg_write(gspca_dev, 0x12, 0x80);
	msleep(10);

	/* probe the sensor */
	sccb_reg_read(gspca_dev, 0x0a);
	sensor_id = sccb_reg_read(gspca_dev, 0x0a) << 8;
	sccb_reg_read(gspca_dev, 0x0b);
	sensor_id |= sccb_reg_read(gspca_dev, 0x0b);
	PDEBUG(D_PROBE, "Sensor ID: %04x", sensor_id);

	if ((sensor_id & 0xfff0) == 0x7670) {
		sd->sensor = SENSOR_OV767x;
		gspca_dev->ctrl_dis = (1 << GAIN) |
					(1 << AGC) |
					(1 << SHARPNESS);	/* auto */
		sd->ctrls[BRIGHTNESS].min = -127;
		sd->ctrls[BRIGHTNESS].max = 127;
		sd->ctrls[BRIGHTNESS].def = 0;
		sd->ctrls[CONTRAST].max = 0x80;
		sd->ctrls[CONTRAST].def = 0x40;
		sd->ctrls[EXPOSURE].min = 0x08;
		sd->ctrls[EXPOSURE].max = 0x60;
		sd->ctrls[EXPOSURE].def = 0x13;
		sd->ctrls[SHARPNESS].max = 9;
		sd->ctrls[SHARPNESS].def = 4;
		sd->ctrls[HFLIP].def = 1;
		gspca_dev->cam.cam_mode = ov767x_mode;
		gspca_dev->cam.nmodes = ARRAY_SIZE(ov767x_mode);
	} else {
		sd->sensor = SENSOR_OV772x;
		gspca_dev->ctrl_dis = (1 << COLORS);
		gspca_dev->cam.bulk = 1;
		gspca_dev->cam.bulk_size = 16384;
		gspca_dev->cam.bulk_nurbs = 2;
		gspca_dev->cam.mode_framerates = ov772x_framerates;
	}

	/* initialize */
	reg_w_array(gspca_dev, bridge_init[sd->sensor].val,
			bridge_init[sd->sensor].len);
	ov534_set_led(gspca_dev, 1);
	sccb_w_array(gspca_dev, sensor_init[sd->sensor].val,
			sensor_init[sd->sensor].len);
	if (sd->sensor == SENSOR_OV767x)
		sd_start(gspca_dev);
	sd_stopN(gspca_dev);
/*	set_frame_rate(gspca_dev);	*/

	return gspca_dev->usb_err;
}

static int sd_start(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int mode;
	static const struct reg_array bridge_start[NSENSORS][2] = {
	[SENSOR_OV767x] = {{bridge_start_qvga_767x,
					ARRAY_SIZE(bridge_start_qvga_767x)},
			{bridge_start_vga_767x,
					ARRAY_SIZE(bridge_start_vga_767x)}},
	[SENSOR_OV772x] = {{bridge_start_qvga_772x,
					ARRAY_SIZE(bridge_start_qvga_772x)},
			{bridge_start_vga_772x,
					ARRAY_SIZE(bridge_start_vga_772x)}},
	};
	static const struct reg_array sensor_start[NSENSORS][2] = {
	[SENSOR_OV767x] = {{sensor_start_qvga_767x,
					ARRAY_SIZE(sensor_start_qvga_767x)},
			{sensor_start_vga_767x,
					ARRAY_SIZE(sensor_start_vga_767x)}},
	[SENSOR_OV772x] = {{sensor_start_qvga_772x,
					ARRAY_SIZE(sensor_start_qvga_772x)},
			{sensor_start_vga_772x,
					ARRAY_SIZE(sensor_start_vga_772x)}},
	};

	/* (from ms-win trace) */
	if (sd->sensor == SENSOR_OV767x)
		sccb_reg_write(gspca_dev, 0x1e, 0x04);
					/* black sun enable ? */

	mode = gspca_dev->curr_mode;	/* 0: 320x240, 1: 640x480 */
	reg_w_array(gspca_dev, bridge_start[sd->sensor][mode].val,
				bridge_start[sd->sensor][mode].len);
	sccb_w_array(gspca_dev, sensor_start[sd->sensor][mode].val,
				sensor_start[sd->sensor][mode].len);

	set_frame_rate(gspca_dev);

	if (!(gspca_dev->ctrl_dis & (1 << AGC)))
		setagc(gspca_dev);
	setawb(gspca_dev);
	setaec(gspca_dev);
	if (!(gspca_dev->ctrl_dis & (1 << GAIN)))
		setgain(gspca_dev);
	setexposure(gspca_dev);
	setbrightness(gspca_dev);
	setcontrast(gspca_dev);
	if (!(gspca_dev->ctrl_dis & (1 << SHARPNESS)))
		setsharpness(gspca_dev);
	sethvflip(gspca_dev);
	if (!(gspca_dev->ctrl_dis & (1 << COLORS)))
		setcolors(gspca_dev);
	setlightfreq(gspca_dev);

	ov534_set_led(gspca_dev, 1);
	ov534_reg_write(gspca_dev, 0xe0, 0x00);
	return gspca_dev->usb_err;
}

static void sd_stopN(struct gspca_dev *gspca_dev)
{
	ov534_reg_write(gspca_dev, 0xe0, 0x09);
	ov534_set_led(gspca_dev, 0);
}

/* Values for bmHeaderInfo (Video and Still Image Payload Headers, 2.4.3.3) */
#define UVC_STREAM_EOH	(1 << 7)
#define UVC_STREAM_ERR	(1 << 6)
#define UVC_STREAM_STI	(1 << 5)
#define UVC_STREAM_RES	(1 << 4)
#define UVC_STREAM_SCR	(1 << 3)
#define UVC_STREAM_PTS	(1 << 2)
#define UVC_STREAM_EOF	(1 << 1)
#define UVC_STREAM_FID	(1 << 0)

static void sd_pkt_scan(struct gspca_dev *gspca_dev,
			u8 *data, int len)
{
	struct sd *sd = (struct sd *) gspca_dev;
	__u32 this_pts;
	u16 this_fid;
	int remaining_len = len;
	int payload_len;

	payload_len = gspca_dev->cam.bulk ? 2048 : 2040;
	do {
		len = min(remaining_len, payload_len);

		/* Payloads are prefixed with a UVC-style header.  We
		   consider a frame to start when the FID toggles, or the PTS
		   changes.  A frame ends when EOF is set, and we've received
		   the correct number of bytes. */

		/* Verify UVC header.  Header length is always 12 */
		if (data[0] != 12 || len < 12) {
			PDEBUG(D_PACK, "bad header");
			goto discard;
		}

		/* Check errors */
		if (data[1] & UVC_STREAM_ERR) {
			PDEBUG(D_PACK, "payload error");
			goto discard;
		}

		/* Extract PTS and FID */
		if (!(data[1] & UVC_STREAM_PTS)) {
			PDEBUG(D_PACK, "PTS not present");
			goto discard;
		}
		this_pts = (data[5] << 24) | (data[4] << 16)
						| (data[3] << 8) | data[2];
		this_fid = (data[1] & UVC_STREAM_FID) ? 1 : 0;

		/* If PTS or FID has changed, start a new frame. */
		if (this_pts != sd->last_pts || this_fid != sd->last_fid) {
			if (gspca_dev->last_packet_type == INTER_PACKET)
				gspca_frame_add(gspca_dev, LAST_PACKET,
						NULL, 0);
			sd->last_pts = this_pts;
			sd->last_fid = this_fid;
			gspca_frame_add(gspca_dev, FIRST_PACKET,
					data + 12, len - 12);
		/* If this packet is marked as EOF, end the frame */
		} else if (data[1] & UVC_STREAM_EOF) {
			sd->last_pts = 0;
			if (gspca_dev->pixfmt == V4L2_PIX_FMT_YUYV
			 && gspca_dev->image_len + len - 12 !=
				   gspca_dev->width * gspca_dev->height * 2) {
				PDEBUG(D_PACK, "wrong sized frame");
				goto discard;
			}
			gspca_frame_add(gspca_dev, LAST_PACKET,
					data + 12, len - 12);
		} else {

			/* Add the data from this payload */
			gspca_frame_add(gspca_dev, INTER_PACKET,
					data + 12, len - 12);
		}

		/* Done this payload */
		goto scan_next;

discard:
		/* Discard data until a new frame starts. */
		gspca_dev->last_packet_type = DISCARD_PACKET;

scan_next:
		remaining_len -= len;
		data += len;
	} while (remaining_len > 0);
}

static int sd_setagc(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->ctrls[AGC].val = val;

	/* the auto white balance control works only
	 * when auto gain is set */
	if (val) {
		gspca_dev->ctrl_inac &= ~(1 << AWB);
	} else {
		gspca_dev->ctrl_inac |= (1 << AWB);
		if (sd->ctrls[AWB].val) {
			sd->ctrls[AWB].val = 0;
			if (gspca_dev->streaming)
				setawb(gspca_dev);
		}
	}
	if (gspca_dev->streaming)
		setagc(gspca_dev);
	return gspca_dev->usb_err;
}

static int sd_querymenu(struct gspca_dev *gspca_dev,
		struct v4l2_querymenu *menu)
{
	switch (menu->id) {
	case V4L2_CID_POWER_LINE_FREQUENCY:
		switch (menu->index) {
		case 0:         /* V4L2_CID_POWER_LINE_FREQUENCY_DISABLED */
			strcpy((char *) menu->name, "Disabled");
			return 0;
		case 1:         /* V4L2_CID_POWER_LINE_FREQUENCY_50HZ */
			strcpy((char *) menu->name, "50 Hz");
			return 0;
		}
		break;
	}

	return -EINVAL;
}

/* get stream parameters (framerate) */
static void sd_get_streamparm(struct gspca_dev *gspca_dev,
			     struct v4l2_streamparm *parm)
{
	struct v4l2_captureparm *cp = &parm->parm.capture;
	struct v4l2_fract *tpf = &cp->timeperframe;
	struct sd *sd = (struct sd *) gspca_dev;

	cp->capability |= V4L2_CAP_TIMEPERFRAME;
	tpf->numerator = 1;
	tpf->denominator = sd->frame_rate;
}

/* set stream parameters (framerate) */
static void sd_set_streamparm(struct gspca_dev *gspca_dev,
			     struct v4l2_streamparm *parm)
{
	struct v4l2_captureparm *cp = &parm->parm.capture;
	struct v4l2_fract *tpf = &cp->timeperframe;
	struct sd *sd = (struct sd *) gspca_dev;

	/* Set requested framerate */
	sd->frame_rate = tpf->denominator / tpf->numerator;
	if (gspca_dev->streaming)
		set_frame_rate(gspca_dev);

	/* Return the actual framerate */
	tpf->numerator = 1;
	tpf->denominator = sd->frame_rate;
}

/* sub-driver description */
static const struct sd_desc sd_desc = {
	.name     = MODULE_NAME,
	.ctrls    = sd_ctrls,
	.nctrls   = ARRAY_SIZE(sd_ctrls),
	.config   = sd_config,
	.init     = sd_init,
	.start    = sd_start,
	.stopN    = sd_stopN,
	.pkt_scan = sd_pkt_scan,
	.querymenu = sd_querymenu,
	.get_streamparm = sd_get_streamparm,
	.set_streamparm = sd_set_streamparm,
};

/* -- module initialisation -- */
static const struct usb_device_id device_table[] = {
	{USB_DEVICE(0x1415, 0x2000)},
	{USB_DEVICE(0x06f8, 0x3002)},
	{}
};

MODULE_DEVICE_TABLE(usb, device_table);

/* -- device connect -- */
static int sd_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	return gspca_dev_probe(intf, id, &sd_desc, sizeof(struct sd),
				THIS_MODULE);
}

static struct usb_driver sd_driver = {
	.name       = MODULE_NAME,
	.id_table   = device_table,
	.probe      = sd_probe,
	.disconnect = gspca_disconnect,
#ifdef CONFIG_PM
	.suspend    = gspca_suspend,
	.resume     = gspca_resume,
#endif
};

/* -- module insert / remove -- */
static int __init sd_mod_init(void)
{
	return usb_register(&sd_driver);
}

static void __exit sd_mod_exit(void)
{
	usb_deregister(&sd_driver);
}

module_init(sd_mod_init);
module_exit(sd_mod_exit);
