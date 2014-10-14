/*
 * drivers/media/video/tvp514x.c
 *
 * TI TVP5146/47 decoder driver
 *
 * Copyright (C) 2008 Texas Instruments Inc
 * Author: Vaibhav Hiremath <hvaibhav@ti.com>
 *
 * Contributors:
 *     Sivaraj R <sivaraj@ti.com>
 *     Brijesh R Jadav <brijesh.j@ti.com>
 *     Hardik Shah <hardik.shah@ti.com>
 *     Manjunath Hadli <mrh@ti.com>
 *     Karicheri Muralidharan <m-karicheri2@ti.com>
 *
 * This package is free software; you can redistribute it and/or modify
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
 *
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/videodev2.h>

#include <media/v4l2-device.h>
#include <media/v4l2-common.h>
#include <media/v4l2-mediabus.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-ctrls.h>
#include <media/tvp514x.h>

#include "tvp514x_regs.h"

/* Module Name */
#define TVP514X_MODULE_NAME		"tvp514x"

/* Private macros for TVP */
#define I2C_RETRY_COUNT                 (5)
#define LOCK_RETRY_COUNT                (5)
#define LOCK_RETRY_DELAY                (200)

/* Debug functions */
static int debug;
module_param(debug, bool, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-1)");

MODULE_AUTHOR("Texas Instruments");
MODULE_DESCRIPTION("TVP514X linux decoder driver");
MODULE_LICENSE("GPL");

/* enum tvp514x_std - enum for supported standards */
enum tvp514x_std {
	STD_NTSC_MJ = 0,
	STD_PAL_BDGHIN,
	STD_INVALID
};

/**
 * struct tvp514x_std_info - Structure to store standard informations
 * @width: Line width in pixels
 * @height:Number of active lines
 * @video_std: Value to write in REG_VIDEO_STD register
 * @standard: v4l2 standard structure information
 */
struct tvp514x_std_info {
	unsigned long width;
	unsigned long height;
	u8 video_std;
	struct v4l2_standard standard;
};

static struct tvp514x_reg tvp514x_reg_list_default[0x40];

static int tvp514x_s_stream(struct v4l2_subdev *sd, int enable);
/**
 * struct tvp514x_decoder - TVP5146/47 decoder object
 * @sd: Subdevice Slave handle
 * @tvp514x_regs: copy of hw's regs with preset values.
 * @pdata: Board specific
 * @ver: Chip version
 * @streaming: TVP5146/47 decoder streaming - enabled or disabled.
 * @current_std: Current standard
 * @num_stds: Number of standards
 * @std_list: Standards list
 * @input: Input routing at chip level
 * @output: Output routing at chip level
 */
struct tvp514x_decoder {
	struct v4l2_subdev sd;
	struct v4l2_ctrl_handler hdl;
	struct tvp514x_reg tvp514x_regs[ARRAY_SIZE(tvp514x_reg_list_default)];
	const struct tvp514x_platform_data *pdata;

	int ver;
	int streaming;

	enum tvp514x_std current_std;
	int num_stds;
	const struct tvp514x_std_info *std_list;
	/* Input and Output Routing parameters */
	u32 input;
	u32 output;
};

/* TVP514x default register values */
static struct tvp514x_reg tvp514x_reg_list_default[] = {
	/* Composite selected */
	{TOK_WRITE, REG_INPUT_SEL, 0x05},
	{TOK_WRITE, REG_AFE_GAIN_CTRL, 0x0F},
	/* Auto mode */
	{TOK_WRITE, REG_VIDEO_STD, 0x00},
	{TOK_WRITE, REG_OPERATION_MODE, 0x00},
	{TOK_SKIP, REG_AUTOSWITCH_MASK, 0x3F},
	{TOK_WRITE, REG_COLOR_KILLER, 0x10},
	{TOK_WRITE, REG_LUMA_CONTROL1, 0x00},
	{TOK_WRITE, REG_LUMA_CONTROL2, 0x00},
	{TOK_WRITE, REG_LUMA_CONTROL3, 0x02},
	{TOK_WRITE, REG_BRIGHTNESS, 0x80},
	{TOK_WRITE, REG_CONTRAST, 0x80},
	{TOK_WRITE, REG_SATURATION, 0x80},
	{TOK_WRITE, REG_HUE, 0x00},
	{TOK_WRITE, REG_CHROMA_CONTROL1, 0x00},
	{TOK_WRITE, REG_CHROMA_CONTROL2, 0x0E},
	/* Reserved */
	{TOK_SKIP, 0x0F, 0x00},
	{TOK_WRITE, REG_COMP_PR_SATURATION, 0x80},
	{TOK_WRITE, REG_COMP_Y_CONTRAST, 0x80},
	{TOK_WRITE, REG_COMP_PB_SATURATION, 0x80},
	/* Reserved */
	{TOK_SKIP, 0x13, 0x00},
	{TOK_WRITE, REG_COMP_Y_BRIGHTNESS, 0x80},
	/* Reserved */
	{TOK_SKIP, 0x15, 0x00},
	/* NTSC timing */
	{TOK_SKIP, REG_AVID_START_PIXEL_LSB, 0x55},
	{TOK_SKIP, REG_AVID_START_PIXEL_MSB, 0x00},
	{TOK_SKIP, REG_AVID_STOP_PIXEL_LSB, 0x25},
	{TOK_SKIP, REG_AVID_STOP_PIXEL_MSB, 0x03},
	/* NTSC timing */
	{TOK_SKIP, REG_HSYNC_START_PIXEL_LSB, 0x00},
	{TOK_SKIP, REG_HSYNC_START_PIXEL_MSB, 0x00},
	{TOK_SKIP, REG_HSYNC_STOP_PIXEL_LSB, 0x40},
	{TOK_SKIP, REG_HSYNC_STOP_PIXEL_MSB, 0x00},
	/* NTSC timing */
	{TOK_SKIP, REG_VSYNC_START_LINE_LSB, 0x04},
	{TOK_SKIP, REG_VSYNC_START_LINE_MSB, 0x00},
	{TOK_SKIP, REG_VSYNC_STOP_LINE_LSB, 0x07},
	{TOK_SKIP, REG_VSYNC_STOP_LINE_MSB, 0x00},
	/* NTSC timing */
	{TOK_SKIP, REG_VBLK_START_LINE_LSB, 0x01},
	{TOK_SKIP, REG_VBLK_START_LINE_MSB, 0x00},
	{TOK_SKIP, REG_VBLK_STOP_LINE_LSB, 0x15},
	{TOK_SKIP, REG_VBLK_STOP_LINE_MSB, 0x00},
	/* Reserved */
	{TOK_SKIP, 0x26, 0x00},
	/* Reserved */
	{TOK_SKIP, 0x27, 0x00},
	{TOK_SKIP, REG_FAST_SWTICH_CONTROL, 0xCC},
	/* Reserved */
	{TOK_SKIP, 0x29, 0x00},
	{TOK_SKIP, REG_FAST_SWTICH_SCART_DELAY, 0x00},
	/* Reserved */
	{TOK_SKIP, 0x2B, 0x00},
	{TOK_SKIP, REG_SCART_DELAY, 0x00},
	{TOK_SKIP, REG_CTI_DELAY, 0x00},
	{TOK_SKIP, REG_CTI_CONTROL, 0x00},
	/* Reserved */
	{TOK_SKIP, 0x2F, 0x00},
	/* Reserved */
	{TOK_SKIP, 0x30, 0x00},
	/* Reserved */
	{TOK_SKIP, 0x31, 0x00},
	/* HS, VS active high */
	{TOK_WRITE, REG_SYNC_CONTROL, 0x00},
	/* 10-bit BT.656 */
	{TOK_WRITE, REG_OUTPUT_FORMATTER1, 0x00},
	/* Enable clk & data */
	{TOK_WRITE, REG_OUTPUT_FORMATTER2, 0x11},
	/* Enable AVID & FLD */
	{TOK_WRITE, REG_OUTPUT_FORMATTER3, 0xEE},
	/* Enable VS & HS */
	{TOK_WRITE, REG_OUTPUT_FORMATTER4, 0xAF},
	{TOK_WRITE, REG_OUTPUT_FORMATTER5, 0xFF},
	{TOK_WRITE, REG_OUTPUT_FORMATTER6, 0xFF},
	/* Clear status */
	{TOK_WRITE, REG_CLEAR_LOST_LOCK, 0x01},
	{TOK_TERM, 0, 0},
};

/**
 * Supported standards -
 *
 * Currently supports two standards only, need to add support for rest of the
 * modes, like SECAM, etc...
 */
static const struct tvp514x_std_info tvp514x_std_list[] = {
	/* Standard: STD_NTSC_MJ */
	[STD_NTSC_MJ] = {
	 .width = NTSC_NUM_ACTIVE_PIXELS,
	 .height = NTSC_NUM_ACTIVE_LINES,
	 .video_std = VIDEO_STD_NTSC_MJ_BIT,
	 .standard = {
		      .index = 0,
		      .id = V4L2_STD_NTSC,
		      .name = "NTSC",
		      .frameperiod = {1001, 30000},
		      .framelines = 525
		     },
	/* Standard: STD_PAL_BDGHIN */
	},
	[STD_PAL_BDGHIN] = {
	 .width = PAL_NUM_ACTIVE_PIXELS,
	 .height = PAL_NUM_ACTIVE_LINES,
	 .video_std = VIDEO_STD_PAL_BDGHIN_BIT,
	 .standard = {
		      .index = 1,
		      .id = V4L2_STD_PAL,
		      .name = "PAL",
		      .frameperiod = {1, 25},
		      .framelines = 625
		     },
	},
	/* Standard: need to add for additional standard */
};


static inline struct tvp514x_decoder *to_decoder(struct v4l2_subdev *sd)
{
	return container_of(sd, struct tvp514x_decoder, sd);
}

static inline struct v4l2_subdev *to_sd(struct v4l2_ctrl *ctrl)
{
	return &container_of(ctrl->handler, struct tvp514x_decoder, hdl)->sd;
}


/**
 * tvp514x_read_reg() - Read a value from a register in an TVP5146/47.
 * @sd: ptr to v4l2_subdev struct
 * @reg: TVP5146/47 register address
 *
 * Returns value read if successful, or non-zero (-1) otherwise.
 */
static int tvp514x_read_reg(struct v4l2_subdev *sd, u8 reg)
{
	int err, retry = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

read_again:

	err = i2c_smbus_read_byte_data(client, reg);
	if (err < 0) {
		if (retry <= I2C_RETRY_COUNT) {
			v4l2_warn(sd, "Read: retry ... %d\n", retry);
			retry++;
			msleep_interruptible(10);
			goto read_again;
		}
	}

	return err;
}

/**
 * dump_reg() - dump the register content of TVP5146/47.
 * @sd: ptr to v4l2_subdev struct
 * @reg: TVP5146/47 register address
 */
static void dump_reg(struct v4l2_subdev *sd, u8 reg)
{
	u32 val;

	val = tvp514x_read_reg(sd, reg);
	v4l2_info(sd, "Reg(0x%.2X): 0x%.2X\n", reg, val);
}

/**
 * tvp514x_write_reg() - Write a value to a register in TVP5146/47
 * @sd: ptr to v4l2_subdev struct
 * @reg: TVP5146/47 register address
 * @val: value to be written to the register
 *
 * Write a value to a register in an TVP5146/47 decoder device.
 * Returns zero if successful, or non-zero otherwise.
 */
static int tvp514x_write_reg(struct v4l2_subdev *sd, u8 reg, u8 val)
{
	int err, retry = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

write_again:

	err = i2c_smbus_write_byte_data(client, reg, val);
	if (err) {
		if (retry <= I2C_RETRY_COUNT) {
			v4l2_warn(sd, "Write: retry ... %d\n", retry);
			retry++;
			msleep_interruptible(10);
			goto write_again;
		}
	}

	return err;
}

/**
 * tvp514x_write_regs() : Initializes a list of TVP5146/47 registers
 * @sd: ptr to v4l2_subdev struct
 * @reglist: list of TVP5146/47 registers and values
 *
 * Initializes a list of TVP5146/47 registers:-
 *		if token is TOK_TERM, then entire write operation terminates
 *		if token is TOK_DELAY, then a delay of 'val' msec is introduced
 *		if token is TOK_SKIP, then the register write is skipped
 *		if token is TOK_WRITE, then the register write is performed
 * Returns zero if successful, or non-zero otherwise.
 */
static int tvp514x_write_regs(struct v4l2_subdev *sd,
			      const struct tvp514x_reg reglist[])
{
	int err;
	const struct tvp514x_reg *next = reglist;

	for (; next->token != TOK_TERM; next++) {
		if (next->token == TOK_DELAY) {
			msleep(next->val);
			continue;
		}

		if (next->token == TOK_SKIP)
			continue;

		err = tvp514x_write_reg(sd, next->reg, (u8) next->val);
		if (err) {
			v4l2_err(sd, "Write failed. Err[%d]\n", err);
			return err;
		}
	}
	return 0;
}

/**
 * tvp514x_query_current_std() : Query the current standard detected by TVP5146/47
 * @sd: ptr to v4l2_subdev struct
 *
 * Returns the current standard detected by TVP5146/47, STD_INVALID if there is no
 * standard detected.
 */
static enum tvp514x_std tvp514x_query_current_std(struct v4l2_subdev *sd)
{
	u8 std, std_status;

	std = tvp514x_read_reg(sd, REG_VIDEO_STD);
	if ((std & VIDEO_STD_MASK) == VIDEO_STD_AUTO_SWITCH_BIT)
		/* use the standard status register */
		std_status = tvp514x_read_reg(sd, REG_VIDEO_STD_STATUS);
	else
		/* use the standard register itself */
		std_status = std;

	switch (std_status & VIDEO_STD_MASK) {
	case VIDEO_STD_NTSC_MJ_BIT:
		return STD_NTSC_MJ;

	case VIDEO_STD_PAL_BDGHIN_BIT:
		return STD_PAL_BDGHIN;

	default:
		return STD_INVALID;
	}

	return STD_INVALID;
}

/* TVP5146/47 register dump function */
static void tvp514x_reg_dump(struct v4l2_subdev *sd)
{
	dump_reg(sd, REG_INPUT_SEL);
	dump_reg(sd, REG_AFE_GAIN_CTRL);
	dump_reg(sd, REG_VIDEO_STD);
	dump_reg(sd, REG_OPERATION_MODE);
	dump_reg(sd, REG_COLOR_KILLER);
	dump_reg(sd, REG_LUMA_CONTROL1);
	dump_reg(sd, REG_LUMA_CONTROL2);
	dump_reg(sd, REG_LUMA_CONTROL3);
	dump_reg(sd, REG_BRIGHTNESS);
	dump_reg(sd, REG_CONTRAST);
	dump_reg(sd, REG_SATURATION);
	dump_reg(sd, REG_HUE);
	dump_reg(sd, REG_CHROMA_CONTROL1);
	dump_reg(sd, REG_CHROMA_CONTROL2);
	dump_reg(sd, REG_COMP_PR_SATURATION);
	dump_reg(sd, REG_COMP_Y_CONTRAST);
	dump_reg(sd, REG_COMP_PB_SATURATION);
	dump_reg(sd, REG_COMP_Y_BRIGHTNESS);
	dump_reg(sd, REG_AVID_START_PIXEL_LSB);
	dump_reg(sd, REG_AVID_START_PIXEL_MSB);
	dump_reg(sd, REG_AVID_STOP_PIXEL_LSB);
	dump_reg(sd, REG_AVID_STOP_PIXEL_MSB);
	dump_reg(sd, REG_HSYNC_START_PIXEL_LSB);
	dump_reg(sd, REG_HSYNC_START_PIXEL_MSB);
	dump_reg(sd, REG_HSYNC_STOP_PIXEL_LSB);
	dump_reg(sd, REG_HSYNC_STOP_PIXEL_MSB);
	dump_reg(sd, REG_VSYNC_START_LINE_LSB);
	dump_reg(sd, REG_VSYNC_START_LINE_MSB);
	dump_reg(sd, REG_VSYNC_STOP_LINE_LSB);
	dump_reg(sd, REG_VSYNC_STOP_LINE_MSB);
	dump_reg(sd, REG_VBLK_START_LINE_LSB);
	dump_reg(sd, REG_VBLK_START_LINE_MSB);
	dump_reg(sd, REG_VBLK_STOP_LINE_LSB);
	dump_reg(sd, REG_VBLK_STOP_LINE_MSB);
	dump_reg(sd, REG_SYNC_CONTROL);
	dump_reg(sd, REG_OUTPUT_FORMATTER1);
	dump_reg(sd, REG_OUTPUT_FORMATTER2);
	dump_reg(sd, REG_OUTPUT_FORMATTER3);
	dump_reg(sd, REG_OUTPUT_FORMATTER4);
	dump_reg(sd, REG_OUTPUT_FORMATTER5);
	dump_reg(sd, REG_OUTPUT_FORMATTER6);
	dump_reg(sd, REG_CLEAR_LOST_LOCK);
}

/**
 * tvp514x_configure() - Configure the TVP5146/47 registers
 * @sd: ptr to v4l2_subdev struct
 * @decoder: ptr to tvp514x_decoder structure
 *
 * Returns zero if successful, or non-zero otherwise.
 */
static int tvp514x_configure(struct v4l2_subdev *sd,
		struct tvp514x_decoder *decoder)
{
	int err;

	/* common register initialization */
	err =
	    tvp514x_write_regs(sd, decoder->tvp514x_regs);
	if (err)
		return err;

	if (debug)
		tvp514x_reg_dump(sd);

	return 0;
}

/**
 * tvp514x_detect() - Detect if an tvp514x is present, and if so which revision.
 * @sd: pointer to standard V4L2 sub-device structure
 * @decoder: pointer to tvp514x_decoder structure
 *
 * A device is considered to be detected if the chip ID (LSB and MSB)
 * registers match the expected values.
 * Any value of the rom version register is accepted.
 * Returns ENODEV error number if no device is detected, or zero
 * if a device is detected.
 */
static int tvp514x_detect(struct v4l2_subdev *sd,
		struct tvp514x_decoder *decoder)
{
	u8 chip_id_msb, chip_id_lsb, rom_ver;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	chip_id_msb = tvp514x_read_reg(sd, REG_CHIP_ID_MSB);
	chip_id_lsb = tvp514x_read_reg(sd, REG_CHIP_ID_LSB);
	rom_ver = tvp514x_read_reg(sd, REG_ROM_VERSION);

	v4l2_dbg(1, debug, sd,
		 "chip id detected msb:0x%x lsb:0x%x rom version:0x%x\n",
		 chip_id_msb, chip_id_lsb, rom_ver);
	if ((chip_id_msb != TVP514X_CHIP_ID_MSB)
		|| ((chip_id_lsb != TVP5146_CHIP_ID_LSB)
		&& (chip_id_lsb != TVP5147_CHIP_ID_LSB))) {
		/* We didn't read the values we expected, so this must not be
		 * an TVP5146/47.
		 */
		v4l2_err(sd, "chip id mismatch msb:0x%x lsb:0x%x\n",
				chip_id_msb, chip_id_lsb);
		return -ENODEV;
	}

	decoder->ver = rom_ver;

	v4l2_info(sd, "%s (Version - 0x%.2x) found at 0x%x (%s)\n",
			client->name, decoder->ver,
			client->addr << 1, client->adapter->name);
	return 0;
}

/**
 * tvp514x_querystd() - V4L2 decoder interface handler for querystd
 * @sd: pointer to standard V4L2 sub-device structure
 * @std_id: standard V4L2 std_id ioctl enum
 *
 * Returns the current standard detected by TVP5146/47. If no active input is
 * detected then *std_id is set to 0 and the function returns 0.
 */
static int tvp514x_querystd(struct v4l2_subdev *sd, v4l2_std_id *std_id)
{
	struct tvp514x_decoder *decoder = to_decoder(sd);
	enum tvp514x_std current_std;
	enum tvp514x_input input_sel;
	u8 sync_lock_status, lock_mask;

	if (std_id == NULL)
		return -EINVAL;

	*std_id = V4L2_STD_UNKNOWN;

	/* query the current standard */
	current_std = tvp514x_query_current_std(sd);
	if (current_std == STD_INVALID)
		return 0;

	input_sel = decoder->input;

	switch (input_sel) {
	case INPUT_CVBS_VI1A:
	case INPUT_CVBS_VI1B:
	case INPUT_CVBS_VI1C:
	case INPUT_CVBS_VI2A:
	case INPUT_CVBS_VI2B:
	case INPUT_CVBS_VI2C:
	case INPUT_CVBS_VI3A:
	case INPUT_CVBS_VI3B:
	case INPUT_CVBS_VI3C:
	case INPUT_CVBS_VI4A:
		lock_mask = STATUS_CLR_SUBCAR_LOCK_BIT |
			STATUS_HORZ_SYNC_LOCK_BIT |
			STATUS_VIRT_SYNC_LOCK_BIT;
		break;

	case INPUT_SVIDEO_VI2A_VI1A:
	case INPUT_SVIDEO_VI2B_VI1B:
	case INPUT_SVIDEO_VI2C_VI1C:
	case INPUT_SVIDEO_VI2A_VI3A:
	case INPUT_SVIDEO_VI2B_VI3B:
	case INPUT_SVIDEO_VI2C_VI3C:
	case INPUT_SVIDEO_VI4A_VI1A:
	case INPUT_SVIDEO_VI4A_VI1B:
	case INPUT_SVIDEO_VI4A_VI1C:
	case INPUT_SVIDEO_VI4A_VI3A:
	case INPUT_SVIDEO_VI4A_VI3B:
	case INPUT_SVIDEO_VI4A_VI3C:
		lock_mask = STATUS_HORZ_SYNC_LOCK_BIT |
			STATUS_VIRT_SYNC_LOCK_BIT;
		break;
		/*Need to add other interfaces*/
	default:
		return -EINVAL;
	}
	/* check whether signal is locked */
	sync_lock_status = tvp514x_read_reg(sd, REG_STATUS1);
	if (lock_mask != (sync_lock_status & lock_mask))
		return 0;	/* No input detected */

	*std_id = decoder->std_list[current_std].standard.id;

	v4l2_dbg(1, debug, sd, "Current STD: %s\n",
			decoder->std_list[current_std].standard.name);
	return 0;
}

/**
 * tvp514x_s_std() - V4L2 decoder interface handler for s_std
 * @sd: pointer to standard V4L2 sub-device structure
 * @std_id: standard V4L2 v4l2_std_id ioctl enum
 *
 * If std_id is supported, sets the requested standard. Otherwise, returns
 * -EINVAL
 */
static int tvp514x_s_std(struct v4l2_subdev *sd, v4l2_std_id std_id)
{
	struct tvp514x_decoder *decoder = to_decoder(sd);
	int err, i;

	for (i = 0; i < decoder->num_stds; i++)
		if (std_id & decoder->std_list[i].standard.id)
			break;

	if ((i == decoder->num_stds) || (i == STD_INVALID))
		return -EINVAL;

	err = tvp514x_write_reg(sd, REG_VIDEO_STD,
				decoder->std_list[i].video_std);
	if (err)
		return err;

	decoder->current_std = i;
	decoder->tvp514x_regs[REG_VIDEO_STD].val =
		decoder->std_list[i].video_std;

	v4l2_dbg(1, debug, sd, "Standard set to: %s\n",
			decoder->std_list[i].standard.name);
	return 0;
}

/**
 * tvp514x_s_routing() - V4L2 decoder interface handler for s_routing
 * @sd: pointer to standard V4L2 sub-device structure
 * @input: input selector for routing the signal
 * @output: output selector for routing the signal
 * @config: config value. Not used
 *
 * If index is valid, selects the requested input. Otherwise, returns -EINVAL if
 * the input is not supported or there is no active signal present in the
 * selected input.
 */
static int tvp514x_s_routing(struct v4l2_subdev *sd,
				u32 input, u32 output, u32 config)
{
	struct tvp514x_decoder *decoder = to_decoder(sd);
	int err;
	enum tvp514x_input input_sel;
	enum tvp514x_output output_sel;
	u8 sync_lock_status, lock_mask;
	int try_count = LOCK_RETRY_COUNT;

	if ((input >= INPUT_INVALID) ||
			(output >= OUTPUT_INVALID))
		/* Index out of bound */
		return -EINVAL;

	/*
	 * For the sequence streamon -> streamoff and again s_input
	 * it fails to lock the signal, since streamoff puts TVP514x
	 * into power off state which leads to failure in sub-sequent s_input.
	 *
	 * So power up the TVP514x device here, since it is important to lock
	 * the signal at this stage.
	 */
	if (!decoder->streaming)
		tvp514x_s_stream(sd, 1);

	input_sel = input;
	output_sel = output;

	err = tvp514x_write_reg(sd, REG_INPUT_SEL, input_sel);
	if (err)
		return err;

	output_sel |= tvp514x_read_reg(sd,
			REG_OUTPUT_FORMATTER1) & 0x7;
	err = tvp514x_write_reg(sd, REG_OUTPUT_FORMATTER1,
			output_sel);
	if (err)
		return err;

	decoder->tvp514x_regs[REG_INPUT_SEL].val = input_sel;
	decoder->tvp514x_regs[REG_OUTPUT_FORMATTER1].val = output_sel;

	/* Clear status */
	msleep(LOCK_RETRY_DELAY);
	err =
	    tvp514x_write_reg(sd, REG_CLEAR_LOST_LOCK, 0x01);
	if (err)
		return err;

	switch (input_sel) {
	case INPUT_CVBS_VI1A:
	case INPUT_CVBS_VI1B:
	case INPUT_CVBS_VI1C:
	case INPUT_CVBS_VI2A:
	case INPUT_CVBS_VI2B:
	case INPUT_CVBS_VI2C:
	case INPUT_CVBS_VI3A:
	case INPUT_CVBS_VI3B:
	case INPUT_CVBS_VI3C:
	case INPUT_CVBS_VI4A:
		lock_mask = STATUS_CLR_SUBCAR_LOCK_BIT |
			STATUS_HORZ_SYNC_LOCK_BIT |
			STATUS_VIRT_SYNC_LOCK_BIT;
		break;

	case INPUT_SVIDEO_VI2A_VI1A:
	case INPUT_SVIDEO_VI2B_VI1B:
	case INPUT_SVIDEO_VI2C_VI1C:
	case INPUT_SVIDEO_VI2A_VI3A:
	case INPUT_SVIDEO_VI2B_VI3B:
	case INPUT_SVIDEO_VI2C_VI3C:
	case INPUT_SVIDEO_VI4A_VI1A:
	case INPUT_SVIDEO_VI4A_VI1B:
	case INPUT_SVIDEO_VI4A_VI1C:
	case INPUT_SVIDEO_VI4A_VI3A:
	case INPUT_SVIDEO_VI4A_VI3B:
	case INPUT_SVIDEO_VI4A_VI3C:
		lock_mask = STATUS_HORZ_SYNC_LOCK_BIT |
			STATUS_VIRT_SYNC_LOCK_BIT;
		break;
	/* Need to add other interfaces*/
	default:
		return -EINVAL;
	}

	while (try_count-- > 0) {
		/* Allow decoder to sync up with new input */
		msleep(LOCK_RETRY_DELAY);

		sync_lock_status = tvp514x_read_reg(sd,
				REG_STATUS1);
		if (lock_mask == (sync_lock_status & lock_mask))
			/* Input detected */
			break;
	}

	if (try_count < 0)
		return -EINVAL;

	decoder->input = input;
	decoder->output = output;

	v4l2_dbg(1, debug, sd, "Input set to: %d\n", input_sel);

	return 0;
}

/**
 * tvp514x_s_ctrl() - V4L2 decoder interface handler for s_ctrl
 * @ctrl: pointer to v4l2_ctrl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW. Otherwise, returns -EINVAL if the control is not supported.
 */
static int tvp514x_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = to_sd(ctrl);
	struct tvp514x_decoder *decoder = to_decoder(sd);
	int err = -EINVAL, value;

	value = ctrl->val;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		err = tvp514x_write_reg(sd, REG_BRIGHTNESS, value);
		if (!err)
			decoder->tvp514x_regs[REG_BRIGHTNESS].val = value;
		break;
	case V4L2_CID_CONTRAST:
		err = tvp514x_write_reg(sd, REG_CONTRAST, value);
		if (!err)
			decoder->tvp514x_regs[REG_CONTRAST].val = value;
		break;
	case V4L2_CID_SATURATION:
		err = tvp514x_write_reg(sd, REG_SATURATION, value);
		if (!err)
			decoder->tvp514x_regs[REG_SATURATION].val = value;
		break;
	case V4L2_CID_HUE:
		if (value == 180)
			value = 0x7F;
		else if (value == -180)
			value = 0x80;
		err = tvp514x_write_reg(sd, REG_HUE, value);
		if (!err)
			decoder->tvp514x_regs[REG_HUE].val = value;
		break;
	case V4L2_CID_AUTOGAIN:
		err = tvp514x_write_reg(sd, REG_AFE_GAIN_CTRL, value ? 0x0f : 0x0c);
		if (!err)
			decoder->tvp514x_regs[REG_AFE_GAIN_CTRL].val = value;
		break;
	}

	v4l2_dbg(1, debug, sd, "Set Control: ID - %d - %d\n",
			ctrl->id, ctrl->val);
	return err;
}

/**
 * tvp514x_enum_mbus_fmt() - V4L2 decoder interface handler for enum_mbus_fmt
 * @sd: pointer to standard V4L2 sub-device structure
 * @index: index of pixelcode to retrieve
 * @code: receives the pixelcode
 *
 * Enumerates supported mediabus formats
 */
static int
tvp514x_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index)
		return -EINVAL;

	*code = V4L2_MBUS_FMT_YUYV10_2X10;
	return 0;
}

/**
 * tvp514x_mbus_fmt_cap() - V4L2 decoder interface handler for try/s/g_mbus_fmt
 * @sd: pointer to standard V4L2 sub-device structure
 * @f: pointer to the mediabus format structure
 *
 * Negotiates the image capture size and mediabus format.
 */
static int
tvp514x_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *f)
{
	struct tvp514x_decoder *decoder = to_decoder(sd);
	enum tvp514x_std current_std;

	if (f == NULL)
		return -EINVAL;

	/* Calculate height and width based on current standard */
	current_std = decoder->current_std;

	f->code = V4L2_MBUS_FMT_YUYV10_2X10;
	f->width = decoder->std_list[current_std].width;
	f->height = decoder->std_list[current_std].height;
	f->field = V4L2_FIELD_INTERLACED;
	f->colorspace = V4L2_COLORSPACE_SMPTE170M;

	v4l2_dbg(1, debug, sd, "MBUS_FMT: Width - %d, Height - %d\n",
			f->width, f->height);
	return 0;
}

/**
 * tvp514x_g_parm() - V4L2 decoder interface handler for g_parm
 * @sd: pointer to standard V4L2 sub-device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the decoder's video CAPTURE parameters.
 */
static int
tvp514x_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct tvp514x_decoder *decoder = to_decoder(sd);
	struct v4l2_captureparm *cparm;
	enum tvp514x_std current_std;

	if (a == NULL)
		return -EINVAL;

	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		/* only capture is supported */
		return -EINVAL;

	/* get the current standard */
	current_std = decoder->current_std;

	cparm = &a->parm.capture;
	cparm->capability = V4L2_CAP_TIMEPERFRAME;
	cparm->timeperframe =
		decoder->std_list[current_std].standard.frameperiod;

	return 0;
}

/**
 * tvp514x_s_parm() - V4L2 decoder interface handler for s_parm
 * @sd: pointer to standard V4L2 sub-device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the decoder to use the input parameters, if possible. If
 * not possible, returns the appropriate error code.
 */
static int
tvp514x_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct tvp514x_decoder *decoder = to_decoder(sd);
	struct v4l2_fract *timeperframe;
	enum tvp514x_std current_std;

	if (a == NULL)
		return -EINVAL;

	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		/* only capture is supported */
		return -EINVAL;

	timeperframe = &a->parm.capture.timeperframe;

	/* get the current standard */
	current_std = decoder->current_std;

	*timeperframe =
	    decoder->std_list[current_std].standard.frameperiod;

	return 0;
}

/**
 * tvp514x_s_stream() - V4L2 decoder i/f handler for s_stream
 * @sd: pointer to standard V4L2 sub-device structure
 * @enable: streaming enable or disable
 *
 * Sets streaming to enable or disable, if possible.
 */
static int tvp514x_s_stream(struct v4l2_subdev *sd, int enable)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct tvp514x_decoder *decoder = to_decoder(sd);

	if (decoder->streaming == enable)
		return 0;

	switch (enable) {
	case 0:
	{
		/* Power Down Sequence */
		err = tvp514x_write_reg(sd, REG_OPERATION_MODE, 0x01);
		if (err) {
			v4l2_err(sd, "Unable to turn off decoder\n");
			return err;
		}
		decoder->streaming = enable;
		break;
	}
	case 1:
	{
		struct tvp514x_reg *int_seq = (struct tvp514x_reg *)
				client->driver->id_table->driver_data;

		/* Power Up Sequence */
		err = tvp514x_write_regs(sd, int_seq);
		if (err) {
			v4l2_err(sd, "Unable to turn on decoder\n");
			return err;
		}
		/* Detect if not already detected */
		err = tvp514x_detect(sd, decoder);
		if (err) {
			v4l2_err(sd, "Unable to detect decoder\n");
			return err;
		}
		err = tvp514x_configure(sd, decoder);
		if (err) {
			v4l2_err(sd, "Unable to configure decoder\n");
			return err;
		}
		decoder->streaming = enable;
		break;
	}
	default:
		err = -ENODEV;
		break;
	}

	return err;
}

static const struct v4l2_ctrl_ops tvp514x_ctrl_ops = {
	.s_ctrl = tvp514x_s_ctrl,
};

static const struct v4l2_subdev_core_ops tvp514x_core_ops = {
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.queryctrl = v4l2_subdev_queryctrl,
	.querymenu = v4l2_subdev_querymenu,
	.s_std = tvp514x_s_std,
};

static const struct v4l2_subdev_video_ops tvp514x_video_ops = {
	.s_routing = tvp514x_s_routing,
	.querystd = tvp514x_querystd,
	.enum_mbus_fmt = tvp514x_enum_mbus_fmt,
	.g_mbus_fmt = tvp514x_mbus_fmt,
	.try_mbus_fmt = tvp514x_mbus_fmt,
	.s_mbus_fmt = tvp514x_mbus_fmt,
	.g_parm = tvp514x_g_parm,
	.s_parm = tvp514x_s_parm,
	.s_stream = tvp514x_s_stream,
};

static const struct v4l2_subdev_ops tvp514x_ops = {
	.core = &tvp514x_core_ops,
	.video = &tvp514x_video_ops,
};

static struct tvp514x_decoder tvp514x_dev = {
	.streaming = 0,
	.current_std = STD_NTSC_MJ,
	.std_list = tvp514x_std_list,
	.num_stds = ARRAY_SIZE(tvp514x_std_list),

};

/**
 * tvp514x_probe() - decoder driver i2c probe handler
 * @client: i2c driver client device structure
 * @id: i2c driver id table
 *
 * Register decoder as an i2c client device and V4L2
 * device.
 */
static int
tvp514x_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tvp514x_decoder *decoder;
	struct v4l2_subdev *sd;

	/* Check if the adapter supports the needed features */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	if (!client->dev.platform_data) {
		v4l2_err(client, "No platform data!!\n");
		return -ENODEV;
	}

	decoder = kzalloc(sizeof(*decoder), GFP_KERNEL);
	if (!decoder)
		return -ENOMEM;

	/* Initialize the tvp514x_decoder with default configuration */
	*decoder = tvp514x_dev;
	/* Copy default register configuration */
	memcpy(decoder->tvp514x_regs, tvp514x_reg_list_default,
			sizeof(tvp514x_reg_list_default));

	/* Copy board specific information here */
	decoder->pdata = client->dev.platform_data;

	/**
	 * Fetch platform specific data, and configure the
	 * tvp514x_reg_list[] accordingly. Since this is one
	 * time configuration, no need to preserve.
	 */
	decoder->tvp514x_regs[REG_OUTPUT_FORMATTER2].val |=
		(decoder->pdata->clk_polarity << 1);
	decoder->tvp514x_regs[REG_SYNC_CONTROL].val |=
		((decoder->pdata->hs_polarity << 2) |
		 (decoder->pdata->vs_polarity << 3));
	/* Set default standard to auto */
	decoder->tvp514x_regs[REG_VIDEO_STD].val =
		VIDEO_STD_AUTO_SWITCH_BIT;

	/* Register with V4L2 layer as slave device */
	sd = &decoder->sd;
	v4l2_i2c_subdev_init(sd, client, &tvp514x_ops);

	v4l2_ctrl_handler_init(&decoder->hdl, 5);
	v4l2_ctrl_new_std(&decoder->hdl, &tvp514x_ctrl_ops,
		V4L2_CID_BRIGHTNESS, 0, 255, 1, 128);
	v4l2_ctrl_new_std(&decoder->hdl, &tvp514x_ctrl_ops,
		V4L2_CID_CONTRAST, 0, 255, 1, 128);
	v4l2_ctrl_new_std(&decoder->hdl, &tvp514x_ctrl_ops,
		V4L2_CID_SATURATION, 0, 255, 1, 128);
	v4l2_ctrl_new_std(&decoder->hdl, &tvp514x_ctrl_ops,
		V4L2_CID_HUE, -180, 180, 180, 0);
	v4l2_ctrl_new_std(&decoder->hdl, &tvp514x_ctrl_ops,
		V4L2_CID_AUTOGAIN, 0, 1, 1, 1);
	sd->ctrl_handler = &decoder->hdl;
	if (decoder->hdl.error) {
		int err = decoder->hdl.error;

		v4l2_ctrl_handler_free(&decoder->hdl);
		kfree(decoder);
		return err;
	}
	v4l2_ctrl_handler_setup(&decoder->hdl);

	v4l2_info(sd, "%s decoder driver registered !!\n", sd->name);

	return 0;

}

/**
 * tvp514x_remove() - decoder driver i2c remove handler
 * @client: i2c driver client device structure
 *
 * Unregister decoder as an i2c client device and V4L2
 * device. Complement of tvp514x_probe().
 */
static int tvp514x_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct tvp514x_decoder *decoder = to_decoder(sd);

	v4l2_device_unregister_subdev(sd);
	v4l2_ctrl_handler_free(&decoder->hdl);
	kfree(decoder);
	return 0;
}
/* TVP5146 Init/Power on Sequence */
static const struct tvp514x_reg tvp5146_init_reg_seq[] = {
	{TOK_WRITE, REG_VBUS_ADDRESS_ACCESS1, 0x02},
	{TOK_WRITE, REG_VBUS_ADDRESS_ACCESS2, 0x00},
	{TOK_WRITE, REG_VBUS_ADDRESS_ACCESS3, 0x80},
	{TOK_WRITE, REG_VBUS_DATA_ACCESS_NO_VBUS_ADDR_INCR, 0x01},
	{TOK_WRITE, REG_VBUS_ADDRESS_ACCESS1, 0x60},
	{TOK_WRITE, REG_VBUS_ADDRESS_ACCESS2, 0x00},
	{TOK_WRITE, REG_VBUS_ADDRESS_ACCESS3, 0xB0},
	{TOK_WRITE, REG_VBUS_DATA_ACCESS_NO_VBUS_ADDR_INCR, 0x01},
	{TOK_WRITE, REG_VBUS_DATA_ACCESS_NO_VBUS_ADDR_INCR, 0x00},
	{TOK_WRITE, REG_OPERATION_MODE, 0x01},
	{TOK_WRITE, REG_OPERATION_MODE, 0x00},
	{TOK_TERM, 0, 0},
};

/* TVP5147 Init/Power on Sequence */
static const struct tvp514x_reg tvp5147_init_reg_seq[] =	{
	{TOK_WRITE, REG_VBUS_ADDRESS_ACCESS1, 0x02},
	{TOK_WRITE, REG_VBUS_ADDRESS_ACCESS2, 0x00},
	{TOK_WRITE, REG_VBUS_ADDRESS_ACCESS3, 0x80},
	{TOK_WRITE, REG_VBUS_DATA_ACCESS_NO_VBUS_ADDR_INCR, 0x01},
	{TOK_WRITE, REG_VBUS_ADDRESS_ACCESS1, 0x60},
	{TOK_WRITE, REG_VBUS_ADDRESS_ACCESS2, 0x00},
	{TOK_WRITE, REG_VBUS_ADDRESS_ACCESS3, 0xB0},
	{TOK_WRITE, REG_VBUS_DATA_ACCESS_NO_VBUS_ADDR_INCR, 0x01},
	{TOK_WRITE, REG_VBUS_ADDRESS_ACCESS1, 0x16},
	{TOK_WRITE, REG_VBUS_ADDRESS_ACCESS2, 0x00},
	{TOK_WRITE, REG_VBUS_ADDRESS_ACCESS3, 0xA0},
	{TOK_WRITE, REG_VBUS_DATA_ACCESS_NO_VBUS_ADDR_INCR, 0x16},
	{TOK_WRITE, REG_VBUS_ADDRESS_ACCESS1, 0x60},
	{TOK_WRITE, REG_VBUS_ADDRESS_ACCESS2, 0x00},
	{TOK_WRITE, REG_VBUS_ADDRESS_ACCESS3, 0xB0},
	{TOK_WRITE, REG_VBUS_DATA_ACCESS_NO_VBUS_ADDR_INCR, 0x00},
	{TOK_WRITE, REG_OPERATION_MODE, 0x01},
	{TOK_WRITE, REG_OPERATION_MODE, 0x00},
	{TOK_TERM, 0, 0},
};

/* TVP5146M2/TVP5147M1 Init/Power on Sequence */
static const struct tvp514x_reg tvp514xm_init_reg_seq[] = {
	{TOK_WRITE, REG_OPERATION_MODE, 0x01},
	{TOK_WRITE, REG_OPERATION_MODE, 0x00},
	{TOK_TERM, 0, 0},
};

/**
 * I2C Device Table -
 *
 * name - Name of the actual device/chip.
 * driver_data - Driver data
 */
static const struct i2c_device_id tvp514x_id[] = {
	{"tvp5146", (unsigned long)tvp5146_init_reg_seq},
	{"tvp5146m2", (unsigned long)tvp514xm_init_reg_seq},
	{"tvp5147", (unsigned long)tvp5147_init_reg_seq},
	{"tvp5147m1", (unsigned long)tvp514xm_init_reg_seq},
	{},
};

MODULE_DEVICE_TABLE(i2c, tvp514x_id);

static struct i2c_driver tvp514x_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = TVP514X_MODULE_NAME,
	},
	.probe = tvp514x_probe,
	.remove = tvp514x_remove,
	.id_table = tvp514x_id,
};

static int __init tvp514x_init(void)
{
	return i2c_add_driver(&tvp514x_driver);
}

static void __exit tvp514x_exit(void)
{
	i2c_del_driver(&tvp514x_driver);
}

module_init(tvp514x_init);
module_exit(tvp514x_exit);
